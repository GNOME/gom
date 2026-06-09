/* test-gom-entity-type-invalid-inverse-source.c
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

typedef struct _TestEntityInvalidInverseSource      TestEntityInvalidInverseSource;
typedef struct _TestEntityInvalidInverseSourceClass TestEntityInvalidInverseSourceClass;

struct _TestEntityInvalidInverseSource
{
  GomEntity parent_instance;
  int64_t   id;
};

struct _TestEntityInvalidInverseSourceClass
{
  GomEntityClass parent_class;
};

GType test_entity_invalid_inverse_source_get_type (void);

enum
{
  TEST_ENTITY_INVALID_INVERSE_SOURCE_PROP_0,
  TEST_ENTITY_INVALID_INVERSE_SOURCE_PROP_ID,
  TEST_ENTITY_INVALID_INVERSE_SOURCE_N_PROPS
};

static GParamSpec *test_entity_invalid_inverse_source_properties[TEST_ENTITY_INVALID_INVERSE_SOURCE_N_PROPS];

G_DEFINE_TYPE (TestEntityInvalidInverseSource,
               test_entity_invalid_inverse_source,
               GOM_TYPE_ENTITY)

static void
test_entity_invalid_inverse_source_get_property (GObject    *object,
                                                 guint       prop_id,
                                                 GValue     *value,
                                                 GParamSpec *pspec)
{
  TestEntityInvalidInverseSource *self = (TestEntityInvalidInverseSource *) object;

  switch (prop_id)
    {
    case TEST_ENTITY_INVALID_INVERSE_SOURCE_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_invalid_inverse_source_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec)
{
  TestEntityInvalidInverseSource *self = (TestEntityInvalidInverseSource *) object;

  switch (prop_id)
    {
    case TEST_ENTITY_INVALID_INVERSE_SOURCE_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_invalid_inverse_source_class_init (TestEntityInvalidInverseSourceClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->get_property = test_entity_invalid_inverse_source_get_property;
  object_class->set_property = test_entity_invalid_inverse_source_set_property;

  test_entity_invalid_inverse_source_properties[TEST_ENTITY_INVALID_INVERSE_SOURCE_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                    TEST_ENTITY_INVALID_INVERSE_SOURCE_N_PROPS,
                                    test_entity_invalid_inverse_source_properties);

  gom_entity_class_set_relation (entity_class, "invalid_inverse_source");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_add_many_to_one (entity_class,
                                    "parent",
                                    TEST_ENTITY_INVALID_INVERSE_TARGET_TYPE,
                                    "id",
                                    "children");
}

static void
test_entity_invalid_inverse_source_init (TestEntityInvalidInverseSource *self)
{
}

#define TEST_ENTITY_INVALID_INVERSE_SOURCE_TYPE (test_entity_invalid_inverse_source_get_type ())

static void
test_entity_registry_builder_warns_for_broken_inverse (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();
  g_autoptr(GomRegistry) registry = NULL;

  g_test_expect_message ("Gom",
                         G_LOG_LEVEL_WARNING,
                         "*TestEntityInvalidInverseSource*relationship `parent` references inverse `children`*does not exist*");

  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_INVALID_INVERSE_SOURCE_TYPE);
  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_INVALID_INVERSE_TARGET_TYPE);
  registry = gom_registry_builder_build (builder);

  g_test_assert_expected_messages ();
  g_assert_nonnull (registry);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Entity/registry-builder/warns-for-broken-inverse",
                    test_entity_registry_builder_warns_for_broken_inverse);
  return g_test_run ();
}
