/* gom-resource-macros.h
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GOM_RESOURCE_MACROS_H
#define GOM_RESOURCE_MACROS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_DEFINE_RESOURCE(TN, t_n, _C_)                        \
                                                                 \
G_DEFINE_TYPE(TN, t_n, GOM_TYPE_RESOURCE);                       \
                                                                 \
static void                                                      \
t_n##_class_init (TN##Class *resource_class)                     \
{                                                                \
    GObjectClass *object_class = G_OBJECT_CLASS(resource_class); \
                                                                 \
    object_class->set_property = gom_resource_set_property;      \
    object_class->get_property = gom_resource_get_property;      \
                                                                 \
    _C_;                                                         \
}                                                                \
                                                                 \
static void                                                      \
t_n##_init (TN *resource)                                        \
{                                                                \
}                                                                \

#define GOM_RESOURCE_HAS_MANY(_name, _nick, _desc, _type, ...)   \
                                                                 \
gom_resource_class_has_many(GOM_RESOURCE_CLASS(resource_class),  \
                            _name, _nick, _desc, _type,          \
                            ## __VA_ARGS__);

#define GOM_RESOURCE_HAS_A(_name, _nick, _desc, _type, ...)      \
                                                                 \
gom_resource_class_has_a(GOM_RESOURCE_CLASS(resource_class),     \
                         _name, _nick, _desc, _type,             \
                         ## __VA_ARGS__);

#define GOM_RESOURCE_BELONGS_TO(_name, _nick, _desc, _type, ...) \
                                                                 \
gom_resource_class_belongs_to(GOM_RESOURCE_CLASS(resource_class),\
                              _name, _nick, _desc, _type,        \
                              ## __VA_ARGS__);

#define GOM_RESOURCE_PROPERTY(_pspec, ...)                       \
                                                                 \
gom_resource_class_install_property(                             \
        GOM_RESOURCE_CLASS(resource_class), _pspec,              \
                           ## __VA_ARGS__);

#define GOM_RESOURCE_TABLE(_n)                                   \
                                                                 \
gom_resource_class_table(GOM_RESOURCE_CLASS(resource_class), _n);


G_END_DECLS

#endif /* GOM_RESOURCE_MACROS_H */
