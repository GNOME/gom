/* mock/mock-gender.c
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

#include "mock-gender.h"

GType
mock_gender_get_type (void)
{
	static gsize initialized = FALSE;
	static GType type_id = 0;
	static const GEnumValue values[] = {
		{ MOCK_GENDER_UNKNOWN, "MOCK_GENDER_UNKNOWN", "UNKNOWN" },
		{ MOCK_GENDER_MALE, "MOCK_GENDER_MALE", "MALE" },
		{ MOCK_GENDER_FEMALE, "MOCK_GENDER_FEMALE", "FEMALE" },
		{ 0 }
	};

	if (g_once_init_enter(&initialized)) {
		type_id = g_enum_register_static("MockGender", values);
		g_once_init_leave(&initialized, TRUE);
	}

	return type_id;
}
