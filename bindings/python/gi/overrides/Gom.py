# Gom.py
#
# Copyright (C) 2015 Mathieu Bridon <bochecha@daitauha.fr>
#
# This file is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This file is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


from ..module import get_introspection_module
from ..overrides import override


Gom = get_introspection_module('Gom')


__all__ = [
    'ResourceGroup',
    ]


class ResourceGroupOverride(Gom.ResourceGroup):
    def __len__(self):
        return self.get_count()

    def __getitem__(self, index):
        if index >= self.get_count():
            raise IndexError()

        return self.get_index(index)


ResourceGroup = override(ResourceGroupOverride)
