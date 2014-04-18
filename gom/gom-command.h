/* gom-command.h
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

#ifndef GOM_COMMAND_H
#define GOM_COMMAND_H

#include <glib-object.h>

#include "gom-cursor.h"

G_BEGIN_DECLS

#define GOM_TYPE_COMMAND            (gom_command_get_type())
#define GOM_COMMAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_COMMAND, GomCommand))
#define GOM_COMMAND_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_COMMAND, GomCommand const))
#define GOM_COMMAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_COMMAND, GomCommandClass))
#define GOM_IS_COMMAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_COMMAND))
#define GOM_IS_COMMAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_COMMAND))
#define GOM_COMMAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_COMMAND, GomCommandClass))

typedef struct _GomCommand        GomCommand;
typedef struct _GomCommandClass   GomCommandClass;
typedef struct _GomCommandPrivate GomCommandPrivate;

struct _GomCommand
{
   GObject parent;

   /*< private >*/
   GomCommandPrivate *priv;
};

struct _GomCommandClass
{
   GObjectClass parent_class;
};

gboolean     gom_command_execute          (GomCommand    *command,
                                           GomCursor    **cursor,
                                           GError       **error);
GType        gom_command_get_type         (void) G_GNUC_CONST;
void         gom_command_set_sql          (GomCommand    *command,
                                           const gchar   *sql);
gint         gom_command_get_param_index  (GomCommand    *command,
                                           const gchar   *param_name);
void         gom_command_reset            (GomCommand    *command);
void         gom_command_set_param        (GomCommand    *command,
                                           guint          param,
                                           const GValue  *value);
void         gom_command_set_param_double (GomCommand    *command,
                                           guint          param,
                                           gdouble        value);
void         gom_command_set_param_float  (GomCommand    *command,
                                           guint          param,
                                           gfloat         value);
void         gom_command_set_param_int    (GomCommand    *command,
                                           guint          param,
                                           gint           value);
void         gom_command_set_param_int64  (GomCommand    *command,
                                           guint          param,
                                           gint64         value);
void         gom_command_set_param_uint   (GomCommand    *command,
                                           guint          param,
                                           guint          value);
void         gom_command_set_param_uint64 (GomCommand    *command,
                                           guint          param,
                                           guint64        value);
void         gom_command_set_param_string (GomCommand    *command,
                                           guint          param,
                                           const gchar   *value);

G_END_DECLS

#endif /* GOM_COMMAND_H */
