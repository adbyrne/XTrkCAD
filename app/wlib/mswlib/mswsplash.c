/**
 *	Splash window for Windows
 *  $header$
 */

 /*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2007 Martin Fischer
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

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "mswint.h"

extern HINSTANCE mswHInst;
extern HWND mswHWnd;

static HWND hSplash;

#define IDAPPNAME   150
#define IDMESSAGE   200
#define IDBITMAP	250

static LPWORD lpwAlign( LPWORD lpIn )
{
    ULONG ul;

    ul = (ULONG) lpIn;
    ul +=3;
    ul >>=2;
    ul <<=2;
    return (LPWORD) ul;
}

/**
 * Draw the logo bitmap. Thanks to Charles Petzold.
 */

BOOL
PaintBitmap( HWND hWnd, HBITMAP hBmp )
{
	HDC hdc, hdcMem;	
	RECT rect;

	UpdateWindow( hWnd );

	/* get device context for destination window ( the dialog control ) */
	hdc = GetDC( hWnd );
	GetClientRect( hWnd, &rect );
	
	/* create a memory dc holding the bitmap */
	hdcMem = CreateCompatibleDC( hdc );
	SelectObject( hdcMem, hBmp );

	/* 
	   show it in the uppler left corner 
	   the window is created with the size of the bitmap, so there is no need 
	   for any special transformation 
	*/

	BitBlt( hdc, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top,
			hdcMem, 0, 0, SRCCOPY );

	/* release the DCs that are not needed any more */
	DeleteDC( hdcMem );
	ReleaseDC( hWnd, hdc );

	return( 0 );
}

/**
 * This is the dialog procedure for the splash window. Main activity is to 
 * catch the WM_PAINT message and draw the logo bitmap into that area.
 */

BOOL CALLBACK 
SplashDlgProc( HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam )
{
	static HWND hWndBmp;
	static HBITMAP hBmp;

	switch( msg ) {
		case WM_INITDIALOG:
			/* bitmap handle is passed at dialog creation */
			hBmp = (HBITMAP)lParam;

			hWndBmp = GetDlgItem( hDlg, IDBITMAP );
			return TRUE;
		case WM_PAINT:
			/* paint the logo bitmap */
			PaintBitmap( hWndBmp, hBmp );
			break;
		case WM_DESTROY:
			/* destroy the bitmap */
			DeleteObject( hBmp );
			break;
	}
	return FALSE;
}

/**
 * Show the splash screen. For display of the splash screen, a dialog template is
 * created in memory. This template has three static elements:
 * - the logo
 * - the application name
 * - the progress message
 *
 * return TRUE if successful, FALSE otherwise.
 *
 */

int
wCreateSplash( char *appname, char *appver )
{
	HGLOBAL hgbl;
    LPDLGTEMPLATE lpdt;
	LPWORD lpw;
	LPDLGITEMTEMPLATE lpdit;
	int cxDlgUnit, cyDlgUnit;
	int cx, cy;
	char *pszBuf;
	HBITMAP hBmp;
	BITMAP bmp;
	char logoPath[MAX_PATH];

	/* find the size of a dialog unit */
	cxDlgUnit = LOWORD(GetDialogBaseUnits());
	cyDlgUnit = HIWORD(GetDialogBaseUnits());

	/* load the logo bitmap */
	sprintf( logoPath, "%s\\logo.bmp", wGetAppLibDir());
	hBmp = LoadImage( mswHInst, logoPath, IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR | LR_LOADFROMFILE );
	if( !hBmp )
		return( 0 );

	/* get info about the loaded logo file */	
	GetObject( hBmp, sizeof(BITMAP), (LPVOID)&bmp );

	/* calculate the size of dialog box */
	cx = (bmp.bmWidth * 4) / cxDlgUnit;			/* start with the size of the bitmap */
	cy = (bmp.bmHeight * 8) / cyDlgUnit + 20;	/* 20 is enough for two lines of text and some room */

	/* allocate memory block for dialog template */
	hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
    if (!hgbl)
		return -1;
    lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

    /* Define a dialog box. */
    lpdt->style = WS_POPUP | WS_BORDER | WS_VISIBLE | DS_MODALFRAME | DS_CENTER;
    lpdt->cdit = 3;  // number of controls
    lpdt->x  = 0;  lpdt->y  = 0;
    lpdt->cx = cx; lpdt->cy = cy;

    lpw = (LPWORD) (lpdt + 1);
    *lpw++ = 0;   /* no menu */
    *lpw++ = 0;   /* predefined dialog box class (by default) */
	*lpw++ = 0; 

	/* add the static control for the logo bitmap */
	lpdit = (LPDLGITEMTEMPLATE)lpwAlign(lpw);
	lpdit->x  = 0; lpdit->y  = 0;
    lpdit->cx = (SHORT)((bmp.bmWidth * 4) / cxDlgUnit); 
	lpdit->cy = (SHORT)((bmp.bmHeight * 8) / cyDlgUnit);

    lpdit->id = IDBITMAP;  
    lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0082;		/* static class */

	lpw  += 1+MultiByteToWideChar (CP_ACP, 0, "Logo should be here...", -1, (LPWSTR)lpw, 50);

	/* add the static control for the program title */
	lpdit = (LPDLGITEMTEMPLATE)lpwAlign(lpw);

	lpdit->x  = 2; lpdit->y  = (short)( 1 + (bmp.bmHeight * 8) / cyDlgUnit );
    lpdit->cx = cx - 2; lpdit->cy = cyDlgUnit;
    lpdit->id = IDAPPNAME;  
    lpdit->style = WS_CHILD | WS_VISIBLE | SS_CENTER;
    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0082;		/* static class */

	/* create the title string */	
	pszBuf = malloc( strlen( appname ) + strlen( appver ) + 2 );
	if( !pszBuf )
		return( 0 );
	sprintf( pszBuf, "%s %s", appname, appver );

	lpw  += 1+MultiByteToWideChar (CP_ACP, 0, pszBuf, -1, (LPWSTR)lpw, 50);

	/* add the static control for the loading message */
	lpdit = (LPDLGITEMTEMPLATE)lpwAlign(lpw);
	lpdit->x  = 2; lpdit->y  = (short)(bmp.bmHeight * 8) / cyDlgUnit  + 10;
    lpdit->cx = cx - 2; lpdit->cy = cyDlgUnit;
    lpdit->id = IDMESSAGE;  
    lpdit->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = 0x0082;		/* static class */

	lpw  += 1+MultiByteToWideChar (CP_ACP, 0, "Starting Application...", -1, (LPWSTR)lpw, 50);

	/* create the dialog */
    GlobalUnlock(hgbl); 
    hSplash = CreateDialogIndirectParam( mswHInst, (LPDLGTEMPLATE) hgbl, 
        mswHWnd, (DLGPROC)SplashDlgProc, (LPARAM)hBmp ); 
	GetLastError();
	
	/* free allocated memory */
	GlobalFree(hgbl);     
	free( pszBuf );

	/* that's it */
	return 1; 
}


/**
 * Update the progress message inside the splash window
 * msg	text message to display
 * return nonzero if ok
 */ 

int 
wSetSplashInfo( char *msg )
{
	if( msg ) {
		SetWindowText( GetDlgItem( hSplash, IDMESSAGE ), msg );
		wFlush();
		return TRUE;
	}
	wFlush();
	return FALSE;
}

/**
 * Remove the splash window from the screen.
 */

void
wDestroySplash(void)
{
	DestroyWindow( hSplash );
	return;
}