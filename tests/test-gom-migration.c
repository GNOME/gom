#include <gom/gom.h>
#include <glib/gstdio.h>

static GMainLoop *gMainLoop;
static char *db_dir_path = NULL;

/* BookmarksResource object */

#define BOOKMARKS_TYPE_RESOURCE            (bookmarks_resource_get_type())
#define BOOKMARKS_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BOOKMARKS_TYPE_RESOURCE, BookmarksResource))
#define BOOKMARKS_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  BOOKMARKS_TYPE_RESOURCE, BookmarksResourceClass))
#define BOOKMARKS_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BOOKMARKS_TYPE_RESOURCE))
#define BOOKMARKS_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BOOKMARKS_TYPE_RESOURCE))
#define BOOKMARKS_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  BOOKMARKS_TYPE_RESOURCE, BookmarksResourceClass))

typedef struct {
  char *id;
  char *url;
  char *title;
  char *thumbnail_url;
  /* FIXME: other properties */
} BookmarksResourcePrivate;

typedef struct {
  GomResource parent;
  BookmarksResourcePrivate *priv;
} BookmarksResource;

typedef struct {
  GomResourceClass parent_class;
} BookmarksResourceClass;

G_DEFINE_TYPE(BookmarksResource, bookmarks_resource, GOM_TYPE_RESOURCE)

enum {
  PROP_0,
  PROP_ID,
  PROP_URL,
  PROP_TITLE,
  PROP_THUMBNAIL_URL,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

static void
bookmarks_resource_finalize (GObject *object)
{
  BookmarksResource *resource = BOOKMARKS_RESOURCE(object);
  g_free (resource->priv->url);
  g_free (resource->priv->thumbnail_url);
  g_free (resource->priv->id);
}

static void
bookmarks_resource_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  BookmarksResource *resource = BOOKMARKS_RESOURCE(object);

  switch (prop_id) {
  case PROP_ID:
    g_value_set_string(value, resource->priv->id);
    break;
  case PROP_URL:
    g_value_set_string(value, resource->priv->url);
    break;
  case PROP_TITLE:
    g_value_set_string(value, resource->priv->title);
    break;
  case PROP_THUMBNAIL_URL:
    g_value_set_string(value, resource->priv->thumbnail_url);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
bookmarks_resource_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  BookmarksResource *resource = BOOKMARKS_RESOURCE(object);

  switch (prop_id) {
  case PROP_ID:
    g_free (resource->priv->id);
    resource->priv->id = g_value_dup_string(value);
    break;
  case PROP_URL:
    g_free (resource->priv->url);
    resource->priv->url = g_value_dup_string(value);
    break;
  case PROP_TITLE:
    g_free (resource->priv->title);
    resource->priv->title = g_value_dup_string(value);
    break;
  case PROP_THUMBNAIL_URL:
    g_free (resource->priv->thumbnail_url);
    resource->priv->thumbnail_url = g_value_dup_string(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
bookmarks_resource_class_init (BookmarksResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = bookmarks_resource_finalize;
  object_class->get_property = bookmarks_resource_get_property;
  object_class->set_property = bookmarks_resource_set_property;
  g_type_class_add_private(object_class, sizeof(BookmarksResourcePrivate));

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, "bookmarks");

  specs[PROP_ID] = g_param_spec_string("id",
                                        "ID",
                                        "The ID for the bookmark.",
                                        NULL,
                                        G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_ID,
                                  specs[PROP_ID]);
  gom_resource_class_set_primary_key(resource_class, "id");

  specs[PROP_URL] = g_param_spec_string("url",
                                        "URL",
                                        "The URL for the bookmark.",
                                        NULL,
                                        G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_URL,
                                  specs[PROP_URL]);

  specs[PROP_TITLE] = g_param_spec_string("title",
                                          "Title",
                                          "The title for the bookmark.",
                                          NULL,
                                          G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_TITLE,
                                  specs[PROP_TITLE]);
  specs[PROP_THUMBNAIL_URL] = g_param_spec_string("thumbnail-url",
                                                  "Thumbnail URL",
                                                  "The thumbnail URL for the bookmark.",
                                                  NULL,
                                                  G_PARAM_READWRITE);
  g_object_class_install_property(object_class, PROP_THUMBNAIL_URL,
                                  specs[PROP_THUMBNAIL_URL]);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class),
                                                 "thumbnail-url",
                                                 2);
}

static void
bookmarks_resource_init (BookmarksResource *resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                               BOOKMARKS_TYPE_RESOURCE,
                                               BookmarksResourcePrivate);
}

#if 0
static gboolean
do_migrate (GomRepository  *repository,
            GomAdapter     *adapter,
            guint           version,
            gpointer        user_data,
            GError        **error)
{
#define EXEC_OR_FAIL(sql) \
   G_STMT_START { \
      GomCommand *c = g_object_new(GOM_TYPE_COMMAND, \
                                   "adapter", adapter, \
                                   "sql", (sql), \
                                   NULL); \
      if (!gom_command_execute(c, NULL, error)) { \
         g_object_unref(c); \
         goto failure; \
      } \
      g_object_unref(c); \
   } G_STMT_END

   if (version == 1) {
      EXEC_OR_FAIL("CREATE TABLE IF NOT EXISTS bookmarks ("
                   "id     INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "parent INTEGER REFERENCES bookmarks (id),"
                   "type   INTEGER,"
                   "url    TEXT,"
                   "title  TEXT,"
                   "date   TEXT,"
                   "mime   TEXT,"
                   "desc   TEXT"
                   ");");
      return TRUE;
   }

   if (version == 2) {
     EXEC_OR_FAIL("ALTER TABLE bookmarks ADD COLUMN thumbnail_url TEXT;");
     return TRUE;
   }

failure:
   return FALSE;
}
#endif

static void
close_cb (GObject      *object,
	  GAsyncResult *result,
	  gpointer      user_data)
{
   GomAdapter *adapter = (GomAdapter *)object;
   gboolean *success = user_data;
   gboolean ret;
   GError *error = NULL;
   GFile *file;

   ret = gom_adapter_close_finish(adapter, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   file = g_file_new_for_path(db_dir_path);
   g_file_trash(file, NULL, NULL);
   g_object_unref(file);

   g_clear_pointer(&db_dir_path, g_free);

   *success = TRUE;
   g_main_loop_quit(gMainLoop);
}

static void
migrate_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
   GomRepository *repository = (GomRepository *)object;
   GomAdapter *adapter;
   gboolean ret;
   GError *error = NULL;

   ret = gom_repository_migrate_finish(repository, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   adapter = gom_repository_get_adapter(repository);
   gom_adapter_close_async(adapter, close_cb, user_data);
}

static void
open_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
   GomRepository *repository;
   GomAdapter *adapter = (GomAdapter *)object;
   gboolean ret;
   GError *error = NULL;
   GList *object_types;

   ret = gom_adapter_open_finish(adapter, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   repository = gom_repository_new(adapter);

   /* This allows switching between automatic and manual DB upgrades */
#if 0
   gom_repository_migrate_async(repository, 2, do_migrate, NULL, migrate_cb, user_data);
#else
   object_types = g_list_prepend(NULL, GINT_TO_POINTER(BOOKMARKS_TYPE_RESOURCE));
   gom_repository_automatic_migrate_async(repository, 2, object_types, migrate_cb, user_data);
#endif
   g_object_unref(repository);
}

static char *
setup_db(void)
{
  GFile *dest;
  char *url, *path;
  GError *error = NULL;

  db_dir_path = g_dir_make_tmp("test-gom-find-XXXXXXX", &error);
  g_assert(db_dir_path);
  g_assert_no_error(error);

  path = g_build_filename(db_dir_path, "gom-db-test.db", NULL);
  dest = g_file_new_for_path (path);
  g_free (path);

  url = g_file_get_uri(dest);
  g_object_unref (dest);

  return url;
}

static void
migration_no_existing_db (void)
{
   GomAdapter *adapter;
   gboolean success = FALSE;
   char *uri;

   uri = setup_db();

   adapter = gom_adapter_new();
   gom_adapter_open_async(adapter, uri, open_cb, &success);
   g_free (uri);
   g_object_unref(adapter);

   g_main_loop_run(gMainLoop);

   g_assert(success);
}

gint
main (gint   argc,
	  gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomRepository/migration", migration_no_existing_db);
   gMainLoop = g_main_loop_new(NULL, FALSE);
   return g_test_run();
}
