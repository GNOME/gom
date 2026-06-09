/* gom-tombstone-private.h
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

#define GOM_TYPE_TOMBSTONE (gom_tombstone_get_type())

GOM_DECLARE_INTERNAL_TYPE (GomTombstone, gom_tombstone, GOM, TOMBSTONE, GomEntity)

DexFuture     *_gom_tombstone_ensure_schema             (GomRepository  *repository) G_GNUC_WARN_UNUSED_RESULT;
char          *_gom_tombstone_serialize_entity_identity (GomEntity      *entity,
                                                         GError        **error);
gboolean       _gom_tombstone_apply_identity_to_entity  (GomEntity      *entity,
                                                         const char     *identity,
                                                         GError        **error);
GomExpression *_gom_tombstone_build_identity_filter     (GType           entity_type,
                                                         const char     *identity,
                                                         GError        **error);
DexFuture     *_gom_tombstone_record                    (GomRepository  *repository,
                                                         GType           entity_type,
                                                         const char     *relation,
                                                         const char     *identity,
                                                         guint64         delete_sequence) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_tombstone_record_with_session       (GomRepository  *repository,
                                                         GomSession     *session,
                                                         GType           entity_type,
                                                         const char     *relation,
                                                         const char     *identity,
                                                         guint64         delete_sequence) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_tombstone_remove                    (GomRepository  *repository,
                                                         GType           entity_type,
                                                         const char     *identity) G_GNUC_WARN_UNUSED_RESULT;
gboolean       _gom_tombstone_lookup_sequence           (GomRepository  *repository,
                                                         GType           entity_type,
                                                         const char     *identity,
                                                         guint64        *sequence,
                                                         GError        **error);

G_END_DECLS
