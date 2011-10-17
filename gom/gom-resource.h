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

#ifndef GOM_RESOURCE_H
#define GOM_RESOURCE_H

#include <gio/gio.h>

#include "gom-filter.h"

G_BEGIN_DECLS

#define GOM_TYPE_RESOURCE            (gom_resource_get_type())
#define GOM_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_RESOURCE, GomResource))
#define GOM_RESOURCE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_RESOURCE, GomResource const))
#define GOM_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_RESOURCE, GomResourceClass))
#define GOM_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_RESOURCE))
#define GOM_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_RESOURCE))
#define GOM_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_RESOURCE, GomResourceClass))
#define GOM_RESOURCE_ERROR           (gom_resource_error_quark())

typedef struct _GomResource        GomResource;
typedef struct _GomResourceClass   GomResourceClass;
typedef struct _GomResourcePrivate GomResourcePrivate;
typedef enum   _GomResourceError   GomResourceError;

enum _GomResourceError
{
   GOM_RESOURCE_ERROR_CURSOR = 1,
};

struct _GomResource
{
   GObject parent;

   /*< private >*/
   GomResourcePrivate *priv;
};

struct _GomResourceClass
{
   GObjectClass parent_class;

   gchar primary_key[64];
   gchar table[64];
};

void              gom_resource_class_set_table       (GomResourceClass *resource_class,
                                                      const gchar      *table);
void              gom_resource_class_set_primary_key (GomResourceClass *resource_class,
                                                      const gchar      *primary_key);
void              gom_resource_delete_async          (GomResource          *resource,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gboolean          gom_resource_delete_finish         (GomResource          *resource,
                                                      GAsyncResult         *result,
                                                      GError              **error);
GQuark            gom_resource_error_quark           (void) G_GNUC_CONST;
GType             gom_resource_get_type              (void) G_GNUC_CONST;
void              gom_resource_save_async            (GomResource          *resource,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gboolean          gom_resource_save_finish           (GomResource          *resource,
                                                      GAsyncResult         *result,
                                                      GError              **error);
gboolean          gom_resource_save_sync             (GomResource          *resource,
                                                      GError              **error);
void              gom_resource_fetch_m2m_async       (GomResource          *resource,
                                                      GType                 resource_type,
                                                      const gchar          *m2m_table,
                                                      GomFilter            *filter,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gpointer          gom_resource_fetch_m2m_finish      (GomResource          *resource,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif /* GOM_RESOURCE_H */
