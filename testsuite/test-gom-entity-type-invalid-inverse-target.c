/* test-gom-entity-type-invalid-inverse-target.c
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
test_entity_invalid_inverse_target_reports_expected_type (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();
  g_autoptr(GomRegistry) registry = NULL;

  gom_registry_builder_add_entity_type (builder, TEST_ENTITY_INVALID_INVERSE_TARGET_TYPE);
  registry = gom_registry_builder_build (builder);

  g_assert_nonnull (registry);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Entity/type-invalid-inverse-target/registry-builds",
                    test_entity_invalid_inverse_target_reports_expected_type);
  return g_test_run ();
}
