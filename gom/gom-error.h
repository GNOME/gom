/* gom-filter.h
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

#ifndef GOM_ERROR_H
#define GOM_ERROR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_ERROR                  (gom_error_quark())

typedef enum _GomError GomError;

enum _GomError
{
    GOM_ERROR_ADAPTER_OPEN,
    GOM_ERROR_COMMAND_NO_SQL,
    GOM_ERROR_COMMAND_SQLITE,
    GOM_ERROR_REPOSITORY_EMPTY_RESULT,
    GOM_ERROR_RESOURCE_CURSOR,
    GOM_ERROR_COMMAND_NO_REPOSITORY
};

GQuark    gom_error_quark    (void) G_GNUC_CONST;
GType     gom_error_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
