/* gom-update-private.h
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

#include "gom-update.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

GomUpdate     *_gom_update_new                    (GType          target_entity_type,
                                                   const char    *target_relation,
                                                   GPtrArray     *columns,
                                                   GPtrArray     *values,
                                                   GomExpression *filter,
                                                   guint64        limit,
                                                   gboolean       has_limit);
GType          _gom_update_get_target_entity_type (GomUpdate     *self);
const char    *_gom_update_get_target_relation    (GomUpdate     *self);
GPtrArray     *_gom_update_get_columns            (GomUpdate     *self);
GPtrArray     *_gom_update_get_values             (GomUpdate     *self);
GomExpression *_gom_update_get_filter             (GomUpdate     *self);
gboolean       _gom_update_has_limit              (GomUpdate     *self);
guint64        _gom_update_get_limit              (GomUpdate     *self);

G_END_DECLS
