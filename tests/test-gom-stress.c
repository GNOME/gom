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
  char *first_name;
  char *surname;
} ItemResourcePrivate;

typedef struct {
  GomResource parent;
  ItemResourcePrivate *priv;
} ItemResource;

typedef struct {
  GomResourceClass parent_class;
} ItemResourceClass;

GType item_resource_get_type(void);

G_DEFINE_TYPE(ItemResource, item_resource, GOM_TYPE_RESOURCE)

enum {
  PROP_0,
  PROP_ID,
  PROP_FIRST_NAME,
  PROP_SURNAME,
  LAST_PROP
};

#define NUM_RECORDS 1000
#define ITEM_TO_GET (NUM_RECORDS/2)

static GParamSpec *specs[LAST_PROP];

static void
item_resource_finalize (GObject *object)
{
  ItemResource *resource = ITEM_RESOURCE(object);
  g_clear_pointer(&resource->priv->first_name, g_free);
  g_clear_pointer(&resource->priv->surname, g_free);
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
  case PROP_FIRST_NAME:
    g_value_set_string(value, resource->priv->first_name);
    break;
  case PROP_SURNAME:
    g_value_set_string(value, resource->priv->surname);
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
  case PROP_FIRST_NAME:
    g_clear_pointer(&resource->priv->first_name, g_free);
    resource->priv->first_name = g_value_dup_string(value);
    break;
  case PROP_SURNAME:
    g_clear_pointer(&resource->priv->surname, g_free);
    resource->priv->surname = g_value_dup_string(value);
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
  object_class->finalize = item_resource_finalize;
  object_class->get_property = item_resource_get_property;
  object_class->set_property = item_resource_set_property;
  g_type_class_add_private(object_class, sizeof(ItemResourcePrivate));

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, "items");

  specs[PROP_ID] = g_param_spec_uint("id",
                                     "ID",
                                     "The ID for the item.",
                                     0, G_MAXUINT, 0,
                                     G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_ID,
                                  specs[PROP_ID]);
  gom_resource_class_set_primary_key(resource_class, "id");

  specs[PROP_FIRST_NAME] = g_param_spec_string("first-name",
                                               "First name",
                                               "The First name for the item.",
                                               NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_FIRST_NAME,
                                  specs[PROP_FIRST_NAME]);

  specs[PROP_SURNAME] = g_param_spec_string("surname",
                                            "Surname",
                                            "The Surname for the item.",
                                            NULL,
                                            G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_SURNAME,
                                  specs[PROP_SURNAME]);
}

static void
item_resource_init (ItemResource *resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                               ITEM_TYPE_RESOURCE,
                                               ItemResourcePrivate);
}

static void
stress (void)
{
   GomAdapter *adapter;
   GError *error = NULL;
   gboolean ret;
   GomRepository *repository;
   GList *object_types;
   GValue value = { 0, };
   GomFilter *filter;
   guint i;
   char *s1, *s2;
   ItemResource *it;

   adapter = gom_adapter_new();
   //ret = gom_adapter_open_sync(adapter, "file:test.db", &error);
   ret = gom_adapter_open_sync(adapter, ":memory:", &error);
   g_assert_no_error(error);
   g_assert(ret);

   repository = gom_repository_new(adapter);

   object_types = g_list_prepend(NULL, GINT_TO_POINTER(ITEM_TYPE_RESOURCE));
   ret = gom_repository_automatic_migrate_sync(repository, 1, object_types, &error);
   g_assert_no_error(error);
   g_assert(ret);

   for (i = 1; i <= NUM_RECORDS; i++) {
      ItemResource *item;
      char *first_name, *surname;

      first_name = g_strdup_printf("%s #%d", "First name", i);
      surname = g_strdup_printf("%s #%d", "Surname", i);

      item = g_object_new (ITEM_TYPE_RESOURCE,
                           "repository", repository,
                           "first-name", first_name,
                           "surname", surname,
                           NULL);
      gom_resource_save_sync(GOM_RESOURCE(item), NULL);
      g_object_unref(item);
   }

   g_value_init(&value, G_TYPE_UINT);
   g_value_set_uint(&value, ITEM_TO_GET);
   filter = gom_filter_new_eq(ITEM_TYPE_RESOURCE, "id", &value);
   g_value_unset(&value);

   it = ITEM_RESOURCE (gom_repository_find_one_sync(repository,
                                                    ITEM_TYPE_RESOURCE,
                                                    filter,
                                                    &error));
   g_assert_no_error(error);
   g_assert(it);
   g_object_unref(filter);

   g_object_get(it,
                "first-name", &s1,
                "surname", &s2,
                NULL);
   g_object_unref(it);

   g_assert_cmpstr(s1, ==, "First name #500");
   g_assert_cmpstr(s2, ==, "Surname #500");
   g_free(s1);
   g_free(s2);

   ret = gom_adapter_close_sync(adapter, &error);
   g_assert_no_error(error);
   g_assert(ret);

   g_object_unref(repository);
   g_object_unref(adapter);
}

gint
main (int argc, char **argv)
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomRepository/stress", stress);
   return g_test_run();
}
