/* gom-repository.h
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

#define GOM_TYPE_REPOSITORY (gom_repository_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomRepository, gom_repository, GOM, REPOSITORY, GObject)

GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_new                      (GomDriver            *driver,
                                                             GomRegistry          *registry,
                                                             GomMigrator          *migrator) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_new_with_coordinator     (GomDriver            *driver,
                                                             GomRegistry          *registry,
                                                             GomMigrator          *migrator,
                                                             GomSyncCoordinator   *coordinator) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
void                gom_repository_register_entity_type     (GomRepository        *self,
                                                             GType                 entity_type);
GOM_AVAILABLE_IN_ALL
GType              *gom_repository_list_entity_types        (GomRepository        *self,
                                                             guint                *n_entity_types);
GOM_AVAILABLE_IN_ALL
GomDriver          *gom_repository_dup_driver               (GomRepository        *self);
GOM_AVAILABLE_IN_ALL
GomSyncCoordinator *gom_repository_dup_coordinator          (GomRepository        *self);
GOM_AVAILABLE_IN_ALL
gboolean            gom_repository_supports_feature         (GomRepository        *self,
                                                             GomRepositoryFeature  feature);
GOM_AVAILABLE_IN_ALL
gboolean            gom_repository_supports_vector_distance (GomRepository        *self,
                                                             GomVectorFormat       format,
                                                             GomVectorMetric       metric);
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_begin_session            (GomRepository        *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_query                    (GomRepository        *self,
                                                             GomQuery             *query);
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_count                    (GomRepository        *self,
                                                             GomQuery             *query) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_find_one                 (GomRepository        *self,
                                                             GType                 entity_type,
                                                             const char           *first_property,
                                                             ...) G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_find_one_with_properties (GomRepository        *self,
                                                             GType                 entity_type,
                                                             guint                 n_properties,
                                                             const char * const   *properties,
                                                             const GValue         *values) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_list_entities            (GomRepository        *self,
                                                             GType                 entity_type,
                                                             GomExpression        *filter,
                                                             GomOrdering          *ordering);
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_list_query               (GomRepository        *self,
                                                             GomQuery             *query) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_list_records             (GomRepository        *self,
                                                             GomQuery             *query) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_mutate                   (GomRepository        *self,
                                                             GomMutation          *mutation);
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_insert_entity            (GomRepository        *self,
                                                             GomEntity            *entity) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_describe_relation        (GomRepository        *self,
                                                             const char           *relation);
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_repository_list_relations           (GomRepository        *self);

G_END_DECLS
