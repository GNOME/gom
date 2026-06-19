/* gom-sqlite-session.c
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

#include <sqlite3.h>

#include "gom-cursor-private.h"
#include "gom-entity-private.h"
#include "gom-query-private.h"
#include "gom-repository-private.h"
#include "gom-sqlite-connection-private.h"
#include "gom-sqlite-driver-private.h"
#include "gom-sqlite-lease-private.h"
#include "gom-sqlite-session-private.h"
#include "gom-trace-private.h"

struct _GomSqliteSession
{
  GomSession parent_instance;

  GomSqliteLeaseState *lease_state;
  GQueue               all_entities;
  GQueue               pending_entities;
  GQueue               dirty_entities;
  GHashTable          *entities_by_key;
  DexLimiter          *write_limiter;
  gboolean             flushing;
};

struct _GomSqliteSessionClass
{
  GomSessionClass parent_class;
};

typedef struct
{
  GomSqliteSession *session;
} GomSqliteSessionAttachState;

typedef struct
{
  GomSqliteSession *session;
  gboolean          rollback;
} GomSqliteSessionCloseState;

typedef struct
{
  gatomicrefcount   ref_count;
  GomSqliteSession *session;
} GomSqliteSessionFlushState;

static void       gom_sqlite_session_finalize                  (GObject                    *object);
static GomEntity *gom_sqlite_session_lookup_entity             (GomSession                 *session,
                                                                const char                 *entity_key);
static GomEntity *gom_sqlite_session_register_entity           (GomSession                 *session,
                                                                GomEntity                  *entity,
                                                                char                       *entity_key);
static void       gom_sqlite_session_unregister_pending_entity (GomSession                 *session,
                                                                GomEntity                  *entity);
static gboolean   gom_sqlite_session_rekey_entity_identity     (GomSession                 *session,
                                                                GomEntity                  *entity,
                                                                char                       *entity_key);
static void       gom_sqlite_session_unregister_entity         (GomSession                 *session,
                                                                GomEntity                  *entity);
static void       gom_sqlite_session_clear_entities_vfunc      (GomSession                 *session);
static void       gom_sqlite_session_track_entity_changes      (GomSession                 *session,
                                                                GomEntity                  *entity);
static void       gom_sqlite_session_untrack_entity_changes    (GomSession                 *session,
                                                                GomEntity                  *entity);
static void       gom_sqlite_session_accept_entity_changes     (GomSession                 *session,
                                                                GomEntity                  *entity,
                                                                GomDelta                   *delta);
static void       gom_sqlite_session_mark_entity_dirty         (GomSession                 *session,
                                                                GomEntity                  *entity);
static DexFuture *gom_sqlite_session_persist                   (GomSession                 *session,
                                                                GomEntity                  *entity);
static DexFuture *gom_sqlite_session_flush                     (GomSession                 *session);
static DexFuture *gom_sqlite_session_flush_next                (GomSqliteSessionFlushState *state);
static DexFuture *gom_sqlite_session_attach_cursor_cb          (DexFuture                  *completed,
                                                                gpointer                    user_data);
static DexFuture *gom_sqlite_session_query                     (GomSession                 *session,
                                                                GomQuery                   *query);
static DexFuture *gom_sqlite_session_mutate                    (GomSession                 *session,
                                                                GomMutation                *mutation);
static DexFuture *gom_sqlite_session_close_thread              (gpointer                    user_data);
static DexFuture *gom_sqlite_session_close_complete_cb         (DexFuture                  *completed,
                                                                gpointer                    user_data);
static DexFuture *gom_sqlite_session_commit_after_flush_cb     (DexFuture                  *completed,
                                                                gpointer                    user_data);
static DexFuture *gom_sqlite_session_commit                    (GomSession                 *session);
static DexFuture *gom_sqlite_session_rollback                  (GomSession                 *session);

static void
gom_sqlite_session_release_write_permit (GomSqliteSession *self)
{
  g_assert (GOM_IS_SQLITE_SESSION (self));

  if (self->write_limiter != NULL)
    {
      dex_limiter_release (self->write_limiter);
      dex_clear (&self->write_limiter);
    }
}

static void
gom_sqlite_session_attach_state_free (GomSqliteSessionAttachState *state)
{
  g_clear_object (&state->session);
  g_free (state);
}

static void
gom_sqlite_session_close_state_free (GomSqliteSessionCloseState *state)
{
  g_clear_object (&state->session);
  g_free (state);
}

G_DEFINE_FINAL_TYPE (GomSqliteSession, gom_sqlite_session, GOM_TYPE_SESSION)

static void
gom_sqlite_session_clear_entities (GomSqliteSession *self)
{
  gint identity_entries = 0;
  gint pending_entries = g_queue_get_length (&self->pending_entities);

  while (self->all_entities.head != NULL)
    {
      GList *link = self->all_entities.head;
      GomEntity *entity = link->data;
      gboolean was_pending = FALSE;
      g_autofree char *entity_key = NULL;

      g_queue_unlink (&self->all_entities, link);

      if (entity != NULL)
        {
          was_pending = _gom_entity_is_pending (entity);
          entity_key = _gom_entity_dup_session_key (entity);

          g_object_ref (entity);
          gom_sqlite_session_untrack_entity_changes (GOM_SESSION (self), entity);

          if (was_pending)
            {
              g_queue_unlink (&self->pending_entities, _gom_entity_get_pending_link (entity));
              _gom_entity_set_pending (entity, FALSE);
              g_object_unref (entity);
            }

          if (entity_key != NULL && g_hash_table_remove (self->entities_by_key, entity_key))
            identity_entries++;

          _gom_entity_detach (entity);
          g_object_unref (entity);
        }
    }

  if (self->entities_by_key != NULL && g_hash_table_size (self->entities_by_key) > 0)
    {
      GHashTableIter iter;
      gpointer value;
      gint remaining_entries;

      remaining_entries = g_hash_table_size (self->entities_by_key);

      g_hash_table_iter_init (&iter, self->entities_by_key);
      while (g_hash_table_iter_next (&iter, NULL, &value))
        {
          GomEntity *entity = value;

          if (!GOM_IS_ENTITY (entity))
            continue;

          gom_sqlite_session_untrack_entity_changes (GOM_SESSION (self), entity);
          _gom_entity_detach (entity);
        }

      g_hash_table_remove_all (self->entities_by_key);
      identity_entries += remaining_entries;
    }

  while (!g_queue_is_empty (&self->pending_entities))
    {
      GomEntity *entity = g_queue_peek_head (&self->pending_entities);

      if (entity != NULL)
        {
          gom_sqlite_session_untrack_entity_changes (GOM_SESSION (self), entity);
          g_queue_unlink (&self->pending_entities, _gom_entity_get_pending_link (entity));
          _gom_entity_set_pending (entity, FALSE);
          _gom_entity_detach (entity);
          g_object_unref (entity);
        }
    }

  while (!g_queue_is_empty (&self->dirty_entities))
    {
      GomEntity *entity = g_queue_peek_head (&self->dirty_entities);

      if (entity != NULL)
        {
          g_queue_unlink (&self->dirty_entities, _gom_entity_get_dirty_link (entity));
          _gom_entity_set_dirty (entity, FALSE);
          g_object_unref (entity);
        }
    }

  gom_trace_counter_add (GOM_TRACE_COUNTER_IDENTITY_ENTRIES, -identity_entries);
  gom_trace_counter_add (GOM_TRACE_COUNTER_PENDING_ENTITIES, -pending_entries);
}

static void
gom_sqlite_session_init (GomSqliteSession *self)
{
  g_queue_init (&self->all_entities);
  g_queue_init (&self->pending_entities);
  g_queue_init (&self->dirty_entities);
  self->entities_by_key = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

static void
gom_sqlite_session_finalize (GObject *object)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (object);
  GomSqliteConnection *connection = NULL;
  sqlite3 *db = NULL;

  if (self->lease_state != NULL)
    {
      if ((connection = gom_sqlite_lease_state_get_connection (self->lease_state)))
        db = gom_sqlite_connection_get_native (connection);
    }

  if (!self->parent_instance.closed && db != NULL)
    {
      GOM_TRACE_MARK ("Session",
                      "rollback",
                      "session=%" G_GINT64_FORMAT " implicit-finalize",
                      self->parent_instance.id);
      gom_sqlite_driver_exec_sql (db, "ROLLBACK", "rollback session transaction", NULL);
    }

  gom_sqlite_session_release_write_permit (self);

  if (self->entities_by_key != NULL)
    gom_sqlite_session_clear_entities (self);

  g_clear_pointer (&self->entities_by_key, g_hash_table_unref);
  if (self->lease_state != NULL)
    {
      gom_sqlite_lease_state_unref (self->lease_state);
      self->lease_state = NULL;
    }

  G_OBJECT_CLASS (gom_sqlite_session_parent_class)->finalize (object);
}

static GomEntity *
gom_sqlite_session_lookup_entity (GomSession *session,
                                  const char *entity_key)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomEntity *entity;

  g_assert (GOM_IS_SQLITE_SESSION (self));

  entity = g_hash_table_lookup (self->entities_by_key, entity_key);
  return entity ? g_object_ref (entity) : NULL;
}

static GomEntity *
gom_sqlite_session_register_entity (GomSession *session,
                                    GomEntity  *entity,
                                    char       *entity_key)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomSession *entity_session;
  GomEntity *existing;

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (entity_key != NULL);

  if ((existing = g_hash_table_lookup (self->entities_by_key, entity_key)))
    {
      g_free (entity_key);
      return g_object_ref (existing);
    }

  _gom_entity_attach (entity, GOM_SESSION (self), entity_key);
  entity_session = _gom_entity_dup_session (entity);
  g_assert (entity_session != NULL);
  g_assert (entity_session == GOM_SESSION (self));
  g_clear_object (&entity_session);

  if (self->parent_instance.repository != NULL)
    gom_entity_set_repository (entity, self->parent_instance.repository);

  g_queue_push_tail_link (&self->all_entities, _gom_entity_get_session_link (entity));
  g_hash_table_insert (self->entities_by_key, entity_key, g_object_ref (entity));

  gom_sqlite_session_track_entity_changes (session, entity);

  gom_trace_counter_add (GOM_TRACE_COUNTER_IDENTITY_ENTRIES, 1);

  return entity;
}

static void
gom_sqlite_session_track_entity_changes (GomSession *session,
                                         GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_pending (entity))
    return;

  _gom_entity_track_changes (entity, session);
}

static void
gom_sqlite_session_untrack_entity_changes (GomSession *session,
                                           GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_dirty (entity))
    {
      g_queue_unlink (&self->dirty_entities, _gom_entity_get_dirty_link (entity));
      _gom_entity_set_dirty (entity, FALSE);
      g_object_unref (entity);
    }

  _gom_entity_untrack_changes (entity);
}

static void
gom_sqlite_session_accept_entity_changes (GomSession *session,
                                          GomEntity  *entity,
                                          GomDelta   *delta)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_dirty (entity))
    {
      g_queue_unlink (&self->dirty_entities, _gom_entity_get_dirty_link (entity));
      _gom_entity_set_dirty (entity, FALSE);
      g_object_unref (entity);
    }

  _gom_entity_apply_delta (entity, delta, _gom_entity_change_state_is_complete (entity));
}

static void
gom_sqlite_session_mark_entity_dirty (GomSession *session,
                                      GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_pending (entity) || _gom_entity_is_dirty (entity))
    return;

  _gom_entity_set_dirty (entity, TRUE);
  g_queue_push_tail_link (&self->dirty_entities, _gom_entity_get_dirty_link (entity));
  g_object_ref (entity);
}

static gboolean
gom_sqlite_session_is_pending_entity (GomSqliteSession *self,
                                      GomEntity        *entity)
{
  return _gom_entity_is_pending (entity);
}

static DexFuture *
gom_sqlite_session_persist_impl (GomSession *session,
                                 GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GObjectClass *object_class;
  GomEntityClass *entity_class;
  const char * const *identity_fields;
  g_autoptr(GomSession) entity_session = NULL;
  g_autoptr(GomRepository) entity_repository = NULL;
  GomExpression *identity_value = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *entity_key = NULL;
  gboolean identity_ready = FALSE;
  GomEntity *existing = NULL;

  dex_return_error_if_fail (GOM_IS_SQLITE_SESSION (self));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));

  object_class = G_OBJECT_GET_CLASS (entity);
  entity_class = GOM_ENTITY_CLASS (object_class);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  entity_session = _gom_entity_dup_session (entity);
  if (entity_session != NULL && entity_session != session)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Entity is already bound to another session");

  entity_repository = gom_entity_dup_repository (entity);
  if (entity_repository != NULL && entity_repository != self->parent_instance.repository)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Entity is already bound to another repository");

  if (entity_repository == NULL)
    gom_entity_set_repository (entity, self->parent_instance.repository);

  if (identity_fields != NULL && identity_fields[0] != NULL)
    {
      identity_ready = TRUE;

      for (guint i = 0; identity_fields[i] != NULL; i++)
        {
          g_clear_object (&identity_value);

          if (!gom_entity_dup_identity_value_is_set (entity,
                                                     entity_class,
                                                     identity_fields[i],
                                                     &identity_value,
                                                     &error))
            {
              if (error != NULL)
                return dex_future_new_for_error (g_steal_pointer (&error));

              identity_ready = FALSE;
              break;
            }
        }
    }

  if (identity_ready)
    entity_key = _gom_entity_dup_session_key (entity);

  if (entity_key != NULL)
    {
      existing = _gom_session_lookup_entity (session, entity_key);
      if (existing != NULL && existing != entity)
        {
          g_object_unref (existing);
          return dex_future_new_reject (G_IO_ERROR,
                                        G_IO_ERROR_INVALID_ARGUMENT,
                                        "Entity with the same identity is already managed by the session");
        }

      if (existing == entity)
        {
          if (existing != NULL)
            g_object_unref (existing);
          return dex_future_new_true ();
        }

      if (existing != NULL)
        g_object_unref (existing);

      existing = _gom_session_register_entity (session, entity, g_steal_pointer (&entity_key));
      g_assert (existing == entity);
      return dex_future_new_true ();
    }

  if (gom_sqlite_session_is_pending_entity (self, entity))
    return dex_future_new_true ();

  _gom_entity_attach (entity, session, NULL);
  g_queue_push_tail_link (&self->pending_entities, _gom_entity_get_pending_link (entity));
  _gom_entity_set_pending (entity, TRUE);
  g_object_ref (entity);
  gom_trace_counter_add (GOM_TRACE_COUNTER_PENDING_ENTITIES, 1);

  return dex_future_new_true ();
}

static void
gom_sqlite_session_unregister_pending_entity (GomSession *session,
                                              GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (!_gom_entity_is_pending (entity))
    return;

  _gom_entity_set_pending (entity, FALSE);
  gom_trace_counter_add (GOM_TRACE_COUNTER_PENDING_ENTITIES, -1);

  if (_gom_entity_dup_session_key (entity) != NULL)
    gom_sqlite_session_track_entity_changes (session, entity);

  if (self->flushing)
    return;

  g_queue_unlink (&self->pending_entities, _gom_entity_get_pending_link (entity));
  g_object_unref (entity);
}

static gboolean
gom_sqlite_session_rekey_entity_identity (GomSession *session,
                                          GomEntity  *entity,
                                          char       *entity_key)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  g_autofree char *old_key = NULL;
  gboolean was_tracked;
  GomEntity *existing;

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (entity_key != NULL);

  if (!(old_key = _gom_entity_dup_session_key (entity)))
    {
      g_free (entity_key);
      return FALSE;
    }

  existing = g_hash_table_lookup (self->entities_by_key, entity_key);
  if (existing != NULL && existing != entity)
    {
      g_free (entity_key);
      return FALSE;
    }

  was_tracked = g_hash_table_contains (self->entities_by_key, old_key);

  if (g_strcmp0 (old_key, entity_key) == 0 && was_tracked)
    {
      g_free (entity_key);
      return TRUE;
    }

  g_hash_table_steal (self->entities_by_key, old_key);
  _gom_entity_attach (entity, GOM_SESSION (self), entity_key);
  g_hash_table_insert (self->entities_by_key,
                       _gom_entity_dup_session_key (entity),
                       g_object_ref (entity));

  gom_sqlite_session_track_entity_changes (session, entity);

  if (!was_tracked)
    {
      g_queue_push_tail_link (&self->all_entities, _gom_entity_get_session_link (entity));
      gom_trace_counter_add (GOM_TRACE_COUNTER_IDENTITY_ENTRIES, 1);
    }

  return TRUE;
}

static void
gom_sqlite_session_unregister_entity (GomSession *session,
                                      GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  g_autoptr(GomSession) entity_session = NULL;

  g_assert (GOM_IS_SQLITE_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  entity_session = _gom_entity_dup_session (entity);

  if (entity_session == NULL || entity_session != GOM_SESSION (self))
    return;

  g_queue_unlink (&self->all_entities, _gom_entity_get_session_link (entity));

  {
    g_autofree char *entity_key = _gom_entity_dup_session_key (entity);

    if (entity_key != NULL)
      {
        gboolean removed;

        g_object_ref (entity);
        gom_sqlite_session_untrack_entity_changes (session, entity);
        removed = g_hash_table_remove (self->entities_by_key, entity_key);
        _gom_entity_detach (entity);
        g_object_unref (entity);
        if (removed)
          gom_trace_counter_add (GOM_TRACE_COUNTER_IDENTITY_ENTRIES, -1);
        return;
      }
  }

  gom_sqlite_session_untrack_entity_changes (session, entity);
  _gom_entity_detach (entity);
}

static void
gom_sqlite_session_clear_entities_vfunc (GomSession *session)
{
  gom_sqlite_session_clear_entities (GOM_SQLITE_SESSION (session));
}

static void
gom_sqlite_session_flush_state_free (GomSqliteSessionFlushState *state)
{
  if (state->session != NULL)
    state->session->flushing = FALSE;

  g_clear_object (&state->session);
  g_free (state);
}

static GomSqliteSessionFlushState *
gom_sqlite_session_flush_state_ref (GomSqliteSessionFlushState *state)
{
  g_atomic_ref_count_inc (&state->ref_count);

  return state;
}

static void
gom_sqlite_session_flush_state_unref (GomSqliteSessionFlushState *state)
{
  if (g_atomic_ref_count_dec (&state->ref_count))
    gom_sqlite_session_flush_state_free (state);
}

static DexFuture *
gom_sqlite_session_flush_step_cb (DexFuture *completed,
                                  gpointer   user_data)
{
  GomSqliteSessionFlushState *state = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  GomEntity *entity;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_SESSION (state->session));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!g_queue_is_empty (&state->session->pending_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->pending_entities)))
        return dex_future_new_true ();

      if (_gom_entity_is_pending (entity))
        gom_trace_counter_add (GOM_TRACE_COUNTER_PENDING_ENTITIES, -1);
      g_queue_unlink (&state->session->pending_entities, _gom_entity_get_pending_link (entity));
      _gom_entity_set_pending (entity, FALSE);
      g_object_unref (entity);
      return gom_sqlite_session_flush_next (state);
    }

  if (!g_queue_is_empty (&state->session->dirty_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->dirty_entities)))
        return dex_future_new_true ();

      g_queue_unlink (&state->session->dirty_entities, _gom_entity_get_dirty_link (entity));
      _gom_entity_set_dirty (entity, FALSE);
      g_object_unref (entity);
      return gom_sqlite_session_flush_next (state);
    }

  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_session_flush_next (GomSqliteSessionFlushState *state)
{
  GomEntity *entity;

  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_SESSION (state->session));

  if (!g_queue_is_empty (&state->session->pending_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->pending_entities)))
        return dex_future_new_true ();

      return dex_future_then (gom_entity_insert (entity),
                              gom_sqlite_session_flush_step_cb,
                              gom_sqlite_session_flush_state_ref (state),
                              (GDestroyNotify)gom_sqlite_session_flush_state_unref);
    }

  if (!g_queue_is_empty (&state->session->dirty_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->dirty_entities)))
        return dex_future_new_true ();

      return dex_future_then (gom_entity_update (entity),
                              gom_sqlite_session_flush_step_cb,
                              gom_sqlite_session_flush_state_ref (state),
                              (GDestroyNotify)gom_sqlite_session_flush_state_unref);
    }

  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_session_flush_complete_cb (DexFuture *completed,
                                      gpointer   user_data)
{
  GomSqliteSessionFlushState *state = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);

  value = dex_future_get_value (completed, &error);
  state->session->flushing = FALSE;
  if (value == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_session_attach_cursor_cb (DexFuture *completed,
                                     gpointer   user_data)
{
  GomSqliteSessionAttachState *state = user_data;
  const GValue *value;
  GomCursor *cursor;
  g_autoptr(GomRepository) repository = NULL;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_SESSION (state->session));

  value = dex_future_get_value (completed, NULL);
  g_assert (value != NULL);
  g_assert (G_VALUE_HOLDS (value, GOM_TYPE_CURSOR));

  cursor = g_value_get_object (value);
  repository = gom_session_dup_repository (GOM_SESSION (state->session));
  _gom_cursor_set_repository (cursor, repository);
  _gom_cursor_set_session (cursor, GOM_SESSION (state->session));

  return dex_future_new_take_object (g_object_ref (cursor));
}

static DexFuture *
gom_sqlite_session_query (GomSession *session,
                          GomQuery   *query)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomSqliteSessionAttachState *state;
  GomRepository *repository;

  g_assert (GOM_IS_SQLITE_SESSION (self));

  GOM_TRACE_MARK ("Session",
                  "query",
                  "session=%" G_GINT64_FORMAT " backend=sqlite",
                  self->parent_instance.id);

  repository = self->parent_instance.repository;
  state = g_new0 (GomSqliteSessionAttachState, 1);
  state->session = g_object_ref (self);

  return dex_future_then (gom_sqlite_driver_query_on_lease (self->lease_state,
                                                            repository,
                                                            query,
                                                            _gom_query_get_with_count (query)
                                                              ? GOM_CURSOR_FLAGS_COUNT_ROWS
                                                              : GOM_CURSOR_FLAGS_NONE,
                                                            TRUE),
                          gom_sqlite_session_attach_cursor_cb,
                          state,
                          (GDestroyNotify)gom_sqlite_session_attach_state_free);
}

static DexFuture *
gom_sqlite_session_mutate (GomSession  *session,
                           GomMutation *mutation)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomRegistry *registry;

  g_assert (GOM_IS_SQLITE_SESSION (self));

  GOM_TRACE_MARK ("Session",
                  "mutate",
                  "session=%" G_GINT64_FORMAT " backend=sqlite mutation=%s",
                  self->parent_instance.id,
                  G_OBJECT_TYPE_NAME (mutation));

  registry = _gom_repository_get_registry (self->parent_instance.repository);
  return _gom_session_track_mutation_result (session,
                                             gom_sqlite_driver_mutate_on_lease (self->lease_state,
                                                                                registry,
                                                                                mutation));
}

static DexFuture *
gom_sqlite_session_persist (GomSession *session,
                            GomEntity  *entity)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);

  GOM_TRACE_MARK ("Session",
                  "persist",
                  "session=%" G_GINT64_FORMAT " entity=%s",
                  self->parent_instance.id,
                  G_OBJECT_TYPE_NAME (entity));
  return gom_sqlite_session_persist_impl (session, entity);
}

static DexFuture *
gom_sqlite_session_flush (GomSession *session)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomSqliteSessionFlushState *state;

  dex_return_error_if_fail (GOM_IS_SQLITE_SESSION (self));

  if (self->flushing)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_BUSY,
                                  "Session flush is already in progress");

  if (g_queue_is_empty (&self->pending_entities) && g_queue_is_empty (&self->dirty_entities))
    return dex_future_new_true ();

  self->flushing = TRUE;
  GOM_TRACE_MARK ("Session",
                  "flush",
                  "session=%" G_GINT64_FORMAT " pending=%u",
                  self->parent_instance.id,
                  g_queue_get_length (&self->pending_entities));

  state = g_new0 (GomSqliteSessionFlushState, 1);
  g_atomic_ref_count_init (&state->ref_count);
  state->session = g_object_ref (self);

  return dex_future_then (gom_sqlite_session_flush_next (state),
                          gom_sqlite_session_flush_complete_cb,
                          state,
                          (GDestroyNotify)gom_sqlite_session_flush_state_unref);
}

static DexFuture *
gom_sqlite_session_commit_after_flush_cb (DexFuture *completed,
                                          gpointer   user_data)
{
  GomSqliteSessionCloseState *state = user_data;
  GomSqliteSessionCloseState *close_state;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_SESSION (state->session));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  close_state = g_new0 (GomSqliteSessionCloseState, 1);
  close_state->session = g_object_ref (state->session);
  close_state->rollback = state->rollback;

  return gom_sqlite_lease_state_invoke (state->session->lease_state,
                                        state->rollback ? "[gom-sqlite-session-rollback]"
                                                        : "[gom-sqlite-session-commit]",
                                        gom_sqlite_session_close_thread,
                                        close_state,
                                        (GDestroyNotify)gom_sqlite_session_close_state_free);
}

static DexFuture *
gom_sqlite_session_close_complete_cb (DexFuture *completed,
                                      gpointer   user_data)
{
  GomSqliteSession *session = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (GOM_IS_SQLITE_SESSION (session));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  gom_sqlite_session_release_write_permit (session);

  return dex_future_new_for_value (value);
}

static DexFuture *
gom_sqlite_session_close_thread (gpointer user_data)
{
  GomSqliteSessionCloseState *state = user_data;
  g_autoptr(GError) error = NULL;
  GomSqliteConnection *connection;
  sqlite3 *db;
  gint64 start_time = GOM_TRACE_BEGIN_MARK ();

  g_assert (state != NULL);
  g_assert (GOM_IS_SQLITE_SESSION (state->session));

  if (state->session->lease_state == NULL)
    return dex_future_new_true ();

  connection = gom_sqlite_lease_state_get_connection (state->session->lease_state);
  db = gom_sqlite_connection_get_native (connection);

  if (!gom_sqlite_driver_exec_sql (db,
                                   state->rollback ? "ROLLBACK" : "COMMIT",
                                   state->rollback ? "rollback session transaction"
                                                   : "commit session transaction",
                                   &error))
    {
      GOM_TRACE_END_MARK (start_time,
                          "Session",
                          state->rollback ? "rollback" : "commit",
                          "failed: %s",
                          error->message);
      return dex_future_new_for_error (g_steal_pointer (&error));
    }

  _gom_session_set_closed (GOM_SESSION (state->session), TRUE);
  gom_sqlite_session_clear_entities (state->session);
  if (state->session->lease_state != NULL)
    {
      gom_sqlite_lease_state_unref (state->session->lease_state);
      state->session->lease_state = NULL;
    }

  GOM_TRACE_END_MARK (start_time,
                      "Session",
                      state->rollback ? "rollback" : "commit",
                      "session=%" G_GINT64_FORMAT,
                      state->session->parent_instance.id);

  return dex_future_new_true ();
}

static DexFuture *
gom_sqlite_session_commit (GomSession *session)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomSqliteSessionCloseState *state;

  if (self->lease_state == NULL)
    return dex_future_new_true ();

  state = g_new0 (GomSqliteSessionCloseState, 1);
  state->session = g_object_ref (GOM_SQLITE_SESSION (session));
  state->rollback = FALSE;

  GOM_TRACE_MARK ("Session",
                  "commit",
                  "session=%" G_GINT64_FORMAT " backend=sqlite",
                  self->parent_instance.id);

  return dex_future_finally (dex_future_then (gom_sqlite_session_flush (session),
                                              gom_sqlite_session_commit_after_flush_cb,
                                              state,
                                              (GDestroyNotify)gom_sqlite_session_close_state_free),
                             gom_sqlite_session_close_complete_cb,
                             g_object_ref (self),
                             g_object_unref);
}

static DexFuture *
gom_sqlite_session_rollback (GomSession *session)
{
  GomSqliteSession *self = GOM_SQLITE_SESSION (session);
  GomSqliteSessionCloseState *state;

  if (self->lease_state == NULL)
    return dex_future_new_true ();

  state = g_new0 (GomSqliteSessionCloseState, 1);
  state->session = g_object_ref (self);
  state->rollback = TRUE;

  GOM_TRACE_MARK ("Session",
                  "rollback",
                  "session=%" G_GINT64_FORMAT " backend=sqlite",
                  self->parent_instance.id);

  return dex_future_finally (gom_sqlite_lease_state_invoke (self->lease_state,
                                                            "[gom-sqlite-session-rollback]",
                                                            gom_sqlite_session_close_thread,
                                                            state,
                                                            (GDestroyNotify)gom_sqlite_session_close_state_free),
                             gom_sqlite_session_close_complete_cb,
                             g_object_ref (self),
                             g_object_unref);
}

static void
gom_sqlite_session_class_init (GomSqliteSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomSessionClass *session_class = GOM_SESSION_CLASS (klass);

  object_class->finalize = gom_sqlite_session_finalize;

  session_class->query = gom_sqlite_session_query;
  session_class->mutate = gom_sqlite_session_mutate;
  session_class->persist = gom_sqlite_session_persist;
  session_class->flush = gom_sqlite_session_flush;
  session_class->commit = gom_sqlite_session_commit;
  session_class->rollback = gom_sqlite_session_rollback;
  session_class->track_entity_changes = gom_sqlite_session_track_entity_changes;
  session_class->untrack_entity_changes = gom_sqlite_session_untrack_entity_changes;
  session_class->accept_entity_changes = gom_sqlite_session_accept_entity_changes;
  session_class->mark_entity_dirty = gom_sqlite_session_mark_entity_dirty;
  session_class->lookup_entity = gom_sqlite_session_lookup_entity;
  session_class->register_entity = gom_sqlite_session_register_entity;
  session_class->unregister_pending_entity = gom_sqlite_session_unregister_pending_entity;
  session_class->rekey_entity_identity = gom_sqlite_session_rekey_entity_identity;
  session_class->unregister_entity = gom_sqlite_session_unregister_entity;
  session_class->clear_entities = gom_sqlite_session_clear_entities_vfunc;
}

GomSqliteSession *
gom_sqlite_session_new (GomRepository       *repository,
                        GomSqliteLeaseState *state,
                        DexLimiter          *write_limiter)
{
  GomSqliteSession *self;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (state != NULL, NULL);
  g_return_val_if_fail (DEX_IS_LIMITER (write_limiter), NULL);

  self = g_object_new (GOM_TYPE_SQLITE_SESSION, NULL);
  _gom_session_set_repository (GOM_SESSION (self), repository);
  self->lease_state = gom_sqlite_lease_state_ref (state);
  self->write_limiter = dex_ref (write_limiter);

  return self;
}
