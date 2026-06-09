/* gom-update-builder.h
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

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_UPDATE_BUILDER (gom_update_builder_get_type())

GOM_AVAILABLE_IN_ALL
GType             gom_update_builder_get_type               (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GomUpdateBuilder *gom_update_builder_new                    (void);
GOM_AVAILABLE_IN_ALL
GomUpdateBuilder *gom_update_builder_ref                    (GomUpdateBuilder  *self);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_unref                  (GomUpdateBuilder  *self);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_set_target_entity_type (GomUpdateBuilder  *self,
                                                             GType              target_entity_type);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_set_target_relation    (GomUpdateBuilder  *self,
                                                             const char        *target_relation);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_add_assignment         (GomUpdateBuilder  *self,
                                                             GomExpression     *column,
                                                             GomExpression     *value);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_clear_assignments      (GomUpdateBuilder  *self);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_set_filter             (GomUpdateBuilder  *self,
                                                             GomExpression     *filter);
GOM_AVAILABLE_IN_ALL
void              gom_update_builder_set_limit              (GomUpdateBuilder  *self,
                                                             guint64            limit);
GOM_AVAILABLE_IN_ALL
GomUpdate        *gom_update_builder_build                  (GomUpdateBuilder  *self,
                                                             GError           **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomUpdateBuilder, gom_update_builder_unref)

G_END_DECLS
