/* test-gom.c
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
#include "lib/gom-cursor-private.h"
#include "lib/gom-entity-private.h"
#include "lib/gom-query-private.h"
#include "lib/gom-session-private.h"
#include "lib/gom-value-private.h"
#include "gom-mock-driver-private.h"
#include "test-util.h"

typedef struct _TestCursor                 TestCursor;
typedef struct _TestCursorClass            TestCursorClass;
typedef struct _TestCountCursor            TestCountCursor;
typedef struct _TestCountCursorClass       TestCountCursorClass;
typedef struct _TestCursorEntity           TestCursorEntity;
typedef struct _TestCursorEntityClass      TestCursorEntityClass;
typedef struct _TestCursorChildEntity      TestCursorChildEntity;
typedef struct _TestCursorChildEntityClass TestCursorChildEntityClass;
typedef struct _TestSyncSession            TestSyncSession;
typedef struct _TestSyncSessionClass       TestSyncSessionClass;

typedef enum
{
  TEST_CURSOR_SCENARIO_VALUES,
  TEST_CURSOR_SCENARIO_MATERIALIZE,
} TestCursorScenario;

typedef enum
{
  TEST_CURSOR_STATE_ALPHA = 1,
  TEST_CURSOR_STATE_BETA  = 2,
} TestCursorState;

typedef enum
{
  TEST_CURSOR_PERM_NONE  = 0,
  TEST_CURSOR_PERM_READ  = 1 << 0,
  TEST_CURSOR_PERM_WRITE = 1 << 1,
} TestCursorPerms;

struct _TestCursor
{
  GomCursor          parent_instance;
  TestCursorScenario scenario;
  gint               row;
  guint              n_rows;
  gboolean           closed;
};

struct _TestCursorClass
{
  GomCursorClass parent_class;
};

struct _TestCountCursor
{
  TestCursor parent_instance;
};

struct _TestCountCursorClass
{
  TestCursorClass parent_class;
};

struct _TestCursorEntity
{
  GomEntity        parent_instance;
  char            *type;
  char            *name;
  TestCursorState  state;
  TestCursorPerms  perms;
  gboolean         enabled;
  gint             count;
  gint64           total;
  double           ratio;
  float            weight;
  GBytes          *blob;
};

struct _TestCursorEntityClass
{
  GomEntityClass parent_class;
};

struct _TestCursorChildEntity
{
  TestCursorEntity parent_instance;
};

struct _TestCursorChildEntityClass
{
  TestCursorEntityClass parent_class;
};

struct _TestSyncSession
{
  GomSession  parent_instance;
  guint       accept_count;
  GomDelta   *last_delta;
};

struct _TestSyncSessionClass
{
  GomSessionClass parent_class;
};

enum
{
  TEST_CURSOR_ENTITY_PROP_0,
  TEST_CURSOR_ENTITY_PROP_TYPE,
  TEST_CURSOR_ENTITY_PROP_NAME,
  TEST_CURSOR_ENTITY_PROP_STATE,
  TEST_CURSOR_ENTITY_PROP_PERMS,
  TEST_CURSOR_ENTITY_PROP_ENABLED,
  TEST_CURSOR_ENTITY_PROP_COUNT,
  TEST_CURSOR_ENTITY_PROP_TOTAL,
  TEST_CURSOR_ENTITY_PROP_RATIO,
  TEST_CURSOR_ENTITY_PROP_WEIGHT,
  TEST_CURSOR_ENTITY_PROP_BLOB,
  TEST_CURSOR_ENTITY_N_PROPS
};

static GParamSpec *test_cursor_entity_properties[TEST_CURSOR_ENTITY_N_PROPS];

static GType test_cursor_get_type              (void);
static GType test_count_cursor_get_type        (void);
static GType test_cursor_entity_get_type       (void);
static GType test_cursor_child_entity_get_type (void);
static GType test_sync_session_get_type        (void);
GType        test_cursor_state_get_type        (void) G_GNUC_CONST;
GType        test_cursor_perms_get_type        (void) G_GNUC_CONST;

#define TEST_TYPE_CURSOR             (test_cursor_get_type())
#define TEST_TYPE_COUNT_CURSOR       (test_count_cursor_get_type())
#define TEST_TYPE_CURSOR_ENTITY      (test_cursor_entity_get_type())
#define TEST_TYPE_CURSOR_CHILD_ENTITY (test_cursor_child_entity_get_type())
#define TEST_TYPE_SYNC_SESSION       (test_sync_session_get_type())

G_DEFINE_TYPE (TestCursor, test_cursor, GOM_TYPE_CURSOR)
G_DEFINE_TYPE (TestCountCursor, test_count_cursor, TEST_TYPE_CURSOR)
G_DEFINE_TYPE (TestCursorEntity, test_cursor_entity, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (TestCursorChildEntity, test_cursor_child_entity, TEST_TYPE_CURSOR_ENTITY)
G_DEFINE_ENUM_TYPE (TestCursorState, test_cursor_state,
                    G_DEFINE_ENUM_VALUE (TEST_CURSOR_STATE_ALPHA, "alpha"),
                    G_DEFINE_ENUM_VALUE (TEST_CURSOR_STATE_BETA, "beta"))
G_DEFINE_FLAGS_TYPE (TestCursorPerms, test_cursor_perms,
                     G_DEFINE_ENUM_VALUE (TEST_CURSOR_PERM_NONE, "none"),
                     G_DEFINE_ENUM_VALUE (TEST_CURSOR_PERM_READ, "read"),
                     G_DEFINE_ENUM_VALUE (TEST_CURSOR_PERM_WRITE, "write"))

static guint
test_cursor_get_n_columns (GomCursor *cursor)
{
  TestCursor *self = (TestCursor *)cursor;

  return self->scenario == TEST_CURSOR_SCENARIO_VALUES ? 12 : 10;
}

static const char *
test_cursor_get_column_name (GomCursor *cursor,
                             guint      column)
{
  TestCursor *self = (TestCursor *)cursor;
  static const char * const value_columns[] = {
    "i64", "i", "u", "u64", "b", "d", "f", "s", "null_ptr", "null_str", "bytes", "datetime"
  };
  static const char * const materialize_columns[] = {
    "type", "name", "state", "perms", "enabled", "count", "total", "ratio", "weight", "blob"
  };

  if (self->scenario == TEST_CURSOR_SCENARIO_VALUES)
    return column < G_N_ELEMENTS (value_columns) ? value_columns[column] : NULL;

  return column < G_N_ELEMENTS (materialize_columns) ? materialize_columns[column] : NULL;
}

static gboolean
test_cursor_get_column_value (GomCursor *cursor,
                              guint      column,
                              GValue    *value)
{
  TestCursor *self = (TestCursor *)cursor;

  if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
    g_value_unset (value);

  if (self->scenario == TEST_CURSOR_SCENARIO_VALUES)
    {
      switch (column)
        {
        case 0:
          g_value_init (value, G_TYPE_INT64);
          g_value_set_int64 (value, 101);
          return TRUE;
        case 1:
          g_value_init (value, G_TYPE_INT);
          g_value_set_int (value, 7);
          return TRUE;
        case 2:
          g_value_init (value, G_TYPE_UINT);
          g_value_set_uint (value, 8);
          return TRUE;
        case 3:
          g_value_init (value, G_TYPE_UINT64);
          g_value_set_uint64 (value, 9);
          return TRUE;
        case 4:
          g_value_init (value, G_TYPE_BOOLEAN);
          g_value_set_boolean (value, TRUE);
          return TRUE;
        case 5:
          g_value_init (value, G_TYPE_DOUBLE);
          g_value_set_double (value, 10.5);
          return TRUE;
        case 6:
          g_value_init (value, G_TYPE_FLOAT);
          g_value_set_float (value, 11.5f);
          return TRUE;
        case 7:
          g_value_init (value, G_TYPE_STRING);
          g_value_set_static_string (value, "12");
          return TRUE;
        case 8:
          g_value_init (value, G_TYPE_POINTER);
          g_value_set_pointer (value, NULL);
          return TRUE;
        case 9:
          g_value_init (value, G_TYPE_STRING);
          g_value_set_string (value, NULL);
          return TRUE;
        case 10:
          {
            static const char test_string[] = "xyz";

            g_value_init (value, G_TYPE_BYTES);
            g_value_take_boxed (value, g_bytes_new_static (test_string, strlen (test_string)));
            return TRUE;
          }
        case 11:
          g_value_init (value, G_TYPE_DATE_TIME);
          g_value_take_boxed (value, g_date_time_new_from_iso8601 ("2026-06-09T12:34:56Z", NULL));
          return TRUE;
        default:
          return FALSE;
        }
    }

  switch (column)
    {
    case 0:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, self->row == 0 ? "child" : "base");
      return TRUE;
    case 1:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, self->row == 0 ? "materialized-child" : "materialized-base");
      return TRUE;
    case 2:
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, TEST_CURSOR_STATE_BETA);
      return TRUE;
    case 3:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, "3");
      return TRUE;
    case 4:
      g_value_init (value, G_TYPE_DOUBLE);
      g_value_set_double (value, 1.0);
      return TRUE;
    case 5:
      g_value_init (value, G_TYPE_FLOAT);
      g_value_set_float (value, 7.25f);
      return TRUE;
    case 6:
      g_value_init (value, G_TYPE_UINT);
      g_value_set_uint (value, 42);
      return TRUE;
    case 7:
      g_value_init (value, G_TYPE_STRING);
      g_value_set_static_string (value, "2.5");
      return TRUE;
    case 8:
      g_value_init (value, G_TYPE_UINT64);
      g_value_set_uint64 (value, 99);
      return TRUE;
    case 9:
      {
        static const char test_string[] = "mat";

        g_value_init (value, G_TYPE_BYTES);
        g_value_take_boxed (value, g_bytes_new_static (test_string, strlen (test_string)));
        return TRUE;
      }
    default:
      return FALSE;
    }
}

static const char *
test_cursor_get_column_string (GomCursor *cursor,
                               guint      column)
{
  TestCursor *self = (TestCursor *)cursor;

  if (self->scenario == TEST_CURSOR_SCENARIO_VALUES)
    return column == 7 ? "12" : NULL;

  if (column == 0)
    return self->row == 0 ? "child" : "base";

  if (column == 1)
    return self->row == 0 ? "materialized-child" : "materialized-base";

  if (column == 7)
    return "2.5";

  return NULL;
}

static DexFuture *
test_cursor_next (GomCursor *cursor)
{
  TestCursor *self = (TestCursor *)cursor;

  if (self->closed)
    return dex_future_new_false ();

  self->row++;

  if ((guint)self->row < self->n_rows)
    return dex_future_new_true ();

  return dex_future_new_false ();
}

static DexFuture *
test_cursor_close (GomCursor *cursor)
{
  TestCursor *self = (TestCursor *)cursor;

  self->closed = TRUE;

  return dex_future_new_true ();
}

static void
test_cursor_class_init (TestCursorClass *klass)
{
  GomCursorClass *cursor_class = GOM_CURSOR_CLASS (klass);

  cursor_class->get_n_columns = test_cursor_get_n_columns;
  cursor_class->get_column_name = test_cursor_get_column_name;
  cursor_class->get_column_value = test_cursor_get_column_value;
  cursor_class->get_column_string = test_cursor_get_column_string;
  cursor_class->next = test_cursor_next;
  cursor_class->close = test_cursor_close;
}

static void
test_cursor_init (TestCursor *self)
{
  self->scenario = TEST_CURSOR_SCENARIO_VALUES;
  self->row = -1;
  self->n_rows = 1;
  self->closed = FALSE;
}

static GomCursorCapabilities
test_count_cursor_get_capabilities (GomCursor *cursor)
{
  return GOM_CURSOR_CAPABILITIES_COUNT;
}

static guint64
test_count_cursor_get_count (GomCursor *cursor)
{
  return 77;
}

static void
test_count_cursor_class_init (TestCountCursorClass *klass)
{
  GomCursorClass *cursor_class = GOM_CURSOR_CLASS (klass);

  cursor_class->get_capabilities = test_count_cursor_get_capabilities;
  cursor_class->get_count = test_count_cursor_get_count;
}

static void
test_count_cursor_init (TestCountCursor *self)
{
}

G_DEFINE_TYPE (TestSyncSession, test_sync_session, GOM_TYPE_SESSION)

static void
test_sync_session_accept_entity_changes (GomSession *session,
                                         GomEntity  *entity,
                                         GomDelta   *delta)
{
  TestSyncSession *self = (TestSyncSession *) session;

  g_assert_true (GOM_IS_SESSION (session));
  g_assert_true (GOM_IS_ENTITY (entity));

  self->accept_count++;
  g_clear_object (&self->last_delta);

  if (delta != NULL)
    self->last_delta = g_object_ref (delta);

  _gom_entity_apply_delta (entity, delta, TRUE);
}

static void
test_sync_session_finalize (GObject *object)
{
  TestSyncSession *self = (TestSyncSession *) object;

  g_clear_object (&self->last_delta);

  G_OBJECT_CLASS (test_sync_session_parent_class)->finalize (object);
}

static void
test_sync_session_class_init (TestSyncSessionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomSessionClass *session_class = GOM_SESSION_CLASS (klass);

  object_class->finalize = test_sync_session_finalize;
  session_class->accept_entity_changes = test_sync_session_accept_entity_changes;
}

static void
test_sync_session_init (TestSyncSession *self)
{
  self->accept_count = 0;
  self->last_delta = NULL;
}

static void
test_cursor_entity_finalize (GObject *object)
{
  TestCursorEntity *self = (TestCursorEntity *)object;

  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->blob, g_bytes_unref);

  G_OBJECT_CLASS (test_cursor_entity_parent_class)->finalize (object);
}

static void
test_cursor_entity_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TestCursorEntity *self = (TestCursorEntity *)object;

  switch (prop_id)
    {
    case TEST_CURSOR_ENTITY_PROP_TYPE:
      g_value_set_string (value, self->type);
      break;
    case TEST_CURSOR_ENTITY_PROP_NAME:
      g_value_set_string (value, self->name);
      break;
    case TEST_CURSOR_ENTITY_PROP_STATE:
      g_value_set_enum (value, self->state);
      break;
    case TEST_CURSOR_ENTITY_PROP_PERMS:
      g_value_set_flags (value, self->perms);
      break;
    case TEST_CURSOR_ENTITY_PROP_ENABLED:
      g_value_set_boolean (value, self->enabled);
      break;
    case TEST_CURSOR_ENTITY_PROP_COUNT:
      g_value_set_int (value, self->count);
      break;
    case TEST_CURSOR_ENTITY_PROP_TOTAL:
      g_value_set_int64 (value, self->total);
      break;
    case TEST_CURSOR_ENTITY_PROP_RATIO:
      g_value_set_double (value, self->ratio);
      break;
    case TEST_CURSOR_ENTITY_PROP_WEIGHT:
      g_value_set_float (value, self->weight);
      break;
    case TEST_CURSOR_ENTITY_PROP_BLOB:
      g_value_set_boxed (value, self->blob);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_cursor_entity_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  TestCursorEntity *self = (TestCursorEntity *)object;

  switch (prop_id)
    {
    case TEST_CURSOR_ENTITY_PROP_TYPE:
      g_set_str (&self->type, g_value_get_string (value));
      break;
    case TEST_CURSOR_ENTITY_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;
    case TEST_CURSOR_ENTITY_PROP_STATE:
      self->state = (TestCursorState)g_value_get_enum (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_PERMS:
      self->perms = (TestCursorPerms)g_value_get_flags (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_ENABLED:
      self->enabled = g_value_get_boolean (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_COUNT:
      self->count = g_value_get_int (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_TOTAL:
      self->total = g_value_get_int64 (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_RATIO:
      self->ratio = g_value_get_double (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_WEIGHT:
      self->weight = g_value_get_float (value);
      break;
    case TEST_CURSOR_ENTITY_PROP_BLOB:
      g_clear_pointer (&self->blob, g_bytes_unref);
      self->blob = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_cursor_entity_class_init (TestCursorEntityClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = test_cursor_entity_finalize;
  object_class->get_property = test_cursor_entity_get_property;
  object_class->set_property = test_cursor_entity_set_property;

  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_TYPE] =
    g_param_spec_string ("type", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       test_cursor_state_get_type (), TEST_CURSOR_STATE_ALPHA,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_PERMS] =
    g_param_spec_flags ("perms", NULL, NULL,
                        test_cursor_perms_get_type (), TEST_CURSOR_PERM_NONE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_ENABLED] =
    g_param_spec_boolean ("enabled", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_COUNT] =
    g_param_spec_int ("count", NULL, NULL,
                      G_MININT, G_MAXINT, 0,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_TOTAL] =
    g_param_spec_int64 ("total", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_RATIO] =
    g_param_spec_double ("ratio", NULL, NULL,
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_WEIGHT] =
    g_param_spec_float ("weight", NULL, NULL,
                        -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  test_cursor_entity_properties[TEST_CURSOR_ENTITY_PROP_BLOB] =
    g_param_spec_boxed ("blob", NULL, NULL,
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     TEST_CURSOR_ENTITY_N_PROPS,
                                     test_cursor_entity_properties);

  gom_entity_class_set_relation (entity_class, "test_cursor_entity");
  gom_entity_class_set_identity_field (entity_class, "name");
  gom_entity_class_set_version_added (entity_class, 1);
  gom_entity_class_set_discriminator_field (entity_class, "type");
  gom_entity_class_set_discriminator_value (entity_class, "base");
}

static void
test_cursor_entity_init (TestCursorEntity *self)
{
}

static void
test_cursor_child_entity_class_init (TestCursorChildEntityClass *klass)
{
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  gom_entity_class_set_discriminator_value (entity_class, "child");
}

static void
test_cursor_child_entity_init (TestCursorChildEntity *self)
{
}

static TestCursor *
test_cursor_new_values (void)
{
  TestCursor *cursor = g_object_new (TEST_TYPE_CURSOR, NULL);

  cursor->scenario = TEST_CURSOR_SCENARIO_VALUES;
  cursor->row = -1;
  cursor->n_rows = 1;

  return cursor;
}

static TestCursor *
test_cursor_new_materialize (void)
{
  g_autoptr(GomRegistryBuilder) builder = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GError) error = NULL;
  TestCursor *cursor = g_object_new (TEST_TYPE_CURSOR, NULL);

  builder = gom_registry_builder_new ();
  gom_registry_builder_add_entity_type (builder, TEST_TYPE_CURSOR_ENTITY);
  gom_registry_builder_add_entity_type (builder, TEST_TYPE_CURSOR_CHILD_ENTITY);
  registry = gom_registry_builder_build (builder);
  driver = _gom_mock_driver_new ();
  repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver), registry, NULL), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));

  cursor->scenario = TEST_CURSOR_SCENARIO_MATERIALIZE;
  cursor->row = -1;
  cursor->n_rows = 2;
  ((GomCursor *)cursor)->entity_type = TEST_TYPE_CURSOR_ENTITY;
  _gom_cursor_set_repository (GOM_CURSOR (cursor), repository);

  return cursor;
}

static GomExpression *
new_int_literal (gint value)
{
  GValue v = G_VALUE_INIT;
  GomExpression *expr;

  g_value_init (&v, G_TYPE_INT);
  g_value_set_int (&v, value);
  expr = gom_literal_expression_new (&v);
  g_value_unset (&v);

  return expr;
}

static GomExpression *
new_int64_literal (gint64 value)
{
  return gom_literal_expression_new_int64 (value);
}

static GomExpression *
new_string_literal (const char *value)
{
  return gom_literal_expression_new_string (value);
}

static GomExpression *
new_boolean_literal (gboolean value)
{
  return gom_literal_expression_new_boolean (value);
}

static void
test_expression_api (void)
{
  g_autoptr(GomExpression) literal = new_int64_literal (42);
  g_autoptr(GomExpression) field = gom_field_expression_new ("title");
  GomExpression *function_args[2] = { field, literal };
  g_autoptr(GomExpression) function = NULL;
  g_autoptr(GomExpression) unary_neg = NULL;
  g_autoptr(GomExpression) unary_not = NULL;
  g_autoptr(GomExpression) binary_add = NULL;
  g_autoptr(GomExpression) binary_subtract = NULL;
  g_autoptr(GomExpression) binary_multiply = NULL;
  g_autoptr(GomExpression) binary_divide = NULL;
  g_autoptr(GomExpression) binary_modulo = NULL;
  g_autoptr(GomExpression) binary_equal = NULL;
  g_autoptr(GomExpression) binary_not_equal = NULL;
  g_autoptr(GomExpression) binary_less_than = NULL;
  g_autoptr(GomExpression) binary_less_equal = NULL;
  g_autoptr(GomExpression) binary_greater_than = NULL;
  g_autoptr(GomExpression) binary_greater_equal = NULL;
  g_autoptr(GomExpression) binary_and = NULL;
  g_autoptr(GomExpression) binary_or = NULL;
  g_autoptr(GomExpression) binary_like = NULL;
  g_autoptr(GomExpression) search = NULL;
  g_autoptr(GomExpression) search_for_field = NULL;
  g_autoptr(GomExpression) null_string_literal = NULL;
  g_autoptr(GomExpression) string_literal = NULL;
  g_autoptr(GomExpression) int64_literal = NULL;
  g_autoptr(GomExpression) boolean_literal = NULL;
  g_autoptr(GomExpression) ref_copy = NULL;
  g_autoptr(GParamSpec) param = NULL;
  GValue expression_value = G_VALUE_INIT;

  g_assert_true (GOM_IS_LITERAL_EXPRESSION (literal));
  g_assert_true (gom_expression_is_constant (literal));

  g_assert_true (GOM_IS_FIELD_EXPRESSION (field));
  g_assert_false (gom_expression_is_constant (field));

  string_literal = new_string_literal ("hello");
  g_assert_true (GOM_IS_LITERAL_EXPRESSION (string_literal));
  g_assert_true (gom_expression_is_constant (string_literal));

  null_string_literal = new_string_literal (NULL);
  g_assert_true (GOM_IS_LITERAL_EXPRESSION (null_string_literal));
  g_assert_true (gom_expression_is_constant (null_string_literal));

  int64_literal = new_int64_literal (G_GINT64_CONSTANT (9223372036854775807));
  g_assert_true (GOM_IS_LITERAL_EXPRESSION (int64_literal));
  g_assert_true (gom_expression_is_constant (int64_literal));

  boolean_literal = new_boolean_literal (TRUE);
  g_assert_true (GOM_IS_LITERAL_EXPRESSION (boolean_literal));
  g_assert_true (gom_expression_is_constant (boolean_literal));

  function = gom_function_expression_new ("coalesce", function_args, G_N_ELEMENTS (function_args));
  g_assert_true (GOM_IS_FUNCTION_EXPRESSION (function));
  g_assert_false (gom_expression_is_constant (function));

  unary_neg = gom_unary_expression_new_negate (new_int_literal (7));
  g_assert_true (GOM_IS_UNARY_EXPRESSION (unary_neg));

  unary_not = gom_unary_expression_new_not (new_int_literal (0));
  g_assert_true (GOM_IS_UNARY_EXPRESSION (unary_not));
  binary_add = gom_binary_expression_new_add (new_int_literal (1), new_int_literal (2));
  binary_subtract = gom_binary_expression_new_subtract (new_int_literal (3), new_int_literal (4));
  binary_multiply = gom_binary_expression_new_multiply (new_int_literal (5), new_int_literal (6));
  binary_divide = gom_binary_expression_new_divide (new_int_literal (7), new_int_literal (8));
  binary_modulo = gom_binary_expression_new_modulo (new_int_literal (9), new_int_literal (10));
  binary_equal = gom_binary_expression_new_equal (new_int_literal (11), new_int_literal (12));
  binary_not_equal = gom_binary_expression_new_not_equal (new_int_literal (13), new_int_literal (14));
  binary_less_than = gom_binary_expression_new_less_than (new_int_literal (15), new_int_literal (16));
  binary_less_equal = gom_binary_expression_new_less_equal (new_int_literal (17), new_int_literal (18));
  binary_greater_than = gom_binary_expression_new_greater_than (new_int_literal (19), new_int_literal (20));
  binary_greater_equal = gom_binary_expression_new_greater_equal (new_int_literal (21), new_int_literal (22));
  binary_and = gom_binary_expression_new_and (new_int_literal (23), new_int_literal (24));
  binary_or = gom_binary_expression_new_or (new_int_literal (25), new_int_literal (26));
  binary_like = gom_binary_expression_new_like (new_string_literal ("a"), new_string_literal ("b"));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_add));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_subtract));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_multiply));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_divide));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_modulo));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_equal));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_not_equal));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_less_than));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_less_equal));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_greater_than));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_greater_equal));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_and));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_or));
  g_assert_true (GOM_IS_BINARY_EXPRESSION (binary_like));

  search = gom_search_expression_new (gom_field_expression_new ("body"),
                                      new_string_literal ("hello"),
                                      GOM_SEARCH_MODE_PREFIX);
  g_assert_true (GOM_IS_SEARCH_EXPRESSION (search));
  g_assert_true (GOM_IS_FIELD_EXPRESSION (gom_search_expression_get_target (GOM_SEARCH_EXPRESSION (search))));
  g_assert_true (GOM_IS_LITERAL_EXPRESSION (gom_search_expression_get_query (GOM_SEARCH_EXPRESSION (search))));
  g_assert_cmpint (gom_search_expression_get_mode (GOM_SEARCH_EXPRESSION (search)), ==, GOM_SEARCH_MODE_PREFIX);

  search_for_field = gom_search_expression_new_for_field ("body", "needle", GOM_SEARCH_MODE_PHRASE);
  g_assert_true (GOM_IS_SEARCH_EXPRESSION (search_for_field));

  g_value_init (&expression_value, GOM_TYPE_EXPRESSION);
  gom_value_set_expression (&expression_value, literal);
  g_assert_true (GOM_IS_EXPRESSION (gom_value_get_expression (&expression_value)));

  ref_copy = gom_value_dup_expression (&expression_value);
  g_assert_true (GOM_IS_EXPRESSION (ref_copy));

  gom_value_take_expression (&expression_value, new_string_literal ("owned"));
  g_assert_true (GOM_IS_LITERAL_EXPRESSION (gom_value_get_expression (&expression_value)));

  g_value_unset (&expression_value);

  param = gom_param_spec_expression ("expr", "expr", "expr", G_PARAM_READWRITE);
  g_assert_true (G_IS_PARAM_SPEC (param));
  g_assert_cmpstr (g_param_spec_get_name (param), ==, "expr");
}

static void
test_ordering_and_query_builder (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomQueryBuilder) builder = gom_query_builder_new ();
  g_autoptr(GomQueryBuilder) extra_ref = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomQuery) copied_query = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomOrdering) ordering_full = NULL;
  g_autoptr(GomOrdering) copied = NULL;

  g_assert_nonnull (builder);
  extra_ref = gom_query_builder_ref (builder);
  g_assert_nonnull (extra_ref);

  query = gom_query_builder_build (builder, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (query);
  g_clear_error (&error);

  gom_query_builder_set_target_relation (builder, "manual_relation");
  gom_query_builder_set_target_entity_type (builder, GOM_TYPE_ENTITY);

  gom_query_builder_add_projection (builder, gom_field_expression_new ("id"));
  gom_query_builder_clear_projections (builder);
  gom_query_builder_add_projection (builder, gom_field_expression_new ("title"));

  gom_query_builder_set_filter (builder,
                                gom_binary_expression_new_equal (gom_field_expression_new ("enabled"),
                                                                 new_int_literal (1)));

  gom_query_builder_add_grouping (builder, gom_field_expression_new ("group_id"));
  gom_query_builder_clear_groupings (builder);
  gom_query_builder_add_grouping (builder, gom_field_expression_new ("group_id"));

  gom_query_builder_set_group_filter (builder,
                                      gom_binary_expression_new_greater_than (gom_field_expression_new ("count"),
                                                                              new_int_literal (0)));

  ordering = gom_ordering_new (gom_field_expression_new ("title"), GOM_SORT_DESCENDING);
  g_assert_cmpint (gom_ordering_get_direction (ordering), ==, GOM_SORT_DESCENDING);
  g_assert_cmpint (gom_ordering_get_nulls_mode (ordering), ==, GOM_NULLS_DEFAULT);
  g_assert_true (GOM_IS_EXPRESSION (gom_ordering_get_expression (ordering)));

  ordering_full = gom_ordering_new_full (gom_field_expression_new ("title"), GOM_NULLS_LAST);
  g_assert_cmpint (gom_ordering_get_direction (ordering_full), ==, GOM_SORT_ASCENDING);
  g_assert_cmpint (gom_ordering_get_nulls_mode (ordering_full), ==, GOM_NULLS_LAST);

  copied = gom_ordering_copy (ordering_full);
  g_assert_true (GOM_IS_ORDERING (copied));
  g_assert_cmpint (gom_ordering_get_nulls_mode (copied), ==, GOM_NULLS_LAST);

  gom_query_builder_add_ordering (builder, g_object_ref (ordering));
  gom_query_builder_clear_orderings (builder);
  gom_query_builder_add_ordering (builder, g_object_ref (ordering_full));

  gom_query_builder_set_offset (builder, 2);
  gom_query_builder_set_limit (builder, 7);
  query = gom_query_builder_build_with_count (builder, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_QUERY (query));
  g_assert_true (_gom_query_get_with_count (query));
  copied_query = _gom_query_slice (query, 1, 3);
  g_assert_true (GOM_IS_QUERY (copied_query));
  g_assert_true (_gom_query_get_with_count (copied_query));
}

static DexFuture *
apply_flag_cb (GomDriver *driver,
               gpointer   user_data)
{
  gboolean *called = user_data;

  g_assert_true (GOM_IS_DRIVER (driver));
  *called = TRUE;

  return dex_future_new_true ();
}

static void
test_nested_migration_apply (void)
{
  g_autoptr(GomMockDriver) driver = _gom_mock_driver_new ();
  g_autoptr(GomMockDriver) apply_driver = _gom_mock_driver_new ();
  g_autoptr(GomCustomMigrator) migrator = gom_custom_migrator_new (0);
  g_autoptr(GomMigration) nested = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *driver_uri = NULL;
  gboolean called = FALSE;
  gboolean success;

  g_object_get (driver,
                "uri", &driver_uri,
                NULL);
  g_assert_nonnull (driver_uri);

  gom_custom_migrator_add_migration (migrator,
                                     gom_custom_migration_new (1,
                                                               apply_flag_cb,
                                                               &called,
                                                               NULL));

  nested = gom_nested_migration_new (GOM_MIGRATOR (migrator), GOM_DRIVER (driver));
  g_assert_true (GOM_IS_NESTED_MIGRATION (nested));
  g_assert_cmpuint (gom_migration_get_version (nested), ==, 0);

  success = dex_await (gom_migration_apply (nested, GOM_DRIVER (apply_driver)), &error);
  g_assert_no_error (error);
  g_assert_true (success);
  g_assert_true (called);
}

static void
test_sql_migration_api (void)
{
  static const char test_string[] = "SELECT 1;";
  g_autoptr(GomMockDriver) driver = _gom_mock_driver_new ();
  g_autoptr(GError) error = NULL;
  g_autoptr(GBytes) script = NULL;
  g_autoptr(GBytes) copy = NULL;
  g_autoptr(GomMigration) migration = NULL;
  g_autofree char *data = NULL;
  gsize len = 0;
  guint version = 0;

  script = g_bytes_new_static (test_string, strlen (test_string));
  migration = gom_sql_migration_new (7, script);
  g_assert_true (GOM_IS_SQL_MIGRATION (migration));
  g_assert_true (GOM_IS_MIGRATION (migration));

  version = gom_migration_get_version (migration);
  g_assert_cmpuint (version, ==, 7);

  g_object_get (migration,
                "version", &version,
                NULL);
  g_assert_cmpuint (version, ==, 7);

  copy = gom_sql_migration_dup_script (GOM_SQL_MIGRATION (migration));
  g_assert_nonnull (copy);
  data = g_bytes_unref_to_data (g_steal_pointer (&copy), &len);
  g_assert_cmpuint (len, ==, 9);
  g_assert_cmpmem (data, len, "SELECT 1;", 9);

  g_assert_false (dex_await (gom_migration_apply (migration, GOM_DRIVER (driver)), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);
}

static void
test_update_builder_api (void)
{
  g_autoptr(GomUpdateBuilder) builder = gom_update_builder_new ();
  g_autoptr(GomUpdateBuilder) extra_ref = NULL;
  g_autoptr(GomUpdate) update = NULL;
  g_autoptr(GError) error = NULL;

  extra_ref = gom_update_builder_ref (builder);
  g_assert_nonnull (extra_ref);

  update = gom_update_builder_build (builder, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (update);
  g_clear_error (&error);

  gom_update_builder_set_target_relation (builder, "items");
  gom_update_builder_add_assignment (builder,
                                     gom_literal_expression_new (NULL),
                                     new_string_literal ("updated"));

  update = gom_update_builder_build (builder, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (update);
  g_clear_error (&error);

  gom_update_builder_clear_assignments (builder);
  gom_update_builder_set_target_entity_type (builder, GOM_TYPE_ENTITY);
  gom_update_builder_add_assignment (builder,
                                     gom_field_expression_new ("name"),
                                     new_string_literal ("updated"));
  gom_update_builder_set_filter (builder,
                                 gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                                                  new_int_literal (1)));
  gom_update_builder_set_limit (builder, 1);

  update = gom_update_builder_build (builder, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_UPDATE (update));
}

static void
test_insertion_deletion_builder_api (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomRegistryBuilder) registry_builder = gom_registry_builder_new ();
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomMockDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomInsertionBuilder) insertion_builder = NULL;
  g_autoptr(GomInsertionBuilder) insertion_ref = NULL;
  g_autoptr(GomInsertion) insertion = NULL;
  g_autoptr(GomDeletionBuilder) deletion_builder = gom_deletion_builder_new ();
  g_autoptr(GomDeletionBuilder) deletion_ref = NULL;
  g_autoptr(GomDeletion) deletion = NULL;
  GomExpression *row_values[1];
  GValue value = G_VALUE_INIT;

  registry = gom_registry_builder_build (registry_builder);
  driver = _gom_mock_driver_new ();
  repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver), registry, NULL), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_REPOSITORY (repository));
  insertion_builder = gom_insertion_builder_new (repository);

  insertion_ref = gom_insertion_builder_ref (insertion_builder);
  g_assert_nonnull (insertion_ref);

  insertion = gom_insertion_builder_build (insertion_builder, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (insertion);
  g_clear_error (&error);

  gom_insertion_builder_set_target_relation (insertion_builder, "items");
  gom_insertion_builder_add_column (insertion_builder, gom_field_expression_new ("name"));
  gom_insertion_builder_clear_columns (insertion_builder);
  gom_insertion_builder_add_column (insertion_builder, gom_literal_expression_new (NULL));
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "alpha");
  row_values[0] = gom_literal_expression_new (&value);
  g_value_unset (&value);
  gom_insertion_builder_add_row (insertion_builder, row_values, 1);

  insertion = gom_insertion_builder_build (insertion_builder, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (insertion);
  g_clear_error (&error);

  gom_insertion_builder_clear_columns (insertion_builder);
  gom_insertion_builder_add_column (insertion_builder, gom_field_expression_new ("name"));
  insertion = gom_insertion_builder_build (insertion_builder, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_INSERTION (insertion));

  deletion_ref = gom_deletion_builder_ref (deletion_builder);
  g_assert_nonnull (deletion_ref);

  deletion = gom_deletion_builder_build (deletion_builder, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_assert_null (deletion);
  g_clear_error (&error);

  gom_deletion_builder_set_target_relation (deletion_builder, "items");
  gom_deletion_builder_set_filter (deletion_builder,
                                   gom_binary_expression_new_equal (gom_field_expression_new ("id"),
                                                                    new_int_literal (1)));
  gom_deletion_builder_set_limit (deletion_builder, 1);
  deletion = gom_deletion_builder_build (deletion_builder, &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_DELETION (deletion));
}

static void
test_cursor_default_exhaust_and_getters (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) cursor = G_OBJECT (g_object_new (TEST_TYPE_CURSOR, NULL));
  g_autoptr(GObject) value_cursor = G_OBJECT (test_cursor_new_values ());
  g_auto(GValue) value = G_VALUE_INIT;
  g_autoptr(GBytes) bytes = NULL;
  gsize size = 0;
  const guint8 *data;

  ((TestCursor *) cursor)->n_rows = 2;

  g_assert_cmpuint (gom_cursor_get_capabilities (GOM_CURSOR (cursor)),
                    ==,
                    GOM_CURSOR_CAPABILITIES_NONE);
  g_assert_true (dex_await (gom_cursor_exhaust (GOM_CURSOR (cursor)), &error));
  g_assert_no_error (error);
  g_assert_true (((TestCursor *) cursor)->closed);

  g_assert_false (dex_await (gom_cursor_rewind (GOM_CURSOR (value_cursor)), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  g_assert_false (dex_await (gom_cursor_move_relative (GOM_CURSOR (value_cursor), 1), &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_clear_error (&error);

  g_assert_true (dex_await_boolean (gom_cursor_next (GOM_CURSOR (value_cursor)), &error));
  g_assert_no_error (error);

  g_assert_cmpint (gom_cursor_get_n_columns (GOM_CURSOR (value_cursor)), ==, 12);
  g_assert_cmpstr (gom_cursor_get_column_name (GOM_CURSOR (value_cursor), 7), ==, "s");
  g_assert_cmpstr (gom_cursor_get_column_string (GOM_CURSOR (value_cursor), 7), ==, "12");

  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 0), ==, 101);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 1), ==, 7);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 2), ==, 8);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 3), ==, 9);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 4), ==, 1);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 5), ==, 10);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 6), ==, 11);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 7), ==, 12);
  g_assert_cmpint ((gint)gom_cursor_get_column_int64 (GOM_CURSOR (value_cursor), 99), ==, 0);

  g_assert_true (gom_cursor_get_column_boolean (GOM_CURSOR (value_cursor), 4));
  g_assert_true (gom_cursor_get_column_boolean (GOM_CURSOR (value_cursor), 7));
  g_assert_true (gom_cursor_get_column_boolean (GOM_CURSOR (value_cursor), 3));
  g_assert_false (gom_cursor_get_column_boolean (GOM_CURSOR (value_cursor), 10));
  g_assert_false (gom_cursor_get_column_boolean (GOM_CURSOR (value_cursor), 99));
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 0), ==, 101.0);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 1), ==, 7.0);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 2), ==, 8.0);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 4), ==, 1.0);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 6), ==, 11.5);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 7), ==, 12.0);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 10), ==, 0.0);
  g_assert_cmpfloat (gom_cursor_get_column_double (GOM_CURSOR (value_cursor), 99), ==, 0.0);
  g_assert_true (gom_cursor_get_column_null (GOM_CURSOR (value_cursor), 8));
  g_assert_true (gom_cursor_get_column_null (GOM_CURSOR (value_cursor), 9));
  g_assert_false (gom_cursor_get_column_null (GOM_CURSOR (value_cursor), 4));
  g_assert_true (gom_cursor_get_column_null (GOM_CURSOR (value_cursor), 99));

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, 22);
  g_assert_true (gom_cursor_get_column_by_name (GOM_CURSOR (value_cursor), "s", &value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "12");
  g_assert_false (gom_cursor_get_column_by_name (GOM_CURSOR (value_cursor), "missing", &value));

  bytes = gom_cursor_dup_column_bytes (GOM_CURSOR (value_cursor), 10);
  g_assert_nonnull (bytes);
  data = g_bytes_get_data (bytes, &size);
  g_assert_cmpuint (size, ==, 3);
  g_assert_cmpmem (data, size, "xyz", 3);
  g_assert_null (gom_cursor_dup_column_bytes (GOM_CURSOR (value_cursor), 8));
  g_assert_null (gom_cursor_dup_column_bytes (GOM_CURSOR (value_cursor), 99));

  {
    g_autoptr(GomRecord) record = NULL;
    g_autoptr(GDateTime) datetime = NULL;
    g_autoptr(GBytes) record_bytes = NULL;
    g_autofree char *datetime_text = NULL;

    record = gom_cursor_snapshot (GOM_CURSOR (value_cursor), &error);
    g_assert_no_error (error);
    g_assert_true (GOM_IS_RECORD (record));

    datetime = gom_record_dup_column_date_time (record, 11);
    g_assert_nonnull (datetime);
    datetime_text = g_date_time_format_iso8601 (datetime);
    g_assert_cmpstr (datetime_text, ==, "2026-06-09T12:34:56Z");
    g_assert_null (gom_record_dup_column_date_time (record, 10));
    g_assert_null (gom_record_dup_column_date_time (record, 99));

    record_bytes = gom_record_dup_column_bytes (record, 10);
    g_assert_nonnull (record_bytes);
    data = g_bytes_get_data (record_bytes, &size);
    g_assert_cmpuint (size, ==, 3);
    g_assert_cmpmem (data, size, "xyz", 3);
    g_assert_null (gom_record_dup_column_bytes (record, 11));
    g_assert_null (gom_record_dup_column_bytes (record, 99));
  }

}

static void
test_cursor_get_count (void)
{
  g_autoptr(GObject) cursor = G_OBJECT (g_object_new (TEST_TYPE_COUNT_CURSOR, NULL));

  g_assert_cmpuint (gom_cursor_get_capabilities (GOM_CURSOR (cursor)),
                    ==,
                    GOM_CURSOR_CAPABILITIES_COUNT);
  g_assert_cmpuint (gom_cursor_get_count (GOM_CURSOR (cursor)), ==, 77);

}

static void
test_value_identity_key (void)
{
  static const guint8 byte_data[] = { 0x00, 0x01, 0x7f, 0x80, 0xff };
  g_autoptr(GDateTime) datetime = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autofree char *str_key = NULL;
  g_autofree char *datetime_key = NULL;
  g_autofree char *bytes_key = NULL;
  g_autofree char *int_key = NULL;
  g_autofree char *bool_key = NULL;
  g_autofree char *null_string_key = NULL;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, "line\nslash\\");
  str_key = _gom_value_dup_identity_key (&value);
  g_value_unset (&value);
  g_assert_cmpstr (str_key, ==, "line\\x0aslash\\\\");

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, NULL);
  null_string_key = _gom_value_dup_identity_key (&value);
  g_value_unset (&value);
  g_assert_cmpstr (null_string_key, ==, "\\N");

  datetime = g_date_time_new_from_iso8601 ("2026-06-09T12:34:56Z", NULL);
  g_assert_nonnull (datetime);
  g_value_init (&value, G_TYPE_DATE_TIME);
  g_value_set_boxed (&value, datetime);
  datetime_key = _gom_value_dup_identity_key (&value);
  g_value_unset (&value);
  g_assert_cmpstr (datetime_key, ==, "2026-06-09T12:34:56Z");

  bytes = g_bytes_new_static (byte_data, sizeof byte_data);
  g_value_init (&value, G_TYPE_BYTES);
  g_value_set_boxed (&value, bytes);
  bytes_key = _gom_value_dup_identity_key (&value);
  g_value_unset (&value);
  g_assert_cmpstr (bytes_key, ==, "\\x00\\x01\\x7f\\x80\\xff");

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, -123);
  int_key = _gom_value_dup_identity_key (&value);
  g_value_unset (&value);
  g_assert_cmpstr (int_key, ==, "-123");

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, TRUE);
  bool_key = _gom_value_dup_identity_key (&value);
  g_value_unset (&value);
  g_assert_cmpstr (bool_key, ==, "true");
}

static void
test_value_identity_key_unsupported (void)
{
  g_autofree char *key = NULL;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_OBJECT);

  g_test_expect_message ("Gom",
                         G_LOG_LEVEL_CRITICAL,
                         "*unsupported value type `GObject`*");
  key = _gom_value_dup_identity_key (&value);
  g_test_assert_expected_messages ();

  g_value_unset (&value);
  g_assert_null (key);
}

static void
test_cursor_materialize_discriminator (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GObject) cursor = G_OBJECT (test_cursor_new_materialize ());
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomEntity) second = NULL;
  g_autoptr(GBytes) blob = NULL;
  g_autofree char *type = NULL;
  g_autofree char *name = NULL;
  gsize size = 0;
  const guint8 *data;
  gint state = 0;
  guint perms = 0;
  gboolean enabled = FALSE;
  gint count = 0;
  gint64 total = 0;
  double ratio = 0.0;
  float weight = 0.0f;

  g_type_ensure (TEST_TYPE_CURSOR_CHILD_ENTITY);

  g_assert_true (dex_await_boolean (gom_cursor_next (GOM_CURSOR (cursor)), &error));
  g_assert_no_error (error);

  entity = gom_cursor_materialize (GOM_CURSOR (cursor), &error);
  g_assert_no_error (error);
  g_assert_true (G_TYPE_CHECK_INSTANCE_TYPE (entity, TEST_TYPE_CURSOR_CHILD_ENTITY));
  g_assert_true (((GomCursor *) cursor)->discriminator_cache != NULL);

  g_object_get (entity,
                "type", &type,
                "name", &name,
                "state", &state,
                "perms", &perms,
                "enabled", &enabled,
                "count", &count,
                "total", &total,
                "ratio", &ratio,
                "weight", &weight,
                "blob", &blob,
                NULL);

  g_assert_cmpstr (type, ==, "child");
  g_assert_cmpstr (name, ==, "materialized-child");
  g_assert_cmpint (state, ==, TEST_CURSOR_STATE_BETA);
  g_assert_cmpuint (perms, ==, TEST_CURSOR_PERM_READ | TEST_CURSOR_PERM_WRITE);
  g_assert_true (enabled);
  g_assert_cmpint (count, ==, 7);
  g_assert_cmpint ((gint)total, ==, 42);
  g_assert_cmpfloat (ratio, ==, 2.5);
  g_assert_cmpfloat (weight, ==, 99.0f);
  g_assert_nonnull (blob);
  data = g_bytes_get_data (blob, &size);
  g_assert_cmpuint (size, ==, 3);
  g_assert_cmpmem (data, size, "mat", 3);

  g_assert_true (dex_await_boolean (gom_cursor_next (GOM_CURSOR (cursor)), &error));
  g_assert_no_error (error);

  second = gom_cursor_materialize (GOM_CURSOR (cursor), &error);
  g_assert_no_error (error);
  g_assert_true (G_TYPE_CHECK_INSTANCE_TYPE (second, TEST_TYPE_CURSOR_ENTITY));

}

static void
test_cursor_materialize_change_tracking (void)
{
  g_autoptr(GError) error = NULL;
  TestCursor *cursor = test_cursor_new_materialize ();
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;
  g_autofree char *original = NULL;
  g_autofree char *current = NULL;
  GValue value = G_VALUE_INIT;

  g_type_ensure (TEST_TYPE_CURSOR_CHILD_ENTITY);

  g_assert_true (dex_await_boolean (gom_cursor_next (GOM_CURSOR (cursor)), &error));
  g_assert_no_error (error);

  entity = gom_cursor_materialize (GOM_CURSOR (cursor), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (entity));

  g_object_set (entity,
                "state", TEST_CURSOR_STATE_ALPHA,
                NULL);

  delta = gom_entity_build_delta (entity, &error);
  g_assert_no_error (error);
  g_assert_nonnull (delta);
  g_assert_cmpuint (gom_delta_get_n_changes (delta), ==, 1);
  g_assert_cmpstr (gom_delta_get_property_name (delta, 0), ==, "state");

  g_value_init (&value, test_cursor_state_get_type ());
  g_assert_true (gom_delta_get_original_value (delta, 0, &value));
  original = g_strdup_printf ("%d", g_value_get_enum (&value));
  g_value_unset (&value);
  g_assert_cmpstr (original, ==, "2");

  g_value_init (&value, test_cursor_state_get_type ());
  g_assert_true (gom_delta_get_current_value (delta, 0, &value));
  current = g_strdup_printf ("%d", g_value_get_enum (&value));
  g_value_unset (&value);
  g_assert_cmpstr (current, ==, "1");

  g_object_set (entity,
                "state", TEST_CURSOR_STATE_BETA,
                NULL);
  g_clear_object (&delta);

  delta = gom_entity_build_delta (entity, &error);
  g_assert_no_error (error);
  g_assert_null (delta);

  g_clear_object (&cursor);
}

static void
test_session_accept_entity_changes (void)
{
  g_autoptr(GError) error = NULL;
  TestCursor *cursor = test_cursor_new_materialize ();
  TestSyncSession *session = NULL;
  g_autoptr(GomEntity) entity = NULL;
  g_autoptr(GomDelta) delta = NULL;

  g_assert_true (dex_await_boolean (gom_cursor_next (GOM_CURSOR (cursor)), &error));
  g_assert_no_error (error);

  entity = gom_cursor_materialize (GOM_CURSOR (cursor), &error);
  g_assert_no_error (error);
  g_assert_true (GOM_IS_ENTITY (entity));

  g_object_set (entity,
                "state", TEST_CURSOR_STATE_ALPHA,
                NULL);

  delta = gom_entity_build_delta (entity, &error);
  g_assert_no_error (error);
  g_assert_nonnull (delta);
  g_assert_cmpuint (gom_delta_get_kind (delta), ==, GOM_DELTA_KIND_UPDATE);
  g_assert_cmpuint (gom_delta_get_entity_type (delta), ==, G_OBJECT_TYPE (entity));

  session = g_object_new (TEST_TYPE_SYNC_SESSION, NULL);
  _gom_session_accept_entity_changes (GOM_SESSION (session), entity, delta);

  g_assert_cmpuint (session->accept_count, ==, 1);
  g_assert_nonnull (session->last_delta);
  g_assert_cmpuint (gom_delta_get_n_changes (session->last_delta), ==, 1);

  g_clear_object (&delta);
  delta = gom_entity_build_delta (entity, &error);
  g_assert_no_error (error);
  g_assert_null (delta);

  g_clear_object (&session);
  g_clear_object (&cursor);
}

static void
test_vector_distance (void)
{
  const float left_values[] = { 1.f, 0.f };
  const float right_values[] = { 0.f, 1.f };
  g_autoptr(GomVector) left = NULL;
  g_autoptr(GomVector) right = NULL;
  g_autoptr(GError) error = NULL;
  const float *left_readback;
  double distance = 0;
  guint n_values = 0;

  left = gom_vector_new_float32 (left_values, G_N_ELEMENTS (left_values));
  right = gom_vector_new_float32 (right_values, G_N_ELEMENTS (right_values));

  left_readback = gom_vector_get_float32 (left, &n_values);
  g_assert_nonnull (left_readback);
  g_assert_cmpuint (n_values, ==, G_N_ELEMENTS (left_values));
  g_assert_cmpfloat_with_epsilon (left_readback[0], 1.0, .0001);
  g_assert_cmpfloat_with_epsilon (left_readback[1], 0.0, .0001);

  g_assert_true (gom_vector_distance (left, right, GOM_VECTOR_METRIC_COSINE, &distance, &error));
  g_assert_no_error (error);
  g_assert_cmpfloat_with_epsilon (distance, 1.0, .0001);

  g_assert_true (gom_vector_distance (left, right, GOM_VECTOR_METRIC_DOT, &distance, &error));
  g_assert_no_error (error);
  g_assert_cmpfloat_with_epsilon (distance, 0.0, .0001);

  g_assert_true (gom_vector_distance (left, right, GOM_VECTOR_METRIC_L2, &distance, &error));
  g_assert_no_error (error);
  g_assert_cmpfloat_with_epsilon (distance, 2.0, .0001);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  _g_test_add_func ("/Gom/expression/api", test_expression_api);
  _g_test_add_func ("/Gom/query/builder", test_ordering_and_query_builder);
  _g_test_add_func ("/Gom/update/builder", test_update_builder_api);
  _g_test_add_func ("/Gom/mutation/builders", test_insertion_deletion_builder_api);
  _g_test_add_func ("/Gom/migration/nested", test_nested_migration_apply);
  _g_test_add_func ("/Gom/migration/sql", test_sql_migration_api);
  _g_test_add_func ("/Gom/value/identity-key", test_value_identity_key);
  _g_test_add_func ("/Gom/value/identity-key-unsupported", test_value_identity_key_unsupported);
  _g_test_add_func ("/Gom/cursor/default-exhaust-getters", test_cursor_default_exhaust_and_getters);
  _g_test_add_func ("/Gom/cursor/get-count", test_cursor_get_count);
  _g_test_add_func ("/Gom/cursor/materialize-discriminator", test_cursor_materialize_discriminator);
  _g_test_add_func ("/Gom/cursor/materialize-change-tracking", test_cursor_materialize_change_tracking);
  _g_test_add_func ("/Gom/session/accept-entity-changes", test_session_accept_entity_changes);
  _g_test_add_func ("/Gom/vector/distance", test_vector_distance);
  return g_test_run ();
}
