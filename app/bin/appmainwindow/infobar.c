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

#include "draw.h"
#include "misc.h"
#include "param.h"


static wWinPix_t infoHeight;
static wWinPix_t textHeight;

#define XLABEL "X: "
#define YLABEL "Y: "
#define ZOOMLABEL "Zoom: "

static wWinPix_t info_yb_offset = 2;
static wWinPix_t info_xm_offset = 2;
static wWinPix_t messageOrControlX = 0;
static wWinPix_t messageOrControlY = 0;
#define NUM_INFOCTL				(4)
static wControl_p curInfoControl[NUM_INFOCTL];
static wWinPix_t curInfoLabelWidth[NUM_INFOCTL];

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
	if (mainD.scale >= 1.0) {
		sprintf(message, "%s%.4g:1", ZOOMLABEL, lround(mainD.scale * 4.0) / 4.0);
	}
	else {
		sprintf(message, "%s1:%.4g", ZOOMLABEL, lround((1.0 / mainD.scale) * 4.0) / 4.0);
	}

	wStatusSetValue(infoD.scale_m, message);
}

void InfoPos(coOrd pos)
{
	sprintf(message, "%s%s", XLABEL, FormatDistance(pos.x));
	wStatusSetValue(infoD.posX_m, message);
	sprintf(message, "%s%s", YLABEL, FormatDistance(pos.y));
	wStatusSetValue(infoD.posY_m, message);

	oldMarker = pos;
}

static wControl_p deferSubstituteControls[NUM_INFOCTL + 1];
static char* deferSubstituteLabels[NUM_INFOCTL];

EXPORT void InfoSubstituteControls(
	wControl_p* controls,
	char** labels)
{
	wWinPix_t x, y;
	int inx;
	for (inx = 0; inx < NUM_INFOCTL; inx++) {
		if (curInfoControl[inx]) {
			wControlShow(curInfoControl[inx], FALSE);
			curInfoControl[inx] = NULL;
		}
		curInfoLabelWidth[inx] = 0;
		curInfoControl[inx] = NULL;
	}
	if (inError && (controls != NULL && controls[0] != NULL)) {
		memcpy(deferSubstituteControls, controls, sizeof deferSubstituteControls);
		memcpy(deferSubstituteLabels, labels, sizeof deferSubstituteLabels);
	}
	if (inError || controls == NULL || controls[0] == NULL) {
		wControlSetPos((wControl_p)infoD.info_m, messageOrControlX, messageOrControlY);
		wControlShow((wControl_p)infoD.info_m, TRUE);
		return;
	}
	//x = wControlGetPosX( (wControl_p)infoD.info_m );
	x = messageOrControlX;
	y = messageOrControlY;
	wStatusSetValue(infoD.info_m, "");
	wControlShow((wControl_p)infoD.info_m, FALSE);
	for (inx = 0; controls[inx]; inx++) {
		curInfoLabelWidth[inx] = wLabelWidth(_(labels[inx]));
		x += curInfoLabelWidth[inx];
#ifdef WINDOWS
		wWinPix_t	y_this = y + (infoHeight / 2) - (textHeight / 2);
#else
		wWinPix_t	y_this = y + (infoHeight / 2) - (wControlGetHeight(
			controls[inx]) / 2) - 2;
#endif
		wControlSetPos(controls[inx], x, y_this);
		x += wControlGetWidth(controls[inx]);
		wControlSetLabel(controls[inx], _(labels[inx]));
		wControlShow(controls[inx], TRUE);
		curInfoControl[inx] = controls[inx];
		x += 3;
	}
	wControlSetPos((wControl_p)infoD.info_m, x, y);
	curInfoControl[inx] = NULL;
	deferSubstituteControls[0] = NULL;
}

void SetMessage(char* msg)
{
	wStatusSetValue(infoD.info_m, msg);
}


void
InfoCount(int count)
{
/** \todo Remove all references and then this dummy function */
}

