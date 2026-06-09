/* gom-query-private.h
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

#include <stdarg.h>

#include "gom-query.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

GomQuery      *_gom_query_new                        (GType                 target_entity_type,
                                                      const char           *target_relation,
                                                      GPtrArray            *projections,
                                                      GomExpression        *filter,
                                                      GPtrArray            *groupings,
                                                      GomExpression        *group_filter,
                                                      GPtrArray            *orderings,
                                                      guint64               offset,
                                                      guint64               limit,
                                                      gboolean              has_offset,
                                                      gboolean              has_limit,
                                                      gboolean              with_count);
GomQuery      *_gom_query_new_find_one               (GomRegistry          *registry,
                                                      GType                 entity_type,
                                                      guint                 n_properties,
                                                      const char * const   *properties,
                                                      const GValue         *values,
                                                      GError              **error);
GomQuery      *_gom_query_slice                      (GomQuery             *query,
                                                      guint64               offset,
                                                      guint64               length);
gboolean       _gom_query_collect_properties         (GomRegistry          *registry,
                                                      GType                 entity_type,
                                                      const char           *first_property,
                                                      va_list               args,
                                                      guint                *n_properties,
                                                      char               ***properties,
                                                      GValue              **values,
                                                      GError              **error);
void           _gom_query_clear_collected_properties (guint                 n_properties,
                                                      char                **properties,
                                                      GValue               *values);
gboolean       _gom_query_validate_entity_list       (GomQuery             *query,
                                                      GError              **error);
GType          _gom_query_get_target_entity_type     (GomQuery             *self);
const char    *_gom_query_get_target_relation        (GomQuery             *self);
GPtrArray     *_gom_query_get_projections            (GomQuery             *self);
GomExpression *_gom_query_get_filter                 (GomQuery             *self);
GPtrArray     *_gom_query_get_groupings              (GomQuery             *self);
GomExpression *_gom_query_get_group_filter           (GomQuery             *self);
GPtrArray     *_gom_query_get_orderings              (GomQuery             *self);
gboolean       _gom_query_has_offset                 (GomQuery             *self);
guint64        _gom_query_get_offset                 (GomQuery             *self);
gboolean       _gom_query_has_limit                  (GomQuery             *self);
guint64        _gom_query_get_limit                  (GomQuery             *self);
gboolean       _gom_query_get_with_count             (GomQuery             *self);

G_END_DECLS
