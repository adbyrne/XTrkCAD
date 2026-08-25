/**
 * \file   dialogs.c
 * \brief
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2024 Dave Bullis
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

#include <ctype.h>
#include <string.h>

#include <wlib.h>
#include <param.h>
#include <dynarray.h>
#include <form.h>
#include "formprivate.h"

int log_form = 0;

EXPORT char* prefSect = "DialogItem";

static dynArr_t dialogGroups_da;
#define dialogGroups(N) DYNARR_N( paramGroup_p, dialogGroups_da, N )

EXPORT paramGroup_cp DialogGroupFind( const char* sName )
{
	for ( paramGroup_cp * ppg = DialogGroupIter(NULL); ppg;
	      ppg = DialogGroupIter( ppg ) ) {
		if ( (*ppg)->nameStr == NULL ) { continue; }
		size_t len = strlen( (*ppg)->nameStr );
		if ( strncmp( (*ppg)->nameStr, sName, len ) == 0 &&
		     sName[len] == ' ' ) {
			return *ppg;
		}
	}
	return NULL;
}

EXPORT paramGroup_cp * DialogGroupIter( paramGroup_cp * ppg )
{
	if ( ppg == NULL ) {
		return &dialogGroups(0);
	}
	if ( ppg >= &DYNARR_LAST( paramGroup_cp, dialogGroups_da ) ) {
		return NULL;
	}
	ppg++;
	return ppg;
}


static void AddGroupPtrToItem(paramGroup_p pg)
{
	for (int i = 0; i < (pg->paramCnt); i++) {
		paramData_t* p = (pg->paramPtr) + i;
		p->group = pg;
	}
}

void FormRegister(paramGroup_p pg)
{
	DYNARR_APPEND(paramGroup_p, dialogGroups_da, 10);
	dialogGroups(dialogGroups_da.cnt - 1) = pg;

	AddGroupPtrToItem(pg);

	FormLoadDefaultValues(pg);
}

static void ButtonOk(paramGroup_p group)
{
	LOG(log_form, 1, ("DialogsButtonOk: %s\n", group->nameStr));
	if (!FormCheckInputs(group, (wControl_p)group->okB)) {
		return;
	}

	if (group->nameStr) {
		FormMacroRecord("PARAMETER %s %s\n", group->nameStr, "ok");
	}

	FormSaveDefaultValues(group);

	if (group->okProc) {
		group->okProc(group);
	}

	LOG(log_form, 1, ("DialogsButtonOk -> Ok\n"));

	wDialogSaveSizePos(group->win);

	wHide(group->win);
}

static void ButtonCancel(paramGroup_p group)
{
	FormMacroRecord("PARAMETER %s %s\n", group->nameStr, "cancel");

	if (group->cancelProc) {
		group->cancelProc(group);
	}
}

// wHelp() genuinely uses its argument as a topic string; the button
// dispatcher really does deliver the MyStrdup(helpStr) context here, so this
// wrapper matches wButtonCallBack_p's real signature instead of casting
// wHelp()'s const char* parameter through a mismatch.
static void ButtonHelp(void *context)
{
	wHelp((const char *)context);
}


static void DialogProc(
        wControl_p win,
        winProcEvent e,
        void* refresh,
        void* data)
{
	paramGroup_p pg = (paramGroup_p)data;
//	static int iResizeCnt = 0;
	switch (e) {
	case wClose_e:
		if (pg->changeProc) {
			pg->changeProc(pg, -1, NULL);
		}
		if ((pg->options & PGO_NODEFAULTPROC) == 0) {
			DefaultProc(win, wClose_e, data);
		}
		break;
	//case wResize_e:
	//	LOG(log_form, 1, ("DialogsDlgProc %d/n", iResizeCnt++));
	//	LayoutControls(pg, ParamPositionControl, NULL, NULL);
	//	break;
	default:
		break;
	}
}


/**
 * Alias table for GetDialogHelpTopic(). Maps a paramGroup_t dialog name to a
 * User Guide page (the *.dox files under app/doc) for the minority of
 * dialogs whose own name doesn't produce a real page under the standard
 * "cmd" + Capitalize(name) transform below -- e.g. a dialog whose logical
 * topic already has a differently-named existing page, or (temporarily,
 * until real content exists) an entry tracked in
 * app/doc/known_missing_dialog_help.txt.
 *
 * SF #730. Entries added in verified batches -- each one confirmed by
 * reading the target page's actual content, not assumed from name
 * similarity alone.
 */
static struct { const char *dialogName; const char *page; } dialogHelpAliases[]
= {
	/* Content confirmed by reading the target page, not name similarity. */
	{ "index", "cmdSelectIndex" }, /* menu.c's "Select Index" dialog */

	/* SF #730 batch 2: create/edit dialogs whose page already covers both
	 * (e.g. cmdBlock's "name and script" fields serve blockedit too). */
	{ "blockedit", "cmdBlock" },
	{ "switchmotoredit", "cmdSwitchmotor" },
	{ "controledit", "cmdControl" },
	{ "sensoredit", "cmdSensor" },
	{ "signaledit", "cmdSignal" },
	{ "aspectedit", "cmdSignal" }, /* cmdSignal's "list of aspects" section */

	/* Same command/feature, different existing page name. */
	{ "turnout", "cmdNewFixedTrack" }, /* cturnout.c's Fixed-Track dialog */
	{ "joinfixed", "cmdJoinTrack" },
	{ "addElev", "cmdRaiseElev" }, /* "Raise or lower all selected tracks" */
	{ "curvefixed", "cmdCurve" }, /* the "desired radius" field cmdCurve documents */
	{ "elev", "cmdElevation" },
	{ "cornuMod", "chgCornu" },

	/* cdraw.c's draw-tool property panels -> the matching Draw-menu page
	 * (each confirmed to discuss the same width/color fields). */
	{ "linedata", "cmdDrawLine" },
	{ "curvedata", "cmdDrawCurves" },
	{ "circledata", "cmdDrawCircles" },
	{ "polygondata", "cmdDrawPolygon" },
	{ "boxdata", "cmdDrawBox" },
	{ "benchstyle", "cmdDrawBench" },
	{ "dimensionstyle", "cmdDrawDimLine" },

	/* Modify-a-drawn-shape property panels -> chgDraw (Lines/Curves/Boxes
	 * covered inline there) or its dedicated Polygon/PolyLine subpage.
	 * drawModCircle deliberately NOT aliased -- chgDraw doesn't cover
	 * Circles explicitly, unlike the other 4 shapes; left tracked in
	 * known_missing_dialog_help.txt pending real content or a better match. */
	{ "drawModStraight", "chgDraw" },
	{ "drawModCurve", "chgDraw" },
	{ "drawModRect", "chgDraw" },
	{ "drawModPoly", "polyModify" },

	{ NULL, NULL }
};

/**
 * Resolve a dialog's Help-button topic. Given a paramGroup_t's own name,
 * returns either an alias to an existing page confirmed to document it, or
 * "cmd" + the name with its first letter capitalized -- the transform this
 * function replaces (previously computed inline in FormCreateDialog, moved
 * here so the same logic can consult the alias table first) -- written into
 * buffer (size bufferSize) and returned. Most dialogs' bare name already
 * produces a real page this way (e.g. "block" -> cmdBlock); only genuine
 * mismatches need an alias-table entry.
 */
static const char * GetDialogHelpTopic(const char *dialogName, char *buffer,
                                       size_t bufferSize)
{
	for (int i = 0; dialogHelpAliases[i].dialogName; i++) {
		if (strcmp(dialogHelpAliases[i].dialogName, dialogName) == 0) {
			return dialogHelpAliases[i].page;
		}
	}
	snprintf(buffer, bufferSize, "cmd%s", dialogName);
	buffer[3] = toupper((unsigned char)buffer[3]);
	return buffer;
}

/**
 * Create a dialog box from data definition.
 *
 * \param[in] group	data definition for the dialog
 * \param[in] title  title of the new dialog
 * \param[in] okLabel text for the affirmative button
 * \param[in] okProc	subroutine to call when ok is pressed
 * \param[in] cancelLabel text for the cancel button
 * \param[in] cancelProc if not NULL a subroutine for Cancel event. If NULL no cancel button is created
 * \param[in] needHelpButton if TRUE a help button is created
 * \param[in] winOption ???
 * \param[in] changeProc ???
 */

wControl_p FormCreateDialog(
        paramGroup_p group,
        char* title,
        char* okLabel,
        paramActionOkProc okProc,
        char *cancelLabel,
        paramActionCancelProc cancelProc,
        BOOL_T needHelpButton,
        long winOption,
        paramChangeProc changeProc)
{
	char helpStr[STR_SHORT_SIZE] = "";
	wWinPix_t w0, h0;

	winOption &= ~PD_F_ALT_CANCELLABEL;
	group->okProc = okProc;
	if ( cancelProc == FormCancel_Null ) {
		cancelProc = NULL;
	}
	group->cancelProc = cancelProc;
	group->layoutProc = NULL;
	group->changeProc = changeProc;
	group->winOption = winOption;

	if ((winOption & F_CENTER) == 0) {
		winOption |= F_RECALLPOS;
	}
	if ((winOption & F_RESIZE) != 0) {
		winOption |= F_RECALLSIZE;
	}

	if (group->options & PGO_FULLDIALOGFROMBUILDER) {
		winOption |= F_DEFINEDINBUILDER;
	}

	group->win = wWinDialogCreate(mainW, helpStr, title, group->nameStr,
	                              F_AUTOSIZE | winOption, DialogProc, group);

	if (group->okProc) {
		sprintf(helpStr, "%s-ok", group->nameStr);
		group->okB = wButtonCreate(group->win, 0, 0, "id_ok", okLabel, BB_DEFAULT, 0,
		                           ButtonOk, group);
	}
	if (group->cancelProc) {
		group->cancelB = wButtonCreate(group->win, 0, 0, "id_cancel", cancelLabel,
		                               BB_CANCEL,
		                               0, ButtonCancel, group);
	}
	if (needHelpButton) {
		/* SF #730: GetDialogHelpTopic() (app/bin/paramwrapper.c) applies the
		 * same "cmd" + Capitalize(name) default this used to compute inline,
		 * plus a small alias table for the minority of dialogs it doesn't
		 * fit. */
		group->helpB = wButtonCreate(group->win, 0, 0, "id_help", _("Help"), BB_HELP, 0,
		                             ButtonHelp, MyStrdup(GetDialogHelpTopic(group->nameStr, helpStr,
		                                     sizeof(helpStr))));
	}

	LOG(log_form, 1, ("DialogsCreateDialog/"));
	FormCreateControls(group);

	wWinGetSize(group->win, &w0, &h0);
	LOG(log_form, 1, ("    winSize: %dx%d\n", w0, h0));

	/** \todo the following code sets the limits for the window size, needs work */
	if ((winOption & F_RESIZE)) {

		wWinPix_t scr_w, scr_h;
		wGetDisplaySize(&scr_w, &scr_h);
		wSetGeometry(group->win, 10, scr_w - 10, 10, scr_h, -1, -1,
		             -1);
	}

	return group->win;
}

void
FormInit(void)
{
	log_form= LogFindIndex("dialogs");
	DYNARR_INIT(paramGroup_p, dialogGroups_da);
}

