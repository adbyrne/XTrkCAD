/** \file layout.c
 * Layout data and dialog
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2017 Martin Fischer
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

#include <string.h>
#include <dynstring.h>
#include "paths.h"
#include "track.h"

#include "i18n.h"

struct sDataLayout {
    DynString	fullFileName;
	coOrd		roomSize;
};

static struct sDataLayout thisLayout;

static paramFloatRange_t r0_90 = { 0, 90 };
static paramFloatRange_t r1_10000 = { 1, 10000 };
static paramFloatRange_t r1_9999999 = { 1, 9999999 };

static void LayoutDlgUpdate(paramGroup_p pg, int inx, void * valueP);

/**
* Update the full file name. Do not do anything if the new filename is identical to the old one.
*
* \param filename IN the new filename
*/

void
SetLayoutFullPath(const char *fileName)
{
    if (DynStringToCStr(&thisLayout.fullFileName) != fileName) {
        if (isnas(&thisLayout.fullFileName)) {
            DynStringMalloc(&thisLayout.fullFileName, strlen(fileName) + 1);
        } else {
            DynStringClear(&thisLayout.fullFileName);
        }

        DynStringCatCStr(&thisLayout.fullFileName, fileName);
    }
}

/**
* Return the full filename.
*
* \return    pointer to the full filename, should not be modified or freed
*/

char *
GetLayoutFullPath()
{
    return (DynStringToCStr(&thisLayout.fullFileName));
}

/**
* Return the filename part of the full path
*
* \return    pointer to the filename part, NULL is no filename is set
*/

char *
GetLayoutFilename()
{
    char *string = DynStringToCStr(&thisLayout.fullFileName);

    if (string) {
        return (FindFilename(string));
    } else {
        return (NULL);
    }
}

/****************************************************************************
*
* Layout Dialog
*
*/

static wWin_p layoutW;
static coOrd newSize;

static paramData_t layoutPLs[] = {
	{ PD_FLOAT, &newSize.x, "roomsizeX", PDO_NOPREF | PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &r1_9999999, N_("Room Width"), 0, (void*)(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_FLOAT, &newSize.y, "roomsizeY", PDO_NOPREF | PDO_DIM | PDO_NOPSHUPD | PDO_DRAW | PDO_DLGHORZ, &r1_9999999, N_("    Height"), 0, (void*)(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_STRING, &Title1, "title1", PDO_NOPSHUPD, NULL, N_("Layout Title") },
	{ PD_STRING, &Title2, "title2", PDO_NOPSHUPD, NULL, N_("Subtitle") },
#define SCALEINX (4)
	{ PD_DROPLIST, &curScaleDescInx, "scale", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT, (void *)120, N_("Scale"), 0, (void*)(CHANGE_SCALE) },
#define GAUGEINX (5)
	{ PD_DROPLIST, &curGaugeInx, "gauge", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT | PDO_DLGHORZ, (void *)120, N_("     Gauge"), 0, (void *)(CHANGE_SCALE) },
#define MINRADIUSENTRY (6)
	{ PD_FLOAT, &minTrackRadius, "mintrackradius", PDO_DIM | PDO_NOPSHUPD | PDO_NOPREF, &r1_10000, N_("Min Track Radius"), 0, (void*)(CHANGE_MAIN | CHANGE_LIMITS) },
	{ PD_FLOAT, &maxTrackGrade, "maxtrackgrade", PDO_NOPSHUPD | PDO_DLGHORZ, &r0_90 , N_(" Max Track Grade"), 0, (void*)(CHANGE_MAIN) }
};


static paramGroup_t layoutPG = { "layout", PGO_RECORD | PGO_PREFMISC, layoutPLs, sizeof layoutPLs / sizeof layoutPLs[0] };


static void LayoutOk(void * junk)
{
	long changes;
	char prefString[30];

	changes = GetChanges(&layoutPG);

	/* [mf Nov. 15, 2005] Get the gauge/scale settings */
	if (changes & CHANGE_SCALE) {
		SetScaleGauge(curScaleDescInx, curGaugeInx);
	}
	/* [mf Nov. 15, 2005] end */

	if (changes & CHANGE_MAP) {
		SetRoomSize(newSize);
	}

	wHide(layoutW);
	DoChangeNotification(changes);

	if (changes & CHANGE_LIMITS) {
		// now set the minimum track radius
		sprintf(prefString, "minTrackRadius-%s", curScaleName);
		wPrefSetFloat("misc", prefString, minTrackRadius);
	}
}


static void LayoutChange(long changes)
{
	if (changes & (CHANGE_SCALE | CHANGE_UNITS))
		if (layoutW != NULL && wWinIsVisible(layoutW))
			ParamLoadControls(&layoutPG);
}

void DoLayout(void * junk)
{
	newSize = mapD.size;
	if (layoutW == NULL) {
		layoutW = ParamCreateDialog(&layoutPG, MakeWindowTitle(_("Layout Options")), _("Ok"), LayoutOk, wHide, TRUE, NULL, 0, LayoutDlgUpdate);
		LoadScaleList((wList_p)layoutPLs[4].control);
	}
	LoadGaugeList((wList_p)layoutPLs[5].control, curScaleDescInx); /* set correct gauge list here */
	ParamLoadControls(&layoutPG);
	wShow(layoutW);
}



EXPORT addButtonCallBack_t LayoutInit(void)
{
	ParamRegister(&layoutPG);
	RegisterChangeNotification(LayoutChange);
	return &DoLayout;
}

static void
LayoutDlgUpdate(
	paramGroup_p pg,
	int inx,
	void * valueP)
{
	char prefString[100];
	char scaleDesc[100];

	/* did the scale change ? */
	if (inx == SCALEINX) {
		LoadGaugeList((wList_p)layoutPLs[GAUGEINX].control, *((int *)valueP));
		// set the first entry as default, usually the standard gauge for a scale
		wListSetIndex((wList_p)layoutPLs[GAUGEINX].control, 0);

		// get the minimum radius
		// get the selected scale first
		wListGetValues((wList_p)layoutPLs[SCALEINX].control, scaleDesc, 99, NULL, NULL);
		strtok(scaleDesc, " ");

		// now get the minimum track radius
		sprintf(prefString, "minTrackRadius-%s", scaleDesc);
		wPrefGetFloat("misc", prefString, &minTrackRadius, 0.0);

		// put the scale's minimum value into the dialog
		wStringSetValue((wString_p)layoutPLs[MINRADIUSENTRY].control, FormatDistance(minTrackRadius));
	}
}
