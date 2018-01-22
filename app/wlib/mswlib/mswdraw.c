/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/mswlib/mswdraw.c,v 1.6 2009-05-15 18:16:16 m_fischer Exp $
 */

#define _WIN32_WINNT 0x0500		/* for wheel mouse supposrt */
#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <winuser.h>

#ifdef WIN32
#define wFont_t tagLOGFONTA
#else
#define wFont_t tagLOGFONT
#endif
#include "mswint.h"

/*
 *****************************************************************************
 *
 * Draw
 *
 *****************************************************************************
 */

static wBool_t initted = FALSE;

long wDebugFont;

static FARPROC oldDrawProc;


static long tmpOp = 0x990066;
static long setOp = 0x8800c6;
static long clrOp = 0xbb0226;

#define CENTERMARK_LENGTH 6

#ifdef SLOW
static wPos_t XPIX2INCH( wDraw_p d, int ix )
{
	return (wPos_t)ix;
}

static wPos_t YPIX2INCH( wDraw_p d, int iy )
{
	wPos_t y;
	y = (wPos_t)(d->h-2-iy);
	return y;
}

static int XINCH2PIX( wDraw_p d, wPos_t xx )
{
	int ix;
	ix = (int)(xx);
	return ix;
}

static int YINCH2PIX( wDraw_p d, wPos_t y )
{
	int iy;
	iy = d->h-2 - (int)(y);
	return iy;
}


static wPos_t XPIXELSTOINCH( wDraw_p d, int ix )
{
	return (wPos_t)ix;
}


static wPos_t YPIXELSTOINCH( wDraw_p d, int iy )
{
	return (wPos_t)iy;
}
#else
#define XPIX2INCH( d, ix ) \
	((wPos_t)ix)

#define YPIX2INCH( d, iy ) \
	((wPos_t)(d->h-2-iy))

#define XINCH2PIX( d, xx ) \
	((int)(xx))

#define YINCH2PIX( d, y ) \
	(d->h-2 - (int)(y))


#define XPIXELSTOINCH( d, ix ) \
	((wPos_t)ix)


#define YPIXELSTOINCH( d, iy ) \
	((wPos_t)iy)

#endif

/*
 *****************************************************************************
 *
 * Basic Line Draw
 *
 *****************************************************************************
 */



static long noNegDrawArgs = -1;
static long noFlatEndCaps = 0;

void wDrawDelayUpdate(
		wDraw_p d,
		wBool_t delay )
{
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
		HDC hDc,
		wDraw_p d,
		wDrawWidth dw,
		wDrawLineType_e lt,
		wDrawColor dc,
		wDrawOpts dopt )
{
	int mode;
	HPEN hOldPen;
	static wDraw_p d0;
	static wDrawWidth dw0 = -1;
	static wDrawLineType_e lt0 = (wDrawLineType_e)-1;
	static wDrawColor dc0 = -1;
	static int mode0 = -1;
	static LOGBRUSH logBrush = { 0, 0, 0 };
	DWORD penStyle;

	if ( d->hasPalette ) {
		int winPaletteClock = mswGetPaletteClock();
		if ( d->paletteClock < winPaletteClock ) {
			RealizePalette( hDc );
			d->paletteClock = winPaletteClock;
		}
	}

	if (dopt & wDrawOptTemp) {
		mode = R2_NOTXORPEN;
	} else {
		mode = R2_COPYPEN;
	}
	SetROP2( hDc, mode );
	if ( d == d0 && mode == mode0 && dw0 == dw && lt == lt0 && dc == dc0 )
		return;

	// make sure that the line width is at least 1!
	if( !dw ) 
		dw++;

	d0 = d; mode0 = mode; dw0 = dw; lt0 = lt; dc0 = dc;

	logBrush.lbColor = mswGetColor(d->hasPalette,dc);
	if ( lt==wDrawLineSolid ) {
		penStyle = PS_GEOMETRIC | PS_SOLID;
		if ( noFlatEndCaps == FALSE )
			penStyle |= PS_ENDCAP_FLAT;
		d->hPen = ExtCreatePen( penStyle,
				dw,
				&logBrush,
				0,
				NULL );
				/*colorPalette.palPalEntry[dc] );*/
	} else {
		d->hPen = CreatePen( PS_DOT, 0, mswGetColor( d->hasPalette, dc ) );
	}
	hOldPen = SelectObject( hDc, d->hPen );
	DeleteObject( hOldPen );
}

static void setDrawBrush(
		HDC hDc,
		wDraw_p d,
		wDrawColor dc,
		wDrawOpts dopt )
{
	HBRUSH hOldBrush;
	static wDraw_p d0;
	static wDrawColor dc0 = -1;

	setDrawMode( hDc, d, 0, wDrawLineSolid, dc, dopt );
	if ( d == d0 && dc == dc0 )
		return;

	d0 = d; dc0 = dc;

	d->hBrush = CreateSolidBrush( 
				mswGetColor(d->hasPalette,dc) );
	hOldBrush = SelectObject( hDc, d->hBrush );
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
		wPos_t p0x,
		wPos_t p0y,
		wPos_t p1x,
		wPos_t p1y,
		wDrawWidth dw,
		wDrawLineType_e lt,
		wDrawColor dc,
		wDrawOpts dopt )
{
	POINT p0, p1;
	RECT rect;
	setDrawMode( d->hDc, d, dw, lt, dc, dopt );
	p0.x = XINCH2PIX(d,p0x);
	p0.y = YINCH2PIX(d,p0y);
	p1.x = XINCH2PIX(d,p1x);
	p1.y = YINCH2PIX(d,p1y);
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
		wPos_t px,
		wPos_t py,
		wPos_t r,
		double a0,
		double a1,
		int drawCenter,
		wDrawWidth dw,
		wDrawLineType_e lt,
		wDrawColor dc,
		wDrawOpts dopt )
{
	int i, cnt;
	POINT p0, p1, ps, pe, pp0, pp1, pp2, pc;
	double psx, psy, pex, pey, len, aa;
	RECT rect;
	int needMoveTo;
	wBool_t fakeArc = FALSE;

	len = a1/360.0 * (2 * M_PI) * r;
	if (len < 3)
		return;

	p0.x = XINCH2PIX(d,px-r);
	p0.y = YINCH2PIX(d,py+r)+1;
	p1.x = XINCH2PIX(d,px+r);
	p1.y = YINCH2PIX(d,py-r)+1;

	pex = px + r * mswsin(a0);
	pey = py + r * mswcos(a0);
	psx = px + r * mswsin(a0+a1);
	psy = py + r * mswcos(a0+a1);

	/*pointOnCircle( &pe, p, r, a0 );
	pointOnCircle( &ps, p, r, a0+a1 );*/
	ps.x = XINCH2PIX(d,(wPos_t)psx);
	ps.y = YINCH2PIX(d,(wPos_t)psy);
	pe.x = XINCH2PIX(d,(wPos_t)pex);
	pe.y = YINCH2PIX(d,(wPos_t)pey);

	setDrawMode( d->hDc, d, dw, lt, dc, dopt );

	if (dw == 0)
		dw = 1;

	if (r>4096) {
		/* The book says 32K but experience says otherwise */
		fakeArc = TRUE;
	}
	if ( noNegDrawArgs > 0 ) {
		if ( p0.x < 0 || p0.y < 0 || p1.x < 0 || p1.y < 0 )
			fakeArc = TRUE;
	}
	if ( fakeArc ) {
		cnt = (int)a1;
		if ( cnt <= 0 ) cnt = 1;
		if ( cnt > 360 ) cnt = 360;
		aa = a1 / cnt;
		psx = px + r * mswsin(a0);
		psy = py + r * mswcos(a0);
		pp0.x = XINCH2PIX( d, (wPos_t)psx );
		pp0.y = YINCH2PIX( d, (wPos_t)psy );
		needMoveTo = TRUE;
		for ( i=0; i<cnt; i++ ) {
			a0 += aa;
			psx = px + r * mswsin(a0);
			psy = py + r * mswcos(a0);
			pp2.x = pp1.x = XINCH2PIX( d, (wPos_t)psx );
			pp2.y = pp1.y = YINCH2PIX( d, (wPos_t)psy );
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
			Arc( d->hDc, p0.x, p1.y, p1.x, p0.y, ps.x, p0.y-1, pe.x, p1.y-1 );
			Arc( d->hDc, p0.x, p1.y, p1.x, p0.y, ps.x, p1.y-1, pe.x, p0.y-1 );
		} else {
			Arc( d->hDc, p0.x, p1.y, p1.x, p0.y, ps.x, ps.y, pe.x, pe.y );
		}
	}

	// should the center of the arc be drawn?
	if( drawCenter ) {
			
			// calculate the center coordinates
			pc.x = XINCH2PIX( d, px );
			pc.y = YINCH2PIX( d, py );
			// now draw the crosshair
			MoveTo( d->hDc, pc.x - CENTERMARK_LENGTH/2, pc.y );
			LineTo( d->hDc, pc.x + CENTERMARK_LENGTH/2, pc.y );
			MoveTo( d->hDc, pc.x, pc.y - CENTERMARK_LENGTH/2 );
			LineTo( d->hDc, pc.x, pc.y + CENTERMARK_LENGTH/2 );
			
			// invalidate the area of the crosshair
			rect.top  = pc.y - CENTERMARK_LENGTH / 2 - 1;
			rect.bottom  = pc.y + CENTERMARK_LENGTH / 2 + 1;
			rect.left = pc.x - CENTERMARK_LENGTH / 2 - 1;
			rect.right = pc.x + CENTERMARK_LENGTH / 2 + 1;
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
		wPos_t px,
		wPos_t py,
		wDrawColor dc,
		wDrawOpts dopt )
{
	POINT p0;
	RECT rect;

	p0.x = XINCH2PIX(d,px);
	p0.y = YINCH2PIX(d,py);

	if ( p0.x < 0 || p0.y < 0 )
		return;
	if ( p0.x >= d->w || p0.y >= d->h )
		return;
	setDrawMode( d->hDc, d, 0, wDrawLineSolid, dc, dopt );

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
	face = wPrefGetString( "msw window font", "face" );
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
		wPos_t *w,
		wPos_t *h,
		wPos_t *d,
		wDraw_p bd,
		const char * text,
		wFont_p fp,
		double siz )
{
	int x, y;
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
	extent = GetTextExtent( bd->hDc, CAST_AWAY_CONST text, strlen(text) );

	GetTextMetrics(bd->hDc, &textMetric);

	x = LOWORD(extent);
	y = HIWORD(extent);
	*w = XPIXELSTOINCH( bd, x );
	*h = YPIXELSTOINCH( bd, y );
	*d = YPIXELSTOINCH(bd, textMetric.tmDescent );

	SelectObject( bd->hDc, prevFont );
	DeleteObject( newFont );
	fp->lfHeight = oldLfHeight;
}

void wDrawString(
		wDraw_p d,
		wPos_t px,
		wPos_t py,
		double angle,
		const char * text,
		wFont_p fp,
		double siz,
		wDrawColor dc,
		wDrawOpts dopts )
{
	int x, y;
	HFONT newFont, prevFont;
	HDC newDc;
	HBITMAP oldBm, newBm;
	DWORD extent;
	int w, h;
	RECT rect;
	int oldLfHeight;

	if (fp == NULL)
		fp = &logFont;
	oldLfHeight = fp->lfHeight;
	fp->lfEscapement = (int)(angle*10.0);
	fp->lfHeight = computeFontSize( d, siz );
	fp->lfWidth = 0;
	newFont = CreateFontIndirect( fp );
	x = XINCH2PIX(d,px) + (int)(mswsin(angle)*fp->lfHeight-0.5);
	y = YINCH2PIX(d,py) + (int)(mswcos(angle)*fp->lfHeight-0.5);
	if ( noNegDrawArgs > 0 && ( x < 0 || y < 0 ) )
		return;
	if (dopts & wDrawOptTemp) {
		setDrawMode( d->hDc, d, 0, wDrawLineSolid, dc, dopts );
		newDc = CreateCompatibleDC( d->hDc );
		prevFont = SelectObject( newDc, newFont );
		extent = GetTextExtent( newDc, CAST_AWAY_CONST text, strlen(text) );
		w = LOWORD(extent);
		h = HIWORD(extent);
		if ( h > w ) w = h;
		newBm = CreateCompatibleBitmap( d->hDc, w*2, w*2 );
		oldBm = SelectObject( newDc, newBm ); 
		rect.top = rect.left = 0;
		rect.bottom = rect.right = w*2;
		FillRect( newDc, &rect, GetStockObject(WHITE_BRUSH) );
		TextOut( newDc, w, w, text, strlen(text) );
		BitBlt( d->hDc, x-w, y-w, w*2, w*2, newDc, 0, 0, tmpOp );
		SelectObject( newDc, oldBm );
		DeleteObject( newBm );
		SelectObject( newDc, prevFont );
		DeleteDC( newDc );
		if (d->hWnd) {
		rect.top = y-(w+1);
		rect.bottom = y+(w+1);
		rect.left = x-(w+1);
		rect.right = x+(w+1);
		myInvalidateRect( d, &rect );
		}
#ifdef LATER
		/* KLUDGE: Can't Invert text, so we just draw a bow - a pox on windows*/
		MoveTo( d->hDc, x, y );
		LineTo( d->hDc, x+w, y );
		LineTo( d->hDc, x+w, y+h );
		LineTo( d->hDc, x, y+h );
		LineTo( d->hDc, x, y );
#endif
	} else {
		prevFont = SelectObject( d->hDc, newFont );
		SetBkMode( d->hDc, TRANSPARENT );
		if (dc != wDrawColorBlack) {
			COLORREF old;
			old = SetTextColor( d->hDc, mswGetColor(d->hasPalette,dc)/*colorPalette.palPalEntry[dc]*/ );
			TextOut( d->hDc, x, y, text, strlen(text) );
			SetTextColor( d->hDc, old );
		} else
			TextOut( d->hDc, x, y, text, strlen(text) );
		extent = GetTextExtent( d->hDc, CAST_AWAY_CONST text, strlen(text) );
		SelectObject( d->hDc, prevFont );
		w = LOWORD(extent);
		h = HIWORD(extent);
		if (d->hWnd) {
		rect.top = y-(w+h+1);
		rect.bottom = y+(w+h+1);
		rect.left = x-(w+h+1);
		rect.right = x+(w+h+1);
		myInvalidateRect( d, &rect );
		}
	}
	DeleteObject( newFont );
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

void wSetSelectedFontSize(int size)
{
	fontSize = (wFontSize_t)size;
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
		wPos_t px,
		wPos_t py,
		wPos_t sx,
		wPos_t sy,
		wDrawColor color,
		wDrawOpts opts )
{
	RECT rect;
	if (d == NULL)
		return;
	setDrawBrush( d->hDc, d, color, opts );
	rect.left = XINCH2PIX(d,px);
	rect.right = XINCH2PIX(d,px+sx);
	rect.top = YINCH2PIX(d,py+sy);
	rect.bottom = YINCH2PIX(d,py);
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
static int wFillPointsMax = 0;
static POINT * wFillPoints;

static void addPoint(
		int * pk,
		POINT * pp,
		RECT * pr )
{
#ifdef DRAWFILLPOLYLOG
fprintf( logF, "	q[%d] = {%d,%d}\n", *pk, pp->x, pp->y );
#endif
	if ( *pk > 0 &&
		 wFillPoints[(*pk)-1].x == pp->x && wFillPoints[(*pk)-1].y == pp->y )
		return;
	wFillPoints[ (*pk)++ ] = *pp;
	if (pp->x<pr->left)
		pr->left = pp->x;
	if (pp->x>pr->right)
		pr->right = pp->x;
	if (pp->y<pr->top)
		pr->top = pp->y;
	if (pp->y>pr->bottom)
		pr->bottom = pp->y;
}

void wDrawFilledPolygon(
		wDraw_p d,
		wPos_t p[][2],
		int cnt,
		wDrawColor color,
		wDrawOpts opts )
{				 
	RECT rect;
	int i, k;
	POINT p0, p1, q0, q1;
	static POINT zero = { 0, 0 };
	wBool_t p1Clipped;

	if (d == NULL)
		return;
		if (cnt*2 > wFillPointsMax) {
				wFillPoints = realloc( wFillPoints, cnt * 2 * sizeof *(POINT*)NULL );
				wFillPointsMax = cnt*2;
		}
	setDrawBrush( d->hDc, d, color, opts );
	p1.x = rect.left = rect.right = XINCH2PIX(d,p[cnt-1][0]-1);
	p1.y = rect.top = rect.bottom = YINCH2PIX(d,p[cnt-1][1]+1);
#ifdef DRAWFILLPOLYLOG
logF = fopen( "log.txt", "a" );
fprintf( logF, "\np[%d] = {%d,%d}\n", cnt-1, p1.x, p1.y );
#endif
	p1Clipped = FALSE;
	for ( i=k=0; i<cnt; i++ ) {
		p0 = p1;
		p1.x = XINCH2PIX(d,p[i][0]-1);
		p1.y = YINCH2PIX(d,p[i][1]+1);
#ifdef DRAWFILLPOLYLOG
fprintf( logF, "p[%d] = {%d,%d}\n", i, p1.x, p1.y );
#endif
		q0 = p0;
		q1 = p1;
		if ( clip0( &q0, &q1, NULL ) ) {
#ifdef DRAWFILLPOLYLOG
fprintf( logF, "  clip( {%d,%d} {%d,%d} )  = {%d,%d} {%d,%d}\n", p0.x, p0.y, p1.x, p1.y, q0.x, q0.y, q1.x, q1.y );
#endif
			if ( q0.x != p0.x || q0.y != p0.y ) {
				if ( k > 0 && ( q0.x > q0.y ) != ( wFillPoints[k-1].x > wFillPoints[k-1].y ) )
					 addPoint( &k, &zero, &rect );
				addPoint( &k, &q0, &rect );
			}
			addPoint( &k, &q1, &rect );
			p1Clipped = ( q1.x != p1.x || q1.y != p1.y );
		}
	}
	if ( p1Clipped &&
		 ( wFillPoints[k-1].x > wFillPoints[k-1].y ) != ( wFillPoints[0].x > wFillPoints[0].y ) )
		addPoint( &k, &zero, &rect );
#ifdef DRAWFILLPOLYLOG
fflush( logF );
fclose( logF );
#endif
	if ( k <= 2 )
		return;
	Polygon( d->hDc, wFillPoints, k );
	if (d->hWnd) {
		rect.top--;
		rect.left--;
		rect.bottom++;
		rect.right++;
		myInvalidateRect( d, &rect );
	}
}

#define MAX_FILLCIRCLE_POINTS	(30)
void wDrawFilledCircle(
		wDraw_p d,
		wPos_t x,
		wPos_t y,
		wPos_t r,
		wDrawColor color,
		wDrawOpts opts )
{
	POINT p0, p1;
	RECT rect;
	static wPos_t circlePts[MAX_FILLCIRCLE_POINTS][2];
	int inx, cnt;
	double dang;

	p0.x = XINCH2PIX(d,x-r);
	p0.y = YINCH2PIX(d,y+r)+1;
	p1.x = XINCH2PIX(d,x+r);
	p1.y = YINCH2PIX(d,y-r)+1;
						   
	setDrawBrush( d->hDc, d, color, opts );						  
	if ( noNegDrawArgs > 0 && ( p0.x < 0 || p0.y < 0 ) ) {
		if ( r > MAX_FILLCIRCLE_POINTS )
			cnt = MAX_FILLCIRCLE_POINTS;
		else if ( r > 8 )
			cnt = r;
		else
			cnt = 8;
		dang = 360.0/cnt;
		for ( inx=0; inx<cnt; inx++ ) {
			circlePts[inx][0] = x + (int)(r * mswcos( inx*dang ) + 0.5 );
			circlePts[inx][1] = y + (int)(r * mswsin( inx*dang ) + 0.5 );
		}
		wDrawFilledPolygon( d, circlePts, cnt, color, opts );
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


void wDrawClear( wDraw_p d )
{
	RECT rect;
	SetROP2( d->hDc, R2_WHITE );
	Rectangle( d->hDc, 0, 0, d->w, d->h );
	if (d->hWnd) {
	rect.top = 0;
	rect.bottom = d->h;
	rect.left = 0;
	rect.right = d->w;
	InvalidateRect( d->hWnd, &rect, FALSE );
	}
}


void wDrawSetSize(
		wDraw_p d,
		wPos_t width,
		wPos_t height )
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
		wPos_t * width,
		wPos_t * height )
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
		wPos_t x,
		wPos_t y,
		wPos_t w,
		wPos_t h )
{
	int ix0, iy0, ix1, iy1;
	HRGN hRgnClip;
	ix0 = XINCH2PIX(d,x);
	iy0 = YINCH2PIX(d,y);
	ix1 = XINCH2PIX(d,x+w);
	iy1 = YINCH2PIX(d,y+h);
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
		wPos_t x;
		wPos_t y;
		wPos_t w;
		wPos_t h;
		char * bmx;
		wDrawColor color;
		HBITMAP bm;
		};
wDrawBitMap_p bmRoot = NULL;


void wDrawBitMap(
		wDraw_p d,
		wDrawBitMap_p bm,
		wPos_t px,
		wPos_t py,
		wDrawColor dc,
		wDrawOpts dopt )
{
	HDC bmDc, hDc;
	HBITMAP oldBm;
	DWORD mode;
	int x0, y0;
	RECT rect;

	x0 = XINCH2PIX(d,px-bm->x);
	y0 = YINCH2PIX(d,py-bm->y+bm->h);
#ifdef LATER
	if ( noNegDrawArgs > 0 && ( x0 < 0 || y0 < 0 ) )
		return;
#endif
	if (dopt & wDrawOptTemp) {
		mode = tmpOp;
	} else if (dc == wDrawColorWhite) {
		mode = clrOp;
		dc = wDrawColorBlack;
	} else {
		mode = setOp;
	}

	if ( bm->color != dc ) {
		if ( bm->bm )
			DeleteObject( bm->bm );
		bm->bm = mswCreateBitMap( mswGetColor(d->hasPalette,dc) /*colorPalette.palPalEntry[dc]*/, RGB( 255, 255, 255 ),
				RGB( 255, 255, 255 ), bm->w, bm->h, bm->bmx );
		bm->color = dc;
	}
	if ( (dopt & wDrawOptNoClip) != 0 &&
		 ( px < 0 || px >= d->w || py < 0 || py >= d->h ) ) {
		x0 += d->x;
		y0 += d->y;
		hDc = GetDC( ((wControl_p)(d->parent))->hWnd );
		bmDc = CreateCompatibleDC( hDc );
		oldBm = SelectObject( bmDc, bm->bm );
		BitBlt( hDc, x0, y0, bm->w, bm->h, bmDc, 0, 0, tmpOp );
		SelectObject( bmDc, oldBm );
		DeleteDC( bmDc );
		ReleaseDC( ((wControl_p)(d->parent))->hWnd, hDc );
		return;
	}

	bmDc = CreateCompatibleDC( d->hDc );
	setDrawMode( d->hDc, d, 0, wDrawLineSolid, dc, dopt );
	oldBm = SelectObject( bmDc, bm->bm );
	BitBlt( d->hDc, x0, y0, bm->w, bm->h, bmDc, 0, 0, mode );
	SelectObject( bmDc, oldBm );
	DeleteDC( bmDc );
	if (d->hWnd) {
	rect.top = y0-1;
	rect.bottom = rect.top+bm->h+1;
	rect.left = x0-1;
	rect.right = rect.left+bm->w+1;
	myInvalidateRect( d, &rect );
	}
}


wDrawBitMap_p wDrawBitMapCreate(
		wDraw_p d,
		int w,
		int h,
		int x,
		int y,
		const char * bits )
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

int doSetFocus = 1;

long FAR PASCAL XEXPORT mswDrawPush(
		HWND hWnd,
		UINT message,
		UINT wParam,
		LONG lParam )
{
#ifdef WIN32
	long inx = GetWindowLong( hWnd, GWL_ID );
#else
	short inx = GetWindowWord( hWnd, GWW_ID );
#endif
	wDraw_p b;
	short int ix, iy;
	wPos_t x, y;
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
			b->hBm = 0;
			b->hBmOld = 0;
		} else {
			b->hDc = CreateCompatibleDC( hDc ); 
			b->hBm = CreateCompatibleBitmap( hDc, b->w, b->h );
			b->hBmOld = SelectObject( b->hDc, b->hBm );
		}
		if (mswPalette) {
			SelectPalette( b->hDc, mswPalette, 0 );
			RealizePalette( b->hDc );
		}
		b->wFactor = (double)GetDeviceCaps( b->hDc, LOGPIXELSX );
		b->hFactor = (double)GetDeviceCaps( b->hDc, LOGPIXELSY );
		b->DPI = 96.0; /*min( b->wFactor, b->hFactor );*/
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
			b->hBm = CreateCompatibleBitmap( hDc, b->w, b->h );
			DeleteObject(SelectObject( b->hDc, b->hBm ));
			ReleaseDC( b->hWnd, hDc );
			SetROP2( b->hDc, R2_WHITE );
			Rectangle( b->hDc, 0, 0, b->w, b->h );
			}
		}
		/*if (b->drawResize)
			b->drawResize( b, b->size );*/
		if (b->drawRepaint)
			b->drawRepaint( b, b->data, 0, 0 );
		return 0;
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
		if (message == WM_LBUTTONDOWN)
			action = wActionLDown;
		else if (message == WM_RBUTTONDOWN)
			action = wActionRDown;
		else if (message == WM_LBUTTONUP)
			action = wActionLUp;
		else if (message == WM_RBUTTONUP)
			action = wActionRUp;
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
		x = XPIX2INCH( b, ix );
		y = YPIX2INCH( b, iy );
		if (b->action)
			b->action( b, b->data, action, x, y );
		if (b->hWnd)
			UpdateWindow(b->hWnd);
		return 0;
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
		/*case VK_F1:	extChar = wAccelKey_F1; break;*/
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
				b->action( b, b->data, wActionExtKey + ( (int)extChar << 8 ), 0, 0 );
			else
				b->action( b, b->data, wActionText + ( wParam << 8 ), 0, 0 );
		}
		return 0;

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
				BitBlt( hDc, rect.left, rect.top,
						rect.right-rect.left, rect.bottom-rect.top,
						b->hDc, rect.left, rect.top,
						SRCCOPY );
				EndPaint( hWnd, &ps );
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
		/* fwKeys = GET_KEYSTATE_WPARAM(wParam); modifier keys are currently ignored */
		if ( GET_WHEEL_DELTA_WPARAM(wParam) > 0 ) {
			action = wActionWheelUp;
		} else {
			action = wActionWheelDown;
		}
		if (b->action)
			b->action( b, b->data, action, 0, 0 );
		return 0;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}


static void drawDoneProc( wControl_p b )
{
	wDraw_p d = (wDraw_p)b;
	if (d->hBm) {
		SelectObject( d->hDc, d->hBmOld );
		DeleteObject( d->hBm );
		d->hBm = (HBITMAP)0;
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

wDraw_p drawList = NULL;


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
			BitBlt( hDc, rect.left, rect.top,
						rect.right-rect.left, rect.bottom-rect.top,
						b->hDc, rect.left, rect.top,
						SRCCOPY );
			EndPaint( b->hWnd, &ps );
		}
	}
}


wDraw_p wDrawCreate(
		wWin_p parent,
		wPos_t x,
		wPos_t y,
		const char * helpStr,
		long option,
		wPos_t w,
		wPos_t h,
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
				((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );

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
	return d;
}

/*
 *****************************************************************************
 *
 * Bitmaps
 *
 *****************************************************************************
 */

wDraw_p wBitMapCreate( wPos_t w, wPos_t h, int planes )
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
	d->hBm = CreateCompatibleBitmap( hDc, d->w, d->h );
	if ( d->hBm == (HBITMAP)0 ) {
		wNoticeEx( NT_ERROR, "CreateBitMap: CreateBM fails", "Ok", NULL );
		return FALSE;
	}
	d->hasPalette = (GetDeviceCaps(hDc,RASTERCAPS ) & RC_PALETTE) != 0;
	ReleaseDC( mswHWnd, hDc );
	d->hBmOld = SelectObject( d->hDc, d->hBm );
	if (mswPalette) {
		SelectPalette( d->hDc, mswPalette, 0 );
		RealizePalette( d->hDc );
	}
	d->wFactor = (double)GetDeviceCaps( d->hDc, LOGPIXELSX );
	d->hFactor = (double)GetDeviceCaps( d->hDc, LOGPIXELSY );
	d->DPI = 96.0; /*min( d->wFactor, d->hFactor );*/
	d->hWnd = 0;
	SetROP2( d->hDc, R2_WHITE );
	Rectangle( d->hDc, 0, 0, d->w, d->h );
	return d;
}

wBool_t wBitMapDelete( wDraw_p d )
{
	if (d->hPen) {
		SelectObject( d->hDc, GetStockObject( BLACK_PEN ) );
		DeleteObject( d->hPen );
		d->hPen = (HPEN)0;
	}
	if (d->hBm) {
		SelectObject( d->hDc, d->hBmOld );
		DeleteObject( d->hBm );
		d->hBm = (HBITMAP)0;
	}
	if (d->hDc) {
		DeleteDC( d->hDc );
		d->hDc = (HDC)0;
	}
	free(d);
	return TRUE;
}

wBool_t wBitMapWriteFile( wDraw_p d, const char * fileName )
{
	char *pixels;
	int j, ww, chunk;
	FILE * f;
	BITMAPFILEHEADER bmfh;
	struct {
		BITMAPINFOHEADER bmih;
		RGBQUAD colors[256];
	} bmi;
	int rc;
	
	if ( d->hBm == 0)
		return FALSE;
	f = wFileOpen( fileName, "wb" );
	if (!f) {
		wNoticeEx( NT_ERROR, fileName, "Ok", NULL );
		return FALSE;
	}
	ww = ((d->w +3) / 4) * 4;
	bmfh.bfType = 'B'+('M'<<8);
	bmfh.bfSize = (long)(sizeof bmfh) + (long)(sizeof bmi.bmih) + (long)(sizeof bmi.colors) + (long)ww * (long)(d->h);
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof bmfh + sizeof bmi.bmih + sizeof bmi.colors;
	fwrite( &bmfh, 1, sizeof bmfh, f );
	bmi.bmih.biSize = sizeof bmi.bmih;
	bmi.bmih.biWidth = d->w;
	bmi.bmih.biHeight = d->h;
	bmi.bmih.biPlanes = 1;
	bmi.bmih.biBitCount = 8;
	bmi.bmih.biCompression = BI_RGB;
	bmi.bmih.biSizeImage = 0;
	bmi.bmih.biXPelsPerMeter = 75*(10000/254);
	bmi.bmih.biYPelsPerMeter = 75*(10000/254);
	bmi.bmih.biClrUsed = bmi.bmih.biClrImportant = mswGetColorList( bmi.colors );
	SelectObject( d->hDc, d->hBmOld );
	rc = GetDIBits( d->hDc, d->hBm, 0, 1, NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS );
	if ( rc == 0 ) {
		wNoticeEx( NT_ERROR, "WriteBitMap: Can't get bitmapinfo from Bitmap", "Ok", NULL );
		return FALSE;
	}
	bmi.bmih.biClrUsed = 256;
	fwrite( &bmi.bmih, 1, sizeof bmi.bmih, f );
	fwrite( bmi.colors, 1, sizeof bmi.colors, f );
	chunk = 32000/ww;
	pixels = (char*)malloc( ww*chunk );
	if ( pixels == NULL ) {
		wNoticeEx( NT_ERROR, "WriteBitMap: no memory", "OK", NULL );
		return FALSE;
	}
	for (j=0;j<d->h;j+=chunk) {
		if (j+chunk>d->h)
			chunk = d->h-j;
		rc = GetDIBits( d->hDc, d->hBm, j, chunk, pixels, (BITMAPINFO*)&bmi, DIB_RGB_COLORS );
		if ( rc == 0 ) 
		if ( rc == 0 ) {
			wNoticeEx( NT_ERROR, "WriteBitMap: Can't get bits from Bitmap", "Ok", NULL );
			return FALSE;
		}
		rc = fwrite( pixels, 1, ww*chunk, f );
		if (rc != ww*chunk) {
			wNoticeEx( NT_ERROR, "WriteBitMap: Bad fwrite", "Ok", NULL);
		}
	}
	free( pixels );
	SelectObject( d->hDc, d->hBm );
	fclose( f );
	return TRUE;
}

