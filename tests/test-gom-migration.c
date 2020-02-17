#include <gom/gom.h>
#include <glib/gstdio.h>

static GMainLoop *gMainLoop;
static char *db_dir_path = NULL;

/* SeriesResource object */

#define SERIES_TYPE_RESOURCE            (series_resource_get_type())
#define SERIES_TYPE_TYPE                (series_type_get_type())
#define SERIES_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SERIES_TYPE_RESOURCE, SeriesResource))
#define SERIES_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  SERIES_TYPE_RESOURCE, SeriesResourceClass))
#define SERIES_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SERIES_TYPE_RESOURCE))
#define SERIES_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  SERIES_TYPE_RESOURCE))
#define SERIES_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  SERIES_TYPE_RESOURCE, SeriesResourceClass))

struct _SeriesResourcePrivate {
  gint64      db_id;
  gchar      *series_name;
  gchar      *series_id;
  gchar      *imdb_id;
  /* FIXME: many other properties excluded */
};

#define SERIES_TABLE_NAME           "series"
#define SERIES_COLUMN_ID            "id"
#define SERIES_COLUMN_SERIES_NAME   "series-name"
#define SERIES_COLUMN_SERIES_ID     "series-id"
#define SERIES_COLUMN_IMDB_ID       "imdb-id"

#define GOM_DB_PREVIOUS_VERSION     3
#define GOM_DB_CURRENT_VERSION      4

typedef struct _SeriesResource        SeriesResource;
typedef struct _SeriesResourceClass   SeriesResourceClass;
typedef struct _SeriesResourcePrivate SeriesResourcePrivate;

struct _SeriesResource
{
   GomResource parent;
   SeriesResourcePrivate *priv;
};

struct _SeriesResourceClass
{
   GomResourceClass parent_class;
};

GType series_resource_get_type (void);

G_DEFINE_TYPE_WITH_PRIVATE (SeriesResource, series_resource, GOM_TYPE_RESOURCE)

enum {
  PROP_SERIES_0,
  PROP_SERIES_DB_ID,
  PROP_SERIES_SERIES_NAME,
  PROP_SERIES_SERIES_ID,
  PROP_SERIES_IMDB_ID,
  LAST_SERIES_PROP
};

static GParamSpec *series_specs[LAST_SERIES_PROP];

static void
series_resource_finalize (GObject *object)
{
  SeriesResourcePrivate *priv = SERIES_RESOURCE(object)->priv;

  g_clear_pointer (&priv->series_name, g_free);
  g_clear_pointer (&priv->series_id, g_free);
  g_clear_pointer (&priv->imdb_id, g_free);

  G_OBJECT_CLASS(series_resource_parent_class)->finalize(object);
}

static void
series_resource_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  SeriesResource *resource = SERIES_RESOURCE(object);

  switch (prop_id) {
  case PROP_SERIES_DB_ID:
    g_value_set_int64 (value, resource->priv->db_id);
    break;
  case PROP_SERIES_SERIES_NAME:
    g_value_set_string (value, resource->priv->series_name);
    break;
  case PROP_SERIES_SERIES_ID:
    g_value_set_string (value, resource->priv->series_id);
    break;
  case PROP_SERIES_IMDB_ID:
    g_value_set_string (value, resource->priv->imdb_id);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
series_resource_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  SeriesResource *resource = SERIES_RESOURCE(object);

  switch (prop_id) {
  case PROP_SERIES_DB_ID:
    resource->priv->db_id = g_value_get_int64 (value);
    break;
  case PROP_SERIES_SERIES_NAME:
    g_clear_pointer (&resource->priv->series_name, g_free);
    resource->priv->series_name = g_value_dup_string (value);
    break;
  case PROP_SERIES_SERIES_ID:
    g_clear_pointer (&resource->priv->series_id, g_free);
    resource->priv->series_id = g_value_dup_string (value);
    break;
  case PROP_SERIES_IMDB_ID:
    g_clear_pointer (&resource->priv->imdb_id, g_free);
    resource->priv->imdb_id = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
series_resource_class_init (SeriesResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = series_resource_finalize;
  object_class->get_property = series_resource_get_property;
  object_class->set_property = series_resource_set_property;

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, SERIES_TABLE_NAME);

  series_specs[PROP_SERIES_DB_ID] = g_param_spec_int64 (SERIES_COLUMN_ID,
                                                        NULL, NULL,
                                                        0, G_MAXINT64,
                                                        0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_DB_ID,
                                   series_specs[PROP_SERIES_DB_ID]);
  gom_resource_class_set_primary_key (resource_class, SERIES_COLUMN_ID);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class),
                                                 SERIES_COLUMN_ID,
                                                 4);

  series_specs[PROP_SERIES_SERIES_NAME] = g_param_spec_string (SERIES_COLUMN_SERIES_NAME,
                                                               NULL, NULL, NULL,
                                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_SERIES_NAME,
                                   series_specs[PROP_SERIES_SERIES_NAME]);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class),
                                                 SERIES_COLUMN_SERIES_NAME,
                                                 4);

  series_specs[PROP_SERIES_SERIES_ID] = g_param_spec_string (SERIES_COLUMN_SERIES_ID,
                                                             NULL, NULL, NULL,
                                                             G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_SERIES_ID,
                                   series_specs[PROP_SERIES_SERIES_ID]);
  gom_resource_class_set_unique (resource_class, SERIES_COLUMN_SERIES_ID);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class),
                                                 SERIES_COLUMN_SERIES_ID,
                                                 4);

  series_specs[PROP_SERIES_IMDB_ID] = g_param_spec_string (SERIES_COLUMN_IMDB_ID,
                                                           NULL, NULL, NULL,
                                                           G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_IMDB_ID,
                                   series_specs[PROP_SERIES_IMDB_ID]);
  gom_resource_class_set_property_new_in_version(GOM_RESOURCE_CLASS(object_class),
                                                 SERIES_COLUMN_IMDB_ID,
                                                 4);
}

static void
series_resource_init (SeriesResource *resource)
{
  resource->priv = series_resource_get_instance_private(resource);
}

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

GType bookmarks_resource_get_type(void);

G_DEFINE_TYPE_WITH_PRIVATE(BookmarksResource, bookmarks_resource, GOM_TYPE_RESOURCE)

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
  resource->priv = bookmarks_resource_get_instance_private(resource);
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

static void
do_migrate (const gchar *uri)
{
  GomAdapter *adapter;
  GList *tables;
  GomRepository *repository;
  BookmarksResource *res;
  GError *error = NULL;

  adapter = gom_adapter_new();
  gom_adapter_open_sync (adapter, uri, &error);
  g_assert_no_error (error);

  repository = gom_repository_new (adapter);
  tables = g_list_prepend (NULL, GINT_TO_POINTER (BOOKMARKS_TYPE_RESOURCE));
  tables = g_list_prepend (tables, GINT_TO_POINTER (SERIES_TYPE_RESOURCE));
  gom_repository_automatic_migrate_sync (repository, GOM_DB_CURRENT_VERSION, tables, &error);
  g_assert_no_error (error);

  res = g_object_new (SERIES_TYPE_RESOURCE,
                      "repository", repository,
                      SERIES_COLUMN_SERIES_NAME, "Gom Adventures",
                      NULL);
  gom_resource_save_sync (GOM_RESOURCE (res), &error);
  g_assert_no_error (error);

  gom_adapter_close_sync (adapter, &error);
  g_assert_no_error (error);

  g_object_unref (res);
  g_object_unref (repository);
  g_object_unref (adapter);
}


static void
create_old_db (const gchar *uri)
{
  GomAdapter *adapter;
  GList *tables;
  GomRepository *repository;
  BookmarksResource *res;
  GError *error = NULL;

  adapter = gom_adapter_new();
  gom_adapter_open_sync (adapter, uri, &error);
  g_assert_no_error (error);

  repository = gom_repository_new (adapter);
  tables = g_list_prepend (NULL, GINT_TO_POINTER (BOOKMARKS_TYPE_RESOURCE));
  gom_repository_automatic_migrate_sync (repository, GOM_DB_PREVIOUS_VERSION, tables, &error);
  g_assert_no_error (error);

  res = g_object_new (BOOKMARKS_TYPE_RESOURCE,
                      "repository", repository,
                      "title","test file",
                      NULL);
  gom_resource_save_sync (GOM_RESOURCE (res), &error);
  g_assert_no_error (error);

  gom_adapter_close_sync (adapter, &error);
  g_assert_no_error (error);

  g_object_unref (res);
  g_object_unref (repository);
  g_object_unref (adapter);
}

/* First we create an 'old' db only with BookmarksResource;
 * Then we will try to migrate to 'current' db with new
 * table: SeriesResource */
static void
migration_new_table (void)
{
  char *uri;

  uri = setup_db();
  create_old_db (uri);
  do_migrate (uri);
  g_free (uri);
}

gint
main (gint   argc,
	  gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomRepository/migration", migration_no_existing_db);
   g_test_add_func("/GomRepository/migration-new-table", migration_new_table);
   gMainLoop = g_main_loop_new(NULL, FALSE);
   return g_test_run();
}
