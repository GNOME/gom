/* test-gom-schema-role.c
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

#include <stdint.h>

#include <libgom.h>

#include "lib/gom-meta-private.h"
#include "lib/gom-registry-diff-private.h"

#include "test-util.h"

typedef struct _TestSchemaRoleAccount      TestSchemaRoleAccount;
typedef struct _TestSchemaRoleAccountClass TestSchemaRoleAccountClass;
typedef struct _TestSchemaRoleAlias        TestSchemaRoleAlias;
typedef struct _TestSchemaRoleAliasClass   TestSchemaRoleAliasClass;

struct _TestSchemaRoleAccount
{
  GomEntity  parent_instance;
  int64_t    id;
  char      *username;
  char      *details;
};

struct _TestSchemaRoleAccountClass
{
  GomEntityClass parent_class;
};

struct _TestSchemaRoleAlias
{
  GomEntity  parent_instance;
  int64_t    id;
  char      *username;
  char      *token;
};

struct _TestSchemaRoleAliasClass
{
  GomEntityClass parent_class;
};

enum
{
  ACCOUNT_PROP_0,
  ACCOUNT_PROP_ID,
  ACCOUNT_PROP_USERNAME,
  ACCOUNT_PROP_DETAILS,
  ACCOUNT_N_PROPS
};

enum
{
  ALIAS_PROP_0,
  ALIAS_PROP_ID,
  ALIAS_PROP_USERNAME,
  ALIAS_PROP_TOKEN,
  ALIAS_N_PROPS
};

static GParamSpec *account_properties[ACCOUNT_N_PROPS];
static GParamSpec *alias_properties[ALIAS_N_PROPS];

GType test_schema_role_account_get_type (void);
GType test_schema_role_alias_get_type   (void);

G_DEFINE_TYPE (TestSchemaRoleAccount, test_schema_role_account, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestSchemaRoleAlias, test_schema_role_alias, GOM_TYPE_ENTITY)

#define TEST_TYPE_SCHEMA_ROLE_ACCOUNT (test_schema_role_account_get_type())
#define TEST_TYPE_SCHEMA_ROLE_ALIAS (test_schema_role_alias_get_type())

static void
test_schema_role_account_finalize (GObject *object)
{
  TestSchemaRoleAccount *self = (TestSchemaRoleAccount *)object;

  g_clear_pointer (&self->username, g_free);
  g_clear_pointer (&self->details, g_free);

  G_OBJECT_CLASS (test_schema_role_account_parent_class)->finalize (object);
}

static void
test_schema_role_account_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  TestSchemaRoleAccount *self = (TestSchemaRoleAccount *)object;

  switch (prop_id)
    {
    case ACCOUNT_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case ACCOUNT_PROP_USERNAME:
      g_value_set_string (value, self->username);
      break;

    case ACCOUNT_PROP_DETAILS:
      g_value_set_string (value, self->details);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_schema_role_account_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  TestSchemaRoleAccount *self = (TestSchemaRoleAccount *)object;

  switch (prop_id)
    {
    case ACCOUNT_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case ACCOUNT_PROP_USERNAME:
      g_set_str (&self->username, g_value_get_string (value));
      break;

    case ACCOUNT_PROP_DETAILS:
      g_set_str (&self->details, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_schema_role_account_class_init (TestSchemaRoleAccountClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_schema_role_account_finalize;
  object_class->get_property = test_schema_role_account_get_property;
  object_class->set_property = test_schema_role_account_set_property;

  account_properties[ACCOUNT_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  account_properties[ACCOUNT_PROP_USERNAME] =
    g_param_spec_string ("username", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  account_properties[ACCOUNT_PROP_DETAILS] =
    g_param_spec_string ("details", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, ACCOUNT_N_PROPS, account_properties);

  gom_entity_class_set_relation (entity_class, "schema_role_accounts");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
}

static void
test_schema_role_account_init (TestSchemaRoleAccount *self)
{
}

static void
test_schema_role_alias_finalize (GObject *object)
{
  TestSchemaRoleAlias *self = (TestSchemaRoleAlias *)object;

  g_clear_pointer (&self->username, g_free);
  g_clear_pointer (&self->token, g_free);

  G_OBJECT_CLASS (test_schema_role_alias_parent_class)->finalize (object);
}

static void
test_schema_role_alias_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  TestSchemaRoleAlias *self = (TestSchemaRoleAlias *)object;

  switch (prop_id)
    {
    case ALIAS_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case ALIAS_PROP_USERNAME:
      g_value_set_string (value, self->username);
      break;

    case ALIAS_PROP_TOKEN:
      g_value_set_string (value, self->token);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_schema_role_alias_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  TestSchemaRoleAlias *self = (TestSchemaRoleAlias *)object;

  switch (prop_id)
    {
    case ALIAS_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case ALIAS_PROP_USERNAME:
      g_set_str (&self->username, g_value_get_string (value));
      break;

    case ALIAS_PROP_TOKEN:
      g_set_str (&self->token, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_schema_role_alias_class_init (TestSchemaRoleAliasClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_schema_role_alias_finalize;
  object_class->get_property = test_schema_role_alias_get_property;
  object_class->set_property = test_schema_role_alias_set_property;

  alias_properties[ALIAS_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  alias_properties[ALIAS_PROP_USERNAME] =
    g_param_spec_string ("username", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  alias_properties[ALIAS_PROP_TOKEN] =
    g_param_spec_string ("token", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, ALIAS_N_PROPS, alias_properties);

  gom_entity_class_set_relation (entity_class, "schema_role_accounts");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_set_schema_role (entity_class, GOM_ENTITY_SCHEMA_ROLE_ALIAS);
  gom_entity_class_property_set_version_added (entity_class, "token", 99);
  gom_entity_class_property_set_unique (entity_class, "token", TRUE);
}

static void
test_schema_role_alias_init (TestSchemaRoleAlias *self)
{
}

static GomRegistry *
test_schema_role_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, TEST_TYPE_SCHEMA_ROLE_ALIAS);
  gom_registry_builder_add_entity_type (builder, TEST_TYPE_SCHEMA_ROLE_ACCOUNT);

  return gom_registry_builder_build (builder);
}

static void
test_schema_role_class_api (void)
{
  GomEntityClass *primary_class = g_type_class_get (TEST_TYPE_SCHEMA_ROLE_ACCOUNT);
  GomEntityClass *alias_class = g_type_class_get (TEST_TYPE_SCHEMA_ROLE_ALIAS);

  g_assert_cmpint (gom_entity_class_get_schema_role (primary_class),
                   ==,
                   GOM_ENTITY_SCHEMA_ROLE_PRIMARY);
  g_assert_cmpint (gom_entity_class_get_schema_role (alias_class),
                   ==,
                   GOM_ENTITY_SCHEMA_ROLE_ALIAS);
}

static void
test_schema_role_registry_lookup (void)
{
  g_autoptr(GomRegistry) registry = NULL;
  const GomEntitySpec *primary_spec;
  const GomEntitySpec *alias_spec;
  const GomEntitySpec *table_spec;

  registry = test_schema_role_create_registry ();
  primary_spec = _gom_registry_lookup_entity_by_type (registry, TEST_TYPE_SCHEMA_ROLE_ACCOUNT);
  alias_spec = _gom_registry_lookup_entity_by_type (registry, TEST_TYPE_SCHEMA_ROLE_ALIAS);
  table_spec = _gom_registry_lookup_entity_by_table (registry, "schema_role_accounts");

  g_assert_nonnull (primary_spec);
  g_assert_nonnull (alias_spec);
  g_assert_true (table_spec == primary_spec);
  g_assert_cmpint (gom_entity_spec_get_schema_role ((GomEntitySpec *)primary_spec),
                   ==,
                   GOM_ENTITY_SCHEMA_ROLE_PRIMARY);
  g_assert_cmpint (gom_entity_spec_get_schema_role ((GomEntitySpec *)alias_spec),
                   ==,
                   GOM_ENTITY_SCHEMA_ROLE_ALIAS);
}

static void
test_schema_role_migrations_ignore_aliases (void)
{
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRegistry) current = NULL;
  g_autoptr(GomRegistry) next = NULL;
  g_autoptr(GomRegistryDiff) diff = NULL;
  const GPtrArray *added_entities;
  GomEntitySpec *added_entity;

  registry = test_schema_role_create_registry ();

  g_assert_cmpuint (gom_registry_get_max_version (registry), ==, 1);

  current = gom_registry_snapshot (registry, 0);
  next = gom_registry_snapshot (registry, 1);
  diff = _gom_registry_diff_new (current, next);
  added_entities = _gom_registry_diff_get_added_entities (diff);

  g_assert_cmpuint (added_entities->len, ==, 1);
  added_entity = g_ptr_array_index ((GPtrArray *)added_entities, 0);
  g_assert_cmpint (gom_entity_spec_get_entity_type (added_entity),
                   ==,
                   TEST_TYPE_SCHEMA_ROLE_ACCOUNT);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  _g_test_add_func ("/Gom/Entity/schema-role/class-api", test_schema_role_class_api);
  _g_test_add_func ("/Gom/Entity/schema-role/registry-lookup", test_schema_role_registry_lookup);
  _g_test_add_func ("/Gom/Entity/schema-role/migrations-ignore-aliases", test_schema_role_migrations_ignore_aliases);

  return g_test_run ();
}
