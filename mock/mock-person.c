/* mock-person.c
 *
 * Copyright (C) 2011 Christian Hergert <chris@dronelabs.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "mock-gender.h"
#include "mock-occupation.h"
#include "mock-person.h"

GOM_DEFINE_RESOURCE(MockPerson, mock_person,
                    GOM_RESOURCE_TABLE("persons");
                    GOM_RESOURCE_HAS_MANY("friends",
                                          _("Friends"),
                                          _("The persons friends."),
                                          MOCK_TYPE_PERSON,
                                          NULL);
                    GOM_RESOURCE_BELONGS_TO("occupation",
                                            _("Occupation"),
                                            _("The persons occupation."),
                                            MOCK_TYPE_OCCUPATION,
                                            NULL);
                    GOM_RESOURCE_PROPERTY(g_param_spec_uint64("id",
                                                              _("Identifier"),
                                                              _("The persons identifier."),
                                                              0,
                                                              G_MAXUINT64,
                                                              0,
                                                              G_PARAM_READWRITE),
                                          "key", TRUE,
                                          "serial", TRUE,
                                          NULL);
                    GOM_RESOURCE_PROPERTY(g_param_spec_string("name",
                                                              _("Name"),
                                                              _("The persons name."),
                                                              NULL,
                                                              G_PARAM_READWRITE),
                                          NULL);
                    GOM_RESOURCE_PROPERTY(g_param_spec_enum("gender",
                                                            _("Gender"),
                                                            _("The persons gender"),
                                                            MOCK_TYPE_GENDER,
                                                            MOCK_GENDER_UNKNOWN,
                                                            G_PARAM_READWRITE),
                                          NULL););
