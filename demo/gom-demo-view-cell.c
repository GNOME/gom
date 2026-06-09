/* gom-demo-view-cell.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libgom.h>

#include "gom-demo-view-cell.h"

struct _GomDemoViewCell
{
  GtkBox             parent_instance;
  GomRecordListItem *item;
  char              *field;
};

G_DEFINE_FINAL_TYPE (GomDemoViewCell, gom_demo_view_cell, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_ITEM,
  PROP_FIELD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char *
gom_demo_value_to_string (const GValue *value)
{
  if (value == NULL || !G_IS_VALUE (value))
    return g_strdup ("");

  if (G_VALUE_HOLDS_STRING (value))
    return g_strdup (g_value_get_string (value));

  if (G_VALUE_HOLDS_BOOLEAN (value))
    return g_strdup (g_value_get_boolean (value) ? "true" : "false");

  if (G_VALUE_HOLDS_INT (value))
    return g_strdup_printf ("%d", g_value_get_int (value));

  if (G_VALUE_HOLDS_UINT (value))
    return g_strdup_printf ("%u", g_value_get_uint (value));

  if (G_VALUE_HOLDS_INT64 (value))
    return g_strdup_printf ("%"G_GINT64_FORMAT, g_value_get_int64 (value));

  if (G_VALUE_HOLDS_UINT64 (value))
    return g_strdup_printf ("%"G_GUINT64_FORMAT, g_value_get_uint64 (value));

  if (G_VALUE_HOLDS_DOUBLE (value))
    return g_strdup_printf ("%g", g_value_get_double (value));

  if (G_VALUE_HOLDS_FLOAT (value))
    return g_strdup_printf ("%g", (double)g_value_get_float (value));

  if (G_VALUE_HOLDS_POINTER (value) && g_value_get_pointer (value) == NULL)
    return g_strdup ("");

  if (G_VALUE_HOLDS (value, G_TYPE_BYTES) && g_value_get_boxed (value) != NULL)
    {
      GBytes *bytes = g_value_get_boxed (value);
      GString *str = g_string_new (NULL);
      gsize len = g_bytes_get_size (bytes);
      const guint8 *data = g_bytes_get_data (bytes, NULL);

      g_string_append_printf (str, "b[%"G_GUINT64_FORMAT"]", len);

      if (len > 0)
        {
          g_string_append_c (str, '{');
          g_string_append_printf (str, "%02x", data[0]);
          for (guint i = 1; i < MIN (8, len); i++)
            g_string_append_printf (str, ", %02x", data[i]);
          g_string_append_c (str, '}');
        }

      return g_string_free (str, FALSE);
    }

  return g_strdup_value_contents (value);
}

static char *
gom_demo_view_cell_string (GObject     *object,
                           GomRecord   *record,
                           const char  *field)
{
  if (record != NULL && field != NULL)
    {
      g_auto(GValue) value = G_VALUE_INIT;

      if (gom_record_get_column_by_name (record, field, &value))
        return gom_demo_value_to_string (&value);
    }

  return g_strdup ("");
}

static void
gom_demo_view_cell_set_item (GomDemoViewCell   *self,
                             GomRecordListItem *item)
{
  g_assert (GOM_DEMO_IS_VIEW_CELL (self));
  g_assert (item == NULL || GOM_IS_RECORD_LIST_ITEM (item));

  if (g_set_object (&self->item, item))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ITEM]);
}

static void
gom_demo_view_cell_set_field (GomDemoViewCell *self,
                              const char      *field)
{
  g_assert (GOM_DEMO_IS_VIEW_CELL (self));

  if (g_set_str (&self->field, field))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FIELD]);
}

static void
gom_demo_view_cell_dispose (GObject *object)
{
  GomDemoViewCell *self = GOM_DEMO_VIEW_CELL (object);

  gtk_widget_dispose_template (GTK_WIDGET (self), GOM_DEMO_TYPE_VIEW_CELL);
  g_clear_object (&self->item);
  g_clear_pointer (&self->field, g_free);

  G_OBJECT_CLASS (gom_demo_view_cell_parent_class)->dispose (object);
}

static void
gom_demo_view_cell_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GomDemoViewCell *self = GOM_DEMO_VIEW_CELL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_FIELD:
      g_value_set_string (value, self->field);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_demo_view_cell_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GomDemoViewCell *self = GOM_DEMO_VIEW_CELL (object);

  switch (prop_id)
    {
    case PROP_ITEM:
      gom_demo_view_cell_set_item (self, g_value_get_object (value));
      break;

    case PROP_FIELD:
      gom_demo_view_cell_set_field (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gom_demo_view_cell_class_init (GomDemoViewCellClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gom_demo_view_cell_dispose;
  object_class->get_property = gom_demo_view_cell_get_property;
  object_class->set_property = gom_demo_view_cell_set_property;

  properties[PROP_ITEM] =
    g_param_spec_object ("item", NULL, NULL,
                         GOM_TYPE_RECORD_LIST_ITEM,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties[PROP_FIELD] =
    g_param_spec_string ("field", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/ui/gom-demo-view-cell.ui");
  gtk_widget_class_bind_template_callback (widget_class, gom_demo_view_cell_string);
}

static void
gom_demo_view_cell_init (GomDemoViewCell *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
