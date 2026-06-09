/* gom-sqlite-lease-private.h
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

#define GOM_TYPE_SQLITE_LEASE (gom_sqlite_lease_get_type())

G_DECLARE_FINAL_TYPE (GomSqliteLease, gom_sqlite_lease, GOM, SQLITE_LEASE, GObject)

GomSqliteLease      *gom_sqlite_lease_new                  (GomSqliteConnection *connection,
                                                            GomSqlitePool       *pool);
GomSqliteConnection *gom_sqlite_lease_get_connection       (GomSqliteLease      *self);
GomSqliteLeaseState *gom_sqlite_lease_state_ref            (GomSqliteLeaseState *state);
GomSqliteLeaseState *gom_sqlite_lease_ref_state            (GomSqliteLease      *self);
void                 gom_sqlite_lease_state_unref          (GomSqliteLeaseState *state);
GomSqliteConnection *gom_sqlite_lease_state_get_connection (GomSqliteLeaseState *state);
DexFuture           *gom_sqlite_lease_state_invoke         (GomSqliteLeaseState *state,
                                                            const char          *thread_name,
                                                            DexThreadFunc        thread_func,
                                                            gpointer             user_data,
                                                            GDestroyNotify       user_data_destroy);
DexFuture           *gom_sqlite_lease_invoke               (GomSqliteLease      *self,
                                                            const char          *thread_name,
                                                            DexThreadFunc        thread_func,
                                                            gpointer             user_data,
                                                            GDestroyNotify       user_data_destroy);

G_END_DECLS
