/* gom-util.h
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

#ifndef GOM_UTIL_H
#define GOM_UTIL_H

#include <glib.h>

G_BEGIN_DECLS

static inline void
gom_clear_object (gpointer location)
{
	gpointer real_location;

	if (location) {
		real_location = *(gpointer *)location;
		if (real_location) {
			*(gpointer *)location = NULL;
			g_object_unref(real_location);
		}
	}
}

static inline void
gom_clear_pointer (gpointer location,
                   gpointer free_func)
{
	void (*free_func_real) (gpointer);
	gpointer real_location;

	if (location) {
		real_location = *(gpointer *)location;
		if (real_location) {
			*(gpointer *)location = NULL;
			free_func_real = free_func;
			free_func_real(real_location);
		}
	}
}

G_END_DECLS

#endif /* GOM_UTIL_H */
