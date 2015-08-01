#!/usr/bin/python3


from gi.types import GObjectMeta
from gi.repository import GLib
from gi.repository import GObject
from gi.repository import Gom


# Need a metaclass until we get something like _gclass_init_
#     https://bugzilla.gnome.org/show_bug.cgi?id=701843
class ItemResourceMeta(GObjectMeta):
    def __init__(cls, name, bases, dct):
        super(ItemResourceMeta, cls).__init__(name, bases, dct)
        cls.set_table("items")
        cls.set_primary_key("id")
        cls.set_notnull("name")


class ItemResource(Gom.Resource, metaclass=ItemResourceMeta):
    id = GObject.Property(type=int)
    name = GObject.Property(type=str)


if __name__ == '__main__':
    # Connect to the database
    adapter = Gom.Adapter()
    adapter.open_sync(":memory:")

    # Create the table
    repository = Gom.Repository(adapter=adapter)
    repository.automatic_migrate_sync(1, [ItemResource])

    # Insert an item
    item = ItemResource(repository=repository, name="item1")
    item.save_sync()

    # Fetch the item back
    item = repository.find_one_sync(ItemResource, None)
    assert item.id == 1
    assert item.name == 'item1'

    # Insert a new item
    item = ItemResource(repository=repository, name="item2")
    item.save_sync()

    # Fetch them all with a None filter, ordered by name
    names = ['item2', 'item1']
    sorting = Gom.Sorting(ItemResource, "name", Gom.SortingMode.DESCENDING)
    group = repository.find_sorted_sync(ItemResource, None, sorting)
    count = len(group)
    assert count == 2

    group.fetch_sync(0, count)
    for i, item in enumerate(group):
        assert item.name == names[i]

    # Fetch only one of them with a filter, asynchronously
    loop = GLib.MainLoop()

    def fetch_cb(group, result, user_data):
        group.fetch_finish(result)

        item = group[0]
        assert item.name == "item2"

        # Close the database
        adapter.close_sync()

        loop.quit()

    def find_cb(repository, result, user_data):
        group = repository.find_finish(result)

        count = len(group)
        assert count == 1

        group.fetch_async(0, count, fetch_cb, None)

    filter = Gom.Filter.new_eq(ItemResource, "name", "item2")
    group = repository.find_async(ItemResource, filter, find_cb, None)

    loop.run()
