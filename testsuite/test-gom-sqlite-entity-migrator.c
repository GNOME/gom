/* test-gom-sqlite-entity-migrator.c
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

#include <glib/gstdio.h>
#include <sqlite3.h>

#include <libgom.h>

#include "test-util.h"

typedef struct _TestAutoMigrateItem             TestAutoMigrateItem;
typedef struct _TestAutoMigrateItemClass        TestAutoMigrateItemClass;
typedef struct _TestVersionedMigrateItem        TestVersionedMigrateItem;
typedef struct _TestVersionedMigrateItemClass   TestVersionedMigrateItemClass;
typedef struct _TestVersionedMigrateItemV5      TestVersionedMigrateItemV5;
typedef struct _TestVersionedMigrateItemV5Class TestVersionedMigrateItemV5Class;

struct _TestAutoMigrateItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
};

struct _TestAutoMigrateItemClass
{
  GomEntityClass parent_class;
};

struct _TestVersionedMigrateItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *title;
  char      *subtitle;
  char      *keywords;
  gint64     rank;
};

struct _TestVersionedMigrateItemClass
{
  GomEntityClass parent_class;
};

struct _TestVersionedMigrateItemV5
{
  TestVersionedMigrateItem parent_instance;
};

struct _TestVersionedMigrateItemV5Class
{
  TestVersionedMigrateItemClass parent_class;
};

enum {
  TEST_AUTO_MIGRATE_ITEM_PROP_0,
  TEST_AUTO_MIGRATE_ITEM_PROP_ID,
  TEST_AUTO_MIGRATE_ITEM_PROP_NAME,
  TEST_AUTO_MIGRATE_ITEM_N_PROPS
};

enum {
  TEST_VERSIONED_MIGRATE_ITEM_PROP_0,
  TEST_VERSIONED_MIGRATE_ITEM_PROP_ID,
  TEST_VERSIONED_MIGRATE_ITEM_PROP_TITLE,
  TEST_VERSIONED_MIGRATE_ITEM_PROP_SUBTITLE,
  TEST_VERSIONED_MIGRATE_ITEM_PROP_KEYWORDS,
  TEST_VERSIONED_MIGRATE_ITEM_PROP_RANK,
  TEST_VERSIONED_MIGRATE_ITEM_N_PROPS
};

static GParamSpec *test_auto_migrate_item_properties[TEST_AUTO_MIGRATE_ITEM_N_PROPS];
static GParamSpec *test_versioned_migrate_item_properties[TEST_VERSIONED_MIGRATE_ITEM_N_PROPS];

GType test_auto_migrate_item_get_type         (void) G_GNUC_CONST;
GType test_versioned_migrate_item_get_type    (void) G_GNUC_CONST;
GType test_versioned_migrate_item_v5_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (TestAutoMigrateItem, test_auto_migrate_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestVersionedMigrateItem, test_versioned_migrate_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestVersionedMigrateItemV5, test_versioned_migrate_item_v5, test_versioned_migrate_item_get_type ())

static void
test_auto_migrate_item_finalize (GObject *object)
{
  TestAutoMigrateItem *self = (TestAutoMigrateItem *) object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (test_auto_migrate_item_parent_class)->finalize (object);
}

static void
test_auto_migrate_item_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  TestAutoMigrateItem *self = (TestAutoMigrateItem *) object;

  switch (prop_id)
    {
    case TEST_AUTO_MIGRATE_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_AUTO_MIGRATE_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_auto_migrate_item_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  TestAutoMigrateItem *self = (TestAutoMigrateItem *) object;

  switch (prop_id)
    {
    case TEST_AUTO_MIGRATE_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_AUTO_MIGRATE_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_auto_migrate_item_class_init (TestAutoMigrateItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_auto_migrate_item_finalize;
  object_class->get_property = test_auto_migrate_item_get_property;
  object_class->set_property = test_auto_migrate_item_set_property;

  test_auto_migrate_item_properties[TEST_AUTO_MIGRATE_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_auto_migrate_item_properties[TEST_AUTO_MIGRATE_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_AUTO_MIGRATE_ITEM_N_PROPS,
                                     test_auto_migrate_item_properties);

  gom_entity_class_set_relation (entity_class, "auto_migrate_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "name", 1);
  gom_entity_class_property_set_nonnull (entity_class, "name", TRUE);
  gom_entity_class_property_set_search_flags (entity_class, "name", GOM_SEARCH_INDEXED);
}

static void
test_auto_migrate_item_init (TestAutoMigrateItem *self)
{
}

static void
test_versioned_migrate_item_finalize (GObject *object)
{
  TestVersionedMigrateItem *self = (TestVersionedMigrateItem *) object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);
  g_clear_pointer (&self->keywords, g_free);

  G_OBJECT_CLASS (test_versioned_migrate_item_parent_class)->finalize (object);
}

static void
test_versioned_migrate_item_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  TestVersionedMigrateItem *self = (TestVersionedMigrateItem *) object;

  switch (prop_id)
    {
    case TEST_VERSIONED_MIGRATE_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_SUBTITLE:
      g_value_set_string (value, self->subtitle);
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_KEYWORDS:
      g_value_set_string (value, self->keywords);
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_RANK:
      g_value_set_int64 (value, self->rank);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_versioned_migrate_item_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  TestVersionedMigrateItem *self = (TestVersionedMigrateItem *) object;

  switch (prop_id)
    {
    case TEST_VERSIONED_MIGRATE_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_TITLE:
      g_set_str (&self->title, g_value_get_string (value));
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_SUBTITLE:
      g_set_str (&self->subtitle, g_value_get_string (value));
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_KEYWORDS:
      g_set_str (&self->keywords, g_value_get_string (value));
      break;

    case TEST_VERSIONED_MIGRATE_ITEM_PROP_RANK:
      self->rank = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_versioned_migrate_item_class_init (TestVersionedMigrateItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_versioned_migrate_item_finalize;
  object_class->get_property = test_versioned_migrate_item_get_property;
  object_class->set_property = test_versioned_migrate_item_set_property;

  test_versioned_migrate_item_properties[TEST_VERSIONED_MIGRATE_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_versioned_migrate_item_properties[TEST_VERSIONED_MIGRATE_ITEM_PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_versioned_migrate_item_properties[TEST_VERSIONED_MIGRATE_ITEM_PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_versioned_migrate_item_properties[TEST_VERSIONED_MIGRATE_ITEM_PROP_KEYWORDS] =
    g_param_spec_string ("keywords", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_versioned_migrate_item_properties[TEST_VERSIONED_MIGRATE_ITEM_PROP_RANK] =
    g_param_spec_int64 ("rank", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_VERSIONED_MIGRATE_ITEM_N_PROPS,
                                     test_versioned_migrate_item_properties);

  gom_entity_class_set_relation (entity_class, "versioned_migrate_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "title", 1);
  gom_entity_class_property_set_nonnull (entity_class, "title", TRUE);
  gom_entity_class_property_set_search_flags (entity_class, "title", GOM_SEARCH_INDEXED);
  gom_entity_class_property_set_version_added (entity_class, "subtitle", 2);
  gom_entity_class_property_set_version_added (entity_class, "keywords", 3);
  gom_entity_class_property_set_search_flags (entity_class, "keywords", GOM_SEARCH_INDEXED);
  gom_entity_class_property_set_version_added (entity_class, "rank", 4);
}

static void
test_versioned_migrate_item_init (TestVersionedMigrateItem *self)
{
}

static GomRegistry *
test_sqlite_create_auto_migrate_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_auto_migrate_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_sqlite_create_versioned_migrate_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_versioned_migrate_item_get_type ());

  return gom_registry_builder_build (builder);
}

static void
test_versioned_migrate_item_v5_class_init (TestVersionedMigrateItemV5Class *klass)
{
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  gom_entity_class_set_version_removed (entity_class, 5);
}

static void
test_versioned_migrate_item_v5_init (TestVersionedMigrateItemV5 *self)
{
}

static GomRegistry *
test_sqlite_create_versioned_migrate_registry_with_entity_removed (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_versioned_migrate_item_v5_get_type ());

  return gom_registry_builder_build (builder);
}

static void
test_sqlite_entity_migrator_auto_creates_schema_single_version_crud (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) inserted = NULL;
  g_autoptr(GomEntity) materialized = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gint64 id;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-entity-migrator-test-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_sqlite_create_auto_migrate_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  test_sqlite_open (context.db_path, &db);
  g_assert_cmpuint (test_sqlite_read_user_version (db), ==, 1);
  g_assert_true (test_sqlite_relation_exists (db, "auto_migrate_items", "table"));
  g_assert_true (test_sqlite_relation_exists (db, "auto_migrate_items_name", "index"));
  test_sqlite_close (db);
  db = NULL;

  inserted = g_object_new (test_auto_migrate_item_get_type (),
                           "name", "alpha",
                           NULL);
  gom_entity_set_repository (inserted, repository);

  result = dex_await_object (gom_entity_insert (inserted), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_object_get (inserted,
                "id", &id,
                NULL);
  g_assert_cmpint ((gint) id, >, 0);
  g_clear_object (&result);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_auto_migrate_item_get_type ());
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_nonnull (cursor);
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  materialized = gom_cursor_materialize (cursor, &error);
  g_assert_no_error (error);
  g_assert_nonnull (materialized);
  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
  g_clear_object (&cursor);

  g_object_set (materialized,
                "name", "beta",
                NULL);

  result = dex_await_object (gom_entity_update (materialized), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_clear_object (&result);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT name FROM auto_migrate_items WHERE id = ?1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_int64 (stmt, 1, id);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "beta");
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  result = dex_await_object (gom_entity_delete (materialized), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_clear_object (&result);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT COUNT(*) FROM auto_migrate_items WHERE id = ?1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_int64 (stmt, 1, id);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_int (stmt, 0), ==, 0);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

}

static void
test_sqlite_entity_migrator_multi_version_with_final_entity_removal (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry_v4 = NULL;
  g_autoptr(GomRegistry) registry_v5 = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-entity-migrator-versioned-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE versioned_migrate_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  title TEXT NOT NULL"
                     ");"
                     "INSERT INTO versioned_migrate_items (id, title) VALUES (1, 'alpha');"
                     "PRAGMA user_version = 1"
  );
  test_sqlite_close (db);
  db = NULL;

  registry_v4 = test_sqlite_create_versioned_migrate_registry ();
  g_assert_cmpuint (gom_registry_get_max_version (registry_v4), ==, 4);
  repository = test_sqlite_context_create_repository (&context, registry_v4, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  test_sqlite_open (context.db_path, &db);

  g_assert_cmpuint (test_sqlite_read_user_version (db), ==, 4);
  g_assert_true (test_sqlite_relation_exists (db, "versioned_migrate_items", "table"));
  g_assert_true (test_sqlite_column_exists (db, "versioned_migrate_items", "id"));
  g_assert_true (test_sqlite_column_exists (db, "versioned_migrate_items", "title"));
  g_assert_true (test_sqlite_column_exists (db, "versioned_migrate_items", "subtitle"));
  g_assert_true (test_sqlite_column_exists (db, "versioned_migrate_items", "keywords"));
  g_assert_true (test_sqlite_column_exists (db, "versioned_migrate_items", "rank"));
  g_assert_true (test_sqlite_relation_exists (db, "versioned_migrate_items_title", "index"));
  g_assert_true (test_sqlite_relation_exists (db, "versioned_migrate_items_keywords", "index"));
  g_assert_true (test_sqlite_relation_exists (db, "versioned_migrate_items_fts", "table"));

  rc = sqlite3_prepare_v2 (db,
                           "SELECT title, subtitle, keywords, rank "
                           "FROM versioned_migrate_items WHERE id = 1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "alpha");
  g_assert_cmpint (sqlite3_column_type (stmt, 1), ==, SQLITE_NULL);
  g_assert_cmpint (sqlite3_column_type (stmt, 2), ==, SQLITE_NULL);
  g_assert_cmpint (sqlite3_column_type (stmt, 3), ==, SQLITE_NULL);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  registry_v5 = test_sqlite_create_versioned_migrate_registry_with_entity_removed ();
  g_assert_cmpuint (gom_registry_get_max_version (registry_v5), ==, 5);
  g_clear_object (&repository);
  repository = test_sqlite_context_create_repository (&context, registry_v5, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  test_sqlite_open (context.db_path, &db);

  g_assert_cmpuint (test_sqlite_read_user_version (db), ==, 5);
  g_assert_false (test_sqlite_relation_exists (db, "versioned_migrate_items", "table"));
  g_assert_false (test_sqlite_relation_exists (db, "versioned_migrate_items_title", "index"));
  g_assert_false (test_sqlite_relation_exists (db, "versioned_migrate_items_keywords", "index"));
  g_assert_false (test_sqlite_relation_exists (db, "versioned_migrate_items_fts", "table"));
  test_sqlite_close (db);
  db = NULL;

}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  _g_test_add_func ("/Gom/Sqlite/entity-migrator-auto-creates-schema-single-version-crud",
                    test_sqlite_entity_migrator_auto_creates_schema_single_version_crud);
  _g_test_add_func ("/Gom/Sqlite/entity-migrator-multi-version-with-final-entity-removal",
                    test_sqlite_entity_migrator_multi_version_with_final_entity_removal);

  return g_test_run ();
}
