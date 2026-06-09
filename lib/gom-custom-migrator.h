/* gom-custom-migrator.h
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

#include "gom-migrator.h"

G_BEGIN_DECLS

#define GOM_TYPE_CUSTOM_MIGRATOR (gom_custom_migrator_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomCustomMigrator, gom_custom_migrator, GOM, CUSTOM_MIGRATOR, GomMigrator)

GOM_AVAILABLE_IN_ALL
GomCustomMigrator *gom_custom_migrator_new           (guint              version);
GOM_AVAILABLE_IN_ALL
void               gom_custom_migrator_add_migration (GomCustomMigrator *self,
                                                      GomMigration      *migration);

G_END_DECLS
