#include <gtk/gtk.h>
#include <gom/gom.h>
#include "mock/mock-occupation.h"
#include "mock/mock-person.h"

#define TEST_DB ".test-gom.db"

gint
main (gint   argc,
      gchar *argv[])
{
	GtkTreeViewColumn *column;
	GomResourceClass *klass;
	GtkCellRenderer *cell;
	GomPropertySet *fields;
	GtkTreeModel *model;
	GomProperty *occupation;
	GomAdapter *adapter;
	GtkWidget *scroller;
	GtkWidget *treeview;
	GtkWidget *window;
	GomQuery *query;
	GError *error = NULL;

	gtk_init(&argc, &argv);

	adapter = g_object_new(GOM_TYPE_ADAPTER_SQLITE, NULL);
	if (!gom_adapter_sqlite_load_from_file(GOM_ADAPTER_SQLITE(adapter), TEST_DB, &error)) {
		g_message("Error loading database: %s", error->message);
		g_error_free(error);
		g_object_unref(adapter);
		return -1;
	}

	klass = g_type_class_ref(MOCK_TYPE_PERSON);
	fields = gom_property_set_newv(0, NULL);
	gom_property_set_add(fields, gom_property_set_find(klass->properties, "name"));
	gom_property_set_add(fields, gom_property_set_find(klass->properties, "gender"));
	occupation = gom_property_set_find(klass->properties, "occupation");

	klass = g_type_class_ref(MOCK_TYPE_OCCUPATION);
	gom_property_set_add(fields, gom_property_set_find(klass->properties, "name"));

	query = g_object_new(GOM_TYPE_QUERY,
	                     "resource-type", MOCK_TYPE_OCCUPATION,
	                     "fields", fields,
	                     "join", occupation,
	                     NULL);

	window = g_object_new(GTK_TYPE_WINDOW,
	                      "title", "Gom SQLite TreeView Test",
	                      "default-width", 640,
	                      "default-height", 480,
	                      NULL);
	gtk_window_present(GTK_WINDOW(window));

	scroller = g_object_new(GTK_TYPE_SCROLLED_WINDOW,
	                        "visible", TRUE,
	                        NULL);
	gtk_container_add(GTK_CONTAINER(window), scroller);

	model = g_object_new(GOM_TYPE_LIST_STORE,
	                     "adapter", adapter,
	                     "query", query,
	                     NULL);

	treeview = g_object_new(GTK_TYPE_TREE_VIEW,
	                        "fixed-height-mode", TRUE,
	                        "model", model,
	                        "visible", TRUE,
	                        NULL);
	gtk_container_add(GTK_CONTAINER(scroller), treeview);

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(cell), 1);
	column = gtk_tree_view_column_new_with_attributes("Name", cell, "text", 0, NULL);
	g_object_set(column,
	             "min-width", 100,
	             "resizable", TRUE,
	             "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
	             NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(cell), 1);
	column = gtk_tree_view_column_new_with_attributes("Gender", cell, "text", 1, NULL);
	g_object_set(column,
	             "min-width", 100,
	             "resizable", TRUE,
	             "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
	             NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	cell = gtk_cell_renderer_text_new();
	gtk_cell_renderer_text_set_fixed_height_from_font(GTK_CELL_RENDERER_TEXT(cell), 1);
	column = gtk_tree_view_column_new_with_attributes("Occupation", cell, "text", 2, NULL);
	g_object_set(column,
	             "min-width", 100,
	             "resizable", TRUE,
	             "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
	             NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	g_signal_connect(window, "delete-event", gtk_main_quit, NULL);

	return gtk_main(), 0;
}
