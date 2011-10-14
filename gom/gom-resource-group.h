/* gom-resource-group.h
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

#ifndef GOM_RESOURCE_GROUP_H
#define GOM_RESOURCE_GROUP_H

#include <gio/gio.h>

#include "gom-resource.h"

G_BEGIN_DECLS

#define GOM_TYPE_RESOURCE_GROUP            (gom_resource_group_get_type())
#define GOM_RESOURCE_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_RESOURCE_GROUP, GomResourceGroup))
#define GOM_RESOURCE_GROUP_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_RESOURCE_GROUP, GomResourceGroup const))
#define GOM_RESOURCE_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_RESOURCE_GROUP, GomResourceGroupClass))
#define GOM_IS_RESOURCE_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_RESOURCE_GROUP))
#define GOM_IS_RESOURCE_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_RESOURCE_GROUP))
#define GOM_RESOURCE_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_RESOURCE_GROUP, GomResourceGroupClass))

typedef struct _GomResourceGroup        GomResourceGroup;
typedef struct _GomResourceGroupClass   GomResourceGroupClass;
typedef struct _GomResourceGroupPrivate GomResourceGroupPrivate;

struct _GomResourceGroup
{
   GObject parent;

   /*< private >*/
   GomResourceGroupPrivate *priv;
};

struct _GomResourceGroupClass
{
   GObjectClass parent_class;
};

void         gom_resource_group_fetch_async   (GomResourceGroup     *group,
                                               guint                 index_,
                                               guint                 count,
                                               GAsyncReadyCallback   callback,
                                               gpointer              user_data);
gboolean     gom_resource_group_fetch_finish  (GomResourceGroup     *group,
                                               GAsyncResult         *result,
                                               GError              **error);
guint        gom_resource_group_get_count     (GomResourceGroup     *group);
GomResource *gom_resource_group_get_index     (GomResourceGroup     *group,
                                               guint                 index_);
const gchar *gom_resource_group_get_m2m_table (GomResourceGroup     *group);
GType        gom_resource_group_get_type      (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GOM_RESOURCE_GROUP_H */
