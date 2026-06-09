/* gom-tombstone.c
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

#include <string.h>

#include "gom-cursor.h"
#include "gom-deletion.h"
#include "gom-deletion-builder.h"
#include "gom-driver-private.h"
#include "gom-entity-private.h"
#include "gom-expression.h"
#include "gom-insertion.h"
#include "gom-insertion-builder.h"
#include "gom-mutation.h"
#include "gom-mutation-result.h"
#include "gom-query.h"
#include "gom-query-builder.h"
#include "gom-repository.h"
#include "gom-repository-private.h"
#include "gom-session-private.h"
#include "gom-tombstone-private.h"
#include "gom-update.h"
#include "gom-update-builder.h"

struct _GomTombstone
{
  GomEntity  parent_instance;
  char      *entity_type;
  char      *relation;
  char      *identity;
  char      *deleted_at;
  guint64    delete_sequence;
};

struct _GomTombstoneClass
{
  GomEntityClass parent_class;
};

/**
 * GomTombstone:
 *
 * A repository-managed record that tracks logical deletions for entities.
 *
 * Tombstones preserve an entity's type, relation, serialized identity,
 * deletion sequence, and timestamp so libgom can distinguish a deletion from
 * a missing row, reconcile delete state across sessions, and remove or update
 * tombstone records without needing the original entity instance.
 */
typedef struct
{
  GomRepository *repository;
  GomSession    *session;
  char          *entity_type;
  char          *relation;
  char          *identity;
  char          *deleted_at;
  guint64        delete_sequence;
} GomTombstoneRecord;

enum
{
  PROP_0,
  PROP_ENTITY_TYPE,
  PROP_RELATION,
  PROP_IDENTITY,
  PROP_DELETE_SEQUENCE,
  PROP_DELETED_AT,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomTombstone, gom_tombstone, GOM_TYPE_ENTITY)

static GParamSpec *properties[N_PROPS];

static void
gom_tombstone_record_free (gpointer data)
{
  GomTombstoneRecord *state = data;

  g_clear_object (&state->repository);
  g_clear_object (&state->session);
  g_clear_pointer (&state->entity_type, g_free);
  g_clear_pointer (&state->relation, g_free);
  g_clear_pointer (&state->identity, g_free);
  g_clear_pointer (&state->deleted_at, g_free);
  g_free (state);
}

static void
gom_tombstone_finalize (GObject *object)
{
  GomTombstone *self = (GomTombstone *)object;

  g_clear_pointer (&self->entity_type, g_free);
  g_clear_pointer (&self->relation, g_free);
  g_clear_pointer (&self->identity, g_free);
  g_clear_pointer (&self->deleted_at, g_free);

  G_OBJECT_CLASS (gom_tombstone_parent_class)->finalize (object);
}

static void
gom_tombstone_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GomTombstone *self = (GomTombstone *)object;

  switch (prop_id)
    {
    case PROP_ENTITY_TYPE:
      g_value_set_string (value, self->entity_type);
      break;

    case PROP_RELATION:
      g_value_set_string (value, self->relation);
      break;

    case PROP_IDENTITY:
      g_value_set_string (value, self->identity);
      break;

    case PROP_DELETE_SEQUENCE:
      g_value_set_uint64 (value, self->delete_sequence);
      break;

    case PROP_DELETED_AT:
      g_value_set_string (value, self->deleted_at);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_tombstone_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GomTombstone *self = (GomTombstone *)object;

  switch (prop_id)
    {
    case PROP_ENTITY_TYPE:
      g_set_str (&self->entity_type, g_value_get_string (value));
      break;

    case PROP_RELATION:
      g_set_str (&self->relation, g_value_get_string (value));
      break;

    case PROP_IDENTITY:
      g_set_str (&self->identity, g_value_get_string (value));
      break;

    case PROP_DELETE_SEQUENCE:
      self->delete_sequence = g_value_get_uint64 (value);
      break;

    case PROP_DELETED_AT:
      g_set_str (&self->deleted_at, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_tombstone_class_init (GomTombstoneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);
  const char *identity_fields[] = { "entity-type", "identity", NULL };

  object_class->finalize = gom_tombstone_finalize;
  object_class->get_property = gom_tombstone_get_property;
  object_class->set_property = gom_tombstone_set_property;

  properties[PROP_ENTITY_TYPE] =
    g_param_spec_string ("entity-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_RELATION] =
    g_param_spec_string ("relation", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_IDENTITY] =
    g_param_spec_string ("identity", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_DELETE_SEQUENCE] =
    g_param_spec_uint64 ("delete-sequence", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  properties[PROP_DELETED_AT] =
    g_param_spec_string ("deleted-at", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gom_entity_class_set_relation (entity_class, "gom_tombstones");
  gom_entity_class_set_identity_fields (entity_class, identity_fields);
  gom_entity_class_set_version_added (entity_class, 1);

  gom_entity_class_property_set_mapped (entity_class, "entity-type", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "relation", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "identity", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "delete-sequence", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "deleted-at", TRUE);

  gom_entity_class_property_set_field_name (entity_class, "entity-type", "entity_type");
  gom_entity_class_property_set_field_name (entity_class, "delete-sequence", "delete_sequence");
  gom_entity_class_property_set_field_name (entity_class, "deleted-at", "deleted_at");

  gom_entity_class_property_set_nonnull (entity_class, "entity-type", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "relation", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "identity", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "delete-sequence", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "deleted-at", TRUE);

  for (guint i = 1; i < N_PROPS; i++)
    gom_entity_class_property_set_version_added (entity_class,
                                                 g_param_spec_get_name (properties[i]),
                                                 1);
}

static void
gom_tombstone_init (GomTombstone *self)
{
}

static const char *
gom_tombstone_get_field_name (GomEntityClass *entity_class,
                              const char     *property_name)
{
  GomEntityPropertyInfo *prop_info;

  g_assert (GOM_IS_ENTITY_CLASS (entity_class));
  g_assert (property_name != NULL);

  prop_info = _gom_entity_class_get_property (entity_class, property_name, FALSE);
  if (prop_info != NULL && prop_info->field_name != NULL)
    return prop_info->field_name;

  return property_name;
}

static GVariant *
gom_tombstone_variant_new_from_value (const GValue  *value,
                                      GError       **error)
{
  g_assert (value != NULL);
  g_assert (G_IS_VALUE (value));

  if (G_VALUE_HOLDS_BOOLEAN (value))
    return g_variant_new_boolean (g_value_get_boolean (value));

  if (G_VALUE_HOLDS_INT (value))
    return g_variant_new_int32 (g_value_get_int (value));

  if (G_VALUE_HOLDS_UINT (value))
    return g_variant_new_uint32 (g_value_get_uint (value));

  if (G_VALUE_HOLDS_INT64 (value))
    return g_variant_new_int64 (g_value_get_int64 (value));

  if (G_VALUE_HOLDS_UINT64 (value))
    return g_variant_new_uint64 (g_value_get_uint64 (value));

  if (G_VALUE_HOLDS_STRING (value))
    return g_variant_new_string (g_value_get_string (value) ?: "");

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Cannot serialize identity value of type %s",
               G_VALUE_TYPE_NAME (value));

  return NULL;
}

static gboolean
gom_tombstone_value_init_from_variant (GValue    *value,
                                       GVariant  *variant,
                                       GError   **error)
{
  g_autoptr(GVariant) child = NULL;

  g_assert (value != NULL);
  g_assert (variant != NULL);

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_VARIANT))
    child = g_variant_get_variant (variant);
  else
    child = g_variant_ref (variant);

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_BOOLEAN))
    {
      g_value_init (value, G_TYPE_BOOLEAN);
      g_value_set_boolean (value, g_variant_get_boolean (child));
      return TRUE;
    }

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_INT32))
    {
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, g_variant_get_int32 (child));
      return TRUE;
    }

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_UINT32))
    {
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, g_variant_get_uint32 (child));
      return TRUE;
    }

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_INT64))
    {
      g_value_init (value, G_TYPE_INT64);
      g_value_set_int64 (value, g_variant_get_int64 (child));
      return TRUE;
    }

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_UINT64))
    {
      g_value_init (value, G_TYPE_UINT64);
      g_value_set_uint64 (value, g_variant_get_uint64 (child));
      return TRUE;
    }

  if (g_variant_is_of_type (child, G_VARIANT_TYPE_STRING))
    {
      g_value_init (value, G_TYPE_STRING);
      g_value_set_string (value, g_variant_get_string (child, NULL));
      return TRUE;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Cannot deserialize identity value of type %s",
               g_variant_get_type_string (child));

  return FALSE;
}

static gboolean
gom_tombstone_transform_value_for_property (GValue      *value,
                                            GParamSpec  *pspec,
                                            GError     **error)
{
  g_auto(GValue) transformed = G_VALUE_INIT;

  g_assert (value != NULL);
  g_assert (G_IS_VALUE (value));
  g_assert (pspec != NULL);

  if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
    return TRUE;

  g_value_init (&transformed, G_PARAM_SPEC_VALUE_TYPE (pspec));

  if (!g_value_transform (value, &transformed))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Cannot convert identity value to property '%s'",
                   g_param_spec_get_name (pspec));
      return FALSE;
    }

  g_value_unset (value);
  g_value_init (value, G_VALUE_TYPE (&transformed));
  g_value_copy (&transformed, value);

  return TRUE;
}

static GVariant *
gom_tombstone_parse_identity (const char  *identity,
                              GError     **error)
{
  g_assert (identity != NULL);

  return g_variant_parse (G_VARIANT_TYPE ("a{sv}"), identity, NULL, NULL, error);
}

static GomExpression *
gom_tombstone_build_tombstone_filter (const char *entity_type,
                                      const char *identity)
{
  g_autoptr(GomExpression) entity_type_field = NULL;
  g_autoptr(GomExpression) entity_type_value = NULL;
  g_autoptr(GomExpression) identity_field = NULL;
  g_autoptr(GomExpression) identity_value = NULL;
  g_autoptr(GomExpression) entity_type_match = NULL;
  g_autoptr(GomExpression) identity_match = NULL;

  g_assert (entity_type != NULL);
  g_assert (identity != NULL);

  entity_type_field = gom_field_expression_new ("entity_type");
  entity_type_value = gom_literal_expression_new_string (entity_type);
  identity_field = gom_field_expression_new ("identity");
  identity_value = gom_literal_expression_new_string (identity);
  entity_type_match = gom_binary_expression_new_equal (entity_type_field, entity_type_value);
  identity_match = gom_binary_expression_new_equal (identity_field, identity_value);

  return gom_binary_expression_new_and (entity_type_match, identity_match);
}

static GomExpression *
gom_tombstone_build_identity_filter_from_variant (GType      entity_type,
                                                  GVariant  *identity,
                                                  GError   **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;
  g_autoptr(GomExpression) filter = NULL;

  g_assert (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  g_assert (identity != NULL);

  entity_class = g_type_class_get (entity_type);
  object_class = G_OBJECT_CLASS (entity_class);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type '%s' has no identity fields",
                   g_type_name (entity_type));
      return NULL;
    }

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      const char *identity_field = identity_fields[i];
      const char *storage_field;
      g_autoptr(GVariant) variant = NULL;
      g_autoptr(GomExpression) field = NULL;
      g_autoptr(GomExpression) literal = NULL;
      g_autoptr(GomExpression) predicate = NULL;
      GParamSpec *pspec;
      g_auto(GValue) value = G_VALUE_INIT;

      if (!(variant = g_variant_lookup_value (identity, identity_field, NULL)))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Serialized identity is missing field '%s'",
                       identity_field);
          return NULL;
        }

      if (!gom_tombstone_value_init_from_variant (&value, variant, error))
        return NULL;

      if (!(pspec = g_object_class_find_property (object_class, identity_field)))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Identity field '%s' is not a property on '%s'",
                       identity_field,
                       g_type_name (entity_type));
          return NULL;
        }

      if (!gom_tombstone_transform_value_for_property (&value, pspec, error))
        return NULL;

      storage_field = gom_tombstone_get_field_name (entity_class, identity_field);
      field = gom_field_expression_new (storage_field);
      literal = gom_literal_expression_new (&value);
      predicate = gom_binary_expression_new_equal (field, literal);

      if (filter == NULL)
        {
          filter = g_steal_pointer (&predicate);
        }
      else
        {
          g_autoptr(GomExpression) previous = g_steal_pointer (&filter);

          filter = gom_binary_expression_new_and (previous, predicate);
        }
    }

  return g_steal_pointer (&filter);
}

static gboolean
gom_tombstone_apply_identity_variant_to_entity (GomEntity  *entity,
                                                GVariant   *identity,
                                                GError    **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;

  g_assert (GOM_IS_ENTITY (entity));
  g_assert (identity != NULL);

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  object_class = G_OBJECT_GET_CLASS (entity);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type '%s' has no identity fields",
                   G_OBJECT_TYPE_NAME (entity));
      return FALSE;
    }

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      const char *identity_field = identity_fields[i];
      g_autoptr(GVariant) variant = NULL;
      GParamSpec *pspec;
      g_auto(GValue) value = G_VALUE_INIT;

      if (!(variant = g_variant_lookup_value (identity, identity_field, NULL)))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Serialized identity is missing field '%s'",
                       identity_field);
          return FALSE;
        }

      pspec = g_object_class_find_property (object_class, identity_field);
      if (pspec == NULL || (pspec->flags & G_PARAM_WRITABLE) == 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Identity field '%s' is not writable on '%s'",
                       identity_field,
                       G_OBJECT_TYPE_NAME (entity));
          return FALSE;
        }

      if (!gom_tombstone_value_init_from_variant (&value, variant, error))
        return FALSE;

      if (!gom_tombstone_transform_value_for_property (&value, pspec, error))
        return FALSE;

      g_object_set_property (G_OBJECT (entity), identity_field, &value);
    }

  return TRUE;
}

static void
gom_tombstone_add_string_assignment (GomUpdateBuilder *builder,
                                     const char       *field,
                                     const char       *value)
{
  g_autoptr(GomExpression) column = NULL;
  g_autoptr(GomExpression) literal = NULL;

  g_assert (builder != NULL);
  g_assert (field != NULL);
  g_assert (value != NULL);

  column = gom_field_expression_new (field);
  literal = gom_literal_expression_new_string (value);
  gom_update_builder_add_assignment (builder,
                                     g_steal_pointer (&column),
                                     g_steal_pointer (&literal));
}

static void
gom_tombstone_add_uint64_assignment (GomUpdateBuilder *builder,
                                     const char       *field,
                                     guint64           value)
{
  g_autoptr(GomExpression) column = NULL;
  g_autoptr(GomExpression) literal = NULL;
  g_auto(GValue) gvalue = G_VALUE_INIT;

  g_assert (builder != NULL);
  g_assert (field != NULL);

  g_value_init (&gvalue, G_TYPE_UINT64);
  g_value_set_uint64 (&gvalue, value);

  column = gom_field_expression_new (field);
  literal = gom_literal_expression_new (&gvalue);
  gom_update_builder_add_assignment (builder,
                                     g_steal_pointer (&column),
                                     g_steal_pointer (&literal));
}

static DexFuture *
gom_tombstone_record_mutate (GomTombstoneRecord *state,
                             GomMutation        *mutation)
{
  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));
  g_assert (GOM_IS_MUTATION (mutation));

  if (state->session != NULL)
    return _gom_session_mutate (state->session, mutation);

  return gom_repository_mutate (state->repository, mutation);
}

static DexFuture *
gom_tombstone_record_fiber (gpointer user_data)
{
  GomTombstoneRecord *state = user_data;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomUpdateBuilder) update_builder = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));

  filter = gom_tombstone_build_tombstone_filter (state->entity_type, state->identity);

  update_builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (update_builder, GOM_TYPE_TOMBSTONE);
  gom_update_builder_set_filter (update_builder, filter);
  gom_update_builder_set_limit (update_builder, 1);
  gom_tombstone_add_string_assignment (update_builder, "relation", state->relation);
  gom_tombstone_add_uint64_assignment (update_builder, "delete_sequence", state->delete_sequence);
  gom_tombstone_add_string_assignment (update_builder, "deleted_at", state->deleted_at);

  if (!(update = gom_update_builder_build (update_builder, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(result = dex_await_object (gom_tombstone_record_mutate (state, GOM_MUTATION (update)), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (gom_mutation_result_get_affected_rows (result) == 0)
    {
      g_autoptr(GomInsertionBuilder) insert_builder = NULL;
      g_autoptr(GomInsertion) insertion = NULL;
      g_autoptr(GPtrArray) row = NULL;
      g_auto(GValue) entity_type = G_VALUE_INIT;
      g_auto(GValue) relation = G_VALUE_INIT;
      g_auto(GValue) identity = G_VALUE_INIT;
      g_auto(GValue) delete_sequence = G_VALUE_INIT;
      g_auto(GValue) deleted_at = G_VALUE_INIT;

      g_clear_object (&result);

      g_value_init (&entity_type, G_TYPE_STRING);
      g_value_set_string (&entity_type, state->entity_type);
      g_value_init (&relation, G_TYPE_STRING);
      g_value_set_string (&relation, state->relation);
      g_value_init (&identity, G_TYPE_STRING);
      g_value_set_string (&identity, state->identity);
      g_value_init (&delete_sequence, G_TYPE_UINT64);
      g_value_set_uint64 (&delete_sequence, state->delete_sequence);
      g_value_init (&deleted_at, G_TYPE_STRING);
      g_value_set_string (&deleted_at, state->deleted_at);

      insert_builder = gom_insertion_builder_new (state->repository);
      gom_insertion_builder_set_target_entity_type (insert_builder, GOM_TYPE_TOMBSTONE);
      gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("entity_type"));
      gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("relation"));
      gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("identity"));
      gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("delete_sequence"));
      gom_insertion_builder_add_column (insert_builder, gom_field_expression_new ("deleted_at"));

      row = g_ptr_array_new ();
      g_ptr_array_add (row, gom_literal_expression_new (&entity_type));
      g_ptr_array_add (row, gom_literal_expression_new (&relation));
      g_ptr_array_add (row, gom_literal_expression_new (&identity));
      g_ptr_array_add (row, gom_literal_expression_new (&delete_sequence));
      g_ptr_array_add (row, gom_literal_expression_new (&deleted_at));
      gom_insertion_builder_add_row (insert_builder, (GomExpression **)row->pdata, row->len);

      if (!(insertion = gom_insertion_builder_build (insert_builder, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(result = dex_await_object (gom_tombstone_record_mutate (state, GOM_MUTATION (insertion)), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_true ();
}

DexFuture *
_gom_tombstone_ensure_schema (GomRepository *repository)
{
  static const char script[] =
    "CREATE TABLE IF NOT EXISTS gom_tombstones ("
    "  entity_type TEXT NOT NULL, "
    "  relation TEXT NOT NULL, "
    "  identity TEXT NOT NULL, "
    "  delete_sequence INTEGER NOT NULL, "
    "  deleted_at TEXT NOT NULL, "
    "  PRIMARY KEY (entity_type, identity)"
    ");";
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GBytes) bytes = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));

  if (!(driver = gom_repository_dup_driver (repository)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Repository has no driver");

  bytes = g_bytes_new_static (script, strlen (script));
  return _gom_driver_execute_sql (driver, bytes);
}

char *
_gom_tombstone_serialize_entity_identity (GomEntity  *entity,
                                          GError    **error)
{
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;
  GVariantBuilder builder;
  g_autoptr(GVariant) variant = NULL;

  g_return_val_if_fail (GOM_IS_ENTITY (entity), NULL);

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  object_class = G_OBJECT_GET_CLASS (entity);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type '%s' has no identity fields",
                   G_OBJECT_TYPE_NAME (entity));
      return NULL;
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      const char *identity_field = identity_fields[i];
      g_autoptr(GVariant) key_variant = NULL;
      g_autoptr(GVariant) value_variant = NULL;
      g_autoptr(GVariant) wrapped_value = NULL;
      g_autoptr(GVariant) entry = NULL;
      g_auto(GValue) value = G_VALUE_INIT;
      GParamSpec *pspec;

      pspec = g_object_class_find_property (object_class, identity_field);
      if (pspec == NULL || (pspec->flags & G_PARAM_READABLE) == 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Identity field '%s' is not readable on '%s'",
                       identity_field,
                       G_OBJECT_TYPE_NAME (entity));
          return NULL;
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (G_OBJECT (entity), identity_field, &value);
      value_variant = gom_tombstone_variant_new_from_value (&value, error);

      if (value_variant == NULL)
        return NULL;

      value_variant = g_variant_ref_sink (g_steal_pointer (&value_variant));
      key_variant = g_variant_ref_sink (g_variant_new_string (identity_field));
      wrapped_value = g_variant_ref_sink (g_variant_new_variant (value_variant));
      entry = g_variant_ref_sink (g_variant_new_dict_entry (key_variant, wrapped_value));
      g_variant_builder_add_value (&builder, entry);
    }

  variant = g_variant_ref_sink (g_variant_builder_end (&builder));

  return g_variant_print (variant, TRUE);
}

GomExpression *
_gom_tombstone_build_identity_filter (GType        entity_type,
                                      const char  *identity,
                                      GError     **error)
{
  g_autoptr(GVariant) variant = NULL;

  g_return_val_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY), NULL);
  g_return_val_if_fail (identity != NULL, NULL);

  if (!(variant = gom_tombstone_parse_identity (identity, error)))
    return NULL;

  return gom_tombstone_build_identity_filter_from_variant (entity_type, variant, error);
}

gboolean
_gom_tombstone_apply_identity_to_entity (GomEntity   *entity,
                                         const char  *identity,
                                         GError     **error)
{
  g_autoptr(GVariant) variant = NULL;

  g_return_val_if_fail (GOM_IS_ENTITY (entity), FALSE);
  g_return_val_if_fail (identity != NULL, FALSE);

  if (!(variant = gom_tombstone_parse_identity (identity, error)))
    return FALSE;

  return gom_tombstone_apply_identity_variant_to_entity (entity, variant, error);
}

DexFuture *
_gom_tombstone_record_with_session (GomRepository *repository,
                                    GomSession    *session,
                                    GType          entity_type,
                                    const char    *relation,
                                    const char    *identity,
                                    guint64        delete_sequence)
{
  GomTombstoneRecord *state;
  g_autoptr(GDateTime) now = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  if (session != NULL)
    dex_return_error_if_fail (GOM_IS_SESSION (session));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  dex_return_error_if_fail (relation != NULL);
  dex_return_error_if_fail (identity != NULL);

  now = g_date_time_new_now_utc ();

  state = g_new0 (GomTombstoneRecord, 1);
  state->repository = g_object_ref (repository);
  state->session = session != NULL ? g_object_ref (session) : NULL;
  state->entity_type = g_strdup (g_type_name (entity_type));
  state->relation = g_strdup (relation);
  state->identity = g_strdup (identity);
  state->delete_sequence = delete_sequence;
  state->deleted_at = g_date_time_format_iso8601 (now);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_tombstone_record_fiber,
                              state,
                              gom_tombstone_record_free);
}

DexFuture *
_gom_tombstone_record (GomRepository *repository,
                       GType          entity_type,
                       const char    *relation,
                       const char    *identity,
                       guint64        delete_sequence)
{
  return _gom_tombstone_record_with_session (repository,
                                             NULL,
                                             entity_type,
                                             relation,
                                             identity,
                                             delete_sequence);
}

DexFuture *
_gom_tombstone_remove (GomRepository *repository,
                       GType          entity_type,
                       const char    *identity)
{
  g_autoptr(GomDeletionBuilder) builder = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  g_autoptr(GError) error = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));
  dex_return_error_if_fail (identity != NULL);

  filter = gom_tombstone_build_tombstone_filter (g_type_name (entity_type), identity);
  builder = gom_deletion_builder_new ();
  gom_deletion_builder_set_target_entity_type (builder, GOM_TYPE_TOMBSTONE);
  gom_deletion_builder_set_filter (builder, filter);
  gom_deletion_builder_set_limit (builder, 1);

  if (!(deletion = gom_deletion_builder_build (builder, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return gom_repository_mutate (repository, GOM_MUTATION (deletion));
}

gboolean
_gom_tombstone_lookup_sequence (GomRepository  *repository,
                                GType           entity_type,
                                const char     *identity,
                                guint64        *sequence,
                                GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) local_error = NULL;
  gboolean has_row;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), FALSE);
  g_return_val_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY), FALSE);
  g_return_val_if_fail (identity != NULL, FALSE);
  g_return_val_if_fail (sequence != NULL, FALSE);

  *sequence = 0;

  filter = gom_tombstone_build_tombstone_filter (g_type_name (entity_type), identity);
  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, GOM_TYPE_TOMBSTONE);
  gom_query_builder_set_filter (builder, filter);
  gom_query_builder_set_limit (builder, 1);

  if (!(query = gom_query_builder_build (builder, error)))
    return FALSE;

  if (!(cursor = dex_await_object (gom_repository_query (repository, query), error)))
    return FALSE;

  has_row = dex_await_boolean (gom_cursor_next (cursor), &local_error);
  if (local_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return FALSE;
    }

  if (has_row)
    {
      g_auto(GValue) value = G_VALUE_INIT;

      if (!gom_cursor_get_column_by_name (cursor, "delete_sequence", &value))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Tombstone row does not include delete_sequence");
          dex_await (gom_cursor_close (cursor), NULL);
          return FALSE;
        }

      if (G_VALUE_HOLDS_UINT64 (&value))
        *sequence = g_value_get_uint64 (&value);
      else if (G_VALUE_HOLDS_INT64 (&value))
        *sequence = (guint64)g_value_get_int64 (&value);
      else if (G_VALUE_HOLDS_INT (&value))
        *sequence = (guint64)g_value_get_int (&value);
      else if (G_VALUE_HOLDS_UINT (&value))
        *sequence = g_value_get_uint (&value);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Tombstone delete_sequence has unsupported type %s",
                       G_VALUE_TYPE_NAME (&value));
          dex_await (gom_cursor_close (cursor), NULL);
          return FALSE;
        }
    }

  if (!dex_await (gom_cursor_close (cursor), error))
    return FALSE;

  return TRUE;
}
