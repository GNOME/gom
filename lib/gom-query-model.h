/* gom-query-model.h
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

#define GOM_TYPE_QUERY_MODEL (gom_query_model_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomQueryModel, gom_query_model, GOM, QUERY_MODEL, GObject)

GOM_AVAILABLE_IN_ALL
GomQueryModel *gom_query_model_new             (GomSession    *session,
                                                GType          entity_type,
                                                GomExpression *filter,
                                                GomOrdering   *ordering);
GOM_AVAILABLE_IN_ALL
DexFuture     *gom_query_model_reload          (GomQueryModel *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture     *gom_query_model_refresh         (GomQueryModel *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
gboolean       gom_query_model_get_loading     (GomQueryModel *self);
GOM_AVAILABLE_IN_ALL
GomSession    *gom_query_model_dup_session     (GomQueryModel *self);
GOM_AVAILABLE_IN_ALL
GType          gom_query_model_get_entity_type (GomQueryModel *self);

G_END_DECLS
