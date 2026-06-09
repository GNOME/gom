/* gom-sqlite-cursor.c
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

#include <sqlite3.h>

#include "gom-entity.h"
#include "gom-meta.h"
#include "gom-repository.h"
#include "gom-cursor-private.h"
#include "gom-sqlite-connection-private.h"
#include "gom-sqlite-driver-private.h"
#include "gom-sqlite-lease-private.h"
#include "gom-sqlite-cursor-private.h"
#include "gom-sqlite-statement-private.h"
#include "gom-trace-private.h"

struct _GomSqliteCursor
{
  GomCursor           parent_instance;
  GomSqliteStatement *statement;
  char               *sql;
  gint64              position;
  gboolean            closed;
  gboolean            on_row;
  guint64             count;
  guint               has_count : 1;
  guint               owns_transaction : 1;
};

struct _GomSqliteCursorClass
{
  GomCursorClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomSqliteCursor, gom_sqlite_cursor, GOM_TYPE_CURSOR)

static DexFuture *gom_sqlite_cursor_move_absolute (GomCursor *cursor,
                                                   guint64    position);

static guint
gom_sqlite_cursor_get_n_columns (GomCursor *cursor)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  sqlite3_stmt *stmt;

  if (self->closed || self->statement == NULL)
    return 0;

  stmt = gom_sqlite_statement_get_native (self->statement);
  return stmt != NULL ? (guint)sqlite3_column_count (stmt) : 0;
}

static const char *
gom_sqlite_cursor_get_column_name (GomCursor *cursor,
                                   guint      column)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  sqlite3_stmt *stmt;

  if (self->closed || self->statement == NULL)
    return NULL;

  stmt = gom_sqlite_statement_get_native (self->statement);
  if (stmt == NULL || (int)column >= sqlite3_column_count (stmt))
    return NULL;

  return sqlite3_column_name (stmt, (int)column);
}

static gboolean
gom_sqlite_cursor_get_column_value (GomCursor *cursor,
                                    guint      column,
                                    GValue    *value)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  sqlite3_stmt *stmt;
  int type;
  int col;

  if (self->closed || self->statement == NULL || value == NULL)
    return FALSE;

  stmt = gom_sqlite_statement_get_native (self->statement);
  if (stmt == NULL || (int)column >= sqlite3_column_count (stmt))
    return FALSE;

  col = (int)column;
  type = sqlite3_column_type (stmt, col);

  switch (type)
    {
    case SQLITE_INTEGER:
      g_value_init (value, G_TYPE_INT64);
      g_value_set_int64 (value, sqlite3_column_int64 (stmt, col));
      return TRUE;

    case SQLITE_FLOAT:
      g_value_init (value, G_TYPE_DOUBLE);
      g_value_set_double (value, sqlite3_column_double (stmt, col));
      return TRUE;

    case SQLITE_TEXT:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, (const char *)sqlite3_column_text (stmt, col));
      return TRUE;

    case SQLITE_BLOB:
      {
        const guint8 *blob = sqlite3_column_blob (stmt, col);
        gsize size = sqlite3_column_bytes (stmt, col);
        g_value_init (value, G_TYPE_BYTES);
        g_value_set_boxed (value, size > 0 ? g_bytes_new (blob, size) : g_bytes_new (NULL, 0));
        return TRUE;
      }

    case SQLITE_NULL:
      g_value_init (value, G_TYPE_POINTER);
      g_value_set_pointer (value, NULL);
      return TRUE;

    default:
      return FALSE;
    }
}

static const char *
gom_sqlite_cursor_get_column_string (GomCursor *cursor,
                                     guint      column)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  sqlite3_stmt *stmt;
  const unsigned char *text;

  if (self->closed || self->statement == NULL)
    return NULL;

  stmt = gom_sqlite_statement_get_native (self->statement);
  if (stmt == NULL || (int)column >= sqlite3_column_count (stmt))
    return NULL;

  if (!(text = sqlite3_column_text (stmt, (int)column)))
    return NULL;

  return (const char *)text;
}

static DexFuture *
gom_sqlite_cursor_next (GomCursor *cursor)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  g_autoptr(GError) error = NULL;
  sqlite3_stmt *stmt;
  int rc;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  if (self->closed || self->statement == NULL)
    return dex_future_new_false ();

  if (!(stmt = gom_sqlite_statement_get_native (self->statement)))
    return dex_future_new_false ();

  rc = gom_sqlite_driver_step (stmt, "step cursor", &error);
  if (rc == SQLITE_ROW)
    {
      self->position++;
      self->on_row = TRUE;
      GOM_TRACE_END_MARK (start_time, "Cursor", "next", "sqlite row");
      return dex_future_new_true ();
    }
  if (rc == SQLITE_DONE)
    {
      self->position = MAX (self->position + 1, 0);
      self->on_row = FALSE;
      GOM_TRACE_END_MARK (start_time, "Cursor", "next", "sqlite done");
      return dex_future_new_false ();
    }

  GOM_TRACE_END_MARK (start_time, "Cursor", "next", "sqlite error");
  if (error != NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_FAILED,
                                "SQLite step failed: %s",
                                sqlite3_errmsg (sqlite3_db_handle (stmt)));
}

static gboolean
gom_sqlite_cursor_commit_transaction (GomSqliteCursor  *self,
                                      GError          **error)
{
  GomSqliteLeaseState *state;
  GomSqliteConnection *connection;
  sqlite3 *db;

  if (!self->owns_transaction)
    return TRUE;

  if (self->statement == NULL)
    {
      self->owns_transaction = FALSE;
      return TRUE;
    }

  state = gom_sqlite_statement_get_state (self->statement);
  connection = gom_sqlite_lease_state_get_connection (state);
  db = gom_sqlite_connection_get_native (connection);

  if (!gom_sqlite_driver_exec_sql (db, "COMMIT", "commit cursor transaction", error))
    return FALSE;

  self->owns_transaction = FALSE;
  return TRUE;
}

static DexFuture *
gom_sqlite_cursor_close (GomCursor *cursor)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  g_autoptr(GError) error = NULL;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  if (!gom_sqlite_cursor_commit_transaction (self, &error))
    {
      GOM_TRACE_END_MARK (start_time, "Cursor", "close", "sqlite error: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  self->closed = TRUE;
  self->on_row = FALSE;
  g_clear_object (&self->statement);
  GOM_TRACE_END_MARK (start_time, "Cursor", "close", "sqlite closed");
  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_cursor_exhaust_thread (gpointer user_data)
{
  GomSqliteCursor *self = user_data;
  g_autoptr(GError) error = NULL;
  sqlite3_stmt *stmt;
  int rc;

  if (self->closed || self->statement == NULL)
    return dex_future_new_true ();

  if (!(stmt = gom_sqlite_statement_get_native (self->statement)))
    return dex_future_new_true ();

  do
    rc = gom_sqlite_driver_step (stmt, "exhaust cursor", &error);
  while (rc == SQLITE_ROW);

  if (rc != SQLITE_DONE && error == NULL)
    g_set_error (&error,
                 G_IO_ERROR,
                 G_IO_ERROR_FAILED,
                 "SQLite step failed: %s",
                 sqlite3_errmsg (sqlite3_db_handle (stmt)));

  if (error == NULL)
    gom_sqlite_cursor_commit_transaction (self, &error);

  stmt = NULL;
  g_clear_object (&self->statement);
  self->closed = TRUE;

  if (error == NULL)
    return dex_future_new_true ();
  else
    return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
gom_sqlite_cursor_exhaust (GomCursor *cursor)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  GomSqliteLeaseState *state;

  state = gom_sqlite_statement_get_state (self->statement);

  return gom_sqlite_lease_state_invoke (state,
                                        "[gom-sqlite-exhaust]",
                                        gom_sqlite_cursor_exhaust_thread,
                                        g_object_ref (self),
                                        g_object_unref);
}

static DexFuture *
gom_sqlite_cursor_rewind (GomCursor *cursor)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);

  if (self->statement != NULL)
    gom_sqlite_statement_reset (self->statement);

  self->position = -1;
  self->on_row = FALSE;

  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_cursor_move_absolute (GomCursor *cursor,
                                 guint64    position)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  g_autoptr(GError) error = NULL;
  gint64 target;
  sqlite3_stmt *stmt;
  int rc;

  if (self->closed || self->statement == NULL)
    return dex_future_new_false ();

  if (position > G_MAXINT64)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Absolute position is out of range");
  target = (gint64)position;

  if (!(stmt = gom_sqlite_statement_get_native (self->statement)))
    return dex_future_new_false ();

  gom_sqlite_statement_reset (self->statement);
  self->position = -1;
  self->on_row = FALSE;

  while (self->position < target)
    {
      rc = gom_sqlite_driver_step (stmt, "move cursor", &error);

      if (rc == SQLITE_ROW)
        {
          self->position++;
          self->on_row = TRUE;
          continue;
        }

      if (rc == SQLITE_DONE)
        {
          self->position = MAX (self->position + 1, 0);
          self->on_row = FALSE;
          return dex_future_new_false ();
        }

      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "SQLite step failed: %s",
                                    sqlite3_errmsg (sqlite3_db_handle (stmt)));
    }

  return self->on_row ? dex_future_new_true () : dex_future_new_false ();
}

static DexFuture *
gom_sqlite_cursor_move_relative (GomCursor *cursor,
                                 gint64     offset)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  gint64 target;

  if (self->closed || self->statement == NULL)
    return dex_future_new_false ();

  if (offset == 0)
    return self->on_row ? dex_future_new_true () : dex_future_new_false ();

  if ((offset > 0 && self->position > G_MAXINT64 - offset) ||
      (offset < 0 && self->position < G_MININT64 - offset))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Relative movement is out of range");

  target = self->position + offset;

  if (target < 0)
    {
      gom_sqlite_statement_reset (self->statement);
      self->position = -1;
      self->on_row = FALSE;
      return dex_future_new_false ();
    }

  return gom_sqlite_cursor_move_absolute (cursor, (guint64)target);
}

static GomCursorCapabilities
gom_sqlite_cursor_get_capabilities (GomCursor *cursor)
{
  GomSqliteCursor *self = GOM_SQLITE_CURSOR (cursor);
  GomCursorCapabilities capabilities = GOM_CURSOR_CAPABILITIES_NONE;

  if (self->closed || self->statement == NULL)
    return GOM_CURSOR_CAPABILITIES_NONE;

  capabilities |= GOM_CURSOR_CAPABILITIES_REWIND;
  capabilities |= GOM_CURSOR_CAPABILITIES_ABSOLUTE;
  capabilities |= GOM_CURSOR_CAPABILITIES_RELATIVE;

  if (self->has_count)
    capabilities |= GOM_CURSOR_CAPABILITIES_COUNT;

  return capabilities;
}

static guint64
gom_sqlite_cursor_get_count (GomCursor *cursor)
{
  GomSqliteCursor *self = (GomSqliteCursor *)cursor;

  g_assert (GOM_IS_SQLITE_CURSOR (self));
  g_assert (self->has_count);

  return self->count;
}

static void
gom_sqlite_cursor_finalize (GObject *object)
{
  GomSqliteCursor *self = (GomSqliteCursor *)object;

  if (self->owns_transaction)
    gom_sqlite_cursor_commit_transaction (self, NULL);

  g_clear_object (&self->statement);
  g_clear_pointer (&self->sql, g_free);

  G_OBJECT_CLASS (gom_sqlite_cursor_parent_class)->finalize (object);
}

static void
gom_sqlite_cursor_class_init (GomSqliteCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomCursorClass *cursor_class = GOM_CURSOR_CLASS (klass);

  object_class->finalize = gom_sqlite_cursor_finalize;

  cursor_class->get_n_columns = gom_sqlite_cursor_get_n_columns;
  cursor_class->get_column_name = gom_sqlite_cursor_get_column_name;
  cursor_class->get_column_value = gom_sqlite_cursor_get_column_value;
  cursor_class->get_column_string = gom_sqlite_cursor_get_column_string;
  cursor_class->next = gom_sqlite_cursor_next;
  cursor_class->close = gom_sqlite_cursor_close;
  cursor_class->exhaust = gom_sqlite_cursor_exhaust;
  cursor_class->rewind = gom_sqlite_cursor_rewind;
  cursor_class->move_absolute = gom_sqlite_cursor_move_absolute;
  cursor_class->move_relative = gom_sqlite_cursor_move_relative;
  cursor_class->get_capabilities = gom_sqlite_cursor_get_capabilities;
  cursor_class->get_count = gom_sqlite_cursor_get_count;
}

static void
gom_sqlite_cursor_init (GomSqliteCursor *self)
{
  self->position = -1;
  self->on_row = FALSE;
}

GomSqliteCursor *
gom_sqlite_cursor_new (GomSqliteStatement *statement,
                       GomRepository      *repository,
                       char               *sql,
                       guint64             count,
                       gboolean            has_count,
                       gboolean            owns_transaction,
                       GType               entity_type)
{
  GomSqliteCursor *self;

  g_return_val_if_fail (GOM_IS_SQLITE_STATEMENT (statement), NULL);
  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (entity_type == G_TYPE_INVALID ||
                        g_type_is_a (entity_type, GOM_TYPE_ENTITY),
                        NULL);

  self = g_object_new (GOM_TYPE_SQLITE_CURSOR, NULL);
  self->statement = g_object_ref (statement);
  self->sql = sql;
  self->count = count;
  self->has_count = !!has_count;
  self->owns_transaction = !!owns_transaction;

  GOM_CURSOR (self)->entity_type = entity_type;
  _gom_cursor_set_repository (GOM_CURSOR (self), repository);

  return self;
}

/**
 * gom_sqlite_cursor_get_sql:
 * @self: a [class@Gom.SqliteCursor]
 *
 * Gets the SQL used to create the cursor, if available.
 *
 * Returns: (nullable):
 */
const char *
gom_sqlite_cursor_get_sql (GomSqliteCursor *self)
{
  g_return_val_if_fail (GOM_IS_SQLITE_CURSOR (self), NULL);

  return self->sql;
}
