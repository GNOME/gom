/* gom-expression.h
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

#pragma once

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_EXPRESSION (gom_expression_get_type())
#define GOM_VALUE_HOLDS_EXPRESSION(value) (G_VALUE_HOLDS ((value), GOM_TYPE_EXPRESSION))
#define GOM_TYPE_LITERAL_EXPRESSION (gom_literal_expression_get_type())
#define GOM_TYPE_FIELD_EXPRESSION (gom_field_expression_get_type())
#define GOM_TYPE_FUNCTION_EXPRESSION (gom_function_expression_get_type())
#define GOM_TYPE_UNARY_EXPRESSION (gom_unary_expression_get_type())
#define GOM_TYPE_BINARY_EXPRESSION (gom_binary_expression_get_type())
#define GOM_TYPE_SEARCH_EXPRESSION (gom_search_expression_get_type())
#define GOM_TYPE_VECTOR_DISTANCE_EXPRESSION (gom_vector_distance_expression_get_type())

GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomExpression, gom_expression, GOM, EXPRESSION, GObject)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomLiteralExpression, gom_literal_expression, GOM, LITERAL_EXPRESSION, GomExpression)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomFieldExpression, gom_field_expression, GOM, FIELD_EXPRESSION, GomExpression)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomFunctionExpression, gom_function_expression, GOM, FUNCTION_EXPRESSION, GomExpression)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomUnaryExpression, gom_unary_expression, GOM, UNARY_EXPRESSION, GomExpression)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomBinaryExpression, gom_binary_expression, GOM, BINARY_EXPRESSION, GomExpression)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomSearchExpression, gom_search_expression, GOM, SEARCH_EXPRESSION, GomExpression)
GOM_AVAILABLE_IN_ALL
GOM_DECLARE_INTERNAL_TYPE (GomVectorDistanceExpression, gom_vector_distance_expression, GOM, VECTOR_DISTANCE_EXPRESSION, GomExpression)

GOM_AVAILABLE_IN_ALL
GParamSpec    *gom_param_spec_expression               (const char           *name,
                                                        const char           *nick,
                                                        const char           *blurb,
                                                        GParamFlags           flags);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_value_dup_expression                (const GValue         *value);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_value_get_expression                (const GValue         *value);
GOM_AVAILABLE_IN_ALL
void           gom_value_set_expression                (GValue               *value,
                                                        GomExpression        *expression);
GOM_AVAILABLE_IN_ALL
void           gom_value_take_expression               (GValue               *value,
                                                        GomExpression        *expression);
GOM_AVAILABLE_IN_ALL
gboolean       gom_expression_is_constant              (GomExpression        *self);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_literal_expression_new              (const GValue         *value);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_literal_expression_new_string       (const char           *value);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_literal_expression_new_int64        (gint64                value);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_literal_expression_new_boolean      (gboolean              value);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_field_expression_new                (const char           *field);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_function_expression_new             (const char           *name,
                                                        GomExpression       **arguments,
                                                        gsize                 n_arguments);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_unary_expression_new_negate         (GomExpression        *operand);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_unary_expression_new_not            (GomExpression        *operand);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_add           (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_subtract      (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_multiply      (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_divide        (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_modulo        (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_equal         (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_not_equal     (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_less_than     (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_less_equal    (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_greater_than  (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_greater_equal (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_and           (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_or            (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_binary_expression_new_like          (GomExpression        *left,
                                                        GomExpression        *right);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_search_expression_new               (GomExpression        *target,
                                                        GomExpression        *query,
                                                        GomSearchMode         mode);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_search_expression_new_for_field     (const char           *field,
                                                        const char           *query,
                                                        GomSearchMode         mode);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_search_expression_get_target        (GomSearchExpression  *self);
GOM_AVAILABLE_IN_ALL
GomExpression *gom_search_expression_get_query         (GomSearchExpression  *self);
GOM_AVAILABLE_IN_ALL
GomSearchMode  gom_search_expression_get_mode          (GomSearchExpression  *self);

#ifndef __GI_SCANNER__
static inline gboolean
gom_set_expression (GomExpression **expression_ptr,
                    GomExpression  *new_expression)
{
  GomExpression *old_expression;

  g_return_val_if_fail (expression_ptr != NULL, FALSE);
  g_return_val_if_fail (new_expression == NULL || GOM_IS_EXPRESSION (new_expression), FALSE);

  old_expression = *expression_ptr;

  if (old_expression == new_expression)
    return FALSE;

  *expression_ptr = new_expression ? g_object_ref (new_expression) : NULL;

  if (old_expression != NULL)
    g_object_unref (old_expression);

  return TRUE;
}

static inline void
gom_clear_expression (GomExpression **expression_ptr)
{
  GomExpression *old_expression;

  g_return_if_fail (expression_ptr != NULL);

  old_expression = g_steal_pointer (expression_ptr);

  if (old_expression != NULL)
    g_object_unref (old_expression);
}
#endif

G_END_DECLS
