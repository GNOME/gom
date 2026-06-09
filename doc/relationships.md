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
