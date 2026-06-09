/* test-gom-entity-type-missing-metadata.c
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

#include "test-util.h"

typedef struct _TestEntityMissingMetadata      TestEntityMissingMetadata;
typedef struct _TestEntityMissingMetadataClass TestEntityMissingMetadataClass;

struct _TestEntityMissingMetadata
{
  GomEntity parent_instance;
};

struct _TestEntityMissingMetadataClass
{
  GomEntityClass parent_class;
};

GType test_entity_missing_metadata_get_type (void);

G_DEFINE_TYPE (TestEntityMissingMetadata, test_entity_missing_metadata, GOM_TYPE_ENTITY)

static void
test_entity_missing_metadata_class_init (TestEntityMissingMetadataClass *klass)
{
}

static void
test_entity_missing_metadata_init (TestEntityMissingMetadata *self)
{
}

#define TEST_ENTITY_MISSING_METADATA_TYPE (test_entity_missing_metadata_get_type ())

static void
test_entity_registry_builder_warns_for_missing_metadata (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();
  g_autoptr(GomRegistry) registry = NULL;

  g_test_expect_message ("Gom",
                         G_LOG_LEVEL_WARNING,
                         "*TestEntityMissingMetadata*missing a relation name*");
  g_test_expect_message ("Gom",
                         G_LOG_LEVEL_WARNING,
                         "*TestEntityMissingMetadata*missing version_added*");

  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_MISSING_METADATA_TYPE);
  registry = gom_registry_builder_build (builder);

  g_test_assert_expected_messages ();
  g_assert_nonnull (registry);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Entity/registry-builder/warns-for-missing-metadata",
                    test_entity_registry_builder_warns_for_missing_metadata);
  return g_test_run ();
}
