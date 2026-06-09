/* gom-meta-version.c
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

#include "gom-meta-version-private.h"

struct _GomMetaVersion
{
  GomEntity parent_instance;
  guint     version;
};

enum
{
  PROP_0,
  PROP_VERSION,
  N_PROPS
};

G_DEFINE_FINAL_TYPE (GomMetaVersion, gom_meta_version, GOM_TYPE_ENTITY)

static GParamSpec *properties[N_PROPS];

static void
gom_meta_version_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GomMetaVersion *self = GOM_META_VERSION (object);

  switch (prop_id)
    {
    case PROP_VERSION:
      g_value_set_uint (value, gom_meta_version_get_version (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_meta_version_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GomMetaVersion *self = GOM_META_VERSION (object);

  switch (prop_id)
    {
    case PROP_VERSION:
      gom_meta_version_set_version (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_meta_version_class_init (GomMetaVersionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gom_meta_version_get_property;
  object_class->set_property = gom_meta_version_set_property;

  properties[PROP_VERSION] =
    g_param_spec_uint ("version", NULL, NULL,
                       0, G_MAXUINT, 0,
                       (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_meta_version_init (GomMetaVersion *self)
{
}

GomMetaVersion *
gom_meta_version_new (void)
{
  return g_object_new (GOM_TYPE_META_VERSION, NULL);
}

guint
gom_meta_version_get_version (GomMetaVersion *self)
{
  g_return_val_if_fail (GOM_IS_META_VERSION (self), 0);

  return self->version;
}

void
gom_meta_version_set_version (GomMetaVersion *self,
                              guint           version)
{
  g_return_if_fail (GOM_IS_META_VERSION (self));

  if (self->version == version)
    return;

  self->version = version;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VERSION]);
}
