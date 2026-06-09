Title: Overview

`libgom` is an asynchronous data-mapper for GObject types with a first-class SQL/cursor API and support for multiple SQL backends.

The library supports two complementary workflows:

1. Generic cursor workflow: `GomQuery` -> `GomCursor` -> `GomRecord`.
2. Entity mapper workflow: `GomEntity` subclasses + registry metadata + entity CRUD helpers.

## Core Model

- `GomDriver`: backend implementation (for example, SQLite or PostgreSQL).
- `GomRepository`: entry point bound to a driver and a registry.
- `GomRegistry`: schema metadata built from entity types.
- `GomQuery`/`GomMutation`: query and write request models.
- `GomCursor`: async row stream returned by query and mutation operations.
- `GomQueryModel`/`GomRelatedModel`: `GListModel`-based UI views over query
  results and relationships.

[ctor@Gom.Repository.new] initializes the repository and runs automatic schema migration from the current backend version to the registry max version before the repository is returned.

## Backend Feature Macros

`libgom.h` includes `gom-config.h`, which defines backend availability macros
for applications that need to compile conditionally against a specific driver.
Check the relevant `GOM_DATABASE_*` macros at build time before using
backend-specific code or includes:

- `GOM_DATABASE_SQLITE`
- `GOM_DATABASE_POSTGRESQL`

Use `#ifdef` or `#if defined(...)` around backend-specific code and includes.

## Data Access Paths

### Generic cursor path

Use builders to describe SQL-like operations without writing SQL directly:

- `GomQueryBuilder` for `SELECT`
- `GomInsertionBuilder` for `INSERT`
- `GomUpdateBuilder` for `UPDATE`
- `GomDeletionBuilder` for `DELETE`

Execute through [method@Gom.Repository.query] or [method@Gom.Repository.mutate], then iterate rows with [method@Gom.Cursor.next].

Query results still use [class@Gom.Cursor]. Mutation results now use
[class@Gom.MutationResult], a [iface@Gio.ListModel] of [class@Gom.Record]
snapshots.

### Entity mapper path

Define `GomEntity` subclasses and register them in a `GomRegistry`.

- [method@Gom.Entity.insert] builds and runs an insertion mutation from mapped properties.
- [method@Gom.Entity.update] builds and runs an update mutation using identity fields in `WHERE`.
- [method@Gom.Entity.delete] builds and runs a deletion mutation using identity fields in `WHERE`.
- [method@Gom.Session.persist] stages new entities in the unit of work.
- [method@Gom.Session.flush] writes staged changes without committing.
- [method@Gom.Entity.get_origin] reports whether an entity was constructed or materialized.
- [method@Gom.Entity.get_lifecycle] reports whether an entity is transient, pending, persistent, detached, or deleted.
- Relationship edits keep inverses synchronized, apply delete rules, and
  enforce the schema's cardinality and storage shape before the save reaches the backend.

Entities must be bound to a repository with [method@Gom.Entity.set_repository] before calling entity CRUD helpers.

## Mutation Result Contract

Mutation APIs return a [class@Gom.MutationResult] with one
[class@Gom.Record] per affected row.

- Each record includes `changes` (`INTEGER`) and `rowid`
  (`INTEGER` for insertion, `NULL` for update/delete in SQLite).
- Single-entity inserts back-fill generated identity fields on the entity when
  the backend returns a matching `rowid` or identity value.

## Next Reading

- [Quickstart](quickstart.html)
- [Migration Guide](migration-guide.html)
- [Sessions](session.html)
- [Relationship Semantics](relationships.html)
- [UI-facing Models](ui-models.html)
- [Mutation Builders and Entity CRUD](mutations-and-entity-crud.html)
- [Migration and Versioning](migrations-and-versioning.html)
- [Sample: Entity Mapper](examples-entity-mapper.html)
- [Sample: Cursor and Record Wrapper](examples-cursor-record.html)
- [Sample: SQLite Encryption Setup](examples-sqlite-encryption.html)
- [Sample: Python](examples-python.html)
