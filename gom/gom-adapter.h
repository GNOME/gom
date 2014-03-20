/* gom-adapter.h
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

#ifndef GOM_ADAPTER_H
#define GOM_ADAPTER_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define GOM_TYPE_ADAPTER            (gom_adapter_get_type())
#define GOM_ADAPTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ADAPTER, GomAdapter))
#define GOM_ADAPTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ADAPTER, GomAdapter const))
#define GOM_ADAPTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_ADAPTER, GomAdapterClass))
#define GOM_IS_ADAPTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_ADAPTER))
#define GOM_IS_ADAPTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_ADAPTER))
#define GOM_ADAPTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_ADAPTER, GomAdapterClass))
#define GOM_ADAPTER_ERROR           (gom_adapter_error_quark())

typedef struct _GomAdapter        GomAdapter;
typedef struct _GomAdapterClass   GomAdapterClass;
typedef struct _GomAdapterPrivate GomAdapterPrivate;
typedef enum   _GomAdapterError   GomAdapterError;

typedef void (*GomAdapterCallback) (GomAdapter *adapter,
                                    gpointer    user_data);

enum _GomAdapterError
{
   GOM_ADAPTER_ERROR_OPEN = 1,
};

struct _GomAdapter
{
   GObject parent;

   /*< private >*/
   GomAdapterPrivate *priv;
};

struct _GomAdapterClass
{
   GObjectClass parent_class;
};

void        gom_adapter_close_async  (GomAdapter           *adapter,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
gboolean    gom_adapter_close_finish (GomAdapter           *adapter,
                                      GAsyncResult         *result,
                                      GError              **error);
GQuark      gom_adapter_error_quark  (void) G_GNUC_CONST;
gpointer    gom_adapter_get_handle   (GomAdapter           *adapter);
GType       gom_adapter_get_type     (void) G_GNUC_CONST;
GomAdapter *gom_adapter_new          (void);
void        gom_adapter_open_async   (GomAdapter           *adapter,
                                      const gchar          *uri,
                                      GAsyncReadyCallback   callback,
                                      gpointer              user_data);
gboolean    gom_adapter_open_finish  (GomAdapter           *adapter,
                                      GAsyncResult         *result,
                                      GError              **error);
void        gom_adapter_queue_read   (GomAdapter           *adapter,
                                      GomAdapterCallback    callback,
                                      gpointer              user_data);
void        gom_adapter_queue_write  (GomAdapter           *adapter,
                                      GomAdapterCallback    callback,
                                      gpointer              user_data);
gboolean    gom_adapter_execute_sql  (GomAdapter           *adapter,
                                      const gchar          *sql,
                                      GError              **error);

G_END_DECLS

#endif /* GOM_ADAPTER_H */
