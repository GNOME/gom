/* gom-pgsql-session.c
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

#include "gom-cursor-private.h"
#include "gom-entity-private.h"
#include "gom-query-private.h"
#include "gom-pgsql-driver-private.h"
#include "gom-pgsql-session-private.h"
#include "gom-repository-private.h"
#include "gom-session-private.h"

struct _GomPgsqlSession
{
  GomSession parent_instance;

  PgsqlConnectionPool *pool;
  PgsqlConnection     *connection;
  PgsqlTransaction    *transaction;
  GQueue               all_entities;
  GQueue               pending_entities;
  GQueue               dirty_entities;
  GHashTable          *entities_by_key;
  gboolean             flushing;
};

struct _GomPgsqlSessionClass
{
  GomSessionClass parent_class;
};

typedef struct
{
  GomPgsqlSession *session;
} GomPgsqlSessionAttachState;

typedef struct
{
  GomPgsqlSession *session;
  gboolean         rollback;
} GomPgsqlSessionCloseState;

typedef struct
{
  gatomicrefcount  ref_count;
  GomPgsqlSession *session;
} GomPgsqlSessionFlushState;

static void       gom_pgsql_session_finalize                  (GObject                   *object);
static GomEntity *gom_pgsql_session_lookup_entity             (GomSession                *session,
                                                               const char                *entity_key);
static GomEntity *gom_pgsql_session_register_entity           (GomSession                *session,
                                                               GomEntity                 *entity,
                                                               char                      *entity_key);
static void       gom_pgsql_session_unregister_pending_entity (GomSession                *session,
                                                               GomEntity                 *entity);
static gboolean   gom_pgsql_session_rekey_entity_identity     (GomSession                *session,
                                                               GomEntity                 *entity,
                                                               char                      *entity_key);
static void       gom_pgsql_session_unregister_entity         (GomSession                *session,
                                                               GomEntity                 *entity);
static void       gom_pgsql_session_clear_entities_vfunc      (GomSession                *session);
static void       gom_pgsql_session_track_entity_changes      (GomSession                *session,
                                                               GomEntity                 *entity);
static void       gom_pgsql_session_untrack_entity_changes    (GomSession                *session,
                                                               GomEntity                 *entity);
static void       gom_pgsql_session_accept_entity_changes     (GomSession                *session,
                                                               GomEntity                 *entity,
                                                               GomDelta                  *delta);
static void       gom_pgsql_session_mark_entity_dirty         (GomSession                *session,
                                                               GomEntity                 *entity);
static DexFuture *gom_pgsql_session_persist                   (GomSession                *session,
                                                               GomEntity                 *entity);
static DexFuture *gom_pgsql_session_flush                     (GomSession                *session);
static DexFuture *gom_pgsql_session_flush_next                (GomPgsqlSessionFlushState *state);
static DexFuture *gom_pgsql_session_attach_cursor_cb          (DexFuture                 *completed,
                                                               gpointer                   user_data);
static DexFuture *gom_pgsql_session_query                     (GomSession                *session,
                                                               GomQuery                  *query);
static DexFuture *gom_pgsql_session_mutate                    (GomSession                *session,
                                                               GomMutation               *mutation);
static DexFuture *gom_pgsql_session_close_thread              (gpointer                   user_data);
static DexFuture *gom_pgsql_session_commit_after_flush_cb     (DexFuture                 *completed,
                                                               gpointer                   user_data);
static DexFuture *gom_pgsql_session_commit                    (GomSession                *session);
static DexFuture *gom_pgsql_session_rollback                  (GomSession                *session);

static void
gom_pgsql_session_attach_state_free (GomPgsqlSessionAttachState *state)
{
  g_clear_object (&state->session);
  g_free (state);
}

static void
gom_pgsql_session_close_state_free (GomPgsqlSessionCloseState *state)
{
  g_clear_object (&state->session);
  g_free (state);
}

static void
gom_pgsql_session_flush_state_free (GomPgsqlSessionFlushState *state)
{
  if (state->session != NULL)
    state->session->flushing = FALSE;

  g_clear_object (&state->session);
  g_free (state);
}

static GomPgsqlSessionFlushState *
gom_pgsql_session_flush_state_ref (GomPgsqlSessionFlushState *state)
{
  g_atomic_ref_count_inc (&state->ref_count);
  return state;
}

static void
gom_pgsql_session_flush_state_unref (GomPgsqlSessionFlushState *state)
{
  if (g_atomic_ref_count_dec (&state->ref_count))
    gom_pgsql_session_flush_state_free (state);
}

G_DEFINE_FINAL_TYPE (GomPgsqlSession, gom_pgsql_session, GOM_TYPE_SESSION)

static void
gom_pgsql_session_clear_entities (GomPgsqlSession *self)
{
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

          gom_pgsql_session_untrack_entity_changes (GOM_SESSION (self), entity);

          if (was_pending)
            {
              g_queue_unlink (&self->pending_entities, _gom_entity_get_pending_link (entity));
              _gom_entity_set_pending (entity, FALSE);
              g_object_unref (entity);
            }

          _gom_entity_detach (entity);

          if (entity_key != NULL)
            g_hash_table_remove (self->entities_by_key, entity_key);
        }
    }

  while (!g_queue_is_empty (&self->pending_entities))
    {
      GomEntity *entity = g_queue_peek_head (&self->pending_entities);

      if (entity != NULL)
        {
          gom_pgsql_session_untrack_entity_changes (GOM_SESSION (self), entity);
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
}

static void
gom_pgsql_session_init (GomPgsqlSession *self)
{
  g_queue_init (&self->all_entities);
  g_queue_init (&self->pending_entities);
  g_queue_init (&self->dirty_entities);
  self->entities_by_key = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

static void
gom_pgsql_session_finalize (GObject *object)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (object);

  if (!self->parent_instance.closed && self->transaction != NULL)
    {
      g_warning ("GomSession[%" G_GINT64_FORMAT "]: implicit rollback during finalize",
                 self->parent_instance.id);
      dex_future_disown (pgsql_transaction_rollback (self->transaction));
    }

  if (self->entities_by_key != NULL)
    gom_pgsql_session_clear_entities (self);

  g_clear_pointer (&self->entities_by_key, g_hash_table_unref);

  if (self->transaction != NULL)
    g_clear_object (&self->transaction);

  if (self->connection != NULL && self->pool != NULL)
    {
      pgsql_connection_pool_release (self->pool, self->connection);
      g_clear_object (&self->connection);
    }

  g_clear_object (&self->pool);

  G_OBJECT_CLASS (gom_pgsql_session_parent_class)->finalize (object);
}

static GomEntity *
gom_pgsql_session_lookup_entity (GomSession *session,
                                 const char *entity_key)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomEntity *entity;

  entity = g_hash_table_lookup (self->entities_by_key, entity_key);
  return entity ? g_object_ref (entity) : NULL;
}

static GomEntity *
gom_pgsql_session_register_entity (GomSession *session,
                                   GomEntity  *entity,
                                   char       *entity_key)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomEntity *existing;

  if ((existing = g_hash_table_lookup (self->entities_by_key, entity_key)))
    {
      g_free (entity_key);
      return g_object_ref (existing);
    }

  _gom_entity_attach (entity, GOM_SESSION (self), entity_key);
  if (self->parent_instance.repository != NULL)
    gom_entity_set_repository (entity, self->parent_instance.repository);

  g_queue_push_tail_link (&self->all_entities, _gom_entity_get_session_link (entity));
  g_hash_table_insert (self->entities_by_key,
                       _gom_entity_dup_session_key (entity),
                       g_object_ref (entity));

  gom_pgsql_session_track_entity_changes (session, entity);

  return entity;
}

static void
gom_pgsql_session_track_entity_changes (GomSession *session,
                                        GomEntity  *entity)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);

  g_assert (GOM_IS_PGSQL_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_pending (entity))
    return;

  _gom_entity_track_changes (entity, session);
}

static void
gom_pgsql_session_untrack_entity_changes (GomSession *session,
                                          GomEntity  *entity)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);

  g_assert (GOM_IS_PGSQL_SESSION (self));
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
gom_pgsql_session_accept_entity_changes (GomSession *session,
                                         GomEntity  *entity,
                                         GomDelta   *delta)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);

  g_assert (GOM_IS_PGSQL_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_dirty (entity))
    {
      g_queue_unlink (&self->dirty_entities, _gom_entity_get_dirty_link (entity));
      _gom_entity_set_dirty (entity, FALSE);
    }

  _gom_entity_apply_delta (entity, delta, _gom_entity_change_state_is_complete (entity));
}

static void
gom_pgsql_session_mark_entity_dirty (GomSession *session,
                                     GomEntity  *entity)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);

  g_assert (GOM_IS_PGSQL_SESSION (self));
  g_assert (GOM_IS_ENTITY (entity));

  if (_gom_entity_is_pending (entity) || _gom_entity_is_dirty (entity))
    return;

  _gom_entity_set_dirty (entity, TRUE);
  g_queue_push_tail_link (&self->dirty_entities, _gom_entity_get_dirty_link (entity));
  g_object_ref (entity);
}

static gboolean
gom_pgsql_session_is_pending_entity (GomPgsqlSession *self,
                                     GomEntity       *entity)
{
  return _gom_entity_is_pending (entity);
}

static DexFuture *
gom_pgsql_session_persist_impl (GomSession *session,
                                GomEntity  *entity)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
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

  dex_return_error_if_fail (GOM_IS_PGSQL_SESSION (self));
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

  if (gom_pgsql_session_is_pending_entity (self, entity))
    return dex_future_new_true ();

  _gom_entity_attach (entity, session, NULL);
  g_queue_push_tail_link (&self->pending_entities, _gom_entity_get_pending_link (entity));
  _gom_entity_set_pending (entity, TRUE);
  g_object_ref (entity);

  return dex_future_new_true ();
}

static void
gom_pgsql_session_unregister_pending_entity (GomSession *session,
                                             GomEntity  *entity)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);

  if (!_gom_entity_is_pending (entity))
    return;

  _gom_entity_set_pending (entity, FALSE);

  if (_gom_entity_dup_session_key (entity) != NULL)
    gom_pgsql_session_track_entity_changes (session, entity);

  if (self->flushing)
    return;

  g_queue_unlink (&self->pending_entities, _gom_entity_get_pending_link (entity));
  g_object_unref (entity);
}

static gboolean
gom_pgsql_session_rekey_entity_identity (GomSession *session,
                                         GomEntity  *entity,
                                         char       *entity_key)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  g_autofree char *old_key = NULL;
  gboolean was_tracked;
  GomEntity *existing;

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

  gom_pgsql_session_track_entity_changes (session, entity);

  if (!was_tracked)
    g_queue_push_tail_link (&self->all_entities, _gom_entity_get_session_link (entity));

  return TRUE;
}

static void
gom_pgsql_session_unregister_entity (GomSession *session,
                                     GomEntity  *entity)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  g_autoptr(GomSession) entity_session = NULL;

  entity_session = _gom_entity_dup_session (entity);
  if (entity_session == NULL || entity_session != GOM_SESSION (self))
    return;

  g_queue_unlink (&self->all_entities, _gom_entity_get_session_link (entity));

  {
    g_autofree char *entity_key = _gom_entity_dup_session_key (entity);

    if (entity_key != NULL)
      {
        gom_pgsql_session_untrack_entity_changes (session, entity);
        _gom_entity_detach (entity);
        g_hash_table_remove (self->entities_by_key, entity_key);
        return;
      }
  }

  gom_pgsql_session_untrack_entity_changes (session, entity);
  _gom_entity_detach (entity);
}

static void
gom_pgsql_session_clear_entities_vfunc (GomSession *session)
{
  gom_pgsql_session_clear_entities (GOM_PGSQL_SESSION (session));
}

static DexFuture *
gom_pgsql_session_flush_step_cb (DexFuture *completed,
                                 gpointer   user_data)
{
  GomPgsqlSessionFlushState *state = user_data;
  g_autoptr(GError) error = NULL;
  const GValue *value;
  GomEntity *entity;

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!g_queue_is_empty (&state->session->pending_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->pending_entities)))
        return dex_future_new_true ();

      g_queue_unlink (&state->session->pending_entities, _gom_entity_get_pending_link (entity));
      _gom_entity_set_pending (entity, FALSE);
      g_object_unref (entity);
      return gom_pgsql_session_flush_next (state);
    }

  if (!g_queue_is_empty (&state->session->dirty_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->dirty_entities)))
        return dex_future_new_true ();

      g_queue_unlink (&state->session->dirty_entities, _gom_entity_get_dirty_link (entity));
      _gom_entity_set_dirty (entity, FALSE);
      g_object_unref (entity);
      return gom_pgsql_session_flush_next (state);
    }

  return dex_future_new_true ();
}

static DexFuture *
gom_pgsql_session_flush_next (GomPgsqlSessionFlushState *state)
{
  GomEntity *entity;

  g_assert (state != NULL);
  g_assert (GOM_IS_PGSQL_SESSION (state->session));

  if (!g_queue_is_empty (&state->session->pending_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->pending_entities)))
        return dex_future_new_true ();

      return dex_future_then (gom_entity_insert (entity),
                              gom_pgsql_session_flush_step_cb,
                              gom_pgsql_session_flush_state_ref (state),
                              (GDestroyNotify)gom_pgsql_session_flush_state_unref);
    }

  if (!g_queue_is_empty (&state->session->dirty_entities))
    {
      if (!(entity = g_queue_peek_head (&state->session->dirty_entities)))
        return dex_future_new_true ();

      return dex_future_then (gom_entity_update (entity),
                              gom_pgsql_session_flush_step_cb,
                              gom_pgsql_session_flush_state_ref (state),
                              (GDestroyNotify)gom_pgsql_session_flush_state_unref);
    }

  return dex_future_new_true ();
}

static DexFuture *
gom_pgsql_session_flush_complete_cb (DexFuture *completed,
                                     gpointer   user_data)
{
  GomPgsqlSessionFlushState *state = user_data;
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
gom_pgsql_session_attach_cursor_cb (DexFuture *completed,
                                    gpointer   user_data)
{
  GomPgsqlSessionAttachState *state = user_data;
  const GValue *value;
  GomCursor *cursor;
  g_autoptr(GomRepository) repository = NULL;

  if (!(value = dex_future_get_value (completed, NULL)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Failed to build PostgreSQL cursor");

  cursor = g_value_get_object (value);
  repository = gom_session_dup_repository (GOM_SESSION (state->session));
  _gom_cursor_set_repository (cursor, repository);
  _gom_cursor_set_session (cursor, GOM_SESSION (state->session));

  return dex_future_new_take_object (g_object_ref (cursor));
}

static DexFuture *
gom_pgsql_session_query (GomSession *session,
                         GomQuery   *query)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomPgsqlSessionAttachState *state;
  GomRepository *repository;

  repository = self->parent_instance.repository;
  state = g_new0 (GomPgsqlSessionAttachState, 1);
  state->session = g_object_ref (self);

  return dex_future_then (gom_pgsql_query_on_executor (repository,
                                                       query,
                                                       _gom_query_get_with_count (query) ? GOM_CURSOR_FLAGS_COUNT_ROWS : GOM_CURSOR_FLAGS_NONE,
                                                       self->transaction,
                                                       (GomPgsqlQueryRunner) pgsql_transaction_query),
                          gom_pgsql_session_attach_cursor_cb,
                          state,
                          (GDestroyNotify) gom_pgsql_session_attach_state_free);
}

static DexFuture *
gom_pgsql_session_mutate (GomSession  *session,
                          GomMutation *mutation)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomRegistry *registry;

  registry = _gom_repository_get_registry (self->parent_instance.repository);
  return _gom_session_track_mutation_result (session,
                                             gom_pgsql_mutate_on_executor (registry,
                                                                           mutation,
                                                                           self->transaction,
                                                                           (GomPgsqlQueryRunner) pgsql_transaction_query));
}

static DexFuture *
gom_pgsql_session_persist (GomSession *session,
                           GomEntity  *entity)
{
  return gom_pgsql_session_persist_impl (session, entity);
}

static DexFuture *
gom_pgsql_session_flush (GomSession *session)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomPgsqlSessionFlushState *state;

  dex_return_error_if_fail (GOM_IS_PGSQL_SESSION (self));

  if (self->flushing)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_BUSY,
                                  "Session flush is already in progress");

  if (g_queue_is_empty (&self->pending_entities) && g_queue_is_empty (&self->dirty_entities))
    return dex_future_new_true ();

  self->flushing = TRUE;

  state = g_new0 (GomPgsqlSessionFlushState, 1);
  g_atomic_ref_count_init (&state->ref_count);
  state->session = g_object_ref (self);

  return dex_future_then (gom_pgsql_session_flush_next (state),
                          gom_pgsql_session_flush_complete_cb,
                          state,
                          (GDestroyNotify)gom_pgsql_session_flush_state_unref);
}

static DexFuture *
gom_pgsql_session_commit_after_flush_cb (DexFuture *completed,
                                         gpointer   user_data)
{
  GomPgsqlSessionCloseState *state = user_data;
  GomPgsqlSessionCloseState *close_state;
  g_autoptr(GError) error = NULL;
  const GValue *value;

  g_assert (DEX_IS_FUTURE (completed));
  g_assert (state != NULL);
  g_assert (GOM_IS_PGSQL_SESSION (state->session));

  if (!(value = dex_future_get_value (completed, &error)))
    return dex_future_new_for_error (g_steal_pointer (&error));

  close_state = g_new0 (GomPgsqlSessionCloseState, 1);
  close_state->session = g_object_ref (state->session);
  close_state->rollback = state->rollback;

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_session_close_thread,
                              close_state,
                              (GDestroyNotify)gom_pgsql_session_close_state_free);
}

static DexFuture *
gom_pgsql_session_close_thread (gpointer user_data)
{
  GomPgsqlSessionCloseState *state = user_data;
  g_autoptr(GError) error = NULL;
  gboolean success;

  if (state->session->transaction == NULL)
    return dex_future_new_true ();

  if (state->rollback)
    success = dex_await (pgsql_transaction_rollback (state->session->transaction), &error);
  else
    success = dex_await (pgsql_transaction_commit (state->session->transaction), &error);

  _gom_session_set_closed (GOM_SESSION (state->session), TRUE);
  gom_pgsql_session_clear_entities (state->session);

  g_clear_object (&state->session->transaction);

  if (state->session->connection != NULL && state->session->pool != NULL)
    {
      pgsql_connection_pool_release (state->session->pool, state->session->connection);
      g_clear_object (&state->session->connection);
    }

  g_clear_object (&state->session->pool);

  if (!success)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

static DexFuture *
gom_pgsql_session_commit (GomSession *session)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomPgsqlSessionCloseState *state;

  if (self->transaction == NULL)
    return dex_future_new_true ();

  state = g_new0 (GomPgsqlSessionCloseState, 1);
  state->session = g_object_ref (self);
  state->rollback = FALSE;

  return dex_future_then (gom_pgsql_session_flush (session),
                          gom_pgsql_session_commit_after_flush_cb,
                          state,
                          (GDestroyNotify)gom_pgsql_session_close_state_free);
}

static DexFuture *
gom_pgsql_session_rollback (GomSession *session)
{
  GomPgsqlSession *self = GOM_PGSQL_SESSION (session);
  GomPgsqlSessionCloseState *state;

  if (self->transaction == NULL)
    return dex_future_new_true ();

  state = g_new0 (GomPgsqlSessionCloseState, 1);
  state->session = g_object_ref (self);
  state->rollback = TRUE;

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_pgsql_session_close_thread,
                              state,
                              (GDestroyNotify)gom_pgsql_session_close_state_free);
}

static void
gom_pgsql_session_class_init (GomPgsqlSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomSessionClass *session_class = GOM_SESSION_CLASS (klass);

  object_class->finalize = gom_pgsql_session_finalize;

  session_class->query = gom_pgsql_session_query;
  session_class->mutate = gom_pgsql_session_mutate;
  session_class->persist = gom_pgsql_session_persist;
  session_class->flush = gom_pgsql_session_flush;
  session_class->commit = gom_pgsql_session_commit;
  session_class->rollback = gom_pgsql_session_rollback;
  session_class->track_entity_changes = gom_pgsql_session_track_entity_changes;
  session_class->untrack_entity_changes = gom_pgsql_session_untrack_entity_changes;
  session_class->accept_entity_changes = gom_pgsql_session_accept_entity_changes;
  session_class->mark_entity_dirty = gom_pgsql_session_mark_entity_dirty;
  session_class->lookup_entity = gom_pgsql_session_lookup_entity;
  session_class->register_entity = gom_pgsql_session_register_entity;
  session_class->unregister_pending_entity = gom_pgsql_session_unregister_pending_entity;
  session_class->rekey_entity_identity = gom_pgsql_session_rekey_entity_identity;
  session_class->unregister_entity = gom_pgsql_session_unregister_entity;
  session_class->clear_entities = gom_pgsql_session_clear_entities_vfunc;
}

GomPgsqlSession *
gom_pgsql_session_new (GomRepository       *repository,
                       PgsqlConnectionPool *pool,
                       PgsqlConnection     *connection,
                       PgsqlTransaction    *transaction)
{
  GomPgsqlSession *self;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (PGSQL_IS_CONNECTION_POOL (pool), NULL);
  g_return_val_if_fail (PGSQL_IS_CONNECTION (connection), NULL);
  g_return_val_if_fail (PGSQL_IS_TRANSACTION (transaction), NULL);

  self = g_object_new (GOM_TYPE_PGSQL_SESSION, NULL);
  _gom_session_set_repository (GOM_SESSION (self), repository);
  self->pool = g_object_ref (pool);
  self->connection = g_object_ref (connection);
  self->transaction = g_object_ref (transaction);

  return self;
}
