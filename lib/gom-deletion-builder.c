/* gom-deletion-builder.c
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

#include <gio/gio.h>

#include "gom-deletion-builder.h"
#include "gom-deletion-private.h"
#include "gom-expression.h"

struct _GomDeletionBuilder
{
  gatomicrefcount ref_count;

  GType          target_entity_type;
  char          *target_relation;
  GomExpression *filter;
  guint64        limit;
  guint          has_limit : 1;
};

G_DEFINE_BOXED_TYPE (GomDeletionBuilder,
                     gom_deletion_builder,
                     gom_deletion_builder_ref,
                     gom_deletion_builder_unref)

static void
gom_deletion_builder_finalize (GomDeletionBuilder *self)
{
  gom_clear_expression (&self->filter);
  g_clear_pointer (&self->target_relation, g_free);
}

GomDeletionBuilder *
gom_deletion_builder_new (void)
{
  GomDeletionBuilder *self = g_new0 (GomDeletionBuilder, 1);

  g_atomic_ref_count_init (&self->ref_count);
  self->target_entity_type = G_TYPE_INVALID;

  return self;
}

GomDeletionBuilder *
gom_deletion_builder_ref (GomDeletionBuilder *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

void
gom_deletion_builder_unref (GomDeletionBuilder *self)
{
  g_return_if_fail (self != NULL);

  if (!g_atomic_ref_count_dec (&self->ref_count))
    return;

  gom_deletion_builder_finalize (self);
  g_free (self);
}

void
gom_deletion_builder_set_target_entity_type (GomDeletionBuilder *self,
                                             GType               target_entity_type)
{
  g_return_if_fail (self != NULL);

  self->target_entity_type = target_entity_type;
}

void
gom_deletion_builder_set_target_relation (GomDeletionBuilder *self,
                                          const char         *target_relation)
{
  g_return_if_fail (self != NULL);

  g_free (self->target_relation);
  self->target_relation = g_strdup (target_relation);
}

void
gom_deletion_builder_set_filter (GomDeletionBuilder *self,
                                 GomExpression      *filter)
{
  g_return_if_fail (self != NULL);

  gom_set_expression (&self->filter, filter);
}

void
gom_deletion_builder_set_limit (GomDeletionBuilder *self,
                                guint64             limit)
{
  g_return_if_fail (self != NULL);

  self->limit = limit;
  self->has_limit = TRUE;
}

/**
 * gom_deletion_builder_build:
 * @self: a [struct@Gom.DeletionBuilder]
 * @error: return location for a [struct@GLib.Error], or %NULL
 *
 * Builds a [class@Gom.Deletion] from the builder state.
 *
 * Either a target entity type or a target relation is required.
 *
 * Returns: (transfer full): a [class@Gom.Deletion], or %NULL with @error set.
 */
GomDeletion *
gom_deletion_builder_build (GomDeletionBuilder  *self,
                            GError             **error)
{
  g_return_val_if_fail (self != NULL, NULL);

  if (self->target_entity_type == G_TYPE_INVALID && self->target_relation == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "GomDeletionBuilder requires a target entity type or relation");
      return NULL;
    }

  return _gom_deletion_new (self->target_entity_type,
                            self->target_relation,
                            self->filter,
                            self->limit,
                            self->has_limit);
}
