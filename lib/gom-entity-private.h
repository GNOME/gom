/* gom-entity-private.h
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

#include "gom-entity.h"
#include "gom-delta.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

typedef struct _GomEntityPropertyInfo
{
  struct _GomEntityPropertyInfo *next;
  const char                    *property_name;
  const char                    *field_name;
  const char                    *ref_table;
  const char                    *ref_field;
  GomToBytesFunc                 to_bytes_func;
  GomFromBytesFunc               from_bytes_func;
  gpointer                       bytes_user_data;
  GDestroyNotify                 bytes_notify;
  guint                          version_added;
  guint                          version_removed;
  GomSearchFlags                 search_flags;
  guint                          nonnull : 1;
  guint                          unique : 1;
  guint                          ignored : 1;
} GomEntityPropertyInfo;

typedef struct _GomEntityRelationshipInfo
{
  struct _GomEntityRelationshipInfo  *next;
  const char                         *name;
  GType                               target_type;
  GomRelationshipCardinality          cardinality;
  GomRelationshipStorage              storage;
  const char                         *inverse_name;
  char                              **local_fields;
  char                              **remote_fields;
  const char                         *join_relation;
  char                              **join_local_fields;
  char                              **join_remote_fields;
  guint                               version_added;
  guint                               version_removed;
  guint                               min_count;
  guint                               max_count;
  GomRelationshipDeleteRule           delete_rule;
  guint                               optional : 1;
  guint                               ordered : 1;
} GomEntityRelationshipInfo;

typedef struct _GomEntityClassInfo
{
  GomEntityClass              *klass;
  struct _GomEntityClassInfo  *parent_info;
  const char                  *table;
  char                       **identity_fields;
  GomEntityPropertyInfo       *properties;
  GomEntityRelationshipInfo   *relationships;
  guint                        version_added;
  guint                        version_removed;
  const char                  *discriminator_field;
  const char                  *discriminator_value;
} GomEntityClassInfo;

GomEntityClassInfo        *_gom_entity_class_get_info            (GomEntityClass      *klass,
                                                                  gboolean             create);
GomEntityPropertyInfo     *_gom_entity_class_get_property        (GomEntityClass      *klass,
                                                                  const char          *property_name,
                                                                  gboolean             create);
GomEntityRelationshipInfo *_gom_entity_class_get_relationship    (GomEntityClass      *klass,
                                                                  const char          *name,
                                                                  gboolean             create);
gboolean                   gom_entity_dup_identity_value_is_set  (GomEntity           *self,
                                                                  GomEntityClass      *entity_class,
                                                                  const char          *identity_field,
                                                                  GomExpression      **out_identity_value,
                                                                  GError             **error);
gboolean                   gom_entity_get_property_storage_value (GomEntity           *self,
                                                                  GomEntityClass      *entity_class,
                                                                  GObjectClass        *object_class,
                                                                  const char          *property_name,
                                                                  GValue              *out_value,
                                                                  GError             **error);
void                       _gom_entity_capture_change_state      (GomEntity           *self,
                                                                  const char * const  *property_names,
                                                                  const GValue        *property_values,
                                                                  guint                n_properties,
                                                                  gboolean             complete);
GomDelta                  *_gom_entity_build_delta               (GomEntity           *self,
                                                                  GError             **error);
void                       _gom_entity_apply_delta               (GomEntity           *self,
                                                                  GomDelta            *delta,
                                                                  gboolean             complete);
gboolean                   _gom_entity_change_state_is_complete  (GomEntity           *self);
void                       _gom_entity_clear_change_state        (GomEntity           *self);
GomSession                *_gom_entity_dup_session               (GomEntity           *self);
char                      *_gom_entity_dup_session_key           (GomEntity           *self);
GList                     *_gom_entity_get_session_link          (GomEntity           *self);
GList                     *_gom_entity_get_pending_link          (GomEntity           *self);
GList                     *_gom_entity_get_dirty_link            (GomEntity           *self);
gboolean                   _gom_entity_is_pending                (GomEntity           *self);
void                       _gom_entity_set_pending               (GomEntity           *self,
                                                                  gboolean             pending);
gboolean                   _gom_entity_is_dirty                  (GomEntity           *self);
void                       _gom_entity_set_dirty                 (GomEntity           *self,
                                                                  gboolean             dirty);
void                       _gom_entity_attach                    (GomEntity           *self,
                                                                  GomSession          *session,
                                                                  char                *entity_key);
void                       _gom_entity_detach                    (GomEntity           *self);
void                       _gom_entity_track_changes             (GomEntity           *self,
                                                                  GomSession          *session);
void                       _gom_entity_untrack_changes           (GomEntity           *self);
void                       _gom_entity_set_origin                (GomEntity           *self,
                                                                  GomEntityOrigin      origin);
void                       _gom_entity_set_lifecycle             (GomEntity           *self,
                                                                  GomEntityLifecycle   lifecycle);

G_END_DECLS
