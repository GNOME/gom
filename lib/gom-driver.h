/* gom-driver.h
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

#define GOM_TYPE_DRIVER (gom_driver_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomDriver, gom_driver, GOM, DRIVER, GObject)

GOM_AVAILABLE_IN_ALL
char      *gom_driver_dup_uri           (GomDriver         *self);
GOM_AVAILABLE_IN_ALL
GomDriver *gom_driver_open              (const char        *uri,
                                         GError           **error);
GOM_AVAILABLE_IN_ALL
GomDriver *gom_driver_open_with_options (const char        *uri,
                                         GomDriverOptions  *options,
                                         GError           **error);
GOM_AVAILABLE_IN_ALL
DexFuture *gom_driver_rekey             (GomDriver         *self,
                                         GomDriverOptions   *options) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
