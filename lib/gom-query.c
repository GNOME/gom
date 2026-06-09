/* gom-query.c
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
#include <gobject/gvaluecollector.h>

#include "gom-entity.h"
#include "gom-expression.h"
#include "gom-meta-private.h"
#include "gom-ordering.h"
#include "gom-query-private.h"
#include "gom-repository.h"

struct _GomQuery
{
  GObject parent_instance;

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
  guint          with_count : 1;
};

G_DEFINE_FINAL_TYPE (GomQuery, gom_query, G_TYPE_OBJECT)

gboolean
_gom_query_validate_entity_list (GomQuery  *query,
                                 GError   **error)
{
  g_return_val_if_fail (GOM_IS_QUERY (query), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (_gom_query_get_target_relation (query) != NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Entity list queries do not support target relations");
      return FALSE;
    }

  if (_gom_query_get_projections (query) != NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Entity list queries do not support projections");
      return FALSE;
    }

  if (_gom_query_get_groupings (query) != NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Entity list queries do not support groupings");
      return FALSE;
    }

  if (_gom_query_get_group_filter (query) != NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Entity list queries do not support group filters");
      return FALSE;
    }

  if (_gom_query_get_target_entity_type (query) == G_TYPE_INVALID ||
      !g_type_is_a (_gom_query_get_target_entity_type (query), GOM_TYPE_ENTITY))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Entity list queries require a target entity type");
      return FALSE;
    }

  return TRUE;
}

static void
gom_query_finalize (GObject *object)
{
  GomQuery *self = (GomQuery *)object;

  g_clear_pointer (&self->projections, g_ptr_array_unref);
  gom_clear_expression (&self->filter);
  g_clear_pointer (&self->groupings, g_ptr_array_unref);
  gom_clear_expression (&self->group_filter);
  g_clear_pointer (&self->orderings, g_ptr_array_unref);
  g_clear_pointer (&self->target_relation, g_free);

  G_OBJECT_CLASS (gom_query_parent_class)->finalize (object);
}

static void
gom_query_class_init (GomQueryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_query_finalize;
}

static void
gom_query_init (GomQuery *self)
{
}

void
_gom_query_clear_collected_properties (guint    n_properties,
                                       char   **properties,
                                       GValue  *values)
{
  if (values != NULL)
    {
      for (guint i = 0; i < n_properties; i++)
        g_value_unset (&values[i]);
    }

  g_free (values);
  g_strfreev (properties);
}

gboolean
_gom_query_collect_properties (GomRegistry *registry,
                               GType        entity_type,
                               const char  *first_property,
                               va_list      args,
                               guint       *n_properties,
                               char      ***properties,
                               GValue     **values,
                               GError     **error)
{
  const GomEntitySpec *entity_spec;
  g_autoptr(GPtrArray) property_array = NULL;
  g_autoptr(GArray) value_array = NULL;
  const char *property_name = first_property;
  guint len = 0;

  g_return_val_if_fail (GOM_IS_REGISTRY (registry), FALSE);
  g_return_val_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY), FALSE);
  g_return_val_if_fail (first_property != NULL, FALSE);
  g_return_val_if_fail (n_properties != NULL, FALSE);
  g_return_val_if_fail (properties != NULL, FALSE);
  g_return_val_if_fail (values != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!(entity_spec = _gom_registry_lookup_entity_by_type (registry, entity_type)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "`%s` is not registered with the repository",
                   g_type_name (entity_type));
      return FALSE;
    }

  property_array = g_ptr_array_new_with_free_func (g_free);
  value_array = g_array_new (FALSE, FALSE, sizeof (GValue));
  g_array_set_clear_func (value_array, (GDestroyNotify) g_value_unset);

  while (property_name != NULL)
    {
      g_autofree char *collect_error = NULL;
      g_auto(GValue) value = G_VALUE_INIT;
      const GomPropertySpec *property_spec;
      GType value_type;

      property_spec = _gom_entity_spec_lookup_property_by_name ((GomEntitySpec *)entity_spec, property_name);

      if (property_spec == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "`%s` has no mapped property named `%s`",
                       g_type_name (entity_type),
                       property_name);
          return FALSE;
        }

      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "`%s` is not a mapped property on `%s`",
                       property_name,
                       g_type_name (entity_type));
          return FALSE;
        }

      value_type = gom_property_spec_get_value_type ((GomPropertySpec *)property_spec);

      G_VALUE_COLLECT_INIT (&value, value_type, args, 0, &collect_error);

      if (collect_error != NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Failed to collect `%s`: %s",
                       property_name,
                       collect_error);
          return FALSE;
        }

      g_ptr_array_add (property_array, g_strdup (property_name));

      g_array_append_val (value_array, value);
      memset (&value, 0, sizeof value);

      len++;

      property_name = va_arg (args, const char *);
    }

  g_ptr_array_add (property_array, NULL);

  *n_properties = len;
  *properties = (char **)g_ptr_array_free (g_steal_pointer (&property_array), FALSE);
  *values = (GValue *)g_array_free (g_steal_pointer (&value_array), FALSE);

  return TRUE;
}

GomQuery *
_gom_query_new_find_one (GomRegistry        *registry,
                         GType              entity_type,
                         guint              n_properties,
                         const char * const *properties,
                         const GValue      *values,
                         GError           **error)
{
  const GomEntitySpec *entity_spec;
  g_autoptr(GomExpression) filter = NULL;

  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY), NULL);
  g_return_val_if_fail (n_properties > 0, NULL);
  g_return_val_if_fail (properties != NULL, NULL);
  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!(entity_spec = _gom_registry_lookup_entity_by_type (registry, entity_type)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "`%s` is not registered with the repository",
                   g_type_name (entity_type));
      return NULL;
    }

  for (guint i = 0; i < n_properties; i++)
    {
      const GomPropertySpec *property_spec;
      g_autoptr(GomExpression) comparison = NULL;
      g_autoptr(GomExpression) left = NULL;
      g_autoptr(GomExpression) right = NULL;
      const char *field;

      if (properties[i] == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Property names must not be NULL");
          return NULL;
        }

      if (!G_IS_VALUE (&values[i]))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Value for `%s` is not initialized",
                       properties[i]);
          return NULL;
        }

      property_spec = _gom_entity_spec_lookup_property_by_name ((GomEntitySpec *)entity_spec, properties[i]);

      if (property_spec == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "`%s` has no mapped property named `%s`",
                       g_type_name (entity_type),
                       properties[i]);
          return NULL;
        }

      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "`%s` is not a mapped property on `%s`",
                       properties[i],
                       g_type_name (entity_type));
          return NULL;
        }

      field = gom_property_spec_get_field ((GomPropertySpec *)property_spec);
      left = gom_field_expression_new (field);
      right = gom_literal_expression_new (&values[i]);
      comparison = gom_binary_expression_new_equal (g_steal_pointer (&left),
                                                    g_steal_pointer (&right));

      if (filter == NULL)
        filter = g_steal_pointer (&comparison);
      else
        filter = gom_binary_expression_new_and (g_steal_pointer (&filter),
                                                g_steal_pointer (&comparison));
    }

  return _gom_query_new (entity_type,
                         NULL,
                         NULL,
                         filter,
                         NULL,
                         NULL,
                         NULL,
                         0,
                         1,
                         FALSE,
                         TRUE,
                         FALSE);
}

GomQuery *
_gom_query_new (GType          target_entity_type,
                const char    *target_relation,
                GPtrArray     *projections,
                GomExpression *filter,
                GPtrArray     *groupings,
                GomExpression *group_filter,
                GPtrArray     *orderings,
                guint64        offset,
                guint64        limit,
                gboolean       has_offset,
                gboolean       has_limit,
                gboolean       with_count)
{
  GomQuery *query = g_object_new (GOM_TYPE_QUERY, NULL);

  query->target_entity_type = target_entity_type;
  query->target_relation = g_strdup (target_relation);
  gom_set_expression (&query->filter, filter);
  gom_set_expression (&query->group_filter, group_filter);
  query->offset = offset;
  query->limit = limit;
  query->has_offset = has_offset;
  query->has_limit = has_limit;
  query->with_count = !!with_count;

  if (projections != NULL && projections->len > 0)
    {
      query->projections = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (guint i = 0; i < projections->len; i++)
        g_ptr_array_add (query->projections, g_object_ref (g_ptr_array_index (projections, i)));
    }

  if (groupings != NULL && groupings->len > 0)
    {
      query->groupings = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (guint i = 0; i < groupings->len; i++)
        g_ptr_array_add (query->groupings, g_object_ref (g_ptr_array_index (groupings, i)));
    }

  if (orderings != NULL && orderings->len > 0)
    {
      query->orderings = g_ptr_array_new_with_free_func (g_object_unref);

      for (guint i = 0; i < orderings->len; i++)
        g_ptr_array_add (query->orderings, gom_ordering_copy (g_ptr_array_index (orderings, i)));
    }

  return query;
}

GType
_gom_query_get_target_entity_type (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), G_TYPE_INVALID);

  return self->target_entity_type;
}

const char *
_gom_query_get_target_relation (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), NULL);

  return self->target_relation;
}

GPtrArray *
_gom_query_get_projections (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), NULL);

  return self->projections;
}

GomExpression *
_gom_query_get_filter (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), NULL);

  return self->filter;
}

GPtrArray *
_gom_query_get_groupings (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), NULL);

  return self->groupings;
}

GomExpression *
_gom_query_get_group_filter (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), NULL);

  return self->group_filter;
}

GPtrArray *
_gom_query_get_orderings (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), NULL);

  return self->orderings;
}

gboolean
_gom_query_has_offset (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), FALSE);

  return self->has_offset;
}

guint64
_gom_query_get_offset (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), 0);

  return self->offset;
}

gboolean
_gom_query_has_limit (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), FALSE);

  return self->has_limit;
}

guint64
_gom_query_get_limit (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), 0);

  return self->limit;
}

gboolean
_gom_query_get_with_count (GomQuery *self)
{
  g_return_val_if_fail (GOM_IS_QUERY (self), FALSE);

  return self->with_count;
}

GomQuery *
_gom_query_slice (GomQuery *query,
                  guint64   offset,
                  guint64   length)
{
  guint64 base_offset;
  guint64 new_offset;
  guint64 new_limit;
  gboolean has_offset;
  gboolean has_limit;

  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  base_offset = query->has_offset ? query->offset : 0;
  if (G_UNLIKELY (offset > G_MAXUINT64 - base_offset))
    new_offset = G_MAXUINT64;
  else
    new_offset = base_offset + offset;
  has_offset = query->has_offset || offset > 0;

  new_limit = length;
  has_limit = TRUE;

  if (query->has_limit)
    {
      if (offset >= query->limit)
        new_limit = 0;
      else
        {
          guint64 remaining = query->limit - offset;

          if (new_limit > remaining)
            new_limit = remaining;
        }
    }

  return _gom_query_new (query->target_entity_type,
                         query->target_relation,
                         query->projections,
                         query->filter,
                         query->groupings,
                         query->group_filter,
                         query->orderings,
                         new_offset,
                         new_limit,
                         has_offset,
                         has_limit,
                         query->with_count);
}
