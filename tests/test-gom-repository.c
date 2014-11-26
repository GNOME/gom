#include <gom/gom.h>

static GMainLoop *gMainLoop;

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
      EXEC_OR_FAIL("CREATE TABLE messages ("
                   " id INTEGER PRIMARY KEY,"
                   " sender TEXT,"
                   " recipient TEXT,"
                   " body TEXT"
                   ");");
      return TRUE;
   }

   if (version == 2) {
     EXEC_OR_FAIL("ALTER TABLE messages ADD COLUMN headers TEXT");
     return TRUE;
   }

failure:
   return FALSE;
}

static void
close_cb (GObject      *object,
	  GAsyncResult *result,
	  gpointer      user_data)
{
   GomAdapter *adapter = (GomAdapter *)object;
   gboolean *success = user_data;
   gboolean ret;
   GError *error = NULL;

   ret = gom_adapter_close_finish(adapter, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

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

   ret = gom_adapter_open_finish(adapter, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   repository = gom_repository_new(adapter);
   gom_repository_migrate_async(repository, 2, do_migrate, NULL, migrate_cb, user_data);
   g_object_unref(repository);
}

static void
migrate (void)
{
   GomAdapter *adapter;
   gboolean success = FALSE;

   adapter = gom_adapter_new();
   gom_adapter_open_async(adapter, ":memory:", open_cb, &success);
   g_object_unref(adapter);

   g_main_loop_run(gMainLoop);

   g_assert(success);
}

static void
test_repo_finalize (void)
{
  GomAdapter *adapter;
  GomRepository *repository;
  gboolean ret;
  GError *error = NULL;

  /* Unref repo, then close and unref adapter */
  adapter = gom_adapter_new();
  ret = gom_adapter_open_sync (adapter, ":memory:", &error);
  g_assert_no_error (error);
  g_assert (ret);

  repository = gom_repository_new (adapter);

  g_clear_object (&repository);
  ret = gom_adapter_close_sync (adapter, &error);
  g_assert_no_error (error);
  g_assert (ret);

  g_clear_object (&adapter);

  /* Close and unref adapter, then unref repo */
  adapter = gom_adapter_new();
  ret = gom_adapter_open_sync (adapter, ":memory:", &error);
  g_assert_no_error (error);
  g_assert (ret);

  repository = gom_repository_new (adapter);

  ret = gom_adapter_close_sync (adapter, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_clear_object (&adapter);

  g_clear_object (&repository);
}

gint
main (gint   argc,
	  gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomRepository/migrate", migrate);
   g_test_add_func ("/GomRepository/test-repo-finalize", test_repo_finalize);
   gMainLoop = g_main_loop_new(NULL, FALSE);
   return g_test_run();
}
