/* test-gom-sqlite-stress.c
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

#include <errno.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <libgom.h>

#include "lib/sqlite/gom-sqlite-pool-private.h"
#include "lib/gom-trace-private.h"
#include "test-util.h"

#ifndef STRESS_WORKER_COUNT
#define STRESS_WORKER_COUNT 4
#endif

#ifndef STRESS_ITERATIONS
#define STRESS_ITERATIONS 20
#endif
#define READ_WORKER_COUNT (GOM_SQLITE_POOL_MAX_LEASES * 3)
#define READ_ITERATIONS 8
#define LIMITER_WORKER_COUNT (GOM_SQLITE_POOL_MAX_LEASES * 3)
#define LIMITER_SESSION_WORKER_COUNT (GOM_SQLITE_POOL_MAX_LEASES * 2)
#define LIMITER_ITERATIONS 4
#define LONG_READ_SEED_COUNT 8
#define MIGRATION_OPEN_WORKER_COUNT (GOM_SQLITE_POOL_MAX_LEASES * 2)
#define SESSION_LOCK_HELPER_ARG "--gom-sqlite-session-lock-helper"
#define SESSION_LOCK_HOLDER_HELPER_ARG "--gom-sqlite-session-lock-holder"

typedef struct _TestStressItem         TestStressItem;
typedef struct _TestStressItemClass    TestStressItemClass;
typedef struct _WorkerData             WorkerData;
typedef struct _ReadWorkerData         ReadWorkerData;
typedef struct _NoRetryWorkerData      NoRetryWorkerData;
typedef struct _SessionWorkerData      SessionWorkerData;
typedef struct _SessionCounters        SessionCounters;
typedef struct _OpenWorkerData         OpenWorkerData;
typedef struct _PoolShutdownWorkerData PoolShutdownWorkerData;
typedef struct _TraceCounters          TraceCounters;

static const char *test_program_path;

struct _TestStressItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
  char      *payload;
  GDateTime *when;
  gint       count;
  gint64     big_count;
  guint      ucount;
  gboolean   flag;
  double     ratio;
  float      fvalue;
  gint       mode;
};

struct _TestStressItemClass
{
  GomEntityClass parent_class;
};

struct _WorkerData
{
  GomRepository *repository;
  guint          worker_id;
  guint          iterations;
};

struct _ReadWorkerData
{
  GomRepository *repository;
  DexFuture     *start;
  guint64        expected_count;
  guint          iterations;
};

struct _NoRetryWorkerData
{
  GomRepository *repository;
  DexFuture     *start;
  guint          worker_id;
  guint          iterations;
};

struct _SessionCounters
{
  int active;
  int max_active;
};

struct _SessionWorkerData
{
  GomRepository   *repository;
  DexFuture       *start;
  SessionCounters *counters;
  gint64          *ids;
  guint            worker_id;
  guint            update_existing : 1;
};

struct _OpenWorkerData
{
  GomDriver   *driver;
  GomRegistry *registry;
  DexFuture   *start;
};

struct _PoolShutdownWorkerData
{
  DexPromise *started;
  DexFuture  *release;
};

struct _TraceCounters
{
  int sessions;
  int cursors;
  int identity_entries;
  int pending_entities;
};

enum {
  TEST_STRESS_ITEM_PROP_0,
  TEST_STRESS_ITEM_PROP_ID,
  TEST_STRESS_ITEM_PROP_NAME,
  TEST_STRESS_ITEM_PROP_PAYLOAD,
  TEST_STRESS_ITEM_PROP_WHEN,
  TEST_STRESS_ITEM_PROP_COUNT,
  TEST_STRESS_ITEM_PROP_BIG_COUNT,
  TEST_STRESS_ITEM_PROP_UCOUNT,
  TEST_STRESS_ITEM_PROP_FLAG,
  TEST_STRESS_ITEM_PROP_RATIO,
  TEST_STRESS_ITEM_PROP_FVALUE,
  TEST_STRESS_ITEM_PROP_MODE,
  TEST_STRESS_ITEM_N_PROPS
};

static GParamSpec *test_stress_item_properties[TEST_STRESS_ITEM_N_PROPS];

GType test_stress_item_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TestStressItem, test_stress_item, GOM_TYPE_ENTITY)

static int
test_sqlite_session_lock_helper (const char *db_path)
{
  sqlite3 *db = NULL;
  char *errmsg = NULL;
  int rc;

  g_assert_nonnull (db_path);

  rc = sqlite3_open (db_path, &db);
  if (rc != SQLITE_OK)
    {
      g_printerr ("Failed to open helper database: %s\n",
                  db != NULL ? sqlite3_errmsg (db) : "unknown error");
      if (db != NULL)
        sqlite3_close (db);
      return 3;
    }

  sqlite3_busy_timeout (db, 0);
  rc = sqlite3_exec (db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, &errmsg);
  if (rc == SQLITE_OK)
    {
      sqlite3_exec (db, "ROLLBACK", NULL, NULL, NULL);
      sqlite3_close (db);
      return 2;
    }

  if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED)
    {
      sqlite3_free (errmsg);
      sqlite3_close (db);
      return 0;
    }

  g_printerr ("Unexpected helper BEGIN IMMEDIATE failure: %s\n",
              errmsg != NULL ? errmsg : sqlite3_errmsg (db));
  sqlite3_free (errmsg);
  sqlite3_close (db);
  return 4;
}

static int
test_sqlite_session_lock_holder (const char *db_path)
{
  sqlite3 *db = NULL;
  char *errmsg = NULL;
  int rc;

  g_assert_nonnull (db_path);

  rc = sqlite3_open (db_path, &db);
  if (rc != SQLITE_OK)
    {
      g_printerr ("Failed to open lock-holder database: %s\n",
                  db != NULL ? sqlite3_errmsg (db) : "unknown error");
      if (db != NULL)
        sqlite3_close (db);
      return 3;
    }

  sqlite3_busy_timeout (db, 0);
  rc = sqlite3_exec (db,
                     "CREATE TABLE IF NOT EXISTS gom_lock_probe "
                     "(id INTEGER PRIMARY KEY AUTOINCREMENT)",
                     NULL,
                     NULL,
                     &errmsg);
  if (rc != SQLITE_OK)
    goto fail;

  rc = sqlite3_exec (db, "BEGIN IMMEDIATE TRANSACTION", NULL, NULL, &errmsg);
  if (rc != SQLITE_OK)
    goto fail;

  rc = sqlite3_exec (db,
                     "INSERT INTO gom_lock_probe DEFAULT VALUES",
                     NULL,
                     NULL,
                     &errmsg);
  if (rc != SQLITE_OK)
    goto fail;

  g_print ("ready\n");
  fflush (stdout);

  g_usleep (750 * G_TIME_SPAN_MILLISECOND);

  sqlite3_exec (db, "ROLLBACK", NULL, NULL, NULL);
  sqlite3_close (db);
  return 0;

fail:
  g_printerr ("Unexpected lock-holder SQLite failure: %s\n",
              errmsg != NULL ? errmsg : sqlite3_errmsg (db));
  sqlite3_free (errmsg);
  sqlite3_close (db);
  return 4;
}

static void
test_sqlite_assert_external_begin_immediate_blocked (const char *db_path)
{
  g_autoptr(GError) error = NULL;
  char *argv[] = {
    (char *) test_program_path,
    (char *) SESSION_LOCK_HELPER_ARG,
    (char *) db_path,
    NULL
  };
  int wait_status = 0;
  gboolean spawned;

  g_assert_nonnull (test_program_path);
  g_assert_nonnull (db_path);

  spawned = g_spawn_sync (NULL,
                          argv,
                          NULL,
                          G_SPAWN_DEFAULT,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          &wait_status,
                          &error);
  g_assert_no_error (error);
  g_assert_true (spawned);

  g_assert_true (g_spawn_check_wait_status (wait_status, &error));
  g_assert_no_error (error);
}

static void
test_sqlite_wait_for_lock_holder_ready (int stdout_fd)
{
  char buffer[16] = {0};
  gsize len = 0;

  g_assert_cmpint (stdout_fd, >=, 0);

  while (len + 1 < sizeof buffer)
    {
      ssize_t n_read;

      n_read = read (stdout_fd, buffer + len, 1);
      if (n_read < 0 && errno == EINTR)
        continue;

      g_assert_cmpint (n_read, ==, 1);
      len++;

      if (buffer[len - 1] == '\n')
        break;
    }

  buffer[len] = '\0';
  g_assert_cmpstr (buffer, ==, "ready\n");
}

static GPid
test_sqlite_spawn_external_write_lock (const char *db_path)
{
  g_autoptr(GError) error = NULL;
  char *argv[] = {
    (char *) test_program_path,
    (char *) SESSION_LOCK_HOLDER_HELPER_ARG,
    (char *) db_path,
    NULL
  };
  GPid child_pid = 0;
  int stdout_fd = -1;
  gboolean spawned;

  g_assert_nonnull (test_program_path);
  g_assert_nonnull (db_path);

  spawned = g_spawn_async_with_pipes (NULL,
                                      argv,
                                      NULL,
                                      G_SPAWN_DO_NOT_REAP_CHILD,
                                      NULL,
                                      NULL,
                                      &child_pid,
                                      NULL,
                                      &stdout_fd,
                                      NULL,
                                      &error);
  g_assert_no_error (error);
  g_assert_true (spawned);

  test_sqlite_wait_for_lock_holder_ready (stdout_fd);
  close (stdout_fd);

  return child_pid;
}

static void
test_sqlite_wait_for_external_write_lock (GPid child_pid)
{
  g_autoptr(GError) error = NULL;
  int wait_status = 0;

  g_assert_cmpint (child_pid, >, 0);
  g_assert_cmpint (waitpid (child_pid, &wait_status, 0), ==, child_pid);
  g_assert_true (g_spawn_check_wait_status (wait_status, &error));
  g_assert_no_error (error);
  g_spawn_close_pid (child_pid);
}

static GBytes *
stress_item_to_bytes (const GValue  *value,
                      gpointer       user_data,
                      GError       **error)
{
  const char *str = g_value_get_string (value);

  if (str == NULL)
    return g_bytes_new (NULL, 0);

  return g_bytes_new (str, strlen (str));
}

static gboolean
stress_item_from_bytes (GBytes    *bytes,
                        GValue    *value,
                        gpointer   user_data,
                        GError   **error)
{
  gsize size = 0;
  const char *data = NULL;

  g_value_init (value, G_TYPE_STRING);

  if (bytes == NULL)
    {
      g_value_set_string (value, NULL);
      return TRUE;
    }

  data = g_bytes_get_data (bytes, &size);
  g_value_take_string (value, g_strndup (data, size));

  return TRUE;
}

static GBytes *
stress_item_datetime_to_bytes (const GValue  *value,
                               gpointer       user_data,
                               GError       **error)
{
  GDateTime *dt = g_value_get_boxed (value);
  g_autofree char *iso8601 = NULL;

  if (dt == NULL)
    return g_bytes_new (NULL, 0);

  iso8601 = g_date_time_format_iso8601 (dt);
  return g_bytes_new (iso8601, strlen (iso8601));
}

static gboolean
stress_item_datetime_from_bytes (GBytes    *bytes,
                                 GValue    *value,
                                 gpointer   user_data,
                                 GError   **error)
{
  gsize size = 0;
  const char *data = NULL;
  g_autofree char *iso8601 = NULL;
  g_autoptr(GDateTime) dt = NULL;

  g_value_init (value, G_TYPE_DATE_TIME);

  if (bytes == NULL)
    {
      g_value_set_boxed (value, NULL);
      return TRUE;
    }

  data = g_bytes_get_data (bytes, &size);
  iso8601 = g_strndup (data, size);
  dt = g_date_time_new_from_iso8601 (iso8601, NULL);

  if (dt == NULL)
    return FALSE;

  g_value_take_boxed (value, g_steal_pointer (&dt));
  return TRUE;
}

static void
test_stress_item_finalize (GObject *object)
{
  TestStressItem *self = (TestStressItem *) object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->payload, g_free);
  g_clear_pointer (&self->when, g_date_time_unref);

  G_OBJECT_CLASS (test_stress_item_parent_class)->finalize (object);
}

static void
test_stress_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestStressItem *self = (TestStressItem *) object;

  switch (prop_id)
    {
    case TEST_STRESS_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_STRESS_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_STRESS_ITEM_PROP_PAYLOAD:
      g_value_set_string (value, self->payload);
      break;

    case TEST_STRESS_ITEM_PROP_WHEN:
      g_value_set_boxed (value, self->when);
      break;

    case TEST_STRESS_ITEM_PROP_COUNT:
      g_value_set_int (value, self->count);
      break;

    case TEST_STRESS_ITEM_PROP_BIG_COUNT:
      g_value_set_int64 (value, self->big_count);
      break;

    case TEST_STRESS_ITEM_PROP_UCOUNT:
      g_value_set_uint (value, self->ucount);
      break;

    case TEST_STRESS_ITEM_PROP_FLAG:
      g_value_set_boolean (value, self->flag);
      break;

    case TEST_STRESS_ITEM_PROP_RATIO:
      g_value_set_double (value, self->ratio);
      break;

    case TEST_STRESS_ITEM_PROP_FVALUE:
      g_value_set_float (value, self->fvalue);
      break;

    case TEST_STRESS_ITEM_PROP_MODE:
      g_value_set_int (value, self->mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_stress_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestStressItem *self = (TestStressItem *) object;

  switch (prop_id)
    {
    case TEST_STRESS_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_STRESS_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_STRESS_ITEM_PROP_PAYLOAD:
      g_set_str (&self->payload, g_value_get_string (value));
      break;

    case TEST_STRESS_ITEM_PROP_WHEN:
      g_clear_pointer (&self->when, g_date_time_unref);
      if (g_value_get_boxed (value) != NULL)
        self->when = g_date_time_ref (g_value_get_boxed (value));
      break;

    case TEST_STRESS_ITEM_PROP_COUNT:
      self->count = g_value_get_int (value);
      break;

    case TEST_STRESS_ITEM_PROP_BIG_COUNT:
      self->big_count = g_value_get_int64 (value);
      break;

    case TEST_STRESS_ITEM_PROP_UCOUNT:
      self->ucount = g_value_get_uint (value);
      break;

    case TEST_STRESS_ITEM_PROP_FLAG:
      self->flag = g_value_get_boolean (value);
      break;

    case TEST_STRESS_ITEM_PROP_RATIO:
      self->ratio = g_value_get_double (value);
      break;

    case TEST_STRESS_ITEM_PROP_FVALUE:
      self->fvalue = g_value_get_float (value);
      break;

    case TEST_STRESS_ITEM_PROP_MODE:
      self->mode = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_stress_item_class_init (TestStressItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_stress_item_finalize;
  object_class->set_property = test_stress_item_set_property;
  object_class->get_property = test_stress_item_get_property;

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_PAYLOAD] =
    g_param_spec_string ("payload", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_WHEN] =
    g_param_spec_boxed ("stamp", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_COUNT] =
    g_param_spec_int ("count", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_BIG_COUNT] =
    g_param_spec_int64 ("bigcount", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_UCOUNT] =
    g_param_spec_uint ("ucount", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_FLAG] =
    g_param_spec_boolean ("flag", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_RATIO] =
    g_param_spec_double ("ratio", NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_FVALUE] =
    g_param_spec_float ("fvalue", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_stress_item_properties[TEST_STRESS_ITEM_PROP_MODE] =
    g_param_spec_int ("mode", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_STRESS_ITEM_N_PROPS,
                                     test_stress_item_properties);

  gom_entity_class_set_relation (entity_class, "stress_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_byte_transform (entity_class,
                                                "payload",
                                                stress_item_to_bytes,
                                                stress_item_from_bytes,
                                                NULL,
                                                NULL);
  gom_entity_class_property_set_byte_transform (entity_class,
                                                "stamp",
                                                stress_item_datetime_to_bytes,
                                                stress_item_datetime_from_bytes,
                                                NULL,
                                                NULL);
}

static void
test_stress_item_init (TestStressItem *self)
{
}

static GomRegistry *
test_stress_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_stress_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_sqlite_stress_get_registry (void)
{
  static gsize initialized = 0;

  if (g_once_init_enter (&initialized))
    {
      g_autoptr(GomRegistry) registry = NULL;

      registry = test_stress_create_registry ();
      g_once_init_leave (&initialized, (gsize) g_steal_pointer (&registry));
    }

  return g_object_ref (GSIZE_TO_POINTER (initialized));
}

static GomRepository *
test_sqlite_stress_open_repository (TestSqliteContext  *context,
                                    GomRegistry       **registry_out,
                                    GError            **error)
{
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;

  registry = test_sqlite_stress_get_registry ();
  repository = test_sqlite_context_create_repository (context, registry, error);
  if (registry_out != NULL)
    *registry_out = g_steal_pointer (&registry);

  return g_steal_pointer (&repository);
}

static void
test_sqlite_create_stress_table (const char *db_path)
{
  sqlite3 *db = NULL;

  test_sqlite_open (db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE stress_items ("
                       "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
                       "  name TEXT NOT NULL, "
                       "  payload BLOB, "
                       "  stamp BLOB NOT NULL, "
                       "  count INTEGER NOT NULL, "
                       "  bigcount INTEGER NOT NULL, "
                       "  ucount INTEGER NOT NULL, "
                       "  flag INTEGER NOT NULL, "
                       "  ratio REAL NOT NULL, "
                       "  fvalue REAL NOT NULL, "
                       "  mode INTEGER NOT NULL"
                       ")");
  test_sqlite_close (db);
}

static guint
test_sqlite_count_stress_items (const char *db_path)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  guint count;
  int rc;

  test_sqlite_open (db_path, &db);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT COUNT(*) FROM stress_items",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  count = (guint) sqlite3_column_int (stmt, 0);

  sqlite3_finalize (stmt);
  test_sqlite_close (db);

  return count;
}

static guint
test_sqlite_count_stress_item_by_id (const char *db_path,
                                     gint64      id)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  guint count;
  int rc;

  test_sqlite_open (db_path, &db);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT COUNT(*) FROM stress_items WHERE id = ?1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  rc = sqlite3_bind_int64 (stmt, 1, id);
  g_assert_cmpint (rc, ==, SQLITE_OK);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  count = (guint) sqlite3_column_int (stmt, 0);

  sqlite3_finalize (stmt);
  test_sqlite_close (db);

  return count;
}

static int
test_sqlite_read_stress_item_count_by_id (const char *db_path,
                                          gint64      id)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int count;
  int rc;

  test_sqlite_open (db_path, &db);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT \"count\" FROM stress_items WHERE id = ?1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  rc = sqlite3_bind_int64 (stmt, 1, id);
  g_assert_cmpint (rc, ==, SQLITE_OK);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  count = sqlite3_column_int (stmt, 0);

  sqlite3_finalize (stmt);
  test_sqlite_close (db);

  return count;
}

static GomEntity *
stress_item_new (guint worker_id,
                 guint iteration)
{
  g_autofree char *name = NULL;
  g_autofree char *payload = NULL;
  g_autoptr(GDateTime) when = NULL;

  name = g_strdup_printf ("worker-%u-item-%u", worker_id, iteration);
  payload = g_strdup_printf ("payload-%u-%u", worker_id, iteration);
  when = g_date_time_new_utc (2026, 1, 1, 0, 0, 0);
  when = g_date_time_add_seconds (g_steal_pointer (&when),
                                  worker_id * 1000 + iteration);

  return g_object_new (test_stress_item_get_type (),
                       "name", name,
                       "payload", payload,
                       "stamp", when,
                       "count", (gint) (iteration % 1000),
                       "bigcount", (gint64) worker_id * 100000 + iteration,
                       "ucount", iteration,
                       "flag", (iteration % 2) == 0,
                       "ratio", ((double) iteration / 3.0) + worker_id,
                       "fvalue", (float) ((double) iteration / 7.0),
                       "mode", (gint) (iteration % 5),
                       NULL);
}

static GomQuery *
stress_item_query_new (GError **error)
{
  g_autoptr(GomQueryBuilder) builder = gom_query_builder_new ();

  gom_query_builder_set_target_entity_type (builder, test_stress_item_get_type ());

  return gom_query_builder_build (builder, error);
}

static GomQuery *
stress_item_count_query_new (GError **error)
{
  g_autoptr(GomQueryBuilder) builder = gom_query_builder_new ();

  gom_query_builder_set_target_entity_type (builder, test_stress_item_get_type ());

  return gom_query_builder_build_with_count (builder, error);
}

static GomQuery *
stress_item_query_by_id_new (gint64   id,
                             GError **error)
{
  g_autoptr(GomQueryBuilder) builder = gom_query_builder_new ();
  g_auto(GValue) id_value = G_VALUE_INIT;

  g_value_init (&id_value, G_TYPE_INT64);
  g_value_set_int64 (&id_value, id);

  gom_query_builder_set_target_entity_type (builder, test_stress_item_get_type ());
  gom_query_builder_set_filter (builder,
                                gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                                                 gom_literal_expression_new (&id_value)));

  return gom_query_builder_build (builder, error);
}

static GomEntity *
query_stress_item_by_id (GomRepository  *repository,
                         gint64          id,
                         GError        **error)
{
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomEntity) materialized = NULL;
  gboolean has_row;

  query = stress_item_query_by_id_new (id, error);
  if (query == NULL)
    return NULL;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return NULL;

  has_row = dex_await_boolean (gom_cursor_next (cursor), error);
  if (error != NULL && *error != NULL)
    return NULL;

  if (!has_row)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "No row for id=%" G_GINT64_FORMAT,
                   id);
      return NULL;
    }

  materialized = gom_cursor_materialize (cursor, error);
  if (materialized == NULL)
    return NULL;

  if (!dex_await (gom_cursor_close (cursor), error))
    return NULL;

  return g_steal_pointer (&materialized);
}

static GomEntity *
query_stress_item_by_id_in_session (GomSession  *session,
                                    gint64       id,
                                    GError     **error)
{
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomEntity) materialized = NULL;
  gboolean has_row;

  query = stress_item_query_by_id_new (id, error);
  if (query == NULL)
    return NULL;

  cursor = dex_await_object (gom_session_query (session, query), error);
  if (cursor == NULL)
    return NULL;

  has_row = dex_await_boolean (gom_cursor_next (cursor), error);
  if (error != NULL && *error != NULL)
    return NULL;

  if (!has_row)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "No row for id=%" G_GINT64_FORMAT,
                   id);
      return NULL;
    }

  materialized = gom_cursor_materialize (cursor, error);
  if (materialized == NULL)
    return NULL;

  if (!dex_await (gom_cursor_close (cursor), error))
    return NULL;

  return g_steal_pointer (&materialized);
}

static GomEntity *
insert_stress_item (GomRepository  *repository,
                    guint           worker_id,
                    guint           iteration,
                    GError        **error)
{
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;

  entity = stress_item_new (worker_id, iteration);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), error);
  if (result == NULL)
    return NULL;

  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  return g_steal_pointer (&entity);
}

static gboolean
stress_one_iteration (GomRepository  *repository,
                      guint           worker_id,
                      guint           iteration,
                      GError        **error)
{
  g_autoptr(GomEntity) inserted = NULL;
  g_autoptr(GomEntity) materialized = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autofree char *name = NULL;
  g_autofree char *payload = NULL;
  g_autofree char *updated_name = NULL;
  g_autofree char *updated_payload = NULL;
  g_autoptr(GDateTime) when = NULL;
  g_autoptr(GDateTime) updated_when = NULL;
  g_auto(GValue) id_value = G_VALUE_INIT;
  gint64 id;
  gboolean has_row;

  name = g_strdup_printf ("worker-%u-item-%u", worker_id, iteration);
  payload = g_strdup_printf ("payload-%u-%u", worker_id, iteration);
  updated_name = g_strdup_printf ("worker-%u-item-%u-updated", worker_id, iteration);
  updated_payload = g_strdup_printf ("payload-%u-%u-updated", worker_id, iteration);

  when = g_date_time_new_utc (2026, 1, 1, 0, 0, 0);
  when = g_date_time_add_seconds (g_steal_pointer (&when), worker_id * 1000 + iteration);
  updated_when = g_date_time_add_seconds (when, 60);

  inserted = g_object_new (test_stress_item_get_type (),
                           "name", name,
                           "payload", payload,
                           "stamp", when,
                           "count", (gint) (iteration % 1000),
                           "bigcount", (gint64) worker_id * 100000 + iteration,
                           "ucount", iteration,
                           "flag", (iteration % 2) == 0,
                           "ratio", ((double) iteration / 3.0) + worker_id,
                           "fvalue", (float) ((double) iteration / 7.0),
                           "mode", (gint) (iteration % 5),
                           NULL);
  gom_entity_set_repository (inserted, repository);

  result = dex_await_object (gom_entity_insert (inserted), error);
  if (result == NULL)
    return FALSE;

  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_object_get (inserted,
                "id", &id,
                NULL);
  g_assert_cmpint (id, >, 0);
  g_clear_object (&result);

  materialized = query_stress_item_by_id (repository, id, error);
  if (materialized == NULL)
    return FALSE;

  g_object_set (materialized,
                "name", updated_name,
                "payload", updated_payload,
                "stamp", updated_when,
                "count", (gint) iteration + 1,
                "bigcount", ((gint64) worker_id * 100000 + iteration) * 2,
                "ucount", iteration + 1,
                "flag", (iteration % 3) == 0,
                "ratio", ((double) iteration / 2.0) + 0.5,
                "fvalue", (float) ((double) iteration / 5.0),
                "mode", (gint) ((iteration + worker_id) % 11),
                NULL);

  result = dex_await_object (gom_entity_update (materialized), error);
  if (result == NULL)
    return FALSE;

  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_clear_object (&result);

  result = dex_await_object (gom_entity_delete (materialized), error);
  if (result == NULL)
    return FALSE;

  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_clear_object (&result);

  g_value_init (&id_value, G_TYPE_INT64);
  g_value_set_int64 (&id_value, id);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_stress_item_get_type ());
  gom_query_builder_set_filter (query_builder,
                                gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                                                 gom_literal_expression_new (&id_value)));

  query = gom_query_builder_build (query_builder, error);
  if (query == NULL)
    return FALSE;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return FALSE;

  has_row = dex_await_boolean (gom_cursor_next (cursor), error);
  if (error != NULL && *error != NULL)
    return FALSE;

  if (has_row)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Expected row %" G_GINT64_FORMAT " to be deleted",
                   id);
      return FALSE;
    }

  if (!dex_await (gom_cursor_close (cursor), error))
    return FALSE;

  return TRUE;
}

static void
worker_data_free (WorkerData *worker_data)
{
  g_clear_object (&worker_data->repository);
  g_free (worker_data);
}

static void
no_retry_worker_data_free (NoRetryWorkerData *worker_data)
{
  g_clear_object (&worker_data->repository);
  dex_clear (&worker_data->start);
  g_free (worker_data);
}

static void
read_worker_data_free (ReadWorkerData *worker_data)
{
  g_clear_object (&worker_data->repository);
  dex_clear (&worker_data->start);
  g_free (worker_data);
}

static void
session_worker_data_free (SessionWorkerData *worker_data)
{
  g_clear_object (&worker_data->repository);
  dex_clear (&worker_data->start);
  g_free (worker_data);
}

static void
open_worker_data_free (OpenWorkerData *worker_data)
{
  g_clear_object (&worker_data->driver);
  g_clear_object (&worker_data->registry);
  dex_clear (&worker_data->start);
  g_free (worker_data);
}

static void
pool_shutdown_worker_data_free (PoolShutdownWorkerData *worker_data)
{
  dex_clear (&worker_data->started);
  dex_clear (&worker_data->release);
  g_free (worker_data);
}

static GPtrArray *
test_future_array_new (void)
{
  return g_ptr_array_new_with_free_func (dex_unref);
}

static void
test_trace_counters_snapshot (TraceCounters *counters)
{
  g_assert_nonnull (counters);

  counters->sessions = gom_trace_counter_get (GOM_TRACE_COUNTER_SESSIONS);
  counters->cursors = gom_trace_counter_get (GOM_TRACE_COUNTER_CURSORS);
  counters->identity_entries = gom_trace_counter_get (GOM_TRACE_COUNTER_IDENTITY_ENTRIES);
  counters->pending_entities = gom_trace_counter_get (GOM_TRACE_COUNTER_PENDING_ENTITIES);
}

static void
test_trace_counters_assert_equal (const TraceCounters *baseline)
{
  TraceCounters current;

  g_assert_nonnull (baseline);

  dex_await (dex_timeout_new_msec (25), NULL);
  test_trace_counters_snapshot (&current);

  g_assert_cmpint (current.sessions, ==, baseline->sessions);
  g_assert_cmpint (current.cursors, ==, baseline->cursors);
  g_assert_cmpint (current.identity_entries, ==, baseline->identity_entries);
  g_assert_cmpint (current.pending_entities, ==, baseline->pending_entities);
}

static void
session_counters_update_max (SessionCounters *counters,
                             int              value)
{
  int old_value;

  g_assert_nonnull (counters);

  for (;;)
    {
      old_value = g_atomic_int_get (&counters->max_active);
      if (value <= old_value)
        return;

      if (g_atomic_int_compare_and_exchange (&counters->max_active, old_value, value))
        return;
    }
}

static void
assert_no_retry_futures_complete (GPtrArray  *fibers,
                                  GError    **error)
{
  g_assert_nonnull (fibers);

  for (guint i = 0; i < fibers->len; i++)
    {
      DexFuture *future = g_ptr_array_index (fibers, i);
      gboolean r;

      r = dex_await (dex_ref (future), error);
      if (error != NULL && *error != NULL)
        return;

      g_assert_true (r);
    }
}

static void
test_sqlite_assert_pool_lease_limit (GomRepository *repository)
{
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(DexFuture) pending_query = NULL;
  g_autoptr(GomCursor) blocked_cursor = NULL;
  g_autoptr(GError) error = NULL;
  GomCursor *held_cursors[GOM_SQLITE_POOL_MAX_LEASES] = {NULL};

  g_assert_true (GOM_IS_REPOSITORY (repository));

  query = stress_item_query_new (&error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_QUERY (query));

  for (guint i = 0; i < GOM_SQLITE_POOL_MAX_LEASES; i++)
    {
      held_cursors[i] = dex_await_object (gom_repository_query (repository, query), &error);
      g_assert_no_error (error);
      g_assert_true (GOM_IS_CURSOR (held_cursors[i]));
    }

  pending_query = gom_repository_query (repository, query);
  dex_await (dex_timeout_new_msec (25), NULL);
  g_assert_true (dex_future_is_pending (pending_query));

  g_assert_true (dex_await (gom_cursor_close (held_cursors[0]), &error));
  g_assert_no_error (error);
  g_clear_object (&held_cursors[0]);

  blocked_cursor = dex_await_object (g_steal_pointer (&pending_query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (blocked_cursor));

  g_assert_true (dex_await (gom_cursor_close (blocked_cursor), &error));
  g_assert_no_error (error);
  g_clear_object (&blocked_cursor);

  for (guint i = 1; i < GOM_SQLITE_POOL_MAX_LEASES; i++)
    {
      g_assert_true (dex_await (gom_cursor_close (held_cursors[i]), &error));
      g_assert_no_error (error);
      g_clear_object (&held_cursors[i]);
    }
}

static DexFuture *
test_sqlite_pool_shutdown_blocking_thread (gpointer user_data)
{
  PoolShutdownWorkerData *worker_data = user_data;
  g_autoptr(GError) error = NULL;

  g_assert_nonnull (worker_data);
  g_assert (DEX_IS_PROMISE (worker_data->started));
  g_assert (DEX_IS_FUTURE (worker_data->release));

  dex_promise_resolve_boolean (worker_data->started, TRUE);

  if (!dex_thread_wait_for (dex_ref (worker_data->release), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
read_worker_fiber (gpointer user_data)
{
  ReadWorkerData *worker_data = user_data;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (worker_data != NULL);
  g_assert (GOM_IS_REPOSITORY (worker_data->repository));
  g_assert (DEX_IS_FUTURE (worker_data->start));

  if (!dex_await (dex_ref (worker_data->start), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  query = stress_item_count_query_new (&error);
  if (query == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < worker_data->iterations; i++)
    {
      g_autoptr(GomCursor) cursor = NULL;

      cursor = dex_await_object (gom_repository_query (worker_data->repository, query), &error);
      if (cursor == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (gom_cursor_get_count (cursor) != worker_data->expected_count)
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Expected %" G_GUINT64_FORMAT " rows, got %" G_GUINT64_FORMAT,
                       worker_data->expected_count,
                       gom_cursor_get_count (cursor));
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (!dex_await (gom_cursor_close (cursor), &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
stress_worker_fiber (gpointer user_data)
{
  WorkerData *worker_data = user_data;
  g_autoptr(GError) error = NULL;

  for (guint i = 0; i < worker_data->iterations; i++)
    {
      if (!stress_one_iteration (worker_data->repository,
                                 worker_data->worker_id,
                                 i,
                                 &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
no_retry_worker_fiber (gpointer user_data)
{
  NoRetryWorkerData *worker_data = user_data;
  g_autoptr(GError) error = NULL;

  if (!dex_await (dex_ref (worker_data->start), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < worker_data->iterations; i++)
    {
      g_autoptr(GomEntity) inserted = NULL;
      g_autoptr(GomEntity) materialized = NULL;
      gint64 id = 0;

      inserted = insert_stress_item (worker_data->repository,
                                     worker_data->worker_id,
                                     i,
                                     &error);
      if (inserted == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_object_get (inserted,
                    "id", &id,
                    NULL);
      g_assert_cmpint (id, >, 0);

      materialized = query_stress_item_by_id (worker_data->repository,
                                              id,
                                              &error);
      if (materialized == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

static DexFuture *
open_repository_worker_fiber (gpointer user_data)
{
  OpenWorkerData *worker_data = user_data;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (worker_data != NULL);
  g_assert (GOM_IS_DRIVER (worker_data->driver));
  g_assert (GOM_IS_REGISTRY (worker_data->registry));
  g_assert (DEX_IS_FUTURE (worker_data->start));

  if (!dex_await (dex_ref (worker_data->start), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  repository = dex_await_object (gom_repository_new (worker_data->driver,
                                                     worker_data->registry,
                                                     NULL),
                                 &error);
  if (repository == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
session_worker_fiber (gpointer user_data)
{
  SessionWorkerData *worker_data = user_data;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GError) error = NULL;
  gint64 id = 0;
  int active;
  gboolean counted_active = FALSE;

  if (!dex_await (dex_ref (worker_data->start), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  session = dex_await_object (gom_repository_begin_session (worker_data->repository), &error);
  if (session == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  active = g_atomic_int_add (&worker_data->counters->active, 1) + 1;
  counted_active = TRUE;
  session_counters_update_max (worker_data->counters, active);

  if (active != 1)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Expected one active write session, got %d",
                   active);
      goto fail;
    }

  dex_await (dex_timeout_new_msec (20), NULL);

  if (worker_data->update_existing)
    {
      id = worker_data->ids[worker_data->worker_id];
      if (id <= 0)
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Worker %u does not have an id to update",
                       worker_data->worker_id);
          goto fail;
        }

      entity = query_stress_item_by_id_in_session (session, id, &error);
      if (entity == NULL)
        goto fail;

      g_object_set (entity,
                    "count", (gint) (20000 + worker_data->worker_id),
                    NULL);

      if (!dex_await (gom_session_flush (session), &error))
        goto fail;
    }
  else
    {
      entity = stress_item_new (1000 + worker_data->worker_id, 0);
      gom_entity_set_repository (entity, worker_data->repository);

      if (!dex_await (gom_session_persist (session, entity), &error))
        goto fail;

      if (!dex_await (gom_session_flush (session), &error))
        goto fail;

      g_object_get (entity,
                    "id", &id,
                    NULL);
      if (id <= 0)
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Worker %u session flush did not backfill an identity",
                       worker_data->worker_id);
          goto fail;
        }

    }

  if (!dex_await (gom_session_commit (session), &error))
    goto fail;

  g_atomic_int_add (&worker_data->counters->active, -1);
  counted_active = FALSE;

  if (!worker_data->update_existing)
    worker_data->ids[worker_data->worker_id] = id;

  return dex_future_new_true ();

fail:
  if (counted_active)
    g_atomic_int_add (&worker_data->counters->active, -1);

  if (session != NULL)
    dex_await (gom_session_rollback (session), NULL);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static void
test_sqlite_concurrent_repository_reads (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GPtrArray) fibers = NULL;
  g_autoptr(DexPromise) start_promise = NULL;
  g_autoptr(GError) error = NULL;
  guint64 expected_count = GOM_SQLITE_POOL_MAX_LEASES * 2;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-concurrent-read-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  for (guint i = 0; i < expected_count; i++)
    {
      g_autoptr(GomEntity) entity = NULL;

      entity = insert_stress_item (repository, 4000, i, &error);
      g_assert_no_error (error);
      g_assert_true (GOM_IS_ENTITY (entity));
    }

  fibers = test_future_array_new ();
  start_promise = dex_promise_new ();

  for (guint i = 0; i < READ_WORKER_COUNT; i++)
    {
      ReadWorkerData *worker_data = g_new0 (ReadWorkerData, 1);

      worker_data->repository = g_object_ref (repository);
      worker_data->start = dex_ref (DEX_FUTURE (start_promise));
      worker_data->expected_count = expected_count;
      worker_data->iterations = READ_ITERATIONS;

      g_ptr_array_add (fibers,
                       dex_scheduler_spawn (NULL,
                                            0,
                                            read_worker_fiber,
                                            worker_data,
                                            (GDestroyNotify) read_worker_data_free));
    }

  dex_promise_resolve_boolean (start_promise, TRUE);

  for (guint i = 0; i < fibers->len; i++)
    {
      DexFuture *future = g_ptr_array_index (fibers, i);
      gboolean r;

      r = dex_await (dex_ref (future), &error);
      g_assert_no_error (error);
      g_assert_true (r);
    }

  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path),
                    ==,
                    expected_count);

  g_clear_pointer (&fibers, g_ptr_array_unref);
  dex_clear (&start_promise);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_limiter_concurrency_no_retry (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GPtrArray) fibers = NULL;
  g_autoptr(DexPromise) start_promise = NULL;
  g_autoptr(GError) error = NULL;
  gint64 session_ids[LIMITER_SESSION_WORKER_COUNT] = {0};
  SessionCounters insert_counters = {0};
  SessionCounters update_counters = {0};
  guint expected_count;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-limiter-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  fibers = test_future_array_new ();
  start_promise = dex_promise_new ();

  for (guint i = 0; i < LIMITER_WORKER_COUNT; i++)
    {
      NoRetryWorkerData *worker_data = g_new0 (NoRetryWorkerData, 1);

      worker_data->repository = g_object_ref (repository);
      worker_data->start = dex_ref (DEX_FUTURE (start_promise));
      worker_data->worker_id = i;
      worker_data->iterations = LIMITER_ITERATIONS;

      g_ptr_array_add (fibers,
                       dex_scheduler_spawn (NULL,
                                            0,
                                            no_retry_worker_fiber,
                                            worker_data,
                                            (GDestroyNotify) no_retry_worker_data_free));
    }

  dex_promise_resolve_boolean (start_promise, TRUE);
  assert_no_retry_futures_complete (fibers, &error);
  g_assert_no_error (error);

  expected_count = LIMITER_WORKER_COUNT * LIMITER_ITERATIONS;
  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path),
                    ==,
                    expected_count);
  test_sqlite_assert_pool_lease_limit (repository);

  g_clear_pointer (&fibers, g_ptr_array_unref);
  dex_clear (&start_promise);
  test_trace_counters_assert_equal (&baseline);

  fibers = test_future_array_new ();
  start_promise = dex_promise_new ();

  for (guint i = 0; i < LIMITER_SESSION_WORKER_COUNT; i++)
    {
      SessionWorkerData *worker_data = g_new0 (SessionWorkerData, 1);

      worker_data->repository = g_object_ref (repository);
      worker_data->start = dex_ref (DEX_FUTURE (start_promise));
      worker_data->counters = &insert_counters;
      worker_data->ids = session_ids;
      worker_data->worker_id = i;

      g_ptr_array_add (fibers,
                       dex_scheduler_spawn (NULL,
                                            0,
                                            session_worker_fiber,
                                            worker_data,
                                            (GDestroyNotify) session_worker_data_free));
    }

  dex_promise_resolve_boolean (start_promise, TRUE);
  assert_no_retry_futures_complete (fibers, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_atomic_int_get (&insert_counters.active), ==, 0);
  g_assert_cmpint (g_atomic_int_get (&insert_counters.max_active), ==, 1);

  for (guint i = 0; i < LIMITER_SESSION_WORKER_COUNT; i++)
    {
      g_assert_cmpint (session_ids[i], >, 0);
      g_assert_cmpuint (test_sqlite_count_stress_item_by_id (context.db_path,
                                                             session_ids[i]),
                        ==,
                        1);
    }

  expected_count += LIMITER_SESSION_WORKER_COUNT;
  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path),
                    ==,
                    expected_count);

  g_clear_pointer (&fibers, g_ptr_array_unref);
  dex_clear (&start_promise);
  test_trace_counters_assert_equal (&baseline);

  fibers = test_future_array_new ();
  start_promise = dex_promise_new ();

  for (guint i = 0; i < LIMITER_SESSION_WORKER_COUNT; i++)
    {
      SessionWorkerData *worker_data = g_new0 (SessionWorkerData, 1);

      worker_data->repository = g_object_ref (repository);
      worker_data->start = dex_ref (DEX_FUTURE (start_promise));
      worker_data->counters = &update_counters;
      worker_data->ids = session_ids;
      worker_data->worker_id = i;
      worker_data->update_existing = TRUE;

      g_ptr_array_add (fibers,
                       dex_scheduler_spawn (NULL,
                                            0,
                                            session_worker_fiber,
                                            worker_data,
                                            (GDestroyNotify) session_worker_data_free));
    }

  dex_promise_resolve_boolean (start_promise, TRUE);
  assert_no_retry_futures_complete (fibers, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_atomic_int_get (&update_counters.active), ==, 0);
  g_assert_cmpint (g_atomic_int_get (&update_counters.max_active), ==, 1);

  for (guint i = 0; i < LIMITER_SESSION_WORKER_COUNT; i++)
    g_assert_cmpint (test_sqlite_read_stress_item_count_by_id (context.db_path,
                                                               session_ids[i]),
                     ==,
                     (gint) (20000 + i));

  g_clear_pointer (&fibers, g_ptr_array_unref);
  dex_clear (&start_promise);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_long_read_cursor_queues_writes (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) read_cursor = NULL;
  g_autoptr(GomEntity) write_entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(DexFuture) write_future = NULL;
  g_autoptr(GError) error = NULL;
  gboolean has_row;
  guint cursor_count = 0;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-long-read-write-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  for (guint i = 0; i < LONG_READ_SEED_COUNT; i++)
    {
      g_autoptr(GomEntity) entity = NULL;

      entity = insert_stress_item (repository, 5000, i, &error);
      g_assert_no_error (error);
      g_assert_true (GOM_IS_ENTITY (entity));
    }

  query = stress_item_query_new (&error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_QUERY (query));

  read_cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (read_cursor));

  has_row = dex_await_boolean (gom_cursor_next (read_cursor), &error);
  g_assert_no_error (error);
  g_assert_true (has_row);
  cursor_count++;

  write_entity = stress_item_new (6000, 0);
  gom_entity_set_repository (write_entity, repository);

  write_future = gom_entity_insert (write_entity);
  g_clear_object (&write_entity);
  g_clear_object (&repository);
  result = dex_await_object (dex_future_with_timeout_msec (g_steal_pointer (&write_future),
                                                           1000),
                             &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  for (;;)
    {
      has_row = dex_await_boolean (gom_cursor_next (read_cursor), &error);
      g_assert_no_error (error);

      if (!has_row)
        break;

      cursor_count++;
    }

  g_assert_cmpuint (cursor_count, ==, LONG_READ_SEED_COUNT);

  g_assert_true (dex_await (gom_cursor_close (read_cursor), &error));
  g_assert_no_error (error);
  g_clear_object (&read_cursor);

  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path),
                    ==,
                    LONG_READ_SEED_COUNT + 1);

  g_clear_object (&result);
  g_clear_object (&query);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_migration_open_contention (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GPtrArray) fibers = NULL;
  g_autoptr(DexPromise) start_promise = NULL;
  g_autoptr(GError) error = NULL;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-migration-open-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  registry = test_sqlite_stress_get_registry ();
  fibers = test_future_array_new ();
  start_promise = dex_promise_new ();

  for (guint i = 0; i < MIGRATION_OPEN_WORKER_COUNT; i++)
    {
      OpenWorkerData *worker_data = g_new0 (OpenWorkerData, 1);

      worker_data->driver = g_object_ref (context.driver);
      worker_data->registry = g_object_ref (registry);
      worker_data->start = dex_ref (DEX_FUTURE (start_promise));

      g_ptr_array_add (fibers,
                       dex_scheduler_spawn (NULL,
                                            0,
                                            open_repository_worker_fiber,
                                            worker_data,
                                            (GDestroyNotify) open_worker_data_free));
    }

  dex_promise_resolve_boolean (start_promise, TRUE);

  for (guint i = 0; i < fibers->len; i++)
    {
      DexFuture *future = g_ptr_array_index (fibers, i);
      gboolean r;

      r = dex_await (dex_ref (future), &error);
      g_assert_no_error (error);
      g_assert_true (r);
    }

  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path), ==, 0);

  g_clear_pointer (&fibers, g_ptr_array_unref);
  dex_clear (&start_promise);
  g_clear_object (&registry);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_cursor_close_releases_lease (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(DexFuture) pending_query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  GomCursor *held_cursors[GOM_SQLITE_POOL_MAX_LEASES] = {NULL};
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-cursor-close-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  query = stress_item_count_query_new (&error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_QUERY (query));

  for (guint i = 0; i < GOM_SQLITE_POOL_MAX_LEASES; i++)
    {
      held_cursors[i] = dex_await_object (gom_repository_query (repository, query), &error);
      g_assert_no_error (error);
      g_assert_true (GOM_IS_CURSOR (held_cursors[i]));
      g_assert_cmpuint (gom_cursor_get_count (held_cursors[i]), ==, 0);
      g_assert_true (dex_await (gom_cursor_close (held_cursors[i]), &error));
      g_assert_no_error (error);
      g_assert_true (dex_await (gom_cursor_close (held_cursors[i]), &error));
      g_assert_no_error (error);
    }

  pending_query = gom_repository_query (repository, query);
  cursor = dex_await_object (dex_future_with_timeout_msec (g_steal_pointer (&pending_query),
                                                           250),
                             &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await (gom_cursor_close (cursor), &error));
  g_assert_no_error (error);

  for (guint i = 0; i < GOM_SQLITE_POOL_MAX_LEASES; i++)
    g_clear_object (&held_cursors[i]);

  g_clear_object (&cursor);
  g_clear_object (&query);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_thread_pool_cancel_queued_shutdown (void)
{
  g_autoptr(DexThreadPool) pool = NULL;
  g_autoptr(DexPromise) started = NULL;
  g_autoptr(DexPromise) release = NULL;
  g_autoptr(DexPromise) queued_started = NULL;
  g_autoptr(DexPromise) queued_release = NULL;
  g_autoptr(DexFuture) first_future = NULL;
  g_autoptr(DexFuture) second_future = NULL;
  g_autoptr(DexFuture) close_future = NULL;
  g_autoptr(GError) error = NULL;
  PoolShutdownWorkerData *worker_data;
  PoolShutdownWorkerData *queued_worker_data;
  gboolean resolved;

  pool = dex_thread_pool_new (1);
  started = dex_promise_new ();
  release = dex_promise_new ();
  queued_started = dex_promise_new ();
  queued_release = dex_promise_new ();

  worker_data = g_new0 (PoolShutdownWorkerData, 1);
  worker_data->started = dex_ref (started);
  worker_data->release = dex_ref (DEX_FUTURE (release));

  first_future = dex_thread_pool_submit (pool,
                                         "[gom-sqlite-shutdown-first]",
                                         test_sqlite_pool_shutdown_blocking_thread,
                                         worker_data,
                                         (GDestroyNotify) pool_shutdown_worker_data_free);

  resolved = dex_await_boolean (dex_ref (DEX_FUTURE (started)), &error);
  g_assert_no_error (error);
  g_assert_true (resolved);

  queued_worker_data = g_new0 (PoolShutdownWorkerData, 1);
  queued_worker_data->started = dex_ref (queued_started);
  queued_worker_data->release = dex_ref (DEX_FUTURE (queued_release));

  second_future = dex_thread_pool_submit (pool,
                                          "[gom-sqlite-shutdown-queued]",
                                          test_sqlite_pool_shutdown_blocking_thread,
                                          queued_worker_data,
                                          (GDestroyNotify) pool_shutdown_worker_data_free);

  close_future = dex_thread_pool_close (pool, DEX_THREAD_POOL_SHUTDOWN_CANCEL_QUEUED);

  g_assert_false (dex_await (dex_future_with_timeout_msec (g_steal_pointer (&second_future),
                                                           1000),
                             &error));
  g_assert_error (error, DEX_ERROR, DEX_ERROR_SEMAPHORE_CLOSED);
  g_assert_cmpstr (error->message, ==, "Thread pool is closed");
  g_clear_error (&error);

  dex_promise_resolve_boolean (release, TRUE);

  g_assert_true (dex_await (dex_future_with_timeout_msec (g_steal_pointer (&first_future),
                                                          1000),
                            &error));
  g_assert_no_error (error);

  g_assert_true (dex_await (dex_future_with_timeout_msec (g_steal_pointer (&close_future),
                                                          1000),
                            &error));
  g_assert_no_error (error);
}

static void
test_sqlite_limiter_pending_acquire_rejects_on_close (void)
{
  g_autoptr(DexLimiter) limiter = NULL;
  g_autoptr(DexFuture) first_acquire = NULL;
  g_autoptr(DexFuture) second_acquire = NULL;
  g_autoptr(GError) error = NULL;
  gboolean acquired;

  limiter = dex_limiter_new (1);

  first_acquire = dex_limiter_acquire (limiter);
  acquired = dex_await_boolean (dex_ref (first_acquire), &error);
  g_assert_no_error (error);
  g_assert_true (acquired);

  second_acquire = dex_limiter_acquire (limiter);
  g_assert_true (dex_future_is_pending (second_acquire));

  dex_limiter_close (limiter);

  g_assert_false (dex_await (dex_future_with_timeout_msec (g_steal_pointer (&second_acquire),
                                                           1000),
                             &error));
  g_assert_error (error, DEX_ERROR, DEX_ERROR_SEMAPHORE_CLOSED);
  g_assert_cmpstr (error->message, ==, "Limiter is closed");
  g_clear_error (&error);

  dex_limiter_release (limiter);
}

static void
test_sqlite_session_begins_immediate_transaction (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GError) error = NULL;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-session-immediate-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_SESSION (session));

  test_sqlite_assert_external_begin_immediate_blocked (context.db_path);

  g_assert_true (dex_await (gom_session_rollback (session), &error));
  g_assert_no_error (error);

  g_clear_object (&session);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_session_holds_write_boundary (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(DexFuture) insert_future = NULL;
  g_autoptr(GError) error = NULL;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-session-boundary-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_SESSION (session));

  entity = stress_item_new (9000, 0);
  gom_entity_set_repository (entity, repository);

  insert_future = gom_entity_insert (entity);
  dex_await (dex_timeout_new_msec (25), NULL);
  g_assert_true (dex_future_is_pending (insert_future));

  query = stress_item_count_query_new (&error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_QUERY (query));

  cursor = dex_await_object (gom_session_query (session, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_cmpuint (gom_cursor_get_count (cursor), ==, 0);
  g_assert_true (dex_await (gom_cursor_close (cursor), &error));
  g_assert_no_error (error);

  g_assert_true (dex_await (gom_session_rollback (session), &error));
  g_assert_no_error (error);
  g_clear_object (&session);

  result = dex_await_object (g_steal_pointer (&insert_future), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path), ==, 1);

  g_clear_object (&result);
  g_clear_object (&cursor);
  g_clear_object (&entity);
  g_clear_object (&query);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_backend_busy_retry_timeout (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(DexFuture) insert_future = NULL;
  g_autoptr(GError) error = NULL;
  GPid lock_holder_pid = 0;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context,
                                           "gom-sqlite-busy-retry-test-XXXXXX",
                                           &error));
  g_assert_no_error (error);

  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  lock_holder_pid = test_sqlite_spawn_external_write_lock (context.db_path);

  entity = stress_item_new (7000, 0);
  gom_entity_set_repository (entity, repository);

  insert_future = gom_entity_insert (entity);
  result = dex_await_object (dex_future_with_timeout_msec (g_steal_pointer (&insert_future),
                                                           1000),
                             &error);
  g_assert_null (result);
  g_assert_nonnull (error);
  g_assert_true (g_error_matches (error, GOM_ERROR, GOM_ERROR_BUSY_TIMEOUT));
  g_clear_error (&error);

  test_sqlite_wait_for_external_write_lock (lock_holder_pid);

  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path), ==, 0);

  g_clear_object (&entity);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

static void
test_sqlite_repository_fiber_stress (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GPtrArray) fibers = NULL;
  g_autoptr(GError) error = NULL;
  TraceCounters baseline;

  test_trace_counters_snapshot (&baseline);

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-stress-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_create_stress_table (context.db_path);

  repository = test_sqlite_stress_open_repository (&context, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  fibers = test_future_array_new ();

  for (guint i = 0; i < STRESS_WORKER_COUNT; i++)
    {
      WorkerData *worker_data = g_new0 (WorkerData, 1);

      worker_data->repository = g_object_ref (repository);
      worker_data->worker_id = i;
      worker_data->iterations = STRESS_ITERATIONS;

      g_ptr_array_add (fibers,
                       dex_scheduler_spawn (NULL,
                                            0,
                                            stress_worker_fiber,
                                            worker_data,
                                            (GDestroyNotify) worker_data_free));
    }

  for (guint i = 0; i < fibers->len; i++)
    {
      DexFuture *future = g_ptr_array_index (fibers, i);

      gboolean r = dex_await (dex_ref (future), &error);
      g_assert_no_error (error);
      g_assert_true (r);
    }

  g_assert_cmpuint (test_sqlite_count_stress_items (context.db_path), ==, 0);

  g_clear_pointer (&fibers, g_ptr_array_unref);
  g_clear_object (&repository);
  test_trace_counters_assert_equal (&baseline);
}

int
main (int   argc,
      char *argv[])
{
  if (argc == 3 && g_strcmp0 (argv[1], SESSION_LOCK_HELPER_ARG) == 0)
    return test_sqlite_session_lock_helper (argv[2]);
  if (argc == 3 && g_strcmp0 (argv[1], SESSION_LOCK_HOLDER_HELPER_ARG) == 0)
    return test_sqlite_session_lock_holder (argv[2]);

  test_program_path = argv[0];

  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Sqlite/concurrent-repository-reads", test_sqlite_concurrent_repository_reads);
  _g_test_add_func ("/Gom/Sqlite/limiter-concurrency-no-retry", test_sqlite_limiter_concurrency_no_retry);
  _g_test_add_func ("/Gom/Sqlite/long-read-cursor-queues-writes", test_sqlite_long_read_cursor_queues_writes);
  _g_test_add_func ("/Gom/Sqlite/migration-open-contention", test_sqlite_migration_open_contention);
  _g_test_add_func ("/Gom/Sqlite/cursor-close-releases-lease", test_sqlite_cursor_close_releases_lease);
  _g_test_add_func ("/Gom/Sqlite/thread-pool-cancel-queued-shutdown", test_sqlite_thread_pool_cancel_queued_shutdown);
  _g_test_add_func ("/Gom/Sqlite/limiter-pending-acquire-rejects-on-close", test_sqlite_limiter_pending_acquire_rejects_on_close);
  _g_test_add_func ("/Gom/Sqlite/session-begins-immediate-transaction", test_sqlite_session_begins_immediate_transaction);
  _g_test_add_func ("/Gom/Sqlite/session-holds-write-boundary", test_sqlite_session_holds_write_boundary);
  _g_test_add_func ("/Gom/Sqlite/backend-busy-retry-timeout", test_sqlite_backend_busy_retry_timeout);
  _g_test_add_func ("/Gom/Sqlite/repository-fiber-stress", test_sqlite_repository_fiber_stress);

  return g_test_run ();
}
