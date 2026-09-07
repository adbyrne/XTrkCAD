/** \file dlayergroupui.c
 * Manage Layer Groups dialog (SF #222 phase 0, SF #782).
 *
 * A dual-list shuttle (Available / Included) per selected group, plus
 * New/Rename/Delete for the groups themselves. See layergroup.ui's own
 * header comment for why this shape was chosen over a single
 * multi-select-as-membership list.
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
#include "include/dlayergroupui.h"
#include "misc.h"

static int log_layergroups = -1;
#define LOGLAYERGROUPS() \
	if (log_layergroups < 0) { log_layergroups = LogFindIndex("layergroups"); }

static void GroupNew(void *action);
static void GroupRename(void *action);
static void GroupDelete(void *action);
static void GroupShowOnly(void *action);
static void GroupAddLayers(void *action);
static void GroupRemoveLayers(void *action);
static void RefreshGroupList(void);
static void RefreshShuttleLists(int groupIdx);

static paramData_t layerGroupPLs[] = {
#define I_GROUPLIST	(0)
#define groupsL		(layerGroupPLs[I_GROUPLIST].control)
	{	PD_LIST, NULL, "groups", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, 0 },
#define I_GROUPNEW	(1)
	{	PD_BUTTON, GroupNew, "new", 0, NULL, NULL },
#define I_GROUPRENAME	(2)
	{	PD_BUTTON, GroupRename, "rename", 0, NULL, NULL },
#define I_GROUPDELETE	(3)
	{	PD_BUTTON, GroupDelete, "delete", 0, NULL, NULL },
#define I_GROUPSHOWONLY	(4)
	{	PD_BUTTON, GroupShowOnly, "showonly", 0, NULL, NULL },
#define I_AVAILABLE	(5)
#define availableL	(layerGroupPLs[I_AVAILABLE].control)
	{	PD_LIST, NULL, "available", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
#define I_ADDLAYERS	(6)
	{	PD_BUTTON, GroupAddLayers, "addlayer", 0, NULL, NULL },
#define I_REMOVELAYERS	(7)
	{	PD_BUTTON, GroupRemoveLayers, "removelayer", 0, NULL, NULL },
#define I_INCLUDED	(8)
#define includedL	(layerGroupPLs[I_INCLUDED].control)
	{	PD_LIST, NULL, "included", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY },
};
static paramGroup_t layerGroupPG = { "layergroup", PGO_FULLDIALOGFROMBUILDER, layerGroupPLs, COUNT( layerGroupPLs ) };

/* "Enter a name" dialog, reused for both New and Rename -- same field
 * shape as dcustmgm.c's custMgmContentsPG (a single PD_STRING field, no
 * builder .ui needed for a dialog this simple).
 *
 * wShow()/FormCreateDialog() do NOT pump a nested event loop in this
 * codebase's GTK3 backend -- F_BLOCK is defined but never consumed
 * anywhere in app/wlib/gtk3lib (confirmed by grep), so a synchronous
 * "show it, then immediately read a confirmed-by-the-user flag" design
 * (the shape this file originally copied from dcustmgm.c's
 * CustomDoExport/custMgmContentsPG) always reads the flag before the
 * user has clicked anything -- found live via Xvfb click-through testing
 * (SF #782): the dialog appeared and accepted text, but confirming it
 * never created/renamed a group. Fixed by doing the actual create/rename
 * work inside the Ok button's own callback (LayerGroupNameOk) instead of
 * in the function that shows the dialog. */
static char layerGroupNameBuf[STR_SHORT_SIZE];
/** -1 = the pending Ok is for GroupNew(); else the group index GroupRename() is targeting. */
static int layerGroupNameTarget = -1;
static paramData_t layerGroupNamePLs[] = {
	{ PD_STRING, layerGroupNameBuf, "name", PDO_NOTBLANK, I2VP(30), N_("Group Name"), 0, 0, sizeof( layerGroupNameBuf ) }
};
static paramGroup_t layerGroupNamePG = { "layergroupname", 0, layerGroupNamePLs, COUNT( layerGroupNamePLs ) };

/**
 * Ok callback for the "enter a group name" dialog (layerGroupNamePG),
 * shared by GroupNew() and GroupRename(). Does the actual create/rename
 * work itself -- see this file's header comment for why ShowGroupNameDialog()
 * can't do it synchronously.
 *
 * \param junk IN unused, required by the PD_BUTTON/FormButton signature
 */
static void LayerGroupNameOk(void *junk)
{
	/* FormCheckInputs() (called by the generic ButtonOk() before this
	 * callback runs) only checks pre-existing p->bInvalid flags -- it does
	 * NOT pull the live widget text into layerGroupNameBuf. That sync only
	 * happens via FormUpdate(), which nothing calls automatically for a
	 * plain (non-builder) dialog's Ok button; other callers
	 * (cblock.c/doption.c/cgroup.c) call it themselves for the same
	 * reason. Found live via Xvfb click-through testing (SF #782): typing
	 * a name and clicking Ok silently created/renamed using an empty
	 * string every time, since layerGroupNameBuf was still whatever it was
	 * set to before the dialog was shown. */
	FormUpdate(&layerGroupNamePG);

	wHide(layerGroupNamePG.win);

	if (layerGroupNameTarget < 0) {
		int idx = LayerGroupCreate(layerGroupNameBuf);
		if (idx < 0) {
			return;
		}

		LOGLAYERGROUPS()
		LOG(log_layergroups, 1, ("layergroups: created group %d \"%s\"\n", idx,
		                         layerGroupNameBuf))

		changed++;
		DoChangeNotification(CHANGE_LAYER);
		RefreshGroupList();
		wListSetIndex(groupsL, idx);
		RefreshShuttleLists(idx);
		FormControlActive(&layerGroupPG, I_GROUPRENAME, TRUE);
		FormControlActive(&layerGroupPG, I_GROUPDELETE, TRUE);
		FormControlActive(&layerGroupPG, I_GROUPSHOWONLY, TRUE);
	} else {
		LOGLAYERGROUPS()
		LOG(log_layergroups, 1, ("layergroups: renamed group %d \"%s\" -> \"%s\"\n",
		                         layerGroupNameTarget, LayerGroupName(layerGroupNameTarget),
		                         layerGroupNameBuf))

		LayerGroupRename(layerGroupNameTarget, layerGroupNameBuf);
		changed++;
		DoChangeNotification(CHANGE_LAYER);
		RefreshGroupList();
	}
}

/**
 * Show the "enter a group name" dialog, pre-filled with \p initial. The
 * actual create/rename happens later, in LayerGroupNameOk(), once the
 * user confirms -- see this file's header comment on why showing the
 * dialog can't just synchronously return whether they did.
 *
 * \param title IN dialog title ("New Group" / "Rename Group")
 * \param initial IN starting text, "" for a blank field
 * \param targetGroup IN -1 to create a new group, else the group index
 *                       to rename once confirmed
 */
static void ShowGroupNameDialog(const char *title, const char *initial,
                                int targetGroup)
{
	layerGroupNameTarget = targetGroup;
	strncpy(layerGroupNameBuf, initial ? initial : "",
	        sizeof layerGroupNameBuf - 1);
	layerGroupNameBuf[sizeof layerGroupNameBuf - 1] = '\0';

	if (layerGroupNamePG.win == NULL) {
		FormCreateDialog(&layerGroupNamePG, MakeWindowTitle(title), _("Ok"),
		                 LayerGroupNameOk, _("Cancel"), FormCancel_Current,
		                 TRUE, 0, NULL);
	} else {
		wWinSetTitle(layerGroupNamePG.win, MakeWindowTitle(title));
	}
	FormLoadControls(&layerGroupNamePG);
	wShow(layerGroupNamePG.win);
	/* So the user can start typing the name immediately, no click needed
	 * first -- found worth fixing via live user testing (SF #782). */
	wControlSetFocus(layerGroupNamePLs[0].control);
}

/**
 * Rebuild the Available/Included lists from scratch for \p groupIdx,
 * every named layer going into exactly one of the two depending on
 * LayerGroupHasMember() -- never both, never skipped.
 *
 * \param groupIdx IN group to reflect, or -1 for "no group selected"
 *                    (Included ends up empty, Available holds every layer)
 */
static void RefreshShuttleLists(int groupIdx)
{
	wListClear(availableL);
	wListClear(includedL);

	for (unsigned int inx = 0; inx < NUM_LAYERS; inx++) {
		if (inx != 0 && strlen(GetLayerName(inx)) == 0) {
			continue;
		}

		char *label = FormatLayerName(inx);
		int layerIdx1Based = (int)(inx + 1);

		if (groupIdx >= 0 && LayerGroupHasMember(groupIdx, layerIdx1Based)) {
			wListAddValue(includedL, label, NULL, I2VP(inx));
		} else {
			wListAddValue(availableL, label, NULL, I2VP(inx));
		}
		free(label);
	}
}

/**
 * Rebuild the groups list, keeping the previous selection if it's still
 * valid (else selecting the first group, or none if the list is empty),
 * and refreshing the shuttle lists/button states to match.
 */
static void RefreshGroupList(void)
{
	int prevSelected = groupsL ? wListGetIndex(groupsL) : -1;

	wListClear(groupsL);
	for (int i = 0; i < LayerGroupCount(); i++) {
		wListAddValue(groupsL, LayerGroupName(i), NULL, I2VP(i));
	}

	int newSelected = -1;
	if (LayerGroupCount() > 0) {
		newSelected = (prevSelected >= 0 && prevSelected < LayerGroupCount()) ?
		              prevSelected : 0;
		wListSetIndex(groupsL, newSelected);
	}

	FormControlActive(&layerGroupPG, I_GROUPRENAME, newSelected >= 0);
	FormControlActive(&layerGroupPG, I_GROUPDELETE, newSelected >= 0);
	FormControlActive(&layerGroupPG, I_GROUPSHOWONLY, newSelected >= 0);
	RefreshShuttleLists(newSelected);
}

/**
 * "New..." button: open the name-entry dialog to create a group.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void GroupNew(void *action)
{
	ShowGroupNameDialog(_("New Group"), "", -1);
}

/**
 * "Rename..." button: open the name-entry dialog, pre-filled with the
 * selected group's current name, to rename it. A no-op if no group is
 * selected (button is inactive in that state, but guard anyway).
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void GroupRename(void *action)
{
	int selectedGroup = wListGetIndex(groupsL);
	if (selectedGroup < 0) {
		return;
	}

	ShowGroupNameDialog(_("Rename Group"), LayerGroupName(selectedGroup),
	                    selectedGroup);
}

/**
 * "Delete" button: confirm, then delete the selected group (its layers
 * are untouched -- only the group's own membership list is discarded).
 * A no-op if no group is selected.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void GroupDelete(void *action)
{
	int selectedGroup = wListGetIndex(groupsL);
	if (selectedGroup < 0) {
		return;
	}

	if (!NoticeMessage2(1, _("Delete group \"%s\"?"), _("Yes"), _("No"),
	                    LayerGroupName(selectedGroup))) {
		return;
	}

	LOGLAYERGROUPS()
	LOG(log_layergroups, 1, ("layergroups: deleted group %d \"%s\"\n",
	                         selectedGroup, LayerGroupName(selectedGroup)))

	LayerGroupDelete(selectedGroup);
	changed++;
	DoChangeNotification(CHANGE_LAYER);
	RefreshGroupList();
}

/**
 * Move every selected row of \p fromList into group \p groupIdx's
 * membership (if \p adding) or out of it (if not), by 1-based layer
 * index stashed as each row's context. Caller refreshes the shuttle
 * lists afterward -- this only mutates the data model.
 */
static void TransferSelected(wControl_p fromList, int groupIdx, BOOL_T adding)
{
	wIndex_t cnt = wListGetCount(fromList);

	for (wIndex_t inx = 0; inx < cnt; inx++) {
		if (!wListGetItemSelected(fromList, inx)) {
			continue;
		}

		int layerInx = (int)(intptr_t) wListGetItemContext(fromList, inx);
		int layerIdx1Based = layerInx + 1;

		LOGLAYERGROUPS()
		LOG(log_layergroups, 1, ("layergroups: group %d %s layer %d\n", groupIdx,
		                         adding ? "add" : "remove", layerIdx1Based))

		if (adding) {
			LayerGroupAddMember(groupIdx, layerIdx1Based);
		} else {
			LayerGroupRemoveMember(groupIdx, layerIdx1Based);
		}
	}
}

/**
 * "→" button: add every selected row of the Available list to the
 * selected group's membership. A no-op if no group is selected.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void GroupAddLayers(void *action)
{
	int selectedGroup = wListGetIndex(groupsL);
	if (selectedGroup < 0) {
		return;
	}

	TransferSelected(availableL, selectedGroup, TRUE);
	changed++;
	DoChangeNotification(CHANGE_LAYER);
	RefreshShuttleLists(selectedGroup);
}

/**
 * "←" button: remove every selected row of the Included list from the
 * selected group's membership. A no-op if no group is selected.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void GroupRemoveLayers(void *action)
{
	int selectedGroup = wListGetIndex(groupsL);
	if (selectedGroup < 0) {
		return;
	}

	TransferSelected(includedL, selectedGroup, FALSE);
	changed++;
	DoChangeNotification(CHANGE_LAYER);
	RefreshShuttleLists(selectedGroup);
}

/**
 * "Show Only" button: hide every layer except the selected group's
 * members (LayerGroupShowOnly() in dlayer.c also always keeps the
 * current layer visible). A no-op if no group is selected.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void GroupShowOnly(void *action)
{
	int selectedGroup = wListGetIndex(groupsL);
	if (selectedGroup < 0) {
		return;
	}

	LOGLAYERGROUPS()
	LOG(log_layergroups, 1, ("layergroups: show only group %d \"%s\"\n",
	                         selectedGroup, LayerGroupName(selectedGroup)))

	LayerGroupShowOnly(selectedGroup);
	changed++;
	DoChangeNotification(CHANGE_LAYER);
}

/**
 * FormCreateDialog() update callback: reacts to the groups list's
 * selection changing by re-enabling/disabling the per-group action
 * buttons and rebuilding the Available/Included shuttle for the newly
 * selected group.
 *
 * \param pg IN the dialog's paramGroup_t (unused, always layerGroupPG)
 * \param inx IN index of the paramData_t whose value changed
 * \param valueP IN new value (unused)
 * \return always FALSE (no further generic handling needed)
 */
static wBool_t LayerGroupDlgUpdate(paramGroup_p pg, int inx, void *valueP)
{
	if (inx == I_GROUPLIST) {
		int selectedGroup = wListGetIndex(groupsL);
		FormControlActive(&layerGroupPG, I_GROUPRENAME, selectedGroup >= 0);
		FormControlActive(&layerGroupPG, I_GROUPDELETE, selectedGroup >= 0);
		FormControlActive(&layerGroupPG, I_GROUPSHOWONLY, selectedGroup >= 0);
		RefreshShuttleLists(selectedGroup);
	}
	return FALSE;
}

/**
 * "Done" button: just hide the dialog. All create/rename/delete/
 * membership actions already committed to the data model immediately
 * when performed, so there's nothing left to apply or cancel here.
 *
 * \param junk IN unused, required by the FormCreateDialog() Ok-action signature
 */
static void LayerGroupDone(void *junk)
{
	wHide(layerGroupPG.win);
}

/**
 * Show the Manage Layer Groups dialog, creating it on first use.
 *
 * \param unused IN unused, required by the addButtonCallBack_t signature
 */
static void DoLayerGroups(void *unused)
{
	LOGLAYERGROUPS()
	LOG(log_layergroups, 1, ("layergroups: dialog opened, %d group(s) defined\n",
	                         LayerGroupCount()))

	if (layerGroupPG.win == NULL) {
		FormCreateDialog(&layerGroupPG, MakeWindowTitle(_("Layer Groups")),
		                 NULL, LayerGroupDone, NULL, FormCancel_Current,
		                 TRUE, F_RESIZE|F_RECALLSIZE|F_BLOCK,
		                 LayerGroupDlgUpdate);
	}

	FormLoadControls(&layerGroupPG);
	FormGroupRecord(&layerGroupPG);
	RefreshGroupList();
	wShow(layerGroupPG.win);
}

/**
 * One-time setup for the Layer Groups feature's menu wiring: registers
 * both this dialog's paramGroup_t and the shared name-entry sub-dialog's
 * (a missing FormRegister() on the latter crashed CreateControl() the
 * first time GroupNew()/GroupRename() ran -- found live via Xvfb
 * click-through testing, SF #782) and returns the callback to hand to
 * MiscMenuItemCreate() for the "Layer Groups ..." menu item.
 *
 * \return callback that shows the dialog when the menu item is chosen
 */
EXPORT addButtonCallBack_t InitLayerGroupsDialog(void)
{
	FormRegister(&layerGroupPG);
	FormRegister(&layerGroupNamePG);
	return &DoLayerGroups;
}
