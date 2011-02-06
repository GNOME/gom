/* gom-property-set.c
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

#include "gom-property-set.h"
#include "gom-util.h"

struct _GomPropertySet
{
	volatile gint ref_count;

	GomProperty **properties;
	guint         n_properties;
};

/**
 * gom_property_set_dispose:
 * @set: A #GomPropertySet.
 *
 * Cleans up the #GomPropertySet instance and frees any allocated resources.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_property_set_dispose (GomPropertySet *set) /* IN */
{
	g_free(set->properties);
}

/**
 * gom_property_set_newv:
 * @n_properties: (in): The length of the array.
 * @properties: (transfer full) (array length=n_properties): The properties.
 *
 * Creates a new #GomPropertySet using the array of properties. This method
 * claims ownership of the @properties array and should not be modified
 * by the caller after calling this function.
 *
 * Returns: #GomPropertySet which should be freed with gom_property_set_unref().
 * Side effects: None.
 */
GomPropertySet*
gom_property_set_newv (guint         n_properties,
                       GomProperty **properties)
{
	GomPropertySet *set;

	set = g_slice_new0(GomPropertySet);
	set->ref_count = 1;
	set->n_properties = n_properties;
	set->properties = properties;

	return set;
}

/**
 * gom_property_set_new:
 *
 * Creates a new instance of #GomPropertySet.
 *
 * Returns: the newly created instance which should be freed with
 *   gom_property_set_unref().
 * Side effects: None.
 */
GomPropertySet*
gom_property_set_new (GomProperty *first_property,
                      ...)
{
	GomPropertySet *set;
	GomProperty *property;
	GPtrArray *array;
	va_list args;

	g_return_val_if_fail(first_property != NULL, NULL);

	va_start(args, first_property);

	array = g_ptr_array_new();
	property = first_property;

	do {
		g_ptr_array_add(array, property);
	} while ((property = va_arg(args, GomProperty*)));

	va_end(args);

	set = gom_property_set_newv(array->len, (GomProperty **)array->pdata);
	g_ptr_array_free(array, FALSE);
	return set;
}

GomProperty*
gom_property_set_find (GomPropertySet *set,
                       GQuark          name)
{
	gint i;

	g_return_val_if_fail(set != NULL, NULL);
	g_return_val_if_fail(name != 0, NULL);

	for (i = 0; i < set->n_properties; i++) {
		if (set->properties[i]->name == name) {
			return set->properties[i];
		}
	}

	return NULL;
}

/**
 * gom_property_set_get_nth:
 * @set: (in): A #GomPropertySet.
 * @index: (in): The index to retrieve.
 *
 * Retrieves the #GomProperty from the #GomPropertySet at the given index.
 *
 * Returns: A #GomProperty.
 * Side effects: None.
 */
GomProperty*
gom_property_set_get_nth (GomPropertySet *set,
                          guint           nth)
{
	g_return_val_if_fail(set != NULL, NULL);
	g_return_val_if_fail(nth < set->n_properties, NULL);

	return set->properties[nth];
}

/**
 * gom_property_set_length:
 * @set: (in): A #GomPropertySet.
 *
 * Retrieves the length of the set.
 *
 * Returns: The length of the set.
 * Side effects: None.
 */
guint
gom_property_set_length (GomPropertySet *set)
{
	g_return_val_if_fail(set != NULL, 0);

	return set->n_properties;
}

/**
 * gom_property_set_add:
 * @set: (in): A #GomPropertySet.
 *
 * Adds an item to the set. This is a private, internal API call.
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_property_set_add (GomPropertySet *set,
                      GomProperty    *property)
{
	g_return_if_fail(set != NULL);
	g_return_if_fail(property != NULL);

	set->properties = g_realloc_n(set->properties, set->n_properties + 1,
	                              sizeof *set->properties);
	set->properties[set->n_properties++] = property;
}

/**
 * GomPropertySet_ref:
 * @set: A #GomPropertySet.
 *
 * Atomically increments the reference count of @set by one.
 *
 * Returns: A reference to @set.
 * Side effects: None.
 */
GomPropertySet*
gom_property_set_ref (GomPropertySet *set) /* IN */
{
	g_return_val_if_fail(set != NULL, NULL);
	g_return_val_if_fail(set->ref_count > 0, NULL);

	g_atomic_int_inc(&set->ref_count);

	return set;
}

/**
 * gom_property_set_unref:
 * @set: A GomPropertySet.
 *
 * Atomically decrements the reference count of @set by one.  When the
 * reference count reaches zero, the structure will be destroyed and
 * freed.
 *
 * Returns: None.
 * Side effects: The structure will be freed when the reference count
 *   reaches zero.
 */
void
gom_property_set_unref (GomPropertySet *set) /* IN */
{
	g_return_if_fail(set != NULL);
	g_return_if_fail(set->ref_count > 0);

	if (g_atomic_int_dec_and_test(&set->ref_count)) {
		gom_property_set_dispose(set);
		g_slice_free(GomPropertySet, set);
	}
}

GType
gom_property_set_get_type (void)
{
	static gsize initialized = FALSE;
	static GType type_id = 0;

	if (g_once_init_enter(&initialized)) {
		type_id = g_boxed_type_register_static(
				"GomPropertySet",
				(GBoxedCopyFunc)gom_property_set_ref,
				(GBoxedFreeFunc)gom_property_set_unref);
		g_once_init_leave(&initialized, TRUE);
	}

	return type_id;
}
