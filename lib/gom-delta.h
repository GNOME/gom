/* gom-delta.h
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

#include <gio/gio.h>

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_DELTA (gom_delta_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomDelta, gom_delta, GOM, DELTA, GObject)

GOM_AVAILABLE_IN_ALL
GomDeltaKind  gom_delta_get_kind           (GomDelta     *self);
GOM_AVAILABLE_IN_ALL
GType         gom_delta_get_entity_type    (GomDelta     *self);
GOM_AVAILABLE_IN_ALL
gboolean      gom_delta_is_empty           (GomDelta     *self);
GOM_AVAILABLE_IN_ALL
guint         gom_delta_get_n_changes      (GomDelta     *self);
GOM_AVAILABLE_IN_ALL
const char   *gom_delta_get_property_name  (GomDelta     *self,
                                            guint         index);
GOM_AVAILABLE_IN_ALL
gboolean      gom_delta_get_original_value (GomDelta     *self,
                                            guint         index,
                                            GValue       *value);
GOM_AVAILABLE_IN_ALL
gboolean      gom_delta_get_current_value  (GomDelta     *self,
                                            guint         index,
                                            GValue       *value);
GOM_AVAILABLE_IN_ALL
GomDelta     *gom_delta_new                (GType         entity_type,
                                            GomDeltaKind  kind);
GOM_AVAILABLE_IN_ALL
void          gom_delta_add_property       (GomDelta     *self,
                                            const char   *property_name,
                                            const GValue *original_value,
                                            const GValue *current_value);

G_END_DECLS
