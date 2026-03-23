/**
 * \file   infobar.c
 * \brief  Infobar / statusbar functions
 *
 * \author Martin Fischer
 */

 /*  XTrackCad - Model Railroad CAD
  *  Copyright (C) 2005, 2024 Dave Bullis
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

#include <string.h>

#include <wlib.h>

#include "form.h"

#define XLABEL "X: "
#define YLABEL "Y: "
#define ZOOMLABEL "Zoom: "

static struct {
	wControl_p scale_m;
	wControl_p posX_m;
	wControl_p posY_m;
	wControl_p info_m;
} infoD;

/**
 * Determine the width of a mouse pointer position string ( coordinate plus label ).
 *
 * \return length of position string 
 */
static size_t GetInfoPosLength(const char *fixedLabel)
{
	DIST_T dist;
	if (mapD.size.x > mapD.size.y) {
		dist = mapD.size.x;
	}
	else {
		dist = mapD.size.y;
	}
	if (units == UNITS_METRIC) {
		dist *= 2.54;
		if (dist >= 1000) {
			dist = 9999.999 * 2.54;
		}
		else if (dist >= 100) {
			dist = 999.999 * 2.54;
		}
		else if (dist >= 10) {
			dist = 99.999 * 2.54;
		}
	}
	else {
		if (dist >= 100 * 12) {
			dist = 999.0 * 12.0 + 11.0 + 3.0 / 4.0 - 1.0 / 64.0;
		}
		else if (dist >= 10 * 12) {
			dist = 99.0 * 12.0 + 11.0 + 3.0 / 4.0 - 1.0 / 64.0;
		}
		else if (dist >= 1 * 12) {
			dist = 9.0 * 12.0 + 11.0 + 3.0 / 4.0 - 1.0 / 64.0;
		}
	}

	return( strlen(FormatDistance(dist)) + strlen(fixedLabel));
}

static void
SetInfoPositionLength(wControl_p control, const char* labelString)
{
	size_t length = GetInfoPosLength(labelString);
	wMessageSetLength(control, length);
}


static void
SetInfoMessageHeight(wControl_p label)
{
	wStatusSetRequiredHeight(label, 0);

}
  /**
   * Initialize the status line at the bottom of the window.
   *
   */

void InitInfoBar(void)
{
	infoD.scale_m = wStatusCreate(mainW, "infoBarScale", ZOOMLABEL);
	infoD.posX_m = wStatusCreate(mainW,  "infoBarPosX", XLABEL);
	infoD.posY_m = wStatusCreate(mainW, "infoBarPosY", YLABEL);
	infoD.info_m = wStatusCreate(mainW, "infoBarStatus", "");

	SetInfoMessageHeight(infoD.info_m);

	SetInfoPositionLength(infoD.posX_m, ZOOMLABEL);
	SetInfoPositionLength(infoD.posX_m, XLABEL);
	SetInfoPositionLength(infoD.posY_m, YLABEL);
}

void SetInfoBar(void)
{
	static long oldDistanceFormat = -1;
	long newDistanceFormat = GetDistanceFormat();

	if (newDistanceFormat != oldDistanceFormat) {
		SetInfoPositionLength(infoD.posX_m, XLABEL);
		SetInfoPositionLength(infoD.posY_m, YLABEL);
	}
}


void InfoScale(void)
{
	char zoomString[sizeof(ZOOMLABEL) + 10 ];

	if (mainD.scale >= 1.0) {
		snprintf(zoomString, sizeof(zoomString),"%s%.4g:1", ZOOMLABEL, lround(mainD.scale * 4.0) / 4.0);
	}
	else {
		snprintf(zoomString, sizeof(zoomString), "%s1:%.4g", ZOOMLABEL, lround((1.0 / mainD.scale) * 4.0) / 4.0);
	}

	wStatusSetValue(infoD.scale_m, zoomString);
}

#define POSSTRINGLEN 20

void InfoPos(coOrd pos)
{
	char posString[POSSTRINGLEN];
	snprintf(posString, POSSTRINGLEN, "%s%s", XLABEL, FormatDistance(pos.x));
	wStatusSetValue(infoD.posX_m, posString);
	snprintf(posString, POSSTRINGLEN, "%s%s", YLABEL, FormatDistance(pos.y));
	wStatusSetValue(infoD.posY_m, posString);

	oldMarker = pos;
}

EXPORT void InfoSubstituteControls(
	const char * name,
	wControl_p* controls, 
	char** labels)
{
	wStatusSetVisibleControlSet(mainW, name);
}

void InfoSetControls(wControl_p window, const char* name)
{
	wStatusSetVisibleControlSet(mainW, name);
}

void InfoDefaultControls(void)
{
	wStatusSetVisibleControlSet(mainW, "message" );
}

void SetMessage(char* msg)
{
	InfoDefaultControls();
	wStatusSetValue(infoD.info_m, msg);
}


