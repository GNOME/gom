Title: Sample: Entity Mapper

This is the smallest useful entity-mapper example: define one entity, register
it, open a repository, insert a row, and read it back.

The entity type is normal GObject code. The `GomEntityClass` metadata is the
part that tells `libgom` how to map it to a relation.

## Note Entity

```c
#include <libgom.h>

#define SAMPLE_TYPE_NOTE (sample_note_get_type ())

G_DECLARE_FINAL_TYPE (SampleNote, sample_note, SAMPLE, NOTE, GomEntity)

struct _SampleNote
{
  GomEntity parent_instance;

  gint64 id;
  char *title;
};

G_DEFINE_FINAL_TYPE (SampleNote, sample_note, GOM_TYPE_ENTITY)

enum {
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
sample_note_finalize (GObject *object)
{
  SampleNote *self = SAMPLE_NOTE (object);

  g_clear_pointer (&self->title, g_free);

  G_OBJECT_CLASS (sample_note_parent_class)->finalize (object);
}

static void
sample_note_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  SampleNote *self = SAMPLE_NOTE (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sample_note_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  SampleNote *self = SAMPLE_NOTE (object);

  switch (prop_id)
    {
    case PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case PROP_TITLE:
      g_set_str (&self->title, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
sample_note_class_init (SampleNoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GomEntityClass *entity_class = GOM_ENTITY_CLASS (klass);

  object_class->finalize = sample_note_finalize;
  object_class->get_property = sample_note_get_property;
  object_class->set_property = sample_note_set_property;

  properties[PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        0, G_MAXINT64, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  gom_entity_class_property_set_mapped (entity_class, "id", TRUE);
  gom_entity_class_property_set_version_added (entity_class, "id", 1);

  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  gom_entity_class_property_set_mapped (entity_class, "title", TRUE);
  gom_entity_class_property_set_version_added (entity_class, "title", 1);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gom_entity_class_set_relation (entity_class, "sample_notes");
  gom_entity_class_set_identity_field (entity_class, "id");
  gom_entity_class_set_version_added (entity_class, 1);
}

static void
sample_note_init (SampleNote *self)
{
  self->id = 0;
}
```

## Repository Workflow

Run this from a `Dex` fiber. The repository constructor opens the backend and
runs schema migration before it resolves.

```c
static DexFuture *
run (gpointer user_data)
{
  const char *uri = user_data;
  g_autoptr(GError) error = NULL;
  g_autoptr(GomDriver) driver = NULL;
  g_autoptr(GomRegistryBuilder) builder = NULL;
  g_autoptr(GomRegistry) registry = NULL;
  g_autoptr(GomRepository) repository = NULL;
  g_autoptr(SampleNote) note = NULL;
  g_autoptr(SampleNote) loaded = NULL;
  gint64 id = 0;

  driver = gom_driver_open (uri, &error);
  if (driver == NULL)
    g_error ("Failed to open driver: %s", error->message);

  builder = gom_registry_builder_new ();
  gom_registry_builder_add_entity_type (builder, SAMPLE_TYPE_NOTE);
  registry = gom_registry_builder_build (builder);

  repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver),
                                                     registry,
                                                     NULL),
                                 &error);
  if (repository == NULL)
    g_error ("Failed to open repository: %s", error->message);

  note = g_object_new (SAMPLE_TYPE_NOTE,
                       "title", "Hello from libgom",
                       NULL);

  if (!dex_await (gom_repository_insert_entity (repository, GOM_ENTITY (note)), &error))
    g_error ("Insert failed: %s", error->message);

  g_object_get (note, "id", &id, NULL);
  loaded = dex_await_object (gom_repository_find_one (repository, SAMPLE_TYPE_NOTE,
                                                      "id", id,
                                                      NULL),
                             &error);
  if (loaded == NULL)
    g_error ("Find failed: %s", error ? error->message : "not found");

  g_print ("Loaded note: %s\n", loaded->title);

  return dex_future_new_for_boolean (TRUE);
}
```

Use [method@Gom.Repository.query] with [struct@Gom.QueryBuilder] when you need
ordering, projections, joins, or cursor-level control.

Use [method@Gom.Entity.update] and [method@Gom.Entity.delete] in the same
shape after a materialized entity has a repository and identity value.
