/** \file dcustmgm.c
 * Custom List Management
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "custom.h"
#include "fileio.h"
#include "form.h"
#include "paths.h"
#include "include/paramfilelist.h"
#include "common-ui.h"
#include "ctrain.h"
#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif

static void CustomEdit( void * action );
static void CustomDelete( void * action );
static void CustomExport( void * action );
static void CustomDone( void * action );
static void SelectNew(void *context );

#ifdef TODO_UNUSED
static const char * customTypes[] = { "Car Part", "Car Prototype", NULL };
#endif
static wIndex_t selectedType;

static wWinPix_t customListWidths[] = { 18, 100, 30, 80, 220 };
static const char * customListTitles[] = { "", N_("Manufacturer"),
                                           N_("Scale"), N_("Part No"), N_("Description")
                                         };
static paramListData_t customListData = { 10, 400, 5, customListWidths, customListTitles };
static paramData_t customPLs[] = {
#define I_CUSTOMLIST	(0)
#define customSelL		(customPLs[I_CUSTOMLIST].control)
	{	PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE|PDO_DLGBOXEND, &customListData, NULL, BL_MANY },
#define I_CUSTOMNEWTYPE (1)
	{   PD_MENU, &selectedType, "select_new", 0L, I2VP(150), NULL },
#define I_CUSTOMNEW     (2)
	{   PD_BUTTON, NULL, "newcar", PDO_DLGHORZ| PDO_DLGBOXEND, NULL },
#define I_CUSTOMEDIT	(3)
	{	PD_BUTTON, CustomEdit, "edit", PDO_DLGCMDBUTTON, NULL},
#define I_CUSTOMDEL		(4)
	{	PD_BUTTON, CustomDelete, "delete", 0, NULL},
#define I_CUSTOMCOPYTO	(5)
	{	PD_BUTTON, CustomExport, "export", 0, NULL},
#define I_NEWPROTO	(6)
	{	PD_MENUITEM, SelectNew, "newproto", .context = I2VP(I_NEWPROTO)},
#define I_NEWPART (7)
	{	PD_MENUITEM, SelectNew, "newpart", .context = I2VP(I_NEWPART)},
} ;

static paramGroup_t customPG = { "custmgm", PGO_FULLDIALOGFROMBUILDER, customPLs, COUNT( customPLs ) };


typedef struct {
	custMgmCallBack_p proc;
	void * data;
	wIcon_p icon;
} custMgmContext_t, *custMgmContext_p;


static wBool_t CustomDlgUpdate(
        paramGroup_p pg,
        int inx,
        void *valueP )
{
	custMgmContext_p context = NULL;
	wIndex_t selcnt = wListGetSelectedCount(customPLs[I_CUSTOMLIST].control );
	
	if ( inx != I_CUSTOMLIST ) { return(TRUE); }

	if ( selcnt == 1 ) {
		wIndex_t lcnt = wListGetCount(pg->paramPtr[inx].control );

		for (int linx = 0; linx < lcnt; linx++)
		{
			if (wListGetItemSelected(customPLs[I_CUSTOMLIST].control, linx)) {
				context = (custMgmContext_p)wListGetItemContext(pg->paramPtr[I_CUSTOMLIST].control, linx);

				wButtonSetLabel(customPLs[I_CUSTOMEDIT].control,
					context->proc(CUSTMGM_CAN_EDIT, context->data) ? _("Edit") : _("Rename"));
				FormControlActive(&customPG, I_CUSTOMEDIT, TRUE);
			}
		}
	} else {
		FormControlActive( &customPG, I_CUSTOMEDIT, FALSE );
	}
	FormControlActive( &customPG, I_CUSTOMDEL, selcnt>0 );
	FormControlActive( &customPG, I_CUSTOMCOPYTO, selcnt>0 );

	return(TRUE);
}


static void CustomEdit( void * action )
{
	custMgmContext_p context = NULL;
	wIndex_t selcnt = wListGetSelectedCount(customPLs[0].control );
	wIndex_t inx, cnt;

	if ( selcnt != 1 ) {
		return;
	}
	cnt = wListGetCount(customPLs[0].control );
	for ( inx=0;
	      inx<cnt && wListGetItemSelected(customPLs[0].control, inx ) != TRUE;
	      inx++ );
	if ( inx >= cnt ) {
		return;
	}
	context = (custMgmContext_p)wListGetItemContext( customSelL, inx );
	if ( context == NULL ) {
		return;
	}
	context->proc( CUSTMGM_DO_EDIT, context->data );
#ifdef OBSOLETE
	context->proc( CUSTMGM_GET_TITLE, context->data );
	wListSetValues( customSelL, inx, message, context->icon, context );
#endif
}

static void
SelectNew(void *context)
{
	unsigned index = (unsigned)VP2L(context);
	void(*handler)(void);
	const char *label;
	paramData_p menuItem = customPLs+index;;

	// get selected function
	switch(index) {
		case I_NEWPART:
		handler = CarDlgAddDesc;
		break;
		case I_NEWPROTO:
		handler = CarDlgAddProto;
		break;
	}
	label = wMenuGetLabel(menuItem->control);

	// configure the button
	wButtonSetLabel((customPLs+I_CUSTOMNEW)->control, label);
	customPLs[I_CUSTOMNEW].valueP = handler;	
}

static void CustomDelete( void * action )
{
	wIndex_t selcnt = wListGetSelectedCount(customPLs[I_CUSTOMLIST].control );
	wIndex_t inx, cnt;
	custMgmContext_p context = NULL;

	if ( selcnt <= 0 ) {
		return;
	}
	if ( (!NoticeMessage2( 1, MSG_CUSTMGM_DELETE_CONFIRM, _("Yes"), _("No"),
	                       selcnt ) ) ) {
		return;
	}

	cnt = wListGetCount(customPLs[I_CUSTOMLIST].control );
	for ( inx=0; inx<cnt; inx++ ) {
		if ( !wListGetItemSelected(customPLs[I_CUSTOMLIST].control, inx ) ) {
			continue;
		}
		context = (custMgmContext_p)wListGetItemContext( customSelL, inx );
		context->proc( CUSTMGM_DO_DELETE, context->data );
		MyFree( context );
	}

	wListDeleteSelected(customPLs[I_CUSTOMLIST].control);

	DoChangeNotification( CHANGE_PARAMS );
}

static struct wFilSel_t * customMgmExport_fs;
EXPORT FILE * customMgmF;
static char custMgmContentsStr[STR_SIZE];
static BOOL_T custMgmProceed;
static paramData_t custMgmContentsPLs[] = {
	{ PD_STRING, custMgmContentsStr, "label", PDO_NOTBLANK, I2VP(40), N_("Label"), 0, 0, sizeof(custMgmContentsStr)}
};
static paramGroup_t custMgmContentsPG = { "contents", 0, custMgmContentsPLs, COUNT( custMgmContentsPLs ) };

static void CustMgmContentsOk( void * junk )
{
	custMgmProceed = TRUE;
	wHide( custMgmContentsPG.win );
}

/**
 * Save custom parameter definitions to a parameter file.
 *
 * \param files		count of filenames, must be 1
 * \param fileName	array of filenames, one member only
 * \param data		unused
 * \return			TRUE on success, FALSE in case of failure
 */

static int CustomDoExport(
        int files,
        char ** fileName,
        void * data )
{
	int rc;
	wIndex_t selcnt = wListGetSelectedCount(customPLs[0].control );
	wIndex_t inx, cnt;
	custMgmContext_p context = NULL;

	CHECK( fileName != NULL );
	CHECK( files == 1 );

	if ( selcnt <= 0 ) {
		return FALSE;
	}

	SetCurrentPath( CUSTOMPATHKEY, fileName[ 0 ] );
	rc = access( fileName[ 0 ], F_OK );
	if ( rc != -1 ) {
		rc = access( fileName[ 0 ], W_OK );
		if ( rc == -1 ) {
			NoticeMessage( MSG_CUSTMGM_CANT_WRITE, _("Ok"), NULL, fileName[ 0 ] );
			return FALSE;
		}
		custMgmProceed = TRUE;
	} else {
		if ( custMgmContentsPG.win == NULL ) {
			FormCreateDialog( &custMgmContentsPG, MakeWindowTitle(_("Contents Label")),
			                   _("Ok"), CustMgmContentsOk, 
							   _("Cancel"), FormCancel_Current, 
								TRUE, F_BLOCK, NULL);
		}
		custMgmProceed = FALSE;
		wShow( custMgmContentsPG.win );
	}
	if ( !custMgmProceed ) {
		return FALSE;
	}
	customMgmF = fopen( fileName[ 0 ], "a" );
	if ( customMgmF == NULL ) {
		NoticeMessage( MSG_CUSTMGM_CANT_WRITE, _("Ok"), NULL, fileName[ 0 ] );
		return FALSE;
	}

	SetCLocale();

	if (rc == -1) {
#ifdef UTFCONVERT
		char *contents = MyStrdup(custMgmContentsStr);
		contents = Convert2UTF8(contents);
		fprintf(customMgmF, "CONTENTS %s\n", contents);
		MyFree(contents);
#else
		fprintf(customMgmF, "CONTENTS %s\n", custMgmContentsStr);
#endif // UTFCONVERT
	}

	cnt = wListGetCount(customPLs[0].control );
	for ( inx=0; inx<cnt; inx++ ) {
		if ( !wListGetItemSelected(customPLs[0].control, inx ) ) {
			continue;
		}
		context = (custMgmContext_p)wListGetItemContext( customSelL, inx );
		if ( context == NULL ) { continue; }
		if (!context->proc( CUSTMGM_DO_COPYTO, context->data )) {
			NoticeMessage( MSG_WRITE_FAILURE, _("Ok"), NULL, strerror(errno),
			               fileName[ 0 ] );
			fclose( customMgmF );
			SetUserLocale();
			return FALSE;
		}
		context->proc( CUSTMGM_DO_DELETE, context->data );
		MyFree( context );
		wListDelete( customSelL, inx );
		inx--;
		cnt--;
	}
	fclose( customMgmF );
	SetUserLocale();
	LoadParamFile( 1, fileName, NULL );
	DoChangeNotification( CHANGE_PARAMS );
	return TRUE;
}


static void CustomExport( void * junk )
{
	if ( customMgmExport_fs == NULL )
		customMgmExport_fs = wFilSelCreate( mainW, FS_UPDATE, 0, _("Move To XTP"),
		                                    _("Parameter File (*.xtp)|*.xtp"), CustomDoExport, NULL );
	wFilSelect( customMgmExport_fs, GetCurrentPath(CUSTOMPATHKEY));
}


static void CustomDone( void * action )
{
	FILE * f = OpenCustom("w");

	if (f == NULL) {
		wHide( customPG.win );
		return;
	}
	SetCLocale();
	CompoundCustomSave(f);
	CarCustomSave(f);
	fclose(f);
	SetUserLocale();
	wHide( customPG.win );
}


EXPORT void CustMgmLoad(
        wIcon_p icon,
        custMgmCallBack_p proc,
        void * data )
{
	custMgmContext_p context;
	context = MyMalloc( sizeof *context );
	context->proc = proc;
	context->data = data;
	context->icon = icon;
	context->proc( CUSTMGM_GET_TITLE, context->data );
	wListAddValue( customSelL, message, icon, context );
}


static void LoadCustomMgmList( void )
{
	wIndex_t curInx, cnt=0;
	long tempL;
	custMgmContext_p context;

	curInx = wListGetIndex( customSelL );

	if ( curInx >= 0 ) {
		context = (custMgmContext_p)wListGetItemContext( customSelL, curInx );
	}
	cnt = wListGetCount( customSelL );
	for ( curInx=0; curInx<cnt; curInx++ ) {
		context = (custMgmContext_p)wListGetItemContext( customSelL, curInx );
		if ( context ) {
			MyFree( context );
		}
	}
	curInx = wListGetIndex( customSelL );
	wControlShow( (wControl_p)customSelL, FALSE );
	wListClear( customSelL );

	CompoundCustMgmLoad();
	CarCustMgmLoad();

	tempL = -1;
	CustomDlgUpdate( &customPG, I_CUSTOMLIST, &tempL );
	wControlShow( (wControl_p)customSelL, TRUE );
}


static void CustMgmChange( long changes )
{
	wIndex_t selcnt; 

	if (changes) {
		if (changed) {
			changed = checkPtMark = 1;
		}
	}
	if ((changes&CHANGE_PARAMS) == 0 ||
	    customPG.win == NULL || !wWinIsVisible(customPG.win) ) {
		return;
	}

	selcnt = wListGetSelectedCount(customPLs[0].control);
	if (selcnt) {
		for (int i = 0; i < wListGetCount(customPG.paramPtr[I_CUSTOMLIST].control); i++)
		{
			if (wListGetItemSelected(customPLs[0].control, i)) {
				custMgmContext_p context = (custMgmContext_p)wListGetItemContext(customSelL, i);
				context->proc(CUSTMGM_GET_TITLE, context->data);
				wListSetValues(customPG.paramPtr[I_CUSTOMLIST].control, i, message, NULL, NULL);
				break;
			}
		}
		return;
	}
	LoadCustomMgmList();
}

static void DoCustomMgr( void * junk )
{
#ifdef TODO_UNUSED
	int i = 0;
#endif

	if (customPG.win == NULL) {
		FormCreateDialog( &customPG, MakeWindowTitle(_("Manage custom designed parts")), 
							_("Done"), CustomDone,
							NULL, NULL,
							TRUE, F_RESIZE|F_RECALLSIZE|F_BLOCK,
		                   CustomDlgUpdate );
		SelectNew(I2VP(I_NEWPART));
		//selectedType = 0;
		// wComboBoxSetIndex( (customPLs[I_CUSTOMNEWTYPE].control), selectedType);

	} else {
		wListClear( customSelL );
	}

	/*FormLoadControls( &customPG );*/
	/*FormGroupRecord( &customPG );*/
	LoadCustomMgmList();
	wShow( customPG.win );
}


EXPORT addButtonCallBack_t CustomMgrInit( void )
{
	FormRegister( &customPG );
	FormRegister( &custMgmContentsPG );
	RegisterChangeNotification( CustMgmChange );
	return &DoCustomMgr;
}
