/* gom-record.c
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

#include <gio/gio.h>

#include "gom-cursor-private.h"
#include "gom-record-private.h"

struct _GomRecord
{
  GObject parent_instance;

  guint    n_columns;
  char   **column_names;
  GValue  *values;
};

G_DEFINE_FINAL_TYPE (GomRecord, gom_record, G_TYPE_OBJECT)

static void
gom_record_finalize (GObject *object)
{
  GomRecord *self = GOM_RECORD (object);

  if (self->values != NULL)
    {
      for (guint i = 0; i < self->n_columns; i++)
        if (G_IS_VALUE (&self->values[i]))
          g_value_unset (&self->values[i]);

      g_clear_pointer (&self->values, g_free);
    }

  g_clear_pointer (&self->column_names, g_strfreev);

  G_OBJECT_CLASS (gom_record_parent_class)->finalize (object);
}

static void
gom_record_class_init (GomRecordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_record_finalize;
}

static void
gom_record_init (GomRecord *self)
{
}

static int
gom_record_find_column (GomRecord  *self,
                        const char *name)
{
  g_assert (GOM_IS_RECORD (self));
  g_assert (name != NULL);

  for (guint i = 0; i < self->n_columns; i++)
    {
      if (g_strcmp0 (self->column_names[i], name) == 0)
        return (int)i;
    }

  return -1;
}

static const GValue *
gom_record_get_column_value (GomRecord *self,
                             guint      column)
{
  const GValue *value;

  g_assert (GOM_IS_RECORD (self));

  if (column >= self->n_columns)
    return NULL;

  value = &self->values[column];
  if (!G_IS_VALUE (value))
    return NULL;

  return value;
}

static gboolean
gom_record_transform_column (GomRecord *self,
                             guint      column,
                             GType      value_type,
                             GValue    *out)
{
  const GValue *value;

  g_assert (GOM_IS_RECORD (self));
  g_assert (out != NULL);
  g_assert (value_type != G_TYPE_INVALID);

  if (!(value = gom_record_get_column_value (self, column)))
    return FALSE;

  if (!g_value_type_transformable (G_VALUE_TYPE (value), value_type))
    return FALSE;

  g_value_init (out, value_type);

  if (!g_value_transform (value, out))
    {
      g_value_unset (out);
      return FALSE;
    }

  return TRUE;
}

GomRecord *
_gom_record_new_from_values (const char * const *column_names,
                             const GValue       *values,
                             guint               n_columns)
{
  GomRecord *self;

  g_return_val_if_fail (n_columns == 0 || column_names != NULL, NULL);
  g_return_val_if_fail (n_columns == 0 || values != NULL, NULL);

  self = g_object_new (GOM_TYPE_RECORD, NULL);

  if (n_columns == 0)
    return self;

  self->n_columns = n_columns;
  self->values = g_new0 (GValue, n_columns);
  self->column_names = g_new0 (char *, n_columns + 1);

  for (guint i = 0; i < n_columns; i++)
    {
      self->column_names[i] = g_strdup (column_names[i]);
      g_value_init (&self->values[i], G_VALUE_TYPE (&values[i]));
      g_value_copy (&values[i], &self->values[i]);
    }

  return self;
}

GomRecord *
_gom_record_new (GomCursor  *cursor,
                 GError    **error)
{
  GomRecord *self = NULL;
  guint n_columns;
  const char **column_names = NULL;
  GValue *values = NULL;

  g_return_val_if_fail (GOM_IS_CURSOR (cursor), NULL);

  if (!(n_columns = gom_cursor_get_n_columns (cursor)))
    return g_object_new (GOM_TYPE_RECORD, NULL);

  column_names = g_new0 (const char *, n_columns);
  values = g_new0 (GValue, n_columns);

  for (guint i = 0; i < n_columns; i++)
    {
      const char *name = gom_cursor_get_column_name (cursor, i);

      column_names[i] = name;

      if (!GOM_CURSOR_GET_CLASS (cursor)->get_column_value (cursor, i, &values[i]))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to read cursor column %u",
                       i);
          goto cleanup;
        }
    }

  self = _gom_record_new_from_values (column_names, values, n_columns);

cleanup:
  for (guint i = 0; i < n_columns; i++)
    if (G_IS_VALUE (&values[i]))
      g_value_unset (&values[i]);

  g_free (column_names);
  g_free (values);

  return self;
}

guint
gom_record_get_n_columns (GomRecord *self)
{
  g_return_val_if_fail (GOM_IS_RECORD (self), 0);

  return self->n_columns;
}

const char *
gom_record_get_column_name (GomRecord *self,
                            guint      column)
{
  g_return_val_if_fail (GOM_IS_RECORD (self), NULL);

  if (column >= self->n_columns)
    return NULL;

  return self->column_names[column];
}

gboolean
gom_record_get_column (GomRecord *self,
                       guint      column,
                       GValue    *value)
{
  const GValue *src;

  g_return_val_if_fail (GOM_IS_RECORD (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (!(src = gom_record_get_column_value (self, column)))
    return FALSE;

  if (G_IS_VALUE (value))
    g_value_unset (value);

  g_value_init (value, G_VALUE_TYPE (src));
  g_value_copy (src, value);
  return TRUE;
}

/**
 * gom_record_get_column_boolean:
 * @self: a [class@Gom.Record]
 * @column: the column index
 *
 * Gets the boolean value for @column.
 *
 * If the column is not stored as a boolean, this tries to transform the
 * column value to a boolean first.
 *
 * Returns: the boolean value, or %FALSE
 */
gboolean
gom_record_get_column_boolean (GomRecord *self,
                               guint      column)
{
  const GValue *value;
  g_auto(GValue) transformed = G_VALUE_INIT;

  g_return_val_if_fail (GOM_IS_RECORD (self), FALSE);

  if (!(value = gom_record_get_column_value (self, column)))
    return FALSE;

  if (G_VALUE_HOLDS_BOOLEAN (value))
    return g_value_get_boolean (value);

  if (gom_record_transform_column (self, column, G_TYPE_BOOLEAN, &transformed))
    return g_value_get_boolean (&transformed);

  return FALSE;
}

/**
 * gom_record_get_column_string:
 * @self: a [class@Gom.Record]
 * @column: the column index
 *
 * Gets the string value for @column.
 *
 * Returns: (transfer none) (nullable): the string value, or %NULL
 */
const char *
gom_record_get_column_string (GomRecord *self,
                              guint      column)
{
  const GValue *value;

  g_return_val_if_fail (GOM_IS_RECORD (self), NULL);

  if (!(value = gom_record_get_column_value (self, column)))
    return NULL;

  if (G_VALUE_HOLDS_STRING (value))
    return g_value_get_string (value);

  return NULL;
}

/**
 * gom_record_get_column_int64:
 * @self: a [class@Gom.Record]
 * @column: the column index
 *
 * Gets the 64-bit integer value for @column.
 *
 * If the column is not stored as a 64-bit integer, this tries to transform the
 * column value to a 64-bit integer first.
 *
 * Returns: the integer value, or 0
 */
gint64
gom_record_get_column_int64 (GomRecord *self,
                             guint      column)
{
  const GValue *value;
  g_auto(GValue) transformed = G_VALUE_INIT;

  g_return_val_if_fail (GOM_IS_RECORD (self), 0);

  if (!(value = gom_record_get_column_value (self, column)))
    return 0;

  if (G_VALUE_HOLDS_INT64 (value))
    return g_value_get_int64 (value);

  if (gom_record_transform_column (self, column, G_TYPE_INT64, &transformed))
    return g_value_get_int64 (&transformed);

  return 0;
}

/**
 * gom_record_dup_column_string:
 * @self: a [class@Gom.Record]
 * @column: the column index
 *
 * Gets the string value for @column.
 *
 * Returns: (transfer full) (nullable): a newly allocated string, or %NULL
 */
char *
gom_record_dup_column_string (GomRecord *self,
                              guint      column)
{
  const char *str;

  g_return_val_if_fail (GOM_IS_RECORD (self), NULL);

  if (!(str = gom_record_get_column_string (self, column)))
    return NULL;

  return g_strdup (str);
}

/**
 * gom_record_dup_column_date_time:
 * @self: a [class@Gom.Record]
 * @column: the column index
 *
 * Gets the `GDateTime` value for @column.
 *
 * Returns: (transfer full) (nullable): a newly referenced datetime, or %NULL
 */
GDateTime *
gom_record_dup_column_date_time (GomRecord *self,
                                 guint      column)
{
  const GValue *value;

  g_return_val_if_fail (GOM_IS_RECORD (self), NULL);

  if (!(value = gom_record_get_column_value (self, column)))
    return NULL;

  if (!G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
    return NULL;

  return g_value_dup_boxed (value);
}

/**
 * gom_record_dup_column_bytes:
 * @self: a [class@Gom.Record]
 * @column: the column index
 *
 * Gets the `GBytes` value for @column.
 *
 * Returns: (transfer full) (nullable): a newly referenced bytes object, or
 *   %NULL
 */
GBytes *
gom_record_dup_column_bytes (GomRecord *self,
                             guint      column)
{
  const GValue *value;

  g_return_val_if_fail (GOM_IS_RECORD (self), NULL);

  if (!(value = gom_record_get_column_value (self, column)))
    return NULL;

  if (!G_VALUE_HOLDS (value, G_TYPE_BYTES))
    return NULL;

  return g_value_dup_boxed (value);
}

gboolean
gom_record_get_column_by_name (GomRecord  *self,
                               const char *name,
                               GValue     *value)
{
  int column;

  g_return_val_if_fail (GOM_IS_RECORD (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if ((column = gom_record_find_column (self, name)) < 0)
    return FALSE;

  return gom_record_get_column (self, (guint)column, value);
}
