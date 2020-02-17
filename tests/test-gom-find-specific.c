#include <gom/gom.h>
#include <glib/gstdio.h>

static GMainLoop *gMainLoop;

/* EpisodeResource object */

#define EPISODE_TYPE_RESOURCE              (episode_resource_get_type())
#define EPISODE_TYPE_TYPE                  (episode_type_get_type())
#define EPISODE_RESOURCE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPISODE_TYPE_RESOURCE, EpisodeResource))
#define EPISODE_RESOURCE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EPISODE_TYPE_RESOURCE, EpisodeResourceClass))
#define EPISODE_IS_RESOURCE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPISODE_TYPE_RESOURCE))
#define EPISODE_IS_RESOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EPISODE_TYPE_RESOURCE))
#define EPISODE_RESOURCE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), EPISODE_TYPE_RESOURCE, EpisodeResourceClass))

#define EPISODE_COLUMN_ID                 "id"
#define EPISODE_COLUMN_SERIES_ID          "series-id"
#define EPISODE_COLUMN_IMDB_ID            "imdb-id"
#define EPISODE_COLUMN_SEASON_NUMBER      "season-number"
#define EPISODE_COLUMN_EPISODE_NUMBER     "episode-number"
#define EPISODE_COLUMN_EPISODE_NAME       "episode-name"

typedef struct {
  gint64      db_id;
  gchar      *series_id;
  gchar      *imdb_id;
  guint8      season_number;
  guint8      episode_number;
  gchar      *episode_name;
} EpisodeResourcePrivate;

typedef struct
{
   GomResource parent;
   EpisodeResourcePrivate *priv;
} EpisodeResource;

typedef struct
{
   GomResourceClass parent_class;
} EpisodeResourceClass;

GType episode_resource_get_type(void);

G_DEFINE_TYPE_WITH_PRIVATE(EpisodeResource, episode_resource, GOM_TYPE_RESOURCE)

enum {
  PROP_0,
  PROP_DB_ID,
  PROP_SERIES_ID,
  PROP_IMDB_ID,
  PROP_SEASON_NUMBER,
  PROP_EPISODE_NUMBER,
  PROP_EPISODE_NAME,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

static struct {
  const gchar *series_id;
  const gchar *imdb_id;
  guint8       season_number;
  guint8       episode_number;
  const gchar *episode_name;
} values[] = {
  { "84947", "tt2483070", 4, 1, "New York Sour" },
  { "84947", "tt2778300", 4, 2, "Resignation" },
  { "84947", "tt3767076", 5, 2, "The Good Listener" },
  { "84947", "tt3767078", 5, 3, NULL }
};

static void
episode_resource_finalize (GObject *object)
{
  EpisodeResourcePrivate *priv = EPISODE_RESOURCE(object)->priv;

  g_clear_pointer (&priv->series_id, g_free);
  g_clear_pointer (&priv->imdb_id, g_free);
  g_clear_pointer (&priv->episode_name, g_free);

  G_OBJECT_CLASS(episode_resource_parent_class)->finalize(object);
}

static void
episode_resource_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EpisodeResource *resource = EPISODE_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    g_value_set_int64 (value, resource->priv->db_id);
    break;
  case PROP_SERIES_ID:
    g_value_set_string (value, resource->priv->series_id);
    break;
  case PROP_IMDB_ID:
    g_value_set_string (value, resource->priv->imdb_id);
    break;
  case PROP_SEASON_NUMBER:
    g_value_set_uchar (value, resource->priv->season_number);
    break;
  case PROP_EPISODE_NUMBER:
    g_value_set_uchar (value, resource->priv->episode_number);
    break;
  case PROP_EPISODE_NAME:
    g_value_set_string (value, resource->priv->episode_name);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
episode_resource_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EpisodeResource *resource = EPISODE_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    resource->priv->db_id = g_value_get_int64 (value);
    break;
  case PROP_SERIES_ID:
    g_clear_pointer (&resource->priv->series_id, g_free);
    resource->priv->series_id = g_value_dup_string (value);
    break;
  case PROP_IMDB_ID:
    g_clear_pointer (&resource->priv->imdb_id, g_free);
    resource->priv->imdb_id = g_value_dup_string (value);
    break;
  case PROP_SEASON_NUMBER:
    resource->priv->season_number = g_value_get_uchar (value);
    break;
  case PROP_EPISODE_NUMBER:
    resource->priv->episode_number = g_value_get_uchar (value);
    break;
  case PROP_EPISODE_NAME:
    g_clear_pointer (&resource->priv->episode_name, g_free);
    resource->priv->episode_name = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
episode_resource_class_init (EpisodeResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = episode_resource_finalize;
  object_class->get_property = episode_resource_get_property;
  object_class->set_property = episode_resource_set_property;

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, "episodes");

  specs[PROP_DB_ID] = g_param_spec_int64 (EPISODE_COLUMN_ID,
                                          NULL, NULL,
                                          0, G_MAXINT64,
                                          0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DB_ID,
                                   specs[PROP_DB_ID]);
  gom_resource_class_set_primary_key (resource_class, "id");

  specs[PROP_SERIES_ID] = g_param_spec_string (EPISODE_COLUMN_SERIES_ID,
                                               NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_ID,
                                   specs[PROP_SERIES_ID]);

  specs[PROP_IMDB_ID] = g_param_spec_string (EPISODE_COLUMN_IMDB_ID,
                                             NULL, NULL, NULL,
                                             G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_IMDB_ID,
                                   specs[PROP_IMDB_ID]);

  specs[PROP_SEASON_NUMBER] = g_param_spec_uchar (EPISODE_COLUMN_SEASON_NUMBER,
                                                  NULL, NULL,
                                                  0, G_MAXUINT8,
                                                  0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SEASON_NUMBER,
                                   specs[PROP_SEASON_NUMBER]);

  specs[PROP_EPISODE_NUMBER] = g_param_spec_uchar (EPISODE_COLUMN_EPISODE_NUMBER,
                                                   NULL, NULL,
                                                   0, G_MAXUINT8,
                                                   0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_EPISODE_NUMBER,
                                   specs[PROP_EPISODE_NUMBER]);

  specs[PROP_EPISODE_NAME] = g_param_spec_string (EPISODE_COLUMN_EPISODE_NAME,
                                                  NULL, NULL, NULL,
                                                  G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_EPISODE_NAME,
                                   specs[PROP_EPISODE_NAME]);

}

static void
episode_resource_init (EpisodeResource *resource)
{
  resource->priv = episode_resource_get_instance_private(resource);
}

static void
create_memory_db (GomAdapter **adapter,
                  GomRepository **repository)
{
  gboolean ret;
  GError *error = NULL;
  GList *object_types;
  EpisodeResource *eres;
  guint i;

  *adapter = gom_adapter_new();
  ret = gom_adapter_open_sync (*adapter, ":memory:", &error);
  //ret = gom_adapter_open_sync (*adapter, "/tmp/test_gom_find.db", &error);
  g_assert_no_error (error);
  g_assert (ret);

  *repository = gom_repository_new (*adapter);

  object_types = g_list_prepend (NULL, GINT_TO_POINTER (EPISODE_TYPE_RESOURCE));
  ret = gom_repository_automatic_migrate_sync (*repository, 1, object_types, &error);
  g_assert_no_error (error);
  g_assert (ret);

  for (i = 0; i < G_N_ELEMENTS(values); i++) {
    eres = g_object_new (EPISODE_TYPE_RESOURCE,
                         "repository", *repository,
                         EPISODE_COLUMN_SERIES_ID, values[i].series_id,
                         EPISODE_COLUMN_IMDB_ID, values[i].imdb_id,
                         EPISODE_COLUMN_SEASON_NUMBER, values[i].season_number,
                         EPISODE_COLUMN_EPISODE_NUMBER, values[i].episode_number,
                         EPISODE_COLUMN_EPISODE_NAME, values[i].episode_name,
                         NULL);
    ret = gom_resource_save_sync (GOM_RESOURCE (eres), &error);
    g_assert (ret);
    g_assert_no_error (error);
    g_object_unref (eres);
  }
}

static void
free_memory_db (GomAdapter *adapter,
                GomRepository *repository)
{
  gboolean ret;
  GError *error = NULL;

  ret = gom_adapter_close_sync (adapter, &error);
  g_assert_no_error (error);
  g_assert (ret);

  g_object_unref (repository);
  g_object_unref (adapter);
}

/* Try to find values[0] from values[0].series_id */
static void
find_simple (void)
{
  GError *error = NULL;
  GValue value = { 0, };
  GomFilter *filter;
  char *s1, *s2;
  GomResource *resource;
  EpisodeResource *eres;
  GomAdapter *adapter;
  GomRepository *repository;

  create_memory_db (&adapter, &repository);

  /* Series ID */
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, values[0].series_id);
  filter = gom_filter_new_like (EPISODE_TYPE_RESOURCE,
                                EPISODE_COLUMN_SERIES_ID,
                                &value);
  g_value_unset (&value);

  resource = gom_repository_find_one_sync (repository,
                                           EPISODE_TYPE_RESOURCE,
                                           filter,
                                           &error);
  g_assert_no_error (error);
  g_assert (resource);
  g_object_unref (filter);
  eres = EPISODE_RESOURCE (resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_IMDB_ID, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr (s1, ==, values[0].series_id);
  g_assert_cmpstr (s2, ==, values[0].imdb_id);
  g_free (s1);
  g_free (s2);

  free_memory_db (adapter, repository);
}

/* Try to find values[0] from:
 * values[0].series_id
 * AND values[0].season_number
 * AND values[0].episode_number */
static void
find_specific (void)
{
  GError *error = NULL;
  GValue value = { 0, };
  GomFilter *filter1, *filter2, *filter3, *filter4, *filter5;
  char *s1, *s2;
  GomResource *resource;
  EpisodeResource *eres;
  GomAdapter *adapter;
  GomRepository *repository;

  create_memory_db (&adapter, &repository);

  /* Season Number */
  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, values[0].season_number);
  filter1 = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_SEASON_NUMBER,
                               &value);
  g_value_unset (&value);

  /* Episode Number */
  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, values[0].episode_number);
  filter2 = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_EPISODE_NUMBER,
                               &value);
  g_value_unset (&value);

  /* Series ID */
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, values[0].series_id);
  filter3 = gom_filter_new_like (EPISODE_TYPE_RESOURCE,
                                 EPISODE_COLUMN_SERIES_ID,
                                 &value);
  g_value_unset (&value);

  /* Season and Episode Number */
  filter4 = gom_filter_new_and (filter1, filter2);
  g_object_unref (filter1);
  g_object_unref (filter2);

  /* Series ID and Season and Episode Number */
  filter5 = gom_filter_new_and (filter3, filter4);
  g_object_unref (filter3);
  g_object_unref (filter4);

  resource = gom_repository_find_one_sync (repository,
                                           EPISODE_TYPE_RESOURCE,
                                           filter5,
                                           &error);
  g_assert_no_error (error);
  g_assert (resource);
  g_object_unref (filter5);
  eres = EPISODE_RESOURCE (resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_IMDB_ID, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr (s1, ==, values[0].series_id);
  g_assert_cmpstr (s2, ==, values[0].imdb_id);
  g_free (s1);
  g_free (s2);

  free_memory_db (adapter, repository);
}

/* Same as find_specific, but using the _full filter constructors */
static void
find_specific_and_full (void)
{
  GError *error = NULL;
  GValue value = { 0, };
  GomFilter *filter1, *filter2, *filter3, *filter4;
  char *s1, *s2;
  GomResource *resource;
  EpisodeResource *eres;
  GomAdapter *adapter;
  GomRepository *repository;

  create_memory_db(&adapter, &repository);

  /* Season Number */
  g_value_init(&value, G_TYPE_INT64);
  g_value_set_int64(&value, values[0].season_number);
  filter1 = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_SEASON_NUMBER,
                               &value);
  g_value_unset(&value);

  /* Episode Number */
  g_value_init(&value, G_TYPE_INT64);
  g_value_set_int64(&value, values[0].episode_number);
  filter2 = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                              EPISODE_COLUMN_EPISODE_NUMBER,
                              &value);
  g_value_unset(&value);

  /* Series ID */
  g_value_init(&value, G_TYPE_STRING);
  g_value_set_string(&value, values[0].series_id);
  filter3 = gom_filter_new_like(EPISODE_TYPE_RESOURCE,
                                EPISODE_COLUMN_SERIES_ID,
                                &value);
  g_value_unset(&value);

  /* Season Number and Episode Number and Series ID */
  filter4 = gom_filter_new_and_full(filter1, filter2, filter3, NULL);
  g_object_unref(filter1);
  g_object_unref(filter2);
  g_object_unref(filter3);

  resource = gom_repository_find_one_sync(repository,
                                          EPISODE_TYPE_RESOURCE,
                                          filter4,
                                          &error);
  g_assert_no_error(error);
  g_assert(resource);
  g_object_unref(filter4);
  eres = EPISODE_RESOURCE(resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_IMDB_ID, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr(s1, ==, values[0].series_id);
  g_assert_cmpstr(s2, ==, values[0].imdb_id);
  g_free(s1);
  g_free(s2);

  free_memory_db(adapter, repository);
}

/* Same as find_specific, but using the _fullv filter constructors */
static void
find_specific_and_fullv (void)
{
  GError *error = NULL;
  GValue value = { 0, };
  GomFilter **filter_array = g_new(GomFilter *, 4);
  GomFilter *filter;
  char *s1, *s2;
  GomResource *resource;
  EpisodeResource *eres;
  GomAdapter *adapter;
  GomRepository *repository;

  create_memory_db(&adapter, &repository);

  /* Season Number */
  g_value_init(&value, G_TYPE_INT64);
  g_value_set_int64(&value, values[0].season_number);
  filter_array[0] = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                                      EPISODE_COLUMN_SEASON_NUMBER,
                                      &value);
  g_value_unset(&value);

  /* Episode Number */
  g_value_init(&value, G_TYPE_INT64);
  g_value_set_int64(&value, values[0].episode_number);
  filter_array[1] = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                                      EPISODE_COLUMN_EPISODE_NUMBER,
                                      &value);
  g_value_unset(&value);

  /* Series ID */
  g_value_init(&value, G_TYPE_STRING);
  g_value_set_string(&value, values[0].series_id);
  filter_array[2] = gom_filter_new_like(EPISODE_TYPE_RESOURCE,
                                        EPISODE_COLUMN_SERIES_ID,
                                        &value);
  g_value_unset(&value);

  /* Season Number and Episode Number and Series ID */
  filter_array[3] = NULL;
  filter = gom_filter_new_and_fullv(filter_array);
  g_object_unref(filter_array[0]);
  g_object_unref(filter_array[1]);
  g_object_unref(filter_array[2]);
  g_free(filter_array);

  resource = gom_repository_find_one_sync(repository,
                                          EPISODE_TYPE_RESOURCE,
                                          filter,
                                          &error);
  g_assert_no_error(error);
  g_assert(resource);
  g_object_unref(filter);
  eres = EPISODE_RESOURCE(resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_IMDB_ID, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr(s1, ==, values[0].series_id);
  g_assert_cmpstr(s2, ==, values[0].imdb_id);
  g_free(s1);
  g_free(s2);

  free_memory_db(adapter, repository);
}

/* Ensure we respect filter priorities */
static void
find_specific_ensure_priorities (void)
{
   GomAdapter *adapter;
   GomRepository *repository;
   GomFilter *filter1, *filter2, *filter3, *filter4, *filter5;
   GomResourceGroup *group;
   GValue value = { 0, };
   GError *error = NULL;
   guint count;

   create_memory_db(&adapter, &repository);

   g_value_init(&value, G_TYPE_INT64);
   g_value_set_int64(&value, 4);
   filter1 = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_SEASON_NUMBER, &value);
   g_value_unset(&value);

   g_value_init(&value, G_TYPE_INT64);
   g_value_set_int64(&value, 1);
   filter2 = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_EPISODE_NUMBER, &value);
   g_value_unset(&value);

   g_value_init(&value, G_TYPE_INT64);
   g_value_set_int64(&value, 2);
   filter3 = gom_filter_new_eq(EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_EPISODE_NUMBER, &value);
   g_value_unset(&value);

   /* Search for:
    *   ( season-number=4 AND episode-number=1 ) OR episode-number=2
    */
   filter4 = gom_filter_new_and_full(filter1, filter2, NULL);
   filter5 = gom_filter_new_or_full(filter4, filter3, NULL);

   group = gom_repository_find_sync(repository, EPISODE_TYPE_RESOURCE,
                                    filter5, &error);
   g_assert_no_error(error);

   count = gom_resource_group_get_count(group);
   g_assert_cmpuint(count, ==, 3);

   g_object_unref(filter4);
   g_object_unref(filter5);

   /* Now search for:
    *   season-number=4 AND ( episode-number=1 OR episode-number=2 )
    */
   filter4 = gom_filter_new_or_full(filter2, filter3, NULL);
   filter5 = gom_filter_new_and_full(filter1, filter4, NULL);

   group = gom_repository_find_sync(repository, EPISODE_TYPE_RESOURCE,
                                    filter5, &error);
   g_assert_no_error(error);

   count = gom_resource_group_get_count(group);
   g_assert_cmpuint(count, ==, 2);

   g_object_unref(filter1);
   g_object_unref(filter2);
   g_object_unref(filter3);
   g_object_unref(filter4);
   g_object_unref(filter5);

   free_memory_db(adapter, repository);
}

static void
find_glob (void)
{
  GomAdapter *adapter;
  GomRepository *repository;

  GValue value = { 0, };
  GomFilter *filter;
  GError *error = NULL;

  GomResource *resource;
  EpisodeResource *eres;

  char *s1, *s2;

  create_memory_db (&adapter, &repository);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, "New York *");
  filter = gom_filter_new_glob (EPISODE_TYPE_RESOURCE,
                                EPISODE_COLUMN_EPISODE_NAME,
                                &value);
  g_value_unset (&value);

  resource = gom_repository_find_one_sync (repository,
                                           EPISODE_TYPE_RESOURCE,
                                           filter,
                                           &error);
  g_assert_no_error (error);
  g_assert (resource);
  g_object_unref (filter);
  eres = EPISODE_RESOURCE (resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_EPISODE_NAME, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr (s1, ==, values[0].series_id);
  g_assert_cmpstr (s2, ==, values[0].episode_name);
  g_free (s1);
  g_free (s2);

  free_memory_db (adapter, repository);
}

static void
find_null (void)
{
  GError *error = NULL;
  GValue value = { 0, };
  GomFilter *filter1, *filter2, *filter3;
  char *s1, *s2;
  GomResource *resource;
  EpisodeResource *eres;
  GomAdapter *adapter;
  GomRepository *repository;

  create_memory_db (&adapter, &repository);

  /* Season Number */
  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, values[3].season_number);
  filter1 = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_SEASON_NUMBER,
                               &value);
  g_value_unset (&value);

  /* NULL name */
  filter2 = gom_filter_new_is_null (EPISODE_TYPE_RESOURCE,
                                    EPISODE_COLUMN_EPISODE_NAME);

  /* Season and NULL name */
  filter3 = gom_filter_new_and (filter1, filter2);
  g_object_unref (filter1);
  g_object_unref (filter2);

  resource = gom_repository_find_one_sync (repository,
                                           EPISODE_TYPE_RESOURCE,
                                           filter3,
                                           &error);
  g_assert_no_error (error);
  g_assert (resource);
  g_object_unref (filter3);
  eres = EPISODE_RESOURCE (resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_IMDB_ID, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr (s1, ==, values[3].series_id);
  g_assert_cmpstr (s2, ==, values[3].imdb_id);
  g_free (s1);
  g_free (s2);

  free_memory_db (adapter, repository);
}

static void
find_not_null (void)
{
  GError *error = NULL;
  GValue value = { 0, };
  GomFilter *filter1, *filter2, *filter3;
  char *s1, *s2;
  GomResource *resource;
  EpisodeResource *eres;
  GomAdapter *adapter;
  GomRepository *repository;

  create_memory_db (&adapter, &repository);

  /* Season Number */
  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, values[2].season_number);
  filter1 = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                               EPISODE_COLUMN_SEASON_NUMBER,
                               &value);
  g_value_unset (&value);

  /* NULL name */
  filter2 = gom_filter_new_is_not_null (EPISODE_TYPE_RESOURCE,
                                        EPISODE_COLUMN_EPISODE_NAME);

  /* Season and NULL name */
  filter3 = gom_filter_new_and (filter1, filter2);
  g_object_unref (filter1);
  g_object_unref (filter2);

  resource = gom_repository_find_one_sync (repository,
                                           EPISODE_TYPE_RESOURCE,
                                           filter3,
                                           &error);
  g_assert_no_error (error);
  g_assert (resource);
  g_object_unref (filter3);
  eres = EPISODE_RESOURCE (resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_IMDB_ID, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr (s1, ==, values[2].series_id);
  g_assert_cmpstr (s2, ==, values[2].imdb_id);
  g_free (s1);
  g_free (s2);

  free_memory_db (adapter, repository);
}

static void
find_sql (void)
{
  GomAdapter *adapter;
  GomRepository *repository;

  GValue value = { 0, };
  GArray *sql_values;
  GomFilter *filter;
  GError *error = NULL;

  GomResource *resource;
  EpisodeResource *eres;

  char *s1, *s2;

  create_memory_db(&adapter, &repository);

  g_value_init(&value, G_TYPE_STRING);
  g_value_set_string(&value, "New York Sour");
  sql_values = g_array_new(FALSE, FALSE, sizeof(GValue));
  g_array_append_val(sql_values, value);
  filter = gom_filter_new_sql("'episodes'.'episode-name' == ?", sql_values);
  g_value_unset(&value);
  g_array_unref (sql_values);

  resource = gom_repository_find_one_sync(repository, EPISODE_TYPE_RESOURCE,
                                          filter, &error);
  g_assert_no_error(error);
  g_assert(resource);
  g_object_unref(filter);
  eres = EPISODE_RESOURCE(resource);

  g_object_get(eres,
               EPISODE_COLUMN_SERIES_ID, &s1,
               EPISODE_COLUMN_EPISODE_NAME, &s2,
               NULL);
  g_object_unref(eres);

  g_assert_cmpstr(s1, ==, values[0].series_id);
  g_assert_cmpstr(s2, ==, values[0].episode_name);
  g_free(s1);
  g_free(s2);

  free_memory_db(adapter, repository);
}

gint
main (gint argc, gchar *argv[])
{
   g_test_init (&argc, &argv, NULL);
   g_test_add_func ("/GomRepository/find-simple", find_simple);
   g_test_add_func ("/GomRepository/find-specific", find_specific);
   g_test_add_func ("/GomRepository/find-specific-and-full", find_specific_and_full);
   g_test_add_func ("/GomRepository/find-specific-and-fullv", find_specific_and_fullv);
   g_test_add_func ("/GomRepository/find-specific-ensure-priorities", find_specific_ensure_priorities);
   g_test_add_func ("/GomRepository/find-glob", find_glob);
   g_test_add_func ("/GomRepository/find-null", find_null);
   g_test_add_func ("/GomRepository/find-not-null", find_not_null);
   g_test_add_func ("/GomRepository/find-sql", find_sql);
   gMainLoop = g_main_loop_new (NULL, FALSE);
   return g_test_run ();
}
