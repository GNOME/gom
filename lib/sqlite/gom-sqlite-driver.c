/* gom-sqlite-driver.c
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

#include <gmodule.h>
#include <errno.h>
#include <sqlite3mc.h>
#include <string.h>

#include "gom-mutation.h"
#include "gom-cursor-private.h"
#include "gom-deletion-private.h"
#include "gom-driver-private.h"
#include "gom-driver-options.h"
#include "gom-entity-private.h"
#include "gom-expression-private.h"
#include "gom-insertion-private.h"
#include "gom-mutation-result-private.h"
#include "gom-meta-private.h"
#include "gom-record-private.h"
#include "gom-update-private.h"
#include "gom-repository-private.h"
#include "gom-schema-private.h"
#include "gom-sqlite-lease-private.h"
#include "gom-sqlite-connection-private.h"
#include "gom-sqlite-cursor-private.h"
#include "gom-sqlite-driver-private.h"
#include "gom-sqlite-session-private.h"
#include "gom-sqlite-pool-private.h"
#include "gom-sqlite-statement-private.h"
#include "gom-query-private.h"
#include "gom-registry-diff-private.h"
#include "gom-vector.h"
#include "gom-ordering.h"
#include "gom-trace-private.h"
#include "gom-util-private.h"

#define GOM_SQLITE_LOCK_RETRY_TIMEOUT_MS 250
#define GOM_SQLITE_LOCK_RETRY_USEC       1000
#define GOM_SQLITE_LOCK_RETRY_MAX_USEC   10000

/**
 * GomSqliteDriver:
 *
 * SQLite backend for [class@Gom.Driver].
 *
 * `GomSqliteDriver` translates Gom query, mutation, schema, and migration
 * models into SQLite statements and executes them through a pooled
 * `sqlite3` connection. It is the backend used by [ctor@Gom.SqliteDriver.new]
 * and by [ctor@Gom.Repository.new] when the repository is configured with a
 * SQLite URI.
 *
 * ## Implementation Notes
 *
 * - Identifiers are quoted per path segment. Dotted paths are preserved, and
 *   `rowid` is passed through unquoted so custom SQL can still refer to
 *   SQLite's native row identifier.
 * - Target relations can be resolved from either an entity type or an
 *   explicit relation name. When both are supplied, the driver validates that
 *   they refer to the same mapped table.
 * - Query and mutation expressions are compiled from [class@Gom.Expression]
 *   trees. Entity-scoped expressions resolve property names and field names
 *   against the registry metadata, while unmapped properties are rejected.
 * - When a query touches an indexed search property, the driver transparently
 *   switches to the companion FTS5 table and joins it as `fts` against the
 *   base table as `t`.
 * - The driver creates and manages FTS5 content tables named
 *   `<table>_fts`, with trigger helpers named `<table>_fts_ai`,
 *   `<table>_fts_au`, and `<table>_fts_ad`.
 * - Schema description uses `sqlite_master`, `PRAGMA table_info()`,
 *   `PRAGMA index_list()`, and `PRAGMA index_info()`. This underpins the
 *   public [method@Gom.Repository.describe_relation] and
 *   [method@Gom.Repository.list_relations] APIs, which are the supported way
 *   for applications to inspect or align custom SQL with the driver's schema
 *   model.
 * - Migrations are transactional and may either add columns or rebuild a
 *   table when SQLite's limited `ALTER TABLE` support is not enough.
 *   Incompatible changes that introduce required columns without defaults are
 *   rejected.
 * - Raw SQL scripts are executed through the driver's backend hook inside a
 *   `BEGIN IMMEDIATE TRANSACTION`/`COMMIT` wrapper. This is used by
 *   [class@Gom.SqlMigration] and by repository initialization paths that need
 *   to apply SQL directly. Empty scripts are no-ops, and NUL bytes are
 *   rejected.
 * - Query execution can also operate inside a read transaction when row counts
 *   are requested, so the count and result cursor observe the same snapshot.
 *
 * ## Type and Value Mapping
 *
 * SQLite column types are chosen from the entity property type and any
 * byte-transform hook:
 *
 * - booleans, integers, enums, and flags are stored as `INTEGER`
 * - floating-point values are stored as `REAL`
 * - strings, `GType`, `GDateTime`, and `GStrv` values are stored as `TEXT`
 * - byte arrays and transformed properties are stored as `BLOB`
 *
 * `GDateTime` values are serialized as ISO-8601 text and parsed back by the
 * cursor implementation. `GStrv` values use a newline-delimited escape format
 * where `\n` and `\\` are escaped and empty elements are preserved explicitly;
 * this round-trips through [method@Gom.Cursor.get_column] and
 * [method@Gom.Cursor.materialize].
 *
 * ## Mutation Result Contract
 *
 * Insertions return the affected row count plus `last_insert_rowid()` as the
 * `rowid` column. Updates and deletions return the affected row count and
 * `NULL` for `rowid`.
 *
 * These details are useful when an application needs to write custom SQL that
 * follows Gom's SQLite conventions or integrate tightly with the driver's
 * schema and entity layout.
 */
struct _GomSqliteDriver
{
  GomDriver      parent_instance;
  GomSqlitePool *pool;
  DexLimiter    *write_limiter;
  char          *uri;
  GBytes        *encryption_key;
};

struct _GomSqliteDriverClass
{
  GomDriverClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomSqliteDriver, gom_sqlite_driver, GOM_TYPE_DRIVER)

enum
{
  PROP_0,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct
{
  GValue value;
  guint  has_value : 1;
} GomSqliteBinding;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  GomQuery            *query;
  GomRepository       *repository;
  GomCursorFlags       flags;
  guint                transaction_active : 1;
} GomSqliteQueryTask;

typedef struct
{
  GomQuery       *query;
  GomRepository  *repository;
  GomCursorFlags  flags;
} GomSqliteQueryRequest;

typedef struct
{
  GomRegistry *current;
  GomRegistry *next;
} GomSqliteMigrateRequest;

typedef struct
{
  GBytes *script;
} GomSqliteExecuteRequest;

typedef struct
{
  GomMutation *mutation;
  GomRegistry *registry;
} GomSqliteMutationRequest;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  GomMutation         *mutation;
  GomRegistry         *registry;
} GomSqliteMutationTask;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  char                *relation;
  GomRegistry         *registry;
} GomSqliteDescribeTask;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  GomRegistry         *registry;
} GomSqliteListRelationsTask;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  GomRegistry         *current;
  GomRegistry         *next;
} GomSqliteMigrateTask;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  GBytes              *script;
} GomSqliteExecuteTask;

typedef struct
{
  GomRepository *repository;
  DexLimiter    *write_limiter;
} GomSqliteSessionRequest;

typedef struct
{
  GomSqliteLeaseState *lease_state;
  GomRepository       *repository;
  DexLimiter          *write_limiter;
} GomSqliteSessionTask;

typedef enum
{
  GOM_SQLITE_WRITE_MUTATE,
  GOM_SQLITE_WRITE_MIGRATE,
  GOM_SQLITE_WRITE_EXECUTE_SQL,
  GOM_SQLITE_WRITE_REKEY,
} GomSqliteWriteOperation;

typedef struct
{
  GomSqliteDriver         *driver;
  GomSqliteWriteOperation  operation;
  union
  {
    GomSqliteMutationRequest *mutation;
    GomSqliteMigrateRequest  *migrate;
    GomSqliteExecuteRequest  *execute;
    GBytes                   *rekey;
  } request;
} GomSqliteWriteState;

typedef struct
{
  GomSqlitePool           *pool;
  GomSqliteSessionRequest *request;
} GomSqliteBeginSessionState;

typedef struct
{
  GomSqliteDriver     *driver;
  GomSqliteLeaseState *lease_state;
  GBytes              *encryption_key;
} GomSqliteRekeyTask;

typedef struct
{
  const char    *field_prefix;
  const char    *fts_prefix;
  GomEntitySpec *entity;
} GomSqliteExpressionContext;

static gboolean   gom_sqlite_driver_append_expression_with_context (GomExpression                     *expression,
                                                                    GString                           *sql,
                                                                    GPtrArray                         *bindings,
                                                                    GError                           **error,
                                                                    const GomSqliteExpressionContext  *context);
static char      *gom_sqlite_driver_resolve_target_relation        (GomRegistry                       *registry,
                                                                    GType                              target_entity_type,
                                                                    const char                        *target_relation,
                                                                    const char                        *operation_name,
                                                                    const GomEntitySpec              **out_entity,
                                                                    GError                           **error);
static DexFuture *gom_sqlite_driver_query_thread                   (gpointer                           user_data);
static DexFuture *gom_sqlite_driver_mutate_thread                  (gpointer                           user_data);
static DexFuture *gom_sqlite_driver_begin_session_thread           (gpointer                           user_data);
static DexFuture *gom_sqlite_driver_mutate_cb                      (DexFuture                         *completed,
                                                                    gpointer                           user_data);
static DexFuture *gom_sqlite_driver_migrate_cb                     (DexFuture                         *completed,
                                                                    gpointer                           user_data);
static DexFuture *gom_sqlite_driver_execute_sql_cb                 (DexFuture                         *completed,
                                                                    gpointer                           user_data);
static DexFuture *gom_sqlite_driver_rekey_cb                        (DexFuture                        *completed,
                                                                    gpointer                           user_data);
static DexFuture *gom_sqlite_driver_rekey_thread                    (gpointer                          user_data);
static void       gom_sqlite_rekey_task_free                        (gpointer                          data);
static gboolean   gom_sqlite_driver_verify_sqlite_access            (sqlite3                          *db,
                                                                     GError                          **error);
static gboolean   gom_sqlite_driver_verify_integrity                (sqlite3                          *db,
                                                                     GError                          **error);

static GomSqliteBinding *
gom_sqlite_binding_new (const GValue *value)
{
  GomSqliteBinding *binding = g_new0 (GomSqliteBinding, 1);

  if (value != NULL && G_VALUE_TYPE (value) != G_TYPE_INVALID)
    {
      g_value_init (&binding->value, G_VALUE_TYPE (value));
      g_value_copy (value, &binding->value);
      binding->has_value = TRUE;
    }

  return binding;
}

static void
gom_sqlite_binding_free (gpointer data)
{
  GomSqliteBinding *binding = data;

  if (binding->has_value)
    g_value_unset (&binding->value);

  g_free (binding);
}

static void
gom_sqlite_query_task_free (gpointer data)
{
  GomSqliteQueryTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_object (&task->query);
  g_clear_object (&task->repository);
  g_free (task);
}

static void
gom_sqlite_query_request_free (gpointer data)
{
  GomSqliteQueryRequest *request = data;

  g_clear_object (&request->query);
  g_clear_object (&request->repository);
  g_free (request);
}

DexFuture *
gom_sqlite_driver_query_on_lease (GomSqliteLeaseState *lease_state,
                                  GomRepository       *repository,
                                  GomQuery            *query,
                                  GomCursorFlags       flags,
                                  gboolean             transaction_active)
{
  GomSqliteQueryTask *task;

  g_return_val_if_fail (lease_state != NULL, NULL);
  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  task = g_new0 (GomSqliteQueryTask, 1);
  task->lease_state = gom_sqlite_lease_state_ref (lease_state);
  task->query = g_object_ref (query);
  task->repository = g_object_ref (repository);
  task->flags = flags;
  task->transaction_active = !!transaction_active;

  return gom_sqlite_lease_state_invoke (lease_state,
                                        "[gom-sqlite-query]",
                                        gom_sqlite_driver_query_thread,
                                        task,
                                        gom_sqlite_query_task_free);
}

static void
gom_sqlite_migrate_request_free (gpointer data)
{
  GomSqliteMigrateRequest *request = data;

  g_clear_object (&request->current);
  g_clear_object (&request->next);
  g_free (request);
}

static void
gom_sqlite_execute_request_free (gpointer data)
{
  GomSqliteExecuteRequest *request = data;

  g_clear_pointer (&request->script, g_bytes_unref);
  g_free (request);
}

static void
gom_sqlite_mutation_request_free (gpointer data)
{
  GomSqliteMutationRequest *request = data;

  g_clear_object (&request->mutation);
  g_clear_object (&request->registry);
  g_free (request);
}

static void
gom_sqlite_mutation_task_free (gpointer data)
{
  GomSqliteMutationTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_object (&task->mutation);
  g_clear_object (&task->registry);
  g_free (task);
}

DexFuture *
gom_sqlite_driver_mutate_on_lease (GomSqliteLeaseState *lease_state,
                                   GomRegistry         *registry,
                                   GomMutation         *mutation)
{
  GomSqliteMutationTask *task;

  g_return_val_if_fail (lease_state != NULL, NULL);
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (GOM_IS_MUTATION (mutation), NULL);

  task = g_new0 (GomSqliteMutationTask, 1);
  task->lease_state = gom_sqlite_lease_state_ref (lease_state);
  task->mutation = g_object_ref (mutation);
  task->registry = g_object_ref (registry);

  return gom_sqlite_lease_state_invoke (lease_state,
                                        "[gom-sqlite-mutate]",
                                        gom_sqlite_driver_mutate_thread,
                                        task,
                                        gom_sqlite_mutation_task_free);
}

static void
gom_sqlite_describe_task_free (gpointer data)
{
  GomSqliteDescribeTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_pointer (&task->relation, g_free);
  g_clear_object (&task->registry);
  g_free (task);
}

static void
gom_sqlite_list_relations_task_free (gpointer data)
{
  GomSqliteListRelationsTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_object (&task->registry);
  g_free (task);
}

static void
gom_sqlite_migrate_task_free (gpointer data)
{
  GomSqliteMigrateTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_object (&task->current);
  g_clear_object (&task->next);
  g_free (task);
}

static void
gom_sqlite_execute_task_free (gpointer data)
{
  GomSqliteExecuteTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_pointer (&task->script, g_bytes_unref);
  g_free (task);
}

static void
gom_sqlite_session_request_free (gpointer data)
{
  GomSqliteSessionRequest *request = data;

  g_clear_object (&request->repository);
  dex_clear (&request->write_limiter);
  g_free (request);
}

static void
gom_sqlite_session_task_free (gpointer data)
{
  GomSqliteSessionTask *task = data;

  if (task->lease_state != NULL)
    gom_sqlite_lease_state_unref (task->lease_state);
  g_clear_object (&task->repository);
  dex_clear (&task->write_limiter);
  g_free (task);
}

static void
gom_sqlite_begin_session_state_free (gpointer data)
{
  GomSqliteBeginSessionState *state = data;

  if (state == NULL)
    return;

  g_clear_object (&state->pool);
  g_clear_pointer (&state->request, gom_sqlite_session_request_free);
  g_free (state);
}

static void
gom_sqlite_write_state_free (gpointer data)
{
  GomSqliteWriteState *state = data;

  if (state == NULL)
    return;

  g_clear_object (&state->driver);

  switch (state->operation)
    {
    case GOM_SQLITE_WRITE_MUTATE:
      g_clear_pointer (&state->request.mutation, gom_sqlite_mutation_request_free);
      break;

    case GOM_SQLITE_WRITE_MIGRATE:
      g_clear_pointer (&state->request.migrate, gom_sqlite_migrate_request_free);
      break;

    case GOM_SQLITE_WRITE_EXECUTE_SQL:
      g_clear_pointer (&state->request.execute, gom_sqlite_execute_request_free);
      break;

    case GOM_SQLITE_WRITE_REKEY:
      g_clear_pointer (&state->request.rekey, g_bytes_unref);
      break;

    default:
      g_assert_not_reached ();
    }

  g_free (state);
}

static DexFuture *
gom_sqlite_driver_release_write_limiter_cb (DexFuture *completed,
                                            gpointer   user_data)
{
  DexLimiter *write_limiter = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (DEX_IS_LIMITER (write_limiter));

  value = dex_future_get_value (completed, &error);
  dex_limiter_release (write_limiter);

  if (value == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_value (value);
}

static DexFuture *
gom_sqlite_driver_write_acquired_cb (DexFuture *completed,
                                     gpointer   user_data)
{
  GomSqliteWriteState *state = user_data;
  g_autoptr(GError) error = NULL;
  DexFuture *future = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_DRIVER (state->driver));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  switch (state->operation)
    {
    case GOM_SQLITE_WRITE_MUTATE:
      future = dex_future_then (gom_sqlite_pool_acquire (state->driver->pool),
                                gom_sqlite_driver_mutate_cb,
                                g_steal_pointer (&state->request.mutation),
                                gom_sqlite_mutation_request_free);
      break;

    case GOM_SQLITE_WRITE_MIGRATE:
      future = dex_future_then (gom_sqlite_pool_acquire (state->driver->pool),
                                gom_sqlite_driver_migrate_cb,
                                g_steal_pointer (&state->request.migrate),
                                gom_sqlite_migrate_request_free);
      break;

    case GOM_SQLITE_WRITE_EXECUTE_SQL:
      future = dex_future_then (gom_sqlite_pool_acquire (state->driver->pool),
                                gom_sqlite_driver_execute_sql_cb,
                                g_steal_pointer (&state->request.execute),
                                gom_sqlite_execute_request_free);
      break;

    case GOM_SQLITE_WRITE_REKEY:
      {
        GomSqliteRekeyTask *rekey_task;

        rekey_task = g_new0 (GomSqliteRekeyTask, 1);
        rekey_task->driver = g_object_ref (state->driver);
        rekey_task->encryption_key = g_steal_pointer (&state->request.rekey);
        future = dex_future_then (gom_sqlite_pool_acquire (state->driver->pool),
                                  gom_sqlite_driver_rekey_cb,
                                  rekey_task,
                                  NULL);
      }
      break;

    default:
      g_assert_not_reached ();
    }

  return dex_future_finally (future,
                             gom_sqlite_driver_release_write_limiter_cb,
                             dex_ref (state->driver->write_limiter),
                             dex_unref);
}

static void
gom_sqlite_rekey_task_free (gpointer data)
{
  GomSqliteRekeyTask *task = data;

  if (task == NULL)
    return;

  g_clear_object (&task->driver);
  g_clear_pointer (&task->encryption_key, g_bytes_unref);
  gom_sqlite_lease_state_unref (task->lease_state);
  g_free (task);
}

static gboolean
gom_sqlite_driver_verify_sqlite_access (sqlite3  *db,
                                        GError  **error)
{
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert (db != NULL);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT name FROM sqlite_master WHERE type='table'",
                           -1,
                           &stmt,
                           NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "Failed to verify SQLite database access: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  for (;;)
    {
      rc = gom_sqlite_driver_step (stmt, "verify SQLite database access", error);
      if (rc != SQLITE_ROW)
        break;
    }

  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "Failed to verify SQLite database access: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_verify_integrity (sqlite3  *db,
                                    GError  **error)
{
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert (db != NULL);

  rc = sqlite3_prepare_v2 (db,
                           "PRAGMA integrity_check",
                           -1,
                           &stmt,
                           NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "Failed to prepare integrity check: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  rc = gom_sqlite_driver_step (stmt, "verify SQLite integrity", error);
  if (rc != SQLITE_ROW)
    {
      sqlite3_finalize (stmt);

      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "Failed to verify SQLite integrity: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  {
    const char *status = (const char *)sqlite3_column_text (stmt, 0);

    if (status == NULL || g_strcmp0 (status, "ok") != 0)
      {
        sqlite3_finalize (stmt);
        g_set_error (error,
                     GOM_ERROR,
                     GOM_ERROR_METADATA_READ_FAILED,
                     "SQLite integrity check failed: %s",
                     status != NULL ? status : "unknown");
        return FALSE;
      }
  }

  rc = gom_sqlite_driver_step (stmt, "verify SQLite integrity", error);
  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "SQLite integrity check did not finish cleanly: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  return TRUE;
}

static DexFuture *
gom_sqlite_driver_rekey_cb (DexFuture *completed,
                            gpointer   user_data)
{
  GomSqliteRekeyTask *task = user_data;
  const GValue *value;
  DexFuture *future;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (task != NULL);

  value = dex_future_get_value (completed, NULL);
  if (value == NULL || !G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE))
    {
      gom_sqlite_rekey_task_free (task);

      if (value == NULL)
        {
          g_autoptr(GError) error = NULL;

          if (dex_future_get_value (completed, &error) == NULL)
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_FAILED,
                                          "Failed to acquire write lease for rekey: %s",
                                          error != NULL ? error->message : "unknown");
        }

      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to acquire write lease for rekey");
    }

  task->lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));

  future = gom_sqlite_lease_state_invoke (task->lease_state,
                                           "[gom-sqlite-rekey]",
                                           gom_sqlite_driver_rekey_thread,
                                           task,
                                           gom_sqlite_rekey_task_free);

  return future;
}

static DexFuture *
gom_sqlite_driver_rekey_thread (gpointer user_data)
{
  GomSqliteRekeyTask *task = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  const guint8 *new_key_data = NULL;
  gsize new_key_len = 0;
  int new_key_len_int;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();
  sqlite3_stmt *stmt;

  g_assert (task != NULL);
  g_assert (task->driver != NULL);
  g_assert (task->lease_state != NULL);

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  if (!sqlite3_get_autocommit (db))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_BUSY,
                                  "Cannot rekey while a transaction is active");

  for (stmt = sqlite3_next_stmt (db, NULL); stmt != NULL; stmt = sqlite3_next_stmt (db, stmt))
    {
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_BUSY,
                                    "Cannot rekey while SQLite statements are active");
    }

  gom_sqlite_pool_clear_idle (task->driver->pool);

  if (!gom_sqlite_driver_exec_sql (db,
                                   "PRAGMA wal_checkpoint(FULL)",
                                   "checkpoint SQLite WAL before rekey",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_sqlite_driver_exec_sql (db,
                                   "PRAGMA journal_mode = DELETE",
                                   "disable SQLite WAL before rekey",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (task->encryption_key != NULL)
    {
      new_key_data = g_bytes_get_data (task->encryption_key, &new_key_len);

      if (new_key_len == 0)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_ARGUMENT,
                                      "Encryption key is empty");

      if (new_key_len > G_MAXINT)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_ARGUMENT,
                                      "Encryption key exceeds maximum supported length");

      new_key_len_int = (int)new_key_len;
    }
  else
    {
      new_key_len_int = 0;
    }

  if (sqlite3_rekey_v2 (db, "main", new_key_data, new_key_len_int) != SQLITE_OK)
    {
      gom_sqlite_driver_exec_sql (db,
                                  "PRAGMA journal_mode = WAL",
                                  "restore SQLite WAL after failed rekey",
                                  NULL);

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_FAILED,
                                    "Failed to rekey SQLite database: %s",
                                    sqlite3_errmsg (db));
    }

  g_clear_pointer (&task->driver->encryption_key, g_bytes_unref);
  if (task->encryption_key != NULL)
    task->driver->encryption_key = g_bytes_ref (task->encryption_key);

  gom_sqlite_pool_set_encryption_key (task->driver->pool, task->driver->encryption_key);

  if (!gom_sqlite_driver_verify_sqlite_access (db, &error))
    {
      gom_sqlite_driver_exec_sql (db,
                                  "PRAGMA journal_mode = WAL",
                                  "restore SQLite WAL after rekey",
                                  NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!gom_sqlite_driver_verify_integrity (db, &error))
    {
      gom_sqlite_driver_exec_sql (db,
                                  "PRAGMA journal_mode = WAL",
                                  "restore SQLite WAL after rekey",
                                  NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!gom_sqlite_driver_exec_sql (db,
                                   "PRAGMA journal_mode = WAL",
                                   "restore SQLite WAL after rekey",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  GOM_TRACE_END_MARK (start_time,
                      "SQLite",
                      "rekey",
                      "encrypted database");

  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_driver_run_write_state (GomSqliteWriteState *state)
{
  GomSqliteDriver *self;

  dex_return_error_if_fail (state != NULL);
  dex_return_error_if_fail (GOM_IS_SQLITE_DRIVER (state->driver));

  self = state->driver;

  return dex_future_then (dex_limiter_acquire (self->write_limiter),
                          gom_sqlite_driver_write_acquired_cb,
                          state,
                          gom_sqlite_write_state_free);
}

typedef struct
{
  char  *field;
  char  *sql_type;
  char  *ref_table;
  char  *ref_field;
  guint  nonnull : 1;
  guint  unique : 1;
  guint  primary_key : 1;
} GomSqliteColumnDef;

static GomSqliteColumnDef *
gom_sqlite_column_def_new (const char *field,
                           const char *sql_type,
                           gboolean    nonnull,
                           gboolean    unique,
                           gboolean    primary_key,
                           const char *ref_table,
                           const char *ref_field)
{
  GomSqliteColumnDef *def;

  g_assert (field != NULL);
  g_assert (sql_type != NULL);

  def = g_new0 (GomSqliteColumnDef, 1);
  def->field = g_strdup (field);
  def->sql_type = g_strdup (sql_type);
  def->nonnull = !!nonnull;
  def->unique = !!unique;
  def->primary_key = !!primary_key;
  def->ref_table = g_strdup (ref_table);
  def->ref_field = g_strdup (ref_field);

  return def;
}

static void
gom_sqlite_column_def_free (gpointer data)
{
  GomSqliteColumnDef *def = data;

  if (def == NULL)
    return;

  g_clear_pointer (&def->field, g_free);
  g_clear_pointer (&def->sql_type, g_free);
  g_clear_pointer (&def->ref_table, g_free);
  g_clear_pointer (&def->ref_field, g_free);
  g_free (def);
}

static gboolean
gom_sqlite_column_def_equal (const GomSqliteColumnDef *a,
                             const GomSqliteColumnDef *b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  return g_strcmp0 (a->field, b->field) == 0 &&
         g_strcmp0 (a->sql_type, b->sql_type) == 0 &&
         g_strcmp0 (a->ref_table, b->ref_table) == 0 &&
         g_strcmp0 (a->ref_field, b->ref_field) == 0 &&
         a->nonnull == b->nonnull &&
         a->unique == b->unique &&
         a->primary_key == b->primary_key;
}

static char *
gom_sqlite_driver_quote_identifier (const char *identifier)
{
  g_assert (identifier != NULL);

  return sqlite3_mprintf ("\"%w\"", identifier);
}

static void gom_sqlite_driver_append_quoted_identifier (GString    *sql,
                                                        const char *identifier);

static void
gom_sqlite_driver_append_quoted_identifier_path (GString    *sql,
                                                 const char *identifier)
{
  const char *segment_start;
  const char *segment_end;

  g_assert (sql != NULL);
  g_assert (identifier != NULL);

  segment_start = identifier;
  segment_end = identifier;

  while (*segment_end != '\0')
    {
      if (*segment_end == '.')
        {
          if (segment_end > segment_start)
            {
              g_autofree char *segment = g_strndup (segment_start, segment_end - segment_start);
              gom_sqlite_driver_append_quoted_identifier (sql, segment);
            }

          g_string_append_c (sql, '.');
          segment_start = segment_end + 1;
        }

      segment_end++;
    }

  if (segment_end > segment_start)
    {
      g_autofree char *segment = g_strndup (segment_start, segment_end - segment_start);

      if (g_strcmp0 (segment, "rowid") == 0)
        g_string_append (sql, segment);
      else
        gom_sqlite_driver_append_quoted_identifier (sql, segment);
    }
}

static void
gom_sqlite_driver_append_quoted_identifier (GString    *sql,
                                            const char *identifier)
{
  char *quoted;

  g_assert (sql != NULL);
  g_assert (identifier != NULL);

  quoted = gom_sqlite_driver_quote_identifier (identifier);
  g_assert (quoted != NULL);
  g_string_append (sql, quoted);
  sqlite3_free (quoted);
}

static gboolean
gom_sqlite_driver_result_is_lock_contention (int rc)
{
  rc &= 0xff;

  return rc == SQLITE_BUSY || rc == SQLITE_LOCKED;
}

static gint64
gom_sqlite_driver_lock_retry_deadline (void)
{
  return g_get_monotonic_time () + (GOM_SQLITE_LOCK_RETRY_TIMEOUT_MS * G_TIME_SPAN_MILLISECOND);
}

static guint
gom_sqlite_driver_lock_retry_delay_usec (guint attempt)
{
  guint64 delay_usec = GOM_SQLITE_LOCK_RETRY_USEC;

  for (guint i = 0; i < attempt && delay_usec < GOM_SQLITE_LOCK_RETRY_MAX_USEC; i++)
    delay_usec *= 2;

  return (guint)MIN (delay_usec, (guint64)GOM_SQLITE_LOCK_RETRY_MAX_USEC);
}

static gboolean
gom_sqlite_driver_wait_for_lock (const char *action,
                                 guint       attempt,
                                 gint64      deadline)
{
  gint64 now;
  gint64 remaining_usec;
  guint delay_usec;

  g_assert (action != NULL);

  now = g_get_monotonic_time ();
  if (now >= deadline)
    return FALSE;

  remaining_usec = deadline - now;
  delay_usec = gom_sqlite_driver_lock_retry_delay_usec (attempt);
  delay_usec = (guint)MIN ((gint64)delay_usec, remaining_usec);

  GOM_TRACE_MARK ("SQLite",
                  "lock-wait",
                  "%s attempt=%u delay-usec=%u",
                  action,
                  attempt + 1,
                  delay_usec);
  g_usleep (delay_usec);

  return TRUE;
}

static void
gom_sqlite_driver_set_lock_error (sqlite3     *db,
                                  const char  *action,
                                  GError     **error)
{
  g_assert (db != NULL);
  g_assert (action != NULL);

  g_set_error (error,
               GOM_ERROR,
               GOM_ERROR_BUSY_TIMEOUT,
               "SQLite %s timed out after %u ms waiting for SQLITE_BUSY/SQLITE_LOCKED: %s",
               action,
               GOM_SQLITE_LOCK_RETRY_TIMEOUT_MS,
               sqlite3_errmsg (db));
}

int
gom_sqlite_driver_step (sqlite3_stmt  *stmt,
                        const char    *action,
                        GError       **error)
{
  sqlite3 *db;
  gint64 deadline;
  int rc = SQLITE_OK;

  g_return_val_if_fail (stmt != NULL, SQLITE_MISUSE);
  g_return_val_if_fail (action != NULL, SQLITE_MISUSE);

  db = sqlite3_db_handle (stmt);
  deadline = gom_sqlite_driver_lock_retry_deadline ();

  for (guint attempt = 0;; attempt++)
    {
      rc = sqlite3_step (stmt);
      if (!gom_sqlite_driver_result_is_lock_contention (rc))
        return rc;

      if (!gom_sqlite_driver_wait_for_lock (action, attempt, deadline))
        break;
    }

  gom_sqlite_driver_set_lock_error (db, action, error);
  return rc;
}

static int
gom_sqlite_driver_prepare (sqlite3       *db,
                           const char    *sql,
                           sqlite3_stmt **stmt,
                           const char    *action,
                           GError       **error)
{
  gint64 deadline;
  int rc = SQLITE_OK;

  g_assert (db != NULL);
  g_assert (sql != NULL);
  g_assert (stmt != NULL);
  g_assert (action != NULL);

  *stmt = NULL;
  deadline = gom_sqlite_driver_lock_retry_deadline ();

  for (guint attempt = 0;; attempt++)
    {
      rc = sqlite3_prepare_v2 (db, sql, -1, stmt, NULL);
      if (!gom_sqlite_driver_result_is_lock_contention (rc))
        return rc;

      if (*stmt != NULL)
        {
          sqlite3_finalize (*stmt);
          *stmt = NULL;
        }

      if (!gom_sqlite_driver_wait_for_lock (action, attempt, deadline))
        break;
    }

  gom_sqlite_driver_set_lock_error (db, action, error);
  return rc;
}

static gboolean
gom_sqlite_driver_exec_sql_full (sqlite3     *db,
                                 const char  *sql,
                                 const char  *action,
                                 gboolean     retry_locked,
                                 GError     **error)
{
  char *errmsg = NULL;
  gint64 deadline = 0;
  int rc;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (db != NULL);
  g_assert (sql != NULL);
  g_assert (action != NULL);

  if (retry_locked)
    deadline = gom_sqlite_driver_lock_retry_deadline ();

  GOM_TRACE_MARK ("SQLite", "execute", "%s: %s", action, sql);
  for (guint attempt = 0;; attempt++)
    {
      rc = sqlite3_exec (db, sql, NULL, NULL, &errmsg);
      if (rc == SQLITE_OK)
        {
          GOM_TRACE_END_MARK (start_time, "SQLite", "execute", "%s", action);
          return TRUE;
        }

      if (!gom_sqlite_driver_result_is_lock_contention (rc))
        break;

      if (!retry_locked)
        break;

      g_clear_pointer (&errmsg, sqlite3_free);
      if (!gom_sqlite_driver_wait_for_lock (action, attempt, deadline))
        break;
    }

  if (retry_locked && gom_sqlite_driver_result_is_lock_contention (rc))
    {
      gom_sqlite_driver_set_lock_error (db, action, error);
      GOM_TRACE_END_MARK (start_time, "SQLite", "execute", "locked: %s", action);
      g_clear_pointer (&errmsg, sqlite3_free);
      return FALSE;
    }

  g_set_error (error,
               GOM_ERROR,
               GOM_ERROR_FAILED,
               "Failed to %s: %s",
               action,
               errmsg != NULL ? errmsg : sqlite3_errmsg (db));
  GOM_TRACE_END_MARK (start_time, "SQLite", "execute", "failed: %s", action);
  g_clear_pointer (&errmsg, sqlite3_free);
  return FALSE;
}

gboolean
gom_sqlite_driver_exec_sql (sqlite3     *db,
                            const char  *sql,
                            const char  *action,
                            GError     **error)
{
  return gom_sqlite_driver_exec_sql_full (db, sql, action, TRUE, error);
}

static const char *
gom_sqlite_driver_value_type_to_sql_type (GType    value_type,
                                          gboolean has_transform)
{
  if (has_transform)
    return "BLOB";

  if (value_type == G_TYPE_BOOLEAN ||
      value_type == G_TYPE_INT ||
      value_type == G_TYPE_UINT ||
      value_type == G_TYPE_INT64 ||
      value_type == G_TYPE_UINT64 ||
      g_type_is_a (value_type, G_TYPE_ENUM) ||
      g_type_is_a (value_type, G_TYPE_FLAGS))
    return "INTEGER";

  if (value_type == G_TYPE_DOUBLE || value_type == G_TYPE_FLOAT)
    return "REAL";

  if (value_type == G_TYPE_STRING)
    return "TEXT";

  if (value_type == G_TYPE_GTYPE)
    return "TEXT";

  if (value_type == G_TYPE_DATE_TIME)
    return "TEXT";

  if (value_type == G_TYPE_STRV)
    return "TEXT";

  if (value_type == G_TYPE_BYTES)
    return "BLOB";

  return "BLOB";
}

static GType
gom_sqlite_driver_get_property_value_type (GomEntitySpec *entity,
                                           const char    *property_name)
{
  g_autoptr(GTypeClass) klass = NULL;
  GParamSpec *pspec;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (property_name != NULL);

  if (!(klass = g_type_class_ref (gom_entity_spec_get_entity_type (entity))))
    return G_TYPE_INVALID;

  if (!(pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), property_name)))
    return G_TYPE_INVALID;

  return G_PARAM_SPEC_VALUE_TYPE (pspec);
}

static gboolean
gom_sqlite_driver_property_has_transform (GomEntitySpec *entity,
                                          const char    *property_name)
{
  g_autoptr(GTypeClass) klass = NULL;
  GomEntityPropertyInfo *prop_info;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (property_name != NULL);

  if (!(klass = g_type_class_ref (gom_entity_spec_get_entity_type (entity))))
    return FALSE;

  prop_info = _gom_entity_class_get_property (GOM_ENTITY_CLASS (klass), property_name, FALSE);
  return prop_info != NULL && prop_info->to_bytes_func != NULL;
}

static gboolean
gom_sqlite_driver_append_column_definition (GString                  *sql,
                                            const GomSqliteColumnDef *column)
{
  g_assert (sql != NULL);
  g_assert (column != NULL);
  g_assert (column->field != NULL);
  g_assert (column->sql_type != NULL);

  gom_sqlite_driver_append_quoted_identifier (sql, column->field);
  g_string_append_c (sql, ' ');
  g_string_append (sql, column->sql_type);

  if (column->primary_key)
    g_string_append (sql, " PRIMARY KEY");

  if (column->nonnull)
    g_string_append (sql, " NOT NULL");

  if (column->unique)
    g_string_append (sql, " UNIQUE");

  if (column->ref_table != NULL && column->ref_field != NULL)
    {
      g_string_append (sql, " REFERENCES ");
      gom_sqlite_driver_append_quoted_identifier (sql, column->ref_table);
      g_string_append_c (sql, '(');
      gom_sqlite_driver_append_quoted_identifier (sql, column->ref_field);
      g_string_append_c (sql, ')');
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_collect_entity_columns (GomEntitySpec  *entity,
                                          GPtrArray     **out_columns,
                                          GPtrArray     **out_pk_fields,
                                          GError        **error)
{
  const GomPropertySpec * const *entity_properties;
  const char * const *identity_fields;
  g_autoptr(GPtrArray) columns = NULL;
  g_autoptr(GPtrArray) pk_fields = NULL;
  g_autoptr(GHashTable) by_field = NULL;
  guint n_properties = 0;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (out_columns != NULL);
  g_assert (out_pk_fields != NULL);

  columns = g_ptr_array_new_with_free_func (gom_sqlite_column_def_free);
  pk_fields = g_ptr_array_new_with_free_func (g_free);
  by_field = g_hash_table_new (g_str_hash, g_str_equal);

  entity_properties = gom_entity_spec_list_properties (entity, &n_properties);
  for (guint i = 0; i < n_properties; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)entity_properties[i];
      const char *property_name = gom_property_spec_get_name (property);
      const char *field = gom_property_spec_get_field (property);
      GType value_type;
      const char *sql_type;
      GomSqliteColumnDef *column;
      gboolean has_transform;

      if (!gom_property_spec_get_mapped (property))
        continue;

      if (field == NULL || *field == '\0')
        continue;

      if (g_hash_table_contains (by_field, field))
        continue;

      value_type = gom_sqlite_driver_get_property_value_type (entity, property_name);
      if (value_type == G_TYPE_INVALID)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' property '%s' is missing from class metadata",
                       gom_entity_spec_get_name (entity),
                       property_name);
          return FALSE;
        }

      has_transform = gom_sqlite_driver_property_has_transform (entity, property_name);
      sql_type = gom_sqlite_driver_value_type_to_sql_type (value_type, has_transform);

      column = gom_sqlite_column_def_new (field,
                                          sql_type,
                                          gom_property_spec_get_nonnull (property),
                                          gom_property_spec_get_unique (property),
                                          FALSE,
                                          gom_property_spec_get_reference_table (property),
                                          gom_property_spec_get_reference_field (property));
      g_ptr_array_add (columns, column);
      g_hash_table_add (by_field, column->field);
    }

  if ((identity_fields = gom_entity_spec_get_identity_fields (entity)))
    {
      for (guint i = 0; identity_fields[i] != NULL; i++)
        g_ptr_array_add (pk_fields, g_strdup (identity_fields[i]));
    }

  if (pk_fields->len == 1)
    {
      const char *pk = g_ptr_array_index (pk_fields, 0);
      gboolean found = FALSE;

      for (guint i = 0; i < columns->len; i++)
        {
          GomSqliteColumnDef *column = g_ptr_array_index (columns, i);

          if (g_strcmp0 (column->field, pk) == 0)
            {
              column->primary_key = TRUE;
              column->nonnull = TRUE;
              found = TRUE;
              break;
            }
        }

      if (!found)
        {
          g_ptr_array_add (columns,
                           gom_sqlite_column_def_new (pk,
                                                      "INTEGER",
                                                      TRUE,
                                                      FALSE,
                                                      TRUE,
                                                      NULL,
                                                      NULL));
        }
    }
  else if (pk_fields->len > 1)
    {
      for (guint i = 0; i < pk_fields->len; i++)
        {
          const char *pk = g_ptr_array_index (pk_fields, i);
          gboolean found = FALSE;

          for (guint j = 0; j < columns->len; j++)
            {
              GomSqliteColumnDef *column = g_ptr_array_index (columns, j);

              if (g_strcmp0 (column->field, pk) == 0)
                {
                  column->nonnull = TRUE;
                  found = TRUE;
                  break;
                }
            }

          if (!found)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Entity '%s' identity field '%s' is not mapped",
                           gom_entity_spec_get_name (entity),
                           pk);
              return FALSE;
            }
        }
    }

  *out_columns = g_steal_pointer (&columns);
  *out_pk_fields = g_steal_pointer (&pk_fields);
  return TRUE;
}

static gboolean
gom_sqlite_driver_create_table_for_entity (sqlite3        *db,
                                           GomEntitySpec  *entity,
                                           const char     *table,
                                           GError        **error)
{
  g_autoptr(GPtrArray) columns = NULL;
  g_autoptr(GPtrArray) pk_fields = NULL;
  g_autoptr(GString) sql = NULL;

  g_assert (db != NULL);
  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (table != NULL);

  if (!gom_sqlite_driver_collect_entity_columns (entity, &columns, &pk_fields, error))
    return FALSE;

  if (columns->len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity '%s' has no mapped columns",
                   gom_entity_spec_get_name (entity));
      return FALSE;
    }

  sql = g_string_new ("CREATE TABLE IF NOT EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  g_string_append (sql, " (");

  for (guint i = 0; i < columns->len; i++)
    {
      if (i > 0)
        g_string_append (sql, ", ");

      gom_sqlite_driver_append_column_definition (sql, g_ptr_array_index (columns, i));
    }

  if (pk_fields->len > 1)
    {
      g_string_append (sql, ", PRIMARY KEY (");
      for (guint i = 0; i < pk_fields->len; i++)
        {
          if (i > 0)
            g_string_append (sql, ", ");
          gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (pk_fields, i));
        }
      g_string_append_c (sql, ')');
    }

  g_string_append_c (sql, ')');
  return gom_sqlite_driver_exec_sql (db, sql->str, "create table", error);
}

static char *
gom_sqlite_driver_get_index_name (const char   *table,
                                  GomIndexSpec *index)
{
  const char *name = gom_index_spec_get_name (index);

  g_assert (table != NULL);
  g_assert (GOM_IS_INDEX_SPEC (index));

  if (!gom_str_empty0 (name))
    return g_strdup_printf ("%s_%s", table, name);

  return g_strdup_printf ("%s_index", table);
}

static gboolean
gom_sqlite_driver_create_index (sqlite3       *db,
                                const char    *table,
                                GomIndexSpec  *index,
                                GError       **error)
{
  g_autofree char *index_name = NULL;
  g_autoptr(GString) sql = NULL;
  const char * const *fields;

  g_assert (db != NULL);
  g_assert (table != NULL);
  g_assert (GOM_IS_INDEX_SPEC (index));

  fields = gom_index_spec_get_fields (index);
  if (fields == NULL || fields[0] == NULL)
    return TRUE;

  index_name = gom_sqlite_driver_get_index_name (table, index);
  sql = g_string_new ("CREATE ");

  if (gom_index_spec_get_unique (index))
    g_string_append (sql, "UNIQUE ");

  g_string_append (sql, "INDEX IF NOT EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, index_name);
  g_string_append (sql, " ON ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  g_string_append (sql, " (");

  for (guint i = 0; fields[i] != NULL; i++)
    {
      if (i > 0)
        g_string_append (sql, ", ");
      gom_sqlite_driver_append_quoted_identifier (sql, fields[i]);
    }

  g_string_append_c (sql, ')');
  return gom_sqlite_driver_exec_sql (db, sql->str, "create index", error);
}

static gboolean
gom_sqlite_driver_drop_index (sqlite3     *db,
                              const char  *index_name,
                              GError     **error)
{
  g_autoptr(GString) sql = NULL;

  g_assert (db != NULL);
  g_assert (index_name != NULL);

  sql = g_string_new ("DROP INDEX IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, index_name);
  return gom_sqlite_driver_exec_sql (db, sql->str, "drop index", error);
}

static gboolean
gom_sqlite_driver_collect_entity_index_names (GomEntitySpec *entity,
                                              GHashTable    *out_names)
{
  const GomIndexSpec * const *indexes;
  guint n_indexes = 0;
  const char *table;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (out_names != NULL);

  table = gom_entity_spec_get_table (entity);
  indexes = gom_entity_spec_list_indexes (entity, &n_indexes);

  for (guint i = 0; i < n_indexes; i++)
    {
      g_autofree char *index_name = NULL;

      index_name = gom_sqlite_driver_get_index_name (table, (GomIndexSpec *)indexes[i]);
      g_hash_table_add (out_names, g_steal_pointer (&index_name));
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_sync_entity_indexes (sqlite3        *db,
                                       GomEntitySpec  *current_entity,
                                       GomEntitySpec  *next_entity,
                                       GError        **error)
{
  GHashTableIter iter;
  gpointer key;
  g_autoptr(GHashTable) current_names = NULL;
  g_autoptr(GHashTable) next_names = NULL;
  const GomIndexSpec * const *next_indexes;
  guint n_next_indexes = 0;
  const char *table;

  g_assert (db != NULL);
  g_assert (GOM_IS_ENTITY_SPEC (next_entity));

  current_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  next_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (current_entity != NULL)
    gom_sqlite_driver_collect_entity_index_names (current_entity, current_names);
  gom_sqlite_driver_collect_entity_index_names (next_entity, next_names);

  g_hash_table_iter_init (&iter, current_names);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      const char *index_name = key;

      if (!g_hash_table_contains (next_names, index_name) &&
          !gom_sqlite_driver_drop_index (db, index_name, error))
        return FALSE;
    }

  table = gom_entity_spec_get_table (next_entity);
  next_indexes = gom_entity_spec_list_indexes (next_entity, &n_next_indexes);
  for (guint i = 0; i < n_next_indexes; i++)
    {
      GomIndexSpec *index = (GomIndexSpec *)next_indexes[i];

      if (!gom_sqlite_driver_create_index (db, table, index, error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_property_is_search_indexed (GomPropertySpec *property)
{
  guint flags;

  g_assert (GOM_IS_PROPERTY_SPEC (property));

  flags = gom_property_spec_get_search_flags (property);
  return (flags & GOM_SEARCH_INDEXED) != 0;
}

static gboolean
gom_sqlite_driver_collect_fts_fields (GomEntitySpec *entity,
                                      GPtrArray     *fields)
{
  const GomPropertySpec * const *entity_properties;
  g_autoptr(GHashTable) seen = NULL;
  guint n_properties = 0;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (fields != NULL);

  seen = g_hash_table_new (g_str_hash, g_str_equal);
  entity_properties = gom_entity_spec_list_properties (entity, &n_properties);
  for (guint i = 0; i < n_properties; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)entity_properties[i];
      const char *field;

      if (!gom_property_spec_get_mapped (property))
        continue;
      if (!gom_sqlite_driver_property_is_search_indexed (property))
        continue;

      field = gom_property_spec_get_field (property);
      if (field == NULL || *field == '\0')
        continue;

      if (g_hash_table_contains (seen, field))
        continue;

      g_hash_table_add (seen, (gpointer)field);
      g_ptr_array_add (fields, g_strdup (field));
    }

  return TRUE;
}

static char *
gom_sqlite_driver_get_fts_table_name (const char *table)
{
  g_assert (table != NULL);

  return g_strdup_printf ("%s_fts", table);
}

static char *
gom_sqlite_driver_get_fts_trigger_name (const char *table,
                                        const char *suffix)
{
  g_assert (table != NULL);
  g_assert (suffix != NULL);

  return g_strdup_printf ("%s_fts_%s", table, suffix);
}

static gboolean
gom_sqlite_driver_drop_fts_for_table (sqlite3     *db,
                                      const char  *table,
                                      GError     **error)
{
  g_autofree char *fts_table = NULL;
  g_autofree char *insert_trigger = NULL;
  g_autofree char *update_trigger = NULL;
  g_autofree char *delete_trigger = NULL;
  g_autofree char *legacy_before_update_trigger = NULL;
  g_autofree char *legacy_delete_trigger = NULL;
  g_autoptr(GString) sql = NULL;

  g_assert (db != NULL);
  g_assert (table != NULL);

  fts_table = gom_sqlite_driver_get_fts_table_name (table);
  insert_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "ai");
  update_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "au");
  delete_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "ad");
  legacy_before_update_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "bu");
  legacy_delete_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "bd");

  sql = g_string_new ("DROP TRIGGER IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, insert_trigger);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop FTS insert trigger", error))
    return FALSE;

  g_string_assign (sql, "DROP TRIGGER IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, update_trigger);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop FTS update trigger", error))
    return FALSE;

  g_string_assign (sql, "DROP TRIGGER IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, delete_trigger);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop FTS delete trigger", error))
    return FALSE;

  g_string_assign (sql, "DROP TRIGGER IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, legacy_before_update_trigger);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop FTS legacy before update trigger", error))
    return FALSE;

  g_string_assign (sql, "DROP TRIGGER IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, legacy_delete_trigger);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop FTS legacy delete trigger", error))
    return FALSE;

  g_string_assign (sql, "DROP TABLE IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  return gom_sqlite_driver_exec_sql (db, sql->str, "drop FTS table", error);
}

static gboolean
gom_sqlite_driver_create_fts_for_table (sqlite3        *db,
                                        GomEntitySpec  *entity,
                                        GError        **error)
{
  g_autoptr(GPtrArray) fields = NULL;
  g_autofree char *fts_table = NULL;
  g_autofree char *insert_trigger = NULL;
  g_autofree char *update_trigger = NULL;
  g_autofree char *delete_trigger = NULL;
  g_autoptr(GString) sql = NULL;
  const char *table;

  g_assert (db != NULL);
  g_assert (GOM_IS_ENTITY_SPEC (entity));

  table = gom_entity_spec_get_table (entity);
  fields = g_ptr_array_new_with_free_func (g_free);
  gom_sqlite_driver_collect_fts_fields (entity, fields);

  if (fields->len == 0)
    return gom_sqlite_driver_drop_fts_for_table (db, table, error);

  fts_table = gom_sqlite_driver_get_fts_table_name (table);
  insert_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "ai");
  update_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "au");
  delete_trigger = gom_sqlite_driver_get_fts_trigger_name (table, "ad");

  if (!gom_sqlite_driver_drop_fts_for_table (db, table, error))
    return FALSE;

  sql = g_string_new ("CREATE VIRTUAL TABLE ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, " USING fts5 (");
  for (guint i = 0; i < fields->len; i++)
    {
      if (i > 0)
        g_string_append (sql, ", ");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, ", content=");
  {
    char *quoted_table_literal = sqlite3_mprintf ("%Q", table);
    g_string_append (sql, quoted_table_literal);
    sqlite3_free (quoted_table_literal);
  }
  g_string_append (sql, ", content_rowid='rowid')");
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "create FTS table", error))
    return FALSE;

  g_string_assign (sql, "CREATE TRIGGER ");
  gom_sqlite_driver_append_quoted_identifier (sql, insert_trigger);
  g_string_append (sql, " AFTER INSERT ON ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  g_string_append (sql, " BEGIN INSERT INTO ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, " (rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", ");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, ") VALUES (new.rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", new.");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, "); END");
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "create FTS insert trigger", error))
    return FALSE;

  g_string_assign (sql, "CREATE TRIGGER ");
  gom_sqlite_driver_append_quoted_identifier (sql, delete_trigger);
  g_string_append (sql, " AFTER DELETE ON ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  g_string_append (sql, " BEGIN INSERT INTO ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, " (");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, ", rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", ");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, ") VALUES ('delete', old.rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", old.");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, "); END");
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "create FTS delete trigger", error))
    return FALSE;

  g_string_assign (sql, "CREATE TRIGGER ");
  gom_sqlite_driver_append_quoted_identifier (sql, update_trigger);
  g_string_append (sql, " AFTER UPDATE ON ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  g_string_append (sql, " BEGIN INSERT INTO ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, " (");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, ", rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", ");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, ") VALUES ('delete', old.rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", old.");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, "); INSERT INTO ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, " (rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", ");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, ") VALUES (new.rowid");
  for (guint i = 0; i < fields->len; i++)
    {
      g_string_append (sql, ", new.");
      gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (fields, i));
    }
  g_string_append (sql, "); END");
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "create FTS update trigger", error))
    return FALSE;

  g_string_assign (sql, "INSERT INTO ");
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append_c (sql, '(');
  gom_sqlite_driver_append_quoted_identifier (sql, fts_table);
  g_string_append (sql, ") VALUES ('rebuild')");
  return gom_sqlite_driver_exec_sql (db, sql->str, "rebuild FTS index", error);
}

static gboolean
gom_sqlite_driver_table_columns_require_rebuild (GPtrArray  *current_columns,
                                                 GPtrArray  *next_columns,
                                                 GError    **error)
{
  GHashTableIter iter;
  gpointer key;
  gpointer value;
  g_autoptr(GHashTable) current_by_field = NULL;
  g_autoptr(GHashTable) next_by_field = NULL;

  g_assert (current_columns != NULL);
  g_assert (next_columns != NULL);

  current_by_field = g_hash_table_new (g_str_hash, g_str_equal);
  next_by_field = g_hash_table_new (g_str_hash, g_str_equal);

  for (guint i = 0; i < current_columns->len; i++)
    {
      GomSqliteColumnDef *column = g_ptr_array_index (current_columns, i);
      g_hash_table_insert (current_by_field, column->field, column);
    }

  for (guint i = 0; i < next_columns->len; i++)
    {
      GomSqliteColumnDef *column = g_ptr_array_index (next_columns, i);
      g_hash_table_insert (next_by_field, column->field, column);
    }

  g_hash_table_iter_init (&iter, current_by_field);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomSqliteColumnDef *current_column = value;
      GomSqliteColumnDef *next_column = g_hash_table_lookup (next_by_field, key);

      if (next_column == NULL)
        return TRUE;

      if (!gom_sqlite_column_def_equal (current_column, next_column))
        return TRUE;
    }

  g_hash_table_iter_init (&iter, next_by_field);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GomSqliteColumnDef *next_column = value;
      GomSqliteColumnDef *current_column = g_hash_table_lookup (current_by_field, key);

      if (current_column != NULL)
        continue;

      if (next_column->primary_key || next_column->unique ||
          next_column->ref_table != NULL || next_column->nonnull)
        return TRUE;
    }

  if (error != NULL)
    *error = NULL;
  return FALSE;
}

static gboolean
gom_sqlite_driver_add_missing_columns (sqlite3     *db,
                                       const char  *table,
                                       GPtrArray   *current_columns,
                                       GPtrArray   *next_columns,
                                       GError     **error)
{
  g_autoptr(GHashTable) current_by_field = NULL;

  g_assert (db != NULL);
  g_assert (table != NULL);
  g_assert (current_columns != NULL);
  g_assert (next_columns != NULL);

  current_by_field = g_hash_table_new (g_str_hash, g_str_equal);
  for (guint i = 0; i < current_columns->len; i++)
    {
      GomSqliteColumnDef *column = g_ptr_array_index (current_columns, i);
      g_hash_table_insert (current_by_field, column->field, column);
    }

  for (guint i = 0; i < next_columns->len; i++)
    {
      GomSqliteColumnDef *column = g_ptr_array_index (next_columns, i);
      g_autoptr(GString) sql = NULL;

      if (g_hash_table_contains (current_by_field, column->field))
        continue;

      if (column->nonnull)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "Cannot add NOT NULL column '%s' to non-empty table '%s' without default",
                       column->field,
                       table);
          return FALSE;
        }

      sql = g_string_new ("ALTER TABLE ");
      gom_sqlite_driver_append_quoted_identifier (sql, table);
      g_string_append (sql, " ADD COLUMN ");
      gom_sqlite_driver_append_column_definition (sql, column);

      if (!gom_sqlite_driver_exec_sql (db, sql->str, "add column", error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_rebuild_table (sqlite3        *db,
                                 const char     *table,
                                 GomEntitySpec  *next_entity,
                                 GPtrArray      *current_columns,
                                 GError        **error)
{
  g_autoptr(GPtrArray) next_columns = NULL;
  g_autoptr(GPtrArray) next_pk_fields = NULL;
  g_autoptr(GHashTable) current_by_field = NULL;
  g_autoptr(GString) sql = NULL;
  g_autoptr(GPtrArray) common_fields = NULL;
  g_autofree char *tmp_table = NULL;

  g_assert (db != NULL);
  g_assert (table != NULL);
  g_assert (GOM_IS_ENTITY_SPEC (next_entity));
  g_assert (current_columns != NULL);

  if (!gom_sqlite_driver_collect_entity_columns (next_entity,
                                                 &next_columns,
                                                 &next_pk_fields,
                                                 error))
    return FALSE;

  current_by_field = g_hash_table_new (g_str_hash, g_str_equal);
  for (guint i = 0; i < current_columns->len; i++)
    {
      GomSqliteColumnDef *column = g_ptr_array_index (current_columns, i);
      g_hash_table_insert (current_by_field, column->field, column);
    }

  common_fields = g_ptr_array_new_with_free_func (g_free);
  for (guint i = 0; i < next_columns->len; i++)
    {
      GomSqliteColumnDef *column = g_ptr_array_index (next_columns, i);
      if (g_hash_table_contains (current_by_field, column->field))
        g_ptr_array_add (common_fields, g_strdup (column->field));
      else if (column->nonnull)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "Cannot introduce NOT NULL column '%s' during table rebuild for '%s'",
                       column->field,
                       table);
          return FALSE;
        }
    }

  tmp_table = g_strdup_printf ("__gom_tmp_%s", table);

  sql = g_string_new ("DROP TABLE IF EXISTS ");
  gom_sqlite_driver_append_quoted_identifier (sql, tmp_table);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop temporary table", error))
    return FALSE;

  if (!gom_sqlite_driver_create_table_for_entity (db, next_entity, tmp_table, error))
    return FALSE;

  if (common_fields->len > 0)
    {
      g_string_assign (sql, "INSERT INTO ");
      gom_sqlite_driver_append_quoted_identifier (sql, tmp_table);
      g_string_append (sql, " (");
      for (guint i = 0; i < common_fields->len; i++)
        {
          if (i > 0)
            g_string_append (sql, ", ");
          gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (common_fields, i));
        }
      g_string_append (sql, ") SELECT ");
      for (guint i = 0; i < common_fields->len; i++)
        {
          if (i > 0)
            g_string_append (sql, ", ");
          gom_sqlite_driver_append_quoted_identifier (sql, g_ptr_array_index (common_fields, i));
        }
      g_string_append (sql, " FROM ");
      gom_sqlite_driver_append_quoted_identifier (sql, table);

      if (!gom_sqlite_driver_exec_sql (db, sql->str, "copy table rows", error))
        return FALSE;
    }

  g_string_assign (sql, "DROP TABLE ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop old table", error))
    return FALSE;

  g_string_assign (sql, "ALTER TABLE ");
  gom_sqlite_driver_append_quoted_identifier (sql, tmp_table);
  g_string_append (sql, " RENAME TO ");
  gom_sqlite_driver_append_quoted_identifier (sql, table);
  return gom_sqlite_driver_exec_sql (db, sql->str, "rename rebuilt table", error);
}

static gboolean
gom_sqlite_driver_sync_entity_table (sqlite3        *db,
                                     GomEntitySpec  *current_entity,
                                     GomEntitySpec  *next_entity,
                                     GError        **error)
{
  const char *table;
  g_autoptr(GPtrArray) current_columns = NULL;
  g_autoptr(GPtrArray) current_pk = NULL;
  g_autoptr(GPtrArray) next_columns = NULL;
  g_autoptr(GPtrArray) next_pk = NULL;
  gboolean requires_rebuild;

  g_assert (db != NULL);
  g_assert (GOM_IS_ENTITY_SPEC (next_entity));

  table = gom_entity_spec_get_table (next_entity);

  if (current_entity == NULL)
    return gom_sqlite_driver_create_table_for_entity (db, next_entity, table, error);

  if (!gom_sqlite_driver_collect_entity_columns (current_entity,
                                                 &current_columns,
                                                 &current_pk,
                                                 error))
    return FALSE;
  if (!gom_sqlite_driver_collect_entity_columns (next_entity, &next_columns, &next_pk, error))
    return FALSE;

  requires_rebuild = gom_sqlite_driver_table_columns_require_rebuild (current_columns,
                                                                      next_columns,
                                                                      error);
  if (requires_rebuild)
    return gom_sqlite_driver_rebuild_table (db, table, next_entity, current_columns, error);

  return gom_sqlite_driver_add_missing_columns (db, table, current_columns, next_columns, error);
}

static gboolean
gom_sqlite_driver_apply_registry_migration (sqlite3      *db,
                                            GomRegistry  *current,
                                            GomRegistry  *next,
                                            GError      **error)
{
  g_autoptr(GomRegistryDiff) diff = NULL;
  const GPtrArray *dropped_entities;
  const GPtrArray *added_entities;
  const GPtrArray *changed_entities;

  g_assert (db != NULL);
  g_assert (GOM_IS_REGISTRY (current));
  g_assert (GOM_IS_REGISTRY (next));

  diff = _gom_registry_diff_new (current, next);
  dropped_entities = _gom_registry_diff_get_dropped_entities (diff);
  added_entities = _gom_registry_diff_get_added_entities (diff);
  changed_entities = _gom_registry_diff_get_changed_entities (diff);

  for (guint i = 0; i < dropped_entities->len; i++)
    {
      GomEntitySpec *entity = g_ptr_array_index ((GPtrArray *)dropped_entities, i);
      const char *table = gom_entity_spec_get_table (entity);
      g_autoptr(GString) sql = NULL;

      if (table == NULL || *table == '\0')
        continue;

      if (!gom_sqlite_driver_drop_fts_for_table (db, table, error))
        return FALSE;

      sql = g_string_new ("DROP TABLE IF EXISTS ");
      gom_sqlite_driver_append_quoted_identifier (sql, table);
      if (!gom_sqlite_driver_exec_sql (db, sql->str, "drop table", error))
        return FALSE;
    }

  for (guint i = 0; i < added_entities->len; i++)
    {
      GomEntitySpec *next_entity = g_ptr_array_index ((GPtrArray *)added_entities, i);
      const char *table = gom_entity_spec_get_table (next_entity);

      if (table == NULL || *table == '\0')
        continue;

      if (!gom_sqlite_driver_sync_entity_table (db, NULL, next_entity, error))
        return FALSE;

      if (!gom_sqlite_driver_sync_entity_indexes (db, NULL, next_entity, error))
        return FALSE;

      if (!gom_sqlite_driver_create_fts_for_table (db, next_entity, error))
        return FALSE;
    }

  for (guint i = 0; i < changed_entities->len; i++)
    {
      GomEntityDiff *entity_diff = g_ptr_array_index ((GPtrArray *)changed_entities, i);
      GomEntitySpec *current_entity = _gom_entity_diff_get_current_entity (entity_diff);
      GomEntitySpec *next_entity = _gom_entity_diff_get_next_entity (entity_diff);

      if (!gom_sqlite_driver_sync_entity_table (db, current_entity, next_entity, error))
        return FALSE;

      if (!gom_sqlite_driver_sync_entity_indexes (db, current_entity, next_entity, error))
        return FALSE;

      if (!gom_sqlite_driver_create_fts_for_table (db, next_entity, error))
        return FALSE;
    }

  return TRUE;
}


static gboolean
gom_sqlite_binding_is_null_literal (GomExpression *expression)
{
  const GValue *value;

  if (!GOM_IS_LITERAL_EXPRESSION (expression))
    return FALSE;

  if (!_gom_literal_expression_has_value (GOM_LITERAL_EXPRESSION (expression)))
    return TRUE;

  value = _gom_literal_expression_peek_value (GOM_LITERAL_EXPRESSION (expression));
  if (value == NULL || G_VALUE_TYPE (value) == G_TYPE_INVALID)
    return TRUE;

  if (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value) == NULL)
    return TRUE;

  if (G_VALUE_HOLDS (value, G_TYPE_BYTES) && g_value_get_boxed (value) == NULL)
    return TRUE;

  if (G_VALUE_HOLDS_POINTER (value) && g_value_get_pointer (value) == NULL)
    return TRUE;

  return FALSE;
}

static gboolean
gom_sqlite_driver_bind_value (sqlite3_stmt      *stmt,
                              guint              index,
                              GomSqliteBinding  *binding,
                              GError           **error)
{
  int rc = SQLITE_OK;

  if (binding == NULL || !binding->has_value)
    {
      rc = sqlite3_bind_null (stmt, (int)index);
      if (rc == SQLITE_OK)
        return TRUE;
      goto failed;
    }

  if (G_VALUE_HOLDS_BOOLEAN (&binding->value))
    rc = sqlite3_bind_int (stmt, (int)index, g_value_get_boolean (&binding->value));
  else if (G_VALUE_HOLDS_INT (&binding->value))
    rc = sqlite3_bind_int64 (stmt, (int)index, (sqlite3_int64)g_value_get_int (&binding->value));
  else if (G_VALUE_HOLDS_UINT (&binding->value))
    rc = sqlite3_bind_int64 (stmt, (int)index, (sqlite3_int64)g_value_get_uint (&binding->value));
  else if (G_VALUE_HOLDS_INT64 (&binding->value))
    rc = sqlite3_bind_int64 (stmt, (int)index, (sqlite3_int64)g_value_get_int64 (&binding->value));
  else if (G_VALUE_HOLDS_UINT64 (&binding->value))
    {
      guint64 value = g_value_get_uint64 (&binding->value);

      if (value > G_MAXINT64)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "SQLite cannot bind UINT64 value %" G_GUINT64_FORMAT,
                       value);
          return FALSE;
        }

      rc = sqlite3_bind_int64 (stmt, (int)index, (sqlite3_int64)value);
    }
  else if (G_VALUE_HOLDS_DOUBLE (&binding->value))
    rc = sqlite3_bind_double (stmt, (int)index, g_value_get_double (&binding->value));
  else if (G_VALUE_HOLDS_FLOAT (&binding->value))
    rc = sqlite3_bind_double (stmt, (int)index, (double)g_value_get_float (&binding->value));
  else if (G_VALUE_HOLDS (&binding->value, G_TYPE_DATE_TIME))
    {
      GDateTime *dt = g_value_get_boxed (&binding->value);
      g_autofree char *iso8601 = NULL;

      if (dt == NULL)
        rc = sqlite3_bind_null (stmt, (int)index);
      else
        {
          iso8601 = g_date_time_format_iso8601 (dt);

          if (iso8601 == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Failed to format GDateTime as ISO8601");
              return FALSE;
            }

          rc = sqlite3_bind_text (stmt, (int)index, iso8601, -1, SQLITE_TRANSIENT);
        }
    }
  else if (G_VALUE_HOLDS_STRING (&binding->value))
    {
      const char *str = g_value_get_string (&binding->value);

      if (str == NULL)
        rc = sqlite3_bind_null (stmt, (int)index);
      else
        rc = sqlite3_bind_text (stmt, (int)index, str, -1, SQLITE_TRANSIENT);
    }
  else if (G_VALUE_HOLDS (&binding->value, G_TYPE_STRV))
    {
      const char * const *strv = g_value_get_boxed (&binding->value);
      g_autofree char *encoded = NULL;

      if (strv == NULL)
        rc = sqlite3_bind_null (stmt, (int)index);
      else
        {
          encoded = _gom_strv_to_text (strv);
          rc = sqlite3_bind_text (stmt, (int)index, encoded, -1, SQLITE_TRANSIENT);
        }
    }
  else if (G_VALUE_HOLDS_ENUM (&binding->value))
    rc = sqlite3_bind_int64 (stmt, (int)index, (sqlite3_int64)g_value_get_enum (&binding->value));
  else if (G_VALUE_HOLDS (&binding->value, G_TYPE_GTYPE))
    {
      GType type = g_value_get_gtype (&binding->value);
      const char *type_name = g_type_name (type);

      if (type_name == NULL || *type_name == '\0')
        rc = sqlite3_bind_null (stmt, (int)index);
      else
        rc = sqlite3_bind_text (stmt, (int)index, type_name, -1, SQLITE_TRANSIENT);
    }
  else if (G_VALUE_HOLDS (&binding->value, G_TYPE_BYTES))
    {
      GBytes *bytes = g_value_get_boxed (&binding->value);

      if (bytes == NULL)
        rc = sqlite3_bind_null (stmt, (int)index);
      else
        {
          gsize size = 0;
          const guint8 *data = g_bytes_get_data (bytes, &size);

          rc = sqlite3_bind_blob (stmt, (int)index, data, (int)size, SQLITE_TRANSIENT);
        }
    }
  else if (G_VALUE_HOLDS_POINTER (&binding->value) && g_value_get_pointer (&binding->value) == NULL)
    rc = sqlite3_bind_null (stmt, (int)index);
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Unsupported SQLite binding type %s",
                   g_type_name (G_VALUE_TYPE (&binding->value)));
      return FALSE;
    }

  if (rc == SQLITE_OK)
    return TRUE;

failed:
  g_set_error (error,
               GOM_ERROR,
               GOM_ERROR_BIND_FAILED,
               "SQLite bind failed: %s",
               sqlite3_errmsg (sqlite3_db_handle (stmt)));
  return FALSE;
}

static gboolean
gom_sqlite_driver_collect_table_info (sqlite3     *db,
                                      const char  *relation,
                                      GListStore  *fields,
                                      GError     **error)
{
  sqlite3_stmt *stmt = NULL;
  char *sql = NULL;
  gboolean have_rows = FALSE;
  int rc;

  g_assert (db != NULL);
  g_assert (relation != NULL);
  g_assert (G_IS_LIST_STORE (fields));

  sql = sqlite3_mprintf ("PRAGMA table_info(%Q)", relation);
  rc = gom_sqlite_driver_prepare (db, sql, &stmt, "prepare table_info", error);
  sqlite3_free (sql);

  if (rc != SQLITE_OK)
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_PREPARE_FAILED,
                   "Failed to prepare table_info for '%s': %s",
                   relation,
                   sqlite3_errmsg (db));
      return FALSE;
    }

  while ((rc = gom_sqlite_driver_step (stmt, "read table_info", error)) == SQLITE_ROW)
    {
      GomFieldSchema *field;
      const char *name = (const char *)sqlite3_column_text (stmt, 1);
      const char *type = (const char *)sqlite3_column_text (stmt, 2);
      gboolean nonnull = sqlite3_column_int (stmt, 3) != 0;
      const char *default_value = (const char *)sqlite3_column_text (stmt, 4);
      gboolean primary_key = sqlite3_column_int (stmt, 5) != 0;

      if (name == NULL || *name == '\0')
        continue;

      have_rows = TRUE;
      field = _gom_field_schema_new (name, type, nonnull, primary_key, default_value);
      g_list_store_append (fields, field);
      g_object_unref (field);
    }

  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "Failed to read table_info for '%s': %s",
                   relation,
                   sqlite3_errmsg (db));
      return FALSE;
    }

  if (!have_rows)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_FOUND,
                   "Relation '%s' was not found",
                   relation);
      return FALSE;
    }

  return TRUE;
}

static char **
gom_sqlite_driver_ptr_array_to_strv (GPtrArray *array)
{
  char **strv;

  g_assert (array != NULL);

  strv = g_new0 (char *, array->len + 1);
  for (guint i = 0; i < array->len; i++)
    strv[i] = g_strdup (g_ptr_array_index (array, i));

  return strv;
}

static gboolean
gom_sqlite_driver_collect_indexes (sqlite3     *db,
                                   const char  *relation,
                                   GListStore  *indexes,
                                   GError     **error)
{
  sqlite3_stmt *stmt = NULL;
  char *sql = NULL;
  int rc;

  g_assert (db != NULL);
  g_assert (relation != NULL);
  g_assert (G_IS_LIST_STORE (indexes));

  sql = sqlite3_mprintf ("PRAGMA index_list(%Q)", relation);
  rc = gom_sqlite_driver_prepare (db, sql, &stmt, "prepare index_list", error);
  sqlite3_free (sql);

  if (rc != SQLITE_OK)
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_PREPARE_FAILED,
                   "Failed to prepare index_list for '%s': %s",
                   relation,
                   sqlite3_errmsg (db));
      return FALSE;
    }

  while ((rc = gom_sqlite_driver_step (stmt, "read index_list", error)) == SQLITE_ROW)
    {
      const char *index_name = (const char *)sqlite3_column_text (stmt, 1);
      gboolean unique = sqlite3_column_int (stmt, 2) != 0;
      sqlite3_stmt *idx_stmt = NULL;
      g_autoptr(GPtrArray) fields = NULL;
      char *idx_sql = NULL;

      if (index_name == NULL || *index_name == '\0')
        continue;

      fields = g_ptr_array_new_with_free_func (g_free);

      idx_sql = sqlite3_mprintf ("PRAGMA index_info(%Q)", index_name);
      rc = gom_sqlite_driver_prepare (db, idx_sql, &idx_stmt, "prepare index_info", error);
      if (rc != SQLITE_OK)
        {
          sqlite3_free (idx_sql);
          sqlite3_finalize (stmt);
          if (error != NULL && *error != NULL)
            return FALSE;

          g_set_error (error,
                       GOM_ERROR,
                       GOM_ERROR_PREPARE_FAILED,
                       "Failed to prepare index_info for '%s': %s",
                       index_name,
                       sqlite3_errmsg (db));
          return FALSE;
        }
      sqlite3_free (idx_sql);

      while ((rc = gom_sqlite_driver_step (idx_stmt, "read index_info", error)) == SQLITE_ROW)
        {
          const char *field = (const char *)sqlite3_column_text (idx_stmt, 2);

          if (!gom_str_empty0 (field))
            g_ptr_array_add (fields, g_strdup (field));
        }

      sqlite3_finalize (idx_stmt);

      if (rc != SQLITE_DONE)
        {
          sqlite3_finalize (stmt);
          if (error != NULL && *error != NULL)
            return FALSE;

          g_set_error (error,
                       GOM_ERROR,
                       GOM_ERROR_METADATA_READ_FAILED,
                       "Failed to read index_info for '%s': %s",
                       index_name,
                       sqlite3_errmsg (db));
          return FALSE;
        }

      if (fields->len > 0)
        {
          GomIndexSchema *index;
          char **field_strv;

          field_strv = gom_sqlite_driver_ptr_array_to_strv (fields);
          index = _gom_index_schema_new (index_name, unique, (const char * const *)field_strv);
          g_list_store_append (indexes, index);
          g_object_unref (index);
          g_strfreev (field_strv);
        }
    }

  sqlite3_finalize (stmt);

  if (rc != SQLITE_DONE)
    {
      if (error != NULL && *error != NULL)
        return FALSE;

      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_METADATA_READ_FAILED,
                   "Failed to read index_list for '%s': %s",
                   relation,
                   sqlite3_errmsg (db));
      return FALSE;
    }

  return TRUE;
}

static DexFuture *
gom_sqlite_driver_describe_thread (gpointer user_data)
{
  GomSqliteDescribeTask *task = user_data;
  g_autoptr(GListStore) fields = NULL;
  g_autoptr(GListStore) indexes = NULL;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  GomRelationSchema *schema;

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (task->relation != NULL);

  fields = g_list_store_new (GOM_TYPE_FIELD_SCHEMA);
  indexes = g_list_store_new (GOM_TYPE_INDEX_SCHEMA);

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  if (!gom_sqlite_driver_collect_table_info (db, task->relation, fields, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_sqlite_driver_collect_indexes (db, task->relation, indexes, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  schema = _gom_relation_schema_new (task->relation, G_LIST_MODEL (fields), G_LIST_MODEL (indexes));
  return dex_future_new_take_object (g_steal_pointer (&schema));
}

static gboolean
gom_sqlite_driver_append_expression_list_with_context (GPtrArray                         *expressions,
                                                       GString                           *sql,
                                                       GPtrArray                         *bindings,
                                                       GError                           **error,
                                                       const GomSqliteExpressionContext  *context)
{
  if (expressions == NULL || expressions->len == 0)
    return TRUE;

  for (guint i = 0; i < expressions->len; i++)
    {
      if (!gom_sqlite_driver_append_expression_with_context (g_ptr_array_index (expressions, i),
                                                             sql,
                                                             bindings,
                                                             error,
                                                             context))
        return FALSE;

      if (i + 1 < expressions->len)
        g_string_append (sql, ", ");
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_append_expression_list (GPtrArray  *expressions,
                                          GString    *sql,
                                          GPtrArray  *bindings,
                                          GError    **error)
{
  return gom_sqlite_driver_append_expression_list_with_context (expressions,
                                                                sql,
                                                                bindings,
                                                                error,
                                                                NULL);
}

static const char *
gom_sqlite_driver_binary_operator_to_sql (GomBinaryOperator op)
{
  switch (op)
    {
    case GOM_BINARY_ADD:
      return "+";
    case GOM_BINARY_SUBTRACT:
      return "-";
    case GOM_BINARY_MULTIPLY:
      return "*";
    case GOM_BINARY_DIVIDE:
      return "/";
    case GOM_BINARY_MODULO:
      return "%";
    case GOM_BINARY_EQUAL:
      return "=";
    case GOM_BINARY_NOT_EQUAL:
      return "!=";
    case GOM_BINARY_LESS_THAN:
      return "<";
    case GOM_BINARY_LESS_EQUAL:
      return "<=";
    case GOM_BINARY_GREATER_THAN:
      return ">";
    case GOM_BINARY_GREATER_EQUAL:
      return ">=";
    case GOM_BINARY_AND:
      return "AND";
    case GOM_BINARY_OR:
      return "OR";
    case GOM_BINARY_LIKE:
      return "LIKE";
    default:
      return NULL;
    }
}

static gboolean
gom_sqlite_driver_fts_token_is_operator (const char *token)
{
  if (token == NULL || *token == '\0')
    return FALSE;

  return (g_ascii_strcasecmp (token, "AND") == 0 ||
          g_ascii_strcasecmp (token, "OR") == 0 ||
          g_ascii_strcasecmp (token, "NOT") == 0 ||
          g_ascii_strcasecmp (token, "NEAR") == 0);
}

static char *
gom_sqlite_driver_fts_escape_phrase (const char *query)
{
  GString *escaped = g_string_new ("\"");

  for (const char *iter = query; *iter; iter = g_utf8_next_char (iter))
    {
      gunichar ch = g_utf8_get_char (iter);

      if (ch == '"')
        g_string_append (escaped, "\"\"");
      else
        g_string_append_unichar (escaped, ch);
    }

  g_string_append_c (escaped, '"');

  return g_string_free (escaped, FALSE);
}

static char *
gom_sqlite_driver_fts_apply_prefix (const char *query)
{
  gchar **parts = g_strsplit_set (query, " \t\r\n", -1);
  GString *prefixed = g_string_new (NULL);

  for (guint i = 0; parts != NULL && parts[i] != NULL; i++)
    {
      const char *part = parts[i];

      if (part == NULL || *part == '\0')
        continue;

      if (prefixed->len > 0)
        g_string_append_c (prefixed, ' ');

      if (gom_sqlite_driver_fts_token_is_operator (part) ||
          strchr (part, '"') != NULL ||
          strchr (part, ':') != NULL ||
          g_str_has_suffix (part, "*"))
        {
          g_string_append (prefixed, part);
        }
      else
        {
          g_string_append (prefixed, part);
          g_string_append_c (prefixed, '*');
        }
    }

  g_strfreev (parts);

  return g_string_free (prefixed, FALSE);
}

static char *
gom_sqlite_driver_build_fts_query (const char     *query,
                                   GomSearchMode   mode,
                                   GError        **error)
{
  switch (mode)
    {
    case GOM_SEARCH_MODE_NATURAL:
      return g_strdup (query);

    case GOM_SEARCH_MODE_PREFIX:
      return gom_sqlite_driver_fts_apply_prefix (query);

    case GOM_SEARCH_MODE_PHRASE:
      return gom_sqlite_driver_fts_escape_phrase (query);

    default:
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Unknown search mode");
      return NULL;
    }
}

static gboolean
gom_sqlite_driver_expression_requires_fts (GomExpression *expression,
                                           GomEntitySpec *entity)
{
  if (expression == NULL || entity == NULL)
    return FALSE;

  if (GOM_IS_SEARCH_EXPRESSION (expression))
    {
      GomExpression *target = _gom_search_expression_get_target (GOM_SEARCH_EXPRESSION (expression));
      GomExpression *query = _gom_search_expression_get_query (GOM_SEARCH_EXPRESSION (expression));

      if (GOM_IS_FIELD_EXPRESSION (target))
        {
          const char *field = _gom_field_expression_get_field (GOM_FIELD_EXPRESSION (target));

          if (!gom_str_empty0 (field))
            {
              const GomPropertySpec *property = _gom_entity_spec_lookup_property_by_name (entity, field);

              if (property == NULL)
                property = _gom_entity_spec_lookup_property_by_field (entity, field);

              if (property != NULL &&
                  gom_property_spec_get_mapped ((GomPropertySpec *) property) &&
                  (gom_property_spec_get_search_flags ((GomPropertySpec *) property) & GOM_SEARCH_INDEXED) != 0)
                return TRUE;
            }
        }

      if (gom_sqlite_driver_expression_requires_fts (target, entity))
        return TRUE;

      if (gom_sqlite_driver_expression_requires_fts (query, entity))
        return TRUE;

      return FALSE;
    }

  if (GOM_IS_UNARY_EXPRESSION (expression))
    return gom_sqlite_driver_expression_requires_fts (_gom_unary_expression_get_operand (GOM_UNARY_EXPRESSION (expression)),
                                                      entity);

  if (GOM_IS_BINARY_EXPRESSION (expression))
    {
      if (gom_sqlite_driver_expression_requires_fts (_gom_binary_expression_get_left (GOM_BINARY_EXPRESSION (expression)),
                                                     entity))
        return TRUE;

      return gom_sqlite_driver_expression_requires_fts (_gom_binary_expression_get_right (GOM_BINARY_EXPRESSION (expression)),
                                                        entity);
    }

  if (GOM_IS_FUNCTION_EXPRESSION (expression))
    {
      GPtrArray *arguments = _gom_function_expression_get_arguments (GOM_FUNCTION_EXPRESSION (expression));

      if (arguments == NULL)
        return FALSE;

      for (guint i = 0; i < arguments->len; i++)
        {
          if (gom_sqlite_driver_expression_requires_fts (g_ptr_array_index (arguments, i), entity))
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gom_sqlite_driver_expression_contains_search (GomExpression *expression)
{
  if (expression == NULL)
    return FALSE;

  if (GOM_IS_SEARCH_EXPRESSION (expression))
    return TRUE;

  if (GOM_IS_UNARY_EXPRESSION (expression))
    return gom_sqlite_driver_expression_contains_search (_gom_unary_expression_get_operand (GOM_UNARY_EXPRESSION (expression)));

  if (GOM_IS_BINARY_EXPRESSION (expression))
    {
      if (gom_sqlite_driver_expression_contains_search (_gom_binary_expression_get_left (GOM_BINARY_EXPRESSION (expression))))
        return TRUE;

      return gom_sqlite_driver_expression_contains_search (_gom_binary_expression_get_right (GOM_BINARY_EXPRESSION (expression)));
    }

  if (GOM_IS_FUNCTION_EXPRESSION (expression))
    {
      GPtrArray *arguments = _gom_function_expression_get_arguments (GOM_FUNCTION_EXPRESSION (expression));

      if (arguments == NULL)
        return FALSE;

      for (guint i = 0; i < arguments->len; i++)
        {
          if (gom_sqlite_driver_expression_contains_search (g_ptr_array_index (arguments, i)))
            return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gom_sqlite_driver_expression_list_contains_search (GPtrArray *expressions)
{
  if (expressions == NULL || expressions->len == 0)
    return FALSE;

  for (guint i = 0; i < expressions->len; i++)
    {
      if (gom_sqlite_driver_expression_contains_search (g_ptr_array_index (expressions, i)))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gom_sqlite_driver_orderings_contains_search (GPtrArray *orderings)
{
  if (orderings == NULL || orderings->len == 0)
    return FALSE;

  for (guint i = 0; i < orderings->len; i++)
    {
      GomOrdering *ordering = g_ptr_array_index (orderings, i);

      if (gom_sqlite_driver_expression_contains_search (gom_ordering_get_expression (ordering)))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gom_sqlite_driver_expression_list_requires_fts (GPtrArray     *expressions,
                                                GomEntitySpec *entity)
{
  if (expressions == NULL || expressions->len == 0)
    return FALSE;

  for (guint i = 0; i < expressions->len; i++)
    {
      if (gom_sqlite_driver_expression_requires_fts (g_ptr_array_index (expressions, i), entity))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gom_sqlite_driver_orderings_requires_fts (GPtrArray     *orderings,
                                          GomEntitySpec *entity)
{
  if (orderings == NULL || orderings->len == 0)
    return FALSE;

  for (guint i = 0; i < orderings->len; i++)
    {
      GomOrdering *ordering = g_ptr_array_index (orderings, i);

      if (gom_sqlite_driver_expression_requires_fts (gom_ordering_get_expression (ordering),
                                                     entity))
        return TRUE;
    }

  return FALSE;
}

static void
gom_sqlite_driver_append_field (GString    *sql,
                                const char *prefix,
                                const char *field)
{
  if (field == NULL)
    return;

  if (!gom_str_empty0 (prefix) && strchr (field, '.') == NULL)
    {
      g_string_append (sql, prefix);
      g_string_append_c (sql, '.');
    }

  if (g_strcmp0 (field, "rowid") == 0)
    {
      g_string_append (sql, field);
      return;
    }

  if (strchr (field, '.') != NULL)
    gom_sqlite_driver_append_quoted_identifier_path (sql, field);
  else
    gom_sqlite_driver_append_quoted_identifier (sql, field);
}

static const char *
gom_sqlite_driver_resolve_entity_field (GomEntitySpec  *entity,
                                        const char     *field,
                                        GError        **error)
{
  const GomPropertySpec *property;
  const char *mapped_field;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (field != NULL);

  if (g_strcmp0 (field, "rowid") == 0)
    return field;

  if ((property = _gom_entity_spec_lookup_property_by_name (entity, field)))
    {
      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' property '%s' is not mapped",
                       gom_entity_spec_get_name (entity),
                       field);
          return NULL;
        }

      mapped_field = gom_property_spec_get_field ((GomPropertySpec *)property);
      if (mapped_field == NULL || *mapped_field == '\0')
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' property '%s' has no mapped field",
                       gom_entity_spec_get_name (entity),
                       field);
          return NULL;
        }

      return mapped_field;
    }

  if ((property = _gom_entity_spec_lookup_property_by_field (entity, field)))
    {
      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' field '%s' refers to an unmapped property",
                       gom_entity_spec_get_name (entity),
                       field);
          return NULL;
        }

      return field;
    }

  {
    g_autoptr(GTypeClass) klass = NULL;
    GParamSpec *pspec = NULL;

    if ((klass = g_type_class_ref (gom_entity_spec_get_entity_type (entity))))
      {
        if ((pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), field)))
          {
            if (!gom_entity_class_property_get_mapped (GOM_ENTITY_CLASS (klass), field))
              {
                g_set_error (error,
                             G_IO_ERROR,
                             G_IO_ERROR_INVALID_ARGUMENT,
                             "Entity '%s' property '%s' is not mapped",
                             gom_entity_spec_get_name (entity),
                             field);
                return NULL;
              }

            return field;
          }
      }
  }

  {
    const char * const *identity_fields = gom_entity_spec_get_identity_fields (entity);

    if (_gom_strv_contains (identity_fields, field))
      return field;
  }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_ARGUMENT,
               "Entity '%s' has no mapped property or field named '%s'",
               gom_entity_spec_get_name (entity),
               field);
  return NULL;
}

static gboolean
gom_sqlite_driver_append_expression_with_context (GomExpression                     *expression,
                                                  GString                           *sql,
                                                  GPtrArray                         *bindings,
                                                  GError                           **error,
                                                  const GomSqliteExpressionContext  *context)
{
  g_return_val_if_fail (GOM_IS_EXPRESSION (expression), FALSE);
  g_return_val_if_fail (sql != NULL, FALSE);
  g_return_val_if_fail (bindings != NULL, FALSE);

  if (GOM_IS_LITERAL_EXPRESSION (expression))
    {
      if (gom_sqlite_binding_is_null_literal (expression))
        {
          g_string_append (sql, "NULL");
          return TRUE;
        }

      g_string_append (sql, "?");
      g_ptr_array_add (bindings,
                       gom_sqlite_binding_new (_gom_literal_expression_peek_value (GOM_LITERAL_EXPRESSION (expression))));
      return TRUE;
    }

  if (GOM_IS_FIELD_EXPRESSION (expression))
    {
      const char *field = _gom_field_expression_get_field (GOM_FIELD_EXPRESSION (expression));
      const char *prefix = context ? context->field_prefix : NULL;
      const char *resolved_field = field;

      if (field == NULL || *field == '\0')
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Field expression requires a field name");
          return FALSE;
        }

      if (context != NULL && context->entity != NULL)
        {
          if (strchr (field, '.') != NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Qualified field references are not supported for entity '%s': '%s'",
                           gom_entity_spec_get_name (context->entity),
                           field);
              return FALSE;
            }

          if (!(resolved_field = gom_sqlite_driver_resolve_entity_field (context->entity, field, error)))
            return FALSE;
        }

      gom_sqlite_driver_append_field (sql, prefix, resolved_field);
      return TRUE;
    }

  if (GOM_IS_FUNCTION_EXPRESSION (expression))
    {
      const char *name = _gom_function_expression_get_name (GOM_FUNCTION_EXPRESSION (expression));
      GPtrArray *arguments = _gom_function_expression_get_arguments (GOM_FUNCTION_EXPRESSION (expression));

      if (name == NULL || *name == '\0')
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Function expression requires a name");
          return FALSE;
        }

      g_string_append (sql, name);
      g_string_append_c (sql, '(');

      if (!gom_sqlite_driver_append_expression_list_with_context (arguments,
                                                                  sql,
                                                                  bindings,
                                                                  error,
                                                                  context))
        return FALSE;

      g_string_append_c (sql, ')');
      return TRUE;
    }

  if (GOM_IS_VECTOR_DISTANCE_EXPRESSION (expression))
    {
#if HAVE_SQLITE_VEC1
      GomVectorDistanceExpression *distance = GOM_VECTOR_DISTANCE_EXPRESSION (expression);
      GomExpression *target = _gom_vector_distance_expression_get_target (distance);
      GomVector *query = _gom_vector_distance_expression_get_query (distance);
      GomVectorMetric metric = _gom_vector_distance_expression_get_metric (distance);
      g_autoptr(GBytes) bytes = NULL;
      g_auto(GValue) value = G_VALUE_INIT;
      const char *function_name;

#if G_BYTE_ORDER != G_LITTLE_ENDIAN
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "SQLite vec1 requires native little-endian float32 vectors");
      return FALSE;
#endif

      if (gom_vector_get_format (query) != GOM_VECTOR_FORMAT_FLOAT32_LE)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "SQLite vec1 only supports float32 vectors");
          return FALSE;
        }

      switch (metric)
        {
        case GOM_VECTOR_METRIC_COSINE:
          function_name = "vec1_cos_distance";
          break;

        case GOM_VECTOR_METRIC_L2:
          function_name = "vec1_l2_distance";
          break;

        case GOM_VECTOR_METRIC_DOT:
        default:
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "SQLite vec1 does not support dot-product vector search");
          return FALSE;
        }

      g_string_append (sql, function_name);
      g_string_append_c (sql, '(');

      if (!gom_sqlite_driver_append_expression_with_context (target, sql, bindings, error, context))
        return FALSE;

      bytes = gom_vector_dup_bytes (query);
      g_value_init (&value, G_TYPE_BYTES);
      g_value_set_boxed (&value, bytes);

      g_string_append (sql, ", ?");
      g_ptr_array_add (bindings, gom_sqlite_binding_new (&value));
      g_string_append_c (sql, ')');

      return TRUE;
#else
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "SQLite vector search support was not built");
      return FALSE;
#endif
    }

  if (GOM_IS_SEARCH_EXPRESSION (expression))
    {
      GomExpression *target = _gom_search_expression_get_target (GOM_SEARCH_EXPRESSION (expression));
      GomExpression *query = _gom_search_expression_get_query (GOM_SEARCH_EXPRESSION (expression));
      GomSearchMode mode = _gom_search_expression_get_mode (GOM_SEARCH_EXPRESSION (expression));

      if (target == NULL || query == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Search expression requires a target and query");
          return FALSE;
        }

      g_string_append_c (sql, '(');

      if (context != NULL &&
          context->fts_prefix != NULL &&
          GOM_IS_FIELD_EXPRESSION (target))
        {
          const char *field = _gom_field_expression_get_field (GOM_FIELD_EXPRESSION (target));
          const char *resolved_field = field;

          if (field == NULL || *field == '\0')
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   "Search expression requires a field target");
              return FALSE;
            }

          if (context->entity != NULL)
            {
              if (strchr (field, '.') != NULL)
                {
                  g_set_error (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Qualified search field references are not supported for entity '%s': '%s'",
                               gom_entity_spec_get_name (context->entity),
                               field);
                  return FALSE;
                }

              resolved_field = gom_sqlite_driver_resolve_entity_field (context->entity,
                                                                       field,
                                                                       error);
              if (resolved_field == NULL)
                return FALSE;
            }

          gom_sqlite_driver_append_field (sql, context->fts_prefix, resolved_field);
        }
      else
        {
          if (!gom_sqlite_driver_append_expression_with_context (target,
                                                                 sql,
                                                                 bindings,
                                                                 error,
                                                                 context))
            return FALSE;
        }

      g_string_append (sql, " MATCH ");

      if (GOM_IS_LITERAL_EXPRESSION (query))
        {
          const GValue *value = _gom_literal_expression_peek_value (GOM_LITERAL_EXPRESSION (query));
          const char *query_text = NULL;
          g_autofree char *fts_query = NULL;
          g_auto(GValue) binding_value = G_VALUE_INIT;

          if (value == NULL || !G_VALUE_HOLDS_STRING (value))
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   "Search query must be a string literal");
              return FALSE;
            }

          if (!(query_text = g_value_get_string (value)))
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   "Search query cannot be NULL");
              return FALSE;
            }

          if (!(fts_query = gom_sqlite_driver_build_fts_query (query_text, mode, error)))
            return FALSE;

          g_string_append (sql, "?");
          g_value_init (&binding_value, G_TYPE_STRING);
          g_value_take_string (&binding_value, g_steal_pointer (&fts_query));
          g_ptr_array_add (bindings, gom_sqlite_binding_new (&binding_value));
        }
      else
        {
          if (mode != GOM_SEARCH_MODE_NATURAL)
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   "Search mode requires a literal query");
              return FALSE;
            }

          if (!gom_sqlite_driver_append_expression_with_context (query,
                                                                 sql,
                                                                 bindings,
                                                                 error,
                                                                 context))
            return FALSE;
        }

      g_string_append_c (sql, ')');
      return TRUE;
    }

  if (GOM_IS_UNARY_EXPRESSION (expression))
    {
      GomUnaryOperator op = _gom_unary_expression_get_operator (GOM_UNARY_EXPRESSION (expression));
      GomExpression *operand = _gom_unary_expression_get_operand (GOM_UNARY_EXPRESSION (expression));

      if (operand == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Unary expression requires an operand");
          return FALSE;
        }

      g_string_append_c (sql, '(');

      switch (op)
        {
        case GOM_UNARY_NEGATE:
          g_string_append (sql, "-");
          break;

        case GOM_UNARY_NOT:
          g_string_append (sql, "NOT ");
          break;

        default:
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Unknown unary operator");
          return FALSE;
        }

      if (!gom_sqlite_driver_append_expression_with_context (operand,
                                                             sql,
                                                             bindings,
                                                             error,
                                                             context))
        return FALSE;

      g_string_append_c (sql, ')');
      return TRUE;
    }

  if (GOM_IS_BINARY_EXPRESSION (expression))
    {
      GomBinaryOperator op = _gom_binary_expression_get_operator (GOM_BINARY_EXPRESSION (expression));
      GomExpression *left = _gom_binary_expression_get_left (GOM_BINARY_EXPRESSION (expression));
      GomExpression *right = _gom_binary_expression_get_right (GOM_BINARY_EXPRESSION (expression));
      const char *op_sql = gom_sqlite_driver_binary_operator_to_sql (op);
      gboolean left_is_null = gom_sqlite_binding_is_null_literal (left);
      gboolean right_is_null = gom_sqlite_binding_is_null_literal (right);

      if (left == NULL || right == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Binary expression requires both operands");
          return FALSE;
        }

      if ((op == GOM_BINARY_EQUAL || op == GOM_BINARY_NOT_EQUAL) && (left_is_null || right_is_null))
        {
          GomExpression *non_null = left_is_null ? right : left;

          if (!gom_sqlite_driver_append_expression_with_context (non_null,
                                                                 sql,
                                                                 bindings,
                                                                 error,
                                                                 context))
            return FALSE;

          g_string_append (sql, op == GOM_BINARY_EQUAL ? " IS NULL" : " IS NOT NULL");
          return TRUE;
        }

      if (op_sql == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Unknown binary operator");
          return FALSE;
        }

      g_string_append_c (sql, '(');
      if (!gom_sqlite_driver_append_expression_with_context (left, sql, bindings, error, context))
        return FALSE;

      g_string_append_c (sql, ' ');
      g_string_append (sql, op_sql);
      g_string_append_c (sql, ' ');

      if (!gom_sqlite_driver_append_expression_with_context (right, sql, bindings, error, context))
        return FALSE;

      g_string_append_c (sql, ')');

      return TRUE;
    }

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Unknown expression type");

  return FALSE;
}

static gboolean
gom_sqlite_driver_append_orderings_with_context (GPtrArray                         *orderings,
                                                 GString                           *sql,
                                                 GPtrArray                         *bindings,
                                                 GError                           **error,
                                                 const GomSqliteExpressionContext  *context)
{
  if (orderings == NULL || orderings->len == 0)
    return TRUE;

  for (guint i = 0; i < orderings->len; i++)
    {
      GomOrdering *ordering = g_ptr_array_index (orderings, i);
      GomExpression *expression = gom_ordering_get_expression (ordering);
      GomSortDirection direction = gom_ordering_get_direction (ordering);
      GomNullsMode nulls_mode = gom_ordering_get_nulls_mode (ordering);

      if (!gom_sqlite_driver_append_expression_with_context (expression,
                                                             sql,
                                                             bindings,
                                                             error,
                                                             context))
        return FALSE;

      if (direction == GOM_SORT_DESCENDING)
        g_string_append (sql, " DESC");

      if (nulls_mode == GOM_NULLS_FIRST)
        g_string_append (sql, " NULLS FIRST");
      else if (nulls_mode == GOM_NULLS_LAST)
        g_string_append (sql, " NULLS LAST");

      if (i + 1 < orderings->len)
        g_string_append (sql, ", ");
    }

  return TRUE;
}

static gboolean
gom_sqlite_driver_append_uint64_binding (GPtrArray  *bindings,
                                         guint64     value,
                                         GError    **error)
{
  GValue binding_value = G_VALUE_INIT;

  if (value > G_MAXINT64)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "SQLite cannot bind UINT64 value %" G_GUINT64_FORMAT,
                   value);
      return FALSE;
    }

  g_value_init (&binding_value, G_TYPE_INT64);
  g_value_set_int64 (&binding_value, (gint64)value);
  g_ptr_array_add (bindings, gom_sqlite_binding_new (&binding_value));
  g_value_unset (&binding_value);

  return TRUE;
}

static gboolean
gom_sqlite_driver_build_query_sql (GomQuery                          *query,
                                   const char                        *base_relation,
                                   const char                        *fts_relation,
                                   gboolean                           use_fts,
                                   const GomSqliteExpressionContext  *expression_context_ptr,
                                   gboolean                           use_projections,
                                   gboolean                           include_orderings,
                                   gboolean                           include_limit_offset,
                                   GString                          **out_sql,
                                   GPtrArray                        **out_bindings,
                                   GError                           **error)
{
  g_autoptr(GString) sql = NULL;
  g_autoptr(GPtrArray) bindings = NULL;
  GomExpression *filter;
  GomExpression *group_filter;
  GPtrArray *projections;
  GPtrArray *groupings;
  GPtrArray *orderings;

  g_assert (GOM_IS_QUERY (query));
  g_assert (base_relation != NULL);
  g_assert (out_sql != NULL);
  g_assert (out_bindings != NULL);

  sql = g_string_new ("SELECT ");
  bindings = g_ptr_array_new_with_free_func (gom_sqlite_binding_free);

  projections = _gom_query_get_projections (query);
  filter = _gom_query_get_filter (query);
  groupings = _gom_query_get_groupings (query);
  group_filter = _gom_query_get_group_filter (query);
  orderings = _gom_query_get_orderings (query);

  if (use_projections)
    {
      if (projections == NULL || projections->len == 0)
        {
          if (use_fts)
            g_string_append (sql, "t.*");
          else
            g_string_append (sql, "*");
        }
      else if (!gom_sqlite_driver_append_expression_list_with_context (projections,
                                                                       sql,
                                                                       bindings,
                                                                       error,
                                                                       expression_context_ptr))
        return FALSE;
    }
  else
    {
      g_string_append (sql, "1");
    }

  g_string_append (sql, " FROM ");

  if (use_fts)
    {
      gom_sqlite_driver_append_quoted_identifier_path (sql, fts_relation);
      g_string_append (sql, " AS fts JOIN ");
      gom_sqlite_driver_append_quoted_identifier_path (sql, base_relation);
      g_string_append (sql, " AS t ON t.rowid = fts.rowid");
    }
  else
    {
      gom_sqlite_driver_append_quoted_identifier_path (sql, base_relation);
    }

  if (filter != NULL)
    {
      g_string_append (sql, " WHERE ");

      if (!gom_sqlite_driver_append_expression_with_context (filter,
                                                             sql,
                                                             bindings,
                                                             error,
                                                             expression_context_ptr))
        return FALSE;
    }

  if (groupings != NULL && groupings->len > 0)
    {
      g_string_append (sql, " GROUP BY ");

      if (!gom_sqlite_driver_append_expression_list_with_context (groupings,
                                                                  sql,
                                                                  bindings,
                                                                  error,
                                                                  expression_context_ptr))
        return FALSE;
    }

  if (group_filter != NULL)
    {
      g_string_append (sql, " HAVING ");

      if (!gom_sqlite_driver_append_expression_with_context (group_filter,
                                                             sql,
                                                             bindings,
                                                             error,
                                                             expression_context_ptr))
        return FALSE;
    }

  if (include_orderings && orderings != NULL && orderings->len > 0)
    {
      g_string_append (sql, " ORDER BY ");

      if (!gom_sqlite_driver_append_orderings_with_context (orderings,
                                                            sql,
                                                            bindings,
                                                            error,
                                                            expression_context_ptr))
        return FALSE;
    }

  if (include_limit_offset)
    {
      gboolean has_limit = _gom_query_has_limit (query);
      gboolean has_offset = _gom_query_has_offset (query);

      if (has_limit)
        {
          guint64 limit = _gom_query_get_limit (query);

          g_string_append (sql, " LIMIT ?");

          if (!gom_sqlite_driver_append_uint64_binding (bindings, limit, error))
            return FALSE;
        }

      if (has_offset)
        {
          guint64 offset = _gom_query_get_offset (query);

          if (!has_limit)
            g_string_append (sql, " LIMIT -1");

          g_string_append (sql, " OFFSET ?");

          if (!gom_sqlite_driver_append_uint64_binding (bindings, offset, error))
            return FALSE;
        }
    }

  *out_sql = g_steal_pointer (&sql);
  *out_bindings = g_steal_pointer (&bindings);

  return TRUE;
}

static DexFuture *
gom_sqlite_driver_query_thread (gpointer user_data)
{
  GomSqliteQueryTask *task = user_data;
  g_autoptr(GString) sql = NULL;
  g_autoptr(GString) count_sql = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomSqliteStatement) statement = NULL;
  g_autoptr(GPtrArray) bindings = NULL;
  g_autoptr(GPtrArray) count_bindings = NULL;
  GomSqliteConnection *connection;
  GomExpression *filter;
  GomExpression *group_filter;
  GPtrArray *projections;
  GPtrArray *groupings;
  GPtrArray *orderings;
  const GomEntitySpec *entity = NULL;
  const char *relation;
  const char *base_relation = NULL;
  GType entity_type;
  GomRegistry *registry;
  g_autofree char *resolved_relation = NULL;
  g_autofree char *fts_relation = NULL;
  GomSqliteExpressionContext expression_context = { 0 };
  const GomSqliteExpressionContext *expression_context_ptr = NULL;
  gboolean use_fts = FALSE;
  gboolean relation_is_fts = FALSE;
  sqlite3 *db = NULL;
  sqlite3_stmt *count_stmt = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;
  guint64 count = 0;
  gboolean has_count = FALSE;
  gboolean owns_transaction = FALSE;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_QUERY (task->query));
  g_assert (GOM_IS_REPOSITORY (task->repository));
  registry = _gom_repository_get_registry (task->repository);

  GOM_TRACE_MARK ("Query", "plan", "backend=sqlite flags=%u", task->flags);

  projections = _gom_query_get_projections (task->query);
  filter = _gom_query_get_filter (task->query);
  groupings = _gom_query_get_groupings (task->query);
  group_filter = _gom_query_get_group_filter (task->query);
  orderings = _gom_query_get_orderings (task->query);

  relation = _gom_query_get_target_relation (task->query);
  entity_type = _gom_query_get_target_entity_type (task->query);
  resolved_relation = gom_sqlite_driver_resolve_target_relation (registry,
                                                                 entity_type,
                                                                 relation,
                                                                 "Query",
                                                                 &entity,
                                                                 &error);
  if (resolved_relation == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (entity_type == G_TYPE_INVALID)
    entity = NULL;

  base_relation = resolved_relation;
  relation_is_fts = g_str_has_suffix (base_relation, "_fts");
  use_fts = gom_sqlite_driver_expression_requires_fts (filter, (GomEntitySpec *)entity) ||
            gom_sqlite_driver_expression_list_requires_fts (projections, (GomEntitySpec *)entity) ||
            gom_sqlite_driver_expression_list_requires_fts (groupings, (GomEntitySpec *)entity) ||
            gom_sqlite_driver_expression_requires_fts (group_filter, (GomEntitySpec *)entity) ||
            gom_sqlite_driver_orderings_requires_fts (orderings, (GomEntitySpec *)entity) ||
            (!relation_is_fts && (gom_sqlite_driver_expression_contains_search (filter) ||
                                  gom_sqlite_driver_expression_list_contains_search (projections) ||
                                  gom_sqlite_driver_expression_list_contains_search (groupings) ||
                                  gom_sqlite_driver_expression_contains_search (group_filter) ||
                                  gom_sqlite_driver_orderings_contains_search (orderings)));

  if (entity != NULL || use_fts)
    {
      if (use_fts)
        {
          fts_relation = g_strdup_printf ("%s_fts", base_relation);
          expression_context.field_prefix = "t";
          expression_context.fts_prefix = "fts";
        }

      expression_context.entity = (GomEntitySpec *)entity;
      expression_context_ptr = &expression_context;
    }

  if ((task->flags & GOM_CURSOR_FLAGS_COUNT_ROWS) != 0)
    {
      g_autoptr(GString) base_sql = NULL;

      if (!gom_sqlite_driver_build_query_sql (task->query,
                                              base_relation,
                                              fts_relation,
                                              use_fts,
                                              expression_context_ptr,
                                              FALSE,
                                              FALSE,
                                              FALSE,
                                              &base_sql,
                                              &count_bindings,
                                              &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      count_sql = g_string_new ("SELECT COUNT(*) FROM (");
      g_string_append (count_sql, base_sql->str);
      g_string_append (count_sql, ")");

      connection = gom_sqlite_lease_state_get_connection (task->lease_state);
      db = gom_sqlite_connection_get_native (connection);

      if (!task->transaction_active)
        {
          if (!gom_sqlite_driver_exec_sql (db, "BEGIN", "begin query transaction", &error))
            {
              return dex_future_new_for_error (g_steal_pointer (&error));
            }

          owns_transaction = TRUE;
        }

      rc = gom_sqlite_driver_prepare (db,
                                      count_sql->str,
                                      &count_stmt,
                                      "prepare count statement",
                                      &error);
      if (rc != SQLITE_OK)
        {
          gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback query transaction", NULL);
          if (error != NULL)
            return dex_future_new_for_error (g_steal_pointer (&error));

          return dex_future_new_reject (GOM_ERROR,
                                        GOM_ERROR_PREPARE_FAILED,
                                        "Failed to prepare count statement: %s",
                                        sqlite3_errmsg (db));
        }

      for (guint i = 0; i < count_bindings->len; i++)
        {
          if (!gom_sqlite_driver_bind_value (count_stmt,
                                             i + 1,
                                             g_ptr_array_index (count_bindings, i),
                                             &error))
            {
              sqlite3_finalize (count_stmt);
              gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback query transaction", NULL);
              return dex_future_new_for_error (g_steal_pointer (&error));
            }
        }

      rc = gom_sqlite_driver_step (count_stmt, "step count statement", &error);
      if (rc == SQLITE_ROW)
        {
          count = (guint64)sqlite3_column_int64 (count_stmt, 0);
          has_count = TRUE;
        }
      else
        {
          sqlite3_finalize (count_stmt);
          gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback query transaction", NULL);
          if (error != NULL)
            return dex_future_new_for_error (g_steal_pointer (&error));

          return dex_future_new_reject (GOM_ERROR,
                                        GOM_ERROR_FAILED,
                                        "SQLite count failed: %s",
                                        sqlite3_errmsg (db));
        }

      sqlite3_finalize (count_stmt);
      count_stmt = NULL;
    }

  if (!gom_sqlite_driver_build_query_sql (task->query,
                                          base_relation,
                                          fts_relation,
                                          use_fts,
                                          expression_context_ptr,
                                          TRUE,
                                          TRUE,
                                          TRUE,
                                          &sql,
                                          &bindings,
                                          &error))
    {
      if (owns_transaction)
        {
          gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback query transaction", NULL);
        }
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!owns_transaction)
    {
      connection = gom_sqlite_lease_state_get_connection (task->lease_state);
      db = gom_sqlite_connection_get_native (connection);
    }

  rc = gom_sqlite_driver_prepare (db, sql->str, &stmt, "prepare query statement", &error);
  if (rc != SQLITE_OK)
    {
      if (owns_transaction)
        {
          gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback query transaction", NULL);
        }
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_PREPARE_FAILED,
                                    "Failed to prepare statement: %s",
                                    sqlite3_errmsg (db));
    }

  for (guint i = 0; i < bindings->len; i++)
    {
      if (!gom_sqlite_driver_bind_value (stmt, i + 1, g_ptr_array_index (bindings, i), &error))
        {
          sqlite3_finalize (stmt);
          if (owns_transaction)
            {
              gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback query transaction", NULL);
            }
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  statement = gom_sqlite_statement_new (task->lease_state, g_steal_pointer (&stmt));
  GOM_TRACE_END_MARK (start_time,
                      "Query",
                      "execute",
                      "backend=sqlite relation=%s rows=%s",
                      base_relation,
                      has_count ? "counted" : "streaming");
  return dex_future_new_take_object (gom_sqlite_cursor_new (statement,
                                                            task->repository,
                                                            g_string_free (g_steal_pointer (&sql), FALSE),
                                                            count,
                                                            has_count,
                                                            owns_transaction && !task->transaction_active,
                                                            entity_type));
}

static DexFuture *
gom_sqlite_driver_query_cb (DexFuture *completed,
                            gpointer   user_data)
{
  GomSqliteQueryRequest *request = user_data;
  const GValue *value;
  GomSqliteLease *lease;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (request != NULL);
  g_assert (GOM_IS_QUERY (request->query));

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  lease = g_value_get_object (value);
  lease_state = gom_sqlite_lease_ref_state (lease);
  {
    DexFuture *future = gom_sqlite_driver_query_on_lease (lease_state,
                                                          request->repository,
                                                          request->query,
                                                          request->flags,
                                                          FALSE);

    /* The cursor owns the lease state through its statement after this point. */
    g_object_run_dispose (G_OBJECT (lease));
    gom_sqlite_lease_state_unref (lease_state);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_query (GomDriver      *driver,
                         GomRepository  *repository,
                         GomQuery       *query,
                         GomCursorFlags  flags)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);
  GomSqliteQueryRequest *request;

  g_assert (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (GOM_IS_QUERY (query));

  request = g_new0 (GomSqliteQueryRequest, 1);
  request->query = g_object_ref (query);
  request->repository = g_object_ref (repository);
  request->flags = flags;

  return dex_future_then (gom_sqlite_pool_acquire (self->pool),
                          gom_sqlite_driver_query_cb,
                          request,
                          gom_sqlite_query_request_free);
}

static DexFuture *
gom_sqlite_driver_describe_cb (DexFuture *completed,
                               gpointer   user_data)
{
  char *relation = user_data;
  const GValue *value;
  GomSqliteDescribeTask *task;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (relation != NULL);

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  task = g_new0 (GomSqliteDescribeTask, 1);
  lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
  task->lease_state = lease_state;
  task->relation = g_strdup (relation);

  {
    DexFuture *future = gom_sqlite_lease_state_invoke (lease_state,
                                                       "[gom-sqlite-describe]",
                                                       gom_sqlite_driver_describe_thread,
                                                       task,
                                                       gom_sqlite_describe_task_free);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_describe_relation (GomDriver   *driver,
                                     GomRegistry *registry,
                                     const char  *relation)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);
  const GomEntitySpec *entity = NULL;
  const char *resolved_relation = relation;

  g_assert (GOM_IS_REGISTRY (registry));
  dex_return_error_if_fail (relation != NULL);

  if (!(entity = _gom_registry_lookup_entity_by_table (registry, relation)))
    entity = _gom_registry_lookup_entity_by_name (registry, relation);
  if (entity != NULL && gom_entity_spec_get_table ((GomEntitySpec *)entity) != NULL)
    resolved_relation = gom_entity_spec_get_table ((GomEntitySpec *)entity);

  return dex_future_then (gom_sqlite_pool_acquire (self->pool),
                          gom_sqlite_driver_describe_cb,
                          g_strdup (resolved_relation),
                          g_free);
}

static DexFuture *
gom_sqlite_driver_list_relations_thread (gpointer user_data)
{
  GomSqliteListRelationsTask *task = user_data;
  g_autoptr(GPtrArray) relations = NULL;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);

  relations = g_ptr_array_new_with_free_func (g_free);
  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  rc = gom_sqlite_driver_prepare (db,
                                  "SELECT name FROM sqlite_master "
                                  "WHERE type IN ('table', 'view') "
                                  "AND name NOT LIKE 'sqlite_%' "
                                  "ORDER BY name",
                                  &stmt,
                                  "prepare relation list",
                                  &error);
  if (rc != SQLITE_OK)
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_set_error (&error,
                   GOM_ERROR,
                   GOM_ERROR_PREPARE_FAILED,
                   "Failed to prepare relation list: %s",
                   sqlite3_errmsg (db));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  while ((rc = gom_sqlite_driver_step (stmt, "read relation list", &error)) == SQLITE_ROW)
    {
      const unsigned char *name = sqlite3_column_text (stmt, 0);

      if (name != NULL)
        g_ptr_array_add (relations, g_strdup ((const char *)name));
    }

  if (rc != SQLITE_DONE)
    {
      sqlite3_finalize (stmt);
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_set_error (&error,
                   GOM_ERROR,
                   GOM_ERROR_FAILED,
                   "Failed to read relation list: %s",
                   sqlite3_errmsg (db));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  sqlite3_finalize (stmt);

  return dex_future_new_take_boxed (G_TYPE_STRV, gom_sqlite_driver_ptr_array_to_strv (relations));
}

static DexFuture *
gom_sqlite_driver_list_relations_cb (DexFuture *completed,
                                     gpointer   user_data)
{
  const GValue *value;
  GomSqliteListRelationsTask *task;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (user_data == NULL);

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  task = g_new0 (GomSqliteListRelationsTask, 1);
  lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
  task->lease_state = lease_state;

  {
    DexFuture *future = gom_sqlite_lease_state_invoke (lease_state,
                                                       "[gom-sqlite-list-relations]",
                                                       gom_sqlite_driver_list_relations_thread,
                                                       task,
                                                       gom_sqlite_list_relations_task_free);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_list_relations (GomDriver   *driver,
                                  GomRegistry *registry)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);

  g_assert (GOM_IS_REGISTRY (registry));

  return dex_future_then (gom_sqlite_pool_acquire (self->pool),
                          gom_sqlite_driver_list_relations_cb,
                          NULL,
                          NULL);
}

static DexFuture *
gom_sqlite_driver_query_version_thread (gpointer user_data)
{
  GomSqliteLeaseState *lease_state = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  guint version = 0;
  int rc;

  g_assert (lease_state != NULL);

  connection = gom_sqlite_lease_state_get_connection (lease_state);
  db = gom_sqlite_connection_get_native (connection);

  rc = gom_sqlite_driver_prepare (db,
                                  "PRAGMA user_version",
                                  &stmt,
                                  "prepare PRAGMA user_version",
                                  &error);
  if (rc != SQLITE_OK)
    {
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_PREPARE_FAILED,
                                    "Failed to prepare PRAGMA user_version: %s",
                                    sqlite3_errmsg (db));
    }

  rc = gom_sqlite_driver_step (stmt, "query PRAGMA user_version", &error);
  if (rc != SQLITE_ROW)
    {
      sqlite3_finalize (stmt);
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_FAILED,
                                    "Failed to query PRAGMA user_version: %s",
                                    sqlite3_errmsg (db));
    }

  version = (guint)sqlite3_column_int (stmt, 0);
  sqlite3_finalize (stmt);
  return dex_future_new_for_uint (version);
}

static DexFuture *
gom_sqlite_driver_query_version_cb (DexFuture *completed,
                                    gpointer   user_data)
{
  const GValue *value;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (user_data == NULL);

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
  {
    DexFuture *future = gom_sqlite_lease_state_invoke (lease_state,
                                                       "[gom-sqlite-version]",
                                                       gom_sqlite_driver_query_version_thread,
                                                       lease_state,
                                                       (GDestroyNotify) gom_sqlite_lease_state_unref);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_query_version (GomDriver *driver)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);

  return dex_future_then (gom_sqlite_pool_acquire (self->pool),
                          gom_sqlite_driver_query_version_cb,
                          NULL,
                          NULL);
}

static DexFuture *
gom_sqlite_driver_migrate_thread (gpointer user_data)
{
  GomSqliteMigrateTask *task = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  g_autofree char *pragma_sql = NULL;
  guint next_version;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_REGISTRY (task->current));
  g_assert (GOM_IS_REGISTRY (task->next));

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  if (!gom_sqlite_driver_exec_sql (db,
                                   "BEGIN IMMEDIATE TRANSACTION",
                                   "begin migration transaction",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_sqlite_driver_apply_registry_migration (db, task->current, task->next, &error))
    {
      gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback migration transaction", NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  next_version = gom_registry_get_version (task->next);
  pragma_sql = g_strdup_printf ("PRAGMA user_version = %u", next_version);
  if (!gom_sqlite_driver_exec_sql (db, pragma_sql, "set schema version", &error))
    {
      gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback migration transaction", NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!gom_sqlite_driver_exec_sql (db, "COMMIT", "commit migration transaction", &error))
    {
      gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback migration transaction", NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  GOM_TRACE_END_MARK (start_time,
                      "Migration",
                      "apply",
                      "sqlite current=%u next=%u",
                      gom_registry_get_version (task->current),
                      next_version);
  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_driver_migrate_cb (DexFuture *completed,
                              gpointer   user_data)
{
  GomSqliteMigrateRequest *request = user_data;
  const GValue *value;
  GomSqliteMigrateTask *task;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (request != NULL);
  g_assert (GOM_IS_REGISTRY (request->current));
  g_assert (GOM_IS_REGISTRY (request->next));

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  task = g_new0 (GomSqliteMigrateTask, 1);
  lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
  task->lease_state = lease_state;
  task->current = g_object_ref (request->current);
  task->next = g_object_ref (request->next);

  {
    DexFuture *future = gom_sqlite_lease_state_invoke (lease_state,
                                                       "[gom-sqlite-migrate]",
                                                       gom_sqlite_driver_migrate_thread,
                                                       task,
                                                       gom_sqlite_migrate_task_free);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_migrate (GomDriver   *driver,
                           GomRegistry *current,
                           GomRegistry *next)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);
  GomSqliteMigrateRequest *request;
  GomSqliteWriteState *state;

  dex_return_error_if_fail (GOM_IS_REGISTRY (current));
  dex_return_error_if_fail (GOM_IS_REGISTRY (next));

  request = g_new0 (GomSqliteMigrateRequest, 1);
  request->current = g_object_ref (current);
  request->next = g_object_ref (next);

  state = g_new0 (GomSqliteWriteState, 1);
  state->driver = g_object_ref (self);
  state->operation = GOM_SQLITE_WRITE_MIGRATE;
  state->request.migrate = request;

  return gom_sqlite_driver_run_write_state (state);
}

static DexFuture *
gom_sqlite_driver_execute_sql_thread (gpointer user_data)
{
  GomSqliteExecuteTask *task = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  const guint8 *script_data;
  gsize script_len = 0;
  g_autofree char *script = NULL;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (task->script != NULL);

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  script_data = g_bytes_get_data (task->script, &script_len);
  if (script_data == NULL || script_len == 0)
    return dex_future_new_true ();

  if (memchr (script_data, '\0', script_len) != NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "SQL script contains NUL byte");

  script = g_strndup ((const char *)script_data, script_len);

  if (!gom_sqlite_driver_exec_sql (db,
                                   "BEGIN IMMEDIATE TRANSACTION",
                                   "begin SQL transaction",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_sqlite_driver_exec_sql_full (db, script, "execute SQL script", FALSE, &error))
    {
      gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback SQL transaction", NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!gom_sqlite_driver_exec_sql (db, "COMMIT", "commit SQL transaction", &error))
    {
      gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback SQL transaction", NULL);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  GOM_TRACE_END_MARK (start_time, "SQLite", "execute", "script");
  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_driver_execute_sql_cb (DexFuture *completed,
                                  gpointer   user_data)
{
  GomSqliteExecuteRequest *request = user_data;
  const GValue *value;
  GomSqliteExecuteTask *task;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (request != NULL);
  g_assert (request->script != NULL);

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  task = g_new0 (GomSqliteExecuteTask, 1);
  lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
  task->lease_state = lease_state;
  task->script = g_bytes_ref (request->script);

  {
    DexFuture *future = gom_sqlite_lease_state_invoke (lease_state,
                                                       "[gom-sqlite-execute-sql]",
                                                       gom_sqlite_driver_execute_sql_thread,
                                                       task,
                                                       gom_sqlite_execute_task_free);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_execute_sql (GomDriver *driver,
                               GBytes    *script)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);
  GomSqliteExecuteRequest *request;
  GomSqliteWriteState *state;

  dex_return_error_if_fail (script != NULL);

  request = g_new0 (GomSqliteExecuteRequest, 1);
  request->script = g_bytes_ref (script);

  state = g_new0 (GomSqliteWriteState, 1);
  state->driver = g_object_ref (self);
  state->operation = GOM_SQLITE_WRITE_EXECUTE_SQL;
  state->request.execute = request;

  return gom_sqlite_driver_run_write_state (state);
}

static DexFuture *
gom_sqlite_driver_begin_session_thread (gpointer user_data)
{
  GomSqliteSessionTask *task = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_REPOSITORY (task->repository));

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  if (!gom_sqlite_driver_exec_sql (db,
                                   "BEGIN IMMEDIATE TRANSACTION",
                                   "begin session transaction",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  GOM_TRACE_END_MARK (start_time, "Session", "open", "backend=sqlite");
  return dex_future_new_take_object (gom_sqlite_session_new (task->repository,
                                                             task->lease_state,
                                                             task->write_limiter));
}

static DexFuture *
gom_sqlite_driver_begin_session_cb (DexFuture *completed,
                                    gpointer   user_data)
{
  GomSqliteSessionRequest *request = user_data;
  const GValue *value;
  GomSqliteSessionTask *task;
  GomSqliteLeaseState *lease_state;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (request != NULL);
  g_assert (GOM_IS_REPOSITORY (request->repository));

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  task = g_new0 (GomSqliteSessionTask, 1);
  lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
  task->lease_state = lease_state;
  task->repository = g_object_ref (request->repository);
  task->write_limiter = dex_ref (request->write_limiter);

  {
    DexFuture *future = gom_sqlite_lease_state_invoke (lease_state,
                                                       "[gom-sqlite-session]",
                                                       gom_sqlite_driver_begin_session_thread,
                                                       task,
                                                       gom_sqlite_session_task_free);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_begin_session_complete_cb (DexFuture *completed,
                                             gpointer   user_data)
{
  DexLimiter *write_limiter = user_data;
  g_autoptr(GError) error = NULL;
  gpointer instance;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (DEX_IS_LIMITER (write_limiter));

  if (!(value = dex_future_get_value (completed, &error)))
    {
      dex_limiter_release (write_limiter);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  instance = G_VALUE_HOLDS_OBJECT (value) ? g_value_get_object (value) : NULL;
  if (!GOM_IS_SQLITE_SESSION (instance))
    dex_limiter_release (write_limiter);

  return dex_future_new_for_value (value);
}

static DexFuture *
gom_sqlite_driver_begin_session_acquired_cb (DexFuture *completed,
                                             gpointer   user_data)
{
  GomSqliteBeginSessionState *state = user_data;
  GomSqliteSessionRequest *request;
  g_autoptr(GError) error = NULL;
  DexFuture *future;
  DexLimiter *write_limiter;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_POOL (state->pool));
  g_assert (state->request != NULL);

  request = state->request;

  g_assert (request != NULL);
  g_assert (GOM_IS_REPOSITORY (request->repository));
  g_assert (DEX_IS_LIMITER (request->write_limiter));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  write_limiter = dex_ref (request->write_limiter);
  future = dex_future_then (gom_sqlite_pool_acquire (state->pool),
                            gom_sqlite_driver_begin_session_cb,
                            g_steal_pointer (&state->request),
                            gom_sqlite_session_request_free);

  return dex_future_finally (future,
                             gom_sqlite_driver_begin_session_complete_cb,
                             write_limiter,
                             dex_unref);
}

static DexFuture *
gom_sqlite_driver_begin_session (GomDriver     *driver,
                                 GomRepository *repository)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);
  GomSqliteSessionRequest *request;
  GomSqliteBeginSessionState *state;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);

  request = g_new0 (GomSqliteSessionRequest, 1);
  request->repository = g_object_ref (repository);
  request->write_limiter = dex_ref (self->write_limiter);

  state = g_new0 (GomSqliteBeginSessionState, 1);
  state->pool = g_object_ref (self->pool);
  state->request = request;

  return dex_future_then (dex_limiter_acquire (self->write_limiter),
                          gom_sqlite_driver_begin_session_acquired_cb,
                          state,
                          gom_sqlite_begin_session_state_free);
}

static gboolean
gom_sqlite_driver_supports_feature (GomDriver            *driver,
                                    GomRepositoryFeature  feature)
{
  g_return_val_if_fail (GOM_IS_SQLITE_DRIVER (driver), FALSE);

  switch (feature)
    {
    case GOM_REPOSITORY_FEATURE_VECTOR_SEARCH:
#if HAVE_SQLITE_VEC1
      return TRUE;
#else
      return FALSE;
#endif

    default:
      return FALSE;
    }
}

static gboolean
gom_sqlite_driver_supports_vector_distance (GomDriver       *driver,
                                            GomVectorFormat  format,
                                            GomVectorMetric  metric)
{
  g_return_val_if_fail (GOM_IS_SQLITE_DRIVER (driver), FALSE);

#if HAVE_SQLITE_VEC1 && G_BYTE_ORDER == G_LITTLE_ENDIAN
  if (format != GOM_VECTOR_FORMAT_FLOAT32_LE)
    return FALSE;

  return metric == GOM_VECTOR_METRIC_COSINE || metric == GOM_VECTOR_METRIC_L2;
#else
  return FALSE;
#endif
}

static const GomEntitySpec *
gom_sqlite_driver_lookup_entity_for_type (GomRegistry  *registry,
                                          GType         entity_type,
                                          const char   *operation_name,
                                          GError      **error)
{
  const GomEntitySpec *entity;

  g_assert (GOM_IS_REGISTRY (registry));
  g_assert (operation_name != NULL);

  if (entity_type == G_TYPE_INVALID)
    return NULL;

  if (!g_type_is_a (entity_type, GOM_TYPE_ENTITY))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "%s target entity type '%s' is not a GomEntity",
                   operation_name,
                   g_type_name (entity_type));
      return NULL;
    }

  if (!(entity = _gom_registry_lookup_entity_by_type (registry, entity_type)))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "%s target entity type '%s' is not in repository registry",
                   operation_name,
                   g_type_name (entity_type));
      return NULL;
    }

  return entity;
}

static const GomEntitySpec *
gom_sqlite_driver_lookup_entity_for_relation (GomRegistry *registry,
                                              const char  *relation)
{
  g_assert (GOM_IS_REGISTRY (registry));

  if (relation == NULL || *relation == '\0')
    return NULL;

  return _gom_registry_lookup_entity_by_table (registry, relation);
}

static char *
gom_sqlite_driver_resolve_target_relation (GomRegistry          *registry,
                                           GType                 target_entity_type,
                                           const char           *target_relation,
                                           const char           *operation_name,
                                           const GomEntitySpec **out_entity,
                                           GError              **error)
{
  static const char fts_suffix[] = "_fts";
  const GomEntitySpec *entity_from_type = NULL;
  const GomEntitySpec *entity_from_relation = NULL;

  g_assert (GOM_IS_REGISTRY (registry));
  g_assert (operation_name != NULL);

  if (out_entity != NULL)
    *out_entity = NULL;

  entity_from_type = gom_sqlite_driver_lookup_entity_for_type (registry,
                                                               target_entity_type,
                                                               operation_name,
                                                               error);
  if (target_entity_type != G_TYPE_INVALID && entity_from_type == NULL)
    return NULL;

  if (!gom_str_empty0 (target_relation))
    {
      entity_from_relation = gom_sqlite_driver_lookup_entity_for_relation (registry, target_relation);

      if (entity_from_relation == NULL && g_str_has_suffix (target_relation, fts_suffix))
        {
          g_autofree char *base_relation = g_strndup (target_relation,
                                                      strlen (target_relation) - strlen (fts_suffix));

          if ((entity_from_relation = gom_sqlite_driver_lookup_entity_for_relation (registry, base_relation)))
            {
              if (out_entity != NULL)
                *out_entity = entity_from_type != NULL ? entity_from_type : entity_from_relation;

              return g_steal_pointer (&base_relation);
            }
        }

      if (entity_from_type != NULL)
        {
          const char *registered_table = gom_entity_spec_get_table ((GomEntitySpec *) entity_from_type);

          if (registered_table == NULL || *registered_table == '\0')
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "%s target entity '%s' has no mapped table",
                           operation_name,
                           gom_entity_spec_get_name ((GomEntitySpec *)entity_from_type));
              return NULL;
            }

          if (g_strcmp0 (target_relation, registered_table) != 0)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "%s target relation '%s' does not match entity table '%s'",
                           operation_name,
                           target_relation,
                           registered_table);
              return NULL;
            }
        }

      if (out_entity != NULL)
        *out_entity = entity_from_type != NULL ? entity_from_type : entity_from_relation;

      return g_strdup (target_relation);
    }

  if (entity_from_type != NULL)
    {
      const char *table = NULL;

      table = gom_entity_spec_get_table ((GomEntitySpec *)entity_from_type);

      if (table == NULL || *table == '\0')
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "%s target entity '%s' has no mapped table",
                       operation_name,
                       gom_entity_spec_get_name ((GomEntitySpec *)entity_from_type));
          return NULL;
        }

      if (out_entity != NULL)
        *out_entity = entity_from_type;

      return g_strdup (table);
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_ARGUMENT,
               "%s requires a target relation or entity type",
               operation_name);
  return NULL;
}

static void
gom_sqlite_driver_mutation_result_append (GomMutationResult *result,
                                          guint64            changes,
                                          gboolean           has_rowid,
                                          gint64             rowid)
{
  const char * const column_names[] = {
    "changes",
    "rowid",
  };
  GValue values[2] = { G_VALUE_INIT, G_VALUE_INIT };
  g_autoptr(GomRecord) record = NULL;

  g_assert (GOM_IS_MUTATION_RESULT (result));

  g_value_init (&values[0], G_TYPE_INT64);
  g_value_set_int64 (&values[0], (gint64)changes);

  if (has_rowid)
    {
      g_value_init (&values[1], G_TYPE_INT64);
      g_value_set_int64 (&values[1], rowid);
    }
  else
    {
      g_value_init (&values[1], G_TYPE_POINTER);
      g_value_set_pointer (&values[1], NULL);
    }

  record = _gom_record_new_from_values (column_names, values, G_N_ELEMENTS (values));
  _gom_mutation_result_append_record (result, record, changes);

  g_value_unset (&values[0]);
  g_value_unset (&values[1]);
}

static void
gom_sqlite_driver_mutation_result_append_changes_rowid (GomMutationResult *result,
                                                        guint64            changes,
                                                        gint64             rowid)
{
  gom_sqlite_driver_mutation_result_append (result, changes, TRUE, rowid);
}

static void
gom_sqlite_driver_mutation_result_append_changes_only (GomMutationResult *result,
                                                       guint64            changes)
{
  gom_sqlite_driver_mutation_result_append (result, changes, FALSE, 0);
}

static DexFuture *
gom_sqlite_driver_insertion_thread (GomSqliteMutationTask *task)
{
  GomInsertion *insertion;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autofree char *relation = NULL;
  const GomEntitySpec *entity = NULL;
  GomSqliteExpressionContext expression_context = { 0 };
  const GomSqliteExpressionContext *expression_context_ptr = NULL;
  GomSqliteConnection *connection;
  GPtrArray *columns;
  GPtrArray *rows;
  GType entity_type;
  GomRegistry *registry;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  int rc;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_INSERTION (task->mutation));
  g_assert (GOM_IS_REGISTRY (task->registry));
  registry = task->registry;

  GOM_TRACE_MARK ("Mutation", "insert", "backend=sqlite");

  insertion = GOM_INSERTION (task->mutation);
  columns = _gom_insertion_get_columns (insertion);
  rows = _gom_insertion_get_rows (insertion);
  entity_type = _gom_insertion_get_target_entity_type (insertion);
  relation = gom_sqlite_driver_resolve_target_relation (registry,
                                                        entity_type,
                                                        _gom_insertion_get_target_relation (insertion),
                                                        "Insertion",
                                                        &entity,
                                                        &error);
  if (relation == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (entity_type == G_TYPE_INVALID)
    entity = NULL;

  if (entity != NULL)
    {
      expression_context.entity = (GomEntitySpec *)entity;
      expression_context_ptr = &expression_context;
    }

  if (columns == NULL || columns->len == 0)
    {
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Insertion requires at least one column");
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (rows == NULL || rows->len == 0)
    {
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Insertion requires at least one row");
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  result = _gom_mutation_result_new ();

  if (!gom_sqlite_driver_exec_sql (db,
                                   "SAVEPOINT gom_sqlite_insert",
                                   "begin insert transaction",
                                   &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < rows->len; i++)
    {
      GPtrArray *row = g_ptr_array_index (rows, i);
      g_autoptr(GString) row_sql = NULL;
      g_autoptr(GPtrArray) row_bindings = NULL;

      if (row->len != columns->len)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Insertion row length does not match columns");
          goto rollback_insert;
        }

      row_sql = g_string_new ("INSERT INTO ");
      row_bindings = g_ptr_array_new_with_free_func (gom_sqlite_binding_free);
      gom_sqlite_driver_append_quoted_identifier_path (row_sql, relation);

      g_string_append (row_sql, " (");
      if (!gom_sqlite_driver_append_expression_list_with_context (columns,
                                                                  row_sql,
                                                                  row_bindings,
                                                                  &error,
                                                                  expression_context_ptr))
        goto rollback_insert;
      g_string_append (row_sql, ") VALUES (");

      if (!gom_sqlite_driver_append_expression_list (row, row_sql, row_bindings, &error))
        goto rollback_insert;

      g_string_append_c (row_sql, ')');

      rc = gom_sqlite_driver_prepare (db, row_sql->str, &stmt, "prepare insert statement", &error);
      if (rc != SQLITE_OK)
        {
          if (error != NULL)
            goto rollback_insert;

          g_set_error (&error,
                       GOM_ERROR,
                       GOM_ERROR_PREPARE_FAILED,
                       "Failed to prepare statement: %s",
                       sqlite3_errmsg (db));
          goto rollback_insert;
        }

      for (guint j = 0; j < row_bindings->len; j++)
        {
          if (!gom_sqlite_driver_bind_value (stmt,
                                             j + 1,
                                             g_ptr_array_index (row_bindings, j),
                                             &error))
            {
              sqlite3_finalize (stmt);
              stmt = NULL;
              goto rollback_insert;
            }
        }

      rc = gom_sqlite_driver_step (stmt, "step insert statement", &error);
      if (rc != SQLITE_DONE)
        {
          sqlite3_finalize (stmt);
          stmt = NULL;

          if (error == NULL)
            {
              if ((rc & 0xff) == SQLITE_CONSTRAINT)
                g_set_error (&error,
                             GOM_ERROR,
                             GOM_ERROR_CONSTRAINT,
                             "SQLite insert failed: %s",
                             sqlite3_errmsg (db));
              else
                g_set_error (&error,
                             GOM_ERROR,
                             GOM_ERROR_INSERT_FAILED,
                             "SQLite insert failed: %s",
                             sqlite3_errmsg (db));
            }

          goto rollback_insert;
        }

      if (stmt != NULL)
        {
          sqlite3_finalize (stmt);
          stmt = NULL;
        }

      gom_sqlite_driver_mutation_result_append_changes_rowid (result,
                                                              (guint64) sqlite3_changes (db),
                                                              (gint64) sqlite3_last_insert_rowid (db));
    }

  if (!gom_sqlite_driver_exec_sql (db,
                                   "RELEASE SAVEPOINT gom_sqlite_insert",
                                   "commit insert transaction",
                                   &error))
    {
      GOM_TRACE_END_MARK (start_time, "Mutation", "insert", "failed: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  GOM_TRACE_END_MARK (start_time, "Mutation", "insert", "relation=%s rows=%u", relation, rows->len);
  return dex_future_new_take_object (g_steal_pointer (&result));

rollback_insert:
  if (stmt != NULL)
    sqlite3_finalize (stmt);

  gom_sqlite_driver_exec_sql (db,
                              "ROLLBACK TO SAVEPOINT gom_sqlite_insert",
                              "rollback insert transaction",
                              NULL);
  gom_sqlite_driver_exec_sql (db,
                              "RELEASE SAVEPOINT gom_sqlite_insert",
                              "release insert transaction",
                              NULL);
  return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
gom_sqlite_driver_update_thread (GomSqliteMutationTask *task)
{
  GomUpdate *update;
  g_autoptr(GString) sql = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GPtrArray) bindings = NULL;
  g_autofree char *relation = NULL;
  const GomEntitySpec *entity = NULL;
  GomSqliteExpressionContext expression_context = { 0 };
  const GomSqliteExpressionContext *expression_context_ptr = NULL;
  GomSqliteConnection *connection;
  GPtrArray *columns;
  GPtrArray *values;
  GomExpression *filter;
  GType entity_type;
  GomRegistry *registry;
  gboolean has_limit;
  guint64 limit = 0;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  int rc;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_UPDATE (task->mutation));
  g_assert (GOM_IS_REGISTRY (task->registry));
  registry = task->registry;

  GOM_TRACE_MARK ("Mutation", "update", "backend=sqlite");

  update = GOM_UPDATE (task->mutation);
  columns = _gom_update_get_columns (update);
  values = _gom_update_get_values (update);
  filter = _gom_update_get_filter (update);
  entity_type = _gom_update_get_target_entity_type (update);
  has_limit = _gom_update_has_limit (update);
  if (has_limit)
    limit = _gom_update_get_limit (update);

  relation = gom_sqlite_driver_resolve_target_relation (registry,
                                                        entity_type,
                                                        _gom_update_get_target_relation (update),
                                                        "Update",
                                                        &entity,
                                                        &error);
  if (relation == NULL)
    {
      GOM_TRACE_END_MARK (start_time, "Mutation", "update", "failed: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (entity_type == G_TYPE_INVALID)
    entity = NULL;

  if (entity != NULL)
    {
      expression_context.entity = (GomEntitySpec *)entity;
      expression_context_ptr = &expression_context;
    }

  if (columns == NULL || values == NULL || columns->len == 0 || columns->len != values->len)
    {
      g_set_error_literal (&error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Update requires at least one assignment");
      GOM_TRACE_END_MARK (start_time, "Mutation", "update", "invalid assignments");
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  sql = g_string_new ("UPDATE ");
  bindings = g_ptr_array_new_with_free_func (gom_sqlite_binding_free);

  gom_sqlite_driver_append_quoted_identifier_path (sql, relation);
  g_string_append (sql, " SET ");

  for (guint i = 0; i < columns->len; i++)
    {
      GomExpression *column = g_ptr_array_index (columns, i);
      GomExpression *value = g_ptr_array_index (values, i);

      if (!GOM_IS_FIELD_EXPRESSION (column))
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Update assignment targets must be field expressions");
          return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (!gom_sqlite_driver_append_expression_with_context (column,
                                                             sql,
                                                             bindings,
                                                             &error,
                                                             expression_context_ptr))
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_string_append (sql, " = ");

      if (!gom_sqlite_driver_append_expression_with_context (value,
                                                             sql,
                                                             bindings,
                                                             &error,
                                                             expression_context_ptr))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (i + 1 < columns->len)
        g_string_append (sql, ", ");
    }

  if (has_limit)
    {
      g_string_append (sql, " WHERE rowid IN (SELECT rowid FROM ");
      gom_sqlite_driver_append_quoted_identifier_path (sql, relation);

      if (filter != NULL)
        {
          g_string_append (sql, " WHERE ");
          if (!gom_sqlite_driver_append_expression_with_context (filter,
                                                                 sql,
                                                                 bindings,
                                                                 &error,
                                                                 expression_context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }

      g_string_append (sql, " LIMIT ?)");

      if (!gom_sqlite_driver_append_uint64_binding (bindings, limit, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else if (filter != NULL)
    {
      g_string_append (sql, " WHERE ");
      if (!gom_sqlite_driver_append_expression_with_context (filter,
                                                             sql,
                                                             bindings,
                                                             &error,
                                                             expression_context_ptr))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  rc = gom_sqlite_driver_prepare (db, sql->str, &stmt, "prepare update statement", &error);
  if (rc != SQLITE_OK)
    {
      GOM_TRACE_END_MARK (start_time, "Mutation", "update", "prepare failed");
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_PREPARE_FAILED,
                                    "Failed to prepare statement: %s",
                                    sqlite3_errmsg (db));
    }

  for (guint i = 0; i < bindings->len; i++)
    {
      if (!gom_sqlite_driver_bind_value (stmt, i + 1, g_ptr_array_index (bindings, i), &error))
        {
          sqlite3_finalize (stmt);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  rc = gom_sqlite_driver_step (stmt, "step update statement", &error);
  if (rc != SQLITE_DONE)
    {
      sqlite3_finalize (stmt);
      GOM_TRACE_END_MARK (start_time, "Mutation", "update", "execute failed");
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_UPDATE_FAILED,
                                    "SQLite update failed: %s",
                                    sqlite3_errmsg (db));
    }

  if (stmt != NULL)
    {
      sqlite3_finalize (stmt);
      stmt = NULL;
    }
  result = _gom_mutation_result_new ();
  gom_sqlite_driver_mutation_result_append_changes_only (result, (guint64)sqlite3_changes (db));
  GOM_TRACE_END_MARK (start_time, "Mutation", "update", "relation=%s", relation);
  return dex_future_new_take_object (g_steal_pointer (&result));
}

static DexFuture *
gom_sqlite_driver_deletion_thread (GomSqliteMutationTask *task)
{
  GomDeletion *deletion;
  g_autoptr(GString) sql = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GPtrArray) bindings = NULL;
  g_autofree char *relation = NULL;
  const GomEntitySpec *entity = NULL;
  GomSqliteExpressionContext expression_context = { 0 };
  const GomSqliteExpressionContext *expression_context_ptr = NULL;
  GomSqliteConnection *connection;
  GomExpression *filter;
  GType entity_type;
  GomRegistry *registry;
  gboolean has_limit;
  guint64 limit = 0;
  sqlite3 *db;
  sqlite3_stmt *stmt = NULL;
  int rc;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_DELETION (task->mutation));
  g_assert (GOM_IS_REGISTRY (task->registry));
  registry = task->registry;

  GOM_TRACE_MARK ("Mutation", "delete", "backend=sqlite");

  deletion = GOM_DELETION (task->mutation);
  filter = _gom_deletion_get_filter (deletion);
  entity_type = _gom_deletion_get_target_entity_type (deletion);
  has_limit = _gom_deletion_has_limit (deletion);
  if (has_limit)
    limit = _gom_deletion_get_limit (deletion);

  relation = gom_sqlite_driver_resolve_target_relation (registry,
                                                        entity_type,
                                                        _gom_deletion_get_target_relation (deletion),
                                                        "Deletion",
                                                        &entity,
                                                        &error);
  if (relation == NULL)
    {
      GOM_TRACE_END_MARK (start_time, "Mutation", "delete", "failed: %s", error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (entity_type == G_TYPE_INVALID)
    entity = NULL;

  if (entity != NULL)
    {
      expression_context.entity = (GomEntitySpec *)entity;
      expression_context_ptr = &expression_context;
    }

  sql = g_string_new ("DELETE FROM ");
  bindings = g_ptr_array_new_with_free_func (gom_sqlite_binding_free);
  gom_sqlite_driver_append_quoted_identifier_path (sql, relation);

  if (has_limit)
    {
      g_string_append (sql, " WHERE rowid IN (SELECT rowid FROM ");
      gom_sqlite_driver_append_quoted_identifier_path (sql, relation);

      if (filter != NULL)
        {
          g_string_append (sql, " WHERE ");
          if (!gom_sqlite_driver_append_expression_with_context (filter,
                                                                 sql,
                                                                 bindings,
                                                                 &error,
                                                                 expression_context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }

      g_string_append (sql, " LIMIT ?)");

      if (!gom_sqlite_driver_append_uint64_binding (bindings, limit, &error))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }
  else if (filter != NULL)
    {
      g_string_append (sql, " WHERE ");
      if (!gom_sqlite_driver_append_expression_with_context (filter,
                                                             sql,
                                                             bindings,
                                                             &error,
                                                             expression_context_ptr))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  connection = gom_sqlite_lease_state_get_connection (task->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  rc = gom_sqlite_driver_prepare (db, sql->str, &stmt, "prepare delete statement", &error);
  if (rc != SQLITE_OK)
    {
      GOM_TRACE_END_MARK (start_time, "Mutation", "delete", "prepare failed");
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_PREPARE_FAILED,
                                    "Failed to prepare statement: %s",
                                    sqlite3_errmsg (db));
    }

  for (guint i = 0; i < bindings->len; i++)
    {
      if (!gom_sqlite_driver_bind_value (stmt, i + 1, g_ptr_array_index (bindings, i), &error))
        {
          sqlite3_finalize (stmt);
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  rc = gom_sqlite_driver_step (stmt, "step delete statement", &error);
  if (rc != SQLITE_DONE)
    {
      sqlite3_finalize (stmt);
      GOM_TRACE_END_MARK (start_time, "Mutation", "delete", "execute failed");
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      return dex_future_new_reject (GOM_ERROR,
                                    GOM_ERROR_DELETE_FAILED,
                                    "SQLite delete failed: %s",
                                    sqlite3_errmsg (db));
    }

  if (stmt != NULL)
    {
      sqlite3_finalize (stmt);
      stmt = NULL;
    }
  result = _gom_mutation_result_new ();
  gom_sqlite_driver_mutation_result_append_changes_only (result, (guint64)sqlite3_changes (db));
  GOM_TRACE_END_MARK (start_time, "Mutation", "delete", "relation=%s", relation);
  return dex_future_new_take_object (g_steal_pointer (&result));
}

static DexFuture *
gom_sqlite_driver_mutate_thread (gpointer user_data)
{
  GomSqliteMutationTask *task = user_data;

  g_assert (task != NULL);
  g_assert (task->lease_state != NULL);
  g_assert (GOM_IS_MUTATION (task->mutation));

  GOM_TRACE_MARK ("Mutation",
                  "apply",
                  "backend=sqlite type=%s",
                  G_OBJECT_TYPE_NAME (task->mutation));

  if (GOM_IS_INSERTION (task->mutation))
    return gom_sqlite_driver_insertion_thread (task);
  else if (GOM_IS_UPDATE (task->mutation))
    return gom_sqlite_driver_update_thread (task);
  else if (GOM_IS_DELETION (task->mutation))
    return gom_sqlite_driver_deletion_thread (task);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Unsupported mutation type %s",
                                G_OBJECT_TYPE_NAME (task->mutation));
}

static DexFuture *
gom_sqlite_driver_mutate_cb (DexFuture *completed,
                             gpointer   user_data)
{
  GomSqliteMutationRequest *request = user_data;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (request != NULL);
  g_assert (GOM_IS_MUTATION (request->mutation));
  g_assert (GOM_IS_REGISTRY (request->registry));

  value = dex_future_get_value (completed, NULL);

  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_LEASE));

  {
    GomSqliteLeaseState *lease_state = gom_sqlite_lease_ref_state (g_value_get_object (value));
    DexFuture *future = gom_sqlite_driver_mutate_on_lease (lease_state,
                                                           request->registry,
                                                           request->mutation);
    gom_sqlite_lease_state_unref (lease_state);
    return future;
  }
}

static DexFuture *
gom_sqlite_driver_mutate (GomDriver   *driver,
                          GomRegistry *registry,
                          GomMutation *mutation)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (driver);
  GomSqliteMutationRequest *request;
  GomSqliteWriteState *state;

  g_assert (GOM_IS_REGISTRY (registry));
  dex_return_error_if_fail (GOM_IS_MUTATION (mutation));

  request = g_new0 (GomSqliteMutationRequest, 1);
  request->mutation = g_object_ref (mutation);
  request->registry = g_object_ref (registry);

  state = g_new0 (GomSqliteWriteState, 1);
  state->driver = g_object_ref (self);
  state->operation = GOM_SQLITE_WRITE_MUTATE;
  state->request.mutation = request;

  return gom_sqlite_driver_run_write_state (state);
}

static void
gom_sqlite_driver_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GomSqliteDriver *self = GOM_SQLITE_DRIVER (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static char *
gom_sqlite_driver_dup_uri (GomDriver *driver)
{
  return g_strdup (GOM_SQLITE_DRIVER (driver)->uri);
}

static DexFuture *
gom_sqlite_driver_rekey (GomDriver        *driver,
                         GomDriverOptions *options)
{
  GomSqliteDriver *self = (GomSqliteDriver *)driver;
  g_autoptr(GBytes) encryption_key = NULL;
  GomSqliteWriteState *state;

  g_assert (GOM_IS_SQLITE_DRIVER (self));
  dex_return_error_if_fail (GOM_IS_DRIVER_OPTIONS (options));

  encryption_key = gom_driver_options_dup_encryption_key (options);
  if (encryption_key != NULL && g_bytes_get_size (encryption_key) == 0)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Encryption key is empty");

  state = g_new0 (GomSqliteWriteState, 1);
  state->driver = g_object_ref (self);
  state->operation = GOM_SQLITE_WRITE_REKEY;
  state->request.rekey = g_steal_pointer (&encryption_key);

  return gom_sqlite_driver_run_write_state (state);
}

static void
gom_sqlite_driver_finalize (GObject *object)
{
  GomSqliteDriver *self = (GomSqliteDriver *)object;

  if (self->write_limiter != NULL)
    dex_limiter_close (self->write_limiter);

  g_clear_object (&self->pool);
  dex_clear (&self->write_limiter);
  g_clear_pointer (&self->encryption_key, g_bytes_unref);
  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (gom_sqlite_driver_parent_class)->finalize (object);
}

static void
gom_sqlite_driver_class_init (GomSqliteDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomDriverClass *driver_class = GOM_DRIVER_CLASS (klass);

  object_class->finalize = gom_sqlite_driver_finalize;
  object_class->get_property = gom_sqlite_driver_get_property;

  driver_class->begin_session = gom_sqlite_driver_begin_session;
  driver_class->describe_relation = gom_sqlite_driver_describe_relation;
  driver_class->dup_uri = gom_sqlite_driver_dup_uri;
  driver_class->execute_sql = gom_sqlite_driver_execute_sql;
  driver_class->list_relations = gom_sqlite_driver_list_relations;
  driver_class->migrate = gom_sqlite_driver_migrate;
  driver_class->mutate = gom_sqlite_driver_mutate;
  driver_class->query = gom_sqlite_driver_query;
  driver_class->query_version = gom_sqlite_driver_query_version;
  driver_class->rekey = gom_sqlite_driver_rekey;
  driver_class->supports_feature = gom_sqlite_driver_supports_feature;
  driver_class->supports_vector_distance = gom_sqlite_driver_supports_vector_distance;

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_sqlite_driver_init (GomSqliteDriver *self)
{
  self->write_limiter = dex_limiter_new (1);
}

G_MODULE_EXPORT GomDriver *
_gom_sqlite_driver_new (const char        *uri,
                        GomDriverOptions  *options,
                        GError           **error)
{
  g_autoptr(GUri) guri = NULL;
  g_autoptr(GBytes) encryption_key = NULL;
  GomSqliteDriver *self;

  if (uri == NULL || !(guri = g_uri_parse (uri, G_URI_FLAGS_NONE, error)))
    return NULL;

  if (options != NULL)
    encryption_key = gom_driver_options_dup_encryption_key (options);

  if (sqlite3_initialize () != SQLITE_OK)
    {
      g_set_error (error,
                   GOM_ERROR,
                   GOM_ERROR_BACKEND_INITIALIZATION_FAILED,
                   "Failed to initialize SQLite");
      return NULL;
    }

  self = g_object_new (GOM_TYPE_SQLITE_DRIVER, NULL);
  self->uri = g_strdup (uri);
  if (encryption_key != NULL)
    self->encryption_key = g_bytes_ref (encryption_key);
  self->pool = gom_sqlite_pool_new (uri, encryption_key);

  return GOM_DRIVER (g_steal_pointer (&self));
}
