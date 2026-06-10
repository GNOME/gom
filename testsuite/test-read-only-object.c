/* test-read-only-object.c
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <sqlite3.h>

#include <libgom.h>

#include "test-util.h"

typedef struct _TestReadOnlyObject      TestReadOnlyObject;
typedef struct _TestReadOnlyObjectClass TestReadOnlyObjectClass;

struct _TestReadOnlyObject
{
  GomEntity  parent_instance;
  guint64    id;
  char      *name;
  GDateTime *created_at;
};

struct _TestReadOnlyObjectClass
{
  GomEntityClass parent_class;
};

enum
{
  TEST_READ_ONLY_OBJECT_PROP_0,
  TEST_READ_ONLY_OBJECT_PROP_ID,
  TEST_READ_ONLY_OBJECT_PROP_NAME,
  TEST_READ_ONLY_OBJECT_PROP_CREATED_AT,
  TEST_READ_ONLY_OBJECT_N_PROPS
};

static GParamSpec *test_read_only_object_properties[TEST_READ_ONLY_OBJECT_N_PROPS];

GType test_read_only_object_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TestReadOnlyObject, test_read_only_object, GOM_TYPE_ENTITY)

static TestReadOnlyObject *test_read_only_object_new               (const char          *name,
                                                                    GDateTime           *created_at);
static void                test_read_only_object_finalize          (GObject             *object);
static void                test_read_only_object_get_property      (GObject             *object,
                                                                    guint                prop_id,
                                                                    GValue              *value,
                                                                    GParamSpec          *pspec);
static GomEntity          *test_read_only_object_materialize       (GomEntityClass      *klass,
                                                                    GomCursor           *cursor,
                                                                    const char * const  *property_names,
                                                                    const GValue        *property_values,
                                                                    guint                n_properties,
                                                                    GError             **error);
static gboolean            test_read_only_object_backfill_identity (GomEntity           *self,
                                                                    const char * const  *identity_fields,
                                                                    GomRecord           *record,
                                                                    GError             **error);
static GDateTime          *test_read_only_object_get_datetime      (const GValue        *value);

static void
test_read_only_object_finalize (GObject *object)
{
  TestReadOnlyObject *self = (TestReadOnlyObject *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->created_at, g_date_time_unref);

  G_OBJECT_CLASS (test_read_only_object_parent_class)->finalize (object);
}

static void
test_read_only_object_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  TestReadOnlyObject *self = (TestReadOnlyObject *)object;

  switch (prop_id)
    {
    case TEST_READ_ONLY_OBJECT_PROP_ID:
      g_value_set_uint64 (value, self->id);
      break;

    case TEST_READ_ONLY_OBJECT_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_READ_ONLY_OBJECT_PROP_CREATED_AT:
      g_value_set_boxed (value, self->created_at);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static guint64
test_read_only_object_get_value_as_u64 (const GValue *value)
{
  if (G_VALUE_HOLDS (value, G_TYPE_UINT64))
    return g_value_get_uint64 (value);

  if (G_VALUE_HOLDS (value, G_TYPE_INT64))
    return (guint64)g_value_get_int64 (value);

  if (G_VALUE_HOLDS (value, G_TYPE_UINT))
    return (guint64)g_value_get_uint (value);

  if (G_VALUE_HOLDS (value, G_TYPE_INT))
    return (guint64)g_value_get_int (value);

  if (G_VALUE_HOLDS (value, G_TYPE_ULONG))
    return (guint64)g_value_get_ulong (value);

  if (G_VALUE_HOLDS (value, G_TYPE_LONG))
    return (guint64)g_value_get_long (value);

  return 0;
}

static GDateTime *
test_read_only_object_get_datetime (const GValue *value)
{
  if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
    {
      GDateTime *source = g_value_get_boxed (value);

      if (source != NULL)
        return g_date_time_ref (source);
    }

  return NULL;
}

static GomEntity *
test_read_only_object_materialize (GomEntityClass      *klass,
                                   GomCursor           *cursor,
                                   const char * const  *property_names,
                                   const GValue        *property_values,
                                   guint                n_properties,
                                   GError             **error)
{
  g_autofree char *name = NULL;
  g_autoptr(GDateTime) created_at = NULL;
  guint64 id = 0;
  TestReadOnlyObject *self = NULL;

  g_return_val_if_fail (GOM_IS_ENTITY_CLASS (klass), NULL);
  g_return_val_if_fail (GOM_IS_CURSOR (cursor), NULL);
  g_return_val_if_fail (n_properties == 0 || property_names != NULL, NULL);
  g_return_val_if_fail (n_properties == 0 || property_values != NULL, NULL);
  (void)cursor;

  for (guint i = 0; i < n_properties; i++)
    {
      const char *property_name = property_names[i];

      if (g_strcmp0 (property_name, "id") == 0)
        id = test_read_only_object_get_value_as_u64 (&property_values[i]);
      else if (g_strcmp0 (property_name, "name") == 0)
        g_set_str (&name, g_value_get_string (&property_values[i]));
      else if (g_strcmp0 (property_name, "created-at") == 0)
        {
          g_clear_pointer (&created_at, g_date_time_unref);
          created_at = test_read_only_object_get_datetime (&property_values[i]);
        }
    }

  self = test_read_only_object_new (name, created_at);
  self->id = id;

  if (self->created_at == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to materialize created-at");
      g_clear_object (&self);
      return NULL;
    }

  return GOM_ENTITY (self);
}

static gboolean
test_read_only_object_backfill_identity (GomEntity           *self,
                                         const char * const  *identity_fields,
                                         GomRecord           *record,
                                         GError             **error)
{
  gboolean updated = FALSE;

  g_assert (GOM_IS_ENTITY (self));
  g_assert (GOM_IS_RECORD (record));

  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Entity type has no identity fields");
      return FALSE;
    }

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      const char *identity_field = identity_fields[i];
      g_auto(GValue) identity_value = G_VALUE_INIT;
      guint64 id = 0;
      gboolean used_record = FALSE;

      if (g_strcmp0 (identity_field, "id") != 0)
        continue;

      if (gom_record_get_column_by_name (record, identity_field, &identity_value))
        {
          id = test_read_only_object_get_value_as_u64 (&identity_value);
          used_record = TRUE;
        }
      else if (gom_record_get_column_by_name (record, "rowid", &identity_value))
        {
          id = test_read_only_object_get_value_as_u64 (&identity_value);
          used_record = TRUE;
        }

      if (!used_record)
        continue;

      ((TestReadOnlyObject *)self)->id = id;
      updated = TRUE;
      break;
    }

  if (!updated)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Insert did not return an identity value");
      return FALSE;
    }

  return TRUE;
}

static TestReadOnlyObject *
test_read_only_object_new (const char *name,
                          GDateTime  *created_at)
{
  g_autoptr(GDateTime) created_at_copy = NULL;
  TestReadOnlyObject *self;

  if (created_at != NULL)
    created_at_copy = g_date_time_ref (created_at);

  self = g_object_new (test_read_only_object_get_type (), NULL);
  self->name = g_strdup (name);
  self->created_at = g_steal_pointer (&created_at_copy);

  return self;
}

static void
test_read_only_object_class_init (TestReadOnlyObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_read_only_object_finalize;
  object_class->get_property = test_read_only_object_get_property;
  entity_class->materialize = test_read_only_object_materialize;
  entity_class->backfill_identity = test_read_only_object_backfill_identity;

  test_read_only_object_properties[TEST_READ_ONLY_OBJECT_PROP_ID] =
    g_param_spec_uint64 ("id",
                         NULL,
                         NULL,
                         0,
                         G_MAXUINT64,
                         0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  test_read_only_object_properties[TEST_READ_ONLY_OBJECT_PROP_NAME] =
    g_param_spec_string ("name",
                         NULL,
                         NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  test_read_only_object_properties[TEST_READ_ONLY_OBJECT_PROP_CREATED_AT] =
    g_param_spec_boxed ("created-at",
                        NULL,
                        NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                    TEST_READ_ONLY_OBJECT_N_PROPS,
                                    test_read_only_object_properties);

  gom_entity_class_set_relation (entity_class, "read_only_objects");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "name", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "created-at", TRUE);
}

static void
test_read_only_object_init (TestReadOnlyObject *self)
{
}

static void
test_read_only_object_query (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistryBuilder) builder = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomEntity) loaded = NULL;
  g_autoptr(GDateTime) created_at = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autofree char *loaded_name = NULL;
  g_autoptr(GDateTime) loaded_created_at = NULL;
  sqlite3 *db = NULL;
  guint64 id = 0;
  gint64 id_filter = 0;
  const char *query = "CREATE TABLE read_only_objects ("
                      "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "  name TEXT NOT NULL, "
                      "  \"created-at\" TEXT NOT NULL"
                      ")";

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-read-only-XXXXXX", &error));
  g_assert_no_error (error);

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db, query);
  test_sqlite_close (db);
  db = NULL;

  builder = gom_registry_builder_new ();
  gom_registry_builder_add_entity_type (builder, test_read_only_object_get_type ());
  registry = gom_registry_builder_build (builder);
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  created_at = g_date_time_new_utc (2026, 6, 10, 0, 0, 0);
  entity = GOM_ENTITY (test_read_only_object_new ("alpha", created_at));
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  g_object_get (entity, "id", &id, NULL);
  g_assert_cmpuint (id, >, 0);
  id_filter = (gint64) id;

  loaded = dex_await_object (gom_repository_find_one (repository,
                                                      test_read_only_object_get_type (),
                                                      "id", id_filter,
                                                      NULL),
                             &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (loaded));
  g_object_get (loaded,
                "name", &loaded_name,
                "created-at", &loaded_created_at,
                NULL);
  g_assert_cmpstr (loaded_name, ==, "alpha");
  g_assert_true (g_date_time_equal (loaded_created_at, created_at));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Sqlite/read-only-object", test_read_only_object_query);
  return g_test_run ();
}
