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
 * List Boxes
 *
 *****************************************************************************
 */

static XWNDPROC oldListProc = NULL;
static XWNDPROC newListProc;
static XWNDPROC oldComboProc = NULL;
static XWNDPROC newComboProc;

struct wList_t {
		WOBJ_COMMON
		int count;
		int last;
		long * valueP;
		wListCallBack_p action;
		wBool_t editable;
		int colCnt;
		wPos_t * colWidths;
		wBool_t * colRightJust;
		const char * * colTitles;
		wPos_t maxWidth;
		wPos_t scrollPos;
		HWND hScrollWnd;
		wPos_t scrollH;
		wPos_t dragPos;
		int dragCol;
		wPos_t dragColWidth;
		};


typedef struct {
		void * itemContext;
		wIcon_p bm;
		wBool_t selected;
		} listData;

static int LIST_HEIGHT = 19;
static int listTitleHeight = 16;


void wListClear(
		wList_p b )
{
	UINT msg;
	if (b->type==B_LIST)
		msg = LB_RESETCONTENT;
	else
		msg = CB_RESETCONTENT;
	SendMessage( b->hWnd, msg, 0, 0 );
	b->last = -1;
	b->count = 0;
}



void wListSetSize( wList_p bl, wPos_t w, wPos_t h )
{
	int rc;
	RECT rect;
	wPos_t y;

	bl->w = w;
	bl->h = h;
	y = bl->y;	  
	if ( bl->hScrollWnd && bl->maxWidth > bl->w )
		h -= bl->scrollH;
	if ( bl->colTitles ) {
		h -= listTitleHeight;
		y += listTitleHeight;
	}
	rc = SetWindowPos( bl->hWnd, HWND_TOP, 0, 0,
		w, h, SWP_NOMOVE|SWP_NOZORDER);
	if ( bl->hScrollWnd ) {
		if ( bl->maxWidth > bl->w ) {
			GetClientRect( bl->hWnd, &rect );
			rc = SetWindowPos( bl->hScrollWnd, HWND_TOP, bl->x, y+rect.bottom+2,
				bl->w, bl->scrollH, SWP_NOZORDER);
			ShowWindow( bl->hScrollWnd, SW_SHOW );
		} else {
			ShowWindow( bl->hScrollWnd, SW_HIDE );
		}
	}

}


void wListSetIndex(
		wList_p bl,
		int index )
{
	listData * ldp;

	wListGetCount(bl);
	if ( index >= bl->count )
		index = bl->count-1;
	if ( bl->last == index && index == -1 )
		return;
	if ( bl->type==B_LIST && (bl->option&BL_MANY) != 0 ) {
		if ( bl->last != -1 )
			SendMessage( bl->hWnd, LB_SETSEL, 0, MAKELPARAM(bl->last,0) );
		if ( index >= 0 )
			SendMessage( bl->hWnd, LB_SETSEL, 1, MAKELPARAM(index, 0) );
	} else {
		SendMessage( bl->hWnd,
				bl->type==B_LIST?LB_SETCURSEL:CB_SETCURSEL, index, 0 );
	}
	if ( bl->last >= 0 ) {
		ldp = (listData*)SendMessage( bl->hWnd,
				(bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
				bl->last, 0L );
		if ( ldp && ldp!=(void*)LB_ERR )
			ldp->selected = FALSE;
	}
	if ( index >= 0 ) {
		ldp = (listData*)SendMessage( bl->hWnd,
				(bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
				index, 0L );
		if ( ldp && ldp!=(void*)LB_ERR )
			ldp->selected = TRUE;
	}
	/*if (b->option&BL_ICON)*/
		InvalidateRect( bl->hWnd, NULL, FALSE );
	bl->last = index;
}


wIndex_t wListGetIndex(
		wList_p b )
{
	return b->last;
}


void wListSetActive(
		wList_p b,
		int inx,
		wBool_t active )
{
}


void wListSetEditable(
		wList_p b,
		wBool_t editable )
{
	b->editable = editable;
}


void wListSetValue(
		wList_p bl,
		const char * val )
{
	if ( bl->type == B_DROPLIST ) {
		SendMessage( bl->hWnd, WM_SETTEXT, 0, (DWORD)(LPSTR)val );
		bl->last = -1;
	}
}


wIndex_t wListFindValue(
		wList_p bl,
		const char * val )
{
	wIndex_t inx;
	WORD cnt;
	wListGetCount(bl);
	for ( inx = 0; inx < bl->count ; inx++ ) {
		cnt = (int)SendMessage( bl->hWnd,
				(bl->type==B_LIST?LB_GETTEXT:CB_GETLBTEXT), inx,
				(DWORD)(LPSTR)mswTmpBuff );
		mswTmpBuff[cnt] = '\0';
		if ( strcmp( val, mswTmpBuff ) == 0 )
			return inx;
	}
	return -1;
}


wIndex_t wListGetValues(
		wList_p bl,
		char * s,
		int siz,
		void * * listContextRef,
		void * * itemContextRef )
{
	WORD cnt;
	WORD msg;
	WORD inx = bl->last;
	listData *ldp = NULL;
	if ( bl->type==B_DROPLIST && bl->last < 0 ) {
		msg = WM_GETTEXT;
		inx = sizeof mswTmpBuff;
	} else {
		if ( bl->last < 0 )
			goto EMPTY;
		if ( bl->type==B_LIST ) {
			msg = LB_GETTEXT;
		} else {
			msg = CB_GETLBTEXT;
		}
	}
	cnt = (int)SendMessage( bl->hWnd, msg, inx, (DWORD)(LPSTR)mswTmpBuff );
	mswTmpBuff[cnt] = '\0';
	if (s) {
		strncpy(s, mswTmpBuff, siz);
		s[siz] = '\0';
	}
	if (bl->last >= 0) {
		ldp = (listData*)SendMessage( bl->hWnd,
				(bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
				bl->last, 0L );
		if ( ldp==(listData*)LB_ERR )
			ldp = NULL;
	} else {
		ldp = NULL;
	}
EMPTY:
	if (itemContextRef)
		*itemContextRef = (ldp?ldp->itemContext:NULL);
	if (listContextRef)
		*listContextRef = bl->data;
	return bl->last;
}

wBool_t wListSetValues(
		wList_p b,
		wIndex_t inx,
		const char * labelStr,
		wIcon_p bm,
		void * itemData )
{
	listData * ldp;		
	WORD curSel;
	ldp = (listData*)malloc( sizeof *ldp );
	ldp->itemContext = itemData;
	ldp->bm = bm;
	ldp->selected = FALSE;
	if ( (b->option&BL_MANY) == 0 )
		curSel = (WORD)SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_GETCURSEL:CB_GETCURSEL,
				(WPARAM)0,
				(DWORD)0L );
	SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_DELETESTRING:CB_DELETESTRING,
				(WPARAM)inx,
				(DWORD)0L );
	inx = (wIndex_t)SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_INSERTSTRING:CB_INSERTSTRING,
				(WPARAM)inx,
				(DWORD)(LPSTR)labelStr );
	SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_SETITEMDATA:CB_SETITEMDATA,
				(WPARAM)inx,
				(DWORD)ldp );
	if ( (b->option&BL_MANY) == 0 && curSel == (WORD)inx)
			SendMessage( b->hWnd,
					(UINT)b->type==B_LIST?LB_SETCURSEL:CB_SETCURSEL,
					(WPARAM)inx,
					(DWORD)0L );
	/*if (b->option&BL_ICON)*/
		InvalidateRect( b->hWnd, NULL, FALSE );
	return TRUE;
}


void wListDelete(
		wList_p b,
		wIndex_t inx )
{
	SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_DELETESTRING:CB_DELETESTRING,
				(WPARAM)inx,
				(DWORD)0L );
}


/**
 * Select all items in list.
 *
 * \param bl IN list handle
 * \return
 */

void wListSelectAll( wList_p bl )
{
	wIndex_t inx;
	listData *ldp;

	// mark all items selected
	SendMessage( bl->hWnd,
				 LB_SETSEL,
				 (WPARAM)TRUE,
				 (DWORD)-1L );

	// and synchronize the internal data structures 
	wListGetCount(bl);
	for ( inx=0; inx<bl->count; inx++ ) {
		ldp = (listData*)SendMessage( bl->hWnd,
									  (bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
									   inx, 0L );
		ldp->selected = TRUE;
		SendMessage( bl->hWnd,
					(UINT)bl->type==B_LIST?LB_SETITEMDATA:CB_SETITEMDATA,
					(WPARAM)inx,
					(DWORD)ldp );
	}
}


wIndex_t wListGetCount(
		wList_p bl )
{			  
	bl->count = (int)SendMessage( bl->hWnd, (UINT)bl->type==B_LIST?LB_GETCOUNT:CB_GETCOUNT, 0, 0L );
	return bl->count;
}


void * wListGetItemContext(
		wList_p bl,
		wIndex_t inx )
{
	listData * ldp;
	wListGetCount(bl);
	if ( inx < 0 || inx >= bl->count ) return NULL;
	ldp = (listData*)SendMessage( bl->hWnd,
				(bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
				inx, 0L );
	return ((ldp&&ldp!=(void*)LB_ERR)?ldp->itemContext:NULL);
}


wBool_t wListGetItemSelected(
		wList_p bl,
		wIndex_t inx )
{
	listData * ldp;
	wListGetCount(bl);
	if ( inx < 0 || inx >= bl->count ) return FALSE;
	ldp = (listData*)SendMessage( bl->hWnd,
				(bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
				inx, 0L );
	return ((ldp&&ldp!=(void*)LB_ERR)?ldp->selected:FALSE);
}


wIndex_t wListGetSelectedCount(
		wList_p bl )
{
	wIndex_t selcnt, inx;
	wListGetCount(bl);
	for ( selcnt=inx=0; inx<bl->count; inx++ )
		if ( wListGetItemSelected( bl, inx ) )
			selcnt++;
	return selcnt;
}




wIndex_t wListAddValue(
		wList_p b,
		const char * value,
		wIcon_p bm,
		void * itemContext )
{
	int nindex;
	listData * ldp;
	ldp = (listData*)malloc( sizeof *ldp );
	ldp->itemContext = itemContext;
	ldp->bm = bm;
	ldp->selected = FALSE;
	if ( value == NULL )
		value = "";
	b->count++;
	nindex = (int)SendMessage(
				b->hWnd,
				(UINT)b->type==B_LIST?LB_ADDSTRING:CB_ADDSTRING,
				(WPARAM)0,
				(DWORD)value );
	if (nindex == 0) {
		SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_SETCURSEL:CB_SETCURSEL,
				(WPARAM)nindex,
				(DWORD)0 );
		b->last = 0;
	}
	SendMessage( b->hWnd,
				(UINT)b->type==B_LIST?LB_SETITEMDATA:CB_SETITEMDATA,
				(WPARAM)nindex,
				(DWORD)ldp );
	return nindex;
}


int wListGetColumnWidths(
		wList_p bl,
		int colCnt,
		wPos_t * colWidths )
{
	wIndex_t inx;

	if ( bl->type != B_LIST )
		return 0;
	if ( bl->colWidths == NULL )
		return 0;
	for ( inx=0; inx<colCnt; inx++ ) {
		if ( inx < bl->colCnt )
			colWidths[inx] = bl->colWidths[inx];
		else
			colWidths[inx] = 0;
	}
	return bl->colCnt;
}


static void listSetBusy(
		wControl_p b,
		BOOL_T busy)
{
	wList_p bl = (wList_p)b;

	EnableWindow( bl->hWnd, !(BOOL)busy );
	if ( bl->hScrollWnd )
		EnableWindow( bl->hScrollWnd, !(BOOL)busy );
}

static void listShow(
		wControl_p b,
		BOOL_T show)
{
	wList_p bl = (wList_p)b;

	ShowWindow( bl->hWnd, show?SW_SHOW:SW_HIDE );
	if ( bl->hScrollWnd && bl->maxWidth > bl->w )
		ShowWindow( bl->hScrollWnd, show?SW_SHOW:SW_HIDE );
#ifdef SHOW_DOES_SETFOCUS
	if ( show && (bl->option&BO_READONLY)==0 )
		hWnd = SetFocus( bl->hWnd );
#endif
}

static void listSetPos(
		wControl_p b,
		wPos_t x,
		wPos_t y )
{
	wList_p bl = (wList_p)b;
	wPos_t x1, y1;
	RECT rect;
	
	bl->x = x1 = x;
	bl->y = y1 = y;
	if ( bl->colTitles )
		y1 += listTitleHeight;
	if (!SetWindowPos( b->hWnd, HWND_TOP, x1, y1,
				CW_USEDEFAULT, CW_USEDEFAULT,
				SWP_NOSIZE|SWP_NOZORDER))
				mswFail("listSetPos");
	if ( bl->hScrollWnd && bl->maxWidth > bl->w ) {
		GetClientRect( bl->hWnd, &rect );
		if (!SetWindowPos( bl->hScrollWnd, HWND_TOP, x1, y1+rect.bottom+2,
				CW_USEDEFAULT, CW_USEDEFAULT,
				SWP_NOSIZE|SWP_NOZORDER))
				mswFail("listSetPos2");
	}
}


static void listRepaintLabel(
		HWND hWnd,
		wControl_p b )
{
	wList_p bl = (wList_p)b;
	HDC hDc;
	RECT rc;
	HFONT hFont;
	HPEN hPen0, hPen1, hPen2, hPen3;
	HBRUSH hBrush;
	const char * * title;
	int inx;
	int start;
	wPos_t colWidth;

	mswRepaintLabel( hWnd, b );
	if ( bl->colTitles == NULL )
		return;
	hDc = GetDC( hWnd );
	start = bl->x-bl->scrollPos+2;
	rc.top = bl->y;
	rc.bottom = bl->y+listTitleHeight;
	rc.left = bl->x-1;
	rc.right = bl->x+bl->w;
	hBrush = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
	FillRect( hDc, &rc, hBrush );
	SetBkColor( hDc, GetSysColor( COLOR_BTNFACE ) );
	
	hFont = SelectObject( hDc, mswLabelFont );
	hPen1 = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNTEXT ) );
	hPen2 = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNHIGHLIGHT ) );
	hPen3 = CreatePen( PS_SOLID, 0, GetSysColor( COLOR_BTNSHADOW ) );
	hPen0 = SelectObject( hDc, hPen1 );
	MoveTo( hDc, rc.left, rc.top );
	LineTo( hDc, rc.right, rc.top );
	LineTo( hDc, rc.right, rc.bottom );
	LineTo( hDc, rc.left, rc.bottom );
	LineTo( hDc, rc.left, rc.top );
	SelectObject( hDc, hPen2 );
	MoveTo( hDc, rc.left+1, rc.bottom-1 );
	LineTo( hDc, rc.left+1, rc.top+1 );
	LineTo( hDc, rc.right-1, rc.top+1 );
	SelectObject( hDc, hPen3 );
	MoveTo( hDc, rc.left+2, rc.bottom-1 );
	LineTo( hDc, rc.right-1, rc.bottom-1 );
	LineTo( hDc, rc.right-1, rc.top+1 );
	rc.top += 2;
	rc.bottom -= 1;
	for ( inx=0,title=bl->colTitles; inx<bl->colCnt&&*title&&start<bl->x+bl->w; inx++ ) {
		colWidth = bl->colWidths[inx];
		if ( start+colWidth >= 3 ) {
			rc.left = start;
			if ( rc.left < bl->x+2 )
				rc.left = bl->x+2;
			rc.right = start+colWidth;
			if ( rc.right > bl->x+bl->w-1 )
				rc.right = bl->x+bl->w-1;
			ExtTextOut( hDc, start+1, rc.top+0,
				ETO_CLIPPED|ETO_OPAQUE, &rc,
				*title, strlen(*title), NULL );
			if ( start-bl->x >= 3 ) {
			SelectObject( hDc, hPen1 );
			MoveTo( hDc, start-1, rc.top-1 );
			LineTo( hDc, start-1, rc.bottom+3 );
			SelectObject( hDc, hPen2 );
			MoveTo( hDc, start, rc.top );
			LineTo( hDc, start, rc.bottom+1 );
			SelectObject( hDc, hPen3 );
			MoveTo( hDc, start-2, rc.top );
			LineTo( hDc, start-2, rc.bottom+1 );
			}
		}
		title++;
		start += colWidth;
	}
	SelectObject( hDc, hPen0 );
	SelectObject( hDc, hFont );
	DeleteObject( hBrush );
	DeleteObject( hPen1 );
	DeleteObject( hPen2 );
	DeleteObject( hPen3 );
}


#ifdef LATER
static void listHandleSelectionState( LPDRAWITEMSTRUCT lpdis, LPRECT rc )
{
	int oldROP;
	oldROP = SetROP2( lpdis->hDC, R2_NOT );
	Rectangle( lpdis->hDC, rc->left, rc->top, rc->right, rc->bottom );
	SetROP2( lpdis->hDC, oldROP );
	/*InvertRect( lpdis->hDC, rc );*/
}
#endif

static void listHandleFocusState( LPDRAWITEMSTRUCT lpdis, LPRECT rc )
{
	DrawFocusRect( lpdis->hDC, rc );
}


LRESULT listProc(
		wControl_p b,
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam )
{						
	wList_p bl = (wList_p)b;
	int cnt, inx, selected;
	long len;
	listData * ldp;
	HDC hDc;
	LPMEASUREITEMSTRUCT lpmis;
	TEXTMETRIC tm;
	LPDRAWITEMSTRUCT lpdis;
	RECT rc, rc1;
	char * cp0, * cp1;
	wPos_t colWidth, x;
	int nPos;
	HFONT hFont;
	HPEN hPen;
	HBRUSH hBrush;
	WPARAM notification;
	COLORREF col;

	if (bl) switch( message ) {
	
	case WM_COMMAND:
		notification = WCMD_PARAM_NOTF;
		switch (bl->type) {
		case B_LIST:
			switch (notification) {
			case LBN_SELCHANGE:
			case LBN_DBLCLK:
				if ( (bl->option&BL_DBLCLICK)!=0 ?
						notification!=LBN_DBLCLK :
						notification==LBN_DBLCLK )
					break;
				if ( (bl->option&BL_MANY) ) {
					wListGetCount(bl);
					for ( inx=0; inx<bl->count; inx++ ) {
					ldp = (listData*)SendMessage( bl->hWnd, LB_GETITEMDATA, inx, 0L );
					if ( ldp != NULL && ldp != (void*)LB_ERR ) {
						selected = ((long)SendMessage( bl->hWnd, LB_GETSEL, inx, 0L ) != 0L );
						if ( selected != ldp->selected ) {
							ldp->selected = selected;
							if ( selected ) {
								bl->last = inx;
								cnt = (int)SendMessage( bl->hWnd, LB_GETTEXT, bl->last, (DWORD)(LPSTR)mswTmpBuff );
								mswTmpBuff[cnt] = '\0';
							} else {
								mswTmpBuff[0] = '\0';
							}
							if ( bl->action )
								bl->action( inx, mswTmpBuff, selected?1:2, bl->data, ldp->itemContext );
							if ( selected && bl->valueP )
								*bl->valueP = bl->last;
						}
					}
					}
				} else {
					bl->last = (int)SendMessage( bl->hWnd, LB_GETCURSEL, 0, 0L );
					cnt = (int)SendMessage( bl->hWnd, LB_GETTEXT, bl->last,
										(DWORD)(LPSTR)mswTmpBuff );
					mswTmpBuff[cnt] = '\0';
					if (bl->action) {
						ldp = (listData*)SendMessage( bl->hWnd, LB_GETITEMDATA,
										bl->last, 0L );
						bl->action( bl->last, mswTmpBuff, 1, bl->data,
							((bl->last>=0&&ldp&&ldp!=(void*)LB_ERR)?ldp->itemContext:NULL) );
					}
					if (bl->valueP) {
						*bl->valueP = bl->last;
					}
				}
				break;

			case LBN_KILLFOCUS:
				if ( ( bl->option&BL_MANY ) == 0 &&
					 bl->last != (int)SendMessage( bl->hWnd, LB_GETCURSEL, 0, 0L ) )
					(void)SendMessage( bl->hWnd, LB_SETCURSEL, bl->last, 0L );
				break;
			}
			break;

		case B_DROPLIST:
		case B_COMBOLIST:
			switch (notification) {
			case CBN_SELCHANGE:
			case CBN_DBLCLK:
				if ( (bl->type == B_DROPLIST) ||
					 ( (bl->option&BL_DBLCLICK)!=0 ?
						notification!=CBN_DBLCLK :
						notification==CBN_DBLCLK) )
					break;

			case CBN_CLOSEUP:
				bl->last = (int)SendMessage( bl->hWnd, CB_GETCURSEL, 0, 0L );
				if (bl->last < 0)
					break;
				if (bl->action) {
					cnt = (int)SendMessage( bl->hWnd, CB_GETLBTEXT, bl->last,
										(DWORD)(LPSTR)mswTmpBuff );
					ldp = (listData*)SendMessage( bl->hWnd, CB_GETITEMDATA,
										bl->last, 0L );
					mswTmpBuff[cnt] = '\0';
					bl->action( bl->last, mswTmpBuff, 1, bl->data,
						((bl->last>=0&&ldp&&ldp!=(void*)LB_ERR)?ldp->itemContext:NULL) );
				}
				if (bl->valueP) {
					*bl->valueP = bl->last;
				}
				mswAllowBalloonHelp = TRUE;
				/*SendMessage( bl->bWnd, CB_SETCURSEL, bl->last, 0L );*/
				break;

			case CBN_KILLFOCUS:
				inx = (int)SendMessage( bl->hWnd, CB_GETCURSEL, 0, 0L );
				if ( bl->last != inx )
					(void)SendMessage( bl->hWnd, CB_SETCURSEL, bl->last, 0L );
				break;

			case CBN_DROPDOWN:
				mswAllowBalloonHelp = FALSE;
				break;

			case CBN_EDITCHANGE:
				bl->last = -1;
				if (bl->action) {
					cnt = (int)SendMessage( bl->hWnd, WM_GETTEXT, sizeof mswTmpBuff,
										(DWORD)(LPSTR)mswTmpBuff );
					mswTmpBuff[cnt] = '\0';
					bl->action( -1, mswTmpBuff, 1, bl->data, NULL );
				}
				break;
			}
			break;
		}
		break;

	case WM_MEASUREITEM:
		lpmis = (LPMEASUREITEMSTRUCT)lParam;
		hDc = GetDC( hWnd );
		if ( bl->type == B_LIST )
			hFont = SelectObject( hDc, mswLabelFont );
		GetTextMetrics( hDc, &tm );
		lpmis->itemHeight = tm.tmHeight;			  
		if ( bl->type == B_LIST )
			SelectObject( hDc, hFont );
		ReleaseDC( hWnd, hDc );
		break;

	case WM_DRAWITEM:
		lpdis = (LPDRAWITEMSTRUCT)lParam;
		if (lpdis->itemID == -1) {
			listHandleFocusState(lpdis, &lpdis->rcItem);
			return TRUE;
		}
		ldp = (listData*)SendMessage( bl->hWnd,
						(bl->type==B_LIST?LB_GETITEMDATA:CB_GETITEMDATA),
						lpdis->itemID, 0L );
		rc = lpdis->rcItem;
		if (lpdis->itemAction & (ODA_DRAWENTIRE|ODA_SELECT|ODA_FOCUS)) {
			if( bl->type == B_LIST )
				hFont = SelectObject( lpdis->hDC, mswLabelFont );
			cnt = (int)SendMessage( lpdis->hwndItem, 
				(bl->type==B_LIST?LB_GETTEXT:CB_GETLBTEXT),
				lpdis->itemID, (LONG)(LPSTR)mswTmpBuff );
			mswTmpBuff[cnt] = '\0';
			if ( lpdis->itemState & ODS_SELECTED ) {
				SetTextColor( lpdis->hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
				SetBkColor( lpdis->hDC, GetSysColor( COLOR_HIGHLIGHT ) );
			} else {
				SetTextColor( lpdis->hDC, GetSysColor( COLOR_WINDOWTEXT ) );
				SetBkColor( lpdis->hDC, GetSysColor( COLOR_WINDOW ) );
			}
			rc1 = rc;
			rc1.left -= bl->scrollPos;
			for ( inx=0,cp0=mswTmpBuff; inx<bl->colCnt&&cp0&&rc1.left<rc.right; inx++ ) {
				if ( inx>=bl->colCnt-1 || (cp1=strchr(cp0,'\t')) == NULL ) {
					len = strlen( cp0 );
					cp1=cp0 + len; // JBB, to avoid an MSC error below where cp1 has not been defined.
				} else {
					len = cp1-cp0;
					cp1 ++;
				}
				if ( bl->colWidths ) {
					colWidth = bl->colWidths[inx];
				} else {
					colWidth = rc.right;
				}
				if ( inx == 0 && ldp && ldp!=(void*)LB_ERR && ldp->bm ) {
					if (mswPalette) {
						SelectPalette( lpdis->hDC, mswPalette, 0 );
						cnt = RealizePalette( lpdis->hDC );
					}
					hPen = SelectObject( lpdis->hDC, CreatePen( PS_SOLID, 0, GetSysColor( COLOR_WINDOW ) ) );
					hBrush = SelectObject( lpdis->hDC, CreateSolidBrush( GetSysColor( COLOR_WINDOW ) ) );
					Rectangle( lpdis->hDC, rc1.left, rc1.top, rc1.right, rc1.bottom );
					DeleteObject( SelectObject( lpdis->hDC, hPen ) );
					DeleteObject( SelectObject( lpdis->hDC, hBrush ) );

					col = RGB(	(ldp->bm->colormap[ 1 ]).rgbRed, 
								(ldp->bm->colormap[ 1 ]).rgbGreen,
								(ldp->bm->colormap[ 1 ]).rgbBlue );
					mswDrawIcon( lpdis->hDC, rc1.left+2, rc.top+0, ldp->bm, 0, col, col);

					rc1.left += ldp->bm->w+6;
					colWidth -= ldp->bm->w+6;
				}
				if ( inx>=bl->colCnt-1 || (rc1.right = rc1.left + colWidth) > rc.right )
					 rc1.right = rc.right;
				if ( rc1.right > 0 && rc1.left+3 < rc.right ) {
					ExtTextOut( lpdis->hDC, rc1.left+3, rc1.top+1,
						ETO_CLIPPED | ETO_OPAQUE, &rc1,
						(LPSTR)cp0, (int)len, NULL );
				}
				rc1.left = rc1.right;
				cp0 = cp1;
			}
			if ( lpdis->itemState & ODS_SELECTED ) {
				SetTextColor( lpdis->hDC, GetSysColor( COLOR_WINDOWTEXT ) );
				SetBkColor( lpdis->hDC, GetSysColor( COLOR_WINDOW ) );
			}
			if (lpdis->itemState & ODS_FOCUS) {
				DrawFocusRect( lpdis->hDC, &rc );
			}
			if ( bl->type == B_LIST)
				SelectObject( lpdis->hDC, hFont );
			return TRUE;
		}
		
		break;

	case WM_HSCROLL:
		len = ((long)bl->maxWidth)-((long)bl->w);
		if ( len <= 0 )
			return 0;
		switch ( WSCROLL_PARAM_CODE ) {
		case SB_LEFT:
			if ( bl->scrollPos == 0 )
				return 0;
			bl->scrollPos = 0;
			break;
		case SB_LINELEFT:
		case SB_PAGELEFT:
			if ( bl->scrollPos == 0 )
				return 0;
			for ( inx=colWidth=0; inx<bl->colCnt; inx++ ) {
				if ( colWidth+bl->colWidths[inx] >= bl->scrollPos ) {
					bl->scrollPos = colWidth; 
					break;
				}
				colWidth += bl->colWidths[inx];
			}
			break;
		case SB_LINERIGHT:
		case SB_PAGERIGHT:
			if ( bl->scrollPos >= len )
				return 0;
			for ( inx=colWidth=0; inx<bl->colCnt; inx++ ) {
				if ( colWidth >= bl->scrollPos ) {
					bl->scrollPos = colWidth+bl->colWidths[inx];
					break;
				}
				colWidth += bl->colWidths[inx];
			}
			break;
		case SB_RIGHT:
			if ( bl->scrollPos >= len )
				return 0;
			bl->scrollPos = (int)len;
			break;
		case SB_THUMBTRACK:	  
			return 0;
		case SB_THUMBPOSITION:
			nPos = (int)WSCROLL_PARAM_NPOS;
			bl->scrollPos = (int)(len*nPos/100);
			break;
		case SB_ENDSCROLL:
			return 0;
		}
		if ( bl->scrollPos > len ) bl->scrollPos = (int)len;
		if ( bl->scrollPos < 0 ) bl->scrollPos = 0;
		nPos = (int)(((long)bl->scrollPos)*100L/len+0.5);
		SetScrollPos( bl->hScrollWnd, SB_CTL, nPos, TRUE );
		InvalidateRect( bl->hWnd, NULL, FALSE );
		listRepaintLabel( ((wControl_p)(bl->parent))->hWnd, (wControl_p)bl );
		return 0;

	case WM_LBUTTONDOWN:
		if ( bl->type != B_LIST )
			break;
		if ( bl->colCnt <= 1 )
			break;
		x = bl->dragPos = LOWORD(lParam)+bl->scrollPos-4;
		bl->dragCol = -1;
		for ( inx=0; inx<bl->colCnt; inx++ ) {
			x -= bl->colWidths[inx];
			if ( x < -5 ) break;
			if ( x <= 0 ) { bl->dragCol = inx; break; }
			if ( x > bl->colWidths[inx+1] ) continue;
			if ( x <= 10 ) { bl->dragCol = inx; break; }
		}
		if ( bl->dragCol >= 0 )
			bl->dragColWidth = bl->colWidths[inx];
		return 0L;

#ifdef LATER
	case WM_MOUSEMOVE:
		if ( (wParam&MK_LBUTTON) == 0 )
			break;
		if ( bl->type != B_LIST )
			break;
		if ( bl->colCnt <= 1 )
			break;
		x = LOWORD(lParam)+bl->scrolPos;
		for ( inx=0; inx<bl->colCnt; inx++ ) {
			x -= bl->colWidths[inx];
			if ( x <= 0 )
				break;
		}
		return 0L;
#endif

	case WM_MOUSEMOVE:
		if ( (wParam&MK_LBUTTON) == 0 )
			break;
	case WM_LBUTTONUP:
		if ( bl->type != B_LIST )
			break;
		if ( bl->colCnt <= 1 )
			break;
		if ( bl->dragCol < 0 )
			break;
		x = LOWORD(lParam)+bl->scrollPos-4-bl->dragPos;			/* WIN32??? */
		bl->colWidths[bl->dragCol] = bl->dragColWidth+x;
		if ( bl->colWidths[bl->dragCol] < 0 )
			bl->colWidths[bl->dragCol] = 0;
		for ( bl->maxWidth=inx=0; inx<bl->colCnt; inx++ )
			bl->maxWidth += bl->colWidths[inx];
		if ( bl->maxWidth <= bl->w ) {
			x = bl->w - bl->maxWidth;
			bl->colWidths[bl->colCnt-1] += x;
			bl->maxWidth = bl->w;
			bl->scrollPos = 0;
		} else {
			if ( bl->scrollPos+bl->w > bl->maxWidth ) {
				bl->scrollPos = bl->maxWidth - bl->w;
			}
		}
		InvalidateRect( bl->hWnd, NULL, FALSE );
		listRepaintLabel( ((wControl_p)(bl->parent))->hWnd, (wControl_p)bl );
		return 0L;

	}													  
	
	return DefWindowProc( hWnd, message, wParam, lParam );
}					

long FAR PASCAL _export pushList(
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
	wControl_p b = mswMapIndex( inx );

	switch (message) {
	case WM_CHAR:
		if ( b != NULL) {
			switch( WCMD_PARAM_ID ) {
			case 0x0D:
			case 0x1B:
			case 0x09:
				SetFocus( ((wControl_p)(b->parent))->hWnd ); 
				SendMessage( ((wControl_p)(b->parent))->hWnd, WM_CHAR,
						wParam, lParam );
				/*SendMessage( ((wControl_p)(b->parent))->hWnd, WM_COMMAND,
						inx, MAKELONG( hWnd, EN_KILLFOCUS ) );*/
				return 0L;
			}
		}
		break;
	}
	return CallWindowProc( oldListProc, hWnd, message, wParam, lParam );
}

long FAR PASCAL _export pushCombo(
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
	wControl_p b = mswMapIndex( inx );

	switch (message) {
	case WM_CHAR:
		if ( b != NULL) {
			switch( WCMD_PARAM_ID ) {
			case 0x0D:
			case 0x1B:
			case 0x09:
				SetFocus( ((wControl_p)(b->parent))->hWnd ); 
				SendMessage( ((wControl_p)(b->parent))->hWnd, WM_CHAR,
						wParam, lParam );
				/*SendMessage( ((wControl_p)(b->parent))->hWnd, WM_COMMAND,
						inx, MAKELONG( hWnd, EN_KILLFOCUS ) );*/
				return 0L;
			}
		}
		break;
	}
	return CallWindowProc( oldComboProc, hWnd, message, wParam, lParam );
}

static callBacks_t listCallBacks = {
		listRepaintLabel,
		NULL,
		listProc,
		listSetBusy,
		listShow,
		listSetPos };


static wList_p listCreate(
		int		typ,
		const char	*className,
		long	style,
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		long	number,
		POS_T	width,
		long	*valueP,
		wListCallBack_p action,
		void	*data,
		wBool_t addFocus,
		int		*indexR )
{ 
	wList_p b;
	RECT rect;	
	int index;

	b = (wList_p)mswAlloc( parent, typ, mswStrdup(labelStr), sizeof *b, data, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option = option;
	b->count = 0;
	b->last = -1;
	b->valueP = valueP;
	b->labelY += 4;
	b->action = action;
	b->maxWidth = 0;
	b->scrollPos = 0;
	b->scrollH = 0;
	b->dragPos = 0;
	b->dragCol = -1;

	b->hWnd = CreateWindow( className, NULL,
				style | WS_CHILD | WS_VISIBLE | mswGetBaseStyle(parent), b->x, b->y,
				width, LIST_HEIGHT*(int)number,
				((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(LIST)");
		return b;
	}

#ifdef CONTROL3D
	Ctl3dSubclassCtl( b->hWnd );
#endif

	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;
	b->colCnt = 1;
	b->colWidths = NULL;
	b->colTitles = NULL;

	mswAddButton( (wControl_p)b, TRUE, helpStr );
	mswCallBacks[typ] = &listCallBacks;
	if (addFocus) {
		mswChainFocus( (wControl_p)b );
		if (b->type == B_LIST) {
			newListProc = MakeProcInstance( (XWNDPROC)pushList, mswHInst );
			oldListProc = (XWNDPROC)GetWindowLong( b->hWnd, GWL_WNDPROC );
			SetWindowLong( b->hWnd, GWL_WNDPROC, (LONG)newListProc );
		} else {
			newComboProc = MakeProcInstance( (XWNDPROC)pushCombo, mswHInst );
			oldComboProc = (XWNDPROC)GetWindowLong( b->hWnd, GWL_WNDPROC );
			SetWindowLong( b->hWnd, GWL_WNDPROC, (LONG)newComboProc );
		}
	}
	if ( indexR )
		*indexR = index;
	if ( !mswThickFont )
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	return b;
}


wList_p wListCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		long	number,
		POS_T	width,
		int		colCnt,
		wPos_t	* colWidths,
		wBool_t * colRightJust,
		const char	* * colTitles,
		long	*valueP,
		wListCallBack_p action,
		void	*data )
{ 
	long bs;
	wList_p bl;
	static int dbu = 0;
	RECT rect;
	int index;
	int i;

	bs = LBS_NOTIFY | WS_VSCROLL | WS_BORDER | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS;
	if (option & BL_MANY)
		bs |= LBS_MULTIPLESEL|LBS_EXTENDEDSEL;
	if (option & BL_SORT)
		bs |= LBS_SORT;
	if ( colCnt > 1 )
		bs |= WS_HSCROLL;
	if ( colTitles ) {
		y += listTitleHeight;
		number -= 1;
	}
	bl = listCreate( B_LIST, "LISTBOX", bs, parent, x, y, helpStr,
						labelStr, option, number, width, valueP, action, data, TRUE, &index );
	if ( colTitles ) {
		bl->y -= listTitleHeight;
		bl->h += listTitleHeight;
	}	 
	if ( colCnt > 1 ) {
		bl->colCnt = colCnt;
		bl->colWidths = (int*)malloc( colCnt * sizeof *bl->colWidths );
		bl->colRightJust = (wBool_t*)malloc( colCnt * sizeof *bl->colRightJust );
		bl->colTitles = colTitles;
		bl->maxWidth = 0;
		memcpy( bl->colWidths, colWidths, colCnt * sizeof *bl->colWidths );
		for ( i=0; i<colCnt; i++ ) {
			bl->colWidths[i] = colWidths[i];
			bl->maxWidth += bl->colWidths[i];
		}
		bl->hScrollWnd = CreateWindow( "ScrollBar", NULL,
				SBS_HORZ | SBS_BOTTOMALIGN | WS_CHILD | WS_VISIBLE | mswGetBaseStyle(parent), bl->x, bl->y,
				width, CW_USEDEFAULT,
				((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
		if (bl->hScrollWnd == NULL)
			mswFail("CreateWindow(LISTSCROLL)");
		SetScrollRange( bl->hScrollWnd, SB_CTL, 0, 100, TRUE );
		GetWindowRect( bl->hScrollWnd, &rect );
		bl->scrollH = rect.bottom - rect.top+2;
	}
	return bl;
}


wList_p wDropListCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		long	number,
		POS_T	width,
		long	*valueP,
		wListCallBack_p action,
		void	*data )
{
	long bs;

	if ( (option&BL_EDITABLE) != 0 )
		bs = CBS_DROPDOWN;
	else
		bs = CBS_DROPDOWNLIST;
	bs |= WS_VSCROLL | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
	if (option & BL_SORT)
		bs |= CBS_SORT;
	return listCreate( B_DROPLIST, "COMBOBOX", bs, parent, x, y, helpStr,
						labelStr, option, number, width, valueP, action, data, TRUE, NULL );
}

wList_p wComboListCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		long	number,
		POS_T	width,
		long	*valueP,
		wListCallBack_p action,
		void	*data )
{
	long bs;

	bs = CBS_SIMPLE | WS_VSCROLL | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
	if (option & BL_SORT)
		bs |= CBS_SORT;
	return listCreate( B_COMBOLIST, "COMBOBOX", bs, parent, x, y, helpStr,
						labelStr, option, number, width, valueP, action, data, FALSE, NULL );
}
