#include <gtk/gtk.h>
#include <gom/gom.h>
#include "mock/mock-occupation.h"
#include "mock/mock-person.h"

#define TEST_DB ".test-gom.db"

gint
main (gint   argc,
      gchar *argv[])
{
	GtkWidget *scroller;
	GtkWidget *treeview;
	GtkWidget *window;
	GtkTreeViewColumn *column;
	GomPropertySet *fields;
	GomProperty *occupation;
	GomResourceClass *klass;
	GomAdapter *adapter;
	GomQuery *query;
	GError *error = NULL;
	GtkTreeModel *model;

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
	                        "model", model,
	                        "visible", TRUE,
	                        NULL);
	gtk_container_add(GTK_CONTAINER(scroller), treeview);

	column = gtk_tree_view_column_new_with_attributes("Name",
	                                                  gtk_cell_renderer_text_new(),
	                                                  "text", 0,
	                                                  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	column = gtk_tree_view_column_new_with_attributes("Gender",
	                                                  gtk_cell_renderer_text_new(),
	                                                  "text", 1,
	                                                  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	column = gtk_tree_view_column_new_with_attributes("Occupation",
	                                                  gtk_cell_renderer_text_new(),
	                                                  "text", 2,
	                                                  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

	return gtk_main(), 0;
}
