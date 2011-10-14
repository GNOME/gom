/* gom-cursor.c
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

#include "gom-cursor.h"

G_DEFINE_TYPE(GomCursor, gom_cursor, G_TYPE_OBJECT)

struct _GomCursorPrivate
{
   sqlite3_stmt *stmt;
   guint n_columns;
};

enum
{
   PROP_0,
   PROP_STATEMENT,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

void
gom_cursor_get_column (GomCursor *cursor,
                       guint      column,
                       GValue    *value)
{
   GomCursorPrivate *priv;
   int sql_type;

   g_return_if_fail(GOM_IS_CURSOR(cursor));
   g_return_if_fail(column < cursor->priv->n_columns);
   g_return_if_fail(value != NULL);

   priv = cursor->priv;

   switch (G_VALUE_TYPE(value)) {
   case G_TYPE_BOOLEAN:
      g_value_set_boolean(value, !!sqlite3_column_int(priv->stmt, column));
      break;
   case G_TYPE_DOUBLE:
      g_value_set_double(value, sqlite3_column_double(priv->stmt, column));
      break;
   case G_TYPE_FLOAT:
      g_value_set_float(value, sqlite3_column_double(priv->stmt, column));
      break;
   case G_TYPE_INT:
      g_value_set_int(value, sqlite3_column_int(priv->stmt, column));
      break;
   case G_TYPE_INT64:
      g_value_set_int64(value, sqlite3_column_int64(priv->stmt, column));
      break;
   case G_TYPE_STRING:
      g_value_set_string(value,
                         (gchar *)sqlite3_column_text(priv->stmt, column));
      break;
   case G_TYPE_UINT:
      g_value_set_uint(value, sqlite3_column_int(priv->stmt, column));
      break;
   case G_TYPE_UINT64:
      g_value_set_uint64(value, sqlite3_column_int64(priv->stmt, column));
      break;
   default:
      if (G_VALUE_TYPE(value) == G_TYPE_DATE_TIME) {
         GTimeVal tv = { 0 };
         GDateTime *dt;
         const gchar *iso8601 = (gchar *)sqlite3_column_text(priv->stmt, column);
         if (iso8601) {
            g_time_val_from_iso8601(iso8601, &tv);
         }
         dt = g_date_time_new_from_timeval_utc(&tv);
         g_value_take_boxed(value, dt);
         break;
      }
      if (G_VALUE_TYPE(value) == G_TYPE_STRV) {
         const gchar *blob;
         GPtrArray *ar = g_ptr_array_new();
         gchar *str;
         guint bytes;
         guint i;

         bytes = sqlite3_column_bytes(priv->stmt, column);
         blob = sqlite3_column_blob(priv->stmt, column);

         for (i = 0; i < bytes && blob[i]; i++) {
            str = g_strdup(&blob[i]);
            g_ptr_array_add(ar, str);
            i += strlen(str);
         }

         g_ptr_array_add(ar, NULL);

         g_value_take_boxed(value, (gchar **)ar->pdata);
         g_ptr_array_free(ar, FALSE);
         break;
      }
      if (G_VALUE_TYPE(value)) {
         g_value_unset(value);
      }
      sql_type = sqlite3_column_type(priv->stmt, column);
      switch (sql_type) {
      case SQLITE_INTEGER:
         g_value_init(value, G_TYPE_INT64);
         g_value_set_int64(value, sqlite3_column_int64(priv->stmt, column));
         break;
      case SQLITE_FLOAT:
         g_value_init(value, G_TYPE_DOUBLE);
         g_value_set_double(value, sqlite3_column_double(priv->stmt, column));
         break;
      case SQLITE_TEXT:
         g_value_init(value, G_TYPE_STRING);
         g_value_set_string(value,
                            (gchar *)sqlite3_column_text(priv->stmt, column));
         break;
      default:
         g_assert_not_reached();
      }
      break;
   }
}

#define GET_COLUMN_HELPER(rtype, vtype, VTYPE)    \
rtype                                             \
gom_cursor_get_column_##vtype (GomCursor *cursor, \
                               guint      column) \
{                                                 \
   GValue val = { 0 };                            \
   rtype ret;                                     \
                                                  \
   g_value_init(&val, G_TYPE_##VTYPE);            \
   gom_cursor_get_column(cursor, column, &val);   \
   ret = g_value_get_##vtype(&val);               \
   g_value_unset(&val);                           \
   return ret;                                    \
}

GET_COLUMN_HELPER(gdouble, double, DOUBLE)
GET_COLUMN_HELPER(gfloat, float, FLOAT)
GET_COLUMN_HELPER(gint, int, INT)
GET_COLUMN_HELPER(gint64, int64, INT64)
GET_COLUMN_HELPER(guint, uint, UINT)
GET_COLUMN_HELPER(guint64, uint64, UINT64)

const gchar *
gom_cursor_get_column_string (GomCursor *cursor,
                              guint      column)
{
   g_return_val_if_fail(GOM_IS_CURSOR(cursor), NULL);
   g_return_val_if_fail(column < cursor->priv->n_columns, NULL);

   return (gchar *)sqlite3_column_text(cursor->priv->stmt, column);
}

guint
gom_cursor_get_n_columns (GomCursor *cursor)
{
   g_return_val_if_fail(GOM_IS_CURSOR(cursor), 0);
   return cursor->priv->n_columns;
}

const gchar *
gom_cursor_get_column_name (GomCursor *cursor,
                            guint      column)
{
   g_return_val_if_fail(GOM_IS_CURSOR(cursor), NULL);
   return sqlite3_column_name(cursor->priv->stmt, column);
}

gboolean
gom_cursor_next (GomCursor *cursor)
{
   g_return_val_if_fail(GOM_IS_CURSOR(cursor), FALSE);
   return (sqlite3_step(cursor->priv->stmt) == SQLITE_ROW);
}

/**
 * gom_cursor_finalize:
 * @object: (in): A #GomCursor.
 *
 * Finalizer for a #GomCursor instance.  Frees any resources held by
 * the instance.
 */
static void
gom_cursor_finalize (GObject *object)
{
   G_OBJECT_CLASS(gom_cursor_parent_class)->finalize(object);
}

/**
 * gom_cursor_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_cursor_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
   GomCursor *cursor = GOM_CURSOR(object);

   switch (prop_id) {
   case PROP_STATEMENT:
      g_value_set_pointer(value, cursor->priv->stmt);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_cursor_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_cursor_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
   GomCursor *cursor = GOM_CURSOR(object);

   switch (prop_id) {
   case PROP_STATEMENT:
      cursor->priv->stmt = g_value_get_pointer(value);
      cursor->priv->n_columns = sqlite3_column_count(cursor->priv->stmt);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_cursor_class_init:
 * @klass: (in): A #GomCursorClass.
 *
 * Initializes the #GomCursorClass and prepares the vtable.
 */
static void
gom_cursor_class_init (GomCursorClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_cursor_finalize;
   object_class->get_property = gom_cursor_get_property;
   object_class->set_property = gom_cursor_set_property;
   g_type_class_add_private(object_class, sizeof(GomCursorPrivate));

   gParamSpecs[PROP_STATEMENT] =
      g_param_spec_pointer("statement",
                          _("Statement"),
                          _("A pointer to a sqlite3_stmt."),
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_STATEMENT,
                                   gParamSpecs[PROP_STATEMENT]);
}

/**
 * gom_cursor_init:
 * @cursor: (in): A #GomCursor.
 *
 * Initializes the newly created #GomCursor instance.
 */
static void
gom_cursor_init (GomCursor *cursor)
{
   cursor->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(cursor,
                                  GOM_TYPE_CURSOR,
                                  GomCursorPrivate);
}
