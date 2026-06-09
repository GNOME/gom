/* gom-delta.c
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

typedef struct
{
  char   *property_name;
  GValue  original_value;
  GValue  current_value;
  guint   has_original : 1;
  guint   has_current : 1;
} GomDeltaEntry;

struct _GomDelta
{
  GObject       parent_instance;
  GType         entity_type;
  GomDeltaKind  kind;
  GPtrArray    *entries;
};

struct _GomDeltaClass
{
  GObjectClass parent_class;
};

static void gom_delta_entry_clear (GomDeltaEntry *entry);

G_DEFINE_FINAL_TYPE (GomDelta, gom_delta, G_TYPE_OBJECT)

static void
gom_delta_entry_clear (GomDeltaEntry *entry)
{
  if (entry == NULL)
    return;

  g_clear_pointer (&entry->property_name, g_free);

  if (G_IS_VALUE (&entry->original_value))
    g_value_unset (&entry->original_value);

  if (G_IS_VALUE (&entry->current_value))
    g_value_unset (&entry->current_value);
}

static void
gom_delta_finalize (GObject *object)
{
  GomDelta *self = GOM_DELTA (object);

  g_clear_pointer (&self->entries, g_ptr_array_unref);

  G_OBJECT_CLASS (gom_delta_parent_class)->finalize (object);
}

static void
gom_delta_class_init (GomDeltaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_delta_finalize;
}

static void
gom_delta_init (GomDelta *self)
{
  self->entity_type = G_TYPE_INVALID;
  self->entries = g_ptr_array_new_with_free_func ((GDestroyNotify)gom_delta_entry_clear);
}

GomDelta *
gom_delta_new (GType        entity_type,
               GomDeltaKind kind)
{
  GomDelta *self;

  self = g_object_new (GOM_TYPE_DELTA, NULL);
  self->entity_type = entity_type;
  self->kind = kind;

  return self;
}

GomDeltaKind
gom_delta_get_kind (GomDelta *self)
{
  g_return_val_if_fail (GOM_IS_DELTA (self), GOM_DELTA_KIND_UPDATE);

  return self->kind;
}

GType
gom_delta_get_entity_type (GomDelta *self)
{
  g_return_val_if_fail (GOM_IS_DELTA (self), G_TYPE_INVALID);

  return self->entity_type;
}

gboolean
gom_delta_is_empty (GomDelta *self)
{
  g_return_val_if_fail (GOM_IS_DELTA (self), TRUE);

  return self->entries == NULL || self->entries->len == 0;
}

guint
gom_delta_get_n_changes (GomDelta *self)
{
  g_return_val_if_fail (GOM_IS_DELTA (self), 0);

  return self->entries != NULL ? self->entries->len : 0;
}

const char *
gom_delta_get_property_name (GomDelta *self,
                             guint     index)
{
  GomDeltaEntry *entry;

  g_return_val_if_fail (GOM_IS_DELTA (self), NULL);

  if (self->entries == NULL || index >= self->entries->len)
    return NULL;

  entry = g_ptr_array_index (self->entries, index);
  return entry != NULL ? entry->property_name : NULL;
}

static gboolean
gom_delta_copy_value (const GValue *src,
                      GValue       *dest)
{
  if (src == NULL || !G_IS_VALUE (src))
    return FALSE;

  if (G_IS_VALUE (dest))
    g_value_unset (dest);

  g_value_init (dest, G_VALUE_TYPE (src));
  g_value_copy (src, dest);
  return TRUE;
}

gboolean
gom_delta_get_original_value (GomDelta *self,
                              guint     index,
                              GValue   *value)
{
  GomDeltaEntry *entry;

  g_return_val_if_fail (GOM_IS_DELTA (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (self->entries == NULL || index >= self->entries->len)
    return FALSE;

  entry = g_ptr_array_index (self->entries, index);
  if (entry == NULL || !entry->has_original)
    return FALSE;

  return gom_delta_copy_value (&entry->original_value, value);
}

gboolean
gom_delta_get_current_value (GomDelta *self,
                             guint     index,
                             GValue   *value)
{
  GomDeltaEntry *entry;

  g_return_val_if_fail (GOM_IS_DELTA (self), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (self->entries == NULL || index >= self->entries->len)
    return FALSE;

  entry = g_ptr_array_index (self->entries, index);
  if (entry == NULL || !entry->has_current)
    return FALSE;

  return gom_delta_copy_value (&entry->current_value, value);
}

void
gom_delta_add_property (GomDelta     *self,
                        const char   *property_name,
                        const GValue *original_value,
                        const GValue *current_value)
{
  GomDeltaEntry *entry;

  g_return_if_fail (GOM_IS_DELTA (self));
  g_return_if_fail (property_name != NULL);

  entry = g_new0 (GomDeltaEntry, 1);
  entry->property_name = g_strdup (property_name);

  if (original_value != NULL && G_IS_VALUE (original_value))
    {
      entry->has_original = TRUE;
      gom_delta_copy_value (original_value, &entry->original_value);
    }

  if (current_value != NULL && G_IS_VALUE (current_value))
    {
      entry->has_current = TRUE;
      gom_delta_copy_value (current_value, &entry->current_value);
    }

  g_ptr_array_add (self->entries, entry);
}
