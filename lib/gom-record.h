/* gom-record.h
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

#include "gom-types.h"

#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_RECORD (gom_record_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomRecord, gom_record, GOM, RECORD, GObject)

GOM_AVAILABLE_IN_ALL
guint       gom_record_get_n_columns        (GomRecord  *self);
GOM_AVAILABLE_IN_ALL
const char *gom_record_get_column_name      (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
gboolean    gom_record_get_column           (GomRecord  *self,
                                             guint       column,
                                             GValue     *value);
GOM_AVAILABLE_IN_ALL
gboolean    gom_record_get_column_boolean   (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
const char *gom_record_get_column_string    (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
gint64      gom_record_get_column_int64     (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
char       *gom_record_dup_column_string    (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
GDateTime  *gom_record_dup_column_date_time (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
GBytes     *gom_record_dup_column_bytes     (GomRecord  *self,
                                             guint       column);
GOM_AVAILABLE_IN_ALL
gboolean    gom_record_get_column_by_name   (GomRecord  *self,
                                             const char *name,
                                             GValue     *value);

G_END_DECLS
