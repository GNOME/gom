/* libgom.h
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

#include "gom-config.h"
#include "gom-types.h"
#include "gom-version-macros.h"

#define GOM_INSIDE
#include "gom-delta.h"
#include "gom-cursor.h"
#include "gom-custom-migration.h"
#include "gom-custom-migrator.h"
#include "gom-entity-list-item.h"
#include "gom-entity-list-model.h"
#include "gom-deletion.h"
#include "gom-deletion-builder.h"
#include "gom-driver.h"
#include "gom-driver-options.h"
#include "gom-entity-migrator.h"
#include "gom-entity.h"
#include "gom-expression.h"
#include "gom-insertion-builder.h"
#include "gom-insertion.h"
#include "gom-migration.h"
#include "gom-meta.h"
#include "gom-migrator.h"
#include "gom-mutation.h"
#include "gom-mutation-result.h"
#include "gom-nested-migration.h"
#include "gom-related-model.h"
#include "gom-ordering.h"
#include "gom-session.h"
#include "gom-query-builder.h"
#include "gom-query.h"
#include "gom-query-model.h"
#include "gom-record.h"
#include "gom-record-list-item.h"
#include "gom-record-list-model.h"
#include "gom-repository.h"
#include "gom-schema.h"
#include "gom-sql-migration.h"
#include "gom-sync-coordinator.h"
#include "gom-sync-transport.h"
#include "gom-merge-decision.h"
#include "gom-merge-policy.h"
#include "gom-update.h"
#include "gom-update-builder.h"
#include "gom-vector.h"
#include "gom-version.h"
#undef GOM_INSIDE
