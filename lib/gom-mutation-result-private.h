/* gom-mutation-result-private.h
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

#include <gio/gio.h>

#include "gom-record-private.h"
#include "gom-mutation-result.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

struct _GomMutationResult
{
  GObject     parent_instance;
  GListStore *records;
  guint64     affected_rows;
};

struct _GomMutationResultClass
{
  GObjectClass parent_class;
};

GomMutationResult *_gom_mutation_result_new           (void);
void               _gom_mutation_result_append_record (GomMutationResult *self,
                                                       GomRecord         *record,
                                                       guint64            affected_rows);

G_END_DECLS
