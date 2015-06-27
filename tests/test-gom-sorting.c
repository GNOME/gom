#include <gom/gom.h>


static GMainLoop *gMainLoop;


#define EPISODE_TYPE_RESOURCE              (episode_resource_get_type())
#define EPISODE_TYPE_TYPE                  (episode_type_get_type())
#define EPISODE_RESOURCE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPISODE_TYPE_RESOURCE, EpisodeResource))
#define EPISODE_RESOURCE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), EPISODE_TYPE_RESOURCE, EpisodeResourceClass))
#define EPISODE_IS_RESOURCE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPISODE_TYPE_RESOURCE))
#define EPISODE_IS_RESOURCE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), EPISODE_TYPE_RESOURCE))
#define EPISODE_RESOURCE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), EPISODE_TYPE_RESOURCE, EpisodeResourceClass))

typedef struct {
   gint64  db_id;
   gchar  *series_id;
   gchar  *imdb_id;
   guint8  season_number;
   guint8  episode_number;
   gchar  *episode_name;
} EpisodeResourcePrivate;

typedef struct
{
   GomResource             parent;
   EpisodeResourcePrivate *priv;
} EpisodeResource;

typedef struct
{
   GomResourceClass parent_class;
} EpisodeResourceClass;

GType episode_resource_get_type(void);

G_DEFINE_TYPE(EpisodeResource, episode_resource, GOM_TYPE_RESOURCE)

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
   { "84947", "tt3216480", 5, 1, "Golden Days for Boys and Girls" },
   { "84947", "tt3767076", 5, 2, "The Good Listener" }
};

static void
episode_resource_finalize (GObject *object)
{
   EpisodeResourcePrivate *priv = EPISODE_RESOURCE(object)->priv;

   g_clear_pointer(&priv->series_id, g_free);
   g_clear_pointer(&priv->imdb_id, g_free);
   g_clear_pointer(&priv->episode_name, g_free);

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
      g_value_set_int64(value, resource->priv->db_id);
      break;
   case PROP_SERIES_ID:
      g_value_set_string(value, resource->priv->series_id);
      break;
   case PROP_IMDB_ID:
      g_value_set_string(value, resource->priv->imdb_id);
      break;
   case PROP_SEASON_NUMBER:
      g_value_set_uchar(value, resource->priv->season_number);
      break;
   case PROP_EPISODE_NUMBER:
      g_value_set_uchar(value, resource->priv->episode_number);
      break;
   case PROP_EPISODE_NAME:
      g_value_set_string(value, resource->priv->episode_name);
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
      resource->priv->db_id = g_value_get_int64(value);
      break;
   case PROP_SERIES_ID:
      g_clear_pointer(&resource->priv->series_id, g_free);
      resource->priv->series_id = g_value_dup_string(value);
      break;
   case PROP_IMDB_ID:
      g_clear_pointer(&resource->priv->imdb_id, g_free);
      resource->priv->imdb_id = g_value_dup_string(value);
      break;
   case PROP_SEASON_NUMBER:
      resource->priv->season_number = g_value_get_uchar(value);
      break;
   case PROP_EPISODE_NUMBER:
      resource->priv->episode_number = g_value_get_uchar(value);
      break;
   case PROP_EPISODE_NAME:
      g_clear_pointer(&resource->priv->episode_name, g_free);
      resource->priv->episode_name = g_value_dup_string(value);
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
   g_type_class_add_private(object_class, sizeof(EpisodeResourcePrivate));

   resource_class = GOM_RESOURCE_CLASS(klass);
   gom_resource_class_set_table(resource_class, "episodes");

   specs[PROP_DB_ID] = g_param_spec_int64("id", NULL, NULL, 0, G_MAXINT64,
                                          0, G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_DB_ID,
                                   specs[PROP_DB_ID]);
   gom_resource_class_set_primary_key(resource_class, "id");

   specs[PROP_SERIES_ID] = g_param_spec_string("series-id", NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_SERIES_ID,
                                   specs[PROP_SERIES_ID]);

   specs[PROP_IMDB_ID] = g_param_spec_string("imdb-id", NULL, NULL, NULL,
                                             G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_IMDB_ID,
                                   specs[PROP_IMDB_ID]);

   specs[PROP_SEASON_NUMBER] = g_param_spec_uchar("season-number", NULL, NULL,
                                                  0, G_MAXUINT8, 0,
                                                  G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_SEASON_NUMBER,
                                   specs[PROP_SEASON_NUMBER]);

   specs[PROP_EPISODE_NUMBER] = g_param_spec_uchar("episode-number", NULL,
                                                   NULL, 0, G_MAXUINT8, 0,
                                                   G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_EPISODE_NUMBER,
                                   specs[PROP_EPISODE_NUMBER]);

   specs[PROP_EPISODE_NAME] = g_param_spec_string("episode-name", NULL, NULL,
                                                  NULL, G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_EPISODE_NAME,
                                   specs[PROP_EPISODE_NAME]);

}

static void
episode_resource_init (EpisodeResource *resource)
{
   resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                                EPISODE_TYPE_RESOURCE,
                                                EpisodeResourcePrivate);
}

static void
create_memory_db (GomAdapter    **adapter,
                  GomRepository **repository)
{
   gboolean ret;
   GError *error = NULL;
   GList *object_types;
   EpisodeResource *eres;
   guint i;

   *adapter = gom_adapter_new();
   ret = gom_adapter_open_sync(*adapter, ":memory:", &error);
   g_assert_no_error(error);
   g_assert(ret);

   *repository = gom_repository_new(*adapter);

   object_types = g_list_prepend(NULL,
                                 GINT_TO_POINTER(EPISODE_TYPE_RESOURCE));
   ret = gom_repository_automatic_migrate_sync(*repository, 1, object_types,
                                               &error);
   g_assert_no_error(error);
   g_assert(ret);

   for (i = 0; i < G_N_ELEMENTS(values); i++) {
      eres = g_object_new(EPISODE_TYPE_RESOURCE, "repository", *repository,
                          "series-id", values[i].series_id,
                          "imdb-id", values[i].imdb_id,
                          "season-number", values[i].season_number,
                          "episode-number", values[i].episode_number,
                          "episode-name", values[i].episode_name,
                          NULL);
      ret = gom_resource_save_sync(GOM_RESOURCE(eres), &error);
      g_assert(ret);
      g_assert_no_error(error);
      g_object_unref(eres);
   }
}

static void
free_memory_db (GomAdapter    *adapter,
                GomRepository *repository)
{
   gboolean ret;
   GError *error = NULL;

   ret = gom_adapter_close_sync(adapter, &error);
   g_assert_no_error(error);
   g_assert(ret);

   g_object_unref(repository);
   g_object_unref(adapter);
}


static void
find_order_by_asc (void)
{
   GomAdapter *adapter;
   GomRepository *repository;
   GomFilter *filter;
   GomSorting *sorting;
   GValue value = { 0, };
   GError *error = NULL;
   GomResourceGroup *group;
   EpisodeResource *eres;
   guint count;
   guint8 i;

   create_memory_db(&adapter, &repository);

   /* Filter on season number */
   g_value_init(&value, G_TYPE_INT64);
   g_value_set_int64(&value, 4);
   filter = gom_filter_new_eq(EPISODE_TYPE_RESOURCE, "season-number", &value);
   g_value_unset(&value);

   /* Order by episode */
   sorting = gom_sorting_new(EPISODE_TYPE_RESOURCE, "episode-number",
                             GOM_SORTING_ASCENDING, NULL);

   group = gom_repository_find_sorted_sync(repository, EPISODE_TYPE_RESOURCE,
                                           filter, sorting, &error);
   g_assert_no_error(error);
   g_object_unref(filter);
   g_object_unref(sorting);

   count = gom_resource_group_get_count(group);
   g_assert_cmpuint(count, ==, 2);

   gom_resource_group_fetch_sync(group, 0, count, &error);
   g_assert_no_error(error);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 0));
   g_assert(eres);
   g_object_get(eres, "episode-number", &i, NULL);
   g_assert_cmpuint(i, ==, 1);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 1));
   g_assert(eres);
   g_object_get(eres, "episode-number", &i, NULL);
   g_assert_cmpuint(i, ==, 2);
   g_object_unref(eres);

   free_memory_db(adapter, repository);
}

static void
find_order_by_desc (void)
{
   GomAdapter *adapter;
   GomRepository *repository;
   GomFilter *filter;
   GomSorting *sorting;
   GValue value = { 0, };
   GError *error = NULL;
   GomResourceGroup *group;
   EpisodeResource *eres;
   guint count;
   guint8 i;

   create_memory_db(&adapter, &repository);

   /* Filter on season number */
   g_value_init(&value, G_TYPE_INT64);
   g_value_set_int64(&value, 4);
   filter = gom_filter_new_eq(EPISODE_TYPE_RESOURCE, "season-number", &value);
   g_value_unset(&value);

   /* Order by episode */
   sorting = gom_sorting_new(EPISODE_TYPE_RESOURCE, "episode-number",
                             GOM_SORTING_DESCENDING, NULL);

   group = gom_repository_find_sorted_sync(repository, EPISODE_TYPE_RESOURCE,
                                           filter, sorting, &error);
   g_assert_no_error(error);
   g_object_unref(filter);
   g_object_unref(sorting);

   count = gom_resource_group_get_count(group);
   g_assert_cmpuint(count, ==, 2);

   gom_resource_group_fetch_sync(group, 0, count, &error);
   g_assert_no_error(error);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 0));
   g_assert(eres);
   g_object_get(eres, "episode-number", &i, NULL);
   g_assert_cmpuint(i, ==, 2);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 1));
   g_assert(eres);
   g_object_get(eres, "episode-number", &i, NULL);
   g_assert_cmpuint(i, ==, 1);
   g_object_unref(eres);

   free_memory_db(adapter, repository);
}

static void
find_order_by_complex (void)
{
   GomAdapter *adapter;
   GomRepository *repository;
   GomFilter *filter;
   GomSorting *sorting;
   GomResourceGroup *group;
   GValue value = { 0, };
   GError *error = NULL;
   EpisodeResource *eres;
   guint count;
   gchar *id;
   guint8 season, episode;

   create_memory_db(&adapter, &repository);

   /* Select only the episode for a single show */
   g_value_init(&value, G_TYPE_STRING);
   g_value_set_string(&value, "84947");
   filter = gom_filter_new_eq(EPISODE_TYPE_RESOURCE, "series-id", &value);
   g_value_unset(&value);

   /* Order by season, then by episode */
   sorting = gom_sorting_new(EPISODE_TYPE_RESOURCE, "season-number",
                             GOM_SORTING_DESCENDING,
                             EPISODE_TYPE_RESOURCE, "episode-number",
                             GOM_SORTING_ASCENDING,
                             NULL);

   group = gom_repository_find_sorted_sync(repository, EPISODE_TYPE_RESOURCE,
                                           filter, sorting, &error);
   g_assert_no_error(error);
   g_object_unref(sorting);

   count = gom_resource_group_get_count(group);
   g_assert_cmpuint(count, ==, 4);

   gom_resource_group_fetch_sync(group, 0, count, &error);
   g_assert_no_error(error);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 0));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 5);
   g_assert_cmpuint(episode, ==, 1);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 1));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 5);
   g_assert_cmpuint(episode, ==, 2);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 2));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 4);
   g_assert_cmpuint(episode, ==, 1);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 3));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 4);
   g_assert_cmpuint(episode, ==, 2);
   g_object_unref(eres);

   free_memory_db(adapter, repository);
}

static void
find_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
   GomAdapter *adapter;
   GomRepository *repository;
   GomResourceGroup *group;
   GError *error = NULL;
   EpisodeResource *eres;
   guint count;
   gchar *id;
   guint8 season, episode;

   repository = GOM_REPOSITORY(object);

   group = gom_repository_find_finish(repository, result, &error);
   g_assert_no_error(error);

   count = gom_resource_group_get_count(group);
   g_assert_cmpuint(count, ==, 4);

   gom_resource_group_fetch_sync(group, 0, count, &error);
   g_assert_no_error(error);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 0));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 5);
   g_assert_cmpuint(episode, ==, 1);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 1));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 5);
   g_assert_cmpuint(episode, ==, 2);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 2));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 4);
   g_assert_cmpuint(episode, ==, 1);
   g_object_unref(eres);

   eres = EPISODE_RESOURCE(gom_resource_group_get_index(group, 3));
   g_assert(eres);
   g_object_get(eres, "series-id", &id, "season-number", &season,
                "episode-number", &episode, NULL);
   g_assert_cmpstr(id, ==, "84947");
   g_assert_cmpuint(season, ==, 4);
   g_assert_cmpuint(episode, ==, 2);
   g_object_unref(eres);

   adapter = gom_repository_get_adapter(repository);
   free_memory_db(adapter, repository);

   g_main_loop_quit(gMainLoop);
}

static void
find_order_by_complex_async (void)
{
   GomAdapter *adapter;
   GomRepository *repository;
   GomFilter *filter;
   GomSorting *sorting;
   GValue value = { 0, };

   create_memory_db(&adapter, &repository);

   /* Select only the episode for a single show */
   g_value_init(&value, G_TYPE_STRING);
   g_value_set_string(&value, "84947");
   filter = gom_filter_new_eq(EPISODE_TYPE_RESOURCE, "series-id", &value);
   g_value_unset(&value);

   /* Order by season, then by episode */
   sorting = gom_sorting_new(EPISODE_TYPE_RESOURCE, "season-number",
                             GOM_SORTING_DESCENDING,
                             EPISODE_TYPE_RESOURCE, "episode-number",
                             GOM_SORTING_ASCENDING,
                             NULL);

   gom_repository_find_sorted_async(repository, EPISODE_TYPE_RESOURCE,
                                    filter, sorting, find_cb, NULL);
   g_object_unref(filter);
   g_object_unref(sorting);

   g_main_loop_run(gMainLoop);
}

gint
main (gint argc, gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomRepository/find-order-by-asc", find_order_by_asc);
   g_test_add_func("/GomRepository/find-order-by-desc", find_order_by_desc);
   g_test_add_func("/GomRepository/find-order-by-complex",
                   find_order_by_complex);
   g_test_add_func("/GomRepository/find-order-by-complex-async",
                   find_order_by_complex_async);
   gMainLoop = g_main_loop_new(NULL, FALSE);
   return g_test_run();
}
