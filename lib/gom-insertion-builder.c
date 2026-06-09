/* gom-insertion-builder.c
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

#include "gom-entity.h"
#include "gom-expression.h"
#include "gom-entity-private.h"
#include "gom-expression-private.h"
#include "gom-insertion-builder.h"
#include "gom-repository-private.h"
#include "gom-insertion-private.h"
#include "gom-meta-private.h"
#include "gom-util-private.h"

struct _GomInsertionBuilder
{
  gatomicrefcount  ref_count;
  GomRepository   *repository;
  GType            target_entity_type;
  char            *target_relation;
  GPtrArray       *columns;
  GPtrArray       *rows;
};

G_DEFINE_BOXED_TYPE (GomInsertionBuilder,
                     gom_insertion_builder,
                     gom_insertion_builder_ref,
                     gom_insertion_builder_unref)

static void
gom_insertion_builder_row_free (gpointer data)
{
  GPtrArray *row = data;

  if (row != NULL)
    g_ptr_array_unref (row);
}

static void
gom_insertion_builder_finalize (GomInsertionBuilder *self)
{
  g_clear_pointer (&self->columns, g_ptr_array_unref);
  g_clear_pointer (&self->rows, g_ptr_array_unref);
  g_clear_pointer (&self->target_relation, g_free);
  g_clear_object (&self->repository);
}

GomInsertionBuilder *
gom_insertion_builder_new (GomRepository *repository)
{
  GomInsertionBuilder *self = g_new0 (GomInsertionBuilder, 1);

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);

  g_atomic_ref_count_init (&self->ref_count);
  self->repository = g_object_ref (repository);
  self->target_entity_type = G_TYPE_INVALID;

  return self;
}

GomInsertionBuilder *
gom_insertion_builder_ref (GomInsertionBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

void
gom_insertion_builder_unref (GomInsertionBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  gom_insertion_builder_finalize (self);
  g_free (self);
}

void
gom_insertion_builder_set_target_entity_type (GomInsertionBuilder *self,
                                              GType                target_entity_type)
{
  g_return_if_fail (self != NULL);

  self->target_entity_type = target_entity_type;
}

void
gom_insertion_builder_set_target_relation (GomInsertionBuilder *self,
                                           const char          *target_relation)
{
  g_return_if_fail (self != NULL);

  g_free (self->target_relation);
  self->target_relation = g_strdup (target_relation);
}

void
gom_insertion_builder_add_column (GomInsertionBuilder *self,
                                  GomExpression       *column)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (GOM_IS_EXPRESSION (column));

  if (self->columns == NULL)
    self->columns = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  g_ptr_array_add (self->columns, column);
}

void
gom_insertion_builder_clear_columns (GomInsertionBuilder *self)
{
  g_return_if_fail (self != NULL);

  g_clear_pointer (&self->columns, g_ptr_array_unref);
}

static void
_gom_insertion_builder_add_row (GomInsertionBuilder *self,
                                GPtrArray           *row)
{
  g_assert (self != NULL);
  g_assert (row != NULL);

  if (self->rows == NULL)
    self->rows = g_ptr_array_new_with_free_func (gom_insertion_builder_row_free);

  g_ptr_array_add (self->rows, g_steal_pointer (&row));
}

void
gom_insertion_builder_add_row (GomInsertionBuilder  *self,
                               GomExpression       **values,
                               gsize                 n_values)
{
  GPtrArray *row;

  g_return_if_fail (self != NULL);
  g_return_if_fail (n_values == 0 || values != NULL);

  row = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

  for (gsize i = 0; i < n_values; i++)
    g_ptr_array_add (row, values[i]);

  _gom_insertion_builder_add_row (self, g_steal_pointer (&row));
}

gboolean
gom_insertion_builder_add_entity (GomInsertionBuilder  *self,
                                  GomEntity            *entity,
                                  GError              **error)
{
  GObjectClass *object_class;
  GomEntityClass *entity_class;
  const GomEntitySpec *entity_spec;
  const GomPropertySpec * const *properties = NULL;
  const GomPropertySpec *property_spec;
  const char * const *identity_fields;
  guint n_properties = 0;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (GOM_IS_ENTITY (entity), FALSE);

  if (self->target_entity_type == G_TYPE_INVALID)
    self->target_entity_type = G_OBJECT_TYPE (entity);
  else
    g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (entity), self->target_entity_type), FALSE);

  object_class = G_OBJECT_GET_CLASS (entity);
  entity_class = GOM_ENTITY_CLASS (object_class);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  gom_entity_set_repository (entity, self->repository);

  if (!(entity_spec = _gom_registry_lookup_entity_by_type (_gom_repository_get_registry (self->repository),
                                                           self->target_entity_type)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type `%s` is not registered",
                   g_type_name (self->target_entity_type));
      return FALSE;
    }

  properties = gom_entity_spec_list_properties ((GomEntitySpec *)entity_spec, &n_properties);

  if (self->columns == NULL || self->columns->len == 0)
    {
      g_autoptr(GPtrArray) row = NULL;

      row = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (guint i = 0; i < n_properties; i++)
        {
          const char *property_name;
          const char *field_name;
          g_auto(GValue) value = G_VALUE_INIT;
          gboolean is_identity;

          property_spec = properties[i];
          if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
            continue;

          property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec);
          field_name = gom_property_spec_get_field ((GomPropertySpec *)property_spec);
          if (field_name == NULL)
            field_name = property_name;

          is_identity = _gom_strv_contains (identity_fields, property_name);

          if (is_identity)
            {
              g_autoptr(GomExpression) identity_value = NULL;
              g_autoptr(GError) local_error = NULL;

              if (!gom_entity_dup_identity_value_is_set (entity, entity_class, property_name, &identity_value, &local_error))
                {
                  if (local_error != NULL)
                    {
                      g_propagate_error (error, g_steal_pointer (&local_error));
                      return FALSE;
                    }

                  continue;
                }

              gom_insertion_builder_add_column (self, gom_field_expression_new (field_name));
              g_ptr_array_add (row, g_steal_pointer (&identity_value));
              continue;
            }

          if (!gom_entity_get_property_storage_value (entity, entity_class, object_class, property_name, &value, error))
            return FALSE;

          gom_insertion_builder_add_column (self, gom_field_expression_new (field_name));
          g_ptr_array_add (row, gom_literal_expression_new (&value));
        }

      if (row->len == 0)
        return FALSE;

      if (self->rows == NULL)
        self->rows = g_ptr_array_new_with_free_func (gom_insertion_builder_row_free);

      g_ptr_array_add (self->rows, g_steal_pointer (&row));
      return TRUE;
    }

  {
    g_autoptr(GPtrArray) row = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

    for (guint i = 0; i < self->columns->len; i++)
      {
        GomExpression *column = g_ptr_array_index (self->columns, i);
        g_auto(GValue) value = G_VALUE_INIT;
        const char *field_name;
        const char *property_name;

        g_return_val_if_fail (GOM_IS_FIELD_EXPRESSION (column), FALSE);

        field_name = _gom_field_expression_get_field (GOM_FIELD_EXPRESSION (column));
        g_return_val_if_fail (field_name != NULL, FALSE);

        if (!(property_spec = _gom_entity_spec_lookup_property_by_field ((GomEntitySpec *)entity_spec, field_name)) &&
            !(property_spec = _gom_entity_spec_lookup_property_by_name ((GomEntitySpec *)entity_spec, field_name)))
          {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         "Entity type `%s` does not have field `%s`",
                         G_OBJECT_TYPE_NAME (entity),
                         field_name);
            return FALSE;
          }

        property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec);

        if (!gom_entity_get_property_storage_value (entity, entity_class, object_class, property_name, &value, error))
          return FALSE;

        g_ptr_array_add (row, gom_literal_expression_new (&value));
      }

    _gom_insertion_builder_add_row (self, g_steal_pointer (&row));
  }

  return TRUE;
}

gboolean
gom_insertion_builder_add_entity_list (GomInsertionBuilder  *self,
                                       GListModel           *entities,
                                       GError              **error)
{
  guint n_items;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (G_IS_LIST_MODEL (entities), FALSE);

  n_items = g_list_model_get_n_items (entities);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) item = g_list_model_get_item (entities, i);

      g_return_val_if_fail (GOM_IS_ENTITY (item), FALSE);

      if (!gom_insertion_builder_add_entity (self, GOM_ENTITY (item), error))
        return FALSE;
    }

  return TRUE;
}

/**
 * gom_insertion_builder_build:
 * @self: a [struct@Gom.InsertionBuilder]
 *
 * Returns: (transfer full):
 */
GomInsertion *
gom_insertion_builder_build (GomInsertionBuilder  *self,
                             GError              **error)
{
  g_return_val_if_fail (self != NULL, NULL);

  if (self->target_entity_type == G_TYPE_INVALID && self->target_relation == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomInsertionBuilder requires a target entity type or relation");
      return NULL;
    }

  if (self->columns == NULL || self->columns->len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomInsertionBuilder requires at least one column");
      return NULL;
    }

  if (self->rows == NULL || self->rows->len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomInsertionBuilder requires at least one row");
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
                       "GomInsertionBuilder columns must be field expressions");
          return NULL;
        }
    }

  for (guint i = 0; i < self->rows->len; i++)
    {
      GPtrArray *row = g_ptr_array_index (self->rows, i);

      if (row->len != self->columns->len)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "GomInsertionBuilder row length does not match columns");
          return NULL;
        }
    }

  return _gom_insertion_new (self->target_entity_type,
                             self->target_relation,
                             self->columns,
                             self->rows);
}
