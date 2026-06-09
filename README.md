# libgom

`libgom` is an asynchronous data-mapper for GObject built on top of `libdex`.
It is designed for applications that want a strong data layer with:

- async repository, session, and cursor APIs
- entity mapping with identity, lifecycle, and relationship management
- schema-aware query and mutation builders
- backend-specific capabilities exposed through a single public API
- GTK-friendly model wrappers for UI-facing data

The library is intentionally broader than a thin SQL wrapper. It combines
schema metadata, object mapping, transactional unit-of-work semantics, and
driver-specific capabilities into one coherent stack.

## Status

`libgom` is currently in development. The stable surface in this alpha
is the core data-access stack.

Stable today:

- repository, session, query, cursor, record, and mutation APIs
- entity mapping, identity handling, relationship metadata, and entity CRUD
- schema migration from registered entity metadata
- SQLite and PostgreSQL backend modules for the core API
- GTK-facing `GListModel` adapters for query and relationship views

Planned or still settling:

The sync APIs are public scaffolding for local-first replication work, but the
replication system itself is not complete in this alpha. Treat sync
coordination, durable history, transport integration, and conflict-resolution
policy as an active development area rather than a finished application feature.

Vector search is backend- and build-dependent. It is available for SQLite when
the vec1 extension is enabled, and it is not currently implemented for
PostgreSQL in this tree.

## What libgom Gives You

`libgom` supports two complementary data-access styles:

1. Generic cursor workflows for direct query/mutation/result handling.
2. Entity mapper workflows for object-centric application code.

Both are first-class. You can move between them as needed without changing
drivers or rebuilding your data model around a single use case.

### Core API Surface

- `GomDriver` loads backend implementations from a database URI at runtime.
- `GomRepository` binds a driver to a registry and serves as the primary entry point.
- `GomRegistry` turns entity metadata into a backend schema model.
- `GomQuery` and `GomMutation` describe database operations without handwritten SQL.
- `GomCursor` streams query results asynchronously.
- `GomRecord` snapshots row values when you need detached result handling.
- `GomEntity` subclasses model application objects with identity, versioning, and change tracking.
- `GomSession` provides transaction-scoped unit-of-work behavior.
- `GomSyncCoordinator`, `GomSyncTransport`, `GomMergePolicy`, `GomMergeDecision`, and `GomDelta` define the in-progress sync contract.

## Feature Breakdown

### Querying

- Builder-based `SELECT` construction with entity-targeted or relation-targeted queries
- Filters, projections, grouping, group filters, ordering, offset, and limit
- Optional count-aware queries when a backend can provide row counts
- Async cursor iteration with `next()`, `rewind()`, `move_absolute()`, and `move_relative()`
- Cursor inspection helpers for typed column access, materialization, and detached snapshots

### Mutating

- Builder APIs for `INSERT`, `UPDATE`, and `DELETE`
- Entity-targeted or relation-targeted mutation definitions
- Multi-row insertion support
- Mutation results returned as `GomMutationResult` with `GomRecord` rows
- Backend-agnostic handling of affected rows and returned values
- Entity CRUD helpers that translate mapped properties into insert/update/delete operations

### Entity Mapping

- Explicit identity fields for safe updates and deletes
- Explicit mapped-property flags for persistent fields
- Version metadata on entities, properties, and indexes
- Entity lifecycle tracking: transient, pending, persistent, detached, and deleted
- Entity origin tracking: constructed vs. materialized
- Relationship metadata for one-to-one, one-to-many, many-to-many, and self-referential graphs
- Inverse relationship maintenance
- Schema-driven delete rules: nullify, cascade, deny, and no action
- Property byte transforms for custom serialization
- Vector property metadata for backend-supported vector search
- Search flags for indexed, prefix, case-folded, and normalized text search

### Sessions

- Transaction-scoped read/write boundaries
- Sticky identity-map behavior for materialized entities
- `persist()`, `flush()`, `commit()`, and `rollback()` for unit-of-work workflows
- Immediate property coherence across holders of the same session-managed entity
- Session-backed query and relationship models for UI layers

### UI-Facing Models

- `GomQueryModel` for live query-backed `GListModel` consumers
- `GomRelatedModel` for relationship-backed collections
- `GomEntityListModel` for database-backed entity lists with a stable model surface
- Incremental `items-changed` updates instead of hand-written row diffing
- Loading state exposed for skeleton rows, spinners, or progressive UI

### Schema And Migration

- Automatic schema migration during repository creation
- Versioned registry snapshots derived from entity and property metadata
- Driver-backed migration hooks for backend-specific evolution
- Public migration helpers for custom migrations, SQL migrations, nested migrations, and entity migrations
- Schema introspection through relation listing and relation description APIs
- Versioned entity and property lifecycle management for long-lived schemas

### Sync Coordination

`libgom` exposes the public surface being used to build a future local-first
replication layer:

- `GomDelta` captures property-level original and current values
- entities can produce deltas from local edits or materialized state
- repositories can own a sync coordinator
- sessions can access the repository coordinator
- accepted deltas can clear dirty state and update entity baselines
- transports and merge policies are separated from storage backends

This is deliberately modeled as orchestration over normal repository and
session flows, not as a special storage mode. The storage and transport pieces
are still in progress.

Planned direction for the sync coordinator:

- durable local history and replayable change logs
- transport implementations outside the repository process
- conflict resolution through application-owned merge policy objects
- tombstone handling and remote replay/application
- background delivery, retry, and causal ordering
- transport encryption and enrollment/authentication layers on top of the core
  replication contract

## Driver Coverage

Backend support is not identical across drivers. The core API is intended to
work across supported backends, while features that depend on backend-specific
storage or extensions should be checked before use.

| Feature | SQLite | PostgreSQL |
| --- | --- | --- |
| Runtime-loaded driver module | Yes | Yes |
| Repository and session APIs | Yes | Yes |
| Query, mutation, cursor, and record APIs | Yes | Yes |
| Entity mapping and entity CRUD | Yes | Yes |
| Relationship metadata and session semantics | Yes | Yes |
| Automatic schema migration | Yes | Yes |
| Schema introspection | Yes | Yes |
| GTK-facing query and relationship models | Yes | Yes |
| Search expressions for mapped text properties | FTS5-backed | `to_tsvector`/`tsquery`-backed |
| Encryption support | Via `sqlite3mc` | Use PostgreSQL deployment features |
| Vector search | Conditional, via SQLite vec1 | Not currently implemented |
| Dot-product vector distance | Not supported by SQLite vec1 | Not currently implemented |
| SQLite `rowid` identity back-fill behavior | Yes | Not applicable |

Use the `GOM_DATABASE_SQLITE`, `GOM_DATABASE_SQLITE_VEC1`, and
`GOM_DATABASE_POSTGRESQL` macros from `gom-config.h` when compiling code that
depends on backend-specific capabilities.

### SQLite

SQLite is the most feature-rich backend in `libgom` today.

- Runtime-loaded backend module selected by `file://` or `sqlite://` URIs
- Pooled SQLite connections with async repository/session/query support
- Automatic migration support, including transactional schema updates
- Native encryption support through `sqlite3mc`
- FTS5-backed full-text search for mapped text properties
- `vec1`-backed vector search when built with the vector extension
- Backend feature reporting through `GomRepositoryFeature`
- Support for vector-distance expressions when the backend and build enable it
- SQLite-specific rowid behavior in mutation results and identity back-filling

Important notes:

- Vector search is conditional on the build enabling SQLite vec1 support.
- Dot-product vector search is not supported by the SQLite vec1 backend in this tree.
- Vector support is also constrained by the storage format and platform endianness.

### PostgreSQL

PostgreSQL is a supported backend with the same overall core data-access model.

- Runtime-loaded backend module selected by `postgresql://` URIs
- Connection-pool backed async repository/session/query support
- Query, mutation, migration, and introspection support through the same public API
- Full entity mapping, relationship handling, and session semantics
- PostgreSQL-native type binding and result handling

Current backend distinction:

- PostgreSQL supports the core libgom stack, but not repository-level vector search in this implementation.
- Code that depends on backend-specific behavior should always check the relevant `GOM_DATABASE_*` macro at compile time.

## Documentation

API reference:

- [libgom API docs](https://gnome.pages.gitlab.gnome.org/gom/libgom-2/)

Guides and concepts:

- [Overview](doc/overview.md)
- [Sessions](doc/session.md)
- [Relationship Semantics](doc/relationships.md)
- [UI-Facing Models](doc/ui-models.md)
- [Mutation Builders and Entity CRUD](doc/mutations-and-entity-crud.md)
- [Migration and Versioning](doc/migrations-and-versioning.md)
- [Migration Guide](doc/migration-guide.md)
- [Examples: Entity Mapper](doc/examples-entity-mapper.md)
- [Examples: Cursor and Record Wrapper](doc/examples-cursor-record.md)

## License

`libgom` is licensed under the LGPL-2.1-or-later.
