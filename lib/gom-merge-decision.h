/* gom-merge-decision.h
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

#define GOM_TYPE_MERGE_DECISION (gom_merge_decision_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GomMergeDecision, gom_merge_decision, GOM, MERGE_DECISION, GObject)

GOM_AVAILABLE_IN_ALL
GomMergeDecision *gom_merge_decision_new              (GomRepository    *repository,
                                                       GomDelta         *local_delta,
                                                       GomDelta         *remote_delta);
GOM_AVAILABLE_IN_ALL
GomRepository    *gom_merge_decision_dup_repository   (GomMergeDecision *self);
GOM_AVAILABLE_IN_ALL
GType             gom_merge_decision_get_entity_type  (GomMergeDecision *self);
GOM_AVAILABLE_IN_ALL
GomDelta         *gom_merge_decision_dup_local_delta  (GomMergeDecision *self);
GOM_AVAILABLE_IN_ALL
GomDelta         *gom_merge_decision_dup_remote_delta (GomMergeDecision *self);
GOM_AVAILABLE_IN_ALL
void              gom_merge_decision_apply            (GomMergeDecision *self,
                                                       GomDelta         *delta);
GOM_AVAILABLE_IN_ALL
void              gom_merge_decision_reject           (GomMergeDecision *self,
                                                       GError           *error);

G_END_DECLS
