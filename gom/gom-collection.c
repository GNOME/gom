/* gom-collection.c
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

#include "gom-collection.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomCollection, gom_collection, G_TYPE_OBJECT)

struct _GomCollectionPrivate
{
	GomQuery *query;
};

/**
 * gom_collection_finalize:
 * @object: (in): A #GomCollection.
 *
 * Finalizer for a #GomCollection instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_collection_finalize (GObject *object)
{
	GomCollectionPrivate *priv = GOM_COLLECTION(object)->priv;

	gom_clear_object(&priv->query);

	G_OBJECT_CLASS(gom_collection_parent_class)->finalize(object);
}

/**
 * gom_collection_class_init:
 * @klass: (in): A #GomCollectionClass.
 *
 * Initializes the #GomCollectionClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_collection_class_init (GomCollectionClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_collection_finalize;
	g_type_class_add_private(object_class, sizeof(GomCollectionPrivate));
}

/**
 * gom_collection_init:
 * @collection: (in): A #GomCollection.
 *
 * Initializes the newly created #GomCollection instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_collection_init (GomCollection *collection)
{
	collection->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(collection,
		                            GOM_TYPE_COLLECTION,
		                            GomCollectionPrivate);
}
