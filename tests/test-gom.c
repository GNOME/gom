#include <glib-object.h>
#include <glib/gstdio.h>
#include <gom/gom.h>
#include <stdlib.h>

#include "mock/mock-person.h"
#include "mock/mock-occupation.h"
#include "mock/mock-gender.h"

#define TEST_DB ".test-gom.db"

#define ASSERT_PROP_STR(_o, _p, _v) \
	G_STMT_START { \
		gchar *val = NULL; \
		g_object_get(_o, _p, &val, NULL); \
		g_assert_cmpstr(val, ==, _v); \
		g_free(val); \
	} G_STMT_END

#define ASSERT_PROP_OBJECT(_o, _p, _v) \
	G_STMT_START { \
		GObject *obj = NULL; \
		g_object_get(_o, _p, &obj, NULL); \
		g_assert((gpointer)obj == (gpointer)_v); \
		g_object_unref(obj); \
	} G_STMT_END

#define ASSERT_PROP_ENUM(_o, _p, _v) \
	G_STMT_START { \
		gint val = 0; \
		g_object_get(_o, _p, &val, NULL); \
		g_assert_cmpint(val, ==, _v); \
	} G_STMT_END

#define ASSERT_IS_NEW(_o) g_assert(gom_resource_is_new(_o))
#define ASSERT_NOT_NEW(_o) g_assert(!gom_resource_is_new(_o))

typedef struct
{
	GVoidFunc test_func;
} TestGomFixture;

static void
test_gom_fixture_setup (TestGomFixture *fixture,
                        gconstpointer   user_data)
{
	fixture->test_func = user_data;
}

static void
test_gom_fixture_teardown (TestGomFixture *fixture,
                           gconstpointer   user_data)
{
}

static void
test_gom_fixture_test (TestGomFixture *fixture,
                       gconstpointer   user_data)
{
	if (!g_getenv("NO_FORK")) {
		if (g_test_trap_fork(0, 0)) {
			fixture->test_func();
			exit(0);
		}
		g_test_trap_assert_passed();
	} else {
		fixture->test_func();
	}
}

static void
test_gom_resource_properties (void)
{
	GomResource *occupation;
	GomResource *person;

	occupation = g_object_new(MOCK_TYPE_OCCUPATION,
	                          "name", "Software Engineer",
	                          "industry", "Information Systems",
	                          NULL);
	ASSERT_PROP_STR(occupation, "name", "Software Engineer");
	ASSERT_PROP_STR(occupation, "industry", "Information Systems");
	person = g_object_new(MOCK_TYPE_PERSON,
	                        "name", "Jane Smith",
	                        "gender", MOCK_GENDER_FEMALE,
	                        "occupation", occupation,
	                        NULL);
	ASSERT_PROP_STR(person, "name", "Jane Smith");
	ASSERT_PROP_ENUM(person, "gender", MOCK_GENDER_FEMALE);
	ASSERT_PROP_OBJECT(person, "occupation", occupation);
}

static void
test_gom_resource_class_init (void)
{
	GObjectClass *object_class;

	object_class = g_type_class_ref(MOCK_TYPE_PERSON);
	g_assert(g_object_class_find_property(object_class, "friends"));
	g_assert(g_object_class_find_property(object_class, "gender"));
	g_assert(g_object_class_find_property(object_class, "id"));
	g_assert(g_object_class_find_property(object_class, "name"));
	g_assert(g_object_class_find_property(object_class, "occupation"));
	g_type_class_unref(object_class);

	object_class = g_type_class_ref(MOCK_TYPE_OCCUPATION);
	g_assert(g_object_class_find_property(object_class, "name"));
	g_assert(g_object_class_find_property(object_class, "id"));
	g_assert(g_object_class_find_property(object_class, "industry"));
	g_type_class_unref(object_class);
}

static void
on_sqlite_opened (GomAdapterSqlite *sqlite,
                  gboolean         *state)
{
	g_assert_cmpint(*state, ==, FALSE);
	*state = TRUE;
}

static void
on_sqlite_closed (GomAdapterSqlite *sqlite,
                  gboolean         *state)
{
	g_assert_cmpint(*state, ==, TRUE);
	*state = FALSE;
}

static void
test_gom_adapter_sqlite_basic (void)
{
	GomAdapterSqlite *sqlite;
	GomResource *occupation;
	GomResource *person;
	GomAdapter *adapter;
	gboolean state = FALSE;
	GError *error = NULL;
	gint i;

	sqlite = g_object_new(GOM_TYPE_ADAPTER_SQLITE, NULL);
	adapter = GOM_ADAPTER(sqlite);
	g_signal_connect(sqlite, "opened", G_CALLBACK(on_sqlite_opened), &state);
	g_signal_connect(sqlite, "closed", G_CALLBACK(on_sqlite_closed), &state);
	if (!gom_adapter_sqlite_load_from_file(sqlite, TEST_DB, &error)) {
		g_error("%s", error->message);
		g_error_free(error);
		g_assert_not_reached();
	}
	g_assert_cmpint(state, ==, TRUE);
	if (!gom_adapter_sqlite_create_table(sqlite, MOCK_TYPE_PERSON, &error)) {
		g_error("%s", error->message);
		g_error_free(error);
		g_assert_not_reached();
	}
	occupation = gom_resource_create(MOCK_TYPE_OCCUPATION, adapter,
	                                 "name", "Software Engineer",
	                                 "industry", "Information Systems",
	                                 NULL);
	person = gom_resource_create(MOCK_TYPE_PERSON, adapter,
	                             "name", "John Smith",
	                             "occupation", occupation,
	                             "gender", MOCK_GENDER_MALE,
	                             NULL);
	ASSERT_IS_NEW(occupation);
	ASSERT_IS_NEW(person);
	ASSERT_PROP_STR(person, "name", "John Smith");
	ASSERT_PROP_STR(occupation, "name", "Software Engineer");
	ASSERT_PROP_STR(occupation, "industry", "Information Systems");
	if (!gom_resource_save(person, &error)) {
		g_error("%s", error->message);
		g_error_free(error);
		g_assert_not_reached();
	}
	ASSERT_NOT_NEW(occupation);
	ASSERT_NOT_NEW(person);
	g_object_unref(occupation);
	g_object_unref(person);
	if (g_test_slow()) {
		gom_adapter_sqlite_begin(sqlite);
		for (i = 0; i < 1000; i++) {
			gchar name[32];
			g_snprintf(name, sizeof name, "Occupation %d", i);
			occupation = gom_resource_create(MOCK_TYPE_OCCUPATION, adapter,
			                                 "name", name,
			                                 "industry", "Information Systems",
			                                 NULL);
			person = gom_resource_create(MOCK_TYPE_PERSON, adapter,
			                             "name", "John Smith",
			                             "occupation", occupation,
			                             "gender", MOCK_GENDER_MALE,
			                             NULL);
			if (!gom_resource_save(person, &error)) {
				g_error("%s", error->message);
				g_error_free(error);
				g_assert_not_reached();
			}
			g_object_unref(occupation);
			g_object_unref(person);
		}
		if (!gom_adapter_sqlite_commit(sqlite, &error)) {
			g_error("%s", error->message);
			g_error_free(error);
			g_assert_not_reached();
		}
	}
	gom_adapter_sqlite_close(sqlite);
	g_assert_cmpint(state, ==, FALSE);
	g_object_unref(sqlite);
}

static void
test_gom_query_basic (void)
{
	GomQuery *query;
	GomPropertySet *fields;
	GomProperty *prop;
	GomResourceClass *klass;
	GomCondition *cond;
	gboolean unique = FALSE;
	guint64 limit = 0;
	guint64 offset = 0;
	GType resource_type;
	GValue val = { 0 };

	klass = g_type_class_ref(MOCK_TYPE_PERSON);
	fields = gom_resource_class_get_properties(klass);
	prop = gom_property_set_find(fields, "id");
	g_value_init(&val, G_TYPE_UINT64);
	g_value_set_uint64(&val, 1);
	cond = gom_condition_equal(prop, &val);
	query = g_object_new(GOM_TYPE_QUERY,
	                     "resource-type", MOCK_TYPE_PERSON,
	                     "condition", cond,
	                     "direction", GOM_QUERY_ASCENDING,
	                     "unique", TRUE,
	                     "limit", G_GUINT64_CONSTANT(123),
	                     "offset", G_GUINT64_CONSTANT(321),
	                     "fields", fields,
	                     NULL);
	gom_condition_unref(cond);
	g_assert(query);
	g_object_get(query,
	             "unique", &unique,
	             "limit", &limit,
	             "offset", &offset,
	             "resource-type", &resource_type,
	             NULL);
	g_assert_cmpint(unique, ==, TRUE);
	g_assert_cmpint(limit, ==, 123);
	g_assert_cmpint(offset, ==, 321);
	g_assert_cmpint(resource_type, ==, MOCK_TYPE_PERSON);
	g_object_unref(query);
}

static void
test_gom_resource_delete (void)
{
	GomAdapterSqlite *sqlite;
	GomResource *person;
	GError *error = NULL;

	sqlite = g_object_new(GOM_TYPE_ADAPTER_SQLITE, NULL);

	if (!gom_adapter_sqlite_load_from_file(sqlite, TEST_DB, &error)) {
		g_error("%s", error->message);
		g_error_free(error);
		g_assert_not_reached();
	}

	person = g_object_new(MOCK_TYPE_PERSON,
	                      "adapter", sqlite,
	                      "name", "Christian Hergert",
	                      NULL);
	if (!gom_resource_save(person, &error)) {
		g_error("%s", error->message);
		g_error_free(error);
		g_assert_not_reached();
	}

	if (!gom_resource_delete(person, &error)) {
		g_error("%s", error->message);
		g_error_free(error);
		g_assert_not_reached();
	}

	gom_adapter_sqlite_close(sqlite);

	g_object_unref(sqlite);
}

gint
main (gint   argc,
      gchar *argv[])
{
	g_test_init(&argc, &argv, NULL);
	g_type_init();

	if (g_file_test(TEST_DB, G_FILE_TEST_EXISTS)) {
		g_assert(!g_remove(TEST_DB));
	}

#define ADD_FORKED_TEST(_n, _f) \
	g_test_add(_n, TestGomFixture, _f, \
	           test_gom_fixture_setup, \
	           test_gom_fixture_test, \
	           test_gom_fixture_teardown)
			
	ADD_FORKED_TEST("/Gom/Resource/delete",
	                test_gom_resource_delete);
	ADD_FORKED_TEST("/Gom/Resource/properties",
	                test_gom_resource_properties);
	ADD_FORKED_TEST("/Gom/Resource/Class/init",
	                test_gom_resource_class_init);
	ADD_FORKED_TEST("/Gom/Adapter/Sqlite/basic",
	                test_gom_adapter_sqlite_basic);
	g_test_add_func("/Gom/Query/basic", test_gom_query_basic);

#undef ADD_FORKED_TEST

	return g_test_run();
}
