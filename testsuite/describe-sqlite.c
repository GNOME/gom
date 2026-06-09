/* describe-sqlite.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <string.h>

#include <libgom.h>

#include "test-util.h"

static const char *describe_db_path;
static gboolean describe_dump;

static GomRegistry *
describe_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  return gom_registry_builder_build (builder);
}

static void
describe_append_csv_field (GString    *line,
                           const char *value)
{
  gboolean needs_quotes;

  if (value == NULL)
    return;

  needs_quotes = strpbrk (value, ",\"\n\r") != NULL;

  if (!needs_quotes)
    {
      g_string_append (line, value);
      return;
    }

  g_string_append_c (line, '"');
  for (const char *p = value; *p != '\0'; p++)
    {
      if (*p == '"')
        g_string_append (line, "\"\"");
      else
        g_string_append_c (line, *p);
    }
  g_string_append_c (line, '"');
}

static gboolean
describe_print_relation (GomRepository  *repository,
                         const char     *relation,
                         GError        **error)
{
  g_autoptr(GomRelationSchema) schema = NULL;
  g_autoptr(GListModel) fields = NULL;
  g_autoptr(GListModel) indexes = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  guint n_fields;
  guint n_indexes;
  guint n_rows = 0;

  schema = dex_await_object (gom_repository_describe_relation (repository, relation), error);
  if (schema == NULL)
    return FALSE;

  g_print ("Relation: %s\n", relation);

  fields = gom_relation_schema_list_fields (schema);
  indexes = gom_relation_schema_list_indexes (schema);

  n_fields = fields ? g_list_model_get_n_items (fields) : 0;
  n_indexes = indexes ? g_list_model_get_n_items (indexes) : 0;

  g_print ("  Fields:\n");
  if (n_fields == 0)
    {
      g_print ("    (none)\n");
    }
  else
    {
      for (guint i = 0; i < n_fields; i++)
        {
          g_autoptr(GomFieldSchema) field = g_list_model_get_item (fields, i);
          const char *name = gom_schema_get_name (GOM_SCHEMA (field));
          const char *sql_type = gom_field_schema_get_sql_type (field);
          const char *default_value = gom_field_schema_get_default_value (field);
          gboolean nonnull = gom_field_schema_get_nonnull (field);
          gboolean primary_key = gom_field_schema_get_primary_key (field);

          g_print ("    %s", name ? name : "-");
          g_print (" type=%s", sql_type ? sql_type : "-");
          if (nonnull)
            g_print (" not-null");
          if (primary_key)
            g_print (" primary-key");
          if (default_value != NULL)
            g_print (" default=%s", default_value);
          g_print ("\n");
        }
    }

  g_print ("  Indexes:\n");
  if (n_indexes == 0)
    {
      g_print ("    (none)\n");
    }
  else
    {
      for (guint i = 0; i < n_indexes; i++)
        {
          g_autoptr(GomIndexSchema) index = g_list_model_get_item (indexes, i);
          const char *name = gom_schema_get_name (GOM_SCHEMA (index));
          const char * const *index_fields = gom_index_schema_get_fields (index);
          gboolean unique = gom_index_schema_get_unique (index);
          g_autoptr(GString) fields_str = NULL;

          fields_str = g_string_new (NULL);
          if (index_fields != NULL)
            {
              for (guint j = 0; index_fields[j] != NULL; j++)
                {
                  if (j > 0)
                    g_string_append (fields_str, ", ");
                  g_string_append (fields_str, index_fields[j]);
                }
            }

          g_print ("    %s", name ? name : "-");
          if (unique)
            g_print (" unique");
          if (fields_str->len > 0)
            g_print (" fields=%s", fields_str->str);
          g_print ("\n");
        }
    }

  if (describe_dump)
    {
      g_print ("  Dump:\n");

      query_builder = gom_query_builder_new ();
      gom_query_builder_set_target_relation (query_builder, relation);
      query = gom_query_builder_build (query_builder, error);
      if (query == NULL)
        return FALSE;

      cursor = dex_await_object (gom_repository_query (repository, query), error);
      if (cursor == NULL)
        return FALSE;

      while (TRUE)
        {
          g_autoptr(GString) line = NULL;
          guint n_columns;
          gboolean has_row = dex_await_boolean (gom_cursor_next (cursor), error);

          if (error != NULL && *error != NULL)
            return FALSE;

          if (!has_row)
            break;

          n_columns = gom_cursor_get_n_columns (cursor);
          line = g_string_new (NULL);

          for (guint i = 0; i < n_columns; i++)
            {
              g_auto(GValue) value = G_VALUE_INIT;
              g_auto(GValue) str_value = G_VALUE_INIT;
              g_autofree char *fallback = NULL;
              const char *value_str = NULL;

              if (i > 0)
                g_string_append_c (line, ',');

              if (!gom_cursor_get_column (cursor, i, &value))
                return FALSE;

              if (G_VALUE_HOLDS_POINTER (&value) &&
                  g_value_get_pointer (&value) == NULL)
                value_str = NULL;
              else if (G_VALUE_HOLDS_STRING (&value))
                value_str = g_value_get_string (&value);
              else
                {
                  g_value_init (&str_value, G_TYPE_STRING);
                  if (g_value_transform (&value, &str_value))
                    value_str = g_value_get_string (&str_value);
                  else
                    {
                      fallback = g_strdup_value_contents (&value);
                      value_str = fallback;
                    }
                }

              describe_append_csv_field (line, value_str);
            }

          g_print ("    %s\n", line->str);
          n_rows++;
        }

      if (n_rows == 0)
        g_print ("    (none)\n");

      if (!dex_await (gom_cursor_close (cursor), error))
        return FALSE;
    }

  return TRUE;
}

static void
describe_run (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char **relations = NULL;

  db_uri = g_filename_to_uri (describe_db_path, NULL, &error);
  if (db_uri == NULL)
    {
      g_printerr ("Failed to convert path to URI: %s\n", error->message);
      return;
    }

  if (!(driver = gom_driver_open (db_uri, &error)))
    {
      g_printerr ("Failed to open database: %s\n", error->message);
      return;
    }

  registry = describe_create_registry ();

  if (!(repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver), registry, NULL), &error)))
    {
      g_printerr ("Failed to create repository: %s\n", error->message);
      return;
    }

  if (!(relations = dex_await_boxed (gom_repository_list_relations (repository), &error)))
    {
      g_printerr ("Failed to list relations: %s\n", error->message);
      return;
    }

  for (guint i = 0; relations[i] != NULL; i++)
    {
      if (!describe_print_relation (repository, relations[i], &error))
        {
          g_printerr ("Failed to describe %s: %s\n", relations[i], error->message);
          return;
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  static const GOptionEntry entries[] = {
    { "dump", 0, 0, G_OPTION_ARG_NONE, &describe_dump, "Dump table contents as CSV", NULL },
    { NULL }
  };
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *help = NULL;

  context = g_option_context_new ("/path/to/database.db");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Failed to parse options: %s\n", error->message);
      return 1;
    }

  if (argc != 2)
    {
      help = g_option_context_get_help (context, FALSE, NULL);
      g_printerr ("%s", help);
      return 1;
    }

  describe_db_path = argv[1];

  _dex_test_runner (describe_run);

  return 0;
}
