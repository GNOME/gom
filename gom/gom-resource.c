/* gom-resource.c
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

#include "gom-command.h"
#include "gom-command-builder.h"
#include "gom-error.h"
#include "gom-filter.h"
#include "gom-repository.h"
#include "gom-resource.h"

G_DEFINE_ABSTRACT_TYPE(GomResource, gom_resource, G_TYPE_OBJECT)

struct _GomResourcePrivate
{
   GomRepository *repository;
};

enum
{
   PROP_0,
   PROP_REPOSITORY,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];
static GHashTable *gPropMaps;

void
gom_resource_class_set_primary_key (GomResourceClass *resource_class,
                                    const gchar      *primary_key)
{
   GParamSpec *pspec;
   const GValue *value;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(primary_key != NULL);
   g_return_if_fail(strlen(primary_key) <= sizeof(resource_class->primary_key));

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), primary_key);
   if (!pspec) {
      g_warning("Property for primary key '%s' isn't declared yet. Are you running gom_resource_class_set_primary_key() too early?",
                primary_key);
      return;
   }

   if (pspec->flags & G_PARAM_CONSTRUCT_ONLY) {
      g_warning("Property for primary key '%s' is declared as construct-only. This will not work as expected.", primary_key);
      return;
   }

   /* Same check as in has_primary_key() */
   value = g_param_spec_get_default_value (pspec);
   if (value->data[0].v_pointer) {
      g_warning("Property for primary key '%s' has a non-NULL/non-zero default value. This will not work as expected.", primary_key);
      return;
   }

   g_snprintf(resource_class->primary_key,
              sizeof(resource_class->primary_key),
              "%s", primary_key);
}

void
gom_resource_class_set_property_new_in_version (GomResourceClass *resource_class,
                                                const gchar      *property_name,
                                                guint             version)
{
   GParamSpec *pspec;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(property_name != NULL);
   g_return_if_fail(version >= 1);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), property_name);
   g_assert(pspec);

   /* See is_new_in_version() in gom-repository.c for the reasoning
    * behind the "- 1" */
   g_param_spec_set_qdata(pspec, GOM_RESOURCE_NEW_IN_VERSION, GINT_TO_POINTER(version - 1));
}

void
gom_resource_class_set_property_set_mapped (GomResourceClass *resource_class,
                                            const gchar      *property_name,
                                            gboolean          is_mapped)
{
   GParamSpec *pspec;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(property_name != NULL);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), property_name);
   g_assert(pspec);

   g_param_spec_set_qdata(pspec, GOM_RESOURCE_NOT_MAPPED, GINT_TO_POINTER(!is_mapped));
}

/**
 * gom_resource_class_set_property_transform: (skip)
 */
void
gom_resource_class_set_property_transform (GomResourceClass         *resource_class,
                                           const gchar              *property_name,
                                           GomResourceToBytesFunc    to_bytes_func,
                                           GomResourceFromBytesFunc  from_bytes_func)
{
   GParamSpec *pspec;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(property_name != NULL);
   g_return_if_fail(to_bytes_func != NULL);
   g_return_if_fail(from_bytes_func != NULL);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), property_name);
   g_assert(pspec);

   g_param_spec_set_qdata(pspec, GOM_RESOURCE_TO_BYTES_FUNC, to_bytes_func);
   g_param_spec_set_qdata(pspec, GOM_RESOURCE_FROM_BYTES_FUNC, from_bytes_func);
}

void
gom_resource_class_set_property_to_bytes (GomResourceClass         *resource_class,
                                          const gchar              *property_name,
                                          GomResourceToBytesFunc    to_bytes_func,
                                          GDestroyNotify            notify)
{
   GParamSpec *pspec;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(property_name != NULL);
   g_return_if_fail(to_bytes_func != NULL);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), property_name);
   g_assert(pspec);

   g_param_spec_set_qdata(pspec, GOM_RESOURCE_TO_BYTES_FUNC, to_bytes_func);
}

void
gom_resource_class_set_property_from_bytes (GomResourceClass         *resource_class,
                                            const gchar              *property_name,
                                            GomResourceFromBytesFunc  from_bytes_func,
                                            GDestroyNotify            notify)
{
   GParamSpec *pspec;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(property_name != NULL);
   g_return_if_fail(from_bytes_func != NULL);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), property_name);
   g_assert(pspec);

   g_param_spec_set_qdata(pspec, GOM_RESOURCE_FROM_BYTES_FUNC, from_bytes_func);
}

void
gom_resource_class_set_reference (GomResourceClass     *resource_class,
                                  const gchar          *property_name,
                                  const gchar          *ref_table_name,
                                  const gchar          *ref_property_name)
{
   GParamSpec *pspec;

   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(property_name != NULL);
   g_return_if_fail(ref_property_name != NULL);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(resource_class), property_name);
   g_assert(pspec);

   if (ref_table_name == NULL)
     ref_table_name = G_OBJECT_CLASS_NAME(resource_class);

   g_param_spec_set_qdata_full(pspec, GOM_RESOURCE_REF_TABLE_CLASS,
                               g_strdup(ref_table_name), g_free);
   g_param_spec_set_qdata_full(pspec, GOM_RESOURCE_REF_PROPERTY_NAME,
                               g_strdup(ref_property_name), g_free);
}

void
gom_resource_class_set_table (GomResourceClass *resource_class,
                              const gchar      *table)
{
   g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
   g_return_if_fail(table != NULL);
   g_return_if_fail(strlen(table) <= sizeof(resource_class->table));

   g_snprintf(resource_class->table,
              sizeof(resource_class->table),
              "%s", table);
}

GomRepository *
gom_resource_get_repository (GomResource *resource)
{
   g_return_val_if_fail(GOM_IS_RESOURCE(resource), NULL);
   return resource->priv->repository;
}

static void
gom_resource_set_repository (GomResource   *resource,
                             GomRepository *repository)
{
   GomResourcePrivate *priv;
   GomRepository *old;

   g_return_if_fail(GOM_IS_RESOURCE(resource));
   g_return_if_fail(!repository || GOM_IS_REPOSITORY(repository));

   priv = resource->priv;

   if ((old = priv->repository)) {
      priv->repository = NULL;
      g_object_remove_weak_pointer(G_OBJECT(old),
                                   (gpointer  *)&priv->repository);
   }

   if (repository) {
      priv->repository = repository;
      g_object_add_weak_pointer(G_OBJECT(priv->repository),
                                (gpointer  *)&priv->repository);
      g_object_notify_by_pspec(G_OBJECT(resource),
                               gParamSpecs[PROP_REPOSITORY]);
   }
}

static gboolean
gom_resource_do_delete (GomResource  *resource,
                        GomAdapter   *adapter,
                        GError      **error)
{
   GomCommandBuilder *builder;
   GType resource_type;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);
   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);

   resource_type = G_TYPE_FROM_INSTANCE(resource);
   builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                          "adapter", adapter,
                          NULL);

   do {
      GomResourceClass *klass;
      GParamSpec *pspec;
      GomCommand *command;
      GomFilter *filter;
      GArray *values;
      GValue value = { 0 };
      gchar *sql;

      klass = g_type_class_peek(resource_type);
      g_assert(GOM_IS_RESOURCE_CLASS(klass));

      pspec = g_object_class_find_property(G_OBJECT_CLASS(klass),
                                           klass->primary_key);
      g_assert(pspec);

      g_value_init(&value, pspec->value_type);
      g_object_get_property(G_OBJECT(resource), klass->primary_key, &value);
      sql = g_strdup_printf("'%s'.'%s' = ?", klass->table, klass->primary_key);
      values = g_array_sized_new(FALSE, FALSE, sizeof(GValue), 1);
      g_array_append_val(values, value);
      filter = gom_filter_new_sql(sql, values);
      g_free(sql);
      memset(&value, 0, sizeof value);
      g_array_unref(values);
      g_object_set(builder,
                   "filter", filter,
                   "resource-type", resource_type,
                   NULL);
      g_object_unref(filter);

      command = gom_command_builder_build_delete(builder);
      if (!gom_command_execute(command, NULL, error)) {
         g_object_unref(command);
         g_object_unref(builder);
         return FALSE;
      }
      g_object_unref(command);
   } while ((resource_type = g_type_parent(resource_type)) != GOM_TYPE_RESOURCE);

   g_object_unref(builder);

   return TRUE;
}

static void
gom_resource_delete_cb (GomAdapter *adapter,
                        gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomResource *resource;
   gboolean ret;
   GError *error = NULL;
   GAsyncQueue *queue;

   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));
   resource = GOM_RESOURCE(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
   g_return_if_fail(GOM_IS_RESOURCE(resource));

   queue = g_object_get_data(G_OBJECT(simple), "queue");

   if (!(ret = gom_resource_do_delete(resource, adapter, &error))) {
      g_simple_async_result_take_error(simple, error);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
   g_object_unref(resource);
}

/**
 * gom_resource_delete_sync:
 * @resource: (in): A #GomResource.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Synchronously deletes a resource. This may only be called from inside a
 * callback to gom_adapter_queue_write().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
gom_resource_delete_sync (GomResource  *resource,
                          GError      **error)
{
   GomResourcePrivate *priv;
   GomAdapter *adapter;
   GAsyncQueue *queue;
   GSimpleAsyncResult *simple;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

   priv = resource->priv;

   if (!priv->repository) {
      g_warning("Cannot save resource, no repository set!");
      return FALSE;
   }

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(resource), NULL, NULL,
                                      gom_resource_delete_sync);
   adapter = gom_repository_get_adapter(priv->repository);
   g_object_set_data(G_OBJECT(simple), "queue", queue);
   g_assert(GOM_IS_ADAPTER(adapter));

   gom_adapter_queue_write(adapter, gom_resource_delete_cb, simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

void
gom_resource_delete_async (GomResource         *resource,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
   GomResourcePrivate *priv;
   GSimpleAsyncResult *simple;
   GomAdapter *adapter;

   g_return_if_fail(GOM_IS_RESOURCE(resource));

   priv = resource->priv;

   if (!priv->repository) {
      g_warning("Cannot delete resource, no repository set!");
      return;
   }

   simple = g_simple_async_result_new(G_OBJECT(resource), callback, user_data,
                                      gom_resource_delete_async);
   adapter = gom_repository_get_adapter(priv->repository);
   g_assert(GOM_IS_ADAPTER(adapter));
   gom_adapter_queue_write(adapter, gom_resource_delete_cb, simple);
}

gboolean
gom_resource_delete_finish (GomResource   *resource,
                            GAsyncResult  *result,
                            GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

static gboolean
is_dynamic_pkey (GType type)
{
   GomResourceClass *klass;
   GParamSpec *pspec;
   gboolean ret = FALSE;

   g_assert(type);
   g_assert(g_type_is_a(type, GOM_TYPE_RESOURCE));

   klass = g_type_class_ref(type);
   g_assert(GOM_IS_RESOURCE_CLASS(klass));

   pspec = g_object_class_find_property(G_OBJECT_CLASS(klass), klass->primary_key);
   g_assert(pspec);

   switch (pspec->value_type) {
   case G_TYPE_INT:
   case G_TYPE_INT64:
   case G_TYPE_UINT:
   case G_TYPE_UINT64:
      ret = TRUE;
      break;
   default:
      break;
   }

   g_type_class_unref(klass);

   return ret;
}

static void
set_pkey (GomResource *resource,
          GValue      *value)
{
   GParamSpec *pspec;
   GValue dst_value = { 0 };

   pspec = g_object_class_find_property(G_OBJECT_GET_CLASS(resource),
                                        GOM_RESOURCE_GET_CLASS(resource)->primary_key);
   g_assert(pspec);
   g_value_init(&dst_value, pspec->value_type);
   g_value_transform(value, &dst_value);
   g_object_set_property(G_OBJECT(resource), pspec->name, &dst_value);
   g_value_unset(&dst_value);
}

static gboolean
has_primary_key (GomResource *resource)
{
   GomResourceClass *klass;
   GParamSpec *pspec;
   gboolean ret;
   GValue value = { 0 };

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

   klass = GOM_RESOURCE_GET_CLASS(resource);

   pspec = g_object_class_find_property(G_OBJECT_CLASS(klass),
                                        klass->primary_key);
   g_assert(pspec);

   g_value_init(&value, pspec->value_type);
   g_object_get_property(G_OBJECT(resource), klass->primary_key, &value);
   ret = !value.data[0].v_pointer;
   g_value_unset(&value);

   return ret;
}

static gboolean
gom_resource_do_save (GomResource  *resource,
                      GomAdapter   *adapter,
                      GError      **error)
{
   GomCommandBuilder *builder;
   gboolean ret = FALSE;
   gboolean is_insert;
   gint64 row_id = -1;
   GSList *types = NULL;
   GSList *iter;
   GType resource_type;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);
   g_return_val_if_fail(GOM_IS_ADAPTER(adapter), FALSE);

   resource_type = G_TYPE_FROM_INSTANCE(resource);
   g_assert(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));

   builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                          "adapter", adapter,
                          NULL);

   is_insert = has_primary_key(resource);

   do {
      types = g_slist_prepend(types, GINT_TO_POINTER(resource_type));
   } while ((resource_type = g_type_parent(resource_type)) != GOM_TYPE_RESOURCE);

   for (iter = types; iter; iter = iter->next) {
      GomCommand *command;

      resource_type = GPOINTER_TO_INT(iter->data);

      g_object_set(builder,
                   "resource-type", resource_type,
                   NULL);

      if (is_insert) {
         command = gom_command_builder_build_insert(builder, resource);
      } else {
         command = gom_command_builder_build_update(builder, resource);
      }

      if (!gom_command_execute(command, NULL, error)) {
         g_object_unref(command);
         goto out;
      }

      if (is_insert && row_id == -1 && is_dynamic_pkey(resource_type)) {
         sqlite3 *handle = gom_adapter_get_handle(adapter);
         GValue value = { 0 };

         row_id = sqlite3_last_insert_rowid(handle);
         g_value_init(&value, G_TYPE_INT64);
         g_value_set_int64(&value, row_id);
         set_pkey(resource, &value);
         g_value_unset(&value);
      }

      g_object_unref(command);
   }

   ret = TRUE;

out:
   g_slist_free(types);
   g_object_unref(builder);

   return ret;
}

static void
gom_resource_save_cb (GomAdapter *adapter,
                      gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomResource *resource;
   gboolean ret;
   GError *error = NULL;
   GAsyncQueue *queue;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   resource = GOM_RESOURCE(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
   g_assert(GOM_IS_RESOURCE(resource));

   queue = g_object_get_data(G_OBJECT(simple), "queue");

   if (!(ret = gom_resource_do_save(resource, adapter, &error))) {
      g_simple_async_result_take_error(simple, error);
   }

   g_simple_async_result_set_op_res_gboolean(simple, ret);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
   g_object_unref(resource);
}

/**
 * gom_resource_save_sync:
 * @resource: (in): A #GomResource.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 */
gboolean
gom_resource_save_sync (GomResource  *resource,
                        GError      **error)
{
   GomResourcePrivate *priv;
   GomAdapter *adapter;
   GAsyncQueue *queue;
   GSimpleAsyncResult *simple;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

   priv = resource->priv;

   if (!priv->repository) {
      g_set_error(error, GOM_ERROR, GOM_ERROR_COMMAND_NO_REPOSITORY,
                  _("Cannot save resource, no repository set"));
      return FALSE;
   }

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(resource), NULL, NULL,
                                      gom_resource_save_sync);
   adapter = gom_repository_get_adapter(priv->repository);
   g_object_set_data(G_OBJECT(simple), "queue", queue);
   g_assert(GOM_IS_ADAPTER(adapter));

   gom_adapter_queue_write(adapter, gom_resource_save_cb, simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

void
gom_resource_save_async (GomResource         *resource,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
   GomResourcePrivate *priv;
   GSimpleAsyncResult *simple;
   GomAdapter *adapter;

   g_return_if_fail(GOM_IS_RESOURCE(resource));
   g_return_if_fail(callback != NULL);

   priv = resource->priv;

   if (!priv->repository) {
      g_warning("Cannot save resource, no repository set!");
      return;
   }

   simple = g_simple_async_result_new(G_OBJECT(resource), callback, user_data,
                                      gom_resource_save_async);
   adapter = gom_repository_get_adapter(priv->repository);
   g_assert(GOM_IS_ADAPTER(adapter));
   gom_adapter_queue_write(adapter, gom_resource_save_cb, simple);
}

gboolean
gom_resource_save_finish (GomResource   *resource,
                          GAsyncResult  *result,
                          GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

static void
gom_resource_fetch_m2m_cb (GomAdapter *adapter,
                           gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomCommandBuilder *builder = NULL;
   GomResourceGroup *group;
   GomRepository *repository;
   const gchar *m2m_table;
   GomResource *resource;
   GomCommand *command = NULL;
   GomCursor *cursor = NULL;
   GomFilter *filter = NULL;
   GError *error = NULL;
   guint count = 0;
   GType resource_type;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   m2m_table = g_object_get_data(G_OBJECT(simple), "m2m-table");
   resource_type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple),
                                                     "resource-type"));
   filter = g_object_get_data(G_OBJECT(simple), "filter");
   resource = GOM_RESOURCE(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
   repository = gom_resource_get_repository(resource);

   g_assert(GOM_IS_RESOURCE(resource));
   g_assert(m2m_table);
   g_assert(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
   g_assert(!filter || GOM_IS_FILTER(filter));
   g_assert(GOM_IS_REPOSITORY(repository));

   builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                          "adapter", adapter,
                          "filter", filter,
                          "resource-type", resource_type,
                          "m2m-table", m2m_table,
                          "m2m-type", G_TYPE_FROM_INSTANCE(resource),
                          NULL);

   command = gom_command_builder_build_count(builder);

   if (!gom_command_execute(command, &cursor, &error)) {
      g_simple_async_result_take_error(simple, error);
      goto out;
   }

   if (!gom_cursor_next(cursor)) {
      g_simple_async_result_set_error(simple, GOM_ERROR,
                                      GOM_ERROR_RESOURCE_CURSOR,
                                      _("No result was returned from the cursor."));
      goto out;
   }

   count = gom_cursor_get_column_int64(cursor, 0);
   group = g_object_new(GOM_TYPE_RESOURCE_GROUP,
                        "adapter", adapter,
                        "count", count,
                        "filter", filter,
                        "m2m-table", m2m_table,
                        "m2m-type", G_TYPE_FROM_INSTANCE(resource),
                        "repository", repository,
                        "resource-type", resource_type,
                        NULL);

   g_simple_async_result_set_op_res_gpointer(simple, group, g_object_unref);

out:
   g_object_unref(resource);
   g_clear_object(&command);
   g_clear_object(&cursor);
   g_clear_object(&builder);

   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
}

void
gom_resource_fetch_m2m_async (GomResource          *resource,
                              GType                 resource_type,
                              const gchar          *m2m_table,
                              GomFilter            *filter,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data)
{
   GSimpleAsyncResult *simple;
   GomRepository *repository;
   GomAdapter *adapter;

   g_return_if_fail(GOM_IS_RESOURCE(resource));
   g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
   g_return_if_fail(m2m_table != NULL);
   g_return_if_fail(callback != NULL);

   repository = gom_resource_get_repository(resource);
   g_assert(GOM_IS_REPOSITORY(repository));

   adapter = gom_repository_get_adapter(repository);
   g_assert(GOM_IS_ADAPTER(adapter));

   simple = g_simple_async_result_new(G_OBJECT(resource), callback, user_data,
                                      gom_resource_fetch_m2m_async);
   g_object_set_data(G_OBJECT(simple), "resource-type",
                     GINT_TO_POINTER(resource_type));
   g_object_set_data_full(G_OBJECT(simple), "m2m-table",
                          g_strdup(m2m_table), g_free);
   if (filter) {
      g_object_set_data_full(G_OBJECT(simple), "filter",
                             g_object_ref(filter), g_object_unref);
   }

   gom_adapter_queue_read(adapter,
                          gom_resource_fetch_m2m_cb,
                          simple);
}

/**
 * gom_resource_fetch_m2m_finish:
 * @resource: (in): A #GomResource.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes the asynchronous request to fetch a group of resources that
 * are related to the resource through a many-to-many table.
 *
 * Returns: (transfer full): A #GomResourceGroup.
 */
GomResourceGroup *
gom_resource_fetch_m2m_finish (GomResource   *resource,
                               GAsyncResult  *result,
                               GError       **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GomResourceGroup *group;

   g_return_val_if_fail(GOM_IS_RESOURCE(resource), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(result), NULL);

   if (!(group = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return group ? g_object_ref(group) : NULL;
}

/**
 * gom_resource_finalize:
 * @object: (in): A #GomResource.
 *
 * Finalizer for a #GomResource instance.  Frees any resources held by
 * the instance.
 */
static void
gom_resource_finalize (GObject *object)
{
   gom_resource_set_repository(GOM_RESOURCE(object), NULL);
   G_OBJECT_CLASS(gom_resource_parent_class)->finalize(object);
}

/**
 * gom_resource_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_resource_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
   GomResource *resource = GOM_RESOURCE(object);

   switch (prop_id) {
   case PROP_REPOSITORY:
      g_value_set_object(value, gom_resource_get_repository(resource));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_resource_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_resource_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
   GomResource *resource = GOM_RESOURCE(object);

   switch (prop_id) {
   case PROP_REPOSITORY:
      gom_resource_set_repository(resource, g_value_get_object(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_resource_class_init:
 * @klass: (in): A #GomResourceClass.
 *
 * Initializes the #GomResourceClass and prepares the vtable.
 */
static void
gom_resource_class_init (GomResourceClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_resource_finalize;
   object_class->get_property = gom_resource_get_property;
   object_class->set_property = gom_resource_set_property;
   g_type_class_add_private(object_class, sizeof(GomResourcePrivate));

   gPropMaps = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                     (GDestroyNotify)g_hash_table_destroy);

   gParamSpecs[PROP_REPOSITORY] =
      g_param_spec_object("repository",
                          _("Repository"),
                          _("The resources repository."),
                          GOM_TYPE_REPOSITORY,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_REPOSITORY,
                                   gParamSpecs[PROP_REPOSITORY]);
}

/**
 * gom_resource_init:
 * @resource: (in): A #GomResource.
 *
 * Initializes the newly created #GomResource instance.
 */
static void
gom_resource_init (GomResource *resource)
{
   resource->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                  GOM_TYPE_RESOURCE,
                                  GomResourcePrivate);
}

GQuark
gom_resource_new_in_version_quark (void)
{
   return g_quark_from_static_string("gom_resource_new_in_version_quark");
}

GQuark
gom_resource_not_mapped_quark (void)
{
   return g_quark_from_static_string("gom_resource_not_mapped_quark");
}

GQuark
gom_resource_to_bytes_func_quark (void)
{
   return g_quark_from_static_string("gom_resource_to_bytes_func_quark");
}

GQuark
gom_resource_from_bytes_func_quark (void)
{
   return g_quark_from_static_string("gom_resource_from_bytes_func_quark");
}

GQuark
gom_resource_ref_table_class (void)
{
   return g_quark_from_static_string("gom_resource_ref_table_class");
}

GQuark
gom_resource_ref_property_name (void)
{
   return g_quark_from_static_string("gom_resource_ref_property_name");
}
