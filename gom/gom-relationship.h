/* gom-relationship.h
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

#ifndef GOM_RELATIONSHIP_H
#define GOM_RELATIONSHIP_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum   _GomRelation     GomRelation;
typedef struct _GomRelationship GomRelationship;

enum _GomRelation
{
	GOM_RELATION_ONE_TO_ONE = 1,
	GOM_RELATION_ONE_TO_MANY,
	GOM_RELATION_MANY_TO_ONE,
	GOM_RELATION_MANY_TO_MANY,
};

struct _GomRelationship
{
	GomRelation relation;
};

G_END_DECLS

#endif /* GOM_RELATIONSHIP_H */
