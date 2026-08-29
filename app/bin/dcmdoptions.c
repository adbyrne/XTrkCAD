/**
 * \file   dcmdoptions.c
 * \brief  Command Options Dialog
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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
#include <form.h>
#include "i18n.h"
#include "cselect.h"

static wControl_p cmdoptW;

static paramData_t cmdoptPLs[] = {
	{ PD_RADIO, &preSelect, "preselect", PDO_NOPSHUPD, .context = I2VP(CHANGE_CMDOPT) },
	{ PD_RADIO, &rightClickMode, "rightclickmode", PDO_NOPSHUPD, .context = I2VP(CHANGE_CMDOPT)},
	{ PD_RADIO, &selectMode, "selectmode", PDO_NOPSHUPD, .context = I2VP(CHANGE_CMDOPT)},
	{ PD_TOGGLE, &selectZero, "selectzero", PDO_NOPSHUPD, .context = I2VP(CHANGE_CMDOPT)}
};
static paramGroup_t cmdoptPG = { "cmdopt", PGO_RECORD | PGO_PREFMISC | PGO_FULLDIALOGFROMBUILDER, cmdoptPLs, COUNT(cmdoptPLs) };

static void CmdoptOk(void* junk)
{
	long changes;
	changes = GetChanges(&cmdoptPG);
	wHide(cmdoptW);
	FormSaveDefaultValues(&cmdoptPG);
	DoChangeNotification(changes);
}

static void CmdoptChange(long changes)
{
	if (changes & CHANGE_CMDOPT)
		if (cmdoptW != NULL && wWinIsVisible(cmdoptW)) {
			FormLoadControls(&cmdoptPG);
		}
}

static void DoCmdopt(void* junk)
{
	if (cmdoptW == NULL) {
		cmdoptW = FormCreateDialog(&cmdoptPG, NULL,
		                           NULL, CmdoptOk,
		                           NULL, FormCancel_Restore, TRUE, 0L, NULL);
		FormLoadDefaultValues(&cmdoptPG); // TODO - remove?
	}
	FormLoadControls(&cmdoptPG);
	wShow(cmdoptW);
}

EXPORT addButtonCallBack_t CmdoptInit(void)
{
	FormRegister(&cmdoptPG);
	RegisterChangeNotification(CmdoptChange);
	return &DoCmdopt;
}


