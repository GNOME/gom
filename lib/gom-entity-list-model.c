/* gom-entity-list-model.c
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

#include "gom-cursor.h"
#include "gom-entity.h"
#include "gom-entity-list-item-private.h"
#include "gom-entity-list-model-private.h"
#include "gom-query-private.h"
#include "gom-repository-private.h"
#include "gom-trace-private.h"
#include "gom-session-private.h"

#define GOM_ENTITY_LIST_DEFAULT_PAGE_SIZE 64

typedef enum
{
  GOM_ENTITY_LIST_SOURCE_SESSION,
  GOM_ENTITY_LIST_SOURCE_REPOSITORY,
} GomEntityListSource;

typedef struct
{
  gatomicrefcount  ref_count;
  guint            index;
  guint64          generation;
  GListStore      *items;
  guint            loaded : 1;
  guint            loading : 1;
} GomEntityListPage;

struct _GomEntityListModel
{
  GObject              parent_instance;
  GomSession          *session;
  GomRepository       *repository;
  GomQuery            *query;
  GHashTable          *wrappers;
  GHashTable          *pages;
  DexFuture           *reload_future;
  guint64              generation;
  gulong               session_changed_handler;
  guint                n_items;
  guint                page_size;
  guint                loading : 1;
  guint                refresh_pending : 1;
  GomEntityListSource  source : 1;
};

enum
{
  PROP_0,
  PROP_SESSION,
  PROP_REPOSITORY,
  PROP_QUERY,
  PROP_LOADING,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void       gom_entity_list_model_list_model_init     (GListModelInterface *iface);
static void       gom_entity_list_model_finalize            (GObject             *object);
static void       gom_entity_list_model_constructed         (GObject             *object);
static void       gom_entity_list_model_get_property        (GObject             *object,
                                                             guint                prop_id,
                                                             GValue              *value,
                                                             GParamSpec          *pspec);
static void       gom_entity_list_model_set_property        (GObject             *object,
                                                             guint                prop_id,
                                                             const GValue        *value,
                                                             GParamSpec          *pspec);
static void       gom_entity_list_model_invalidate_snapshot (GomEntityListModel  *self);
static DexFuture *gom_entity_list_model_request_reload      (GomEntityListModel  *self,
                                                             gboolean             invalidate);
static void       gom_entity_list_model_queue_page          (GomEntityListModel  *self,
                                                             guint                page_index);

G_DEFINE_TYPE_WITH_CODE (GomEntityListModel,
                         gom_entity_list_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gom_entity_list_model_list_model_init))

static GomEntityListPage *
gom_entity_list_page_ref (GomEntityListPage *page)
{
  g_return_val_if_fail (page != NULL, NULL);

  g_atomic_ref_count_inc (&page->ref_count);

  return page;
}

static void
gom_entity_list_page_free (GomEntityListPage *page)
{
  g_clear_pointer (&page->items, g_object_unref);
  g_free (page);
}

static void
gom_entity_list_page_unref (GomEntityListPage *page)
{
  g_return_if_fail (page != NULL);

  if (!g_atomic_ref_count_dec (&page->ref_count))
    return;

  gom_entity_list_page_free (page);
}

static GomEntityListPage *
gom_entity_list_page_new (guint   index,
                          guint64 generation)
{
  GomEntityListPage *page = g_new0 (GomEntityListPage, 1);

  g_atomic_ref_count_init (&page->ref_count);
  page->index = index;
  page->generation = generation;

  return page;
}

static void
gom_entity_list_model_set_loading (GomEntityListModel *self,
                                   gboolean            loading)
{
  g_assert (GOM_IS_ENTITY_LIST_MODEL (self));

  loading = !!loading;

  if (loading != self->loading)
    {
      self->loading = loading;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
    }
}

static guint
gom_entity_list_model_get_visible_count (GomEntityListModel *self,
                                         guint64             total_count)
{
  guint64 count = total_count;

  if (_gom_query_has_offset (self->query))
    {
      guint64 offset = _gom_query_get_offset (self->query);

      if (offset >= count)
        count = 0;
      else
        count -= offset;
    }

  if (_gom_query_has_limit (self->query))
    {
      guint64 limit = _gom_query_get_limit (self->query);

      if (count > limit)
        count = limit;
    }

  if (count > G_MAXUINT)
    count = G_MAXUINT;

  return count;
}

static GomQuery *
gom_entity_list_model_dup_count_query (GomEntityListModel *self)
{
  return _gom_query_new (_gom_query_get_target_entity_type (self->query),
                         NULL,
                         NULL,
                         _gom_query_get_filter (self->query),
                         NULL,
                         NULL,
                         _gom_query_get_orderings (self->query),
                         0,
                         0,
                         FALSE,
                         FALSE,
                         TRUE);
}

static GomQuery *
gom_entity_list_model_dup_page_query (GomEntityListModel *self,
                                      guint               page_index)
{
  guint64 page_offset = (guint64)page_index * self->page_size;

  return _gom_query_slice (self->query, page_offset, self->page_size);
}

static GomEntityListItem *
gom_entity_list_model_ensure_wrapper (GomEntityListModel *self,
                                      guint               position)
{
  GomEntityListItem *wrapper;

  if ((wrapper = g_hash_table_lookup (self->wrappers, GUINT_TO_POINTER (position))))
    return wrapper;

  wrapper = gom_entity_list_item_new (position);
  g_hash_table_insert (self->wrappers, GUINT_TO_POINTER (position), wrapper);

  return wrapper;
}

static GomEntityListItem *
gom_entity_list_model_lookup_wrapper (GomEntityListModel *self,
                                      guint               position)
{
  return g_hash_table_lookup (self->wrappers, GUINT_TO_POINTER (position));
}

static GomEntityListPage *
gom_entity_list_model_lookup_page (GomEntityListModel *self,
                                   guint               page_index)
{
  return g_hash_table_lookup (self->pages, GUINT_TO_POINTER (page_index));
}

static void
gom_entity_list_model_set_wrapper_loading (gpointer key,
                                           gpointer value,
                                           gpointer user_data)
{
  GomEntityListItem *item = value;

  g_assert (GOM_IS_ENTITY_LIST_ITEM (item));

  _gom_entity_list_item_set_loading (item, FALSE);
}

static void
gom_entity_list_model_clear_snapshot (GomEntityListModel *self)
{
  guint old_n_items;

  g_return_if_fail (GOM_IS_ENTITY_LIST_MODEL (self));

  old_n_items = self->n_items;
  self->n_items = 0;

  if (old_n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, 0);

  if (self->wrappers != NULL)
    {
      g_hash_table_foreach (self->wrappers, gom_entity_list_model_set_wrapper_loading, NULL);
      g_hash_table_remove_all (self->wrappers);
    }

  if (self->pages != NULL)
    g_hash_table_remove_all (self->pages);
}

static void
gom_entity_list_model_emit_size_change (GomEntityListModel *self,
                                        guint               old_n_items,
                                        guint               new_n_items)
{
  g_assert (GOM_IS_ENTITY_LIST_MODEL (self));

  if (old_n_items == new_n_items)
    {
      if (new_n_items > 0)
        g_list_model_items_changed (G_LIST_MODEL (self), 0, new_n_items, new_n_items);
      return;
    }

  if (old_n_items > 0 || new_n_items > 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);
}

static void
gom_entity_list_model_update_page_wrappers (GomEntityListModel *self,
                                            GomEntityListPage  *page)
{
  guint n_items;

  g_assert (GOM_IS_ENTITY_LIST_MODEL (self));
  g_assert (page != NULL);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (page->items));

  for (guint i = 0; i < n_items; i++)
    {
      guint position = page->index * self->page_size + i;
      g_autoptr(GomEntity) item = NULL;
      GomEntityListItem *wrapper;

      if (position >= self->n_items)
        break;

      if (!(wrapper = gom_entity_list_model_lookup_wrapper (self, position)))
        continue;

      item = g_list_model_get_item (G_LIST_MODEL (page->items), i);

      _gom_entity_list_item_set_item (wrapper, item);
      _gom_entity_list_item_set_loading (wrapper, FALSE);
    }
}

static void
gom_entity_list_model_page_started (GomEntityListModel *self,
                                    GomEntityListPage  *page)
{
  guint start;
  guint end;

  g_assert (GOM_IS_ENTITY_LIST_MODEL (self));
  g_assert (page != NULL);

  start = page->index * self->page_size;
  end = MIN (start + self->page_size, self->n_items);

  for (guint position = start; position < end; position++)
    {
      GomEntityListItem *wrapper = gom_entity_list_model_lookup_wrapper (self, position);

      if (wrapper != NULL)
        _gom_entity_list_item_set_loading (wrapper, TRUE);
    }
}

typedef struct
{
  GomEntityListModel *self;
  GomEntityListPage  *page;
} GomEntityListPageTask;

static DexFuture *
gom_entity_list_model_query_to_list_fiber (gpointer user_data)
{
  GomEntityListModel *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) results = NULL;
  g_autoptr(GomQuery) query = NULL;
  guint64 generation;
  guint new_n_items = 0;
  guint old_n_items;

  g_assert (GOM_IS_ENTITY_LIST_MODEL (self));

  generation = self->generation;

  query = gom_entity_list_model_dup_count_query (self);

  if (self->source == GOM_ENTITY_LIST_SOURCE_SESSION)
    {
      g_autoptr(GomCursor) cursor = NULL;

      if (!(cursor = dex_await_object (gom_session_query (self->session, query), &error)))
        goto fail;

      if ((gom_cursor_get_capabilities (cursor) & GOM_CURSOR_CAPABILITIES_COUNT) == 0)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               "Backend did not provide a row count");
          goto fail;
        }

      new_n_items = gom_entity_list_model_get_visible_count (self, gom_cursor_get_count (cursor));
    }
  else
    {
      gint64 count;

      count = dex_await_int64 (gom_repository_count (self->repository, query), &error);
      if (error != NULL)
        goto fail;

      new_n_items = gom_entity_list_model_get_visible_count (self, count);
    }

  if (generation != self->generation)
    goto complete;

  old_n_items = self->n_items;
  self->n_items = new_n_items;

  gom_entity_list_model_emit_size_change (self, old_n_items, self->n_items);

  if (self->n_items > 0)
    gom_entity_list_model_queue_page (self, 0);

complete:
  gom_entity_list_model_set_loading (self, FALSE);
  dex_clear (&self->reload_future);

  if (self->refresh_pending)
    {
      self->refresh_pending = FALSE;
      return gom_entity_list_model_request_reload (self, FALSE);
    }

  return dex_future_new_take_object (g_object_ref (self));

fail:
  if (generation == self->generation)
    {
      gom_entity_list_model_clear_snapshot (self);
      gom_entity_list_model_set_loading (self, FALSE);
    }

  dex_clear (&self->reload_future);

  if (self->refresh_pending)
    {
      self->refresh_pending = FALSE;
      return gom_entity_list_model_request_reload (self, FALSE);
    }

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static void
gom_entity_list_model_page_future_free (gpointer data)
{
  GomEntityListPageTask *task = data;

  g_clear_object (&task->self);
  gom_entity_list_page_unref (task->page);
  g_free (task);
}

static DexFuture *
gom_entity_list_model_page_fiber (gpointer user_data)
{
  GomEntityListPageTask *task = user_data;
  GomEntityListPage *page;
  GomEntityListModel *self;
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) results = NULL;
  g_autoptr(GomQuery) query = NULL;
  guint64 generation;

  g_assert (task != NULL);
  g_assert (GOM_IS_ENTITY_LIST_MODEL (task->self));

  self = task->self;
  page = task->page;

  generation = page->generation;

  if (generation != self->generation)
    goto complete;

  query = gom_entity_list_model_dup_page_query (self, page->index);

  if (self->source == GOM_ENTITY_LIST_SOURCE_SESSION)
    {
      g_autoptr(GomCursor) cursor = NULL;

      if (!(cursor = dex_await_object (gom_session_query (self->session, query), &error)))
        goto fail;

      if (!(results = dex_await_object (gom_cursor_exhaust_to_list (cursor), &error)))
        goto fail;
    }
  else
    {
      g_autoptr(GomCursor) cursor = NULL;

      if (!(cursor = dex_await_object (gom_repository_query (self->repository, query), &error)))
        goto fail;

      if (!(results = dex_await_object (gom_cursor_exhaust_to_list (cursor), &error)))
        goto fail;
    }

  if (generation != self->generation)
    goto complete;

  page->items = G_LIST_STORE (g_object_ref (results));
  page->loaded = TRUE;
  page->loading = FALSE;

  gom_entity_list_model_update_page_wrappers (self, page);

complete:
  return dex_future_new_true ();

fail:
  page->loading = FALSE;

  {
    guint start = page->index * self->page_size;
    guint end = MIN (start + self->page_size, self->n_items);

    for (guint position = start; position < end; position++)
      {
        GomEntityListItem *wrapper = gom_entity_list_model_lookup_wrapper (self, position);

        if (wrapper != NULL)
          _gom_entity_list_item_set_loading (wrapper, FALSE);
      }
  }

  if (generation != self->generation)
    return dex_future_new_true ();

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static void
gom_entity_list_model_queue_page (GomEntityListModel *self,
                                  guint               page_index)
{
  GomEntityListPage *page;
  GomEntityListPageTask *task;

  g_return_if_fail (GOM_IS_ENTITY_LIST_MODEL (self));

  if (page_index * self->page_size >= self->n_items)
    return;

  if (!(page = gom_entity_list_model_lookup_page (self, page_index)))
    {
      page = gom_entity_list_page_new (page_index, self->generation);
      g_hash_table_insert (self->pages, GUINT_TO_POINTER (page_index), page);
    }

  if (page->loading || page->loaded)
    return;

  page->loading = TRUE;
  gom_entity_list_model_page_started (self, page);

  task = g_new0 (GomEntityListPageTask, 1);
  task->self = g_object_ref (self);
  task->page = gom_entity_list_page_ref (page);

  dex_future_disown (dex_scheduler_spawn (NULL,
                                          0,
                                          gom_entity_list_model_page_fiber,
                                          task,
                                          gom_entity_list_model_page_future_free));
}

static void
gom_entity_list_model_invalidate_snapshot (GomEntityListModel *self)
{
  g_return_if_fail (GOM_IS_ENTITY_LIST_MODEL (self));

  self->generation++;
  gom_entity_list_model_clear_snapshot (self);
}

static DexFuture *
gom_entity_list_model_request_reload (GomEntityListModel *self,
                                      gboolean            invalidate)
{
  DexFuture *future;
  gint64 start_time;

  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), NULL);

  if (invalidate)
    gom_entity_list_model_invalidate_snapshot (self);

  if (self->loading)
    {
      self->refresh_pending |= invalidate;
      return self->reload_future ? dex_ref (self->reload_future) : NULL;
    }

  gom_entity_list_model_set_loading (self, TRUE);
  start_time = GOM_TRACE_BEGIN_MARK ();

  future = dex_scheduler_spawn (NULL,
                                0,
                                gom_entity_list_model_query_to_list_fiber,
                                g_object_ref (self),
                                g_object_unref);

  dex_set_object (&self->reload_future, future);

  return GOM_TRACE_MARKED_FUTURE (future,
                                  start_time,
                                  "EntityListModel",
                                  "reload",
                                  "source=%d invalidate=%d",
                                  self->source,
                                  invalidate);
}

static void
gom_entity_list_model_session_changed_cb (GomEntityListModel *self,
                                          GomSession         *session)
{
  g_assert (GOM_IS_ENTITY_LIST_MODEL (self));
  g_assert (GOM_IS_SESSION (session));

  dex_future_disown (gom_entity_list_model_request_reload (self, TRUE));
}

static GType
gom_entity_list_model_get_item_type_iface (GListModel *model)
{
  return GOM_TYPE_ENTITY_LIST_ITEM;
}

static guint
gom_entity_list_model_get_n_items_iface (GListModel *model)
{
  GomEntityListModel *self = GOM_ENTITY_LIST_MODEL (model);

  return self->n_items;
}

static gpointer
gom_entity_list_model_get_item_iface (GListModel *model,
                                      guint       position)
{
  GomEntityListModel *self = GOM_ENTITY_LIST_MODEL (model);
  GomEntityListItem *wrapper;
  guint page_index;

  if (position >= self->n_items)
    return NULL;

  wrapper = gom_entity_list_model_ensure_wrapper (self, position);
  page_index = position / self->page_size;

  {
    GomEntityListPage *page = gom_entity_list_model_lookup_page (self, page_index);

    if (page != NULL && page->loaded && page->items != NULL)
      {
        guint row = position - (page_index * self->page_size);

        if (row < g_list_model_get_n_items (G_LIST_MODEL (page->items)))
          {
            g_autoptr(GomEntity) item = g_list_model_get_item (G_LIST_MODEL (page->items), row);

            _gom_entity_list_item_set_item (wrapper, item);
            _gom_entity_list_item_set_loading (wrapper, FALSE);
          }
      }
    else
      {
        _gom_entity_list_item_set_loading (wrapper, TRUE);
        gom_entity_list_model_queue_page (self, page_index);
      }
  }

  return g_object_ref (wrapper);
}

static void
gom_entity_list_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gom_entity_list_model_get_item_type_iface;
  iface->get_n_items = gom_entity_list_model_get_n_items_iface;
  iface->get_item = gom_entity_list_model_get_item_iface;
}

static void
gom_entity_list_model_finalize (GObject *object)
{
  GomEntityListModel *self = GOM_ENTITY_LIST_MODEL (object);

  if (self->session != NULL && self->session_changed_handler != 0)
    g_clear_signal_handler (&self->session_changed_handler, self->session);

  g_clear_object (&self->session);
  g_clear_object (&self->repository);
  g_clear_object (&self->query);
  g_clear_pointer (&self->wrappers, g_hash_table_unref);
  g_clear_pointer (&self->pages, g_hash_table_unref);
  g_clear_pointer (&self->reload_future, dex_unref);

  G_OBJECT_CLASS (gom_entity_list_model_parent_class)->finalize (object);
}

static void
gom_entity_list_model_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GomEntityListModel *self = GOM_ENTITY_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      g_value_set_object (value, self->session);
      break;

    case PROP_REPOSITORY:
      g_value_set_object (value, self->repository);
      break;

    case PROP_QUERY:
      g_value_set_object (value, self->query);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, self->loading);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_entity_list_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GomEntityListModel *self = GOM_ENTITY_LIST_MODEL (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      g_set_object (&self->session, g_value_get_object (value));
      if (g_value_get_object (value) != NULL)
        self->source = GOM_ENTITY_LIST_SOURCE_SESSION;
      break;

    case PROP_REPOSITORY:
      g_set_object (&self->repository, g_value_get_object (value));
      if (g_value_get_object (value) != NULL)
        self->source = GOM_ENTITY_LIST_SOURCE_REPOSITORY;
      break;

    case PROP_QUERY:
      g_set_object (&self->query, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_entity_list_model_constructed (GObject *object)
{
  GomEntityListModel *self = GOM_ENTITY_LIST_MODEL (object);

  G_OBJECT_CLASS (gom_entity_list_model_parent_class)->constructed (object);

  if (self->session != NULL)
    self->source = GOM_ENTITY_LIST_SOURCE_SESSION;
  else if (self->repository != NULL)
    self->source = GOM_ENTITY_LIST_SOURCE_REPOSITORY;

  if (self->session != NULL)
    self->session_changed_handler =
      g_signal_connect_object (self->session,
                               "changed",
                               G_CALLBACK (gom_entity_list_model_session_changed_cb),
                               self,
                               G_CONNECT_SWAPPED);

  gom_entity_list_model_request_reload (self, FALSE);
}

static void
gom_entity_list_model_class_init (GomEntityListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gom_entity_list_model_constructed;
  object_class->finalize = gom_entity_list_model_finalize;
  object_class->get_property = gom_entity_list_model_get_property;
  object_class->set_property = gom_entity_list_model_set_property;

  properties[PROP_SESSION] =
    g_param_spec_object ("session", NULL, NULL,
                         GOM_TYPE_SESSION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_REPOSITORY] =
    g_param_spec_object ("repository", NULL, NULL,
                         GOM_TYPE_REPOSITORY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_QUERY] =
    g_param_spec_object ("query", NULL, NULL,
                         GOM_TYPE_QUERY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_LOADING] =
    g_param_spec_boolean ("loading", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_entity_list_model_init (GomEntityListModel *self)
{
  self->wrappers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  self->pages = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                       (GDestroyNotify)gom_entity_list_page_unref);
  self->page_size = GOM_ENTITY_LIST_DEFAULT_PAGE_SIZE;
}

GomEntityListModel *
_gom_entity_list_model_new_session (GomSession  *session,
                                    GomQuery    *query,
                                    GError     **error)
{
  g_return_val_if_fail (GOM_IS_SESSION (session), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  if (!_gom_query_validate_entity_list (query, error))
    return NULL;

  return g_object_new (GOM_TYPE_ENTITY_LIST_MODEL,
                       "session", session,
                       "query", query,
                       NULL);
}

GomEntityListModel *
_gom_entity_list_model_new_repository (GomRepository  *repository,
                                       GomQuery       *query,
                                       GError        **error)
{
  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (GOM_IS_QUERY (query), NULL);

  if (!_gom_query_validate_entity_list (query, error))
    return NULL;

  return g_object_new (GOM_TYPE_ENTITY_LIST_MODEL,
                       "repository", repository,
                       "query", query,
                       NULL);
}

/**
 * gom_entity_list_model_reload:
 * @self: a [class@Gom.EntityListModel]
 *
 * Reloads the current snapshot from the backing query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 */
DexFuture *
gom_entity_list_model_reload (GomEntityListModel *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), NULL);

  return gom_entity_list_model_request_reload (self, TRUE);
}

/**
 * gom_entity_list_model_refresh:
 * @self: a [class@Gom.EntityListModel]
 *
 * Refreshes the current snapshot from the backing query.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 */
DexFuture *
gom_entity_list_model_refresh (GomEntityListModel *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), NULL);

  return gom_entity_list_model_request_reload (self, TRUE);
}

gboolean
gom_entity_list_model_get_loading (GomEntityListModel *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), FALSE);

  return self->loading;
}

/**
 * gom_entity_list_model_dup_query:
 * @self: a [class@Gom.EntityListModel]
 *
 * Returns: (transfer full) (nullable): the stored [class@Gom.Query]
 */
GomQuery *
gom_entity_list_model_dup_query (GomEntityListModel *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), NULL);

  return self->query ? g_object_ref (self->query) : NULL;
}

/**
 * gom_entity_list_model_dup_session:
 * @self: a [class@Gom.EntityListModel]
 *
 * Returns: (transfer full) (nullable): the bound [class@Gom.Session]
 */
GomSession *
gom_entity_list_model_dup_session (GomEntityListModel *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), NULL);

  return self->session ? g_object_ref (self->session) : NULL;
}

/**
 * gom_entity_list_model_dup_repository:
 * @self: a [class@Gom.EntityListModel]
 *
 * Returns: (transfer full) (nullable): the bound [class@Gom.Repository]
 */
GomRepository *
gom_entity_list_model_dup_repository (GomEntityListModel *self)
{
  g_return_val_if_fail (GOM_IS_ENTITY_LIST_MODEL (self), NULL);

  if (self->repository != NULL)
    return g_object_ref (self->repository);

  return self->session ? gom_session_dup_repository (self->session) : NULL;
}
