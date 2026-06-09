/* gom-types.h
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

#include <glib-object.h>

#include "gom-version-macros.h"

G_BEGIN_DECLS

/*<private>
 * GOM_DECLARE_INTERNAL_TYPE:
 * @ModuleObjName: The name of the new type, in camel case (like GtkWidget)
 * @module_obj_name: The name of the new type in lowercase, with words
 *  separated by '_' (like 'gtk_widget')
 * @MODULE: The name of the module, in all caps (like 'GTK')
 * @OBJ_NAME: The bare name of the type, in all caps (like 'WIDGET')
 * @ParentName: the name of the parent type, in camel case (like GtkWidget)
 *
 * A convenience macro for emitting the usual declarations in the
 * header file for a type which is intended to be subclassed only
 * by internal consumers.
 *
 * This macro differs from %G_DECLARE_DERIVABLE_TYPE and %G_DECLARE_FINAL_TYPE
 * by declaring a type that is only derivable internally. Internal users can
 * derive this type, assuming they have access to the instance and class
 * structures; external users will not be able to subclass this type.
 */
#define GOM_DECLARE_INTERNAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName)       \
  GType module_obj_name##_get_type (void);                                                            \
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                    \
  typedef struct _##ModuleObjName ModuleObjName;                                                      \
  typedef struct _##ModuleObjName##Class ModuleObjName##Class;                                        \
                                                                                                      \
  _GLIB_DEFINE_AUTOPTR_CHAINUP (ModuleObjName, ParentName)                                            \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (ModuleObjName##Class, g_type_class_unref)                            \
                                                                                                      \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) {                    \
    return G_TYPE_CHECK_INSTANCE_CAST (ptr, module_obj_name##_get_type (), ModuleObjName); }          \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_CLASS (gpointer ptr) {     \
    return G_TYPE_CHECK_CLASS_CAST (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }      \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) {                        \
    return G_TYPE_CHECK_INSTANCE_TYPE (ptr, module_obj_name##_get_type ()); }                         \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME##_CLASS (gpointer ptr) {                \
    return G_TYPE_CHECK_CLASS_TYPE (ptr, module_obj_name##_get_type ()); }                            \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) { \
    return G_TYPE_INSTANCE_GET_CLASS (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }    \
  G_GNUC_END_IGNORE_DEPRECATIONS

typedef struct _GomCursor                   GomCursor;
typedef struct _GomCustomMigration          GomCustomMigration;
typedef struct _GomCustomMigrator           GomCustomMigrator;
typedef struct _GomDeletion                 GomDeletion;
typedef struct _GomDeletionBuilder          GomDeletionBuilder;
typedef struct _GomDelta                    GomDelta;
typedef struct _GomDriver                   GomDriver;
typedef struct _GomDriverOptions            GomDriverOptions;
typedef struct _GomEntity                   GomEntity;
typedef struct _GomEntityListItem           GomEntityListItem;
typedef struct _GomEntityListModel          GomEntityListModel;
typedef struct _GomEntityMigrator           GomEntityMigrator;
typedef struct _GomEntitySpec               GomEntitySpec;
typedef struct _GomExpression               GomExpression;
typedef struct _GomFieldSchema              GomFieldSchema;
typedef struct _GomIndexSchema              GomIndexSchema;
typedef struct _GomIndexSpec                GomIndexSpec;
typedef struct _GomInsertion                GomInsertion;
typedef struct _GomInsertionBuilder         GomInsertionBuilder;
typedef struct _GomMergeDecision            GomMergeDecision;
typedef struct _GomMergePolicy              GomMergePolicy;
typedef struct _GomMigration                GomMigration;
typedef struct _GomMigrator                 GomMigrator;
typedef struct _GomMutation                 GomMutation;
typedef struct _GomMutationResult           GomMutationResult;
typedef struct _GomNestedMigration          GomNestedMigration;
typedef struct _GomOrdering                 GomOrdering;
typedef struct _GomPropertySpec             GomPropertySpec;
typedef struct _GomQuery                    GomQuery;
typedef struct _GomQueryBuilder             GomQueryBuilder;
typedef struct _GomQueryModel               GomQueryModel;
typedef struct _GomRecord                   GomRecord;
typedef struct _GomRecordListItem           GomRecordListItem;
typedef struct _GomRecordListModel          GomRecordListModel;
typedef struct _GomRegistry                 GomRegistry;
typedef struct _GomRegistryBuilder          GomRegistryBuilder;
typedef struct _GomRelatedModel             GomRelatedModel;
typedef struct _GomRelationSchema           GomRelationSchema;
typedef struct _GomRelationshipSpec         GomRelationshipSpec;
typedef struct _GomRepository               GomRepository;
typedef struct _GomSchema                   GomSchema;
typedef struct _GomSession                  GomSession;
typedef struct _GomSpec                     GomSpec;
typedef struct _GomSpecClass                GomSpecClass;
typedef struct _GomSqlMigration             GomSqlMigration;
typedef struct _GomSyncCoordinator          GomSyncCoordinator;
typedef struct _GomSyncTransport            GomSyncTransport;
typedef struct _GomUpdate                   GomUpdate;
typedef struct _GomUpdateBuilder            GomUpdateBuilder;
typedef struct _GomVector                   GomVector;
typedef struct _GomVectorDistanceExpression GomVectorDistanceExpression;

/**
 * GOM_ERROR:
 *
 * Error domain for libgom.
 */
#define GOM_ERROR (gom_error_quark())

/**
 * GomError:
 * @GOM_ERROR_FAILED: Generic failure.
 * @GOM_ERROR_CONSTRAINT: A database constraint failed.
 * @GOM_ERROR_BACKEND_INITIALIZATION_FAILED: A database backend failed to initialize.
 * @GOM_ERROR_MISSING_EXTENSION: A required database extension could not be loaded.
 * @GOM_ERROR_BUSY_TIMEOUT: Timed out while waiting for a busy or locked database.
 * @GOM_ERROR_PREPARE_FAILED: A database statement could not be prepared.
 * @GOM_ERROR_BIND_FAILED: A database statement parameter could not be bound.
 * @GOM_ERROR_METADATA_READ_FAILED: Database schema metadata could not be read.
 * @GOM_ERROR_INSERT_FAILED: A database insert failed.
 * @GOM_ERROR_UPDATE_FAILED: A database update failed.
 * @GOM_ERROR_DELETE_FAILED: A database delete failed.
 * @GOM_ERROR_INVALID_ENCRYPTION_KEY: The encryption key is invalid or cannot
 *  read the encrypted database.
 *
 * Error codes for the %GOM_ERROR domain.
 */
typedef enum _GomError
{
  GOM_ERROR_FAILED,
  GOM_ERROR_CONSTRAINT,
  GOM_ERROR_BACKEND_INITIALIZATION_FAILED,
  GOM_ERROR_MISSING_EXTENSION,
  GOM_ERROR_BUSY_TIMEOUT,
  GOM_ERROR_PREPARE_FAILED,
  GOM_ERROR_BIND_FAILED,
  GOM_ERROR_METADATA_READ_FAILED,
  GOM_ERROR_INSERT_FAILED,
  GOM_ERROR_UPDATE_FAILED,
  GOM_ERROR_DELETE_FAILED,
  GOM_ERROR_INVALID_ENCRYPTION_KEY,
} GomError;

GOM_AVAILABLE_IN_ALL
GQuark gom_error_quark (void) G_GNUC_CONST;

/**
 * GomSortDirection:
 * @GOM_SORT_ASCENDING: Sort results in ascending order.
 * @GOM_SORT_DESCENDING: Sort results in descending order.
 *
 * The direction used when ordering query results.
 */
typedef enum _GomSortDirection
{
  GOM_SORT_ASCENDING  = 0,
  GOM_SORT_DESCENDING = 1,
} GomSortDirection;

/**
 * GomNullsMode:
 * @GOM_NULLS_DEFAULT: Use the database default NULL ordering.
 * @GOM_NULLS_FIRST: Sort NULL values before non-NULL values.
 * @GOM_NULLS_LAST: Sort NULL values after non-NULL values.
 *
 * Controls how NULL values are ordered for an [class@Gom.Ordering].
 */
typedef enum _GomNullsMode
{
  GOM_NULLS_DEFAULT = 0,
  GOM_NULLS_FIRST   = 1,
  GOM_NULLS_LAST    = 2,
} GomNullsMode;

/**
 * GomSearchMode:
 * @GOM_SEARCH_MODE_NATURAL: Use the query as-is.
 * @GOM_SEARCH_MODE_PREFIX: Match terms by prefix.
 * @GOM_SEARCH_MODE_PHRASE: Match the query as a quoted phrase.
 *
 * Controls how a search query is interpreted for a
 * [class@Gom.SearchExpression].
 */
typedef enum _GomSearchMode
{
  GOM_SEARCH_MODE_NATURAL = 0,
  GOM_SEARCH_MODE_PREFIX  = 1,
  GOM_SEARCH_MODE_PHRASE  = 2,
} GomSearchMode;

/**
 * GomSearchFlags:
 * @GOM_SEARCH_NONE: No special search behavior.
 * @GOM_SEARCH_INDEXED: Include the property in the full-text index.
 * @GOM_SEARCH_PREFIX: Enable prefix matching for the property.
 * @GOM_SEARCH_CASE_FOLDED: Case-fold text before indexing.
 * @GOM_SEARCH_NORMALIZED: Normalize text before indexing.
 *
 * Flags controlling how a property participates in full-text search.
 */
typedef enum _GomSearchFlags
{
  GOM_SEARCH_NONE        = 0,
  GOM_SEARCH_INDEXED     = 1 << 0,
  GOM_SEARCH_PREFIX      = 1 << 1,
  GOM_SEARCH_CASE_FOLDED = 1 << 2,
  GOM_SEARCH_NORMALIZED  = 1 << 3,
} GomSearchFlags;

/**
 * GomVectorFormat:
 * @GOM_VECTOR_FORMAT_FLOAT32_LE: IEEE 754 single-precision floats in little-endian order.
 * @GOM_VECTOR_FORMAT_FLOAT32: alias for %GOM_VECTOR_FORMAT_FLOAT32_LE.
 *
 * The storage format for a [struct@Gom.Vector].
 */
typedef enum _GomVectorFormat
{
  GOM_VECTOR_FORMAT_FLOAT32_LE = 0,
  GOM_VECTOR_FORMAT_FLOAT32    = GOM_VECTOR_FORMAT_FLOAT32_LE,
} GomVectorFormat;

/**
 * GomVectorMetric:
 * @GOM_VECTOR_METRIC_COSINE: Cosine distance.
 * @GOM_VECTOR_METRIC_DOT: Dot-product similarity.
 * @GOM_VECTOR_METRIC_L2: Squared Euclidean distance.
 *
 * The metric used to compare vectors.
 */
typedef enum _GomVectorMetric
{
  GOM_VECTOR_METRIC_COSINE = 0,
  GOM_VECTOR_METRIC_DOT    = 1,
  GOM_VECTOR_METRIC_L2     = 2,
} GomVectorMetric;

/**
 * GomRepositoryFeature:
 * @GOM_REPOSITORY_FEATURE_VECTOR_SEARCH: backend-supported vector distance search.
 *
 * Optional repository features supported by a backend.
 */
typedef enum _GomRepositoryFeature
{
  GOM_REPOSITORY_FEATURE_VECTOR_SEARCH = 0,
} GomRepositoryFeature;

/**
 * GomDeltaKind:
 * @GOM_DELTA_KIND_UPDATE: The delta updates an existing record.
 * @GOM_DELTA_KIND_INSERT: The delta inserts a new record.
 * @GOM_DELTA_KIND_DELETE: The delta deletes an existing record.
 *
 * The kind of change represented by a [class@Gom.Delta].
 */
typedef enum _GomDeltaKind
{
  GOM_DELTA_KIND_UPDATE,
  GOM_DELTA_KIND_INSERT,
  GOM_DELTA_KIND_DELETE,
} GomDeltaKind;

/**
 * GomEntityLifecycle:
 * @GOM_ENTITY_LIFECYCLE_TRANSIENT: The entity is newly constructed and not yet tracked.
 * @GOM_ENTITY_LIFECYCLE_PENDING: The entity is scheduled to be persisted.
 * @GOM_ENTITY_LIFECYCLE_PERSISTENT: The entity is stored and has a stable identity.
 * @GOM_ENTITY_LIFECYCLE_DETACHED: The entity is no longer associated with a session.
 * @GOM_ENTITY_LIFECYCLE_DELETED: The entity has been deleted from the repository.
 *
 * The lifecycle state of a [class@Gom.Entity].
 */
typedef enum
{
  GOM_ENTITY_LIFECYCLE_TRANSIENT,
  GOM_ENTITY_LIFECYCLE_PENDING,
  GOM_ENTITY_LIFECYCLE_PERSISTENT,
  GOM_ENTITY_LIFECYCLE_DETACHED,
  GOM_ENTITY_LIFECYCLE_DELETED,
} GomEntityLifecycle;

/**
 * GomEntityOrigin:
 * @GOM_ENTITY_ORIGIN_CONSTRUCTED: The entity was created in memory.
 * @GOM_ENTITY_ORIGIN_MATERIALIZED: The entity was loaded from storage.
 *
 * The origin of a [class@Gom.Entity].
 */
typedef enum
{
  GOM_ENTITY_ORIGIN_CONSTRUCTED,
  GOM_ENTITY_ORIGIN_MATERIALIZED,
} GomEntityOrigin;

/**
 * GomRelationshipCardinality:
 * @GOM_RELATIONSHIP_CARDINALITY_TO_ONE: The relationship points to one target.
 * @GOM_RELATIONSHIP_CARDINALITY_TO_MANY: The relationship points to multiple targets.
 *
 * The cardinality of a relationship specification.
 */
typedef enum _GomRelationshipCardinality
{
  GOM_RELATIONSHIP_CARDINALITY_TO_ONE,
  GOM_RELATIONSHIP_CARDINALITY_TO_MANY,
} GomRelationshipCardinality;

/**
 * GomRelationshipStorage:
 * @GOM_RELATIONSHIP_STORAGE_FK: Store the relationship using a foreign key.
 * @GOM_RELATIONSHIP_STORAGE_JOIN_TABLE: Store the relationship in a join table.
 * @GOM_RELATIONSHIP_STORAGE_ASSOCIATION_ENTITY: Store the relationship through an association entity.
 *
 * The storage strategy for a relationship specification.
 */
typedef enum _GomRelationshipStorage
{
  GOM_RELATIONSHIP_STORAGE_FK,
  GOM_RELATIONSHIP_STORAGE_JOIN_TABLE,
  GOM_RELATIONSHIP_STORAGE_ASSOCIATION_ENTITY,
} GomRelationshipStorage;

/**
 * GomRelationshipDeleteRule:
 * @GOM_RELATIONSHIP_DELETE_NO_ACTION: Leave related data unchanged.
 * @GOM_RELATIONSHIP_DELETE_NULLIFY: Clear the relationship when the target is deleted.
 * @GOM_RELATIONSHIP_DELETE_CASCADE: Delete related data when the target is deleted.
 * @GOM_RELATIONSHIP_DELETE_DENY: Reject the delete when related data exists.
 *
 * The action to take when a related row is deleted.
 */
typedef enum _GomRelationshipDeleteRule
{
  GOM_RELATIONSHIP_DELETE_NO_ACTION,
  GOM_RELATIONSHIP_DELETE_NULLIFY,
  GOM_RELATIONSHIP_DELETE_CASCADE,
  GOM_RELATIONSHIP_DELETE_DENY,
} GomRelationshipDeleteRule;

/**
 * GomCursorCapabilities:
 * @GOM_CURSOR_CAPABILITIES_NONE: No special capabilities beyond `next()`.
 * @GOM_CURSOR_CAPABILITIES_REWIND: The cursor can be rewound to the start.
 * @GOM_CURSOR_CAPABILITIES_ABSOLUTE: The cursor can move to an absolute position.
 * @GOM_CURSOR_CAPABILITIES_RELATIVE: The cursor can move relative to the current position.
 * @GOM_CURSOR_CAPABILITIES_COUNT: The cursor can return a count of total matches.
 *
 * Capability flags for [class@Gom.Cursor] movement and counting support.
 */
typedef enum _GomCursorCapabilities
{
  GOM_CURSOR_CAPABILITIES_NONE     = 0,
  GOM_CURSOR_CAPABILITIES_REWIND   = 1 << 0,
  GOM_CURSOR_CAPABILITIES_ABSOLUTE = 1 << 1,
  GOM_CURSOR_CAPABILITIES_RELATIVE = 1 << 2,
  GOM_CURSOR_CAPABILITIES_COUNT    = 1 << 3,
} GomCursorCapabilities;

/**
 * GomToBytesFunc:
 * @value: the value to transform
 * @user_data: data supplied to closure
 * @error: return location for a [type@GLib.Error]
 *
 * This function is responsible for converting a GValue into a
 * byte buffer suitable for storage. This is generally used when
 * you have a blob field in the storage layer.
 *
 * Returns: (transfer full): a [struct@GLib.Bytes] if successful; otherwise
 *   %NULL to indicate error.
 */
typedef GBytes *(*GomToBytesFunc) (const GValue  *value,
                                   gpointer       user_data,
                                   GError       **error);

/**
 * GomFromBytesFunc:
 * @bytes: the byte buffer to deserialize
 * @value: a value to hold what is created from @bytes
 * @user_data: data supplied to closure
 * @error: return location for a [type@GLib.Error]
 *
 * This function converts @bytes into a new value and stores it within
 * the @value box.
 *
 * Returns: %TRUE if successful; otherwise FALSE
 */
typedef gboolean (*GomFromBytesFunc) (GBytes    *bytes,
                                      GValue    *value,
                                      gpointer   user_data,
                                      GError   **error);

G_END_DECLS
