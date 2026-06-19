/* gom-meta.h
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

#pragma once

#include <gio/gio.h>

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_REGISTRY (gom_registry_get_type())
#define GOM_TYPE_REGISTRY_BUILDER (gom_registry_builder_get_type())
#define GOM_TYPE_SPEC (gom_spec_get_type())
#define GOM_TYPE_ENTITY_SPEC (gom_entity_spec_get_type())
#define GOM_TYPE_PROPERTY_SPEC (gom_property_spec_get_type())
#define GOM_TYPE_INDEX_SPEC (gom_index_spec_get_type())
#define GOM_TYPE_RELATIONSHIP_SPEC (gom_relationship_spec_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomRegistry, gom_registry, GOM, REGISTRY, GObject)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomSpec, gom_spec, GOM, SPEC, GObject)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomEntitySpec, gom_entity_spec, GOM, ENTITY_SPEC, GomSpec)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomPropertySpec, gom_property_spec, GOM, PROPERTY_SPEC, GomSpec)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomIndexSpec, gom_index_spec, GOM, INDEX_SPEC, GomSpec)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomRelationshipSpec, gom_relationship_spec, GOM, RELATIONSHIP_SPEC, GomSpec)

GOM_AVAILABLE_IN_ALL
GType                          gom_registry_builder_get_type                (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GomRegistryBuilder            *gom_registry_builder_new                     (void);
GOM_AVAILABLE_IN_ALL
GomRegistryBuilder            *gom_registry_builder_ref                     (GomRegistryBuilder  *builder);
GOM_AVAILABLE_IN_ALL
void                           gom_registry_builder_unref                   (GomRegistryBuilder  *builder);
GOM_AVAILABLE_IN_ALL
void                           gom_registry_builder_add_entity_type         (GomRegistryBuilder  *builder,
                                                                             GType                entity_type);
GOM_AVAILABLE_IN_ALL
GomRegistry                   *gom_registry_builder_build                   (GomRegistryBuilder  *builder);
GOM_AVAILABLE_IN_ALL
GomRegistry                   *gom_registry_snapshot                        (GomRegistry         *registry,
                                                                             guint                version);
GOM_AVAILABLE_IN_ALL
const GomEntitySpec * const   *gom_registry_list_entities                   (GomRegistry         *registry,
                                                                             guint               *n_entities);
GOM_AVAILABLE_IN_ALL
guint                          gom_registry_get_version                     (GomRegistry         *registry);
GOM_AVAILABLE_IN_ALL
guint                          gom_registry_get_max_version                 (GomRegistry         *registry);
GOM_AVAILABLE_IN_ALL
const char                    *gom_spec_get_name                            (GomSpec             *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_entity_spec_get_name                     (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_entity_spec_get_table                    (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
GType                          gom_entity_spec_get_entity_type              (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
GomEntitySchemaRole            gom_entity_spec_get_schema_role              (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
const char * const            *gom_entity_spec_get_identity_fields          (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_entity_spec_get_discriminator_field      (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_entity_spec_get_discriminator_value      (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_entity_spec_get_version_added            (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_entity_spec_get_version_removed          (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
const GomPropertySpec * const *gom_entity_spec_list_properties              (GomEntitySpec       *self,
                                                                             guint               *n_properties);
GOM_AVAILABLE_IN_ALL
const GomIndexSpec * const    *gom_entity_spec_list_indexes                 (GomEntitySpec       *self,
                                                                             guint               *n_indexes);
GOM_AVAILABLE_IN_ALL
GListModel                    *gom_entity_spec_list_relationships           (GomEntitySpec       *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_property_spec_get_name                   (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_property_spec_get_field                  (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_property_spec_get_reference_table        (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_property_spec_get_reference_field        (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
GType                          gom_property_spec_get_value_type             (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_property_spec_get_version_added          (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_property_spec_get_version_removed        (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
gboolean                       gom_property_spec_get_nonnull                (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
gboolean                       gom_property_spec_get_unique                 (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
gboolean                       gom_property_spec_get_mapped                 (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
const GValue                  *gom_property_spec_get_default_value          (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_property_spec_get_search_flags           (GomPropertySpec     *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_relationship_spec_get_name               (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
GType                          gom_relationship_spec_get_target_type        (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
GomRelationshipCardinality     gom_relationship_spec_get_cardinality        (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
GomRelationshipStorage         gom_relationship_spec_get_storage            (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_relationship_spec_get_inverse_name       (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char * const            *gom_relationship_spec_get_local_fields       (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char * const            *gom_relationship_spec_get_remote_fields      (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_relationship_spec_get_join_relation      (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char * const            *gom_relationship_spec_get_join_local_fields  (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char * const            *gom_relationship_spec_get_join_remote_fields (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
gboolean                       gom_relationship_spec_get_optional           (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
gboolean                       gom_relationship_spec_get_ordered            (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_relationship_spec_get_min_count          (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_relationship_spec_get_max_count          (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
GomRelationshipDeleteRule      gom_relationship_spec_get_delete_rule        (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_relationship_spec_get_version_added      (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_relationship_spec_get_version_removed    (GomRelationshipSpec *self);
GOM_AVAILABLE_IN_ALL
const char                    *gom_index_spec_get_name                      (GomIndexSpec        *self);
GOM_AVAILABLE_IN_ALL
const char * const            *gom_index_spec_get_fields                    (GomIndexSpec        *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_index_spec_get_version_added             (GomIndexSpec        *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_index_spec_get_version_removed           (GomIndexSpec        *self);
GOM_AVAILABLE_IN_ALL
gboolean                       gom_index_spec_get_unique                    (GomIndexSpec        *self);
GOM_AVAILABLE_IN_ALL
guint                          gom_index_spec_get_search_flags              (GomIndexSpec        *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomRegistryBuilder, gom_registry_builder_unref)

G_END_DECLS
