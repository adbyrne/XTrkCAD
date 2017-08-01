/** \file dxfoutput.c
 * Exporting DXF files
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef WINDOWS
  #include <io.h>
  #include <windows.h>
#else
  #include <errno.h>
#endif

#include <xtrkcad-config.h>
#include <locale.h>
#include <assert.h>

#include <dynstring.h>

#include "cselect.h"
#include "custom.h"
#include "dxfformat.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "paths.h"
#include "track.h"
#include "utility.h"

static struct wFilSel_t * exportDXFFile_fs;

static void DxfLine(
    drawCmd_p d,
    coOrd p0,
    coOrd p1,
    wDrawWidth width,
    wDrawColor color)
{
    DynString command = NaS;
    DynStringMalloc(&command, 100);
    DxfLineCommand(&command,
                   curTrackLayer + 1,
                   p0.x, p0.y,
                   p1.x, p1.y,
                   ((d->options&DC_DASH) != 0));
    fputs(DynStringToCStr(&command), (FILE *)d->d);
    DynStringFree(&command);
}

static void DxfArc(
    drawCmd_p d,
    coOrd p,
    DIST_T r,
    ANGLE_T angle0,
    ANGLE_T angle1,
    BOOL_T drawCenter,
    wDrawWidth width,
    wDrawColor color)
{
    DynString command = NaS;
    DynStringMalloc(&command, 100);
    angle0 = NormalizeAngle(90.0-(angle0+angle1));

    if (angle1 >= 360.0) {
        DxfCircleCommand(&command,
                         curTrackLayer + 1,
                         p.x,
                         p.y,
                         r,
                         ((d->options&DC_DASH) != 0));
    } else {
        DxfArcCommand(&command,
                      curTrackLayer + 1,
                      p.x,
                      p.y,
                      r,
                      angle0,
                      angle1,
                      ((d->options&DC_DASH) != 0));
    }

    fputs(DynStringToCStr(&command), (FILE *)d->d);
    DynStringFree(&command);
}

static void DxfString(
    drawCmd_p d,
    coOrd p,
    ANGLE_T a,
    char * s,
    wFont_p fp,
    FONTSIZE_T fontSize,
    wDrawColor color)
{
    DynString command = NaS;
    DynStringMalloc(&command, 100);
    DxfTextCommand(&command,
                   curTrackLayer + 1,
                   p.x,
                   p.y,
                   fontSize,
                   s);
    fputs(DynStringToCStr(&command), (FILE *)d->d);
    DynStringFree(&command);
}

static void DxfBitMap(
    drawCmd_p d,
    coOrd p,
    wDrawBitMap_p bm,
    wDrawColor color)
{
}

static void DxfFillPoly(
    drawCmd_p d,
    int cnt,
    coOrd * pts,
    wDrawColor color)
{
    int inx;

    for (inx=1; inx<cnt; inx++) {
        DxfLine(d, pts[inx-1], pts[inx], 0, color);
    }

    DxfLine(d, pts[cnt-1], pts[0], 0, color);
}

static void DxfFillCircle(drawCmd_p d, coOrd center, DIST_T radius,
                          wDrawColor color)
{
    DxfArc(d, center, radius, 0.0, 360, FALSE, 0, color);
}


static drawFuncs_t dxfDrawFuncs = {
    0,
    DxfLine,
    DxfArc,
    DxfString,
    DxfBitMap,
    DxfFillPoly,
    DxfFillCircle
};

static drawCmd_t dxfD = {
    NULL, &dxfDrawFuncs, 0, 1.0, 0.0, {0.0,0.0}, {0.0,0.0}, Pix2CoOrd, CoOrd2Pix, 100.0
};

static int DoExportDXFTracks(
    int cnt,
    char ** fileName,
    void * data)
{
    time_t clock;
    char *oldLocale;
	DynString command = NaS;
	FILE * dxfF;

    assert(fileName != NULL);
    assert(cnt == 1);

	DynStringMalloc(&command, 100);

	SetCurrentPath(DXFPATHKEY, fileName[ 0 ]);
    dxfF = fopen(fileName[0], "w");

    if (dxfF==NULL) {
        NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, "DXF", fileName[0],
                      strerror(errno));
        return FALSE;
    }

    oldLocale = SaveLocale("C");
    wSetCursor(wCursorWait);
    time(&clock);
 
	DxfPrologue(&command, 10, 0.0, 0.0, mapD.size.x, mapD.size.y);
	fputs(DynStringToCStr(&command), dxfF);
	dxfD.d = (wDraw_p)dxfF;

    DrawSelectedTracks(&dxfD);

	DynStringClear(&command);
	DxfEpilogue(&command);
	fputs(DynStringToCStr(&command), dxfF);

    fclose(dxfF);
    RestoreLocale(oldLocale);
    Reset();
    wSetCursor(wCursorNormal);
    return TRUE;
}

/**
* Create and show the dialog for selected the DXF export filename
*/

void DoExportDXF(void)
{
    //if (selectedTrackCount <= 0) {
    //    ErrorMessage(MSG_NO_SELECTED_TRK);
    //    return;
    //}
    assert(selectedTrackCount > 0);

    if (exportDXFFile_fs == NULL)
        exportDXFFile_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("Export to DXF"),
                                         sDXFFilePattern, DoExportDXFTracks, NULL);

    wFilSelect(exportDXFFile_fs, GetCurrentPath(DXFPATHKEY));
}


