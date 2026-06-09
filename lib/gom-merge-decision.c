/* gom-merge-decision.c
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

#include "gom-delta.h"
#include "gom-merge-decision-private.h"
#include "gom-repository.h"

typedef enum
{
  GOM_MERGE_DECISION_PENDING,
  GOM_MERGE_DECISION_APPLY,
  GOM_MERGE_DECISION_REJECT,
} GomMergeDecisionState;

struct _GomMergeDecision
{
  GObject parent_instance;

  GomRepository         *repository;
  GomDelta              *local_delta;
  GomDelta              *remote_delta;
  GomDelta              *result_delta;
  GError                *rejection;
  GomMergeDecisionState  state;
};

G_DEFINE_FINAL_TYPE (GomMergeDecision, gom_merge_decision, G_TYPE_OBJECT)

static void
gom_merge_decision_finalize (GObject *object)
{
  GomMergeDecision *self = (GomMergeDecision *)object;

  g_clear_object (&self->repository);
  g_clear_object (&self->local_delta);
  g_clear_object (&self->remote_delta);
  g_clear_object (&self->result_delta);
  g_clear_error (&self->rejection);

  G_OBJECT_CLASS (gom_merge_decision_parent_class)->finalize (object);
}

static void
gom_merge_decision_class_init (GomMergeDecisionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_merge_decision_finalize;
}

static void
gom_merge_decision_init (GomMergeDecision *self)
{
  self->state = GOM_MERGE_DECISION_PENDING;
}

/**
 * gom_merge_decision_new:
 * @repository: a [class@Gom.Repository]
 * @local_delta: (nullable): the local delta
 * @remote_delta: (nullable): the remote delta
 *
 * Creates a merge decision object for a local/remote delta pair.
 *
 * Returns: (transfer full): a [class@Gom.MergeDecision]
 */
GomMergeDecision *
gom_merge_decision_new (GomRepository *repository,
                        GomDelta      *local_delta,
                        GomDelta      *remote_delta)
{
  GomMergeDecision *self;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (local_delta == NULL || GOM_IS_DELTA (local_delta), NULL);
  g_return_val_if_fail (remote_delta == NULL || GOM_IS_DELTA (remote_delta), NULL);
  g_return_val_if_fail (local_delta != NULL || remote_delta != NULL, NULL);

  if (local_delta != NULL && remote_delta != NULL &&
      gom_delta_get_entity_type (local_delta) != gom_delta_get_entity_type (remote_delta))
    return NULL;

  self = g_object_new (GOM_TYPE_MERGE_DECISION, NULL);
  self->repository = g_object_ref (repository);
  self->local_delta = local_delta ? g_object_ref (local_delta) : NULL;
  self->remote_delta = remote_delta ? g_object_ref (remote_delta) : NULL;

  return self;
}

/**
 * gom_merge_decision_dup_repository:
 * @self: a [class@Gom.MergeDecision]
 *
 * Returns: (transfer full): the repository used for this merge
 */
GomRepository *
gom_merge_decision_dup_repository (GomMergeDecision *self)
{
  g_return_val_if_fail (GOM_IS_MERGE_DECISION (self), NULL);
  g_return_val_if_fail (GOM_IS_REPOSITORY (self->repository), NULL);

  return g_object_ref (self->repository);
}

/**
 * gom_merge_decision_get_entity_type:
 * @self: a [class@Gom.MergeDecision]
 *
 * Returns: the entity type being merged.
 */
GType
gom_merge_decision_get_entity_type (GomMergeDecision *self)
{
  g_return_val_if_fail (GOM_IS_MERGE_DECISION (self), G_TYPE_INVALID);

  if (self->remote_delta != NULL)
    return gom_delta_get_entity_type (self->remote_delta);

  if (self->local_delta != NULL)
    return gom_delta_get_entity_type (self->local_delta);

  return G_TYPE_INVALID;
}

/**
 * gom_merge_decision_dup_local_delta:
 * @self: a [class@Gom.MergeDecision]
 *
 * Returns: (transfer full) (nullable): the local delta
 */
GomDelta *
gom_merge_decision_dup_local_delta (GomMergeDecision *self)
{
  g_return_val_if_fail (GOM_IS_MERGE_DECISION (self), NULL);

  return self->local_delta ? g_object_ref (self->local_delta) : NULL;
}

/**
 * gom_merge_decision_dup_remote_delta:
 * @self: a [class@Gom.MergeDecision]
 *
 * Returns: (transfer full) (nullable): the remote delta
 */
GomDelta *
gom_merge_decision_dup_remote_delta (GomMergeDecision *self)
{
  g_return_val_if_fail (GOM_IS_MERGE_DECISION (self), NULL);

  return self->remote_delta ? g_object_ref (self->remote_delta) : NULL;
}

/**
 * gom_merge_decision_apply:
 * @self: a [class@Gom.MergeDecision]
 * @delta: the delta to apply locally
 *
 * Marks @self as resolved by applying @delta.
 */
void
gom_merge_decision_apply (GomMergeDecision *self,
                          GomDelta         *delta)
{
  GType entity_type;

  g_return_if_fail (GOM_IS_MERGE_DECISION (self));
  g_return_if_fail (GOM_IS_DELTA (delta));
  g_return_if_fail (self->state == GOM_MERGE_DECISION_PENDING);

  entity_type = gom_merge_decision_get_entity_type (self);
  g_return_if_fail (entity_type == G_TYPE_INVALID ||
                    entity_type == gom_delta_get_entity_type (delta));

  g_set_object (&self->result_delta, delta);
  self->state = GOM_MERGE_DECISION_APPLY;
}

/**
 * gom_merge_decision_reject:
 * @self: a [class@Gom.MergeDecision]
 * @error: (transfer full): the rejection error
 *
 * Marks @self as rejected with @error.
 */
void
gom_merge_decision_reject (GomMergeDecision *self,
                           GError           *error)
{
  g_autoptr(GError) rejection = error;

  g_return_if_fail (GOM_IS_MERGE_DECISION (self));
  g_return_if_fail (rejection != NULL);
  g_return_if_fail (self->state == GOM_MERGE_DECISION_PENDING);

  self->rejection = g_steal_pointer (&rejection);
  self->state = GOM_MERGE_DECISION_REJECT;
}

GomDelta *
_gom_merge_decision_await (GomMergeDecision  *self,
                           DexFuture         *future,
                           GError           **error)
{
  g_return_val_if_fail (GOM_IS_MERGE_DECISION (self), NULL);
  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);

  if (!dex_await (future, error))
    return NULL;

  switch (self->state)
    {
    case GOM_MERGE_DECISION_APPLY:
      g_assert (GOM_IS_DELTA (self->result_delta));
      return g_object_ref (self->result_delta);

    case GOM_MERGE_DECISION_REJECT:
      g_assert (self->rejection != NULL);
      if (error != NULL)
        *error = g_error_copy (self->rejection);
      return NULL;

    case GOM_MERGE_DECISION_PENDING:
    default:
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Merge policy completed without resolving the merge decision");
      return NULL;
    }

  g_assert_not_reached ();
}
