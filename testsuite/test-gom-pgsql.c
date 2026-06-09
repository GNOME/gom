/* test-gom-pgsql.c
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

#include <pgsql-glib.h>
#include <libgom.h>

#include "lib/gom-util-private.h"
#include "test-util.h"

typedef struct _TestPgsqlItem      TestPgsqlItem;
typedef struct _TestPgsqlItemClass TestPgsqlItemClass;

GType test_pgsql_item_get_type (void) G_GNUC_CONST;

struct _TestPgsqlItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
  char      *legacy;
  char      *tag;
};

struct _TestPgsqlItemClass
{
  GomEntityClass parent_class;
};

enum
{
  TEST_PGSQL_ITEM_PROP_0,
  TEST_PGSQL_ITEM_PROP_ID,
  TEST_PGSQL_ITEM_PROP_NAME,
  TEST_PGSQL_ITEM_PROP_LEGACY,
  TEST_PGSQL_ITEM_PROP_TAG,
  TEST_PGSQL_ITEM_N_PROPS,
};

static GParamSpec *test_pgsql_item_properties[TEST_PGSQL_ITEM_N_PROPS];

G_DEFINE_TYPE (TestPgsqlItem, test_pgsql_item, GOM_TYPE_ENTITY)

static void
test_pgsql_item_finalize (GObject *object)
{
  TestPgsqlItem *self = (TestPgsqlItem *) object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->legacy, g_free);
  g_clear_pointer (&self->tag, g_free);

  G_OBJECT_CLASS (test_pgsql_item_parent_class)->finalize (object);
}

static void
test_pgsql_item_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  TestPgsqlItem *self = (TestPgsqlItem *) object;

  switch (prop_id)
    {
    case TEST_PGSQL_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_PGSQL_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_PGSQL_ITEM_PROP_LEGACY:
      g_value_set_string (value, self->legacy);
      break;

    case TEST_PGSQL_ITEM_PROP_TAG:
      g_value_set_string (value, self->tag);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_pgsql_item_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  TestPgsqlItem *self = (TestPgsqlItem *) object;

  switch (prop_id)
    {
    case TEST_PGSQL_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_PGSQL_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_PGSQL_ITEM_PROP_LEGACY:
      g_set_str (&self->legacy, g_value_get_string (value));
      break;

    case TEST_PGSQL_ITEM_PROP_TAG:
      g_set_str (&self->tag, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_pgsql_item_class_init (TestPgsqlItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_pgsql_item_finalize;
  object_class->get_property = test_pgsql_item_get_property;
  object_class->set_property = test_pgsql_item_set_property;

  test_pgsql_item_properties[TEST_PGSQL_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_pgsql_item_properties[TEST_PGSQL_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_pgsql_item_properties[TEST_PGSQL_ITEM_PROP_LEGACY] =
    g_param_spec_string ("legacy", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_pgsql_item_properties[TEST_PGSQL_ITEM_PROP_TAG] =
    g_param_spec_string ("tag", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_PGSQL_ITEM_N_PROPS,
                                     test_pgsql_item_properties);

  gom_entity_class_set_relation (entity_class, "pgsql_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "id", 1);
  gom_entity_class_property_set_version_added (entity_class, "name", 1);
  gom_entity_class_property_set_nonnull (entity_class, "name", TRUE);
  gom_entity_class_property_set_search_flags (entity_class, "name", GOM_SEARCH_INDEXED);
  gom_entity_class_property_set_version_added (entity_class, "legacy", 1);
  gom_entity_class_property_set_version_removed (entity_class, "legacy", 2);
  gom_entity_class_property_set_version_added (entity_class, "tag", 2);
}

static void
test_pgsql_item_init (TestPgsqlItem *self)
{
}

static GomExpression *
test_pgsql_int64_literal_expression (gint64 value)
{
  g_auto(GValue) literal = G_VALUE_INIT;

  g_value_init (&literal, G_TYPE_INT64);
  g_value_set_int64 (&literal, value);

  return gom_literal_expression_new (&literal);
}

static const char *
test_pgsql_require_uri (void)
{
  const char *uri = g_getenv ("GOM_PGSQL_TEST_URI");

  if (uri == NULL || *uri == '\0')
    {
      g_test_skip ("Set GOM_PGSQL_TEST_URI to run PostgreSQL conformance tests");
      return NULL;
    }

  if (g_find_program_in_path ("psql") == NULL)
    {
      g_test_skip ("psql is not installed or not on PATH");
      return NULL;
    }

  return uri;
}

static char **
test_pgsql_parse_uri_list (GUri     *uri,
                           gboolean  keywords)
{
  g_autoptr(GPtrArray) names = NULL;
  g_autoptr(GPtrArray) values = NULL;
  const char *host;
  const char *user;
  const char *password;
  const char *path;
  GHashTable *query_params;
  GHashTableIter iter;
  gpointer key;
  gpointer val;
  char **result;

  names = g_ptr_array_new_with_free_func (g_free);
  values = g_ptr_array_new_with_free_func (g_free);

  host = g_uri_get_host (uri);
  user = g_uri_get_user (uri);
  password = g_uri_get_password (uri);
  path = g_uri_get_path (uri);

  if (!gom_str_empty0 (host))
    {
      g_ptr_array_add (names, g_strdup ("host"));
      g_ptr_array_add (values, g_strdup (host));
    }
  if (g_uri_get_port (uri) > 0)
    {
      g_ptr_array_add (names, g_strdup ("port"));
      g_ptr_array_add (values, g_strdup_printf ("%u", g_uri_get_port (uri)));
    }
  if (!gom_str_empty0 (user))
    {
      g_ptr_array_add (names, g_strdup ("user"));
      g_ptr_array_add (values, g_strdup (user));
    }
  if (!gom_str_empty0 (password))
    {
      g_ptr_array_add (names, g_strdup ("password"));
      g_ptr_array_add (values, g_strdup (password));
    }
  if (path != NULL && path[0] == '/' && path[1] != '\0')
    {
      g_autofree char *dbname = g_uri_unescape_string (path + 1, NULL);
      g_ptr_array_add (names, g_strdup ("dbname"));
      g_ptr_array_add (values, g_steal_pointer (&dbname));
    }

  if (!gom_str_empty0 (g_uri_get_query (uri)))
    {
      query_params = g_uri_parse_params (g_uri_get_query (uri), -1, "&", G_URI_PARAMS_NONE, NULL);
      if (query_params != NULL)
        {
          g_hash_table_iter_init (&iter, query_params);
          while (g_hash_table_iter_next (&iter, &key, &val))
            {
              g_ptr_array_add (names, g_strdup (key));
              g_ptr_array_add (values, g_strdup (val));
            }
          g_hash_table_unref (query_params);
        }
    }

  g_ptr_array_add (names, NULL);
  g_ptr_array_add (values, NULL);

  if (keywords)
    {
      result = g_new0 (char *, names->len);
      for (guint i = 0; i < names->len; i++)
        result[i] = g_strdup (g_ptr_array_index (names, i));
    }
  else
    {
      result = g_new0 (char *, values->len);
      for (guint i = 0; i < values->len; i++)
        result[i] = g_strdup (g_ptr_array_index (values, i));
    }

  return result;
}

static void
test_pgsql_exec_sql (const char *uri,
                     const char *sql)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GUri) guri = NULL;
  g_autoptr(PgsqlConnection) connection = NULL;
  g_auto(GStrv) statements = NULL;
  g_auto(GStrv) keywords = NULL;
  g_auto(GStrv) values = NULL;

  g_assert_nonnull (uri);
  g_assert_nonnull (sql);

  guri = g_uri_parse (uri, G_URI_FLAGS_NONE, &error);
  g_assert_no_error (error);
  g_assert_nonnull (guri);

  keywords = test_pgsql_parse_uri_list (guri, TRUE);
  values = test_pgsql_parse_uri_list (guri, FALSE);

  connection = dex_await_object (pgsql_connection_new ((const char * const *) keywords,
                                                       (const char * const *) values,
                                                       1),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (connection);

  statements = g_strsplit (sql, ";", -1);
  for (guint i = 0; statements[i] != NULL; i++)
    {
      char *statement = g_strstrip (statements[i]);

      if (statement == NULL || *statement == '\0')
        continue;

      if (!dex_await (pgsql_connection_query (connection, statement, NULL), &error))
        {
          g_printerr ("test_pgsql_exec_sql failed: %s\n", error->message);
          g_assert_no_error (error);
        }
    }
}

static gint64
test_pgsql_query_scalar_int64 (const char *uri,
                               const char *sql)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GUri) guri = NULL;
  g_autoptr(PgsqlConnection) connection = NULL;
  g_auto(GStrv) keywords = NULL;
  g_auto(GStrv) values = NULL;
  g_autoptr(PgsqlResult) result = NULL;
  gint64 value;

  g_assert_nonnull (uri);
  g_assert_nonnull (sql);

  guri = g_uri_parse (uri, G_URI_FLAGS_NONE, &error);
  g_assert_no_error (error);
  g_assert_nonnull (guri);

  keywords = test_pgsql_parse_uri_list (guri, TRUE);
  values = test_pgsql_parse_uri_list (guri, FALSE);

  connection = dex_await_object (pgsql_connection_new ((const char * const *) keywords,
                                                       (const char * const *) values,
                                                       1),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (connection);

  result = dex_await_object (pgsql_connection_query (connection, sql, NULL), &error);
  if (result == NULL)
    {
      g_printerr ("test_pgsql_query_scalar_int64 failed: %s\n", error->message);
      g_assert_no_error (error);
    }
  g_assert_nonnull (result);

  g_assert_cmpuint (pgsql_result_get_n_rows (result), ==, 1);
  g_assert_cmpuint (pgsql_result_get_n_fields (result), ==, 1);
  value = g_ascii_strtoll (pgsql_result_get_value (result, 0, 0), NULL, 10);

  return value;
}

static GomRegistry *
test_pgsql_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = NULL;

  builder = gom_registry_builder_new ();
  gom_registry_builder_add_entity_type (builder, test_pgsql_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRepository *
test_pgsql_create_repository (const char   *uri,
                              GomRegistry  *registry,
                              GError      **error)
{
  g_autoptr(GomDriver) driver = NULL;

  driver = gom_driver_open (uri, error);
  g_assert_nonnull (driver);

  return dex_await_object (gom_repository_new (driver, registry, NULL), error);
}

static gboolean
test_pgsql_relation_has_field (GomRepository *repository,
                               const char    *relation,
                               const char    *field_name)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) relation_schema = NULL;
  g_autoptr(GListModel) fields = NULL;
  guint n_items;

  relation_schema = dex_await_object (gom_repository_describe_relation (repository,
                                                                        relation),
                                      &error);
  g_assert_no_error (error);
  g_assert_nonnull (relation_schema);

  fields = gom_relation_schema_list_fields (GOM_RELATION_SCHEMA (relation_schema));
  n_items = g_list_model_get_n_items (fields);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(GObject) field = g_list_model_get_item (fields, i);

      if (g_strcmp0 (gom_schema_get_name (GOM_SCHEMA (field)), field_name) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
test_pgsql_prepare_v1_schema (const char *uri)
{
  test_pgsql_exec_sql (uri,
                       "DROP TABLE IF EXISTS pgsql_items; "
                       "DROP TABLE IF EXISTS gom_schema_version");
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE pgsql_items ("
                       "  id bigint NOT NULL PRIMARY KEY, "
                       "  name text NOT NULL, "
                       "  legacy text"
                       ")");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO pgsql_items (id, name, legacy) "
                       "VALUES (1, 'alpha', 'legacy-value')");
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE gom_schema_version (version integer NOT NULL)");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO gom_schema_version (version) VALUES (1)");
}

static void
test_pgsql_cleanup (const char *uri)
{
  test_pgsql_exec_sql (uri,
                       "DROP TABLE IF EXISTS pgsql_items; "
                       "DROP TABLE IF EXISTS gom_schema_version");
}

static void
test_pgsql_repository_migrate_v1_to_v2_preserves_data (void)
{
  const char *uri = test_pgsql_require_uri ();
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;

  if (uri == NULL)
    return;

  registry = test_pgsql_create_registry ();
  driver = gom_driver_open (uri, &error);
  g_assert_no_error (error);
  g_assert_nonnull (driver);

  test_pgsql_prepare_v1_schema (uri);

  repository = dex_await_object (gom_repository_new (driver, registry, NULL), &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  g_assert_true (test_pgsql_relation_has_field (repository, "pgsql_items", "id"));
  g_assert_true (test_pgsql_relation_has_field (repository, "pgsql_items", "name"));
  g_assert_true (test_pgsql_relation_has_field (repository, "pgsql_items", "tag"));
  g_assert_false (test_pgsql_relation_has_field (repository, "pgsql_items", "legacy"));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_pgsql_item_get_type ());
  gom_query_builder_set_filter (query_builder,
                               gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                                                test_pgsql_int64_literal_expression (1)));
  query = gom_query_builder_build_with_count (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_session_query (session, query), &error);
  g_assert_no_error (error);
  g_assert_nonnull (cursor);
  g_assert_true (gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_COUNT);
  g_assert_cmpuint (gom_cursor_get_count (cursor), ==, 1);

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (gom_cursor_get_n_columns (cursor), ==, 3);
  g_assert_cmpstr (gom_cursor_get_column_name (cursor, 0), ==, "id");
  g_assert_cmpstr (gom_cursor_get_column_name (cursor, 1), ==, "name");
  g_assert_cmpstr (gom_cursor_get_column_name (cursor, 2), ==, "tag");
  g_assert_cmpint (gom_cursor_get_column_int64 (cursor, 0), ==, 1);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "alpha");
  g_assert_null (gom_cursor_get_column_string (cursor, 2));

  g_assert_false (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  g_assert_true (dex_await (gom_session_commit (session), &error));
  g_assert_no_error (error);

  g_clear_object (&cursor);
  g_clear_object (&session);
  g_clear_object (&repository);
  g_clear_object (&driver);

  g_assert_cmpint (test_pgsql_query_scalar_int64 (uri,
                                                  "SELECT version FROM gom_schema_version"),
                   ==,
                   2);

  test_pgsql_cleanup (uri);
}

static void
test_pgsql_session_persist_flush_commit_and_search (void)
{
  const char *uri = test_pgsql_require_uri ();
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomQueryBuilder) search_builder = NULL;
  g_autoptr(GomQuery) search_query = NULL;
  g_autoptr(GomCursor) search_cursor = NULL;
  g_autoptr(GError) error = NULL;
  gint64 id = 0;

  if (uri == NULL)
    return;

  registry = test_pgsql_create_registry ();
  test_pgsql_cleanup (uri);
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE pgsql_items ("
                       "  id bigserial NOT NULL PRIMARY KEY, "
                       "  name text NOT NULL, "
                       "  tag text"
                       ")");
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE gom_schema_version (version integer NOT NULL)");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO gom_schema_version (version) VALUES (2)");

  repository = test_pgsql_create_repository (uri, registry, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  entity = g_object_new (test_pgsql_item_get_type (),
                         "name", "alpha beta",
                         NULL);
  gom_entity_set_repository (entity, repository);

  g_assert_true (dex_await (gom_session_persist (session, GOM_ENTITY (entity)), &error));
  g_assert_no_error (error);

  g_assert_true (dex_await (gom_session_flush (session), &error));
  g_assert_no_error (error);
  g_object_get (entity,
                "id", &id,
                NULL);
  g_assert_cmpint (id, >, 0);

  g_assert_true (dex_await (gom_session_commit (session), &error));
  g_assert_no_error (error);

  g_clear_object (&session);

  g_assert_cmpint (test_pgsql_query_scalar_int64 (uri,
                                                  "SELECT count(*) FROM pgsql_items"),
                   ==,
                   1);

  search_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (search_builder, test_pgsql_item_get_type ());
  gom_query_builder_set_filter (search_builder,
                                gom_binary_expression_new_equal (gom_field_expression_new ("name"),
                                                                 gom_literal_expression_new_string ("alpha beta")));
  search_query = gom_query_builder_build (search_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (search_query);

  search_cursor = dex_await_object (gom_repository_query (repository, search_query), &error);
  g_assert_no_error (error);
  g_assert_nonnull (search_cursor);
  g_assert_true (dex_await_boolean (gom_cursor_next (search_cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_name (search_cursor, 0), ==, "id");
  g_assert_cmpstr (gom_cursor_get_column_name (search_cursor, 1), ==, "name");
  g_assert_cmpint (gom_cursor_get_column_int64 (search_cursor, 0), ==, id);
  g_assert_cmpstr (gom_cursor_get_column_string (search_cursor, 1), ==, "alpha beta");

  g_assert_false (dex_await_boolean (gom_cursor_next (search_cursor), &error));
  g_assert_no_error (error);

  g_clear_object (&search_cursor);
  g_clear_object (&search_query);
  g_clear_pointer (&search_builder, gom_query_builder_unref);
  g_clear_object (&entity);
  g_clear_object (&session);
  g_clear_object (&repository);

  test_pgsql_cleanup (uri);
}

static void
test_pgsql_repository_count (void)
{
  const char *uri = test_pgsql_require_uri ();
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GError) error = NULL;
  gint64 count;

  if (uri == NULL)
    return;

  registry = test_pgsql_create_registry ();
  test_pgsql_cleanup (uri);
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE pgsql_items ("
                       "  id bigserial NOT NULL PRIMARY KEY, "
                       "  name text NOT NULL, "
                       "  tag text"
                       ")");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO pgsql_items (name, tag) VALUES "
                       "('alpha', 'one'), "
                       "('beta', 'two'), "
                       "('gamma', 'three')");
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE gom_schema_version (version integer NOT NULL)");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO gom_schema_version (version) VALUES (2)");

  repository = test_pgsql_create_repository (uri, registry, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_pgsql_item_get_type ());
  gom_query_builder_set_limit (query_builder, 1);
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  count = dex_await_int64 (gom_repository_count (repository, query), &error);
  g_assert_no_error (error);
  g_assert_cmpint (count, ==, 3);

  g_clear_object (&repository);
  test_pgsql_cleanup (uri);
}

static void
test_pgsql_repository_mutate_limits_and_update_results (void)
{
  const char *uri = test_pgsql_require_uri ();
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomUpdateBuilder) update_builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomDeletionBuilder) deletion_builder = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;

  if (uri == NULL)
    return;

  registry = test_pgsql_create_registry ();
  test_pgsql_cleanup (uri);
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE pgsql_items ("
                       "  id bigserial NOT NULL PRIMARY KEY, "
                       "  name text NOT NULL, "
                       "  tag text"
                       ")");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO pgsql_items (name, tag) VALUES "
                       "('alpha', 'one'), "
                       "('beta', 'two'), "
                       "('gamma', 'three')");
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE gom_schema_version (version integer NOT NULL)");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO gom_schema_version (version) VALUES (2)");

  repository = test_pgsql_create_repository (uri, registry, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  update_builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (update_builder, test_pgsql_item_get_type ());
  gom_update_builder_add_assignment (update_builder,
                                     gom_field_expression_new ("tag"),
                                     gom_literal_expression_new_string ("updated"));
  gom_update_builder_set_limit (update_builder, 2);
  update = gom_update_builder_build (update_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (update);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (update)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 2);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (result)), ==, 2);
  g_assert_cmpint (test_pgsql_query_scalar_int64 (uri,
                                                  "SELECT count(*) FROM pgsql_items "
                                                  "WHERE tag = 'updated'"),
                   ==,
                   2);

  deletion_builder = gom_deletion_builder_new ();
  gom_deletion_builder_set_target_relation (deletion_builder, "pgsql_items");
  gom_deletion_builder_set_limit (deletion_builder, 1);
  deletion = gom_deletion_builder_build (deletion_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (deletion);

  g_clear_object (&result);
  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (deletion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (result)), ==, 1);
  g_assert_cmpint (test_pgsql_query_scalar_int64 (uri,
                                                  "SELECT count(*) FROM pgsql_items"),
                   ==,
                   2);

  g_clear_object (&repository);
  test_pgsql_cleanup (uri);
}

static void
test_pgsql_repository_multi_insert_is_atomic (void)
{
  const char *uri = test_pgsql_require_uri ();
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomInsertionBuilder) insertion_builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;

  if (uri == NULL)
    return;

  registry = test_pgsql_create_registry ();
  test_pgsql_cleanup (uri);
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE pgsql_items ("
                       "  id bigint NOT NULL PRIMARY KEY, "
                       "  name text NOT NULL, "
                       "  tag text"
                       ")");
  test_pgsql_exec_sql (uri,
                       "CREATE TABLE gom_schema_version (version integer NOT NULL)");
  test_pgsql_exec_sql (uri,
                       "INSERT INTO gom_schema_version (version) VALUES (2)");

  repository = test_pgsql_create_repository (uri, registry, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  insertion_builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_relation (insertion_builder, "pgsql_items");
  gom_insertion_builder_add_column (insertion_builder, gom_field_expression_new ("id"));
  gom_insertion_builder_add_column (insertion_builder, gom_field_expression_new ("name"));

  {
    GomExpression *row[] = {
      test_pgsql_int64_literal_expression (1),
      gom_literal_expression_new_string ("alpha"),
    };
    gom_insertion_builder_add_row (insertion_builder, row, G_N_ELEMENTS (row));
  }

  {
    GomExpression *row[] = {
      test_pgsql_int64_literal_expression (1),
      gom_literal_expression_new_string ("duplicate"),
    };
    gom_insertion_builder_add_row (insertion_builder, row, G_N_ELEMENTS (row));
  }

  insertion = gom_insertion_builder_build (insertion_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (insertion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), &error);
  g_assert_nonnull (error);
  g_assert_null (result);
  g_clear_error (&error);
  g_assert_cmpint (test_pgsql_query_scalar_int64 (uri,
                                                  "SELECT count(*) FROM pgsql_items"),
                   ==,
                   0);

  g_clear_object (&repository);
  test_pgsql_cleanup (uri);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Pgsql/repository-migrate-v1-to-v2-preserves-data",
                    test_pgsql_repository_migrate_v1_to_v2_preserves_data);
  _g_test_add_func ("/Gom/Pgsql/repository-count",
                    test_pgsql_repository_count);
  _g_test_add_func ("/Gom/Pgsql/repository-mutate-limits-and-update-results",
                    test_pgsql_repository_mutate_limits_and_update_results);
  _g_test_add_func ("/Gom/Pgsql/repository-multi-insert-is-atomic",
                    test_pgsql_repository_multi_insert_is_atomic);
  _g_test_add_func ("/Gom/Pgsql/session-persist-flush-commit-and-search",
                    test_pgsql_session_persist_flush_commit_and_search);
  return g_test_run ();
}
