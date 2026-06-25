Title: Sessions

`GomSession` is the transaction-scoped API for workflows that need a stable read/write boundary.

Use a session when you want:

- one transaction for several reads and writes
- sticky materialization of entities within that scope
- a place to stage related changes before commit
- explicit `persist()`/`flush()` control for unit-of-work workflows
- the option to roll everything back if part of the workflow fails

## Why Use A Session

Repository-level operations are still the right tool for short, isolated queries and writes.

Sessions are for the cases where the sequence matters:

- load a row, make decisions based on it, then write multiple rows
- keep a parent row and child rows coherent while editing
- reuse the same in-memory entity if the same row is materialized more than once
- ensure a series of operations runs on one backend connection and one transaction

In other words, a session is the unit of work for stateful data access.

## Session Lifetime And SQLite

On SQLite, calling [method@Gom.Repository.begin_session] does more than group work:

- it opens `BEGIN IMMEDIATE TRANSACTION` for the session
- it acquires the repository write limiter for the life of that session
- it holds both until the session is committed or rolled back

That means creating additional sessions while one is still alive can block.
For session-backed UI models, this includes `GomQueryModel`: the model keeps a
strong reference to its session, so long-lived views can unintentionally
serialise write-bound workflows.

Treat sessions as short-lived transaction scopes. Keep them as UI-owned state
only when you are actively editing or need session identity semantics across
operations.

For read-only list views, prefer repository-backed models created with
[method@Gom.Repository.list_query] and inspect data through
[class@Gom.EntityListModel].

## How To Use It

Start one from a repository with [method@Gom.Repository.begin_session].

```c
g_autoptr(GomSession) session = NULL;
g_autoptr(GError) error = NULL;

session = dex_await_object (gom_repository_begin_session (repository), &error);
if (session == NULL)
  {
    g_warning ("Failed to begin session: %s", error->message);
    return;
  }
```

Run queries and mutations through the session:

```c
cursor1 = dex_await_object (gom_session_query (session, query), &error);
cursor2 = dex_await_object (gom_session_mutate (session, mutation), &error);

/* stage new entities */
gboolean ok = dex_await (gom_session_persist (session, GOM_ENTITY (entity)), &error);

/* force staged changes out before commit */
ok = dex_await (gom_session_flush (session), &error);
```

For simple entity work, use the session helpers:

```c
if (!dex_await (gom_session_insert_entity (session, GOM_ENTITY (note)), &error))
  return;

loaded = dex_await_object (gom_session_find_one (session, SAMPLE_TYPE_NOTE,
                                                 "id", id,
                                                 NULL),
                           &error);
```

`gom_session_find_one()` materializes through the session, so the identity map
still applies. Use `gom_session_query()` when you need ordering, projections,
joins, or cursor-level control. Use `persist()` and `flush()` directly when
staging multiple entities before one write.

When the work is complete, either commit or roll back:

```c
if (!dex_await_boolean (gom_session_commit (session), &error))
  g_warning ("Commit failed: %s", error->message);

/* or */
if (!dex_await_boolean (gom_session_rollback (session), &error))
  g_warning ("Rollback failed: %s", error->message);
```

## Sticky Entities

Within one session, `libgom` keeps an identity map.

If a row is materialized more than once during the session, the existing entity instance is reused instead of creating a duplicate object when queried through the session. That makes it easier to reason about object graphs and avoids losing local changes in memory.

Because the session reuses the same entity instance, property notifications are
visible immediately to every holder of that object, including any
`GomQueryModel` or `GomRelatedModel` that already contains it.

## Persist And Flush

`gom_session_persist()` stages a new or detached entity inside the session.
Session-managed transient entities are tracked as pending until they are
flushed or committed.

`gom_session_flush()` writes any pending session work to the backend without
ending the transaction. `gom_session_commit()` performs an implicit flush
before it commits.

## Entity State

`GomEntity` exposes two related pieces of state:

- `GomEntityOrigin` tells you whether the object was `CONSTRUCTED` in memory or `MATERIALIZED` from storage.
- `GomEntityLifecycle` tells you whether it is `TRANSIENT`, `PENDING`, `PERSISTENT`, `DETACHED`, or `DELETED`.

The important session cases are:

- Newly constructed objects start as `CONSTRUCTED` + `TRANSIENT`.
- Cursor materialization without a session produces `MATERIALIZED` + `DETACHED`.
- Cursor materialization with a session produces `MATERIALIZED` + `PERSISTENT` and registers the entity in the identity map.
- After an insert back-fills a generated identity, `gom_entity_rekey_session_identity()` updates the session identity map so the entity stays reachable under the new key.

The sticky behavior ends when the session is committed or rolled back.

For UI layers, this means the model is stable in identity but not
snapshot-isolated: entity edits are immediate, while list membership and
ordering refresh after the session emits `changed` and the model reruns its
query.

## When Not To Use It

If you only need a single query, a single insert, or a simple update, use the repository APIs directly.

Sessions add structure and correctness for multi-step workflows, but they also add lifecycle management. Keep them scoped to the part of the code that actually needs transactional consistency.

## Related API

- [method@Gom.Repository.begin_session]
- [method@Gom.Session.query]
- [method@Gom.Session.mutate]
- [method@Gom.Session.persist]
- [method@Gom.Session.flush]
- [method@Gom.Session.commit]
- [method@Gom.Session.rollback]
