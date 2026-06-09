/* gom-pgsql-driver-private.h
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
#include <pgsql-glib.h>

#include "gom-driver.h"
#include "gom-driver-private.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

#define GOM_TYPE_PGSQL_DRIVER (gom_pgsql_driver_get_type())

typedef DexFuture *(*GomPgsqlQueryRunner) (gpointer     executor,
                                           const char  *sql,
                                           PgsqlParams *params);

G_DECLARE_FINAL_TYPE (GomPgsqlDriver, gom_pgsql_driver, GOM, PGSQL_DRIVER, GomDriver)

GomDriver *_gom_pgsql_driver_new        (const char           *uri,
                                         GomDriverOptions     *options,
                                         GError              **error);
DexFuture *gom_pgsql_query_on_executor  (GomRepository        *repository,
                                         GomQuery             *query,
                                         GomCursorFlags        flags,
                                         gpointer              executor,
                                         GomPgsqlQueryRunner   runner) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *gom_pgsql_mutate_on_executor (GomRegistry          *registry,
                                         GomMutation          *mutation,
                                         gpointer              executor,
                                         GomPgsqlQueryRunner   runner) G_GNUC_WARN_UNUSED_RESULT;
G_END_DECLS
