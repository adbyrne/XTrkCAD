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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "cselect.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "misc.h"
#include "param.h"
#include "track.h"
#include "layout.h"
#include "common-ui.h"


EXPORT wIndex_t panCmdInx;

static void DrawRoomWalls( wBool_t );
static void DrawMarkers( void );
static void ConstraintOrig( coOrd *, coOrd, int, int );
static void DoMouse( wAction_t action, coOrd pos );
static void DDrawPoly(
		drawCmd_p d,
		int cnt,
		coOrd * pts,
		int * types,
		wDrawColor color,
		wDrawWidth width,
		drawFill_e eFillOpt );
static void DrawMapBoundingBox( BOOL_T set );
static void DrawTicks( drawCmd_p d, coOrd size );
static void DoZoom( void * pScaleVP );

EXPORT int log_pan = 0;
static int log_zoom = 0;
static int log_mouse = 0;
static int log_redraw = 0;
static int log_timemainredraw = 0;

static wFontSize_t drawMaxTextFontSize = 100;

/****************************************************************************
 *
 * EXPORTED VARIABLES
 *
 */

// static char FAR message[STR_LONG_SIZE];

EXPORT wDrawPix_t closePixels = 10;
EXPORT long maxArcSegStraightLen = 100;
EXPORT long drawCount;
EXPORT BOOL_T drawEnable = TRUE;
EXPORT long currRedraw = 0;

EXPORT coOrd panCenter;
EXPORT coOrd menuPos;

EXPORT wDrawColor drawColorBlack;
EXPORT wDrawColor drawColorWhite;
EXPORT wDrawColor drawColorRed;
EXPORT wDrawColor drawColorBlue;
EXPORT wDrawColor drawColorGreen;
EXPORT wDrawColor drawColorAqua;
EXPORT wDrawColor drawColorPreviewSelected;
EXPORT wDrawColor drawColorPreviewUnselected;
EXPORT wDrawColor drawColorPowderedBlue;
EXPORT wDrawColor drawColorPurple;
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

static wWinPix_t infoHeight;
static wWinPix_t textHeight;
EXPORT wWin_p mapW;
EXPORT BOOL_T mapVisible;
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

static wFont_p rulerFp;

static struct {
		wStatus_p scale_m;
		wStatus_p count_m;
		wStatus_p posX_m;
		wStatus_p posY_m;
		wStatus_p info_m;
		wWinPix_t scale_w;
		wWinPix_t count_w;
		wWinPix_t pos_w;
		wWinPix_t info_w;
		wBox_p scale_b;
		wBox_p count_b;
		wBox_p posX_b;
		wBox_p posY_b;
		wBox_p info_b;
		} infoD;

EXPORT coOrd oldMarker = { 0.0, 0.0 };

EXPORT long dragPixels = 20;
EXPORT long dragTimeout = 500;
EXPORT long autoPan = 0;
EXPORT BOOL_T inError = FALSE;

typedef enum { mouseNone, mouseLeft, mouseRight, mouseLeftPending } mouseState_e;
static mouseState_e mouseState;
static wDrawPix_t mousePositionx, mousePositiony;	/**< position of mouse pointer */

static int delayUpdate = 1;

static char xLabel[] = "X: ";
static char yLabel[] = "Y: ";
static char zoomLabel[] = "Zoom: ";

static struct {
		char * name;
		double value;
		wMenuRadio_p pdRadio;
		wMenuRadio_p btRadio;
		wMenuRadio_p ctxRadio1;
		wMenuRadio_p panRadio;
		} zoomList[] = {
				{ "1:10", 1.0 / 10.0 },
				{ "1:9", 1.0 / 9.0 },
				{ "1:8", 1.0 / 8.0 },
				{ "1:7", 1.0 / 7.0 },
				{ "1:6", 1.0 / 6.0 },
				{ "1:5", 1.0 / 5.0 },
				{ "1:4", 1.0 / 4.0 },
				{ "1:3", 1.0 / 3.0 },
				{ "1:2", 1.0 / 2.0 },
				{ "1:1", 1.0 },
				{ "2:1", 2.0 },
				{ "3:1", 3.0 },
				{ "4:1", 4.0 },
				{ "5:1", 5.0 },
				{ "6:1", 6.0 },
				{ "7:1", 7.0 },
				{ "8:1", 8.0 },
				{ "9:1", 9.0 },
				{ "10:1", 10.0 },
				{ "12:1", 12.0 },
				{ "14:1", 14.0 },
				{ "16:1", 16.0 },
				{ "18:1", 18.0 },
				{ "20:1", 20.0 },
				{ "24:1", 24.0 },
				{ "28:1", 28.0 },
				{ "32:1", 32.0 },
				{ "36:1", 36.0 },
				{ "40:1", 40.0 },
				{ "48:1", 48.0 },
				{ "56:1", 56.0 },
				{ "64:1", 64.0 },
				{ "80:1", 80.0 },
				{ "96:1", 96.0 },
				{ "112:1", 112.0 },
				{ "128:1", 128.0 },
				{ "160:1", 160.0 },
				{ "192:1", 192.0 },
				{ "224:1", 224.0 },
				{ "256:1", 256.0 },
				{ "320:1", 320.0 },
			    { "384:1", 384.0 },
			    { "448:1", 448.0 },
			    { "512:1", 512.0 },
			    { "640:1", 640.0 },
			    { "768:1", 768.0 },
			    { "896:1", 896.0 },
			    { "1024:1", 1024.0 },
		};



/****************************************************************************
 *
 * DRAWING
 *
 */

static void MainCoOrd2Pix( drawCmd_p d, coOrd p, wDrawPix_t * x, wDrawPix_t * y )
{
	DIST_T t;
	if (d->angle != 0.0)
		Rotate( &p, d->orig, -d->angle );
	p.x = (p.x - d->orig.x) / d->scale;
	p.y = (p.y - d->orig.y) / d->scale;
	t = p.x*d->dpi;
	if ( t > 0.0 )
		t += 0.5;
	else
		t -= 0.5;
	*x = ((wDrawPix_t)t) + ((d->options&DC_TICKS)?LBORDER:0);
	t = p.y*d->dpi;
	if ( t > 0.0 )
		t += 0.5;
	else
		t -= 0.5;
	*y = ((wDrawPix_t)t) + ((d->options&DC_TICKS)?BBORDER:0);
}


static int Pix2CoOrd_interpolate = 0;

static void MainPix2CoOrd(
		drawCmd_p d,
		wDrawPix_t px,
		wDrawPix_t py,
		coOrd * posR )
{
	DIST_T x, y;
	DIST_T bins = pixelBins;
	x = ((((POS_T)((px)-LBORDER))/d->dpi)) * d->scale;
	y = ((((POS_T)((py)-BBORDER))/d->dpi)) * d->scale;
	x = (long)(x*bins)/bins;
	y = (long)(y*bins)/bins;
if (Pix2CoOrd_interpolate) {
	DIST_T x1, y1;
	x1 = ((((POS_T)((px-1)-LBORDER))/d->dpi)) * d->scale;
	y1 = ((((POS_T)((py-1)-BBORDER))/d->dpi)) * d->scale;
	x1 = (long)(x1*bins)/bins;
	y1 = (long)(y1*bins)/bins;
	if (x == x1) {
		x += 1/bins/2;
		printf ("px=%0.1f x1=%0.6f x=%0.6f\n", px, x1, x );
	}
	if (y == y1)
		y += 1/bins/2;
}
	x += d->orig.x;
	y += d->orig.y;
	posR->x = x;
	posR->y = y;
}


#define DRAWOPTS( D ) (((D->options&DC_TEMP)?wDrawOptTemp:0)|((D->options&DC_OUTLINE)?wDrawOutlineFont:0))

static void DDrawLine(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		wDrawWidth width,
		wDrawColor color )
{
	wDrawPix_t x0, y0, x1, y1;
	BOOL_T in0 = FALSE, in1 = FALSE;
	coOrd orig, size;
	if (d == &mapD && !mapVisible)
		return;
	if ( (d->options&DC_NOCLIP) == 0 ) {
	if (d->angle == 0.0) {
		in0 = (p0.x >= d->orig.x && p0.x <= d->orig.x+d->size.x &&
			   p0.y >= d->orig.y && p0.y <= d->orig.y+d->size.y);
		in1 = (p1.x >= d->orig.x && p1.x <= d->orig.x+d->size.x &&
			   p1.y >= d->orig.y && p1.y <= d->orig.y+d->size.y);
	}
	if ( (!in0) || (!in1) ) {
		orig = d->orig;
		size = d->size;
		if (d->options&DC_TICKS) {
			orig.x -= LBORDER/d->dpi*d->scale;
			orig.y -= BBORDER/d->dpi*d->scale;
			size.x += (LBORDER+RBORDER)/d->dpi*d->scale;
			size.y += (BBORDER+TBORDER)/d->dpi*d->scale;
		}
		if (!ClipLine( &p0, &p1, orig, d->angle, size ))
			return;
	}
	}
	d->CoOrd2Pix(d,p0,&x0,&y0);
	d->CoOrd2Pix(d,p1,&x1,&y1);
	drawCount++;
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long NotSolid = DC_NOTSOLIDLINE;
	unsigned long opt = d->options&NotSolid;
		if (opt == DC_DASH)
			lineOpt = wDrawLineDash;
		else if(opt == DC_DOT)
			lineOpt = wDrawLineDot;
		else if(opt == DC_DASHDOT)
			lineOpt = wDrawLineDashDot;
		else if (opt == DC_DASHDOTDOT)
			lineOpt = wDrawLineDashDotDot;
		else if(opt == DC_CENTER)
			lineOpt = wDrawLineCenter;
		else if (opt == DC_PHANTOM)
			lineOpt = wDrawLinePhantom;

	if (drawEnable) {
		wDrawLine( d->d, x0, y0, x1, y1,
				width,
				lineOpt,
				color, DRAWOPTS(d) );
	}
}


static void DDrawArc(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		ANGLE_T angle0,
		ANGLE_T angle1,
		BOOL_T drawCenter,
		wDrawWidth width,
		wDrawColor color )
{
    wDrawPix_t x, y;
    ANGLE_T da;
    coOrd p0, p1;
    DIST_T rr;
    int i, cnt;

    if (d == &mapD && !mapVisible)
    {
        return;
    }
    rr = (r / d->scale) * d->dpi + 0.5;
    if (rr > wDrawGetMaxRadius(d->d))
    {
        da = (maxArcSegStraightLen * 180) / (M_PI * rr);
        cnt = (int)(angle1/da) + 1;
        da = angle1 / cnt;
        coOrd min,max;
        min = d->orig;
        max.x = min.x + d->size.x;
        max.y = min.y + d->size.y;
        PointOnCircle(&p0, p, r, angle0);
        for (i=1; i<=cnt; i++) {
            angle0 += da;
            PointOnCircle(&p1, p, r, angle0);
            if (d->angle == 0.0 &&
                    ((p0.x >= min.x &&
                      p0.x <= max.x &&
                      p0.y >= min.y &&
                      p0.y <= max.y) ||
                     (p1.x >= min.x &&
                      p1.x <= max.x &&
                      p1.y >= min.y &&
                      p1.y <= max.y))) {
                DrawLine(d, p0, p1, width, color);
            } else {
                coOrd clip0 = p0, clip1 = p1;
                if (ClipLine(&clip0, &clip1, d->orig, d->angle, d->size)) {
                    DrawLine(d, clip0, clip1, width, color);
                }
            }

            p0 = p1;
        }
        return;
    }
    if (d->angle!=0.0 && angle1 < 360.0)
    {
        angle0 = NormalizeAngle(angle0-d->angle);
    }
    d->CoOrd2Pix(d,p,&x,&y);
    drawCount++;
    wDrawLineType_e lineOpt = wDrawLineSolid;
    unsigned long NotSolid = DC_NOTSOLIDLINE;
    unsigned long opt = d->options&NotSolid;
	if (opt == DC_DASH)
		lineOpt = wDrawLineDash;
	else if(opt == DC_DOT)
		lineOpt = wDrawLineDot;
	else if(opt == DC_DASHDOT)
		lineOpt = wDrawLineDashDot;
	else if (opt == DC_DASHDOTDOT)
		lineOpt = wDrawLineDashDotDot;
	else if(opt == DC_CENTER)
		lineOpt = wDrawLineCenter;
	else if (opt == DC_PHANTOM)
		lineOpt = wDrawLinePhantom;
    if (drawEnable)
    {
        int sizeCenter = (int)(drawCenter ? ((d->options & DC_PRINT) ? (d->dpi / BASE_DPI) : 1) : 0);
        wDrawArc(d->d, x, y, (wDrawPix_t)(rr), angle0, angle1, sizeCenter,
                 width, lineOpt,
                 color, DRAWOPTS(d) );
    }
}


static void DDrawString(
		drawCmd_p d,
		coOrd p,
		ANGLE_T a,
		char * s,
		wFont_p fp,
		FONTSIZE_T fontSize,
		wDrawColor color )
{
	wDrawPix_t x, y;
	if (d == &mapD && !mapVisible)
		return;
	d->CoOrd2Pix(d,p,&x,&y);
	if ( color == wDrawColorWhite ) {
		wDrawPix_t width, height, descent, ascent;
		coOrd pos[4], size;
		double scale = 1.0;
		wDrawGetTextSize( &width, &height, &descent, &ascent, d->d, s, fp, fontSize );
		pos[0] = p;
		size.x = SCALEX(mainD,width)*scale;
		size.y = SCALEY(mainD,height)*scale;
		pos[1].x = p.x+size.x;
		pos[1].y = p.y;
		pos[2].x = p.x+size.x;
		pos[2].y = p.y+size.y;
		pos[3].x = p.x;
		pos[3].y = p.y+size.y;
		Rotate( &pos[1], pos[0], a );
		Rotate( &pos[2], pos[0], a );
		Rotate( &pos[3], pos[0], a );
		DDrawPoly( d, 4, pos, NULL, color, 0, DRAW_FILL );
	} else {
		fontSize /= d->scale;
		wDrawString( d->d, x, y, d->angle-a, s, fp, fontSize, color, DRAWOPTS(d) );
	}
}


static void DDrawPoly(
		drawCmd_p d,
		int cnt,
		coOrd * pts,
		int * types,
		wDrawColor color,
		wDrawWidth width,
		drawFill_e eFillOpt )
{
	typedef wDrawPix_t wPos2[2];
	static dynArr_t wpts_da;
	static  dynArr_t wpts_type_da;
	int inx;
	int fill = 0;
	int open = 0;
	wDrawPix_t x, y;
	DYNARR_SET( wPos2, wpts_da, cnt * 2 );
	DYNARR_SET( int, wpts_type_da, cnt);
#define wpts(N) DYNARR_N( wPos2, wpts_da, N )
#define wtype(N) DYNARR_N( wPolyLine_e, wpts_type_da, N )
	for ( inx=0; inx<cnt; inx++ ) {
		d->CoOrd2Pix( d, pts[inx], &x, &y );
		wpts(inx)[0] = x;
		wpts(inx)[1] = y;
		if (!types)
			wtype(inx) = 0;
		else
			wtype(inx) = (wPolyLine_e)types[inx];
	}
	wDrawLineType_e lineOpt = wDrawLineSolid;
	unsigned long NotSolid = DC_NOTSOLIDLINE;
	unsigned long opt = d->options&NotSolid;
	if (opt == DC_DASH)
		lineOpt = wDrawLineDash;
	else if(opt == DC_DOT)
		lineOpt = wDrawLineDot;
	else if(opt == DC_DASHDOT)
		lineOpt = wDrawLineDashDot;
	else if (opt == DC_DASHDOTDOT)
		lineOpt = wDrawLineDashDotDot;
	else if(opt == DC_CENTER)
		lineOpt = wDrawLineCenter;
	else if (opt == DC_PHANTOM)
		lineOpt = wDrawLinePhantom;

	wDrawOpts drawOpts = DRAWOPTS(d);
	switch ( eFillOpt ) {
	case DRAW_OPEN:
		open = 1;
		break;
	case DRAW_CLOSED:
		break;
	case DRAW_FILL:
		fill = 1;
		break;
	case DRAW_TRANSPARENT:
		fill = 1;
		drawOpts |= wDrawOptTransparent;
		break;
	default:
		abort();
	}
	wDrawPolygon( d->d, &wpts(0), &wtype(0), cnt, color, width, lineOpt, drawOpts, fill, open );
}


static void DDrawFillCircle(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		wDrawColor color )
{
	wDrawPix_t x, y;
	DIST_T rr;

	if (d == &mapD && !mapVisible)
		return;
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) {
		// Circle too big
		return;
	}
	d->CoOrd2Pix(d,p,&x,&y);
	wWinPix_t w, h;
	wDrawGetSize( d->d, &w, &h );
	if ( d->options & DC_TICKS ) {
		if ( x+rr < LBORDER || x-rr > w-RBORDER ||
		     y+rr < BBORDER || y-rr > h-TBORDER )
			return;
	} else {
		if ( x+rr < 0 || x-rr > w ||
		     y+rr < 0 || y-rr > h )
			return;
	}
	drawCount++;
	if (drawEnable) {
		wDrawFilledCircle( d->d, x, y, (wDrawPix_t)(rr),
				color, DRAWOPTS(d) );
	}
}


static void DDrawRectangle(
		drawCmd_p d,
		coOrd orig,
		coOrd size,
		wDrawColor color,
		drawFill_e eFillOpt )
{
	wDrawPix_t x, y, w, h;

	if (d == &mapD && !mapVisible)
		return; 
	d->CoOrd2Pix(d,orig,&x,&y);
	w = (wDrawPix_t)((size.x/d->scale)*d->dpi+0.5);
	h = (wDrawPix_t)((size.y/d->scale)*d->dpi+0.5);
	drawCount++;
	if (drawEnable) {
		wDrawOpts opts = DRAWOPTS(d);
		coOrd p1, p2;
		switch (eFillOpt) {
		case DRAW_CLOSED:
			// 1 2
			// 0 3
			p1.x = orig.x;
			p1.y = orig.y+size.y;
			DrawLine( d, orig, p1, 0, color );
			p2.x = orig.x+size.x;
			p2.y = p1.y;
			DrawLine( d, p1, p2, 0, color );
			p1.x = p2.x;
			p1.y = orig.y;
			DrawLine( d, p2, p1, 0, color );
			DrawLine( d, p1, orig, 0, color );
			break;
		case DRAW_TRANSPARENT:
			opts |= wDrawOptTransparent;
			// Fallthru
		case DRAW_FILL:
			wDrawFilledRectangle( d->d, x, y, w, h, color, opts );
			break;
		default:
			abort();
		}
	}
}


EXPORT void DrawHilight( drawCmd_p d, coOrd p, coOrd s, BOOL_T add )
{
	unsigned long options = d->options;
	d->options |= DC_TEMP;
	wBool_t bTemp = wDrawSetTempMode( d->d, TRUE );
	DrawRectangle( d, p, s, add?drawColorPowderedBlue:selectedColor, DRAW_TRANSPARENT );
	wDrawSetTempMode( d->d, bTemp );
	d->options = options;
}

EXPORT void DrawHilightPolygon( drawCmd_p d, coOrd *p, int cnt )
{
	ASSERT( cnt <= 4 );
	static wDrawColor color = 0;
	if ( color == 0 )
		color = wDrawColorGray( 70 );
	DrawPoly( d, cnt, p, NULL, color, 0, DRAW_TRANSPARENT );
}


EXPORT void DrawMultiString(
		drawCmd_p d,
		coOrd pos,
		char * text,
		wFont_p fp,
		wFontSize_t fs,
		wDrawColor color,
		ANGLE_T a,
		coOrd * lo,
		coOrd * hi,
		BOOL_T boxed)
{
	char * cp;
	char * cp1;
	POS_T lineH, lineW;
	coOrd size, textsize, posl, orig;
	POS_T descent, ascent;
	char *line;

	if (!text || !*text) {
		return;  //No string or blank
	}
	line = malloc(strlen(text) + 1);

	DrawTextSize2( &mainD, "Aqjlp", fp, fs, TRUE, &textsize, &descent, &ascent);
	//POS_T ascent = textsize.y-descent;
	lineH = (ascent+descent)*1.0;
	size.x = 0.0;
	size.y = 0.0;
	orig.x = pos.x;
	orig.y = pos.y;
	cp = line;				// Build up message to hold all of the strings separated by nulls
	while (*text) {
		cp1 = cp;
		while (*text != '\0' && *text != '\n')
			*cp++ = *text++;
		*cp = '\0';
		DrawTextSize2( &mainD, cp1, fp, fs, TRUE, &textsize, &descent, &ascent);
		lineW = textsize.x;
		if (lineW>size.x)
			size.x = lineW;
		posl.x = pos.x;
		posl.y = pos.y;
		Rotate( &posl, orig, a);
		DrawString( d, posl, a, cp1, fp, fs, color );
		pos.y -= lineH;
		size.y += lineH;
		if (*text == '\0')
			break;
		text++;
		cp++;
	}
	if (lo) {
		lo->x = posl.x;
		lo->y = posl.y-descent;
	}
	if (hi) {
		hi->x = posl.x+size.x;
		hi->y = orig.y+ascent;
	}
	if (boxed && (d != &mapD)) {
		int bw=2, bh=2, br=1, bb=1;
		size.x += bw*d->scale/d->dpi;
		size.y = fabs(orig.y-posl.y)+bh*d->scale/d->dpi;
		size.y += descent+ascent;
		coOrd p[4];
		p[0] = orig; p[0].x -= (bw-br)*d->scale/d->dpi; p[0].y += (bh-bb)*d->scale/d->dpi+ascent;
		p[1] = p[0]; p[1].x += size.x;
		p[2] = p[1]; p[2].y -= size.y;
		p[3] = p[2]; p[3].x = p[0].x;
		for (int i=0;i<4;i++) {
			Rotate( &p[i], orig, a);
		}
		DrawPoly( d, 4, p, NULL, color, 0, DRAW_CLOSED );
	}

	free(line);
}


EXPORT void DrawBoxedString(
		int style,
		drawCmd_p d,
		coOrd pos,
		char * text,
		wFont_p fp, wFontSize_t fs,
		wDrawColor color,
		ANGLE_T a )
{
	coOrd size, p[4], p0=pos, p1, p2;
	static int bw=2, bh=2, br=1, bb=1;
	static double arrowScale = 0.5;
	unsigned long options = d->options;
	POS_T descent, ascent;
	/*DrawMultiString( d, pos, text, fp, fs, color, a, &lo, &hi );*/
	if ( fs < 2*d->scale )
		return;
#ifndef WINDOWS
	if ( ( d->options & DC_PRINT) != 0 ) {
		double scale = ((FLOAT_T)fs)/((FLOAT_T)drawMaxTextFontSize)/mainD.dpi;
		wDrawPix_t w, h, d, a;
		wDrawGetTextSize( &w, &h, &d, &a, mainD.d, text, fp, drawMaxTextFontSize );
		size.x = w*scale;
		size.y = h*scale;
		descent = d*scale;
		ascent = a*scale;
	} else
#endif
		DrawTextSize2( &mainD, text, fp, fs, TRUE, &size, &descent, &ascent );
#ifdef WINDOWS
	/*h -= 15;*/
#endif
	p0.x -= size.x/2.0;
	p0.y -= size.y/2.0;
	if (style == BOX_NONE || d == &mapD) {
		DrawString( d, p0, 0.0, text, fp, fs, color );
		return;
	}
	size.x += bw*d->scale/d->dpi;
	size.y += bh*d->scale/d->dpi;
	p[0] = p0;
	p[0].x -= br*d->scale/d->dpi;          	//Top of box
	p[0].y += (bb*d->scale/d->dpi+ascent);
	p[1].y = p[0].y;
	p[2].y = p[3].y = p[0].y-size.y-descent;  //Bottom of box
	p[1].x = p[2].x = p[0].x + size.x;
	p[3].x = p[0].x;
	d->options &= ~DC_DASH;
	switch (style) {
	case BOX_ARROW:
	case BOX_ARROW_BACKGROUND:
		// Reset size to actual size of the box
		size.x = p[2].x-p[0].x;
		size.y = p[0].y-p[2].y;
		// Pick a point (p1) outside of Box in arrow direction
		Translate( &p1, pos, a, size.x+size.y );
		// Find point on edge of Box (p1)
		ClipLine( &pos, &p1, p[3], 0.0, size );
		// Draw line from edge (p1) to Arrow head (p2)
		Translate( &p2, p1, a, size.y*arrowScale );
		DrawLine( d, p1, p2, 0, color );
		// Draw Arrow edges
		Translate( &p1, p2, a+150, size.y*0.7*arrowScale );
		DrawLine( d, p1, p2, 0, color );
		Translate( &p1, p2, a-150, size.y*0.7*arrowScale );
		DrawLine( d, p1, p2, 0, color );
		/* no break */
	case BOX_BOX:
	case BOX_BOX_BACKGROUND:
		if (style == BOX_ARROW_BACKGROUND || style == BOX_BOX_BACKGROUND)
			DrawPoly( d, 4, p, NULL, wDrawColorWhite, 0, DRAW_FILL );  //Clear background for box and box-arrow
		DrawLine( d, p[1], p[2], 0, color );
		DrawLine( d, p[2], p[3], 0, color );
		DrawLine( d, p[3], p[0], 0, color );
		/* no break */
	case BOX_UNDERLINE:
		DrawLine( d, p[0], p[1], 0, color );
		DrawString( d, p0, 0.0, text, fp, fs, color );
		break;
	case BOX_INVERT:
		DrawPoly( d, 4, p, NULL, color, 0, DRAW_FILL );
		if ( color != wDrawColorWhite )
			DrawString( d, p0, 0.0, text, fp, fs, wDrawColorGray( 94 ) );
		break;
	case BOX_BACKGROUND:
		DrawPoly( d, 4, p, NULL, wDrawColorWhite, 0, DRAW_FILL );
		DrawString( d, p0, 0.0, text, fp, fs, color );
		break;
	}
	d->options = options;
}


EXPORT void DrawTextSize2(
		drawCmd_p dp,
		char * text,
		wFont_p fp,
		wFontSize_t fs,
		BOOL_T relative,
		coOrd * size,
		POS_T * descent,
		POS_T * ascent)
{
	wDrawPix_t w, h, d, a;
	FLOAT_T scale = 1.0;
	if ( relative )
		fs /= dp->scale;
	if ( fs > drawMaxTextFontSize ) {
		scale = ((FLOAT_T)fs)/((FLOAT_T)drawMaxTextFontSize);
		fs = drawMaxTextFontSize;
	}
	wDrawGetTextSize( &w, &h, &d, &a, dp->d, text, fp, fs );
	size->x = SCALEX(mainD,w)*scale;
	size->y = SCALEY(mainD,h)*scale;
	*descent = SCALEY(mainD,d)*scale;
	*ascent = SCALEY(mainD,a)*scale;
	if ( relative ) {
		size->x *= dp->scale;
		size->y *= dp->scale;
		*descent *= dp->scale;
		*ascent *=dp->scale;
	}
/*	printf( "DTS2(\"%s\",%0.3f,%d) = (w%d,h%d,d%d) *%0.3f x%0.3f y%0.3f %0.3f\n", text, fs, relative, w, h, d, scale, size->x, size->y, *descent );*/
}

EXPORT void DrawTextSize(
		drawCmd_p dp,
		char * text,
		wFont_p fp,
		wFontSize_t fs,
		BOOL_T relative,
		coOrd * size )
{
	POS_T descent, ascent;
	DrawTextSize2( dp, text, fp, fs, relative, size, &descent, &ascent );
}

EXPORT void DrawMultiLineTextSize(
		drawCmd_p dp,
		char * text,
		wFont_p fp,
		wFontSize_t fs,
		BOOL_T relative,
		coOrd * size,
		coOrd * lastline )
{
	POS_T descent, ascent, lineW, lineH;
	coOrd textsize, blocksize;

	char *cp;
	char *line = malloc(strlen(text) + 1);

	DrawTextSize2( &mainD, "Aqlip", fp, fs, TRUE, &textsize, &descent, &ascent);
	lineH = (ascent+descent)*1.0;
	blocksize.x = 0;
	blocksize.y = 0;
	lastline->x = 0;
	lastline->y = 0;
	while (text && *text != '\0' ) {
		cp = line;
		while (*text != '\0' && *text != '\n')
			*cp++ = *text++;
		*cp = '\0';
		blocksize.y += lineH;
		DrawTextSize2( &mainD, line, fp, fs, TRUE, &textsize, &descent, &ascent);
		lineW = textsize.x;
		if (lineW>blocksize.x)
			blocksize.x = lineW;
		lastline->x = textsize.x;
		if (*text =='\n') {
			blocksize.y += lineH;
			lastline->y -= lineH;
			lastline->x = 0;
		}
		if (*text == '\0') {
			blocksize.y += textsize.y;
			break;
		}
		text++;
	}
	size->x = blocksize.x;
	size->y = blocksize.y;
	free(line);
}


static void DDrawBitMap( drawCmd_p d, coOrd p, wDrawBitMap_p bm, wDrawColor color)
{
	wDrawPix_t x, y;
	d->CoOrd2Pix( d, p, &x, &y );
	wDrawBitMap( d->d, bm, x, y, color, DRAWOPTS(d) );
}


static void TempSegLine(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		wDrawWidth width,
		wDrawColor color )
{
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
	tempSegs(tempSegs_da.cnt-1).type = SEG_STRLIN;
	tempSegs(tempSegs_da.cnt-1).color = color;
	if (d->options&DC_SIMPLE)
		tempSegs(tempSegs_da.cnt-1).width = 0;
	else if (width<0)
		tempSegs(tempSegs_da.cnt-1).width = width;
	else
		tempSegs(tempSegs_da.cnt-1).width = width*d->scale/d->dpi;
	tempSegs(tempSegs_da.cnt-1).u.l.pos[0] = p0;
	tempSegs(tempSegs_da.cnt-1).u.l.pos[1] = p1;
}


static void TempSegArc(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		ANGLE_T angle0,
		ANGLE_T angle1,
		BOOL_T drawCenter,
		wDrawWidth width,
		wDrawColor color )
{
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
	tempSegs(tempSegs_da.cnt-1).type = SEG_CRVLIN;
	tempSegs(tempSegs_da.cnt-1).color = color;
	if (d->options&DC_SIMPLE)
		tempSegs(tempSegs_da.cnt-1).width = 0;
	else if (width<0)
		tempSegs(tempSegs_da.cnt-1).width = width;
	else
		tempSegs(tempSegs_da.cnt-1).width = width*d->scale/d->dpi;
	tempSegs(tempSegs_da.cnt-1).u.c.center = p;
	tempSegs(tempSegs_da.cnt-1).u.c.radius = r;
	tempSegs(tempSegs_da.cnt-1).u.c.a0 = angle0;
	tempSegs(tempSegs_da.cnt-1).u.c.a1 = angle1;
}


static void TempSegString(
		drawCmd_p d,
		coOrd p,
		ANGLE_T a,
		char * s,
		wFont_p fp,
		FONTSIZE_T fontSize,
		wDrawColor color )
{
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
	tempSegs(tempSegs_da.cnt-1).type = SEG_TEXT;
	tempSegs(tempSegs_da.cnt-1).color = color;
	tempSegs(tempSegs_da.cnt-1).u.t.boxed = FALSE;
	tempSegs(tempSegs_da.cnt-1).width = 0;
	tempSegs(tempSegs_da.cnt-1).u.t.pos = p;
	tempSegs(tempSegs_da.cnt-1).u.t.angle = a;
	tempSegs(tempSegs_da.cnt-1).u.t.fontP = fp;
	tempSegs(tempSegs_da.cnt-1).u.t.fontSize = fontSize;
	tempSegs(tempSegs_da.cnt-1).u.t.string = MyStrdup(s);
}


static void TempSegPoly(
		drawCmd_p d,
		int cnt,
		coOrd * pts,
		int * types,
		wDrawColor color,
		wDrawWidth width,
		drawFill_e eFillOpt )
{
	int fill = 0;
	int open = 0;
	switch (eFillOpt) {
	case DRAW_OPEN:
		open = 1;
		break;
	case DRAW_CLOSED:
		break;
	case DRAW_FILL:
		fill = 1;
		break;
	case DRAW_TRANSPARENT:
		fill = 1;
		break;
	default:
		abort();
	}
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 1);
	tempSegs(tempSegs_da.cnt-1).type = fill?SEG_FILPOLY:SEG_POLY;
	tempSegs(tempSegs_da.cnt-1).color = color;
	if (d->options&DC_SIMPLE)
		tempSegs(tempSegs_da.cnt-1).width = 0;
	else if (width<0)
		tempSegs(tempSegs_da.cnt-1).width = width;
	else
		tempSegs(tempSegs_da.cnt-1).width = width*d->scale/d->dpi;
	tempSegs(tempSegs_da.cnt-1).u.p.polyType = open?POLYLINE:FREEFORM;
	tempSegs(tempSegs_da.cnt-1).u.p.cnt = cnt;
	tempSegs(tempSegs_da.cnt-1).u.p.orig = zero;
	tempSegs(tempSegs_da.cnt-1).u.p.angle = 0.0;
	tempSegs(tempSegs_da.cnt-1).u.p.pts = (pts_t *)MyMalloc(cnt*sizeof(pts_t));
	for (int i=0;i<=cnt-1;i++) {
		tempSegs(tempSegs_da.cnt-1).u.p.pts[i].pt = pts[i];
		tempSegs(tempSegs_da.cnt-1).u.p.pts[i].pt_type = ((d->options&DC_SIMPLE)==0 && (types!=0))?types[i]:wPolyLineStraight;
	}
}


static void TempSegFillCircle(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		wDrawColor color )
{
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
	tempSegs(tempSegs_da.cnt-1).type = SEG_FILCRCL;
	tempSegs(tempSegs_da.cnt-1).color = color;
	tempSegs(tempSegs_da.cnt-1).width = 0;
	tempSegs(tempSegs_da.cnt-1).u.c.center = p;
	tempSegs(tempSegs_da.cnt-1).u.c.radius = r;
	tempSegs(tempSegs_da.cnt-1).u.c.a0 = 0.0;
	tempSegs(tempSegs_da.cnt-1).u.c.a1 = 360.0;
}


static void TempSegRectangle(
		drawCmd_p d,
		coOrd orig,
		coOrd size,
		wDrawColor color,
		drawFill_e eOpts )
{
	coOrd p[4];
	// p1 p2
	// p0 p3
	p[0].x = p[1].x = orig.x;
	p[2].x = p[3].x = orig.x+size.x;
	p[0].y = p[3].y = orig.y;
	p[1].y = p[2].y = orig.y+size.y;
	TempSegPoly( d, 4, p, NULL, color, 0, eOpts );
}


static void NoDrawBitMap( drawCmd_p d, coOrd p, wDrawBitMap_p bm, wDrawColor color )
{
}



EXPORT drawFuncs_t screenDrawFuncs = {
		DDrawLine,
		DDrawArc,
		DDrawString,
		DDrawBitMap,
		DDrawPoly,
		DDrawFillCircle,
		DDrawRectangle};

EXPORT drawFuncs_t printDrawFuncs = {
		DDrawLine,
		DDrawArc,
		DDrawString,
		NoDrawBitMap,
		DDrawPoly,
		DDrawFillCircle,
		DDrawRectangle};

EXPORT drawFuncs_t tempSegDrawFuncs = {
		TempSegLine,
		TempSegArc,
		TempSegString,
		NoDrawBitMap,
		TempSegPoly,
		TempSegFillCircle,
		TempSegRectangle};


EXPORT drawCmd_t mainD = {
		NULL, &screenDrawFuncs, DC_TICKS, INIT_MAIN_SCALE, 0.0, {0.0,0.0}, {0.0,0.0}, MainPix2CoOrd, MainCoOrd2Pix, 96.0};

EXPORT drawCmd_t tempD = {
		NULL, &screenDrawFuncs, DC_TICKS|DC_TEMP, INIT_MAIN_SCALE, 0.0, {0.0,0.0}, {0.0,0.0}, MainPix2CoOrd, MainCoOrd2Pix, 96.0};

EXPORT drawCmd_t mapD = {
		NULL, &screenDrawFuncs, DC_SIMPLE, INIT_MAP_SCALE, 0.0, {0.0,0.0}, {96.0,48.0}, Pix2CoOrd, CoOrd2Pix, 96.0};


/*****************************************************************************
 *
 * MAIN AND MAP WINDOW DEFINTIONS
 *
 */


static wWinPix_t info_yb_offset = 2;
static wWinPix_t six = 2;
static wWinPix_t info_xm_offset = 2;
static wWinPix_t messageOrControlX = 0;
static wWinPix_t messageOrControlY = 0;
#define NUM_INFOCTL				(4)
static wControl_p curInfoControl[NUM_INFOCTL];
static wWinPix_t curInfoLabelWidth[NUM_INFOCTL];

/**
 * Determine the width of a mouse pointer position string ( coordinate plus label ).
 *
 * \return width of position string
 */
static wWinPix_t GetInfoPosWidth( void )
{
	wWinPix_t labelWidth;
	
	DIST_T dist;
		if ( mapD.size.x > mapD.size.y )
			dist = mapD.size.x;
		else
			dist = mapD.size.y;
		if ( units == UNITS_METRIC ) {
			dist *= 2.54;
			if ( dist >= 1000 )
				dist = 9999.999*2.54;
			else if ( dist >= 100 )
				dist = 999.999*2.54;
			else if ( dist >= 10 )
				dist = 99.999*2.54;
		} else {
			if ( dist >= 100*12 )
				dist = 999.0*12.0+11.0+3.0/4.0-1.0/64.0;
			else if ( dist >= 10*12 )
				dist = 99.0*12.0+11.0+3.0/4.0-1.0/64.0;
			else if ( dist >= 1*12 )
				dist = 9.0*12.0+11.0+3.0/4.0-1.0/64.0;
		}
		
		labelWidth = (wStatusGetWidth( xLabel ) > wStatusGetWidth( yLabel ) ? wStatusGetWidth( xLabel ):wStatusGetWidth( yLabel ));
			
		return wStatusGetWidth( FormatDistance(dist) ) + labelWidth;
}

/**
 * Initialize the status line at the bottom of the window.
 * 
 */

EXPORT void InitInfoBar( void )
{
	wWinPix_t width, height, y, yb, ym, x, boxH;
	wWinGetSize( mainW, &width, &height );
	infoHeight = 2 + wStatusGetHeight( COMBOBOX ) + 2 ;
	textHeight = wStatusGetHeight(0L);
	y = height - max(infoHeight,textHeight)-10;

#ifdef WINDOWS
	y -= 19; /* Kludge for MSW */
#endif

	infoD.pos_w = GetInfoPosWidth();
	infoD.scale_w = wStatusGetWidth( "999:1" ) + wStatusGetWidth( zoomLabel );
	/* we do not use the count label for the moment */
	infoD.count_w = 0;
	infoD.info_w = width - 20 - infoD.pos_w*2 - infoD.scale_w - infoD.count_w - 45;      // Allow Window to resize down
	if (infoD.info_w <= 0) {
		infoD.info_w = 10;
	}
	yb = y+info_yb_offset;
	ym = y+(infoHeight-textHeight)/2;
	boxH = infoHeight;
		x = 2;
		infoD.scale_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.scale_w, boxH );
		infoD.scale_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarScale", infoD.scale_w-six, zoomLabel);
		x += infoD.scale_w + 2;
		infoD.posX_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.pos_w, boxH );
		infoD.posX_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarPosX", infoD.pos_w-six, xLabel );
		x += infoD.pos_w + 2;
		infoD.posY_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.pos_w, boxH );
		infoD.posY_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarPosY", infoD.pos_w-six, yLabel );
		x += infoD.pos_w + 2;
		messageOrControlX = x+info_xm_offset;									//Remember Position
		messageOrControlY = ym;
		infoD.info_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.info_w, boxH );
		infoD.info_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarStatus", infoD.info_w-six, "" );
}



static void SetInfoBar( void )
{
	wWinPix_t width, height, y, yb, ym, x, boxH;
	int inx;
	static long oldDistanceFormat = -1;
	long newDistanceFormat;
	wWinGetSize( mainW, &width, &height );
	y = height - max(infoHeight,textHeight)-10;
	newDistanceFormat = GetDistanceFormat();
	if ( newDistanceFormat != oldDistanceFormat ) {
		infoD.pos_w = GetInfoPosWidth();
		wBoxSetSize( infoD.posX_b, infoD.pos_w, infoHeight-3 );
		wStatusSetWidth( infoD.posX_m, infoD.pos_w-six );
		wBoxSetSize( infoD.posY_b, infoD.pos_w, infoHeight-3 );
		wStatusSetWidth( infoD.posY_m, infoD.pos_w-six );
	}
	infoD.info_w = width - 20 - infoD.pos_w*2 - infoD.scale_w - infoD.count_w - 40 + 4;
	if (infoD.info_w <= 0) {
		infoD.info_w = 10;
	}
	yb = y+info_yb_offset;
	ym = y+(infoHeight-textHeight)/2;
	boxH = infoHeight;
		wWinClear( mainW, 0, y, width-20, infoHeight );
		x = 0;
		wControlSetPos( (wControl_p)infoD.scale_b, x, yb );
		wControlSetPos( (wControl_p)infoD.scale_m, x+info_xm_offset, ym );
		x += infoD.scale_w + 10;
		wControlSetPos( (wControl_p)infoD.posX_b, x, yb );
		wControlSetPos( (wControl_p)infoD.posX_m, x+info_xm_offset, ym );
		x += infoD.pos_w + 5;
		wControlSetPos( (wControl_p)infoD.posY_b, x, yb );
		wControlSetPos( (wControl_p)infoD.posY_m, x+info_xm_offset, ym );
		x += infoD.pos_w + 10;
		wControlSetPos( (wControl_p)infoD.info_b, x, yb );
		wControlSetPos( (wControl_p)infoD.info_m, x+info_xm_offset, ym );
		wBoxSetSize( infoD.info_b, infoD.info_w, boxH );
		wStatusSetWidth( infoD.info_m, infoD.info_w-six );
		messageOrControlX = x+info_xm_offset;
		messageOrControlY = ym;
		if (curInfoControl[0]) {
			for ( inx=0; curInfoControl[inx]; inx++ ) {
				x += curInfoLabelWidth[inx];
				wWinPix_t y_this = ym + (textHeight/2) - (wControlGetHeight( curInfoControl[inx] )/2);
				wControlSetPos( curInfoControl[inx], x, y_this );
				x += wControlGetWidth( curInfoControl[inx] )+3;
				wControlShow( curInfoControl[inx], TRUE );
			}
			wControlSetPos( (wControl_p)infoD.info_m, x+info_xm_offset, ym );  //Move to end
		}
}


static void InfoScale( void )
{
	if (mainD.scale >= 1)
		sprintf( message, "%s%0.0f:1", zoomLabel, mainD.scale );
	else
		sprintf( message, "%s1:%0.0f", zoomLabel, floor(1/mainD.scale+0.5) );
	wStatusSetValue( infoD.scale_m, message );
}

EXPORT void InfoCount( wIndex_t count )
{
/*
	sprintf( message, "%d", count );
	wMessageSetValue( infoD.count_m, message );
*/	
}

EXPORT void InfoPos( coOrd pos )
{
	sprintf( message, "%s%s", xLabel, FormatDistance(pos.x) );
	wStatusSetValue( infoD.posX_m, message );
	sprintf( message, "%s%s", yLabel, FormatDistance(pos.y) );
	wStatusSetValue( infoD.posY_m, message );
	
	oldMarker = pos;
}

static wControl_p deferSubstituteControls[NUM_INFOCTL+1];
static char * deferSubstituteLabels[NUM_INFOCTL];

EXPORT void InfoSubstituteControls(
		wControl_p * controls,
		char ** labels )
{
	wWinPix_t x, y;
	int inx;
	for ( inx=0; inx<NUM_INFOCTL; inx++ ) {
		if (curInfoControl[inx]) {
			wControlShow( curInfoControl[inx], FALSE );
			curInfoControl[inx] = NULL;
		}
		curInfoLabelWidth[inx] = 0;
		curInfoControl[inx] = NULL;
	}
	if ( inError && ( controls!=NULL && controls[0]!=NULL) ) {
		memcpy( deferSubstituteControls, controls, sizeof deferSubstituteControls );
		memcpy( deferSubstituteLabels, labels, sizeof deferSubstituteLabels );
	}
	if ( inError || controls == NULL || controls[0]==NULL ) {
		wControlSetPos( (wControl_p)infoD.info_m, messageOrControlX, messageOrControlY);
		wControlShow( (wControl_p)infoD.info_m, TRUE );
		return;
	}
	//x = wControlGetPosX( (wControl_p)infoD.info_m );
	x = messageOrControlX;
	y = messageOrControlY;
	wStatusSetValue( infoD.info_m, "" );
	wControlShow( (wControl_p)infoD.info_m, FALSE );
	for ( inx=0; controls[inx]; inx++ ) {
		curInfoLabelWidth[inx] = wLabelWidth(_(labels[inx]));
		x += curInfoLabelWidth[inx];
#ifdef WINDOWS
		wWinPix_t	y_this = y + (infoHeight/2) - (textHeight / 2 );
#else
		wWinPix_t	y_this = y + (infoHeight / 2) - (wControlGetHeight(controls[inx]) / 2) - 2;
#endif
		wControlSetPos( controls[inx], x, y_this );
		x += wControlGetWidth( controls[inx] );
		wControlSetLabel( controls[inx], _(labels[inx]) );
		wControlShow( controls[inx], TRUE );
		curInfoControl[inx] = controls[inx];
		x += 3;
	}
	wControlSetPos( (wControl_p)infoD.info_m, x, y );
	curInfoControl[inx] = NULL;
	deferSubstituteControls[0] = NULL;
}

EXPORT void SetMessage( char * msg )
{
	wStatusSetValue( infoD.info_m, msg );
}


static void ChangeMapScale( BOOL_T reset )
{
	wWinPix_t w, h;
	wWinPix_t dw, dh;
	FLOAT_T fw, fh;


    fw = (((mapD.size.x/mapD.scale)*mapD.dpi) + 0.5)+2;
    fh = (((mapD.size.y/mapD.scale)*mapD.dpi) + 0.5)+2;

	w = (wWinPix_t)fw;
	h = (wWinPix_t)fh;
	if (reset) {
		wGetDisplaySize( &dw, &dh );
		wSetGeometry(mapW, 50, dw, 50, dh, -1, -1, mapD.size.x/mapD.size.y);
		wWinSetSize( mapW, w+DlgSepLeft+DlgSepRight, h+DlgSepTop+DlgSepBottom);
	}
	wDrawSetSize( mapD.d, w, h, NULL );
}


EXPORT BOOL_T SetRoomSize( coOrd size )
{
	SetLayoutRoomSize(size);
	if (size.x < 12.0)
		size.x = 12.0;
	if (size.y < 12.0)
		size.y = 12.0;
	if ( mapD.size.x == size.x &&
		 mapD.size.y == size.y )
		return TRUE;
	mapD.size = size;
	SetLayoutRoomSize(size);
	if ( mapW == NULL)
		return TRUE;
	ChangeMapScale(TRUE);
	wPrefSetFloat( "draw", "roomsizeX", mapD.size.x );
	wPrefSetFloat( "draw", "roomsizeY", mapD.size.y );
	return TRUE;
}


EXPORT void GetRoomSize( coOrd * froomSize )
{
	*froomSize = mapD.size;
}


static void MapRedraw(
		wDraw_p bd, void * pContex, wWinPix_t px, wWinPix_t py )
{
	if (inPlaybackQuit)
		return;
	static int cMR = 0;
	LOG( log_redraw, 2, ( "MapRedraw: %d\n", cMR++ ) );
	if (!mapVisible)
		return;
	if (delayUpdate)
	wDrawDelayUpdate( mapD.d, TRUE );
	//wSetCursor( mapD.d, wCursorWait );
	wBool_t bTemp = wDrawSetTempMode( mapD.d, FALSE );
	if ( bTemp )
		printf( "MapRedraw TempMode\n" );
	wDrawClear( mapD.d );
	DrawTracks( &mapD, mapD.scale, mapD.orig, mapD.size );
	DrawMapBoundingBox( TRUE );
	//wSetCursor( mapD.d, defaultCursor );
	wDrawSetTempMode( mapD.d, bTemp );
	wDrawDelayUpdate( mapD.d, FALSE );
}


static void MapResize( void )
{
	mapD.scale = mapScale;
	ChangeMapScale(TRUE);
	MapRedraw( mapD.d, NULL, 0, 0 );
}


EXPORT void SetMainSize( void )
{
	wWinPix_t ww, hh;
	DIST_T w, h;
	wDrawGetSize( mainD.d, &ww, &hh );
	ww -= LBORDER+RBORDER;
	hh -= BBORDER+TBORDER;
	w = ww/mainD.dpi;
	h = hh/mainD.dpi;
	mainD.size.x = w * mainD.scale;
	mainD.size.y = h * mainD.scale;
	tempD.size = mainD.size;
}

// Hack to switch between TempRedraw and MainRedraw
extern wBool_t wDrawDoTempDraw;

/* Update temp_surface after executing a command
 */
EXPORT void TempRedraw( void ) {

	static int cTR = 0;
	LOG( log_redraw, 2, ( "TempRedraw: %d\n", cTR++ ) );
if (wDrawDoTempDraw == FALSE) {
	// Remove this after windows supports GTK
	MainRedraw(); // TempRedraw - windows
} else {
	wDrawDelayUpdate( tempD.d, TRUE );
	wDrawSetTempMode( tempD.d, TRUE );
	DrawMarkers();
	DoCurCommand( C_REDRAW, zero );
	RulerRedraw( FALSE );
	RedrawPlaybackCursor();              //If in playback
	wDrawSetTempMode( tempD.d, FALSE );
	wDrawDelayUpdate( tempD.d, FALSE );
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
 * \returns true on success, false otherwise
 */

void
TranslateBackground(drawCmd_p drawP, POS_T origX, POS_T origY, wWinPix_t* posX,
                    wWinPix_t* posY, wWinPix_t* pWidth)
{
    coOrd back_pos = GetLayoutBackGroundPos();

    *pWidth = (wWinPix_t)(GetLayoutBackGroundSize() / drawP->scale *
                          drawP->dpi);

    *posX = (wWinPix_t)((back_pos.x - origX) / drawP->scale *
                        drawP->dpi);
    *posY = (wWinPix_t)((back_pos.y - origY) / drawP->scale *
                        drawP->dpi);
}

/*
* Redraw contents on main window
*/
EXPORT void MainRedraw( void )
{
	coOrd orig, size;

	static int cMR = 0;
	LOG( log_redraw, 1, ( "MainRedraw: %d\n", cMR++ ) );
	unsigned long time0 = wGetTimer();
	if (delayUpdate)
	wDrawDelayUpdate( mainD.d, TRUE );

	wDrawSetTempMode( mainD.d, FALSE );
	wDrawClear( mainD.d );

	orig = mainD.orig;
	size = mainD.size;
	orig.x -= LBORDER/mainD.dpi*mainD.scale;
	orig.y -= BBORDER/mainD.dpi*mainD.scale;

	DrawRoomWalls( TRUE );
	if (GetLayoutBackGroundScreen() < 100.0 && GetLayoutBackGroundVisible()) {
		wWinPix_t bitmapPosX;
		wWinPix_t bitmapPosY;
		wWinPix_t bitmapWidth;

		TranslateBackground(&mainD, orig.x, orig.y, &bitmapPosX, &bitmapPosY,
		                    &bitmapWidth);

		wDrawShowBackground(mainD.d,
		                    bitmapPosX,
		                    bitmapPosY,
		                    bitmapWidth,
		                    GetLayoutBackGroundAngle(),
		                    GetLayoutBackGroundScreen());
	}
	DrawSnapGrid( &mainD, mapD.size, TRUE );

	orig = mainD.orig;
	size = mainD.size;
	orig.x -= RBORDER/mainD.dpi*mainD.scale;
	orig.y -= BBORDER/mainD.dpi*mainD.scale;
	size.x += (RBORDER+LBORDER)/mainD.dpi*mainD.scale;
	size.y += (BBORDER+TBORDER)/mainD.dpi*mainD.scale;
	DrawTracks( &mainD, mainD.scale, orig, size );

	DrawRoomWalls( FALSE );  //No background, just rulers

	currRedraw++;

	//wSetCursor( mainD.d, defaultCursor );
	InfoScale();
	LOG( log_timemainredraw, 1, ( "MainRedraw time = %lu mS\n", wGetTimer()-time0 ) );
	// The remainder is from TempRedraw
	wDrawSetTempMode( tempD.d, TRUE );
	DrawMarkers();
	DoCurCommand( C_REDRAW, zero );
	RulerRedraw( FALSE );
	RedrawPlaybackCursor();              //If in playback
	wDrawSetTempMode( tempD.d, FALSE );
	wDrawDelayUpdate( mainD.d, FALSE );
}

/*
* Layout main window in response to Pan/Zoom
*
* \param bRedraw Redraw mainD
* \param bNoBorder Don't allow mainD.orig to go negative
*/
EXPORT void MainLayout(
	wBool_t bRedraw,
	wBool_t bNoBorder )
{
	DIST_T t1;
	if (inPlaybackQuit)
		return;
	static int cML = 0;
	LOG( log_redraw, 1, ( "MainLayout: %d\n", cML++ ) );

	SetMainSize();
	t1 = mainD.dpi/mainD.scale;
	if (units == UNITS_ENGLISH) {
		t1 /= 2.0;
		for ( pixelBins=0.25; pixelBins<t1; pixelBins*=2.0 );
	} else {
		t1 /= 2.0;
		pixelBins = 0.127;
		while (1) {
			pixelBins *= 2.0;
			if ( pixelBins >= t1 )
				break;
			pixelBins *= 2.0;
			if ( pixelBins >= t1 )
				break;
			pixelBins *= 2.5;
			if ( pixelBins >= t1 )
				break;
		}
	}
	LOG( log_pan, 2, ( "PixelBins=%0.6f\n", pixelBins ) );
	ConstraintOrig( &mainD.orig, mainD.size, bNoBorder, TRUE );
	tempD.orig = mainD.orig;
	tempD.size = mainD.size;
	mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
	mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
	DrawMapBoundingBox( TRUE );

	if ( bRedraw )
		MainRedraw();

	if ( bRedraw && wDrawDoTempDraw ) { // Temporary until mswlib supports TempDraw
		wAction_t action = wActionMove;
		coOrd pos;
		if ( mouseState == mouseLeft )
			action = wActionLDrag;
		if ( mouseState == mouseRight )
			action = wActionRDrag;
		mainD.Pix2CoOrd( &mainD, mousePositionx, mousePositiony, &pos );
		// Prevent recursion if curCommand calls MainLayout when action if a Move or Drag
		// ie CmdPan
		static int iRecursion = 0;
		iRecursion++;
		if ( iRecursion == 1 )
			DoMouse( action, pos );
		iRecursion--;
	}
}

/**
 * The wlib event handler for the main window.
 * 
 * \param win wlib window information
 * \param e the wlib event
 * \param data additional data (unused)
 */

void MainProc( wWin_p win, winProcEvent e, void * refresh, void * data )
{
	wWinPix_t width, height;
	switch( e ) {
	case wResize_e:
		if (mainD.d == NULL)
			return;
		wWinGetSize( mainW, &width, &height );
		LayoutToolBar(refresh);
        if (wWinIsTemplated(mainW)) {
			if (height>=0) {
				wDrawSetSize( mainD.d, width, height, refresh );
				SetMainSize();
				panCenter.x = mainD.orig.x + mainD.size.x/2.0;
				panCenter.y = mainD.orig.y + mainD.size.y/2.0;
				LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
				MainLayout( !refresh, TRUE ); // MainProc: wResize_e event
				wPrefSetInteger( "draw", "m {ainwidth", width );
				wPrefSetInteger( "draw", "mainheight", height );
			} else DrawMapBoundingBox( TRUE );
        } else {
        	height -= (toolbarHeight+max(infoHeight,textHeight)+10);
        	if (height >= 0) {
				wDrawSetSize( mainD.d, width-20, height, refresh );
				wControlSetPos( (wControl_p)mainD.d, 0, toolbarHeight );
				SetMainSize();
				SetInfoBar();
				panCenter.x = mainD.orig.x + mainD.size.x/2.0;
				panCenter.y = mainD.orig.y + mainD.size.y/2.0;
				LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
				MainLayout( !refresh, TRUE ); // MainProc: wResize_e event
				wPrefSetInteger( "draw", "mainwidth", (int)width );
				wPrefSetInteger( "draw", "mainheight", (int)height );
        	} else	DrawMapBoundingBox( TRUE );
        }
		break;
	case wState_e:
		wPrefSetInteger( "draw", "maximized", wWinIsMaximized(win) );
		break;
	case wQuit_e:
		CleanupCustom();
		break;
	case wClose_e:
		/* shutdown the application via "close window"  button  */
		DoQuit(NULL);
		break;
	default:
		break;
	}
}


EXPORT void DoRedraw( void )
{
	MapRedraw( mapD.d, NULL, 0, 0 );
	MainRedraw(); // DoRedraw
}

/*****************************************************************************
 *
 * RULERS and OTHER DECORATIONS
 *
 */


static void DrawRoomWalls( wBool_t drawBackground )
{
	if (mainD.d == NULL)
		return;

	if (drawBackground) {
		DrawRectangle( &mainD, mainD.orig, mainD.size, drawColorGrey80, DRAW_FILL );
		DrawRectangle( &mainD, zero, mapD.size, wDrawColorWhite, DRAW_FILL );

	} else {
		coOrd p[4];
		DrawTicks( &mainD, mapD.size );
		p[0].x = p[0].y = p[1].x = p[3].y = 0.0;
		p[2].x = p[3].x = mapD.size.x;
		p[1].y = p[2].y = mapD.size.y;
		DrawPoly( &mainD, 4, p, NULL, borderColor, 3, DRAW_CLOSED );
	}
}


static void DrawMarkers( void )
{
	coOrd p0, p1;
	p0.x = p1.x = oldMarker.x;
	p0.y = mainD.orig.y;
	p1.y = mainD.orig.y-BBORDER*mainD.scale/mainD.dpi;
	DrawLine( &tempD, p0, p1, 0, markerColor );
	p0.y = p1.y = oldMarker.y;
	p0.x = mainD.orig.x;
	p1.x = mainD.orig.x-LBORDER*mainD.scale/mainD.dpi;
	DrawLine( &tempD, p0, p1, 0, markerColor );
}

static DIST_T rulerFontSize = 12.0;


EXPORT void DrawRuler(
		drawCmd_p d,
		coOrd pos0,
		coOrd pos1,
		DIST_T offset,
		int number,
		int tickSide,
		wDrawColor color )
{
	coOrd orig = pos0;
	wAngle_t a, aa;
	DIST_T start, end;
	long inch, lastInch;
	DIST_T len;
	int digit;
	char quote;
	char message[STR_SHORT_SIZE];
	coOrd d_orig, d_size;
	wFontSize_t fs;
	long mm, mm0, mm1, power, skip;
	
	static double lengths[] = {
		0, 2.0, 4.0, 2.0, 6.0, 2.0, 4.0, 2.0, 8.0, 2.0, 4.0, 2.0, 6.0, 2.0, 4.0, 2.0, 0.0 };
	int fraction, incr, firstFraction, lastFraction;
	int majorLength;
	coOrd p0, p1;

	a = FindAngle( pos0, pos1 );
	Translate( &pos0, pos0, a, offset );
	Translate( &pos1, pos1, a, offset );
	aa = NormalizeAngle(a+(tickSide==0?+90:-90));

	end = FindDistance( pos0, pos1 );
	if (end < 0.1)
		return;
	d_orig.x = d->orig.x - 0.1;
	d_orig.y = d->orig.y - 0.1;
	d_size.x = d->size.x + 0.2;
	d_size.y = d->size.y + 0.2;
	if (!ClipLine( &pos0, &pos1, d_orig, d->angle, d_size ))
		return;

	start = FindDistance( orig, pos0 );
	if (offset < 0)
		start = -start;
	end = FindDistance( orig, pos1 );

	DrawLine( d, pos0, pos1, 0, color );

	if (units == UNITS_METRIC) {
		mm0 = (int)ceil(start*25.4-0.5);
		mm1 = (int)floor(end*25.4+0.5);
		len = 5;
		if (d->scale <= 1) {
			power = 1;
		} else if (d->scale <= 8) {
			power = 10;
		} else if (d->scale <= 32) {
			power = 100;
		} else {
			power = 1000;
		}

		// Label interval for scale > 40
		if (d->scale <= 200) {
			skip = 2000;
		}
		else if (d->scale <= 400) {
			skip = 5000;
		}
		else {
			skip = 10000;
		}

		for ( ; power<=1000; power*=10,len+=3 ) {
			if (power == 1000)
				len = 10;
			for (mm=((mm0+(mm0>0?power-1:0))/power)*power; mm<=mm1; mm+=power) {
				if (power==1000 || mm%(power*10) != 0) {
					Translate( &p0, orig, a, mm/25.4 );
					Translate( &p1, p0, aa, len*d->scale/mainD.dpi );
					DrawLine( d, p0, p1, 0, color );
					if (!number || (d->scale > 40 && mm % skip != 0.0))
						continue;
					if ( (power>=1000) ||
						 (d->scale<=8 && power>=100) ||
						 (d->scale<=1 && power>=10) ) {
						if (mm%100 != 0) {
							sprintf(message, "%ld", mm/10%10 );
							fs = rulerFontSize*2/3;
							Translate( &p0, p0, aa, (fs/2.0+len)*d->scale/mainD.dpi );
							Translate( &p0, p0, 225, fs*d->scale/mainD.dpi ); 
							//p0.x = p1.x+4*dxn/10*d->scale/mainD.dpi;
							//p0.y = p1.y+dyn*d->scale/mainD.dpi;
						} else {
							sprintf(message, "%0.1f", mm/1000.0 );
							fs = rulerFontSize;
							Translate( &p0, p0, aa, (fs/2.0+len)*d->scale/mainD.dpi );
							Translate( &p0, p0, 225, 1.5*fs*d->scale/mainD.dpi ); 
							//p0.x = p0.x+((-(LBORDER-2)/2)+((LBORDER-2)/2+2)*sin_aa)*d->scale/mainD.dpi;
							//p0.y = p1.y+dyn*d->scale/mainD.dpi;
						}
						DrawString( d, p0, 0.0, message, rulerFp, fs*d->scale, color );
					}
				}
			}
		}
	} else {
		if (d->scale <= 1)
			incr = 1;             //16ths
		else if (d->scale <= 3)
			incr = 2;			  //8ths
		else if (d->scale <= 5)
			incr = 4;			  //4ths
		else if (d->scale <= 7)
			incr = 8;			  //1/2ths
		else if (d->scale <= 48)
			incr = 32;
		else
			incr = 16;			  //Inches

		lastInch = (int)floor(end);
		lastFraction = 16;
		inch = (int)ceil(start);
		firstFraction = (((int)((inch-start)*16/*+1*/)) / incr) * incr;
		if (firstFraction > 0) {
			inch--;
			firstFraction = 16 - firstFraction;
		}
		for ( ; inch<=lastInch; inch++){
			if(inch % 12 == 0) {
				lengths[0] = 12;
				majorLength = 16;
				digit = (int)(inch/12);
				fs = rulerFontSize;
				quote = '\'';
			} else if (d->scale <= 12) { // 8
				lengths[0] = 12;
				majorLength = 16;
				digit = (int)(inch%12);
				fs = rulerFontSize*(2.0/3.0);
				quote = '"';
			} else if (d->scale <= 24){ // 16
				lengths[0] = 10;
				majorLength = 12;
				digit = (int)(inch%12);
				fs = rulerFontSize*(1.0/2.0);
			} else {
				continue;
			}
			if (inch == lastInch)
				lastFraction = (((int)((end - lastInch)*16)) / incr) * incr;
			for ( fraction = firstFraction; fraction <= lastFraction; fraction += incr ) {
				// Tick interval for scale > 128
				skip = 0;
				if(d->scale > 512) {
					skip = (inch % 120 != 0);
				}
				else if(d->scale > 256) {
					skip = (inch % 60 != 0);
				}
				else if(d->scale > 128) {
					skip = (inch % 24 != 0);
				}
				else if(d->scale > 64) {
					skip = (inch % 12 != 0);
				}
				if(!skip){
					Translate( &p0, orig, a, inch+fraction/16.0 );
					Translate( &p1, p0, aa, lengths[fraction]*d->scale/mainD.dpi );
					DrawLine( d, p0, p1, 0, color );
				}
				if (fraction == 0) {
					// Label interval for scale > 40
					if (d->scale <= 80) {
						skip = 2;
					}
					else if (d->scale <= 120) {
						skip = 5;
					}
					else if (d->scale <= 240) {
						skip = 10;
					}
					else if (d->scale <= 480) {
						skip = 20;
					}
					else {
						skip = 50;
					}
					if ( (number == TRUE && d->scale <= 40) || (digit % skip == 0)) {
						if (inch % 12 == 0 || d->scale <= 2) {
							Translate( &p0, p0, aa, majorLength*d->scale/mainD.dpi );
							Translate( &p0, p0, 225, fs*d->scale/mainD.dpi );
							sprintf(message, "%d%c", digit, quote );
							DrawString( d, p0, 0.0, message, rulerFp, fs*d->scale, color );
						}
					}
				}
			firstFraction = 0;
			}
		}
	}
}


static void DrawTicks( drawCmd_p d, coOrd size )
{
	coOrd p0, p1;
	DIST_T offset;

	offset = 0.0;

	double blank_zone = 40*d->scale/mainD.dpi;

	if ( d->orig.x<0.0-blank_zone ) {
		 p0.y = 0.0; p1.y = mapD.size.y;
		 p0.x = p1.x = 0.0;
		 DrawRuler( d, p0, p1, offset, FALSE, TRUE, borderColor );
	}
	if (d->orig.x+d->size.x>mapD.size.x+blank_zone) {
		 p0.y = 0.0; p1.y = mapD.size.y;
		 p0.x = p1.x = mapD.size.x;
		 DrawRuler( d, p0, p1, offset, FALSE, FALSE, borderColor );
	}
	p0.x = 0.0; p1.x = d->size.x;
	offset = d->orig.x;
	p0.y = p1.y = d->orig.y;
	DrawRuler( d, p0, p1, offset, TRUE, FALSE, borderColor );
	p0.y = p1.y = d->size.y+d->orig.y;
	DrawRuler( d, p0, p1, offset, FALSE, TRUE, borderColor );

	offset = 0.0;

	if ( d->orig.y<0.0-blank_zone) {
		p0.x = 0.0; p1.x = mapD.size.x;
		p0.y = p1.y = 0.0;
		DrawRuler( d, p0, p1, offset, FALSE, FALSE, borderColor );
	}
	if (d->orig.y+d->size.y>mapD.size.y+blank_zone) {
		 p0.x = 0.0; p1.x = mapD.size.x;
		 p0.y = p1.y = mapD.size.y;
		 DrawRuler( d, p0, p1, offset, FALSE, TRUE, borderColor );
	}
	p0.y = 0.0; p1.y = d->size.y;
	offset = d->orig.y;
	p0.x = p1.x = d->orig.x;
	DrawRuler( d, p0, p1, offset, TRUE, TRUE, borderColor );
	p0.x = p1.x = d->size.x+d->orig.x;
	DrawRuler( d, p0, p1, offset, FALSE, FALSE, borderColor );
}

/*****************************************************************************
 *
 * ZOOM and PAN
 *
 */

EXPORT coOrd mainCenter;

static void DrawMapBoundingBox( BOOL_T set )
{
	if (mainD.d == NULL || mapD.d == NULL)
		abort();
	if (!mapVisible)
		return;
	DrawHilight( &mapD, mainD.orig, mainD.size, TRUE );
}


static void ConstraintOrig( coOrd * orig, coOrd size, wBool_t bNoBorder, wBool_t bRound  )
{
LOG( log_pan, 2, ( "ConstraintOrig [ %0.6f, %0.6f ] RoomSize(%0.3f %0.3f), WxH=%0.3fx%0.3f",
				orig->x, orig->y, mapD.size.x, mapD.size.y,
				size.x, size.y ) )

	coOrd bound = zero;

    if ( !bNoBorder ) {
		bound.x = size.x/2;
		bound.y = size.y/2;
	}



	if (orig->x > 0.0) {
		if ((orig->x+size.x) > (mapD.size.x+bound.x)) {
			orig->x = mapD.size.x-size.x+bound.x;
			//orig->x += (units==UNITS_ENGLISH?1.0:(1.0/2.54));
		}
	}

	if (orig->x < (0-bound.x))
				orig->x = 0-bound.x;

	if (orig->y > 0.0) {
		if ((orig->y+size.y) > (mapD.size.y+bound.y) ) {
			orig->y = mapD.size.y-size.y+bound.y;
			//orig->y += (units==UNITS_ENGLISH?1.0:1.0/2.54);

		}
	}

	if (orig->y < (0-bound.y))
				orig->y = 0-bound.y;

	if (bRound) {
		orig->x = round(orig->x*pixelBins)/pixelBins;
		orig->y = round(orig->y*pixelBins)/pixelBins;
	}
	LOG( log_pan, 2, ( " = [ %0.6f %0.6f ]\n", orig->x, orig->y ) )
}

/**
 * Initialize the menu for setting zoom factors. 
 * 
 * \param IN zoomM			Menu to which radio button is added
 * \param IN zoomSubM	Second menu to which radio button is added, ignored if NULL
 * \param IN ctxMenu1
 * \param IN ctxMenu2
 *
 */

EXPORT void InitCmdZoom( wMenu_p zoomM, wMenu_p zoomSubM, wMenu_p ctxMenu1, wMenu_p panMenu )
{
	int inx;
	
	for ( inx=0; inx<COUNT( zoomList ); inx++ ) {
		if( (zoomList[ inx ].value >= 1.0 && zoomList[ inx ].value<=10 ) ||
				 (ceil(log2(zoomList[ inx ].value)) == floor(log2(zoomList[ inx ].value))))
				{
			if (zoomM)
				zoomList[inx].btRadio = wMenuRadioCreate( zoomM, "cmdZoom", zoomList[inx].name, 0, DoZoom, (&(zoomList[inx].value)));
			if( zoomSubM )
				zoomList[inx].pdRadio = wMenuRadioCreate( zoomSubM, "cmdZoom", zoomList[inx].name, 0, DoZoom, (&(zoomList[inx].value)));
			if (panMenu)
				zoomList[inx].panRadio = wMenuRadioCreate( panMenu, "cmdZoom", zoomList[inx].name, 0, DoZoom, (&(zoomList[inx].value)));
		}
		if ((zoomList[inx].value >=1.0 && zoomList[inx].value <= 10.0) || (ceil(log2(zoomList[ inx ].value)) == floor(log2(zoomList[ inx ].value)))) {
			if (ctxMenu1)
				zoomList[inx].ctxRadio1 = wMenuRadioCreate( ctxMenu1, "cmdZoom", zoomList[inx].name, 0, DoZoom, (&(zoomList[inx].value)));
		}
	}
}

/**
 * Set radio button(s) corresponding to current scale.
 * 
 * \param IN scale		current scale
 *
 */

static void SetZoomRadio( DIST_T scale )
{
	int inx;
	long curScale = (long)scale;
	
	for ( inx=0; inx<COUNT( zoomList ); inx++ ) {
		if( curScale == zoomList[inx].value ) {
			if (zoomList[inx].btRadio)
				wMenuRadioSetActive( zoomList[inx].btRadio );
			if( zoomList[inx].pdRadio )
				wMenuRadioSetActive( zoomList[inx].pdRadio );
			if (zoomList[inx].ctxRadio1)
				wMenuRadioSetActive( zoomList[inx].ctxRadio1 );
			if (zoomList[inx].panRadio)
				wMenuRadioSetActive( zoomList[inx].panRadio );
			/* activate / deactivate zoom buttons when appropriate */				
			wControlLinkedActive( (wControl_p)zoomUpB, ( inx != 0 ) );
			wControlLinkedActive( (wControl_p)zoomDownB, ( inx < (COUNT( zoomList ) - 1)));			
		}
	}
}	

/**
 * Find current scale 
 *
 * \param IN scale current scale
 * \return index in scale table or -1 if error
 *
 */
 
static int ScaleInx(  DIST_T scale )
{
	int inx;
	
	for ( inx=0; inx<COUNT( zoomList); inx++ ) {
		if( scale == zoomList[inx].value ) {
			return inx;
		}
	}
	return -1;	
}

/*
 * Find Nearest Scale
 */

static int NearestScaleInx ( DIST_T scale, BOOL_T larger ) {
	int inx;

		for ( inx=0; inx<COUNT( zoomList ); inx++ ) {
			if( scale == zoomList[inx].value ) {
				return inx;
			}
			if (scale < zoomList[inx].value) return inx;
		}
		return inx-1;
}

/**
 * Set up for new drawing scale. After the scale was changed, eg. via zoom button, everything 
 * is set up for the new scale. 
 *
 * \param scale IN new scale
 */
	
static void DoNewScale( DIST_T scale )
{
	char tmp[20];

	if (scale > MAX_MAIN_SCALE)
		scale = MAX_MAIN_SCALE;

	tempD.scale = mainD.scale = scale;
	mainD.dpi = wDrawGetDPI( mainD.d );
	if ( scale > 1.0 && scale <= 12.0 ) {
		mainD.dpi = floor( (mainD.dpi + scale/2)/scale) * scale;
	}
	tempD.dpi = mainD.dpi;

	SetZoomRadio( scale ); 
	InfoScale();
	SetMainSize(); 
	PanHere( I2VP(1) );
LOG( log_zoom, 1, ( "center = [%0.3f %0.3f]\n", mainCenter.x, mainCenter.y ) )
	sprintf( tmp, "%0.3f", mainD.scale );
	wPrefSetString( "draw", "zoom", tmp );
	if (recordF) {
		fprintf( recordF, "ORIG %0.3f %0.3f %0.3f\n",
						mainD.scale, mainD.orig.x, mainD.orig.y );
	}
}


/**
 * User selected zoom in, via mouse wheel, button or pulldown.
 *
 * \param mode IN FALSE if zoom button was activated, TRUE if activated via popup or mousewheel
 */

EXPORT void DoZoomUp( void * mode )
{
	long newScale;
	int i;
	
	LOG( log_zoom, 2, ( "DoZoomUp KS:%x\n", MyGetKeyState() ) );
	if ( mode != NULL || (MyGetKeyState()&WKEY_SHIFT) == 0) {
		i = ScaleInx( mainD.scale );
		if (i < 0) i = NearestScaleInx(mainD.scale, TRUE);
		/* 
		 * Zooming into macro mode happens when we are at scale 1:1. 
		 * To jump into macro mode, the CTRL-key has to be pressed and held.
		 */
		if( mainD.scale != 1.0 || (mainD.scale == 1.0 && (MyGetKeyState()&WKEY_CTRL))) {
			if( i ) {
				if (mainD.scale <=1.0) 
					InfoMessage(_("Macro Zoom Mode"));
				else
					InfoMessage("");
				DoNewScale( zoomList[ i - 1 ].value );	
				
			} else InfoMessage("Minimum Macro Zoom");
		} else {
			InfoMessage(_("Scale 1:1 - Use Ctrl+ to go to Macro Zoom Mode"));
		}
	} else if ( (MyGetKeyState()&WKEY_CTRL) == 0 ) {
		wPrefGetInteger( "misc", "zoomin", &newScale, 4 );
		InfoMessage(_("Preset Zoom In Value selected. Shift+Ctrl+PageDwn to reset value"));
		DoNewScale( newScale );
	} else {
		wPrefSetInteger( "misc", "zoomin", (long)mainD.scale );
		InfoMessage( _("Zoom In Program Value %ld:1, Shift+PageDwn to use"), (long)mainD.scale );
	}
}

/*
 * Mode - 0 = Extents are Map size
 * Mode - 1 = Extents are Selected size
 */
EXPORT void DoZoomExtents( void * mode) {

	DIST_T scale_x, scale_y;
	if ( 1 == VP2L(mode) ) {
		if ( selectedTrackCount == 0 )
				return;
			track_p trk = NULL;
			coOrd bot, top;
			BOOL_T first = TRUE;
			while ( TrackIterate( &trk ) ) {
			    if(GetTrkSelected(trk)) {
			    	coOrd hi, lo;
			    	GetBoundingBox(trk,&hi,&lo);
			    	if (first) {
			    		first = FALSE;
			    		bot = lo;
			    		top = hi;
			    	} else {
			    		if (lo.x < bot.x) bot.x = lo.x;
			    		if (lo.y < bot.y) bot.y = lo.y;
			    		if (hi.x > top.x) top.x = hi.x;
			    		if (hi.y > top.y) top.y = hi.y;
			    	}
			    }
			}
		panCenter.x = (top.x/2)+(bot.x)/2;
		panCenter.y = (top.y/2)+(bot.y)/2;
		PanHere(I2VP(1));
		scale_x = (top.x-bot.x)*1.5/(mainD.size.x/mainD.scale);
		scale_y = (top.y-bot.y)*1.5/(mainD.size.y/mainD.scale);
	} else {
		scale_x = mapD.size.x/(mainD.size.x/mainD.scale);
		scale_y = mapD.size.y/(mainD.size.y/mainD.scale);
	}


	if (scale_x<scale_y)
		scale_x = scale_y;
	scale_x = ceil(scale_x);
	if (scale_x < 1) scale_x = 1;
	if (scale_x > MAX_MAIN_SCALE) scale_x = MAX_MAIN_SCALE;
	if (1 != (intptr_t)1)
		mainD.orig = zero;
	DoNewScale(scale_x);
	MainLayout(TRUE,TRUE);

}


/**
 * User selected zoom out, via mouse wheel, button or pulldown.
 *
 * \param mode IN FALSE if zoom button was activated, TRUE if activated via popup or mousewheel
 */

EXPORT void DoZoomDown( void  * mode)
{
	long newScale;
	int i;
	
	LOG( log_zoom, 2, ( "DoZoomDown KS:%x\n", MyGetKeyState() ) );
	if ( mode != NULL || (MyGetKeyState()&WKEY_SHIFT) == 0 ) {
		i = ScaleInx( mainD.scale );
		if (i < 0) i = NearestScaleInx(mainD.scale, TRUE);
		if( i>= 0 && i < ( COUNT( zoomList ) - 1 )) {
			InfoMessage("");
			DoNewScale( zoomList[ i + 1 ].value );
		} else
			InfoMessage(_("At Maximum Zoom Out"));
					
			
	} else if ( (MyGetKeyState()&WKEY_CTRL) == 0 ) {
		wPrefGetInteger( "misc", "zoomout", &newScale, 16 );
		InfoMessage(_("Preset Zoom Out Value selected. Shift+Ctrl+PageUp to reset value"));
		DoNewScale( newScale );
	} else {
		wPrefSetInteger( "misc", "zoomout", (long)mainD.scale );
		InfoMessage( _("Zoom Out Program Value %ld:1 set, Shift+PageUp to use"), (long)mainD.scale );
	}
}

/**
 * Zoom to user selected value. This is the callback function for the 
 * user-selectable preset zoom values. 
 *
 * \param IN scale current pScale
 *
 */
static void DoZoom( void * pScaleVP )
{
	DIST_T *pScale = pScaleVP;
	DIST_T scale = *pScale;

	if( scale != mainD.scale )
		DoNewScale( scale );
}


EXPORT void Pix2CoOrd(
		drawCmd_p d,
		wDrawPix_t x,
		wDrawPix_t y,
		coOrd * pos )
{
	pos->x = (((DIST_T)x)/d->dpi)*d->scale+d->orig.x;
	pos->y = (((DIST_T)y)/d->dpi)*d->scale+d->orig.y;
}

EXPORT void CoOrd2Pix(
		drawCmd_p d,
		coOrd pos,
		wDrawPix_t * x,
		wDrawPix_t * y )
{
	*x = (wDrawPix_t)((pos.x-d->orig.x)/d->scale*d->dpi);
	*y = (wDrawPix_t)((pos.y-d->orig.y)/d->scale*d->dpi);
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
EXPORT void PanHere(void * mode) {
	if ( 3 == VP2L(mode ) ) {
		panCenter = menuPos;
		LOG( log_pan, 2, ( "MCenter:Mod-%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
	}
	mainD.orig.x = panCenter.x - mainD.size.x/2.0;
	mainD.orig.y = panCenter.y - mainD.size.y/2.0;
	wBool_t bNoBorder = (constrainMain != 0);
	if ( 1 != VP2L(mode) )
		if ( (MyGetKeyState()&WKEY_CTRL)!= 0 )
			bNoBorder = !bNoBorder;
	wBool_t bLiveMap = TRUE;
	if ( 2 == VP2L(mode) )
		bLiveMap = liveMap;

	MainLayout( bLiveMap, bNoBorder ); // PanHere
}


static int DoPanKeyAction( wAction_t action )
{
	switch ((wAccelKey_e)(action>>8)&0xFF) {
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
		panCenter.x = mainD.orig.x + mainD.size.x/2;
		panCenter.y = mainD.orig.y + mainD.size.y/2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0)
			panCenter.x += 0.25*mainD.scale;    //~1cm in 1::1, 1ft in 30:1, 1mm in 10:1
		else
			panCenter.x += mainD.size.x/2;
		LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
		PanHere(I2VP(0));
		break;

	case wAccelKey_Left:
		panCenter.x = mainD.orig.x + mainD.size.x/2;
		panCenter.y = mainD.orig.y + mainD.size.y/2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0)
			panCenter.x -= 0.25*mainD.scale;
		else
			panCenter.x -= mainD.size.x/2;
		LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
		PanHere(I2VP(0));
		break;

	case wAccelKey_Up:
		panCenter.x = mainD.orig.x + mainD.size.x/2;
		panCenter.y = mainD.orig.y + mainD.size.y/2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0)
			panCenter.y += 0.25*mainD.scale;
		else
			panCenter.y += mainD.size.y/2;
		LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
		PanHere(I2VP(0));
		break;

	case wAccelKey_Down:
		panCenter.x = mainD.orig.x + mainD.size.x/2;
		panCenter.y = mainD.orig.y + mainD.size.y/2;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0)
			panCenter.y -= 0.25*mainD.scale;
		else
			panCenter.y -= mainD.size.y/2;
		LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
		PanHere(I2VP(0));
		break;

	default:
		return 0;
	}
	return 0;
}


static void DoMapPan( wAction_t action, coOrd pos )
{
	static coOrd mapOrig;
	static coOrd size;
	static DIST_T xscale, yscale;
	static enum { noPan, movePan, resizePan } mode = noPan;
	wDrawPix_t x, y;

	switch (action & 0xFF) {

	case C_DOWN:
		if ( mode == noPan )
			mode = movePan;
		else
			break;
	case C_MOVE:
		if ( mode != movePan )
			break;
//		mainD.orig.x = pos.x - mainD.size.x/2.0;
//		mainD.orig.y = pos.y - mainD.size.y/2.0;
		panCenter = pos;
LOG( log_pan, 1, ( "%s = [ %0.3f, %0.3f ]\n", action == C_DOWN? "START":"MOVE", mainD.orig.x, mainD.orig.y ) )
		PanHere( I2VP(2));
		break;
	case C_UP:
		if ( mode != movePan )
			break;
		panCenter = pos;
		PanHere( I2VP(0));
LOG( log_pan, 1, ( "FINAL = [ %0.3f, %0.3f ]\n", mainD.orig.x, mainD.orig.y ) )
		mode = noPan;
		break;

	case C_RDOWN:
		if ( mode == noPan )
			mode = resizePan;
		else
			break;
		mapOrig = pos;
		size.x = mainD.size.x/mainD.scale;  //How big screen?
		size.y = mainD.size.y/mainD.scale;
		xscale = mainD.scale; //start at current
		panCenter = pos;
LOG( log_pan, 1, ( "START %0.3fx%0.3f %0.3f+%0.3f\n", mapOrig.x, mapOrig.y, size.x, size.y ) )
		break;

	case C_RMOVE:
		if ( mode != resizePan )
			break;
		if (pos.x < 0)
			pos.x = 0;
		if (pos.x > mapD.size.x)
			pos.x = mapD.size.x;
		if (pos.y < 0)
			pos.y = 0;
		if (pos.y > mapD.size.y)
			pos.y = mapD.size.y;

		xscale = fabs((pos.x-mapOrig.x)*2.0/size.x);
		yscale = fabs((pos.y-mapOrig.y)*2.0/size.y);
		if (xscale < yscale)
			xscale = yscale;
		xscale = ceil( xscale );

		if (xscale < 0.01)
			xscale = 0.01;
		if (xscale > 64)
			xscale = 64;

		mainD.size.x = size.x * xscale;
		mainD.size.y = size.y * xscale;
		mainD.orig.x = mapOrig.x - mainD.size.x / 2.0;
		mainD.orig.y = mapOrig.y - mainD.size.y / 2.0;
		tempD.scale = mainD.scale = xscale;
		PanHere( I2VP(2));
LOG( log_pan, 1, ( "MOVE SCL:%0.3f %0.3fx%0.3f %0.3f+%0.3f\n", xscale, mainD.orig.x, mainD.orig.y, mainD.size.x, mainD.size.y ) )
		InfoScale();
	    break;

	case C_RUP:
		if ( mode != resizePan )
			break;
		DoNewScale( xscale );
		mode = noPan;
		break;

	case wActionExtKey:
		mainD.CoOrd2Pix(&mainD,pos,&x,&y);
		DoPanKeyAction( action );
		mainD.Pix2CoOrd( &mainD, x, y, &pos );
		InfoPos( pos );
		return;

	default:
		return;
	}
}


/*
* IsClose
* is distance smaller than 10 pixels at 72 DPI?
*/
EXPORT BOOL_T IsClose(
		DIST_T d )
{
	wDrawPix_t pd;
	pd = (wDrawPix_t)(d/mainD.scale * mainD.dpi);
	return pd <= closePixels;
}

/*****************************************************************************
 *
 * MAIN MOUSE HANDLER
 *
 */

static int ignoreMoves = 0;

EXPORT void ResetMouseState( void )
{
	mouseState = mouseNone;
}


EXPORT void FakeDownMouseState( void )
{
	mouseState = mouseLeftPending;
}

/**
 * Return the current position of the mouse pointer in drawing coordinates.
 *
 * \param x OUT pointer x position
 * \param y OUT pointer y position
 * \return 
 */

void
GetMousePosition( wDrawPix_t *x, wDrawPix_t *y )
{
	if( x && y ) {
		*x = mousePositionx;
		*y = mousePositiony;
	}
}

coOrd minIncrementSizes() {
	double x,y;
	if (mainD.scale >= 1.0) {
		if (units == UNITS_ENGLISH) {
			x = 0.25;   //>1:1 = 1/4 inch
			y = 0.25;
		} else {
			x = 1/(2.54*2);  //>1:1 = 0.5 cm
			y = 1/(2.54*2);
		}
	} else {
		if (units == UNITS_ENGLISH) {
			x = 1/64;   //<1:1 = 1/64 inch
			y = 1/64;
		} else {
			x = 1/(25.4*2);  //>1:1 = 0.5 mm
			y = 1/(25.4*2);
		}
	}
	coOrd ret;
	ret.x = x*1.5;
	ret.y = y*1.5;
	return ret;
}

static void DoMouse( wAction_t action, coOrd pos )
{

	BOOL_T rc;
	wDrawPix_t x, y;
	static BOOL_T ignoreCommands;

	LOG( log_mouse, 2, ( "DoMouse( %d, %0.3f, %0.3f )\n", action, pos.x, pos.y ) )

	if (recordF) {
		RecordMouse( "MOUSE", action, pos.x, pos.y );
	}



	switch (action&0xFF) {
	case C_UP:
		if (mouseState != mouseLeft)
			return;
		if (ignoreCommands) {
			ignoreCommands = FALSE;
			return;
		}
		mouseState = mouseNone;
		break;
	case C_RUP:
		if (mouseState != mouseRight)
			return;
		if (ignoreCommands) {
			ignoreCommands = FALSE;
			return;
		}
		mouseState = mouseNone;
		break;
	case C_MOVE:
		if (mouseState == mouseLeftPending ) {
			action = C_DOWN;
			mouseState = mouseLeft;
		}
		if (mouseState != mouseLeft)
			return;
		if (ignoreCommands)
			 return;
		break;
	case C_RMOVE:
		if (mouseState != mouseRight)
			return;
		if (ignoreCommands)
			 return;
		break;
	case C_DOWN:
		mouseState = mouseLeft;
		break;
	case C_RDOWN:
		mouseState = mouseRight;
		break;
	}

	inError = FALSE;
	if ( deferSubstituteControls[0] )
		InfoSubstituteControls( deferSubstituteControls, deferSubstituteLabels );

//	panCenter.y = mainD.orig.y + mainD.size.y/2;
//	panCenter.x = mainD.orig.x + mainD.size.x/2;
//printf( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y );

	coOrd min = minIncrementSizes();

	switch ( action&0xFF ) {
		case C_DOWN:
		case C_RDOWN:
			tempSegs_da.cnt = 0;
			break;
		case wActionMove:
			InfoPos( pos );
			if ( ignoreMoves )
				return;
			break;
		case wActionExtKey:
			mainD.CoOrd2Pix(&mainD,pos,&x,&y);
			if ((MyGetKeyState() &
					(WKEY_SHIFT | WKEY_CTRL)) == (WKEY_SHIFT | WKEY_CTRL) &&
				( action>>8 == wAccelKey_Up ||
				  action>>8 == wAccelKey_Down ||
				  action>>8 == wAccelKey_Left ||
				  action>>8 == wAccelKey_Right ))
				break;  //Allow SHIFT+CTRL for Move
			if (((action>>8)&0xFF) == wAccelKey_LineFeed) {
				action = C_TEXT+((int)(0x0A<<8));
				break;
			}
			int rc = DoPanKeyAction(action);
			if (rc!=1) return;
			break;
		case C_TEXT:
			if ((action>>8) == 0x0D) {
				action = C_OK;
			} else if ((action>>8) == 0x1B) {
				ConfirmReset( TRUE );
				return;
			}
			/*no break */
		case C_MODKEY:
		case C_MOVE:
		case C_UP:
		case C_RMOVE:
		case C_RUP:
		case C_LDOUBLE:
			InfoPos( pos );
			/*DrawTempTrack();*/
			break;
		case C_WUP:
			DoZoomUp(I2VP(1L));			
			break;
		case C_WDOWN:
			DoZoomDown(I2VP(1L));
			break;
		case C_SCROLLUP:
			panCenter.y = panCenter.y + ((mainD.size.y/20>min.y)?mainD.size.y/20:min.y);
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			PanHere(I2VP(1));
			break;
		case C_SCROLLDOWN:
			panCenter.y = panCenter.y - ((mainD.size.y/20>min.y)?mainD.size.y/20:min.y);
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			PanHere(I2VP(1));
			break;
		case C_SCROLLLEFT:
			panCenter.x = panCenter.x - ((mainD.size.x/20>min.x)?mainD.size.x/20:min.x);
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			PanHere(I2VP(1));
			break;
		case C_SCROLLRIGHT:
			panCenter.x = panCenter.x + ((mainD.size.x/20>min.x)?mainD.size.x/20:min.x);
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			PanHere(I2VP(1));
			break;
		default:
			NoticeMessage( MSG_DOMOUSE_BAD_OP, _("Ok"), NULL, action&0xFF );
			break;
		}
		if (delayUpdate)
		wDrawDelayUpdate( mainD.d, TRUE );
		rc = DoCurCommand( action, pos );
		wDrawDelayUpdate( mainD.d, FALSE );
		switch( rc ) {
		case C_CONTINUE:
			/*DrawTempTrack();*/
			break;
		case C_ERROR:
			ignoreCommands = TRUE;
			inError = TRUE;
			Reset();
			LOG( log_mouse, 1, ( "Mouse returns Error\n" ) )
			break;
		case C_TERMINATE:
			Reset();
			DoCurCommand( C_START, zero );
			break;
		case C_INFO:
			Reset();
			break;
		}
}


wDrawPix_t autoPanFactor = 10;
static void DoMousew( wDraw_p d, void * context, wAction_t action, wDrawPix_t x, wDrawPix_t y )
{
	coOrd pos;
	coOrd orig;
	wWinPix_t w, h;
	static wDrawPix_t lastX, lastY;
	DIST_T minDist;
	wDrawGetSize( mainD.d, &w, &h );
	if ( autoPan && !inPlayback ) {
		if ( action == wActionLDown || action == wActionRDown ||
			 (action == wActionLDrag && mouseState == mouseLeftPending ) /*||
			 (action == wActionRDrag && mouseState == mouseRightPending ) */ ) {
			lastX = x;
			lastY = y;
		}
		if ( action == wActionLDrag || action == wActionRDrag ) {
			orig = mainD.orig;
			if ( ( x < 10 && x < lastX ) ||
				 ( x > w-10 && x > lastX ) ||
				 ( y < 10 && y < lastY ) ||
				 ( y > h-10 && y > lastY ) ) {
				mainD.Pix2CoOrd( &mainD, x, y, &pos );
				orig.x = mainD.orig.x + (pos.x - (mainD.orig.x + mainD.size.x/2.0) )/autoPanFactor;
				orig.y = mainD.orig.y + (pos.y - (mainD.orig.y + mainD.size.y/2.0) )/autoPanFactor;
				if ( orig.x != mainD.orig.x || orig.y != mainD.orig.y ) {
					if ( mainD.scale >= 1 ) {
						if ( units == UNITS_ENGLISH )
							minDist = 1.0;
						else
							minDist = 1.0/2.54;
						if (  orig.x != mainD.orig.x ) {
							if ( fabs( orig.x-mainD.orig.x ) < minDist ) {
								if ( orig.x < mainD.orig.x )
									orig.x -= minDist;
								else
									orig.x += minDist;
							}
						}
						if (  orig.y != mainD.orig.y ) {
							if ( fabs( orig.y-mainD.orig.y ) < minDist ) {
								if ( orig.y < mainD.orig.y )
									orig.y -= minDist;
								else
									orig.y += minDist;
							}
						}
					}
					mainD.orig = orig;
					panCenter.x = mainD.orig.x + mainD.size.x/2.0;
					panCenter.y = mainD.orig.y + mainD.size.y/2.0;
					LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
					MainLayout( TRUE, TRUE ); // DoMouseW: autopan
					tempD.orig = mainD.orig;
					wFlush();
				}
			}
			lastX = x;
			lastY = y;
		}
	}
	mainD.Pix2CoOrd( &mainD, x, y, &pos );
	switch (action & 0xFF) {
	case wActionMove:
	case wActionLDown:
	case wActionLDrag:
	case wActionLUp:
	case wActionRDown:
	case wActionRDrag:
	case wActionRUp:
	case wActionLDownDouble:
		mousePositionx = x;
		mousePositiony = y;
		break;
	default:
		break;
	}

	DoMouse( action, pos );

	if (( x < 20 ) || ( x > w-20 ) || ( y < 20 ) || ( y > h-20 ) ) {
		wSetCursor(mainD.d,defaultCursor);  //Add system cursor if close to edges
	}
}

static wBool_t PlaybackMain( char * line )
{
	int rc;
	int action;
	coOrd pos;

	SetCLocale();

	rc=sscanf( line, "%d " SCANF_FLOAT_FORMAT SCANF_FLOAT_FORMAT, &action, &pos.x, &pos.y);
	SetUserLocale();

	if (rc != 3) {
		SyntaxError( "MOUSE", rc, 3 );
	} else {
		PlaybackMouse( DoMouse, &tempD, (wAction_t)action, pos, wDrawColorBlack );
	}
	return TRUE;
}

static wBool_t PlaybackKey( char * line )
{
	int rc;
	int action = C_TEXT;
	coOrd pos;
	int c;

	SetCLocale();
	rc=sscanf( line, "%d " SCANF_FLOAT_FORMAT SCANF_FLOAT_FORMAT, &c, &pos.x, &pos.y );
	SetUserLocale();

	if (rc != 3) {
		SyntaxError( "MOUSE", rc, 3 );
	} else {
		action = action|c<<8;
		PlaybackMouse( DoMouse, &tempD, (wAction_t)action, pos, wDrawColorBlack );
	}
	return TRUE;
}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

static paramDrawData_t mapDrawData = { 100, 100, MapRedraw, DoMapPan, &mapD };
static paramData_t mapPLs[] = {
	{	PD_DRAW, NULL, "canvas", 0, &mapDrawData } };
static paramGroup_t mapPG = { "map", PGO_NODEFAULTPROC | PGO_DIALOGTEMPLATE, mapPLs, COUNT( mapPLs ) };

static void MapDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	wWinPix_t width,height;
	switch(inx) {
		case wResize_e:
			if (mapD.d == NULL)
				return;
			wWinGetSize( mapW, &width, &height );
			if (height >= 100) {
				wControlSetPos( (wControl_p)mapD.d, 0, 0 );
				double scaleX = (mapD.size.x/((width-DlgSepLeft-DlgSepRight-10)/mapD.dpi));
				double scaleY = (mapD.size.y/((height-DlgSepTop-DlgSepBottom-10)/mapD.dpi));
				double scale;

				if (scaleX<scaleY) scale = scaleX;
				else scale = scaleY;

				if (scale > MAX_MAIN_SCALE) scale = MAX_MAIN_SCALE;
				if (scale < MIN_MAIN_MACRO) scale = MIN_MAIN_MACRO;

				mapScale = (long)scale;

				mapD.scale = mapScale;
				ChangeMapScale(FALSE);

				if (mapVisible) {
					MapRedraw( mapD.d, NULL, 0, 0 );
				}
				wPrefSetInteger( "draw", "mapscale", (long)mapD.scale );
			}
			break;
		case -1:
			MapWindowShow( FALSE );
			break;
	default:
		break;
	}
}


static void DrawChange( long changes )
{
	if (changes & CHANGE_MAIN) {
		MainLayout( TRUE, FALSE ); // DrawChange: CHANGE_MAIN
	}
	if (changes &CHANGE_UNITS)
		SetInfoBar();
	if (changes & CHANGE_MAP)
		MapResize();
}


static void MainLayoutCB(
	wDraw_p bd, void * pContex, wWinPix_t px, wWinPix_t py )
{
	MainLayout( TRUE, FALSE );
}


EXPORT void DrawInit( int initialZoom )
{
	wWinPix_t w, h;


	wWinGetSize( mainW, &w, &h );
	/*LayoutToolBar();*/
	if (wWinIsTemplated(mainW)) {
		if ( w <= 0 ) w = 1;
		if ( h <= 0 ) h = 1;
		tempD.d = mainD.d = wDrawCreate( mainW, 0, 0, "main-maindraw", BD_TICKS|BO_USETEMPLATE|BD_MODKEYS,
												w, h, &mainD,
												(wDrawRedrawCallBack_p)MainLayoutCB, DoMousew );

	} else {
		h = h - (toolbarHeight+max(textHeight,infoHeight)+10);
		if ( w <= 0 ) w = 1;
		if ( h <= 0 ) h = 1;
		tempD.d = mainD.d = wDrawCreate( mainW, 0, toolbarHeight, "main-maindraw", BD_TICKS,
											w, h, &mainD,
											(wDrawRedrawCallBack_p)MainLayoutCB, DoMousew );
	}

	if (initialZoom == 0) {
		WDOUBLE_T tmpR;
		wPrefGetFloat( "draw", "zoom", &tmpR, mainD.scale );
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
	mainD.dpi = wDrawGetDPI( mainD.d );
	if ( mainD.scale > 1.0 && mainD.scale <= 12.0 ) {
		mainD.dpi = floor( (mainD.dpi + mainD.scale/2)/mainD.scale) * mainD.scale;
	}
	tempD.dpi = mainD.dpi;

	SetMainSize();
	panCenter.x = mainD.size.x/2 +mainD.orig.x;
	panCenter.y = mainD.size.y/2 +mainD.orig.y;
	mapD.scale = mapScale;
	/*w = (wWinPix_t)((mapD.size.x/mapD.scale)*mainD.dpi + 0.5)+2;*/
	/*h = (wWinPix_t)((mapD.size.y/mapD.scale)*mainD.dpi + 0.5)+2;*/
	ParamRegister( &mapPG );
	mapW = ParamCreateDialog( &mapPG, MakeWindowTitle(_("Map")), NULL, NULL, NULL, FALSE, NULL, F_RESIZE, MapDlgUpdate );
	ChangeMapScale(TRUE);

	log_pan = LogFindIndex( "pan" );
	log_zoom = LogFindIndex( "zoom" );
	log_mouse = LogFindIndex( "mouse" );
	log_redraw = LogFindIndex( "redraw" );
	log_timemainredraw = LogFindIndex( "timemainredraw" );

	AddPlaybackProc( "MOUSE ", (playbackProc_p)PlaybackMain, NULL );
	AddPlaybackProc( "KEY ", (playbackProc_p)PlaybackKey, NULL );

	rulerFp = wStandardFont( F_HELV, FALSE, FALSE );

	SetZoomRadio( mainD.scale ); 
	InfoScale();
	SetInfoBar();
	InfoPos( zero );
	RegisterChangeNotification( DrawChange );
}

#include "bitmaps/pan-zoom.xpm"

EXPORT static wMenu_p panPopupM;

static STATUS_T CmdPan(
		wAction_t action,
		coOrd pos )
{
	static enum { PAN, ZOOM, NONE } panmode;

	static coOrd base, size;

	DIST_T scale_x,scale_y;

	static coOrd start_pos;
	if ( action == C_DOWN ) {
		panmode = PAN;
	} else if ( action == C_RDOWN) {
		panmode = ZOOM;
	}

	switch (action&0xFF) {
	case C_START:
		start_pos = zero;
        panmode = NONE;
        InfoMessage(_("Left-Drag to pan, Ctrl+Left-Drag to zoom, 0 to set origin to zero, 1-9 to zoom#, e to set to extents"));
        wSetCursor(mainD.d,wCursorSizeAll);
		 break;
	case C_DOWN:
		if ((MyGetKeyState()&WKEY_CTRL) == 0) {
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
						min_inc = 1/4;   //>1:1 = 1/4 inch
					} else {
						min_inc = 1/(2.54*2);  //>1:1 = 0.5 cm
					}
				} else {
					if (units == UNITS_ENGLISH) {
						min_inc = 1/64;   //<1:1 = 1/64 inch
					} else {
						min_inc = 1/(25.4*2);  //>1:1 = 0.5 mm
					}
				}
				if ((fabs(pos.x-start_pos.x) > min_inc) || (fabs(pos.y-start_pos.y) > min_inc)) {
					coOrd oldOrig = mainD.orig;
					mainD.orig.x -= (pos.x - start_pos.x);
					mainD.orig.y -= (pos.y - start_pos.y);
					if ((MyGetKeyState()&WKEY_SHIFT) != 0)
						ConstraintOrig(&mainD.orig,mainD.size,TRUE,TRUE);
					if ((oldOrig.x == mainD.orig.x) && (oldOrig.y == mainD.orig.y))
						InfoMessage(_("Can't move any further in that direction"));
					else
					    InfoMessage(_("Left click to pan, right click to zoom, 'o' for origin, 'e' for extents"));
				}
			}
		else if (panmode == ZOOM) {
			base = start_pos;
			size.x = pos.x - base.x;
			if (size.x < 0) {
				size.x = - size.x ;
				base.x = pos.x;
			}
			size.y = pos.y - base.y;
			if (size.y < 0) {
				size.y = - size.y;
				base.y = pos.y;
			}
		}
		panCenter.x = mainD.orig.x + mainD.size.x/2.0;
		panCenter.y = mainD.orig.y + mainD.size.y/2.0;
		PanHere( I2VP(0));
		break;
	case C_UP:
		if (panmode == ZOOM) {
			scale_x = size.x/mainD.size.x*mainD.scale;
			scale_y = size.y/mainD.size.y*mainD.scale;

			if (scale_x<scale_y)
					scale_x = scale_y;
			if (scale_x>1) scale_x = ceil( scale_x );
			else scale_x = 1/(ceil(1/scale_x));

			if (scale_x > MAX_MAIN_SCALE) scale_x = MAX_MAIN_SCALE;
			if (scale_x < MIN_MAIN_MACRO) scale_x = MIN_MAIN_MACRO;

			mainD.orig.x = base.x;
			mainD.orig.y = base.y;

			panmode = NONE;
			InfoMessage(_("Left Drag to Pan, +CTRL to Zoom, 0 to set Origin to 0,0, 1-9 to Zoom#, e to set to Extent"));
			DoNewScale(scale_x);
			break;
		} else if (panmode == PAN) {
			panCenter.x = mainD.orig.x + mainD.size.x/2.0;
			panCenter.y = mainD.orig.y + mainD.size.y/2.0;
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			panmode = NONE;
		}
		break;
	case C_REDRAW:
		if (panmode == ZOOM) {
			if (base.x && base.y && size.x && size.y)
				DrawHilight( &tempD, base, size, TRUE );
		}
		break;
	case C_CANCEL:
		base = zero;
		wSetCursor(mainD.d,defaultCursor);
		return C_TERMINATE;
	case C_TEXT:
		panmode = NONE;

		if ((action>>8) == 'e') {     //"e"
			DoZoomExtents(I2VP(0));
		} else if((action>>8) == 's') {
			DoZoomExtents(I2VP(1));
		} else if (((action>>8) == '0') || ((action>>8) == 'o')) {     //"0" or "o"
			mainD.orig = zero;
			panCenter.x = mainD.size.x/2.0;
			panCenter.y = mainD.size.y/2.0;
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			MainLayout( TRUE, TRUE ); // CmdPan C_TEXT '0' 'o'
		} else if ((action>>8) >= '1' && (action>>8) <= '9') {         //"1" to "9"
			scale_x = (action>>8)&0x0F;
			DoNewScale(scale_x);
		} else if ((action>>8) == 'c') {				// "c"
			panCenter = pos;
			LOG( log_pan, 2, ( "PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			PanHere( I2VP(0)); // CmdPan C_TEXT 'c'
		}

		if ((action>>8) == 0x0D) {
			wSetCursor(mainD.d,defaultCursor);
			return C_TERMINATE;
		} else if ((action>>8) == 0x1B) {
			wSetCursor(mainD.d,defaultCursor);
			return C_TERMINATE;
		}
		break;
	case C_CMDMENU:
		menuPos = pos;
		wMenuPopupShow( panPopupM );
		return C_CONTINUE;
		break;
	}

	return C_CONTINUE;
}
static wMenuPush_p zoomExtents,panOrig,panHere;
static wMenuPush_p zoomLvl1,zoomLvl2,zoomLvl3,zoomLvl4,zoomLvl5,zoomLvl6,zoomLvl7,zoomLvl8,zoomLvl9;

EXPORT void PanMenuEnter( void * keyVP )
{
	int key = (int)VP2L(keyVP);
	int action;
	action = C_TEXT;
	action |= key<<8;
	CmdPan(action,zero);
}

extern wIndex_t selectCmdInx;
extern wIndex_t describeCmdInx;
extern wIndex_t joinCmdInx;
extern wIndex_t modifyCmdInx;

EXPORT void InitCmdPan( wMenu_p menu )
{
	panCmdInx = AddMenuButton( menu, CmdPan, "cmdPan", _("Pan/Zoom"), wIconCreatePixMap(pan_zoom_xpm[iconSize]),
				LEVEL0, IC_CANCEL|IC_POPUP|IC_LCLICK|IC_CMDMENU, ACCL_PAN, NULL );
}
EXPORT void InitCmdPan2( wMenu_p menu )
{
	panPopupM = MenuRegister( "Pan Options" );
	wMenuPushCreate(panPopupM, "cmdSelectMode", GetBalloonHelpStr("cmdSelectMode"), 0, DoCommandB, I2VP(selectCmdInx));
	wMenuPushCreate(panPopupM, "cmdDescribeMode", GetBalloonHelpStr("cmdDescribeMode"), 0, DoCommandB, I2VP(describeCmdInx));
	wMenuPushCreate(panPopupM, "cmdModifyMode", GetBalloonHelpStr("cmdModifyMode"), 0, DoCommandB, I2VP(modifyCmdInx));
	wMenuSeparatorCreate(panPopupM);
	zoomExtents = wMenuPushCreate( panPopupM, "", _("Zoom to extents - 'e'"), 0, PanMenuEnter, I2VP( 'e'));
	zoomLvl1 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:1 - '1'"), 0, PanMenuEnter, I2VP( '1'));
	zoomLvl2 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:2 - '2'"), 0, PanMenuEnter, I2VP( '2'));
	zoomLvl3 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:3 - '3'"), 0, PanMenuEnter, I2VP( '3'));
	zoomLvl4 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:4 - '4'"), 0, PanMenuEnter, I2VP( '4'));
	zoomLvl5 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:5 - '5'"), 0, PanMenuEnter, I2VP( '5'));
	zoomLvl6 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:6 - '6'"), 0, PanMenuEnter, I2VP( '6'));
	zoomLvl7 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:7 - '7'"), 0, PanMenuEnter, I2VP( '7'));
	zoomLvl8 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:8 - '8'"), 0, PanMenuEnter, I2VP( '8'));
	zoomLvl9 = wMenuPushCreate( panPopupM, "", _("Zoom to 1:9 - '9'"), 0, PanMenuEnter, I2VP( '9'));
	panOrig = wMenuPushCreate( panPopupM, "", _("Pan to Origin - 'o'/'0'"), 0, PanMenuEnter, I2VP( 'o'));
	wMenu_p zoomPanM = wMenuMenuCreate(panPopupM, "", _("&Zoom"));
	InitCmdZoom(NULL, NULL, NULL, zoomPanM);
	panHere = wMenuPushCreate( panPopupM, "", _("Pan center here - 'c'"), 0, PanHere, I2VP( 3));
}
