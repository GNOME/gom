Title: Quickstart

This guide walks through the smallest useful `libgom` flow. The sample uses
the SQLite backend, but `libgom` also supports PostgreSQL when built with that
backend:

1. Create/open a repository backed by SQLite or PostgreSQL.
2. Let [ctor@Gom.Repository.new] run schema migration.
3. Insert one entity row.
4. Query and materialize it back into a `GomEntity`.

If you need a transaction-scoped identity map for a longer workflow, see [Sessions](session.html).

If you are compiling code that targets a specific backend, `libgom.h` also
pulls in `gom-config.h`, which defines `GOM_DATABASE_SQLITE` and
`GOM_DATABASE_POSTGRESQL` when the corresponding backend was built. Check for
the relevant `GOM_DATABASE_*` macro before using backend-specific code.

## Prerequisites

- GLib/GObject/GIO
- libdex-1
- libgom-2
- A writable backend URI:
  - SQLite example: `file:///tmp/libgom-quickstart.db`
  - PostgreSQL example: `postgresql:///libgom?user=$USER`

## Minimal Sample

The note sample in [Sample: Entity Mapper](examples-entity-mapper.html) shows
the smallest entity-mapper flow without a full application around it.

## What the Sample Does

1. Defines `SampleNote` as a `GomEntity` with:
   - relation: `sample_notes`
   - identity field: `id`
   - mapped fields: `id`, `title`
   - version metadata for the entity and mapped fields
2. Registers the type through `GomRegistryBuilder`.
3. Opens `GomRepository` with [ctor@Gom.Repository.new].
4. Relies on startup migration to create/update schema to registry max version.
5. Inserts a row with [method@Gom.Repository.insert_entity], which back-fills the
   generated identity onto the entity.
6. Reads it back with [method@Gom.Repository.find_one].

## Minimal API Skeleton

Inside a fiber spawned with `dex_scheduler_spawn()`:

```c
#define SAMPLE_TYPE_NOTE (sample_note_get_type ())

registry_builder = gom_registry_builder_new ();
gom_registry_builder_add_entity_type (registry_builder, SAMPLE_TYPE_NOTE);
registry = gom_registry_builder_build (registry_builder);

repository = dex_await_object (gom_repository_new (GOM_DRIVER (driver), registry, NULL), &error);

note = g_object_new (SAMPLE_TYPE_NOTE, "title", "Hello", NULL);

if (!dex_await (gom_repository_insert_entity (repository, GOM_ENTITY (note)), &error))
  g_error ("Insert failed: %s", error->message);

gint64 id = 0;
g_object_get (note, "id", &id, NULL);

loaded = dex_await_object (gom_repository_find_one (repository, SAMPLE_TYPE_NOTE,
                                                    "id", id,
                                                    NULL),
                           &error);
```

See [Sample: Entity Mapper](examples-entity-mapper.html) for the mapped entity
type and full repository workflow.
