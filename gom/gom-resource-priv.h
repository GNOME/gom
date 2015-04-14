/* gom-resource.h
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

#ifndef GOM_RESOURCE_PRIV_H
#define GOM_RESOURCE_PRIV_H

#include "gom-resource.h"

G_BEGIN_DECLS

gboolean gom_resource_get_is_from_table        (GomResource  *resource);
void     gom_resource_set_is_from_table        (GomResource  *resource,
                                                gboolean      is_from_table);
gboolean gom_resource_has_dynamic_pkey         (GType         type);
gboolean gom_resource_do_save                  (GomResource  *resource,
                                                GomAdapter   *adapter,
                                                GError      **error);
gboolean gom_resource_do_delete                (GomResource  *resource,
                                                GomAdapter   *adapter,
                                                GError      **error);
void     gom_resource_build_save_cmd           (GomResource  *resource,
                                                GomAdapter   *adapter);
void     gom_resource_set_post_save_properties (GomResource  *resource);

G_END_DECLS

#endif /* GOM_RESOURCE_PRIV_H */
