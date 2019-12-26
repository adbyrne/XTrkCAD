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
#include "include/paramfilelist.h"

#include "bitmaps/magnifier.xpm"

static CatalogEntry *catalogFileBrowse;				/**< current search results */
static TrackLibrary *trackLibrary;					/**< parameter library index */

/* define the search / browse dialog */

static struct wFilSel_t * searchUi_fs;				/**< searchdialog for parameter files */

static void SearchUiBrowse(void *junk);
static void SearchUiApply(wWin_p junk);
static void SearchUiSelectAll(void *junk);
static void SearchUiDoSearch(void *junk);

static long searchUiMode = 0;
static paramListData_t searchUiListData = { 10, 370, 0 };
#define MAXQUERYLENGTH 250
static char searchUiQuery[MAXQUERYLENGTH];
static char * searchUiLabels[] = { N_("Show File Names"), NULL };

static paramData_t searchUiPLs[] = {
#define I_QUERYSTRING  (0)
	{ PD_STRING, searchUiQuery, "query", PDO_NOPREF | PDO_STRINGLIMITLENGTH, (void*)(340), N_(""), 0, 0, MAXQUERYLENGTH-1 },
#define I_SEARCHBUTTON (1)
	{ PD_BUTTON, (void*)SearchUiDoSearch, "find", PDO_DLGHORZ, 0, NULL,  BO_ICON, (void *)NULL },
#define I_MESSAGE (2)
	{ PD_MESSAGE, N_("Enter single search word"), NULL, PDO_DLGBOXEND, (void *)370 },
#define I_RESULTLIST	(3)
    {	PD_LIST, NULL, "inx", 0, &searchUiListData, NULL, BL_DUP|BL_SETSTAY|BL_MANY },
#define I_MODETOGGLE	(4)
    {	PD_TOGGLE, &searchUiMode, "mode", PDO_DLGBOXEND, searchUiLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_APPLYBUTTON	(5)
	{	PD_BUTTON, (void *)SearchUiApply, "apply", PDO_DLGCMDBUTTON, NULL, N_("Apply") },
#define I_SELECTALLBUTTON (6)
    {	PD_BUTTON, (void *)SearchUiSelectAll, "selectall", PDO_DLGCMDBUTTON, NULL, N_("Select all") },
    {	PD_BUTTON, (void*)SearchUiBrowse, "browse", 0, NULL, N_("Browse ...") }
};

#define SEARCHBUTTON ((wButton_p)searchUiPLs[I_SEARCHBUTTON].control)
#define RESULTLIST	 ((wList_p)searchUiPLs[I_RESULTLIST].control)
#define APPLYBUTTON  ((wButton_p)searchUiPLs[I_APPLYBUTTON].control)
#define SELECTALLBUTTON  ((wButton_p)searchUiPLs[I_SELECTALLBUTTON].control)
#define MESSAGETEXT ((wMessage_p)searchUiPLs[I_MESSAGE].control)
#define QUERYSTRING ((wString_p)searchUiPLs[I_QUERYSTRING].control)

static paramGroup_t searchUiPG = { "searchgui", 0, searchUiPLs, sizeof searchUiPLs/sizeof searchUiPLs[0] };
static wWin_p searchUiW;


/**
 * Reload the listbox showing the current catalog
 */

static
void SearchFileListLoad(CatalogEntry *catalog)
{
	CatalogEntry *currentEntry = catalog->next;
	DynString description;
    DynStringMalloc(&description, STR_SHORT_SIZE);
	
    wControlShow((wControl_p)RESULTLIST, FALSE);
    wListClear(RESULTLIST);
		
	while (currentEntry != currentEntry->next) {
		DynStringClear(&description);
		DynStringCatCStr(&description,
			((!searchUiMode) && currentEntry->contents) ?
			currentEntry->contents :
			currentEntry->fullFileName[currentEntry->files - 1]);

		wListAddValue(RESULTLIST,
			DynStringToCStr(&description),
			NULL,
	//		indicatorIcons[paramFileInfo.favorite][paramFileInfo.trackState],
			(void*)currentEntry);
			currentEntry = currentEntry->next;
	}

    wControlShow((wControl_p)RESULTLIST, TRUE);
	wControlActive((wControl_p)SELECTALLBUTTON,
					wListGetCount(RESULTLIST));

    DynStringFree(&description);
}

/**
 * Find parameter files using the file selector
 * 
 * \param junk
 */

static void SearchUiBrowse(void * junk)
{
	EmptyCatalog(catalogFileBrowse);

    wFilSelect(searchUi_fs, GetParamFileDir());
	
	SearchFileListLoad(catalogFileBrowse);
    return;
}

/**
 * Update the action buttons.
 *
 * If there is at least one selected file, Apply is enabled
 * If there are entries in the list, Select All is enabled
 *
 * \return
 */

static void UpdateSearchUiButton(void)
{
    wIndex_t selCnt = wListGetSelectedCount(RESULTLIST);
	wIndex_t cnt = wListGetCount(RESULTLIST);
    
	wControlActive((wControl_p)APPLYBUTTON, selCnt > 0);
	wControlActive((wControl_p)SELECTALLBUTTON, cnt > 0);
}

/**
 * Perform the search. If successful, the results are loaded into the list
 * 
 * \param ptr INignored
 */
 
static void SearchUiDoSearch(void * ptr)
{
	unsigned result;
	
	EmptyCatalog(catalogFileBrowse);

	result = SearchLibrary(trackLibrary, searchUiQuery, catalogFileBrowse);
	if(result) {
		DynString hitsMessage;
		DynStringMalloc(&hitsMessage, 16);
		DynStringPrintf(&hitsMessage, _("%d parameter files found."), result);
		wMessageSetValue(MESSAGETEXT, DynStringToCStr(&hitsMessage));
		DynStringFree(&hitsMessage);

		SearchFileListLoad(catalogFileBrowse);
	} else {
		wListClear(RESULTLIST);
		wControlActive((wControl_p)SELECTALLBUTTON, FALSE);
		wMessageSetValue(MESSAGETEXT, _("No matches found."));
	}
}

/**
 * Select all files in the list
 *
 * \param junk IN ignored
 * \return
 */

static void SearchUiSelectAll(void *junk)
{
    wListSelectAll(RESULTLIST);
}

/**
 * Action handler for Done button. Hides the dialog.
 * 
 * \param junk ignored
 */
static void SearchUiOk(void * junk)
{
    wHide(searchUiW);
}

/**
 * Handle the Apply button: a list of selected list elements is created and
 * passed to the parameter file list. 
 * 
 * \param junk IN/OUT ignored
 */

static void SearchUiApply(wWin_p junk)
{
	char **fileNames;
	int files = wListGetSelectedCount(RESULTLIST);
	int found = 0;
	CatalogEntry *currentEntry;
	
	fileNames = malloc(sizeof(char *)*files);
	if (!fileNames) {
		AbortProg("Couldn't allocate memory for result list: %s (%d)", __FILE__, __LINE__, NULL);
	}

	for (int inx = 0; found < files; inx++) {
		if (wListGetItemSelected(RESULTLIST, inx)) {
			currentEntry = (CatalogEntry *)wListGetItemContext(RESULTLIST, inx);
			fileNames[found++] = currentEntry->fullFileName[currentEntry->files - 1];
		}
	}

	LoadParamFile(files, fileNames, NULL);
	free(fileNames);
}

/**
 * Event handling for the Search dialog
 * 
 * \param pg IN ignored
 * \param inx IN ignored
 * \param valueP IN ignored
 */
 
static void SearchUiDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    switch (inx) {
    case I_RESULTLIST:
        UpdateSearchUiButton();
        break;
    case I_MODETOGGLE:
		SearchFileListLoad(catalogFileBrowse);
        break;
    }
}

/**
 * Create and open the search dialog.
 *
 * \param junk
 */

void DoSearchParams(void * junk)
{
    if (searchUiW == NULL) {
		catalogFileBrowse = InitCatalog();
		trackLibrary = CreateLibrary(GetParamFileDir());

		searchUiPLs[I_SEARCHBUTTON].winLabel = (char *)wIconCreatePixMap(magnifier_xpm);

        ParamRegister(&searchUiPG);

        searchUiW = ParamCreateDialog(&searchUiPG,
                                       MakeWindowTitle(_("Find parameter files")), _("Done"), SearchUiOk, NULL,
                                       TRUE, NULL, 0, SearchUiDlgUpdate);
		
		wControlActive((wControl_p)APPLYBUTTON, FALSE);
		wControlActive((wControl_p)SELECTALLBUTTON, FALSE);

        searchUi_fs = wFilSelCreate(mainW, FS_LOAD, FS_MULTIPLEFILES,
                                     _("Load Parameters"), _("Parameter files (*.xtp)|*.xtp"), GetParameterFileInfo, (void *)catalogFileBrowse);
    }
    ParamLoadControls(&searchUiPG);
    ParamGroupRecord(&searchUiPG);

	if (!trackLibrary) {
		wControlActive((wControl_p)SEARCHBUTTON, FALSE);
		wControlActive((wControl_p)QUERYSTRING, FALSE);
		wMessageSetValue(MESSAGETEXT, _("No parameter files found, search is disabled."));
	} else {
		wStringSetValue(QUERYSTRING, "");
	}
    wShow(searchUiW);
}


