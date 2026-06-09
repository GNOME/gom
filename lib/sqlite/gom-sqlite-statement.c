/* gom-sqlite-statement.c
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

#include <sqlite3.h>

#include "gom-sqlite-lease-private.h"
#include "gom-sqlite-statement-private.h"
#include "gom-trace-private.h"

struct _GomSqliteStatement
{
  GObject              parent_instance;
  GomSqliteLeaseState *state;
  sqlite3_stmt        *stmt;
};

struct _GomSqliteStatementClass
{
  GObjectClass parent_class;
};

G_DEFINE_FINAL_TYPE (GomSqliteStatement, gom_sqlite_statement, G_TYPE_OBJECT)

static void
gom_sqlite_statement_finalize (GObject *object)
{
  GomSqliteStatement *self = (GomSqliteStatement *)object;

  if (self->stmt != NULL)
    {
      sqlite3_finalize (self->stmt);
      self->stmt = NULL;
    }
  if (self->state != NULL)
    gom_sqlite_lease_state_unref (self->state);

  G_OBJECT_CLASS (gom_sqlite_statement_parent_class)->finalize (object);
}

static void
gom_sqlite_statement_class_init (GomSqliteStatementClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gom_sqlite_statement_finalize;
}

static void
gom_sqlite_statement_init (GomSqliteStatement *self)
{
}

GomSqliteStatement *
gom_sqlite_statement_new (GomSqliteLeaseState *state,
                          sqlite3_stmt        *stmt)
{
  GomSqliteStatement *self;

  g_return_val_if_fail (state != NULL, NULL);
  g_return_val_if_fail (stmt != NULL, NULL);

  self = g_object_new (GOM_TYPE_SQLITE_STATEMENT, NULL);
  self->state = gom_sqlite_lease_state_ref (state);
  self->stmt = stmt;

  return self;
}

GomSqliteLeaseState *
gom_sqlite_statement_get_state (GomSqliteStatement *self)
{
  g_return_val_if_fail (GOM_IS_SQLITE_STATEMENT (self), NULL);

  return self->state;
}

sqlite3_stmt *
gom_sqlite_statement_get_native (GomSqliteStatement *self)
{
  g_return_val_if_fail (GOM_IS_SQLITE_STATEMENT (self), NULL);

  return self->stmt;
}

void
gom_sqlite_statement_reset (GomSqliteStatement *self)
{
  g_return_if_fail (GOM_IS_SQLITE_STATEMENT (self));

  if (self->stmt != NULL)
    sqlite3_reset (self->stmt);
}
