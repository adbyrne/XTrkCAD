/** \file dselectlayers.c
 * "Select Layers/Groups..." dialog (SF #222 phase 1, SF #787).
 *
 * Two independent multi-select lists -- Layers and Groups -- combined by
 * union when Select/Deselect is clicked. Deliberately not a single
 * checklist with a "check this group's members" quick-apply action: wlib
 * has no API to programmatically pre-select specific rows in a PD_LIST
 * (the same constraint documented in dlayergroupui.c/layergroup.ui, found
 * during SF #782), so a group's membership can't be reflected as
 * pre-checked rows in a shared layer list. Instead, checking a group row
 * and clicking Select/Deselect expands that group's *current* membership
 * directly against the real canvas selection -- a one-shot action, not a
 * live link -- so deselecting one member of an applied group afterward is
 * just a normal follow-up Select/Deselect on that one layer in the Layers
 * list, no different from deselecting any other layer.
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
#include "cselect.h"
#include "custom.h"
#include "dlayer.h"
#include "form.h"
#include "include/dlayergroup.h"
#include "include/dselectlayers.h"
#include "misc.h"

/** @logcmd @showrefby `selectlayers=n` `dselectlayers.c` - logs Select/Deselect
 * actions from the Select Layers/Groups dialog */
static int log_selectlayers = -1;
#define LOGSELECTLAYERS() \
	if (log_selectlayers < 0) { log_selectlayers = LogFindIndex("selectlayers"); }

static void DoSelect(void *action);
static void DoDeselect(void *action);

static paramData_t selectLayersPLs[] = {
#define I_LAYERLIST	(0)
#define layersL		(selectLayersPLs[I_LAYERLIST].control)
	{	PD_LIST, NULL, "layers", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
#define I_GROUPLIST	(1)
#define groupsL		(selectLayersPLs[I_GROUPLIST].control)
	{	PD_LIST, NULL, "groups", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
#define I_SELECT	(2)
	{	PD_BUTTON, DoSelect, "select", 0, NULL, NULL },
#define I_DESELECT	(3)
	{	PD_BUTTON, DoDeselect, "deselect", 0, NULL, NULL },
};
static paramGroup_t selectLayersPG = { "selectlayers", PGO_FULLDIALOGFROMBUILDER, selectLayersPLs, COUNT( selectLayersPLs ) };

/**
 * Rebuild the Layers list from scratch, one row per named layer (layer 0
 * is always included even if unnamed -- same convention as
 * dlayergroupui.c's RefreshShuttleLists()). Row context is the 0-based
 * layer index.
 */
static void RefreshLayerList(void)
{
	wListClear(layersL);

	for (unsigned int inx = 0; inx < NUM_LAYERS; inx++) {
		if (inx != 0 && strlen(GetLayerName(inx)) == 0) {
			continue;
		}

		char *label = FormatLayerName(inx);
		wListAddValue(layersL, label, NULL, I2VP(inx));
		free(label);
	}
}

/**
 * Rebuild the Groups list from scratch. Row context is the group index.
 */
static void RefreshGroupsList(void)
{
	wListClear(groupsL);

	for (int i = 0; i < LayerGroupCount(); i++) {
		wListAddValue(groupsL, LayerGroupName(i), NULL, I2VP(i));
	}
}

/**
 * Collect the 0-based layer indices to act on: every checked row of the
 * Layers list, plus every current member of every checked row of the
 * Groups list. A layer reachable both directly and via a checked group is
 * only written once.
 *
 * \param out OUT array of at least NUM_LAYERS entries
 * \return number of layer indices written to \p out
 */
static int CollectWantedLayers(unsigned int *out)
{
	BOOL_T wanted[NUM_LAYERS];
	memset(wanted, FALSE, sizeof wanted);

	wIndex_t layerCnt = wListGetCount(layersL);
	for (wIndex_t inx = 0; inx < layerCnt; inx++) {
		if (!wListGetItemSelected(layersL, inx)) {
			continue;
		}
		unsigned int layer = (unsigned int)(intptr_t) wListGetItemContext(layersL, inx);
		if (layer < NUM_LAYERS) {
			wanted[layer] = TRUE;
		}
	}

	wIndex_t groupCnt = wListGetCount(groupsL);
	for (wIndex_t inx = 0; inx < groupCnt; inx++) {
		if (!wListGetItemSelected(groupsL, inx)) {
			continue;
		}
		int groupIdx = (int)(intptr_t) wListGetItemContext(groupsL, inx);
		int memberCnt = LayerGroupMemberCount(groupIdx);
		for (int m = 0; m < memberCnt; m++) {
			int layerIdx1Based = LayerGroupMemberAt(groupIdx, m);
			if (layerIdx1Based > 0 && (unsigned int)(layerIdx1Based - 1) < NUM_LAYERS) {
				wanted[layerIdx1Based - 1] = TRUE;
			}
		}
	}

	int count = 0;
	for (unsigned int i = 0; i < NUM_LAYERS; i++) {
		if (wanted[i]) {
			out[count++] = i;
		}
	}
	return count;
}

/**
 * "Select" button: select every track on the checked layers/groups.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void DoSelect(void *action)
{
	unsigned int layers[NUM_LAYERS];
	int count = CollectWantedLayers(layers);
	if (count == 0) {
		return;
	}

	LOGSELECTLAYERS()
	LOG(log_selectlayers, 1, ("selectlayers: select %d layer(s)\n", count))

	SelectLayerSet(layers, count, TRUE);
}

/**
 * "Deselect" button: deselect every track on the checked layers/groups.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void DoDeselect(void *action)
{
	unsigned int layers[NUM_LAYERS];
	int count = CollectWantedLayers(layers);
	if (count == 0) {
		return;
	}

	LOGSELECTLAYERS()
	LOG(log_selectlayers, 1, ("selectlayers: deselect %d layer(s)\n", count))

	SelectLayerSet(layers, count, FALSE);
}

/**
 * CHANGE_LAYER notification callback: this dialog's Layers and Groups lists
 * are only ever populated when it's shown (RefreshLayerList()/
 * RefreshGroupsList() in DoSelectLayersDialog()), not on every relevant
 * edit -- and it's non-modal (F_BLOCK is unimplemented on GTK3, see
 * dialog.c's wWinDialogCreate()), so the Manage > Layer Groups dialog can be
 * open and edited at the same time. Left unrefreshed, a Groups-list row's
 * cached group index can point at a *different* group than the one displayed
 * after a group elsewhere is deleted (LayerGroupDelete() shifts every later
 * index down by one) -- selecting/deselecting by that stale row would
 * silently act on the wrong group's membership. Refresh live while visible,
 * same pattern as dlayer.c's own LayerChange().
 *
 * \param changes IN change bitmask from DoChangeNotification()
 */
static void SelectLayersChangeNotify(long changes)
{
	if ((changes & CHANGE_LAYER) && selectLayersPG.win != NULL &&
	    wWinIsVisible(selectLayersPG.win)) {
		RefreshLayerList();
		RefreshGroupsList();
	}
}

/**
 * Show the Select Layers/Groups dialog, creating it on first use. There is
 * no separate Ok action -- Select/Deselect already act on the canvas
 * selection immediately -- so \c okProc is NULL (matching reports.c's
 * dialogs) and "Done" is just the Cancel button (labeled "_Done" in
 * selectlayers.ui) hiding the window via the default FormCancel_Current.
 * A non-NULL okProc would need an "id_ok" widget the .ui deliberately
 * doesn't define, since there's nothing for a separate Ok action to do.
 *
 * \param unused IN unused, required by the addButtonCallBack_t signature
 */
static void DoSelectLayersDialog(void *unused)
{
	if (selectLayersPG.win == NULL) {
		FormCreateDialog(&selectLayersPG, MakeWindowTitle(_("Select Layers/Groups")),
		                 NULL, NULL, NULL, FormCancel_Current,
		                 TRUE, F_RESIZE|F_RECALLSIZE|F_BLOCK, NULL);
		RegisterChangeNotification(SelectLayersChangeNotify);
	}

	FormLoadControls(&selectLayersPG);
	FormGroupRecord(&selectLayersPG);
	RefreshLayerList();
	RefreshGroupsList();
	wShow(selectLayersPG.win);
}

/**
 * One-time setup for this dialog's menu wiring: registers its
 * paramGroup_t and returns the callback to hand to MiscMenuItemCreate()
 * for the "Select Layers/Groups ..." menu item.
 *
 * \return callback that shows the dialog when the menu item is chosen
 */
EXPORT addButtonCallBack_t InitSelectLayersDialog(void)
{
	FormRegister(&selectLayersPG);
	return &DoSelectLayersDialog;
}
