/* gom/gom-enumerable-sqlite.c
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
#include <sqlite3.h>

#include "gom-enumerable-sqlite.h"
#include "gom-util.h"

G_DEFINE_TYPE(GomEnumerableSqlite, gom_enumerable_sqlite, GOM_TYPE_ENUMERABLE)

/*
 * Structures and enums.
 */

struct _GomEnumerableSqlitePrivate
{
	sqlite3_stmt *stmt;
};

enum
{
	PROP_0,
	PROP_STATEMENT,
	LAST_PROP
};

/*
 * Globals.
 */

static GParamSpec *gParamSpecs[LAST_PROP];

static gboolean
gom_enumerable_sqlite_iter_init (GomEnumerable     *enumerable,
                                 GomEnumerableIter *iter)
{
	GomEnumerableSqlitePrivate *priv;
	GomEnumerableSqlite *sqlite = (GomEnumerableSqlite *)enumerable;

	g_return_val_if_fail(GOM_IS_ENUMERABLE_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);

	priv = sqlite->priv;

	return (SQLITE_ROW == sqlite3_step(priv->stmt));
}

static gboolean
gom_enumerable_sqlite_iter_next (GomEnumerable     *enumerable,
                                 GomEnumerableIter *iter)
{
	GomEnumerableSqlitePrivate *priv;
	GomEnumerableSqlite *sqlite = (GomEnumerableSqlite *)enumerable;

	g_return_val_if_fail(GOM_IS_ENUMERABLE_SQLITE(sqlite), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);

	priv = sqlite->priv;

	return (SQLITE_ROW == sqlite3_step(priv->stmt));
}

static guint
gom_enumerable_sqlite_get_n_columns (GomEnumerable *enumerable)
{
	GomEnumerableSqlitePrivate *priv;
	GomEnumerableSqlite *sqlite = (GomEnumerableSqlite *)enumerable;

	g_return_val_if_fail(GOM_IS_ENUMERABLE_SQLITE(sqlite), 0);

	priv = sqlite->priv;

	return sqlite3_column_count(priv->stmt);
}

static void
gom_enumerable_sqlite_get_value (GomEnumerable     *enumerable,
                                 GomEnumerableIter *iter,
                                 gint               column,
                                 GValue            *value)
{
	GomEnumerableSqlitePrivate *priv;
	GomEnumerableSqlite *sqlite = (GomEnumerableSqlite *)enumerable;
	GValue v = { 0 };

	g_return_if_fail(GOM_IS_ENUMERABLE_SQLITE(sqlite));
	g_return_if_fail(iter != NULL);
	g_return_if_fail(iter->enumerable == enumerable);
	g_return_if_fail(column >= 0);
	g_return_if_fail(column < sqlite3_column_count(sqlite->priv->stmt));
	g_return_if_fail(value != NULL);

	priv = sqlite->priv;

	switch (sqlite3_column_type(priv->stmt, column)) {
	case SQLITE_INTEGER:
		g_value_init(&v, G_TYPE_INT64);
		g_value_set_int64(&v, sqlite3_column_int64(priv->stmt, column));
		break;
	case SQLITE_TEXT:
		g_value_init(&v, G_TYPE_STRING);
		g_value_set_string(&v, (const gchar *)sqlite3_column_text(priv->stmt, column));
		break;
	case SQLITE_FLOAT:
		g_value_init(&v, G_TYPE_DOUBLE);
		g_value_set_double(&v, sqlite3_column_double(priv->stmt, column));
		break;
	case SQLITE_NULL:
		g_critical("I don't know how to handle SQLITE_NULL yet");
		break;
	default:
		break;
	}

	if (G_VALUE_TYPE(&v) != G_VALUE_TYPE(value)) {
		if (g_value_type_transformable(G_VALUE_TYPE(&v), G_VALUE_TYPE(value))) {
			g_value_transform(&v, value);
		} else {
			g_critical("Cannot transform column %s of type %s to %s",
			           sqlite3_column_name(priv->stmt, column),
			           g_type_name(G_VALUE_TYPE(&v)),
			           g_type_name(G_VALUE_TYPE(value)));
		}
	} else {
		g_value_copy(&v, value);
	}

	g_value_unset(&v);
}

/**
 * gom_enumerable_sqlite_finalize:
 * @object: (in): A #GomEnumerableSqlite.
 *
 * Finalizer for a #GomEnumerableSqlite instance.  Frees any resources held by
 * the instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_sqlite_finalize (GObject *object)
{
	GomEnumerableSqlitePrivate *priv = GOM_ENUMERABLE_SQLITE(object)->priv;

	gom_clear_pointer(&priv->stmt, sqlite3_finalize);

	G_OBJECT_CLASS(gom_enumerable_sqlite_parent_class)->finalize(object);
}

/**
 * gom_enumerable_sqlite_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (out): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Get a given #GObject property.
 */
static void
gom_enumerable_sqlite_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GomEnumerableSqlite *sqlite = GOM_ENUMERABLE_SQLITE(object);

	switch (prop_id) {
	case PROP_STATEMENT:
		g_value_set_pointer(value, sqlite->priv->stmt);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_enumerable_sqlite_set_property:
 * @object: (in): A #GObject.
 * @prop_id: (in): The property identifier.
 * @value: (in): The given property.
 * @pspec: (in): A #ParamSpec.
 *
 * Set a given #GObject property.
 */
static void
gom_enumerable_sqlite_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GomEnumerableSqlite *sqlite = GOM_ENUMERABLE_SQLITE(object);

	switch (prop_id) {
	case PROP_STATEMENT:
		sqlite->priv->stmt = g_value_get_pointer(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

/**
 * gom_enumerable_sqlite_class_init:
 * @klass: (in): A #GomEnumerableSqliteClass.
 *
 * Initializes the #GomEnumerableSqliteClass and prepares the vtable.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_sqlite_class_init (GomEnumerableSqliteClass *klass)
{
	GObjectClass *object_class;
	GomEnumerableClass *enum_class;

	object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = gom_enumerable_sqlite_finalize;
	object_class->get_property = gom_enumerable_sqlite_get_property;
	object_class->set_property = gom_enumerable_sqlite_set_property;
	g_type_class_add_private(object_class, sizeof(GomEnumerableSqlitePrivate));

	enum_class = GOM_ENUMERABLE_CLASS(klass);
	enum_class->iter_init = gom_enumerable_sqlite_iter_init;
	enum_class->iter_next = gom_enumerable_sqlite_iter_next;
	enum_class->get_n_columns = gom_enumerable_sqlite_get_n_columns;
	enum_class->get_value = gom_enumerable_sqlite_get_value;

	gParamSpecs[PROP_STATEMENT] =
		g_param_spec_pointer("statement",
		                     _("Statement"),
		                     _("The enumerables sqlite statement."),
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(object_class, PROP_STATEMENT,
	                                gParamSpecs[PROP_STATEMENT]);
}

/**
 * gom_enumerable_sqlite_init:
 * @sqlite: (in): A #GomEnumerableSqlite.
 *
 * Initializes the newly created #GomEnumerableSqlite instance.
 *
 * Returns: None.
 * Side effects: None.
 */
static void
gom_enumerable_sqlite_init (GomEnumerableSqlite *sqlite)
{
	sqlite->priv =
		G_TYPE_INSTANCE_GET_PRIVATE(sqlite,
		                            GOM_TYPE_ENUMERABLE_SQLITE,
		                            GomEnumerableSqlitePrivate);
}
