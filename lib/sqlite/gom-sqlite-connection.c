/* gom-sqlite-connection.c
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

#include <errno.h>

#include <sqlite3mc.h>

#include <gio/gio.h>

#include "gom-sqlite-connection-private.h"
#include "gom-sqlite-driver-private.h"
#include "gom-trace-private.h"

#define GOM_SQLITE_BUSY_TIMEOUT_MS 0

#if HAVE_SQLITE_VEC1
extern int sqlite3_extension_init (sqlite3                     *db,
                                   char                       **pzErrMsg,
                                   const sqlite3_api_routines  *pApi);
#endif

struct _GomSqliteConnection
{
  GObject  parent_instance;
  sqlite3 *native;
  char    *uri;
  GBytes  *encryption_key;
};

typedef struct
{
  char   *uri;
  GBytes *encryption_key;
} GomSqliteConnectionNewState;

struct _GomSqliteConnectionClass
{
  GObjectClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomSqliteConnection, gom_sqlite_connection, G_TYPE_OBJECT)

static void
gom_sqlite_connection_finalize (GObject *object)
{
  GomSqliteConnection *self = (GomSqliteConnection *)object;

  if (self->native != NULL)
    {
      sqlite3_close_v2 (self->native);
      self->native = NULL;
    }
  g_clear_pointer (&self->encryption_key, g_bytes_unref);
  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (gom_sqlite_connection_parent_class)->finalize (object);
}

static void
gom_sqlite_connection_class_init (GomSqliteConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_sqlite_connection_finalize;
}

static void
gom_sqlite_connection_init (GomSqliteConnection *self)
{
}

static gboolean
gom_sqlite_connection_configure (sqlite3  *db,
                                 GError  **error)
{
  g_assert (db != NULL);

  /* Keep lock waiting in gom-sqlite-driver.c so errors and trace marks are consistent. */
  if (sqlite3_busy_timeout (db, GOM_SQLITE_BUSY_TIMEOUT_MS) != SQLITE_OK)
    {
      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_FAILED,
                   "Failed to configure SQLite busy timeout: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  if (!gom_sqlite_driver_exec_sql (db,
                                   "PRAGMA journal_mode = WAL",
                                   "configure SQLite WAL mode",
                                   error))
    return FALSE;

  if (!gom_sqlite_driver_exec_sql (db,
                                   "PRAGMA wal_autocheckpoint = 1",
                                   "configure SQLite WAL autocheckpoint",
                                   error))
    return FALSE;

  if (!gom_sqlite_driver_exec_sql (db,
                                   "PRAGMA synchronous = NORMAL",
                                   "configure SQLite synchronous mode",
                                   error))
    return FALSE;

  return TRUE;
}

static DexFuture *
gom_sqlite_connection_new_thread (gpointer user_data)
{
  GomSqliteConnectionNewState *state = user_data;
  const char *uri;
  GomSqliteConnection *self;
  sqlite3 *db = NULL;
  const guint8 *key_data;
  gsize key_len = 0;
  guint flags;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (state->uri != NULL);

  uri = state->uri;

  flags = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI;

  if (sqlite3_initialize () != SQLITE_OK)
    return dex_future_new_reject (GOM_ERROR,
                                  GOM_ERROR_BACKEND_INITIALIZATION_FAILED,
                                  "Failed to initialize SQLite");

  if (sqlite3_open_v2 (uri, &db, flags, NULL) != SQLITE_OK)
    {
      if (db != NULL)
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       "Failed to open SQLite database: %s",
                       sqlite3_errmsg (db));
          sqlite3_close_v2 (db);
        }
      else
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errno),
                       "Failed to open SQLite database");
        }

      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (state->encryption_key != NULL)
    {
      key_data = g_bytes_get_data (state->encryption_key, &key_len);
      if (key_data == NULL || key_len == 0 || key_len > G_MAXINT)
        {
          sqlite3_close_v2 (db);
          return dex_future_new_reject (GOM_ERROR,
                                        GOM_ERROR_INVALID_ENCRYPTION_KEY,
                                        "SQLite encryption key is invalid");
        }

      if (sqlite3_key_v2 (db, "main", key_data, (int)key_len) != SQLITE_OK)
        {
          g_set_error (&error,
                       GOM_ERROR,
                       GOM_ERROR_INVALID_ENCRYPTION_KEY,
                       "Failed to apply SQLite encryption key: %s",
                       sqlite3_errmsg (db));
          sqlite3_close_v2 (db);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (sqlite3_exec (db, "SELECT count(*) FROM sqlite_master", NULL, NULL, NULL) != SQLITE_OK)
        {
          g_set_error (&error,
                       GOM_ERROR,
                       GOM_ERROR_INVALID_ENCRYPTION_KEY,
                       "Failed to verify SQLite encryption key: %s",
                       sqlite3_errmsg (db));
          sqlite3_close_v2 (db);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

#if HAVE_SQLITE_VEC1
  {
    char *errmsg = NULL;

    if (sqlite3_extension_init (db, &errmsg, NULL) != SQLITE_OK)
      {
        g_set_error (&error,
                     GOM_ERROR,
                     GOM_ERROR_MISSING_EXTENSION,
                     "Failed to initialize SQLite vec1 extension: %s",
                     errmsg != NULL ? errmsg : sqlite3_errmsg (db));
        g_clear_pointer (&errmsg, sqlite3_free);
        sqlite3_close_v2 (db);
        return dex_future_new_for_error (g_steal_pointer (&error));
      }

    g_clear_pointer (&errmsg, sqlite3_free);
  }
#endif

  if (!gom_sqlite_connection_configure (db, &error))
    {
      sqlite3_close_v2 (db);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  self = g_object_new (GOM_TYPE_SQLITE_CONNECTION, NULL);
  self->native = g_steal_pointer (&db);
  self->uri = g_strdup (uri);
  if (state->encryption_key != NULL)
    self->encryption_key = g_bytes_ref (state->encryption_key);

  return dex_future_new_take_object (g_steal_pointer (&self));
}

static void
gom_sqlite_connection_new_state_free (gpointer data)
{
  GomSqliteConnectionNewState *state = data;

  if (state == NULL)
    return;

  g_clear_pointer (&state->uri, g_free);
  g_clear_pointer (&state->encryption_key, g_bytes_unref);
  g_free (state);
}

DexFuture *
gom_sqlite_connection_new (const char    *uri,
                           GBytes        *encryption_key,
                           DexThreadPool *thread_pool,
                           DexLimiter    *open_limiter)
{
  GomSqliteConnectionNewState *state;

  dex_return_error_if_fail (uri != NULL);
  dex_return_error_if_fail (DEX_IS_THREAD_POOL (thread_pool));
  dex_return_error_if_fail (DEX_IS_LIMITER (open_limiter));

  state = g_new0 (GomSqliteConnectionNewState, 1);
  state->uri = g_strdup (uri);
  if (encryption_key != NULL)
    state->encryption_key = g_bytes_ref (encryption_key);

  return dex_limiter_run_on_pool (open_limiter,
                                  thread_pool,
                                  gom_sqlite_connection_new_thread,
                                  state,
                                  gom_sqlite_connection_new_state_free);
}

sqlite3 *
gom_sqlite_connection_get_native (GomSqliteConnection *connection)
{
  g_return_val_if_fail (GOM_IS_SQLITE_CONNECTION (connection), NULL);

  return connection->native;
}
