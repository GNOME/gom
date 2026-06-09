/* gom-custom-migration.c
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

#include "gom-migration.h"
#include "gom-custom-migration.h"
#include "gom-driver.h"

struct _GomCustomMigration
{
  GomMigration           parent_instance;
  GomCustomMigrationFunc apply_func;
  gpointer               user_data;
  GDestroyNotify         user_destroy;
  guint                  version;
};

G_DEFINE_FINAL_TYPE (GomCustomMigration, gom_custom_migration, GOM_TYPE_MIGRATION)

static guint
gom_custom_migration_get_version (GomMigration *migration)
{
  return GOM_CUSTOM_MIGRATION (migration)->version;
}

static DexFuture *
gom_custom_migration_apply (GomMigration *migration,
                            GomDriver    *driver)
{
  GomCustomMigration *self = GOM_CUSTOM_MIGRATION (migration);

  g_assert (GOM_IS_CUSTOM_MIGRATION (migration));
  g_assert (GOM_IS_DRIVER (driver));

  if (self->apply_func != NULL)
    return self->apply_func (driver, self->user_data);

  return dex_future_new_true ();
}

static void
gom_custom_migration_finalize (GObject *object)
{
  GomCustomMigration *self = GOM_CUSTOM_MIGRATION (object);

  if (self->user_destroy != NULL)
    {
      self->user_destroy (self->user_data);
      self->user_data = NULL;
      self->user_destroy = NULL;
    }

  G_OBJECT_CLASS (gom_custom_migration_parent_class)->finalize (object);
}

static void
gom_custom_migration_class_init (GomCustomMigrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomMigrationClass *migration_class = GOM_MIGRATION_CLASS (klass);

  object_class->finalize = gom_custom_migration_finalize;

  migration_class->get_version = gom_custom_migration_get_version;
  migration_class->apply = gom_custom_migration_apply;
}

static void
gom_custom_migration_init (GomCustomMigration *self)
{
}

/**
 * gom_custom_migration_new:
 * @version: the migration version number
 * @apply_func: (nullable): callback to run when the migration is applied
 * @user_data: (nullable): user data to pass to @apply_func
 * @user_destroy: (nullable): destroy notify for @user_data
 *
 * Creates a new [class@Gom.CustomMigration] that invokes @apply_func
 * when [method@Gom.Migration.apply] is called.
 *
 * If @apply_func is %NULL, applying the migration succeeds immediately
 * with no work done.
 *
 * Returns: (transfer full): a new [class@Gom.CustomMigration]
 */
GomMigration *
gom_custom_migration_new (guint                  version,
                          GomCustomMigrationFunc apply_func,
                          gpointer               user_data,
                          GDestroyNotify         user_destroy)
{
  GomCustomMigration *self;

  self = g_object_new (GOM_TYPE_CUSTOM_MIGRATION, NULL);
  self->version = version;
  self->apply_func = apply_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  return GOM_MIGRATION (self);
}
