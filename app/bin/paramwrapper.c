/**
 * \file   paramwrapper.c
 * \brief  Wrapper around old Param*() functions to facilitate simple switch to new variants
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
#include <dialogs.h>

void ParamLoadControlsOrig(paramGroup_p);
void ParamLoadControlOrig(paramGroup_p, int);
void ParamControlActiveOrig(paramGroup_p, int, BOOL_T);
void ParamLoadMessageOrig(paramGroup_p, int, char*);
void ParamLoadDataOrig(paramGroup_p);
long ParamUpdateOrig(paramGroup_p);
void ParamRegisterOrig(paramGroup_p);
void ParamGroupRecordOrig(paramGroup_p);
void ParamUpdatePrefsOrig(void);
void ParamStartRecordOrig(FILE* recordF);
void ParamRestoreAllOrig(void);
void ParamSaveAllOrig(void);
void ParamSetInReadTracksOrig(bool state);
void ParamSetInPlaybackOrig(bool state, long delay);
void ParamTurnOffDelaysOrig(bool disable);
void ParamMenuPushOrig(void*);
void ParamHiliteOrig(wWin_p, wControl_p, BOOL_T);
wBool_t ParamCheckInputsOrig(paramGroup_p pg, wControl_p b);
void ParamInitOrig(void);
void ParamResetInvalidOrig(wWin_p win);
void ParamControlShowOrig(paramGroup_p pg, wIndex_t inx, wBool_t bShow);
wWin_p ParamCreateDialogOrig(
    paramGroup_p group,
    char* title,
    char* okLabel,
    paramActionOkProc okProc,
    paramActionCancelProc cancelProc,
    BOOL_T needHelpButton,
    paramLayoutProc layoutProc,
    long winOption,
    paramChangeProc changeProc);
void ParamLayoutDialogOrig(paramGroup_p pg);
void ParamDialogOkActiveOrig(paramGroup_p pg, int active);
void ParamCreateControlsOrig(paramGroup_p pg, paramChangeProc changeProc);

#define USESBUILDER(pg) ((pg->options)&BO_DIALOGFROMBUILDER)

EXPORT void ParamLoadControls(
    paramGroup_p pg)
{
    if (USESBUILDER(pg)) {
        //DialogLoadControls(pg);
    }
    else {
        ParamLoadControlsOrig(pg);
    }
}

EXPORT void ParamLoadControl(
    paramGroup_p pg,
    int inx)
{
    if (USESBUILDER(pg)) {
        //DialogLoadControl(pg, inx);
    }
    else {
        ParamLoadControlOrig(pg, inx);
    }
}

EXPORT void ParamControlActive(
    paramGroup_p pg,
    int inx,
    BOOL_T active)
{

    ParamControlActiveOrig( pg, inx, active);
}

EXPORT void ParamLoadMessage(
    paramGroup_p pg,
    int inx,
    char* message)
{
    ParamLoadMessageOrig(pg, inx, message);
}

EXPORT long ParamUpdate(
    paramGroup_p pg)
{
    return(ParamUpdateOrig(pg));
}

void ParamLoadData(
    paramGroup_p pg)
{
    ParamLoadDataOrig(pg);
}

EXPORT void ParamRegister(paramGroup_p pg)
{
    if (USESBUILDER(pg)) {
        DialogsRegister(pg);
        DialogsSetDefaultValues(pg);
    }
    else {
        ParamRegisterOrig(pg);
    }
}

EXPORT void ParamUpdatePrefs(void) 
{
    void ParamUpdatePrefsOrig(void);
}

EXPORT void ParamGroupRecord(paramGroup_p pg)
{
    void ParamGroupRecordOrig(pg);
}

EXPORT void ParamStartRecord(FILE* macroFile)
{
    void ParamStartRecordOrig(macroFile);
}

EXPORT void ParamRestoreAll(void)
{
    ParamRestoreAllOrig();
}

EXPORT void ParamSaveAll(void)
{
    ParamSaveAllOrig();
}

EXPORT void ParamMenuPush(void* dp)
{
    ParamMenuPushOrig(dp);
}

EXPORT wBool_t ParamCheckInputs(
    paramGroup_p group,
    wControl_p b)
{
    return( ParamCheckInputsOrig(group, b));
}

EXPORT void ParamHilite(
    wWin_p win,
    wControl_p control,
    BOOL_T hilite)
{
    ParamHiliteOrig(win, control, hilite);
}


EXPORT void ParamResetInvalid(
    wWin_p win)
{

    ParamResetInvalidOrig(win);
}

EXPORT void ParamControlShow(paramGroup_p pg, wIndex_t inx, wBool_t bShow)
{
    ParamControlShowOrig(pg, inx, bShow);
}

void
ParamSetInPlayback(bool state, long delay)
{
    ParamSetInPlaybackOrig(state, delay);
}

EXPORT void
ParamSetInReadTracks(bool state)
{
    ParamSetInReadTracksOrig( state);
}

EXPORT void
ParamTurnOffDelays(bool disable)
{
    ParamTurnOffDelaysOrig(disable);
}


wControl_p ParamCreateDialog(
    paramGroup_p group,
    char* title,
    char* okLabel,
    paramActionOkProc okProc,
    paramActionCancelProc cancelProc,
    BOOL_T needHelpButton,
    paramLayoutProc layoutProc,
    long winOption,
    paramChangeProc changeProc)
{
    wControl_p dialog;
    if (group->options & BO_DIALOGFROMBUILDER) {
        char* cancelLabel = (winOption & PD_F_ALT_CANCELLABEL ? _("Close") : _("Cancel"));
        dialog = DialogsCreateDialog(group, title, 
            okLabel, okProc, 
            cancelLabel, cancelProc, 
            needHelpButton,
            winOption,
            changeProc);
    }
    else {
        dialog = (wControl_p) ParamCreateDialogOrig(group, title, okLabel, okProc, cancelProc, needHelpButton,
            layoutProc,
            winOption,
            changeProc);
    }

    return(dialog);
}

EXPORT void ParamLayoutDialog(
    paramGroup_p pg)
{
    ParamLayoutDialogOrig(pg);
}

EXPORT void ParamDialogOkActive(
    paramGroup_p pg,
    int active)
{
    ParamDialogOkActiveOrig(pg, active);
}

EXPORT void ParamCreateControls(
    paramGroup_p pg,
    paramChangeProc changeProc)
{
    ParamCreateControlsOrig(pg,  changeProc);
}


EXPORT void ParamInit(void)
{
    ParamInitOrig();
}
