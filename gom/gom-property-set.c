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

#include <string.h>

#include "gom-property-set.h"
#include "gom-util.h"

typedef struct
{
	guint         n_properties;
	GomProperty **properties;
	volatile gint ref_count;
} GomPropertySetReal;

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
gom_property_set_dispose (GomPropertySet *set)
{
	GomPropertySetReal *real = (GomPropertySetReal *)set;
	g_free(real->properties);
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
	GomPropertySetReal *real;

	real = g_slice_new0(GomPropertySetReal);
	real->ref_count = 1;
	real->n_properties = n_properties;
	real->properties = properties;

	return (GomPropertySet *)real;
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

/**
 * gom_property_set_dup:
 * @set: (in): A #GomPropertySet.
 *
 * Copies the contents of a #GomPropertySet. The set should be freed using
 * gom_property_set_unref().
 *
 * Returns: A newly allocated #GomPropertySet.
 * Side effects: None.
 */
GomPropertySet*
gom_property_set_dup (GomPropertySet *set)
{
	GomPropertySetReal *real = (GomPropertySetReal *)set;
	GomPropertySetReal *new_set;

	g_return_val_if_fail(real != NULL, NULL);

	new_set = g_slice_new(GomPropertySetReal);
	new_set->ref_count = 1;
	new_set->n_properties = real->n_properties;
	new_set->properties = g_malloc_n(new_set->n_properties,
	                                 sizeof(GomProperty*));
	memcpy(new_set->properties, real->properties,
	       new_set->n_properties * sizeof(GomProperty*));

	return (GomPropertySet *)new_set;
}

/**
 * gom_property_set_find:
 * @set: (in): A #GomPropertySet.
 * @name: (in): The property to find.
 *
 * Locates the property named @name contained in @set.
 *
 * Returns: A #GomProperty if successful; otherwise %NULL.
 * Side effects: None.
 */
GomProperty*
gom_property_set_find (GomPropertySet *set,
                       const gchar    *name)
{
	return gom_property_set_findq(set, g_quark_from_string(name));
}

/**
 * gom_property_set_findq:
 * @set: (in): A #GomPropertySet.
 * @name: (in): A #GQuark of the property name.
 *
 * Locates the property named @name contained in @set.
 *
 * Returns: A #GomProperty if successful; otherwise %NULL.
 * Side effects: None.
 */
GomProperty*
gom_property_set_findq (GomPropertySet *set,
                        GQuark          name)
{
	GomPropertySetReal *real = (GomPropertySetReal *)set;
	gint i;

	g_return_val_if_fail(real != NULL, NULL);
	g_return_val_if_fail(name != 0, NULL);

	for (i = 0; i < real->n_properties; i++) {
		if (real->properties[i]->name == name) {
			return real->properties[i];
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
	GomPropertySetReal *real = (GomPropertySetReal *)set;

	g_return_val_if_fail(real != NULL, NULL);
	g_return_val_if_fail(nth < real->n_properties, NULL);

	return real->properties[nth];
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
	return set->len;
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
	GomPropertySetReal *real = (GomPropertySetReal *)set;

	g_return_if_fail(real != NULL);
	g_return_if_fail(property != NULL);

	real->properties = g_realloc_n(real->properties, set->len + 1,
	                              sizeof *real->properties);
	real->properties[real->n_properties++] = property;
}

/**
 * gom_property_set_remove:
 * @set: (in): A #GomPropertySet.
 * @property: (in): A #GomProperty.
 *
 * An internal method to remove a @property from @set.
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_property_set_remove (GomPropertySet *set,
                         GomProperty    *property)
{
	GomPropertySetReal *real = (GomPropertySetReal *)set;
	gint i;

	g_return_if_fail(real != NULL);
	g_return_if_fail(property != NULL);

	for (i = 0; i < set->len; i++) {
		if (real->properties[i] == property) {
			real->properties[i] = real->properties[set->len - 1];
			set->len--;
			real->properties = g_realloc_n(real->properties, set->len,
			                               sizeof *real->properties);
			return;
		}
	}

	g_critical("GomPropertySet did not contain %s",
	           g_quark_to_string(property->name));
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
gom_property_set_ref (GomPropertySet *set)
{
	GomPropertySetReal *real = (GomPropertySetReal *)set;

	g_return_val_if_fail(real!= NULL, NULL);
	g_return_val_if_fail(real->ref_count > 0, NULL);

	g_atomic_int_inc(&real->ref_count);

	return (GomPropertySet *)real;
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
gom_property_set_unref (GomPropertySet *set)
{
	GomPropertySetReal *real = (GomPropertySetReal *)set;

	g_return_if_fail(real != NULL);
	g_return_if_fail(real->ref_count > 0);

	if (g_atomic_int_dec_and_test(&real->ref_count)) {
		gom_property_set_dispose(set);
		g_slice_free(GomPropertySetReal, real);
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
