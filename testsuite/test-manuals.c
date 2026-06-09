/* test-manuals.c
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

#include "test-util.h"

typedef struct _ManualsSdk          ManualsSdk;
typedef struct _ManualsSdkClass     ManualsSdkClass;
typedef struct _ManualsBook         ManualsBook;
typedef struct _ManualsBookClass    ManualsBookClass;
typedef struct _ManualsHeading      ManualsHeading;
typedef struct _ManualsHeadingClass ManualsHeadingClass;
typedef struct _ManualsKeyword      ManualsKeyword;
typedef struct _ManualsKeywordClass ManualsKeywordClass;

GType manuals_sdk_get_type     (void) G_GNUC_CONST;
GType manuals_book_get_type    (void) G_GNUC_CONST;
GType manuals_heading_get_type (void) G_GNUC_CONST;
GType manuals_keyword_get_type (void) G_GNUC_CONST;

struct _ManualsSdk
{
  GomEntity parent_instance;

  gint64  id;
  char   *name;
  char   *kind;
  char   *ident;
  char   *version;
};

struct _ManualsSdkClass
{
  GomEntityClass parent_class;
};

struct _ManualsBook
{
  GomEntity parent_instance;

  gint64  id;
  gint64  sdk_id;
  char   *title;
  char   *language;
  char   *uri;
};

struct _ManualsBookClass
{
  GomEntityClass parent_class;
};

struct _ManualsHeading
{
  GomEntity parent_instance;

  gint64  id;
  gint64  book_id;
  gint64  parent_id;
  gint64  has_children;
  char   *title;
  char   *uri;
};

struct _ManualsHeadingClass
{
  GomEntityClass parent_class;
};

struct _ManualsKeyword
{
  GomEntity parent_instance;

  gint64  id;
  gint64  book_id;
  char   *name;
  char   *kind;
  char   *uri;
  char   *since;
  char   *stability;
  char   *deprecated;
};

struct _ManualsKeywordClass
{
  GomEntityClass parent_class;
};

enum {
  SDK_PROP_0,
  SDK_PROP_ID,
  SDK_PROP_NAME,
  SDK_PROP_KIND,
  SDK_PROP_IDENT,
  SDK_PROP_VERSION,
  SDK_N_PROPS
};

enum {
  BOOK_PROP_0,
  BOOK_PROP_ID,
  BOOK_PROP_SDK_ID,
  BOOK_PROP_TITLE,
  BOOK_PROP_LANGUAGE,
  BOOK_PROP_URI,
  BOOK_N_PROPS
};

enum {
  HEADING_PROP_0,
  HEADING_PROP_ID,
  HEADING_PROP_BOOK_ID,
  HEADING_PROP_PARENT_ID,
  HEADING_PROP_HAS_CHILDREN,
  HEADING_PROP_TITLE,
  HEADING_PROP_URI,
  HEADING_N_PROPS
};

enum {
  KEYWORD_PROP_0,
  KEYWORD_PROP_ID,
  KEYWORD_PROP_BOOK_ID,
  KEYWORD_PROP_NAME,
  KEYWORD_PROP_KIND,
  KEYWORD_PROP_URI,
  KEYWORD_PROP_SINCE,
  KEYWORD_PROP_STABILITY,
  KEYWORD_PROP_DEPRECATED,
  KEYWORD_N_PROPS
};

static GParamSpec *sdk_properties[SDK_N_PROPS];
static GParamSpec *book_properties[BOOK_N_PROPS];
static GParamSpec *heading_properties[HEADING_N_PROPS];
static GParamSpec *keyword_properties[KEYWORD_N_PROPS];

G_DEFINE_TYPE (ManualsSdk, manuals_sdk, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (ManualsBook, manuals_book, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (ManualsHeading, manuals_heading, GOM_TYPE_ENTITY)
G_DEFINE_TYPE (ManualsKeyword, manuals_keyword, GOM_TYPE_ENTITY)

static GomRegistry *
manuals_create_registry (void)
{
  g_autoptr(GomRegistryBuilder) builder = gom_registry_builder_new ();

  gom_registry_builder_add_entity_type (builder, manuals_sdk_get_type ());
  gom_registry_builder_add_entity_type (builder, manuals_book_get_type ());
  gom_registry_builder_add_entity_type (builder, manuals_heading_get_type ());
  gom_registry_builder_add_entity_type (builder, manuals_keyword_get_type ());

  return gom_registry_builder_build (builder);
}

static void
manuals_sdk_finalize (GObject *object)
{
  ManualsSdk *self = (ManualsSdk *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->kind, g_free);
  g_clear_pointer (&self->ident, g_free);
  g_clear_pointer (&self->version, g_free);

  G_OBJECT_CLASS (manuals_sdk_parent_class)->finalize (object);
}

static void
manuals_sdk_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  ManualsSdk *self = (ManualsSdk *)object;

  switch (prop_id)
    {
    case SDK_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case SDK_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case SDK_PROP_KIND:
      g_value_set_string (value, self->kind);
      break;

    case SDK_PROP_IDENT:
      g_value_set_string (value, self->ident);
      break;

    case SDK_PROP_VERSION:
      g_value_set_string (value, self->version);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_sdk_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ManualsSdk *self = (ManualsSdk *)object;

  switch (prop_id)
    {
    case SDK_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case SDK_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case SDK_PROP_KIND:
      g_set_str (&self->kind, g_value_get_string (value));
      break;

    case SDK_PROP_IDENT:
      g_set_str (&self->ident, g_value_get_string (value));
      break;

    case SDK_PROP_VERSION:
      g_set_str (&self->version, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_sdk_class_init (ManualsSdkClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = manuals_sdk_finalize;
  object_class->get_property = manuals_sdk_get_property;
  object_class->set_property = manuals_sdk_set_property;

  sdk_properties[SDK_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  sdk_properties[SDK_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  sdk_properties[SDK_PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  sdk_properties[SDK_PROP_IDENT] =
    g_param_spec_string ("ident", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  sdk_properties[SDK_PROP_VERSION] =
    g_param_spec_string ("version", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, SDK_N_PROPS, sdk_properties);

  gom_entity_class_set_relation (entity_class, "sdks");
  gom_entity_class_set_identity_field (entity_class, "id");
}

static void
manuals_sdk_init (ManualsSdk *self)
{
}

static void
manuals_book_finalize (GObject *object)
{
  ManualsBook *self = (ManualsBook *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->language, g_free);
  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (manuals_book_parent_class)->finalize (object);
}

static void
manuals_book_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ManualsBook *self = (ManualsBook *)object;

  switch (prop_id)
    {
    case BOOK_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case BOOK_PROP_SDK_ID:
      g_value_set_int64 (value, self->sdk_id);
      break;

    case BOOK_PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case BOOK_PROP_LANGUAGE:
      g_value_set_string (value, self->language);
      break;

    case BOOK_PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_book_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ManualsBook *self = (ManualsBook *)object;

  switch (prop_id)
    {
    case BOOK_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case BOOK_PROP_SDK_ID:
      self->sdk_id = g_value_get_int64 (value);
      break;

    case BOOK_PROP_TITLE:
      g_set_str (&self->title, g_value_get_string (value));
      break;

    case BOOK_PROP_LANGUAGE:
      g_set_str (&self->language, g_value_get_string (value));
      break;

    case BOOK_PROP_URI:
      g_set_str (&self->uri, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_book_class_init (ManualsBookClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = manuals_book_finalize;
  object_class->get_property = manuals_book_get_property;
  object_class->set_property = manuals_book_set_property;

  book_properties[BOOK_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  book_properties[BOOK_PROP_SDK_ID] =
    g_param_spec_int64 ("sdk-id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  book_properties[BOOK_PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  book_properties[BOOK_PROP_LANGUAGE] =
    g_param_spec_string ("language", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  book_properties[BOOK_PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, BOOK_N_PROPS, book_properties);

  gom_entity_class_set_relation (entity_class, "books");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_property_set_unique (entity_class, "uri", TRUE);
}

static void
manuals_book_init (ManualsBook *self)
{
}

static void
manuals_heading_finalize (GObject *object)
{
  ManualsHeading *self = (ManualsHeading *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (manuals_heading_parent_class)->finalize (object);
}

static void
manuals_heading_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ManualsHeading *self = (ManualsHeading *)object;

  switch (prop_id)
    {
    case HEADING_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case HEADING_PROP_BOOK_ID:
      g_value_set_int64 (value, self->book_id);
      break;

    case HEADING_PROP_PARENT_ID:
      g_value_set_int64 (value, self->parent_id);
      break;

    case HEADING_PROP_HAS_CHILDREN:
      g_value_set_int64 (value, self->has_children);
      break;

    case HEADING_PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case HEADING_PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_heading_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ManualsHeading *self = (ManualsHeading *)object;

  switch (prop_id)
    {
    case HEADING_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case HEADING_PROP_BOOK_ID:
      self->book_id = g_value_get_int64 (value);
      break;

    case HEADING_PROP_PARENT_ID:
      self->parent_id = g_value_get_int64 (value);
      break;

    case HEADING_PROP_HAS_CHILDREN:
      self->has_children = g_value_get_int64 (value);
      break;

    case HEADING_PROP_TITLE:
      g_set_str (&self->title, g_value_get_string (value));
      break;

    case HEADING_PROP_URI:
      g_set_str (&self->uri, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_heading_class_init (ManualsHeadingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = manuals_heading_finalize;
  object_class->get_property = manuals_heading_get_property;
  object_class->set_property = manuals_heading_set_property;

  heading_properties[HEADING_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  heading_properties[HEADING_PROP_BOOK_ID] =
    g_param_spec_int64 ("book-id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  heading_properties[HEADING_PROP_PARENT_ID] =
    g_param_spec_int64 ("parent-id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  heading_properties[HEADING_PROP_HAS_CHILDREN] =
    g_param_spec_int64 ("has-children", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  heading_properties[HEADING_PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  heading_properties[HEADING_PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, HEADING_N_PROPS, heading_properties);

  gom_entity_class_set_relation (entity_class, "headings");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_property_set_unique (entity_class, "uri", TRUE);
}

static void
manuals_heading_init (ManualsHeading *self)
{
}

static void
manuals_keyword_finalize (GObject *object)
{
  ManualsKeyword *self = (ManualsKeyword *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->kind, g_free);
  g_clear_pointer (&self->uri, g_free);
  g_clear_pointer (&self->since, g_free);
  g_clear_pointer (&self->stability, g_free);
  g_clear_pointer (&self->deprecated, g_free);

  G_OBJECT_CLASS (manuals_keyword_parent_class)->finalize (object);
}

static void
manuals_keyword_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ManualsKeyword *self = (ManualsKeyword *)object;

  switch (prop_id)
    {
    case KEYWORD_PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case KEYWORD_PROP_BOOK_ID:
      g_value_set_int64 (value, self->book_id);
      break;

    case KEYWORD_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case KEYWORD_PROP_KIND:
      g_value_set_string (value, self->kind);
      break;

    case KEYWORD_PROP_URI:
      g_value_set_string (value, self->uri);
      break;

    case KEYWORD_PROP_SINCE:
      g_value_set_string (value, self->since);
      break;

    case KEYWORD_PROP_STABILITY:
      g_value_set_string (value, self->stability);
      break;

    case KEYWORD_PROP_DEPRECATED:
      g_value_set_string (value, self->deprecated);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_keyword_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ManualsKeyword *self = (ManualsKeyword *)object;

  switch (prop_id)
    {
    case KEYWORD_PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case KEYWORD_PROP_BOOK_ID:
      self->book_id = g_value_get_int64 (value);
      break;

    case KEYWORD_PROP_NAME:
      g_set_str (&self->name, g_value_get_string (value));
      break;

    case KEYWORD_PROP_KIND:
      g_set_str (&self->kind, g_value_get_string (value));
      break;

    case KEYWORD_PROP_URI:
      g_set_str (&self->uri, g_value_get_string (value));
      break;

    case KEYWORD_PROP_SINCE:
      g_set_str (&self->since, g_value_get_string (value));
      break;

    case KEYWORD_PROP_STABILITY:
      g_set_str (&self->stability, g_value_get_string (value));
      break;

    case KEYWORD_PROP_DEPRECATED:
      g_set_str (&self->deprecated, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
manuals_keyword_class_init (ManualsKeywordClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = manuals_keyword_finalize;
  object_class->get_property = manuals_keyword_get_property;
  object_class->set_property = manuals_keyword_set_property;

  keyword_properties[KEYWORD_PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_BOOK_ID] =
    g_param_spec_int64 ("book-id", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_KIND] =
    g_param_spec_string ("kind", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_URI] =
    g_param_spec_string ("uri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_SINCE] =
    g_param_spec_string ("since", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_STABILITY] =
    g_param_spec_string ("stability", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  keyword_properties[KEYWORD_PROP_DEPRECATED] =
    g_param_spec_string ("deprecated", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, KEYWORD_N_PROPS, keyword_properties);

  gom_entity_class_set_relation (entity_class, "keywords");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_property_set_unique (entity_class, "uri", TRUE);
  gom_entity_class_property_set_search_flags (entity_class, "name", GOM_SEARCH_INDEXED);
}

static void
manuals_keyword_init (ManualsKeyword *self)
{
}

static const char *
manuals_str_or_dash (const char *value)
{
  return value != NULL ? value : "-";
}

static GomExpression *
manuals_equal_int64 (const char *field,
                     gint64      value)
{
  GValue literal = G_VALUE_INIT;
  GomExpression *expr;

  g_value_init (&literal, G_TYPE_INT64);
  g_value_set_int64 (&literal, value);
  expr = gom_binary_expression_new_equal (gom_field_expression_new (field),
                                          gom_literal_expression_new (&literal));
  g_value_unset (&literal);

  return expr;
}

static gboolean
manuals_print_headings (GomRepository  *repository,
                        gint64          book_id,
                        GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomExpression) filter = NULL;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, manuals_heading_get_type ());
  gom_query_builder_add_projection (builder, gom_field_expression_new ("id"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("title"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("uri"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("parent-id"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("has-children"));

  filter = manuals_equal_int64 ("book-id", book_id);
  gom_query_builder_set_filter (builder, filter);

  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));

  query = gom_query_builder_build (builder, error);
  if (query == NULL)
    return FALSE;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return FALSE;

  g_print ("      Headings:\n");

  while (dex_await_boolean (gom_cursor_next (cursor), error))
    {
      g_autoptr(GomEntity) entity = NULL;
      ManualsHeading *heading;
      const char *title;
      const char *uri;

      entity = gom_cursor_materialize (cursor, error);
      if (entity == NULL)
        break;

      heading = (ManualsHeading *)entity;
      title = manuals_str_or_dash (heading->title);
      uri = manuals_str_or_dash (heading->uri);

      g_print ("        [%" G_GINT64_FORMAT "] %s", heading->id, title);
      if (heading->parent_id > 0)
        g_print (" parent=%" G_GINT64_FORMAT, heading->parent_id);
      if (heading->has_children)
        g_print (" children=%" G_GINT64_FORMAT, heading->has_children);
      if (uri[0] != '\0' && g_strcmp0 (uri, "-") != 0)
        g_print (" uri=%s", uri);
      g_print ("\n");
    }

  if (*error != NULL)
    return FALSE;

  dex_await (gom_cursor_close (cursor), error);
  return *error == NULL;
}

static gboolean
manuals_print_keywords (GomRepository  *repository,
                        gint64          book_id,
                        GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomExpression) filter = NULL;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, manuals_keyword_get_type ());
  gom_query_builder_add_projection (builder, gom_field_expression_new ("id"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("name"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("kind"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("uri"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("since"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("stability"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("deprecated"));

  filter = manuals_equal_int64 ("book-id", book_id);
  gom_query_builder_set_filter (builder, filter);

  gom_query_builder_add_ordering (builder, gom_ordering_new (gom_field_expression_new ("name"), GOM_SORT_ASCENDING));

  query = gom_query_builder_build (builder, error);
  if (query == NULL)
    return FALSE;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return FALSE;

  g_print ("      Keywords:\n");

  while (dex_await_boolean (gom_cursor_next (cursor), error))
    {
      g_autoptr(GomEntity) entity = NULL;
      ManualsKeyword *keyword;
      const char *name;
      const char *kind;
      const char *uri;
      const char *since;
      const char *stability;
      const char *deprecated;

      entity = gom_cursor_materialize (cursor, error);
      if (entity == NULL)
        break;

      keyword = (ManualsKeyword *)entity;
      name = manuals_str_or_dash (keyword->name);
      kind = manuals_str_or_dash (keyword->kind);
      uri = manuals_str_or_dash (keyword->uri);
      since = manuals_str_or_dash (keyword->since);
      stability = manuals_str_or_dash (keyword->stability);
      deprecated = manuals_str_or_dash (keyword->deprecated);

      g_print ("        [%" G_GINT64_FORMAT "] %s", keyword->id, name);
      if (kind[0] != '\0' && g_strcmp0 (kind, "-") != 0)
        g_print (" kind=%s", kind);
      if (uri[0] != '\0' && g_strcmp0 (uri, "-") != 0)
        g_print (" uri=%s", uri);
      if (since[0] != '\0' && g_strcmp0 (since, "-") != 0)
        g_print (" since=%s", since);
      if (stability[0] != '\0' && g_strcmp0 (stability, "-") != 0)
        g_print (" stability=%s", stability);
      if (deprecated[0] != '\0' && g_strcmp0 (deprecated, "-") != 0)
        g_print (" deprecated=%s", deprecated);
      g_print ("\n");
    }

  if (*error != NULL)
    return FALSE;

  dex_await (gom_cursor_close (cursor), error);
  return *error == NULL;
}

static gboolean
manuals_print_books (GomRepository  *repository,
                     gint64          sdk_id,
                     GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;
  g_autoptr(GomExpression) filter = NULL;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, manuals_book_get_type ());
  gom_query_builder_add_projection (builder, gom_field_expression_new ("id"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("title"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("language"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("uri"));

  filter = manuals_equal_int64 ("sdk-id", sdk_id);
  gom_query_builder_set_filter (builder, filter);

  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));

  query = gom_query_builder_build (builder, error);
  if (query == NULL)
    return FALSE;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return FALSE;

  while (dex_await_boolean (gom_cursor_next (cursor), error))
    {
      g_autoptr(GomEntity) entity = NULL;
      ManualsBook *book;
      const char *title;
      const char *language;
      const char *uri;

      entity = gom_cursor_materialize (cursor, error);
      if (entity == NULL)
        break;

      book = (ManualsBook *)entity;
      title = manuals_str_or_dash (book->title);
      language = manuals_str_or_dash (book->language);
      uri = manuals_str_or_dash (book->uri);

      g_print ("    Book [%" G_GINT64_FORMAT "]: %s", book->id, title);
      if (language[0] != '\0' && g_strcmp0 (language, "-") != 0)
        g_print (" lang=%s", language);
      if (uri[0] != '\0' && g_strcmp0 (uri, "-") != 0)
        g_print (" uri=%s", uri);
      g_print ("\n");

      if (!manuals_print_headings (repository, book->id, error))
        break;
      if (!manuals_print_keywords (repository, book->id, error))
        break;
    }

  if (*error != NULL)
    return FALSE;

  dex_await (gom_cursor_close (cursor), error);
  return *error == NULL;
}

static gboolean
manuals_print_sdks (GomRepository  *repository,
                    GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomOrdering) ordering = NULL;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, manuals_sdk_get_type ());
  gom_query_builder_add_projection (builder, gom_field_expression_new ("id"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("name"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("kind"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("ident"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("version"));

  ordering = gom_ordering_new (gom_field_expression_new ("id"), GOM_SORT_ASCENDING);
  gom_query_builder_add_ordering (builder, g_steal_pointer (&ordering));

  query = gom_query_builder_build (builder, error);
  if (query == NULL)
    return FALSE;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return FALSE;

  while (dex_await_boolean (gom_cursor_next (cursor), error))
    {
      g_autoptr(GomEntity) entity = NULL;
      ManualsSdk *sdk;
      const char *name;
      const char *kind;
      const char *ident;
      const char *version;

      entity = gom_cursor_materialize (cursor, error);
      if (entity == NULL)
        break;

      sdk = (ManualsSdk *)entity;
      name = manuals_str_or_dash (sdk->name);
      kind = manuals_str_or_dash (sdk->kind);
      ident = manuals_str_or_dash (sdk->ident);
      version = manuals_str_or_dash (sdk->version);

      g_print ("SDK [%" G_GINT64_FORMAT "]: %s", sdk->id, name);
      if (kind[0] != '\0' && g_strcmp0 (kind, "-") != 0)
        g_print (" kind=%s", kind);
      if (ident[0] != '\0' && g_strcmp0 (ident, "-") != 0)
        g_print (" ident=%s", ident);
      if (version[0] != '\0' && g_strcmp0 (version, "-") != 0)
        g_print (" version=%s", version);
      g_print ("\n");

      if (!manuals_print_books (repository, sdk->id, error))
        break;
    }

  if (*error != NULL)
    return FALSE;

  dex_await (gom_cursor_close (cursor), error);
  return *error == NULL;
}

static const char *manuals_db_path = NULL;
static const char *manuals_keyword_query = NULL;

static gboolean
manuals_print_keyword_matches (GomRepository  *repository,
                               const char     *query_text,
                               GError        **error)
{
  g_autoptr(GomQueryBuilder) builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomCursor) cursor = NULL;
  g_autoptr(GomExpression) filter = NULL;

  builder = gom_query_builder_new ();
  gom_query_builder_set_target_entity_type (builder, manuals_keyword_get_type ());
  gom_query_builder_add_projection (builder, gom_field_expression_new ("id"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("name"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("kind"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("uri"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("since"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("stability"));
  gom_query_builder_add_projection (builder, gom_field_expression_new ("deprecated"));

  filter = gom_search_expression_new_for_field ("name",
                                                query_text,
                                                GOM_SEARCH_MODE_PREFIX);
  gom_query_builder_set_filter (builder, filter);

  gom_query_builder_add_ordering (builder, gom_ordering_new (gom_field_expression_new ("fts.name"), GOM_SORT_ASCENDING));
  gom_query_builder_add_ordering (builder, gom_ordering_new (gom_field_expression_new ("name"), GOM_SORT_ASCENDING));

  query = gom_query_builder_build (builder, error);
  if (query == NULL)
    return FALSE;

  cursor = dex_await_object (gom_repository_query (repository, query), error);
  if (cursor == NULL)
    return FALSE;

  g_print ("Keywords matching \"%s\":\n", query_text);

  while (dex_await_boolean (gom_cursor_next (cursor), error))
    {
      g_autoptr(GomEntity) entity = NULL;
      ManualsKeyword *keyword;
      const char *name;
      const char *kind;
      const char *uri;
      const char *since;
      const char *stability;
      const char *deprecated;

      entity = gom_cursor_materialize (cursor, error);
      if (entity == NULL)
        break;

      keyword = (ManualsKeyword *)entity;
      name = manuals_str_or_dash (keyword->name);
      kind = manuals_str_or_dash (keyword->kind);
      uri = manuals_str_or_dash (keyword->uri);
      since = manuals_str_or_dash (keyword->since);
      stability = manuals_str_or_dash (keyword->stability);
      deprecated = manuals_str_or_dash (keyword->deprecated);

      g_print ("  [%" G_GINT64_FORMAT "] %s", keyword->id, name);
      if (kind[0] != '\0' && g_strcmp0 (kind, "-") != 0)
        g_print (" kind=%s", kind);
      if (uri[0] != '\0' && g_strcmp0 (uri, "-") != 0)
        g_print (" uri=%s", uri);
      if (since[0] != '\0' && g_strcmp0 (since, "-") != 0)
        g_print (" since=%s", since);
      if (stability[0] != '\0' && g_strcmp0 (stability, "-") != 0)
        g_print (" stability=%s", stability);
      if (deprecated[0] != '\0' && g_strcmp0 (deprecated, "-") != 0)
        g_print (" deprecated=%s", deprecated);
      g_print ("\n");
    }

  if (*error != NULL)
    return FALSE;

  dex_await (gom_cursor_close (cursor), error);
  return *error == NULL;
}

static void
manuals_run (void)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autofree char *db_uri = NULL;

  db_uri = g_filename_to_uri (manuals_db_path, NULL, &error);
  if (db_uri == NULL)
    {
      g_printerr ("Failed to convert path to URI: %s\n", error->message);
      return;
    }

  driver = gom_driver_open (db_uri, &error);
  if (driver == NULL)
    {
      g_printerr ("Failed to open database: %s\n", error->message);
      return;
    }

  registry = manuals_create_registry ();
  repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver), registry, NULL), &error);
  if (repository == NULL)
    {
      g_printerr ("Failed to create repository: %s\n", error->message);
      return;
    }

  if (manuals_keyword_query != NULL)
    {
      if (!manuals_print_keyword_matches (repository, manuals_keyword_query, &error))
        {
          g_printerr ("Query failed: %s\n", error->message);
          return;
        }
    }
  else if (!manuals_print_sdks (repository, &error))
    {
      g_printerr ("Query failed: %s\n", error->message);
      return;
    }
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *help = NULL;
  GOptionEntry entries[] = {
    { "keyword", 'k', 0, G_OPTION_ARG_STRING, (gpointer)&manuals_keyword_query,
      "Fulltext keyword query (filters on keywords.name)", "KEYWORD" },
    { NULL }
  };

  context = g_option_context_new ("/path/to/manuals.db");
  g_option_context_add_main_entries (context, entries, NULL);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Failed to parse options: %s\n", error->message);
      return 1;
    }

  if (argc != 2)
    {
      help = g_option_context_get_help (context, FALSE, NULL);
      g_printerr ("%s", help);
      return 1;
    }

  manuals_db_path = argv[1];

  _dex_test_runner (manuals_run);

  return 0;
}
