/* gom-demo-view.c
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

#include "gom-demo-view.h"
#include "gom-demo-view-cell.h"

struct _GomDemoView
{
  GtkBox         parent_instance;
  GomRepository *repository;
  char          *relation;
  GtkColumnView *column_view;
};

G_DEFINE_FINAL_TYPE (GomDemoView, gom_demo_view, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_REPOSITORY,
  PROP_RELATION,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
gom_demo_view_binding_free (gpointer data)
{
  GBinding *binding = data;

  g_binding_unbind (binding);
  g_object_unref (binding);
}

static void
gom_demo_view_closure_notify (gpointer  data,
                              GClosure *closure)
{
  g_free (data);
}

static void
gom_demo_view_cell_setup_cb (GtkSignalListItemFactory *factory,
                             GtkListItem              *list_item,
                             gpointer                  user_data)
{
  const char *field = user_data;
  GtkWidget *cell;

  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));
  g_assert (GTK_IS_LIST_ITEM (list_item));
  g_assert (field != NULL);

  cell = g_object_new (GOM_DEMO_TYPE_VIEW_CELL,
                       "field", field,
                       NULL);
  gtk_list_item_set_child (list_item, cell);
}

static void
gom_demo_view_cell_bind_cb (GtkSignalListItemFactory *factory,
                            GtkListItem              *list_item,
                            gpointer                  user_data)
{
  GtkWidget *cell;
  GBinding *binding;

  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));
  g_assert (GTK_IS_LIST_ITEM (list_item));

  cell = gtk_list_item_get_child (list_item);
  g_assert (GOM_DEMO_IS_VIEW_CELL (cell));

  binding = g_object_bind_property (list_item, "item",
                                    cell, "item",
                                    G_BINDING_SYNC_CREATE);
  g_object_set_data_full (G_OBJECT (list_item),
                          "GomDemoViewCellItemBinding",
                          g_object_ref (binding),
                          gom_demo_view_binding_free);
}

static void
gom_demo_view_cell_unbind_cb (GtkSignalListItemFactory *factory,
                              GtkListItem              *list_item,
                              gpointer                  user_data)
{
  GtkWidget *cell;

  g_assert (GTK_IS_SIGNAL_LIST_ITEM_FACTORY (factory));
  g_assert (GTK_IS_LIST_ITEM (list_item));

  cell = gtk_list_item_get_child (list_item);
  g_assert (cell == NULL || GOM_DEMO_IS_VIEW_CELL (cell));

  g_object_set_data (G_OBJECT (list_item), "GomDemoViewCellItemBinding", NULL);

  if (cell != NULL)
    g_object_set (cell, "item", NULL, NULL);
}

static GtkColumnViewColumn *
gom_demo_view_create_column (GomFieldSchema *field)
{
  g_autoptr(GtkListItemFactory) factory = NULL;
  const char *name;

  g_assert (GOM_IS_FIELD_SCHEMA (field));

  name = gom_schema_get_name (GOM_SCHEMA (field));
  gom_demo_view_cell_get_type ();

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect_data (factory,
                         "setup",
                         G_CALLBACK (gom_demo_view_cell_setup_cb),
                         g_strdup (name),
                         gom_demo_view_closure_notify,
                         0);
  g_signal_connect (factory, "bind", G_CALLBACK (gom_demo_view_cell_bind_cb), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (gom_demo_view_cell_unbind_cb), NULL);

  return gtk_column_view_column_new (name, g_steal_pointer (&factory));
}

static void
gom_demo_view_set_repository (GomDemoView   *self,
                              GomRepository *repository)
{
  g_assert (GOM_DEMO_IS_VIEW (self));
  g_assert (repository == NULL || GOM_IS_REPOSITORY (repository));

  if (g_set_object (&self->repository, repository))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_REPOSITORY]);
}

static void
gom_demo_view_set_relation (GomDemoView *self,
                            const char  *relation)
{
  g_assert (GOM_DEMO_IS_VIEW (self));

  if (g_set_str (&self->relation, relation))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RELATION]);
}

static gboolean
gom_demo_view_load (GomDemoView  *self,
                    GError      **error)
{
  g_autoptr(GomRelationSchema) schema = NULL;
  g_autoptr(GListModel) fields = NULL;
  g_autoptr(GomQueryBuilder) query_builder = NULL;
  g_autoptr(GomQuery) query = NULL;
  g_autoptr(GomRecordListModel) model = NULL;
  g_autoptr(GtkNoSelection) selection = NULL;
  guint n_fields;

  g_assert (GOM_DEMO_IS_VIEW (self));
  g_assert (GOM_IS_REPOSITORY (self->repository));
  g_assert (self->relation != NULL);

  if (!(schema = dex_await_object (gom_repository_describe_relation (self->repository, self->relation), error)))
    return FALSE;

  query_builder = gom_query_builder_new ();
  gom_query_builder_set_target_relation (query_builder, self->relation);
  if (!(query = gom_query_builder_build (query_builder, error)))
    return FALSE;

  if (!(model = dex_await_object (gom_repository_list_records (self->repository, query), error)))
    return FALSE;

  selection = gtk_no_selection_new (G_LIST_MODEL (g_object_ref (model)));
  gtk_column_view_set_model (self->column_view, GTK_SELECTION_MODEL (selection));

  fields = gom_relation_schema_list_fields (schema);
  n_fields = g_list_model_get_n_items (fields);

  for (guint i = 0; i < n_fields; i++)
    {
      g_autoptr(GomFieldSchema) field = NULL;
      GtkColumnViewColumn *column;

      field = g_list_model_get_item (fields, i);
      column = gom_demo_view_create_column (field);
      gtk_column_view_append_column (self->column_view, column);
      g_object_unref (column);
    }

  return TRUE;
}

static void
gom_demo_view_dispose (GObject *object)
{
  GomDemoView *self = GOM_DEMO_VIEW (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), GOM_DEMO_TYPE_VIEW);

  g_clear_object (&self->repository);
  g_clear_pointer (&self->relation, g_free);

  G_OBJECT_CLASS (gom_demo_view_parent_class)->dispose (object);
}

static void
gom_demo_view_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GomDemoView *self = GOM_DEMO_VIEW (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      g_value_set_object (value, self->repository);
      break;

    case PROP_RELATION:
      g_value_set_string (value, self->relation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_demo_view_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GomDemoView *self = GOM_DEMO_VIEW (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      gom_demo_view_set_repository (self, g_value_get_object (value));
      break;

    case PROP_RELATION:
      gom_demo_view_set_relation (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_demo_view_class_init (GomDemoViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gom_demo_view_dispose;
  object_class->get_property = gom_demo_view_get_property;
  object_class->set_property = gom_demo_view_set_property;

  properties[PROP_REPOSITORY] =
    g_param_spec_object ("repository", NULL, NULL,
                         GOM_TYPE_REPOSITORY,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_RELATION] =
    g_param_spec_string ("relation", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/ui/gom-demo.ui");
  gtk_widget_class_bind_template_child (widget_class, GomDemoView, column_view);
}

static void
gom_demo_view_init (GomDemoView *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
gom_demo_view_new (GomRepository  *repository,
                   const char     *relation,
                   GError        **error)
{
  GomDemoView *self;

  g_return_val_if_fail (GOM_IS_REPOSITORY (repository), NULL);
  g_return_val_if_fail (relation != NULL, NULL);

  self = g_object_new (GOM_DEMO_TYPE_VIEW,
                       "repository", repository,
                       "relation", relation,
                       NULL);

  if (!gom_demo_view_load (self, error))
    {
      g_object_unref (self);
      return NULL;
    }

  return GTK_WIDGET (self);
}
