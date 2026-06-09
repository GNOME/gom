/* gom-pgsql-driver.c
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
#include <gmodule.h>

#include <pgsql-glib.h>
#include <pgsql-connection-pool.h>
#include <pgsql-transaction.h>

#include "gom-mutation.h"
#include "gom-cursor-private.h"
#include "gom-driver-private.h"
#include "gom-entity-private.h"
#include "gom-expression-private.h"
#include "gom-meta-private.h"
#include "gom-mutation-result-private.h"
#include "gom-pgsql-driver-private.h"
#include "gom-pgsql-cursor-private.h"
#include "gom-pgsql-session-private.h"
#include "gom-repository-private.h"
#include "gom-schema-private.h"
#include "gom-registry-diff-private.h"
#include "gom-ordering.h"
#include "gom-insertion-private.h"
#include "gom-update-private.h"
#include "gom-deletion-private.h"
#include "gom-query-private.h"
#include "gom-record-private.h"
#include "gom-repository-private.h"
#include "gom-util-private.h"

typedef struct
{
  GValue   value;
  gboolean has_value;
} GomPgsqlBinding;

static void
gom_pgsql_binding_free (gpointer data)
{
  GomPgsqlBinding *binding = data;

  if (binding == NULL)
    return;

  if (binding->has_value)
    g_value_unset (&binding->value);

  g_free (binding);
}

static GomPgsqlBinding *
gom_pgsql_binding_new (const GValue *value)
{
  GomPgsqlBinding *binding = g_new0 (GomPgsqlBinding, 1);

  if (value != NULL && G_VALUE_TYPE (value) != G_TYPE_INVALID)
    {
      g_value_init (&binding->value, G_VALUE_TYPE (value));
      g_value_copy (value, &binding->value);
      binding->has_value = TRUE;
    }

  return binding;
}

static gboolean
gom_pgsql_binding_append_to_params (PgsqlParams            *params,
                                    const GomPgsqlBinding  *binding,
                                    GError                **error)
{
  const GValue *value;

  g_return_val_if_fail (PGSQL_IS_PARAMS (params), FALSE);
  g_return_val_if_fail (binding != NULL, FALSE);

  if (!binding->has_value || G_VALUE_TYPE (&binding->value) == G_TYPE_INVALID)
    {
      pgsql_params_add_null (params);
      return TRUE;
    }

  value = &binding->value;

  if (G_VALUE_HOLDS_BOOLEAN (value))
    pgsql_params_add_boolean (params, g_value_get_boolean (value));
  else if (G_VALUE_HOLDS_INT64 (value))
    pgsql_params_add_int64 (params, g_value_get_int64 (value));
  else if (G_VALUE_HOLDS_INT (value))
    pgsql_params_add_int64 (params, (gint64)g_value_get_int (value));
  else if (G_VALUE_HOLDS_UINT (value))
    pgsql_params_add_int64 (params, (gint64)g_value_get_uint (value));
  else if (G_VALUE_HOLDS_UINT64 (value))
    {
      guint64 v = g_value_get_uint64 (value);
      if (v > G_MAXINT64)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "UINT64 parameter is out of PostgreSQL int64 range");
          return FALSE;
        }
      pgsql_params_add_int64 (params, (gint64)v);
    }
  else if (G_VALUE_HOLDS_DOUBLE (value))
    pgsql_params_add_float8 (params, g_value_get_double (value));
  else if (G_VALUE_HOLDS_FLOAT (value))
    pgsql_params_add_float8 (params, (gdouble)g_value_get_float (value));
  else if (G_VALUE_HOLDS_STRING (value))
    pgsql_params_add_text (params, g_value_get_string (value));
  else if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
    pgsql_params_add_date_time (params, g_value_get_boxed (value));
  else if (G_VALUE_HOLDS (value, G_TYPE_BYTES))
    pgsql_params_add_bytea (params, g_value_get_boxed (value));
  else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
    {
      g_autofree char *encoded = _gom_strv_to_text (g_value_get_boxed (value));
      pgsql_params_add_text (params, encoded);
    }
  else if (G_VALUE_HOLDS_ENUM (value))
    pgsql_params_add_int64 (params, (gint64)g_value_get_enum (value));
  else if (G_VALUE_HOLDS_FLAGS (value))
    pgsql_params_add_int64 (params, (gint64)g_value_get_flags (value));
  else if (G_VALUE_HOLDS (value, G_TYPE_GTYPE))
    pgsql_params_add_text (params, g_type_name (g_value_get_gtype (value)));
  else if (G_VALUE_HOLDS_POINTER (value) && g_value_get_pointer (value) == NULL)
    pgsql_params_add_null (params);
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Unsupported PostgreSQL binding type %s",
                   g_type_name (G_VALUE_TYPE (value)));
      return FALSE;
    }

  return TRUE;
}

static PgsqlParams *
gom_pgsql_params_from_bindings (GPtrArray  *bindings,
                                GError    **error)
{
  g_autoptr(PgsqlParams) params = NULL;

  params = pgsql_params_new ();

  if (bindings != NULL)
    {
      for (guint i = 0; i < bindings->len; i++)
        {
          GomPgsqlBinding *binding = g_ptr_array_index (bindings, i);

          if (!gom_pgsql_binding_append_to_params (params, binding, error))
            return NULL;
        }
    }

  return g_steal_pointer (&params);
}

static char *
gom_pgsql_renumber_placeholders (const char *sql)
{
  g_autoptr(GString) translated = NULL;
  guint param = 1;

  translated = g_string_sized_new (strlen (sql) + 8);

  for (const char *iter = sql; *iter; iter++)
    {
      if (*iter == '?')
        g_string_append_printf (translated, "$%u", param++);
      else
        g_string_append_c (translated, *iter);
    }

  return g_string_free (g_steal_pointer (&translated), FALSE);
}

static void
gom_pgsql_append_quoted_identifier (GString    *str,
                                    const char *identifier)
{
  g_string_append_c (str, '"');
  for (const char *iter = identifier; *iter; iter++)
    {
      if (*iter == '"')
        g_string_append (str, "\"\"");
      else
        g_string_append_c (str, *iter);
    }
  g_string_append_c (str, '"');
}

static void
gom_pgsql_append_quoted_identifier_path (GString    *str,
                                         const char *identifier)
{
  gchar **parts;

  if (identifier == NULL || *identifier == '\0')
    return;

  parts = g_strsplit (identifier, ".", -1);
  for (guint i = 0; parts[i] != NULL; i++)
    {
      if (i > 0)
        g_string_append_c (str, '.');
      gom_pgsql_append_quoted_identifier (str, parts[i]);
    }
  g_strfreev (parts);
}

static const char *
gom_pgsql_binary_operator_to_sql (GomBinaryOperator op)
{
  switch (op)
    {
    case GOM_BINARY_ADD:
      return "+";
    case GOM_BINARY_SUBTRACT:
      return "-";
    case GOM_BINARY_MULTIPLY:
      return "*";
    case GOM_BINARY_DIVIDE:
      return "/";
    case GOM_BINARY_MODULO:
      return "%";
    case GOM_BINARY_EQUAL:
      return "=";
    case GOM_BINARY_NOT_EQUAL:
      return "!=";
    case GOM_BINARY_LESS_THAN:
      return "<";
    case GOM_BINARY_LESS_EQUAL:
      return "<=";
    case GOM_BINARY_GREATER_THAN:
      return ">";
    case GOM_BINARY_GREATER_EQUAL:
      return ">=";
    case GOM_BINARY_AND:
      return "AND";
    case GOM_BINARY_OR:
      return "OR";
    case GOM_BINARY_LIKE:
      return "LIKE";
    default:
      return NULL;
    }
}

static gboolean
gom_pgsql_token_is_operator (const char *token)
{
  if (token == NULL || *token == '\0')
    return FALSE;

  return g_ascii_strcasecmp (token, "AND") == 0 ||
         g_ascii_strcasecmp (token, "OR") == 0 ||
         g_ascii_strcasecmp (token, "NOT") == 0;
}

static char *
gom_pgsql_build_prefix_tsquery (const char *query)
{
  g_auto(GStrv) parts = g_strsplit_set (query, " \t\r\n", -1);
  g_autoptr(GString) tsquery = g_string_new (NULL);

  for (guint i = 0; parts != NULL && parts[i] != NULL; i++)
    {
      const char *part = parts[i];

      if (part == NULL || *part == '\0')
        continue;

      if (tsquery->len > 0)
        g_string_append_c (tsquery, ' ');

      if (gom_pgsql_token_is_operator (part) ||
          strchr (part, '"') != NULL ||
          strchr (part, ':') != NULL ||
          g_str_has_suffix (part, "*"))
        g_string_append (tsquery, part);
      else
        {
          g_string_append (tsquery, part);
          g_string_append (tsquery, ":*");
        }
    }

  return g_string_free (g_steal_pointer (&tsquery), FALSE);
}

static char *
gom_pgsql_build_tsquery (const char     *query,
                         GomSearchMode   mode,
                         GError        **error)
{
  switch (mode)
    {
    case GOM_SEARCH_MODE_NATURAL:
      return g_strdup (query);

    case GOM_SEARCH_MODE_PHRASE:
      return g_strdup (query);

    case GOM_SEARCH_MODE_PREFIX:
      return gom_pgsql_build_prefix_tsquery (query);

    default:
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Unknown search mode");
      return NULL;
    }
}

static gboolean
gom_pgsql_binding_is_null_literal (GomExpression *expression)
{
  const GValue *value;

  if (!GOM_IS_LITERAL_EXPRESSION (expression))
    return FALSE;

  value = _gom_literal_expression_peek_value (GOM_LITERAL_EXPRESSION (expression));
  return value == NULL ||
         (G_VALUE_HOLDS_POINTER (value) && g_value_get_pointer (value) == NULL) ||
         (G_VALUE_HOLDS_STRING (value) && g_value_get_string (value) == NULL);
}

static gboolean
gom_pgsql_resolve_entity_field (GomEntitySpec  *entity,
                                const char     *field,
                                const char    **out_field,
                                GError        **error)
{
  const GomPropertySpec *property;

  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (entity), FALSE);
  g_return_val_if_fail (field != NULL, FALSE);
  g_return_val_if_fail (out_field != NULL, FALSE);

  if ((property = _gom_entity_spec_lookup_property_by_name (entity, field)))
    {
      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' property '%s' is not mapped",
                       gom_entity_spec_get_name (entity),
                       field);
          return FALSE;
        }

      *out_field = gom_property_spec_get_field ((GomPropertySpec *)property);
      return TRUE;
    }

  if ((property = _gom_entity_spec_lookup_property_by_field (entity, field)))
    {
      if (!gom_property_spec_get_mapped ((GomPropertySpec *)property))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' field '%s' is not mapped",
                       gom_entity_spec_get_name (entity),
                       field);
          return FALSE;
        }

      *out_field = field;
      return TRUE;
    }

  if (_gom_strv_contains (gom_entity_spec_get_identity_fields (entity), field))
    {
      *out_field = field;
      return TRUE;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_ARGUMENT,
               "Entity '%s' has no mapped property or field named '%s'",
               gom_entity_spec_get_name (entity),
               field);
  return FALSE;
}

typedef struct
{
  GomEntitySpec *entity;
  const char    *field_prefix;
} GomPgsqlExpressionContext;

static gboolean
gom_pgsql_append_expression_with_context (GomExpression                    *expression,
                                          GString                          *sql,
                                          GPtrArray                        *bindings,
                                          GError                          **error,
                                          const GomPgsqlExpressionContext  *context);

static gboolean
gom_pgsql_append_expression_list_with_context (GPtrArray                        *expressions,
                                               GString                          *sql,
                                               GPtrArray                        *bindings,
                                               GError                          **error,
                                               const GomPgsqlExpressionContext  *context)
{
  if (expressions == NULL || expressions->len == 0)
    return TRUE;

  for (guint i = 0; i < expressions->len; i++)
    {
      if (!gom_pgsql_append_expression_with_context (g_ptr_array_index (expressions, i),
                                                     sql,
                                                     bindings,
                                                     error,
                                                     context))
        return FALSE;

      if (i + 1 < expressions->len)
        g_string_append (sql, ", ");
    }

  return TRUE;
}

static gboolean
gom_pgsql_append_orderings_with_context (GPtrArray                        *orderings,
                                         GString                          *sql,
                                         GPtrArray                        *bindings,
                                         GError                          **error,
                                         const GomPgsqlExpressionContext  *context)
{
  if (orderings == NULL || orderings->len == 0)
    return TRUE;

  for (guint i = 0; i < orderings->len; i++)
    {
      GomOrdering *ordering = g_ptr_array_index (orderings, i);

      if (!gom_pgsql_append_expression_with_context (gom_ordering_get_expression (ordering),
                                                     sql,
                                                     bindings,
                                                     error,
                                                     context))
        return FALSE;

      if (gom_ordering_get_direction (ordering) == GOM_SORT_DESCENDING)
        g_string_append (sql, " DESC");

      if (gom_ordering_get_nulls_mode (ordering) == GOM_NULLS_FIRST)
        g_string_append (sql, " NULLS FIRST");
      else if (gom_ordering_get_nulls_mode (ordering) == GOM_NULLS_LAST)
        g_string_append (sql, " NULLS LAST");

      if (i + 1 < orderings->len)
        g_string_append (sql, ", ");
    }

  return TRUE;
}

static gboolean
gom_pgsql_append_uint64_binding (GPtrArray  *bindings,
                                 guint64     value,
                                 GError    **error)
{
  GValue binding_value = G_VALUE_INIT;

  if (value > G_MAXINT64)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "PostgreSQL cannot bind UINT64 value %" G_GUINT64_FORMAT,
                   value);
      return FALSE;
    }

  g_value_init (&binding_value, G_TYPE_INT64);
  g_value_set_int64 (&binding_value, (gint64)value);
  g_ptr_array_add (bindings, gom_pgsql_binding_new (&binding_value));
  g_value_unset (&binding_value);

  return TRUE;
}

static gboolean
gom_pgsql_append_expression_with_context (GomExpression                    *expression,
                                          GString                          *sql,
                                          GPtrArray                        *bindings,
                                          GError                          **error,
                                          const GomPgsqlExpressionContext  *context)
{
  if (GOM_IS_LITERAL_EXPRESSION (expression))
    {
      if (gom_pgsql_binding_is_null_literal (expression))
        {
          g_string_append (sql, "NULL");
          return TRUE;
        }

      g_string_append_c (sql, '?');
      g_ptr_array_add (bindings,
                       gom_pgsql_binding_new (_gom_literal_expression_peek_value (GOM_LITERAL_EXPRESSION (expression))));
      return TRUE;
    }

  if (GOM_IS_FIELD_EXPRESSION (expression))
    {
      const char *field = _gom_field_expression_get_field (GOM_FIELD_EXPRESSION (expression));
      const char *resolved = field;

      if (field == NULL || *field == '\0')
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Field expression requires a field name");
          return FALSE;
        }

      if (context != NULL && context->entity != NULL)
        {
          if (strchr (field, '.') != NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Qualified field references are not supported for entity '%s': '%s'",
                           gom_entity_spec_get_name (context->entity),
                           field);
              return FALSE;
            }

          if (!gom_pgsql_resolve_entity_field (context->entity, field, &resolved, error))
            return FALSE;
        }

      if (context != NULL && context->field_prefix != NULL && *context->field_prefix != '\0')
        {
          g_string_append (sql, context->field_prefix);
          g_string_append_c (sql, '.');
        }

      if (resolved != NULL && strchr (resolved, '.') != NULL)
        gom_pgsql_append_quoted_identifier_path (sql, resolved);
      else if (resolved != NULL)
        gom_pgsql_append_quoted_identifier (sql, resolved);
      return TRUE;
    }

  if (GOM_IS_FUNCTION_EXPRESSION (expression))
    {
      const char *name = _gom_function_expression_get_name (GOM_FUNCTION_EXPRESSION (expression));
      GPtrArray *arguments = _gom_function_expression_get_arguments (GOM_FUNCTION_EXPRESSION (expression));

      if (name == NULL || *name == '\0')
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Function expression requires a name");
          return FALSE;
        }

      g_string_append (sql, name);
      g_string_append_c (sql, '(');

      if (!gom_pgsql_append_expression_list_with_context (arguments, sql, bindings, error, context))
        return FALSE;

      g_string_append_c (sql, ')');
      return TRUE;
    }

  if (GOM_IS_VECTOR_DISTANCE_EXPRESSION (expression))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "PostgreSQL vector search is not supported");
      return FALSE;
    }

  if (GOM_IS_SEARCH_EXPRESSION (expression))
    {
      GomExpression *target = _gom_search_expression_get_target (GOM_SEARCH_EXPRESSION (expression));
      GomExpression *query = _gom_search_expression_get_query (GOM_SEARCH_EXPRESSION (expression));
      GomSearchMode mode = _gom_search_expression_get_mode (GOM_SEARCH_EXPRESSION (expression));

      if (target == NULL || query == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Search expression requires a target and query");
          return FALSE;
        }

      if (!GOM_IS_LITERAL_EXPRESSION (query))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Search query must be a string literal");
          return FALSE;
        }

      g_string_append (sql, "(to_tsvector('simple', ");
      if (!gom_pgsql_append_expression_with_context (target, sql, bindings, error, context))
        return FALSE;
      g_string_append (sql, ") @@ ");

      {
        const GValue *value = _gom_literal_expression_peek_value (GOM_LITERAL_EXPRESSION (query));
        const char *query_text;
        g_autofree char *tsquery = NULL;
        g_auto(GValue) binding_value = G_VALUE_INIT;

        if (value == NULL || !G_VALUE_HOLDS_STRING (value))
          {
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_ARGUMENT,
                                 "Search query must be a string literal");
            return FALSE;
          }

        if (!(query_text = g_value_get_string (value)))
          {
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_ARGUMENT,
                                 "Search query cannot be NULL");
            return FALSE;
          }

        if (!(tsquery = gom_pgsql_build_tsquery (query_text, mode, error)))
          return FALSE;

        switch (mode)
          {
          case GOM_SEARCH_MODE_NATURAL:
            g_string_append (sql, "plainto_tsquery('simple', ?))");
            break;

          case GOM_SEARCH_MODE_PHRASE:
            g_string_append (sql, "phraseto_tsquery('simple', ?))");
            break;

          case GOM_SEARCH_MODE_PREFIX:
            g_string_append (sql, "to_tsquery('simple', ?))");
            break;

          default:
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_ARGUMENT,
                                 "Unknown search mode");
            return FALSE;
          }

        g_value_init (&binding_value, G_TYPE_STRING);
        g_value_take_string (&binding_value, g_steal_pointer (&tsquery));
        g_ptr_array_add (bindings, gom_pgsql_binding_new (&binding_value));
      }

      return TRUE;
    }

  if (GOM_IS_UNARY_EXPRESSION (expression))
    {
      GomUnaryOperator op = _gom_unary_expression_get_operator (GOM_UNARY_EXPRESSION (expression));
      GomExpression *operand = _gom_unary_expression_get_operand (GOM_UNARY_EXPRESSION (expression));

      g_string_append_c (sql, '(');
      switch (op)
        {
        case GOM_UNARY_NEGATE:
          g_string_append_c (sql, '-');
          break;
        case GOM_UNARY_NOT:
          g_string_append (sql, "NOT ");
          break;
        default:
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Unknown unary operator");
          return FALSE;
        }

      if (!gom_pgsql_append_expression_with_context (operand, sql, bindings, error, context))
        return FALSE;

      g_string_append_c (sql, ')');
      return TRUE;
    }

  if (GOM_IS_BINARY_EXPRESSION (expression))
    {
      GomBinaryOperator op = _gom_binary_expression_get_operator (GOM_BINARY_EXPRESSION (expression));
      GomExpression *left = _gom_binary_expression_get_left (GOM_BINARY_EXPRESSION (expression));
      GomExpression *right = _gom_binary_expression_get_right (GOM_BINARY_EXPRESSION (expression));
      const char *op_sql = gom_pgsql_binary_operator_to_sql (op);
      gboolean left_is_null = gom_pgsql_binding_is_null_literal (left);
      gboolean right_is_null = gom_pgsql_binding_is_null_literal (right);

      if (op_sql == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Unknown binary operator");
          return FALSE;
        }

      if ((op == GOM_BINARY_EQUAL || op == GOM_BINARY_NOT_EQUAL) && (left_is_null || right_is_null))
        {
          GomExpression *non_null = left_is_null ? right : left;

          if (!gom_pgsql_append_expression_with_context (non_null, sql, bindings, error, context))
            return FALSE;

          g_string_append (sql, op == GOM_BINARY_EQUAL ? " IS NULL" : " IS NOT NULL");
          return TRUE;
        }

      g_string_append_c (sql, '(');
      if (!gom_pgsql_append_expression_with_context (left, sql, bindings, error, context))
        return FALSE;
      g_string_append_printf (sql, " %s ", op_sql);
      if (!gom_pgsql_append_expression_with_context (right, sql, bindings, error, context))
        return FALSE;
      g_string_append_c (sql, ')');
      return TRUE;
    }

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Unknown expression type");
  return FALSE;
}

static gboolean
gom_pgsql_build_query_sql (GomQuery                         *query,
                           const char                       *base_relation,
                           const GomPgsqlExpressionContext  *context,
                           gboolean                          use_projections,
                           gboolean                          include_orderings,
                           gboolean                          include_limit_offset,
                           GString                         **out_sql,
                           GPtrArray                       **out_bindings,
                           GError                          **error)
{
  g_autoptr(GString) sql = NULL;
  g_autoptr(GPtrArray) bindings = NULL;
  GPtrArray *projections;
  GPtrArray *groupings;
  GPtrArray *orderings;
  GomExpression *filter;
  GomExpression *group_filter;

  projections = _gom_query_get_projections (query);
  filter = _gom_query_get_filter (query);
  groupings = _gom_query_get_groupings (query);
  group_filter = _gom_query_get_group_filter (query);
  orderings = _gom_query_get_orderings (query);

  sql = g_string_new ("SELECT ");
  bindings = g_ptr_array_new_with_free_func (gom_pgsql_binding_free);

  if (use_projections)
    {
      if (projections == NULL || projections->len == 0)
        g_string_append (sql, "*");
      else if (!gom_pgsql_append_expression_list_with_context (projections,
                                                               sql,
                                                               bindings,
                                                               error,
                                                               context))
        return FALSE;
    }
  else
    {
      g_string_append (sql, "1");
    }

  g_string_append (sql, " FROM ");
  gom_pgsql_append_quoted_identifier_path (sql, base_relation);

  if (filter != NULL)
    {
      g_string_append (sql, " WHERE ");
      if (!gom_pgsql_append_expression_with_context (filter, sql, bindings, error, context))
        return FALSE;
    }

  if (groupings != NULL && groupings->len > 0)
    {
      g_string_append (sql, " GROUP BY ");
      if (!gom_pgsql_append_expression_list_with_context (groupings, sql, bindings, error, context))
        return FALSE;
    }

  if (group_filter != NULL)
    {
      g_string_append (sql, " HAVING ");
      if (!gom_pgsql_append_expression_with_context (group_filter, sql, bindings, error, context))
        return FALSE;
    }

  if (include_orderings && orderings != NULL && orderings->len > 0)
    {
      g_string_append (sql, " ORDER BY ");
      if (!gom_pgsql_append_orderings_with_context (orderings, sql, bindings, error, context))
        return FALSE;
    }

  if (include_limit_offset)
    {
      if (_gom_query_has_limit (query))
        {
          g_string_append (sql, " LIMIT ?");
          if (!gom_pgsql_append_uint64_binding (bindings, _gom_query_get_limit (query), error))
            return FALSE;
        }

      if (_gom_query_has_offset (query))
        {
          if (!_gom_query_has_limit (query))
            g_string_append (sql, " LIMIT ALL");

          g_string_append (sql, " OFFSET ?");
          if (!gom_pgsql_append_uint64_binding (bindings, _gom_query_get_offset (query), error))
            return FALSE;
        }
    }

  *out_sql = g_steal_pointer (&sql);
  *out_bindings = g_steal_pointer (&bindings);
  return TRUE;
}

static DexFuture *
gom_pgsql_result_to_mutation_result (PgsqlResult *result)
{
  g_autoptr(GomMutationResult) mutation_result = NULL;

  mutation_result = _gom_mutation_result_new ();

  for (guint i = 0; i < pgsql_result_get_n_rows (result); i++)
    {
      guint n_fields = pgsql_result_get_n_fields (result);
      g_autofree const char **column_names = g_new0 (const char *, n_fields);
      g_autofree GValue *values = g_new0 (GValue, n_fields);
      g_autoptr(GomRecord) record = NULL;

      for (guint j = 0; j < n_fields; j++)
        {
          column_names[j] = pgsql_result_get_field_name (result, j);
          if (!gom_pgsql_cursor_set_value (result, i, j, &values[j]))
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_INVALID_DATA,
                                          "Failed to convert PostgreSQL result");
        }

      record = _gom_record_new_from_values (column_names, values, n_fields);
      _gom_mutation_result_append_record (mutation_result, record, 1);

      for (guint j = 0; j < n_fields; j++)
        if (G_IS_VALUE (&values[j]))
          g_value_unset (&values[j]);
    }

  return dex_future_new_take_object (g_steal_pointer (&mutation_result));
}

static const char *
gom_pgsql_resolve_relation_name (GomRegistry          *registry,
                                 GType                 entity_type,
                                 const char           *relation,
                                 const char           *operation_name,
                                 const GomEntitySpec **out_entity,
                                 GError              **error)
{
  const GomEntitySpec *entity = NULL;
  const char *resolved = relation;

  if (entity_type != G_TYPE_INVALID)
    {
      if (!(entity = _gom_registry_lookup_entity_by_type (registry, entity_type)))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "%s target entity type '%s' is not registered",
                       operation_name,
                       g_type_name (entity_type));
          return NULL;
        }

      if (!gom_str_empty0 (resolved))
        {
          const char *table = gom_entity_spec_get_table ((GomEntitySpec *)entity);
          const char *name = gom_entity_spec_get_name ((GomEntitySpec *)entity);

          if (g_strcmp0 (resolved, table) != 0 && g_strcmp0 (resolved, name) != 0)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "%s target relation '%s' does not match entity type '%s'",
                           operation_name,
                           resolved,
                           g_type_name (entity_type));
              return NULL;
            }
        }

      resolved = gom_entity_spec_get_table ((GomEntitySpec *)entity);
      if (resolved == NULL || *resolved == '\0')
        resolved = gom_entity_spec_get_name ((GomEntitySpec *)entity);
    }
  else if (!gom_str_empty0 (resolved))
    {
      if (!(entity = _gom_registry_lookup_entity_by_table (registry, resolved)))
        if ((entity = _gom_registry_lookup_entity_by_name (registry, resolved)))
        {
          const char *table = gom_entity_spec_get_table ((GomEntitySpec *)entity);
          resolved = !gom_str_empty0 (table)
                       ? table
                       : gom_entity_spec_get_name ((GomEntitySpec *)entity);
        }
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "%s requires a target entity type or relation",
                   operation_name);
      return NULL;
    }

  if (out_entity != NULL)
    *out_entity = entity;

  return resolved;
}

typedef struct
{
  char     *property_name;
  char     *field;
  char     *sql_type;
  char     *ref_table;
  char     *ref_field;
  gboolean  nonnull;
  gboolean  primary_key;
  gboolean  unique;
} GomPgsqlColumnDef;

static void
gom_pgsql_column_def_free (gpointer data)
{
  GomPgsqlColumnDef *def = data;

  if (def == NULL)
    return;

  g_clear_pointer (&def->property_name, g_free);
  g_clear_pointer (&def->field, g_free);
  g_clear_pointer (&def->sql_type, g_free);
  g_clear_pointer (&def->ref_table, g_free);
  g_clear_pointer (&def->ref_field, g_free);
  g_free (def);
}

static const char *
gom_pgsql_sql_type_for_gtype (GType type)
{
  if (type == G_TYPE_BOOLEAN)
    return "boolean";
  if (type == G_TYPE_INT || type == G_TYPE_UINT ||
      type == G_TYPE_INT64 || type == G_TYPE_UINT64 ||
      G_TYPE_IS_ENUM (type) || G_TYPE_IS_FLAGS (type))
    return "bigint";
  if (type == G_TYPE_FLOAT || type == G_TYPE_DOUBLE)
    return "double precision";
  if (type == G_TYPE_STRING || type == G_TYPE_GTYPE)
    return "text";
  if (type == G_TYPE_DATE_TIME)
    return "timestamptz";
  if (type == G_TYPE_BYTES)
    return "bytea";
  if (type == G_TYPE_STRV)
    return "text[]";

  return "text";
}

static gboolean
gom_pgsql_driver_collect_entity_columns (GomEntitySpec  *entity,
                                         GPtrArray     **out_columns,
                                         GPtrArray     **out_pk_fields,
                                         GError       **error)
{
  g_autoptr(GPtrArray) columns = NULL;
  g_autoptr(GPtrArray) pk_fields = NULL;
  GTypeClass *klass = NULL;
  const GomPropertySpec * const *properties;
  guint n_properties = 0;
  const char * const *identity_fields;

  g_assert (GOM_IS_ENTITY_SPEC (entity));
  g_assert (out_columns != NULL);
  g_assert (out_pk_fields != NULL);

  columns = g_ptr_array_new_with_free_func (gom_pgsql_column_def_free);
  pk_fields = g_ptr_array_new_with_free_func (g_free);
  klass = g_type_class_ref (gom_entity_spec_get_entity_type (entity));
  properties = gom_entity_spec_list_properties (entity, &n_properties);
  identity_fields = gom_entity_spec_get_identity_fields (entity);

  for (guint i = 0; i < n_properties; i++)
    {
      GomPropertySpec *property = (GomPropertySpec *)properties[i];
      GParamSpec *pspec;
      const char *property_name;
      const char *field;
      const char *ref_table;
      const char *ref_field;
      GomPgsqlColumnDef *column;

      if (!gom_property_spec_get_mapped (property))
        continue;

      property_name = gom_property_spec_get_name (property);
      field = gom_property_spec_get_field (property);
      if (field == NULL || *field == '\0')
        field = property_name;

      if (field == NULL || *field == '\0')
        continue;

      if (!(pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), property_name)))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity '%s' property '%s' has no GParamSpec",
                       gom_entity_spec_get_name (entity),
                       property_name);
          return FALSE;
        }

      column = g_new0 (GomPgsqlColumnDef, 1);
      column->property_name = g_strdup (property_name);
      column->field = g_strdup (field);
      column->sql_type = g_strdup (gom_pgsql_sql_type_for_gtype (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      column->nonnull = gom_property_spec_get_nonnull (property);
      column->unique = gom_property_spec_get_unique (property);

      ref_table = gom_property_spec_get_reference_table (property);
      ref_field = gom_property_spec_get_reference_field (property);
      if (!gom_str_empty0 (ref_table) && !gom_str_empty0 (ref_field))
        {
          column->ref_table = g_strdup (ref_table);
          column->ref_field = g_strdup (ref_field);
        }

      g_ptr_array_add (columns, column);
    }

  if (identity_fields != NULL)
    {
      for (guint i = 0; identity_fields[i] != NULL; i++)
        g_ptr_array_add (pk_fields, g_strdup (identity_fields[i]));
    }

  *out_columns = g_steal_pointer (&columns);
  *out_pk_fields = g_steal_pointer (&pk_fields);
  g_type_class_unref (klass);
  return TRUE;
}

static gboolean gom_pgsql_driver_exec_sql (gpointer              executor,
                                           GomPgsqlQueryRunner   runner,
                                           const char           *sql,
                                           GError              **error);

static void
gom_pgsql_driver_append_column_definition (GString           *sql,
                                           GomPgsqlColumnDef *column,
                                           gboolean           primary_key)
{
  g_string_append (sql, column->field);
  g_string_append_c (sql, ' ');
  g_string_append (sql, column->sql_type != NULL ? column->sql_type : "text");

  if (primary_key)
    g_string_append (sql, " PRIMARY KEY");

  if (column->nonnull || primary_key)
    g_string_append (sql, " NOT NULL");

  if (column->unique)
    g_string_append (sql, " UNIQUE");

  if (column->ref_table != NULL && column->ref_field != NULL)
    {
      g_string_append (sql, " REFERENCES ");
      gom_pgsql_append_quoted_identifier_path (sql, column->ref_table);
      g_string_append_c (sql, '(');
      gom_pgsql_append_quoted_identifier (sql, column->ref_field);
      g_string_append_c (sql, ')');
    }
}

static gboolean
gom_pgsql_driver_exec_sql (gpointer              executor,
                           GomPgsqlQueryRunner   runner,
                           const char           *sql,
                           GError              **error)
{
  g_autoptr(PgsqlResult) result = NULL;

  g_return_val_if_fail (runner != NULL, FALSE);
  g_return_val_if_fail (sql != NULL, FALSE);

  if (!(result = dex_await_object (runner (executor, sql, NULL), error)))
    return FALSE;

  if (!pgsql_result_is_successful (result))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "PostgreSQL statement failed: %s",
                   pgsql_result_get_error_message (result));
      return FALSE;
    }

  return TRUE;
}

static gboolean
gom_pgsql_driver_create_table_for_entity (gpointer              executor,
                                          GomPgsqlQueryRunner   runner,
                                          GomEntitySpec        *entity,
                                          const char           *table,
                                          GError              **error)
{
  g_autoptr(GPtrArray) columns = NULL;
  g_autoptr(GPtrArray) pk_fields = NULL;
  g_autoptr(GString) sql = NULL;
  g_autofree char *sql_to_run = NULL;

  if (!gom_pgsql_driver_collect_entity_columns (entity, &columns, &pk_fields, error))
    return FALSE;

  if (columns->len == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity '%s' has no mapped columns",
                   gom_entity_spec_get_name (entity));
      return FALSE;
    }

  sql = g_string_new ("CREATE TABLE IF NOT EXISTS ");
  gom_pgsql_append_quoted_identifier_path (sql, table);
  g_string_append (sql, " (");

  for (guint i = 0; i < columns->len; i++)
    {
      GomPgsqlColumnDef *column = g_ptr_array_index (columns, i);
      gboolean primary_key = FALSE;

      if (pk_fields->len == 1 && g_strcmp0 (g_ptr_array_index (pk_fields, 0), column->field) == 0)
        primary_key = TRUE;

      if (i > 0)
        g_string_append (sql, ", ");

      gom_pgsql_driver_append_column_definition (sql, column, primary_key);
    }

  if (pk_fields->len > 1)
    {
      g_string_append (sql, ", PRIMARY KEY (");
      for (guint i = 0; i < pk_fields->len; i++)
        {
          if (i > 0)
            g_string_append (sql, ", ");
          gom_pgsql_append_quoted_identifier (sql, g_ptr_array_index (pk_fields, i));
        }
      g_string_append_c (sql, ')');
    }

  g_string_append_c (sql, ')');

  sql_to_run = g_string_free (g_steal_pointer (&sql), FALSE);
  return gom_pgsql_driver_exec_sql (executor, runner, sql_to_run, error);
}

static char *
gom_pgsql_driver_get_index_name (const char   *table,
                                 GomIndexSpec *index)
{
  const char *name = gom_index_spec_get_name (index);

  if (!gom_str_empty0 (name))
    return g_strdup_printf ("%s_%s", table, name);

  return g_strdup_printf ("%s_index", table);
}

static gboolean
gom_pgsql_driver_create_index_for_entity (gpointer              executor,
                                          GomPgsqlQueryRunner   runner,
                                          const char           *table,
                                          GomIndexSpec         *index,
                                          GError              **error)
{
  g_autofree char *index_name = NULL;
  g_autoptr(GString) sql = NULL;
  const char * const *fields;

  fields = gom_index_spec_get_fields (index);
  if (fields == NULL || fields[0] == NULL)
    return TRUE;

  index_name = gom_pgsql_driver_get_index_name (table, index);
  sql = g_string_new ("CREATE ");

  if (gom_index_spec_get_unique (index))
    g_string_append (sql, "UNIQUE ");

  g_string_append (sql, "INDEX IF NOT EXISTS ");
  gom_pgsql_append_quoted_identifier (sql, index_name);
  g_string_append (sql, " ON ");
  gom_pgsql_append_quoted_identifier_path (sql, table);
  g_string_append (sql, " (");

  for (guint i = 0; fields[i] != NULL; i++)
    {
      if (i > 0)
        g_string_append (sql, ", ");
      gom_pgsql_append_quoted_identifier (sql, fields[i]);
    }

  g_string_append_c (sql, ')');
  return gom_pgsql_driver_exec_sql (executor, runner, sql->str, error);
}

static gboolean
gom_pgsql_driver_drop_index_for_entity (gpointer              executor,
                                        GomPgsqlQueryRunner   runner,
                                        const char           *table,
                                        GomIndexSpec         *index,
                                        GError              **error)
{
  g_autofree char *index_name = NULL;
  g_autoptr(GString) sql = NULL;

  index_name = gom_pgsql_driver_get_index_name (table, index);
  sql = g_string_new ("DROP INDEX IF EXISTS ");
  gom_pgsql_append_quoted_identifier (sql, index_name);
  return gom_pgsql_driver_exec_sql (executor, runner, sql->str, error);
}

static gboolean
gom_pgsql_driver_drop_all_indexes_for_entity (gpointer              executor,
                                              GomPgsqlQueryRunner   runner,
                                              GomEntitySpec        *entity,
                                              GError              **error)
{
  const GomIndexSpec * const *entity_indexes;
  guint n_indexes = 0;
  const char *table;

  table = gom_entity_spec_get_table (entity);
  if (table == NULL || *table == '\0')
    table = gom_entity_spec_get_name (entity);

  entity_indexes = gom_entity_spec_list_indexes (entity, &n_indexes);
  for (guint i = 0; i < n_indexes; i++)
    {
      if (!gom_pgsql_driver_drop_index_for_entity (executor,
                                                   runner,
                                                   table,
                                                   (GomIndexSpec *)entity_indexes[i],
                                                   error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gom_pgsql_driver_create_all_indexes_for_entity (gpointer              executor,
                                                GomPgsqlQueryRunner   runner,
                                                GomEntitySpec        *entity,
                                                GError              **error)
{
  const GomIndexSpec * const *entity_indexes;
  guint n_indexes = 0;
  const char *table;

  table = gom_entity_spec_get_table (entity);
  if (table == NULL || *table == '\0')
    table = gom_entity_spec_get_name (entity);

  entity_indexes = gom_entity_spec_list_indexes (entity, &n_indexes);
  for (guint i = 0; i < n_indexes; i++)
    {
      if (!gom_pgsql_driver_create_index_for_entity (executor,
                                                     runner,
                                                     table,
                                                     (GomIndexSpec *)entity_indexes[i],
                                                     error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gom_pgsql_driver_column_def_is_compatible (GomPgsqlColumnDef *current,
                                           GomPgsqlColumnDef *next)
{
  return g_strcmp0 (current->sql_type, next->sql_type) == 0 &&
         g_strcmp0 (current->ref_table, next->ref_table) == 0 &&
         g_strcmp0 (current->ref_field, next->ref_field) == 0 &&
         current->unique == next->unique;
}

static gboolean
gom_pgsql_driver_apply_column_rename (gpointer              executor,
                                      GomPgsqlQueryRunner   runner,
                                      const char           *table,
                                      const char           *old_name,
                                      const char           *new_name,
                                      GError              **error)
{
  g_autoptr(GString) sql = NULL;

  sql = g_string_new ("ALTER TABLE ");
  gom_pgsql_append_quoted_identifier_path (sql, table);
  g_string_append (sql, " RENAME COLUMN ");
  gom_pgsql_append_quoted_identifier (sql, old_name);
  g_string_append (sql, " TO ");
  gom_pgsql_append_quoted_identifier (sql, new_name);
  return gom_pgsql_driver_exec_sql (executor, runner, sql->str, error);
}

static gboolean
gom_pgsql_driver_apply_column_nullability (gpointer              executor,
                                           GomPgsqlQueryRunner   runner,
                                           const char           *table,
                                           const char           *column,
                                           gboolean              nonnull,
                                           GError              **error)
{
  g_autoptr(GString) sql = NULL;

  sql = g_string_new ("ALTER TABLE ");
  gom_pgsql_append_quoted_identifier_path (sql, table);
  g_string_append (sql, " ALTER COLUMN ");
  gom_pgsql_append_quoted_identifier (sql, column);
  g_string_append (sql, nonnull ? " SET NOT NULL" : " DROP NOT NULL");
  return gom_pgsql_driver_exec_sql (executor, runner, sql->str, error);
}

static gboolean
gom_pgsql_driver_apply_column_add (gpointer              executor,
                                   GomPgsqlQueryRunner   runner,
                                   const char           *table,
                                   GomPgsqlColumnDef    *column,
                                   GError              **error)
{
  g_autoptr(GString) sql = NULL;

  if (column->nonnull)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Cannot add required column '%s' to table '%s' without rebuilding it",
                   column->field,
                   table);
      return FALSE;
    }

  sql = g_string_new ("ALTER TABLE ");
  gom_pgsql_append_quoted_identifier_path (sql, table);
  g_string_append (sql, " ADD COLUMN ");
  gom_pgsql_driver_append_column_definition (sql, column, FALSE);
  return gom_pgsql_driver_exec_sql (executor, runner, sql->str, error);
}

static gboolean
gom_pgsql_driver_apply_column_drop (gpointer              executor,
                                    GomPgsqlQueryRunner   runner,
                                    const char           *table,
                                    const char           *column,
                                    GError              **error)
{
  g_autoptr(GString) sql = NULL;

  sql = g_string_new ("ALTER TABLE ");
  gom_pgsql_append_quoted_identifier_path (sql, table);
  g_string_append (sql, " DROP COLUMN ");
  gom_pgsql_append_quoted_identifier (sql, column);
  return gom_pgsql_driver_exec_sql (executor, runner, sql->str, error);
}

static gboolean
gom_pgsql_driver_apply_entity (gpointer              executor,
                               GomPgsqlQueryRunner   runner,
                               GomEntitySpec        *entity,
                               gboolean              drop_existing,
                               GError              **error)
{
  const GomIndexSpec * const *entity_indexes;
  guint n_indexes = 0;
  const char *table;

  table = gom_entity_spec_get_table (entity);
  if (table == NULL || *table == '\0')
    table = gom_entity_spec_get_name (entity);

  if (drop_existing)
    {
      g_autoptr(GString) drop_sql = g_string_new ("DROP TABLE IF EXISTS ");
      gom_pgsql_append_quoted_identifier_path (drop_sql, table);
      if (!gom_pgsql_driver_exec_sql (executor, runner, drop_sql->str, error))
        return FALSE;
    }

  if (!gom_pgsql_driver_create_table_for_entity (executor, runner, entity, table, error))
    return FALSE;

  entity_indexes = gom_entity_spec_list_indexes (entity, &n_indexes);
  for (guint i = 0; i < n_indexes; i++)
    {
      if (!gom_pgsql_driver_create_index_for_entity (executor,
                                                     runner,
                                                     table,
                                                     (GomIndexSpec *)entity_indexes[i],
                                                     error))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gom_pgsql_driver_pk_fields_equal (GPtrArray *a,
                                  GPtrArray *b)
{
  if (a == NULL || b == NULL)
    return a == b;

  if (a->len != b->len)
    return FALSE;

  for (guint i = 0; i < a->len; i++)
    {
      if (g_strcmp0 (g_ptr_array_index (a, i), g_ptr_array_index (b, i)) != 0)
        return FALSE;
    }

  return TRUE;
}

static gboolean
gom_pgsql_driver_migrate_changed_entity (gpointer              executor,
                                         GomPgsqlQueryRunner   runner,
                                         GomEntitySpec        *current_entity,
                                         GomEntitySpec        *next_entity,
                                         GError              **error)
{
  const char *current_table;
  g_autoptr(GPtrArray) current_columns = NULL;
  g_autoptr(GPtrArray) current_pk_fields = NULL;
  g_autoptr(GPtrArray) next_columns = NULL;
  g_autoptr(GPtrArray) next_pk_fields = NULL;
  g_autoptr(GHashTable) current_by_field = NULL;
  g_autoptr(GHashTable) current_by_property = NULL;
  g_autoptr(GHashTable) used_current = NULL;

  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (current_entity), FALSE);
  g_return_val_if_fail (GOM_IS_ENTITY_SPEC (next_entity), FALSE);

  current_table = gom_entity_spec_get_table (current_entity);
  if (current_table == NULL || *current_table == '\0')
    current_table = gom_entity_spec_get_name (current_entity);

  if (!gom_pgsql_driver_collect_entity_columns (current_entity,
                                                &current_columns,
                                                &current_pk_fields,
                                                error))
    return FALSE;

  if (!gom_pgsql_driver_collect_entity_columns (next_entity, &next_columns, &next_pk_fields, error))
    return FALSE;

  if (!gom_pgsql_driver_pk_fields_equal (current_pk_fields, next_pk_fields))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Cannot alter primary key for table '%s' in place",
                   current_table);
      return FALSE;
    }

  current_by_field = g_hash_table_new (g_str_hash, g_str_equal);
  current_by_property = g_hash_table_new (g_str_hash, g_str_equal);
  used_current = g_hash_table_new (g_direct_hash, g_direct_equal);

  for (guint i = 0; i < current_columns->len; i++)
    {
      GomPgsqlColumnDef *column = g_ptr_array_index (current_columns, i);
      g_hash_table_insert (current_by_field, column->field, column);
      if (column->property_name != NULL)
        g_hash_table_insert (current_by_property, column->property_name, column);
    }

  if (!gom_pgsql_driver_drop_all_indexes_for_entity (executor, runner, current_entity, error))
    return FALSE;

  for (guint i = 0; i < next_columns->len; i++)
    {
      GomPgsqlColumnDef *next_column = g_ptr_array_index (next_columns, i);
      GomPgsqlColumnDef *current_column = NULL;
      const char *rename_from = NULL;
      const char *rename_to = NULL;

      current_column = g_hash_table_lookup (current_by_field, next_column->field);
      if (current_column != NULL && g_hash_table_contains (used_current, current_column))
        current_column = NULL;

      if (current_column == NULL && next_column->property_name != NULL)
        {
          current_column = g_hash_table_lookup (current_by_property, next_column->property_name);
          if (current_column != NULL && g_hash_table_contains (used_current, current_column))
            current_column = NULL;
        }

      if (current_column != NULL)
        g_hash_table_add (used_current, current_column);

      if (current_column == NULL)
        {
          if (!gom_pgsql_driver_apply_column_add (executor,
                                                  runner,
                                                  current_table,
                                                  next_column,
                                                  error))
            return FALSE;
          continue;
        }

      if (g_strcmp0 (current_column->field, next_column->field) != 0)
        {
          rename_from = current_column->field;
          rename_to = next_column->field;
          if (!gom_pgsql_driver_apply_column_rename (executor,
                                                     runner,
                                                     current_table,
                                                     rename_from,
                                                     rename_to,
                                                     error))
            return FALSE;
        }

      if (!gom_pgsql_driver_column_def_is_compatible (current_column, next_column))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       "Cannot alter column '%s' in table '%s' in place; schema change is not supported",
                       next_column->field,
                       current_table);
          return FALSE;
        }

      if (current_column->nonnull != next_column->nonnull)
        {
          if (!gom_pgsql_driver_apply_column_nullability (executor,
                                                          runner,
                                                          current_table,
                                                          rename_to != NULL ? rename_to
                                                                            : current_column->field,
                                                          next_column->nonnull,
                                                          error))
            return FALSE;
        }
    }

  for (guint i = 0; i < current_columns->len; i++)
    {
      GomPgsqlColumnDef *current_column = g_ptr_array_index (current_columns, i);

      if (g_hash_table_contains (used_current, current_column))
        continue;

      if (!gom_pgsql_driver_apply_column_drop (executor,
                                               runner,
                                               current_table,
                                               current_column->field,
                                               error))
        return FALSE;
    }

  if (!gom_pgsql_driver_create_all_indexes_for_entity (executor, runner, next_entity, error))
    return FALSE;

  return TRUE;
}

static char **
gom_pgsql_parse_uri_list (GUri     *uri,
                          gboolean  keywords)
{
  g_autoptr(GPtrArray) names = NULL;
  g_autoptr(GPtrArray) values = NULL;
  const char *scheme;
  const char *host;
  const char *user;
  const char *password;
  const char *path;
  GHashTable *query_params;
  GHashTableIter iter;
  gpointer key;
  gpointer val;
  char **result;

  scheme = g_uri_get_scheme (uri);
  if (scheme == NULL ||
      (g_strcmp0 (scheme, "postgresql") != 0 &&
       g_strcmp0 (scheme, "postgres") != 0))
    return NULL;

  names = g_ptr_array_new_with_free_func (g_free);
  values = g_ptr_array_new_with_free_func (g_free);

  host = g_uri_get_host (uri);
  user = g_uri_get_user (uri);
  password = g_uri_get_password (uri);
  path = g_uri_get_path (uri);

  if (!gom_str_empty0 (host))
    {
      g_ptr_array_add (names, g_strdup ("host"));
      g_ptr_array_add (values, g_strdup (host));
    }
  if (g_uri_get_port (uri) > 0)
    {
      g_ptr_array_add (names, g_strdup ("port"));
      g_ptr_array_add (values, g_strdup_printf ("%u", g_uri_get_port (uri)));
    }
  if (!gom_str_empty0 (user))
    {
      g_ptr_array_add (names, g_strdup ("user"));
      g_ptr_array_add (values, g_strdup (user));
    }
  if (!gom_str_empty0 (password))
    {
      g_ptr_array_add (names, g_strdup ("password"));
      g_ptr_array_add (values, g_strdup (password));
    }
  if (path != NULL && path[0] == '/' && path[1] != '\0')
    {
      g_autofree char *dbname = g_uri_unescape_string (path + 1, NULL);
      g_ptr_array_add (names, g_strdup ("dbname"));
      g_ptr_array_add (values, g_steal_pointer (&dbname));
    }

  if (!gom_str_empty0 (g_uri_get_query (uri)))
    {
      if ((query_params = g_uri_parse_params (g_uri_get_query (uri), -1, "&", G_URI_PARAMS_NONE, NULL)))
        {
          g_hash_table_iter_init (&iter, query_params);
          while (g_hash_table_iter_next (&iter, &key, &val))
            {
              g_ptr_array_add (names, g_strdup (key));
              g_ptr_array_add (values, g_strdup (val));
            }
          g_hash_table_unref (query_params);
        }
    }

  g_ptr_array_add (names, NULL);
  g_ptr_array_add (values, NULL);

  if (keywords)
    {
      result = g_new0 (char *, names->len);
      for (guint i = 0; i < names->len; i++)
        result[i] = g_strdup (g_ptr_array_index (names, i));
    }
  else
    {
      result = g_new0 (char *, values->len);
      for (guint i = 0; i < values->len; i++)
        result[i] = g_strdup (g_ptr_array_index (values, i));
    }

  return result;
}

struct _GomPgsqlDriver
{
  GomDriver parent_instance;

  char  *uri;
  char **keywords;
  char **values;
  int    expand_dbname;
};

struct _GomPgsqlDriverClass
{
  GomDriverClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomPgsqlDriver, gom_pgsql_driver, GOM_TYPE_DRIVER)

static void
gom_pgsql_driver_finalize (GObject *object)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (object);

  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->keywords, g_strfreev);
  g_clear_pointer (&self->values, g_strfreev);

  G_OBJECT_CLASS (gom_pgsql_driver_parent_class)->finalize (object);
}

static void
gom_pgsql_driver_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (object);

  if (prop_id == 1)
    g_value_set_string (value, self->uri);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static char *
gom_pgsql_driver_dup_uri (GomDriver *driver)
{
  return g_strdup (GOM_PGSQL_DRIVER (driver)->uri);
}

DexFuture *
gom_pgsql_query_on_executor (GomRepository       *repository,
                             GomQuery            *query,
                             GomCursorFlags       flags,
                             gpointer             executor,
                             GomPgsqlQueryRunner  runner)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GString) sql = NULL;
  g_autoptr(GString) count_sql = NULL;
  g_autoptr(GPtrArray) bindings = NULL;
  g_autoptr(GPtrArray) count_bindings = NULL;
  g_autoptr(PgsqlParams) params = NULL;
  g_autoptr(PgsqlParams) count_params = NULL;
  g_autoptr(PgsqlResult) count_result = NULL;
  g_autoptr(PgsqlResult) result = NULL;
  g_autofree char *sql_to_run = NULL;
  g_autofree char *count_sql_to_run = NULL;
  const GomEntitySpec *entity = NULL;
  const char *relation;
  const char *base_relation;
  GomRegistry *registry;
  GomPgsqlExpressionContext context = { 0 };
  const GomPgsqlExpressionContext *context_ptr = NULL;
  guint64 count = 0;
  gboolean has_count = FALSE;

  registry = _gom_repository_get_registry (repository);
  relation = _gom_query_get_target_relation (query);
  base_relation = gom_pgsql_resolve_relation_name (registry,
                                                   _gom_query_get_target_entity_type (query),
                                                   relation,
                                                   "Query",
                                                   &entity,
                                                   &error);
  if (base_relation == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (entity != NULL)
    {
      context.entity = (GomEntitySpec *)entity;
      context_ptr = &context;
    }

  if ((flags & GOM_CURSOR_FLAGS_COUNT_ROWS) != 0)
    {
      if (!gom_pgsql_build_query_sql (query,
                                      base_relation,
                                      context_ptr,
                                      FALSE,
                                      FALSE,
                                      FALSE,
                                      &count_sql,
                                      &count_bindings,
                                      &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      sql = g_string_new ("SELECT COUNT(*) FROM (");
      g_string_append (sql, count_sql->str);
      g_string_append (sql, ") AS gom_count");

      count_sql_to_run = gom_pgsql_renumber_placeholders (sql->str);
      if (!(count_params = gom_pgsql_params_from_bindings (count_bindings, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(count_result = dex_await_object (runner (executor, count_sql_to_run, count_params), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (pgsql_result_get_n_rows (count_result) == 0)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Count query returned no rows");

      count = (guint64)g_ascii_strtoull (pgsql_result_get_value (count_result, 0, 0), NULL, 10);
      has_count = TRUE;
    }

  if (!gom_pgsql_build_query_sql (query,
                                  base_relation,
                                  context_ptr,
                                  TRUE,
                                  TRUE,
                                  TRUE,
                                  &sql,
                                  &bindings,
                                  &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  sql_to_run = gom_pgsql_renumber_placeholders (sql->str);
  if (!(params = gom_pgsql_params_from_bindings (bindings, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(result = dex_await_object (runner (executor, sql_to_run, params), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  {
    g_autoptr(GomPgsqlCursor) cursor = NULL;

    cursor = gom_pgsql_cursor_new (result, repository, count, has_count);
    return dex_future_new_take_object (g_steal_pointer (&cursor));
  }
}

typedef struct
{
  GomPgsqlDriver      *self;
  GomRepository       *repository;
  GomQuery            *query;
  GomCursorFlags       flags;
  PgsqlConnection     *connection;
  GomPgsqlQueryRunner  runner;
} GomPgsqlQueryRequest;

static void
gom_pgsql_query_request_free (gpointer data)
{
  GomPgsqlQueryRequest *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->repository);
  g_clear_object (&request->query);
  g_clear_object (&request->connection);
  g_free (request);
}

static DexFuture *
gom_pgsql_query_fiber (gpointer user_data)
{
  GomPgsqlQueryRequest *request = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlTransaction) transaction = NULL;
  g_autoptr(GomCursor) cursor = NULL;

  if ((request->flags & GOM_CURSOR_FLAGS_COUNT_ROWS) == 0)
    return gom_pgsql_query_on_executor (request->repository,
                                        request->query,
                                        request->flags,
                                        request->connection,
                                        request->runner);

  if (!(transaction = dex_await_object (pgsql_transaction_new (request->connection), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  cursor = dex_await_object (gom_pgsql_query_on_executor (request->repository,
                                                          request->query,
                                                          request->flags,
                                                          transaction,
                                                          (GomPgsqlQueryRunner)pgsql_transaction_query),
                             &error);
  if (cursor == NULL)
    {
      dex_future_disown (pgsql_transaction_rollback (transaction));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!dex_await (pgsql_transaction_commit (transaction), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&cursor));
}

static DexFuture *
gom_pgsql_query_connection_cb (DexFuture *completed,
                               gpointer   user_data)
{
  GomPgsqlQueryRequest *request = user_data;
  const GValue *value;

  if (!(value = dex_future_get_value (completed, NULL)))
    {
      gom_pgsql_query_request_free (request);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to open PostgreSQL connection");
    }

  request->connection = g_object_ref (g_value_get_object (value));
  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_query_fiber,
                              request,
                              gom_pgsql_query_request_free);
}

static DexFuture *
gom_pgsql_query (GomDriver      *driver,
                 GomRepository  *repository,
                 GomQuery       *query,
                 GomCursorFlags  flags)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  GomPgsqlQueryRequest *request;

  request = g_new0 (GomPgsqlQueryRequest, 1);
  request->self = g_object_ref (self);
  request->repository = g_object_ref (repository);
  request->query = g_object_ref (query);
  request->flags = flags;
  request->runner = (GomPgsqlQueryRunner)pgsql_connection_query;

  return dex_future_then (pgsql_connection_new ((const char * const *)self->keywords,
                                                (const char * const *)self->values,
                                                self->expand_dbname),
                          gom_pgsql_query_connection_cb,
                          request,
                          NULL);
}

static DexFuture *
gom_pgsql_execute_sql_on_connection (GomPgsqlDriver  *self,
                                     PgsqlConnection *connection,
                                     GBytes          *script)
{
  g_autoptr(GError) error = NULL;
  gsize script_len = 0;
  const guint8 *script_data;
  g_autofree char *sql = NULL;
  g_autoptr(PgsqlResult) result = NULL;

  script_data = g_bytes_get_data (script, &script_len);
  if (script_data == NULL || script_len == 0)
    return dex_future_new_true ();

  if (memchr (script_data, '\0', script_len) != NULL)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "SQL script contains NUL byte");

  sql = g_strndup ((const char *)script_data, script_len);
  if (!(result = dex_await_object (pgsql_connection_query (connection, sql, NULL), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

typedef struct
{
  GomPgsqlDriver  *self;
  GBytes          *script;
  PgsqlConnection *connection;
} GomPgsqlExecuteSqlRequest;

static void
gom_pgsql_execute_sql_request_free (gpointer data)
{
  GomPgsqlExecuteSqlRequest *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->connection);
  g_clear_pointer (&request->script, g_bytes_unref);
  g_free (request);
}

static DexFuture *
gom_pgsql_execute_sql_fiber (gpointer user_data)
{
  GomPgsqlExecuteSqlRequest *request = user_data;

  return gom_pgsql_execute_sql_on_connection (request->self, request->connection, request->script);
}

static DexFuture *
gom_pgsql_execute_sql_connection_cb (DexFuture *completed,
                                     gpointer   user_data)
{
  struct
  {
    GomPgsqlDriver *self;
    GBytes *script;
    PgsqlConnection *connection;
  } *request = user_data;
  const GValue *value;

  if (!(value = dex_future_get_value (completed, NULL)))
    {
      gom_pgsql_execute_sql_request_free (request);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to open PostgreSQL connection");
    }

  request->connection = g_object_ref (g_value_get_object (value));
  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_execute_sql_fiber,
                              request,
                              gom_pgsql_execute_sql_request_free);
}

static DexFuture *
gom_pgsql_execute_sql (GomDriver *driver,
                       GBytes    *script)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  GomPgsqlExecuteSqlRequest *request;

  request = g_new0 (GomPgsqlExecuteSqlRequest, 1);
  request->self = g_object_ref (self);
  request->script = g_bytes_ref (script);

  return dex_future_then (pgsql_connection_new ((const char * const *)self->keywords,
                                                (const char * const *)self->values,
                                                self->expand_dbname),
                          gom_pgsql_execute_sql_connection_cb,
                          request,
                          NULL);
}

static DexFuture *
gom_pgsql_query_version_on_connection (PgsqlConnection *connection)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlResult) result = NULL;

  result = dex_await_object (pgsql_connection_query (connection,
                                                     "SELECT version FROM gom_schema_version LIMIT 1",
                                                     NULL),
                             &error);
  if (result == NULL)
    return dex_future_new_for_uint (0);

  if (pgsql_result_get_n_rows (result) == 0)
    return dex_future_new_for_uint (0);

  return dex_future_new_for_uint ((guint) g_ascii_strtoull (pgsql_result_get_value (result, 0, 0), NULL, 10));
}

typedef struct
{
  GomPgsqlDriver  *self;
  PgsqlConnection *connection;
} GomPgsqlQueryVersionRequest;

static void
gom_pgsql_query_version_request_free (gpointer data)
{
  GomPgsqlQueryVersionRequest *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->connection);
  g_free (request);
}

static DexFuture *
gom_pgsql_query_version_fiber (gpointer user_data)
{
  GomPgsqlQueryVersionRequest *request = user_data;

  return gom_pgsql_query_version_on_connection (request->connection);
}

static DexFuture *
gom_pgsql_query_version_cb (DexFuture *completed,
                            gpointer   user_data)
{
  GomPgsqlQueryVersionRequest *request = user_data;
  const GValue *value;

  if (!(value = dex_future_get_value (completed, NULL)))
    {
      gom_pgsql_query_version_request_free (request);
      return dex_future_new_for_uint (0);
    }

  request->connection = g_object_ref (g_value_get_object (value));
  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_query_version_fiber,
                              request,
                              gom_pgsql_query_version_request_free);
}

static DexFuture *
gom_pgsql_query_version (GomDriver *driver)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  GomPgsqlQueryVersionRequest *request;

  request = g_new0 (GomPgsqlQueryVersionRequest, 1);
  request->self = g_object_ref (self);

  return dex_future_then (pgsql_connection_new ((const char * const *)self->keywords,
                                                (const char * const *)self->values,
                                                self->expand_dbname),
                          gom_pgsql_query_version_cb,
                          request,
                          NULL);
}

static char **
gom_pgsql_parse_index_fields (const char *indexdef)
{
  const char *start;
  const char *end;
  g_autoptr(GPtrArray) fields = NULL;
  g_autoptr(GString) current = NULL;

  if (indexdef == NULL)
    return g_new0 (char *, 1);

  start = strrchr (indexdef, '(');
  end = strrchr (indexdef, ')');
  if (start == NULL || end == NULL || end <= start)
    return g_new0 (char *, 1);

  fields = g_ptr_array_new_with_free_func (g_free);
  current = g_string_new (NULL);

  for (const char *iter = start + 1; iter < end; iter++)
    {
      if (*iter == ',')
        {
          g_strstrip (current->str);
          if (current->len > 0)
            g_ptr_array_add (fields, g_string_free (g_steal_pointer (&current), FALSE));
          current = g_string_new (NULL);
          continue;
        }

      if (*iter != ' ')
        g_string_append_c (current, *iter);
    }

  g_strstrip (current->str);
  if (current->len > 0)
    g_ptr_array_add (fields, g_string_free (g_steal_pointer (&current), FALSE));

  g_ptr_array_add (fields, NULL);
  return (char **)g_ptr_array_free (g_steal_pointer (&fields), FALSE);
}

static DexFuture *
gom_pgsql_list_relations_on_connection (PgsqlConnection *connection)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlResult) result = NULL;
  g_autoptr(GPtrArray) relations = NULL;
  char **strv;

  relations = g_ptr_array_new_with_free_func (g_free);

  result = dex_await_object (pgsql_connection_query (connection,
                                                     "SELECT table_name FROM information_schema.tables "
                                                     "WHERE table_schema NOT IN ('pg_catalog', 'information_schema') "
                                                     "AND table_type IN ('BASE TABLE', 'VIEW') "
                                                     "ORDER BY table_name",
                                                     NULL),
                             &error);
  if (result == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < pgsql_result_get_n_rows (result); i++)
    {
      const char *name = pgsql_result_get_value (result, i, 0);
      if (name != NULL)
        g_ptr_array_add (relations, g_strdup (name));
    }

  strv = g_new0 (char *, relations->len + 1);
  for (guint i = 0; i < relations->len; i++)
    strv[i] = g_strdup (g_ptr_array_index (relations, i));

  return dex_future_new_take_boxed (G_TYPE_STRV, strv);
}

typedef struct
{
  GomPgsqlDriver  *self;
  PgsqlConnection *connection;
} GomPgsqlListRelationsRequest;

static void
gom_pgsql_list_relations_request_free (gpointer data)
{
  GomPgsqlListRelationsRequest *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->connection);
  g_free (request);
}

static DexFuture *
gom_pgsql_list_relations_fiber (gpointer user_data)
{
  GomPgsqlListRelationsRequest *request = user_data;

  return gom_pgsql_list_relations_on_connection (request->connection);
}

static DexFuture *
gom_pgsql_list_relations_cb (DexFuture *completed,
                             gpointer   user_data)
{
  GomPgsqlListRelationsRequest *request = user_data;
  const GValue *value;

  if (!(value = dex_future_get_value (completed, NULL)))
    {
      gom_pgsql_list_relations_request_free (request);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to open PostgreSQL connection");
    }

  request->connection = g_object_ref (g_value_get_object (value));
  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_list_relations_fiber,
                              request,
                              gom_pgsql_list_relations_request_free);
}

static DexFuture *
gom_pgsql_list_relations (GomDriver   *driver,
                          GomRegistry *registry)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  GomPgsqlListRelationsRequest *request;

  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);

  request = g_new0 (GomPgsqlListRelationsRequest, 1);
  request->self = g_object_ref (self);

  return dex_future_then (pgsql_connection_new ((const char * const *)self->keywords,
                                                (const char * const *)self->values,
                                                self->expand_dbname),
                          gom_pgsql_list_relations_cb,
                          request,
                          NULL);
}

static DexFuture *
gom_pgsql_describe_relation_on_connection (PgsqlConnection *connection,
                                           GomRegistry     *registry,
                                           const char      *relation)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlResult) columns = NULL;
  g_autoptr(PgsqlResult) primary_keys = NULL;
  g_autoptr(PgsqlResult) indexes = NULL;
  g_autoptr(GListStore) fields = NULL;
  g_autoptr(GListStore) index_store = NULL;
  const GomEntitySpec *entity = NULL;
  const char *table = relation;
  g_autoptr(GHashTable) pk_names = NULL;

  if (!(entity = _gom_registry_lookup_entity_by_table (registry, table)))
    if ((entity = _gom_registry_lookup_entity_by_name (registry, table)))
    {
      const char *mapped = gom_entity_spec_get_table ((GomEntitySpec *)entity);
      if (!gom_str_empty0 (mapped))
        table = mapped;
    }

  fields = g_list_store_new (GOM_TYPE_FIELD_SCHEMA);
  index_store = g_list_store_new (GOM_TYPE_INDEX_SCHEMA);
  pk_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  columns = dex_await_object (pgsql_connection_query (connection,
                                                      "SELECT column_name, data_type, is_nullable, column_default "
                                                      "FROM information_schema.columns "
                                                      "WHERE table_schema = current_schema() AND table_name = $1 "
                                                      "ORDER BY ordinal_position",
                                                      ({
                                                        g_autoptr(PgsqlParams) params = pgsql_params_new ();
                                                        pgsql_params_add_text (params, table);
                                                        g_steal_pointer (&params);
                                                      })),
                              &error);
  if (columns == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  primary_keys = dex_await_object (pgsql_connection_query (connection,
                                                           "SELECT kcu.column_name "
                                                           "FROM information_schema.table_constraints tc "
                                                           "JOIN information_schema.key_column_usage kcu "
                                                           "ON tc.constraint_name = kcu.constraint_name "
                                                           "AND tc.table_schema = kcu.table_schema "
                                                           "WHERE tc.table_schema = current_schema() "
                                                           "AND tc.table_name = $1 "
                                                           "AND tc.constraint_type = 'PRIMARY KEY' "
                                                           "ORDER BY kcu.ordinal_position",
                                                           ({
                                                             g_autoptr(PgsqlParams) params = pgsql_params_new ();
                                                             pgsql_params_add_text (params, table);
                                                             g_steal_pointer (&params);
                                                           })),
                                 &error);
  if (primary_keys == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < pgsql_result_get_n_rows (primary_keys); i++)
    {
      const char *name = pgsql_result_get_value (primary_keys, i, 0);
      if (name != NULL)
        g_hash_table_add (pk_names, g_strdup (name));
    }

  for (guint i = 0; i < pgsql_result_get_n_rows (columns); i++)
    {
      const char *name = pgsql_result_get_value (columns, i, 0);
      const char *data_type = pgsql_result_get_value (columns, i, 1);
      const char *is_nullable = pgsql_result_get_value (columns, i, 2);
      const char *default_value = pgsql_result_get_value (columns, i, 3);

      if (name != NULL)
        g_list_store_append (fields,
                             _gom_field_schema_new (name,
                                                    data_type,
                                                    g_strcmp0 (is_nullable, "NO") == 0,
                                                    g_hash_table_contains (pk_names, name),
                                                    default_value));
    }

  indexes = dex_await_object (pgsql_connection_query (connection,
                                                      "SELECT indexname, indexdef "
                                                      "FROM pg_indexes "
                                                      "WHERE schemaname = current_schema() AND tablename = $1 "
                                                      "ORDER BY indexname",
                                                      ({
                                                        g_autoptr(PgsqlParams) params = pgsql_params_new ();
                                                        pgsql_params_add_text (params, table);
                                                        g_steal_pointer (&params);
                                                      })),
                              &error);
  if (indexes == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (guint i = 0; i < pgsql_result_get_n_rows (indexes); i++)
    {
      const char *indexname = pgsql_result_get_value (indexes, i, 0);
      const char *indexdef = pgsql_result_get_value (indexes, i, 1);
      gboolean unique = indexdef != NULL && g_str_has_prefix (indexdef, "CREATE UNIQUE INDEX");
      g_auto(GStrv) index_fields = gom_pgsql_parse_index_fields (indexdef);

      if (indexname != NULL)
        g_list_store_append (index_store,
                             _gom_index_schema_new (indexname,
                                                    unique,
                                                    (const char * const *) index_fields));
    }

  return dex_future_new_take_object (_gom_relation_schema_new (table, G_LIST_MODEL (fields), G_LIST_MODEL (index_store)));
}

typedef struct
{
  GomPgsqlDriver  *self;
  GomRegistry     *registry;
  char            *relation;
  PgsqlConnection *connection;
} GomPgsqlDescribeRelationRequest;

static void
gom_pgsql_describe_relation_request_free (gpointer data)
{
  GomPgsqlDescribeRelationRequest *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->registry);
  g_clear_object (&request->connection);
  g_clear_pointer (&request->relation, g_free);
  g_free (request);
}

static DexFuture *
gom_pgsql_describe_relation_fiber (gpointer user_data)
{
  GomPgsqlDescribeRelationRequest *request = user_data;

  return gom_pgsql_describe_relation_on_connection (request->connection,
                                                    request->registry,
                                                    request->relation);
}

static DexFuture *
gom_pgsql_describe_relation_cb (DexFuture *completed,
                                gpointer   user_data)
{
  GomPgsqlDescribeRelationRequest *request = user_data;
  const GValue *value;

  if (!(value = dex_future_get_value (completed, NULL)))
    {
      gom_pgsql_describe_relation_request_free (request);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to open PostgreSQL connection");
    }

  request->connection = g_object_ref (g_value_get_object (value));
  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_describe_relation_fiber,
                              request,
                              gom_pgsql_describe_relation_request_free);
}

static DexFuture *
gom_pgsql_describe_relation (GomDriver   *driver,
                             GomRegistry *registry,
                             const char  *relation)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  GomPgsqlDescribeRelationRequest *request;

  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (relation != NULL, NULL);

  request = g_new0 (GomPgsqlDescribeRelationRequest, 1);
  request->self = g_object_ref (self);
  request->registry = g_object_ref (registry);
  request->relation = g_strdup (relation);

  return dex_future_then (pgsql_connection_new ((const char * const *)self->keywords,
                                                (const char * const *)self->values,
                                                self->expand_dbname),
                          gom_pgsql_describe_relation_cb,
                          request,
                          NULL);
}

DexFuture *
gom_pgsql_mutate_on_executor (GomRegistry         *registry,
                              GomMutation         *mutation,
                              gpointer             executor,
                              GomPgsqlQueryRunner  runner)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(PgsqlResult) pgresult = NULL;

  result = _gom_mutation_result_new ();

  if (GOM_IS_INSERTION (mutation))
    {
      GomInsertion *insertion = GOM_INSERTION (mutation);
      GPtrArray *columns = _gom_insertion_get_columns (insertion);
      GPtrArray *rows = _gom_insertion_get_rows (insertion);
      GType entity_type = _gom_insertion_get_target_entity_type (insertion);
      const GomEntitySpec *entity = NULL;
      const char *relation = _gom_insertion_get_target_relation (insertion);
      const char *base_relation;
      GomPgsqlExpressionContext context = { 0 };
      const GomPgsqlExpressionContext *context_ptr = NULL;

      base_relation = gom_pgsql_resolve_relation_name (registry,
                                                       entity_type,
                                                       relation,
                                                       "Insertion",
                                                       &entity,
                                                       &error);
      if (base_relation == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (entity != NULL)
        {
          context.entity = (GomEntitySpec *)entity;
          context_ptr = &context;
        }

      if (columns == NULL || rows == NULL || columns->len == 0 || rows->len == 0)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_ARGUMENT,
                                      "Insertion requires columns and rows");

      for (guint i = 0; i < rows->len; i++)
        {
          GPtrArray *row = g_ptr_array_index (rows, i);
          g_autoptr(GString) sql = NULL;
          g_autoptr(GPtrArray) bindings = NULL;
          g_autoptr(PgsqlParams) params = NULL;
          g_autofree char *sql_to_run = NULL;

          if (row->len != columns->len)
            return dex_future_new_reject (G_IO_ERROR,
                                          G_IO_ERROR_INVALID_ARGUMENT,
                                          "Insertion row length mismatch");

          sql = g_string_new ("INSERT INTO ");
          gom_pgsql_append_quoted_identifier_path (sql, base_relation);
          g_string_append (sql, " (");
          bindings = g_ptr_array_new_with_free_func (gom_pgsql_binding_free);
          if (!gom_pgsql_append_expression_list_with_context (columns,
                                                              sql,
                                                              bindings,
                                                              &error,
                                                              context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
          g_string_append (sql, ") VALUES (");
          if (!gom_pgsql_append_expression_list_with_context (row,
                                                              sql,
                                                              bindings,
                                                              &error,
                                                              context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
          g_string_append (sql, ") RETURNING *");

          sql_to_run = gom_pgsql_renumber_placeholders (sql->str);
          if (!(params = gom_pgsql_params_from_bindings (bindings, &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          if (!(pgresult = dex_await_object (runner (executor, sql_to_run, params), &error)))
            return dex_future_new_for_error (g_steal_pointer (&error));

          {
            g_autoptr(GomMutationResult) one_result = NULL;
            g_autoptr(GomRecord) appended_record = NULL;
            one_result = g_object_new (GOM_TYPE_MUTATION_RESULT, NULL);
            for (guint j = 0; j < pgsql_result_get_n_rows (pgresult); j++)
              {
                guint n_fields = pgsql_result_get_n_fields (pgresult);
                g_autofree const char **column_names = g_new0 (const char *, n_fields);
                g_autofree GValue *values = g_new0 (GValue, n_fields);
                g_autoptr(GomRecord) record = NULL;

                for (guint k = 0; k < n_fields; k++)
                  {
                    column_names[k] = pgsql_result_get_field_name (pgresult, k);
                    if (!gom_pgsql_cursor_set_value (pgresult, j, k, &values[k]))
                      return dex_future_new_reject (G_IO_ERROR,
                                                    G_IO_ERROR_INVALID_DATA,
                                                    "Failed to convert insert result");
                  }

                record = _gom_record_new_from_values (column_names, values, n_fields);
                _gom_mutation_result_append_record (one_result, record, 1);

                for (guint k = 0; k < n_fields; k++)
                  if (G_IS_VALUE (&values[k]))
                    g_value_unset (&values[k]);
              }

            appended_record = g_list_model_get_item (G_LIST_MODEL (one_result), 0);
            _gom_mutation_result_append_record (result, appended_record, 1);
          }
        }

      return dex_future_new_take_object (g_steal_pointer (&result));
    }

  if (GOM_IS_UPDATE (mutation))
    {
      GomUpdate *update = GOM_UPDATE (mutation);
      GPtrArray *columns = _gom_update_get_columns (update);
      GPtrArray *values = _gom_update_get_values (update);
      GomExpression *filter = _gom_update_get_filter (update);
      GType entity_type = _gom_update_get_target_entity_type (update);
      const GomEntitySpec *entity = NULL;
      const char *relation = _gom_update_get_target_relation (update);
      const char *base_relation;
      GomPgsqlExpressionContext context = { 0 };
      const GomPgsqlExpressionContext *context_ptr = NULL;
      g_autoptr(GString) sql = NULL;
      g_autoptr(GPtrArray) bindings = NULL;
      g_autoptr(PgsqlParams) params = NULL;
      g_autofree char *sql_to_run = NULL;
      gboolean has_limit;
      guint64 limit = 0;

      base_relation = gom_pgsql_resolve_relation_name (registry,
                                                       entity_type,
                                                       relation,
                                                       "Update",
                                                       &entity,
                                                       &error);
      if (base_relation == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (entity != NULL)
        {
          context.entity = (GomEntitySpec *)entity;
          context_ptr = &context;
        }

      if (columns == NULL || values == NULL || columns->len == 0 || columns->len != values->len)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_INVALID_ARGUMENT,
                                      "Update requires assignments");

      has_limit = _gom_update_has_limit (update);
      if (has_limit)
        limit = _gom_update_get_limit (update);

      sql = g_string_new ("UPDATE ");
      gom_pgsql_append_quoted_identifier_path (sql, base_relation);
      g_string_append (sql, " SET ");
      bindings = g_ptr_array_new_with_free_func (gom_pgsql_binding_free);

      for (guint i = 0; i < columns->len; i++)
        {
          if (i > 0)
            g_string_append (sql, ", ");

          if (!gom_pgsql_append_expression_with_context (g_ptr_array_index (columns, i),
                                                         sql,
                                                         bindings,
                                                         &error,
                                                         context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));

          g_string_append (sql, " = ");

          if (!gom_pgsql_append_expression_with_context (g_ptr_array_index (values, i),
                                                         sql,
                                                         bindings,
                                                         &error,
                                                         context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }

      if (has_limit)
        {
          g_string_append (sql, " WHERE ctid IN (SELECT ctid FROM ");
          gom_pgsql_append_quoted_identifier_path (sql, base_relation);

          if (filter != NULL)
            {
              g_string_append (sql, " WHERE ");
              if (!gom_pgsql_append_expression_with_context (filter,
                                                             sql,
                                                             bindings,
                                                             &error,
                                                             context_ptr))
                return dex_future_new_for_error (g_steal_pointer (&error));
            }

          g_string_append (sql, " LIMIT ?)");

          if (!gom_pgsql_append_uint64_binding (bindings, limit, &error))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }
      else if (filter != NULL)
        {
          g_string_append (sql, " WHERE ");
          if (!gom_pgsql_append_expression_with_context (filter,
                                                         sql,
                                                         bindings,
                                                         &error,
                                                         context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }

      g_string_append (sql, " RETURNING *");
      sql_to_run = gom_pgsql_renumber_placeholders (sql->str);
      if (!(params = gom_pgsql_params_from_bindings (bindings, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(pgresult = dex_await_object (runner (executor, sql_to_run, params), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      return gom_pgsql_result_to_mutation_result (pgresult);
    }

  if (GOM_IS_DELETION (mutation))
    {
      GomDeletion *deletion = GOM_DELETION (mutation);
      GomExpression *filter = _gom_deletion_get_filter (deletion);
      GType entity_type = _gom_deletion_get_target_entity_type (deletion);
      const GomEntitySpec *entity = NULL;
      const char *relation = _gom_deletion_get_target_relation (deletion);
      const char *base_relation;
      GomPgsqlExpressionContext context = { 0 };
      const GomPgsqlExpressionContext *context_ptr = NULL;
      g_autoptr(GString) sql = NULL;
      g_autoptr(GPtrArray) bindings = NULL;
      g_autoptr(PgsqlParams) params = NULL;
      g_autofree char *sql_to_run = NULL;
      gboolean has_limit;
      guint64 limit = 0;

      base_relation = gom_pgsql_resolve_relation_name (registry,
                                                       entity_type,
                                                       relation,
                                                       "Deletion",
                                                       &entity,
                                                       &error);
      if (base_relation == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (entity != NULL)
        {
          context.entity = (GomEntitySpec *)entity;
          context_ptr = &context;
        }

      sql = g_string_new ("DELETE FROM ");
      gom_pgsql_append_quoted_identifier_path (sql, base_relation);
      bindings = g_ptr_array_new_with_free_func (gom_pgsql_binding_free);

      has_limit = _gom_deletion_has_limit (deletion);
      if (has_limit)
        limit = _gom_deletion_get_limit (deletion);

      if (has_limit)
        {
          g_string_append (sql, " WHERE ctid IN (SELECT ctid FROM ");
          gom_pgsql_append_quoted_identifier_path (sql, base_relation);

          if (filter != NULL)
            {
              g_string_append (sql, " WHERE ");
              if (!gom_pgsql_append_expression_with_context (filter,
                                                             sql,
                                                             bindings,
                                                             &error,
                                                             context_ptr))
                return dex_future_new_for_error (g_steal_pointer (&error));
            }

          g_string_append (sql, " LIMIT ?)");

          if (!gom_pgsql_append_uint64_binding (bindings, limit, &error))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }
      else if (filter != NULL)
        {
          g_string_append (sql, " WHERE ");
          if (!gom_pgsql_append_expression_with_context (filter,
                                                         sql,
                                                         bindings,
                                                         &error,
                                                         context_ptr))
            return dex_future_new_for_error (g_steal_pointer (&error));
        }

      g_string_append (sql, " RETURNING *");
      sql_to_run = gom_pgsql_renumber_placeholders (sql->str);
      if (!(params = gom_pgsql_params_from_bindings (bindings, &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (!(pgresult = dex_await_object (runner (executor, sql_to_run, params), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      return gom_pgsql_result_to_mutation_result (pgresult);
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Unsupported mutation type");
}

typedef struct
{
  GomPgsqlDriver      *self;
  GomRegistry         *registry;
  GomMutation         *mutation;
  PgsqlConnection     *connection;
  GomPgsqlQueryRunner  runner;
} GomPgsqlMutateRequest;

static void
gom_pgsql_mutate_request_free (gpointer data)
{
  GomPgsqlMutateRequest *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->registry);
  g_clear_object (&request->mutation);
  g_clear_object (&request->connection);
  g_free (request);
}

static DexFuture *
gom_pgsql_mutate_fiber (gpointer user_data)
{
  GomPgsqlMutateRequest *request = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlTransaction) transaction = NULL;
  g_autoptr(GomMutationResult) result = NULL;

  if (!(transaction = dex_await_object (pgsql_transaction_new (request->connection), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  result = dex_await_object (gom_pgsql_mutate_on_executor (request->registry,
                                                           request->mutation,
                                                           transaction,
                                                           (GomPgsqlQueryRunner)pgsql_transaction_query),
                             &error);
  if (result == NULL)
    {
      dex_future_disown (pgsql_transaction_rollback (transaction));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  if (!dex_await (pgsql_transaction_commit (transaction), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&result));
}

static DexFuture *
gom_pgsql_mutate_connection_cb (DexFuture *completed,
                                gpointer   user_data)
{
  GomPgsqlMutateRequest *request = user_data;
  const GValue *value;

  if (!(value = dex_future_get_value (completed, NULL)))
    {
      gom_pgsql_mutate_request_free (request);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to open PostgreSQL connection");
    }

  request->connection = g_object_ref (g_value_get_object (value));
  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_mutate_fiber,
                              request,
                              gom_pgsql_mutate_request_free);
}

static DexFuture *
gom_pgsql_mutate (GomDriver   *driver,
                  GomRegistry *registry,
                  GomMutation *mutation)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  GomPgsqlMutateRequest *request;

  g_return_val_if_fail (GOM_IS_REGISTRY (registry), NULL);
  g_return_val_if_fail (GOM_IS_MUTATION (mutation), NULL);

  request = g_new0 (GomPgsqlMutateRequest, 1);
  request->self = g_object_ref (self);
  request->registry = g_object_ref (registry);
  request->mutation = g_object_ref (mutation);
  request->runner = (GomPgsqlQueryRunner) pgsql_connection_query;

  return dex_future_then (pgsql_connection_new ((const char * const *) self->keywords,
                                                (const char * const *) self->values,
                                                self->expand_dbname),
                          gom_pgsql_mutate_connection_cb,
                          request,
                          NULL);
}

static DexFuture *
gom_pgsql_migrate_fiber (gpointer user_data)
{
  struct
  {
    GomPgsqlDriver *self;
    GomRegistry *current;
    GomRegistry *next;
  } *request = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlConnection) connection = NULL;
  g_autoptr(PgsqlTransaction) transaction = NULL;
  g_autoptr(GomRegistryDiff) diff = NULL;
  const GPtrArray *dropped_entities;
  const GPtrArray *added_entities;
  const GPtrArray *changed_entities;

  connection = dex_await_object (pgsql_connection_new ((const char * const *) request->self->keywords,
                                                       (const char * const *) request->self->values,
                                                       request->self->expand_dbname),
                                 &error);
  if (connection == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(transaction = dex_await_object (pgsql_transaction_new (connection), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_pgsql_driver_exec_sql (transaction,
                                  (GomPgsqlQueryRunner) pgsql_transaction_query,
                                  "CREATE TABLE IF NOT EXISTS gom_schema_version (version integer NOT NULL)",
                                  &error))
    {
      dex_future_disown (pgsql_transaction_rollback (transaction));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  diff = _gom_registry_diff_new (request->current, request->next);
  dropped_entities = _gom_registry_diff_get_dropped_entities (diff);
  added_entities = _gom_registry_diff_get_added_entities (diff);
  changed_entities = _gom_registry_diff_get_changed_entities (diff);

  for (guint i = 0; i < dropped_entities->len; i++)
    {
      GomEntitySpec *entity = g_ptr_array_index ((GPtrArray *)dropped_entities, i);
      const char *table = gom_entity_spec_get_table (entity);
      g_autoptr(GString) drop_sql = NULL;

      if (table == NULL || *table == '\0')
        table = gom_entity_spec_get_name (entity);

      drop_sql = g_string_new ("DROP TABLE IF EXISTS ");
      gom_pgsql_append_quoted_identifier_path (drop_sql, table);
      if (!gom_pgsql_driver_exec_sql (transaction,
                                      (GomPgsqlQueryRunner)pgsql_transaction_query,
                                      drop_sql->str,
                                      &error))
        {
          dex_future_disown (pgsql_transaction_rollback (transaction));
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  for (guint i = 0; i < added_entities->len; i++)
    {
      GomEntitySpec *entity = g_ptr_array_index ((GPtrArray *)added_entities, i);

      if (!gom_pgsql_driver_apply_entity (transaction,
                                          (GomPgsqlQueryRunner)pgsql_transaction_query,
                                          entity,
                                          FALSE,
                                          &error))
        {
          dex_future_disown (pgsql_transaction_rollback (transaction));
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  for (guint i = 0; i < changed_entities->len; i++)
    {
      GomEntitySpec *entity = _gom_entity_diff_get_next_entity (g_ptr_array_index ((GPtrArray *) changed_entities, i));

      if (!gom_pgsql_driver_migrate_changed_entity (transaction,
                                                    (GomPgsqlQueryRunner) pgsql_transaction_query,
                                                    _gom_entity_diff_get_current_entity (g_ptr_array_index ((GPtrArray *) changed_entities, i)),
                                                    entity,
                                                    &error))
        {
          dex_future_disown (pgsql_transaction_rollback (transaction));
          return dex_future_new_for_error (g_steal_pointer (&error));
        }
    }

  if (!gom_pgsql_driver_exec_sql (transaction,
                                  (GomPgsqlQueryRunner)pgsql_transaction_query,
                                  "DELETE FROM gom_schema_version",
                                  &error))
    {
      dex_future_disown (pgsql_transaction_rollback (transaction));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  {
    g_autofree char *version_sql = g_strdup_printf ("INSERT INTO gom_schema_version (version) VALUES (%u)",
                                                    gom_registry_get_version (request->next));

    if (!gom_pgsql_driver_exec_sql (transaction,
                                    (GomPgsqlQueryRunner)pgsql_transaction_query,
                                    version_sql,
                                    &error))
      {
        dex_future_disown (pgsql_transaction_rollback (transaction));
        return dex_future_new_for_error (g_steal_pointer (&error));
      }
  }

  if (!dex_await (pgsql_transaction_commit (transaction), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static void
gom_pgsql_migrate_request_free (gpointer data)
{
  struct
  {
    GomPgsqlDriver *self;
    GomRegistry *current;
    GomRegistry *next;
  } *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->current);
  g_clear_object (&request->next);
  g_free (request);
}

static DexFuture *
gom_pgsql_migrate (GomDriver   *driver,
                   GomRegistry *current,
                   GomRegistry *next)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  struct
  {
    GomPgsqlDriver *self;
    GomRegistry *current;
    GomRegistry *next;
  } *request;

  g_return_val_if_fail (GOM_IS_REGISTRY (current), NULL);
  g_return_val_if_fail (GOM_IS_REGISTRY (next), NULL);

  request = g_new0 (typeof (*request), 1);
  request->self = g_object_ref (self);
  request->current = g_object_ref (current);
  request->next = g_object_ref (next);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_migrate_fiber,
                              request,
                              gom_pgsql_migrate_request_free);
}

static DexFuture *
gom_pgsql_begin_session_fiber (gpointer user_data)
{
  struct
  {
    GomPgsqlDriver *self;
    GomRepository *repository;
  } *request = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(PgsqlConnectionPool) pool = NULL;
  g_autoptr(PgsqlConnection) connection = NULL;
  g_autoptr(PgsqlTransaction) transaction = NULL;

  pool = dex_await_object (pgsql_connection_pool_new ((const char * const *)request->self->keywords,
                                                      (const char * const *)request->self->values,
                                                      request->self->expand_dbname,
                                                      1,
                                                      1),
                           &error);
  if (pool == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(connection = dex_await_object (pgsql_connection_pool_acquire (pool), &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!(transaction = dex_await_object (pgsql_transaction_new (connection), &error)))
    {
      pgsql_connection_pool_release (pool, g_steal_pointer (&connection));
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  return dex_future_new_take_object (gom_pgsql_session_new (request->repository,
                                                            g_steal_pointer (&pool),
                                                            g_steal_pointer (&connection),
                                                            g_steal_pointer (&transaction)));
}

static void
gom_pgsql_begin_session_request_free (gpointer data)
{
  struct
  {
    GomPgsqlDriver *self;
    GomRepository *repository;
  } *request = data;

  g_clear_object (&request->self);
  g_clear_object (&request->repository);
  g_free (request);
}

static DexFuture *
gom_pgsql_begin_session (GomDriver     *driver,
                         GomRepository *repository)
{
  GomPgsqlDriver *self = GOM_PGSQL_DRIVER (driver);
  struct
  {
    GomPgsqlDriver *self;
    GomRepository *repository;
  } *request;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);

  request = g_new0 (typeof (*request), 1);
  request->self = g_object_ref (self);
  request->repository = g_object_ref (repository);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_begin_session_fiber,
                              request,
                              gom_pgsql_begin_session_request_free);
}

static void
gom_pgsql_driver_class_init (GomPgsqlDriverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomDriverClass *driver_class = GOM_DRIVER_CLASS (klass);

  object_class->finalize = gom_pgsql_driver_finalize;
  object_class->get_property = gom_pgsql_driver_get_property;

  driver_class->dup_uri = gom_pgsql_driver_dup_uri;
  driver_class->query = gom_pgsql_query;
  driver_class->mutate = gom_pgsql_mutate;
  driver_class->describe_relation = gom_pgsql_describe_relation;
  driver_class->list_relations = gom_pgsql_list_relations;
  driver_class->query_version = gom_pgsql_query_version;
  driver_class->migrate = gom_pgsql_migrate;
  driver_class->execute_sql = gom_pgsql_execute_sql;
  driver_class->begin_session = gom_pgsql_begin_session;
}

static void
gom_pgsql_driver_init (GomPgsqlDriver *self)
{
  self->expand_dbname = 1;
}

G_MODULE_EXPORT GomDriver *
_gom_pgsql_driver_new (const char        *uri,
                       GomDriverOptions  *options,
                       GError           **error)
{
  g_autoptr(GUri) guri = NULL;
  GomPgsqlDriver *self;

  if (uri == NULL || !(guri = g_uri_parse (uri, G_URI_FLAGS_PARSE_RELAXED, error)))
    return NULL;

  self = g_object_new (GOM_TYPE_PGSQL_DRIVER, NULL);
  self->uri = g_strdup (uri);
  self->keywords = gom_pgsql_parse_uri_list (guri, TRUE);
  self->values = gom_pgsql_parse_uri_list (guri, FALSE);

  if (self->keywords == NULL || self->values == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Unsupported PostgreSQL URI: %s",
                   uri);
      g_clear_object (&self);
      return NULL;
    }

  return GOM_DRIVER (self);
}
