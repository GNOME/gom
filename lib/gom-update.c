/* gom-update.c
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
#include "gom-update-private.h"
#include "gom-mutation-private.h"

struct _GomUpdate
{
  GomMutation    parent_instance;
  GType          target_entity_type;
  char          *target_relation;
  GPtrArray     *columns;
  GPtrArray     *values;
  GomExpression *filter;
  guint64        limit;
  guint          has_limit : 1;
};

struct _GomUpdateClass
{
  GomMutationClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomUpdate, gom_update, GOM_TYPE_MUTATION)

static void
gom_update_finalize (GObject *object)
{
  GomUpdate *self = (GomUpdate *)object;

  g_clear_pointer (&self->columns, g_ptr_array_unref);
  g_clear_pointer (&self->values, g_ptr_array_unref);
  g_clear_pointer (&self->target_relation, g_free);
  gom_clear_expression (&self->filter);

  G_OBJECT_CLASS (gom_update_parent_class)->finalize (object);
}

static void
gom_update_class_init (GomUpdateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_update_finalize;
}

static void
gom_update_init (GomUpdate *self)
{
  self->target_entity_type = G_TYPE_INVALID;
}

GomUpdate *
_gom_update_new (GType          target_entity_type,
                 const char    *target_relation,
                 GPtrArray     *columns,
                 GPtrArray     *values,
                 GomExpression *filter,
                 guint64        limit,
                 gboolean       has_limit)
{
  GomUpdate *update = g_object_new (GOM_TYPE_UPDATE, NULL);

  update->target_entity_type = target_entity_type;
  update->target_relation = g_strdup (target_relation);
  gom_set_expression (&update->filter, filter);
  update->limit = limit;
  update->has_limit = has_limit;

  if (columns != NULL && columns->len > 0)
    {
      update->columns = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (guint i = 0; i < columns->len; i++)
        g_ptr_array_add (update->columns, g_object_ref (g_ptr_array_index (columns, i)));
    }

  if (values != NULL && values->len > 0)
    {
      update->values = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (guint i = 0; i < values->len; i++)
        g_ptr_array_add (update->values, g_object_ref (g_ptr_array_index (values, i)));
    }

  return update;
}

GType
_gom_update_get_target_entity_type (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), G_TYPE_INVALID);

  return self->target_entity_type;
}

const char *
_gom_update_get_target_relation (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), NULL);

  return self->target_relation;
}

GPtrArray *
_gom_update_get_columns (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), NULL);

  return self->columns;
}

GPtrArray *
_gom_update_get_values (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), NULL);

  return self->values;
}

GomExpression *
_gom_update_get_filter (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), NULL);

  return self->filter;
}

gboolean
_gom_update_has_limit (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), FALSE);

  return self->has_limit;
}

guint64
_gom_update_get_limit (GomUpdate *self)
{
  g_return_val_if_fail (GOM_IS_UPDATE (self), 0);

  return self->limit;
}
