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

#include <wlib.h>
#include <param.h>
#include <dynarray.h>
#include <dialogs.h>
#include "dialogsprivate.h"

int log_dialogs = 0;
static dynArr_t dialogGroups_da;
#define dialogGroups(N) DYNARR_N( paramGroup_p, dialogGroups_da, N )

static AddGroupPtrToItem(paramGroup_p pg)
{
	for (int i = 0; i < (pg->paramCnt); i++) {
		paramData_t* p = (pg->paramPtr) + i;
		p->group = pg;
	}
}

void DialogsRegister(paramGroup_p pg)
{
	DYNARR_APPEND(paramGroup_p, dialogGroups_da, 10);
	dialogGroups(dialogGroups_da.cnt - 1) = pg;

	AddGroupPtrToItem(pg);

	DialogsSetDefaultValues(pg);
}

static void DialogsButtonOk(paramGroup_p group)
{
	LOG(log_dialogs, 1, ("DialogsButtonOk: %s\n", group->nameStr));
	if (!ParamCheckInputs(group, (wControl_p)group->okB)) {
		return;
	}
	/** \todo Recording */
	//if (recordParamF && group->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s\n", group->nameStr, "ok");
	//	fflush(recordParamF);
	//}

	if (group->okProc) {
		group->okProc(group);
	}

	wControlSetBalloon((wControl_p)group->okB, 0, 0, NULL);

	LOG(log_dialogs, 1, ("DialogsButtonOk -> Ok\n"));
}

static void DialogsButtonCancel(paramGroup_p group)
{

	//if (recordParamF && group->nameStr) {
	//	fprintf(recordParamF, "PARAMETER %s %s\n", group->nameStr, "cancel");
	//	fflush(recordParamF);
	//}
	if (group->cancelProc) {
		group->cancelProc(group->win);
	}
}


static void DialogsDlgProc(
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
	//	LOG(log_dialogs, 1, ("DialogsDlgProc %d/n", iResizeCnt++));
	//	LayoutControls(pg, ParamPositionControl, NULL, NULL);
	//	break;
	default:
		break;
	}
}



/**
 * Create a dialog box from data definition.
 *
 * \param IN group	data definition for the dialog
 * \param IN title  title of the new dialog
 * \param IN okLabel text for the affirmative button
 * \param IN okProc	subroutine to call when ok is pressed
 * \param IN cancelLabel text for the cancel button
 * \param IN cancelProc if not NULL a subroutine for Cancel event. If NULL no cancel button is created
 * \param IN needHelpButton if TRUE a help button is created
 * \param IN winOption ???
 * \param IN changeProc ???
 */

wControl_p DialogsCreateDialog(
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

	group->win = wWinDialogCreate(mainW, helpStr, title, group->nameStr, 
		F_AUTOSIZE | winOption | BO_DIALOGFROMBUILDER, DialogsDlgProc, group);

	if (okLabel && okProc) {
		sprintf(helpStr, "%s-ok", group->nameStr);
		group->okB = wButtonCreate(group->win, 0, 0, "id_ok", okLabel, BB_DEFAULT, 0,
			DialogsButtonOk, group);
	}
	if (group->cancelProc) {
		group->cancelB = wButtonCreate(group->win, 0, 0, "id_cancel", cancelLabel, BB_CANCEL,
			0, DialogsButtonCancel, group);
	}
	if (needHelpButton) {
		sprintf(helpStr, "cmd%s", group->nameStr);
		helpStr[3] = toupper((unsigned char)helpStr[3]);
		group->helpB = wButtonCreate(group->win, 0, 0, "id_help", _("Help"), BB_HELP, 0,
			(wButtonCallBack_p)wHelp, MyStrdup(helpStr));
	}

	LOG(log_dialogs, 1, ("DialogsCreateDialog/"));
	CreateControls(group);
//	LayoutControls(group, ParamCreateControl, &group->origW, &group->origH);

	wWinGetSize(group->win, &w0, &h0);
	LOG(log_dialogs, 1, ("    winSize: %dx%d\n", w0, h0));

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
DialogsInit(void)
{
	log_dialogs= LogFindIndex("dialogs");
	DYNARR_INIT(paramGroup_p, dialogGroups_da);
}

