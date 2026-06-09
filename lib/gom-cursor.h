/* gom-cursor.h
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

#define GOM_TYPE_CURSOR (gom_cursor_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomCursor, gom_cursor, GOM, CURSOR, GObject)

GOM_AVAILABLE_IN_ALL
guint                  gom_cursor_get_n_columns      (GomCursor   *self);
GOM_AVAILABLE_IN_ALL
const char            *gom_cursor_get_column_name    (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_next               (GomCursor   *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_close              (GomCursor   *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_exhaust            (GomCursor   *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_exhaust_to_list    (GomCursor   *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_rewind             (GomCursor   *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_move_absolute      (GomCursor   *self,
                                                      guint64      position) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture             *gom_cursor_move_relative      (GomCursor   *self,
                                                      gint64       offset) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
gboolean               gom_cursor_is_closed          (GomCursor   *self);
GOM_AVAILABLE_IN_ALL
GomEntity             *gom_cursor_materialize        (GomCursor   *self,
                                                      GError     **error);
GOM_AVAILABLE_IN_ALL
GomRecord             *gom_cursor_snapshot           (GomCursor   *self,
                                                      GError     **error);
GOM_AVAILABLE_IN_ALL
gboolean               gom_cursor_get_column         (GomCursor   *self,
                                                      guint        column,
                                                      GValue      *value);
GOM_AVAILABLE_IN_ALL
gboolean               gom_cursor_get_column_null    (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
gboolean               gom_cursor_get_column_boolean (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
double                 gom_cursor_get_column_double  (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
gboolean               gom_cursor_get_column_by_name (GomCursor   *self,
                                                      const char  *name,
                                                      GValue      *value);
GOM_AVAILABLE_IN_ALL
GBytes                *gom_cursor_dup_column_bytes   (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
const char            *gom_cursor_get_column_string  (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
gint64                 gom_cursor_get_column_int64   (GomCursor   *self,
                                                      guint        column);
GOM_AVAILABLE_IN_ALL
GomCursorCapabilities  gom_cursor_get_capabilities   (GomCursor   *self);
GOM_AVAILABLE_IN_ALL
guint64                gom_cursor_get_count          (GomCursor   *self);

G_END_DECLS
