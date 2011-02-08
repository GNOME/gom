/* gom/gom-condition.c
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

#include "gom-condition.h"

static GQuark gQuarkEqual;
static GQuark gQuarkAnd;
static GQuark gQuarkOr;

static void gom_condition_initialize (void);

static void
gom_condition_destroy (GomCondition *condition)
{
	g_return_if_fail(condition != NULL);

	if (condition->oper == gQuarkEqual) {
		if (G_IS_VALUE(&condition->u.equality.value)) {
			g_value_unset(&condition->u.equality.value);
		}
		condition->u.equality.property = NULL;
	} else if ((condition->oper == gQuarkAnd) ||
	           (condition->oper == gQuarkOr)) {
		gom_condition_unref(condition->u.boolean.left);
		gom_condition_unref(condition->u.boolean.right);
	} else {
		g_assert_not_reached();
	}
}

static GomCondition*
gom_condition_new (void)
{
	GomCondition *condition;

	gom_condition_initialize();

	condition = g_slice_new0(GomCondition);
	condition->ref_count = 1;

	return condition;
}

/**
 * gom_condition_ref:
 * @condition: (in): A #GomCondition.
 *
 * Increments the reference count of @condition by one.
 *
 * Returns: @condition.
 */
GomCondition*
gom_condition_ref (GomCondition *condition)
{
	g_return_val_if_fail(condition != NULL, NULL);
	g_return_val_if_fail(condition->ref_count > 0, NULL);

	g_atomic_int_inc(&condition->ref_count);
	return condition;
}

/**
 * gom_condition_unref:
 * @condition: (in): A #GomCondition.
 *
 * Unrefs the reference count of @condition by one. @condition is destroyed
 * once the reference count reaches zero.
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_condition_unref (GomCondition *condition)
{
	g_return_if_fail(condition != NULL);
	g_return_if_fail(condition->ref_count > 0);

	if (g_atomic_int_dec_and_test(&condition->ref_count)) {
		gom_condition_destroy(condition);
		g_slice_free(GomCondition, condition);
	}
}

/**
 * gom_condition_is_a:
 * @condition: (in): A #GomCondition.
 * @oper: (in): The #GQuark of the operator.
 *
 * Checks if @condition matches the operator @oper.
 *
 * Returns: %TRUE if condition matches @oper; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_condition_is_a (GomCondition *condition,
                    GQuark        oper)
{
	g_return_val_if_fail(condition != NULL, 0);
	return condition->oper == oper;
}

/**
 * gom_condition_equal:
 * @property: (in): A #GomProperty.
 * @value: (in): A #GValue.
 *
 * Creates a new #GomCondition checking that compares the property @property
 * with the value @value.
 *
 * Returns: A newly allocated #GomCondition that should be freed with
 *   gom_condition_unref().
 * Side effects: None.
 */
GomCondition*
gom_condition_equal (GomProperty  *property,
                     const GValue *value)
{
	GomCondition *condition;

	condition = gom_condition_new();
	condition->oper = gQuarkEqual;
	condition->u.equality.property = property;
	g_value_init(&condition->u.equality.value, G_VALUE_TYPE(value));
	g_value_copy(value, &condition->u.equality.value);

	return condition;
}

/**
 * gom_condition_and:
 * @left: (in): A #GomCondition.
 * @right: (in): A #GomCondition.
 *
 * Creates a new #GomCondition checking that ensures that both the right
 * and left condition are matched.
 *
 * Returns: A newly allocated #GomCondition that should be freed with
 *   gom_condition_unref().
 * Side effects: None.
 */
GomCondition*
gom_condition_and (GomCondition *right,
                   GomCondition *left)
{
	GomCondition *condition;

	condition = gom_condition_new();
	condition->oper = gQuarkAnd;
	condition->u.boolean.left = left;
	condition->u.boolean.right = right;

	return condition;
}

/**
 * gom_condition_or:
 * @left: (in): A #GomCondition.
 * @right: (in): A #GomCondition.
 *
 * Creates a new #GomCondition checking that either the @right or @left
 * condition are matched.
 *
 * Returns: A newly allocated #GomCondition that should be freed with
 *   gom_condition_unref().
 * Side effects: None.
 */
GomCondition*
gom_condition_or (GomCondition *right,
                  GomCondition *left)
{
	GomCondition *condition;

	condition = gom_condition_new();
	condition->oper = gQuarkOr;
	condition->u.boolean.left = left;
	condition->u.boolean.right = right;

	return condition;
}

static void
gom_condition_initialize (void)
{
	static gsize initialized = FALSE;

	/*
	 * Allocate our internal quarks.
	 */
	if (g_once_init_enter(&initialized)) {
		gQuarkAnd = g_quark_from_static_string(":and");
		gQuarkOr = g_quark_from_static_string(":or");
		gQuarkEqual = g_quark_from_static_string(":equal");
		g_once_init_leave(&initialized, TRUE);
	}
}

GType
gom_condition_get_type (void)
{
	static gsize initialized = FALSE;
	static GType type_id = 0;

	if (g_once_init_enter(&initialized)) {
		type_id =
			g_boxed_type_register_static(
				"GomCondition",
				(GBoxedCopyFunc)gom_condition_ref,
				(GBoxedFreeFunc)gom_condition_unref);
		g_once_init_leave(&initialized, TRUE);
	}

	return type_id;
}

GQuark
gom_condition_and_quark (void)
{
	gom_condition_initialize();
	return gQuarkAnd;
}

GQuark
gom_condition_or_quark (void)
{
	gom_condition_initialize();
	return gQuarkOr;
}

GQuark
gom_condition_equal_quark (void)
{
	gom_condition_initialize();
	return gQuarkEqual;
}
