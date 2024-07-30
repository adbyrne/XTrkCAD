/** \file paramfilesearch_ui.c
 * Parameter File Search Dialog
 */

/*  XTrackCAD - Model Railroad CAD
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "custom.h"
#include "dynstring.h"
#include "param.h"
#include "include/partcatalog.h"
#include "paths.h"
#include "include/paramfilelist.h"
#include "include/paramfile.h"
#include "fileio.h"
#include "directory.h"
#include "layout.h"

static ParameterLib *trackLibrary;			/**< Track Library          */
static Catalog currentCat;					/**< catalog being shown    */

/* define the search / browse dialog */

static void SearchUiDefault(void);
static void SearchUiApply(wWin_p junk);
static void SearchUiSelectAll(void *junk);
static void SearchUiDoSearch(void *junk);
static void SearchUiClearFilter(void *ptr);

static long searchUiMode = 0;
static long searchFitMode = 0;
static paramListData_t searchUiListData = { 10, 370, 0 };
#define MAXQUERYLENGTH 250
static char searchUiQuery[MAXQUERYLENGTH];
static char * searchUiLabels[] = { N_("Show File Names"), NULL };
// Note these are defined in the same order as FIT_ANY, FIT_COMPATIBLE, FIT_EXACT
static char * searchFitLabels[] = { N_("Fit Any"), N_("Fit Compatible"), N_("Fit Exact"), NULL};


#define QUERYPROMPTSTRING "Enter at least one search word"

static paramData_t searchUiPLs[] = {
#define I_QUERYSTRING  (0)
	{ PD_STRING, searchUiQuery, "query", PDO_ENTER | PDO_NOPREF | PDO_STRINGLIMITLENGTH | PDO_DLGRESIZE, I2VP(340), "", 0, 0, MAXQUERYLENGTH-1 },
#define I_SEARCHBUTTON (1)
	{ PD_BUTTON, SearchUiDoSearch, "find", PDO_DLGHORZ, 0, NULL,  BO_ICON, NULL },
#define I_CLEARBUTTON (2)
	{ PD_BUTTON, SearchUiClearFilter, "clearfilter", PDO_DLGHORZ, 0, NULL,  BO_ICON, NULL },
#define I_FITRADIO	(3)
	{	PD_RADIO, &searchFitMode, "fit", PDO_NOPREF | PDO_DLGBOXEND, searchFitLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_MESSAGE (4)
	{ PD_MESSAGE, N_(QUERYPROMPTSTRING), NULL, 0, I2VP(370) },
#define I_STATISTICS (5)
	{ PD_MESSAGE, "", NULL, PDO_DLGBOXEND, I2VP(370) },
#define I_RESULTLIST	(6)
	{	PD_LIST, NULL, "inx", PDO_NOPREF | PDO_DLGRESIZE, &searchUiListData, NULL, BL_DUP|BL_SETSTAY|BL_MANY },
#define I_MODETOGGLE	(7)
	{	PD_TOGGLE, &searchUiMode, "mode", PDO_DLGBOXEND, searchUiLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_APPLYBUTTON	(8)
	{	PD_BUTTON, SearchUiApply, "apply", PDO_DLGCMDBUTTON, NULL, N_("Add") },
#define I_SELECTALLBUTTON (9)
	{	PD_BUTTON, SearchUiSelectAll, "selectall", PDO_DLGCMDBUTTON, NULL, N_("Select all") },
};

#define SEARCHBUTTON ((wButton_p)searchUiPLs[I_SEARCHBUTTON].control)
#define CLEARBUTTON ((wButton_p)searchUiPLs[I_CLEARBUTTON].control)
#define RESULTLIST	 ((wList_p)searchUiPLs[I_RESULTLIST].control)
#define APPLYBUTTON  ((wButton_p)searchUiPLs[I_APPLYBUTTON].control)
#define SELECTALLBUTTON  ((wButton_p)searchUiPLs[I_SELECTALLBUTTON].control)
#define MESSAGETEXT ((wMessage_p)searchUiPLs[I_MESSAGE].control)
#define QUERYSTRING ((wString_p)searchUiPLs[I_QUERYSTRING].control)
#define SEARCHSTAT ((wMessage_p)searchUiPLs[I_STATISTICS].control)
#define FITRADIO ((wChoice_p)searchUiPLs[I_FITRADIO].control)

static paramGroup_t searchUiPG = { "searchgui", 0, searchUiPLs, COUNT( searchUiPLs ) };
static wWin_p searchUiW;

#define FILESECTION "file"
#define PARAMDIRECTORY "paramdir"


/**
 * Clears the current catalog
 */

void
ClearCurrentCatalog(void)
{
	if (currentCat.head) {
		DestroyCatalog(&currentCat);
		currentCat.head = NULL;
	}
}

/**
 * Reload the listbox showing the current catalog. The catalog is either the system
 * default library catalog or a search result
 *
 * \param [in] catalog the current catalog.
 */

static
int SearchFileListLoad(Catalog *catalog)

{
	CatalogEntry *head = catalog->head;
	CatalogEntry *catalogEntry;

	DynString description;
	DynStringMalloc(&description, STR_SHORT_SIZE);

	wControlShow((wControl_p)RESULTLIST, FALSE);
	wListClear(RESULTLIST);

	DL_FOREACH(head, catalogEntry) {
		for (unsigned int i=0; i<catalogEntry->files; i++) {
			if (catalogEntry->tag && searchFitMode != 0) {
				char * type_copy = MyStrdup(catalogEntry->tag);
				char * cp = type_copy;
				char * type = strtok(cp, " \t");
				SCALE_FIT_TYPE_T fit_type = FIT_STRUCTURE;
				if (strcmp(type,TURNOUTCOMMAND) == 0) {
					fit_type = FIT_TURNOUT;
				} else if (strcmp(type,STRUCTURECOMMAND)==0) {
					fit_type = FIT_STRUCTURE;
				} else if ((strcmp(type,CARCOMMAND)==0) || (strcmp(type,CARPROTOCOMMAND)==0)) {
					fit_type = FIT_CAR;
				}
				char * scale = strtok(NULL, " \t\n");
				if (scale) {
					SCALEINX_T scale1 = LookupScale(scale);
					SCALEINX_T scale2 = GetLayoutCurScale();
					if (searchFitMode == FIT_COMPATIBLE) {
						if (CompatibleScale(fit_type,scale1,scale2)<FIT_COMPATIBLE) { continue; }
					} else {
						if (CompatibleScale(fit_type,scale1,scale2)<FIT_EXACT) { continue; }
					}

				}
				MyFree(type_copy);
			}
			DynStringClear(&description);
			DynStringCatCStr(&description,
			                 ((!searchUiMode) && catalogEntry->contents) ?
			                 catalogEntry->contents :
			                 catalogEntry->fullFileName[i]);

			wListAddValue(RESULTLIST,
			              DynStringToCStr(&description),
			              NULL,
			              catalogEntry->fullFileName[i]);
		}
	}

	wControlShow((wControl_p)RESULTLIST, TRUE);
	wControlActive((wControl_p)SELECTALLBUTTON,
	               wListGetCount(RESULTLIST));

	DynStringFree(&description);
	return wListGetCount(RESULTLIST);
}

/**
 * Reload just the system files into the searchable set
 */

static void SearchUiDefault(void)
{
	DynString dsSummary;

	int matches = SearchFileListLoad(
	                      trackLibrary->catalog);  //Start with system files
	wStringSetValue(QUERYSTRING, "");

	wMessageSetValue(MESSAGETEXT, _(QUERYPROMPTSTRING));
	DynStringMalloc(&dsSummary, 16);
	DynStringPrintf(&dsSummary, _("%u parameter files in library. %d Fit Scale."),
	                CountCatalogEntries(trackLibrary->catalog), matches);
	wMessageSetValue(SEARCHSTAT, DynStringToCStr(&dsSummary));
	DynStringFree(&dsSummary);

	wControlActive((wControl_p)CLEARBUTTON, FALSE);
}

/**
 * Load the selected items of search results
 */

void static
SearchUILoadResults(void)
{
	int files = wListGetSelectedCount(RESULTLIST);

	if (files) {
		char **fileNames;
		int found = 0;
		fileNames = MyMalloc(sizeof(char *)*files);

		for (int inx = 0; found < files; inx++) {
			if (wListGetItemSelected(RESULTLIST, inx)) {
				fileNames[found++] = (char *)wListGetItemContext(RESULTLIST, inx);
			}
		}

		LoadParamFile(files, fileNames, NULL);
		MyFree(fileNames);
		SearchUiOk(NULL);
	}

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
 * Return a pointer to the (shifted) trimmed string
 *
 * \param [in,out] s If non-null, a char to process.
 *
 * \returns pointer to the trimmed string
 */

char * StringTrim(char *s)
{
	char *original = s;
	size_t len = 0;

	while (isspace((unsigned char) *s)) {
		s++;
	}
	if (*s) {
		char *p = s;
		while (*p) {
			p++;
		}
		while (isspace((unsigned char) *(--p)));
		p[1] = '\0';
		len = (size_t)(p - s + 1);
	}

	return (s == original) ? s : memmove(original, s, len + 1);
}

/**
 * Perform the search. If successful, the results are loaded into the list
 *
 * \param [in,out] ptr ignored.
 */

static void SearchUiDoSearch(void * ptr)
{
	unsigned result;
	SearchResult *currentResults = MyMalloc(sizeof(SearchResult));
	char * search;

	ClearCurrentCatalog();

	strcpy(searchUiQuery, wStringGetValue((wString_p)
	                                      searchUiPG.paramPtr[I_QUERYSTRING].control));
	search = StringTrim(searchUiQuery);

	if (search[0]) {
		result = SearchLibrary(trackLibrary, search, currentResults);

		if (result) {

			char *statistics;

			statistics = SearchStatistics(currentResults);
			wMessageSetValue(SEARCHSTAT, statistics);
			MyFree(statistics);

			int matches = SearchFileListLoad(&(currentResults->subCatalog));

			DynString hitsMessage;
			DynStringMalloc(&hitsMessage, 16);
			DynStringPrintf(&hitsMessage, _("%d parameter files found. %d Fit Scale"),
			                result, matches);
			wMessageSetValue(MESSAGETEXT, DynStringToCStr(&hitsMessage));
			DynStringFree(&hitsMessage);

			currentCat = currentResults->subCatalog;
			wControlActive((wControl_p)CLEARBUTTON, TRUE);
		} else {
			wListClear(RESULTLIST);
			wControlActive((wControl_p)SELECTALLBUTTON, FALSE);
			wMessageSetValue(MESSAGETEXT, _("No matches found."));
		}
	} else {
		SearchUiDefault();
	}
	MyFree(currentResults);  //Because SearchFileList also caches the currentResults->subCatalog address as currentCatalog for reuse.
}

/**
 * Clear the current filter
 *
 * \param [in,out] ptr ignored
 */

static void
SearchUiClearFilter(void *ptr)
{
	ClearCurrentCatalog();
	SearchUiDefault();
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
 * \param [in,out] junk ignored.
 */

void SearchUiOk(void * junk)
{
	if (searchUiW) {
		wHide(searchUiW);
	}
}

/**
 * Handle the Add button: a list of selected list elements is created and
 * passed to the parameter file list.
 *
 * \param junk IN/OUT ignored
 */

static void SearchUiApply(wWin_p junk)
{
	SearchUILoadResults();
}

/**
 * Event handling for the Search dialog. If the 'X' decoration is pressed the
 * dialog window is closed.
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
	case I_QUERYSTRING:
		if (pg->paramPtr[inx].enter_pressed) {
			strcpy( searchUiQuery, wStringGetValue((wString_p)pg->paramPtr[inx].control) );
			SearchUiDoSearch(NULL);
		}
		break;
	case I_RESULTLIST:
		UpdateSearchUiButton();
		break;
	case I_FITRADIO:
		strcpy( searchUiQuery, wStringGetValue((wString_p)
		                                       pg->paramPtr[I_QUERYSTRING].control) );
		SearchUiDoSearch(NULL);
		break;
	case I_MODETOGGLE:
		if (currentCat.head) {
			SearchFileListLoad(&currentCat);
		} else {
			SearchFileListLoad(trackLibrary->catalog);
		}
		break;
	case -1:
		SearchUiOk(valueP);
		break;
	}
}

void
SearchUiCancel(wWin_p window)
{
	ClearCurrentCatalog();
	wHide(window);
}

/**
 * Get the system default directory for parameter files. First step is to
 * check the configuration file for a user specific setting. If that is not
 * found, the diretory is based derived from the installation directory.
 * The returned string has to be free'd() when no longer needed.
 *
 * \return   parameter file directory
 */

static char *
GetParamsPath()
{
	char * params_path;
	char *params_pref;
	params_pref = wPrefGetString(FILESECTION, PARAMDIRECTORY);

	if (!params_pref) {
		MakeFullpath(&params_path, wGetAppLibDir(), "params", NULL);
	} else {
		params_path = strdup(params_pref);
	}
	return (params_path);
}

#include "bitmaps/funnel.xpm"
#include "bitmaps/funnelclear.xpm"

/**
 * Create and open the search dialog.
 *
 * \param junk
 */

void DoSearchParams(void * junk)
{
	if (searchUiW == NULL) {

		//Make the Find menu bound to the System Library initially
		char *paramsDir = GetParamsPath();
		trackLibrary = CreateLibrary(paramsDir);
		free(paramsDir);

		searchUiPLs[I_SEARCHBUTTON].winLabel = (char *)wIconCreatePixMap(funnel_xpm);
		searchUiPLs[I_CLEARBUTTON].winLabel = (char *)wIconCreatePixMap(
		                funnelclear_xpm);

		searchFitMode = FIT_COMPATIBLE;  //Default to "Any" after startup

		ParamRegister(&searchUiPG);



		searchUiW = ParamCreateDialog(&searchUiPG,
		                              MakeWindowTitle(_("Choose parameter files")), _("Done"), NULL,
					      ParamCancel_Custom( SearchUiCancel ),
		                              TRUE, NULL, F_RESIZE | F_RECALLSIZE, SearchUiDlgUpdate);


		wControlActive((wControl_p)APPLYBUTTON, FALSE);
		wControlActive((wControl_p)SELECTALLBUTTON, FALSE);
	}

	wControlActive((wControl_p)FITRADIO, TRUE);

	ParamLoadControls(&searchUiPG);
	ParamGroupRecord(&searchUiPG);


	if (!trackLibrary) {
		wControlActive((wControl_p)SEARCHBUTTON, FALSE);
		wControlActive((wControl_p)QUERYSTRING, FALSE);
		wMessageSetValue(MESSAGETEXT,
		                 _("No system parameter files found, search is disabled."));
	} else {
		SearchUiDefault();
	}

	wShow(searchUiW);
}


