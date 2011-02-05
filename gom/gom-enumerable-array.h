/* gom/gom-enumerable-array.h
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

#ifndef GOM_ENUMERABLE_ARRAY_H
#define GOM_ENUMERABLE_ARRAY_H

#include <glib-object.h>

#include "gom-enumerable.h"
#include "gom-resource.h"

G_BEGIN_DECLS

#define GOM_TYPE_ENUMERABLE_ARRAY            (gom_enumerable_array_get_type())
#define GOM_ENUMERABLE_ARRAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ENUMERABLE_ARRAY, GomEnumerableArray))
#define GOM_ENUMERABLE_ARRAY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ENUMERABLE_ARRAY, GomEnumerableArray const))
#define GOM_ENUMERABLE_ARRAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_ENUMERABLE_ARRAY, GomEnumerableArrayClass))
#define GOM_IS_ENUMERABLE_ARRAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_ENUMERABLE_ARRAY))
#define GOM_IS_ENUMERABLE_ARRAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_ENUMERABLE_ARRAY))
#define GOM_ENUMERABLE_ARRAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_ENUMERABLE_ARRAY, GomEnumerableArrayClass))

typedef struct _GomEnumerableArray        GomEnumerableArray;
typedef struct _GomEnumerableArrayClass   GomEnumerableArrayClass;
typedef struct _GomEnumerableArrayPrivate GomEnumerableArrayPrivate;

struct _GomEnumerableArray
{
	GomEnumerable parent;

	/*< private >*/
	GomEnumerableArrayPrivate *priv;
};

struct _GomEnumerableArrayClass
{
	GomEnumerableClass parent_class;
};

GType          gom_enumerable_array_get_type (void) G_GNUC_CONST;
GomEnumerable* gom_enumerable_array_new      (GomResource **resources,
                                              guint         n_resources);

G_END_DECLS

#endif /* GOM_ENUMERABLE_ARRAY_H */
