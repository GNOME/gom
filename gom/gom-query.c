/* gom-query.c
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

#include "gom-property-set.h"
#include "gom-query.h"

G_DEFINE_TYPE(GomQuery, gom_query, G_TYPE_OBJECT)

struct _GomQueryPrivate
{
	GomQueryDirection direction;
	GomPropertySet *fields;
	GPtrArray *relations;
	gboolean unique;
	guint64 offset;
	guint64 limit;
	GType resource_type;
};

enum
{
	PROP_0,
	PROP_DIRECTION,
	PROP_FIELDS,
	PROP_LIMIT,
	PROP_OFFSET,
	PROP_RELATIONS,
	PROP_RESOURCE_TYPE,
	PROP_UNIQUE,
	LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * gom_query_finalize:
 * @object: (in): A #GomQuery.
 *
 * Finalizer for a #GomQuery instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_query_finalize (GObject *object)
{
	G_OBJECT_CLASS(gom_query_parent_class)->finalize(object);
}

/**
 * gom_query_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_query_get_property (GObject    *object,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_query_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_query_set_property (GObject      *object,
                        guint         prop_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_query_class_init:
 * @klass: (in): A #GomQueryClass.
 *
 * Initializes the #GomQueryClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_query_class_init (GomQueryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_query_finalize;
	object_class->get_property = gom_query_get_property;
	object_class->set_property = gom_query_set_property;
	g_type_class_add_private(object_class, sizeof(GomQueryPrivate));

	gParamSpecs[PROP_DIRECTION] =
		g_param_spec_enum("direction",
		                  _("Direction"),
		                  _("The sort direction of the query"),
		                  GOM_TYPE_QUERY_DIRECTION,
		                  GOM_QUERY_ASCENDING,
		                  G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_DIRECTION,
	                                gParamSpecs[PROP_DIRECTION]);
}

/**
 * gom_query_init:
 * @query: (in): A #GomQuery.
 *
 * Initializes the newly created #GomQuery instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_query_init (GomQuery *query)
{
	query->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(query,
		                            GOM_TYPE_QUERY,
		                            GomQueryPrivate);
}

GType
gom_query_direction_get_type (void)
{
	static gsize initialized = FALSE;
	static GType type_id = 0;
	static const GEnumValue values[] = {
		{ GOM_QUERY_ASCENDING, "GOM_QUERY_ASCENDING", "ASCENDING" },
		{ GOM_QUERY_DESCENDING, "GOM_QUERY_DESCENDING", "DESCENDING" },
		{ 0 }
	};

	if (g_once_init_enter(&initialized)) {
		type_id = g_enum_register_static("GomQueryDirection", values);
		g_once_init_leave(&initialized, TRUE);
	}

	return type_id;
}
