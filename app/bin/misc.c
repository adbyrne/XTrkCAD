/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/misc.c,v 1.12 2007-05-12 15:01:51 m_fischer Exp $
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

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#endif
#include <malloc.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#define R_OK (02)
#define access _access
#if _MSC_VER >1300
	#define strdup _strdup
#endif
#else
#include <sys/stat.h>
#endif
#include <stdarg.h>

#include "track.h"
#include "common.h"
#include "utility.h"
#include "draw.h"
#include "misc.h"
#include "cjoin.h"
#include "compound.h"

extern wBalloonHelp_t balloonHelp[];
#ifdef DEBUG
#define CHECK_BALLOONHELP
/*#define CHECK_UNUSED_BALLOONHELP*/
#endif
#ifdef CHECK_UNUSED_BALLOONHELP
static void ShowUnusedBalloonHelp(void);
#endif
void DoCarDlg(void);

/****************************************************************************
 *
  EXPORTED VARIABLES
 *
 */

EXPORT int foobar = 0;

EXPORT int log_error;
static int log_command;

EXPORT wWin_p mainW;

EXPORT wIndex_t changed = 0;

EXPORT char FAR message[STR_LONG_SIZE];
static char message2[STR_LONG_SIZE];

EXPORT REGION_T curRegion = 0;

EXPORT long paramVersion = -1;

EXPORT coOrd zero = { 0.0, 0.0 };

EXPORT wBool_t extraButtons = FALSE;

EXPORT wButton_p undoB;
EXPORT wButton_p redoB;

EXPORT wIndex_t checkPtMark = 0;

EXPORT wMenu_p demoM;
EXPORT wMenu_p popup1M, popup2M;
EXPORT wMenu_p popup1aM, popup2aM;


static wIndex_t curCommand = 0;
EXPORT void * commandContext;
EXPORT wIndex_t cmdGroup;
EXPORT wIndex_t joinCmdInx;
EXPORT wIndex_t modifyCmdInx;
EXPORT long rightClickMode = 0;
EXPORT DIST_T easementVal = 0.0;
EXPORT DIST_T easeR = 0.0;
EXPORT DIST_T easeL = 0.0;
EXPORT coOrd cmdMenuPos;

EXPORT wPos_t DlgSepLeft = 3;
EXPORT wPos_t DlgSepMid = 10;
EXPORT wPos_t DlgSepRight = 3;
EXPORT wPos_t DlgSepTop = 3;
EXPORT wPos_t DlgSepBottom = 3;
EXPORT wPos_t DlgSepNarrow = 4;
EXPORT wPos_t DlgSepWide = 14;
EXPORT wPos_t DlgSepFrmLeft = 4;
EXPORT wPos_t DlgSepFrmRight = 4;
EXPORT wPos_t DlgSepFrmTop = 4;
EXPORT wPos_t DlgSepFrmBottom = 4;

static int verbose = 0;

static wMenuList_p winList_mi;
static wWin_p aboutW;
static BOOL_T inMainW = TRUE;

static long stickySet;
static long stickyCnt = 0;
static char * stickyLabels[33];
#define TOOLBARSET_INIT				(0xFFFF)
EXPORT long toolbarSet = TOOLBARSET_INIT;
EXPORT wPos_t toolbarHeight = 0;
static wPos_t toolbarWidth = 0;

static wMenuList_p messageList_ml;
static BOOL_T messageListEmpty = TRUE;
#define MESSAGE_LIST_EMPTY			"No Messages"

#define NUM_FILELIST (5)

extern long curTurnoutEp;
static wIndex_t printCmdInx;
static wIndex_t gridCmdInx;
static paramData_t menuPLs[101] = {
		{ PD_LONG, &toolbarSet, "toolbarset" },
		{ PD_LONG, &curTurnoutEp, "cur-turnout-ep" } };
static paramGroup_t menuPG = { "misc", PGO_RECORD, menuPLs, 2 };

/****************************************************************************
 *
 * LOCAL UTILITIES
 *
 */

EXPORT long totalMallocs = 0;
EXPORT long totalMalloced = 0;
EXPORT long totalRealloced = 0;
EXPORT long totalReallocs = 0;
EXPORT long totalFreeed = 0;
EXPORT long totalFrees = 0;

static unsigned long guard0 =  0xDEADBEEF;
static unsigned long guard1 =  0xAF00BA8A;
static int log_malloc;

EXPORT void * MyMalloc ( long size )
{
	void * p;
	totalMallocs++;
	totalMalloced += size;
#if defined(WINDOWS) && ! defined(WIN32)
	if ( size > 65500L ) {
		AbortProg( "mallocing > 65500 bytes" );
	}
#endif
	p = malloc( (size_t)size + sizeof (size_t) + 2 * sizeof (unsigned long) );
	if (p == NULL)
		AbortProg( "No memory" );
		
LOG1( log_malloc, ( "Malloc(%ld) = %lx (%lx-%lx)\n", size,
				(long)((char*)p+sizeof (size_t) + sizeof (unsigned long)),
				(long)p,
				(long)((char*)p+size+sizeof (size_t) + 2 * sizeof(unsigned long)) ));
	*(size_t*)p = (size_t)size;
	p = (char*)p + sizeof (size_t);
	*(unsigned long*)p = guard0;
	p = (char*)p + sizeof (unsigned long);
	*(unsigned long*)((char*)p+size) = guard1;
	memset( p, 0, (size_t)size );
	return p;
}

EXPORT void * MyRealloc( void * old, long size )
{
	size_t oldSize;
	void * new;
	if (old==NULL)
		return MyMalloc( size );
	totalReallocs++;
	totalRealloced += size;
#if defined(WINDOWS) && ! defined(WIN32)
	if ( size > 65500L ) {
		AbortProg( "reallocing > 65500 bytes" );
	}
#endif
	if ( *(unsigned long*)((char*)old - sizeof (unsigned long)) != guard0 ) {
		AbortProg( "Guard0 is hosed" );
	}
	oldSize = *(size_t*)((char*)old - sizeof (unsigned long) - sizeof (size_t));
	if ( *(unsigned long*)((char*)old + oldSize) != guard1 ) {
		AbortProg( "Guard1 is hosed" );
	}
LOG1( log_malloc, ("Realloc(%lx,%ld) was %d\n", (long)old, size, oldSize ) )
	if ((long)oldSize == size) {
		return old;
	}
	if (size == 0) {
		free( (char*)old - sizeof *(long*)0 - sizeof *(size_t*)0 );
		return NULL;
	}
	new = MyMalloc( size );
	if (new == NULL && size)
		AbortProg( "No memory" );
	memcpy( new, old, min((size_t)size, oldSize) );
	MyFree(old);
	return new;
}


EXPORT void MyFree( void * ptr )
{
	size_t oldSize;
	totalFrees++;
	if (ptr) {
		if ( *(unsigned long*)((char*)ptr - sizeof (unsigned long)) != guard0 ) {
			AbortProg( "Guard0 is hosed" );
		}
		oldSize = *(size_t*)((char*)ptr - sizeof (unsigned long) - sizeof (size_t));
		if ( *(unsigned long*)((char*)ptr + oldSize) != guard1 ) {
			AbortProg( "Guard1 is hosed" );
		}
LOG1( log_malloc, ("Free %d at %lx (%lx-%lx)\n", oldSize, (long)ptr,
				(long)((char*)ptr-sizeof *(size_t*)0-sizeof *(long*)0),
				(long)((char*)ptr+oldSize+sizeof *(long*)0)) )
		totalFreeed += oldSize;
		free( (char*)ptr - sizeof *(long*)0 - sizeof *(size_t*)0 );
	}
}


EXPORT void * memdup( void * src, size_t size )
{
	void * p;
	p = MyMalloc( size );
	if (p == NULL)
		AbortProg( "No memory" );
	memcpy( p, src, size );
	return p;
}


EXPORT char * MyStrdup( const char * str )
{
	char * ret;
	ret = (char*)MyMalloc( strlen( str ) + 1 );
	strcpy( ret, str );
	return ret;
}


EXPORT void AbortProg(
		char * msg,
		... )
{
	static BOOL_T abort2 = FALSE;
	int rc;
	va_list ap;
	va_start( ap, msg );
	vsprintf( message, msg, ap );
	va_end( ap );
	if (abort2) {
		wNotice( message, "ABORT", NULL );
	} else {
		strcat( message, "\nDo you want to save your layout?" );
		rc = wNotice( message, "Ok", "ABORT" );
		if (rc) {
			DoSaveAs( (doSaveCallBack_p)abort );
		} else {
			abort();
		}
	}
}


EXPORT char * Strcpytrimed( char * dst, char * src, BOOL_T double_quotes )
{
	char * cp;
	while (*src && isspace(*src) ) src++;
	if (!*src)
		return dst;
	cp = src+strlen(src)-1;
	while ( cp>src && isspace(*cp) ) cp--;
	while ( src<=cp ) {
		if (*src == '"' && double_quotes)
			*dst++ = '"';
		*dst++ = *src++;
	}
	*dst = '\0';
	return dst;
}


EXPORT char * BuildTrimedTitle( char * cp, char * sep, char * mfg, char * desc, char * partno )
{
	cp = Strcpytrimed( cp, mfg, FALSE );
	strcpy( cp, sep );
	cp += strlen(cp);
	cp = Strcpytrimed( cp, desc, FALSE );
	strcpy( cp, sep );
	cp += strlen(cp);
	cp = Strcpytrimed( cp, partno, FALSE );
	return cp;
}


static void ShowMessageHelp( int index, const char * label, void * data )
{
	char msgKey[STR_SIZE], *cp, *msgSrc;
	msgSrc = (char*)data;
	if (!msgSrc)
		return;
	cp = strchr( msgSrc, '\t' );
	if (cp==NULL) {
		sprintf( msgKey, "No help for %s", msgSrc );
		wNotice( msgKey, "Ok", NULL );
		return;
	}
	memcpy( msgKey, msgSrc, cp-msgSrc );
	msgKey[cp-msgSrc] = 0;
	wHelp( msgKey );
}


static char * ParseMessage(
		char *msgSrc )
{
	char *cp1=NULL, *cp2=NULL;
	static char shortMsg[STR_SIZE];
	cp1 = strchr( msgSrc, '\t' );
	if (cp1) {
		cp2 = strchr( cp1+1, '\t' );
		if (cp2) {
			cp1++;
			memcpy( shortMsg, cp1, cp2-cp1 );
			shortMsg[cp2-cp1] = 0;
			cp1 = shortMsg;
			cp2++;
		} else {
			cp1++;
			cp2 = cp1;
		}
		if (messageListEmpty) {
			wMenuListDelete( messageList_ml, MESSAGE_LIST_EMPTY );
			messageListEmpty = FALSE;
		}
		wMenuListAdd( messageList_ml, 0, cp1, msgSrc );
		return cp2;
	} else {
		return msgSrc;
	}
}


EXPORT void InfoMessage( char * format, ... )
{
	va_list ap;
	va_start( ap, format );
	format = ParseMessage( format );
	vsprintf( message2, format, ap );
	va_end( ap );
	/*InfoSubstituteControl( NULL, NULL );*/
	if (inError)
		return;
	SetMessage( message2 );
}


EXPORT void ErrorMessage( char * format, ... )
{
	va_list ap;
	va_start( ap, format );
	format = ParseMessage( format );
	vsprintf( message2, format, ap );
	va_end( ap );
	InfoSubstituteControls( NULL, NULL );
	SetMessage( message2 );
	wBeep();
	inError = TRUE;
}


EXPORT int NoticeMessage( char * format, char * yes, char * no, ... )
{
	va_list ap;
	va_start( ap, no );
	format = ParseMessage( format );
	vsprintf( message2, format, ap );
	va_end( ap );
	return wNotice( message2, yes, no );
}


EXPORT int NoticeMessage2( int playbackRC, char * format, char * yes, char * no, ... )
{
	va_list ap;
	if ( inPlayback )
		return playbackRC;
	va_start( ap, no );
	format = ParseMessage( format );
	vsprintf( message2, format, ap );
	va_end( ap );
	return wNotice( message2, yes, no );
}

/*****************************************************************************
 *
 * MAIN BUTTON HANDLERS
 *
 */


EXPORT void Confirm( char * label2, doSaveCallBack_p after )
{
	int rc;
	if (changed) {
		 rc = wNotice3( 
						"Save changes to the layout design before closing?\n\n"
						"If you don't save now, your unsaved changes will be discarded.",
						"&Save", "&Cancel", "&Don't Save" );
		if (rc == 1) {
			DoSave( after );
			return;
		} else if (rc == 0) {
			return;
		}
	}
	after();
	return;
}

static void ChkLoad( void )
{
	Confirm("Load", DoLoad);
}

static void ChkRevert( void )
{
	int rc;
	
	if( changed) {
		rc = wNotice( "Do you want to return to the last saved state?\n\n"
									"Revert will cause all changes done since last save to be lost.",
									"&Revert", "&Cancel" );
		if( rc ) {
			/* load the file */
			LoadTracks( curPathName, curFileName, NULL );
		} 								
	}
}


static char * fileListPathName;
static void AfterFileList( void )
{
	DoFileList( 0, NULL, fileListPathName );
}

static void ChkFileList( int index, const char * label, void * data )
{
	fileListPathName = (char*)data;
	Confirm( "Load", AfterFileList );
}


EXPORT void SaveState( void )
{
	wPos_t width, height;
	const char * fileName;
	void * pathName;
	char file[6];
	int inx;

	wWinGetSize( mainW, &width, &height );
	wPrefSetInteger( "draw", "mainwidth", width );
	wPrefSetInteger( "draw", "mainheight", height );
	RememberParamFiles();
	ParamUpdatePrefs();
	if ( fileList_ml ) {
		strcpy( file, "file" );
		file[5] = 0;
		for ( inx=0; inx<NUM_FILELIST; inx++ ) {
			fileName = wMenuListGet( fileList_ml, inx, &pathName );
			if (fileName) {
				file[4] = '0'+inx;
				sprintf( message, "%s", (char*)pathName );
				wPrefSetString( "filelist", file, message );
			}
		}
	}
	wPrefFlush();
	LogClose();
}

/*
 * Clean up befor quitting
 */
static int quitting;
static void DoQuitAfter( void )
{
	changed = 0;
	SaveState();

	CleanupFiles();
		
	quitting = TRUE;
}

static void DoQuit( void )
{
	Confirm("Quit", DoQuitAfter );
	if ( quitting ) {
#ifdef CHECK_UNUSED_BALLOONHELP
		ShowUnusedBalloonHelp();
#endif
		LogClose();
		wExit(0);
	}
}


static void DoClearAfter( void )
{
	ClearTracks();
	checkPtMark = 0;
	Reset();
	DoChangeNotification( CHANGE_MAIN|CHANGE_MAP );
	EnableCommands();
	curPathName[0] = '\0';
	curFileName = curPathName;
	SetWindowTitle();
}

static void DoClear( void )
{
	Confirm("Clear", DoClearAfter);
}


static void DoShowWindow(
		int index,
		const char * name,
		void * data )
{
	if (data == mapW) {
		if (mapVisible == FALSE) {
			mapVisible = TRUE;
			DoChangeNotification( CHANGE_MAP );
		}
		mapVisible = TRUE;
	}
	wWinShow( (wWin_p)data, TRUE );
}


static dynArr_t demoWindows_da;
#define demoWindows(N) DYNARR_N( wWin_p, demoWindows_da, N )

EXPORT void wShow(
		wWin_p win )
{
	int inx;
	if (inPlayback && win != demoW) {
		wWinSetBusy( win, TRUE );
		for ( inx=0; inx<demoWindows_da.cnt; inx++ )
			if ( demoWindows(inx) == win )
				break;
		if ( inx >= demoWindows_da.cnt ) {
			for ( inx=0; inx<demoWindows_da.cnt; inx++ )
				if ( demoWindows(inx) == NULL )
					 break;
			if ( inx >= demoWindows_da.cnt ) {
				DYNARR_APPEND( wWin_p, demoWindows_da, 10 );
				inx = demoWindows_da.cnt-1;
			}
			demoWindows(inx) = win;
		}
	}
	if (win != mainW)
		wMenuListAdd( winList_mi, -1, wWinGetTitle(win), win );
	wWinShow( win, TRUE );
}


EXPORT void wHide(
		wWin_p win )
{
	int inx;
	wWinShow( win, FALSE );
	wWinSetBusy( win, FALSE );
	if ( inMainW && win == aboutW )
		return;
	wMenuListDelete( winList_mi, wWinGetTitle(win) );
	if ( inPlayback )
		for ( inx=0; inx<demoWindows_da.cnt; inx++ )
			if ( demoWindows(inx) == win )
				demoWindows(inx) = NULL;
}


EXPORT void CloseDemoWindows( void )
{
	int inx;
	for ( inx=0; inx<demoWindows_da.cnt; inx++ )
		if ( demoWindows(inx) != NULL )
			wHide( demoWindows(inx) );
	demoWindows_da.cnt = 0;
}


EXPORT void DefaultProc(
		wWin_p win,
		winProcEvent e,
		void * data )
{
	switch( e ) {
	case wClose_e:
		wMenuListDelete( winList_mi, wWinGetTitle(win) );
		if (data != NULL)
			ConfirmReset( FALSE );
		wWinDoCancel( win );
		break;
	default:
		break;
	}
}


static void NextWindow( void )
{
}

EXPORT void SelectFont( void )
{
	wSelectFont("XTrkCad Font");
}

/*****************************************************************************
 *
 * COMMAND
 *
 */

#define COMMAND_MAX (100)
#define BUTTON_MAX (100)
#define NUM_CMDMENUS (4)

#ifdef LATER
static struct {
		addButtonCallBack_t actionProc;
		procCommand_t cmdProc;
		char * helpStr;
		wControl_p control;
		char * labelStr;
		int reqLevel;
		wBool_t enabled;
		wPos_t x, y;
		long options;
		long stickyMask;
		int group;
		long acclKey;
		wMenuPush_p menu[NUM_CMDMENUS];
		void * context;
		} commandList[COMMAND_MAX];
#endif

static struct {
		wControl_p control;
		wBool_t enabled;
		wPos_t x, y;
		long options;
		int group;
		wIndex_t cmdInx;
		} buttonList[BUTTON_MAX];
static int buttonCnt = 0;

static struct {
		procCommand_t cmdProc;
		char * helpKey;
		wIndex_t buttInx;
		char * labelStr;
		wIcon_p icon;
		int reqLevel;
		wBool_t enabled;
		long options;
		long stickyMask;
		long acclKey;
		wMenuPush_p menu[NUM_CMDMENUS];
		void * context;
		} commandList[COMMAND_MAX];
static int commandCnt = 0;


#ifdef CHECK_UNUSED_BALLOONHELP
int * balloonHelpCnts;
#endif

EXPORT const char * GetBalloonHelpStr( char * helpKey )
{
	wBalloonHelp_t * bh;
#ifdef CHECK_UNUSED_BALLOONHELP
	if ( balloonHelpCnts == NULL ) {
		for ( bh=balloonHelp; bh->name; bh++ );
		balloonHelpCnts = (int*)malloc( (sizeof *(int*)0) * (bh-balloonHelp) );
		memset( balloonHelpCnts, 0, (sizeof *(int*)0) * (bh-balloonHelp) );
	}
#endif
	for ( bh=balloonHelp; bh->name; bh++ ) {
		if ( strcmp( bh->name, helpKey ) == 0 ) {
#ifdef CHECK_UNUSED_BALLOONHELP
			balloonHelpCnts[(bh-balloonHelp)]++;
#endif
			return bh->value;
		}
	}
#ifdef CHECK_BALLOONHELP
fprintf( stderr, "No balloon help for %s\n", helpKey );
#endif
	return "No Help";
}


#ifdef CHECK_UNUSED_BALLOONHELP
static void ShowUnusedBalloonHelp( void )
{
	int cnt;
	for ( cnt=0; balloonHelp[cnt].name; cnt++ )
		if ( balloonHelpCnts[cnt] == 0 )
			fprintf( stderr, "unused BH %s\n", balloonHelp[cnt].name );
}
#endif


EXPORT void EnableCommands( void )
{
	int inx, minx;
	wBool_t enable;

LOG( log_command, 5, ( "COMMAND enable S%d M%d\n", selectedTrackCount, programMode ) )
	for ( inx=0; inx<commandCnt; inx++ ) {
		if (commandList[inx].buttInx) {
			if ( (commandList[inx].options & IC_SELECTED) &&
				 selectedTrackCount <= 0 )
				enable = FALSE;
			else if ( (programMode==MODE_TRAIN&&(commandList[inx].options&(IC_MODETRAIN_TOO|IC_MODETRAIN_ONLY))==0) ||
					  (programMode!=MODE_TRAIN&&(commandList[inx].options&IC_MODETRAIN_ONLY)!=0) )
				enable = FALSE;
			else
				enable = TRUE;
			if ( commandList[inx].enabled != enable ) {
				if ( commandList[inx].buttInx >= 0 )
					wControlActive( buttonList[commandList[inx].buttInx].control, enable );
				for ( minx=0; minx<NUM_CMDMENUS; minx++ )
					if (commandList[inx].menu[minx])
						wMenuPushEnable( commandList[inx].menu[minx], enable );
				commandList[inx].enabled = enable;
			}
		}
	}

	for ( inx=0; inx<menuPG.paramCnt; inx++ ) {
		if ( menuPLs[inx].control == NULL )
			continue;
		if ( (menuPLs[inx].option & IC_SELECTED) &&
			 selectedTrackCount <= 0 )
			enable = FALSE;
		else if ( (programMode==MODE_TRAIN&&(menuPLs[inx].option&(IC_MODETRAIN_TOO|IC_MODETRAIN_ONLY))==0) ||
				  (programMode!=MODE_TRAIN&&(menuPLs[inx].option&IC_MODETRAIN_ONLY)!=0) )
			enable = FALSE;
		else
			enable = TRUE;
		wMenuPushEnable( (wMenuPush_p)menuPLs[inx].control, enable );
	}
		
	for ( inx=0; inx<buttonCnt; inx++ ) {
		if ( buttonList[inx].cmdInx < 0 && (buttonList[inx].options&IC_SELECTED) )
			wControlActive( buttonList[inx].control, selectedTrackCount>0 );
	 }
}


EXPORT void Reset( void )
{
	if (recordF) {
		fprintf( recordF, "RESET\n" );
		fflush( recordF );
	}
LOG( log_command, 2, ( "COMMAND CANCEL %s\n", commandList[curCommand].helpKey ) )
	commandList[curCommand].cmdProc( C_CANCEL, zero );
	if ( commandList[curCommand].buttInx>=0 )
		wButtonSetBusy( (wButton_p)buttonList[commandList[curCommand].buttInx].control, FALSE );
	curCommand = (preSelect?selectCmdInx:describeCmdInx);
	commandContext = commandList[curCommand].context;
	if ( commandList[curCommand].buttInx >= 0 )
		wButtonSetBusy( (wButton_p)buttonList[commandList[curCommand].buttInx].control, TRUE );
	tempSegs_da.cnt = 0;
	if (checkPtInterval > 0 &&
		changed >= checkPtMark+(wIndex_t)checkPtInterval &&
		!inPlayback ) {
		DoCheckPoint();
		checkPtMark = changed;
	} 
	EnableCommands();
	ResetMouseState();
LOG( log_command, 1, ( "COMMAND RESET %s\n", commandList[curCommand].helpKey ) )
	(void)commandList[curCommand].cmdProc( C_START, zero );
}


static BOOL_T CheckClick(
		wAction_t *action,
		coOrd *pos,
		BOOL_T checkLeft,
		BOOL_T checkRight )
{
	static long time0;
	static coOrd pos0;
	long time1;
	long timeDelta;
	DIST_T distDelta;

	switch (*action) {
	case C_DOWN:
		if (!checkLeft)
			return TRUE;
		time0 = wGetTimer() - adjTimer;
		pos0 = *pos;
		return FALSE;
	case C_MOVE:
		if (!checkLeft)
			return TRUE;
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance( *pos, pos0 );
			if ( timeDelta > dragTimeout ||
				 distDelta > dragDistance ) {
				time0 = 0;
				*pos = pos0;
				*action = C_DOWN;
			} else {
				return FALSE;
			}
		}
		break;
	case C_UP:
		if (!checkLeft)
			return TRUE;
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance( *pos, pos0 );
			time0 = 0;
			*action = C_LCLICK;
		}
		break;
	case C_RDOWN:
		if (!checkRight)
			return TRUE;
		time0 = wGetTimer() - adjTimer;
		pos0 = *pos;
		return FALSE;
	case C_RMOVE:
		if (!checkRight)
			return TRUE;
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance( *pos, pos0 );
			if ( timeDelta > dragTimeout ||
				 distDelta > dragDistance ) {
				time0 = 0;
				*pos = pos0;
				*action = C_RDOWN;
			} else {
				return FALSE;
			}
		}
		break;
	case C_RUP:
		if (!checkRight)
			return TRUE;
		if (time0 != 0) {
			time0 = 0;
			*action = C_RCLICK;
		}
		break;
	}
	return TRUE;
}


EXPORT wBool_t DoCurCommand( wAction_t action, coOrd pos )
{
	wAction_t rc;
	int mode;
	
	if ( action == wActionMove && (commandList[curCommand].options & IC_WANT_MOVE) == 0 )
		return C_CONTINUE;

	if ( !CheckClick( &action, &pos,
		 (int)(commandList[curCommand].options & IC_LCLICK), TRUE ) )
		return C_CONTINUE;

	if ( action == C_RCLICK && (commandList[curCommand].options&IC_RCLICK)==0 ) {
		if ( !inPlayback ) {
			mode = MyGetKeyState();
			if ( ( mode & (~WKEY_SHIFT) ) != 0 ) {
				wBeep();
				return C_CONTINUE;
			}
			if ( ((mode&WKEY_SHIFT) == 0) == (rightClickMode==0) ) {
				if ( selectedTrackCount > 0 ) {
					if (commandList[curCommand].options & IC_CMDMENU) {
					}
					wMenuPopupShow( popup2M );
				} else {
					wMenuPopupShow( popup1M );
				}
				return C_CONTINUE;
			} else if ( (commandList[curCommand].options & IC_CMDMENU) ) {
				cmdMenuPos = pos;
				action = C_CMDMENU;
			} else {
				wBeep();
				return C_CONTINUE;
			}
		} else {
			return C_CONTINUE;
		}
	}

LOG( log_command, 2, ( "COMMAND MOUSE %s %d @ %0.3f %0.3f\n", commandList[curCommand].helpKey, (int)action, pos.x, pos.y ) )
	rc = commandList[curCommand].cmdProc( action, pos );
LOG( log_command, 4, ( "    COMMAND returns %d\n", rc ) )
	if ( (rc == C_TERMINATE || rc == C_INFO) &&
		 (commandList[curCommand].options & IC_STICKY) &&
		 (commandList[curCommand].stickyMask & stickySet) ) {
		tempSegs_da.cnt = 0;
		UpdateAllElevations();
		if (commandList[curCommand].options & IC_NORESTART) {
			return C_CONTINUE;
		}
LOG( log_command, 1, ( "COMMAND START %s\n", commandList[curCommand].helpKey ) )
	rc = commandList[curCommand].cmdProc( C_START, pos );
LOG( log_command, 4, ( "    COMMAND returns %d\n", rc ) )
		switch( rc ) {
		case C_CONTINUE:
			break;
		case C_ERROR:
			Reset();
#ifdef VERBOSE
			lprintf( "Start returns Error");
#endif
			break;
		case C_TERMINATE:
			InfoMessage( "" );
		case C_INFO:
			Reset();
			break;
		}
	}
	return rc;
}


EXPORT void ConfirmReset( BOOL_T retry )
{
	wAction_t rc;
	if (curCommand != describeCmdInx && curCommand != selectCmdInx ) {
LOG( log_command, 3, ( "COMMAND CONFIRM %s\n", commandList[curCommand].helpKey ) )
		rc = commandList[curCommand].cmdProc( C_CONFIRM, zero );
LOG( log_command, 4, ( "    COMMAND returns %d\n", rc ) )
		if ( rc == C_ERROR ) {
			if (retry)
				rc = wNotice3(
				"Cancelling the current command will undo the changes\n"
				"you are currently making. Do you want to update?",
				"Yes", "No", "Cancel" );
			else
				rc = wNotice(
				"Cancelling the current command will undo the changes\n"
				"you are currently making. Do you want to update?",
				"Yes", "No" );
			if (rc == 1) {
LOG( log_command, 3, ( "COMMAND OK %s\n", commandList[curCommand].helpKey ) )
				commandList[curCommand].cmdProc( C_OK, zero );
				return;
			} else if (rc == -1) {
				return;
			}
		} else if ( rc == C_TERMINATE ) {
			return;
		}
	}
	Reset();
	if (retry) {
		/* because user pressed esc */
		SetAllTrackSelect( FALSE );
	}
LOG( log_command, 1, ( "COMMAND RESET %s\n", commandList[curCommand].helpKey ) )
	commandList[curCommand].cmdProc( C_START, zero );
}


EXPORT void ResetIfNotSticky( void )
{
	if ( (commandList[curCommand].options & IC_STICKY) == 0 ||
		 (commandList[curCommand].stickyMask & stickySet) == 0 )
		Reset();
}


EXPORT void DoCommandB(
		void * data )
{
	wIndex_t inx = (wIndex_t)(long)data;
	STATUS_T rc;
	static coOrd pos = {0,0};
	static int inDoCommandB = FALSE;
	wIndex_t buttInx;

	if (inDoCommandB)
		return;
	inDoCommandB = TRUE;

	if (inx < 0 || inx >= commandCnt) {
		ASSERT( FALSE );
		inDoCommandB = FALSE;
		return;
	}

	if ( (!inPlayback) && (!commandList[inx].enabled) ) {
		ErrorMessage( MSG_COMMAND_DISABLED );
		inx = describeCmdInx;
	}

	InfoMessage( "" );
	if (curCommand != selectCmdInx ) {
LOG( log_command, 3, ( "COMMAND FINISH %s\n", commandList[curCommand].helpKey ) )
		rc = commandList[curCommand].cmdProc( C_FINISH, zero );
LOG( log_command, 3, ( "COMMAND CONFIRM %s\n", commandList[curCommand].helpKey ) )
		rc = commandList[curCommand].cmdProc( C_CONFIRM, zero );
LOG( log_command, 4, ( "    COMMAND returns %d\n", rc ) )
		if ( rc == C_ERROR ) {
			rc = wNotice3(
				"Cancelling the current command will undo the changes\n"
				"you are currently making. Do you want to update?",
				"Yes", "No", "Cancel" );
			if (rc == 1)
				commandList[curCommand].cmdProc( C_OK, zero );
			else if (rc == -1) {
				inDoCommandB = FALSE;
				return;
			}
		}
LOG( log_command, 3, ( "COMMAND CANCEL %s\n", commandList[curCommand].helpKey ) )
		commandList[curCommand].cmdProc( C_CANCEL, pos );
		tempSegs_da.cnt = 0;
	}
	if (commandList[curCommand].buttInx>=0)
		wButtonSetBusy( (wButton_p)buttonList[commandList[curCommand].buttInx].control, FALSE );

	if (recordF) {
		fprintf( recordF, "COMMAND %s\n", commandList[inx].helpKey+3 );
		fflush( recordF );
	}

	curCommand = inx;
	commandContext = commandList[curCommand].context;
	if ( (buttInx=commandList[curCommand].buttInx) >= 0 ) {
		if ( buttonList[buttInx].cmdInx != curCommand ) {
			wButtonSetLabel( (wButton_p)buttonList[buttInx].control, (char*)commandList[curCommand].icon );
			wControlSetHelp( buttonList[buttInx].control, GetBalloonHelpStr(commandList[curCommand].helpKey) );
			wControlSetContext( buttonList[buttInx].control, (void*)curCommand );
			buttonList[buttInx].cmdInx = curCommand;
		}
		wButtonSetBusy( (wButton_p)buttonList[commandList[curCommand].buttInx].control, TRUE );
	}
LOG( log_command, 1, ( "COMMAND START %s\n", commandList[curCommand].helpKey ) )
	rc = commandList[curCommand].cmdProc( C_START, pos );
LOG( log_command, 4, ( "    COMMAND returns %d\n", rc ) )
	switch( rc ) {
	case C_CONTINUE:
		break;
	case C_ERROR:
		Reset();
#ifdef VERBOSE
		lprintf( "Start returns Error");
#endif
		break;
	case C_TERMINATE:
	case C_INFO:
		if (rc == C_TERMINATE)
			InfoMessage( "" );
		Reset();
		break;
	}
	inDoCommandB = FALSE;
}


static void DoCommandBIndirect( void * cmdInxP )
{
	wIndex_t cmdInx;
	cmdInx = *(wIndex_t*)cmdInxP;
	DoCommandB( (void*)cmdInx );
}


EXPORT void LayoutSetPos(
		wIndex_t inx )
{
	wPos_t w, h;
	static wPos_t toolbarRowHeight = 0;
	static wPos_t width;
	static int lastGroup;
	static wPos_t gap;
	static int layerButtCnt;
	int currGroup;

	if ( inx == 0 ) {
		lastGroup = 0;
		wWinGetSize( mainW, &width, &h );
		gap = 5;
		toolbarWidth = width+5;
		layerButtCnt = 0;
		toolbarHeight = 0;
	}

	if (buttonList[inx].control) {
		if ( toolbarRowHeight <= 0 )
			toolbarRowHeight = wControlGetHeight( buttonList[inx].control );

		currGroup = buttonList[inx].group & ~BG_BIGGAP;
		if ( currGroup != lastGroup && (buttonList[inx].group&BG_BIGGAP) ) {
				gap = 15;
		}
		if ((toolbarSet & (1<<currGroup)) &&
				(programMode!=MODE_TRAIN||(buttonList[inx].options&(IC_MODETRAIN_TOO|IC_MODETRAIN_ONLY))) &&
				(programMode==MODE_TRAIN||(buttonList[inx].options&IC_MODETRAIN_ONLY)==0) &&
				((buttonList[inx].group&~BG_BIGGAP) != BG_LAYER ||
				  layerButtCnt++ <= layerCount) ) {
				if (currGroup != lastGroup) {
					toolbarWidth += gap;
					lastGroup = currGroup;
					gap = 5;
				}
				w = wControlGetWidth( buttonList[inx].control );
				h = wControlGetHeight( buttonList[inx].control );
				if ( inx<buttonCnt-1 && (buttonList[inx+1].options&IC_ABUT) )
					w += wControlGetWidth( buttonList[inx+1].control );
				if (toolbarWidth+w>width) {
					toolbarWidth = 0;
					toolbarHeight += h + 5;
				}
				wControlSetPos( buttonList[inx].control, toolbarWidth, toolbarHeight-(h+5) );
				buttonList[inx].x = toolbarWidth;
				buttonList[inx].y = toolbarHeight-(h+5);
				toolbarWidth += wControlGetWidth( buttonList[inx].control );
				wControlShow( buttonList[inx].control, TRUE );
		} else {
				wControlShow( buttonList[inx].control, FALSE );
		}
	}
}


EXPORT void LayoutToolBar( void )
{
	int inx;

	for (inx = 0; inx<buttonCnt; inx++) {
		LayoutSetPos( inx );
	}
	if (toolbarSet&(1<<BG_HOTBAR)) {
		LayoutHotBar();
	} else {
		HideHotBar();
	}
}


static void ToolbarChange( long changes )
{
	if ( (changes&CHANGE_TOOLBAR) ) {
		/*if ( !(changes&CHANGE_MAIN) )*/
			MainProc( mainW, wResize_e, NULL );
		/*else
			LayoutToolBar();*/
	}
}

/***************************************************************************
 *
 *
 *
 */


EXPORT BOOL_T CommandEnabled(
		wIndex_t cmdInx )
{
	return commandList[cmdInx].enabled;
}


static wIndex_t AddCommand(
		procCommand_t cmdProc,
		char * helpKey,
		char * nameStr,
		wIcon_p icon,
		int reqLevel,
		long options,
		long acclKey,
		void * context )
{
	if (commandCnt >= COMMAND_MAX-1) {
		AbortProg("addCommand: too many commands" );
	}
	commandList[commandCnt].labelStr = MyStrdup(nameStr);
	commandList[commandCnt].helpKey = MyStrdup(helpKey);
	commandList[commandCnt].cmdProc = cmdProc;
	commandList[commandCnt].icon = icon;
	commandList[commandCnt].reqLevel = reqLevel;
	commandList[commandCnt].enabled = TRUE;
	commandList[commandCnt].options = options;
	commandList[commandCnt].acclKey = acclKey;
	commandList[commandCnt].context = context;
	commandList[commandCnt].buttInx = -1;
	commandList[commandCnt].menu[0] = NULL;
	commandList[commandCnt].menu[1] = NULL;
	commandList[commandCnt].menu[2] = NULL;
	commandList[commandCnt].menu[3] = NULL;
	commandCnt++;
	return commandCnt-1;
}

EXPORT void AddToolbarControl(
		wControl_p control,
		long options )
{
	if (buttonCnt >= COMMAND_MAX-1) {
		AbortProg("addToolbarControl: too many buttons" );
	}
	buttonList[buttonCnt].enabled = TRUE;
	buttonList[buttonCnt].options = options;
	buttonList[buttonCnt].group = cmdGroup;
	buttonList[buttonCnt].x = 0;
	buttonList[buttonCnt].y = 0;
	buttonList[buttonCnt].control = control;
	buttonList[buttonCnt].cmdInx = -1;
	LayoutSetPos( buttonCnt );
	buttonCnt++;
}


EXPORT wButton_p AddToolbarButton(
		char * helpStr,
		wIcon_p icon,
		long options,
		wButtonCallBack_p action,
		void * context )
{
	wButton_p bb;
	wIndex_t inx;

	GetBalloonHelpStr(helpStr);
	if ( context == NULL ) {
		for ( inx=0; inx<menuPG.paramCnt; inx++ ) {
			if ( action != DoCommandB && menuPLs[inx].valueP == (void*)action ) {
				context = &menuPLs[inx];
				action = ParamMenuPush;
				menuPLs[inx].context = (void*)buttonCnt;
				menuPLs[inx].option |= IC_PLAYBACK_PUSH;
				break;
			}
		}
	}
	bb = wButtonCreate( mainW, 0, 0, helpStr, (char*)icon,
				BO_ICON/*|((options&IC_CANCEL)?BB_CANCEL:0)*/, 0,
				action, context );
	AddToolbarControl( (wControl_p)bb, options );
	return bb;
}


EXPORT void PlaybackButtonMouse(
		wIndex_t buttInx )
{
	wPos_t cmdX, cmdY;

	if ( buttInx < 0 || buttInx >= buttonCnt ) return;
	if ( buttonList[buttInx].control == NULL ) return;
	cmdX = buttonList[buttInx].x+17;
	cmdY = toolbarHeight - (buttonList[buttInx].y+17) +
				   (wPos_t)(mainD.size.y/mainD.scale*mainD.dpi) + 30;
	MovePlaybackCursor( &mainD, cmdX, cmdY );
	if ( playbackTimer == 0 ) {
		wButtonSetBusy( (wButton_p)buttonList[buttInx].control, TRUE );
		wFlush();
		wPause( 500 );
		wButtonSetBusy( (wButton_p)buttonList[buttInx].control, FALSE );
		wFlush();
	}
}


#include "openbutt.xpm"
static char * buttonGroupMenuTitle;
static char * buttonGroupHelpKey;
static char * buttonGroupStickyLabel;
static wMenu_p buttonGroupPopupM;

EXPORT void ButtonGroupBegin(
		char * menuTitle,
		char * helpKey,
		char * stickyLabel )
{
	buttonGroupMenuTitle = menuTitle;
	buttonGroupHelpKey = helpKey;
	buttonGroupStickyLabel = stickyLabel;
	buttonGroupPopupM = NULL;
}

EXPORT void ButtonGroupEnd( void )
{
	buttonGroupMenuTitle = NULL;
	buttonGroupHelpKey = NULL;
	buttonGroupPopupM = NULL;
}


#ifdef LATER
EXPORT wIndex_t AddCommandControl(
		procCommand_t command,
		char * helpKey,
		char * nameStr,
		wControl_p control,
		int reqLevel,
		long options,
		long acclKey,
		void * context )
{
	wIndex_t buttInx = -1;
	wIndex_t cmdInx;
	BOOL_T newButtonGroup = FALSE;
	wMenu_p tm, p1m, p2m;
	static wIcon_p openbuttIcon = NULL;
	static wMenu_p commandsSubmenu;
	static wMenu_p popup1Submenu;
	static wMenu_p popup2Submenu;

	AddToolbarControl( control, options );

	buttonList[buttInx].cmdInx = commandCnt;
	cmdInx = AddCommand( command, helpKey, nameStr, NULL, reqLevel, options, acclKey, context );
	commandList[cmdInx].buttInx = buttInx;
	if (nameStr[0] == '\0')
		return cmdInx;
	if (commandList[cmdInx].options&IC_STICKY) {
		if ( buttonGroupPopupM==NULL || newButtonGroup ) {
			if ( stickyCnt > 32 )
				AbortProg( "stickyCnt>32" );
			stickyCnt++;
		}
		if ( buttonGroupPopupM==NULL) {
			stickyLabels[stickyCnt-1] = nameStr;
		} else {
			stickyLabels[stickyCnt-1] = buttonGroupStickyLabel;
		}
		stickyLabels[stickyCnt] = NULL;
		commandList[cmdInx].stickyMask = 1L<<(stickyCnt-1);
	}
	if ( buttonGroupPopupM ) {
		commandList[cmdInx].menu[0] = 
		wMenuPushCreate( buttonGroupPopupM, helpKey, GetBalloonHelpStr(helpKey), 0, DoCommandB, (void*)cmdInx );
		tm = commandsSubmenu;
		p1m = popup1Submenu;
		p2m = popup2Submenu;
	} else {
		tm = commandsM;
		p1m = (options&IC_POPUP2)?popup1aM:popup1M;
		p2m = (options&IC_POPUP2)?popup2aM:popup2M;
	}
	commandList[cmdInx].menu[1] = 
	wMenuPushCreate( tm, helpKey, nameStr, acclKey, DoCommandB, (void*)cmdInx );
	if ( (options & (IC_POPUP|IC_POPUP2)) ) {
		if ( !(options & IC_SELECTED) ) {
			commandList[cmdInx].menu[2] = 
			wMenuPushCreate( p1m, helpKey, nameStr, 0, DoCommandB, (void*)cmdInx );
		}
		commandList[cmdInx].menu[3] = 
		wMenuPushCreate( p2m, helpKey, nameStr, 0, DoCommandB, (void*)cmdInx );
	}

	return cmdInx;
}
#endif


EXPORT wIndex_t AddMenuButton(
		wMenu_p menu,
		procCommand_t command,
		char * helpKey,
		char * nameStr,
		wIcon_p icon,
		int reqLevel,
		long options,
		long acclKey,
		void * context )
{
	wIndex_t buttInx = -1;
	wIndex_t cmdInx;
	BOOL_T newButtonGroup = FALSE;
	wMenu_p tm, p1m, p2m;
	static wIcon_p openbuttIcon = NULL;
	static wMenu_p commandsSubmenu;
	static wMenu_p popup1Submenu;
	static wMenu_p popup2Submenu;

	if ( icon ) {
		if ( buttonGroupPopupM!=NULL ) {
			buttInx = buttonCnt-2;
		} else {
			buttInx = buttonCnt;
			AddToolbarButton( helpKey, icon, options, (wButtonCallBack_p)DoCommandB, (void*)commandCnt );
			buttonList[buttInx].cmdInx = commandCnt;
		}
		if ( buttonGroupMenuTitle!=NULL && buttonGroupPopupM==NULL ) {
			if ( openbuttIcon == NULL )
				openbuttIcon = wIconCreatePixMap(openbutt_xpm);
			buttonGroupPopupM = wMenuPopupCreate( mainW, buttonGroupMenuTitle );
			AddToolbarButton( buttonGroupHelpKey, openbuttIcon, IC_ABUT, (wButtonCallBack_p)wMenuPopupShow, (void*)buttonGroupPopupM );
			newButtonGroup = TRUE;
			commandsSubmenu = wMenuMenuCreate( menu, "", buttonGroupMenuTitle );
			popup1Submenu = wMenuMenuCreate( ((options&IC_POPUP2)?popup1aM:popup1M), "", buttonGroupMenuTitle );
			popup2Submenu = wMenuMenuCreate( ((options&IC_POPUP2)?popup2aM:popup2M), "", buttonGroupMenuTitle );
		}
	}
	cmdInx = AddCommand( command, helpKey, nameStr, icon, reqLevel, options, acclKey, context );
	commandList[cmdInx].buttInx = buttInx;
	if (nameStr[0] == '\0')
		return cmdInx;
	if (commandList[cmdInx].options&IC_STICKY) {
		if ( buttonGroupPopupM==NULL || newButtonGroup ) {
			if ( stickyCnt > 32 )
				AbortProg( "stickyCnt>32" );
			stickyCnt++;
		}
		if ( buttonGroupPopupM==NULL) {
			stickyLabels[stickyCnt-1] = nameStr;
		} else {
			stickyLabels[stickyCnt-1] = buttonGroupStickyLabel;
		}
		stickyLabels[stickyCnt] = NULL;
		commandList[cmdInx].stickyMask = 1L<<(stickyCnt-1);
	}
	if ( buttonGroupPopupM ) {
		commandList[cmdInx].menu[0] = 
		wMenuPushCreate( buttonGroupPopupM, helpKey, GetBalloonHelpStr(helpKey), 0, DoCommandB, (void*)cmdInx );
		tm = commandsSubmenu;
		p1m = popup1Submenu;
		p2m = popup2Submenu;
	} else {
		tm = menu; 
		p1m = (options&IC_POPUP2)?popup1aM:popup1M;
		p2m = (options&IC_POPUP2)?popup2aM:popup2M;
	}
	commandList[cmdInx].menu[1] = 
	wMenuPushCreate( tm, helpKey, nameStr, acclKey, DoCommandB, (void*)cmdInx );
	if ( (options & (IC_POPUP|IC_POPUP2)) ) {
		if ( !(options & IC_SELECTED) ) {
			commandList[cmdInx].menu[2] = 
			wMenuPushCreate( p1m, helpKey, nameStr, 0, DoCommandB, (void*)cmdInx );
		}
		commandList[cmdInx].menu[3] = 
		wMenuPushCreate( p2m, helpKey, nameStr, 0, DoCommandB, (void*)cmdInx );
	}

	return cmdInx;
}


EXPORT wIndex_t InitCommand(
		wMenu_p menu,
		procCommand_t command,
		char * nameStr,
		char * bits,
		int reqLevel,
		long options,
		long acclKey )
{
	char helpKey[STR_SHORT_SIZE];
	wIcon_p icon = NULL;
	if (bits)
		icon = wIconCreateBitMap( 16, 16, bits, wDrawColorBlack );
	strcpy( helpKey, "cmd" );
	strcat( helpKey, nameStr );
	return AddMenuButton( menu, command, helpKey, nameStr, icon, reqLevel, options, acclKey, NULL );
}

/*--------------------------------------------------------------------*/

EXPORT void PlaybackCommand(
		char * line,
		wIndex_t lineNum )
{
	wIndex_t inx;
	wIndex_t buttInx;
	int len1, len2;
	len1 = strlen(line+8);
	for (inx=0;inx<commandCnt;inx++) {
		len2 = strlen(commandList[inx].helpKey+3);
		if (len1 == len2 && strncmp( line+8, commandList[inx].helpKey+3, len2 ) == 0) {
			break;
		}
	}
	if (inx >= commandCnt) {
		fprintf(stderr, "Unknown playback COMMAND command %d : %s\n",
			lineNum, line );
	} else {
		wPos_t cmdX, cmdY;
		if ((buttInx=commandList[inx].buttInx)>=0) {
			cmdX = buttonList[buttInx].x+17;
			cmdY = toolbarHeight - (buttonList[buttInx].y+17) +
				   (wPos_t)(mainD.size.y/mainD.scale*mainD.dpi) + 30;
			MovePlaybackCursor( &mainD, cmdX, cmdY );
		}
		if (strcmp( line+8, "Undo") == 0) {
			if (buttInx>0 && playbackTimer == 0) {
				wButtonSetBusy( (wButton_p)buttonList[buttInx].control, TRUE );
				wFlush();
				wPause( 500 );
				wButtonSetBusy( (wButton_p)buttonList[buttInx].control, FALSE );
				wFlush();
			}
			UndoUndo();
		} else if (strcmp( line+8, "Redo") == 0) {
			if (buttInx>=0 && playbackTimer == 0) {
				wButtonSetBusy( (wButton_p)buttonList[buttInx].control, TRUE );
				wFlush();
				wPause( 500 );
				wButtonSetBusy( (wButton_p)buttonList[buttInx].control, FALSE );
				wFlush();
			}
			UndoRedo();
		} else {
			if ( buttInx>=0 &&
				 playbackTimer == 0 ) {
				wButtonSetBusy( (wButton_p)buttonList[buttInx].control, TRUE );
				wFlush();
				wPause( 500 );
				wButtonSetBusy( (wButton_p)buttonList[buttInx].control, FALSE );
				wFlush();
			}
			DoCommandB( (void*)inx );
		}
	}
}


/*--------------------------------------------------------------------*/
typedef struct {
		char * label;
		wMenu_p menu;
		} menuTrace_t, *menuTrace_p;
static dynArr_t menuTrace_da;
#define menuTrace(N) DYNARR_N( menuTrace_t, menuTrace_da, N )


static void DoMenuTrace(
		wMenu_p menu,
		const char * label,
		void * data )
{
	/*printf( "MENUTRACE: %s/%s\n", (char*)data, label );*/
	if (recordF) {
		fprintf( recordF, "MOUSE 1 %0.3f %0.3f\n", oldMarker.x, oldMarker.y );
		fprintf( recordF, "MENU %0.3f %0.3f \"%s\" \"%s\"\n", oldMarker.x, oldMarker.y, (char*)data, label );
	}
}


EXPORT wMenu_p MenuRegister( char * label )
{
	wMenu_p m;
	menuTrace_p mt;
	m = wMenuPopupCreate( mainW, label );
	DYNARR_APPEND( menuTrace_t, menuTrace_da, 10 );
	mt = &menuTrace( menuTrace_da.cnt-1 );
	mt->label = strdup(label);
	mt->menu = m;
	wMenuSetTraceCallBack( m, DoMenuTrace, mt->label );
	return m;
}


void MenuPlayback( char * line )
{
	char * menuName, * itemName;
	coOrd pos;
	wPos_t x, y;
	menuTrace_p mt;

	if (!GetArgs( line, "pqq", &pos, &menuName, &itemName ))
		return;
	for ( mt=&menuTrace(0); mt<&menuTrace(menuTrace_da.cnt); mt++ ) {
		if ( strcmp( mt->label, menuName ) == 0 ) {
			mainD.CoOrd2Pix( &mainD, pos, &x, &y );
			MovePlaybackCursor( &mainD, x, y );
			oldMarker = cmdMenuPos = pos;
			wMenuAction( mt->menu, itemName );
			return;
		}
	}
	AbortProg( "menuPlayback: %s not found", menuName );
}
/*--------------------------------------------------------------------*/


static wWin_p stickyW;

static void StickyOk( void * );
static paramData_t stickyPLs[] = {
	{   PD_TOGGLE, &stickySet, "set", 0, stickyLabels } };
static paramGroup_t stickyPG = { "sticky", PGO_RECORD, stickyPLs, sizeof stickyPLs/sizeof stickyPLs[0] };


static void StickyOk( void * junk )
{
	wHide( stickyW );
}

static void DoSticky( void )
{
	if ( !stickyW )
		stickyW = ParamCreateDialog( &stickyPG, MakeWindowTitle("Sticky Commands"), "Ok", StickyOk, NULL, TRUE, NULL, 0, NULL );
	ParamLoadControls( &stickyPG );
	wShow( stickyW );
}
/*--------------------------------------------------------------------*/

static char *AllToolbarLabels[] = {
		"Zoom Buttons",
		"Undo Buttons",
		"Easement Button",
		"SnapGrid Buttons",
		"Create Track Buttons",
		"Modify Track Buttons",
		"Describe/Select",
		"Track Group Buttons",
		"Train Group Buttons",
		"Create Misc Buttons",
		"Ruler Button",
		"Layer Buttons",
		"Hot Bar",
		NULL };
static long AllToolbarMasks[] = {
		1<<BG_ZOOM,
		1<<BG_UNDO,
		1<<BG_EASE,
		1<<BG_SNAP,
		1<<BG_TRKCRT,
		1<<BG_TRKMOD,
		1<<BG_SELECT,
		1<<BG_TRKGRP,
		1<<BG_TRAIN,
		1<<BG_MISCCRT,
		1<<BG_RULER,
		1<<BG_LAYER,
		1<<BG_HOTBAR };


static char *NonflexToolbarLabels[] = {
		"Zoom Buttons",
		"Undo Buttons",
		"SnapGrid Buttons",
		"Modify Track Buttons",
		"Describe/Select",
		"Track Group Buttons",
		"Train Group Buttons",
		"Create Misc Buttons",
		"Ruler Button",
		"Layer Buttons",
		"Hot Bar",
		NULL };
static long NonflexToolbarMasks[] = {
		1<<BG_ZOOM,
		1<<BG_UNDO,
		1<<BG_SNAP,
		1<<BG_TRKMOD,
		1<<BG_SELECT,
		1<<BG_TRKGRP,
		1<<BG_TRAIN,
		1<<BG_MISCCRT,
		1<<BG_RULER,
		1<<BG_LAYER,
		1<<BG_HOTBAR };



static void ToolbarAction( wBool_t set, void * data )
{
	long mask = (long)data;
	if (set)
		toolbarSet |= mask;
	else
		toolbarSet &= ~mask;
	wPrefSetInteger( "misc", "toolbarset", toolbarSet );
	MainProc( mainW, wResize_e, NULL );
	if (recordF)
		fprintf( recordF, "PARAMETER %s %s %ld", "misc", "toolbarset", toolbarSet );
}


static void CreateToolbarM( wMenu_p toolbarM )
{
	int inx, cnt;
	long *masks;
	char **labels;
	wBool_t set;

	if (bEnableFlex) {
		cnt = sizeof(AllToolbarMasks)/sizeof(AllToolbarMasks[0]);
		masks = AllToolbarMasks;
		labels = AllToolbarLabels;
	} else {
		cnt = sizeof(NonflexToolbarMasks)/sizeof(NonflexToolbarMasks[0]);
		masks = NonflexToolbarMasks;
		labels = NonflexToolbarLabels;
	}
	for (inx=0; inx<cnt; inx++,masks++,labels++) {
		set = ( toolbarSet & *masks ) != 0;
		wMenuToggleCreate( toolbarM, "toolbarM", *labels, 0, set, ToolbarAction, (void*)*masks );
	}
}

/*--------------------------------------------------------------------*/

static wWin_p addElevW;
#define addElevF (wFloat_p)addElevPD.control
EXPORT DIST_T addElevValueV;
static void DoAddElev( void * );

static paramFloatRange_t rn1000_1000 = { -1000.0, 1000.0 };
static paramData_t addElevPLs[] = {
	{   PD_FLOAT, &addElevValueV, "value", PDO_DIM, &rn1000_1000, NULL, 0 } };
static paramGroup_t addElevPG = { "addElev", 0, addElevPLs, sizeof addElevPLs/sizeof addElevPLs[0] };


static void DoAddElev( void * junk )
{
	ParamLoadData( &addElevPG );
	AddElevations( addElevValueV );
	wHide( addElevW );
}


static void ShowAddElevations( void )
{
	if ( selectedTrackCount <= 0 ) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	if (addElevW == NULL)
		addElevW = ParamCreateDialog( &addElevPG, MakeWindowTitle("Change Elevations"), "Change", DoAddElev, wHide, FALSE, NULL, 0, NULL );
	wShow( addElevW );
}

/*--------------------------------------------------------------------*/

static wWin_p tipW;
static long showTipAtStart = 1;
static void ShowTip(void);
static dynArr_t tips_da;
#define tips(N) DYNARR_N( char *, tips_da, N )

static char * tipLabels[] = { "Show Tip at Start", NULL };
static paramTextData_t tipTextData = { 40, 10 };
static paramData_t tipPLs[] = {
#define I_TIPTEXT		(0)
#define tipT			((wText_p)tipPLs[I_TIPTEXT].control)
	{   PD_TEXT, NULL, "text", 0, &tipTextData, NULL, BO_READONLY|BT_CHARUNITS },
	{   PD_TOGGLE, &showTipAtStart, "showatstart", PDO_DLGCMDBUTTON, tipLabels, NULL, BC_NOBORDER },
	{   PD_BUTTON, ShowTip, "next", 0, NULL, "Next" } };
static paramGroup_t tipPG = { "tip", 0, tipPLs, sizeof tipPLs/sizeof tipPLs[0] };

static void CreateTipW( void )
{
	FILE * tipF;
	char buff[4096];
	char * cp;

	tipW = ParamCreateDialog( &tipPG, MakeWindowTitle("Tip of the Day"), "Ok", (paramActionOkProc)wHide, NULL, FALSE, NULL, 0, NULL );

	sprintf( buff, "%s%s%s.tip", libDir, FILE_SEP_CHAR, sProdNameLower );
	tipF = fopen( buff, "r" );
	if (tipF == NULL) {
		DYNARR_APPEND( char *, tips_da, 1 );
		tips(0) = "No tips are available";
	} else {
		while (fgets( buff, sizeof buff, tipF )) {
			if (buff[0] == '#')
				continue;
			cp = buff+strlen(buff)-1;
			if (*cp=='\n') cp--;
			if (*cp=='\r') cp--;
			if (cp < buff)
				continue;
			cp[1] = 0;
			while (*cp=='\\') {
				*cp++ = '\n';
				if (!fgets( cp, (sizeof buff) - (cp-buff), tipF )) {
					return;
				}
				if (*cp=='#')
					continue;
				cp += strlen(cp)-1;
				if (*cp=='\n') cp--;
				if (*cp=='\r') cp--;
				cp[1] = 0;
			}
			DYNARR_APPEND( char *, tips_da, 10 );
			tips(tips_da.cnt-1) = strdup( buff );
		}
	}
}


static void ShowTip( void )
{
	long tipNum;
	
	if (tipW == NULL) {
		CreateTipW();
	}
	ParamLoadControls( &tipPG );
	wTextClear( tipT );
	wPrefGetInteger( "misc", "tip-number", &tipNum, 0 );
	if (tipNum >= tips_da.cnt)
		tipNum = 0;
	wTextAppend( tipT, tips(tipNum) );
	tipNum++;
	wPrefSetInteger( "misc", "tip-number", tipNum );
	wShow( tipW );
}


/*--------------------------------------------------------------------*/

static wWin_p rotateW;
static long rotateValue;
static rotateDialogCallBack_t rotateDialogCallBack;

static void RotateEnterOk( void * );
static paramIntegerRange_t rn360_360 = { -360, 360, 80 };
static paramData_t rotatePLs[] = {
	{   PD_LONG, &rotateValue, "rotate", PDO_ANGLE, &rn360_360, "Angle:" } };
static paramGroup_t rotatePG = { "rotate", 0, rotatePLs, sizeof rotatePLs/sizeof rotatePLs[0] };


EXPORT void StartRotateDialog( rotateDialogCallBack_t func )
{
	if ( rotateW == NULL )
		rotateW = ParamCreateDialog( &rotatePG, MakeWindowTitle("Rotate"), "Ok", RotateEnterOk, wHide, FALSE, NULL, 0, NULL );
	ParamLoadControls( &rotatePG );
	rotateDialogCallBack = func;
	wShow( rotateW );
}


static void RotateEnterOk( void * junk )
{
	ParamLoadData( &rotatePG );
	if (angleSystem==ANGLE_POLAR)
		rotateDialogCallBack( (void*)rotateValue );
	else
		rotateDialogCallBack( (void*)-rotateValue );
	wHide( rotateW );
}


static void RotateDialogInit( void )
{
	ParamRegister( &rotatePG );
}


EXPORT void AddRotateMenu(
		wMenu_p m,
		rotateDialogCallBack_t func )
{
	wMenuPushCreate( m, "", "180° ", 0, func, (void*)180 );
	wMenuPushCreate( m, "", "90°  CW", 0, func, (void*)(long)(90) );
	wMenuPushCreate( m, "", "45°  CW", 0, func, (void*)(long)(45) );
	wMenuPushCreate( m, "", "30°  CW", 0, func, (void*)(long)(30) );
	wMenuPushCreate( m, "", "15°  CW", 0, func, (void*)(long)(15) );
	wMenuPushCreate( m, "", "15°  CCW", 0, func, (void*)(long)(360-15) );
	wMenuPushCreate( m, "", "30°  CCW", 0, func, (void*)(long)(360-30) );
	wMenuPushCreate( m, "", "45°  CCW", 0, func, (void*)(long)(360-45) );
	wMenuPushCreate( m, "", "90°  CCW", 0, func, (void*)(long)(360-90) );
	wMenuPushCreate( m, "", "Enter Angle ...", 0, (wMenuCallBack_p)StartRotateDialog, (void*)func );
}

/*****************************************************************************
 *
 * INITIALIZATON
 *
 */


static wWin_p debugW;

static int debugCnt = 0;
static paramIntegerRange_t r0_100 = { 0, 100, 80 };
static void DebugOk( void * junk );
static paramData_t debugPLs[20];
static paramGroup_t debugPG = { "debug", 0, debugPLs, 0 };

static void DebugOk( void * junk )
{
	wHide( debugW );
}

static void CreateDebugW( void )
{
	debugPG.paramCnt = debugCnt;
	ParamRegister( &debugPG );
	debugW = ParamCreateDialog( &debugPG, MakeWindowTitle("Debug"), "Ok", DebugOk, NULL, FALSE, NULL, 0, NULL );
}


EXPORT void InitDebug(
		char * label,
		long * valueP )
{
	if ( debugCnt >= sizeof debugPLs/sizeof debugPLs[0] )
		AbortProg( "Too many debug flags" );
	memset( &debugPLs[debugCnt], 0, sizeof debugPLs[debugCnt] );
	debugPLs[debugCnt].type = PD_LONG;
	debugPLs[debugCnt].valueP = valueP;
	debugPLs[debugCnt].nameStr = label;
	debugPLs[debugCnt].winData = &r0_100;
	debugPLs[debugCnt].winLabel = label;
	debugCnt++;
}


void RecomputeElevations( void );

static void MiscMenuItemCreate(
		wMenu_p m1,
		wMenu_p m2,
		char * name,
		char * label,
		long acclKey,
		void * func,
		long option,
		void * context )
{
	wMenuPush_p mp;
	mp = wMenuPushCreate( m1, name, label, acclKey, ParamMenuPush, &menuPLs[menuPG.paramCnt] );
	if ( m2 )
		wMenuPushCreate( m2, name, label, acclKey, ParamMenuPush, &menuPLs[menuPG.paramCnt] );
	menuPLs[menuPG.paramCnt].control = (wControl_p)mp;
	menuPLs[menuPG.paramCnt].type = PD_MENUITEM;
	menuPLs[menuPG.paramCnt].valueP = func;
	menuPLs[menuPG.paramCnt].nameStr = name;
	menuPLs[menuPG.paramCnt].option = option;
	menuPLs[menuPG.paramCnt].context = context;
	
	if ( name ) GetBalloonHelpStr( name );
	menuPG.paramCnt++;
}


static char * accelKeyNames[] = {
      "Del",
      "Ins",
      "Home",
      "End",
      "Pgup",
      "Pgdn",
      "Up",
      "Down",
      "Right",
      "Left",
      "Back",
      "F1",
      "F2",
      "F3",
      "F4",
      "F5",
      "F6",
      "F7",
      "F8",
      "F9",
      "F10",
      "F11",
      "F12" };

static void SetAccelKey(
      char * prefName,
      wAccelKey_e key,
      int mode,
      wAccelKeyCallBack_p func,
      void * context )
{
      int mode1 = 0;
      int inx;
      const char * prefValue = wPrefGetString( "accelKey", prefName );
      if ( prefValue != NULL ) {
              while ( prefValue[1] == '-' ) {
                      switch ( prefValue[0] ) {
                      case 'S': mode1 |= WKEY_SHIFT; break;
                      case 'C': mode1 |= WKEY_CTRL; break;
                      case 'A': mode1 |= WKEY_ALT; break;
                      default:
                                       ;
                      }
                      prefValue += 2;
              }
              for ( inx=0; inx<sizeof accelKeyNames/sizeof accelKeyNames[0]; inx++ ) {
                      if ( strcmp( prefValue, accelKeyNames[inx] ) == 0 ) {
                              key = inx+1;
                              mode = mode1;
                              break;
                      }
              }
      }
      wAttachAccelKey( key, mode, func, context );
}

#include "zoomin.xpm"
#include "zoom.xpm"
#include "zoomout.xpm"
#include "undo.xpm"
#include "redo.xpm"
#include "partlist.xpm"
#include "export.xpm"
#include "import.xpm"

static void CreateMenus( void )
{
	wMenu_p fileM, editM, viewM, optionM, windowM, macroM, helpM, toolbarM, messageListM, manageM, addM, changeM, drawM;
	wMenu_p zoomM, zoomSubM, defcmdM;
	wIcon_p bm_p;

	fileM = wMenuBarAdd( mainW, "menuFile", "&File" );
	editM = wMenuBarAdd( mainW, "menuEdit", "&Edit" );
	viewM = wMenuBarAdd( mainW, "menuView", "&View" );
	addM = wMenuBarAdd( mainW, "menuAdd", "&Add" );
	changeM = wMenuBarAdd( mainW, "menuChange", "&Change" );
	drawM = wMenuBarAdd( mainW, "menuDraw", "&Draw" );
	manageM = wMenuBarAdd( mainW, "menuManage", "&Manage" );
	optionM = wMenuBarAdd( mainW, "menuOption", "&Options" );
	macroM = wMenuBarAdd( mainW, "menuMacro", "&Macro" );
	windowM = wMenuBarAdd( mainW, "menuWindow", "&Window" );
	helpM = wMenuBarAdd( mainW, "menuHelp", "&Help" );

	/* 
	 * POPUP MENUS
	 */ 

	popup1M = wMenuPopupCreate( mainW, "Commands" );
	popup2M = wMenuPopupCreate( mainW, "Commands" );
	wMenuPushCreate( popup1M, "cmdPaste", "Paste", 0, (wMenuCallBack_p)EditPaste, NULL );
	wMenuSeparatorCreate( popup1M );
	MiscMenuItemCreate( popup1M, popup2M, "cmdUndo", "Undo", 0, (void*)(wMenuCallBack_p)UndoUndo, 0, (void *)0 );
	MiscMenuItemCreate( popup1M, popup2M, "cmdRedo", "Redo", 0, (void*)(wMenuCallBack_p)UndoRedo, 0, (void *)0 );
	wMenuPushCreate( popup1M, "cmdZoomIn", "Zoom In", 0, (wMenuCallBack_p)DoZoomUp, (void*)1 );
	wMenuPushCreate( popup2M, "cmdZoomIn", "Zoom In", 0, (wMenuCallBack_p)DoZoomUp, (void*)1 );
	wMenuPushCreate( popup1M, "cmdZoomOut", "Zoom Out", 0, (wMenuCallBack_p)DoZoomDown, (void*)1 );
	wMenuPushCreate( popup2M, "cmdZoomOut", "Zoom Out", 0, (wMenuCallBack_p)DoZoomDown, (void*)1 );
	MiscMenuItemCreate( popup1M, popup2M, "cmdGridEnable", "SnapGrid Enable", 0, (void*)(wMenuCallBack_p)SnapGridEnable, 0, (void *)0 );
	MiscMenuItemCreate( popup1M, popup2M, "cmdGridShow", "SnapGrid Show", 0, (void*)(wMenuCallBack_p)SnapGridShow, 0, (void *)0 );
	wMenuSeparatorCreate( popup1M );
	wMenuSeparatorCreate( popup2M );
	MiscMenuItemCreate( popup2M, NULL, "cmdCopy", "Copy", 0, (void*)(wMenuCallBack_p)EditCopy, 0, (void *)0 );
	MiscMenuItemCreate( popup1M, popup2M, "cmdPaste", "Paste", 0, (void*)(wMenuCallBack_p)EditPaste, 0, (void *)0 );
	MiscMenuItemCreate( popup2M, NULL, "cmdDeselectAll", "Deselect All", 0, (void*)(wMenuCallBack_p)SetAllTrackSelect, 0, (void *)0 );
	wMenuPushCreate( popup2M, "cmdMove", "Move", 0, (wMenuCallBack_p)DoCommandBIndirect, &moveCmdInx );
	wMenuPushCreate( popup2M, "cmdRotate", "Rotate", 0, (wMenuCallBack_p)DoCommandBIndirect, &rotateCmdInx );
	MiscMenuItemCreate( popup2M, NULL, "cmdTunnel", "Tunnel", 0, (void*)(wMenuCallBack_p)SelectTunnel, 0, (void *)0 );
	wMenuSeparatorCreate( popup1M );
	wMenuSeparatorCreate( popup2M );
	MiscMenuItemCreate( popup2M, NULL, "cmdDelete", "Delete", 0, (void*)(wMenuCallBack_p)SelectDelete, 0, (void *)0 );
	wMenuSeparatorCreate( popup2M );
	popup1aM = wMenuMenuCreate( popup1M, "", "More" );
	popup2aM = wMenuMenuCreate( popup2M, "", "More" );

	cmdGroup = BG_ZOOM;
	AddToolbarButton( "cmdZoomIn", wIconCreatePixMap(zoomin_xpm), IC_MODETRAIN_TOO,
		(addButtonCallBack_t)DoZoomUp, NULL );
	bm_p = wIconCreatePixMap(zoom_xpm);
	zoomM = wMenuPopupCreate( mainW, "" );
	AddToolbarButton( "cmdZoom", wIconCreatePixMap(zoom_xpm), IC_MODETRAIN_TOO, (wButtonCallBack_p)wMenuPopupShow, zoomM );
	AddToolbarButton( "cmdZoomOut", wIconCreatePixMap(zoomout_xpm), IC_MODETRAIN_TOO,
		(addButtonCallBack_t)DoZoomDown, NULL );

	cmdGroup = BG_UNDO;
	undoB = AddToolbarButton( "cmdUndo", wIconCreatePixMap(undo_xpm), 0, (addButtonCallBack_t)UndoUndo, NULL );
	redoB = AddToolbarButton( "cmdRedo", wIconCreatePixMap(redo_xpm), 0, (addButtonCallBack_t)UndoRedo, NULL );
	wControlActive( (wControl_p)undoB, FALSE );
	wControlActive( (wControl_p)redoB, FALSE );
 

	/*
	 * FILE MENU
	 */
	MiscMenuItemCreate( fileM, NULL, "menuFile-clear", "&New", ACCL_NEW, (void*)(wMenuCallBack_p)DoClear, 0, (void *)0 );
	wMenuPushCreate( fileM, "menuFile-load", "&Open ...", ACCL_OPEN, (wMenuCallBack_p)ChkLoad, NULL );
	wMenuSeparatorCreate( fileM );

	wMenuPushCreate( fileM, "menuFile-save", "&Save", ACCL_SAVE, (wMenuCallBack_p)DoSave, NULL );
	wMenuPushCreate( fileM, "menuFile-saveAs", "Save &As ...", ACCL_SAVEAS, (wMenuCallBack_p)DoSaveAs, NULL );
	wMenuPushCreate( fileM, "menuFile-revert", "Revert", ACCL_REVERT, (wMenuCallBack_p)ChkRevert, NULL );
	wMenuSeparatorCreate( fileM );
	MiscMenuItemCreate( fileM, NULL, "printSetup", "P&rint Setup ...", ACCL_PRINTSETUP, (void*)(wMenuCallBack_p)wPrintSetup, 0, (void *)0 );
	printCmdInx = InitCmdPrint( fileM );
	wMenuSeparatorCreate( fileM );
	MiscMenuItemCreate( fileM, NULL, "cmdImport", "&Import", ACCL_IMPORT, (void*)(wMenuCallBack_p)DoImport, 0, (void *)0 );
	MiscMenuItemCreate( fileM, NULL, "cmdOutputbitmap", "Export to &Bitmap", ACCL_PRINTBM, (void*)(wMenuCallBack_p)OutputBitMapInit(), 0, (void *)0 );
	MiscMenuItemCreate( fileM, NULL, "cmdExport", "E&xport", ACCL_EXPORT, (void*)(wMenuCallBack_p)DoExport, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( fileM, NULL, "cmdExportDXF", "Export D&XF", ACCL_EXPORTDXF, (void*)(wMenuCallBack_p)DoExportDXF, IC_SELECTED, (void *)0 );
	wMenuSeparatorCreate( fileM );

	MiscMenuItemCreate( fileM, NULL, "cmdPrmfile", "Parameter &Files ...", ACCL_PARAMFILES, (void*)ParamFilesInit(), 0, (void *)0 );
	MiscMenuItemCreate( fileM, NULL, "cmdFileNote", "No&tes ...", ACCL_NOTES, (void*)(wMenuCallBack_p)DoNote, 0, (void *)0 );

	wMenuSeparatorCreate( fileM );
	fileList_ml = wMenuListCreate( fileM, "menuFileList", NUM_FILELIST, ChkFileList );
	wMenuSeparatorCreate( fileM );
	wMenuPushCreate( fileM, "menuFile-quit", "E&xit", 0,
		(wMenuCallBack_p)DoQuit, NULL );

	/*
	 * EDIT MENU
	 */
	MiscMenuItemCreate( editM, NULL, "cmdUndo", "&Undo", ACCL_UNDO, (void*)(wMenuCallBack_p)UndoUndo, 0, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdRedo", "R&edo", ACCL_REDO, (void*)(wMenuCallBack_p)UndoRedo, 0, (void *)0 );
	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdCut", "Cu&t", ACCL_CUT, (void*)(wMenuCallBack_p)EditCut, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdCopy", "&Copy", ACCL_COPY, (void*)(wMenuCallBack_p)EditCopy, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdPaste", "&Paste", ACCL_PASTE, (void*)(wMenuCallBack_p)EditPaste, 0, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdDelete", "De&lete", ACCL_DELETE, (void*)(wMenuCallBack_p)SelectDelete, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdMoveToCurrentLayer", "Move To Current Layer", ACCL_MOVCURLAYER, (void*)(wMenuCallBack_p)MoveSelectedTracksToCurrentLayer, IC_SELECTED, (void *)0 );


	wMenuSeparatorCreate( editM );
	menuPLs[menuPG.paramCnt].context = (void*)1;
	MiscMenuItemCreate( editM, NULL, "cmdSelectAll", "Select &All", ACCL_SELECTALL, (void*)(wMenuCallBack_p)SetAllTrackSelect, 0, (void *)1 );
	MiscMenuItemCreate( editM, NULL, "cmdSelectCurrentLayer", "Select Current Layer", ACCL_SETCURLAYER, (void*)(wMenuCallBack_p)SelectCurrentLayer, 0, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdDeselectAll", "&Deselect All", ACCL_DESELECTALL, (void*)(wMenuCallBack_p)SetAllTrackSelect, 0, (void *)0 );
	MiscMenuItemCreate( editM, NULL,  "cmdSelectInvert", "&Invert Selection", 0L, (void*)(wMenuCallBack_p)InvertTrackSelect, 0, (void *)0 );
	MiscMenuItemCreate( editM, NULL,  "cmdSelectOrphaned", "Select Stranded Track", 0L, (void*)(wMenuCallBack_p)OrphanedTrackSelect, 0, (void *)0 );
	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdTunnel", "Tu&nnel", ACCL_TUNNEL, (void*)(wMenuCallBack_p)SelectTunnel, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdAbove", "A&bove", ACCL_ABOVE, (void*)(wMenuCallBack_p)SelectAbove, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdBelow", "Belo&w", ACCL_BELOW, (void*)(wMenuCallBack_p)SelectBelow, IC_SELECTED, (void *)0 );
	
	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdWidth0", "Thin Tracks", ACCL_THIN, (void*)(wMenuCallBack_p)SelectTrackWidth, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( editM, NULL, "cmdWidth2", "Medium Tracks", ACCL_MEDIUM, (void*)(wMenuCallBack_p)SelectTrackWidth, IC_SELECTED, (void *)2 );
	MiscMenuItemCreate( editM, NULL, "cmdWidth3", "Thick Tracks", ACCL_THICK, (void*)(wMenuCallBack_p)SelectTrackWidth, IC_SELECTED, (void *)3 );

	/*
	 * VIEW MENU
	 */
	wMenuPushCreate( viewM, "menuEdit-zoomIn", "Zoom &In", ACCL_ZOOMIN, (wMenuCallBack_p)DoZoomUp, (void*)1 );
	zoomSubM = wMenuMenuCreate( viewM, "menuEdit-zoomTo", "&Zoom" );
	wMenuPushCreate( viewM, "menuEdit-zoomOut", "Zoom &Out", ACCL_ZOOMOUT, (wMenuCallBack_p)DoZoomDown, (void*)1 );
	wMenuSeparatorCreate( viewM );

	InitCmdZoom( zoomM, zoomSubM );

	wMenuPushCreate( viewM, "menuEdit-redraw", "&Redraw", ACCL_REDRAW, (wMenuCallBack_p)MainRedraw, NULL );
	wMenuPushCreate( viewM, "menuEdit-redraw", "Redraw All", ACCL_REDRAWALL, (wMenuCallBack_p)DoRedraw, NULL );
	wMenuSeparatorCreate( viewM );

	snapGridEnableMI = wMenuToggleCreate( viewM, "cmdGridEnable", "Enable SnapGrid", ACCL_SNAPENABLE,
		0, (wMenuToggleCallBack_p)SnapGridEnable, NULL );
	snapGridShowMI = wMenuToggleCreate( viewM, "cmdGridShow", "Show SnapGrid", ACCL_SNAPSHOW,
		FALSE, (wMenuToggleCallBack_p)SnapGridShow, NULL );
	gridCmdInx = InitGrid( viewM );
	wMenuSeparatorCreate( viewM );

	toolbarM = wMenuMenuCreate( viewM, "toolbarM", "&Tool Bar" );
	CreateToolbarM( toolbarM );

  cmdGroup = BG_EASE;
	InitCmdEasement();
	
	cmdGroup = BG_SNAP;
	InitSnapGridButtons();

	/*
	 * ADD MENU
	 */

	cmdGroup = BG_TRKCRT|BG_BIGGAP;	 
 	InitCmdStraight( addM );
	InitCmdCurve( addM );
	InitCmdParallel( addM );
	InitCmdTurnout( addM );
	InitCmdHandLaidTurnout( addM );
	InitCmdStruct( addM );
	InitCmdHelix( addM );
	InitCmdTurntable( addM );
	
	
	/*
	 * CHANGE MENU
	 */
	defcmdM = wMenuMenuCreate( changeM, "defaultCmd", "Default Command" );
	
	wMenuRadioCreate( defcmdM, "defCmd", "Properties", 0, NULL, "0" ); 
	wMenuRadioCreate( defcmdM, "defCmd", "Select", 0, NULL, "1" ); 
	
	cmdGroup = BG_SELECT;
	InitCmdDescribe( changeM );
	InitCmdSelect( changeM );
	wMenuSeparatorCreate( changeM );

	cmdGroup = BG_TRKGRP;
	InitCmdMove( changeM );
	InitCmdDelete();
	InitCmdTunnel();
	InitCmdAboveBelow();
	
	cmdGroup = BG_TRKMOD; 
	if (extraButtons)
		MiscMenuItemCreate( changeM, NULL, "loosen", "&Loosen Tracks", ACCL_LOOSEN, (void*)(wMenuCallBack_p)LoosenTracks, IC_SELECTED, (void *)0 );

	InitCmdModify( changeM );
	InitCmdJoin( changeM );
	InitCmdPull( changeM );
	InitCmdSplit( changeM );
	InitCmdMoveDescription( changeM );
	wMenuSeparatorCreate( changeM );

	MiscMenuItemCreate( changeM, NULL, "cmdAddElevations", "Raise/Lower Elevations", ACCL_CHGELEV, (void*)(wMenuCallBack_p)ShowAddElevations, IC_SELECTED, (void *)0 );
	InitCmdElevation( changeM );
	InitCmdProfile( changeM );

	MiscMenuItemCreate( changeM, NULL, "cmdClearElevations", "Clear Elevations", ACCL_CLRELEV, (void*)(wMenuCallBack_p)ClearElevations, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( changeM, NULL, "cmdElevation", "Recompute Elevations", 0, (void*)(wMenuCallBack_p)RecomputeElevations, 0, (void *)0 );
	ParamRegister( &addElevPG );

	wMenuSeparatorCreate( changeM );
	MiscMenuItemCreate( changeM, NULL, "cmdRescale", "Change Scale", 0, (void*)(wMenuCallBack_p)DoRescale, IC_SELECTED, (void *)0 );
	
	/* 
	 * DRAW MENU
	 */ 
	cmdGroup = BG_MISCCRT;
	InitCmdDraw( drawM );
	InitCmdText( drawM );
	InitCmdNote( drawM );

	cmdGroup = BG_RULER;
	InitCmdRuler( drawM );

	 
	/*
	 * OPTION MENU
	 */
	MiscMenuItemCreate( optionM, NULL, "cmdLayout", "L&ayout ...", ACCL_LAYOUTW, (void*)LayoutInit(), IC_MODETRAIN_TOO, (void *)0 );
	MiscMenuItemCreate( optionM, NULL, "cmdDisplay", "&Display ...", ACCL_DISPLAYW, (void*)DisplayInit(), IC_MODETRAIN_TOO, (void *)0 );
	MiscMenuItemCreate( optionM, NULL, "cmdCmdopt", "Co&mmand ...", ACCL_CMDOPTW, (void*)CmdoptInit(), IC_MODETRAIN_TOO, (void *)0 );
	if ( bEnableFlex )
		MiscMenuItemCreate( optionM, NULL, "cmdEasement", "&Easements ...", ACCL_EASEW, (void*)(wMenuCallBack_p)DoEasementRedir, IC_MODETRAIN_TOO, (void *)0 );
	MiscMenuItemCreate( optionM, NULL, "fontSelW", "&Fonts ...", ACCL_FONTW, (void*)(wMenuCallBack_p)SelectFont, IC_MODETRAIN_TOO, (void *)0 );
	MiscMenuItemCreate( optionM, NULL, "cmdSticky", "Stic&ky ...", ACCL_STICKY, (void*)(wMenuCallBack_p)DoSticky, IC_MODETRAIN_TOO, (void *)0 );
	if (extraButtons) {
		menuPLs[menuPG.paramCnt].context = debugW;
		MiscMenuItemCreate( optionM, NULL, "cmdDebug", "&Debug ...", 0, (void*)(wMenuCallBack_p)wShow, IC_MODETRAIN_TOO, (void *)0 );
	}
	MiscMenuItemCreate( optionM, NULL, "cmdPref", "&Preferences ...", ACCL_PREFERENCES, (void*)PrefInit(), IC_MODETRAIN_TOO, (void *)0 );
	MiscMenuItemCreate( optionM, NULL, "cmdColor", "&Colors ...", ACCL_COLORW, (void*)ColorInit(), IC_MODETRAIN_TOO, (void *)0 );

	/*
	 * MACRO MENU
	 */
	wMenuPushCreate( macroM, "cmdRecord", "&Record ...", ACCL_RECORD, DoRecord, NULL );
	wMenuPushCreate( macroM, "cmdDemo", "&Play Back ...", ACCL_PLAYBACK, DoPlayBack, NULL );


	/*
	 * WINDOW MENU
	 */
	wMenuPushCreate( windowM, "menuWindow", "Main", 0, (wMenuCallBack_p)wShow, mainW );
	winList_mi = wMenuListCreate( windowM, "menuWindow", -1, DoShowWindow );

	/*
	 * HELP MENU
	 */
	wMenuAddHelp( helpM );
	wMenuSeparatorCreate( helpM );
	messageListM = wMenuMenuCreate( helpM, "menuHelpRecentMessages", "Recent Messages" );
	messageList_ml = wMenuListCreate( messageListM, "messageListM", 10, ShowMessageHelp );
	wMenuListAdd( messageList_ml, 0, MESSAGE_LIST_EMPTY, NULL );
	wMenuSeparatorCreate( helpM );
	wMenuPushCreate( helpM, "cmdTip", "Tip of the Day...", 0, (wMenuCallBack_p)ShowTip, NULL );
	demoM = wMenuMenuCreate( helpM, "cmdDemo", "&Demos" );
	wMenuSeparatorCreate( helpM );
	wMenuPushCreate( helpM, "about", "About", 0, (wMenuCallBack_p)wShow, aboutW );

	/*
	 * MANAGE MENU
	 */

	cmdGroup = BG_TRAIN|BG_BIGGAP;
	InitCmdTrain( manageM );
	wMenuSeparatorCreate( manageM );

	InitNewTurnRedir( wMenuMenuCreate( manageM, "cmdTurnoutNew", "Tur&nout Designer..." ) );
	
	MiscMenuItemCreate( manageM, NULL, "cmdGroup", "&Group", ACCL_GROUP, (void*)(wMenuCallBack_p)DoGroup, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( manageM, NULL, "cmdUngroup", "&Ungroup", ACCL_UNGROUP, (void*)(wMenuCallBack_p)DoUngroup, IC_SELECTED, (void *)0 );

	MiscMenuItemCreate( manageM, NULL, "cmdCustmgm", "Custom Management...", ACCL_CUSTMGM, (void*)CustomMgrInit(), 0, (void *)0 );
	MiscMenuItemCreate( manageM, NULL, "cmdRefreshCompound", "Update Turnouts and Structures", 0, (void*)(wMenuCallBack_p)DoRefreshCompound, 0, (void *)0 );

	MiscMenuItemCreate( manageM, NULL, "cmdCarInventory", "Car Inventory", ACCL_CARINV, (void*)(wMenuCallBack_p)DoCarDlg, IC_MODETRAIN_TOO, (void *)0 );
	
	wMenuSeparatorCreate( manageM );

	MiscMenuItemCreate( manageM, NULL, "cmdLayer", "Layers ...", ACCL_LAYERS, (void*)InitLayersDialog(), 0, (void *)0 );
	wMenuSeparatorCreate( manageM );
	 
	MiscMenuItemCreate( manageM, NULL, "cmdEnumerate", "Parts &List ...", ACCL_PARTSLIST, (void*)(wMenuCallBack_p)EnumerateTracks, IC_SELECTED, (void *)0 );
	MiscMenuItemCreate( manageM, NULL, "cmdPricelist", "Price List...", ACCL_PRICELIST, (void*)PriceListInit(), 0, (void *)0 );

	cmdGroup = BG_LAYER|BG_BIGGAP;
	InitLayers();

	cmdGroup = BG_HOTBAR;
	InitHotBar();

#ifdef LATER
#ifdef WINDOWS
	wAttachAccelKey( wAccelKey_Pgdn, 0, (wAccelKeyCallBack_p)DoZoomUp, (void*)1 );
	wAttachAccelKey( wAccelKey_Pgup, 0, (wAccelKeyCallBack_p)DoZoomDown, (void*)1 );
	wAttachAccelKey( wAccelKey_F5, 0, (wAccelKeyCallBack_p)MainRedraw, (void*)1 );
#endif
	wAttachAccelKey( wAccelKey_Ins, WKEY_CTRL, (wAccelKeyCallBack_p)EditCopy, 0 );
	wAttachAccelKey( wAccelKey_Ins, WKEY_SHIFT, (wAccelKeyCallBack_p)EditPaste, 0 );
	wAttachAccelKey( wAccelKey_Back, WKEY_SHIFT, (wAccelKeyCallBack_p)UndoUndo, 0 );
	wAttachAccelKey( wAccelKey_Del, WKEY_SHIFT, (wAccelKeyCallBack_p)EditCut, 0 );
	wAttachAccelKey( wAccelKey_F6, 0, (wAccelKeyCallBack_p)NextWindow, 0 );
#endif
	SetAccelKey( "zoomUp", wAccelKey_Pgdn, 0, (wAccelKeyCallBack_p)DoZoomUp, (void*)1 );
	SetAccelKey( "zoomDown", wAccelKey_Pgup, 0, (wAccelKeyCallBack_p)DoZoomDown, (void*)1 );
	SetAccelKey( "redraw", wAccelKey_F5, 0, (wAccelKeyCallBack_p)MainRedraw, (void*)1 );
	SetAccelKey( "delete", wAccelKey_Del, 0, (wAccelKeyCallBack_p)SelectDelete, (void*)1 );
	SetAccelKey( "copy", wAccelKey_Ins, WKEY_CTRL, (wAccelKeyCallBack_p)EditCopy, 0 );
	SetAccelKey( "paste", wAccelKey_Ins, WKEY_SHIFT, (wAccelKeyCallBack_p)EditPaste, 0 );
	SetAccelKey( "undo", wAccelKey_Back, WKEY_SHIFT, (wAccelKeyCallBack_p)UndoUndo, 0 );
	SetAccelKey( "cut", wAccelKey_Del, WKEY_SHIFT, (wAccelKeyCallBack_p)EditCut, 0 );
	SetAccelKey( "nextWindow", wAccelKey_F6, 0, (wAccelKeyCallBack_p)NextWindow, 0 );

	InitBenchDialog();
}


static void LoadFileList( void )
{
	char file[6];
	int inx;
	const char * cp;
	const char *fileName, *pathName;
	strcpy( file, "fileX" );
	for (inx=NUM_FILELIST-1; inx>=0; inx--) {
		file[4] = '0'+inx;
		cp = wPrefGetString( "filelist", file );
		if (!cp)
			continue;
		pathName = MyStrdup(cp);
		fileName = strrchr( pathName, FILE_SEP_CHAR[0] ); 
		if (fileName)
		wMenuListAdd( fileList_ml, 0, fileName+1, pathName );
	}
}

EXPORT void InitCmdEnumerate( void )
{
	AddToolbarButton( "cmdEnumerate", wIconCreatePixMap(partlist_xpm), IC_SELECTED|IC_ACCLKEY, (addButtonCallBack_t)EnumerateTracks, NULL );
}


EXPORT void InitCmdExport( void )
{
	AddToolbarButton( "cmdExport", wIconCreatePixMap(export_xpm), IC_SELECTED|IC_ACCLKEY, (addButtonCallBack_t)DoExport, NULL );
	AddToolbarButton( "cmdImport", wIconCreatePixMap(import_xpm), IC_ACCLKEY, (addButtonCallBack_t)DoImport, NULL );
}


static drawCmd_t aboutD = {
		NULL,
		&screenDrawFuncs,
		0,
		1.0,
		0.0,
		{0.0,0.0}, {0.0,0.0},
		Pix2CoOrd, CoOrd2Pix };

static paramDrawData_t aboutDrawData = { ICON_WIDTH, ICON_HEIGHT, (wDrawRedrawCallBack_p)RedrawAbout, NULL, &aboutD };
#define COPYRIGHT "Copyright (c) 2005 Sillub Technology"
static paramData_t aboutPLs[] = {
#define I_ABOUTDRAW				(0)
	{   PD_DRAW, NULL, "about", PDO_NOPSHUPD, &aboutDrawData, NULL, 0 },
#define I_ABOUTVERSION			(1)
	{   PD_MESSAGE, NULL, NULL, PDO_DLGNEWCOLUMN, NULL, NULL, 0 },
#define I_ABOUTCOPYRIGHT		(2)
	{   PD_MESSAGE, COPYRIGHT, NULL, 0, NULL, NULL, 0 },
#define I_ABOUTREGTO			(3)
	{   PD_MESSAGE, NULL, NULL, PDO_DLGUNDERCMDBUTT, (void*)250, NULL, 0 } };
static paramGroup_t aboutPG = { "about", 0, aboutPLs, sizeof aboutPLs/sizeof aboutPLs[0] };

static void CreateAboutW( void )
{
	wPos_t w;
	char * copyright = COPYRIGHT;
	w = wLabelWidth( copyright );
	aboutPLs[I_ABOUTVERSION].winData = (void*)w;
	aboutPLs[I_ABOUTCOPYRIGHT].winData = (void*)w;
	ParamRegister( &aboutPG );
	aboutW = ParamCreateDialog( &aboutPG, MakeWindowTitle("About"), "Ok", (paramActionOkProc)wHide, NULL, FALSE, NULL, F_TOP|F_CENTER, NULL );
	RedrawAbout( aboutD.d, NULL, ICON_WIDTH, ICON_HEIGHT );
	ParamLoadMessage( &aboutPG, I_ABOUTVERSION, sAboutProd );
}

/* Give user the option to continue work after crash. This function gives the user 
 * the option to load the checkpoint file to continue working after a crash.
 *
 * \param none
 * \return none
 *
 */
 
static void OfferCheckpoint( void ) 
{
	int ret;
	
	/* sProdName */
	ret = wNotice( "Program was not terminated properly. Do you want to resume working on the previous trackplan?", "Resume", "Ignore" );
	if( ret ) {
		/* load the checkpoint file */ 
		LoadCheckpoint();	
	}

}


EXPORT wWin_p wMain(
		int argc,
		char * argv[] )
{
	char * logFileName = NULL;
	int log_init = 0;
	int initialZoom;
	char * initialFile;
	const char * pref;
	coOrd roomSize;
	long oldToolbarMax;
	long newToolbarMax;
	char *cp;

	initialZoom = 0;
	initialFile = NULL;

	/*
	 * ARGUMENTS
	 */
#ifdef VERBOSE
	lprintf( "argc = %d\n", argc );
#endif
	argv++; argc--;
	while (argc-- > 0) {
#ifdef VERBOSE
		lprintf( "*argv = %s\n", *argv );
#endif

#ifdef WINDOWS
#define OPTIONCHAR '/'
#else
#define OPTIONCHAR '-'
#endif

		if ((*argv)[0] != OPTIONCHAR) {
			if ( argc == 0 ) {
				initialFile = *argv;
				break;
			} else {
				NoticeMessage( MSG_BAD_OPTION, "Ok", NULL, *argv );
			}
		}
		switch ( (*argv)[1] ) {
		case 'd':
			cp = strchr( &(*argv)[2], '=' );
			if ( cp != NULL ) {
				*cp++ = '\0';
				LogSet( &(*argv)[2], atoi(cp) );
			} else {
				LogSet( &(*argv)[2], 1 );
			}
			break;
		case 'l':
			logFileName = &(*argv)[2];
			break;
		case 'v':
			verbose++;
			break;
#ifdef LATER
		} else if ( strncmp( (*argv)+1, "dir", 3 ) == 0 ) {
			if ( *((*argv)+4) == '=' ) {
				appDir = &(*argv)[5];
			} else {
				appDir = "";
			}
#endif
		default:
			NoticeMessage( MSG_BAD_OPTION, "Ok", NULL, *argv );
			break;
		}
		argv++;
	}
	extraButtons = ( getenv(sEnvExtra) != NULL );
	LogOpen( logFileName );
	log_init = LogFindIndex( "init" );
	log_malloc = LogFindIndex( "malloc" );
	log_error = LogFindIndex( "error" );
	log_command = LogFindIndex( "command" );


LOG1( log_init, ( "initCustom\n" ) )
	InitCustom();

	/*
	 * MAIN WINDOW
	 */
LOG1( log_init, ( "create main window\n" ) )
	strcpy( Title1, sProdName );
	sprintf( message, "Unnamed Trackplan - %s(%s)", sProdName, sVersion );
	wSetBalloonHelp( balloonHelp );
	mainW = wWinMainCreate( sProdNameLower, 600, 350, "xtrkcadW", message, "main",
				F_RESIZE|F_MENUBAR|F_NOTAB|F_RECALLPOS,
				MainProc, NULL );
	if ( mainW == NULL )
		return NULL;

	drawColorBlack  = wDrawFindColor( wRGB(  0,  0,  0) );
	drawColorWhite  = wDrawFindColor( wRGB(255,255,255) );
	drawColorRed    = wDrawFindColor( wRGB(255,  0,  0) );
	drawColorBlue   = wDrawFindColor( wRGB(  0,  0,255) );
	drawColorGreen  = wDrawFindColor( wRGB(  0,255,  0) );
	drawColorAqua   = wDrawFindColor( wRGB(  0,255,255) );
	drawColorPurple = wDrawFindColor( wRGB(255,  0,255) );
	drawColorGold   = wDrawFindColor( wRGB(255,215,  0) );
	snapGridColor = drawColorGreen;
	markerColor = drawColorRed;
	borderColor = drawColorBlack;
	crossMajorColor = drawColorRed;
	crossMinorColor = drawColorBlue;
	selectedColor = drawColorRed;
	normalColor = drawColorBlack;
	elevColorIgnore = drawColorBlue;
	elevColorDefined = drawColorGold;
	profilePathColor = drawColorPurple;
	exceptionColor = wDrawFindColor( wRGB(255,0,128) );
	tieColor = wDrawFindColor( wRGB(255,128,0) );

	newToolbarMax = (1<<BG_COUNT)-1;
	wPrefGetInteger( "misc", "toolbarset", &toolbarSet, newToolbarMax );
	wPrefGetInteger( "misc", "max-toolbarset", &oldToolbarMax, 0 );
	toolbarSet |= newToolbarMax & ~oldToolbarMax;
	wPrefSetInteger( "misc", "max-toolbarset", newToolbarMax );
	wPrefSetInteger( "misc", "toolbarset", toolbarSet );

LOG1( log_init, ( "fileInit\n" ) )
	FileInit();

	CreateAboutW();
/*	wWinShow( mainW, TRUE );
	wFlush(); */
	wWinShow( aboutW, TRUE );
	wFlush();

	if (!initialFile) {
		WDOUBLE_T tmp;
LOG1( log_init, ( "set roomsize\n" ) )
		wPrefGetFloat( "draw", "roomsizeX", &tmp, 96.0 );
		roomSize.x = tmp;
		wPrefGetFloat( "draw", "roomsizeY", &tmp, 48.0 );
		roomSize.y = tmp;
		SetRoomSize( roomSize );
	}

	/*
	 * INITIALIZE
	 */
LOG1( log_init, ( "initInfoBar\n" ) )
	InitInfoBar();
/*	wWinShow( mainW, TRUE ); */
	InfoMessage( "Misc2 Init..." );
/*	wFlush(); */
LOG1( log_init, ( "misc2Init\n" ) )
	Misc2Init();

	RotateDialogInit();

	InfoMessage( "Initializing commands" );
/*	wFlush(); */
LOG1( log_init, ( "paramInit\n" ) )
	ParamInit();
LOG1( log_init, ( "initTrkTrack\n" ) )
	InitTrkTrack();

	/*
	 * MENUS
	 */
	InfoMessage( "Initializing menus" );
/*	wFlush(); */
LOG1( log_init, ( "createMenus\n" ) )
	CreateMenus();



LOG1( log_init, ( "initialize\n" ) )
	if (!Initialize())
		return NULL;
	ParamRegister( &menuPG );
	ParamRegister( &stickyPG );

	ResetLayers();
LOG1( log_init, ( "loadFileList\n" ) )
	LoadFileList();
	AddPlaybackProc( "MENU", MenuPlayback, NULL );
	CreateDebugW();

	/*
	 * TIDY UP
	 */
	curCommand = 0;
	commandContext = commandList[curCommand].context;

	/*
	 * READ PARAMETERS
	 */
	if (toolbarSet&(1<<BG_HOTBAR)) {
		LayoutHotBar();
	} else {
		HideHotBar();
	}
LOG1( log_init, ( "drawInit\n" ) )
	DrawInit( initialZoom );

	MacroInit();
	InfoMessage( "Reading parameter files" );
/*	wFlush(); */
LOG1( log_init, ( "paramFileInit\n" ) )
	if (!ParamFileInit())
		return NULL;

	curCommand = describeCmdInx;
LOG1( log_init, ( "Reset\n" ) )
	Reset();

	/*
	 * SCALE
	 */

	/* Set up the data for scale and gauge description */
	DoSetScaleDesc();
	 
	pref = wPrefGetString( "misc", "scale" );
	DoSetScale( pref );

	if (initialFile) {
		DoFileList( 0, NULL, initialFile );
	}

	/*
	 * THE END
	 */

LOG1( log_init, ( "the end\n" ) )
	EnableCommands();
LOG1( log_init, ( "Initialization complete\n" ) )
	InfoMessage( "Initialization complete" );
/*	wFlush(); */
	RegisterChangeNotification( ToolbarChange );
	DoChangeNotification( CHANGE_MAIN|CHANGE_MAP );

	wWinShow( mainW, TRUE );
	wFlush();
	wWinShow( aboutW, FALSE );

	ParamRegister( &tipPG );
	if (showTipAtStart)
		ShowTip();

	/* check for existing checkpoint file */
	if (ExistsCheckpoint())
		OfferCheckpoint();

	inMainW = FALSE;
	return mainW;
}

