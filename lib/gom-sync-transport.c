/* gom-sync-transport.c
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

#include "gom-delta.h"
#include "gom-entity.h"
#include "gom-repository.h"
#include "gom-session.h"
#include "gom-sync-coordinator.h"
#include "gom-sync-transport.h"

G_DEFINE_TYPE (GomSyncTransport, gom_sync_transport, G_TYPE_OBJECT)

static void
gom_sync_transport_class_init (GomSyncTransportClass *klass)
{
}

static void
gom_sync_transport_init (GomSyncTransport *self)
{
}

/**
 * gom_sync_transport_stage_local_change:
 * @self: a [class@Gom.SyncTransport]
 * @coordinator: a [class@Gom.SyncCoordinator]
 * @repository: a [class@Gom.Repository]
 * @session: (nullable): a [class@Gom.Session]
 * @entity: a [class@Gom.Entity]
 * @delta: a [class@Gom.Delta]
 *
 * Allows @self to record @delta as part of @session's local commit.
 *
 * The default implementation is a no-op so applications may attach a
 * coordinator before a transport backend has implemented durable history.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to `TRUE`
 */
DexFuture *
gom_sync_transport_stage_local_change (GomSyncTransport   *self,
                                       GomSyncCoordinator *coordinator,
                                       GomRepository      *repository,
                                       GomSession         *session,
                                       GomEntity          *entity,
                                       GomDelta           *delta)
{
  dex_return_error_if_fail (GOM_IS_SYNC_TRANSPORT (self));
  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (coordinator));
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (!session || GOM_IS_SESSION (session));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));
  dex_return_error_if_fail (GOM_IS_DELTA (delta));

  if (GOM_SYNC_TRANSPORT_GET_CLASS (self)->stage_local_change)
    return GOM_SYNC_TRANSPORT_GET_CLASS (self)->stage_local_change (self, coordinator, repository, session, entity, delta);

  return dex_future_new_true ();
}

/**
 * gom_sync_transport_push:
 * @self: a [class@Gom.SyncTransport]
 * @coordinator: a [class@Gom.SyncCoordinator]
 *
 * Pushes locally staged sync changes to the external service.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the push
 *   attempt has completed.
 */
DexFuture *
gom_sync_transport_push (GomSyncTransport   *self,
                         GomSyncCoordinator *coordinator)
{
  dex_return_error_if_fail (GOM_IS_SYNC_TRANSPORT (self));
  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (coordinator));

  if (GOM_SYNC_TRANSPORT_GET_CLASS (self)->push)
    return GOM_SYNC_TRANSPORT_GET_CLASS (self)->push (self, coordinator);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Sync transport does not support push");
}

/**
 * gom_sync_transport_pull:
 * @self: a [class@Gom.SyncTransport]
 * @coordinator: a [class@Gom.SyncCoordinator]
 *
 * Pulls remote sync changes from the external service.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when the pull
 *   attempt has completed.
 */
DexFuture *
gom_sync_transport_pull (GomSyncTransport   *self,
                         GomSyncCoordinator *coordinator)
{
  dex_return_error_if_fail (GOM_IS_SYNC_TRANSPORT (self));
  dex_return_error_if_fail (GOM_IS_SYNC_COORDINATOR (coordinator));

  if (GOM_SYNC_TRANSPORT_GET_CLASS (self)->pull)
    return GOM_SYNC_TRANSPORT_GET_CLASS (self)->pull (self, coordinator);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Sync transport does not support pull");
}
