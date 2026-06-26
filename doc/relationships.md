Title: Relationship Semantics

`libgom` relationship handling keeps object graphs coherent, not just loadable.

This is the behavior that makes entity graphs safe for forms, editors, and
cascading workflows.

## What libgom Maintains

When you mutate a relationship property on a `GomEntity`, the library keeps the
other side consistent where the schema declares an inverse. That includes:

- one-to-one relationships
- one-to-many relationships
- many-to-many relationships
- self-referential relationship graphs

The intent is that you can update one side of the graph and trust the inverse
to stay aligned without hand-written fixup code.

## One-to-One Modeling

Use this when one object should have a single companion object (for example one
`NewsFeed` has one `NewsIcon`).

In this pattern, the `NewsIcon` row keeps the FK (`feed_id`) that points back to
`NewsFeed`. So you define the relation on the feed side as:

- `feed.icon` (has-one) using `gom_entity_class_add_one_to_one()`
- `icon.feed` (belongs-to) using `gom_entity_class_add_many_to_one()`

`gom_entity_class_add_one_to_one()` is the API-friendly way to express “at most
one related row” for this shape.

It is still important to enforce uniqueness in the database too (for example
with a unique index on `feed_id`) so your schema cannot accidentally store two
icons for the same feed.

### Both Directions

If you want both objects to reference each other (`feed.icon` and `icon.feed`), you
still define them as a pair:

- `feed.icon` with `gom_entity_class_add_one_to_one()`
- `icon.feed` with `gom_entity_class_add_many_to_one()`

That gives you a bidirectional one-to-one navigation while matching libgom’s
storage model (the FK is on `NewsIcon`). In practice, this is the correct pattern
for “one object has one child, and that child belongs to one parent.”

## Delete Rules

Relationship delete behavior is explicit and schema-driven:

- `nullify`: clear foreign-key references on dependents
- `cascade`: delete dependent rows
- `deny`: reject the delete when dependents exist
- `no action`: leave storage unchanged and rely on backend constraints

Pick the rule that matches the ownership semantics of the relation.

## Integrity And Validation

`libgom` validates relationship metadata early so bad schemas fail before
runtime:

- inverse relationships must resolve
- cardinality must match the declared storage shape
- optionality and storage mapping must be coherent
- join-table field shapes must line up with the target identity fields
- broken many-to-many mappings are rejected during registry build or startup

That gives you predictable behavior before data is ever loaded.

## Session Interaction

Relationship edits are tracked through the session when the entity is
session-managed. That keeps relationship changes coherent with flush/commit
workflows and lets UI-facing models refresh from one session-level change
signal.

## Related API

- [Sessions](session.html)
- [UI-facing Models](ui-models.html)
- [Mutation Builders and Entity CRUD](mutations-and-entity-crud.html)
