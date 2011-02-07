/* gom-resource.c
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
#include <gobject/gvaluecollector.h>
#include <string.h>

#include "gom-condition.h"
#include "gom-enumerable.h"
#include "gom-enumerable-array.h"
#include "gom-private.h"
#include "gom-resource.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomResource, gom_resource, G_TYPE_OBJECT)

/*
 * Structures and enums.
 */

struct _GomResourcePrivate
{
	GomAdapter *adapter;
	gboolean    is_new;
	GHashTable *properties;
};

enum
{
	PROP_0,
	PROP_ADAPTER,
	LAST_PROP
};

/*
 * Forward declarations.
 */

static void gom_resource_finalize          (GObject      *object);
static void gom_resource_real_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);
static void gom_resource_real_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);

/*
 * Globals.
 */

static GParamSpec *gParamSpecs[LAST_PROP];
static GHashTable *gMetaByType;

/**
 * _property_value_free:
 * @data: (in): A #GomPropertyValue or %NULL.
 *
 * #GDestroyNotify typed function to unset and free a #GomPropertyValue.
 *
 * Returns: None.
 * Side effects: @data is freed if it is a valid pointer.
 */
static void
_property_value_free (gpointer data)
{
	GomPropertyValue *value = (GomPropertyValue *)data;

	if (value) {
		if (G_IS_VALUE(&value->value)) {
			g_value_unset(&value->value);
		}
		g_slice_free(GomPropertyValue, value);
	}
}

/**
 * _property_value_new:
 *
 * Allocates a new #GomPropertyValue. Use _property_value_free() to free
 * the structure.
 *
 * Returns: A new #GomPropertyValue.
 * Side effects: None.
 */
static GomPropertyValue*
_property_value_new (void)
{
	return g_slice_new0(GomPropertyValue);
}

/**
 * gom_resource_delete:
 * @resource: (in): A #GomResource.
 *
 * Deletes a resource from the underlying data store.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 * Side effects: None.
 */
gboolean
gom_resource_delete (GomResource  *resource,
                     GError      **error)
{
	GomResourcePrivate *priv;
	GomResourceClass *resource_class;
	GomPropertySet *set;
	GomCollection *collection;
	GomCondition *condition = NULL;
	GomCondition *conditions = NULL;
	GomProperty *prop;
	GomQuery *query;
	gboolean ret = FALSE;
	GValue value = { 0 };
	GType resource_type;
	guint n_props;
	gint i;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = resource->priv;

	if (priv->is_new) {
		return TRUE;
	}

	resource_type = G_TYPE_FROM_INSTANCE(resource);
	resource_class = g_type_class_peek(resource_type);
	set = gom_resource_class_get_properties(resource_class);
	n_props = gom_property_set_length(set);

	for (i = 0; i < n_props; i++) {
		prop = gom_property_set_get_nth(set, i);
		if (prop->is_key) {
			g_value_init(&value, prop->value_type);
			g_object_get_property(G_OBJECT(resource),
			                      g_quark_to_string(prop->name),
			                      &value);
			condition = gom_condition_equal(prop, &value);
			g_value_unset(&value);
			conditions = conditions ?
			             gom_condition_and(conditions, condition) :
			             gom_condition_ref(condition);
			gom_condition_unref(condition);
		}
	}

	query = g_object_new(GOM_TYPE_QUERY,
	                     "condition", condition,
	                     "limit", G_GUINT64_CONSTANT(1),
	                     "resource-type", resource_type,
	                     NULL);
	collection = g_object_new(GOM_TYPE_COLLECTION,
	                          "query", query,
	                          NULL);

	ret = gom_adapter_delete(priv->adapter, collection, error);

	gom_clear_object(&query);
	gom_clear_object(&collection);
	gom_clear_pointer(&conditions, gom_condition_unref);

	return ret;
}

/**
 * gom_resource_error_quark:
 *
 * Retrieves the error quark for #GomResourceError<!-- -->'s.
 *
 * Returns: A #GQuark.
 * Side effects: None.
 */
GQuark
gom_resource_error_quark (void)
{
	return g_quark_from_string("gom_resource_error_quark");
}

/**
 * gom_resource_class_get_meta:
 * @resource_class: (in): A #GomResourceClass.
 *
 * Retrieves the #GomResourceClassMeta for a particular class. If it is
 * not yet created, then it will be created and cached for later use.
 *
 * Returns: A #GomResourceClassMeta corresponding to @resource_class.
 * Side effects: None.
 */
GomResourceClassMeta*
gom_resource_class_get_meta (GomResourceClass *resource_class)
{
	GomResourceClassMeta *meta;
	GType gtype;

	g_return_val_if_fail(GOM_IS_RESOURCE_CLASS(resource_class), NULL);

	gtype = G_TYPE_FROM_CLASS(resource_class);

	/*
	 * If the meta introspection has not yet been created, create it now.
	 */
	if (!(meta = g_hash_table_lookup(gMetaByType, &gtype))) {
		meta = g_new0(GomResourceClassMeta, 1);
		meta->type = gtype;
		meta->properties = gom_property_set_newv(0, NULL);
		g_hash_table_insert(gMetaByType, &meta->type, meta);
	}

	return meta;
}

/**
 * gom_resource_class_get_properties:
 * @resource_class: (in): A #GomResourceClass.
 *
 * Retrieves a #GomPropertySet containing all the registered properties
 * for the given #GomResourceClass.
 *
 * Returns: A #GomPropertySet or %NULL if no properties are registered.
 * Side effects: None.
 */
GomPropertySet*
gom_resource_class_get_properties (GomResourceClass *resource_class)
{
	GomResourceClassMeta *meta;

	g_return_val_if_fail(GOM_IS_RESOURCE_CLASS(resource_class), NULL);

	meta = gom_resource_class_get_meta(resource_class);
	return gom_property_set_ref(meta->properties);
}

/**
 * gom_resource_class_has_many:
 * @resource_class: (in): A #GomResourceClass.
 * @property_name: (in): The property name.
 * @property_nick: (in): The property nick.
 * @property_desc: (in): The property description.
 * @resource_type: (in): The #GType of the related type.
 * ...: (in): Optional two-part tuples of special options, followed by %NULL.
 *
 * Configures a "has-many" property for the #GomResource based class.
 * Additionally, various options can be specified at the end of the
 * paramter list such has "many-to-many".
 *
 * [[
 * gom_resource_class_has_many(klass, "name", "name", "name", FOO_TYPE_BAZ,
 *                             "many-to-many", TRUE,
 *                             NULL);
 * ]]
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_resource_class_has_many (GomResourceClass *resource_class,
                             const gchar      *property_name,
                             const gchar      *property_nick,
                             const gchar      *property_desc,
                             GType             resource_type,
                             ...)
{
	GomResourceClassMeta *meta;
	GomProperty *property;
	const gchar *option;
	va_list args;
	GValue value = { 0 };
	GType this_type;
	gchar *errstr = NULL;

	g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
	g_return_if_fail(property_name != NULL);
	g_return_if_fail(property_nick != NULL);
	g_return_if_fail(property_desc != NULL);
	g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
	g_return_if_fail(resource_type != GOM_TYPE_RESOURCE);

	this_type = G_TYPE_FROM_CLASS(resource_class);
	meta = gom_resource_class_get_meta(resource_class);

	/*
	 * Create a new property for the collection.
	 */
	property = g_new0(GomProperty, 1);
	property->name = g_quark_from_string(property_name);
	property->owner_type = this_type;
	property->value_type = resource_type;
	g_value_init(&property->default_value, GOM_TYPE_COLLECTION);
	property->relationship.relation = GOM_RELATION_ONE_TO_MANY;

	va_start(args, resource_type);

	/*
	 * Handle additional key/value options.
	 */
	while ((option = va_arg(args, const gchar *))) {
		if (!g_strcmp0(option, "many-to-many")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errstr);
			if (errstr) {
				g_error("%s", errstr); /* Fatal */
				g_assert_not_reached();
				g_free(errstr);
				return;
			}
			if (g_value_get_boolean(&value)) {
				property->relationship.relation = GOM_RELATION_MANY_TO_MANY;
			}
			g_value_unset(&value);
		} else {
			g_error("Unknown has_many option %s", option); /* Fatal */
			g_assert_not_reached();
			break;
		}
	}

	va_end(args);

	/*
	 * Add the property to the meta data.
	 */
	gom_property_set_add(meta->properties, property);

	/*
	 * Install the property on the GObjectClass.
	 */
	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                gom_property_set_length(meta->properties),
	                                g_param_spec_object(property_name,
	                                                    property_nick,
	                                                    property_desc,
	                                                    GOM_TYPE_COLLECTION,
	                                                    G_PARAM_READABLE));
}

/**
 * gom_resource_class_has_a:
 * @resource_class: (in): A #GomResourceClass.
 * @property_name: (in): The property name.
 * @property_nick: (in): The property nick.
 * @property_desc: (in): The property description.
 * @resource_type: (in): The #GType of the related type.
 *
 * Adds a new property in @resource_class for the related resource of type
 * @resource_type. @property_name will be used as the name of the property.
 *
 * A one-to-one mapping can be achieved by marking the particular field
 * as unique.
 *
 * [[
 * gom_resource_class_has_a(my_class, "child", "child", "child", FOO_TYPE_BAR,
 *                          "unique", TRUE,
 *                          NULL);
 * ]]
 *
 * Returns: None.
 * Side effects: A new property is installed on @resource_class.
 */
void
gom_resource_class_has_a (GomResourceClass *resource_class,
                          const gchar      *property_name,
                          const gchar      *property_nick,
                          const gchar      *property_desc,
                          GType             resource_type,
                          ...)
{
	GomResourceClassMeta *meta;
	GomProperty *property;
	const gchar *option;
	va_list args;
	GValue value = { 0 };
	GType this_type;
	gchar *errstr = NULL;

	g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
	g_return_if_fail(property_name != NULL);
	g_return_if_fail(property_nick != NULL);
	g_return_if_fail(property_desc != NULL);
	g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
	g_return_if_fail(resource_type != GOM_TYPE_RESOURCE);

	this_type = G_TYPE_FROM_CLASS(resource_class);
	meta = gom_resource_class_get_meta(resource_class);

	/*
	 * Create a new property for the relation.
	 */
	property = g_new0(GomProperty, 1);
	property->name = g_quark_from_string(property_name);
	property->owner_type = this_type;
	property->value_type = resource_type;
	g_value_init(&property->default_value, resource_type);
	property->relationship.relation = GOM_RELATION_MANY_TO_ONE;

	va_start(args, resource_type);

	/*
	 * Handle additional va_arg options.
	 */
	while ((option = va_arg(args, const gchar *))) {
		if (!g_strcmp0(option, "unique")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errstr);
			if (errstr) {
				g_error("%s", errstr); /* Fatal */
				g_assert_not_reached();
				g_free(errstr);
				return;
			}
			if (g_value_get_boolean(&value)) {
				property->relationship.relation = GOM_RELATION_ONE_TO_ONE;
				property->is_unique = TRUE;
			}
			g_value_unset(&value);
		} else {
			g_error("Unknown has_a option %s", option); /* Fatal */
			g_assert_not_reached();
			break;
		}
	}

	va_end(args);

	/*
	 * Add property to the meta data.
	 */
	gom_property_set_add(meta->properties, property);

	/*
	 * Install the resource property on the GObjectClass.
	 */
	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                gom_property_set_length(meta->properties),
	                                g_param_spec_object(property_name,
	                                                    property_nick,
	                                                    property_desc,
	                                                    resource_type,
	                                                    G_PARAM_READWRITE));
}

/**
 * gom_resource_class_belongs_to:
 * @resource_class: (in): A #GomResourceClass.
 * @property_name: (in): The property name.
 * @property_nick: (in): The property nick.
 * @property_desc: (in): The property description.
 * @resource_type: (in): The #GType of the related type.
 *
 * Registers a "belongs-to" relationship.
 *
 * Adds a new property in @resource_class for the related resource of type
 * @resource_type. @property_name will be used as the name of the property.
 *
 * A one-to-one mapping can be achieved by marking the particular field
 * as unique.
 *
 * [[
 * gom_resource_class_belongs_to(my_class, "child", "child", "child",
 *                               FOO_TYPE_BAR,
 *                               "unique", TRUE,
 *                               NULL);
 * ]]
 *
 * Returns: None.
 * Side effects: A new property is installed on @resource_class.
 */
void
gom_resource_class_belongs_to (GomResourceClass *resource_class,
                               const gchar      *property_name,
                               const gchar      *property_nick,
                               const gchar      *property_desc,
                               GType             resource_type,
                               ...)
{
	GomResourceClassMeta *meta;
	GomProperty *property;
	const gchar *option;
	va_list args;
	GValue value = { 0 };
	GType this_type;
	gchar *errstr = NULL;

	g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
	g_return_if_fail(property_name != NULL);
	g_return_if_fail(property_nick != NULL);
	g_return_if_fail(property_desc != NULL);
	g_return_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE));
	g_return_if_fail(resource_type != GOM_TYPE_RESOURCE);

	this_type = G_TYPE_FROM_CLASS(resource_class);
	meta = gom_resource_class_get_meta(resource_class);

	property = g_new0(GomProperty, 1);
	property->name = g_quark_from_string(property_name);
	property->owner_type = this_type;
	property->value_type = resource_type;
	g_value_init(&property->default_value, resource_type);
	property->relationship.relation = GOM_RELATION_MANY_TO_ONE;

	va_start(args, resource_type);

	while ((option = va_arg(args, const gchar *))) {
		if (!g_strcmp0(option, "unique")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errstr);
			if (errstr) {
				g_error("%s", errstr); /* Fatal */
				g_assert_not_reached();
				g_free(errstr);
				return;
			}
			if (g_value_get_boolean(&value)) {
				property->relationship.relation = GOM_RELATION_ONE_TO_ONE;
				property->is_unique = TRUE;
			}
			g_value_unset(&value);
		} else {
			g_error("Unknown belongs_to option %s", option); /* Fatal */
			g_assert_not_reached();
			break;
		}
	}

	va_end(args);

	gom_property_set_add(meta->properties, property);

	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                gom_property_set_length(meta->properties),
	                                g_param_spec_object(property_name,
	                                                    property_nick,
	                                                    property_desc,
	                                                    resource_type,
	                                                    G_PARAM_READWRITE));
}

/**
 * gom_resource_class_init:
 * @resource_class: (in): A #GomResourceClass.
 *
 * Initializes the #GomResourceClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_resource_class_init (GomResourceClass *resource_class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(resource_class);
	object_class->finalize = gom_resource_finalize;
	object_class->get_property = gom_resource_real_get_property;
	object_class->set_property = gom_resource_real_set_property;
	g_type_class_add_private(object_class, sizeof(GomResourcePrivate));

	gParamSpecs[PROP_ADAPTER] =
		g_param_spec_object("adapter",
		                    _("Adapter"),
		                    _("The resources adapter."),
		                    GOM_TYPE_ADAPTER,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_ADAPTER,
	                                gParamSpecs[PROP_ADAPTER]);

	gMetaByType = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, g_free);
}

/**
 * gom_resource_class_install_property:
 * @resource_class: (in): A #GomResourceClass.
 * @param_spec: (in): A #GParamSpec.
 *
 * Adds a new property to the #GomResourceClass. The property is installed
 * and registered wth the resouce.
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_resource_class_install_property (GomResourceClass *resource_class,
                                     GParamSpec       *param_spec,
                                     ...)
{
	GomResourceClassMeta *meta;
	GomProperty *property;
	const gchar *option;
	va_list args;
	GValue value = { 0 };
	GType this_type;
	gchar *errmsg = NULL;

	g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
	g_return_if_fail(G_IS_PARAM_SPEC(param_spec));

	this_type = G_TYPE_FROM_CLASS(resource_class);
	meta = gom_resource_class_get_meta(resource_class);

	property = g_new0(GomProperty, 1);
	property->name = g_quark_from_string(param_spec->name);
	property->owner_type = this_type;
	property->value_type = param_spec->value_type;
	g_value_init(&property->default_value, property->value_type);

	va_start(args, param_spec);

	while ((option = va_arg(args, const gchar *))) {
		if (!g_strcmp0(option, "default")) {
			G_VALUE_COLLECT(&property->default_value, args, 0, &errmsg);
		} else if (!g_strcmp0(option, "unique")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errmsg);
			property->is_unique = g_value_get_boolean(&value);
			g_value_unset(&value);
		} else if (!g_strcmp0(option, "key")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errmsg);
			property->is_key = g_value_get_boolean(&value);
			g_value_unset(&value);
		} else if (!g_strcmp0(option, "serial")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errmsg);
			property->is_serial = g_value_get_boolean(&value);
			g_value_unset(&value);
		} else if (!g_strcmp0(option, "eager")) {
			G_VALUE_COLLECT_INIT(&value, G_TYPE_BOOLEAN, args, 0, &errmsg);
			property->is_eager = g_value_get_boolean(&value);
			g_value_unset(&value);
		} else {
			g_error("Invalid property option %s", option);
			g_assert_not_reached();
			break;
		}
		if (errmsg) {
			g_error("%s", errmsg);
			g_assert_not_reached();
			g_free(errmsg);
			break;
		}
	}

	va_end(args);

	gom_property_set_add(meta->properties, property);

	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                gom_property_set_length(meta->properties),
	                                param_spec);
}

/**
 * gom_resource_class_table:
 * @resource_class: (in): A #GomResourceClass.
 * @table: (in): The name of the table to store items.
 *
 * Sets the name of the table in which to store instances of the
 * #GomResource based class. This may mean different things based on the
 * particular adapter used.
 *
 * Returns: None.
 * Side effects: None.
 */
void
gom_resource_class_table (GomResourceClass *resource_class,
                          const gchar      *table)
{
	GomResourceClassMeta *meta;

	g_return_if_fail(table != NULL);
	g_return_if_fail(g_utf8_validate(table, -1, NULL));

	meta = gom_resource_class_get_meta(resource_class);
	g_free(meta->table);
	meta->table = g_strdup(table);
}

gpointer
gom_resource_create (GType        resource_type,
                     GomAdapter  *adapter,
                     const gchar *first_property,
                     ...)
{
	GObjectClass *klass;
	const gchar *property;
	GParamSpec *pspec;
	GParameter param = { 0 };
	gpointer ret = NULL;
	va_list args;
	GArray *params;
	gchar *errstr = NULL;
	gint i;

	if (!first_property) {
		return g_object_new(resource_type, "adapter", adapter, NULL);
	}

	klass = g_type_class_ref(resource_type);
	params = g_array_new(FALSE, FALSE, sizeof(GParameter));

	param.name = "adapter";
	g_value_init(&param.value, GOM_TYPE_ADAPTER);
	g_value_set_object(&param.value, adapter);
	g_array_append_val(params, param);

	va_start(args, first_property);

	property = first_property;

	do {
		pspec = g_object_class_find_property(klass, property);
		g_assert(pspec);
		memset(&param, 0, sizeof param);
		param.name = property;
		G_VALUE_COLLECT_INIT(&param.value, pspec->value_type, args, 0, &errstr);
		if (errstr) {
			g_error("Failed to read property %s value", param.name);
			g_free(errstr);
			g_assert_not_reached();
		}
		g_array_append_val(params, param);
	} while ((property = va_arg(args, const gchar *)));

	va_end(args);

	ret = g_object_newv(resource_type, params->len,
	                    (GParameter *)params->data);

	for (i = 0; i < params->len; i++) {
		param = g_array_index(params, GParameter, i);
		g_value_unset(&param.value);
	}

	g_array_free(params, TRUE);

	if (ret) {
		GOM_RESOURCE(ret)->priv->is_new = TRUE;
	}

	return ret;
}

/**
 * gom_resource_finalize:
 * @object: (in): A #GomResource.
 *
 * Finalizer for a #GomResource instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_resource_finalize (GObject *object)
{
	GomResourcePrivate *priv = GOM_RESOURCE(object)->priv;

	gom_clear_object(&priv->adapter);
	gom_clear_pointer(&priv->properties, g_hash_table_unref);

	G_OBJECT_CLASS(gom_resource_parent_class)->finalize(object);
}

/**
 * gom_resource_init:
 * @resource: (in): A #GomResource.
 *
 * Initializes the newly created #GomResource instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_resource_init (GomResource *resource)
{
	resource->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(resource,
		                            GOM_TYPE_RESOURCE,
		                            GomResourcePrivate);

	resource->priv->properties =
		g_hash_table_new_full(g_int_hash, g_int_equal,
		                      NULL, _property_value_free);
}

/**
 * gom_resource_is_new:
 * @resource: (in): A #GomResource.
 *
 * Checks if the resource is new; meaning it has never been saved to, or
 * loaded from, the adapter.
 *
 * Returns: %TRUE if the resource is new; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_resource_is_new (GomResource *resource)
{
	GomResourcePrivate *priv;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = resource->priv;

	return priv->is_new;
}

/**
 * gom_resource_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
void
gom_resource_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
	GomResourceClassMeta *meta;
	GomResourcePrivate *priv;
	GomPropertyValue *prop_value;
	GomProperty *prop;
	GomResource *resource = (GomResource *)object;
	GQuark name;

	g_return_if_fail(GOM_IS_RESOURCE(resource));

	priv = resource->priv;

	name = g_quark_from_string(pspec->name);
	if ((prop_value = g_hash_table_lookup(priv->properties, &name))) {
		g_value_copy(&prop_value->value, value);
		return;
	}

	meta = gom_resource_class_get_meta(GOM_RESOURCE_GET_CLASS(object));
	if ((prop = gom_property_set_findq(meta->properties, name))) {
		g_value_copy(&prop->default_value, value);
	}
}

/**
 * gom_resource_real_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_resource_real_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	GomResource *resource = GOM_RESOURCE(object);

	switch (prop_id) {
	case PROP_ADAPTER:
		g_value_set_object(value, resource->priv->adapter);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

GomPropertyValue**
gom_resource_get_properties (GomResource *resource,
                             guint       *n_values)
{
	GomResourcePrivate *priv;
	GomPropertyValue **values;
	GomPropertyValue *value;
	GHashTableIter iter;
	gint i = 0;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), NULL);
	g_return_val_if_fail(n_values != NULL, NULL);

	priv = resource->priv;

	*n_values = g_hash_table_size(priv->properties);
	values = g_malloc_n(*n_values, sizeof(GomPropertyValue*));

	g_hash_table_iter_init(&iter, priv->properties);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&value)) {
		g_assert_cmpint(i, <, *n_values);
		values[i++] = value;
	}

	return values;
}

/**
 * gom_resource_real_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_resource_real_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
	GomResource *resource = GOM_RESOURCE(object);

	switch (prop_id) {
	case PROP_ADAPTER:
		resource->priv->adapter = g_value_dup_object(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_resource_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
void
gom_resource_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
	GomResourcePrivate *priv;
	GomPropertyValue *prop_value;
	GomResource *resource = (GomResource *)object;
	GQuark name;

	g_return_if_fail(GOM_IS_RESOURCE(resource));

	priv = resource->priv;

	name = g_quark_from_string(pspec->name);

	if (!(prop_value = g_hash_table_lookup(priv->properties, &name))) {
		prop_value = _property_value_new();
		prop_value->name = name;
		g_value_init(&prop_value->value, G_VALUE_TYPE(value));
		g_hash_table_insert(priv->properties, &prop_value->name, prop_value);
	}

	prop_value->is_dirty = TRUE;

	g_value_copy(value, &prop_value->value);
}

static gboolean
gom_resource_save_self (GomResource  *resource,
                        GError      **error)
{
	GomResourcePrivate *priv;
	GomEnumerable *enumerable;
	gboolean ret = FALSE;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = resource->priv;

	if (!priv->adapter) {
		g_set_error(error, GOM_RESOURCE_ERROR,
		            GOM_RESOURCE_ERROR_NO_ADAPTER,
		            "The resource is not attached to an adapter.");
		return FALSE;
	}

	if (priv->is_new) {
		enumerable = gom_enumerable_array_new(&resource, 1);
		if ((ret = gom_adapter_create(priv->adapter, enumerable, error))) {
			priv->is_new = FALSE;
		}
		g_object_unref(enumerable);
	} else {
		ret = TRUE;
	}

	return ret;
}

static gboolean
gom_resource_save_parents (GomResource  *resource,
                           GError      **error)
{
	GomResourceClassMeta *meta;
	GomProperty *prop;
	GomResource *related = NULL;
	guint n_props;
	gint i;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	meta = gom_resource_class_get_meta(GOM_RESOURCE_GET_CLASS(resource));
	n_props = gom_property_set_length(meta->properties);

	for (i = 0; i < n_props; i++) {
		prop = gom_property_set_get_nth(meta->properties, i);
		if (prop->relationship.relation == GOM_RELATION_MANY_TO_ONE) {
			g_object_get(resource,
			             g_quark_to_string(prop->name), &related,
			             NULL);
			if (related) {
				if (!gom_resource_save_self(related, error)) {
					return FALSE;
				}
			}
			gom_clear_object(&related);
		}
	}

	return TRUE;
}

static gboolean
gom_resource_save_children (GomResource  *resource,
                            GError      **error)
{
	return TRUE;
}

gboolean
gom_resource_save (GomResource  *resource,
                   GError      **error)
{
	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	if (gom_resource_save_parents(resource, error)) {
		if (gom_resource_save_self(resource, error)) {
			if (gom_resource_save_children(resource, error)) {
				return TRUE;
			}
		}
	}

	return FALSE;
}
