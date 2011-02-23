/* adapters/sqlite/gom-adapter-sqlite.c
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

#include <sqlite3.h>

#include "gom-adapter-sqlite.h"
#include "gom-condition.h"
#include "gom-enumerable-sqlite.h"
#include "gom-resource.h"
#include "gom-util.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "GomSqlite"

G_DEFINE_TYPE(GomAdapterSqlite, gom_adapter_sqlite, GOM_TYPE_ADAPTER)

/*
 * Structures and enums.
 */

struct _GomAdapterSqlitePrivate
{
	sqlite3    *sqlite;
	GHashTable *created_tables;
};

enum
{
	OPENED,
	CLOSED,
	LAST_SIGNAL
};

/*
 * Forward declarations.
 */

static gboolean gom_adapter_sqlite_create          (GomAdapter        *adapter,
                                                    GomEnumerable     *enumerable,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_create_read     (GomAdapterSqlite  *sqlite,
                                                    GomQuery          *query,
                                                    sqlite3_stmt     **stmt,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_create_resource (GomAdapterSqlite  *sqlite,
                                                    GomResource       *resource,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_delete          (GomAdapter        *adapter,
                                                    GomCollection     *collection,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_ensure_table    (GomAdapterSqlite  *sqlite,
                                                    GomResource       *resource,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_execute_sql     (GomAdapterSqlite  *sqlite,
                                                    const gchar       *sql,
                                                    GError           **error);
static void     gom_adapter_sqlite_finalize        (GObject           *object);
static gboolean gom_adapter_sqlite_read            (GomAdapter        *adapter,
                                                    GomQuery          *query,
                                                    GomEnumerable    **enumerable,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_update          (GomAdapter        *adapter,
                                                    GomPropertySet    *properties,
                                                    GValueArray       *values,
                                                    GomCollection     *collection,
                                                    GError           **error);

/*
 * Globals.
 */

static guint    gSignals[LAST_SIGNAL];
static gboolean gLogSql;

static const gchar *
_get_table_name (GType type)
{
	GomResourceClass *resource_class;
	const gchar *table;

	resource_class = g_type_class_ref(type);
	table = resource_class->table;
	g_type_class_unref(resource_class);

	return table;
}

static GValue *
_g_value_dup (const GValue *value)
{
	GValue *dup_value;

	dup_value = g_new0(GValue, 1);
	g_value_init(dup_value, G_VALUE_TYPE(value));
	g_value_copy(value, dup_value);

	return dup_value;
}

static void
_g_value_free (gpointer data)
{
	GValue *value = (GValue *)data;

	if (value) {
		if (G_IS_VALUE(value)) {
			g_value_unset(value);
		}
		g_free(value);
	}
}

static void
_resource_mark_clean (GomResource *resource)
{
	GomPropertyValue **values;
	guint n_values = 0;
	gint i;

	values = gom_resource_get_properties(resource, &n_values);
	for (i = 0; i < n_values; i++) {
		values[i]->is_dirty = FALSE;
	}
	g_free(values);
}

/**
 * gtype_to_sqltype:
 * @type: (in): A #GType.
 *
 * Retrieves the name of the SQLite type that should be used to store
 * instances of @type.
 *
 * Returns: A string which should not be modified or freed.
 * Side effects: None.
 */
static const gchar *
gtype_to_sqltype (GType type)
{
	switch (type) {
	case G_TYPE_INT:
	case G_TYPE_UINT:
	case G_TYPE_INT64:
	case G_TYPE_UINT64:
	case G_TYPE_BOOLEAN:
		return "INTEGER";
	case G_TYPE_STRING:
		return "TEXT";
	case G_TYPE_DOUBLE:
	case G_TYPE_FLOAT:
		return "FLOAT";
	default:
		if (type == G_TYPE_DATE_TIME) {
			return "INTEGER";
		} else if (g_type_is_a(type, G_TYPE_ENUM)) {
			return "INTEGER";
		}
		g_assert_not_reached();
		return "TEXT";
	}
}

static void
gom_adapter_sqlite_append_condition (GomAdapterSqlite *sqlite,
                                     GomCondition     *condition,
                                     GHashTable       *hash,
                                     GString          *str)
{
	const gchar *table;
	const gchar *field;
	GValue *value;
	gchar *key;

	g_string_append(str, "(");

	if (gom_condition_is_a(condition, GOM_CONDITION_EQUAL)) {
		field = g_quark_to_string(condition->u.equality.property->name);
		table = _get_table_name(condition->u.equality.property->owner_type);
		key = g_strdup_printf("%s_%s", table, field);
		value = _g_value_dup(&condition->u.equality.value);
		g_string_append_printf(str, " '%s'.'%s' IS :%s ", table, field, key);
		g_hash_table_insert(hash, key, value);
	} else if (gom_condition_is_a(condition, GOM_CONDITION_AND)) {
		g_assert_not_reached(); /* TODO */
	} else if (gom_condition_is_a(condition, GOM_CONDITION_OR)) {
		g_assert_not_reached(); /* TODO */
	} else {
		g_assert_not_reached();
	}

	g_string_append(str, ") ");
}

void
gom_adapter_sqlite_begin (GomAdapterSqlite *sqlite)
{
	static const gchar sql[] = "BEGIN;";
	gom_adapter_sqlite_execute_sql(sqlite, sql, NULL);
}

/**
 * gom_adapter_sqlite_class_init:
 * @klass: (in): A #GomAdapterSqliteClass.
 *
 * Initializes the #GomAdapterSqliteClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_adapter_sqlite_class_init (GomAdapterSqliteClass *klass)
{
	GObjectClass *object_class;
	GomAdapterClass *adapter_class;

	adapter_class = GOM_ADAPTER_CLASS(klass);
	adapter_class->create = gom_adapter_sqlite_create;
	adapter_class->read = gom_adapter_sqlite_read;
	adapter_class->update = gom_adapter_sqlite_update;
	adapter_class->delete = gom_adapter_sqlite_delete;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_adapter_sqlite_finalize;
	g_type_class_add_private(object_class, sizeof(GomAdapterSqlitePrivate));

	gSignals[OPENED] = g_signal_new("opened",
	                                GOM_TYPE_ADAPTER_SQLITE,
	                                G_SIGNAL_RUN_FIRST,
	                                0,
	                                NULL,
	                                NULL,
	                                g_cclosure_marshal_VOID__VOID,
	                                G_TYPE_NONE,
	                                0);

	gSignals[CLOSED] = g_signal_new("closed",
	                                GOM_TYPE_ADAPTER_SQLITE,
	                                G_SIGNAL_RUN_FIRST,
	                                0,
	                                NULL,
	                                NULL,
	                                g_cclosure_marshal_VOID__VOID,
	                                G_TYPE_NONE,
	                                0);

	gLogSql = !!g_getenv("GOM_ADAPTER_SQLITE_DEBUG");
}

gboolean
gom_adapter_sqlite_commit (GomAdapterSqlite  *sqlite,
                           GError           **error)
{
	static const gchar sql[] = "COMMIT;";
	return gom_adapter_sqlite_execute_sql(sqlite, sql, error);
}

/**
 * gom_adapter_sqlite_close:
 * @sqlite: (in): A #GomAdapterSqlite.
 *
 * Close the underlying sqlite storage for #GomAdapterSqlite.
 *
 * Returns: None.
 * Side effects: sqlite db is closed.
 */
void
gom_adapter_sqlite_close (GomAdapterSqlite *sqlite)
{
	GomAdapterSqlitePrivate *priv;

	g_return_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite));

	priv = sqlite->priv;

	gom_clear_pointer(&priv->sqlite, sqlite3_close);

	g_signal_emit(sqlite, gSignals[CLOSED], 0);
}

/**
 * gom_adapter_sqlite_create:
 * @adapter: (in): A #GomAdapter.
 * @enumerable: (in): A #GomEnumerable.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * Creates the enumerable of resources in the sqlite database. @enumerable is
 * expected to have a single column and contain GomResources.
 *
 * Returns: None.
 * Side effects: None.
 */
static gboolean
gom_adapter_sqlite_create (GomAdapter     *adapter,
                           GomEnumerable  *enumerable,
                           GError        **error)
{
	GomAdapterSqlite *sqlite = (GomAdapterSqlite *)adapter;
	GomAdapterSqlitePrivate *priv;
	GomEnumerableIter iter;
	GValue value = { 0 };

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(GOM_IS_ENUMERABLE(enumerable), FALSE);
	g_return_val_if_fail(gom_enumerable_get_n_columns(enumerable) == 1, FALSE);

	priv = sqlite->priv;

	if (!priv->sqlite) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_NOT_OPEN,
		            "Must open a database before creating a resource.");
		return FALSE;
	}

	if (gom_enumerable_iter_init(&iter, enumerable)) {
		do {
			gom_enumerable_get_value(enumerable, &iter, 0, &value);
			if (!g_type_is_a(value.g_type, GOM_TYPE_RESOURCE)) {
				g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
				            GOM_ADAPTER_SQLITE_ERROR_INVALID_TYPE,
				            "The type %s is not a GomResource.",
				            g_type_name(value.g_type));
				g_value_unset(&value);
				return FALSE;
			}
			if (!gom_adapter_sqlite_ensure_table(sqlite,
			                                     g_value_get_object(&value),
			                                     error)) {
				g_value_unset(&value);
				return FALSE;
			}
			if (!gom_adapter_sqlite_create_resource(sqlite,
			                                        g_value_get_object(&value),
			                                        error)) {
				g_value_unset(&value);
				return FALSE;
			}
			_resource_mark_clean(g_value_get_object(&value));
			g_value_unset(&value);
		} while (gom_enumerable_iter_next(&iter));
	}

	return TRUE;
}

static void
_bind_parameter (sqlite3_stmt *stmt,
                 const gchar  *name,
                 const GValue *value)
{
	gchar *param_name;
	gint column;

	if (G_UNLIKELY(gLogSql)) {
		gchar *msg = g_strdup_printf("%s => %s",
		                             name, g_type_name(value->g_type));

		switch (value->g_type) {
		case G_TYPE_STRING:
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%s]",
			      msg, g_value_get_string(value));
			break;
		case G_TYPE_INT:
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%d]",
			      msg, g_value_get_int(value));
			break;
		case G_TYPE_INT64:
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%"G_GINT64_FORMAT"]",
			      msg, g_value_get_int64(value));
			break;
		default:
			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", msg);
			break;
		}

		g_free(msg);
	}

	/*
	 * Get the index in the query for the named paramter.
	 */
	param_name = g_strdup_printf(":%s", name);
	column = sqlite3_bind_parameter_index(stmt, param_name);
	g_free(param_name);
	if (!column) {
		g_critical("Column \"%s\" not needed in query.", name);
		return;
	}

	switch (value->g_type) {
	case G_TYPE_INT:
	case G_TYPE_UINT:
	case G_TYPE_BOOLEAN:
		sqlite3_bind_int(stmt, column, value->data[0].v_int);
		break;
	case G_TYPE_INT64:
	case G_TYPE_UINT64:
		sqlite3_bind_int64(stmt, column, value->data[0].v_int64);
		break;
	case G_TYPE_FLOAT:
		sqlite3_bind_double(stmt, column, g_value_get_float(value));
		break;
	case G_TYPE_DOUBLE:
		sqlite3_bind_double(stmt, column, g_value_get_double(value));
		break;
	case G_TYPE_STRING:
		sqlite3_bind_text(stmt, column, g_value_dup_string(value), -1, g_free);
		break;
	default:
		if (value->g_type == G_TYPE_DATE_TIME) {
			if (g_value_get_boxed(value)) {
				gint64 utc;
				utc = g_date_time_to_unix(g_value_get_boxed(value));
				sqlite3_bind_int(stmt, column, utc);
			}
		} else if (g_type_is_a(value->g_type, G_TYPE_ENUM)) {
			sqlite3_bind_int(stmt, column, value->data[0].v_int);
		}
	}
}

static GHashTable*
resource_to_hash (GomResource *resource)
{
	GomResourceClass *related_class;
	GomPropertyValue **values;
	GomProperty *prop;
	GomResource *related;
	GHashTable *hash;
	GValue *value;
	guint n_values;
	gchar *key;
	gint i;
	gint j;

	g_return_val_if_fail(GOM_IS_RESOURCE(resource), NULL);

	values = gom_resource_get_properties(resource, &n_values);
	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
	                             g_free, _g_value_free);

	for (i = 0; i < n_values; i++) {
		if (g_type_is_a(values[i]->value.g_type, GOM_TYPE_RESOURCE)) {
			related_class = g_type_class_peek(values[i]->value.g_type);

			related = g_value_get_object(&values[i]->value);
			if (!related) {
				continue;
			}

			for (j = 0; j < related_class->properties->len; j++) {
				prop = gom_property_set_get_nth(related_class->properties, j);
				if (prop->is_key) {
					key = g_strdup_printf("%s_%s",
					                      g_quark_to_string(values[i]->name),
					                      g_quark_to_string(prop->name));
					value = g_new0(GValue, 1);
					g_value_init(value, prop->value_type);
					g_object_get_property(values[i]->value.data[0].v_pointer,
					                      g_quark_to_string(prop->name),
					                      value);
					g_hash_table_insert(hash, key, value);
				}
			}
		} else if (G_VALUE_HOLDS(&values[i]->value, GOM_TYPE_COLLECTION)) {
			/*
			 * TODO: 
			 */
		} else {
			key = g_strdup(g_quark_to_string(values[i]->name));
			value = g_new0(GValue, 1);
			g_value_init(value, values[i]->value.g_type);
			g_value_copy(&values[i]->value, value);
			g_hash_table_insert(hash, key, value);
		}
	}

	g_free(values);

	return hash;
}

static gboolean
gom_adapter_sqlite_create_read (GomAdapterSqlite  *sqlite,
                                GomQuery          *query,
                                sqlite3_stmt     **stmt,
                                GError           **error)
{
	GomAdapterSqlitePrivate *priv;
	GomResourceClass *resource_class = NULL;
	GomResourceClass *join_class = NULL;
	GomPropertySet *fields = NULL;
	GHashTableIter iter;
	GomCondition *condition = NULL;
	GomProperty *field;
	GomProperty *join = NULL;
	const gchar *table = NULL;
	const gchar *k;
	GHashTable *hash = NULL;
	gboolean count_only = FALSE;
	gboolean ret = FALSE;
	gboolean reverse = FALSE;
	GString *str = NULL;
	guint64 limit = 0;
	guint64 offset = 0;
	GValue *v;
	GType resource_type = 0;
	guint n_fields = 0;
	gint i;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(GOM_IS_QUERY(query), FALSE);
	g_return_val_if_fail(stmt != NULL, FALSE);
	g_return_val_if_fail(*stmt == NULL, FALSE);

	priv = sqlite->priv;

	g_object_get(query,
	             "count-only", &count_only,
	             "condition", &condition,
	             "fields", &fields,
	             "join", &join,
	             "limit", &limit,
	             "offset", &offset,
	             "resource-type", &resource_type,
	             "reverse", &reverse,
	             NULL);

	if (!resource_type) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_INVALID_TYPE,
		            "No resource-type was provided for the query.");
		goto failure;
	}

	resource_class = g_type_class_ref(resource_type);

	str = g_string_new("SELECT ");

	if (count_only) {
		g_string_append(str, "COUNT(*)");
	} else {
		n_fields = gom_property_set_length(fields);
		for (i = 0; i < n_fields; i++) {
			field = gom_property_set_get_nth(fields, i);
			table = _get_table_name(field->owner_type);
			g_string_append_printf(str, "'%s'.'%s', ", table,
			                       g_quark_to_string(field->name));
		}
		g_string_truncate(str, str->len - 2);
	}

	table = _get_table_name(resource_type);
	g_string_append_printf(str, " FROM %s ", table);

	if (join) {
		join_class = g_type_class_ref(join->owner_type);
		g_string_append_printf(str, "LEFT JOIN %s ON ", join_class->table);
		n_fields = gom_property_set_length(resource_class->keys);
		for (i = 0; i < n_fields; i++) {
			field = gom_property_set_get_nth(resource_class->keys, i);
			g_string_append_printf(str, "'%s'.'%s_%s' = '%s'.'%s' ",
			                       join_class->table,
			                       g_quark_to_string(join->name),
			                       g_quark_to_string(field->name),
			                       resource_class->table,
			                       g_quark_to_string(field->name));
		}
	}

	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
								 g_free, _g_value_free);

	if (condition) {
		g_string_append(str, " WHERE ");
		gom_adapter_sqlite_append_condition(sqlite, condition, hash, str);
	}

	if (reverse) {
		/*
		 * TODO: This should probably do something different to reverse
		 *       the result set.
		 */
		g_string_append(str, "ORDER BY ROWID DESC ");
	}

	if (!count_only) {
		if (limit) {
			g_string_append_printf(str, "LIMIT %"G_GUINT64_FORMAT" ", limit);
		}
		if (offset) {
			g_string_append_printf(str, "OFFSET %"G_GUINT64_FORMAT" ", offset);
		}
	}

	g_string_append(str, ";");

	if (gLogSql) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", str->str);
	}

	if (!!sqlite3_prepare_v2(priv->sqlite, str->str, -1, stmt, NULL)) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "SQLite failed to compile SQL: %s",
		            sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, (gpointer *)&v)) {
		_bind_parameter(*stmt, k, v);
	}

	ret = TRUE;

  failure:
	g_string_free(str, TRUE);
	gom_clear_pointer(&fields, gom_property_set_unref);
	gom_clear_pointer(&hash, g_hash_table_destroy);
	gom_clear_pointer(&condition, gom_condition_unref);
	gom_clear_pointer(&join_class, g_type_class_unref);
	gom_clear_pointer(&resource_class, g_type_class_unref);

	return ret;
}

static gboolean
gom_adapter_sqlite_create_resource (GomAdapterSqlite  *sqlite,
                                    GomResource       *resource,
                                    GError           **error)
{
	GomAdapterSqlitePrivate *priv;
	GomResourceClass *resource_class;
	GHashTableIter iter;
	sqlite3_stmt *stmt = NULL;
	const gchar *table_name;
	GomProperty *prop;
	GHashTable *hash;
	gboolean ret = FALSE;
	GString *str;
	guint64 insert_id;
	GValue *v;
	GValue value = { 0 };
	GType resource_type;
	gchar *k;
	gint code;
	gint i;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = sqlite->priv;

	resource_class = GOM_RESOURCE_GET_CLASS(resource);
	resource_type = G_TYPE_FROM_CLASS(resource_class);
	g_assert(resource_class);

	str = g_string_new("INSERT INTO ");
	table_name = _get_table_name(resource_type);
	hash = resource_to_hash(resource);

	g_assert(table_name);
	g_assert(hash);

	g_string_append_printf(str, "%s (", table_name);

	/*
	 * Build the list of columns to update in the query.
	 */
	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, NULL)) {
		g_string_append_printf(str, "'%s', ", k);
	}
	g_string_truncate(str, str->len - 2);

	g_string_append(str, ") VALUES (");

	/*
	 * Build the list of named parameters in the query.
	 */
	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, NULL)) {
		gchar *d = g_strdelimit(g_strdup(k), "-", '_');
		g_string_append_printf(str, ":%s, ", d);
		g_free(d);
	}
	g_string_truncate(str, str->len - 2);

	g_string_append(str, ");");

	if (gLogSql) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", str->str);
	}

	/*
	 * Prepare the sql query in sqlite.
	 */
	if (!!sqlite3_prepare_v2(priv->sqlite, str->str, -1, &stmt, NULL)) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "Failed to insert row: %s",
		            sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	/*
	 * Bind the values for each named parameter.
	 */
	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, (gpointer*)&v)) {
		gchar *d = g_strdelimit(g_strdup(k), "-", '_');
		_bind_parameter(stmt, d, v);
		g_free(d);
	}

	/*
	 * Execute the sql query in sqlite.
	 */
	if (!(ret = (SQLITE_DONE == (code = sqlite3_step(stmt))))) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "Failed to insert row: %s",
		            sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	/*
	 * If one of the keys was a serial (primary key for example), lets get the
	 * insert id and set the property.
	 *
	 * TODO: We need to figure out how we want to mark the properties as clean.
	 *       Those that are not eager, should also be dropped.
	 */
	for (i = 0; i < resource_class->properties->len; i++) {
		prop = gom_property_set_get_nth(resource_class->properties, i);
		if (prop->is_key) {
			if (prop->is_serial) {
				insert_id = sqlite3_last_insert_rowid(priv->sqlite);
				g_value_init(&value, G_TYPE_UINT64);
				g_value_set_uint64(&value, insert_id);
				g_object_set_property(G_OBJECT(resource),
				                      g_quark_to_string(prop->name),
				                      &value);
				g_value_unset(&value);
				break;
			}
		}
	}

  failure:
	gom_clear_pointer(&hash, g_hash_table_destroy);
	gom_clear_pointer(&stmt, sqlite3_finalize);
	g_string_free(str, TRUE);

	return ret;
}

gboolean
gom_adapter_sqlite_create_table (GomAdapterSqlite  *sqlite,
                                 GType              resource_type,
                                 GError           **error)
{
	GomResourceClass *relation_class;
	GomResourceClass *resource_class;
	GomPropertySet *relation_set;
	GomProperty *property;
	GomProperty *related_prop;
	const gchar *table_name;
	gboolean ret = FALSE;
	GString *str;
	gint i;
	gint j;

	resource_class = g_type_class_ref(resource_type);
	str = g_string_new("CREATE TABLE IF NOT EXISTS ");

	table_name = _get_table_name(resource_type);
	g_string_append_printf(str, "%s (", table_name);

	for (i = 0; i < resource_class->properties->len; i++) {
		property = gom_property_set_get_nth(resource_class->properties, i);

		if (g_type_is_a(property->value_type, GOM_TYPE_RESOURCE)) {
			if (property->relationship.relation == GOM_RELATION_ONE_TO_ONE ||
			    property->relationship.relation == GOM_RELATION_MANY_TO_ONE) {
				relation_class = g_type_class_ref(property->value_type);
				relation_set = gom_resource_class_get_properties(relation_class);
				for (j = 0; j < relation_set->len; j++) {
					related_prop = gom_property_set_get_nth(relation_set, j);
					if (related_prop->is_key) {
						g_string_append_printf(str, "'%s_%s' %s, ",
											   g_quark_to_string(property->name),
											   g_quark_to_string(related_prop->name),
											   gtype_to_sqltype(related_prop->value_type));
					}
				}
				if (i + 1 >= resource_class->properties->len) {
					g_string_truncate(str, str->len - 2);
				}
				g_type_class_unref(relation_class);
			} else {
				/*
				 * TODO: Handle colletions.
				 */
			}
		} else {
			g_string_append_printf(str, "'%s' %s",
			                       g_quark_to_string(property->name),
			                       gtype_to_sqltype(property->value_type));
			if (property->is_key) {
				g_string_append_printf(str, " PRIMARY KEY");
			} else {
				if (property->is_unique) {
					g_string_append_printf(str, " UNIQUE");
				}
			}
			if (i + 1 < resource_class->properties->len) {
				g_string_append(str, ", ");
			}
		}
	}

	g_string_append(str, ");");

	ret = gom_adapter_sqlite_execute_sql(sqlite, str->str, error);

	g_type_class_unref(resource_class);
	g_string_free(str, TRUE);

	return ret;
}

static gboolean
gom_adapter_sqlite_delete (GomAdapter     *adapter,
                           GomCollection  *collection,
                           GError        **error)
{
	GomAdapterSqlitePrivate *priv;
	GomAdapterSqlite *sqlite = (GomAdapterSqlite *)adapter;
	GHashTableIter iter;
	GomCondition *condition = NULL;
	sqlite3_stmt *stmt = NULL;
	const gchar *table;
	GHashTable *hash = NULL;
	GomQuery *query = NULL;
	gboolean ret = FALSE;
	GString *str = NULL;
	GValue *v;
	gchar *k;
	GType resource_type = 0;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);

	priv = sqlite->priv;

	if (!priv->sqlite) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_NOT_OPEN,
		            "Must open sqlite before calling %s()",
		            G_STRFUNC);
		return FALSE;
	}

	str = g_string_new("DELETE FROM ");
	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
	                             g_free, _g_value_free);

	g_object_get(collection,
	             "query", &query,
	             NULL);

	if (query) {
		g_object_get(query,
		             "resource-type", &resource_type,
		             "condition", &condition,
		             NULL);
		table = _get_table_name(resource_type);
		g_string_append_printf(str, "%s WHERE ", table);
		gom_adapter_sqlite_append_condition(sqlite, condition, hash, str);
		gom_clear_pointer(&condition, gom_condition_unref);
	} else {
		/*
		 * TODO: Iterate the collection and remove each item individually
		 */
	}

	if (gLogSql) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", str->str);
	}

	if (!!sqlite3_prepare_v2(priv->sqlite, str->str, -1, &stmt, NULL)) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "%s", sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, (gpointer*)&v)) {
		_bind_parameter(stmt, k, v);
	}

	if (!(ret = (SQLITE_DONE == sqlite3_step(stmt)))) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "%s", sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	ret = TRUE;

  failure:
	gom_clear_object(&query);
	gom_clear_pointer(&hash, g_hash_table_destroy);
	gom_clear_pointer(&stmt, sqlite3_finalize);
	g_string_free(str, TRUE);

	return ret;
}

static gboolean
gom_adapter_sqlite_ensure_table (GomAdapterSqlite  *sqlite,
                                 GomResource       *resource,
                                 GError           **error)
{
	GomAdapterSqlitePrivate *priv;
	GType resource_type;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = sqlite->priv;

	if (!priv->sqlite) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_NOT_OPEN,
		            "Must open a database before creating tables.");
		return FALSE;
	}

	resource_type = G_TYPE_FROM_INSTANCE(resource);

	if (g_hash_table_lookup(priv->created_tables,
	                        GINT_TO_POINTER(resource_type))) {
		return TRUE;
	}

	if (!gom_adapter_sqlite_create_table(sqlite, resource_type, error)) {
		return FALSE;
	}

	g_hash_table_insert(priv->created_tables, GINT_TO_POINTER(resource_type),
	                    GINT_TO_POINTER(TRUE));

	return TRUE;
}

/**
 * gom_adapter_sqlite_error_quark:
 *
 * Retrieves the quark for the error domain.
 *
 * Returns: A #GQuark.
 * Side effects: The quark will be registered on first call.
 */
GQuark
gom_adapter_sqlite_error_quark (void)
{
	return g_quark_from_static_string("gom_adapter_sqlite_error_quark");
}

/**
 * gom_adapter_sqlite_execute_sql:
 * @sqlite: (in): A #GomAdapterSqlite.
 * @sql: (in): Sql to execute.
 * @error: (error): A location for a #GError, or %NULL.
 *
 * Executes a SQL query on the underlying SQLite database.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 * Side effects: None.
 */
static gboolean
gom_adapter_sqlite_execute_sql (GomAdapterSqlite  *sqlite,
                                const gchar       *sql,
                                GError           **error)
{
	GomAdapterSqlitePrivate *priv;
	sqlite3_stmt *stmt;
	gboolean ret = FALSE;
	gint code;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(sql != NULL, FALSE);
	g_return_val_if_fail(g_utf8_validate(sql, -1, NULL), FALSE);

	priv = sqlite->priv;

	if (gLogSql) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", sql);
	}

	if (!!sqlite3_prepare_v2(priv->sqlite, sql, -1, &stmt, NULL)) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "SQLite: %s", sqlite3_errmsg(priv->sqlite));
		return FALSE;
	}

	if (!(ret = (SQLITE_DONE == (code = sqlite3_step(stmt))))) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "SQLite(%d): %s",
		            code, sqlite3_errmsg(priv->sqlite));
	}

	sqlite3_finalize(stmt);

	return ret;
}

/**
 * gom_adapter_sqlite_finalize:
 * @object: (in): A #GomAdapterSqlite.
 *
 * Finalizer for a #GomAdapterSqlite instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_adapter_sqlite_finalize (GObject *object)
{
	GomAdapterSqlitePrivate *priv = GOM_ADAPTER_SQLITE(object)->priv;

	gom_clear_pointer(&priv->sqlite, sqlite3_close);

	G_OBJECT_CLASS(gom_adapter_sqlite_parent_class)->finalize(object);
}

/**
 * gom_adapter_sqlite_init:
 * @sqlite: (in): A #GomAdapterSqlite.
 *
 * Initializes the newly created #GomAdapterSqlite instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_adapter_sqlite_init (GomAdapterSqlite *sqlite)
{
	sqlite->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(sqlite,
		                            GOM_TYPE_ADAPTER_SQLITE,
		                            GomAdapterSqlitePrivate);

	sqlite->priv->created_tables = g_hash_table_new(g_int_hash, g_int_equal);
}

/**
 * gom_adapter_sqlite_load_from_file:
 * @sqlite: (in): A #GomAdapterSqlite.
 *
 * Loads a sqlite database from the file specified by @filename. If the file
 * does not exist, it will be created.
 *
 * Returns: None.
 * Side effects: None.
 */
gboolean
gom_adapter_sqlite_load_from_file (GomAdapterSqlite  *sqlite,
                                   const gchar       *filename,
                                   GError           **error)
{
	GomAdapterSqlitePrivate *priv;
	gint flags;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(filename != NULL, FALSE);
	g_return_val_if_fail(g_utf8_validate(filename, -1, NULL), FALSE);

	priv = sqlite->priv;

	if (priv->sqlite) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_ALREADY_OPEN,
		            "A sqlite database has already been loaded.");
		return FALSE;
	}

	flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

	if (!!sqlite3_open_v2(filename, &priv->sqlite, flags, NULL)) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_OPEN_FAILED,
		            "Failed to open the sqlite database.");
		return FALSE;
	}

	g_signal_emit(sqlite, gSignals[OPENED], 0);

	return TRUE;
}

static gboolean
gom_adapter_sqlite_read (GomAdapter     *adapter,
                         GomQuery       *query,
                         GomEnumerable **enumerable,
                         GError        **error)
{
	GomAdapterSqlitePrivate *priv;
	GomAdapterSqlite *sqlite = (GomAdapterSqlite *)adapter;
	GomPropertySet *fields = NULL;
	GomProperty *field;
	sqlite3_stmt *stmt = NULL;
	gboolean ret = FALSE;
	gint i;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(GOM_IS_QUERY(query), FALSE);
	g_return_val_if_fail(enumerable != NULL, FALSE);

	priv = sqlite->priv;

	if (!priv->sqlite) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_NOT_OPEN,
		            "Must open a database before reading.");
		goto failure;
	}

	g_object_get(query,
	             "fields", &fields,
	             NULL);

	if (!fields) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_INVALID_TYPE,
		            "No field specified in query.");
		goto failure;
	}

	for (i = 0; i < fields->len; i++) {
		field = gom_property_set_get_nth(fields, i);
		if (g_type_is_a(field->value_type, GOM_TYPE_RESOURCE)) {
			g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
			            GOM_ADAPTER_SQLITE_ERROR_INVALID_TYPE,
			            "Attempt to retrieve a GomResource field.");
			goto failure;
		} else if (g_type_is_a(field->value_type, GOM_TYPE_COLLECTION)) {
			g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
			            GOM_ADAPTER_SQLITE_ERROR_INVALID_TYPE,
			            "Attempt to retrieve a GomCollection field.");
			goto failure;
		}
	}

	if (!gom_adapter_sqlite_create_read(sqlite, query, &stmt, error)) {
		goto failure;
	}

	*enumerable = g_object_new(GOM_TYPE_ENUMERABLE_SQLITE,
	                           "statement", stmt,
	                           NULL);

	ret = TRUE;

  failure:
	gom_clear_pointer(&fields, gom_property_set_unref);

	return ret;
}

static gboolean
gom_adapter_sqlite_update (GomAdapter      *adapter,
                           GomPropertySet  *properties,
                           GValueArray     *values,
                           GomCollection   *collection,
                           GError         **error)
{
	GomAdapterSqlitePrivate *priv;
	GomAdapterSqlite *sqlite = (GomAdapterSqlite *)adapter;
	GHashTableIter iter;
	GomCondition *condition = NULL;
	sqlite3_stmt *stmt = NULL;
	GomProperty *prop;
	const gchar *k = NULL;
	const gchar *table;
	GHashTable *hash = NULL;
	GomQuery *query = NULL;
	gboolean ret = FALSE;
	GString *str = NULL;
	GValue *v = NULL;
	GType resource_type = 0;
	guint n_props;
	gint i;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(properties != NULL, FALSE);
	g_return_val_if_fail(values != NULL, FALSE);
	g_return_val_if_fail(values->n_values ==
	                     gom_property_set_length(properties),
	                     FALSE);
	g_return_val_if_fail(GOM_IS_COLLECTION(collection), FALSE);

	priv = sqlite->priv;

	if (!priv->sqlite) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_NOT_OPEN,
		            "Must open a database before updating a resource.");
		return FALSE;
	}

	g_object_get(collection,
	             "query", &query,
	             NULL);

	if (!query) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "The collection does not contain a query.");
		goto failure;
	}

	g_object_get(query,
	             "condition", &condition,
	             "resource-type", &resource_type,
	             NULL);

	if (!condition) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "The query does not contain a condition.");
		goto failure;
	}

	if (!resource_type) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "The query does not contain a resource type.");
		goto failure;
	}

	n_props = gom_property_set_length(properties);
	table = _get_table_name(resource_type);
	hash = g_hash_table_new_full(g_str_hash, g_str_equal,
	                             g_free, _g_value_free);
	str = g_string_new("UPDATE ");
	g_string_append_printf(str, "%s SET ", table);

	for (i = 0; i < n_props; i++) {
		gchar *d;
		prop = gom_property_set_get_nth(properties, i);
		d = g_strdelimit(g_strdup(g_quark_to_string(prop->name)), "-", '_');
		g_string_append_printf(str, "'%s' = :%s, ",
		                       g_quark_to_string(prop->name), d);
		v = g_value_array_get_nth(values, i);
		v = _g_value_dup(v);
		g_hash_table_insert(hash, g_strdup(g_quark_to_string(prop->name)), v);
		g_free(d);
	}

	g_string_truncate(str, str->len - 2);

	g_string_append(str, " WHERE ");

	gom_adapter_sqlite_append_condition(sqlite, condition, hash, str);

	g_string_append(str, ";");

	if (gLogSql) {
		g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", str->str);
	}

	if (!!sqlite3_prepare_v2(priv->sqlite, str->str, -1, &stmt, NULL)) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "%s", sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, (gpointer *)&v)) {
		gchar *d = g_strdelimit(g_strdup(k), "-", '_');
		_bind_parameter(stmt, d, v);
		g_free(d);
	}

	if (!(ret = (SQLITE_DONE == sqlite3_step(stmt)))) {
		g_set_error(error, GOM_ADAPTER_SQLITE_ERROR,
		            GOM_ADAPTER_SQLITE_ERROR_SQLITE,
		            "%s", sqlite3_errmsg(priv->sqlite));
		goto failure;
	}

	ret = TRUE;

failure:
	if (str) {
		g_string_free(str, TRUE);
	}
	gom_clear_pointer(&stmt, sqlite3_finalize);
	gom_clear_pointer(&condition, gom_condition_unref);
	gom_clear_pointer(&hash, g_hash_table_unref);
	gom_clear_object(&query);

	return ret;
}
