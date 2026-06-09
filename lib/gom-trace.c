/* gom-trace.c
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

#include <glib.h>
#include <stdarg.h>

#include <libdex.h>

#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
#include <sysprof-collector.h>
#include <sysprof-capture-types.h>
#endif

#include "gom-trace-private.h"

#define GOM_TRACE_DOMAIN "Gom"
#define GOM_TRACE_GROUP "Gom"

typedef struct
{
  const char *category;
  const char *name;
  const char *description;
  gint        value;
} GomTraceCounterState;

typedef struct
{
  gint64  start_time;
  char   *group;
  char   *name;
  char   *message;
} GomTraceMarkedFuture;

struct _GomTraceScope
{
  gint64  start_time;
  char   *group;
  char   *name;
  char   *message;
};

static gsize trace_initialized = 0;
static gboolean trace_active = FALSE;
static guint trace_counter_base = 0;

static GomTraceCounterState trace_counters[GOM_TRACE_COUNTER_COUNT] = {
  { GOM_TRACE_GROUP, "repositories", "Open repositories", 0 },
  { GOM_TRACE_GROUP, "sessions", "Active sessions", 0 },
  { GOM_TRACE_GROUP, "cursors", "Active cursors", 0 },
  { GOM_TRACE_GROUP, "identity-entries", "Identity-map entries", 0 },
  { GOM_TRACE_GROUP, "pending-entities", "Pending dirty entities", 0 },
};

static void
gom_trace_define_counters (void)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  SysprofCaptureCounter counters[GOM_TRACE_COUNTER_COUNT];
  SysprofCaptureCounterValue values[GOM_TRACE_COUNTER_COUNT];
  unsigned int ids[GOM_TRACE_COUNTER_COUNT];

  trace_counter_base = sysprof_collector_request_counters (GOM_TRACE_COUNTER_COUNT);

  for (guint i = 0; i < GOM_TRACE_COUNTER_COUNT; i++)
    {
      g_strlcpy (counters[i].category, trace_counters[i].category, sizeof counters[i].category);
      g_strlcpy (counters[i].name, trace_counters[i].name, sizeof counters[i].name);
      g_strlcpy (counters[i].description, trace_counters[i].description, sizeof counters[i].description);
      counters[i].id = trace_counter_base + i;
      counters[i].type = SYSPROF_CAPTURE_COUNTER_INT64;
      values[i].v64 = g_atomic_int_get (&trace_counters[i].value);
      ids[i] = trace_counter_base + i;
    }

  sysprof_collector_define_counters (counters, GOM_TRACE_COUNTER_COUNT);
  sysprof_collector_set_counters (ids, values, GOM_TRACE_COUNTER_COUNT);
#endif
}

static gboolean
gom_trace_ensure_active (void)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  sysprof_collector_init ();

  if (!sysprof_collector_is_active ())
    return FALSE;

  if (g_once_init_enter (&trace_initialized))
    {
      trace_active = TRUE;
      gom_trace_define_counters ();
      g_once_init_leave (&trace_initialized, 1);
    }

  return trace_active;
#else
  return FALSE;
#endif
}

static void
gom_trace_set_counter_internal (GomTraceCounter counter,
                                gint            value)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  SysprofCaptureCounterValue counter_value;
  unsigned int id;

  if ((int)counter < 0 || counter >= GOM_TRACE_COUNTER_COUNT)
    return;

  g_atomic_int_set (&trace_counters[counter].value, value);
  if (!gom_trace_ensure_active ())
    return;

  id = trace_counter_base + counter;
  counter_value.v64 = value;
  sysprof_collector_set_counters (&id, &counter_value, 1);
#else
  if ((int)counter >= 0 && counter < GOM_TRACE_COUNTER_COUNT)
    g_atomic_int_set (&trace_counters[counter].value, value);
#endif
}

static void
gom_trace_add_counter_internal (GomTraceCounter counter,
                                gint            delta)
{
  gint value;

  if (delta == 0 || (int)counter < 0 || counter >= GOM_TRACE_COUNTER_COUNT)
    return;

  value = g_atomic_int_add (&trace_counters[counter].value, delta) + delta;

#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  {
    SysprofCaptureCounterValue counter_value;
    unsigned int id;

    if (!gom_trace_ensure_active ())
      return;

    id = trace_counter_base + counter;
    counter_value.v64 = value;
    sysprof_collector_set_counters (&id, &counter_value, 1);
  }
#endif
}

#ifdef G_HAS_CONSTRUCTORS
#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(gom_trace_pre_init)
#endif
G_DEFINE_CONSTRUCTOR (gom_trace_pre_init)
#endif

static void gom_trace_pre_init (void) G_GNUC_UNUSED;

static void
gom_trace_pre_init (void)
{
  gom_trace_ensure_active ();
}

gint64
gom_trace_now (void)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  return SYSPROF_CAPTURE_CURRENT_TIME;
#else
  return g_get_monotonic_time ();
#endif
}

void
gom_trace_init (void)
{
  gom_trace_ensure_active ();
}

GomTraceScope *
gom_trace_scope_new (const char *group,
                     const char *name,
                     const char *message_format,
                     ...)
{
  GomTraceScope *scope;

  g_return_val_if_fail (group != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  if (!gom_trace_ensure_active ())
    return NULL;

  scope = g_new0 (GomTraceScope, 1);
  scope->start_time = gom_trace_now ();
  scope->group = g_strdup (group);
  scope->name = g_strdup (name);

  if (message_format != NULL)
    {
      va_list args;

      va_start (args, message_format);
      scope->message = g_strdup_vprintf (message_format, args);
      va_end (args);
    }

  return scope;
}

void
gom_trace_scope_free (GomTraceScope *scope)
{
  if (scope == NULL)
    return;

  gom_trace_mark_printf (scope->start_time,
                         gom_trace_now () - scope->start_time,
                         scope->group,
                         scope->name,
                         "%s",
                         scope->message != NULL ? scope->message : "");

  g_clear_pointer (&scope->message, g_free);
  g_clear_pointer (&scope->name, g_free);
  g_clear_pointer (&scope->group, g_free);
  g_free (scope);
}

void
gom_trace_mark_printf (gint64      start_time,
                       gint64      duration,
                       const char *group,
                       const char *name,
                       const char *message_format,
                       ...)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  va_list args;
  g_autofree char *prefix = NULL;
  g_autofree char *mark_name = NULL;

  if (!gom_trace_ensure_active ())
    return;

  prefix = g_ascii_strdown (group, -1);
  mark_name = g_strconcat (prefix, ".", name, NULL);

  va_start (args, message_format);
  sysprof_collector_mark_vprintf (start_time,
                                  duration,
                                  GOM_TRACE_GROUP,
                                  mark_name,
                                  message_format,
                                  args);
  va_end (args);
#endif
}

static DexFuture *
gom_trace_marked_future_cb (DexFuture *future,
                            gpointer   user_data)
{
  GomTraceMarkedFuture *marked_future = user_data;

  gom_trace_mark_printf (marked_future->start_time,
                         gom_trace_now () - marked_future->start_time,
                         marked_future->group,
                         marked_future->name,
                         "%s",
                         marked_future->message != NULL ? marked_future->message : "");

  return NULL;
}

static void
gom_trace_marked_future_free (gpointer data)
{
  GomTraceMarkedFuture *marked_future = data;

  g_clear_pointer (&marked_future->message, g_free);
  g_clear_pointer (&marked_future->name, g_free);
  g_clear_pointer (&marked_future->group, g_free);
  g_free (marked_future);
}

DexFuture *
gom_trace_marked_future (DexFuture   *future,
                         gint64       start_time,
                         const char  *group,
                         const char  *name,
                         const char  *message_format,
                         ...)
{
  GomTraceMarkedFuture *marked_future;
  va_list args;

  g_return_val_if_fail (future != NULL, NULL);
  g_return_val_if_fail (group != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  marked_future = g_new0 (GomTraceMarkedFuture, 1);
  marked_future->start_time = start_time;
  marked_future->group = g_strdup (group);
  marked_future->name = g_strdup (name);

  if (message_format != NULL)
    {
      va_start (args, message_format);
      marked_future->message = g_strdup_vprintf (message_format, args);
      va_end (args);
    }

  return dex_future_finally (future,
                             gom_trace_marked_future_cb,
                             marked_future,
                             gom_trace_marked_future_free);
}

void
gom_trace_log_printf (int         severity,
                      const char *domain,
                      const char *message_format,
                      ...)
{
#if defined(HAVE_SYSPROF) && HAVE_SYSPROF
  va_list args;
  g_autofree char *message = NULL;

  if (!gom_trace_ensure_active ())
    return;

  va_start (args, message_format);
  message = g_strdup_vprintf (message_format, args);
  va_end (args);

  sysprof_collector_log (severity, domain != NULL ? domain : GOM_TRACE_DOMAIN, message);
#endif
}

void
gom_trace_counters_set (gint repositories,
                        gint sessions,
                        gint cursors,
                        gint identity_entries,
                        gint pending_entities)
{
  gom_trace_set_counter_internal (GOM_TRACE_COUNTER_REPOSITORIES, repositories);
  gom_trace_set_counter_internal (GOM_TRACE_COUNTER_SESSIONS, sessions);
  gom_trace_set_counter_internal (GOM_TRACE_COUNTER_CURSORS, cursors);
  gom_trace_set_counter_internal (GOM_TRACE_COUNTER_IDENTITY_ENTRIES, identity_entries);
  gom_trace_set_counter_internal (GOM_TRACE_COUNTER_PENDING_ENTITIES, pending_entities);
}

void
gom_trace_counter_set (GomTraceCounter counter,
                       gint            value)
{
  gom_trace_set_counter_internal (counter, value);
}

void
gom_trace_counter_add (GomTraceCounter counter,
                       gint            delta)
{
  gom_trace_add_counter_internal (counter, delta);
}

int
gom_trace_counter_get (GomTraceCounter counter)
{
  g_return_val_if_fail ((int)counter >= 0, 0);
  g_return_val_if_fail (counter < GOM_TRACE_COUNTER_COUNT, 0);

  return g_atomic_int_get (&trace_counters[counter].value);
}
