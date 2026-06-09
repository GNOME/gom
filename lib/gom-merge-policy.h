/* gom-merge-policy.h
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

#include <libdex.h>

#include "gom-types.h"
#include "gom-version-macros.h"

G_BEGIN_DECLS

#define GOM_TYPE_MERGE_POLICY (gom_merge_policy_get_type())

GOM_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GomMergePolicy, gom_merge_policy, GOM, MERGE_POLICY, GObject)

struct _GomMergePolicyClass
{
  GObjectClass parent_class;

  DexFuture *(*merge) (GomMergePolicy   *self,
                       GomMergeDecision *decision);

  /*< private >*/
  gpointer _reserved[14];
};

GOM_AVAILABLE_IN_ALL
DexFuture *gom_merge_policy_merge (GomMergePolicy   *self,
                                   GomMergeDecision *decision) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
