/* gom-vector.c
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

#include <math.h>

#include "gom-expression-private.h"
#include "gom-vector.h"

struct _GomVector
{
  gatomicrefcount  ref_count;
  GBytes          *bytes;
#if G_BYTE_ORDER != G_LITTLE_ENDIAN
  float           *native_float32;
#endif
  GomVectorFormat  format;
  guint            dimensions;
};

G_DEFINE_BOXED_TYPE (GomVector, gom_vector, gom_vector_ref, gom_vector_unref)
G_DEFINE_ENUM_TYPE (GomVectorFormat, gom_vector_format,
                    G_DEFINE_ENUM_VALUE (GOM_VECTOR_FORMAT_FLOAT32_LE, "float32-le"),
                    G_DEFINE_ENUM_VALUE (GOM_VECTOR_FORMAT_FLOAT32, "float32"))
G_DEFINE_ENUM_TYPE (GomVectorMetric, gom_vector_metric,
                    G_DEFINE_ENUM_VALUE (GOM_VECTOR_METRIC_COSINE, "cosine"),
                    G_DEFINE_ENUM_VALUE (GOM_VECTOR_METRIC_DOT, "dot"),
                    G_DEFINE_ENUM_VALUE (GOM_VECTOR_METRIC_L2, "l2"))
G_DEFINE_ENUM_TYPE (GomRepositoryFeature, gom_repository_feature,
                    G_DEFINE_ENUM_VALUE (GOM_REPOSITORY_FEATURE_VECTOR_SEARCH, "vector-search"))

static gboolean
gom_vector_validate (GomVectorFormat   format,
                     guint             dimensions,
                     GBytes           *bytes,
                     GError          **error)
{
  gsize size = 0;

  g_assert (bytes != NULL);

  size = g_bytes_get_size (bytes);

  if (dimensions == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Vector dimensions must be non-zero");
      return FALSE;
    }

  switch (format)
    {
    case GOM_VECTOR_FORMAT_FLOAT32_LE:
      if (size != (gsize)dimensions * sizeof (float))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Float32 vector has %" G_GSIZE_FORMAT " bytes for %u dimensions",
                       size,
                       dimensions);
          return FALSE;
        }
      return TRUE;

    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Unsupported vector format %d",
                   format);
      return FALSE;
    }
}

#if G_BYTE_ORDER != G_LITTLE_ENDIAN
static float *
gom_vector_dup_native_float32 (GBytes *bytes,
                               guint   dimensions)
{
  const guint8 *data;
  float *values;

  g_assert (bytes != NULL);
  g_assert (dimensions > 0);

  data = g_bytes_get_data (bytes, NULL);
  values = g_new (float, dimensions);

  for (guint i = 0; i < dimensions; i++)
    {
      guint32 value;

      memcpy (&value, data + ((gsize)i * sizeof value), sizeof value);
      value = GUINT32_FROM_LE (value);
      memcpy (&values[i], &value, sizeof values[i]);
    }

  return values;
}
#endif

/**
 * gom_vector_new:
 * @format: the vector storage format
 * @dimensions: the number of vector dimensions
 * @bytes: packed vector data
 * @error: return location for a [type@GLib.Error]
 *
 * Creates a new immutable vector from packed bytes.
 *
 * Returns: (transfer full): a new [struct@Gom.Vector], or %NULL on failure.
 */
GomVector *
gom_vector_new (GomVectorFormat   format,
                guint             dimensions,
                GBytes           *bytes,
                GError          **error)
{
  GomVector *self;

  g_return_val_if_fail (bytes != NULL, NULL);

  if (!gom_vector_validate (format, dimensions, bytes, error))
    return NULL;

  self = g_new0 (GomVector, 1);
  g_atomic_ref_count_init (&self->ref_count);
  self->format = format;
  self->dimensions = dimensions;
  self->bytes = g_bytes_ref (bytes);
#if G_BYTE_ORDER != G_LITTLE_ENDIAN
  if (format == GOM_VECTOR_FORMAT_FLOAT32_LE)
    self->native_float32 = gom_vector_dup_native_float32 (bytes, dimensions);
#endif

  return self;
}

/**
 * gom_vector_new_float32:
 * @values: (array length=n_values): float values to copy into the vector
 * @n_values: the number of values in @values
 *
 * Creates a new float32 vector by copying @values.
 *
 * Returns: (transfer full): a new [struct@Gom.Vector].
 */
GomVector *
gom_vector_new_float32 (const float *values,
                        guint        n_values)
{
  g_autoptr(GBytes) bytes = NULL;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  const float *stored_values = values;
#else
  g_autofree guint32 *stored_values = NULL;
#endif

  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (n_values > 0, NULL);

#if G_BYTE_ORDER != G_LITTLE_ENDIAN
  stored_values = g_new (guint32, n_values);
  for (guint i = 0; i < n_values; i++)
    {
      guint32 value;

      memcpy (&value, &values[i], sizeof value);
      stored_values[i] = GUINT32_TO_LE (value);
    }
#endif

  bytes = g_bytes_new (stored_values, (gsize)n_values * sizeof (float));
  return gom_vector_new (GOM_VECTOR_FORMAT_FLOAT32_LE, n_values, bytes, NULL);
}

/**
 * gom_vector_ref:
 * @self: a [struct@Gom.Vector]
 *
 * Acquires a reference to @self.
 *
 * Returns: (transfer full): @self.
 */
GomVector *
gom_vector_ref (GomVector *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

/**
 * gom_vector_unref:
 * @self: (transfer full): a [struct@Gom.Vector]
 *
 * Releases a reference to @self.
 */
void
gom_vector_unref (GomVector *self)
{
  if (self == NULL)
    return;

  if (g_atomic_ref_count_dec (&self->ref_count))
    {
      g_clear_pointer (&self->bytes, g_bytes_unref);
#if G_BYTE_ORDER != G_LITTLE_ENDIAN
      g_clear_pointer (&self->native_float32, g_free);
#endif
      g_free (self);
    }
}

GomVectorFormat
gom_vector_get_format (GomVector *self)
{
  g_return_val_if_fail (self != NULL, GOM_VECTOR_FORMAT_FLOAT32_LE);

  return self->format;
}

guint
gom_vector_get_dimensions (GomVector *self)
{
  g_return_val_if_fail (self != NULL, 0);

  return self->dimensions;
}

/**
 * gom_vector_dup_bytes:
 * @self: a [struct@Gom.Vector]
 *
 * Gets the packed storage bytes for @self.
 *
 * Returns: (transfer full): a [struct@GLib.Bytes].
 */
GBytes *
gom_vector_dup_bytes (GomVector *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return g_bytes_ref (self->bytes);
}

/**
 * gom_vector_get_float32:
 * @self: a [struct@Gom.Vector]
 * @n_values: (out) (optional): return location for the number of values
 *
 * Gets the float32 values in @self.
 *
 * Returns: (transfer none) (array length=n_values): the vector values in
 *   native byte order.
 */
const float *
gom_vector_get_float32 (GomVector *self,
                        guint     *n_values)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->format == GOM_VECTOR_FORMAT_FLOAT32_LE, NULL);

  if (n_values != NULL)
    *n_values = self->dimensions;

#if G_BYTE_ORDER != G_LITTLE_ENDIAN
  return self->native_float32;
#else
  return g_bytes_get_data (self->bytes, NULL);
#endif
}

static float
gom_vector_read_float32_le (const guint8 *data,
                            guint         index)
{
  guint32 value;
  float ret;

  memcpy (&value, data + ((gsize)index * sizeof value), sizeof value);
  value = GUINT32_FROM_LE (value);
  memcpy (&ret, &value, sizeof ret);

  return ret;
}

/**
 * gom_vector_distance:
 * @left: a [struct@Gom.Vector]
 * @right: a [struct@Gom.Vector]
 * @metric: the metric to use
 * @distance: return location for the distance
 * @error: return location for a [type@GLib.Error]
 *
 * Computes a distance or similarity score between two vectors in-process.
 *
 * Returns: %TRUE on success; otherwise %FALSE and @error is set.
 */
gboolean
gom_vector_distance (GomVector        *left,
                     GomVector        *right,
                     GomVectorMetric   metric,
                     double           *distance,
                     GError          **error)
{
  const guint8 *left_values;
  const guint8 *right_values;
  double dot = 0;
  double left_norm = 0;
  double right_norm = 0;
  double l2 = 0;

  g_return_val_if_fail (left != NULL, FALSE);
  g_return_val_if_fail (right != NULL, FALSE);
  g_return_val_if_fail (distance != NULL, FALSE);

  if (left->format != GOM_VECTOR_FORMAT_FLOAT32_LE || right->format != GOM_VECTOR_FORMAT_FLOAT32_LE)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Only float32-le vectors are supported");
      return FALSE;
    }

  if (left->dimensions != right->dimensions)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Vector dimensions differ: %u != %u",
                   left->dimensions,
                   right->dimensions);
      return FALSE;
    }

  left_values = g_bytes_get_data (left->bytes, NULL);
  right_values = g_bytes_get_data (right->bytes, NULL);

  for (guint i = 0; i < left->dimensions; i++)
    {
      double left_value = gom_vector_read_float32_le (left_values, i);
      double right_value = gom_vector_read_float32_le (right_values, i);
      double diff = left_value - right_value;

      dot += left_value * right_value;
      left_norm += left_value * left_value;
      right_norm += right_value * right_value;
      l2 += diff * diff;
    }

  switch (metric)
    {
    case GOM_VECTOR_METRIC_COSINE:
      *distance = (left_norm == 0 || right_norm == 0)
                ? 1.0
                : 1.0 - (dot / sqrt (left_norm * right_norm));
      return TRUE;

    case GOM_VECTOR_METRIC_DOT:
      *distance = dot;
      return TRUE;

    case GOM_VECTOR_METRIC_L2:
      *distance = l2;
      return TRUE;

    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Unsupported vector metric %d",
                   metric);
      return FALSE;
    }
}

/**
 * gom_vector_distance_expression_new:
 * @target: the vector expression to compare against
 * @query: the query vector
 * @metric: the metric to use
 *
 * Creates an expression that evaluates the distance between @target and
 * @query. Backends that do not support the requested vector search feature
 * reject queries containing this expression.
 *
 * Returns: (transfer full): a [class@Gom.Expression].
 */
GomExpression *
gom_vector_distance_expression_new (GomExpression   *target,
                                    GomVector       *query,
                                    GomVectorMetric  metric)
{
  g_return_val_if_fail (GOM_IS_EXPRESSION (target), NULL);
  g_return_val_if_fail (query != NULL, NULL);

  return _gom_vector_distance_expression_new (target, query, metric);
}

/**
 * gom_vector_distance_expression_new_for_field:
 * @field: the field or property containing the vector
 * @query: the query vector
 * @metric: the metric to use
 *
 * Creates a vector distance expression for @field.
 *
 * Returns: (transfer full): a [class@Gom.Expression].
 */
GomExpression *
gom_vector_distance_expression_new_for_field (const char      *field,
                                              GomVector       *query,
                                              GomVectorMetric  metric)
{
  g_autoptr(GomExpression) target = NULL;

  g_return_val_if_fail (field != NULL, NULL);
  g_return_val_if_fail (query != NULL, NULL);

  target = gom_field_expression_new (field);
  return gom_vector_distance_expression_new (target, query, metric);
}
