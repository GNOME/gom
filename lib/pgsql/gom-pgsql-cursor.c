/* gom-pgsql-cursor.c
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

#include <string.h>

#include "gom-meta.h"
#include "gom-pgsql-cursor-private.h"
#include "gom-repository.h"
#include "gom-trace-private.h"

struct _GomPgsqlCursor
{
  GomCursor parent_instance;

  PgsqlResult *result;
  gint64       position;
  gboolean     on_row;
  gboolean     closed;
  guint64      count;
  guint        has_count : 1;
};

struct _GomPgsqlCursorClass
{
  GomCursorClass parent_class;
};

static gboolean               gom_pgsql_parse_bytea_hex          (const char  *text,
                                                                  GBytes     **out_bytes);
static void                   gom_pgsql_cursor_finalize          (GObject     *object);
static guint                  gom_pgsql_cursor_get_n_columns     (GomCursor   *cursor);
static const char            *gom_pgsql_cursor_get_column_name   (GomCursor   *cursor,
                                                                  guint        column);
static gboolean               gom_pgsql_cursor_get_column_value  (GomCursor   *cursor,
                                                                  guint        column,
                                                                  GValue      *value);
static const char            *gom_pgsql_cursor_get_column_string (GomCursor   *cursor,
                                                                  guint        column);
static DexFuture             *gom_pgsql_cursor_next              (GomCursor   *cursor);
static DexFuture             *gom_pgsql_cursor_close             (GomCursor   *cursor);
static DexFuture             *gom_pgsql_cursor_exhaust           (GomCursor   *cursor);
static DexFuture             *gom_pgsql_cursor_rewind            (GomCursor   *cursor);
static DexFuture             *gom_pgsql_cursor_move_absolute     (GomCursor   *cursor,
                                                                  guint64      position);
static DexFuture             *gom_pgsql_cursor_move_relative     (GomCursor   *cursor,
                                                                  gint64       offset);
static GomCursorCapabilities  gom_pgsql_cursor_get_capabilities  (GomCursor   *cursor);
static guint64                gom_pgsql_cursor_get_count         (GomCursor   *cursor);

G_DEFINE_FINAL_TYPE (GomPgsqlCursor, gom_pgsql_cursor, GOM_TYPE_CURSOR)

static gboolean
gom_pgsql_parse_bytea_hex (const char  *text,
                           GBytes     **out_bytes)
{
  gsize offset = 0;
  gsize len;
  guint8 *data;

  if (text == NULL)
    {
      *out_bytes = NULL;
      return TRUE;
    }

  if (g_str_has_prefix (text, "\\x"))
    offset = 2;

  len = strlen (text + offset);
  if (len % 2 != 0)
    return FALSE;

  data = g_new0 (guint8, len / 2);
  for (gsize i = 0; i < len / 2; i++)
    {
      int hi = g_ascii_xdigit_value (text[offset + i * 2]);
      int lo = g_ascii_xdigit_value (text[offset + i * 2 + 1]);

      if (hi < 0 || lo < 0)
        {
          g_free (data);
          return FALSE;
        }

      data[i] = (guint8)((hi << 4) | lo);
    }

  *out_bytes = g_bytes_new_take (data, len / 2);
  return TRUE;
}

gboolean
gom_pgsql_cursor_set_value (PgsqlResult *result,
                            guint        row,
                            guint        column,
                            GValue      *value)
{
  PgsqlValueType type;
  const char *text;

  if (!(text = pgsql_result_get_value (result, row, column)))
    {
      g_value_init (value, G_TYPE_POINTER);
      g_value_set_pointer (value, NULL);
      return TRUE;
    }

  type = pgsql_result_get_field_type (result, column);

  switch (type)
    {
    case PGSQL_VALUE_TYPE_INVALID:
    case PGSQL_VALUE_TYPE_TEXT:
    case PGSQL_VALUE_TYPE_VARCHAR:
    case PGSQL_VALUE_TYPE_NUMERIC:
    case PGSQL_VALUE_TYPE_DATE:
    case PGSQL_VALUE_TYPE_TIME:
    case PGSQL_VALUE_TYPE_TIMESTAMP:
    case PGSQL_VALUE_TYPE_TIMESTAMPTZ:
    case PGSQL_VALUE_TYPE_UUID:
    case PGSQL_VALUE_TYPE_JSON:
    case PGSQL_VALUE_TYPE_JSONB:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, text);
      break;
    case PGSQL_VALUE_TYPE_BOOL:
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, g_strcmp0 (text, "t") == 0 || g_strcmp0 (text, "true") == 0 || g_strcmp0 (text, "1") == 0);
      break;
    case PGSQL_VALUE_TYPE_INT2:
    case PGSQL_VALUE_TYPE_INT4:
    case PGSQL_VALUE_TYPE_INT8:
      g_value_init (value, G_TYPE_INT64);
      g_value_set_int64 (value, g_ascii_strtoll (text, NULL, 10));
      break;
    case PGSQL_VALUE_TYPE_FLOAT4:
    case PGSQL_VALUE_TYPE_FLOAT8:
      g_value_init (value, G_TYPE_DOUBLE);
      g_value_set_double (value, g_ascii_strtod (text, NULL));
      break;
    case PGSQL_VALUE_TYPE_BYTEA:
      {
        g_autoptr(GBytes) bytes = NULL;
        if (!gom_pgsql_parse_bytea_hex (text, &bytes))
          return FALSE;
        g_value_init (value, G_TYPE_BYTES);
        g_value_take_boxed (value, g_steal_pointer (&bytes));
      }
      break;
    default:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, text);
      break;
    }

  return TRUE;
}

static void
gom_pgsql_cursor_finalize (GObject *object)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (object);

  g_clear_object (&self->result);

  G_OBJECT_CLASS (gom_pgsql_cursor_parent_class)->finalize (object);
}

static void
gom_pgsql_cursor_class_init (GomPgsqlCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomCursorClass *cursor_class = GOM_CURSOR_CLASS (klass);

  object_class->finalize = gom_pgsql_cursor_finalize;
  cursor_class->get_n_columns = gom_pgsql_cursor_get_n_columns;
  cursor_class->get_column_name = gom_pgsql_cursor_get_column_name;
  cursor_class->get_column_value = gom_pgsql_cursor_get_column_value;
  cursor_class->get_column_string = gom_pgsql_cursor_get_column_string;
  cursor_class->next = gom_pgsql_cursor_next;
  cursor_class->close = gom_pgsql_cursor_close;
  cursor_class->exhaust = gom_pgsql_cursor_exhaust;
  cursor_class->rewind = gom_pgsql_cursor_rewind;
  cursor_class->move_absolute = gom_pgsql_cursor_move_absolute;
  cursor_class->move_relative = gom_pgsql_cursor_move_relative;
  cursor_class->get_capabilities = gom_pgsql_cursor_get_capabilities;
  cursor_class->get_count = gom_pgsql_cursor_get_count;
}

static void
gom_pgsql_cursor_init (GomPgsqlCursor *self)
{
  self->position = -1;
  self->count = 0;
  self->has_count = FALSE;
}

static guint
gom_pgsql_cursor_get_n_columns (GomCursor *cursor)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  return (self->closed || self->result == NULL) ? 0 : pgsql_result_get_n_fields (self->result);
}

static const char *
gom_pgsql_cursor_get_column_name (GomCursor *cursor,
                                  guint      column)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  return (self->closed || self->result == NULL)
           ? NULL
           : pgsql_result_get_field_name (self->result, column);
}

static gboolean
gom_pgsql_cursor_get_column_value (GomCursor *cursor,
                                   guint      column,
                                   GValue    *value)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  if (self->closed || self->result == NULL || !self->on_row)
    return FALSE;

  return gom_pgsql_cursor_set_value (self->result, (guint)self->position, column, value);
}

static const char *
gom_pgsql_cursor_get_column_string (GomCursor *cursor,
                                    guint      column)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  return (self->closed || self->result == NULL || !self->on_row)
           ? NULL
           : pgsql_result_get_value (self->result, (guint)self->position, column);
}

static DexFuture *
gom_pgsql_cursor_next (GomCursor *cursor)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);
  guint n_rows;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  if (self->closed || self->result == NULL)
    {
      GOM_TRACE_END_MARK (start_time, "Cursor", "next", "pgsql closed");
      return dex_future_new_false ();
    }

  n_rows = pgsql_result_get_n_rows (self->result);
  self->position++;
  self->on_row = (guint)self->position < n_rows;
  GOM_TRACE_END_MARK (start_time, "Cursor", "next", "pgsql %s", self->on_row ? "row" : "done");
  return self->on_row ? dex_future_new_true () : dex_future_new_false ();
}

static DexFuture *
gom_pgsql_cursor_close (GomCursor *cursor)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  self->closed = TRUE;
  self->on_row = FALSE;
  g_clear_object (&self->result);

  GOM_TRACE_END_MARK (start_time, "Cursor", "close", "pgsql closed");
  return dex_future_new_true ();
}

static DexFuture *
gom_pgsql_cursor_exhaust (GomCursor *cursor)
{
  return gom_pgsql_cursor_close (cursor);
}

static DexFuture *
gom_pgsql_cursor_rewind (GomCursor *cursor)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  if (self->closed || self->result == NULL)
    return dex_future_new_false ();

  self->position = -1;
  self->on_row = FALSE;
  return dex_future_new_true ();
}

static DexFuture *
gom_pgsql_cursor_move_absolute (GomCursor *cursor,
                                guint64    position)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);
  guint n_rows;

  if (self->closed || self->result == NULL)
    return dex_future_new_false ();

  n_rows = pgsql_result_get_n_rows (self->result);
  if (position >= n_rows)
    {
      self->position = (gint64)n_rows;
      self->on_row = FALSE;
      return dex_future_new_false ();
    }

  self->position = (gint64)position;
  self->on_row = TRUE;
  return dex_future_new_true ();
}

static DexFuture *
gom_pgsql_cursor_move_relative (GomCursor *cursor,
                                gint64     offset)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  if (self->closed || self->result == NULL)
    return dex_future_new_false ();

  return gom_pgsql_cursor_move_absolute (cursor, (guint64)MAX (0, self->position + offset));
}

static GomCursorCapabilities
gom_pgsql_cursor_get_capabilities (GomCursor *cursor)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  return (self->closed || self->result == NULL)
           ? GOM_CURSOR_CAPABILITIES_NONE
           : (GOM_CURSOR_CAPABILITIES_REWIND | GOM_CURSOR_CAPABILITIES_ABSOLUTE |
              GOM_CURSOR_CAPABILITIES_RELATIVE |
              (self->has_count ? GOM_CURSOR_CAPABILITIES_COUNT : GOM_CURSOR_CAPABILITIES_NONE));
}

static guint64
gom_pgsql_cursor_get_count (GomCursor *cursor)
{
  GomPgsqlCursor *self = GOM_PGSQL_CURSOR (cursor);

  g_assert (self->has_count);

  return self->count;
}

GomPgsqlCursor *
gom_pgsql_cursor_new (PgsqlResult   *result,
                      GomRepository *repository,
                      guint64        count,
                      gboolean       has_count)
{
  GomPgsqlCursor *self;

  g_return_val_if_fail (PGSQL_IS_RESULT (result), NULL);
  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);

  self = g_object_new (GOM_TYPE_PGSQL_CURSOR, NULL);
  self->result = g_object_ref (result);
  self->count = count;
  self->has_count = !!has_count;

  _gom_cursor_set_repository (GOM_CURSOR (self), repository);

  return self;
}
