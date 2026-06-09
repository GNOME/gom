/* gom-merge-policy.c
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

#include "gom-merge-decision.h"
#include "gom-merge-policy.h"

G_DEFINE_ABSTRACT_TYPE (GomMergePolicy, gom_merge_policy, G_TYPE_OBJECT)

static void
gom_merge_policy_class_init (GomMergePolicyClass *klass)
{
}

static void
gom_merge_policy_init (GomMergePolicy *self)
{
}

/**
 * gom_merge_policy_merge:
 * @self: a [class@Gom.MergePolicy]
 * @decision: a [class@Gom.MergeDecision]
 *
 * Resolves @decision by calling [method@Gom.MergeDecision.apply] or
 * [method@Gom.MergeDecision.reject].
 *
 * The returned future signals when the policy has finished resolving
 * @decision. Its value is ignored.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves when
 *   @decision has been resolved or rejects with error.
 */
DexFuture *
gom_merge_policy_merge (GomMergePolicy   *self,
                        GomMergeDecision *decision)
{
  dex_return_error_if_fail (GOM_IS_MERGE_POLICY (self));
  dex_return_error_if_fail (GOM_IS_MERGE_DECISION (decision));

  if (GOM_MERGE_POLICY_GET_CLASS (self)->merge)
    return GOM_MERGE_POLICY_GET_CLASS (self)->merge (self, decision);

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_NOT_SUPPORTED,
                                "Merge policy does not implement merge");
}
