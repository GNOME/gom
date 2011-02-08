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

#include "gom-condition.h"
#include "gom-property-set.h"
#include "gom-query.h"
#include "gom-resource.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomQuery, gom_query, G_TYPE_OBJECT)

/*
 * Structures and enums.
 */

struct _GomQueryPrivate
{
	GomQueryDirection direction;
	GomPropertySet *fields;
	GomCondition *condition;
	GPtrArray *relations;
	gboolean unique;
	guint64 offset;
	guint64 limit;
	GType resource_type;
};

enum
{
	PROP_0,
	PROP_CONDITION,
	PROP_DIRECTION,
	PROP_FIELDS,
	PROP_LIMIT,
	PROP_OFFSET,
	/* TODO: */PROP_RELATIONS,
	PROP_RESOURCE_TYPE,
	PROP_UNIQUE,
	LAST_PROP
};

/*
 * Forward declarations.
 */

static void gom_query_finalize          (GObject           *object);
static void gom_query_get_property      (GObject           *object,
                                         guint              prop_id,
                                         GValue            *value,
                                         GParamSpec        *pspec);
static void gom_query_set_condition     (GomQuery          *query,
                                         GomCondition      *condition);
static void gom_query_set_direction     (GomQuery          *query,
                                         GomQueryDirection  direction);
static void gom_query_set_fields        (GomQuery          *query,
                                         GomPropertySet    *set);
static void gom_query_set_limit         (GomQuery          *query,
                                         guint64            limit);
static void gom_query_set_offset        (GomQuery          *query,
                                         guint64            offset);
static void gom_query_set_property      (GObject           *object,
                                         guint              prop_id,
                                         const GValue      *value,
                                         GParamSpec        *pspec);
static void gom_query_set_resource_type (GomQuery          *query,
                                         GType              resource_type);
static void gom_query_set_unique        (GomQuery          *query,
                                         gboolean           unique);

/*
 * Globals.
 */

static GParamSpec *gParamSpecs[LAST_PROP];

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

	gParamSpecs[PROP_CONDITION] =
		g_param_spec_boxed("condition",
		                   _("Condition"),
		                   _("The condition for the query."),
		                   GOM_TYPE_CONDITION,
		                   G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_CONDITION,
	                                gParamSpecs[PROP_CONDITION]);

	gParamSpecs[PROP_DIRECTION] =
		g_param_spec_enum("direction",
		                  _("Direction"),
		                  _("The sort direction of the query"),
		                  GOM_TYPE_QUERY_DIRECTION,
		                  GOM_QUERY_DEFAULT,
		                  G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_DIRECTION,
	                                gParamSpecs[PROP_DIRECTION]);

	gParamSpecs[PROP_FIELDS] =
		g_param_spec_boxed("fields",
		                   _("Fields"),
		                   _("The fields to retrieve in the query."),
		                   GOM_TYPE_PROPERTY_SET,
		                   G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_FIELDS,
	                                gParamSpecs[PROP_FIELDS]);

	gParamSpecs[PROP_LIMIT] =
		g_param_spec_uint64("limit",
		                    _("Limit"),
		                    _("The limited number of rows."),
		                    0,
		                    G_MAXUINT64,
		                    0,
		                    G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_LIMIT,
	                                gParamSpecs[PROP_LIMIT]);

	gParamSpecs[PROP_OFFSET] =
		g_param_spec_uint64("offset",
		                    _("Limit"),
		                    _("The offset to the first result."),
		                    0,
		                    G_MAXUINT64,
		                    0,
		                    G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_OFFSET,
	                                gParamSpecs[PROP_OFFSET]);

	gParamSpecs[PROP_RESOURCE_TYPE] =
		g_param_spec_gtype("resource-type",
		                   _("Resource Type"),
		                   _("The resource type to query."),
		                   GOM_TYPE_RESOURCE,
		                   G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_RESOURCE_TYPE,
	                                gParamSpecs[PROP_RESOURCE_TYPE]);

	gParamSpecs[PROP_UNIQUE] =
		g_param_spec_boolean("unique",
		                     _("Unique"),
		                     _("Specifies that each row must be unique."),
		                     FALSE,
		                     G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_UNIQUE,
	                                gParamSpecs[PROP_UNIQUE]);
}

GType
gom_query_direction_get_type (void)
{
	static gsize initialized = FALSE;
	static GType type_id = 0;
	static const GEnumValue values[] = {
		{ GOM_QUERY_DEFAULT, "GOM_QUERY_DEFAULT", "DEFAULT" },
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
	GomQuery *query = GOM_QUERY(object);

	switch (prop_id) {
	case PROP_CONDITION:
		g_value_set_boxed(value, query->priv->condition);
		break;
	case PROP_DIRECTION:
		g_value_set_enum(value, query->priv->direction);
		break;
	case PROP_FIELDS:
		g_value_set_boxed(value, query->priv->fields);
		break;
	case PROP_LIMIT:
		g_value_set_uint64(value, query->priv->limit);
		break;
	case PROP_OFFSET:
		g_value_set_uint64(value, query->priv->offset);
		break;
	case PROP_RESOURCE_TYPE:
		g_value_set_gtype(value, query->priv->resource_type);
		break;
	case PROP_UNIQUE:
		g_value_set_boolean(value, query->priv->unique);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
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

static void
gom_query_set_condition (GomQuery     *query,
                         GomCondition *condition)
{
	GomQueryPrivate *priv;

	g_return_if_fail(condition != NULL);

	priv = query->priv;

	if (condition != priv->condition) {
		gom_clear_pointer(&priv->condition, gom_condition_unref);
		if (condition) {
			priv->condition = gom_condition_ref(condition);
		}
	}
}

static void
gom_query_set_direction (GomQuery          *query,
                         GomQueryDirection  direction)
{
	GomQueryPrivate *priv;

	g_return_if_fail(GOM_IS_QUERY(query));
	g_return_if_fail(direction >= GOM_QUERY_DEFAULT);
	g_return_if_fail(direction <= GOM_QUERY_DESCENDING);

	priv = query->priv;

	priv->direction = direction;
}

static void
gom_query_set_fields (GomQuery       *query,
                      GomPropertySet *set)
{
	GomQueryPrivate *priv;

	g_return_if_fail(GOM_IS_QUERY(query));

	priv = query->priv;

	gom_clear_pointer(&priv->fields, gom_property_set_unref);
	priv->fields = gom_property_set_ref(set);
}

static void
gom_query_set_limit (GomQuery *query,
                     guint64   limit)
{
	GomQueryPrivate *priv;

	g_return_if_fail(GOM_IS_QUERY(query));

	priv = query->priv;

	priv->limit = limit;
	g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_LIMIT]);
}

static void
gom_query_set_offset (GomQuery *query,
                      guint64   offset)
{
	GomQueryPrivate *priv;

	g_return_if_fail(GOM_IS_QUERY(query));

	priv = query->priv;

	priv->offset = offset;
	g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_OFFSET]);
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
	GomQuery *query = GOM_QUERY(object);

	switch (prop_id) {
	case PROP_CONDITION:
		gom_query_set_condition(query, g_value_get_boxed(value));
		break;
	case PROP_DIRECTION:
		gom_query_set_direction(query, g_value_get_enum(value));
		break;
	case PROP_FIELDS:
		gom_query_set_fields(query, g_value_get_boxed(value));
		break;
	case PROP_LIMIT:
		gom_query_set_limit(query, g_value_get_uint64(value));
		break;
	case PROP_OFFSET:
		gom_query_set_offset(query, g_value_get_uint64(value));
		break;
	case PROP_RESOURCE_TYPE:
		gom_query_set_resource_type(query, g_value_get_gtype(value));
		break;
	case PROP_UNIQUE:
		gom_query_set_unique(query, g_value_get_boolean(value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
gom_query_set_resource_type (GomQuery *query,
                             GType     resource_type)
{
	GomQueryPrivate *priv;

	g_return_if_fail(GOM_IS_QUERY(query));

	priv = query->priv;

	priv->resource_type = resource_type;
	g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_RESOURCE_TYPE]);
}

static void
gom_query_set_unique (GomQuery *query,
                      gboolean  unique)
{
	GomQueryPrivate *priv;

	g_return_if_fail(GOM_IS_QUERY(query));

	priv = query->priv;

	priv->unique = unique;
	g_object_notify_by_pspec(G_OBJECT(query), gParamSpecs[PROP_UNIQUE]);
}
