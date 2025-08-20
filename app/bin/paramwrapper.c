/**
 * \file   paramwrapper.c
 * \brief  Wrapper around old Param*() functions to facilitate simple switch to new variants
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
#include <form.h>

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

#define USESBUILDER(pg) ((pg->options)&PGO_FULLDIALOGFROMBUILDER)

EXPORT void ParamLoadControls(
    paramGroup_p pg)
{
    if (USESBUILDER(pg)) {
        FormLoadControls(pg);
    }
    else {
        printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
        //ParamLoadControlsOrig(pg);
    }
}

EXPORT void ParamLoadControl(
    paramGroup_p pg,
    int inx)
{
    if (USESBUILDER(pg)) {
        FormLoadSingleControl(pg, inx);
    }
    else {
        printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
        //ParamLoadControlOrig(pg, inx);
    }
}

EXPORT void ParamControlActive(
    paramGroup_p pg,
    int inx,
    BOOL_T active)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
    //ParamControlActiveOrig( pg, inx, active);
}

EXPORT void ParamLoadMessage(
    paramGroup_p pg,
    int inx,
    char* message)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
    //ParamLoadMessageOrig(pg, inx, message);
}

EXPORT long ParamUpdate(
    paramGroup_p pg)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
    //return(ParamUpdateOrig(pg));
    return(0L);
}

void ParamLoadData(
    paramGroup_p pg)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
    //ParamLoadDataOrig(pg);
}

EXPORT void ParamRegister(paramGroup_p pg)
{
    if (USESBUILDER(pg)) {
        FormRegister(pg);
        FormLoadDefaultValues(pg);
    }
    else {
        printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
        //ParamRegisterOrig(pg);
    }
}

EXPORT void ParamUpdatePrefs(void) 
{
    printf("%s:%d Old Param function %s used\n", __FILE__, __LINE__, __func__);
    //ParamUpdatePrefsOrig();
}

EXPORT void ParamGroupRecord(paramGroup_p pg)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
    //ParamGroupRecordOrig(pg);
}

EXPORT void ParamStartRecord(FILE* macroFile)
{
    FormStartRecord(macroFile);
    printf("%s:%d Old Param Function %s used\n", __FILE__, __LINE__, __func__);
    //ParamStartRecordOrig(macroFile);
}



EXPORT void ParamRestoreAll(void)
{
    printf("%s:%d Old Param Function %s used\n", __FILE__, __LINE__, __func__);
    //ParamRestoreAllOrig();
}

EXPORT void ParamSaveAll(void)
{
    printf("%s:%d Old Param Function %s used\n", __FILE__, __LINE__, __func__);
    //ParamSaveAllOrig();
}

EXPORT void ParamMenuPush(void* dp)
{
    printf("%s:%d Old Param function %s used\n", __FILE__, __LINE__, __func__);
    //ParamMenuPushOrig(dp);
    FormMenuPush(dp);
}

EXPORT wBool_t ParamCheckInputs(
    paramGroup_p group,
    wControl_p b)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, group->nameStr);
    //return( ParamCheckInputsOrig(group, b));
    return(FALSE);
}


EXPORT void ParamResetInvalid(wWin_p win)
{

    //FormResetInvalid(win);
    //ParamResetInvalidOrig(win);
}

EXPORT void ParamControlShow(paramGroup_p pg, wIndex_t inx, wBool_t bShow)
{
    printf("%s:%d Old Param Function %s used by %s\n", __FILE__, __LINE__, __func__, pg->nameStr);
    //ParamControlShowOrig(pg, inx, bShow);
}

void
ParamSetInPlayback(bool state, long delay)
{
    printf("%s:%d Old Param Function used\n", __FILE__, __LINE__);
    //ParamSetInPlaybackOrig(state, delay);
}

EXPORT void
ParamSetInReadTracks(bool state)
{
    printf("%s:%d Old Param Function used\n", __FILE__, __LINE__);

    //ParamSetInReadTracksOrig( state);
}

EXPORT void
ParamTurnOffDelays(bool disable)
{
    printf("%s:%d Old Param Function used\n", __FILE__, __LINE__);

    //ParamTurnOffDelaysOrig(disable);
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
    if (group->options & PGO_FULLDIALOGFROMBUILDER) {
        char* cancelLabel = (winOption & PD_F_ALT_CANCELLABEL ? _("Close") : _("Cancel"));
        dialog = FormCreateDialog(group, title, 
            okLabel, okProc, 
            cancelLabel, cancelProc, 
            needHelpButton,
            winOption,
            changeProc);
    }
    else {
        printf("%s:%d Old Param Function used by %s\n", __FILE__, __LINE__, group->nameStr);

        //dialog = (wControl_p) ParamCreateDialogOrig(group, title, okLabel, okProc, cancelProc, needHelpButton,
        //    layoutProc,
        //    winOption,
        //    changeProc);
        dialog = NULL;
    }

    return(dialog);
}

EXPORT void ParamLayoutDialog(
    paramGroup_p pg)
{
    printf("%s:%d Old Param Function used for %s\n", __FILE__, __LINE__, pg->nameStr);

    //ParamLayoutDialogOrig(pg);
}

EXPORT void ParamDialogOkActive(
    paramGroup_p pg,
    int active)
{
    printf("%s:%d Old Param Function used for %s\n", __FILE__, __LINE__, pg->nameStr);
    //ParamDialogOkActiveOrig(pg, active);
}

EXPORT void ParamCreateControls(
    paramGroup_p pg,
    paramChangeProc changeProc)
{
    printf("%s:%d Old Param Function used for %s\n", __FILE__, __LINE__, pg->nameStr);
    //ParamCreateControlsOrig(pg,  changeProc);
}

EXPORT void ParamHilite(
    wWin_p win,
    wControl_p control,
    BOOL_T hilite)
{
    printf("%s:%d Old Param Function used\n", __FILE__, __LINE__);
}

EXPORT void ParamInit(void)
{
    printf("%s:%d Old Param Function %s used\n", __FILE__, __LINE__, __func__);
    //ParamInitOrig();
}

EXPORT void ParamCancel_Current(
    paramGroup_p group)
{
    FormCancel_Current(group);
}

EXPORT void ParamCancel_Reset(
    paramGroup_p group)
{
    FormCancel_Reset(group);
}

EXPORT void ParamCancel_Restore(
    paramGroup_p group)
{
    FormCancel_Restore(group);
}

EXPORT void ParamCancel_Undo(
    wWin_p winP)
{
    printf("%s:%d Old Param Function used\n", __FILE__, __LINE__);
}
EXPORT void ParamCancel_Null(void* dummy)
    {
        printf("%s:%d Old Param Function used\n", __FILE__, __LINE__);
    }
