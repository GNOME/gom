/* test-gom-util.c
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

#include <glib.h>

#include "lib/gom-util-private.h"

static void
test_gom_util_strv_text_roundtrip (void)
{
  const char * const tags[] = {
    "alpha",
    "",
    "the-key",
    "line1\nline2",
    "slash\\tail",
    NULL,
  };
  const char * const empty_tags[] = { NULL };
  g_auto(GStrv) parsed = NULL;
  g_autofree char *encoded = NULL;
  g_autofree char *empty_encoded = NULL;
  g_autoptr(GError) error = NULL;

  encoded = _gom_strv_to_text (tags);
  parsed = _gom_strv_from_text (encoded, &error);
  g_assert_no_error (error);
  g_assert_nonnull (parsed);
  g_assert_true (g_strv_equal ((const char * const *) parsed, tags));

  empty_encoded = _gom_strv_to_text (empty_tags);
  g_assert_cmpstr (empty_encoded, ==, "\n\n");
  g_clear_pointer (&error, g_error_free);
  g_clear_pointer (&parsed, g_strfreev);
  parsed = _gom_strv_from_text (empty_encoded, &error);
  g_assert_no_error (error);
  g_assert_nonnull (parsed);
  g_assert_cmpuint (g_strv_length (parsed), ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/Gom/Util/strv-text-roundtrip", test_gom_util_strv_text_roundtrip);

  return g_test_run ();
}
