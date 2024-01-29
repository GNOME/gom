/* gom-adapter.h
 *
 * Copyright (C) 2015 Alexander Larsson <alexl@redhat.com>
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

#ifndef GOM_AUTOCLEANUPS_H
#define GOM_AUTOCLEANUPS_H

/* We need all the types, so don't try to include this directly */
#if !defined (GOM_INSIDE)
#error "Include <gom.h> instead of gom-autocleanups.h."
#endif

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomAdapter, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomCommand, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomCommandBuilder, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomCursor, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomFilter, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomRepository, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomResource, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomResourceGroup, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GomSorting, g_object_unref)

#endif

#endif /* GOM_AUTOCLEANUP_H */
