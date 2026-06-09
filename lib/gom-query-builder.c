/* gom-query-builder.c
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

#include "config.h"

#include <gio/gio.h>

#include "gom-expression.h"
#include "gom-ordering.h"
#include "gom-query-builder.h"
#include "gom-query-private.h"

struct _GomQueryBuilder
{
  gatomicrefcount ref_count;

  GType          target_entity_type;
  char          *target_relation;
  GPtrArray     *projections;
  GomExpression *filter;
  GPtrArray     *groupings;
  GomExpression *group_filter;
  GPtrArray     *orderings;
  guint64        offset;
  guint64        limit;
  guint          has_offset : 1;
  guint          has_limit : 1;
};

G_DEFINE_BOXED_TYPE (GomQueryBuilder,
                     gom_query_builder,
                     gom_query_builder_ref,
                     gom_query_builder_unref)

static void
gom_query_builder_finalize (GomQueryBuilder *self)
{
  g_clear_pointer (&self->projections, g_ptr_array_unref);
  gom_clear_expression (&self->filter);
  g_clear_pointer (&self->groupings, g_ptr_array_unref);
  gom_clear_expression (&self->group_filter);
  g_clear_pointer (&self->orderings, g_ptr_array_unref);
  g_clear_pointer (&self->target_relation, g_free);
}

GomQueryBuilder *
gom_query_builder_new (void)
{
  GomQueryBuilder *self = g_new0 (GomQueryBuilder, 1);

  g_atomic_ref_count_init (&self->ref_count);
  self->target_entity_type = G_TYPE_INVALID;

  return self;
}

GomQueryBuilder *
gom_query_builder_ref (GomQueryBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

void
gom_query_builder_unref (GomQueryBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  gom_query_builder_finalize (self);
  g_free (self);
}

void
gom_query_builder_set_target_entity_type (GomQueryBuilder *self,
                                          GType            target_entity_type)
{
  g_return_if_fail (self != NULL);

  self->target_entity_type = target_entity_type;
}

void
gom_query_builder_set_target_relation (GomQueryBuilder *self,
                                       const char      *target_relation)
{
  g_return_if_fail (self != NULL);

  g_set_str (&self->target_relation, target_relation);
}

/**
 * gom_query_builder_add_projection:
 * @self: a [struct@Gom.QueryBuilder]
 * @projection: (transfer full): a [class@Gom.Expression]
 *
 * Adds @projection to the query builder.
 */
void
gom_query_builder_add_projection (GomQueryBuilder *self,
                                  GomExpression   *projection)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GOM_IS_EXPRESSION (projection));

  if (self->projections == NULL)
    self->projections = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  g_ptr_array_add (self->projections, g_steal_pointer (&projection));
}

void
gom_query_builder_clear_projections (GomQueryBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (self->projections != NULL && self->projections->len > 0)
    g_ptr_array_remove_range (self->projections, 0, self->projections->len);
}

/**
 * gom_query_builder_set_filter:
 * @self: a [struct@Gom.QueryBuilder]
 * @filter: (transfer none) (nullable): a [class@Gom.Expression]
 *
 * Sets the filter expression for the query builder.
 */
void
gom_query_builder_set_filter (GomQueryBuilder *self,
                              GomExpression   *filter)
{
  g_return_if_fail (self != NULL);

  gom_set_expression (&self->filter, filter);
}

/**
 * gom_query_builder_add_grouping:
 * @self: a [struct@Gom.QueryBuilder]
 * @grouping: (transfer full): a [class@Gom.Expression]
 *
 * Adds @grouping to the query builder.
 */
void
gom_query_builder_add_grouping (GomQueryBuilder *self,
                                GomExpression   *grouping)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GOM_IS_EXPRESSION (grouping));

  if (self->groupings == NULL)
    self->groupings = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  g_ptr_array_add (self->groupings, grouping);
}

void
gom_query_builder_clear_groupings (GomQueryBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (self->groupings != NULL && self->groupings->len > 0)
    g_ptr_array_remove_range (self->groupings, 0, self->groupings->len);
}

/**
 * gom_query_builder_set_group_filter:
 * @self: a [struct@Gom.QueryBuilder]
 * @group_filter: (transfer none) (nullable): a [class@Gom.Expression]
 *
 * Sets the group filter expression for the query builder.
 */
void
gom_query_builder_set_group_filter (GomQueryBuilder *self,
                                    GomExpression   *group_filter)
{
  g_return_if_fail (self != NULL);

  gom_set_expression (&self->group_filter, group_filter);
}

/**
 * gom_query_builder_add_ordering:
 * @self: a [struct@Gom.QueryBuilder]
 * @ordering: (transfer full): a [class@Gom.Ordering]
 *
 * Adds @ordering to the query builder.
 */
void
gom_query_builder_add_ordering (GomQueryBuilder *self,
                                GomOrdering     *ordering)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GOM_IS_ORDERING (ordering));

  if (self->orderings == NULL)
    self->orderings = g_ptr_array_new_with_free_func (g_object_unref);

  g_ptr_array_add (self->orderings, ordering);
}

void
gom_query_builder_clear_orderings (GomQueryBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (self->orderings != NULL && self->orderings->len > 0)
    g_ptr_array_remove_range (self->orderings, 0, self->orderings->len);
}

void
gom_query_builder_set_offset (GomQueryBuilder *self,
                              guint64          offset)
{
  g_return_if_fail (self != NULL);

  self->offset = offset;
  self->has_offset = TRUE;
}

void
gom_query_builder_set_limit (GomQueryBuilder *self,
                             guint64          limit)
{
  g_return_if_fail (self != NULL);

  self->limit = limit;
  self->has_limit = TRUE;
}

static GomQuery *
gom_query_builder_build_internal (GomQueryBuilder  *self,
                                  gboolean          with_count,
                                  GError          **error)
{
  if (self->target_entity_type == G_TYPE_INVALID && self->target_relation == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomQueryBuilder requires a target entity type or relation");
      return NULL;
    }

  return _gom_query_new (self->target_entity_type,
                         self->target_relation,
                         self->projections,
                         self->filter,
                         self->groupings,
                         self->group_filter,
                         self->orderings,
                         self->offset,
                         self->limit,
                         self->has_offset,
                         self->has_limit,
                         with_count);
}

/**
 * gom_query_builder_build:
 * @self: a [struct@Gom.QueryBuilder]
 * @error: a location for #GError
 *
 * Build a new query.
 *
 * Returns: (transfer full): a [class@Gom.Query] or %NULL and @error is set.
 */
GomQuery *
gom_query_builder_build (GomQueryBuilder  *self,
                         GError          **error)
{
  g_return_val_if_fail (self != NULL, NULL);

  return gom_query_builder_build_internal (self, FALSE, error);
}

/**
 * gom_query_builder_build_with_count:
 * @self: a [struct@Gom.QueryBuilder]
 * @error: a location for #GError
 *
 * Build a new query and request a total row count when supported.
 *
 * Returns: (transfer full): a [class@Gom.Query] or %NULL and @error is set.
 */
GomQuery *
gom_query_builder_build_with_count (GomQueryBuilder  *self,
                                    GError          **error)
{
  g_return_val_if_fail (self != NULL, NULL);

  return gom_query_builder_build_internal (self, TRUE, error);
}
