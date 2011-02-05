/* mock/mock-occupation.c
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

#include "mock-occupation.h"

GOM_DEFINE_RESOURCE(MockOccupation, mock_occupation,
                    GOM_RESOURCE_TABLE("occupations");
                    GOM_RESOURCE_PROPERTY(g_param_spec_uint64("id",
                                                              _("Identifier"),
                                                              _("The occupational identifier."),
                                                              0,
                                                              G_MAXUINT64,
                                                              0,
                                                              G_PARAM_READWRITE),
                                           "key", TRUE,
                                           "serial", TRUE,
                                           NULL);
                    GOM_RESOURCE_PROPERTY(g_param_spec_string("name",
                                                              _("Name"),
                                                              _("The occupation name."),
                                                              NULL,
                                                              G_PARAM_READWRITE),
                                           "unique", TRUE,
                                           NULL);
                    GOM_RESOURCE_PROPERTY(g_param_spec_string("industry",
                                                              _("Industry"),
                                                              _("The occupational industry."),
                                                              NULL,
                                                              G_PARAM_READWRITE),
                                           NULL););
