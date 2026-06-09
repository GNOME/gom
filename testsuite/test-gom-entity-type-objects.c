/* test-gom-entity-type-objects.c
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

enum
{
  TEST_ENTITY_BASE_PROP_0,
  TEST_ENTITY_BASE_PROP_ID1,
  TEST_ENTITY_BASE_PROP_NAME,
  TEST_ENTITY_BASE_PROP_INTERNAL,
  TEST_ENTITY_BASE_N_PROPS
};

enum
{
  TEST_ENTITY_INVALID_INVERSE_TARGET_PROP_0,
  TEST_ENTITY_INVALID_INVERSE_TARGET_PROP_ID,
  TEST_ENTITY_INVALID_INVERSE_TARGET_N_PROPS
};

static GParamSpec *test_entity_base_properties[TEST_ENTITY_BASE_N_PROPS];
static GParamSpec *test_entity_invalid_inverse_target_properties[TEST_ENTITY_INVALID_INVERSE_TARGET_N_PROPS];

G_DEFINE_TYPE (TestEntityBase, test_entity_base, GOM_TYPE_ENTITY)

static void
test_entity_base_finalize (GObject *object)
{
  TestEntityBase *self = (TestEntityBase *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->internal, g_free);

  G_OBJECT_CLASS (test_entity_base_parent_class)->finalize (object);
}

static void
test_entity_base_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestEntityBase *self = (TestEntityBase *)object;

  switch (prop_id)
    {
    case TEST_ENTITY_BASE_PROP_ID1:
      g_value_set_int64 (value, self->id1);
      break;

    case TEST_ENTITY_BASE_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_ENTITY_BASE_PROP_INTERNAL:
      g_value_set_string (value, self->internal);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_base_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestEntityBase *self = (TestEntityBase *)object;

  switch (prop_id)
    {
    case TEST_ENTITY_BASE_PROP_ID1:
      self->id1 = g_value_get_int64 (value);
      break;

    case TEST_ENTITY_BASE_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_ENTITY_BASE_PROP_INTERNAL:
      g_set_str (&self->internal, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_base_class_init (TestEntityBaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_entity_base_finalize;
  object_class->get_property = test_entity_base_get_property;
  object_class->set_property = test_entity_base_set_property;

  test_entity_base_properties[TEST_ENTITY_BASE_PROP_ID1] =
    g_param_spec_int64 ("id1", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_entity_base_properties[TEST_ENTITY_BASE_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_entity_base_properties[TEST_ENTITY_BASE_PROP_INTERNAL] =
    g_param_spec_string ("internal", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_ENTITY_BASE_N_PROPS,
                                     test_entity_base_properties);

  gom_entity_class_set_relation (entity_class, "test_entity_base");
  gom_entity_class_set_identity_field (entity_class, "id1");
  gom_entity_class_set_discriminator_field (entity_class, "type");
  gom_entity_class_set_discriminator_value (entity_class, "base");
  gom_entity_class_set_version_added (entity_class, 100);
  gom_entity_class_set_version_removed (entity_class, 350);
  gom_entity_class_property_set_nonnull (entity_class, "name", TRUE);
  gom_entity_class_property_set_unique (entity_class, "name", TRUE);
  gom_entity_class_property_set_search_flags (entity_class,
                                             "name",
                                             GOM_SEARCH_INDEXED | GOM_SEARCH_PREFIX);
  gom_entity_class_property_set_version_added (entity_class, "name", 100);
  gom_entity_class_property_set_reference (entity_class, "name", "names", "value");
  gom_entity_class_property_set_mapped (entity_class, "internal", FALSE);
  gom_entity_class_property_set_version_removed (entity_class, "internal", 300);
  gom_entity_class_add_many_to_one (entity_class, "parent", TEST_ENTITY_BASE_TYPE, "id1", "children");
  gom_entity_class_add_one_to_many (entity_class, "children", TEST_ENTITY_BASE_TYPE, "id1", "parent");
  gom_entity_class_add_many_to_many (entity_class,
                                     "related",
                                     TEST_ENTITY_BASE_TYPE,
                                     "test_entity_base_links",
                                     "left_id",
                                     "right_id",
                                     "related");
}

static void
test_entity_base_init (TestEntityBase *self)
{
}

G_DEFINE_TYPE (TestEntityBaseCustomIdentity,
               test_entity_base_custom_identity,
               TEST_ENTITY_BASE_TYPE)

static void
test_entity_base_custom_identity_class_init (TestEntityBaseCustomIdentityClass *klass)
{
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  gom_entity_class_set_relation (entity_class, "test_entity_base_custom_identity");
  gom_entity_class_set_identity_field (entity_class, "id1a");
  gom_entity_class_set_version_added (entity_class, 200);
}

static void
test_entity_base_custom_identity_init (TestEntityBaseCustomIdentity *self)
{
}

G_DEFINE_TYPE (TestEntityBaseInheritedMetadata,
               test_entity_base_inherited_metadata,
               TEST_ENTITY_BASE_TYPE)

static void
test_entity_base_inherited_metadata_class_init (TestEntityBaseInheritedMetadataClass *klass)
{
  /* TestEntityBaseInheritedMetadata inherits table, identity_fields, and versioning from
   * TestEntityBase.
   */
}

static void
test_entity_base_inherited_metadata_init (TestEntityBaseInheritedMetadata *self)
{
}

G_DEFINE_TYPE (TestEntityInvalidInverseTarget,
               test_entity_invalid_inverse_target,
               GOM_TYPE_ENTITY)

static void
test_entity_invalid_inverse_target_finalize (GObject *object)
{
  G_OBJECT_CLASS (test_entity_invalid_inverse_target_parent_class)->finalize (object);
}

static void
test_entity_invalid_inverse_target_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  TestEntityInvalidInverseTarget *self = (TestEntityInvalidInverseTarget *) object;

  switch (prop_id)
    {
    case TEST_ENTITY_INVALID_INVERSE_TARGET_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_invalid_inverse_target_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  TestEntityInvalidInverseTarget *self = (TestEntityInvalidInverseTarget *) object;

  switch (prop_id)
    {
    case TEST_ENTITY_INVALID_INVERSE_TARGET_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_invalid_inverse_target_class_init (TestEntityInvalidInverseTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_entity_invalid_inverse_target_finalize;
  object_class->get_property = test_entity_invalid_inverse_target_get_property;
  object_class->set_property = test_entity_invalid_inverse_target_set_property;

  test_entity_invalid_inverse_target_properties[TEST_ENTITY_INVALID_INVERSE_TARGET_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                    TEST_ENTITY_INVALID_INVERSE_TARGET_N_PROPS,
                                    test_entity_invalid_inverse_target_properties);

  gom_entity_class_set_relation (entity_class, "invalid_inverse_target");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
}

static void
test_entity_invalid_inverse_target_init (TestEntityInvalidInverseTarget *self)
{
}

void
test_entity_register_types (GomRegistryBuilder *builder)
{
  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_BASE_TYPE);
  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_BASE_CUSTOM_IDENTITY_TYPE);
  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_BASE_INHERITED_METADATA_TYPE);
}
