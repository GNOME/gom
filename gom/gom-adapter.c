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
#include "gom-command.h"

G_DEFINE_TYPE(GomAdapter, gom_adapter, G_TYPE_OBJECT)

struct _GomAdapterPrivate
{
   sqlite3 *db;
   GThread *thread;
   GAsyncQueue *queue;
};

typedef enum {
   ASYNC_CMD_TYPE_OPEN,
   ASYNC_CMD_TYPE_READ,
   ASYNC_CMD_TYPE_WRITE,
   ASYNC_CMD_TYPE_CLOSE
} GomAsyncCmdType;

typedef struct {
  GomAdapter *adapter;
  GomAsyncCmdType type;
  GomAdapterCallback callback;
  gpointer callback_data;
} GomAsyncCmd;

static GomAsyncCmd *
_async_cmd_new(GomAdapter         *adapter,
               GomAsyncCmdType     type,
               GomAdapterCallback  callback,
               gpointer            callback_data)
{
   GomAsyncCmd *cmd;
   cmd = g_new0(GomAsyncCmd, 1);
   cmd->adapter = g_object_ref(adapter);
   cmd->type = type;
   cmd->callback = callback;
   cmd->callback_data = callback_data;
   return cmd;
}

GomAdapter *
gom_adapter_new (void)
{
   return g_object_new(GOM_TYPE_ADAPTER, NULL);
}

/**
 * gom_adapter_get_handle:
 * @adapter: (in): A #GomAdapter.
 *
 * Fetches the sqlite3 structure used by the adapter.
 *
 * Returns: (transfer none): A handle to the #sqlite3 structure.
 * Side effects: None.
 */
gpointer
gom_adapter_get_handle (GomAdapter *adapter)
{
   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), NULL);
   g_return_val_if_fail(adapter->priv->thread != NULL, NULL);
   g_assert (g_thread_self () == adapter->priv->thread);
   return adapter->priv->db;
}

static gpointer
gom_adapter_worker (gpointer data)
{
   GomAsyncCmd *cmd;
   GAsyncQueue *queue = data;

   /*
    * First item is open request.
    */
   cmd = g_async_queue_pop(queue);
   g_assert (cmd->type == ASYNC_CMD_TYPE_OPEN);
   cmd->callback(cmd->adapter, cmd->callback_data);
   g_object_unref(cmd->adapter);
   g_free(cmd);

   /*
    * Handle additional requests.
    */
   while ((cmd = g_async_queue_pop(queue))) {
      /*
       * XXX: Right now, we synchronize all requests. I hope to make this
       *      more performant when necessary.
       */
      cmd->callback(cmd->adapter, cmd->callback_data);
      if (cmd->type == ASYNC_CMD_TYPE_CLOSE) {
         g_object_unref(cmd->adapter);
         g_free(cmd);
         break;
      }

      g_object_unref(cmd->adapter);
      g_free(cmd);
   }

   return NULL;
}

/**
 * gom_adapter_queue_write:
 * @adapter: (in): A #GomAdapter.
 * @callback: (in) (scope async): A callback to execute write queries on SQLite.
 * @user_data: (in): User data for @callback.
 *
 * Queues a callback to be executed within the SQLite thwrite. The callback can
 * perform reads and writes.
 */
void
gom_adapter_queue_write (GomAdapter         *adapter,
                         GomAdapterCallback  callback,
                         gpointer            user_data)
{
   GomAdapterPrivate *priv;
   GomAsyncCmd *cmd;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(adapter->priv->queue != NULL);

   priv = adapter->priv;

   cmd = _async_cmd_new(adapter, ASYNC_CMD_TYPE_WRITE, callback, user_data);

   g_async_queue_push(priv->queue, cmd);
}

/**
 * gom_adapter_queue_read:
 * @adapter: (in): A #GomAdapter.
 * @callback: (in) (scope async): A callback to execute read queries on SQLite.
 * @user_data: (in): User data for @callback.
 *
 * Queues a callback to be executed within the SQLite thread. The callback is
 * expected to perform reads only.
 */
void
gom_adapter_queue_read (GomAdapter         *adapter,
                        GomAdapterCallback  callback,
                        gpointer            user_data)
{
   GomAdapterPrivate *priv;
   GomAsyncCmd *cmd;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(adapter->priv->queue != NULL);

   priv = adapter->priv;

   cmd = _async_cmd_new(adapter, ASYNC_CMD_TYPE_READ, callback, user_data);

   g_async_queue_push(priv->queue, cmd);
}

static void
open_callback (GomAdapter *adapter,
               gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GAsyncQueue *queue;
   const char *uri;
   gint flags;
   gint ret;

   queue = g_object_get_data(G_OBJECT(simple), "queue");
   uri = g_object_get_data(G_OBJECT(simple), "uri");
   flags = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI;
   ret = sqlite3_open_v2(uri, &adapter->priv->db, flags, NULL);
   if (ret != SQLITE_OK) {
      g_simple_async_result_set_error(simple, GOM_ADAPTER_ERROR,
                                      GOM_ADAPTER_ERROR_OPEN,
                                      _("Failed to open database at %s"), uri);
   }
   g_simple_async_result_set_op_res_gboolean(simple, ret == SQLITE_OK);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
}

gboolean
gom_adapter_open_sync (GomAdapter           *adapter,
                       const gchar          *uri,
                       GError              **error)
{
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;
   GomAsyncCmd *cmd;
   GAsyncQueue *queue;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
   g_return_val_if_fail(uri != NULL, FALSE);

   priv = adapter->priv;

   if (priv->thread) {
      g_warning("%s may only be called once per adapter.",
                G_STRFUNC);
      return FALSE;
   }

   priv->queue = g_async_queue_new();

#if GLIB_CHECK_VERSION(2, 32, 0)
   priv->thread = g_thread_new("gom-adapter-worker",
                               gom_adapter_worker,
                               priv->queue);
#else
   priv->thread = g_thread_create(gom_adapter_worker, priv->queue,
                                  TRUE, NULL);
#endif

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(adapter), NULL, NULL,
                                      gom_adapter_open_sync);
   g_object_set_data_full(G_OBJECT(simple), "uri",
                          g_strdup(uri), g_free);
   g_object_set_data (G_OBJECT(simple), "queue", queue);

   cmd = _async_cmd_new(adapter, ASYNC_CMD_TYPE_OPEN, open_callback, simple);

   g_async_queue_push(priv->queue, cmd);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

/**
 * gom_adapter_open_async:
 * @adapter: a #GomAdapter
 * @uri: a URI understood by SQLite
 * @callback: the function to call when the operation finished, or %NULL
 * @user_data: the user data to pass to the callback function
 *
 * Opens the database pointed to by @uri. @uri can be in any format understood
 * by SQLite. See http://www.sqlite.org/c3ref/open.html for details.
 */
void
gom_adapter_open_async (GomAdapter          *adapter,
                        const gchar         *uri,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;
   GomAsyncCmd *cmd;

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

#if GLIB_CHECK_VERSION(2, 32, 0)
   priv->thread = g_thread_new("gom-adapter-worker",
                               gom_adapter_worker,
                               priv->queue);
#else
   priv->thread = g_thread_create(gom_adapter_worker, priv->queue,
                                  TRUE, NULL);
#endif

   simple = g_simple_async_result_new(G_OBJECT(adapter), callback, user_data,
                                      gom_adapter_open_async);
   g_object_set_data_full(G_OBJECT(simple), "uri",
                          g_strdup(uri), g_free);

   cmd = _async_cmd_new(adapter, ASYNC_CMD_TYPE_OPEN, open_callback, simple);

   g_async_queue_push(priv->queue, cmd);
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
   g_object_unref(simple);

   return ret;
}

static void
close_callback (GomAdapter *adapter,
                gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GAsyncQueue *queue;

   queue = g_object_get_data(user_data, "queue");

   sqlite3_close(adapter->priv->db);
   adapter->priv->db = NULL;

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
}

gboolean
gom_adapter_close_sync (GomAdapter    *adapter,
                        GError       **error)
{
   GomAsyncCmd *cmd;
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;
   GAsyncQueue *queue;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);

   priv = adapter->priv;

   if (!priv->db) {
      return TRUE;
   }

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(adapter), NULL, NULL,
                                      gom_adapter_close_sync);
   g_object_set_data(G_OBJECT(simple), "queue", queue);

   cmd = _async_cmd_new(adapter, ASYNC_CMD_TYPE_CLOSE, close_callback, simple);

   g_async_queue_push(priv->queue, cmd);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

void
gom_adapter_close_async (GomAdapter          *adapter,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
   GomAdapterPrivate *priv;
   GSimpleAsyncResult *simple;
   GomAsyncCmd *cmd;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(callback != NULL);

   priv = adapter->priv;

   simple = g_simple_async_result_new(G_OBJECT(adapter), callback, user_data,
                                      gom_adapter_close_async);

   if (!priv->db) {
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      return;
   }

   cmd = _async_cmd_new(adapter, ASYNC_CMD_TYPE_CLOSE, close_callback, simple);

   g_async_queue_push(priv->queue, cmd);
}

gboolean
gom_adapter_close_finish (GomAdapter    *adapter,
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
   g_object_unref(simple);

   return ret;
}

/**
 * gom_adapter_execute_sql:
 * @adapter: (in): A #GomAdapter.
 * @sql: (in): SQL to execute.
 *
 * This is a helper function to make simple execution of SQL easier.
 * It is primarily meant for things like "BEGIN;" and "COMMIT;".
 *
 * This MUST be called from within a write transaction using
 * gom_adapter_queue_write().
 *
 * Returns: %TRUE if successful;
 */
gboolean
gom_adapter_execute_sql (GomAdapter   *adapter,
                         const gchar  *sql,
                         GError      **error)
{
   GomCommand *command;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);
   g_return_val_if_fail(sql, FALSE);
   g_assert (g_thread_self () == adapter->priv->thread);

   command = g_object_new(GOM_TYPE_COMMAND,
                          "adapter", adapter,
                          "sql", sql,
                          NULL);
   ret = gom_command_execute(command, NULL, error);
   g_object_unref(command);

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


   if (priv->db) {
      g_warning("Adapter not closed, leaking!");
   } else {
      g_async_queue_unref(priv->queue);
      g_thread_unref(priv->thread);
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
