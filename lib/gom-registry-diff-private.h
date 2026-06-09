/* gom-registry-diff-private.h
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

#include "gom-types-private.h"

G_BEGIN_DECLS

typedef struct _GomRegistryDiff GomRegistryDiff;
typedef struct _GomEntityDiff   GomEntityDiff;
typedef struct _GomPropertyDiff GomPropertyDiff;
typedef struct _GomIndexDiff    GomIndexDiff;

GomRegistryDiff *_gom_registry_diff_new                  (GomRegistry     *current,
                                                          GomRegistry     *next);
void             _gom_registry_diff_free                 (GomRegistryDiff *self);
const GPtrArray *_gom_registry_diff_get_dropped_entities (GomRegistryDiff *self);
const GPtrArray *_gom_registry_diff_get_added_entities   (GomRegistryDiff *self);
const GPtrArray *_gom_registry_diff_get_changed_entities (GomRegistryDiff *self);
GomEntitySpec   *_gom_entity_diff_get_current_entity     (GomEntityDiff   *self);
GomEntitySpec   *_gom_entity_diff_get_next_entity        (GomEntityDiff   *self);
gboolean         _gom_entity_diff_get_identity_changed   (GomEntityDiff   *self);
const GPtrArray *_gom_entity_diff_get_dropped_properties (GomEntityDiff   *self);
const GPtrArray *_gom_entity_diff_get_added_properties   (GomEntityDiff   *self);
const GPtrArray *_gom_entity_diff_get_changed_properties (GomEntityDiff   *self);
const GPtrArray *_gom_entity_diff_get_dropped_indexes    (GomEntityDiff   *self);
const GPtrArray *_gom_entity_diff_get_added_indexes      (GomEntityDiff   *self);
const GPtrArray *_gom_entity_diff_get_changed_indexes    (GomEntityDiff   *self);
GomPropertySpec *_gom_property_diff_get_current_property (GomPropertyDiff *self);
GomPropertySpec *_gom_property_diff_get_next_property    (GomPropertyDiff *self);
GomIndexSpec    *_gom_index_diff_get_current_index       (GomIndexDiff    *self);
GomIndexSpec    *_gom_index_diff_get_next_index          (GomIndexDiff    *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomRegistryDiff, _gom_registry_diff_free)

G_END_DECLS
