/* gom-meta-private.h
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

#include "gom-meta.h"
#include "gom-types.h"

G_BEGIN_DECLS

GomRegistry               *_gom_registry_new_empty                      (void);
void                       _gom_registry_add_entity_type                (GomRegistry     *registry,
                                                                         GType            entity_type);
const GomEntitySpec       *_gom_registry_lookup_entity_by_type          (GomRegistry     *registry,
                                                                         GType            entity_type);
const GomEntitySpec       *_gom_registry_lookup_entity_by_name          (GomRegistry     *registry,
                                                                         const char      *name);
const GomEntitySpec       *_gom_registry_lookup_entity_by_table         (GomRegistry     *registry,
                                                                         const char      *table);
const GomPropertySpec     *_gom_entity_spec_lookup_property_by_name     (GomEntitySpec   *entity,
                                                                         const char      *name);
const GomPropertySpec     *_gom_entity_spec_lookup_property_by_field    (GomEntitySpec   *entity,
                                                                         const char      *field);
GParamSpec                *_gom_property_spec_get_pspec                 (GomPropertySpec *property);
const GomIndexSpec        *_gom_entity_spec_lookup_index_by_name        (GomEntitySpec   *entity,
                                                                         const char      *name);
const GomRelationshipSpec *_gom_entity_spec_lookup_relationship_by_name (GomEntitySpec   *entity,
                                                                         const char      *name);

G_END_DECLS
