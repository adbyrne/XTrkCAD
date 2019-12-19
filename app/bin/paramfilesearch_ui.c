/** \file paramfilesearch_ui.c
 * Parameter File Search Dialog
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2019 Martin Fischer
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
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "include/partcatalog.h"
#include "paths.h"
#include "paramfilelist.h"

static struct wFilSel_t * searchUi_fs;

#include "bitmaps/greendot.xpm"
#include "bitmaps/greydot.xpm"
#include "bitmaps/yellowdot.xpm"
#include "bitmaps/reddot.xpm"
#include "bitmaps/greenstar.xpm"
#include "bitmaps/greystar.xpm"
#include "bitmaps/yellowstar.xpm"
#include "bitmaps/redstar.xpm"

#include "bitmaps/magnifier.xpm"

#define FAVORITE_PARAM 1
#define STANDARD_PARAM 0

#define PARAMBUTTON_HIDE "Hide"
#define PARAMBUTTON_UNHIDE "Unhide"

static wIcon_p indicatorIcons[ 2 ][PARAMFILE_MAXSTATE];

static wWin_p searchUiW;

static long searchUiMode = 0;

static void SearchUiBrowse(void *);
static void SearchUiSelectAll(void *);
static void SearchUiDoSearch(void *);

static paramListData_t searchUiListData = { 10, 370 };

#define MAXQUERYLENGTH 250
static char searchUiQuery[MAXQUERYLENGTH];

static char * searchUiLabels[] = { N_("Show File Names"), NULL };
static paramData_t searchUiPLs[] = {
	{ PD_STRING, searchUiQuery, "query", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)(250), N_(""), 0, 0, MAXQUERYLENGTH-1 },
#define I_SEARCHBUTTON (1)
	{ PD_BUTTON, (void*)SearchUiDoSearch, "find", PDO_DLGHORZ, 0, NULL,  BO_ICON, (void *)NULL },
#define I_MESSAGE (2)
	{ PD_MESSAGE, N_("No results"), NULL, PDO_DLGBOXEND, (void *)180 },
#define I_RESULTLIST	(3)
    {	PD_LIST, NULL, "inx", 0, &searchUiListData, NULL, BL_DUP|BL_SETSTAY|BL_MANY },
#define I_MODETOGGLE	(1)
    {	PD_TOGGLE, &searchUiMode, "mode", PDO_DLGBOXEND, searchUiLabels, NULL, BC_HORZ|BC_NOBORDER },
    {	PD_BUTTON, (void *)SearchUiSelectAll, "selectall", PDO_DLGCMDBUTTON, NULL, N_("Select all") },
    {	PD_BUTTON, (void*)SearchUiBrowse, "browse", 0, NULL, N_("Browse ...") }
};

#define SEARCHBUTTON ((wButton_p)searchUiPLs[I_SEARCHBUTTON].control)
#define RESULTLIST				((wList_p)searchUiPLs[I_RESULTLIST].control)

static paramGroup_t searchUiPG = { "searcgui", 0, searchUiPLs, sizeof searchUiPLs/sizeof searchUiPLs[0] };

/**
 * Create a sorted list of indexes into the parameter file array. That way, the elements
 * in the array will not be moved. Instead the list is used for the order in which the
 * list box is populated.
 *
 * \param cnt IN number of parameter files
 * \param files IN parameter file array
 * \param list OUT the ordered list
 */

//void
//SortResultList(size_t cnt,  dynArr_t *files, int *list)
//{
//    for (size_t i = 0; i < cnt; i++) {
//        list[i] = i;
//    }
//
//    sortFiles = files;
//
//    qsort((void *)list, (size_t)cnt, sizeof(int), CompareParameterFiles);
//}


/**
 * Reload the listbox showing the current parameter files
 */
//void ParamFileListLoad(int paramFileCnt,  dynArr_t *paramFiles)
//{
//    DynString description;
//    DynStringMalloc(&description, STR_SHORT_SIZE);
//    int *sortedIndex = MyMalloc(sizeof(int)*paramFileCnt);
//
//    SortParamFileList(paramFileCnt, paramFiles, sortedIndex);
//
//    wControlShow((wControl_p)paramFileL, FALSE);
//    wListClear(paramFileL);
//
//    for (int i = 0; i < paramFileCnt; i++) {
//        paramFileInfo_t paramFileInfo = DYNARR_N(paramFileInfo_t, (*paramFiles),
//                                        sortedIndex[ i ]);
//        if (paramFileInfo.valid) {
//            DynStringClear(&description);
//            DynStringCatCStr(&description,
//                             ((!paramFileSel) && paramFileInfo.contents) ?
//                             paramFileInfo.contents :
//                             paramFileInfo.name);
//
//            wListAddValue(paramFileL,
//                          DynStringToCStr(&description),
//                          indicatorIcons[ paramFileInfo.favorite ][paramFileInfo.trackState],
//                          (void*)(intptr_t)sortedIndex[i]);
//        }
//    }
//    wControlShow((wControl_p)paramFileL, TRUE);
//    DynStringFree(&description);
//    MyFree(sortedIndex);
//}


static void SearchUiBrowse(void * junk)
{

    wFilSelect(searchUi_fs, GetParamFileDir());
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
//
//static void UpdateParamFileButton(void)
//{
//    wIndex_t selcnt = wListGetSelectedCount(paramFileL);
//    wIndex_t inx, cnt;
//    wIndex_t fileInx;
//
//    //nothing selected -> leave
//    if (selcnt <= 0) {
//        return;
//    }
//
//    // set the default
//    wButtonSetLabel(paramFileActionB, _(PARAMBUTTON_HIDE));
//    searchUiPLs[ I_PRMFILACTION ].context = FALSE;
//    searchUiPLs[I_PRMFILEFAVORITE].context = FALSE;
//
//    // get the number of items in list
//    cnt = wListGetCount(paramFileL);
//
//    // walk through the whole list box
//    for (inx=0; inx<cnt; inx++) {
//        if (wListGetItemSelected((wList_p)paramFileL, inx)) {
//            // if item is selected, get status
//            fileInx = (intptr_t)wListGetItemContext(paramFileL, inx);
//
//            if (fileInx < 0 || fileInx >= GetParamFileCount()) {
//                return;
//            }
//            if (IsParamFileDeleted(fileInx)) {
//                // if selected file was unloaded, set button to reload
//                wButtonSetLabel(paramFileActionB, _(PARAMBUTTON_UNHIDE));
//                searchUiPLs[ I_PRMFILACTION ].context = (void *)TRUE;
//            }
//            if (!IsParamFileFavorite(fileInx)) {
//                searchUiPLs[I_PRMFILEFAVORITE].context = (void *)TRUE;
//            }
//        }
//    }
//}

/**
 * Set the property for a parameter file in memory
 *
 * \param paramSetting IN property to be changed
 * \param newState IN new value for property
 */

//void
//UpdateParamFileProperties(enum PARAMFILESETTING paramSetting, bool newState)
//{
//    wIndex_t inx, cnt;
//    wIndex_t fileInx;
//
//    // get the number of items in list
//    cnt = wListGetCount(paramFileL);
//
//    // walk through the whole list box
//    for (inx = 0; inx < cnt; inx++) {
//        if (wListGetItemSelected((wList_p)paramFileL, inx)) {
//            fileInx = (intptr_t)wListGetItemContext(paramFileL, inx);
//
//            // set the desired state
//            if (paramSetting == SET_FAVORITE) {
//                SetParamFileFavorite(fileInx, newState);
//            } else {
//                SetParamFileDeleted(fileInx, newState);
//            }
//        }
//    }
//    DoChangeNotification(CHANGE_PARAMS);
//}

static void SearchUiDoSearch(void * ptr)
{

}

/**
 * Select all files in the list and set action button
 *
 * \param junk IN ignored
 * \return
 */

static void SearchUiSelectAll(void *junk)
{
    wListSelectAll(RESULTLIST);
   // UpdateParamFileButton();
}

static void SearchUiOk(void * junk)
{
    ParamFileListConfirmChange();
    wHide(searchUiW);
}


static void SearchUiCancel(wWin_p junk)
{
    ParamFileListCancelChange();
    wHide(searchUiW);
    DoChangeNotification(CHANGE_PARAMS);
}

static void SearchUiDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    //switch (inx) {
    //case I_PRMFILLIST:
    //    UpdateParamFileButton();
    //    break;
    //case I_PRMFILTOGGLE:
    //    DoChangeNotification(CHANGE_PARAMS);
    //    break;
    //}
}


//void ParamFilesChange(long changes)
//{
//    if (changes & CHANGE_PARAMS || changes & CHANGE_SCALE) {
//        UpdateParamFileList();
//        if (searchUiW) {
//            ParamFileListLoad(paramFileInfo_da.cnt, &paramFileInfo_da);
//        }
//    }
//}

/**
 * Create and open the search dialog.
 *
 * \param junk
 */

void DoSearchParams(void * junk)
{
    wIndex_t listInx;
    void * data;

    if (searchUiW == NULL) {
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

		searchUiPLs[I_SEARCHBUTTON].winLabel = (char *)wIconCreatePixMap(magnifier_xpm);

        ParamRegister(&searchUiPG);

        searchUiW = ParamCreateDialog(&searchUiPG,
                                       MakeWindowTitle(_("Search part catalog")), _("Ok"), SearchUiOk, SearchUiCancel,
                                       TRUE, NULL, 0, SearchUiDlgUpdate);



        searchUi_fs = wFilSelCreate(mainW, FS_LOAD, FS_MULTIPLEFILES,
                                     _("Load Parameters"), _("Parameter files (*.xtp)|*.xtp"), LoadParamFile, NULL);
    }
    ParamLoadControls(&searchUiPG);
    ParamGroupRecord(&searchUiPG);
    //if ((listInx = wListGetValues(paramFileL, NULL, 0, NULL, &data))>=0) {
    //    UpdateParamFileButton();
    //}

    wShow(searchUiW);
}


