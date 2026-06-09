/* gom-entity.c
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

#include "gom-deletion-builder.h"
#include "gom-expression.h"
#include "gom-insertion-builder.h"
#include "gom-mutation-private.h"
#include "gom-deletion-private.h"
#include "gom-insertion-private.h"
#include "gom-mutation-result-private.h"
#include "gom-update-private.h"
#include "gom-meta-private.h"
#include "gom-query-private.h"
#include "gom-query-builder.h"
#include "gom-related-model.h"
#include "gom-repository-private.h"
#include "gom-cursor-private.h"
#include "gom-session-private.h"
#include "gom-sync-coordinator.h"
#include "gom-entity-private.h"
#include "gom-update-builder.h"
#include "gom-util-private.h"
#include "gom-value-private.h"
#include "gom-vector.h"

typedef struct _GomEntityPrivate
{
  GomRepository      *repository;
  GomSession         *session;
  char               *session_key;
  GHashTable         *baseline_values;
  GHashTable         *dirty_properties;
  GList               link;
  GList               pending_link;
  GList               dirty_link;
  GomEntityOrigin     origin;
  GomEntityLifecycle  lifecycle;
  guint               pending : 1;
  guint               dirty : 1;
  guint               baseline_complete : 1;
  gulong              change_notify_handler;
} GomEntityPrivate;

typedef struct
{
  GValue value;
  guint  has_value : 1;
} GomEntityTrackedValue;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GomEntity, gom_entity, G_TYPE_OBJECT)
G_DEFINE_ENUM_TYPE (GomEntityLifecycle, gom_entity_lifecycle,
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_LIFECYCLE_TRANSIENT, "GOM_ENTITY_LIFECYCLE_TRANSIENT"),
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_LIFECYCLE_PENDING, "GOM_ENTITY_LIFECYCLE_PENDING"),
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_LIFECYCLE_PERSISTENT, "GOM_ENTITY_LIFECYCLE_PERSISTENT"),
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_LIFECYCLE_DETACHED, "GOM_ENTITY_LIFECYCLE_DETACHED"),
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_LIFECYCLE_DELETED, "GOM_ENTITY_LIFECYCLE_DELETED"))
G_DEFINE_ENUM_TYPE (GomEntityOrigin, gom_entity_origin,
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_ORIGIN_CONSTRUCTED, "GOM_ENTITY_ORIGIN_CONSTRUCTED"),
                    G_DEFINE_ENUM_VALUE (GOM_ENTITY_ORIGIN_MATERIALIZED, "GOM_ENTITY_ORIGIN_MATERIALIZED"))

static GomExpression         *gom_entity_real_dup_identity_value         (GomEntity                  *self,
                                                                          const char                 *identity_field,
                                                                          GError                    **error);
static char                  *gom_entity_build_identity_key              (GomEntity                  *self);
static GomExpression         *gom_entity_build_identity_filter           (GomEntity                  *self,
                                                                          GomEntityClass             *entity_class,
                                                                          const char * const         *identity_fields,
                                                                          GError                    **error);
static GomExpression         *gom_entity_build_field_filter              (GomEntity                  *self,
                                                                          const char * const         *source_fields,
                                                                          const char * const         *target_fields,
                                                                          GError                    **error);
static gboolean               gom_entity_property_visible_at_version     (GomEntityClass             *entity_class,
                                                                          const char                 *property_name,
                                                                          guint                       version);
static gboolean               gom_entity_relationship_visible_at_version (GomEntityRelationshipInfo  *relationship,
                                                                          guint                       version);
static gboolean               gom_property_spec_visible_at_version       (GomPropertySpec            *property,
                                                                          guint                       version);
static const GomEntitySpec   *gom_entity_get_entity_spec                 (GomEntity                  *self);
static const GomPropertySpec *gom_entity_get_property_spec               (GomEntity                  *self,
                                                                          const char                 *property_name);
static const GomPropertySpec *gom_entity_get_property_spec_by_field      (GomEntity                  *self,
                                                                          const char                 *field_name);
static void                   gom_entity_real_attach                     (GomEntity                  *self,
                                                                          GomSession                 *session,
                                                                          char                       *entity_key);
static void                   gom_entity_real_detach                     (GomEntity                  *self);
static DexFuture             *gom_entity_mutate_run                      (GomEntity                  *self,
                                                                          GomMutation                *mutation,
                                                                          GomSession                 *session,
                                                                          GomRepository              *repository);
static gboolean               gom_entity_backfill_identity_from_record   (GomEntity                  *self,
                                                                          GomRecord                  *record);
static void                   gom_entity_tracked_value_clear             (GomEntityTrackedValue      *tracked);
static GomEntityTrackedValue *gom_entity_tracked_value_new               (const GValue               *value);
static void                   gom_entity_mark_property_dirty             (GomEntity                  *self,
                                                                          const char                 *property_name);
static void                   gom_entity_capture_current_state           (GomEntity                  *self,
                                                                          gboolean                    complete);

static GomEntity *
gom_entity_real_materialize (GomEntityClass      *klass,
                             GomCursor           *cursor,
                             const char * const  *property_names,
                             const GValue        *property_values,
                             guint                n_properties,
                             GError             **error)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);
  g_return_val_if_fail (GOM_IS_CURSOR (cursor), NULL);
  g_return_val_if_fail (n_properties == 0 || property_names != NULL, NULL);
  g_return_val_if_fail (n_properties == 0 || property_values != NULL, NULL);

  return GOM_ENTITY (g_object_new_with_properties (G_TYPE_FROM_CLASS (klass),
                                                   n_properties,
                                                   (const char **)property_names,
                                                   property_values));
}

static GomDelta *
gom_entity_real_build_delta (GomEntity  *self,
                             GError    **error)
{
  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return _gom_entity_build_delta (self, error);
}

static void
gom_entity_finalize (GObject *object)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private ((GomEntity *)object);

  g_clear_pointer (&priv->session_key, g_free);
  g_clear_pointer (&priv->baseline_values, g_hash_table_unref);
  g_clear_pointer (&priv->dirty_properties, g_hash_table_unref);

  G_OBJECT_CLASS (gom_entity_parent_class)->finalize (object);
}

static void
gom_entity_dispose (GObject *object)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private ((GomEntity *)object);

  g_clear_object (&priv->repository);
  g_clear_object (&priv->session);

  G_OBJECT_CLASS (gom_entity_parent_class)->dispose (object);
}

void
_gom_entity_set_origin (GomEntity       *self,
                        GomEntityOrigin  origin)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));

  priv->origin = origin;
}

void
_gom_entity_set_lifecycle (GomEntity          *self,
                           GomEntityLifecycle  lifecycle)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));

  priv->lifecycle = lifecycle;
}

static void
gom_entity_class_init (GomEntityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->materialize = gom_entity_real_materialize;
  klass->build_delta = gom_entity_real_build_delta;
  klass->dup_identity_value = gom_entity_real_dup_identity_value;
  klass->attach = gom_entity_real_attach;
  klass->detach = gom_entity_real_detach;

  object_class->dispose = gom_entity_dispose;
  object_class->finalize = gom_entity_finalize;
}

static void
gom_entity_init (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  priv->link.data = self;
  priv->pending_link.data = self;
  priv->dirty_link.data = self;
  priv->origin = GOM_ENTITY_ORIGIN_CONSTRUCTED;
  priv->lifecycle = GOM_ENTITY_LIFECYCLE_TRANSIENT;
}

static void
gom_entity_tracked_value_clear (GomEntityTrackedValue *tracked)
{
  if (tracked == NULL)
    return;

  if (tracked->has_value && G_IS_VALUE (&tracked->value))
    g_value_unset (&tracked->value);

  g_free (tracked);
}

static GomEntityTrackedValue *
gom_entity_tracked_value_new (const GValue *value)
{
  GomEntityTrackedValue *tracked;

  tracked = g_new0 (GomEntityTrackedValue, 1);

  if (value != NULL && G_IS_VALUE (value))
    {
      tracked->has_value = TRUE;
      g_value_init (&tracked->value, G_VALUE_TYPE (value));
      g_value_copy (value, &tracked->value);
    }

  return tracked;
}

static const GomEntitySpec *
gom_entity_get_entity_spec (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  if (priv->repository != NULL)
    return _gom_registry_lookup_entity_by_type (_gom_repository_get_registry (priv->repository),
                                               G_OBJECT_TYPE (self));

  return NULL;
}

static const GomPropertySpec *
gom_entity_get_property_spec (GomEntity  *self,
                              const char *property_name)
{
  const GomEntitySpec *entity_spec;

  if (!(entity_spec = gom_entity_get_entity_spec (self)))
    return NULL;

  return _gom_entity_spec_lookup_property_by_name ((GomEntitySpec *)entity_spec,
                                                  property_name);
}

static const GomPropertySpec *
gom_entity_get_property_spec_by_field (GomEntity  *self,
                                       const char *field_name)
{
  const GomEntitySpec *entity_spec;

  if (!(entity_spec = gom_entity_get_entity_spec (self)))
    return NULL;

  return _gom_entity_spec_lookup_property_by_field ((GomEntitySpec *)entity_spec,
                                                   field_name);
}

static void
gom_entity_mark_property_dirty (GomEntity  *self,
                                const char *property_name)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (property_name != NULL);

  if (priv->dirty_properties == NULL)
    priv->dirty_properties = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_add (priv->dirty_properties, (gpointer)g_intern_string (property_name));
}

/**
 * gom_entity_set_repository:
 * @self: a [class@Gom.Entity]
 * @repository: (nullable): a [class@Gom.Repository]
 *
 * Binds @self to @repository.
 *
 * Entity mutation helpers ([method@Gom.Entity.insert],
 * [method@Gom.Entity.update], and [method@Gom.Entity.delete])
 * require an entity to be bound to a repository.
 */
void
gom_entity_set_repository (GomEntity     *self,
                           GomRepository *repository)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (repository == NULL || GOM_IS_REPOSITORY (repository));

  g_set_object (&priv->repository, repository);
}

/**
 * gom_entity_dup_repository:
 * @self: a [class@Gom.Entity]
 *
 * Gets the bound repository for @self.
 *
 * Returns: (transfer full) (nullable): a [class@Gom.Repository]
 */
GomRepository *
gom_entity_dup_repository (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return priv->repository ? g_object_ref (priv->repository) : NULL;
}

GomEntityLifecycle
gom_entity_get_lifecycle (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), GOM_ENTITY_LIFECYCLE_TRANSIENT);

  return priv->lifecycle;
}

GomEntityOrigin
gom_entity_get_origin (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), GOM_ENTITY_ORIGIN_CONSTRUCTED);

  return priv->origin;
}

static char *
gom_entity_build_identity_key (GomEntity *self)
{
  g_autoptr(GString) key = NULL;
  GomEntityClass *entity_class;
  const GomPropertySpec *property_spec;
  GType value_type;
  const char * const *identity_fields;

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  entity_class = GOM_ENTITY_GET_CLASS (self);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  if (identity_fields == NULL || identity_fields[0] == NULL)
    return NULL;

  if (gom_entity_get_entity_spec (self) == NULL)
    return NULL;

  key = g_string_new (G_OBJECT_TYPE_NAME (self));
  g_string_append_c (key, '\n');

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      g_auto(GValue) value = G_VALUE_INIT;
      g_autofree char *value_contents = NULL;

      if (!(property_spec = gom_entity_get_property_spec (self, identity_fields[i])))
        return NULL;

      value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);

      g_value_init (&value, value_type);
      g_object_get_property (G_OBJECT (self), identity_fields[i], &value);
      value_contents = g_strdup_value_contents (&value);

      g_string_append (key, identity_fields[i]);
      g_string_append_c (key, '=');
      g_string_append (key, value_contents != NULL ? value_contents : "");
      g_string_append_c (key, '\n');
    }

  return g_string_free (g_steal_pointer (&key), FALSE);
}

static void
gom_entity_real_attach (GomEntity  *self,
                        GomSession *session,
                        char       *entity_key)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (session == NULL || GOM_IS_SESSION (session));

  g_set_object (&priv->session, session);
  g_clear_pointer (&priv->session_key, g_free);
  priv->session_key = entity_key;
  priv->link.data = self;
  priv->pending_link.data = self;
  priv->dirty_link.data = self;

  if (entity_key != NULL)
    {
      priv->lifecycle = GOM_ENTITY_LIFECYCLE_PERSISTENT;
    }
  else
    {
      priv->pending = TRUE;
      priv->lifecycle = GOM_ENTITY_LIFECYCLE_PENDING;
    }
}

static void
gom_entity_real_detach (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);
  gboolean had_key;

  g_return_if_fail (GOM_IS_ENTITY (self));

  had_key = priv->session_key != NULL;

  _gom_entity_untrack_changes (self);
  _gom_entity_clear_change_state (self);

  g_clear_object (&priv->session);
  g_clear_pointer (&priv->session_key, g_free);

  priv->pending = FALSE;
  priv->dirty = FALSE;

  priv->lifecycle = had_key ? GOM_ENTITY_LIFECYCLE_DETACHED : GOM_ENTITY_LIFECYCLE_TRANSIENT;
}

GomSession *
_gom_entity_dup_session (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return priv->session ? g_object_ref (priv->session) : NULL;
}

char *
_gom_entity_dup_session_key (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  if (priv->session_key == NULL)
    priv->session_key = gom_entity_build_identity_key (self);

  return priv->session_key ? g_strdup (priv->session_key) : NULL;
}

GList *
_gom_entity_get_session_link (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return &priv->link;
}

GList *
_gom_entity_get_pending_link (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return &priv->pending_link;
}

GList *
_gom_entity_get_dirty_link (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return &priv->dirty_link;
}

gboolean
_gom_entity_is_pending (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), FALSE);

  return priv->pending != FALSE;
}

void
_gom_entity_set_pending (GomEntity *self,
                         gboolean   pending)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));

  priv->pending = pending != FALSE;
}

gboolean
_gom_entity_is_dirty (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), FALSE);

  return priv->dirty != FALSE;
}

void
_gom_entity_set_dirty (GomEntity *self,
                       gboolean   dirty)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));

  priv->dirty = dirty != FALSE;
}

void
_gom_entity_attach (GomEntity  *self,
                    GomSession *session,
                    char       *entity_key)
{
  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (session == NULL || GOM_IS_SESSION (session));

  GOM_ENTITY_GET_CLASS (self)->attach (self, session, entity_key);
}

static void
gom_entity_notify_cb (GomEntity  *self,
                      GParamSpec *pspec,
                      GomSession *session)
{
  GomEntityClass *entity_class;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_SESSION (session));

  if (_gom_entity_is_pending (self))
    return;

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self));
  if (!gom_entity_class_property_get_mapped (entity_class, pspec->name))
    return;

  gom_entity_mark_property_dirty (self, pspec->name);
  _gom_session_mark_entity_dirty (session, self);
}

void
_gom_entity_track_changes (GomEntity  *self,
                           GomSession *session)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (session == NULL || GOM_IS_SESSION (session));

  if (session == NULL || priv->change_notify_handler != 0)
    return;

  priv->change_notify_handler =
    g_signal_connect_object (self,
                             "notify",
                             G_CALLBACK (gom_entity_notify_cb),
                             session,
                             0);
}

void
_gom_entity_untrack_changes (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));

  if (priv->change_notify_handler != 0)
    g_clear_signal_handler (&priv->change_notify_handler, self);
}

void
_gom_entity_capture_change_state (GomEntity          *self,
                                  const char * const *property_names,
                                  const GValue       *property_values,
                                  guint               n_properties,
                                  gboolean            complete)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (n_properties == 0 || property_names != NULL);
  g_return_if_fail (n_properties == 0 || property_values != NULL);

  g_clear_pointer (&priv->baseline_values, g_hash_table_unref);
  priv->baseline_values = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 NULL,
                                                 (GDestroyNotify)gom_entity_tracked_value_clear);
  priv->baseline_complete = complete != FALSE;

  for (guint i = 0; i < n_properties; i++)
    {
      GomEntityTrackedValue *tracked;

      if (property_names[i] == NULL || !G_IS_VALUE (&property_values[i]))
        continue;

      tracked = gom_entity_tracked_value_new (&property_values[i]);
      g_hash_table_insert (priv->baseline_values,
                           (gpointer)g_intern_string (property_names[i]),
                           tracked);
    }

  if (priv->dirty_properties != NULL)
    g_hash_table_remove_all (priv->dirty_properties);

  priv->dirty = FALSE;
}

void
_gom_entity_clear_change_state (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));

  g_clear_pointer (&priv->baseline_values, g_hash_table_unref);
  if (priv->dirty_properties != NULL)
    g_hash_table_remove_all (priv->dirty_properties);

  priv->baseline_complete = FALSE;
  priv->dirty = FALSE;
}

static void
gom_entity_apply_change_entry_to_baseline (GomEntityPrivate *priv,
                                           GomDelta         *delta,
                                           guint             index)
{
  const char *property_name;
  GValue current = G_VALUE_INIT;
  GomEntityTrackedValue *tracked;

  if (!(property_name = gom_delta_get_property_name (delta, index)))
    return;

  if (!gom_delta_get_current_value (delta, index, &current))
    return;

  if (priv->baseline_values == NULL)
    priv->baseline_values = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   NULL,
                                                   (GDestroyNotify)gom_entity_tracked_value_clear);

  if (!(tracked = g_hash_table_lookup (priv->baseline_values, property_name)))
    {
      tracked = gom_entity_tracked_value_new (&current);
      g_hash_table_insert (priv->baseline_values,
                           (gpointer)g_intern_string (property_name),
                           tracked);
    }
  else
    {
      if (tracked->has_value && G_IS_VALUE (&tracked->value))
        g_value_unset (&tracked->value);

      tracked->has_value = TRUE;
      g_value_init (&tracked->value, G_VALUE_TYPE (&current));
      g_value_copy (&current, &tracked->value);
    }

  g_value_unset (&current);
}

void
_gom_entity_apply_delta (GomEntity *self,
                         GomDelta  *delta,
                         gboolean   complete)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_if_fail (GOM_IS_ENTITY (self));
  g_return_if_fail (delta == NULL || GOM_IS_DELTA (delta));

  if (delta != NULL)
    {
      for (guint i = 0; i < gom_delta_get_n_changes (delta); i++)
        gom_entity_apply_change_entry_to_baseline (priv, delta, i);
    }

  if (complete)
    priv->baseline_complete = TRUE;

  if (priv->dirty_properties != NULL)
    g_hash_table_remove_all (priv->dirty_properties);

  priv->dirty = FALSE;
}

gboolean
_gom_entity_change_state_is_complete (GomEntity *self)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_ENTITY (self), TRUE);

  return priv->baseline_values == NULL || priv->baseline_complete;
}

static void
gom_entity_capture_current_state (GomEntity *self,
                                  gboolean   complete)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);
  GomEntityClass *entity_class;
  const GomEntitySpec *entity_spec;
  const GomPropertySpec * const *properties = NULL;
  const GomPropertySpec *property_spec;
  const char * const *identity_fields;
  guint n_properties = 0;
  guint version = 0;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_REPOSITORY (priv->repository));

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self));
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  version = gom_registry_get_version (_gom_repository_get_registry (priv->repository));

  if (!(entity_spec = gom_entity_get_entity_spec (self)))
    return;

  properties = gom_entity_spec_list_properties ((GomEntitySpec *)entity_spec, &n_properties);

  g_clear_pointer (&priv->baseline_values, g_hash_table_unref);
  priv->baseline_values = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 NULL,
                                                 (GDestroyNotify)gom_entity_tracked_value_clear);

  priv->baseline_complete = complete != FALSE;

  for (guint i = 0; i < n_properties; i++)
    {
      g_auto(GValue) value = G_VALUE_INIT;
      GType value_type;
      const char *property_name;

      property_spec = properties[i];
      property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec);

      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
        continue;

      if (!gom_property_spec_visible_at_version ((GomPropertySpec *)property_spec, version))
        continue;

      if (_gom_strv_contains (identity_fields, property_name))
        continue;

      value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);
      if (value_type == G_TYPE_INVALID)
        continue;

      g_value_init (&value, value_type);
      g_object_get_property (G_OBJECT (self), property_name, &value);

      g_hash_table_insert (priv->baseline_values,
                           (gpointer)g_intern_string (property_name),
                           gom_entity_tracked_value_new (&value));
    }

  if (priv->dirty_properties != NULL)
    g_hash_table_remove_all (priv->dirty_properties);

  priv->dirty = FALSE;
}

GomDelta *
_gom_entity_build_delta (GomEntity  *self,
                         GError    **error)
{
  GomEntityPrivate *priv = gom_entity_get_instance_private (self);
  GomEntityClass *entity_class;
  const GomEntitySpec *entity_spec;
  const GomPropertySpec * const *properties = NULL;
  const GomPropertySpec *property_spec;
  g_autoptr(GomDelta) delta = NULL;
  const char * const *identity_fields;
  guint n_properties = 0;
  guint version = 0;
  gboolean have_dirty = FALSE;

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);
  g_return_val_if_fail (GOM_IS_REPOSITORY (priv->repository), NULL);

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self));
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  version = gom_registry_get_version (_gom_repository_get_registry (priv->repository));
  delta = gom_delta_new (G_OBJECT_TYPE (self), GOM_DELTA_KIND_UPDATE);

  if (priv->dirty_properties != NULL)
    have_dirty = g_hash_table_size (priv->dirty_properties) > 0;

  if (!(entity_spec = gom_entity_get_entity_spec (self)))
    return NULL;

  properties = gom_entity_spec_list_properties ((GomEntitySpec *)entity_spec, &n_properties);

  if (priv->baseline_values == NULL)
    {
      for (guint i = 0; i < n_properties; i++)
        {
          g_auto(GValue) value = G_VALUE_INIT;
          GType value_type;
          const char *property_name;

          property_spec = properties[i];
          property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec);

          if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
            continue;

          if (!gom_property_spec_visible_at_version ((GomPropertySpec *)property_spec, version))
            continue;

          if (_gom_strv_contains (identity_fields, property_name))
            continue;

          value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);
          if (value_type == G_TYPE_INVALID)
            continue;

          g_value_init (&value, value_type);
          g_object_get_property (G_OBJECT (self), property_name, &value);

          gom_delta_add_property (delta, property_name, NULL, &value);
        }

      if (gom_delta_is_empty (delta))
        return NULL;

      return g_steal_pointer (&delta);
    }

  for (guint i = 0; i < n_properties; i++)
    {
      g_auto(GValue) value = G_VALUE_INIT;
      GomEntityTrackedValue *tracked = NULL;
      const char *property_name;
      GType value_type;

      property_spec = properties[i];
      property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec);

      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
        continue;

      if (!gom_property_spec_visible_at_version ((GomPropertySpec *)property_spec, version))
        continue;

      if (_gom_strv_contains (identity_fields, property_name))
        continue;

      value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);
      if (value_type == G_TYPE_INVALID)
        continue;

      g_value_init (&value, value_type);
      g_object_get_property (G_OBJECT (self), property_name, &value);

      if (priv->baseline_values != NULL)
        tracked = g_hash_table_lookup (priv->baseline_values, property_name);

      if (tracked != NULL && tracked->has_value)
        {
          if (_gom_value_equal (&tracked->value, &value))
            continue;

          gom_delta_add_property (delta, property_name, &tracked->value, &value);
          continue;
        }

      if (!priv->baseline_complete && !have_dirty)
        continue;

      if (!have_dirty)
        continue;

      if (priv->dirty_properties != NULL &&
          !g_hash_table_contains (priv->dirty_properties, g_intern_string (property_name)))
        continue;

      gom_delta_add_property (delta, property_name, NULL, &value);
    }

  if (gom_delta_is_empty (delta))
    return NULL;

  return g_steal_pointer (&delta);
}

static void
gom_entity_get_property_value (GomEntity  *self,
                               const char *property_name,
                               GValue     *out_value)
{
  const GomPropertySpec *property_spec;
  GType value_type;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (property_name != NULL);
  g_assert (out_value != NULL);

  if (!(property_spec = gom_entity_get_property_spec (self, property_name)))
    g_assert_not_reached ();

  value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);
  g_assert (value_type != G_TYPE_INVALID);

  g_value_init (out_value, value_type);
  g_object_get_property (G_OBJECT (self), property_name, out_value);
}

static GomDelta *
gom_entity_build_snapshot_delta (GomEntity      *self,
                                 GomRepository  *repository,
                                 GomDeltaKind    kind,
                                 GError        **error)
{
  GomRegistry *registry;
  const GomEntitySpec *entity_spec;
  const GomPropertySpec * const *properties;
  g_autoptr(GomDelta) delta = NULL;
  guint n_properties = 0;
  guint version = 0;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_REPOSITORY (repository));

  registry = _gom_repository_get_registry (repository);

  if (!(entity_spec = _gom_registry_lookup_entity_by_type (registry, G_OBJECT_TYPE (self))))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type `%s` is not registered with the repository",
                   G_OBJECT_TYPE_NAME (self));
      return NULL;
    }

  version = gom_registry_get_version (registry);
  delta = gom_delta_new (G_OBJECT_TYPE (self), kind);
  properties = gom_entity_spec_list_properties ((GomEntitySpec *)entity_spec, &n_properties);

  for (guint i = 0; i < n_properties; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)properties[i];
      const char *property_name;
      g_auto(GValue) value = G_VALUE_INIT;

      if (!gom_property_spec_get_mapped (property))
        continue;

      if (!gom_property_spec_visible_at_version (property, version))
        continue;

      if (!(property_name = gom_property_spec_get_name (property)))
        continue;

      gom_entity_get_property_value (self, property_name, &value);

      if (kind == GOM_DELTA_KIND_DELETE)
        gom_delta_add_property (delta, property_name, &value, NULL);
      else
        gom_delta_add_property (delta, property_name, NULL, &value);
    }

  if (gom_delta_is_empty (delta))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Entity has no mapped properties for change capture");
      return NULL;
    }

  return g_steal_pointer (&delta);
}

/**
 * gom_entity_build_delta:
 * @self: a [class@Gom.Entity]
 * @error: a location for a #GError
 *
 * Returns: (transfer full):
 */
GomDelta *
gom_entity_build_delta (GomEntity  *self,
                        GError    **error)
{
  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);

  return GOM_ENTITY_GET_CLASS (self)->build_delta (self, error);
}

void
gom_entity_clear_change_tracking (GomEntity *self)
{
  g_return_if_fail (GOM_IS_ENTITY (self));

  _gom_entity_clear_change_state (self);
}

void
_gom_entity_detach (GomEntity *self)
{
  g_return_if_fail (GOM_IS_ENTITY (self));

  GOM_ENTITY_GET_CLASS (self)->detach (self);
}

gboolean
gom_entity_rekey_session_identity (GomEntity *self)
{
  g_autofree char *entity_key = NULL;
  g_autoptr(GomSession) session = NULL;

  g_return_val_if_fail (GOM_IS_ENTITY (self), FALSE);

  if (!(session = _gom_entity_dup_session (self)))
    return FALSE;

  if (!(entity_key = gom_entity_build_identity_key (self)))
    return FALSE;

  return _gom_session_rekey_entity_identity (session, self, g_steal_pointer (&entity_key));
}

GomEntityClassInfo *
_gom_entity_class_get_info (GomEntityClass *klass,
                            gboolean        create)
{
  GomEntityClassInfo *info;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);

  info = klass->_info;

  if (info != NULL)
    {
      if (info->klass == klass)
        return info;

      if (!create)
        return info;
    }

  if (create)
    {
      info = g_new0 (GomEntityClassInfo, 1);
      info->klass = klass;
      info->parent_info = klass->_info;

      klass->_info = info;

      return info;
    }

  return NULL;
}

#define FOREACH_INFO_TO_ROOT(_klass, _name, _block)                       \
{                                                                         \
  GomEntityClassInfo *_name = _gom_entity_class_get_info (_klass, FALSE); \
                                                                          \
  while ((_name) != NULL)                                                 \
    {                                                                     \
      _block                                                              \
      _name = (_name)->parent_info;                                       \
    }                                                                     \
}

GomEntityPropertyInfo *
_gom_entity_class_get_property (GomEntityClass *klass,
                                const char     *property_name,
                                gboolean        create)
{
  GomEntityPropertyInfo *prop;
  GomEntityClassInfo *class_info;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  FOREACH_INFO_TO_ROOT (klass, iter, {
    for (prop = iter->properties; prop; prop = prop->next)
      {
        if (g_str_equal (property_name, prop->property_name))
          return prop;
      }
  })

  if (create == FALSE)
    return NULL;

  prop = g_new0 (GomEntityPropertyInfo, 1);
  prop->property_name = g_intern_string (property_name);
  class_info = _gom_entity_class_get_info (klass, TRUE);
  prop->next = class_info->properties;
  class_info->properties = prop;

  return prop;
}

GomEntityRelationshipInfo *
_gom_entity_class_get_relationship (GomEntityClass *klass,
                                    const char     *name,
                                    gboolean        create)
{
  GomEntityRelationshipInfo *relationship;
  GomEntityClassInfo *class_info;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  FOREACH_INFO_TO_ROOT (klass, iter, {
    for (relationship = iter->relationships; relationship; relationship = relationship->next)
      {
        if (g_str_equal (name, relationship->name))
          return relationship;
      }
  })

  if (create == FALSE)
    return NULL;

  relationship = g_new0 (GomEntityRelationshipInfo, 1);
  relationship->name = g_intern_string (name);
  class_info = _gom_entity_class_get_info (klass, TRUE);
  relationship->next = class_info->relationships;
  class_info->relationships = relationship;

  return relationship;
}

void
gom_entity_class_set_identity_fields (GomEntityClass     *klass,
                                      const char * const *identity_fields)
{
  GomEntityClassInfo *info;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  info = _gom_entity_class_get_info (klass, TRUE);
  _gom_set_strv (&info->identity_fields, identity_fields);
}

const char * const *
gom_entity_class_get_identity_fields (GomEntityClass *klass)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);

  FOREACH_INFO_TO_ROOT (klass, info, {
    if (info->identity_fields != NULL)
      return (const char * const *)info->identity_fields;
  })

  return NULL;
}

void
gom_entity_class_set_identity_field (GomEntityClass *klass,
                                     const char     *identity_field)
{
  const char *fields[] = { identity_field, NULL };

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (identity_field != NULL);

  gom_entity_class_set_identity_fields (klass, fields);
}

void
gom_entity_class_set_relation (GomEntityClass *klass,
                               const char     *relation)
{
  GomEntityClassInfo *info;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  info = _gom_entity_class_get_info (klass, TRUE);
  info->table = g_intern_string (relation);
}

const char *
gom_entity_class_get_relation (GomEntityClass *klass)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);

  FOREACH_INFO_TO_ROOT (klass, info, {
    if (info->table != NULL)
      return info->table;
  })

  return NULL;
}

void
gom_entity_class_set_discriminator_field (GomEntityClass *klass,
                                          const char     *discriminator_field)
{
  GomEntityClassInfo *info;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (discriminator_field != NULL);

  info = _gom_entity_class_get_info (klass, TRUE);
  info->discriminator_field = g_intern_string (discriminator_field);
}

const char *
gom_entity_class_get_discriminator_field (GomEntityClass *klass)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);

  FOREACH_INFO_TO_ROOT (klass, info, {
    if (info->discriminator_field != NULL)
      return info->discriminator_field;
  })

  return NULL;
}

void
gom_entity_class_set_discriminator_value (GomEntityClass *klass,
                                          const char     *discriminator_value)
{
  GomEntityClassInfo *info;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (discriminator_value != NULL);

  info = _gom_entity_class_get_info (klass, TRUE);
  info->discriminator_value = g_intern_string (discriminator_value);
}

const char *
gom_entity_class_get_discriminator_value (GomEntityClass *klass)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);

  FOREACH_INFO_TO_ROOT (klass, info, {
    if (info->discriminator_value != NULL)
      return info->discriminator_value;
  })

  return NULL;
}

void
gom_entity_class_set_version_added (GomEntityClass *klass,
                                    guint           version_added)
{
  GomEntityClassInfo *info;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  info = _gom_entity_class_get_info (klass, TRUE);
  info->version_added = version_added;
}

guint
gom_entity_class_get_version_added (GomEntityClass *klass)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), 0);

  FOREACH_INFO_TO_ROOT (klass, info, {
    if (info->version_added)
      return info->version_added;
  })

  return 0;
}

void
gom_entity_class_set_version_removed (GomEntityClass *klass,
                                      guint           version_removed)
{
  GomEntityClassInfo *info;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  info = _gom_entity_class_get_info (klass, TRUE);
  info->version_removed = version_removed;
}

guint
gom_entity_class_get_version_removed (GomEntityClass *klass)
{
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), 0);

  FOREACH_INFO_TO_ROOT (klass, info, {
    if (info->version_removed)
      return info->version_removed;
  })

  return 0;
}

void
gom_entity_class_property_set_nonnull (GomEntityClass *klass,
                                       const char     *property_name,
                                       gboolean        nonnull)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->nonnull = !!nonnull;
}

gboolean
gom_entity_class_property_get_nonnull (GomEntityClass *klass,
                                       const char     *property_name)
{
  GomEntityPropertyInfo *prop;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), 0);

  if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
    return prop->nonnull;

  return FALSE;
}

void
gom_entity_class_property_set_unique (GomEntityClass *klass,
                                      const char     *property_name,
                                      gboolean        unique)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->unique = !!unique;
}

gboolean
gom_entity_class_property_get_unique (GomEntityClass *klass,
                                      const char     *property_name)
{
  GomEntityPropertyInfo *prop;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), 0);

  if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
    return prop->unique;

  return FALSE;
}

/**
 * gom_entity_class_property_set_mapped:
 * @klass: a [struct@Gom.EntityClass]
 * @property_name: the name of the property
 * @mapped: whether the property participates in the entity data model
 *
 * Marks a property as mapped data.
 *
 * Call this during class initialization for each property that should be
 * persisted, materialized, and considered by entity CRUD helpers. Properties
 * left unmapped remain regular in-memory GObject state.
 */
void
gom_entity_class_property_set_mapped (GomEntityClass *klass,
                                      const char     *property_name,
                                      gboolean        mapped)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->ignored = !mapped;
}

gboolean
gom_entity_class_property_get_mapped (GomEntityClass *klass,
                                      const char     *property_name)
{
  GomEntityPropertyInfo *prop;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), FALSE);

  if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
    return !prop->ignored;

  return TRUE;
}

GomSearchFlags
gom_entity_class_property_get_search_flags (GomEntityClass *klass,
                                            const char     *property_name)
{
  GomEntityPropertyInfo *prop;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), GOM_SEARCH_NONE);

  if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
    return prop->search_flags;

  return GOM_SEARCH_NONE;
}

void
gom_entity_class_property_set_search_flags (GomEntityClass *klass,
                                            const char     *property_name,
                                            GomSearchFlags  search_flags)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->search_flags = search_flags;
}

void
gom_entity_class_property_set_version_added (GomEntityClass *klass,
                                             const char     *property_name,
                                             guint           version_added)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->version_added = version_added;
}

guint
gom_entity_class_property_get_version_added (GomEntityClass *klass,
                                             const char     *property_name)
{
  GomEntityPropertyInfo *prop;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), 0);

  if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
    return prop->version_added;

  return 0;
}

void
gom_entity_class_property_set_version_removed (GomEntityClass *klass,
                                               const char     *property_name,
                                               guint           version_removed)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->version_removed = version_removed;
}

guint
gom_entity_class_property_get_version_removed (GomEntityClass *klass,
                                               const char     *property_name)
{
  GomEntityPropertyInfo *prop;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), 0);

  if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
    return prop->version_removed;

  return 0;
}

void
gom_entity_class_property_set_reference (GomEntityClass *klass,
                                         const char     *property_name,
                                         const char     *ref_table,
                                         const char     *ref_field)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (ref_table != NULL);
  g_return_if_fail (ref_field != NULL);

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->ref_table = g_intern_string (ref_table);
  prop->ref_field = g_intern_string (ref_field);
}

void
gom_entity_class_property_set_field_name (GomEntityClass *klass,
                                          const char     *property_name,
                                          const char     *field_name)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (field_name != NULL);

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);
  prop->field_name = g_intern_string (field_name);
}

static void
gom_entity_class_add_relationship (GomEntityClass             *klass,
                                   const char                 *name,
                                   GType                       target_type,
                                   GomRelationshipCardinality  cardinality,
                                   GomRelationshipStorage      storage,
                                   const char * const         *local_fields,
                                   const char * const         *remote_fields,
                                   const char                 *join_relation,
                                   const char * const         *join_local_fields,
                                   const char * const         *join_remote_fields,
                                   const char                 *inverse_name,
                                   gboolean                    optional,
                                   gboolean                    ordered,
                                   guint                       min_count,
                                   guint                       max_count,
                                   GomRelationshipDeleteRule   delete_rule)
{
  GomEntityRelationshipInfo *relationship;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (name != NULL);
  g_return_if_fail (g_type_is_a (target_type, G_TYPE_OBJECT));

  relationship = _gom_entity_class_get_relationship (klass, name, TRUE);
  relationship->target_type = target_type;
  relationship->cardinality = cardinality;
  relationship->storage = storage;
  relationship->inverse_name = inverse_name ? g_intern_string (inverse_name) : NULL;
  _gom_set_strv (&relationship->local_fields, local_fields);
  _gom_set_strv (&relationship->remote_fields, remote_fields);
  relationship->join_relation = join_relation ? g_intern_string (join_relation) : NULL;
  _gom_set_strv (&relationship->join_local_fields, join_local_fields);
  _gom_set_strv (&relationship->join_remote_fields, join_remote_fields);
  relationship->version_added = gom_entity_class_get_version_added (klass);
  if (relationship->version_added == 0)
    relationship->version_added = 1;
  relationship->optional = !!optional;
  relationship->ordered = !!ordered;
  relationship->min_count = min_count;
  relationship->max_count = max_count;
  relationship->delete_rule = delete_rule;
}

void
gom_entity_class_add_many_to_one (GomEntityClass *klass,
                                  const char     *name,
                                  GType           target_type,
                                  const char     *local_field,
                                  const char     *inverse_name)
{
  const char *local_fields[] = { local_field, NULL };

  g_return_if_fail (local_field != NULL);

  gom_entity_class_add_relationship (klass,
                                     name,
                                     target_type,
                                     GOM_RELATIONSHIP_CARDINALITY_TO_ONE,
                                     GOM_RELATIONSHIP_STORAGE_FK,
                                     local_fields,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     inverse_name,
                                     TRUE,
                                     FALSE,
                                     0,
                                     1,
                                     GOM_RELATIONSHIP_DELETE_NULLIFY);
}

void
gom_entity_class_add_one_to_many (GomEntityClass *klass,
                                  const char     *name,
                                  GType           target_type,
                                  const char     *foreign_field,
                                  const char     *inverse_name)
{
  const char *remote_fields[] = { foreign_field, NULL };

  g_return_if_fail (foreign_field != NULL);

  gom_entity_class_add_relationship (klass,
                                     name,
                                     target_type,
                                     GOM_RELATIONSHIP_CARDINALITY_TO_MANY,
                                     GOM_RELATIONSHIP_STORAGE_FK,
                                     NULL,
                                     remote_fields,
                                     NULL,
                                     NULL,
                                     NULL,
                                     inverse_name,
                                     TRUE,
                                     FALSE,
                                     0,
                                     0,
                                     GOM_RELATIONSHIP_DELETE_NULLIFY);
}

void
gom_entity_class_add_many_to_many (GomEntityClass *klass,
                                   const char     *name,
                                   GType           target_type,
                                   const char     *join_relation,
                                   const char     *join_local_field,
                                   const char     *join_remote_field,
                                   const char     *inverse_name)
{
  const char *join_local_fields[] = { join_local_field, NULL };
  const char *join_remote_fields[] = { join_remote_field, NULL };

  g_return_if_fail (join_relation != NULL);
  g_return_if_fail (join_local_field != NULL);
  g_return_if_fail (join_remote_field != NULL);

  gom_entity_class_add_relationship (klass,
                                     name,
                                     target_type,
                                     GOM_RELATIONSHIP_CARDINALITY_TO_MANY,
                                     GOM_RELATIONSHIP_STORAGE_JOIN_TABLE,
                                     NULL,
                                     NULL,
                                     join_relation,
                                     join_local_fields,
                                     join_remote_fields,
                                     inverse_name,
                                     TRUE,
                                     FALSE,
                                     0,
                                     0,
                                     GOM_RELATIONSHIP_DELETE_CASCADE);
}

void
gom_entity_class_relationship_set_delete_rule (GomEntityClass            *klass,
                                               const char                *name,
                                               GomRelationshipDeleteRule  delete_rule)
{
  GomEntityRelationshipInfo *relationship;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (name != NULL);

  relationship = _gom_entity_class_get_relationship (klass, name, FALSE);

  g_return_if_fail (relationship != NULL);

  relationship->delete_rule = delete_rule;
}

/**
 * gom_entity_class_property_set_byte_transform:
 * @klass: a [struct@Gom.EntityClass]
 * @property_name: the name of the property
 * @to_bytes_func: (scope notified): the transform from value to bytes
 * @from_bytes_func: (scope notified): the transform from bytes to value
 * @user_data: closure data for @to_bytes_func and @from_bytes_func
 * @notify: (nullable): closure notify for @user_data
 *
 * Sets a transform to be used to convert a field to and from a byte
 * buffer suitable for storage.
 */
void
gom_entity_class_property_set_byte_transform (GomEntityClass   *klass,
                                              const char       *property_name,
                                              GomToBytesFunc    to_bytes_func,
                                              GomFromBytesFunc  from_bytes_func,
                                              gpointer          user_data,
                                              GDestroyNotify    notify)
{
  GomEntityPropertyInfo *prop;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (property_name != NULL);

  prop = _gom_entity_class_get_property (klass, property_name, TRUE);

  if (prop->bytes_notify != NULL)
    prop->bytes_notify (prop->bytes_user_data);

  prop->to_bytes_func = to_bytes_func;
  prop->from_bytes_func = from_bytes_func;
  prop->bytes_user_data = user_data;
  prop->bytes_notify = notify;
}

typedef struct
{
  GomVectorFormat format;
  guint           dimensions;
} GomEntityVectorTransform;

static GBytes *
gom_entity_vector_to_bytes (const GValue  *value,
                            gpointer       user_data,
                            GError       **error)
{
  GomEntityVectorTransform *transform = user_data;
  GomVector *vector;

  g_assert (value != NULL);
  g_assert (transform != NULL);

  if (!G_VALUE_HOLDS (value, GOM_TYPE_VECTOR))
    return NULL;

  if (!(vector = g_value_get_boxed (value)))
    return NULL;

  if (gom_vector_get_format (vector) != transform->format ||
      gom_vector_get_dimensions (vector) != transform->dimensions)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Vector format or dimensions do not match property mapping");
      return NULL;
    }

  return gom_vector_dup_bytes (vector);
}

static gboolean
gom_entity_vector_from_bytes (GBytes    *bytes,
                              GValue    *value,
                              gpointer   user_data,
                              GError   **error)
{
  GomEntityVectorTransform *transform = user_data;
  g_autoptr(GomVector) vector = NULL;

  g_assert (value != NULL);
  g_assert (transform != NULL);

  g_value_init (value, GOM_TYPE_VECTOR);

  if (bytes == NULL)
    {
      g_value_set_boxed (value, NULL);
      return TRUE;
    }

  if (!(vector = gom_vector_new (transform->format, transform->dimensions, bytes, error)))
    return FALSE;

  g_value_take_boxed (value, g_steal_pointer (&vector));

  return TRUE;
}

/**
 * gom_entity_class_property_set_vector:
 * @klass: a [struct@Gom.EntityClass]
 * @property_name: the name of the vector property
 * @format: the vector storage format
 * @dimensions: the number of dimensions expected for the vector
 *
 * Configures @property_name as a [struct@Gom.Vector] property stored as bytes.
 *
 * This is a convenience wrapper around
 * [method@Gom.EntityClass.property_set_byte_transform] for entity properties
 * whose value type is `GOM_TYPE_VECTOR`.
 */
void
gom_entity_class_property_set_vector (GomEntityClass  *klass,
                                      const char      *property_name,
                                      GomVectorFormat  format,
                                      guint            dimensions)
{
  GomEntityVectorTransform *transform;

  g_return_if_fail (GOM_IS_ENTITY_CLASS (klass));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (dimensions > 0);

  transform = g_new0 (GomEntityVectorTransform, 1);
  transform->format = format;
  transform->dimensions = dimensions;

  gom_entity_class_property_set_byte_transform (klass,
                                                property_name,
                                                gom_entity_vector_to_bytes,
                                                gom_entity_vector_from_bytes,
                                                transform,
                                                g_free);
}

static gboolean
gom_entity_value_matches_default (const GValue *default_value,
                                  const GValue *value)
{
  g_assert (default_value == NULL || G_IS_VALUE (default_value));
  g_assert (value != NULL);

  if (default_value == NULL)
    return FALSE;

  if (G_VALUE_TYPE (default_value) != G_VALUE_TYPE (value))
    return FALSE;

  return _gom_value_equal (default_value, value);
}

static gboolean
gom_entity_value_is_default (const GomPropertySpec *property_spec,
                             const GValue          *value)
{
  const GValue *default_value;

  g_assert (property_spec != NULL);
  g_assert (value != NULL);

  default_value = gom_property_spec_get_default_value ((GomPropertySpec *)property_spec);

  return gom_entity_value_matches_default (default_value, value);
}

gboolean
gom_entity_get_property_storage_value (GomEntity       *self,
                                       GomEntityClass  *entity_class,
                                       GObjectClass    *object_class,
                                       const char      *property_name,
                                       GValue          *out_value,
                                       GError         **error)
{
  GomEntityPropertyInfo *prop_info;
  const GomPropertySpec *property_spec;
  GType value_type;
  g_auto(GValue) value = G_VALUE_INIT;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (object_class != NULL);
  g_assert (property_name != NULL);
  g_assert (out_value != NULL);

  if (!(property_spec = gom_entity_get_property_spec (self, property_name)) &&
      !(property_spec = gom_entity_get_property_spec_by_field (self, property_name)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity does not have property '%s'",
                   property_name);
      return FALSE;
    }

  value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);

  if (value_type == G_TYPE_INVALID)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity property '%s' does not have a value type",
                   property_name);
      return FALSE;
    }

  property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec);
  g_value_init (&value, value_type);
  g_object_get_property (G_OBJECT (self), property_name, &value);

  prop_info = _gom_entity_class_get_property (entity_class, property_name, FALSE);
  if (prop_info != NULL && prop_info->to_bytes_func != NULL)
    {
      GBytes *bytes = prop_info->to_bytes_func (&value, prop_info->bytes_user_data, error);

      if (bytes == NULL && error != NULL && *error != NULL)
        return FALSE;

      g_value_init (out_value, G_TYPE_BYTES);
      g_value_take_boxed (out_value, bytes);

      return TRUE;
    }

  if (G_VALUE_HOLDS (&value, G_TYPE_GTYPE))
    {
      const char *type_name = g_type_name (g_value_get_gtype (&value));

      g_value_init (out_value, G_TYPE_STRING);
      g_value_set_string (out_value, type_name);

      return TRUE;
    }

  g_value_init (out_value, G_VALUE_TYPE (&value));
  g_value_copy (&value, out_value);

  return TRUE;
}

typedef struct
{
  GomEntity     *self;
  GomSession    *session;
  GomRepository *repository;
  GomRegistry   *registry;
  GHashTable    *visited;
} GomEntityDeleteTask;

static DexFuture *
gom_entity_delete_run_mutation (GomEntityDeleteTask *task,
                                GomMutation         *mutation)
{
  g_assert (task != NULL);
  g_assert (GOM_IS_MUTATION (mutation));

  if (task->session != NULL)
    return _gom_session_mutate (task->session, mutation);

  return gom_repository_mutate (task->repository, mutation);
}

static DexFuture *
gom_entity_mutate_run (GomEntity     *self,
                       GomMutation   *mutation,
                       GomSession    *session,
                       GomRepository *repository)
{
  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_MUTATION (mutation));
  g_assert (GOM_IS_REPOSITORY (repository));

  if (session != NULL)
    return _gom_session_mutate (session, mutation);

  return gom_repository_mutate (repository, mutation);
}

static gboolean
gom_entity_stage_repository_change (GomEntity      *self,
                                    GomRepository  *repository,
                                    GomDelta       *delta,
                                    GError        **error)
{
  g_autoptr(GomSyncCoordinator) coordinator = NULL;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_REPOSITORY (repository));

  if (delta == NULL || gom_delta_is_empty (delta))
    return TRUE;

  if (!(coordinator = gom_repository_dup_coordinator (repository)))
    return TRUE;

  if (!dex_await (gom_sync_coordinator_stage_local_change (coordinator, repository, NULL, self, delta), error))
    return FALSE;

  return TRUE;
}

static GListModel *
gom_entity_delete_run_query (GomEntityDeleteTask  *task,
                             GomQuery             *query,
                             GError              **error)
{
  g_autoptr(GObject) cursor = NULL;

  g_assert (task != NULL);
  g_assert (GOM_IS_QUERY (query));

  if (task->session != NULL)
    cursor = dex_await_object (gom_session_query (task->session, query), error);
  else
    cursor = dex_await_object (gom_repository_query (task->repository, query), error);

  if (cursor == NULL)
    return NULL;

  return dex_await_object (_gom_cursor_exhaust_to_records (GOM_CURSOR (cursor)), error);
}

static gboolean
gom_entity_delete_relation_exists (GomEntityDeleteTask  *task,
                                   const char           *relation,
                                   GError              **error)
{
  g_autoptr(GObject) schema = NULL;

  g_assert (task != NULL);
  g_assert (relation != NULL);

  if (!(schema = dex_await_object (gom_repository_describe_relation (task->repository, relation), error)))
    {
      if (error != NULL && *error != NULL &&
          g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          g_clear_error (error);
          return FALSE;
        }

      return FALSE;
    }

  return TRUE;
}

static GListModel *
gom_entity_delete_load_entities (GomEntityDeleteTask  *task,
                                 GType                 entity_type,
                                 GomExpression        *filter,
                                 GError              **error)
{
  g_assert (task != NULL);

  if (task->session != NULL)
    return dex_await_object (gom_session_list_entities (task->session,
                                                        entity_type,
                                                        filter,
                                                        NULL),
                             error);

  return dex_await_object (gom_repository_list_entities (task->repository,
                                                         entity_type,
                                                         filter,
                                                         NULL),
                           error);
}

static GListModel *
gom_entity_load_entities (GomEntity      *self,
                          GType           entity_type,
                          GomExpression  *filter,
                          GError        **error)
{
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;

  g_assert (GOM_IS_ENTITY (self));

  if ((session = _gom_entity_dup_session (self)))
    return dex_await_object (gom_session_list_entities (session, entity_type, filter, NULL), error);

  if ((repository = gom_entity_dup_repository (self)))
    return dex_await_object (gom_repository_list_entities (repository, entity_type, filter, NULL), error);

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity is not bound to a repository");

  return NULL;
}

static GListModel *
gom_entity_load_relation_records (GomEntity      *self,
                                  const char     *relation,
                                  GomExpression  *filter,
                                  GError        **error)
{
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomQuery) query = NULL;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (relation != NULL);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, relation);
  gom_query_builder_set_filter (query_builder, filter);

  if (!(query = gom_query_builder_build (query_builder, error)))
    return NULL;

  if ((session = _gom_entity_dup_session (self)))
    {
      cursor = dex_await_object (gom_session_query (session, query), error);
    }
  else
    {
      if ((repository = gom_entity_dup_repository (self)))
        {
          cursor = dex_await_object (gom_repository_query (repository, query), error);
        }
      else
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Entity is not bound to a repository");
          return NULL;
        }
    }

  if (cursor == NULL)
    return NULL;

  return dex_await_object (_gom_cursor_exhaust_to_records (cursor), error);
}

static gboolean
gom_entity_relationship_has_unset_storage_fields (GomEntity           *self,
                                                  GomEntityClass      *entity_class,
                                                  GObjectClass        *object_class,
                                                  const char * const  *fields,
                                                  gboolean            *all_set,
                                                  GError             **error)
{
  gboolean saw_set = FALSE;
  gboolean saw_unset = FALSE;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (object_class != NULL);
  g_assert (fields != NULL);
  g_assert (all_set != NULL);

  *all_set = FALSE;

  for (guint i = 0; fields[i] != NULL; i++)
    {
      g_auto(GValue) value = G_VALUE_INIT;
      const GomPropertySpec *property_spec;
      gboolean value_set;

      if (!(property_spec = gom_entity_get_property_spec (self, fields[i])) &&
          !(property_spec = gom_entity_get_property_spec_by_field (self, fields[i])))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity does not have property '%s'",
                       fields[i]);
          return FALSE;
        }

      if (!gom_entity_get_property_storage_value (self, entity_class, object_class, fields[i], &value, error))
        return FALSE;

      value_set = !gom_entity_value_is_default (property_spec, &value);
      saw_set |= value_set;
      saw_unset |= !value_set;

      g_value_unset (&value);
    }

  if (saw_set && saw_unset)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Relationship has partially unset storage fields");
      return FALSE;
    }

  *all_set = saw_set && !saw_unset;

  return TRUE;
}

static gboolean
gom_entity_validate_relationship_state (GomEntity                  *self,
                                        GomEntityRelationshipInfo  *relationship,
                                        guint                       version,
                                        GError                    **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (relationship != NULL);

  if (!gom_entity_relationship_visible_at_version (relationship, version))
    return TRUE;

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self));
  object_class = G_OBJECT_GET_CLASS (self);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  if (relationship->storage == GOM_RELATIONSHIP_STORAGE_FK)
    {
      const char * const *source_fields;
      const char * const *target_fields;
      g_autoptr(GomExpression) filter = NULL;
      g_autoptr(GListModel) related = NULL;
      guint n_related = 0;
      gboolean have_fields = FALSE;

      if (relationship->cardinality == GOM_RELATIONSHIP_CARDINALITY_TO_ONE)
        {
          source_fields = (const char * const *)relationship->local_fields;
          target_fields = identity_fields;
        }
      else
        {
          source_fields = identity_fields;
          target_fields = (const char * const *)relationship->remote_fields;
        }

      if (source_fields == NULL || source_fields[0] == NULL || target_fields == NULL ||
          target_fields[0] == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Relationship `%s` is missing field bindings",
                       relationship->name);
          return FALSE;
        }

      if (!gom_entity_relationship_has_unset_storage_fields (self, entity_class, object_class, source_fields, &have_fields, error))
        return FALSE;

      if (!have_fields)
        {
          if (!relationship->optional || relationship->min_count > 0)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Relationship `%s` requires a value",
                           relationship->name);
              return FALSE;
            }

          return TRUE;
        }

      if (!(filter = gom_entity_build_field_filter (self, source_fields, target_fields, error)))
        return FALSE;

      if (!(related = gom_entity_load_entities (self, relationship->target_type, filter, error)))
        return FALSE;

      n_related = g_list_model_get_n_items (G_LIST_MODEL (related));
      if (n_related == 0 || (relationship->max_count != 0 && n_related > relationship->max_count))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Relationship `%s` has %u related row(s), outside the allowed range",
                       relationship->name,
                       n_related);
          return FALSE;
        }
    }
  else if (relationship->storage == GOM_RELATIONSHIP_STORAGE_JOIN_TABLE)
    {
      const char *join_relation = relationship->join_relation;
      const char * const *join_local_fields = (const char * const *)relationship->join_local_fields;
      g_autoptr(GomExpression) filter = NULL;
      g_autoptr(GObject) records = NULL;
      guint n_records = 0;
      gboolean have_identity = FALSE;

      if (join_relation == NULL || join_relation[0] == '\0')
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Relationship `%s` is missing a join relation",
                       relationship->name);
          return FALSE;
        }

      if (identity_fields == NULL || identity_fields[0] == NULL)
        {
          if (!relationship->optional || relationship->min_count > 0)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Relationship `%s` requires identity fields to validate",
                           relationship->name);
              return FALSE;
            }

          return TRUE;
        }

      if (join_local_fields == NULL || join_local_fields[0] == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Relationship `%s` is missing join-table fields",
                       relationship->name);
          return FALSE;
        }

      if (!gom_entity_relationship_has_unset_storage_fields (self, entity_class, object_class, identity_fields, &have_identity, error))
        return FALSE;

      if (!have_identity)
        {
          if (!relationship->optional || relationship->min_count > 0)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Relationship `%s` requires an identity before it can be validated",
                           relationship->name);
              return FALSE;
            }

          return TRUE;
        }

      if (!(filter = gom_entity_build_field_filter (self, identity_fields, join_local_fields, error)))
        return FALSE;

      if (!(records = (GObject *)gom_entity_load_relation_records (self, join_relation, filter, error)))
        return FALSE;

      n_records = g_list_model_get_n_items (G_LIST_MODEL (records));
      if ((!relationship->optional && n_records == 0) ||
          (relationship->min_count > 0 && n_records < relationship->min_count) ||
          (relationship->max_count != 0 && n_records > relationship->max_count))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Relationship `%s` has %u related row(s), outside the allowed range",
                       relationship->name,
                       n_records);
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
gom_entity_validate_relationships_for_mutation (GomEntity  *self,
                                                GError    **error)
{
  GomEntityClass *entity_class;
  GomEntityClassInfo *class_info;
  g_autoptr(GomRepository) repository = NULL;
  guint version;

  g_assert (GOM_IS_ENTITY (self));

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self));

  if (!(repository = gom_entity_dup_repository (self)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Entity is not bound to a repository");
      return FALSE;
    }

  version = gom_registry_get_version (_gom_repository_get_registry (repository));
  class_info = _gom_entity_class_get_info (entity_class, FALSE);

  for (GomEntityClassInfo *iter = class_info; iter != NULL; iter = iter->parent_info)
    {
      for (GomEntityRelationshipInfo *relationship = iter->relationships;
           relationship != NULL;
           relationship = relationship->next)
        {
          if (relationship->name == NULL)
            continue;

          if (!gom_entity_validate_relationship_state (self, relationship, version, error))
            return FALSE;
        }
    }

  return TRUE;
}

typedef struct
{
  GomEntity     *self;
  GomSession    *session;
  GomRepository *repository;
  GomMutation   *mutation;
  GomDelta      *delta;
  gboolean       change_state_complete;
} GomEntityMutationTask;

static void
gom_entity_mutation_task_free (gpointer data)
{
  GomEntityMutationTask *task = data;

  g_clear_object (&task->self);
  g_clear_object (&task->session);
  g_clear_object (&task->repository);
  g_clear_object (&task->mutation);
  g_clear_object (&task->delta);
  g_free (task);
}

static DexFuture *
gom_entity_mutation_fiber (gpointer user_data)
{
  GomEntityMutationTask *task = user_data;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (task != NULL);
  g_assert (GOM_IS_ENTITY (task->self));
  g_assert (GOM_IS_MUTATION (task->mutation));

  if (!gom_entity_validate_relationships_for_mutation (task->self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(result = dex_await_object (gom_entity_mutate_run (task->self, task->mutation, task->session, task->repository), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (GOM_IS_INSERTION (task->mutation))
    {
      g_autoptr(GomDelta) delta = NULL;
      g_autoptr(GomRecord) record = NULL;

      if (g_list_model_get_n_items (G_LIST_MODEL (result)) == 0)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Insert did not return mutation rows");

      record = g_list_model_get_item (G_LIST_MODEL (result), 0);
      if (record == NULL || !gom_entity_backfill_identity_from_record (task->self, record))
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Insert did not return an identity value");

      if (!(delta = gom_entity_build_snapshot_delta (task->self, task->repository, GOM_DELTA_KIND_INSERT, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      gom_entity_capture_current_state (task->self, TRUE);

      if (task->session != NULL)
        _gom_session_record_entity_changes (task->session, task->self, delta);
      else if (!gom_entity_stage_repository_change (task->self, task->repository, delta, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else if (task->session != NULL && GOM_IS_UPDATE (task->mutation))
    {
      _gom_session_accept_entity_changes (task->session, task->self, task->delta);
    }
  else if (GOM_IS_UPDATE (task->mutation))
    {
      _gom_entity_apply_delta (task->self, task->delta, task->change_state_complete);
      if (!gom_entity_stage_repository_change (task->self, task->repository, task->delta, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_take_object (g_steal_pointer (&result));
}

static gboolean
gom_entity_delete_visit (GomEntityDeleteTask  *task,
                         GomEntity            *entity,
                         GError              **error)
{
  g_autoptr(GomRegistry) snapshot = NULL;
  g_autoptr(GomDeletionBuilder) builder = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autofree char *entity_key = NULL;
  GomEntityClass *entity_class;
  const char * const *identity_fields;
  GType entity_type;
  const GomEntitySpec * const *entities;
  guint n_entities = 0;

  g_assert (task != NULL);
  g_assert (GOM_IS_ENTITY (entity));

  if (!(entity_key = _gom_entity_dup_session_key (entity)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Entity identity is unset");
      return FALSE;
    }

  if (g_hash_table_contains (task->visited, entity_key))
    return TRUE;

  g_hash_table_add (task->visited, g_steal_pointer (&entity_key));

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (entity));
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  entity_type = G_OBJECT_TYPE (entity);

  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Entity type has no identity fields");
      return FALSE;
    }

  snapshot = gom_registry_snapshot (task->registry, gom_registry_get_version (task->registry));
  entities = gom_registry_list_entities (snapshot, &n_entities);

  for (guint i = 0; i < n_entities; i++)
    {
      GomEntitySpec *entity_spec = (GomEntitySpec *)entities[i];
      g_autoptr(GListModel) relationships = NULL;
      guint n_relationships;

      relationships = gom_entity_spec_list_relationships (entity_spec);
      n_relationships = g_list_model_get_n_items (relationships);

      for (guint j = 0; j < n_relationships; j++)
        {
          g_autoptr(GomRelationshipSpec) relationship = NULL;
          g_autoptr(GomExpression) related_filter = NULL;
          g_autoptr(GListModel) related_entities = NULL;
          guint n_related = 0;
          GType owner_type;

          if (!(relationship = g_list_model_get_item (relationships, j)))
            continue;

          if (gom_relationship_spec_get_target_type (relationship) != entity_type)
            continue;

          owner_type = gom_entity_spec_get_entity_type (entity_spec);

          if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_FK)
            {
              const char * const *local_fields = gom_relationship_spec_get_local_fields (relationship);

              if (local_fields == NULL || local_fields[0] == NULL)
                continue;

              if (!(related_filter = gom_entity_build_field_filter (entity, identity_fields, local_fields, error)))
                return FALSE;

              if (!(related_entities = gom_entity_delete_load_entities (task, owner_type, related_filter, error)))
                return FALSE;

              n_related = g_list_model_get_n_items (G_LIST_MODEL (related_entities));

              if (gom_relationship_spec_get_delete_rule (relationship) == GOM_RELATIONSHIP_DELETE_DENY)
                {
                  if (n_related > 0)
                    {
                      g_set_error (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "Deleting `%s` is denied by relationship `%s`",
                                   G_OBJECT_TYPE_NAME (entity),
                                   gom_relationship_spec_get_name (relationship));
                      return FALSE;
                    }

                  continue;
                }

              if (gom_relationship_spec_get_delete_rule (relationship) == GOM_RELATIONSHIP_DELETE_NULLIFY)
                {
                  g_autoptr(GomUpdateBuilder) update_builder = NULL;
                  g_autoptr(GomUpdate) update = NULL;
                  g_autoptr(GomMutationResult) update_result = NULL;

                  if (n_related == 0)
                    continue;

                  update_builder = gom_update_builder_new ();
                  gom_update_builder_set_target_entity_type (update_builder, owner_type);
                  gom_update_builder_set_filter (update_builder, related_filter);

                  for (guint k = 0; local_fields[k] != NULL; k++)
                    gom_update_builder_add_assignment (update_builder,
                                                       gom_field_expression_new (local_fields[k]),
                                                       gom_literal_expression_new (NULL));

                  if (!(update = gom_update_builder_build (update_builder, error)))
                    return FALSE;

                  if (!(update_result = dex_await_object (gom_entity_delete_run_mutation (task, GOM_MUTATION (update)), error)))
                    return FALSE;

                  continue;
                }

              if (gom_relationship_spec_get_delete_rule (relationship) == GOM_RELATIONSHIP_DELETE_NO_ACTION)
                continue;

              for (guint k = 0; k < n_related; k++)
                {
                  g_autoptr(GomEntity) related_entity = g_list_model_get_item (G_LIST_MODEL (related_entities), k);
                  g_autoptr(GomMutationResult) related_result = NULL;

                  if (related_entity == NULL)
                    {
                      g_set_error_literal (error,
                                           G_IO_ERROR,
                                           G_IO_ERROR_FAILED,
                                           "Failed to load related entity for cascade delete");
                      return FALSE;
                    }

                  if (!(related_result = dex_await_object (gom_entity_delete (related_entity), error)))
                    return FALSE;
                }
            }
          else if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_JOIN_TABLE)
            {
              g_autoptr(GomQueryBuilder) query_builder = NULL;
              g_autoptr(GomQuery) query = NULL;
              g_autoptr(GomExpression) join_filter = NULL;
              g_autoptr(GObject) join_rows = NULL;
              guint n_join_rows = 0;
              const char *join_relation;
              const char * const *join_remote_fields;

              join_relation = gom_relationship_spec_get_join_relation (relationship);
              join_remote_fields = gom_relationship_spec_get_join_remote_fields (relationship);

              if (join_relation == NULL || join_relation[0] == '\0')
                {
                  g_set_error (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Relationship `%s` is missing a join relation",
                               gom_relationship_spec_get_name (relationship));
                  return FALSE;
                }

              if (!gom_entity_delete_relation_exists (task, join_relation, error))
                {
                  if (error != NULL && *error != NULL)
                    return FALSE;

                  continue;
                }

              if (join_remote_fields == NULL || join_remote_fields[0] == NULL)
                {
                  g_set_error (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Relationship `%s` is missing join-table fields",
                               gom_relationship_spec_get_name (relationship));
                  return FALSE;
                }

              join_filter = gom_entity_build_field_filter (entity,
                                                           identity_fields,
                                                           join_remote_fields,
                                                           error);
              if (join_filter == NULL)
                return FALSE;

              query_builder = gom_query_builder_new ();
              gom_query_builder_set_target_relation (query_builder, join_relation);
              gom_query_builder_set_filter (query_builder, join_filter);
              if (!(query = gom_query_builder_build (query_builder, error)))
                return FALSE;

              if (gom_relationship_spec_get_delete_rule (relationship) == GOM_RELATIONSHIP_DELETE_DENY)
                {
                  if (!(join_rows = (GObject *) gom_entity_delete_run_query (task, query, error)))
                    return FALSE;

                  n_join_rows = g_list_model_get_n_items (G_LIST_MODEL (join_rows));
                  if (n_join_rows > 0)
                    {
                      g_set_error (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "Deleting `%s` is denied by relationship `%s`",
                                   G_OBJECT_TYPE_NAME (entity),
                                   gom_relationship_spec_get_name (relationship));
                      return FALSE;
                    }

                  continue;
                }

              if (gom_relationship_spec_get_delete_rule (relationship) == GOM_RELATIONSHIP_DELETE_NO_ACTION)
                continue;

              {
                g_autoptr(GomDeletionBuilder) delete_builder = NULL;
                g_autoptr(GomDeletion) join_deletion = NULL;

                delete_builder = gom_deletion_builder_new ();
                gom_deletion_builder_set_target_relation (delete_builder, join_relation);
                gom_deletion_builder_set_filter (delete_builder, join_filter);

                if (!(join_deletion = gom_deletion_builder_build (delete_builder, error)))
                  return FALSE;

                if (!dex_await (gom_entity_delete_run_mutation (task, GOM_MUTATION (join_deletion)), error))
                  return FALSE;
              }
            }
        }
    }

  return TRUE;
}

static GomExpression *
gom_entity_real_dup_identity_value (GomEntity   *self,
                                    const char  *identity_field,
                                    GError     **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const GomPropertySpec *property_spec;
  GParamSpec *pspec;
  GType value_type;
  g_auto(GValue) fallback_default_value = G_VALUE_INIT;
  const GValue *default_value = NULL;
  g_auto(GValue) value = G_VALUE_INIT;
  g_auto(GValue) storage_value = G_VALUE_INIT;

  g_return_val_if_fail (GOM_IS_ENTITY (self), NULL);
  g_return_val_if_fail (identity_field != NULL, NULL);

  object_class = G_OBJECT_GET_CLASS (self);
  entity_class = GOM_ENTITY_CLASS (object_class);

  if (!(property_spec = gom_entity_get_property_spec (self, identity_field)) &&
      !(pspec = g_object_class_find_property (object_class, identity_field)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity identity field '%s' is not a property",
                   identity_field);
      return NULL;
    }

  if (property_spec != NULL)
    {
      value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);
      default_value = gom_property_spec_get_default_value ((GomPropertySpec *)property_spec);
    }
  else
    {
      value_type = G_PARAM_SPEC_VALUE_TYPE (pspec);
    }

  g_value_init (&value, value_type);
  g_object_get_property (G_OBJECT (self), identity_field, &value);

  if (property_spec == NULL)
    {
      g_value_init (&fallback_default_value, value_type);
      g_param_value_set_default (pspec, &fallback_default_value);
      default_value = &fallback_default_value;
    }

  if (gom_entity_value_matches_default (default_value, &value))
    return NULL;

  if (property_spec != NULL)
    {
      if (!gom_entity_get_property_storage_value (self, entity_class, object_class, identity_field, &storage_value, error))
        return NULL;
    }
  else
    {
      g_value_init (&storage_value, value_type);
      g_value_copy (&value, &storage_value);
    }

  return gom_literal_expression_new (&storage_value);
}

static GomExpression *
gom_entity_build_identity_filter (GomEntity           *self,
                                  GomEntityClass      *entity_class,
                                  const char * const  *identity_fields,
                                  GError             **error)
{
  g_autoptr(GomExpression) filter = NULL;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (identity_fields != NULL);

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      const char *identity_field = identity_fields[i];
      g_autoptr(GomExpression) identity_value = NULL;
      g_autoptr(GomExpression) predicate = NULL;

      if (!(identity_value = entity_class->dup_identity_value (self, identity_field, error)))
        {
          if (error != NULL && *error != NULL)
            return NULL;

          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity identity field '%s' is unset",
                       identity_field);
          return NULL;
        }

      if (!gom_expression_is_constant (identity_value))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity identity field '%s' did not produce a constant identity expression",
                       identity_field);
          return NULL;
        }

      predicate = gom_binary_expression_new_equal (gom_field_expression_new (identity_field),
                                                   g_steal_pointer (&identity_value));

      if (filter == NULL)
        filter = g_steal_pointer (&predicate);
      else
        filter = gom_binary_expression_new_and (g_steal_pointer (&filter),
                                                g_steal_pointer (&predicate));
    }

  return g_steal_pointer (&filter);
}

static GomExpression *
gom_entity_build_field_filter (GomEntity           *self,
                               const char * const  *source_fields,
                               const char * const  *target_fields,
                               GError             **error)
{
  g_autoptr(GomExpression) filter = NULL;
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  guint n_source_fields = 0;
  guint n_target_fields = 0;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (source_fields != NULL);
  g_assert (target_fields != NULL);

  object_class = G_OBJECT_GET_CLASS (self);
  entity_class = GOM_ENTITY_CLASS (object_class);

  while (source_fields[n_source_fields] != NULL)
    n_source_fields++;

  while (target_fields[n_target_fields] != NULL)
    n_target_fields++;

  if (n_source_fields != n_target_fields)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Relationship field cardinality does not match");
      return NULL;
    }

  for (guint i = 0; source_fields[i] != NULL && target_fields[i] != NULL; i++)
    {
      g_auto(GValue) value = G_VALUE_INIT;
      g_autoptr(GomExpression) predicate = NULL;
      g_autoptr(GomExpression) left = NULL;
      g_autoptr(GomExpression) right = NULL;

      if (!gom_entity_get_property_storage_value (self, entity_class, object_class, source_fields[i], &value, error))
        return NULL;

      left = gom_field_expression_new (target_fields[i]);
      right = gom_literal_expression_new (&value);
      predicate = gom_binary_expression_new_equal (g_steal_pointer (&left),
                                                   g_steal_pointer (&right));

      if (filter == NULL)
        filter = g_steal_pointer (&predicate);
      else
        filter = gom_binary_expression_new_and (g_steal_pointer (&filter),
                                                g_steal_pointer (&predicate));
    }

  return g_steal_pointer (&filter);
}

static gboolean
gom_entity_property_visible_at_version (GomEntityClass *entity_class,
                                        const char     *property_name,
                                        guint           version)
{
  guint version_added;
  guint version_removed;

  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (property_name != NULL);

  version_added = gom_entity_class_property_get_version_added (entity_class, property_name);
  version_removed = gom_entity_class_property_get_version_removed (entity_class, property_name);

  if (version_added > version)
    return FALSE;

  if (version_removed != 0 && version >= version_removed)
    return FALSE;

  return TRUE;
}

static gboolean
gom_property_spec_visible_at_version (GomPropertySpec *property,
                                      guint            version)
{
  guint version_added;
  guint version_removed;

  g_assert (GOM_IS_PROPERTY_SPEC (property));

  version_added = gom_property_spec_get_version_added (property);
  version_removed = gom_property_spec_get_version_removed (property);

  if (version_added > version)
    return FALSE;

  if (version_removed != 0 && version >= version_removed)
    return FALSE;

  return TRUE;
}

static gboolean
gom_entity_relationship_visible_at_version (GomEntityRelationshipInfo *relationship,
                                            guint                      version)
{
  g_assert (relationship != NULL);

  if (relationship->version_added > version)
    return FALSE;

  if (relationship->version_removed != 0 && version >= relationship->version_removed)
    return FALSE;

  return TRUE;
}

gboolean
gom_entity_dup_identity_value_is_set (GomEntity       *self,
                                      GomEntityClass  *entity_class,
                                      const char      *identity_field,
                                      GomExpression  **out_identity_value,
                                      GError         **error)
{
  g_autoptr(GomExpression) identity_value = NULL;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (identity_field != NULL);
  g_assert (out_identity_value != NULL);

  if (!(identity_value = entity_class->dup_identity_value (self, identity_field, error)))
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      return FALSE;
    }

  if (!gom_expression_is_constant (identity_value))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity identity field '%s' did not produce a constant identity expression",
                   identity_field);
      return FALSE;
    }

  *out_identity_value = g_steal_pointer (&identity_value);

  return TRUE;
}

static DexFuture *
gom_entity_validate_mutation_setup (GomEntity           *self,
                                    GomEntityClass     **entity_class_out,
                                    GObjectClass       **object_class_out,
                                    GomRepository      **repository_out,
                                    const char * const **identity_fields_out)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  GomRepository *repository;
  const char * const *identity_fields;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (entity_class_out != NULL);
  g_assert (object_class_out != NULL);
  g_assert (repository_out != NULL);
  g_assert (identity_fields_out != NULL);

  object_class = G_OBJECT_GET_CLASS (self);
  entity_class = GOM_ENTITY_CLASS (object_class);
  repository = gom_entity_dup_repository (self);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  if (repository == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Entity is not bound to a repository");

  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_object_unref (repository);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_INVALID_ARGUMENT,
                                    "Entity type has no identity fields");
    }

  *entity_class_out = entity_class;
  *object_class_out = object_class;
  *repository_out = repository;
  *identity_fields_out = identity_fields;

  return NULL;
}

static gboolean
gom_entity_backfill_identity_from_record (GomEntity *self,
                                          GomRecord *record)
{
  g_autoptr(GomSession) session = NULL;
  GObjectClass *object_class;
  GomEntityClass *entity_class;
  const char * const *identity_fields;
  guint n_identity_fields;
  guint n_identity_fields_total;
  gboolean have_writable_identity = FALSE;
  gboolean updated = FALSE;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_RECORD (record));

  object_class = G_OBJECT_GET_CLASS (self);
  entity_class = GOM_ENTITY_CLASS (object_class);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  session = _gom_entity_dup_session (self);

  if (identity_fields == NULL || identity_fields[0] == NULL)
    return FALSE;

  for (n_identity_fields_total = 0;
       identity_fields[n_identity_fields_total] != NULL;
       n_identity_fields_total++) { /* Do Nothing */ }

  for (n_identity_fields = 0; identity_fields[n_identity_fields] != NULL; n_identity_fields++)
    {
      const char *identity_field = identity_fields[n_identity_fields];
      g_auto(GValue) value = G_VALUE_INIT;
      gboolean have_value = FALSE;

      if (gom_entity_get_property_spec (self, identity_field) == NULL)
        continue;

      have_writable_identity = TRUE;
      have_value = gom_record_get_column_by_name (record, identity_field, &value);

      if (!have_value && n_identity_fields_total == 1)
        have_value = gom_record_get_column_by_name (record, "rowid", &value);

      if (!have_value)
        continue;

      g_object_set_property (G_OBJECT (self), identity_field, &value);

      updated = TRUE;
    }

  if (!have_writable_identity)
    return TRUE;

  if (!updated)
    return FALSE;

  if (session != NULL && !gom_entity_rekey_session_identity (self))
    return FALSE;

  if (session != NULL)
    _gom_session_unregister_pending_entity (session, self);

  return TRUE;
}

/**
 * gom_entity_insert:
 * @self: a [class@Gom.Entity]
 *
 * Inserts @self using mapped properties.
 *
 * Identity fields are omitted from insertion when
 * `GomEntityClass.dup_identity_value` returns `NULL`.
 *
 * The entity must be bound to a repository with
 * [method@Gom.Entity.set_repository].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.MutationResult] of [class@Gom.Record] rows or rejects with
 *   error.
 */
DexFuture *
gom_entity_insert (GomEntity *self)
{
  g_autoptr(GomInsertionBuilder) builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GPtrArray) row_values = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GError) error = NULL;
  const GomEntitySpec *entity_spec;
  const GomPropertySpec * const *properties = NULL;
  GObjectClass *object_class;
  GomEntityClass *entity_class;
  guint version = 0;
  const char * const *identity_fields;
  guint n_properties = 0;
  guint n_columns = 0;

  dex_return_error_if_fail (GOM_IS_ENTITY (self));

  if (!(repository = gom_entity_dup_repository (self)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Entity is not bound to a repository");

  session = _gom_entity_dup_session (self);
  object_class = G_OBJECT_GET_CLASS (self);
  entity_class = GOM_ENTITY_CLASS (object_class);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  version = gom_registry_get_version (_gom_repository_get_registry (repository));

  if (!(entity_spec = gom_entity_get_entity_spec (self)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Entity type `%s` is not in the registry",
                                  G_OBJECT_TYPE_NAME (self));

  properties = gom_entity_spec_list_properties ((GomEntitySpec *)entity_spec, &n_properties);
  builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_entity_type (builder, G_OBJECT_TYPE (self));
  row_values = g_ptr_array_new ();

  for (guint i = 0; i < n_properties; i++)
    {
      const char *property_name = gom_property_spec_get_name ((GomPropertySpec *)properties[i]);
      GomEntityPropertyInfo *prop_info;
      const char *field_name;
      g_auto(GValue) value = G_VALUE_INIT;
      gboolean is_identity;

      if (!gom_property_spec_get_mapped ((GomPropertySpec *)properties[i]))
        continue;

      if (!gom_property_spec_visible_at_version ((GomPropertySpec *)properties[i], version))
        continue;

      prop_info = _gom_entity_class_get_property (entity_class, property_name, FALSE);
      field_name = prop_info != NULL && prop_info->field_name != NULL ? prop_info->field_name
                                                                      : property_name;
      is_identity = _gom_strv_contains (identity_fields, property_name);

      if (is_identity)
        {
          g_autoptr(GomExpression) identity_value = NULL;

          if (!gom_entity_dup_identity_value_is_set (self, entity_class, property_name, &identity_value, &error))
            {
              if (error != NULL)
                return dex_future_new_for_error (g_steal_pointer (&error));

              continue;
            }

          gom_insertion_builder_add_column (builder, gom_field_expression_new (field_name));
          g_ptr_array_add (row_values, g_steal_pointer (&identity_value));
          n_columns++;
          continue;
        }

      if (!gom_entity_get_property_storage_value (self, entity_class, object_class, property_name, &value, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      gom_insertion_builder_add_column (builder, gom_field_expression_new (field_name));
      g_ptr_array_add (row_values, gom_literal_expression_new (&value));
      n_columns++;
    }

  if (n_columns == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Entity has no mapped properties to insert");

  gom_insertion_builder_add_row (builder, (GomExpression **)row_values->pdata, row_values->len);

  if (!(insertion = gom_insertion_builder_build (builder, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  {
    GomEntityMutationTask *task = g_new0 (GomEntityMutationTask, 1);

    task->self = g_object_ref (self);
    task->session = session != NULL ? g_object_ref (session) : NULL;
    task->repository = g_object_ref (repository);
    task->mutation = GOM_MUTATION (g_object_ref (insertion));

    return dex_scheduler_spawn (NULL,
                                0,
                                gom_entity_mutation_fiber,
                                task,
                                gom_entity_mutation_task_free);
  }
}

/**
 * gom_entity_update:
 * @self: a [class@Gom.Entity]
 *
 * Updates @self using mapped properties.
 *
 * A `WHERE` clause is generated from identity fields and the update is
 * limited to one row.
 *
 * The entity must be bound to a repository and must have identity fields
 * whose values resolve through `GomEntityClass.dup_identity_value`.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.MutationResult] of [class@Gom.Record] rows or rejects with
 *   error.
 */
DexFuture *
gom_entity_update (GomEntity *self)
{
  g_autoptr(GomUpdateBuilder) builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomDelta) delta = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GError) error = NULL;
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;
  DexFuture *failure;

  dex_return_error_if_fail (GOM_IS_ENTITY (self));

  if ((failure = gom_entity_validate_mutation_setup (self, &entity_class, &object_class, &repository, &identity_fields)))
    return failure;

  session = _gom_entity_dup_session (self);

  if (!(delta = gom_entity_build_delta (self, &error)))
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (session != NULL)
        _gom_session_accept_entity_changes (session, self, NULL);
      else
        _gom_entity_apply_delta (self, NULL, FALSE);

      return dex_future_new_take_object (_gom_mutation_result_new ());
    }

  if (gom_delta_is_empty (delta))
    {
      if (session != NULL)
        _gom_session_accept_entity_changes (session, self, NULL);
      else
        _gom_entity_apply_delta (self, NULL, FALSE);

      return dex_future_new_take_object (_gom_mutation_result_new ());
    }

  builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (builder, G_OBJECT_TYPE (self));

  if (!(filter = gom_entity_build_identity_filter (self, entity_class, identity_fields, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  gom_update_builder_set_filter (builder, g_steal_pointer (&filter));
  gom_update_builder_set_limit (builder, 1);

  {
    guint version = gom_registry_get_version (_gom_repository_get_registry (repository));

    for (guint i = 0; i < gom_delta_get_n_changes (delta); i++)
      {
        const char *property_name = gom_delta_get_property_name (delta, i);
        GomEntityPropertyInfo *prop_info;
        const char *field_name;
        g_auto(GValue) value = G_VALUE_INIT;

        if (property_name == NULL)
          continue;

        if (!gom_entity_property_visible_at_version (entity_class, property_name, version))
          continue;

        if (gom_entity_get_property_spec (self, property_name) == NULL)
          continue;

        prop_info = _gom_entity_class_get_property (entity_class, property_name, FALSE);
        field_name = prop_info != NULL && prop_info->field_name != NULL ? prop_info->field_name
                                                                        : property_name;

        if (!gom_entity_get_property_storage_value (self, entity_class, object_class, property_name, &value, &error))
          return dex_future_new_for_error (g_steal_pointer (&error));

        gom_update_builder_add_assignment (builder,
                                           gom_field_expression_new (field_name),
                                           gom_literal_expression_new (&value));
      }
  }

  if (!(update = gom_update_builder_build (builder, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  {
    GomEntityMutationTask *task = g_new0 (GomEntityMutationTask, 1);

    task->self = g_object_ref (self);
    task->session = session != NULL ? g_object_ref (session) : NULL;
    task->repository = g_object_ref (repository);
    task->mutation = GOM_MUTATION (g_object_ref (update));
    task->delta = g_object_ref (delta);
    task->change_state_complete = _gom_entity_change_state_is_complete (self);

    return dex_scheduler_spawn (NULL,
                                0,
                                gom_entity_mutation_fiber,
                                task,
                                gom_entity_mutation_task_free);
  }
}

static void
gom_entity_delete_task_free (gpointer data)
{
  GomEntityDeleteTask *task = data;

  g_clear_object (&task->self);
  g_clear_object (&task->session);
  g_clear_object (&task->repository);
  g_clear_object (&task->registry);
  g_clear_pointer (&task->visited, g_hash_table_unref);
  g_free (task);
}

static DexFuture *
gom_entity_delete_fiber (gpointer user_data)
{
  GomEntityDeleteTask *task = user_data;
  g_autoptr(GomDelta) delta = NULL;
  g_autoptr(GomDeletionBuilder) builder = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;
  GomEntityClass *entity_class;
  const char * const *identity_fields;

  g_assert (task != NULL);
  g_assert (GOM_IS_ENTITY (task->self));

  if (!gom_entity_delete_visit (task, task->self, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(delta = gom_entity_build_snapshot_delta (task->self, task->repository, GOM_DELTA_KIND_DELETE, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (task->self));
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  builder = gom_deletion_builder_new ();
  gom_deletion_builder_set_target_entity_type (builder, G_OBJECT_TYPE (task->self));

  if (!(filter = gom_entity_build_identity_filter (task->self, entity_class, identity_fields, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  gom_deletion_builder_set_filter (builder, filter);
  gom_deletion_builder_set_limit (builder, 1);

  if (!(deletion = gom_deletion_builder_build (builder, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(result = dex_await_object (gom_entity_delete_run_mutation (task, GOM_MUTATION (deletion)), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  _gom_entity_set_lifecycle (task->self, GOM_ENTITY_LIFECYCLE_DELETED);

  if (task->session != NULL)
    _gom_session_record_entity_changes (task->session, task->self, delta);
  else if (!gom_entity_stage_repository_change (task->self, task->repository, delta, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&result));
}

/**
 * gom_entity_delete:
 * @self: a [class@Gom.Entity]
 *
 * Deletes @self using identity fields.
 *
 * A `WHERE` clause is generated from identity fields and the deletion is
 * limited to one row.
 *
 * The entity must be bound to a repository and must have identity fields
 * whose values resolve through `GomEntityClass.dup_identity_value`.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.MutationResult] of [class@Gom.Record] rows or rejects with
 *   error.
 */
DexFuture *
gom_entity_delete (GomEntity *self)
{
  GomEntityDeleteTask *task;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;
  DexFuture *failure;

  dex_return_error_if_fail (GOM_IS_ENTITY (self));

  if ((failure = gom_entity_validate_mutation_setup (self, &entity_class, &object_class, &repository, &identity_fields)))
    return g_steal_pointer (&failure);

  session = _gom_entity_dup_session (self);

  task = g_new0 (GomEntityDeleteTask, 1);
  task->self = g_object_ref (self);
  task->session = session != NULL ? g_object_ref (session) : NULL;
  task->repository = g_object_ref (repository);
  task->registry = g_object_ref (_gom_repository_get_registry (repository));
  task->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_entity_delete_fiber,
                              task,
                              gom_entity_delete_task_free);
}

typedef struct
{
  GomEntity *self;
  char      *relationship_name;
} GomRelationshipLoadTask;

static void
gom_relationship_load_task_free (gpointer data)
{
  GomRelationshipLoadTask *task = data;

  g_clear_object (&task->self);
  g_clear_pointer (&task->relationship_name, g_free);
  g_free (task);
}

static DexFuture *
gom_entity_load_related_model_fiber (gpointer user_data)
{
  GomRelationshipLoadTask *task = user_data;
  g_autoptr(GomRelatedModel) model = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (task != NULL);
  g_assert (GOM_IS_ENTITY (task->self));

  if (!(model = gom_related_model_new (task->self, task->relationship_name)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Failed to create related model");

  if (!dex_await (gom_related_model_reload (model), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&model));
}

static DexFuture *
gom_entity_load_related_entity_fiber (gpointer user_data)
{
  GomRelationshipLoadTask *task = user_data;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) list = NULL;
  GomEntityClass *entity_class;
  GomEntityRelationshipInfo *relationship;

  g_assert (task != NULL);
  g_assert (GOM_IS_ENTITY (task->self));

  entity_class = GOM_ENTITY_GET_CLASS (task->self);
  relationship = _gom_entity_class_get_relationship (entity_class, task->relationship_name, FALSE);

  if (relationship == NULL)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type `%s` does not define relationship `%s`",
                   G_OBJECT_TYPE_NAME (task->self),
                   task->relationship_name);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (relationship->cardinality != GOM_RELATIONSHIP_CARDINALITY_TO_ONE ||
      relationship->storage != GOM_RELATIONSHIP_STORAGE_FK)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Relationship `%s` is not a to-one foreign-key relation",
                   task->relationship_name);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (relationship->local_fields == NULL || relationship->local_fields[0] == NULL)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Relationship `%s` is missing local fields",
                   task->relationship_name);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  session = _gom_entity_dup_session (task->self);
  repository = gom_entity_dup_repository (task->self);

  {
    const GomEntitySpec *target_entity = NULL;
    const char * const *identity_fields;

    if (repository == NULL && session != NULL)
      repository = _gom_session_dup_repository (session);

    if (repository != NULL)
      target_entity = _gom_registry_lookup_entity_by_type (_gom_repository_get_registry (repository),
                                                           relationship->target_type);

    identity_fields = target_entity != NULL
      ? gom_entity_spec_get_identity_fields ((GomEntitySpec *)target_entity)
      : NULL;

    if (identity_fields == NULL || identity_fields[0] == NULL)
      {
        g_set_error (&error,
                     G_IO_ERROR,
                     G_IO_ERROR_INVALID_ARGUMENT,
                     "Target entity type `%s` has no identity fields",
                     g_type_name (relationship->target_type));
        return dex_future_new_for_error (g_steal_pointer (&error));
      }

    filter = gom_entity_build_field_filter (task->self,
                                            (const char * const *)relationship->local_fields,
                                            identity_fields,
                                            &error);
  }

  if (filter == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (session != NULL)
    list = dex_await_object (gom_session_list_entities (session, relationship->target_type, filter, NULL), &error);
  else if (repository != NULL)
    list = dex_await_object (gom_repository_list_entities (repository, relationship->target_type, filter, NULL), &error);
  else
    g_set_error_literal (&error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         "Entity is not bound to a repository");

  if (list == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (g_list_model_get_n_items (G_LIST_MODEL (list)) == 0)
    return dex_future_new_for_object (NULL);

  return dex_future_new_take_object (g_list_model_get_item (G_LIST_MODEL (list), 0));
}

/**
 * gom_entity_load_related_entity:
 * @self: a [class@Gom.Entity]
 * @relationship_name: the relationship to load
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to the
 *   related entity or %NULL.
 */
DexFuture *
gom_entity_load_related_entity (GomEntity  *self,
                                const char *relationship_name)
{
  GomRelationshipLoadTask *task;

  dex_return_error_if_fail (GOM_IS_ENTITY (self));
  dex_return_error_if_fail (relationship_name != NULL);

  task = g_new0 (GomRelationshipLoadTask, 1);
  task->self = g_object_ref (self);
  task->relationship_name = g_strdup (relationship_name);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_entity_load_related_entity_fiber,
                              task,
                              gom_relationship_load_task_free);
}

/**
 * gom_entity_load_related_model:
 * @self: a [class@Gom.Entity]
 * @relationship_name: the relationship to load
 *
 * Loads a related collection asynchronously.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.RelatedModel]
 */
DexFuture *
gom_entity_load_related_model (GomEntity  *self,
                               const char *relationship_name)
{
  GomRelationshipLoadTask *task;

  dex_return_error_if_fail (GOM_IS_ENTITY (self));
  dex_return_error_if_fail (relationship_name != NULL);

  task = g_new0 (GomRelationshipLoadTask, 1);
  task->self = g_object_ref (self);
  task->relationship_name = g_strdup (relationship_name);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_entity_load_related_model_fiber,
                              task,
                              gom_relationship_load_task_free);
}
