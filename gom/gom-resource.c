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
	PROP_IS_NEW,
	LAST_PROP
};

/*
 * Forward declarations.
 */

static void              _date_time_to_uint64           (const GValue  *src_value,
                                                         GValue        *dst_value);
static void              _date_time_to_int64            (const GValue  *src_value,
                                                         GValue        *dst_value);
static void              _uint64_to_date_time           (const GValue  *src_value,
                                                         GValue        *dst_value);
static void              _int64_to_date_time            (const GValue  *src_value,
                                                         GValue        *dst_value);
static void              _property_value_free           (gpointer       data);
static GomPropertyValue* _property_value_new            (void);
static void              gom_resource_finalize          (GObject       *object);
static void              gom_resource_read_property     (GomResource   *resource,
                                                         GQuark         property,
                                                         GValue        *value);
static void              gom_resource_real_get_property (GObject       *object,
                                                         guint          prop_id,
                                                         GValue        *value,
                                                         GParamSpec    *pspec);
static void              gom_resource_real_set_property (GObject       *object,
                                                         guint          prop_id,
                                                         const GValue  *value,
                                                         GParamSpec    *pspec);
static gboolean          gom_resource_save_children     (GomResource   *resource,
                                                         GError       **error);
static gboolean          gom_resource_save_parents      (GomResource   *resource,
                                                         GError       **error);
static gboolean          gom_resource_save_self         (GomResource   *resource,
                                                         GError       **error);

/*
 * Globals.
 */

static GParamSpec *gParamSpecs[LAST_PROP];
static gpointer    gom_resource_parent_class;

/**
 * _date_time_to_uint64:
 * @src_value: (in): A #GValue.
 * @dst_value: (in): A #GValue.
 *
 * Converts from a #GDateTime to a #guint64.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
_date_time_to_uint64 (const GValue *src_value,
                      GValue       *dst_value)
{
	GDateTime *dt = g_value_get_boxed(src_value);

	if (dt) {
		dst_value->data[0].v_uint64 = g_date_time_to_unix(dt);
	}
}

/**
 * _date_time_to_int64:
 * @src_value: (in): A #GValue.
 * @dst_value: (in): A #GValue.
 *
 * Converts from a #GDateTime to a #gint64.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
_date_time_to_int64 (const GValue *src_value,
                     GValue       *dst_value)
{
	GDateTime *dt = g_value_get_boxed(src_value);

	if (dt) {
		dst_value->data[0].v_uint64 = g_date_time_to_unix(dt);
	}
}

/**
 * _uint64_to_date_time:
 * @src_value: (in): A #GValue.
 * @dst_value: (in): A #GValue.
 *
 * Converts from a #guint64 to a #GDateTime.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
_uint64_to_date_time (const GValue *src_value,
                      GValue       *dst_value)
{
	GDateTime *dt;

	dt = g_date_time_new_from_unix_utc(src_value->data[0].v_uint64);
	g_value_take_boxed(dst_value, dt);
}

/**
 * _int64_to_date_time:
 * @src_value: (in): A #GValue.
 * @dst_value: (in): A #GValue.
 *
 * Converts from a #gint64 to a #GDateTime.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
_int64_to_date_time (const GValue *src_value,
                     GValue       *dst_value)
{
	GDateTime *dt;

	dt = g_date_time_new_from_unix_utc(src_value->data[0].v_uint64);
	g_value_take_boxed(dst_value, dt);
}

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
 * gom_resource_find:
 * @resource_type: (in): A #GomResource based #GType.
 * @adapter: (in): A #GomAdapter.
 * @condition: (in) (allow-none): A #GomCondition or %NULL.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * Locates a set of #GomResource<!-- -->'s based on @condition from @adapter.
 *
 * Returns: A #GomCollection which should be freed with g_object_unref().
 * Side effects: None.
 */
GomCollection*
gom_resource_find (GType          resource_type,
                   GomAdapter    *adapter,
                   GomCondition  *condition,
                   GError       **error)
{
	GomResourceClass *resource_class = NULL;
	GomPropertySet *fields = NULL;
	GomPropertySet *all = NULL;
	GomCollection *ret = NULL;
	GomProperty *prop;
	GomQuery *query = NULL;
	guint n_props;
	gint i;

	g_return_val_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE), NULL);
	g_return_val_if_fail(GOM_IS_ADAPTER(adapter), NULL);

	resource_class = g_type_class_ref(resource_type);
	all = gom_resource_class_get_properties(resource_class);
	n_props = gom_property_set_length(all);
	fields = gom_property_set_newv(0, NULL);

	for (i = 0; i < n_props; i++) {
		prop = gom_property_set_get_nth(all, i);
		if (prop->is_key || prop->is_eager) {
			gom_property_set_add(fields, prop);
		}
	}

	query = g_object_new(GOM_TYPE_QUERY,
	                     "fields", fields,
	                     "resource-type", resource_type,
	                     "condition", condition,
	                     NULL);

	ret = g_object_new(GOM_TYPE_COLLECTION,
	                   "adapter", adapter,
	                   "query", query,
	                   NULL);

	gom_clear_pointer(&resource_class, g_type_class_unref);
	gom_clear_pointer(&fields, gom_property_set_unref);
	gom_clear_object(&query);

	return ret;
}

/**
 * gom_resource_find_first:
 * @resource_type: (in): A #GomResource based #GType.
 * @adapter: (in): A #GomAdapter.
 * @condition: (in) (allow-none): A #GomCondition or %NULL.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * A convenience function that calls gom_resource_find() and returns the
 * first result.
 *
 * If no resource was found, %NULL is returned and @error is set.
 *
 * Returns: A #GomResource if successful; otherwise %NULL.
 * Side effects: None.
 */
gpointer
gom_resource_find_first (GType          resource_type,
                         GomAdapter    *adapter,
                         GomCondition  *condition,
                         GError       **error)
{
	GomCollection *collection = NULL;
	GomResource *ret = NULL;

	g_return_val_if_fail(g_type_is_a(resource_type, GOM_TYPE_RESOURCE), NULL);
	g_return_val_if_fail(adapter != NULL, NULL);

	if ((collection = gom_resource_find(resource_type, adapter, condition, error))) {
		if ((ret = gom_collection_first(collection))) {
			g_assert(GOM_IS_RESOURCE(ret));
		}
	}

	gom_clear_object(&collection);

	if (!ret) {
		g_set_error(error, GOM_RESOURCE_ERROR, GOM_RESOURCE_ERROR_NOT_FOUND,
		            "No resource was found matching the query.");
	}

	return ret;
}

/**
 * gom_resource_get_condition:
 * @resource: (in): A #GomResource.
 *
 * Retrieves a condition to uniquely identify this resource.
 *
 * Returns: A #GomCondition.
 * Side effects: None.
 */
static GomCondition*
gom_resource_get_condition (GomResource *resource)
{
	GomResourcePrivate *priv;
	GomResourceClass *resource_class;
	GomPropertyValue *prop_value;
	GomPropertySet *props;
	GomCondition *condition = NULL;
	GomProperty *prop;
	GPtrArray *all;
	guint n_props;
	gint i;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), NULL);

	/*
	 * XXX: The MockOccupation is not getting saved, but getting
	 *      requested from the MockPerson when it goes to save.
	 *      Not good.
	 */
	g_debug("Retrieving condition for type %s", g_type_name(G_TYPE_FROM_INSTANCE(resource)));

	priv = resource->priv;

	resource_class = GOM_RESOURCE_GET_CLASS(resource);
	props = gom_resource_class_get_properties(resource_class);
	all = g_ptr_array_new_with_free_func((GDestroyNotify)gom_condition_unref);
	n_props = gom_property_set_length(props);

	for (i = 0; i < n_props; i++) {
		prop = gom_property_set_get_nth(props, i);
		if (prop->is_key) {
			if ((prop_value = g_hash_table_lookup(priv->properties, &prop->name))) {
				g_assert(G_IS_VALUE(&prop_value->value));
				condition = gom_condition_equal(prop, &prop_value->value);
				g_ptr_array_add(all, condition);
			}
		}
	}

	condition = NULL;

	if (all->len > 1) {
		condition = g_ptr_array_index(all, 0);
		for (i = 1; i < all->len; i++) {
			condition = gom_condition_and(condition,
			                              g_ptr_array_index(all, i));
		}
	} else if (all->len == 1) {
		condition = gom_condition_ref(g_ptr_array_index(all, 0));
	} else {
		g_assert_not_reached();
	}

	gom_clear_pointer(&all, g_ptr_array_unref);

	return condition;
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
	g_return_val_if_fail(GOM_IS_RESOURCE_CLASS(resource_class), NULL);
	return resource_class->properties;
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
	 * Install the property on the GObjectClass.
	 */
	gom_property_set_add(resource_class->properties, property);
	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                resource_class->properties->len,
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
	 * Install the resource property on the GObjectClass.
	 */
	gom_property_set_add(resource_class->properties, property);
	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                resource_class->properties->len,
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

	gom_property_set_add(resource_class->properties, property);
	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                resource_class->properties->len,
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
gom_resource_class_init (gpointer klass,
                         gpointer data)
{
	GomResourceClass *resource_class;
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_resource_finalize;
	object_class->get_property = gom_resource_real_get_property;
	object_class->set_property = gom_resource_real_set_property;
	g_type_class_add_private(object_class, sizeof(GomResourcePrivate));

	resource_class = GOM_RESOURCE_CLASS(klass);
	gom_resource_parent_class = g_type_class_peek_parent(resource_class);

	gParamSpecs[PROP_ADAPTER] =
		g_param_spec_object("adapter",
		                    _("Adapter"),
		                    _("The resources adapter."),
		                    GOM_TYPE_ADAPTER,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_ADAPTER,
	                                gParamSpecs[PROP_ADAPTER]);

	gParamSpecs[PROP_IS_NEW] =
		g_param_spec_boolean("is-new",
		                     _("Is New"),
		                     _("If this is a newly created object not yet persisted."),
		                     FALSE,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_IS_NEW,
	                                gParamSpecs[PROP_IS_NEW]);

	g_value_register_transform_func(G_TYPE_DATE_TIME, G_TYPE_UINT64, _date_time_to_uint64);
	g_value_register_transform_func(G_TYPE_DATE_TIME, G_TYPE_INT64, _date_time_to_int64);
	g_value_register_transform_func(G_TYPE_UINT64, G_TYPE_DATE_TIME, _uint64_to_date_time);
	g_value_register_transform_func(G_TYPE_INT64, G_TYPE_DATE_TIME, _int64_to_date_time);
}

static void
gom_resource_base_init (gpointer klass)
{
	GomResourceClass *resource_class;

	resource_class = GOM_RESOURCE_CLASS(klass);
	resource_class->keys = gom_property_set_newv(0, NULL);
	resource_class->properties = gom_property_set_newv(0, NULL);
	resource_class->tableq =
		g_quark_from_string(g_type_name(G_TYPE_FROM_CLASS(resource_class)));
	resource_class->table = g_quark_to_string(resource_class->tableq);
}

static void
gom_resource_base_finalize (gpointer klass)
{
	GomResourceClass *resource_class;

	resource_class = GOM_RESOURCE_CLASS(klass);
	gom_clear_pointer(&resource_class->keys, gom_property_set_unref);
	gom_clear_pointer(&resource_class->properties, gom_property_set_unref);
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
	GomProperty *property;
	const gchar *option;
	va_list args;
	GValue value = { 0 };
	GType this_type;
	gchar *errmsg = NULL;

	g_return_if_fail(GOM_IS_RESOURCE_CLASS(resource_class));
	g_return_if_fail(G_IS_PARAM_SPEC(param_spec));

	this_type = G_TYPE_FROM_CLASS(resource_class);

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

	gom_property_set_add(resource_class->properties, property);

	if (property->is_key) {
		gom_property_set_add(resource_class->keys, property);
	}

	g_object_class_install_property(G_OBJECT_CLASS(resource_class),
	                                resource_class->properties->len,
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
	g_return_if_fail(table != NULL);
	g_return_if_fail(g_utf8_validate(table, -1, NULL));

	resource_class->tableq = g_quark_from_string(table);
	resource_class->table = g_quark_to_string(resource_class->tableq);
}

/**
 * gom_resource_create:
 * @resource_type: (in): A #GomResource based #GType.
 * @adapter: (in): A #GomAdapter.
 * @first_property: (in): The name of the first property to set.
 *
 * This acts similar to g_object_new(). It creates a new #GomResource
 * with the properties specified and marks it as a new resource so that
 * a new item is added to the adapter when gom_adapter_save() is called.
 *
 * Returns: A #GomResource which should be freed with g_object_unref().
 * Side effects: None.
 */
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
gom_resource_init (GTypeInstance *instance,
                   gpointer       g_class)
{
	GomResource *resource = (GomResource *)instance;

	resource->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(resource,
		                            GOM_TYPE_RESOURCE,
		                            GomResourcePrivate);

	resource->priv->properties =
		g_hash_table_new_full(g_int_hash, g_int_equal,
		                      NULL, _property_value_free);
}

/**
 * gom_resource_is_dirty:
 * @resource: (in): A #GomResource.
 *
 * Checks to see if a #GomResource has been dirtied.
 *
 * Returns: %TRUe if the resource is dirty; otherwise %FALSE.
 * Side effects: None.
 */
gboolean
gom_resource_is_dirty (GomResource *resource)
{
	GomResourcePrivate *priv;
	GHashTableIter iter;
	GomPropertyValue *prop_value;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = resource->priv;

	g_hash_table_iter_init(&iter, priv->properties);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&prop_value)) {
		if (prop_value->is_dirty) {
			return TRUE;
		}
	}

	return FALSE;
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
	GomResourcePrivate *priv;
	GomResourceClass *resource_class;
	GomPropertyValue *prop_value;
	GomProperty *prop;
	GomResource *resource = (GomResource *)object;
	GQuark name;

	g_return_if_fail(GOM_IS_RESOURCE(resource));

	priv = resource->priv;

	/*
	 * If we currently have the value in our hashtable, retrieve it.
	 */
	name = g_quark_from_string(pspec->name);
	if ((prop_value = g_hash_table_lookup(priv->properties, &name))) {
		g_value_copy(&prop_value->value, value);
		return;
	}

	/*
	 * If we were loaded from the database, retrieve the property value.
	 */
	if (!priv->is_new) {
		gom_resource_read_property(resource, name, value);
		return;
	}

	/*
	 * New item, lets get the default and store it.
	 */
	resource_class = GOM_RESOURCE_GET_CLASS(object);
	if ((prop = gom_property_set_findq(resource_class->properties, name))) {
		prop_value = _property_value_new();
		prop_value->name = name;
		g_value_init(&prop_value->value, prop->value_type);
		if (g_type_is_a(prop->value_type, GOM_TYPE_RESOURCE)) {
			g_value_take_object(&prop_value->value,
			                    g_object_new(prop->value_type,
			                                 "adapter", priv->adapter,
			                                 NULL));
		} else if (g_type_is_a(prop->value_type, GOM_TYPE_COLLECTION)) {
			/* TODO */
		} else {
			g_value_copy(&prop->default_value, &prop_value->value);
		}
		g_value_copy(&prop_value->value, value);
		g_hash_table_insert(priv->properties, &prop_value->name, prop_value);
		return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID(resource, prop_id, pspec);
}

static void
gom_resource_read_property (GomResource *resource,
                            GQuark       property,
                            GValue      *value)
{
	GomResourcePrivate *priv;
	GomEnumerableIter iter;
	GomPropertyValue *prop_value = NULL;
	GomResourceClass *resource_class;
	GomPropertySet *fields = NULL;
	GomEnumerable *enumerable = NULL;
	GomCondition *condition = NULL;
	GomProperty *prop;
	GomQuery *query = NULL;
	GError *error = NULL;
	GType resource_type;

	g_return_if_fail(GOM_IS_RESOURCE(resource));
	g_return_if_fail(property != 0);
	g_return_if_fail(G_VALUE_TYPE(value) != G_TYPE_INVALID);

	priv = resource->priv;

	resource_class = GOM_RESOURCE_GET_CLASS(resource);
	resource_type = G_TYPE_FROM_INSTANCE(resource);
	fields = gom_resource_class_get_properties(resource_class);
	prop = gom_property_set_findq(fields, property);
	fields = NULL;

	if (!prop) {
		g_critical("Unknown property %s", g_quark_to_string(property));
		return;
	}

	/*
	 * TODO: Implement relations.
	 */
	if (g_type_is_a(prop->value_type, GOM_TYPE_RESOURCE)) {
		//g_critical("Retrieving related resources not yet supported");
		return;
	} else if (g_type_is_a(prop->value_type, GOM_TYPE_COLLECTION)) {
		//g_critical("Retrieving collection properties not yet supported");
		return;
	}

	condition = gom_resource_get_condition(resource);
	fields = gom_property_set_newv(1, &prop);
	query = g_object_new(GOM_TYPE_QUERY,
	                     "condition", condition,
	                     "fields", fields,
	                     "limit", G_GUINT64_CONSTANT(1),
	                     "resource-type", resource_type,
	                     NULL);

	if (!gom_adapter_read(priv->adapter, query, &enumerable, &error)) {
		g_critical("%s", error->message);
		g_clear_error(&error);
		goto failure;
	}

	if (gom_enumerable_get_n_columns(enumerable) != 1) {
		g_critical("Received invalid number of columns for query.");
		return;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		gom_enumerable_get_value(enumerable, &iter, 0, value);
	}

	prop_value = _property_value_new();
	prop_value->name = property;
	g_value_init(&prop_value->value, G_VALUE_TYPE(value));
	g_value_copy(value, &prop_value->value);
	g_hash_table_insert(priv->properties, &prop_value->name, prop_value);

  failure:
	gom_clear_object(&query);
	gom_clear_object(&enumerable);
	gom_clear_pointer(&condition, gom_condition_unref);
	gom_clear_pointer(&fields, gom_property_set_unref);
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
	case PROP_IS_NEW:
		g_value_set_boolean(value, resource->priv->is_new);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_resource_get_properties:
 * @resource: (in): A #GomResource.
 * @n_values: (out): A location for the length of the resulting array.
 *
 * Retrieves a list of values for the currently loaded properties of
 * @resource.
 *
 * Returns: (array length=n_values) (transfer container): Array of #GomPropertyValue.
 * Side effects: None.
 */
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

	/*
	 * Allocate memory to store the resulting array.
	 */
	*n_values = g_hash_table_size(priv->properties);
	values = g_malloc_n(*n_values, sizeof(GomPropertyValue*));

	/*
	 * Iterate the hash table, populating the array.
	 */
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
	case PROP_IS_NEW:
		resource->priv->is_new = g_value_get_boolean(value);
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

	/*
	 * Retrieve the property container, or create it.
	 */
	if (!(prop_value = g_hash_table_lookup(priv->properties, &name))) {
		prop_value = _property_value_new();
		prop_value->name = name;
		g_value_init(&prop_value->value, G_VALUE_TYPE(value));
		g_hash_table_insert(priv->properties, &prop_value->name, prop_value);
	}

	/*
	 * Mark the property as dirty and store a private copy of the value.
	 */
	prop_value->is_dirty = TRUE;
	g_value_copy(value, &prop_value->value);
}

static gboolean
gom_resource_save_self (GomResource  *resource,
                        GError      **error)
{
	GomResourcePrivate *priv;
	GomPropertyValue *value;
	GomPropertySet *props;
	GomPropertySet *set = NULL;
	GomEnumerable *enumerable = NULL;
	GomCollection *collection = NULL;
	GomCondition *condition = NULL;
	GomProperty *prop;
	GValueArray *values = NULL;
	GomQuery *query = NULL;
	gboolean ret = FALSE;
	guint n_props;
	gint i;

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
	} else {
		props = GOM_RESOURCE_GET_CLASS(resource)->properties;
		n_props = gom_property_set_length(props);
		set = gom_property_set_dup(props);
		values = g_value_array_new(n_props);
		for (i = 0; i < n_props; i++) {
			prop = gom_property_set_get_nth(props, i);
			if ((value = g_hash_table_lookup(priv->properties, &prop->name))) {
				if (value->is_dirty) {
					g_value_array_append(values, &value->value);
					continue;
				}
			}
			gom_property_set_remove(set, prop);
		}
		condition = gom_resource_get_condition(resource);
		query = g_object_new(GOM_TYPE_QUERY,
		                     "resource-type", G_TYPE_FROM_INSTANCE(resource),
		                     "condition", condition,
		                     NULL);
		collection = g_object_new(GOM_TYPE_COLLECTION,
		                          "query", query,
		                          NULL);
		ret = gom_adapter_update(priv->adapter, set, values,
		                         collection, error);
	}

	gom_clear_object(&collection);
	gom_clear_object(&enumerable);
	gom_clear_object(&query);
	gom_clear_pointer(&condition, gom_condition_unref);
	gom_clear_pointer(&set, gom_property_set_unref);
	gom_clear_pointer(&values, g_value_array_free);

	return ret;
}

static gboolean
gom_resource_save_parents (GomResource  *resource,
                           GError      **error)
{
	GomResourcePrivate *priv;
	GomResourceClass *resource_class;
	GomPropertyValue *prop_value;
	GomResource *related = NULL;
	GomProperty *prop;
	gint i;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = resource->priv;

	resource_class = GOM_RESOURCE_GET_CLASS(resource);

	for (i = 0; i < resource_class->properties->len; i++) {
		prop = gom_property_set_get_nth(resource_class->properties, i);
		if (prop->relationship.relation == GOM_RELATION_MANY_TO_ONE) {
			prop_value = g_hash_table_lookup(priv->properties, &prop->name);
			if (prop_value) {
				related = g_value_get_object(&prop_value->value);
				if (gom_resource_is_dirty(related)) {
					if (!gom_resource_save_self(related, error)) {
						return FALSE;
					}
				}
			}
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

GType
gom_resource_get_type (void)
{
	static gsize initialized = FALSE;
	static GType type_id = 0;
	static GTypeInfo type_info = {
		.class_size    = sizeof(GomResourceClass),
		.base_init     = gom_resource_base_init,
		.base_finalize = gom_resource_base_finalize,
		.class_init    = gom_resource_class_init,
		.instance_size = sizeof(GomResource),
		.instance_init = gom_resource_init,
	};

	if (g_once_init_enter(&initialized)) {
		type_id = g_type_register_static(G_TYPE_OBJECT,
		                                 g_intern_static_string("GomResource"),
		                                 &type_info,
		                                 G_TYPE_FLAG_ABSTRACT);
		g_once_init_leave(&initialized, TRUE);
	}

	return type_id;
}
