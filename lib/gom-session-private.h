/* gom-session-private.h
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

#include <libdex.h>

#include "gom-session.h"
#include "gom-types-private.h"

G_BEGIN_DECLS

struct _GomSession
{
  GObject parent_instance;

  gint64         id;
  GomRepository *repository;
  GPtrArray     *sync_changes;
  guint          closed : 1;
};

struct _GomSessionClass
{
  GObjectClass parent_class;

  DexFuture *(*query)                     (GomSession  *self,
                                           GomQuery    *query);
  DexFuture *(*mutate)                    (GomSession  *self,
                                           GomMutation *mutation);
  DexFuture *(*persist)                   (GomSession  *self,
                                           GomEntity   *entity);
  DexFuture *(*flush)                     (GomSession  *self);
  DexFuture *(*commit)                    (GomSession  *self);
  DexFuture *(*rollback)                  (GomSession  *self);
  void       (*track_entity_changes)      (GomSession  *self,
                                           GomEntity   *entity);
  void       (*untrack_entity_changes)    (GomSession  *self,
                                           GomEntity   *entity);
  void       (*accept_entity_changes)     (GomSession  *self,
                                           GomEntity   *entity,
                                           GomDelta    *delta);
  void       (*mark_entity_dirty)         (GomSession  *self,
                                           GomEntity   *entity);
  GomEntity *(*lookup_entity)             (GomSession  *self,
                                           const char  *entity_key);
  GomEntity *(*register_entity)           (GomSession  *self,
                                           GomEntity   *entity,
                                           char        *entity_key);
  void       (*unregister_pending_entity) (GomSession  *self,
                                           GomEntity   *entity);
  gboolean   (*rekey_entity_identity)     (GomSession  *self,
                                           GomEntity   *entity,
                                           char        *entity_key);
  void       (*unregister_entity)         (GomSession  *self,
                                           GomEntity   *entity);
  void       (*clear_entities)            (GomSession  *self);
};

void           _gom_session_set_repository            (GomSession    *self,
                                                       GomRepository *repository);
GomRepository *_gom_session_dup_repository            (GomSession    *self);
void           _gom_session_set_closed                (GomSession    *self,
                                                       gboolean       closed);
gboolean       _gom_session_is_closed                 (GomSession    *self);
GomEntity     *_gom_session_lookup_entity             (GomSession    *self,
                                                       const char    *entity_key) G_GNUC_WARN_UNUSED_RESULT;
GomEntity     *_gom_session_register_entity           (GomSession    *self,
                                                       GomEntity     *entity,
                                                       char          *entity_key) G_GNUC_WARN_UNUSED_RESULT;
gboolean       _gom_session_rekey_entity_identity     (GomSession    *self,
                                                       GomEntity     *entity,
                                                       char          *entity_key) G_GNUC_WARN_UNUSED_RESULT;
void           _gom_session_unregister_entity         (GomSession    *self,
                                                       GomEntity     *entity);
void           _gom_session_clear_entities            (GomSession    *self);
DexFuture     *_gom_session_query                     (GomSession    *self,
                                                       GomQuery      *query) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_session_mutate                    (GomSession    *self,
                                                       GomMutation   *mutation) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_session_persist                   (GomSession    *self,
                                                       GomEntity     *entity) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_session_flush                     (GomSession    *self) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_session_commit                    (GomSession    *self) G_GNUC_WARN_UNUSED_RESULT;
DexFuture     *_gom_session_rollback                  (GomSession    *self) G_GNUC_WARN_UNUSED_RESULT;
void           _gom_session_unregister_pending_entity (GomSession    *self,
                                                       GomEntity     *entity);
void           _gom_session_track_entity_changes      (GomSession    *self,
                                                       GomEntity     *entity);
void           _gom_session_untrack_entity_changes    (GomSession    *self,
                                                       GomEntity     *entity);
void           _gom_session_accept_entity_changes     (GomSession    *self,
                                                       GomEntity     *entity,
                                                       GomDelta      *delta);
void           _gom_session_record_entity_changes     (GomSession    *self,
                                                       GomEntity     *entity,
                                                       GomDelta      *delta);
void           _gom_session_mark_entity_dirty         (GomSession    *self,
                                                       GomEntity     *entity);
void           _gom_session_emit_changed              (GomSession    *self);
DexFuture     *_gom_session_track_mutation_result     (GomSession    *self,
                                                       DexFuture     *mutation_result) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
