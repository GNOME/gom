/* gom-collection.h
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

#ifndef GOM_COLLECTION_H
#define GOM_COLLECTION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_TYPE_COLLECTION            (gom_collection_get_type())
#define GOM_COLLECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_COLLECTION, GomCollection))
#define GOM_COLLECTION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_COLLECTION, GomCollection const))
#define GOM_COLLECTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_COLLECTION, GomCollectionClass))
#define GOM_IS_COLLECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_COLLECTION))
#define GOM_IS_COLLECTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_COLLECTION))
#define GOM_COLLECTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_COLLECTION, GomCollectionClass))

typedef struct _GomCollection        GomCollection;
typedef struct _GomCollectionClass   GomCollectionClass;
typedef struct _GomCollectionPrivate GomCollectionPrivate;

struct _GomCollection
{
	GObject parent;

	/*< private >*/
	GomCollectionPrivate *priv;
};

struct _GomCollectionClass
{
	GObjectClass parent_class;
};

GType          gom_collection_get_type (void) G_GNUC_CONST;
guint64        gom_collection_count    (GomCollection *collection);
gpointer       gom_collection_first    (GomCollection *collection);
gpointer       gom_collection_last     (GomCollection *collection);
GomCollection* gom_collection_slice    (GomCollection *collection,
                                        gint64         begin,
                                        gint64         end);

G_END_DECLS

#endif /* GOM_COLLECTION_H */
