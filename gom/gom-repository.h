/* gom-repository.h
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

#ifndef GOM_REPOSITORY_H
#define GOM_REPOSITORY_H

#include <gio/gio.h>

#include "gom-adapter.h"
#include "gom-filter.h"
#include "gom-resource-group.h"

G_BEGIN_DECLS

#define GOM_TYPE_REPOSITORY            (gom_repository_get_type())
#define GOM_REPOSITORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_REPOSITORY, GomRepository))
#define GOM_REPOSITORY_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_REPOSITORY, GomRepository const))
#define GOM_REPOSITORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_REPOSITORY, GomRepositoryClass))
#define GOM_IS_REPOSITORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_REPOSITORY))
#define GOM_IS_REPOSITORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_REPOSITORY))
#define GOM_REPOSITORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_REPOSITORY, GomRepositoryClass))

typedef struct _GomRepository        GomRepository;
typedef struct _GomRepositoryClass   GomRepositoryClass;
typedef struct _GomRepositoryPrivate GomRepositoryPrivate;

typedef gboolean (*GomRepositoryMigrator) (GomRepository  *repository,
                                           GomAdapter     *adapter,
                                           guint           version,
                                           gpointer        user_data,
                                           GError        **error);

struct _GomRepository
{
   GObject parent;

   /*< private >*/
   GomRepositoryPrivate *priv;
};

struct _GomRepositoryClass
{
   GObjectClass parent_class;
};

GomAdapter       *gom_repository_get_adapter    (GomRepository          *repository);
GType             gom_repository_get_type       (void) G_GNUC_CONST;
GomRepository    *gom_repository_new            (GomAdapter             *adapter);
void              gom_repository_migrate_async  (GomRepository          *repository,
                                                 guint                   version,
                                                 GomRepositoryMigrator   migrator,
                                                 GAsyncReadyCallback     callback,
                                                 gpointer                user_data);
gboolean          gom_repository_migrate_finish (GomRepository          *repository,
                                                 GAsyncResult           *result,
                                                 GError                **error);
void              gom_repository_find_async     (GomRepository          *repository,
                                                 GType                   resource_type,
                                                 GomFilter              *filter,
                                                 GAsyncReadyCallback     callback,
                                                 gpointer                user_data);
GomResourceGroup *gom_repository_find_finish    (GomRepository          *repository,
                                                 GAsyncResult           *result,
                                                 GError                **error);

G_END_DECLS

#endif /* GOM_REPOSITORY_H */
