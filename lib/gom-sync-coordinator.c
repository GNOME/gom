/* gom-sync-coordinator.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gom-delta.h"
#include "gom-deletion.h"
#include "gom-deletion-builder.h"
#include "gom-entity.h"
#include "gom-entity-private.h"
#include "gom-expression.h"
#include "gom-insertion.h"
#include "gom-insertion-builder.h"
#include "gom-merge-decision-private.h"
#include "gom-merge-policy.h"
#include "gom-mutation.h"
#include "gom-mutation-result.h"
#include "gom-repository.h"
#include "gom-repository-private.h"
#include "gom-session.h"
#include "gom-sync-coordinator-private.h"
#include "gom-sync-coordinator.h"
#include "gom-sync-history-private.h"
#include "gom-sync-transport.h"
#include "gom-tombstone-private.h"
#include "gom-update.h"
#include "gom-update-builder.h"
#include "gom-util-private.h"

struct _GomSyncCoordinator
{
  GObject           parent_instance;
  GomMergePolicy   *merge_policy;
  GomSyncTransport *transport;
};

enum
{
  PROP_0,
  PROP_MERGE_POLICY,
  PROP_TRANSPORT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomSyncCoordinator, gom_sync_coordinator, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gom_sync_coordinator_finalize (GObject *object)
{
  GomSyncCoordinator *self = (GomSyncCoordinator *)object;

  g_clear_object (&self->merge_policy);
  g_clear_object (&self->transport);

  G_OBJECT_CLASS (gom_sync_coordinator_parent_class)->finalize (object);
}

static void
gom_sync_coordinator_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GomSyncCoordinator *self = GOM_SYNC_COORDINATOR (object);

  switch (prop_id)
    {
    case PROP_TRANSPORT:
      g_value_take_object (value, gom_sync_coordinator_dup_transport (self));
      break;

    case PROP_MERGE_POLICY:
      g_value_take_object (value, gom_sync_coordinator_dup_merge_policy (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sync_coordinator_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GomSyncCoordinator *self = GOM_SYNC_COORDINATOR (object);

  switch (prop_id)
    {
    case PROP_TRANSPORT:
      self->transport = g_value_dup_object (value);
      break;

    case PROP_MERGE_POLICY:
      self->merge_policy = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sync_coordinator_class_init (GomSyncCoordinatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_sync_coordinator_finalize;
  object_class->get_property = gom_sync_coordinator_get_property;
  object_class->set_property = gom_sync_coordinator_set_property;

  properties[PROP_MERGE_POLICY] =
    g_param_spec_object ("merge-policy", NULL, NULL,
                         GOM_TYPE_MERGE_POLICY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_TRANSPORT] =
    g_param_spec_object ("transport", NULL, NULL,
                         GOM_TYPE_SYNC_TRANSPORT,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_sync_coordinator_init (GomSyncCoordinator *self)
{
}

DEX_DEFINE_CLOSURE_TYPE (GomSyncCoordinatorStage, gom_sync_coordinator_stage,
                         DEX_DEFINE_CLOSURE_OBJECT (GomSyncCoordinator, coordinator),
                         DEX_DEFINE_CLOSURE_OBJECT (GomRepository, repository),
                         DEX_DEFINE_CLOSURE_OBJECT (GomSession, session),
                         DEX_DEFINE_CLOSURE_OBJECT (GomEntity, entity),
                         DEX_DEFINE_CLOSURE_OBJECT (GomDelta, delta))

static DexFuture *
gom_sync_coordinator_stage_fiber (gpointer user_data)
{
  GomSyncCoordinatorStage *state = user_data;
  g_autoptr(GError) error = NULL;
  guint64 sequence;

  g_assert (state != NULL);
  g_assert (GOM_IS_SYNC_COORDINATOR (state->coordinator));
  g_assert (GOM_IS_REPOSITORY (state->repository));
  g_assert (state->session == NULL || GOM_IS_SESSION (state->session));
  g_assert (GOM_IS_ENTITY (state->entity));
  g_assert (GOM_IS_DELTA (state->delta));

  sequence = dex_await_uint64 (_gom_sync_history_stage_local_change (state->repository,
                                                                     state->session,
                                                                     state->entity,
                                                                     state->delta),
                               &error);
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (sequence != 0 && gom_delta_get_kind (state->delta) == GOM_DELTA_KIND_DELETE)
    {
      GomEntityClass *entity_class = GOM_ENTITY_GET_CLASS (state->entity);
      g_autofree char *identity = NULL;
      const char *relation;

      relation = gom_entity_class_get_relation (entity_class);
      if (!(identity = _gom_tombstone_serialize_entity_identity (state->entity, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!dex_await (_gom_tombstone_record_with_session (state->repository,
                                                          state->session,
                                                          G_OBJECT_TYPE (state->entity),
                                                          relation,
                                                          identity,
                                                          sequence),
                      &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return gom_sync_transport_stage_local_change (state->coordinator->transport,
                                                state->coordinator,
                                                state->repository,
                                                state->session,
                                                state->entity,
                                                state->delta);
}

/**
 * gom_sync_coordinator_new:
 * @transport: a [class@Gom.SyncTransport]
 * @merge_policy: a [class@Gom.MergePolicy]
 *
 * Returns: (transfer full):
 */
GomSyncCoordinator *
gom_sync_coordinator_new (GomSyncTransport *transport,
                          GomMergePolicy   *merge_policy)
{
  g_return_val_if_fail (GOM_IS_SYNC_TRANSPORT (transport), NULL);
  g_return_val_if_fail (GOM_IS_MERGE_POLICY (merge_policy), NULL);

  return g_object_new (GOM_TYPE_SYNC_COORDINATOR,
                       "merge-policy", merge_policy,
                       "transport", transport,
                       NULL);
}

/**
 * gom_sync_coordinator_get_transport:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Returns: (transfer none): a [class@Gom.SyncTransport]
 */
GomSyncTransport *
gom_sync_coordinator_get_transport (GomSyncCoordinator *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_COORDINATOR (self), NULL);

  return self->transport;
}

/**
 * gom_sync_coordinator_get_merge_policy:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Returns: (transfer none): a [class@Gom.MergePolicy]
 */
GomMergePolicy *
gom_sync_coordinator_get_merge_policy (GomSyncCoordinator *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_COORDINATOR (self), NULL);

  return self->merge_policy;
}

/**
 * gom_sync_coordinator_dup_transport:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Returns: (transfer full): a [class@Gom.SyncTransport]
 */
GomSyncTransport *
gom_sync_coordinator_dup_transport (GomSyncCoordinator *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_COORDINATOR (self), NULL);
  g_return_val_if_fail (GOM_IS_SYNC_TRANSPORT (self->transport), NULL);

  return g_object_ref (self->transport);
}

/**
 * gom_sync_coordinator_dup_merge_policy:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Returns: (transfer full): a [class@Gom.MergePolicy]
 */
GomMergePolicy *
gom_sync_coordinator_dup_merge_policy (GomSyncCoordinator *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_COORDINATOR (self), NULL);
  g_return_val_if_fail (GOM_IS_MERGE_POLICY (self->merge_policy), NULL);

  return g_object_ref (self->merge_policy);
}

/**
 * gom_sync_coordinator_stage_local_change:
 * @self: a [class@Gom.SyncCoordinator]
 * @repository: a [class@Gom.Repository]
 * @session: (nullable): a [class@Gom.Session]
 * @entity: a [class@Gom.Entity]
 * @delta: a [class@Gom.Delta]
 *
 * Stages @delta with the coordinator's transport before @session is
 * committed.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to `TRUE`
 */
DexFuture *
gom_sync_coordinator_stage_local_change (GomSyncCoordinator *self,
                                         GomRepository      *repository,
                                         GomSession         *session,
                                         GomEntity          *entity,
                                         GomDelta           *delta)
{
  GomSyncCoordinatorStage *state;

  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (self));
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (!session || GOM_IS_SESSION (session));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));
  dex_return_error_if_fail (GOM_IS_DELTA (delta));

  state = gom_sync_coordinator_stage_new ();
  g_set_object (&state->coordinator, self);
  g_set_object (&state->repository, repository);
  g_set_object (&state->session, session);
  g_set_object (&state->entity, entity);
  g_set_object (&state->delta, delta);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_sync_coordinator_stage_fiber,
                              state,
                              (GDestroyNotify) gom_sync_coordinator_stage_free);
}

/**
 * gom_sync_coordinator_push:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Pushes locally staged changes using the coordinator's transport.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 */
DexFuture *
gom_sync_coordinator_push (GomSyncCoordinator *self)
{
  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (self));

  return gom_sync_transport_push (self->transport, self);
}

/**
 * gom_sync_coordinator_pull:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Pulls remote changes using the coordinator's transport.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 */
DexFuture *
gom_sync_coordinator_pull (GomSyncCoordinator *self)
{
  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (self));

  return gom_sync_transport_pull (self->transport, self);
}

static DexFuture *
gom_sync_coordinator_sync_after_pull_cb (DexFuture *completed,
                                         gpointer   user_data)
{
  GomSyncCoordinator *self = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SYNC_COORDINATOR (self));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return gom_sync_coordinator_push (self);
}

/**
 * gom_sync_coordinator_sync:
 * @self: a [class@Gom.SyncCoordinator]
 *
 * Pulls remote changes and then pushes local staged changes.
 *
 * Returns: (transfer full): a [class@Dex.Future]
 */
DexFuture *
gom_sync_coordinator_sync (GomSyncCoordinator *self)
{
  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (self));

  return dex_future_then (gom_sync_coordinator_pull (self),
                          gom_sync_coordinator_sync_after_pull_cb,
                          g_object_ref (self),
                          g_object_unref);
}

DEX_DEFINE_CLOSURE_TYPE (GomSyncCoordinatorMerge, gom_sync_coordinator_merge,
                         DEX_DEFINE_CLOSURE_OBJECT (GomMergePolicy, merge_policy),
                         DEX_DEFINE_CLOSURE_OBJECT (GomMergeDecision, decision))

static DexFuture *
gom_sync_coordinator_merge_remote_change_fiber (gpointer user_data)
{
  GomSyncCoordinatorMerge *state = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomDelta) delta = NULL;

  g_assert (GOM_IS_MERGE_POLICY (state->merge_policy));
  g_assert (GOM_IS_MERGE_DECISION (state->decision));

  delta = _gom_merge_decision_await (state->decision,
                                     gom_merge_policy_merge (state->merge_policy, state->decision),
                                     &error);
  if (delta == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&delta));
}

/**
 * gom_sync_coordinator_merge_remote_change:
 * @self: a [class@Gom.SyncCoordinator]
 * @repository: a [class@Gom.Repository]
 * @local_delta: (nullable): the local delta
 * @remote_delta: the remote delta
 *
 * Creates a [class@Gom.MergeDecision] and delegates it to the coordinator's
 * merge policy. The policy must resolve the decision before its returned
 * future completes.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Delta]
 */
DexFuture *
gom_sync_coordinator_merge_remote_change (GomSyncCoordinator *self,
                                          GomRepository      *repository,
                                          GomDelta           *local_delta,
                                          GomDelta           *remote_delta)
{
  g_autoptr(GomMergeDecision) decision = NULL;
  GomSyncCoordinatorMerge *state;

  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (self));
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (!local_delta || GOM_IS_DELTA (local_delta));
  dex_return_error_if_fail (GOM_IS_DELTA (remote_delta));

  if (!(decision = gom_merge_decision_new (repository, local_delta, remote_delta)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Local and remote deltas do not match");

  state = gom_sync_coordinator_merge_new ();
  g_set_object (&state->merge_policy, self->merge_policy);
  g_set_object (&state->decision, decision);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_sync_coordinator_merge_remote_change_fiber,
                              state,
                              (GDestroyNotify) gom_sync_coordinator_merge_free);
}

DEX_DEFINE_CLOSURE_TYPE (GomSyncCoordinatorApply, gom_sync_coordinator_apply,
                         DEX_DEFINE_CLOSURE_OBJECT (GomSyncCoordinator, coordinator),
                         DEX_DEFINE_CLOSURE_OBJECT (GomRepository, repository),
                         DEX_DEFINE_CLOSURE_OBJECT (GomDelta, delta),
                         DEX_DEFINE_CLOSURE_POINTER (char *, relation, g_free),
                         DEX_DEFINE_CLOSURE_POINTER (char *, identity, g_free),
                         DEX_DEFINE_CLOSURE_VALUE (guint64, sequence))

static const char *
gom_sync_coordinator_get_field_name (GomEntityClass *entity_class,
                                     const char     *property_name)
{
  GomEntityPropertyInfo *prop_info;

  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (property_name != NULL);

  prop_info = _gom_entity_class_get_property (entity_class, property_name, FALSE);
  if (prop_info != NULL && prop_info->field_name != NULL)
    return prop_info->field_name;

  return property_name;
}

static gboolean
gom_sync_coordinator_apply_delta_values_to_entity (GomEntity  *entity,
                                                   GomDelta   *delta,
                                                   GError    **error)
{
  GObjectClass *object_class;
  guint n_changes;

  g_assert (GOM_IS_ENTITY (entity));
  g_assert (GOM_IS_DELTA (delta));

  object_class = G_OBJECT_GET_CLASS (entity);
  n_changes = gom_delta_get_n_changes (delta);

  for (guint i = 0; i < n_changes; i++)
    {
      const char *property_name;
      GParamSpec *pspec;
      GValue value = G_VALUE_INIT;

      if (!gom_delta_get_current_value (delta, i, &value))
        continue;

      property_name = gom_delta_get_property_name (delta, i);
      pspec = g_object_class_find_property (object_class, property_name);
      if (pspec == NULL || (pspec->flags & G_PARAM_WRITABLE) == 0)
        {
          g_value_unset (&value);
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Remote delta property '%s' is not writable on '%s'",
                       property_name,
                       G_OBJECT_TYPE_NAME (entity));
          return FALSE;
        }

      g_object_set_property (G_OBJECT (entity), property_name, &value);
      g_value_unset (&value);
    }

  return TRUE;
}

static GomEntity *
gom_sync_coordinator_create_entity_for_delta (GomRepository  *repository,
                                              const char     *identity,
                                              GomDelta       *delta,
                                              GError        **error)
{
  g_autoptr(GomEntity) entity = NULL;
  GType entity_type;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (identity != NULL);
  g_assert (GOM_IS_DELTA (delta));

  entity_type = gom_delta_get_entity_type (delta);
  if (!g_type_is_a (entity_type, GOM_TYPE_ENTITY))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Remote delta entity type '%s' is not a GomEntity",
                   g_type_name (entity_type));
      return NULL;
    }

  entity = g_object_new (entity_type, NULL);
  gom_entity_set_repository (entity, repository);

  if (!_gom_tombstone_apply_identity_to_entity (entity, identity, error))
    return NULL;

  if (!gom_sync_coordinator_apply_delta_values_to_entity (entity, delta, error))
    return NULL;

  return g_steal_pointer (&entity);
}

static gboolean
gom_sync_coordinator_add_entity_value_to_insert (GomInsertionBuilder  *builder,
                                                 GPtrArray            *row,
                                                 GomEntity            *entity,
                                                 const char           *property_name,
                                                 GHashTable           *seen,
                                                 GError              **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char *field_name;
  GValue value = G_VALUE_INIT;

  g_assert (builder != NULL);
  g_assert (row != NULL);
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (property_name != NULL);
  g_assert (seen != NULL);

  if (g_hash_table_contains (seen, property_name))
    return TRUE;

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  object_class = G_OBJECT_GET_CLASS (entity);

  if (!gom_entity_class_property_get_mapped (entity_class, property_name))
    return TRUE;

  if (!gom_entity_get_property_storage_value (entity,
                                              entity_class,
                                              object_class,
                                              property_name,
                                              &value,
                                              error))
    return FALSE;

  field_name = gom_sync_coordinator_get_field_name (entity_class, property_name);
  gom_insertion_builder_add_column (builder, gom_field_expression_new (field_name));
  g_ptr_array_add (row, gom_literal_expression_new (&value));
  g_hash_table_add (seen, (gpointer)g_intern_string (property_name));
  g_value_unset (&value);

  return TRUE;
}

static GomInsertion *
gom_sync_coordinator_build_remote_insert (GomRepository  *repository,
                                          const char     *identity,
                                          GomDelta       *delta,
                                          GError        **error)
{
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomInsertionBuilder) builder = NULL;
  g_autoptr(GPtrArray) row = NULL;
  g_autoptr(GHashTable) seen = NULL;
  GomEntityClass *entity_class;
  const char * const *identity_fields;
  guint n_changes;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (identity != NULL);
  g_assert (GOM_IS_DELTA (delta));

  if (!(entity = gom_sync_coordinator_create_entity_for_delta (repository, identity, delta, error)))
    return NULL;

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  builder = gom_insertion_builder_new (repository);
  row = g_ptr_array_new ();
  seen = g_hash_table_new (g_str_hash, g_str_equal);

  gom_insertion_builder_set_target_entity_type (builder, G_OBJECT_TYPE (entity));

  if (identity_fields != NULL)
    {
      for (guint i = 0; identity_fields[i] != NULL; i++)
        {
          if (!gom_sync_coordinator_add_entity_value_to_insert (builder,
                                                                row,
                                                                entity,
                                                                identity_fields[i],
                                                                seen,
                                                                error))
            return NULL;
        }
    }

  n_changes = gom_delta_get_n_changes (delta);
  for (guint i = 0; i < n_changes; i++)
    {
      const char *property_name;
      g_auto(GValue) value = G_VALUE_INIT;

      if (!gom_delta_get_current_value (delta, i, &value))
        continue;

      property_name = gom_delta_get_property_name (delta, i);
      if (_gom_strv_contains (identity_fields, property_name))
        continue;

      if (!gom_sync_coordinator_add_entity_value_to_insert (builder, row, entity, property_name, seen, error))
        return NULL;
    }

  if (row->len == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Remote insert delta has no insertable properties");
      return NULL;
    }

  gom_insertion_builder_add_row (builder, (GomExpression **)row->pdata, row->len);

  return gom_insertion_builder_build (builder, error);
}

static gboolean
gom_sync_coordinator_add_update_assignments (GomUpdateBuilder  *builder,
                                             GomEntity         *entity,
                                             GomDelta          *delta,
                                             GError           **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;
  guint n_changes;
  guint n_assignments = 0;

  g_assert (builder != NULL);
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (GOM_IS_DELTA (delta));

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  object_class = G_OBJECT_GET_CLASS (entity);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  n_changes = gom_delta_get_n_changes (delta);

  for (guint i = 0; i < n_changes; i++)
    {
      const char *property_name;
      const char *field_name;
      g_autoptr(GomExpression) column = NULL;
      g_autoptr(GomExpression) literal = NULL;
      g_auto(GValue) current = G_VALUE_INIT;
      g_auto(GValue) storage = G_VALUE_INIT;

      if (!gom_delta_get_current_value (delta, i, &current))
        continue;

      property_name = gom_delta_get_property_name (delta, i);
      if (_gom_strv_contains (identity_fields, property_name))
        continue;

      if (!gom_entity_class_property_get_mapped (entity_class, property_name))
        continue;

      if (!gom_entity_get_property_storage_value (entity, entity_class, object_class, property_name, &storage, error))
        return FALSE;

      field_name = gom_sync_coordinator_get_field_name (entity_class, property_name);
      column = gom_field_expression_new (field_name);
      literal = gom_literal_expression_new (&storage);
      gom_update_builder_add_assignment (builder,
                                         g_steal_pointer (&column),
                                         g_steal_pointer (&literal));

      n_assignments++;
    }

  if (n_assignments == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Remote update delta has no updateable properties");
      return FALSE;
    }

  return TRUE;
}

static DexFuture *
gom_sync_coordinator_apply_remote_insert (GomSyncCoordinatorApply  *state,
                                          GError                  **error)
{
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  guint64 tombstone_sequence = 0;

  g_assert (state != NULL);

  if (!_gom_tombstone_lookup_sequence (state->repository,
                                       gom_delta_get_entity_type (state->delta),
                                       state->identity,
                                       &tombstone_sequence,
                                       error))
    return NULL;

  if (tombstone_sequence >= state->sequence && tombstone_sequence != 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Remote insert is older than a local tombstone");

  if (!(insertion = gom_sync_coordinator_build_remote_insert (state->repository, state->identity, state->delta, error)) ||
      !(result = dex_await_object (gom_repository_mutate (state->repository, GOM_MUTATION (insertion)), error)) ||
      (tombstone_sequence != 0 &&
       !dex_await (_gom_tombstone_remove (state->repository, gom_delta_get_entity_type (state->delta), state->identity), error)))
    return NULL;

  return dex_future_new_true ();
}

static DexFuture *
gom_sync_coordinator_apply_remote_update (GomSyncCoordinatorApply  *state,
                                          GError                  **error)
{
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomUpdateBuilder) builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  guint64 tombstone_sequence = 0;

  g_assert (state != NULL);

  if (!_gom_tombstone_lookup_sequence (state->repository,
                                       gom_delta_get_entity_type (state->delta),
                                       state->identity,
                                       &tombstone_sequence,
                                       error))
    return NULL;

  if (tombstone_sequence >= state->sequence && tombstone_sequence != 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Remote update is older than a local tombstone");

  if (!(entity = gom_sync_coordinator_create_entity_for_delta (state->repository, state->identity, state->delta, error)))
    return NULL;

  if (!(filter = _gom_tombstone_build_identity_filter (G_OBJECT_TYPE (entity), state->identity, error)))
    return NULL;

  builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (builder, G_OBJECT_TYPE (entity));
  gom_update_builder_set_filter (builder, filter);
  gom_update_builder_set_limit (builder, 1);

  if (!gom_sync_coordinator_add_update_assignments (builder, entity, state->delta, error))
    return NULL;

  if (!(update = gom_update_builder_build (builder, error)))
    return NULL;

  if (!(result = dex_await_object (gom_repository_mutate (state->repository, GOM_MUTATION (update)), error)))
    return NULL;

  return dex_future_new_true ();
}

static DexFuture *
gom_sync_coordinator_apply_remote_delete (GomSyncCoordinatorApply  *state,
                                          GError                  **error)
{
  g_autoptr(GomDeletionBuilder) builder = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  GType entity_type;

  g_assert (state != NULL);

  entity_type = gom_delta_get_entity_type (state->delta);

  if (!(filter = _gom_tombstone_build_identity_filter (entity_type, state->identity, error)))
    return NULL;

  builder = gom_deletion_builder_new ();
  gom_deletion_builder_set_target_entity_type (builder, entity_type);
  gom_deletion_builder_set_filter (builder, filter);
  gom_deletion_builder_set_limit (builder, 1);

  if (!(deletion = gom_deletion_builder_build (builder, error)))
    return NULL;

  if (!(result = dex_await_object (gom_repository_mutate (state->repository, GOM_MUTATION (deletion)), error)))
    return NULL;

  if (!dex_await (_gom_tombstone_record (state->repository,
                                         entity_type,
                                         state->relation,
                                         state->identity,
                                         state->sequence),
                  error))
    return NULL;

  return dex_future_new_true ();
}

static DexFuture *
gom_sync_coordinator_apply_remote_change_fiber (gpointer user_data)
{
  GomSyncCoordinatorApply *state = user_data;
  g_autoptr(GError) error = NULL;
  DexFuture *future;

  g_assert (state != NULL);
  g_assert (GOM_IS_SYNC_COORDINATOR (state->coordinator));
  g_assert (GOM_IS_REPOSITORY (state->repository));
  g_assert (GOM_IS_DELTA (state->delta));

  switch (gom_delta_get_kind (state->delta))
    {
    case GOM_DELTA_KIND_INSERT:
      future = gom_sync_coordinator_apply_remote_insert (state, &error);
      break;

    case GOM_DELTA_KIND_UPDATE:
      future = gom_sync_coordinator_apply_remote_update (state, &error);
      break;

    case GOM_DELTA_KIND_DELETE:
      future = gom_sync_coordinator_apply_remote_delete (state, &error);
      break;

    default:
      future = dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_ARGUMENT,
                                      "Unsupported remote delta kind");
      break;
    }

  if (future == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (future, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (_gom_sync_history_append_remote_change (state->repository,
                                                          state->relation,
                                                          state->identity,
                                                          state->delta),
                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

DexFuture *
_gom_sync_coordinator_apply_remote_change (GomSyncCoordinator *self,
                                           GomRepository      *repository,
                                           const char         *relation,
                                           const char         *identity,
                                           guint64             sequence,
                                           GomDelta           *delta)
{
  GomSyncCoordinatorApply *state;
  GomEntityClass *entity_class;
  GType entity_type;

  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (self));
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (identity != NULL);
  dex_return_error_if_fail (sequence > 0);
  dex_return_error_if_fail (GOM_IS_DELTA (delta));

  entity_type = gom_delta_get_entity_type (delta);
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));

  entity_class = g_type_class_get (entity_type);

  if (relation == NULL)
    relation = gom_entity_class_get_relation (entity_class);

  state = gom_sync_coordinator_apply_new ();
  g_set_object (&state->coordinator, self);
  g_set_object (&state->repository, repository);
  g_set_object (&state->delta, delta);
  g_set_str (&state->relation, relation);
  g_set_str (&state->identity, identity);
  state->sequence = sequence;

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_sync_coordinator_apply_remote_change_fiber,
                              state,
                              (GDestroyNotify) gom_sync_coordinator_apply_free);
}
