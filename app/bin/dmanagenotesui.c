/** \file dmanagenotesui.c
 * Manage Notes dialog (SF #800 phase 3).
 *
 * A flat list + Add/Delete for the ROOT Names registry (notenames.h) that
 * the Notes Report's "ROOT Names" filter/grouping reads from (see
 * ReportsNoteResolveGroup(), reports.h), plus a "Search for Names" button
 * that scans this layout's JSON Notes for ROOT-level field names not yet
 * registered and adds them -- the "or picks it from a search" convenience
 * named in SF #800's own ticket text. Modeled on dlayergroupui.c's list+
 * buttons column, minus its Available/Included shuttle: this registry has
 * no per-entry membership, just names.
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

#include "cJSON.h"
#include "common-ui.h"
#include "common.h"
#include "custom.h"
#include "form.h"
#include "include/dmanagenotesui.h"
#include "include/notenames.h"
#include "misc.h"
#include "note.h"
#include "track.h"

static int log_managenotes = -1;
#define LOGMANAGENOTES() \
	if (log_managenotes < 0) { log_managenotes = LogFindIndex("managenotes"); }

static void NameNew(void *action);
static void NameDelete(void *action);
static void NameSearch(void *action);
static void RefreshNameList(void);

static paramData_t manageNotesPLs[] = {
#define I_NAMELIST	(0)
#define namesL		(manageNotesPLs[I_NAMELIST].control)
	{	PD_LIST, NULL, "names", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, 0 },
#define I_NAMENEW	(1)
	{	PD_BUTTON, NameNew, "new", 0, NULL, NULL },
#define I_NAMEDELETE	(2)
	{	PD_BUTTON, NameDelete, "delete", 0, NULL, NULL },
#define I_NAMESEARCH	(3)
	{	PD_BUTTON, NameSearch, "search", 0, NULL, NULL },
};
static paramGroup_t manageNotesPG = { "managenotes", PGO_FULLDIALOGFROMBUILDER, manageNotesPLs, COUNT( manageNotesPLs ) };

/* "Enter a name" dialog for Add -- same shape/rationale as
 * dlayergroupui.c's layerGroupNamePLs (see that file's header comment for
 * why the actual work happens in the Ok callback, not synchronously in
 * the function that shows the dialog: FormCreateDialog()/wShow() don't
 * pump a nested event loop in this codebase's GTK3 backend). */
static char manageNoteNameBuf[NOTENAME_SIZE];
static paramData_t manageNotesNamePLs[] = {
	{ PD_STRING, manageNoteNameBuf, "name", PDO_NOTBLANK, I2VP(30), N_("ROOT-Level Name"), 0, 0, sizeof( manageNoteNameBuf ) }
};
static paramGroup_t manageNotesNamePG = { "managenotesname", 0, manageNotesNamePLs, COUNT( manageNotesNamePLs ) };

/**
 * Ok callback for the "enter a name" dialog (manageNotesNamePG).
 *
 * \param junk IN unused, required by the PD_BUTTON/FormButton signature
 */
static void ManageNotesNameOk(void *junk)
{
	/* FormUpdate() is required to pull the live widget text into
	 * manageNoteNameBuf before use -- see dlayergroupui.c's
	 * LayerGroupNameOk() header comment for the underlying wlib gap this
	 * works around (already found+fixed live via SF #782). */
	FormUpdate(&manageNotesNamePG);

	wHide(manageNotesNamePG.win);

	int idx = NoteNameAdd(manageNoteNameBuf);
	if (idx < 0) {
		return;
	}

	LOGMANAGENOTES()
	LOG(log_managenotes, 1, ("managenotes: registered name %d \"%s\"\n", idx,
	                         manageNoteNameBuf))

	changed++;
	RefreshNameList();
}

/**
 * "Add..." button: open the name-entry dialog to register a new ROOT
 * Name.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void NameNew(void *action)
{
	manageNoteNameBuf[0] = '\0';

	if (manageNotesNamePG.win == NULL) {
		FormCreateDialog(&manageNotesNamePG, MakeWindowTitle(_("Add ROOT Name")),
		                 _("Ok"), ManageNotesNameOk, _("Cancel"), FormCancel_Current,
		                 TRUE, 0, NULL);
	} else {
		wWinSetTitle(manageNotesNamePG.win, MakeWindowTitle(_("Add ROOT Name")));
	}
	FormLoadControls(&manageNotesNamePG);
	wShow(manageNotesNamePG.win);
	wControlSetFocus(manageNotesNamePLs[0].control);
}

/**
 * "Delete" button: remove the selected name from the registry. A no-op
 * if none is selected.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void NameDelete(void *action)
{
	int selected = wListGetIndex(namesL);
	if (selected < 0) {
		return;
	}

	LOGMANAGENOTES()
	LOG(log_managenotes, 1, ("managenotes: deleted name %d \"%s\"\n",
	                         selected, NoteNameAt(selected)))

	NoteNameRemove(selected);
	changed++;
	RefreshNameList();
}

/**
 * "Search for Names" button: scan every JSON Note (T_NOTE, OP_NOTEJSON)
 * in the layout for its ROOT-level object keys and register any not
 * already in the registry. Reports how many were found via a notice --
 * matches SF #800's own ticket text ("or picks it from a search, if a
 * search-assist is added later"), a direct add-and-report rather than a
 * second candidate-review list, to keep this phase's scope to the
 * registry itself.
 *
 * \param action IN unused, required by the PD_BUTTON signature
 */
static void NameSearch(void *action)
{
	int added = 0;
	track_p trk;

	/* NoteNameAdd()'s return value alone can't distinguish "already
	 * registered" (returns the existing index) from "just added as the
	 * new last index" when the name happens to already be the most
	 * recent registration -- count real additions by comparing
	 * NoteNameCount() before/after each call instead. */
	TRK_ITERATE( trk ) {
		if ( GetTrkType(trk) != T_NOTE || !IsJsonNote(trk) ) {
			continue;
		}

		struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
		cJSON *parsed = cJSON_Parse(xx->noteData.text);
		if (parsed == NULL || !cJSON_IsObject(parsed)) {
			if (parsed) {
				cJSON_Delete(parsed);
			}
			continue;
		}

		for (cJSON *child = parsed->child; child != NULL; child = child->next) {
			int before;
			if (child->string == NULL || child->string[0] == '\0') {
				continue;
			}
			before = NoteNameCount();
			NoteNameAdd(child->string);
			if (NoteNameCount() > before) {
				added++;
			}
		}

		cJSON_Delete(parsed);
	}

	LOGMANAGENOTES()
	LOG(log_managenotes, 1, ("managenotes: search found %d new name(s)\n", added))

	if (added > 0) {
		changed++;
		RefreshNameList();
	}

	if (added > 0) {
		InfoMessage(_("Found and registered %d new ROOT-level name(s)."), added);
	} else {
		InfoMessage(_("No new ROOT-level names found."));
	}
}

/**
 * Rebuild the names list, keeping the previous selection if it's still
 * valid.
 */
static void RefreshNameList(void)
{
	int prevSelected = namesL ? wListGetIndex(namesL) : -1;

	wListClear(namesL);
	for (int i = 0; i < NoteNameCount(); i++) {
		wListAddValue(namesL, NoteNameAt(i), NULL, I2VP(i));
	}

	if (NoteNameCount() > 0) {
		int newSelected = (prevSelected >= 0 && prevSelected < NoteNameCount()) ?
		                  prevSelected : 0;
		wListSetIndex(namesL, newSelected);
	}
}

/**
 * "Done" button: just hide the dialog. Add/Delete/Search all already
 * committed to the data model immediately when performed.
 *
 * \param junk IN unused, required by the FormCreateDialog() Ok-action signature
 */
static void ManageNotesDone(void *junk)
{
	wHide(manageNotesPG.win);
}

/**
 * Show the Manage Notes dialog, creating it on first use.
 *
 * \param unused IN unused, required by the addButtonCallBack_t signature
 */
static void DoManageNotes(void *unused)
{
	LOGMANAGENOTES()
	LOG(log_managenotes, 1, ("managenotes: dialog opened, %d name(s) registered\n",
	                         NoteNameCount()))

	if (manageNotesPG.win == NULL) {
		FormCreateDialog(&manageNotesPG, MakeWindowTitle(_("Manage Notes")),
		                 NULL, ManageNotesDone, NULL, FormCancel_Current,
		                 TRUE, F_RESIZE|F_RECALLSIZE|F_BLOCK, NULL);
	}

	FormLoadControls(&manageNotesPG);
	FormGroupRecord(&manageNotesPG);
	RefreshNameList();
	wShow(manageNotesPG.win);
}

/**
 * One-time setup for the Manage Notes feature's menu wiring: registers
 * both this dialog's paramGroup_t and the name-entry sub-dialog's, and
 * returns the callback to hand to MiscMenuItemCreate() for the
 * "Manage Notes ..." menu item.
 *
 * \return callback that shows the dialog when the menu item is chosen
 */
EXPORT addButtonCallBack_t InitManageNotesDialog(void)
{
	FormRegister(&manageNotesPG);
	FormRegister(&manageNotesNamePG);
	return &DoManageNotes;
}
