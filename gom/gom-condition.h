/* gom-condition.h
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

#ifndef GOM_CONDITION_H
#define GOM_CONDITION_H

#include <glib-object.h>

#include "gom-property.h"

G_BEGIN_DECLS

#define GOM_TYPE_CONDITION  (gom_condition_get_type())
#define GOM_CONDITION_AND   (gom_condition_and_quark())
#define GOM_CONDITION_OR    (gom_condition_or_quark())
#define GOM_CONDITION_EQUAL (gom_condition_equal_quark())

typedef struct _GomCondition GomCondition;
typedef enum   _GomOperator  GomOperator;

struct _GomCondition
{
	volatile gint ref_count;
	GQuark oper;
	union {
		struct {
			GomProperty *property;
			GValue value;
		} equality;
		struct {
			GomCondition *left;
			GomCondition *right;
		} boolean;
	} u;
};

GType         gom_condition_get_type    (void) G_GNUC_CONST;
GomCondition* gom_condition_and         (GomCondition *left,
                                         GomCondition *right);
GomCondition* gom_condition_equal       (GomProperty  *property,
                                         const GValue *value);
GomCondition* gom_condition_or          (GomCondition *left,
                                         GomCondition *right);
GomCondition* gom_condition_ref         (GomCondition *condition);
void          gom_condition_unref       (GomCondition *condition);
gboolean      gom_condition_is_a        (GomCondition *condition,
                                         GQuark        oper);
GQuark        gom_condition_and_quark   (void) G_GNUC_CONST;
GQuark        gom_condition_equal_quark (void) G_GNUC_CONST;
GQuark        gom_condition_or_quark    (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GOM_CONDITION_H */
