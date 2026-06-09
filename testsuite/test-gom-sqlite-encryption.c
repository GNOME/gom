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
#include <sqlite3.h>
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

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  dex_init ();

  _g_test_add_func ("/Gom/SqliteEncryption/encrypted-database", test_cipher_encrypted_database);

  return g_test_run ();
}
