/* gom-update-builder.c
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
#include "gom-update-builder.h"
#include "gom-update-private.h"

struct _GomUpdateBuilder
{
  gatomicrefcount  ref_count;
  GType            target_entity_type;
  char            *target_relation;
  GPtrArray       *columns;
  GPtrArray       *values;
  GomExpression   *filter;
  guint64          limit;
  guint            has_limit : 1;
};

G_DEFINE_BOXED_TYPE (GomUpdateBuilder,
                     gom_update_builder,
                     gom_update_builder_ref,
                     gom_update_builder_unref)

static void
gom_update_builder_finalize (GomUpdateBuilder *self)
{
  g_clear_pointer (&self->columns, g_ptr_array_unref);
  g_clear_pointer (&self->values, g_ptr_array_unref);
  g_clear_pointer (&self->target_relation, g_free);
  gom_clear_expression (&self->filter);
}

GomUpdateBuilder *
gom_update_builder_new (void)
{
  GomUpdateBuilder *self = g_new0 (GomUpdateBuilder, 1);

  g_atomic_ref_count_init (&self->ref_count);
  self->target_entity_type = G_TYPE_INVALID;

  return self;
}

GomUpdateBuilder *
gom_update_builder_ref (GomUpdateBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

void
gom_update_builder_unref (GomUpdateBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  gom_update_builder_finalize (self);
  g_free (self);
}

void
gom_update_builder_set_target_entity_type (GomUpdateBuilder *self,
                                           GType             target_entity_type)
{
  g_return_if_fail (self != NULL);

  self->target_entity_type = target_entity_type;
}

void
gom_update_builder_set_target_relation (GomUpdateBuilder *self,
                                        const char       *target_relation)
{
  g_return_if_fail (self != NULL);

  g_set_str (&self->target_relation, target_relation);
}

void
gom_update_builder_add_assignment (GomUpdateBuilder *self,
                                   GomExpression    *column,
                                   GomExpression    *value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GOM_IS_EXPRESSION (column));
  g_return_if_fail (GOM_IS_EXPRESSION (value));

  if (self->columns == NULL)
    self->columns = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  if (self->values == NULL)
    self->values = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  g_ptr_array_add (self->columns, column);
  g_ptr_array_add (self->values, value);
}

void
gom_update_builder_clear_assignments (GomUpdateBuilder *self)
{
  g_return_if_fail (self != NULL);

  g_clear_pointer (&self->columns, g_ptr_array_unref);
  g_clear_pointer (&self->values, g_ptr_array_unref);
}

void
gom_update_builder_set_filter (GomUpdateBuilder *self,
                               GomExpression    *filter)
{
  g_return_if_fail (self != NULL);

  gom_set_expression (&self->filter, filter);
}

void
gom_update_builder_set_limit (GomUpdateBuilder *self,
                              guint64           limit)
{
  g_return_if_fail (self != NULL);

  self->limit = limit;
  self->has_limit = TRUE;
}

/**
 * gom_update_builder_build:
 * @self: a [struct@Gom.UpdateBuilder]
 * @error: return location for a [struct@GLib.Error], or %NULL
 *
 * Builds a [class@Gom.Update] from the builder state.
 *
 * At least one target (entity type or relation) and one assignment are
 * required. Assignment targets must be field expressions.
 *
 * Returns: (transfer full): a [class@Gom.Update], or %NULL with @error set.
 */
GomUpdate *
gom_update_builder_build (GomUpdateBuilder  *self,
                          GError           **error)
{
  g_return_val_if_fail (self != NULL, NULL);

  if (self->target_entity_type == G_TYPE_INVALID && self->target_relation == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomUpdateBuilder requires a target entity type or relation");
      return NULL;
    }

  if (self->columns == NULL || self->columns->len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomUpdateBuilder requires at least one assignment");
      return NULL;
    }

  if (self->values == NULL || self->values->len != self->columns->len)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomUpdateBuilder assignments are malformed");
      return NULL;
    }

  for (guint i = 0; i < self->columns->len; i++)
    {
      GomExpression *column = g_ptr_array_index (self->columns, i);

      if (!GOM_IS_FIELD_EXPRESSION (column))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "GomUpdateBuilder assignment targets must be field expressions");
          return NULL;
        }
    }

  return _gom_update_new (self->target_entity_type,
                          self->target_relation,
                          self->columns,
                          self->values,
                          self->filter,
                          self->limit,
                          self->has_limit);
}
