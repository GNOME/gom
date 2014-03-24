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

#include <glib/gi18n.h>

#include "gom-adapter.h"
#include "gom-command.h"
#include "gom-command-builder.h"
#include "gom-cursor.h"
#include "gom-filter.h"
#include "gom-repository.h"
#include "gom-resource.h"
#include "gom-resource-group.h"

G_DEFINE_TYPE(GomResourceGroup, gom_resource_group, G_TYPE_OBJECT)

struct _GomResourceGroupPrivate
{
   GomRepository *repository;
   GomAdapter *adapter;
   guint count;
   GomFilter *filter;
   GType resource_type;
   GHashTable *items;
   gchar *m2m_table;
   GType m2m_type;
};

enum
{
   PROP_0,
   PROP_ADAPTER,
   PROP_COUNT,
   PROP_FILTER,
   PROP_M2M_TABLE,
   PROP_M2M_TYPE,
   PROP_RESOURCE_TYPE,
   PROP_REPOSITORY,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

GomAdapter *
gom_resource_group_get_adapter (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);
   return group->priv->adapter;
}

static void
gom_resource_group_set_adapter (GomResourceGroup *group,
                                GomAdapter       *adapter)
{
   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(GOM_IS_ADAPTER(adapter));

   group->priv->adapter = g_object_ref(adapter);
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_ADAPTER]);
}

GomFilter *
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

guint
gom_resource_group_get_count (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), 0);
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

GType
gom_resource_group_get_m2m_type (GomResourceGroup *group)
{
   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), 0);
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

GomRepository *
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

   group->priv->repository = g_object_ref(repository);
   g_object_notify_by_pspec(G_OBJECT(group), gParamSpecs[PROP_REPOSITORY]);
}

GType
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
set_props (GomResource *resource,
           GomCursor   *cursor)
{
   GObjectClass *klass;
   const gchar *name;
   GParamSpec *pspec;
   GValue value = { 0 };
   guint n_cols;
   guint i;

   g_assert(GOM_IS_RESOURCE(resource));
   g_assert(GOM_IS_CURSOR(cursor));

   klass = G_OBJECT_GET_CLASS(resource);
   n_cols = gom_cursor_get_n_columns(cursor);

   for (i = 0; i < n_cols; i++) {
      name = gom_cursor_get_column_name(cursor, i);
      if ((pspec = g_object_class_find_property(klass, name))) {
         g_value_init(&value, pspec->value_type);
         gom_cursor_get_column(cursor, i, &value);
         g_object_set_property(G_OBJECT(resource), name, &value);
         g_value_unset(&value);
      }
   }
}

static void
gom_resource_group_fetch_cb (GomAdapter *adapter,
                             gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomCommandBuilder *builder = NULL;
   GomResourceGroup *group;
   GomRepository *repository = NULL;
   GomResource *resource;
   GomCommand *command = NULL;
   GomCursor *cursor = NULL;
   GomFilter *filter = NULL;
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
                "adapter", &adapter,
                "filter", &filter,
                "m2m-table", &m2m_table,
                "m2m-type", &m2m_type,
                "repository", &repository,
                "resource-type", &resource_type,
                NULL);
   g_assert(GOM_IS_ADAPTER(adapter));
   g_assert(!filter || GOM_IS_FILTER(filter));
   g_assert(GOM_IS_REPOSITORY(repository));
   g_assert(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));

   limit = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple), "limit"));
   offset = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple), "offset"));
   queue = g_object_get_data(G_OBJECT(simple), "queue");

   builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                          "adapter", adapter,
                          "filter", filter,
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

   if (!group->priv->items) {
      group->priv->items = g_hash_table_new_full(g_int_hash, g_int_equal,
                                                 g_free, g_object_unref);
   }

   idx = offset;

   while (gom_cursor_next(cursor)) {
      guint *key = g_new0(guint, 1);
      *key = idx++;
      resource = g_object_new(resource_type,
                              "repository", repository,
                              NULL);
      set_props(resource, cursor);
      g_hash_table_insert(group->priv->items, key, resource);
   }

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);

out:
   g_clear_object(&adapter);
   g_clear_object(&builder);
   g_clear_object(&command);
   g_clear_object(&cursor);
   g_clear_object(&filter);
   g_clear_object(&repository);
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

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), FALSE);

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(group), NULL, NULL,
                                      gom_resource_group_fetch_sync);
   g_object_set_data(G_OBJECT(simple), "offset", GINT_TO_POINTER(index_));
   g_object_set_data(G_OBJECT(simple), "limit", GINT_TO_POINTER(count));
   g_object_set_data(G_OBJECT(simple), "queue", queue);

   gom_adapter_queue_read(group->priv->adapter, gom_resource_group_fetch_cb, simple);
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

   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(callback != NULL);

   priv = group->priv;

   simple = g_simple_async_result_new(G_OBJECT(group), callback, user_data,
                                      gom_resource_group_fetch_async);
   g_object_set_data(G_OBJECT(simple), "offset", GINT_TO_POINTER(index_));
   g_object_set_data(G_OBJECT(simple), "limit", GINT_TO_POINTER(count));

   gom_adapter_queue_read(priv->adapter, gom_resource_group_fetch_cb, simple);
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

   g_return_val_if_fail(GOM_IS_RESOURCE_GROUP(group), NULL);

   priv = group->priv;

   if (priv->items) {
      return g_hash_table_lookup(priv->items, &index_);
   }

   return NULL;
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

   g_clear_object(&priv->adapter);
   g_clear_object(&priv->filter);

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
   case PROP_ADAPTER:
      g_value_set_object(value, gom_resource_group_get_adapter(group));
      break;
   case PROP_COUNT:
      g_value_set_uint(value, gom_resource_group_get_count(group));
      break;
   case PROP_FILTER:
      g_value_set_object(value, gom_resource_group_get_filter(group));
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
   case PROP_ADAPTER:
      gom_resource_group_set_adapter(group, g_value_get_object(value));
      break;
   case PROP_COUNT:
      gom_resource_group_set_count(group, g_value_get_uint(value));
      break;
   case PROP_FILTER:
      gom_resource_group_set_filter(group, g_value_get_object(value));
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
   g_type_class_add_private(object_class, sizeof(GomResourceGroupPrivate));

   gParamSpecs[PROP_COUNT] =
      g_param_spec_uint("count",
                        _("Count"),
                        _("The size of the resource group."),
                        0,
                        G_MAXUINT,
                        0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_COUNT,
                                   gParamSpecs[PROP_COUNT]);

   gParamSpecs[PROP_ADAPTER] =
      g_param_spec_object("adapter",
                          _("Adapter"),
                          _("The adapter used for queries."),
                          GOM_TYPE_ADAPTER,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_ADAPTER,
                                   gParamSpecs[PROP_ADAPTER]);

   gParamSpecs[PROP_FILTER] =
      g_param_spec_object("filter",
                          _("Filter"),
                          _("The query filter."),
                          GOM_TYPE_FILTER,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_FILTER,
                                   gParamSpecs[PROP_FILTER]);

   gParamSpecs[PROP_M2M_TABLE] =
      g_param_spec_string("m2m-table",
                          _("Many-to-Many Table"),
                          _("The table used to join a Many to Many query."),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_M2M_TABLE,
                                   gParamSpecs[PROP_M2M_TABLE]);

   gParamSpecs[PROP_M2M_TYPE] =
      g_param_spec_gtype("m2m-type",
                          _("Many-to-Many type"),
                          _("The type used in the m2m-table join."),
                          GOM_TYPE_RESOURCE,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_M2M_TYPE,
                                   gParamSpecs[PROP_M2M_TYPE]);

   gParamSpecs[PROP_REPOSITORY] =
      g_param_spec_object("repository",
                          _("Repository"),
                          _("The repository for object storage."),
                          GOM_TYPE_REPOSITORY,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_REPOSITORY,
                                   gParamSpecs[PROP_REPOSITORY]);

   gParamSpecs[PROP_RESOURCE_TYPE] =
      g_param_spec_gtype("resource-type",
                         _("Resource Type"),
                         _("The type of resources contained."),
                         GOM_TYPE_RESOURCE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_RESOURCE_TYPE,
                                   gParamSpecs[PROP_RESOURCE_TYPE]);
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
   group->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(group,
                                  GOM_TYPE_RESOURCE_GROUP,
                                  GomResourceGroupPrivate);
}
