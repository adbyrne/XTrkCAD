#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include "mswint.h"

/*
 *****************************************************************************
 *
 * Multi-line Text Boxes
 *
 *****************************************************************************
 */

static LOGFONT fixedFont = {
		/* Initial default values */
		-18, 0, /* H, W */
		0,		/* A */
		0,
		FW_REGULAR,
		0, 0, 0,/* I, U, SO */
		ANSI_CHARSET,
		0,		/* OP */
		0,		/* CP */
		0,		/* Q */
		FIXED_PITCH|FF_MODERN,	/* P&F */
		"Courier" };
static HFONT fixedTextFont, prevTextFont;

struct wText_t {
		WOBJ_COMMON
		HANDLE hText;
		};

BOOL_T textPrintAbort = FALSE;


void wTextClear(
		wText_p b )
{
	long rc;
	rc = SendMessage( b->hWnd, EM_SETREADONLY, 0, 0L );
#ifdef WIN32
	rc = SendMessage( b->hWnd, EM_SETSEL, 0, -1 );
#else
	rc = SendMessage( b->hWnd, EM_SETSEL, 1, MAKELONG( 0, -1 ) );
#endif
	rc = SendMessage( b->hWnd, WM_CLEAR, 0, 0L );
	if ( b->option&BO_READONLY )
		rc = SendMessage( b->hWnd, EM_SETREADONLY, 1, 0L );
}


void wTextAppend(
		wText_p b,
		const char * text )
{
	HANDLE hMem;
	char * pMem, *cp;
	int len, textSize;
	long rc;
	long lc;
	len = strlen(text);
	if (len <= 0)
		return;
	for (cp= CAST_AWAY_CONST text; *cp; cp++) {
		if ( *cp == '\n' )
			len++;
	}
	hMem = GlobalAlloc( GHND, (DWORD)len+10+1 );
	pMem = (char*)GlobalLock( hMem );
	for (cp=pMem; *text; cp++,text++) {
		if (*text == '\n') {
			*cp++ = '\r';
			*cp = '\n';
		} else
			*cp = *text;
	}
	textSize = LocalSize( b->hText );
	if ((long)textSize+(long)len > 10000L) {
		if (len < 1024)
			len = 1024;
#ifdef WIN32
		rc = SendMessage( b->hWnd, EM_SETSEL, 0, len );
#else
		rc = SendMessage( b->hWnd, EM_SETSEL, 0, MAKELONG( 0, len ) );
#endif
		rc = SendMessage( b->hWnd, WM_CLEAR, 0, 0L );
#ifdef WIN32
		rc = SendMessage( b->hWnd, EM_SCROLLCARET, 0, 0 );
#else
		rc = SendMessage( b->hWnd, EM_SETSEL, 0, MAKELONG( 32767, 32767 ) );
#endif
	}
	lc = SendMessage( b->hWnd, EM_GETFIRSTVISIBLELINE, 0, 0L );
	if ( lc < 0 )
		lc = 0;
	GlobalUnlock( hMem );
	rc = OpenClipboard( b->hWnd );
	rc = EmptyClipboard();
	rc = (long)SetClipboardData( CF_TEXT, hMem );
	rc = CloseClipboard();
	rc = SendMessage( b->hWnd, EM_SETREADONLY, 0, 0L );
	rc = SendMessage( b->hWnd, WM_PASTE, 0, 0L );
	lc -= SendMessage( b->hWnd, EM_GETFIRSTVISIBLELINE, 0, 0L );
#ifdef LATER
	if ( lc < 0 )
		SendMessage( b->hWnd, EM_LINESCROLL, 0, MAKELPARAM((WPARAM)lc,0) );
#endif
	lc = GetWindowTextLength( b->hWnd );
	if ( b->option&BO_READONLY )
		rc = SendMessage( b->hWnd, EM_SETREADONLY, 1, 0L );
}


BOOL_T wTextSave(
		wText_p b,
		const char * fileName )
{
	FILE * f;
	int lc, l, len;
	char line[255];

	f = wFileOpen( fileName, "w" );
	if (f == NULL) {
		MessageBox( ((wControl_p)(b->parent))->hWnd, "TextSave", "", MB_OK|MB_ICONHAND );
		return FALSE;
	}

	lc = (int)SendMessage( b->hWnd, EM_GETLINECOUNT, 0, 0L );

	for ( l=0; l<lc; l++ ) {
		*(WORD*)line = sizeof(line)-1;
		len = (int)SendMessage( b->hWnd, EM_GETLINE, l, (DWORD)(LPSTR)line );
		line[len] = '\0';
		fprintf( f, "%s\n", line );
	}
	fclose( f );
	return TRUE;
}


BOOL_T wTextPrint(
		wText_p b )
{
	int lc, l, len;
	char line[255];
	HDC hDc;
	int lineSpace;
	int linesPerPage;
	int currentLine;
	int IOStatus;
	TEXTMETRIC textMetric;
	DOCINFO docInfo;

	hDc = mswGetPrinterDC();
	if (hDc == (HDC)0) {
		MessageBox( ((wControl_p)(b->parent))->hWnd, "Print", "Cannot print", MB_OK|MB_ICONHAND );
		return FALSE;
	}
	docInfo.cbSize = sizeof(DOCINFO);
	docInfo.lpszDocName = "XTrkcad Log";
	docInfo.lpszOutput = (LPSTR)NULL;
	if (StartDoc(hDc, &docInfo) < 0) {
		MessageBox( ((wControl_p)(b->parent))->hWnd, "Unable to start print job", NULL, MB_OK|MB_ICONHAND );
		DeleteDC( hDc );
		return FALSE;
	}
	StartPage( hDc );
	GetTextMetrics( hDc, &textMetric );
	lineSpace = textMetric.tmHeight + textMetric.tmExternalLeading;
	linesPerPage = GetDeviceCaps( hDc, VERTRES ) / lineSpace;
	currentLine = 1;

	lc = (int)SendMessage( b->hWnd, EM_GETLINECOUNT, 0, 0L );

	IOStatus = 0;
	for ( l=0; l<lc; l++ ) {
		*(WORD*)line = sizeof(line)-1;
		len = (int)SendMessage( b->hWnd, EM_GETLINE, l, (DWORD)(LPSTR)line );
		TextOut( hDc, 0, currentLine*lineSpace, line, len );
		if (++currentLine > linesPerPage) {
			EndPage( hDc );
			currentLine = 1;
			IOStatus = EndPage(hDc);
			if (IOStatus < 0 || textPrintAbort )
				break;
			StartPage( hDc );
		}
	}
	if (IOStatus >= 0 && !textPrintAbort ) {
		EndPage( hDc );
		EndDoc( hDc );
	}
	DeleteDC( hDc );
	return TRUE;
}


wBool_t wTextGetModified(
		wText_p b )
{
	int rc;
	rc = (int)SendMessage( b->hWnd, EM_GETMODIFY, 0, 0L );
	return (wBool_t)rc;
}

int wTextGetSize(
		wText_p b )
{
	int lc, l, li, len=0;
	lc = (int)SendMessage( b->hWnd, EM_GETLINECOUNT, 0, 0L );
	for ( l=0; l<lc ; l++ ) {
		li = (int)SendMessage( b->hWnd, EM_LINEINDEX, l, 0L );
		len += (int)SendMessage( b->hWnd, EM_LINELENGTH, l, 0L ) + 1;
	}
	if ( len == 1 )
		len = 0;
	return len;
}

void wTextGetText(
		wText_p b,
		char * t,
		int s )
{
	int lc, l, len;
	s--;
	lc = (int)SendMessage(b->hWnd, EM_GETLINECOUNT, 0, 0L);

	for (l=0; l<lc && s>=0; l++)
	{
	    *(WORD*)t = s;
	    len = (int)SendMessage(b->hWnd, EM_GETLINE, l, (DWORD)(LPSTR)t);
	    t += len;
	    *t++ = '\n';
	    s -= len+1;
	}

	*(t - 1) = '\0';		// overwrite the last \n added
}


void wTextSetReadonly(
		wText_p b,
		wBool_t ro )
{
	if (ro)
		b->option |= BO_READONLY;
	else
		b->option &= ~BO_READONLY;
	SendMessage( b->hWnd, EM_SETREADONLY, ro, 0L );
}


void wTextSetSize(
		wText_p bt,
		wPos_t width,
		wPos_t height )
{
	bt->w = width;
	bt->h = height;
	if (!SetWindowPos( bt->hWnd, HWND_TOP, 0, 0,
		bt->w, bt->h, SWP_NOMOVE|SWP_NOZORDER)) {
		mswFail("wTextSetSize: SetWindowPos");
	}
}


void wTextComputeSize(
		wText_p bt,
		int rows,
		int lines,
		wPos_t * w,
		wPos_t * h )
{
	static wPos_t scrollV_w = -1;
	static wPos_t scrollH_h = -1;
	HDC hDc;
	TEXTMETRIC metrics;

	if (scrollV_w < 0)
		scrollV_w = GetSystemMetrics( SM_CXVSCROLL );
	if (scrollH_h < 0)
		scrollH_h = GetSystemMetrics( SM_CYHSCROLL );
	hDc = GetDC( bt->hWnd );
	GetTextMetrics( hDc, &metrics );
	*w = rows * metrics.tmAveCharWidth + scrollV_w;
	*h = lines * (metrics.tmHeight + metrics.tmExternalLeading);
	ReleaseDC( bt->hWnd, hDc );
	if (bt->option&BT_HSCROLL)
		 *h += scrollH_h;
}


void wTextSetPosition(
		wText_p bt,
		int pos )
{
	long rc;
	rc = SendMessage( bt->hWnd, EM_LINESCROLL, 0, MAKELONG( -65535, 0 ) );
}

static void textDoneProc( wControl_p b )
{
	wText_p t = (wText_p)b;
		HDC hDc;
		hDc = GetDC( t->hWnd );
	SelectObject( hDc, mswOldTextFont );
		ReleaseDC( t->hWnd, hDc );
}

static callBacks_t textCallBacks = {
		mswRepaintLabel,
		textDoneProc,
		NULL };

wText_p wTextCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		POS_T	width,
		POS_T	height )
{
	wText_p b;
	DWORD style;
	RECT rect;
	int index;

	b = mswAlloc( parent, B_TEXT, labelStr, sizeof *b, NULL, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option = option;
	style = ES_MULTILINE | ES_LEFT | ES_AUTOVSCROLL | ES_WANTRETURN |
			WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL;
#ifdef BT_HSCROLL
	if (option & BT_HSCROLL)
		style |= WS_HSCROLL | ES_AUTOHSCROLL;
#endif
/*	  if (option & BO_READONLY)
		style |= ES_READONLY;*/

	b->hWnd = CreateWindow( "EDIT", NULL,
						style, b->x, b->y,
						width, height,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
	if (b->hWnd == NULL) {
		mswFail("CreateWindow(TEXT)");
		return b;
	}
#ifdef CONTROL3D
	Ctl3dSubclassCtl( b->hWnd );
#endif

	if (option & BT_FIXEDFONT) {
		if (fixedTextFont == (HFONT)0)
			fixedTextFont =	 CreateFontIndirect( &fixedFont );
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)fixedTextFont, (LPARAM)MAKELONG( 1, 0 ) );
	} else 	if ( !mswThickFont ) {
		SendMessage( b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
	}

	b->hText = (HANDLE)SendMessage( b->hWnd, EM_GETHANDLE, 0, 0L );

	if (option & BT_CHARUNITS) {
		wPos_t w, h;
		wTextComputeSize( b, width, height, &w, &h );
		if (!SetWindowPos( b->hWnd, HWND_TOP, 0, 0,
			w, h, SWP_NOMOVE|SWP_NOZORDER)) {
			mswFail("wTextCreate: SetWindowPos");
		}
	}

	GetWindowRect( b->hWnd, &rect );
	b->w = rect.right - rect.left;
	b->h = rect.bottom - rect.top;

	mswAddButton( (wControl_p)b, FALSE, helpStr );
	mswCallBacks[B_TEXT] = &textCallBacks;
	return b;
}
