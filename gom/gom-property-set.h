/* gom-property-set.h
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

#ifndef GOM_PROPERTY_SET_H
#define GOM_PROPERTY_SET_H

#include <glib-object.h>

#include "gom-property.h"

G_BEGIN_DECLS

#define GOM_TYPE_PROPERTY_SET (gom_property_set_get_type())

typedef struct _GomPropertySet GomPropertySet;

GomPropertySet* gom_property_set_dup      (GomPropertySet  *set);
GomProperty*    gom_property_set_find     (GomPropertySet  *set,
                                           const gchar     *name);
GomProperty*    gom_property_set_findq    (GomPropertySet  *set,
                                           GQuark           name);
GType           gom_property_set_get_type (void) G_GNUC_CONST;
GomPropertySet* gom_property_set_new      (GomProperty     *first_property,
                                           ...) G_GNUC_NULL_TERMINATED;
GomPropertySet* gom_property_set_newv     (guint            n_properties,
                                           GomProperty    **properties);
GomPropertySet* gom_property_set_ref      (GomPropertySet  *set);
void            gom_property_set_unref    (GomPropertySet  *set);
guint           gom_property_set_length   (GomPropertySet  *set);
GomProperty*    gom_property_set_get_nth  (GomPropertySet  *set,
                                           guint            index);

G_END_DECLS

#endif /* GOM_PROPERTY_SET_H */

