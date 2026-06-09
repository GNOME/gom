/* test-gom-entity-type-base-inherited-metadata.c
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

static void
test_entity_base_inherited_metadata_inherits_base_relationships (void)
{
  GTypeClass *klass = g_type_class_ref (TEST_ENTITY_BASE_INHERITED_METADATA_TYPE);
  GType parent;

  g_assert_nonnull (klass);
  parent = g_type_parent (TEST_ENTITY_BASE_INHERITED_METADATA_TYPE);

  g_assert_cmpuint (parent, ==, TEST_ENTITY_BASE_TYPE);
  g_assert_cmpstr (gom_entity_class_get_relation (GOM_ENTITY_CLASS (klass)), ==, "test_entity_base");
  g_assert_cmpstr (gom_entity_class_get_discriminator_field (GOM_ENTITY_CLASS (klass)), ==, "type");
  g_assert_cmpstr (gom_entity_class_get_discriminator_value (GOM_ENTITY_CLASS (klass)), ==, "base");
  g_type_class_unref (klass);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Entity/type-base-inherited-metadata/inherits",
                    test_entity_base_inherited_metadata_inherits_base_relationships);
  return g_test_run ();
}
