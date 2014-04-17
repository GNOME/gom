#include <gom/gom.h>
#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* ItemResource object */

#define ITEM_TYPE_RESOURCE            (item_resource_get_type())
#define ITEM_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), ITEM_TYPE_RESOURCE, ItemResource))
#define ITEM_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  ITEM_TYPE_RESOURCE, ItemResourceClass))
#define ITEM_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ITEM_TYPE_RESOURCE))
#define ITEM_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  ITEM_TYPE_RESOURCE))
#define ITEM_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  ITEM_TYPE_RESOURCE, ItemResourceClass))

typedef struct {
  int id;
  GdkPixbuf *pixbuf;
  char **strv;
  GDateTime *datetime;
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
  PROP_PIXBUF,
  PROP_STRV,
  PROP_DATE_TIME,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

static void
item_resource_finalize (GObject *object)
{
  ItemResource *resource = ITEM_RESOURCE(object);
  g_clear_object(&resource->priv->pixbuf);
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
  case PROP_PIXBUF:
    g_value_set_object(value, resource->priv->pixbuf);
    break;
  case PROP_STRV:
    g_value_set_boxed(value, resource->priv->strv);
    break;
  case PROP_DATE_TIME:
    g_value_set_boxed(value, resource->priv->datetime);
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
  case PROP_PIXBUF:
    g_clear_object(&resource->priv->pixbuf);
    resource->priv->pixbuf = g_value_dup_object(value);
    break;
  case PROP_STRV:
    g_clear_pointer(&resource->priv->strv, g_strfreev);
    resource->priv->strv = g_strdupv((gchar**)g_value_get_boxed (value));
    break;
  case PROP_DATE_TIME:
    g_clear_pointer(&resource->priv->datetime, g_date_time_unref);
    resource->priv->datetime = g_date_time_ref(g_value_get_boxed (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static GBytes *
pixbuf_to_bytes (GValue *value)
{
   GdkPixbuf *pixbuf;
   guchar *pixels;
   guint len;

   pixbuf = g_value_get_object(value);
   g_assert(pixbuf);
   pixels = gdk_pixbuf_get_pixels_with_length (pixbuf, &len);

   return g_bytes_new_static(pixels, len);
}

static void
bytes_unref(guchar   *pixels,
	    gpointer  data)
{
   GBytes *bytes = data;
   g_bytes_unref(bytes);
}

static void
pixbuf_from_bytes (GBytes *bytes, GValue *value)
{
   GdkPixbuf *pixbuf;

   g_assert(bytes);

   /* NOTE: This is a hack, don't use that in a real application */
#if 1
   pixbuf = gdk_pixbuf_new_from_data(g_bytes_get_data(bytes, NULL),
                                     GDK_COLORSPACE_RGB,
                                     TRUE,
                                     8,
                                     1011, 1243, 4044,
                                     bytes_unref,
                                     g_bytes_ref(bytes));
#else
   pixbuf = gdk_pixbuf_new_from_file(IMAGE, NULL);
#endif
   g_value_init(value, GDK_TYPE_PIXBUF);
   g_value_take_object(value, pixbuf);
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
  gom_resource_class_set_primary_key(resource_class, "id");

  specs[PROP_ID] = g_param_spec_uint("id",
                                     "ID",
                                     "The ID for the item.",
                                     0, G_MAXUINT, 0,
                                     G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_ID,
                                  specs[PROP_ID]);

  specs[PROP_PIXBUF] = g_param_spec_object("pixbuf",
                                           "Pixbuf",
                                           "The Pixbuf for the item.",
                                           GDK_TYPE_PIXBUF,
                                           G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_PIXBUF,
                                  specs[PROP_PIXBUF]);
  gom_resource_class_set_property_transform(resource_class, "pixbuf",
                                            pixbuf_to_bytes, pixbuf_from_bytes);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class), "pixbuf", 1);
  specs[PROP_STRV] = g_param_spec_boxed("strv",
                                        "Strv",
                                        "The Strv for the item.",
                                        G_TYPE_STRV,
                                        G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_STRV,
                                  specs[PROP_STRV]);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class), "strv", 1);
  specs[PROP_DATE_TIME] = g_param_spec_boxed("date_time",
                                             "DateTime",
                                             "The DateTime for the item.",
                                             G_TYPE_DATE_TIME,
                                             G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_DATE_TIME,
                                  specs[PROP_DATE_TIME]);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class), "date_time", 1);
}

static void
item_resource_init (ItemResource *resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                               ITEM_TYPE_RESOURCE,
                                               ItemResourcePrivate);
}

static gboolean
pixbuf_compare(GdkPixbuf *a,
               GdkPixbuf *b)
{
  int width, height;
  int rowstride, n_channels;
  guchar *pixels_a, *pixels_b;
  int x, y;

  g_assert(a);
  g_assert(b);

  width = gdk_pixbuf_get_width(a);
  g_assert(width == gdk_pixbuf_get_width(b));

  height = gdk_pixbuf_get_height(a);
  g_assert(height == gdk_pixbuf_get_height(b));

  rowstride = gdk_pixbuf_get_rowstride(a);
  g_assert(rowstride == gdk_pixbuf_get_rowstride(b));

  n_channels = gdk_pixbuf_get_n_channels(a);
  g_assert(n_channels == gdk_pixbuf_get_n_channels(b));
  g_assert(n_channels == 4);

  pixels_a = gdk_pixbuf_get_pixels(a);
  pixels_b = gdk_pixbuf_get_pixels(b);

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      guint offset = y * rowstride + x * n_channels;
      guchar *pa, *pb;

      pa = pixels_a + offset;
      pb = pixels_b + offset;

      g_assert(pa[0] == pb[0]);
      g_assert(pa[1] == pb[1]);
      g_assert(pa[2] == pb[2]);
      g_assert(pa[3] == pb[3]);
    }
  }

  return TRUE;
}

static void
transform (void)
{
   GomAdapter *adapter;
   GError *error = NULL;
   gboolean ret;
   GomRepository *repository;
   GList *object_types;
   ItemResource *item;
   guint id;
   GdkPixbuf *pixbuf, *pixbuf2;
   GValue value = { 0, };
   GomFilter *filter;
   GDateTime *datetime;
   char **strv;
   const char *strv_input[] = { "Foo", "Bar", "Baz", NULL };
   GTimeZone *tz;

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

   pixbuf = gdk_pixbuf_new_from_file(IMAGE, &error);
   g_assert_no_error(error);
   g_assert(pixbuf);

   tz = g_time_zone_new_utc();
   datetime = g_date_time_new(tz, 2014, 4, 17, 9, 9, 0);
   g_time_zone_unref(tz);
   item = g_object_new (ITEM_TYPE_RESOURCE,
                        "pixbuf", pixbuf,
                        "repository", repository,
                        "date-time", datetime,
                        "strv", strv_input,
                        NULL);
   g_object_unref(pixbuf);
   g_date_time_unref(datetime);

   ret = gom_resource_save_sync(GOM_RESOURCE(item), &error);
   g_assert_no_error(error);
   g_assert(ret);

   g_object_get(item, "id", &id, NULL);
   g_object_unref(item);

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

   g_object_get(item,
                "pixbuf", &pixbuf,
                "date-time", &datetime,
                "strv", &strv,
                NULL);
   g_object_unref(item);

   pixbuf2 = gdk_pixbuf_new_from_file(IMAGE, &error);
   ret = pixbuf_compare(pixbuf, pixbuf2);
   g_assert(ret);
   g_object_unref(pixbuf);
   g_object_unref(pixbuf2);

   g_assert_cmpstr(strv[0], ==, strv_input[0]);
   g_assert_cmpstr(strv[1], ==, strv_input[1]);
   g_assert_cmpstr(strv[2], ==, strv_input[2]);
   g_strfreev(strv);

   g_assert_cmpint(g_date_time_get_year(datetime), ==, 2014);
   g_assert_cmpint(g_date_time_get_hour(datetime), ==, 9);
   g_date_time_unref(datetime);

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
   g_test_add_func("/GomRepository/transform", transform);
   return g_test_run();
}
