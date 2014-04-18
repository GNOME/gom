/* gom-error.c
 *
 * Copyright (C) 2014 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gom-error.h"


G_DEFINE_QUARK("gom-error-quark", gom_error)

GType
gom_error_get_type (void)
{
   static GType g_type = 0;
   static gsize initialized = FALSE;
   static const GEnumValue values[] = {
       { GOM_ERROR_ADAPTER_OPEN, "GOM_ERROR_ADAPTER_OPEN", "ADAPTER_OPEN" },
       { GOM_ERROR_COMMAND_NO_SQL, "GOM_ERROR_COMMAND_NO_SQL", "COMMAND_NO_SQL" },
       { GOM_ERROR_COMMAND_SQLITE, "GOM_ERROR_COMMAND_SQLITE", "COMMAND_SQLITE" },
       { GOM_ERROR_REPOSITORY_EMPTY_RESULT, "GOM_ERROR_REPOSITORY_EMPTY_RESULT",
           "REPOSITORY_EMPTY_RESULT" },
       { GOM_ERROR_RESOURCE_CURSOR, "GOM_ERROR_RESOURCE_CURSOR", "RESOURCE_CURSOR" },
       { 0 }
   };

   if (g_once_init_enter(&initialized)) {
      g_type = g_enum_register_static("GomError", values);
      g_once_init_leave(&initialized, TRUE);
   }

   return g_type;
}
