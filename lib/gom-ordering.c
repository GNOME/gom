/* gom-ordering.c
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

#include "gom-expression.h"
#include "gom-ordering.h"

struct _GomOrdering
{
  GObject           parent_instance;
  GomExpression    *expression;
  GomSortDirection  direction;
  GomNullsMode      nulls_mode;
};

enum
{
  PROP_0,
  PROP_EXPRESSION,
  PROP_DIRECTION,
  PROP_NULLS_MODE,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomOrdering, gom_ordering, G_TYPE_OBJECT)
G_DEFINE_ENUM_TYPE (GomSortDirection, gom_sort_direction,
                    G_DEFINE_ENUM_VALUE (GOM_SORT_ASCENDING, "GOM_SORT_ASCENDING"),
                    G_DEFINE_ENUM_VALUE (GOM_SORT_DESCENDING, "GOM_SORT_DESCENDING"))
G_DEFINE_ENUM_TYPE (GomNullsMode, gom_nulls_mode,
                    G_DEFINE_ENUM_VALUE (GOM_NULLS_DEFAULT, "GOM_NULLS_DEFAULT"),
                    G_DEFINE_ENUM_VALUE (GOM_NULLS_FIRST, "GOM_NULLS_FIRST"),
                    G_DEFINE_ENUM_VALUE (GOM_NULLS_LAST, "GOM_NULLS_LAST"))

static GParamSpec *properties[N_PROPS];

static void
gom_ordering_finalize (GObject *object)
{
  GomOrdering *self = (GomOrdering *)object;

  gom_clear_expression (&self->expression);

  G_OBJECT_CLASS (gom_ordering_parent_class)->finalize (object);
}

static void
gom_ordering_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GomOrdering *self = GOM_ORDERING (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gom_value_set_expression (value, self->expression);
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, self->direction);
      break;

    case PROP_NULLS_MODE:
      g_value_set_enum (value, self->nulls_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_ordering_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GomOrdering *self = GOM_ORDERING (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gom_set_expression (&self->expression, gom_value_get_expression (value));
      break;

    case PROP_DIRECTION:
      self->direction = g_value_get_enum (value);
      break;

    case PROP_NULLS_MODE:
      self->nulls_mode = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_ordering_class_init (GomOrderingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_ordering_finalize;
  object_class->get_property = gom_ordering_get_property;
  object_class->set_property = gom_ordering_set_property;

  properties[PROP_EXPRESSION] =
    gom_param_spec_expression ("expression", NULL, NULL,
                               (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_DIRECTION] =
    g_param_spec_enum ("direction", NULL, NULL,
                       GOM_TYPE_SORT_DIRECTION, GOM_SORT_ASCENDING,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_NULLS_MODE] =
    g_param_spec_enum ("nulls-mode", NULL, NULL,
                       GOM_TYPE_NULLS_MODE, GOM_NULLS_DEFAULT,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_ordering_init (GomOrdering *self)
{
  self->direction = GOM_SORT_ASCENDING;
  self->nulls_mode = GOM_NULLS_DEFAULT;
}

/**
 * gom_ordering_new:
 * @expression: (transfer full): a [class@Gom.Expression]
 * @direction: the sort direction
 *
 * Creates a new ordering expression.
 *
 * Returns: (transfer full): a [class@Gom.Ordering]
 */
GomOrdering *
gom_ordering_new (GomExpression    *expression,
                  GomSortDirection  direction)
{
  GomOrdering *self;

  g_return_val_if_fail (GOM_IS_EXPRESSION (expression), NULL);

  self = g_object_new (GOM_TYPE_ORDERING,
                       "expression", expression,
                       "direction", direction,
                       NULL);

  g_object_unref (expression);

  return self;
}

/**
 * gom_ordering_new_full:
 * @expression: (transfer full): a [class@Gom.Expression]
 * @nulls_mode: the nulls ordering mode
 *
 * Creates a new ordering expression with explicit nulls mode.
 *
 * Returns: (transfer full): a [class@Gom.Ordering]
 */
GomOrdering *
gom_ordering_new_full (GomExpression *expression,
                       GomNullsMode   nulls_mode)
{
  GomOrdering *self;

  g_return_val_if_fail (GOM_IS_EXPRESSION (expression), NULL);

  self = g_object_new (GOM_TYPE_ORDERING,
                       "expression", expression,
                       "nulls-mode", nulls_mode,
                       NULL);

  g_object_unref (expression);

  return self;
}

/**
 * gom_ordering_get_expression:
 * @self: a [class@Gom.Ordering]
 *
 * Gets the expression to order by.
 *
 * Returns: (transfer none): a [class@Gom.Expression]
 */
GomExpression *
gom_ordering_get_expression (GomOrdering *self)
{
  g_return_val_if_fail (GOM_IS_ORDERING (self), NULL);

  return self->expression;
}

/**
 * gom_ordering_get_direction:
 * @self: a [class@Gom.Ordering]
 *
 * Gets the sort direction.
 *
 * Returns: the sort direction
 */
GomSortDirection
gom_ordering_get_direction (GomOrdering *self)
{
  g_return_val_if_fail (GOM_IS_ORDERING (self), GOM_SORT_ASCENDING);

  return self->direction;
}

/**
 * gom_ordering_get_nulls_mode:
 * @self: a [class@Gom.Ordering]
 *
 * Gets the nulls ordering mode.
 *
 * Returns: the nulls ordering mode
 */
GomNullsMode
gom_ordering_get_nulls_mode (GomOrdering *self)
{
  g_return_val_if_fail (GOM_IS_ORDERING (self), GOM_NULLS_DEFAULT);

  return self->nulls_mode;
}

/**
 * gom_ordering_copy:
 * @self: a [class@Gom.Ordering]
 *
 * Returns: (transfer full):
 */
GomOrdering *
gom_ordering_copy (GomOrdering *self)
{
  GomOrdering *copy;

  g_return_val_if_fail (GOM_IS_ORDERING (self), NULL);

  copy = g_object_new (GOM_TYPE_ORDERING, NULL);
  gom_set_expression (&copy->expression, self->expression);
  copy->direction = self->direction;
  copy->nulls_mode = self->nulls_mode;

  return g_steal_pointer (&copy);
}
