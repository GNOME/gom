/* gom-cursor.c
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

#include "gom-cursor-private.h"
#include "gom-entity-private.h"
#include "gom-meta-private.h"
#include "gom-session-private.h"
#include "gom-repository-private.h"
#include "gom-record-private.h"
#include "gom-trace-private.h"
#include "gom-util-private.h"
#include "gom-value-private.h"

G_DEFINE_ABSTRACT_TYPE (GomCursor, gom_cursor, G_TYPE_OBJECT)

static void
gom_cursor_finalize (GObject *object)
{
  GomCursor *self = (GomCursor *)object;

  g_clear_object (&self->repository);
  g_clear_object (&self->session);
  g_clear_pointer (&self->discriminator_cache, g_hash_table_unref);
  gom_trace_counter_add (GOM_TRACE_COUNTER_CURSORS, -1);

  G_OBJECT_CLASS (gom_cursor_parent_class)->finalize (object);
}

static DexFuture *
gom_cursor_exhaust_fiber (gpointer user_data)
{
  GomCursor *self = user_data;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_CURSOR (self));

  for (;;)
    {
      gboolean r;

      r = dex_await_boolean (gom_cursor_next (self), &error);

      if (error != NULL)
        {
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (r == FALSE)
        break;
    }

  if (!dex_await (gom_cursor_close (self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
gom_cursor_exhaust_to_list_fiber (gpointer user_data)
{
  GomCursor *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GListStore) result = NULL;

  g_assert (GOM_IS_CURSOR (self));

  if (self->entity_type == G_TYPE_INVALID)
    {
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Cursor does not have an entity type");
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  result = g_list_store_new (self->entity_type);

  for (;;)
    {
      g_autoptr(GomEntity) entity = NULL;
      gboolean has_row;

      has_row = dex_await_boolean (gom_cursor_next (self), &error);

      if (error != NULL)
        {
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (!has_row)
        break;

      if (!(entity = gom_cursor_materialize (self, &error)))
        {
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (entity == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to materialize cursor row");
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      g_list_store_append (result, entity);
    }

  if (!dex_await (gom_cursor_close (self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&result));
}

static DexFuture *
gom_cursor_exhaust_to_records_fiber (gpointer user_data)
{
  GomCursor *self = user_data;
  g_autoptr(GListStore) result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (GOM_IS_CURSOR (self));

  result = g_list_store_new (GOM_TYPE_RECORD);

  for (;;)
    {
      g_autoptr(GomRecord) record = NULL;
      gboolean has_row;

      has_row = dex_await_boolean (gom_cursor_next (self), &error);

      if (error != NULL)
        {
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (!has_row)
        break;

      if (!(record = gom_cursor_snapshot (self, &error)))
        {
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (record == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to snapshot cursor row");
          dex_await (gom_cursor_close (self), NULL);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      g_list_store_append (result, record);
    }

  if (!dex_await (gom_cursor_close (self), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&result));
}

static DexFuture *
gom_cursor_real_exhaust (GomCursor *self)
{
  g_assert (GOM_IS_CURSOR (self));

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_cursor_exhaust_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * gom_cursor_exhaust_to_list:
 * @self: a [class@Gom.Cursor]
 *
 * Advances the cursor until exhaustion, materializing each row into a
 * [class@Gom.Entity] and appending it to a [iface@Gio.ListModel].
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [iface@Gio.ListModel] of materialized entities.
 */
DexFuture *
gom_cursor_exhaust_to_list (GomCursor *self)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_cursor_exhaust_to_list_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

DexFuture *
_gom_cursor_exhaust_to_records (GomCursor *self)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_cursor_exhaust_to_records_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

static GomCursorCapabilities
gom_cursor_real_get_capabilities (GomCursor *self)
{
  g_assert (GOM_IS_CURSOR (self));

  return GOM_CURSOR_CAPABILITIES_NONE;
}

static void
gom_cursor_class_init (GomCursorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_cursor_finalize;

  klass->exhaust = gom_cursor_real_exhaust;
  klass->get_capabilities = gom_cursor_real_get_capabilities;
}

static void
gom_cursor_init (GomCursor *self)
{
  self->discriminator_column = -1;

  gom_trace_counter_add (GOM_TRACE_COUNTER_CURSORS, 1);
}

void
_gom_cursor_set_repository (GomCursor     *self,
                            GomRepository *repository)
{
  g_return_if_fail (GOM_IS_CURSOR (self));
  g_return_if_fail (GOM_IS_REPOSITORY (repository));

  g_set_object (&self->repository, repository);
}

GomRepository *
_gom_cursor_dup_repository (GomCursor *self)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);

  return self->repository ? g_object_ref (self->repository) : NULL;
}

void
_gom_cursor_set_session (GomCursor  *self,
                         GomSession *session)
{
  g_return_if_fail (GOM_IS_CURSOR (self));
  g_return_if_fail (session == NULL || GOM_IS_SESSION (session));

  g_set_object (&self->session, session);
}

GomSession *
_gom_cursor_dup_session (GomCursor *self)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);

  return self->session ? g_object_ref (self->session) : NULL;
}

static char *
gom_cursor_build_identity_key (GType               entity_type,
                               GomEntityClass     *entity_class,
                               const char * const *property_names,
                               const GValue       *property_values,
                               guint               n_properties)
{
  g_autoptr(GString) key = NULL;
  const char * const *identity_fields;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (entity_class), NULL);
  g_return_val_if_fail (property_names != NULL || n_properties == 0, NULL);
  g_return_val_if_fail (property_values != NULL || n_properties == 0, NULL);

  identity_fields = gom_entity_class_get_identity_fields (entity_class);
  if (identity_fields == NULL || identity_fields[0] == NULL)
    return NULL;

  key = g_string_new (g_type_name (entity_type));
  g_string_append_c (key, '\n');

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      gboolean found = FALSE;

      for (guint j = 0; j < n_properties; j++)
        {
          if (g_str_equal (property_names[j], identity_fields[i]))
            {
              g_autofree char *value_contents = _gom_value_dup_identity_key (&property_values[j]);

              if (value_contents == NULL)
                return NULL;

              g_string_append (key, identity_fields[i]);
              g_string_append_c (key, '=');
              g_string_append (key, value_contents);
              g_string_append_c (key, '\n');
              found = TRUE;
              break;
            }
        }

      if (!found)
        return NULL;
    }

  return g_string_free (g_steal_pointer (&key), FALSE);
}

static gboolean
gom_value_get_int64 (const GValue *value,
                     gint64       *out)
{
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (out != NULL, FALSE);

  if (G_VALUE_HOLDS_INT64 (value))
    {
      *out = g_value_get_int64 (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT (value))
    {
      *out = g_value_get_int (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT (value))
    {
      *out = (gint64)g_value_get_uint (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT64 (value))
    {
      *out = (gint64)g_value_get_uint64 (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      *out = g_value_get_boolean (value) ? 1 : 0;
      return TRUE;
    }

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      *out = (gint64)g_value_get_double (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_FLOAT (value))
    {
      *out = (gint64)g_value_get_float (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      const char *str = g_value_get_string (value);

      if (str != NULL)
        {
          *out = g_ascii_strtoll (str, NULL, 10);
          return TRUE;
        }
    }

  return FALSE;
}

static void
gom_entity_class_cache_discriminators (GType       type,
                                       GHashTable *cache)
{
  g_autofree GType *children = NULL;
  guint n_children = 0;

  if (g_type_is_a (type, GOM_TYPE_ENTITY))
    {
      GomEntityClass *klass = g_type_class_get (type);
      const char *value = gom_entity_class_get_discriminator_value (klass);

      if (value != NULL)
        g_hash_table_insert (cache, (gpointer)value, GSIZE_TO_POINTER (type));
    }

  children = g_type_children (type, &n_children);

  for (guint i = 0; i < n_children; i++)
    gom_entity_class_cache_discriminators (children[i], cache);
}

static GomEntityClass *
gom_cursor_lookup_discriminator (GomCursor      *self,
                                 GomEntityClass *klass,
                                 const char     *discriminator_value)
{
  gpointer value;

  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);
  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);
  g_return_val_if_fail (discriminator_value != NULL, NULL);

  if (self->discriminator_cache == NULL)
    {
      self->discriminator_cache = g_hash_table_new (g_str_hash, g_str_equal);
      gom_entity_class_cache_discriminators (G_TYPE_FROM_CLASS (klass), self->discriminator_cache);
    }

  if (!(value = g_hash_table_lookup (self->discriminator_cache, discriminator_value)))
    return NULL;

  return g_type_class_get (GPOINTER_TO_SIZE (value));
}

static gboolean
gom_value_get_uint64 (const GValue *value,
                      guint64      *out)
{
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (out != NULL, FALSE);

  if (G_VALUE_HOLDS_UINT64 (value))
    {
      *out = g_value_get_uint64 (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT (value))
    {
      *out = g_value_get_uint (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT64 (value))
    {
      *out = (guint64)g_value_get_int64 (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT (value))
    {
      *out = (guint64)g_value_get_int (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      *out = g_value_get_boolean (value) ? 1 : 0;
      return TRUE;
    }

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      *out = (guint64)g_value_get_double (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_FLOAT (value))
    {
      *out = (guint64)g_value_get_float (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      const char *str = g_value_get_string (value);

      if (str != NULL)
        {
          *out = g_ascii_strtoull (str, NULL, 10);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gom_value_get_double (const GValue *value,
                      double       *out)
{
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (out != NULL, FALSE);

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      *out = g_value_get_double (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_FLOAT (value))
    {
      *out = (double)g_value_get_float (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT64 (value))
    {
      *out = (double)g_value_get_int64 (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT64 (value))
    {
      *out = (double)g_value_get_uint64 (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT (value))
    {
      *out = (double)g_value_get_int (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT (value))
    {
      *out = (double)g_value_get_uint (value);
      return TRUE;
    }

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      *out = g_value_get_boolean (value) ? 1.0 : 0.0;
      return TRUE;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      const char *str = g_value_get_string (value);

      if (str != NULL)
        {
          *out = g_ascii_strtod (str, NULL);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gom_cursor_value_to_property (GomCursor   *self,
                              guint        column,
                              GParamSpec  *pspec,
                              GValue      *out,
                              GError     **error)
{
  g_auto(GValue) value = G_VALUE_INIT;
  GType target_type;

  g_assert (GOM_IS_CURSOR (self));
  g_assert (pspec != NULL);
  g_assert (out != NULL);

  target_type = G_PARAM_SPEC_VALUE_TYPE (pspec);

  /* Fast path for string types where we just get the string value from the
   * cursor and treat it as a "static string" (e.g. avoid additional copy).
   */
  if (target_type == G_TYPE_STRING && G_VALUE_HOLDS_STRING (&value))
    {
      g_value_set_static_string (out, gom_cursor_get_column_string (self, column));
      return TRUE;
    }

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to read cursor column %u",
                   column);
      return FALSE;
    }

  if (G_VALUE_HOLDS_POINTER (&value) && g_value_get_pointer (&value) == NULL)
    {
      g_value_init (out, target_type);
      g_param_value_set_default (pspec, out);
      return TRUE;
    }

  g_value_init (out, target_type);

  if (target_type == G_TYPE_DATE_TIME)
    {
      if (G_VALUE_HOLDS_STRING (&value))
        {
          const char *iso8601 = g_value_get_string (&value);
          GDateTime *dt = NULL;

          if (iso8601 != NULL)
            dt = g_date_time_new_from_iso8601 (iso8601, NULL);

          if (iso8601 != NULL && dt == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Failed to parse GDateTime from '%s'",
                           iso8601);
              return FALSE;
            }

          g_value_take_boxed (out, dt);
          return TRUE;
        }
    }

  if (target_type == G_TYPE_GTYPE)
    {
      const char *type_name;
      GType gtype;

      if (!G_VALUE_HOLDS_STRING (&value))
        goto transform;

      type_name = g_value_get_string (&value);
      if (type_name == NULL || *type_name == '\0')
        {
          g_value_set_gtype (out, G_TYPE_NONE);
          return TRUE;
        }

      gtype = g_type_from_name (type_name);
      if (gtype == G_TYPE_INVALID)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Failed to resolve GType from '%s'",
                       type_name);
          return FALSE;
        }

      g_value_set_gtype (out, gtype);
      return TRUE;
    }

  if (G_TYPE_IS_ENUM (target_type))
    {
      gint64 v = 0;

      if (!gom_value_get_int64 (&value, &v))
        goto transform;

      g_value_set_enum (out, (gint)v);
      return TRUE;
    }

  if (G_TYPE_IS_FLAGS (target_type))
    {
      guint64 v = 0;

      if (!gom_value_get_uint64 (&value, &v))
        goto transform;

      g_value_set_flags (out, (guint)v);
      return TRUE;
    }

  if (target_type == G_TYPE_BOOLEAN)
    {
      gint64 v = 0;

      if (!gom_value_get_int64 (&value, &v))
        goto transform;

      g_value_set_boolean (out, v != 0);
      return TRUE;
    }

  if (target_type == G_TYPE_INT)
    {
      gint64 v = 0;

      if (!gom_value_get_int64 (&value, &v))
        goto transform;

      g_value_set_int (out, (gint)v);
      return TRUE;
    }

  if (target_type == G_TYPE_INT64)
    {
      gint64 v = 0;

      if (!gom_value_get_int64 (&value, &v))
        goto transform;

      g_value_set_int64 (out, v);
      return TRUE;
    }

  if (target_type == G_TYPE_UINT)
    {
      guint64 v = 0;

      if (!gom_value_get_uint64 (&value, &v))
        goto transform;

      g_value_set_uint (out, (guint)v);
      return TRUE;
    }

  if (target_type == G_TYPE_UINT64)
    {
      guint64 v = 0;

      if (!gom_value_get_uint64 (&value, &v))
        goto transform;

      g_value_set_uint64 (out, v);
      return TRUE;
    }

  if (target_type == G_TYPE_DOUBLE)
    {
      double v = 0.0;

      if (!gom_value_get_double (&value, &v))
        goto transform;

      g_value_set_double (out, v);
      return TRUE;
    }

  if (target_type == G_TYPE_FLOAT)
    {
      double v = 0.0;

      if (!gom_value_get_double (&value, &v))
        goto transform;

      g_value_set_float (out, (float)v);
      return TRUE;
    }

  if (target_type == G_TYPE_BYTES && G_VALUE_TYPE (&value) == G_TYPE_BYTES)
    {
      g_value_set_boxed (out, g_value_get_boxed (&value));
      return TRUE;
    }

  if (target_type == G_TYPE_STRV)
    {
      const char *text;
      char **strv;

      if (!G_VALUE_HOLDS_STRING (&value))
        goto transform;

      text = g_value_get_string (&value);
      if (!(strv = _gom_strv_from_text (text, error)))
        return FALSE;

      g_value_take_boxed (out, strv);
      return TRUE;
    }

transform:
  if (g_value_type_transformable (G_VALUE_TYPE (&value), target_type) &&
      g_value_transform (&value, out))
    return TRUE;

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_DATA,
               "Failed to convert column %u to %s",
               column,
               g_type_name (target_type));
  return FALSE;
}

static gboolean
gom_cursor_bytes_to_property (GomCursor              *self,
                              guint                   column,
                              GomEntityPropertyInfo  *prop_info,
                              GValue                 *out,
                              GError                **error)
{
  g_auto(GValue) value = G_VALUE_INIT;
  GBytes *bytes = NULL;
  gboolean ok;

  g_assert (GOM_IS_CURSOR (self));
  g_assert (prop_info != NULL);
  g_assert (prop_info->from_bytes_func != NULL);
  g_assert (out != NULL);

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to read cursor column %u",
                   column);
      return FALSE;
    }

  if (G_VALUE_HOLDS_POINTER (&value) && g_value_get_pointer (&value) == NULL)
    {
      bytes = NULL;
    }
  else if (G_VALUE_TYPE (&value) == G_TYPE_BYTES)
    {
      bytes = g_value_get_boxed (&value);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Expected bytes for column %u",
                   column);
      return FALSE;
    }

  ok = prop_info->from_bytes_func (bytes, out, prop_info->bytes_user_data, error);

  if (!ok && (error == NULL || *error == NULL))
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_INVALID_DATA,
                 "Failed to deserialize bytes for column %u",
                 column);

  return ok;
}

guint
gom_cursor_get_n_columns (GomCursor *self)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), 0);

  return GOM_CURSOR_GET_CLASS (self)->get_n_columns (self);
}

const char *
gom_cursor_get_column_name (GomCursor *self,
                            guint      column)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), 0);

  return GOM_CURSOR_GET_CLASS (self)->get_column_name (self, column);
}

const char *
gom_cursor_get_column_string (GomCursor *self,
                              guint      column)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);

  return GOM_CURSOR_GET_CLASS (self)->get_column_string (self, column);
}

gint64
gom_cursor_get_column_int64 (GomCursor *self,
                             guint      column)
{
  g_auto(GValue) value = G_VALUE_INIT;
  gint64 result = 0;

  g_return_val_if_fail (GOM_IS_CURSOR (self), 0);

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    return 0;

  if (G_VALUE_HOLDS_INT64 (&value))
    result = g_value_get_int64 (&value);
  else if (G_VALUE_HOLDS_INT (&value))
    result = (gint64)g_value_get_int (&value);
  else if (G_VALUE_HOLDS_UINT (&value))
    result = (gint64)g_value_get_uint (&value);
  else if (G_VALUE_HOLDS_UINT64 (&value))
    result = (gint64)g_value_get_uint64 (&value);
  else if (G_VALUE_HOLDS_DOUBLE (&value))
    result = (gint64)g_value_get_double (&value);
  else if (G_VALUE_HOLDS_FLOAT (&value))
    result = (gint64)g_value_get_float (&value);
  else if (G_VALUE_HOLDS_BOOLEAN (&value))
    result = g_value_get_boolean (&value) ? 1 : 0;
  else if (G_VALUE_HOLDS_STRING (&value))
    {
      const char *str = g_value_get_string (&value);
      if (str != NULL)
        result = g_ascii_strtoll (str, NULL, 10);
    }

  return result;
}

gboolean
gom_cursor_get_column (GomCursor *self,
                       guint      column,
                       GValue    *value)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
    g_value_unset (value);

  return GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, value);
}

gboolean
gom_cursor_get_column_null (GomCursor *self,
                            guint      column)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_val_if_fail (GOM_IS_CURSOR (self), TRUE);

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    return TRUE;

  if (G_VALUE_HOLDS_POINTER (&value))
    return g_value_get_pointer (&value) == NULL;

  if (G_VALUE_HOLDS_STRING (&value))
    return g_value_get_string (&value) == NULL;

  return FALSE;
}

gboolean
gom_cursor_get_column_boolean (GomCursor *self,
                               guint      column)
{
  g_auto(GValue) value = G_VALUE_INIT;
  gint64 v = 0;

  g_return_val_if_fail (GOM_IS_CURSOR (self), FALSE);

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    return FALSE;

  if (G_VALUE_HOLDS_BOOLEAN (&value))
    return g_value_get_boolean (&value);

  if (gom_value_get_int64 (&value, &v))
    return v != 0;

  return FALSE;
}

double
gom_cursor_get_column_double (GomCursor *self,
                              guint      column)
{
  g_auto(GValue) value = G_VALUE_INIT;
  double v = 0.0;

  g_return_val_if_fail (GOM_IS_CURSOR (self), 0.0);

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    return 0.0;

  if (gom_value_get_double (&value, &v))
    return v;

  return 0.0;
}

gboolean
gom_cursor_get_column_by_name (GomCursor  *self,
                               const char *name,
                               GValue     *value)
{
  guint n_columns;

  g_return_val_if_fail (GOM_IS_CURSOR (self), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  n_columns = gom_cursor_get_n_columns (self);

  for (guint i = 0; i < n_columns; i++)
    {
      const char *column_name = gom_cursor_get_column_name (self, i);

      if (column_name != NULL && g_strcmp0 (column_name, name) == 0)
        return gom_cursor_get_column (self, i, value);
    }

  return FALSE;
}

/**
 * gom_cursor_dup_column_bytes:
 * @self: a [class@Gom.Cursor]
 *
 * Gets the column data as bytes.
 *
 * Returns: (transfer full) (nullable):
 */
GBytes *
gom_cursor_dup_column_bytes (GomCursor *self,
                             guint      column)
{
  g_auto(GValue) value = G_VALUE_INIT;

  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);

  if (!GOM_CURSOR_GET_CLASS (self)->get_column_value (self, column, &value))
    return NULL;

  if (G_VALUE_HOLDS_POINTER (&value) && g_value_get_pointer (&value) == NULL)
    return NULL;

  if (G_VALUE_TYPE (&value) == G_TYPE_BYTES)
    return g_value_dup_boxed (&value);

  return NULL;
}

GomCursorCapabilities
gom_cursor_get_capabilities (GomCursor *self)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), GOM_CURSOR_CAPABILITIES_NONE);

  return GOM_CURSOR_GET_CLASS (self)->get_capabilities (self);
}

/**
 * gom_cursor_next:
 * @self: a [class@Gom.Cursor]
 *
 * Advance the cursor by one row.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
gom_cursor_next (GomCursor *self)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  GOM_TRACE_MARK ("Cursor", "next", "type=%s", G_OBJECT_TYPE_NAME (self));
  return GOM_CURSOR_GET_CLASS (self)->next (self);
}

/**
 * gom_cursor_close:
 * @self: a [class@Gom.Cursor]
 *
 * Closes the underlying cursor.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
gom_cursor_close (GomCursor *self)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  GOM_TRACE_MARK ("Cursor", "close", "type=%s", G_OBJECT_TYPE_NAME (self));
  return GOM_CURSOR_GET_CLASS (self)->close (self);
}

/**
 * gom_cursor_exhaust:
 * @self: a [class@Gom.Cursor]
 *
 * Advance the cursor until it is exhausted and close it.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves once the
 *   cursor has been closed or rejects with error.
 */
DexFuture *
gom_cursor_exhaust (GomCursor *self)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  return GOM_CURSOR_GET_CLASS (self)->exhaust (self);
}

/**
 * gom_cursor_rewind:
 * @self: a [class@Gom.Cursor]
 *
 * Rewind the cursor to the beginning.
 *
 * This will reject if [method@Gom.Cursor.get_capabilities] does not support
 * rewinding the cursor.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves once the
 *   cursor has been rewound or rejects with error.
 */
DexFuture *
gom_cursor_rewind (GomCursor *self)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  if (GOM_CURSOR_GET_CLASS (self)->rewind == NULL ||
      (gom_cursor_get_capabilities (self) & GOM_CURSOR_CAPABILITIES_REWIND) == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Rewinding is not supported");

  return GOM_CURSOR_GET_CLASS (self)->rewind (self);
}

/**
 * gom_cursor_move_absolute:
 * @self: a [class@Gom.Cursor]
 * @position: the zero-based row index to move to
 *
 * Move the cursor to an absolute row position.
 *
 * This will reject if [method@Gom.Cursor.get_capabilities] does not support
 * absolute cursor movement.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a boolean
 *   indicating if the destination row exists or rejects with error.
 */
DexFuture *
gom_cursor_move_absolute (GomCursor *self,
                          guint64    position)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  if (GOM_CURSOR_GET_CLASS (self)->move_absolute == NULL ||
      (gom_cursor_get_capabilities (self) & GOM_CURSOR_CAPABILITIES_ABSOLUTE) == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Absolute movement is not supported");

  return GOM_CURSOR_GET_CLASS (self)->move_absolute (self, position);
}

/**
 * gom_cursor_move_relative:
 * @self: a [class@Gom.Cursor]
 * @offset: the relative row offset to move by
 *
 * Move the cursor by @offset rows relative to the current position.
 *
 * This will reject if [method@Gom.Cursor.get_capabilities] does not support
 * relative cursor movement.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a boolean
 *   indicating if the destination row exists or rejects with error.
 */
DexFuture *
gom_cursor_move_relative (GomCursor *self,
                          gint64     offset)
{
  dex_return_error_if_fail (GOM_IS_CURSOR (self));

  if (GOM_CURSOR_GET_CLASS (self)->move_relative == NULL ||
      (gom_cursor_get_capabilities (self) & GOM_CURSOR_CAPABILITIES_RELATIVE) == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Relative movement is not supported");

  return GOM_CURSOR_GET_CLASS (self)->move_relative (self, offset);
}

/**
 * gom_cursor_materialize:
 * @self: a [class@Gom.Cursor]
 * @error: a location for a GError
 *
 * Materializes a new [class@Gom.Entity] using the values from the cursor.
 *
 * Returns: (transfer full): a [class@Gom.Entity] or %NULL and @error is set.
 */
GomEntity *
gom_cursor_materialize (GomCursor  *self,
                        GError    **error)
{
  g_autofree const char **property_names = NULL;
  g_autofree GValue *property_values = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomSession) session = NULL;
  const GomEntitySpec *entity_spec;
  GomRepository *repository;
  GomRegistry *registry;
  GomEntityClass *entity_class;
  guint n_columns;
  guint n_properties = 0;
  GType entity_type;

  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);

  entity_type = self->entity_type;
  repository = self->repository;
  g_assert (GOM_IS_REPOSITORY (repository));
  registry = _gom_repository_get_registry (repository);
  session = _gom_cursor_dup_session (self);

  if (session != NULL && _gom_session_is_closed (session))
    g_clear_object (&session);

  if (entity_type == G_TYPE_INVALID)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Cursor does not have an entity type");
      return NULL;
    }

  if (!(entity_spec = _gom_registry_lookup_entity_by_type (registry, entity_type)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Cursor entity type `%s` is not in the registry",
                   g_type_name (entity_type));
      return NULL;
    }

  entity_class = g_type_class_get (entity_type);
  n_columns = gom_cursor_get_n_columns (self);

  if (n_columns == 0)
    return NULL;

  if (!self->discriminator_cached && entity_class != NULL)
    {
      self->discriminator_cached = TRUE;
      self->discriminator_column = -1;
      self->discriminator_field = gom_entity_class_get_discriminator_field (entity_class);

      if (self->discriminator_field != NULL)
        {
          for (guint i = 0; i < n_columns; i++)
            {
              const char *column_name = gom_cursor_get_column_name (self, i);

              if (column_name == NULL || *column_name == '\0')
                continue;

              if (g_str_equal (column_name, self->discriminator_field))
                {
                  self->discriminator_column = (gint)i;
                  break;
                }
            }
        }
    }

  if (self->discriminator_field != NULL && self->discriminator_column >= 0)
    {
      const char *discriminator_value;

      if ((discriminator_value = gom_cursor_get_column_string (self, self->discriminator_column)))
        {
          const char *klass_value;

          if (!(klass_value = gom_entity_class_get_discriminator_value (entity_class)) ||
              !g_str_equal (klass_value, discriminator_value))
            {
              GomEntityClass *match;

              if ((match = gom_cursor_lookup_discriminator (self,
                                                            entity_class,
                                                            discriminator_value)))
                {
                  entity_class = match;
                  entity_type = G_TYPE_FROM_CLASS (entity_class);
                  if (!(entity_spec = _gom_registry_lookup_entity_by_type (registry, entity_type)))
                    {
                      g_set_error (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   "Cursor entity type `%s` is not in the registry",
                                   g_type_name (entity_type));
                      return NULL;
                    }
                }
            }
        }
    }

  property_names = g_new0 (const char *, n_columns);
  property_values = g_new0 (GValue, n_columns);

  for (guint i = 0; i < n_columns; i++)
    {
      const char *column_name = gom_cursor_get_column_name (self, i);
      const GomPropertySpec *property_spec;
      const char *property_name;
      GParamSpec *pspec;
      GomEntityPropertyInfo *prop_info;

      if (column_name == NULL || *column_name == '\0')
        continue;

      if (!(property_spec = _gom_entity_spec_lookup_property_by_field ((GomEntitySpec *)entity_spec, column_name)) &&
          !(property_spec = _gom_entity_spec_lookup_property_by_name ((GomEntitySpec *)entity_spec, column_name)))
        continue;

      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property_spec))
        continue;

      if (!(property_name = gom_property_spec_get_name ((GomPropertySpec *)property_spec)))
        continue;

      if (!(pspec = _gom_property_spec_get_pspec ((GomPropertySpec *)property_spec)))
        continue;

      prop_info = _gom_entity_class_get_property (entity_class, property_name, FALSE);

      if (prop_info != NULL && prop_info->from_bytes_func != NULL)
        {
          if (!gom_cursor_bytes_to_property (self,
                                             i,
                                             prop_info,
                                             &property_values[n_properties],
                                             error))
            goto cleanup;
        }
      else
        {
          if (!gom_cursor_value_to_property (self, i, pspec, &property_values[n_properties], error))
            goto cleanup;
        }

      property_names[n_properties] = property_name;
      n_properties++;
    }

  if (entity_class->materialize == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Entity class does not support materialization");
      goto cleanup;
    }

  if (session != NULL)
    {
      g_autofree char *entity_key = gom_cursor_build_identity_key (entity_type,
                                                                   entity_class,
                                                                   property_names,
                                                                   property_values,
                                                                   n_properties);

      if (entity_key != NULL)
        {
          GomEntity *existing;

          if ((existing = _gom_session_lookup_entity (session, entity_key)))
            {
              entity = existing;
              goto cleanup;
            }
        }
    }

  entity = entity_class->materialize (entity_class,
                                      self,
                                      property_names,
                                      property_values,
                                      n_properties,
                                      error);
  if (entity != NULL)
    {
      _gom_entity_set_origin (entity, GOM_ENTITY_ORIGIN_MATERIALIZED);
      _gom_entity_capture_change_state (entity,
                                        property_names,
                                        property_values,
                                        n_properties,
                                        FALSE);

      if (repository != NULL)
        gom_entity_set_repository (entity, repository);

      if (session != NULL)
        {
          g_autofree char *entity_key = gom_cursor_build_identity_key (entity_type,
                                                                       entity_class,
                                                                       property_names,
                                                                       property_values,
                                                                       n_properties);

          if (entity_key != NULL)
            {
              GomEntity *registered;

              registered = _gom_session_register_entity (session,
                                                         entity,
                                                         g_steal_pointer (&entity_key));
              if (registered != entity)
                {
                  g_clear_object (&entity);
                  entity = registered;
                }
            }
          else
            {
              _gom_entity_set_lifecycle (entity, GOM_ENTITY_LIFECYCLE_DETACHED);
            }
        }
      else
        {
          _gom_entity_set_lifecycle (entity, GOM_ENTITY_LIFECYCLE_DETACHED);
        }
    }

cleanup:
  for (guint i = 0; i < n_properties; i++)
    g_value_unset (&property_values[i]);

  return g_steal_pointer (&entity);
}

guint64
gom_cursor_get_count (GomCursor *self)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), 0);
  g_return_val_if_fail (gom_cursor_get_capabilities (self) & GOM_CURSOR_CAPABILITIES_COUNT, 0);

  return GOM_CURSOR_GET_CLASS (self)->get_count (self);
}

/**
 * gom_cursor_snapshot:
 * @self: a [class@Gom.Cursor]
 * @error: a location for a GError
 *
 * Snapshots the current cursor values into a [class@Gom.Record].
 *
 * Returns: (transfer full): a [class@Gom.Record] if successful; otherwise
 *   `NULL` and @error is set.
 */
GomRecord *
gom_cursor_snapshot (GomCursor  *self,
                     GError    **error)
{
  g_return_val_if_fail (GOM_IS_CURSOR (self), NULL);

  return _gom_record_new (self, error);
}
