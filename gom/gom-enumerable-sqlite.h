/* gom/gom-enumerable-sqlite.h
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

#ifndef GOM_ENUMERABLE_SQLITE_H
#define GOM_ENUMERABLE_SQLITE_H

#include <glib-object.h>

#include "gom-enumerable.h"

G_BEGIN_DECLS

#define GOM_TYPE_ENUMERABLE_SQLITE            (gom_enumerable_sqlite_get_type())
#define GOM_ENUMERABLE_SQLITE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ENUMERABLE_SQLITE, GomEnumerableSqlite))
#define GOM_ENUMERABLE_SQLITE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ENUMERABLE_SQLITE, GomEnumerableSqlite const))
#define GOM_ENUMERABLE_SQLITE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_ENUMERABLE_SQLITE, GomEnumerableSqliteClass))
#define GOM_IS_ENUMERABLE_SQLITE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_ENUMERABLE_SQLITE))
#define GOM_IS_ENUMERABLE_SQLITE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_ENUMERABLE_SQLITE))
#define GOM_ENUMERABLE_SQLITE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_ENUMERABLE_SQLITE, GomEnumerableSqliteClass))

typedef struct _GomEnumerableSqlite        GomEnumerableSqlite;
typedef struct _GomEnumerableSqliteClass   GomEnumerableSqliteClass;
typedef struct _GomEnumerableSqlitePrivate GomEnumerableSqlitePrivate;

struct _GomEnumerableSqlite
{
	GomEnumerable parent;

	/*< private >*/
	GomEnumerableSqlitePrivate *priv;
};

struct _GomEnumerableSqliteClass
{
	GomEnumerableClass parent_class;
};

GType gom_enumerable_sqlite_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GOM_ENUMERABLE_SQLITE_H */
