/* gom-schema.h
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

#define GOM_TYPE_SCHEMA (gom_schema_get_type())
#define GOM_TYPE_FIELD_SCHEMA (gom_field_schema_get_type())
#define GOM_TYPE_RELATION_SCHEMA (gom_relation_schema_get_type())
#define GOM_TYPE_INDEX_SCHEMA (gom_index_schema_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomSchema, gom_schema, GOM, SCHEMA, GObject)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomFieldSchema, gom_field_schema, GOM, FIELD_SCHEMA, GomSchema)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomRelationSchema, gom_relation_schema, GOM, RELATION_SCHEMA, GomSchema)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomIndexSchema, gom_index_schema, GOM, INDEX_SCHEMA, GomSchema)

GOM_AVAILABLE_IN_ALL
const char         *gom_schema_get_name                (GomSchema         *self);
GOM_AVAILABLE_IN_ALL
const char         *gom_field_schema_get_sql_type      (GomFieldSchema    *self);
GOM_AVAILABLE_IN_ALL
gboolean            gom_field_schema_get_nonnull       (GomFieldSchema    *self);
GOM_AVAILABLE_IN_ALL
gboolean            gom_field_schema_get_primary_key   (GomFieldSchema    *self);
GOM_AVAILABLE_IN_ALL
const char         *gom_field_schema_get_default_value (GomFieldSchema    *self);
GOM_AVAILABLE_IN_ALL
gboolean            gom_index_schema_get_unique        (GomIndexSchema    *self);
GOM_AVAILABLE_IN_ALL
const char * const *gom_index_schema_get_fields        (GomIndexSchema    *self);
GOM_AVAILABLE_IN_ALL
GListModel         *gom_relation_schema_list_fields    (GomRelationSchema *self);
GOM_AVAILABLE_IN_ALL
GListModel         *gom_relation_schema_list_indexes   (GomRelationSchema *self);

G_END_DECLS
