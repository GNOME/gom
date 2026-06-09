/* gom-related-model.h
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

#define GOM_TYPE_RELATED_MODEL (gom_related_model_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomRelatedModel, gom_related_model, GOM, RELATED_MODEL, GObject)

GOM_AVAILABLE_IN_ALL
GomRelatedModel *gom_related_model_new                   (GomEntity       *owner,
                                                          const char      *relationship_name);
GOM_AVAILABLE_IN_ALL
DexFuture       *gom_related_model_reload                (GomRelatedModel *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
DexFuture       *gom_related_model_refresh               (GomRelatedModel *self) G_GNUC_WARN_UNUSED_RESULT;
GOM_AVAILABLE_IN_ALL
gboolean         gom_related_model_get_loading           (GomRelatedModel *self);
GOM_AVAILABLE_IN_ALL
GomEntity       *gom_related_model_dup_owner             (GomRelatedModel *self);
GOM_AVAILABLE_IN_ALL
const char      *gom_related_model_get_relationship_name (GomRelatedModel *self);

G_END_DECLS
