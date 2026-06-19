/* gom-registry-diff.c
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

#include <string.h>

#include "gom-meta.h"
#include "gom-registry-diff-private.h"
#include "gom-util-private.h"

struct _GomPropertyDiff
{
  GomPropertySpec *current_property;
  GomPropertySpec *next_property;
};

struct _GomIndexDiff
{
  GomIndexSpec *current_index;
  GomIndexSpec *next_index;
};

struct _GomRelationshipDiff
{
  GomRelationshipSpec *current_relationship;
  GomRelationshipSpec *next_relationship;
};

typedef struct _GomRelationshipDiff GomRelationshipDiff;

struct _GomEntityDiff
{
  GomEntitySpec *current_entity;
  GomEntitySpec *next_entity;
  GPtrArray     *dropped_properties;
  GPtrArray     *added_properties;
  GPtrArray     *changed_properties;
  GPtrArray     *dropped_indexes;
  GPtrArray     *added_indexes;
  GPtrArray     *changed_indexes;
  GPtrArray     *dropped_relationships;
  GPtrArray     *added_relationships;
  GPtrArray     *changed_relationships;
  guint          identity_changed : 1;
};

struct _GomRegistryDiff
{
  GPtrArray *dropped_entities;
  GPtrArray *added_entities;
  GPtrArray *changed_entities;
};

static void
gom_property_diff_free (gpointer data)
{
  GomPropertyDiff *self = data;

  g_clear_object (&self->current_property);
  g_clear_object (&self->next_property);
  g_free (self);
}

static void
gom_index_diff_free (gpointer data)
{
  GomIndexDiff *self = data;

  g_clear_object (&self->current_index);
  g_clear_object (&self->next_index);
  g_free (self);
}

static void
gom_relationship_diff_free (gpointer data)
{
  GomRelationshipDiff *self = data;

  g_clear_object (&self->current_relationship);
  g_clear_object (&self->next_relationship);
  g_free (self);
}

static void
gom_entity_diff_free (gpointer data)
{
  GomEntityDiff *self = data;

  g_clear_object (&self->current_entity);
  g_clear_object (&self->next_entity);
  g_clear_pointer (&self->dropped_properties, g_ptr_array_unref);
  g_clear_pointer (&self->added_properties, g_ptr_array_unref);
  g_clear_pointer (&self->changed_properties, g_ptr_array_unref);
  g_clear_pointer (&self->dropped_indexes, g_ptr_array_unref);
  g_clear_pointer (&self->added_indexes, g_ptr_array_unref);
  g_clear_pointer (&self->changed_indexes, g_ptr_array_unref);
  g_clear_pointer (&self->dropped_relationships, g_ptr_array_unref);
  g_clear_pointer (&self->added_relationships, g_ptr_array_unref);
  g_clear_pointer (&self->changed_relationships, g_ptr_array_unref);
  g_free (self);
}

void
_gom_registry_diff_free (GomRegistryDiff *self)
{
  if (self == NULL)
    return;

  g_clear_pointer (&self->dropped_entities, g_ptr_array_unref);
  g_clear_pointer (&self->added_entities, g_ptr_array_unref);
  g_clear_pointer (&self->changed_entities, g_ptr_array_unref);
  g_free (self);
}

static const char *
gom_registry_diff_property_key (GomPropertySpec *property)
{
  const char *field;
  const char *name;

  g_assert (GOM_IS_PROPERTY_SPEC (property));

  field = gom_property_spec_get_field (property);
  if (!gom_str_empty0 (field))
    return field;

  name = gom_property_spec_get_name (property);
  return name != NULL ? name : "";
}

static gboolean
gom_registry_diff_property_equals (GomPropertySpec *a,
                                   GomPropertySpec *b)
{
  g_assert (GOM_IS_PROPERTY_SPEC (a));
  g_assert (GOM_IS_PROPERTY_SPEC (b));

  return g_strcmp0 (gom_property_spec_get_name (a), gom_property_spec_get_name (b)) == 0 &&
         g_strcmp0 (gom_property_spec_get_field (a), gom_property_spec_get_field (b)) == 0 &&
         g_strcmp0 (gom_property_spec_get_reference_table (a), gom_property_spec_get_reference_table (b)) == 0 &&
         g_strcmp0 (gom_property_spec_get_reference_field (a), gom_property_spec_get_reference_field (b)) == 0 &&
         gom_property_spec_get_value_type (a) == gom_property_spec_get_value_type (b) &&
         gom_property_spec_get_nonnull (a) == gom_property_spec_get_nonnull (b) &&
         gom_property_spec_get_unique (a) == gom_property_spec_get_unique (b) &&
         gom_property_spec_get_mapped (a) == gom_property_spec_get_mapped (b) &&
         gom_property_spec_get_search_flags (a) == gom_property_spec_get_search_flags (b);
}

static char *
gom_registry_diff_index_key (GomIndexSpec *index)
{
  const char *name;
  const char * const *fields;
  g_autoptr(GString) sig = NULL;

  g_assert (GOM_IS_INDEX_SPEC (index));

  name = gom_index_spec_get_name (index);
  if (!gom_str_empty0 (name))
    return g_strdup_printf ("name:%s", name);

  sig = g_string_new ("fields:");
  if ((fields = gom_index_spec_get_fields (index)))
    {
      for (guint i = 0; fields[i] != NULL; i++)
        {
          if (i > 0)
            g_string_append_c (sig, ',');
          g_string_append (sig, fields[i]);
        }
    }

  g_string_append_printf (sig,
                          "|unique:%u|search:%u",
                          gom_index_spec_get_unique (index),
                          gom_index_spec_get_search_flags (index));

  return g_string_free (g_steal_pointer (&sig), FALSE);
}

static gboolean
gom_registry_diff_index_equals (GomIndexSpec *a,
                                GomIndexSpec *b)
{
  const char * const *fields_a;
  const char * const *fields_b;

  g_assert (GOM_IS_INDEX_SPEC (a));
  g_assert (GOM_IS_INDEX_SPEC (b));

  if (g_strcmp0 (gom_index_spec_get_name (a), gom_index_spec_get_name (b)) != 0 ||
      gom_index_spec_get_unique (a) != gom_index_spec_get_unique (b) ||
      gom_index_spec_get_search_flags (a) != gom_index_spec_get_search_flags (b))
    return FALSE;

  fields_a = gom_index_spec_get_fields (a);
  fields_b = gom_index_spec_get_fields (b);

  if (fields_a == NULL || fields_b == NULL)
    return fields_a == fields_b;

  for (guint i = 0; ; i++)
    {
      if (fields_a[i] == NULL || fields_b[i] == NULL)
        return fields_a[i] == fields_b[i];
      if (g_strcmp0 (fields_a[i], fields_b[i]) != 0)
        return FALSE;
    }
}

static gboolean
gom_registry_diff_relationship_equals (GomRelationshipSpec *a,
                                       GomRelationshipSpec *b)
{
  g_assert (GOM_IS_RELATIONSHIP_SPEC (a));
  g_assert (GOM_IS_RELATIONSHIP_SPEC (b));

  return g_strcmp0 (gom_relationship_spec_get_name (a), gom_relationship_spec_get_name (b)) == 0 &&
         gom_relationship_spec_get_target_type (a) == gom_relationship_spec_get_target_type (b) &&
         gom_relationship_spec_get_cardinality (a) == gom_relationship_spec_get_cardinality (b) &&
         gom_relationship_spec_get_storage (a) == gom_relationship_spec_get_storage (b) &&
         g_strcmp0 (gom_relationship_spec_get_inverse_name (a), gom_relationship_spec_get_inverse_name (b)) == 0 &&
         _gom_strv_equal (gom_relationship_spec_get_local_fields (a), gom_relationship_spec_get_local_fields (b)) &&
         _gom_strv_equal (gom_relationship_spec_get_remote_fields (a), gom_relationship_spec_get_remote_fields (b)) &&
         g_strcmp0 (gom_relationship_spec_get_join_relation (a), gom_relationship_spec_get_join_relation (b)) == 0 &&
         _gom_strv_equal (gom_relationship_spec_get_join_local_fields (a), gom_relationship_spec_get_join_local_fields (b)) &&
         _gom_strv_equal (gom_relationship_spec_get_join_remote_fields (a), gom_relationship_spec_get_join_remote_fields (b)) &&
         gom_relationship_spec_get_optional (a) == gom_relationship_spec_get_optional (b) &&
         gom_relationship_spec_get_ordered (a) == gom_relationship_spec_get_ordered (b) &&
         gom_relationship_spec_get_min_count (a) == gom_relationship_spec_get_min_count (b) &&
         gom_relationship_spec_get_max_count (a) == gom_relationship_spec_get_max_count (b) &&
         gom_relationship_spec_get_delete_rule (a) == gom_relationship_spec_get_delete_rule (b) &&
         gom_relationship_spec_get_version_added (a) == gom_relationship_spec_get_version_added (b) &&
         gom_relationship_spec_get_version_removed (a) == gom_relationship_spec_get_version_removed (b);
}

static GHashTable *
gom_registry_diff_build_entity_table_map (GomRegistry *registry)
{
  const GomEntitySpec * const *entities;
  g_autoptr(GHashTable) by_table = NULL;
  guint n_entities = 0;

  g_assert (GOM_IS_REGISTRY (registry));

  by_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  entities = gom_registry_list_entities (registry, &n_entities);

  for (guint i = 0; i < n_entities; i++)
    {
      GomEntitySpec *entity = (GomEntitySpec *)entities[i];
      const char *table = gom_entity_spec_get_table (entity);

      if (gom_entity_spec_get_schema_role (entity) != GOM_ENTITY_SCHEMA_ROLE_PRIMARY)
        continue;

      if (table == NULL || *table == '\0')
        continue;

      g_hash_table_insert (by_table, g_strdup (table), g_object_ref (entity));
    }

  return g_steal_pointer (&by_table);
}

static GomEntityDiff *
gom_registry_diff_entity_new (GomEntitySpec *current_entity,
                              GomEntitySpec *next_entity)
{
  GomEntityDiff *self;

  g_assert (GOM_IS_ENTITY_SPEC (current_entity));
  g_assert (GOM_IS_ENTITY_SPEC (next_entity));

  self = g_new0 (GomEntityDiff, 1);
  self->current_entity = g_object_ref (current_entity);
  self->next_entity = g_object_ref (next_entity);
  self->dropped_properties = g_ptr_array_new_with_free_func (g_object_unref);
  self->added_properties = g_ptr_array_new_with_free_func (g_object_unref);
  self->changed_properties = g_ptr_array_new_with_free_func (gom_property_diff_free);
  self->dropped_indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->added_indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->changed_indexes = g_ptr_array_new_with_free_func (gom_index_diff_free);
  self->dropped_relationships = g_ptr_array_new_with_free_func (g_object_unref);
  self->added_relationships = g_ptr_array_new_with_free_func (g_object_unref);
  self->changed_relationships = g_ptr_array_new_with_free_func (gom_relationship_diff_free);
  self->identity_changed = !g_strv_equal (gom_entity_spec_get_identity_fields (current_entity),
                                          gom_entity_spec_get_identity_fields (next_entity));

  return self;
}

static void
gom_registry_diff_populate_property_changes (GomEntityDiff *entity_diff)
{
  const GomPropertySpec * const *current_props;
  const GomPropertySpec * const *next_props;
  g_autoptr(GHashTable) current_map = NULL;
  g_autoptr(GHashTable) next_map = NULL;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  guint n_current = 0;
  guint n_next = 0;

  current_map = g_hash_table_new (g_str_hash, g_str_equal);
  next_map = g_hash_table_new (g_str_hash, g_str_equal);
  current_props = gom_entity_spec_list_properties (entity_diff->current_entity, &n_current);
  next_props = gom_entity_spec_list_properties (entity_diff->next_entity, &n_next);

  for (guint i = 0; i < n_current; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)current_props[i];
      const char *property_key = gom_registry_diff_property_key (property);

      g_hash_table_insert (current_map, (gpointer)property_key, property);
    }

  for (guint i = 0; i < n_next; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)next_props[i];
      const char *property_key = gom_registry_diff_property_key (property);

      g_hash_table_insert (next_map, (gpointer)property_key, property);
    }

  g_hash_table_iter_init (&iter, current_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomPropertySpec *current_property = value;
      GomPropertySpec *next_property = g_hash_table_lookup (next_map, key);

      if (next_property == NULL)
        {
          g_ptr_array_add (entity_diff->dropped_properties, g_object_ref (current_property));
        }
      else if (!gom_registry_diff_property_equals (current_property, next_property))
        {
          GomPropertyDiff *property_diff = g_new0 (GomPropertyDiff, 1);

          property_diff->current_property = g_object_ref (current_property);
          property_diff->next_property = g_object_ref (next_property);

          g_ptr_array_add (entity_diff->changed_properties, property_diff);
        }
    }

  g_hash_table_iter_init (&iter, next_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomPropertySpec *next_property = value;

      if (!g_hash_table_contains (current_map, key))
        g_ptr_array_add (entity_diff->added_properties, g_object_ref (next_property));
    }
}

static void
gom_registry_diff_populate_index_changes (GomEntityDiff *entity_diff)
{
  const GomIndexSpec * const *current_indexes;
  const GomIndexSpec * const *next_indexes;
  g_autoptr(GHashTable) current_map = NULL;
  g_autoptr(GHashTable) next_map = NULL;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  guint n_current = 0;
  guint n_next = 0;

  current_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  next_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  current_indexes = gom_entity_spec_list_indexes (entity_diff->current_entity, &n_current);
  next_indexes = gom_entity_spec_list_indexes (entity_diff->next_entity, &n_next);

  for (guint i = 0; i < n_current; i++)
    {
      GomIndexSpec *index = (GomIndexSpec *)current_indexes[i];
      g_autofree char *index_key = gom_registry_diff_index_key (index);

      g_hash_table_insert (current_map, g_steal_pointer (&index_key), index);
    }

  for (guint i = 0; i < n_next; i++)
    {
      GomIndexSpec *index = (GomIndexSpec *)next_indexes[i];
      g_autofree char *index_key = gom_registry_diff_index_key (index);

      g_hash_table_insert (next_map, g_steal_pointer (&index_key), index);
    }

  g_hash_table_iter_init (&iter, current_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomIndexSpec *current_index = value;
      GomIndexSpec *next_index = g_hash_table_lookup (next_map, key);

      if (next_index == NULL)
        {
          g_ptr_array_add (entity_diff->dropped_indexes, g_object_ref (current_index));
        }
      else if (!gom_registry_diff_index_equals (current_index, next_index))
        {
          GomIndexDiff *index_diff = g_new0 (GomIndexDiff, 1);

          index_diff->current_index = g_object_ref (current_index);
          index_diff->next_index = g_object_ref (next_index);

          g_ptr_array_add (entity_diff->changed_indexes, index_diff);
        }
    }

  g_hash_table_iter_init (&iter, next_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomIndexSpec *next_index = value;

      if (!g_hash_table_contains (current_map, key))
        g_ptr_array_add (entity_diff->added_indexes, g_object_ref (next_index));
    }
}

static void
gom_registry_diff_populate_relationship_changes (GomEntityDiff *entity_diff)
{
  g_autoptr(GListModel) current_relationships_model = NULL;
  g_autoptr(GListModel) next_relationships_model = NULL;
  g_autoptr(GHashTable) current_map = NULL;
  g_autoptr(GHashTable) next_map = NULL;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  guint n_current = 0;
  guint n_next = 0;

  current_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  next_map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  current_relationships_model = gom_entity_spec_list_relationships (entity_diff->current_entity);
  next_relationships_model = gom_entity_spec_list_relationships (entity_diff->next_entity);
  n_current = g_list_model_get_n_items (current_relationships_model);
  n_next = g_list_model_get_n_items (next_relationships_model);

  for (guint i = 0; i < n_current; i++)
    {
      g_autoptr(GomRelationshipSpec) relationship = NULL;

      relationship = g_list_model_get_item (current_relationships_model, i);
      g_hash_table_insert (current_map,
                           g_strdup (gom_relationship_spec_get_name (relationship)),
                           g_object_ref (relationship));
    }

  for (guint i = 0; i < n_next; i++)
    {
      g_autoptr(GomRelationshipSpec) relationship = NULL;

      relationship = g_list_model_get_item (next_relationships_model, i);
      g_hash_table_insert (next_map,
                           g_strdup (gom_relationship_spec_get_name (relationship)),
                           g_object_ref (relationship));
    }

  g_hash_table_iter_init (&iter, current_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomRelationshipSpec *current_relationship = value;
      GomRelationshipSpec *next_relationship = g_hash_table_lookup (next_map, key);

      if (next_relationship == NULL)
        {
          g_ptr_array_add (entity_diff->dropped_relationships, g_object_ref (current_relationship));
        }
      else if (!gom_registry_diff_relationship_equals (current_relationship, next_relationship))
        {
          GomRelationshipDiff *relationship_diff = g_new0 (GomRelationshipDiff, 1);

          relationship_diff->current_relationship = g_object_ref (current_relationship);
          relationship_diff->next_relationship = g_object_ref (next_relationship);

          g_ptr_array_add (entity_diff->changed_relationships, relationship_diff);
        }
    }

  g_hash_table_iter_init (&iter, next_map);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomRelationshipSpec *next_relationship = value;

      if (!g_hash_table_contains (current_map, key))
        g_ptr_array_add (entity_diff->added_relationships, g_object_ref (next_relationship));
    }
}

static gboolean
gom_registry_diff_entity_has_changes (GomEntityDiff *entity_diff)
{
  g_assert (entity_diff != NULL);

  return entity_diff->identity_changed ||
         entity_diff->dropped_properties->len > 0 ||
         entity_diff->added_properties->len > 0 ||
         entity_diff->changed_properties->len > 0 ||
         entity_diff->dropped_indexes->len > 0 ||
         entity_diff->added_indexes->len > 0 ||
         entity_diff->changed_indexes->len > 0 ||
         entity_diff->dropped_relationships->len > 0 ||
         entity_diff->added_relationships->len > 0 ||
         entity_diff->changed_relationships->len > 0;
}

GomRegistryDiff *
_gom_registry_diff_new (GomRegistry *current,
                        GomRegistry *next)
{
  g_autoptr(GHashTable) current_by_table = NULL;
  g_autoptr(GHashTable) next_by_table = NULL;
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  GomRegistryDiff *self;

  g_return_val_if_fail (GOM_IS_REGISTRY (current), NULL);
  g_return_val_if_fail (GOM_IS_REGISTRY (next), NULL);

  self = g_new0 (GomRegistryDiff, 1);
  self->dropped_entities = g_ptr_array_new_with_free_func (g_object_unref);
  self->added_entities = g_ptr_array_new_with_free_func (g_object_unref);
  self->changed_entities = g_ptr_array_new_with_free_func (gom_entity_diff_free);

  current_by_table = gom_registry_diff_build_entity_table_map (current);
  next_by_table = gom_registry_diff_build_entity_table_map (next);

  g_hash_table_iter_init (&iter, current_by_table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *table = key;
      GomEntitySpec *current_entity = value;
      GomEntitySpec *next_entity = g_hash_table_lookup (next_by_table, table);

      if (next_entity == NULL)
        {
          g_ptr_array_add (self->dropped_entities, g_object_ref (current_entity));
        }
      else
        {
          GomEntityDiff *entity_diff = gom_registry_diff_entity_new (current_entity, next_entity);

          gom_registry_diff_populate_property_changes (entity_diff);
          gom_registry_diff_populate_index_changes (entity_diff);
          gom_registry_diff_populate_relationship_changes (entity_diff);

          if (gom_registry_diff_entity_has_changes (entity_diff))
            g_ptr_array_add (self->changed_entities, entity_diff);
          else
            gom_entity_diff_free (entity_diff);
        }
    }

  g_hash_table_iter_init (&iter, next_by_table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *table = key;
      GomEntitySpec *next_entity = value;

      if (!g_hash_table_contains (current_by_table, table))
        g_ptr_array_add (self->added_entities, g_object_ref (next_entity));
    }

  return self;
}

const GPtrArray *
_gom_registry_diff_get_dropped_entities (GomRegistryDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->dropped_entities;
}

const GPtrArray *
_gom_registry_diff_get_added_entities (GomRegistryDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->added_entities;
}

const GPtrArray *
_gom_registry_diff_get_changed_entities (GomRegistryDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->changed_entities;
}

GomEntitySpec *
_gom_entity_diff_get_current_entity (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->current_entity;
}

GomEntitySpec *
_gom_entity_diff_get_next_entity (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->next_entity;
}

gboolean
_gom_entity_diff_get_identity_changed (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->identity_changed;
}

const GPtrArray *
_gom_entity_diff_get_dropped_properties (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->dropped_properties;
}

const GPtrArray *
_gom_entity_diff_get_added_properties (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->added_properties;
}

const GPtrArray *
_gom_entity_diff_get_changed_properties (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->changed_properties;
}

const GPtrArray *
_gom_entity_diff_get_dropped_indexes (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->dropped_indexes;
}

const GPtrArray *
_gom_entity_diff_get_added_indexes (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->added_indexes;
}

const GPtrArray *
_gom_entity_diff_get_changed_indexes (GomEntityDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->changed_indexes;
}

GomPropertySpec *
_gom_property_diff_get_current_property (GomPropertyDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->current_property;
}

GomPropertySpec *
_gom_property_diff_get_next_property (GomPropertyDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->next_property;
}

GomIndexSpec *
_gom_index_diff_get_current_index (GomIndexDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->current_index;
}

GomIndexSpec *
_gom_index_diff_get_next_index (GomIndexDiff *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->next_index;
}
