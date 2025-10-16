#include <gom/gom.h>

static GMainLoop *gMainLoop;

static void
test_GomAdapter_open_async_close_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
   gboolean *success = user_data;
   gboolean ret;
   GError *error = NULL;

   ret = gom_adapter_close_finish(GOM_ADAPTER(object), result, &error);
   g_assert_no_error(error);
   g_assert_true(ret);

   *success = TRUE;
   g_main_loop_quit(gMainLoop);
   g_clear_pointer(&gMainLoop, g_main_loop_unref);
}

static void
test_GomAdapter_open_async_open_cb (GObject      *object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
   gboolean ret;
   GError *error = NULL;

   ret = gom_adapter_open_finish(GOM_ADAPTER(object), result, &error);
   g_assert_no_error(error);
   g_assert_true(ret);

   gom_adapter_close_async(GOM_ADAPTER(object), test_GomAdapter_open_async_close_cb, user_data);
}

static void
test_GomAdapter_open_async (void)
{
   GomAdapter *adapter;
   gboolean success = FALSE;

   adapter = gom_adapter_new();
   gom_adapter_open_async(adapter, ":memory:", test_GomAdapter_open_async_open_cb, &success);
   g_main_loop_run(gMainLoop);
   g_assert_true(success);

   g_clear_object (&adapter);
}

gint
main (gint   argc,
      gchar *argv[])
{
   g_test_init(&argc, &argv, NULL);
   g_test_add_func("/GomAdapter/open_async", test_GomAdapter_open_async);
   gMainLoop = g_main_loop_new(NULL, FALSE);
   return g_test_run();
}
