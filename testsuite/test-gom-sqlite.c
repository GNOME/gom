/* test-gom-sqlite.c
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

#include "lib/gom-util-private.h"
#include "test-util.h"

typedef struct _TestInsertItem                    TestInsertItem;
typedef struct _TestInsertItemClass               TestInsertItemClass;
typedef struct _TestHyphenItem                    TestHyphenItem;
typedef struct _TestHyphenItemClass               TestHyphenItemClass;
typedef struct _TestGTypeItem                     TestGTypeItem;
typedef struct _TestGTypeItemClass                TestGTypeItemClass;
typedef struct _TestEnumItem                      TestEnumItem;
typedef struct _TestEnumItemClass                 TestEnumItemClass;
typedef struct _TestDateTimeItem                  TestDateTimeItem;
typedef struct _TestDateTimeItemClass             TestDateTimeItemClass;
typedef struct _TestMaterializeItem               TestMaterializeItem;
typedef struct _TestMaterializeItemClass          TestMaterializeItemClass;
typedef struct _TestCrudItem                      TestCrudItem;
typedef struct _TestCrudItemClass                 TestCrudItemClass;
typedef struct _TestCrudAllowDefaultIdItem        TestCrudAllowDefaultIdItem;
typedef struct _TestCrudAllowDefaultIdItemClass   TestCrudAllowDefaultIdItemClass;
typedef struct _TestNoIdentityItem                TestNoIdentityItem;
typedef struct _TestNoIdentityItemClass           TestNoIdentityItemClass;
typedef struct _TestMigrationItem                 TestMigrationItem;
typedef struct _TestMigrationItemClass            TestMigrationItemClass;
typedef struct _TestUnsupportedTransformItem      TestUnsupportedTransformItem;
typedef struct _TestUnsupportedTransformItemClass TestUnsupportedTransformItemClass;
typedef struct _TestInvalidMigrationItem          TestInvalidMigrationItem;
typedef struct _TestInvalidMigrationItemClass     TestInvalidMigrationItemClass;
typedef struct _TestStrvItem                      TestStrvItem;
typedef struct _TestStrvItemClass                 TestStrvItemClass;

struct _TestInsertItem
{
  GomEntity  parent_instance;
  char      *name;
  char      *category;
};

struct _TestInsertItemClass
{
  GomEntityClass parent_class;
};

struct _TestHyphenItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *format_type;
};

struct _TestHyphenItemClass
{
  GomEntityClass parent_class;
};

struct _TestGTypeItem
{
  GomEntity parent_instance;
  gint64    id;
  GType     format_type;
};

struct _TestGTypeItemClass
{
  GomEntityClass parent_class;
};

typedef enum
{
  TEST_MATERIALIZE_MODE_ALPHA = 1,
  TEST_MATERIALIZE_MODE_BETA  = 2,
} TestMaterializeMode;

GType test_materialize_mode_get_type (void) G_GNUC_CONST;

struct _TestEnumItem
{
  GomEntity           parent_instance;
  gint64              id;
  TestMaterializeMode mode;
};

struct _TestEnumItemClass
{
  GomEntityClass parent_class;
};

struct _TestDateTimeItem
{
  GomEntity  parent_instance;
  gint64     id;
  GDateTime *stamp;
};

struct _TestDateTimeItemClass
{
  GomEntityClass parent_class;
};

struct _TestMaterializeItem
{
  GomEntity            parent_instance;
  char                *name;
  char                *payload;
  GDateTime           *when;
  char                *internal;
  gint                 count;
  gint64               big_count;
  guint                ucount;
  gboolean             flag;
  double               ratio;
  float                fvalue;
  TestMaterializeMode  mode;
};

struct _TestMaterializeItemClass
{
  GomEntityClass parent_class;
};

struct _TestCrudItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
  char      *payload;
};

struct _TestCrudItemClass
{
  GomEntityClass parent_class;
};

struct _TestCrudAllowDefaultIdItem
{
  TestCrudItem parent_instance;
};

struct _TestCrudAllowDefaultIdItemClass
{
  TestCrudItemClass parent_class;
};

struct _TestNoIdentityItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
};

struct _TestNoIdentityItemClass
{
  GomEntityClass parent_class;
};

struct _TestMigrationItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
  char      *legacy;
  char      *tag;
};

struct _TestMigrationItemClass
{
  GomEntityClass parent_class;
};

struct _TestUnsupportedTransformItem
{
  GomEntity  parent_instance;
  gint64     id;
  GVariant  *opaque;
};

struct _TestUnsupportedTransformItemClass
{
  GomEntityClass parent_class;
};

struct _TestInvalidMigrationItem
{
  GomEntity  parent_instance;
  gint64     id;
  char      *name;
  char      *critical;
};

struct _TestInvalidMigrationItemClass
{
  GomEntityClass parent_class;
};

struct _TestStrvItem
{
  GomEntity   parent_instance;
  gint64      id;
  char      **tags;
};

struct _TestStrvItemClass
{
  GomEntityClass parent_class;
};

G_DEFINE_ENUM_TYPE (TestMaterializeMode, test_materialize_mode,
                    G_DEFINE_ENUM_VALUE (TEST_MATERIALIZE_MODE_ALPHA, "alpha"),
                    G_DEFINE_ENUM_VALUE (TEST_MATERIALIZE_MODE_BETA, "beta"))

GType test_materialize_item_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (TestMaterializeItem, test_materialize_item, GOM_TYPE_ENTITY)

enum {
  TEST_MATERIALIZE_ITEM_PROP_0,
  TEST_MATERIALIZE_ITEM_PROP_NAME,
  TEST_MATERIALIZE_ITEM_PROP_PAYLOAD,
  TEST_MATERIALIZE_ITEM_PROP_WHEN,
  TEST_MATERIALIZE_ITEM_PROP_INTERNAL,
  TEST_MATERIALIZE_ITEM_PROP_COUNT,
  TEST_MATERIALIZE_ITEM_PROP_BIG_COUNT,
  TEST_MATERIALIZE_ITEM_PROP_UCOUNT,
  TEST_MATERIALIZE_ITEM_PROP_FLAG,
  TEST_MATERIALIZE_ITEM_PROP_RATIO,
  TEST_MATERIALIZE_ITEM_PROP_FVALUE,
  TEST_MATERIALIZE_ITEM_PROP_MODE,
  TEST_MATERIALIZE_ITEM_N_PROPS
};

static GParamSpec *test_materialize_item_properties[TEST_MATERIALIZE_ITEM_N_PROPS];

enum {
  TEST_INSERT_ITEM_PROP_0,
  TEST_INSERT_ITEM_PROP_NAME,
  TEST_INSERT_ITEM_PROP_CATEGORY,
  TEST_INSERT_ITEM_N_PROPS
};

static GParamSpec *test_insert_item_properties[TEST_INSERT_ITEM_N_PROPS];

enum {
  TEST_HYPHEN_ITEM_PROP_0,
  TEST_HYPHEN_ITEM_PROP_ID,
  TEST_HYPHEN_ITEM_PROP_FORMAT_TYPE,
  TEST_HYPHEN_ITEM_N_PROPS
};

static GParamSpec *test_hyphen_item_properties[TEST_HYPHEN_ITEM_N_PROPS];

enum {
  TEST_GTYPE_ITEM_PROP_0,
  TEST_GTYPE_ITEM_PROP_ID,
  TEST_GTYPE_ITEM_PROP_FORMAT_TYPE,
  TEST_GTYPE_ITEM_N_PROPS
};

static GParamSpec *test_gtype_item_properties[TEST_GTYPE_ITEM_N_PROPS];

enum {
  TEST_ENUM_ITEM_PROP_0,
  TEST_ENUM_ITEM_PROP_ID,
  TEST_ENUM_ITEM_PROP_MODE,
  TEST_ENUM_ITEM_N_PROPS
};

static GParamSpec *test_enum_item_properties[TEST_ENUM_ITEM_N_PROPS];

enum {
  TEST_DATETIME_ITEM_PROP_0,
  TEST_DATETIME_ITEM_PROP_ID,
  TEST_DATETIME_ITEM_PROP_STAMP,
  TEST_DATETIME_ITEM_N_PROPS
};

static GParamSpec *test_datetime_item_properties[TEST_DATETIME_ITEM_N_PROPS];

enum {
  TEST_CRUD_ITEM_PROP_0,
  TEST_CRUD_ITEM_PROP_ID,
  TEST_CRUD_ITEM_PROP_NAME,
  TEST_CRUD_ITEM_PROP_PAYLOAD,
  TEST_CRUD_ITEM_N_PROPS
};

enum {
  TEST_NO_IDENTITY_ITEM_PROP_0,
  TEST_NO_IDENTITY_ITEM_PROP_ID,
  TEST_NO_IDENTITY_ITEM_PROP_NAME,
  TEST_NO_IDENTITY_ITEM_N_PROPS
};

static GParamSpec *test_crud_item_properties[TEST_CRUD_ITEM_N_PROPS];
static GParamSpec *test_no_identity_item_properties[TEST_NO_IDENTITY_ITEM_N_PROPS];

enum {
  TEST_MIGRATION_ITEM_PROP_0,
  TEST_MIGRATION_ITEM_PROP_ID,
  TEST_MIGRATION_ITEM_PROP_NAME,
  TEST_MIGRATION_ITEM_PROP_LEGACY,
  TEST_MIGRATION_ITEM_PROP_TAG,
  TEST_MIGRATION_ITEM_N_PROPS
};

static GParamSpec *test_migration_item_properties[TEST_MIGRATION_ITEM_N_PROPS];

enum {
  TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_0,
  TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_ID,
  TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_OPAQUE,
  TEST_UNSUPPORTED_TRANSFORM_ITEM_N_PROPS
};

static GParamSpec *test_unsupported_transform_item_properties[TEST_UNSUPPORTED_TRANSFORM_ITEM_N_PROPS];

enum {
  TEST_INVALID_MIGRATION_ITEM_PROP_0,
  TEST_INVALID_MIGRATION_ITEM_PROP_ID,
  TEST_INVALID_MIGRATION_ITEM_PROP_NAME,
  TEST_INVALID_MIGRATION_ITEM_PROP_CRITICAL,
  TEST_INVALID_MIGRATION_ITEM_N_PROPS
};

static GParamSpec *test_invalid_migration_item_properties[TEST_INVALID_MIGRATION_ITEM_N_PROPS];

enum {
  TEST_STRV_ITEM_PROP_0,
  TEST_STRV_ITEM_PROP_ID,
  TEST_STRV_ITEM_PROP_TAGS,
  TEST_STRV_ITEM_N_PROPS
};

static GParamSpec *test_strv_item_properties[TEST_STRV_ITEM_N_PROPS];

GType test_insert_item_get_type                (void) G_GNUC_CONST;
GType test_hyphen_item_get_type                (void) G_GNUC_CONST;
GType test_gtype_item_get_type                 (void) G_GNUC_CONST;
GType test_enum_item_get_type                  (void) G_GNUC_CONST;
GType test_datetime_item_get_type              (void) G_GNUC_CONST;
GType test_crud_item_get_type                  (void) G_GNUC_CONST;
GType test_crud_allow_default_id_item_get_type (void) G_GNUC_CONST;
GType test_no_identity_item_get_type           (void) G_GNUC_CONST;
GType test_migration_item_get_type             (void) G_GNUC_CONST;
GType test_unsupported_transform_item_get_type (void) G_GNUC_CONST;
GType test_invalid_migration_item_get_type     (void) G_GNUC_CONST;
GType test_strv_item_get_type                  (void) G_GNUC_CONST;

G_DEFINE_TYPE (TestInsertItem, test_insert_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestHyphenItem, test_hyphen_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestGTypeItem, test_gtype_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestEnumItem, test_enum_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestDateTimeItem, test_datetime_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestCrudItem, test_crud_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestCrudAllowDefaultIdItem, test_crud_allow_default_id_item, test_crud_item_get_type ())
G_DEFINE_TYPE (TestNoIdentityItem, test_no_identity_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestMigrationItem, test_migration_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestUnsupportedTransformItem, test_unsupported_transform_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestInvalidMigrationItem, test_invalid_migration_item, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestStrvItem, test_strv_item, GOM_TYPE_ENTITY)

static GBytes   *test_materialize_item_to_bytes   (const GValue  *value,
                                                   gpointer       user_data,
                                                   GError       **error);
static gboolean  test_materialize_item_from_bytes (GBytes        *bytes,
                                                   GValue        *value,
                                                   gpointer       user_data,
                                                   GError       **error);

static GomRegistry *
test_sqlite_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_hyphen_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_gtype_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_enum_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_datetime_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_insert_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_materialize_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_crud_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_no_identity_item_get_type ());
  gom_registry_builder_add_entity_type (builder, test_strv_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_sqlite_create_allow_default_identity_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_crud_allow_default_id_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_sqlite_create_migration_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_migration_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_sqlite_create_unsupported_transform_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_unsupported_transform_item_get_type ());

  return gom_registry_builder_build (builder);
}

static GomRegistry *
test_sqlite_create_invalid_migration_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, test_invalid_migration_item_get_type ());

  return gom_registry_builder_build (builder);
}

static void
test_insert_item_finalize (GObject *object)
{
  TestInsertItem *self = (TestInsertItem *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->category, g_free);

  G_OBJECT_CLASS (test_insert_item_parent_class)->finalize (object);
}

static void
test_insert_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestInsertItem *self = (TestInsertItem *)object;

  switch (prop_id)
    {
    case TEST_INSERT_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_INSERT_ITEM_PROP_CATEGORY:
      g_value_set_string (value, self->category);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_insert_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestInsertItem *self = (TestInsertItem *)object;

  switch (prop_id)
    {
    case TEST_INSERT_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_INSERT_ITEM_PROP_CATEGORY:
      g_set_str (&self->category, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_insert_item_class_init (TestInsertItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_insert_item_finalize;
  object_class->set_property = test_insert_item_set_property;
  object_class->get_property = test_insert_item_get_property;

  test_insert_item_properties[TEST_INSERT_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_insert_item_properties[TEST_INSERT_ITEM_PROP_CATEGORY] =
    g_param_spec_string ("category", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_INSERT_ITEM_N_PROPS,
                                     test_insert_item_properties);

  gom_entity_class_set_relation (entity_class, "items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_search_flags (entity_class, "name", GOM_SEARCH_INDEXED);
}

static void
test_insert_item_init (TestInsertItem *self)
{
}

static void
test_hyphen_item_finalize (GObject *object)
{
  TestHyphenItem *self = (TestHyphenItem *)object;

  g_clear_pointer (&self->format_type, g_free);

  G_OBJECT_CLASS (test_hyphen_item_parent_class)->finalize (object);
}

static void
test_hyphen_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestHyphenItem *self = (TestHyphenItem *)object;

  switch (prop_id)
    {
    case TEST_HYPHEN_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_HYPHEN_ITEM_PROP_FORMAT_TYPE:
      g_value_set_string (value, self->format_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_hyphen_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestHyphenItem *self = (TestHyphenItem *)object;

  switch (prop_id)
    {
    case TEST_HYPHEN_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_HYPHEN_ITEM_PROP_FORMAT_TYPE:
      g_set_str (&self->format_type, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_hyphen_item_class_init (TestHyphenItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_hyphen_item_finalize;
  object_class->set_property = test_hyphen_item_set_property;
  object_class->get_property = test_hyphen_item_get_property;

  test_hyphen_item_properties[TEST_HYPHEN_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_hyphen_item_properties[TEST_HYPHEN_ITEM_PROP_FORMAT_TYPE] =
    g_param_spec_string ("format-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_HYPHEN_ITEM_N_PROPS,
                                     test_hyphen_item_properties);

  gom_entity_class_set_relation (entity_class, "hyphen_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "format-type", TRUE);
}

static void
test_hyphen_item_init (TestHyphenItem *self)
{
}

static void
test_gtype_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (test_gtype_item_parent_class)->finalize (object);
}

static void
test_gtype_item_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  TestGTypeItem *self = (TestGTypeItem *)object;

  switch (prop_id)
    {
    case TEST_GTYPE_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_GTYPE_ITEM_PROP_FORMAT_TYPE:
      g_value_set_gtype (value, self->format_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_gtype_item_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  TestGTypeItem *self = (TestGTypeItem *)object;

  switch (prop_id)
    {
    case TEST_GTYPE_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_GTYPE_ITEM_PROP_FORMAT_TYPE:
      self->format_type = g_value_get_gtype (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_gtype_item_class_init (TestGTypeItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_gtype_item_finalize;
  object_class->set_property = test_gtype_item_set_property;
  object_class->get_property = test_gtype_item_get_property;

  test_gtype_item_properties[TEST_GTYPE_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_gtype_item_properties[TEST_GTYPE_ITEM_PROP_FORMAT_TYPE] =
    g_param_spec_gtype ("format-type", NULL, NULL,
                        G_TYPE_NONE,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_GTYPE_ITEM_N_PROPS,
                                     test_gtype_item_properties);

  gom_entity_class_set_relation (entity_class, "gtype_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "format-type", TRUE);
}

static void
test_gtype_item_init (TestGTypeItem *self)
{
  self->format_type = G_TYPE_NONE;
}

static void
test_enum_item_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  TestEnumItem *self = (TestEnumItem *)object;

  switch (prop_id)
    {
    case TEST_ENUM_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_ENUM_ITEM_PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_enum_item_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  TestEnumItem *self = (TestEnumItem *)object;

  switch (prop_id)
    {
    case TEST_ENUM_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_ENUM_ITEM_PROP_MODE:
      self->mode = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_enum_item_class_init (TestEnumItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->set_property = test_enum_item_set_property;
  object_class->get_property = test_enum_item_get_property;

  test_enum_item_properties[TEST_ENUM_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_enum_item_properties[TEST_ENUM_ITEM_PROP_MODE] =
    g_param_spec_enum ("mode", NULL, NULL,
                       test_materialize_mode_get_type (), TEST_MATERIALIZE_MODE_ALPHA,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_ENUM_ITEM_N_PROPS,
                                     test_enum_item_properties);

  gom_entity_class_set_relation (entity_class, "enum_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "mode", TRUE);
}

static void
test_enum_item_init (TestEnumItem *self)
{
  self->mode = TEST_MATERIALIZE_MODE_ALPHA;
}

static void
test_datetime_item_finalize (GObject *object)
{
  TestDateTimeItem *self = (TestDateTimeItem *)object;

  g_clear_pointer (&self->stamp, g_date_time_unref);

  G_OBJECT_CLASS (test_datetime_item_parent_class)->finalize (object);
}

static void
test_datetime_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TestDateTimeItem *self = (TestDateTimeItem *)object;

  switch (prop_id)
    {
    case TEST_DATETIME_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_DATETIME_ITEM_PROP_STAMP:
      g_value_set_boxed (value, self->stamp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_datetime_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TestDateTimeItem *self = (TestDateTimeItem *)object;

  switch (prop_id)
    {
    case TEST_DATETIME_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_DATETIME_ITEM_PROP_STAMP:
      g_clear_pointer (&self->stamp, g_date_time_unref);
      if (g_value_get_boxed (value) != NULL)
        self->stamp = g_date_time_ref (g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_datetime_item_class_init (TestDateTimeItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_datetime_item_finalize;
  object_class->set_property = test_datetime_item_set_property;
  object_class->get_property = test_datetime_item_get_property;

  test_datetime_item_properties[TEST_DATETIME_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_datetime_item_properties[TEST_DATETIME_ITEM_PROP_STAMP] =
    g_param_spec_boxed ("stamp", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_DATETIME_ITEM_N_PROPS,
                                     test_datetime_item_properties);

  gom_entity_class_set_relation (entity_class, "datetime_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "stamp", TRUE);
}

static void
test_datetime_item_init (TestDateTimeItem *self)
{
}

static void
test_crud_item_finalize (GObject *object)
{
  TestCrudItem *self = (TestCrudItem *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->payload, g_free);

  G_OBJECT_CLASS (test_crud_item_parent_class)->finalize (object);
}

static void
test_crud_item_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  TestCrudItem *self = (TestCrudItem *)object;

  switch (prop_id)
    {
    case TEST_CRUD_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_CRUD_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_CRUD_ITEM_PROP_PAYLOAD:
      g_value_set_string (value, self->payload);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_crud_item_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  TestCrudItem *self = (TestCrudItem *)object;

  switch (prop_id)
    {
    case TEST_CRUD_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_CRUD_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_CRUD_ITEM_PROP_PAYLOAD:
      g_set_str (&self->payload, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_crud_item_class_init (TestCrudItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_crud_item_finalize;
  object_class->set_property = test_crud_item_set_property;
  object_class->get_property = test_crud_item_get_property;

  test_crud_item_properties[TEST_CRUD_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_crud_item_properties[TEST_CRUD_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_crud_item_properties[TEST_CRUD_ITEM_PROP_PAYLOAD] =
    g_param_spec_string ("payload", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_CRUD_ITEM_N_PROPS,
                                     test_crud_item_properties);

  gom_entity_class_set_relation (entity_class, "entity_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_byte_transform (entity_class,
                                                "payload",
                                                test_materialize_item_to_bytes,
                                                test_materialize_item_from_bytes,
                                                NULL,
                                                NULL);
}

static void
test_crud_item_init (TestCrudItem *self)
{
}

static GomExpression *
test_crud_allow_default_id_item_dup_identity_value (GomEntity   *self,
                                                    const char  *identity_field,
                                                    GError     **error)
{
  GObjectClass *object_class;
  GParamSpec *pspec;
  g_auto(GValue) value = G_VALUE_INIT;

  object_class = G_OBJECT_GET_CLASS (self);
  pspec = g_object_class_find_property (object_class, identity_field);

  if (pspec == NULL)
    return GOM_ENTITY_CLASS (test_crud_allow_default_id_item_parent_class)->dup_identity_value (self,
                                                                                                  identity_field,
                                                                                                  error);

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_object_get_property (G_OBJECT (self), identity_field, &value);

  return gom_literal_expression_new (&value);
}

static void
test_crud_allow_default_id_item_class_init (TestCrudAllowDefaultIdItemClass *klass)
{
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  entity_class->dup_identity_value = test_crud_allow_default_id_item_dup_identity_value;
}

static void
test_crud_allow_default_id_item_init (TestCrudAllowDefaultIdItem *self)
{
}

static void
test_no_identity_item_finalize (GObject *object)
{
  TestNoIdentityItem *self = (TestNoIdentityItem *)object;

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (test_no_identity_item_parent_class)->finalize (object);
}

static void
test_no_identity_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  TestNoIdentityItem *self = (TestNoIdentityItem *)object;

  switch (prop_id)
    {
    case TEST_NO_IDENTITY_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_NO_IDENTITY_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_no_identity_item_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  TestNoIdentityItem *self = (TestNoIdentityItem *)object;

  switch (prop_id)
    {
    case TEST_NO_IDENTITY_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_NO_IDENTITY_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_no_identity_item_class_init (TestNoIdentityItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_no_identity_item_finalize;
  object_class->set_property = test_no_identity_item_set_property;
  object_class->get_property = test_no_identity_item_get_property;

  test_no_identity_item_properties[TEST_NO_IDENTITY_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_no_identity_item_properties[TEST_NO_IDENTITY_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_NO_IDENTITY_ITEM_N_PROPS,
                                     test_no_identity_item_properties);

  gom_entity_class_set_relation (entity_class, "no_identity_items");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "name", TRUE);
}

static void
test_no_identity_item_init (TestNoIdentityItem *self)
{
}

static void
test_migration_item_finalize (GObject *object)
{
  TestMigrationItem *self = (TestMigrationItem *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->legacy, g_free);
  g_clear_pointer (&self->tag, g_free);

  G_OBJECT_CLASS (test_migration_item_parent_class)->finalize (object);
}

static void
test_migration_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  TestMigrationItem *self = (TestMigrationItem *)object;

  switch (prop_id)
    {
    case TEST_MIGRATION_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_MIGRATION_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_MIGRATION_ITEM_PROP_LEGACY:
      g_value_set_string (value, self->legacy);
      break;

    case TEST_MIGRATION_ITEM_PROP_TAG:
      g_value_set_string (value, self->tag);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_migration_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  TestMigrationItem *self = (TestMigrationItem *)object;

  switch (prop_id)
    {
    case TEST_MIGRATION_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_MIGRATION_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_MIGRATION_ITEM_PROP_LEGACY:
      g_set_str (&self->legacy, g_value_get_string (value));
      break;

    case TEST_MIGRATION_ITEM_PROP_TAG:
      g_set_str (&self->tag, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_migration_item_class_init (TestMigrationItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_migration_item_finalize;
  object_class->set_property = test_migration_item_set_property;
  object_class->get_property = test_migration_item_get_property;

  test_migration_item_properties[TEST_MIGRATION_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_migration_item_properties[TEST_MIGRATION_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_migration_item_properties[TEST_MIGRATION_ITEM_PROP_LEGACY] =
    g_param_spec_string ("legacy", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_migration_item_properties[TEST_MIGRATION_ITEM_PROP_TAG] =
    g_param_spec_string ("tag", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_MIGRATION_ITEM_N_PROPS,
                                     test_migration_item_properties);

  gom_entity_class_set_relation (entity_class, "migrate_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "name", 1);
  gom_entity_class_property_set_nonnull (entity_class, "name", TRUE);
  gom_entity_class_property_set_version_added (entity_class, "legacy", 1);
  gom_entity_class_property_set_version_removed (entity_class, "legacy", 2);
  gom_entity_class_property_set_search_flags (entity_class, "legacy", GOM_SEARCH_INDEXED);
  gom_entity_class_property_set_version_added (entity_class, "tag", 2);
  gom_entity_class_property_set_search_flags (entity_class, "tag", GOM_SEARCH_INDEXED);
}

static void
test_migration_item_init (TestMigrationItem *self)
{
}

static void
test_unsupported_transform_item_finalize (GObject *object)
{
  TestUnsupportedTransformItem *self = (TestUnsupportedTransformItem *) object;

  g_clear_pointer (&self->opaque, g_variant_unref);

  G_OBJECT_CLASS (test_unsupported_transform_item_parent_class)->finalize (object);
}

static void
test_unsupported_transform_item_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  TestUnsupportedTransformItem *self = (TestUnsupportedTransformItem *) object;

  switch (prop_id)
    {
    case TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_OPAQUE:
      g_value_set_variant (value, self->opaque);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_unsupported_transform_item_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  TestUnsupportedTransformItem *self = (TestUnsupportedTransformItem *) object;

  switch (prop_id)
    {
    case TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_OPAQUE:
      g_clear_pointer (&self->opaque, g_variant_unref);
      self->opaque = g_value_dup_variant (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_unsupported_transform_item_class_init (TestUnsupportedTransformItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_unsupported_transform_item_finalize;
  object_class->set_property = test_unsupported_transform_item_set_property;
  object_class->get_property = test_unsupported_transform_item_get_property;

  test_unsupported_transform_item_properties[TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_unsupported_transform_item_properties[TEST_UNSUPPORTED_TRANSFORM_ITEM_PROP_OPAQUE] =
    g_param_spec_variant ("opaque", NULL, NULL,
                          G_VARIANT_TYPE_ANY, NULL,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_UNSUPPORTED_TRANSFORM_ITEM_N_PROPS,
                                     test_unsupported_transform_item_properties);

  gom_entity_class_set_relation (entity_class, "unsupported_transform_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
}

static void
test_unsupported_transform_item_init (TestUnsupportedTransformItem *self)
{
}

static void
test_invalid_migration_item_finalize (GObject *object)
{
  TestInvalidMigrationItem *self = (TestInvalidMigrationItem *) object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->critical, g_free);

  G_OBJECT_CLASS (test_invalid_migration_item_parent_class)->finalize (object);
}

static void
test_invalid_migration_item_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  TestInvalidMigrationItem *self = (TestInvalidMigrationItem *) object;

  switch (prop_id)
    {
    case TEST_INVALID_MIGRATION_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_INVALID_MIGRATION_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_INVALID_MIGRATION_ITEM_PROP_CRITICAL:
      g_value_set_string (value, self->critical);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_invalid_migration_item_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  TestInvalidMigrationItem *self = (TestInvalidMigrationItem *) object;

  switch (prop_id)
    {
    case TEST_INVALID_MIGRATION_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_INVALID_MIGRATION_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_INVALID_MIGRATION_ITEM_PROP_CRITICAL:
      g_set_str (&self->critical, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_invalid_migration_item_class_init (TestInvalidMigrationItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_invalid_migration_item_finalize;
  object_class->set_property = test_invalid_migration_item_set_property;
  object_class->get_property = test_invalid_migration_item_get_property;

  test_invalid_migration_item_properties[TEST_INVALID_MIGRATION_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_invalid_migration_item_properties[TEST_INVALID_MIGRATION_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_invalid_migration_item_properties[TEST_INVALID_MIGRATION_ITEM_PROP_CRITICAL] =
    g_param_spec_string ("critical", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_INVALID_MIGRATION_ITEM_N_PROPS,
                                     test_invalid_migration_item_properties);

  gom_entity_class_set_relation (entity_class, "invalid_migration_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_version_added (entity_class, "name", 1);
  gom_entity_class_property_set_version_added (entity_class, "critical", 2);
  gom_entity_class_property_set_nonnull (entity_class, "critical", TRUE);
}

static void
test_invalid_migration_item_init (TestInvalidMigrationItem *self)
{
}

static void
test_strv_item_finalize (GObject *object)
{
  TestStrvItem *self = (TestStrvItem *)object;

  g_clear_pointer (&self->tags, g_strfreev);

  G_OBJECT_CLASS (test_strv_item_parent_class)->finalize (object);
}

static void
test_strv_item_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  TestStrvItem *self = (TestStrvItem *)object;

  switch (prop_id)
    {
    case TEST_STRV_ITEM_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case TEST_STRV_ITEM_PROP_TAGS:
      g_value_set_boxed (value, self->tags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_strv_item_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  TestStrvItem *self = (TestStrvItem *)object;

  switch (prop_id)
    {
    case TEST_STRV_ITEM_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case TEST_STRV_ITEM_PROP_TAGS:
      g_clear_pointer (&self->tags, g_strfreev);
      self->tags = g_strdupv ((char **) g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_strv_item_class_init (TestStrvItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_strv_item_finalize;
  object_class->set_property = test_strv_item_set_property;
  object_class->get_property = test_strv_item_get_property;

  test_strv_item_properties[TEST_STRV_ITEM_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_strv_item_properties[TEST_STRV_ITEM_PROP_TAGS] =
    g_param_spec_boxed ("tags", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_STRV_ITEM_N_PROPS,
                                     test_strv_item_properties);

  gom_entity_class_set_relation (entity_class, "strv_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "tags", TRUE);
}

static void
test_strv_item_init (TestStrvItem *self)
{
}

static void
test_materialize_item_finalize (GObject *object)
{
  TestMaterializeItem *self = (TestMaterializeItem *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->payload, g_free);
  g_clear_pointer (&self->internal, g_free);
  g_clear_pointer (&self->when, g_date_time_unref);

  G_OBJECT_CLASS (test_materialize_item_parent_class)->finalize (object);
}

static void
test_materialize_item_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  TestMaterializeItem *self = (TestMaterializeItem *)object;

  switch (prop_id)
    {
    case TEST_MATERIALIZE_ITEM_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_PAYLOAD:
      g_value_set_string (value, self->payload);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_WHEN:
      g_value_set_boxed (value, self->when);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_INTERNAL:
      g_value_set_string (value, self->internal);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_COUNT:
      g_value_set_int (value, self->count);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_BIG_COUNT:
      g_value_set_int64 (value, self->big_count);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_UCOUNT:
      g_value_set_uint (value, self->ucount);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_FLAG:
      g_value_set_boolean (value, self->flag);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_RATIO:
      g_value_set_double (value, self->ratio);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_FVALUE:
      g_value_set_float (value, self->fvalue);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_materialize_item_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  TestMaterializeItem *self = (TestMaterializeItem *)object;

  switch (prop_id)
    {
    case TEST_MATERIALIZE_ITEM_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case TEST_MATERIALIZE_ITEM_PROP_PAYLOAD:
      g_set_str (&self->payload, g_value_get_string (value));
      break;

    case TEST_MATERIALIZE_ITEM_PROP_WHEN:
      g_clear_pointer (&self->when, g_date_time_unref);
      if (g_value_get_boxed (value) != NULL)
        self->when = g_date_time_ref (g_value_get_boxed (value));
      break;

    case TEST_MATERIALIZE_ITEM_PROP_INTERNAL:
      g_set_str (&self->internal, g_value_get_string (value));
      break;

    case TEST_MATERIALIZE_ITEM_PROP_COUNT:
      self->count = g_value_get_int (value);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_BIG_COUNT:
      self->big_count = g_value_get_int64 (value);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_UCOUNT:
      self->ucount = g_value_get_uint (value);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_FLAG:
      self->flag = g_value_get_boolean (value);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_RATIO:
      self->ratio = g_value_get_double (value);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_FVALUE:
      self->fvalue = g_value_get_float (value);
      break;

    case TEST_MATERIALIZE_ITEM_PROP_MODE:
      self->mode = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GBytes *
test_materialize_item_to_bytes (const GValue  *value,
                                gpointer       user_data,
                                GError       **error)
{
  const char *str = g_value_get_string (value);

  if (str == NULL)
    return g_bytes_new (NULL, 0);

  return g_bytes_new (str, strlen (str));
}

static gboolean
test_materialize_item_from_bytes (GBytes    *bytes,
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

static void
test_materialize_item_class_init (TestMaterializeItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_materialize_item_finalize;
  object_class->set_property = test_materialize_item_set_property;
  object_class->get_property = test_materialize_item_get_property;

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_PAYLOAD] =
    g_param_spec_string ("payload", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_WHEN] =
    g_param_spec_boxed ("when", NULL, NULL,
                        G_TYPE_DATE_TIME,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_INTERNAL] =
    g_param_spec_string ("internal", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_COUNT] =
    g_param_spec_int ("count", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_BIG_COUNT] =
    g_param_spec_int64 ("big-count", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_UCOUNT] =
    g_param_spec_uint ("ucount", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_FLAG] =
    g_param_spec_boolean ("flag", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_RATIO] =
    g_param_spec_double ("ratio", NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_FVALUE] =
    g_param_spec_float ("fvalue", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  test_materialize_item_properties[TEST_MATERIALIZE_ITEM_PROP_MODE] =
    g_param_spec_enum ("mode", NULL, NULL,
                       test_materialize_mode_get_type (), TEST_MATERIALIZE_MODE_ALPHA,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     TEST_MATERIALIZE_ITEM_N_PROPS,
                                     test_materialize_item_properties);

  gom_entity_class_set_relation (entity_class, "materialize_items");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_field_name (entity_class, "big-count", "big_count");
  gom_entity_class_property_set_mapped (entity_class, "internal", FALSE);
  gom_entity_class_property_set_byte_transform (entity_class,
                                                "payload",
                                                test_materialize_item_to_bytes,
                                                test_materialize_item_from_bytes,
                                                NULL,
                                                NULL);
}

static void
test_materialize_item_init (TestMaterializeItem *self)
{
}

static void
test_sqlite_driver_pool (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GError) error = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);

  g_assert_true (GOM_IS_DRIVER (context.driver));
  g_assert_cmpstr (gom_driver_dup_uri (context.driver), ==, context.db_uri);
}

static void
test_sqlite_repository_query (void)
{
  static const char *expected_names[] = { "alpha", "beta", "gamma" };
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  guint count = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "INSERT INTO items (name) VALUES "
                     "('alpha'), ('beta'), ('gamma')"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, "items_fts");
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build_with_count (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_COUNT);
  g_assert_cmpuint (gom_cursor_get_count (cursor), ==, G_N_ELEMENTS (expected_names));

  while (TRUE)
    {
      const char *name = NULL;
      gboolean has_row = dex_await_boolean (gom_cursor_next (cursor), &error);

      g_assert_no_error (error);

      if (!has_row)
        break;

      g_assert_cmpint (gom_cursor_get_n_columns (cursor), >=, 2);

      if (count < G_N_ELEMENTS (expected_names))
        {
          g_assert_cmpint (gom_cursor_get_column_int64 (cursor, 0), ==, (gint64) count + 1);
          name = gom_cursor_get_column_string (cursor, 1);
          g_assert_cmpstr (name, ==, expected_names[count]);
        }

      count++;
    }

  g_assert_cmpuint (count, ==, G_N_ELEMENTS (expected_names));

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
}

static void
test_sqlite_repository_count (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  gint64 count;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "INSERT INTO items (name) VALUES "
                     "('alpha'), ('beta'), ('gamma')"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, test_insert_item_get_type ());
  gom_query_builder_set_limit (builder, 1);
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  count = dex_await_int64 (gom_repository_count (repository, query), &error);
  g_assert_no_error (error);
  g_assert_cmpint (count, ==, 3);
}

static GomRecordListItem *
test_sqlite_wait_for_record_item (GListModel *model,
                                  guint       position)
{
  g_autoptr(GomRecordListItem) item = NULL;

  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  item = g_list_model_get_item (model, position);
  for (guint i = 0; i < 100; i++)
    {
      g_autoptr(GomRecord) record = NULL;

      if (item != NULL)
        record = gom_record_list_item_dup_record (item);
      if (record != NULL)
        return g_steal_pointer (&item);

      dex_await (dex_timeout_new_msec (10), NULL);
    }

  return NULL;
}

static void
test_sqlite_repository_list_records (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomRecordListModel) model = NULL;
  g_autoptr(GomRecordListItem) first_item = NULL;
  g_autoptr(GomRecord) first = NULL;
  g_autoptr(GError) error = NULL;
  GWeakRef first_item_weak;
  GWeakRef first_weak;
  GValue value = G_VALUE_INIT;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE record_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "INSERT INTO record_items (name) VALUES "
                     "('alpha'), ('beta'), ('gamma')"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, "record_items");
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  model = dex_await_object (gom_repository_list_records (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_RECORD_LIST_MODEL (model));
  g_assert_cmpuint (g_list_model_get_item_type (G_LIST_MODEL (model)), ==, GOM_TYPE_RECORD_LIST_ITEM);

  dex_await (gom_record_list_model_reload (model), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 3);

  first_item = test_sqlite_wait_for_record_item (G_LIST_MODEL (model), 0);
  g_assert_true (GOM_IS_RECORD_LIST_ITEM (first_item));
  first = gom_record_list_item_dup_record (first_item);
  g_assert_true (GOM_IS_RECORD (first));
  g_assert_true (gom_record_get_column_by_name (first, "name", &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "alpha");
  g_value_unset (&value);

  g_weak_ref_init (&first_item_weak, first_item);
  g_weak_ref_init (&first_weak, first);
  g_clear_object (&first);
  g_clear_object (&first_item);

  {
    g_autoptr(GObject) retained_item = NULL;
    g_autoptr(GObject) retained_record = NULL;

    retained_item = g_weak_ref_get (&first_item_weak);
    retained_record = g_weak_ref_get (&first_weak);

    g_assert_null (retained_item);
    g_assert_null (retained_record);
  }

  g_weak_ref_clear (&first_item_weak);
  g_weak_ref_clear (&first_weak);
}

static void
test_sqlite_repository_list_entities (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GListStore) list = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  guint n_items;
  g_autofree char *first_name = NULL;
  g_autofree char *second_name = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "INSERT INTO entity_items (name, payload) VALUES "
                     "('alpha', x'7061796c6f61642d61'), "
                     "('beta', x'7061796c6f61642d62'), "
                     "('gamma', x'7061796c6f61642d63')"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  filter = gom_binary_expression_new_not_equal (gom_field_expression_new ("name"),
                                                gom_literal_expression_new_string ("gamma"));
  ordering = gom_ordering_new (gom_field_expression_new ("name"), GOM_SORT_DESCENDING);

  list = dex_await_object (gom_repository_list_entities (repository,
                                                         test_crud_item_get_type (),
                                                         filter,
                                                         ordering),
                           &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_LIST_STORE (list));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (list));
  g_assert_cmpuint (n_items, ==, 2);

  {
    g_autoptr(GomEntity) first = g_list_model_get_item (G_LIST_MODEL (list), 0);
    g_autoptr(GomEntity) second = g_list_model_get_item (G_LIST_MODEL (list), 1);

    g_assert_true (GOM_IS_ENTITY (first));
    g_assert_true (GOM_IS_ENTITY (second));

    g_object_get (first,
                  "name", &first_name,
                  NULL);
    g_object_get (second,
                  "name", &second_name,
                  NULL);
  }

  g_assert_cmpstr (first_name, ==, "beta");
  g_assert_cmpstr (second_name, ==, "alpha");
}

static void
test_sqlite_repository_insert (void)
{
  static const char *expected_names[] = { "alpha", "beta", "gamma" };
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomInsertionBuilder) insert_builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomRecord) record = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  GValue name_value = G_VALUE_INIT;
  GValue category_value = G_VALUE_INIT;
  guint count = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  insert_builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_relation (insert_builder, "items");
  gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("name"));
  gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("category"));

  g_value_init (&name_value, G_TYPE_STRING);
  g_value_init (&category_value, G_TYPE_STRING);
  g_value_set_string (&name_value, "alpha");
  g_value_set_string (&category_value, "first");
  {
    GomExpression *row[] = {
      gom_literal_expression_new (&name_value),
      gom_literal_expression_new (&category_value)
    };
    gom_insertion_builder_add_row (insert_builder, row, G_N_ELEMENTS (row));
  }

  g_value_set_string (&name_value, "beta");
  g_value_set_string (&category_value, "second");
  {
    GomExpression *row[] = {
      gom_literal_expression_new (&name_value),
      gom_literal_expression_new (&category_value)
    };
    gom_insertion_builder_add_row (insert_builder, row, G_N_ELEMENTS (row));
  }

  g_value_unset (&name_value);
  g_value_unset (&category_value);

  insertion = gom_insertion_builder_build (insert_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (insertion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 2);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (result)), ==, 2);

  record = g_list_model_get_item (G_LIST_MODEL (result), 0);
  g_assert_nonnull (record);
  {
    GValue value = G_VALUE_INIT;

    g_assert_true (gom_record_get_column_by_name (record, "changes", &value));
    g_assert_true (G_VALUE_HOLDS_INT64 (&value));
    g_assert_cmpint (g_value_get_int64 (&value), ==, 1);
    g_value_unset (&value);

    g_assert_true (gom_record_get_column_by_name (record, "rowid", &value));
    g_assert_true (G_VALUE_HOLDS_INT64 (&value));
    g_assert_cmpint (g_value_get_int64 (&value), >, 0);
    g_value_unset (&value);
  }
  g_clear_object (&record);

  g_clear_pointer (&insert_builder, gom_insertion_builder_unref);
  g_clear_object (&insertion);

  insert_builder = gom_insertion_builder_new (repository);
  {
    g_autoptr(GomEntity) entity = g_object_new (test_insert_item_get_type (),
                                                "name", "gamma",
                                                "category", "third",
                                                NULL);
    g_assert_true (gom_insertion_builder_add_entity (insert_builder, entity, &error));
    g_assert_no_error (error);
  }

  insertion = gom_insertion_builder_build (insert_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (insertion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (result)), ==, 1);

  g_clear_object (&record);
  record = g_list_model_get_item (G_LIST_MODEL (result), 0);
  g_assert_nonnull (record);
  {
    GValue value = G_VALUE_INIT;

    g_assert_true (gom_record_get_column_by_name (record, "rowid", &value));
    g_assert_true (G_VALUE_HOLDS_INT64 (&value));
    g_assert_cmpint (g_value_get_int64 (&value), >, 0);
    g_value_unset (&value);
  }
  g_clear_object (&record);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, "items");
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  while (TRUE)
    {
      const char *name = NULL;
      gboolean has_row = dex_await_boolean (gom_cursor_next (cursor), &error);

      g_assert_no_error (error);

      if (!has_row)
        break;

      name = gom_cursor_get_column_string (cursor, 1);
      g_assert_cmpstr (name, ==, expected_names[count]);
      count++;
    }

  g_assert_cmpuint (count, ==, G_N_ELEMENTS (expected_names));

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
}

static void
test_sqlite_repository_insert_during_open_read_cursor (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomInsertionBuilder) insert_builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  GValue name_value = G_VALUE_INIT;
  GValue category_value = G_VALUE_INIT;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "INSERT INTO items (name, category) VALUES "
                     "('alpha', 'first'), ('beta', 'second')"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, "items");
  query = gom_query_builder_build_with_count (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_cmpuint (gom_cursor_get_count (cursor), ==, 2);
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  insert_builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_relation (insert_builder, "items");
  gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("name"));
  gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("category"));

  g_value_init (&name_value, G_TYPE_STRING);
  g_value_init (&category_value, G_TYPE_STRING);
  g_value_set_string (&name_value, "gamma");
  g_value_set_string (&category_value, "third");
  {
    GomExpression *row[] = {
      gom_literal_expression_new (&name_value),
      gom_literal_expression_new (&category_value)
    };

    gom_insertion_builder_add_row (insert_builder, row, G_N_ELEMENTS (row));
  }

  g_value_unset (&name_value);
  g_value_unset (&category_value);

  insertion = gom_insertion_builder_build (insert_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (insertion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
}

static GomRecord *
test_sqlite_mutation_result_get_record (GomMutationResult *result,
                                        guint              index)
{
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (result)), >, index);

  return g_list_model_get_item (G_LIST_MODEL (result), index);
}

static void
test_sqlite_assert_mutation_result_row (GomMutationResult *result,
                                        guint              index,
                                        guint64            expected_changes,
                                        gboolean           expect_rowid)
{
  g_autoptr(GomRecord) record = NULL;
  GValue value = G_VALUE_INIT;

  record = test_sqlite_mutation_result_get_record (result, index);
  g_assert_nonnull (record);
  g_assert_cmpuint (gom_record_get_n_columns (record), ==, 2);

  g_assert_true (gom_record_get_column_by_name (record, "changes", &value));
  g_assert_true (G_VALUE_HOLDS_INT64 (&value));
  g_assert_cmpint (g_value_get_int64 (&value), ==, (gint64) expected_changes);
  g_value_unset (&value);

  g_assert_true (gom_record_get_column_by_name (record, "rowid", &value));
  if (expect_rowid)
    {
      g_assert_true (G_VALUE_HOLDS_INT64 (&value));
      g_assert_cmpint (g_value_get_int64 (&value), >, 0);
    }
  else
    {
      g_assert_true (G_VALUE_HOLDS_POINTER (&value));
      g_assert_null (g_value_get_pointer (&value));
    }
  g_value_unset (&value);
}

static void
test_sqlite_repository_insert_unique_constraint (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomEntity) duplicate = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                       "CREATE UNIQUE INDEX items_name_idx ON items (name)"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_insert_item_get_type (),
                         "name", "alpha",
                         "category", "first",
                         NULL);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (result)), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, TRUE);
  g_clear_object (&result);

  duplicate = g_object_new (test_insert_item_get_type (),
                            "name", "alpha",
                            "category", "second",
                            NULL);
  gom_entity_set_repository (duplicate, repository);

  result = dex_await_object (gom_entity_insert (duplicate), &error);
  g_assert_null (result);
  g_assert_true (g_error_matches (error, GOM_ERROR, GOM_ERROR_CONSTRAINT));
  g_clear_error (&error);
}

static void
test_sqlite_repository_insert_hyphenated_property (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomInsertionBuilder) insert_builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  g_autoptr(GomEntity) entity = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE hyphen_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  \"format-type\" TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_hyphen_item_get_type (),
                         "format-type", "omega",
                         NULL);
  gom_entity_set_repository (entity, repository);

  insert_builder = gom_insertion_builder_new (repository);
  g_assert_true (gom_insertion_builder_add_entity (insert_builder, entity, &error));
  g_assert_no_error (error);

  insertion = gom_insertion_builder_build (insert_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (insertion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_clear_object (&result);

  test_sqlite_open (context.db_path, &db);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT \"format-type\" FROM hyphen_items ORDER BY id LIMIT 1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "omega");
  sqlite3_finalize (stmt);
  test_sqlite_close (db);
}

static void
test_sqlite_repository_find_one (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) inserted = NULL;
  g_autoptr(GomEntity) loaded = NULL;
  g_autoptr(GomEntity) missing = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  gint64 id = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE hyphen_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  \"format-type\" TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  inserted = g_object_new (test_hyphen_item_get_type (),
                           "format-type", "omega",
                           NULL);

  g_assert_true (dex_await (gom_repository_insert_entity (repository, inserted), &error));
  g_assert_no_error (error);

  g_object_get (inserted,
                "id", &id,
                NULL);
  g_assert_cmpint (id, >, 0);

  loaded = dex_await_object (gom_repository_find_one (repository,
                                                      test_hyphen_item_get_type (),
                                                      "id", id,
                                                      NULL),
                             &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (loaded));
  g_assert_cmpint (gom_entity_get_origin (loaded), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);

  missing = dex_await_object (gom_repository_find_one (repository,
                                                       test_hyphen_item_get_type (),
                                                       "format-type", "missing",
                                                       NULL),
                              &error);
  g_assert_no_error (error);
  g_assert_null (missing);
}

static void
test_sqlite_repository_find_one_with_properties (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) inserted = NULL;
  g_autoptr(GomEntity) loaded = NULL;
  g_autoptr(GError) error = NULL;
  const char *properties[] = { "id", "format-type" };
  GValue values[2] = { G_VALUE_INIT, G_VALUE_INIT };
  sqlite3 *db = NULL;
  gint64 id = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE hyphen_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  \"format-type\" TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  inserted = g_object_new (test_hyphen_item_get_type (),
                           "format-type", "omega",
                           NULL);

  g_assert_true (dex_await (gom_repository_insert_entity (repository, inserted), &error));
  g_assert_no_error (error);

  g_object_get (inserted,
                "id", &id,
                NULL);
  g_assert_cmpint (id, >, 0);

  g_value_init (&values[0], G_TYPE_INT64);
  g_value_set_int64 (&values[0], id);
  g_value_init (&values[1], G_TYPE_STRING);
  g_value_set_static_string (&values[1], "omega");

  loaded = dex_await_object (gom_repository_find_one_with_properties (repository,
                                                                      test_hyphen_item_get_type (),
                                                                      G_N_ELEMENTS (properties),
                                                                      properties,
                                                                      values),
                             &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (loaded));

  g_value_unset (&values[0]);
  g_value_unset (&values[1]);
}

static void
test_sqlite_session_find_one (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomEntity) inserted = NULL;
  g_autoptr(GomEntity) loaded = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  gint64 id = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE hyphen_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  \"format-type\" TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_SESSION (session));

  inserted = g_object_new (test_hyphen_item_get_type (),
                           "format-type", "omega",
                           NULL);

  g_assert_true (dex_await (gom_session_insert_entity (session, inserted), &error));
  g_assert_no_error (error);

  g_object_get (inserted,
                "id", &id,
                NULL);
  g_assert_cmpint (id, >, 0);

  loaded = dex_await_object (gom_session_find_one (session,
                                                   test_hyphen_item_get_type (),
                                                   "id", id,
                                                   "format-type", "omega",
                                                   NULL),
                             &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (loaded));

  g_assert_true (dex_await (gom_session_rollback (session), &error));
  g_assert_no_error (error);
}

static void
test_sqlite_repository_insert_gtype_property (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  GType format_type = G_TYPE_NONE;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE gtype_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  \"format-type\" TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_gtype_item_get_type (),
                         "format-type", test_materialize_mode_get_type (),
                         NULL);
  gom_entity_set_repository (entity, repository);

  {
    g_autoptr(DexFuture) insert_future = gom_entity_insert (entity);
    g_autoptr(GomMutationResult) insert_result = dex_await_object (g_steal_pointer (&insert_future),
                                                                   &error);

    g_assert_no_error (error);
    g_assert_true (GOM_IS_MUTATION_RESULT (insert_result));
    g_assert_cmpuint (gom_mutation_result_get_affected_rows (insert_result), ==, 1);
    test_sqlite_assert_mutation_result_row (insert_result, 0, 1, TRUE);
  }

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_gtype_item_get_type ());
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  {
    g_autoptr(GomEntity) materialized = gom_cursor_materialize (cursor, &error);

    g_assert_no_error (error);
    g_assert_true (GOM_IS_ENTITY (materialized));
    g_object_get (materialized,
                  "format-type", &format_type,
                  NULL);
  g_assert_cmpuint (format_type, ==, test_materialize_mode_get_type ());
  }

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
}

static void
test_sqlite_repository_insert_enum_property (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE enum_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  mode INTEGER NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_enum_item_get_type (),
                         "mode", TEST_MATERIALIZE_MODE_BETA,
                         NULL);
  gom_entity_set_repository (entity, repository);

  {
    g_autoptr(DexFuture) insert_future = gom_entity_insert (entity);
    g_autoptr(GomMutationResult) insert_result = dex_await_object (g_steal_pointer (&insert_future),
                                                                   &error);

    g_assert_no_error (error);
    g_assert_true (GOM_IS_MUTATION_RESULT (insert_result));
    g_assert_cmpuint (gom_mutation_result_get_affected_rows (insert_result), ==, 1);
    test_sqlite_assert_mutation_result_row (insert_result, 0, 1, TRUE);
  }

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT mode FROM enum_items ORDER BY id LIMIT 1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_int (stmt, 0), ==, TEST_MATERIALIZE_MODE_BETA);
  sqlite3_finalize (stmt);
  sqlite3_close (db);
  db = NULL;

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_enum_item_get_type ());
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  {
    g_autoptr(GomEntity) materialized = gom_cursor_materialize (cursor, &error);
    TestMaterializeMode stored = TEST_MATERIALIZE_MODE_ALPHA;

    g_assert_no_error (error);
    g_assert_true (GOM_IS_ENTITY (materialized));
    g_object_get (materialized,
                  "mode", &stored,
                  NULL);
    g_assert_cmpint (stored, ==, TEST_MATERIALIZE_MODE_BETA);
  }

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
}

static void
test_sqlite_repository_insert_datetime_property (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomEntity) null_entity = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomCursor) null_cursor = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  g_autoptr(GDateTime) when = NULL;
  g_autoptr(GDateTime) materialized_when = NULL;
  g_autofree char *when_str = NULL;
  g_autofree char *materialized_when_str = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE datetime_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  stamp TEXT"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  when = g_date_time_new_from_iso8601 ("2024-01-02T03:04:05Z", NULL);
  g_assert_nonnull (when);
  when_str = g_date_time_format_iso8601 (when);

  entity = g_object_new (test_datetime_item_get_type (),
                         "stamp", when,
                         NULL);
  gom_entity_set_repository (entity, repository);

  {
    g_autoptr(DexFuture) insert_future = gom_entity_insert (entity);
    g_autoptr(GomMutationResult) insert_result = dex_await_object (g_steal_pointer (&insert_future),
                                                                   &error);

    g_assert_no_error (error);
    g_assert_true (GOM_IS_MUTATION_RESULT (insert_result));
    g_assert_cmpuint (gom_mutation_result_get_affected_rows (insert_result), ==, 1);
    test_sqlite_assert_mutation_result_row (insert_result, 0, 1, TRUE);
  }

  null_entity = g_object_new (test_datetime_item_get_type (), NULL);
  gom_entity_set_repository (null_entity, repository);

  {
    g_autoptr(DexFuture) insert_future = gom_entity_insert (null_entity);
    g_autoptr(GomMutationResult) insert_result = dex_await_object (g_steal_pointer (&insert_future),
                                                                   &error);

    g_assert_no_error (error);
    g_assert_true (GOM_IS_MUTATION_RESULT (insert_result));
    g_assert_cmpuint (gom_mutation_result_get_affected_rows (insert_result), ==, 1);
    test_sqlite_assert_mutation_result_row (insert_result, 0, 1, TRUE);
  }

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT stamp FROM datetime_items ORDER BY id LIMIT 1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, when_str);
  g_assert_cmpint (sqlite3_column_type (stmt, 0), ==, SQLITE_TEXT);
  sqlite3_finalize (stmt);
  sqlite3_close (db);
  db = NULL;

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT stamp FROM datetime_items ORDER BY id LIMIT 1 OFFSET 1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_type (stmt, 0), ==, SQLITE_NULL);
  sqlite3_finalize (stmt);
  sqlite3_close (db);
  db = NULL;

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_datetime_item_get_type ());
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  {
    g_autoptr(GomEntity) materialized = gom_cursor_materialize (cursor, &error);

    g_assert_no_error (error);
    g_assert_true (GOM_IS_ENTITY (materialized));
    g_object_get (materialized,
                  "stamp", &materialized_when,
                  NULL);
    g_assert_nonnull (materialized_when);
    materialized_when_str = g_date_time_format_iso8601 (materialized_when);
    g_assert_cmpstr (materialized_when_str, ==, when_str);
  }

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_datetime_item_get_type ());
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  null_cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (null_cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (null_cursor), &error));
  g_assert_no_error (error);
  g_assert_true (dex_await_boolean (gom_cursor_next (null_cursor), &error));
  g_assert_no_error (error);

  {
    g_autoptr(GomEntity) materialized = gom_cursor_materialize (null_cursor, &error);
    GDateTime *stamp = (GDateTime *) 1;

    g_assert_no_error (error);
    g_assert_true (GOM_IS_ENTITY (materialized));
    g_object_get (materialized,
                  "stamp", &stamp,
                  NULL);
    g_assert_null (stamp);
  }

  dex_await (gom_cursor_close (null_cursor), &error);
  g_assert_no_error (error);
}

static void
test_sqlite_cursor_move (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GError) error = NULL;
  GValue value = G_VALUE_INIT;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  active INTEGER NOT NULL DEFAULT 0, "
                     "  score REAL NOT NULL DEFAULT 0, "
                     "  note TEXT"
                     ");"
                     "INSERT INTO items (name, active, score, note) VALUES "
                     "('alpha', 1, 3.5, 'first'), "
                     "('beta', 0, 2.25, NULL), "
                     "('gamma', 1, 7.75, 'third')"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, "items");
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_cmpuint (gom_cursor_get_n_columns (cursor), ==, 5);
  g_assert_cmpstr (gom_cursor_get_column_name (cursor, 0), ==, "id");
  g_assert_null (gom_cursor_get_column_name (cursor, 99));
  g_assert_cmpint (gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_ABSOLUTE, !=, 0);
  g_assert_cmpint (gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_RELATIVE, !=, 0);
  g_assert_cmpint (gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_REWIND, !=, 0);
  g_assert_false (gom_cursor_get_column_by_name (cursor, "missing", &value));

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "alpha");
  g_assert_true (gom_cursor_get_column_boolean (cursor, 2));
  g_assert_cmpfloat (gom_cursor_get_column_double (cursor, 3), ==, 3.5);
  g_assert_false (gom_cursor_get_column_null (cursor, 4));
  g_assert_true (gom_cursor_get_column_by_name (cursor, "name", &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "alpha");
  g_value_unset (&value);

  g_assert_true (dex_await_boolean (gom_cursor_move_relative (cursor, 0), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "alpha");

  g_assert_true (dex_await_boolean (gom_cursor_move_absolute (cursor, 2), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "gamma");

  g_assert_false (dex_await_boolean (gom_cursor_move_absolute (cursor, G_MAXUINT64), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);

  g_assert_true (dex_await_boolean (gom_cursor_move_relative (cursor, -1), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "beta");
  g_assert_false (gom_cursor_get_column_boolean (cursor, 2));
  g_assert_cmpfloat (gom_cursor_get_column_double (cursor, 3), ==, 2.25);
  g_assert_true (gom_cursor_get_column_null (cursor, 4));

  g_assert_true (dex_await (gom_cursor_rewind (cursor), &error));
  g_assert_no_error (error);
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "alpha");
  g_assert_null (gom_cursor_dup_column_bytes (cursor, 3));

  g_assert_false (dex_await_boolean (gom_cursor_move_relative (cursor, 42), &error));
  g_assert_no_error (error);

  g_assert_false (dex_await_boolean (gom_cursor_move_relative (cursor, -42), &error));
  g_assert_no_error (error);
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "alpha");

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
  g_assert_cmpuint (gom_cursor_get_n_columns (cursor), ==, 0);
  g_assert_null (gom_cursor_get_column_name (cursor, 0));
  g_assert_cmpuint (gom_cursor_get_capabilities (cursor), ==, GOM_CURSOR_CAPABILITIES_NONE);

  g_assert_false (dex_await_boolean (gom_cursor_move_absolute (cursor, 0), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  g_clear_object (&cursor);
  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await (gom_cursor_exhaust (cursor), &error));
  g_assert_no_error (error);
  g_assert_false (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
}

static void
test_sqlite_repository_query_invalid_entity_field (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  GValue value = G_VALUE_INIT;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "alpha");

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, test_insert_item_get_type ());
  gom_query_builder_set_filter (builder,
                                gom_binary_expression_new_equal (gom_field_expression_new ("does_not_exist"),
                                                                 gom_literal_expression_new (&value)));
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_null (cursor);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_nonnull (strstr (error->message, "does_not_exist"));

  g_value_unset (&value);
}

static void
test_sqlite_repository_query_unregistered_entity_type (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_sqlite_create_migration_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, test_insert_item_get_type ());
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_null (cursor);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_nonnull (strstr (error->message, "not in repository registry"));

}

static void
test_sqlite_repository_mutate_invalid_entity_field (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomUpdateBuilder) update_builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  GValue value = G_VALUE_INIT;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "updated");

  update_builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (update_builder, test_insert_item_get_type ());
  gom_update_builder_add_assignment (update_builder,
                                     gom_field_expression_new ("does_not_exist"),
                                     gom_literal_expression_new (&value));
  update = gom_update_builder_build (update_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (update);

  cursor = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (update)), &error);
  g_assert_null (cursor);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_nonnull (strstr (error->message, "does_not_exist"));

  g_value_unset (&value);
}

static void
test_sqlite_cursor_materialize (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;
  g_autoptr(GDateTime) updated_when = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *when_str = NULL;
  g_autofree char *updated_when_str = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  const char *payload = "blob-data";
  const char *payload2 = "blob-data-2";
  const char *iso8601 = "2024-01-02T03:04:05Z";
  const char *iso8601_2 = "2024-02-03T04:05:06Z";
  g_autofree char *name = NULL;
  g_autofree char *payload_out = NULL;
  g_autoptr(GDateTime) when = NULL;
  gint count = 0;
  gint64 big_count = 0;
  guint ucount = 0;
  gboolean flag = FALSE;
  double ratio = 0.0;
  float fvalue = 0.0f;
  TestMaterializeMode mode = TEST_MATERIALIZE_MODE_ALPHA;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE materialize_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB, "
                     "  \"when\" TEXT NOT NULL, "
                     "  count TEXT NOT NULL, "
                     "  big_count INTEGER NOT NULL, "
                     "  ucount INTEGER NOT NULL, "
                     "  flag INTEGER NOT NULL, "
                     "  ratio REAL NOT NULL, "
                     "  fvalue REAL NOT NULL, "
                     "  mode INTEGER NOT NULL"
                     ")"
  );

  rc = sqlite3_prepare_v2 (db,
                           "INSERT INTO materialize_items "
                           "(id, name, payload, \"when\", count, big_count, ucount, flag, ratio, fvalue, mode) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  sqlite3_bind_int (stmt, 1, 1);
  sqlite3_bind_text (stmt, 2, "alpha", -1, SQLITE_STATIC);
  sqlite3_bind_blob (stmt, 3, payload, strlen (payload), SQLITE_STATIC);
  sqlite3_bind_text (stmt, 4, iso8601, -1, SQLITE_STATIC);
  sqlite3_bind_text (stmt, 5, "42", -1, SQLITE_STATIC);
  sqlite3_bind_int64 (stmt, 6, 1234567890123LL);
  sqlite3_bind_int (stmt, 7, 7);
  sqlite3_bind_int (stmt, 8, 1);
  sqlite3_bind_double (stmt, 9, 3.14159);
  sqlite3_bind_double (stmt, 10, 2.5);
  sqlite3_bind_int (stmt, 11, TEST_MATERIALIZE_MODE_BETA);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_DONE);
  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);

  sqlite3_bind_int (stmt, 1, 2);
  sqlite3_bind_text (stmt, 2, "beta", -1, SQLITE_STATIC);
  sqlite3_bind_blob (stmt, 3, payload2, strlen (payload2), SQLITE_STATIC);
  sqlite3_bind_text (stmt, 4, iso8601_2, -1, SQLITE_STATIC);
  sqlite3_bind_text (stmt, 5, "7", -1, SQLITE_STATIC);
  sqlite3_bind_int64 (stmt, 6, 9876543210LL);
  sqlite3_bind_int (stmt, 7, 9);
  sqlite3_bind_int (stmt, 8, 0);
  sqlite3_bind_double (stmt, 9, 6.25);
  sqlite3_bind_double (stmt, 10, 1.25);
  sqlite3_bind_int (stmt, 11, TEST_MATERIALIZE_MODE_ALPHA);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_DONE);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_materialize_item_get_type ());
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  entity = gom_cursor_materialize (cursor, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (entity));
  g_assert_cmpint (gom_entity_get_origin (entity), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);
  g_assert_cmpint (gom_entity_get_lifecycle (entity), ==, GOM_ENTITY_LIFECYCLE_DETACHED);

  g_object_get (entity,
                "name", &name,
                "payload", &payload_out,
                "when", &when,
                "count", &count,
                "big-count", &big_count,
                "ucount", &ucount,
                "flag", &flag,
                "ratio", &ratio,
                "fvalue", &fvalue,
                "mode", &mode,
                NULL);

  g_assert_cmpstr (name, ==, "alpha");
  g_assert_cmpstr (payload_out, ==, payload);
  g_assert_cmpint (count, ==, 42);
  g_assert_true (big_count == 1234567890123LL);
  g_assert_cmpuint (ucount, ==, 7);
  g_assert_true (flag);
  g_assert_cmpfloat (ratio, ==, 3.14159);
  g_assert_cmpfloat (fvalue, ==, 2.5f);
  g_assert_cmpint (mode, ==, TEST_MATERIALIZE_MODE_BETA);

  g_assert_nonnull (when);
  when_str = g_date_time_format_iso8601 (when);
  g_assert_cmpstr (when_str, ==, iso8601);

  g_object_set (entity,
                "internal", "hidden-1",
                NULL);
  delta = gom_entity_build_delta (entity, &error);
  g_assert_no_error (error);
  g_assert_null (delta);

  g_object_set (entity,
                "name", "alpha-updated",
                "payload", "blob-data-updated",
                "internal", "hidden-2",
                NULL);
  updated_when = g_date_time_new_from_iso8601 ("2024-01-02T03:04:06Z", NULL);
  g_assert_nonnull (updated_when);
  updated_when_str = g_date_time_format_iso8601 (updated_when);
  g_object_set (entity,
                "when", updated_when,
                NULL);

  delta = gom_entity_build_delta (entity, &error);
  g_assert_no_error (error);
  g_assert_nonnull (delta);
  g_assert_cmpuint (gom_delta_get_kind (delta), ==, GOM_DELTA_KIND_UPDATE);
  g_assert_cmpuint (gom_delta_get_entity_type (delta), ==, test_materialize_item_get_type ());
  g_assert_cmpuint (gom_delta_get_n_changes (delta), ==, 3);
  g_assert_cmpstr (gom_delta_get_property_name (delta, 0), ==, "name");
  g_assert_cmpstr (gom_delta_get_property_name (delta, 1), ==, "payload");
  g_assert_cmpstr (gom_delta_get_property_name (delta, 2), ==, "when");

  {
    GValue value = G_VALUE_INIT;
    g_autofree char *formatted = NULL;

    g_value_init (&value, G_TYPE_STRING);
    g_assert_true (gom_delta_get_original_value (delta, 0, &value));
    g_assert_cmpstr (g_value_get_string (&value), ==, "alpha");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_assert_true (gom_delta_get_current_value (delta, 0, &value));
    g_assert_cmpstr (g_value_get_string (&value), ==, "alpha-updated");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_assert_true (gom_delta_get_original_value (delta, 1, &value));
    g_assert_cmpstr (g_value_get_string (&value), ==, payload);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_assert_true (gom_delta_get_current_value (delta, 1, &value));
    g_assert_cmpstr (g_value_get_string (&value), ==, "blob-data-updated");
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_DATE_TIME);
    g_assert_true (gom_delta_get_original_value (delta, 2, &value));
    formatted = g_date_time_format_iso8601 (g_value_get_boxed (&value));
    g_assert_cmpstr (formatted, ==, when_str);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_DATE_TIME);
    g_assert_true (gom_delta_get_current_value (delta, 2, &value));
    formatted = g_date_time_format_iso8601 (g_value_get_boxed (&value));
    g_assert_cmpstr (formatted, ==, updated_when_str);
    g_value_unset (&value);
  }

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);

  g_clear_object (&cursor);
  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  {
    g_autoptr(GListStore) list = NULL;
    g_autoptr(GomEntity) first = NULL;
    g_autoptr(GomEntity) second = NULL;
    g_autofree char *first_name = NULL;
    g_autofree char *second_name = NULL;

    list = dex_await_object (gom_cursor_exhaust_to_list (cursor), &error);
    g_assert_no_error (error);
    g_assert_true (G_IS_LIST_STORE (list));
    g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (list)), ==, 2);

    first = g_list_model_get_item (G_LIST_MODEL (list), 0);
    second = g_list_model_get_item (G_LIST_MODEL (list), 1);
    g_assert_true (GOM_IS_ENTITY (first));
    g_assert_true (GOM_IS_ENTITY (second));

    g_object_get (first,
                  "name", &first_name,
                  NULL);
    g_object_get (second,
                  "name", &second_name,
                  NULL);
    g_assert_cmpstr (first_name, ==, "alpha");
    g_assert_cmpstr (second_name, ==, "beta");
  }

  g_assert_false (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

}

static void
test_sqlite_session_identity_map (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GListStore) list = NULL;
  g_autoptr(GomEntity) first = NULL;
  g_autoptr(GomEntity) second = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  const char *payload = "blob-data";
  const char *input_name = "alpha";
  g_autofree char *materialized_name = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );

  rc = sqlite3_prepare_v2 (db,
                           "INSERT INTO entity_items "
                           "(id, name, payload) "
                           "VALUES (?, ?, ?)",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  sqlite3_bind_int (stmt, 1, 1);
  sqlite3_bind_text (stmt, 2, input_name, -1, SQLITE_STATIC);
  sqlite3_bind_blob (stmt, 3, payload, strlen (payload), SQLITE_STATIC);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_DONE);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_SESSION (session));

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_crud_item_get_type ());
  gom_query_builder_add_ordering (query_builder,
                                  gom_ordering_new (gom_field_expression_new ("id"),
                                                    GOM_SORT_ASCENDING));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_session_query (session, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  first = gom_cursor_materialize (cursor, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (first));
  g_assert_cmpint (gom_entity_get_origin (first), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);
  g_assert_cmpint (gom_entity_get_lifecycle (first), ==, GOM_ENTITY_LIFECYCLE_PERSISTENT);
  g_object_set (first,
                "name", "beta",
                NULL);

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
  g_clear_object (&cursor);

  cursor = dex_await_object (gom_session_query (session, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  second = gom_cursor_materialize (cursor, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (second));
  g_assert_cmpint (gom_entity_get_origin (second), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);
  g_assert_cmpint (gom_entity_get_lifecycle (second), ==, GOM_ENTITY_LIFECYCLE_PERSISTENT);

  g_assert_true (first == second);
  g_object_get (second,
                "name", &materialized_name,
                NULL);
  g_assert_cmpstr (materialized_name, ==, "beta");

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);

  list = dex_await_object (gom_session_list_entities (session,
                                                      test_crud_item_get_type (),
                                                      NULL,
                                                      NULL),
                           &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_LIST_STORE (list));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (list)), ==, 1);

  {
    g_autoptr(GomEntity) listed = g_list_model_get_item (G_LIST_MODEL (list), 0);

    g_assert_true (listed == first);
    g_assert_cmpint (gom_entity_get_origin (listed), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);
    g_assert_cmpint (gom_entity_get_lifecycle (listed), ==, GOM_ENTITY_LIFECYCLE_PERSISTENT);
  }
}

static void
test_sqlite_session_rekey_identity_map (void)
{
  static const char blob_data[] = "blob-data";
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomEntity) reloaded = NULL;
  g_autoptr(GomMutationResult) update_result = NULL;
  g_autoptr(GomUpdateBuilder) update_builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomQueryBuilder) requery_builder = NULL;
  g_autoptr(GomQuery) requery = NULL;
  g_autoptr(GomCursor) requery_cursor = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GError) error = NULL;
  g_auto(GValue) old_id_value = G_VALUE_INIT;
  g_auto(GValue) new_id_value = G_VALUE_INIT;
  g_auto(GValue) id_value = G_VALUE_INIT;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );

  rc = sqlite3_prepare_v2 (db,
                           "INSERT INTO entity_items (id, name, payload) VALUES (?, ?, ?)",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  sqlite3_bind_int (stmt, 1, 1);
  sqlite3_bind_text (stmt, 2, "alpha", -1, SQLITE_STATIC);
  sqlite3_bind_blob (stmt, 3, blob_data, strlen (blob_data), SQLITE_STATIC);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_DONE);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_SESSION (session));

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_crud_item_get_type ());
  gom_query_builder_add_ordering (query_builder,
                                  gom_ordering_new (gom_field_expression_new ("id"),
                                                    GOM_SORT_ASCENDING));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_session_query (session, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  entity = gom_cursor_materialize (cursor, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (entity));
  g_assert_cmpint (gom_entity_get_origin (entity), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);
  g_assert_cmpint (gom_entity_get_lifecycle (entity), ==, GOM_ENTITY_LIFECYCLE_PERSISTENT);

  g_object_set (entity,
                "id", (gint64) 2,
                NULL);
  g_assert_true (gom_entity_rekey_session_identity (entity));

  g_value_init (&old_id_value, G_TYPE_INT64);
  g_value_set_int64 (&old_id_value, 1);
  g_value_init (&new_id_value, G_TYPE_INT64);
  g_value_set_int64 (&new_id_value, 2);

  update_builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (update_builder, test_crud_item_get_type ());
  gom_update_builder_add_assignment (update_builder,
                                     gom_field_expression_new ("id"),
                                     gom_literal_expression_new (&new_id_value));
  gom_update_builder_set_filter (update_builder,
                                 gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                                                  gom_literal_expression_new (&old_id_value)));
  update = gom_update_builder_build (update_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (update);

  update_result = dex_await_object (gom_session_mutate (session, GOM_MUTATION (update)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (update_result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (update_result), ==, 1);

  g_value_init (&id_value, G_TYPE_INT64);
  g_value_set_int64 (&id_value, 2);
  filter = gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                            gom_literal_expression_new (&id_value));

  requery_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (requery_builder, test_crud_item_get_type ());
  gom_query_builder_set_filter (requery_builder, g_steal_pointer (&filter));
  requery = gom_query_builder_build (requery_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (requery);

  requery_cursor = dex_await_object (gom_session_query (session, requery), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (requery_cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (requery_cursor), &error));
  g_assert_no_error (error);

  reloaded = gom_cursor_materialize (requery_cursor, &error);
  g_assert_no_error (error);
  g_assert_true (reloaded == entity);
}

static void
test_sqlite_session_persist_flush_commit (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomEntity) first = NULL;
  g_autoptr(GomEntity) second = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gint64 first_id = 0;
  gint64 second_id = 0;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  session = dex_await_object (gom_repository_begin_session (repository), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_SESSION (session));

  first = g_object_new (test_crud_item_get_type (),
                        "name", "first",
                        "payload", "payload-first",
                        NULL);
  gom_entity_set_repository (first, repository);

  g_assert_true (dex_await (gom_session_persist (session, GOM_ENTITY (first)), &error));
  g_assert_no_error (error);
  g_assert_cmpint (gom_entity_get_lifecycle (first), ==, GOM_ENTITY_LIFECYCLE_PENDING);

  g_assert_true (dex_await (gom_session_flush (session), &error));
  g_assert_no_error (error);
  g_assert_cmpint (gom_entity_get_lifecycle (first), ==, GOM_ENTITY_LIFECYCLE_PERSISTENT);
  g_object_get (first,
                "id", &first_id,
                NULL);
  g_assert_cmpint (first_id, >, 0);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_crud_item_get_type ());
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_session_query (session, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  {
    g_autoptr(GomEntity) listed = gom_cursor_materialize (cursor, &error);

    g_assert_no_error (error);
    g_assert_true (listed == first);
  }

  g_assert_false (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
  g_clear_object (&cursor);

  second = g_object_new (test_crud_item_get_type (),
                         "name", "second",
                         "payload", "payload-second",
                         NULL);
  gom_entity_set_repository (second, repository);

  g_assert_true (dex_await (gom_session_persist (session, GOM_ENTITY (second)), &error));
  g_assert_no_error (error);
  g_assert_cmpint (gom_entity_get_lifecycle (second), ==, GOM_ENTITY_LIFECYCLE_PENDING);

  if (!dex_await (gom_session_commit (session), &error))
    g_printerr ("session commit failed: %s\n", error->message);
  g_assert_no_error (error);
  g_assert_cmpint (gom_entity_get_lifecycle (second), ==, GOM_ENTITY_LIFECYCLE_DETACHED);
  g_object_get (second,
                "id", &second_id,
                NULL);
  g_assert_cmpint (second_id, >, 0);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT COUNT(*) FROM entity_items",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_int (stmt, 0), ==, 2);
  sqlite3_finalize (stmt);
  stmt = NULL;

  rc = sqlite3_prepare_v2 (db,
                           "SELECT name FROM entity_items WHERE id = ?",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_int64 (stmt, 1, first_id);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "first");
  sqlite3_finalize (stmt);
  stmt = NULL;

  rc = sqlite3_prepare_v2 (db,
                           "SELECT name FROM entity_items WHERE id = ?",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_int64 (stmt, 1, second_id);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "second");
  sqlite3_finalize (stmt);
  stmt = NULL;

  test_sqlite_close (db);
  db = NULL;
}

static void
test_sqlite_repository_update_delete (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomUpdateBuilder) update_builder = NULL;
  g_autoptr(GomDeletionBuilder) deletion_builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT NOT NULL"
                     ");"
                     "INSERT INTO items (name, category) VALUES "
                     "  ('alpha', 'first'),"
                     "  ('beta', 'second'),"
                     "  ('gamma', 'third');"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  update_builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (update_builder, test_insert_item_get_type ());
  {
    g_auto(GValue) value = G_VALUE_INIT;
    g_auto(GValue) filter_value = G_VALUE_INIT;

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, "updated");
    gom_update_builder_add_assignment (update_builder,
                                       gom_field_expression_new ("category"),
                                       gom_literal_expression_new (&value));

    g_value_init (&filter_value, G_TYPE_STRING);
    g_value_set_string (&filter_value, "alpha");
    gom_update_builder_set_filter (update_builder,
                                   gom_binary_expression_new_equal (gom_field_expression_new ("name"),
                                                                    gom_literal_expression_new (&filter_value)));
  }
  gom_update_builder_set_limit (update_builder, 1);

  update = gom_update_builder_build (update_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (update);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (update)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, FALSE);
  g_clear_object (&result);
  deletion_builder = gom_deletion_builder_new ();
  gom_deletion_builder_set_target_relation (deletion_builder, "items");
  {
    g_auto(GValue) value = G_VALUE_INIT;

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, "beta");
    gom_deletion_builder_set_filter (deletion_builder,
                                     gom_binary_expression_new_equal (gom_field_expression_new ("name"),
                                                                      gom_literal_expression_new (&value)));
  }
  gom_deletion_builder_set_limit (deletion_builder, 1);

  deletion = gom_deletion_builder_build (deletion_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (deletion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (deletion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, FALSE);
  g_clear_object (&result);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, "items");
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  g_clear_object (&cursor);
  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "alpha");
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 2), ==, "updated");

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, "gamma");
  g_assert_cmpstr (gom_cursor_get_column_string (cursor, 2), ==, "third");

  g_assert_false (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);

}

static void
test_sqlite_entity_crud (void)
{
  static const char payload_b[] = "payload-b";
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) inserted = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomEntity) materialized = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gint64 id = 0;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  inserted = g_object_new (test_crud_item_get_type (),
                           "name", "alpha",
                           "payload", "payload-a",
                           NULL);
  gom_entity_set_repository (inserted, repository);

  result = dex_await_object (gom_entity_insert (inserted), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, TRUE);
  g_object_get (inserted,
                "id", &id,
                NULL);
  g_assert_cmpint ((gint) id, >, 0);
  g_assert_cmpint (gom_entity_get_origin (inserted), ==, GOM_ENTITY_ORIGIN_CONSTRUCTED);
  g_assert_cmpint (gom_entity_get_lifecycle (inserted), ==, GOM_ENTITY_LIFECYCLE_TRANSIENT);
  g_clear_object (&result);

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (query_builder, test_crud_item_get_type ());
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  materialized = gom_cursor_materialize (cursor, &error);
  g_assert_no_error (error);
  g_assert_nonnull (materialized);
  g_assert_cmpint (gom_entity_get_origin (materialized), ==, GOM_ENTITY_ORIGIN_MATERIALIZED);
  g_assert_cmpint (gom_entity_get_lifecycle (materialized), ==, GOM_ENTITY_LIFECYCLE_DETACHED);
  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);
  g_clear_object (&cursor);

  g_object_set (materialized,
                "name", "beta",
                "payload", payload_b,
                NULL);

  result = dex_await_object (gom_entity_update (materialized), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, FALSE);
  g_clear_object (&result);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT name, payload FROM entity_items WHERE id = ?",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_int64 (stmt, 1, id);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "beta");
  g_assert_cmpint (sqlite3_column_bytes (stmt, 1), ==, strlen (payload_b));
  g_assert_cmpmem (sqlite3_column_blob (stmt, 1),
                   sqlite3_column_bytes (stmt, 1),
                   payload_b,
                   strlen (payload_b));
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  result = dex_await_object (gom_entity_delete (materialized), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, FALSE);
  g_clear_object (&result);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT COUNT(*) FROM entity_items WHERE id = ?",
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
test_sqlite_entity_crud_errors (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) no_identity = NULL;
  g_autoptr(GomEntity) missing_identity_value = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ");"
                     "CREATE TABLE no_identity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  no_identity = g_object_new (test_no_identity_item_get_type (),
                              "id", 1,
                              "name", "alpha",
                              NULL);
  gom_entity_set_repository (no_identity, repository);

  g_assert_false (dex_await (gom_entity_update (no_identity), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_nonnull (strstr (error->message, "identity fields"));
  g_clear_error (&error);

  missing_identity_value = g_object_new (test_crud_item_get_type (),
                                         "name", "alpha",
                                         "payload", "payload-a",
                                         NULL);
  gom_entity_set_repository (missing_identity_value, repository);

  g_assert_false (dex_await (gom_entity_delete (missing_identity_value), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_nonnull (strstr (error->message, "identity field 'id' is unset"));
  g_clear_error (&error);

}

static void
test_sqlite_entity_insert_unsupported_property_transform (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE unsupported_transform_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  opaque BLOB"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_unsupported_transform_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_unsupported_transform_item_get_type (),
                         "id", 1,
                         "opaque", g_variant_new_string ("alpha"),
                         NULL);
  gom_entity_set_repository (entity, repository);

  g_assert_false (dex_await (gom_entity_insert (entity), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_nonnull (strstr (error->message, "Unsupported SQLite binding type"));
  g_clear_error (&error);

}

static void
test_sqlite_entity_default_identity_override (void)
{
  static const char payload_b[] = "payload-b";
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gint64 id = 0;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_allow_default_identity_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_crud_allow_default_id_item_get_type (),
                         "id", (gint64) 0,
                         "name", "alpha",
                         "payload", "payload-a",
                         NULL);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_object_get (entity,
                "id", &id,
                NULL);
  g_assert_cmpint (id, ==, 0);
  g_assert_cmpint (gom_entity_get_origin (entity), ==, GOM_ENTITY_ORIGIN_CONSTRUCTED);
  g_assert_cmpint (gom_entity_get_lifecycle (entity), ==, GOM_ENTITY_LIFECYCLE_TRANSIENT);
  g_clear_object (&result);

  g_object_set (entity,
                "name", "beta",
                "payload", payload_b,
                NULL);

  result = dex_await_object (gom_entity_update (entity), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, FALSE);
  g_clear_object (&result);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT name, payload FROM entity_items WHERE id = ?",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_int64 (stmt, 1, id);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 0), ==, "beta");
  g_assert_cmpint (sqlite3_column_bytes (stmt, 1), ==, strlen (payload_b));
  g_assert_cmpmem (sqlite3_column_blob (stmt, 1),
                   sqlite3_column_bytes (stmt, 1),
                   payload_b,
                   strlen (payload_b));
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  result = dex_await_object (gom_entity_delete (entity), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  test_sqlite_assert_mutation_result_row (result, 0, 1, FALSE);
  g_clear_object (&result);

  rc = sqlite3_open (context.db_path, &db);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT COUNT(*) FROM entity_items WHERE id = ?",
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
test_sqlite_repository_insert_omits_default_identity (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomInsertionBuilder) insert_builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  gint64 id = 0;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE entity_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  payload BLOB"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  entity = g_object_new (test_crud_item_get_type (),
                         "name", "alpha",
                         "payload", "payload-a",
                         NULL);

  insert_builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_relation (insert_builder, "entity_items");
  g_assert_true (gom_insertion_builder_add_entity (insert_builder, entity, &error));
  g_assert_no_error (error);

  insertion = gom_insertion_builder_build (insert_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (insertion);

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_MUTATION_RESULT (result));
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  test_sqlite_open (context.db_path, &db);
  rc = sqlite3_prepare_v2 (db,
                           "SELECT id FROM entity_items WHERE name = ?",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  sqlite3_bind_text (stmt, 1, "alpha", -1, SQLITE_STATIC);
  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  id = sqlite3_column_int64 (stmt, 0);
  g_assert_cmpint (id, >, 0);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;
}

static guint
test_sqlite_search_query_count (GomRepository *repository,
                                const char    *query,
                                GomSearchMode  mode)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) gom_query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomExpression) filter = NULL;
  guint count = 0;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, "items");

  filter = gom_search_expression_new_for_field ("name", query, mode);
  gom_query_builder_set_filter (builder, filter);

  gom_query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (gom_query);

  cursor = dex_await_object (gom_repository_query (repository, gom_query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  while (TRUE)
    {
      gboolean has_row = dex_await_boolean (gom_cursor_next (cursor), &error);

      g_assert_no_error (error);

      if (!has_row)
        break;

      count++;
    }

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);

  return count;
}

static guint
test_sqlite_query_count_for_filter (GomRepository  *repository,
                                    const char     *relation,
                                    GomExpression  *filter,
                                    GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) gom_query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  guint count = 0;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, relation);
  gom_query_builder_set_filter (builder, filter);

  gom_query = gom_query_builder_build (builder, error);
  if (gom_query == NULL)
    return 0;

  cursor = dex_await_object (gom_repository_query (repository, gom_query), error);
  if (cursor == NULL)
    return 0;

  while (TRUE)
    {
      gboolean has_row = dex_await_boolean (gom_cursor_next (cursor), error);

      if (*error != NULL)
        return 0;

      if (!has_row)
        break;

      count++;
    }

  dex_await (gom_cursor_close (cursor), error);
  if (*error != NULL)
    return 0;

  return count;
}

static void
test_sqlite_repository_expression_variants (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  gchar *errmsg = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);

  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                       "CREATE TABLE expr_items ("
                       "  id INTEGER PRIMARY KEY, "
                       "  a INTEGER NOT NULL, "
                       "  b REAL NOT NULL, "
                       "  name TEXT, "
                       "  payload BLOB"
                       ");"
                       "INSERT INTO expr_items (a, b, name, payload) VALUES "
                       "(1, 1.0, 'alpha', X'6161'), "
                       "(2, 2.5, 'beta', NULL), "
                       "(3, 3.0, 'gamma', X'6262')");

  rc = sqlite3_exec (db,
                     "CREATE VIRTUAL TABLE expr_items_fts USING fts5 (name)",
                     NULL, NULL, &errmsg);
  if (rc == SQLITE_OK)
    {
      test_sqlite_exec_ok (db,
                           "INSERT INTO expr_items_fts (rowid, name) VALUES "
                           "(1, 'alpha'), (2, 'beta'), (3, 'gamma')");
    }
  g_clear_pointer (&errmsg, sqlite3_free);
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  {
    typedef GomExpression *(*BinaryCtor) (GomExpression *left,
                                          GomExpression *right);
    static const BinaryCtor ctors[] = {
      gom_binary_expression_new_add,
      gom_binary_expression_new_subtract,
      gom_binary_expression_new_multiply,
      gom_binary_expression_new_divide,
      gom_binary_expression_new_modulo,
      gom_binary_expression_new_equal,
      gom_binary_expression_new_not_equal,
      gom_binary_expression_new_less_than,
      gom_binary_expression_new_less_equal,
      gom_binary_expression_new_greater_than,
      gom_binary_expression_new_greater_equal,
      gom_binary_expression_new_and,
      gom_binary_expression_new_or,
      gom_binary_expression_new_like,
    };

    for (guint i = 0; i < G_N_ELEMENTS (ctors); i++)
      {
        g_autoptr(GomExpression) filter = NULL;
        guint count = 0;

        if (ctors[i] == gom_binary_expression_new_like)
          {
            GValue pattern = G_VALUE_INIT;
            g_value_init (&pattern, G_TYPE_STRING);
            g_value_set_string (&pattern, "%a%");
            filter = ctors[i] (gom_field_expression_new ("name"),
                               gom_literal_expression_new (&pattern));
            g_value_unset (&pattern);
          }
        else if (ctors[i] == gom_binary_expression_new_and ||
                 ctors[i] == gom_binary_expression_new_or)
          {
            GValue one = G_VALUE_INIT;
            GValue two = G_VALUE_INIT;
            g_value_init (&one, G_TYPE_INT);
            g_value_init (&two, G_TYPE_INT);
            g_value_set_int (&one, 1);
            g_value_set_int (&two, 1);
            filter = ctors[i] (gom_literal_expression_new (&one),
                               gom_literal_expression_new (&two));
            g_value_unset (&one);
            g_value_unset (&two);
          }
        else
          {
            GValue rhs = G_VALUE_INIT;
            g_value_init (&rhs, G_TYPE_INT);
            g_value_set_int (&rhs, 1);
            filter = ctors[i] (gom_field_expression_new ("a"),
                               gom_literal_expression_new (&rhs));
            g_value_unset (&rhs);
          }

        count = test_sqlite_query_count_for_filter (repository, "expr_items", filter, &error);
        g_assert_no_error (error);
        g_assert_cmpuint (count, <=, 3);
      }
  }

  {
    g_autoptr(GomExpression) negate = NULL;
    g_autoptr(GomExpression) logical_not = NULL;
    GValue one = G_VALUE_INIT;

    g_value_init (&one, G_TYPE_INT);
    g_value_set_int (&one, 1);

    negate = gom_unary_expression_new_negate (gom_literal_expression_new (&one));
    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items", negate, &error), <=, 3);
    g_assert_no_error (error);

    logical_not = gom_unary_expression_new_not (gom_literal_expression_new (&one));
    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items", logical_not, &error), <=, 3);
    g_assert_no_error (error);

    g_value_unset (&one);
  }

  {
    g_autoptr(GomExpression) func_arg0 = NULL;
    g_autoptr(GomExpression) func_arg1 = NULL;
    GomExpression *func_args[2];
    g_autoptr(GomExpression) filter = NULL;
    GValue one = G_VALUE_INIT;
    GValue two = G_VALUE_INIT;

    g_value_init (&one, G_TYPE_INT);
    g_value_init (&two, G_TYPE_INT);
    g_value_set_int (&one, 1);
    g_value_set_int (&two, 2);
    func_arg0 = gom_literal_expression_new (&one);
    func_arg1 = gom_literal_expression_new (&two);
    func_args[0] = func_arg0;
    func_args[1] = func_arg1;
    filter = gom_binary_expression_new_greater_than (gom_function_expression_new ("max",
                                                                                   func_args,
                                                                                   2),
                                                      gom_literal_expression_new (&one));

    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository,
                                                          "expr_items",
                                                          filter,
                                                          &error),
                      <=, 3);
    g_assert_no_error (error);

    g_value_unset (&one);
    g_value_unset (&two);
  }

  if (rc == SQLITE_OK)
    {
      g_assert_cmpuint (test_sqlite_query_count_for_filter (repository,
                                                            "expr_items_fts",
                                                            gom_search_expression_new_for_field ("name", "alp", GOM_SEARCH_MODE_PREFIX),
                                                            &error),
                        ==, 1);
      g_assert_no_error (error);

      g_assert_cmpuint (test_sqlite_query_count_for_filter (repository,
                                                            "expr_items_fts",
                                                            gom_search_expression_new_for_field ("name", "alpha", GOM_SEARCH_MODE_NATURAL),
                                                            &error),
                        ==, 1);
      g_assert_no_error (error);

      g_assert_cmpuint (test_sqlite_query_count_for_filter (repository,
                                                            "expr_items_fts",
                                                            gom_search_expression_new_for_field ("name", "alpha", GOM_SEARCH_MODE_PHRASE),
                                                            &error),
                        ==, 1);
      g_assert_no_error (error);
    }

  {
    g_autoptr(GomExpression) bad = NULL;
    GValue null_string = G_VALUE_INIT;

    g_value_init (&null_string, G_TYPE_STRING);
    g_value_set_string (&null_string, NULL);
    bad = gom_search_expression_new (gom_field_expression_new ("name"),
                                     gom_literal_expression_new (&null_string),
                                     GOM_SEARCH_MODE_NATURAL);
    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items_fts", bad, &error), ==, 0);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error (&error);
    g_value_unset (&null_string);
  }

  {
    g_autoptr(GomExpression) bad_mode = NULL;

    bad_mode = gom_search_expression_new (gom_field_expression_new ("name"),
                                          gom_field_expression_new ("name"),
                                          GOM_SEARCH_MODE_PREFIX);
    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items_fts", bad_mode, &error), ==, 0);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error (&error);
  }

  {
    g_autoptr(GomExpression) is_null = NULL;
    g_autoptr(GomExpression) is_not_null = NULL;
    GValue null_string = G_VALUE_INIT;

    g_value_init (&null_string, G_TYPE_STRING);
    g_value_set_string (&null_string, NULL);

    is_null = gom_binary_expression_new_equal (gom_field_expression_new ("payload"),
                                               gom_literal_expression_new (&null_string));
    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items", is_null, &error), ==, 1);
    g_assert_no_error (error);

    is_not_null = gom_binary_expression_new_not_equal (gom_field_expression_new ("payload"),
                                                       gom_literal_expression_new (&null_string));
    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items", is_not_null, &error), ==, 2);
    g_assert_no_error (error);

    g_value_unset (&null_string);
  }

  {
    g_autoptr(GomExpression) bad_field = gom_field_expression_new ("");

    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items", bad_field, &error), ==, 0);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error (&error);
  }

  {
    g_autoptr(GomExpression) bad_func = gom_function_expression_new ("", NULL, 0);

    g_assert_cmpuint (test_sqlite_query_count_for_filter (repository, "expr_items", bad_func, &error), ==, 0);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
    g_clear_error (&error);
  }
}

static GomFieldSchema *
test_find_field_schema (GListModel *fields,
                        const char *name)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (fields), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (guint i = 0, n = g_list_model_get_n_items (fields); i < n; i++)
    {
      g_autoptr(GomFieldSchema) field = g_list_model_get_item (fields, i);
      const char *field_name = gom_schema_get_name (GOM_SCHEMA (field));

      if (g_strcmp0 (field_name, name) == 0)
        return g_steal_pointer (&field);
    }

  return NULL;
}

static GomIndexSchema *
test_find_index_schema (GListModel *indexes,
                        const char *name)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (indexes), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (guint i = 0, n = g_list_model_get_n_items (indexes); i < n; i++)
    {
      g_autoptr(GomIndexSchema) index = g_list_model_get_item (indexes, i);
      const char *index_name = gom_schema_get_name (GOM_SCHEMA (index));

      if (g_strcmp0 (index_name, name) == 0)
        return g_steal_pointer (&index);
    }

  return NULL;
}

static void
test_sqlite_repository_describe_relation (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRelationSchema) schema = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *schema_name = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT, "
                     "  count INTEGER DEFAULT 42"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "CREATE UNIQUE INDEX items_name_idx ON items (name)"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  schema = dex_await_object (gom_repository_describe_relation (repository, "items"), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_RELATION_SCHEMA (schema));
  g_assert_cmpstr (gom_schema_get_name (GOM_SCHEMA (schema)), ==, "items");
  g_object_get (schema,
                "name", &schema_name,
                NULL);
  g_assert_cmpstr (schema_name, ==, "items");

  {
    g_autoptr(GListModel) fields = gom_relation_schema_list_fields (schema);
    g_autoptr(GListModel) indexes = gom_relation_schema_list_indexes (schema);
    g_autoptr(GomFieldSchema) id_field = NULL;
    g_autoptr(GomFieldSchema) name_field = NULL;
    g_autoptr(GomFieldSchema) count_field = NULL;
    g_autoptr(GomIndexSchema) name_index = NULL;
    g_autofree char *sql_type = NULL;
    g_autofree char *default_value = NULL;
    gboolean nonnull = FALSE;
    gboolean primary_key = FALSE;
    gboolean unique = FALSE;
    GListModel *fields_model = NULL;
    GListModel *indexes_model = NULL;
    const char * const *index_fields;

    g_assert_nonnull (fields);
    g_assert_nonnull (indexes);
    g_assert_cmpuint (g_list_model_get_n_items (fields), >=, 3);

    id_field = test_find_field_schema (fields, "id");
    g_assert_nonnull (id_field);
    g_assert_true (gom_field_schema_get_primary_key (id_field));
    g_object_get (id_field,
                  "primary-key", &primary_key,
                  NULL);
    g_assert_true (primary_key);

    name_field = test_find_field_schema (fields, "name");
    g_assert_nonnull (name_field);
    g_assert_true (gom_field_schema_get_nonnull (name_field));
    g_assert_cmpstr (gom_field_schema_get_sql_type (name_field), ==, "TEXT");
    g_object_get (name_field,
                  "sql-type", &sql_type,
                  "not-null", &nonnull,
                  NULL);
    g_assert_cmpstr (sql_type, ==, "TEXT");
    g_assert_true (nonnull);

    count_field = test_find_field_schema (fields, "count");
    g_assert_nonnull (count_field);
    g_assert_cmpstr (gom_field_schema_get_default_value (count_field), ==, "42");
    g_object_get (count_field,
                  "default-value", &default_value,
                  NULL);
    g_assert_cmpstr (default_value, ==, "42");

    name_index = test_find_index_schema (indexes, "items_name_idx");
    g_assert_nonnull (name_index);
    g_assert_true (gom_index_schema_get_unique (name_index));
    g_object_get (name_index,
                  "unique", &unique,
                  "fields", &index_fields,
                  NULL);
    g_assert_true (unique);

    index_fields = gom_index_schema_get_fields (name_index);
    g_assert_nonnull (index_fields);
    g_assert_cmpstr (index_fields[0], ==, "name");

    g_object_get (schema,
                  "fields", &fields_model,
                  "indexes", &indexes_model,
                  NULL);
    g_assert_true (G_IS_LIST_MODEL (fields_model));
    g_assert_true (G_IS_LIST_MODEL (indexes_model));
    g_clear_object (&fields_model);
    g_clear_object (&indexes_model);
  }

}

static void
test_sqlite_repository_list_relations (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char **relations = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "CREATE TABLE users ("
                     "  id INTEGER PRIMARY KEY, "
                     "  username TEXT NOT NULL"
                     ")"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  relations = dex_await_boxed (gom_repository_list_relations (repository), &error);
  g_assert_no_error (error);
  g_assert_nonnull (relations);
  g_assert_true (_gom_strv_contains (relations, "items"));
  g_assert_true (_gom_strv_contains (relations, "users"));

}

static void
test_sqlite_repository_search (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  gchar *errmsg = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  category TEXT NOT NULL"
                     ")"
  );
  test_sqlite_exec_ok (db,
                       "INSERT INTO items (name, category) VALUES "
                       "('alpha beta', 'first'), "
                       "('beta gamma', 'second'), "
                       "('alphabet soup', 'third')");
  rc = sqlite3_exec (db,
                     "CREATE VIRTUAL TABLE items_fts USING fts5 ("
                     "  name, "
                     "  category"
                     ")",
                     NULL, NULL, &errmsg);
  if (rc != SQLITE_OK)
    {
      g_clear_pointer (&errmsg, sqlite3_free);
      test_sqlite_close (db);
      db = NULL;
      g_test_skip ("SQLite FTS5 not available");
      return;
    }

  test_sqlite_exec_ok (db,
                       "INSERT INTO items_fts (name, category) VALUES "
                       "('alpha beta', 'first'), "
                       "('beta gamma', 'second'), "
                       "('alphabet soup', 'third')");
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  g_assert_cmpuint (test_sqlite_search_query_count (repository,
                                                    "alpha",
                                                    GOM_SEARCH_MODE_NATURAL),
                    ==,
                    1);
  g_assert_cmpuint (test_sqlite_search_query_count (repository,
                                                    "alpha",
                                                    GOM_SEARCH_MODE_PREFIX),
                    ==,
                    2);
  g_assert_cmpuint (test_sqlite_search_query_count (repository,
                                                    "alpha beta",
                                                    GOM_SEARCH_MODE_PHRASE),
                    ==,
                    1);

}

static void
test_sqlite_cursor_snapshot (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomRecord) record = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  const char *payload = "blob-data";
  const char *name = "alpha";
  double ratio = 3.5;
  guint n_columns = 0;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE snapshot_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  ratio REAL NOT NULL, "
                     "  payload BLOB, "
                     "  note TEXT"
                     ")"
  );

  rc = sqlite3_prepare_v2 (db,
                           "INSERT INTO snapshot_items "
                           "(id, name, ratio, payload, note) "
                           "VALUES (?, ?, ?, ?, ?)",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);
  g_assert_nonnull (stmt);

  sqlite3_bind_int (stmt, 1, 1);
  sqlite3_bind_text (stmt, 2, name, -1, SQLITE_STATIC);
  sqlite3_bind_double (stmt, 3, ratio);
  sqlite3_bind_blob (stmt, 4, payload, strlen (payload), SQLITE_STATIC);
  sqlite3_bind_null (stmt, 5);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_DONE);
  sqlite3_finalize (stmt);
  stmt = NULL;
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, "snapshot_items");
  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (query_builder, g_steal_pointer (&ordering));
  query = gom_query_builder_build (query_builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_CURSOR (cursor));

  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);

  record = gom_cursor_snapshot (cursor, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_RECORD (record));

  n_columns = gom_cursor_get_n_columns (cursor);
  g_assert_cmpuint (gom_record_get_n_columns (record), ==, n_columns);

  for (guint i = 0; i < n_columns; i++)
    g_assert_cmpstr (gom_record_get_column_name (record, i), ==,
                     gom_cursor_get_column_name (cursor, i));

  {
    g_autofree char *record_name = NULL;
    g_auto(GValue) value = G_VALUE_INIT;

    g_assert_cmpint (gom_cursor_get_column_int64 (cursor, 0), ==, 1);
    g_assert_cmpint (gom_record_get_column_int64 (record, 0), ==, 1);
    g_assert_true (gom_record_get_column_boolean (record, 0));
    g_assert_null (gom_record_get_column_string (record, 0));
    g_assert_null (gom_record_dup_column_string (record, 0));
    g_assert_true (gom_record_get_column (record, 0, &value));
    g_assert_true (G_VALUE_HOLDS_INT64 (&value));
    g_assert_cmpint (g_value_get_int64 (&value), ==, 1);
    g_value_unset (&value);

    g_assert_cmpstr (gom_cursor_get_column_string (cursor, 1), ==, name);
    g_assert_cmpstr (gom_record_get_column_string (record, 1), ==, name);
    record_name = gom_record_dup_column_string (record, 1);
    g_assert_cmpstr (record_name, ==, name);
    g_assert_true (gom_record_get_column_by_name (record, "name", &value));
    g_assert_true (G_VALUE_HOLDS_STRING (&value));
    g_assert_cmpstr (g_value_get_string (&value), ==, name);
    g_value_unset (&value);

    g_assert_cmpfloat (gom_cursor_get_column_double (cursor, 2), ==, ratio);
    g_assert_cmpint (gom_record_get_column_int64 (record, 2), ==, 3);
    g_assert_true (gom_record_get_column (record, 2, &value));
    g_assert_true (G_VALUE_HOLDS_DOUBLE (&value));
    g_assert_cmpfloat (g_value_get_double (&value), ==, ratio);
    g_value_unset (&value);

    {
      g_autoptr(GBytes) cursor_bytes = NULL;
      GBytes *record_bytes = NULL;
      gsize record_size = 0;
      gsize cursor_size = 0;
      const guint8 *record_data = NULL;
      const guint8 *cursor_data = NULL;

      cursor_bytes = gom_cursor_dup_column_bytes (cursor, 3);
      g_assert_nonnull (cursor_bytes);
      cursor_data = g_bytes_get_data (cursor_bytes, &cursor_size);
      g_assert_cmpuint (cursor_size, ==, strlen (payload));
      g_assert_cmpmem (cursor_data, cursor_size, payload, strlen (payload));

      g_assert_true (gom_record_get_column (record, 3, &value));
      g_assert_true (G_VALUE_HOLDS (&value, G_TYPE_BYTES));
      record_bytes = g_value_get_boxed (&value);
      g_assert_nonnull (record_bytes);
      record_data = g_bytes_get_data (record_bytes, &record_size);
      g_assert_cmpuint (record_size, ==, strlen (payload));
      g_assert_cmpmem (record_data, record_size, payload, strlen (payload));
      g_value_unset (&value);
    }

    g_assert_true (gom_record_get_column (record, 4, &value));
    g_assert_true (G_VALUE_HOLDS_POINTER (&value));
    g_assert_null (g_value_get_pointer (&value));
    g_value_unset (&value);
  }

  dex_await (gom_cursor_close (cursor), &error);
  g_assert_no_error (error);

}

static void
test_sqlite_repository_auto_migrate_empty (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_sqlite_create_migration_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  test_sqlite_open (context.db_path, &db);

  g_assert_cmpuint (test_sqlite_read_user_version (db), ==, 2);
  g_assert_true (test_sqlite_relation_exists (db, "migrate_items", "table"));
  g_assert_true (test_sqlite_column_exists (db, "migrate_items", "id"));
  g_assert_true (test_sqlite_column_exists (db, "migrate_items", "name"));
  g_assert_true (test_sqlite_column_exists (db, "migrate_items", "tag"));
  g_assert_false (test_sqlite_column_exists (db, "migrate_items", "legacy"));
  g_assert_true (test_sqlite_relation_exists (db, "migrate_items_fts", "table"));
  g_assert_true (test_sqlite_relation_exists (db, "migrate_items_tag", "index"));
  test_sqlite_close (db);
  db = NULL;

}

static void
test_sqlite_repository_migrate_v1_to_v2 (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *stmt = NULL;
  int rc;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE migrate_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT NOT NULL, "
                     "  legacy TEXT"
                     ")"
  );
  test_sqlite_exec_ok (db,
                     "INSERT INTO migrate_items (id, name, legacy) VALUES (1, 'alpha', 'legacy-value')"
  );
  test_sqlite_exec_ok (db,
                     "PRAGMA user_version = 1"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_migration_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  test_sqlite_open (context.db_path, &db);

  g_assert_cmpuint (test_sqlite_read_user_version (db), ==, 2);
  g_assert_true (test_sqlite_column_exists (db, "migrate_items", "id"));
  g_assert_true (test_sqlite_column_exists (db, "migrate_items", "name"));
  g_assert_true (test_sqlite_column_exists (db, "migrate_items", "tag"));
  g_assert_false (test_sqlite_column_exists (db, "migrate_items", "legacy"));

  rc = sqlite3_prepare_v2 (db,
                           "SELECT id, name, tag FROM migrate_items WHERE id = 1",
                           -1,
                           &stmt,
                           NULL);
  g_assert_cmpint (rc, ==, SQLITE_OK);

  rc = sqlite3_step (stmt);
  g_assert_cmpint (rc, ==, SQLITE_ROW);
  g_assert_cmpint (sqlite3_column_int64 (stmt, 0), ==, 1);
  g_assert_cmpstr ((const char *) sqlite3_column_text (stmt, 1), ==, "alpha");
  g_assert_cmpint (sqlite3_column_type (stmt, 2), ==, SQLITE_NULL);
  sqlite3_finalize (stmt);
  stmt = NULL;

  g_assert_true (test_sqlite_relation_exists (db, "migrate_items_fts", "table"));
  test_sqlite_close (db);
  db = NULL;

}

static void
test_sqlite_repository_migrate_invalid_schema_transition (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GError) error = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-test-XXXXXX", &error));
  g_assert_no_error (error);
  test_sqlite_open (context.db_path, &db);
  test_sqlite_exec_ok (db,
                     "CREATE TABLE invalid_migration_items ("
                     "  id INTEGER PRIMARY KEY, "
                     "  name TEXT"
                     ");"
                     "INSERT INTO invalid_migration_items (id, name) VALUES (1, 'alpha');"
                     "PRAGMA user_version = 1"
  );
  test_sqlite_close (db);
  db = NULL;

  registry = test_sqlite_create_invalid_migration_registry ();
  repository = test_sqlite_context_create_repository (&context, registry, &error);
  g_assert_null (repository);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_assert_nonnull (strstr (error->message, "Cannot introduce NOT NULL column 'critical'"));
  g_clear_error (&error);
  test_sqlite_open (context.db_path, &db);

  g_assert_cmpuint (test_sqlite_read_user_version (db), ==, 1);
  g_assert_true (test_sqlite_column_exists (db, "invalid_migration_items", "name"));
  g_assert_false (test_sqlite_column_exists (db, "invalid_migration_items", "critical"));
  test_sqlite_close (db);
  db = NULL;

}

static void
test_sqlite_repository_vector_distance (void)
{
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomCustomMigrator) migrator = NULL;
  g_autoptr(GomMigration) migration = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomExpression) order_distance = NULL;
  g_autoptr(GomExpression) projection_distance = NULL;
  g_autoptr(GomExpression) field = NULL;
  g_autoptr(GomExpression) id = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomVector) query_vector = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GBytes) script = NULL;
  g_autoptr(GError) error = NULL;
  const float query_values[] = { 1.f, 0.f };
  static const char sql[] =
    "CREATE TABLE vectors (id INTEGER PRIMARY KEY, vector BLOB NOT NULL);"
    "INSERT INTO vectors (id, vector) VALUES "
    "(1, x'0000803f00000000'),"
    "(2, x'000000000000803f');";

  g_assert_true (test_sqlite_context_init (&context, "gom-sqlite-vector-test-XXXXXX", &error));
  g_assert_no_error (error);

  migrator = gom_custom_migrator_new (0);
  script = g_bytes_new_static (sql, strlen (sql));
  migration = gom_sql_migration_new (1, script);
  gom_custom_migrator_add_migration (migrator, g_steal_pointer (&migration));

  repository = dex_await_object (gom_repository_new (GOM_DRIVER (context.driver),
                                                     NULL,
                                                     GOM_MIGRATOR (migrator)),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

#if defined(GOM_DATABASE_SQLITE_VEC1)
  g_assert_true (gom_repository_supports_feature (repository,
                                                  GOM_REPOSITORY_FEATURE_VECTOR_SEARCH));
  g_assert_true (gom_repository_supports_vector_distance (repository,
                                                          GOM_VECTOR_FORMAT_FLOAT32_LE,
                                                          GOM_VECTOR_METRIC_COSINE));
  g_assert_true (gom_repository_supports_vector_distance (repository,
                                                          GOM_VECTOR_FORMAT_FLOAT32_LE,
                                                          GOM_VECTOR_METRIC_L2));
  g_assert_false (gom_repository_supports_vector_distance (repository,
                                                           GOM_VECTOR_FORMAT_FLOAT32_LE,
                                                           GOM_VECTOR_METRIC_DOT));
#else
  g_assert_false (gom_repository_supports_feature (repository,
                                                   GOM_REPOSITORY_FEATURE_VECTOR_SEARCH));
  g_assert_false (gom_repository_supports_vector_distance (repository,
                                                           GOM_VECTOR_FORMAT_FLOAT32_LE,
                                                           GOM_VECTOR_METRIC_COSINE));
#endif

  query_vector = gom_vector_new_float32 (query_values, G_N_ELEMENTS (query_values));
  g_assert_nonnull (query_vector);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (builder, "vectors");
  id = gom_field_expression_new ("id");
  field = gom_field_expression_new ("vector");
  projection_distance = gom_vector_distance_expression_new (field,
                                                            query_vector,
                                                            GOM_VECTOR_METRIC_COSINE);
  order_distance = gom_vector_distance_expression_new (field,
                                                       query_vector,
                                                       GOM_VECTOR_METRIC_COSINE);
  gom_query_builder_add_projection (builder, g_steal_pointer (&id));
  gom_query_builder_add_projection (builder, g_steal_pointer (&projection_distance));
  ordering = gom_ordering_new (g_steal_pointer (&order_distance), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));
  gom_query_builder_set_limit (builder, 1);
  query = gom_query_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_nonnull (query);

  cursor = dex_await_object (gom_repository_query (repository, query), &error);

#if defined(GOM_DATABASE_SQLITE_VEC1)
  g_assert_no_error (error);
  g_assert_nonnull (cursor);
  g_assert_true (dex_await_boolean (gom_cursor_next (cursor), &error));
  g_assert_no_error (error);
  g_assert_cmpint (gom_cursor_get_column_int64 (cursor, 0), ==, 1);
  g_assert_cmpfloat_with_epsilon (gom_cursor_get_column_double (cursor, 1), 0.0, .0001);
#else
  g_assert_null (cursor);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);
#endif
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/Sqlite/driver-pool", test_sqlite_driver_pool);
  _g_test_add_func ("/Gom/Sqlite/repository-query", test_sqlite_repository_query);
  _g_test_add_func ("/Gom/Sqlite/repository-count", test_sqlite_repository_count);
  _g_test_add_func ("/Gom/Sqlite/repository-list-records", test_sqlite_repository_list_records);
  _g_test_add_func ("/Gom/Sqlite/repository-list-entities", test_sqlite_repository_list_entities);
  _g_test_add_func ("/Gom/Sqlite/cursor-move", test_sqlite_cursor_move);
  _g_test_add_func ("/Gom/Sqlite/repository-insert", test_sqlite_repository_insert);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-during-open-read-cursor", test_sqlite_repository_insert_during_open_read_cursor);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-omits-default-identity", test_sqlite_repository_insert_omits_default_identity);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-unique-constraint", test_sqlite_repository_insert_unique_constraint);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-hyphenated-property", test_sqlite_repository_insert_hyphenated_property);
  _g_test_add_func ("/Gom/Sqlite/repository-find-one", test_sqlite_repository_find_one);
  _g_test_add_func ("/Gom/Sqlite/repository-find-one-with-properties", test_sqlite_repository_find_one_with_properties);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-gtype-property", test_sqlite_repository_insert_gtype_property);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-enum-property", test_sqlite_repository_insert_enum_property);
  _g_test_add_func ("/Gom/Sqlite/repository-insert-datetime-property", test_sqlite_repository_insert_datetime_property);
  _g_test_add_func ("/Gom/Sqlite/repository-query-invalid-entity-field", test_sqlite_repository_query_invalid_entity_field);
  _g_test_add_func ("/Gom/Sqlite/repository-query-unregistered-entity-type", test_sqlite_repository_query_unregistered_entity_type);
  _g_test_add_func ("/Gom/Sqlite/repository-mutate-invalid-entity-field", test_sqlite_repository_mutate_invalid_entity_field);
  _g_test_add_func ("/Gom/Sqlite/repository-update-delete", test_sqlite_repository_update_delete);
  _g_test_add_func ("/Gom/Sqlite/entity-crud", test_sqlite_entity_crud);
  _g_test_add_func ("/Gom/Sqlite/entity-crud-errors", test_sqlite_entity_crud_errors);
  _g_test_add_func ("/Gom/Sqlite/entity-default-identity-override", test_sqlite_entity_default_identity_override);
  _g_test_add_func ("/Gom/Sqlite/entity-insert-unsupported-property-transform", test_sqlite_entity_insert_unsupported_property_transform);
  _g_test_add_func ("/Gom/Sqlite/cursor-materialize", test_sqlite_cursor_materialize);
  _g_test_add_func ("/Gom/Sqlite/session-identity-map", test_sqlite_session_identity_map);
  _g_test_add_func ("/Gom/Sqlite/session-rekey-identity-map", test_sqlite_session_rekey_identity_map);
  _g_test_add_func ("/Gom/Sqlite/session-find-one", test_sqlite_session_find_one);
  _g_test_add_func ("/Gom/Sqlite/session-persist-flush-commit", test_sqlite_session_persist_flush_commit);
  _g_test_add_func ("/Gom/Sqlite/cursor-snapshot", test_sqlite_cursor_snapshot);
  _g_test_add_func ("/Gom/Sqlite/repository-describe-relation", test_sqlite_repository_describe_relation);
  _g_test_add_func ("/Gom/Sqlite/repository-list-relations", test_sqlite_repository_list_relations);
  _g_test_add_func ("/Gom/Sqlite/repository-search", test_sqlite_repository_search);
  _g_test_add_func ("/Gom/Sqlite/repository-expression-variants", test_sqlite_repository_expression_variants);
  _g_test_add_func ("/Gom/Sqlite/repository-vector-distance", test_sqlite_repository_vector_distance);
  _g_test_add_func ("/Gom/Sqlite/repository-auto-migrate-empty", test_sqlite_repository_auto_migrate_empty);
  _g_test_add_func ("/Gom/Sqlite/repository-migrate-v1-to-v2", test_sqlite_repository_migrate_v1_to_v2);
  _g_test_add_func ("/Gom/Sqlite/repository-migrate-invalid-schema-transition", test_sqlite_repository_migrate_invalid_schema_transition);
  return g_test_run ();
}
