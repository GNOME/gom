Title: UI-Facing Models

`libgom` exposes query and relationship results as `GListModel` instances so
GTK views can bind to them directly.

This is the runtime behavior for list views, detail panes, and other UI layers
that need data in model form.

## `GomQueryModel`

Use `GomQueryModel` when you want a live query-backed list model.

- create it from a `GomSession`, entity type, optional filter, and optional ordering
- call [method@Gom.QueryModel.reload] to populate it initially
- call [method@Gom.QueryModel.refresh] to rerun the query when you know the session changed
- watch the `loading` property while a refresh is in flight
- bind `items-changed` to incremental row updates

The model is session-scoped. It reacts to the session's `changed` signal and
reruns the query against that session.

That means:

- inserts become visible after the session records a change
- updates stay visible immediately on the shared entity instance, and the list
  shape or ordering updates after a refresh if membership changed
- deletes disappear after refresh when the deleted row no longer matches the query

`GomQueryModel` is stable in identity, not snapshot-isolated. If the same entity
instance is already materialized in the session, the list and the app will share
that object.

## `GomEntityListModel`

Use `GomEntityListModel` for long-lived read-only list views.

- get one with [method@Gom.Repository.list_query] for repository-backed reads
- get one with [method@Gom.Session.list_query] for session-scoped reads
- watch `loading` while pages are loaded lazily
- handle rows as [class@Gom.EntityListItem] wrappers and read the real entity via
  [method@Gom.EntityListItem.dup_item]

`GomEntityListModel` is often a better match for UI lists because it does not
require a long-lived session for read-only workflows.

## Model Choice

Use `GomQueryModel` when you already have a session-scoped workflow and want
the model to reflect that session’s `changed` signal and identity map.

Use `GomEntityListModel` (via [method@Gom.Repository.list_query]) for
long-lived read-only UI lists where you want pagination and do not need session
liveness.

## `GomRelatedModel`

Use `GomRelatedModel` for relationship-backed collections.

- it is the same `GListModel` pattern for a to-many relationship on a loaded entity
- it exposes `loading`, `reload()`, and `refresh()`
- it updates incrementally through `items-changed`

This is the right choice for editing child collections and related-object lists.

## Stable Views

The intended UI contract is:

- entity property notifications are immediate because the session reuses the same object instance
- list membership and ordering are updated by model refresh, not by manual row diffing
- a GTK view can bind directly to the model without rebuilding its own cache

If you need commit-isolated display state, use a different session or detached
copies.

## Related API

- [Sessions](session.html)
- [Mutation Builders and Entity CRUD](mutations-and-entity-crud.html)
- [Overview](overview.html)
