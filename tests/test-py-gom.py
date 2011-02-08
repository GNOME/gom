#!/usr/bin/env python

"""
A simple test case for Gom using Python and GObject introspection.
"""

from gi.repository import GObject
from gi.repository import Gom

class Person(Gom.Resource):
    __gproperties__ = {
        'name': (GObject.TYPE_STRING, 'Name', 'The persons name.', None, GObject.PARAM_READWRITE),
    }

    __gtype_name__ = 'Person'

    name = None

    def do_get_property(self, prop):
        if prop.name == 'name':
            return self.name
        else:
            raise AttributeError, 'Unknown property %s' % prop.name

    def do_set_property(self, prop, value):
        if prop.name == 'name':
            self.name = value
        else:
            raise AttributeError, 'Unknown property %s' % prop.name

def testSqlite():
    adapter = Gom.AdapterSqlite()
    adapter.load_from_file('test-py-gom.db')
    person = Person(adapter=adapter, is_new=True, name="Christian")
    print person.is_new()
    person.save()
    print person.is_new()
    adapter.close()

def runTests():
    testSqlite()

if __name__ == '__main__':
    runTests()
