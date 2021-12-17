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

#include <xtrkcad-config.h>

#include <dynstring.h>

#include "cselect.h"
#include "custom.h"
#include "dxfformat.h"
#include "fileio.h"
#include "paths.h"
#include "track.h"
#include "draw.h"
#include "common-ui.h"

static struct wFilSel_t * exportDXFFile_fs;

static void DxfLine(
    drawCmd_p d,
    coOrd p0,
    coOrd p1,
    wDrawWidth width,
    wDrawColor color)
{
	long c = wDrawGetRGB( color );
	long s = d->options & DC_DASH ? 1 : (d->options & DC_DOT ? 2 : 0);

	DynString command = NaS;
    DynStringMalloc(&command, 100);
    DxfLineCommand(&command,
                   curTrackLayer + 1,
                   p0.x, p0.y,
                   p1.x, p1.y,
                   s,
		           c);
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
	long c = wDrawGetRGB( color );
	long s = d->options & DC_DASH ? 1 : (d->options & DC_DOT ? 2 : 0);

	DynString command = NaS;
    DynStringMalloc(&command, 100);
    angle0 = NormalizeAngle(90.0-(angle0+angle1));

    if (angle1 >= 360.0) {
        DxfCircleCommand(&command,
                         curTrackLayer + 1,
                         p.x,
                         p.y,
                         r,
                         s,
			             c);
    } else {
        DxfArcCommand(&command,
                      curTrackLayer + 1,
                      p.x,
                      p.y,
                      r,
                      angle0,
                      angle1,
                      s,
			          c);
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
	long c = wDrawGetRGB( color );
	DynString command = NaS;
    DynStringMalloc(&command, 100);
    DxfTextCommand(&command,
                   curTrackLayer + 1,
                   p.x,
                   p.y,
                   fontSize,
                   s,
		           c);
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

static void DxfPoly(
    drawCmd_p d,
    int cnt,
    coOrd * pts,
	int * types,
    wDrawColor color,
	wDrawWidth width,
	drawFill_e eOpts )
{
    int inx;

    for (inx=1; inx<cnt; inx++) {
        DxfLine(d, pts[inx-1], pts[inx], width, color);
    }
    if (eOpts != DRAW_OPEN)
    	DxfLine(d, pts[cnt-1], pts[0], width, color);
}

static void DxfFillCircle(drawCmd_p d, coOrd center, DIST_T radius,
                          wDrawColor color)
{
    DxfArc(d, center, radius, 0.0, 360, FALSE, 0, color);
}


static void DxfRectangle(drawCmd_p d, coOrd orig, coOrd size, wDrawColor color, drawFill_e eOpts)
{
	coOrd p[4];
	// p1 p2
	// p0 p3
	p[0].x = p[1].x = orig.x;
	p[2].x = p[3].x = orig.x+size.x;
	p[0].y = p[3].y = orig.y;
	p[1].y = p[2].y = orig.y+size.y;
	DxfPoly( d, 4, p, NULL, color, 0, eOpts );
}


static drawFuncs_t dxfDrawFuncs = {
    DxfLine,
    DxfArc,
    DxfString,
    DxfBitMap,
    DxfPoly,
    DxfFillCircle,
    DxfRectangle
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

    SetCLocale();
    wSetCursor(mainD.d, wCursorWait);
    time(&clock);
 
	DxfPrologue(&command, 10, 0.0, 0.0, mapD.size.x, mapD.size.y);
	fputs(DynStringToCStr(&command), dxfF);
	dxfD.d = (wDraw_p)dxfF;

    DrawSelectedTracks(&dxfD);

	DynStringClear(&command);
	DxfEpilogue(&command);
	fputs(DynStringToCStr(&command), dxfF);

    fclose(dxfF);
    SetUserLocale();
    Reset();
    wSetCursor(mainD.d, defaultCursor);
    return TRUE;
}

/**
* Create and show the dialog for selected the DXF export filename
*/

void DoExportDXF(void* unused )
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


