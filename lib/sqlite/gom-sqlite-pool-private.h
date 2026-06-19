/* gom-sqlite-pool-private.h
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

#include "gom-types-private.h"

G_BEGIN_DECLS

#define GOM_TYPE_SQLITE_POOL (gom_sqlite_pool_get_type())

#define GOM_SQLITE_POOL_MAX_LEASES 4
#define GOM_SQLITE_POOL_OPEN_THREADS 2
#define GOM_SQLITE_POOL_MAX_CONNECTION_OPENS 2

G_DECLARE_FINAL_TYPE (GomSqlitePool, gom_sqlite_pool, GOM, SQLITE_POOL, GObject)

GomSqlitePool *gom_sqlite_pool_new                (const char          *uri,
                                                   GBytes              *encryption_key);
DexFuture     *gom_sqlite_pool_acquire            (GomSqlitePool       *self);
void           gom_sqlite_pool_clear_idle         (GomSqlitePool       *self);
void           gom_sqlite_pool_return_connection  (GomSqlitePool       *self,
                                                   GomSqliteConnection *connection);
void           gom_sqlite_pool_set_encryption_key (GomSqlitePool       *self,
                                                   GBytes              *encryption_key);

G_END_DECLS
