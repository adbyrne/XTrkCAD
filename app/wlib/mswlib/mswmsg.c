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
 * Message Boxes
 *
 *****************************************************************************
 */

/**
 * factors by which fonts are resized if nonstandard text height is used
 */

#define SCALE_LARGE 1.6
#define SCALE_SMALL 0.8

#ifdef CONTROL3D
static int messageHeight = 18;
#endif

struct wMessage_t {
		WOBJ_COMMON
		long flags;
		const char * message;
		};

#ifndef CONTROL3D
static void repaintMessage(
		HWND hWnd,
		wControl_p b )
{
	wMessage_p bm = (wMessage_p)b;
	HDC hDc;
	RECT rect;
	HFONT hFont;
	LOGFONT msgFont;
	double scale = 1.0;

	hDc = GetDC( hWnd );

	if ( !mswThickFont )
		hFont = SelectObject( hDc, mswLabelFont );

	switch( wMessageSetFont( ((wMessage_p)b)->flags )) 
	{
		case BM_LARGE:
			scale = SCALE_LARGE;
			break;
		case BM_SMALL:
			scale = SCALE_SMALL;
			break;
	}

	/* is a non-standard text height required? */	 
	if( scale != 1.0 )
	{
		/* if yes, get information about the standard font used */
		GetObject( GetStockObject( DEFAULT_GUI_FONT ), sizeof( LOGFONT ), &msgFont );

		/* change the height */
		msgFont.lfHeight = (long)((double)msgFont.lfHeight * scale); 

		/* create and activate the new font */
		hFont = SelectObject( hDc, CreateFontIndirect( &msgFont ) );
	} else {
		if ( !mswThickFont )
			hFont = SelectObject( hDc, mswLabelFont );
	}

	rect.bottom = (long)(bm->y+( bm->h ));
	rect.right = (long)(bm->x+( scale * bm->w ));
	rect.top = bm->y;
	rect.left = bm->x;

	SetBkColor( hDc, GetSysColor( COLOR_BTNFACE ) );
	ExtTextOut( hDc, bm->x, bm->y, ETO_CLIPPED|ETO_OPAQUE, &rect, bm->message, strlen( bm->message ), NULL );

	if( scale != 1.0 )
		/* in case we did create a new font earlier, delete it now */
		DeleteObject( SelectObject( hDc, GetStockObject( DEFAULT_GUI_FONT )));
	else 
		if ( !mswThickFont )
			SelectObject( hDc, hFont );

	ReleaseDC( hWnd, hDc );
}
#endif

void wMessageSetValue(
		wMessage_p b,
		const char * arg )
{
	if (b->message)
		free( CAST_AWAY_CONST b->message );
	if (arg)
		b->message = mswStrdup( arg );
	else
		b->message = NULL;
#ifdef CONTROL3D
	SetWindowText( b->hWnd, arg );
#else
	repaintMessage( ((wControl_p)(b->parent))->hWnd, (wControl_p)b );
#endif
}

void wMessageSetWidth(
		wMessage_p b,
		wPos_t width )
{
	b->w = width;
#ifdef CONTROL3D
	SetWindowPos( b->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
				width, messageHeight, SWP_NOMOVE );
#endif
}

wPos_t wMessageGetWidth(const char *string)
{
	return(wLabelWidth(string));
}

wPos_t wMessageGetHeight( long flags )
{
#ifdef CONTROL3D
	return messageHeight;
#else
	double scale = 1.0;

	if( flags & BM_LARGE )
		scale = SCALE_LARGE;
	if( flags & BM_SMALL )
		scale = SCALE_SMALL;

	return((wPos_t)((mswEditHeight - 4) * scale ));
#endif
}

static void mswMessageSetBusy(
		wControl_p b,
		BOOL_T busy )
{
}


#ifndef CONTROL3D
static callBacks_t messageCallBacks = {
		repaintMessage,
		NULL,
		NULL,
		mswMessageSetBusy };
#endif


wMessage_p wMessageCreateEx(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		POS_T	width,
		const char	*message,
		long	flags )
{
	wMessage_p b;
	int index;
	
#ifdef CONTROL3D
	RECT rect;
#endif
	
	b = (wMessage_p)mswAlloc( parent, B_MESSAGE, NULL, sizeof *b, NULL, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option |= BO_READONLY;
	b->message = mswStrdup( message );
	b->flags = flags;

#ifdef CONTROL3D
	if ( width <= 0	 && strlen(b->message) > 0 ) {
		width = wLabelWidth( b->message );
	}
		
	b->hWnd = CreateWindow( "STATIC", NULL,
						SS_LEFTNOWORDWRAP | WS_CHILD | WS_VISIBLE,
						b->x, b->y,
						width, messageHeight,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(MESSAGE)");
		return b;
	}

	if ( !mswThickFont )
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	SetWindowText( b->hWnd, message );

	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;
#else
	b->w = width;
	b->h = wMessageGetHeight( flags ) + 1;

	repaintMessage( ((wControl_p)parent)->hWnd, (wControl_p)b );
#endif
	mswAddButton( (wControl_p)b, FALSE, helpStr );
#ifndef CONTROL3D
	mswCallBacks[B_MESSAGE] = &messageCallBacks;
#endif
	return b;
}
