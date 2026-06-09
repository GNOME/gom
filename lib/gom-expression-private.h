/* gom-expression-private.h
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

#include "gom-expression.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

GomUnaryOperator   _gom_unary_expression_get_operator         (GomUnaryExpression          *self);
GomExpression     *_gom_unary_expression_get_operand          (GomUnaryExpression          *self);
GomBinaryOperator  _gom_binary_expression_get_operator        (GomBinaryExpression         *self);
GomExpression     *_gom_binary_expression_get_left            (GomBinaryExpression         *self);
GomExpression     *_gom_binary_expression_get_right           (GomBinaryExpression         *self);
gboolean           _gom_literal_expression_has_value          (GomLiteralExpression        *self);
const GValue      *_gom_literal_expression_peek_value         (GomLiteralExpression        *self);
const char        *_gom_field_expression_get_field            (GomFieldExpression          *self);
const char        *_gom_function_expression_get_name          (GomFunctionExpression       *self);
GPtrArray         *_gom_function_expression_get_arguments     (GomFunctionExpression       *self);
GomExpression     *_gom_search_expression_get_target          (GomSearchExpression         *self);
GomExpression     *_gom_search_expression_get_query           (GomSearchExpression         *self);
GomSearchMode      _gom_search_expression_get_mode            (GomSearchExpression         *self);
GomExpression     *_gom_vector_distance_expression_new        (GomExpression               *target,
                                                               GomVector                   *query,
                                                               GomVectorMetric              metric);
GomExpression     *_gom_vector_distance_expression_get_target (GomVectorDistanceExpression *self);
GomVector         *_gom_vector_distance_expression_get_query  (GomVectorDistanceExpression *self);
GomVectorMetric    _gom_vector_distance_expression_get_metric (GomVectorDistanceExpression *self);

G_END_DECLS
