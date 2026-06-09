/* gom-types-private.h
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

#include "gom-types.h"

G_BEGIN_DECLS

typedef struct _GomEntityClassInfo        GomEntityClassInfo;
typedef struct _GomEntityPropertyInfo     GomEntityPropertyInfo;
typedef struct _GomEntityRelationshipInfo GomEntityRelationshipInfo;
typedef struct _GomEntityDiff             GomEntityDiff;
typedef struct _GomIndexDiff              GomIndexDiff;
typedef struct _GomPropertyDiff           GomPropertyDiff;
typedef struct _GomRegistryDiff           GomRegistryDiff;
typedef struct _GomSqliteConnection       GomSqliteConnection;
typedef struct _GomSqliteLease            GomSqliteLease;
typedef struct _GomSqliteLeaseState       GomSqliteLeaseState;
typedef struct _GomSqlitePool             GomSqlitePool;
typedef struct _GomSqliteStatement        GomSqliteStatement;
typedef struct _GomPgsqlCursor            GomPgsqlCursor;
typedef struct _GomPgsqlDriver            GomPgsqlDriver;
typedef struct _GomPgsqlSession           GomPgsqlSession;
typedef struct _GomMockDriver             GomMockDriver;

typedef enum _GomUnaryOperator
{
  GOM_UNARY_NEGATE = 0,
  GOM_UNARY_NOT    = 1,
} GomUnaryOperator;

typedef enum _GomBinaryOperator
{
  GOM_BINARY_ADD           = 0,
  GOM_BINARY_SUBTRACT      = 1,
  GOM_BINARY_MULTIPLY      = 2,
  GOM_BINARY_DIVIDE        = 3,
  GOM_BINARY_MODULO        = 4,
  GOM_BINARY_EQUAL         = 5,
  GOM_BINARY_NOT_EQUAL     = 6,
  GOM_BINARY_LESS_THAN     = 7,
  GOM_BINARY_LESS_EQUAL    = 8,
  GOM_BINARY_GREATER_THAN  = 9,
  GOM_BINARY_GREATER_EQUAL = 10,
  GOM_BINARY_AND           = 11,
  GOM_BINARY_OR            = 12,
  GOM_BINARY_LIKE          = 13,
} GomBinaryOperator;

typedef enum _GomCursorFlags
{
  GOM_CURSOR_FLAGS_NONE       = 0,
  GOM_CURSOR_FLAGS_COUNT_ROWS = 1 << 0,
} GomCursorFlags;

typedef enum
{
  GOM_TRACE_COUNTER_REPOSITORIES = 0,
  GOM_TRACE_COUNTER_SESSIONS,
  GOM_TRACE_COUNTER_CURSORS,
  GOM_TRACE_COUNTER_IDENTITY_ENTRIES,
  GOM_TRACE_COUNTER_PENDING_ENTITIES,
  GOM_TRACE_COUNTER_COUNT,
} GomTraceCounter;

G_END_DECLS
