/* gom-util.c
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

#include <gio/gio.h>
#include <string.h>

#include "gom-util-private.h"

char *
_gom_strv_to_text (const char * const *strv)
{
  g_autoptr(GString) str = g_string_new ("\n");
  gboolean had_items = FALSE;

  if (strv != NULL)
    {
      for (guint i = 0; strv[i] != NULL; i++)
        {
          const char *item = strv[i];

          had_items = TRUE;

          if (!gom_str_empty0 (item))
            {
              for (const char *iter = item; *iter; iter++)
                {
                  if (*iter == '\\')
                    g_string_append (str, "\\\\");
                  else if (*iter == '\n')
                    g_string_append (str, "\\n");
                  else
                    g_string_append_c (str, *iter);
                }
            }
          else
            {
              g_string_append_c (str, '\\');
            }

          g_string_append_c (str, '\n');
        }
    }

  if (!had_items)
    g_string_append_c (str, '\n');

  return g_string_free (g_steal_pointer (&str), FALSE);
}

char **
_gom_strv_from_text (const char  *text,
                     GError     **error)
{
  g_autoptr(GPtrArray) parts = NULL;
  g_autoptr(GString) current = NULL;
  g_autofree char *decoded = NULL;
  gsize len;

  if (text == NULL)
    return NULL;

  len = strlen (text);
  if (len == 0)
    return g_new0 (char *, 1);

  if (len < 2 || text[0] != '\n' || text[len - 1] != '\n')
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Invalid strv encoding");
      return NULL;
    }

  if (len == 2)
    return g_new0 (char *, 1);

  parts = g_ptr_array_new_with_free_func (g_free);
  current = g_string_new (NULL);

  for (gsize i = 1; i < len; i++)
    {
      char c = text[i];

      if (c == '\n')
        {
          if (current->len == 0)
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_DATA,
                                   "Invalid strv encoding");
              return NULL;
            }

          if (current->len == 1 && current->str[0] == '\\')
            g_ptr_array_add (parts, g_strdup (""));
          else
            {
              g_autoptr(GString) item = g_string_new (NULL);

              for (gsize j = 0; j < current->len; j++)
                {
                  char ch = current->str[j];

                  if (ch == '\\')
                    {
                      if (++j >= current->len)
                        {
                          g_set_error_literal (error,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_DATA,
                                               "Invalid strv escape sequence");
                          return NULL;
                        }

                      ch = current->str[j];
                      if (ch == '\\')
                        g_string_append_c (item, '\\');
                      else if (ch == 'n')
                        g_string_append_c (item, '\n');
                      else
                        {
                          g_set_error_literal (error,
                                               G_IO_ERROR,
                                               G_IO_ERROR_INVALID_DATA,
                                               "Invalid strv escape sequence");
                          return NULL;
                        }
                    }
                  else
                    {
                      g_string_append_c (item, ch);
                    }
                }

              decoded = g_string_free (g_steal_pointer (&item), FALSE);
              g_ptr_array_add (parts, g_steal_pointer (&decoded));
            }

          g_string_set_size (current, 0);
          continue;
        }

      g_string_append_c (current, c);
    }

  if (current->len != 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Invalid strv encoding");
      return NULL;
    }

  g_ptr_array_add (parts, NULL);
  return (char **)g_ptr_array_free (g_steal_pointer (&parts), FALSE);
}
