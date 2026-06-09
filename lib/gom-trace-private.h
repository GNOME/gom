/* gom-trace-private.h
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

#include <glib.h>

#include "gom-config.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

typedef struct _GomTraceScope GomTraceScope;
typedef struct _DexFuture DexFuture;

void           gom_trace_init          (void);
gint64         gom_trace_now           (void);
GomTraceScope *gom_trace_scope_new     (const char      *group,
                                        const char      *name,
                                        const char      *message_format,
                                        ...) G_GNUC_PRINTF (3, 4);
void           gom_trace_scope_free    (GomTraceScope   *scope);
void           gom_trace_mark_printf   (gint64           start_time,
                                        gint64           duration,
                                        const char      *group,
                                        const char      *name,
                                        const char      *message_format,
                                        ...) G_GNUC_PRINTF (5, 6);
DexFuture     *gom_trace_marked_future (DexFuture       *future,
                                        gint64           start_time,
                                        const char      *group,
                                        const char      *name,
                                        const char      *message_format,
                                        ...) G_GNUC_PRINTF (5, 6) G_GNUC_WARN_UNUSED_RESULT;
void           gom_trace_log_printf    (int              severity,
                                        const char      *domain,
                                        const char      *message_format,
                                        ...) G_GNUC_PRINTF (3, 4);
void           gom_trace_counters_set  (gint             repositories,
                                        gint             sessions,
                                        gint             cursors,
                                        gint             identity_entries,
                                        gint             pending_entities);
void           gom_trace_counter_set   (GomTraceCounter  counter,
                                        gint             value);
void           gom_trace_counter_add   (GomTraceCounter  counter,
                                        gint             delta);
int            gom_trace_counter_get   (GomTraceCounter  counter);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GomTraceScope, gom_trace_scope_free)

#define GOM_TRACE_SCOPE(group, name, ...) \
  g_autoptr(GomTraceScope) G_PASTE (__gom_trace_scope_, __LINE__) = \
    gom_trace_scope_new ((group), (name), __VA_ARGS__)

#define GOM_TRACE_BEGIN_MARK() \
  (gom_trace_now ())

#define GOM_TRACE_END_MARK(start_time, group, name, ...) \
  gom_trace_mark_printf ((start_time), gom_trace_now () - (start_time), (group), (name), __VA_ARGS__)

#define GOM_TRACE_MARK(group, name, ...) \
  gom_trace_mark_printf (gom_trace_now (), 0, (group), (name), __VA_ARGS__)

#define GOM_TRACE_MARKED_FUTURE(future, start_time, group, name, ...) \
  gom_trace_marked_future ((future), (start_time), (group), (name), __VA_ARGS__)

#define GOM_TRACE_LOG(severity, domain, ...) \
  gom_trace_log_printf ((severity), (domain), __VA_ARGS__)

#define GOM_TRACE_COUNTERS(repositories, sessions, cursors, identity_entries, pending_entities) \
  gom_trace_counters_set ((repositories), (sessions), (cursors), (identity_entries), (pending_entities))

G_END_DECLS
