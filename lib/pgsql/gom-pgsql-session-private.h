/* gom-pgsql-session-private.h
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

#include <pgsql-glib.h>

#include "gom-session.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

#define GOM_TYPE_PGSQL_SESSION (gom_pgsql_session_get_type())

GOM_DECLARE_INTERNAL_TYPE (GomPgsqlSession, gom_pgsql_session, GOM, PGSQL_SESSION, GomSession)

GomPgsqlSession *gom_pgsql_session_new (GomRepository       *repository,
                                        PgsqlConnectionPool *pool,
                                        PgsqlConnection     *connection,
                                        PgsqlTransaction    *transaction);

G_END_DECLS
