/* Copied from test-gom-update.c */

#include <gom/gom.h>
#include <glib/gstdio.h>

/* Common for both resource objects */

enum {
  PROP_0,
  PROP_ID,
  PROP_START,
  PROP_END,
  LAST_PROP
};

/* ItemResource object */

#define ITEM_TYPE_RESOURCE            (item_resource_get_type())
#define ITEM_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ITEM_TYPE_RESOURCE, ItemResource))
#define ITEM_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  ITEM_TYPE_RESOURCE, ItemResourceClass))
#define ITEM_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ITEM_TYPE_RESOURCE))
#define ITEM_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  ITEM_TYPE_RESOURCE))
#define ITEM_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  ITEM_TYPE_RESOURCE, ItemResourceClass))

typedef struct {
  char *id;
  GDateTime *start;
  GDateTime *end;
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

static GParamSpec *item_specs[LAST_PROP];

static void
item_resource_finalize (GObject *object)
{
  ItemResource *resource = ITEM_RESOURCE(object);
  g_clear_pointer(&resource->priv->id, g_free);
  g_clear_pointer(&resource->priv->start, g_date_time_unref);
  g_clear_pointer(&resource->priv->end, g_date_time_unref);
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
    g_value_set_string(value, resource->priv->id);
    break;
  case PROP_START:
    g_value_set_boxed(value, resource->priv->start);
    break;
  case PROP_END:
    g_value_set_boxed(value, resource->priv->end);
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
    g_clear_pointer(&resource->priv->id, g_free);
    resource->priv->id = g_value_dup_string(value);
    break;
  case PROP_START:
    g_clear_pointer(&resource->priv->start, g_free);
    resource->priv->start = g_date_time_ref (g_value_get_boxed(value));
    break;
  case PROP_END: {
    g_clear_pointer(&resource->priv->end, g_free);

    gpointer boxed = g_value_get_boxed(value);
    if (boxed)
      {
	resource->priv->end = g_date_time_ref (boxed);
      }
    break;
  }
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

  item_specs[PROP_ID] = g_param_spec_string("id",
                                            "ID",
                                            "The ID for the item.",
                                            NULL,
                                            G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_ID,
                                  item_specs[PROP_ID]);
  gom_resource_class_set_primary_key(resource_class, "id");

  item_specs[PROP_START] = g_param_spec_boxed("start",
					      "Start",
					      "The start point of the item.",
					      G_TYPE_DATE_TIME,
					      G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_START,
                                  item_specs[PROP_START]);

  item_specs[PROP_END] = g_param_spec_boxed("end",
					    "End",
					    "The end point for the item.",
					    G_TYPE_DATE_TIME,
					    G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_END,
                                  item_specs[PROP_END]);
}

static void
item_resource_init (ItemResource *resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                               ITEM_TYPE_RESOURCE,
                                               ItemResourcePrivate);
}

static void
update (void)
{
  GomAdapter *adapter;
  GError *error = NULL;
  gboolean ret;
  GomRepository *repository;
  GList *object_types;
  ItemResource *it;
  GTimeVal tv = { 0 };
  gchar *iso8601;
  GDateTime *start, *end;
  GValue value = { 0, };
  GomFilter *filter;

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


  iso8601 = g_time_val_to_iso8601(&tv);
  g_assert_cmpstr(iso8601, ==, "1970-01-01T00:00:00Z");
  g_free (iso8601);

  start = g_date_time_new_from_timeval_utc(&tv);
  g_assert(g_date_time_to_unix(start) == 0);

  it = g_object_new (ITEM_TYPE_RESOURCE,
                     "repository", repository,
                     "id", "My identifier",
                     "start", start,
                     "end", NULL,
                     NULL);
  ret = gom_resource_save_sync(GOM_RESOURCE(it), &error);
  g_assert(ret);
  g_assert_no_error(error);

  g_object_unref(it);
  g_date_time_unref(start);
  

  g_value_init(&value, G_TYPE_STRING);
  g_value_set_string(&value, "My identifier");
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
	       "start", &start,
	       "end", &end,
	       NULL);
  g_object_unref(it);

  g_date_time_to_timeval(start, &tv);
  g_date_time_unref (start);

  iso8601 = g_time_val_to_iso8601(&tv);
  g_assert_cmpstr(iso8601, ==, "1970-01-01T00:00:00Z");
  g_free (iso8601);

  g_assert(end == NULL);
  

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
  g_test_add_func("/GomRepository/datetime", update);
  return g_test_run();
}
