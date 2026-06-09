# Gom.py
#
# Copyright 2026 Christian Hergert <christian@sourceandstack.com>
#
# This library is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: LGPL-2.1-or-later

from gi.repository import Gom

__all__ = [
    "Mapped",
    "configure_entity_type",
    "mapped",
]

_registry_builder_add_entity_type = Gom.RegistryBuilder.add_entity_type


def _set_if_not_none(setter, entity_class, value):
    if value is not None:
        setter(entity_class, value)


def _iter_identity(identity):
    if identity is None:
        return None
    if isinstance(identity, str):
        return [identity]
    return list(identity)


def _apply_mapped_property(entity_class, name, options):
    Gom.EntityClass.property_set_mapped(entity_class, name, True)
    Gom.EntityClass.property_set_version_added(entity_class, name, options["version"])

    if options["version_removed"] is not None:
        Gom.EntityClass.property_set_version_removed(entity_class,
                                                     name,
                                                     options["version_removed"])
    if options["field"] is not None:
        Gom.EntityClass.property_set_field_name(entity_class,
                                                name,
                                                options["field"])

    if options["nonnull"]:
        Gom.EntityClass.property_set_nonnull(entity_class, name, True)
    if options["unique"]:
        Gom.EntityClass.property_set_unique(entity_class, name, True)
    if options["search"] is not None:
        Gom.EntityClass.property_set_search_flags(entity_class, name, options["search"])
    if options["reference"] is not None:
        ref_table, ref_field = options["reference"]
        Gom.EntityClass.property_set_reference(entity_class, name, ref_table, ref_field)


def _get_mapped_options(value):
    options = getattr(value, "_gom_mapped", None)

    if options is None:
        fget = getattr(value, "fget", None)
        options = getattr(fget, "_gom_mapped", None)

    return options


def _configure_entity_type(entity_type):
    relation = getattr(entity_type, "__gom_relation__", None)
    identity = getattr(entity_type, "__gom_identity__", None)
    version = getattr(entity_type, "__gom_version__", 1)

    if relation is not None:
        Gom.EntityClass.set_relation(entity_type, relation)
        Gom.EntityClass.set_version_added(entity_type, version)

    _set_if_not_none(Gom.EntityClass.set_version_removed,
                     entity_type, getattr(entity_type, "__gom_version_removed__", None))
    _set_if_not_none(Gom.EntityClass.set_discriminator_field,
                     entity_type, getattr(entity_type, "__gom_discriminator_field__", None))
    _set_if_not_none(Gom.EntityClass.set_discriminator_value,
                     entity_type, getattr(entity_type, "__gom_discriminator_value__", None))

    identity_fields = _iter_identity(identity)
    if identity_fields is not None:
        if len(identity_fields) == 1:
            Gom.EntityClass.set_identity_field(entity_type, identity_fields[0])
        else:
            Gom.EntityClass.set_identity_fields(entity_type, identity_fields)

    for name, value in vars(entity_type).items():
        options = _get_mapped_options(value)
        if options is not None:
            _apply_mapped_property(entity_type, name.replace("_", "-"), options)


def _add_entity_type(self, entity_type):
    _configure_entity_type(entity_type)
    return _registry_builder_add_entity_type(self, entity_type)


Gom.RegistryBuilder.add_entity_type = _add_entity_type


def configure_entity_type(entity_type):
    """Apply Python mapping metadata to a Gom.Entity subclass."""
    _configure_entity_type(entity_type)
    return entity_type


def Mapped(prop=None, *, version=1, field=None, nonnull=False, unique=False,
           search=None, reference=None, version_removed=None):
    """Mark a GObject.Property as persistent libgom entity data."""
    options = {
        "version": version,
        "field": field,
        "nonnull": nonnull,
        "unique": unique,
        "search": search,
        "reference": reference,
        "version_removed": version_removed,
    }

    def decorator(value):
        value._gom_mapped = options
        return value

    if prop is None:
        return decorator

    return decorator(prop)


mapped = Mapped
