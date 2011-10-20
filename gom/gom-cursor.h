/* gom-cursor.h
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

#ifndef GOM_CURSOR_H
#define GOM_CURSOR_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GOM_TYPE_CURSOR            (gom_cursor_get_type())
#define GOM_CURSOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_CURSOR, GomCursor))
#define GOM_CURSOR_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOM_TYPE_CURSOR, GomCursor const))
#define GOM_CURSOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GOM_TYPE_CURSOR, GomCursorClass))
#define GOM_IS_CURSOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOM_TYPE_CURSOR))
#define GOM_IS_CURSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GOM_TYPE_CURSOR))
#define GOM_CURSOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GOM_TYPE_CURSOR, GomCursorClass))

typedef struct _GomCursor        GomCursor;
typedef struct _GomCursorClass   GomCursorClass;
typedef struct _GomCursorPrivate GomCursorPrivate;

struct _GomCursor
{
   GObject parent;

   /*< private >*/
   GomCursorPrivate *priv;
};

struct _GomCursorClass
{
   GObjectClass parent_class;
};

const gchar *gom_cursor_get_column_name   (GomCursor   *cursor,
                                           guint        column);
guint        gom_cursor_get_n_columns     (GomCursor   *cursor);
GType        gom_cursor_get_type          (void) G_GNUC_CONST;
void         gom_cursor_get_column        (GomCursor   *cursor,
                                           guint        column,
                                           GValue      *value);
gboolean     gom_cursor_get_column_boolean(GomCursor   *cursor,
                                           guint        column);
gdouble      gom_cursor_get_column_double (GomCursor   *cursor,
                                           guint        column);
gfloat       gom_cursor_get_column_float  (GomCursor   *cursor,
                                           guint        column);
gint         gom_cursor_get_column_int    (GomCursor   *cursor,
                                           guint        column);
gint64       gom_cursor_get_column_int64  (GomCursor   *cursor,
                                           guint        column);
const gchar *gom_cursor_get_column_string (GomCursor   *cursor,
                                           guint        column);
guint        gom_cursor_get_column_uint   (GomCursor   *cursor,
                                           guint        column);
guint64      gom_cursor_get_column_uint64 (GomCursor   *cursor,
                                           guint        column);
gboolean     gom_cursor_next              (GomCursor   *cursor);

G_END_DECLS

#endif /* GOM_CURSOR_H */
