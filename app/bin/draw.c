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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_MALLOC_C
#include <malloc.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>
#ifndef WINDOWS
#include <unistd.h>
#include <sys/time.h>
#else
#include <sys/timeb.h>
#endif

#include "cselect.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "misc.h"
#include "param.h"
#include "track.h"
#include "utility.h"

static void DrawRoomWalls( wBool_t );
EXPORT void DrawMarkers( void );
static void ConstraintOrig( coOrd *, coOrd );

static int log_pan = 0;
static int log_zoom = 0;
static int log_mouse = 0;

static wFontSize_t drawMaxTextFontSize = 100;

extern long zoomCorner; 

/****************************************************************************
 *
 * EXPORTED VARIABLES
 *
 */

#define INIT_MAIN_SCALE (8.0)
#define INIT_MAP_SCALE	(64.0)
#define MAX_MAIN_SCALE	(256.0)
#define MIN_MAIN_SCALE	(1.0)

// static char FAR message[STR_LONG_SIZE];

EXPORT wPos_t closePixels = 10;
EXPORT long maxArcSegStraightLen = 100;
EXPORT long drawCount;
EXPORT BOOL_T drawEnable = TRUE;
EXPORT long currRedraw = 0;

EXPORT wDrawColor drawColorBlack;
EXPORT wDrawColor drawColorWhite;
EXPORT wDrawColor drawColorRed;
EXPORT wDrawColor drawColorBlue;
EXPORT wDrawColor drawColorGreen;
EXPORT wDrawColor drawColorAqua;
EXPORT wDrawColor drawColorPurple;
EXPORT wDrawColor drawColorGold;

EXPORT DIST_T pixelBins = 80;

/****************************************************************************
 *
 * LOCAL VARIABLES
 *
 */

static wPos_t infoHeight;
static wPos_t textHeight;
EXPORT wWin_p mapW;
EXPORT BOOL_T mapVisible;

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
		wPos_t scale_w;
		wPos_t count_w;
		wPos_t pos_w;
		wPos_t info_w;
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
static int mousePositionx, mousePositiony;	/**< position of mouse pointer */

static int delayUpdate = 1;

static char xLabel[] = "X : ";
static char yLabel[] = "Y : ";
static char zoomLabel[] = "Zoom : ";

static struct {
		char * name;
		double value;
		wMenuRadio_p pdRadio;
		wMenuRadio_p btRadio;
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
				{ "128:1", 128.0 },
				{ "256:1", 256.0 },
};



/****************************************************************************
 *
 * DRAWING
 *
 */

static void MainCoOrd2Pix( drawCmd_p d, coOrd p, wPos_t * x, wPos_t * y )
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
	*x = ((wPos_t)t) + ((d->options&DC_TICKS)?LBORDER:0);
	t = p.y*d->dpi;
	if ( t > 0.0 )
		t += 0.5;
	else
		t -= 0.5;
	*y = ((wPos_t)t) + ((d->options&DC_TICKS)?BBORDER:0);
}


static int Pix2CoOrd_interpolate = 0;

static void MainPix2CoOrd(
		drawCmd_p d,
		wPos_t px,
		wPos_t py,
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
		printf ("px=%d x1=%0.6f x=%0.6f\n", px, x1, x );
	}
	if (y == y1)
		y += 1/bins/2;
}
	x += d->orig.x;
	y += d->orig.y;
	posR->x = x;
	posR->y = y;
}


static void DDrawLine(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		wDrawWidth width,
		wDrawColor color )
{
	wPos_t x0, y0, x1, y1;
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
	if (drawEnable) {
		wDrawLine( d->d, x0, y0, x1, y1,
				width, ((d->options&DC_DASH)==0)?wDrawLineSolid:wDrawLineDash,
				color, (wDrawOpts)d->funcs->options );
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
	wPos_t x, y;
	ANGLE_T da;
	coOrd p0, p1;
	DIST_T rr;
	int i, cnt;

	if (d == &mapD && !mapVisible)
		return;
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) {
		da = (maxArcSegStraightLen * 180) / (M_PI * rr);
		cnt = (int)(angle1/da) + 1;
		da = angle1 / cnt;
		PointOnCircle( &p0, p, r, angle0 );
		for ( i=1; i<=cnt; i++ ) {
			angle0 += da;
			PointOnCircle( &p1, p, r, angle0 );
			DrawLine( d, p0, p1, width, color );
			p0 = p1;
		}
		return;
	}
	if (d->angle!=0.0 && angle1 < 360.0)
		angle0 = NormalizeAngle( angle0-d->angle );
	d->CoOrd2Pix(d,p,&x,&y);
	drawCount++;
	if (drawEnable) {
		wDrawArc( d->d, x, y, (wPos_t)(rr), angle0, angle1, drawCenter,
				width, ((d->options&DC_DASH)==0)?wDrawLineSolid:wDrawLineDash,
				color, (wDrawOpts)d->funcs->options );
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
	wPos_t x, y;
	if (d == &mapD && !mapVisible)
		return;
	fontSize /= d->scale;
	d->CoOrd2Pix(d,p,&x,&y);
	wDrawString( d->d, x, y, d->angle-a, s, fp, fontSize, color, (wDrawOpts)d->funcs->options );
}


static void DDrawFillPoly(
		drawCmd_p d,
		int cnt,
		coOrd * pts,
		wDrawColor color )
{
	typedef wPos_t wPos2[2];
	static dynArr_t wpts_da;
	int inx;
	wPos_t x, y;
	DYNARR_SET( wPos2, wpts_da, cnt * 2 );
#define wpts(N) DYNARR_N( wPos2, wpts_da, N )
	for ( inx=0; inx<cnt; inx++ ) {
		d->CoOrd2Pix( d, pts[inx], &x, &y );
		wpts(inx)[0] = x;
		wpts(inx)[1] = y;
	}
	wDrawFilledPolygon( d->d, &wpts(0), cnt, color, (wDrawOpts)d->funcs->options );
}


static void DDrawFillCircle(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		wDrawColor color )
{
	wPos_t x, y;
	DIST_T rr;

	if (d == &mapD && !mapVisible)
		return;
	rr = (r / d->scale) * d->dpi + 0.5;
	if (rr > wDrawGetMaxRadius(d->d)) {
#ifdef LATER
		da = (maxArcSegStraightLen * 180) / (M_PI * rr);
		cnt = (int)(angle1/da) + 1;
		da = angle1 / cnt;
		PointOnCircle( &p0, p, r, angle0 );
		for ( i=1; i<=cnt; i++ ) {
			angle0 += da;
			PointOnCircle( &p1, p, r, angle0 );
			DrawLine( d, p0, p1, width, color );
			p0 = p1;
		}
#endif
		return;
	}
	d->CoOrd2Pix(d,p,&x,&y);
	drawCount++;
	if (drawEnable) {
		wDrawFilledCircle( d->d, x, y, (wPos_t)(rr),
				color, (wDrawOpts)d->funcs->options );
	}
}


EXPORT void DrawHilight( drawCmd_p d, coOrd p, coOrd s )
{
	wPos_t x, y, w, h;
	if (d == &mapD && !mapVisible)
		return;
#ifdef LATER
	if (d->options&DC_TEMPSEGS) {
		return;
	}
	if (d->options&DC_PRINT)
		return;
#endif
	w = (wPos_t)((s.x/d->scale)*d->dpi+0.5);
	h = (wPos_t)((s.y/d->scale)*d->dpi+0.5);
	d->CoOrd2Pix(d,p,&x,&y);
	wDrawFilledRectangle( d->d, x, y, w, h, wDrawColorBlack, wDrawOptTemp );
}


EXPORT void DrawHilightPolygon( drawCmd_p d, coOrd *p, int cnt )
{
	wPos_t q[4][2];
	int i;
#ifdef LATER
	if (d->options&DC_TEMPSEGS) {
		return;
	}
	if (d->options&DC_PRINT)
		return;
#endif
	ASSERT( cnt <= 4 );
	for (i=0; i<cnt; i++) {
		d->CoOrd2Pix(d,p[i],&q[i][0],&q[i][1]);
	}
	wDrawFilledPolygon( d->d, q, cnt, wDrawColorBlack, wDrawOptTemp );
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
		coOrd * hi)
{
	char * cp;
	char * cp1;
	POS_T lineH, lineW;
	coOrd size, textsize, posl, orig;
	POS_T descent;
	char *line;

	if (!text || !*text) {
		return;  //No string or blank
	}
	line = malloc(strlen(text) + 1);

	DrawTextSize2( &mainD, "Aqjlp", fp, fs, TRUE, &textsize, &descent);
	POS_T ascent = textsize.y-descent;
	lineH = ascent+descent*1.5;
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
		DrawTextSize2( &mainD, cp1, fp, fs, TRUE, &textsize, &descent);
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
		hi->y = posl.y+ascent;
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
	static int bw=5, bh=4, br=2, bb=2;
	static double arrowScale = 0.5;
	long options = d->options;
	POS_T descent;
	/*DrawMultiString( d, pos, text, fp, fs, color, a, &lo, &hi );*/
	if ( fs < 2*d->scale )
		return;
#ifndef WINDOWS
	if ( ( d->options & DC_PRINT) != 0 ) {
		double scale = ((FLOAT_T)fs)/((FLOAT_T)drawMaxTextFontSize)/72.0;
		wPos_t w, h, d;
		wDrawGetTextSize( &w, &h, &d, mainD.d, text, fp, drawMaxTextFontSize );
		size.x = w*scale;
		size.y = h*scale;
		descent = d*scale;
	} else
#endif
		DrawTextSize2( &mainD, text, fp, fs, TRUE, &size, &descent );
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
	size.y += descent;
	p[0] = p0;
	p[0].x -= br*d->scale/d->dpi;
	p[0].y -= bb*d->scale/d->dpi+descent;
	p[1].y = p[0].y;
	p[2].y = p[3].y = p[0].y + size.y;
	p[1].x = p[2].x = p[0].x + size.x;
	p[3].x = p[0].x;
	d->options &= ~DC_DASH;
	switch (style) {
	case BOX_ARROW:
		Translate( &p1, pos, a, size.x+size.y );
		ClipLine( &pos, &p1, p[0], 0.0, size );
		Translate( &p2, p1, a, size.y*arrowScale );
		DrawLine( d, p1, p2, 0, color );
		Translate( &p1, p2, a+150, size.y*0.7*arrowScale );
		DrawLine( d, p1, p2, 0, color );
		Translate( &p1, p2, a-150, size.y*0.7*arrowScale );
		DrawLine( d, p1, p2, 0, color );
	case BOX_BOX:
		DrawLine( d, p[1], p[2], 0, color );
		DrawLine( d, p[2], p[3], 0, color );
		DrawLine( d, p[3], p[0], 0, color );
	case BOX_UNDERLINE:
		DrawLine( d, p[0], p[1], 0, color );
		DrawString( d, p0, 0.0, text, fp, fs, color );
		break;
	case BOX_INVERT:
		DrawFillPoly( d, 4, p, color );
		if ( color != wDrawColorWhite )
			DrawString( d, p0, 0.0, text, fp, fs, wDrawColorWhite );
		break;
	case BOX_BACKGROUND:
		DrawFillPoly( d, 4, p, wDrawColorWhite );
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
		POS_T * descent )
{
	wPos_t w, h, d;
	FLOAT_T scale = 1.0;
	if ( relative )
		fs /= dp->scale;
	if ( fs > drawMaxTextFontSize ) {
		scale = ((FLOAT_T)fs)/((FLOAT_T)drawMaxTextFontSize);
		fs = drawMaxTextFontSize;
	}
	wDrawGetTextSize( &w, &h, &d, dp->d, text, fp, fs );
	size->x = SCALEX(mainD,w)*scale;
	size->y = SCALEY(mainD,h)*scale;
	*descent = SCALEY(mainD,d)*scale;
	if ( relative ) {
		size->x *= dp->scale;
		size->y *= dp->scale;
		*descent *= dp->scale;
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
	POS_T descent;
	DrawTextSize2( dp, text, fp, fs, relative, size, &descent );
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
	POS_T descent, lineW, lineH;
	coOrd textsize, blocksize;

	char *cp;
	char *line = malloc(strlen(text) + 1);

	DrawTextSize2( &mainD, "Aqlip", fp, fs, TRUE, &textsize, &descent);
	POS_T ascent = textsize.y-descent;
	lineH = ascent+descent*1.5;
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
		DrawTextSize2( &mainD, line, fp, fs, TRUE, &textsize, &descent);
		lineW = textsize.x;
		if (lineW>blocksize.x)
			blocksize.x = lineW;
		lastline->x = textsize.x;
		if (*text =='\n') {
			lastline->y -= lineH;
			lastline->x = 0;
		}
		if (*text == '\0')
			break;
		text++;
	}
	size->x = blocksize.x;
	size->y = blocksize.y;
	free(line);
}


static void DDrawBitMap( drawCmd_p d, coOrd p, wDrawBitMap_p bm, wDrawColor color)
{
	wPos_t x, y;
#ifdef LATER
	if (d->options&DC_TEMPSEGS) {
		return;
	}
	if (d->options&DC_PRINT)
		return;
#endif
	d->CoOrd2Pix( d, p, &x, &y );
	wDrawBitMap( d->d, bm, x, y, color, (wDrawOpts)d->funcs->options );
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
	tempSegs(tempSegs_da.cnt-1).width = 0;
	tempSegs(tempSegs_da.cnt-1).u.t.pos = p;
	tempSegs(tempSegs_da.cnt-1).u.t.angle = a;
	tempSegs(tempSegs_da.cnt-1).u.t.fontP = fp;
	tempSegs(tempSegs_da.cnt-1).u.t.fontSize = fontSize;
	tempSegs(tempSegs_da.cnt-1).u.t.string = MyStrdup(s);
}


static void TempSegFillPoly(
		drawCmd_p d,
		int cnt,
		coOrd * pts,
		wDrawColor color )
{
#ifdef LATER
	pts is not guaranteed to valid
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
	tempSegs(tempSegs_da.cnt-1).type = SEG_FILPOLY;
	tempSegs(tempSegs_da.cnt-1).color = color;
	tempSegs(tempSegs_da.cnt-1).width = 0;
	tempSegs(tempSegs_da.cnt-1).u.p.cnt = cnt;
	tempSegs(tempSegs_da.cnt-1).u.p.pts = pts;
#endif
	return;
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


static void NoDrawBitMap( drawCmd_p d, coOrd p, wDrawBitMap_p bm, wDrawColor color )
{
}



EXPORT drawFuncs_t screenDrawFuncs = {
		0,
		DDrawLine,
		DDrawArc,
		DDrawString,
		DDrawBitMap,
		DDrawFillPoly,
		DDrawFillCircle };

EXPORT drawFuncs_t tempDrawFuncs = {
		wDrawOptTemp,
		DDrawLine,
		DDrawArc,
		DDrawString,
		DDrawBitMap,
		DDrawFillPoly,
		DDrawFillCircle };

EXPORT drawFuncs_t printDrawFuncs = {
		0,
		DDrawLine,
		DDrawArc,
		DDrawString,
		NoDrawBitMap,
		DDrawFillPoly,
		DDrawFillCircle };

EXPORT drawFuncs_t tempSegDrawFuncs = {
		0,
		TempSegLine,
		TempSegArc,
		TempSegString,
		NoDrawBitMap,
		TempSegFillPoly,
		TempSegFillCircle };

EXPORT drawCmd_t mainD = {
		NULL, &screenDrawFuncs, DC_TICKS, INIT_MAIN_SCALE, 0.0, {0.0,0.0}, {0.0,0.0}, MainPix2CoOrd, MainCoOrd2Pix };

EXPORT drawCmd_t tempD = {
		NULL, &tempDrawFuncs, DC_TICKS|DC_SIMPLE, INIT_MAIN_SCALE, 0.0, {0.0,0.0}, {0.0,0.0}, MainPix2CoOrd, MainCoOrd2Pix };

EXPORT drawCmd_t mapD = {
		NULL, &screenDrawFuncs, 0, INIT_MAP_SCALE, 0.0, {0.0,0.0}, {96.0,48.0}, Pix2CoOrd, CoOrd2Pix };


/*****************************************************************************
 *
 * MAIN AND MAP WINDOW DEFINTIONS
 *
 */


static wPos_t info_yb_offset = 2;
static wPos_t info_ym_offset = 3;
static wPos_t six = 2;
static wPos_t info_xm_offset = 2;
static wPos_t messageOrControlX = 0;
static wPos_t messageOrControlY = 0;
#define NUM_INFOCTL				(4)
static wControl_p curInfoControl[NUM_INFOCTL];
static wPos_t curInfoLabelWidth[NUM_INFOCTL];

/**
 * Determine the width of a mouse pointer position string ( coordinate plus label ).
 *
 * \return width of position string
 */
static wPos_t GetInfoPosWidth( void )
{
	wPos_t labelWidth;
	
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
	wPos_t width, height, y, yb, ym, x, boxH;
	wWinGetSize( mainW, &width, &height );
	infoHeight = 3 + wStatusGetHeight( COMBOBOX ) + 3;
	textHeight = wStatusGetHeight(0L);
	y = height - max(infoHeight,textHeight)-10;

#ifdef WINDOWS
	y -= 19; /* Kludge for MSW */
#endif

	infoD.pos_w = GetInfoPosWidth() + 2;
	infoD.scale_w = wStatusGetWidth( "999:1" ) + wStatusGetWidth( zoomLabel ) + 6;
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
		x += infoD.scale_w + 10;
		infoD.posX_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.pos_w, boxH );
		infoD.posX_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarPosX", infoD.pos_w-six, xLabel );
		x += infoD.pos_w + 5;
		infoD.posY_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.pos_w, boxH );
		infoD.posY_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarPosY", infoD.pos_w-six, yLabel );
		x += infoD.pos_w + 10;
		messageOrControlX = x+info_xm_offset;									//Remember Position
		messageOrControlY = ym;
		infoD.info_b = wBoxCreate( mainW, x, yb, NULL, wBoxBelow, infoD.info_w, boxH );
		infoD.info_m = wStatusCreate( mainW, x+info_xm_offset, ym, "infoBarStatus", infoD.info_w-six, "" );
}



static void SetInfoBar( void )
{
	wPos_t width, height, y, yb, ym, x, boxH;
	int inx;
	static long oldDistanceFormat = -1;
	long newDistanceFormat;
	wWinGetSize( mainW, &width, &height );
	y = height - max(infoHeight,textHeight)-10;
	newDistanceFormat = GetDistanceFormat();
	if ( newDistanceFormat != oldDistanceFormat ) {
		infoD.pos_w = GetInfoPosWidth() + 2;
		wBoxSetSize( infoD.posX_b, infoD.pos_w, infoHeight-5 );
		wStatusSetWidth( infoD.posX_m, infoD.pos_w-six );
		wBoxSetSize( infoD.posY_b, infoD.pos_w, infoHeight-5 );
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
				int y_this = ym + (textHeight/2) - (wControlGetHeight( curInfoControl[inx] )/2);
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
	DrawMarkers();
	sprintf( message, "%s%s", xLabel, FormatDistance(pos.x) );
	wStatusSetValue( infoD.posX_m, message );
	sprintf( message, "%s%s", yLabel, FormatDistance(pos.y) );
	wStatusSetValue( infoD.posY_m, message );
	oldMarker = pos;
	DrawMarkers();
}

static wControl_p deferSubstituteControls[NUM_INFOCTL+1];
static char * deferSubstituteLabels[NUM_INFOCTL];

EXPORT void InfoSubstituteControls(
		wControl_p * controls,
		char ** labels )
{
	wPos_t x, y;
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
		int	y_this = y + (textHeight/2) - (wControlGetHeight( controls[inx] )/2);
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


static void ChangeMapScale( void )
{
	wPos_t w, h;
	wPos_t dw, dh;
	FLOAT_T fw, fh;

	wGetDisplaySize( &dw, &dh );
	dw /= 2;
	dh /= 2;
	fw = ((mapD.size.x/mapD.scale)*mapD.dpi + 0.5)+2;
	fh = ((mapD.size.y/mapD.scale)*mapD.dpi + 0.5)+2;
	if (fw > dw || fh > dh) {
		if (fw/dw > fh/dh) {
			mapD.scale = ceil(mapD.size.x*mapD.dpi/dw);
		} else {
			mapD.scale = ceil(mapD.size.y*mapD.dpi/dh);
		}
		mapScale = (long)mapD.scale;
		fw = ((mapD.size.x/mapD.scale)*mapD.dpi + 0.5)+2;
		fh = ((mapD.size.y/mapD.scale)*mapD.dpi + 0.5)+2;
	} else if ( fw < 100.0 && fh < 100.0 ) {
		if (fw > fh) {
			mapD.scale = ceil(mapD.size.x*mapD.dpi/100);
		} else {
			mapD.scale = ceil(mapD.size.y*mapD.dpi/100);
		}
		mapScale = (long)mapD.scale;
		fw = ((mapD.size.x/mapD.scale)*mapD.dpi + 0.5)+2;
		fh = ((mapD.size.y/mapD.scale)*mapD.dpi + 0.5)+2;
	}
	w = (wPos_t)fw;
	h = (wPos_t)fh;
	wWinSetSize( mapW, w+DlgSepLeft+DlgSepRight, h+DlgSepTop+DlgSepBottom );
	wDrawSetSize( mapD.d, w, h );
}


EXPORT BOOL_T SetRoomSize( coOrd size )
{
	if (size.x < 12.0)
		size.x = 12.0;
	if (size.y < 12.0)
		size.y = 12.0;
	if ( mapD.size.x == size.x &&
		 mapD.size.y == size.y )
		return TRUE;
	mapD.size = size;
	if ( mapW == NULL)
		return TRUE;
	ChangeMapScale();
	ConstraintOrig( &mainD.orig, mainD.size );
	tempD.orig = mainD.orig;
	/*MainRedraw();*/
	wPrefSetFloat( "draw", "roomsizeX", mapD.size.x );
	wPrefSetFloat( "draw", "roomsizeY", mapD.size.y );
	return TRUE;
}


EXPORT void GetRoomSize( coOrd * froomSize )
{
	*froomSize = mapD.size;
}


EXPORT void MapRedraw( void )
{
	if (inPlaybackQuit)
		return;
#ifdef VERBOSE
lprintf("MapRedraw\n");
#endif
	if (!mapVisible)
		return;

	if (delayUpdate)
	wDrawDelayUpdate( mapD.d, TRUE );
	wSetCursor( wCursorWait );
	wDrawClear( mapD.d );
	DrawTracks( &mapD, mapD.scale, mapD.orig, mapD.size );
	DrawMapBoundingBox( TRUE );
	wSetCursor( wCursorNormal );
	wDrawDelayUpdate( mapD.d, FALSE );
}


static void MapResize( void )
{
	mapD.scale = mapScale;
	ChangeMapScale();
	MapRedraw();
}

#ifdef LATER
static void MapProc( wWin_p win, winProcEvent e, void * data )
{
	switch( e ) {
	case wResize_e:
		if (mapD.d == NULL)
			return;
		DrawMapBoundingBox( FALSE );
		ChangeMapScale();
		break;
	case wClose_e:
		mapVisible = FALSE;
		break;
	/*case wRedraw_e:
		if (mapD.d == NULL)
			break;
		MapRedraw();
		break;*/
	default:
		break;
	}
}
#endif


EXPORT void SetMainSize( void )
{
	wPos_t ww, hh;
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


EXPORT void MainRedraw( void )
{
#ifdef LATER
   wPos_t ww, hh;
   DIST_T w, h;
#endif

	coOrd orig, size;
	DIST_T t1;
	if (inPlaybackQuit)
		return;
#ifdef VERBOSE
lprintf("mainRedraw\n");
#endif

	wSetCursor( wCursorWait );
	if (delayUpdate)
	wDrawDelayUpdate( mainD.d, TRUE );
#ifdef LATER
	wDrawGetSize( mainD.d, &ww, &hh );
	w = ww/mainD.dpi;
	h = hh/mainD.dpi;
#endif
	SetMainSize();
#ifdef LATER
	/*wDrawClip( mainD.d, 0, 0, w, h );*/
#endif
	t1 = mainD.dpi/mainD.scale;
	if (units == UNITS_ENGLISH) {
		t1 /= 2.0;
		for ( pixelBins=0.25; pixelBins<t1; pixelBins*=2.0 );
	} else {
		pixelBins = 50.8;
		if (pixelBins >= t1)
		  while (1) {
			if ( pixelBins <= t1 )
				break;
			pixelBins /= 2.0;
			if ( pixelBins <= t1 )
				break;
			pixelBins /= 2.5;
			if ( pixelBins <= t1 )
				break;
			pixelBins /= 2.0;
		}
	}
	ConstraintOrig( &mainD.orig, mainD.size );
	tempD.orig = mainD.orig;
	wDrawClear( mainD.d );
	currRedraw++;
	DrawSnapGrid( &tempD, mapD.size, TRUE );
	DrawRoomWalls( TRUE );
	orig = mainD.orig;
	size = mainD.size;
	orig.x -= RBORDER/mainD.dpi*mainD.scale;
	orig.y -= BBORDER/mainD.dpi*mainD.scale;
	size.x += (RBORDER+LBORDER)/mainD.dpi*mainD.scale;
	size.y += (BBORDER+TBORDER)/mainD.dpi*mainD.scale;
	DrawTracks( &mainD, mainD.scale, orig, size );
	RulerRedraw( FALSE );
	DoCurCommand( C_REDRAW, zero );
	DrawMarkers();
	wSetCursor( wCursorNormal );
	InfoScale();
	wDrawDelayUpdate( mainD.d, FALSE );
}

/**
 * The wlib event handler for the main window.
 * 
 * \param win wlib window information
 * \param e the wlib event
 * \param data additional data (unused)
 */

void MainProc( wWin_p win, winProcEvent e, void * data )
{
	wPos_t width, height;
	switch( e ) {
	case wResize_e:
		if (mainD.d == NULL)
			return;
		DrawMapBoundingBox( FALSE );
		wWinGetSize( mainW, &width, &height );
		LayoutToolBar();
		height -= (toolbarHeight+max(infoHeight,textHeight)+10);
		if (height >= 0) {
			wDrawSetSize( mainD.d, width-20, height );
			wControlSetPos( (wControl_p)mainD.d, 0, toolbarHeight );
			SetMainSize();
			ConstraintOrig( &mainD.orig, mainD.size );
			tempD.orig = mainD.orig;
			SetInfoBar();
			MainRedraw();
			MapRedraw();
			wPrefSetInteger( "draw", "mainwidth", width );
			wPrefSetInteger( "draw", "mainheight", height );
		} else	DrawMapBoundingBox( TRUE );
		break;
	case wState_e:
		wPrefSetInteger( "draw", "maximized", wWinIsMaximized(win) );
		break;
	case wQuit_e:
		CleanupCustom();
		break;
	case wClose_e:
		/* shutdown the application via "close window"  button  */
		DoQuit();
		break;
	default:
		break;
	}
}


#ifdef WINDOWS
int profRedraw = 0;
void 
#ifndef WIN32
_far _pascal 
#endif
ProfStart( void );
void 
#ifndef WIN32
_far _pascal 
#endif
ProfStop( void );
#endif

EXPORT void DoRedraw( void )
{
#ifdef WINDOWS
#ifndef WIN32
	if (profRedraw)
		ProfStart();
#endif
#endif
	MapRedraw();
	MainRedraw();
#ifdef WINDOWS
#ifndef WIN32
	if (profRedraw)
		ProfStop();
#endif
#endif


}

/*****************************************************************************
 *
 * RULERS and OTHER DECORATIONS
 *
 */


static void DrawRoomWalls( wBool_t t )
{
	coOrd p01, p11, p10;

	if (mainD.d == NULL)
		return;
#ifdef LATER
	wDrawGetDim( mainD.d, &w, &h );
#endif
	DrawTicks( &mainD, mapD.size );

	p01.x = p10.y = 0.0;
	p11.x = p10.x = mapD.size.x;
	p01.y = p11.y = mapD.size.y;
	DrawLine( &mainD, p01, p11, 3, t?borderColor:wDrawColorWhite );
	DrawLine( &mainD, p11, p10, 3, t?borderColor:wDrawColorWhite );
#ifdef LATER
	/*wDrawClip( mainD.d, LBORDER, BBORDER,
			   w-(LBORDER+RBORDER), h-(BBORDER+TBORDER) );*/
#endif
}


EXPORT void DrawMarkers( void )
{
	wPos_t x, y;
	mainD.CoOrd2Pix(&mainD,oldMarker,&x,&y);
	wDrawLine( mainD.d, 0, y, (wPos_t)LBORDER, y,
				0, wDrawLineSolid, markerColor, wDrawOptTemp );
	wDrawLine( mainD.d, x, 0, x, (wPos_t)BBORDER,
				0, wDrawLineSolid, markerColor, wDrawOptTemp );
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
	wPos_t len;
	int digit;
	char quote;
	char message[10];
	coOrd d_orig, d_size;
	wFontSize_t fs;
	long mm, mm0, mm1, power;
	wPos_t x0, y0, x1, y1;
	long dxn, dyn;
	static int lengths[8] = {
		0, 2, 4, 2, 6, 2, 4, 2 };
	int fraction, incr, firstFraction, lastFraction;
	int majorLength;
	coOrd p0, p1;
	FLOAT_T sin_aa;

	a = FindAngle( pos0, pos1 );
	Translate( &pos0, pos0, a, offset );
	Translate( &pos1, pos1, a, offset );
	aa = NormalizeAngle(a+(tickSide==0?+90:-90));
	if (aa > 90.0 && aa < 270.0) {
#ifdef WINDOWS
		dyn = -17;
#else
		dyn = -12;
#endif
	} else {
		dyn = +3;
	}
	sin_aa = sin(D2R(aa));
	dxn = (long)floor(10.0*sin_aa);
	end = FindDistance( pos0, pos1 );
	if (end < 0.1)
		return;
	d_orig.x = d->orig.x - 0.001;
	d_orig.y = d->orig.y - 0.001;
	d_size.x = d->size.x + 0.002;
	d_size.y = d->size.y + 0.002;
	if (!ClipLine( &pos0, &pos1, d_orig, d->angle, d_size ))
		return;

	start = FindDistance( orig, pos0 );
	if (offset < 0)
		start = -start;
	end = FindDistance( orig, pos1 );

	d->CoOrd2Pix( d, pos0, &x0, &y0 );
	d->CoOrd2Pix( d, pos1, &x1, &y1 );
	wDrawLine( d->d, x0, y0, x1, y1,
				0, wDrawLineSolid, color, (wDrawOpts)d->funcs->options );

	if (units == UNITS_METRIC) {
		mm0 = (int)ceil(start*25.4-0.5);
		mm1 = (int)floor(end*25.4+0.5);
		len = 2;
		if (d->scale <= 1) {
			power = 1;
		} else if (d->scale <= 8) {
			power = 10;
		} else if (d->scale <= 32) {
			power = 100;
		} else {
			power = 1000;
		}
		for ( ; power<=1000; power*=10,len+=3 ) {
			if (power == 1000)
				len = 10;
			for (mm=((mm0+(mm0>0?power-1:0))/power)*power; mm<=mm1; mm+=power) {
				if (power==1000 || mm%(power*10) != 0) {
					Translate( &p0, orig, a, mm/25.4 );
					Translate( &p1, p0, aa, len*d->scale/mainD.dpi );
					d->CoOrd2Pix( d, p0, &x0, &y0 );
					d->CoOrd2Pix( d, p1, &x1, &y1 );
					wDrawLine( d->d, x0, y0, x1, y1,
							0, wDrawLineSolid, color, (wDrawOpts)d->funcs->options );

					if (!number)
						continue;
					if ( (power>=1000) ||
						 (d->scale<=8 && power>=100) ||
						 (d->scale<=1 && power>=10) ) {
						if (mm%100 != 0) {
							sprintf(message, "%ld", mm/10%10 );
							fs = rulerFontSize*2/3;
							p0.x = p1.x+4*dxn/10*d->scale/mainD.dpi;
							p0.y = p1.y+dyn*d->scale/mainD.dpi;
						} else {
							sprintf(message, "%0.1f", mm/1000.0 );
							fs = rulerFontSize;
							p0.x = p0.x+((-(LBORDER-2)/2)+((LBORDER-2)/2+2)*sin_aa)*d->scale/mainD.dpi;
							p0.y = p1.y+dyn*d->scale/mainD.dpi;
						}
						d->CoOrd2Pix( d, p0, &x0, &y0 );
						wDrawString( d->d, x0, y0, d->angle, message, rulerFp,
										fs, color, (wDrawOpts)d->funcs->options );
					}
				}
			}
		}
	} else {
		if (d->scale <= 1)
			incr = 1;
		else if (d->scale <= 2)
			incr = 2;
		else if (d->scale <= 4)
			incr = 4;
		else
			incr = 8;
		lastInch = (int)floor(end);
		lastFraction = 7;
		inch = (int)ceil(start);
		firstFraction = (((int)((inch-start)*8/*+1*/)) / incr) * incr;
		if (firstFraction > 0) {
			inch--;
			firstFraction = 8 - firstFraction;
		}
		for ( ; inch<=lastInch; inch++){
			if (inch % 12 == 0) {
				lengths[0] = 10;
				majorLength = 16;
				digit = (int)(inch/12);
				fs = rulerFontSize;
				quote = '\'';
			} else if (d->scale <= 8) {
				lengths[0] = 8;
				majorLength = 13;
				digit = (int)(inch%12);
				fs = rulerFontSize*(2.0/3.0);
				quote = '"';
			} else {
				continue;
			}
			if (inch == lastInch)
				lastFraction = (((int)((end - lastInch)*8)) / incr) * incr;
			for ( fraction = firstFraction; fraction<=lastFraction; fraction += incr ) {
				Translate( &p0, orig, a, inch+fraction/8.0 );
				Translate( &p1, p0, aa, lengths[fraction]*d->scale/72.0 );
				d->CoOrd2Pix( d, p0, &x0, &y0 );
				d->CoOrd2Pix( d, p1, &x1, &y1 );
				wDrawLine( d->d, x0, y0, x1, y1,
						0, wDrawLineSolid, color,
						(wDrawOpts)d->funcs->options );
#ifdef KLUDGEWINDOWS
		/* KLUDGE: can't draw invertable strings on windows */
			if ( (opts&DO_TEMP) == 0)
#endif
			if ( fraction == 0 && number == TRUE) {
				if (inch % 12 == 0 || d->scale <= 2) {
					Translate( &p0, p0, aa, majorLength*d->scale/72.0 );
					Translate( &p0, p0, 225, 11*d->scale/72.0 );
					sprintf(message, "%d%c", digit, quote );
					d->CoOrd2Pix( d, p0, &x0, &y0 );
					wDrawString( d->d, x0, y0, d->angle, message, rulerFp, fs, color, (wDrawOpts)d->funcs->options );
				}
			}
			firstFraction = 0;
			}
		}
	}
}


EXPORT void DrawTicks( drawCmd_p d, coOrd size )
{
	coOrd p0, p1;
	DIST_T offset;

	offset = 0.0;
	if ( d->orig.x<0.0 )
		offset = d->orig.x;
	p0.x = 0.0/*d->orig.x*/; p1.x = size.x;
	p0.y = p1.y = /*max(d->orig.y,0.0)*/ d->orig.y;
	DrawRuler( d, p0, p1, offset, TRUE, FALSE, borderColor );
	p0.y = p1.y = min(d->orig.y + d->size.y, size.y);
	DrawRuler( d, p0, p1, offset, FALSE, TRUE, borderColor );
	offset = 0.0;
	if ( d->orig.y<0.0 )
		offset = d->orig.y;
	p0.y = 0.0/*d->orig.y*/; p1.y = max(size.y,0.0);
	p0.x = p1.x = d->orig.x;
	DrawRuler( d, p0, p1, offset, TRUE, TRUE, borderColor );
	p0.x = p1.x = min(d->orig.x + d->size.x, size.x);
	DrawRuler( d, p0, p1, offset, FALSE, FALSE, borderColor );
}

/*****************************************************************************
 *
 * ZOOM and PAN
 *
 */

EXPORT coOrd mainCenter;


EXPORT void DrawMapBoundingBox( BOOL_T set )
{
	if (mainD.d == NULL || mapD.d == NULL)
		return;
	DrawHilight( &mapD, mainD.orig, mainD.size );
}


static void ConstraintOrig( coOrd * orig, coOrd size )
{
LOG( log_pan, 2, ( "ConstraintOrig [ %0.3f, %0.3f ] RoomSize(%0.3f %0.3f), WxH=%0.3fx%0.3f",
				orig->x, orig->y, mapD.size.x, mapD.size.y,
				size.x, size.y ) )
	if (orig->x+size.x > mapD.size.x ) {
		orig->x = mapD.size.x-size.x;
		orig->x += (units==UNITS_ENGLISH?1.0:(1.0/2.54));
	}
	if (orig->x < 0)
		orig->x = 0;
	if (orig->y+size.y > mapD.size.y ) {
		orig->y = mapD.size.y-size.y;
		orig->y += (units==UNITS_ENGLISH?1.0:1.0/2.54);
				
	}
	if (orig->y < 0)
		orig->y = 0;
	if (mainD.scale >= 1.0) {
		if (units == UNITS_ENGLISH) {
			orig->x = floor(orig->x*4)/4;   //>1:1 = 1/4 inch
			orig->y = floor(orig->y*4)/4;
		} else {
			orig->x = floor(orig->x*2.54*2)/(2.54*2);  //>1:1 = 0.5 cm
			orig->y = floor(orig->y*2.54*2)/(2.54*2);
		}
	} else {
		if (units == UNITS_ENGLISH) {
			orig->x = floor(orig->x*64)/64;   //<1:1 = 1/64 inch
			orig->y = floor(orig->y*64)/64;
		} else {
			orig->x = floor(orig->x*25.4*2)/(25.4*2);  //>1:1 = 0.5 mm
			orig->y = floor(orig->y*25.4*2)/(25.4*2);
		}
	}
	orig->x = (long)(orig->x*pixelBins+0.5)/pixelBins;
	orig->y = (long)(orig->y*pixelBins+0.5)/pixelBins;
LOG( log_pan, 2, ( " = [ %0.3f %0.3f ]\n", orig->y, orig->y ) )
}

/**
 * Initialize the menu for setting zoom factors. 
 * 
 * \param IN zoomM			Menu to which radio button is added
 * \param IN zoomSubM	Second menu to which radio button is added, ignored if NULL
 *
 */

EXPORT void InitCmdZoom( wMenu_p zoomM, wMenu_p zoomSubM )
{
	int inx;
	
	for ( inx=0; inx<sizeof zoomList/sizeof zoomList[0]; inx++ ) {
		if( zoomList[ inx ].value >= 1.0 ) {
			zoomList[inx].btRadio = wMenuRadioCreate( zoomM, "cmdZoom", zoomList[inx].name, 0, (wMenuCallBack_p)DoZoom, (void *)(&(zoomList[inx].value)));
			if( zoomSubM )
				zoomList[inx].pdRadio = wMenuRadioCreate( zoomSubM, "cmdZoom", zoomList[inx].name, 0, (wMenuCallBack_p)DoZoom, (void *)(&(zoomList[inx].value)));
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
	
	for ( inx=0; inx<sizeof zoomList/sizeof zoomList[0]; inx++ ) {
		if( curScale == zoomList[inx].value ) {
		
			wMenuRadioSetActive( zoomList[inx].btRadio );		
			if( zoomList[inx].pdRadio )
				wMenuRadioSetActive( zoomList[inx].pdRadio );

			/* activate / deactivate zoom buttons when appropriate */				
			wControlLinkedActive( (wControl_p)zoomUpB, ( inx != 0 ) );
			wControlLinkedActive( (wControl_p)zoomDownB, ( inx < (sizeof zoomList/sizeof zoomList[0] - 1)));			
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
	
	for ( inx=0; inx<sizeof zoomList/sizeof zoomList[0]; inx++ ) {
		if( scale == zoomList[inx].value ) {
			return inx;
		}	
	}
	return -1;	
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

	DrawHilight( &mapD, mainD.orig, mainD.size );
	tempD.scale = mainD.scale = scale;
	mainD.dpi = wDrawGetDPI( mainD.d );
	if ( mainD.dpi == 75 ) {
		mainD.dpi = 72.0;
	} else if ( scale > 1.0 && scale <= 12.0 ) {
		mainD.dpi = floor( (mainD.dpi + scale/2)/scale) * scale;
	}
	tempD.dpi = mainD.dpi;

	SetZoomRadio( scale ); 
	InfoScale();
	SetMainSize(); 
	if (zoomCorner) {
		mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
		mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
	} else {
		mainD.orig.x = mainCenter.x - mainD.size.x/2.0;
		mainD.orig.y = mainCenter.y - mainD.size.y/2.0;
	}
	ConstraintOrig( &mainD.orig, mainD.size );
	MainRedraw();
	MapRedraw();
	tempD.orig = mainD.orig;
LOG( log_zoom, 1, ( "center = [%0.3f %0.3f]\n", mainCenter.x, mainCenter.y ) )
	/*SetFont(0);*/
	sprintf( tmp, "%0.3f", mainD.scale );
	wPrefSetString( "draw", "zoom", tmp );
	//DrawHilight( &mapD, mainD.orig, mainD.size );
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
	
	if ( mode != NULL || (MyGetKeyState()&WKEY_SHIFT) == 0 ) {
		i = ScaleInx( mainD.scale );
		/* 
		 * Zooming into macro mode happens when we are at scale 1:1. 
		 * To jump into macro mode, the CTRL-key has to be pressed and held.
		 */
		if( mainD.scale != 1.0 || (mainD.scale == 1.0 && (MyGetKeyState()&WKEY_CTRL))) {
			if( i ) {
				if (mainD.scale <=1.0) 
					InfoMessage(_("Macro Zoom Mode"));
				else 
					InfoMessage(_("Use Shift+PageDwn to jump to preset Zoom In"));
				DoNewScale( zoomList[ i - 1 ].value );	
				
			} else InfoMessage("Min Macro Zoom");
		} else {
			InfoMessage(_("Scale 1:1 - Use Ctrl+PageDwn to go to Macro Zoom Mode"));
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


/**
 * User selected zoom out, via mouse wheel, button or pulldown.
 *
 * \param mode IN FALSE if zoom button was activated, TRUE if activated via popup or mousewheel
 */

EXPORT void DoZoomDown( void  * mode)
{
	long newScale;
	int i;
	
	if ( mode != NULL || (MyGetKeyState()&WKEY_SHIFT) == 0 ) {
		i = ScaleInx( mainD.scale );
		if( i>= 0 && i < ( sizeof zoomList/sizeof zoomList[0] - 1 )) {
			InfoMessage(_("Use Shift+PageUp to jump to preset Zoom Out"));
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

EXPORT void DoZoom( DIST_T *pScale )
{
	DIST_T scale = *pScale;

	if( scale != mainD.scale )
		DoNewScale( scale );
}



EXPORT void Pix2CoOrd(
		drawCmd_p d,
		wPos_t x,
		wPos_t y,
		coOrd * pos )
{
	pos->x = (((DIST_T)x)/d->dpi)*d->scale+d->orig.x;
	pos->y = (((DIST_T)y)/d->dpi)*d->scale+d->orig.y;
}

EXPORT void CoOrd2Pix(
		drawCmd_p d,
		coOrd pos,
		wPos_t * x,
		wPos_t * y )
{
	*x = (wPos_t)((pos.x-d->orig.x)/d->scale*d->dpi);
	*y = (wPos_t)((pos.y-d->orig.y)/d->scale*d->dpi);
}


static void DoMapPan( wAction_t action, coOrd pos )
{
	static coOrd mapOrig;
	static coOrd oldOrig, newOrig;
	static coOrd size;
	static DIST_T xscale, yscale;
	static enum { noPan, movePan, resizePan } mode = noPan;
	wPos_t x, y;

	switch (action & 0xFF) {

	case C_DOWN:
		if ( mode == noPan )
			mode = movePan;
		else
			break;
		mapOrig = pos;
		size = mainD.size;
		newOrig = oldOrig = mainD.orig;
LOG( log_pan, 1, ( "ORIG = [ %0.3f, %0.3f ]\n", mapOrig.x, mapOrig.y ) )
		break;
	case C_MOVE:
		if ( mode != movePan )
			break;
		DrawHilight( &mapD, newOrig, size );
LOG( log_pan, 2, ( "NEW = [ %0.3f, %0.3f ] \n", pos.x, pos.y ) )
		newOrig.x = oldOrig.x + pos.x-mapOrig.x;
		newOrig.y = oldOrig.y + pos.y-mapOrig.y;
		ConstraintOrig( &newOrig, mainD.size );
		if (liveMap) {
			tempD.orig = mainD.orig = newOrig;
			MainRedraw();
		}
		DrawHilight( &mapD, newOrig, size );
		break;
	case C_UP:
		if ( mode != movePan )
			break;
		tempD.orig = mainD.orig = newOrig;
		mainCenter.x = newOrig.x + mainD.size.x/2.0;
		mainCenter.y = newOrig.y + mainD.size.y/2.0;
		if (!liveMap)
			MainRedraw();
LOG( log_pan, 1, ( "FINAL = [ %0.3f, %0.3f ]\n", pos.x, pos.y ) )
#ifdef LATER
		if (recordF) {
			fprintf( recordF, "ORIG %0.3f %0.3f %0.3f\n",
						mainD.scale, mainD.orig.x, mainD.orig.y );
		}
#endif
		mode = noPan;
		break;

	case C_RDOWN:
		if ( mode == noPan )
			mode = resizePan;
		else
			break;
		DrawHilight( &mapD, mainD.orig, mainD.size );
		newOrig = pos;
		oldOrig = newOrig;
#ifdef LATER
		xscale = INIT_MAP_SCALE;
		size.x = mapD.size.x/xscale;
		size.y = mapD.size.y/xscale;
#endif
		xscale = 1;
		size.x = mainD.size.x/mainD.scale;
		size.y = mainD.size.y/mainD.scale;
		newOrig.x -= size.x/2.0;
		newOrig.y -= size.y/2.0;
		DrawHilight( &mapD, newOrig, size );
		break;

	case C_RMOVE:
		if ( mode != resizePan )
			break;
		DrawHilight( &mapD, newOrig, size );
		if (pos.x < 0)
			pos.x = 0;
		if (pos.x > mapD.size.x)
			pos.x = mapD.size.x;
		if (pos.y < 0)
			pos.y = 0;
		if (pos.y > mapD.size.y)
			pos.y = mapD.size.y;
		size.x = (pos.x - oldOrig.x)*2.0;
		size.y = (pos.y - oldOrig.y)*2.0;
		if (size.x < 0) {
			size.x = - size.x;
		}
		if (size.y < 0) {
			size.y = - size.y;
		}
		xscale = size.x / (mainD.size.x/mainD.scale);
		yscale = size.y / (mainD.size.y/mainD.scale);
		if (xscale < yscale)
			xscale = yscale;
		xscale = ceil( xscale );
		if (xscale < 1)
			xscale = 1;
		if (xscale > 64)
			xscale = 64;
		size.x = (mainD.size.x/mainD.scale) * xscale;
		size.y = (mainD.size.y/mainD.scale) * xscale;
		newOrig = oldOrig;
		newOrig.x -= size.x/2.0;
		newOrig.y -= size.y/2.0;
		DrawHilight( &mapD, newOrig, size );
		break;

	case C_RUP:
		if ( mode != resizePan )
			break;
		tempD.size = mainD.size = size;
		tempD.orig = mainD.orig = newOrig;
		mainCenter.x = newOrig.x + mainD.size.x/2.0;
		mainCenter.y = newOrig.y + mainD.size.y/2.0;
		DoNewScale( xscale );
		mode = noPan;
		break;

		case wActionExtKey:
			mainD.CoOrd2Pix(&mainD,pos,&x,&y);
			switch ((wAccelKey_e)(action>>8)) {
#ifndef WINDOWS
			case wAccelKey_Pgdn:
				DoZoomUp(NULL);
				return;
			case wAccelKey_Pgup:
				DoZoomDown(NULL);
				return;
			case wAccelKey_F5:
				MainRedraw();
				MapRedraw();
				return;
#endif
			case wAccelKey_Right:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				mainD.orig.x += mainD.size.x/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			case wAccelKey_Left:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				mainD.orig.x -= mainD.size.x/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			case wAccelKey_Up:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				mainD.orig.y += mainD.size.y/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			case wAccelKey_Down:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				mainD.orig.y -= mainD.size.y/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			default:
				return;
			}
			mainD.Pix2CoOrd( &mainD, x, y, &pos );
			InfoPos( pos );
			return;
	default:
		return;
	}
}


EXPORT BOOL_T IsClose(
		DIST_T d )
{
	wPos_t pd;
	pd = (wPos_t)(d/mainD.scale * mainD.dpi);
	return pd <= closePixels;
}

/*****************************************************************************
 *
 * MAIN MOUSE HANDLER
 *
 */

static int ignoreMoves = 1;

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
GetMousePosition( int *x, int *y )
{
	if( x && y ) {
		*x = mousePositionx;
		*y = mousePositiony;
	}
}

static void DoMouse( wAction_t action, coOrd pos )
{

	BOOL_T rc;
	wPos_t x, y;
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
					(WKEY_SHIFT | WKEY_CTRL)) == (WKEY_SHIFT | WKEY_CTRL)) break;  //Allow SHIFT+CTRL for Move
			if (((action>>8)&0xFF) == wAccelKey_LineFeed) {
				action = C_TEXT+((int)(0x0A<<8));
				break;
			}
			switch ((wAccelKey_e)(action>>8)) {
			case wAccelKey_Del:
				SelectDelete();
				return;
#ifndef WINDOWS
			case wAccelKey_Pgdn:
				DoZoomUp(NULL);
				break;
			case wAccelKey_Pgup:
				DoZoomDown(NULL);
				break;
#endif
			case wAccelKey_Right:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				if ((MyGetKeyState() & WKEY_SHIFT) != 0)
					mainD.orig.x += 0.25*mainD.scale;    //~1cm in 1::1, 1ft in 30:1, 1mm in 10:1
				else
					mainD.orig.x += mainD.size.x/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			case wAccelKey_Left:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				if ((MyGetKeyState() & WKEY_SHIFT) != 0)
					mainD.orig.x -= 0.25*mainD.scale;
				else
					mainD.orig.x -= mainD.size.x/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			case wAccelKey_Up:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				if ((MyGetKeyState() & WKEY_SHIFT) != 0)
					mainD.orig.y += 0.25*mainD.scale;
				else
					mainD.orig.y += mainD.size.x/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			case wAccelKey_Down:
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				if ((MyGetKeyState() & WKEY_SHIFT) != 0)
					mainD.orig.y -= 0.25*mainD.scale;
				else
					mainD.orig.y -= mainD.size.x/2;
				ConstraintOrig( &mainD.orig, mainD.size );
				mainCenter.x = mainD.orig.x + mainD.size.x/2.0;
				mainCenter.y = mainD.orig.y + mainD.size.y/2.0;
				MainRedraw();
				MapRedraw();
				//DrawHilight( &mapD, mainD.orig, mainD.size );
				break;
			default:
				return;
			}
			mainD.Pix2CoOrd( &mainD, x, y, &pos );
			InfoPos( pos );
			return;
		case C_TEXT:
			if ((action>>8) == 0x0D) {
				action = C_OK;
			} else if ((action>>8) == 0x1B) {
				ConfirmReset( TRUE );
				return;
			}
		case C_MOVE:
		case C_UP:
		case C_RMOVE:
		case C_RUP:
			InfoPos( pos );
			/*DrawTempTrack();*/
			break;
		case C_WUP:
			DoZoomUp((void *)1L);			
			break;
		case C_WDOWN:
			DoZoomDown((void *)1L);
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


wPos_t autoPanFactor = 10;
static void DoMousew( wDraw_p d, void * context, wAction_t action, wPos_t x, wPos_t y )
{
	coOrd pos;
	coOrd orig;
	wPos_t w, h;
	static wPos_t lastX, lastY;
	DIST_T minDist;

	if ( autoPan && !inPlayback ) {
		wDrawGetSize( mainD.d, &w, &h );
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
					ConstraintOrig( &orig, mainD.size );
					if ( orig.x != mainD.orig.x || orig.y != mainD.orig.y ) {
						//DrawMapBoundingBox( FALSE );
						mainD.orig = orig;
						MainRedraw();
						MapRedraw();
						/*DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );*/
						//DrawMapBoundingBox( TRUE );
						wFlush();
					}
				}
			}
			lastX = x;
			lastY = y;
		}
	}
	mainD.Pix2CoOrd( &mainD, x, y, &pos );
	mousePositionx = x;
	mousePositiony = y;

	DoMouse( action, pos );
}

static wBool_t PlaybackMain( char * line )
{
	int rc;
	int action;
	coOrd pos;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");
	rc=sscanf( line, "%d " SCANF_FLOAT_FORMAT SCANF_FLOAT_FORMAT, &action, &pos.x, &pos.y);
	RestoreLocale(oldLocale);

	if (rc != 3) {
		SyntaxError( "MOUSE", rc, 3 );
	} else {
		PlaybackMouse( DoMouse, &mainD, (wAction_t)action, pos, wDrawColorBlack );
	}
	return TRUE;
}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

static paramDrawData_t mapDrawData = { 100, 100, (wDrawRedrawCallBack_p)MapRedraw, DoMapPan, &mapD };
static paramData_t mapPLs[] = {
	{	PD_DRAW, NULL, "canvas", 0, &mapDrawData } };
static paramGroup_t mapPG = { "map", PGO_NODEFAULTPROC, mapPLs, sizeof mapPLs/sizeof mapPLs[0] };

static void MapDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	if ( inx == -1 ) {
		MapWindowShow( FALSE );
	}
}


static void DrawChange( long changes )
{
	if (changes & CHANGE_MAIN) {
		MainRedraw();
		MapRedraw();
	}
	if (changes &CHANGE_UNITS)
		SetInfoBar();
	if (changes & CHANGE_MAP)
		MapResize();
}


EXPORT void DrawInit( int initialZoom )
{
	wPos_t w, h;

	wWinGetSize( mainW, &w, &h );
	/*LayoutToolBar();*/
	h = h - (toolbarHeight+max(textHeight,infoHeight)+10);
	if ( w <= 0 ) w = 1;
	if ( h <= 0 ) h = 1;
	tempD.d = mainD.d = wDrawCreate( mainW, 0, toolbarHeight, "", BD_TICKS,
												w, h, &mainD,
				(wDrawRedrawCallBack_p)MainRedraw, DoMousew );

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
	if ( mainD.dpi == 75 ) {
		mainD.dpi = 72.0;
	} else if ( mainD.scale > 1.0 && mainD.scale <= 12.0 ) {
		mainD.dpi = floor( (mainD.dpi + mainD.scale/2)/mainD.scale) * mainD.scale;
	}
	tempD.dpi = mainD.dpi;

	SetMainSize();
	mapD.scale = mapScale;
	/*w = (wPos_t)((mapD.size.x/mapD.scale)*mainD.dpi + 0.5)+2;*/
	/*h = (wPos_t)((mapD.size.y/mapD.scale)*mainD.dpi + 0.5)+2;*/
	ParamRegister( &mapPG );
	mapW = ParamCreateDialog( &mapPG, MakeWindowTitle(_("Map")), NULL, NULL, NULL, FALSE, NULL, 0, MapDlgUpdate );
	ChangeMapScale();

	log_pan = LogFindIndex( "pan" );
	log_zoom = LogFindIndex( "zoom" );
	log_mouse = LogFindIndex( "mouse" );
	AddPlaybackProc( "MOUSE ", (playbackProc_p)PlaybackMain, NULL );

	rulerFp = wStandardFont( F_HELV, FALSE, FALSE );

	SetZoomRadio( mainD.scale ); 
	InfoScale();
	SetInfoBar();
	InfoPos( zero );
	RegisterChangeNotification( DrawChange );
#ifdef LATER
	wAttachAccelKey( wAccelKey_Pgup, 0, (wAccelKeyCallBack_p)doZoomUp, NULL );
	wAttachAccelKey( wAccelKey_Pgdn, 0, (wAccelKeyCallBack_p)doZoomDown, NULL );
#endif
}
