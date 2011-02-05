/* gom-query.h
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

#ifndef GOM_QUERY_H
#define GOM_QUERY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_TYPE_QUERY            (gom_query_get_type())
#define GOM_QUERY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_QUERY, GomQuery))
#define GOM_QUERY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_QUERY, GomQuery const))
#define GOM_QUERY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_QUERY, GomQueryClass))
#define GOM_IS_QUERY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_QUERY))
#define GOM_IS_QUERY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_QUERY))
#define GOM_QUERY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_QUERY, GomQueryClass))

typedef struct _GomQuery        GomQuery;
typedef struct _GomQueryClass   GomQueryClass;
typedef struct _GomQueryPrivate GomQueryPrivate;

struct _GomQuery
{
	GObject parent;

	/*< private >*/
	GomQueryPrivate *priv;
};

struct _GomQueryClass
{
	GObjectClass parent_class;
};

GType gom_query_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GOM_QUERY_H */
