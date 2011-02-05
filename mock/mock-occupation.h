/* mock/mock-occupation.h
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

#ifndef MOCK_OCCUPATION_H
#define MOCK_OCCUPATION_H

#include <gom/gom.h>

G_BEGIN_DECLS

#define MOCK_TYPE_OCCUPATION            (mock_occupation_get_type())
#define MOCK_OCCUPATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOCK_TYPE_OCCUPATION, MockOccupation))
#define MOCK_OCCUPATION_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOCK_TYPE_OCCUPATION, MockOccupation const))
#define MOCK_OCCUPATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  MOCK_TYPE_OCCUPATION, MockOccupationClass))
#define MOCK_IS_OCCUPATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOCK_TYPE_OCCUPATION))
#define MOCK_IS_OCCUPATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  MOCK_TYPE_OCCUPATION))
#define MOCK_OCCUPATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  MOCK_TYPE_OCCUPATION, MockOccupationClass))

typedef struct _MockOccupation        MockOccupation;
typedef struct _MockOccupationClass   MockOccupationClass;
typedef struct _MockOccupationPrivate MockOccupationPrivate;

struct _MockOccupation
{
	GomResource parent;

	/*< private >*/
	MockOccupationPrivate *priv;
};

struct _MockOccupationClass
{
	GomResourceClass parent_class;
};

GType mock_occupation_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* MOCK_OCCUPATION_H */
