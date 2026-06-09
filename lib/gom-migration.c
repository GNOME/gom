/* gom-migration.c
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

enum
{
  PROP_0,
  PROP_VERSION,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (GomMigration, gom_migration, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gom_migration_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GomMigration *self = GOM_MIGRATION (object);

  switch (prop_id)
    {
    case PROP_VERSION:
      g_value_set_uint (value, gom_migration_get_version (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_migration_class_init (GomMigrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gom_migration_get_property;

  properties[PROP_VERSION] =
    g_param_spec_uint ("version", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_migration_init (GomMigration *self)
{
}

/**
 * gom_migration_apply:
 * @self: a [class@Gom.Migration]
 * @driver: a [class@Gom.Driver]
 *
 * Applies the migration using @driver.
 *
 * Returns: (transfer full): A [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
DexFuture *
gom_migration_apply (GomMigration *self,
                     GomDriver    *driver)
{
  dex_return_error_if_fail (GOM_IS_MIGRATION (self));
  dex_return_error_if_fail (GOM_IS_DRIVER (driver));

  if (GOM_MIGRATION_GET_CLASS (self)->apply)
    return GOM_MIGRATION_GET_CLASS (self)->apply (self, driver);

  return dex_future_new_true ();
}

guint
gom_migration_get_version (GomMigration *self)
{
  g_return_val_if_fail (GOM_IS_MIGRATION (self), 0);

  if (GOM_MIGRATION_GET_CLASS (self)->get_version)
    return GOM_MIGRATION_GET_CLASS (self)->get_version (self);

  return 0;
}
