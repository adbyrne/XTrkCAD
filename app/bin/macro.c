/** \file macro.c
 * Macros
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
#include <errno.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif
#include <stdarg.h>
#ifndef WINDOWS
#include <sys/time.h>
#else
#include <sys/timeb.h>
#endif
#include <locale.h>

#include <stdint.h>

#include "track.h"
#include "version.h"
#include "common.h"
#include "utility.h"
#include "draw.h"
#include "misc.h"
#include "compound.h"
#include "i18n.h"

EXPORT long adjTimer;
static void DemoInitValues( void );

extern char *userLocale;

/*****************************************************************************
 *
 * RECORD
 *
 */

EXPORT FILE * recordF;
static wWin_p recordW;
struct wFilSel_t * recordFile_fs;
static BOOL_T recordMouseMoves = TRUE;

static void DoRecordButton( void * context );
static paramTextData_t recordTextData = { 50, 16 };
static paramData_t recordPLs[] = {
#define I_RECSTOP		(0)
#define recStopB		((wButton_p)recordPLs[I_RECSTOP].control)
	{   PD_BUTTON, (void*)DoRecordButton, "stop", PDO_NORECORD, NULL, N_("Stop"), 0, (void*)0 },
#define I_RECMESSAGE	(1)
#define recMsgB ((wButton_p)recordPLs[I_RECMESSAGE].control)
	{   PD_BUTTON, (void*)DoRecordButton, "message", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("Message"), 0, (void*)2 },
#define I_RECEND		(2)
#define recEndB ((wButton_p)recordPLs[I_RECEND].control)
	{   PD_BUTTON, (void*)DoRecordButton, "end", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("End"), BO_DISABLED, (void*)4 },
#define I_RECTEXT		(3)
#define recordT			((wText_p)recordPLs[I_RECTEXT].control)
	{   PD_TEXT, NULL, "text", PDO_NORECORD|PDO_DLGRESIZE, &recordTextData, NULL, BT_CHARUNITS|BO_READONLY} };
static paramGroup_t recordPG = { "record", 0, recordPLs, sizeof recordPLs/sizeof recordPLs[0] };


#ifndef WINDOWS
static struct timeval lastTim = {0,0};
static void ComputePause( void )
{
	struct timeval tim;
	long secs;
	long msecs;
	gettimeofday( &tim, NULL );
	secs = tim.tv_sec-lastTim.tv_sec;
	if (secs > 10 || secs < 0)
		return;
	msecs = secs * 1000 + (tim.tv_usec - lastTim.tv_usec)/1000;
	if (msecs > 5000)
		msecs = 5000;
	if (msecs > 1)
		fprintf( recordF, "PAUSE %ld\n", msecs );
	lastTim = tim;
}
#else
static struct _timeb lastTim;
static void ComputePause( void )
{
	struct _timeb tim;
	long secs, msecs;
	_ftime( &tim );
	secs = (long)(tim.time - lastTim.time);
	if (secs > 10 || secs < 0)
		return;
	msecs = secs * 1000;
	if (tim.millitm >= lastTim.millitm) {
		msecs += (tim.millitm - lastTim.millitm);
	} else {
		msecs -= (lastTim.millitm - tim.millitm);
	}
	if (msecs > 5000)
		msecs = 5000;
	if (msecs > 1)
		fprintf( recordF, "PAUSE %ld\n", msecs );
	lastTim = tim;
}
#endif


EXPORT void RecordMouse( char * name, wAction_t action, POS_T px, POS_T py )
{
	int keyState;
	if ( action == C_MOVE || action == C_RMOVE || (action&0xFF) == C_TEXT )
		ComputePause();
	else if ( action == C_DOWN || action == C_RDOWN )
#ifndef WINDOWS
		gettimeofday( &lastTim, NULL );
#else
		_ftime( &lastTim );
#endif
	if (action == wActionMove && !recordMouseMoves)
		return;
	keyState = wGetKeyState();
	if (keyState)
		fprintf( recordF, "KEYSTATE %d\n", keyState );
	fprintf( recordF, "%s %d %0.3f %0.3f\n", name, (int)action, px, py );
	fflush( recordF );
}


static int StartRecord( const char * pathName, const char * fileName, void * context )
{
	time_t clock;
	if (pathName == NULL)
		return TRUE;
	SetCurDir( pathName, fileName );
	recordF = fopen(pathName, "w");
	if (recordF==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Recording"), fileName, strerror(errno) );
		return FALSE;
	}
	time(&clock);
	fprintf(recordF, "# %s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(recordF, "VERSION %d\n", iParamVersion );
	fprintf(recordF, "ROOMSIZE %0.1f x %0.1f\n", mapD.size.x, mapD.size.y );
	fprintf(recordF, "SCALE %s\n", curScaleName );
	fprintf(recordF, "ORIG %0.3f %0.3f %0.3f\n", mainD.scale, mainD.orig.x, mainD.orig.y );
	if ( logClock != 0 )
		fprintf( recordF, "# LOG CLOCK %s\n", ctime(&logClock) );
	if ( logTable_da.cnt > 11 )
		lprintf( "StartRecord( %s ) @ %s\n", pathName, ctime(&clock) );
	ParamStartRecord();
	WriteTracks( recordF );
	WriteLayers( recordF );
	fprintf( recordF, "REDRAW\n" );
	fflush( recordF );
	wTextClear( recordT );
	wShow( recordW );
	Reset();
	wControlActive( (wControl_p)recEndB, FALSE );
	return TRUE;
}


static void DoRecordButton( void * context )
{
	static wBool_t recordingMessage = FALSE;
	char * cp;
	int len;

	switch( (int)(long)context ){
	case 0: /* Stop */
		fprintf( recordF, "CLEAR\nMESSAGE\n");
		fprintf( recordF, N_("End of Playback.  Hit Step to exit\n"));
		fprintf( recordF, "END\nSTEP\n" );
		fclose( recordF );
		recordF = NULL;
		wHide( recordW );
		break;

	case 1: /* Step */
		fprintf( recordF, "STEP\n" );
		break;

	case 4: /* End */
		if (recordingMessage) {
			len = wTextGetSize( recordT );
			if (len == 0)
				break;
			cp = (char*)MyMalloc( len+2 );
			wTextGetText( recordT, cp, len );
			if ( cp[len-1] == '\n' ) len--;
			cp[len] = '\0';
			fprintf( recordF, "%s\nEND\nSTEP\n", cp );
			MyFree( cp );
			recordingMessage = FALSE;
		}
		wTextSetReadonly( recordT, TRUE );
		fflush( recordF );
		wControlActive( (wControl_p)recStopB, TRUE );
		wControlActive( (wControl_p)recMsgB, TRUE );
		wControlActive( (wControl_p)recEndB, FALSE );
		wWinSetBusy( mainW, FALSE );
		break;

	case 2: /* Message */
		fprintf( recordF, "MESSAGE\n" );
		wTextSetReadonly( recordT, FALSE );
		wTextClear( recordT );
		recordingMessage = TRUE;
		wControlActive( (wControl_p)recStopB, FALSE );
		wControlActive( (wControl_p)recMsgB, FALSE );
		wControlActive( (wControl_p)recEndB, TRUE );
		wWinSetBusy( mainW, TRUE );
		break;

	case 3: /* Pause */
		fprintf( recordF, "BIGPAUSE\n" );
		fflush( recordF );
		break;

	case 5: /* CLEAR */
		fprintf( recordF, "CLEAR\n" );
		fflush( recordF );
		wTextClear( recordT );
		break;

	default:
		;
	}
}



EXPORT void DoRecord( void * context )
{
	if (recordW == NULL) {
		char * title = MakeWindowTitle(_("Record"));
		recordW = ParamCreateDialog( &recordPG, title, NULL, NULL, NULL, FALSE, NULL, F_RESIZE, NULL );
		recordFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, title, sRecordFilePattern, StartRecord, NULL );
	}
	wTextClear( recordT );
	wFilSelect( recordFile_fs, curDirName );
}

/*****************************************************************************
 *
 * PLAYBACK MOUSE
 *
 */

static wDrawBitMap_p playbackBm = NULL;
static wDrawColor playbackColor;
static drawCmd_p playbackD;
static wPos_t playbackX, playbackY;

#include "bitmaps/arrow0.xbm"
#include "bitmaps/arrow3.xbm"
#include "bitmaps/arrows.xbm"
#include "bitmaps/flash.xbm"

static wDrawColor rightDragColor;
static wDrawColor leftDragColor;
static wDrawBitMap_p arrow0_bm;
static wDrawBitMap_p arrow3_bm;
static wDrawBitMap_p arrows_bm;
static wDrawBitMap_p flash_bm;

static long flashTO = 60;
static DIST_T PixelsPerStep = 20;
static long stepTO = 100;
EXPORT unsigned long playbackTimer;

static wBool_t didPause;
static wBool_t flashTwice = FALSE;


#define DRAWALL
static void MacroDrawBitMap(
		drawCmd_p d,
		wDrawBitMap_p bm,
		wPos_t x,
		wPos_t y,
		wDrawColor color )
{
	wDrawBitMap( d->d, bm, x, y, color, wDrawOptTemp|wDrawOptNoClip );
	wFlush();
}


static void Flash( drawCmd_p d, wPos_t x, wPos_t y, wDrawColor flashColor )
{
	if (playbackTimer != 0)
		return;
	MacroDrawBitMap( d, flash_bm, x, y, flashColor );
	wPause( flashTO );
	MacroDrawBitMap( d, flash_bm, x, y, flashColor );
	wPause( flashTO );
#ifdef LATER
	MacroDrawBitMap( d->d, flash_bm, x, y, flashColor );
	wPause( flashTO );
	MacroDrawBitMap( d->d, flash_bm, x, y, flashColor );
	wPause( flashTO );
#endif
}


EXPORT long playbackDelay = 100;
static long playbackSpeed = 2;

static void SetPlaybackSpeed(
		wIndex_t inx )
{
	switch (inx) {
	case 0: playbackDelay = 500; break;
	case 1: playbackDelay = 200; break;
	default:
	case 2: playbackDelay = 100; break;
	case 3: playbackDelay = 50; break;
	case 4: playbackDelay = 15; break;
	case 5: playbackDelay = 0; break;
	}
	playbackSpeed = inx;
}

static void ClearPlaybackCursor( void )
{
	if (playbackBm != NULL)
		MacroDrawBitMap( playbackD, playbackBm, playbackX, playbackY, playbackColor );
	playbackBm = NULL;
}


static void MoveCursor(
		drawCmd_p d,
		playbackProc proc,
		wAction_t action,
		coOrd pos,
		wDrawBitMap_p bm,
		wDrawColor color )
{
	DIST_T dist, dx, dy;
	coOrd pos1, dpos;
	int i, steps;
	wPos_t x, y;
	wPos_t xx, yy;

	ClearPlaybackCursor();

	if (d == NULL)
		return;

	pos1 = pos;
	d->CoOrd2Pix( d, pos, &x, &y );

	if (playbackTimer == 0 && playbackD == d && !didPause) {
		dx = (DIST_T)(x-playbackX);
		dy = (DIST_T)(y-playbackY);
		dist = sqrt( dx*dx + dy*dy );
		steps = (int)(dist / PixelsPerStep ) + 1;
		dx /= steps;
		dy /= steps;
		d->Pix2CoOrd( d, playbackX, playbackY, &pos1 );
		dpos.x = (pos.x-pos1.x)/steps;
		dpos.y = (pos.y-pos1.y)/steps;
		for ( i=1; i<=steps; i++ ) {
			xx = playbackX+(wPos_t)(i*dx);
			yy = playbackY+(wPos_t)(i*dy);
			MacroDrawBitMap( d, bm, xx, yy, color );
			pos1.x += dpos.x;
			pos1.y += dpos.y;
			if (proc)
				proc( action, pos1 );
			else if (d->d == mainD.d) {
				InfoPos( pos1 );
			}
			wPause( stepTO*playbackDelay/100 );
			MacroDrawBitMap( d, bm, xx, yy, color );
			if (!inPlayback) {
				return;
			}
		}
	}
	MacroDrawBitMap( playbackD=d, playbackBm=bm, playbackX=x, playbackY=y, playbackColor=color );
}


static void PlaybackCursor(
		drawCmd_p d,
		playbackProc proc,
		wAction_t action,
		coOrd pos,
		wDrawColor color )
{
	wDrawBitMap_p bm;
	wPos_t x, y;
	long time0, time1;

	time0 = wGetTimer();
	ClearPlaybackCursor();

	d->CoOrd2Pix( d, pos, &x, &y );

	switch( action ) {

	case wActionMove:
		MacroDrawBitMap( playbackD=d, playbackBm=arrow0_bm, playbackX=x, playbackY=y, playbackColor=wDrawColorBlack );
		break;

	case C_DOWN:
		MoveCursor( d, proc, wActionMove, pos, arrow0_bm, wDrawColorBlack );
		if (flashTwice) Flash( d, x, y, rightDragColor );
		MacroDrawBitMap( d, arrow0_bm, x, y, wDrawColorBlack );
		MacroDrawBitMap( playbackD=d, playbackBm=((MyGetKeyState()&WKEY_SHIFT)?arrows_bm:arrow3_bm), playbackX=x, playbackY=y,
				playbackColor=rightDragColor );
		Flash( d, x, y, rightDragColor );
		break;

	case C_MOVE:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrows_bm:arrow3_bm);
		MoveCursor( d, proc, C_MOVE, pos, bm, rightDragColor );
		playbackD=d; playbackBm=bm; playbackX=x; playbackY=y; playbackColor=rightDragColor;
		break;

	case C_UP:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrows_bm:arrow3_bm);
		MoveCursor( d, proc, C_MOVE, pos, bm, rightDragColor );
		/*MacroDrawBitMap( d, bm, x, y, rightDragColor );*/
		if (flashTwice) Flash( d, x, y, rightDragColor );
		MacroDrawBitMap( d, bm, x, y, rightDragColor );
		MacroDrawBitMap( playbackD=d, playbackBm=arrow0_bm, playbackX=x, playbackY=y, playbackColor=wDrawColorBlack );
		Flash( d, x, y, rightDragColor );
		break;

	case C_RDOWN:
		MoveCursor( d, proc, wActionMove, pos, arrow0_bm, wDrawColorBlack );
		if (flashTwice) Flash( d, x, y, leftDragColor );
		MacroDrawBitMap( d, arrow0_bm, x, y, wDrawColorBlack );
		MacroDrawBitMap( playbackD=d, playbackBm=((MyGetKeyState()&WKEY_SHIFT)?arrows_bm:arrow3_bm), playbackX=x, playbackY=y, playbackColor=leftDragColor );
		Flash( d, x, y, leftDragColor );
		break;

	case C_RMOVE:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrows_bm:arrow3_bm);
		MoveCursor( d, proc, C_RMOVE, pos, bm, leftDragColor );
		playbackD=d; playbackBm=bm; playbackX=x; playbackY=y; playbackColor=leftDragColor;
		break;

	case C_RUP:
		bm = ((MyGetKeyState()&WKEY_SHIFT)?arrows_bm:arrow3_bm);
		MoveCursor( d, proc, C_RMOVE, pos, bm, leftDragColor );
		if (flashTwice) Flash( d, x, y, leftDragColor );
		MacroDrawBitMap( d, bm, x, y, leftDragColor );
		MacroDrawBitMap( playbackD=d, playbackBm=arrow0_bm, playbackX=x, playbackY=y, playbackColor=wDrawColorBlack );
		Flash( d, x, y, leftDragColor );
		break;

	case C_REDRAW:
		MacroDrawBitMap( playbackD, playbackBm, playbackX, playbackY, playbackColor );
		break;

	default:
		;
	}
	time1 = wGetTimer();
	adjTimer += (time1-time0);
}


EXPORT void PlaybackMouse(
		playbackProc proc,
		drawCmd_p d,
		wAction_t action,
		coOrd pos,
		wDrawColor color )
{
#ifdef LATER
	if (action == C_DOWN || action == C_RDOWN) {
		MoveCursor( d, proc, wActionMove, pos, arrow0_bm, wDrawColorBlack );
		ClearPlaybackCursor();
	} else {
		PlaybackCursor( d, proc, action, pos, wDrawColorBlack );
	}
#endif
	PlaybackCursor( d, proc, action, pos, wDrawColorBlack );
	if (playbackBm != NULL)
		MacroDrawBitMap( playbackD, playbackBm, playbackX, playbackY, playbackColor );
	proc( action, pos );
	if (playbackBm != NULL)
		MacroDrawBitMap( playbackD, playbackBm, playbackX, playbackY, playbackColor );
#ifdef LATER
	if (action == C_DOWN || action == C_RDOWN) {
		PlaybackCursor( d, proc, action, pos, wDrawColorBlack );
	}
#endif
	didPause = FALSE;
}


EXPORT void MovePlaybackCursor(
		drawCmd_p d,
		wPos_t x,
		wPos_t y )
{
	coOrd pos;
	d->Pix2CoOrd( d, x, y, &pos );
	d->CoOrd2Pix( d, pos, &x, &y );
	MoveCursor( d, NULL, wActionMove, pos, arrow0_bm, wDrawColorBlack );
	MacroDrawBitMap( d, arrow0_bm, x, y, wDrawColorBlack );
	MacroDrawBitMap( d, arrow3_bm, x, y, rightDragColor );
	Flash( d, x, y, rightDragColor );
	MacroDrawBitMap( d, arrow3_bm, x, y, rightDragColor );
	MacroDrawBitMap( d, arrow0_bm, x, y, wDrawColorBlack );
}

/*****************************************************************************
 *
 * PLAYBACK
 *
 */

EXPORT wBool_t inPlayback;
EXPORT wBool_t inPlaybackQuit;
EXPORT wWin_p demoW;
EXPORT int curDemo = -1;

typedef struct {
		char * title;
		char * fileName;
		} demoList_t;
static dynArr_t demoList_da;
#define demoList(N) DYNARR_N( demoList_t, demoList_da, N )
static struct wFilSel_t * playbackFile_fs;

typedef struct {
		char * label;
		playbackProc_p proc;
		void * data;
		} playbackProc_t;
static dynArr_t playbackProc_da;
#define playbackProc(N) DYNARR_N( playbackProc_t, playbackProc_da, N )

static coOrd oldRoomSize;
static coOrd oldMainOrig;
static coOrd oldMainSize;
static DIST_T oldMainScale;
static char * oldScaleName;

static wBool_t pauseDemo = FALSE;
static long bigPause = 2000;
#ifdef LATER
static long MSEC_PER_PIXEL = 6;
#endif
#ifdef DEMOPAUSE
static wButton_p demoPause;
#endif
static BOOL_T playbackNonStop = FALSE;

static BOOL_T showParamLineNum = FALSE;

static int playbackKeyState;

static void DoDemoButton( void * context );
static paramTextData_t demoTextData = { 50, 16 };
static paramData_t demoPLs[] = {
#define I_DEMOSTEP		(0)
#define demoStep		((wButton_p)demoPLs[I_DEMOSTEP].control)
	{   PD_BUTTON, (void*)DoDemoButton, "step", PDO_NORECORD, NULL, N_("Step"), BB_DEFAULT, (void*)0 },
#define I_DEMONEXT		(1)
#define demoNext		((wButton_p)demoPLs[I_DEMONEXT].control)
	{   PD_BUTTON, (void*)DoDemoButton, "next", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("Next"), 0, (void*)1 },
#define I_DEMOQUIT		(2)
#define demoQuit		((wButton_p)demoPLs[I_DEMOQUIT].control)
	{   PD_BUTTON, (void*)DoDemoButton, "quit", PDO_NORECORD|PDO_DLGHORZ, NULL, N_("Quit"), BB_CANCEL, (void*)3 },
#define I_DEMOSPEED		(3)
#define demoSpeedL		((wList_p)demoPLs[I_DEMOSPEED].control)
	{   PD_DROPLIST, &playbackSpeed, "speed", PDO_NORECORD|PDO_LISTINDEX|PDO_DLGHORZ, (void*)80, N_("Speed") },
#define I_DEMOTEXT		(4)
#define demoT			((wText_p)demoPLs[I_DEMOTEXT].control)
	{   PD_TEXT, NULL, "text", PDO_NORECORD|PDO_DLGRESIZE, &demoTextData, NULL, BT_CHARUNITS|BO_READONLY} };
static paramGroup_t demoPG = { "demo", 0, demoPLs, sizeof demoPLs/sizeof demoPLs[0] };

EXPORT int MyGetKeyState( void )
{
	if (inPlayback)
		return playbackKeyState;
	else
		return wGetKeyState();
}


EXPORT void AddPlaybackProc( char * label, playbackProc_p proc, void * data )
{
	DYNARR_APPEND( playbackProc_t, playbackProc_da, 10 );
	playbackProc(playbackProc_da.cnt-1).label = MyStrdup(label);
	playbackProc(playbackProc_da.cnt-1).proc = proc;
	playbackProc(playbackProc_da.cnt-1).data = data;
}


static void PlaybackQuit( void )
{
	long playbackSpeed1 = playbackSpeed;
	if (paramFile)
		fclose( paramFile );
	paramFile = NULL;
	if (!inPlayback)
		return;
	inPlaybackQuit = TRUE;
	ClearPlaybackCursor();
	wPrefReset();
	wHide( demoW );
	wWinSetBusy( mainW, FALSE );
	wWinSetBusy( mapW, FALSE );
	ParamRestoreAll();
	RestoreLayers();
	wEnableBalloonHelp( (int)enableBalloonHelp );
	mainD.scale = oldMainScale;
	mainD.size = oldMainSize;
	mainD.orig = oldMainOrig;
	SetRoomSize( oldRoomSize );
	tempD.orig = mainD.orig;
	tempD.size = mainD.size;
	tempD.scale = mainD.scale;
	ClearTracks();
	checkPtMark = changed = 0;
	RestoreTrackState();
	inPlaybackQuit = FALSE;
	Reset();
	DoSetScale( oldScaleName );
	DoChangeNotification( CHANGE_ALL );
	CloseDemoWindows();
	inPlayback = FALSE;
	curDemo = -1;
	wPrefSetInteger( "misc", "playbackspeed", playbackSpeed );
	playbackNonStop = FALSE;
	playbackSpeed = playbackSpeed1;
	UndoResume();
	wWinBlockEnable( TRUE );
}


static int documentEnable = 0;
static int documentAutoSnapshot = 0;

static drawCmd_t snapshot_d = {
		NULL,
		&screenDrawFuncs,
		0,
		16.0,
		0,
		{0.0, 0.0}, {1.0, 1.0},
		Pix2CoOrd, CoOrd2Pix };
static int documentSnapshotNum = 1;
static int documentCopy = 0;
static FILE * documentFile;
static BOOL_T snapshotMouse = FALSE;

EXPORT void TakeSnapshot( drawCmd_t * d )
{
	char * cp;
	wPos_t ix, iy;
	if (d->dpi < 0)
		d->dpi = mainD.dpi;
	if (d->scale < 0)
		d->scale = mainD.scale;
	if (d->orig.x < 0 || d->orig.y < 0)
		d->orig = mainD.orig;
	if (d->size.x < 0 || d->size.y < 0)
		d->size = mainD.size;
	ix = (wPos_t)(d->dpi*d->size.x/d->scale);
	iy = (wPos_t)(d->dpi*d->size.y/d->scale);
	d->d = wBitMapCreate( ix, iy, 8 );
	if (d->d == (wDraw_p)0) {
		return;
	}
	DrawTracks( d, d->scale, d->orig, d->size );
	if ( snapshotMouse && playbackBm )
		wDrawBitMap( d->d, playbackBm, playbackX, playbackY, playbackColor, 0 );
	wDrawLine( d->d, 0, 0, ix-1, 0, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	wDrawLine( d->d, ix-1, 0, ix-1, iy-1, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	wDrawLine( d->d, ix-1, iy-1, 0, iy-1, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	wDrawLine( d->d, 0, iy-1, 0, 0, 0, wDrawLineSolid, wDrawColorBlack, 0 );
	strcpy( message, paramFileName );
	cp = message+strlen(message)-4;
	sprintf( cp, "-%4.4d.xpm", documentSnapshotNum );
	wBitMapWriteFile( d->d, message );
	wBitMapDelete( d->d );
	documentSnapshotNum++;
	if (documentCopy && documentFile) {
		cp = strrchr( message, FILE_SEP_CHAR[0] );
		if (cp == 0)
			cp = message;
		else
			cp++;
		cp[strlen(cp)-4] = 0;
		fprintf( documentFile, "\n?G%s\n", cp );
	}
}

static void EnableButtons(
		BOOL_T enable )
{
	wButtonSetBusy( demoStep, !enable );
	wButtonSetBusy( demoNext, !enable );
	wControlActive( (wControl_p)demoStep, enable );
	wControlActive( (wControl_p)demoNext, enable );
#ifdef DEMOPAUSE
	wButtonSetBusy( demoPause, enable );
#endif
}

EXPORT void PlaybackMessage(
		char * line )
{
	char * cp;
	wTextAppend( demoT, _(line) );
	if ( documentCopy && documentFile ) {
		if (strncmp(line, "__________", 10) != 0) {
			for (cp=line; *cp; cp++) {
				switch (*cp) {
				case '<':
					fprintf( documentFile, "$B" );
					break;
				case '>':
					fprintf( documentFile, "$" );
					break;
				default:
					fprintf( documentFile, "%c", *cp );
				}
			}
		}
	}
}


static void PlaybackSetup( void )
{
	SaveTrackState();
	EnableButtons( TRUE );
	SetPlaybackSpeed( (wIndex_t)playbackSpeed );
	wListSetIndex( demoSpeedL, (wIndex_t)playbackSpeed );
	wTextClear( demoT );
	wShow( demoW );
	wFlush();
	RulerRedraw( TRUE );
	wPrefFlush();
	wWinSetBusy( mainW, TRUE );
	wWinSetBusy( mapW, TRUE );
	ParamSaveAll();
	paramLineNum = 0;
	oldRoomSize = mapD.size;
	oldMainOrig = mainD.orig;
	oldMainSize = mainD.size;
	oldMainScale = mainD.scale;
	oldScaleName = curScaleName;
	Reset();
	paramVersion = -1;
	playbackColor=wDrawColorBlack;
	paramTogglePlaybackHilite = FALSE;
	CompoundClearDemoDefns();
	SaveLayers();
}


static void Playback( void )
{
	POS_T x, y;
	POS_T zoom;
	wIndex_t inx;
	long timeout;
	static enum { pauseCmd, mouseCmd, otherCmd } thisCmd, lastCmd;
	int len;
	static wBool_t demoWinOnTop = FALSE;
	coOrd roomSize;
	char * cp, * cq;

	useCurrentLayer = FALSE;
	inPlayback = TRUE;
	EnableButtons( FALSE );
	lastCmd = otherCmd;
	playbackTimer = 0;
	if (demoWinOnTop) {
		wWinTop( mainW );
		demoWinOnTop = FALSE;
	}
	while (TRUE) {
		if ( paramFile == NULL ||
			 fgets(paramLine, STR_LONG_SIZE, paramFile) == NULL ) {
			paramTogglePlaybackHilite = FALSE;
			ClearPlaybackCursor();
			CloseDemoWindows();
			if (paramFile) {
				fclose( paramFile );
				paramFile = NULL;
			}
			if (documentFile) {
				fclose( documentFile );
				documentFile = NULL;
			}
			Reset();
			if (curDemo < 0 || curDemo >= demoList_da.cnt)
				break;
			strcpy( paramFileName, demoList(curDemo).fileName );
			paramFile = fopen( paramFileName, "r" );
			if ( paramFile == NULL ) {
				NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Demo"), paramFileName, strerror(errno) );
				return;
			}

			playbackColor=wDrawColorBlack;
			paramLineNum = 0;
			wWinSetTitle( demoW, demoList( curDemo ).title );
			curDemo++;
			ClearTracks();
			UndoSuspend();
			wWinBlockEnable( FALSE );
			checkPtMark = 0;
			RulerRedraw( TRUE );
			DoChangeNotification( CHANGE_ALL );
			CompoundClearDemoDefns();
			if ( fgets(paramLine, STR_LONG_SIZE, paramFile) == NULL ) {
				NoticeMessage( MSG_CANT_READ_DEMO, _("Continue"), NULL, sProdName, paramFileName );
				fclose( paramFile );
				paramFile = NULL;
				return;
			}
		}
		if (paramLineNum == 0) {
			documentSnapshotNum = 1;
			if (documentEnable) {
				strcpy( message, paramFileName );
				cp = message+strlen(message)-4;
				strcpy( cp, ".hlpsrc" );
				documentFile = fopen( message, "w" );
				documentCopy = TRUE;
			}
		}
		thisCmd = otherCmd;
		paramLineNum++;
		if (showParamLineNum)
			InfoCount( paramLineNum );
		Stripcr( paramLine );
		if (paramLine[0] == '#') {
			/* comment */
		} else if (paramLine[0] == 0) {
			/* empty paramLine */
		} else if (ReadTrack( paramLine ) ) {
		} else if (strncmp( paramLine, "STEP", 5 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			wWinTop( demoW );
			demoWinOnTop = TRUE;
			didPause = FALSE;
			EnableButtons( TRUE );
			if (!demoWinOnTop) {
				wWinTop( demoW );
				demoWinOnTop = TRUE;
			}
			if ( documentAutoSnapshot ) {
				snapshot_d.dpi=snapshot_d.scale=snapshot_d.orig.x=snapshot_d.orig.y=snapshot_d.size.x=snapshot_d.size.y=-1;
				TakeSnapshot(&snapshot_d);
			}
			if (playbackNonStop) {
				wPause( 1000 );
				EnableButtons( FALSE );
			} else {
				return;
			}
		} else if (strncmp( paramLine, "CLEAR", 5 ) == 0) {
			wTextClear( demoT );
		} else if (strncmp( paramLine, "MESSAGE", 7 ) == 0) {
			didPause = FALSE;
			wWinTop( demoW );
			demoWinOnTop = TRUE;
			while ( ( fgets( paramLine, STR_LONG_SIZE, paramFile ) ) != NULL ) {
				paramLineNum++;
				if ( strncmp(paramLine, "END", 3) == 0 )
					break;
				if ( strncmp(paramLine, "STEP", 3) == 0 ) {
					wWinTop( demoW );
					demoWinOnTop = TRUE;
					EnableButtons( TRUE );
					return;
				}
				PlaybackMessage( paramLine );
			}
		} else if (strncmp( paramLine, "ROOMSIZE ", 9 ) == 0) {
			if (ParseRoomSize( paramLine+9, &roomSize ))
				SetRoomSize( roomSize );
		} else if (strncmp( paramLine, "SCALE ", 6 ) == 0) {
			DoSetScale( paramLine+6 );
		} else if (strncmp( paramLine, "REDRAW", 6 ) == 0) {
			ResolveIndex();
			RecomputeElevations();
			DoRedraw();
			/*DoChangeNotification( CHANGE_ALL );*/
			if (playbackD != NULL && playbackBm != NULL)
				MacroDrawBitMap( playbackD, playbackBm, playbackX, playbackY, wDrawColorBlack );
		} else if (strncmp( paramLine, "COMMAND ", 8 ) == 0) {
			paramTogglePlaybackHilite = FALSE;
			PlaybackCommand( paramLine, paramLineNum );
		} else if (strncmp( paramLine, "RESET", 5 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			Reset();
		} else if (strncmp( paramLine, "VERSION", 7 ) == 0) {
			paramVersion = atol( paramLine+8 );
			if ( paramVersion > iParamVersion ) {
				NoticeMessage( MSG_PLAYBACK_VERSION_UPGRADE, _("Ok"), NULL, paramVersion, iParamVersion, sProdName );
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_PLAYBACK_VERSION_DOWNGRADE, _("Ok"), NULL, paramVersion, iMinParamVersion, sProdName );
				break;
			}
		} else if (strncmp( paramLine, "ORIG ", 5 ) == 0) {
			if ( !GetArgs( paramLine+5, "fff", &zoom, &x, &y ) )
				continue;
			mainD.scale = zoom;
			mainD.orig.x = x;
			mainD.orig.y = y;
			SetMainSize();
			tempD.orig = mainD.orig;
			tempD.size = mainD.size;
			tempD.scale = mainD.scale;
#ifdef LATER
			ResolveIndex();
			RecomputeElevations();
#endif
			DoRedraw();
			if (playbackD != NULL && playbackBm != NULL)
				MacroDrawBitMap( playbackD, playbackBm, playbackX, playbackY, wDrawColorBlack );
#ifdef LATER
		} else if (strncmp( paramLine, "POSITION ", 9 ) == 0) {
			if ( !GetArgs( paramLine+9, "ff", &x, &y ) )
				continue;
			MovePlaybackCursor( &mainD, x, y );
#endif
		} else if (strncmp( paramLine, "PAUSE ", 6 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			didPause = TRUE;
#ifdef DOPAUSE
			if (lastCmd == mouseCmd) {
				thisCmd = pauseCmd;
			} else {
				if ( !GetArgs( paramLine+6, "l", &timeout ) )
					continue;
#ifdef LATER
				wFlush();
				wAlarm( timeout*playbackDelay/100, playback );
				return;
#else
				if (playbackTimer == 0)
					wPause( timeout*playbackDelay/100 );
#endif
			}
#endif
			if ( !GetArgs( paramLine+6, "l", &timeout ) )
				continue;
			if (timeout > 10000)
				timeout = 1000;
			if (playbackTimer == 0)
				wPause( timeout*playbackDelay/100 );
			wFlush();
			if (demoWinOnTop) {
				wWinTop( mainW );
				demoWinOnTop = FALSE;
			}
		} else if (strncmp( paramLine, "BIGPAUSE ", 6 ) == 0) {
			paramTogglePlaybackHilite = TRUE;
			didPause = FALSE;
#ifdef LATER
			wFlush();
			wAlarm( bigPause*playbackDelay/100, playback );
			return;
#else
			if (playbackTimer == 0) {
				timeout = bigPause*playbackDelay/100;
				if (timeout <= dragTimeout)
					timeout = dragTimeout+1;
				wPause( timeout );
			}
#endif
		} else if (strncmp( paramLine, "KEYSTATE ", 9 ) == 0 ) {
			playbackKeyState = atoi( paramLine+9 );
		} else if (strncmp( paramLine, "TIMESTART", 9 ) == 0 ) {
			playbackTimer = wGetTimer();
		} else if (strncmp( paramLine, "TIMEEND", 7 ) == 0 ) {
			if (playbackTimer == 0) {
				NoticeMessage( MSG_PLAYBACK_TIMEEND, _("Ok"), NULL );
			} else {
				playbackTimer = wGetTimer() - playbackTimer;
				sprintf( message, _("Elapsed time %lu\n"), playbackTimer );
				wTextAppend( demoT, message );
				playbackTimer = 0;
			}
		} else if (strncmp( paramLine, "MEMSTATS", 8 ) == 0 ) {
			wTextAppend( demoT, wMemStats() );
			wTextAppend( demoT, "\n" );
		} else if (strncmp( paramLine, "SNAPSHOT", 8 ) == 0 ) {
			if ( !documentEnable )
				continue;
			snapshot_d.dpi=snapshot_d.scale=snapshot_d.orig.x=snapshot_d.orig.y=snapshot_d.size.x=snapshot_d.size.y=-1;
			cp = paramLine+8;
			while (*cp && isspace((unsigned char)*cp)) cp++;
			if (snapshot_d.dpi = strtod( cp, &cq ), cp == cq)
				snapshot_d.dpi = -1;
			else if (snapshot_d.scale = strtod( cq, &cp ), cp == cq)
				snapshot_d.scale = -1;
			else if (snapshot_d.orig.x = strtod( cp, &cq ), cp == cq)
				snapshot_d.orig.x = -1;
			else if (snapshot_d.orig.y = strtod( cq, &cp ), cp == cq)
				snapshot_d.orig.y = -1;
			else if (snapshot_d.size.x = strtod( cp, &cq ), cp == cq)
				snapshot_d.size.x = -1;
			else if (snapshot_d.size.y = strtod( cq, &cp ), cp == cq)
				snapshot_d.size.y = -1;
			TakeSnapshot(&snapshot_d);
		} else if (strncmp( paramLine, "DOCUMENT ON", 11 ) == 0 ) {
			documentCopy = documentEnable;
		} else if (strncmp( paramLine, "DOCUMENT OFF", 12 ) == 0 ) {
			documentCopy = FALSE;
		} else if (strncmp( paramLine, "DOCUMENT COPY", 13 ) == 0 ) {
			while ( ( fgets( paramLine, STR_LONG_SIZE, paramFile ) ) != NULL ) {
				paramLineNum++;
				if ( strncmp(paramLine, "END", 3) == 0 )
					break;
				if ( documentCopy && documentFile )
					fprintf( documentFile, "%s", paramLine );
			}
		} else if ( strncmp( paramLine, "DEMOINIT", 8 ) == 0 ) {
			DemoInitValues();
		} else {
			if (strncmp( paramLine, "MOUSE ", 6 ) == 0) {
#ifdef LATER
				if ( GetArgs( paramLine+6, "dff", &rc, &pos.x, &pos.y) ) {
					pos.x = pos.x / mainD.scale - mainD.orig.x;
					pos.y = pos.y / mainD.scale - mainD.orig.y;
#ifdef DOPAUSE
					if (lastCmd == pauseCmd) {
#endif
						d = sqrt( (pos.x-mainPos.x)*(pos.x-mainPos.x) +
								  (pos.y-mainPos.y)*(pos.y-mainPos.y) );
						d *= mainD.dpi;
						timeout = (long)(MSEC_PER_PIXEL * d);
						if (timeout > 2)
							if (playbackTimer == 0)
								wPause( timeout );
#ifdef DOPAUSE
					}
#endif
					mainPos = pos;
				}
#endif
				thisCmd = mouseCmd;
			}
			if (strncmp( paramLine, "MAP ", 6 ) == 0) {
#ifdef LATER
				if ( GetArgs( paramLine+6, "dff", &rc, &pos.x, &pos.y ) ) {
					pos.x = pos.x / mapD.scale - mapD.orig.x;
					pos.y = pos.y / mapD.scale - mapD.orig.y;
#ifdef DOPAUSE
					if (lastCmd == pauseCmd) {
#endif
						d = sqrt( (pos.x-mapPos.y)*(pos.x-mapPos.x) +
								  (pos.y-mapPos.y)*(pos.y-mapPos.y) );
						d *= mapD.dpi;
						timeout = (long)(MSEC_PER_PIXEL * d);
						if (timeout > 2)
							if (playbackTimer == 0)
								wPause( timeout );
#ifdef DOPAUSE
					}
#endif
					mapPos = pos;
				}
#endif
				thisCmd = mouseCmd;
			}
			for ( inx=0; inx<playbackProc_da.cnt; inx++ ) {
				len = strlen(playbackProc(inx).label);
				if (strncmp( paramLine, playbackProc(inx).label, len ) == 0) {
					if (playbackProc(inx).data == NULL) {
						while (paramLine[len] == ' ') len++;
						playbackProc(inx).proc( paramLine+len );
					} else
						playbackProc(inx).proc( (char*)playbackProc(inx).data );
					break;
				}
			}
			if ( thisCmd == mouseCmd ) {
				EnableButtons( FALSE );
				playbackKeyState = 0;
			}
			if (inx == playbackProc_da.cnt) {
				NoticeMessage( MSG_PLAYBACK_UNK_CMD, _("Ok"), NULL, paramLineNum, paramLine );
			}
		}
		lastCmd = thisCmd;
		wFlush();
		if (pauseDemo) {
			EnableButtons( TRUE );
			pauseDemo = FALSE;
			return;
		}
	}
	if (paramFile) {
		fclose( paramFile );
		paramFile = NULL;
	}
	if (documentFile) {
		fclose( documentFile );
		documentFile = NULL;
	}
	PlaybackQuit();
}


static int StartPlayback( const char * pathName, const char * fileName, void * context )
{
	if (pathName == NULL)
		return TRUE;

	SetCurDir( pathName, fileName );
	paramFile = fopen( pathName, "r" );
	if ( paramFile == NULL ) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Playback"), pathName, strerror(errno) );
		return FALSE;
	}

	strcpy( paramFileName, pathName );

	PlaybackSetup();
	curDemo = -1;
	UndoSuspend();
	wWinBlockEnable( FALSE );
	Playback();

	return TRUE;
}


static void DoDemoButton( void * command )
{
	switch( (int)(long)command ) {
	case 0:
		/* step */
		playbackNonStop = (wGetKeyState() & WKEY_SHIFT) != 0;
		paramHiliteFast = (wGetKeyState() & WKEY_CTRL) != 0;
		Playback();
		break;
	case 1:
		if (curDemo == -1) {
			DoSaveAs( NULL );
		} else {
			/* next */
			if (paramFile)
				fclose(paramFile);
			paramFile = NULL;
			wTextClear( demoT );
			if ( (wGetKeyState()&WKEY_SHIFT)!=0 ) {
				if ( curDemo >= 2 )
					curDemo -= 2;
				else
					curDemo = 0;
			}
			Playback();
		}
		break;
	case 2:
		/* pause */
		pauseDemo = TRUE;
		break;
	case 3:
		/* quit */
		PlaybackQuit();
		break;
	default:
		;
	}
}




static void DemoDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	if ( inx != I_DEMOSPEED ) return;
	SetPlaybackSpeed( (wIndex_t)*(long*)valueP );
}


static void CreateDemoW( void )
{
	char * title = MakeWindowTitle(_("Demo"));
	demoW = ParamCreateDialog( &demoPG, title, NULL, NULL, NULL, FALSE, NULL, F_RESIZE, DemoDlgUpdate );

	wListAddValue( demoSpeedL, _("Slowest"), NULL, (void*)0 );
	wListAddValue( demoSpeedL, _("Slow"), NULL, (void*)1 );
	wListAddValue( demoSpeedL, _("Normal"), NULL, (void*)2 );
	wListAddValue( demoSpeedL, _("Fast"), NULL, (void*)3 );
	wListAddValue( demoSpeedL, _("Faster"), NULL, (void*)4 );
	wListAddValue( demoSpeedL, _("Fastest"), NULL, (void*)5 );
	wListSetIndex( demoSpeedL, (wIndex_t)playbackSpeed );
	playbackFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, title, sRecordFilePattern, StartPlayback, NULL );
}


EXPORT void DoPlayBack( void * context )
{
	if (demoW == NULL)
		CreateDemoW();
	wButtonSetLabel( demoNext, _("Save") );
	wFilSelect( playbackFile_fs, curDirName );
}



/*****************************************************************************
 *
 * DEMO
 *
 */

static char * demoInitParams[] = {
		"layout title1 XTrackCAD",
		"layout title2 Demo",
		"GROUP layout",
		"display tunnels 1",
		"display endpt 2",
		"display labelenable 7",
		"display description-fontsize 48",
		"display labelscale 8",
		"display layoutlabels 6",
		"display color-layers 0",
		"display tworailscale 16",
		"display tiedraw 0",
		"pref mingridspacing 5",
		"pref balloonhelp 1",
		"display hotbarlabels 1",
		"display mapscale 64",
		"display livemap 0",
		"display carhotbarlabels 1",
		"display hideTrainsInTunnels 0",
		"GROUP display",
		"cmdopt move-quick 0",
		"pref turntable-angle 7.500",
		"cmdopt preselect 1",
		"pref coupling-speed-max 100",
		"cmdopt rightclickmode 0",
		"GROUP cmdopt",
		"pref checkpoint 0",
		"pref units 0",
		"pref dstfmt 1",
		"pref anglesystem 0",
		"pref minlength 0.100",
		"pref connectdistance 0.100",
		"pref connectangle 1.000",
		"pref dragpixels 20",
		"pref dragtimeout 500",
		"display autoPan 0",
		"display listlabels 7",
		"layout mintrackradius 1.000",
		"layout maxtrackgrade 5.000",
		"display trainpause 300",
		"GROUP pref",
		"rgbcolor snapgrid 65280",
		"rgbcolor marker 16711680",
		"rgbcolor border 0",
		"rgbcolor crossmajor 16711680",
		"rgbcolor crossminor 255",
		"rgbcolor normal 0",
		"rgbcolor selected 16711680",
		"rgbcolor profile 16711935",
		"rgbcolor exception 16711808",
		"rgbcolor tie 16744448",
		"GROUP rgbcolor",
		"easement val 0.000",
		"easement r 0.000",
		"easement x 0.000",
		"easement l 0.000",
		"GROUP easement",
		"grid horzspacing 12.000",
		"grid horzdivision 12",
		"grid horzenable 0",
		"grid vertspacing 12.000",
		"grid vertdivision 12",
		"grid vertenable 0",
		"grid origx 0.000",
		"grid origy 0.000",
		"grid origa 0.000",
		"grid show 0",
		"GROUP grid",
		"misc toolbarset 65535",
		"GROUP misc",
		"sticky set 268435383", /* 0xfffffb7 - all but Helix and Turntable */
		"GROUP sticky",
		"turnout hide 0",
		"layer button-count 10",
		NULL };

static void DemoInitValues( void )
{
	int inx;
	char **cpp;
	static playbackProc_p paramPlaybackProc = NULL;
	static coOrd roomSize = { 96.0, 48.0 };
	char scaleName[10];
	if ( paramPlaybackProc == NULL ) {
		for ( inx=0; inx<playbackProc_da.cnt; inx++ ) {
			if (strncmp( "PARAMETER", playbackProc(inx).label, 9 ) == 0 ) {
				paramPlaybackProc = playbackProc(inx).proc;
				break;
			}
		}
	}
	SetRoomSize( roomSize );
	strcpy( scaleName, "DEMO" );
	DoSetScale( scaleName );
	if ( paramPlaybackProc == NULL ) {
		wNoticeEx( NT_INFORMATION, _("Can not find PARAMETER playback proc"), _("Ok"), NULL );
		return;
	}
	for ( cpp = demoInitParams; *cpp; cpp++ )
		paramPlaybackProc( *cpp );
}


static void DoDemo( void * demoNumber )
{

	if (demoW == NULL)
		CreateDemoW();
	wButtonSetLabel( demoNext, _("Next") );
	curDemo = (int)(long)demoNumber;
	if ( curDemo < 0 || curDemo >= demoList_da.cnt ) {
		NoticeMessage( MSG_DEMO_BAD_NUM, _("Ok"), NULL, curDemo );
		return;
	}
	PlaybackSetup();
	playbackNonStop = (wGetKeyState() & WKEY_SHIFT) != 0;
	paramFile = NULL;
	Playback();
}


static BOOL_T ReadDemo(
		char * line )
{
		static wMenu_p m;
		char * cp;
		char *oldLocale = NULL;

		if ( m == NULL )
			m = demoM;

		if ( strncmp( line, "DEMOGROUP ", 10 ) == 0 ) {
			if (userLocale)
				oldLocale = SaveLocale(userLocale);
			m = wMenuMenuCreate( demoM, NULL, _(line+10) );
			if (oldLocale)
				RestoreLocale(oldLocale);
		} else if ( strncmp( line, "DEMO ", 5 ) == 0 ) {
			if (line[5] != '"')
				goto error;
			cp = line+6;
			while (*cp && *cp != '"') cp++;
			if ( !*cp )
				goto error;
			*cp++ = '\0';
			while (*cp && *cp == ' ') cp++;
			if ( strlen(cp)==0 )
				goto error;
			DYNARR_APPEND( demoList_t, demoList_da, 10 );
			if (userLocale)
				oldLocale = SaveLocale(userLocale);
			demoList( demoList_da.cnt-1 ).title = MyStrdup( _(line+6) );
			demoList( demoList_da.cnt-1 ).fileName =
				(char*)MyMalloc( strlen(libDir) + 1 + 5 + 1 + strlen(cp) + 1 );
			sprintf( demoList( demoList_da.cnt-1 ).fileName, "%s%s%s%s%s",
				libDir, FILE_SEP_CHAR, "demos", FILE_SEP_CHAR, cp );
			wMenuPushCreate( m, NULL, _(line+6), 0, DoDemo, (void*)(intptr_t)(demoList_da.cnt-1) );
			if (oldLocale)
				RestoreLocale(oldLocale);
		}
		return TRUE;
error:
		InputError( "Expected 'DEMO \"<Demo Name>\" <File Name>'", TRUE );
		return FALSE;
}



EXPORT BOOL_T MacroInit( void )
{
	AddParam( "DEMOGROUP ", ReadDemo );
	AddParam( "DEMO ", ReadDemo );

	recordMouseMoves = ( getenv( "XTRKCADNORECORDMOUSEMOVES" ) == NULL );

	rightDragColor = drawColorRed;
	leftDragColor = drawColorBlue;

	arrow0_bm = wDrawBitMapCreate( mainD.d, arrow0_width, arrow0_height, 12, 12, arrow0_bits );
	arrow3_bm = wDrawBitMapCreate( mainD.d, arrow3_width, arrow3_height, 12, 12, arrow3_bits );
	arrows_bm = wDrawBitMapCreate( mainD.d, arrows_width, arrows_height, 12, 12, arrows_bits );
	flash_bm = wDrawBitMapCreate( mainD.d, flash_width, flash_height, 12, 12, flash_bits );

	ParamRegister( &recordPG );
	ParamRegister( &demoPG );
	return TRUE;
}
