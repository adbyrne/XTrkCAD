#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include "mswint.h"

/*
 *****************************************************************************
 *
 * Boxes
 *
 *****************************************************************************
 */

struct wBox_t {
		WOBJ_COMMON
		wBoxType_e boxTyp;
		};

#define B (1)
#define W (2)
#define SETCOLOR( S, N ) \
		if ( lastColor != colors[bb->boxTyp][S][N] ) { \
			lastColor = colors[bb->boxTyp][S][N]; \
			SetROP2( hDc, (lastColor==B?R2_BLACK:R2_WHITE) ); \
		}


void wBoxSetSize(
		wBox_p bb,
		wPos_t w,
		wPos_t h )
{
	bb->w = w;
	bb->h = h;
}


static void repaintBox( HWND hWnd, wControl_p b )
{						  
	HDC hDc;
	wBox_p bb = (wBox_p)(b);
	wPos_t x0, y0, x1, y1;
	char lastColor;
	int lastRop;
	static char colors[8][4][2] = {
		{ /* ThinB	*/ {B,0}, {B,0}, {B,0}, {B,0} },
		{ /* ThinW	*/ {W,0}, {W,0}, {W,0}, {W,0} },
		{ /* AboveW */ {W,0}, {W,0}, {B,0}, {B,0} },
		{ /* BelowW */ {B,0}, {B,0}, {W,0}, {W,0} },
		{ /* ThickB */ {B,B}, {B,B}, {B,B}, {B,B} },
		{ /* ThickW */ {W,W}, {W,W}, {W,W}, {W,W} },
		{ /* RidgeW */ {W,B}, {W,B}, {B,W}, {B,W} },
		{ /* TroughW*/ {B,W}, {B,W}, {W,B}, {W,B} } };

	x0 = bb->x;
	x1 = bb->x+bb->w;
	y0 = bb->y;
	y1 = bb->y+bb->h;
	hDc = GetDC( hWnd );
	MoveTo( hDc, x0, y1 );
	/*SETCOLOR( 0, 0 );*/
	lastColor = colors[bb->boxTyp][0][0];
	lastRop = SetROP2( hDc, (lastColor==B?R2_BLACK:R2_WHITE) );
	LineTo( hDc, x0, y0 );
	SETCOLOR( 1, 0 );
	LineTo( hDc, x1, y0 );
	SETCOLOR( 2, 0 );
	LineTo( hDc, x1, y1 );
	SETCOLOR( 3, 0 );
	LineTo( hDc, x0, y1 );
	if (bb->boxTyp >= wBoxThickB) {
		x0++; y0++; x1--; y1--;
		MoveTo( hDc, x0, y1 );
		SETCOLOR( 0, 1 );
		LineTo( hDc, x0, y0 );
		SETCOLOR( 1, 1 );
		LineTo( hDc, x1, y0 );
		SETCOLOR( 2, 1 );
		LineTo( hDc, x1, y1 );
		SETCOLOR( 3, 1 );
		LineTo( hDc, x0, y1 );
	}
	SetROP2( hDc, lastRop );
	ReleaseDC( hWnd, hDc );
}


static callBacks_t boxCallBacks = {
		repaintBox,
		NULL,
		NULL };

wBox_p wBoxCreate(
		wWin_p	parent,
		wPos_t	origX,
		wPos_t	origY,
		const char	* labelStr,
		wBoxType_e typ,
		wPos_t	width,
		wPos_t	height )
{
	wBox_p b;
	int index;

	b = (wBox_p)mswAlloc( parent, B_BOX, labelStr, sizeof *b, NULL, &index );
	b->boxTyp = typ;

	b->x = origX;
	b->y = origY;
	b->w = width;
	b->h = height;
	mswAddButton( (wControl_p)b, FALSE, NULL );
	mswCallBacks[B_BOX] = &boxCallBacks; 
	repaintBox( ((wControl_p)parent)->hWnd, (wControl_p)b );
	return b;
} 
