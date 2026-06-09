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

#include "gom-driver-options.h"

struct _GomDriverOptions
{
  GObject  parent_instance;
  GBytes  *encryption_key;
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
}

/**
 * gom_driver_options_new:
 *
 * Creates options to pass to [func@Gom.Driver.open_with_options].
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
