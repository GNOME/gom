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
#include "gom-resource.h"
#include "gom-util.h"

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
static gboolean gom_adapter_sqlite_ensure_table    (GomAdapterSqlite  *sqlite,
                                                    GomResource       *resource,
                                                    GError           **error);
static gboolean gom_adapter_sqlite_execute_sql     (GomAdapterSqlite  *sqlite,
                                                    const gchar       *sql,
                                                    GError           **error);
static void     gom_adapter_sqlite_finalize        (GObject           *object);
static gboolean gom_adapter_sqlite_create_resource (GomAdapterSqlite  *sqlite,
                                                    GomResource       *resource,
                                                    GError           **error);

/*
 * Globals.
 */

static guint    gSignals[LAST_SIGNAL];
static gboolean gLogSql;

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
		sqlite3_bind_double(stmt, column, value->data[0].v_float);
		break;
	case G_TYPE_DOUBLE:
		sqlite3_bind_double(stmt, column, value->data[0].v_double);
		break;
	case G_TYPE_STRING:
		sqlite3_bind_text(stmt, column, value->data[0].v_pointer, -1, NULL);
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
	GomResourceClassMeta *related_meta;
	GomResourceClass *related_class;
	GomPropertyValue **values;
	GomProperty *prop;
	GHashTable *hash;
	GValue *value;
	guint n_values;
	guint n_props;
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
			related_meta = gom_resource_class_get_meta(related_class);
			n_props = gom_property_set_length(related_meta->properties);

			for (j = 0; j < n_props; j++) {
				prop = gom_property_set_get_nth(related_meta->properties, j);
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
gom_adapter_sqlite_create_resource (GomAdapterSqlite  *sqlite,
                                    GomResource       *resource,
                                    GError           **error)
{
	GomAdapterSqlitePrivate *priv;
	GomResourceClassMeta *meta;
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
	guint n_props;
	gint code;
	gint i;

	g_return_val_if_fail(GOM_IS_ADAPTER_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(GOM_IS_RESOURCE(resource), FALSE);

	priv = sqlite->priv;

	resource_class = GOM_RESOURCE_GET_CLASS(resource);
	resource_type = G_TYPE_FROM_CLASS(resource_class);
	meta = gom_resource_class_get_meta(resource_class);

	g_assert(resource_class);
	g_assert(meta);

	str = g_string_new("INSERT INTO ");
	table_name = meta->table ? meta->table : g_type_name(resource_type);
	hash = resource_to_hash(resource);

	g_assert(table_name);
	g_assert(hash);

	g_string_append_printf(str, "%s (", table_name);

	/*
	 * Build the list of columns to update in the query.
	 */
	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, NULL)) {
		g_string_append_printf(str, "%s, ", k);
	}
	g_string_truncate(str, str->len - 2);

	g_string_append(str, ") VALUES (");

	/*
	 * Build the list of named parameters in the query.
	 */
	g_hash_table_iter_init(&iter, hash);
	while (g_hash_table_iter_next(&iter, (gpointer *)&k, NULL)) {
		g_string_append_printf(str, ":%s, ", k);
	}
	g_string_truncate(str, str->len - 2);

	g_string_append(str, ");");

	if (gLogSql) {
		g_log("Gom", G_LOG_LEVEL_DEBUG, "%s", str->str);
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
		_bind_parameter(stmt, k, v);
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
	n_props = gom_property_set_length(meta->properties);
	for (i = 0; i < n_props; i++) {
		prop = gom_property_set_get_nth(meta->properties, i);
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
	GomResourceClassMeta *meta;
	GomResourceClass *relation_class;
	GomResourceClass *resource_class;
	GomPropertySet *relation_set;
	GomProperty *property;
	GomProperty *related_prop;
	const gchar *table_name;
	gboolean ret = FALSE;
	GString *str;
	guint n_props = 0;
	guint n_related_props = 0;
	gint i;
	gint j;

	resource_class = g_type_class_ref(resource_type);
	meta = gom_resource_class_get_meta(resource_class);
	str = g_string_new("CREATE TABLE IF NOT EXISTS ");

	table_name = meta->table ? meta->table : g_type_name(resource_type);
	g_string_append_printf(str, "%s (", table_name);

	n_props = gom_property_set_length(meta->properties);
	for (i = 0; i < n_props; i++) {
		property = gom_property_set_get_nth(meta->properties, i);

		if (g_type_is_a(property->value_type, GOM_TYPE_RESOURCE)) {
			if (property->relationship.relation == GOM_RELATION_ONE_TO_ONE ||
			    property->relationship.relation == GOM_RELATION_MANY_TO_ONE) {
				relation_class = g_type_class_ref(property->value_type);
				relation_set = gom_resource_class_get_properties(relation_class);
				n_related_props = gom_property_set_length(relation_set);
				for (j = 0; j < n_related_props; j++) {
					related_prop = gom_property_set_get_nth(relation_set, j);
					if (related_prop->is_key) {
						g_string_append_printf(str, "'%s_%s' %s, ",
											   g_quark_to_string(property->name),
											   g_quark_to_string(related_prop->name),
											   gtype_to_sqltype(related_prop->value_type));
					}
				}
				if (i + 1 >= n_props) {
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
			if (i + 1 < n_props) {
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
		g_log("Gom", G_LOG_LEVEL_DEBUG, "%s", sql);
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
