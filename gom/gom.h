/* gom.h
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GOM_H
#define GOM_H

#include <glib.h>

G_BEGIN_DECLS

#define GOM_INSIDE

#include "gom-adapter.h"
#include "gom-command.h"
#include "gom-command-builder.h"
#include "gom-cursor.h"
#include "gom-error.h"
#include "gom-filter.h"
#include "gom-repository.h"
#include "gom-resource-group.h"
#include "gom-resource.h"

#include "gom-autocleanups.h"

#undef GOM_INSIDE

G_END_DECLS

#endif /* GOM_H */
