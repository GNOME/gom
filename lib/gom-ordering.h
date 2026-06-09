/* gom-ordering.h
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

#define GOM_TYPE_ORDERING (gom_ordering_get_type())
#define GOM_TYPE_SORT_DIRECTION (gom_sort_direction_get_type())
#define GOM_TYPE_NULLS_MODE (gom_nulls_mode_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomOrdering, gom_ordering, GOM, ORDERING, GObject)

GOM_AVAILABLE_IN_ALL
GType             gom_sort_direction_get_type (void);
GOM_AVAILABLE_IN_ALL
GType             gom_nulls_mode_get_type     (void);
GOM_AVAILABLE_IN_ALL
GomOrdering      *gom_ordering_new            (GomExpression    *expression,
                                               GomSortDirection  direction);
GOM_AVAILABLE_IN_ALL
GomOrdering      *gom_ordering_new_full       (GomExpression    *expression,
                                               GomNullsMode      nulls_mode);
GOM_AVAILABLE_IN_ALL
GomExpression    *gom_ordering_get_expression (GomOrdering      *self);
GOM_AVAILABLE_IN_ALL
GomSortDirection  gom_ordering_get_direction  (GomOrdering      *self);
GOM_AVAILABLE_IN_ALL
GomNullsMode      gom_ordering_get_nulls_mode (GomOrdering      *self);
GOM_AVAILABLE_IN_ALL
GomOrdering      *gom_ordering_copy           (GomOrdering      *self) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
