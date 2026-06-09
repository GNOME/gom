/* gom-mutation-result.c
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

#include "gom-mutation-result-private.h"

enum
{
  PROP_0,
  PROP_AFFECTED_ROWS,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GType
gom_mutation_result_get_item_type (GListModel *model)
{
  return GOM_TYPE_RECORD;
}

static guint
gom_mutation_result_get_n_items (GListModel *model)
{
  GomMutationResult *self = GOM_MUTATION_RESULT (model);

  return self->records != NULL
    ? g_list_model_get_n_items (G_LIST_MODEL (self->records))
    : 0;
}

static gpointer
gom_mutation_result_get_item (GListModel *model,
                              guint       position)
{
  GomMutationResult *self = GOM_MUTATION_RESULT (model);

  if (self->records == NULL)
    return NULL;

  return g_list_model_get_item (G_LIST_MODEL (self->records), position);
}

static void
gom_mutation_result_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gom_mutation_result_get_item_type;
  iface->get_n_items = gom_mutation_result_get_n_items;
  iface->get_item = gom_mutation_result_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GomMutationResult,
                         gom_mutation_result,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gom_mutation_result_list_model_init))

static void
gom_mutation_result_finalize (GObject *object)
{
  GomMutationResult *self = GOM_MUTATION_RESULT (object);

  g_clear_object (&self->records);

  G_OBJECT_CLASS (gom_mutation_result_parent_class)->finalize (object);
}

static void
gom_mutation_result_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GomMutationResult *self = GOM_MUTATION_RESULT (object);

  switch (prop_id)
    {
    case PROP_AFFECTED_ROWS:
      g_value_set_uint64 (value, self->affected_rows);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_mutation_result_class_init (GomMutationResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_mutation_result_finalize;
  object_class->get_property = gom_mutation_result_get_property;

  properties[PROP_AFFECTED_ROWS] =
    g_param_spec_uint64 ("affected-rows", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_mutation_result_init (GomMutationResult *self)
{
  self->records = g_list_store_new (GOM_TYPE_RECORD);
}

GomMutationResult *
_gom_mutation_result_new (void)
{
  return g_object_new (GOM_TYPE_MUTATION_RESULT, NULL);
}

void
_gom_mutation_result_append_record (GomMutationResult *self,
                                    GomRecord         *record,
                                    guint64            affected_rows)
{
  g_return_if_fail (GOM_IS_MUTATION_RESULT (self));
  g_return_if_fail (GOM_IS_RECORD (record));

  g_list_store_append (self->records, record);

  self->affected_rows += affected_rows;
}

guint64
gom_mutation_result_get_affected_rows (GomMutationResult *self)
{
  g_return_val_if_fail (GOM_IS_MUTATION_RESULT (self), 0);

  return self->affected_rows;
}
