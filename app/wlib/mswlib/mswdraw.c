/** \file mswdraw.c
 * Draw basic geometric shapes
 */

/*  XTrackCAD - Model Railroad CAD
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

#define _WIN32_WINNT 0x0600		/* for wheel mouse supposrt */
#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <winuser.h>

#define wFont_t tagLOGFONTA

#include "mswint.h"
#include <FreeImage.h>

wBool_t wDrawDoTempDraw = TRUE;
/*
 *****************************************************************************
 *
 * Draw
 *
 *****************************************************************************
 */

static wBool_t initted = FALSE;

static FARPROC oldDrawProc;


static long tmpOp = 0x990066;
static long setOp = 0x8800c6;
static long clrOp = 0xbb0226;

#define CENTERMARK_LENGTH 4

static bool bDrawMainBM = 0;

typedef struct {
	double x, y;
} coOrd;

#ifdef SLOW
static wDrawPix_t XWINPIX2DRAWPIX( wDraw_p d, wWinPix_t ix )
{
	return (wDrawPix_t)ix;
}

static wDrawPix_t YWINPIX2DRAWPIX( wDraw_p d, wWinPix_t iy )
{
	wWinPix_t y;
	y = (wDrawPix_t)(d->h-2-iy);
	return y;
}

static wWinPix_t XDRAWPIX2WINPIX( wDraw_p d, wDrawPix_t xx )
{
	wWinPix_t ix;
	ix = (wWinPix_t)(xx);
	return ix;
}

static wWinPix_t YDRAWPIX2WINPIX( wDraw_p d, wDrawPix_t y )
{
	wWinPix_t iy;
	iy = (d->h)-2 - (wWinPix_t)(y);
	return iy;
}

#else
#define XWINPIX2DRAWPIX( d, ix ) \
	((wDrawPix_t)ix)

#define YWINPIX2DRAWPIX( d, iy ) \
	((wDrawPix_t)(d->h-2-iy))

#define XDRAWPIX2WINPIX( d, xx ) \
	((wWinPix_t)(xx))

#define YDRAWPIX2WINPIX( d, y ) \
	(d->h - 2 - (wWinPix_t)(y))
#endif

/*
 *****************************************************************************
 *
 * Basic Line Draw
 *
 *****************************************************************************
 */



/** @prefs [msw tweak] NoNegDrawArgs=1 Suppress drawing if x < 0 or y < 0 (-1 value causes preference read) */
static long noNegDrawArgs = -1;
/** @prefs [msw tweak] NoFlatEndCaps=1 Suppress EndCap Flat pen style */
static long noFlatEndCaps = 0;

void wDrawDelayUpdate(
		wDraw_p d,
		wBool_t delay )
{
}

wBool_t wDrawSetTempMode(
	wDraw_p bd,
	wBool_t bTemp )
{
	wBool_t rc = bd->bTempMode;
	bd->bTempMode = bTemp;
	if (rc == FALSE && bTemp == TRUE) {
		// Main to Temp drawing
		// Copy mainBM to tempBM
		wDrawClearTemp( bd );
		if (bDrawMainBM) return rc;
		HDC hDcOld = CreateCompatibleDC(bd->hDc);
		HBITMAP hBmOld = SelectObject(hDcOld, bd->hBmMain);
		SelectObject(bd->hDc, bd->hBmTemp);
		BitBlt(bd->hDc, 0, 0,
			bd->w, bd->h,
			hDcOld, 0, 0,
			SRCCOPY);
		SelectObject(hDcOld, hBmOld);
		DeleteDC(hDcOld);
		bd->bCopiedMain = TRUE;
	}
	return rc;
}

/**
 * Sets the proper pen and composition for the next drawing operation
 * 
 *
 * \param hDc IN device context
 * \param d IN ???
 * \param dw IN line width
 * \param lt IN line type (dashed, solid, ...)
 * \param dc IN color
 * \param dopt IN ????
 */
static void setDrawMode(
		wDraw_p d,
		wDrawWidth dw,
		wDrawLineType_e lt,
		wDrawColor dc,
		wDrawOpts dopt )
{
	long centerPen[] = {40,10,20,10};
	long phantomPen[] = {40,10,20,10,20,10};

	HPEN hOldPen;
	static wDraw_p d0;
	static wDrawWidth dw0 = -1;
	static wDrawLineType_e lt0 = (wDrawLineType_e)-1;
	static wDrawColor dc0 = -1;
	static LOGBRUSH logBrush = { 0, 0, 0 };
	DWORD penStyle;

	if ( wDrawDoTempDraw && (dopt & wDrawOptTemp) )
		SelectObject(d->hDc, d->hBmTemp);
	else
		SelectObject(d->hDc, d->hBmMain);

	if ( d->hasPalette ) {
		int winPaletteClock = mswGetPaletteClock();
		if ( d->paletteClock < winPaletteClock ) {
			RealizePalette( d->hDc );
			d->paletteClock = winPaletteClock;
		}
	}

	SetROP2( d->hDc, R2_COPYPEN );
	if ( d == d0 && dw0 == dw && lt == lt0 && dc == dc0 )
		return;

	// make sure that the line width is at least 1!
	if( !dw ) 
		dw++;

	d0 = d; dw0 = dw; lt0 = lt; dc0 = dc;

	void * penarray = NULL;
	int penarray_size = 0;

	logBrush.lbColor = mswGetColor(d->hasPalette,dc);
	if ( lt==wDrawLineSolid ) {
		penStyle = PS_GEOMETRIC | PS_SOLID;
		if ( noFlatEndCaps == FALSE )
			penStyle |= PS_ENDCAP_FLAT;
	} else if (lt == wDrawLineDot) {
		penStyle = PS_GEOMETRIC | PS_DOT;
	} else if (lt == wDrawLineDash) {
		penStyle = PS_GEOMETRIC | PS_DASH;
	} else if (lt == wDrawLineDashDot) {
		penStyle = PS_GEOMETRIC | PS_DASHDOT;
	} else if ( lt == wDrawLineDashDotDot){
		penStyle = PS_GEOMETRIC | PS_DASHDOTDOT;
	} else if (  lt == wDrawLineCenter) {
		penStyle = PS_GEOMETRIC | PS_USERSTYLE;
		penarray = &centerPen;
		penarray_size = sizeof(centerPen)/sizeof(long);
	} else if (  lt == wDrawLinePhantom) {
		penStyle = PS_GEOMETRIC | PS_USERSTYLE;
		penarray = &phantomPen;
		penarray_size = sizeof(phantomPen) / sizeof(long);
	} else
		penStyle = PS_GEOMETRIC | PS_SOLID;
	d->hPen = ExtCreatePen( penStyle,
					dw,
					&logBrush,
					penarray_size,
					penarray );
	hOldPen = SelectObject( d->hDc, d->hPen );
	DeleteObject( hOldPen );
}

static void setDrawBrush(
		wDraw_p d,
		wDrawColor dc,
		wDrawOpts dopt )
{
	HBRUSH hOldBrush;
	static wDraw_p d0;
	static wDrawColor dc0 = -1;

	setDrawMode( d, 0, wDrawLineSolid, dc, dopt );
	if ( d == d0 && dc == dc0 )
		return;

	d0 = d; dc0 = dc;

	d->hBrush = CreateSolidBrush( 
				mswGetColor(d->hasPalette,dc) );
	hOldBrush = SelectObject( d->hDc, d->hBrush );
	DeleteObject( hOldBrush );
}


static void myInvalidateRect(
		wDraw_p d,
		RECT * prect )
{
	if ( prect->top < 0 ) prect->top = 0;
	if ( prect->left < 0 ) prect->left = 0;
	if ( prect->bottom > d->h ) prect->bottom = d->h;
	if ( prect->right > d->w ) prect->right = d->w;
	InvalidateRect( d->hWnd, prect, FALSE );
}


static int clip0( POINT * p0, POINT * p1, wDraw_p d )
{
	long int x0=p0->x, y0=p0->y, x1=p1->x, y1=p1->y;
	long int dx, dy;
	if ( x0<0 && x1<0 ) return 0;
	if ( y0<0 && y1<0 ) return 0;
	dx=x1-x0;
	dy=y1-y0;
	if ( x0 < 0 ) {
		y0 -= x0*dy/dx;
		x0 = 0;
	}
	if ( y0 < 0 ) {
		if ( (x0 -= y0*dx/dy) < 0 ) return 0;
		y0 = 0;
	}
	if ( x1 < 0 ) {
		y1 -= x1*dy/dx;
		x1 = 0;
	}
	if ( y1 < 0 ) {
		if ( (x1 -= y1*dx/dy) < 0 ) return 0;
		y1 = 0;
	}
	p0->x = (int)x0;
	p0->y = (int)y0;
	p1->x = (int)x1;
	p1->y = (int)y1;
	return 1;
}


void wDrawLine(
		wDraw_p d,
		wDrawPix_t p0x,
		wDrawPix_t p0y,
		wDrawPix_t p1x,
		wDrawPix_t p1y,
		wDrawWidth dw,
		wDrawLineType_e lt,
		wDrawColor dc,
		wDrawOpts dopt )
{
	POINT p0, p1;
	RECT rect;
	setDrawMode( d, dw, lt, dc, dopt );
	p0.x = XDRAWPIX2WINPIX(d,p0x);
	p0.y = YDRAWPIX2WINPIX(d,p0y);
	p1.x = XDRAWPIX2WINPIX(d,p1x);
	p1.y = YDRAWPIX2WINPIX(d,p1y);
	if ( noNegDrawArgs>0 && !clip0( &p0, &p1, d ) )
		return;
	MoveTo( d->hDc, p0.x, p0.y );
	LineTo( d->hDc, p1.x, p1.y );
	if (d->hWnd) {
		if (dw==0)
			dw = 1;
		dw++;
	if (p0.y<p1.y) {
		rect.top = p0.y-dw;
		rect.bottom = p1.y+dw;
	} else {
		rect.top = p1.y-dw;
		rect.bottom = p0.y+dw;
	}
	if (p0.x<p1.x) {
		rect.left = p0.x-dw;
		rect.right = p1.x+dw;
	} else {
		rect.left = p1.x-dw;
		rect.right = p0.x+dw;
	}
	myInvalidateRect( d, &rect );
	}
}

static double d2r(double angle)
{
	while (angle < 0.0) angle += 360.0;
	while (angle >= 360.0) angle -= 360.0;
	angle *= (M_PI / 180.0);
	return angle;
}

static double mswsin( double angle )
{
	while (angle < 0.0) angle += 360.0;
	while (angle >= 360.0) angle -= 360.0;
	angle *= (M_PI*2.0)/360.0;
	return sin( angle );
}

static double mswcos( double angle )
{
	while (angle < 0.0) angle += 360.0;
	while (angle >= 360.0) angle -= 360.0;
	angle *= (M_PI*2.0)/360.0;
	return cos( angle );
}

static double mswasin( double x, double h )
{
	double angle;
	angle = asin( x/h );
	angle /= (M_PI*2.0)/360.0;
	return angle;
}

/**
 * Draw an arc around a specified center
 *
 * \param d IN ?
 * \param px, py IN  center of arc
 * \param r IN radius
 * \param a0, a1 IN start and end angle
 * \param drawCenter draw marking for center
 * \param dw line width
 * \param lt line type
 * \param dc color
 * \param dopt ?
 */


void wDrawArc(
		wDraw_p d,
		wDrawPix_t px,
		wDrawPix_t py,
		wDrawPix_t r,
		double a0,
		double a1,
		int sizeCenter,
		wDrawWidth dw,
		wDrawLineType_e lt,
		wDrawColor dc,
		wDrawOpts dopt )
{
	int i, cnt;
	POINT p0, p1, ps, pe, pp0, pp1, pp2, pc;
	wDrawPix_t psx, psy, pex, pey;
    double len, aa, ai;
	RECT rect;
	int needMoveTo;
	wBool_t fakeArc = FALSE;

	len = d2r(a1) * r;
	if (len < 3)
		return;

	p0.x = XDRAWPIX2WINPIX(d,px-r);
	p0.y = YDRAWPIX2WINPIX(d,py+r);
	p1.x = XDRAWPIX2WINPIX(d,px+r);
	p1.y = YDRAWPIX2WINPIX(d,py-r);

	pex = px + r * mswsin(a0);
	pey = py + r * mswcos(a0);
	psx = px + r * mswsin(a0+a1);
	psy = py + r * mswcos(a0+a1);

	/*pointOnCircle( &pe, p, r, a0 );
	pointOnCircle( &ps, p, r, a0+a1 );*/
	ps.x = XDRAWPIX2WINPIX(d,psx);
	ps.y = YDRAWPIX2WINPIX(d,psy);
	pe.x = XDRAWPIX2WINPIX(d,pex);
	pe.y = YDRAWPIX2WINPIX(d,pey);

	setDrawMode( d, dw, lt, dc, dopt );

	if (dw == 0)
		dw = 1;

	/* Windows drawing will overshoot the end of the arc for large radius */
	if (r > 500) { 
		/* The book says 32K but experience says otherwise */
		fakeArc = TRUE;
	}
	if ( noNegDrawArgs > 0 ) {
		if ( p0.x < 0 || p0.y < 0 || p1.x < 0 || p1.y < 0 )
			fakeArc = TRUE;
	}
	if ( fakeArc ) {
		cnt = (int)(a1 / 2);
		if ( cnt <= 0 ) cnt = 1;
		if ( cnt > 180 ) cnt = 180;
		// Convert a0 and a1 to radians here
		ai = d2r(a1) / cnt;
		aa = d2r(a0);
		psx = px + r * sin(aa);
		psy = py + r * cos(aa);
		pp0.x = XDRAWPIX2WINPIX( d, psx );
		pp0.y = YDRAWPIX2WINPIX( d, psy );
		needMoveTo = TRUE;
		for ( i=0; i<cnt; i++ ) {
			aa += ai;
			psx = px + r * sin(aa);
			psy = py + r * cos(aa);
			pp2.x = pp1.x = XDRAWPIX2WINPIX( d, psx );
			pp2.y = pp1.y = YDRAWPIX2WINPIX( d, psy );
			if ( clip0( &pp0, &pp1, d ) ) {
				if (needMoveTo) {
					MoveTo( d->hDc, pp0.x, pp0.y );
					needMoveTo = FALSE;
				}
				LineTo( d->hDc, pp1.x, pp1.y );
			} else {
				needMoveTo = TRUE;
			}
			pp0.x = pp2.x; pp0.y = pp2.y;
		}
	} else {
		if ( a0 == 0.0 && a1 == 360.0 ) {
			Arc( d->hDc, p0.x, p1.y, p1.x, p0.y, ps.x, p0.y, pe.x, p1.y );
			Arc( d->hDc, p0.x, p1.y, p1.x, p0.y, ps.x, p1.y, pe.x, p0.y );
		} else {
			Arc( d->hDc, p0.x, p1.y, p1.x, p0.y, ps.x, ps.y, pe.x, pe.y );
		}
	}

	// should the center of the arc be drawn?
	if( sizeCenter ) {
			
			// calculate the center coordinates
			pc.x = XDRAWPIX2WINPIX( d, px );
			pc.y = YDRAWPIX2WINPIX( d, py );
			// now draw the crosshair
			MoveTo( d->hDc, pc.x - CENTERMARK_LENGTH*sizeCenter, pc.y );
			LineTo( d->hDc, pc.x + CENTERMARK_LENGTH*sizeCenter, pc.y );
			MoveTo( d->hDc, pc.x, pc.y - CENTERMARK_LENGTH*sizeCenter );
			LineTo( d->hDc, pc.x, pc.y + CENTERMARK_LENGTH*sizeCenter );
			
			// invalidate the area of the crosshair
			rect.top  = pc.y - CENTERMARK_LENGTH*sizeCenter - 1;
			rect.bottom  = pc.y + CENTERMARK_LENGTH*sizeCenter + 1;
			rect.left = pc.x - CENTERMARK_LENGTH*sizeCenter - 1;
			rect.right = pc.x + CENTERMARK_LENGTH*sizeCenter + 1;
			myInvalidateRect( d, &rect );
	}

	if (d->hWnd) {
		dw++;
		a1 += a0;
		if (a1>360.0)
			rect.top = p0.y;
		else
			rect.top = min(pe.y,ps.y);
		if (a1>(a0>180?360.0:0.0)+180)
			rect.bottom = p1.y;
		else
			rect.bottom = max(pe.y,ps.y);
		if (a1>(a0>270?360.0:0.0)+270)
			rect.left = p0.x;
		else
			rect.left = min(pe.x,ps.x);
		if (a1>(a0>90?360.0:0.0)+90)
			rect.right = p1.x;
		else
			rect.right = max(pe.x,ps.x);
		rect.top -= dw;
		rect.bottom += dw;
		rect.left -= dw;
		rect.right += dw;
		myInvalidateRect( d, &rect );

	}
}

void wDrawPoint(
		wDraw_p d,
		wDrawPix_t px,
		wDrawPix_t py,
		wDrawColor dc,
		wDrawOpts dopt )
{
	POINT p0;
	RECT rect;

	p0.x = XDRAWPIX2WINPIX(d,px);
	p0.y = YDRAWPIX2WINPIX(d,py);

	if ( p0.x < 0 || p0.y < 0 )
		return;
	if ( p0.x >= d->w || p0.y >= d->h )
		return;
	setDrawMode( d, 0, wDrawLineSolid, dc, dopt );

	SetPixel( d->hDc, p0.x, p0.y, mswGetColor(d->hasPalette,dc) /*colorPalette.palPalEntry[dc]*/ );
	if (d->hWnd) {
		rect.top = p0.y-1;
		rect.bottom = p0.y+1;
		rect.left = p0.x-1;
		rect.right = p0.x+1;
		myInvalidateRect( d, &rect );
	}
}

/*
 *****************************************************************************
 *
 * Fonts
 *
 *****************************************************************************
 */


static LOGFONT logFont = {
		/* Initial default values */
		-24, 0, /* H, W */
		0,		/* A */
		0,
		FW_REGULAR,
		0, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Arial" };

static LOGFONT timesFont[2][2] = {
		{ {
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_REGULAR,
		0, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Times" },
		{
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_REGULAR,
		1, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Times" } },
		{ {
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_BOLD,
		0, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Times" },
		{
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_BOLD,
		1, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Times" } } };

static LOGFONT helvFont[2][2] = {
		{ {
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_REGULAR,
		0, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Arial" },
		{
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_REGULAR,
		1, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Arial" } },
		{ {
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_BOLD,
		0, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Arial" },
		{
		/* Initial default values */
		0, 0,	/* H, W */
		0,		/* A */
		0,
		FW_BOLD,
		1, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		0,		/* P&F */
		"Hevletica" } } };


void mswFontInit( void )
{
	const char * face;
	long size;
	/** @prefs [msw window font] face=FontName */
	face = wPrefGetString( "msw window font", "face" );
	/** @prefs [msw window font] size=-24 */
	wPrefGetInteger( "msw window font", "size", &size, -24 );
	if (face) {
		strncpy( logFont.lfFaceName, face, LF_FACESIZE );
	}
	logFont.lfHeight = (int)size;
}


static CHOOSEFONT chooseFont;
static wFontSize_t fontSize = 18;
static double fontFactor = 1.0;

static void doChooseFont( void )
{
	int rc;
	memset( &chooseFont, 0, sizeof chooseFont );
	chooseFont.lStructSize = sizeof chooseFont;
	chooseFont.hwndOwner = mswHWnd;
	chooseFont.lpLogFont = &logFont;
	chooseFont.Flags = CF_SCREENFONTS|CF_SCALABLEONLY|CF_INITTOLOGFONTSTRUCT;
	chooseFont.nFontType = SCREEN_FONTTYPE;
	rc = ChooseFont( &chooseFont );
	if (rc) {
		fontSize = (wFontSize_t)(-logFont.lfHeight * 72) / 96.0 / fontFactor;
		if (fontSize < 1)
			fontSize = 1;
		wPrefSetString( "msw window font", "face", logFont.lfFaceName );
		wPrefSetInteger( "msw window font", "size", logFont.lfHeight );
	}
}

static int computeFontSize( wDraw_p d, double siz )
{
	int ret;
	siz = (siz * d->DPI) / 72.0;
	ret = (int)(siz * fontFactor);
	if (ret < 1)
		ret = 1;
	return -ret;
}

void wDrawGetTextSize(
		wDrawPix_t *w,
		wDrawPix_t *h,
		wDrawPix_t *d,
		wDrawPix_t *a,
		wDraw_p bd,
		const char * text,
		wFont_p fp,
		double siz )
{
	wWinPix_t x, y;
	HFONT newFont, prevFont;
	DWORD extent;
	int oldLfHeight;
	TEXTMETRIC textMetric;

	if (fp == NULL)
		fp = &logFont;
	fp->lfEscapement = 0;
	oldLfHeight = fp->lfHeight;
	fp->lfHeight = computeFontSize( bd, siz );
	fp->lfWidth = 0;
	newFont = CreateFontIndirect( fp );
	prevFont = SelectObject( bd->hDc, newFont );
	extent = GetTextExtent( bd->hDc, CAST_AWAY_CONST text, (int)(strlen(text)) );

	GetTextMetrics(bd->hDc, &textMetric);

	x = LOWORD(extent);
	y = HIWORD(extent);
	*w = (wDrawPix_t)x;
	*h = (wDrawPix_t)y;
	*d = (wDrawPix_t)textMetric.tmDescent;
	*a = (wDrawPix_t)textMetric.tmAscent;

	SelectObject( bd->hDc, prevFont );
	DeleteObject( newFont );
	fp->lfHeight = oldLfHeight;
}
/**
 * Draw text
 * 
 * \param d	device context
 * \param px position x
 * \param py position y
 * \param angle drawing angle
 * \param text text to print
 * \param fp font
 * \param siz font size
 * \param dc color
 * \param dopts drawing options
 */
void wDrawString(
    wDraw_p d,
    wDrawPix_t px,
    wDrawPix_t py,
    double angle,
    const char * text,
    wFont_p fp,
    double siz,
    wDrawColor dc,
    wDrawOpts dopts)
{
    int x, y;
    HFONT newFont, prevFont;
    DWORD extent;
    int w, h;
    RECT rect;
    int oldLfHeight;

    if (fp == NULL) {
        fp = &logFont;
    }

    oldLfHeight = fp->lfHeight;
    fp->lfEscapement = (int)(angle*10.0);
    fp->lfHeight = computeFontSize(d, siz);
    fp->lfWidth = 0;
    newFont = CreateFontIndirect(fp);
    x = XDRAWPIX2WINPIX(d,px) + (int)(mswsin(angle)*fp->lfHeight-0.5);
    y = YDRAWPIX2WINPIX(d,py) + (int)(mswcos(angle)*fp->lfHeight-0.5);

    if (noNegDrawArgs > 0 && (x < 0 || y < 0)) {
		DeleteObject(newFont);
        return;
    }

		setDrawMode( d, 0, wDrawLineSolid, dc, dopts );
		prevFont = SelectObject(d->hDc, newFont);
        SetBkMode(d->hDc, TRANSPARENT);

        if (dopts & wDrawOutlineFont) {
            HPEN oldPen;
            BeginPath(d->hDc);
            TextOut(d->hDc, x, y, text, (int)strlen(text));
            EndPath(d->hDc);

            // Now draw outline text
            oldPen = SelectObject(d->hDc,
                                  CreatePen(PS_SOLID, 1,
                                            mswGetColor(d->hasPalette, dc)));
            StrokePath(d->hDc);
            SelectObject(d->hDc, oldPen);
        } else {
            COLORREF old;

            old = SetTextColor(d->hDc, mswGetColor(d->hasPalette,
                                                   dc));
            TextOut(d->hDc, x, y, text, (int)(strlen(text)));
            SetTextColor(d->hDc, old);
        }

        extent = GetTextExtent(d->hDc, CAST_AWAY_CONST text, (int)(strlen(text)));
        SelectObject(d->hDc, prevFont);
        w = LOWORD(extent);
        h = HIWORD(extent);

        if (d->hWnd) {
            rect.top = y - (w + h + 1);
            rect.bottom = y + (w + h + 1);
            rect.left = x - (w + h + 1);
            rect.right = x + (w + h + 1);
            myInvalidateRect(d, &rect);
        }

    DeleteObject(newFont);
    fp->lfHeight = oldLfHeight;
}

static const char * wCurFont( void )
{
	return logFont.lfFaceName;
}

void wInitializeFonts()
{
}

wFont_p wStandardFont( int family, wBool_t bold, wBool_t italic )
{
	if (family == F_TIMES)
		return &timesFont[bold][italic];
	else if (family == F_HELV)
		return &helvFont[bold][italic];
	else
		return NULL;
}

void wSelectFont( const char * title )
{
	doChooseFont();
}


wFontSize_t wSelectedFontSize( void )
{
	return fontSize;
}

void wSetSelectedFontSize(wFontSize_t size)
{
	fontSize = size;
}

/*
 *****************************************************************************
 *
 * Misc
 *
 *****************************************************************************
 */



void wDrawFilledRectangle(
		wDraw_p d,
		wDrawPix_t px,
		wDrawPix_t py,
		wDrawPix_t sx,
		wDrawPix_t sy,
		wDrawColor color,
		wDrawOpts opts )
{
	int mode;
	RECT rect;
	if (d == NULL)
		return;
	setDrawBrush( d, color, opts );
	if (opts & wDrawOptTransparent) {
		mode = R2_NOTXORPEN;
	}
	else {
		mode = R2_COPYPEN;
	}
	SetROP2(d->hDc, mode);
	rect.left = XDRAWPIX2WINPIX(d,px);
	rect.right = XDRAWPIX2WINPIX(d,px+sx);
	rect.top = YDRAWPIX2WINPIX(d,py+sy);
	rect.bottom = YDRAWPIX2WINPIX(d,py);
	if ( rect.right < 0 ||
		 rect.bottom < 0 )
		return;
	if ( rect.left < 0 )
		rect.left = 0;
	if ( rect.top < 0 )
		rect.top = 0;
	if ( rect.left > d->w ||
		 rect.top > d->h )
		return;
	if ( rect.right > d->w )
		rect.right = d->w;
	if ( rect.bottom > d->h )
		rect.bottom = d->h;
	Rectangle( d->hDc, rect.left, rect.top, rect.right, rect.bottom );
	if (d->hWnd) {
		rect.top--;
		rect.left--;
		rect.bottom++;
		rect.right++;
		myInvalidateRect( d, &rect );
	}
}

#ifdef DRAWFILLPOLYLOG
    static FILE * logF;
#endif

static dynArr_t wFillPoints_da;
static dynArr_t wFillType_da;

#define POINTTYPE(N) DYNARR_N( BYTE, wFillType_da, (N) )
#define POINTPOS(N) DYNARR_N( POINT, wFillPoints_da, (N) )

/**
 * Add a point definition to the list. The clipping rectangle is recalculated to
 * include the new point.
 *
 * \param d IN drawing context
 * \param pk IN index of new point
 * \param pp IN pointer to the point's coordinates
 * \param type IN line type
 * \param pr IN/OUT clipping rectangle
 */

static void addPoint(
    wDraw_p d,
    int pk,
    coOrd * pp,
    BYTE type, RECT * pr)
{
    POINT p;
    p.x = XDRAWPIX2WINPIX(d, pp->x);
    p.y = YDRAWPIX2WINPIX(d, pp->y);

#ifdef DRAWFILLPOLYLOG
    fprintf(logF, "	q[%d] = {%d,%d}\n", pk, p.x, p.y);
#endif

    DYNARR_N(POINT, wFillPoints_da, pk) = p;
    DYNARR_N(BYTE, wFillType_da, pk) = type;

    if (p.x < pr->left) {
        pr->left = p.x;
    }
    if (p.x > pr->right) {
        pr->right = p.x;
    }
    if (p.y < pr->top) {
        pr->top = p.y;
    }
    if (p.y > pr->bottom) {
        pr->bottom = p.y;
    }
}

/**
 * Draw a polyline consisting of straights with smoothed or rounded corners.
 * Optionally the area can be filled.
 *
 * \param d	IN	drawing context
 * \param node IN 2 dimensional array of coordinates
 * \param type IN type of corener (vertex, smooth or round)
 * \param cnt IN number of points
 * \param color IN color
 * \param dw IN line width
 * \param lt IN line type
 * \param opts IN drawing options
 * \param fill IN area will be filled if true
 * \param open IN do not close area
 */

void wDrawPolygon(
    wDraw_p d,
    wDrawPix_t node[][2],
    wPolyLine_e type[],
    wIndex_t cnt,
    wDrawColor color,
    wDrawWidth dw,
    wDrawLineType_e lt,
    wDrawOpts opts,
    int fill,
    int open)
{
    RECT rect;
    int i, prevNode, nextNode;
    int pointCount = 0;
    coOrd endPoint0, endPoint1, controlPoint0, controlPoint1;
    coOrd point, startingPoint;
    BOOL rc;
    int closed = 0;

    if (d == NULL) {
        return;
    }

    // make sure the array for the points is large enough
    // worst case are rounded corners that require 4 points
    DYNARR_RESET(POINT,wFillPoints_da);
    DYNARR_SET(POINT,wFillPoints_da,(cnt + 1) * 4);
    DYNARR_RESET(BYTE,wFillType_da);
    DYNARR_SET(POINT,wFillType_da, (cnt + 1) * 4);

    BeginPath(d->hDc);

    if (fill) {
		int mode;
        setDrawBrush(d, color, opts);
		if (opts & wDrawOptTransparent) {
			mode = R2_NOTXORPEN;
		}
		else {
			mode = R2_COPYPEN;
		}
		SetROP2(d->hDc, mode);

    } else {
        setDrawMode(d, dw, lt, color, opts);
    }

    rect.left = rect.right = XDRAWPIX2WINPIX(d,node[cnt-1][0]-1);
    rect.top = rect.bottom = YDRAWPIX2WINPIX(d,node[cnt-1][1]+1);

#ifdef DRAWFILLPOLYLOG
    logF = fopen("log.txt", "a");
    fprintf(logF, "\np[%d] = {%d,%d}\n", cnt-1, node[0][0], node[0][1]);
#endif

    for (i=0; i<cnt; i++) {
        wPolyLine_e type1;
        point.x = node[i][0];
        point.y = node[i][1];
		if (type != NULL)
			type1 = type[i];
		else
			type1 = wPolyLineStraight;

        if (type1 == wPolyLineRound || type1 == wPolyLineSmooth) {
            prevNode = (i == 0) ? cnt - 1 : i - 1;
            nextNode = (i == cnt - 1) ? 0 : i + 1;

            // calculate distance to neighboring nodes
            int prevXDistance = (wWinPix_t)(node[i][0] - node[prevNode][0]);
            int prevYDistance = (wWinPix_t)(node[i][1] - node[prevNode][1]);
            int nextXDistance = (wWinPix_t)(node[nextNode][0]-node[i][0]);
            int nextYDistance = (wWinPix_t)(node[nextNode][1]-node[i][1]);

            // distance from node to endpoints of curve is half the line length
            endPoint0.x = (prevXDistance/2)+node[prevNode][0];
            endPoint0.y = (prevYDistance/2)+node[prevNode][1];
            endPoint1.x = (nextXDistance/2)+node[i][0];
            endPoint1.y = (nextYDistance/2)+node[i][1];

            if (type1 == wPolyLineRound) {
                double distNext = (nextXDistance*nextXDistance + nextYDistance * nextYDistance);
                double distPrev = (prevXDistance*prevXDistance + prevYDistance * prevYDistance);
                // but should be half of the shortest line length (equidistant from node) for round
                if ((distPrev > 0) && (distNext > 0)) {
                    double ratio = sqrt(distPrev / distNext);
                    if (distPrev < distNext) {
                        endPoint1.x = ((nextXDistance*ratio) / 2) + node[i][0];
                        endPoint1.y = ((nextYDistance*ratio) / 2) + node[i][1];
                    } else {
                        endPoint0.x = node[i][0] - (prevXDistance / (2 * ratio));
                        endPoint0.y = node[i][1] - (prevYDistance / (2 * ratio));
                    }
                }
                // experience says that the best look is achieved if the
                // control points are in the middle between end point and node
                controlPoint0.x = (node[i][0] - endPoint0.x) / 2 + endPoint0.x;
                controlPoint0.y = (node[i][1] - endPoint0.y) / 2 + endPoint0.y;

                controlPoint1.x = (endPoint1.x - node[i][0]) / 2 + node[i][0];
                controlPoint1.y = (endPoint1.y - node[i][1]) / 2 + node[i][1];
            } else {
                controlPoint0 = point;
                controlPoint1 = point;
            }
        }

        if (i==0) {
            if (type1 == wPolyLineStraight || open) {
                // for straight lines or open shapes use the starting point as passed
                addPoint(d, pointCount++, &point, PT_MOVETO, &rect);
                startingPoint = point;
            } else {
                // for Bezier begin with the calculated starting point
                addPoint(d, pointCount++, &endPoint0, PT_MOVETO, &rect);
                addPoint(d, pointCount++, &controlPoint0, PT_BEZIERTO, &rect);
                addPoint(d, pointCount++, &controlPoint1, PT_BEZIERTO, &rect);
                addPoint(d, pointCount++, &endPoint1, PT_BEZIERTO, &rect);
                startingPoint = endPoint0;
            }
        } else {
            if (type1 == wPolyLineStraight || (open && (i==cnt-1))) {
                addPoint(d, pointCount++, &point, PT_LINETO, &rect);
            } else {
                if (i==cnt-1 && !open) {
                    closed = TRUE;
                }
                addPoint(d, pointCount++, &endPoint0, PT_LINETO, &rect);
                addPoint(d, pointCount++, &controlPoint0, PT_BEZIERTO, &rect);
                addPoint(d, pointCount++, &controlPoint1, PT_BEZIERTO, &rect);
                addPoint(d, pointCount++, &endPoint1,
                         PT_BEZIERTO | (closed ? PT_CLOSEFIGURE : 0), &rect);
            }
        }
    }

    if (!open && !closed) {
        addPoint(d, pointCount++, &startingPoint, PT_LINETO, &rect);
    }
    rc = PolyDraw(d->hDc, wFillPoints_da.ptr, wFillType_da.ptr, pointCount);

    EndPath(d->hDc);

    if (fill && !open) {
        FillPath(d->hDc);
    } else {
        StrokePath(d->hDc);
    }

    if (d->hWnd) {
        rect.top--;
        rect.left--;
        rect.bottom++;
        rect.right++;
        myInvalidateRect(d, &rect);
    }
}

#define MAX_FILLCIRCLE_POINTS	(30)
void wDrawFilledCircle(
		wDraw_p d,
		wDrawPix_t x,
		wDrawPix_t y,
		wDrawPix_t r,
		wDrawColor color,
		wDrawOpts opts )
{
	POINT p0, p1;
	RECT rect;
	static wDrawPix_t circlePts[MAX_FILLCIRCLE_POINTS][2];
	int inx, cnt;
	double dang;

	p0.x = XDRAWPIX2WINPIX(d,x-r);
	p0.y = YDRAWPIX2WINPIX(d,y+r);
	p1.x = XDRAWPIX2WINPIX(d,x+r);
	p1.y = YDRAWPIX2WINPIX(d,y-r);
						   
	setDrawBrush( d, color, opts );						  
	if ( noNegDrawArgs > 0 && ( p0.x < 0 || p0.y < 0 ) ) {
		if ( r > MAX_FILLCIRCLE_POINTS )
			cnt = MAX_FILLCIRCLE_POINTS;
		else if ( r > 8 )
			cnt = XDRAWPIX2WINPIX(d,r);
		else
			cnt = 8;
		dang = 360.0/cnt;
		for ( inx=0; inx<cnt; inx++ ) {
			circlePts[inx][0] = x + (int)(r * mswcos( inx*dang ) + 0.5 );
			circlePts[inx][1] = y + (int)(r * mswsin( inx*dang ) + 0.5 );
		}
		//wDrawFilledPolygon( d, circlePts, NULL, cnt, color, opts );
		wDrawPolygon(d, circlePts, NULL, cnt, color, 1, wDrawLineSolid,opts, TRUE, FALSE );
	} else {
		Ellipse( d->hDc, p0.x, p0.y, p1.x, p1.y );
		if (d->hWnd) {
			rect.top = p0.y;
			rect.bottom = p1.y;
			rect.left = p0.x;
			rect.right = p1.x;
			myInvalidateRect( d, &rect );
		}
	}
}

/*
 *****************************************************************************
 *
 * Misc
 *
 *****************************************************************************
 */


void wDrawSaveImage(
		wDraw_p bd )
{
	if ( bd->hBmBackup ) {			
		SelectObject( bd->hDcBackup, bd->hBmBackupOld );
		DeleteObject( bd->hBmBackup );
		bd->hBmBackup = (HBITMAP)0;
	}
	if ( bd->hDcBackup == (HDC)0 )
		bd->hDcBackup = CreateCompatibleDC( bd->hDc ); 
	bd->hBmBackup = CreateCompatibleBitmap( bd->hDc, bd->w, bd->h );
	bd->hBmBackupOld = SelectObject( bd->hDcBackup, bd->hBmBackup );
	BitBlt( bd->hDcBackup, 0, 0, bd->w, bd->h, bd->hDc, 0, 0, SRCCOPY );
}

void wDrawRestoreImage(
		wDraw_p bd )
{
	if ( bd->hBmBackup == (HBITMAP)0 ) {
		mswFail( "wDrawRestoreImage: hBmBackup == 0" );
		return;
	}
	BitBlt( bd->hDc, 0, 0, bd->w, bd->h, bd->hDcBackup, 0, 0, SRCCOPY );
	InvalidateRect( bd->hWnd, NULL, FALSE );
}


void wDrawClearTemp( wDraw_p d )
{
	RECT rect;
	SelectObject( d->hDc, d->hBmTemp );
	BitBlt(d->hDc, 0, 0, d->w, d->h, d->hDc, 0, 0, WHITENESS);
	if (d->hWnd) {
		rect.top = 0;
		rect.bottom = d->h;
		rect.left = 0;
		rect.right = d->w;
		InvalidateRect( d->hWnd, &rect, FALSE );
	}
}


void wDrawClear( wDraw_p d )
{
	SelectObject( d->hDc, d->hBmMain );
	// BitBlt is faster than Rectangle
	BitBlt(d->hDc, 0, 0, d->w, d->h, d->hDc, 0, 0, WHITENESS);
	wDrawClearTemp(d);
}


void wDrawSetSize(
		wDraw_p d,
		wWinPix_t width,
		wWinPix_t height, void * redraw)
{
	d->w = width;
	d->h = height;
	if (!SetWindowPos( d->hWnd, HWND_TOP, 0, 0,
		d->w, d->h, SWP_NOMOVE|SWP_NOZORDER)) {
		mswFail("wDrawSetSize: SetWindowPos");
	}
	/*wRedraw( d );*/
}


void wDrawGetSize(
		wDraw_p d,
		wWinPix_t * width,
		wWinPix_t * height )
{
	*width = d->w-2;
	*height = d->h-2;
}


void * wDrawGetContext( wDraw_p d )
{
	return d->data;
}


double wDrawGetDPI( wDraw_p d )
{
	return d->DPI;
}

double wDrawGetMaxRadius( wDraw_p d )
{
	return 4096.0;
}

void wDrawClip(
		wDraw_p d,
		wDrawPix_t x,
		wDrawPix_t y,
		wDrawPix_t w,
		wDrawPix_t h )
{
	wWinPix_t ix0, iy0, ix1, iy1;
	HRGN hRgnClip;
	ix0 = XDRAWPIX2WINPIX(d,x);
	iy0 = YDRAWPIX2WINPIX(d,y);
	ix1 = XDRAWPIX2WINPIX(d,x+w);
	iy1 = YDRAWPIX2WINPIX(d,y+h);
	/* Note: Ydim is upside down so iy1<iy0 */
	hRgnClip = CreateRectRgn( ix0, iy1, ix1, iy0 );
	SelectClipRgn( d->hDc, hRgnClip );
	DeleteObject( hRgnClip );
}


void wRedraw( wDraw_p d )
{
	wDrawClear( d );
	if (d->drawRepaint)
		d->drawRepaint( d, d->data, 0, 0 );
}

/*
 *****************************************************************************
 *
 * BitMap
 *
 *****************************************************************************
 */

struct wDrawBitMap_t {
		wDrawBitMap_p next;
		wDrawPix_t x;
		wDrawPix_t y;
		wDrawPix_t w;
		wDrawPix_t h;
		char * bmx;
		wDrawColor color;
		HBITMAP bm;
		};
static wDrawBitMap_p bmRoot = NULL;

extern wDrawColor drawColorWhite;
extern wDrawColor drawColorBlack;

void wDrawBitMap(
		wDraw_p d,
		wDrawBitMap_p bm,
		wDrawPix_t px,
		wDrawPix_t py,
		wDrawColor dc,
		wDrawOpts dopt )
{
	HDC bmDc;
	HBITMAP oldBm;
	DWORD mode;
	int x0, y0;
	RECT rect;

	x0 = XDRAWPIX2WINPIX(d,px-bm->x);
	y0 = YDRAWPIX2WINPIX(d,py-bm->y+bm->h);
#ifdef LATER
	if ( noNegDrawArgs > 0 && ( x0 < 0 || y0 < 0 ) )
		return;
#endif
	if (dc == drawColorWhite) {
		mode = clrOp;
		dc = drawColorBlack;
	} else {
		mode = setOp;
	}

	if ( bm->color != dc ) {
		if ( bm->bm )
			DeleteObject( bm->bm );
		bm->bm = mswCreateBitMap( mswGetColor(d->hasPalette,dc) /*colorPalette.palPalEntry[dc]*/, RGB( 255, 255, 255 ),
				RGB( 255, 255, 255 ), (wWinPix_t)bm->w, (wWinPix_t)bm->h, bm->bmx );
		bm->color = dc;
	}

	bmDc = CreateCompatibleDC( d->hDc );
	setDrawMode( d, 0, wDrawLineSolid, dc, dopt );
	oldBm = SelectObject( bmDc, bm->bm );
	BitBlt( d->hDc, x0, y0, (wWinPix_t)bm->w, (wWinPix_t)bm->h, bmDc, 0, 0, mode );
	SelectObject( bmDc, oldBm );
	DeleteDC( bmDc );
	if (d->hWnd) {
	rect.top = y0-1;
	rect.bottom = rect.top+ (wWinPix_t)bm->h+1;
	rect.left = x0-1;
	rect.right = rect.left+ (wWinPix_t)bm->w+1;
	myInvalidateRect( d, &rect );
	}
}


wDrawBitMap_p wDrawBitMapCreate(
		wDraw_p d,
		int w,
		int h,
		int x,
		int y,
		const unsigned char * bits )
{
	wDrawBitMap_p bm;
	int bmSize = ((w+7)/8) * h;
	bm = (wDrawBitMap_p)malloc( sizeof *bm );
	if (bmRoot == NULL) {
		bmRoot = bm;
		bm->next = NULL;
	} else {
		bm->next = bmRoot;
		bmRoot = bm;
	}
	bm->x = x;
	bm->y = y;
	bm->w = w;
	bm->h = h;
	bm->bmx = malloc( bmSize );
	bm->bm = (HBITMAP)0;
	bm->color = -1;
	memcpy( bm->bmx, bits, bmSize );
	/*bm->bm = mswCreateBitMap( GetSysColor(COLOR_BTNTEXT), RGB( 255, 255, 255 ), w, h, bits );*/
	return bm;
}

/*
 *****************************************************************************
 *
 * Create
 *
 *****************************************************************************
 */

static int doSetFocus = 1;

LRESULT FAR PASCAL XEXPORT mswDrawPush(
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam )
{
	wIndex_t inx = (wIndex_t)GetWindowLongPtr( hWnd, GWL_ID );
	wDraw_p b;
	wWinPix_t ix, iy;
	wDrawPix_t x, y;
	HDC hDc;
	PAINTSTRUCT ps;
	wAction_t action;
	RECT rect;
	HWND activeWnd;
	HWND focusWnd;
	wAccelKey_e extChar;

	switch( message ) {
	case WM_CREATE:
		b = (wDraw_p)mswMapIndex( inx );
		hDc = GetDC(hWnd);
		if ( b->option & BD_DIRECT ) {
			b->hDc = hDc;
			b->hBmMain = 0;
			b->hBmTemp = 0;
			b->hBmOld = 0;
		} else {
			b->hDc = CreateCompatibleDC( hDc ); 
			b->hBmMain = CreateCompatibleBitmap( hDc, b->w, b->h );
			b->hBmTemp = CreateCompatibleBitmap( hDc, b->w, b->h );
			b->hBmOld = SelectObject( b->hDc, b->hBmMain );
		}
		if (mswPalette) {
			SelectPalette( b->hDc, mswPalette, 0 );
			RealizePalette( b->hDc );
		}
		b->wFactor = (double)GetDeviceCaps( b->hDc, LOGPIXELSX );
		b->hFactor = (double)GetDeviceCaps( b->hDc, LOGPIXELSY );
		double dpi;
		/** @prefs [Preference] ScreenDPI=96.0 Sets DPI of screen */
		wPrefGetFloat(PREFSECTION, DPISET, &dpi, 96.0);
		b->DPI = dpi;
		b->hWnd = hWnd;
		SetROP2( b->hDc, R2_WHITE );
		Rectangle( b->hDc, 0, 0, b->w, b->h );
		if ( (b->option & BD_DIRECT) == 0 ) {
			SetROP2( hDc, R2_WHITE );
			Rectangle( hDc, 0, 0, b->w, b->h );
			ReleaseDC( hWnd, hDc );
		}
		break;
	case WM_SIZE:
		b = (wDraw_p)mswMapIndex( inx );
		ix = LOWORD( lParam );
		iy = HIWORD( lParam );
		b->w = ix+2;
		b->h = iy+2;
		if (b->hWnd) {
			if ( b->option & BD_DIRECT ) {
			} else {
			hDc = GetDC( b->hWnd );
//-			DeleteObject( b->hBmOld );
			DeleteObject( b->hBmMain );
			DeleteObject( b->hBmTemp );
			b->hBmMain = CreateCompatibleBitmap( hDc, b->w, b->h );
			b->hBmTemp = CreateCompatibleBitmap( hDc, b->w, b->h );
//-			b->hBmOld = SelectObject( b->hDc, b->hBmMain );
			ReleaseDC( b->hWnd, hDc );
			SetROP2( b->hDc, R2_WHITE );
			Rectangle( b->hDc, 0, 0, b->w, b->h );
			}
		}
		/*if (b->drawResize)
			b->drawResize( b, b->size );*/
		if (b->drawRepaint)
			b->drawRepaint( b, b->data, 0, 0 );
		return (LRESULT)0;
	case WM_MOUSEMOVE:
		activeWnd = GetActiveWindow();
		focusWnd = GetFocus();
		if (focusWnd != hWnd) {
			b = (wDraw_p)mswMapIndex( inx );
			if (!b)
				break;
			if ( !((wControl_p)b->parent) )
				break;
			if ( ((wControl_p)b->parent)->hWnd != activeWnd )
				break;
		}
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_LBUTTONDBLCLK:
		if (message == WM_LBUTTONDOWN)
			action = wActionLDown;
		else if (message == WM_RBUTTONDOWN)
			action = wActionRDown;
		else if (message == WM_LBUTTONUP)
			action = wActionLUp;
		else if (message == WM_RBUTTONUP)
			action = wActionRUp;
		else if (message == WM_LBUTTONDBLCLK)
			action = wActionLDownDouble;
		else {
			if ( (wParam & MK_LBUTTON) != 0)
				action = wActionLDrag;
			else if ( (wParam & MK_RBUTTON) != 0)
				action = wActionRDrag;
			else
				action = wActionMove;
		}
		b = (wDraw_p)mswMapIndex( inx );
		if (!b)
			break;
		if (doSetFocus && message != WM_MOUSEMOVE)
			SetFocus( ((wControl_p)b->parent)->hWnd );
		if ( (b->option&BD_NOCAPTURE) == 0 ) {
			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN)
				SetCapture( b->hWnd );
			else if (message == WM_LBUTTONUP || message == WM_RBUTTONUP)
				ReleaseCapture();
		}
		ix = LOWORD( lParam );
		iy = HIWORD( lParam );
		x = XWINPIX2DRAWPIX( b, ix );
		y = YWINPIX2DRAWPIX( b, iy );
		b->lastX = x;
		b->lastY = y;
		if (b->action)
			b->action( b, b->data, action, x, y );
		if (b->hWnd)
			UpdateWindow(b->hWnd);
		return (LRESULT)0;
	case WM_CHAR:
		b = (wDraw_p)mswMapIndex( inx );
		extChar = wAccelKey_None;
		if (lParam & 0x01000000L)
		switch( wParam ) {
		case VK_DELETE: extChar = wAccelKey_Del; break;
		case VK_INSERT: extChar = wAccelKey_Ins; break;
		case VK_HOME:	extChar = wAccelKey_Home; break;
		case VK_END:	extChar = wAccelKey_End; break;
		case VK_PRIOR:	extChar = wAccelKey_Pgup; break;
		case VK_NEXT:	extChar = wAccelKey_Pgdn; break;
		case VK_UP:		extChar = wAccelKey_Up; break;
		case VK_DOWN:	extChar = wAccelKey_Down; break;
		case VK_RIGHT:	extChar = wAccelKey_Right; break;				
		case VK_LEFT:	extChar = wAccelKey_Left; break;
		case VK_BACK:	extChar = wAccelKey_Back; break;
		case VK_F1:		extChar = wAccelKey_F1; break;
		case VK_F2:		extChar = wAccelKey_F2; break;
		case VK_F3:		extChar = wAccelKey_F3; break;
		case VK_F4:		extChar = wAccelKey_F4; break;
		case VK_F5:		extChar = wAccelKey_F5; break;
		case VK_F6:		extChar = wAccelKey_F6; break;
		case VK_F7:		extChar = wAccelKey_F7; break;
		case VK_F8:		extChar = wAccelKey_F8; break;
		case VK_F9:		extChar = wAccelKey_F9; break;
		case VK_F10:	extChar = wAccelKey_F10; break;
		case VK_F11:	extChar = wAccelKey_F11; break;
		case VK_F12:	extChar = wAccelKey_F12; break;
		}
		if (b && b->action) {
			if (extChar != wAccelKey_None)
				b->action( b, b->data, wActionExtKey + ( (int)extChar << 8 ), b->lastX, b->lastY );
			else
				b->action( b, b->data, wActionText + ( (int)wParam << 8 ), b->lastX, b->lastY );
		}
		return (LRESULT)0;

	case WM_PAINT:
		b = (wDraw_p)mswMapIndex( inx );
		if (b && b->type == B_DRAW) {
			if (GetUpdateRect( b->hWnd, &rect, FALSE )) {
				hDc = BeginPaint( hWnd, &ps );
				if ( b->hasPalette ) {
					int winPaletteClock = mswGetPaletteClock();
					if ( b->paletteClock < winPaletteClock ) {
						RealizePalette( hDc );
						b->paletteClock = winPaletteClock;
					}
				}
				HBITMAP hBmOld = SelectObject( b->hDc, b->hBmMain );

			if (bDrawMainBM) {
				BitBlt(hDc, rect.left, rect.top,
					rect.right - rect.left, rect.bottom - rect.top,
					b->hDc, rect.left, rect.top,
					SRCCOPY);
			}
				SelectObject( b->hDc, b->bCopiedMain?b->hBmTemp:b->hBmMain );
				BitBlt( hDc, rect.left, rect.top,
						rect.right-rect.left, rect.bottom-rect.top,
						b->hDc, rect.left, rect.top,
						bDrawMainBM?SRCAND:SRCCOPY);
				SelectObject( b->hDc, hBmOld );
				EndPaint( hWnd, &ps );
				b->bCopiedMain = FALSE;
			}
		}
		break;
	case WM_DESTROY:
		b = (wDraw_p)mswMapIndex( inx );
		if (b && b->type == B_DRAW) {
			if (b->hDc) {
				DeleteDC( b->hDc );
				b->hDc = (HDC)0;
			}
			if (b->hDcBackup) {
				DeleteDC( b->hDcBackup );
				b->hDcBackup = (HDC)0;
			}
		}
		break;
	default:
		break;
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}


static LRESULT drawMsgProc( wDraw_p b, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	wAction_t action;
	
	switch( message ) {
	case WM_MOUSEWHEEL:
		/* handle mouse wheel events */
		if (GET_KEYSTATE_WPARAM(wParam) & (MK_SHIFT|MK_MBUTTON) ) {
			if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL ) {
				if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
					action = wActionScrollLeft;
				} else {
					action = wActionScrollRight;
				}
			} else {
				if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
					action = wActionScrollUp;
				} else {
					action = wActionScrollDown;
				}
			}
		} else {
			if (GET_WHEEL_DELTA_WPARAM(wParam) > 0) {
				action = wActionWheelUp;
			} else {
				action = wActionWheelDown;
			}
		}
		if (b->action)
			b->action( b, b->data, action, b->lastX, b->lastY );
		return (LRESULT)0;
	case WM_MOUSEHWHEEL:
		if ( GET_KEYSTATE_WPARAM(wParam) & (MK_SHIFT|MK_MBUTTON)) {
			if ( GET_WHEEL_DELTA_WPARAM(wParam) > 0 ) {
				action = wActionScrollRight;
			} else {
				action = wActionScrollLeft;
			}
		}
		if (b->action)
			b->action( b, b->data, action, b->lastX, b->lastY );
		return (LRESULT)0;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}


static void drawDoneProc( wControl_p b )
{
	wDraw_p d = (wDraw_p)b;
	if (d->hBmMain) {
		SelectObject( d->hDc, d->hBmOld );
		DeleteObject( d->hBmMain );
		d->hBmMain = (HBITMAP)0;
		DeleteObject( d->hBmTemp );
		d->hBmTemp = (HBITMAP)0;
	}
	if (d->hPen) {
		SelectObject( d->hDc, GetStockObject( BLACK_PEN ) );
		DeleteObject( d->hPen );
		d->hPen = (HPEN)0;
	}
	if (d->hBrush) {
		SelectObject( d->hDc, GetStockObject( BLACK_BRUSH) );
		DeleteObject( d->hBrush );
		d->hBrush = (HBRUSH)0;
	}
	if (d->hDc) {
		DeleteDC( d->hDc );
		d->hDc = (HDC)0;
	}
	if ( d->hDcBackup ) {
		DeleteDC( d->hDcBackup );
		d->hDcBackup = (HDC)0;
	}
	while (bmRoot) {
		if (bmRoot->bm)
			DeleteObject( bmRoot->bm );
		bmRoot = bmRoot->next;
	}
}


static callBacks_t drawCallBacks = {
		NULL,
		drawDoneProc,
		(messageCallback_p)drawMsgProc };

static wDraw_p drawList = NULL;


void mswRedrawAll( void )
{
	wDraw_p p;
	for ( p=drawList; p; p=p->drawNext ) {
		if (p->drawRepaint)
			p->drawRepaint( p, p->data, 0, 0 );
	}
}


void mswRepaintAll( void )
{
	wDraw_p b;
	HDC hDc;
	RECT rect;
	PAINTSTRUCT ps;

	for ( b=drawList; b; b=b->drawNext ) {
		if (GetUpdateRect( b->hWnd, &rect, FALSE )) {
			hDc = BeginPaint( b->hWnd, &ps );
			HBITMAP hBmOld = SelectObject( b->hDc, b->hBmMain );
			BitBlt( hDc, rect.left, rect.top,
						rect.right-rect.left, rect.bottom-rect.top,
						b->hDc, rect.left, rect.top,
						SRCCOPY );
			SelectObject( b->hDc, b->hBmTemp );
			BitBlt( hDc, rect.left, rect.top,
						rect.right-rect.left, rect.bottom-rect.top,
						b->hDc, rect.left, rect.top,
						SRCAND );
			SelectObject( b->hDc, hBmOld );
			EndPaint( b->hWnd, &ps );
		}
	}
}


wDraw_p wDrawCreate(
		wWin_p parent,
		wWinPix_t x,
		wWinPix_t y,
		const char * helpStr,
		long option,
		wWinPix_t w,
		wWinPix_t h,
		void * data,
		wDrawRedrawCallBack_p redrawProc,
		wDrawActionCallBack_p action )
{
	wDraw_p d;
	RECT rect;
	int index;
	HDC hDc;

	if ( noNegDrawArgs < 0 ) {
		wPrefGetInteger( "msw tweak", "NoNegDrawArgs", &noNegDrawArgs, 0 );
		wPrefGetInteger( "msw tweak", "NoFlatEndCaps", &noFlatEndCaps, 0 );
	}

	d = mswAlloc( parent, B_DRAW, NULL, sizeof *d, data, &index );
	mswComputePos( (wControl_p)d, x, y );
	d->w = w;
	d->h = h;
	d->drawRepaint = NULL;
	d->action = action;
	d->option = option;

	d->hWnd = CreateWindow( mswDrawWindowClassName, NULL,
				WS_CHILDWINDOW|WS_VISIBLE|WS_BORDER,
				d->x, d->y, w, h,
				((wControl_p)parent)->hWnd, (HMENU)(UINT_PTR)index, mswHInst, NULL );

	if (d->hWnd == (HWND)0) {
		mswFail( "CreateWindow(DRAW)" );
		return d;
	}

	GetWindowRect( d->hWnd, &rect );

	d->w = rect.right - rect.left;
	d->h = rect.bottom - rect.top;
	d->drawRepaint = redrawProc;
	/*if (d->drawRepaint)
		d->drawRepaint( d, d->data, 0.0, 0.0 );*/

	mswAddButton( (wControl_p)d, FALSE, helpStr );
	mswCallBacks[B_DRAW] = &drawCallBacks;
	d->drawNext = drawList;
	drawList = d;
	if (mswPalette) {
		hDc = GetDC( d->hWnd );
		d->hasPalette = TRUE;
		SelectPalette( hDc, mswPalette, 0 );
		ReleaseDC( d->hWnd, hDc );
	}
	d->bCopiedMain = FALSE;
	return d;
}

/*
 *****************************************************************************
 *
 * Bitmaps
 *
 *****************************************************************************
 */

wDraw_p wBitMapCreate( wWinPix_t w, wWinPix_t h, int planes )
{
	wDraw_p d;
	HDC hDc;

	d = (wDraw_p)calloc(1,sizeof *d);
	d->type = B_DRAW;
	d->shown = TRUE;
	d->x = 0;
	d->y = 0;
	d->w = w;
	d->h = h;
	d->drawRepaint = NULL;
	d->action = NULL;
	d->option = 0;

	hDc = GetDC(mswHWnd);
	d->hDc = CreateCompatibleDC( hDc ); 
	if ( d->hDc == (HDC)0 ) {
		wNoticeEx( NT_ERROR, "CreateBitMap: CreateDC fails", "Ok", NULL );
		return FALSE;
	}
	d->hBmMain = CreateCompatibleBitmap( hDc, d->w, d->h );
	if ( d->hBmMain == (HBITMAP)0 ) {
		wNoticeEx( NT_ERROR, "CreateBitMap: CreateBM Main fails", "Ok", NULL );
		ReleaseDC(mswHWnd, hDc);
		return FALSE;
	}
	d->hBmTemp = CreateCompatibleBitmap( hDc, d->w, d->h );
	if ( d->hBmTemp == (HBITMAP)0 ) {
		wNoticeEx( NT_ERROR, "CreateBitMap: CreateBM Temp fails", "Ok", NULL );
		ReleaseDC(mswHWnd, hDc);
		return FALSE;
	}
	d->hasPalette = (GetDeviceCaps(hDc,RASTERCAPS ) & RC_PALETTE) != 0;
	ReleaseDC( mswHWnd, hDc );
	d->hBmOld = SelectObject( d->hDc, d->hBmMain );
	if (mswPalette) {
		SelectPalette( d->hDc, mswPalette, 0 );
		RealizePalette( d->hDc );
	}
	d->wFactor = (double)GetDeviceCaps( d->hDc, LOGPIXELSX );
	d->hFactor = (double)GetDeviceCaps( d->hDc, LOGPIXELSY );
	d->DPI = 96.0; /*min( d->wFactor, d->hFactor );*/
	d->hWnd = 0;
	wDrawClear(d);
//-	SetROP2( d->hDc, R2_WHITE );
//-	Rectangle( d->hDc, 0, 0, d->w, d->h );
	return d;
}

wBool_t wBitMapDelete( wDraw_p d )
{
	if (d->hPen) {
		SelectObject( d->hDc, GetStockObject( BLACK_PEN ) );
		DeleteObject( d->hPen );
		d->hPen = (HPEN)0;
	}
	if (d->hBmMain) {
		SelectObject( d->hDc, d->hBmOld );
		DeleteObject( d->hBmMain );
		d->hBmMain = (HBITMAP)0;
		DeleteObject( d->hBmTemp );
		d->hBmTemp = (HBITMAP)0;
	}
	if (d->hDc) {
		DeleteDC( d->hDc );
		d->hDc = (HDC)0;
	}
	free(d);
	return TRUE;
}

/**
 * write bitmap file. The bitmap in d must contain a valid HBITMAP
 *
 * \param  d	    A wDraw_p to process.
 * \param  fileName Filename of the file.
 *
 * \returns A wBool_t. TRUE on success
 */

wBool_t
wBitMapWriteFile(wDraw_p d, const char * fileName)
{
    FIBITMAP *dib = NULL;
    FIBITMAP *dib2 = NULL;
    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
    BOOL bSuccess = FALSE;

    if (d->hBmMain) {

        BITMAP bm;
        GetObject(d->hBmMain, sizeof(BITMAP), (LPSTR)&bm);
        dib = FreeImage_Allocate(bm.bmWidth, bm.bmHeight, bm.bmBitsPixel, 0, 0, 0);
        // The GetDIBits function clears the biClrUsed and biClrImportant BITMAPINFO members (dont't know why)
        // So we save these infos below. This is needed for palettized images only.
        int nColors = FreeImage_GetColorsUsed(dib);
        HDC dc = GetDC(NULL);
        GetDIBits(dc,
                  d->hBmMain,
                  0,
                  FreeImage_GetHeight(dib),
                  FreeImage_GetBits(dib),
                  FreeImage_GetInfo(dib),
                  DIB_RGB_COLORS);
        ReleaseDC(NULL, dc);

        // restore BITMAPINFO members
        FreeImage_GetInfoHeader(dib)->biClrUsed = nColors;
        FreeImage_GetInfoHeader(dib)->biClrImportant = nColors;
        // we will get a 32 bit bitmap on Windows systems with invalid alpha
        // so it needs to be converted to 24 bits.
        // (see: https://sourceforge.net/p/freeimage/discussion/36110/thread/0699ce8e/ )
        dib2 = FreeImage_ConvertTo24Bits(dib);
        FreeImage_Unload(dib);
    }

    // Try to guess the file format from the file extension
    fif = FreeImage_GetFIFFromFilename(fileName);
    if (fif != FIF_UNKNOWN) {
        // Check that the dib can be saved in this format
        BOOL bCanSave;

        FREE_IMAGE_TYPE image_type = FreeImage_GetImageType(dib2);
        if (image_type == FIT_BITMAP) {
            // standard bitmap type
            WORD bpp = FreeImage_GetBPP(dib2);
            bCanSave = (FreeImage_FIFSupportsWriting(fif) &&
                        FreeImage_FIFSupportsExportBPP(fif, bpp));
        } else {
            // special bitmap type
            bCanSave = FreeImage_FIFSupportsExportType(fif, image_type);
        }

        if (bCanSave) {
            bSuccess = FreeImage_Save(fif, dib2, fileName, PNG_DEFAULT);
        }
    }
    FreeImage_Unload(dib2);

    return bSuccess;
}

