#!/usr/bin/gjs

const Lang = imports.lang;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const GIRepository = imports.gi.GIRepository;
const Gom = imports.gi.Gom;
const System = imports.system;

const INT32_MAX = (2147483647);

const ItemClass = new Lang.Class({
    Name: 'Item',
    Extends: Gom.Resource,

    Properties: {
        'id':  GObject.ParamSpec.int('id', 'ID',
                                     'An ID', GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE | GObject.ParamFlags.CONSTRUCT,
                                     0, INT32_MAX, 0),
        'url': GObject.ParamSpec.string('url', 'URL',
					'A URL',
					GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE | GObject.ParamFlags.CONSTRUCT,
					''),
    },

    _init: function(params) {
        this.parent(params);

        Gom.Resource.set_table.call(this, 'items');
        Gom.Resource.set_primary_key.call(this, 'id');
    },

    _instance_init: function() {
    },
});

// Open
let adapter = new Gom.Adapter;
adapter.open_sync('/tmp/gom-js-test.db');
let repository = new Gom.Repository({adapter: adapter});

let item = new ItemClass({ 'repository': repository });
//item.url = 'http://www.gnome.org';
item.url = 'http://www.gnome.org';
print (item.id);
item.id = 0;
print (item.url);

// Migrate
let object_types = [ ItemClass ];
repository.automatic_migrate_sync(2, object_types);

// Add item
let ret = item.save_sync();
print("New item ID:", item.id, "URL:", item.url);

// Close
adapter.close_sync();

// Open
let adapter = new Gom.Adapter;
adapter.open_sync('/tmp/gom-js-test.db');
let repository = new Gom.Repository({adapter: adapter});

// Find item
let filter = Gom.Filter.new_eq(ItemClass, "id", 1);
let found_item = repository.find_one_sync(ItemClass, filter);

print (found_item.url);

// Close
adapter.close_sync();
