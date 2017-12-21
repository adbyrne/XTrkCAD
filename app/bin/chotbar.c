/** \file chotbar.c
 * HOT BAR
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

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "compound.h"
#include "fileio.h"
#include "messages.h"
#include "track.h"

EXPORT DIST_T curBarScale = -1;
EXPORT long hotBarLabels = 0;

#include "bitmaps/hotbarl.xbm"
#include "bitmaps/hotbarr.xbm"

static wButton_p hotBarLeftB = NULL;
static wButton_p hotBarRightB = NULL;
static wMenu_p hotbarPopupM;
static wMenuList_p hotBarML = NULL;
static wIndex_t hotBarMLcnt = 0;
static drawCmd_t hotBarD = {
		NULL,
		&screenDrawFuncs,
		0,
		1.0,
		0.0,
		{0.0, 0.0}, {0.0, 0.0},
		Pix2CoOrd, CoOrd2Pix };
static wPos_t hotBarDrawHeight = 28;
static wPos_t hotBarHeight = 28;
typedef struct {
		DIST_T x;
		DIST_T w;
		DIST_T objectW;
		DIST_T labelW;
		coOrd size;
		coOrd orig;
		BOOL_T isTrack;
		void * context;
		hotBarProc_t proc;
		DIST_T barScale;
		} hotBarMap_t;
static dynArr_t hotBarMap_da;
#define hotBarMap(N) DYNARR_N( hotBarMap_t, hotBarMap_da, N )
static int hotBarCurrSelects[2] = { -1, -1 };
static int hotBarCurrStarts[2] = { -1, -1 };
static int hotBarCurrEnds[2] = { -1, -1 };
#define hotBarCurrSelect (hotBarCurrSelects[programMode])
#define hotBarCurrStart (hotBarCurrStarts[programMode])
#define hotBarCurrEnd (hotBarCurrEnds[programMode])
static DIST_T hotBarWidth = 0.0;

static void HotBarHighlight( int inx )
{
	wPos_t x0;
	if ( inx >= hotBarCurrStart && inx < hotBarCurrEnd ) {
		x0 = (wPos_t)((hotBarMap(inx).x-hotBarMap((int)hotBarCurrStart).x)*hotBarD.dpi);
		wDrawFilledRectangle( hotBarD.d, x0, 0, (wPos_t)(hotBarMap(inx).w*hotBarD.dpi-2), hotBarHeight, wDrawColorBlack, wDrawOptTemp );
	}
}


static wFont_p hotBarFp = NULL;
static wFontSize_t hotBarFs = 8;

static void RedrawHotBar( wDraw_p dd, void * data, wPos_t w, wPos_t h  )
{
	DIST_T hh = (double)hotBarDrawHeight/hotBarD.dpi;
	coOrd orig;
	int inx;
	hotBarMap_t * tbm;
	DIST_T barHeight = (DIST_T)(wControlGetHeight( (wControl_p)hotBarD.d ) - 2)/hotBarD.dpi;
	DIST_T barWidth = (DIST_T)(wControlGetWidth( (wControl_p)hotBarD.d ) - 2)/hotBarD.dpi;
	DIST_T barScale;
	DIST_T x;

	wDrawClear( hotBarD.d );
	wControlActive( (wControl_p)hotBarLeftB, hotBarCurrStart > 0 );
	if (hotBarCurrStart < 0) {
		wControlActive( (wControl_p)hotBarRightB, FALSE );
		return;
	}
	if ( hotBarLabels && !hotBarFp )
		hotBarFp = wStandardFont( F_HELV, FALSE, FALSE );
	wPos_t textSize = wMessageGetHeight(0L);
	for ( inx=hotBarCurrStart; inx < hotBarMap_da.cnt; inx++ ) {
		tbm = &hotBarMap(inx);
		barScale = tbm->barScale;
		x = tbm->x - hotBarMap(hotBarCurrStart).x;
		if ( x + tbm->w > barWidth ) {
			break;
		}
		orig.y = hh/2.0*barScale - tbm->size.y/2.0 - tbm->orig.y;
		if ( hotBarLabels ) {
			orig.y += textSize/hotBarD.dpi*barScale;
			if ( tbm->labelW > tbm->objectW ) {
				x += (tbm->labelW-tbm->objectW)/2;
			}
		}
		x *= barScale;
		orig.x = x;
		hotBarD.scale = barScale;
		hotBarD.size.x = barWidth*barScale;
		hotBarD.size.y = barHeight*barScale;
		tbm->proc( HB_DRAW, tbm->context, &hotBarD, &orig );
		if ( hotBarLabels ) {
			hotBarD.scale = 1.0;
			orig.x = tbm->x - hotBarMap(hotBarCurrStart).x;
			orig.y = 2.0/hotBarD.dpi;	            //Draw Label under icon
			DrawString( &hotBarD, orig, 0.0, tbm->proc( HB_BARTITLE, tbm->context, NULL, NULL ), hotBarFp, hotBarFs, drawColorBlack );
		}
	}
	hotBarCurrEnd = inx;
	if (hotBarCurrSelect >= hotBarCurrStart && hotBarCurrSelect < hotBarCurrEnd )
		HotBarHighlight( hotBarCurrSelect );
/*	  else
		hotBarCurrSelect = -1;*/
	wControlActive( (wControl_p)hotBarRightB, hotBarCurrEnd < hotBarMap_da.cnt );
	wPrefSetInteger( "misc", "hotbar-start", hotBarCurrStart );
}


static void DoHotBarRight( void * data )
{
	DIST_T barWidth = ((DIST_T)wControlGetWidth( (wControl_p)hotBarD.d ) - 2.0)/hotBarD.dpi;
	int inx = hotBarCurrStart;
	DIST_T lastX = hotBarMap(hotBarMap_da.cnt-1).x + hotBarMap(hotBarMap_da.cnt-1).w + 2.0/hotBarD.dpi;
	if (MyGetKeyState()&WKEY_SHIFT) {
		inx += hotBarMap_da.cnt/8;
	} else {
		inx++;
	}
	if ( inx >= hotBarMap_da.cnt )
		inx = hotBarMap_da.cnt-1;
	while ( inx > 1 && lastX - hotBarMap(inx-1).x <= barWidth )
			 inx--;
	if ( inx != hotBarCurrStart ) {
		hotBarCurrStart = inx;
		RedrawHotBar( hotBarD.d, NULL, 0, 0 );
	}
}


static void DoHotBarLeft( void * data )
{
	int inx = hotBarCurrStart;
	if (MyGetKeyState()&WKEY_SHIFT) {
		inx -= hotBarMap_da.cnt/8;
	} else {
		inx --;
	}
	if ( inx < 0 )
		inx = 0;
	if ( inx != hotBarCurrStart ) {
		hotBarCurrStart = inx;
		RedrawHotBar( hotBarD.d, NULL, 0, 0 );
	}
}


static void DoHotBarJump( int inx )
{
	DIST_T x, barWidth;

	inx -= '0';
	if (inx < 0 || inx > 9)
		return;
	if (inx == 0)
		inx = 9;
	else
		inx--;
	barWidth = (DIST_T)wControlGetWidth( (wControl_p)hotBarD.d )/hotBarD.dpi;
	x = (inx*(hotBarWidth-barWidth))/9.0;
	for ( inx=0; inx<hotBarMap_da.cnt; inx++ ) {
		if (x <= hotBarMap(inx).x)
			break;
	}
	if ( hotBarCurrStart != inx ) {
		hotBarCurrStart = inx;
		RedrawHotBar( NULL, NULL, 0, 0 );
	}
}


static void SelectHotBar( wDraw_p d, void * context, wAction_t action, wPos_t w, wPos_t h )
{
	int inx;
	coOrd pos;
	DIST_T x;
	wPos_t px;
	hotBarMap_t * tbm;
	char * titleP;

	if ( hotBarMap_da.cnt <= 0 )
		return;
#if 0
	if ( !CommandEnabled( hotBarCmdInx ) )
		return;
#endif
	if ( (action&0xFF) ==  wActionRUp ) {
		wMenuPopupShow( hotbarPopupM );
		return;
	}
	x = w/hotBarD.dpi + hotBarMap(hotBarCurrStart).x;
	for ( inx=hotBarCurrStart; inx<hotBarCurrEnd; inx++ ) {
		if ((x >= hotBarMap(inx).x) &&						//leave spaces between buttons
			(x <= hotBarMap(inx).x + hotBarMap(inx).w )) {
				break;
		}
	}
	if (inx >= hotBarCurrEnd)
		return;
	tbm = &hotBarMap(inx);
	px = (wPos_t)((tbm->x-hotBarMap(hotBarCurrStart).x)*hotBarD.dpi);
	px += (wPos_t)(tbm->w*hotBarD.dpi/2);
	titleP = tbm->proc( HB_LISTTITLE, tbm->context, NULL, NULL );
	px -= wLabelWidth( titleP ) / 2;
	wControlSetBalloon( (wControl_p)hotBarD.d, px, -20, titleP );
	switch (action & 0xff) {
	case wActionLDown:
		pos.x = mainD.size.x+mainD.orig.x;
		pos.y = mainD.size.y+mainD.orig.y;
		if ( hotBarCurrSelect >= 0 ) {
			HotBarHighlight( hotBarCurrSelect );
			hotBarCurrSelect = -1;
		}
		tbm->proc( HB_SELECT, tbm->context, NULL, NULL );
		hotBarCurrSelect = inx;
		HotBarHighlight( hotBarCurrSelect );
		if (recordF) {
			fprintf( recordF, "HOTBARSELECT %s\n", tbm->proc( HB_FULLTITLE, tbm->context, NULL, NULL ) );
		}
		FakeDownMouseState();
		break;
	case wActionExtKey:
		switch ((wAccelKey_e)(action>>8)) {
		case wAccelKey_Right:
			DoHotBarRight(NULL);
			break;
		case wAccelKey_Left:
			DoHotBarLeft(NULL);
			break;
		case wAccelKey_Up:
			break;
		case wAccelKey_Down:
			break;
		default:
			break;
		}
		break;
	case wActionText:
		switch (action >> 8) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			DoHotBarJump( action >> 8 );
			break;
		case 0x1B:
			ConfirmReset(FALSE);
			break;
		}
		break;
	}
}


EXPORT void HotBarCancel( void )
{
	if ( hotBarCurrSelect >= 0 )
		HotBarHighlight( hotBarCurrSelect );
	hotBarCurrSelect = -1;
}


static BOOL_T HotBarSelectPlayback( char * line )
{
	int inx;
	hotBarMap_t * tbm;
	while (*line && isspace((unsigned char)*line) ) line++;
	for ( inx=0; inx<hotBarMap_da.cnt; inx++ ) {
		tbm = &hotBarMap(inx);
		if ( strcmp( tbm->proc( HB_FULLTITLE, tbm->context, NULL, NULL ), line) == 0) {
			if ( hotBarCurrSelect >= 0 ) {
				HotBarHighlight( hotBarCurrSelect );
			}
			hotBarCurrSelect = inx;
			if ( hotBarCurrSelect < hotBarCurrStart || hotBarCurrSelect > hotBarCurrEnd ) {
				hotBarCurrStart = hotBarCurrSelect;
				RedrawHotBar( hotBarD.d, NULL, 0, 0 );
			}
			HotBarHighlight( hotBarCurrSelect );
			hotBarMap(inx).proc( HB_SELECT, hotBarMap(inx).context, NULL, NULL );
			FakeDownMouseState();
			return TRUE;
		}
	}
	return FALSE;
}


static void HotbarJump( int inx, const char * name, void * arg )
{
	hotBarCurrStart = (int)(long)arg;
	RedrawHotBar( hotBarD.d, NULL, 0, 0 );
}


static BOOL_T SetHotBarScale( char * line )
{
	curBarScale = atof( line + 9 );
	return TRUE;
}


static char curContentsLabel[STR_SHORT_SIZE];
EXPORT void AddHotBarElement(
		char * contentsLabel,
		coOrd size,
		coOrd orig,
		BOOL_T isTrack,
		DIST_T barScale,
		void * context,
		hotBarProc_t proc_p )
{
	hotBarMap_t * tbm;
	coOrd textsize;

		if ( contentsLabel && strncmp(contentsLabel, curContentsLabel, sizeof curContentsLabel) != 0 ) {
			wMenuListAdd( hotBarML, hotBarMLcnt++, contentsLabel, (void*)(intptr_t)hotBarMap_da.cnt );
			strncpy( curContentsLabel, contentsLabel, sizeof curContentsLabel );
		}

		if (barScale <= 0) {
			if (isTrack)
				barScale = (trackGauge>0.1)?trackGauge*24:10;
			else
				barScale = size.y/((double)hotBarDrawHeight/hotBarD.dpi-0.07);
		}
		DYNARR_APPEND( hotBarMap_t, hotBarMap_da, 10 );
		tbm = &hotBarMap(hotBarMap_da.cnt-1);
		if (barScale < 1)
			barScale = 1;
		if (size.x > barScale)
			 barScale = size.x;
		tbm->context = context;
		tbm->size = size;
		tbm->orig = orig;
		tbm->proc = proc_p;
		tbm->barScale = barScale;
		tbm->w = tbm->objectW = size.x/barScale + 5.0/hotBarD.dpi;
		tbm->labelW = 0;
		tbm->x = hotBarWidth;
		if ( hotBarLabels ) {
			DrawTextSize( &hotBarD, proc_p( HB_BARTITLE, context, NULL, NULL), hotBarFp, hotBarFs, FALSE, &textsize );
			tbm->labelW = textsize.x+5/hotBarD.dpi;
			if ( tbm->labelW > tbm->w ) {
				tbm->w = tbm->labelW;
			}
		}
		hotBarWidth += tbm->w + 2/hotBarD.dpi;
}


static void ChangeHotBar( long changes )
{
#ifdef LATER
	int curFileIndex = -3;
	char * name;
#endif
	static long programModeOld = 0;

	if ( (changes&(CHANGE_SCALE|CHANGE_PARAMS|CHANGE_TOOLBAR)) == 0 )
		return;
	if ( hotBarLabels && !hotBarFp )
		hotBarFp = wStandardFont( F_HELV, FALSE, FALSE );
	if (hotBarLeftB != NULL && curScaleName) {
	hotBarWidth = 0.0;
	hotBarMLcnt = 0;
	wMenuListClear( hotBarML );
	DYNARR_RESET( hotBarMap_t, hotBarMap_da );
	curContentsLabel[0] = '\0';
	if ( programMode == MODE_DESIGN ) {
		AddHotBarTurnouts();
		AddHotBarStructures();
	} else {
		AddHotBarCarDesc();
	}

	if ( programModeOld != programMode ) {
		hotBarCurrSelects[0] = hotBarCurrSelects[1] = -1;
		programModeOld = programMode;
	}
	if (hotBarMap_da.cnt > 0 && (hotBarCurrStart >= hotBarMap_da.cnt||hotBarCurrStart < 0))
		hotBarCurrStart = 0;
	RedrawHotBar( NULL, NULL, 0, 0 );
	}
}


EXPORT void InitHotBar( void )
{
	long v;

	AddParam( "BARSCALE", SetHotBarScale );
	AddPlaybackProc( "HOTBARSELECT", (playbackProc_p)HotBarSelectPlayback, NULL );
	RegisterChangeNotification( ChangeHotBar );
	wPrefGetInteger( "misc", "hotbar-start", &v, hotBarCurrStart );
	hotBarCurrStart = (int)v;
	hotbarPopupM = MenuRegister( "Hotbar Select" );
	hotBarML = wMenuListCreate( hotbarPopupM, "", -1, HotbarJump );
}

EXPORT void LayoutHotBar( void )
{
	wPos_t buttonWidth, winWidth, winHeight;
	BOOL_T initialize = FALSE;

	wWinGetSize( mainW, &winWidth, &winHeight );
	hotBarHeight = hotBarDrawHeight;
	if ( hotBarLabels) {
	   hotBarHeight += wMessageGetHeight(0L);
	}
	if (hotBarLeftB == NULL) {
		wIcon_p bm_p;
		if (winWidth < 50)
			return;
		bm_p = wIconCreateBitMap( 16, 16, turnbarl_bits, wDrawColorBlack );
		hotBarLeftB = wButtonCreate( mainW, 0, 0, "hotBarLeft", (char*)bm_p, BO_ICON, 0, DoHotBarLeft, NULL );
		bm_p = wIconCreateBitMap( 16, 16, turnbarr_bits, wDrawColorBlack );
		hotBarRightB = wButtonCreate( mainW, 0, 0, "hotBarRight", (char*)bm_p, BO_ICON, 0, DoHotBarRight, NULL );
		hotBarD.d = wDrawCreate( mainW, 0, 0, NULL, BD_NOCAPTURE|BD_NOFOCUS, 100, hotBarHeight, NULL, RedrawHotBar, SelectHotBar );
		hotBarD.dpi = wDrawGetDPI( hotBarD.d );
		hotBarD.scale = 1.0;
		initialize = TRUE;
	}
	buttonWidth = wControlGetWidth((wControl_p)hotBarLeftB);
	wControlSetPos( (wControl_p)hotBarLeftB, 0, toolbarHeight );
	wControlSetPos( (wControl_p)hotBarRightB, winWidth-20-buttonWidth, toolbarHeight );
	wControlSetPos( (wControl_p)hotBarD.d, buttonWidth, toolbarHeight );
	wDrawSetSize( hotBarD.d, winWidth-20-buttonWidth*2, hotBarHeight+2 );
	hotBarD.size.x = ((double)(winWidth-20-buttonWidth*2))/hotBarD.dpi*hotBarD.scale;
	hotBarD.size.y = (double)hotBarDrawHeight/hotBarD.dpi*hotBarD.scale;  //Exclude Label from calc
	wControlShow( (wControl_p)hotBarLeftB, TRUE );
	wControlShow( (wControl_p)hotBarRightB, TRUE );
	wControlShow( (wControl_p)hotBarD.d, TRUE );
	if (initialize)
		ChangeHotBar( CHANGE_PARAMS );
	else
		RedrawHotBar( NULL, NULL, 0, 0 );
	toolbarHeight += hotBarHeight+3;
}

void HideHotBar( void )
{
	if (hotBarLeftB != NULL) {
		wControlShow( (wControl_p)hotBarLeftB, FALSE );
		wControlShow( (wControl_p)hotBarRightB, FALSE );
		wControlShow( (wControl_p)hotBarD.d, FALSE );
	}
}
