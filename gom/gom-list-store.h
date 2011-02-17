/* gom/gom-list-store.h
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

#ifndef GOM_LIST_STORE_H
#define GOM_LIST_STORE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GOM_TYPE_LIST_STORE            (gom_list_store_get_type())
#define GOM_LIST_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_LIST_STORE, GomListStore))
#define GOM_LIST_STORE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_LIST_STORE, GomListStore const))
#define GOM_LIST_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_LIST_STORE, GomListStoreClass))
#define GOM_IS_LIST_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_LIST_STORE))
#define GOM_IS_LIST_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_LIST_STORE))
#define GOM_LIST_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_LIST_STORE, GomListStoreClass))

typedef struct _GomListStore        GomListStore;
typedef struct _GomListStoreClass   GomListStoreClass;
typedef struct _GomListStorePrivate GomListStorePrivate;

struct _GomListStore
{
	GObject parent;

	/*< private >*/
	GomListStorePrivate *priv;
};

struct _GomListStoreClass
{
	GObjectClass parent_class;
};

GType gom_list_store_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GOM_LIST_STORE_H */
