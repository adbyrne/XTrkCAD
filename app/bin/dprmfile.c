/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/dprmfile.c,v 1.3 2008-03-10 18:59:53 m_fischer Exp $
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

#include <time.h>
#include "track.h"
#include "i18n.h"

#include <stdint.h>

/****************************************************************************
 *
 * Param File Management
 *
 */

typedef struct {
		char * name;
		char * contents;
		int deleted;
		int deletedShadow;
		int valid;
		} paramFileInfo_t;
typedef paramFileInfo_t * paramFileInfo_p;
static dynArr_t paramFileInfo_da;
#define paramFileInfo(N) DYNARR_N( paramFileInfo_t, paramFileInfo_da, N )

EXPORT int curParamFileIndex = PARAM_DEMO;
static char curParamDir[STR_LONG_SIZE];
static struct wFilSel_t * paramFile_fs;


EXPORT wBool_t IsParamValid(
		int fileInx )
{
	if (fileInx == PARAM_DEMO)
		return (curDemo>=0);
	else if (fileInx == PARAM_CUSTOM)
		return TRUE;
	else if (fileInx == PARAM_LAYOUT)
		return TRUE;
	else if (fileInx >= 0 && fileInx < paramFileInfo_da.cnt)
		return (!paramFileInfo(fileInx).deleted) && paramFileInfo(fileInx).valid;
	else
		return FALSE;
}


EXPORT char * GetParamFileName(
		int fileInx )
{
	return paramFileInfo(fileInx).contents;
}


static BOOL_T UpdateParamFiles( void )
{
	char fileName[STR_LONG_SIZE], *fileNameP;
	char * contents;
	const char * cp;
	FILE * updateF;
	FILE * paramF;
	long updateTime;
	long lastTime;

	sprintf( message, "%s%sxtrkcad.upd", libDir, FILE_SEP_CHAR );
	updateF = fopen( message, "r" );
	if ( updateF == NULL )
		return FALSE;
	if ( fgets( message, sizeof message, updateF ) == NULL ) {
		NoticeMessage( "short file: xtrkcad.upd", _("Ok"), NULL ); 
		return FALSE;
	}
	wPrefGetInteger( "file", "updatetime", &lastTime, 0 );
	updateTime = atol( message );
	if ( lastTime >= updateTime )
		return FALSE;
	sprintf( fileName, "%s%sparams%s", libDir, FILE_SEP_CHAR, FILE_SEP_CHAR );
	fileNameP = fileName+strlen(fileName);
	while ( ( fgets( fileNameP, (fileName+sizeof fileName)-fileNameP, updateF ) ) != NULL ) {
		Stripcr( fileNameP );
		InfoMessage( _("Updating %s"), fileNameP );
		paramF = fopen( fileName, "r" );
		if ( paramF == NULL ) {
			NoticeMessage( MSG_PRMFIL_OPEN_NEW, _("Ok"), NULL, fileName );
			continue;
		}
		contents = NULL;
		while ( ( fgets(message, sizeof message, paramF) ) != NULL ) {
			if (strncmp( message, "CONTENTS", 8 ) == 0) {
				Stripcr( message );
				contents = message+9;
				break;
			}
		}
		fclose( paramF );
		if (contents == NULL) {
			NoticeMessage( MSG_PRMFIL_NO_CONTENTS, _("Ok"), NULL, fileName );
			continue;
		}
		cp = wPrefGetString( "Parameter File Map", contents );
		wPrefSetString( "Parameter File Map", contents, fileName );
		if (cp!=NULL && *cp!='\0') {
			/* been there, done that */
			continue;
		}

		DYNARR_APPEND( paramFileInfo_t, paramFileInfo_da, 10 );
		curParamFileIndex = paramFileInfo_da.cnt-1;
		paramFileInfo(curParamFileIndex).name = MyStrdup( fileName );
		curContents = curSubContents = NULL;
		paramFileInfo(curParamFileIndex).deleted = FALSE;
		paramFileInfo(curParamFileIndex).valid = TRUE;
		paramFileInfo(curParamFileIndex).deletedShadow = 
		paramFileInfo(curParamFileIndex).deleted = !ReadParams( 0, NULL, fileName );
		paramFileInfo(curParamFileIndex).contents = curContents;
	}
	wPrefSetInteger( "file", "updatetime", updateTime );
	return TRUE;
}


EXPORT void ReadParamFiles( void )
{
	int fileNo;
	const char *fileName;
	const char * contents;
	BOOL_T updated = FALSE;

	updated = UpdateParamFiles();

	for ( fileNo=1; ; fileNo++ ) {
		sprintf( message, "File%d", fileNo );
		contents = wPrefGetString( "Parameter File Names", message );
		if (contents==NULL || *contents=='\0')
			break;
		InfoMessage( "Parameters for %s", contents );
		fileName = wPrefGetString( "Parameter File Map", contents );
		if (fileName==NULL || *fileName=='\0') {
			NoticeMessage( MSG_PRMFIL_NO_MAP, _("Ok"), NULL, contents );
			continue;
		}
		DYNARR_APPEND( paramFileInfo_t, paramFileInfo_da, 10 );
		curParamFileIndex = paramFileInfo_da.cnt-1;
		paramFileInfo(curParamFileIndex).name = MyStrdup( fileName );
		curContents = NULL;
		paramFileInfo(curParamFileIndex).deleted = FALSE;
		paramFileInfo(curParamFileIndex).valid = TRUE;
		paramFileInfo(curParamFileIndex).deletedShadow = 
		paramFileInfo(curParamFileIndex).deleted = !ReadParams( 0, NULL, fileName );
		if (curContents == NULL)
			curContents = curSubContents = MyStrdup(contents);
		paramFileInfo(curParamFileIndex).contents = curContents;
	}
	curParamFileIndex = PARAM_CUSTOM;
	if (updated) {
		RememberParamFiles();
	}
}


EXPORT void RememberParamFiles( void )
{
	int fileInx;
	int fileNo;
	char * contents, *cp;

	for (fileInx=0, fileNo=1; fileInx<paramFileInfo_da.cnt; fileInx++ ) {
		if (paramFileInfo(fileInx).valid && !paramFileInfo(fileInx).deleted) {
			sprintf( message, "File%d", fileNo++ );
			contents = paramFileInfo(fileInx).contents;
			for ( cp=contents; *cp; cp++ ) {
				if ( *cp == '=' || *cp == '\'' || *cp == '"'  || *cp == ':' || *cp == '.' )
					*cp = ' ';
			}
			wPrefSetString( "Parameter File Names", message, contents );
		}
	}
	sprintf( message, "File%d", fileNo++ );
	wPrefSetString( "Parameter File Names", message, "" );
}



/****************************************************************************
 *
 * Param File Dialog
 *
 */

static wWin_p paramFileW;

static long paramFileSel = 1;
static wIcon_p mtbox_bm;
static wIcon_p chkbox_bm;

static void ParamFileAction( void * );
static void ParamFileBrowse( void * );

static paramListData_t paramFileListData = { 10, 370 };
static char * paramFileLabels[] = { N_("Show File Names"), NULL };
static paramData_t paramFilePLs[] = {
#define I_PRMFILLIST	(0)
#define paramFileL				((wList_p)paramFilePLs[I_PRMFILLIST].control)
	{	PD_LIST, NULL, "inx", 0, &paramFileListData, NULL, BL_DUP|BL_SETSTAY },
#define I_PRMFILTOGGLE	(1)
	{	PD_TOGGLE, &paramFileSel, "mode", 0, paramFileLabels, NULL, BC_HORZ|BC_NOBORDER },
#define I_PRMFILACTION	(2)
#define paramFileActionB		((wButton_p)paramFilePLs[I_PRMFILACTION].control)
	{	PD_BUTTON, (void*)ParamFileAction, "action", PDO_DLGCMDBUTTON, NULL, N_("Unload") },
	{	PD_BUTTON, (void*)ParamFileBrowse, "browse", 0, NULL, N_("Browse ...") } };

static paramGroup_t paramFilePG = { "prmfile", 0, paramFilePLs, sizeof paramFilePLs/sizeof paramFilePLs[0] };


static void ParamFileLoadList( void )
{
	int fileInx;
	wIndex_t listInx;
	wControlShow( (wControl_p)paramFileL, FALSE );
	listInx = wListGetIndex(paramFileL);
	wListClear( paramFileL );
	for ( fileInx = 0; fileInx < paramFileInfo_da.cnt; fileInx++ ) {
		if (paramFileInfo(fileInx).valid) {
			strcpy( message, ((!paramFileSel) && paramFileInfo(fileInx).contents)?
						paramFileInfo(fileInx).contents:
						paramFileInfo(fileInx).name );
			wListAddValue( paramFileL, message, (paramFileInfo(fileInx).deleted)?mtbox_bm:chkbox_bm, (void*)(intptr_t)fileInx );
		}
	}
	wListSetIndex( paramFileL, listInx );
	wControlShow( (wControl_p)paramFileL, TRUE );
}


EXPORT int LoadParamFile(
		const char * pathName,
		const char * fileName,
		void * data )
{
	char * cp;
	wIndex_t inx;
	wBool_t redrawList;

	if (pathName == NULL)
		return TRUE;
	memcpy( curParamDir, pathName, fileName-pathName );
	curParamDir[fileName-pathName] = '\0';
	wPrefSetString( "file", "paramdir", curParamDir );

	redrawList = FALSE;
	curContents = curSubContents = NULL;
	curParamFileIndex = paramFileInfo_da.cnt;
	if ( !ReadParams( 0, NULL, pathName ) )
		return FALSE;
	if (curContents == NULL) {
		curContents = curSubContents = MyStrdup( fileName );
		for ( cp=curContents; *cp; cp++ ) {
			if ( *cp == '=' || *cp == '\'' || *cp == '"'  || *cp == ':' || *cp == '.' )
				*cp = ' ';
		}
	}

	for ( inx=0; inx<paramFileInfo_da.cnt; inx++ ) {
		if ( paramFileInfo(inx).valid &&
			 strcmp( paramFileInfo(inx).contents, curContents ) == 0 ) {
			paramFileInfo(inx).valid = FALSE;
			redrawList = TRUE;
			break;
		}
	}

	DYNARR_APPEND( paramFileInfo_t, paramFileInfo_da, 10 );
	paramFileInfo(curParamFileIndex).name = MyStrdup( pathName );
	paramFileInfo(curParamFileIndex).valid = TRUE;
	paramFileInfo(curParamFileIndex).deleted = FALSE;
	paramFileInfo(curParamFileIndex).deletedShadow =
	paramFileInfo(curParamFileIndex).deleted = FALSE;
	paramFileInfo(curParamFileIndex).contents = curContents;

	if ( paramFilePG.win ) {
		if ( redrawList ) {
			ParamFileLoadList();
		} else {
			strcpy( message, ((!paramFileSel) && paramFileInfo(curParamFileIndex).contents)?
						paramFileInfo(curParamFileIndex).contents:
						paramFileInfo(curParamFileIndex).name );
			wListAddValue( paramFileL, message, chkbox_bm, (void*)(intptr_t)curParamFileIndex );
			wListSetIndex( paramFileL, wListGetCount(paramFileL)-1 );
		}
	}

	wPrefSetString( "Parameter File Map", curContents,
				paramFileInfo(curParamFileIndex).name );
	curParamFileIndex = PARAM_CUSTOM;
	DoChangeNotification( CHANGE_PARAMS );
	return TRUE;
}


static void ParamFileBrowse( void * junk )
{
	wFilSelect( paramFile_fs, curParamDir );
	return;
}


static void UpdateParamFileButton(
		wIndex_t fileInx )
{
	if (fileInx < 0 || fileInx >= paramFileInfo_da.cnt)
		return;
	wButtonSetLabel( paramFileActionB,
		paramFileInfo(fileInx).deleted?_("Reload"):_("Unload") );
}


static void ParamFileAction( void * junk )
{
	wIndex_t listInx;
	wIndex_t fileInx;
	void * data;
	listInx = wListGetValues( paramFileL, NULL, 0, NULL, &data );
	if (listInx<0)
		return;
	fileInx = (wIndex_t)(long)data;
	paramFileInfo(fileInx).deleted = ! paramFileInfo(fileInx).deleted;
#ifndef LATER
	strcpy( message, ((!paramFileSel) && paramFileInfo(fileInx).contents)?
					 paramFileInfo(fileInx).contents:
					 paramFileInfo(fileInx).name );
	wListSetValues( paramFileL, listInx, message, (paramFileInfo(fileInx).deleted)?mtbox_bm:chkbox_bm, (void*)(intptr_t)fileInx );
#endif
	DoChangeNotification( CHANGE_PARAMS );
	UpdateParamFileButton( fileInx );
}


static void ParamFileOk( void * junk )
{
	wIndex_t fileInx;
	for ( fileInx = 0; fileInx < paramFileInfo_da.cnt; fileInx++ )
		paramFileInfo(fileInx).deletedShadow = paramFileInfo(fileInx).deleted;
	wHide( paramFileW );
}


static void ParamFileCancel( wWin_p junk )
{
	wIndex_t fileInx;
	for ( fileInx = 0; fileInx < paramFileInfo_da.cnt; fileInx++ )
		paramFileInfo(fileInx).deleted = paramFileInfo(fileInx).deletedShadow;
	wHide( paramFileW );
	DoChangeNotification( CHANGE_PARAMS );
}


static void ParamFilesChange( long changes )
{
#ifdef LATER
	int fileInx;
	wIndex_t listInx;
	if ((changes&CHANGE_PARAMS) == 0 ||
		paramFileW == NULL || !wWinIsVisible(paramFileW) )
		return;
	wControlShow( (wControl_p)paramFileL, FALSE );
	listInx = wListGetIndex(paramFileL);
	wListClear( paramFileL );
	for ( fileInx = 0; fileInx < paramFileInfo_da.cnt; fileInx++ ) {
		if (paramFileInfo(fileInx).valid) {
			strcpy( message, ((!paramFileSel) && paramFileInfo(fileInx).contents)?
						paramFileInfo(fileInx).contents:
						paramFileInfo(fileInx).name );
			wListAddValue( paramFileL, message, (paramFileInfo(fileInx).deleted)?mtbox_bm:chkbox_bm, (void*)fileInx );
		}
	}
	wListSetIndex( paramFileL, listInx );
	wControlShow( (wControl_p)paramFileL, TRUE );
#endif
}


static void ParamFileDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	switch (inx) {
	case I_PRMFILLIST:
		UpdateParamFileButton( (wIndex_t)(long)wListGetItemContext(paramFileL,wListGetIndex(paramFileL)) );
		break;
	case I_PRMFILTOGGLE:
		ParamFileLoadList();
		break;
	}
}


#include "bitmaps/mtbox.xbm"
#include "bitmaps/chkbox.xbm"
static void DoParamFiles( void * junk )
{
	wIndex_t listInx;
	void * data;

	if (paramFileW == NULL) {
		const char * dir;
		dir = wPrefGetString( "file", "paramdir" );
		if (dir != NULL)
			strcpy( curParamDir, dir );
		else
			strcpy( curParamDir, libDir );
		mtbox_bm = wIconCreateBitMap( mtbox_width, mtbox_height, mtbox_bits, drawColorBlack );
		chkbox_bm = wIconCreateBitMap( chkbox_width, chkbox_height, chkbox_bits, drawColorBlack );
		paramFileW = ParamCreateDialog( &paramFilePG, MakeWindowTitle(_("Parameter Files")), _("Ok"), ParamFileOk, ParamFileCancel, TRUE, NULL, 0, ParamFileDlgUpdate );
		paramFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Load Parameters"), _("Parameter files|*.xtp"), LoadParamFile, NULL );
		ParamFileLoadList();
	}
	ParamLoadControls( &paramFilePG );
	ParamGroupRecord( &paramFilePG );
	if ((listInx = wListGetValues( paramFileL, NULL, 0, NULL, &data ))>=0)
		UpdateParamFileButton( (wIndex_t)(long)data );
	ParamFileLoadList();
	wShow( paramFileW );
}


EXPORT addButtonCallBack_t ParamFilesInit( void )
{
	BOOL_T initted = FALSE;
	if (!initted) {
		ParamRegister( &paramFilePG );
		RegisterChangeNotification( ParamFilesChange );
		initted = TRUE;
	}
	return &DoParamFiles;
}
