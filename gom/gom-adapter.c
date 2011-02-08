/* gom-adapter.c
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

#include "gom-adapter.h"
#include "gom-resource.h"

G_DEFINE_ABSTRACT_TYPE(GomAdapter, gom_adapter, G_TYPE_OBJECT)

static void
gom_adapter_class_init (GomAdapterClass *klass)
{
}

static void
gom_adapter_init (GomAdapter *adapter)
{
}

/**
 * gom_adapter_create:
 * @adapter: (in): A #GomAdapter.
 * @enumerable: (in): A #GomEnumerable.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * Creates a series of new #GomResource<!-- -->'s found in enumerable.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_create (GomAdapter     *adapter,
                    GomEnumerable  *enumerable,
                    GError        **error)
{
	return GOM_ADAPTER_GET_CLASS(adapter)->create(adapter, enumerable, error);
}

/**
 * gom_adapter_delete:
 * @adapter: (in): A #GomAdapter.
 * @collection: (in): A #GomCollection of #GomResource<!-- -->'s.
 * @error: (in): A location for a #GError, or %NULL.
 *
 * Deletes a resource from the underlying data adapter.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_delete (GomAdapter     *adapter,
                    GomCollection  *collection,
                    GError        **error)
{
	return GOM_ADAPTER_GET_CLASS(adapter)->delete(adapter, collection, error);
}

/**
 * gom_adapter_update:
 * @adapter: (in): A #GomAdapter.
 * @properties: (in): A set of properties to update.
 * @values: (in): The values for @properties.
 * @collection: (in): A collection of resources that should be updated.
 *
 * Updates the resources found in @collection. The properties found in
 * @properties are updated to the values found in @values at the
 * corresponding index.
 *
 * Returns: %TRUE if successful; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_adapter_update (GomAdapter     *adapter,
                    GomPropertySet *properties,
                    GValueArray    *values,
                    GomCollection  *collection,
                    GError        **error)
{
	return GOM_ADAPTER_GET_CLASS(adapter)->update(adapter, properties, values,
	                                              collection, error);
}
