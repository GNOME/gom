/* adapters/sqlite/gom-adapter-sqlite.h
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

#ifndef GOM_ADAPTER_SQLITE_H
#define GOM_ADAPTER_SQLITE_H

#include <glib-object.h>

#include "gom-adapter.h"

G_BEGIN_DECLS

#define GOM_TYPE_ADAPTER_SQLITE            (gom_adapter_sqlite_get_type())
#define GOM_ADAPTER_SQLITE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ADAPTER_SQLITE, GomAdapterSqlite))
#define GOM_ADAPTER_SQLITE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_ADAPTER_SQLITE, GomAdapterSqlite const))
#define GOM_ADAPTER_SQLITE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_ADAPTER_SQLITE, GomAdapterSqliteClass))
#define GOM_IS_ADAPTER_SQLITE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_ADAPTER_SQLITE))
#define GOM_IS_ADAPTER_SQLITE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_ADAPTER_SQLITE))
#define GOM_ADAPTER_SQLITE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_ADAPTER_SQLITE, GomAdapterSqliteClass))
#define GOM_ADAPTER_SQLITE_ERROR           (gom_adapter_sqlite_error_quark())

typedef struct _GomAdapterSqlite        GomAdapterSqlite;
typedef struct _GomAdapterSqliteClass   GomAdapterSqliteClass;
typedef struct _GomAdapterSqlitePrivate GomAdapterSqlitePrivate;
typedef enum   _GomAdapterSqliteError   GomAdapterSqliteError;

enum _GomAdapterSqliteError
{
	GOM_ADAPTER_SQLITE_ERROR_ALREADY_OPEN = 1,
	GOM_ADAPTER_SQLITE_ERROR_OPEN_FAILED,
	GOM_ADAPTER_SQLITE_ERROR_NOT_OPEN,
	GOM_ADAPTER_SQLITE_ERROR_INVALID_TYPE,
	GOM_ADAPTER_SQLITE_ERROR_SQLITE,
	GOM_ADAPTER_SQLITE_ERROR_INVALID_PROPERTIES,
};

struct _GomAdapterSqlite
{
	GomAdapter parent;

	/*< private >*/
	GomAdapterSqlitePrivate *priv;
};

struct _GomAdapterSqliteClass
{
	GomAdapterClass parent_class;
};

GQuark   gom_adapter_sqlite_error_quark    (void) G_GNUC_CONST;
GType    gom_adapter_sqlite_get_type       (void) G_GNUC_CONST;
gboolean gom_adapter_sqlite_load_from_file (GomAdapterSqlite  *sqlite,
                                            const gchar       *filename,
                                            GError           **error);
void     gom_adapter_sqlite_close          (GomAdapterSqlite  *sqlite);
gboolean gom_adapter_sqlite_create_table   (GomAdapterSqlite  *sqlite,
                                            GType              resource_type,
                                            GError           **error);
void     gom_adapter_sqlite_begin          (GomAdapterSqlite  *sqlite);
gboolean gom_adapter_sqlite_commit         (GomAdapterSqlite  *sqlite,
                                            GError           **error);
void     gom_adapter_sqlite_rollback       (GomAdapterSqlite  *sqlite);

G_END_DECLS

#endif /* GOM_ADAPTER_SQLITE_H */
