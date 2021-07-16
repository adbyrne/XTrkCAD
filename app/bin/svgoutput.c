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
    #define UTFCONVERT
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
#include "draw.h"
#include "include/svgformat.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "paths.h"
#include "track.h"
#include "include/utf8convert.h"
#include "utility.h"
#include "wlib.h"

static struct wFilSel_t * exportSVGFile_fs;
static coOrd roomSize;

/**
 * get line style for element
 *
 * \param  d drawCmd_p to process.
 *
 * \returns the line style
 */

static unsigned
SvgDrawGetLineStyle(drawCmd_p d)
{
    unsigned long notSolid = DC_NOTSOLIDLINE;
    unsigned long opt = d->options & notSolid;
    unsigned lineOpt;

    switch (opt) {
    case DC_DASH:
        lineOpt = wDrawLineDash;
        break;
    case DC_DOT:
        lineOpt = wDrawLineDot;
        break;
    case DC_DASHDOT:
        lineOpt = wDrawLineDashDot;
        break;
    case DC_DASHDOTDOT:
        lineOpt = wDrawLineDashDotDot;
        break;
    case DC_CENTER:
        lineOpt = wDrawLineCenter;
        break;
    case DC_PHANTOM:
        lineOpt = wDrawLinePhantom;
        break;
    default:
        lineOpt = wDrawLineSolid;
        break;
    }

    return (lineOpt);
}

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
    unsigned lineOpt = SvgDrawGetLineStyle(d);

    width = (wDrawWidth)(width > MININMUMLINEWIDTH ? width : MININMUMLINEWIDTH);

    SvgLineCommand((SVGParent *)(d->d),
                   p0.x, roomSize.y - p0.y,
                   p1.x, roomSize.y - p1.y,
                   (double)width,
                   wDrawGetRGB(color),
                   lineOpt);
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
    unsigned lineOpt = SvgDrawGetLineStyle(d);

    if (angle1 >= 360.0) {
        SvgCircleCommand((SVGParent *)(d->d),
                         p.x,
                         roomSize.y - p.y,
                         r,
                         (width > MININMUMLINEWIDTH ? width : MININMUMLINEWIDTH),
                         wDrawGetRGB(color),
                         false,
                         lineOpt);
    } else {
        SvgArcCommand((SVGParent *)(d->d),
                      p.x,
                      roomSize.y-p.y,
                      r,
                      angle0,
                      angle1,
                      drawCenter,
                      (width > MININMUMLINEWIDTH ? width: MININMUMLINEWIDTH),
                      wDrawGetRGB(color),
                      lineOpt);
    }

}

/**
 * Svg draw string. Perform conversion to UTF-8 if required.
 *
 * \param 		   d	    A drawCmd_p to process.
 * \param 		   p	    position of text
 * \param 		   a	    text angle
 * \param [in,out] s	    the string
 * \param 		   fp	    font definition (ignored)
 * \param 		   fontSize Size of the font.
 * \param 		   color    color.
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
    char *text = MyStrdup(s);

#ifdef UTFCONVERT
    text = Convert2UTF8(text);
#endif // UTFCONVERT

    SvgTextCommand((SVGParent *)(d->d),
                   p.x,
                   roomSize.y - p.y,
                   fontSize,
                   wDrawGetRGB(color),
                   text);

    MyFree(text);
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
 * \param 		   d		 A drawCmd_p to process.
 * \param 		   cnt		 Number of points in polyline.
 * \param [in,out] pts		 the coordinates
 * \param [in,out] pointer   If non-null, the pointer.
 * \param 		   color	 color.
 * \param 		   width	 line width.
 * \param 		   fillStyle fill style.
 */

static void SvgDrawFillPoly(
    drawCmd_p d,
    int cnt,
    coOrd * pts,
    int * pointer,
    wDrawColor color, wDrawWidth width, drawFill_e fillStyle)
{
    int i;
    double *points = malloc((cnt + 1) * 2 * sizeof(double));

    unsigned lineOpt = SvgDrawGetLineStyle(d);

    if (!points) {
        puts("memory for poly line coordinates could not be allocated!");
        abort();
    }
    for (i = 0; i < cnt; i++) {
        points[i * 2] = pts[i].x;
        points[i * 2 + 1] = roomSize.y - pts[i].y;
    }

    if (fillStyle == DRAW_CLOSED || fillStyle == DRAW_FILL) {
        points[i * 2] = points[0];
        points[i * 2 + 1] = points[1];
        cnt++;
    }

    width = (wDrawWidth)(width > MININMUMLINEWIDTH ? width : MININMUMLINEWIDTH);
    SvgPolyLineCommand((SVGParent *)(d->d), cnt, points,  wDrawGetRGB(color),
                       (double)width, fillStyle == DRAW_FILL, lineOpt);

    free(points);
}

/**
 * Svg draw filled circle
 *
 * \param  d	  A drawCmd_p to process.
 * \param  center The center.
 * \param  radius The radius.
 * \param  color  The fill color.
 */

static void SvgDrawFillCircle(drawCmd_p d, coOrd center, DIST_T radius,
                              wDrawColor color)
{
    SvgCircleCommand((SVGParent *)(d->d),
                     center.x,
                     roomSize.y - center.y,
                     radius,
                     0,
                     wDrawGetRGB(color),
                     true,
                     0);
}

/**
 * Svg draw rectangle
 *
 * \param  d	   A drawCmd_p to process.
 * \param  corner1 The first corner.
 * \param  corner2 The second corner.
 * \param  color   The color.
 * \param  pattern Specifies the pattern.
 */

static void
SvgDrawRectangle(drawCmd_p d, coOrd corner1, coOrd corner2, wDrawColor color,
                 drawFill_e fillOpt)
{
    SvgRectCommand((SVGParent *)(d->d),
                   corner1.x, roomSize.y - corner1.y,
                   corner2.x, roomSize.y - corner2.y,
                   wDrawGetRGB(color),
                   fillOpt);
}

static drawFuncs_t svgDrawFuncs = {
    SvgDrawLine,
    SvgDrawArc,
    SvgDrawString,
    SvgDrawBitmap,
    SvgDrawFillPoly,
    SvgDrawFillCircle,
    SvgDrawRectangle
};

static drawCmd_t svgD = {
    NULL, &svgDrawFuncs, 0, 1.0, 0.0, {0.0,0.0}, {0.0,0.0}, Pix2CoOrd, CoOrd2Pix, 100.0
};

/**
 * Creates valid identifier from a string. Whitespaces are removed
 * and characters are prepended to make sure the i starts with
 * valid chars.
 * https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/id
 *
 * \param [in,out] base the base for the id.
 *
 * \returns Null if it fails, else the new valid identifier.
 */

static char *
CreateValidId(char *base)
{
    const char *idHead = "id";
    char *out = MyMalloc(strlen(idHead) + strlen(base) + 1);
    char *tmp;
    int j;

    strcpy(out, idHead);
    j = strlen(out);

    for (unsigned int i = 0; i < strlen(base); i++) {
        if (isblank(base[i])) {
            i++;
        } else {
            out[j++] = base[i];
        }
    }

    out[j] = '\0';

    // strip off the extension
    tmp = strchr(out, '.');
    if (tmp) {
        *tmp = '\0';
    }
    return (out);
}

/**
 * get a valid identifier for SVG export
 *
 * \returns Null if it fails, else a pointer to a char.
 */

static char * SvgGetId(void)
{
    char *fileName = GetLayoutFilename();
    char *id = NULL;

    if (fileName) {
        id = CreateValidId(fileName);
#ifdef WINDOWS
        id = Convert2UTF8(id);
#endif
    }

    return (id);
}

/**
 * Set title for SVG file
 * The first title line of the design is used and stored in the SVG file
 *
 * \param  d A drawCmd_p to process.
 */

static void SvgSetTitle(drawCmd_p d)
{
    char *tmp = GetLayoutTitle();
    char *title;

    if (tmp) {
        title = MyStrdup(tmp);
#ifdef WINDOWS
        title = Convert2UTF8(title);
#endif
        SvgAddTitle((SVGParent *)(d->d), title);
        MyFree(title);
    }

}

/**
 * Executes the export tracks to SVG operation
 *
 * \param 		   cnt	    Number of filenames, has to be 1
 * \param [in]     fileName filename of the export file.
 * \param [in]     data	    If non-null, the data.
 *
 * \returns TRUE on success, FALSE on failure
 */

static int DoExportSVGTracks(
    int cnt,
    char ** fileName,
    void * data)
{
    DynString command = NaS;
    SVGDocument *svg;
    SVGParent *svgData;
    char *id;

    assert(fileName != NULL);
    assert(cnt == 1);

    SetCLocale();
    GetLayoutRoomSize(&roomSize);

    SetCurrentPath(SVGPATHKEY, fileName[ 0 ]);

    svg = SvgCreateDocument();
    id = SvgGetId();
    svgData = SvgPrologue(svg, id, 0, 0.0, 0.0, roomSize.x, roomSize.y);
    MyFree(id);

    wSetCursor(mainD.d, wCursorWait);
//    time(&clock);

    svgD.d = (wDraw_p)svgData;

    DrawSelectedTracks(&svgD);
    SvgAddCSSStyle((SVGParent *)svgD.d);
    SvgSetTitle(&svgD);						// make sure this is the last element

    if (!SvgSaveFile(svg, fileName[0])) {
        NoticeMessage(MSG_OPEN_FAIL, _("Cancel"), NULL, "SVG", fileName[0],
                      strerror(errno));

        SvgDestroyDocument(svg);
        wSetCursor(mainD.d, wCursorNormal);
        SetUserLocale();
        return FALSE;
    }
    SvgDestroyDocument(svg);
    Reset();	/**<TODO: was tut das? */
    wSetCursor(mainD.d, wCursorNormal);
    SetUserLocale();
    return TRUE;
}

/**
 * Create and show the dialog for selecting the DXF export filename
 */

void DoExportSVG(void)
{
    assert(selectedTrackCount > 0);

    if (exportSVGFile_fs == NULL)
        exportSVGFile_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("Export to SVG"),
                                         sSVGFilePattern, DoExportSVGTracks, NULL);

    wFilSelect(exportSVGFile_fs, GetCurrentPath(SVGPATHKEY));
}


