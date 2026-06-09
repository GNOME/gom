/* gom-sync-transport.h
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

#define GOM_TYPE_SYNC_TRANSPORT (gom_sync_transport_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GomSyncTransport, gom_sync_transport, GOM, SYNC_TRANSPORT, GObject)

struct _GomSyncTransportClass
{
  GObjectClass parent_class;

  DexFuture *(*stage_local_change) (GomSyncTransport   *self,
                                    GomSyncCoordinator *coordinator,
                                    GomRepository      *repository,
                                    GomSession         *session,
                                    GomEntity          *entity,
                                    GomDelta           *delta);
  DexFuture *(*push)               (GomSyncTransport   *self,
                                    GomSyncCoordinator *coordinator);
  DexFuture *(*pull)               (GomSyncTransport   *self,
                                    GomSyncCoordinator *coordinator);

  /*< private >*/
  gpointer _reserved[20];
};

GOM_AVAILABLE_IN_ALL
DexFuture *gom_sync_transport_stage_local_change (GomSyncTransport   *self,
                                                  GomSyncCoordinator *coordinator,
                                                  GomRepository      *repository,
                                                  GomSession         *session,
                                                  GomEntity          *entity,
                                                  GomDelta           *delta) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture *gom_sync_transport_push               (GomSyncTransport   *self,
                                                  GomSyncCoordinator *coordinator) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture *gom_sync_transport_pull               (GomSyncTransport   *self,
                                                  GomSyncCoordinator *coordinator) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
