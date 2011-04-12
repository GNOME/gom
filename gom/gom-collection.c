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

	GPtrArray *to_add;
	GPtrArray *to_remove;
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
 * gom_collection_save:
 * @collection: (in): A #GomCollection.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Saves the items within the collection. Currently, this is only
 * implemented for many-to-many collections.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 * Side effects: None.
 */
gboolean
gom_collection_save (GomCollection  *collection,
                     GError        **error)
{
	GomCollectionPrivate *priv;
	GomResource *resource;
	gboolean ret = TRUE;
	gint i;

	g_return_val_if_fail(GOM_IS_COLLECTION(collection), FALSE);

	priv = collection->priv;

	if (!priv->adapter) {
		g_set_error(error, GOM_COLLECTION_ERROR, GOM_COLLECTION_ERROR_ADAPTER,
					_("No adapter available for collection storage."));
		return FALSE;
	}

	if (!priv->query) {
		g_set_error(error, GOM_COLLECTION_ERROR, GOM_COLLECTION_ERROR_QUERY,
		            _("No query available for collection storage."));
		return FALSE;
	}

	if (priv->to_add) {
		for (i = 0; i < priv->to_add->len; i++) {
			resource = g_ptr_array_index(priv->to_add, i);
			if (!gom_resource_save(resource, error)) {
				return FALSE;
			}
		}
		gom_clear_pointer(&priv->to_add, g_ptr_array_unref);
	}

	if (priv->to_remove) {
		for (i = 0; i < priv->to_remove->len; i++) {
			resource = g_ptr_array_index(priv->to_remove, i);
			/*
			 * XXX: Need to remove the items. Probably with a
			 *      temporary collection with removed items.
			 */
			g_assert_not_reached();
		}
		gom_clear_pointer(&priv->to_remove, g_ptr_array_unref);
	}

	return ret;
}

/**
 * gom_collection_add_resource:
 * @collection: (in): A #GomCollection.
 *
 * Adds a resource to a collection. When the collection is saved, the resource
 * will be joined to the parent resource through the many-to-many table (when
 * used with SQLite).
 *
 * Returns: None.
 * Side effects: A reference to resource is taken.
 */
void
gom_collection_add_resource (GomCollection *collection,
							 gpointer       resource)
{
	GomCollectionPrivate *priv;

	g_return_if_fail(GOM_IS_COLLECTION(collection));
	g_return_if_fail(GOM_IS_RESOURCE(resource));

	priv = collection->priv;

	if (!priv->to_add) {
		priv->to_add = g_ptr_array_new_with_free_func(g_object_unref);
	}
	g_ptr_array_add(priv->to_add, g_object_ref(resource));
}

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
	gom_clear_pointer(&priv->to_add, g_ptr_array_unref);
	gom_clear_pointer(&priv->to_remove, g_ptr_array_unref);

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

static gboolean
gom_collection_consistent (GomCollection *collection)
{
	if (!collection->priv->query) {
		g_critical("GomCollection  has no query!");
		return FALSE;
	}

	if (!collection->priv->adapter) {
		g_critical("GomCollection  has no adapter!");
		return FALSE;
	}

	return TRUE;
}

/**
 * gom_collection_count:
 * @collection: (in): A #GomCollection.
 *
 * Retrieves the number of resources that match the query for @collection.
 *
 * Returns: A #guint64 containing the resource count.
 * Side effects: None.
 */
guint64
gom_collection_count (GomCollection *collection)
{
	GomCollectionPrivate *priv;
	GomEnumerableIter iter;
	GomEnumerable *enumerable =  NULL;
	GomQuery *query = NULL;
	guint64 offset = 0;
	guint64 limit = 0;
	guint64 ret = 0;
	GValue value = { 0 };
	GError *error = NULL;

	g_return_val_if_fail(GOM_IS_COLLECTION(collection), 0);
	g_return_val_if_fail(gom_collection_consistent(collection), 0);

	priv = collection->priv;

	query = gom_query_dup(priv->query);
	g_object_set(query,
	             "count-only", TRUE,
	             NULL);
	g_object_get(query,
	             "limit", &limit,
	             "offset", &offset,
	             NULL);

	if (!gom_adapter_read(priv->adapter, query, &enumerable, &error)) {
		g_critical("%s", error->message);
		g_clear_error(&error);
		goto failure;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		g_value_init(&value, G_TYPE_UINT64);
		gom_enumerable_get_value(enumerable, &iter, 0, &value);
		ret = g_value_get_uint64(&value);
		g_value_unset(&value);
	} else {
		g_critical("No result from adapter");
	}

	if (limit) {
		ret = MIN(limit, (ret - offset));
	} else if (offset) {
		ret = MAX(0, ((gint64)ret - (gint64)offset));
	}

failure:
	gom_clear_object(&enumerable);
	gom_clear_object(&query);

	return ret;
}

static GomResource*
gom_collection_build_resource (GomCollection     *collection,
                               GomEnumerable     *enumerable,
                               GomEnumerableIter *iter,
                               GomQuery          *query)
{
	GomCollectionPrivate *priv;
	GomPropertySet *fields = NULL;
	GomProperty *field;
	GomResource *ret = NULL;
	GParameter param;
	GArray *parameters = NULL;
	GType resource_type = 0;
	guint n_fields = 0;
	gint i;

	g_return_val_if_fail(GOM_IS_COLLECTION(collection), NULL);
	g_return_val_if_fail(GOM_IS_ENUMERABLE(enumerable), NULL);
	g_return_val_if_fail(iter != NULL, NULL);
	g_return_val_if_fail(GOM_IS_QUERY(query), NULL);

	priv = collection->priv;

	g_object_get(query,
	             "fields", &fields,
	             "resource-type", &resource_type,
	             NULL);

	if (!fields || !g_type_is_a(resource_type, GOM_TYPE_RESOURCE)) {
		goto failure;
	}

	n_fields = gom_property_set_length(fields);
	parameters = g_array_sized_new(FALSE, FALSE, sizeof(GParameter), n_fields + 1);

	for (i = 0; i < n_fields; i++) {
		field = gom_property_set_get_nth(fields, i);
		memset(&param, 0, sizeof param);
		param.name = g_quark_to_string(field->name);
		g_value_init(&param.value, field->value_type);
		gom_enumerable_get_value(enumerable, iter, i, &param.value);
		g_array_append_val(parameters, param);
		if (!gom_enumerable_iter_next(iter)) {
			break;
		}
	}

	memset(&param, 0, sizeof param);
	param.name = "adapter";
	g_value_init(&param.value, GOM_TYPE_ADAPTER);
	g_value_set_object(&param.value, priv->adapter);
	g_array_append_val(parameters, param);

	ret = g_object_newv(resource_type,
	                    parameters->len,
	                    (GParameter *)parameters->data);

  failure:
	if (parameters) {
		for (i = 0; i < parameters->len; i++) {
			param = g_array_index(parameters, GParameter, i);
			g_value_unset(&param.value);
		}
		g_array_free(parameters, TRUE);
	}

	gom_clear_pointer(&fields, gom_property_set_unref);

	return ret;
}

static gpointer
gom_collection_first_with_reverse (GomCollection *collection,
                                   gboolean       reverse)
{
	GomCollectionPrivate *priv;
	GomEnumerableIter iter;
	GomEnumerable *enumerable =  NULL;
	GomResource *ret = NULL;
	GomQuery *query = NULL;
	GError *error = NULL;

	g_return_val_if_fail(GOM_IS_COLLECTION(collection), 0);
	g_return_val_if_fail(gom_collection_consistent(collection), 0);

	priv = collection->priv;

	query = gom_query_dup(priv->query);
	g_object_set(query,
	             "limit", G_GUINT64_CONSTANT(1),
	             "reverse", reverse,
	             NULL);

	if (!gom_adapter_read(priv->adapter, query, &enumerable, &error)) {
		g_critical("%s", error->message);
		g_clear_error(&error);
		goto failure;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		ret = gom_collection_build_resource(collection, enumerable,
		                                    &iter, query);
	}

failure:
	gom_clear_object(&enumerable);
	gom_clear_object(&query);

	return ret;
}

gpointer
gom_collection_first (GomCollection *collection)
{
	return gom_collection_first_with_reverse(collection, FALSE);
}

gpointer
gom_collection_last (GomCollection *collection)
{
	return gom_collection_first_with_reverse(collection, TRUE);
}

GomCollection*
gom_collection_slice (GomCollection *collection,
                      gint64         begin,
                      gint64         end)
{
	GomCollectionPrivate *priv;
	GomCollection *ret = NULL;
	GomQuery *query = NULL;
	guint64 limit = 0;
	guint64 offset = 0;

	g_return_val_if_fail(GOM_IS_COLLECTION(collection), NULL);
	g_return_val_if_fail(begin > 0, NULL);
	g_return_val_if_fail(begin < end || end == -1, NULL);

	priv = collection->priv;

	query = gom_query_dup(priv->query);

	g_object_get(query,
	             "limit", &limit,
	             "offset", &offset,
	             NULL);

	if (begin) {
		offset += begin;
	}

	if (end == -1) {
		limit = 0;
	} else {
		limit = end - begin;
	}

	g_object_set(query,
	             "limit", limit,
	             "offset", offset,
	             NULL);

	ret = g_object_new(GOM_TYPE_COLLECTION,
	                   "adapter", priv->adapter,
	                   "query", query,
	                   NULL);

	gom_clear_object(&query);

	return ret;
}

gpointer
gom_collection_get_nth (GomCollection *collection,
                        guint64        nth)
{
	GomCollectionPrivate *priv;
	GomEnumerableIter iter;
	GomEnumerable *enumerable =  NULL;
	GomResource *ret = NULL;
	GomQuery *query = NULL;
	guint64 offset = 0;
	GError *error = NULL;

	g_return_val_if_fail(GOM_IS_COLLECTION(collection), 0);
	g_return_val_if_fail(gom_collection_consistent(collection), 0);

	priv = collection->priv;

	query = gom_query_dup(priv->query);
	g_object_get(query,
	             "offset", &offset,
	             NULL);
	offset += nth;
	g_object_set(query,
	             "limit", G_GUINT64_CONSTANT(1),
	             "offset", offset,
	             NULL);

	if (!gom_adapter_read(priv->adapter, query, &enumerable, &error)) {
		g_critical("%s", error->message);
		g_clear_error(&error);
		goto failure;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		ret = gom_collection_build_resource(collection, enumerable,
		                                    &iter, query);
	}

  failure:
	gom_clear_object(&enumerable);
	gom_clear_object(&query);

	return ret;
}

/**
 * gom_collection_error_quark:
 *
 * Error quark for #GomCollection.
 *
 * Returns: A #GQuark.
 * Side effects: None.
 */
GQuark
gom_collection_error_quark (void)
{
	return g_quark_from_static_string("gom-collection-error-quark");
}
