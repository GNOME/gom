/* gom-repository.c
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

#include <string.h>

#include "gom-entity.h"
#include "gom-migrator.h"
#include "gom-mutation.h"
#include "gom-ordering.h"
#include "gom-cursor-private.h"
#include "gom-deletion-private.h"
#include "gom-driver-private.h"
#include "gom-insertion-private.h"
#include "gom-query-private.h"
#include "gom-entity-list-model-private.h"
#include "gom-meta-private.h"
#include "gom-meta-version-private.h"
#include "gom-record-list-model-private.h"
#include "gom-repository-private.h"
#include "gom-sync-coordinator.h"
#include "gom-sync-history-private.h"
#include "gom-tombstone-private.h"
#include "gom-trace-private.h"
#include "gom-update-private.h"
#include "gom-util-private.h"

struct _GomRepository
{
  GObject             parent_instance;
  GMutex              mutex;
  GomDriver          *driver;
  GArray             *entity_types;
  GHashTable         *built_types;
  GomRegistry        *registry;
  GomMigrator        *migrator;
  GomSyncCoordinator *coordinator;
  guint               dirty : 1;
  guint               sync_history_available : 1;
};

enum
{
  PROP_0,
  PROP_DRIVER,
  PROP_MIGRATOR,
  PROP_COORDINATOR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomRepository, gom_repository, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static gboolean
gom_repository_relations_have_sync_history (char **relations)
{
  return _gom_strv_contains (relations, "gom_sync_history") &&
         _gom_strv_contains (relations, "gom_sync_history_values");
}

static void
gom_repository_precompute_locked (GomRepository *self)
{
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();
  g_autoptr(GomRegistryBuilder) builder = NULL;
  g_autoptr(GomRegistry) registry = NULL;

  if (!self->dirty)
    return;

  builder = gom_registry_builder_new ();
  for (guint i = 0; i < self->entity_types->len; i++)
    {
      GType entity_type = g_array_index (self->entity_types, GType, i);

      gom_registry_builder_add_entity_type (builder, entity_type);
    }

  if (self->coordinator != NULL || self->sync_history_available)
    {
      gom_registry_builder_add_entity_type (builder, GOM_TYPE_TOMBSTONE);
      gom_registry_builder_add_entity_type (builder, GOM_TYPE_SYNC_HISTORY_CHANGE);
      gom_registry_builder_add_entity_type (builder, GOM_TYPE_SYNC_HISTORY_VALUE);
    }

  registry = gom_registry_builder_build (builder);
  g_set_object (&self->registry, registry);

  self->dirty = FALSE;
  GOM_TRACE_END_MARK (start_time,
                      "Repository",
                      "registry-load",
                      "rebuilt registry for %u entity types",
                      self->entity_types->len);
}

void
_gom_repository_precompute (GomRepository *self)
{
  g_mutex_lock (&self->mutex);
  gom_repository_precompute_locked (self);
  g_mutex_unlock (&self->mutex);
}

static DexFuture *
gom_repository_bind_cursor_cb (DexFuture *completed,
                               gpointer   user_data)
{
  GomRepository *self = user_data;
  const GValue *value;
  GomCursor *cursor;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_REPOSITORY (self));

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_CURSOR));

  cursor = g_value_get_object (value);
  _gom_cursor_set_repository (cursor, self);

  return dex_future_new_take_object (g_object_ref (cursor));
}

static DexFuture *
gom_repository_exhaust_cursor_to_list_cb (DexFuture *completed,
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

static DexFuture *
gom_repository_count_cb (DexFuture *completed,
                         gpointer   user_data)
{
  const GValue *value;
  GomCursor *cursor;
  gint64 count;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (user_data == NULL);

  if (!(value = dex_future_get_value (completed, NULL)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Failed to execute count query");

  if (!(cursor = g_value_get_object (value)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Count query returned no cursor");

  if ((gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_COUNT) == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Backend did not provide a row count");

  if (gom_cursor_get_count (cursor) > (guint64)G_MAXINT64)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Row count exceeds signed 64-bit range");

  count = (gint64)gom_cursor_get_count (cursor);
  return dex_future_new_for_int64 (count);
}

typedef struct
{
  GomRepository *repository;
  GomQuery      *query;
} GomRepositoryFindOneState;

static void
gom_repository_find_one_state_free (gpointer data)
{
  GomRepositoryFindOneState *state = data;

  g_clear_object (&state->repository);
  g_clear_object (&state->query);
  g_free (state);
}

static DexFuture *
gom_repository_find_one_fiber (gpointer user_data)
{
  GomRepositoryFindOneState *state = user_data;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));
  g_assert (GOM_IS_QUERY (state->query));

  if (!(cursor = dex_await_object (gom_repository_query (state->repository, state->query), &error)))
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
  GomRepository *repository;
  GomEntity     *entity;
} GomRepositoryInsertEntityState;

static void
gom_repository_insert_entity_state_free (gpointer data)
{
  GomRepositoryInsertEntityState *state = data;

  g_clear_object (&state->repository);
  g_clear_object (&state->entity);
  g_free (state);
}

static DexFuture *
gom_repository_insert_entity_fiber (gpointer user_data)
{
  GomRepositoryInsertEntityState *state = user_data;
  g_autoptr(GObject) result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));
  g_assert (GOM_IS_ENTITY (state->entity));

  gom_entity_set_repository (state->entity, state->repository);

  if (!(result = dex_await_object (gom_entity_insert (state->entity), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static void
gom_repository_finalize (GObject *object)
{
  GomRepository *self = (GomRepository *)object;

  g_clear_pointer (&self->entity_types, g_array_unref);
  g_clear_pointer (&self->built_types, g_hash_table_unref);
  g_clear_object (&self->registry);
  g_clear_object (&self->migrator);
  g_clear_object (&self->coordinator);
  g_mutex_clear (&self->mutex);
  g_clear_object (&self->driver);
  gom_trace_counter_add (GOM_TRACE_COUNTER_REPOSITORIES, -1);

  G_OBJECT_CLASS (gom_repository_parent_class)->finalize (object);
}

static void
gom_repository_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GomRepository *self = GOM_REPOSITORY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_take_object (value, gom_repository_dup_driver (self));
      break;

    case PROP_MIGRATOR:
      g_value_set_object (value, self->migrator);
      break;

    case PROP_COORDINATOR:
      g_value_take_object (value, gom_repository_dup_coordinator (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_repository_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GomRepository *self = GOM_REPOSITORY (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      self->driver = g_value_dup_object (value);
      break;

    case PROP_MIGRATOR:
      self->migrator = g_value_dup_object (value);
      break;

    case PROP_COORDINATOR:
      self->coordinator = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_repository_class_init (GomRepositoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_repository_finalize;
  object_class->get_property = gom_repository_get_property;
  object_class->set_property = gom_repository_set_property;

  properties[PROP_DRIVER] =
    g_param_spec_object ("driver", NULL, NULL,
                         GOM_TYPE_DRIVER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_MIGRATOR] =
    g_param_spec_object ("migrator", NULL, NULL,
                         GOM_TYPE_MIGRATOR,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_COORDINATOR] =
    g_param_spec_object ("coordinator", NULL, NULL,
                         GOM_TYPE_SYNC_COORDINATOR,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_repository_init (GomRepository *self)
{
  self->entity_types = g_array_new (FALSE, FALSE, sizeof (GType));
  self->built_types = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->registry = NULL;
  self->migrator = NULL;
  self->coordinator = NULL;
  g_mutex_init (&self->mutex);
  self->dirty = FALSE;
  self->sync_history_available = FALSE;
  gom_trace_counter_add (GOM_TRACE_COUNTER_REPOSITORIES, 1);
}

typedef struct
{
  GomDriver          *driver;
  GomRegistry        *registry;
  GomMigrator        *migrator;
  GomSyncCoordinator *coordinator;
} GomRepositoryNewState;

static void
gom_repository_new_state_free (GomRepositoryNewState *state)
{
  g_clear_object (&state->driver);
  g_clear_object (&state->registry);
  g_clear_object (&state->migrator);
  g_clear_object (&state->coordinator);
  g_free (state);
}

static DexFuture *
gom_repository_new_fiber (gpointer user_data)
{
  GomRepositoryNewState *state = user_data;
  g_autoptr(GomRepository) self = NULL;
  g_autoptr(GError) error = NULL;
  const GomEntitySpec * const *entities = NULL;
  guint n_entities = 0;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (state != NULL);
  g_assert (GOM_IS_DRIVER (state->driver));

  self = g_object_new (GOM_TYPE_REPOSITORY,
                       "driver", state->driver,
                       "migrator", state->migrator,
                       "coordinator", state->coordinator,
                       NULL);

  if (state->registry != NULL)
    self->registry = g_object_ref (state->registry);
  else
    self->registry = _gom_registry_new_empty ();

  entities = gom_registry_list_entities (self->registry, &n_entities);

  for (guint i = 0; i < n_entities; i++)
    {
      GType entity_type = gom_entity_spec_get_entity_type ((GomEntitySpec *)entities[i]);

      g_hash_table_add (self->built_types, GSIZE_TO_POINTER (entity_type));
      g_array_append_val (self->entity_types, entity_type);
    }

  GOM_TRACE_MARK ("Repository",
                  "open",
                  "driver=%s entities=%u",
                  G_OBJECT_TYPE_NAME (state->driver),
                  n_entities);

  if (self->migrator != NULL &&
      !dex_await (gom_migrator_update (self->migrator, state->driver), &error))
    {
      GOM_TRACE_END_MARK (start_time, "Repository", "open", "failed: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!dex_await (_gom_repository_migrate (self), &error))
    {
      GOM_TRACE_END_MARK (start_time, "Repository", "open", "failed: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (self->coordinator == NULL)
    {
      g_autofree char **relations = NULL;

      relations = dex_await_boxed (_gom_driver_list_relations (self->driver,
                                                               self->registry),
                                   &error);
      if (relations != NULL)
        self->sync_history_available = gom_repository_relations_have_sync_history (relations);
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        g_clear_error (&error);
      else
        {
          GOM_TRACE_END_MARK (start_time, "Repository", "open", "failed: %s", error->message);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  self->dirty = TRUE;

  _gom_repository_precompute (self);

  if (self->coordinator != NULL &&
      !dex_await (_gom_tombstone_ensure_schema (self), &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
        {
          GOM_TRACE_END_MARK (start_time, "Repository", "open", "failed: %s", error->message);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      self->sync_history_available = FALSE;
      g_clear_error (&error);
    }

  if (self->coordinator != NULL)
    {
      if (!dex_await (_gom_sync_history_ensure_schema (self), &error))
        {
          if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
            {
              GOM_TRACE_END_MARK (start_time, "Repository", "open", "failed: %s", error->message);
              return dex_future_new_for_error (g_steal_pointer (&error));
            }

          self->sync_history_available = FALSE;
          g_clear_error (&error);
        }
      else
        {
          self->sync_history_available = TRUE;
        }
    }

  GOM_TRACE_END_MARK (start_time,
                      "Repository",
                      "open",
                      "driver=%s entities=%u",
                      G_OBJECT_TYPE_NAME (state->driver),
                      n_entities);

  return dex_future_new_take_object (g_steal_pointer (&self));
}

/**
 * gom_repository_new:
 * @driver: a [class@Gom.Driver]
 * @registry: (nullable): a [class@Gom.Registry]
 * @migrator: (nullable): a [class@Gom.Migrator]
 *
 * Creates a new [class@Gom.Repository] using @driver to communicate
 * with storage.
 *
 * If @migrator is provided, repository initialization executes it
 * before applying registry-driven schema migrations.
 *
 * If @registry is provided, repository initialization runs migrations
 * from the backend's current schema version to the registry max version
 * before resolving successfully.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Repository] or rejects with error.
 */
DexFuture *
gom_repository_new (GomDriver   *driver,
                    GomRegistry *registry,
                    GomMigrator *migrator)
{
  return gom_repository_new_with_coordinator (driver, registry, migrator, NULL);
}

/**
 * gom_repository_new_with_coordinator:
 * @driver: a [class@Gom.Driver]
 * @registry: (nullable): a [class@Gom.Registry]
 * @migrator: (nullable): a [class@Gom.Migrator]
 * @coordinator: (nullable): a [class@Gom.SyncCoordinator]
 *
 * Creates a new [class@Gom.Repository] with a sync coordinator attached.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Repository] or rejects with error.
 */
DexFuture *
gom_repository_new_with_coordinator (GomDriver          *driver,
                                     GomRegistry        *registry,
                                     GomMigrator        *migrator,
                                     GomSyncCoordinator *coordinator)
{
  GomRepositoryNewState *state;

  dex_return_error_if_fail (GOM_IS_DRIVER (driver));

  if (registry != NULL)
    dex_return_error_if_fail (GOM_IS_REGISTRY (registry));

  if (migrator != NULL)
    dex_return_error_if_fail (GOM_IS_MIGRATOR (migrator));

  if (coordinator != NULL)
    dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (coordinator));

  state = g_new0 (GomRepositoryNewState, 1);
  state->driver = g_object_ref (driver);
  state->registry = registry ? g_object_ref (registry) : NULL;
  state->migrator = migrator ? g_object_ref (migrator) : NULL;
  state->coordinator = coordinator ? g_object_ref (coordinator) : NULL;

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_repository_new_fiber,
                              state,
                              (GDestroyNotify)gom_repository_new_state_free);
}

/**
 * gom_repository_register_entity_type:
 * @self: a [class@Gom.Repository]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 *
 * Registers @entity_type with the repository.
 */
void
gom_repository_register_entity_type (GomRepository *self,
                                     GType          entity_type)
{
  g_return_if_fail (GOM_IS_REPOSITORY (self));
  g_return_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));

  g_mutex_lock (&self->mutex);
  if (!g_hash_table_contains (self->built_types, GSIZE_TO_POINTER (entity_type)))
    {
      g_hash_table_add (self->built_types, GSIZE_TO_POINTER (entity_type));
      g_array_append_val (self->entity_types, entity_type);
      self->dirty = TRUE;
    }
  g_mutex_unlock (&self->mutex);
}

/**
 * gom_repository_list_entity_types:
 * @self: a [class@Gom.Repository]
 * @n_entity_types: (out): the number of entity types
 *
 * Lists entity types registered with the repository.
 *
 * Returns: (transfer container) (array length=n_entity_types): an array
 *   of `GType` values.
 */
GType *
gom_repository_list_entity_types (GomRepository *self,
                                  guint         *n_entity_types)
{
  GType *types = NULL;
  guint len = 0;

  g_return_val_if_fail (GOM_IS_REPOSITORY (self), NULL);
  g_return_val_if_fail (n_entity_types != NULL, NULL);

  g_mutex_lock (&self->mutex);
  len = self->entity_types->len;
  if (len > 0)
    {
      types = g_new (GType, len);
      memcpy (types, self->entity_types->data, sizeof (GType) * len);
    }
  g_mutex_unlock (&self->mutex);

  *n_entity_types = len;

  return types;
}

GomRegistry *
_gom_repository_get_registry (GomRepository *self)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (self), NULL);

  return self->registry;
}

typedef struct
{
  GomRepository *repository;
} GomRepositoryMigrateState;

static void
gom_repository_migrate_state_free (GomRepositoryMigrateState *state)
{
  g_clear_object (&state->repository);
  g_free (state);
}

static DexFuture *
gom_repository_migrate_fiber (gpointer user_data)
{
  GomRepositoryMigrateState *state = user_data;
  GomRegistry *registry;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GError) error = NULL;
  guint current_version;
  guint max_version;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));

  registry = _gom_repository_get_registry (state->repository);
  driver = gom_repository_dup_driver (state->repository);

  current_version = dex_await_uint (_gom_driver_query_version (driver), &error);
  if (error != NULL)
    {
      GOM_TRACE_END_MARK (start_time, "Migration", "plan", "failed: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  max_version = gom_registry_get_max_version (registry);
  if (current_version >= max_version)
    {
      GOM_TRACE_END_MARK (start_time,
                          "Migration",
                          "plan",
                          "current=%u target=%u steps=0",
                          current_version,
                          max_version);
      return dex_future_new_true ();
    }

  for (guint version = current_version; version < max_version; version++)
    {
      g_autoptr(GomRegistry) current = NULL;
      g_autoptr(GomRegistry) next = NULL;

      current = gom_registry_snapshot (registry, version);
      next = gom_registry_snapshot (registry, version + 1);

      GOM_TRACE_MARK ("Migration", "apply", "version=%u->%u", version, version + 1);

      if (!dex_await (_gom_driver_migrate (driver, current, next), &error))
        {
          GOM_TRACE_END_MARK (start_time,
                              "Migration",
                              "apply",
                              "failed at version=%u: %s",
                              version,
                              error->message);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  GOM_TRACE_END_MARK (start_time,
                      "Migration",
                      "plan",
                      "current=%u target=%u steps=%u",
                      current_version,
                      max_version,
                      max_version - current_version);
  return dex_future_new_true ();
}

DexFuture *
_gom_repository_migrate (GomRepository *self)
{
  GomRepositoryMigrateState *state;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));

  state = g_new0 (GomRepositoryMigrateState, 1);
  state->repository = g_object_ref (self);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_repository_migrate_fiber,
                              state,
                              (GDestroyNotify)gom_repository_migrate_state_free);
}

/**
 * gom_repository_dup_driver:
 * @self: a [class@Gom.Repository]
 *
 * Gets the driver used to create the repository.
 *
 * Returns: (transfer full): the [class@Gom.Driver]
 */
GomDriver *
gom_repository_dup_driver (GomRepository *self)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (self), NULL);
  g_return_val_if_fail (GOM_IS_DRIVER (self->driver), NULL);

  return g_object_ref (self->driver);
}

/**
 * gom_repository_dup_coordinator:
 * @self: a [class@Gom.Repository]
 *
 * Gets the sync coordinator attached to the repository.
 *
 * Returns: (transfer full) (nullable): the [class@Gom.SyncCoordinator]
 */
GomSyncCoordinator *
gom_repository_dup_coordinator (GomRepository *self)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (self), NULL);

  return self->coordinator ? g_object_ref (self->coordinator) : NULL;
}

/**
 * gom_repository_supports_feature:
 * @self: a [class@Gom.Repository]
 * @feature: a repository feature
 *
 * Checks whether the repository backend supports @feature.
 *
 * Returns: %TRUE if @feature is supported.
 */
gboolean
gom_repository_supports_feature (GomRepository        *self,
                                 GomRepositoryFeature  feature)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (self), FALSE);
  g_return_val_if_fail (GOM_IS_DRIVER (self->driver), FALSE);

  return _gom_driver_supports_feature (self->driver, feature);
}

/**
 * gom_repository_supports_vector_distance:
 * @self: a [class@Gom.Repository]
 * @format: the vector storage format
 * @metric: the vector distance metric
 *
 * Checks whether the repository backend supports SQL distance expressions for
 * @format and @metric.
 *
 * Returns: %TRUE if vector distance expressions are supported.
 */
gboolean
gom_repository_supports_vector_distance (GomRepository   *self,
                                         GomVectorFormat  format,
                                         GomVectorMetric  metric)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (self), FALSE);
  g_return_val_if_fail (GOM_IS_DRIVER (self->driver), FALSE);

  return _gom_driver_supports_vector_distance (self->driver, format, metric);
}

/**
 * gom_repository_begin_session:
 * @self: a [class@Gom.Repository]
 *
 * Opens a session that will use the repository's driver and registry.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a session
 */
DexFuture *
gom_repository_begin_session (GomRepository *self)
{
  g_autoptr(GomDriver) driver = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));

  _gom_repository_precompute (self);
  driver = gom_repository_dup_driver (self);

  return _gom_driver_begin_session (driver, self);
}

/**
 * gom_repository_query:
 * @self: a [class@Gom.Repository]
 * @query: a [class@Gom.Query] to perform
 *
 * Performs @query and returns a cursor at some point in the future.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Cursor] or rejects with error.
 */
DexFuture *
gom_repository_query (GomRepository *self,
                      GomQuery      *query)
{
  GomCursorFlags flags = GOM_CURSOR_FLAGS_NONE;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (GOM_IS_QUERY (query));

  _gom_repository_precompute (self);

  if (_gom_query_get_with_count (query))
    flags |= GOM_CURSOR_FLAGS_COUNT_ROWS;

  return dex_future_then (_gom_driver_query (self->driver, self, query, flags),
                          gom_repository_bind_cursor_cb,
                          g_object_ref (self),
                          g_object_unref);
}

/**
 * gom_repository_count:
 * @self: a [class@Gom.Repository]
 * @query: a [class@Gom.Query] describing the rows to count
 *
 * Returns the total matching row count for @query without materializing rows.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a `gint64`
 *   count or rejects with error.
 */
DexFuture *
gom_repository_count (GomRepository *self,
                      GomQuery      *query)
{
  g_autoptr(GomQuery) counted_query = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (GOM_IS_QUERY (query));

  counted_query = _gom_query_new (_gom_query_get_target_entity_type (query),
                                  _gom_query_get_target_relation (query),
                                  _gom_query_get_projections (query),
                                  _gom_query_get_filter (query),
                                  _gom_query_get_groupings (query),
                                  _gom_query_get_group_filter (query),
                                  _gom_query_get_orderings (query),
                                  _gom_query_has_offset (query) ? _gom_query_get_offset (query) : 0,
                                  _gom_query_has_limit (query) ? _gom_query_get_limit (query) : 0,
                                  _gom_query_has_offset (query),
                                  _gom_query_has_limit (query),
                                  TRUE);

  return dex_future_then (gom_repository_query (self, counted_query),
                          gom_repository_count_cb,
                          NULL,
                          NULL);
}

/**
 * gom_repository_find_one: (skip)
 * @self: a [class@Gom.Repository]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 * @first_property: the first mapped property name to match
 * @...: value for @first_property, followed by additional property/value
 *   pairs, terminated by %NULL
 *
 * Finds the first entity of @entity_type matching all provided property/value
 * pairs.
 *
 * This is a C convenience wrapper around
 * [method@Gom.Repository.find_one_with_properties]. The value for each
 * property is collected using the property's `GParamSpec` value type.
 *
 * If no row matches, the returned future resolves to %NULL. If ordering or a
 * more complex filter is required, use [method@Gom.Repository.query].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Entity] or %NULL, or rejects with error.
 */
DexFuture *
gom_repository_find_one (GomRepository *self,
                         GType          entity_type,
                         const char    *first_property,
                         ...)
{
  g_autoptr(GError) error = NULL;
  char **property_names = NULL;
  GValue *values = NULL;
  guint n_properties = 0;
  DexFuture *ret;
  va_list args;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  dex_return_error_if_fail (first_property != NULL);

  va_start (args, first_property);
  _gom_repository_precompute (self);
  if (!_gom_query_collect_properties (self->registry,
                                      entity_type,
                                      first_property,
                                      args,
                                      &n_properties,
                                      &property_names,
                                      &values,
                                      &error))
    {
      va_end (args);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }
  va_end (args);

  ret = gom_repository_find_one_with_properties (self,
                                                 entity_type,
                                                 n_properties,
                                                 (const char * const *)property_names,
                                                 values);
  _gom_query_clear_collected_properties (n_properties, property_names, values);

  return g_steal_pointer (&ret);
}

/**
 * gom_repository_find_one_with_properties:
 * @self: a [class@Gom.Repository]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 * @n_properties: number of property/value pairs
 * @properties: (array length=n_properties) (element-type utf8): mapped
 *   property names to match
 * @values: (array length=n_properties) (element-type GObject.Value): values
 *   for @properties
 *
 * Finds the first entity of @entity_type matching all provided property/value
 * pairs.
 *
 * Property names are resolved through the repository registry, so they must be
 * mapped entity properties. All predicates are equality comparisons combined
 * with `AND`.
 *
 * If no row matches, the returned future resolves to %NULL. If ordering,
 * projections, joins, or a more complex filter is required, use
 * [method@Gom.Repository.query].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Entity] or %NULL, or rejects with error.
 */
DexFuture *
gom_repository_find_one_with_properties (GomRepository      *self,
                                         GType               entity_type,
                                         guint               n_properties,
                                         const char * const *property_names,
                                         const GValue       *values)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomQuery) query = NULL;
  GomRepositoryFindOneState *state;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  dex_return_error_if_fail (n_properties > 0);
  dex_return_error_if_fail (property_names != NULL);
  dex_return_error_if_fail (values != NULL);

  _gom_repository_precompute (self);

  query = _gom_query_new_find_one (self->registry,
                                   entity_type,
                                   n_properties,
                                   property_names,
                                   values,
                                   &error);

  if (query == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  state = g_new0 (GomRepositoryFindOneState, 1);
  state->repository = g_object_ref (self);
  state->query = g_object_ref (query);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_repository_find_one_fiber,
                              state,
                              gom_repository_find_one_state_free);
}

/**
 * gom_repository_list_entities:
 * @self: a [class@Gom.Repository]
 * @entity_type: a `GType` for a [class@Gom.Entity] subclass
 * @filter: (nullable): a [class@Gom.Expression] used as the query filter
 * @ordering: (nullable): a [class@Gom.Ordering] used to sort results
 *
 * Builds a query for @entity_type, applies the optional @filter and
 * @ordering, and returns all matching entities as a list model.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of materialized entities or rejects with error.
 */
DexFuture *
gom_repository_list_entities (GomRepository *self,
                              GType          entity_type,
                              GomExpression *filter,
                              GomOrdering   *ordering)
{
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GPtrArray) orderings = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));

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

  return dex_future_then (gom_repository_query (self, query),
                          gom_repository_exhaust_cursor_to_list_cb,
                          NULL,
                          NULL);
}

/**
 * gom_repository_list_query:
 * @self: a [class@Gom.Repository]
 * @query: a [class@Gom.Query]
 *
 * Creates a lazy entity list model for @query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.EntityListModel], or a rejected future on error
 */
DexFuture *
gom_repository_list_query (GomRepository *self,
                           GomQuery      *query)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomEntityListModel) model = NULL;

  g_return_val_if_fail (GOM_IS_REPOSITORY (self), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  if (!(model = _gom_entity_list_model_new_repository (self, query, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&model));
}

/**
 * gom_repository_list_records:
 * @self: a [class@Gom.Repository]
 * @query: a [class@Gom.Query]
 *
 * Creates a lazy record list model for @query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.RecordListModel], or a rejected future on error
 */
DexFuture *
gom_repository_list_records (GomRepository *self,
                             GomQuery      *query)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRecordListModel) model = NULL;

  g_return_val_if_fail (GOM_IS_REPOSITORY (self), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  if (!(model = _gom_record_list_model_new_repository (self, query, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&model));
}

/**
 * gom_repository_mutate:
 * @self: a [class@Gom.Repository]
 * @mutation: a [class@Gom.Mutation] to perform
 *
 * Performs @mutation and returns a mutation result at some point in the future.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.MutationResult] or rejects with error.
 */
DexFuture *
gom_repository_mutate (GomRepository *self,
                       GomMutation   *mutation)
{
  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (GOM_IS_MUTATION (mutation));

  _gom_repository_precompute (self);

  return _gom_driver_mutate (self->driver, self->registry, mutation);
}

/**
 * gom_repository_insert_entity:
 * @self: a [class@Gom.Repository]
 * @entity: a [class@Gom.Entity]
 *
 * Binds @entity to @self and inserts it with [method@Gom.Entity.insert].
 *
 * This is a convenience wrapper for one-shot inserts. For transaction-scoped
 * inserts use [method@Gom.Session.insert_entity]. Use
 * [method@Gom.Entity.insert] directly if you need the mutation result.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to %TRUE when
 *   @entity has been inserted, or rejects with error.
 */
DexFuture *
gom_repository_insert_entity (GomRepository *self,
                              GomEntity     *entity)
{
  GomRepositoryInsertEntityState *state;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));

  _gom_repository_precompute (self);

  state = g_new0 (GomRepositoryInsertEntityState, 1);
  state->repository = g_object_ref (self);
  state->entity = g_object_ref (entity);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_repository_insert_entity_fiber,
                              state,
                              gom_repository_insert_entity_state_free);
}

/**
 * gom_repository_describe_relation:
 * @self: a [class@Gom.Repository]
 * @relation: a relation name
 *
 * Describes @relation.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.RelationSchema] or rejects with error.
 */
DexFuture *
gom_repository_describe_relation (GomRepository *self,
                                  const char    *relation)
{
  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));
  dex_return_error_if_fail (relation != NULL);

  _gom_repository_precompute (self);

  return _gom_driver_describe_relation (self->driver, self->registry, relation);
}

/**
 * gom_repository_list_relations:
 * @self: a [class@Gom.Repository]
 *
 * Lists relations available in the repository.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   `GStrv` of relation names or rejects with error.
 */
DexFuture *
gom_repository_list_relations (GomRepository *self)
{
  dex_return_error_if_fail (GOM_IS_REPOSITORY (self));

  _gom_repository_precompute (self);

  return _gom_driver_list_relations (self->driver, self->registry);
}

gboolean
_gom_repository_has_sync_history (GomRepository *self)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (self), FALSE);

  return self->sync_history_available;
}

void
_gom_repository_set_sync_history_available (GomRepository *self,
                                            gboolean       available)
{
  g_return_if_fail (GOM_IS_REPOSITORY (self));

  self->sync_history_available = !!available;
}
