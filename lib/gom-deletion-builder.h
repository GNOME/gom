/* gom-deletion-builder.h
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

#define GOM_TYPE_DELETION_BUILDER (gom_deletion_builder_get_type())

GOM_AVAILABLE_IN_ALL
GType               gom_deletion_builder_get_type               (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GomDeletionBuilder *gom_deletion_builder_new                    (void);
GOM_AVAILABLE_IN_ALL
GomDeletionBuilder *gom_deletion_builder_ref                    (GomDeletionBuilder  *self);
GOM_AVAILABLE_IN_ALL
void                gom_deletion_builder_unref                  (GomDeletionBuilder  *self);
GOM_AVAILABLE_IN_ALL
void                gom_deletion_builder_set_target_entity_type (GomDeletionBuilder  *self,
                                                                 GType                target_entity_type);
GOM_AVAILABLE_IN_ALL
void                gom_deletion_builder_set_target_relation    (GomDeletionBuilder  *self,
                                                                 const char          *target_relation);
GOM_AVAILABLE_IN_ALL
void                gom_deletion_builder_set_filter             (GomDeletionBuilder  *self,
                                                                 GomExpression       *filter);
GOM_AVAILABLE_IN_ALL
void                gom_deletion_builder_set_limit              (GomDeletionBuilder  *self,
                                                                 guint64              limit);
GOM_AVAILABLE_IN_ALL
GomDeletion        *gom_deletion_builder_build                  (GomDeletionBuilder  *self,
                                                                 GError             **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomDeletionBuilder, gom_deletion_builder_unref)

G_END_DECLS
