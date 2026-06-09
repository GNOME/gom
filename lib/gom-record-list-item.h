/* gom-record-list-item.h
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

#define GOM_TYPE_RECORD_LIST_ITEM (gom_record_list_item_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomRecordListItem, gom_record_list_item, GOM, RECORD_LIST_ITEM, GObject)

GOM_AVAILABLE_IN_ALL
GomRecordListItem *gom_record_list_item_new          (guint              position);
GOM_AVAILABLE_IN_ALL
GomRecord         *gom_record_list_item_dup_record   (GomRecordListItem *self);
GOM_AVAILABLE_IN_ALL
guint              gom_record_list_item_get_position (GomRecordListItem *self);
GOM_AVAILABLE_IN_ALL
gboolean           gom_record_list_item_get_loading  (GomRecordListItem *self);

G_END_DECLS
