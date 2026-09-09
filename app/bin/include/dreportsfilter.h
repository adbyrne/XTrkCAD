/** \file dreportsfilter.h
 * Shared "Filter Layers/Groups..." dialog for the Reports feature (SF #217,
 * SF #787 phase 1). One dialog window, re-targeted at whichever report's
 * own reportsFilter_t is being edited -- see dreportsfilter.c's header
 * comment for why a single shared dialog serves every report instead of
 * seven separate copies.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2026 XTrkCAD contributors
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef DREPORTSFILTER_H
#define DREPORTSFILTER_H

#include "common.h"

/** One report's layer/group scope. \c included is indexed by 0-based
 * layer index; \c includedCount is the number of TRUE entries, kept in
 * sync by every mutator below so ReportsFilterActive() is O(1). An
 * all-FALSE/zero-initialized instance (every report's own static starts
 * this way) means "unfiltered" -- see ReportsFilterLayerIncluded(). */
typedef struct {
	BOOL_T included[NUM_LAYERS];
	int includedCount;
} reportsFilter_t;

/** FALSE (unfiltered, every layer included) until at least one layer has
 * been explicitly added to \p f via the Filter dialog. */
BOOL_T ReportsFilterActive(const reportsFilter_t *f);

/** TRUE if \p layer (0-based) is in scope for \p f -- always TRUE while
 * !ReportsFilterActive(f), matching every report's original, pre-filter
 * behavior when nothing has been filtered yet. */
BOOL_T ReportsFilterLayerIncluded(const reportsFilter_t *f, unsigned int layer);

/** Show the shared Filter dialog, re-targeted at \p filter. Mutates
 * \p filter in place as the user shuttles layers/groups between Available
 * and Included; the caller's own report is not re-run automatically --
 * matching every other report control, the user clicks that report's own
 * Refresh button afterward to see the new scope take effect. */
void ShowReportsFilterDialog(reportsFilter_t *filter);

#endif
