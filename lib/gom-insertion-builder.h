/* gom-insertion-builder.h
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

#define GOM_TYPE_INSERTION_BUILDER (gom_insertion_builder_get_type())

GOM_AVAILABLE_IN_ALL
GType                gom_insertion_builder_get_type               (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GomInsertionBuilder *gom_insertion_builder_new                    (GomRepository        *repository);
GOM_AVAILABLE_IN_ALL
GomInsertionBuilder *gom_insertion_builder_ref                    (GomInsertionBuilder  *self);
GOM_AVAILABLE_IN_ALL
void                 gom_insertion_builder_unref                  (GomInsertionBuilder  *self);
GOM_AVAILABLE_IN_ALL
void                 gom_insertion_builder_set_target_entity_type (GomInsertionBuilder  *self,
                                                                   GType                 target_entity_type);
GOM_AVAILABLE_IN_ALL
void                 gom_insertion_builder_set_target_relation    (GomInsertionBuilder  *self,
                                                                   const char           *target_relation);
GOM_AVAILABLE_IN_ALL
void                 gom_insertion_builder_add_column             (GomInsertionBuilder  *self,
                                                                   GomExpression        *column);
GOM_AVAILABLE_IN_ALL
void                 gom_insertion_builder_clear_columns          (GomInsertionBuilder  *self);
GOM_AVAILABLE_IN_ALL
void                 gom_insertion_builder_add_row                (GomInsertionBuilder  *self,
                                                                   GomExpression       **values,
                                                                   gsize                 n_values);
GOM_AVAILABLE_IN_ALL
gboolean             gom_insertion_builder_add_entity             (GomInsertionBuilder  *self,
                                                                   GomEntity            *entity,
                                                                   GError              **error);
GOM_AVAILABLE_IN_ALL
gboolean             gom_insertion_builder_add_entity_list        (GomInsertionBuilder  *self,
                                                                   GListModel           *entities,
                                                                   GError              **error);
GOM_AVAILABLE_IN_ALL
GomInsertion        *gom_insertion_builder_build                  (GomInsertionBuilder  *self,
                                                                   GError              **error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomInsertionBuilder, gom_insertion_builder_unref)

G_END_DECLS
