/* test-gom-sync.c
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

#include <libgom.h>

#include "lib/gom-delta.h"
#include "lib/gom-repository-private.h"
#include "lib/gom-session-private.h"
#include "lib/gom-sync-coordinator-private.h"
#include "lib/gom-sync-history-private.h"
#include "lib/gom-tombstone-private.h"
#include "gom-mock-driver-private.h"
#include "test-util.h"

typedef struct _TestSyncEntity           TestSyncEntity;
typedef struct _TestSyncEntityClass      TestSyncEntityClass;
typedef struct _TestOtherSyncEntity      TestOtherSyncEntity;
typedef struct _TestOtherSyncEntityClass TestOtherSyncEntityClass;
typedef struct _TestSyncSession          TestSyncSession;
typedef struct _TestSyncSessionClass     TestSyncSessionClass;
typedef struct _TestSyncTransport        TestSyncTransport;
typedef struct _TestSyncTransportClass   TestSyncTransportClass;
typedef struct _TestMergePolicy          TestMergePolicy;
typedef struct _TestMergePolicyClass     TestMergePolicyClass;
typedef struct _TestMemorySyncChange     TestMemorySyncChange;
typedef struct _TestMemorySyncHistory    TestMemorySyncHistory;
typedef struct _TestMemorySyncValue      TestMemorySyncValue;

struct _TestSyncEntity
{
  GomEntity parent_instance;
  int       id;
  int       value;
  int       remote_value;
};

struct _TestSyncEntityClass
{
  GomEntityClass parent_class;
};

struct _TestOtherSyncEntity
{
  GomEntity parent_instance;
};

struct _TestOtherSyncEntityClass
{
  GomEntityClass parent_class;
};

struct _TestSyncSession
{
  GomSession parent_instance;
  guint      commit_count;
  guint      rollback_count;
};

struct _TestSyncSessionClass
{
  GomSessionClass parent_class;
};

struct _TestSyncTransport
{
  GomSyncTransport       parent_instance;
  TestMemorySyncHistory *history;
  GQueue                 inbound;
  guint64                next_remote_sequence;
  guint64                last_applied_remote_sequence;
  guint                  stage_count;
  guint                  push_count;
  guint                  pull_count;
  guint                  applied_remote_count;
  GomRepository         *last_repository;
  GomSession            *last_session;
  GomEntity             *last_entity;
  GomDelta              *last_delta;
  GomDelta              *last_merged_delta;
  guint                  fail_stage : 1;
  guint                  fail_push : 1;
  guint                  apply_remote : 1;
};

struct _TestSyncTransportClass
{
  GomSyncTransportClass parent_class;
};

struct _TestMergePolicy
{
  GomMergePolicy  parent_instance;
  GomDelta       *apply_delta;
  GomDelta       *last_local_delta;
  GomDelta       *last_remote_delta;
  guint           merge_count;
  guint           reject_same_field : 1;
  guint           reject : 1;
  guint           skip_resolution : 1;
};

struct _TestMergePolicyClass
{
  GomMergePolicyClass parent_class;
};

struct _TestMemorySyncValue
{
  char   *name;
  GValue  value;
};

struct _TestMemorySyncChange
{
  guint64        sequence;
  guint64        remote_sequence;
  guint          batch;
  GType          entity_type;
  char          *relation;
  char          *identity_string;
  GPtrArray     *identity;
  GomDeltaKind   kind;
  GomDelta      *delta;
  GomRepository *repository;
  guint          tombstone : 1;
  guint          outbound : 1;
  guint          sent : 1;
  guint          acked : 1;
};

struct _TestMemorySyncHistory
{
  GPtrArray *changes;
  GQueue     sent;
  guint64    next_sequence;
  guint      next_batch;
};

static GType test_sync_entity_get_type       (void);
static GType test_other_sync_entity_get_type (void);
static GType test_sync_session_get_type      (void);
static GType test_sync_transport_get_type    (void);
static GType test_merge_policy_get_type      (void);

#define TEST_TYPE_SYNC_ENTITY       (test_sync_entity_get_type())
#define TEST_TYPE_OTHER_SYNC_ENTITY (test_other_sync_entity_get_type())
#define TEST_TYPE_SYNC_SESSION      (test_sync_session_get_type())
#define TEST_TYPE_SYNC_TRANSPORT    (test_sync_transport_get_type())
#define TEST_TYPE_MERGE_POLICY      (test_merge_policy_get_type())

G_DEFINE_TYPE (TestSyncEntity, test_sync_entity, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestOtherSyncEntity, test_other_sync_entity, GOM_TYPE_ENTITY)

enum {
  PROP_SYNC_ENTITY_0,
  PROP_SYNC_ENTITY_ID,
  PROP_SYNC_ENTITY_VALUE,
  PROP_SYNC_ENTITY_REMOTE_VALUE,
  N_SYNC_ENTITY_PROPS
};

static GParamSpec *sync_entity_props[N_SYNC_ENTITY_PROPS];

static void
test_sync_entity_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  TestSyncEntity *self = (TestSyncEntity *) object;

  switch (prop_id)
    {
    case PROP_SYNC_ENTITY_ID:
      g_value_set_int (value, self->id);
      break;

    case PROP_SYNC_ENTITY_VALUE:
      g_value_set_int (value, self->value);
      break;

    case PROP_SYNC_ENTITY_REMOTE_VALUE:
      g_value_set_int (value, self->remote_value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_sync_entity_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  TestSyncEntity *self = (TestSyncEntity *) object;

  switch (prop_id)
    {
    case PROP_SYNC_ENTITY_ID:
      self->id = g_value_get_int (value);
      break;

    case PROP_SYNC_ENTITY_VALUE:
      self->value = g_value_get_int (value);
      break;

    case PROP_SYNC_ENTITY_REMOTE_VALUE:
      self->remote_value = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
test_memory_sync_value_free (gpointer data)
{
  TestMemorySyncValue *value = data;

  if (value == NULL)
    return;

  g_clear_pointer (&value->name, g_free);

  if (G_IS_VALUE (&value->value))
    g_value_unset (&value->value);

  g_free (value);
}

static TestMemorySyncValue *
test_memory_sync_value_new (const char   *name,
                            const GValue *value)
{
  TestMemorySyncValue *copy;

  g_assert (name != NULL);
  g_assert (value != NULL);
  g_assert (G_IS_VALUE (value));

  copy = g_new0 (TestMemorySyncValue, 1);
  copy->name = g_strdup (name);
  g_value_init (&copy->value, G_VALUE_TYPE (value));
  g_value_copy (value, &copy->value);

  return copy;
}

static GomDelta *
test_delta_copy (GomDelta *delta)
{
  g_autoptr(GomDelta) copy = NULL;
  guint n_changes;

  g_assert (GOM_IS_DELTA (delta));

  copy = gom_delta_new (gom_delta_get_entity_type (delta),
                        gom_delta_get_kind (delta));
  n_changes = gom_delta_get_n_changes (delta);

  for (guint i = 0; i < n_changes; i++)
    {
      const char *property_name;
      GValue original_value = G_VALUE_INIT;
      GValue current_value = G_VALUE_INIT;
      gboolean has_original;
      gboolean has_current;

      property_name = gom_delta_get_property_name (delta, i);
      has_original = gom_delta_get_original_value (delta, i, &original_value);
      has_current = gom_delta_get_current_value (delta, i, &current_value);

      gom_delta_add_property (copy,
                              property_name,
                              has_original ? &original_value : NULL,
                              has_current ? &current_value : NULL);

      if (G_IS_VALUE (&original_value))
        g_value_unset (&original_value);

      if (G_IS_VALUE (&current_value))
        g_value_unset (&current_value);
    }

  return g_steal_pointer (&copy);
}

static void
test_delta_copy_property (GomDelta *src,
                          guint     index,
                          GomDelta *dest)
{
  const char *property_name;
  GValue original_value = G_VALUE_INIT;
  GValue current_value = G_VALUE_INIT;
  gboolean has_original;
  gboolean has_current;

  g_assert (GOM_IS_DELTA (src));
  g_assert (GOM_IS_DELTA (dest));

  property_name = gom_delta_get_property_name (src, index);
  has_original = gom_delta_get_original_value (src, index, &original_value);
  has_current = gom_delta_get_current_value (src, index, &current_value);

  gom_delta_add_property (dest,
                          property_name,
                          has_original ? &original_value : NULL,
                          has_current ? &current_value : NULL);

  if (G_IS_VALUE (&original_value))
    g_value_unset (&original_value);

  if (G_IS_VALUE (&current_value))
    g_value_unset (&current_value);
}

static gboolean
test_delta_has_property (GomDelta   *delta,
                         const char *property_name)
{
  guint n_changes;

  g_assert (GOM_IS_DELTA (delta));
  g_assert (property_name != NULL);

  n_changes = gom_delta_get_n_changes (delta);

  for (guint i = 0; i < n_changes; i++)
    {
      if (g_strcmp0 (gom_delta_get_property_name (delta, i), property_name) == 0)
        return TRUE;
    }

  return FALSE;
}

static gboolean
test_delta_has_overlapping_property (GomDelta *a,
                                     GomDelta *b)
{
  guint n_changes;

  g_assert (GOM_IS_DELTA (a));
  g_assert (GOM_IS_DELTA (b));

  n_changes = gom_delta_get_n_changes (a);

  for (guint i = 0; i < n_changes; i++)
    {
      if (test_delta_has_property (b, gom_delta_get_property_name (a, i)))
        return TRUE;
    }

  return FALSE;
}

static GomDelta *
test_delta_merge_disjoint (GomDelta *local_delta,
                           GomDelta *remote_delta)
{
  g_autoptr(GomDelta) merged = NULL;
  guint n_changes;

  g_assert (GOM_IS_DELTA (remote_delta));

  merged = test_delta_copy (remote_delta);

  if (local_delta == NULL)
    return g_steal_pointer (&merged);

  g_assert (GOM_IS_DELTA (local_delta));

  n_changes = gom_delta_get_n_changes (local_delta);

  for (guint i = 0; i < n_changes; i++)
    {
      const char *property_name = gom_delta_get_property_name (local_delta, i);

      if (!test_delta_has_property (merged, property_name))
        test_delta_copy_property (local_delta, i, merged);
    }

  return g_steal_pointer (&merged);
}

static void
test_memory_sync_change_free (gpointer data)
{
  TestMemorySyncChange *change = data;

  if (change == NULL)
    return;

  g_clear_pointer (&change->relation, g_free);
  g_clear_pointer (&change->identity_string, g_free);
  g_clear_pointer (&change->identity, g_ptr_array_unref);
  g_clear_object (&change->delta);
  g_clear_object (&change->repository);
  g_free (change);
}

static GPtrArray *
test_memory_sync_capture_identity (GomEntity *entity)
{
  GPtrArray *identity;
  GomEntityClass *entity_class;
  GObjectClass *object_class;
  const char * const *identity_fields;

  g_assert (GOM_IS_ENTITY (entity));

  identity = g_ptr_array_new_with_free_func (test_memory_sync_value_free);
  entity_class = GOM_ENTITY_GET_CLASS (entity);
  object_class = G_OBJECT_GET_CLASS (entity);
  identity_fields = gom_entity_class_get_identity_fields (entity_class);

  if (identity_fields == NULL)
    return identity;

  for (guint i = 0; identity_fields[i] != NULL; i++)
    {
      const char *identity_field = identity_fields[i];
      GParamSpec *pspec;
      GValue value = G_VALUE_INIT;

      pspec = g_object_class_find_property (object_class, identity_field);
      if (pspec == NULL)
        continue;

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (G_OBJECT (entity), identity_field, &value);
      g_ptr_array_add (identity, test_memory_sync_value_new (identity_field, &value));
      g_value_unset (&value);
    }

  return identity;
}

static TestMemorySyncChange *
test_memory_sync_change_new (GomRepository *repository,
                             GomEntity     *entity,
                             GomDelta      *delta,
                             gboolean       outbound)
{
  TestMemorySyncChange *change;
  GomEntityClass *entity_class;
  const char *relation;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (GOM_IS_DELTA (delta));

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  relation = gom_entity_class_get_relation (entity_class);

  change = g_new0 (TestMemorySyncChange, 1);
  change->entity_type = gom_delta_get_entity_type (delta);
  change->relation = g_strdup (relation);
  change->identity_string = _gom_tombstone_serialize_entity_identity (entity, NULL);
  change->identity = test_memory_sync_capture_identity (entity);
  change->kind = gom_delta_get_kind (delta);
  change->delta = test_delta_copy (delta);
  change->repository = g_object_ref (repository);
  change->tombstone = change->kind == GOM_DELTA_KIND_DELETE;
  change->outbound = outbound;

  return change;
}

static TestMemorySyncHistory *
test_memory_sync_history_new (void)
{
  TestMemorySyncHistory *history;

  history = g_new0 (TestMemorySyncHistory, 1);
  history->changes = g_ptr_array_new_with_free_func (test_memory_sync_change_free);
  history->next_sequence = 1;
  history->next_batch = 1;

  return history;
}

static void
test_memory_sync_history_free (TestMemorySyncHistory *history)
{
  if (history == NULL)
    return;

  g_queue_clear (&history->sent);
  g_clear_pointer (&history->changes, g_ptr_array_unref);
  g_free (history);
}

static TestMemorySyncChange *
test_memory_sync_history_append (TestMemorySyncHistory *history,
                                 GomRepository         *repository,
                                 GomEntity             *entity,
                                 GomDelta              *delta,
                                 gboolean               outbound)
{
  TestMemorySyncChange *change;

  g_assert (history != NULL);
  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (GOM_IS_DELTA (delta));

  change = test_memory_sync_change_new (repository, entity, delta, outbound);
  change->sequence = history->next_sequence++;
  change->batch = history->next_batch++;
  g_ptr_array_add (history->changes, change);

  return change;
}

static gboolean
test_memory_sync_value_equal (TestMemorySyncValue *a,
                              TestMemorySyncValue *b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  if (g_strcmp0 (a->name, b->name) != 0)
    return FALSE;

  if (!G_VALUE_HOLDS (&a->value, G_VALUE_TYPE (&b->value)))
    return FALSE;

  if (G_VALUE_HOLDS_INT (&a->value))
    return g_value_get_int (&a->value) == g_value_get_int (&b->value);

  if (G_VALUE_HOLDS_STRING (&a->value))
    return g_strcmp0 (g_value_get_string (&a->value),
                      g_value_get_string (&b->value)) == 0;

  return FALSE;
}

static gboolean
test_memory_sync_identity_equal (GPtrArray *a,
                                 GPtrArray *b)
{
  g_assert (a != NULL);
  g_assert (b != NULL);

  if (a->len != b->len)
    return FALSE;

  for (guint i = 0; i < a->len; i++)
    {
      TestMemorySyncValue *a_value = g_ptr_array_index (a, i);
      TestMemorySyncValue *b_value = g_ptr_array_index (b, i);

      if (!test_memory_sync_value_equal (a_value, b_value))
        return FALSE;
    }

  return TRUE;
}

static TestMemorySyncChange *
test_memory_sync_history_find_pending_local (TestMemorySyncHistory *history,
                                             TestMemorySyncChange  *remote)
{
  g_assert (history != NULL);
  g_assert (remote != NULL);

  for (guint i = 0; i < history->changes->len; i++)
    {
      TestMemorySyncChange *local = g_ptr_array_index (history->changes, i);

      if (!local->outbound || local->acked)
        continue;

      if (local->repository != remote->repository)
        continue;

      if (local->entity_type != remote->entity_type)
        continue;

      if (!test_memory_sync_identity_equal (local->identity, remote->identity))
        continue;

      return local;
    }

  return NULL;
}

static GPtrArray *
test_memory_sync_history_replay (TestMemorySyncHistory *history,
                                 guint64                sequence)
{
  GPtrArray *replay;

  g_assert (history != NULL);

  replay = g_ptr_array_new ();

  for (guint i = 0; i < history->changes->len; i++)
    {
      TestMemorySyncChange *change = g_ptr_array_index (history->changes, i);

      if (change->sequence > sequence)
        g_ptr_array_add (replay, change);
    }

  return replay;
}

static void
test_memory_sync_history_ack (TestMemorySyncHistory *history,
                              guint64                sequence)
{
  g_assert (history != NULL);

  for (guint i = 0; i < history->changes->len; i++)
    {
      TestMemorySyncChange *change = g_ptr_array_index (history->changes, i);

      if (change->outbound && change->sequence <= sequence)
        change->acked = TRUE;
    }
}

static guint
test_memory_sync_history_count_pending_outbound (TestMemorySyncHistory *history)
{
  guint count = 0;

  g_assert (history != NULL);

  for (guint i = 0; i < history->changes->len; i++)
    {
      TestMemorySyncChange *change = g_ptr_array_index (history->changes, i);

      if (change->outbound && !change->acked)
        count++;
    }

  return count;
}

static guint
test_memory_sync_history_count_unsent_outbound (TestMemorySyncHistory *history)
{
  guint count = 0;

  g_assert (history != NULL);

  for (guint i = 0; i < history->changes->len; i++)
    {
      TestMemorySyncChange *change = g_ptr_array_index (history->changes, i);

      if (change->outbound && !change->sent && !change->acked)
        count++;
    }

  return count;
}

static void
test_sync_transport_queue_inbound_delta_with_sequence (TestSyncTransport *self,
                                                       GomRepository     *repository,
                                                       GomEntity         *entity,
                                                       GomDelta          *delta,
                                                       guint64            remote_sequence)
{
  TestMemorySyncChange *change;

  g_assert (GOM_IS_SYNC_TRANSPORT (self));
  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (GOM_IS_ENTITY (entity));
  g_assert (GOM_IS_DELTA (delta));
  g_assert_cmpuint (remote_sequence, >, 0);

  change = test_memory_sync_history_append (self->history,
                                            repository,
                                            entity,
                                            delta,
                                            FALSE);
  change->remote_sequence = remote_sequence;
  g_queue_push_tail (&self->inbound, change);
}

static void
test_sync_transport_queue_inbound_delta (TestSyncTransport *self,
                                         GomRepository     *repository,
                                         GomEntity         *entity,
                                         GomDelta          *delta)
{
  g_assert (GOM_IS_SYNC_TRANSPORT (self));

  test_sync_transport_queue_inbound_delta_with_sequence (self,
                                                        repository,
                                                        entity,
                                                        delta,
                                                        self->next_remote_sequence++);
}

static void
test_sync_entity_class_init (TestSyncEntityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->get_property = test_sync_entity_get_property;
  object_class->set_property = test_sync_entity_set_property;

  sync_entity_props[PROP_SYNC_ENTITY_ID] =
    g_param_spec_int ("id", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  sync_entity_props[PROP_SYNC_ENTITY_VALUE] =
    g_param_spec_int ("value", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  sync_entity_props[PROP_SYNC_ENTITY_REMOTE_VALUE] =
    g_param_spec_int ("remote-value", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class,
                                     N_SYNC_ENTITY_PROPS,
                                     sync_entity_props);

  gom_entity_class_set_relation (entity_class, "test_sync_entities");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "value", TRUE);
  gom_entity_class_property_set_mapped (entity_class, "remote-value", TRUE);
  gom_entity_class_property_set_version_added (entity_class, "id", 1);
  gom_entity_class_property_set_version_added (entity_class, "value", 1);
  gom_entity_class_property_set_version_added (entity_class, "remote-value", 1);
}

static void
test_sync_entity_init (TestSyncEntity *self)
{
}

static void
test_other_sync_entity_class_init (TestOtherSyncEntityClass *klass)
{
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  gom_entity_class_set_relation (entity_class, "test_other_sync_entities");
}

static void
test_other_sync_entity_init (TestOtherSyncEntity *self)
{
}

G_DEFINE_TYPE (TestSyncSession, test_sync_session, GOM_TYPE_SESSION)

static DexFuture *
test_sync_session_flush (GomSession *session)
{
  g_assert_true (GOM_IS_SESSION (session));

  return dex_future_new_true ();
}

static DexFuture *
test_sync_session_commit (GomSession *session)
{
  TestSyncSession *self = (TestSyncSession *) session;

  g_assert_true (GOM_IS_SESSION (session));

  self->commit_count++;
  _gom_session_set_closed (session, TRUE);

  return dex_future_new_true ();
}

static DexFuture *
test_sync_session_rollback (GomSession *session)
{
  TestSyncSession *self = (TestSyncSession *) session;

  g_assert_true (GOM_IS_SESSION (session));

  self->rollback_count++;
  _gom_session_set_closed (session, TRUE);

  return dex_future_new_true ();
}

static void
test_sync_session_class_init (TestSyncSessionClass *klass)
{
  GomSessionClass *session_class = GOM_SESSION_CLASS (klass);

  session_class->flush = test_sync_session_flush;
  session_class->commit = test_sync_session_commit;
  session_class->rollback = test_sync_session_rollback;
}

static void
test_sync_session_init (TestSyncSession *self)
{
  self->commit_count = 0;
  self->rollback_count = 0;
}

G_DEFINE_TYPE (TestSyncTransport, test_sync_transport, GOM_TYPE_SYNC_TRANSPORT)

static DexFuture *
test_sync_transport_stage_local_change (GomSyncTransport   *transport,
                                        GomSyncCoordinator *coordinator,
                                        GomRepository      *repository,
                                        GomSession         *session,
                                        GomEntity          *entity,
                                        GomDelta           *delta)
{
  TestSyncTransport *self = (TestSyncTransport *) transport;

  g_assert_true (GOM_IS_SYNC_TRANSPORT (transport));
  g_assert_true (GOM_IS_SYNC_COORDINATOR (coordinator));
  g_assert_true (GOM_IS_REPOSITORY (repository));
  if (session != NULL)
    g_assert_true (GOM_IS_SESSION (session));
  g_assert_true (GOM_IS_ENTITY (entity));
  g_assert_true (GOM_IS_DELTA (delta));

  self->stage_count++;
  g_set_object (&self->last_repository, repository);
  g_set_object (&self->last_session, session);
  g_set_object (&self->last_entity, entity);
  g_set_object (&self->last_delta, delta);

  if (self->fail_stage)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Memory sync stage failed");

  test_memory_sync_history_append (self->history,
                                   repository,
                                   entity,
                                   delta,
                                   TRUE);

  return dex_future_new_true ();
}

static DexFuture *
test_sync_transport_push (GomSyncTransport   *transport,
                          GomSyncCoordinator *coordinator)
{
  TestSyncTransport *self = (TestSyncTransport *) transport;

  g_assert_true (GOM_IS_SYNC_TRANSPORT (transport));
  g_assert_true (GOM_IS_SYNC_COORDINATOR (coordinator));

  if (self->fail_push)
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "Memory sync push failed");

  self->push_count++;

  for (guint i = 0; i < self->history->changes->len; i++)
    {
      TestMemorySyncChange *change = g_ptr_array_index (self->history->changes, i);

      if (change->outbound && !change->sent && !change->acked)
        {
          change->sent = TRUE;
          g_queue_push_tail (&self->history->sent, change);
        }
    }

  return dex_future_new_true ();
}

typedef struct
{
  TestSyncTransport  *transport;
  GomSyncCoordinator *coordinator;
} TestSyncTransportPull;

static void
test_sync_transport_pull_free (gpointer data)
{
  TestSyncTransportPull *state = data;

  g_clear_object (&state->transport);
  g_clear_object (&state->coordinator);
  g_free (state);
}

static DexFuture *
test_sync_transport_pull_fiber (gpointer user_data)
{
  TestSyncTransportPull *state = user_data;
  TestSyncTransport *self = state->transport;

  g_assert (GOM_IS_SYNC_TRANSPORT (self));
  g_assert (GOM_IS_SYNC_COORDINATOR (state->coordinator));

  self->pull_count++;

  while (!g_queue_is_empty (&self->inbound))
    {
      TestMemorySyncChange *change;
      TestMemorySyncChange *local;
      g_autoptr(GError) error = NULL;
      g_autoptr(GomDelta) merged = NULL;
      GomDelta *local_delta;

      change = g_queue_peek_head (&self->inbound);
      if (change->remote_sequence <= self->last_applied_remote_sequence)
        {
          g_queue_pop_head (&self->inbound);
          continue;
        }

      if (change->remote_sequence != self->last_applied_remote_sequence + 1)
        return dex_future_new_reject (G_IO_ERROR,
                                      G_IO_ERROR_FAILED,
                                      "Remote sync sequence is out of order");

      local = test_memory_sync_history_find_pending_local (self->history, change);
      local_delta = local ? local->delta : NULL;

      merged = dex_await_object (gom_sync_coordinator_merge_remote_change (state->coordinator,
                                                                           change->repository,
                                                                           local_delta,
                                                                           change->delta),
                                 &error);
      if (merged == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      if (self->apply_remote &&
          !dex_await (_gom_sync_coordinator_apply_remote_change (state->coordinator,
                                                                 change->repository,
                                                                 change->relation,
                                                                 change->identity_string,
                                                                 change->remote_sequence,
                                                                 merged),
                      &error))
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_set_object (&self->last_merged_delta, merged);
      self->last_applied_remote_sequence = change->remote_sequence;
      self->applied_remote_count++;
      g_queue_pop_head (&self->inbound);
    }

  return dex_future_new_true ();
}

static DexFuture *
test_sync_transport_pull (GomSyncTransport   *transport,
                          GomSyncCoordinator *coordinator)
{
  TestSyncTransportPull *state;

  g_assert_true (GOM_IS_SYNC_TRANSPORT (transport));
  g_assert_true (GOM_IS_SYNC_COORDINATOR (coordinator));

  state = g_new0 (TestSyncTransportPull, 1);
  state->transport = g_object_ref ((TestSyncTransport *) transport);
  state->coordinator = g_object_ref (coordinator);

  return dex_scheduler_spawn (NULL,
                              0,
                              test_sync_transport_pull_fiber,
                              state,
                              test_sync_transport_pull_free);
}

static void
test_sync_transport_finalize (GObject *object)
{
  TestSyncTransport *self = (TestSyncTransport *) object;

  g_queue_clear (&self->inbound);
  g_clear_pointer (&self->history, test_memory_sync_history_free);
  g_clear_object (&self->last_repository);
  g_clear_object (&self->last_session);
  g_clear_object (&self->last_entity);
  g_clear_object (&self->last_delta);
  g_clear_object (&self->last_merged_delta);

  G_OBJECT_CLASS (test_sync_transport_parent_class)->finalize (object);
}

static void
test_sync_transport_class_init (TestSyncTransportClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomSyncTransportClass *transport_class = GOM_SYNC_TRANSPORT_CLASS (klass);

  object_class->finalize = test_sync_transport_finalize;
  transport_class->stage_local_change = test_sync_transport_stage_local_change;
  transport_class->push = test_sync_transport_push;
  transport_class->pull = test_sync_transport_pull;
}

static void
test_sync_transport_init (TestSyncTransport *self)
{
  self->history = test_memory_sync_history_new ();
  self->next_remote_sequence = 1;
}

G_DEFINE_TYPE (TestMergePolicy, test_merge_policy, GOM_TYPE_MERGE_POLICY)

static DexFuture *
test_merge_policy_merge (GomMergePolicy   *policy,
                         GomMergeDecision *decision)
{
  TestMergePolicy *self = (TestMergePolicy *) policy;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autoptr(GomDelta) merged_delta = NULL;

  g_assert_true (GOM_IS_MERGE_POLICY (policy));
  g_assert_true (GOM_IS_MERGE_DECISION (decision));

  self->merge_count++;
  local_delta = gom_merge_decision_dup_local_delta (decision);
  remote_delta = gom_merge_decision_dup_remote_delta (decision);
  g_set_object (&self->last_local_delta, local_delta);
  g_set_object (&self->last_remote_delta, remote_delta);

  if (self->skip_resolution)
    return dex_future_new_true ();

  if (self->reject)
    {
      gom_merge_decision_reject (decision,
                                 g_error_new_literal (G_IO_ERROR,
                                                      G_IO_ERROR_FAILED,
                                                      "Policy rejected merge"));
      return dex_future_new_true ();
    }

  if (self->reject_same_field &&
      local_delta != NULL &&
      remote_delta != NULL &&
      test_delta_has_overlapping_property (local_delta, remote_delta))
    {
      gom_merge_decision_reject (decision,
                                 g_error_new_literal (G_IO_ERROR,
                                                      G_IO_ERROR_FAILED,
                                                      "Policy rejected same-field conflict"));
      return dex_future_new_true ();
    }

  if (self->apply_delta != NULL)
    {
      gom_merge_decision_apply (decision, self->apply_delta);
      return dex_future_new_true ();
    }

  merged_delta = test_delta_merge_disjoint (local_delta, remote_delta);
  gom_merge_decision_apply (decision, merged_delta);

  return dex_future_new_true ();
}

static void
test_merge_policy_finalize (GObject *object)
{
  TestMergePolicy *self = (TestMergePolicy *) object;

  g_clear_object (&self->apply_delta);
  g_clear_object (&self->last_local_delta);
  g_clear_object (&self->last_remote_delta);

  G_OBJECT_CLASS (test_merge_policy_parent_class)->finalize (object);
}

static void
test_merge_policy_class_init (TestMergePolicyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomMergePolicyClass *policy_class = GOM_MERGE_POLICY_CLASS (klass);

  object_class->finalize = test_merge_policy_finalize;
  policy_class->merge = test_merge_policy_merge;
}

static void
test_merge_policy_init (TestMergePolicy *self)
{
}

static GomDelta *
test_delta_new_for_property (GType       entity_type,
                             const char *property_name,
                             int         value_)
{
  g_autoptr(GomDelta) delta = NULL;
  GValue value = G_VALUE_INIT;

  g_assert (property_name != NULL);

  delta = gom_delta_new (entity_type, GOM_DELTA_KIND_UPDATE);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, value_);
  gom_delta_add_property (delta, property_name, NULL, &value);
  g_value_unset (&value);

  return g_steal_pointer (&delta);
}

static GomDelta *
test_delta_new (GType entity_type)
{
  return test_delta_new_for_property (entity_type, "value", 1);
}

static void
test_delta_add_int_property (GomDelta   *delta,
                             const char *property_name,
                             int         value_)
{
  GValue value = G_VALUE_INIT;

  g_assert (GOM_IS_DELTA (delta));
  g_assert (property_name != NULL);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, value_);
  gom_delta_add_property (delta, property_name, NULL, &value);
  g_value_unset (&value);
}

#ifdef GOM_DATABASE_SQLITE
static GomRegistry *
test_sync_registry_new (void)
{
  g_autoptr(GomRegistryBuilder) builder = NULL;

  builder = gom_registry_builder_new ();
  gom_registry_builder_add_entity_type (builder, TEST_TYPE_SYNC_ENTITY);

  return gom_registry_builder_build (builder);
}

static GomRepository *
test_sync_sqlite_create_repository (TestSqliteContext  *context,
                                    TestSyncTransport  *transport,
                                    TestMergePolicy    *merge_policy,
                                    GError            **error)
{
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;

  g_assert (context != NULL);
  g_assert (GOM_IS_SYNC_TRANSPORT (transport));
  g_assert (GOM_IS_MERGE_POLICY (merge_policy));

  registry = test_sync_registry_new ();
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));

  return dex_await_object (gom_repository_new_with_coordinator (GOM_DRIVER (context->driver),
                                                                registry,
                                                                NULL,
                                                                coordinator),
                           error);
}

static gboolean
test_sync_sqlite_insert_entity (GomRepository  *repository,
                                int             id,
                                int             value_,
                                int             remote_value,
                                GError        **error)
{
  g_autoptr(GomInsertionBuilder) builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GPtrArray) row = NULL;
  GValue id_value = G_VALUE_INIT;
  GValue value = G_VALUE_INIT;
  GValue remote = G_VALUE_INIT;

  g_assert (GOM_IS_REPOSITORY (repository));

  g_value_init (&id_value, G_TYPE_INT);
  g_value_set_int (&id_value, id);
  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, value_);
  g_value_init (&remote, G_TYPE_INT);
  g_value_set_int (&remote, remote_value);

  builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_entity_type (builder, TEST_TYPE_SYNC_ENTITY);
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("id"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("value"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("remote-value"));

  row = g_ptr_array_new ();
  g_ptr_array_add (row, gom_literal_expression_new (&id_value));
  g_ptr_array_add (row, gom_literal_expression_new (&value));
  g_ptr_array_add (row, gom_literal_expression_new (&remote));
  gom_insertion_builder_add_row (builder, (GomExpression **) row->pdata, row->len);

  g_value_unset (&id_value);
  g_value_unset (&value);
  g_value_unset (&remote);

  insertion = gom_insertion_builder_build (builder, error);
  if (insertion == NULL)
    return FALSE;

  result = dex_await_object (gom_repository_mutate (repository, GOM_MUTATION (insertion)), error);
  if (result == NULL)
    return FALSE;

  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  return TRUE;
}

static GomEntity *
test_sync_sqlite_find_entity (GomRepository  *repository,
                              int             id,
                              GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomExpression) field = NULL;
  g_autoptr(GomExpression) literal = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  GomEntity *entity = NULL;
  GValue value = G_VALUE_INIT;
  gboolean has_row;

  g_assert (GOM_IS_REPOSITORY (repository));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, id);
  field = gom_field_expression_new ("id");
  literal = gom_literal_expression_new (&value);
  filter = gom_binary_expression_new_equal (field, literal);
  g_value_unset (&value);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, TEST_TYPE_SYNC_ENTITY);
  gom_query_builder_set_filter (builder, filter);
  gom_query_builder_set_limit (builder, 1);

  query = gom_query_builder_build (builder, error);
  if (query == NULL)
    return NULL;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return NULL;

  has_row = dex_await_boolean (gom_cursor_next (cursor), error);
  if (error != NULL && *error != NULL)
    return NULL;

  if (has_row)
    entity = gom_cursor_materialize (cursor, error);

  if (!dex_await (gom_cursor_close (cursor), error))
    g_clear_object (&entity);

  return entity;
}

static char *
test_sync_identity_for_id (int id)
{
  g_autoptr(GomEntity) entity = NULL;

  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", id,
                         NULL);

  return _gom_tombstone_serialize_entity_identity (entity, NULL);
}

static void
test_repository_no_sidecars_without_coordinator (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  sqlite3 *db = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-sidecar-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_sync_registry_new ();
  repository = dex_await_object (gom_repository_new (GOM_DRIVER (context.driver),
                                                     registry,
                                                     NULL),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);
  g_assert_null (gom_repository_dup_coordinator (repository));

  test_sqlite_open (context.db_path, &db);
  g_assert_false (test_sqlite_relation_exists (db, "gom_tombstones", "table"));
  g_assert_false (test_sqlite_relation_exists (db, "gom_sync_history", "table"));
  g_assert_false (test_sqlite_relation_exists (db, "gom_sync_history_values", "table"));
  test_sqlite_close (db);
}

static void
test_sync_history_sqlite_persistent_replay_ack (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) update_entity = NULL;
  g_autoptr(GomEntity) delete_entity = NULL;
  g_autoptr(GomDelta) update_delta = NULL;
  g_autoptr(GomDelta) delete_delta = NULL;
  g_autoptr(GListModel) replay = NULL;
  g_autoptr(GomSyncHistoryChange) first = NULL;
  g_autoptr(GomSyncHistoryChange) second = NULL;
  g_autoptr(GomDelta) replay_delta = NULL;
  g_autoptr(GomMutationResult) ack_result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  GValue value = G_VALUE_INIT;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-history-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_sync_registry_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = dex_await_object (gom_repository_new_with_coordinator (GOM_DRIVER (context.driver),
                                                                      registry,
                                                                      NULL,
                                                                      coordinator),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  update_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 8,
                                NULL);
  delete_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 9,
                                NULL);
  update_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 80);
  delete_delta = gom_delta_new (TEST_TYPE_SYNC_ENTITY, GOM_DELTA_KIND_DELETE);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     update_entity,
                                                                     update_delta),
                            &error));
  g_assert_no_error (error);
  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     delete_entity,
                                                                     delete_delta),
                            &error));
  g_assert_no_error (error);
  g_assert_cmpuint (transport->stage_count, ==, 2);

  g_clear_object (&repository);
  repository = dex_await_object (gom_repository_new (GOM_DRIVER (context.driver),
                                                     registry,
                                                     NULL),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  replay = dex_await_object (_gom_sync_history_replay (repository, 0), &error);
  g_assert_no_error (error);
  g_assert_nonnull (replay);
  g_assert_cmpuint (g_list_model_get_n_items (replay), ==, 2);

  first = g_list_model_get_item (replay, 0);
  second = g_list_model_get_item (replay, 1);
  g_assert_true (GOM_IS_SYNC_HISTORY_CHANGE (first));
  g_assert_true (GOM_IS_SYNC_HISTORY_CHANGE (second));

  g_assert_cmpuint (_gom_sync_history_change_get_sequence (first), ==, 1);
  g_assert_cmpuint (_gom_sync_history_change_get_batch (first), ==, 1);
  g_assert_true (_gom_sync_history_change_get_entity_type (first) == TEST_TYPE_SYNC_ENTITY);
  g_assert_cmpuint (_gom_sync_history_change_get_delta_kind (first), ==, GOM_DELTA_KIND_UPDATE);
  g_assert_cmpstr (_gom_sync_history_change_get_relation (first), ==, "test_sync_entities");
  g_assert_true (_gom_sync_history_change_get_outbound (first));
  g_assert_false (_gom_sync_history_change_get_acked (first));
  g_assert_false (_gom_sync_history_change_get_tombstone (first));

  replay_delta = _gom_sync_history_change_dup_delta (first);
  g_assert_nonnull (replay_delta);
  g_assert_cmpuint (gom_delta_get_n_changes (replay_delta), ==, 1);
  g_assert_cmpstr (gom_delta_get_property_name (replay_delta, 0), ==, "value");
  g_assert_false (gom_delta_get_original_value (replay_delta, 0, &value));
  g_assert_true (gom_delta_get_current_value (replay_delta, 0, &value));
  g_assert_cmpint (g_value_get_int (&value), ==, 80);
  g_value_unset (&value);

  g_assert_cmpuint (_gom_sync_history_change_get_sequence (second), ==, 2);
  g_assert_cmpuint (_gom_sync_history_change_get_delta_kind (second), ==, GOM_DELTA_KIND_DELETE);
  g_assert_true (_gom_sync_history_change_get_tombstone (second));

  ack_result = dex_await_object (_gom_sync_history_ack (repository, 2), &error);
  g_assert_no_error (error);
  g_assert_nonnull (ack_result);

  g_clear_object (&replay);
  g_clear_object (&first);
  g_clear_object (&second);

  replay = dex_await_object (_gom_sync_history_replay (repository, 0), &error);
  g_assert_no_error (error);
  first = g_list_model_get_item (replay, 0);
  second = g_list_model_get_item (replay, 1);
  g_assert_true (_gom_sync_history_change_get_acked (first));
  g_assert_true (_gom_sync_history_change_get_acked (second));

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_repository_mutations_stage_without_session (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-stage-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", 1,
                         "value", 10,
                         "remote-value", 0,
                         NULL);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (transport->stage_count, ==, 1);
  g_assert_true (transport->last_repository == repository);
  g_assert_null (transport->last_session);

  g_object_set (entity,
                "value", 11,
                NULL);

  result = dex_await_object (gom_entity_update (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (transport->stage_count, ==, 2);
  g_assert_true (transport->last_repository == repository);
  g_assert_null (transport->last_session);

  result = dex_await_object (gom_entity_delete (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (transport->stage_count, ==, 3);
  g_assert_true (transport->last_repository == repository);
  g_assert_null (transport->last_session);

  g_clear_object (&entity);
  g_clear_object (&repository);
  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_repository_mutations_without_coordinator_are_unchanged (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-disabled-XXXXXX", &error));
  g_assert_no_error (error);

  registry = test_sync_registry_new ();
  repository = dex_await_object (gom_repository_new (GOM_DRIVER (context.driver),
                                                     registry,
                                                     NULL),
                                 &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);
  g_assert_null (gom_repository_dup_coordinator (repository));

  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", 2,
                         "value", 20,
                         "remote-value", 0,
                         NULL);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  g_object_set (entity,
                "value", 21,
                NULL);

  result = dex_await_object (gom_entity_update (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  result = dex_await_object (gom_entity_delete (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);

  g_clear_object (&entity);
  g_clear_object (&repository);
}

static void
test_repository_staging_failure_is_reported (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-stage-failure-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->fail_stage = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", 3,
                         "value", 30,
                         "remote-value", 0,
                         NULL);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_null (result);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Memory sync stage failed");
  g_assert_cmpuint (transport->stage_count, ==, 1);
  g_assert_true (transport->last_repository == repository);
  g_assert_null (transport->last_session);
  g_clear_error (&error);

  g_clear_object (&entity);
  g_clear_object (&repository);
  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}
#endif

static void
test_sync_coordinator_accessors (void)
{
  g_autoptr(GomSyncTransport) transport = NULL;
  g_autoptr(GomMergePolicy) merge_policy = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomSyncTransport) dup_transport = NULL;
  g_autoptr(GomMergePolicy) dup_merge_policy = NULL;
  g_autoptr(GomSyncTransport) property_transport = NULL;
  g_autoptr(GomMergePolicy) property_merge_policy = NULL;

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (transport, merge_policy);

  g_assert_true (gom_sync_coordinator_get_transport (coordinator) == transport);
  g_assert_true (gom_sync_coordinator_get_merge_policy (coordinator) == merge_policy);

  dup_transport = gom_sync_coordinator_dup_transport (coordinator);
  dup_merge_policy = gom_sync_coordinator_dup_merge_policy (coordinator);

  g_assert_true (dup_transport == transport);
  g_assert_true (dup_merge_policy == merge_policy);

  g_object_get (coordinator,
                "transport", &property_transport,
                "merge-policy", &property_merge_policy,
                NULL);

  g_assert_true (property_transport == transport);
  g_assert_true (property_merge_policy == merge_policy);
}

static void
test_repository_and_session_coordinator_accessors (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomSyncTransport) transport = NULL;
  g_autoptr(GomMergePolicy) merge_policy = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRepository) repository_without_coordinator = NULL;
  g_autoptr(GomSyncCoordinator) repository_coordinator = NULL;
  g_autoptr(GomSyncCoordinator) property_coordinator = NULL;
  g_autoptr(GomSyncCoordinator) missing_coordinator = NULL;
  g_autoptr(GomSyncCoordinator) session_coordinator = NULL;
  g_autoptr(GomSyncCoordinator) unbound_session_coordinator = NULL;
  TestSyncSession *session = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (transport, merge_policy);
  repository = dex_await_object (gom_repository_new_with_coordinator (GOM_DRIVER (driver),
                                                                      NULL,
                                                                      NULL,
                                                                      coordinator),
                                 &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  repository_without_coordinator = dex_await_object (gom_repository_new (GOM_DRIVER (driver),
                                                                         NULL,
                                                                         NULL),
                                                     &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository_without_coordinator));

  session = g_object_new (TEST_TYPE_SYNC_SESSION, NULL);

  repository_coordinator = gom_repository_dup_coordinator (repository);
  g_object_get (repository,
                "coordinator", &property_coordinator,
                NULL);
  missing_coordinator = gom_repository_dup_coordinator (repository_without_coordinator);

  g_assert_true (repository_coordinator == coordinator);
  g_assert_true (property_coordinator == coordinator);
  g_assert_null (missing_coordinator);

  unbound_session_coordinator = gom_session_dup_coordinator (GOM_SESSION (session));
  g_assert_null (unbound_session_coordinator);

  _gom_session_set_repository (GOM_SESSION (session), repository);
  session_coordinator = gom_session_dup_coordinator (GOM_SESSION (session));
  g_assert_true (session_coordinator == coordinator);

  g_clear_object (&session);
}

static void
test_sync_transport_default_behavior (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomSyncTransport) transport = NULL;
  g_autoptr(GomMergePolicy) merge_policy = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (GOM_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (transport, merge_policy);
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             NULL);
  entity = g_object_new (TEST_TYPE_SYNC_ENTITY, NULL);
  delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     entity,
                                                                     delta),
                            &error));
  g_assert_no_error (error);

  g_assert_false (dex_await (gom_sync_coordinator_push (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_assert_cmpstr (error->message, ==, "Sync transport does not support push");
  g_clear_error (&error);

  g_assert_false (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_assert_cmpstr (error->message, ==, "Sync transport does not support pull");
  g_clear_error (&error);

  g_assert_false (dex_await (gom_sync_coordinator_sync (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_assert_cmpstr (error->message, ==, "Sync transport does not support pull");
}

static void
test_session_commit_stages_sync_changes (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  TestSyncSession *session = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  session = g_object_new (TEST_TYPE_SYNC_SESSION, NULL);
  entity = g_object_new (TEST_TYPE_SYNC_ENTITY, NULL);
  delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);

  _gom_session_set_repository (GOM_SESSION (session), repository);
  _gom_session_record_entity_changes (GOM_SESSION (session), entity, delta);

  g_assert_true (dex_await (gom_session_commit (GOM_SESSION (session)), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (session->commit_count, ==, 1);
  g_assert_cmpuint (transport->stage_count, ==, 1);
  g_assert_true (transport->last_repository == repository);
  g_assert_true (transport->last_session == GOM_SESSION (session));
  g_assert_true (transport->last_entity == entity);
  g_assert_true (transport->last_delta == delta);

  g_clear_object (&session);
  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_session_rollback_drops_pending_staged_changes (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;
  TestSyncSession *session = NULL;

  session = g_object_new (TEST_TYPE_SYNC_SESSION, NULL);
  entity = g_object_new (TEST_TYPE_SYNC_ENTITY, NULL);
  delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);

  _gom_session_record_entity_changes (GOM_SESSION (session), entity, delta);
  g_assert_cmpuint (session->parent_instance.sync_changes->len, ==, 1);

  g_assert_true (dex_await (gom_session_rollback (GOM_SESSION (session)), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (session->rollback_count, ==, 1);
  g_assert_cmpuint (session->parent_instance.sync_changes->len, ==, 0);

  g_clear_object (&session);
}

static void
test_memory_sync_history_stage_replay_ack (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;
  g_autoptr(GPtrArray) replay = NULL;
  GValue value = G_VALUE_INIT;
  TestMemorySyncChange *change;
  TestMemorySyncValue *identity;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", 42,
                         NULL);
  delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     entity,
                                                                     delta),
                            &error));
  g_assert_no_error (error);

  g_assert_cmpuint (transport->history->changes->len, ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_unsent_outbound (transport->history), ==, 1);
  g_assert_cmpuint (g_queue_get_length (&transport->history->sent), ==, 0);

  replay = test_memory_sync_history_replay (transport->history, 0);
  g_assert_cmpuint (replay->len, ==, 1);

  change = g_ptr_array_index (replay, 0);
  g_assert_cmpuint (change->sequence, ==, 1);
  g_assert_cmpuint (change->batch, ==, 1);
  g_assert_true (change->entity_type == TEST_TYPE_SYNC_ENTITY);
  g_assert_cmpstr (change->relation, ==, "test_sync_entities");
  g_assert_cmpuint (change->kind, ==, GOM_DELTA_KIND_UPDATE);
  g_assert_false (change->tombstone);
  g_assert_true (change->outbound);
  g_assert_false (change->acked);
  g_assert_true (change->delta != delta);

  g_assert_cmpuint (change->identity->len, ==, 1);
  identity = g_ptr_array_index (change->identity, 0);
  g_assert_cmpstr (identity->name, ==, "id");
  g_assert_cmpint (g_value_get_int (&identity->value), ==, 42);

  g_assert_true (gom_delta_get_current_value (change->delta, 0, &value));
  g_assert_cmpint (g_value_get_int (&value), ==, 1);
  g_value_unset (&value);

  g_clear_pointer (&replay, g_ptr_array_unref);
  replay = test_memory_sync_history_replay (transport->history, 1);
  g_assert_cmpuint (replay->len, ==, 0);

  g_assert_true (dex_await (gom_sync_coordinator_push (coordinator), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (transport->push_count, ==, 1);
  g_assert_cmpuint (g_queue_get_length (&transport->history->sent), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_unsent_outbound (transport->history), ==, 0);

  test_memory_sync_history_ack (transport->history, 1);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 0);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_failed_push_keeps_pending (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", 7,
                         NULL);
  delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     entity,
                                                                     delta),
                            &error));
  g_assert_no_error (error);

  transport->fail_push = TRUE;

  g_assert_false (dex_await (gom_sync_coordinator_push (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Memory sync push failed");
  g_clear_error (&error);

  g_assert_cmpuint (transport->push_count, ==, 0);
  g_assert_cmpuint (g_queue_get_length (&transport->history->sent), ==, 0);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_unsent_outbound (transport->history), ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_sync_pulls_then_pushes (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) local_entity = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  local_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                               "id", 1,
                               NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 2,
                                NULL);
  local_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  remote_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     local_entity,
                                                                     local_delta),
                            &error));
  g_assert_no_error (error);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  g_assert_true (dex_await (gom_sync_coordinator_sync (coordinator), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (transport->pull_count, ==, 1);
  g_assert_cmpuint (transport->push_count, ==, 1);
  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_true (g_queue_is_empty (&transport->inbound));
  g_assert_cmpuint (g_queue_get_length (&transport->history->sent), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_unsent_outbound (transport->history), ==, 0);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_remote_only_applies (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 11,
                                NULL);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "remote-value", 2);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (transport->pull_count, ==, 1);
  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_null (merge_policy->last_local_delta);
  g_assert_true (test_delta_has_property (merge_policy->last_remote_delta, "remote-value"));
  g_assert_true (transport->last_merged_delta != remote_delta);
  g_assert_true (test_delta_has_property (transport->last_merged_delta, "remote-value"));

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_local_only_remains_pending (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) local_entity = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  local_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                               "id", 12,
                               NULL);
  local_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "local-value", 3);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     local_entity,
                                                                     local_delta),
                            &error));
  g_assert_no_error (error);

  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (transport->pull_count, ==, 1);
  g_assert_cmpuint (merge_policy->merge_count, ==, 0);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_unsent_outbound (transport->history), ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_merges_disjoint_local_remote (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) local_entity = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  merge_policy->reject_same_field = TRUE;
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  local_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                               "id", 13,
                               NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 13,
                                NULL);
  local_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "local-value", 4);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "remote-value", 5);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     local_entity,
                                                                     local_delta),
                            &error));
  g_assert_no_error (error);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_nonnull (merge_policy->last_local_delta);
  g_assert_true (test_delta_has_property (merge_policy->last_remote_delta, "remote-value"));
  g_assert_nonnull (transport->last_merged_delta);
  g_assert_true (test_delta_has_property (transport->last_merged_delta, "local-value"));
  g_assert_true (test_delta_has_property (transport->last_merged_delta, "remote-value"));

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_rejects_same_field_conflict (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) local_entity = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  merge_policy->reject_same_field = TRUE;
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  local_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                               "id", 14,
                               NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 14,
                                NULL);
  local_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 6);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 7);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     local_entity,
                                                                     local_delta),
                            &error));
  g_assert_no_error (error);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  g_assert_false (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Policy rejected same-field conflict");
  g_clear_error (&error);

  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_nonnull (merge_policy->last_local_delta);
  g_assert_true (test_delta_has_property (merge_policy->last_remote_delta, "value"));
  g_assert_null (transport->last_merged_delta);
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_mismatched_identity_is_remote_only (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) local_entity = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  merge_policy->reject_same_field = TRUE;
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  local_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                               "id", 15,
                               NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 16,
                                NULL);
  local_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 8);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 9);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     local_entity,
                                                                     local_delta),
                            &error));
  g_assert_no_error (error);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_null (merge_policy->last_local_delta);
  g_assert_true (test_delta_has_property (merge_policy->last_remote_delta, "value"));
  g_assert_cmpuint (test_memory_sync_history_count_pending_outbound (transport->history), ==, 1);
  g_assert_cmpuint (test_memory_sync_history_count_unsent_outbound (transport->history), ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_failed_pull_leaves_inbound_replayable (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) local_entity = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  merge_policy->reject_same_field = TRUE;
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  local_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                               "id", 17,
                               NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 17,
                                NULL);
  local_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 10);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 11);

  g_assert_true (dex_await (gom_sync_coordinator_stage_local_change (coordinator,
                                                                     repository,
                                                                     NULL,
                                                                     local_entity,
                                                                     local_delta),
                            &error));
  g_assert_no_error (error);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  g_assert_false (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Policy rejected same-field conflict");
  g_clear_error (&error);

  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_cmpuint (g_queue_get_length (&transport->inbound), ==, 1);
  g_assert_cmpuint (transport->last_applied_remote_sequence, ==, 0);
  g_assert_cmpuint (transport->applied_remote_count, ==, 0);

  merge_policy->reject_same_field = FALSE;

  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_cmpuint (merge_policy->merge_count, ==, 2);
  g_assert_true (g_queue_is_empty (&transport->inbound));
  g_assert_cmpuint (transport->last_applied_remote_sequence, ==, 1);
  g_assert_cmpuint (transport->applied_remote_count, ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_duplicate_remote_sequence_is_idempotent (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 18,
                                NULL);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "remote-value", 12);

  test_sync_transport_queue_inbound_delta_with_sequence (transport,
                                                        repository,
                                                        remote_entity,
                                                        remote_delta,
                                                        1);
  test_sync_transport_queue_inbound_delta_with_sequence (transport,
                                                        repository,
                                                        remote_entity,
                                                        remote_delta,
                                                        1);

  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_true (g_queue_is_empty (&transport->inbound));
  g_assert_cmpuint (merge_policy->merge_count, ==, 1);
  g_assert_cmpuint (transport->last_applied_remote_sequence, ==, 1);
  g_assert_cmpuint (transport->applied_remote_count, ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_rejects_out_of_order_remote_sequence (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             "coordinator", coordinator,
                             NULL);
  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 19,
                                NULL);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "remote-value", 13);

  test_sync_transport_queue_inbound_delta_with_sequence (transport,
                                                        repository,
                                                        remote_entity,
                                                        remote_delta,
                                                        2);

  g_assert_false (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote sync sequence is out of order");
  g_clear_error (&error);

  g_assert_cmpuint (g_queue_get_length (&transport->inbound), ==, 1);
  g_assert_cmpuint (merge_policy->merge_count, ==, 0);
  g_assert_cmpuint (transport->last_applied_remote_sequence, ==, 0);
  g_assert_cmpuint (transport->applied_remote_count, ==, 0);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

#ifdef GOM_DATABASE_SQLITE
static void
test_memory_sync_transport_remote_update_applies_to_sqlite (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomEntity) stored = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  int value = 0;
  int remote_value = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-update-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->apply_remote = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  g_assert_true (test_sync_sqlite_insert_entity (repository, 1, 10, 0, &error));
  g_assert_no_error (error);

  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 1,
                                NULL);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "remote-value", 9);

  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  coordinator = gom_repository_dup_coordinator (repository);
  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (transport->applied_remote_count, ==, 1);

  stored = test_sync_sqlite_find_entity (repository, 1, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stored);
  g_object_get (stored,
                "value", &value,
                "remote-value", &remote_value,
                NULL);
  g_assert_cmpint (value, ==, 10);
  g_assert_cmpint (remote_value, ==, 9);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_local_delete_records_tombstone (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomEntity) stored = NULL;
  g_autofree char *identity = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  guint64 tombstone_sequence = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-local-delete-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->apply_remote = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                         "id", 4,
                         "value", 40,
                         "remote-value", 0,
                         NULL);
  gom_entity_set_repository (entity, repository);

  result = dex_await_object (gom_entity_insert (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (transport->stage_count, ==, 1);
  g_assert_true (transport->last_delta != NULL);
  g_assert_cmpuint (gom_delta_get_kind (transport->last_delta), ==, GOM_DELTA_KIND_INSERT);

  identity = test_sync_identity_for_id (4);
  g_assert_nonnull (identity);

  result = dex_await_object (gom_entity_delete (entity), &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpuint (gom_mutation_result_get_affected_rows (result), ==, 1);
  g_assert_cmpuint (transport->stage_count, ==, 2);
  g_assert_true (transport->last_repository == repository);
  g_assert_null (transport->last_session);
  g_assert_true (transport->last_entity == entity);
  g_assert_true (transport->last_delta != NULL);
  g_assert_cmpuint (gom_delta_get_kind (transport->last_delta), ==, GOM_DELTA_KIND_DELETE);

  stored = test_sync_sqlite_find_entity (repository, 4, &error);
  g_assert_no_error (error);
  g_assert_null (stored);

  g_assert_true (_gom_tombstone_lookup_sequence (repository,
                                                 TEST_TYPE_SYNC_ENTITY,
                                                 identity,
                                                 &tombstone_sequence,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpuint (tombstone_sequence, ==, 2);

  g_clear_object (&stored);
  g_clear_object (&entity);
  g_clear_object (&repository);
  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_remote_delete_records_tombstone (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomEntity) stored = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autofree char *identity = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  guint64 tombstone_sequence = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-delete-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->apply_remote = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  g_assert_true (test_sync_sqlite_insert_entity (repository, 2, 20, 0, &error));
  g_assert_no_error (error);

  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 2,
                                NULL);
  identity = _gom_tombstone_serialize_entity_identity (remote_entity, &error);
  g_assert_no_error (error);
  g_assert_nonnull (identity);

  remote_delta = gom_delta_new (TEST_TYPE_SYNC_ENTITY, GOM_DELTA_KIND_DELETE);
  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  coordinator = gom_repository_dup_coordinator (repository);
  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (transport->applied_remote_count, ==, 1);

  stored = test_sync_sqlite_find_entity (repository, 2, &error);
  g_assert_no_error (error);
  g_assert_null (stored);

  g_assert_true (_gom_tombstone_lookup_sequence (repository,
                                                 TEST_TYPE_SYNC_ENTITY,
                                                 identity,
                                                 &tombstone_sequence,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpuint (tombstone_sequence, ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_remote_delete_missing_records_tombstone (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autofree char *identity = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  guint64 tombstone_sequence = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-missing-delete-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->apply_remote = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 3,
                                NULL);
  identity = _gom_tombstone_serialize_entity_identity (remote_entity, &error);
  g_assert_no_error (error);
  g_assert_nonnull (identity);

  remote_delta = gom_delta_new (TEST_TYPE_SYNC_ENTITY, GOM_DELTA_KIND_DELETE);
  test_sync_transport_queue_inbound_delta (transport,
                                           repository,
                                           remote_entity,
                                           remote_delta);

  coordinator = gom_repository_dup_coordinator (repository);
  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);

  g_assert_true (_gom_tombstone_lookup_sequence (repository,
                                                 TEST_TYPE_SYNC_ENTITY,
                                                 identity,
                                                 &tombstone_sequence,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpuint (tombstone_sequence, ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_stale_update_after_tombstone_rejects (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) remote_entity = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autofree char *identity = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-stale-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->apply_remote = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  identity = test_sync_identity_for_id (5);
  g_assert_nonnull (identity);
  g_assert_true (dex_await (_gom_tombstone_record (repository,
                                                   TEST_TYPE_SYNC_ENTITY,
                                                   "test_sync_entities",
                                                   identity,
                                                   5),
                            &error));
  g_assert_no_error (error);

  remote_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 5,
                                NULL);
  remote_delta = test_delta_new_for_property (TEST_TYPE_SYNC_ENTITY, "value", 50);
  test_sync_transport_queue_inbound_delta_with_sequence (transport,
                                                        repository,
                                                        remote_entity,
                                                        remote_delta,
                                                        1);

  coordinator = gom_repository_dup_coordinator (repository);
  g_assert_false (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Remote update is older than a local tombstone");
  g_clear_error (&error);
  g_assert_cmpuint (g_queue_get_length (&transport->inbound), ==, 1);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_memory_sync_transport_reinsert_after_tombstone_is_ordered (void)
{
  g_autoptr(GError) error = NULL;
  g_auto(TestSqliteContext) context = {0};
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomEntity) delete_entity = NULL;
  g_autoptr(GomEntity) insert_entity = NULL;
  g_autoptr(GomEntity) stored = NULL;
  g_autoptr(GomDelta) delete_delta = NULL;
  g_autoptr(GomDelta) insert_delta = NULL;
  g_autofree char *identity = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;
  guint64 tombstone_sequence = 0;
  int value = 0;
  int remote_value = 0;

  g_assert_true (test_sqlite_context_init (&context, "gom-sync-reinsert-XXXXXX", &error));
  g_assert_no_error (error);

  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  transport->apply_remote = TRUE;
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  repository = test_sync_sqlite_create_repository (&context, transport, merge_policy, &error);
  g_assert_no_error (error);
  g_assert_nonnull (repository);

  delete_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 6,
                                NULL);
  insert_entity = g_object_new (TEST_TYPE_SYNC_ENTITY,
                                "id", 6,
                                NULL);
  identity = _gom_tombstone_serialize_entity_identity (insert_entity, &error);
  g_assert_no_error (error);
  g_assert_nonnull (identity);

  delete_delta = gom_delta_new (TEST_TYPE_SYNC_ENTITY, GOM_DELTA_KIND_DELETE);
  insert_delta = gom_delta_new (TEST_TYPE_SYNC_ENTITY, GOM_DELTA_KIND_INSERT);
  test_delta_add_int_property (insert_delta, "value", 60);
  test_delta_add_int_property (insert_delta, "remote-value", 61);

  test_sync_transport_queue_inbound_delta_with_sequence (transport,
                                                        repository,
                                                        delete_entity,
                                                        delete_delta,
                                                        1);
  test_sync_transport_queue_inbound_delta_with_sequence (transport,
                                                        repository,
                                                        insert_entity,
                                                        insert_delta,
                                                        2);

  coordinator = gom_repository_dup_coordinator (repository);
  g_assert_true (dex_await (gom_sync_coordinator_pull (coordinator), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (transport->applied_remote_count, ==, 2);

  stored = test_sync_sqlite_find_entity (repository, 6, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stored);
  g_object_get (stored,
                "value", &value,
                "remote-value", &remote_value,
                NULL);
  g_assert_cmpint (value, ==, 60);
  g_assert_cmpint (remote_value, ==, 61);

  g_assert_true (_gom_tombstone_lookup_sequence (repository,
                                                 TEST_TYPE_SYNC_ENTITY,
                                                 identity,
                                                 &tombstone_sequence,
                                                 &error));
  g_assert_no_error (error);
  g_assert_cmpuint (tombstone_sequence, ==, 0);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}
#endif

static void
test_merge_decision_rejects_mismatched_deltas (void)
{
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autoptr(GomMergeDecision) decision = NULL;

  driver = _gom_mock_driver_new ();
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             NULL);
  local_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  remote_delta = test_delta_new (TEST_TYPE_OTHER_SYNC_ENTITY);

  decision = gom_merge_decision_new (repository, local_delta, remote_delta);
  g_assert_null (decision);
}

static void
test_sync_coordinator_merge_applies_policy_delta (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autoptr(GomDelta) result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             NULL);
  local_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  remote_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  merge_policy->apply_delta = g_object_ref (remote_delta);

  result = dex_await_object (gom_sync_coordinator_merge_remote_change (coordinator,
                                                                       repository,
                                                                       local_delta,
                                                                       remote_delta),
                             &error);
  g_assert_no_error (error);
  g_assert_true (result == remote_delta);

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_sync_coordinator_merge_rejects_unresolved_decision (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autoptr(GomDelta) result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             NULL);
  local_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  remote_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  merge_policy->skip_resolution = TRUE;

  result = dex_await_object (gom_sync_coordinator_merge_remote_change (coordinator,
                                                                       repository,
                                                                       local_delta,
                                                                       remote_delta),
                             &error);
  g_assert_null (result);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==,
                   "Merge policy completed without resolving the merge decision");

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

static void
test_sync_coordinator_merge_propagates_policy_rejection (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomSyncCoordinator) coordinator = NULL;
  g_autoptr(GomDelta) local_delta = NULL;
  g_autoptr(GomDelta) remote_delta = NULL;
  g_autoptr(GomDelta) result = NULL;
  TestSyncTransport *transport = NULL;
  TestMergePolicy *merge_policy = NULL;

  driver = _gom_mock_driver_new ();
  transport = g_object_new (TEST_TYPE_SYNC_TRANSPORT, NULL);
  merge_policy = g_object_new (TEST_TYPE_MERGE_POLICY, NULL);
  coordinator = gom_sync_coordinator_new (GOM_SYNC_TRANSPORT (transport),
                                          GOM_MERGE_POLICY (merge_policy));
  repository = g_object_new (GOM_TYPE_REPOSITORY,
                             "driver", driver,
                             NULL);
  local_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  remote_delta = test_delta_new (TEST_TYPE_SYNC_ENTITY);
  merge_policy->reject = TRUE;

  result = dex_await_object (gom_sync_coordinator_merge_remote_change (coordinator,
                                                                       repository,
                                                                       local_delta,
                                                                       remote_delta),
                             &error);
  g_assert_null (result);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
  g_assert_cmpstr (error->message, ==, "Policy rejected merge");

  g_clear_object (&merge_policy);
  g_clear_object (&transport);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/sync-coordinator/accessors",
                    test_sync_coordinator_accessors);
  _g_test_add_func ("/Gom/repository/coordinator-accessors",
                    test_repository_and_session_coordinator_accessors);
  _g_test_add_func ("/Gom/sync-transport/default-behavior",
                    test_sync_transport_default_behavior);
  _g_test_add_func ("/Gom/session/commit-stages-sync-changes",
                    test_session_commit_stages_sync_changes);
  _g_test_add_func ("/Gom/session/rollback-drops-pending-staged-changes",
                    test_session_rollback_drops_pending_staged_changes);
  _g_test_add_func ("/Gom/memory-sync-history/stage-replay-ack",
                    test_memory_sync_history_stage_replay_ack);
  _g_test_add_func ("/Gom/memory-sync-transport/failed-push-keeps-pending",
                    test_memory_sync_transport_failed_push_keeps_pending);
  _g_test_add_func ("/Gom/memory-sync-transport/sync-pulls-then-pushes",
                    test_memory_sync_transport_sync_pulls_then_pushes);
  _g_test_add_func ("/Gom/memory-sync-transport/remote-only-applies",
                    test_memory_sync_transport_remote_only_applies);
  _g_test_add_func ("/Gom/memory-sync-transport/local-only-remains-pending",
                    test_memory_sync_transport_local_only_remains_pending);
  _g_test_add_func ("/Gom/memory-sync-transport/merges-disjoint-local-remote",
                    test_memory_sync_transport_merges_disjoint_local_remote);
  _g_test_add_func ("/Gom/memory-sync-transport/rejects-same-field-conflict",
                    test_memory_sync_transport_rejects_same_field_conflict);
  _g_test_add_func ("/Gom/memory-sync-transport/mismatched-identity-is-remote-only",
                    test_memory_sync_transport_mismatched_identity_is_remote_only);
  _g_test_add_func ("/Gom/memory-sync-transport/failed-pull-leaves-inbound-replayable",
                    test_memory_sync_transport_failed_pull_leaves_inbound_replayable);
  _g_test_add_func ("/Gom/memory-sync-transport/duplicate-remote-sequence-is-idempotent",
                    test_memory_sync_transport_duplicate_remote_sequence_is_idempotent);
  _g_test_add_func ("/Gom/memory-sync-transport/rejects-out-of-order-remote-sequence",
                    test_memory_sync_transport_rejects_out_of_order_remote_sequence);
#ifdef GOM_DATABASE_SQLITE
  _g_test_add_func ("/Gom/repository/sqlite-no-sidecars-without-coordinator",
                    test_repository_no_sidecars_without_coordinator);
  _g_test_add_func ("/Gom/repository/mutations-stage-without-session",
                    test_repository_mutations_stage_without_session);
  _g_test_add_func ("/Gom/repository/mutations-without-coordinator-unchanged",
                    test_repository_mutations_without_coordinator_are_unchanged);
  _g_test_add_func ("/Gom/repository/staging-failure-is-reported",
                    test_repository_staging_failure_is_reported);
  _g_test_add_func ("/Gom/sync-history/sqlite-persistent-replay-ack",
                    test_sync_history_sqlite_persistent_replay_ack);
  _g_test_add_func ("/Gom/memory-sync-transport/sqlite-remote-update-applies",
                    test_memory_sync_transport_remote_update_applies_to_sqlite);
  _g_test_add_func ("/Gom/memory-sync-transport/sqlite-local-delete-records-tombstone",
                    test_memory_sync_transport_local_delete_records_tombstone);
  _g_test_add_func ("/Gom/memory-sync-transport/sqlite-remote-delete-records-tombstone",
                    test_memory_sync_transport_remote_delete_records_tombstone);
  _g_test_add_func ("/Gom/memory-sync-transport/sqlite-remote-delete-missing-records-tombstone",
                    test_memory_sync_transport_remote_delete_missing_records_tombstone);
  _g_test_add_func ("/Gom/memory-sync-transport/sqlite-stale-update-after-tombstone-rejects",
                    test_memory_sync_transport_stale_update_after_tombstone_rejects);
  _g_test_add_func ("/Gom/memory-sync-transport/sqlite-reinsert-after-tombstone-is-ordered",
                    test_memory_sync_transport_reinsert_after_tombstone_is_ordered);
#endif
  _g_test_add_func ("/Gom/merge-decision/rejects-mismatched-deltas",
                    test_merge_decision_rejects_mismatched_deltas);
  _g_test_add_func ("/Gom/sync-coordinator/merge-applies-policy-delta",
                    test_sync_coordinator_merge_applies_policy_delta);
  _g_test_add_func ("/Gom/sync-coordinator/merge-rejects-unresolved-decision",
                    test_sync_coordinator_merge_rejects_unresolved_decision);
  _g_test_add_func ("/Gom/sync-coordinator/merge-propagates-policy-rejection",
                    test_sync_coordinator_merge_propagates_policy_rejection);
  return g_test_run ();
}
