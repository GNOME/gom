/* gom-property.h
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

#ifndef GOM_PROPERTY_H
#define GOM_PROPERTY_H

#include <glib-object.h>

#include "gom-relationship.h"

G_BEGIN_DECLS

typedef struct _GomProperty      GomProperty;
typedef struct _GomPropertyValue GomPropertyValue;

struct _GomProperty
{
	GQuark          name;
	GType           owner_type;
	GType           value_type;
	GValue          default_value;
	gboolean        is_key;
	gboolean        is_serial;
	gboolean        is_eager;
	gboolean        is_unique;
	GomRelationship relationship;
};

struct _GomPropertyValue
{
	GQuark   name;
	GValue   value;
	gboolean is_dirty;
};

G_END_DECLS

#endif /* GOM_PROPERTY_H */
