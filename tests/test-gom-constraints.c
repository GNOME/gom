#include <gom/gom.h>
#include <glib/gstdio.h>

/* ItemResource object */

#define ITEM_TYPE_RESOURCE            (item_resource_get_type())
#define ITEM_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ITEM_TYPE_RESOURCE, ItemResource))
#define ITEM_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  ITEM_TYPE_RESOURCE, ItemResourceClass))
#define ITEM_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ITEM_TYPE_RESOURCE))
#define ITEM_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  ITEM_TYPE_RESOURCE))
#define ITEM_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  ITEM_TYPE_RESOURCE, ItemResourceClass))

typedef struct {
  int id;
  char *name;
  char *email;
} ItemResourcePrivate;

typedef struct {
  GomResource parent;
  ItemResourcePrivate *priv;
} ItemResource;

typedef struct {
  GomResourceClass parent_class;
} ItemResourceClass;

G_DEFINE_TYPE(ItemResource, item_resource, GOM_TYPE_RESOURCE)

enum {
  PROP_0,
  PROP_ID,
  PROP_NAME,
  PROP_EMAIL,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

static void
item_resource_finalize (GObject *object)
{
  ItemResource *resource = ITEM_RESOURCE(object);
  g_free(resource->priv->name);
}

static void
item_resource_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ItemResource *resource = ITEM_RESOURCE(object);

  switch (prop_id) {
  case PROP_ID:
    g_value_set_uint(value, resource->priv->id);
    break;
  case PROP_NAME:
    g_value_set_string(value, resource->priv->name);
    break;
  case PROP_EMAIL:
    g_value_set_string(value, resource->priv->email);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
item_resource_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ItemResource *resource = ITEM_RESOURCE(object);

  switch (prop_id) {
  case PROP_ID:
    resource->priv->id = g_value_get_uint(value);
    break;
  case PROP_NAME:
    g_free(resource->priv->name);
    resource->priv->name = g_value_dup_string(value);
    break;
  case PROP_EMAIL:
    g_free(resource->priv->email);
    resource->priv->email = g_value_dup_string(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
item_resource_class_init (ItemResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->get_property = item_resource_get_property;
  object_class->set_property = item_resource_set_property;
  object_class->finalize = item_resource_finalize;
  g_type_class_add_private(object_class, sizeof(ItemResourcePrivate));

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, "items");

  specs[PROP_ID] = g_param_spec_uint("id", "ID", "The ID for the item.",
                                     0, G_MAXUINT, 0, G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_ID, specs[PROP_ID]);
  gom_resource_class_set_primary_key(resource_class, "id");

  specs[PROP_NAME] = g_param_spec_string("name", "Name",
                                         "The name for the item.",
                                         NULL, G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_NAME,
                                  specs[PROP_NAME]);
  gom_resource_class_set_notnull(resource_class, "name");

  specs[PROP_EMAIL] = g_param_spec_string("email", "Email",
                                          "The email for the item.",
                                          NULL, G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_EMAIL,
                                  specs[PROP_EMAIL]);
  gom_resource_class_set_unique(resource_class, "email");
  gom_resource_class_set_notnull(resource_class, "email");
}

static void
item_resource_init (ItemResource *resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                               ITEM_TYPE_RESOURCE,
                                               ItemResourcePrivate);
}

static void
test_unique (void)
{
  GomAdapter *adapter;
  GomRepository *repository;
  GList *object_types;
  ItemResource *item;

  GomFilter *filter;
  GValue value = { 0, };
  guint id;
  gchar *name, *email;

  GError *error = NULL;

  adapter = gom_adapter_new();
  gom_adapter_open_sync(adapter, ":memory:", &error);
  g_assert_no_error(error);

  repository = gom_repository_new(adapter);
  object_types = g_list_prepend(NULL, GINT_TO_POINTER(ITEM_TYPE_RESOURCE));
  gom_repository_automatic_migrate_sync(repository, 1, object_types, &error);
  g_assert_no_error(error);

  /* Insert an item */
  item = g_object_new (ITEM_TYPE_RESOURCE, "repository", repository,
                       "name", "gom", "email", "gom@gom.gom",
                       NULL);
  gom_resource_save_sync(GOM_RESOURCE(item), &error);
  g_assert_no_error(error);
  g_object_get(item, "id", &id, NULL);
  g_object_unref(item);

  /* Fetch it, to make sure it was saved correctly */
  g_value_init(&value, G_TYPE_UINT);
  g_value_set_uint(&value, id);
  filter = gom_filter_new_eq(ITEM_TYPE_RESOURCE, "id", &value);
  g_value_unset(&value);

  item = ITEM_RESOURCE (gom_repository_find_one_sync(repository,
                                                     ITEM_TYPE_RESOURCE,
                                                     filter,
                                                     &error));
  g_assert_no_error(error);
  g_assert(item);
  g_object_unref(filter);

  g_object_get(item, "name", &name, "email", &email, NULL);
  g_assert_cmpstr(name, ==, "gom");
  g_free(name);
  g_assert_cmpstr(email, ==, "gom@gom.gom");
  g_free(email);
  g_object_unref(item);

  /* Now try inserting a new item with the same name (non UNIQUE column) */
  item = g_object_new (ITEM_TYPE_RESOURCE, "repository", repository,
                       "name", "gom", "email", "gom2@gom.gom",
                       NULL);
  gom_resource_save_sync(GOM_RESOURCE(item), &error);
  g_assert_no_error(error);
  g_object_get(item, "id", &id, NULL);
  g_object_unref(item);

  /* Fetch it, to make sure it was saved correctly */
  g_value_init(&value, G_TYPE_UINT);
  g_value_set_uint(&value, id);
  filter = gom_filter_new_eq(ITEM_TYPE_RESOURCE, "id", &value);
  g_value_unset(&value);

  item = ITEM_RESOURCE (gom_repository_find_one_sync(repository,
                                                     ITEM_TYPE_RESOURCE,
                                                     filter,
                                                     &error));
  g_assert_no_error(error);
  g_assert(item);
  g_object_unref(filter);

  g_object_get(item, "name", &name, "email", &email, NULL);
  g_assert_cmpstr(name, ==, "gom");
  g_free(name);
  g_assert_cmpstr(email, ==, "gom2@gom.gom");
  g_free(email);
  g_object_unref(item);

  /* And finally try inserting an item with the same email (UNIQUE column) */
  item = g_object_new (ITEM_TYPE_RESOURCE, "repository", repository,
                       "name", "gom2", "email", "gom2@gom.gom",
                       NULL);
  gom_resource_save_sync(GOM_RESOURCE(item), &error);
  g_assert_error(error, GOM_ERROR, GOM_ERROR_COMMAND_SQLITE);
  g_assert_nonnull (strstr (error->message, "UNIQUE"));
  g_assert_nonnull (strstr (error->message, "items.email"));
  g_object_unref(item);
  g_clear_error(&error);

  gom_adapter_close_sync(adapter, &error);
  g_assert_no_error(error);

  g_object_unref(repository);
  g_object_unref(adapter);
}

static void
test_notnull (void)
{
  GomAdapter *adapter;
  GomRepository *repository;
  GList *object_types;
  ItemResource *item;

  GError *error = NULL;

  adapter = gom_adapter_new();
  gom_adapter_open_sync(adapter, ":memory:", &error);
  g_assert_no_error(error);

  repository = gom_repository_new(adapter);
  object_types = g_list_prepend(NULL, GINT_TO_POINTER(ITEM_TYPE_RESOURCE));
  gom_repository_automatic_migrate_sync(repository, 1, object_types, &error);
  g_assert_no_error(error);

  /* Insert an item without a name */
  item = g_object_new (ITEM_TYPE_RESOURCE, "repository", repository,
                       "email", "gom@gom.gom",
                       NULL);
  gom_resource_save_sync(GOM_RESOURCE(item), &error);
  g_assert_error(error, GOM_ERROR, GOM_ERROR_COMMAND_SQLITE);
  g_assert_true (g_str_match_string ("NOT NULL", error->message, FALSE));
  g_assert_true (g_str_match_string ("items.name", error->message, FALSE));
  g_clear_error(&error);
  g_object_unref(item);

  /* Try inserting an item without an email */
  item = g_object_new (ITEM_TYPE_RESOURCE, "repository", repository,
                       "name", "gom",
                       NULL);
  gom_resource_save_sync(GOM_RESOURCE(item), &error);
  g_assert_error(error, GOM_ERROR, GOM_ERROR_COMMAND_SQLITE);
  g_assert_true (g_str_match_string ("NOT NULL", error->message, FALSE));
  g_assert_true (g_str_match_string ("items.email", error->message, FALSE));
  g_clear_error(&error);
  g_object_unref(item);

  gom_adapter_close_sync(adapter, &error);
  g_assert_no_error(error);

  g_object_unref(repository);
  g_object_unref(adapter);
}

gint
main (int argc, char **argv)
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomResource/unique", test_unique);
   g_test_add_func("/GomResource/not-null", test_notnull);
   return g_test_run();
}
