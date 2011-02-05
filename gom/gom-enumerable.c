/* gom-enumerable.c
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

#include "gom-enumerable.h"

G_DEFINE_ABSTRACT_TYPE(GomEnumerable, gom_enumerable, G_TYPE_OBJECT)

static void
gom_enumerable_class_init (GomEnumerableClass *klass)
{
}

static void
gom_enumerable_init (GomEnumerable *enumerable)
{
}

/**
 * gom_enumerable_iter_init:
 * @enumerable: (in): A #GomEnumerable.
 * @iter: (out): A #GomEnumerableIter.
 *
 * Initializes a #GomEnumerableIter at the beginning of the enumerable.
 *
 * Returns: %TRUE if there is data to be read; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_enumerable_iter_init (GomEnumerableIter *iter,
                          GomEnumerable     *enumerable)
{
	return GOM_ENUMERABLE_GET_CLASS(enumerable)->iter_init(enumerable, iter);
}

/**
 * gom_enumerable_iter_next:
 * @enumerable: (in): A #GomEnumerable.
 * @iter: (out): A #GomEnumerableIter.
 *
 * Moves @iter to the next position in the enumerable.
 *
 * Returns: %TRUE if there is data to be read; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_enumerable_iter_next (GomEnumerableIter *iter)
{
	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(GOM_IS_ENUMERABLE(iter->enumerable), FALSE);

	return GOM_ENUMERABLE_GET_CLASS(iter->enumerable)->iter_next(iter->enumerable, iter);
}

/**
 * gom_enumerable_get_value:
 * @enumerable: (in): A #GomEnumerable.
 * @iter: (in): A #GomEnumerableIter.
 * @column: (in): The index of the column to retrieve; 0-based.
 * @value: (out): A location to store the boxed value.
 *
 * Retrieves the value in @column at the potition specified by @iter.
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_enumerable_get_value (GomEnumerable     *enumerable,
                          GomEnumerableIter *iter,
                          guint              column,
                          GValue            *value)
{
	GOM_ENUMERABLE_GET_CLASS(enumerable)->get_value(enumerable, iter,
	                                                column, value);
}

/**
 * gom_enumerable_get_n_columns:
 * @enumerable: (in): A #GomEnumerable.
 *
 * Retrieves the number of columns in this enumerable.
 *
 * Returns: A #guint containing the number of columns.
 * Side effects: None.
 */
guint
gom_enumerable_get_n_columns (GomEnumerable *enumerable)
{
	return GOM_ENUMERABLE_GET_CLASS(enumerable)->get_n_columns(enumerable);
}
