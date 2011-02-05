/* mock/mock-gender.h
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

#ifndef MOCK_GENDER_H
#define MOCK_GENDER_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum _MockGender MockGender;

enum _MockGender
{
	MOCK_GENDER_UNKNOWN = 0,
	MOCK_GENDER_MALE    = 1,
	MOCK_GENDER_FEMALE  = 2,
};

#define MOCK_TYPE_GENDER (mock_gender_get_type())

GType mock_gender_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MOCK_GENDER_H */
