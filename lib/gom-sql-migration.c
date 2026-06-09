/* gom-sql-migration.c
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
#include "gom-migration.h"
#include "gom-sql-migration.h"

struct _GomSqlMigration
{
  GomMigration  parent_instance;
  GBytes       *script;
  guint         version;
};

enum
{
  PROP_0,
  PROP_SCRIPT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomSqlMigration, gom_sql_migration, GOM_TYPE_MIGRATION)

static GParamSpec *properties[N_PROPS];

static guint
gom_sql_migration_get_version (GomMigration *migration)
{
  return GOM_SQL_MIGRATION (migration)->version;
}

static DexFuture *
gom_sql_migration_apply (GomMigration *migration,
                         GomDriver    *driver)
{
  GomSqlMigration *self = GOM_SQL_MIGRATION (migration);

  g_assert (GOM_IS_SQL_MIGRATION (migration));
  g_assert (GOM_IS_DRIVER (driver));

  if (self->script == NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "SQL migration script is required");

  return _gom_driver_execute_sql (driver, self->script);
}

static void
gom_sql_migration_finalize (GObject *object)
{
  GomSqlMigration *self = (GomSqlMigration *)object;

  g_clear_pointer (&self->script, g_bytes_unref);

  G_OBJECT_CLASS (gom_sql_migration_parent_class)->finalize (object);
}

static void
gom_sql_migration_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GomSqlMigration *self = GOM_SQL_MIGRATION (object);

  switch (prop_id)
    {
    case PROP_SCRIPT:
      g_value_take_boxed (value, gom_sql_migration_dup_script (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sql_migration_class_init (GomSqlMigrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomMigrationClass *migration_class = GOM_MIGRATION_CLASS (klass);

  object_class->finalize = gom_sql_migration_finalize;
  object_class->get_property = gom_sql_migration_get_property;

  migration_class->apply = gom_sql_migration_apply;
  migration_class->get_version = gom_sql_migration_get_version;

  properties[PROP_SCRIPT] =
    g_param_spec_boxed ("script", NULL, NULL,
                        G_TYPE_BYTES,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_sql_migration_init (GomSqlMigration *self)
{
}

/**
 * gom_sql_migration_new:
 * @script: a [struct@GLib.Bytes] containing the SQL to execute
 *
 * Creates a new [class@Gom.SqlMigration] that will execute the
 * SQL script provided.
 *
 * Returns: (transfer full):
 */
GomMigration *
gom_sql_migration_new (guint   version,
                       GBytes *script)
{
  GomSqlMigration *self;

  g_return_val_if_fail (script != NULL, NULL);

  self = g_object_new (GOM_TYPE_SQL_MIGRATION, NULL);
  self->script = g_bytes_ref (script);
  self->version = version;

  return GOM_MIGRATION (self);
}

/**
 * gom_sql_migration_dup_script:
 * @self: a [class@Gom.SqlMigration]
 *
 * Get the underlying script to be run.
 *
 * Returns: (transfer full): a [struct@GLib.Bytes]
 */
GBytes *
gom_sql_migration_dup_script (GomSqlMigration *self)
{
  g_return_val_if_fail (GOM_IS_SQL_MIGRATION (self), NULL);
  g_return_val_if_fail (self->script != NULL, NULL);

  return g_bytes_ref (self->script);
}
