/* gom-sqlite-pool.c
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

#include "gom-sqlite-connection-private.h"
#include "gom-sqlite-lease-private.h"
#include "gom-sqlite-pool-private.h"

struct _GomSqlitePool
{
  GObject        parent_instance;
  char          *uri;
  GBytes        *encryption_key;
  GMutex         mutex;
  GPtrArray     *idle_connections;
  DexThreadPool *thread_pool;
  DexLimiter    *lease_limiter;
  DexLimiter    *open_limiter;
};

struct _GomSqlitePoolClass
{
  GObjectClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomSqlitePool, gom_sqlite_pool, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_URI,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gom_sqlite_pool_finalize (GObject *object)
{
  GomSqlitePool *self = (GomSqlitePool *)object;
  g_autoptr(DexFuture) close_future = NULL;

  if (self->lease_limiter != NULL)
    dex_limiter_close (self->lease_limiter);

  if (self->open_limiter != NULL)
    dex_limiter_close (self->open_limiter);

  if (self->thread_pool != NULL)
    close_future = dex_thread_pool_close (self->thread_pool, DEX_THREAD_POOL_SHUTDOWN_DRAIN);

  g_mutex_clear (&self->mutex);

  g_clear_pointer (&self->idle_connections, g_ptr_array_unref);
  dex_clear (&self->open_limiter);
  dex_clear (&self->lease_limiter);
  dex_clear (&self->thread_pool);
  g_clear_pointer (&self->encryption_key, g_bytes_unref);
  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (gom_sqlite_pool_parent_class)->finalize (object);
}

static void
gom_sqlite_pool_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GomSqlitePool *self = GOM_SQLITE_POOL (object);

  switch (prop_id)
    {
    case PROP_URI:
      self->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sqlite_pool_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GomSqlitePool *self = GOM_SQLITE_POOL (object);

  switch (prop_id)
    {
    case PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sqlite_pool_class_init (GomSqlitePoolClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_sqlite_pool_finalize;
  object_class->set_property = gom_sqlite_pool_set_property;
  object_class->get_property = gom_sqlite_pool_get_property;

  properties[PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_sqlite_pool_init (GomSqlitePool *self)
{
  g_mutex_init (&self->mutex);

  self->idle_connections = g_ptr_array_new_with_free_func (g_object_unref);
  self->thread_pool = dex_thread_pool_new (GOM_SQLITE_POOL_OPEN_THREADS);
  self->lease_limiter = dex_limiter_new (GOM_SQLITE_POOL_MAX_LEASES);
  self->open_limiter = dex_limiter_new (GOM_SQLITE_POOL_MAX_CONNECTION_OPENS);
}

GomSqlitePool *
gom_sqlite_pool_new (const char *uri,
                     GBytes     *encryption_key)
{
  GomSqlitePool *self;

  g_return_val_if_fail (uri != NULL, NULL);

  self = g_object_new (GOM_TYPE_SQLITE_POOL,
                       "uri", uri,
                       NULL);
  if (encryption_key != NULL)
    self->encryption_key = g_bytes_ref (encryption_key);
  return self;
}

void
gom_sqlite_pool_return_connection (GomSqlitePool       *self,
                                   GomSqliteConnection *connection)
{
  g_return_if_fail (GOM_IS_SQLITE_POOL (self));
  g_return_if_fail (GOM_IS_SQLITE_CONNECTION (connection));

  g_mutex_lock (&self->mutex);
  g_ptr_array_add (self->idle_connections, g_object_ref (connection));
  g_mutex_unlock (&self->mutex);

  dex_limiter_release (self->lease_limiter);
}

static DexFuture *
gom_sqlite_pool_open_complete_cb (DexFuture *completed,
                                  gpointer   user_data)
{
  GomSqlitePool *self = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteLease *lease;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SQLITE_POOL (self));

  if (!(value = dex_future_get_value (completed, &error)))
    {
      dex_limiter_release (self->lease_limiter);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_SQLITE_CONNECTION));

  if (!(lease = gom_sqlite_lease_new (g_value_get_object (value), self)))
    {
      dex_limiter_release (self->lease_limiter);
      return dex_future_new_reject (G_IO_ERROR,
                                    G_IO_ERROR_FAILED,
                                    "Failed to create SQLite lease");
    }

  return dex_future_new_take_object (lease);
}

static DexFuture *
gom_sqlite_pool_acquire_permit_cb (DexFuture *completed,
                                   gpointer   user_data)
{
  GomSqlitePool *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomSqliteConnection) connection = NULL;
  GomSqliteLease *lease;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SQLITE_POOL (self));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_mutex_lock (&self->mutex);
  if (self->idle_connections->len > 0)
    connection = g_ptr_array_steal_index (self->idle_connections, self->idle_connections->len - 1);
  g_mutex_unlock (&self->mutex);

  if (connection != NULL)
    {
      if (!(lease = gom_sqlite_lease_new (connection, self)))
        {
          dex_limiter_release (self->lease_limiter);
          return dex_future_new_reject (G_IO_ERROR,
                                        G_IO_ERROR_FAILED,
                                        "Failed to create SQLite lease");
        }

      return dex_future_new_take_object (lease);
    }

  return dex_future_finally (gom_sqlite_connection_new (self->uri,
                                                        self->encryption_key,
                                                        self->thread_pool,
                                                        self->open_limiter),
                             gom_sqlite_pool_open_complete_cb,
                             g_object_ref (self),
                             g_object_unref);
}

DexFuture *
gom_sqlite_pool_acquire (GomSqlitePool *self)
{
  dex_return_error_if_fail (GOM_IS_SQLITE_POOL (self));

  return dex_future_then (dex_limiter_acquire (self->lease_limiter),
                          gom_sqlite_pool_acquire_permit_cb,
                          g_object_ref (self),
                          g_object_unref);
}

void
gom_sqlite_pool_clear_idle (GomSqlitePool *self)
{
  g_return_if_fail (GOM_IS_SQLITE_POOL (self));

  g_mutex_lock (&self->mutex);
  g_ptr_array_set_size (self->idle_connections, 0);
  g_mutex_unlock (&self->mutex);
}

void
gom_sqlite_pool_set_encryption_key (GomSqlitePool *self,
                                    GBytes        *encryption_key)
{
  g_return_if_fail (GOM_IS_SQLITE_POOL (self));

  g_mutex_lock (&self->mutex);

  g_clear_pointer (&self->encryption_key, g_bytes_unref);
  if (encryption_key != NULL)
    self->encryption_key = g_bytes_ref (encryption_key);

  g_ptr_array_set_size (self->idle_connections, 0);

  g_mutex_unlock (&self->mutex);
}
