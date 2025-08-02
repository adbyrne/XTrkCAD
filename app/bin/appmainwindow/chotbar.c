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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "fileio.h"
#include "ccornu.h"
#include "track.h"
#include "draw.h"
#include "include/toolbar.h"

#define LARGE_SCROLL_STEP 8

EXPORT DIST_T curBarScale = -1;
EXPORT long hotBarLabels = 0;
EXPORT wWinPix_t hotBarHeight = 32;

#define HOTBAR_LEFT 2

static wControl_p hotBarLeftB = NULL;
static wControl_p hotBarRightB = NULL;

static struct {
	wMenu_p hotbarPopupM;
	wControl_p hotBarML; 
	wIndex_t hotBarMLcnt;
	char curContentsLabel[STR_SHORT_SIZE];
} hotbarContextMenu;

static drawCmd_t hotBarD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{0.0, 0.0}, {0.0, 0.0},
	Pix2CoOrd, CoOrd2Pix
};

static wWinPix_t hotBarDrawHeight[] = {26, 32, 40};
static wFontSize_t hotBarFontSize[] = {7, 11, 14};

typedef struct {
	DIST_T x;
	DIST_T w;
	DIST_T objectW;
	DIST_T labelW;
	coOrd size;
	coOrd orig;
	BOOL_T isFixed;
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

static wBool_t ScrollButtonStatus();
static void ScrollButtonUpdate();
static DIST_T DrawFixedElements(DIST_T barWidth, DIST_T barHeight);
static int DrawVariableElements(DIST_T xStart, DIST_T barWidth,
                                DIST_T barHeight);
static void AddLabel(hotBarMap_t* tbm, POS_T xPos);
static void ShowHotBar(wBool_t show);

static void HotBarHighlight( int inx, DIST_T fixed_x )
{
	if ( inx > 0 && ( inx < hotBarCurrStart || inx >= hotBarCurrEnd ) ) {
		return;
	}
	coOrd orig, size;
	hotBarD.scale = 1;
	if ( inx == 0 && hotBarMap_da.cnt>0 && hotBarMap(0).isFixed) {
		orig.x = 0;
	} else {
		orig.x = hotBarMap(inx).x-hotBarMap(hotBarCurrStart).x + (inx>0?fixed_x:0);
	}
	orig.y = 0;
	size.x = hotBarMap(inx).w - 2.0/hotBarD.dpi;
	size.y = ToolbarGetHeight();

	DrawRectangle( &hotBarD, orig, size, wDrawColorBlack, DRAW_TRANSPARENT );
}


static wFont_p hotBarFp = NULL;
static wFontSize_t hotBarFs = 8;
static wWinPix_t hotBarTextHeight = 11;

static void RedrawHotBar( wControl_p dd, void * data, wWinPix_t w, wWinPix_t h  )
{
	DIST_T barHeight = (DIST_T)(wControlGetHeight( (wControl_p)hotBarD.d ) -
	                            2)/hotBarD.dpi;
	DIST_T barWidth = (DIST_T)(wControlGetWidth( (wControl_p)hotBarD.d ) -
	                           2)/hotBarD.dpi;
	DIST_T fixed_x = 0.0;

	wDrawClear( hotBarD.d );

	if(!ScrollButtonStatus()) {
		return;
	}

	fixed_x = DrawFixedElements(barWidth, barHeight);

	hotBarCurrEnd = DrawVariableElements(fixed_x, barWidth, barHeight);

	if ((hotBarCurrSelect==0 && hotBarMap_da.cnt>0 && hotBarMap(0).isFixed) ||
	    ((hotBarCurrSelect >= hotBarCurrStart)
	     && (hotBarCurrSelect < hotBarCurrEnd)) ) {
		HotBarHighlight( hotBarCurrSelect, fixed_x );
	}

	ScrollButtonUpdate();
	wPrefSetInteger( "misc", "hotbar-start", hotBarCurrStart );
}

static void ScrollButtonUpdate()
{
	if (hotBarCurrEnd < hotBarMap_da.cnt) {
		wControlActive((wControl_p)hotBarRightB, TRUE);
	} else {
		wButtonSetBusy(hotBarRightB, FALSE);
		wControlActive((wControl_p)hotBarRightB, FALSE);
	}
}

static wBool_t ScrollButtonStatus(void)
{

	if (hotBarCurrStart >0) {
		wControlActive((wControl_p)hotBarLeftB, TRUE);
	} else {
		//wButtonSetBusy(hotBarLeftB, FALSE);
		wControlActive((wControl_p)hotBarLeftB, FALSE);
	}

	if (hotBarCurrStart < 0) {
		//wButtonSetBusy(hotBarRightB, FALSE);
		wControlActive((wControl_p)hotBarRightB, FALSE);
		return FALSE;
	}

	return TRUE;
}
static void
DrawElement(hotBarMap_t* tbm, DIST_T baseX, DIST_T barWidth, DIST_T barHeight,
            DIST_T labelX)
{
	DIST_T hh = (double)(hotBarHeight + 1) / hotBarD.dpi;
	DIST_T barScale = tbm->barScale;
	DIST_T x = baseX;
	coOrd orig;

	// Calculate Y position (common to both functions)
	orig.y = barScale / hotBarD.dpi + hh / 2.0 * barScale - tbm->size.y / 2.0 -
	         tbm->orig.y;

	// Handle labels if enabled
	if (hotBarLabels) {
		orig.y += hotBarTextHeight / hotBarD.dpi * barScale;
		if (tbm->labelW > tbm->objectW) {
			x += (tbm->labelW - tbm->objectW) / 2;
		}
	}

	// Apply scaling and origin offset
	x *= barScale;
	x -= tbm->orig.x;
	orig.x = x;

	// Set up drawing context
	hotBarD.scale = barScale;
	hotBarD.size.x = barWidth * barScale;
	hotBarD.size.y = barHeight * barScale;

	// Draw the element
	tbm->proc(HB_DRAW, tbm->context, &hotBarD, &orig);

	// Add label
	AddLabel(tbm, labelX);
}

static DIST_T
DrawFixedElements(DIST_T barWidth, DIST_T barHeight)
{
	DIST_T fixed_x = 0.0;

	if (hotBarMap_da.cnt > 0 && hotBarMap(0).isFixed && hotBarCurrStart > 0) {
		hotBarMap_t* tbm = &hotBarMap(0);
		DIST_T x = 0.0;

		// Calculate fixed width and x position
		if (hotBarLabels) {
			if (tbm->labelW > tbm->objectW) {
				fixed_x = tbm->labelW;
				x += (tbm->labelW - tbm->objectW) / 2;
			} else {
				fixed_x = tbm->objectW;
			}
		} else {
			fixed_x = tbm->objectW;
		}

		// Draw the fixed element
		DrawElement(tbm, x, barWidth, barHeight, 0.0);
	}

	return fixed_x;
}

static int
DrawVariableElements(DIST_T xStart, DIST_T barWidth, DIST_T barHeight)
{
	int inx;

	for (inx = hotBarCurrStart; inx < hotBarMap_da.cnt; inx++) {
		hotBarMap_t* tbm = &hotBarMap(inx);

		// Calculate position relative to start
		DIST_T x = tbm->x - hotBarMap(hotBarCurrStart).x + xStart;

		// Check if element fits within bar width
		if (x + tbm->w > barWidth) {
			break;
		}

		// Calculate label position for AddLabel
		DIST_T labelX = tbm->x - hotBarMap(hotBarCurrStart).x + xStart;

		// Draw the variable element
		DrawElement(tbm, x, barWidth, barHeight, labelX);
	}

	return inx;
}

static void
AddLabel(hotBarMap_t* tbm, POS_T xPos)
{
	//Draw Label under icon
	coOrd orig = { .x = xPos, .y = 2.0 / hotBarD.dpi };

	if (hotBarLabels) {
		if (!hotBarFp) {
			hotBarFp = wStandardFont(F_HELV, FALSE, FALSE);
		}

		hotBarD.scale = 1.0;
		DrawString(&hotBarD,
		           orig,
		           0.0,
		           tbm->proc(HB_BARTITLE, tbm->context, NULL, NULL),
		           hotBarFp,
		           hotBarFs,
		           drawColorBlack);
	}
}


static void DoHotBarRight( void * data )
{
	DIST_T barWidth = ((DIST_T)wControlGetWidth( (wControl_p)hotBarD.d ) -
	                   2.0)/hotBarD.dpi;
	int inx = hotBarCurrStart;
	DIST_T lastX = hotBarMap(hotBarMap_da.cnt-1).x + hotBarMap(
	                       hotBarMap_da.cnt-1).w + 2.0/hotBarD.dpi;
	if (MyGetKeyState()&WKEY_SHIFT) {
		inx += hotBarMap_da.cnt/ LARGE_SCROLL_STEP;
	} else {
		inx++;
	}
	if ( inx >= hotBarMap_da.cnt ) {
		inx = hotBarMap_da.cnt-1;
	}
	DIST_T fixed_x = 0.0;
	if (hotBarCurrStart>0 && hotBarMap(0).isFixed) {
		fixed_x = hotBarMap(0).w;
	}
	while ( (inx > 1) && ((lastX - hotBarMap(inx-1).x) < (barWidth - fixed_x)) ) {
		inx--;
	}
	if ( inx != hotBarCurrStart ) {
		hotBarCurrStart = inx;
		RedrawHotBar( hotBarD.d, NULL, 0, 0 );
	}
}


static void DoHotBarLeft( void * data )
{
	int inx = hotBarCurrStart;
	if (MyGetKeyState()&WKEY_SHIFT) {
		inx -= hotBarMap_da.cnt/ LARGE_SCROLL_STEP;
	} else {
		inx --;
	}
	if ( inx < 0 ) {
		inx = 0;
	}
	if ( inx != hotBarCurrStart ) {
		hotBarCurrStart = inx;
		RedrawHotBar( hotBarD.d, NULL, 0, 0 );
	}
}


static void DoHotBarJump( int inx )
{
	DIST_T x, barWidth;

	inx -= '0';
	if (inx < 0 || inx > 9) {
		return;
	}
	if (inx == 0) {
		inx = 9;
	} else {
		inx--;
	}
	barWidth = (DIST_T)wControlGetWidth( (wControl_p)hotBarD.d )/hotBarD.dpi;
	x = (inx*(hotBarWidth-barWidth))/9.0;
	for ( inx=0; inx<hotBarMap_da.cnt; inx++ ) {
		if (x <= hotBarMap(inx).x) {
			break;
		}
	}
	if ( hotBarCurrStart != inx ) {
		hotBarCurrStart = inx;
		RedrawHotBar( NULL, NULL, 0, 0 );
	}
}


static void SelectHotBar( wControl_p d, void * context, wAction_t action,
                          wDrawPix_t w, wDrawPix_t h )
{
	int inx;
	DIST_T x;
	wWinPix_t px;
	hotBarMap_t * tbm;
	char * titleP;

	if ( hotBarMap_da.cnt <= 0 ) {
		return;
	}

	if ( ( action & 0xff ) == wActionText ) {
		int key = (int)(action >> 8);
		if ( key >= '0' && key <= '9') {
			DoHotBarJump( key );
		} else if ( key == 0x1B ) {
			ConfirmReset( FALSE );
		}
		return;
	}

	if ( (action&0xFF) ==  wActionRUp ) {
		wMenuPopupShow( hotbarContextMenu.hotbarPopupM );
		return;
	}
	inx = -1;
	x = hotBarMap(0).x;
	DIST_T fixed_x = 0.0;
	if (hotBarCurrStart>0 && hotBarMap_da.cnt>0 && hotBarMap(0).isFixed)  {
		fixed_x = hotBarMap(0).w;
		x = w/hotBarD.dpi + hotBarMap(0).x;
		if ( (x>= hotBarMap(0).x) &&
		     (x <=hotBarMap(0).w )) { inx = 0; }   //Match on fixed
	}
	if (inx<0) {																//NoMatch
		x = w/hotBarD.dpi + hotBarMap(hotBarCurrStart).x;
		for ( inx=hotBarCurrStart; inx<hotBarCurrEnd; inx++ ) {
			if ((x >= hotBarMap(inx).x + fixed_x) &&						//leave spaces between buttons
			    (x <= hotBarMap(inx).x + hotBarMap(inx).w + fixed_x )) {
				break;
			}
		}
	}

	if (inx >= hotBarCurrEnd) {
		return;
	}
	tbm = &hotBarMap(inx);
	if (inx==0) {
		px = (wWinPix_t)((tbm->x-hotBarMap(0).x)*hotBarD.dpi);
	} else {
		px = (wWinPix_t)(((tbm->x-hotBarMap(hotBarCurrStart).x)+fixed_x)*hotBarD.dpi);
	}
	px += (wWinPix_t)(tbm->w*hotBarD.dpi/2);
	titleP = tbm->proc( HB_LISTTITLE, tbm->context, NULL, NULL );
	px -= wLabelWidth( titleP ) / 2;
	//wControlSetBalloon( (wControl_p)hotBarD.d, px, -20, titleP );
	switch (action & 0xff) {
	case wActionLDown:

		if ( hotBarCurrSelect >= 0 ) {
			hotBarCurrSelect = -1;
			RedrawHotBar(hotBarD.d, NULL, 0, 0 );
		}

		tbm->proc( HB_SELECT, tbm->context, NULL, NULL );
		hotBarCurrSelect = inx;
		HotBarHighlight( hotBarCurrSelect, fixed_x );
		if (recordF) {
			fprintf( recordF, "HOTBARSELECT %s\n", tbm->proc( HB_FULLTITLE, tbm->context,
			         NULL, NULL ) );
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
	}
}


EXPORT void HotBarCancel( void )
{
	if ( hotBarCurrSelect >= 0 ) {
		hotBarCurrSelect = -1;
	}
	RedrawHotBar(hotBarD.d, NULL, 0, 0 );
}


static BOOL_T HotBarSelectPlayback( char * line )
{
	while (*line && isspace((unsigned char)*line) ) { line++; }
	for ( int inx=0; inx<hotBarMap_da.cnt; inx++ ) {
		hotBarMap_t * tbm = &hotBarMap(inx);
		if ( strcmp( tbm->proc( HB_FULLTITLE, tbm->context, NULL, NULL ), line) == 0) {
			tbm->proc( HB_SELECT, hotBarMap(inx).context, NULL, NULL );
			hotBarCurrSelect = inx;
			if ( hotBarCurrSelect < hotBarCurrStart || hotBarCurrSelect > hotBarCurrEnd ) {
				// Shift the hotbar so the selected object is visible
				hotBarCurrStart = hotBarCurrSelect;
				RedrawHotBar( hotBarD.d, NULL, 0, 0 );
			}
			DIST_T fixed_x = 0.0;
			if ( inx > 0 && hotBarCurrStart > 0 && hotBarMap(0).isFixed ) {
				// Leave room for Flex object
				fixed_x = hotBarMap(0).w;
			}
			HotBarHighlight( hotBarCurrSelect, fixed_x );
			FakeDownMouseState();
			return TRUE;
		}
	}
	return FALSE;
}


static void HotbarJump( int inx, const char * name, void * arg )
{
	hotBarCurrStart = (int)VP2L(arg);
	RedrawHotBar( hotBarD.d, NULL, 0, 0 );
}


static BOOL_T SetHotBarScale( char * line )
{
	curBarScale = atof( line + 9 );
	return TRUE;
}

void AddToContextMenu(char* contentsLabel, BOOL_T isFixed)
{
	if (contentsLabel
		&& strncmp(contentsLabel, hotbarContextMenu.curContentsLabel, sizeof (hotbarContextMenu.curContentsLabel)) != 0
		&& !isFixed) {
		wMenuListAdd(hotbarContextMenu.hotBarML, hotbarContextMenu.hotBarMLcnt++, contentsLabel, I2VP(hotBarMap_da.cnt));
		strncpy(hotbarContextMenu.curContentsLabel, contentsLabel, sizeof(hotbarContextMenu.curContentsLabel) - 1);
	}
}

EXPORT
void AddHotBarElement(
        char * contentsLabel,
        coOrd size,
        coOrd orig,
        BOOL_T isTrack,
        BOOL_T isFixed,
        DIST_T barScale,
        void * context,
        hotBarProc_t proc_p )
{
	hotBarMap_t * tbm;
	coOrd textsize;

	AddToContextMenu(contentsLabel, isFixed);
	if (barScale <= 0) {
		if (!isTrack) {
			barScale = size.y/(((double)hotBarHeight-2.0)/hotBarD.dpi);
		} else {
			barScale = (trackGauge>0.1)?trackGauge*(36-hotBarHeight/2):10.0;
			if (size.y/barScale > ((double)hotBarHeight-2.0)/hotBarD.dpi) {
				barScale = (size.y+2.0*trackGauge)/(((double)hotBarHeight-2.0)/hotBarD.dpi);
			}
		}
	}
	DYNARR_APPEND( hotBarMap_t, hotBarMap_da, 10 );
	tbm = &hotBarMap(hotBarMap_da.cnt-1);
	if (barScale < 1) {
		barScale = 1;
	}
	if (size.x > barScale) {
		barScale = size.x;
	}
	tbm->context = context;
	tbm->size = size;
	tbm->orig = orig;
	tbm->proc = proc_p;
	tbm->barScale = barScale;
	tbm->isFixed = isFixed;
	tbm->w = tbm->objectW = size.x/barScale + 5.0/hotBarD.dpi;
	tbm->labelW = 0;
	tbm->x = hotBarWidth;
	if ( hotBarLabels ) {
		DrawTextSize( &hotBarD, proc_p( HB_BARTITLE, context, NULL, NULL), hotBarFp,
		              hotBarFs, FALSE, &textsize );
		tbm->labelW = textsize.x+5/hotBarD.dpi;
		if ( tbm->labelW > tbm->w ) {
			tbm->w = tbm->labelW;
		}
	}
	hotBarWidth += tbm->w + 2/hotBarD.dpi;
}

static void
ClearContextMenu()
{
	hotbarContextMenu.hotBarMLcnt = 0;
	wMenuListClear(hotbarContextMenu.hotBarML);

}

EXPORT void ChangeHotBar( long changes )
{
	if ( (changes&(CHANGE_SCALE|CHANGE_PARAMS|CHANGE_TOOLBAR)) == 0 ) {
		return;
	}

	if (hotBarLeftB != NULL && curScaleName) {
		static long programModeOld = 0;
		ClearContextMenu();

		hotBarWidth = 0.0;
		DYNARR_RESET( hotBarMap_t, hotBarMap_da );
		hotbarContextMenu.curContentsLabel[0] = '\0';
		if ( programMode == MODE_DESIGN ) {
			if (showFlexTrack) {
				AddHotBarCornu();
			}
			AddHotBarTurnouts();
			AddHotBarStructures();
		} else {
			AddHotBarCarDesc();
		}

		if ( programModeOld != programMode ) {
			hotBarCurrSelects[0] = hotBarCurrSelects[1] = -1;
			programModeOld = programMode;
		}
		if (hotBarMap_da.cnt > 0 && (hotBarCurrStart >= hotBarMap_da.cnt
		                             ||hotBarCurrStart < 0)) {
			hotBarCurrStart = 0;
		}
		RedrawHotBar( NULL, NULL, 0, 0 );
	}
}

static void
CreateContextMenu(void)
{
	hotbarContextMenu.hotbarPopupM = MenuRegister("Hotbar Select");
	hotbarContextMenu.hotBarML = wMenuListCreate(hotbarContextMenu.hotbarPopupM, "", -1, HotbarJump);
}

EXPORT void InitHotBar( void )
{
	long v;

	AddParam( "BARSCALE", SetHotBarScale );
	AddPlaybackProc( "HOTBARSELECT", (playbackProc_p)HotBarSelectPlayback, NULL );

	RegisterChangeNotification( ChangeHotBar );
	
	wPrefGetInteger( "misc", "hotbar-start", &v, hotBarCurrStart );
	hotBarCurrStart = (int)v;
	
	CreateContextMenu();

	hotBarHeight = hotBarDrawHeight[iconSize];
	hotBarFs = hotBarFontSize[iconSize];
}

EXPORT void LayoutHotBar( void * redraw )
{
	wWinPix_t buttonHeight, buttonWidth, winWidth, winHeight;
	wWinPix_t hbHeight = hotBarHeight;
	BOOL_T initialize = FALSE;

	wWinGetSize( mainW, &winWidth, &winHeight );
	hotBarTextHeight = (wWinPix_t)round(wMessageGetHeight(0L) * (0.6 + 0.4 *
	                                    (double)iconSize));

	if ( hotBarLabels) {
		hbHeight += hotBarTextHeight;
	}
	if (hotBarLeftB == NULL) {
		if (winWidth < 50) {
			return;
		}

		hotBarLeftB = wButtonCreate(mainW, 0, 0, "hotBarLeft", NULL,
			BO_REPEAT, 0, DoHotBarLeft, NULL);

		hotBarRightB = wButtonCreate(mainW, 0, 0, "hotBarRight", NULL,
			BO_REPEAT, 0, DoHotBarRight, NULL);

		hotBarD.d = wDrawCreate(mainW, 0, 0, "hotBarDraw", 0, -1,
			hbHeight + 2, NULL, RedrawHotBar, SelectHotBar);

		hotBarD.dpi = wDrawGetDPI(hotBarD.d);
		hotBarD.scale = 1.0;

		wSetCursor(hotBarD.d, wCursorNormal);
		initialize = TRUE;
	}
	//wDrawSetSize( hotBarD.d, /*winWidth - 20 - buttonWidth * 2 */ -1, hbHeight + 2, redraw);

	wWinPix_t width, height;
	wDrawGetSize(hotBarD.d, &width, &height);

	hotBarD.size.x = width;
	hotBarD.size.y = height;

	//hotBarD.size.x = ((double)(winWidth-20
	//                           -buttonWidth*2))/hotBarD.dpi*hotBarD.scale;
	//hotBarD.size.y = (double)
	//                 hotBarHeight/hotBarD.dpi*hotBarD.scale;  //Exclude Label from calc

	ShowHotBar(TRUE);

	if (initialize) {
		ChangeHotBar( CHANGE_PARAMS );
	} else if (!redraw) {
		RedrawHotBar( NULL, NULL, 0, 0 );
	}
	ToolbarSetHeight( ToolbarGetHeight() + hbHeight+3 );
}


	void HideHotBar(void)
	{
		ShowHotBar(TRUE);
	}

static void ShowHotBar( wBool_t show )
{
	if (hotBarLeftB != NULL) {
		wControlShow( (wControl_p)hotBarLeftB, show );
		wControlShow( (wControl_p)hotBarRightB, show );
		wControlShow( (wControl_p)hotBarD.d, show );
	}
}
