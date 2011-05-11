/* gom-adapter.c
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

#include "gom-adapter.h"
#include "gom-resource.h"

#define MAGIC_CLOSE GINT_TO_POINTER(1)

G_DEFINE_ABSTRACT_TYPE(GomAdapter, gom_adapter, G_TYPE_OBJECT)

struct _GomAdapterPrivate
{
	GAsyncQueue *queue;
	GThread     *thread;
};

static gpointer
gom_adapter_thread_func (gpointer user_data)
{
	GomAdapter *adapter = user_data;
	GClosure *closure;
	GValue value = { 0 };

	g_return_val_if_fail(GOM_IS_ADAPTER(adapter), NULL);

	while (MAGIC_CLOSE != (closure = g_async_queue_pop(adapter->priv->queue))) {
		g_value_init(&value, GOM_TYPE_ADAPTER);
		g_value_set_object(&value, adapter);
		g_closure_invoke(closure, NULL, 1, &value, NULL);
		g_value_unset(&value);
		g_closure_unref(closure);
	}

	return NULL;
}

static void
gom_adapter_finalize (GObject *object)
{
	GomAdapterPrivate *priv = GOM_ADAPTER(object)->priv;

	g_async_queue_push(priv->queue, MAGIC_CLOSE);
	if (priv->thread) {
		g_thread_join(priv->thread);
	}
	g_async_queue_unref(priv->queue);

	G_OBJECT_CLASS(gom_adapter_parent_class)->finalize(object);
}

static void
gom_adapter_class_init (GomAdapterClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_adapter_finalize;
	g_type_class_add_private(object_class, sizeof(GomAdapterPrivate));
}

static void
gom_adapter_init (GomAdapter *adapter)
{
	GomAdapterPrivate *priv;
	GError *error = NULL;

	adapter->priv = priv =
		G_TYPE_INSTANCE_GET_PRIVATE(adapter,
		                           GOM_TYPE_ADAPTER,
		                           GomAdapterPrivate);

	priv->queue = g_async_queue_new();
	priv->thread = g_thread_create(gom_adapter_thread_func,
	                               adapter, TRUE, &error);
	g_assert_no_error(error);
}

void
gom_adapter_call_in_thread (GomAdapter           *adapter,
                            GomAdapterThreadFunc  callback,
                            gpointer              user_data,
                            GDestroyNotify        notify)
{
	GClosure *closure;

	g_return_if_fail(GOM_IS_ADAPTER(adapter));
	g_return_if_fail(callback != NULL);

	closure = g_cclosure_new(G_CALLBACK(callback), user_data,
	                         (GClosureNotify)notify);
	g_closure_set_marshal(closure, g_cclosure_marshal_VOID__VOID);
	g_async_queue_push(adapter->priv->queue, closure);
}

/**
 * gom_adapter_create:
 * @adapter: (in): A #GomAdapter.
 * @enumerable: (in): A #GomEnumerable.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * Creates a series of new #GomResource<!-- -->'s found in enumerable.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_create (GomAdapter     *adapter,
                    GomEnumerable  *enumerable,
                    GError        **error)
{
	g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
	g_return_val_if_fail(GOM_IS_ENUMERABLE(enumerable), FALSE);

	return GOM_ADAPTER_GET_CLASS(adapter)->create(adapter,
	                                              enumerable,
	                                              error);
}

/**
 * gom_adapter_delete:
 * @adapter: (in): A #GomAdapter.
 * @collection: (in): A #GomCollection of #GomResource<!-- -->'s.
 * @error: (in): A location for a #GError, or %NULL.
 *
 * Deletes a resource from the underlying data adapter.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_delete (GomAdapter     *adapter,
                    GomCollection  *collection,
                    GError        **error)
{
	g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
	g_return_val_if_fail(GOM_IS_COLLECTION(collection), FALSE);

	return GOM_ADAPTER_GET_CLASS(adapter)->delete(adapter,
	                                              collection,
	                                              error);
}

/**
 * gom_adapter_read:
 * @adapter: (in): A #GomAdapter.
 * @query: (in): A #GomQuery.
 * @enumerable: (out) (transfer full): A location for a #GomEnumerable.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * Performs @query on the underlying storage. The results are available
 * by iterating through the result set which is stored in the location
 * specified by @enumerable.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_read (GomAdapter     *adapter,
                  GomQuery       *query,
                  GomEnumerable **enumerable,
                  GError        **error)
{
	g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
	g_return_val_if_fail(GOM_IS_QUERY(query), FALSE);
	g_return_val_if_fail(enumerable != NULL, FALSE);
	g_return_val_if_fail(*enumerable == NULL, FALSE);

	return GOM_ADAPTER_GET_CLASS(adapter)->read(adapter,
	                                            query,
	                                            enumerable,
	                                            error);
}

/**
 * gom_adapter_update:
 * @adapter: (in): A #GomAdapter.
 * @properties: (in): A set of properties to update.
 * @values: (in): The values for @properties.
 * @collection: (in): A collection of resources that should be updated.
 *
 * Updates the resources found in @collection. The properties found in
 * @properties are updated to the values found in @values at the
 * corresponding index.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_update (GomAdapter     *adapter,
                    GomPropertySet *properties,
                    GValueArray    *values,
                    GomCollection  *collection,
                    GError        **error)
{
	g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
	g_return_val_if_fail(properties != NULL, FALSE);
	g_return_val_if_fail(values != NULL, FALSE);
	g_return_val_if_fail(GOM_IS_COLLECTION(collection), FALSE);

	return GOM_ADAPTER_GET_CLASS(adapter)->update(adapter,
	                                              properties,
	                                              values,
	                                              collection,
	                                              error);
}
