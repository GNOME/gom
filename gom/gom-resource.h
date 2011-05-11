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

#include <glib-object.h>
#include <gio/gio.h>

#include "gom-adapter.h"
#include "gom-collection.h"
#include "gom-condition.h"
#include "gom-property.h"
#include "gom-property-set.h"
#include "gom-resource-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_RESOURCE            (gom_resource_get_type())
#define GOM_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_RESOURCE, GomResource))
#define GOM_RESOURCE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_RESOURCE, GomResource const))
#define GOM_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_RESOURCE, GomResourceClass))
#define GOM_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_RESOURCE))
#define GOM_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_RESOURCE))
#define GOM_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_RESOURCE, GomResourceClass))
#define GOM_RESOURCE_ERROR           (gom_resource_error_quark())

typedef struct _GomResource          GomResource;
typedef struct _GomResourceClass     GomResourceClass;
typedef struct _GomResourcePrivate   GomResourcePrivate;
typedef enum   _GomResourceError     GomResourceError;

enum _GomResourceError
{
	GOM_RESOURCE_ERROR_NO_ADAPTER = 1,
	GOM_RESOURCE_ERROR_NOT_FOUND,
	GOM_RESOURCE_ERROR_CANCELLED,
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

	/*< private >*/
	const gchar    *table;
	GQuark          tableq;
	GomPropertySet *keys;
	GomPropertySet *properties;
};

void                    gom_resource_class_belongs_to       (GomResourceClass  *resource_class,
                                                             const gchar       *property_name,
                                                             const gchar       *property_nick,
                                                             const gchar       *property_desc,
                                                             GType              resource_type,
                                                             ...) G_GNUC_NULL_TERMINATED;
GomPropertySet*         gom_resource_class_get_properties   (GomResourceClass  *resource_class);
void                    gom_resource_class_has_a            (GomResourceClass  *resource_class,
                                                             const gchar       *property_name,
                                                             const gchar       *property_nick,
                                                             const gchar       *property_desc,
                                                             GType              resource_type,
                                                             ...) G_GNUC_NULL_TERMINATED;
void                    gom_resource_class_has_many         (GomResourceClass  *resource_class,
                                                             const gchar       *property_name,
                                                             const gchar       *property_nick,
                                                             const gchar       *property_desc,
                                                             GType              resource_type,
                                                             ...) G_GNUC_NULL_TERMINATED;
void                    gom_resource_class_table            (GomResourceClass  *resource_class,
                                                             const gchar       *table);
void                    gom_resource_class_install_property (GomResourceClass *resource_class,
                                                             GParamSpec       *param_spec,
                                                             ...) G_GNUC_NULL_TERMINATED;

gpointer                gom_resource_create                 (GType             resource_type,
                                                             GomAdapter       *adapter,
                                                             const gchar      *first_property,
                                                             ...) G_GNUC_NULL_TERMINATED;
gboolean                gom_resource_delete                 (GomResource      *resource,
                                                             GError          **error);
GQuark                  gom_resource_error_quark            (void) G_GNUC_CONST;
GomCollection*          gom_resource_find                   (GType             resource_type,
                                                             GomAdapter       *adapter,
                                                             GomCondition     *condition,
                                                             GError          **error);
gpointer                gom_resource_find_first             (GType             resource_type,
                                                             GomAdapter       *adapter,
                                                             GomCondition     *condition,
                                                             GError          **error);
GomProperty*            gom_resource_find_property          (GomResource      *resource,
                                                             const gchar      *name);
GomCondition*           gom_resource_get_condition          (GomResource      *resource);
GType                   gom_resource_get_type               (void) G_GNUC_CONST;
gboolean                gom_resource_is_dirty               (GomResource      *resource);
gboolean                gom_resource_is_new                 (GomResource      *resource);
void                    gom_resource_merge                  (GomResource      *resource,
                                                             GomResource      *other);
gboolean                gom_resource_reload                 (GomResource      *resource,
                                                             GError          **error);
gboolean                gom_resource_save                   (GomResource      *resource,
                                                             GError          **error);
void                    gom_resource_save_async             (GomResource      *resource,
                                                             GCancellable     *cancellable,
                                                             GAsyncReadyCallback callback,
                                                             gpointer          user_data);
gboolean                gom_resource_save_finish            (GomResource      *resource,
                                                             GAsyncResult     *result,
                                                             GError          **error);
GomPropertyValue**      gom_resource_get_properties         (GomResource      *resource,
                                                             guint            *n_values) G_GNUC_WARN_UNUSED_RESULT;
void                    gom_resource_get_property           (GObject          *object,
                                                             guint             prop_id,
                                                             GValue           *value,
                                                             GParamSpec       *pspec);
void                    gom_resource_set_property           (GObject          *object,
                                                             guint             prop_id,
                                                             const GValue     *value,
                                                             GParamSpec       *pspec);

G_END_DECLS

#endif /* GOM_RESOURCE_H */
