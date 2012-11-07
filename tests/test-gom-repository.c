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

failure:
   return FALSE;
}

static void
migrate_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
   GomRepository *repository = (GomRepository *)object;
   gboolean *success = user_data;
   gboolean ret;
   GError *error = NULL;

   ret = gom_repository_migrate_finish(repository, result, &error);
   g_assert_no_error(error);
   g_assert(ret);

   *success = TRUE;
   g_main_loop_quit(gMainLoop);
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
   gom_repository_migrate_async(repository, 1, do_migrate, migrate_cb, user_data);
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

gint
main (gint   argc,
	  gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomRepository/migrate", migrate);
   g_type_init();
   gMainLoop = g_main_loop_new(NULL, FALSE);
   return g_test_run();
}
