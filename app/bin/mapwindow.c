/**
 * \file   mapwindow.c
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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

#include <wlib.h>

#include "common.h"
#include "draw.h"
#include "fileio.h"
#include "form.h"
#include "mapwindow.h"
#include "misc.h"
#include "track.h"

#include "xtctypes.h"

#define MIN_SCALE 0.01
#define MAX_SCALE 64.0
#define MAP_BORDER_PIXELS 2
#define SCREEN_SIZE_FACTOR 0.5

static int log_mapsize = 0;
static int log_mapredraw = 0;

static int delayUpdate = 1;

static void DoMapPan(wAction_t action, coOrd pos);
static wBool_t MapRedraw(wControl_p bd, void* pContex, wWinPix_t px,
                         wWinPix_t py);

EXPORT drawCmd_t mapD = {
	NULL, &screenDrawFuncs, DC_SIMPLE, INIT_MAP_SCALE, 0.0, {0.0,0.0}, {96.0,48.0}, Pix2CoOrd, CoOrd2Pix, 96.0
};

static paramDrawData_t mapDrawData = { 50, 50, MapRedraw, DoMapPan, &mapD };
static paramGroup_t mapPG;
static paramData_t mapPLs[] = {
	{	PD_DRAW, NULL, "canvas", PDO_DLGRESIZE, &mapDrawData, .group = &mapPG }
};

#define MAPCANVASCONTROL mapPLs[0].control
static paramGroup_t mapPG = { "map", PGO_NODEFAULTPROC | PGO_FULLDIALOGFROMBUILDER, mapPLs, COUNT(mapPLs) };

EXPORT wControl_p mapW;
EXPORT BOOL_T mapVisible;

void MapDrawBoundingBox(BOOL_T set)
{
	CHECK(mainD.d);
	CHECK(mapD.d);
	if (!mapVisible) {
		return;
	}
	DrawHilight(&mapD, mainD.orig, mainD.size, TRUE);
}

/**
 * Map Handling
 *
 */

static void DoMapPan(wAction_t action, coOrd pos)
{
	static coOrd mapOrig;
	static coOrd size;
	static DIST_T xscale, yscale;
	static enum { noPan, movePan, resizePan } mode = noPan;
	wDrawPix_t x, y;

	switch (action & 0xFF) {

	case C_DOWN:
		if (mode == noPan) {
			mode = movePan;
		} else {
			break;
		}
	case C_MOVE:
		if (mode != movePan) {
			break;
		}
		panCenter = pos;
		LOG(log_pan, 1, ("%s = [ %0.3f, %0.3f ]\n", action == C_DOWN ? "START" : "MOVE",
		                 mainD.orig.x, mainD.orig.y))
		PanHere(I2VP(2));
		break;
	case C_UP:
		if (mode != movePan) {
			break;
		}
		panCenter = pos;
		PanHere(I2VP(0));
		LOG(log_pan, 1, ("FINAL = [ %0.3f, %0.3f ]\n", mainD.orig.x, mainD.orig.y))
		mode = noPan;
		break;

	case C_RDOWN:
		if (mode == noPan) {
			mode = resizePan;
		} else {
			break;
		}
		mapOrig = pos;
		size.x = mainD.size.x / mainD.scale;  //How big screen?
		size.y = mainD.size.y / mainD.scale;
		xscale = mainD.scale; //start at current
		panCenter = pos;
		LOG(log_pan, 1, ("START %0.3fx%0.3f %0.3f+%0.3f\n", mapOrig.x, mapOrig.y,
		                 size.x, size.y))
		break;

	case C_RMOVE:
		if (mode != resizePan) {
			break;
		}
		if (pos.x < 0) {
			pos.x = 0;
		}
		if (pos.x > mapD.size.x) {
			pos.x = mapD.size.x;
		}
		if (pos.y < 0) {
			pos.y = 0;
		}
		if (pos.y > mapD.size.y) {
			pos.y = mapD.size.y;
		}

		xscale = fabs((pos.x - mapOrig.x) * 2.0 / size.x);
		yscale = fabs((pos.y - mapOrig.y) * 2.0 / size.y);
		if (xscale < yscale) {
			xscale = yscale;
		}
		xscale = ceil(xscale);

		if (xscale < MIN_SCALE) {
			xscale = MIN_SCALE;
		}
		if (xscale > MAX_SCALE) {
			xscale = MAX_SCALE;
		}

		mainD.size.x = size.x * xscale;
		mainD.size.y = size.y * xscale;
		mainD.orig.x = mapOrig.x - mainD.size.x / 2.0;
		mainD.orig.y = mapOrig.y - mainD.size.y / 2.0;
		tempD.scale = mainD.scale = xscale;
		PanHere(I2VP(2));
		LOG(log_pan, 1, ("MOVE SCL:%0.3f %0.3fx%0.3f %0.3f+%0.3f\n", xscale,
		                 mainD.orig.x, mainD.orig.y, mainD.size.x, mainD.size.y))
		InfoScale();
		break;

	case C_RUP:
		if (mode != resizePan) {
			break;
		}
		DoNewScale(xscale);
		mode = noPan;
		break;

	case wActionExtKey:
		mainD.CoOrd2Pix(&mainD, pos, &x, &y);
		DoPanKeyAction(action);
		mainD.Pix2CoOrd(&mainD, x, y, &pos);
		InfoPos(pos);
		return;

	default:
		return;
	}
}

/**
 * Redraw the Map window using the Scale derived from the Window size and Room size
 * \param bd [inout] Map canvas - not used
 * \param pContext [inout] Param context - not used
 * \param px, py [in] canvas size
 */
static wBool_t MapRedraw( wControl_p bd, void* pContex, wWinPix_t px,
                          wWinPix_t py)
{
	if (inPlaybackQuit) {
		return FALSE;
	}
	static int cMR = 0;
	LOG(log_mapredraw, 2, ("MapRedraw: %d\n", cMR++));
	if (!mapVisible) {
		return FALSE;
	}
	if (delayUpdate) {
		wDrawDelayUpdate(mapD.d, TRUE);
	}

	wBool_t bTemp = wDrawSetTempMode(mapD.d, FALSE);
	if (bTemp) {
		printf("MapRedraw TempMode\n");
	}

	if (log_mapsize >= 2) {
		lprintf("    MapRedraw: parm=%ldx%ld", px, py);
		wWinPix_t tx, ty;
		if (mapW) {
			wWinGetSize(mapW, &tx, &ty);
			lprintf(" win=%ldx%ld", tx, ty);
		}
		if (mapD.d) {
			wDrawGetSize(mapD.d, &tx, &ty);
			lprintf(" draw=%ldx%ld", tx, ty);
		}
		lprintf("\n");
	}

	// Find new mapD.scale
	if ((px <= 0 || py <= 0) && mapD.d) {
		wDrawGetSize(mapD.d, &px, &py);
		px += MAP_BORDER_PIXELS;
		py += MAP_BORDER_PIXELS;
	}
	if (px > 0 && py > 0) {
		FLOAT_T scaleX = mapD.size.x * mapD.dpi / px;
		FLOAT_T scaleY = mapD.size.y * mapD.dpi / py;
		FLOAT_T scale;

		// Find largest scale
		if (scaleX > scaleY)
		{ scale = scaleX; }
		else
		{ scale = scaleY; }

		if (scale > MAX_MAIN_SCALE) { scale = MAX_MAIN_SCALE; }
		if (scale < MIN_MAIN_MACRO) { scale = MIN_MAIN_MACRO; }

		scale = ceil(scale);	// Round up
		LOG(log_mapsize, 2,
		    ("      %ldx%ld mapD.scale=%0.3f, scaleX=%0.3f scaleY=%0.3f scale=%0.3f\n",
		     px, py, mapD.scale, scaleX, scaleY, scale));
		mapD.scale = scale;
	} else {
		LOG(log_mapsize, 2, ("  0x0 mapD.scale=%0.3f\n", mapD.scale));
	}
	wDrawClear(mapD.d);
	DrawTracks(&mapD, mapD.scale, mapD.orig, mapD.size);
	MapDrawBoundingBox(TRUE);

	wDrawSetTempMode(mapD.d, bTemp);
	wDrawDelayUpdate(mapD.d, FALSE);

	return TRUE;
}


/*
 * Set mapW size to fit the rescaled map
 *
 * \param reset IN
 */
static int mapBorderH = 24;
static int mapBorderW = 24;

void MapChangeScale()
{
	wWinPix_t w, h;
	FLOAT_T fw, fh;

	// Restrict map size to SCREEN_SIZE_FACTOR of screen
	FLOAT_T fScaleW = mapD.size.x / (displayWidth * SCREEN_SIZE_FACTOR / mapD.dpi);
	FLOAT_T fScaleH = mapD.size.y / (displayHeight * SCREEN_SIZE_FACTOR / mapD.dpi);
	FLOAT_T fScale = ceil(max(fScaleW, fScaleH));
	if (fScale > mapD.scale) {
		LOG(log_mapsize, 2, ("  ChangeMapScale incr scale from %0.3f to %0.3f\n",
		                     mapD.scale, fScale));
		mapD.scale = fScale;
	}

	fw = (((mapD.size.x / mapD.scale) * mapD.dpi) + 0.5) + MAP_BORDER_PIXELS;
	fh = (((mapD.size.y / mapD.scale) * mapD.dpi) + 0.5) + MAP_BORDER_PIXELS;

	w = (wWinPix_t)fw;
	h = (wWinPix_t)fh;
	LOG(log_mapsize, 2, ("  ChangeMapScale mapD.scale=%0.3f w=%ld h=%ld\n",
	                     mapD.scale, w, h));

	wDrawSetSize(mapD.d, w, h, NULL);
	MapRedraw(mapD.d, NULL, 0, 0);
}

wBool_t MapGetVisiblePref(void)
{

	// visibility toggle for map window
	// get the start value
	long mapVisible_long;
	wPrefGetInteger("map", "visible", &mapVisible_long, 1);
	mapVisible = mapVisible_long ? TRUE : FALSE;

	return mapVisible;
}

/**
 * Toggle visibility state of map window. Additional flag to avoid recursive calls while
 * UI elements in toolbar and menu are sync'ed
 */

EXPORT void MapWindowToggleShow(void* unused)
{
	static int inTransition = FALSE;

	if (!inTransition) {
		inTransition = TRUE;
		MapWindowShow(!mapVisible);
		inTransition = FALSE;
	}
}

/**
 * Set visibility state of map window.
 *
 * \param state IN TRUE if visible, FALSE if hidden
 */

EXPORT void MapWindowShow(int state)
{
	mapVisible = state;

	if (mapW) {
		wWinShow(mapW, mapVisible | DONTGRABFOCUS);
	}

	if (mapVisible) {
		DoChangeNotification(CHANGE_MAP);
	}

	ToggleSetInMenuToolbar(mapShowMI, mapShowB, mapVisible);

	wPrefSetInteger("map", "visible", mapVisible);
}

wControl_p
MapWindowCreate()
{

	LOG(log_mapsize, 2, ("DrawInit/FormCreateDialog(&mapPG\n"));

	mapW = FormCreateDialog(&mapPG, _("Map"),
	                        NULL, NULL,
	                        NULL, NULL,
	                        FALSE, F_RESIZE, NULL);
	mapD.d = MAPCANVASCONTROL;

	MapWindowShow(MapGetVisiblePref());

	/** \TODO Uncomment and enable ChangeMapScale() */
	//ChangeMapScale();

	return(mapW);
}