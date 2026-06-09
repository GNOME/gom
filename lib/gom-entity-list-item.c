/* gom-entity-list-item.c
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

#include "gom-entity.h"
#include "gom-entity-list-item-private.h"

struct _GomEntityListItem
{
  GObject parent_instance;

  GomEntity *item;
  guint      position;
  guint      loading : 1;
};

enum
{
  PROP_0,
  PROP_ITEM,
  PROP_POSITION,
  PROP_LOADING,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (GomEntityListItem, gom_entity_list_item, G_TYPE_OBJECT)

static void
gom_entity_list_item_finalize (GObject *object)
{
  GomEntityListItem *self = GOM_ENTITY_LIST_ITEM (object);

  g_clear_object (&self->item);

  G_OBJECT_CLASS (gom_entity_list_item_parent_class)->finalize (object);
}

static void
gom_entity_list_item_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GomEntityListItem *self = GOM_ENTITY_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_POSITION:
      g_value_set_uint (value, self->position);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, self->loading);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_entity_list_item_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GomEntityListItem *self = GOM_ENTITY_LIST_ITEM (object);

  switch (prop_id)
    {
    case PROP_POSITION:
      self->position = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_entity_list_item_class_init (GomEntityListItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_entity_list_item_finalize;
  object_class->get_property = gom_entity_list_item_get_property;
  object_class->set_property = gom_entity_list_item_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         GOM_TYPE_ENTITY,
                         (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  properties[PROP_POSITION] =
    g_param_spec_uint ("position", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_LOADING] =
    g_param_spec_boolean ("loading", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_entity_list_item_init (GomEntityListItem *self)
{
}

/**
 * gom_entity_list_item_new:
 * @position: the row position represented by the wrapper
 *
 * Creates a new lazy row wrapper for @position.
 *
 * Returns: (transfer full): a new [class@Gom.EntityListItem]
 */
GomEntityListItem *
gom_entity_list_item_new (guint position)
{
  return g_object_new (GOM_TYPE_ENTITY_LIST_ITEM,
                       "position", position,
                       NULL);
}

void
_gom_entity_list_item_set_item (GomEntityListItem *self,
                                GomEntity         *item)
{
  g_return_if_fail (GOM_IS_ENTITY_LIST_ITEM (self));
  g_return_if_fail (item == NULL || GOM_IS_ENTITY (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

void
_gom_entity_list_item_set_loading (GomEntityListItem *self,
                                   gboolean           loading)
{
  g_return_if_fail (GOM_IS_ENTITY_LIST_ITEM (self));

  loading = !!loading;

  if (self->loading != loading)
    {
      self->loading = loading;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
    }
}

/**
 * gom_entity_list_item_dup_item:
 * @self: a [class@Gom.EntityListItem]
 *
 * Returns: (transfer full) (nullable): the loaded [class@Gom.Entity], if any
 */
GomEntity *
gom_entity_list_item_dup_item (GomEntityListItem *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_ITEM (self), NULL);

  return self->item ? g_object_ref (self->item) : NULL;
}

guint
gom_entity_list_item_get_position (GomEntityListItem *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_ITEM (self), 0);

  return self->position;
}

gboolean
gom_entity_list_item_get_loading (GomEntityListItem *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_ITEM (self), FALSE);

  return self->loading;
}
