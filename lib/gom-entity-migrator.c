/* gom-entity-migrator.c
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

#include "gom-driver-private.h"
#include "gom-entity-migrator.h"
#include "gom-meta.h"
#include "gom-migrator-private.h"

struct _GomEntityMigrator
{
  GomMigrator  parent_instance;
  GomRegistry *registry;
};

struct _GomEntityMigratorClass
{
  GomMigratorClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomEntityMigrator, gom_entity_migrator, GOM_TYPE_MIGRATOR)

static DexFuture *gom_entity_migrator_update (GomMigrator *migrator,
                                              GomDriver   *driver);

enum
{
  PROP_0,
  PROP_REGISTRY,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gom_entity_migrator_finalize (GObject *object)
{
  GomEntityMigrator *self = (GomEntityMigrator *)object;

  g_clear_object (&self->registry);

  G_OBJECT_CLASS (gom_entity_migrator_parent_class)->finalize (object);
}

static void
gom_entity_migrator_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GomEntityMigrator *self = GOM_ENTITY_MIGRATOR (object);

  switch (prop_id)
    {
    case PROP_REGISTRY:
      g_value_set_object (value, self->registry);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_entity_migrator_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GomEntityMigrator *self = GOM_ENTITY_MIGRATOR (object);

  switch (prop_id)
    {
    case PROP_REGISTRY:
      self->registry = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_entity_migrator_class_init (GomEntityMigratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomMigratorClass *migrator_class = GOM_MIGRATOR_CLASS (klass);

  object_class->get_property = gom_entity_migrator_get_property;
  object_class->set_property = gom_entity_migrator_set_property;
  object_class->finalize = gom_entity_migrator_finalize;
  migrator_class->update = gom_entity_migrator_update;

  properties[PROP_REGISTRY] =
    g_param_spec_object ("registry", NULL, NULL,
                         GOM_TYPE_REGISTRY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_entity_migrator_init (GomEntityMigrator *self)
{
  self->registry = NULL;
}

/**
 * gom_entity_migrator_new:
 * @registry: a [class@Gom.Registry]
 *
 * Creates a new [class@Gom.EntityMigrator].
 *
 * Use this to automatically migrate your schema based on entity
 * metadata in @registry.
 *
 * Returns: (transfer full): a new [class@Gom.EntityMigrator]
 */
GomEntityMigrator *
gom_entity_migrator_new (GomRegistry *registry)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);

  return g_object_new (GOM_TYPE_ENTITY_MIGRATOR,
                       "registry", registry,
                       NULL);
}

typedef struct
{
  GomEntityMigrator *migrator;
  GomDriver         *driver;
} GomEntityMigratorUpdateState;

static void
gom_entity_migrator_update_state_free (GomEntityMigratorUpdateState *state)
{
  g_clear_object (&state->migrator);
  g_clear_object (&state->driver);
  g_free (state);
}

static DexFuture *
gom_entity_migrator_update_fiber (gpointer user_data)
{
  GomEntityMigratorUpdateState *state = user_data;
  g_autoptr(GomRegistry) current = NULL;
  g_autoptr(GomRegistry) next = NULL;
  g_autoptr(GError) error = NULL;
  guint current_version;
  guint max_version;

  g_assert (state != NULL);
  g_assert (GOM_IS_ENTITY_MIGRATOR (state->migrator));
  g_assert (GOM_IS_DRIVER (state->driver));
  g_assert (GOM_IS_REGISTRY (state->migrator->registry));

  current_version = dex_await_uint (_gom_driver_query_version (state->driver), &error);
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  max_version = gom_registry_get_max_version (state->migrator->registry);
  if (current_version >= max_version)
    return dex_future_new_true ();

  for (guint version = current_version; version < max_version; version++)
    {
      current = gom_registry_snapshot (state->migrator->registry, version);
      next = gom_registry_snapshot (state->migrator->registry, version + 1);

      if (!dex_await (_gom_driver_migrate (state->driver, current, next), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
gom_entity_migrator_update (GomMigrator *migrator,
                            GomDriver   *driver)
{
  GomEntityMigratorUpdateState *state;

  g_assert (GOM_IS_ENTITY_MIGRATOR (migrator));
  g_assert (GOM_IS_DRIVER (driver));

  state = g_new0 (GomEntityMigratorUpdateState, 1);
  state->migrator = g_object_ref (GOM_ENTITY_MIGRATOR (migrator));
  state->driver = g_object_ref (driver);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_entity_migrator_update_fiber,
                              state,
                              (GDestroyNotify)gom_entity_migrator_update_state_free);
}
