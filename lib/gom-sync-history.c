/* gom-sync-history.c
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

#include <string.h>

#include "gom-cursor.h"
#include "gom-delta.h"
#include "gom-driver-private.h"
#include "gom-entity.h"
#include "gom-expression.h"
#include "gom-insertion.h"
#include "gom-insertion-builder.h"
#include "gom-mutation.h"
#include "gom-mutation-result.h"
#include "gom-ordering.h"
#include "gom-query.h"
#include "gom-query-builder.h"
#include "gom-record.h"
#include "gom-repository-private.h"
#include "gom-session-private.h"
#include "gom-sync-history-private.h"
#include "gom-tombstone-private.h"
#include "gom-update.h"
#include "gom-update-builder.h"

struct _GomSyncHistoryChange
{
  GomEntity parent_instance;

  char     *entity_type;
  char     *relation;
  char     *identity;
  char     *created_at;
  GomDelta *delta;
  guint64   sequence;
  guint64   batch;
  int       delta_kind;
  guint     tombstone : 1;
  guint     outbound : 1;
  guint     sent : 1;
  guint     acked : 1;
};

struct _GomSyncHistoryChangeClass
{
  GomEntityClass parent_class;
};

struct _GomSyncHistoryValue
{
  GomEntity parent_instance;

  char    *property_name;
  char    *original_type;
  char    *original_value;
  char    *current_type;
  char    *current_value;
  guint64  sequence;
  guint    has_original : 1;
  guint    original_is_null : 1;
  guint    has_current : 1;
  guint    current_is_null : 1;
};

struct _GomSyncHistoryValueClass
{
  GomEntityClass parent_class;
};

typedef struct
{
  GomRepository *repository;
} GomSyncHistoryEnsure;

typedef struct
{
  GomRepository *repository;
  GomSession    *session;
  GomDelta      *delta;
  char          *relation;
  char          *identity;
  guint          outbound : 1;
} GomSyncHistoryAppend;

typedef struct
{
  GomRepository *repository;
  guint64        sequence;
} GomSyncHistoryReplay;

enum
{
  CHANGE_PROP_0,
  CHANGE_PROP_SEQUENCE,
  CHANGE_PROP_BATCH,
  CHANGE_PROP_ENTITY_TYPE,
  CHANGE_PROP_RELATION,
  CHANGE_PROP_IDENTITY,
  CHANGE_PROP_DELTA_KIND,
  CHANGE_PROP_TOMBSTONE,
  CHANGE_PROP_OUTBOUND,
  CHANGE_PROP_SENT,
  CHANGE_PROP_ACKED,
  CHANGE_PROP_CREATED_AT,
  CHANGE_N_PROPS
};

enum
{
  VALUE_PROP_0,
  VALUE_PROP_SEQUENCE,
  VALUE_PROP_PROPERTY_NAME,
  VALUE_PROP_HAS_ORIGINAL,
  VALUE_PROP_ORIGINAL_TYPE,
  VALUE_PROP_ORIGINAL_VALUE,
  VALUE_PROP_ORIGINAL_IS_NULL,
  VALUE_PROP_HAS_CURRENT,
  VALUE_PROP_CURRENT_TYPE,
  VALUE_PROP_CURRENT_VALUE,
  VALUE_PROP_CURRENT_IS_NULL,
  VALUE_N_PROPS
};

G_DEFINE_FINAL_TYPE (GomSyncHistoryChange, gom_sync_history_change, GOM_TYPE_ENTITY)
G_DEFINE_FINAL_TYPE (GomSyncHistoryValue, gom_sync_history_value, GOM_TYPE_ENTITY)

static GParamSpec *change_properties[CHANGE_N_PROPS];
static GParamSpec *value_properties[VALUE_N_PROPS];

static void
gom_sync_history_ensure_free (gpointer data)
{
  GomSyncHistoryEnsure *state = data;

  g_clear_object (&state->repository);
  g_free (state);
}

static void
gom_sync_history_append_free (gpointer data)
{
  GomSyncHistoryAppend *state = data;

  g_clear_object (&state->repository);
  g_clear_object (&state->session);
  g_clear_object (&state->delta);
  g_clear_pointer (&state->relation, g_free);
  g_clear_pointer (&state->identity, g_free);
  g_free (state);
}

static void
gom_sync_history_replay_free (gpointer data)
{
  GomSyncHistoryReplay *state = data;

  g_clear_object (&state->repository);
  g_free (state);
}

static void
gom_sync_history_change_finalize (GObject *object)
{
  GomSyncHistoryChange *self = (GomSyncHistoryChange *)object;

  g_clear_pointer (&self->entity_type, g_free);
  g_clear_pointer (&self->relation, g_free);
  g_clear_pointer (&self->identity, g_free);
  g_clear_pointer (&self->created_at, g_free);
  g_clear_object (&self->delta);

  G_OBJECT_CLASS (gom_sync_history_change_parent_class)->finalize (object);
}

static void
gom_sync_history_change_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GomSyncHistoryChange *self = (GomSyncHistoryChange *)object;

  switch (prop_id)
    {
    case CHANGE_PROP_SEQUENCE:
      g_value_set_uint64 (value, self->sequence);
      break;

    case CHANGE_PROP_BATCH:
      g_value_set_uint64 (value, self->batch);
      break;

    case CHANGE_PROP_ENTITY_TYPE:
      g_value_set_string (value, self->entity_type);
      break;

    case CHANGE_PROP_RELATION:
      g_value_set_string (value, self->relation);
      break;

    case CHANGE_PROP_IDENTITY:
      g_value_set_string (value, self->identity);
      break;

    case CHANGE_PROP_DELTA_KIND:
      g_value_set_int (value, self->delta_kind);
      break;

    case CHANGE_PROP_TOMBSTONE:
      g_value_set_boolean (value, self->tombstone);
      break;

    case CHANGE_PROP_OUTBOUND:
      g_value_set_boolean (value, self->outbound);
      break;

    case CHANGE_PROP_SENT:
      g_value_set_boolean (value, self->sent);
      break;

    case CHANGE_PROP_ACKED:
      g_value_set_boolean (value, self->acked);
      break;

    case CHANGE_PROP_CREATED_AT:
      g_value_set_string (value, self->created_at);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sync_history_change_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GomSyncHistoryChange *self = (GomSyncHistoryChange *)object;

  switch (prop_id)
    {
    case CHANGE_PROP_SEQUENCE:
      self->sequence = g_value_get_uint64 (value);
      break;

    case CHANGE_PROP_BATCH:
      self->batch = g_value_get_uint64 (value);
      break;

    case CHANGE_PROP_ENTITY_TYPE:
      g_set_str (&self->entity_type, g_value_get_string (value));
      break;

    case CHANGE_PROP_RELATION:
      g_set_str (&self->relation, g_value_get_string (value));
      break;

    case CHANGE_PROP_IDENTITY:
      g_set_str (&self->identity, g_value_get_string (value));
      break;

    case CHANGE_PROP_DELTA_KIND:
      self->delta_kind = g_value_get_int (value);
      break;

    case CHANGE_PROP_TOMBSTONE:
      self->tombstone = g_value_get_boolean (value);
      break;

    case CHANGE_PROP_OUTBOUND:
      self->outbound = g_value_get_boolean (value);
      break;

    case CHANGE_PROP_SENT:
      self->sent = g_value_get_boolean (value);
      break;

    case CHANGE_PROP_ACKED:
      self->acked = g_value_get_boolean (value);
      break;

    case CHANGE_PROP_CREATED_AT:
      g_set_str (&self->created_at, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sync_history_change_class_init (GomSyncHistoryChangeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);
  const char *identity_fields[] = { "sequence", NULL };

  object_class->finalize = gom_sync_history_change_finalize;
  object_class->get_property = gom_sync_history_change_get_property;
  object_class->set_property = gom_sync_history_change_set_property;

  change_properties[CHANGE_PROP_SEQUENCE] =
    g_param_spec_uint64 ("sequence", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_BATCH] =
    g_param_spec_uint64 ("batch", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_ENTITY_TYPE] =
    g_param_spec_string ("entity-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_RELATION] =
    g_param_spec_string ("relation", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_IDENTITY] =
    g_param_spec_string ("identity", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_DELTA_KIND] =
    g_param_spec_int ("delta-kind", NULL, NULL,
                      0, G_MAXINT, 0,
                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_TOMBSTONE] =
    g_param_spec_boolean ("tombstone", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_OUTBOUND] =
    g_param_spec_boolean ("outbound", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_SENT] =
    g_param_spec_boolean ("sent", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_ACKED] =
    g_param_spec_boolean ("acked", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  change_properties[CHANGE_PROP_CREATED_AT] =
    g_param_spec_string ("created-at", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, CHANGE_N_PROPS, change_properties);

  gom_entity_class_set_relation (entity_class, "gom_sync_history");
  gom_entity_class_set_identity_fields (entity_class, identity_fields);
  gom_entity_class_set_version_added (entity_class, 1);

  for (guint i = 1; i < CHANGE_N_PROPS; i++)
    {
      const char *name = g_param_spec_get_name (change_properties[i]);

      gom_entity_class_property_set_mapped (entity_class, name, TRUE);
      gom_entity_class_property_set_nonnull (entity_class, name, TRUE);
      gom_entity_class_property_set_version_added (entity_class, name, 1);
    }

  gom_entity_class_property_set_field_name (entity_class, "entity-type", "entity_type");
  gom_entity_class_property_set_field_name (entity_class, "delta-kind", "delta_kind");
  gom_entity_class_property_set_field_name (entity_class, "created-at", "created_at");
}

static void
gom_sync_history_change_init (GomSyncHistoryChange *self)
{
}

static void
gom_sync_history_value_finalize (GObject *object)
{
  GomSyncHistoryValue *self = (GomSyncHistoryValue *)object;

  g_clear_pointer (&self->property_name, g_free);
  g_clear_pointer (&self->original_type, g_free);
  g_clear_pointer (&self->original_value, g_free);
  g_clear_pointer (&self->current_type, g_free);
  g_clear_pointer (&self->current_value, g_free);

  G_OBJECT_CLASS (gom_sync_history_value_parent_class)->finalize (object);
}

static void
gom_sync_history_value_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GomSyncHistoryValue *self = (GomSyncHistoryValue *)object;

  switch (prop_id)
    {
    case VALUE_PROP_SEQUENCE:
      g_value_set_uint64 (value, self->sequence);
      break;

    case VALUE_PROP_PROPERTY_NAME:
      g_value_set_string (value, self->property_name);
      break;

    case VALUE_PROP_HAS_ORIGINAL:
      g_value_set_boolean (value, self->has_original);
      break;

    case VALUE_PROP_ORIGINAL_TYPE:
      g_value_set_string (value, self->original_type);
      break;

    case VALUE_PROP_ORIGINAL_VALUE:
      g_value_set_string (value, self->original_value);
      break;

    case VALUE_PROP_ORIGINAL_IS_NULL:
      g_value_set_boolean (value, self->original_is_null);
      break;

    case VALUE_PROP_HAS_CURRENT:
      g_value_set_boolean (value, self->has_current);
      break;

    case VALUE_PROP_CURRENT_TYPE:
      g_value_set_string (value, self->current_type);
      break;

    case VALUE_PROP_CURRENT_VALUE:
      g_value_set_string (value, self->current_value);
      break;

    case VALUE_PROP_CURRENT_IS_NULL:
      g_value_set_boolean (value, self->current_is_null);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sync_history_value_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GomSyncHistoryValue *self = (GomSyncHistoryValue *)object;

  switch (prop_id)
    {
    case VALUE_PROP_SEQUENCE:
      self->sequence = g_value_get_uint64 (value);
      break;

    case VALUE_PROP_PROPERTY_NAME:
      g_set_str (&self->property_name, g_value_get_string (value));
      break;

    case VALUE_PROP_HAS_ORIGINAL:
      self->has_original = g_value_get_boolean (value);
      break;

    case VALUE_PROP_ORIGINAL_TYPE:
      g_set_str (&self->original_type, g_value_get_string (value));
      break;

    case VALUE_PROP_ORIGINAL_VALUE:
      g_set_str (&self->original_value, g_value_get_string (value));
      break;

    case VALUE_PROP_ORIGINAL_IS_NULL:
      self->original_is_null = g_value_get_boolean (value);
      break;

    case VALUE_PROP_HAS_CURRENT:
      self->has_current = g_value_get_boolean (value);
      break;

    case VALUE_PROP_CURRENT_TYPE:
      g_set_str (&self->current_type, g_value_get_string (value));
      break;

    case VALUE_PROP_CURRENT_VALUE:
      g_set_str (&self->current_value, g_value_get_string (value));
      break;

    case VALUE_PROP_CURRENT_IS_NULL:
      self->current_is_null = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_sync_history_value_class_init (GomSyncHistoryValueClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);
  const char *identity_fields[] = { "sequence", "property-name", NULL };

  object_class->finalize = gom_sync_history_value_finalize;
  object_class->get_property = gom_sync_history_value_get_property;
  object_class->set_property = gom_sync_history_value_set_property;

  value_properties[VALUE_PROP_SEQUENCE] =
    g_param_spec_uint64 ("sequence", NULL, NULL,
                         0, G_MAXUINT64, 0,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_PROPERTY_NAME] =
    g_param_spec_string ("property-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_HAS_ORIGINAL] =
    g_param_spec_boolean ("has-original", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_ORIGINAL_TYPE] =
    g_param_spec_string ("original-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_ORIGINAL_VALUE] =
    g_param_spec_string ("original-value", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_ORIGINAL_IS_NULL] =
    g_param_spec_boolean ("original-is-null", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_HAS_CURRENT] =
    g_param_spec_boolean ("has-current", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_CURRENT_TYPE] =
    g_param_spec_string ("current-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_CURRENT_VALUE] =
    g_param_spec_string ("current-value", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  value_properties[VALUE_PROP_CURRENT_IS_NULL] =
    g_param_spec_boolean ("current-is-null", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, VALUE_N_PROPS, value_properties);

  gom_entity_class_set_relation (entity_class, "gom_sync_history_values");
  gom_entity_class_set_identity_fields (entity_class, identity_fields);
  gom_entity_class_set_version_added (entity_class, 1);

  for (guint i = 1; i < VALUE_N_PROPS; i++)
    {
      const char *name = g_param_spec_get_name (value_properties[i]);

      gom_entity_class_property_set_mapped (entity_class, name, TRUE);
      gom_entity_class_property_set_version_added (entity_class, name, 1);
    }

  gom_entity_class_property_set_field_name (entity_class, "property-name", "property_name");
  gom_entity_class_property_set_field_name (entity_class, "has-original", "has_original");
  gom_entity_class_property_set_field_name (entity_class, "original-type", "original_type");
  gom_entity_class_property_set_field_name (entity_class, "original-value", "original_value");
  gom_entity_class_property_set_field_name (entity_class, "original-is-null", "original_is_null");
  gom_entity_class_property_set_field_name (entity_class, "has-current", "has_current");
  gom_entity_class_property_set_field_name (entity_class, "current-type", "current_type");
  gom_entity_class_property_set_field_name (entity_class, "current-value", "current_value");
  gom_entity_class_property_set_field_name (entity_class, "current-is-null", "current_is_null");

  gom_entity_class_property_set_nonnull (entity_class, "sequence", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "property-name", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "has-original", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "original-is-null", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "has-current", TRUE);
  gom_entity_class_property_set_nonnull (entity_class, "current-is-null", TRUE);
}

static void
gom_sync_history_value_init (GomSyncHistoryValue *self)
{
}

static DexFuture *
gom_sync_history_mutate (GomRepository *repository,
                         GomSession    *session,
                         GomMutation   *mutation)
{
  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (session == NULL || GOM_IS_SESSION (session));
  g_assert (GOM_IS_MUTATION (mutation));

  if (session != NULL)
    return _gom_session_mutate (session, mutation);

  return gom_repository_mutate (repository, mutation);
}

static DexFuture *
gom_sync_history_query (GomRepository *repository,
                        GomSession    *session,
                        GomQuery      *query)
{
  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (session == NULL || GOM_IS_SESSION (session));
  g_assert (GOM_IS_QUERY (query));

  if (session != NULL)
    return _gom_session_query (session, query);

  return gom_repository_query (repository, query);
}

static void
gom_sync_history_add_uint64_value (GPtrArray *row,
                                   guint64    value)
{
  GValue gvalue = G_VALUE_INIT;

  g_assert (row != NULL);

  g_value_init (&gvalue, G_TYPE_UINT64);
  g_value_set_uint64 (&gvalue, value);
  g_ptr_array_add (row, gom_literal_expression_new (&gvalue));
  g_value_unset (&gvalue);
}

static void
gom_sync_history_add_int_value (GPtrArray *row,
                                int        value)
{
  GValue gvalue = G_VALUE_INIT;

  g_assert (row != NULL);

  g_value_init (&gvalue, G_TYPE_INT);
  g_value_set_int (&gvalue, value);
  g_ptr_array_add (row, gom_literal_expression_new (&gvalue));
  g_value_unset (&gvalue);
}

static void
gom_sync_history_add_boolean_value (GPtrArray *row,
                                    gboolean   value)
{
  GValue gvalue = G_VALUE_INIT;

  g_assert (row != NULL);

  g_value_init (&gvalue, G_TYPE_BOOLEAN);
  g_value_set_boolean (&gvalue, value);
  g_ptr_array_add (row, gom_literal_expression_new (&gvalue));
  g_value_unset (&gvalue);
}

static void
gom_sync_history_add_string_value (GPtrArray  *row,
                                   const char *value)
{
  GValue gvalue = G_VALUE_INIT;

  g_assert (row != NULL);

  g_value_init (&gvalue, G_TYPE_STRING);
  g_value_set_string (&gvalue, value);
  g_ptr_array_add (row, gom_literal_expression_new (&gvalue));
  g_value_unset (&gvalue);
}

static gboolean
gom_sync_history_value_to_text (const GValue  *value,
                                char         **type_name,
                                char         **text,
                                gboolean      *is_null,
                                GError       **error)
{
  GType value_type;

  g_assert (value != NULL);
  g_assert (G_IS_VALUE (value));
  g_assert (type_name != NULL);
  g_assert (text != NULL);
  g_assert (is_null != NULL);

  value_type = G_VALUE_TYPE (value);
  *type_name = g_strdup (g_type_name (value_type));
  *text = NULL;
  *is_null = FALSE;

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      *text = g_strdup (g_value_get_boolean (value) ? "true" : "false");
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT (value))
    {
      *text = g_strdup_printf ("%d", g_value_get_int (value));
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT (value))
    {
      *text = g_strdup_printf ("%u", g_value_get_uint (value));
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT64 (value))
    {
      *text = g_strdup_printf ("%" G_GINT64_FORMAT, g_value_get_int64 (value));
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT64 (value))
    {
      *text = g_strdup_printf ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value));
      return TRUE;
    }

  if (G_VALUE_HOLDS_DOUBLE (value) || G_VALUE_HOLDS_FLOAT (value))
    {
      char buffer[G_ASCII_DTOSTR_BUF_SIZE];
      double number;

      number = G_VALUE_HOLDS_DOUBLE (value) ? g_value_get_double (value)
                                            : g_value_get_float (value);
      *text = g_strdup (g_ascii_dtostr (buffer, sizeof buffer, number));
      return TRUE;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      const char *str = g_value_get_string (value);

      if (str == NULL)
        *is_null = TRUE;
      else
        *text = g_strdup (str);

      return TRUE;
    }

  if (G_VALUE_HOLDS (value, G_TYPE_GTYPE))
    {
      GType gtype = g_value_get_gtype (value);

      *text = g_strdup (g_type_name (gtype));
      return TRUE;
    }

  if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
    {
      GDateTime *datetime = g_value_get_boxed (value);

      if (datetime == NULL)
        *is_null = TRUE;
      else if ((*text = g_date_time_format_iso8601 (datetime)) == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Failed to serialize GDateTime value");
          return FALSE;
        }

      return TRUE;
    }

  if (G_VALUE_HOLDS (value, G_TYPE_BYTES))
    {
      GBytes *bytes = g_value_get_boxed (value);

      if (bytes == NULL)
        *is_null = TRUE;
      else
        {
          gsize size = 0;
          const guint8 *data = g_bytes_get_data (bytes, &size);

          *text = g_base64_encode (data, size);
        }

      return TRUE;
    }

  if (g_type_is_a (value_type, G_TYPE_ENUM))
    {
      *text = g_strdup_printf ("%d", g_value_get_enum (value));
      return TRUE;
    }

  if (g_type_is_a (value_type, G_TYPE_FLAGS))
    {
      *text = g_strdup_printf ("%u", g_value_get_flags (value));
      return TRUE;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Cannot serialize sync history value of type %s",
               G_VALUE_TYPE_NAME (value));
  return FALSE;
}

static gboolean
gom_sync_history_parse_int64 (const char  *text,
                              gint64      *value,
                              GError     **error)
{
  char *endptr = NULL;

  g_assert (text != NULL);
  g_assert (value != NULL);

  *value = g_ascii_strtoll (text, &endptr, 10);
  if (endptr == text || *endptr != '\0')
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Invalid integer sync history value '%s'",
                   text);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gom_sync_history_parse_uint64 (const char  *text,
                               guint64     *value,
                               GError     **error)
{
  char *endptr = NULL;

  g_assert (text != NULL);
  g_assert (value != NULL);

  *value = g_ascii_strtoull (text, &endptr, 10);
  if (endptr == text || *endptr != '\0')
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Invalid unsigned sync history value '%s'",
                   text);
      return FALSE;
    }

  return TRUE;
}

static gboolean
gom_sync_history_text_to_value (const char  *type_name,
                                const char  *text,
                                gboolean     is_null,
                                GValue      *value,
                                GError     **error)
{
  GType value_type;

  g_assert (type_name != NULL);
  g_assert (value != NULL);

  if ((value_type = g_type_from_name (type_name)) == G_TYPE_INVALID)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unknown sync history value type '%s'",
                   type_name);
      return FALSE;
    }

  g_value_init (value, value_type);

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      g_value_set_boolean (value, g_strcmp0 (text, "true") == 0);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT (value))
    {
      gint64 number = 0;

      if (!gom_sync_history_parse_int64 (text, &number, error))
        return FALSE;

      g_value_set_int (value, (int)number);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT (value))
    {
      guint64 number = 0;

      if (!gom_sync_history_parse_uint64 (text, &number, error))
        return FALSE;

      g_value_set_uint (value, (guint)number);
      return TRUE;
    }

  if (G_VALUE_HOLDS_INT64 (value))
    {
      gint64 number = 0;

      if (!gom_sync_history_parse_int64 (text, &number, error))
        return FALSE;

      g_value_set_int64 (value, number);
      return TRUE;
    }

  if (G_VALUE_HOLDS_UINT64 (value))
    {
      guint64 number = 0;

      if (!gom_sync_history_parse_uint64 (text, &number, error))
        return FALSE;

      g_value_set_uint64 (value, number);
      return TRUE;
    }

  if (G_VALUE_HOLDS_DOUBLE (value) || G_VALUE_HOLDS_FLOAT (value))
    {
      double number;
      char *endptr = NULL;

      number = g_ascii_strtod (text, &endptr);
      if (endptr == text || *endptr != '\0')
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Invalid floating point sync history value '%s'",
                       text);
          return FALSE;
        }

      if (G_VALUE_HOLDS_DOUBLE (value))
        g_value_set_double (value, number);
      else
        g_value_set_float (value, (float)number);

      return TRUE;
    }

  if (G_VALUE_HOLDS_STRING (value))
    {
      g_value_set_string (value, is_null ? NULL : text);
      return TRUE;
    }

  if (G_VALUE_HOLDS (value, G_TYPE_GTYPE))
    {
      GType gtype = text != NULL ? g_type_from_name (text) : G_TYPE_INVALID;

      g_value_set_gtype (value, gtype);
      return TRUE;
    }

  if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
    {
      GDateTime *datetime = NULL;

      if (!is_null)
        {
          if ((datetime = g_date_time_new_from_iso8601 (text, NULL)) == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           "Invalid ISO-8601 sync history value '%s'",
                           text);
              return FALSE;
            }
        }

      g_value_take_boxed (value, datetime);
      return TRUE;
    }

  if (G_VALUE_HOLDS (value, G_TYPE_BYTES))
    {
      guchar *data = NULL;
      gsize size = 0;

      if (!is_null)
        data = g_base64_decode (text, &size);

      g_value_take_boxed (value, data != NULL ? g_bytes_new_take (data, size) : NULL);
      return TRUE;
    }

  if (g_type_is_a (value_type, G_TYPE_ENUM))
    {
      gint64 number = 0;

      if (!gom_sync_history_parse_int64 (text, &number, error))
        return FALSE;

      g_value_set_enum (value, (int)number);
      return TRUE;
    }

  if (g_type_is_a (value_type, G_TYPE_FLAGS))
    {
      guint64 number = 0;

      if (!gom_sync_history_parse_uint64 (text, &number, error))
        return FALSE;

      g_value_set_flags (value, (guint)number);
      return TRUE;
    }

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_NOT_SUPPORTED,
               "Cannot deserialize sync history value of type %s",
               type_name);
  return FALSE;
}

static gboolean
gom_sync_history_lookup_next_sequence (GomRepository  *repository,
                                       GomSession     *session,
                                       guint64        *sequence,
                                       GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  gboolean has_row;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (session == NULL || GOM_IS_SESSION (session));
  g_assert (sequence != NULL);

  *sequence = 1;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, GOM_TYPE_SYNC_HISTORY_CHANGE);
  ordering = gom_ordering_new (gom_field_expression_new ("sequence"), GOM_SORT_DESCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));
  gom_query_builder_set_limit (builder, 1);

  if ((query = gom_query_builder_build (builder, error)) == NULL)
    return FALSE;

  if ((cursor = dex_await_object (gom_sync_history_query (repository, session, query),
                                  error)) == NULL)
    return FALSE;

  has_row = dex_await_boolean (gom_cursor_next (cursor), error);
  if (error != NULL && *error != NULL)
    return FALSE;

  if (has_row)
    {
      g_autoptr(GomSyncHistoryChange) change = NULL;

      if (!(change = GOM_SYNC_HISTORY_CHANGE (gom_cursor_materialize (cursor, error))))
        return FALSE;

      *sequence = change->sequence + 1;
    }

  return dex_await (gom_cursor_close (cursor), error);
}

static gboolean
gom_sync_history_insert_change (GomRepository  *repository,
                                GomSession     *session,
                                guint64         sequence,
                                const char     *relation,
                                const char     *identity,
                                GomDelta       *delta,
                                gboolean        outbound,
                                GError        **error)
{
  g_autoptr(GomInsertionBuilder) builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GPtrArray) row = NULL;
  g_autoptr(GDateTime) now = NULL;
  g_autofree char *created_at = NULL;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (session == NULL || GOM_IS_SESSION (session));
  g_assert (relation != NULL);
  g_assert (identity != NULL);
  g_assert (GOM_IS_DELTA (delta));

  now = g_date_time_new_now_utc ();
  if (!(created_at = g_date_time_format_iso8601 (now)))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Failed to create sync history timestamp");
      return FALSE;
    }

  builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_entity_type (builder, GOM_TYPE_SYNC_HISTORY_CHANGE);
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("sequence"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("batch"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("entity_type"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("relation"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("identity"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("delta_kind"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("tombstone"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("outbound"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("sent"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("acked"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("created_at"));

  row = g_ptr_array_new ();
  gom_sync_history_add_uint64_value (row, sequence);
  gom_sync_history_add_uint64_value (row, sequence);
  gom_sync_history_add_string_value (row, g_type_name (gom_delta_get_entity_type (delta)));
  gom_sync_history_add_string_value (row, relation);
  gom_sync_history_add_string_value (row, identity);
  gom_sync_history_add_int_value (row, gom_delta_get_kind (delta));
  gom_sync_history_add_boolean_value (row, gom_delta_get_kind (delta) == GOM_DELTA_KIND_DELETE);
  gom_sync_history_add_boolean_value (row, outbound);
  gom_sync_history_add_boolean_value (row, FALSE);
  gom_sync_history_add_boolean_value (row, FALSE);
  gom_sync_history_add_string_value (row, created_at);

  gom_insertion_builder_add_row (builder, (GomExpression **)row->pdata, row->len);

  if ((insertion = gom_insertion_builder_build (builder, error)) == NULL)
    return FALSE;

  if ((result = dex_await_object (gom_sync_history_mutate (repository,
                                                           session,
                                                           GOM_MUTATION (insertion)),
                                  error)) == NULL)
    return FALSE;

  return TRUE;
}

static gboolean
gom_sync_history_insert_value (GomRepository  *repository,
                               GomSession     *session,
                               guint64         sequence,
                               const char     *property_name,
                               const GValue   *original,
                               const GValue   *current,
                               GError        **error)
{
  g_autoptr(GomInsertionBuilder) builder = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomMutationResult) result = NULL;
  g_autoptr(GPtrArray) row = NULL;
  g_autofree char *original_type = NULL;
  g_autofree char *original_value = NULL;
  g_autofree char *current_type = NULL;
  g_autofree char *current_value = NULL;
  gboolean has_original = original != NULL && G_IS_VALUE (original);
  gboolean has_current = current != NULL && G_IS_VALUE (current);
  gboolean original_is_null = FALSE;
  gboolean current_is_null = FALSE;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (session == NULL || GOM_IS_SESSION (session));
  g_assert (property_name != NULL);

  if (has_original &&
      !gom_sync_history_value_to_text (original,
                                       &original_type,
                                       &original_value,
                                       &original_is_null,
                                       error))
    return FALSE;

  if (has_current &&
      !gom_sync_history_value_to_text (current,
                                       &current_type,
                                       &current_value,
                                       &current_is_null,
                                       error))
    return FALSE;

  builder = gom_insertion_builder_new (repository);
  gom_insertion_builder_set_target_entity_type (builder, GOM_TYPE_SYNC_HISTORY_VALUE);
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("sequence"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("property_name"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("has_original"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("original_type"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("original_value"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("original_is_null"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("has_current"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("current_type"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("current_value"));
  gom_insertion_builder_add_column (builder, gom_field_expression_new ("current_is_null"));

  row = g_ptr_array_new ();
  gom_sync_history_add_uint64_value (row, sequence);
  gom_sync_history_add_string_value (row, property_name);
  gom_sync_history_add_boolean_value (row, has_original);
  gom_sync_history_add_string_value (row, original_type);
  gom_sync_history_add_string_value (row, original_value);
  gom_sync_history_add_boolean_value (row, original_is_null);
  gom_sync_history_add_boolean_value (row, has_current);
  gom_sync_history_add_string_value (row, current_type);
  gom_sync_history_add_string_value (row, current_value);
  gom_sync_history_add_boolean_value (row, current_is_null);

  gom_insertion_builder_add_row (builder, (GomExpression **)row->pdata, row->len);

  if ((insertion = gom_insertion_builder_build (builder, error)) == NULL)
    return FALSE;

  if ((result = dex_await_object (gom_sync_history_mutate (repository,
                                                           session,
                                                           GOM_MUTATION (insertion)),
                                  error)) == NULL)
    return FALSE;

  return TRUE;
}

static gboolean
gom_sync_history_insert_values (GomRepository  *repository,
                                GomSession     *session,
                                guint64         sequence,
                                GomDelta       *delta,
                                GError        **error)
{
  guint n_changes;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (session == NULL || GOM_IS_SESSION (session));
  g_assert (GOM_IS_DELTA (delta));

  n_changes = gom_delta_get_n_changes (delta);
  for (guint i = 0; i < n_changes; i++)
    {
      const char *property_name;
      GValue original = G_VALUE_INIT;
      GValue current = G_VALUE_INIT;
      gboolean has_original;
      gboolean has_current;
      gboolean ok;

      property_name = gom_delta_get_property_name (delta, i);
      has_original = gom_delta_get_original_value (delta, i, &original);
      has_current = gom_delta_get_current_value (delta, i, &current);

      ok = gom_sync_history_insert_value (repository,
                                          session,
                                          sequence,
                                          property_name,
                                          has_original ? &original : NULL,
                                          has_current ? &current : NULL,
                                          error);

      if (G_IS_VALUE (&original))
        g_value_unset (&original);
      if (G_IS_VALUE (&current))
        g_value_unset (&current);

      if (!ok)
        return FALSE;
    }

  return TRUE;
}

static DexFuture *
gom_sync_history_append_fiber (gpointer user_data)
{
  GomSyncHistoryAppend *state = user_data;
  g_autoptr(GError) error = NULL;
  guint64 sequence = 0;

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));
  g_assert (state->session == NULL || GOM_IS_SESSION (state->session));
  g_assert (GOM_IS_DELTA (state->delta));

  if (!_gom_repository_has_sync_history (state->repository))
    return dex_future_new_for_uint64 (0);

  if (!gom_sync_history_lookup_next_sequence (state->repository, state->session, &sequence, &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_sync_history_insert_change (state->repository,
                                       state->session,
                                       sequence,
                                       state->relation,
                                       state->identity,
                                       state->delta,
                                       state->outbound,
                                       &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  if (!gom_sync_history_insert_values (state->repository,
                                       state->session,
                                       sequence,
                                       state->delta,
                                       &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_for_uint64 (sequence);
}

static DexFuture *
gom_sync_history_append (GomRepository *repository,
                         GomSession    *session,
                         const char    *relation,
                         const char    *identity,
                         GomDelta      *delta,
                         gboolean       outbound)
{
  GomSyncHistoryAppend *state;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  if (session != NULL)
    dex_return_error_if_fail (GOM_IS_SESSION (session));
  dex_return_error_if_fail (relation != NULL);
  dex_return_error_if_fail (identity != NULL);
  dex_return_error_if_fail (GOM_IS_DELTA (delta));

  state = g_new0 (GomSyncHistoryAppend, 1);
  state->repository = g_object_ref (repository);
  state->session = session != NULL ? g_object_ref (session) : NULL;
  state->relation = g_strdup (relation);
  state->identity = g_strdup (identity);
  state->delta = g_object_ref (delta);
  state->outbound = outbound;

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_sync_history_append_fiber,
                              state,
                              gom_sync_history_append_free);
}

static GomDelta *
gom_sync_history_load_delta (GomRepository         *repository,
                             GomSyncHistoryChange  *change,
                             GError               **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomExpression) field = NULL;
  g_autoptr(GomExpression) literal = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomDelta) delta = NULL;
  GType entity_type;
  GValue sequence_value = G_VALUE_INIT;

  g_assert (GOM_IS_REPOSITORY (repository));
  g_assert (GOM_IS_SYNC_HISTORY_CHANGE (change));

  if ((entity_type = _gom_sync_history_change_get_entity_type (change)) == G_TYPE_INVALID)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unknown sync history entity type '%s'",
                   change->entity_type);
      return NULL;
    }

  delta = gom_delta_new (entity_type, change->delta_kind);

  g_value_init (&sequence_value, G_TYPE_UINT64);
  g_value_set_uint64 (&sequence_value, change->sequence);
  field = gom_field_expression_new ("sequence");
  literal = gom_literal_expression_new (&sequence_value);
  filter = gom_binary_expression_new_equal (field, literal);
  g_value_unset (&sequence_value);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, GOM_TYPE_SYNC_HISTORY_VALUE);
  gom_query_builder_set_filter (builder, filter);
  ordering = gom_ordering_new (gom_field_expression_new ("property_name"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));

  if ((query = gom_query_builder_build (builder, error)) == NULL)
    return NULL;

  if ((cursor = dex_await_object (gom_repository_query (repository, query), error)) == NULL)
    return NULL;

  for (;;)
    {
      g_autoptr(GomSyncHistoryValue) history_value = NULL;
      GValue original = G_VALUE_INIT;
      GValue current = G_VALUE_INIT;
      gboolean has_row;

      has_row = dex_await_boolean (gom_cursor_next (cursor), error);
      if (error != NULL && *error != NULL)
        return NULL;
      if (!has_row)
        break;

      if (!(history_value = GOM_SYNC_HISTORY_VALUE (gom_cursor_materialize (cursor, error))))
        return NULL;

      if (history_value->has_original &&
          !gom_sync_history_text_to_value (history_value->original_type,
                                           history_value->original_value,
                                           history_value->original_is_null,
                                           &original,
                                           error))
        return NULL;

      if (history_value->has_current &&
          !gom_sync_history_text_to_value (history_value->current_type,
                                           history_value->current_value,
                                           history_value->current_is_null,
                                           &current,
                                           error))
        {
          if (G_IS_VALUE (&original))
            g_value_unset (&original);
          return NULL;
        }

      gom_delta_add_property (delta,
                              history_value->property_name,
                              history_value->has_original ? &original : NULL,
                              history_value->has_current ? &current : NULL);

      if (G_IS_VALUE (&original))
        g_value_unset (&original);
      if (G_IS_VALUE (&current))
        g_value_unset (&current);
    }

  if (!dex_await (gom_cursor_close (cursor), error))
    return NULL;

  return g_steal_pointer (&delta);
}

static DexFuture *
gom_sync_history_replay_fiber (gpointer user_data)
{
  GomSyncHistoryReplay *state = user_data;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomExpression) field = NULL;
  g_autoptr(GomExpression) literal = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GError) error = NULL;
  GValue sequence_value = G_VALUE_INIT;

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));

  if (!_gom_repository_has_sync_history (state->repository))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Repository does not have sync history");

  store = g_list_store_new (GOM_TYPE_SYNC_HISTORY_CHANGE);

  g_value_init (&sequence_value, G_TYPE_UINT64);
  g_value_set_uint64 (&sequence_value, state->sequence);
  field = gom_field_expression_new ("sequence");
  literal = gom_literal_expression_new (&sequence_value);
  filter = gom_binary_expression_new_greater_than (field, literal);
  g_value_unset (&sequence_value);

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, GOM_TYPE_SYNC_HISTORY_CHANGE);
  gom_query_builder_set_filter (builder, filter);
  ordering = gom_ordering_new (gom_field_expression_new ("sequence"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));

  if ((query = gom_query_builder_build (builder, &error)) == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  if ((cursor = dex_await_object (gom_repository_query (state->repository, query), &error)) == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  for (;;)
    {
      g_autoptr(GomSyncHistoryChange) change = NULL;
      g_autoptr(GomDelta) delta = NULL;
      gboolean has_row;

      has_row = dex_await_boolean (gom_cursor_next (cursor), &error);
      if (error != NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));
      if (!has_row)
        break;

      if (!(change = GOM_SYNC_HISTORY_CHANGE (gom_cursor_materialize (cursor, &error))))
        return dex_future_new_for_error (g_steal_pointer (&error));

      if ((delta = gom_sync_history_load_delta (state->repository, change, &error)) == NULL)
        return dex_future_new_for_error (g_steal_pointer (&error));

      g_set_object (&change->delta, delta);
      g_list_store_append (store, change);
    }

  if (!dex_await (gom_cursor_close (cursor), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_take_object (g_steal_pointer (&store));
}

static DexFuture *
gom_sync_history_ensure_fiber (gpointer user_data)
{
  GomSyncHistoryEnsure *state = user_data;
  static const char history_script[] =
    "CREATE TABLE IF NOT EXISTS gom_sync_history ("
    "sequence INTEGER NOT NULL PRIMARY KEY, "
    "batch INTEGER NOT NULL, "
    "entity_type TEXT NOT NULL, "
    "relation TEXT NOT NULL, "
    "identity TEXT NOT NULL, "
    "delta_kind INTEGER NOT NULL, "
    "tombstone BOOLEAN NOT NULL, "
    "outbound BOOLEAN NOT NULL, "
    "sent BOOLEAN NOT NULL, "
    "acked BOOLEAN NOT NULL, "
    "created_at TEXT NOT NULL"
    ");";
  static const char values_script[] =
    "CREATE TABLE IF NOT EXISTS gom_sync_history_values ("
    "sequence INTEGER NOT NULL, "
    "property_name TEXT NOT NULL, "
    "has_original BOOLEAN NOT NULL, "
    "original_type TEXT, "
    "original_value TEXT, "
    "original_is_null BOOLEAN NOT NULL, "
    "has_current BOOLEAN NOT NULL, "
    "current_type TEXT, "
    "current_value TEXT, "
    "current_is_null BOOLEAN NOT NULL, "
    "PRIMARY KEY (sequence, property_name)"
    ");";
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (state != NULL);
  g_assert (GOM_IS_REPOSITORY (state->repository));

  if (!(driver = gom_repository_dup_driver (state->repository)))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_INVALID_ARGUMENT,
                                  "Repository has no driver");

  bytes = g_bytes_new_static (history_script, strlen (history_script));
  if (!dex_await (_gom_driver_execute_sql (driver, bytes), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  g_clear_pointer (&bytes, g_bytes_unref);
  bytes = g_bytes_new_static (values_script, strlen (values_script));
  if (!dex_await (_gom_driver_execute_sql (driver, bytes), &error))
    return dex_future_new_for_error (g_steal_pointer (&error));

  return dex_future_new_true ();
}

DexFuture *
_gom_sync_history_ensure_schema (GomRepository *repository)
{
  GomSyncHistoryEnsure *state;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));

  state = g_new0 (GomSyncHistoryEnsure, 1);
  state->repository = g_object_ref (repository);

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_sync_history_ensure_fiber,
                              state,
                              gom_sync_history_ensure_free);
}

DexFuture *
_gom_sync_history_stage_local_change (GomRepository *repository,
                                      GomSession    *session,
                                      GomEntity     *entity,
                                      GomDelta      *delta)
{
  GomEntityClass *entity_class;
  g_autofree char *identity = NULL;
  g_autoptr(GError) error = NULL;
  const char *relation;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  if (session != NULL)
    dex_return_error_if_fail (GOM_IS_SESSION (session));
  dex_return_error_if_fail (GOM_IS_ENTITY (entity));
  dex_return_error_if_fail (GOM_IS_DELTA (delta));

  if (!_gom_repository_has_sync_history (repository))
    return dex_future_new_for_uint64 (0);

  entity_class = GOM_ENTITY_GET_CLASS (entity);
  relation = gom_entity_class_get_relation (entity_class);

  if ((identity = _gom_tombstone_serialize_entity_identity (entity, &error)) == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return gom_sync_history_append (repository, session, relation, identity, delta, TRUE);
}

DexFuture *
_gom_sync_history_append_remote_change (GomRepository *repository,
                                        const char    *relation,
                                        const char    *identity,
                                        GomDelta      *delta)
{
  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));
  dex_return_error_if_fail (relation != NULL);
  dex_return_error_if_fail (identity != NULL);
  dex_return_error_if_fail (GOM_IS_DELTA (delta));

  if (!_gom_repository_has_sync_history (repository))
    return dex_future_new_for_uint64 (0);

  return gom_sync_history_append (repository, NULL, relation, identity, delta, FALSE);
}

DexFuture *
_gom_sync_history_replay (GomRepository *repository,
                          guint64        sequence)
{
  GomSyncHistoryReplay *state;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));

  state = g_new0 (GomSyncHistoryReplay, 1);
  state->repository = g_object_ref (repository);
  state->sequence = sequence;

  return dex_scheduler_spawn (NULL,
                              0,
                              gom_sync_history_replay_fiber,
                              state,
                              gom_sync_history_replay_free);
}

DexFuture *
_gom_sync_history_ack (GomRepository *repository,
                       guint64        sequence)
{
  g_autoptr(GomUpdateBuilder) builder = NULL;
  g_autoptr(GomExpression) outbound_field = NULL;
  g_autoptr(GomExpression) outbound_value = NULL;
  g_autoptr(GomExpression) outbound_match = NULL;
  g_autoptr(GomExpression) sequence_field = NULL;
  g_autoptr(GomExpression) sequence_value = NULL;
  g_autoptr(GomExpression) sequence_match = NULL;
  g_autoptr(GomExpression) filter = NULL;
  g_autoptr(GomExpression) acked_field = NULL;
  g_autoptr(GomExpression) acked_value = NULL;
  g_autoptr(GomUpdate) update = NULL;
  GValue value = G_VALUE_INIT;
  GValue seq_value = G_VALUE_INIT;
  g_autoptr(GError) error = NULL;

  dex_return_error_if_fail (GOM_IS_REPOSITORY (repository));

  if (!_gom_repository_has_sync_history (repository))
    return dex_future_new_reject (G_IO_ERROR,
                                  G_IO_ERROR_NOT_SUPPORTED,
                                  "Repository does not have sync history");

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, TRUE);
  outbound_field = gom_field_expression_new ("outbound");
  outbound_value = gom_literal_expression_new (&value);
  outbound_match = gom_binary_expression_new_equal (outbound_field, outbound_value);
  acked_field = gom_field_expression_new ("acked");
  acked_value = gom_literal_expression_new (&value);
  g_value_unset (&value);

  g_value_init (&seq_value, G_TYPE_UINT64);
  g_value_set_uint64 (&seq_value, sequence);
  sequence_field = gom_field_expression_new ("sequence");
  sequence_value = gom_literal_expression_new (&seq_value);
  sequence_match = gom_binary_expression_new_less_equal (sequence_field, sequence_value);
  g_value_unset (&seq_value);

  filter = gom_binary_expression_new_and (outbound_match, sequence_match);

  builder = gom_update_builder_new ();
  gom_update_builder_set_target_entity_type (builder, GOM_TYPE_SYNC_HISTORY_CHANGE);
  gom_update_builder_set_filter (builder, filter);
  gom_update_builder_add_assignment (builder,
                                     g_steal_pointer (&acked_field),
                                     g_steal_pointer (&acked_value));

  if ((update = gom_update_builder_build (builder, &error)) == NULL)
    return dex_future_new_for_error (g_steal_pointer (&error));

  return gom_repository_mutate (repository, GOM_MUTATION (update));
}

guint64
_gom_sync_history_change_get_sequence (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), 0);

  return self->sequence;
}

guint64
_gom_sync_history_change_get_batch (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), 0);

  return self->batch;
}

const char *
_gom_sync_history_change_get_relation (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), NULL);

  return self->relation;
}

const char *
_gom_sync_history_change_get_identity (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), NULL);

  return self->identity;
}

GType
_gom_sync_history_change_get_entity_type (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), G_TYPE_INVALID);

  return self->entity_type != NULL ? g_type_from_name (self->entity_type) : G_TYPE_INVALID;
}

GomDeltaKind
_gom_sync_history_change_get_delta_kind (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), GOM_DELTA_KIND_UPDATE);

  return self->delta_kind;
}

gboolean
_gom_sync_history_change_get_tombstone (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), FALSE);

  return self->tombstone;
}

gboolean
_gom_sync_history_change_get_outbound (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), FALSE);

  return self->outbound;
}

gboolean
_gom_sync_history_change_get_sent (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), FALSE);

  return self->sent;
}

gboolean
_gom_sync_history_change_get_acked (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), FALSE);

  return self->acked;
}

GomDelta *
_gom_sync_history_change_dup_delta (GomSyncHistoryChange *self)
{
  g_return_val_if_fail (GOM_IS_SYNC_HISTORY_CHANGE (self), NULL);

  return self->delta != NULL ? g_object_ref (self->delta) : NULL;
}
