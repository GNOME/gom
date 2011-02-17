/* gom/gom-list-store.c
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

#include "gom-adapter.h"
#include "gom-list-store.h"
#include "gom-query.h"
#include "gom-util.h"

/*
 * Structures and enums.
 */

struct _GomListStorePrivate
{
	GomAdapter *adapter;
	GomQuery *query;
	guint stamp;
	gint n_children;
};

enum
{
	PROP_0,
	PROP_ADAPTER,
	PROP_QUERY,
	LAST_PROP
};

/*
 * Forward delcarations.
 */

static void gom_list_store_finalize        (GObject *object);
static void gom_list_store_get_property    (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec);
static gint gom_list_store_get_n_columns   (GtkTreeModel *tree_model);
static void gtk_tree_model_init            (GtkTreeModelIface *iface);
static gint gom_list_store_iter_n_children (GtkTreeModel *tree_model,
                                            GtkTreeIter *tree_iter);
static void gom_list_store_set_property    (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec);

/*
 * Globals.
 */

static GParamSpec *gParamSpecs[LAST_PROP];

G_DEFINE_TYPE_EXTENDED(GomListStore, gom_list_store, G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                             gtk_tree_model_init))

/**
 * gom_list_store_class_init:
 * @klass: (in): A #GomListStoreClass.
 *
 * Initializes the #GomListStoreClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_list_store_class_init (GomListStoreClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_list_store_finalize;
	object_class->get_property = gom_list_store_get_property;
	object_class->set_property = gom_list_store_set_property;
	g_type_class_add_private(object_class, sizeof(GomListStorePrivate));

	gParamSpecs[PROP_ADAPTER] =
		g_param_spec_object("adapter",
		                    _("Adapter"),
		                    _("The adapter in which to request results."),
		                    GOM_TYPE_ADAPTER,
		                    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_ADAPTER,
	                                gParamSpecs[PROP_ADAPTER]);

	gParamSpecs[PROP_QUERY] =
		g_param_spec_object("query",
		                    _("Query"),
		                    _("The query to perform on adapter."),
		                    GOM_TYPE_QUERY,
		                    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_QUERY,
	                                gParamSpecs[PROP_QUERY]);
}

/**
 * gom_list_store_finalize:
 * @object: (in): A #GomListStore.
 *
 * Finalizer for a #GomListStore instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_list_store_finalize (GObject *object)
{
	G_OBJECT_CLASS(gom_list_store_parent_class)->finalize(object);
}

static GType
gom_list_store_get_column_type (GtkTreeModel *tree_model,
                                gint index_)
{
	GomListStorePrivate *priv;
	GomPropertySet *fields = NULL;
	GomListStore *store = (GomListStore *)tree_model;
	GomProperty *field;
	GType ret = G_TYPE_INVALID;

	g_return_val_if_fail(GOM_IS_LIST_STORE(store), G_TYPE_INVALID);
	g_return_val_if_fail(store->priv->adapter != NULL, G_TYPE_INVALID);
	g_return_val_if_fail(store->priv->query != NULL, G_TYPE_INVALID);

	priv = store->priv;

	g_object_get(priv->query,
	             "fields", &fields,
	             NULL);

	if (index_ < fields->len) {
		field = gom_property_set_get_nth(fields, index_);
		ret = field->value_type;
	} else {
		g_critical("index %d > n_columns %d", index_, fields->len);
	}

	gom_clear_pointer(&fields, gom_property_set_unref);

	return ret;
}

static GtkTreeModelFlags
gom_list_store_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gboolean
gom_list_store_get_iter (GtkTreeModel *tree_model,
                         GtkTreeIter  *iter,
                         GtkTreePath  *path)
{
	GomListStorePrivate *priv;
	GomListStore *store = (GomListStore *)tree_model;
	gint *indices;
	gint n_children;

	g_return_val_if_fail(GOM_IS_LIST_STORE(store), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(path != NULL, FALSE);

	priv = store->priv;

	if (gtk_tree_path_get_depth(path) != 1) {
		return FALSE;
	}

	indices = gtk_tree_path_get_indices(path);
	n_children = gom_list_store_iter_n_children(tree_model, NULL);

	iter->stamp = priv->stamp;
	iter->user_data = GINT_TO_POINTER(indices[0]);
	iter->user_data2 = GINT_TO_POINTER(n_children);

	return iter->user_data < iter->user_data2;
}

static gint
gom_list_store_get_n_columns (GtkTreeModel *tree_model)
{
	GomListStorePrivate *priv;
	GomPropertySet *fields = NULL;
	GomListStore *store = (GomListStore *)tree_model;
	gint ret = 0;

	g_return_val_if_fail(GOM_IS_LIST_STORE(store), -1);
	g_return_val_if_fail(store->priv->adapter != NULL, -1);
	g_return_val_if_fail(store->priv->query != NULL, -1);

	priv = store->priv;

	g_object_get(priv->query, "fields", &fields, NULL);
	ret = fields->len;
	gom_property_set_unref(fields);

	return ret;
}

static GtkTreePath *
gom_list_store_get_path (GtkTreeModel *tree_model,
                         GtkTreeIter  *tree_iter)
{
	gint index_;

	g_return_val_if_fail(GOM_IS_LIST_STORE(tree_model), NULL);
	g_return_val_if_fail(tree_iter != NULL, NULL);

	index_ = GPOINTER_TO_INT(tree_iter->user_data);
	return gtk_tree_path_new_from_indices(index_, -1);
}

/**
 * gom_list_store_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_list_store_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	GomListStore *store = GOM_LIST_STORE(object);

	switch (prop_id) {
	case PROP_ADAPTER:
		g_value_set_object(value, store->priv->adapter);
		break;
	case PROP_QUERY:
		g_value_set_object(value, store->priv->query);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
gom_list_store_get_value (GtkTreeModel *tree_model,
                          GtkTreeIter  *tree_iter,
                          gint          column,
                          GValue       *value)
{
	GomListStorePrivate *priv;
	GomEnumerableIter iter;
	GomEnumerable *enumerable = NULL;
	GomListStore *store = (GomListStore *)tree_model;
	GomPropertySet *fields = NULL;
	GomProperty *field;
	GomQuery *query = NULL;
	guint64 offset = 0;
	GError *error = NULL;

	g_return_if_fail(GOM_IS_LIST_STORE(store));
	g_return_if_fail(store->priv->adapter != NULL);
	g_return_if_fail(store->priv->query != NULL);
	g_return_if_fail(tree_iter != NULL);
	g_return_if_fail(value != NULL);
	g_return_if_fail(tree_iter->stamp == store->priv->stamp);

	priv = store->priv;

	/*
	 * TODO: - We can reduce fields to just the desired one.
	 *       - I don't really like that get_value requires an object creation.
	 *         Well really, two since you have to have the enumerable too.
	 */

	query = gom_query_dup(priv->query);
	g_object_get(query,
	             "fields", &fields,
	             "offset", &offset,
	             NULL);

	if (column >= fields->len) {
		goto failure;
	}

	field = gom_property_set_get_nth(fields, column);
	if (!G_IS_VALUE(value)) {
		g_value_init(value, field->value_type);
	}

	gom_property_set_unref(fields);
	fields = gom_property_set_newv(1, &field);

	offset += GPOINTER_TO_INT(tree_iter->user_data);
	g_object_set(query,
	             "fields", fields,
	             "offset", offset,
	             "limit", G_GUINT64_CONSTANT(1),
	             NULL);

	if (!gom_adapter_read(priv->adapter, query, &enumerable, &error)) {
		g_critical("%s", error->message);
		g_error_free(error);
		goto failure;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		gom_enumerable_get_value(enumerable, &iter, 0, value);
	}

  failure:
	gom_clear_object(&query);
	gom_clear_object(&enumerable);
	gom_clear_pointer(&fields, gom_property_set_unref);
}

/**
 * gom_list_store_init:
 * @store: (in): A #GomListStore.
 *
 * Initializes the newly created #GomListStore instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_list_store_init (GomListStore *store)
{
	store->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(store,
		                            GOM_TYPE_LIST_STORE,
		                            GomListStorePrivate);
	store->priv->stamp = g_random_int();
}

static gboolean
gom_list_store_iter_children (GtkTreeModel *tree_model,
                              GtkTreeIter  *iter,
                              GtkTreeIter  *parent)
{
	GomListStorePrivate *priv;
	GomListStore *store = (GomListStore *)tree_model;
	gint n_children;

	g_return_val_if_fail(GOM_IS_LIST_STORE(store), FALSE);
	g_return_val_if_fail(store->priv->adapter != NULL, FALSE);
	g_return_val_if_fail(store->priv->query != NULL, FALSE);

	priv = store->priv;

	if (parent) {
		g_warning("GomListStore does not support children items.");
		return FALSE;
	}

	n_children = gom_list_store_iter_n_children(tree_model, NULL);
	iter->stamp = priv->stamp;
	iter->user_data = NULL;
	iter->user_data2 = GINT_TO_POINTER(n_children);

	return iter->user_data < iter->user_data2;
}

static gboolean
gom_list_store_iter_has_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *tree_iter)
{
	return FALSE;
}

static gint
gom_list_store_iter_n_children (GtkTreeModel *tree_model,
                                GtkTreeIter  *tree_iter)
{
	GomListStorePrivate *priv;
	GomEnumerableIter iter;
	GomEnumerable *enumerable = NULL;
	GomListStore *store = (GomListStore *)tree_model;
	GomQuery *query;
	GError *error = NULL;
	GValue value = { 0 };
	gint ret = 0;

	g_return_val_if_fail(GOM_IS_LIST_STORE(store), 0);
	g_return_val_if_fail(store->priv->adapter != NULL, 0);
	g_return_val_if_fail(store->priv->query != NULL, 0);

	priv = store->priv;

	if (tree_iter) {
		return 0;
	}

	if (priv->n_children) {
		return priv->n_children;
	}

	query = gom_query_dup(priv->query);

	g_object_set(query,
	             "count-only", TRUE,
	             NULL);

	if (!gom_adapter_read(priv->adapter, query, &enumerable, &error)) {
		g_critical("%s", error->message);
		g_error_free(error);
		goto failure;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		g_value_init(&value, G_TYPE_INT64);
		gom_enumerable_get_value(enumerable, &iter, 0, &value);
		ret = g_value_get_int64(&value);
		g_value_unset(&value);
	}

	priv->n_children = ret;

  failure:
	gom_clear_object(&query);
	gom_clear_object(&enumerable);

	return ret;
}

static gboolean
gom_list_store_iter_next (GtkTreeModel *tree_model,
                          GtkTreeIter  *tree_iter)
{
	tree_iter->user_data++;
	return tree_iter->user_data < tree_iter->user_data2;
}

static gboolean
gom_list_store_iter_nth_child (GtkTreeModel *tree_model,
                               GtkTreeIter  *iter,
                               GtkTreeIter  *parent,
                               gint          n)
{
	if (parent) {
		return FALSE;
	}
	iter->user_data = GINT_TO_POINTER(n);
	return iter->user_data < iter->user_data2;
}

static gboolean
gom_list_store_iter_parent (GtkTreeModel *tree_model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *child)
{
	return FALSE;
}

static gboolean
gom_list_store_iter_previous (GtkTreeModel *tree_model,
                              GtkTreeIter  *tree_iter)
{
	if (!tree_iter->user_data) {
		return FALSE;
	}
	return --tree_iter->user_data >= NULL;
}

/**
 * gom_list_store_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_list_store_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	GomListStore *store = GOM_LIST_STORE(object);

	switch (prop_id) {
	case PROP_ADAPTER:
		store->priv->adapter = g_value_dup_object(value);
		break;
	case PROP_QUERY:
		store->priv->query = g_value_dup_object(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
gtk_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_column_type = gom_list_store_get_column_type;
	iface->get_flags = gom_list_store_get_flags;
	iface->get_iter = gom_list_store_get_iter;
	iface->get_n_columns = gom_list_store_get_n_columns;
	iface->get_path = gom_list_store_get_path;
	iface->get_value = gom_list_store_get_value;
	iface->iter_children = gom_list_store_iter_children;
	iface->iter_has_child = gom_list_store_iter_has_child;
	iface->iter_n_children = gom_list_store_iter_n_children;
	iface->iter_next = gom_list_store_iter_next;
	iface->iter_nth_child = gom_list_store_iter_nth_child;
	iface->iter_parent = gom_list_store_iter_parent;
	iface->iter_previous = gom_list_store_iter_previous;
}
