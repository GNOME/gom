/* gom-related-model.c
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

#include "gom-related-model.h"
#include "gom-expression.h"
#include "gom-ordering.h"
#include "gom-entity-private.h"
#include "gom-cursor-private.h"
#include "gom-meta-private.h"
#include "gom-query-private.h"
#include "gom-record.h"
#include "gom-driver-private.h"
#include "gom-repository-private.h"
#include "gom-session-private.h"

struct _GomRelatedModel
{
  GObject parent_instance;

  GomEntity  *owner;
  char       *relationship_name;
  GListStore *items;
  guint       loading : 1;
};

enum
{
  PROP_0,
  PROP_OWNER,
  PROP_RELATIONSHIP_NAME,
  PROP_LOADING,
  N_PROPS
};

static void gom_related_model_list_model_init (GListModelInterface *iface);

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE_WITH_CODE (GomRelatedModel, gom_related_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gom_related_model_list_model_init))

static void
gom_related_model_finalize (GObject *object)
{
  GomRelatedModel *self = GOM_RELATED_MODEL (object);

  g_clear_object (&self->owner);
  g_clear_pointer (&self->relationship_name, g_free);
  g_clear_object (&self->items);

  G_OBJECT_CLASS (gom_related_model_parent_class)->finalize (object);
}

static void
gom_related_model_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GomRelatedModel *self = GOM_RELATED_MODEL (object);

  switch (prop_id)
    {
    case PROP_OWNER:
      g_value_set_object (value, self->owner);
      break;

    case PROP_RELATIONSHIP_NAME:
      g_value_set_string (value, self->relationship_name);
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, self->loading);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_related_model_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GomRelatedModel *self = GOM_RELATED_MODEL (object);

  switch (prop_id)
    {
    case PROP_OWNER:
      self->owner = g_value_dup_object (value);
      break;

    case PROP_RELATIONSHIP_NAME:
      self->relationship_name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_related_model_class_init (GomRelatedModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_related_model_finalize;
  object_class->get_property = gom_related_model_get_property;
  object_class->set_property = gom_related_model_set_property;

  properties[PROP_OWNER] =
    g_param_spec_object ("owner", NULL, NULL,
                         GOM_TYPE_ENTITY,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_RELATIONSHIP_NAME] =
    g_param_spec_string ("relationship-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  properties[PROP_LOADING] =
    g_param_spec_boolean ("loading", NULL, NULL,
                          FALSE,
                          (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gom_related_model_init (GomRelatedModel *self)
{
  self->items = g_list_store_new (G_TYPE_OBJECT);
}

static GType
gom_related_model_get_item_type_iface (GListModel *model)
{
  GomRelatedModel *self = GOM_RELATED_MODEL (model);
  GomEntityRelationshipInfo *relationship;

  if (self->owner == NULL || self->relationship_name == NULL)
    return G_TYPE_OBJECT;

  relationship = _gom_entity_class_get_relationship (GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self->owner)),
                                                     self->relationship_name,
                                                     FALSE);
  if (relationship == NULL)
    return G_TYPE_OBJECT;

  return relationship->target_type;
}

static inline guint
gom_related_model_count_strv (const char * const *strv)
{
  if (strv == NULL)
    return 0;

  return g_strv_length ((char **)strv);
}

static guint
gom_related_model_get_n_items_iface (GListModel *model)
{
  GomRelatedModel *self = GOM_RELATED_MODEL (model);

  return g_list_model_get_n_items (G_LIST_MODEL (self->items));
}

static gpointer
gom_related_model_get_item_iface (GListModel *model,
                                  guint       position)
{
  GomRelatedModel *self = GOM_RELATED_MODEL (model);

  return g_list_model_get_item (G_LIST_MODEL (self->items), position);
}

static void
gom_related_model_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gom_related_model_get_item_type_iface;
  iface->get_n_items = gom_related_model_get_n_items_iface;
  iface->get_item = gom_related_model_get_item_iface;
}

static void
gom_related_model_sync_items (GomRelatedModel *self,
                              GListModel      *model)
{
  g_autoptr(GPtrArray) current_items = NULL;
  g_autoptr(GPtrArray) new_items = NULL;
  guint n_current;
  guint n_new;
  guint prefix = 0;
  guint suffix = 0;

  g_assert (GOM_IS_RELATED_MODEL (self));
  g_assert (G_IS_LIST_MODEL (model));

  n_current = g_list_model_get_n_items (G_LIST_MODEL (self->items));
  n_new = g_list_model_get_n_items (model);

  new_items = g_ptr_array_new_with_free_func (g_object_unref);
  current_items = g_ptr_array_new_with_free_func (g_object_unref);

  for (guint i = 0; i < n_current; i++)
    g_ptr_array_add (current_items, g_list_model_get_item (G_LIST_MODEL (self->items), i));

  for (guint i = 0; i < n_new; i++)
    g_ptr_array_add (new_items, g_list_model_get_item (model, i));

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

static GomExpression *
gom_related_model_build_field_filter (GomRelatedModel     *self,
                                      const char          *relationship_name,
                                      const char * const  *source_fields,
                                      const char * const  *target_fields,
                                      GError             **error)
{
  g_autoptr(GomExpression) filter = NULL;
  GomEntityClass *entity_class;
  GObjectClass *object_class;

  g_assert (GOM_IS_RELATED_MODEL (self));
  g_assert (source_fields != NULL);
  g_assert (target_fields != NULL);

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self->owner));
  object_class = G_OBJECT_GET_CLASS (self->owner);

  if (source_fields == NULL || source_fields[0] == NULL ||
      target_fields == NULL || target_fields[0] == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Relationship `%s` does not provide enough field mapping information",
                   relationship_name);
      return NULL;
    }

  for (guint i = 0; source_fields[i] != NULL && target_fields[i] != NULL; i++)
    {
      g_auto(GValue) value = G_VALUE_INIT;
      g_autoptr(GomExpression) predicate = NULL;
      g_autoptr(GomExpression) left = NULL;
      g_autoptr(GomExpression) right = NULL;

      if (!gom_entity_get_property_storage_value (self->owner, entity_class, object_class, source_fields[i], &value, error))
        return NULL;

      left = gom_field_expression_new (target_fields[i]);
      right = gom_literal_expression_new (&value);
      predicate = gom_binary_expression_new_equal (g_steal_pointer (&left),
                                                   g_steal_pointer (&right));

      if (filter == NULL)
        filter = g_steal_pointer (&predicate);
      else
        filter = gom_binary_expression_new_and (g_steal_pointer (&filter),
                                                g_steal_pointer (&predicate));
    }

  if (gom_related_model_count_strv (source_fields) != gom_related_model_count_strv (target_fields))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Relationship `%s` field cardinality does not match",
                   relationship_name);
      return NULL;
    }

  return g_steal_pointer (&filter);
}

static GomExpression *
gom_related_model_build_relationship_filter (GomRelatedModel            *self,
                                             GomEntityRelationshipInfo  *relationship,
                                             GError                    **error)
{
  g_assert (relationship != NULL);

  return gom_related_model_build_field_filter (self,
                                               self->relationship_name,
                                               gom_entity_class_get_identity_fields (GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self->owner))),
                                               (const char * const *) relationship->remote_fields,
                                               error);
}

static GomExpression *
gom_related_model_build_record_filter (GomRelatedModel            *self,
                                       GomEntityRelationshipInfo  *relationship,
                                       GListModel                 *records,
                                       GError                    **error)
{
  g_autoptr(GomExpression) filter = NULL;
  GomEntityClass *target_class;
  const char * const *identity_fields;
  const char * const *remote_fields;
  guint n_identity_fields;
  guint n_remote_fields;
  guint n_records;

  g_assert (GOM_IS_RELATED_MODEL (self));
  g_assert (relationship != NULL);
  g_assert (G_IS_LIST_MODEL (records));

  target_class = g_type_class_get (relationship->target_type);
  remote_fields = (const char * const *)relationship->join_remote_fields;

  identity_fields = gom_entity_class_get_identity_fields (target_class);
  if (identity_fields == NULL || identity_fields[0] == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Target entity type `%s` has no identity fields",
                   g_type_name (relationship->target_type));
      return NULL;
    }

  n_identity_fields = gom_related_model_count_strv (identity_fields);
  n_remote_fields = gom_related_model_count_strv (remote_fields);
  if (n_identity_fields != n_remote_fields)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Relationship `%s` field cardinality does not match",
                   relationship->name);
      return NULL;
    }

  if (!(n_records = g_list_model_get_n_items (records)))
    return gom_literal_expression_new_boolean (FALSE);

  for (guint i = 0; i < n_records; i++)
    {
      g_autoptr(GomRecord) record = NULL;
      g_autoptr(GomExpression) record_filter = NULL;

      if (!(record = g_list_model_get_item (records, i)))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to read join-table record");
          return NULL;
        }

      for (guint j = 0; identity_fields[j] != NULL && remote_fields[j] != NULL; j++)
        {
          g_auto(GValue) value = G_VALUE_INIT;
          g_autoptr(GomExpression) left = NULL;
          g_autoptr(GomExpression) right = NULL;
          g_autoptr(GomExpression) predicate = NULL;

          if (!gom_record_get_column_by_name (record, remote_fields[j], &value))
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           "Join record is missing field `%s`",
                           remote_fields[j]);
              return NULL;
            }

          left = gom_field_expression_new (identity_fields[j]);
          right = gom_literal_expression_new (&value);
          predicate = gom_binary_expression_new_equal (g_steal_pointer (&left),
                                                       g_steal_pointer (&right));

          if (record_filter == NULL)
            record_filter = g_steal_pointer (&predicate);
          else
            record_filter = gom_binary_expression_new_and (g_steal_pointer (&record_filter),
                                                           g_steal_pointer (&predicate));
        }

      if (filter == NULL)
        filter = g_steal_pointer (&record_filter);
      else
        filter = gom_binary_expression_new_or (g_steal_pointer (&filter),
                                               g_steal_pointer (&record_filter));
    }

  return g_steal_pointer (&filter);
}

static DexFuture *
gom_related_model_query_cursor (GomRelatedModel           *self,
                                GomEntityRelationshipInfo *relationship,
                                GomExpression             *filter)
{
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomRepository) repository = NULL;

  g_assert (GOM_IS_RELATED_MODEL (self));
  g_assert (relationship != NULL);

  query = _gom_query_new (G_TYPE_INVALID,
                          relationship->join_relation,
                          NULL,
                          filter,
                          NULL,
                          NULL,
                          NULL,
                          0,
                          0,
                          FALSE,
                          FALSE,
                          FALSE);

  if ((session = _gom_entity_dup_session (self->owner)))
    return _gom_session_query (session, query);

  if ((repository = gom_entity_dup_repository (self->owner)))
    {
      driver = gom_repository_dup_driver (repository);
      return _gom_driver_query (driver,
                                repository,
                                query,
                                GOM_CURSOR_FLAGS_NONE);
    }

  return dex_future_new_reject (G_IO_ERROR,
                                G_IO_ERROR_INVALID_ARGUMENT,
                                "Entity is not bound to a repository");
}

static DexFuture *
gom_related_model_reload_fiber (gpointer user_data)
{
  GomRelatedModel *self = user_data;
  g_autoptr(GomSession) session = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomExpression) join_filter = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) cursor = NULL;
  g_autoptr(GObject) results = NULL;
  g_autoptr(GObject) records = NULL;
  GomEntityClass *entity_class;
  GomEntityRelationshipInfo *relationship;
  const char * const *identity_fields;

  g_assert (GOM_IS_RELATED_MODEL (self));
  g_assert (GOM_IS_ENTITY (self->owner));

  self->loading = TRUE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

  entity_class = GOM_ENTITY_CLASS (G_OBJECT_GET_CLASS (self->owner));
  if (!(relationship = _gom_entity_class_get_relationship (entity_class, self->relationship_name, FALSE)))
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Entity type `%s` does not define relationship `%s`",
                   G_OBJECT_TYPE_NAME (self->owner),
                   self->relationship_name);
      goto fail;
    }

  if (relationship->cardinality != GOM_RELATIONSHIP_CARDINALITY_TO_MANY ||
      (relationship->storage != GOM_RELATIONSHIP_STORAGE_FK &&
       relationship->storage != GOM_RELATIONSHIP_STORAGE_JOIN_TABLE))
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Relationship `%s` is not supported by the related-model loader yet",
                   self->relationship_name);
      goto fail;
    }

  session = _gom_entity_dup_session (self->owner);
  repository = gom_entity_dup_repository (self->owner);

  if (relationship->storage == GOM_RELATIONSHIP_STORAGE_FK)
    {
      if (!(filter = gom_related_model_build_relationship_filter (self, relationship, &error)))
        goto fail;

      if (session != NULL)
        results = dex_await_object (gom_session_list_entities (session, relationship->target_type, filter, NULL), &error);
      else if (repository != NULL)
        results = dex_await_object (gom_repository_list_entities (repository, relationship->target_type, filter, NULL), &error);
      else
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Entity is not bound to a repository");
          goto fail;
        }
    }
  else
    {
      identity_fields = gom_entity_class_get_identity_fields (entity_class);

      if (identity_fields == NULL || identity_fields[0] == NULL)
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Entity type `%s` has no identity fields",
                       G_OBJECT_TYPE_NAME (self->owner));
          goto fail;
        }

      join_filter = gom_related_model_build_field_filter (self,
                                                          self->relationship_name,
                                                          identity_fields,
                                                          (const char * const *) relationship->join_local_fields,
                                                          &error);
      if (join_filter == NULL)
        goto fail;

      if (relationship->join_relation == NULL || relationship->join_relation[0] == '\0')
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       "Relationship `%s` is missing a join relation",
                       self->relationship_name);
          goto fail;
        }

      if (session == NULL && repository == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               "Entity is not bound to a repository");
          goto fail;
        }

      if (!(cursor = dex_await_object (gom_related_model_query_cursor (self, relationship, join_filter), &error)))
        goto fail;

      if (!(records = dex_await_object (_gom_cursor_exhaust_to_records (GOM_CURSOR (cursor)), &error)))
        goto fail;

      if (!(filter = gom_related_model_build_record_filter (self, relationship, G_LIST_MODEL (records), &error)))
        goto fail;

      if (g_list_model_get_n_items (G_LIST_MODEL (records)) == 0)
        {
          g_list_store_remove_all (self->items);
          self->loading = FALSE;
          g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
          return dex_future_new_take_object (g_object_ref (self));
        }

      if (session != NULL)
        results = dex_await_object (gom_session_list_entities (session, relationship->target_type, filter, NULL), &error);
      else
        results = dex_await_object (gom_repository_list_entities (repository, relationship->target_type, filter, NULL), &error);
    }

  if (results == NULL)
    goto fail;

  gom_related_model_sync_items (self, G_LIST_MODEL (results));
  self->loading = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

  return dex_future_new_take_object (g_object_ref (self));

fail:
  g_list_store_remove_all (self->items);
  self->loading = FALSE;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

  return dex_future_new_for_error (g_steal_pointer (&error));
}

/**
 * gom_related_model_new:
 * @owner: a [class@Gom.Entity]
 * @relationship_name: the relationship to load
 *
 * Creates a related model for @relationship_name.
 *
 * Returns: (transfer full): a new [class@Gom.RelatedModel]
 */
GomRelatedModel *
gom_related_model_new (GomEntity  *owner,
                       const char *relationship_name)
{
  g_return_val_if_fail (GOM_IS_ENTITY (owner), NULL);
  g_return_val_if_fail (relationship_name != NULL, NULL);

  return g_object_new (GOM_TYPE_RELATED_MODEL,
                       "owner", owner,
                       "relationship-name", relationship_name,
                       NULL);
}

/**
 * gom_related_model_reload:
 * @self: a [class@Gom.RelatedModel]
 *
 * Forces a new load of the related collection from the backing relationship.
 *
 * Use this when the relationship may have changed, when you need a hard
 * resync from the repository, or when you want to discard any cached related
 * objects and rebuild the collection from scratch.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 */
DexFuture *
gom_related_model_reload (GomRelatedModel *self)
{
  g_return_val_if_fail (GOM_IS_RELATED_MODEL (self), NULL);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_related_model_reload_fiber,
                              g_object_ref (self),
                              g_object_unref);
}

/**
 * gom_related_model_refresh:
 * @self: a [class@Gom.RelatedModel]
 *
 * Re-loads the related collection from the backing relationship.
 *
 * This is currently an alias for [method@Gom.RelatedModel.reload], but it is
 * intended for callers that want to express "make the collection reflect the
 * current state of the relationship" rather than "recreate the collection
 * from scratch". Use it when you are reacting to external data changes and
 * simply want the related objects to catch up.
 *
 * Returns: (transfer full): a [class@Dex.Future] that resolves to @self
 */
DexFuture *
gom_related_model_refresh (GomRelatedModel *self)
{
  return gom_related_model_reload (self);
}

gboolean
gom_related_model_get_loading (GomRelatedModel *self)
{
  g_return_val_if_fail (GOM_IS_RELATED_MODEL (self), FALSE);

  return self->loading;
}

/**
 * gom_related_model_dup_owner:
 * @self: a [class@Gom.RelatedModel]
 *
 * Returns: (transfer full) (nullable): the owning entity
 */
GomEntity *
gom_related_model_dup_owner (GomRelatedModel *self)
{
  g_return_val_if_fail (GOM_IS_RELATED_MODEL (self), NULL);

  return self->owner ? g_object_ref (self->owner) : NULL;
}

const char *
gom_related_model_get_relationship_name (GomRelatedModel *self)
{
  g_return_val_if_fail (GOM_IS_RELATED_MODEL (self), NULL);

  return self->relationship_name;
}
