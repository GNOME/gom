/* gom-demo.c
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

#include <gtk/gtk.h>
#include <libgom.h>

#include "gom-demo-view.h"

typedef struct
{
  GtkApplication *application;
  char           *db_path;
} GomDemoApp;

typedef struct
{
  GomDemoApp  *app;
  GtkWindow   *window;
  GtkDropDown *dropdown;
  GtkStack    *stack;
} GomDemoLoad;

static gboolean
gom_demo_selected_item_to_visible_child_name (GBinding     *binding,
                                              const GValue *from_value,
                                              GValue       *to_value,
                                              gpointer      user_data)
{
  GtkStringObject *string_object = g_value_get_object (from_value);

  if (string_object != NULL)
    g_value_set_string (to_value, gtk_string_object_get_string (string_object));
  else
    g_value_set_string (to_value, NULL);

  return TRUE;
}

static void
gom_demo_load_free (gpointer data)
{
  GomDemoLoad *load = data;

  g_clear_object (&load->window);
  g_clear_object (&load->dropdown);
  g_clear_object (&load->stack);
  g_free (load);
}

static DexFuture *
gom_demo_load_fiber (gpointer user_data)
{
  GomDemoLoad *load = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRegistryBuilder) registry_builder = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(GtkStringList) relation_model = NULL;
  g_autofree char *db_uri = NULL;
  g_autofree char **relations = NULL;

  if (!(db_uri = g_filename_to_uri (load->app->db_path, NULL, &error)))
    goto fail;

  if (!(driver = gom_driver_open (db_uri, &error)))
    goto fail;

  registry_builder = gom_registry_builder_new ();
  registry = gom_registry_builder_build (registry_builder);

  if (!(repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver), registry, NULL), &error)))
    goto fail;

  if (!(relations = dex_await_boxed (gom_repository_list_relations (repository), &error)))
    goto fail;

  relation_model = gtk_string_list_new (NULL);

  for (guint i = 0; relations[i] != NULL; i++)
    {
      GtkStackPage *stack_page;
      GtkWidget *page;

      gtk_string_list_append (relation_model, relations[i]);

      page = gom_demo_view_new (repository, relations[i], &error);
      if (page == NULL)
        goto fail;

      stack_page = gtk_stack_add_child (load->stack, page);
      g_object_bind_property (page, "relation",
                              stack_page, "name",
                              G_BINDING_SYNC_CREATE);
      g_object_bind_property (page, "relation",
                              stack_page, "title",
                              G_BINDING_SYNC_CREATE);
    }

  gtk_drop_down_set_model (load->dropdown, G_LIST_MODEL (relation_model));

  if (relations[0] != NULL)
    gtk_drop_down_set_selected (load->dropdown, 0);

  gtk_window_present (load->window);

  return dex_future_new_true ();

fail:
  {
    GtkAlertDialog *dialog;

    dialog = gtk_alert_dialog_new ("%s",
                                   error != NULL ? error->message : "Failed to load database");
    gtk_alert_dialog_show (dialog, load->window);
  }

  return dex_future_new_for_error (g_steal_pointer (&error));
}

static void
gom_demo_activate_cb (GtkApplication *application,
                      gpointer        user_data)
{
  GomDemoApp *app = user_data;
  GomDemoLoad *load;
  GtkWidget *header_bar;

  load = g_new0 (GomDemoLoad, 1);
  load->app = app;

  load->window = g_object_ref (GTK_WINDOW (gtk_application_window_new (application)));
  gtk_window_set_title (load->window, "Gom Demo");
  gtk_window_set_default_size (load->window, 1100, 700);

  load->dropdown = g_object_ref (GTK_DROP_DOWN (gtk_drop_down_new (NULL, NULL)));
  gtk_drop_down_set_enable_search (load->dropdown, TRUE);
  gtk_drop_down_set_search_match_mode (load->dropdown, GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
  gtk_widget_set_hexpand (GTK_WIDGET (load->dropdown), FALSE);

  header_bar = gtk_header_bar_new ();
  gtk_header_bar_set_title_widget (GTK_HEADER_BAR (header_bar), GTK_WIDGET (load->dropdown));
  gtk_window_set_titlebar (load->window, header_bar);

  load->stack = g_object_ref (GTK_STACK (gtk_stack_new ()));
  gtk_widget_set_hexpand (GTK_WIDGET (load->stack), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (load->stack), TRUE);
  gtk_window_set_child (load->window, GTK_WIDGET (load->stack));

  g_object_bind_property_full (load->dropdown, "selected-item",
                               load->stack, "visible-child-name",
                               G_BINDING_SYNC_CREATE,
                               gom_demo_selected_item_to_visible_child_name,
                               NULL,
                               NULL,
                               NULL);

  dex_future_disown (dex_scheduler_spawn (NULL,
                                          8 * 1024 * 1024,
                                          gom_demo_load_fiber,
                                          load,
                                          gom_demo_load_free));
}

static void
gom_demo_app_clear (GomDemoApp *app)
{
  g_clear_pointer (&app->db_path, g_free);
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GtkApplication) application = NULL;
  GomDemoApp app = {0};
  char *application_argv[2] = { NULL, NULL };
  int status;

  if (argc != 2)
    {
      g_printerr ("Usage: %s /path/to/database.sqlite\n", argv[0]);
      return 1;
    }

  dex_init ();

  app.db_path = g_strdup (argv[1]);
  application = gtk_application_new ("org.gnome.libgom.Demo", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (application, "activate", G_CALLBACK (gom_demo_activate_cb), &app);

  application_argv[0] = argv[0];
  status = g_application_run (G_APPLICATION (application), 1, application_argv);
  gom_demo_app_clear (&app);

  return status;
}
