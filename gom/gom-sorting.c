/* gom-sorting.c
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

#include <stdarg.h>

#include "gom-sorting.h"
#include "gom-resource.h"

struct _GomSortingPrivate
{
   GQueue *order_by_terms;
};

G_DEFINE_TYPE_WITH_PRIVATE(GomSorting, gom_sorting, G_TYPE_INITIALLY_UNOWNED)

typedef struct GomOrderByTerm
{
   GParamSpec *pspec;
   GomSortingMode mode;
} GomOrderByTerm;

static void
gom_order_by_term_free (GomOrderByTerm *term)
{
   g_param_spec_unref(term->pspec);
   g_free(term);
}

static void
gom_sorting_finalize (GObject *object)
{
   GomSortingPrivate *priv = GOM_SORTING(object)->priv;

   if (priv->order_by_terms != NULL)
      g_queue_free_full(priv->order_by_terms, (GDestroyNotify) gom_order_by_term_free);

   G_OBJECT_CLASS(gom_sorting_parent_class)->finalize(object);
}

static void
gom_sorting_class_init (GomSortingClass *klass)
{
   GObjectClass *object_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = gom_sorting_finalize;
}

static void
gom_sorting_init (GomSorting *sorting)
{
   sorting->priv = gom_sorting_get_instance_private(sorting);
   sorting->priv->order_by_terms = g_queue_new();
}

GType
gom_sorting_mode_get_type (void)
{
   static GType g_type = 0;
   static gsize initialized = FALSE;
   static const GEnumValue values[] = {
      { GOM_SORTING_ASCENDING,  "GOM_SORTING_ASCENDING",  "ASCENDING" },
      { GOM_SORTING_DESCENDING, "GOM_SORTING_DESCENDING", "DESCENDING" },
      { 0 }
   };

   if (g_once_init_enter(&initialized)) {
      g_type = g_enum_register_static("GomSortingMode", values);
      g_once_init_leave(&initialized, TRUE);
   }

   return g_type;
}

static gchar *
get_table (GParamSpec *pspec,
           GHashTable *table_map)
{
   GType type;
   GomResourceClass *klass;
   gchar *table;
   gchar *key;

   type = pspec->owner_type;
   g_return_val_if_fail(g_type_is_a(type, GOM_TYPE_RESOURCE), NULL);

   klass = g_type_class_ref(type);
   key = g_strdup_printf("%s.%s", g_type_name(type), klass->table);
   if (table_map && (table = g_hash_table_lookup(table_map, key))) {
      table = g_strdup(table);
   } else {
      table = g_strdup(klass->table);
   }
   g_free(key);
   g_type_class_unref(klass);

   return table;
}

/**
 * gom_sorting_new: (constructor)
 * @first_resource_type: A subclass of #GomResource.
 * @first_property_name: A string.
 * @first_sorting_mode: A #GomSortingMode.
 * @...: Additional triples of resource_type/property_name/sorting_mode,
 *       followed by %NULL.
 *
 * Creates a new #GomSorting to instance.
 *
 * This is useful to sort query results, as #GomSorting knows how to return
 * the proper "ORDER BY" SQL statements.
 *
 * Example:
 * |[<!-- language="C" -->
 *     GomSorting *sorting = gom_sorting_new(EPISODE_TYPE_RESOURCE,
 *                                           "season-number",
 *                                           GOM_SORTING_DESCENDING,
 *                                           EPISODE_TYPE_RESOURCE,
 *                                           "episode-number",
 *                                           GOM_SORTING_ASCENDING);
 * ]|
 *
 * The above example maps to the following SQL statement:
 * |[<!-- language="SQL" -->
 *     ORDER BY 'episodes'.'season-number' DESC, 'episodes'.'episode-number'
 * ]|
 *
 * Returns: (transfer full): A #GomSorting.
 */
GomSorting *
gom_sorting_new (GType           first_resource_type,
                 const gchar    *first_property_name,
                 GomSortingMode  first_sorting_mode,
                 ...)
{
   GomSorting *sorting;
   va_list args;
   GType resource_type;
   const gchar *property_name;
   GomSortingMode sorting_mode;

   g_return_val_if_fail(g_type_is_a(first_resource_type, GOM_TYPE_RESOURCE),
                        NULL);

   sorting = g_object_new(GOM_TYPE_SORTING, NULL);

   resource_type = first_resource_type;
   property_name = first_property_name;
   sorting_mode = first_sorting_mode;

   va_start(args, first_sorting_mode);

   while TRUE {
      gom_sorting_add(sorting, resource_type, property_name, sorting_mode);

      resource_type = va_arg(args, GType);

      if (!resource_type)
         break;

      property_name = va_arg(args, const gchar*);
      sorting_mode = va_arg(args, GomSortingMode);
   }

   va_end(args);

   return sorting;
}

/**
 * gom_sorting_add:
 * @sorting: A #GomSorting.
 * @resource_type: A subclass of #GomResource.
 * @property_name: A string.
 * @sorting_mode: A #GomSortingMode.
 *
 * Add a new ORDER BY clause to the sorting object.
 *
 * This allows chaining ORDER BY clauses, adding them one at a time, rather
 * than passing them all to the constructor.
 *
 * Example:
 * |[<!-- language="C" -->
 *     GomSorting *sorting = g_object_new (GOM_TYPE_SORTING, NULL);
 *     gom_sorting_add (sorting, EPISODE_TYPE_RESOURCE, "season-number",
 *                      GOM_SORTING_DESCENDING);
 *     gom_sorting_add (sorting, EPISODE_TYPE_RESOURCE, "episode-number",
 *                      GOM_SORTING_ASCENDING);
 * ]|
 *
 * The above example maps to the following SQL statement:
 * |[<!-- language="SQL" -->
 *     ORDER BY 'episodes'.'season-number' DESC, 'episodes'.'episode-number'
 * ]|
 */
void
gom_sorting_add (GomSorting     *sorting,
                 GType           resource_type,
                 const gchar    *property_name,
                 GomSortingMode  sorting_mode)
{
   GObjectClass *klass;
   GParamSpec *pspec;
   GomOrderByTerm *o = g_new(GomOrderByTerm, 1);

   g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
   g_return_if_fail(property_name != NULL);
   g_return_if_fail(sorting_mode);

   klass = g_type_class_ref(resource_type);
   pspec = g_object_class_find_property(klass, property_name);
   g_type_class_unref(klass);

   if (!pspec) {
      g_warning("No such property %s::%s",
                g_type_name(resource_type), property_name);
      return;
   }

   o->pspec = g_param_spec_ref(pspec);
   o->mode = sorting_mode;
   g_queue_push_tail(sorting->priv->order_by_terms, o);
}

/**
 * gom_sorting_get_sql:
 * @sorting: (in): A #GomSorting.
 * @table_map: (in): A #GHashTable.
 *
 * Returns: (transfer full): A string containing the SQL query corresponding
 *                           to this @sorting.
 */
gchar *
gom_sorting_get_sql (GomSorting *sorting,
                     GHashTable *table_map)
{
   GomSortingPrivate *priv;
   gchar *table;
   gchar **sqls;
   gint i, len;
   gchar *ret;

   g_return_val_if_fail(GOM_IS_SORTING(sorting), NULL);

   priv = sorting->priv;
   len = g_queue_get_length(priv->order_by_terms);
   sqls = g_new(gchar *, len + 1);

   for (i = 0; i < len; i++) {
      GomOrderByTerm *o = g_queue_peek_nth(priv->order_by_terms, i);
      table = get_table(o->pspec, table_map);

      sqls[i] = g_strdup_printf("'%s'.'%s'%s", table, o->pspec->name, o->mode == GOM_SORTING_DESCENDING ? " DESC" : "");
   }
   sqls[i] = NULL;

   ret = g_strjoinv(", ", sqls);
   g_strfreev(sqls);

   return ret;
}
