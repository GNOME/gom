/* gom-migrator.c
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

#include "config.h"

#include "gom-migrator.h"
#include "gom-driver.h"
#include "gom-migrator-private.h"

G_DEFINE_ABSTRACT_TYPE (GomMigrator, gom_migrator, G_TYPE_OBJECT)

static void
gom_migrator_class_init (GomMigratorClass *klass)
{
}

static void
gom_migrator_init (GomMigrator *self)
{
}

/**
 * gom_migrator_update:
 * @self: a [class@Gom.Migrator]
 * @driver: a [class@Gom.Driver]
 *
 * Runs any migration steps necessary to get from the previous version
 * to the current version expected by the software.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to any
 *   value or rejects with error.
 */
DexFuture *
gom_migrator_update (GomMigrator *self,
                     GomDriver   *driver)
{
  dex_return_error_if_fail (GOM_IS_MIGRATOR (self));
  dex_return_error_if_fail (GOM_IS_DRIVER (driver));

  if (GOM_MIGRATOR_GET_CLASS (self)->update)
    return GOM_MIGRATOR_GET_CLASS (self)->update (self, driver);

  return dex_future_new_true ();
}
