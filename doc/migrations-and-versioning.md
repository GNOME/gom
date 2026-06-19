Title: Migration and Versioning

`libgom` uses registry metadata and backend migration hooks to move schema forward automatically during repository initialization.

## Version Model

Version data is derived from entity and property metadata:

- Entity lifecycle: `version_added`, `version_removed`.
- Property lifecycle: `version_added`, `version_removed`.
- Index lifecycle: `version_added`, `version_removed`.
- Entity schema role: primary entities participate in schema migration;
  alias entities are query/materialization views over an existing relation.

`GomRegistry` tracks:

- current registry version ([method@Gom.Registry.get_version]), and
- highest version referenced by metadata ([method@Gom.Registry.get_max_version]).

[method@Gom.Registry.snapshot] produces a version-filtered view of entities/properties/indexes visible at version `N`.

## Repository Startup Flow

[ctor@Gom.Repository.new] performs:

1. Build repository from provided registry (or empty registry when `NULL`).
2. Read backend schema version via driver `get_version`.
3. For each step `current_version -> current_version + 1` until registry max version:
   Call driver `migrate(current_snapshot, next_snapshot)`.
4. Return repository only after all steps succeed.

If any migration step fails, repository creation fails.

## Backend Responsibilities

A backend that supports migration should provide driver hooks for:

- `get_version`: return current schema version.
- `migrate`: apply one version step using `current` and `next` snapshots.

Optional (used by `GomSqlMigration`):

- `execute_sql`: run raw SQL scripts.

Backends that do not implement a hook may reject with `G_IO_ERROR_NOT_SUPPORTED`.

## Backend Notes

SQLite backend currently:

- Stores version in `PRAGMA user_version`.
- Computes schema deltas from snapshot pairs.
- Creates/drops relations by primary entity visibility window.
- Adds/removes columns as supported by SQLite migration strategy.
- Creates/drops indexes and search structures from property/index metadata.
- Updates `user_version` as migration steps complete.

## Schema Aliases

Use `gom_entity_class_set_schema_role()` with
`GOM_ENTITY_SCHEMA_ROLE_ALIAS` for lightweight entity types that map to the
same relation as a full entity. Alias entities remain usable as query targets
and materialization targets, but they do not affect the migration target
version and do not create or alter relation schema.

PostgreSQL backend:

- Uses the same repository startup flow and driver migration hooks.
- Is exposed through the pgsql driver when `GOM_DATABASE_POSTGRESQL` is
  defined at build time.
- Should be treated as backend-specific for any SQL or identity behavior that
  differs from SQLite.

## Authoring Guidance

- Treat each schema version as immutable once released.
- Use monotonic version numbers.
- Prefer additive changes when possible.
- For destructive changes, ensure migration path is explicit and validated.
