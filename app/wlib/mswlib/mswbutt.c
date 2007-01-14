#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include "mswint.h"
int kludge12 = 0;

/*
 *****************************************************************************
 *
 * Simple Buttons
 *
 *****************************************************************************
 */



static XWNDPROC oldButtProc = NULL;
static XWNDPROC newButtProc;


struct wButton_t {
		WOBJ_COMMON
		wButtonCallBack_p action;
		wBool_t busy;
		wBool_t selected;
		wIcon_p icon;
		};



void mswButtPush(
		wControl_p b )
{
	if ( ((wButton_p)b)->action )
		((wButton_p)b)->action( ((wButton_p)b)->data );
}
		

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
	POS_T offw=5, offh=5;

#define LEFT (0)
#define RIGHT (bm->w+10)
#define TOP (0)
#define BOTTOM (bm->h+10)
	
		PatBlt( hButtDc, LEFT, TOP, RIGHT, BOTTOM, WHITENESS );
		rect.top = TOP;
		rect.left = LEFT;
		rect.right = RIGHT;
		rect.bottom = BOTTOM;

		newBrush = CreateSolidBrush( GetSysColor( selected?COLOR_BTNSHADOW:COLOR_BTNFACE ) );
		newPen = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNFACE ) );
		oldBrush = SelectObject( hButtDc, newBrush );
		oldPen = SelectObject( hButtDc, newPen );
		FillRect( hButtDc, &rect, newBrush );
		FrameRect( hButtDc, &rect, GetStockObject(BLACK_BRUSH) );
		color1 = GetSysColor( selected ? COLOR_BTNSHADOW : COLOR_BTNHIGHLIGHT );
		newPen = CreatePen( PS_SOLID, 0, color1 );
		DeleteObject( SelectObject( hButtDc, newPen ) );
		MoveTo( hButtDc, LEFT+1,		TOP+1 );
		LineTo( hButtDc, RIGHT-1,		TOP+1 );
		MoveTo( hButtDc, LEFT+1,		TOP+2 );
		LineTo( hButtDc, LEFT+1,		BOTTOM-1 );
		if (!selected) {
			MoveTo( hButtDc, LEFT+2,	TOP+2 );
			LineTo( hButtDc, RIGHT-1-2, TOP+2 );
			MoveTo( hButtDc, LEFT+2,	TOP+3 );
			LineTo( hButtDc, LEFT+2,	BOTTOM-1-2 );
			color1=GetSysColor( selected ? COLOR_BTNFACE : COLOR_BTNSHADOW );
			newPen = CreatePen( PS_SOLID, 0, color1 );
			DeleteObject( SelectObject( hButtDc, newPen ) );
			MoveTo( hButtDc, RIGHT-2-1, TOP+2 );
			LineTo( hButtDc, RIGHT-2-1, BOTTOM-1 );
			MoveTo( hButtDc, RIGHT-2,	TOP+1 );
			LineTo( hButtDc, RIGHT-2,	BOTTOM-1 );
			MoveTo( hButtDc, LEFT+2,	BOTTOM-2-1 );
			LineTo( hButtDc, RIGHT-2-1, BOTTOM-2-1 );
			MoveTo( hButtDc, LEFT+1,	BOTTOM-2 );
			LineTo( hButtDc, RIGHT-2-1, BOTTOM-2 );
		}
		DeleteObject( SelectObject( hButtDc, oldBrush ) );
		DeleteObject( SelectObject( hButtDc, oldPen ) );

	if ( disabled ) {
		color1 = GetSysColor( COLOR_BTNSHADOW );
		color2 = RGB(255,255,255);
	} else {
		color1 = wDrawGetRGB( bm->color );
		color1 = ((color1>>16)&0x0000FF) | (color1&0x00FF00) | ((color1<<16)&0xFF0000);
		/*color1 = mswGetColor( bm->color );*/
		color2 = GetSysColor( COLOR_BTNFACE );
	}

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
		POS_T offw=5, offh=5;

		if (b->selected || b->busy) {
			offw++; offh++;
		} else if ( (b->option & BO_DISABLED) != 0 ) {
			;
		} else {
			;
		}
		drawButton( butt_hDc, bm, b->selected || b->busy, (b->option & BO_DISABLED) != 0 );
}

void wButtonSetBusy(
		wButton_p b,
		int value )
{
	b->busy = value;
	if (!value)
		b->selected = FALSE;
	/*SendMessage( b->hWnd, BM_SETSTATE, (WPARAM)value, 0L );*/
	InvalidateRgn( b->hWnd, NULL, FALSE );
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
				   

static LRESULT buttPush( wControl_p b, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	wButton_p bb = (wButton_p)b;
	DRAWITEMSTRUCT * di = (DRAWITEMSTRUCT *)lParam;
	wBool_t selected;

	switch (message) {
	case WM_COMMAND:
		if (bb->action /*&& !bb->busy*/) {
			bb->action( bb->data );
			return 0L;
		}
		break;

	case WM_MEASUREITEM: {
		MEASUREITEMSTRUCT * mi = (MEASUREITEMSTRUCT *)lParam;
		if (bb->type != B_BUTTON || (bb->option & BO_ICON) == 0)
			break;
		mi->CtlType = ODT_BUTTON;
		mi->CtlID = wParam;
		mi->itemWidth = bb->w;
		mi->itemHeight = bb->h;
		} return 0L;

	case WM_DRAWITEM:
		if (bb->type == B_BUTTON && (bb->option & BO_ICON) != 0) {
			selected = ((di->itemState & ODS_SELECTED) != 0);
			if (bb->selected != selected) {
				bb->selected = selected;
				InvalidateRgn( bb->hWnd, NULL, FALSE );
			}
			return TRUE;
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

long FAR PASCAL _export pushButt(
		HWND hWnd,
		UINT message,
		UINT wParam,
		LONG lParam )
{
	/* Catch <Return> and cause focus to leave control */
#ifdef WIN32
	long inx = GetWindowLong( hWnd, GWL_ID );
#else
	short inx = GetWindowWord( hWnd, GWW_ID );
#endif
	wButton_p b = (wButton_p)mswMapIndex( inx );
	PAINTSTRUCT ps;

	switch (message) {
	case WM_PAINT:
		if ( b && b->type == B_BUTTON && (b->option & BO_ICON) != 0 ) {
			BeginPaint( hWnd, &ps );
			buttDrawIcon( (wButton_p)b, ps.hdc );
			EndPaint( hWnd, &ps );
			return 1L;
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
				return 0L;
			}
		}
		break;
	case WM_KILLFOCUS:
		if ( b )
			InvalidateRect( b->hWnd, NULL, TRUE );
		return 0L;
		break;
	case WM_ERASEBKGND:
		if (kludge12)
		return 1L;
	}
	return CallWindowProc( oldButtProc, hWnd, message, wParam, lParam );
}

static callBacks_t buttonCallBacks = {
		mswRepaintLabel,
		buttDone,
		buttPush };

wButton_p wButtonCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		wPos_t	width,
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

	if (width <= 0)
		width = 80;
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
		width = bm->w+10;
		h = bm->h+10;
		b->icon = bm;
	} else {
		width = (wPos_t)(width*mswScale);
	}
	style = ((b->option&BO_ICON)? BS_OWNERDRAW : BS_PUSHBUTTON) |
				WS_CHILD | WS_VISIBLE |
				mswGetBaseStyle(parent);
	if ((b->option&BB_DEFAULT) != 0)
		style |= BS_DEFPUSHBUTTON;
	b->hWnd = CreateWindow( "BUTTON", labelStr, style, b->x, b->y,
				/*CW_USEDEFAULT, CW_USEDEFAULT,*/ width, h,
				((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
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
	newButtProc = MakeProcInstance( (XWNDPROC)pushButt, mswHInst );
	oldButtProc = (XWNDPROC)GetWindowLong( b->hWnd, GWL_WNDPROC );
	SetWindowLong( b->hWnd, GWL_WNDPROC, (LONG)newButtProc );
	if (mswPalette) {
		hDc = GetDC( b->hWnd );
		SelectPalette( hDc, mswPalette, 0 );
		RealizePalette( hDc );
		ReleaseDC( b->hWnd, hDc );
	}
	if ( !mswThickFont )
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	return b;
}
