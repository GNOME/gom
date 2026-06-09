/* gom-session.c
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
#include "gom-cursor-private.h"
#include "gom-driver-private.h"
#include "gom-entity.h"
#include "gom-ordering.h"
#include "gom-mutation.h"
#include "gom-query-private.h"
#include "gom-entity-list-model-private.h"
#include "gom-record-list-model-private.h"
#include "gom-repository-private.h"
#include "gom-session-private.h"
#include "gom-sync-coordinator.h"
#include "gom-trace-private.h"

/**
 * GomSession:
 *
 * A transaction-scoped context for loading, mutating, and tracking entities.
 *
 * Sessions provide a sticky identity map for entities materialized through the
 * same repository/driver pair. Repeated materialization of the same row within
 * a session returns the same in-memory entity instance until the session is
 * committed or rolled back.
 */
/**
 * GomSessionClass:
 * @query: virtual method for session-scoped query execution
 * @mutate: virtual method for session-scoped mutation execution
 * @commit: virtual method for ending a session with commit
 * @rollback: virtual method for ending a session with rollback
 * @accept_entity_changes: virtual method for accepting a resolved delta
 * @lookup_entity: virtual method for session identity-map lookup
 * @register_entity: virtual method for registering a materialized entity
 * @rekey_entity_identity: virtual method for updating an entity's identity key
 * @unregister_entity: virtual method for unregistering an entity
 * @clear_entities: virtual method for detaching all tracked entities
 *
 * Abstract class vtable used by backend implementations.
 */
G_DEFINE_ABSTRACT_TYPE (GomSession, gom_session, G_TYPE_OBJECT)

enum
{
  SIGNAL_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static DexFuture *_gom_session_stage_sync_changes (GomSession *self) G_GNUC_WARN_UNUSED_RESULT;

typedef struct
{
  GomEntity *entity;
  GomDelta  *delta;
} GomSessionSyncChange;

static void
gom_session_sync_change_free (gpointer data)
{
  GomSessionSyncChange *change = data;

  g_clear_object (&change->entity);
  g_clear_object (&change->delta);
  g_free (change);
}

static void
gom_session_dispose (GObject *object)
{
  GomSession *self = GOM_SESSION (object);

  g_clear_object (&self->repository);
  g_clear_pointer (&self->sync_changes, g_ptr_array_unref);
  gom_trace_counter_add (GOM_TRACE_COUNTER_SESSIONS, -1);

  G_OBJECT_CLASS (gom_session_parent_class)->dispose (object);
}

static void
gom_session_class_init (GomSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gom_session_dispose;

  /**
   * GomSession::changed:
   *
   * Emitted when the session has changed state.
   */
  signals[SIGNAL_CHANGED] =
    g_signal_new ("changed",
                  GOM_TYPE_SESSION,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL,
                  NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
gom_session_init (GomSession *self)
{
  static gpointer next_session_id = NULL;

  self->id = (gintptr)g_atomic_pointer_add (&next_session_id, (gintptr)1) + 1;
  self->sync_changes = g_ptr_array_new_with_free_func (gom_session_sync_change_free);
  gom_trace_counter_add (GOM_TRACE_COUNTER_SESSIONS, 1);
  GOM_TRACE_MARK ("Session", "open", "session=%" G_GINT64_FORMAT, self->id);
}

static DexFuture *
gom_session_track_mutation_result_cb (DexFuture *completed,
                                      gpointer   user_data)
{
  GomSession *self = user_data;
  const GValue *value;
  g_autoptr(GError) error = NULL;
  GObject *result;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SESSION (self));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!G_VALUE_HOLDS_OBJECT (value))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Mutation result did not contain an object");

  if (!(result = g_value_get_object (value)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Mutation result did not contain an object");

  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);

  return dex_future_new_take_object (g_object_ref (result));
}

typedef struct
{
  GomSession *session;
  GomQuery   *query;
} GomSessionFindOneState;

static void
gom_session_find_one_state_free (gpointer data)
{
  GomSessionFindOneState *state = data;

  g_clear_object (&state->session);
  g_clear_object (&state->query);
  g_free (state);
}

static DexFuture *
gom_session_find_one_fiber (gpointer user_data)
{
  GomSessionFindOneState *state = user_data;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (GOM_IS_SESSION (state->session));
  g_assert (GOM_IS_QUERY (state->query));

  if (!(cursor = dex_await_object (gom_session_query (state->session, state->query), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await_boolean (gom_cursor_next (cursor), &error))
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_take_object (NULL);
    }

  if (!(entity = gom_cursor_materialize (cursor, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&entity));
}

typedef struct
{
  GomSession *session;
  GomEntity  *entity;
} GomSessionInsertEntityState;

static void
gom_session_insert_entity_state_free (gpointer data)
{
  GomSessionInsertEntityState *state = data;

  g_clear_object (&state->session);
  g_clear_object (&state->entity);
  g_free (state);
}

static DexFuture *
gom_session_insert_entity_fiber (gpointer user_data)
{
  GomSessionInsertEntityState *state = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (GOM_IS_SESSION (state->session));
  g_assert (GOM_IS_ENTITY (state->entity));

  if (!dex_await (gom_session_persist (state->session, state->entity), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!dex_await (gom_session_flush (state->session), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

/**
 * _gom_session_set_repository:
 * @self: a [class@Gom.Session]
 * @repository: (nullable): a [class@Gom.Repository]
 *
 * Binds the session to its repository.
 */
void
_gom_session_set_repository (GomSession    *self,
                             GomRepository *repository)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (repository == NULL || GOM_IS_REPOSITORY (repository));

  g_set_object (&self->repository, repository);
}

/**
 * _gom_session_dup_repository:
 * @self: a [class@Gom.Session]
 *
 * Returns: (transfer full) (nullable): the bound repository
 */
GomRepository *
_gom_session_dup_repository (GomSession *self)
{
  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);

  return self->repository ? g_object_ref (self->repository) : NULL;
}

/**
 * _gom_session_set_closed:
 * @self: a [class@Gom.Session]
 * @closed: whether the session is closed
 *
 * Marks the session closed state.
 */
void
_gom_session_set_closed (GomSession *self,
                         gboolean    closed)
{
  g_return_if_fail (GOM_IS_SESSION (self));

  self->closed = closed != FALSE;
}

/**
 * _gom_session_is_closed:
 * @self: a [class@Gom.Session]
 *
 * Returns: whether the session is closed
 */
gboolean
_gom_session_is_closed (GomSession *self)
{
  g_return_val_if_fail (GOM_IS_SESSION (self), TRUE);

  return self->closed != FALSE;
}

DexFuture *
_gom_session_query (GomSession *self,
                    GomQuery   *query)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_QUERY (query));

  if (_gom_session_is_closed (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CLOSED,
                                  "Session is closed");

  if (GOM_SESSION_GET_CLASS (self)->query != NULL)
    return GOM_SESSION_GET_CLASS (self)->query (self, query);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Query is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

DexFuture *
_gom_session_mutate (GomSession  *self,
                     GomMutation *mutation)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_MUTATION (mutation));

  if (_gom_session_is_closed (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CLOSED,
                                  "Session is closed");

  if (GOM_SESSION_GET_CLASS (self)->mutate != NULL)
    return GOM_SESSION_GET_CLASS (self)->mutate (self, mutation);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Mutation is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

DexFuture *
_gom_session_persist (GomSession *self,
                      GomEntity  *entity)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));

  if (_gom_session_is_closed (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CLOSED,
                                  "Session is closed");

  if (GOM_SESSION_GET_CLASS (self)->persist != NULL)
    return GOM_SESSION_GET_CLASS (self)->persist (self, entity);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Persist is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

DexFuture *
_gom_session_flush (GomSession *self)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));

  if (_gom_session_is_closed (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CLOSED,
                                  "Session is closed");

  if (GOM_SESSION_GET_CLASS (self)->flush != NULL)
    return GOM_SESSION_GET_CLASS (self)->flush (self);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Flush is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

DexFuture *
_gom_session_commit (GomSession *self)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));

  if (_gom_session_is_closed (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CLOSED,
                                  "Session is closed");

  if (GOM_SESSION_GET_CLASS (self)->commit != NULL)
    return GOM_SESSION_GET_CLASS (self)->commit (self);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Commit is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

DexFuture *
_gom_session_rollback (GomSession *self)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));

  if (_gom_session_is_closed (self))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_CLOSED,
                                  "Session is closed");

  if (GOM_SESSION_GET_CLASS (self)->rollback != NULL)
    return GOM_SESSION_GET_CLASS (self)->rollback (self);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Rollback is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

void
_gom_session_track_entity_changes (GomSession *self,
                                   GomEntity  *entity)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));

  if (GOM_SESSION_GET_CLASS (self)->track_entity_changes != NULL)
    GOM_SESSION_GET_CLASS (self)->track_entity_changes (self, entity);
}

void
_gom_session_untrack_entity_changes (GomSession *self,
                                     GomEntity  *entity)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));

  if (GOM_SESSION_GET_CLASS (self)->untrack_entity_changes != NULL)
    GOM_SESSION_GET_CLASS (self)->untrack_entity_changes (self, entity);
}

void
_gom_session_accept_entity_changes (GomSession *self,
                                    GomEntity  *entity,
                                    GomDelta   *delta)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));
  g_return_if_fail (delta == NULL || GOM_IS_DELTA (delta));

  if (_gom_session_is_closed (self))
    return;

  if (GOM_SESSION_GET_CLASS (self)->accept_entity_changes != NULL)
    GOM_SESSION_GET_CLASS (self)->accept_entity_changes (self, entity, delta);

  _gom_session_record_entity_changes (self, entity, delta);
}

void
_gom_session_record_entity_changes (GomSession *self,
                                    GomEntity  *entity,
                                    GomDelta   *delta)
{
  GomSessionSyncChange *change;

  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));
  g_return_if_fail (delta == NULL || GOM_IS_DELTA (delta));

  if (_gom_session_is_closed (self) || delta == NULL || gom_delta_is_empty (delta))
    return;

  change = g_new0 (GomSessionSyncChange, 1);
  change->entity = g_object_ref (entity);
  change->delta = g_object_ref (delta);

  g_ptr_array_add (self->sync_changes, change);
}

void
_gom_session_mark_entity_dirty (GomSession *self,
                                GomEntity  *entity)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));

  if (_gom_session_is_closed (self))
    return;

  if (GOM_SESSION_GET_CLASS (self)->mark_entity_dirty != NULL)
    GOM_SESSION_GET_CLASS (self)->mark_entity_dirty (self, entity);
}

void
_gom_session_emit_changed (GomSession *self)
{
  g_return_if_fail (GOM_IS_SESSION (self));

  g_signal_emit (self, signals[SIGNAL_CHANGED], 0);
}

DexFuture *
_gom_session_track_mutation_result (GomSession *self,
                                    DexFuture  *mutation_result)
{
  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);
  g_return_val_if_fail (DEX_IS_FUTURE (mutation_result), NULL);

  return dex_future_then (mutation_result,
                          gom_session_track_mutation_result_cb,
                          g_object_ref (self),
                          g_object_unref);
}

/**
 * _gom_session_lookup_entity:
 * @self: a [class@Gom.Session]
 * @entity_key: an identity key
 *
 * Returns: (transfer full) (nullable): a tracked entity
 */
GomEntity *
_gom_session_lookup_entity (GomSession *self,
                            const char *entity_key)
{
  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);
  g_return_val_if_fail (entity_key != NULL, NULL);

  if (_gom_session_is_closed (self))
    return NULL;

  if (GOM_SESSION_GET_CLASS (self)->lookup_entity != NULL)
    return GOM_SESSION_GET_CLASS (self)->lookup_entity (self, entity_key);

  return NULL;
}

/**
 * _gom_session_register_entity:
 * @self: a [class@Gom.Session]
 * @entity: (transfer full): a [class@Gom.Entity]
 * @entity_key: (transfer full): an identity key
 *
 * Returns: (transfer full): the tracked entity
 */
GomEntity *
_gom_session_register_entity (GomSession *self,
                              GomEntity  *entity,
                              char       *entity_key)
{
  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);
  g_return_val_if_fail (GOM_IS_ENTITY (entity), NULL);
  g_return_val_if_fail (entity_key != NULL, NULL);

  if (_gom_session_is_closed (self))
    {
      g_free (entity_key);
      return entity;
    }

  if (GOM_SESSION_GET_CLASS (self)->register_entity != NULL)
    return GOM_SESSION_GET_CLASS (self)->register_entity (self,
                                                          g_steal_pointer (&entity),
                                                          g_steal_pointer (&entity_key));

  g_free (entity_key);
  return entity;
}

/**
 * _gom_session_rekey_entity_identity:
 * @self: a [class@Gom.Session]
 * @entity: (transfer none): the entity
 * @entity_key: (transfer full):
 *
 * Returns: `true` if rekeyed
 */
gboolean
_gom_session_rekey_entity_identity (GomSession *self,
                                    GomEntity  *entity,
                                    char       *entity_key)
{
  g_return_val_if_fail (GOM_IS_SESSION (self), FALSE);
  g_return_val_if_fail (GOM_IS_ENTITY (entity), FALSE);
  g_return_val_if_fail (entity_key != NULL, FALSE);

  if (_gom_session_is_closed (self))
    {
      g_free (entity_key);
      return FALSE;
    }

  if (GOM_SESSION_GET_CLASS (self)->rekey_entity_identity != NULL)
    return GOM_SESSION_GET_CLASS (self)->rekey_entity_identity (self,
                                                                g_steal_pointer (&entity),
                                                                g_steal_pointer (&entity_key));

  g_free (entity_key);
  return FALSE;
}

/**
 * _gom_session_unregister_entity:
 * @self: a [class@Gom.Session]
 * @entity: a [class@Gom.Entity]
 *
 * Unregisters a tracked entity from the session.
 */
void
_gom_session_unregister_entity (GomSession *self,
                                GomEntity  *entity)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));

  if (_gom_session_is_closed (self))
    return;

  if (GOM_SESSION_GET_CLASS (self)->unregister_entity != NULL)
    GOM_SESSION_GET_CLASS (self)->unregister_entity (self, entity);
}

void
_gom_session_unregister_pending_entity (GomSession *self,
                                        GomEntity  *entity)
{
  g_return_if_fail (GOM_IS_SESSION (self));
  g_return_if_fail (GOM_IS_ENTITY (entity));

  if (_gom_session_is_closed (self))
    return;

  if (GOM_SESSION_GET_CLASS (self)->unregister_pending_entity != NULL)
    GOM_SESSION_GET_CLASS (self)->unregister_pending_entity (self, entity);
}

/**
 * _gom_session_clear_entities:
 * @self: a [class@Gom.Session]
 *
 * Detaches all tracked entities from the session.
 */
void
_gom_session_clear_entities (GomSession *self)
{
  g_return_if_fail (GOM_IS_SESSION (self));

  if (GOM_SESSION_GET_CLASS (self)->clear_entities != NULL)
    GOM_SESSION_GET_CLASS (self)->clear_entities (self);
}

/**
 * gom_session_query:
 * @self: a [class@Gom.Session]
 * @query: a [class@Gom.Query]
 *
 * Runs @query through the session's backend and returns a cursor future.
 *
 * The session keeps materialized entities sticky for the duration of the
 * transaction.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a cursor
 */
DexFuture *
gom_session_query (GomSession *self,
                   GomQuery   *query)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_QUERY (query));

  if (self->repository != NULL)
    _gom_repository_precompute (self->repository);

  GOM_TRACE_MARK ("Session",
                  "query",
                  "session=%" G_GINT64_FORMAT " target=%s",
                  self->id,
                  g_type_name (_gom_query_get_target_entity_type (query)));
  return _gom_session_query (self, query);
}

/**
 * gom_session_find_one: (skip)
 * @self: a [class@Gom.Session]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 * @first_property: the first mapped property name to match
 * @...: value for @first_property, followed by additional property/value
 *   pairs, terminated by %NULL
 *
 * Finds the first entity of @entity_type matching all provided property/value
 * pairs through @self.
 *
 * This is a C convenience wrapper around
 * [method@Gom.Session.find_one_with_properties]. The value for each property
 * is collected using the property's `GParamSpec` value type.
 *
 * If no row matches, the returned future resolves to %NULL. If ordering or a
 * more complex filter is required, use [method@Gom.Session.query].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Entity] or %NULL, or rejects with error.
 */
DexFuture *
gom_session_find_one (GomSession *self,
                      GType       entity_type,
                      const char *first_property,
                      ...)
{
  g_autoptr(GError) error = NULL;
  char **properties = NULL;
  GValue *values = NULL;
  g_autoptr(GomRepository) repository = NULL;
  guint n_properties = 0;
  DexFuture *ret;
  va_list args;

  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  dex_return_error_if_fail (first_property != NULL);

  if (!(repository = _gom_session_dup_repository (self)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Session is not bound to a repository");

  _gom_repository_precompute (repository);

  va_start (args, first_property);
  if (!_gom_query_collect_properties (_gom_repository_get_registry (repository),
                                      entity_type,
                                      first_property,
                                      args,
                                      &n_properties,
                                      &properties,
                                      &values,
                                      &error))
    {
      va_end (args);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }
  va_end (args);

  ret = gom_session_find_one_with_properties (self,
                                              entity_type,
                                              n_properties,
                                              (const char * const *)properties,
                                              values);
  _gom_query_clear_collected_properties (n_properties, properties, values);

  return ret;
}

/**
 * gom_session_find_one_with_properties:
 * @self: a [class@Gom.Session]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 * @n_properties: number of property/value pairs
 * @properties: (array length=n_properties) (element-type utf8): mapped
 *   property names to match
 * @values: (array length=n_properties) (element-type GObject.Value): values
 *   for @properties
 *
 * Finds the first entity of @entity_type matching all provided property/value
 * pairs through @self.
 *
 * Property names are resolved through the repository registry, so they must be
 * mapped entity properties. All predicates are equality comparisons combined
 * with `AND`.
 *
 * Materialization happens through @self, preserving the session identity map.
 * If no row matches, the returned future resolves to %NULL. If ordering,
 * projections, joins, or a more complex filter is required, use
 * [method@Gom.Session.query].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Entity] or %NULL, or rejects with error.
 */
DexFuture *
gom_session_find_one_with_properties (GomSession         *self,
                                      GType               entity_type,
                                      guint               n_properties,
                                      const char * const *properties,
                                      const GValue       *values)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomQuery) query = NULL;
  GomSessionFindOneState *state;

  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  dex_return_error_if_fail (n_properties > 0);
  dex_return_error_if_fail (properties != NULL);
  dex_return_error_if_fail (values != NULL);

  if (!(repository = _gom_session_dup_repository (self)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Session is not bound to a repository");

  _gom_repository_precompute (repository);

  query = _gom_query_new_find_one (_gom_repository_get_registry (repository),
                                   entity_type,
                                   n_properties,
                                   properties,
                                   values,
                                   &error);

  if (query == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  state = g_new0 (GomSessionFindOneState, 1);
  state->session = g_object_ref (self);
  state->query = g_object_ref (query);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_session_find_one_fiber,
                              state,
                              gom_session_find_one_state_free);
}

static DexFuture *
gom_session_exhaust_cursor_to_list_cb (DexFuture *completed,
                                       gpointer   user_data)
{
  const GValue *value;
  GomCursor *cursor;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (user_data == NULL);

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_CURSOR));

  cursor = g_value_get_object (value);
  return gom_cursor_exhaust_to_list (cursor);
}

/**
 * gom_session_list_entities:
 * @self: a [class@Gom.Session]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 * @filter: (nullable): a [class@Gom.Expression] used as the query filter
 * @ordering: (nullable): a [class@Gom.Ordering] used to sort results
 *
 * Builds a session-scoped query for @entity_type, applies the optional
 * @filter and @ordering, and returns all matching entities as a list model.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of materialized entities or rejects with error.
 */
DexFuture *
gom_session_list_entities (GomSession    *self,
                           GType          entity_type,
                           GomExpression *filter,
                           GomOrdering   *ordering)
{
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GPtrArray) orderings = NULL;

  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);
  g_return_val_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY), NULL);

  if (self->repository != NULL)
    _gom_repository_precompute (self->repository);

  if (ordering != NULL)
    {
      orderings = g_ptr_array_new_with_free_func (g_object_unref);
      g_ptr_array_add (orderings, g_object_ref (ordering));
    }

  query = _gom_query_new (entity_type,
                          NULL,
                          NULL,
                          filter,
                          NULL,
                          NULL,
                          orderings,
                          0,
                          0,
                          FALSE,
                          FALSE,
                          FALSE);

  return dex_future_then (gom_session_query (self, query),
                          gom_session_exhaust_cursor_to_list_cb,
                          NULL,
                          NULL);
}

/**
 * gom_session_list_query:
 * @self: a [class@Gom.Session]
 * @query: a [class@Gom.Query]
 *
 * Creates a lazy entity list model for @query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.EntityListModel], or a rejected future on error
 */
DexFuture *
gom_session_list_query (GomSession *self,
                        GomQuery   *query)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomEntityListModel) model = NULL;

  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  if (!(model = _gom_entity_list_model_new_session (self, query, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&model));
}

/**
 * gom_session_list_records:
 * @self: a [class@Gom.Session]
 * @query: a [class@Gom.Query]
 *
 * Creates a lazy record list model for @query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.RecordListModel], or a rejected future on error
 */
DexFuture *
gom_session_list_records (GomSession *self,
                          GomQuery   *query)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRecordListModel) model = NULL;

  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  if (!(model = _gom_record_list_model_new_session (self, query, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&model));
}

/**
 * gom_session_mutate:
 * @self: a [class@Gom.Session]
 * @mutation: a [class@Gom.Mutation]
 *
 * Runs @mutation through the session's backend and returns a future for the
 * mutation result.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a mutation
 *   result
 */
DexFuture *
gom_session_mutate (GomSession  *self,
                    GomMutation *mutation)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_MUTATION (mutation));

  if (self->repository != NULL)
    _gom_repository_precompute (self->repository);

  GOM_TRACE_MARK ("Session",
                  "mutate",
                  "session=%" G_GINT64_FORMAT " mutation=%s",
                  self->id,
                  G_OBJECT_TYPE_NAME (mutation));

  return _gom_session_mutate (self, mutation);
}

/**
 * gom_session_insert_entity:
 * @self: a [class@Gom.Session]
 * @entity: a [class@Gom.Entity]
 *
 * Persists @entity in @self and flushes the session.
 *
 * This is a convenience wrapper for inserting a single entity while preserving
 * session identity-map behavior. Use [method@Gom.Session.persist] directly
 * when staging multiple entities before one explicit [method@Gom.Session.flush].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE when
 *   @entity has been inserted, or rejects with error.
 */
DexFuture *
gom_session_insert_entity (GomSession *self,
                           GomEntity  *entity)
{
  GomSessionInsertEntityState *state;

  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));

  if (self->repository != NULL)
    _gom_repository_precompute (self->repository);

  state = g_new0 (GomSessionInsertEntityState, 1);
  state->session = g_object_ref (self);
  state->entity = g_object_ref (entity);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_session_insert_entity_fiber,
                              state,
                              gom_session_insert_entity_state_free);
}

/**
 * gom_session_persist:
 * @self: a [class@Gom.Session]
 * @entity: a [class@Gom.Entity]
 *
 * Stages @entity for persistence within the session.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to `TRUE`
 *   when the entity has been attached to the session.
 */
DexFuture *
gom_session_persist (GomSession *self,
                     GomEntity  *entity)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));

  if (self->repository != NULL)
    _gom_repository_precompute (self->repository);

  GOM_TRACE_MARK ("Session",
                  "persist",
                  "session=%" G_GINT64_FORMAT " entity=%s",
                  self->id,
                  G_OBJECT_TYPE_NAME (entity));

  return _gom_session_persist (self, entity);
}

/**
 * gom_session_flush:
 * @self: a [class@Gom.Session]
 *
 * Flushes pending session changes to the backend.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to `TRUE`
 *   when all pending work has been written.
 */
DexFuture *
gom_session_flush (GomSession *self)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));

  if (self->repository != NULL)
    _gom_repository_precompute (self->repository);

  GOM_TRACE_MARK ("Session", "flush", "session=%" G_GINT64_FORMAT, self->id);

  return _gom_session_flush (self);
}

static DexFuture *
gom_session_commit_after_flush_cb (DexFuture *completed,
                                   gpointer   user_data)
{
  GomSession *session = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  value = dex_future_get_value (completed, &error);
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (value == NULL || !G_VALUE_HOLDS_BOOLEAN (value) || !g_value_get_boolean (value))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Session flush did not complete successfully");

  return _gom_session_stage_sync_changes (session);
}

static DexFuture *
gom_session_commit_after_sync_stage_cb (DexFuture *completed,
                                        gpointer   user_data)
{
  GomSession *session = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SESSION (session));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (G_VALUE_HOLDS_BOOLEAN (value) && !g_value_get_boolean (value))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Sync staging did not complete successfully");

  return _gom_session_commit (session);
}

static DexFuture *
gom_session_commit_complete_cb (DexFuture *completed,
                                gpointer   user_data)
{
  GomSession *session = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SESSION (session));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (G_VALUE_HOLDS_BOOLEAN (value) && !g_value_get_boolean (value))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Session close did not complete successfully");

  g_ptr_array_set_size (session->sync_changes, 0);

  return dex_future_new_true ();
}

static DexFuture *
gom_session_stage_sync_changes_fiber (gpointer user_data)
{
  GomSession *session = user_data;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_SESSION (session));

  if (session->sync_changes == NULL || session->sync_changes->len == 0)
    return dex_future_new_true ();

  if (!(repository = _gom_session_dup_repository (session)))
    return dex_future_new_true ();

  if (!(coordinator = gom_repository_dup_coordinator (repository)))
    return dex_future_new_true ();

  for (guint i = 0; i < session->sync_changes->len; i++)
    {
      GomSessionSyncChange *change = g_ptr_array_index (session->sync_changes, i);

      if (!dex_await (gom_sync_coordinator_stage_local_change (coordinator, repository, session, change->entity, change->delta), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
_gom_session_stage_sync_changes (GomSession *self)
{
  g_assert (GOM_IS_SESSION (self));

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_session_stage_sync_changes_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * gom_session_commit:
 * @self: a [class@Gom.Session]
 *
 * Commits the transaction associated with @self and closes the session.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the
 *   transaction has committed
 */
DexFuture *
gom_session_commit (GomSession *self)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));

  GOM_TRACE_MARK ("Session", "commit", "session=%" G_GINT64_FORMAT, self->id);

  return dex_future_then (dex_future_then (dex_future_then (gom_session_flush (self),
                                                            gom_session_commit_after_flush_cb,
                                                            g_object_ref (self),
                                                            g_object_unref),
                                           gom_session_commit_after_sync_stage_cb,
                                           g_object_ref (self),
                                           g_object_unref),
                          gom_session_commit_complete_cb,
                          g_object_ref (self),
                          g_object_unref);
}

/**
 * gom_session_rollback:
 * @self: a [class@Gom.Session]
 *
 * Rolls back the transaction associated with @self and closes the session.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the
 *   transaction has rolled back
 */
DexFuture *
gom_session_rollback (GomSession *self)
{
  dex_return_error_if_fail (GOM_IS_SESSION (self));

  GOM_TRACE_MARK ("Session", "rollback", "session=%" G_GINT64_FORMAT, self->id);

  return dex_future_then (_gom_session_rollback (self),
                          gom_session_commit_complete_cb,
                          g_object_ref (self),
                          g_object_unref);
}

/**
 * gom_session_dup_repository:
 * @self: a [class@Gom.Session]
 *
 * Gets the repository that created the session.
 *
 * Returns: (transfer full): the repository the session was created from
 */
GomRepository *
gom_session_dup_repository (GomSession *self)
{
  return _gom_session_dup_repository (self);
}

/**
 * gom_session_dup_coordinator:
 * @self: a [class@Gom.Session]
 *
 * Gets the sync coordinator from the session's repository.
 *
 * Returns: (transfer full) (nullable): the [class@Gom.SyncCoordinator]
 */
GomSyncCoordinator *
gom_session_dup_coordinator (GomSession *self)
{
  g_autoptr(GomRepository) repository = NULL;

  g_return_val_if_fail (GOM_IS_SESSION (self), NULL);

  if (!(repository = _gom_session_dup_repository (self)))
    return NULL;

  return gom_repository_dup_coordinator (repository);
}
