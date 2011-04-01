/* gom-util.c
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
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

#include <glib-object.h>
#include <string.h>

#include "gom-util.h"

/**
 * gom_clear_object:
 * @location: (out): A pointer to a #GObject.
 *
 * Clears a object by calling g_object_unref() if the object is non-%NULL.
 * The object is cleared before free'ing to be more friendly with circular
 * referencing.
 *
 * Returns: None.
 */
void
gom_clear_object (gpointer location)
{
	gpointer real_location;

	if (location) {
		real_location = *(gpointer *)location;
		if (real_location) {
			memset(location, 0, sizeof(gpointer));
			g_object_unref(real_location);
		}
	}
}

/**
 * gom_clear_pointer:
 * @location: (out): A pointer to the gpointer to clear.
 * @free_func: (out): A GDestroyNotify to free the pointer.
 *
 * Clears a pointer by calling @free_func if the pointer is non-%NULL.
 * The gpointer is cleared before free'ing to be more friendly with
 * circular referencing.
 *
 * Returns: None.
 */
void
gom_clear_pointer (gpointer location,
                   gpointer free_func)
{
	void (*free_func_real) (gpointer) = free_func;
	gpointer real_location;

	if (location) {
		real_location = *(gpointer *)location;
		if (real_location) {
			memset(location, 0, sizeof(gpointer));
			free_func_real(real_location);
		}
	}
}
