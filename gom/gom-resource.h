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
#define GOM_RESOURCE_NEW_IN_VERSION  (gom_resource_new_in_version_quark())
#define GOM_RESOURCE_NOT_MAPPED      (gom_resource_not_mapped_quark())
#define GOM_RESOURCE_TO_BYTES_FUNC   (gom_resource_to_bytes_func_quark())
#define GOM_RESOURCE_FROM_BYTES_FUNC (gom_resource_from_bytes_func_quark())
#define GOM_RESOURCE_REF_TABLE_CLASS (gom_resource_ref_table_class())
#define GOM_RESOURCE_REF_PROPERTY_NAME (gom_resource_ref_property_name())
#define GOM_RESOURCE_UNIQUE          (gom_resource_unique())

typedef struct _GomResource        GomResource;
typedef struct _GomResourceClass   GomResourceClass;
typedef struct _GomResourcePrivate GomResourcePrivate;

#include "gom-resource-group.h"

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

typedef GBytes * (*GomResourceToBytesFunc) (GValue *value);
typedef void (*GomResourceFromBytesFunc) (GBytes *bytes, GValue *value);

void              gom_resource_class_set_table       (GomResourceClass *resource_class,
                                                      const gchar      *table);
void              gom_resource_class_set_primary_key (GomResourceClass *resource_class,
                                                      const gchar      *primary_key);
void              gom_resource_class_set_property_new_in_version (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name,
                                                                  guint                     version);
void              gom_resource_class_set_property_set_mapped     (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name,
                                                                  gboolean                  is_mapped);
void              gom_resource_class_set_property_transform      (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name,
                                                                  GomResourceToBytesFunc    to_bytes_func,
                                                                  GomResourceFromBytesFunc  from_bytes_func);
void              gom_resource_class_set_property_to_bytes       (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name,
                                                                  GomResourceToBytesFunc    to_bytes_func,
                                                                  GDestroyNotify            notify);
void              gom_resource_class_set_property_from_bytes     (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name,
                                                                  GomResourceFromBytesFunc  from_bytes_func,
                                                                  GDestroyNotify            notify);
void              gom_resource_class_set_reference               (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name,
                                                                  const gchar              *ref_table_name,
                                                                  const gchar              *ref_property_name);
void              gom_resource_class_set_unique                  (GomResourceClass         *resource_class,
                                                                  const gchar              *property_name);

void              gom_resource_delete_async          (GomResource          *resource,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
gboolean          gom_resource_delete_finish         (GomResource          *resource,
                                                      GAsyncResult         *result,
                                                      GError              **error);
gboolean          gom_resource_delete_sync           (GomResource          *resource,
                                                      GError              **error);
GQuark            gom_resource_new_in_version_quark  (void) G_GNUC_CONST;
GQuark            gom_resource_not_mapped_quark      (void) G_GNUC_CONST;
GQuark            gom_resource_to_bytes_func_quark   (void) G_GNUC_CONST;
GQuark            gom_resource_from_bytes_func_quark (void) G_GNUC_CONST;
GQuark            gom_resource_ref_table_class       (void) G_GNUC_CONST;
GQuark            gom_resource_ref_property_name     (void) G_GNUC_CONST;
GQuark            gom_resource_unique                (void) G_GNUC_CONST;
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
GomResourceGroup *gom_resource_fetch_m2m_finish      (GomResource          *resource,
                                                      GAsyncResult         *result,
                                                      GError              **error);

G_END_DECLS

#endif /* GOM_RESOURCE_H */
