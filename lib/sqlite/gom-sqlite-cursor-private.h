/* gom-sqlite-cursor-private.h
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

#include "gom-cursor.h"
#include "gom-cursor-private.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

#define GOM_TYPE_SQLITE_CURSOR (gom_sqlite_cursor_get_type())

G_DECLARE_FINAL_TYPE (GomSqliteCursor, gom_sqlite_cursor, GOM, SQLITE_CURSOR, GomCursor)

GomSqliteCursor *gom_sqlite_cursor_new     (GomSqliteStatement *statement,
                                            GomRepository      *repository,
                                            char               *sql,
                                            guint64             count,
                                            gboolean            has_count,
                                            gboolean            owns_transaction,
                                            GType               entity_type);
const char      *gom_sqlite_cursor_get_sql (GomSqliteCursor    *self);

G_END_DECLS
