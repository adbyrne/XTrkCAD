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

#include "custom.h"
#include "i18n.h"
#include "layout.h"
#include "misc2.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "wlib.h"

#define MINTRACKRADIUSPREFS "minTrackRadius"

struct sLayoutProps {
    char			title1[TITLEMAXLEN];
    char			title2[TITLEMAXLEN];
    SCALEINX_T		curScaleInx;
    SCALEDESCINX_T	curScaleDescInx;
    GAUGEINX_T		curGaugeInx;
    DIST_T			minTrackRadius;
    DIST_T			maxTrackGrade;
    coOrd			roomSize;
    DynString       backgroundFileName;
    coOrd			backgroundPos;
    ANGLE_T			backgroundAngle;
    int				backgroundAlpha;
    double 			backgroundWidth;

};

struct sDataLayout {
    struct sLayoutProps props;
    DynString	fullFileName;
    struct sLayoutProps *copyOfLayoutProps;
};

struct sDataLayout thisLayout = {
    { "", "", -1, 0, 0, 0.0, 5.0, {0.0, 0.0} },
    NaS,
    NULL,
};

static paramFloatRange_t r0_90 = { 0, 90 };
static paramFloatRange_t r1_10000 = { 1, 10000 };
static paramFloatRange_t r1_9999999 = { 1, 9999999 };
static paramFloatRange_t r0_360 = { 0, 360 };
static paramFloatRange_t rN_9999999 = { -99999, 99999 };
static paramIntegerRange_t i0_100 = { 0, 100 };

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
* Set the minimum radius for the selected scale/gauge into the dialog
*
* \param scaleName IN name of the scale/gauge eg. HOn3
* \param defaltValue IN default value will be used if no preference is set
*/

void
LoadLayoutMinRadiusPref(char *scaleName, double defaultValue)
{
    DynString prefString = { NULL };

    DynStringPrintf(&prefString, MINTRACKRADIUSPREFS "-%s", scaleName);
    wPrefGetFloat("misc", DynStringToCStr(&prefString),
                  &thisLayout.props.minTrackRadius, defaultValue);
    DynStringFree(&prefString);
}

static void
CopyLayoutTitle(char* dest, char *src)
{
    strncpy(dest, src, TITLEMAXLEN);
    *(dest + TITLEMAXLEN - 1) = '\0';
}

void
SetLayoutTitle(char *title)
{
    CopyLayoutTitle(thisLayout.props.title1, title);
}

void
SetLayoutSubtitle(char *title)
{
    CopyLayoutTitle(thisLayout.props.title2, title);
}

void
SetLayoutMinTrackRadius(DIST_T radius)
{
    thisLayout.props.minTrackRadius = radius;
}

void
SetLayoutMaxTrackGrade(ANGLE_T angle)
{
    thisLayout.props.maxTrackGrade = angle;
}


void
SetLayoutRoomSize(coOrd size)
{
    thisLayout.props.roomSize = size;
}

void
SetLayoutCurScale(SCALEINX_T scale)
{
    thisLayout.props.curScaleInx = scale;
}

void
SetLayoutCurScaleDesc(SCALEDESCINX_T desc)
{
    thisLayout.props.curScaleDescInx = desc;
}

void
SetLayoutCurGauge(GAUGEINX_T gauge)
{
    thisLayout.props.curGaugeInx = gauge;
}

void SetLayoutBackGroundFullPath(const char *fileName) {
	if (DynStringToCStr(&thisLayout.props.backgroundFileName) != fileName) {
	        if (isnas(&thisLayout.props.backgroundFileName)) {
	            DynStringMalloc(&thisLayout.props.backgroundFileName, strlen(fileName) + 1);
	        } else {
	            DynStringClear(&thisLayout.props.backgroundFileName);
	        }

	        DynStringCatCStr(&thisLayout.props.backgroundFileName, fileName);
	    }
}

void SetLayoutBackGroundSize(double size) {
	if (size > 0.0) {
		thisLayout.props.backgroundWidth = size;
	} else {
		thisLayout.props.backgroundWidth = thisLayout.props.roomSize.x;
	}

}

void SetLayoutBackGroundPos(coOrd pos) {
	thisLayout.props.backgroundPos = pos;

}

void SetLayoutBackGroundAngle(ANGLE_T angle) {
	thisLayout.props.backgroundAngle = angle;

}

void SetLayoutBackGroundAlpha(int alpha) {
	thisLayout.props.backgroundAlpha = alpha;

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

char *
GetLayoutTitle()
{
    return (thisLayout.props.title1);
}

char *
GetLayoutSubtitle()
{
    return (thisLayout.props.title2);
}

DIST_T
GetLayoutMinTrackRadius()
{
    return (thisLayout.props.minTrackRadius);
}

ANGLE_T
GetLayoutMaxTrackGrade()
{
    return (thisLayout.props.maxTrackGrade);
}

SCALEDESCINX_T
GetLayoutCurScaleDesc()
{
    return (thisLayout.props.curScaleDescInx);
}

SCALEINX_T
GetLayoutCurScale()
{
    return (thisLayout.props.curScaleInx);
}

char *
GetLayoutBackGroundFullPath()
{
	return (DynStringToCStr(&thisLayout.props.backgroundFileName));
}

double
GetLayoutBackGroundSize()
{
	if (thisLayout.props.backgroundWidth > 0.0) {
		return (thisLayout.props.backgroundWidth);
	} else {
		return (thisLayout.props.roomSize.x);
	}
}

coOrd
GetLayoutBackGroundPos()
{
	return (thisLayout.props.backgroundPos);
}

ANGLE_T
GetLayoutBackGroundAngle()
{
	return (thisLayout.props.backgroundAngle);
}

int GetLayoutBackGroundAlpha()
{
	return (thisLayout.props.backgroundAlpha);
}
/****************************************************************************
*
* Layout Dialog
*
*/
static char backgroundFileName[STR_LONG_SIZE];

#define TEXT_FIELD_LEN 40
static wWin_p layoutW;

void SetName() {
	char * name = GetLayoutBackGroundFullPath();
	if (name) {
		if (name && (strlen(name)<=TEXT_FIELD_LEN)) {
			for (int i=0; i<=strlen(name);i++) {
				backgroundFileName[i] = name[i];
			}
			backgroundFileName[strlen(name)] = '\0';
		} else {
			for (int i=TEXT_FIELD_LEN;i>=0; i--) {
				backgroundFileName[i] = name[strlen(name)-(TEXT_FIELD_LEN-i)-1];
			}
		}
	} else backgroundFileName[0] = '\0';

}

static struct wFilSel_t * imageFile_fs;
static char curImageDir[STR_LONG_SIZE];
#define PARAM_SUBDIR "params"

static paramData_p layout_p;
static paramGroup_t * layout_pg_p;

EXPORT int LoadImageFile(
		int files,
		char ** fileName,
		void * data )
{
		char * noname = "";
		char * error;
		if (files >0) {
			strcpy(backgroundFileName,fileName[0]);
			SetLayoutBackGroundFullPath( strdup(fileName[0]));
			if (wDrawSetBackground(  mainD.d, GetLayoutBackGroundFullPath(), error)==-1) {
				NoticeMessage(_("Unable to load Image File - %s"),_("Ok"),NULL,error);
				SetLayoutBackGroundFullPath(noname);
				SetName();
				ParamLoadControl(layout_pg_p, 8);
				return FALSE;
			}
			SetName();
			ParamLoadControl(layout_pg_p, 8);
			MainRedraw();
			return TRUE;
		}
		SetLayoutBackGroundFullPath(noname);
		SetName();
		ParamLoadControl(layout_pg_p, 8);
		MainRedraw();
		return FALSE;
}

static void ImageFileBrowse( void * junk )
{
	const char * path;
	imageFile_fs = wFilSelCreate( mainW, FS_LOAD, FS_PICTURES, _("Load Background"), _("|"), LoadImageFile, NULL );

	if (!path) {
	     path = wPrefGetString("file", "directory");
	}

	if (!path) {
	      path = wGetUserHomeDir();
	}
	wFilSelect( imageFile_fs, curImageDir );
	return;
}

static void ImageFileClear( void * junk)
{
	char * noname = "";
	SetLayoutBackGroundFullPath(noname);
	wDrawSetBackground(  mainD.d, NULL, NULL);
	SetName();
	ParamLoadControl(layout_pg_p, 8);
	//wStringSetValue((wString_p)layout_p[8].control,backgroundFileName);
	MainRedraw();
}

static paramData_t layoutPLs[] = {
    { PD_FLOAT, &thisLayout.props.roomSize.x, "roomsizeX", PDO_NOPREF | PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &r1_9999999, N_("Room Width"), 0, (void*)(CHANGE_MAIN | CHANGE_MAP) },
    { PD_FLOAT, &thisLayout.props.roomSize.y, "roomsizeY", PDO_NOPREF | PDO_DIM | PDO_NOPSHUPD | PDO_DRAW | PDO_DLGHORZ, &r1_9999999, N_("    Height"), 0, (void*)(CHANGE_MAIN | CHANGE_MAP) },
    { PD_STRING, &thisLayout.props.title1, "title1", PDO_NOPSHUPD | PDO_STRINGLIMITLENGTH, NULL, N_("Layout Title"), 0, (void *)sizeof(thisLayout.props.title1) },
    { PD_STRING, &thisLayout.props.title2, "title2", PDO_NOPSHUPD | PDO_STRINGLIMITLENGTH, NULL, N_("Subtitle"), 0, (void *)sizeof(thisLayout.props.title2) },
#define SCALEINX (4)
    { PD_DROPLIST, &thisLayout.props.curScaleDescInx, "scale", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT, (void *)120, N_("Scale"), 0, (void*)(CHANGE_SCALE) },
#define GAUGEINX (5)
    { PD_DROPLIST, &thisLayout.props.curGaugeInx, "gauge", PDO_NOPREF | PDO_NOPSHUPD | PDO_NORECORD | PDO_NOUPDACT | PDO_DLGHORZ, (void *)120, N_("     Gauge"), 0, (void *)(CHANGE_SCALE) },
#define MINRADIUSENTRY (6)
    { PD_FLOAT, &thisLayout.props.minTrackRadius, "mintrackradius", PDO_DIM | PDO_NOPSHUPD | PDO_NOPREF, &r1_10000, N_("Min Track Radius"), 0, (void*)(CHANGE_MAIN | CHANGE_LIMITS) },
    { PD_FLOAT, &thisLayout.props.maxTrackGrade, "maxtrackgrade", PDO_NOPSHUPD | PDO_DLGHORZ, &r0_90, N_(" Max Track Grade (%)"), 0, (void*)(CHANGE_MAIN) },
#define BACKGROUNDFILEENTRY (8)  //Note this value used in the file section routines above - if it chnages, they will need to change
	{ PD_STRING, &backgroundFileName, "backgroundfile", PDO_NOPSHUPD,  NULL, N_("Background File Path"), 0, (void *)(CHANGE_FILE) },
	{ PD_BUTTON, (void*)ImageFileBrowse, "browse", PDO_DLGHORZ, NULL, N_("Browse ...") },
	{ PD_BUTTON, (void*)ImageFileClear, "clear", PDO_DLGHORZ, NULL, N_("Clear") },
#define BACKGROUNDPOSX (11)
	{ PD_FLOAT, &thisLayout.props.backgroundPos.x, "backgroundposX", PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &rN_9999999, N_("Background PosX,Y"), 0, (void*)(CHANGE_BACK) },
#define BACKGROUNDPOSY (12)
	{ PD_FLOAT, &thisLayout.props.backgroundPos.y, "backgroundposY", PDO_DIM | PDO_NOPSHUPD | PDO_DRAW | PDO_DLGHORZ, &rN_9999999, NULL, 0, (void*)(CHANGE_BACK) },
#define BACKGROUNDWIDTH (13)
	{ PD_FLOAT, &thisLayout.props.backgroundWidth, "backgroundWidth", PDO_DIM | PDO_NOPSHUPD | PDO_DRAW, &r1_9999999, N_("Background Width"), 0, (void*)(CHANGE_BACK) },
#define BACKGROUNDALPHA (14)
	{ PD_LONG, &thisLayout.props.backgroundAlpha, "backgroundAlpha", PDO_NOPSHUPD | PDO_DRAW, &i0_100, N_("Background Alpha"), 0, (void*)(CHANGE_BACK) },
#define BACKGROUNDANGLE (15)
	{ PD_FLOAT, &thisLayout.props.backgroundAngle, "backgroundAngle", PDO_NOPSHUPD | PDO_DRAW, &r0_360, N_("Background Angle"), 0, (void*)(CHANGE_BACK) }

};

static paramGroup_t layoutPG = { "layout", PGO_RECORD | PGO_PREFMISC, layoutPLs, sizeof layoutPLs / sizeof layoutPLs[0] };

/**
* Apply the changes entered to settings
*
* \param junk IN unused
*/

static void LayoutOk(void * junk)
{
    long changes;

    changes = GetChanges(&layoutPG);

    /* [mf Nov. 15, 2005] Get the gauge/scale settings */
    if (changes & CHANGE_SCALE) {
        SetScaleGauge(thisLayout.props.curScaleDescInx, thisLayout.props.curGaugeInx);
    }

    /* [mf Nov. 15, 2005] end */

    if (changes & CHANGE_MAP) {
        SetRoomSize(thisLayout.props.roomSize);
    }

    DoChangeNotification(changes);

    if (changes & CHANGE_LIMITS) {
        char prefString[30];
        // now set the minimum track radius
        sprintf(prefString, "minTrackRadius-%s", curScaleName);
        wPrefSetFloat("misc", prefString, thisLayout.props.minTrackRadius);
    }


    free(thisLayout.copyOfLayoutProps);
    wHide(layoutW);

    MainRedraw();
}

/**
* Discard the changes entered and replace with earlier values
*
* \param junk IN unused
*/

static void LayoutCancel(struct wWin_t *junk)
{
    thisLayout.props = *(thisLayout.copyOfLayoutProps);
    ParamLoadControls(&layoutPG);
    LayoutOk(junk);
}

static void LayoutChange(long changes)
{
    if (changes & (CHANGE_SCALE | CHANGE_UNITS | CHANGE_FILE))
        if (layoutW != NULL && wWinIsVisible(layoutW)) {
            ParamLoadControls(&layoutPG);
        }
}

void DoLayout(void * junk)
{
    thisLayout.props.roomSize = mapD.size;

    if (layoutW == NULL) {
        layoutW = ParamCreateDialog(&layoutPG, MakeWindowTitle(_("Layout Options")),
                                    _("Ok"), LayoutOk, LayoutCancel, TRUE, NULL, 0, LayoutDlgUpdate);
        LoadScaleList((wList_p)layoutPLs[4].control);
    }

    ParamControlActive(&layoutPG, BACKGROUNDFILEENTRY, FALSE);

    LoadGaugeList((wList_p)layoutPLs[5].control,
                  thisLayout.props.curScaleDescInx); /* set correct gauge list here */
    thisLayout.copyOfLayoutProps = malloc(sizeof(struct sLayoutProps));

    if (!thisLayout.copyOfLayoutProps) {
        exit(1);
    }
    SetName();
    *(thisLayout.copyOfLayoutProps) = thisLayout.props;

    ParamLoadControls(&layoutPG);
    wShow(layoutW);
}

EXPORT addButtonCallBack_t LayoutInit(void)
{
    ParamRegister(&layoutPG);
    RegisterChangeNotification(LayoutChange);
    layout_p = layoutPLs;
    layout_pg_p = &layoutPG;
    return &DoLayout;
}

/**
* Update the dialog when scale was changed. The list of possible gauges for the selected scale is
* updated and the first entry is selected (usually standard gauge). After this the minimum gauge
* is set from the preferences.
*
* \param pg IN dialog
* \param inx IN changed entry field
* \param valueP IN new value
*/

static void
LayoutDlgUpdate(
    paramGroup_p pg,
    int inx,
    void * valueP)
{
    /* did the scale change ? */
    if (inx == SCALEINX) {
        char prefString[100];
        char scaleDesc[100];

        LoadGaugeList((wList_p)layoutPLs[GAUGEINX].control, *((int *)valueP));
        // set the first entry as default, usually the standard gauge for a scale
        wListSetIndex((wList_p)layoutPLs[GAUGEINX].control, 0);

        // get the minimum radius
        // get the selected scale first
        wListGetValues((wList_p)layoutPLs[SCALEINX].control, scaleDesc, 99, NULL, NULL);
        strtok(scaleDesc, " ");

        // now get the minimum track radius
        sprintf(prefString, "minTrackRadius-%s", scaleDesc);
        wPrefGetFloat("misc", prefString, &thisLayout.props.minTrackRadius, 0.0);

        // put the scale's minimum value into the dialog
        wStringSetValue((wString_p)layoutPLs[MINRADIUSENTRY].control,
                        FormatDistance(thisLayout.props.minTrackRadius));
    }
    if (inx == BACKGROUNDPOSX) {
    	coOrd pos;
    	pos.x = *(double *)valueP;
    	pos.y = GetLayoutBackGroundPos().y;
    	SetLayoutBackGroundPos(pos);
    	MainRedraw();
    }
    if (inx == BACKGROUNDPOSY) {
    	coOrd pos;
		pos.y = *(double *)valueP;
		pos.x = GetLayoutBackGroundPos().x;
		SetLayoutBackGroundPos(pos);
    	MainRedraw();
    }
    if (inx == BACKGROUNDWIDTH) {
    	SetLayoutBackGroundSize(*(double *)valueP);
    	MainRedraw();
    }
    if (inx == BACKGROUNDALPHA) {
    	SetLayoutBackGroundAlpha(*(int *)valueP);
    	MainRedraw();
    }
    if (inx == BACKGROUNDANGLE) {
    	SetLayoutBackGroundAngle(*(double *)valueP);
    	MainRedraw();
    }

}
