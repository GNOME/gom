/* gom-query-model.c
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
#include "gom-expression.h"
#include "gom-ordering.h"
#include "gom-query-private.h"
#include "gom-query-model.h"
#include "gom-trace-private.h"
#include "gom-session-private.h"

struct _GomQueryModel
{
  GObject parent_instance;

  GomSession    *session;
  GType          entity_type;
  GomExpression *filter;
  GomOrdering   *ordering;
  GomQuery      *query;
  GListStore    *items;
  DexFuture     *reload_future;
  guint          loading : 1;
  guint          refresh_pending : 1;
};

enum
{
  PROP_0,
  PROP_SESSION,
  PROP_ENTITY_TYPE,
  PROP_FILTER,
  PROP_ORDERING,
  PROP_LOADING,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void       gom_query_model_list_model_init    (GListModelInterface *iface);
static void       gom_query_model_finalize           (GObject             *object);
static void       gom_query_model_constructed        (GObject             *object);
static void       gom_query_model_get_property       (GObject             *object,
                                                      guint                prop_id,
                                                      GValue              *value,
                                                      GParamSpec          *pspec);
static void       gom_query_model_set_property       (GObject             *object,
                                                      guint                prop_id,
                                                      const GValue        *value,
                                                      GParamSpec          *pspec);
static void       gom_query_model_sync_items         (GomQueryModel       *self,
                                                      GListModel          *results);
static void       gom_query_model_items_changed_cb   (GListModel          *list,
                                                      guint                position,
                                                      guint                removed,
                                                      guint                added,
                                                      gpointer             user_data);
static DexFuture *gom_query_model_reload_fiber       (gpointer             user_data);
static DexFuture *gom_query_model_request_reload     (GomQueryModel       *self);
static void       gom_query_model_session_changed_cb (GomSession          *session,
                                                      GomQueryModel       *self);

G_DEFINE_TYPE_WITH_CODE (GomQueryModel,
                         gom_query_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gom_query_model_list_model_init))

static void
gom_query_model_class_init (GomQueryModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gom_query_model_constructed;
  object_class->finalize = gom_query_model_finalize;
  object_class->get_property = gom_query_model_get_property;
  object_class->set_property = gom_query_model_set_property;

  properties[PROP_SESSION] =
    g_param_spec_object ("session", NULL, NULL,
                         GOM_TYPE_SESSION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ENTITY_TYPE] =
    g_param_spec_gtype ("entity-type", NULL, NULL,
                        GOM_TYPE_ENTITY,
                        (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_FILTER] =
    g_param_spec_object ("filter", NULL, NULL,
                         GOM_TYPE_EXPRESSION,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_ORDERING] =
    g_param_spec_object ("ordering", NULL, NULL,
                         GOM_TYPE_ORDERING,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_LOADING] =
    g_param_spec_boolean ("loading", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_query_model_init (GomQueryModel *self)
{
  self->items = g_list_store_new (G_TYPE_OBJECT);
  self->entity_type = G_TYPE_INVALID;

  g_signal_connect_object (self->items,
                           "items-changed",
                           G_CALLBACK (gom_query_model_items_changed_cb),
                           self,
                           0);
}

static void
gom_query_model_finalize (GObject *object)
{
  GomQueryModel *self = GOM_QUERY_MODEL (object);

  g_clear_object (&self->session);
  g_clear_object (&self->filter);
  g_clear_object (&self->ordering);
  g_clear_object (&self->query);
  g_clear_object (&self->items);
  g_clear_pointer (&self->reload_future, dex_unref);

  G_OBJECT_CLASS (gom_query_model_parent_class)->finalize (object);
}

static void
gom_query_model_constructed (GObject *object)
{
  GomQueryModel *self = GOM_QUERY_MODEL (object);

  G_OBJECT_CLASS (gom_query_model_parent_class)->constructed (object);

  if (self->query == NULL && self->entity_type != G_TYPE_INVALID)
    {
      g_autoptr(GPtrArray) orderings = NULL;

      if (self->ordering != NULL)
        {
          orderings = g_ptr_array_new_with_free_func (g_object_unref);
          g_ptr_array_add (orderings, g_object_ref (self->ordering));
        }

      self->query = _gom_query_new (self->entity_type,
                                    NULL,
                                    NULL,
                                    self->filter,
                                    NULL,
                                    NULL,
                                    orderings,
                                    0,
                                    0,
                                    FALSE,
                                    FALSE,
                                    FALSE);
    }

  if (self->session != NULL)
    g_signal_connect_object (self->session,
                             "changed",
                             G_CALLBACK (gom_query_model_session_changed_cb),
                             self,
                             0);
}

static void
gom_query_model_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GomQueryModel *self = GOM_QUERY_MODEL (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      g_value_set_object (value, self->session);
      break;

    case PROP_ENTITY_TYPE:
      g_value_set_gtype (value, self->entity_type);
      break;

    case PROP_FILTER:
      g_value_set_object (value, self->filter);
      break;

    case PROP_ORDERING:
      g_value_set_object (value, self->ordering);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, self->loading);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_query_model_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GomQueryModel *self = GOM_QUERY_MODEL (object);

  switch (prop_id)
    {
    case PROP_SESSION:
      g_set_object (&self->session, g_value_get_object (value));
      break;

    case PROP_ENTITY_TYPE:
      self->entity_type = g_value_get_gtype (value);
      break;

    case PROP_FILTER:
      g_set_object (&self->filter, g_value_get_object (value));
      break;

    case PROP_ORDERING:
      g_set_object (&self->ordering, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GType
gom_query_model_get_item_type_iface (GListModel *model)
{
  GomQueryModel *self = GOM_QUERY_MODEL (model);

  if (self->query == NULL)
    return G_TYPE_OBJECT;

  return _gom_query_get_target_entity_type (self->query);
}

static guint
gom_query_model_get_n_items_iface (GListModel *model)
{
  GomQueryModel *self = GOM_QUERY_MODEL (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->items));
}

static gpointer
gom_query_model_get_item_iface (GListModel *model,
                                guint       position)
{
  GomQueryModel *self = GOM_QUERY_MODEL (model);

  return g_list_model_get_item (G_LIST_MODEL (self->items), position);
}

static void
gom_query_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gom_query_model_get_item_type_iface;
  iface->get_n_items = gom_query_model_get_n_items_iface;
  iface->get_item = gom_query_model_get_item_iface;
}

static void
gom_query_model_sync_items (GomQueryModel *self,
                            GListModel    *results)
{
  g_autoptr(GPtrArray) current_items = NULL;
  g_autoptr(GPtrArray) new_items = NULL;
  guint n_current;
  guint n_new;
  guint prefix = 0;
  guint suffix = 0;

  g_assert (GOM_IS_QUERY_MODEL (self));
  g_assert (G_IS_LIST_MODEL (results));

  n_current = g_list_model_get_n_items (G_LIST_MODEL (self->items));
  n_new = g_list_model_get_n_items (results);

  new_items = g_ptr_array_new_with_free_func (g_object_unref);
  current_items = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < n_current; i++)
    g_ptr_array_add (current_items, g_list_model_get_item (G_LIST_MODEL (self->items), i));

  for (guint i = 0; i < n_new; i++)
    g_ptr_array_add (new_items, g_list_model_get_item (results, i));

  while (prefix < n_current && prefix < n_new &&
         g_ptr_array_index (current_items, prefix) == g_ptr_array_index (new_items, prefix))
    prefix++;

  while (suffix + prefix < n_current && suffix + prefix < n_new &&
         g_ptr_array_index (current_items, n_current - suffix - 1) ==
           g_ptr_array_index (new_items, n_new - suffix - 1))
    suffix++;

  if (prefix == n_current && n_current == n_new)
    return;

  if (suffix == 0 && prefix == 0)
    {
      g_list_store_splice (self->items, 0, n_current, (gpointer *)new_items->pdata, n_new);
      return;
    }

  g_list_store_splice (self->items,
                       prefix,
                       n_current - prefix - suffix,
                       (gpointer *)new_items->pdata + prefix,
                       n_new - prefix - suffix);
}

static void
gom_query_model_items_changed_cb (GListModel *list,
                                  guint       position,
                                  guint       removed,
                                  guint       added,
                                  gpointer    user_data)
{
  GomQueryModel *self = user_data;

  g_assert (G_IS_LIST_MODEL (list));
  g_assert (GOM_IS_QUERY_MODEL (self));

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static DexFuture *
gom_query_model_reload_fiber (gpointer user_data)
{
  GomQueryModel *self = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) results = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomCursor) cursor = NULL;

  g_assert (GOM_IS_QUERY_MODEL (self));

  session = g_object_ref (self->session);
  if (!(cursor = dex_await_object (gom_session_query (session, self->query), &error)))
    goto fail;

  if (!(results = dex_await_object (gom_cursor_exhaust_to_list (cursor), &error)))
    goto fail;

  gom_query_model_sync_items (self, G_LIST_MODEL (results));

  self->loading = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

  g_clear_pointer (&self->reload_future, dex_unref);

  if (self->refresh_pending)
    {
      g_autoptr(DexFuture) future = NULL;

      self->refresh_pending = FALSE;
      future = gom_query_model_reload (self);
    }

  return dex_future_new_take_object (g_object_ref (self));

fail:
  g_list_store_remove_all (self->items);
  self->loading = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
  g_clear_pointer (&self->reload_future, dex_unref);

  if (self->refresh_pending)
    {
      g_autoptr(DexFuture) future = NULL;

      self->refresh_pending = FALSE;
      future = gom_query_model_reload (self);
    }

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static DexFuture *
gom_query_model_request_reload (GomQueryModel *self)
{
  DexFuture *future;
  gint64 start_time;

  g_assert (GOM_IS_QUERY_MODEL (self));

  if (self->loading)
    {
      self->refresh_pending = TRUE;

      return self->reload_future ? dex_ref (self->reload_future)
                                 : dex_future_new_true ();
    }

  self->loading = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
  start_time = GOM_TRACE_BEGIN_MARK ();

  future = dex_scheduler_spawn (NULL,
                                0,
                                gom_query_model_reload_fiber,
                                g_object_ref (self),
                                g_object_unref);

  dex_set_object (&self->reload_future, future);

  return GOM_TRACE_MARKED_FUTURE (future, start_time, "QueryModel", "reload", "session=%p", self->session);
}

static void
gom_query_model_session_changed_cb (GomSession    *session,
                                    GomQueryModel *self)
{
  g_assert (GOM_IS_SESSION (session));
  g_assert (GOM_IS_QUERY_MODEL (self));

  gom_query_model_request_reload (self);
}

/**
 * gom_query_model_new:
 * @session: a [class@Gom.Session]
 * @entity_type: a [GType] for a [class@Gom.Entity] subclass
 * @filter: (nullable): a [class@Gom.Expression] used as the query filter
 * @ordering: (nullable): a [class@Gom.Ordering] used to sort results
 *
 * Creates a live query-backed model for @entity_type.
 *
 * The model reloads automatically when the bound session emits `changed`.
 *
 * Returns: (transfer full): a new [class@Gom.QueryModel]
 */
GomQueryModel *
gom_query_model_new (GomSession    *session,
                     GType          entity_type,
                     GomExpression *filter,
                     GomOrdering   *ordering)
{
  g_return_val_if_fail (GOM_IS_SESSION (session), NULL);
  g_return_val_if_fail (g_type_is_a (entity_type, GOM_TYPE_ENTITY), NULL);

  return g_object_new (GOM_TYPE_QUERY_MODEL,
                       "session", session,
                       "entity-type", entity_type,
                       "filter", filter,
                       "ordering", ordering,
                       NULL);
}

/**
 * gom_query_model_reload:
 * @self: a [class@Gom.QueryModel]
 *
 * Forces a new query against the current session and replaces the model's
 * contents with the latest matching rows.
 *
 * Use this when the query definition itself may have changed, when you need a
 * hard resync from the database, or when you want to discard any currently
 * cached results and rebuild the model from scratch.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 */
DexFuture *
gom_query_model_reload (GomQueryModel *self)
{
  g_return_val_if_fail (GOM_IS_QUERY_MODEL (self), NULL);

  return gom_query_model_request_reload (self);
}

/**
 * gom_query_model_refresh:
 * @self: a [class@Gom.QueryModel]
 *
 * Re-queries the session and updates the model contents.
 *
 * This is currently an alias for [method@Gom.QueryModel.reload], but it is
 * intended for callers that want to express "make the model reflect the
 * current state of the database" rather than "recreate the model's query
 * results from scratch". Keep using this when you are reacting to external
 * data changes and simply want the model to catch up.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 */
DexFuture *
gom_query_model_refresh (GomQueryModel *self)
{
  g_return_val_if_fail (GOM_IS_QUERY_MODEL (self), NULL);

  return gom_query_model_reload (self);
}

gboolean
gom_query_model_get_loading (GomQueryModel *self)
{
  g_return_val_if_fail (GOM_IS_QUERY_MODEL (self), FALSE);

  return self->loading;
}

/**
 * gom_query_model_dup_session:
 * @self: a [class@Gom.QueryModel]
 *
 * Returns: (transfer full) (nullable): the bound session
 */
GomSession *
gom_query_model_dup_session (GomQueryModel *self)
{
  g_return_val_if_fail (GOM_IS_QUERY_MODEL (self), NULL);

  return self->session ? g_object_ref (self->session) : NULL;
}

GType
gom_query_model_get_entity_type (GomQueryModel *self)
{
  g_return_val_if_fail (GOM_IS_QUERY_MODEL (self), G_TYPE_INVALID);

  return self->query != NULL ? _gom_query_get_target_entity_type (self->query)
                             : self->entity_type;
}
