/* gom-filter.h
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

#ifndef GOM_FILTER_H
#define GOM_FILTER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_TYPE_FILTER            (gom_filter_get_type())
#define GOM_TYPE_FILTER_MODE       (gom_filter_mode_get_type())
#define GOM_FILTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_FILTER, GomFilter))
#define GOM_FILTER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_FILTER, GomFilter const))
#define GOM_FILTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_FILTER, GomFilterClass))
#define GOM_IS_FILTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_FILTER))
#define GOM_IS_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_FILTER))
#define GOM_FILTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_FILTER, GomFilterClass))

enum _GomFilterMode
{
   GOM_FILTER_SQL = 1,
   GOM_FILTER_OR,
   GOM_FILTER_AND,
   GOM_FILTER_EQ,
   GOM_FILTER_NEQ,
   GOM_FILTER_GT,
   GOM_FILTER_GTE,
   GOM_FILTER_LT,
   GOM_FILTER_LTE,
   GOM_FILTER_LIKE,
   GOM_FILTER_GLOB
};

typedef struct _GomFilter        GomFilter;
typedef struct _GomFilterClass   GomFilterClass;
typedef struct _GomFilterPrivate GomFilterPrivate;
typedef enum   _GomFilterMode    GomFilterMode;

struct _GomFilter
{
   GObject parent;

   /*< private >*/
   GomFilterPrivate *priv;
};

struct _GomFilterClass
{
   GObjectClass parent_class;
};

GType        gom_filter_mode_get_type (void) G_GNUC_CONST;
GType        gom_filter_get_type      (void) G_GNUC_CONST;
gchar       *gom_filter_get_sql       (GomFilter    *filter,
                                       GHashTable   *table_map);
GArray      *gom_filter_get_values    (GomFilter    *filter);
GomFilter   *gom_filter_new_sql       (const gchar  *sql,
                                       GArray       *values);
GomFilter   *gom_filter_new_or        (GomFilter    *left,
                                       GomFilter    *right);
GomFilter   *gom_filter_new_or_full   (GomFilter    *first,
                                       ...);
GomFilter   *gom_filter_new_and       (GomFilter    *left,
                                       GomFilter    *right);
GomFilter   *gom_filter_new_and_full  (GomFilter    *first,
                                       ...);
GomFilter   *gom_filter_new_eq        (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_neq       (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_gt        (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_gte       (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_lt        (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_lte       (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_like      (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);
GomFilter   *gom_filter_new_glob      (GType         resource_type,
                                       const gchar  *property_name,
                                       const GValue *value);

G_END_DECLS

#endif /* GOM_FILTER_H */
