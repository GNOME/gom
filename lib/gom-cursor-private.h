/* gom-cursor-private.h
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

#include "gom-cursor.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

struct _GomCursor
{
  GObject        parent_instance;
  GType          entity_type;
  GomRepository *repository;
  const char    *discriminator_field;
  gint           discriminator_column;
  guint          discriminator_cached : 1;
  GHashTable    *discriminator_cache;
  GomSession    *session;
};

struct _GomCursorClass
{
  GObjectClass parent_class;

  guint                  (*get_n_columns)     (GomCursor *self);
  const char            *(*get_column_name)   (GomCursor *self,
                                               guint      column);
  gboolean               (*get_column_value)  (GomCursor *self,
                                               guint      column,
                                               GValue    *value);
  const char            *(*get_column_string) (GomCursor *self,
                                               guint      column);
  DexFuture             *(*next)              (GomCursor *self);
  DexFuture             *(*close)             (GomCursor *self);
  DexFuture             *(*exhaust)           (GomCursor *self);
  DexFuture             *(*rewind)            (GomCursor *self);
  DexFuture             *(*move_absolute)     (GomCursor *self,
                                               guint64    position);
  DexFuture             *(*move_relative)     (GomCursor *self,
                                               gint64     offset);
  GomCursorCapabilities  (*get_capabilities)  (GomCursor *self);
  guint64                (*get_count)         (GomCursor *self);
};

void           _gom_cursor_set_repository     (GomCursor     *self,
                                               GomRepository *repository);
GomRepository *_gom_cursor_dup_repository     (GomCursor     *self);
void           _gom_cursor_set_session        (GomCursor     *self,
                                               GomSession    *session);
GomSession    *_gom_cursor_dup_session        (GomCursor     *self);
DexFuture     *_gom_cursor_exhaust_to_records (GomCursor     *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
