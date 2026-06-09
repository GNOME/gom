/* gom-insertion.c
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

#include "gom-expression.h"
#include "gom-insertion-private.h"
#include "gom-mutation-private.h"

struct _GomInsertion
{
  GomMutation  parent_instance;
  GType        target_entity_type;
  char        *target_relation;
  GPtrArray   *columns;
  GPtrArray   *rows;
};

struct _GomInsertionClass
{
  GomMutationClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomInsertion, gom_insertion, GOM_TYPE_MUTATION)

static void
gom_insertion_finalize (GObject *object)
{
  GomInsertion *self = (GomInsertion *)object;

  g_clear_pointer (&self->columns, g_ptr_array_unref);
  g_clear_pointer (&self->rows, g_ptr_array_unref);
  g_clear_pointer (&self->target_relation, g_free);

  G_OBJECT_CLASS (gom_insertion_parent_class)->finalize (object);
}

static void
gom_insertion_class_init (GomInsertionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_insertion_finalize;
}

static void
gom_insertion_init (GomInsertion *self)
{
  self->target_entity_type = G_TYPE_INVALID;
}

static GPtrArray *
gom_insertion_dup_row (GPtrArray *row)
{
  GPtrArray *copy;

  if (row == NULL || row->len == 0)
    return g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  copy = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  for (guint i = 0; i < row->len; i++)
    g_ptr_array_add (copy, g_object_ref (g_ptr_array_index (row, i)));

  return copy;
}

GomInsertion *
_gom_insertion_new (GType       target_entity_type,
                    const char *target_relation,
                    GPtrArray  *columns,
                    GPtrArray  *rows)
{
  GomInsertion *insertion = g_object_new (GOM_TYPE_INSERTION, NULL);

  insertion->target_entity_type = target_entity_type;
  insertion->target_relation = g_strdup (target_relation);

  if (columns != NULL && columns->len > 0)
    {
      insertion->columns = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (guint i = 0; i < columns->len; i++)
        g_ptr_array_add (insertion->columns, g_object_ref (g_ptr_array_index (columns, i)));
    }

  if (rows != NULL && rows->len > 0)
    {
      insertion->rows = g_ptr_array_new_with_free_func ((GDestroyNotify)g_ptr_array_unref);

      for (guint i = 0; i < rows->len; i++)
        g_ptr_array_add (insertion->rows, gom_insertion_dup_row (g_ptr_array_index (rows, i)));
    }

  return insertion;
}

GType
_gom_insertion_get_target_entity_type (GomInsertion *self)
{
  g_return_val_if_fail (GOM_IS_INSERTION (self), G_TYPE_INVALID);

  return self->target_entity_type;
}

const char *
_gom_insertion_get_target_relation (GomInsertion *self)
{
  g_return_val_if_fail (GOM_IS_INSERTION (self), NULL);

  return self->target_relation;
}

GPtrArray *
_gom_insertion_get_columns (GomInsertion *self)
{
  g_return_val_if_fail (GOM_IS_INSERTION (self), NULL);

  return self->columns;
}

GPtrArray *
_gom_insertion_get_rows (GomInsertion *self)
{
  g_return_val_if_fail (GOM_IS_INSERTION (self), NULL);

  return self->rows;
}
