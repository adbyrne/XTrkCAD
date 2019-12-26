/** \file dprmfile.c
 * Param File Dialog
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "custom.h"
#include "dynstring.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "include/paramfile.h"
#include "include/paramfilelist.h"
#include "paths.h"
#include "track.h"

static struct wFilSel_t * paramFile_fs;


#include "bitmaps/greendot.xpm"
#include "bitmaps/greydot.xpm"
#include "bitmaps/yellowdot.xpm"
#include "bitmaps/reddot.xpm"
#include "bitmaps/greenstar.xpm"
#include "bitmaps/greystar.xpm"
#include "bitmaps/yellowstar.xpm"
#include "bitmaps/redstar.xpm"

#define FAVORITE_PARAM 1
#define STANDARD_PARAM 0

#define PARAMBUTTON_HIDE "Hide"
#define PARAMBUTTON_UNHIDE "Unhide"

static wIcon_p indicatorIcons[ 2 ][PARAMFILE_MAXSTATE];

static wWin_p paramFileW;

static long paramFileSel = 0;

static void ParamFileFavorite(void * favorite);
static void ParamFileAction(void * action);
static void ParamFileBrowse(void *);
static void ParamFileSelectAll(void *);

static paramListData_t paramFileListData = { 10, 370 };
static char * paramFileLabels[] = { N_("Show File Names"), NULL };
static paramData_t paramFilePLs[] = {
#define I_PRMFILLIST	(0)
#define paramFileL				((wList_p)paramFilePLs[I_PRMFILLIST].control)
    {	PD_LIST, NULL, "inx", 0, &paramFileListData, NULL, BL_DUP|BL_SETSTAY|BL_MANY },
#define I_PRMFILTOGGLE	(1)
    {	PD_TOGGLE, &paramFileSel, "mode", 0, paramFileLabels, NULL, BC_HORZ|BC_NOBORDER },
    {	PD_BUTTON, (void *)ParamFileSelectAll, "selectall", PDO_DLGCMDBUTTON, NULL, N_("Select all") },
#define I_PRMFILEFAVORITE (3)
    {   PD_BUTTON, (void *)ParamFileFavorite, "favorite", PDO_DLGCMDBUTTON, (void *)TRUE, N_("Favorite")},
#define I_PRMFILACTION	(4)
#define paramFileActionB		((wButton_p)paramFilePLs[I_PRMFILACTION].control)
    {	PD_BUTTON, (void*)ParamFileAction, "action", PDO_DLGCMDBUTTON, NULL, N_(PARAMBUTTON_HIDE), 0L, FALSE },
    {	PD_BUTTON, (void*)DoSearchParams, "find", 0, NULL, N_("Find ...") }
};

static paramGroup_t paramFilePG = { "prmfile", 0, paramFilePLs, sizeof paramFilePLs/sizeof paramFilePLs[0] };

enum PARAMFILESETTING {
    SET_FAVORITE,
    SET_DELETED
};

static dynArr_t *sortFiles;

/** Comparison function per C runtime conventions. Elements are ordered by compatibility
 *  state first and name of contents second.
 *
 * \param index1 IN first element
 * \param index2 IN second element
 * \return
 */

int
CompareParameterFiles(const void *index1, const void *index2)
{
    paramFileInfo_t paramFile1 = DYNARR_N(paramFileInfo_t, (*sortFiles), *(int*)index1);
    paramFileInfo_t paramFile2 = DYNARR_N(paramFileInfo_t, (*sortFiles), *(int*)index2);

    if (paramFile2.trackState != paramFile1.trackState) {
        return (paramFile2.trackState - paramFile1.trackState);
    } else {
        return (strcmp(paramFile1.contents, paramFile2.contents));
    }
}

/**
 * Create a sorted list of indexes into the parameter file array. That way, the elements
 * in the array will not be moved. Instead the list is used for the order in which the
 * list box is populated.
 *
 * \param cnt IN number of parameter files
 * \param files IN parameter file array
 * \param list OUT the ordered list
 */

void
SortParamFileList(size_t cnt,  dynArr_t *files, int *list)
{
    for (size_t i = 0; i < cnt; i++) {
        list[i] = i;
    }

    sortFiles = files;

    qsort((void *)list, (size_t)cnt, sizeof(int), CompareParameterFiles);
}


/**
 * Reload the listbox showing the current parameter files
 */
void ParamFileListLoad(int paramFileCnt,  dynArr_t *paramFiles)
{
    DynString description;
    DynStringMalloc(&description, STR_SHORT_SIZE);
    int *sortedIndex = MyMalloc(sizeof(int)*paramFileCnt);

    SortParamFileList(paramFileCnt, paramFiles, sortedIndex);

    wControlShow((wControl_p)paramFileL, FALSE);
    wListClear(paramFileL);

    for (int i = 0; i < paramFileCnt; i++) {
        paramFileInfo_t paramFileInfo = DYNARR_N(paramFileInfo_t, (*paramFiles),
                                        sortedIndex[ i ]);
        if (paramFileInfo.valid) {
            DynStringClear(&description);
            DynStringCatCStr(&description,
                             ((!paramFileSel) && paramFileInfo.contents) ?
                             paramFileInfo.contents :
                             paramFileInfo.name);

            wListAddValue(paramFileL,
                          DynStringToCStr(&description),
                          indicatorIcons[ paramFileInfo.favorite ][paramFileInfo.trackState],
                          (void*)(intptr_t)sortedIndex[i]);
        }
    }
    wControlShow((wControl_p)paramFileL, TRUE);
    DynStringFree(&description);
    MyFree(sortedIndex);
}


static void ParamFileBrowse(void * junk)
{

    wFilSelect(paramFile_fs, GetParamFileDir());
    return;
}

/**
 * Update the action buttons.
 *
 * If at least one selected file is not a favorite, the favorite button is set to 'SetFavorite'
 * If at least one selected file is unloaded, the action button
 * is set to 'Reload'. If all selected files are loaded, the button will be set to 'Unload'.
 *
 * \param varname1 IN this is a variable
 * \return
 */

static void UpdateParamFileButton(void)
{
    wIndex_t selcnt = wListGetSelectedCount(paramFileL);
    wIndex_t inx, cnt;
    wIndex_t fileInx;

    //nothing selected -> leave
    if (selcnt <= 0) {
        return;
    }

    // set the default
    wButtonSetLabel(paramFileActionB, _(PARAMBUTTON_HIDE));
    paramFilePLs[ I_PRMFILACTION ].context = FALSE;
    paramFilePLs[I_PRMFILEFAVORITE].context = FALSE;

    // get the number of items in list
    cnt = wListGetCount(paramFileL);

    // walk through the whole list box
    for (inx=0; inx<cnt; inx++) {
        if (wListGetItemSelected((wList_p)paramFileL, inx)) {
            // if item is selected, get status
            fileInx = (intptr_t)wListGetItemContext(paramFileL, inx);

            if (fileInx < 0 || fileInx >= GetParamFileCount()) {
                return;
            }
            if (IsParamFileDeleted(fileInx)) {
                // if selected file was unloaded, set button to reload 
                wButtonSetLabel(paramFileActionB, _(PARAMBUTTON_UNHIDE));
                paramFilePLs[ I_PRMFILACTION ].context = (void *)TRUE;
            }
            if (!IsParamFileFavorite(fileInx)) {
                paramFilePLs[I_PRMFILEFAVORITE].context = (void *)TRUE;
            }
        }
    }
}

/**
 * Set the property for a parameter file in memory
 *
 * \param paramSetting IN property to be changed
 * \param newState IN new value for property
 */

void
UpdateParamFileProperties(enum PARAMFILESETTING paramSetting, bool newState)
{
    wIndex_t inx, cnt;
    wIndex_t fileInx;

    // get the number of items in list
    cnt = wListGetCount(paramFileL);

    // walk through the whole list box
    for (inx = 0; inx < cnt; inx++) {
        if (wListGetItemSelected((wList_p)paramFileL, inx)) {
            fileInx = (intptr_t)wListGetItemContext(paramFileL, inx);

            // set the desired state
            if (paramSetting == SET_FAVORITE) {
                SetParamFileFavorite(fileInx, newState);
            } else {
                SetParamFileDeleted(fileInx, newState);
            }
        }
    }
    DoChangeNotification(CHANGE_PARAMS);
}


/**
 * Mark selected files as favorite
 *
 * \param favorite IN FALSE = remove, TRUE = set favorite
 * \return
 */

static void ParamFileFavorite(void * setFavorite)
{
    wIndex_t selcnt = wListGetSelectedCount(paramFileL);

    if (selcnt) {
        bool newFavoriteState;

        if (setFavorite) {
            newFavoriteState = TRUE;
        } else {
            newFavoriteState = FALSE;
        }

        UpdateParamFileProperties(SET_FAVORITE, newFavoriteState);
    }
}

/**
 * Unload selected files.
 *
 * \param action IN FALSE = unload, TRUE = reload parameter files
 * \return
 */

static void ParamFileAction(void * action)
{
    wIndex_t selcnt = wListGetSelectedCount(paramFileL);

    //nothing selected -> leave
    if (selcnt) {
        unsigned newDeletedState;

        if (action) {
            newDeletedState = FALSE;
        } else {
            newDeletedState = TRUE;
        }

        UpdateParamFileProperties(SET_DELETED, newDeletedState);
    }
}

/**
 * Select all files in the list and set action button
 *
 * \param junk IN ignored
 * \return
 */

static void ParamFileSelectAll(void *junk)
{
    wListSelectAll(paramFileL);
    UpdateParamFileButton();
}

static void ParamFileOk(void * junk)
{
    ParamFileListConfirmChange();
    wHide(paramFileW);
}


static void ParamFileCancel(wWin_p junk)
{
    ParamFileListCancelChange();
    wHide(paramFileW);
    DoChangeNotification(CHANGE_PARAMS);
}

static void ParamFileDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    switch (inx) {
    case I_PRMFILLIST:
        UpdateParamFileButton();
        break;
    case I_PRMFILTOGGLE:
        DoChangeNotification(CHANGE_PARAMS);
        break;
    }
}


void ParamFilesChange(long changes)
{
    if (changes & CHANGE_PARAMS || changes & CHANGE_SCALE) {
        UpdateParamFileList();
        if (paramFileW) {
            ParamFileListLoad(paramFileInfo_da.cnt, &paramFileInfo_da);
        }
    }
}

/**
 * Create and open the parameter file dialog.
 *
 * \param junk
 */

void DoParamFiles(void * junk)
{
    wIndex_t listInx;
    void * data;

    if (paramFileW == NULL) {
        indicatorIcons[ STANDARD_PARAM ][ PARAMFILE_UNLOADED ] = wIconCreatePixMap(
                    greydot);
        indicatorIcons[ STANDARD_PARAM ][ PARAMFILE_NOTUSABLE ] = wIconCreatePixMap(
                    reddot);
        indicatorIcons[ STANDARD_PARAM ][ PARAMFILE_COMPATIBLE ] = wIconCreatePixMap(
                    yellowdot);
        indicatorIcons[ STANDARD_PARAM ][ PARAMFILE_FIT] = wIconCreatePixMap(greendot);
        indicatorIcons[ FAVORITE_PARAM ][ PARAMFILE_UNLOADED ] = wIconCreatePixMap(
                    greystar);
        indicatorIcons[ FAVORITE_PARAM ][ PARAMFILE_NOTUSABLE ] = wIconCreatePixMap(
                    redstar);
        indicatorIcons[ FAVORITE_PARAM ][ PARAMFILE_COMPATIBLE ] = wIconCreatePixMap(
                    yellowstar);
        indicatorIcons[ FAVORITE_PARAM ][ PARAMFILE_FIT ] = wIconCreatePixMap(
                    greenstar);

        ParamRegister(&paramFilePG);

        paramFileW = ParamCreateDialog(&paramFilePG,
                                       MakeWindowTitle(_("Parameter Files")), _("Ok"), ParamFileOk, ParamFileCancel,
                                       TRUE, NULL, 0, ParamFileDlgUpdate);
        paramFile_fs = wFilSelCreate(mainW, FS_LOAD, FS_MULTIPLEFILES,
                                     _("Load Parameters"), _("Parameter files (*.xtp)|*.xtp"), LoadParamFile, NULL);
    }
    ParamLoadControls(&paramFilePG);
    ParamGroupRecord(&paramFilePG);
    if ((listInx = wListGetValues(paramFileL, NULL, 0, NULL, &data))>=0) {
        UpdateParamFileButton();
    }

    wShow(paramFileW);
}


