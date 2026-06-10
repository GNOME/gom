/* gom-meta.c
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

#include "gom-entity-private.h"
#include "gom-meta-private.h"
#include "gom-util-private.h"

typedef struct _GomSpecPrivate
{
  char *name;
} GomSpecPrivate;

struct _GomRegistry
{
  GObject     parent_instance;
  GPtrArray  *entities;
  GHashTable *by_type;
  GHashTable *by_name;
  GHashTable *by_table;
  guint       version;
  guint       max_version;
  guint       frozen : 1;
};

struct _GomRegistryClass
{
  GObjectClass parent_class;
};

struct _GomRegistryBuilder
{
  gatomicrefcount  ref_count;
  GArray          *entity_types;
  GHashTable      *seen_types;
};

struct _GomSpec
{
  GObject parent_instance;
};

struct _GomSpecClass
{
  GObjectClass parent_class;
};

struct _GomEntitySpec
{
  GomSpec      parent_instance;
  char        *table;
  GType        entity_type;
  char       **identity_fields;
  char        *discriminator_field;
  char        *discriminator_value;
  guint        version_added;
  guint        version_removed;
  GPtrArray   *properties;
  GPtrArray   *indexes;
  GListStore  *relationships;
  GHashTable  *properties_by_name;
  GHashTable  *properties_by_field;
  GHashTable  *indexes_by_name;
  GHashTable  *relationships_by_name;
};

struct _GomEntitySpecClass
{
  GomSpecClass parent_class;
};

struct _GomPropertySpec
{
  GomSpec     parent_instance;
  char       *field;
  char       *ref_table;
  char       *ref_field;
  GParamSpec *pspec;
  GType       value_type;
  GValue      default_value;
  guint       version_added;
  guint       version_removed;
  guint       nonnull : 1;
  guint       unique : 1;
  guint       mapped : 1;
  guint       has_default_value : 1;
  guint       search_flags;
};

struct _GomPropertySpecClass
{
  GomSpecClass parent_class;
};

struct _GomIndexSpec
{
  GomSpec   parent_instance;
  char    **fields;
  guint     version_added;
  guint     version_removed;
  guint     unique : 1;
  guint     search_flags;
};

struct _GomIndexSpecClass
{
  GomSpecClass parent_class;
};

struct _GomRelationshipSpec
{
  GomSpec                      parent_instance;
  GType                        target_type;
  GomRelationshipCardinality   cardinality;
  GomRelationshipStorage       storage;
  char                        *inverse_name;
  char                       **local_fields;
  char                       **remote_fields;
  char                        *join_relation;
  char                       **join_local_fields;
  char                       **join_remote_fields;
  guint                        version_added;
  guint                        version_removed;
  guint                        min_count;
  guint                        max_count;
  GomRelationshipDeleteRule    delete_rule;
  guint                        optional : 1;
  guint                        ordered : 1;
};

struct _GomRelationshipSpecClass
{
  GomSpecClass parent_class;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GomSpec, gom_spec, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GomRegistry, gom_registry, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GomEntitySpec, gom_entity_spec, GOM_TYPE_SPEC)
G_DEFINE_FINAL_TYPE (GomPropertySpec, gom_property_spec, GOM_TYPE_SPEC)
G_DEFINE_FINAL_TYPE (GomIndexSpec, gom_index_spec, GOM_TYPE_SPEC)
G_DEFINE_FINAL_TYPE (GomRelationshipSpec, gom_relationship_spec, GOM_TYPE_SPEC)
G_DEFINE_BOXED_TYPE (GomRegistryBuilder,
                     gom_registry_builder,
                     gom_registry_builder_ref,
                     gom_registry_builder_unref)

static void
gom_spec_finalize (GObject *object)
{
  GomSpecPrivate *priv = gom_spec_get_instance_private ((GomSpec *)object);

  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (gom_spec_parent_class)->finalize (object);
}

static void
gom_spec_class_init (GomSpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_spec_finalize;
}

static void
gom_spec_init (GomSpec *self)
{
}

const char *
gom_spec_get_name (GomSpec *self)
{
  GomSpecPrivate *priv = gom_spec_get_instance_private (self);

  g_return_val_if_fail (GOM_IS_SPEC (self), NULL);

  return priv->name;
}

static void
gom_spec_set_name (GomSpec    *self,
                   const char *name)
{
  GomSpecPrivate *priv = gom_spec_get_instance_private (self);

  g_return_if_fail (GOM_IS_SPEC (self));

  g_set_str (&priv->name, name);
}

static void
_gom_registry_consider_version (GomRegistry *self,
                                guint        version)
{
  if (version > self->max_version)
    self->max_version = version;
}

static gboolean
_gom_version_visible (guint version,
                      guint version_added,
                      guint version_removed)
{
  if (version_added > version)
    return FALSE;

  if (version_removed != 0 && version >= version_removed)
    return FALSE;

  return TRUE;
}

static gboolean
gom_registry_relationship_has_fields (const char * const *fields)
{
  return fields != NULL && fields[0] != NULL;
}

static void
gom_registry_validate_relationship_pair (GomRegistry         *registry,
                                         GomEntitySpec       *entity,
                                         GomRelationshipSpec *relationship)
{
  const char *entity_name;
  const char *relationship_name;
  const char *inverse_name;
  const char *target_name;
  const char *inverse_entity_name;
  const char * const *source_fields = NULL;
  const char * const *inverse_fields = NULL;
  const GomEntitySpec *target_entity;
  const GomRelationshipSpec *inverse;

  g_assert (GOM_IS_REGISTRY (registry));
  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (GOM_IS_RELATIONSHIP_SPEC (relationship));

  entity_name = gom_entity_spec_get_name (entity);
  relationship_name = gom_relationship_spec_get_name (relationship);
  inverse_name = gom_relationship_spec_get_inverse_name (relationship);
  target_name = g_type_name (gom_relationship_spec_get_target_type (relationship));
  target_entity = _gom_registry_lookup_entity_by_type (registry,
                                                       gom_relationship_spec_get_target_type (relationship));

  if (gom_relationship_spec_get_min_count (relationship) > 0 &&
      gom_relationship_spec_get_optional (relationship))
    g_warning ("Entity `%s` relationship `%s` is optional but requires at least %u item(s)",
               entity_name,
               relationship_name,
               gom_relationship_spec_get_min_count (relationship));

  if (gom_relationship_spec_get_max_count (relationship) != 0 &&
      gom_relationship_spec_get_min_count (relationship) > gom_relationship_spec_get_max_count (relationship))
    g_warning ("Entity `%s` relationship `%s` has min_count %u greater than max_count %u",
               entity_name,
               relationship_name,
               gom_relationship_spec_get_min_count (relationship),
               gom_relationship_spec_get_max_count (relationship));

  if (gom_relationship_spec_get_cardinality (relationship) == GOM_RELATIONSHIP_CARDINALITY_TO_ONE)
    {
      if (gom_relationship_spec_get_max_count (relationship) != 1)
        g_warning ("Entity `%s` relationship `%s` is to-one but max_count is %u",
                   entity_name,
                   relationship_name,
                   gom_relationship_spec_get_max_count (relationship));

      if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_FK)
        {
          if (!gom_registry_relationship_has_fields (gom_relationship_spec_get_local_fields (relationship)))
            g_warning ("Entity `%s` relationship `%s` is missing local foreign-key fields",
                       entity_name,
                       relationship_name);

          if (gom_registry_relationship_has_fields (gom_relationship_spec_get_remote_fields (relationship)))
            g_warning ("Entity `%s` relationship `%s` defines remote fields for a to-one foreign-key relation",
                       entity_name,
                       relationship_name);
        }
    }
  else if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_FK)
    {
      if (!gom_registry_relationship_has_fields (gom_relationship_spec_get_remote_fields (relationship)))
        g_warning ("Entity `%s` relationship `%s` is missing remote foreign-key fields",
                   entity_name,
                   relationship_name);

      if (gom_registry_relationship_has_fields (gom_relationship_spec_get_local_fields (relationship)))
        g_warning ("Entity `%s` relationship `%s` defines local fields for a to-many foreign-key relation",
                   entity_name,
                   relationship_name);
    }

  if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_JOIN_TABLE)
    {
      guint n_join_local;
      guint n_join_remote;

      if (relationship->join_relation == NULL || *relationship->join_relation == '\0')
        g_warning ("Entity `%s` relationship `%s` is missing a join relation",
                   entity_name,
                   relationship_name);

      n_join_local = _gom_strv_length (gom_relationship_spec_get_join_local_fields (relationship));
      n_join_remote = _gom_strv_length (gom_relationship_spec_get_join_remote_fields (relationship));

      if (n_join_local == 0 || n_join_remote == 0)
        g_warning ("Entity `%s` relationship `%s` is missing join-table field mapping",
                   entity_name,
                   relationship_name);
      else if (n_join_local != n_join_remote)
        g_warning ("Entity `%s` relationship `%s` has mismatched join-table field cardinality (%u != %u)",
                   entity_name,
                   relationship_name,
                   n_join_local,
                   n_join_remote);

      if (gom_relationship_spec_get_cardinality (relationship) != GOM_RELATIONSHIP_CARDINALITY_TO_MANY)
        g_warning ("Entity `%s` relationship `%s` uses a join table but is not to-many",
                   entity_name,
                   relationship_name);
    }

  if (inverse_name == NULL || *inverse_name == '\0')
    {
      g_warning ("Entity `%s` relationship `%s` is missing an inverse relationship",
                 entity_name,
                 relationship_name);
      return;
    }

  if (target_entity == NULL)
    {
      g_warning ("Entity `%s` relationship `%s` targets `%s`, which is not present in the registry",
                 entity_name,
                 relationship_name,
                 target_name);
      return;
    }

  if (!(inverse = _gom_entity_spec_lookup_relationship_by_name ((GomEntitySpec *)target_entity, inverse_name)))
    {
      g_warning ("Entity `%s` relationship `%s` references inverse `%s` on `%s`, but that relationship does not exist",
                 entity_name,
                 relationship_name,
                 inverse_name,
                 gom_entity_spec_get_name ((GomEntitySpec *) target_entity));
      return;
    }

  inverse_entity_name = gom_entity_spec_get_name ((GomEntitySpec *)target_entity);
  if (!g_type_is_a (gom_entity_spec_get_entity_type (entity),
                    gom_relationship_spec_get_target_type ((GomRelationshipSpec *)inverse)) &&
      !g_type_is_a (gom_relationship_spec_get_target_type ((GomRelationshipSpec *)inverse),
                    gom_entity_spec_get_entity_type (entity)))
    g_warning ("Entity `%s` relationship `%s` points at inverse `%s` on `%s`, but the inverse targets `%s` instead of `%s`",
               entity_name,
               relationship_name,
               inverse_name,
               inverse_entity_name,
               g_type_name (gom_relationship_spec_get_target_type ((GomRelationshipSpec *) inverse)),
               g_type_name (gom_entity_spec_get_entity_type (entity)));

  if (inverse == relationship)
    return;

  if (_gom_str_equal (relationship_name, inverse_name))
    return;

  if (gom_relationship_spec_get_storage (relationship) != gom_relationship_spec_get_storage ((GomRelationshipSpec *) inverse))
    {
      g_warning ("Entity `%s` relationship `%s` and inverse `%s` on `%s` disagree on storage strategy",
                 entity_name,
                 relationship_name,
                 inverse_name,
                 inverse_entity_name);
      return;
    }

  if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_FK)
    {
      if (gom_relationship_spec_get_cardinality (relationship) == GOM_RELATIONSHIP_CARDINALITY_TO_ONE)
        {
          source_fields = gom_relationship_spec_get_local_fields (relationship);
          inverse_fields = gom_relationship_spec_get_remote_fields ((GomRelationshipSpec *) inverse);
        }
      else
        {
          source_fields = gom_relationship_spec_get_remote_fields (relationship);
          inverse_fields = gom_relationship_spec_get_local_fields ((GomRelationshipSpec *)inverse);
        }

      if (_gom_strv_length (source_fields) == 0 || _gom_strv_length (inverse_fields) == 0)
        {
          g_warning ("Entity `%s` relationship `%s` and inverse `%s` on `%s` are missing foreign-key field bindings",
                     entity_name,
                     relationship_name,
                     inverse_name,
                     inverse_entity_name);
          return;
        }

      if (_gom_strv_length (source_fields) != _gom_strv_length (inverse_fields) ||
          !_gom_strv_equal (source_fields, inverse_fields))
        {
          g_warning ("Entity `%s` relationship `%s` and inverse `%s` on `%s` do not reference the same foreign-key fields",
                     entity_name,
                     relationship_name,
                     inverse_name,
                     inverse_entity_name);
        }
    }
  else if (gom_relationship_spec_get_storage (relationship) == GOM_RELATIONSHIP_STORAGE_JOIN_TABLE)
    {
      if (gom_relationship_spec_get_cardinality ((GomRelationshipSpec *) inverse) != GOM_RELATIONSHIP_CARDINALITY_TO_MANY)
        {
          g_warning ("Entity `%s` relationship `%s` uses a join table but inverse `%s` on `%s` is not to-many",
                     entity_name,
                     relationship_name,
                     inverse_name,
                     inverse_entity_name);
          return;
        }

      if (g_strcmp0 (gom_relationship_spec_get_join_relation (relationship),
                     gom_relationship_spec_get_join_relation ((GomRelationshipSpec *) inverse)) != 0)
        g_warning ("Entity `%s` relationship `%s` and inverse `%s` on `%s` disagree on join relation",
                   entity_name,
                   relationship_name,
                   inverse_name,
                   inverse_entity_name);

      if (_gom_strv_length (gom_relationship_spec_get_join_local_fields (relationship)) !=
          _gom_strv_length (gom_relationship_spec_get_join_remote_fields ((GomRelationshipSpec *) inverse)) ||
          !_gom_strv_equal (gom_relationship_spec_get_join_local_fields (relationship),
                             gom_relationship_spec_get_join_remote_fields ((GomRelationshipSpec *) inverse)) ||
          _gom_strv_length (gom_relationship_spec_get_join_remote_fields (relationship)) !=
          _gom_strv_length (gom_relationship_spec_get_join_local_fields ((GomRelationshipSpec *) inverse)) ||
          !_gom_strv_equal (gom_relationship_spec_get_join_remote_fields (relationship),
                             gom_relationship_spec_get_join_local_fields ((GomRelationshipSpec *) inverse)))
        g_warning ("Entity `%s` relationship `%s` and inverse `%s` on `%s` do not agree on join-table fields",
                   entity_name,
                   relationship_name,
                   inverse_name,
                   inverse_entity_name);
    }
}

static void
gom_registry_validate_relationships (GomRegistry *registry)
{
  g_autoptr(GomRegistry) snapshot = NULL;

  g_return_if_fail (GOM_IS_REGISTRY (registry));

  snapshot = gom_registry_snapshot (registry, gom_registry_get_max_version (registry));

  for (guint i = 0; i < snapshot->entities->len; i++)
    {
      GomEntitySpec *entity = g_ptr_array_index (snapshot->entities, i);
      g_autoptr(GListModel) relationships = NULL;
      guint n_relationships;

      relationships = gom_entity_spec_list_relationships (entity);
      n_relationships = g_list_model_get_n_items (relationships);

      for (guint j = 0; j < n_relationships; j++)
        {
          g_autoptr(GomRelationshipSpec) relationship = g_list_model_get_item (relationships, j);

          if (relationship != NULL)
            gom_registry_validate_relationship_pair (registry, entity, relationship);
        }
    }
}

static void
_gom_registry_finalize (GObject *object)
{
  GomRegistry *self = (GomRegistry *)object;

  g_clear_pointer (&self->entities, g_ptr_array_unref);
  g_clear_pointer (&self->by_type, g_hash_table_unref);
  g_clear_pointer (&self->by_name, g_hash_table_unref);
  g_clear_pointer (&self->by_table, g_hash_table_unref);

  G_OBJECT_CLASS (gom_registry_parent_class)->finalize (object);
}

static void
gom_registry_class_init (GomRegistryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gom_registry_finalize;
}

static void
gom_registry_init (GomRegistry *self)
{
  self->entities = g_ptr_array_new_with_free_func (g_object_unref);
  self->by_type = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->by_name = g_hash_table_new (g_str_hash, g_str_equal);
  self->by_table = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
_gom_registry_builder_finalize (GomRegistryBuilder *self)
{
  g_clear_pointer (&self->entity_types, g_array_unref);
  g_clear_pointer (&self->seen_types, g_hash_table_unref);
}

static void
_gom_entity_spec_finalize (GObject *object)
{
  GomEntitySpec *self = (GomEntitySpec *)object;

  g_clear_pointer (&self->table, g_free);
  g_clear_pointer (&self->identity_fields, g_strfreev);
  g_clear_pointer (&self->discriminator_field, g_free);
  g_clear_pointer (&self->discriminator_value, g_free);
  g_clear_pointer (&self->properties, g_ptr_array_unref);
  g_clear_pointer (&self->indexes, g_ptr_array_unref);
  g_clear_pointer (&self->relationships, g_object_unref);
  g_clear_pointer (&self->properties_by_name, g_hash_table_unref);
  g_clear_pointer (&self->properties_by_field, g_hash_table_unref);
  g_clear_pointer (&self->indexes_by_name, g_hash_table_unref);
  g_clear_pointer (&self->relationships_by_name, g_hash_table_unref);

  G_OBJECT_CLASS (gom_entity_spec_parent_class)->finalize (object);
}

static void
gom_entity_spec_class_init (GomEntitySpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gom_entity_spec_finalize;
}

static void
gom_entity_spec_init (GomEntitySpec *self)
{
  self->properties = g_ptr_array_new_with_free_func (g_object_unref);
  self->indexes = g_ptr_array_new_with_free_func (g_object_unref);
  self->relationships = g_list_store_new (GOM_TYPE_RELATIONSHIP_SPEC);
  self->properties_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  self->properties_by_field = g_hash_table_new (g_str_hash, g_str_equal);
  self->indexes_by_name = g_hash_table_new (g_str_hash, g_str_equal);
  self->relationships_by_name = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
_gom_property_spec_finalize (GObject *object)
{
  GomPropertySpec *self = (GomPropertySpec *)object;

  g_clear_pointer (&self->field, g_free);
  g_clear_pointer (&self->ref_table, g_free);
  g_clear_pointer (&self->ref_field, g_free);
  g_clear_pointer (&self->pspec, g_param_spec_unref);
  if (self->has_default_value)
    g_value_unset (&self->default_value);

  G_OBJECT_CLASS (gom_property_spec_parent_class)->finalize (object);
}

static void
gom_property_spec_class_init (GomPropertySpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gom_property_spec_finalize;
}

static void
gom_property_spec_init (GomPropertySpec *self)
{
}

static void
_gom_index_spec_finalize (GObject *object)
{
  GomIndexSpec *self = (GomIndexSpec *)object;

  g_clear_pointer (&self->fields, g_strfreev);

  G_OBJECT_CLASS (gom_index_spec_parent_class)->finalize (object);
}

static void
gom_index_spec_class_init (GomIndexSpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gom_index_spec_finalize;
}

static void
gom_index_spec_init (GomIndexSpec *self)
{
}

static void
_gom_relationship_spec_finalize (GObject *object)
{
  GomRelationshipSpec *self = (GomRelationshipSpec *)object;

  g_clear_pointer (&self->inverse_name, g_free);
  g_clear_pointer (&self->local_fields, g_strfreev);
  g_clear_pointer (&self->remote_fields, g_strfreev);
  g_clear_pointer (&self->join_relation, g_free);
  g_clear_pointer (&self->join_local_fields, g_strfreev);
  g_clear_pointer (&self->join_remote_fields, g_strfreev);

  G_OBJECT_CLASS (gom_relationship_spec_parent_class)->finalize (object);
}

static void
gom_relationship_spec_class_init (GomRelationshipSpecClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gom_relationship_spec_finalize;
}

static void
gom_relationship_spec_init (GomRelationshipSpec *self)
{
}

static GomEntitySpec *
_gom_entity_spec_new (const char  *name,
                      const char  *table,
                      GType        entity_type,
                      char       **identity_fields,
                      const char  *discriminator_field,
                      const char  *discriminator_value,
                      guint        version_added,
                      guint        version_removed)
{
  GomEntitySpec *self;

  self = g_object_new (GOM_TYPE_ENTITY_SPEC, NULL);
  gom_spec_set_name (GOM_SPEC (self), name);
  self->table = g_strdup (table);
  self->entity_type = entity_type;
  self->identity_fields = identity_fields != NULL ? g_strdupv (identity_fields) : NULL;
  self->discriminator_field = g_strdup (discriminator_field);
  self->discriminator_value = g_strdup (discriminator_value);
  self->version_added = version_added;
  self->version_removed = version_removed;

  return self;
}

static GomPropertySpec *
_gom_property_spec_new (const char   *name,
                        const char   *field,
                        const char   *ref_table,
                        const char   *ref_field,
                        GParamSpec   *pspec,
                        GType         value_type,
                        guint         version_added,
                        guint         version_removed,
                        gboolean      nonnull,
                        gboolean      unique,
                        gboolean      mapped,
                        const GValue *default_value,
                        guint         search_flags)
{
  GomPropertySpec *self;

  g_return_val_if_fail (value_type != G_TYPE_INVALID, NULL);

  self = g_object_new (GOM_TYPE_PROPERTY_SPEC, NULL);
  gom_spec_set_name (GOM_SPEC (self), name);
  self->field = g_strdup (field);
  self->ref_table = g_strdup (ref_table);
  self->ref_field = g_strdup (ref_field);
  self->pspec = pspec != NULL ? g_param_spec_ref (pspec) : NULL;
  self->value_type = value_type;
  self->version_added = version_added;
  self->version_removed = version_removed;
  self->nonnull = !!nonnull;
  self->unique = !!unique;
  self->mapped = !!mapped;
  self->search_flags = search_flags;

  if (default_value != NULL && G_VALUE_TYPE (default_value) != G_TYPE_INVALID)
    {
      g_value_init (&self->default_value, G_VALUE_TYPE (default_value));
      g_value_copy (default_value, &self->default_value);
      self->has_default_value = TRUE;
    }

  return self;
}

static GomIndexSpec *
_gom_index_spec_new (const char  *name,
                     char       **fields,
                     guint        version_added,
                     guint        version_removed,
                     gboolean     unique,
                     guint        search_flags)
{
  GomIndexSpec *self;

  self = g_object_new (GOM_TYPE_INDEX_SPEC, NULL);
  gom_spec_set_name (GOM_SPEC (self), name);
  self->fields = fields != NULL ? g_strdupv (fields) : NULL;
  self->version_added = version_added;
  self->version_removed = version_removed;
  self->unique = !!unique;
  self->search_flags = search_flags;

  return self;
}

static GomRelationshipSpec *
_gom_relationship_spec_new (const char                  *name,
                            GType                        target_type,
                            GomRelationshipCardinality   cardinality,
                            GomRelationshipStorage       storage,
                            const char                  *inverse_name,
                            char                       **local_fields,
                            char                       **remote_fields,
                            const char                  *join_relation,
                            char                       **join_local_fields,
                            char                       **join_remote_fields,
                            guint                        version_added,
                            guint                        version_removed,
                            guint                        min_count,
                            guint                        max_count,
                            gboolean                     optional,
                            gboolean                     ordered,
                            GomRelationshipDeleteRule    delete_rule)
{
  GomRelationshipSpec *self;

  self = g_object_new (GOM_TYPE_RELATIONSHIP_SPEC, NULL);
  gom_spec_set_name (GOM_SPEC (self), name);
  self->target_type = target_type;
  self->cardinality = cardinality;
  self->storage = storage;
  self->inverse_name = g_strdup (inverse_name);
  self->local_fields = local_fields != NULL ? g_strdupv (local_fields) : NULL;
  self->remote_fields = remote_fields != NULL ? g_strdupv (remote_fields) : NULL;
  self->join_relation = g_strdup (join_relation);
  self->join_local_fields = join_local_fields != NULL ? g_strdupv (join_local_fields) : NULL;
  self->join_remote_fields = join_remote_fields != NULL ? g_strdupv (join_remote_fields) : NULL;
  self->version_added = version_added;
  self->version_removed = version_removed;
  self->min_count = min_count;
  self->max_count = max_count;
  self->optional = !!optional;
  self->ordered = !!ordered;
  self->delete_rule = delete_rule;

  return self;
}

GomRegistryBuilder *
gom_registry_builder_new (void)
{
  GomRegistryBuilder *builder = g_new0 (GomRegistryBuilder, 1);

  g_atomic_ref_count_init (&builder->ref_count);
  builder->entity_types = g_array_new (FALSE, FALSE, sizeof (GType));
  builder->seen_types = g_hash_table_new (g_direct_hash, g_direct_equal);

  return builder;
}

GomRegistryBuilder *
gom_registry_builder_ref (GomRegistryBuilder *builder)
{
  g_return_val_if_fail (builder != NULL, NULL);

  g_atomic_ref_count_inc (&builder->ref_count);

  return builder;
}

void
gom_registry_builder_unref (GomRegistryBuilder *builder)
{
  g_return_if_fail (builder != NULL);

  if (!g_atomic_ref_count_dec (&builder->ref_count))
    return;

  _gom_registry_builder_finalize (builder);
  g_free (builder);
}

void
gom_registry_builder_add_entity_type (GomRegistryBuilder *builder,
                                      GType               entity_type)
{
  g_return_if_fail (builder != NULL);
  g_return_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));

  if (g_hash_table_contains (builder->seen_types, GSIZE_TO_POINTER (entity_type)))
    return;

  g_hash_table_add (builder->seen_types, GSIZE_TO_POINTER (entity_type));
  g_array_append_val (builder->entity_types, entity_type);
}

GomRegistry *
_gom_registry_new_empty (void)
{
  return g_object_new (GOM_TYPE_REGISTRY, NULL);
}

static void
_gom_registry_add_entity (GomRegistry   *self,
                          GomEntitySpec *entity)
{
  g_return_if_fail (GOM_IS_REGISTRY (self));
  g_return_if_fail (GOM_IS_ENTITY_SPEC (entity));

  _gom_registry_consider_version (self, entity->version_added);
  _gom_registry_consider_version (self, entity->version_removed);
  self->version = self->max_version;

  g_ptr_array_add (self->entities, g_object_ref (entity));
  g_hash_table_insert (self->by_type, GSIZE_TO_POINTER (entity->entity_type), entity);

  if (gom_spec_get_name (GOM_SPEC (entity)) != NULL)
    g_hash_table_insert (self->by_name, (gpointer)gom_spec_get_name (GOM_SPEC (entity)), entity);

  if (entity->table != NULL)
    g_hash_table_insert (self->by_table, entity->table, entity);
}

static void
_gom_entity_spec_add_property (GomEntitySpec   *entity,
                               GomPropertySpec *property)
{
  const char *name;
  const char *field;

  g_return_if_fail (GOM_IS_ENTITY_SPEC (entity));
  g_return_if_fail (GOM_IS_PROPERTY_SPEC (property));

  name = gom_spec_get_name (GOM_SPEC (property));
  field = property->field;

  g_ptr_array_add (entity->properties, g_object_ref (property));

  if (name != NULL)
    g_hash_table_insert (entity->properties_by_name, (gpointer)name, property);

  if (field != NULL)
    g_hash_table_insert (entity->properties_by_field, (gpointer)field, property);
}

static void
_gom_entity_spec_add_index (GomEntitySpec *entity,
                            GomIndexSpec  *index)
{
  const char *name;

  g_return_if_fail (GOM_IS_ENTITY_SPEC (entity));
  g_return_if_fail (GOM_IS_INDEX_SPEC (index));

  name = gom_spec_get_name (GOM_SPEC (index));

  g_ptr_array_add (entity->indexes, g_object_ref (index));

  if (name != NULL)
    g_hash_table_insert (entity->indexes_by_name, (gpointer)name, index);
}

static void
_gom_entity_spec_add_relationship (GomEntitySpec       *entity,
                                   GomRelationshipSpec *relationship)
{
  const char *name;

  g_return_if_fail (GOM_IS_ENTITY_SPEC (entity));
  g_return_if_fail (GOM_IS_RELATIONSHIP_SPEC (relationship));

  g_list_store_append (entity->relationships, relationship);

  if ((name = gom_spec_get_name (GOM_SPEC (relationship))))
    g_hash_table_insert (entity->relationships_by_name, (gpointer)name, relationship);
}

static void
_gom_registry_add_index_from_property (GomRegistry           *registry,
                                       GomEntitySpec         *entity,
                                       GomEntityPropertyInfo *prop)
{
  const char *field_name = prop->field_name != NULL ? prop->field_name : prop->property_name;
  const char *fields[] = { field_name, NULL };
  gboolean indexed = (prop->search_flags & GOM_SEARCH_INDEXED) != 0;
  g_autoptr(GomIndexSpec) index = NULL;

  if (prop->ignored)
    return;

  if (!prop->unique && !indexed)
    return;

  index = _gom_index_spec_new (prop->property_name,
                               (char **)fields,
                               prop->version_added,
                               prop->version_removed,
                               prop->unique,
                               prop->search_flags);

  _gom_registry_consider_version (registry, prop->version_added);
  _gom_registry_consider_version (registry, prop->version_removed);

  _gom_entity_spec_add_index (entity, index);
}

void
_gom_registry_add_entity_type (GomRegistry *registry,
                               GType        entity_type)
{
  GomEntityClassInfo *info;
  GomEntityClass *klass;
  const char *table;
  const char *discriminator_field;
  const char *discriminator_value;
  const char * const *identity_fields;
  g_autoptr(GHashTable) seen_properties = NULL;
  g_autoptr(GHashTable) seen_relationships = NULL;
  g_autoptr(GomEntitySpec) entity = NULL;
  guint version_added;
  guint version_removed;

  g_return_if_fail (GOM_IS_REGISTRY (registry));
  g_return_if_fail (registry->frozen == FALSE);
  g_return_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY));

  klass = g_type_class_get (entity_type);
  info = _gom_entity_class_get_info (klass, TRUE);

  table = gom_entity_class_get_relation (klass);
  discriminator_field = gom_entity_class_get_discriminator_field (klass);
  discriminator_value = gom_entity_class_get_discriminator_value (klass);
  identity_fields = gom_entity_class_get_identity_fields (klass);
  version_added = gom_entity_class_get_version_added (klass);
  version_removed = gom_entity_class_get_version_removed (klass);

  if (table == NULL || *table == '\0')
    g_warning ("%s is missing a relation name", g_type_name (entity_type));

  if (version_added == 0)
    g_warning ("%s is missing version_added", g_type_name (entity_type));

  entity = _gom_entity_spec_new (g_type_name (entity_type),
                                 table,
                                 entity_type,
                                 (char **)identity_fields,
                                 discriminator_field,
                                 discriminator_value,
                                 version_added,
                                 version_removed);

  seen_properties = g_hash_table_new (g_str_hash, g_str_equal);
  seen_relationships = g_hash_table_new (g_str_hash, g_str_equal);

  {
    g_autofree GParamSpec **properties = NULL;
    guint n_properties = 0;

    properties = g_object_class_list_properties (G_OBJECT_CLASS (klass), &n_properties);

    for (guint i = 0; i < n_properties; i++)
      {
        const char *property_name = g_param_spec_get_name (properties[i]);
        GomEntityPropertyInfo *prop;
        const char *field_name = property_name;
        const char *ref_table = NULL;
        const char *ref_field = NULL;
        g_auto(GValue) default_value = G_VALUE_INIT;
        g_autoptr(GomPropertySpec) spec = NULL;
        GType owner_type = properties[i]->owner_type;
        guint property_version_added = version_added;
        guint property_version_removed = version_removed;
        gboolean nonnull = FALSE;
        gboolean unique = FALSE;
        gboolean mapped = TRUE;
        guint search_flags = 0;
        GType value_type = G_TYPE_INVALID;

        if (property_name == NULL || g_hash_table_contains (seen_properties, property_name))
          continue;

        if (owner_type == GOM_TYPE_ENTITY || !g_type_is_a (owner_type, GOM_TYPE_ENTITY))
          continue;

        if ((properties[i]->flags & G_PARAM_READABLE) == 0)
          continue;

        if ((value_type = G_PARAM_SPEC_VALUE_TYPE (properties[i])) == G_TYPE_INVALID)
          continue;

        if ((prop = _gom_entity_class_get_property (klass, property_name, FALSE)))
          {
            field_name = prop->field_name != NULL ? prop->field_name : property_name;
            ref_table = prop->ref_table;
            ref_field = prop->ref_field;
            property_version_added = prop->version_added;
            property_version_removed = prop->version_removed;
            nonnull = prop->nonnull;
            unique = prop->unique;
            mapped = !prop->ignored;
            search_flags = prop->search_flags;
          }

        g_value_init (&default_value, value_type);
        g_param_value_set_default (properties[i], &default_value);

        if (!(spec = _gom_property_spec_new (property_name,
                                             field_name,
                                             ref_table,
                                             ref_field,
                                             properties[i],
                                             value_type,
                                             property_version_added,
                                             property_version_removed,
                                             nonnull,
                                             unique,
                                             mapped,
                                             &default_value,
                                             search_flags)))
          continue;

        g_hash_table_add (seen_properties, (gpointer)property_name);
        _gom_registry_consider_version (registry, property_version_added);
        _gom_registry_consider_version (registry, property_version_removed);
        _gom_entity_spec_add_property (entity, spec);

        if (prop != NULL)
          _gom_registry_add_index_from_property (registry, entity, prop);
      }
  }

  for (GomEntityClassInfo *iter = info; iter != NULL; iter = iter->parent_info)
    {
      for (GomEntityPropertyInfo *prop = iter->properties; prop; prop = prop->next)
        {
          const char *property_name = prop->property_name;

          if (property_name == NULL || g_hash_table_contains (seen_properties, property_name))
            continue;

          g_warning ("%s.%s is not a property for entity registration",
                     g_type_name (entity_type),
                     property_name);
        }
    }

  for (GomEntityClassInfo *iter = info; iter != NULL; iter = iter->parent_info)
    {
      for (GomEntityRelationshipInfo *relationship = iter->relationships;
           relationship != NULL;
           relationship = relationship->next)
        {
          const char *relationship_name = relationship->name;
          g_autoptr(GomRelationshipSpec) spec = NULL;

          if (relationship_name == NULL || g_hash_table_contains (seen_relationships, relationship_name))
            continue;

          g_hash_table_add (seen_relationships, (gpointer)relationship_name);

          spec = _gom_relationship_spec_new (relationship_name,
                                             relationship->target_type,
                                             relationship->cardinality,
                                             relationship->storage,
                                             relationship->inverse_name,
                                             relationship->local_fields,
                                             relationship->remote_fields,
                                             relationship->join_relation,
                                             relationship->join_local_fields,
                                             relationship->join_remote_fields,
                                             relationship->version_added,
                                             relationship->version_removed,
                                             relationship->min_count,
                                             relationship->max_count,
                                             relationship->optional,
                                             relationship->ordered,
                                             relationship->delete_rule);

          _gom_registry_consider_version (registry, relationship->version_added);
          _gom_registry_consider_version (registry, relationship->version_removed);

          _gom_entity_spec_add_relationship (entity, spec);
        }
    }

  _gom_registry_add_entity (registry, entity);
}

/**
 * gom_registry_builder_build:
 * @builder: a [struct@Gom.RegistryBuilder]
 *
 * Process the builder to create an immutable registry.
 *
 * Returns: (transfer full): a [class@Gom.Registry]
 */
GomRegistry *
gom_registry_builder_build (GomRegistryBuilder *builder)
{
  g_autoptr(GomRegistry) registry = NULL;

  g_return_val_if_fail (builder != NULL, NULL);

  registry = _gom_registry_new_empty ();

  for (guint i = 0; i < builder->entity_types->len; i++)
    {
      GType entity_type = g_array_index (builder->entity_types, GType, i);

      _gom_registry_add_entity_type (registry, entity_type);
    }

  gom_registry_validate_relationships (registry);
  registry->frozen = TRUE;

  return g_steal_pointer (&registry);
}

static GomPropertySpec *
_gom_property_spec_snapshot (GomPropertySpec *property,
                             guint            version)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (property), NULL);
  g_return_val_if_fail (property->value_type != G_TYPE_INVALID, NULL);

  if (!_gom_version_visible (version, property->version_added, property->version_removed))
    return NULL;

  return _gom_property_spec_new (gom_spec_get_name (GOM_SPEC (property)),
                                 property->field,
                                 property->ref_table,
                                 property->ref_field,
                                 property->pspec,
                                 property->value_type,
                                 property->version_added,
                                 property->version_removed,
                                 property->nonnull,
                                 property->unique,
                                 property->mapped,
                                 property->has_default_value ? &property->default_value : NULL,
                                 property->search_flags);
}

static GomIndexSpec *
_gom_index_spec_snapshot (GomIndexSpec *index,
                          guint         version)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (index), NULL);

  if (!_gom_version_visible (version, index->version_added, index->version_removed))
    return NULL;

  return _gom_index_spec_new (gom_spec_get_name (GOM_SPEC (index)),
                              index->fields,
                              index->version_added,
                              index->version_removed,
                              index->unique,
                              index->search_flags);
}

static GomEntitySpec *
_gom_entity_spec_snapshot (GomEntitySpec *entity,
                           guint          version)
{
  g_autoptr(GomEntitySpec) snapshot = NULL;

  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (entity), NULL);

  if (!_gom_version_visible (version, entity->version_added, entity->version_removed))
    return NULL;

  snapshot = _gom_entity_spec_new (gom_spec_get_name (GOM_SPEC (entity)),
                                   entity->table,
                                   entity->entity_type,
                                   entity->identity_fields,
                                   entity->discriminator_field,
                                   entity->discriminator_value,
                                   entity->version_added,
                                   entity->version_removed);

  for (guint i = 0; i < entity->properties->len; i++)
    {
      GomPropertySpec *prop = g_ptr_array_index (entity->properties, i);
      g_autoptr(GomPropertySpec) prop_snapshot = NULL;

      if ((prop_snapshot = _gom_property_spec_snapshot (prop, version)))
        _gom_entity_spec_add_property (snapshot, prop_snapshot);
    }

  for (guint i = 0; i < entity->indexes->len; i++)
    {
      GomIndexSpec *index = g_ptr_array_index (entity->indexes, i);
      g_autoptr(GomIndexSpec) index_snapshot = NULL;

      if ((index_snapshot = _gom_index_spec_snapshot (index, version)))
        _gom_entity_spec_add_index (snapshot, index_snapshot);
    }

  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (entity->relationships)); i++)
    {
      g_autoptr(GomRelationshipSpec) relationship = NULL;
      g_autoptr(GomRelationshipSpec) relationship_snapshot = NULL;

      if (!(relationship = g_list_model_get_item (G_LIST_MODEL (entity->relationships), i)))
        continue;

      if (!_gom_version_visible (version, relationship->version_added, relationship->version_removed))
        continue;

      relationship_snapshot = _gom_relationship_spec_new (gom_spec_get_name (GOM_SPEC (relationship)),
                                                           relationship->target_type,
                                                           relationship->cardinality,
                                                           relationship->storage,
                                                           relationship->inverse_name,
                                                           relationship->local_fields,
                                                           relationship->remote_fields,
                                                           relationship->join_relation,
                                                           relationship->join_local_fields,
                                                           relationship->join_remote_fields,
                                                           relationship->version_added,
                                                           relationship->version_removed,
                                                           relationship->min_count,
                                                           relationship->max_count,
                                                           relationship->optional,
                                                           relationship->ordered,
                                                           relationship->delete_rule);

      _gom_entity_spec_add_relationship (snapshot, relationship_snapshot);
    }

  return g_steal_pointer (&snapshot);
}

/**
 * gom_registry_snapshot:
 * @registry: a [class@Gom.Registry]
 * @version: version to snapshot
 *
 * Snapshots a registry only up to a certain version.
 *
 * This is useful for entity migrators that may want to compare what a
 * registry schema looks like at two specific version points.
 *
 * Returns: (transfer full): a [class@Gom.Registry]
 */
GomRegistry *
gom_registry_snapshot (GomRegistry *registry,
                       guint        version)
{
  g_autoptr(GomRegistry) snapshot = NULL;

  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);

  if (version > registry->max_version)
    version = registry->max_version;

  snapshot = _gom_registry_new_empty ();
  snapshot->version = version;
  snapshot->max_version = version;

  for (guint i = 0; i < registry->entities->len; i++)
    {
      GomEntitySpec *entity = g_ptr_array_index (registry->entities, i);
      g_autoptr(GomEntitySpec) entity_snapshot = NULL;

      if ((entity_snapshot = _gom_entity_spec_snapshot (entity, version)))
        _gom_registry_add_entity (snapshot, entity_snapshot);
    }

  snapshot->version = version;
  snapshot->max_version = version;

  return g_steal_pointer (&snapshot);
}

const GomEntitySpec * const *
gom_registry_list_entities (GomRegistry *registry,
                            guint       *n_entities)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (n_entities != NULL, NULL);

  *n_entities = registry->entities->len;
  return (const GomEntitySpec * const *)registry->entities->pdata;
}

guint
gom_registry_get_version (GomRegistry *registry)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), 0);

  return registry->version;
}

guint
gom_registry_get_max_version (GomRegistry *registry)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), 0);

  return registry->max_version;
}

const char *
gom_entity_spec_get_name (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);

  return gom_spec_get_name (GOM_SPEC (self));
}

const char *
gom_entity_spec_get_table (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);

  return self->table;
}

GType
gom_entity_spec_get_entity_type (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), G_TYPE_INVALID);

  return self->entity_type;
}

const char * const *
gom_entity_spec_get_identity_fields (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);

  return (const char * const *)self->identity_fields;
}

const char *
gom_entity_spec_get_discriminator_field (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);

  return self->discriminator_field;
}

const char *
gom_entity_spec_get_discriminator_value (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);

  return self->discriminator_value;
}

guint
gom_entity_spec_get_version_added (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), 0);

  return self->version_added;
}

guint
gom_entity_spec_get_version_removed (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), 0);

  return self->version_removed;
}

const GomPropertySpec * const *
gom_entity_spec_list_properties (GomEntitySpec *self,
                                 guint         *n_properties)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);
  g_return_val_if_fail (n_properties != NULL, NULL);

  *n_properties = self->properties->len;
  return (const GomPropertySpec * const *)self->properties->pdata;
}

const GomIndexSpec * const *
gom_entity_spec_list_indexes (GomEntitySpec *self,
                              guint         *n_indexes)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);
  g_return_val_if_fail (n_indexes != NULL, NULL);

  *n_indexes = self->indexes->len;
  return (const GomIndexSpec * const *)self->indexes->pdata;
}

/**
 * gom_entity_spec_list_relationships:
 * @self: a [class@Gom.EntitySpec]
 *
 * Returns the relationships defined on the entity.
 *
 * Returns: (transfer full): a [iface@Gio.ListModel] of
 *   [class@Gom.RelationshipSpec]
 */
GListModel *
gom_entity_spec_list_relationships (GomEntitySpec *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (self), NULL);

  return G_LIST_MODEL (g_object_ref (self->relationships));
}

const char *
gom_property_spec_get_name (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), NULL);

  return gom_spec_get_name (GOM_SPEC (self));
}

const char *
gom_property_spec_get_field (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), NULL);

  return self->field;
}

const char *
gom_property_spec_get_reference_table (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), NULL);

  return self->ref_table;
}

const char *
gom_property_spec_get_reference_field (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), NULL);

  return self->ref_field;
}

GType
gom_property_spec_get_value_type (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), G_TYPE_INVALID);

  return self->value_type;
}

guint
gom_property_spec_get_version_added (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), 0);

  return self->version_added;
}

guint
gom_property_spec_get_version_removed (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), 0);

  return self->version_removed;
}

gboolean
gom_property_spec_get_nonnull (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), FALSE);

  return self->nonnull;
}

gboolean
gom_property_spec_get_unique (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), FALSE);

  return self->unique;
}

const GValue *
gom_property_spec_get_default_value (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), NULL);

  return self->has_default_value ? &self->default_value : NULL;
}

gboolean
gom_property_spec_get_mapped (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), FALSE);

  return self->mapped;
}

guint
gom_property_spec_get_search_flags (GomPropertySpec *self)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (self), 0);

  return self->search_flags;
}

GParamSpec *
_gom_property_spec_get_pspec (GomPropertySpec *property)
{
  g_return_val_if_fail (GOM_IS_PROPERTY_SPEC (property), NULL);

  return property->pspec;
}

const char *
gom_index_spec_get_name (GomIndexSpec *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (self), NULL);

  return gom_spec_get_name (GOM_SPEC (self));
}

const char * const *
gom_index_spec_get_fields (GomIndexSpec *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (self), NULL);

  return (const char * const *)self->fields;
}

guint
gom_index_spec_get_version_added (GomIndexSpec *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (self), 0);

  return self->version_added;
}

guint
gom_index_spec_get_version_removed (GomIndexSpec *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (self), 0);

  return self->version_removed;
}

gboolean
gom_index_spec_get_unique (GomIndexSpec *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (self), FALSE);

  return self->unique;
}

guint
gom_index_spec_get_search_flags (GomIndexSpec *self)
{
  g_return_val_if_fail (GOM_IS_INDEX_SPEC (self), 0);

  return self->search_flags;
}

const char *
gom_relationship_spec_get_name (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return gom_spec_get_name (GOM_SPEC (self));
}

GType
gom_relationship_spec_get_target_type (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), G_TYPE_INVALID);

  return self->target_type;
}

GomRelationshipCardinality
gom_relationship_spec_get_cardinality (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), GOM_RELATIONSHIP_CARDINALITY_TO_ONE);

  return self->cardinality;
}

GomRelationshipStorage
gom_relationship_spec_get_storage (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), GOM_RELATIONSHIP_STORAGE_FK);

  return self->storage;
}

const char *
gom_relationship_spec_get_inverse_name (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return self->inverse_name;
}

const char * const *
gom_relationship_spec_get_local_fields (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return (const char * const *)self->local_fields;
}

const char * const *
gom_relationship_spec_get_remote_fields (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return (const char * const *)self->remote_fields;
}

const char *
gom_relationship_spec_get_join_relation (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return self->join_relation;
}

const char * const *
gom_relationship_spec_get_join_local_fields (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return (const char * const *)self->join_local_fields;
}

const char * const *
gom_relationship_spec_get_join_remote_fields (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), NULL);

  return (const char * const *)self->join_remote_fields;
}

gboolean
gom_relationship_spec_get_optional (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), FALSE);

  return self->optional;
}

gboolean
gom_relationship_spec_get_ordered (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), FALSE);

  return self->ordered;
}

guint
gom_relationship_spec_get_min_count (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), 0);

  return self->min_count;
}

guint
gom_relationship_spec_get_max_count (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), 0);

  return self->max_count;
}

GomRelationshipDeleteRule
gom_relationship_spec_get_delete_rule (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), GOM_RELATIONSHIP_DELETE_NO_ACTION);

  return self->delete_rule;
}

guint
gom_relationship_spec_get_version_added (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), 0);

  return self->version_added;
}

guint
gom_relationship_spec_get_version_removed (GomRelationshipSpec *self)
{
  g_return_val_if_fail (GOM_IS_RELATIONSHIP_SPEC (self), 0);

  return self->version_removed;
}

const GomEntitySpec *
_gom_registry_lookup_entity_by_type (GomRegistry *registry,
                                     GType        entity_type)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);

  return g_hash_table_lookup (registry->by_type, GSIZE_TO_POINTER (entity_type));
}

const GomEntitySpec *
_gom_registry_lookup_entity_by_name (GomRegistry *registry,
                                     const char  *name)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (registry->by_name, name);
}

const GomEntitySpec *
_gom_registry_lookup_entity_by_table (GomRegistry *registry,
                                      const char  *table)
{
  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (table != NULL, NULL);

  return g_hash_table_lookup (registry->by_table, table);
}

const GomPropertySpec *
_gom_entity_spec_lookup_property_by_name (GomEntitySpec *entity,
                                          const char    *name)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (entity), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (entity->properties_by_name, name);
}

const GomPropertySpec *
_gom_entity_spec_lookup_property_by_field (GomEntitySpec *entity,
                                           const char    *field)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (entity), NULL);
  g_return_val_if_fail (field != NULL, NULL);

  return g_hash_table_lookup (entity->properties_by_field, field);
}

const GomIndexSpec *
_gom_entity_spec_lookup_index_by_name (GomEntitySpec *entity,
                                       const char    *name)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (entity), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (entity->indexes_by_name, name);
}

const GomRelationshipSpec *
_gom_entity_spec_lookup_relationship_by_name (GomEntitySpec *entity,
                                              const char    *name)
{
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (entity), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (entity->relationships_by_name, name);
}
