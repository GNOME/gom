Title: Mutation Builders and Entity CRUD

This page documents write-operation contracts for builder-based mutations and entity mapper helpers.

## Builder APIs

All mutation builders require a resolvable target relation.

You can set the target by either:

- entity type (`*_set_target_entity_type()`), or
- relation name (`*_set_target_relation()`).

If both are absent, `*_build()` fails with `G_IO_ERROR_INVALID_ARGUMENT`.

### `GomInsertionBuilder`

Minimum requirements:

- At least one column.
- At least one row.
- Every row must have the same number of values as columns.
- Every column expression must be a field expression.

### `GomUpdateBuilder`

Minimum requirements:

- At least one assignment.
- Assignment targets must be field expressions.
- Assignment column/value arrays must remain aligned.

Optional clauses:

- [method@Gom.UpdateBuilder.set_filter] for `WHERE`.
- [method@Gom.UpdateBuilder.set_limit] to limit affected rows.

### `GomDeletionBuilder`

Minimum requirements:

- Target entity type or target relation.

Optional clauses:

- [method@Gom.DeletionBuilder.set_filter] for `WHERE`.
- [method@Gom.DeletionBuilder.set_limit] to limit affected rows.

## Mutation Execution Result

[method@Gom.Repository.mutate] resolves to a
[class@Gom.MutationResult], which is a [iface@Gio.ListModel] of
[class@Gom.Record] rows.

Backend notes:

- SQLite backends return one record per inserted row, with `changes` and
  `last_insert_rowid()` captured after each insert.
- Update and delete return a single record with `changes` and `NULL` for
  `rowid` on SQLite.
- PostgreSQL backends use the same `MutationResult` model, but do not depend
  on SQLite-style `rowid` semantics in backend-specific code.

Read fields from each record with [method@Gom.Record.get_column_by_name].

## Easy Entity Helpers

Use repository helpers for short, common entity operations:

```c
if (!dex_await (gom_repository_insert_entity (repository, GOM_ENTITY (note)), &error))
  return;

loaded = dex_await_object (gom_repository_find_one (repository, SAMPLE_TYPE_NOTE,
                                                    "id", id,
                                                    NULL),
                           &error);
```

[method@Gom.Repository.insert_entity] binds the entity to the repository and
inserts it. The future resolves to `TRUE` on success. Use
[method@Gom.Entity.insert] when you need the [class@Gom.MutationResult].

[method@Gom.Repository.find_one] finds the first entity matching equality
predicates over mapped properties. Additional property/value pairs are combined
with `AND`, so composite identities can be expressed directly:

```c
loaded = dex_await_object (gom_repository_find_one (repository, SAMPLE_TYPE_NOTE,
                                                    "account-id", account_id,
                                                    "slug", slug,
                                                    NULL),
                           &error);
```

If no row matches, the future resolves to `NULL`. Use
[method@Gom.Repository.query] with [struct@Gom.QueryBuilder] when you need
ordering, projections, joins, grouping, custom limits, or cursor-level control.

## Entity CRUD Contract

[method@Gom.Entity.insert], [method@Gom.Entity.update], and [method@Gom.Entity.delete] are convenience wrappers that translate entity state to mutation models.

### Preconditions

- Entity must be bound to a repository ([method@Gom.Entity.set_repository]).
- Entity class should define identity fields for update/delete.
- Entity class should explicitly mark each persisted property with
  `gom_entity_class_property_set_mapped (..., TRUE)`.
- Identity values must resolve through `GomEntityClass.dup_identity_value`.
- Default `GomEntityClass.dup_identity_value` treats default-valued identity properties as unset.

If preconditions are not met, operations fail with `G_IO_ERROR_INVALID_ARGUMENT`.

### Insert behavior

- Reads mapped readable properties.
- Identity properties are skipped when `GomEntityClass.dup_identity_value` returns `NULL`.
- Byte transform ([method@Gom.EntityClass.property_set_byte_transform]) is applied before storage and can reject invalid serialized values with `GError`.
- When the backend returns an inserted rowid, [method@Gom.Entity.insert] writes
  it back to the entity's identity property. This keeps `id INTEGER PRIMARY KEY`
  entities synchronized after insert.
- If the entity is session-managed, insert also rekeys the session identity map
  after the new identity is written back.
- If the entity was staged with [method@Gom.Session.persist], the session will
  remove it from the pending set once the insert succeeds.
- If the entity type declares an identity field but does not expose a writable
  property for it, insert leaves the row result untouched and does not attempt
  a back-fill.

### Update behavior

- Builds `WHERE` from identity fields.
- Updates mapped properties that are both readable and writable.
- Identity fields are excluded from `SET` assignments.
- Uses `LIMIT 1`.

### Delete behavior

- Builds `WHERE` from identity fields.
- Uses `LIMIT 1`.

## Relationship Behavior

Entity CRUD participates in relationship maintenance when the entity class has
declared relationship metadata.

- inverse relationships are updated when a relationship property changes
- delete rules such as `nullify`, `cascade`, `deny`, and `no action` are applied
- join-table relationships are validated and maintained according to the schema

See [Relationship Semantics](relationships.html) for the full graph-integrity
contract.

## Error Cases to Handle

Callers should expect and handle errors such as:

- Entity not bound to repository.
- Entity type has no identity fields.
- Identity field is missing on the class or unset on the instance.
- Unsupported backend binding/value conversion for a property.
- Registry validation failures (unknown entity/property/field mapping).
