/* gom-module-private.h
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
#include <gmodule.h>

G_BEGIN_DECLS

typedef struct
{
  const char *name;
  gpointer   *storage;
} GomModuleSymbol;

#define GOM_MODULE_SYMBOL_TYPE(symbol) __typeof__ (&symbol)
#define GOM_MODULE_SYMBOL_DECLARE(symbol) extern GOM_MODULE_SYMBOL_TYPE (symbol) _GOM_##symbol;
#define GOM_MODULE_SYMBOL_DEFINE(symbol) GOM_MODULE_SYMBOL_TYPE (symbol) _GOM_##symbol = NULL;
#define GOM_MODULE_SYMBOL_ARRAY_ENTRY(symbol) { #symbol, (gpointer *) &_GOM_##symbol },

static inline gboolean
gom_module_load_symbol (GModule               *module,
                        const GomModuleSymbol *symbol)
{
  g_return_val_if_fail (module != NULL, FALSE);
  g_return_val_if_fail (symbol != NULL, FALSE);
  g_return_val_if_fail (symbol->name != NULL, FALSE);
  g_return_val_if_fail (symbol->storage != NULL, FALSE);

  return g_module_symbol (module, symbol->name, symbol->storage);
}

static inline gboolean
gom_module_load_symbols (GModule                *module,
                         const GomModuleSymbol  *symbols,
                         const char             *module_name,
                         GQuark                  error_domain,
                         gint                    error_code,
                         GError                **error)
{
  g_return_val_if_fail (module != NULL, FALSE);
  g_return_val_if_fail (symbols != NULL, FALSE);
  g_return_val_if_fail (module_name != NULL, FALSE);

  for (guint i = 0; symbols[i].name != NULL; i++)
    {
      if (!gom_module_load_symbol (module, &symbols[i]))
        {
          g_set_error (error,
                       error_domain,
                       error_code,
                       "Failed to load %s symbol %s: %s",
                       module_name,
                       symbols[i].name,
                       g_module_error ());
          return FALSE;
        }
    }

  return TRUE;
}

G_END_DECLS
