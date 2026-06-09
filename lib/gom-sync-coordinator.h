/* gom-sync-coordinator.h
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

#define GOM_TYPE_SYNC_COORDINATOR (gom_sync_coordinator_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomSyncCoordinator, gom_sync_coordinator, GOM, SYNC_COORDINATOR, GObject)

GOM_AVAILABLE_IN_ALL
GomSyncCoordinator *gom_sync_coordinator_new                 (GomSyncTransport   *transport,
                                                              GomMergePolicy     *merge_policy);
GOM_AVAILABLE_IN_ALL
GomSyncTransport   *gom_sync_coordinator_get_transport       (GomSyncCoordinator *self);
GOM_AVAILABLE_IN_ALL
GomMergePolicy     *gom_sync_coordinator_get_merge_policy    (GomSyncCoordinator *self);
GOM_AVAILABLE_IN_ALL
GomSyncTransport   *gom_sync_coordinator_dup_transport       (GomSyncCoordinator *self);
GOM_AVAILABLE_IN_ALL
GomMergePolicy     *gom_sync_coordinator_dup_merge_policy    (GomSyncCoordinator *self);
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_sync_coordinator_stage_local_change  (GomSyncCoordinator *self,
                                                              GomRepository      *repository,
                                                              GomSession         *session,
                                                              GomEntity          *entity,
                                                              GomDelta           *delta) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_sync_coordinator_push                (GomSyncCoordinator *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_sync_coordinator_pull                (GomSyncCoordinator *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_sync_coordinator_sync                (GomSyncCoordinator *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture          *gom_sync_coordinator_merge_remote_change (GomSyncCoordinator *self,
                                                              GomRepository      *repository,
                                                              GomDelta           *local_delta,
                                                              GomDelta           *remote_delta) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
