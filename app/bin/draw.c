/** \file draw.c
 * Basic drawing functions.
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

#include <time.h>

#include "draw.h"
#include "common-ui.h"
#include "cselect.h"
#include "custom.h"
#include "fileio.h"
#include "icons.h"
#include "layout.h"
#include "mapwindow.h"
#include "misc.h"
#include "track.h"
#include "wlib.h"

// #define PERFORMANCE_TEST		/**< set to measure performance of redraw */

#undef BBORDER
#undef LBORDER
static int lborder, bborder;
static int charWidth;

/* Accessor functions used by ddrawprim.c and drawruler.c */
int GetLBorder(void)  { return lborder;   }
int GetBBorder(void)  { return bborder;   }
int DrawGetCharWidth(void) { return charWidth;  }

/* Functions moved to drawruler.c */
void DrawRoomWalls(wBool_t drawBackground);
void DrawRulers(void);
void DrawMarkers(void);

/* Defined in ddrawprim.c — wires CoOrd2Pix/Pix2CoOrd into mainD/tempD */
void InitDrawCmds(void);

/* SetRulerFont is defined in drawruler.c */
void SetRulerFont(wFont_p fp, DIST_T fontSize);

#define NUM_INFOCTL (4)
#define MAXLABELCHARS 5

EXPORT wIndex_t panCmdInx;

static void ConstraintOrig(coOrd *, coOrd, BOOL_T, BOOL_T);
static void DoMouse(wAction_t action, coOrd pos);

static void DoZoom(void *pScaleVP);

/** @logcmd @showrefby `pan=n` `draw.c` */
EXPORT int log_pan = 0;
/** @logcmd @showrefby `zoom=n` `draw.c` */
static int log_zoom = 0;
/** @logcmd @showrefby `mouse=n` `draw.c` */
static int log_mouse = 0;
/** @logcmd @showrefby `redraw=n` `draw.c` */
static int log_redraw = 0;
/** @logcmd @showrefby `mapsize=n` `draw.c` */
static int log_size = 0;
/** @logcmd @showrefby `timemainredraw=n` `draw.c` */
static int log_timemainredraw = 0;

/****************************************************************************
 *
 * EXPORTED VARIABLES
 *
 */

EXPORT long drawCount;
EXPORT BOOL_T drawEnable = TRUE;
EXPORT long currRedraw = 0;
EXPORT long constrainMain = 0;
EXPORT long liveMap = 0;
EXPORT long descriptionFontSize = 72;

EXPORT coOrd panCenter;
EXPORT coOrd menuPos;

EXPORT wDrawColor drawColorBlack;
EXPORT wDrawColor drawColorWhite;
EXPORT wDrawColor drawColorRed;
EXPORT wDrawColor drawColorBlue;
EXPORT wDrawColor drawColorGreen;
EXPORT wDrawColor drawColorAqua;
EXPORT wDrawColor drawColorDkRed;
EXPORT wDrawColor drawColorDkBlue;
EXPORT wDrawColor drawColorDkGreen;
EXPORT wDrawColor drawColorDkAqua;
EXPORT wDrawColor drawColorPreviewSelected;
EXPORT wDrawColor drawColorPreviewUnselected;
EXPORT wDrawColor drawColorPowderedBlue;
EXPORT wDrawColor drawColorPurple;
EXPORT wDrawColor drawColorMagenta;
EXPORT wDrawColor drawColorGold;
EXPORT wDrawColor drawColorGrey10;
EXPORT wDrawColor drawColorGrey20;
EXPORT wDrawColor drawColorGrey30;
EXPORT wDrawColor drawColorGrey40;
EXPORT wDrawColor drawColorGrey50;
EXPORT wDrawColor drawColorGrey60;
EXPORT wDrawColor drawColorGrey70;
EXPORT wDrawColor drawColorGrey80;
EXPORT wDrawColor drawColorGrey90;

EXPORT DIST_T pixelBins = 80;

/****************************************************************************
 *
 * LOCAL VARIABLES
 *
 */

EXPORT BOOL_T magneticSnap;

EXPORT wDrawColor markerColor;
EXPORT wDrawColor borderColor;
EXPORT wDrawColor crossMajorColor;
EXPORT wDrawColor crossMinorColor;
EXPORT wDrawColor selectedColor;
EXPORT wDrawColor normalColor;
EXPORT wDrawColor elevColorIgnore;
EXPORT wDrawColor elevColorDefined;
EXPORT wDrawColor profilePathColor;
EXPORT wDrawColor exceptionColor;

DIST_T closeDist = 0.100;

EXPORT coOrd oldMarker = {0.0, 0.0};

EXPORT long dragPixels = 20;
EXPORT long dragTimeout = 500;
EXPORT long autoPan = 0;
EXPORT BOOL_T inError = FALSE;

typedef enum {
	mouseNone,
	mouseLeft,
	mouseRight,
	mouseLeftPending
} mouseState_e;
static mouseState_e mouseState;
static wDrawPix_t mousePositionx,
       mousePositiony; /**< position of mouse pointer */

static int delayUpdate = 1;

static struct {
	char *name;
	DIST_T value;
	wMenuRadio_p pdRadio;
	wMenuRadio_p btRadio;
	wMenuRadio_p ctxRadio1;
	wMenuRadio_p panRadio;
} zoomList[] = {
	{"1:10", 1.0 / 10.0}, {"1:9", 1.0 / 9.0},     {"1:8", 1.0 / 8.0},
	{"1:7", 1.0 / 7.0},   {"1:6", 1.0 / 6.0},     {"1:5", 1.0 / 5.0},
	{"1:4", 1.0 / 4.0},   {"1:3.5", 1.0 / 3.5},   {"1:3", 1.0 / 3.0},
	{"1:2.5", 1.0 / 2.5}, {"1:2", 1.0 / 2.0},     {"1:1.75", 1.0 / 1.75},
	{"1:1.5", 1.0 / 1.5}, {"1:1.25", 1.0 / 1.25}, {"1:1", 1.0},
	{"1.25:1", 1.25},     {"1.5:1", 1.5},         {"1.75:1", 1.75},
	{"2:1", 2.0},         {"2.5:1", 2.5},         {"3:1", 3.0},
	{"3.5:1", 3.5},       {"4:1", 4.0},           {"5:1", 5.0},
	{"6:1", 6.0},         {"7:1", 7.0},           {"8:1", 8.0},
	{"9:1", 9.0},         {"10:1", 10.0},         {"12:1", 12.0},
	{"14:1", 14.0},       {"16:1", 16.0},         {"18:1", 18.0},
	{"20:1", 20.0},       {"24:1", 24.0},         {"28:1", 28.0},
	{"32:1", 32.0},       {"36:1", 36.0},         {"40:1", 40.0},
	{"48:1", 48.0},       {"56:1", 56.0},         {"64:1", 64.0},
	{"80:1", 80.0},       {"96:1", 96.0},         {"112:1", 112.0},
	{"128:1", 128.0},     {"160:1", 160.0},       {"192:1", 192.0},
	{"224:1", 224.0},     {"256:1", 256.0},       {"320:1", 320.0},
	{"384:1", 384.0},     {"448:1", 448.0},       {"512:1", 512.0},
	{"640:1", 640.0},     {"768:1", 768.0},       {"896:1", 896.0},
	{"1024:1", 1024.0},
};

EXPORT drawCmd_t mainD = {
	NULL,  &screenDrawFuncs, DC_TICKS,           INIT_MAIN_SCALE, 0.0,
	{0.0, 0.0}, {0.0, 0.0}, NULL /* Pix2CoOrd */, NULL /* CoOrd2Pix */, 96.0
};

EXPORT drawCmd_t tempD = {
	NULL,  &screenDrawFuncs, DC_TICKS | DC_TEMP,  INIT_MAIN_SCALE, 0.0,
	{0.0, 0.0}, {0.0, 0.0}, NULL /* Pix2CoOrd */, NULL /* CoOrd2Pix */, 96.0
};

/*****************************************************************************
 *
 * MAIN AND MAP WINDOW DEFINITIONS
 *
 */

EXPORT BOOL_T SetRoomSize(coOrd size)
{
	LOG(log_size, 2,
	    ("SetRoomSize NEW:%0.3fx%0.3f OLD:%0.3fx%0.3f\n", size.x, size.y,
	     mapD.size.x, mapD.size.y));
	if (size.x < 1.0) {
		size.x = 1.0;
	}
	if (size.y < 1.0) {
		size.y = 1.0;
	}
	if (mapD.size.x == size.x && mapD.size.y == size.y) {
		return TRUE;
	}
	mapD.size = size;
	SetLayoutRoomSize(size);
	wPrefSetFloat("draw", "roomsizeX", mapD.size.x);
	wPrefSetFloat("draw", "roomsizeY", mapD.size.y);
	return TRUE;
}

EXPORT void SetMainSize(void)
{
	wWinPix_t ww, hh;
	DIST_T w, h;
	wDrawGetSize(mainD.d, &ww, &hh);
	ww -= lborder + RBORDER;
	hh -= bborder + TBORDER;

	w = ww / mainD.dpi;
	h = hh / mainD.dpi;
	mainD.size.x = w * mainD.scale;
	mainD.size.y = h * mainD.scale;
	tempD.size = mainD.size;
}

/* Draw all temp-surface content: current command feedback (clipped to the
 * inner drawing area) followed by markers, rulers and playback cursor
 * (unclipped so they can extend into the border margins).
 *
 * The caller must already be in temp mode (wDrawSetTempMode TRUE).
 */
static void DrawTempContent(void)
{
	wWinPix_t totalW, totalH;

	wDrawStart(tempD.d);
	/* Clip command drawing to the inner area (exclude ruler margins). */
	wDrawGetSize(tempD.d, &totalW, &totalH);
	wDrawClip(tempD.d,
	          (wDrawPix_t)lborder,
	          (wDrawPix_t)bborder,
	          (wDrawPix_t)(totalW - lborder - RBORDER),
	          (wDrawPix_t)(totalH - bborder - TBORDER));

	DoCurCommand(C_REDRAW, zero);

	/* Reset clip so markers and rulers can draw into the margins. */
	wDrawClipClear(tempD.d);

	DrawMarkers();
	RulerRedraw(FALSE);
	wDrawFinish(tempD.d);

	/* Outside the batch: RedrawPlaybackCursor() ends with wFlush(), which
	 * pumps the GTK main loop synchronously. On a cold first paint there can
	 * be a pending window layout/configure event still queued, and pumping
	 * the loop from inside an active wDrawStart/wDrawFinish batch lets that
	 * event's handler reenter drawing (MainLayout -> MainRedraw -> wDrawStart)
	 * before this batch closes, corrupting the single shared cairoCtx (GTK3
	 * issue #21). RedrawPlaybackCursor()'s own drawing doesn't need an
	 * open batch -- prepare_drawing() (gtk3lib/drawcairo.c) already falls
	 * back to a one-shot cairo context per call when none is active. */
	RedrawPlaybackCursor(); /* If in playback */
}

/* Update temp_surface after executing a command
 */
EXPORT void TempRedraw(void)
{

	static int cTR = 0;
	LOG(log_redraw, 2, ("TempRedraw: %d\n", cTR++));
	if (wDrawDoTempDraw == FALSE) {
		/* Remove this after windows supports GTK */
		MainRedraw(); /* TempRedraw - windows */
	} else {
		wDrawDelayUpdate(tempD.d, TRUE);
		wDrawSetTempMode(tempD.d, TRUE);
		DrawTempContent();
		wDrawSetTempMode(tempD.d, FALSE);
		wDrawDelayUpdate(tempD.d, FALSE);
	}
}

/**
 * Calculate position and size of background bitmap
 *
 * \param [in]	    drawP   destination drawing area
 * \param [in]		origX	x origin of drawing area
 * \param [in]		origY	y origin of drawing area
 * \param [out]     posX    x position of bitmap
 * \param [out]     posY    y position of bitmap
 * \param [out]     pWidth  width of bitmap in destination coordinates
 *
 */

void TranslateBackground(drawCmd_p drawP, POS_T origX, POS_T origY,
                         wWinPix_t *posX, wWinPix_t *posY, wWinPix_t *pWidth)
{
	coOrd back_pos = GetLayoutBackGroundPos();

	*pWidth = (wWinPix_t)(GetLayoutBackGroundSize() / drawP->scale * drawP->dpi);

	*posX = (wWinPix_t)((back_pos.x - origX) / drawP->scale * drawP->dpi);
	*posY = (wWinPix_t)((back_pos.y - origY) / drawP->scale * drawP->dpi);
}

/*
 * Redraw contents on main window
 */
EXPORT void MainRedraw(void)
{
	coOrd orig, size;
	wWinPix_t totalW, totalH;

	static int cMR = 0;
	LOG(log_redraw, 1,
	    ("MainRedraw: %d %0.1fx%0.1f\n", cMR++, mainD.size.x, mainD.size.y));

	if(mainD.size.x == 0.0 && mainD.size.y == 0.0 ) {
		LogPrintf("MainRedraw: return early as drawing area size is 0,0");
		return;
	}

	unsigned long time0 = wGetTimer();
	if (delayUpdate) {
		wDrawDelayUpdate(mainD.d, TRUE);
	}

	wDrawSetTempMode(mainD.d, FALSE);
	wDrawClear(mainD.d);

	/*
	 * Phase 1: Background (unclipped)
	 * Grey surround, white room area, background image, snap grid
	 */
	orig = mainD.orig;
	size = mainD.size;
	orig.x -= lborder / mainD.dpi * mainD.scale;
	orig.y -= bborder / mainD.dpi * mainD.scale;

	// measure redraw of main canvas
#ifdef PERFORMANCE_TEST
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
#endif

	wDrawStart(mainD.d);

	DrawRoomWalls(TRUE);

	if (GetLayoutBackGroundScreen() < 100.0 && GetLayoutBackGroundVisible()) {
		wWinPix_t bitmapPosX;
		wWinPix_t bitmapPosY;
		wWinPix_t bitmapWidth;

		TranslateBackground(&mainD, orig.x, orig.y, &bitmapPosX, &bitmapPosY,
		                    &bitmapWidth);

		wDrawPix_t rx, ry, rw, rh;
		rx = lborder + (0.0 - mainD.orig.x) * mainD.dpi / mainD.scale;
		ry = (0.0 - mainD.orig.y) * mainD.dpi / mainD.scale;
		rw = mapD.size.x * mainD.dpi / mainD.scale;
		rh = mapD.size.y * mainD.dpi / mainD.scale;

		wDrawShowBackground(mainD.d, bitmapPosX, bitmapPosY, bitmapWidth,
		                    GetLayoutBackGroundAngle(),
		                    GetLayoutBackGroundScreen(),
		                    rx, ry, rw, rh);
	}

	DrawSnapGrid(&mainD, mapD.size, TRUE);

	/*
	 * Phase 2: Drawing content (clipped to inner area)
	 * Tracks and room border polygon must not overwrite the ruler margins.
	 */


	orig = mainD.orig;
	size = mainD.size;
	orig.x -= RBORDER / mainD.dpi * mainD.scale;
	orig.y -= bborder / mainD.dpi * mainD.scale;
	size.x += (RBORDER + lborder) / mainD.dpi * mainD.scale;
	size.y += (bborder + TBORDER) / mainD.dpi * mainD.scale;

	wDrawGetSize(mainD.d, &totalW, &totalH);
	wDrawClip(mainD.d,
	          (wDrawPix_t)lborder,
	          (wDrawPix_t)bborder,
	          (wDrawPix_t)(totalW - lborder - RBORDER),
	          (wDrawPix_t)(totalH - bborder - TBORDER));
	DrawTracks(&mainD, mainD.scale, orig, size);

	DrawRoomWalls(FALSE); /* Room border polygon only — clipped */

	/*
	 * Phase 3: Rulers and overlays (unclipped)
	 * Reset clip to full canvas so rulers draw into the margins.
	 */
	wDrawClipClear(mainD.d);

	/*
	 * make sure that we have a fresh cairo context having default settings
	 */

	wDrawFinish(mainD.d);
	wDrawStart((mainD.d));

	DrawRulers();

	wDrawFinish(mainD.d);

#ifdef PERFORMANCE_TEST
	// end measuring main canvas
	clock_gettime(CLOCK_MONOTONIC, &end);
	double time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec -
	                    start.tv_nsec) / 1e9;

	LogPrintf("MainRedraw No. %d took %f sec\n", currRedraw, time_spent);
#endif
	currRedraw++;

	InfoScale();
	LOG(log_timemainredraw, 1,
	    ("MainRedraw time = %lu mS\n", wGetTimer() - time0));
	/* Temp-surface content (command feedback, markers, rulers) */

	wDrawSetTempMode(tempD.d, TRUE);
	DrawTempContent();
	wDrawSetTempMode(tempD.d, FALSE);
	wDrawDelayUpdate(mainD.d, FALSE);

}

/*
 * Layout main window in response to Pan/Zoom
 *
 * \param bRedraw Redraw mainD
 * \param bNoBorder Don't allow mainD.orig to go negative
 */
EXPORT void MainLayout(wBool_t bRedraw, wBool_t bNoBorder)
{
	DIST_T t1;
	if (inPlaybackQuit) {
		return;
	}
	static int cML = 0;
	LOG(log_redraw, 1,
	    ("MainLayout: %d %s %s\n", cML++, bRedraw ? "RDW" : "---",
	     bNoBorder ? "NBR" : "---"));

	SetMainSize();
	t1 = mainD.dpi / mainD.scale;
	if (units == UNITS_ENGLISH) {
		t1 /= 2.0;
		for (pixelBins = 0.25; pixelBins < t1; pixelBins *= 2.0)
			;
	} else {
		t1 /= 2.0;
		pixelBins = 0.127;
		while (1) {
			pixelBins *= 2.0;
			if (pixelBins >= t1) {
				break;
			}
			pixelBins *= 2.0;
			if (pixelBins >= t1) {
				break;
			}
			pixelBins *= 2.5;
			if (pixelBins >= t1) {
				break;
			}
		}
	}
	LOG(log_pan, 2, ("PixelBins=%0.6f\n", pixelBins));
	ConstraintOrig(&mainD.orig, mainD.size, bNoBorder, TRUE);
	tempD.orig = mainD.orig;
	tempD.size = mainD.size;
	mainCenter.x = mainD.orig.x + mainD.size.x / 2.0;
	mainCenter.y = mainD.orig.y + mainD.size.y / 2.0;
	MapDrawBoundingBox(TRUE);

	if (bRedraw) {
		MainRedraw();
	}

	if (bRedraw && wDrawDoTempDraw) { /* Temporary until mswlib supports TempDraw */
		wAction_t action = wActionMove;
		coOrd pos;
		if (mouseState == mouseLeft) {
			action = wActionLDrag;
		}
		if (mouseState == mouseRight) {
			action = wActionRDrag;
		}
		mainD.Pix2CoOrd(&mainD, mousePositionx, mousePositiony, &pos);
		/* Prevent recursion if curCommand calls MainLayout when action is a Move
		   or Drag, e.g. CmdPan */
		static int iRecursion = 0;
		iRecursion++;
		if (iRecursion == 1) {
			DoMouse(action, pos);
		}
		iRecursion--;
	}
}

/**
 * The wlib event handler for the main window.
 *
 * \param win wlib window information
 * \param e the wlib event
 * \param refresh refresh data (unused)
 * \param data additional data (unused)
 */

wBool_t MainProc(wControl_p win, winProcEvent e, void *refresh, void *data)
{
	static int cMP = 0;
	wWinPix_t width, height;
	switch (e) {

	case wResize_e:
		if (mainD.d == NULL) {
			return FALSE;
		}
		wWinGetSize(mainW, &width, &height);
		LOG(log_redraw, 1,
		    ("MainProc/Resize: %d %s %ld %ld\n", cMP++,
		     refresh == NULL ? "RDW" : "---", width, height));
		if (height >= 0) {
			wBool_t bTemp = wDrawSetTempMode(mainD.d, FALSE);
			if (bTemp) {
				printf("MainProc TempMode\n");
			}

			/* Remember the center of the current view BEFORE the size changes */
			coOrd oldCenter;
			oldCenter.x = mainD.orig.x + mainD.size.x / 2.0;
			oldCenter.y = mainD.orig.y + mainD.size.y / 2.0;

			SetMainSize();

			/* Recompute origin so the viewport stays centered on the same point */
			mainD.orig.x = oldCenter.x - mainD.size.x / 2.0;
			mainD.orig.y = oldCenter.y - mainD.size.y / 2.0;

			panCenter = oldCenter;

			LOG(log_pan, 2,
			    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
			MainLayout(!refresh, constrainMain != 0); /* MainProc: wResize_e event */
			wPrefSetInteger("draw", "mainwidth", (int)width);
			wPrefSetInteger("draw", "mainheight", (int)height);
			wDrawSetTempMode(mainD.d, bTemp);
		} else {
			MapDrawBoundingBox(TRUE);
		}
		break;
	case wState_e:
		wPrefSetInteger("draw", "maximized", wWinIsMaximized(win));
		break;
	case wQuit_e:
		CleanupCustom();
		break;
	case wClose_e:
		/* shutdown the application via "close window" button */
		DoQuit(NULL);
		/* if DoQuit returns, the user decided to not terminate the application */
		/* tell GTK to not proceed with the delete further, keeping the window open */
		return (TRUE);
		break;
	default:
		break;
	}
	return (FALSE);
}

EXPORT void DoRedraw(void)
{
	LOG(log_size, 2, ("DoRedraw\n"));

	MainRedraw(); /* DoRedraw */
}

/*****************************************************************************
 *
 * ZOOM and PAN
 *
 */

EXPORT coOrd mainCenter;

static void ConstraintOrig(coOrd * orig, coOrd size, wBool_t bNoBorder,
                           wBool_t bRound)
{
	LOG(log_pan, 2,
	    ("ConstraintOrig [ %0.6f, %0.6f ] RoomSize(%0.3f %0.3f), "
	     "WxH=%0.3fx%0.3f",
	     orig->x, orig->y, mapD.size.x, mapD.size.y, size.x, size.y))

	coOrd bound = zero;

	if (!bNoBorder) {
		bound.x = size.x / 2;
		bound.y = size.y / 2;
	}

	if (orig->x > 0.0) {
		if ((orig->x + size.x) > (mapD.size.x + bound.x)) {
			orig->x = mapD.size.x - size.x + bound.x;
		}
	}

	if (orig->x < (0 - bound.x)) {
		orig->x = 0 - bound.x;
	}

	if (orig->y > 0.0) {
		if ((orig->y + size.y) > (mapD.size.y + bound.y)) {
			orig->y = mapD.size.y - size.y + bound.y;
		}
	}

	if (orig->y < (0 - bound.y)) {
		orig->y = 0 - bound.y;
	}

	if (bRound) {
		orig->x = round(orig->x * pixelBins) / pixelBins;
		orig->y = round(orig->y * pixelBins) / pixelBins;
	}
	LOG(log_pan, 2, (" = [ %0.6f %0.6f ]\n", orig->x, orig->y))
}

/**
 * Initialize the menu for setting zoom factors.
 *
 * \param[in] zoomM		Menu to which radio button is added
 * \param[in] zoomSubM	Second menu to which radio button is added, ignored if NULL
 * \param[in] ctxMenu1
 * \param[in] panMenu
 *
 */

EXPORT void InitCmdZoom(wMenu_p zoomM, wMenu_p zoomSubM, wMenu_p ctxMenu1,
                        wMenu_p panMenu)
{
	int inx;

	for (inx = 0; inx < COUNT(zoomList); inx++) {
		if (zoomM) {
			zoomList[inx].btRadio =
			        wMenuRadioCreate(zoomM, "cmdZoom", zoomList[inx].name, 0, DoZoom,
			                         (&(zoomList[inx].value)));
		}
		if (zoomSubM) {
			zoomList[inx].pdRadio =
			        wMenuRadioCreate(zoomSubM, "cmdZoom", zoomList[inx].name, 0, DoZoom,
			                         (&(zoomList[inx].value)));
		}
		if (panMenu) {
			zoomList[inx].panRadio =
			        wMenuRadioCreate(panMenu, "cmdZoom", zoomList[inx].name, 0, DoZoom,
			                         (&(zoomList[inx].value)));
		}
		if (ctxMenu1) {
			zoomList[inx].ctxRadio1 =
			        wMenuRadioCreate(ctxMenu1, "cmdZoom", zoomList[inx].name, 0, DoZoom,
			                         (&(zoomList[inx].value)));
		}
	}
}

/**
 * Set radio button(s) corresponding to current scale.
 *
 * \param[in] scale		current scale
 *
 */

static void SetZoomRadio(DIST_T scale)
{
	int inx;

	for (inx = 0; inx < COUNT(zoomList); inx++) {
		if (scale == zoomList[inx].value) {
			if (zoomList[inx].btRadio) {
				wMenuRadioSetActive(zoomList[inx].btRadio);
			}
			if (zoomList[inx].pdRadio) {
				wMenuRadioSetActive(zoomList[inx].pdRadio);
			}
			if (zoomList[inx].ctxRadio1) {
				wMenuRadioSetActive(zoomList[inx].ctxRadio1);
			}
			if (zoomList[inx].panRadio) {
				wMenuRadioSetActive(zoomList[inx].panRadio);
			}
			/* activate / deactivate zoom buttons when appropriate */
			wControlLinkedActive((wControl_p)zoomUpB, (inx != 0));
			wControlLinkedActive((wControl_p)zoomDownB,
			                     (inx < (COUNT(zoomList) - 1)));
		}
	}
}

/**
 * Find current scale
 *
 * \param[in] scale current scale
 * \return index in scale table or -1 if error
 *
 */

static int ScaleInx(DIST_T scale)
{
	int inx;

	for (inx = 0; inx < COUNT(zoomList); inx++) {
		if (scale == zoomList[inx].value) {
			return inx;
		}
	}
	return -1;
}

/*
 * Find Nearest Scale
 */

static int NearestScaleInx(DIST_T scale, BOOL_T larger)
{
	int inx;

	if (larger) {
		for (inx = 0; inx < COUNT(zoomList) - 1; inx++) {
			if (scale == zoomList[inx].value) {
				return inx;
			}
			if (scale < zoomList[inx].value) {
				return inx;
			}
		}
		return inx - 1;
	} else {
		for (inx = COUNT(zoomList) - 1; inx >= 0; inx--) {
			if (scale == zoomList[inx].value) {
				return inx;
			}
			if (scale > zoomList[inx].value) {
				return inx;
			}
		}
	}
	return 0;
}

/**
 * Set up for new drawing scale. After the scale was changed, eg. via zoom
 * button, everything is set up for the new scale.
 *
 * \param scale IN new scale
 */

void DoNewScale(DIST_T scale)
{
	char tmp[20];

	static BOOL_T in_use = 0; /* lock for recursion to avoid additional setting */

	if (in_use) {
		return;
	}

	in_use = 1; /* set lock */

	if (scale > MAX_MAIN_SCALE) {
		scale = MAX_MAIN_SCALE;
	}

	if (scale < zoomList[0].value) {
		scale = zoomList[0].value;
	}

	tempD.scale = mainD.scale = scale;
	mainD.dpi = wDrawGetDPI(mainD.d);
	tempD.dpi = mainD.dpi;

	SetZoomRadio(scale); /* This routine causes a re-drive of the DoNewScale. */

	InfoScale();
	SetMainSize();
	PanHere(I2VP(1));
	LOG(log_zoom, 1, ("center = [%0.3f %0.3f]\n", mainCenter.x, mainCenter.y))
	snprintf(tmp, sizeof(tmp), "%0.3f", mainD.scale);
	wPrefSetString("draw", "zoom", tmp);
	if (recordF) {
		fprintf(recordF, "ORIG %0.3f %0.3f %0.3f\n", mainD.scale, mainD.orig.x,
		        mainD.orig.y);
	}
	in_use = 0; /* release lock */
}

/**
 * User selected zoom in, via mouse wheel, button or pulldown.
 *
 * \param mode IN FALSE if zoom button was activated, TRUE if activated via
 * popup or mousewheel
 */

EXPORT void DoZoomUp(void *mode)
{
	long newScale;
	int i;

	LOG(log_zoom, 2, ("DoZoomUp KS:%x\n", MyGetKeyState()));
	if (mode != NULL || (MyGetKeyState() & WKEY_SHIFT) == 0) {
		i = ScaleInx(mainD.scale);
		if (i < 0) {
			i = NearestScaleInx(mainD.scale, FALSE);
		}
		/*
		 * Zooming into macro mode happens when we are at scale 1:1.
		 * To jump into macro mode, the CTRL-key has to be pressed and held.
		 */
		if (mainD.scale != 1.0 ||
		    (mainD.scale == 1.0 && (MyGetKeyState() & WKEY_CTRL))) {
			if (i) {
				if (mainD.scale <= 1.0) {
					InfoMessage(_("Macro Zoom Mode"));
				} else {
					InfoMessage("");
				}
				DoNewScale(zoomList[i - 1].value);

			} else {
				InfoMessage("Minimum Macro Zoom");
			}
		} else {
			InfoMessage(_("Scale 1:1 - Use Ctrl+ to go to Macro Zoom Mode"));
		}
	} else if ((MyGetKeyState() & WKEY_CTRL) == 0) {
		wPrefGetInteger("misc", "zoomin", &newScale, 4);
		InfoMessage(_(
		                    "Preset Zoom In Value selected. Shift+Ctrl+PageDwn to reset value"));
		DoNewScale(newScale);
	} else {
		wPrefSetInteger("misc", "zoomin", (long)mainD.scale);
		InfoMessage(_("Zoom In Program Value %ld:1, Shift+PageDwn to use"),
		            (long)mainD.scale);
	}
}

/*
 * Mode - 0 = Extents are Map size
 * Mode - 1 = Extents are Selected size
 */
EXPORT void DoZoomExtents(void *mode)
{

	DIST_T scale_x, scale_y;
	if (1 == VP2L(mode)) {
		if (selectedTrackCount == 0) {
			return;
		}
		track_p trk = NULL;
		coOrd bot = {0.0, 0.0}, top = {0.0, 0.0};
		BOOL_T first = TRUE;
		while (TrackIterate(&trk)) {
			if (GetTrkSelected(trk)) {
				coOrd hi, lo;
				GetBoundingBox(trk, &hi, &lo);
				if (first) {
					first = FALSE;
					bot = lo;
					top = hi;
				} else {
					if (lo.x < bot.x) {
						bot.x = lo.x;
					}
					if (lo.y < bot.y) {
						bot.y = lo.y;
					}
					if (hi.x > top.x) {
						top.x = hi.x;
					}
					if (hi.y > top.y) {
						top.y = hi.y;
					}
				}
			}
		}
		panCenter.x = (top.x / 2) + (bot.x) / 2;
		panCenter.y = (top.y / 2) + (bot.y) / 2;
		PanHere(I2VP(1));
		scale_x = (top.x - bot.x) * 1.5 / (mainD.size.x / mainD.scale);
		scale_y = (top.y - bot.y) * 1.5 / (mainD.size.y / mainD.scale);
	} else {
		scale_x = mapD.size.x / (mainD.size.x / mainD.scale);
		scale_y = mapD.size.y / (mainD.size.y / mainD.scale);
	}

	if (scale_x < scale_y) {
		scale_x = scale_y;
	}
	scale_x = ceil(scale_x);
	if (scale_x < 1) {
		scale_x = 1;
	}
	if (scale_x > MAX_MAIN_SCALE) {
		scale_x = MAX_MAIN_SCALE;
	}
	//if (1 != (intptr_t)1) {
	if (1 != VP2L(mode)) {
		mainD.orig = zero;
	}
	DoNewScale(scale_x);
	MainLayout(TRUE, TRUE);
}

/**
 * User selected zoom out, via mouse wheel, button or pulldown.
 *
 * \param mode IN FALSE if zoom button was activated, TRUE if activated via
 * popup or mousewheel
 */

EXPORT void DoZoomDown(void *mode)
{
	long newScale;
	int i;

	LOG(log_zoom, 2, ("DoZoomDown KS:%x\n", MyGetKeyState()));
	if (mode != NULL || (MyGetKeyState() & WKEY_SHIFT) == 0) {
		i = ScaleInx(mainD.scale);
		if (i < 0) {
			i = NearestScaleInx(mainD.scale, TRUE);
		}
		if (i >= 0 && i < (COUNT(zoomList) - 1)) {
			InfoMessage("");
			DoNewScale(zoomList[i + 1].value);
		} else {
			InfoMessage(_("At Maximum Zoom Out"));
		}

	} else if ((MyGetKeyState() & WKEY_CTRL) == 0) {
		wPrefGetInteger("misc", "zoomout", &newScale, 16);
		InfoMessage(_(
		                    "Preset Zoom Out Value selected. Shift+Ctrl+PageUp to reset value"));
		DoNewScale(newScale);
	} else {
		wPrefSetInteger("misc", "zoomout", (long)mainD.scale);
		InfoMessage(_("Zoom Out Program Value %ld:1 set, Shift+PageUp to use"),
		            (long)mainD.scale);
	}
}

/**
 * Zoom to user selected value. This is the callback function for the
 * user-selectable preset zoom values.
 *
 * \param[in] pScaleVP current pScale
 *
 */
static void DoZoom(void *pScaleVP)
{
	DIST_T *pScale = pScaleVP;
	DIST_T scale = *pScale;

	if (scale != mainD.scale) {
		DoNewScale(scale);
	}
}

EXPORT void Pix2CoOrd(drawCmd_p d, wDrawPix_t x, wDrawPix_t y, coOrd * pos)
{
	pos->x = (((DIST_T)x) / d->dpi) * d->scale + d->orig.x;
	pos->y = (((DIST_T)y) / d->dpi) * d->scale + d->orig.y;
}

EXPORT void CoOrd2Pix(drawCmd_p d, coOrd pos, wDrawPix_t * x,
                      wDrawPix_t * y)
{
	*x = (wDrawPix_t)((pos.x - d->orig.x) / d->scale * d->dpi);
	*y = (wDrawPix_t)((pos.y - d->orig.y) / d->scale * d->dpi);
}

/*
 * Position mainD based on panCenter
 * \param mode control effect of constrainMain and CTRL key
 *
 * mode:
 *  - 0: constrain if constrainMain==1 or CTRL is pressed
 *  - 1: same as 0, but ignore CTRL
 *  - 2: same as 0, plus liveMap
 *  - 3: Take position from menuPos
 */
EXPORT void PanHere(void *mode)
{
	if (3 == VP2L(mode)) {
		panCenter = menuPos;
		LOG(log_pan, 2,
		    ("MCenter:Mod-%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
	}
	wBool_t bLiveMap = TRUE;
	if (2 == VP2L(mode)) {
		bLiveMap = liveMap;
	}
	mainD.orig.x = panCenter.x - mainD.size.x / 2.0;
	mainD.orig.y = panCenter.y - mainD.size.y / 2.0;
	wBool_t bNoBorder = (constrainMain != 0);
	if (1 != VP2L(mode))
		if ((MyGetKeyState() & WKEY_CTRL) != 0) {
			bNoBorder = !bNoBorder;
		}

	MainLayout(bLiveMap, bNoBorder); /* PanHere */
}

int DoPanKeyAction(wAction_t action)
{
	switch ((wAccelKey_e)(action >> 8) & 0xFF) {
	case wAccelKey_Del:
		return SelectDelete();
#ifndef WINDOWS
	case wAccelKey_Pgdn:
		DoZoomUp(NULL);
		break;
	case wAccelKey_Pgup:
		DoZoomDown(NULL);
		break;
#endif
	case wAccelKey_Right:
		panCenter.x = mainD.orig.x + mainD.size.x / 2;
		panCenter.y = mainD.orig.y + mainD.size.y / 2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			panCenter.x += 0.25 * mainD.scale;
		} else {
			panCenter.x += mainD.size.x / 2;
		}
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(0));
		break;

	case wAccelKey_Left:
		panCenter.x = mainD.orig.x + mainD.size.x / 2;
		panCenter.y = mainD.orig.y + mainD.size.y / 2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			panCenter.x -= 0.25 * mainD.scale;
		} else {
			panCenter.x -= mainD.size.x / 2;
		}
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(0));
		break;

	case wAccelKey_Up:
		panCenter.x = mainD.orig.x + mainD.size.x / 2;
		panCenter.y = mainD.orig.y + mainD.size.y / 2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			panCenter.y += 0.25 * mainD.scale;
		} else {
			panCenter.y += mainD.size.y / 2;
		}
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(0));
		break;

	case wAccelKey_Down:
		panCenter.x = mainD.orig.x + mainD.size.x / 2;
		panCenter.y = mainD.orig.y + mainD.size.y / 2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			panCenter.y -= 0.25 * mainD.scale;
		} else {
			panCenter.y -= mainD.size.y / 2;
		}
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(0));
		break;

	default:
		return 0;
	}
	return 0;
}

/*
 * IsClose
 * is distance smaller than 10 pixels at 72 DPI?
 * is distance smaller than 0.1" at 1:1?
 */
EXPORT BOOL_T IsClose(DIST_T d) { return d <= closeDist * mainD.scale; }

/*****************************************************************************
 *
 * MAIN MOUSE HANDLER
 *
 */

static int ignoreMoves = 0;

EXPORT void ResetMouseState(void) { mouseState = mouseNone; }

EXPORT void FakeDownMouseState(void) { mouseState = mouseLeftPending; }

/**
 * Return the current position of the mouse pointer in drawing coordinates.
 *
 * \param x OUT pointer x position
 * \param y OUT pointer y position
 */

void GetMousePosition(wDrawPix_t * x, wDrawPix_t * y)
{
	if (x && y) {
		*x = mousePositionx;
		*y = mousePositiony;
	}
}

coOrd minIncrementSizes()
{
	double x, y;
	if (mainD.scale >= 1.0) {
		if (units == UNITS_ENGLISH) {
			x = 0.25; /* >1:1 = 1/4 inch */
			y = 0.25;
		} else {
			x = 1 / (2.54 * 2); /* >1:1 = 0.5 cm */
			y = 1 / (2.54 * 2);
		}
	} else {
		if (units == UNITS_ENGLISH) {
			x = 1.0 / 64.0; /* <1:1 = 1/64 inch */
			y = 1.0 / 64.0;
		} else {
			x = 1.0 / (25.4 * 2); /* >1:1 = 0.5 mm */
			y = 1.0 / (25.4 * 2);
		}
	}
	coOrd ret;
	ret.x = x * 1.5;
	ret.y = y * 1.5;
	return ret;
}

static void DoMouse(wAction_t action, coOrd pos)
{

	int rc;
	wDrawPix_t x, y;
	static BOOL_T ignoreCommands;
	/* Middle button pan state */
	static BOOL_T panActive = FALSE;
	static coOrd panOrigin, panStart;

	LOG(log_mouse, (action == wActionMove) ? 2 : 1,
	    ("DoMouse( %d, %0.3f, %0.3f )\n", action, pos.x, pos.y))

	if (recordF) {
		RecordMouse("MOUSE", action, pos.x, pos.y);
	}

	switch (action & 0xFF) {
	case C_UP:
		if (mouseState != mouseLeft) {
			return;
		}
		if (ignoreCommands) {
			ignoreCommands = FALSE;
			return;
		}
		mouseState = mouseNone;
		break;
	case C_RUP:
		if (mouseState != mouseRight) {
			return;
		}
		if (ignoreCommands) {
			ignoreCommands = FALSE;
			return;
		}
		mouseState = mouseNone;
		break;
	case C_MUP:
		if (!panActive) {
			return;
		}
		if (ignoreCommands) {
			ignoreCommands = FALSE;
			return;
		}
		break;
	case C_MOVE:
		if (mouseState == mouseLeftPending) {
			action = C_DOWN;
			mouseState = mouseLeft;
		}
		if (mouseState != mouseLeft) {
			return;
		}
		if (ignoreCommands) {
			return;
		}
		break;
	case C_RMOVE:
		if (mouseState != mouseRight) {
			return;
		}
		if (ignoreCommands) {
			return;
		}
		break;
	case C_DOWN:
		mouseState = mouseLeft;
		break;
	case C_RDOWN:
		mouseState = mouseRight;
		break;
	case C_MDOWN:
		/* Set up state for Middle button pan */
		panActive = TRUE;
		panOrigin = panCenter;
		panStart.x = pos.x - mainD.orig.x;
		panStart.y = pos.y - mainD.orig.y;
		break;
	}

	inError = FALSE;

	coOrd min = minIncrementSizes();

	switch (action & 0xFF) {
	case C_DOWN:
	case C_RDOWN:
		DYNARR_RESET(trkSeg_t, tempSegs_da);
		break;
	case C_MDOWN:
		break;
	case wActionMove:
		InfoPos(pos);
		if (ignoreMoves) {
			return;
		}
		break;
	case wActionExtKey:
		mainD.CoOrd2Pix(&mainD, pos, &x, &y);
		if ((MyGetKeyState() & (WKEY_SHIFT | WKEY_CTRL)) ==
		    (WKEY_SHIFT | WKEY_CTRL) &&
		    (action >> 8 == wAccelKey_Up || action >> 8 == wAccelKey_Down ||
		     action >> 8 == wAccelKey_Left || action >> 8 == wAccelKey_Right)) {
			break; /* Allow SHIFT+CTRL for Move */
		}
		if (((action >> 8) & 0xFF) == wAccelKey_LineFeed) {
			action = C_TEXT + ((int)(0x0A << 8));
			break;
		}
		rc = DoPanKeyAction(action);
		if (rc != 1) {
			return;
		}
		break;
	case C_TEXT:
		if ((action >> 8) == 0x0D) {
			action = C_OK;
		} else if ((action >> 8) == 0x1B) {
			ConfirmReset(TRUE);
			return;
		}
		__attribute__((fallthrough));
	case C_MODKEY:
	case C_MOVE:
	case C_UP:
	case C_RMOVE:
	case C_RUP:
	case C_LDOUBLE:
	case C_MUP:
		InfoPos(pos);
		break;
	case C_WUP: {
		/* Zoom in toward mouse position.
		 * Ignore if pointer is outside the drawing area (on rulers/border). */
		wWinPix_t ww_z, hh_z;
		wDrawGetSize(mainD.d, &ww_z, &hh_z);
		if (mousePositionx < lborder || mousePositionx > ww_z - RBORDER ||
		    mousePositiony < bborder || mousePositiony > hh_z - TBORDER) {
			break;
		}
		coOrd anchor = pos;
		{
			DIST_T oldScale = mainD.scale;
			int idx = ScaleInx(oldScale);
			if (idx < 0) { idx = NearestScaleInx(oldScale, FALSE); }

			/* Same guard logic as DoZoomUp: stop at 1:1 unless CTRL held */
			if (oldScale == 1.0 && !(MyGetKeyState() & WKEY_CTRL)) {
				InfoMessage(_("Scale 1:1 - Use Ctrl+ to go to Macro Zoom Mode"));
				break;
			}
			if (idx <= 0) {
				InfoMessage("Minimum Macro Zoom");
				break;
			}

			DIST_T newScale = zoomList[idx - 1].value;
			if (newScale <= 1.0) {
				InfoMessage(_("Macro Zoom Mode"));
			} else {
				InfoMessage("");
			}

			/* Fractional position of anchor within current viewport */
			DIST_T fx = (anchor.x - mainD.orig.x) / mainD.size.x;
			DIST_T fy = (anchor.y - mainD.orig.y) / mainD.size.y;

			/* New model-space viewport size (proportional to scale) */
			DIST_T ratio = newScale / oldScale;
			DIST_T newSizeX = mainD.size.x * ratio;
			DIST_T newSizeY = mainD.size.y * ratio;

			/* Set panCenter so the anchor stays at (fx,fy) in the new viewport */
			panCenter.x = anchor.x + newSizeX * (0.5 - fx);
			panCenter.y = anchor.y + newSizeY * (0.5 - fy);
			LOG(log_pan, 2,
			    ("PanCenter:%d %0.3f %0.3f (zoom-to-mouse)\n",
			     __LINE__, panCenter.x, panCenter.y));
			DoNewScale(newScale);
		}
		break;
	}
	case C_WDOWN: {
		/* Zoom out away from mouse position.
		 * Ignore if pointer is outside the drawing area (on rulers/border). */
		wWinPix_t ww_z2, hh_z2;
		wDrawGetSize(mainD.d, &ww_z2, &hh_z2);
		if (mousePositionx < lborder || mousePositionx > ww_z2 - RBORDER ||
		    mousePositiony < bborder || mousePositiony > hh_z2 - TBORDER) {
			break;
		}
		coOrd anchor = pos;
		{
			DIST_T oldScale = mainD.scale;
			int idx = ScaleInx(oldScale);
			if (idx < 0) { idx = NearestScaleInx(oldScale, TRUE); }
			if (idx < 0 || idx >= (COUNT(zoomList) - 1)) {
				InfoMessage(_("At Maximum Zoom Out"));
				break;
			}

			DIST_T newScale = zoomList[idx + 1].value;
			InfoMessage("");

			/* Fractional position of anchor within current viewport */
			DIST_T fx = (anchor.x - mainD.orig.x) / mainD.size.x;
			DIST_T fy = (anchor.y - mainD.orig.y) / mainD.size.y;

			/* New model-space viewport size (proportional to scale) */
			DIST_T ratio = newScale / oldScale;
			DIST_T newSizeX = mainD.size.x * ratio;
			DIST_T newSizeY = mainD.size.y * ratio;

			/* Set panCenter so the anchor stays at (fx,fy) in the new viewport */
			panCenter.x = anchor.x + newSizeX * (0.5 - fx);
			panCenter.y = anchor.y + newSizeY * (0.5 - fy);
			LOG(log_pan, 2,
			    ("PanCenter:%d %0.3f %0.3f (zoom-to-mouse)\n",
			     __LINE__, panCenter.x, panCenter.y));
			DoNewScale(newScale);
		}
		break;
	}
	case C_SCROLLUP:
		panCenter.y = panCenter.y +
		              ((mainD.size.y / 20 > min.y) ? mainD.size.y / 20 : min.y);
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(1));
		break;
	case C_SCROLLDOWN:
		panCenter.y = panCenter.y -
		              ((mainD.size.y / 20 > min.y) ? mainD.size.y / 20 : min.y);
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(1));
		break;
	case C_SCROLLLEFT:
		panCenter.x = panCenter.x -
		              ((mainD.size.x / 20 > min.x) ? mainD.size.x / 20 : min.x);
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(1));
		break;
	case C_SCROLLRIGHT:
		panCenter.x = panCenter.x +
		              ((mainD.size.x / 20 > min.x) ? mainD.size.x / 20 : min.x);
		LOG(log_pan, 2,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(1));
		break;
	case C_MMOVE:
		/* Middle button pan */
		if (!panActive) {
			break;
		}
		panCenter.x = panOrigin.x + (panStart.x - (pos.x - mainD.orig.x));
		panCenter.y = panOrigin.y + (panStart.y - (pos.y - mainD.orig.y));
		if (panCenter.x < 0) {
			panCenter.x = 0;
		}
		if (panCenter.x > mapD.size.x) {
			panCenter.x = mapD.size.x;
		}
		if (panCenter.y < 0) {
			panCenter.y = 0;
		}
		if (panCenter.y > mapD.size.y) {
			panCenter.y = mapD.size.y;
		}
		LOG(log_pan, 1,
		    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
		PanHere(I2VP(1));
		break;
	default:
		NoticeMessage(MSG_DOMOUSE_BAD_OP, _("Ok"), NULL, action & 0xFF);
		break;
	}
	if (delayUpdate) {
		wDrawDelayUpdate(mainD.d, TRUE);
	}
	rc = DoCurCommand(action, pos);
	wDrawDelayUpdate(mainD.d, FALSE);
	switch (rc) {
	case C_CONTINUE:
		break;
	case C_ERROR:
		ignoreCommands = TRUE;
		inError = TRUE;
		Reset();
		LOG(log_mouse, 1, ("Mouse returns Error\n"))
		break;
	case C_TERMINATE:
		Reset();
		DoCurCommand(C_START, zero);
		break;
	}
}

wDrawPix_t autoPanFactor = 10;
static void DoMousew(wDraw_p d, void *context, wAction_t action, wDrawPix_t x,
                     wDrawPix_t y)
{
	coOrd pos;
	coOrd orig;
	wWinPix_t w, h;
	static wDrawPix_t lastX, lastY;
	DIST_T minDist;
	wDrawGetSize(mainD.d, &w, &h);
	if (autoPan && !inPlayback) {
		if ( action == wActionLDown || action == wActionRDown
		     || action == wActionScrollDown ||
		     (action == wActionLDrag && mouseState == mouseLeftPending)) {
			lastX = x;
			lastY = y;
		}
		if (action == wActionLDrag || action == wActionRDrag) {
			orig = mainD.orig;
			if ((x < 10 && x < lastX) || (x > w - 10 && x > lastX) ||
			    (y < 10 && y < lastY) || (y > h - 10 && y > lastY)) {
				mainD.Pix2CoOrd(&mainD, x, y, &pos);
				orig.x =
				        mainD.orig.x +
				        (pos.x - (mainD.orig.x + mainD.size.x / 2.0)) / autoPanFactor;
				orig.y =
				        mainD.orig.y +
				        (pos.y - (mainD.orig.y + mainD.size.y / 2.0)) / autoPanFactor;
				if (orig.x != mainD.orig.x || orig.y != mainD.orig.y) {
					if (mainD.scale >= 1) {
						if (units == UNITS_ENGLISH) {
							minDist = 1.0;
						} else {
							minDist = 1.0 / 2.54;
						}
						if (orig.x != mainD.orig.x) {
							if (fabs(orig.x - mainD.orig.x) < minDist) {
								if (orig.x < mainD.orig.x) {
									orig.x -= minDist;
								} else {
									orig.x += minDist;
								}
							}
						}
						if (orig.y != mainD.orig.y) {
							if (fabs(orig.y - mainD.orig.y) < minDist) {
								if (orig.y < mainD.orig.y) {
									orig.y -= minDist;
								} else {
									orig.y += minDist;
								}
							}
						}
					}
					mainD.orig = orig;
					panCenter.x = mainD.orig.x + mainD.size.x / 2.0;
					panCenter.y = mainD.orig.y + mainD.size.y / 2.0;
					LOG(log_pan, 2,
					    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x,
					     panCenter.y));
					MainLayout(TRUE, TRUE); /* DoMouseW: autopan */
					tempD.orig = mainD.orig;
					wFlush();
				}
			}
			lastX = x;
			lastY = y;
		}
	}
	mainD.Pix2CoOrd(&mainD, x, y, &pos);
	switch (action & 0xFF) {
	case wActionMove:
	case wActionLDown:
	case wActionLDrag:
	case wActionLUp:
	case wActionRDown:
	case wActionRDrag:
	case wActionRUp:
	case wActionLDownDouble:
	case wActionMDrag:
		mousePositionx = x;
		mousePositiony = y;
		break;
	default:
		break;
	}

	DoMouse(action, pos);

	if ((x < 20) || (x > w - 20) || (y < 20) || (y > h - 20)) {
		wSetCursor(mainD.d, defaultCursor); /* Add system cursor if close to edges */
	}
}

static void PlaybackMain(char *line)
{
	int rc;
	int action;
	coOrd pos;

	SetCLocale();

	rc = sscanf(line, "%d " SCANF_FLOAT_FORMAT SCANF_FLOAT_FORMAT, &action,
	            &pos.x, &pos.y);
	SetUserLocale();

	if (rc != 3) {
		SyntaxError("MOUSE", rc, 3);
	} else {
		PlaybackMouse(DoMouse, &tempD, (wAction_t)action, pos, wDrawColorBlack);
	}
}

static void PlaybackKey(char *line)
{
	int rc;
	int action = C_TEXT;
	coOrd pos;
	int c;

	SetCLocale();
	rc = sscanf(line, "%d " SCANF_FLOAT_FORMAT SCANF_FLOAT_FORMAT, &c, &pos.x,
	            &pos.y);
	SetUserLocale();

	if (rc != 3) {
		SyntaxError("MOUSE", rc, 3);
	} else {
		action = action | (unsigned int)c << 8;
		PlaybackMouse(DoMouse, &tempD, (wAction_t)action, pos, wDrawColorBlack);
	}
}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

static void DrawChange(long changes)
{
	if (changes & CHANGE_MAIN) {
		MainLayout(TRUE, FALSE); /* DrawChange: CHANGE_MAIN */
	}
	if (changes & CHANGE_UNITS) {
		SetInfoBar();
	}
	if(changes & CHANGE_MAP) {
		MapChangeScale();
	}
}

static void MainLayoutCB(wDraw_p bd, void *pContex, wWinPix_t px,
                         wWinPix_t py)
{
	MainLayout(TRUE, FALSE);
}

EXPORT void InitColor(void)
{
	drawColorBlack = wDrawFindColor(wRGB(0, 0, 0));
	drawColorWhite = wDrawFindColor(wRGB(255, 255, 255));
	drawColorRed = wDrawFindColor(wRGB(255, 0, 0));
	drawColorBlue = wDrawFindColor(wRGB(0, 0, 255));
	drawColorGreen = wDrawFindColor(wRGB(0, 255, 0));
	drawColorAqua = wDrawFindColor(wRGB(0, 255, 255));
	drawColorDkRed = wDrawFindColor(wRGB(128, 0, 0));
	drawColorDkBlue = wDrawFindColor(wRGB(0, 0, 128));
	drawColorDkGreen = wDrawFindColor(wRGB(0, 128, 0));
	drawColorDkAqua = wDrawFindColor(wRGB(0, 128, 128));

	/* Last component of special color must be > 3 */
	drawColorPreviewSelected = wDrawFindColor(wRGB(6, 6, 255)); /* Special Blue */
	drawColorPreviewUnselected = wDrawFindColor(wRGB(255, 215,
	                             6)); /* Special Yellow */

	drawColorPowderedBlue = wDrawFindColor(wRGB(129, 212, 250));
	drawColorPurple = wDrawFindColor(wRGB(128, 0, 128));
	drawColorMagenta = wDrawFindColor(wRGB(255, 0, 255));
	drawColorGold = wDrawFindColor(wRGB(255, 215, 0));
	drawColorGrey10 = wDrawFindColor(wRGB(26, 26, 26));
	drawColorGrey20 = wDrawFindColor(wRGB(51, 51, 51));
	drawColorGrey30 = wDrawFindColor(wRGB(72, 72, 72));
	drawColorGrey40 = wDrawFindColor(wRGB(102, 102, 102));
	drawColorGrey50 = wDrawFindColor(wRGB(128, 128, 128));
	drawColorGrey60 = wDrawFindColor(wRGB(153, 153, 153));
	drawColorGrey70 = wDrawFindColor(wRGB(179, 179, 179));
	drawColorGrey80 = wDrawFindColor(wRGB(204, 204, 204));
	drawColorGrey90 = wDrawFindColor(wRGB(230, 230, 230));
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
	exceptionColor = wDrawFindColor(wRGB(255, 89, 0));
	tieColor = wDrawFindColor(wRGB(153, 89, 68));
}

EXPORT void DrawInit(int initialZoom)
{
	wWinPix_t w = 0, h = 0;
	wFont_p rfp;

	InitDrawCmds();   /* wire Pix2CoOrd/CoOrd2Pix into mainD/tempD */

	log_pan = LogFindIndex("pan");
	log_zoom = LogFindIndex("zoom");
	log_mouse = LogFindIndex("mouse");
	log_redraw = LogFindIndex("redraw");
	log_timemainredraw = LogFindIndex("timemainredraw");
	log_size = LogFindIndex("mapsize");

	tempD.d = mainD.d =
	                  wDrawCreate(mainW, 0, 0, "maindraw", BD_TICKS | BD_MODKEYS, w, h,
	                              &mainD, MainLayoutCB, DoMousew);

	if (initialZoom == 0) {
		WDOUBLE_T tmpR;
		wPrefGetFloat("draw", "zoom", &tmpR, mainD.scale);
		mainD.scale = tmpR;
	} else {
		while (initialZoom > 0 && mainD.scale < MAX_MAIN_SCALE) {
			mainD.scale *= 2;
			initialZoom--;
		}
		while (initialZoom < 0 && mainD.scale > MIN_MAIN_SCALE) {
			mainD.scale /= 2;
			initialZoom++;
		}
	}
	tempD.scale = mainD.scale;
	mainD.dpi = wDrawGetDPI(mainD.d);
	if (mainD.scale > 1.0 && mainD.scale <= 12.0) {
		mainD.dpi =
		        floor((mainD.dpi + mainD.scale / 2) / mainD.scale) * mainD.scale;
	}
	tempD.dpi = mainD.dpi;

	/* Initialise ruler font in drawruler.c and compute border metrics */
	rfp = wStandardFont(F_MONO, FALSE, FALSE);
	SetRulerFont(rfp, 12.0);
	charWidth = wFontGetCharWidth(mainD.d, rfp, 12.0);
	lborder = charWidth * MAXLABELCHARS + 8;
	bborder = wFontGetCharHeight(mainD.d, rfp, 12.0) + 12;

	SetMainSize();
	panCenter.x = mainD.size.x / 2 + mainD.orig.x;
	panCenter.y = mainD.size.y / 2 + mainD.orig.y;

	MapWindowCreate();

	AddPlaybackProc("MOUSE ", PlaybackMain, NULL);
	AddPlaybackProc("KEY ", PlaybackKey, NULL);

	SetZoomRadio( mainD.scale );
	InfoScale();
	SetInfoBar();
	InfoPos(zero);
	RegisterChangeNotification(DrawChange);
}

static wMenu_p panPopupM;

static STATUS_T CmdPan(wAction_t action, coOrd pos)
{
	static enum { PAN, ZOOM, NONE } panmode;

	static coOrd base, size;

	DIST_T scale_x, scale_y;

	static coOrd start_pos;
	if (action == C_DOWN) {
		panmode = PAN;
	} else if (action == C_RDOWN) {
		panmode = ZOOM;
	}

	switch (action & 0xFF) {
	case C_START:
		start_pos = zero;
		panmode = NONE;
		InfoMessage(_("Left-Drag to pan, Ctrl+Left-Drag to zoom, 0 to set origin "
		              "to zero, 1-9 to zoom#, e to set to extents"));
		wSetCursor(mainD.d, wCursorSizeAll);
		break;
	case C_DOWN:
		if ((MyGetKeyState() & WKEY_CTRL) == 0) {
			panmode = PAN;
			start_pos = pos;
			InfoMessage(_("Pan Mode - drag point to new position"));
			break;
		} else {
			panmode = ZOOM;
			start_pos = pos;
			base = pos;
			size = zero;
			InfoMessage(_("Zoom Mode - drag area to zoom"));
		}
		break;
	case C_MOVE:
		if (panmode == PAN) {
			double min_inc;
			if (mainD.scale >= 1.0) {
				if (units == UNITS_ENGLISH) {
					min_inc = 1.0 / 4.0; /* >1:1 = 1/4 inch */
				} else {
					min_inc = 1.0 / (2.54 * 2); /* >1:1 = 0.5 cm */
				}
			} else {
				if (units == UNITS_ENGLISH) {
					min_inc = 1.0 / 64.0; /* <1:1 = 1/64 inch */
				} else {
					min_inc = 1.0 / (25.4 * 2); /* >1:1 = 0.5 mm */
				}
			}
			if ((fabs(pos.x - start_pos.x) > min_inc) ||
			    (fabs(pos.y - start_pos.y) > min_inc)) {
				coOrd oldOrig = mainD.orig;
				mainD.orig.x -= (pos.x - start_pos.x);
				mainD.orig.y -= (pos.y - start_pos.y);
				if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
					ConstraintOrig(&mainD.orig, mainD.size, TRUE, TRUE);
				}
				if ((oldOrig.x == mainD.orig.x) && (oldOrig.y == mainD.orig.y)) {
					InfoMessage(_("Can't move any further in that direction"));
				} else {
					InfoMessage(_("Left click to pan, right click to zoom, 'o' for "
					              "origin, 'e' for extents"));
				}
			}
		} else if (panmode == ZOOM) {
			base = start_pos;
			size.x = pos.x - base.x;
			if (size.x < 0) {
				size.x = -size.x;
				base.x = pos.x;
			}
			size.y = pos.y - base.y;
			if (size.y < 0) {
				size.y = -size.y;
				base.y = pos.y;
			}
		}
		panCenter.x = mainD.orig.x + mainD.size.x / 2.0;
		panCenter.y = mainD.orig.y + mainD.size.y / 2.0;
		PanHere(I2VP(0));
		break;
	case C_UP:
		if (panmode == ZOOM) {
			if (size.x <= 0.0 || size.y <= 0.0) {
				panmode = NONE;
				break;
			}
			scale_x = size.x / mainD.size.x * mainD.scale;
			scale_y = size.y / mainD.size.y * mainD.scale;

			if (scale_x < scale_y) {
				scale_x = scale_y;
			}
			if (scale_x > 1) {
				scale_x = ceil(scale_x);
			} else {
				scale_x = 1 / (ceil(1 / scale_x));
			}

			if (scale_x > MAX_MAIN_SCALE) {
				scale_x = MAX_MAIN_SCALE;
			}
			if (scale_x < MIN_MAIN_MACRO) {
				scale_x = MIN_MAIN_MACRO;
			}

			mainD.orig.x = base.x;
			mainD.orig.y = base.y;

			panmode = NONE;
			InfoMessage(_("Left Drag to Pan, +CTRL to Zoom, 0 to set Origin to "
			              "0,0, 1-9 to Zoom#, e to set to Extent"));
			DoNewScale(scale_x);
			break;
		} else if (panmode == PAN) {
			panCenter.x = mainD.orig.x + mainD.size.x / 2.0;
			panCenter.y = mainD.orig.y + mainD.size.y / 2.0;
			LOG(log_pan, 2,
			    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
			panmode = NONE;
		}
		break;
	case C_REDRAW:
		if (panmode == ZOOM) {
			if (size.x > 0.0 && size.y > 0.0) {
				DrawHilight(&tempD, base, size, TRUE);
			}
		}
		break;
	case C_CANCEL:
		base = zero;
		wSetCursor(mainD.d, defaultCursor);
		return C_TERMINATE;
	case C_TEXT:
		panmode = NONE;

		if ((action >> 8) == 'e') { /* "e" */
			DoZoomExtents(I2VP(0));
		} else if ((action >> 8) == 's') {
			DoZoomExtents(I2VP(1));
		} else if (((action >> 8) == '0') ||
		           ((action >> 8) == 'o')) { /* "0" or "o" */
			mainD.orig = zero;
			panCenter.x = mainD.size.x / 2.0;
			panCenter.y = mainD.size.y / 2.0;
			LOG(log_pan, 2,
			    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
			MainLayout(TRUE, TRUE); /* CmdPan C_TEXT '0' 'o' */
		} else if ((action >> 8) >= '1' && (action >> 8) <= '9') { /* "1" to "9" */
			scale_x = (action >> 8) & 0x0F;
			DoNewScale(scale_x);
		} else if ((action >> 8) == 'c') { /* "c" */
			panCenter = pos;
			LOG(log_pan, 2,
			    ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y));
			PanHere(I2VP(0)); /* CmdPan C_TEXT 'c' */
		}

		if ((action >> 8) == 0x0D) {
			wSetCursor(mainD.d, defaultCursor);
			return C_TERMINATE;
		} else if ((action >> 8) == 0x1B) {
			wSetCursor(mainD.d, defaultCursor);
			return C_TERMINATE;
		}
		break;
	case C_CMDMENU:
		menuPos = pos;
		wMenuPopupShow(panPopupM);
		return C_CONTINUE;
		break;
	}

	return C_CONTINUE;
}
static wMenuPush_p zoomExtents, panOrig, panHere;
static wMenuPush_p zoomLvl1, zoomLvl2, zoomLvl3, zoomLvl4, zoomLvl5, zoomLvl6,
       zoomLvl7, zoomLvl8, zoomLvl9;

EXPORT void PanMenuEnter(void *keyVP)
{
	int key = (int)VP2L(keyVP);
	int action;
	action = C_TEXT;
	action |= key << 8;
	CmdPan(action, zero);
}

EXPORT void InitCmdPan(wMenu_p menu)
{
	panCmdInx =
	        AddMenuButton(menu, CmdPan, "cmdPan", _("Pan/Zoom"),
	                      CreateToolbarIconFromResource("pan-zoom.png"), LEVEL0,
	                      IC_TOGGLE | IC_CANCEL | IC_POPUP | IC_LCLICK | IC_CMDMENU,
	                      ACCL_PAN, NULL);
}
EXPORT void InitCmdPan2(wMenu_p menu)
{
	panPopupM = MenuRegister("Pan Options");
	wMenuPushCreate(panPopupM, "cmdSelectMode",
	                GetBalloonHelpStr("cmdSelectMode"), 0, DoCommandB,
	                I2VP(selectCmdInx));
	wMenuPushCreate(panPopupM, "cmdDescribeMode",
	                GetBalloonHelpStr("cmdDescribeMode"), 0, DoCommandB,
	                I2VP(describeCmdInx));
	wMenuPushCreate(panPopupM, "cmdModifyMode",
	                GetBalloonHelpStr("cmdModifyMode"), 0, DoCommandB,
	                I2VP(modifyCmdInx));
	wMenuSeparatorCreate(panPopupM);
	zoomExtents = wMenuPushCreate(panPopupM, "", _("Zoom to extents - 'e'"), 0,
	                              PanMenuEnter, I2VP('e'));
	zoomLvl1 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:1 - '1'"), 0,
	                           PanMenuEnter, I2VP('1'));
	zoomLvl2 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:2 - '2'"), 0,
	                           PanMenuEnter, I2VP('2'));
	zoomLvl3 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:3 - '3'"), 0,
	                           PanMenuEnter, I2VP('3'));
	zoomLvl4 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:4 - '4'"), 0,
	                           PanMenuEnter, I2VP('4'));
	zoomLvl5 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:5 - '5'"), 0,
	                           PanMenuEnter, I2VP('5'));
	zoomLvl6 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:6 - '6'"), 0,
	                           PanMenuEnter, I2VP('6'));
	zoomLvl7 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:7 - '7'"), 0,
	                           PanMenuEnter, I2VP('7'));
	zoomLvl8 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:8 - '8'"), 0,
	                           PanMenuEnter, I2VP('8'));
	zoomLvl9 = wMenuPushCreate(panPopupM, "", _("Zoom to 1:9 - '9'"), 0,
	                           PanMenuEnter, I2VP('9'));
	panOrig = wMenuPushCreate(panPopupM, "", _("Pan to Origin - 'o'/'0'"), 0,
	                          PanMenuEnter, I2VP('o'));
	wMenu_p zoomPanM = wMenuMenuCreate(panPopupM, "", _("&Zoom"));
	InitCmdZoom(NULL, NULL, NULL, zoomPanM);
	panHere = wMenuPushCreate(panPopupM, "", _("Pan center here - 'c'"), 0,
	                          PanHere, I2VP(3));
}
