/* gom-query-builder.h
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

#define GOM_TYPE_QUERY_BUILDER (gom_query_builder_get_type())

GOM_AVAILABLE_IN_ALL
GType            gom_query_builder_get_type               (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GomQueryBuilder *gom_query_builder_new                    (void);
GOM_AVAILABLE_IN_ALL
GomQueryBuilder *gom_query_builder_ref                    (GomQueryBuilder  *self);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_unref                  (GomQueryBuilder  *self);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_set_target_entity_type (GomQueryBuilder  *self,
                                                           GType             target_entity_type);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_set_target_relation    (GomQueryBuilder  *self,
                                                           const char       *target_relation);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_add_projection         (GomQueryBuilder  *self,
                                                           GomExpression    *projection);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_clear_projections      (GomQueryBuilder  *self);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_set_filter             (GomQueryBuilder  *self,
                                                           GomExpression    *filter);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_add_grouping           (GomQueryBuilder  *self,
                                                           GomExpression    *grouping);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_clear_groupings        (GomQueryBuilder  *self);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_set_group_filter       (GomQueryBuilder  *self,
                                                           GomExpression    *group_filter);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_add_ordering           (GomQueryBuilder  *self,
                                                           GomOrdering      *ordering);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_clear_orderings        (GomQueryBuilder  *self);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_set_offset             (GomQueryBuilder  *self,
                                                           guint64           offset);
GOM_AVAILABLE_IN_ALL
void             gom_query_builder_set_limit              (GomQueryBuilder  *self,
                                                           guint64           limit);
GOM_AVAILABLE_IN_ALL
GomQuery        *gom_query_builder_build                  (GomQueryBuilder  *self,
                                                           GError          **error);
GOM_AVAILABLE_IN_ALL
GomQuery        *gom_query_builder_build_with_count       (GomQueryBuilder  *self,
                                                           GError          **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomQueryBuilder, gom_query_builder_unref)

G_END_DECLS
