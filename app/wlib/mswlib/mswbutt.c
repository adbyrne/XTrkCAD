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
	POS_T offw=5, offh=5;
	TRIVERTEX        vert[2] ;
	GRADIENT_RECT    gRect;

	COLORREF colL;
	COLORREF colD;
	COLORREF colF;

#define LEFT (0)
#define RIGHT (bm->w+10)
#define TOP (0)
#define BOTTOM (bm->h+10)

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
	if( !disabled )
	{
		/* select colors for the gradient */
		if( selected ) {
			color1 = colD;
			color2 = colL;
		} else {
			color1 = colL;
			color2 = colD;
		}

#define GRADIENT_WIDTH 6

		/* 
			first draw the top gradient 
			this always ends in the button face color 
			starting color depends on button state (selected or not) 
		*/
		vert [0] .x      = LEFT;
		vert [0] .y      = TOP;
		vert [0] .Red    = GetRValue( color1 )* 256;
		vert [0] .Green  = GetGValue( color1 )* 256;
		vert [0] .Blue   = GetBValue( color1 )* 256;
		vert [0] .Alpha  = 0x0000;
		vert [1] .x      = RIGHT;
		vert [1] .y      = TOP + GRADIENT_WIDTH; 
		vert [1] .Red    = GetRValue( colF )* 256;
		vert [1] .Green  = GetGValue( colF )* 256;
		vert [1] .Blue   = GetBValue( colF )* 256;
		vert [1] .Alpha  = 0x0000;
		
		gRect.UpperLeft  = 0;
		gRect.LowerRight = 1;
		
		GradientFill(hButtDc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

		/* 
			now draw the bottom gradient 
			this always starts with the button face color 
			ending color depends on button state (selected or not) 
		*/
		vert [0] .x      = LEFT;
		vert [0] .y      = BOTTOM - GRADIENT_WIDTH;
		vert [0] .Red    = GetRValue( colF )* 256;
		vert [0] .Green  = GetGValue( colF )* 256;
		vert [0] .Blue   = GetBValue( colF )* 256;
		vert [0] .Alpha  = 0x0000;
		vert [1] .x      = RIGHT;
		vert [1] .y      = BOTTOM; 
		vert [1] .Red    = GetRValue( color2 )* 256;
		vert [1] .Green  = GetGValue( color2 )* 256;
		vert [1] .Blue   = GetBValue( color2 )* 256;
		vert [1] .Alpha  = 0x0000;
		gRect.UpperLeft  = 0;
		gRect.LowerRight = 1;
		GradientFill(hButtDc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

	}

	/* draw delimiting lines in shadow color */
	newPen = CreatePen( PS_SOLID, 0, colD );
	oldPen = SelectObject( hButtDc, newPen );

	MoveTo( hButtDc, LEFT, TOP );
	LineTo( hButtDc, LEFT, BOTTOM );
	MoveTo( hButtDc, RIGHT, TOP );
	LineTo( hButtDc, RIGHT, BOTTOM );
	
	DeleteObject( SelectObject( hButtDc, oldPen ) );
		
	color2 = GetSysColor( COLOR_BTNSHADOW );
	color1 = RGB( bm->colormap[ 1 ].rgbRed, bm->colormap[ 1 ].rgbGreen, bm->colormap[ 1 ].rgbBlue );

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

LRESULT CALLBACK pushButt(
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
	case WM_LBUTTONUP:
		/* don't know why but this solves a problem with color selection */
		Sleep( 0 );
		break;
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

	oldButtProc = (WNDPROC) SetWindowLongPtr(b->hWnd, GWL_WNDPROC, (LONG_PTR)&pushButt);
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
