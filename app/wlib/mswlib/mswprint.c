#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#ifndef WIN32
#include <print.h>
#endif
#include "mswint.h"

/*
 *****************************************************************************
 *
 * PRINT
 *
 *****************************************************************************
 */


struct wDraw_t print_d;

#ifdef WIN32
struct tagPDA printDlg;
#else
struct tagPD printDlg;
#endif
static int printStatus = FALSE;
static DOCINFO docInfo;
static double pageSizeW = 8.5, pageSizeH = 11.0;
static double physSizeW = 8.5, physSizeH = 11.0;
static int pageCount = -1;

static HPALETTE newPrintPalette;
static HPALETTE oldPrintPalette;


void wPrintClip( wPos_t x, wPos_t y, wPos_t w, wPos_t h )
{
	wDrawClip( &print_d, x, y, w, h );
}


void getPageDim( HDC hDc )
{
	int rc;
	POINT dims;
	POINT offs;
	int res_w, res_h, size_w, size_h;
	rc = Escape( hDc, GETPHYSPAGESIZE, 0, NULL, (LPPOINT)&dims );
	if (rc <0) {
	   mswFail( "GETPHYPAGESIZE" );
	}
	rc = Escape( hDc, GETPRINTINGOFFSET, 0, NULL, (LPPOINT)&offs );
	if (rc <0) {
	   mswFail( "GETPRINTINGOFFSET" );
	}
	print_d.wFactor = (double)GetDeviceCaps( hDc, LOGPIXELSX );
	print_d.hFactor = (double)GetDeviceCaps( hDc, LOGPIXELSY );
	if (print_d.wFactor <= 0 || print_d.hFactor <= 0) {
		mswFail( "getPageDim: LOGPIXELS... <= 0" );
		abort();
	}
	print_d.DPI = min( print_d.wFactor, print_d.hFactor );
	size_w = GetDeviceCaps( hDc, HORZSIZE );
	size_h = GetDeviceCaps( hDc, VERTSIZE );
	print_d.w = res_w = GetDeviceCaps( hDc, HORZRES );
	print_d.h = res_h = GetDeviceCaps( hDc, VERTRES );
	pageSizeW = ((double)res_w)/print_d.wFactor;
	pageSizeH = ((double)res_h)/print_d.hFactor;
	physSizeW = ((double)dims.x)/print_d.wFactor;
	physSizeH = ((double)dims.y)/print_d.hFactor;
}

static wBool_t printInit( void )
{
	static int initted = FALSE;
	static int printerOk = FALSE;
	if (initted) {
		if (!printerOk) {
			mswFail( "No Printers are defined" );
		}
		return printerOk;
	}
	initted = TRUE;
	printDlg.lStructSize = sizeof printDlg;
	printDlg.hwndOwner = NULL;
	printDlg.Flags = PD_RETURNDC|PD_RETURNDEFAULT;
	if (PrintDlg(&printDlg) != 0  && printDlg.hDC) {
		getPageDim( printDlg.hDC );
		DeleteDC( printDlg.hDC );
	}
#ifdef LATER
	DEVMODE * printMode;
	HDC hDc;
	char ptrInfo[80];
	char ptrDrvrDvr[80];
	char *temp;
	char *ptrDevice;
	char *ptrDrvr;
	char *ptrPort;
	int size;
	int rc;
	FARPROC extDeviceMode;
	FARPROC deviceMode;
	HINSTANCE hDriver;

	GetProfileString("windows", "device", "", ptrInfo, sizeof ptrInfo );
	if (ptrInfo[0] == 0) {
		mswFail( "No Printers are defined" );
		return FALSE;
	}
	temp = ptrDevice = ptrInfo;
	ptrDrvr = ptrPort = NULL;
	while (*temp) {
		if (*temp == ',') {
			*temp++ = 0;
			while( *temp == ' ' )
				temp++;
			if (!ptrDrvr)
				ptrDrvr = temp;
			else {
				ptrPort = temp;
				break;
			}
		}
		else
			temp = AnsiNext(temp);
	}
	strcpy( ptrDrvrDvr, ptrDrvr );
	strcat( ptrDrvrDvr, ".drv" );
	if ((long)(hDriver = LoadLibrary( ptrDrvrDvr )) <= 32) {
		mswFail( "printInit: LoadLibrary" );
		return FALSE;
	}
	if (( extDeviceMode = GetProcAddress( hDriver, "ExtDeviceMode" )) != NULL) {
		size = extDeviceMode( mswHWnd, (HANDLE)hDriver, (LPDEVMODE)NULL, (LPSTR)ptrDevice, (LPSTR)ptrPort, (LPDEVMODE)NULL, (LPSTR)NULL, 0 );
		printMode = (DEVMODE*)malloc( size );
		rc = extDeviceMode( mswHWnd, (HANDLE)hDriver, (LPDEVMODE)printMode, (LPSTR)ptrDevice, (LPSTR)ptrPort, (LPDEVMODE)NULL, (LPSTR)NULL, DM_OUT_BUFFER );
#ifdef LATER
		if (rc != IDOK && rc != IDCANCEL) {
			mswFail( "printInit: extDeviceMode" );
			return FALSE;
		}
#endif
	} else if (( deviceMode = GetProcAddress( hDriver, "DeviceMode" )) != NULL) {
		rc = deviceMode( mswHWnd, (HANDLE)hDriver, (LPSTR)ptrDevice, (LPSTR)ptrPort );
#ifdef LATER
		if (rc != IDOK && rc != IDCANCEL) {
			mswFail( "printInit: deviceMode" );
			return FALSE;
		}
#endif
	}

	hDc = CreateDC( (LPSTR)ptrDrvr, (LPSTR)ptrDevice, (LPSTR)ptrPort, NULL );
	if (hDc == NULL) {
		mswFail("printInit: createDC" );
		abort();
	}
	getPageDim( hDc );
	DeleteDC( hDc );
	
	FreeLibrary( hDriver );
#endif
	printerOk = TRUE;
	return TRUE;
}


wBool_t wPrintInit( void )
{
	if (!printInit()) {
		return FALSE;
	}
	return TRUE;
}


void wPrintSetup( wPrintSetupCallBack_p callback )
{
	if (!printInit()) {
		return;
	}
	/*memset( &printDlg, 0, sizeof printDlg );*/
	printDlg.lStructSize = sizeof printDlg;
	printDlg.hwndOwner = NULL;
	printDlg.Flags = PD_RETURNDC|PD_PRINTSETUP;
	if (PrintDlg(&printDlg) != 0  && printDlg.hDC) {
		getPageDim( printDlg.hDC );
	}
	if ( callback ) {
		callback( TRUE );
	}
}


void wPrintGetPageSize( double *w, double *h )
{
	printInit();
	*w = pageSizeW;
	*h = pageSizeH;
}


void wPrintGetPhysSize( double *w, double *h )
{
	printInit();
	*w = physSizeW;
	*h = physSizeH;
}


HDC mswGetPrinterDC( void )
{
	if (!printInit()) {
		return (HDC)0;
	}
	/*memset( &printDlg, 0, sizeof printDlg );*/
	printDlg.lStructSize = sizeof printDlg;
	printDlg.hwndOwner = NULL;
	printDlg.Flags = PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION;
	if (PrintDlg(&printDlg) != 0)
		return printDlg.hDC;
	else
		return (HDC)0;
}


static wBool_t printAbort = FALSE;
HWND hAbortDlgWnd;
FARPROC lpAbortDlg, lpAbortProc;
static int pageNumber;

int FAR PASCAL mswAbortDlg( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	if (msg == WM_COMMAND) {
		if (WCMD_PARAM_ID == IDCANCEL) {
			printAbort = TRUE;
			EndDialog( hWnd, wParam );
			return TRUE;
		}
	} else if (msg == WM_INITDIALOG) {
		SetFocus( GetDlgItem( hWnd, IDCANCEL ) );
		return TRUE;
	}
	return FALSE;
}


int FAR PASCAL _export mswAbortProc( HDC hdcPrinter, int Code )
{
	MSG msg;
	while (PeekMessage((LPMSG)&msg, (HWND)0, 0, 0, PM_REMOVE)) {
		if (!IsDialogMessage(hAbortDlgWnd, (LPMSG)&msg) ) {
			TranslateMessage( (LPMSG)&msg );
			DispatchMessage( (LPMSG)&msg );
		}
	}
	return !printAbort;
}


wBool_t wPrintDocStart( const char * title, int fpageCount, int * copiesP )
{
	printStatus = FALSE;
	pageCount = fpageCount;
	pageNumber = 0;
	print_d.hDc = mswGetPrinterDC();
	if (print_d.hDc == (HDC)0) {
		return FALSE;
	}
		printStatus = TRUE;
		docInfo.cbSize = sizeof docInfo;
		docInfo.lpszDocName = title;
		docInfo.lpszOutput = NULL;
		lpAbortDlg = MakeProcInstance( (FARPROC)mswAbortDlg, mswHInst );
		lpAbortProc = MakeProcInstance( (FARPROC)mswAbortProc, mswHInst );
		SetAbortProc( print_d.hDc, (ABORTPROC)lpAbortProc );
		if (StartDoc( print_d.hDc, &docInfo ) < 0) {
			MessageBox( mswHWnd, "Unable to start print job",
						NULL, MB_OK|MB_ICONHAND );
			FreeProcInstance( lpAbortDlg );
			FreeProcInstance( lpAbortProc );
			DeleteDC( print_d.hDc );
			return FALSE;
		}
		printAbort = FALSE;
		hAbortDlgWnd = CreateDialog( mswHInst, "MswAbortDlg", mswHWnd,
						(DLGPROC)lpAbortDlg );
		/*SetDlgItemText( hAbortDlgWnd, IDM_PRINTAPP, title );*/
		SetWindowText( hAbortDlgWnd, title );
		ShowWindow( hAbortDlgWnd, SW_NORMAL );
		UpdateWindow( hAbortDlgWnd );
		EnableWindow( mswHWnd, FALSE );
		if (copiesP)
			*copiesP = printDlg.nCopies;
		if (printDlg.nCopies>1)
			pageCount *= printDlg.nCopies;
		if ( (GetDeviceCaps( printDlg.hDC, RASTERCAPS ) & RC_PALETTE) ) {
			newPrintPalette = mswCreatePalette();
			oldPrintPalette = SelectPalette( printDlg.hDC, newPrintPalette, 0 );
			RealizePalette( printDlg.hDC );
		}
		return TRUE;
}

wDraw_p wPrintPageStart( void )
{
	char pageL[80];
	if (!printStatus)
		return NULL;
	pageNumber++;
	if (pageCount > 0)
		wsprintf( pageL, "Page %d of %d", pageNumber, pageCount );
	else
		wsprintf( pageL, "Page %d", pageNumber );
	SetDlgItemText( hAbortDlgWnd, IDM_PRINTPAGE, pageL );
	StartPage( printDlg.hDC );
#ifdef LATER
	if (mswPrintPalette) {
		SelectPalette( printDlg.hDC, mswPrintPalette, 0 );
		RealizePalette( printDlg.hDC );
	}
#endif
	getPageDim( printDlg.hDC );
	SelectClipRgn( print_d.hDc, NULL );
	return &print_d;
}

wBool_t wPrintPageEnd( wDraw_p d )
{
	return EndPage( printDlg.hDC ) >= 0;
}

wBool_t wPrintQuit( void )
{
	MSG msg;
	while (PeekMessage((LPMSG)&msg, (HWND)0, 0, 0, PM_REMOVE)) {
		if (!IsDialogMessage(hAbortDlgWnd, (LPMSG)&msg) ) {
			TranslateMessage( (LPMSG)&msg );
			DispatchMessage( (LPMSG)&msg );
		}
	}
	return printAbort;
}

void wPrintDocEnd( void )
{
	if (!printStatus)
		return;
	EndDoc( printDlg.hDC );
	if ( newPrintPalette ) {
		SelectPalette( printDlg.hDC, oldPrintPalette, 0 );
		DeleteObject( newPrintPalette );
		newPrintPalette = (HPALETTE)0;
	}
		
	EnableWindow( mswHWnd, TRUE );
	DestroyWindow( hAbortDlgWnd );
	FreeProcInstance( lpAbortDlg );
	FreeProcInstance( lpAbortProc );
	DeleteDC( printDlg.hDC );
	printStatus = FALSE;
}

wBool_t wPrintFontAlias( const char * font, const char * alias )
{
	return TRUE;
}

wBool_t wPrintNewPrinter( const char * printer )
{
	return TRUE;
}

wBool_t wPrintNewMargin( const char * name, double t, double b, double l, double r )
{
	return TRUE;
}

void wPrintSetCallBacks(
		wAddPrinterCallBack_p newPrinter,
		wAddMarginCallBack_p newMargin,
		wAddFontAliasCallBack_p newFontAlias )
{
}
