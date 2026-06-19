/* test-gom-sqlite-encryption.c
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
#include <sqlite3mc.h>
#include <string.h>

#include <libgom.h>

#include "test-util.h"

typedef struct _TestCipherItem      TestCipherItem;
typedef struct _TestCipherItemClass TestCipherItemClass;

struct _TestCipherItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
};

struct _TestCipherItemClass
{
  GomEntityClass parent_class;
};

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GType test_cipher_item_get_type (void);
G_DEFINE_TYPE (TestCipherItem, test_cipher_item, GOM_TYPE_ENTITY)

static void
test_cipher_item_finalize (GObject *object)
{
  TestCipherItem *self = (TestCipherItem *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (test_cipher_item_parent_class)->finalize (object);
}

static void
test_cipher_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestCipherItem *self = (TestCipherItem *)object;

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_cipher_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestCipherItem *self = (TestCipherItem *)object;

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_cipher_item_class_init (TestCipherItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_cipher_item_finalize;
  object_class->get_property = test_cipher_item_get_property;
  object_class->set_property = test_cipher_item_set_property;

  properties[PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gom_entity_class_set_relation (entity_class, "cipher_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "name", 1);
  gom_entity_class_property_set_nonnull (entity_class, "name", TRUE);
}

static void
test_cipher_item_init (TestCipherItem *self)
{
}

static GomRegistry *
test_cipher_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_cipher_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomDriverOptions *
test_cipher_create_options (const guint8 *key,
                            gsize         key_len)
{
  g_autoptr(GBytes) bytes = NULL;
  GomDriverOptions *options;

  bytes = g_bytes_new_static (key, key_len);
  options = gom_driver_options_new ();
  gom_driver_options_set_encryption_key (options, bytes);

  return options;
}

static GomRepository *
test_cipher_open_repository (const char    *uri,
                             const guint8  *key,
                             gsize          key_len,
                             GError       **error)
{
  g_autoptr(GomDriverOptions) options = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRegistry) registry = NULL;

  options = test_cipher_create_options (key, key_len);
  driver = gom_driver_open_with_options (uri, options, error);
  if (driver == NULL)
    return NULL;

  registry = test_cipher_create_registry ();
  return dex_await_object (gom_repository_new (driver, registry, NULL), error);
}

static GomDriver *
test_cipher_open_driver (const char    *uri,
                         const guint8  *key,
                         gsize          key_len,
                         GError       **error)
{
  g_autoptr(GomDriverOptions) options = NULL;

  if (key == NULL)
    return gom_driver_open (uri, error);

  options = test_cipher_create_options (key, key_len);
  return gom_driver_open_with_options (uri, options, error);
}

static GomRepository *
test_cipher_open_repository_no_key (const char  *uri,
                                    GError     **error)
{
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRegistry) registry = NULL;

  if (!(driver = gom_driver_open (uri, error)))
    return NULL;

  registry = test_cipher_create_registry ();
  return dex_await_object (gom_repository_new (driver, registry, NULL), error);
}

#define CIPHER_ITEM_COUNT         256
#define CIPHER_ITEM_PAYLOAD_BYTES 1024

static void
test_cipher_fill_payload (guint64  row_id,
                          guint8  *payload,
                          gsize    payload_len)
{
  for (gsize i = 0; i < payload_len; i++)
    payload[i] = (guint8) ((row_id * 31u) + (i * 17u) + 7u);
}

static gboolean
test_cipher_open_sqlite (const char    *db_path,
                         const guint8  *key,
                         gsize          key_len,
                         int            flags,
                         sqlite3      **out_db,
                         GError       **error)
{
  sqlite3 *db = NULL;
  int rc;

  g_return_val_if_fail (db_path != NULL, FALSE);
  g_return_val_if_fail (out_db != NULL, FALSE);

  *out_db = NULL;

  if (key_len > G_MAXINT)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Encryption key is too long");
      return FALSE;
    }

  rc = sqlite3_open_v2 (db_path, &db, flags, NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to open SQLite database");
      if (db != NULL)
        sqlite3_close_v2 (db);
      return FALSE;
    }

  if (key != NULL && key_len > 0)
    {
      if (sqlite3_key_v2 (db, "main", key, (int) key_len) != SQLITE_OK)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to configure SQLite encryption key");
          sqlite3_close_v2 (db);
          return FALSE;
        }
    }

  *out_db = db;
  return TRUE;
}

static gboolean
test_cipher_sqlite_exec (sqlite3     *db,
                         const char  *sql,
                         GError     **error)
{
  char *errmsg = NULL;
  int rc;

  rc = sqlite3_exec (db, sql, NULL, NULL, &errmsg);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s",
                   errmsg != NULL ? errmsg : "sqlite3_exec failed");
      sqlite3_free (errmsg);
      return FALSE;
    }

  sqlite3_free (errmsg);
  return TRUE;
}

static gboolean
test_cipher_verify_sqlite_read (const char    *db_path,
                                const guint8  *key,
                                gsize          key_len,
                                GError       **error)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  if (!test_cipher_open_sqlite (db_path, key, key_len, SQLITE_OPEN_READONLY, &db, error))
    return FALSE;

  rc = sqlite3_prepare_v2 (db,
                           "SELECT name FROM sqlite_master WHERE type='table'",
                           -1,
                           &stmt,
                           NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to validate SQLite database read: %s",
                   sqlite3_errmsg (db));
      sqlite3_close_v2 (db);
      return FALSE;
    }

  for (;;)
    {
      rc = sqlite3_step (stmt);
      if (rc == SQLITE_ROW)
        continue;

      if (rc != SQLITE_DONE)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to validate SQLite database read: %s",
                       sqlite3_errmsg (db));
          sqlite3_finalize (stmt);
          sqlite3_close_v2 (db);
          return FALSE;
        }

      break;
    }

  sqlite3_finalize (stmt);
  sqlite3_close_v2 (db);

  return rc == SQLITE_DONE;
}

static gboolean
test_cipher_is_encrypted (const char   *db_path,
                          const guint8 *new_key,
                          gsize         new_key_len,
                          const guint8 *wrong_key,
                          gsize         wrong_key_len)
{
  g_autoptr(GError) error = NULL;

  if (!test_cipher_verify_sqlite_read (db_path, new_key, new_key_len, &error))
    return FALSE;

  g_clear_error (&error);

  if (test_cipher_verify_sqlite_read (db_path, NULL, 0, &error))
    return FALSE;

  g_clear_error (&error);

  if (wrong_key != NULL && wrong_key_len > 0 &&
      test_cipher_verify_sqlite_read (db_path, wrong_key, wrong_key_len, &error))
    return FALSE;

  return TRUE;
}

static gboolean
test_cipher_write_fixture (const char    *db_path,
                           const guint8  *key,
                           gsize          key_len,
                           GError       **error)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  if (!test_cipher_open_sqlite (db_path,
                                key,
                                key_len,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                &db,
                                error))
    return FALSE;

  if (!test_cipher_sqlite_exec (db, "DROP TABLE IF EXISTS cipher_items", error))
    goto fail;

  if (!test_cipher_sqlite_exec (db,
                                "CREATE TABLE cipher_items ("
                                "id INTEGER PRIMARY KEY,"
                                "name TEXT NOT NULL,"
                                "payload BLOB NOT NULL)",
                                error))
    goto fail;

  if (!test_cipher_sqlite_exec (db,
                                "CREATE INDEX cipher_items_name_idx ON cipher_items(name)",
                                error))
    goto fail;

  rc = sqlite3_prepare_v2 (db,
                           "INSERT INTO cipher_items (id, name, payload) VALUES (?1, ?2, ?3)",
                           -1,
                           &stmt,
                           NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to prepare fixture insert statement: %s",
                   sqlite3_errmsg (db));
      goto fail;
    }

  for (guint64 i = 1; i <= CIPHER_ITEM_COUNT; i++)
    {
      g_autofree char *name = NULL;
      g_autofree guint8 *payload = NULL;

      name = g_strdup_printf ("item-%06" G_GUINT64_FORMAT, i);
      payload = g_malloc (CIPHER_ITEM_PAYLOAD_BYTES);
      test_cipher_fill_payload (i, payload, CIPHER_ITEM_PAYLOAD_BYTES);

      rc = sqlite3_bind_int64 (stmt, 1, (sqlite3_int64)i);
      if (rc != SQLITE_OK)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to bind fixture id: %s",
                       sqlite3_errmsg (db));
          goto fail;
        }

      rc = sqlite3_bind_text (stmt, 2, name, -1, SQLITE_TRANSIENT);
      if (rc != SQLITE_OK)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to bind fixture name: %s",
                       sqlite3_errmsg (db));
          goto fail;
        }

      rc = sqlite3_bind_blob (stmt, 3, payload, CIPHER_ITEM_PAYLOAD_BYTES, SQLITE_TRANSIENT);
      if (rc != SQLITE_OK)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to bind fixture payload: %s",
                       sqlite3_errmsg (db));
          goto fail;
        }

      rc = sqlite3_step (stmt);
      if (rc != SQLITE_DONE)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to insert fixture row %" G_GUINT64_FORMAT ": %s",
                       i,
                       sqlite3_errmsg (db));
          goto fail;
        }

      sqlite3_clear_bindings (stmt);
      sqlite3_reset (stmt);
    }

  sqlite3_finalize (stmt);
  sqlite3_close_v2 (db);
  return TRUE;

fail:
  if (stmt != NULL)
    sqlite3_finalize (stmt);
  if (db != NULL)
    sqlite3_close_v2 (db);
  return FALSE;
}

static char *
test_cipher_compute_digest (const char    *db_path,
                            const guint8  *key,
                            gsize          key_len,
                            GError       **error)
{
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;
  g_autoptr(GChecksum) checksum = NULL;
  char *digest;

  if (!test_cipher_open_sqlite (db_path, key, key_len, SQLITE_OPEN_READONLY, &db, error))
    return NULL;

  rc = sqlite3_prepare_v2 (db,
                           "SELECT id, name, payload FROM cipher_items ORDER BY id",
                           -1,
                           &stmt,
                           NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to query fixture rows: %s",
                   sqlite3_errmsg (db));
      sqlite3_close_v2 (db);
      return NULL;
    }

  checksum = g_checksum_new (G_CHECKSUM_SHA256);

  for (;;)
    {
      rc = sqlite3_step (stmt);
      if (rc == SQLITE_ROW)
        {
          guint64 id = (guint64) sqlite3_column_int64 (stmt, 0);
          const char *name = (const char *) sqlite3_column_text (stmt, 1);
          gsize name_len = name != NULL ? strlen (name) : 0;
          gconstpointer payload = sqlite3_column_blob (stmt, 2);
          gsize payload_len = sqlite3_column_bytes (stmt, 2);
          guint64 id_be = GUINT64_TO_BE (id);
          guint32 name_len_be = GUINT32_TO_BE ((guint32) name_len);
          guint32 payload_len_be = GUINT32_TO_BE ((guint32) payload_len);

          g_checksum_update (checksum, (guchar *) &id_be, sizeof id_be);
          g_checksum_update (checksum, (guchar *) &name_len_be, sizeof name_len_be);
          g_checksum_update (checksum, (guchar *)name, name_len);
          g_checksum_update (checksum, (guchar *) &payload_len_be, sizeof payload_len_be);
          if (payload_len > 0)
            g_checksum_update (checksum, payload, payload_len);
          continue;
        }

      if (rc != SQLITE_DONE)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Failed to query fixture rows: %s",
                       sqlite3_errmsg (db));
          sqlite3_finalize (stmt);
          sqlite3_close_v2 (db);
          return NULL;
        }

      break;
    }

  sqlite3_finalize (stmt);
  sqlite3_close_v2 (db);

  if (rc != SQLITE_DONE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to iterate fixture rows");
      return NULL;
    }

  digest = g_strdup (g_checksum_get_string (checksum));
  return g_steal_pointer (&digest);
}

static gboolean
test_cipher_insert_fixture_row (sqlite3     *db,
                                guint64      id,
                                const char  *name,
                                GError     **error)
{
  sqlite3_stmt *stmt = NULL;
  g_autofree guint8 *payload = NULL;
  int rc;

  payload = g_malloc (CIPHER_ITEM_PAYLOAD_BYTES);
  test_cipher_fill_payload (id, payload, CIPHER_ITEM_PAYLOAD_BYTES);

  rc = sqlite3_prepare_v2 (db,
                           "INSERT INTO cipher_items (id, name, payload) VALUES (?1, ?2, ?3)",
                           -1,
                           &stmt,
                           NULL);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to prepare post-rekey insert: %s",
                   sqlite3_errmsg (db));
      return FALSE;
    }

  rc = sqlite3_bind_int64 (stmt, 1, (sqlite3_int64)id);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to bind inserted id: %s",
                   sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      return FALSE;
    }

  rc = sqlite3_bind_text (stmt, 2, name, -1, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to bind inserted name: %s",
                   sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      return FALSE;
    }

  rc = sqlite3_bind_blob (stmt, 3, payload, CIPHER_ITEM_PAYLOAD_BYTES, SQLITE_TRANSIENT);
  if (rc != SQLITE_OK)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to bind inserted payload: %s",
                   sqlite3_errmsg (db));
      sqlite3_finalize (stmt);
      return FALSE;
    }

  rc = sqlite3_step (stmt);
  sqlite3_finalize (stmt);
  if (rc != SQLITE_DONE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Failed to insert row %" G_GUINT64_FORMAT ": %s",
                   id,
                   sqlite3_errmsg (db));
      return FALSE;
    }

  return TRUE;
}

static gboolean
test_cipher_apply_post_rekey_mutations (const char    *db_path,
                                        const guint8  *key,
                                        gsize          key_len,
                                        GError       **error)
{
  sqlite3 *db = NULL;

  if (!test_cipher_open_sqlite (db_path,
                               key,
                               key_len,
                               SQLITE_OPEN_READWRITE,
                               &db,
                               error))
    return FALSE;

  if (!test_cipher_insert_fixture_row (db, CIPHER_ITEM_COUNT + 1, "item-after-rekey", error))
    goto fail;

  if (!test_cipher_sqlite_exec (db,
                                "UPDATE cipher_items SET name = 'updated-name' WHERE id = 1",
                                error))
    goto fail;

  if (!test_cipher_sqlite_exec (db, "DELETE FROM cipher_items WHERE id = 2", error))
    goto fail;

  sqlite3_close_v2 (db);
  return TRUE;

fail:
  if (db != NULL)
    sqlite3_close_v2 (db);
  return FALSE;
}

static void
test_cipher_encrypted_database (void)
{
  static const guint8 key[] = {
    0x60, 0x7d, 0x75, 0x4e, 0x98, 0x5e, 0xe8, 0x68,
    0xf8, 0x2c, 0xed, 0x7d, 0x14, 0xea, 0xb4, 0x1b,
    0x3f, 0xdb, 0x12, 0x10, 0x6c, 0x31, 0xf4, 0xab,
    0x2d, 0x44, 0x79, 0xd9, 0x17, 0x93, 0x55, 0x72,
  };
  static const guint8 wrong_key[] = {
    0x72, 0x55, 0x93, 0x17, 0xd9, 0x79, 0x44, 0x2d,
    0xab, 0xf4, 0x31, 0x6c, 0x10, 0x12, 0xdb, 0x3f,
    0x1b, 0xb4, 0xea, 0x14, 0x7d, 0xed, 0x2c, 0xf8,
    0x68, 0xe8, 0x5e, 0x98, 0x4e, 0x75, 0x7d, 0x60,
  };
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRepository) reopened = NULL;
  g_autofree char *tmpdir = NULL;
  g_autofree char *db_path = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char *contents = NULL;
  gsize contents_len = 0;
  sqlite3 *db = NULL;
  char *errmsg = NULL;
  int rc;

  tmpdir = g_dir_make_tmp ("gom-sqlite-encryption-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (tmpdir);

  db_path = g_build_filename (tmpdir, "test.db", NULL);
  db_uri = g_filename_to_uri (db_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (db_uri);

  repository = test_cipher_open_repository (db_uri, key, sizeof key, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  g_clear_object (&repository);

  g_file_get_contents (db_path, &contents, &contents_len, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (contents_len, >, 16);
  g_assert_cmpint (memcmp (contents, "SQLite format 3", 15), !=, 0);

  rc = sqlite3_open (db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_exec (db, "SELECT count(*) FROM sqlite_master", NULL, NULL, &errmsg);
  g_assert_cmpint (rc, !=, SQLITE_OK);
  g_clear_pointer (&errmsg, sqlite3_free);
  sqlite3_close (db);
  db = NULL;

  reopened = test_cipher_open_repository (db_uri, key, sizeof key, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (reopened));
  g_clear_object (&reopened);

  reopened = test_cipher_open_repository (db_uri, wrong_key, sizeof wrong_key, &error);
  g_assert_nonnull (error);
  g_assert_true (g_error_matches (error, GOM_ERROR, GOM_ERROR_INVALID_ENCRYPTION_KEY));
  g_assert_null (reopened);
  g_clear_error (&error);

  test_remove_directory_recursive (tmpdir);
}

static void
test_cipher_rekey_basic (void)
{
  static const guint8 key_a[] = {
    0x60, 0x7d, 0x75, 0x4e, 0x98, 0x5e, 0xe8, 0x68,
    0xf8, 0x2c, 0xed, 0x7d, 0x14, 0xea, 0xb4, 0x1b,
    0x3f, 0xdb, 0x12, 0x10, 0x6c, 0x31, 0xf4, 0xab,
    0x2d, 0x44, 0x79, 0xd9, 0x17, 0x93, 0x55, 0x72,
  };
  static const guint8 key_b[] = {
    0x72, 0x55, 0x93, 0x17, 0xd9, 0x79, 0x44, 0x2d,
    0xab, 0xf4, 0x31, 0x6c, 0x10, 0x12, 0xdb, 0x3f,
    0x1b, 0xb4, 0xea, 0x14, 0x7d, 0xed, 0x2c, 0xf8,
    0x68, 0xe8, 0x5e, 0x98, 0x4e, 0x75, 0x7d, 0x60,
  };
  static const guint8 wrong_key[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
    0x70, 0x7f, 0x8e, 0x9d, 0x6c, 0x5b, 0x4a, 0x3c,
    0x2d, 0x1e, 0x0f, 0xe1, 0x2f, 0x3e, 0x4d, 0x5c,
  };
  g_autoptr(GError) error = NULL;
  g_autofree char *tmpdir = NULL;
  g_autofree char *db_path = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char *digest_before = NULL;
  g_autofree char *digest_after = NULL;
  g_autofree char *digest_updated = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRepository) reopen = NULL;
  g_autoptr(GomDriverOptions) options = NULL;
  gboolean rekey_ok;

  tmpdir = g_dir_make_tmp ("gom-sqlite-rekey-basic-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (tmpdir);

  db_path = g_build_filename (tmpdir, "test.db", NULL);
  db_uri = g_filename_to_uri (db_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (db_uri);

  g_assert_true (test_cipher_write_fixture (db_path, key_a, sizeof key_a, &error));
  g_assert_no_error (error);

  if (!test_cipher_is_encrypted (db_path, key_a, sizeof key_a, wrong_key, sizeof wrong_key))
    {
      g_test_skip ("SQLite encryption is not available on this runtime");
      test_remove_directory_recursive (tmpdir);
      return;
    }

  g_assert_true (test_cipher_verify_sqlite_read (db_path, key_a, sizeof key_a, &error));
  g_assert_no_error (error);

  digest_before = test_cipher_compute_digest (db_path, key_a, sizeof key_a, &error);
  g_assert_no_error (error);
  g_assert_nonnull (digest_before);

  driver = test_cipher_open_driver (db_uri, key_a, sizeof key_a, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_DRIVER (driver));

  options = test_cipher_create_options (key_b, sizeof key_b);
  rekey_ok = dex_await_boolean (gom_driver_rekey (driver, options), &error);
  g_assert_no_error (error);
  g_assert_true (rekey_ok);

  digest_after = test_cipher_compute_digest (db_path, key_b, sizeof key_b, &error);
  g_assert_no_error (error);
  g_assert_nonnull (digest_after);
  g_assert_cmpstr (digest_before, ==, digest_after);

  reopen = test_cipher_open_repository (db_uri, key_b, sizeof key_b, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (reopen));
  g_clear_object (&reopen);

  reopen = test_cipher_open_repository (db_uri, key_a, sizeof key_a, &error);
  g_assert_null (reopen);
  g_assert_nonnull (error);
  g_assert_true (g_error_matches (error, GOM_ERROR, GOM_ERROR_INVALID_ENCRYPTION_KEY));
  g_clear_error (&error);

  g_assert_false (test_cipher_verify_sqlite_read (db_path, NULL, 0, &error));
  g_clear_error (&error);

  g_assert_false (test_cipher_verify_sqlite_read (db_path, wrong_key, sizeof wrong_key, &error));
  g_clear_error (&error);

  g_assert_true (test_cipher_apply_post_rekey_mutations (db_path, key_b, sizeof key_b, &error));
  g_assert_no_error (error);

  digest_updated = test_cipher_compute_digest (db_path, key_b, sizeof key_b, &error);
  g_assert_no_error (error);
  g_assert_nonnull (digest_updated);
  g_assert_cmpstr (digest_after, !=, digest_updated);

  reopen = test_cipher_open_repository (db_uri, key_b, sizeof key_b, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (reopen));
  g_clear_object (&reopen);

  g_assert_cmpstr (test_cipher_compute_digest (db_path, key_b, sizeof key_b, &error), ==, digest_updated);
  g_assert_no_error (error);

  test_remove_directory_recursive (tmpdir);
}

static void
test_cipher_rekey_reject_explicit_transaction (void)
{
  static const guint8 key_a[] = {
    0x60, 0x7d, 0x75, 0x4e, 0x98, 0x5e, 0xe8, 0x68,
    0xf8, 0x2c, 0xed, 0x7d, 0x14, 0xea, 0xb4, 0x1b,
    0x3f, 0xdb, 0x12, 0x10, 0x6c, 0x31, 0xf4, 0xab,
    0x2d, 0x44, 0x79, 0xd9, 0x17, 0x93, 0x55, 0x72,
  };
  static const guint8 key_b[] = {
    0x72, 0x55, 0x93, 0x17, 0xd9, 0x79, 0x44, 0x2d,
    0xab, 0xf4, 0x31, 0x6c, 0x10, 0x12, 0xdb, 0x3f,
    0x1b, 0xb4, 0xea, 0x14, 0x7d, 0xed, 0x2c, 0xf8,
    0x68, 0xe8, 0x5e, 0x98, 0x4e, 0x75, 0x7d, 0x60,
  };
  g_autoptr(GError) error = NULL;
  g_autofree char *tmpdir = NULL;
  g_autofree char *db_path = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char *digest_before = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomDriverOptions) options = NULL;
  sqlite3 *locker_db = NULL;
  gboolean rekey_ok;
  g_autoptr(GomRepository) reopen = NULL;

  tmpdir = g_dir_make_tmp ("gom-sqlite-rekey-explicit-tx-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (tmpdir);

  db_path = g_build_filename (tmpdir, "test.db", NULL);
  db_uri = g_filename_to_uri (db_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (db_uri);

  g_assert_true (test_cipher_write_fixture (db_path, key_a, sizeof key_a, &error));
  g_assert_no_error (error);

  if (!test_cipher_is_encrypted (db_path, key_a, sizeof key_a, key_b, sizeof key_b))
    {
      g_test_skip ("SQLite encryption is not available on this runtime");
      test_remove_directory_recursive (tmpdir);
      return;
    }

  digest_before = test_cipher_compute_digest (db_path, key_a, sizeof key_a, &error);
  g_assert_no_error (error);

  repository = test_cipher_open_repository (db_uri, key_a, sizeof key_a, &error);
  g_assert_no_error (error);
  driver = gom_repository_dup_driver (repository);

  g_assert_true (test_cipher_open_sqlite (db_path,
                                          key_a,
                                          sizeof key_a,
                                          SQLITE_OPEN_READWRITE,
                                          &locker_db,
                                          &error));
  g_assert_no_error (error);
  g_assert_true (test_cipher_sqlite_exec (locker_db, "BEGIN EXCLUSIVE", &error));

  options = test_cipher_create_options (key_b, sizeof key_b);
  rekey_ok = dex_await_boolean (gom_driver_rekey (driver, options), &error);
  g_assert_false (rekey_ok);
  g_assert_true (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_BUSY));
  g_assert_nonnull (error);

  g_clear_error (&error);
  g_assert_true (test_cipher_sqlite_exec (locker_db, "ROLLBACK", &error));
  sqlite3_close_v2 (locker_db);
  locker_db = NULL;

  options = NULL;
  reopen = test_cipher_open_repository (db_uri, key_a, sizeof key_a, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (test_cipher_compute_digest (db_path, key_a, sizeof key_a, &error), ==, digest_before);
  g_assert_no_error (error);
  g_clear_object (&reopen);
  g_clear_object (&repository);

  test_remove_directory_recursive (tmpdir);
}

static void
test_cipher_rekey_reject_implicit_read (void)
{
  static const guint8 key_a[] = {
    0x60, 0x7d, 0x75, 0x4e, 0x98, 0x5e, 0xe8, 0x68,
    0xf8, 0x2c, 0xed, 0x7d, 0x14, 0xea, 0xb4, 0x1b,
    0x3f, 0xdb, 0x12, 0x10, 0x6c, 0x31, 0xf4, 0xab,
    0x2d, 0x44, 0x79, 0xd9, 0x17, 0x93, 0x55, 0x72,
  };
  static const guint8 key_b[] = {
    0x72, 0x55, 0x93, 0x17, 0xd9, 0x79, 0x44, 0x2d,
    0xab, 0xf4, 0x31, 0x6c, 0x10, 0x12, 0xdb, 0x3f,
    0x1b, 0xb4, 0xea, 0x14, 0x7d, 0xed, 0x2c, 0xf8,
    0x68, 0xe8, 0x5e, 0x98, 0x4e, 0x75, 0x7d, 0x60,
  };
  g_autoptr(GError) error = NULL;
  g_autofree char *tmpdir = NULL;
  g_autofree char *db_path = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char *digest_before = NULL;
  g_autofree char *digest_after = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomDriverOptions) options = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  gboolean has_row;
  gboolean rekey_ok;

  tmpdir = g_dir_make_tmp ("gom-sqlite-rekey-implicit-read-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (tmpdir);

  db_path = g_build_filename (tmpdir, "test.db", NULL);
  db_uri = g_filename_to_uri (db_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (db_uri);

  g_assert_true (test_cipher_write_fixture (db_path, key_a, sizeof key_a, &error));
  g_assert_no_error (error);

  if (!test_cipher_is_encrypted (db_path, key_a, sizeof key_a, key_b, sizeof key_b))
    {
      g_test_skip ("SQLite encryption is not available on this runtime");
      test_remove_directory_recursive (tmpdir);
      return;
    }

  digest_before = test_cipher_compute_digest (db_path, key_a, sizeof key_a, &error);
  g_assert_no_error (error);
  g_assert_nonnull (digest_before);

  repository = test_cipher_open_repository (db_uri, key_a, sizeof key_a, &error);
  g_assert_no_error (error);
  driver = gom_repository_dup_driver (repository);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, "cipher_items");
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  has_row = dex_await_boolean (gom_cursor_next (cursor), &error);
  g_assert_no_error (error);
  g_assert_true (has_row);

  options = test_cipher_create_options (key_b, sizeof key_b);
  rekey_ok = dex_await_boolean (gom_driver_rekey (driver, options), &error);
  g_assert_false (rekey_ok);
  g_assert_true (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_BUSY));
  g_assert_nonnull (error);
  g_clear_error (&error);

  g_assert_true (dex_await_boolean (gom_cursor_close (cursor), &error));
  g_clear_object (&cursor);
  g_clear_object (&query);
  g_clear_pointer (&query_builder, gom_query_builder_unref);
  g_clear_error (&error);
  g_clear_object (&repository);
  g_assert_no_error (error);

  options = test_cipher_create_options (key_b, sizeof key_b);
  rekey_ok = dex_await_boolean (gom_driver_rekey (driver, options), &error);
  g_assert_no_error (error);
  g_assert_true (rekey_ok);

  digest_after = test_cipher_compute_digest (db_path, key_b, sizeof key_b, &error);
  g_assert_no_error (error);
  g_assert_nonnull (digest_after);
  g_assert_cmpstr (digest_before, ==, digest_after);

  test_remove_directory_recursive (tmpdir);
}

static void
test_cipher_rekey_special_passphrases (void)
{
  static const gchar *key_a_text = " old key ' \" ; -- café ☕";
  static const gchar *key_b_text = " new key with whitespace\tquotes ' \" and unicode 雪";
  static const guint8 wrong_key[] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
  };
  g_autoptr(GBytes) key_a = NULL;
  g_autoptr(GBytes) key_b = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *tmpdir = NULL;
  g_autofree char *db_path = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char *digest_before = NULL;
  g_autofree char *digest_after = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRepository) reopened = NULL;
  g_autoptr(GomDriverOptions) options = NULL;
  gboolean rekey_ok;

  tmpdir = g_dir_make_tmp ("gom-sqlite-rekey-phrases-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (tmpdir);

  db_path = g_build_filename (tmpdir, "test.db", NULL);
  db_uri = g_filename_to_uri (db_path, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (db_uri);

  key_a = g_bytes_new (key_a_text, strlen (key_a_text));
  g_assert_true (test_cipher_write_fixture (db_path,
                                            g_bytes_get_data (key_a, NULL),
                                            g_bytes_get_size (key_a),
                                            &error));
  g_assert_no_error (error);

  if (!test_cipher_is_encrypted (db_path,
                                 g_bytes_get_data (key_a, NULL),
                                 g_bytes_get_size (key_a),
                                 wrong_key,
                                 sizeof wrong_key))
    {
      g_test_skip ("SQLite encryption is not available on this runtime");
      test_remove_directory_recursive (tmpdir);
      return;
    }

  driver = test_cipher_open_driver (db_uri,
                                    g_bytes_get_data (key_a, NULL),
                                    g_bytes_get_size (key_a),
                                    &error);
  g_assert_no_error (error);

  digest_before = test_cipher_compute_digest (db_path,
                                              g_bytes_get_data (key_a, NULL),
                                              g_bytes_get_size (key_a),
                                              &error);
  g_assert_no_error (error);

  key_b = g_bytes_new (key_b_text, strlen (key_b_text));
  options = test_cipher_create_options (g_bytes_get_data (key_b, NULL), g_bytes_get_size (key_b));
  rekey_ok = dex_await_boolean (gom_driver_rekey (driver, options), &error);
  g_assert_no_error (error);
  g_assert_true (rekey_ok);

  g_clear_error (&error);
  reopened = test_cipher_open_repository_no_key (db_uri, &error);
  g_assert_null (reopened);
  g_assert_nonnull (error);
  g_clear_error (&error);

  digest_after = test_cipher_compute_digest (db_path,
                                             g_bytes_get_data (key_b, NULL),
                                             g_bytes_get_size (key_b),
                                             &error);
  g_assert_no_error (error);
  g_assert_nonnull (digest_after);
  g_assert_cmpstr (digest_before, ==, digest_after);

  reopened = test_cipher_open_repository (db_uri,
                                          g_bytes_get_data (key_a, NULL),
                                          g_bytes_get_size (key_a),
                                          &error);
  g_assert_null (reopened);
  g_assert_nonnull (error);
  g_assert_true (g_error_matches (error, GOM_ERROR, GOM_ERROR_INVALID_ENCRYPTION_KEY));
  g_clear_error (&error);

  g_assert_false (test_cipher_verify_sqlite_read (db_path, NULL, 0, &error));
  g_clear_error (&error);

  g_assert_false (test_cipher_verify_sqlite_read (db_path, wrong_key, sizeof wrong_key, &error));
  g_clear_error (&error);

  test_remove_directory_recursive (tmpdir);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  dex_init ();

  _g_test_add_func ("/Gom/SqliteEncryption/encrypted-database", test_cipher_encrypted_database);
  _g_test_add_func ("/Gom/Sqlite/rekey/basic", test_cipher_rekey_basic);
  _g_test_add_func ("/Gom/Sqlite/rekey/reject-explicit-transaction", test_cipher_rekey_reject_explicit_transaction);
  _g_test_add_func ("/Gom/Sqlite/rekey/reject-implicit-read", test_cipher_rekey_reject_implicit_read);
  _g_test_add_func ("/Gom/Sqlite/rekey/special-passphrases", test_cipher_rekey_special_passphrases);

  return g_test_run ();
}
