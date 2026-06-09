Title: Sample: Python Example

`libgom` installs a small PyGObject override, `Gom.py`, next to the
introspection data. It keeps the `gi.repository.Gom` API shape intact and only
adds mapping metadata where Python needs a place to write it:

- class attributes such as `__gom_relation__` and `__gom_identity__` configure
  the entity class before registration
- `@Gom.Mapped` marks `GObject.Property` values as persistent columns
- `await Gom.Repository.new(driver, registry, migrator)` opens the repository
  and runs migration before returning it.

The only event-loop requirement is the same one used by other Dex-heavy
Python applications: use PyGObject's GLib asyncio integration and import
`Dex.py` so `Dex.Future` can be awaited.

## Minimal Application

```python
#!/usr/bin/env python3

import asyncio
from pathlib import Path

import gi
from gi.events import GLibEventLoopPolicy

gi.require_version("Dex", "1")
gi.require_version("Gom", "2")

from gi.repository import Dex, GLib, GObject, Gom


class Note(Gom.Entity):
    __gtype_name__ = "ExampleNote"
    __gom_relation__ = "notes"
    __gom_identity__ = "id"

    id = Gom.Mapped(
        GObject.Property(
            type=int,
            default=0,
            flags=GObject.ParamFlags.READWRITE,
        )
    )

    @Gom.Mapped(nonnull=True, search=Gom.SearchFlags.INDEXED)
    @GObject.Property(type=str)
    def title(self):
        return getattr(self, "_title", None)

    @title.setter
    def title(self, value):
        self._title = value


async def main():
    db_path = Path.home() / ".local" / "share" / "gom-notes.db"
    db_path.parent.mkdir(parents=True, exist_ok=True)

    driver = Gom.Driver.open(GLib.filename_to_uri(str(db_path)))

    builder = Gom.RegistryBuilder.new()
    builder.add_entity_type(Note)
    registry = builder.build()

    migrator = None

    repository = await Gom.Repository.new(driver, registry, migrator)

    note = Note(title="Hello from Python")
    await repository.insert_entity(note)

    loaded = await repository.find_one_with_properties(Note, ["id"], [note.id])
    print(f"{loaded.id}: {loaded.title}")


if __name__ == "__main__":
    asyncio.set_event_loop_policy(GLibEventLoopPolicy())
    Dex.init()

    loop = asyncio.get_event_loop_policy().get_event_loop()
    loop.run_until_complete(main())
```

## What Matters

The Python class is still a normal GObject type. Define properties with
`GObject.Property`, then mark the properties that should be stored with
`Gom.Mapped`.

Class attributes apply the same metadata a C class would set in `class_init`:
relation name, identity field, entity version, and optional discriminator
metadata. The metadata is applied when the type is registered with
`Gom.RegistryBuilder.add_entity_type()`, immediately before libgom snapshots
the entity type.

`Gom.Mapped` can be used with normal PyGObject property patterns:

```python
@Gom.Mapped
@GObject.Property(type=str)
def title(self):
    return self._title

@title.setter
def title(self, value):
    self._title = value
```

After a repository insert, SQLite back-fills the generated `id` property onto
the entity. Loaded entities are regular `Note` instances, so the rest of your
application can use normal Python and GObject property access.
