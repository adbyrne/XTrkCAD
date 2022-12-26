/** \file mswbutt.c
* Buttons
*/

/*  XTrkCad - Model Railroad CAD
*  Copyright (C)
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
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include "mswint.h"

/** Macros for button repeat timers */
#define REPEAT_STAGE0_DELAY 500
#define REPEAT_STAGE1_DELAY 150
#define REPEAT_STAGE2_DELAY 75
#define STOP_TIMER (-1)
#define INITIAL_WAIT (0)
#define SLOW_REPEATS (1)
#define FAST_REPEATS (2)

/*
 *****************************************************************************
 *
 * Simple Buttons
 *
 *****************************************************************************
 */

static XWNDPROC oldButtProc = NULL;

struct wButton_t {
	WOBJ_COMMON
	wButtonCallBack_p action;
	wBool_t busy;
	wBool_t selected;
	wIcon_p icon;
	UINT_PTR timer_id;
	int timer_count;
	int timer_state;
};



void mswButtPush(
        wControl_p b )
{
	if ( ((wButton_p)b)->action ) {
		((wButton_p)b)->action( ((wButton_p)b)->data );
	}
}

/**
 * Paint function for toolbar buttons
 *
 * \param hButtDc IN valid device context
 * \param bm IN bitmap to add to button
 * \param selected IN selected state of button
 * \param disabled IN disabled state of button
 */

static void drawButton(
        HDC hButtDc,
        wIcon_p bm,
        BOOL_T selected,
        BOOL_T disabled )
{
	HGDIOBJ oldBrush, newBrush;
	HPEN oldPen, newPen;
	RECT rect;
	COLORREF color1, color2;
	wWinPix_t offw=5, offh=5;

	COLORREF colL;
	COLORREF colD;
	COLORREF colF;

#define LEFT (0)
#define RIGHT (bm->w+9)
#define TOP (0)
#define BOTTOM (bm->h+9)

	/* get the lightest and the darkest color to use */
	colL = GetSysColor( COLOR_BTNHIGHLIGHT );
	colD = GetSysColor( COLOR_BTNSHADOW );
	colF = GetSysColor( COLOR_BTNFACE );

	/* define the rectangle for the button */
	rect.top = TOP;
	rect.left = LEFT;
	rect.right = RIGHT;
	rect.bottom = BOTTOM;

	/* fill the button with the face color */
	newBrush = CreateSolidBrush( colF );
	oldBrush = SelectObject( hButtDc, newBrush );
	FillRect( hButtDc, &rect, newBrush );
	DeleteObject( SelectObject( hButtDc, oldBrush ) );

	/* disabled button remain flat */
	if( !disabled ) {
		/* select colors for the gradient */
		if( selected ) {
			color1 = colD;
			color2 = colL;
		} else {
			color1 = colL;
			color2 = colD;
		}

		/* draw delimiting lines in shadow color */
		newPen = CreatePen( PS_SOLID, 0, color1 );
		oldPen = SelectObject( hButtDc, newPen );

		MoveTo( hButtDc, RIGHT-1, TOP );
		LineTo( hButtDc, LEFT, TOP );
		LineTo( hButtDc, LEFT, BOTTOM );
		DeleteObject( SelectObject( hButtDc, oldPen ) );

		newPen = CreatePen( PS_SOLID, 0, color2 );
		oldPen = SelectObject( hButtDc, newPen );

		MoveTo( hButtDc, RIGHT, TOP+1 );
		LineTo( hButtDc, RIGHT, BOTTOM );
		LineTo( hButtDc, LEFT, BOTTOM );
		DeleteObject( SelectObject( hButtDc, oldPen ) );
	}

	color2 = GetSysColor( COLOR_BTNSHADOW );
	color1 = RGB( bm->colormap[ 1 ].rgbRed, bm->colormap[ 1 ].rgbGreen,
	              bm->colormap[ 1 ].rgbBlue );

	if (selected) {
		offw++; offh++;
	}
	mswDrawIcon( hButtDc, offw, offh, bm, disabled, color1, color2 );
}


static void buttDrawIcon(
        wButton_p b,
        HDC butt_hDc )
{
	wIcon_p bm = b->icon;
	wWinPix_t offw=5, offh=5;

	if (b->selected || b->busy) {
		offw++; offh++;
	} else if ( (b->option & BO_DISABLED) != 0 ) {
		;
	} else {
		;
	}
	drawButton( butt_hDc, bm, b->selected
	            || b->busy, (b->option & BO_DISABLED) != 0 );
}

void wButtonSetBusy(
        wButton_p b,
        int value)
{
	b->busy = value;
	if (!value) {
		b->selected = FALSE;
	}

	// in case a timer is associated with the button, kill it
	if (b->timer_id) {
		KillTimer(b->hWnd, b->timer_id);
		b->timer_id = 0;
		b->timer_state = STOP_TIMER;
	}

	InvalidateRgn(b->hWnd, NULL, FALSE);
}


void wButtonSetLabel(
        wButton_p b,
        const char * label )
{
	if ((b->option&BO_ICON) == 0) {
		/*b->labelStr = label;*/
		SetWindowText( b->hWnd, label );
	} else {
		b->icon = (wIcon_p)label;
	}
	InvalidateRgn( b->hWnd, NULL, FALSE );
}

/**
 * Button timer: handle timer events for buttons. These are used for
 * auto-repeating presses. Three phases used are
 * - initial delay before repetitions begin
 * - slow repeats for a few cycles
 * - fast repeats therafter
 * - stop timer
 *
 * \param  hWnd	    Handle of the window, unused
 * \param  message  The message, unused
 * \param  timer    The timer id is the wlib widget .
 * \param  timepast The timepast, unused
  */

void CALLBACK buttTimer(HWND hWnd, UINT message, UINT_PTR timer,
                        DWORD timepast)
{
	wButton_p b = (wButton_p)timer;
	if (b->timer_id == 0) {
		b->timer_state = STOP_TIMER;
		return ;
	}

	/* Autorepeat state machine */
	switch (b->timer_state) {
	case INITIAL_WAIT:
		b->timer_state = SLOW_REPEATS;
		b->timer_count = 0;
		KillTimer(hWnd, (UINT_PTR)b);
		SetTimer(hWnd, (UINT_PTR)b, REPEAT_STAGE1_DELAY, buttTimer);
		break;
	case SLOW_REPEATS: /* Enable slow auto-repeat */
		if (b->timer_count++ > 10) {
			/* Start fast auto-repeat */
			b->timer_state = FAST_REPEATS;
			KillTimer(hWnd, (UINT_PTR)b);
			SetTimer(hWnd, (UINT_PTR)b, REPEAT_STAGE2_DELAY, buttTimer);
		}
		break;
	case FAST_REPEATS:
		break;
	case STOP_TIMER:
	default:
		KillTimer(hWnd, (UINT_PTR)b);
		b->timer_id = 0;
		return;
		break;
	}
	if (b->action) {
		b->action(b->data);
	}
	return;
}


static LRESULT buttPush( wControl_p b, HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam )
{
	wButton_p bb = (wButton_p)b;
	DRAWITEMSTRUCT * di = (DRAWITEMSTRUCT *)lParam;
	wBool_t selected;


	switch (message) {
	case WM_COMMAND:
		if (bb->action /*&& !bb->busy*/) {
			bb->action( bb->data );
			return (LRESULT)0;
		}
		break;

	case WM_MEASUREITEM: {
		MEASUREITEMSTRUCT * mi = (MEASUREITEMSTRUCT *)lParam;
		if (bb->type != B_BUTTON || (bb->option & BO_ICON) == 0) {
			break;
		}
		mi->CtlType = ODT_BUTTON;
		mi->CtlID = (UINT)wParam;
		mi->itemWidth = (UINT)bb->w;
		mi->itemHeight = (UINT)bb->h;
	} return (LRESULT)0;

	case WM_DRAWITEM:
		if (bb->type == B_BUTTON && (bb->option & BO_ICON) != 0) {
			selected = ((di->itemState & ODS_SELECTED) != 0);
			if (bb->selected != selected) {
				bb->selected = selected;
				InvalidateRgn( bb->hWnd, NULL, FALSE );
			}
			return (LRESULT)TRUE;
		}
		break;
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}


static void buttDone(
        wControl_p b )
{
	free(b);
}

LRESULT CALLBACK pushButt(
        HWND hWnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam )
{
	/* Catch <Return> and cause focus to leave control */

	wIndex_t inx = (wIndex_t)GetWindowLongPtr( hWnd, GWL_ID );
	wButton_p b = (wButton_p)mswMapIndex( inx );
	PAINTSTRUCT ps;

	switch (message) {
	case WM_PAINT:
		if ( b && b->type == B_BUTTON && (b->option & BO_ICON) != 0 ) {
			BeginPaint( hWnd, &ps );
			buttDrawIcon( (wButton_p)b, ps.hdc );
			EndPaint( hWnd, &ps );
			return (LRESULT)1;
		}
		break;
	case WM_CHAR:
		if ( b != NULL ) {
			switch( wParam ) {
			case 0x0D:
			case 0x1B:
			case 0x09:
				/*SetFocus( ((wControl_p)(b->parent))->hWnd );*/
				SendMessage( ((wControl_p)(b->parent))->hWnd, WM_CHAR,
				             wParam, lParam );
				/*SendMessage( ((wControl_p)(b->parent))->hWnd, WM_COMMAND,
						inx, MAKELONG( hWnd, EN_KILLFOCUS ) );*/
				return (LONG_PTR)0;
			}
		}
		break;
	case WM_KILLFOCUS:
		if ( b ) {
			InvalidateRect( b->hWnd, NULL, TRUE );
		}
		return (LRESULT)0;
		break;
	case WM_LBUTTONDOWN:
		if (b->option&BO_REPEAT) {
			SetTimer(hWnd, (UINT_PTR)b,REPEAT_STAGE0_DELAY,buttTimer);
			b->timer_state = INITIAL_WAIT;
			b->timer_id = (UINT_PTR)b;
		}
		break;
	case WM_LBUTTONUP:
		/* don't know why but this solves a problem with color selection */
		Sleep( 0 );
		if (b->timer_id) {
			KillTimer(hWnd, (UINT_PTR)b);
		}
		b->timer_id = 0;
		b->timer_state = STOP_TIMER;
		break;
	}
	return CallWindowProc( oldButtProc, hWnd, message, wParam, lParam );
}

static callBacks_t buttonCallBacks = {
	mswRepaintLabel,
	buttDone,
	buttPush
};

wButton_p wButtonCreate(
        wWin_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char	* helpStr,
        const char	* labelStr,
        long	option,
        wWinPix_t	width,
        wButtonCallBack_p action,
        void	* data )
{
	wButton_p b;
	RECT rect;
	int h=20;
	int index;
	DWORD style;
	HDC hDc;
	wIcon_p bm;

	if (width <= 0) {
		width = 80;
	}
	if ((option&BO_ICON) == 0) {
		labelStr = mswStrdup( labelStr );
	} else {
		bm = (wIcon_p)labelStr;
		labelStr = NULL;
	}
	b = (wButton_p)mswAlloc( parent, B_BUTTON, NULL, sizeof *b, data, &index );
	b->option = option;
	b->busy = 0;
	b->selected = 0;
	mswComputePos( (wControl_p)b, x, y );
	if (b->option&BO_ICON) {
		width = (wWinPix_t)(bm->w+10);
		h = bm->h+10;
		b->icon = bm;
	} else {
		width = (wWinPix_t)(width*mswScale);
	}
	style = ((b->option&BO_ICON)? BS_OWNERDRAW : BS_PUSHBUTTON) |
	        WS_CHILD | WS_VISIBLE |
	        mswGetBaseStyle(parent);
	if ((b->option&BB_DEFAULT) != 0) {
		style |= BS_DEFPUSHBUTTON;
	}
	b->hWnd = CreateWindow( "BUTTON", labelStr, style, b->x, b->y,
	                        /*CW_USEDEFAULT, CW_USEDEFAULT,*/ width, h,
	                        ((wControl_p)parent)->hWnd, (HMENU)(UINT_PTR)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(BUTTON)");
		return b;
	}
	/*SetWindowLong( b->hWnd, 0, (long)b );*/
	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;
	mswAddButton( (wControl_p)b, TRUE, helpStr );
	b->action = action;
	mswCallBacks[B_BUTTON] = &buttonCallBacks;
	mswChainFocus( (wControl_p)b );

	oldButtProc = (WNDPROC)SetWindowLongPtr(b->hWnd, GWLP_WNDPROC,
	                                        (LONG_PTR)&pushButt);
	if (mswPalette) {
		hDc = GetDC( b->hWnd );
		SelectPalette( hDc, mswPalette, 0 );
		RealizePalette( hDc );
		ReleaseDC( b->hWnd, hDc );
	}

	SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, (LPARAM)0 );

	InvalidateRect(b->hWnd, &rect, TRUE);

	return b;
}
