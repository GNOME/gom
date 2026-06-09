/* test-gom-migrator.c
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
#include <string.h>

#include <libgom.h>

#include "test-util.h"
#include "gom-mock-driver-private.h"

typedef struct _TestEntityMigratorItem      TestEntityMigratorItem;
typedef struct _TestEntityMigratorItemClass TestEntityMigratorItemClass;

struct _TestEntityMigratorItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
};

struct _TestEntityMigratorItemClass
{
  GomEntityClass parent_class;
};

enum {
  TEST_ENTITY_MIGRATOR_ITEM_PROP_0,
  TEST_ENTITY_MIGRATOR_ITEM_PROP_ID,
  TEST_ENTITY_MIGRATOR_ITEM_PROP_NAME,
  TEST_ENTITY_MIGRATOR_ITEM_N_PROPS
};

static GParamSpec *test_entity_migrator_item_properties[TEST_ENTITY_MIGRATOR_ITEM_N_PROPS];

static GType test_entity_migrator_item_get_type (void);
G_DEFINE_TYPE (TestEntityMigratorItem, test_entity_migrator_item, GOM_TYPE_ENTITY)

static void
test_entity_migrator_item_finalize (GObject *object)
{
  TestEntityMigratorItem *self = (TestEntityMigratorItem *) object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (test_entity_migrator_item_parent_class)->finalize (object);
}

static void
test_entity_migrator_item_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  TestEntityMigratorItem *self = (TestEntityMigratorItem *) object;

  switch (prop_id)
    {
    case TEST_ENTITY_MIGRATOR_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_ENTITY_MIGRATOR_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_migrator_item_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  TestEntityMigratorItem *self = (TestEntityMigratorItem *) object;

  switch (prop_id)
    {
    case TEST_ENTITY_MIGRATOR_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_ENTITY_MIGRATOR_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_entity_migrator_item_class_init (TestEntityMigratorItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_entity_migrator_item_finalize;
  object_class->get_property = test_entity_migrator_item_get_property;
  object_class->set_property = test_entity_migrator_item_set_property;

  test_entity_migrator_item_properties[TEST_ENTITY_MIGRATOR_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_entity_migrator_item_properties[TEST_ENTITY_MIGRATOR_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_ENTITY_MIGRATOR_ITEM_N_PROPS,
                                     test_entity_migrator_item_properties);

  gom_entity_class_set_relation (entity_class, "entity_migrator_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "name", 1);
  gom_entity_class_property_set_nonnull (entity_class, "name", TRUE);
}

static void
test_entity_migrator_item_init (TestEntityMigratorItem *self)
{
}

static GomRegistry *
test_entity_migrator_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_entity_migrator_item_get_type ());

  return gom_registry_builder_build (builder);
}

static DexFuture *
apply_callback_called (GomDriver *driver,
                       gpointer   user_data)
{
  gboolean *called = (gboolean *) user_data;
  *called = TRUE;
  return dex_future_new_true ();
}

static void
test_custom_migration_callback_called (void)
{
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomCustomMigrator) migrator = NULL;
  g_autoptr(GError) error = NULL;
  gboolean apply_called = FALSE;
  gboolean r;

  driver = _gom_mock_driver_new ();
  g_assert_true (GOM_IS_MOCK_DRIVER (driver));

  migrator = gom_custom_migrator_new (0);
  g_assert_true (GOM_IS_CUSTOM_MIGRATOR (migrator));

  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (1,
                                                               apply_callback_called,
                                                               &apply_called,
                                                               NULL));

  r = dex_await (gom_migrator_update (GOM_MIGRATOR (migrator), GOM_DRIVER (driver)), &error);
  g_assert_no_error (error);
  g_assert_true (r);
  g_assert_true (apply_called);
}

static gint apply_order[4];
static guint apply_order_len;

static DexFuture *
apply_record_order_1 (GomDriver *driver,
                      gpointer   user_data)
{
  g_assert_true (apply_order_len < G_N_ELEMENTS (apply_order));
  apply_order[apply_order_len++] = 1;
  return dex_future_new_true ();
}

static DexFuture *
apply_record_order_2 (GomDriver *driver,
                      gpointer   user_data)
{
  g_assert_true (apply_order_len < G_N_ELEMENTS (apply_order));
  apply_order[apply_order_len++] = 2;
  return dex_future_new_true ();
}

static DexFuture *
apply_record_order_3 (GomDriver *driver,
                      gpointer   user_data)
{
  g_assert_true (apply_order_len < G_N_ELEMENTS (apply_order));
  apply_order[apply_order_len++] = 3;
  return dex_future_new_true ();
}

static void
test_add_migration_applied_in_sorted_order (void)
{
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomCustomMigrator) migrator = NULL;
  g_autoptr(GError) error = NULL;
  gboolean r;

  driver = _gom_mock_driver_new ();
  migrator = gom_custom_migrator_new (0);

  /* Add migrations out of order to test they run in order */
  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (3, apply_record_order_3, NULL, NULL));
  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (1, apply_record_order_1, NULL, NULL));
  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (2, apply_record_order_2, NULL, NULL));

  r = dex_await (gom_migrator_update (GOM_MIGRATOR (migrator), GOM_DRIVER (driver)), &error);
  g_assert_no_error (error);
  g_assert_true (r);

  g_assert_cmpuint (apply_order_len, ==, 3);
  g_assert_cmpint (apply_order[0], ==, 1);
  g_assert_cmpint (apply_order[1], ==, 2);
  g_assert_cmpint (apply_order[2], ==, 3);
}

static void
test_migrations_above_version_only (void)
{
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomCustomMigrator) migrator = NULL;
  g_autoptr(GError) error = NULL;
  gboolean v1_called = FALSE;
  gboolean v2_called = FALSE;
  gboolean r;

  driver = _gom_mock_driver_new ();
  /* Current version is 1, so only migration 2 should run */
  migrator = gom_custom_migrator_new (1);

  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (1, apply_callback_called, &v1_called, NULL));
  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (2, apply_callback_called, &v2_called, NULL));

  r = dex_await (gom_migrator_update (GOM_MIGRATOR (migrator), GOM_DRIVER (driver)), &error);
  g_assert_no_error (error);
  g_assert_true (r);

  g_assert_false (v1_called);
  g_assert_true (v2_called);
}

static void
test_repository_new_runs_custom_migrator (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomCustomMigrator) migrator = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GError) error = NULL;
  gboolean apply_called = FALSE;
  g_assert_true (test_sqlite_context_init (&context, "gom-migrator-repository-test-XXXXXX", &error));
  g_assert_no_error (error);

  migrator = gom_custom_migrator_new (0);
  g_assert_true (GOM_IS_CUSTOM_MIGRATOR (migrator));

  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (1,
                                                               apply_callback_called,
                                                               &apply_called,
                                                               NULL));

  repository = dex_await_object (gom_repository_new (GOM_DRIVER (context.driver),
                                                     NULL,
                                                     GOM_MIGRATOR (migrator)),
                                 &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  g_assert_true (apply_called);
}

static void
test_entity_migrator_updates_from_registry (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntityMigrator) migrator = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gboolean r;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-entity-migrator-test-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_entity_migrator_create_registry ();
  g_assert_true (GOM_IS_REGISTRY (registry));

  migrator = gom_entity_migrator_new (registry);
  g_assert_true (GOM_IS_ENTITY_MIGRATOR (migrator));

  r = dex_await (gom_migrator_update (GOM_MIGRATOR (migrator),
                                      GOM_DRIVER (context.driver)),
                 &error);
  g_assert_no_error (error);
  g_assert_true (r);
  test_sqlite_open (context.db_path, &db);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT 1 FROM sqlite_master WHERE type='table' AND name='entity_migrator_items'",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  sqlite3_finalize (stmt);
  stmt = NULL;

  rc = sqlite3_prepare_v2 (db, "PRAGMA user_version", -1, &stmt, NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_int (stmt, 0), ==, 1);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;
}

static void
test_sql_migration_apply_script (void)
{
  static const char test_string[] =
    "CREATE TABLE sql_migration_items (id INTEGER PRIMARY KEY);"
    "PRAGMA user_version = 7;";
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomMigration) migration = NULL;
  g_autoptr(GBytes) script = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;
  gboolean r;

  g_assert_true (test_sqlite_context_init (&context, "gom-migrator-test-XXXXXX", &error));
  g_assert_no_error (error);

  script = g_bytes_new_static (test_string, strlen (test_string));
  migration = gom_sql_migration_new (7, script);
  g_assert_true (GOM_IS_SQL_MIGRATION (migration));

  r = dex_await (gom_migration_apply (migration, GOM_DRIVER (context.driver)), &error);
  g_assert_no_error (error);
  g_assert_true (r);
  test_sqlite_open (context.db_path, &db);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT 1 FROM sqlite_master WHERE type='table' AND name='sql_migration_items'",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  sqlite3_finalize (stmt);
  stmt = NULL;

  rc = sqlite3_prepare_v2 (db, "PRAGMA user_version", -1, &stmt, NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_int (stmt, 0), ==, 7);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  _g_test_add_func ("/Gom/CustomMigrator/callback_called", test_custom_migration_callback_called);
  _g_test_add_func ("/Gom/CustomMigrator/add_migration_sorted_order", test_add_migration_applied_in_sorted_order);
  _g_test_add_func ("/Gom/CustomMigrator/migrations_above_version_only", test_migrations_above_version_only);
  _g_test_add_func ("/Gom/Repository/new-runs-custom-migrator", test_repository_new_runs_custom_migrator);
  _g_test_add_func ("/Gom/EntityMigrator/updates-from-registry", test_entity_migrator_updates_from_registry);
  _g_test_add_func ("/Gom/SqlMigration/apply-script", test_sql_migration_apply_script);

  return g_test_run ();
}
