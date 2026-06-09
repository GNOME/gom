/* gom-expression.c
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

#include "gom-expression-private.h"
#include "gom-vector.h"

struct _GomExpression
{
  GObject parent_instance;
};

struct _GomExpressionClass
{
  GObjectClass parent_class;

  gboolean (*is_constant) (GomExpression *self);
};

struct _GomLiteralExpression
{
  GomExpression parent_instance;

  GValue value;
  guint  has_value : 1;
};

struct _GomLiteralExpressionClass
{
  GomExpressionClass parent_class;
};

struct _GomFieldExpression
{
  GomExpression parent_instance;

  char *field;
};

struct _GomFieldExpressionClass
{
  GomExpressionClass parent_class;
};

struct _GomFunctionExpression
{
  GomExpression parent_instance;

  char      *name;
  GPtrArray *arguments;
};

struct _GomFunctionExpressionClass
{
  GomExpressionClass parent_class;
};

struct _GomUnaryExpression
{
  GomExpression parent_instance;

  GomUnaryOperator  operator;
  GomExpression    *operand;
};

struct _GomUnaryExpressionClass
{
  GomExpressionClass parent_class;
};

struct _GomBinaryExpression
{
  GomExpression parent_instance;

  GomBinaryOperator  operator;
  GomExpression     *left;
  GomExpression     *right;
};

struct _GomBinaryExpressionClass
{
  GomExpressionClass parent_class;
};

struct _GomSearchExpression
{
  GomExpression parent_instance;

  GomExpression *target;
  GomExpression *query;
  GomSearchMode  mode;
};

struct _GomSearchExpressionClass
{
  GomExpressionClass parent_class;
};

struct _GomVectorDistanceExpression
{
  GomExpression parent_instance;

  GomExpression   *target;
  GomVector       *query;
  GomVectorMetric  metric;
};

struct _GomVectorDistanceExpressionClass
{
  GomExpressionClass parent_class;
};

/**
 * GomExpression: (set-value-func gom_value_set_expression)
 *   (get-value-func gom_value_get_expression)
 *
 * Base type for expression nodes used by query and mutation builders.
 */

G_DEFINE_ABSTRACT_TYPE (GomExpression, gom_expression, G_TYPE_OBJECT)
G_DEFINE_FINAL_TYPE (GomLiteralExpression, gom_literal_expression, GOM_TYPE_EXPRESSION)
G_DEFINE_FINAL_TYPE (GomFieldExpression, gom_field_expression, GOM_TYPE_EXPRESSION)
G_DEFINE_FINAL_TYPE (GomFunctionExpression, gom_function_expression, GOM_TYPE_EXPRESSION)
G_DEFINE_FINAL_TYPE (GomUnaryExpression, gom_unary_expression, GOM_TYPE_EXPRESSION)
G_DEFINE_FINAL_TYPE (GomBinaryExpression, gom_binary_expression, GOM_TYPE_EXPRESSION)
G_DEFINE_FINAL_TYPE (GomSearchExpression, gom_search_expression, GOM_TYPE_EXPRESSION)
G_DEFINE_FINAL_TYPE (GomVectorDistanceExpression, gom_vector_distance_expression, GOM_TYPE_EXPRESSION)

static void
gom_expression_finalize (GObject *object)
{
  G_OBJECT_CLASS (gom_expression_parent_class)->finalize (object);
}

static gboolean
gom_expression_real_is_constant (GomExpression *self)
{
  return FALSE;
}

static void
gom_expression_class_init (GomExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_expression_finalize;
  klass->is_constant = gom_expression_real_is_constant;
}

static void
gom_expression_init (GomExpression *self)
{
}

/**
 * gom_expression_is_constant:
 * @self: a [class@Gom.Expression]
 *
 * Checks whether the expression is constant.
 *
 * Returns: %TRUE if the expression is constant
 */
gboolean
gom_expression_is_constant (GomExpression *self)
{
  g_return_val_if_fail (GOM_IS_EXPRESSION (self), FALSE);

  return GOM_EXPRESSION_GET_CLASS (self)->is_constant (self);
}

static gboolean
gom_literal_expression_is_constant (GomExpression *self)
{
  return TRUE;
}

static void
gom_literal_expression_finalize (GObject *object)
{
  GomLiteralExpression *self = (GomLiteralExpression *)object;

  if (self->has_value)
    g_value_unset (&self->value);

  G_OBJECT_CLASS (gom_literal_expression_parent_class)->finalize (object);
}

static void
gom_literal_expression_class_init (GomLiteralExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_literal_expression_finalize;
  expression_class->is_constant = gom_literal_expression_is_constant;
}

static void
gom_literal_expression_init (GomLiteralExpression *self)
{
  self->has_value = FALSE;
}

static void
gom_field_expression_finalize (GObject *object)
{
  GomFieldExpression *self = (GomFieldExpression *)object;

  g_clear_pointer (&self->field, g_free);

  G_OBJECT_CLASS (gom_field_expression_parent_class)->finalize (object);
}

static gboolean
gom_field_expression_is_constant (GomExpression *self)
{
  return FALSE;
}

static void
gom_field_expression_class_init (GomFieldExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_field_expression_finalize;
  expression_class->is_constant = gom_field_expression_is_constant;
}

static void
gom_field_expression_init (GomFieldExpression *self)
{
}

static void
gom_function_expression_finalize (GObject *object)
{
  GomFunctionExpression *self = (GomFunctionExpression *)object;

  g_clear_pointer (&self->arguments, g_ptr_array_unref);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gom_function_expression_parent_class)->finalize (object);
}

static gboolean
gom_function_expression_is_constant (GomExpression *self)
{
  return FALSE;
}

static void
gom_function_expression_class_init (GomFunctionExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_function_expression_finalize;
  expression_class->is_constant = gom_function_expression_is_constant;
}

static void
gom_function_expression_init (GomFunctionExpression *self)
{
}

static void
gom_unary_expression_finalize (GObject *object)
{
  GomUnaryExpression *self = (GomUnaryExpression *)object;

  g_clear_pointer (&self->operand, g_object_unref);

  G_OBJECT_CLASS (gom_unary_expression_parent_class)->finalize (object);
}

static gboolean
gom_unary_expression_is_constant (GomExpression *expression)
{
  GomUnaryExpression *self = (GomUnaryExpression *)expression;

  return gom_expression_is_constant (self->operand);
}

static void
gom_unary_expression_class_init (GomUnaryExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_unary_expression_finalize;
  expression_class->is_constant = gom_unary_expression_is_constant;
}

static void
gom_unary_expression_init (GomUnaryExpression *self)
{
}

static void
gom_binary_expression_finalize (GObject *object)
{
  GomBinaryExpression *self = (GomBinaryExpression *)object;

  g_clear_pointer (&self->left, g_object_unref);
  g_clear_pointer (&self->right, g_object_unref);

  G_OBJECT_CLASS (gom_binary_expression_parent_class)->finalize (object);
}

static gboolean
gom_binary_expression_is_constant (GomExpression *expression)
{
  GomBinaryExpression *self = (GomBinaryExpression *)expression;

  return (gom_expression_is_constant (self->left) && gom_expression_is_constant (self->right));
}

static void
gom_binary_expression_class_init (GomBinaryExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_binary_expression_finalize;
  expression_class->is_constant = gom_binary_expression_is_constant;
}

static void
gom_binary_expression_init (GomBinaryExpression *self)
{
}

static void
gom_search_expression_finalize (GObject *object)
{
  GomSearchExpression *self = (GomSearchExpression *)object;

  g_clear_pointer (&self->target, g_object_unref);
  g_clear_pointer (&self->query, g_object_unref);

  G_OBJECT_CLASS (gom_search_expression_parent_class)->finalize (object);
}

static gboolean
gom_search_expression_is_constant (GomExpression *expression)
{
  return FALSE;
}

static void
gom_search_expression_class_init (GomSearchExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_search_expression_finalize;
  expression_class->is_constant = gom_search_expression_is_constant;
}

static void
gom_search_expression_init (GomSearchExpression *self)
{
}

static void
gom_vector_distance_expression_finalize (GObject *object)
{
  GomVectorDistanceExpression *self = (GomVectorDistanceExpression *)object;

  g_clear_pointer (&self->target, g_object_unref);
  g_clear_pointer (&self->query, gom_vector_unref);

  G_OBJECT_CLASS (gom_vector_distance_expression_parent_class)->finalize (object);
}

static gboolean
gom_vector_distance_expression_is_constant (GomExpression *expression)
{
  GomVectorDistanceExpression *self = (GomVectorDistanceExpression *)expression;

  return self->target != NULL && gom_expression_is_constant (self->target);
}

static void
gom_vector_distance_expression_class_init (GomVectorDistanceExpressionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomExpressionClass *expression_class = GOM_EXPRESSION_CLASS (klass);

  object_class->finalize = gom_vector_distance_expression_finalize;
  expression_class->is_constant = gom_vector_distance_expression_is_constant;
}

static void
gom_vector_distance_expression_init (GomVectorDistanceExpression *self)
{
}

/**
 * gom_literal_expression_new:
 * @value: (nullable): the literal value
 *
 * Creates a literal expression for @value.
 *
 * Returns: (transfer full): a [class@Gom.Expression]
 */
GomExpression *
gom_literal_expression_new (const GValue *value)
{
  GomLiteralExpression *self;

  self = g_object_new (GOM_TYPE_LITERAL_EXPRESSION, NULL);

  if (value != NULL && G_VALUE_TYPE (value) != G_TYPE_INVALID)
    {
      g_value_init (&self->value, G_VALUE_TYPE (value));
      g_value_copy (value, &self->value);
      self->has_value = TRUE;
    }

  return GOM_EXPRESSION (self);
}

/**
 * gom_literal_expression_new_string:
 * @value: (nullable): the literal string value
 *
 * Creates a string literal expression for @value.
 *
 * Returns: (transfer full): a [class@Gom.Expression]
 */
GomExpression *
gom_literal_expression_new_string (const char *value)
{
  GValue literal = G_VALUE_INIT;
  GomExpression *expression;

  g_value_init (&literal, G_TYPE_STRING);
  g_value_set_string (&literal, value);
  expression = gom_literal_expression_new (&literal);
  g_value_unset (&literal);

  return expression;
}

/**
 * gom_literal_expression_new_int64:
 * @value: the literal integer value
 *
 * Creates an integer literal expression for @value.
 *
 * Returns: (transfer full): a [class@Gom.Expression]
 */
GomExpression *
gom_literal_expression_new_int64 (gint64 value)
{
  GValue literal = G_VALUE_INIT;
  GomExpression *expression;

  g_value_init (&literal, G_TYPE_INT64);
  g_value_set_int64 (&literal, value);
  expression = gom_literal_expression_new (&literal);
  g_value_unset (&literal);

  return expression;
}

/**
 * gom_literal_expression_new_boolean:
 * @value: the literal boolean value
 *
 * Creates a boolean literal expression for @value.
 *
 * Returns: (transfer full): a [class@Gom.Expression]
 */
GomExpression *
gom_literal_expression_new_boolean (gboolean value)
{
  GValue literal = G_VALUE_INIT;
  GomExpression *expression;

  g_value_init (&literal, G_TYPE_BOOLEAN);
  g_value_set_boolean (&literal, value);
  expression = gom_literal_expression_new (&literal);
  g_value_unset (&literal);

  return expression;
}

/**
 * gom_field_expression_new:
 * @field: the field or member name
 *
 * Creates a field expression referencing @field.
 *
 * Returns: (transfer full): a [class@Gom.Expression]
 */
GomExpression *
gom_field_expression_new (const char *field)
{
  GomFieldExpression *self;

  g_return_val_if_fail (field != NULL, NULL);

  self = g_object_new (GOM_TYPE_FIELD_EXPRESSION, NULL);
  self->field = g_strdup (field);

  return GOM_EXPRESSION (self);
}

/**
 * gom_function_expression_new:
 * @name: the function name
 * @arguments: (array length=n_arguments) (nullable) (element-type Gom.Expression): arguments
 * @n_arguments: number of arguments in @arguments
 *
 * Creates a function expression.
 *
 * Returns: (transfer full): a [class@Gom.Expression]
 */
GomExpression *
gom_function_expression_new (const char     *name,
                             GomExpression **arguments,
                             gsize           n_arguments)
{
  GomFunctionExpression *self;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (n_arguments == 0 || arguments != NULL, NULL);

  self = g_object_new (GOM_TYPE_FUNCTION_EXPRESSION, NULL);
  self->name = g_strdup (name);

  if (n_arguments > 0)
    {
      self->arguments = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);

      for (gsize i = 0; i < n_arguments; i++)
        g_ptr_array_add (self->arguments, g_object_ref (arguments[i]));
    }

  return GOM_EXPRESSION (self);
}

static GomExpression *
gom_unary_expression_new (GomUnaryOperator  operator,
                          GomExpression    *operand)
{
  GomUnaryExpression *self;

  g_return_val_if_fail (GOM_IS_EXPRESSION (operand), NULL);

  self = g_object_new (GOM_TYPE_UNARY_EXPRESSION, NULL);
  self->operator = operator;
  self->operand = g_object_ref (operand);

  return GOM_EXPRESSION (self);
}

static GomExpression *
gom_binary_expression_new (GomBinaryOperator  operator,
                           GomExpression     *left,
                           GomExpression     *right)
{
  GomBinaryExpression *self;

  g_return_val_if_fail (GOM_IS_EXPRESSION (left), NULL);
  g_return_val_if_fail (GOM_IS_EXPRESSION (right), NULL);

  self = g_object_new (GOM_TYPE_BINARY_EXPRESSION, NULL);
  self->operator = operator;
  self->left = g_object_ref (left);
  self->right = g_object_ref (right);

  return GOM_EXPRESSION (self);
}

/**
 * gom_unary_expression_new_negate:
 * @operand: (transfer full): a [class@Gom.Expression]
 *
 * Creates a unary negation expression.
 *
 * Returns: (transfer full) (type Gom.UnaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_unary_expression_new_negate (GomExpression *operand)
{
  return gom_unary_expression_new (GOM_UNARY_NEGATE, operand);
}

/**
 * gom_unary_expression_new_not:
 * @operand: (transfer full): a [class@Gom.Expression]
 *
 * Creates a logical NOT expression.
 *
 * Returns: (transfer full) (type Gom.UnaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_unary_expression_new_not (GomExpression *operand)
{
  return gom_unary_expression_new (GOM_UNARY_NOT, operand);
}

/**
 * gom_binary_expression_new_add:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates an addition expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_add (GomExpression *left,
                               GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_ADD, left, right);
}

/**
 * gom_binary_expression_new_subtract:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a subtraction expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_subtract (GomExpression *left,
                                    GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_SUBTRACT, left, right);
}

/**
 * gom_binary_expression_new_multiply:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a multiplication expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_multiply (GomExpression *left,
                                    GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_MULTIPLY, left, right);
}

/**
 * gom_binary_expression_new_divide:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a division expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_divide (GomExpression *left,
                                  GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_DIVIDE, left, right);
}

/**
 * gom_binary_expression_new_modulo:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a modulo expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_modulo (GomExpression *left,
                                  GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_MODULO, left, right);
}

/**
 * gom_binary_expression_new_equal:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates an equality comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_equal (GomExpression *left,
                                 GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_EQUAL, left, right);
}

/**
 * gom_binary_expression_new_not_equal:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates an inequality comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_not_equal (GomExpression *left,
                                     GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_NOT_EQUAL, left, right);
}

/**
 * gom_binary_expression_new_less_than:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a less-than comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_less_than (GomExpression *left,
                                     GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_LESS_THAN, left, right);
}

/**
 * gom_binary_expression_new_less_equal:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a less-than-or-equal comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_less_equal (GomExpression *left,
                                      GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_LESS_EQUAL, left, right);
}

/**
 * gom_binary_expression_new_greater_than:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a greater-than comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_greater_than (GomExpression *left,
                                        GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_GREATER_THAN, left, right);
}

/**
 * gom_binary_expression_new_greater_equal:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a greater-than-or-equal comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_greater_equal (GomExpression *left,
                                         GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_GREATER_EQUAL, left, right);
}

/**
 * gom_binary_expression_new_and:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a logical AND expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_and (GomExpression *left,
                               GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_AND, left, right);
}

/**
 * gom_binary_expression_new_or:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a logical OR expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_or (GomExpression *left,
                              GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_OR, left, right);
}

/**
 * gom_binary_expression_new_like:
 * @left: (transfer full): a [class@Gom.Expression]
 * @right: (transfer full): a [class@Gom.Expression]
 *
 * Creates a LIKE comparison expression.
 *
 * Returns: (transfer full) (type Gom.BinaryExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_binary_expression_new_like (GomExpression *left,
                                GomExpression *right)
{
  return gom_binary_expression_new (GOM_BINARY_LIKE, left, right);
}

/**
 * gom_search_expression_new:
 * @target: (transfer full): a [class@Gom.Expression]
 * @query: (transfer full): a [class@Gom.Expression]
 * @mode: the search match mode
 *
 * Creates a search expression.
 *
 * Returns: (transfer full) (type Gom.SearchExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_search_expression_new (GomExpression *target,
                           GomExpression *query,
                           GomSearchMode  mode)
{
  GomSearchExpression *self;

  g_return_val_if_fail (GOM_IS_EXPRESSION (target), NULL);
  g_return_val_if_fail (GOM_IS_EXPRESSION (query), NULL);

  self = g_object_new (GOM_TYPE_SEARCH_EXPRESSION, NULL);
  self->target = g_object_ref (target);
  self->query = g_object_ref (query);
  self->mode = mode;

  return GOM_EXPRESSION (self);
}

/**
 * gom_search_expression_new_for_field:
 * @field: the field or member name
 * @query: the query string
 * @mode: the search match mode
 *
 * Creates a search expression for @field.
 *
 * Returns: (transfer full) (type Gom.SearchExpression): a [class@Gom.Expression]
 */
GomExpression *
gom_search_expression_new_for_field (const char    *field,
                                     const char    *query,
                                     GomSearchMode  mode)
{
  GomExpression *target_expression;
  GomExpression *query_expression;

  g_return_val_if_fail (field != NULL, NULL);
  g_return_val_if_fail (query != NULL, NULL);

  target_expression = gom_field_expression_new (field);
  query_expression = gom_literal_expression_new_string (query);

  return gom_search_expression_new (target_expression, query_expression, mode);
}

/**
 * gom_search_expression_get_target:
 * @self: a [class@Gom.SearchExpression]
 *
 * Gets the target expression for a search.
 *
 * Returns: (transfer none): a [class@Gom.Expression]
 */
GomExpression *
gom_search_expression_get_target (GomSearchExpression *self)
{
  g_return_val_if_fail (GOM_IS_SEARCH_EXPRESSION (self), NULL);

  return self->target;
}

/**
 * gom_search_expression_get_query:
 * @self: a [class@Gom.SearchExpression]
 *
 * Gets the query expression for a search.
 *
 * Returns: (transfer none): a [class@Gom.Expression]
 */
GomExpression *
gom_search_expression_get_query (GomSearchExpression *self)
{
  g_return_val_if_fail (GOM_IS_SEARCH_EXPRESSION (self), NULL);

  return self->query;
}

/**
 * gom_search_expression_get_mode:
 * @self: a [class@Gom.SearchExpression]
 *
 * Gets the search mode.
 *
 * Returns: the search mode
 */
GomSearchMode
gom_search_expression_get_mode (GomSearchExpression *self)
{
  g_return_val_if_fail (GOM_IS_SEARCH_EXPRESSION (self), GOM_SEARCH_MODE_NATURAL);

  return self->mode;
}

GomUnaryOperator
_gom_unary_expression_get_operator (GomUnaryExpression *self)
{
  g_return_val_if_fail (GOM_IS_UNARY_EXPRESSION (self), GOM_UNARY_NEGATE);

  return self->operator;
}

GomExpression *
_gom_unary_expression_get_operand (GomUnaryExpression *self)
{
  g_return_val_if_fail (GOM_IS_UNARY_EXPRESSION (self), NULL);

  return self->operand;
}

GomBinaryOperator
_gom_binary_expression_get_operator (GomBinaryExpression *self)
{
  g_return_val_if_fail (GOM_IS_BINARY_EXPRESSION (self), GOM_BINARY_ADD);

  return self->operator;
}

GomExpression *
_gom_binary_expression_get_left (GomBinaryExpression *self)
{
  g_return_val_if_fail (GOM_IS_BINARY_EXPRESSION (self), NULL);

  return self->left;
}

GomExpression *
_gom_binary_expression_get_right (GomBinaryExpression *self)
{
  g_return_val_if_fail (GOM_IS_BINARY_EXPRESSION (self), NULL);

  return self->right;
}

gboolean
_gom_literal_expression_has_value (GomLiteralExpression *self)
{
  g_return_val_if_fail (GOM_IS_LITERAL_EXPRESSION (self), FALSE);

  return self->has_value;
}

const GValue *
_gom_literal_expression_peek_value (GomLiteralExpression *self)
{
  g_return_val_if_fail (GOM_IS_LITERAL_EXPRESSION (self), NULL);

  return self->has_value ? &self->value : NULL;
}

const char *
_gom_field_expression_get_field (GomFieldExpression *self)
{
  g_return_val_if_fail (GOM_IS_FIELD_EXPRESSION (self), NULL);

  return self->field;
}

const char *
_gom_function_expression_get_name (GomFunctionExpression *self)
{
  g_return_val_if_fail (GOM_IS_FUNCTION_EXPRESSION (self), NULL);

  return self->name;
}

GPtrArray *
_gom_function_expression_get_arguments (GomFunctionExpression *self)
{
  g_return_val_if_fail (GOM_IS_FUNCTION_EXPRESSION (self), NULL);

  return self->arguments;
}

GomExpression *
_gom_search_expression_get_target (GomSearchExpression *self)
{
  g_return_val_if_fail (GOM_IS_SEARCH_EXPRESSION (self), NULL);

  return self->target;
}

GomExpression *
_gom_search_expression_get_query (GomSearchExpression *self)
{
  g_return_val_if_fail (GOM_IS_SEARCH_EXPRESSION (self), NULL);

  return self->query;
}

GomSearchMode
_gom_search_expression_get_mode (GomSearchExpression *self)
{
  g_return_val_if_fail (GOM_IS_SEARCH_EXPRESSION (self), GOM_SEARCH_MODE_NATURAL);

  return self->mode;
}

GomExpression *
_gom_vector_distance_expression_new (GomExpression   *target,
                                     GomVector       *query,
                                     GomVectorMetric  metric)
{
  GomVectorDistanceExpression *self;

  g_return_val_if_fail (GOM_IS_EXPRESSION (target), NULL);
  g_return_val_if_fail (query != NULL, NULL);

  self = g_object_new (GOM_TYPE_VECTOR_DISTANCE_EXPRESSION, NULL);
  self->target = g_object_ref (target);
  self->query = gom_vector_ref (query);
  self->metric = metric;

  return GOM_EXPRESSION (self);
}

GomExpression *
_gom_vector_distance_expression_get_target (GomVectorDistanceExpression *self)
{
  g_return_val_if_fail (GOM_IS_VECTOR_DISTANCE_EXPRESSION (self), NULL);

  return self->target;
}

GomVector *
_gom_vector_distance_expression_get_query (GomVectorDistanceExpression *self)
{
  g_return_val_if_fail (GOM_IS_VECTOR_DISTANCE_EXPRESSION (self), NULL);

  return self->query;
}

GomVectorMetric
_gom_vector_distance_expression_get_metric (GomVectorDistanceExpression *self)
{
  g_return_val_if_fail (GOM_IS_VECTOR_DISTANCE_EXPRESSION (self), GOM_VECTOR_METRIC_COSINE);

  return self->metric;
}

/**
 * gom_value_set_expression:
 * @value: a `GValue` initialized with type `GOM_TYPE_EXPRESSION`
 * @expression: (nullable): a [class@Gom.Expression]
 *
 * Stores @expression inside @value and acquires a reference.
 */
void
gom_value_set_expression (GValue        *value,
                          GomExpression *expression)
{
  g_return_if_fail (G_VALUE_HOLDS (value, GOM_TYPE_EXPRESSION));

  if (expression != NULL)
    g_return_if_fail (GOM_IS_EXPRESSION (expression));

  g_value_set_object (value, expression);
}

/**
 * gom_value_take_expression:
 * @value: a `GValue` initialized with type `GOM_TYPE_EXPRESSION`
 * @expression: (transfer full) (nullable): a [class@Gom.Expression]
 *
 * Stores @expression inside @value and transfers ownership.
 */
void
gom_value_take_expression (GValue        *value,
                           GomExpression *expression)
{
  g_return_if_fail (G_VALUE_HOLDS (value, GOM_TYPE_EXPRESSION));
  g_return_if_fail (expression == NULL || GOM_IS_EXPRESSION (expression));

  g_value_take_object (value, expression);
}

/**
 * gom_value_get_expression:
 * @value: a `GValue` initialized with type `GOM_TYPE_EXPRESSION`
 *
 * Retrieves the expression stored inside @value.
 *
 * Returns: (transfer none) (nullable): a [class@Gom.Expression]
 */
GomExpression *
gom_value_get_expression (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, GOM_TYPE_EXPRESSION), NULL);

  return g_value_get_object (value);
}

/**
 * gom_value_dup_expression:
 * @value: a `GValue` initialized with type `GOM_TYPE_EXPRESSION`
 *
 * Retrieves the expression stored inside @value and acquires a reference.
 *
 * Returns: (transfer full) (nullable): a [class@Gom.Expression]
 */
GomExpression *
gom_value_dup_expression (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, GOM_TYPE_EXPRESSION), NULL);

  return g_value_dup_object (value);
}

/**
 * gom_param_spec_expression:
 * @name: canonical name of the property
 * @nick: a user-readable name for the property
 * @blurb: a user-readable description of the property
 * @flags: flags for the property
 *
 * Creates a new [class@GObject.ParamSpec] for a property holding a
 * [class@Gom.Expression].
 *
 * Returns: (transfer full): a newly created property specification
 */
GParamSpec *
gom_param_spec_expression (const char  *name,
                           const char  *nick,
                           const char  *blurb,
                           GParamFlags  flags)
{
  return g_param_spec_object (name, nick, blurb, GOM_TYPE_EXPRESSION, flags);
}
