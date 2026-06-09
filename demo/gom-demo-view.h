/* gom-demo-view.h
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

#pragma once

#include <gtk/gtk.h>
#include <libgom.h>

G_BEGIN_DECLS

#define GOM_DEMO_TYPE_VIEW (gom_demo_view_get_type())

G_DECLARE_FINAL_TYPE (GomDemoView, gom_demo_view, GOM_DEMO, VIEW, GtkBox)

GtkWidget *gom_demo_view_new (GomRepository  *repository,
                              const char     *relation,
                              GError        **error);

G_END_DECLS
