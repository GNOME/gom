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

#include <glib/gi18n.h>
#include <sqlite3.h>

#include "gom-adapter.h"

G_DEFINE_TYPE(GomAdapter, gom_adapter, G_TYPE_OBJECT)

struct _GomAdapterPrivate
{
   sqlite3 *db;
   GThread *thread;
   GAsyncQueue *queue;
};

gpointer
gom_adapter_get_handle (GomAdapter *adapter)
{
   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), NULL);
   return adapter->priv->db;
}

static gpointer
gom_adapter_worker (gpointer data)
{
   GSimpleAsyncResult *simple;
   GomAdapter *adapter;
   GAsyncQueue *queue = data;
   const gchar *uri;
   gint flags;
   gint ret;

   /*
    * First item is open request.
    */
   simple = g_async_queue_pop(queue);
   adapter = (GomAdapter *)g_async_result_get_source_object(G_ASYNC_RESULT(simple));
   uri = g_object_get_data(G_OBJECT(simple), "uri");
   flags = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
   ret = sqlite3_open_v2(uri, &adapter->priv->db, flags, NULL);
   if (ret != SQLITE_OK) {
      g_simple_async_result_set_error(simple, GOM_ADAPTER_ERROR,
                                      GOM_ADAPTER_ERROR_OPEN,
                                      _("Failed to open database at %s"), uri);
   }
   g_simple_async_result_set_op_res_gboolean(simple, ret == SQLITE_OK);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   /*
    * Handle additional requests.
    */
   while ((simple = g_async_queue_pop(queue))) {
      const gchar *request = g_object_get_data(G_OBJECT(simple), "request");

      /*
       * XXX: Right now, we synchronize all requests. I hope to make this
       *      more performant when necessary.
       */

      if (!g_strcmp0(request, "queue-write")) {
         GomAdapterCallback callback = g_object_get_data(G_OBJECT(simple), "write-callback");
         gpointer callback_data = g_object_get_data(G_OBJECT(simple), "write-callback-data");
         callback(adapter, callback_data);
      } else if (!g_strcmp0(request, "queue-read")) {
         GomAdapterCallback callback = g_object_get_data(G_OBJECT(simple), "read-callback");
         gpointer callback_data = g_object_get_data(G_OBJECT(simple), "read-callback-data");
         callback(adapter, callback_data);
      }

      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
   }

   return NULL;
}

static void
dummy_cb (GObject      *object,
          GAsyncResult *result,
          gpointer      user_data)
{
   /* Do nothing. */
}

void
gom_adapter_queue_write (GomAdapter         *adapter,
                         GomAdapterCallback  callback,
                         gpointer            user_data)
{
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(adapter->priv->queue != NULL);

   priv = adapter->priv;

   simple = g_simple_async_result_new(G_OBJECT(adapter), dummy_cb, NULL,
                                      gom_adapter_queue_write);
   g_object_set_data(G_OBJECT(simple), "request", (gpointer)"queue-write");
   g_object_set_data(G_OBJECT(simple), "write-callback", callback);
   g_object_set_data(G_OBJECT(simple), "write-callback-data", user_data);
   g_async_queue_push(priv->queue, simple);
}

void
gom_adapter_queue_read (GomAdapter         *adapter,
                        GomAdapterCallback  callback,
                        gpointer            user_data)
{
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(adapter->priv->queue != NULL);

   priv = adapter->priv;

   simple = g_simple_async_result_new(G_OBJECT(adapter), dummy_cb, NULL,
                                      gom_adapter_queue_write);
   g_object_set_data(G_OBJECT(simple), "request", (gpointer)"queue-read");
   g_object_set_data(G_OBJECT(simple), "read-callback", callback);
   g_object_set_data(G_OBJECT(simple), "read-callback-data", user_data);
   g_async_queue_push(priv->queue, simple);
}

void
gom_adapter_open_async (GomAdapter          *adapter,
                        const gchar         *uri,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(uri != NULL);
   g_return_if_fail(callback != NULL);

   priv = adapter->priv;

   if (priv->thread) {
      g_warning("%s may only be called once per adapter.",
                G_STRFUNC);
      return;
   }

   priv->queue = g_async_queue_new();
   priv->thread = g_thread_create(gom_adapter_worker, priv->queue,
                                  TRUE, NULL);
   simple = g_simple_async_result_new(G_OBJECT(adapter), callback, user_data,
                                      gom_adapter_open_async);
   g_object_set_data_full(G_OBJECT(simple), "uri",
                          g_strdup(uri), g_free);
   g_async_queue_push(priv->queue, simple);
}

gboolean
gom_adapter_open_finish (GomAdapter    *adapter,
                         GAsyncResult  *result,
                         GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret;
}

/**
 * gom_adapter_finalize:
 * @object: (in): A #GomAdapter.
 *
 * Finalizer for a #GomAdapter instance.  Frees any resources held by
 * the instance.
 */
static void
gom_adapter_finalize (GObject *object)
{
   GomAdapterPrivate *priv = GOM_ADAPTER(object)->priv;

   if (priv->queue) {
      g_warning("Adapter not closed, leaking!");
   }

   G_OBJECT_CLASS(gom_adapter_parent_class)->finalize(object);
}

/**
 * gom_adapter_class_init:
 * @klass: (in): A #GomAdapterClass.
 *
 * Initializes the #GomAdapterClass and prepares the vtable.
 */
static void
gom_adapter_class_init (GomAdapterClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_adapter_finalize;
   g_type_class_add_private(object_class, sizeof(GomAdapterPrivate));
}

/**
 * gom_adapter_init:
 * @adapter: (in): A #GomAdapter.
 *
 * Initializes the newly created #GomAdapter instance.
 */
static void
gom_adapter_init (GomAdapter *adapter)
{
   adapter->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(adapter,
                                  GOM_TYPE_ADAPTER,
                                  GomAdapterPrivate);
}

GQuark
gom_adapter_error_quark (void)
{
   return g_quark_from_static_string("gom_adapter_error_quark");
}
