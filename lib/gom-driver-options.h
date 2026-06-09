/* gom-driver-options.h
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

#define GOM_TYPE_DRIVER_OPTIONS (gom_driver_options_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomDriverOptions, gom_driver_options, GOM, DRIVER_OPTIONS, GObject)

GOM_AVAILABLE_IN_ALL
GomDriverOptions *gom_driver_options_new                (void);
GOM_AVAILABLE_IN_ALL
void              gom_driver_options_set_encryption_key (GomDriverOptions *self,
                                                         GBytes           *key);
GOM_AVAILABLE_IN_ALL
GBytes           *gom_driver_options_dup_encryption_key (GomDriverOptions *self);

G_END_DECLS
