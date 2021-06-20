/** \file svgoutput.c
 * Exporting SVG files
*/

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2020 Martin Fischer
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
#include <mxml.h>
#include <dynstring.h>

#include "cselect.h"
#include "custom.h"
#include "include/svgformat.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "paths.h"
#include "track.h"
#include "utility.h"

static struct wFilSel_t * exportSVGFile_fs;

/**
 * Svg draw line
 *
 * \param  d	 A drawCmd_p to process.
 * \param  p0    The p 0.
 * \param  p1    The first coOrd.
 * \param  width The width.
 * \param  color The color.
 */

static void SvgDrawLine(
    drawCmd_p d,
    coOrd p0,
    coOrd p1,
    wDrawWidth width,
    wDrawColor color)
{
    SvgLineCommand((SVGParent *)(d->d),
                   p0.x, p0.y,
                   p1.x, p1.y,
                   width,
				   color);
}

/**
 * Svg draw arc
 *
 * \param  d		  A drawCmd_p to process.
 * \param  p		  A coOrd to process.
 * \param  r		  A DIST_T to process.
 * \param  angle0	  The angle 0.
 * \param  angle1	  The first angle.
 * \param  drawCenter The draw center.
 * \param  width	  The width.
 * \param  color	  The color.
 */

static void SvgDrawArc(
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
        SvgCircleCommand(&command,
                         curTrackLayer + 1,
                         p.x,
                         p.y,
                         r,
                         ((d->options&DC_DASH) != 0));
    } else {
        SvgArcCommand(&command,
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

/**
 * Svg draw string
 *
 * \param 		   d	    A drawCmd_p to process.
 * \param 		   p	    A coOrd to process.
 * \param 		   a	    An ANGLE_T to process.
 * \param [in,out] s	    If non-null, a char to process.
 * \param 		   fp	    The fp.
 * \param 		   fontSize Size of the font.
 * \param 		   color    The color.
 */

static void SvgDrawString(
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
    SvgTextCommand(&command,
                   curTrackLayer + 1,
                   p.x,
                   p.y,
                   fontSize,
                   s);
    fputs(DynStringToCStr(&command), (FILE *)d->d);
    DynStringFree(&command);
}

/**
 * Svg draw bitmap
 *
 * \param  d	 A drawCmd_p to process.
 * \param  p	 A coOrd to process.
 * \param  bm    The bm.
 * \param  color The color.
 */

static void SvgDrawBitmap(
    drawCmd_p d,
    coOrd p,
    wDrawBitMap_p bm,
    wDrawColor color)
{
}

/**
 * Svg draw fill polygon
 *
 * \param 		   d	 A drawCmd_p to process.
 * \param 		   cnt   Number of.
 * \param [in,out] pts   If non-null, the points.
 * \param 		   color The color.
 */

static void SvgDrawFillPoly(
    drawCmd_p d,
    int cnt,
    coOrd * pts,
    wDrawColor color)
{
    int inx;

    for (inx=1; inx<cnt; inx++) {
        SvgDrawLine(d, pts[inx-1], pts[inx], 0, color);
    }

    SvgDrawLine(d, pts[cnt-1], pts[0], 0, color);
}

static void SvgDrawFillCircle(drawCmd_p d, coOrd center, DIST_T radius,
                          wDrawColor color)
{
    SvgDrawArc(d, center, radius, 0.0, 360, FALSE, 0, color);
}


static drawFuncs_t svgDrawFuncs = {
    0,
    SvgDrawLine,
    SvgDrawArc,
    SvgDrawString,
    SvgDrawBitmap,
    SvgDrawFillPoly,
    SvgDrawFillCircle
};





static drawCmd_t svgD = {
    NULL, &svgDrawFuncs, 0, 1.0, 0.0, {0.0,0.0}, {0.0,0.0}, Pix2CoOrd, CoOrd2Pix, 100.0
};

static int DoExportSVGTracks(
    int cnt,
    char ** fileName,
    void * data)
{
    time_t clock;
	DynString command = NaS;
	SVGDocument *svg;
	SVGParent *svgData;
	coOrd roomSize;
	char *oldLocale;
	
    assert(fileName != NULL);
    assert(cnt == 1);

	oldLocale = SaveLocale("C");
	GetLayoutRoomSize(&roomSize);

	SetCurrentPath(SVGPATHKEY, fileName[ 0 ]);

	svg = SvgCreateDocument();
	svgData = SvgPrologue(svg, 0, 0.0, 0.0, roomSize.x, roomSize.y);
    
    wSetCursor(NULL, wCursorWait);
//    time(&clock);
 
	svgD.d = (wDraw_p)svgData;

    DrawSelectedTracks(&svgD);
	if( !SvgSaveFile(svg, fileName[0] )) {
		NoticeMessage(MSG_OPEN_FAIL, _("Cancel"), NULL, "SVG", fileName[0],
			strerror(errno));
		wSetCursor(NULL, wCursorNormal);
		RestoreLocale(oldLocale);
		return FALSE;
	}

    Reset();	/**<TODO: was tut das? */
	RestoreLocale(oldLocale);
    return TRUE;
}

/** Create and show the dialog for selected the DXF export filename */


void DoExportSVG(void)
{
    assert(selectedTrackCount > 0);

    if (exportSVGFile_fs == NULL)
        exportSVGFile_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("Export to SVG"),
                                         sSVGFilePattern, DoExportSVGTracks, NULL);

    wFilSelect(exportSVGFile_fs, GetCurrentPath(SVGPATHKEY));
}


