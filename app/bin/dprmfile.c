/** \file dprmfile.c
 * Param File Management
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
#include "paramfile.h"
#include "paramfilelist.h"
#include "paths.h"
#include "track.h"

static struct wFilSel_t * paramFile_fs;

/****************************************************************************
 *
 * Param File Dialog
 *
 */

#include "bitmaps/greendot.xpm"
#include "bitmaps/greydot.xpm"
#include "bitmaps/yellowdot.xpm"
#include "bitmaps/reddot.xpm"

static wIcon_p indicatorIcons[PARAMFILE_MAXSTATE];

static wWin_p paramFileW;

static long paramFileSel = 0;

static void ParamFileAction(void *);
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
#define I_PRMFILACTION	(3)
#define paramFileActionB		((wButton_p)paramFilePLs[I_PRMFILACTION].control)
    {	PD_BUTTON, (void*)ParamFileAction, "action", PDO_DLGCMDBUTTON, NULL, N_("Unload"), 0L, FALSE },
    {	PD_BUTTON, (void*)ParamFileBrowse, "browse", 0, NULL, N_("Browse ...") }
};

static paramGroup_t paramFilePG = { "prmfile", 0, paramFilePLs, sizeof paramFilePLs/sizeof paramFilePLs[0] };

/**
 * Reload the listbox showing the current parameter files
 */
void ParamFileListLoad(int paramFileCnt,  dynArr_t *paramFiles)
{
    wIndex_t listInx;
    int fileInx;
    DynString description;
    DynStringMalloc(&description, STR_SHORT_SIZE);


	wControlShow((wControl_p)paramFileL, FALSE);
	listInx = wListGetIndex(paramFileL);
	wListClear(paramFileL);

	for (fileInx = 0; fileInx < paramFileCnt; fileInx++) {
		paramFileInfo_t paramFileInfo = DYNARR_N(paramFileInfo_t, (*paramFiles),
										fileInx);
		if (paramFileInfo.valid) {
			DynStringClear(&description);
			DynStringCatCStr(&description,
							 ((!paramFileSel) && paramFileInfo.contents) ?
							 paramFileInfo.contents :
							 paramFileInfo.name);

			wListAddValue(paramFileL,
						  DynStringToCStr(&description),
						  indicatorIcons[paramFileInfo.trackState],
						  (void*)(intptr_t)fileInx);

			SetCurrentPath( PARAMETERPATHKEY, paramFileInfo(fileInx).name);
		}
	}
	wListSetIndex(paramFileL, listInx);
	wControlShow((wControl_p)paramFileL, TRUE);
    DynStringFree(&description);
    wIndex_t inx;
    int i = 0;
    
    wBool_t redrawList = FALSE;
    
    assert( fileName != NULL );
    assert( files > 0);
    
    SetCurrentPath( PARAMETERPATHKEY, fileName[0] );
    for( i=0; i < files; i++ )
    {
        curContents = curSubContents = NULL;
        curParamFileIndex = paramFileInfo_da.cnt;
        if ( !ReadParams( 0, NULL, fileName[ i ] ) )
            return FALSE;
        
        assert( curContents != NULL );
        // in case the contents is already presented, make invalid
        for ( inx=0; inx<paramFileInfo_da.cnt; inx++ ) {
            if ( paramFileInfo(inx).valid &&
                strcmp( paramFileInfo(inx).contents, curContents ) == 0 ) {
                paramFileInfo(inx).valid = FALSE;
                redrawList = TRUE;
                break;
            }
        }
        
        DYNARR_APPEND( paramFileInfo_t, paramFileInfo_da, 10 );
        paramFileInfo(curParamFileIndex).name = MyStrdup( fileName[ i ] );
        paramFileInfo(curParamFileIndex).valid = TRUE;
        paramFileInfo(curParamFileIndex).deletedShadow =
        paramFileInfo(curParamFileIndex).deleted = FALSE;
        paramFileInfo(curParamFileIndex).contents = curContents;
        
        if ( paramFilePG.win ) {
            if ( redrawList ) {
                ParamFileLoadList();
            } else {
                DynStringClear(&description);
                DynStringCatCStr(&description,
                                 ((!paramFileSel) && paramFileInfo.contents) ?
                                 paramFileInfo.contents :
                                 paramFileInfo.name);
                
                wListAddValue(paramFileL,
                              DynStringToCStr(&description),
                              indicatorIcons[paramFileInfo.trackState],
                              (void*)(intptr_t)fileInx);

                wListSetIndex( paramFileL, wListGetCount(paramFileL)-1 );
            }
        }
        
        wPrefSetString( "Parameter File Map", curContents,
                       paramFileInfo(curParamFileIndex).name );
    }
    curParamFileIndex = PARAM_CUSTOM;
    DoChangeNotification( CHANGE_PARAMS );
    return TRUE;
}


static void ParamFileBrowse(void * junk)
{

    wFilSelect(paramFile_fs, GetParamFileDir());
    return;
}

/**
 * Update the action button. If at least one selected file is unloaded, the action button
 * is set to 'Reload'. If all selected files are loaded, the button will be set to 'Unload'.
 *
 * \param varname1 IN this is a variable
 * \return
 */

static void UpdateParamFileButton(
    wIndex_t fileInx)
{
    wIndex_t selcnt = wListGetSelectedCount(paramFileL);
    wIndex_t inx, cnt;

    // set the default
    wButtonSetLabel(paramFileActionB, _("Unload"));
    paramFilePLs[ I_PRMFILACTION ].context = FALSE;

    //nothing selected -> leave
    if (selcnt <= 0) {
        return;
    }

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
                // if selected file was unloaded, set button to reload and finish loop
                wButtonSetLabel(paramFileActionB, _("Reload"));
                paramFilePLs[ I_PRMFILACTION ].context = (void *)TRUE;
                break;
            }
        }
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
    wIndex_t inx, cnt;
    wIndex_t fileInx;
    unsigned newDeletedState;

    if (action) {
        newDeletedState = FALSE;
    } else {
        newDeletedState = TRUE;
    }

    //nothing selected -> leave
    if (selcnt <= 0) {
        return;
    }

    // get the number of items in list
    cnt = wListGetCount(paramFileL);

    // walk through the whole list box
    for (inx=0; inx<cnt; inx++) {
        if (wListGetItemSelected((wList_p)paramFileL, inx)) {
            fileInx = (intptr_t)wListGetItemContext(paramFileL, inx);

            // set the desired state
            SetParamFileDeleted(fileInx, newDeletedState);
        }
    }
    DoChangeNotification(CHANGE_PARAMS);
    UpdateParamFileButton(fileInx);
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
    UpdateParamFileButton(0);
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
        UpdateParamFileButton((wIndex_t)(long)wListGetItemContext(paramFileL,
                              wListGetIndex(paramFileL)));
        break;
    case I_PRMFILTOGGLE:
        DoChangeNotification(CHANGE_PARAMS);
        break;
    }
}


void ParamFilesChange(long changes)
{
    if (changes & CHANGE_PARAMS || changes & CHANGE_SCALE ) {
        UpdateParamFileList();
        if( paramFileW ) {
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
        indicatorIcons[ PARAMFILE_UNLOADED ] = wIconCreatePixMap(greydot);
        indicatorIcons[ PARAMFILE_NOTUSABLE ] = wIconCreatePixMap(reddot);
        indicatorIcons[ PARAMFILE_COMPATIBLE ] = wIconCreatePixMap(yellowdot);
        indicatorIcons[ PARAMFILE_FIT] = wIconCreatePixMap(greendot);

        ParamRegister(&paramFilePG);

        paramFileW = ParamCreateDialog(&paramFilePG,
                                       MakeWindowTitle(_("Parameter Files")), _("Ok"), ParamFileOk, ParamFileCancel,
                                       TRUE, NULL, 0, ParamFileDlgUpdate);
        paramFile_fs = wFilSelCreate(mainW, FS_LOAD, FS_MULTIPLEFILES,
                                     _("Load Parameters"), _("Parameter files|*.xtp"), LoadParamFile, NULL);
    }
    ParamLoadControls(&paramFilePG);
    ParamGroupRecord(&paramFilePG);
    if ((listInx = wListGetValues(paramFileL, NULL, 0, NULL, &data))>=0) {
        UpdateParamFileButton((wIndex_t)(long)data);
    }

    wShow(paramFileW);
}


