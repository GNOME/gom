/* gom-util-private.h
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

char  *_gom_strv_to_text   (const char * const  *strv);
char **_gom_strv_from_text (const char          *text,
                            GError             **error);

static inline guint
_gom_strv_length (const char * const *strv)
{
  guint len = 0;

  if (strv == NULL)
    return 0;

  while (strv[len] != NULL)
    len++;

  return len;
}

static inline gboolean
_gom_strv_equal (const char * const *a,
                 const char * const *b)
{
  if (a == NULL || b == NULL)
    return a == b;

  for (guint i = 0;; i++)
    {
      if (a[i] == NULL || b[i] == NULL)
        return a[i] == b[i];

      if (g_strcmp0 (a[i], b[i]) != 0)
        return FALSE;
    }
}

/**
 * _gom_strv_contains:
 *
 * Helper that will work with both `char**` and `const char * const *`
 * cleanly. Since we already require `g_autoptr()`, there is not much
 * harm in also requiring statement-expressions.
 */
#define _gom_strv_contains(strv, str)                           \
  ((strv) != NULL && (str) != NULL &&                           \
   ({                                                           \
     gboolean _gom_strv_contains_found = FALSE;                 \
     for (guint _gom_strv_contains_i = 0;                       \
          (strv)[_gom_strv_contains_i] != NULL;                 \
          _gom_strv_contains_i++)                               \
       {                                                        \
         if (g_str_equal ((strv)[_gom_strv_contains_i], (str))) \
           {                                                    \
             _gom_strv_contains_found = TRUE;                   \
             break;                                             \
           }                                                    \
       }                                                        \
     _gom_strv_contains_found;                                  \
   }))

static inline gboolean
_gom_str_equal (const char *a,
                const char *b)
{
  return g_strcmp0 (a, b) == 0;
}

static inline gboolean
gom_str_empty0 (const char *str)
{
  return str == NULL || str[0] == 0;
}

static inline gboolean
_gom_set_strv (char               ***ptr,
               const char * const   *strv)
{
  char **copy;

  if ((const char * const *)*ptr == strv)
    return FALSE;

  if (*ptr && strv && _gom_strv_equal ((const char * const *)*ptr, strv))
    return FALSE;

  copy = g_strdupv ((char **)strv);
  g_strfreev (*ptr);
  *ptr = copy;

  return TRUE;
}

G_END_DECLS
