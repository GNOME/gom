/* gom-repository.c
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

#include "gom-command.h"
#include "gom-command-builder.h"
#include "gom-cursor.h"
#include "gom-error.h"
#include "gom-repository.h"

G_DEFINE_TYPE(GomRepository, gom_repository, G_TYPE_OBJECT)

struct _GomRepositoryPrivate
{
   GomAdapter *adapter;
};

enum
{
   PROP_0,
   PROP_ADAPTER,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

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

GomRepository *
gom_repository_new (GomAdapter *adapter)
{
   return g_object_new(GOM_TYPE_REPOSITORY,
                       "adapter", adapter,
                       NULL);
}

/**
 * gom_repository_get_adapter:
 * @repository: (in): A #GomRepository.
 *
 * Fetches the underlying adapter.
 *
 * Returns: (transfer none): A #GomAdapter.
 */
GomAdapter *
gom_repository_get_adapter (GomRepository *repository)
{
   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), NULL);
   return repository->priv->adapter;
}

static void
gom_repository_set_adapter (GomRepository *repository,
                            GomAdapter    *adapter)
{
   GomRepositoryPrivate *priv;

   g_return_if_fail(GOM_IS_REPOSITORY(repository));
   g_return_if_fail(GOM_IS_ADAPTER(adapter));

   priv = repository->priv;

   g_clear_object(&priv->adapter);
   priv->adapter = g_object_ref(adapter);
   g_object_notify_by_pspec(G_OBJECT(repository), gParamSpecs[PROP_ADAPTER]);
}

static gint
gom_repository_query_version (GomRepository  *repository,
                              GError        **error)
{
   GomRepositoryPrivate *priv;
   GomCommand *command;
   GomCursor *cursor;
   gint version;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), -1);

   priv = repository->priv;

   command = g_object_new(GOM_TYPE_COMMAND,
                          "adapter", priv->adapter,
                          "sql", "CREATE TABLE IF NOT EXISTS _gom_version (version INTEGER);",
                          NULL);
   if (!gom_command_execute(command, NULL, error)) {
      g_object_unref(command);
      return -1;
   }
   g_object_unref(command);

   command = g_object_new(GOM_TYPE_COMMAND,
                          "adapter", priv->adapter,
                          "sql", "SELECT MAX(version) FROM _gom_version;",
                          NULL);
   if (!gom_command_execute(command, &cursor, error)) {
      g_object_unref(command);
      return -1;
   }

   if (!gom_cursor_next(cursor)) {
      g_object_unref(cursor);
      g_object_unref(command);
      return 0;
   }

   version = gom_cursor_get_column_uint(cursor, 0);
   g_object_unref(cursor);
   g_object_unref(command);

   return version;
}

static void
gom_repository_migrate_cb (GomAdapter *adapter,
                           gpointer    user_data)
{
   GomRepositoryMigrator migrator;
   GSimpleAsyncResult *simple = user_data;
   GomRepository *repository;
   GomCommand *command = NULL;
   gpointer migrate_data;
   GError *error = NULL;
   guint current;
   guint i;
   guint version;
   GAsyncQueue *queue;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   repository = GOM_REPOSITORY(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
   version = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(simple), "version"));
   migrator = g_object_get_data(G_OBJECT(simple), "migrator");
   migrate_data = g_object_get_data(G_OBJECT(simple), "migrator_data");
   queue = g_object_get_data(G_OBJECT(simple), "queue");

   g_assert(GOM_IS_REPOSITORY(repository));
   g_assert_cmpint(version, >, 0);
   g_assert(migrator != NULL);

   if (-1 == (current = gom_repository_query_version(repository, &error))) {
      g_warning("Failed to determine schema version: %s", error->message);
      goto error;
   }

   if (version == current) {
      g_simple_async_result_set_op_res_gboolean(simple, TRUE);
      goto out;
   }

   EXECUTE_OR_GOTO(adapter, "PRAGMA synchronous = NORMAL;", &error, rollback);
   EXECUTE_OR_GOTO(adapter, "PRAGMA journal_mode = WAL;", &error, rollback);

   EXECUTE_OR_GOTO(adapter, "BEGIN;", &error, rollback);

   for (i = MAX(current, 1); i <= version; i++) {
      if (!migrator(repository, adapter, i, migrate_data, &error)) {
         if (!error)
           g_warning ("Migration function failed without returning a error");
         goto rollback;
      }
      command = g_object_new(GOM_TYPE_COMMAND,
                             "adapter", adapter,
                             "sql", "INSERT INTO _gom_version ("
                                    " version"
                                    ") VALUES (?);",
                             NULL);
      gom_command_set_param_uint(command, 0, i);
      if (!gom_command_execute(command, NULL, &error)) {
         g_object_unref(command);
         goto rollback;
      }
      g_object_unref(command);
   }

   EXECUTE_OR_GOTO(adapter, "COMMIT;", &error, rollback);

   g_simple_async_result_set_op_res_gboolean(simple, TRUE);
   goto out;

rollback:
   EXECUTE_OR_GOTO(adapter, "ROLLBACK;", NULL, error);

error:
   g_assert(error);
   g_simple_async_result_take_error(simple, error);

out:
   g_object_unref(repository);
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
}

/**
 * gom_repository_migrate_sync:
 * @repository: (in): A #GomRepository.
 * @version: (in): The version to migrate to.
 * @migrator: (in) (scope call): A function to perform the migrations.
 * @migrator_data: (in): User data for @migrator.
 * @error: a #GError
 *
 * Performs a migration on the underlying database. This will
 * call @migrator from the SQLite thread for each migration to perform.
 *
 * Returns: #TRUE in case of success.
 */
gboolean
gom_repository_migrate_sync (GomRepository          *repository,
                             guint                   version,
                             GomRepositoryMigrator   migrator,
                             gpointer                migrator_data,
                             GError                **error)
{
   GomRepositoryPrivate *priv;
   GSimpleAsyncResult *simple;
   GAsyncQueue *queue;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), FALSE);
   g_return_val_if_fail(migrator != NULL, FALSE);
   g_return_val_if_fail(version >= 1, FALSE);

   priv = repository->priv;

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(repository), NULL, NULL,
                                      gom_repository_migrate_sync);
   g_object_set_data(G_OBJECT(simple), "version", GINT_TO_POINTER(version));
   g_object_set_data(G_OBJECT(simple), "migrator", migrator);
   g_object_set_data(G_OBJECT(simple), "migrator_data", migrator_data);
   g_object_set_data(G_OBJECT(simple), "queue", queue);

   gom_adapter_queue_write(priv->adapter,
                           gom_repository_migrate_cb,
                           simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   g_object_unref(simple);

   return ret;
}

/**
 * gom_repository_migrate_async:
 * @repository: (in): A #GomRepository.
 * @version: (in): The version to migrate to.
 * @migrator: (in) (scope async): A function to perform the migrations.
 * @migrator_data: (in): User data for @migrator
 * @callback: (in): A callback to execute upon completion.
 * @user_data: (in): User data for @callback.
 *
 * Asynchronously performs a migration on the underlying database. This will
 * call @migrator from the SQLite thread for each migration to perform.
 *
 * Upon completion, @callback will be executed and it must call
 * gom_repository_migrate_finish().
 */
void
gom_repository_migrate_async (GomRepository         *repository,
                              guint                  version,
                              GomRepositoryMigrator  migrator,
                              gpointer               migrator_data,
                              GAsyncReadyCallback    callback,
                              gpointer               user_data)
{
   GomRepositoryPrivate *priv;
   GSimpleAsyncResult *simple;

   g_return_if_fail(GOM_IS_REPOSITORY(repository));
   g_return_if_fail(migrator != NULL);
   g_return_if_fail(callback != NULL);
   g_return_if_fail(version >= 1);

   priv = repository->priv;

   simple = g_simple_async_result_new(G_OBJECT(repository), callback, user_data,
                                      gom_repository_migrate_async);
   g_object_set_data(G_OBJECT(simple), "version", GINT_TO_POINTER(version));
   g_object_set_data(G_OBJECT(simple), "migrator", migrator);
   g_object_set_data(G_OBJECT(simple), "migrator_data", migrator_data);

   gom_adapter_queue_write(priv->adapter,
                           gom_repository_migrate_cb,
                           simple);
}

gboolean
gom_repository_migrate_finish (GomRepository  *repository,
                               GAsyncResult   *result,
                               GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(result), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret;
}

static gboolean
gom_repository_automatic_migrator (GomRepository  *repository,
                                   GomAdapter     *adapter,
                                   guint           version,
                                   gpointer        user_data,
                                   GError        **error)
{
  GList *object_types = user_data;
  GList *l;

  for (l = object_types; l != NULL; l = l->next) {
    GType type = (GType) l->data;
    GomResourceClass *klass;
    GomCommandBuilder *builder;
    GList *cmds, *c;

    klass = g_type_class_ref (type);
    builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                           "adapter", adapter,
                           "resource-type", type,
                           NULL);
    cmds = gom_command_builder_build_create (builder, version);
    g_object_unref (builder);

    for (c = cmds; c != NULL; c = c->next) {
      if (!gom_command_execute(c->data, NULL, error)) {
        goto bail_object;
      }
    }

bail_object:
    g_list_free_full(cmds, g_object_unref);
    g_type_class_unref(klass);
    if (*error)
      return FALSE;
  }

  return TRUE;
}

/**
 * gom_repository_automatic_migrate_sync:
 * @repository: (in): A #GomRepository.
 * @version: (in): The version to migrate to.
 * @object_types: (element-type GType) (transfer container): a #GList of #GType
 * @error: a #GError
 *
 * Performs an automatic migration on the underlying database. For
 * each of the #GType passed in @object_types, the table will be
 * created if necessary, and each of the columns corresponding to
 * properties will be added if needed.
 *
 * Returns: #TRUE in case of success.
 */
gboolean
gom_repository_automatic_migrate_sync (GomRepository          *repository,
                                       guint                   version,
                                       GList                  *object_types,
                                       GError                **error)
{
   gboolean ret;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), FALSE);
   g_return_val_if_fail(version >= 1, FALSE);
   g_return_val_if_fail(object_types != NULL, FALSE);

   ret = gom_repository_migrate_sync(repository, version,
                                     gom_repository_automatic_migrator, object_types,
                                     error);

   g_list_free(object_types);

   return ret;
}

/**
 * gom_repository_automatic_migrate_async:
 * @repository: (in): A #GomRepository.
 * @version: (in): The version to migrate to.
 * @object_types: (element-type GType) (transfer container): a #GList of #GType
 * @callback: (in): A callback to execute upon completion.
 * @user_data: (in): User data for @callback.
 *
 * Performs an automatic migration on the underlying database. See
 * gom_repository_automatic_migrate_sync() for details.
 *
 * Upon completion, @callback will be executed and it must call
 * gom_repository_automatic_migrate_finish().
 */
void
gom_repository_automatic_migrate_async (GomRepository         *repository,
                                        guint                  version,
                                        GList                 *object_types,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
   g_return_if_fail(GOM_IS_REPOSITORY(repository));
   g_return_if_fail(callback != NULL);
   g_return_if_fail(version >= 1);
   g_return_if_fail(object_types != NULL);

   g_object_set_data_full (G_OBJECT (repository), "object-types", object_types, (GDestroyNotify) g_list_free);
   gom_repository_migrate_async (repository, version,
                                 gom_repository_automatic_migrator, object_types,
                                 callback, user_data);
}

gboolean
gom_repository_automatic_migrate_finish (GomRepository  *repository,
                                         GAsyncResult   *result,
                                         GError        **error)
{
   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(result), FALSE);

   g_object_set_data(G_OBJECT(repository), "object-types", NULL);
   return gom_repository_migrate_finish(repository, result, error);
}

static void
gom_repository_find_cb (GomAdapter *adapter,
                        gpointer    user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomCommandBuilder *builder = NULL;
   GomResourceGroup *ret;
   GomRepository *repository = NULL;
   GomCommand *command;
   GomCursor *cursor;
   GomFilter *filter;
   GomSorting *sorting;
   GError *error = NULL;
   GType resource_type;
   GAsyncQueue *queue;
   guint count;

   g_return_if_fail(GOM_IS_ADAPTER(adapter));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   repository = GOM_REPOSITORY(g_async_result_get_source_object(G_ASYNC_RESULT(simple)));
   g_assert(GOM_IS_REPOSITORY(repository));

   resource_type = (GType) g_object_get_data(G_OBJECT(simple), "resource-type");
   g_assert(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));

   filter = g_object_get_data(G_OBJECT(simple), "filter");
   g_assert(!filter || GOM_IS_FILTER(filter));

   sorting = g_object_get_data(G_OBJECT(simple), "sorting");
   g_assert(!sorting || GOM_IS_SORTING(sorting));

   queue = g_object_get_data(G_OBJECT(simple), "queue");

   builder = g_object_new(GOM_TYPE_COMMAND_BUILDER,
                          "adapter", adapter,
                          "resource-type", resource_type,
                          "filter", filter,
                          "sorting", sorting,
                          NULL);

   command = gom_command_builder_build_count(builder);
   g_assert(GOM_IS_COMMAND(command));

   if (!gom_command_execute(command, &cursor, &error)) {
      g_simple_async_result_take_error(simple, error);
      goto out;
   }

   g_assert(GOM_IS_CURSOR(cursor));
   if (!gom_cursor_next(cursor)) {
      g_assert_not_reached();
      goto out;
   }

   count = gom_cursor_get_column_uint(cursor, 0);
   ret = g_object_new(GOM_TYPE_RESOURCE_GROUP,
                      "count", count,
                      "filter", filter,
                      "sorting", sorting,
                      "repository", repository,
                      "resource-type", resource_type,
                      NULL);
   g_simple_async_result_set_op_res_gpointer(simple, ret, g_object_unref);

out:
   if (!queue)
      g_simple_async_result_complete_in_idle(simple);
   else
      g_async_queue_push(queue, GINT_TO_POINTER(TRUE));
   g_object_unref(repository);
   g_clear_object(&cursor);
   g_clear_object(&command);
   g_clear_object(&builder);
}

/**
 * gom_repository_find_sync:
 * @repository: (in): A #GomRepository.
 * @resource_type: (in): The #GType of the resources to query.
 * @filter: (in) (allow-none): An optional filter for the query.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Synchronously queries the #GomRepository for objects matching the
 * requested query. This must only be run from a callback provided to
 * gom_adapter_queue_read().
 *
 * Returns: (transfer full): A #GomResourceGroup or %NULL.
 */
GomResourceGroup *
gom_repository_find_sync (GomRepository  *repository,
                          GType           resource_type,
                          GomFilter      *filter,
                          GError        **error)
{
   return gom_repository_find_sorted_sync(repository, resource_type, filter,
                                          NULL, error);
}

/**
 * gom_repository_find_sorted_sync:
 * @repository: (in): A #GomRepository.
 * @resource_type: (in): The #GType of the resources to query.
 * @filter: (in) (allow-none): An optional filter for the query.
 * @sorting: (in) (allow-none): An optional #GomSorting to order the query
 *                              results.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Synchronously queries the #GomRepository for objects matching the
 * requested query. This must only be run from a callback provided to
 * gom_adapter_queue_read().
 *
 * Returns: (transfer full): A #GomResourceGroup or %NULL.
 */
GomResourceGroup *
gom_repository_find_sorted_sync (GomRepository  *repository,
                                 GType           resource_type,
                                 GomFilter      *filter,
                                 GomSorting     *sorting,
                                 GError        **error)
{
   GomRepositoryPrivate *priv;
   GSimpleAsyncResult *simple;
   GomResourceGroup *ret;
   GAsyncQueue *queue;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), NULL);
   g_return_val_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE), NULL);
   g_return_val_if_fail(resource_type != GOM_TYPE_RESOURCE, NULL);
   g_return_val_if_fail(!filter || GOM_IS_FILTER(filter), NULL);
   g_return_val_if_fail(!sorting || GOM_IS_SORTING(sorting), NULL);

   priv = repository->priv;

   queue = g_async_queue_new();

   simple = g_simple_async_result_new(G_OBJECT(repository), NULL, NULL,
                                      gom_repository_find_sync);
   g_object_set_data(G_OBJECT(simple), "resource-type",
                     GINT_TO_POINTER(resource_type));
   g_object_set_data_full(G_OBJECT(simple), "filter",
                          filter ? g_object_ref(filter) : NULL,
                          filter ? g_object_unref : NULL);
   g_object_set_data_full(G_OBJECT(simple), "sorting",
                          sorting ? g_object_ref(sorting) : NULL,
                          sorting ? g_object_unref : NULL);
   g_object_set_data(G_OBJECT(simple), "queue", queue);

   gom_adapter_queue_read(priv->adapter, gom_repository_find_cb, simple);
   g_async_queue_pop(queue);
   g_async_queue_unref(queue);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   } else {
      ret = g_object_ref(ret);
   }

   g_object_unref(simple);

   return ret;
}

void
gom_repository_find_async (GomRepository       *repository,
                           GType                resource_type,
                           GomFilter           *filter,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
   GomRepositoryPrivate *priv;
   GSimpleAsyncResult *simple;

   g_return_if_fail(GOM_IS_REPOSITORY(repository));
   g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
   g_return_if_fail(resource_type != GOM_TYPE_RESOURCE);
   g_return_if_fail(!filter || GOM_IS_FILTER(filter));
   g_return_if_fail(callback != NULL);

   priv = repository->priv;

   simple = g_simple_async_result_new(G_OBJECT(repository), callback, user_data,
                                      gom_repository_find_async);
   g_object_set_data(G_OBJECT(simple), "resource-type",
                     GINT_TO_POINTER(resource_type));

   g_object_set_data_full(G_OBJECT(simple), "filter",
                          filter ? g_object_ref(filter) : NULL,
                          filter ? g_object_unref : NULL);
   gom_adapter_queue_read(priv->adapter, gom_repository_find_cb, simple);
}

/**
 * gom_repository_find_finish:
 * @repository: (in): A #GomRepository.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to fetch a group of resources.
 *
 * Returns: (transfer full): A #GomResourceGroup.
 */
GomResourceGroup *
gom_repository_find_finish (GomRepository  *repository,
                            GAsyncResult   *result,
                            GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GomResourceGroup *ret;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }
   g_object_unref(simple);

   return ret ? g_object_ref(ret) : NULL;
}

static void
gom_repository_find_one_fetch_cb (GObject      *object,
                                  GAsyncResult *result,
                                  gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomResourceGroup *group = (GomResourceGroup *)object;
   GomResource *resource;
   GError *error = NULL;

   g_return_if_fail(GOM_IS_RESOURCE_GROUP(group));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!gom_resource_group_fetch_finish(group, result, &error)) {
      g_simple_async_result_take_error(simple, error);
      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      return;
   }

   resource = gom_resource_group_get_index(group, 0);
   g_simple_async_result_set_op_res_gpointer(simple,
                                             g_object_ref(resource),
                                             g_object_unref);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);
}

static void
gom_repository_find_one_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   GomResourceGroup *group;
   GomRepository *repository = (GomRepository *)object;
   GError *error = NULL;

   g_return_if_fail(GOM_IS_REPOSITORY(repository));
   g_return_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(group = gom_repository_find_finish(repository, result, &error))) {
      g_simple_async_result_take_error(simple, error);
      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      return;
   }

   if (!gom_resource_group_get_count(group)) {
      g_simple_async_result_set_error(simple, GOM_ERROR,
                                      GOM_ERROR_REPOSITORY_EMPTY_RESULT,
                                      _("No resources were found."));
      g_simple_async_result_complete_in_idle(simple);
      g_object_unref(simple);
      g_object_unref(group);
      return;
   }

   gom_resource_group_fetch_async(group, 0, 1,
                                  gom_repository_find_one_fetch_cb,
                                  simple);
   g_object_unref(group);
}

/**
 * gom_repository_find_one_sync:
 * @repository: (in): A #GomRepository.
 * @resource_type: (in): A #GType of the resource to lookup.
 * @filter: (in) (allow-none): A #GomFilter to apply to your search.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Synchronous version of gom_repository_find_async().
 *
 * Returns: (transfer full): A #GomResource or %NULL.
 */
GomResource *
gom_repository_find_one_sync (GomRepository  *repository,
                              GType           resource_type,
                              GomFilter      *filter,
                              GError        **error)
{
   GomResourceGroup *group;
   GomResource *ret;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), NULL);
   g_return_val_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE), NULL);
   g_return_val_if_fail(resource_type != GOM_TYPE_RESOURCE, NULL);
   g_return_val_if_fail(!filter || GOM_IS_FILTER(filter), NULL);

   if (!(group = gom_repository_find_sync(repository, resource_type,
                                          filter, error))) {
      return NULL;
   }

   if (!gom_resource_group_get_count(group)) {
      g_set_error(error, GOM_ERROR,
                  GOM_ERROR_REPOSITORY_EMPTY_RESULT,
                  _("No resources were found."));
      g_object_unref(group);
      return NULL;
   }

   if (!gom_resource_group_fetch_sync(group, 0, 1, error)) {
      g_object_unref(group);
      return NULL;
   }

   ret = g_object_ref(gom_resource_group_get_index(group, 0));
   g_object_unref(group);

   return ret;
}

void
gom_repository_find_one_async (GomRepository       *repository,
                               GType                resource_type,
                               GomFilter           *filter,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
   GSimpleAsyncResult *simple;

   g_return_if_fail(GOM_IS_REPOSITORY(repository));
   g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
   g_return_if_fail(!filter || GOM_IS_FILTER(filter));
   g_return_if_fail(callback != NULL);

   simple = g_simple_async_result_new(G_OBJECT(repository), callback, user_data,
                                      gom_repository_find_one_async);
   gom_repository_find_async(repository, resource_type, filter,
                             gom_repository_find_one_cb,
                             simple);
}

/**
 * gom_repository_find_one_finish:
 * @repository: (in): A #GomRepository.
 * @result: (in): A #GAsyncResult.
 * @error: (out): A location for a #GError, or %NULL.
 *
 * Completes an asynchronous request to find a single resource in the
 * repository. See gom_repository_find_one_async() for more info.
 *
 * Returns: (transfer full): A #GomResource if successful, otherwise %NULL.
 */
GomResource *
gom_repository_find_one_finish (GomRepository  *repository,
                                GAsyncResult   *result,
                                GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   GomResource *ret;

   g_return_val_if_fail(GOM_IS_REPOSITORY(repository), NULL);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), NULL);

   if (!(ret = g_simple_async_result_get_op_res_gpointer(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret ? g_object_ref(ret) : NULL;
}

/**
 * gom_repository_finalize:
 * @object: (in): A #GomRepository.
 *
 * Finalizer for a #GomRepository instance.  Frees any resources held by
 * the instance.
 */
static void
gom_repository_finalize (GObject *object)
{
   GomRepositoryPrivate *priv = GOM_REPOSITORY(object)->priv;

   g_clear_object(&priv->adapter);

   G_OBJECT_CLASS(gom_repository_parent_class)->finalize(object);
}

/**
 * gom_repository_get_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_repository_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
   GomRepository *repository = GOM_REPOSITORY(object);

   switch (prop_id) {
   case PROP_ADAPTER:
      g_value_set_object(value, gom_repository_get_adapter(repository));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_repository_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_repository_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
   GomRepository *repository = GOM_REPOSITORY(object);

   switch (prop_id) {
   case PROP_ADAPTER:
      gom_repository_set_adapter(repository, g_value_get_object(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

/**
 * gom_repository_class_init:
 * @klass: (in): A #GomRepositoryClass.
 *
 * Initializes the #GomRepositoryClass and prepares the vtable.
 */
static void
gom_repository_class_init (GomRepositoryClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_repository_finalize;
   object_class->get_property = gom_repository_get_property;
   object_class->set_property = gom_repository_set_property;
   g_type_class_add_private(object_class, sizeof(GomRepositoryPrivate));

   gParamSpecs[PROP_ADAPTER] =
      g_param_spec_object("adapter",
                          _("Adapter"),
                          _("The adapter for the repository."),
                          GOM_TYPE_ADAPTER,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_ADAPTER,
                                   gParamSpecs[PROP_ADAPTER]);
}

/**
 * gom_repository_init:
 * @repository: (in): A #GomRepository.
 *
 * Initializes the newly created #GomRepository instance.
 */
static void
gom_repository_init (GomRepository *repository)
{
   repository->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(repository,
                                  GOM_TYPE_REPOSITORY,
                                  GomRepositoryPrivate);
}
