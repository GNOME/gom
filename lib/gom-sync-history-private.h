/* gom-sync-history-private.h
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
#include "gom-types-private.h"

G_BEGIN_DECLS

#define GOM_TYPE_SYNC_HISTORY_CHANGE (gom_sync_history_change_get_type())
#define GOM_TYPE_SYNC_HISTORY_VALUE  (gom_sync_history_value_get_type())

GOM_DECLARE_INTERNAL_TYPE (GomSyncHistoryChange, gom_sync_history_change, GOM, SYNC_HISTORY_CHANGE, GomEntity)
GOM_DECLARE_INTERNAL_TYPE (GomSyncHistoryValue, gom_sync_history_value, GOM, SYNC_HISTORY_VALUE, GomEntity)

DexFuture    *_gom_sync_history_ensure_schema          (GomRepository        *repository) G_GNUC_WARN_UNUSED_RESULT;
DexFuture    *_gom_sync_history_stage_local_change     (GomRepository        *repository,
                                                        GomSession           *session,
                                                        GomEntity            *entity,
                                                        GomDelta             *delta) G_GNUC_WARN_UNUSED_RESULT;
DexFuture    *_gom_sync_history_append_remote_change   (GomRepository        *repository,
                                                        const char           *relation,
                                                        const char           *identity,
                                                        GomDelta             *delta) G_GNUC_WARN_UNUSED_RESULT;
DexFuture    *_gom_sync_history_replay                 (GomRepository        *repository,
                                                        guint64               sequence) G_GNUC_WARN_UNUSED_RESULT;
DexFuture    *_gom_sync_history_ack                    (GomRepository        *repository,
                                                        guint64               sequence) G_GNUC_WARN_UNUSED_RESULT;
guint64       _gom_sync_history_change_get_sequence    (GomSyncHistoryChange *self);
guint64       _gom_sync_history_change_get_batch       (GomSyncHistoryChange *self);
const char   *_gom_sync_history_change_get_relation    (GomSyncHistoryChange *self);
const char   *_gom_sync_history_change_get_identity    (GomSyncHistoryChange *self);
GType         _gom_sync_history_change_get_entity_type (GomSyncHistoryChange *self);
GomDeltaKind  _gom_sync_history_change_get_delta_kind  (GomSyncHistoryChange *self);
gboolean      _gom_sync_history_change_get_tombstone   (GomSyncHistoryChange *self);
gboolean      _gom_sync_history_change_get_outbound    (GomSyncHistoryChange *self);
gboolean      _gom_sync_history_change_get_sent        (GomSyncHistoryChange *self);
gboolean      _gom_sync_history_change_get_acked       (GomSyncHistoryChange *self);
GomDelta     *_gom_sync_history_change_dup_delta       (GomSyncHistoryChange *self);

G_END_DECLS
