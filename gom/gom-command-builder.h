/* gom-command-builder.h
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

#ifndef GOM_COMMAND_BUILDER_H
#define GOM_COMMAND_BUILDER_H

#include <glib-object.h>

#include "gom-command.h"
#include "gom-resource.h"

G_BEGIN_DECLS

#define GOM_TYPE_COMMAND_BUILDER            (gom_command_builder_get_type())
#define GOM_COMMAND_BUILDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_COMMAND_BUILDER, GomCommandBuilder))
#define GOM_COMMAND_BUILDER_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_COMMAND_BUILDER, GomCommandBuilder const))
#define GOM_COMMAND_BUILDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_COMMAND_BUILDER, GomCommandBuilderClass))
#define GOM_IS_COMMAND_BUILDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_COMMAND_BUILDER))
#define GOM_IS_COMMAND_BUILDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_COMMAND_BUILDER))
#define GOM_COMMAND_BUILDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_COMMAND_BUILDER, GomCommandBuilderClass))

typedef struct _GomCommandBuilder        GomCommandBuilder;
typedef struct _GomCommandBuilderClass   GomCommandBuilderClass;
typedef struct _GomCommandBuilderPrivate GomCommandBuilderPrivate;

struct _GomCommandBuilder
{
   GObject parent;

   /*< private >*/
   GomCommandBuilderPrivate *priv;
};

struct _GomCommandBuilderClass
{
   GObjectClass parent_class;
};

GomCommand *gom_command_builder_build_count  (GomCommandBuilder *builder);
GomCommand *gom_command_builder_build_delete (GomCommandBuilder *builder);
GomCommand *gom_command_builder_build_select (GomCommandBuilder *builder);
GomCommand *gom_command_builder_build_insert (GomCommandBuilder *builder,
                                              GomResource       *resource);
GomCommand *gom_command_builder_build_update (GomCommandBuilder *builder,
                                              GomResource       *resource);
GType       gom_command_builder_get_type     (void) G_GNUC_CONST;

G_END_DECLS

#endif /* GOM_COMMAND_BUILDER_H */
