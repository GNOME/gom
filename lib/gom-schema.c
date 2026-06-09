/* gom-schema.c
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

#include "gom-schema.h"
#include "gom-schema-private.h"

struct _GomSchema
{
  GObject  parent_instance;
  char    *name;
};

struct _GomSchemaClass
{
  GObjectClass parent_class;
};

struct _GomFieldSchema
{
  GomSchema  parent_instance;
  char      *sql_type;
  char      *default_value;
  guint      nonnull : 1;
  guint      primary_key : 1;
};

struct _GomFieldSchemaClass
{
  GomSchemaClass parent_class;
};

struct _GomRelationSchema
{
  GomSchema   parent_instance;
  GListModel *fields;
  GListModel *indexes;
};

struct _GomRelationSchemaClass
{
  GomSchemaClass parent_class;
};

struct _GomIndexSchema
{
  GomSchema   parent_instance;
  char      **fields;
  guint       unique : 1;
};

struct _GomIndexSchemaClass
{
  GomSchemaClass parent_class;
};

G_DEFINE_ABSTRACT_TYPE (GomSchema, gom_schema, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GomFieldSchema, gom_field_schema, GOM_TYPE_SCHEMA)
G_DEFINE_FINAL_TYPE (GomRelationSchema, gom_relation_schema, GOM_TYPE_SCHEMA)
G_DEFINE_FINAL_TYPE (GomIndexSchema, gom_index_schema, GOM_TYPE_SCHEMA)

enum
{
  PROP_0,
  PROP_NAME,
  N_PROPS
};

static GParamSpec *schema_properties[N_PROPS];

static void
gom_schema_finalize (GObject *object)
{
  GomSchema *self = GOM_SCHEMA (object);

  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gom_schema_parent_class)->finalize (object);
}

static void
gom_schema_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GomSchema *self = GOM_SCHEMA (object);

  switch (prop_id)
    {
    case PROP_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_schema_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GomSchema *self = GOM_SCHEMA (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_schema_class_init (GomSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_schema_finalize;
  object_class->set_property = gom_schema_set_property;
  object_class->get_property = gom_schema_get_property;

  schema_properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, schema_properties);
}

static void
gom_schema_init (GomSchema *self)
{
}

const char *
gom_schema_get_name (GomSchema *self)
{
  g_return_val_if_fail (GOM_IS_SCHEMA (self), NULL);

  return self->name;
}

enum
{
  FIELD_PROP_0,
  FIELD_PROP_SQL_TYPE,
  FIELD_PROP_NOT_NULL,
  FIELD_PROP_PRIMARY_KEY,
  FIELD_PROP_DEFAULT_VALUE,
  FIELD_N_PROPS
};

static GParamSpec *field_properties[FIELD_N_PROPS];

static void
gom_field_schema_finalize (GObject *object)
{
  GomFieldSchema *self = GOM_FIELD_SCHEMA (object);

  g_clear_pointer (&self->sql_type, g_free);
  g_clear_pointer (&self->default_value, g_free);

  G_OBJECT_CLASS (gom_field_schema_parent_class)->finalize (object);
}

static void
gom_field_schema_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GomFieldSchema *self = GOM_FIELD_SCHEMA (object);

  switch (prop_id)
    {
    case FIELD_PROP_SQL_TYPE:
      self->sql_type = g_value_dup_string (value);
      break;

    case FIELD_PROP_NOT_NULL:
      self->nonnull = g_value_get_boolean (value);
      break;

    case FIELD_PROP_PRIMARY_KEY:
      self->primary_key = g_value_get_boolean (value);
      break;

    case FIELD_PROP_DEFAULT_VALUE:
      self->default_value = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_field_schema_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GomFieldSchema *self = GOM_FIELD_SCHEMA (object);

  switch (prop_id)
    {
    case FIELD_PROP_SQL_TYPE:
      g_value_set_string (value, self->sql_type);
      break;

    case FIELD_PROP_NOT_NULL:
      g_value_set_boolean (value, self->nonnull);
      break;

    case FIELD_PROP_PRIMARY_KEY:
      g_value_set_boolean (value, self->primary_key);
      break;

    case FIELD_PROP_DEFAULT_VALUE:
      g_value_set_string (value, self->default_value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_field_schema_class_init (GomFieldSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_field_schema_finalize;
  object_class->set_property = gom_field_schema_set_property;
  object_class->get_property = gom_field_schema_get_property;

  field_properties[FIELD_PROP_SQL_TYPE] =
    g_param_spec_string ("sql-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  field_properties[FIELD_PROP_NOT_NULL] =
    g_param_spec_boolean ("not-null", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  field_properties[FIELD_PROP_PRIMARY_KEY] =
    g_param_spec_boolean ("primary-key", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  field_properties[FIELD_PROP_DEFAULT_VALUE] =
    g_param_spec_string ("default-value", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, FIELD_N_PROPS, field_properties);
}

static void
gom_field_schema_init (GomFieldSchema *self)
{
}

GomFieldSchema *
_gom_field_schema_new (const char *name,
                       const char *sql_type,
                       gboolean    nonnull,
                       gboolean    primary_key,
                       const char *default_value)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GOM_TYPE_FIELD_SCHEMA,
                       "name", name,
                       "sql-type", sql_type,
                       "not-null", nonnull,
                       "primary-key", primary_key,
                       "default-value", default_value,
                       NULL);
}

const char *
gom_field_schema_get_sql_type (GomFieldSchema *self)
{
  g_return_val_if_fail (GOM_IS_FIELD_SCHEMA (self), NULL);

  return self->sql_type;
}

gboolean
gom_field_schema_get_nonnull (GomFieldSchema *self)
{
  g_return_val_if_fail (GOM_IS_FIELD_SCHEMA (self), FALSE);

  return self->nonnull;
}

gboolean
gom_field_schema_get_primary_key (GomFieldSchema *self)
{
  g_return_val_if_fail (GOM_IS_FIELD_SCHEMA (self), FALSE);

  return self->primary_key;
}

const char *
gom_field_schema_get_default_value (GomFieldSchema *self)
{
  g_return_val_if_fail (GOM_IS_FIELD_SCHEMA (self), NULL);

  return self->default_value;
}

enum
{
  RELATION_PROP_0,
  RELATION_PROP_FIELDS,
  RELATION_PROP_INDEXES,
  RELATION_N_PROPS
};

static GParamSpec *relation_properties[RELATION_N_PROPS];

static void
gom_relation_schema_finalize (GObject *object)
{
  GomRelationSchema *self = GOM_RELATION_SCHEMA (object);

  g_clear_object (&self->fields);
  g_clear_object (&self->indexes);

  G_OBJECT_CLASS (gom_relation_schema_parent_class)->finalize (object);
}

static void
gom_relation_schema_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GomRelationSchema *self = GOM_RELATION_SCHEMA (object);

  switch (prop_id)
    {
    case RELATION_PROP_FIELDS:
      self->fields = g_value_dup_object (value);
      break;

    case RELATION_PROP_INDEXES:
      self->indexes = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_relation_schema_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GomRelationSchema *self = GOM_RELATION_SCHEMA (object);

  switch (prop_id)
    {
    case RELATION_PROP_FIELDS:
      g_value_set_object (value, self->fields);
      break;

    case RELATION_PROP_INDEXES:
      g_value_set_object (value, self->indexes);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_relation_schema_class_init (GomRelationSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_relation_schema_finalize;
  object_class->set_property = gom_relation_schema_set_property;
  object_class->get_property = gom_relation_schema_get_property;

  relation_properties[RELATION_PROP_FIELDS] =
    g_param_spec_object ("fields", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  relation_properties[RELATION_PROP_INDEXES] =
    g_param_spec_object ("indexes", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, RELATION_N_PROPS, relation_properties);
}

static void
gom_relation_schema_init (GomRelationSchema *self)
{
}

GomRelationSchema *
_gom_relation_schema_new (const char *name,
                          GListModel *fields,
                          GListModel *indexes)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (fields), NULL);
  g_return_val_if_fail (G_IS_LIST_MODEL (indexes), NULL);

  return g_object_new (GOM_TYPE_RELATION_SCHEMA,
                       "name", name,
                       "fields", fields,
                       "indexes", indexes,
                       NULL);
}

/**
 * gom_relation_schema_list_fields:
 * @self: a [class@Gom.Schema]
 *
 * Gets a list of field schema for the relation.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Gom.FieldSchema].
 */
GListModel *
gom_relation_schema_list_fields (GomRelationSchema *self)
{
  g_return_val_if_fail (GOM_IS_RELATION_SCHEMA (self), NULL);

  return g_object_ref (self->fields);
}

/**
 * gom_relation_schema_list_indexes:
 * @self: a [class@Gom.Schema]
 *
 * Gets a list of index schema for the relation.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Gom.IndexSchema].
 */
GListModel *
gom_relation_schema_list_indexes (GomRelationSchema *self)
{
  g_return_val_if_fail (GOM_IS_RELATION_SCHEMA (self), NULL);

  return g_object_ref (self->indexes);
}

enum
{
  INDEX_PROP_0,
  INDEX_PROP_UNIQUE,
  INDEX_PROP_FIELDS,
  INDEX_N_PROPS
};

static GParamSpec *index_properties[INDEX_N_PROPS];

static void
gom_index_schema_finalize (GObject *object)
{
  GomIndexSchema *self = GOM_INDEX_SCHEMA (object);

  g_clear_pointer (&self->fields, g_strfreev);

  G_OBJECT_CLASS (gom_index_schema_parent_class)->finalize (object);
}

static void
gom_index_schema_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GomIndexSchema *self = GOM_INDEX_SCHEMA (object);

  switch (prop_id)
    {
    case INDEX_PROP_UNIQUE:
      self->unique = g_value_get_boolean (value);
      break;

    case INDEX_PROP_FIELDS:
      self->fields = g_strdupv (g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_index_schema_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GomIndexSchema *self = GOM_INDEX_SCHEMA (object);

  switch (prop_id)
    {
    case INDEX_PROP_UNIQUE:
      g_value_set_boolean (value, self->unique);
      break;

    case INDEX_PROP_FIELDS:
      g_value_set_boxed (value, self->fields);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_index_schema_class_init (GomIndexSchemaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_index_schema_finalize;
  object_class->set_property = gom_index_schema_set_property;
  object_class->get_property = gom_index_schema_get_property;

  index_properties[INDEX_PROP_UNIQUE] =
    g_param_spec_boolean ("unique", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  index_properties[INDEX_PROP_FIELDS] =
    g_param_spec_boxed ("fields", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, INDEX_N_PROPS, index_properties);
}

static void
gom_index_schema_init (GomIndexSchema *self)
{
}

GomIndexSchema *
_gom_index_schema_new (const char         *name,
                       gboolean            unique,
                       const char * const *fields)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (fields != NULL, NULL);

  return g_object_new (GOM_TYPE_INDEX_SCHEMA,
                       "name", name,
                       "unique", unique,
                       "fields", fields,
                       NULL);
}

gboolean
gom_index_schema_get_unique (GomIndexSchema *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SCHEMA (self), FALSE);

  return self->unique;
}

const char * const *
gom_index_schema_get_fields (GomIndexSchema *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SCHEMA (self), NULL);

  return (const char * const *)self->fields;
}
