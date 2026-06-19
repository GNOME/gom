/* gom-driver-private.h
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

#include "gom-driver.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

typedef GomDriver *(*GomDriverConstructor) (const char        *uri,
                                            GomDriverOptions  *options,
                                            GError           **error);

struct _GomDriver
{
  GObject parent_instance;
  int     repository_use_count;
};

struct _GomDriverClass
{
  GObjectClass parent_class;

  char      *(*dup_uri)                  (GomDriver            *self);
  DexFuture *(*query)                    (GomDriver            *self,
                                          GomRepository        *repository,
                                          GomQuery             *query,
                                          GomCursorFlags        flags);
  DexFuture *(*mutate)                   (GomDriver            *self,
                                          GomRegistry          *registry,
                                          GomMutation          *mutation);
  DexFuture *(*describe_relation)        (GomDriver            *self,
                                          GomRegistry          *registry,
                                          const char           *relation);
  DexFuture *(*list_relations)           (GomDriver            *self,
                                          GomRegistry          *registry);
  DexFuture *(*query_version)            (GomDriver            *self);
  DexFuture *(*migrate)                  (GomDriver            *self,
                                          GomRegistry          *current,
                                          GomRegistry          *next);
  DexFuture *(*execute_sql)              (GomDriver            *self,
                                          GBytes               *script);
  DexFuture *(*begin_session)            (GomDriver            *self,
                                          GomRepository        *repository);
  gboolean   (*supports_feature)         (GomDriver            *self,
                                          GomRepositoryFeature  feature);
  gboolean   (*supports_vector_distance) (GomDriver            *self,
                                          GomVectorFormat       format,
                                          GomVectorMetric       metric);
  DexFuture *(*rekey)                    (GomDriver            *self,
                                          GomDriverOptions     *options);
};

DexFuture *_gom_driver_query                    (GomDriver            *self,
                                                 GomRepository        *repository,
                                                 GomQuery             *query,
                                                 GomCursorFlags        flags) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_mutate                   (GomDriver            *self,
                                                 GomRegistry          *registry,
                                                 GomMutation          *mutation) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_describe_relation        (GomDriver            *self,
                                                 GomRegistry          *registry,
                                                 const char           *relation) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_list_relations           (GomDriver            *self,
                                                 GomRegistry          *registry) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_query_version            (GomDriver            *self) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_migrate                  (GomDriver            *self,
                                                 GomRegistry          *current,
                                                 GomRegistry          *next) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_execute_sql              (GomDriver            *self,
                                                 GBytes               *script) G_GNUC_WARN_UNUSED_RESULT;
DexFuture *_gom_driver_begin_session            (GomDriver            *self,
                                                 GomRepository        *repository) G_GNUC_WARN_UNUSED_RESULT;
gboolean   _gom_driver_supports_feature         (GomDriver            *self,
                                                 GomRepositoryFeature  feature);
gboolean   _gom_driver_supports_vector_distance (GomDriver            *self,
                                                 GomVectorFormat       format,
                                                 GomVectorMetric       metric);
void       _gom_driver_acquire_repository       (GomDriver            *self);
void       _gom_driver_release_repository       (GomDriver            *self);

G_END_DECLS
