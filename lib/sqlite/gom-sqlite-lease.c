/* gom-sqlite-lease.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>

#include "gom-sqlite-connection-private.h"
#include "gom-sqlite-lease-private.h"
#include "gom-sqlite-pool-private.h"

typedef struct
{
  DexThreadFunc   thread_func;
  gpointer        user_data;
  GDestroyNotify  user_data_destroy;
  DexPromise     *promise;
} GomSqliteLeaseInvokeMessage;

#define GOM_SQLITE_LEASE_STOP_MESSAGE ((gpointer) GINT_TO_POINTER (1))

struct _GomSqliteLeaseState
{
  gatomicrefcount      ref_count;
  GMutex               mutex;
  GAsyncQueue         *queue;
  GomSqliteConnection *connection;
  GomSqlitePool       *pool;
  gboolean             worker_started;
  gboolean             shutting_down;
  gboolean             disposed;
};

struct _GomSqliteLease
{
  GObject              parent_instance;
  GomSqliteLeaseState *state;
};

struct _GomSqliteLeaseClass
{
  GObjectClass parent_class;
};

static GomSqliteLeaseState *gom_sqlite_lease_state_ref_internal (GomSqliteLeaseState *state);
static gpointer             gom_sqlite_lease_worker_thread      (gpointer             user_data);

static void
gom_sqlite_lease_invoke_message_free (gpointer data)
{
  GomSqliteLeaseInvokeMessage *message = data;

  if (message == NULL || message == GOM_SQLITE_LEASE_STOP_MESSAGE)
    return;

  if (message->user_data_destroy != NULL)
    message->user_data_destroy (message->user_data);

  if (message->promise != NULL)
    dex_unref (message->promise);
  g_free (message);
}

static void
gom_sqlite_lease_invoke_message_complete (GomSqliteLeaseInvokeMessage *message)
{
  g_autoptr(GError) error = NULL;
  DexFuture *future;

  g_assert (message != NULL);
  g_assert (DEX_IS_PROMISE (message->promise));

  if (message->thread_func == NULL)
    {
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "SQLite lease invoke callback is NULL");
      dex_promise_reject (message->promise, g_steal_pointer (&error));
      return;
    }

  if (!(future = message->thread_func (message->user_data)))
    {
      if (message->user_data_destroy != NULL)
        {
          message->user_data_destroy (message->user_data);
          message->user_data_destroy = NULL;
          message->user_data = NULL;
        }

      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "SQLite lease invoke callback returned no future");
      dex_promise_reject (message->promise, g_steal_pointer (&error));
      return;
    }

  future = dex_ref (future);

  if (!dex_thread_wait_for (future, &error))
    {
      if (message->user_data_destroy != NULL)
        {
          message->user_data_destroy (message->user_data);
          message->user_data_destroy = NULL;
          message->user_data = NULL;
        }

      dex_promise_reject (message->promise, g_steal_pointer (&error));
      dex_clear (&future);
      return;
    }

  if (message->user_data_destroy != NULL)
    {
      message->user_data_destroy (message->user_data);
      message->user_data_destroy = NULL;
      message->user_data = NULL;
    }

  {
    const GValue *value = dex_future_get_value (future, &error);

    if (error != NULL)
      {
        dex_promise_reject (message->promise, g_steal_pointer (&error));
      }
    else
      {
        dex_promise_resolve (message->promise, value);
      }
  }

  dex_clear (&future);
}

static gboolean
gom_sqlite_lease_state_ensure_worker_locked (GomSqliteLeaseState  *state,
                                             const char           *thread_name,
                                             GError              **error)
{
  GThread *thread;
  GomSqliteLeaseState *thread_state;

  g_assert (state != NULL);

  if (state->queue == NULL)
    state->queue = g_async_queue_new_full (gom_sqlite_lease_invoke_message_free);

  if (state->worker_started)
    return TRUE;

  thread_state = gom_sqlite_lease_state_ref_internal (state);
  thread = g_thread_try_new (thread_name != NULL ? thread_name : "[gom-sqlite-lease]",
                             gom_sqlite_lease_worker_thread,
                             thread_state,
                             error);

  if (thread == NULL)
    {
      gom_sqlite_lease_state_unref (thread_state);
      g_clear_pointer (&state->queue, g_async_queue_unref);
      return FALSE;
    }

  state->worker_started = TRUE;
  g_thread_unref (thread);
  return TRUE;
}

static void
gom_sqlite_lease_state_shutdown (GomSqliteLeaseState *state)
{
  g_assert (state != NULL);

  g_mutex_lock (&state->mutex);
  state->shutting_down = TRUE;

  if (!state->disposed &&
      state->queue != NULL &&
      g_atomic_int_get ((gint *)&state->ref_count) == 2)
    {
      state->disposed = TRUE;
      g_async_queue_push (state->queue, GOM_SQLITE_LEASE_STOP_MESSAGE);
    }

  g_mutex_unlock (&state->mutex);
}

static void
gom_sqlite_lease_state_free (GomSqliteLeaseState *state)
{
  g_assert (state != NULL);

  if (state->connection != NULL && state->pool != NULL)
    gom_sqlite_pool_return_connection (state->pool, state->connection);

  g_clear_pointer (&state->queue, g_async_queue_unref);
  g_clear_object (&state->connection);
  g_clear_object (&state->pool);
  g_mutex_clear (&state->mutex);
  g_free (state);
}

static GomSqliteLeaseState *
gom_sqlite_lease_state_new (GomSqliteConnection *connection,
                            GomSqlitePool       *pool)
{
  GomSqliteLeaseState *state;

  g_return_val_if_fail (GOM_IS_SQLITE_CONNECTION (connection), NULL);
  g_return_val_if_fail (GOM_IS_SQLITE_POOL (pool), NULL);

  state = g_new0 (GomSqliteLeaseState, 1);
  g_atomic_ref_count_init (&state->ref_count);
  g_mutex_init (&state->mutex);
  state->connection = g_object_ref (connection);
  state->pool = g_object_ref (pool);

  return state;
}

static GomSqliteLeaseState *
gom_sqlite_lease_state_ref_internal (GomSqliteLeaseState *state)
{
  g_assert (state != NULL);

  g_atomic_ref_count_inc (&state->ref_count);
  return state;
}

GomSqliteLeaseState *
gom_sqlite_lease_state_ref (GomSqliteLeaseState *state)
{
  g_return_val_if_fail (state != NULL, NULL);

  return gom_sqlite_lease_state_ref_internal (state);
}

G_DEFINE_FINAL_TYPE (GomSqliteLease, gom_sqlite_lease, G_TYPE_OBJECT)

GomSqliteLeaseState *
gom_sqlite_lease_ref_state (GomSqliteLease *self)
{
  g_return_val_if_fail (GOM_IS_SQLITE_LEASE (self), NULL);

  if (self->state != NULL)
    gom_sqlite_lease_state_ref_internal (self->state);

  return self->state;
}

void
gom_sqlite_lease_state_unref (GomSqliteLeaseState *state)
{
  gboolean should_stop = FALSE;

  g_return_if_fail (state != NULL);

  g_mutex_lock (&state->mutex);
  if (!state->disposed &&
      state->shutting_down &&
      g_atomic_int_get ((gint *)&state->ref_count) == 2 &&
      state->queue != NULL)
    {
      state->disposed = TRUE;
      should_stop = TRUE;
    }
  g_mutex_unlock (&state->mutex);

  if (should_stop)
    g_async_queue_push (state->queue, GOM_SQLITE_LEASE_STOP_MESSAGE);

  if (g_atomic_ref_count_dec (&state->ref_count))
    gom_sqlite_lease_state_free (state);
}

GomSqliteConnection *
gom_sqlite_lease_state_get_connection (GomSqliteLeaseState *state)
{
  g_return_val_if_fail (state != NULL, NULL);

  return state->connection;
}

static void
gom_sqlite_lease_dispose (GObject *object)
{
  GomSqliteLease *self = (GomSqliteLease *)object;
  GomSqliteLeaseState *state;

  state = g_steal_pointer (&self->state);

  if (state != NULL)
    {
      gom_sqlite_lease_state_shutdown (state);
      gom_sqlite_lease_state_unref (state);
    }

  G_OBJECT_CLASS (gom_sqlite_lease_parent_class)->dispose (object);
}

static void
gom_sqlite_lease_class_init (GomSqliteLeaseClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gom_sqlite_lease_dispose;
}

static void
gom_sqlite_lease_init (GomSqliteLease *self)
{
}

GomSqliteLease *
gom_sqlite_lease_new (GomSqliteConnection *connection,
                      GomSqlitePool       *pool)
{
  GomSqliteLease *self;

  g_return_val_if_fail (GOM_IS_SQLITE_CONNECTION (connection), NULL);
  g_return_val_if_fail (GOM_IS_SQLITE_POOL (pool), NULL);

  self = g_object_new (GOM_TYPE_SQLITE_LEASE, NULL);
  self->state = gom_sqlite_lease_state_new (connection, pool);

  return self;
}

GomSqliteConnection *
gom_sqlite_lease_get_connection (GomSqliteLease *lease)
{
  GomSqliteLeaseState *state;

  g_return_val_if_fail (GOM_IS_SQLITE_LEASE (lease), NULL);

  state = lease->state;
  return state != NULL ? state->connection : NULL;
}

static gpointer
gom_sqlite_lease_worker_thread (gpointer user_data)
{
  GomSqliteLeaseState *state = user_data;

  g_assert (state != NULL);

  for (;;)
    {
      GomSqliteLeaseInvokeMessage *message;

      message = g_async_queue_pop (state->queue);
      if (message == GOM_SQLITE_LEASE_STOP_MESSAGE)
        break;

      gom_sqlite_lease_invoke_message_complete (message);
      gom_sqlite_lease_invoke_message_free (message);
    }

  gom_sqlite_lease_state_unref (state);
  return NULL;
}

DexFuture *
gom_sqlite_lease_state_invoke (GomSqliteLeaseState *state,
                               const char          *thread_name,
                               DexThreadFunc        thread_func,
                               gpointer             user_data,
                               GDestroyNotify       user_data_destroy)
{
  GomSqliteLeaseInvokeMessage *message;
  DexPromise *promise;
  g_autoptr(GError) error = NULL;
  DexFuture *future;

  g_return_val_if_fail (state != NULL, NULL);

  promise = dex_promise_new ();

  g_mutex_lock (&state->mutex);

  if (state->disposed)
    {
      g_mutex_unlock (&state->mutex);
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "SQLite lease has been closed");
      dex_promise_reject (promise, g_steal_pointer (&error));
      return DEX_FUTURE (promise);
    }

  if (!gom_sqlite_lease_state_ensure_worker_locked (state, thread_name, &error))
    {
      g_mutex_unlock (&state->mutex);
      dex_promise_reject (promise, g_steal_pointer (&error));
      return DEX_FUTURE (promise);
    }

  message = g_new0 (GomSqliteLeaseInvokeMessage, 1);
  message->thread_func = thread_func;
  message->user_data = user_data;
  message->user_data_destroy = user_data_destroy;
  message->promise = dex_ref (promise);

  g_async_queue_push (state->queue, message);
  g_mutex_unlock (&state->mutex);

  future = DEX_FUTURE (promise);
  return future;
}

DexFuture *
gom_sqlite_lease_invoke (GomSqliteLease *self,
                         const char     *thread_name,
                         DexThreadFunc   thread_func,
                         gpointer        user_data,
                         GDestroyNotify  user_data_destroy)
{
  g_return_val_if_fail (GOM_IS_SQLITE_LEASE (self), NULL);

  if (self->state == NULL)
    {
      DexPromise *promise = dex_promise_new ();
      g_autoptr(GError) error = NULL;

      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "SQLite lease has been closed");
      dex_promise_reject (promise, g_steal_pointer (&error));
      return DEX_FUTURE (promise);
    }

  return gom_sqlite_lease_state_invoke (self->state,
                                        thread_name,
                                        thread_func,
                                        user_data,
                                        user_data_destroy);
}
