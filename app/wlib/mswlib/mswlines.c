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
 * Lines
 *
 *****************************************************************************
 */

struct wLine_t {
		WOBJ_COMMON
		int count;
		wLines_t * lines;
		};

static void repaintLines( HWND hWnd, wControl_p b )
{
	int lastWidth;
	HDC hDc;
	wLine_p bl = (wLine_p)(b);
	wLines_p lp;
	HPEN oldPen, newPen;

	lastWidth = -1;
	hDc = GetDC( hWnd );
	for (lp=bl->lines; lp<&bl->lines[bl->count]; lp++) {
		if (lastWidth != lp->width) {
			lastWidth = lp->width;
			newPen = CreatePen( PS_SOLID, lastWidth, RGB(0,0,0) );
			oldPen = SelectObject( hDc, newPen );
			DeleteObject( oldPen );
		}
		MoveTo( hDc, lp->x0, lp->y0 );
		LineTo( hDc, lp->x1, lp->y1 );
	}
	ReleaseDC( hWnd, hDc );
}


static callBacks_t linesCallBacks = {
		repaintLines,
		NULL,
		NULL };


wLine_p wLineCreate(
		wWin_p	parent,
		const char	* labelStr,
		int		count,
		wLines_t * lines )
{
	wLine_p b;
	wLines_p lp;
	POS_T minX, maxX, minY, maxY;
	int index;

	if (count <= 0)
		return NULL;
	b = (wLine_p)mswAlloc( parent, B_LINES, labelStr, sizeof *b, NULL, &index );
	b->count = count;
	b->lines = lines;

	lp = lines;
	minX = maxX = lp->x0;
	minY = maxY = lp->y0;
	for (lp=lines; lp<&b->lines[count]; lp++) {
		if (minX > lp->x0)
			minX = lp->x0;
		if (maxX < lp->x0)
			maxX = lp->x0;
		if (minY > lp->y0)
			minY = lp->y0;
		if (maxY < lp->y0)
			maxY = lp->y0;
		if (minX > lp->x1)
			minX = lp->x1;
		if (maxX < lp->x1)
			maxX = lp->x1;
		if (minY > lp->y1)
			minY = lp->y1;
		if (maxY < lp->y1)
			maxY = lp->y1;
	}
	b->x = minX;
	b->y = minY;
	b->w = maxX-minX;
	b->h = maxY-minY;
	mswAddButton( (wControl_p)b, FALSE, NULL );
	mswCallBacks[B_LINES] = &linesCallBacks;
	return b;
}
