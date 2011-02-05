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

gboolean
gom_adapter_create (GomAdapter     *adapter,
                    GomEnumerable  *enumerable,
                    GError        **error)
{
	return GOM_ADAPTER_GET_CLASS(adapter)->create(adapter, enumerable, error);
}
