/* gom-vector.h
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

#include <gio/gio.h>

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_VECTOR (gom_vector_get_type())
#define GOM_TYPE_VECTOR_FORMAT (gom_vector_format_get_type())
#define GOM_TYPE_VECTOR_METRIC (gom_vector_metric_get_type())
#define GOM_TYPE_REPOSITORY_FEATURE (gom_repository_feature_get_type())

GOM_AVAILABLE_IN_ALL
GType            gom_vector_get_type                          (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GType            gom_vector_format_get_type                   (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GType            gom_vector_metric_get_type                   (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GType            gom_repository_feature_get_type              (void) G_GNUC_CONST;
GOM_AVAILABLE_IN_ALL
GomVector       *gom_vector_new                               (GomVectorFormat   format,
                                                               guint             dimensions,
                                                               GBytes           *bytes,
                                                               GError          **error);
GOM_AVAILABLE_IN_ALL
GomVector       *gom_vector_new_float32                       (const float      *values,
                                                               guint             n_values);
GOM_AVAILABLE_IN_ALL
GomVector       *gom_vector_ref                               (GomVector        *self);
GOM_AVAILABLE_IN_ALL
void             gom_vector_unref                             (GomVector        *self);
GOM_AVAILABLE_IN_ALL
GomVectorFormat  gom_vector_get_format                        (GomVector        *self);
GOM_AVAILABLE_IN_ALL
guint            gom_vector_get_dimensions                    (GomVector        *self);
GOM_AVAILABLE_IN_ALL
GBytes          *gom_vector_dup_bytes                         (GomVector        *self);
GOM_AVAILABLE_IN_ALL
const float     *gom_vector_get_float32                       (GomVector        *self,
                                                               guint            *n_values);
GOM_AVAILABLE_IN_ALL
gboolean         gom_vector_distance                          (GomVector        *left,
                                                               GomVector        *right,
                                                               GomVectorMetric   metric,
                                                               double           *distance,
                                                               GError          **error);
GOM_AVAILABLE_IN_ALL
GomExpression   *gom_vector_distance_expression_new           (GomExpression    *target,
                                                               GomVector        *query,
                                                               GomVectorMetric   metric);
GOM_AVAILABLE_IN_ALL
GomExpression   *gom_vector_distance_expression_new_for_field (const char       *field,
                                                               GomVector        *query,
                                                               GomVectorMetric   metric);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomVector, gom_vector_unref)

G_END_DECLS
