/* gom-value.c
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

#include "gom-util-private.h"
#include "gom-value-private.h"
#include "gom-vector.h"

static gboolean
gom_value_bytes_equal0 (GBytes *a,
                        GBytes *b)
{
  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  return g_bytes_equal (a, b);
}

static gboolean
gom_value_vector_equal0 (GomVector *a,
                         GomVector *b)
{
  g_autoptr(GBytes) a_bytes = NULL;
  g_autoptr(GBytes) b_bytes = NULL;

  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  if (gom_vector_get_format (a) != gom_vector_get_format (b))
    return FALSE;

  if (gom_vector_get_dimensions (a) != gom_vector_get_dimensions (b))
    return FALSE;

  a_bytes = gom_vector_dup_bytes (a);
  b_bytes = gom_vector_dup_bytes (b);

  return gom_value_bytes_equal0 (a_bytes, b_bytes);
}

static void
gom_value_append_escaped_bytes (GString      *str,
                                const guint8 *data,
                                gsize         len)
{
  g_assert (str != NULL);
  g_assert (data != NULL || len == 0);

  for (gsize i = 0; i < len; i++)
    g_string_append_printf (str, "\\x%02x", data[i]);
}

static void
gom_value_append_escaped_string (GString    *str,
                                 const char *value)
{
  g_assert (str != NULL);
  g_assert (value != NULL);

  for (const guint8 *iter = (const guint8 *)value; *iter != 0; iter++)
    {
      if (*iter == '\\')
        g_string_append (str, "\\\\");
      else if (*iter < 0x20 || *iter == 0x7f)
        g_string_append_printf (str, "\\x%02x", *iter);
      else
        g_string_append_c (str, *iter);
    }
}

char *
_gom_value_dup_identity_key (const GValue *value)
{
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  if (G_VALUE_HOLDS_BOOLEAN (value))
    return g_strdup (g_value_get_boolean (value) ? "true" : "false");

  if (G_VALUE_HOLDS_CHAR (value))
    return g_strdup_printf ("%d", (int)g_value_get_schar (value));

  if (G_VALUE_HOLDS_UCHAR (value))
    return g_strdup_printf ("%u", (guint)g_value_get_uchar (value));

  if (G_VALUE_HOLDS_INT (value))
    return g_strdup_printf ("%d", g_value_get_int (value));

  if (G_VALUE_HOLDS_UINT (value))
    return g_strdup_printf ("%u", g_value_get_uint (value));

  if (G_VALUE_HOLDS_LONG (value))
    return g_strdup_printf ("%ld", g_value_get_long (value));

  if (G_VALUE_HOLDS_ULONG (value))
    return g_strdup_printf ("%lu", g_value_get_ulong (value));

  if (G_VALUE_HOLDS_INT64 (value))
    return g_strdup_printf ("%" G_GINT64_FORMAT, g_value_get_int64 (value));

  if (G_VALUE_HOLDS_UINT64 (value))
    return g_strdup_printf ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value));

  if (G_VALUE_HOLDS_FLOAT (value))
    return g_strdup (g_ascii_dtostr (buffer, sizeof buffer, g_value_get_float (value)));

  if (G_VALUE_HOLDS_DOUBLE (value))
    return g_strdup (g_ascii_dtostr (buffer, sizeof buffer, g_value_get_double (value)));

  if (G_VALUE_HOLDS_ENUM (value))
    return g_strdup_printf ("%d", g_value_get_enum (value));

  if (G_VALUE_HOLDS_FLAGS (value))
    return g_strdup_printf ("%u", g_value_get_flags (value));

  if (G_VALUE_HOLDS_STRING (value))
    {
      const char *str = g_value_get_string (value);
      g_autoptr(GString) escaped = NULL;

      if (str == NULL)
        return g_strdup ("\\N");

      escaped = g_string_new (NULL);
      gom_value_append_escaped_string (escaped, str);
      return g_string_free (g_steal_pointer (&escaped), FALSE);
    }

  if (G_VALUE_HOLDS_GTYPE (value))
    {
      GType gtype = g_value_get_gtype (value);
      const char *type_name = g_type_name (gtype);

      return g_strdup (type_name != NULL ? type_name : "\\N");
    }

  if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
    {
      GDateTime *datetime = g_value_get_boxed (value);

      if (datetime == NULL)
        return g_strdup ("\\N");

      return g_date_time_format_iso8601 (datetime);
    }

  if (G_VALUE_HOLDS (value, G_TYPE_BYTES))
    {
      GBytes *bytes = g_value_get_boxed (value);
      g_autoptr(GString) escaped = NULL;
      gsize len = 0;
      const guint8 *data = NULL;

      if (bytes == NULL)
        return g_strdup ("\\N");

      data = g_bytes_get_data (bytes, &len);
      escaped = g_string_new (NULL);
      gom_value_append_escaped_bytes (escaped, data, len);
      return g_string_free (g_steal_pointer (&escaped), FALSE);
    }

  g_critical ("Cannot build identity key for unsupported value type `%s`",
              G_VALUE_TYPE_NAME (value));

  return NULL;
}

gboolean
_gom_value_equal (const GValue *a,
                  const GValue *b)
{
  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);
  g_return_val_if_fail (G_IS_VALUE (a), FALSE);
  g_return_val_if_fail (G_IS_VALUE (b), FALSE);

  if (G_VALUE_TYPE (a) != G_VALUE_TYPE (b))
    return FALSE;

  if (G_VALUE_HOLDS_BOOLEAN (a))
    return g_value_get_boolean (a) == g_value_get_boolean (b);

  if (G_VALUE_HOLDS_CHAR (a))
    return g_value_get_schar (a) == g_value_get_schar (b);

  if (G_VALUE_HOLDS_UCHAR (a))
    return g_value_get_uchar (a) == g_value_get_uchar (b);

  if (G_VALUE_HOLDS_INT (a))
    return g_value_get_int (a) == g_value_get_int (b);

  if (G_VALUE_HOLDS_UINT (a))
    return g_value_get_uint (a) == g_value_get_uint (b);

  if (G_VALUE_HOLDS_LONG (a))
    return g_value_get_long (a) == g_value_get_long (b);

  if (G_VALUE_HOLDS_ULONG (a))
    return g_value_get_ulong (a) == g_value_get_ulong (b);

  if (G_VALUE_HOLDS_INT64 (a))
    return g_value_get_int64 (a) == g_value_get_int64 (b);

  if (G_VALUE_HOLDS_UINT64 (a))
    return g_value_get_uint64 (a) == g_value_get_uint64 (b);

  if (G_VALUE_HOLDS_FLOAT (a))
    return g_value_get_float (a) == g_value_get_float (b);

  if (G_VALUE_HOLDS_DOUBLE (a))
    return g_value_get_double (a) == g_value_get_double (b);

  if (G_VALUE_HOLDS_ENUM (a))
    return g_value_get_enum (a) == g_value_get_enum (b);

  if (G_VALUE_HOLDS_FLAGS (a))
    return g_value_get_flags (a) == g_value_get_flags (b);

  if (G_VALUE_HOLDS_STRING (a))
    return g_strcmp0 (g_value_get_string (a), g_value_get_string (b)) == 0;

  if (G_VALUE_HOLDS_GTYPE (a))
    return g_value_get_gtype (a) == g_value_get_gtype (b);

  if (G_VALUE_HOLDS_VARIANT (a))
    {
      GVariant *a_variant = g_value_get_variant (a);
      GVariant *b_variant = g_value_get_variant (b);

      if (a_variant == b_variant)
        return TRUE;

      if (a_variant == NULL || b_variant == NULL)
        return FALSE;

      return g_variant_equal (a_variant, b_variant);
    }

  if (G_VALUE_HOLDS (a, G_TYPE_BYTES))
    return gom_value_bytes_equal0 (g_value_get_boxed (a), g_value_get_boxed (b));

  if (G_VALUE_HOLDS (a, G_TYPE_DATE_TIME))
    {
      GDateTime *a_datetime = g_value_get_boxed (a);
      GDateTime *b_datetime = g_value_get_boxed (b);

      if (a_datetime == b_datetime)
        return TRUE;

      if (a_datetime == NULL || b_datetime == NULL)
        return FALSE;

      return g_date_time_equal (a_datetime, b_datetime);
    }

  if (G_VALUE_HOLDS (a, G_TYPE_STRV))
    return _gom_strv_equal (g_value_get_boxed (a), g_value_get_boxed (b));

  if (G_VALUE_HOLDS (a, GOM_TYPE_VECTOR))
    return gom_value_vector_equal0 (g_value_get_boxed (a), g_value_get_boxed (b));

  if (G_VALUE_HOLDS_OBJECT (a))
    return g_value_get_object (a) == g_value_get_object (b);

  if (G_VALUE_HOLDS_PARAM (a))
    return g_value_get_param (a) == g_value_get_param (b);

  if (G_VALUE_HOLDS_BOXED (a))
    return g_value_get_boxed (a) == g_value_get_boxed (b);

  if (G_VALUE_HOLDS_POINTER (a))
    return g_value_get_pointer (a) == g_value_get_pointer (b);

  return FALSE;
}
