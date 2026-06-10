/* gom-entity.h
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

#include <libdex.h>

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_ENTITY (gom_entity_get_type())
#define GOM_TYPE_ENTITY_LIFECYCLE (gom_entity_lifecycle_get_type())
#define GOM_TYPE_ENTITY_ORIGIN (gom_entity_origin_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GomEntity, gom_entity, GOM, ENTITY, GObject)

struct _GomEntityClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer _info;

  /*< public >*/
  GomEntity     *(*materialize)        (GomEntityClass      *klass,
                                        GomCursor           *cursor,
                                        const char * const  *property_names,
                                        const GValue        *property_values,
                                        guint                n_properties,
                                        GError             **error);
  GomDelta      *(*build_delta)        (GomEntity           *self,
                                        GError             **error);
  GomExpression *(*dup_identity_value) (GomEntity           *self,
                                        const char          *identity_field,
                                        GError             **error);
  void           (*attach)             (GomEntity           *self,
                                        GomSession          *session,
                                        char                *entity_key);
  void           (*detach)             (GomEntity           *self);
  gboolean       (*backfill_identity)  (GomEntity           *self,
                                        const char * const  *identity_fields,
                                        GomRecord           *record,
                                        GError             **error);

  /*< private >*/
  gpointer _reserved[16];
};

GOM_AVAILABLE_IN_ALL
GType               gom_entity_lifecycle_get_type                 (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GType               gom_entity_origin_get_type                    (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_entity_insert                             (GomEntity                  *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_entity_update                             (GomEntity                  *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_entity_delete                             (GomEntity                  *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_entity_load_related_entity                (GomEntity                  *self,
                                                                   const char                 *relationship_name) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_entity_load_related_model                 (GomEntity                  *self,
                                                                   const char                 *relationship_name) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
gboolean            gom_entity_rekey_session_identity             (GomEntity                  *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
void                gom_entity_set_repository                     (GomEntity                  *self,
                                                                   GomRepository              *repository);
GOM_AVAILABLE_IN_ALL
GomRepository      *gom_entity_dup_repository                     (GomEntity                  *self);
GOM_AVAILABLE_IN_ALL
GomEntityLifecycle  gom_entity_get_lifecycle                      (GomEntity                  *self);
GOM_AVAILABLE_IN_ALL
GomEntityOrigin     gom_entity_get_origin                         (GomEntity                  *self);
GOM_AVAILABLE_IN_ALL
GomDelta           *gom_entity_build_delta                        (GomEntity                  *self,
                                                                   GError                    **error);
GOM_AVAILABLE_IN_ALL
void                gom_entity_clear_change_tracking              (GomEntity                  *self);
GOM_AVAILABLE_IN_ALL
guint               gom_entity_class_get_version_added            (GomEntityClass             *klass);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_version_added            (GomEntityClass             *klass,
                                                                   guint                       version);
GOM_AVAILABLE_IN_ALL
guint               gom_entity_class_get_version_removed          (GomEntityClass             *klass);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_version_removed          (GomEntityClass             *klass,
                                                                   guint                       version);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_identity_fields          (GomEntityClass             *klass,
                                                                   const char * const         *identity_fields);
GOM_AVAILABLE_IN_ALL
const char * const *gom_entity_class_get_identity_fields          (GomEntityClass             *klass);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_identity_field           (GomEntityClass             *klass,
                                                                   const char                 *identity_field);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_relation                 (GomEntityClass             *klass,
                                                                   const char                 *relation);
GOM_AVAILABLE_IN_ALL
const char         *gom_entity_class_get_relation                 (GomEntityClass             *klass);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_discriminator_field      (GomEntityClass             *klass,
                                                                   const char                 *discriminator_field);
GOM_AVAILABLE_IN_ALL
const char         *gom_entity_class_get_discriminator_field      (GomEntityClass             *klass);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_set_discriminator_value      (GomEntityClass             *klass,
                                                                   const char                 *discriminator_value);
GOM_AVAILABLE_IN_ALL
const char         *gom_entity_class_get_discriminator_value      (GomEntityClass             *klass);
GOM_AVAILABLE_IN_ALL
gboolean            gom_entity_class_property_get_nonnull         (GomEntityClass             *klass,
                                                                   const char                 *property_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_nonnull         (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   gboolean                    nonnull);
GOM_AVAILABLE_IN_ALL
gboolean            gom_entity_class_property_get_unique          (GomEntityClass             *klass,
                                                                   const char                 *property_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_unique          (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   gboolean                    unique);
GOM_AVAILABLE_IN_ALL
gboolean            gom_entity_class_property_get_mapped          (GomEntityClass             *klass,
                                                                   const char                 *property_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_mapped          (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   gboolean                    mapped);
GOM_AVAILABLE_IN_ALL
GomSearchFlags      gom_entity_class_property_get_search_flags    (GomEntityClass             *klass,
                                                                   const char                 *property_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_search_flags    (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   GomSearchFlags              search_flags);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_version_added   (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   guint                       version_added);
GOM_AVAILABLE_IN_ALL
guint               gom_entity_class_property_get_version_added   (GomEntityClass             *klass,
                                                                   const char                 *property_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_version_removed (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   guint                       version_removed);
GOM_AVAILABLE_IN_ALL
guint               gom_entity_class_property_get_version_removed (GomEntityClass             *klass,
                                                                   const char                 *property_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_reference       (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   const char                 *ref_table,
                                                                   const char                 *ref_field);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_field_name      (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   const char                 *field_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_add_many_to_one              (GomEntityClass             *klass,
                                                                   const char                 *name,
                                                                   GType                       target_type,
                                                                   const char                 *local_field,
                                                                   const char                 *inverse_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_add_one_to_many              (GomEntityClass             *klass,
                                                                   const char                 *name,
                                                                   GType                       target_type,
                                                                   const char                 *foreign_field,
                                                                   const char                 *inverse_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_add_many_to_many             (GomEntityClass             *klass,
                                                                   const char                 *name,
                                                                   GType                       target_type,
                                                                   const char                 *join_relation,
                                                                   const char                 *join_local_field,
                                                                   const char                 *join_remote_field,
                                                                   const char                 *inverse_name);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_relationship_set_delete_rule (GomEntityClass             *klass,
                                                                   const char                 *name,
                                                                   GomRelationshipDeleteRule   delete_rule);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_byte_transform  (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   GomToBytesFunc              to_bytes_func,
                                                                   GomFromBytesFunc            from_bytes_func,
                                                                   gpointer                    user_data,
                                                                   GDestroyNotify              notify);
GOM_AVAILABLE_IN_ALL
void                gom_entity_class_property_set_vector          (GomEntityClass             *klass,
                                                                   const char                 *property_name,
                                                                   GomVectorFormat             format,
                                                                   guint                       dimensions);

G_END_DECLS
