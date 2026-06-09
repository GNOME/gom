Title: Migration Guide

This page covers source-level changes when moving existing code to `libgom`.

## Package and Headers

If you are migrating from `gom-1.0`, update your build inputs and includes:

- `pkg-config` dependency: `gom-1.0` -> `libgom-2`
- umbrella header: `#include <gom/gom.h>` -> `#include <libgom.h>`

`libgom.h` is the public umbrella header for the 2.x API.

It also includes `gom-config.h`, so applications can check backend support
at compile time. `libgom` supports multiple backends, including SQLite and
PostgreSQL, and backend-specific code should be guarded with the relevant
`GOM_DATABASE_*` macro such as `GOM_DATABASE_SQLITE` or
`GOM_DATABASE_POSTGRESQL`.

## Entity Type Rename

The entity mapper API now uses `GomEntity` as the public type name.

If you are reading older material or porting code from the earlier resource
terminology, replace `GomResource` with `GomEntity` and use the current entity
helpers and metadata APIs.

## Entity Mapping

Entity classes should now explicitly mark each property that belongs in the
data model with `gom_entity_class_property_set_mapped (..., TRUE)`.

Do this during class initialization for every property that should be
persistent, queryable, or included in entity CRUD operations. Keep internal
state properties unmapped.

Example:

```c
gom_entity_class_property_set_mapped (entity_class, "title", TRUE);
gom_entity_class_property_set_mapped (entity_class, "body", TRUE);
gom_entity_class_property_set_mapped (entity_class, "cache", FALSE);
```

## Version Added Values

Migration also depends on explicit `version_added` metadata.

Set `gom_entity_class_set_version_added (..., N)` for each entity class and
`gom_entity_class_property_set_version_added (..., N)` for each mapped
property, where `N` is the schema version where it first exists. For a new
schema, that is typically `1`.

If `version_added` is left at the default value, the entity or property will
not participate in versioned schema migration as expected.

## After the Rename

- Rebuild with the new `libgom-2` dependency name.
- Update any include guards, build scripts, or documentation that still refer
  to `gom-1.0` or `gom/gom.h`.
- Use the current API documentation at the `libgom-2` docs site.
