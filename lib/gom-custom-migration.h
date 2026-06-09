/* gom-custom-migration.h
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

#pragma once

#include <libdex.h>

#include "gom-migration.h"

G_BEGIN_DECLS

#define GOM_TYPE_CUSTOM_MIGRATION (gom_custom_migration_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomCustomMigration, gom_custom_migration, GOM, CUSTOM_MIGRATION, GomMigration)

/**
 * GomCustomMigrationFunc:
 * @driver: a GomDriver
 * @user_data: user data provided when registering the callback
 *
 * This callback is used by the [class@Gom.CustomMigration] to perform custom
 * operations. It is an alternative to subclassing [class@Gom.Migration]
 * directly.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any value
 *   or rejects with error.
 */
typedef DexFuture *(*GomCustomMigrationFunc) (GomDriver *driver,
                                              gpointer   user_data);

GOM_AVAILABLE_IN_ALL
GomMigration *gom_custom_migration_new (guint                  version,
                                        GomCustomMigrationFunc apply_func,
                                        gpointer               user_data,
                                        GDestroyNotify         user_destroy);

G_END_DECLS
