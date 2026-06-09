/* gom-sql-migration.h
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

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_SQL_MIGRATION (gom_sql_migration_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomSqlMigration, gom_sql_migration, GOM, SQL_MIGRATION, GomMigration)

GOM_AVAILABLE_IN_ALL
GomMigration *gom_sql_migration_new        (guint            version,
                                            GBytes          *script);
GOM_AVAILABLE_IN_ALL
GBytes       *gom_sql_migration_dup_script (GomSqlMigration *self);

G_END_DECLS
