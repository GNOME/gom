/* gom-session.h
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

#define GOM_TYPE_SESSION (gom_session_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomSession, gom_session, GOM, SESSION, GObject)
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_query                    (GomSession         *self,
                                                          GomQuery           *query) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_find_one                 (GomSession         *self,
                                                          GType               entity_type,
                                                          const char         *first_property,
                                                          ...) G_GNUC_NULL_TERMINATED G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_find_one_with_properties (GomSession         *self,
                                                          GType               entity_type,
                                                          guint               n_properties,
                                                          const char * const *properties,
                                                          const GValue       *values) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_list_entities            (GomSession         *self,
                                                          GType               entity_type,
                                                          GomExpression      *filter,
                                                          GomOrdering        *ordering) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_list_query               (GomSession         *self,
                                                          GomQuery           *query) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_list_records             (GomSession         *self,
                                                          GomQuery           *query) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_mutate                   (GomSession         *self,
                                                          GomMutation        *mutation) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_insert_entity            (GomSession         *self,
                                                          GomEntity          *entity) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_persist                  (GomSession         *self,
                                                          GomEntity          *entity) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_flush                    (GomSession         *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_commit                   (GomSession         *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_session_rollback                 (GomSession         *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
GomRepository      *gom_session_dup_repository           (GomSession         *self);
GOM_AVAILABLE_IN_ALL
GomSyncCoordinator *gom_session_dup_coordinator          (GomSession         *self);

G_END_DECLS
