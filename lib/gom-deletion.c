/* gom-deletion.c
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
#include "gom-deletion-private.h"
#include "gom-mutation-private.h"

struct _GomDeletion
{
  GomMutation parent_instance;

  GType          target_entity_type;
  char          *target_relation;
  GomExpression *filter;
  guint64        limit;
  guint          has_limit : 1;
};

struct _GomDeletionClass
{
  GomMutationClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomDeletion, gom_deletion, GOM_TYPE_MUTATION)

static void
gom_deletion_finalize (GObject *object)
{
  GomDeletion *self = (GomDeletion *)object;

  gom_clear_expression (&self->filter);
  g_clear_pointer (&self->target_relation, g_free);

  G_OBJECT_CLASS (gom_deletion_parent_class)->finalize (object);
}

static void
gom_deletion_class_init (GomDeletionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_deletion_finalize;
}

static void
gom_deletion_init (GomDeletion *self)
{
  self->target_entity_type = G_TYPE_INVALID;
}

GomDeletion *
_gom_deletion_new (GType          target_entity_type,
                   const char    *target_relation,
                   GomExpression *filter,
                   guint64        limit,
                   gboolean       has_limit)
{
  GomDeletion *deletion = g_object_new (GOM_TYPE_DELETION, NULL);

  deletion->target_entity_type = target_entity_type;
  deletion->target_relation = g_strdup (target_relation);
  gom_set_expression (&deletion->filter, filter);
  deletion->limit = limit;
  deletion->has_limit = has_limit;

  return deletion;
}

GType
_gom_deletion_get_target_entity_type (GomDeletion *self)
{
  g_return_val_if_fail (GOM_IS_DELETION (self), G_TYPE_INVALID);

  return self->target_entity_type;
}

const char *
_gom_deletion_get_target_relation (GomDeletion *self)
{
  g_return_val_if_fail (GOM_IS_DELETION (self), NULL);

  return self->target_relation;
}

GomExpression *
_gom_deletion_get_filter (GomDeletion *self)
{
  g_return_val_if_fail (GOM_IS_DELETION (self), NULL);

  return self->filter;
}

gboolean
_gom_deletion_has_limit (GomDeletion *self)
{
  g_return_val_if_fail (GOM_IS_DELETION (self), FALSE);

  return self->has_limit;
}

guint64
_gom_deletion_get_limit (GomDeletion *self)
{
  g_return_val_if_fail (GOM_IS_DELETION (self), 0);

  return self->limit;
}
