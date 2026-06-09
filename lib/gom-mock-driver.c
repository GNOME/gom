/* gom-mock-driver.c
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

#include <libdex.h>

#include "gom-driver-private.h"
#include "gom-meta.h"
#include "gom-mock-driver-private.h"

struct _GomMockDriver
{
  GomDriver parent_instance;
};

struct _GomMockDriverClass
{
  GomDriverClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomMockDriver, gom_mock_driver, GOM_TYPE_DRIVER)

static char *
gom_mock_driver_dup_uri (GomDriver *driver)
{
  return g_strdup ("mock:");
}

static DexFuture *
gom_mock_driver_query_version (GomDriver *driver)
{
  g_assert (GOM_IS_MOCK_DRIVER (driver));

  return dex_future_new_for_uint (0);
}

static DexFuture *
gom_mock_driver_migrate (GomDriver   *driver,
                         GomRegistry *current,
                         GomRegistry *next)
{
  g_assert (GOM_IS_MOCK_DRIVER (driver));
  g_assert (GOM_IS_REGISTRY (current));
  g_assert (GOM_IS_REGISTRY (next));

  return dex_future_new_true ();
}

static void
gom_mock_driver_class_init (GomMockDriverClass *klass)
{
  GomDriverClass *driver_class = GOM_DRIVER_CLASS (klass);

  driver_class->dup_uri = gom_mock_driver_dup_uri;
  driver_class->query_version = gom_mock_driver_query_version;
  driver_class->migrate = gom_mock_driver_migrate;
}

static void
gom_mock_driver_init (GomMockDriver *self)
{
}

GomMockDriver *
_gom_mock_driver_new (void)
{
  return g_object_new (GOM_TYPE_MOCK_DRIVER, NULL);
}
