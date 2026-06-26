/* test-gom-entity-type-base.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libgom.h>

#include "test-gom-entity-types.h"
#include "test-util.h"

static GomPropertySpec *
test_entity_find_property_spec (GomEntitySpec *spec,
                                const char    *name)
{
  const GomPropertySpec * const *properties;
  guint n_properties = 0;

  properties = gom_entity_spec_list_properties (spec, &n_properties);

  for (guint i = 0; i < n_properties; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)properties[i];

      if (g_strcmp0 (gom_property_spec_get_name (property), name) == 0)
        return property;
    }

  return NULL;
}

static GomRegistry *
test_entity_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  test_entity_register_types (builder);

  return gom_registry_builder_build (builder);
}

static GomRelationshipSpec *
test_entity_find_relationship_spec (GomEntitySpec *spec,
                                    const char    *name)
{
  g_autoptr(GListModel) relationships = NULL;
  guint n_relationships;

  relationships = gom_entity_spec_list_relationships (spec);
  n_relationships = g_list_model_get_n_items (relationships);

  for (guint i = 0; i < n_relationships; i++)
    {
      g_autoptr(GomRelationshipSpec) relationship = NULL;

      relationship = g_list_model_get_item (relationships, i);
      if (g_strcmp0 (gom_relationship_spec_get_name (relationship), name) == 0)
        {
          return g_steal_pointer (&relationship);
        }
    }

  return NULL;
}

static GomEntitySpec *
test_entity_find_spec (GomRegistry *registry,
                       GType        entity_type)
{
  const GomEntitySpec * const *entities;
  guint n_entities = 0;

  entities = gom_registry_list_entities (registry, &n_entities);

  for (guint i = 0; i < n_entities; i++)
    {
      GomEntitySpec *spec = (GomEntitySpec *) entities[i];

      if (gom_entity_spec_get_entity_type (spec) == entity_type)
        return spec;
    }

  return NULL;
}

static void
test_entity_subclass_metadata (void)
{
  g_autoptr(GomRegistry) registry = NULL;
  GTypeClass *base_class = NULL;
  GTypeClass *inherited_metadata_class = NULL;
  GomEntitySpec *base_spec;
  GomEntitySpec *custom_identity_spec;
  GomEntitySpec *inherited_metadata_spec;
  const char *table;
  const char *discriminator_field;
  const char *discriminator_value;
  const char * const *identity_fields;
  const GomPropertySpec * const *property_specs;
  GListModel *relationships;
  const char * const *local_fields;
  const char * const *remote_fields;
  const char * const *join_local_fields;
  const char * const *join_remote_fields;
  GomPropertySpec *name_spec;
  GomPropertySpec *internal_spec;
  GomRelationshipSpec *parent_spec;
  GomRelationshipSpec *children_spec;
  GomRelationshipSpec *related_spec;
  GomRelationshipSpec *feed_child_relationship_spec;
  GomRelationshipSpec *child_feed_relationship_spec;
  GomEntitySpec *one_to_one_feed_spec;
  GomEntitySpec *one_to_one_child_spec;
  guint n_property_specs = 0;
  guint n_relationships = 0;
  guint version_added;
  guint version_removed;

  registry = test_entity_create_registry ();
  g_assert_nonnull (registry);

  base_class = g_type_class_ref (TEST_ENTITY_BASE_TYPE);
  g_assert_nonnull (base_class);
  g_assert_cmpstr (gom_entity_class_get_relation (GOM_ENTITY_CLASS (base_class)), ==, "test_entity_base");
  g_assert_cmpstr (gom_entity_class_get_discriminator_field (GOM_ENTITY_CLASS (base_class)), ==, "type");
  g_assert_cmpstr (gom_entity_class_get_discriminator_value (GOM_ENTITY_CLASS (base_class)), ==, "base");
  g_assert_true (gom_entity_class_property_get_nonnull (GOM_ENTITY_CLASS (base_class), "name"));
  g_assert_true (gom_entity_class_property_get_unique (GOM_ENTITY_CLASS (base_class), "name"));
  g_assert_true (gom_entity_class_property_get_mapped (GOM_ENTITY_CLASS (base_class), "name"));
  g_assert_false (gom_entity_class_property_get_mapped (GOM_ENTITY_CLASS (base_class), "internal"));

  inherited_metadata_class = g_type_class_ref (TEST_ENTITY_BASE_INHERITED_METADATA_TYPE);
  g_assert_nonnull (inherited_metadata_class);
  g_assert_cmpstr (gom_entity_class_get_relation (GOM_ENTITY_CLASS (inherited_metadata_class)), ==, "test_entity_base");
  g_assert_cmpstr (gom_entity_class_get_discriminator_field (GOM_ENTITY_CLASS (inherited_metadata_class)), ==, "type");
  g_assert_cmpstr (gom_entity_class_get_discriminator_value (GOM_ENTITY_CLASS (inherited_metadata_class)), ==, "base");
  g_type_class_unref (inherited_metadata_class);
  g_type_class_unref (base_class);

  base_spec = test_entity_find_spec (registry, TEST_ENTITY_BASE_TYPE);
  g_assert_nonnull (base_spec);

  table = gom_entity_spec_get_table (base_spec);
  g_assert_cmpstr (table, ==, "test_entity_base");

  identity_fields = gom_entity_spec_get_identity_fields (base_spec);
  g_assert_nonnull (identity_fields);
  g_assert_cmpstr (identity_fields[0], ==, "id1");
  g_assert_null (identity_fields[1]);

  version_added = gom_entity_spec_get_version_added (base_spec);
  g_assert_cmpuint (version_added, ==, 100);
  version_removed = gom_entity_spec_get_version_removed (base_spec);
  g_assert_cmpuint (version_removed, ==, 350);

  discriminator_field = gom_entity_spec_get_discriminator_field (base_spec);
  g_assert_cmpstr (discriminator_field, ==, "type");
  discriminator_value = gom_entity_spec_get_discriminator_value (base_spec);
  g_assert_cmpstr (discriminator_value, ==, "base");

  property_specs = gom_entity_spec_list_properties (base_spec, &n_property_specs);
  g_assert_nonnull (property_specs);
  g_assert_cmpuint (n_property_specs, >=, 2);

  relationships = gom_entity_spec_list_relationships (base_spec);
  g_assert_nonnull (relationships);
  n_relationships = g_list_model_get_n_items (relationships);
  g_assert_cmpuint (n_relationships, ==, 3);

  name_spec = test_entity_find_property_spec (base_spec, "name");
  g_assert_nonnull (name_spec);
  g_assert_cmpstr (gom_property_spec_get_field (name_spec), ==, "name");
  g_assert_cmpstr (gom_property_spec_get_reference_table (name_spec), ==, "names");
  g_assert_cmpstr (gom_property_spec_get_reference_field (name_spec), ==, "value");
  g_assert_true (gom_property_spec_get_value_type (name_spec) == G_TYPE_STRING);
  g_assert_true (gom_property_spec_get_nonnull (name_spec));
  g_assert_true (gom_property_spec_get_unique (name_spec));
  g_assert_true (gom_property_spec_get_mapped (name_spec));
  g_assert_cmpuint (gom_property_spec_get_search_flags (name_spec), ==, GOM_SEARCH_INDEXED | GOM_SEARCH_PREFIX);
  g_assert_cmpuint (gom_property_spec_get_version_added (name_spec), ==, 100);
  g_assert_cmpuint (gom_property_spec_get_version_removed (name_spec), ==, 0);

  internal_spec = test_entity_find_property_spec (base_spec, "internal");
  g_assert_nonnull (internal_spec);
  g_assert_true (gom_property_spec_get_value_type (internal_spec) == G_TYPE_STRING);
  g_assert_false (gom_property_spec_get_nonnull (internal_spec));
  g_assert_false (gom_property_spec_get_unique (internal_spec));
  g_assert_false (gom_property_spec_get_mapped (internal_spec));
  g_assert_cmpuint (gom_property_spec_get_search_flags (internal_spec), ==, GOM_SEARCH_NONE);
  g_assert_cmpuint (gom_property_spec_get_version_added (internal_spec), ==, 0);
  g_assert_cmpuint (gom_property_spec_get_version_removed (internal_spec), ==, 300);

  parent_spec = test_entity_find_relationship_spec (base_spec, "parent");
  g_assert_nonnull (parent_spec);
  g_assert_cmpint (gom_relationship_spec_get_cardinality (parent_spec), ==, GOM_RELATIONSHIP_CARDINALITY_TO_ONE);
  g_assert_cmpint (gom_relationship_spec_get_storage (parent_spec), ==, GOM_RELATIONSHIP_STORAGE_FK);
  g_assert_cmpstr (gom_relationship_spec_get_inverse_name (parent_spec), ==, "children");
  local_fields = gom_relationship_spec_get_local_fields (parent_spec);
  g_assert_nonnull (local_fields);
  g_assert_cmpstr (local_fields[0], ==, "id1");
  g_assert_null (local_fields[1]);
  g_assert_cmpuint (gom_relationship_spec_get_min_count (parent_spec), ==, 0);
  g_assert_cmpuint (gom_relationship_spec_get_max_count (parent_spec), ==, 1);

  children_spec = test_entity_find_relationship_spec (base_spec, "children");
  g_assert_nonnull (children_spec);
  g_assert_cmpint (gom_relationship_spec_get_cardinality (children_spec), ==, GOM_RELATIONSHIP_CARDINALITY_TO_MANY);
  remote_fields = gom_relationship_spec_get_remote_fields (children_spec);
  g_assert_nonnull (remote_fields);
  g_assert_cmpstr (remote_fields[0], ==, "id1");
  g_assert_null (remote_fields[1]);

  related_spec = test_entity_find_relationship_spec (base_spec, "related");
  g_assert_nonnull (related_spec);
  g_assert_cmpint (gom_relationship_spec_get_storage (related_spec), ==, GOM_RELATIONSHIP_STORAGE_JOIN_TABLE);
  g_assert_cmpstr (gom_relationship_spec_get_join_relation (related_spec), ==, "test_entity_base_links");
  join_local_fields = gom_relationship_spec_get_join_local_fields (related_spec);
  join_remote_fields = gom_relationship_spec_get_join_remote_fields (related_spec);
  g_assert_nonnull (join_local_fields);
  g_assert_nonnull (join_remote_fields);
  g_assert_cmpstr (join_local_fields[0], ==, "left_id");
  g_assert_cmpstr (join_remote_fields[0], ==, "right_id");

  one_to_one_feed_spec = test_entity_find_spec (registry, TEST_ENTITY_ONE_TO_ONE_FEED_TYPE);
  g_assert_nonnull (one_to_one_feed_spec);

  feed_child_relationship_spec = test_entity_find_relationship_spec (one_to_one_feed_spec, "child");
  g_assert_nonnull (feed_child_relationship_spec);
  g_assert_cmpint (gom_relationship_spec_get_cardinality (feed_child_relationship_spec), ==, GOM_RELATIONSHIP_CARDINALITY_TO_MANY);
  g_assert_cmpint (gom_relationship_spec_get_storage (feed_child_relationship_spec), ==, GOM_RELATIONSHIP_STORAGE_FK);
  g_assert_cmpstr (gom_relationship_spec_get_inverse_name (feed_child_relationship_spec), ==, "feed");
  g_assert_cmpuint (gom_relationship_spec_get_min_count (feed_child_relationship_spec), ==, 0);
  g_assert_cmpuint (gom_relationship_spec_get_max_count (feed_child_relationship_spec), ==, 1);
  remote_fields = gom_relationship_spec_get_remote_fields (feed_child_relationship_spec);
  g_assert_nonnull (remote_fields);
  g_assert_cmpstr (remote_fields[0], ==, "feed-id");
  g_assert_null (remote_fields[1]);

  one_to_one_child_spec = test_entity_find_spec (registry, TEST_ENTITY_ONE_TO_ONE_CHILD_TYPE);
  g_assert_nonnull (one_to_one_child_spec);

  child_feed_relationship_spec = test_entity_find_relationship_spec (one_to_one_child_spec, "feed");
  g_assert_nonnull (child_feed_relationship_spec);
  g_assert_cmpint (gom_relationship_spec_get_cardinality (child_feed_relationship_spec), ==, GOM_RELATIONSHIP_CARDINALITY_TO_ONE);
  g_assert_cmpint (gom_relationship_spec_get_storage (child_feed_relationship_spec), ==, GOM_RELATIONSHIP_STORAGE_FK);
  g_assert_cmpstr (gom_relationship_spec_get_inverse_name (child_feed_relationship_spec), ==, "child");
  local_fields = gom_relationship_spec_get_local_fields (child_feed_relationship_spec);
  g_assert_nonnull (local_fields);
  g_assert_cmpstr (local_fields[0], ==, "feed-id");
  g_assert_null (local_fields[1]);
  custom_identity_spec = test_entity_find_spec (registry, TEST_ENTITY_BASE_CUSTOM_IDENTITY_TYPE);
  g_assert_nonnull (custom_identity_spec);

  table = gom_entity_spec_get_table (custom_identity_spec);
  g_assert_cmpstr (table, ==, "test_entity_base_custom_identity");

  identity_fields = gom_entity_spec_get_identity_fields (custom_identity_spec);
  g_assert_nonnull (identity_fields);
  g_assert_cmpstr (identity_fields[0], ==, "id1a");
  g_assert_null (identity_fields[1]);

  version_added = gom_entity_spec_get_version_added (custom_identity_spec);
  g_assert_cmpuint (version_added, ==, 200);

  /* TestEntityBaseInheritedMetadata does not set table/identity_fields/version_added;
   * it must get the same values as TestEntityBase.
   */
  inherited_metadata_spec = test_entity_find_spec (registry, TEST_ENTITY_BASE_INHERITED_METADATA_TYPE);
  g_assert_nonnull (inherited_metadata_spec);

  table = gom_entity_spec_get_table (inherited_metadata_spec);
  g_assert_cmpstr (table, ==, "test_entity_base");

  identity_fields = gom_entity_spec_get_identity_fields (inherited_metadata_spec);
  g_assert_nonnull (identity_fields);
  g_assert_cmpstr (identity_fields[0], ==, "id1");
  g_assert_null (identity_fields[1]);

  version_added = gom_entity_spec_get_version_added (inherited_metadata_spec);
  g_assert_cmpuint (version_added, ==, 100);
  version_removed = gom_entity_spec_get_version_removed (inherited_metadata_spec);
  g_assert_cmpuint (version_removed, ==, 350);
  g_assert_cmpstr (gom_entity_spec_get_discriminator_field (inherited_metadata_spec), ==, "type");
  g_assert_cmpstr (gom_entity_spec_get_discriminator_value (inherited_metadata_spec), ==, "base");
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Entity/subclass_metadata", test_entity_subclass_metadata);
  return g_test_run ();
}
