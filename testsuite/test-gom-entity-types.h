/* test-gom-entity-types.h
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

#include <libgom.h>

typedef struct _TestEntityBase                       TestEntityBase;
typedef struct _TestEntityBaseClass                  TestEntityBaseClass;
typedef struct _TestEntityBaseCustomIdentity         TestEntityBaseCustomIdentity;
typedef struct _TestEntityBaseCustomIdentityClass    TestEntityBaseCustomIdentityClass;
typedef struct _TestEntityBaseInheritedMetadata      TestEntityBaseInheritedMetadata;
typedef struct _TestEntityBaseInheritedMetadataClass TestEntityBaseInheritedMetadataClass;
typedef struct _TestEntityInvalidInverseTarget       TestEntityInvalidInverseTarget;
typedef struct _TestEntityInvalidInverseTargetClass  TestEntityInvalidInverseTargetClass;
typedef struct _TestEntityOneToOneFeed              TestEntityOneToOneFeed;
typedef struct _TestEntityOneToOneFeedClass         TestEntityOneToOneFeedClass;
typedef struct _TestEntityOneToOneChild             TestEntityOneToOneChild;
typedef struct _TestEntityOneToOneChildClass        TestEntityOneToOneChildClass;

struct _TestEntityBase
{
  GomEntity  parent_instance;
  gint64     id1;
  char      *name;
  char      *internal;
};

struct _TestEntityBaseClass
{
  GomEntityClass parent_class;
};

struct _TestEntityBaseCustomIdentity
{
  TestEntityBase parent_instance;
};

struct _TestEntityBaseCustomIdentityClass
{
  TestEntityBaseClass parent_class;
};

struct _TestEntityBaseInheritedMetadata
{
  TestEntityBase parent_instance;
};

struct _TestEntityBaseInheritedMetadataClass
{
  TestEntityBaseClass parent_class;
};

struct _TestEntityInvalidInverseTarget
{
  GomEntity parent_instance;
  gint64    id;
};

struct _TestEntityInvalidInverseTargetClass
{
  GomEntityClass parent_class;
};

struct _TestEntityOneToOneFeed
{
  GomEntity parent_instance;
  gint64    id;
};

struct _TestEntityOneToOneFeedClass
{
  GomEntityClass parent_class;
};

struct _TestEntityOneToOneChild
{
  GomEntity parent_instance;
  gint64    id;
  gint64    feed_id;
};

struct _TestEntityOneToOneChildClass
{
  GomEntityClass parent_class;
};

GType test_entity_base_get_type                    (void);
GType test_entity_base_custom_identity_get_type    (void);
GType test_entity_base_inherited_metadata_get_type (void);
GType test_entity_invalid_inverse_target_get_type  (void);
GType test_entity_one_to_one_feed_get_type        (void);
GType test_entity_one_to_one_child_get_type       (void);

#define TEST_ENTITY_BASE_TYPE (test_entity_base_get_type ())
#define TEST_ENTITY_BASE_CUSTOM_IDENTITY_TYPE (test_entity_base_custom_identity_get_type ())
#define TEST_ENTITY_BASE_INHERITED_METADATA_TYPE (test_entity_base_inherited_metadata_get_type ())
#define TEST_ENTITY_INVALID_INVERSE_TARGET_TYPE (test_entity_invalid_inverse_target_get_type ())
#define TEST_ENTITY_ONE_TO_ONE_FEED_TYPE (test_entity_one_to_one_feed_get_type ())
#define TEST_ENTITY_ONE_TO_ONE_CHILD_TYPE (test_entity_one_to_one_child_get_type ())

void test_entity_register_types (GomRegistryBuilder *builder);
