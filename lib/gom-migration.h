/* gom-migration.h
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

#define GOM_TYPE_MIGRATION (gom_migration_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GomMigration, gom_migration, GOM, MIGRATION, GObject)

struct _GomMigrationClass
{
  GObjectClass parent_class;

  guint      (*get_version) (GomMigration *self);
  DexFuture *(*apply)       (GomMigration *self,
                             GomDriver    *driver);

  /*< private >*/
  gpointer _reserved[13];
};

GOM_AVAILABLE_IN_ALL
guint      gom_migration_get_version (GomMigration *self);
GOM_AVAILABLE_IN_ALL
DexFuture *gom_migration_apply       (GomMigration *self,
                                      GomDriver    *driver) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
