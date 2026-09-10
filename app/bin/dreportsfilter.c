/** \file dreportsfilter.c
 * Shared "Filter Layers/Groups..." dialog for the Reports feature (SF #217,
 * SF #787 phase 1).
 *
 * One dialog window serves all seven report types (Unconnected Endpoints,
 * Turnout Density, Track Lengths, Curve Stats, Equipment Suitability,
 * Gaps, Kinked Joints) -- each has its own `reportsFilter_t` static in
 * reports.c, and opening this dialog just re-targets it at whichever one
 * the caller passed in, the same re-target-a-shared-dialog pattern
 * dlayergroupui.c's ShowGroupNameDialog() already uses for New/Rename.
 * Building seven near-identical copies of this dialog instead was
 * considered and rejected as needless duplication for a feature that's
 * the same shape everywhere it's used.
 *
 * UI shape: an Available/Included shuttle, exactly like the Manage Layer
 * Groups dialog's own membership editor (dlayergroupui.c) -- a filter's
 * "included" layers are persisted state that needs to be reflected back
 * correctly every time the dialog reopens, which is exactly what a
 * shuttle (state = which list a layer's row is currently in) gives for
 * free and a checkbox-based list cannot (wlib has no API to pre-select
 * specific PD_LIST rows -- see layergroup.ui's own header comment for
 * this same constraint). A Groups list plus one "Add Group to Included"
 * button gives the same one-shot, group-as-preset convenience the Select
 * Layers/Groups dialog (dselectlayers.c) offers for canvas selection --
 * moves every current member of a checked group into Included in one
 * click, then each layer's row is independently movable again afterward.
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

#include "common-ui.h"
#include "common.h"
#include "custom.h"
#include "dlayer.h"
#include "form.h"
#include "include/dlayergroup.h"
#include "include/dreportsfilter.h"
#include "misc.h"

static int log_reportsfilter = -1;
#define LOGREPORTSFILTER() \
	if (log_reportsfilter < 0) { log_reportsfilter = LogFindIndex("reportsfilter"); }

/** The filter this dialog instance is currently editing, set fresh each
 * time ShowReportsFilterDialog() is called. Never NULL while the dialog
 * is visible. */
static reportsFilter_t *currentFilter = NULL;

static void FilterAddLayers(void *action);
static void FilterRemoveLayers(void *action);
static void FilterAddGroup(void *action);
static void FilterClear(void *action);

static paramData_t reportsFilterPLs[] = {
#define I_AVAILABLE	(0)
#define availableL	(reportsFilterPLs[I_AVAILABLE].control)
	{	PD_LIST, NULL, "available", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
#define I_ADDLAYERS	(1)
	{	PD_BUTTON, FilterAddLayers, "addlayer", 0, NULL, NULL },
#define I_REMOVELAYERS	(2)
	{	PD_BUTTON, FilterRemoveLayers, "removelayer", 0, NULL, NULL },
#define I_INCLUDED	(3)
#define includedL	(reportsFilterPLs[I_INCLUDED].control)
	{	PD_LIST, NULL, "included", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
#define I_GROUPLIST	(4)
#define groupsL		(reportsFilterPLs[I_GROUPLIST].control)
	{	PD_LIST, NULL, "groups", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
#define I_ADDGROUP	(5)
	{	PD_BUTTON, FilterAddGroup, "addgroup", 0, NULL, NULL },
#define I_CLEAR	(6)
	{	PD_BUTTON, FilterClear, "clear", 0, NULL, NULL },
};
static paramGroup_t reportsFilterPG = { "reportsfilter", PGO_FULLDIALOGFROMBUILDER, reportsFilterPLs, COUNT( reportsFilterPLs ) };

/**
 * Rebuild the Available/Included lists from scratch against
 * \c currentFilter, one row per named layer (layer 0 always included even
 * if unnamed -- same convention as dlayergroupui.c's RefreshShuttleLists()).
 * Row context is the 0-based layer index.
 */
static void RefreshFilterShuttle(void)
{
	wListClear(availableL);
	wListClear(includedL);

	for (unsigned int inx = 0; inx < NUM_LAYERS; inx++) {
		if (inx != 0 && strlen(GetLayerName(inx)) == 0) {
			continue;
		}

		char *label = FormatLayerName(inx);

		if (currentFilter->included[inx]) {
			wListAddValue(includedL, label, NULL, I2VP(inx));
		} else {
			wListAddValue(availableL, label, NULL, I2VP(inx));
		}
		free(label);
	}
}

/**
 * Rebuild the Groups list from scratch. Row context is the group index.
 */
static void RefreshFilterGroupsList(void)
{
	wListClear(groupsL);

	for (int i = 0; i < LayerGroupCount(); i++) {
		wListAddValue(groupsL, LayerGroupName(i), NULL, I2VP(i));
	}
}

/**
 * Recompute \c currentFilter->includedCount from its \c included array --
 * every mutator below calls this immediately after changing \c included,
 * so ReportsFilterActive()/ReportsFilterLayerIncluded() (reports.c's own
 * compute passes) never see a stale count.
 */
static void SyncIncludedCount(void)
{
	int count = 0;

	for (unsigned int i = 0; i < NUM_LAYERS; i++) {
		if (currentFilter->included[i]) {
			count++;
		}
	}
	currentFilter->includedCount = count;
}

/**
 * "->" button: move every selected row of Available into Included.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void FilterAddLayers(void *action)
{
	wIndex_t cnt = wListGetCount(availableL);

	for (wIndex_t inx = 0; inx < cnt; inx++) {
		if (!wListGetItemSelected(availableL, inx)) {
			continue;
		}
		unsigned int layer = (unsigned int)(intptr_t) wListGetItemContext(availableL,
		                     inx);
		currentFilter->included[layer] = TRUE;
	}

	SyncIncludedCount();
	RefreshFilterShuttle();
}

/**
 * "<-" button: move every selected row of Included back into Available.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void FilterRemoveLayers(void *action)
{
	wIndex_t cnt = wListGetCount(includedL);

	for (wIndex_t inx = 0; inx < cnt; inx++) {
		if (!wListGetItemSelected(includedL, inx)) {
			continue;
		}
		unsigned int layer = (unsigned int)(intptr_t) wListGetItemContext(includedL,
		                     inx);
		currentFilter->included[layer] = FALSE;
	}

	SyncIncludedCount();
	RefreshFilterShuttle();
}

/**
 * "Add Group" button: move every current member of every checked Groups
 * row into Included, in one shot -- a one-time preset, not a live link
 * (matching the Select Layers/Groups dialog's own group semantics, and
 * LayerGroupShowOnly()'s before it): re-running this after editing the
 * group's own membership picks up whatever it contains *now*.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void FilterAddGroup(void *action)
{
	wIndex_t cnt = wListGetCount(groupsL);

	for (wIndex_t inx = 0; inx < cnt; inx++) {
		if (!wListGetItemSelected(groupsL, inx)) {
			continue;
		}
		int groupIdx = (int)(intptr_t) wListGetItemContext(groupsL, inx);
		int memberCnt = LayerGroupMemberCount(groupIdx);

		for (int m = 0; m < memberCnt; m++) {
			int layerIdx1Based = LayerGroupMemberAt(groupIdx, m);
			if (layerIdx1Based > 0 && (unsigned int)(layerIdx1Based - 1) < NUM_LAYERS) {
				currentFilter->included[layerIdx1Based - 1] = TRUE;
			}
		}
	}

	SyncIncludedCount();
	RefreshFilterShuttle();
}

/**
 * "Clear Filter" button: move everything back to Available, restoring
 * the unfiltered ("show every layer") state.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void FilterClear(void *action)
{
	memset(currentFilter->included, FALSE, sizeof currentFilter->included);
	SyncIncludedCount();
	RefreshFilterShuttle();
}

/**
 * CHANGE_LAYER notification callback: this dialog's Available/Included and
 * Groups lists are only ever populated when it's shown (RefreshFilterShuttle()/
 * RefreshFilterGroupsList() in ShowReportsFilterDialog()), not on every
 * relevant edit -- and it's non-modal (F_BLOCK is unimplemented on GTK3, see
 * dialog.c's wWinDialogCreate()), so the Manage > Layer Groups dialog can be
 * open and edited at the same time. Left unrefreshed, a Groups-list row's
 * cached group index can point at a *different* group than the one displayed
 * after a group elsewhere is deleted (LayerGroupDelete() shifts every later
 * index down by one) -- FilterAddGroup() would then silently apply the wrong
 * group's membership. Refresh live while visible, same pattern as dlayer.c's
 * own LayerChange().
 *
 * \param changes IN change bitmask from DoChangeNotification()
 */
static void ReportsFilterChangeNotify(long changes)
{
	if ((changes & CHANGE_LAYER) && reportsFilterPG.win != NULL &&
	    wWinIsVisible(reportsFilterPG.win)) {
		RefreshFilterShuttle();
		RefreshFilterGroupsList();
	}
}

/**
 * Show the shared Filter dialog, creating it on first use, re-targeted at
 * \p filter.
 *
 * \param filter IN/OUT the report's own filter state to edit
 */
void ShowReportsFilterDialog(reportsFilter_t *filter)
{
	currentFilter = filter;

	LOGREPORTSFILTER()
	LOG(log_reportsfilter, 1,
	    ("reportsfilter: dialog opened, %d layer(s) currently included\n",
	     filter->includedCount))

	if (reportsFilterPG.win == NULL) {
		/* Registered lazily on first show, same as every reportsDialog_t
		 * in reports.c itself (ReportsShowDialog()) -- there is no
		 * dedicated menu item for this dialog to hang an InitXxxDialog()
		 * off of, since it opens from each report's own "Filter..."
		 * button instead. */
		FormRegister(&reportsFilterPG);
		FormCreateDialog(&reportsFilterPG, MakeWindowTitle(_("Filter Layers/Groups")),
		                 NULL, NULL, NULL, FormCancel_Current,
		                 TRUE, F_RESIZE|F_RECALLSIZE|F_BLOCK, NULL);
		RegisterChangeNotification(ReportsFilterChangeNotify);
	}

	FormLoadControls(&reportsFilterPG);
	FormGroupRecord(&reportsFilterPG);
	RefreshFilterShuttle();
	RefreshFilterGroupsList();
	wShow(reportsFilterPG.win);
}

BOOL_T ReportsFilterActive(const reportsFilter_t *f)
{
	return f->includedCount > 0;
}

BOOL_T ReportsFilterLayerIncluded(const reportsFilter_t *f, unsigned int layer)
{
	if (f->includedCount == 0) {
		return TRUE;
	}
	return layer < NUM_LAYERS && f->included[layer];
}
