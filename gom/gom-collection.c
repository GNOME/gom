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

#include <glib/gi18n.h>
#include <string.h>

#include "gom-adapter.h"
#include "gom-collection.h"
#include "gom-query.h"
#include "gom-resource.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomCollection, gom_collection, G_TYPE_OBJECT)

struct _GomCollectionPrivate
{
	GomAdapter *adapter;
	GomQuery *query;
};

enum
{
	PROP_0,
	PROP_ADAPTER,
	PROP_QUERY,
	LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

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
	gom_clear_object(&priv->adapter);

	G_OBJECT_CLASS(gom_collection_parent_class)->finalize(object);
}

/**
 * gom_collection_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_collection_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	GomCollection *collection = GOM_COLLECTION(object);

	switch (prop_id) {
	case PROP_ADAPTER:
		g_value_set_object(value, collection->priv->adapter);
		break;
	case PROP_QUERY:
		g_value_set_object(value, collection->priv->query);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_collection_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_collection_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	GomCollection *collection = GOM_COLLECTION(object);

	switch (prop_id) {
	case PROP_ADAPTER:
		gom_clear_object(&collection->priv->adapter);
		collection->priv->adapter = g_value_dup_object(value);
		break;
	case PROP_QUERY:
		gom_clear_object(&collection->priv->query);
		collection->priv->query = g_value_dup_object(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
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
	object_class->get_property = gom_collection_get_property;
	object_class->set_property = gom_collection_set_property;
	g_type_class_add_private(object_class, sizeof(GomCollectionPrivate));

	gParamSpecs[PROP_QUERY] =
		g_param_spec_object("query",
		                    _("Query"),
		                    _("The query of the collection"),
		                    GOM_TYPE_QUERY,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_QUERY,
	                                gParamSpecs[PROP_QUERY]);

	gParamSpecs[PROP_ADAPTER] =
		g_param_spec_object("adapter",
		                    _("Adapter"),
		                    _("The collections adapter."),
		                    GOM_TYPE_ADAPTER,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_ADAPTER,
	                                gParamSpecs[PROP_ADAPTER]);
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
