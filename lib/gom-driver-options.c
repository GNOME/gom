/* gom-driver-options.c
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

#include "gom-driver-options-private.h"

struct _GomDriverOptions
{
  GObject     parent_instance;
  GBytes     *encryption_key;
  GHashTable *values;
};

struct _GomDriverOptionsClass
{
  GObjectClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomDriverOptions, gom_driver_options, G_TYPE_OBJECT)

static void
gom_driver_options_finalize (GObject *object)
{
  GomDriverOptions *self = (GomDriverOptions *)object;

  g_clear_pointer (&self->encryption_key, g_bytes_unref);
  g_clear_pointer (&self->values, g_hash_table_unref);

  G_OBJECT_CLASS (gom_driver_options_parent_class)->finalize (object);
}

static void
gom_driver_options_class_init (GomDriverOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_driver_options_finalize;
}

static void
gom_driver_options_init (GomDriverOptions *self)
{
  self->values = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free, (GDestroyNotify)g_variant_unref);
}

/**
 * gom_driver_options_new:
 *
 * Creates options to pass to [func@Gom.Driver.open_with_options].
 * A driver reads the options when it opens; later changes do not affect it.
 *
 * The SQLite driver recognizes `sqlite.synchronous` as a string with the
 * values `OFF`, `NORMAL`, `FULL`, or `EXTRA`, and `sqlite.foreign-keys` as a
 * boolean. It rejects invalid `sqlite.` options when opening.
 *
 * Returns: (transfer full): a [class@Gom.DriverOptions]
 */
GomDriverOptions *
gom_driver_options_new (void)
{
  return g_object_new (GOM_TYPE_DRIVER_OPTIONS, NULL);
}

/**
 * gom_driver_options_set_encryption_key:
 * @self: a [class@Gom.DriverOptions]
 * @key: (nullable): encryption key bytes
 *
 * Sets the encryption key bytes to use when opening a driver. The bytes are
 * copied by reference and never encoded into the database URI.
 */
void
gom_driver_options_set_encryption_key (GomDriverOptions *self,
                                       GBytes           *key)
{
  g_return_if_fail (GOM_IS_DRIVER_OPTIONS (self));

  if (key != NULL)
    g_bytes_ref (key);

  g_clear_pointer (&self->encryption_key, g_bytes_unref);
  self->encryption_key = key;
}

/**
 * gom_driver_options_dup_encryption_key:
 * @self: a [class@Gom.DriverOptions]
 *
 * Gets the encryption key bytes, if one has been configured.
 *
 * Returns: (transfer full) (nullable): encryption key bytes
 */
GBytes *
gom_driver_options_dup_encryption_key (GomDriverOptions *self)
{
  g_return_val_if_fail (GOM_IS_DRIVER_OPTIONS (self), NULL);

  if (self->encryption_key == NULL)
    return NULL;

  return g_bytes_ref (self->encryption_key);
}

/**
 * gom_driver_options_set_boolean:
 * @self: a [class@Gom.DriverOptions]
 * @key: a namespaced option key
 * @value: the boolean value
 *
 * Sets a boolean driver option. Setting a key replaces its previous value.
 * Backend-specific options are validated when the driver is opened.
 */
void
gom_driver_options_set_boolean (GomDriverOptions *self,
                                const char       *key,
                                gboolean          value)
{
  g_return_if_fail (GOM_IS_DRIVER_OPTIONS (self));
  g_return_if_fail (key != NULL && *key != '\0');

  g_hash_table_replace (self->values, g_strdup (key),
                        g_variant_ref_sink (g_variant_new_boolean (value)));
}

/**
 * gom_driver_options_get_boolean:
 * @self: a [class@Gom.DriverOptions]
 * @key: a namespaced option key
 * @value: (out) (optional): return location for the value
 *
 * Looks up a boolean option. If @value is non-%NULL, it is set to %FALSE
 * when the key is absent or has another type.
 *
 * Returns: %TRUE if @key is set to a boolean value
 */
gboolean
gom_driver_options_get_boolean (GomDriverOptions *self,
                                const char       *key,
                                gboolean         *value)
{
  GVariant *stored;

  g_return_val_if_fail (GOM_IS_DRIVER_OPTIONS (self), FALSE);
  g_return_val_if_fail (key != NULL && *key != '\0', FALSE);

  if (value != NULL)
    *value = FALSE;

  stored = g_hash_table_lookup (self->values, key);
  if (stored == NULL || !g_variant_is_of_type (stored, G_VARIANT_TYPE_BOOLEAN))
    return FALSE;

  if (value != NULL)
    *value = g_variant_get_boolean (stored);

  return TRUE;
}

/**
 * gom_driver_options_set_string:
 * @self: a [class@Gom.DriverOptions]
 * @key: a namespaced option key
 * @value: the UTF-8 string value
 *
 * Sets a string driver option. Setting a key replaces its previous value.
 * Backend-specific options are validated when the driver is opened.
 */
void
gom_driver_options_set_string (GomDriverOptions *self,
                               const char       *key,
                               const char       *value)
{
  g_return_if_fail (GOM_IS_DRIVER_OPTIONS (self));
  g_return_if_fail (key != NULL && *key != '\0');
  g_return_if_fail (value != NULL);
  g_return_if_fail (g_utf8_validate (value, -1, NULL));

  g_hash_table_replace (self->values, g_strdup (key),
                        g_variant_ref_sink (g_variant_new_string (value)));
}

/**
 * gom_driver_options_dup_string:
 * @self: a [class@Gom.DriverOptions]
 * @key: a namespaced option key
 * @value: (out) (optional) (transfer full): return location for a copied string
 *
 * Looks up a string option. If @value is non-%NULL, it is set to %NULL
 * when the key is absent or has another type. Free a returned string with g_free().
 *
 * Returns: %TRUE if @key is set to a string value
 */
gboolean
gom_driver_options_dup_string (GomDriverOptions  *self,
                               const char        *key,
                               char             **value)
{
  GVariant *stored;

  g_return_val_if_fail (GOM_IS_DRIVER_OPTIONS (self), FALSE);
  g_return_val_if_fail (key != NULL && *key != '\0', FALSE);

  if (value != NULL)
    *value = NULL;

  stored = g_hash_table_lookup (self->values, key);
  if (stored == NULL || !g_variant_is_of_type (stored, G_VARIANT_TYPE_STRING))
    return FALSE;

  if (value != NULL)
    *value = g_strdup (g_variant_get_string (stored, NULL));

  return TRUE;
}

/**
 * gom_driver_options_unset:
 * @self: a [class@Gom.DriverOptions]
 * @key: a namespaced option key
 *
 * Removes an option. Removing an absent key has no effect.
 */
void
gom_driver_options_unset (GomDriverOptions *self,
                          const char       *key)
{
  g_return_if_fail (GOM_IS_DRIVER_OPTIONS (self));
  g_return_if_fail (key != NULL && *key != '\0');

  g_hash_table_remove (self->values, key);
}

GHashTable *
_gom_driver_options_dup_values (GomDriverOptions *self)
{
  GHashTable *copy;
  GHashTableIter iter;
  gpointer key;
  gpointer value;

  g_return_val_if_fail (GOM_IS_DRIVER_OPTIONS (self), NULL);

  copy = g_hash_table_new_full (g_str_hash, g_str_equal,
                                g_free, (GDestroyNotify)g_variant_unref);
  g_hash_table_iter_init (&iter, self->values);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_hash_table_insert (copy, g_strdup (key), g_variant_ref (value));

  return copy;
}
