/* gom-enumerable.h
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

#ifndef GOM_ENUMERABLE_H
#define GOM_ENUMERABLE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_TYPE_ENUMERABLE            (gom_enumerable_get_type())
#define GOM_ENUMERABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ENUMERABLE, GomEnumerable))
#define GOM_ENUMERABLE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ENUMERABLE, GomEnumerable const))
#define GOM_ENUMERABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_ENUMERABLE, GomEnumerableClass))
#define GOM_IS_ENUMERABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_ENUMERABLE))
#define GOM_IS_ENUMERABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_ENUMERABLE))
#define GOM_ENUMERABLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_ENUMERABLE, GomEnumerableClass))

typedef struct _GomEnumerable        GomEnumerable;
typedef struct _GomEnumerableClass   GomEnumerableClass;
typedef struct _GomEnumerablePrivate GomEnumerablePrivate;
typedef struct _GomEnumerableIter    GomEnumerableIter;

struct _GomEnumerableIter
{
	/*< public >*/
	GomEnumerable *enumerable;

	/*< private >*/
	union {
		gint v_int;
		guint v_uint;
		gint64 v_int64;
		guint64 v_uint64;
		gboolean v_boolean;
		gpointer v_pointer;
	} data[4];
};

struct _GomEnumerable
{
	GObject parent;

	/*< private >*/
	GomEnumerablePrivate *priv;
};

struct _GomEnumerableClass
{
	GObjectClass parent_class;

	gboolean (*iter_init)     (GomEnumerable     *enumerable,
	                           GomEnumerableIter *iter);

	gboolean (*iter_next)     (GomEnumerable     *enumerable,
	                           GomEnumerableIter *iter);

	void     (*get_value)     (GomEnumerable     *enumerable,
	                           GomEnumerableIter *iter,
	                           gint               column,
	                           GValue            *value);
	guint    (*get_n_columns) (GomEnumerable     *enumerable);
};

GType    gom_enumerable_get_type      (void) G_GNUC_CONST;
gboolean gom_enumerable_iter_init     (GomEnumerableIter *iter,
                                       GomEnumerable     *enumerable);
gboolean gom_enumerable_iter_next     (GomEnumerableIter *iter);
void     gom_enumerable_get_value     (GomEnumerable     *enumerable,
                                       GomEnumerableIter *iter,
                                       guint              column,
                                       GValue            *value);
guint    gom_enumerable_get_n_columns (GomEnumerable     *enumerable);

G_END_DECLS

#endif /* GOM_ENUMERABLE_H */
