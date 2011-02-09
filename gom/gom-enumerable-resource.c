/* gom-enumerable-resource.c
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

#include <glib/gi18n.h>

#include "gom-enumerable-resource.h"
#include "gom-query.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomEnumerableResource,
              gom_enumerable_resource,
              GOM_TYPE_ENUMERABLE)

/*
 * Structs and enums.
 */

struct _GomEnumerableResourcePrivate
{
	GomEnumerable *enumerable;
	GomQuery      *query;
};

enum
{
	PROP_0,
	PROP_ENUMERABLE,
	PROP_QUERY,
	LAST_PROP
};

/*
 * Globals.
 */

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * gom_enumerable_resource_finalize:
 * @object: (in): A #GomEnumerableResource.
 *
 * Finalizer for a #GomEnumerableResource instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_resource_finalize (GObject *object)
{
	GomEnumerableResourcePrivate *priv = GOM_ENUMERABLE_RESOURCE(object)->priv;

	gom_clear_object(&priv->enumerable);
	gom_clear_object(&priv->query);

	G_OBJECT_CLASS(gom_enumerable_resource_parent_class)->finalize(object);
}

/**
 * gom_enumerable_resource_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_enumerable_resource_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	GomEnumerableResource *resource = GOM_ENUMERABLE_RESOURCE(object);

	switch (prop_id) {
	case PROP_ENUMERABLE:
		g_value_set_object(value, resource->priv->enumerable);
		break;
	case PROP_QUERY:
		g_value_set_object(value, resource->priv->query);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_enumerable_resource_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_enumerable_resource_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	GomEnumerableResource *resource = GOM_ENUMERABLE_RESOURCE(object);

	switch (prop_id) {
	case PROP_ENUMERABLE:
		resource->priv->enumerable = g_value_dup_object(value);
		break;
	case PROP_QUERY:
		resource->priv->query = g_value_dup_object(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_enumerable_resource_class_init:
 * @klass: (in): A #GomEnumerableResourceClass.
 *
 * Initializes the #GomEnumerableResourceClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_resource_class_init (GomEnumerableResourceClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_enumerable_resource_finalize;
	object_class->get_property = gom_enumerable_resource_get_property;
	object_class->set_property = gom_enumerable_resource_set_property;
	g_type_class_add_private(object_class, sizeof(GomEnumerableResourcePrivate));

	gParamSpecs[PROP_ENUMERABLE] =
		g_param_spec_object("enumerable",
		                    _("Enumerable"),
		                    _("The result enumerable from the adapter."),
		                    GOM_TYPE_ENUMERABLE,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_ENUMERABLE,
	                                gParamSpecs[PROP_ENUMERABLE]);

	gParamSpecs[PROP_QUERY] =
		g_param_spec_object("query",
		                    _("Query"),
		                    _("Query performed to retrieve the enumerable."),
		                    GOM_TYPE_QUERY,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_QUERY,
	                                gParamSpecs[PROP_QUERY]);
}

/**
 * gom_enumerable_resource_init:
 * @resource: (in): A #GomEnumerableResource.
 *
 * Initializes the newly created #GomEnumerableResource instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_resource_init (GomEnumerableResource *resource)
{
	resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
	                                     GOM_TYPE_ENUMERABLE_RESOURCE,
	                                     GomEnumerableResourcePrivate);

	
}
