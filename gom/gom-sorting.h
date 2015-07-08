/* gom-sorting.h
 *
 * Copyright (C) 2015 Mathieu Bridon <bochecha@daitauha.fr>
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

#ifndef GOM_SORTING_H
#define GOM_SORTING_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_TYPE_SORTING            (gom_sorting_get_type())
#define GOM_TYPE_SORTING_MODE       (gom_sorting_mode_get_type())
#define GOM_SORTING(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_SORTING, GomSorting))
#define GOM_SORTING_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_SORTING, GomSorting const))
#define GOM_SORTING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_SORTING, GomSortingClass))
#define GOM_IS_SORTING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_SORTING))
#define GOM_IS_SORTING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_SORTING))
#define GOM_SORTING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_SORTING, GomSortingClass))

typedef struct _GomSorting        GomSorting;
typedef struct _GomSortingClass   GomSortingClass;
typedef struct _GomSortingPrivate GomSortingPrivate;
typedef enum   _GomSortingMode    GomSortingMode;

struct _GomSorting
{
   GObject parent;
   GomSortingPrivate *priv;
};

struct _GomSortingClass
{
   GObjectClass parent_class;
};

enum _GomSortingMode
{
   GOM_SORTING_ASCENDING = 1,
   GOM_SORTING_DESCENDING
};

GType       gom_sorting_get_type      (void) G_GNUC_CONST;
GType       gom_sorting_mode_get_type (void) G_GNUC_CONST;
gchar      *gom_sorting_get_sql       (GomSorting     *sorting,
                                       GHashTable     *table_map);
GomSorting *gom_sorting_new           (GType           first_resource_type,
                                       const gchar    *first_property_name,
                                       GomSortingMode  first_sorting_mode,
                                       ...);
void gom_sorting_add                  (GomSorting     *sorting,
                                       GType           resource_type,
                                       const gchar    *property_name,
                                       GomSortingMode  sorting_mode);

G_END_DECLS

#endif /* GOM_SORTING_H */
