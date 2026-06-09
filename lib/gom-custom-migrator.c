/* gom-custom-migrator.c
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

#include <libdex.h>

#include "gom-custom-migrator.h"
#include "gom-driver.h"
#include "gom-migration.h"
#include "gom-migrator-private.h"

struct _GomCustomMigrator
{
  GomMigrator  parent_instance;
  GPtrArray   *migrations;
  guint        version;
};

struct _GomCustomMigratorClass
{
  GomMigratorClass parent_class;
};

enum
{
  PROP_0,
  PROP_VERSION,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (GomCustomMigrator, gom_custom_migrator, GOM_TYPE_MIGRATOR)

static void
gom_custom_migrator_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GomCustomMigrator *self = GOM_CUSTOM_MIGRATOR (object);

  switch (prop_id)
    {
    case PROP_VERSION:
      g_value_set_uint (value, self->version);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_custom_migrator_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GomCustomMigrator *self = GOM_CUSTOM_MIGRATOR (object);

  switch (prop_id)
    {
    case PROP_VERSION:
      self->version = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_custom_migrator_finalize (GObject *object)
{
  GomCustomMigrator *self = (GomCustomMigrator *)object;

  g_clear_pointer (&self->migrations, g_ptr_array_unref);

  G_OBJECT_CLASS (gom_custom_migrator_parent_class)->finalize (object);
}

static gint
migration_compare_by_version (gconstpointer a,
                              gconstpointer b)
{
  GomMigration *ma = *(GomMigration **)a;
  GomMigration *mb = *(GomMigration **)b;
  guint va = gom_migration_get_version (ma);
  guint vb = gom_migration_get_version (mb);

  if (va < vb)
    return -1;
  else if (va > vb)
    return 1;
  else
    return 0;
}

typedef struct _Apply
{
  GomCustomMigrator *migrator;
  GomDriver         *driver;
} Apply;

static void
apply_free (Apply *state)
{
  g_clear_object (&state->migrator);
  g_clear_object (&state->driver);
  g_free (state);
}

static DexFuture *
gom_custom_migrator_apply_fiber (gpointer user_data)
{
  Apply *state = user_data;
  g_autoptr(GPtrArray) to_apply = NULL;
  guint current_version;

  g_assert (state != NULL);
  g_assert (GOM_IS_CUSTOM_MIGRATOR (state->migrator));
  g_assert (GOM_IS_DRIVER (state->driver));

  to_apply = g_ptr_array_new_with_free_func (g_object_unref);
  current_version = state->migrator->version;

  for (guint i = 0; i < state->migrator->migrations->len; i++)
    {
      GomMigration *migration = g_ptr_array_index (state->migrator->migrations, i);

      if (gom_migration_get_version (migration) > current_version)
        g_ptr_array_add (to_apply, g_object_ref (migration));
    }

  g_ptr_array_sort (to_apply, migration_compare_by_version);

  for (guint i = 0; i < to_apply->len; i++)
    {
      GomMigration *migration = g_ptr_array_index (to_apply, i);
      g_autoptr(GError) error = NULL;

      if (!dex_await (gom_migration_apply (migration, state->driver), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
gom_custom_migrator_update (GomMigrator *migrator,
                            GomDriver   *driver)
{
  Apply *state;

  g_assert (GOM_IS_CUSTOM_MIGRATOR (migrator));
  g_assert (GOM_IS_DRIVER (driver));

  state = g_new0 (Apply, 1);
  state->migrator = g_object_ref (GOM_CUSTOM_MIGRATOR (migrator));
  state->driver = g_object_ref (driver);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_custom_migrator_apply_fiber,
                              state,
                              (GDestroyNotify)apply_free);
}

static void
gom_custom_migrator_class_init (GomCustomMigratorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomMigratorClass *migrator_class = GOM_MIGRATOR_CLASS (klass);

  object_class->finalize = gom_custom_migrator_finalize;
  object_class->get_property = gom_custom_migrator_get_property;
  object_class->set_property = gom_custom_migrator_set_property;

  migrator_class->update = gom_custom_migrator_update;

  properties[PROP_VERSION] =
    g_param_spec_uint ("version", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_custom_migrator_init (GomCustomMigrator *self)
{
  self->migrations = g_ptr_array_new_with_free_func (g_object_unref);
}

GomCustomMigrator *
gom_custom_migrator_new (guint version)
{
  return g_object_new (GOM_TYPE_CUSTOM_MIGRATOR,
                       "version", version,
                       NULL);
}

/**
 * gom_custom_migrator_add_migration:
 * @self: a [class@Gom.CustomMigrator]
 * @migration: (transfer full): a migration to possibly perform
 *
 * Adds @migration to the list of migrations.
 *
 * When this migrator is updating, it will perform @migration if the
 * version falls within the boundaries of what is being updated.
 */
void
gom_custom_migrator_add_migration (GomCustomMigrator *self,
                                   GomMigration      *migration)
{
  g_return_if_fail (GOM_IS_CUSTOM_MIGRATOR (self));
  g_return_if_fail (GOM_IS_MIGRATION (migration));

  g_ptr_array_add (self->migrations, migration);
}
