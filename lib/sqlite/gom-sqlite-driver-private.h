/* gom-sqlite-driver-private.h
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
#include <sqlite3.h>

#include "gom-driver.h"
#include "gom-driver-private.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

#define GOM_TYPE_SQLITE_DRIVER (gom_sqlite_driver_get_type())

G_DECLARE_FINAL_TYPE (GomSqliteDriver, gom_sqlite_driver, GOM, SQLITE_DRIVER, GomDriver)

gboolean   gom_sqlite_driver_exec_sql        (sqlite3              *db,
                                              const char           *sql,
                                              const char           *action,
                                              GError              **error);
int        gom_sqlite_driver_step            (sqlite3_stmt         *stmt,
                                              const char           *action,
                                              GError              **error);
GomDriver *_gom_sqlite_driver_new            (const char           *uri,
                                              GomDriverOptions     *options,
                                              GError              **error);
DexFuture *gom_sqlite_driver_query_on_lease  (GomSqliteLeaseState  *lease_state,
                                              GomRepository        *repository,
                                              GomQuery             *query,
                                              GomCursorFlags        flags,
                                              gboolean              transaction_active) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *gom_sqlite_driver_mutate_on_lease (GomSqliteLeaseState  *lease_state,
                                              GomRegistry          *registry,
                                              GomMutation          *mutation) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
