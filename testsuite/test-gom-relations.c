/* test-gom-relations.c
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

typedef struct _TestRelationAuthor      TestRelationAuthor;
typedef struct _TestRelationAuthorClass TestRelationAuthorClass;
typedef struct _TestRelationBook        TestRelationBook;
typedef struct _TestRelationBookClass   TestRelationBookClass;
typedef struct _TestRelationTag         TestRelationTag;
typedef struct _TestRelationTagClass    TestRelationTagClass;

struct _TestRelationAuthor
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
};

struct _TestRelationAuthorClass
{
  GomEntityClass parent_class;
};

struct _TestRelationBook
{
  GomEntity  parent_instance;
  gint64     id;
  gint64     author_id;
  char      *title;
};

struct _TestRelationBookClass
{
  GomEntityClass parent_class;
};

struct _TestRelationTag
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
};

struct _TestRelationTagClass
{
  GomEntityClass parent_class;
};

enum {
  TEST_RELATION_AUTHOR_PROP_0,
  TEST_RELATION_AUTHOR_PROP_ID,
  TEST_RELATION_AUTHOR_PROP_NAME,
  TEST_RELATION_AUTHOR_N_PROPS
};

enum {
  TEST_RELATION_BOOK_PROP_0,
  TEST_RELATION_BOOK_PROP_ID,
  TEST_RELATION_BOOK_PROP_AUTHOR_ID,
  TEST_RELATION_BOOK_PROP_TITLE,
  TEST_RELATION_BOOK_N_PROPS
};

enum {
  TEST_RELATION_TAG_PROP_0,
  TEST_RELATION_TAG_PROP_ID,
  TEST_RELATION_TAG_PROP_NAME,
  TEST_RELATION_TAG_N_PROPS
};

static GParamSpec *test_relation_author_properties[TEST_RELATION_AUTHOR_N_PROPS];
static GParamSpec *test_relation_book_properties[TEST_RELATION_BOOK_N_PROPS];
static GParamSpec *test_relation_tag_properties[TEST_RELATION_TAG_N_PROPS];

static GType test_relation_author_get_type (void) G_GNUC_CONST;
static GType test_relation_book_get_type   (void) G_GNUC_CONST;
static GType test_relation_tag_get_type    (void) G_GNUC_CONST;

#define TEST_RELATION_AUTHOR_TYPE (test_relation_author_get_type ())
#define TEST_RELATION_BOOK_TYPE (test_relation_book_get_type ())
#define TEST_RELATION_TAG_TYPE (test_relation_tag_get_type ())

G_DEFINE_TYPE (TestRelationAuthor, test_relation_author, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestRelationBook, test_relation_book, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestRelationTag, test_relation_tag, GOM_TYPE_ENTITY)

static void
test_relation_author_finalize (GObject *object)
{
  TestRelationAuthor *self = (TestRelationAuthor *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (test_relation_author_parent_class)->finalize (object);
}

static void
test_relation_author_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  TestRelationAuthor *self = (TestRelationAuthor *)object;

  switch (prop_id)
    {
    case TEST_RELATION_AUTHOR_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_RELATION_AUTHOR_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_relation_author_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  TestRelationAuthor *self = (TestRelationAuthor *)object;

  switch (prop_id)
    {
    case TEST_RELATION_AUTHOR_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_RELATION_AUTHOR_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_relation_author_class_init (TestRelationAuthorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_relation_author_finalize;
  object_class->get_property = test_relation_author_get_property;
  object_class->set_property = test_relation_author_set_property;

  test_relation_author_properties[TEST_RELATION_AUTHOR_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_relation_author_properties[TEST_RELATION_AUTHOR_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_RELATION_AUTHOR_N_PROPS,
                                     test_relation_author_properties);

  gom_entity_class_set_relation (entity_class, "authors");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "name", TRUE);
}

static void
test_relation_author_init (TestRelationAuthor *self)
{
}

static void
test_relation_book_finalize (GObject *object)
{
  TestRelationBook *self = (TestRelationBook *)object;

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (test_relation_book_parent_class)->finalize (object);
}

static void
test_relation_book_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TestRelationBook *self = (TestRelationBook *)object;

  switch (prop_id)
    {
    case TEST_RELATION_BOOK_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_RELATION_BOOK_PROP_AUTHOR_ID:
      g_value_set_int64 (value, self->author_id);
      break;

    case TEST_RELATION_BOOK_PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_relation_book_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TestRelationBook *self = (TestRelationBook *)object;

  switch (prop_id)
    {
    case TEST_RELATION_BOOK_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_RELATION_BOOK_PROP_AUTHOR_ID:
      self->author_id = g_value_get_int64 (value);
      break;

    case TEST_RELATION_BOOK_PROP_TITLE:
      g_set_str (&self->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_relation_book_class_init (TestRelationBookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_relation_book_finalize;
  object_class->get_property = test_relation_book_get_property;
  object_class->set_property = test_relation_book_set_property;

  test_relation_book_properties[TEST_RELATION_BOOK_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_relation_book_properties[TEST_RELATION_BOOK_PROP_AUTHOR_ID] =
    g_param_spec_int64 ("author-id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_relation_book_properties[TEST_RELATION_BOOK_PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_RELATION_BOOK_N_PROPS,
                                     test_relation_book_properties);

  gom_entity_class_set_relation (entity_class, "books");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "author-id", TRUE);
  gom_entity_class_property_set_field_name (entity_class, "author-id", "author_id");
  gom_entity_class_property_set_reference (entity_class, "author-id", "books", "author_id");
  gom_entity_class_property_set_mapped (entity_class, "title", TRUE);
}

static void
test_relation_book_init (TestRelationBook *self)
{
}

static void
test_relation_tag_finalize (GObject *object)
{
  TestRelationTag *self = (TestRelationTag *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (test_relation_tag_parent_class)->finalize (object);
}

static void
test_relation_tag_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  TestRelationTag *self = (TestRelationTag *)object;

  switch (prop_id)
    {
    case TEST_RELATION_TAG_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_RELATION_TAG_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_relation_tag_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  TestRelationTag *self = (TestRelationTag *)object;

  switch (prop_id)
    {
    case TEST_RELATION_TAG_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_RELATION_TAG_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_relation_tag_class_init (TestRelationTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_relation_tag_finalize;
  object_class->get_property = test_relation_tag_get_property;
  object_class->set_property = test_relation_tag_set_property;

  test_relation_tag_properties[TEST_RELATION_TAG_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_relation_tag_properties[TEST_RELATION_TAG_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_RELATION_TAG_N_PROPS,
                                     test_relation_tag_properties);

  gom_entity_class_set_relation (entity_class, "tags");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "name", TRUE);
}

static void
test_relation_tag_init (TestRelationTag *self)
{
}

static GomRegistry *
test_relations_create_registry_with_book_author_delete_rule (GomRelationshipDeleteRule delete_rule)
{
  g_autoptr(GomRegistryBuilder) builder = NULL;
  GomEntityClass *author_class;
  GomEntityClass *book_class;
  GomEntityClass *tag_class;

  builder = gom_registry_builder_new ();
  gom_registry_builder_add_entity_type (builder, TEST_RELATION_AUTHOR_TYPE);
  gom_registry_builder_add_entity_type (builder, TEST_RELATION_BOOK_TYPE);
  gom_registry_builder_add_entity_type (builder, TEST_RELATION_TAG_TYPE);

  author_class = g_type_class_ref (TEST_RELATION_AUTHOR_TYPE);
  book_class = g_type_class_ref (TEST_RELATION_BOOK_TYPE);
  tag_class = g_type_class_ref (TEST_RELATION_TAG_TYPE);

  gom_entity_class_add_one_to_many (author_class, "books", TEST_RELATION_BOOK_TYPE, "author_id", "author");
  gom_entity_class_add_many_to_one (book_class, "author", TEST_RELATION_AUTHOR_TYPE, "author_id", "books");
  gom_entity_class_relationship_set_delete_rule (book_class, "author", delete_rule);
  gom_entity_class_add_many_to_many (book_class,
                                     "tags",
                                     TEST_RELATION_TAG_TYPE,
                                     "book_tags",
                                     "book_id",
                                     "tag_id",
                                     "books");
  gom_entity_class_add_many_to_many (tag_class,
                                     "books",
                                     TEST_RELATION_BOOK_TYPE,
                                     "book_tags",
                                     "tag_id",
                                     "book_id",
                                     "tags");

  g_type_class_unref (tag_class);
  g_type_class_unref (book_class);
  g_type_class_unref (author_class);

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_relations_get_cached_registry_with_delete_rule (GomRelationshipDeleteRule delete_rule)
{
  static gsize initialized_registry[GOM_RELATIONSHIP_DELETE_DENY + 1] = {0};

  g_assert_cmpint (delete_rule, >=, GOM_RELATIONSHIP_DELETE_NO_ACTION);
  g_assert_cmpint (delete_rule, <=, GOM_RELATIONSHIP_DELETE_DENY);

  if (g_once_init_enter (&initialized_registry[delete_rule]))
    {
      g_autoptr(GomRegistry) registry = NULL;

      registry = test_relations_create_registry_with_book_author_delete_rule (delete_rule);
      g_once_init_leave (&initialized_registry[delete_rule], (gsize) g_steal_pointer (&registry));
    }

  return g_object_ref (GSIZE_TO_POINTER (initialized_registry[delete_rule]));
}

static GomRegistry *
test_relations_create_registry (void)
{
  return test_relations_get_cached_registry_with_delete_rule (GOM_RELATIONSHIP_DELETE_NULLIFY);
}

static gint64 test_sqlite_query_int64 (sqlite3    *db,
                                       const char *sql);

typedef void (*TestRelationsDeleteRulePostAssert) (const char *db_path);

static void
test_relations_delete_rule_nullify_assert (const char *db_path)
{
  sqlite3 *db = NULL;

  test_sqlite_open (db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db,
                                            "SELECT author_id IS NULL FROM books WHERE id = 10"),
                   ==, 1);
  test_sqlite_close (db);
}

static void
test_relations_delete_rule_cascade_assert (const char *db_path)
{
  sqlite3 *db = NULL;

  test_sqlite_open (db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM books"), ==, 0);
  test_sqlite_close (db);
}

static void
test_relations_delete_rule_deny_assert (const char *db_path)
{
  sqlite3 *db = NULL;

  test_sqlite_open (db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM authors"), ==, 1);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM books"), ==, 1);
  test_sqlite_close (db);
}

static void
test_relations_delete_rule_case (const char                        *tmp_pattern,
                                 const char                        *create_sql,
                                 GomRelationshipDeleteRule          delete_rule,
                                 gboolean                           expect_failure,
                                 TestRelationsDeleteRulePostAssert  post_assert)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GObject) authors = NULL;
  g_autoptr(GomEntity) author = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, tmp_pattern, &error));
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db, create_sql);
  test_sqlite_close (db);

  registry = test_relations_get_cached_registry_with_delete_rule (delete_rule);
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  authors = dex_await_object (gom_repository_list_entities (repository,
                                                            TEST_RELATION_AUTHOR_TYPE,
                                                            gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (1)),
                                                            NULL),
                              &error);
  g_assert_no_error (error);
  g_assert_nonnull (authors);
  author = g_list_model_get_item (G_LIST_MODEL (authors), 0);
  g_assert_nonnull (author);

  if (expect_failure)
    {
      g_assert_false (dex_await (gom_entity_delete (author), &error));
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
      g_clear_error (&error);
    }
  else if (!dex_await (gom_entity_delete (author), &error))
    {
      g_error ("Delete failed: %s", error->message);
    }
  else
    {
      g_assert_no_error (error);
    }

  g_assert_nonnull (post_assert);
  post_assert (context.db_path);
}

static void
test_relation_session_changed_cb (GomSession *session,
                                  gpointer    user_data)
{
  guint *changed_count = user_data;

  g_assert_true (GOM_IS_SESSION (session));
  (*changed_count)++;
}

static void
test_query_model_items_changed_cb (GListModel *model,
                                   guint       position,
                                   guint       removed,
                                   guint       added,
                                   gpointer    user_data)
{
  guint *changed_count = user_data;

  g_assert_true (G_IS_LIST_MODEL (model));
  g_assert_cmpuint (position, >=, 0);
  g_assert_cmpuint (removed, >=, 0);
  g_assert_cmpuint (added, >=, 0);

  (*changed_count)++;
}

static gint64
test_sqlite_query_int64 (sqlite3    *db,
                         const char *sql)
{
  sqlite3_stmt *stmt = NULL;
  int rc;
  gint64 value;

  g_assert_nonnull (db);
  g_assert_nonnull (sql);

  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  value = sqlite3_column_int64 (stmt, 0);
  sqlite3_finalize (stmt);

  return value;
}

static void
test_relations_delete_rule_nullify (void)
{
  test_relations_delete_rule_case ("gom-relations-delete-nullify-XXXXXX",
                                  "CREATE TABLE authors ("
                                  "  id INTEGER PRIMARY KEY,"
                                  "  name TEXT NOT NULL"
                                  ");"
                                  "CREATE TABLE books ("
                                  "  id INTEGER PRIMARY KEY,"
                                  "  author_id INTEGER,"
                                  "  title TEXT NOT NULL"
                                  ");"
                                  "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                                  "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');",
                                  GOM_RELATIONSHIP_DELETE_NULLIFY,
                                  FALSE,
                                  test_relations_delete_rule_nullify_assert);
}

static void
test_relations_delete_rule_cascade (void)
{
  test_relations_delete_rule_case ("gom-relations-delete-cascade-XXXXXX",
                                  "CREATE TABLE authors ("
                                  "  id INTEGER PRIMARY KEY,"
                                  "  name TEXT NOT NULL"
                                  ");"
                                  "CREATE TABLE books ("
                                  "  id INTEGER PRIMARY KEY,"
                                  "  author_id INTEGER NOT NULL,"
                                  "  title TEXT NOT NULL"
                                  ");"
                                  "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                                  "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');",
                                  GOM_RELATIONSHIP_DELETE_CASCADE,
                                  FALSE,
                                  test_relations_delete_rule_cascade_assert);
}

static void
test_relations_delete_rule_deny (void)
{
  test_relations_delete_rule_case ("gom-relations-delete-deny-XXXXXX",
                                  "CREATE TABLE authors ("
                                  "  id INTEGER PRIMARY KEY,"
                                  "  name TEXT NOT NULL"
                                  ");"
                                  "CREATE TABLE books ("
                                  "  id INTEGER PRIMARY KEY,"
                                  "  author_id INTEGER NOT NULL,"
                                  "  title TEXT NOT NULL"
                                  ");"
                                  "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                                  "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');",
                                  GOM_RELATIONSHIP_DELETE_DENY,
                                  TRUE,
                                  test_relations_delete_rule_deny_assert);
}

static void
test_relations_insert_rejects_missing_foreign_key_target (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) book = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-insert-missing-target-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  book = g_object_new (TEST_RELATION_BOOK_TYPE,
                       "id", (gint64) 20,
                       "author-id", (gint64) 99,
                       "title", "Broken",
                       NULL);
  gom_entity_set_repository (book, repository);

  g_assert_false (dex_await (gom_entity_insert (book), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);

  test_sqlite_open (context.db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM books"), ==, 0);
  test_sqlite_close (db);

}

static void
test_relations_update_rejects_missing_foreign_key_target (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GObject) books = NULL;
  g_autoptr(GomEntity) book = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-update-missing-target-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  books = dex_await_object (gom_repository_list_entities (repository,
                                                          TEST_RELATION_BOOK_TYPE,
                                                          gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (10)),
                                                          NULL),
                            &error);
  g_assert_no_error (error);
  g_assert_nonnull (books);
  book = g_list_model_get_item (G_LIST_MODEL (books), 0);
  g_assert_nonnull (book);

  g_object_set (book,
                "author-id", (gint64) 99,
                NULL);
  g_assert_false (dex_await (gom_entity_update (book), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);

  test_sqlite_open (context.db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT author_id FROM books WHERE id = 10"), ==, 1);
  test_sqlite_close (db);

}

static void
test_relations_repository_load (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) author = NULL;
  g_autoptr(GomEntity) book = NULL;
  g_autoptr(GomRelatedModel) related_books = NULL;
  g_autoptr(GomRelatedModel) related_tags = NULL;
  g_autoptr(GObject) authors = NULL;
  g_autoptr(GObject) books = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;
  guint n_books = 0;
  guint n_tags = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-test-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES "
                       "  (10, 1, 'First'),"
                       "  (11, 1, 'Second');"
                       "INSERT INTO tags (id, name) VALUES "
                       "  (100, 'gtk'),"
                       "  (101, 'sqlite');"
                       "INSERT INTO book_tags (book_id, tag_id) VALUES "
                       "  (10, 100),"
                       "  (10, 101);");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  authors = dex_await_object (gom_repository_list_entities (repository,
                                                            TEST_RELATION_AUTHOR_TYPE,
                                                            gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (1)),
                                                            NULL),
                              &error);
  g_assert_no_error (error);
  g_assert_nonnull (authors);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (authors)), ==, 1);
  author = g_list_model_get_item (G_LIST_MODEL (authors), 0);
  g_assert_nonnull (author);

  books = dex_await_object (gom_repository_list_entities (repository,
                                                          TEST_RELATION_BOOK_TYPE,
                                                          gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (10)),
                                                          NULL),
                            &error);
  g_assert_no_error (error);
  g_assert_nonnull (books);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (books)), ==, 1);
  book = g_list_model_get_item (G_LIST_MODEL (books), 0);
  g_assert_nonnull (book);

  {
    g_autoptr(GomEntity) loaded_author = NULL;

    loaded_author = dex_await_object (gom_entity_load_related_entity (book, "author"), &error);
    g_assert_no_error (error);
    g_assert_nonnull (loaded_author);
    g_assert_true (TEST_RELATION_AUTHOR_TYPE == G_OBJECT_TYPE (loaded_author));
    g_assert_cmpint (((TestRelationAuthor *) loaded_author)->id, ==, 1);
  }

  related_books = dex_await_object (gom_entity_load_related_model (author, "books"), &error);
  g_assert_nonnull (related_books);

  g_assert_no_error (error);
  g_assert_false (gom_related_model_get_loading (related_books));

  n_books = g_list_model_get_n_items (G_LIST_MODEL (related_books));
  g_assert_cmpuint (n_books, ==, 2);

  for (guint i = 0; i < n_books; i++)
    {
      g_autoptr(GomEntity) related_book = g_list_model_get_item (G_LIST_MODEL (related_books), i);
      TestRelationBook *typed_book = (TestRelationBook *) related_book;

      g_assert_nonnull (related_book);
      g_assert_cmpint (typed_book->author_id, ==, 1);
      g_assert_nonnull (typed_book->title);
    }

  related_tags = dex_await_object (gom_entity_load_related_model (book, "tags"), &error);
  g_assert_nonnull (related_tags);

  g_assert_no_error (error);
  g_assert_false (gom_related_model_get_loading (related_tags));

  n_tags = g_list_model_get_n_items (G_LIST_MODEL (related_tags));
  g_assert_cmpuint (n_tags, ==, 2);

  {
    gboolean saw_gtk = FALSE;
    gboolean saw_sqlite = FALSE;

    for (guint i = 0; i < n_tags; i++)
      {
        g_autoptr(GomEntity) related_tag = g_list_model_get_item (G_LIST_MODEL (related_tags), i);
        TestRelationTag *typed_tag = (TestRelationTag *) related_tag;

        g_assert_nonnull (related_tag);
        g_assert_true (TEST_RELATION_TAG_TYPE == G_OBJECT_TYPE (related_tag));
        g_assert_nonnull (typed_tag->name);

        if (typed_tag->id == 100)
          saw_gtk = TRUE;
        else if (typed_tag->id == 101)
          saw_sqlite = TRUE;
        else
          g_assert_not_reached ();
      }

    g_assert_true (saw_gtk);
    g_assert_true (saw_sqlite);
  }

}

static void
test_relations_session_flush_relationship_change (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GObject) books = NULL;
  g_autoptr(GObject) authors = NULL;
  g_autoptr(GomEntity) book = NULL;
  g_autoptr(GomEntity) author1 = NULL;
  g_autoptr(GomEntity) author2 = NULL;
  g_autoptr(GomRelatedModel) related_books = NULL;
  g_autoptr(GomRelatedModel) related_books_for_new_author = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;
  guint session_changed_count = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-session-flush-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES "
                       "  (1, 'Ada'),"
                       "  (2, 'Bea');"
                       "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);
  g_signal_connect (session,
                    "changed",
                    G_CALLBACK (test_relation_session_changed_cb),
                    &session_changed_count);

  authors = dex_await_object (gom_session_list_entities (session,
                                                         TEST_RELATION_AUTHOR_TYPE,
                                                         gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (1)),
                                                         NULL),
                              &error);
  g_assert_no_error (error);
  g_assert_nonnull (authors);
  author1 = g_list_model_get_item (G_LIST_MODEL (authors), 0);
  g_assert_nonnull (author1);

  authors = dex_await_object (gom_session_list_entities (session,
                                                         TEST_RELATION_AUTHOR_TYPE,
                                                         gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (2)),
                                                         NULL),
                              &error);
  g_assert_no_error (error);
  g_assert_nonnull (authors);
  author2 = g_list_model_get_item (G_LIST_MODEL (authors), 0);
  g_assert_nonnull (author2);

  books = dex_await_object (gom_session_list_entities (session,
                                                       TEST_RELATION_BOOK_TYPE,
                                                       gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (10)),
                                                       NULL),
                            &error);
  g_assert_no_error (error);
  g_assert_nonnull (books);
  book = g_list_model_get_item (G_LIST_MODEL (books), 0);
  g_assert_nonnull (book);

  related_books = dex_await_object (gom_entity_load_related_model (author1, "books"), &error);
  g_assert_no_error (error);
  g_assert_nonnull (related_books);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (related_books)), ==, 1);

  related_books_for_new_author = dex_await_object (gom_entity_load_related_model (author2,
                                                                                  "books"),
                                                   &error);
  g_assert_no_error (error);
  g_assert_nonnull (related_books_for_new_author);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (related_books_for_new_author)), ==, 0);

  g_object_set (book,
                "author-id", (gint64) 2,
                NULL);
  dex_await (gom_session_flush (session), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (session_changed_count, >, 0);

}

static void
test_relations_query_model_refreshes_on_session_change (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryModel) model = NULL;
  g_autoptr(GObject) books = NULL;
  g_autoptr(GomEntity) book = NULL;
  guint *items_changed_count = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;
  guint initial_items_changed_count = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-query-model-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES "
                       "  (1, 'Ada'),"
                       "  (2, 'Bea');"
                       "INSERT INTO books (id, author_id, title) VALUES "
                       "  (10, 1, 'First'),"
                       "  (11, 1, 'Second');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  model = gom_query_model_new (session,
                               TEST_RELATION_BOOK_TYPE,
                               gom_binary_expression_new_equal (gom_field_expression_new ("author-id"),
                                                                gom_literal_expression_new_int64 (1)),
                               gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING));
  g_assert_nonnull (model);
  items_changed_count = g_new0 (guint, 1);
  g_signal_connect_data (model,
                         "items-changed",
                         G_CALLBACK (test_query_model_items_changed_cb),
                         items_changed_count,
                         (GClosureNotify) g_free,
                         0);

  dex_await (gom_query_model_reload (model), &error);
  g_assert_no_error (error);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 2);
  initial_items_changed_count = *items_changed_count;

  books = dex_await_object (gom_session_list_entities (session,
                                                       TEST_RELATION_BOOK_TYPE,
                                                       gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (10)),
                                                       NULL),
                            &error);
  g_assert_no_error (error);
  g_assert_nonnull (books);
  book = g_list_model_get_item (G_LIST_MODEL (books), 0);
  g_assert_nonnull (book);

  g_object_set (book,
                "author-id", (gint64) 2,
                NULL);
  dex_await (gom_session_flush (session), &error);
  g_assert_no_error (error);

  dex_await (gom_query_model_refresh (model), &error);
  g_assert_no_error (error);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 1);
  g_assert_cmpuint (*items_changed_count, >, initial_items_changed_count);

}

static void
test_relation_wait_for_item_load (GomEntityListItem *item)
{
  for (guint i = 0; i < 200 && gom_entity_list_item_get_loading (item); i++)
    g_main_context_iteration (NULL, TRUE);
}

static void
test_relations_entity_list_model_query_validation (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GomEntityListModel) model = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-entity-list-validation-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, "books");
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  future = gom_session_list_query (session, query);
  g_assert_nonnull (future);
  model = dex_await_object (g_steal_pointer (&future), &error);
  g_assert_null (model);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  gom_query_builder_set_target_relation (builder, NULL);
  gom_query_builder_set_target_entity_type (builder, TEST_RELATION_BOOK_TYPE);
  gom_query_builder_add_projection (builder, gom_field_expression_new ("title"));
  g_clear_object (&query);
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  future = gom_session_list_query (session, query);
  g_assert_nonnull (future);
  model = dex_await_object (g_steal_pointer (&future), &error);
  g_assert_null (model);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);

}

static void
test_relations_entity_list_model_lazy_loading (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GomEntityListModel) model = NULL;
  g_autoptr(GomEntityListItem) first = NULL;
  g_autoptr(GomEntityListItem) second = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-entity-list-lazy-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES "
                       "  (10, 1, 'First'),"
                       "  (11, 1, 'Second');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, TEST_RELATION_BOOK_TYPE);
  gom_query_builder_set_limit (builder, 1);
  gom_query_builder_add_ordering (builder, gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING));
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  future = gom_session_list_query (session, query);
  model = dex_await_object (g_steal_pointer (&future), &error);
  g_assert_no_error (error);
  g_assert_nonnull (model);

  dex_await (gom_entity_list_model_reload (model), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 1);

  first = g_list_model_get_item (G_LIST_MODEL (model), 0);
  second = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (first);
  g_assert_true (first == second);
  g_assert_cmpuint (gom_entity_list_item_get_position (first), ==, 0);

  if (gom_entity_list_item_dup_item (first) == NULL)
    test_relation_wait_for_item_load (first);

  g_assert_nonnull (gom_entity_list_item_dup_item (first));
  g_assert_false (gom_entity_list_item_get_loading (first));

}

static void
test_relations_entity_list_model_refreshes_snapshot (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GomEntityListModel) model = NULL;
  g_autoptr(GomEntityListItem) first = NULL;
  g_autoptr(GomEntityListItem) second = NULL;
  g_autoptr(GomEntityListItem) refreshed_first = NULL;
  g_autoptr(GomEntity) deleted_book = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-entity-list-refresh-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES "
                       "  (10, 1, 'First'),"
                       "  (11, 1, 'Second');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, TEST_RELATION_BOOK_TYPE);
  gom_query_builder_add_ordering (builder, gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING));
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  future = gom_session_list_query (session, query);
  model = dex_await_object (g_steal_pointer (&future), &error);
  g_assert_no_error (error);
  g_assert_nonnull (model);

  dex_await (gom_entity_list_model_reload (model), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 2);

  first = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (first);
  if (gom_entity_list_item_dup_item (first) == NULL)
    test_relation_wait_for_item_load (first);
  g_assert_nonnull (gom_entity_list_item_dup_item (first));

  second = g_list_model_get_item (G_LIST_MODEL (model), 1);
  g_assert_nonnull (second);
  deleted_book = gom_entity_list_item_dup_item (second);
  if (deleted_book == NULL)
    test_relation_wait_for_item_load (second);
  deleted_book = gom_entity_list_item_dup_item (second);
  g_assert_nonnull (deleted_book);

  g_assert_true (dex_await (gom_entity_delete (deleted_book), &error));
  g_assert_no_error (error);
  g_assert_true (dex_await (gom_session_flush (session), &error));
  g_assert_no_error (error);
  dex_await (gom_entity_list_model_refresh (model), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 1);

  refreshed_first = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (refreshed_first);
  g_assert_true (first != refreshed_first);

}

static void
test_relations_entity_list_model_wrapper_outlives_model (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GomEntityListModel) model = NULL;
  g_autoptr(GomEntityListItem) wrapper = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-entity-list-wrapper-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (session);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, TEST_RELATION_BOOK_TYPE);
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  future = gom_session_list_query (session, query);
  model = dex_await_object (g_steal_pointer (&future), &error);
  g_assert_no_error (error);
  g_assert_nonnull (model);

  dex_await (gom_entity_list_model_reload (model), &error);
  g_assert_no_error (error);
  wrapper = g_list_model_get_item (G_LIST_MODEL (model), 0);
  g_assert_nonnull (wrapper);
  if (gom_entity_list_item_dup_item (wrapper) == NULL)
    test_relation_wait_for_item_load (wrapper);
  g_assert_nonnull (gom_entity_list_item_dup_item (wrapper));

  g_clear_object (&model);
  g_assert_nonnull (gom_entity_list_item_dup_item (wrapper));

}

static void
test_relations_delete_join_table_book (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GObject) books = NULL;
  g_autoptr(GomEntity) book = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-delete-join-book-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');"
                       "INSERT INTO tags (id, name) VALUES (100, 'gtk'), (101, 'sqlite');"
                       "INSERT INTO book_tags (book_id, tag_id) VALUES (10, 100), (10, 101);");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  books = dex_await_object (gom_repository_list_entities (repository,
                                                          TEST_RELATION_BOOK_TYPE,
                                                          gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (10)),
                                                          NULL),
                            &error);
  g_assert_no_error (error);
  g_assert_nonnull (books);
  book = g_list_model_get_item (G_LIST_MODEL (books), 0);
  g_assert_nonnull (book);

  if (!dex_await (gom_entity_delete (book), &error))
    g_error ("Join-table delete failed: %s", error->message);
  g_assert_no_error (error);

  test_sqlite_open (context.db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM book_tags"), ==, 0);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM books"), ==, 0);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM tags"), ==, 2);
  test_sqlite_close (db);

}

static void
test_relations_delete_join_table_tag (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GObject) tags = NULL;
  g_autoptr(GomEntity) tag = NULL;
  g_auto(TestSqliteContext) context = {0};
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-relations-delete-join-tag-XXXXXX", &error));

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE authors ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE books ("
                       "  id INTEGER PRIMARY KEY,"
                       "  author_id INTEGER NOT NULL,"
                       "  title TEXT NOT NULL"
                       ");"
                       "CREATE TABLE tags ("
                       "  id INTEGER PRIMARY KEY,"
                       "  name TEXT NOT NULL"
                       ");"
                       "CREATE TABLE book_tags ("
                       "  book_id INTEGER NOT NULL,"
                       "  tag_id INTEGER NOT NULL"
                       ");"
                       "INSERT INTO authors (id, name) VALUES (1, 'Ada');"
                       "INSERT INTO books (id, author_id, title) VALUES (10, 1, 'First');"
                       "INSERT INTO tags (id, name) VALUES (100, 'gtk'), (101, 'sqlite');"
                       "INSERT INTO book_tags (book_id, tag_id) VALUES (10, 100), (10, 101);");
  test_sqlite_close (db);

  registry = test_relations_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);

  tags = dex_await_object (gom_repository_list_entities (repository,
                                                         TEST_RELATION_TAG_TYPE,
                                                         gom_binary_expression_new_equal (gom_field_expression_new ("id"), gom_literal_expression_new_int64 (100)),
                                                         NULL),
                           &error);
  g_assert_no_error (error);
  g_assert_nonnull (tags);
  tag = g_list_model_get_item (G_LIST_MODEL (tags), 0);
  g_assert_nonnull (tag);

  if (!dex_await (gom_entity_delete (tag), &error))
    g_error ("Join-table delete failed: %s", error->message);
  g_assert_no_error (error);

  test_sqlite_open (context.db_path, &db);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM book_tags"), ==, 1);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM tags"), ==, 1);
  g_assert_cmpint (test_sqlite_query_int64 (db, "SELECT COUNT(*) FROM books"), ==, 1);
  test_sqlite_close (db);

}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Sqlite/relations-load", test_relations_repository_load);
  _g_test_add_func ("/Gom/Sqlite/relations-session-flush-relationship-change", test_relations_session_flush_relationship_change);
  _g_test_add_func ("/Gom/Sqlite/relations-query-model-refreshes-on-session-change", test_relations_query_model_refreshes_on_session_change);
  _g_test_add_func ("/Gom/Sqlite/entity-list-model/query-validation", test_relations_entity_list_model_query_validation);
  _g_test_add_func ("/Gom/Sqlite/entity-list-model/lazy-loading", test_relations_entity_list_model_lazy_loading);
  _g_test_add_func ("/Gom/Sqlite/entity-list-model/refreshes-snapshot", test_relations_entity_list_model_refreshes_snapshot);
  _g_test_add_func ("/Gom/Sqlite/entity-list-model/wrapper-outlives-model", test_relations_entity_list_model_wrapper_outlives_model);
  _g_test_add_func ("/Gom/Sqlite/relations-delete-rule-nullify", test_relations_delete_rule_nullify);
  _g_test_add_func ("/Gom/Sqlite/relations-delete-rule-cascade", test_relations_delete_rule_cascade);
  _g_test_add_func ("/Gom/Sqlite/relations-delete-rule-deny", test_relations_delete_rule_deny);
  _g_test_add_func ("/Gom/Sqlite/relations-insert-missing-target", test_relations_insert_rejects_missing_foreign_key_target);
  _g_test_add_func ("/Gom/Sqlite/relations-update-missing-target", test_relations_update_rejects_missing_foreign_key_target);
  _g_test_add_func ("/Gom/Sqlite/relations-delete-join-table-book", test_relations_delete_join_table_book);
  _g_test_add_func ("/Gom/Sqlite/relations-delete-join-table-tag", test_relations_delete_join_table_tag);

  return g_test_run ();
}
