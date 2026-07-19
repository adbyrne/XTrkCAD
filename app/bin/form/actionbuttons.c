/**
 * \file   actionbuttons.c
 * \brief
 *
 * \author Martin Fischer
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

#include "formprivate.h"


void FormButtonOk(paramGroup_p group)
{
	// wFlush();
	// LOG(log_form, 1, ("FormButtonOk: %s\n", group->nameStr));
	// if (!FormCheckInputs(group, (wControl_p)group->okB)) {
	// 	return;
	// }
	// if (group->nameStr) {
	// 	FormMacroRecord("PARAMETER %s %s\n", group->nameStr, "ok");
	// }

	// // if (group->okProc) {
	// // 	group->okProc(group);
	// // }

	// wFlush();

	LOG(log_form, 1, ("FormButtonOk -> Ok\n"));
}

void FormDialogOkActive(
        paramGroup_p pg,
        int active)
{
	if (pg->okB) {
		wControlActive((wControl_p)pg->okB, active);
	}
}

#define  FORMCANCEL_NEWUNDO
#ifdef FORMCANCEL_NEWUNDO
/* No Cancel button, Commnd can be undone
 */
EXPORT void FormCancel_Undo(
        paramGroup_p group)
{
}
#else
EXPORT void FormCancel_Undo(
        wWin_p winP)
{
	wHide(winP);
}
#endif

/* Cancel button, exits commands leaving control values as current
 */
EXPORT void FormCancel_Current(
        paramGroup_p group)
{
	wHide(group->win);
}

/* As above, but always exit command
 */
EXPORT void FormCancel_Reset(
        paramGroup_p group)
{
	ResetIfNotSticky();
	FormCancel_Current(group);
}

/* Cancel button, exits commands restoring control values
 */
EXPORT void FormCancel_Restore(
        paramGroup_p group)
{
	FormCancel_Current(group);
}

/* Cancel button, no Cancel button
 */
EXPORT void FormCancel_Null(
        paramGroup_cp group)
{
}

EXPORT void FormButtonCancel(void* groupVP)
{
	paramGroup_p group = groupVP;
	if (group->nameStr) {
		FormMacroRecord("PARAMETER %s %s\n", group->nameStr, "cancel");
	}
	if (group->cancelProc) {
		group->cancelProc(group->win);
	}
}
