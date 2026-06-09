/* test-util.h
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

#include <libdex.h>
#include <glib/gstdio.h>
#include <libgom.h>

#ifdef GOM_DATABASE_SQLITE
#include <sqlite3.h>
#endif

G_BEGIN_DECLS

static inline DexFuture *
_dex_test_runner_fiber (gpointer user_data)
{
  GTestFunc func = (GTestFunc) G_CALLBACK (user_data);
  func ();
  return dex_future_new_true ();
}

static inline DexFuture *
_dex_test_runner_quit (DexFuture *future,
                       gpointer   user_data)
{
  g_main_loop_quit (user_data);
  return dex_ref (future);
}

static inline void
_dex_test_runner (gconstpointer user_data)
{
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);

  dex_init ();

  dex_future_disown (dex_future_finally (dex_scheduler_spawn (NULL,
                                                              /* pthread often defaults to 8mb */
                                                              8*1024*1024,
                                                              _dex_test_runner_fiber,
                                                              (gpointer)user_data,
                                                              NULL),
                                         _dex_test_runner_quit,
                                         g_main_loop_ref (main_loop),
                                         (GDestroyNotify) g_main_loop_unref));

  g_main_loop_run (main_loop);
  g_main_loop_unref (main_loop);
}

static inline void
_g_test_add_func (const char *path,
                  GTestFunc   test_func)
{
  g_test_add_data_func (path, test_func, _dex_test_runner);
}

static inline void
test_remove_directory_recursive (const char *path)
{
  GDir *dir;
  const gchar *name;

  dir = g_dir_open (path, 0, NULL);
  if (dir == NULL)
    return;

  while ((name = g_dir_read_name (dir)) != NULL)
    {
      g_autofree char *child = g_build_filename (path, name, NULL);

      if (g_file_test (child, G_FILE_TEST_IS_DIR))
        test_remove_directory_recursive (child);
      else
        g_unlink (child);
    }

  g_dir_close (dir);
  g_rmdir (path);
}

#ifdef GOM_DATABASE_SQLITE

typedef struct
{
  char *tmpdir;
  char *db_path;
  char *db_uri;
  GomDriver *driver;
} TestSqliteContext;

static inline void
test_sqlite_context_clear (TestSqliteContext *context)
{
  g_clear_object (&context->driver);
  g_clear_pointer (&context->db_uri, g_free);
  g_clear_pointer (&context->db_path, g_free);

  if (context->tmpdir != NULL)
    test_remove_directory_recursive (context->tmpdir);

  g_clear_pointer (&context->tmpdir, g_free);
}

static inline gboolean
test_sqlite_context_init (TestSqliteContext  *context,
                          const char         *tmp_pattern,
                          GError            **error)
{
  context->tmpdir = g_dir_make_tmp (tmp_pattern, error);
  if (context->tmpdir == NULL)
    return FALSE;

  context->db_path = g_build_filename (context->tmpdir, "test.db", NULL);
  context->db_uri = g_filename_to_uri (context->db_path, NULL, error);
  if (context->db_uri == NULL)
    goto fail;

  context->driver = gom_driver_open (context->db_uri, error);
  if (context->driver == NULL)
    {
      if (error != NULL && *error != NULL)
        g_printerr ("Failed to open sqlite driver: %s\n", (*error)->message);
      goto fail;
    }

  return TRUE;

fail:
  test_sqlite_context_clear (context);
  return FALSE;
}

static inline GomRepository *
test_sqlite_context_create_repository (TestSqliteContext  *context,
                                       GomRegistry        *registry,
                                       GError            **error)
{
  return dex_await_object (gom_repository_new (GOM_DRIVER (context->driver), registry, NULL), error);
}

static inline void
test_sqlite_open (const char *db_path,
                  sqlite3   **db)
{
  int rc;

  g_assert_nonnull (db_path);
  g_assert_nonnull (db);

  rc = sqlite3_open (db_path, db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (*db);
}

static inline void
test_sqlite_close (sqlite3 *db)
{
  int rc;

  g_assert_nonnull (db);

  rc = sqlite3_close (db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
}

static inline void
test_sqlite_exec_ok (sqlite3    *db,
                     const char *sql)
{
  char *errmsg = NULL;
  int rc;

  g_assert_nonnull (db);
  g_assert_nonnull (sql);

  rc = sqlite3_exec (db, sql, NULL, NULL, &errmsg);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_null (errmsg);
}

static inline gboolean
test_sqlite_relation_exists (sqlite3     *db,
                             const char  *name,
                             const char  *type)
{
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert_nonnull (db);
  g_assert_nonnull (name);
  g_assert_nonnull (type);

  rc = sqlite3_prepare_v2 (db,
                           "SELECT 1 FROM sqlite_master WHERE type = ?1 AND name = ?2",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_text (stmt, 1, type, -1, SQLITE_STATIC);
  sqlite3_bind_text (stmt, 2, name, -1, SQLITE_STATIC);
  rc = sqlite3_step (stmt);
  sqlite3_finalize (stmt);
  return rc == SQLITE_ROW;
}

static inline gboolean
test_sqlite_column_exists (sqlite3     *db,
                           const char  *table,
                           const char  *column)
{
  sqlite3_stmt *stmt = NULL;
  char *sql = NULL;
  int rc;

  g_assert_nonnull (db);
  g_assert_nonnull (table);
  g_assert_nonnull (column);

  sql = sqlite3_mprintf ("PRAGMA table_info(%Q)", table);
  rc = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  sqlite3_free (sql);
  g_assert_cmpint (rc, ==, SQLITE_OK);

  while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
      const char *name = (const char *) sqlite3_column_text (stmt, 1);
      if (g_strcmp0 (name, column) == 0)
        {
          sqlite3_finalize (stmt);
          return TRUE;
        }
    }

  g_assert_cmpint (rc, ==, SQLITE_DONE);
  sqlite3_finalize (stmt);
  return FALSE;
}

static inline guint
test_sqlite_read_user_version (sqlite3 *db)
{
  sqlite3_stmt *stmt = NULL;
  int rc;
  guint version = 0;

  g_assert_nonnull (db);

  rc = sqlite3_prepare_v2 (db, "PRAGMA user_version", -1, &stmt, NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  version = (guint) sqlite3_column_int (stmt, 0);
  sqlite3_finalize (stmt);
  return version;
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (TestSqliteContext, test_sqlite_context_clear)

#endif /* GOM_DATABASE_SQLITE */

G_END_DECLS
