/* gom-version-macros.h
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

#include <glib.h>

#include "gom-version.h"

#ifndef _GOM_EXTERN
#define _GOM_EXTERN extern
#endif

#define GOM_VERSION_CUR_STABLE (G_ENCODE_VERSION (GOM_MAJOR_VERSION, 0))

#ifdef GOM_DISABLE_DEPRECATION_WARNINGS
#define GOM_DEPRECATED _GOM_EXTERN
#define GOM_DEPRECATED_FOR(f) _GOM_EXTERN
#define GOM_UNAVAILABLE(maj, min) _GOM_EXTERN
#else
#define GOM_DEPRECATED G_DEPRECATED _GOM_EXTERN
#define GOM_DEPRECATED_FOR(f) G_DEPRECATED_FOR (f) _GOM_EXTERN
#define GOM_UNAVAILABLE(maj, min) G_UNAVAILABLE (maj, min) _GOM_EXTERN
#endif

#define GOM_VERSION_2_0 (G_ENCODE_VERSION (2, 0))

#if GOM_MAJOR_VERSION == GOM_VERSION_2_0
#define GOM_VERSION_PREV_STABLE (GOM_VERSION_2_0)
#else
#define GOM_VERSION_PREV_STABLE (G_ENCODE_VERSION (GOM_MAJOR_VERSION - 1, 0))
#endif

#ifndef GOM_VERSION_MIN_REQUIRED
#define GOM_VERSION_MIN_REQUIRED (GOM_VERSION_CUR_STABLE)
#endif

#ifndef GOM_VERSION_MAX_ALLOWED
#if GOM_VERSION_MIN_REQUIRED > GOM_VERSION_PREV_STABLE
#define GOM_VERSION_MAX_ALLOWED (GOM_VERSION_MIN_REQUIRED)
#else
#define GOM_VERSION_MAX_ALLOWED (GOM_VERSION_CUR_STABLE)
#endif
#endif

#define GOM_AVAILABLE_IN_ALL _GOM_EXTERN

#if GOM_VERSION_MIN_REQUIRED >= GOM_VERSION_2_0
#define GOM_DEPRECATED_IN_2_0 GOM_DEPRECATED
#define GOM_DEPRECATED_IN_2_0_FOR(f) GOM_DEPRECATED_FOR(f)
#else
#define GOM_DEPRECATED_IN_2_0 _GOM_EXTERN
#define GOM_DEPRECATED_IN_2_0_FOR(f) _GOM_EXTERN
#endif
#if GOM_VERSION_MAX_ALLOWED < GOM_VERSION_2_0
#define GOM_AVAILABLE_IN_2_0 GOM_UNAVAILABLE(2, 0)
#else
#define GOM_AVAILABLE_IN_2_0 _GOM_EXTERN
#endif
