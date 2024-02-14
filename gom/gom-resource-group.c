/* gom-resource-group.c
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

#include "gom-adapter.h"
#include "gom-command.h"
#include "gom-command-builder.h"
#include "gom-cursor.h"
#include "gom-filter.h"
#include "gom-repository.h"
#include "gom-resource.h"
#include "gom-resource-priv.h"
#include "gom-resource-group.h"
#include "gom-sorting.h"

struct _GomResourceGroupPrivate
{
   GomRepository *repository;

   /* Read group */
   GMutex items_mutex;
   guint count;
   GomFilter *filter;
   GomSorting *sorting;
   GType resource_type;
   GHashTable *items;
   gchar *m2m_table;
   GType m2m_type;

   /* Write group */
   gboolean is_writable;
   GPtrArray *to_write;
};

G_DEFINE_TYPE_WITH_PRIVATE(GomResourceGroup, gom_resource_group, G_TYPE_OBJECT)

enum
{
   PROP_0,
   PROP_COUNT,
   PROP_FILTER,
   PROP_SORTING,
   PROP_M2M_TABLE,
   PROP_M2M_TYPE,
   PROP_RESOURCE_TYPE,
   PROP_REPOSITORY,
   PROP_IS_WRITABLE,
   LAST_PROP
};

typedef struct {
   GomResource *resource;
   GHashTable  *ht;
} ItemData;

static GParamSpec *gParamSpecs[LAST_PROP];

GomResourceGroup *
gom_resource_group_new (GomRepository *repository)
{
   GomResourceGroup *group;

   group = g_object_new(GOM_TYPE_RESOURCE_GROUP,
                        "repository", repository,
                        "is-writable", TRUE,
                        NULL);
   return group;
}

gboolean
gom_resource_group_append (GomResourceGroup *group,
			   GomResource      *resource)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

   if (!group->priv->to_write)
      group->priv->to_write = g_ptr_array_new_with_free_func(g_object_unref);
   gom_resource_build_save_cmd(resource, gom_repository_get_adapter(group->priv->repository));
   g_ptr_array_add (group->priv->to_write, g_object_ref(resource));

   return TRUE;
}

#define EXECUTE_OR_GOTO(adaper, sql, error, label)       \
   G_STMT_START {                                        \
      GomCommand *c = g_object_new(GOM_TYPE_COMMAND,     \
                                   "adapter", adapter,   \
                                   "sql", sql,           \
                                   NULL);                \
      if (!gom_command_execute(c, NULL, error)) {        \
         g_object_unref(c);                              \
         goto label;                                     \
      }                                                  \
      g_object_unref(c);                                 \
   } G_STMT_END

static void
gom_resource_group_write_cb (GomAdapter *adapter,
                             gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomResourceGroup *group;
   GError *error = NULL;
   GAsyncQueue *queue;
   guint i;
   gboolean got_error;
   GPtrArray *items;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   group = GOM_RESOURCE_GROUP(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));

   g_assert(GOM_IS_ADAPTER(adapter));

   items = g_object_get_data(G_OBJECT(simple), "items");
   queue = g_object_get_data(G_OBJECT(simple), "queue");

   /* do BEGIN */
   EXECUTE_OR_GOTO(adapter, "BEGIN;", &error, rollback);

   got_error = FALSE;

   for (i = 0; i < items->len; i++) {
      GomResource *item;

      item = g_ptr_array_index(items, i);
      if (got_error ||
          !gom_resource_do_save (item, adapter, &error)) {
        got_error = TRUE;
      }
   }

   if (got_error)
      goto rollback;

   EXECUTE_OR_GOTO(adapter, "COMMIT;", &error, rollback);

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);
   goto out;

rollback:
   EXECUTE_OR_GOTO(adapter, "ROLLBACK;", NULL, error);

error:
   g_assert(error);
   g_simple_async_result_take_error(simple, error);

out:
   g_object_unref(group);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
}

gboolean
gom_resource_group_write_sync (GomResourceGroup  *group,
                               GError           **error)
{
   GSimpleAsyncResult *simple;
   gboolean ret;
   GAsyncQueue *queue;
   GomAdapter *adapter;
   GPtrArray *items;
   int i;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(group->priv->is_writable, FALSE);

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(group), NULL, NULL,
                                      gom_resource_group_write_sync);
   if (!group->priv->to_write)
      return TRUE;

   g_object_set_data(G_OBJECT(simple), "queue", queue);
   items = group->priv->to_write;
   g_object_set_data_full(G_OBJECT(simple), "items", items, (GDestroyNotify)g_ptr_array_unref);
   group->priv->to_write = NULL;

   adapter = gom_repository_get_adapter(group->priv->repository);
   gom_adapter_queue_write(adapter, gom_resource_group_write_cb, simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      for (i=0; i < items->len; i++) {
         GomResource *item = g_ptr_array_index(items, i);

         gom_resource_set_post_save_properties (item);
      }
   }

   g_object_unref(simple);

   return ret;
}

void
gom_resource_group_write_async (GomResourceGroup    *group,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
   GomResourceGroupPrivate *priv;
   GSimpleAsyncResult *simple;
   GomAdapter *adapter;

   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(group->priv->is_writable);

   priv = group->priv;

   simple = g_simple_async_result_new(G_OBJECT(group), callback, user_data,
                                      gom_resource_group_write_async);
   if (!group->priv->to_write) {
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
      g_simple_async_result_complete_in_idle(simple);
      return;
   }

   g_object_set_data_full(G_OBJECT(simple), "items", group->priv->to_write, (GDestroyNotify)g_ptr_array_unref);
   group->priv->to_write = NULL;

   adapter = gom_repository_get_adapter(priv->repository);
   gom_adapter_queue_read(adapter, gom_resource_group_write_cb, simple);
}

gboolean
gom_resource_group_write_finish (GomResourceGroup  *group,
                                 GAsyncResult      *result,
                                 GError           **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GPtrArray *items;
   gboolean ret;
   int i;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);
   g_return_val_if_fail(group->priv->is_writable, FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      items = g_object_get_data(G_OBJECT(simple), "items");
      for (i=0; i < items->len; i++) {
         GomResource *item = g_ptr_array_index(items, i);

         gom_resource_set_post_save_properties (item);
      }
   }
   g_object_unref(simple);

   return ret;
}

static void
gom_resource_group_delete_cb (GomAdapter *adapter,
                              gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomResourceGroup *group;
   GError *error = NULL;
   GAsyncQueue *queue;
   guint i;
   gboolean got_error;
   GPtrArray *items;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   group = GOM_RESOURCE_GROUP(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));

   g_assert(GOM_IS_ADAPTER(adapter));

   items = g_object_get_data(G_OBJECT(simple), "items");
   queue = g_object_get_data(G_OBJECT(simple), "queue");

   /* do BEGIN */
   EXECUTE_OR_GOTO(adapter, "BEGIN;", &error, rollback);

   got_error = FALSE;

   for (i = 0; i < items->len; i++) {
      GomResource *item;

      item = g_ptr_array_index(items, i);
      if (got_error ||
          !gom_resource_do_delete (item, adapter, &error)) {
        got_error = TRUE;
      }
   }

   if (got_error)
      goto rollback;

   EXECUTE_OR_GOTO(adapter, "COMMIT;", &error, rollback);

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);
   goto out;

rollback:
   EXECUTE_OR_GOTO(adapter, "ROLLBACK;", NULL, error);

error:
   g_assert(error);
   g_simple_async_result_take_error(simple, error);

out:
   g_object_unref(group);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
}

gboolean
gom_resource_group_delete_sync (GomResourceGroup  *group,
                                GError           **error)
{
   GSimpleAsyncResult *simple;
   gboolean ret;
   GAsyncQueue *queue;
   GomAdapter *adapter;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(group->priv->is_writable, FALSE);

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(group), NULL, NULL,
                                      gom_resource_group_delete_sync);
   if (!group->priv->to_write)
      return TRUE;

   g_object_set_data(G_OBJECT(simple), "queue", queue);
   g_object_set_data_full(G_OBJECT(simple), "items", group->priv->to_write, (GDestroyNotify)g_ptr_array_unref);
   group->priv->to_write = NULL;

   adapter = gom_repository_get_adapter(group->priv->repository);
   gom_adapter_queue_write(adapter, gom_resource_group_delete_cb, simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

void
gom_resource_group_delete_async (GomResourceGroup    *group,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
   GomResourceGroupPrivate *priv;
   GSimpleAsyncResult *simple;
   GomAdapter *adapter;

   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(group->priv->is_writable);

   priv = group->priv;

   simple = g_simple_async_result_new(G_OBJECT(group), callback, user_data,
                                      gom_resource_group_delete_async);
   if (!group->priv->to_write) {
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
      g_simple_async_result_complete_in_idle(simple);
      return;
   }

   g_object_set_data_full(G_OBJECT(simple), "items", group->priv->to_write, (GDestroyNotify)g_ptr_array_unref);
   group->priv->to_write = NULL;

   adapter = gom_repository_get_adapter(priv->repository);
   gom_adapter_queue_read(adapter, gom_resource_group_delete_cb, simple);
}

gboolean
gom_resource_group_delete_finish (GomResourceGroup  *group,
                                  GAsyncResult      *result,
                                  GError           **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);
   g_return_val_if_fail(group->priv->is_writable, FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

static GomFilter *
gom_resource_group_get_filter (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);
   return group->priv->filter;
}

static void
gom_resource_group_set_filter (GomResourceGroup *group,
                               GomFilter        *filter)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(!filter || GOM_IS_FILTER(filter));

   if (filter) {
      group->priv->filter = g_object_ref(filter);
      g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_FILTER]);
   }
}

static GomSorting *
gom_resource_group_get_sorting (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);
   return group->priv->sorting;
}

static void
gom_resource_group_set_sorting (GomResourceGroup *group,
                                GomSorting       *sorting)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(!sorting || GOM_IS_SORTING(sorting));

   if (sorting) {
      group->priv->sorting = g_object_ref(sorting);
      g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_SORTING]);
   }
}

guint
gom_resource_group_get_count (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), 0);
   g_return_val_if_fail(!group->priv->is_writable, 0);
   return group->priv->count;
}

static void
gom_resource_group_set_count (GomResourceGroup *group,
                              guint             count)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));

   group->priv->count = count;
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_COUNT]);
}

const gchar *
gom_resource_group_get_m2m_table (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);
   g_return_val_if_fail(!group->priv->is_writable, NULL);
   return group->priv->m2m_table;
}

static void
gom_resource_group_set_m2m_table (GomResourceGroup *group,
                                  const gchar      *m2m_table)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));

   g_free(group->priv->m2m_table);
   group->priv->m2m_table = g_strdup(m2m_table);
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_M2M_TABLE]);
}

static GType
gom_resource_group_get_m2m_type (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), 0);
   g_return_val_if_fail(!group->priv->is_writable, 0);
   return group->priv->m2m_type;
}

static void
gom_resource_group_set_m2m_type (GomResourceGroup *group,
                                 GType             m2m_type)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   group->priv->m2m_type = m2m_type;
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_M2M_TYPE]);
}

static GomRepository *
gom_resource_group_get_repository (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);
   return group->priv->repository;
}

static void
gom_resource_group_set_repository (GomResourceGroup *group,
                                   GomRepository    *repository)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(GOM_IS_REPOSITORY(repository));

   g_clear_object(&group->priv->repository);
   group->priv->repository = g_object_ref(repository);
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_REPOSITORY]);
}

static GType
gom_resource_group_get_resource_type (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), G_TYPE_INVALID);
   return group->priv->resource_type;
}

static void
gom_resource_group_set_resource_type (GomResourceGroup *group,
                                      GType             resource_type)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));

   group->priv->resource_type = resource_type;
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_RESOURCE_TYPE]);
}

static void
value_free (gpointer data)
{
   GValue *value = data;

   if (value == NULL)
      return;
   g_value_unset(value);
   g_free(value);
}

static void
item_data_free (gpointer data)
{
   ItemData *itemdata = data;
   if (itemdata == NULL)
      return;
   g_clear_object(&itemdata->resource);
   g_clear_pointer(&itemdata->ht, g_hash_table_destroy);
   g_free (itemdata);
}

static void
foreach_prop (gpointer key,
              gpointer value,
              gpointer user_data)
{
   g_object_set_property(user_data, key, value);
}

static void
item_data_ensure_resource (ItemData      *itemdata,
                           GType          resource_type,
                           GomRepository *repository)
{
   if (itemdata->resource)
      return;

   itemdata->resource = g_object_new(resource_type,
                                     "repository", repository,
                                     NULL);
   g_hash_table_foreach(itemdata->ht, foreach_prop, itemdata->resource);
   gom_resource_set_is_from_table(itemdata->resource, TRUE);
   g_clear_pointer(&itemdata->ht, g_hash_table_destroy);
}

static ItemData *
set_props (GType         resource_type,
           GomCursor    *cursor)
{
   GObjectClass *klass;
   const gchar *name;
   GParamSpec *pspec;
   guint n_cols;
   guint i;
   GHashTable *ht;
   ItemData *itemdata;

   g_assert(GOM_IS_CURSOR(cursor));

   klass = g_type_class_ref(resource_type);
   n_cols = gom_cursor_get_n_columns(cursor);
   ht = g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free, value_free);

   for (i = 0; i < n_cols; i++) {
      name = gom_cursor_get_column_name(cursor, i);
      if ((pspec = g_object_class_find_property(klass, name))) {
         GomResourceFromBytesFunc from_bytes;

         from_bytes = g_param_spec_get_qdata(pspec, GOM_RESOURCE_FROM_BYTES_FUNC);
         if (from_bytes) {
            GValue *converted;
            GValue value = { 0, };

            converted = g_new0 (GValue, 1);
            g_value_init(&value, G_TYPE_BYTES);
            gom_cursor_get_column(cursor, i, &value);
            (*from_bytes) (g_value_get_boxed(&value), converted);
            g_value_unset(&value);
            g_hash_table_insert(ht, g_strdup(name), converted);
         } else {
            GValue *value;

            value = g_new0 (GValue, 1);
            g_value_init(value, pspec->value_type);
            gom_cursor_get_column(cursor, i, value);
            g_hash_table_insert(ht, g_strdup(name), value);
         }
      }
   }

   g_type_class_unref (klass);

   itemdata = g_new0(ItemData, 1);
   itemdata->ht = ht;

   return itemdata;
}

static gboolean
copy_into (gpointer key,
           gpointer value,
           gpointer user_data)
{
  g_hash_table_insert (user_data, key, value);
  return FALSE;
}

static void
gom_resource_group_fetch_cb (GomAdapter *adapter,
                             gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GHashTable *ht = NULL;
   GomCommandBuilder *builder = NULL;
   GomResourceGroup *group;
   GomRepository *repository = NULL;
   GomCommand *command = NULL;
   GomCursor *cursor = NULL;
   GomFilter *filter = NULL;
   GomSorting *sorting = NULL;
   GError *error = NULL;
   GType resource_type;
   gchar *m2m_table = NULL;
   GType m2m_type = 0;
   guint limit;
   guint offset;
   guint idx;
   GAsyncQueue *queue;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   group = GOM_RESOURCE_GROUP(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
   g_object_get(group,
                "filter", &filter,
                "sorting", &sorting,
                "m2m-table", &m2m_table,
                "m2m-type", &m2m_type,
                "repository", &repository,
                "resource-type", &resource_type,
                NULL);
   g_assert(GOM_IS_ADAPTER(adapter));
   g_assert(!filter || GOM_IS_FILTER(filter));
   g_assert(!sorting || GOM_IS_SORTING(sorting));
   g_assert(GOM_IS_REPOSITORY(repository));
   g_assert(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));

   limit = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple), "limit"));
   offset = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple), "offset"));
   queue = g_object_get_data(G_OBJECT(simple), "queue");

   builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                          "adapter", gom_repository_get_adapter(repository),
                          "filter", filter,
                          "sorting", sorting,
                          "limit", limit,
                          "m2m-table", m2m_table,
                          "m2m-type", m2m_type,
                          "offset", offset,
                          "resource-type", resource_type,
                          NULL);
   g_assert(GOM_IS_COMMAND_BUILDER(builder));

   command = gom_command_builder_build_select(builder);
   g_assert(GOM_IS_COMMAND(command));

   if (!gom_command_execute(command, &cursor, &error)) {
      g_simple_async_result_take_error(simple, error);
      goto out;
   }

   if (!cursor) {
      goto out;
   }

   g_mutex_lock (&group->priv->items_mutex);
   if (!group->priv->items) {
      group->priv->items = g_hash_table_new_full(NULL, NULL, NULL, item_data_free);
   }
   g_mutex_unlock (&group->priv->items_mutex);

   idx = offset;
   ht = g_hash_table_new(NULL, NULL);

   while (gom_cursor_next(cursor)) {
      ItemData *itemdata = set_props(resource_type, cursor);
      g_hash_table_insert(ht, GUINT_TO_POINTER(idx), itemdata);
      idx++;
   }

   g_mutex_lock (&group->priv->items_mutex);
   g_hash_table_foreach (ht, copy_into, group->priv->items);
   g_mutex_unlock (&group->priv->items_mutex);

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);

out:
   g_object_unref(group);
   g_clear_object(&builder);
   g_clear_object(&command);
   g_clear_object(&cursor);
   g_clear_object(&filter);
   g_clear_object(&sorting);
   g_clear_object(&repository);
   g_clear_pointer(&ht, g_hash_table_unref);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
   g_free(m2m_table);
}

/**
 * gom_resource_group_fetch_sync:
 * @group: (in): A #GomResourceGroup.
 * @index_: (in): The first index to fetch.
 * @count: (in): The number of indexes to fetch.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Fetches a sequence of resources from the group synchronously. This must
 * be called from an adapter read callback using gom_adapter_queue_read().
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 */
gboolean
gom_resource_group_fetch_sync (GomResourceGroup  *group,
                               guint              index_,
                               guint              count,
                               GError           **error)
{
   GSimpleAsyncResult *simple;
   gboolean ret;
   GAsyncQueue *queue;
   GomAdapter *adapter;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(!group->priv->is_writable, FALSE);

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(group), NULL, NULL,
                                      gom_resource_group_fetch_sync);
   g_object_set_data(G_OBJECT(simple), "offset", GINT_TO_POINTER(index_));
   g_object_set_data(G_OBJECT(simple), "limit", GINT_TO_POINTER(count));
   g_object_set_data(G_OBJECT(simple), "queue", queue);

   adapter = gom_repository_get_adapter(group->priv->repository);
   gom_adapter_queue_read(adapter, gom_resource_group_fetch_cb, simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

void
gom_resource_group_fetch_async (GomResourceGroup    *group,
                                guint                index_,
                                guint                count,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
   GomResourceGroupPrivate *priv;
   GSimpleAsyncResult *simple;
   GomAdapter *adapter;

   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(!group->priv->is_writable);

   priv = group->priv;

   simple = g_simple_async_result_new(G_OBJECT(group), callback, user_data,
                                      gom_resource_group_fetch_async);
   g_object_set_data(G_OBJECT(simple), "offset", GINT_TO_POINTER(index_));
   g_object_set_data(G_OBJECT(simple), "limit", GINT_TO_POINTER(count));

   adapter = gom_repository_get_adapter(priv->repository);
   gom_adapter_queue_read(adapter, gom_resource_group_fetch_cb, simple);
}

gboolean
gom_resource_group_fetch_finish (GomResourceGroup  *group,
                                 GAsyncResult      *result,
                                 GError           **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);
   g_return_val_if_fail(!group->priv->is_writable, FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

/**
 * gom_resource_group_get_index:
 * @group: (in): A #GomResourceGroup.
 * @index_: (in): The index of the resource.
 *
 * Fetches the resource at @index_. You must have loaded that resource by
 * calling gom_resource_group_fetch_async() with a range inclusive of the
 * index.
 *
 * Returns: (transfer none): A #GomResource.
 */
GomResource *
gom_resource_group_get_index (GomResourceGroup *group,
                              guint             index_)
{
   GomResourceGroupPrivate *priv;
   GomResource *ret = NULL;

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);
   g_return_val_if_fail(!group->priv->is_writable, NULL);

   priv = group->priv;

   g_mutex_lock (&priv->items_mutex);

   if (priv->items) {
      ItemData *itemdata;

      itemdata = g_hash_table_lookup(priv->items, GUINT_TO_POINTER (index_));
      if (itemdata == NULL) {
         g_critical ("Index %u is not found in GomResourceGroup %p of size %u. "
                     "This is an error in your program. "
                     "Make sure you've called gom_resource_group_fetch_async() or "
                     "gom_resource_group_fetch_sync() first.",
                     index_, group, group->priv->count);
         goto unlock;
      }
      item_data_ensure_resource(itemdata, priv->resource_type, priv->repository);
      ret = itemdata->resource;
   }

unlock:
   g_mutex_unlock (&priv->items_mutex);

   return ret;
}

/**
 * gom_resource_group_finalize:
 * @object: (in): A #GomResourceGroup.
 *
 * Finalizer for a #GomResourceGroup instance.  Frees any resources held by
 * the instance.
 */
static void
gom_resource_group_finalize (GObject *object)
{
   GomResourceGroupPrivate *priv = GOM_RESOURCE_GROUP(object)->priv;

   g_clear_object(&priv->repository);
   g_clear_object(&priv->filter);
   g_clear_object(&priv->sorting);
   g_clear_pointer(&priv->items, g_hash_table_unref);
   g_clear_pointer(&priv->to_write, g_ptr_array_unref);
   g_mutex_clear (&priv->items_mutex);

   G_OBJECT_CLASS(gom_resource_group_parent_class)->finalize(object);
}

/**
 * gom_resource_group_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_resource_group_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
   GomResourceGroup *group = GOM_RESOURCE_GROUP(object);

   switch (prop_id) {
   case PROP_COUNT:
      g_value_set_uint(value, gom_resource_group_get_count(group));
      break;
   case PROP_FILTER:
      g_value_set_object(value, gom_resource_group_get_filter(group));
      break;
   case PROP_SORTING:
      g_value_set_object(value, gom_resource_group_get_sorting(group));
      break;
   case PROP_M2M_TABLE:
      g_value_set_string(value, gom_resource_group_get_m2m_table(group));
      break;
   case PROP_M2M_TYPE:
      g_value_set_gtype(value, gom_resource_group_get_m2m_type(group));
      break;
   case PROP_REPOSITORY:
      g_value_set_object(value, gom_resource_group_get_repository(group));
      break;
   case PROP_RESOURCE_TYPE:
      g_value_set_gtype(value, gom_resource_group_get_resource_type(group));
      break;
   case PROP_IS_WRITABLE:
      g_value_set_boolean(value, group->priv->is_writable);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_resource_group_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_resource_group_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
   GomResourceGroup *group = GOM_RESOURCE_GROUP(object);

   switch (prop_id) {
   case PROP_COUNT:
      gom_resource_group_set_count(group, g_value_get_uint(value));
      break;
   case PROP_FILTER:
      gom_resource_group_set_filter(group, g_value_get_object(value));
      break;
   case PROP_SORTING:
      gom_resource_group_set_sorting(group, g_value_get_object(value));
      break;
   case PROP_M2M_TABLE:
      gom_resource_group_set_m2m_table(group, g_value_get_string(value));
      break;
   case PROP_M2M_TYPE:
      gom_resource_group_set_m2m_type(group, g_value_get_gtype(value));
      break;
   case PROP_RESOURCE_TYPE:
      gom_resource_group_set_resource_type(group, g_value_get_gtype(value));
      break;
   case PROP_REPOSITORY:
      gom_resource_group_set_repository(group, g_value_get_object(value));
      break;
   case PROP_IS_WRITABLE:
      group->priv->is_writable = g_value_get_boolean(value);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_resource_group_class_init:
 * @klass: (in): A #GomResourceGroupClass.
 *
 * Initializes the #GomResourceGroupClass and prepares the vtable.
 */
static void
gom_resource_group_class_init (GomResourceGroupClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_resource_group_finalize;
   object_class->get_property = gom_resource_group_get_property;
   object_class->set_property = gom_resource_group_set_property;

   gParamSpecs[PROP_COUNT] =
      g_param_spec_uint("count",
                        "Count",
                        "The size of the resource group.",
                        0,
                        G_MAXUINT,
                        0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_COUNT,
                                   gParamSpecs[PROP_COUNT]);

   gParamSpecs[PROP_FILTER] =
      g_param_spec_object("filter",
                          "Filter",
                          "The query filter.",
                          GOM_TYPE_FILTER,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_FILTER,
                                   gParamSpecs[PROP_FILTER]);

   gParamSpecs[PROP_SORTING] =
      g_param_spec_object("sorting",
                          "Sorting",
                          "The query sorting.",
                          GOM_TYPE_SORTING,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_SORTING,
                                   gParamSpecs[PROP_SORTING]);

   gParamSpecs[PROP_M2M_TABLE] =
      g_param_spec_string("m2m-table",
                          "Many-to-Many Table",
                          "The table used to join a Many to Many query.",
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_M2M_TABLE,
                                   gParamSpecs[PROP_M2M_TABLE]);

   gParamSpecs[PROP_M2M_TYPE] =
      g_param_spec_gtype("m2m-type",
                          "Many-to-Many type",
                          "The type used in the m2m-table join.",
                          GOM_TYPE_RESOURCE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_M2M_TYPE,
                                   gParamSpecs[PROP_M2M_TYPE]);

   gParamSpecs[PROP_REPOSITORY] =
      g_param_spec_object("repository",
                          "Repository",
                          "The repository for object storage.",
                          GOM_TYPE_REPOSITORY,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_REPOSITORY,
                                   gParamSpecs[PROP_REPOSITORY]);

   gParamSpecs[PROP_RESOURCE_TYPE] =
      g_param_spec_gtype("resource-type",
                         "Resource Type",
                         "The type of resources contained.",
                         GOM_TYPE_RESOURCE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_RESOURCE_TYPE,
                                   gParamSpecs[PROP_RESOURCE_TYPE]);

   gParamSpecs[PROP_IS_WRITABLE] =
      g_param_spec_boolean("is-writable",
                           "Is Writable",
                           "Whether the group contains resources to be written.",
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_IS_WRITABLE,
                                   gParamSpecs[PROP_IS_WRITABLE]);
}

/**
 * gom_resource_group_init:
 * @group: (in): A #GomResourceGroup.
 *
 * Initializes the newly created #GomResourceGroup instance.
 */
static void
gom_resource_group_init (GomResourceGroup *group)
{
   group->priv = gom_resource_group_get_instance_private(group);
   g_mutex_init (&group->priv->items_mutex);
}
