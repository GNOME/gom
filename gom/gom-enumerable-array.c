/* gom-enumerable-array.c
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

#include <string.h>

#include "gom-enumerable-array.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomEnumerableArray, gom_enumerable_array, GOM_TYPE_ENUMERABLE)

struct _GomEnumerableArrayPrivate
{
	GomResource **resources;
	guint         n_resources;
};

/**
 * gom_enumerable_array_finalize:
 * @object: (in): A #GomEnumerableArray.
 *
 * Finalizer for a #GomEnumerableArray instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_array_finalize (GObject *object)
{
	GomEnumerableArrayPrivate *priv = GOM_ENUMERABLE_ARRAY(object)->priv;

	gom_clear_pointer(&priv->resources, g_free);

	G_OBJECT_CLASS(gom_enumerable_array_parent_class)->finalize(object);
}

static gboolean
gom_enumerable_array_iter_init (GomEnumerable     *enumerable,
                                GomEnumerableIter *iter)
{
	GomEnumerableArray *array = (GomEnumerableArray *)enumerable;
	GomEnumerableArrayPrivate *priv;

	g_return_val_if_fail(GOM_IS_ENUMERABLE_ARRAY(array), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);

	priv = array->priv;

	iter->enumerable = enumerable;
	iter->data[0].v_int = 0;
	iter->data[1].v_int = priv->n_resources;
	iter->data[2].v_int = 0;

	return (priv->n_resources > 0);
}

static gboolean
gom_enumerable_array_iter_next (GomEnumerable     *enumerable,
                                GomEnumerableIter *iter)
{
	GomEnumerableArray *array = (GomEnumerableArray *)enumerable;
	GomEnumerableArrayPrivate *priv;

	g_return_val_if_fail(GOM_IS_ENUMERABLE_ARRAY(array), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);

	priv = array->priv;

	g_assert(iter->enumerable == enumerable);
	g_assert_cmpint(iter->data[1].v_int, ==, priv->n_resources);
	g_assert_cmpint(iter->data[0].v_int, <, iter->data[1].v_int);
	g_assert_cmpint(iter->data[2].v_int, >=, iter->data[0].v_int);

	iter->data[2].v_int++;

	return (iter->data[2].v_int < priv->n_resources);
}

static void
gom_enumerable_array_get_value (GomEnumerable     *enumerable,
                                GomEnumerableIter *iter,
                                gint               column,
                                GValue            *value)
{
	GomEnumerableArrayPrivate *priv;
	GomEnumerableArray *array = (GomEnumerableArray *)enumerable;

	g_return_if_fail(GOM_IS_ENUMERABLE_ARRAY(array));
	g_return_if_fail(iter != NULL);
	g_return_if_fail(value != NULL);
	g_return_if_fail(column == 0);

	priv = array->priv;

	g_assert(iter->enumerable == enumerable);
	g_assert_cmpint(iter->data[2].v_int, >=, 0);
	g_assert_cmpint(iter->data[2].v_int, <, priv->n_resources);

	g_value_init(value, GOM_TYPE_RESOURCE);
	g_value_set_object(value, priv->resources[iter->data[2].v_int]);
}

guint
gom_enumerable_array_get_n_columns (GomEnumerable *enumerable)
{
	return 1;
}

/**
 * gom_enumerable_array_class_init:
 * @klass: (in): A #GomEnumerableArrayClass.
 *
 * Initializes the #GomEnumerableArrayClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_array_class_init (GomEnumerableArrayClass *klass)
{
	GObjectClass *object_class;
	GomEnumerableClass *enumerable_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_enumerable_array_finalize;
	g_type_class_add_private(object_class, sizeof(GomEnumerableArrayPrivate));

	enumerable_class = GOM_ENUMERABLE_CLASS(klass);
	enumerable_class->iter_init = gom_enumerable_array_iter_init;
	enumerable_class->iter_next = gom_enumerable_array_iter_next;
	enumerable_class->get_value = gom_enumerable_array_get_value;
	enumerable_class->get_n_columns = gom_enumerable_array_get_n_columns;
}

/**
 * gom_enumerable_array_init:
 * @array: (in): A #GomEnumerableArray.
 *
 * Initializes the newly created #GomEnumerableArray instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_array_init (GomEnumerableArray *array)
{
	array->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(array,
		                            GOM_TYPE_ENUMERABLE_ARRAY,
		                            GomEnumerableArrayPrivate);
}

GomEnumerable*
gom_enumerable_array_new (GomResource **resources,
                          guint         n_resources)
{
	GomEnumerableArray *array;

	array = g_object_new(GOM_TYPE_ENUMERABLE_ARRAY, NULL);
	array->priv->resources = g_malloc_n(n_resources, sizeof *resources);
	memcpy(array->priv->resources, resources,
	       (sizeof *resources) * n_resources);
	array->priv->n_resources = n_resources;

	return GOM_ENUMERABLE(array);
}
