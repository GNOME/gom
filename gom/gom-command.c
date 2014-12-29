/* gom-command.c
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
#include <string.h>

#include "gom-adapter.h"
#include "gom-command.h"
#include "gom-error.h"

G_DEFINE_TYPE(GomCommand, gom_command, G_TYPE_OBJECT)

struct _GomCommandPrivate
{
   GomAdapter   *adapter;
   gchar        *sql;
   sqlite3_stmt *stmt;
   GHashTable   *params;

   GPtrArray    *blobs;
};

enum
{
   PROP_0,
   PROP_ADAPTER,
   PROP_SQL,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

static void
gom_command_bind_param (GomCommand   *command,
                        guint         param,
                        const GValue *value)
{
   GomCommandPrivate *priv;

   g_return_if_fail(GOM_IS_COMMAND(command));
   g_return_if_fail(value != NULL);
   g_return_if_fail(G_VALUE_TYPE(value));

   priv = command->priv;

   g_debug("Binding gtype %s (%d).", g_type_name(G_VALUE_TYPE(value)), (int) G_VALUE_TYPE(value));

   switch (G_VALUE_TYPE(value)) {
   case G_TYPE_BOOLEAN:
      sqlite3_bind_int(priv->stmt, param, g_value_get_boolean(value));
      break;
   case G_TYPE_DOUBLE:
      sqlite3_bind_double(priv->stmt, param, g_value_get_double(value));
      break;
   case G_TYPE_FLOAT:
      sqlite3_bind_double(priv->stmt, param, g_value_get_float(value));
      break;
   case G_TYPE_CHAR:
      sqlite3_bind_int(priv->stmt, param, g_value_get_schar(value));
      break;
   case G_TYPE_INT:
      sqlite3_bind_int(priv->stmt, param, g_value_get_int(value));
      break;
   case G_TYPE_INT64:
      sqlite3_bind_int64(priv->stmt, param, g_value_get_int64(value));
      break;
   case G_TYPE_LONG:
      sqlite3_bind_int64(priv->stmt, param, g_value_get_long(value));
      break;
   case G_TYPE_UCHAR:
      sqlite3_bind_int(priv->stmt, param, g_value_get_uchar(value));
      break;
   case G_TYPE_UINT:
      sqlite3_bind_int(priv->stmt, param, g_value_get_uint(value));
      break;
   case G_TYPE_UINT64:
      sqlite3_bind_int64(priv->stmt, param, g_value_get_uint64(value));
      break;
   case G_TYPE_ULONG:
      sqlite3_bind_int64(priv->stmt, param, g_value_get_ulong(value));
      break;
   case G_TYPE_ENUM:
      sqlite3_bind_int(priv->stmt, param, g_value_get_enum(value));
      break;
   case G_TYPE_FLAGS:
      sqlite3_bind_int(priv->stmt, param, g_value_get_flags(value));
      break;
   case G_TYPE_STRING:
      sqlite3_bind_text(priv->stmt, param,
                        g_value_dup_string(value), -1, g_free);
      break;
   default:
      if (G_VALUE_TYPE(value) == G_TYPE_DATE_TIME) {
         GTimeVal tv = { 0 };
         GDateTime *dt = g_value_get_boxed(value);
         gchar *iso8601;
         if (dt) {
            g_date_time_to_timeval(dt, &tv);
         }
         iso8601 = g_time_val_to_iso8601(&tv);
         sqlite3_bind_text(priv->stmt, param, iso8601, -1, g_free);
         break;
      } else if (G_VALUE_TYPE(value) == G_TYPE_STRV) {
         GByteArray *bytes = g_byte_array_new();
         gchar **strv = g_value_get_boxed(value);
         guchar null_byte[] = { 0 };
         guint i;

         if (strv) {
            for (i = 0; strv[i]; i++) {
               g_byte_array_append(bytes, (guchar *)strv[i], strlen(strv[i]) + 1);
            }
         }
         g_byte_array_append(bytes, null_byte, 1);
         sqlite3_bind_blob(priv->stmt, param, bytes->data, bytes->len, g_free);
         g_byte_array_free(bytes, FALSE);
         break;
      } else if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_FLAGS)) {
         sqlite3_bind_int(priv->stmt, param, g_value_get_flags(value));
         break;
      } else if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_ENUM)) {
         sqlite3_bind_int(priv->stmt, param, g_value_get_enum(value));
         break;
      } else if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_BYTES)) {
         GBytes *bytes;

         bytes = g_value_get_boxed(value);
         if (!bytes) {
            sqlite3_bind_blob(priv->stmt, param,
                              NULL, 0, NULL);
         } else {
            sqlite3_bind_blob(priv->stmt, param,
                              g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes), NULL);
            g_ptr_array_add(priv->blobs, g_bytes_ref(bytes));
         }
         break;
      }
      g_warning("Failed to bind gtype %s (%d).", g_type_name(G_VALUE_TYPE(value)), (int) G_VALUE_TYPE(value));
      g_assert_not_reached();
      break;
   }
}

static void
gom_command_bind_params (GomCommand *command)
{
   GomCommandPrivate *priv;
   GHashTableIter iter;
   gpointer key;
   gpointer value;

   g_return_if_fail(GOM_IS_COMMAND(command));
   g_return_if_fail(command->priv->stmt);

   priv = command->priv;

   if (priv->params) {
      g_hash_table_iter_init(&iter, priv->params);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         gom_command_bind_param(command, *(guint *)key, value);
      }
   }
}

void
gom_command_set_sql (GomCommand  *command,
                     const gchar *sql)
{
   GomCommandPrivate *priv;

   g_return_if_fail(GOM_IS_COMMAND(command));

   priv = command->priv;

   g_free(priv->sql);
   priv->sql = g_strdup(sql);
}

static void
_g_value_free (gpointer value)
{
   if (value) {
      g_value_unset(value);
      g_free(value);
   }
}

void
gom_command_set_param (GomCommand   *command,
                       guint         param,
                       const GValue *value)
{
   GomCommandPrivate *priv;
   GValue *dst_value;
   guint *key;

   g_return_if_fail(GOM_IS_COMMAND(command));
   g_return_if_fail(value != NULL);
   g_return_if_fail(G_VALUE_TYPE(value));

   priv = command->priv;

   if (!priv->params) {
      priv->params = g_hash_table_new_full(g_int_hash, g_int_equal,
                                           g_free, _g_value_free);
   }

   key = g_new0(guint, 1);
   *key = param + 1;
   dst_value = g_new0(GValue, 1);
   g_value_init(dst_value, G_VALUE_TYPE(value));
   g_value_copy(value, dst_value);
   g_hash_table_replace(priv->params, key, dst_value);
}

#define SET_PARAM_HELPER(intype, name, TYPE)       \
void                                               \
gom_command_set_param_##name (GomCommand *command, \
                              guint       param,   \
                              intype      value)   \
{                                                  \
   GValue v = { 0 };                               \
   g_value_init(&v, G_TYPE_##TYPE);                \
   g_value_set_##name(&v, value);                  \
   gom_command_set_param(command, param, &v);      \
   g_value_unset(&v);                              \
}

SET_PARAM_HELPER(gdouble, double, DOUBLE)
SET_PARAM_HELPER(gfloat, float, FLOAT)
SET_PARAM_HELPER(gint, int, INT)
SET_PARAM_HELPER(gint64, int64, INT64)
SET_PARAM_HELPER(guint, uint, UINT)
SET_PARAM_HELPER(guint64, uint64, UINT64)
SET_PARAM_HELPER(const gchar *, string, STRING)

gint
gom_command_get_param_index (GomCommand  *command,
                             const gchar *param_name)
{
   g_return_val_if_fail(GOM_IS_COMMAND(command), -1);
   g_return_val_if_fail(param_name != NULL, -1);

   if (!command->priv->stmt) {
      g_warning("Cannot get param, no SQL provided.");
      return -1;
   }

   return sqlite3_bind_parameter_index(command->priv->stmt, param_name) - 1;
}

static gboolean
gom_command_prepare (GomCommand  *command,
                     sqlite3     *db,
                     GError     **error)
{
   GomCommandPrivate *priv;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_COMMAND(command), FALSE);

   priv = command->priv;

   if (priv->stmt) {
      sqlite3_finalize(priv->stmt);
      priv->stmt = NULL;
   }

   if (!priv->sql) {
      g_set_error(error, GOM_ERROR, GOM_ERROR_COMMAND_NO_SQL,
                  _("The command does not contain any SQL"));
      return FALSE;
   }

   if (!(ret = (SQLITE_OK == sqlite3_prepare_v2(db, priv->sql, -1,
                                                &priv->stmt, NULL)))) {
      g_set_error(error, GOM_ERROR, GOM_ERROR_COMMAND_SQLITE,
                  _("sqlite3_prepare_v2 failed: %s: %s"),
                  sqlite3_errmsg(db), priv->sql);
   }

   return ret;
}

gboolean
gom_command_execute (GomCommand  *command,
                     GomCursor  **cursor,
                     GError     **error)
{
   GomCommandPrivate *priv;
   gboolean ret = FALSE;
   sqlite3 *db;
   gint code;

   g_return_val_if_fail(GOM_IS_COMMAND(command), FALSE);

   priv = command->priv;

   if (cursor) {
      *cursor = NULL;
   }

   if (!priv->adapter || !(db = gom_adapter_get_handle(priv->adapter))) {
      g_set_error(error, GOM_ERROR, GOM_ERROR_COMMAND_SQLITE,
                  _("Failed to access SQLite handle."));
      return FALSE;
   }

   if (!priv->stmt) {
      if (!gom_command_prepare(command, db, error)) {
         return FALSE;
      }
   }

   gom_command_reset(command);
   gom_command_bind_params(command);

#if 0
   g_print("%s", sqlite3_sql(priv->stmt));

   if (priv->params != NULL) {
      GHashTableIter iter;
      gpointer key;
      gpointer value;
      gboolean first = TRUE;

      g_print(" (");

      g_hash_table_iter_init(&iter, priv->params);
      while (g_hash_table_iter_next(&iter, &key, &value)) {
         GValue *v = (GValue *)value;

         if (!first)
            g_print(", ");

         if (G_VALUE_HOLDS_BOOLEAN(v)) {
            gboolean b = g_value_get_boolean(v);
            g_print("%s", b ? "TRUE" : "FALSE");
         } else if (G_VALUE_HOLDS_INT(v)) {
            gint i = g_value_get_int(v);
            g_print("%d", i);
         } else if (G_VALUE_HOLDS_INT64(v)) {
            gint64 i = g_value_get_int64(v);
            g_print("%" G_GINT64_FORMAT, i);
         } else if (G_VALUE_HOLDS_STRING(v)) {
            const gchar *s = g_value_get_string(v);
            g_print("'%s'", s);
         } else if (G_VALUE_HOLDS_UCHAR(v)) {
            guchar i = g_value_get_uchar(v);
            g_print("%c", i);
         } else if (G_VALUE_HOLDS_UINT(v)) {
            guint i = g_value_get_uint(v);
            g_print("%u", i);
         } else {
            g_print("'unknown'");
         }

         first = FALSE;
      }

      g_print(")");
   }

   g_print("\n");
#endif

   if (!cursor) {
      /*
       * The caller does not care about the result. Step through
       * the statement once and then return.
       */
      code = sqlite3_step(priv->stmt);
      if (!(ret = (code == SQLITE_ROW || code == SQLITE_DONE))) {
         g_set_error(error, GOM_ERROR,
                     GOM_ERROR_COMMAND_SQLITE,
                     "Failed to execute statement: %s", sqlite3_errmsg(db));
      }
      return ret;
   }

   *cursor = g_object_new(GOM_TYPE_CURSOR,
                          "statement", priv->stmt,
                          NULL);

   return TRUE;
}

void
gom_command_reset (GomCommand *command)
{
   GomCommandPrivate *priv;

   g_return_if_fail(GOM_IS_COMMAND(command));

   priv = command->priv;

   if (priv->stmt) {
      sqlite3_clear_bindings(priv->stmt);
      sqlite3_reset(priv->stmt);

      g_ptr_array_unref(priv->blobs);
      priv->blobs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
   }
}

static GomAdapter *
gom_command_get_adapter (GomCommand *command)
{
   g_return_val_if_fail(GOM_IS_COMMAND(command), NULL);
   return command->priv->adapter;
}

static void
gom_command_set_adapter (GomCommand *command,
                         GomAdapter *adapter)
{
   GomCommandPrivate *priv = command->priv;

   if (priv->adapter) {
      g_object_remove_weak_pointer(G_OBJECT(priv->adapter),
                                   (gpointer *)&priv->adapter);
      priv->adapter = NULL;
   }

   if (adapter) {
      priv->adapter = adapter;
      g_object_add_weak_pointer(G_OBJECT(priv->adapter),
                                (gpointer *)&priv->adapter);
   }
}

/**
 * gom_command_finalize:
 * @object: (in): A #GomCommand.
 *
 * Finalizer for a #GomCommand instance.  Frees any resources held by
 * the instance.
 */
static void
gom_command_finalize (GObject *object)
{
   GomCommandPrivate *priv = GOM_COMMAND(object)->priv;

   g_free(priv->sql);

   gom_command_set_adapter(GOM_COMMAND(object), NULL);

   if (priv->stmt) {
      sqlite3_finalize(priv->stmt);
   }

   if (priv->params) {
      g_hash_table_destroy(priv->params);
   }

   if (priv->blobs) {
      g_ptr_array_unref(priv->blobs);
   }

   G_OBJECT_CLASS(gom_command_parent_class)->finalize(object);
}

/**
 * gom_command_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_command_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
   GomCommand *command = GOM_COMMAND(object);

   switch (prop_id) {
   case PROP_ADAPTER:
      g_value_set_object(value, gom_command_get_adapter(command));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_command_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_command_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
   GomCommand *command = GOM_COMMAND(object);

   switch (prop_id) {
   case PROP_ADAPTER:
      gom_command_set_adapter(command, g_value_get_object(value));
      break;
   case PROP_SQL:
      gom_command_set_sql(command, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_command_class_init:
 * @klass: (in): A #GomCommandClass.
 *
 * Initializes the #GomCommandClass and prepares the vtable.
 */
static void
gom_command_class_init (GomCommandClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_command_finalize;
   object_class->get_property = gom_command_get_property;
   object_class->set_property = gom_command_set_property;
   g_type_class_add_private(object_class, sizeof(GomCommandPrivate));

   gParamSpecs[PROP_ADAPTER] =
      g_param_spec_object("adapter",
                          _("Adapter"),
                          _("The GomAdapter for the command."),
                          GOM_TYPE_ADAPTER,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_ADAPTER,
                                   gParamSpecs[PROP_ADAPTER]);

   gParamSpecs[PROP_SQL] =
      g_param_spec_string("sql",
                          _("SQL"),
                          _("The SQL for the command."),
                          NULL,
                          G_PARAM_WRITABLE);
   g_object_class_install_property(object_class, PROP_SQL,
                                   gParamSpecs[PROP_SQL]);
}

/**
 * gom_command_init:
 * @command: (in): A #GomCommand.
 *
 * Initializes the newly created #GomCommand instance.
 */
static void
gom_command_init (GomCommand *command)
{
   command->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(command,
                                  GOM_TYPE_COMMAND,
                                  GomCommandPrivate);
   command->priv->blobs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
}
