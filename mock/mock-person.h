/* mock-person.h
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

#ifndef MOCK_PERSON_H
#define MOCK_PERSON_H

#include <gom/gom.h>

G_BEGIN_DECLS

#define MOCK_TYPE_PERSON            (mock_person_get_type())
#define MOCK_PERSON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOCK_TYPE_PERSON, MockPerson))
#define MOCK_PERSON_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOCK_TYPE_PERSON, MockPerson const))
#define MOCK_PERSON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MOCK_TYPE_PERSON, MockPersonClass))
#define MOCK_IS_PERSON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOCK_TYPE_PERSON))
#define MOCK_IS_PERSON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MOCK_TYPE_PERSON))
#define MOCK_PERSON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MOCK_TYPE_PERSON, MockPersonClass))

typedef struct _MockPerson        MockPerson;
typedef struct _MockPersonClass   MockPersonClass;
typedef struct _MockPersonPrivate MockPersonPrivate;

struct _MockPerson
{
	GomResource parent;

	/*< private >*/
	MockPersonPrivate *priv;
};

struct _MockPersonClass
{
	GomResourceClass parent_class;
};

GType mock_person_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MOCK_PERSON_H */
