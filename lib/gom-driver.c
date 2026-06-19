/* gom-driver.c
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

#include <gmodule.h>

#include "gom-config.h"
#include "gom-driver-options.h"
#include "gom-driver-private.h"
#include "gom-meta.h"
#include "gom-mutation-private.h"
#include "gom-query-private.h"
#include "gom-repository.h"
#include "gom-schema.h"
#include "gom-util-private.h"

enum
{
  PROP_0,
  PROP_URI,
  N_PROPS
};

G_DEFINE_ABSTRACT_TYPE (GomDriver, gom_driver, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gom_driver_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GomDriver *self = GOM_DRIVER (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_take_string (value, gom_driver_dup_uri (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_driver_class_init (GomDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gom_driver_get_property;

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_driver_init (GomDriver *self)
{
}

typedef struct
{
  const char *scheme;
  const char *module_name;
  const char *symbol_name;
} GomDriverBackend;

static const GomDriverBackend gom_driver_backends[] = {
#if defined(GOM_DATABASE_SQLITE)
  { "file", "gom-sqlite-module", "_gom_sqlite_driver_new" },
  { "sqlite", "gom-sqlite-module", "_gom_sqlite_driver_new" },
#endif
#if defined(GOM_DATABASE_POSTGRESQL)
  { "postgresql", "gom-pgsql-module", "_gom_pgsql_driver_new" },
#endif
  { NULL, NULL, NULL },
};

static GMutex gom_driver_module_mutex;
static GHashTable *gom_driver_module_cache;

static const GomDriverBackend *
gom_driver_lookup_backend (const char *scheme)
{
  for (guint i = 0; gom_driver_backends[i].scheme != NULL; i++)
    {
      if (g_strcmp0 (gom_driver_backends[i].scheme, scheme) == 0)
        return &gom_driver_backends[i];
    }

  return NULL;
}

static const char *
gom_driver_get_default_module_dir (void)
{
  static char *module_dir;

  if (module_dir == NULL)
    module_dir = g_strdup (GOM_MODULES_DIR_DEFAULT);

  return module_dir;
}

static const char *
gom_driver_get_module_dir (const GomDriverBackend *backend)
{
  const char *module_dir;

  module_dir = g_getenv ("GOM_MODULES_DIR");

  if (!gom_str_empty0 (module_dir))
    return module_dir;

  return gom_driver_get_default_module_dir ();
}

static GModule *
gom_driver_ensure_module_loaded (const GomDriverBackend  *backend,
                                 GError                 **error)
{
  g_autofree char *module_filename = NULL;
  g_autofree char *module_path = NULL;
  g_autoptr(GMutexLocker) locker = NULL;
  GModule *module;

  g_return_val_if_fail (backend != NULL, NULL);

  locker = g_mutex_locker_new (&gom_driver_module_mutex);

  if (gom_driver_module_cache == NULL)
    gom_driver_module_cache = g_hash_table_new (g_str_hash, g_str_equal);

  if ((module = g_hash_table_lookup (gom_driver_module_cache, backend->module_name)))
    return module;

  module_filename = g_strdup_printf ("%s.%s", backend->module_name, G_MODULE_SUFFIX);
  module_path = g_build_filename (gom_driver_get_module_dir (backend), module_filename, NULL);

  if (!(module = g_module_open (module_path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to load backend module `%s`: %s",
                   module_path,
                   g_module_error ());
      return NULL;
    }

  g_hash_table_insert (gom_driver_module_cache, (gpointer)backend->module_name, module);

  return module;
}

/**
 * gom_driver_dup_uri:
 * @self: a [class@Gom.Driver]
 *
 * Gets the URI of where the driver is connecting.
 *
 * Returns: (transfer full) (nullable):
 */
char *
gom_driver_dup_uri (GomDriver *self)
{
  g_return_val_if_fail (GOM_IS_DRIVER (self), NULL);

  return GOM_DRIVER_GET_CLASS (self)->dup_uri (self);
}

/**
 * _gom_driver_query:
 * @self: a [class@Gom.Driver]
 * @registry: a [class@Gom.Registry]
 * @query: a [class@Gom.Query]
 * @flags: a set of #GomCursorFlags
 *
 * Executes the query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Cursor] or rejects with error.
 */
DexFuture *
_gom_driver_query (GomDriver      *self,
                   GomRepository  *repository,
                   GomQuery       *query,
                   GomCursorFlags  flags)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (GOM_IS_QUERY (query));

  if (GOM_DRIVER_GET_CLASS (self)->query)
    return GOM_DRIVER_GET_CLASS (self)->query (self, repository, query, flags);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Query is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_mutate:
 * @self: a [class@Gom.Driver]
 * @registry: a [class@Gom.Registry]
 * @mutation: a [class@Gom.Driver]
 *
 * Applies @mutation.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Gom.Cursor] or rejects with error.
 */
DexFuture *
_gom_driver_mutate (GomDriver   *self,
                    GomRegistry *registry,
                    GomMutation *mutation)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_REGISTRY (registry));
  dex_return_error_if_fail (GOM_IS_MUTATION (mutation));

  if (GOM_DRIVER_GET_CLASS (self)->mutate)
    return GOM_DRIVER_GET_CLASS (self)->mutate (self, registry, mutation);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Mutation is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_describe_relation:
 * @self: a [class@Gom.Driver]
 * @registry: a [class@Gom.Registry]
 * @relation: a relation name
 *
 * Describes @relation.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to
 *   a [class@Gom.RelationSchema] or rejects with error.
 */
DexFuture *
_gom_driver_describe_relation (GomDriver   *self,
                               GomRegistry *registry,
                               const char  *relation)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_REGISTRY (registry));
  dex_return_error_if_fail (relation != NULL);

  if (GOM_DRIVER_GET_CLASS (self)->describe_relation)
    return GOM_DRIVER_GET_CLASS (self)->describe_relation (self, registry, relation);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Schema description is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_list_relations:
 * @self: a [class@Gom.Driver]
 * @registry: a [class@Gom.Registry]
 *
 * Lists relations.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   `GStrv` of relation names or rejects with error.
 */
DexFuture *
_gom_driver_list_relations (GomDriver   *self,
                            GomRegistry *registry)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_REGISTRY (registry));

  if (GOM_DRIVER_GET_CLASS (self)->list_relations)
    return GOM_DRIVER_GET_CLASS (self)->list_relations (self, registry);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Listing relations is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_query_version:
 * @self: a [class@Gom.Driver]
 *
 * Gets the current schema version for the driver.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   `guint` or rejects with error.
 */
DexFuture *
_gom_driver_query_version (GomDriver *self)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));

  if (GOM_DRIVER_GET_CLASS (self)->query_version)
    return GOM_DRIVER_GET_CLASS (self)->query_version (self);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Getting schema version is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_migrate:
 * @self: a [class@Gom.Driver]
 * @current: a [class@Gom.Registry] snapshot for the current version
 * @next: a [class@Gom.Registry] snapshot for the next version
 *
 * Migrates the schema from @current to @next.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
_gom_driver_migrate (GomDriver   *self,
                     GomRegistry *current,
                     GomRegistry *next)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_REGISTRY (current));
  dex_return_error_if_fail (GOM_IS_REGISTRY (next));

  if (GOM_DRIVER_GET_CLASS (self)->migrate)
    return GOM_DRIVER_GET_CLASS (self)->migrate (self, current, next);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Migration is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_execute_sql:
 * @self: a [class@Gom.Driver]
 * @script: SQL script to execute
 *
 * Executes @script directly on the backend, if supported.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
_gom_driver_execute_sql (GomDriver *self,
                         GBytes    *script)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (script != NULL);

  if (GOM_DRIVER_GET_CLASS (self)->execute_sql)
    return GOM_DRIVER_GET_CLASS (self)->execute_sql (self, script);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Executing SQL scripts is not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

/**
 * _gom_driver_begin_session:
 * @self: a [class@Gom.Driver]
 * @repository: a [class@Gom.Repository]
 *
 * Begins a transaction-scoped session.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to a
 *   [class@Gom.Session] or rejects with error.
 */
DexFuture *
_gom_driver_begin_session (GomDriver     *self,
                           GomRepository *repository)
{
  dex_return_error_if_fail (GOM_IS_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));

  if (GOM_DRIVER_GET_CLASS (self)->begin_session)
    return GOM_DRIVER_GET_CLASS (self)->begin_session (self, repository);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Sessions are not supported by `%s`",
                                G_OBJECT_TYPE_NAME (self));
}

gboolean
_gom_driver_supports_feature (GomDriver            *self,
                              GomRepositoryFeature  feature)
{
  g_return_val_if_fail (GOM_IS_DRIVER (self), FALSE);

  if (GOM_DRIVER_GET_CLASS (self)->supports_feature)
    return GOM_DRIVER_GET_CLASS (self)->supports_feature (self, feature);

  return FALSE;
}

gboolean
_gom_driver_supports_vector_distance (GomDriver       *self,
                                      GomVectorFormat  format,
                                      GomVectorMetric  metric)
{
  g_return_val_if_fail (GOM_IS_DRIVER (self), FALSE);

  if (GOM_DRIVER_GET_CLASS (self)->supports_vector_distance)
    return GOM_DRIVER_GET_CLASS (self)->supports_vector_distance (self, format, metric);

  return FALSE;
}

/**
 * gom_driver_open_with_options:
 * @uri: a database URI
 * @options: (nullable): driver options
 * @error: return location for a #GError, or %NULL
 *
 * Opens the database driver for @uri by loading the appropriate backend
 * module at runtime and passing @options to the backend constructor.
 *
 * Returns: (transfer full): a [class@Gom.Driver] or %NULL on error.
 */
GomDriver *
gom_driver_open_with_options (const char        *uri,
                              GomDriverOptions  *options,
                              GError           **error)
{
  const GomDriverBackend *backend;
  GomDriverConstructor driver_new = NULL;
  g_autofree char *scheme = NULL;
  gpointer symbol = NULL;
  GModule *module;

  g_return_val_if_fail (uri != NULL, NULL);
  g_return_val_if_fail (options == NULL || GOM_IS_DRIVER_OPTIONS (options), NULL);

  if (!(scheme = g_uri_parse_scheme (uri)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "URI `%s` does not have a supported scheme",
                   uri);
      return NULL;
    }

  if (!(backend = gom_driver_lookup_backend (scheme)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "No driver module is available for scheme `%s`",
                   scheme);
      return NULL;
    }

  if (!(module = gom_driver_ensure_module_loaded (backend, error)))
    return NULL;

  if (!g_module_symbol (module, backend->symbol_name, &symbol) || symbol == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Backend module `%s` does not export `%s`: %s",
                   backend->module_name,
                   backend->symbol_name,
                   g_module_error ());
      return NULL;
    }

  driver_new = (GomDriverConstructor)symbol;

  return driver_new (uri, options, error);
}

/**
 * gom_driver_open:
 * @uri: a database URI
 * @error: return location for a #GError, or %NULL
 *
 * Opens the database driver for @uri by loading the appropriate backend
 * module at runtime.
 *
 * Returns: (transfer full): a [class@Gom.Driver] or %NULL on error.
 */
GomDriver *
gom_driver_open (const char  *uri,
                 GError     **error)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return gom_driver_open_with_options (uri, NULL, error);
}

void
_gom_driver_acquire_repository (GomDriver *self)
{
  g_return_if_fail (GOM_IS_DRIVER (self));

  g_atomic_int_inc (&self->repository_use_count);
}

void
_gom_driver_release_repository (GomDriver *self)
{
  g_return_if_fail (GOM_IS_DRIVER (self));

  g_atomic_int_dec_and_test (&self->repository_use_count);
}
