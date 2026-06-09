/* gom-nested-migration.c
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

#include "gom-driver.h"
#include "gom-migration.h"
#include "gom-migrator.h"
#include "gom-nested-migration.h"

struct _GomNestedMigration
{
  GomMigration  parent_instance;
  GomMigrator  *migrator;
  GomDriver    *driver;
};

enum
{
  PROP_0,
  PROP_DRIVER,
  PROP_MIGRATOR,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomNestedMigration, gom_nested_migration, GOM_TYPE_MIGRATION)

static GParamSpec *properties[N_PROPS];

static DexFuture *
gom_nested_migration_apply (GomMigration *migration,
                            GomDriver    *driver)
{
  GomNestedMigration *self = (GomNestedMigration *)migration;

  g_assert (GOM_IS_NESTED_MIGRATION (self));
  g_assert (GOM_IS_DRIVER (driver));

  if (self->driver != NULL)
    driver = self->driver;

  return gom_migrator_update (self->migrator, driver);
}

static void
gom_nested_migration_finalize (GObject *object)
{
  GomNestedMigration *self = (GomNestedMigration *)object;

  g_clear_object (&self->driver);
  g_clear_object (&self->migrator);

  G_OBJECT_CLASS (gom_nested_migration_parent_class)->finalize (object);
}

static void
gom_nested_migration_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GomNestedMigration *self = GOM_NESTED_MIGRATION (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      g_value_set_object (value, self->driver);
      break;

    case PROP_MIGRATOR:
      g_value_set_object (value, self->migrator);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_nested_migration_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GomNestedMigration *self = GOM_NESTED_MIGRATION (object);

  switch (prop_id)
    {
    case PROP_DRIVER:
      self->driver = g_value_dup_object (value);
      break;

    case PROP_MIGRATOR:
      self->migrator = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_nested_migration_class_init (GomNestedMigrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomMigrationClass *migration_class = GOM_MIGRATION_CLASS (klass);

  object_class->finalize = gom_nested_migration_finalize;
  object_class->get_property = gom_nested_migration_get_property;
  object_class->set_property = gom_nested_migration_set_property;

  migration_class->apply = gom_nested_migration_apply;

  properties[PROP_DRIVER] =
    g_param_spec_object ("driver", NULL, NULL,
                         GOM_TYPE_DRIVER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_MIGRATOR] =
    g_param_spec_object ("migrator", NULL, NULL,
                         GOM_TYPE_MIGRATOR,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_nested_migration_init (GomNestedMigration *self)
{
}

/**
 * gom_nested_migration_new:
 * @migrator: the nested migrator
 * @driver: (nullable): the driver to use or %NULL to inherit
 *
 * If @driver is `null`, then the same driver applied to the
 * nested migrator will be used.
 *
 * Returns: (transfer full): a [class@Gom.NestedMigration]
 */
GomMigration *
gom_nested_migration_new (GomMigrator *migrator,
                          GomDriver   *driver)
{
  g_return_val_if_fail (GOM_IS_MIGRATOR (migrator), NULL);
  g_return_val_if_fail (!driver || GOM_IS_DRIVER (driver), NULL);

  return g_object_new (GOM_TYPE_NESTED_MIGRATION,
                       "migrator", migrator,
                       "driver", driver,
                       NULL);
}
