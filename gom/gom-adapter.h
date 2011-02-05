/* gom-adapter.h
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

#ifndef GOM_ADAPTER_H
#define GOM_ADAPTER_H

#include <glib-object.h>

#include "gom-collection.h"
#include "gom-enumerable.h"
#include "gom-property.h"
#include "gom-property-set.h"
#include "gom-query.h"

G_BEGIN_DECLS

#define GOM_TYPE_ADAPTER            (gom_adapter_get_type())
#define GOM_ADAPTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ADAPTER, GomAdapter))
#define GOM_ADAPTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ADAPTER, GomAdapter const))
#define GOM_ADAPTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_ADAPTER, GomAdapterClass))
#define GOM_IS_ADAPTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_ADAPTER))
#define GOM_IS_ADAPTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_ADAPTER))
#define GOM_ADAPTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_ADAPTER, GomAdapterClass))

typedef struct _GomAdapter        GomAdapter;
typedef struct _GomAdapterClass   GomAdapterClass;
typedef struct _GomAdapterPrivate GomAdapterPrivate;

struct _GomAdapter
{
	GObject parent;

	/*< private >*/
	GomAdapterPrivate *priv;
};

struct _GomAdapterClass
{
	GObjectClass parent_class;

	gboolean (*create) (GomAdapter      *adapter,
	                    GomEnumerable   *enumerable,
	                    GError         **error);

	/*
	 * XXX:
	 *
	 * The query should have a set of properties for which it wants retrieved.
	 * The query may also specify the joining for tables if necessary.
	 *
	 * The result enumerable will need to support columns that map directly
	 * to the property offset in the query: ->get_value(iter, [0..idx], &val).
	 */
	gboolean (*read)   (GomAdapter      *adapter,
	                    GomQuery        *query,
	                    GomEnumerable  **results,
	                    GError         **error);

	gboolean (*update) (GomAdapter      *adapter,
	                    GomPropertySet  *properties,
	                    GValueArray     *values,
	                    GomCollection   *collection,
	                    GError         **error);

	gboolean (*delete) (GomAdapter      *adapter,
	                    GomCollection   *collection,
	                    GError         **error);
};

GType    gom_adapter_get_type      (void) G_GNUC_CONST;
gboolean gom_adapter_create        (GomAdapter      *adapter,
                                    GomEnumerable   *enumerable,
                                    GError         **error);

G_END_DECLS

#endif /* GOM_ADAPTER_H */
