 /*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/mswlib/mswmisc.c,v 1.1 2005-12-07 15:48:57 rc-flyer Exp $
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include "mswint.h"

int __cdecl _heapwalk(_HEAPINFO *);

#define OFN_LONGFILENAMES		0x00200000L

char * mswStrdup( const char * );

#define PAUSE_TIMER		(901)
#define ALARM_TIMER		(902)
#define BALLOONHELP_TIMER		(903)
#define TRIGGER_TIMER	(904)

#define WANT_LITTLE_LABEL_FONT

#ifndef WANT_LITTLE_LABEL_FONT
#define LABELFONTDECL
#define LABELFONTSELECT
#define LABELFONTRESET
#else
#define LABELFONTDECL	HFONT hFont;
#define LABELFONTRESET	if (!mswThickFont) {SelectObject( hDc, hFont );}
#define LABELFONTSELECT if (!mswThickFont) {hFont = SelectObject( hDc, mswLabelFont );}
#endif

/*
 * EXPORTED VARIABLES
 */

long debugWindow = 0;
HINSTANCE mswHInst;
HWND mswHWnd = (HWND)0;
#ifdef DOTRANSACCEL
static HACCEL hMswAccel;
#endif
const char *mswDrawWindowClassName = "DRAWWINDOW";
char mswTmpBuff[1024];
int mswEditHeight;
int mswAllowBalloonHelp = TRUE;
int mswGroupStyle;
HFONT mswOldTextFont;
HFONT mswLabelFont;
long mswThickFont = 1;
double mswScale = 1.0;

callBacks_t *mswCallBacks[CALLBACK_CNT];

void closeBalloonHelp( void );
static wControl_p getControlFromCursor( HWND, wWin_p * );
/*
 * LOCAL VARIABLES
 */

struct wWin_t {
		WOBJ_COMMON
		wPos_t lastX, lastY;
		wPos_t padX, padY;
		wControl_p first, last;
		wWinCallBack_p winProc;
		BOOL_T busy;
#ifdef OWNERICON
		HBITMAP wicon_bm;
		wPos_t wicon_w, wicon_h;
#endif
		DWORD baseStyle;
		wControl_p focusChainFirst;
		wControl_p focusChainLast;
		char * nameStr;
		wBool_t centerWin;
		DWORD style;
		int isBusy;
		int pendingShow;
		int modalLevel;
		};

static needToDoPendingShow = FALSE;

/* System metrics: */
static int mTitleH;
static int mFixBorderW;
static int mFixBorderH;
static int mResizeBorderW;
static int mResizeBorderH;
static int mMenuH;
static int screenWidth = 0, screenHeight = 0;

wWin_p mswWin = NULL;
wWin_p winFirst, winLast;

static long count51 = 0;

static UINT alarmTimer;
static UINT pauseTimer;
static UINT balloonHelpTimer = (UINT)0;
static UINT triggerTimer;

static UINT balloonHelpTimeOut = 500;
static wControl_p balloonHelpButton = NULL;
static enum { balloonHelpIdle , balloonHelpWait, balloonHelpShow } balloonHelpState = balloonHelpIdle;
static HWND balloonHelpHWnd = (HWND)0;
static int balloonHelpFontSize = 8;
static char balloonHelpFaceName[] = "MS Sans Serif";
static HFONT balloonHelpOldFont;
static HFONT balloonHelpNewFont;
static int balloonHelpEnable = TRUE;			 
static wControl_p balloonControlButton = NULL;

static BOOL_T helpInitted = FALSE;

#define CONTROL_BASE (1)
typedef struct {
		wControl_p b;
		} controlMap_t;
dynArr_t controlMap_da;
#define controlMap(N) DYNARR_N(controlMap_t,controlMap_da,N)


static char * appName;
static char * helpFile;
char *mswProfileFile;

static wBalloonHelp_t * balloonHelpStrings;

static wCursor_t curCursor = wCursorNormal;

#ifdef HELPSTR
static FILE * helpStrF;
#endif
static int inMainWndProc = FALSE;

HPALETTE hOldPal;

int newHelp = 1;

static wBool_t mswWinBlockEnabled = TRUE;

static FILE * dumpControlsF;
static int dumpControls;

/*
 *****************************************************************************
 *
 * Internal Utility functions
 *
 *****************************************************************************
 */

#ifdef WIN32
DWORD GetTextExtent(
		HDC hDc,
		CHAR * str,
		UINT len )
{
		SIZE size;
		GetTextExtentPoint( hDc, str, len, &size );
		return size.cx + (size.cy<<16);
}
#endif


static char * controlNames[] = {
		"MAIN", "POPUP",
		"BUTTON", "STRING", "INTEGER", "FLOAT",
		"LIST", "DROPLIST", "COMBOLIST",
		"RADIO", "TOGGLE",
		"DRAW", "TEXT", "MESSAGE", "LINES",
		"MENUITEM", "CHOICEITEM", "BOX" };

static void doDumpControls(void)
{
	wControl_p b;
	int inx;
	if ( !dumpControls )
		return;
	if ( !dumpControlsF ) {
		dumpControlsF = fopen( "controls.lst", "w" );
		if ( !dumpControlsF )
			abort();
	}
	for ( inx=0; inx<controlMap_da.cnt-1; inx++ ) {
		b = controlMap(inx).b;
		if ( b ) {
			fprintf( dumpControlsF, "[%0.3d] [%x] %s %s %s\n", inx,
				b->hWnd,
				(b->type>=0&&b->type<=B_BOX?controlNames[b->type]:"NOTYPE"),
				(b->labelStr?b->labelStr:"<NULL>"),
				(b->helpStr?b->helpStr:"<NULL>") );
		} else {
			fprintf( dumpControlsF, "[%0.3d] <NULL>\n", inx );
		}
	}
	fflush( dumpControlsF );
	fclose( dumpControlsF );
	dumpControls = 0;
}

void mswFail( const char * where )
{
	sprintf( mswTmpBuff, "%s\n# Controls %d", where, controlMap_da.cnt );
	MessageBox( NULL, mswTmpBuff, "FAIL", MB_TASKMODAL|MB_OK );
	doDumpControls();
}

static UINT curSysRes = 100;
static UINT curGdiRes = 100;
static UINT curUsrRes = 100;
static UINT curMinRes = 100;

void mswCheckResources( void )
{
#ifndef WIN32
/* not needed on WIN32 */
	UINT sysRes, gdiRes, usrRes, minRes, style;
	char msg[80];
	char * cp;
	sysRes = GetFreeSystemResources( GFSR_SYSTEMRESOURCES );
	gdiRes = GetFreeSystemResources( GFSR_GDIRESOURCES );
	usrRes = GetFreeSystemResources( GFSR_USERRESOURCES );
	minRes = sysRes;
	if (gdiRes < minRes)
		minRes = gdiRes;
	if (usrRes < minRes)
		minRes = usrRes;
	if (minRes < curMinRes) {
		style = 0;
		if ( (minRes <= 5 && curMinRes > 5) ) {
			style = MB_ICONHAND;
			cp = "Danger";
		} else if ( (minRes <= 10 && curMinRes > 10) ) {
			style = MB_ICONEXCLAMATION;
			cp = "Warning";
		} else if ( (minRes <= 20 && curMinRes > 20) ) {
			style = MB_ICONINFORMATION;
			cp = "Note";
		}
		if (style) {
			 sprintf( msg, "%s - low resources\n%2d System\n%2d Gdi\n%2d User",
						cp, sysRes, gdiRes, usrRes );
			 MessageBox( NULL, msg, appName, MB_TASKMODAL|MB_OK|style );
		}
		curMinRes = minRes;
	}
	if (sysRes < curSysRes)
		curSysRes = sysRes;
	if (gdiRes < curGdiRes)
		curGdiRes = gdiRes;
	if (usrRes < curUsrRes)
		curUsrRes = usrRes;
#endif
}


wControl_p mswMapIndex( INDEX_T inx )
{
	if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
		mswFail("mswMapIndex- bad index");
		exit(1);
	}
	return controlMap(inx-CONTROL_BASE).b;
}


void mswRepaintLabel( HWND hWnd, wControl_p b )
{
	HDC hDc;
	HBRUSH oldBrush, newBrush;
	RECT rect;
	DWORD dw;
	LABELFONTDECL

				
	if (b->labelStr) {
		hDc = GetDC( hWnd );
		LABELFONTSELECT
		newBrush = CreateSolidBrush( GetSysColor( COLOR_BTNFACE ) );
		oldBrush = SelectObject( hDc, newBrush );
	dw = GetTextExtent( hDc, CAST_AWAY_CONST b->labelStr, strlen(b->labelStr) );
		rect.left = b->labelX;
		rect.top = b->labelY;
		rect.right = b->labelX + LOWORD(dw);
		rect.bottom = b->labelY + HIWORD(dw);
		FillRect( hDc, &rect, newBrush );
		DeleteObject( SelectObject( hDc, oldBrush ) );
		/*SetBkMode( hDc, OPAQUE );*/
		SetBkColor( hDc, GetSysColor( COLOR_BTNFACE ) );
		if (!TextOut( hDc, b->labelX, b->labelY, b->labelStr, strlen(b->labelStr) ) )
			mswFail( "Repainting text label" );
		LABELFONTRESET
		ReleaseDC( hWnd, hDc );
	}
}


void mswDrawIcon(
		HDC hDc,
		int offw,
		int offh,
		wIcon_p bm,
		int disabled,
		COLORREF color1,
		COLORREF color2 )
{
	int i, j;
	int byt;
	int w=bm->w, h=bm->h;
	const char *byts_p;
	unsigned const char **row_p;
	COLORREF colorMap[256], color;

	if ( bm->type == mswIcon_bitmap ) {
		byts_p = bm->bits;
		for ( j = 0; j < h; j++ ) {
			byt = (0xFF & *byts_p++) | 0x100;
			for ( i = 0; i < w; i++ ) {
				if (byt == 1)
					byt = (0xFF & *byts_p++) | 0x100;
				if ( byt & 0x1 ) {
					SetPixel( hDc, i+offw, j+offh, color1 );
					if (disabled)
						SetPixel( hDc, i+1+offw, j+1+offh, color2 );
				}
				byt >>= 1;
			}
		}
	} else if ( bm->type == mswIcon_pixmap ) {
		SetROP2( hDc, R2_COPYPEN );
		memset( colorMap, 0, sizeof colorMap );
		for ( i=0; i<bm->colorcnt; i++ ) {
			color = wDrawGetRGB( bm->colormap[i].color );
			color = ((color>>16)&0x0000FF) | (color&0x00FF00) | ((color<<16)&0xFF0000);
			colorMap[bm->colormap[i].key] = color;
			/*colorMap[bm->colormap[i].key] = mswGetColor(bm->colormap[i].color);*/
		}
		row_p = bm->bits;
		for ( j = 0; j < h; j++ ) {
			for ( i = 0; i < w; i++ ) {
				if ( row_p[j][i] != bm->colormap[0].key ) {
					if ( disabled ) {
						SetPixel( hDc, i+offw, j+offh, color1 );
						SetPixel( hDc, i+1+offw, j+1+offh, color2 );
					} else {
						SetPixel( hDc, i+offw, j+offh, colorMap[row_p[j][i]] );
					}
				}
			}
		}
	} else {
		abort();
	}
}


int mswRegister(
		wControl_p w )
{
	int index;
	DYNARR_APPEND( controlMap_t, controlMap_da, 25 );
	index = controlMap_da.cnt-1+CONTROL_BASE;
	controlMap(controlMap_da.cnt-1).b = (wControl_p)w;
	return index;
}


void mswUnregister(
		int index )
{
	if (index < 0 || index > controlMap_da.cnt) {
		mswFail("mswMapIndex- bad index");
		exit(1);
	}
	controlMap(index-CONTROL_BASE).b = NULL;
}

void * mswAlloc(
		wWin_p parent,
		wType_e type,
		const char * labelStr,
		int size,
		void * data,
		int * index )
{
	wControl_p w = (wControl_p)calloc( 1, size );
	mswCheckResources();


	if (w == NULL)
		abort();
	*index = mswRegister( w );
	w->type = type;
	w->next = NULL;
	w->parent = parent;
	w->x = 0;
	w->y = 0;
	w->w = 0;
	w->h = 0;
	w->option = 0;
	w->labelX = w->labelY = 0;
	w->labelStr = labelStr;
	w->helpStr = NULL;
	w->hWnd = (HWND)0;
	w->data = data;
	w->focusChainNext = NULL;
	w->shown = TRUE;
	return w;
}


void mswComputePos(
		wControl_p b,
		wPos_t origX,
		wPos_t origY )
{
	wWin_p w = b->parent;

	if (origX >= 0) 
		b->x = origX;
	else
		b->x = w->lastX + (-origX) - 1;
	if (origY >= 0) 
		b->y = origY;
	else
		b->y = w->lastY + (-origY) - 1;

	b->labelX = b->x;
	b->labelY = b->y+2;

	if (b->labelStr) {
		int lab_l;
		HDC hDc;
		DWORD dw;
		LABELFONTDECL

		hDc = GetDC( w->hWnd );
		LABELFONTSELECT
		lab_l = strlen(b->labelStr);
		dw = GetTextExtent( hDc, CAST_AWAY_CONST b->labelStr, lab_l );
		b->labelX -= LOWORD(dw) + 5;
		LABELFONTRESET
		ReleaseDC( w->hWnd, hDc );
	}
}

void mswAddButton(
		wControl_p b,
		BOOL_T paintLabel,
		const char * helpStr )
{
	wWin_p w = b->parent;
	BOOL_T resize = FALSE;
	RECT rect;

	if (w->first == NULL) {
		w->first = b;
	} else {
		w->last->next = b;
	}
	w->last = b;
	b->next = NULL;
	b->parent = w;
	w->lastX = b->x + b->w;
	w->lastY = b->y + b->h;
	if ((w->option&F_AUTOSIZE)!=0 && w->lastX > w->w) {
		w->w = w->lastX;
		resize = TRUE;
	}
	if ((w->option&F_AUTOSIZE)!=0 && w->lastY > w->h) {
		w->h = w->lastY;
		resize = TRUE;
	}

	if (resize) {
		w->busy = TRUE;
		rect.left = 0;
		rect.top = 0;
		rect.right = w->w+w->padX;
		rect.bottom = w->h+w->padY;
		AdjustWindowRect( &rect, w->style, (w->option&F_MENUBAR)?1:0 );
		rect.bottom += mFixBorderH;
		if (!SetWindowPos( w->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
			rect.right-rect.left, rect.bottom-rect.top,
			SWP_NOMOVE))
			mswFail("SetWindowPos");
		w->busy = FALSE;
	}

	if (paintLabel)
		mswRepaintLabel( w->hWnd, (wControl_p)b );

	if (helpStr == NULL)
		return;
	b->helpStr = mswStrdup( helpStr );
	
	/*if ( b->hWnd )*/
/*		LABELFONTSELECT*/
#ifdef HELPSTR
	if (helpStrF)
		fprintf( helpStrF, "HELPSTR - %s\n", helpStr?helpStr:"<>" );
#endif
#ifdef LATER
	if (b->hWnd) {
		hDc = GetDC( b->hWnd );
		SelectPalette( hDc, mswPalette, 0 );
		rc = RealizePalette( hDc );
		ReleaseDC( b->hWnd, hDc );
	}
#endif

}


void mswResize(
		wWin_p w )
{
	wControl_p b;
	RECT rect;

	w->lastX = 0;
	w->lastY = 0;
	for (b=w->first; b; b=b->next) {
		if (w->lastX < (b->x + b->w))
			w->lastX = b->x + b->w;
		if (w->lastY < (b->y + b->h))
			w->lastY = b->y + b->h;
	}

	if (w->option&F_AUTOSIZE) {
		w->w = w->lastX;
		w->h = w->lastY;
		w->busy = TRUE;
		rect.left = 0;
		rect.top = 0;
		rect.right = w->w + w->padX;
		rect.bottom = w->h + w->padY;
		AdjustWindowRect( &rect, w->style, (w->option&F_MENUBAR)?1:0 );
		rect.bottom += mFixBorderH;
		if (!SetWindowPos( w->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
			rect.right-rect.left, rect.bottom-rect.top,
			SWP_NOMOVE|SWP_NOZORDER))
			mswFail("SetWindowPos");
		w->busy = FALSE;
	}
}


HBITMAP mswCreateBitMap(
		COLORREF fgCol1,
		COLORREF fgCol2,
		COLORREF bgCol,
		wPos_t w,
		wPos_t h,
		const char * bits )
{
	HDC hDc;
	HDC hButtDc;
	HBRUSH oldBrush, newBrush;
	RECT rect;
	HBITMAP hBitMap;
	HBITMAP hOldBitMap;
	const char * byts_p;
	int byt, i, j;

	hDc = GetDC( mswHWnd );
	hButtDc = CreateCompatibleDC( hDc );
	hBitMap = CreateCompatibleBitmap( hDc, w, h );
	ReleaseDC( mswHWnd, hDc );
	hOldBitMap = SelectObject( hButtDc, hBitMap );
	if (mswPalette) {
		hOldPal = SelectPalette( hButtDc, mswPalette, 0 );
	}
	
	/*PatBlt( hButtDc, 0, 0, w, h, WHITENESS );*/
	newBrush = CreateSolidBrush( bgCol );
	oldBrush = SelectObject( hButtDc, newBrush );
	rect.top = 0;
	rect.left = 0;
	rect.bottom = h;
	rect.right = w;
	FillRect( hButtDc, &rect, newBrush );
	DeleteObject( SelectObject( hButtDc, oldBrush ) );

	byts_p = bits;
	for ( j = 0; j < h; j++ ) {
		byt = (0xFF & *byts_p++) | 0x100;
		for ( i = 0; i < w; i++ ) {
			if (byt == 1)
				byt = (0xFF & *byts_p++) | 0x100;
			if ( byt & 0x1 ) {
				SetPixel( hButtDc, i, j, fgCol1 );
				SetPixel( hButtDc, i+1, j+1, fgCol2 );
			}
			byt >>= 1;
		}
	}

	SelectObject( hButtDc, hOldBitMap );
	DeleteDC( hButtDc );
	return hBitMap;
}

dynArr_t bitmap_da;
#define controlMap(N) DYNARR_N(controlMap_t,controlMap_da,N)
#define bitmap(N) DYNARR_N(HBITMAP,bitmap_da,N)

void mswRegisterBitMap(
		HBITMAP hBm )
{
	DYNARR_APPEND( HBITMAP, bitmap_da, 10 );
	bitmap(bitmap_da.cnt-1) = hBm;
}

static void deleteBitmaps( void )
{
	int inx;
	for ( inx=0; inx<bitmap_da.cnt; inx++ )
		DeleteObject( bitmap(inx) );
}


void mswChainFocus(
		wControl_p b )
{
	wWin_p w;
	w = b->parent;
	if (w->option&F_NOTAB)
		return;
	if (b->option&BO_NOTAB)
		return;
	if (w->focusChainFirst == NULL) {
		w->focusChainFirst = w->focusChainLast = w->focusChainNext = b;
		b->focusChainNext = b;
	} else {
		w->focusChainLast->focusChainNext = b;
		w->focusChainLast = b;
		b->focusChainNext = w->focusChainFirst;
	}
}

void mswSetFocus(
		wControl_p b )
{
	if (b && b->type != B_MENUITEM && b->focusChainNext) 
		b->parent->focusChainNext = b;
}

/*
 ******************************************************************************
 *
 * Main and Popup Windows
 *
 ******************************************************************************
 */

static void getSavedSizeAndPos(
		long option,
		const char * nameStr,
		wPos_t *rw,
		wPos_t *rh,
		wPos_t *rx,
		wPos_t *ry,
		int *showCmd )
{
	int x, y, w, h;
	const char *cp;
    char *cq;
	int state;

	*showCmd = SW_SHOWNORMAL;

	if ( (option&F_RECALLPOS) && nameStr ) {
			if ( (option & F_RESIZE) &&
				 (cp = wPrefGetString( "msw window size", nameStr)) &&
				 (state = (int)strtol( cp, &cq, 10 ), cp != cq) &&
				 (cp = cq, w = (wPos_t)strtod( cp, &cq ), cp != cq ) &&
				 (cp = cq, h = (int)strtod( cp, &cq ), cp != cq)
				) {
				if (state == 1)
					*showCmd = SW_SHOWMINIMIZED;
				else if (state == 2)
					*showCmd = SW_SHOWMAXIMIZED;
				if (w < 10)
					w = 10;
				if (h < 10)
					h = 10;
				if (w > screenWidth)
					w = screenWidth;
				if (h > screenHeight)
					h = screenHeight;
				*rw = w;
				*rh = h;
			}

			if ((cp = wPrefGetString( "msw window pos", nameStr)) &&
				(x = (wPos_t)strtod( cp, &cq ), cp != cq) &&
				(cp = cq, y = (wPos_t)strtod( cp, &cq ), cp != cq)
				) {
				if (y < 0)
					y = 0;
				if (x < 0)
					x = 0;
				if ( y > screenHeight-40 )
					y = screenHeight-40;
				if ( x > screenWidth-40 )
					x = screenWidth-40;
				*rx = x;
				*ry = y;
			}
	}
}


static wWin_p winCommonCreate(
		HWND hWnd,
		int typ,
		long option,
		const char * className,
		long style,
		const char * labelStr,
		wWinCallBack_p winProc,
		wPos_t w,
		wPos_t h,
		void * data,
		const char * nameStr,
		int * showCmd )
{
	wWin_p win;
	int index;
	wPos_t ww, hh, xx, yy;
	RECT rect;

	win = (wWin_p)mswAlloc( NULL, typ, mswStrdup(labelStr), sizeof *win, data, &index );
	win->option = option;
	win->first = win->last = NULL;
	win->lastX = 0;
	win->lastY = 0;
	win->winProc = winProc;
	win->centerWin = TRUE;
	win->modalLevel = 0;
#ifdef OWNERICON
	win->wicon_bm = (HBITMAP)0;
#endif
	win->busy = TRUE;
	ww = hh = xx = yy = CW_USEDEFAULT;
	getSavedSizeAndPos( option, nameStr, &ww, &hh, &xx, &yy, showCmd );
	if (xx != CW_USEDEFAULT)
		win->centerWin = FALSE;
	if (option & F_RESIZE) {
		style |= WS_THICKFRAME;
		if ( ww != CW_USEDEFAULT ) {
			w = ww;
			h = hh;
			option &= ~F_AUTOSIZE;
			win->option = option;
		}
	}

	if ( option & F_AUTOSIZE ) {
		win->padX = w;
		win->padY = h;
	} else {
		win->padX = 0;
		win->padY = 0;
		win->w = w;
		win->h = h;
	}
	win->style = style;
	rect.left = 0;
	rect.top = 0;
	rect.right = win->w + win->padX;
	rect.bottom = win->h + win->padY;
	AdjustWindowRect( &rect, win->style, (win->option&F_MENUBAR)?1:0 );
	rect.bottom += mFixBorderH;
	win->hWnd = CreateWindow( className, labelStr, style,
				xx, yy,
				rect.right-rect.left, rect.bottom-rect.top,
				hWnd, NULL,
				mswHInst, NULL );
	if (win->hWnd == (HWND)0) {
		mswFail( "CreateWindow(POPUP)" );
	} else {
		SetWindowWord( win->hWnd, 0, (WORD)index );
	}
	win->baseStyle = WS_GROUP;
	win->focusChainFirst = win->focusChainLast = win->focusChainNext = NULL;
	if (winFirst == NULL) {
		winFirst = winLast = win;
	} else {
		winLast->next = (wControl_p)win;
		winLast = win;
	}
#ifdef HELPSTR
	if (helpStrF)
		fprintf( helpStrF, "WINDOW - %s\n", labelStr );
#endif
	win->nameStr = mswStrdup( nameStr );
	if (typ == W_MAIN)
		mswInitColorPalette();
#ifdef LATER
	hDc = GetDC( win->hWnd );
	oldHPal = SelectPalette( hDc, mswPalette, 0 );
	ReleaseDC( win->hWnd, hDc );
#endif
	return win;
}

wWin_p wWinMainCreate(
		const char * name,
		POS_T x,
		POS_T y,
		const char * helpStr,
		const char * labelStr,
		const char * nameStr,
		long option,
		wWinCallBack_p winProc,
		void * data )
{
	wWin_p w;
	RECT rect;
	const char * appDir;
	const char * libDir;
	int showCmd;
	int error;
	HDC hDc;
	TEXTMETRIC tm;

	appName = (char*)malloc( strlen(name)+1 );
	strcpy( appName, name );
	appDir = wGetAppWorkDir();
	if ( appDir == NULL )
		return NULL;
	mswProfileFile = (char*)malloc( strlen(appDir)+1+strlen(appName)+1+3+1 );
	wsprintf( mswProfileFile, "%s\\%s.ini", appDir, appName );
	error = WritePrivateProfileString( "mswtest", "test", "ok", mswProfileFile );
	if ( error <= 0 ) {
		sprintf( mswTmpBuff, "Can not write to %s.\nPlease make sure the directory exists and is writable", mswProfileFile );
		wNotice( mswTmpBuff, "Ok", NULL );
		return NULL;
	}
	libDir = wGetAppLibDir();
	helpFile = (char*)malloc( strlen(libDir) + 1 + strlen(appName) + 1+3+1 );
	wsprintf( helpFile, "%s\\%s.hlp", libDir, appName );

	wPrefGetInteger( "msw tweak", "ThickFont", &mswThickFont, 0 );

	showCmd = SW_SHOW;
	w = winCommonCreate( NULL, W_MAIN, option|F_RESIZE, "MswMainWindow", 
						WS_OVERLAPPEDWINDOW, labelStr, winProc, x, y, data,
						nameStr, &showCmd );
	mswHWnd = w->hWnd;
	if ( !mswThickFont ) {
		DWORD dw;
		SendMessage( w->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L );
		hDc = GetDC( w->hWnd );
		GetTextMetrics( hDc, &tm );
		mswEditHeight = tm.tmHeight+2;
		dw = GetTextExtent( hDc, "AXqypj", 6 );
		mswEditHeight = HIWORD(dw)+2;
		ReleaseDC( w->hWnd, hDc );
	}
	ShowWindow( w->hWnd, showCmd );
	UpdateWindow( w->hWnd );
	GetWindowRect( w->hWnd, &rect );
	GetClientRect( w->hWnd, &rect );
	w->busy = FALSE;


	return w;
}

wWin_p wWinPopupCreate(
		wWin_p parent,
		POS_T x,
		POS_T y,
		const char * helpStr,
		const char * labelStr,
		const char * nameStr,
		long option,
		wWinCallBack_p winProc,
		void * data )
{
	wWin_p w;
	DWORD style;
	HMENU sysMenu;
	int showCmd;
	static DWORD overlapped = WS_OVERLAPPED;
	static DWORD popup = WS_POPUP;

	style = popup;
	style |= WS_BORDER | WS_CAPTION | WS_SYSMENU;
	w = winCommonCreate( parent?parent->hWnd:mswHWnd, W_POPUP, option,
						  "MswPopUpWindow",
						  style, labelStr, winProc, x, y, data, nameStr, &showCmd );
	sysMenu = GetSystemMenu( w->hWnd, FALSE );
	if (sysMenu) {
		DeleteMenu( sysMenu, SC_RESTORE, MF_BYCOMMAND );
		/*DeleteMenu( sysMenu, SC_MOVE, MF_BYCOMMAND );*/
		/*DeleteMenu( sysMenu, SC_SIZE, MF_BYCOMMAND );*/
		DeleteMenu( sysMenu, SC_MINIMIZE, MF_BYCOMMAND );
		DeleteMenu( sysMenu, SC_MAXIMIZE, MF_BYCOMMAND );
		DeleteMenu( sysMenu, SC_TASKLIST, MF_BYCOMMAND );
		DeleteMenu( sysMenu, 4, MF_BYPOSITION );
	}
	w->busy = FALSE;
	return w;
}

void wWinSetBigIcon(
		wWin_p win,
		wIcon_p bm )
{
#ifdef OWNERICON
	win->wicon_w = bm->w;
	win->wicon_h = bm->h;
	win->wicon_bm = mswCreateBitMap(
		GetSysColor(COLOR_BTNTEXT), RGB( 255, 255, 255 ), RGB( 255, 255, 255 ),
		bm->w, bm->h, bm->bits );
#endif
}


void wWinSetSmallIcon(
		wWin_p win,
		wIcon_p bm )
{
#ifdef OWNERICON
	win->wicon_w = bm->w;
	win->wicon_h = bm->h;
	win->wicon_bm = mswCreateBitMap(
		GetSysColor(COLOR_BTNTEXT), RGB( 255, 255, 255 ), RGB( 255, 255, 255 ),
		bm->w, bm->h, bm->bits );
#endif
}


void wWinTop(
		wWin_p win )
{
	/*BringWindowToTop( win->hWnd );*/
	SetWindowPos( win->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
	SetFocus( win->hWnd );
}


DWORD mswGetBaseStyle( wWin_p win )
{
	DWORD style;
	style = win->baseStyle;
	win->baseStyle = 0;
	return style;
}


static wAccelKey_e translateExtKey( UINT wParam )
{
	wAccelKey_e extChar;
		extChar = wAccelKey_None;
		switch( wParam ) {
		case VK_DELETE: extChar = wAccelKey_Del; break;
		case VK_INSERT: extChar = wAccelKey_Ins; break;
		case VK_HOME:	extChar = wAccelKey_Home; break;
		case VK_END:	extChar = wAccelKey_End; break;
		case VK_PRIOR:	extChar = wAccelKey_Pgup; break;
		case VK_NEXT:	extChar = wAccelKey_Pgdn; break;
		case VK_UP:		extChar = wAccelKey_Up; break;
		case VK_DOWN:	extChar = wAccelKey_Down; break;
		case VK_RIGHT:	extChar = wAccelKey_Right; break;
		case VK_LEFT:	extChar = wAccelKey_Left; break;
		case VK_BACK:	extChar = wAccelKey_Back; break;
		/*case VK_F1:	extChar = wAccelKey_F1; break;*/
		case VK_F2:		extChar = wAccelKey_F2; break;
		case VK_F3:		extChar = wAccelKey_F3; break;
		case VK_F4:		extChar = wAccelKey_F4; break;
		case VK_F5:		extChar = wAccelKey_F5; break;
		case VK_F6:		extChar = wAccelKey_F6; break;
		case VK_F7:		extChar = wAccelKey_F7; break;
		case VK_F8:		extChar = wAccelKey_F8; break;
		case VK_F9:		extChar = wAccelKey_F9; break;
		case VK_F10:	extChar = wAccelKey_F10; break;
		case VK_F11:	extChar = wAccelKey_F11; break;
		case VK_F12:	extChar = wAccelKey_F12; break;
		}
	return extChar;
}


long notModKey;
int mswTranslateAccelerator(
		HWND hWnd,
		LPMSG pMsg )
{
	long acclKey;
	long state;
	wWin_p win;
	wControl_p b;

	if ( pMsg->message != WM_KEYDOWN )
		return FALSE;
	acclKey = pMsg->wParam;
	b = getControlFromCursor( pMsg->hwnd, &win );
	if ( win == NULL )
		return 0;
	if ( b != NULL ) {
		switch (b->type) {
		case B_STRING:
		case B_INTEGER:
		case B_FLOAT:
		case B_LIST:
		case B_DROPLIST:
		case B_COMBOLIST:
		case B_TEXT:
			return 0;
		}
	}
	if ( acclKey == (long)VK_F1 ) {
		if (!b)
			return 0;
		closeBalloonHelp();
		if (b->helpStr)
			wHelp( b->helpStr );
		return 1;
	}
	/*acclKey = translateExtKey( (WORD)acclKey );*/
	state = 0;
	if ( GetKeyState(VK_CONTROL) & 0x1000 )
		state |= WKEY_CTRL;
	if ( GetKeyState(VK_MENU) & 0x1000 )
		state |= WKEY_ALT;
	if ( GetKeyState(VK_SHIFT) & 0x1000 )
		state |= WKEY_SHIFT;
	state <<= 8;
	acclKey |= state;
	if (pMsg->wParam > 0x12)
		notModKey = TRUE;
	return mswMenuAccelerator( win, acclKey );
}

/*
 ******************************************************************************
 *
 * Window Utilities
 *
 ******************************************************************************
 */



void wGetDisplaySize( POS_T * width, POS_T * height )
{
	*width = screenWidth;
	*height = screenHeight;
}


void wWinGetSize( wWin_p w, POS_T * width, POS_T * height )
{
	RECT rect;
	GetWindowRect( w->hWnd, &rect );
	GetClientRect( w->hWnd, &rect );
	w->w = rect.right - rect.left;
	w->h = rect.bottom - rect.top;
	*width = w->w;
	*height = w->h;
}


void wWinSetSize( wWin_p w, POS_T width, POS_T height )
{
	RECT rect;
	w->w = width;
	w->h = height;
	rect.left = 0;
	rect.top = 0;
	rect.right = w->w /*+w->padX*/;
	rect.bottom = w->h /*+w->padY*/;
	AdjustWindowRect( &rect, w->style, (w->option&F_MENUBAR)?1:0 );
	rect.bottom += mFixBorderH;
	if (!SetWindowPos( w->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right-rect.left, rect.bottom-rect.top,
		SWP_NOMOVE|SWP_NOZORDER))
		mswFail("wWinSetSize");
	InvalidateRect( w->hWnd, NULL, TRUE );
}


static int blocking;
static void blockingLoop( void )
{
	MSG msg;
	int myBlocking=blocking;
	while (blocking>=myBlocking && GetMessage( &msg, NULL, 0, 0 )) {
		if ( 
#ifdef DOTRANSACCEL
			 (!TranslateAccelerator( mswWin->hWnd, hMswAccel, &msg )) &&
#endif
			 (!mswTranslateAccelerator( mswWin->hWnd, &msg )) ) {
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
}


static void savePos( wWin_p win )
{
	char posStr[20];
	WINDOWPLACEMENT windowPlace;
	wPos_t w, h;
	RECT rect;

	if ( win->nameStr &&
		 IsWindowVisible( win->hWnd) /*&& !IsIconic( win->hWnd )*/ ) {
		windowPlace.length = sizeof windowPlace;
		GetWindowPlacement( win->hWnd, &windowPlace );
		if (win->option&F_RECALLPOS) {
			wsprintf( posStr, "%d %d",
				windowPlace.rcNormalPosition.left,
				windowPlace.rcNormalPosition.top );
			wPrefSetString( "msw window pos", win->nameStr, posStr );
			if (win->option&F_RESIZE) {
				GetClientRect( win->hWnd, &rect );
				w = rect.right - rect.left;
				h = rect.bottom - rect.top;
				w = windowPlace.rcNormalPosition.right - windowPlace.rcNormalPosition.left;
				h = windowPlace.rcNormalPosition.bottom - windowPlace.rcNormalPosition.top;
				w -= mResizeBorderW*2;
				h -= mResizeBorderH*2 + mTitleH;
				if (win->option&F_MENUBAR)
					h -= mMenuH;
				wsprintf( posStr, "%d %d %d",
					(  windowPlace.showCmd == SW_SHOWMINIMIZED ? 1 :
					  (windowPlace.showCmd == SW_SHOWMAXIMIZED ? 2 : 0 ) ),
					w, h );
				wPrefSetString( "msw window size", win->nameStr, posStr );
			}
		}
	}
}


void wWinShow(
		wWin_p win,
		BOOL_T show )
{
	wPos_t x, y;
	wWin_p win1;

	win->busy = TRUE;
	if (show) {
		if (win->centerWin) {
			x = (screenWidth-win->w)/2;
			y = (screenHeight-win->h)/2;
			if (x<0)
				y = 0;
			if (x<0)
				y = 0;
			if (!SetWindowPos( win->hWnd, HWND_TOP, x, y,
				CW_USEDEFAULT, CW_USEDEFAULT,
				SWP_NOSIZE|SWP_NOZORDER))
				mswFail( "wWinShow:SetWindowPos()" );
		}
		win->centerWin = FALSE;
		win->shown = TRUE;
		if (mswHWnd == (HWND)0 || !IsIconic(mswHWnd) ) {
			ShowWindow( win->hWnd, SW_SHOW );
			if (win->focusChainFirst) {
				SetFocus( win->focusChainFirst->hWnd );
			}
			win->pendingShow = FALSE;
			if ( mswWinBlockEnabled && (win->option & F_BLOCK) ) {
				blocking++;
				inMainWndProc = FALSE;
				for ( win1 = winFirst; win1; win1=(wWin_p)win1->next ) {
					if ( win1->shown && win1 != win ) {
						if (win1->modalLevel == 0 )
							EnableWindow( win1->hWnd, FALSE );
						win1->modalLevel++;
					}
				}
				win->busy = FALSE;
				blockingLoop();
			}
		} else {
			win->pendingShow = TRUE;
			needToDoPendingShow = TRUE;
		}
	} else {
		savePos( win );
		ShowWindow( win->hWnd, SW_HIDE );
		/*SetWindowPos( win->hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_HIDEWINDOW );*/
		if ( mswWinBlockEnabled && (win->option & F_BLOCK) ) {
			blocking--;
				for ( win1 = winFirst; win1; win1=(wWin_p)win1->next ) {
					if ( win1->shown && win1 != win ) {
						if ( win1->modalLevel > 0 )
							win1->modalLevel--;
						if (win1->modalLevel == 0 )
							EnableWindow( win1->hWnd, TRUE );
					}
				}
		}
		savePos( win );
		win->pendingShow = FALSE;
		win->shown = FALSE;
	}
	win->busy = FALSE;
}


void wWinBlockEnable(
		wBool_t enabled )
{
	 mswWinBlockEnabled = enabled;
}


wBool_t wWinIsVisible(
		wWin_p w )
{
	return IsWindowVisible(w->hWnd);
}


void wWinSetTitle(
		wWin_p w,
		const char * title )
{
	SetWindowText( w->hWnd, title );
}


void wWinSetBusy(
		wWin_p w,
		BOOL_T busy )
{
	HMENU menuH;
	UINT uEnable;
	int cnt, inx;
	wControl_p b;

	w->isBusy = busy;
	menuH = GetMenu( w->hWnd );
	if (menuH) {
		uEnable = MF_BYPOSITION|(busy?MF_DISABLED:MF_ENABLED);
		cnt = GetMenuItemCount(menuH);
		for (inx=0; inx<cnt; inx++)
			EnableMenuItem( menuH, inx, uEnable );
	}
	for (b=w->first; b; b=b->next) {
		if ( (b->option&BO_DISABLED)==0) {
			if (mswCallBacks[b->type] != NULL &&
				mswCallBacks[b->type]->setBusyProc) {
				mswCallBacks[b->type]->setBusyProc( b, busy );
			} else {
				if (b->hWnd)
					EnableWindow( b->hWnd, (BOOL)!busy );
			}
		}
	}
}


const char * wWinGetTitle(
		wWin_p w )
{
	return w->labelStr;
}


void wWinClear(
		wWin_p win,
		wPos_t x,
		wPos_t y,
		wPos_t width,
		wPos_t height )
{
}

void wSetCursor(
		wCursor_t cursor )
{
	switch ( cursor ) {
	case wCursorNormal:
	case wCursorQuestion:
	default:
		SetCursor( LoadCursor( NULL, IDC_ARROW ) );
		break;
	case wCursorWait:
		SetCursor( LoadCursor( NULL, IDC_WAIT ) );
		break;
	case wCursorCross:
		SetCursor( LoadCursor( NULL, IDC_CROSS ) );
		break;
	case wCursorIBeam:
		SetCursor( LoadCursor( NULL, IDC_IBEAM ) );
		break;
	}
	curCursor = cursor;
}

void wWinDoCancel( wWin_p win )
{
	wControl_p b;
	for (b=win->first; b; b=b->next) {
		if ((b->type == B_BUTTON) && (b->option & BB_CANCEL) ) {
			mswButtPush( b );
		}
	}
}

unsigned long wGetTimer( void )
{
	return (unsigned long)GetTickCount();
}


wIcon_p wIconCreateBitMap( wPos_t w, wPos_t h, const char * bits, wDrawColor color )
{
	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	ip->type = mswIcon_bitmap;
	ip->w = w;
	ip->h = h;
	ip->color = color;
	ip->bits = bits;
	return ip;
}

wIcon_p wIconCreatePixMap( char *pm[] )
{
	wIcon_p ip;
	int col, r, g, b;
	long rgb;
	char buff[3];
	char * cp, * cq;

	ip = (wIcon_p)malloc( sizeof *ip );
	memset( ip, 0, sizeof *ip );
	ip->type = mswIcon_pixmap;
	cp = pm[0];
	ip->w = (int)strtol(cp, &cq, 10 );
	ip->h = (int)strtol(cq, &cp, 10 );
	ip->colorcnt = (int)strtol(cp, &cq, 10 );
	ip->type = mswIcon_pixmap;
	ip->colormap = (wIconColorMap_t*)malloc( ip->colorcnt * sizeof ip->colormap[0] );
	for ( col=0; col<ip->colorcnt; col++ ) {
		ip->colormap[col].key = pm[col+1][0];
		buff[2] = 0;
		memcpy( buff, &pm[col+1][5], 2 );
		r = (int)strtol(buff, &cp, 16);
		memcpy( buff, &pm[col+1][9], 2 );
		g = (int)strtol(buff, &cp, 16);
		memcpy( buff, &pm[col+1][13], 2 );
		b = (int)strtol(buff, &cp, 16);
		rgb = wRGB( r, g, b );
		ip->colormap[col].color = wDrawFindColor( rgb );
	}
	ip->color = 0;
	ip->bits = &pm[1+ip->colorcnt];
	return ip;
}

void wIconSetColor( wIcon_p ip, wDrawColor color )
{
	ip->color = color;
}


/*
 ******************************************************************************
 *
 * Control Utilities
 *
 ******************************************************************************
 */



void wControlSetHelp(
		wControl_p b,
		const char * help )
{
	if (b->helpStr)
		free(CAST_AWAY_CONST b->helpStr);
	if (help)
		b->helpStr = mswStrdup( help );
	else
		b->helpStr = NULL;
}


void wControlShow( wControl_p b, BOOL_T show )
{
	RECT rc;
	if (show) {
		if (mswCallBacks[b->type] != NULL &&
			mswCallBacks[b->type]->repaintProc)
			mswCallBacks[b->type]->repaintProc( b->parent->hWnd, b );
	} else {
		if( b->labelStr ) {
			rc.left = b->labelX;
			rc.right = b->x;
			rc.top = b->labelY;
			rc.bottom = b->labelY+b->h;
			InvalidateRect( ((wControl_p)b->parent)->hWnd, &rc, TRUE );
		}
	}
	if (mswCallBacks[b->type] != NULL &&
		mswCallBacks[b->type]->showProc) {
		mswCallBacks[b->type]->showProc( b, show );
	} else {
		ShowWindow( b->hWnd, show?SW_SHOW:SW_HIDE );
#ifdef SHOW_DOES_SETFOCUS
		if (show && (b->option&BO_READONLY)==0 && b->hWnd != GetFocus() ) {
			hWnd = SetFocus( b->hWnd );
		}
#endif
	}
	b->shown = show;
}


void wControlSetFocus(
		wControl_p b )
{
	if ( b->hWnd )
		SetFocus( b->hWnd );
}


void wControlActive(
		wControl_p b,
		int active )
{
	if (active)
		b->option &= ~BO_DISABLED;
	else
		b->option |= BO_DISABLED;
	if (b->parent->isBusy)
		return;
	if (mswCallBacks[b->type] != NULL &&
		mswCallBacks[b->type]->setBusyProc) {
		mswCallBacks[b->type]->setBusyProc( b, !active );
	} else {
		EnableWindow( b->hWnd, (BOOL)active );
		InvalidateRect( b->hWnd, NULL, TRUE );
	}
}


const char * wControlGetHelp( wControl_p b )
{
	return b->helpStr;
}


wPos_t wLabelWidth( const char * labelStr )
{
	int lab_l;
	HDC hDc;
	DWORD dw;
	LABELFONTDECL

	hDc = GetDC( mswHWnd );
	lab_l = strlen(labelStr);
	LABELFONTSELECT
	dw = GetTextExtent( hDc, CAST_AWAY_CONST labelStr, lab_l );
	LABELFONTRESET
	ReleaseDC( mswHWnd, hDc );
	return LOWORD(dw) + 5;
}


wPos_t wControlGetWidth(
		wControl_p b)			/* Control */
{
	return b->w;
}


wPos_t wControlGetHeight(
		wControl_p b)			/* Control */
{
	return b->h;
}


wPos_t wControlGetPosX(
		wControl_p b)			/* Control */
{
	return b->x;
}


wPos_t wControlGetPosY(
		wControl_p b)			/* Control */
{
	return b->y;
}


void wControlSetPos(
		wControl_p b,
		wPos_t x,
		wPos_t y )
{
	b->labelX = x;
	b->labelY = y+2;

	if (b->labelStr) {
		int lab_l;
		HDC hDc;
		DWORD dw;
		LABELFONTDECL

		hDc = GetDC( b->parent->hWnd );
		LABELFONTSELECT
		lab_l = strlen(b->labelStr);
		dw = GetTextExtent( hDc, CAST_AWAY_CONST b->labelStr, lab_l );
		b->labelX -= LOWORD(dw) + 5;
		LABELFONTRESET
		ReleaseDC( b->parent->hWnd, hDc );
	}

	if (mswCallBacks[b->type] != NULL &&
		mswCallBacks[b->type]->setPosProc) {
		mswCallBacks[b->type]->setPosProc( b, x, y );
	} else {
		b->x = x;
		b->y = y;
		if (b->hWnd)
			if (!SetWindowPos( b->hWnd, HWND_TOP, x, y,
				CW_USEDEFAULT, CW_USEDEFAULT,
				SWP_NOSIZE|SWP_NOZORDER))
				mswFail("wControlSetPos");
	}
}


void wControlSetLabel(
		wControl_p b,
		const char * labelStr )
{
	if ( b->type == B_RADIO || b->type == B_TOGGLE ) {
		;
	} else {
		int lab_l;
		HDC hDc;
		DWORD dw;
		LABELFONTDECL

		hDc = GetDC( b->parent->hWnd );
		lab_l = strlen(labelStr);
		LABELFONTSELECT
		dw = GetTextExtent( hDc, CAST_AWAY_CONST labelStr, lab_l );
		LABELFONTRESET
		b->labelX = b->x - LOWORD(dw) - 5;
		ReleaseDC( b->parent->hWnd, hDc );
		b->labelStr = mswStrdup( labelStr );
		if (b->type == B_BUTTON)
			SetWindowText( b->hWnd, labelStr );
	}
}


void wControlSetContext(
		wControl_p b,
		void * context )
{
	b->data = context;
}

static int controlHiliteWidth = 5;
static int controlHiliteWidth2 = 3;
void wControlHilite(
		wControl_p b,
		wBool_t hilite )
{
	HDC hDc;
	HPEN oldPen, newPen;
	int oldMode;

	if ( b == NULL ) return;
    if ( !IsWindowVisible(b->parent->hWnd) ) return;
    if ( !IsWindowVisible(b->hWnd) ) return;
	hDc = GetDC( b->parent->hWnd );
	newPen = CreatePen( PS_SOLID, controlHiliteWidth, RGB(0,0,0) );
	oldPen = SelectObject( hDc, newPen );
	oldMode = SetROP2( hDc, R2_NOTXORPEN );
	MoveTo( hDc, b->x-controlHiliteWidth2, b->y-controlHiliteWidth2 );
	LineTo( hDc, b->x+b->w+controlHiliteWidth2, b->y-controlHiliteWidth2 );
	LineTo( hDc, b->x+b->w+controlHiliteWidth2, b->y+b->h+controlHiliteWidth2 );
	LineTo( hDc, b->x-controlHiliteWidth2, b->y+b->h+controlHiliteWidth2 );
	LineTo( hDc, b->x-controlHiliteWidth2, b->y-controlHiliteWidth2 );
	SetROP2( hDc, oldMode );
	SelectObject( hDc, oldPen );
	DeleteObject( newPen );
	ReleaseDC( b->parent->hWnd, hDc );
}

/*
 *****************************************************************************
 *
 * Exported Utility Functions 
 *
 *****************************************************************************
 */


void wMessage(
		wWin_p w,
		const char * msg,
		int beep )
{
	HDC hDc;
	int oldRop;
	POS_T h;
	RECT rect;
	LABELFONTDECL

	if (beep)
		MessageBeep(0);
	GetClientRect( w->hWnd, &rect );
	hDc = GetDC( w->hWnd );
	oldRop = SetROP2( hDc, R2_WHITE );
	h = w->h+2;
	Rectangle( hDc, 0, h, w->w, h );
	SetROP2( hDc, oldRop );
	LABELFONTSELECT
	TextOut( hDc, 0, h, msg, strlen(msg) );
	LABELFONTRESET
	ReleaseDC( w->hWnd, hDc );
}


void wExit( int rc )
{
	INDEX_T inx;
	wControl_p b;

	mswPutCustomColors();
	wPrefFlush();
	for ( inx=controlMap_da.cnt-1; inx>=0; inx-- ) {
		b = controlMap(inx).b;
		if (b != NULL) {
			if (b->type == W_MAIN || b->type == W_POPUP) {
				wWin_p w = (wWin_p)b;
				savePos( w );
				if (w->winProc != NULL)
					w->winProc( w, wQuit_e, w->data );
			}
		}
	}
	for ( inx=controlMap_da.cnt-1; inx>=0; inx-- ) {
		b = controlMap(inx).b;
		if (b != NULL) {
			if (mswCallBacks[b->type] != NULL &&
				mswCallBacks[b->type]->doneProc != NULL)
				mswCallBacks[b->type]->doneProc( b );
		}
		controlMap(inx).b = NULL;
	}
	deleteBitmaps();
	if (mswOldTextFont != (HFONT)0)
		DeleteObject( mswOldTextFont );
	if (helpInitted) {
		WinHelp(mswHWnd, helpFile, HELP_QUIT, 0L );
		helpInitted = FALSE;
	}
	if (balloonHelpHWnd) {
		HDC hDc;
		hDc = GetDC( balloonHelpHWnd );
		SelectObject( hDc, balloonHelpOldFont );
		DeleteObject( balloonHelpNewFont );
		ReleaseDC( balloonHelpHWnd, hDc );
	}
#ifdef HELPSTR
	fclose( helpStrF );
#endif
	DestroyWindow( mswHWnd );
	if (mswPalette) {
		DeleteObject( mswPalette );
		/*DeleteObject( mswPrintPalette );*/
	}
}


void wFlush(
		void )
{
	wWin_p win;

	inMainWndProc = FALSE;
	mswRepaintAll();
	for (win=winFirst; win; win=(wWin_p)win->next)
		UpdateWindow( win->hWnd );
}

void wUpdate(
		wWin_p	win )
{
	UpdateWindow( win->hWnd );
}

static wBool_t paused;
static wAlarmCallBack_p alarmFunc;
static setTriggerCallback_p triggerFunc;
static wControl_p triggerControl;

static void pausedLoop( void )
{
	MSG msg;
	while (paused && GetMessage( &msg, NULL, 0, 0 )) {
		if (
#ifdef DOTRANSACCEL
			 (!TranslateAccelerator( mswWin->hWnd, hMswAccel, &msg )) &&
#endif
			 (!mswTranslateAccelerator( mswWin->hWnd, &msg )) ) {
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
}


void wPause(
		long msec )
{
	paused = TRUE;
	if (msec > 65000L)
		msec = 65000L;
	pauseTimer = SetTimer( mswHWnd, PAUSE_TIMER, (UINT)msec, NULL );
	if (pauseTimer == 0)
		mswFail("wPause: No timers");
	else
		pausedLoop();
}


void wAlarm(
		long msec,
		wAlarmCallBack_p func )
{
	alarmFunc = func;
	if (msec > 65000L)
		msec = 65000L;
	alarmTimer = SetTimer( mswHWnd, ALARM_TIMER, (UINT)msec, NULL );
	if (alarmTimer == 0)
		mswFail("wAlarm: No timers");
}


void mswSetTrigger(
		wControl_p control,
		setTriggerCallback_p func )
{
	UINT msec = (UINT)500;
	triggerControl = control;
	triggerFunc = func;
	if (func == NULL && triggerTimer != 0) {
		KillTimer( mswHWnd, triggerTimer );
		triggerTimer = 0;
		return;
	}
	if (msec > 65000L)
		msec = 65000L;
	triggerTimer = SetTimer( mswHWnd, TRIGGER_TIMER, (UINT)msec, NULL );
	if (triggerTimer == 0)
		mswFail("wAlarm: No timers");
}


void wBeep( void )
{
	MessageBeep( MB_OK );
}


int wNotice(
		const char * msg,
		const char * yes,
		const char * no )
{
	int res;
	res = MessageBox( mswHWnd, msg, "Notice", MB_TASKMODAL|((no==NULL)?MB_OK:MB_YESNO) );
	return res == IDOK || res == IDYES;
}


int wNotice3(
		const char * msg,
		const char * yes,
		const char * no,
		const char * cancel )
{
	int res;
	res = MessageBox( mswHWnd, msg, "Notice", MB_TASKMODAL|MB_YESNOCANCEL );
	if ( res == IDOK || res == IDYES )
		return 1;
	else if ( res == IDCANCEL )
		return -1;
	else
		return 0;
}


void wHelp(
		const char * topic )
{
	const char * cp;
	char command[80], *cq;
	int rc;
	if (!helpInitted) {
		helpInitted = TRUE;
	}
		rc = WinHelp( mswHWnd, helpFile, HELP_CONTENTS, 0L );
	wsprintf( command, "JumpId(\"%s.hlp\",\"", appName );
	for ( cp = topic, cq = command+strlen(command); *cp; cq++,cp++ ) {
		if ( *cq == '-' )
			*cq = '_';
		else
			*cq = toupper(*cp);
	}
	*cq++ = '"';																	 
	*cq++ = ')';
	*cq++ = '\0';
	rc = WinHelp( mswHWnd, helpFile, HELP_COMMAND, (DWORD)command );
	if (rc == 0)
		wNotice( command, "Ok", NULL );
}


void doHelpMenu( void * context )
{
	switch ((int)(long)context) {
	case 1: /* Contents */
		WinHelp( mswHWnd, helpFile, HELP_CONTENTS, (DWORD)0 );
		break;
	case 2: /* Search */
		WinHelp( mswHWnd, helpFile, HELP_CONTENTS, (DWORD)0 );
		WinHelp( mswHWnd, helpFile, HELP_COMMAND, (DWORD)(LPCSTR)"Search()" );
		break;
	case 3: /* Help on Help */
		WinHelp( mswHWnd, "WINHELP.HLP", HELP_INDEX, 0L );
		break;
	default:
		return;
	}
	helpInitted = TRUE;
}

void wMenuAddHelp(
		wMenu_p m )
{
	wMenuPushCreate( m, NULL, "&Contents", 0, doHelpMenu, (void*)1 );
	wMenuPushCreate( m, NULL, "&Search for Help on...", 0, doHelpMenu, (void*)2 );
	wMenuPushCreate( m, NULL, "&How to Use Help", 0, doHelpMenu, (void*)3 );
}


void wSetBalloonHelp( wBalloonHelp_t * bh )
{
	balloonHelpStrings = bh;
}


void wEnableBalloonHelp( int enable )
{
	balloonHelpEnable = enable;
}


void wBalloonHelpUpdate ( void )
{
}


void wControlSetBalloonText(  wControl_p b, const char * text )
{
	b->tipStr = mswStrdup( text );
}


void startBalloonHelp( void )
{
	HDC hDc;
	DWORD extent;
	int w, h;
	RECT rect;
	POINT pt;
	wBalloonHelp_t * bh;
	const char * hs;
	HFONT hFont;

	if (!balloonHelpStrings)
		return;
	if (!balloonHelpEnable)
		return;
	if (balloonHelpHWnd) {
		if ( balloonHelpButton->tipStr ) {
			hs = balloonHelpButton->tipStr;
		} else {
			hs = balloonHelpButton->helpStr;
			if (!hs)
				return;
			for ( bh = balloonHelpStrings; bh->name && strcmp(bh->name,hs) != 0; bh++ );
			if (!bh->name || !bh->value)
				return;
			balloonHelpButton->tipStr = hs = bh->value;
		}
if (newHelp) {
		wControlSetBalloon( balloonHelpButton, 0, 0, hs );
} else {
		hDc = GetDC( balloonHelpHWnd );
		hFont = SelectObject( hDc, mswLabelFont );
		extent = GetTextExtent( hDc, CAST_AWAY_CONST hs, strlen(hs) );
		w = LOWORD( extent );
		h = HIWORD( extent );
		pt.x = 0;
		if ( balloonHelpButton->type == B_RADIO ||
			 balloonHelpButton->type == B_TOGGLE ) {
			pt.y = balloonHelpButton->h;
		} else {
			GetClientRect( balloonHelpButton->hWnd, &rect );
			pt.y = rect.bottom;
		}
		ClientToScreen( balloonHelpButton->hWnd, &pt );
		if (pt.x + w+2 > screenWidth)
			pt.x = screenWidth-(w+2);
		if (pt.x < 0)
			pt.x = 0;
		SetWindowPos( balloonHelpHWnd, HWND_TOPMOST, pt.x, pt.y, w+2, h+2,
						SWP_SHOWWINDOW|SWP_NOACTIVATE );
		SetBkColor( hDc, RGB( 255, 255, 0 ) );
		TextOut( hDc, 0, 0, hs, strlen(hs) );
		SelectObject( hDc, hFont );
		ReleaseDC( balloonHelpHWnd, hDc );
}
	}
}

void closeBalloonHelp( void )
{
	if (balloonHelpTimer) {
		KillTimer( mswHWnd, balloonHelpTimer );
		balloonHelpTimer = 0;
	}
	if (balloonHelpState == balloonHelpShow)
		if (balloonHelpHWnd)
			 ShowWindow( balloonHelpHWnd, SW_HIDE );
	balloonHelpState = balloonHelpIdle;
}


void wControlSetBalloon( wControl_p b, wPos_t dx, wPos_t dy, const char * msg )
{
	HDC hDc;
	DWORD extent;
	int w, h;
	RECT rect;
	POINT pt;
	HFONT hFont;

	if ( msg ) {
		hDc = GetDC( balloonHelpHWnd );
		hFont = SelectObject( hDc, mswLabelFont );
		extent = GetTextExtent( hDc, CAST_AWAY_CONST msg, strlen(msg) );
		w = LOWORD( extent );
		h = HIWORD( extent );
		if ( b->type == B_RADIO ||
			 b->type == B_TOGGLE ) {
			pt.y = b->h;
		} else {
			GetClientRect( b->hWnd, &rect );
			pt.y = rect.bottom;
		}
		pt.x = dx;
		pt.y -= dy;
		ClientToScreen( b->hWnd, &pt );
		if (pt.x + w+2 > screenWidth)
			pt.x = screenWidth-(w+2);
		if (pt.x < 0)
			pt.x = 0;
		SetWindowPos( balloonHelpHWnd, HWND_TOPMOST, pt.x, pt.y, w+2, h+2,
						SWP_SHOWWINDOW|SWP_NOACTIVATE );
		SetBkColor( hDc, RGB( 255, 255, 0 ) );
		TextOut( hDc, 0, 0, msg, strlen(msg) );
		SelectObject( hDc, hFont );
		ReleaseDC( balloonHelpHWnd, hDc );

		balloonHelpState = balloonHelpShow; 
		balloonControlButton = b;
	} else {
		closeBalloonHelp();
	}
}


int wGetKeyState( void )
{
	int rc, keyState;
	rc = 0;
	keyState = GetAsyncKeyState( VK_SHIFT );
	if (keyState & 0x8000)
		rc |= WKEY_SHIFT;
	keyState = GetAsyncKeyState( VK_CONTROL );
	if (keyState & 0x8000)
		rc |= WKEY_CTRL;
	keyState = GetAsyncKeyState( VK_MENU );
	if (keyState & 0x8000)
		rc |= WKEY_ALT;
	return rc;
}


/*
 ******************************************************************************
 *
 * File Selection
 *
 ******************************************************************************
 */

FILE * wFileOpen(
		const char * fileName,
		const char * mode )
{
#ifdef WIN32
		return fopen( fileName, mode );
#else
	HFILE hf;
	FILE * f;
	switch (mode[0]) {
	case 'r':
		hf = _lopen( fileName, READ );
		break;
	case 'w':
		hf = _lcreat( fileName, 0 );
		break;
	case 'a':
		hf = _lopen( fileName, WRITE );
		if (hf != HFILE_ERROR) {
			_llseek( hf, 0L, 2 );
		} else {
			hf = _lcreat( fileName, 0 );
		}
		break;
	default:
		mswFail( "wFileOpen: bad mode" );
		return NULL;
	}
	if ( hf == HFILE_ERROR )
		return NULL;
	f = fdopen( hf, mode );
	return f;
#endif
}


struct wFilSel_t {
		wWin_p parent;
		wFilSelMode_e mode;
		int option;
		const char * title;
		char * extList;
		wFilSelCallBack_p action;
		void * data;
		};
		
static char selFileName[1024];
static char selFileTitle[1024];
static char sysDirName[1024];

int wFilSelect(
		struct wFilSel_t * fs,
		const char * dirName )
{
	int rc;
	OPENFILENAME ofn;
	char * fileName;
	const char * ext;
	char defExt[4];
	int i;

	if (dirName == NULL ||
		dirName[0] == '\0' ||
		strcmp(dirName, ".") == 0 ) {
		GetSystemDirectory( CAST_AWAY_CONST (dirName = sysDirName), sizeof sysDirName );
	}
	memset( &ofn, 0, sizeof ofn );
	ofn.lStructSize = sizeof ofn;
	ofn.hwndOwner = mswHWnd;
	ofn.lpstrFilter = fs->extList;
	ofn.nFilterIndex = 0;
	selFileName[0] = '\0';
	ofn.lpstrFile = selFileName;
	ofn.nMaxFile = sizeof selFileName;
	selFileTitle[0] = '\0';
	ofn.lpstrFileTitle = selFileTitle;
	ofn.nMaxFileTitle = sizeof selFileTitle;
	ofn.lpstrInitialDir = dirName;
	ofn.lpstrTitle = fs->title;
	ext = fs->extList + strlen(fs->extList)+1;
	if (*ext++ == '*' && *ext++ == '.') {
		for ( i=0; i<3 && ext[i] && ext[i]!=';'; i++ )
			defExt[i] = ext[i];
		defExt[i] = '\0';
	} else {
		defExt[0] = '\0';
	}
	ofn.lpstrDefExt = defExt;
	ofn.Flags |= OFN_LONGFILENAMES;
	if (fs->mode == FS_LOAD) {
		ofn.Flags |= OFN_FILEMUSTEXIST;
		rc = GetOpenFileName( &ofn );
	} else if (fs->mode == FS_SAVE) {
		ofn.Flags |= OFN_OVERWRITEPROMPT;
		rc = GetSaveFileName( &ofn );
	} else if (fs->mode == FS_UPDATE) {
		rc = GetSaveFileName( &ofn );
	} else
		return FALSE;
	if (!rc)
		return FALSE;
	fileName = strrchr( selFileName, '\\' );
	if (fileName == NULL) {
		mswFail( "wFilSelect: cant extract fileName" );
		return FALSE;
	}
	fs->action( selFileName, fileName+1, fs->data );
	return TRUE;
}


struct wFilSel_t * wFilSelCreate(
		wWin_p parent,
		wFilSelMode_e mode,
		int option,
		const char * title,
		const char * extList,
		wFilSelCallBack_p action,
		void * data )
{
	char * cp;
	struct wFilSel_t * ret;
	int len;
	ret = (struct wFilSel_t*)malloc(sizeof *ret);
	ret->parent = parent;
	ret->mode = mode;
	ret->option = option;
	ret->title = mswStrdup(title);
	len = strlen(extList);
	ret->extList = (char*)malloc(len+2);
	strcpy(ret->extList,extList);
	for ( cp=ret->extList; *cp; cp++ ) {
		if (*cp == '|')
			*cp = '\0';
	}
	*++cp = '\0';
	ret->action = action;
	ret->data = data;
	return ret;
}


const char * wMemStats( void )
{
	int rc;
	static char msg[80];
	long usedSize = 0;
	long usedCnt = 0;
	long freeSize = 0;
	long freeCnt = 0;
	_HEAPINFO heapinfo;
	heapinfo._pentry = NULL;

	while ( (rc=_heapwalk( &heapinfo )) == _HEAPOK ) {
		switch (heapinfo._useflag) {
		case _FREEENTRY:
			freeSize += (long)heapinfo._size;
			freeCnt++;
			break;
		case _USEDENTRY:
			usedSize += (long)heapinfo._size;
			usedCnt++;
			break;
		}
	}
		
	sprintf( msg, "Used: %ld(%ld), Free %ld(%ld)%s",
		usedSize, usedCnt, freeSize, freeCnt,
		(rc==_HEAPOK)?"":
		(rc==_HEAPEMPTY)?"":
		(rc==_HEAPBADBEGIN)?", BADBEGIN":
		(rc==_HEAPEND)?"":
		(rc==_HEAPBADPTR)?", BADPTR":
		", Unknown Heap Status" );
	return msg;
}

/*
 *****************************************************************************
 *
 * Main
 *
 *****************************************************************************
 */

static wControl_p getControlFromCursor( HWND hWnd, wWin_p * winR )
{
	POINT pt;
	wWin_p w;
	wControl_p b;
	wIndex_t inx;
	HWND hTopWnd;

	if (winR)
		*winR = NULL;
	GetCursorPos( &pt );
	hTopWnd = GetActiveWindow();
	inx = GetWindowWord( hWnd, 0 );
	if ( inx < CONTROL_BASE || inx > controlMap_da.cnt ) {
		/* Unknown control */
		/*MessageBeep( MB_ICONEXCLAMATION );*/
		return NULL;
	}
	w=(wWin_p)controlMap(inx-CONTROL_BASE).b;
	if (!w)
		return NULL;
	if (w->type != W_MAIN && w->type != W_POPUP)
		return NULL;
	if ( winR )
		*winR = w;
	ScreenToClient( hWnd, &pt );
	for (b = w->first; b; b=b->next) {
		if (b->type == B_BOX || b->type == B_LINES)
			continue;
		if (b->hWnd == NULL)
			continue;
		if (IsWindowVisible( b->hWnd ) == FALSE)
			continue;
		if (pt.x > b->x && pt.x < b->x+b->w &&
			pt.y > b->y && pt.y < b->y+b->h )
			return b;
	}
	return b;	
}


LRESULT 
FAR 
PASCAL 
MainWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	int inx;
	wWin_p w;
	wControl_p b;
	int child = ((GetWindowLong( hWnd, GWL_STYLE) & WS_CHILD) != 0);
	POS_T newW, newH;
	RECT rect;
	PAINTSTRUCT ps;
	HWND hWnd2;
#ifdef CONTROL3D
#ifndef WIN32
	HBRUSH hBrush;
#endif
#endif
	LRESULT ret;
	HDC hDc;
	wAccelKey_e extChar;

	switch (message) {

	case WM_COMMAND:
	case WM_MEASUREITEM:
	case WM_DRAWITEM:
	case WM_NOTVALID:
		if (WCMD_PARAM_ID == IDM_DOHELP) {
			b = getControlFromCursor( hWnd, NULL );
			closeBalloonHelp();
			if (!b)
				return 0L;
			if (b->helpStr)
				wHelp( b->helpStr );
			return 0L;
		}
		closeBalloonHelp();
		if (WCMD_PARAM_ID < CONTROL_BASE || WCMD_PARAM_ID > (WPARAM)controlMap_da.cnt)
			break;
		b = controlMap(WCMD_PARAM_ID-CONTROL_BASE).b;
		if (!b)
			break;
		mswSetFocus( b );
		ret = 0L;
		if (!inMainWndProc) {
			inMainWndProc = TRUE;
			if (mswCallBacks[b->type] != NULL &&
				mswCallBacks[b->type]->messageProc) {
				ret = mswCallBacks[b->type]->messageProc( b, hWnd, message, wParam, lParam );
			}
			inMainWndProc = FALSE;
		}
		return ret;

	case WM_PAINT:
		inx = GetWindowWord( hWnd, 0 );
		if (inx >= CONTROL_BASE && inx <= controlMap_da.cnt &&
			(w = (wWin_p)controlMap(inx-CONTROL_BASE).b) &&
			(w->type == W_MAIN || w->type == W_POPUP) &&
			(!IsIconic(mswHWnd)) &&
			(GetUpdateRect( hWnd, &rect, FALSE ) ) ) {
			BeginPaint( hWnd, &ps );
			for (b=w->first; b; b=b->next ) {
				if (b->shown &&
					mswCallBacks[b->type] != NULL &&
					mswCallBacks[b->type]->repaintProc)
					mswCallBacks[b->type]->repaintProc( hWnd, b );
			}
			EndPaint( hWnd, &ps );
			return 1L;
		}
		break;

	case WM_SIZE:
		inx = GetWindowWord( hWnd, 0 );
		if (inx < CONTROL_BASE || inx > controlMap_da.cnt)
			break;
		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;
		if (!w)
			break;
		if (w->type != W_MAIN && w->type != W_POPUP) 
			break;
		if (w->busy)
			break;
		switch( wParam ) {
		case SIZE_MAXIMIZED:
		case SIZE_MINIMIZED:
		case SIZE_RESTORED:
			newW = LOWORD( lParam );		/* WIN32?? */
			newH = HIWORD( lParam );		/* WIN32?? */
			if (newW <= 0 || newH <= 0)
				break;
			if (newW == w->w && newH == w->h)
				break;
			GetWindowRect( w->hWnd, &rect );
			GetClientRect( w->hWnd, &rect );
			InvalidateRect( w->hWnd, NULL, TRUE );
			w->w = newW;
			w->h = newH;
			if (w->winProc)
				w->winProc( w, wResize_e, w->data );
			break;
		default:
			break;
		}
		break;

	case WM_CHAR:  
	case WM_KEYUP:	
		inx = GetWindowWord( hWnd, 0 );
		if ( inx < CONTROL_BASE || inx > controlMap_da.cnt )
			break;
		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;
		if (!w)
			break;
		if (w->type != W_MAIN && w->type != W_POPUP) {
			if (mswCallBacks[w->type] != NULL &&
				mswCallBacks[w->type]->messageProc)
				return mswCallBacks[w->type]->messageProc( (wControl_p)w, hWnd,
								message, wParam, lParam );
			break;
		}
		extChar = translateExtKey( WCMD_PARAM_ID );
		if (message == WM_KEYUP ) {
			if (extChar == wAccelKey_None)
				break;
			if (extChar == wAccelKey_Back)
				break;
		}
		b = getControlFromCursor( hWnd, NULL );
		closeBalloonHelp();
		if (b && b->type == B_DRAW) {
			return SendMessage( b->hWnd, WM_CHAR, wParam, lParam );
		}
		switch (WCMD_PARAM_ID) {
		case 0x0D:
			/* CR - push default button */
			for (b=w->first; b; b=b->next) {
				if (b->type == B_BUTTON && (b->option & BB_DEFAULT) != 0) {
					inMainWndProc = TRUE;
					if (mswCallBacks[B_BUTTON] != NULL &&
						mswCallBacks[B_BUTTON]->messageProc) {
						ret = mswCallBacks[B_BUTTON]->messageProc( b, b->hWnd,
								WM_COMMAND, wParam, lParam );
					}
					inMainWndProc = FALSE;
					break;
				}
			}
			return 0L;
		case 0x1B:
			/* ESC - push cancel button */
			for (b=w->first; b; b=b->next) {
				if (b->type == B_BUTTON && (b->option & BB_CANCEL) != 0) {
					inMainWndProc = TRUE;
					if (mswCallBacks[B_BUTTON] != NULL &&
						mswCallBacks[B_BUTTON]->messageProc) {
						ret = mswCallBacks[B_BUTTON]->messageProc( b, b->hWnd,
								WM_COMMAND, wParam, lParam );
					}
					inMainWndProc = FALSE;
					break;
				}
			}
			return 0L;
		case 0x20:
			/* SPC - push current button with focus */
			if ( (b=w->focusChainNext) != NULL ) {
				switch (b->type) {
				case B_BUTTON:
				case B_CHOICEITEM:
					inMainWndProc = TRUE;
					if (mswCallBacks[b->type] != NULL &&
						mswCallBacks[b->type]->messageProc) {
#ifdef WIN32
						ret = mswCallBacks[b->type]->messageProc( b, b->hWnd,
								WM_COMMAND, MAKELPARAM( LOWORD(wParam), BN_CLICKED), (LPARAM)(b->hWnd) );
#else
						ret = mswCallBacks[b->type]->messageProc( b, b->hWnd,
								WM_COMMAND, wParam, MAKELPARAM( b->hWnd, BN_CLICKED ) );
#endif
					}
					inMainWndProc = FALSE;
					break;
				}
			}
			return 0L;
		case 0x09:
			/* TAB - jump to next control */
			if ( w->focusChainNext ) {
				for ( b = w->focusChainNext->focusChainNext;
					  b!=w->focusChainNext && !IsWindowVisible(b->hWnd) && !IsWindowEnabled(b->hWnd);
					  b=b->focusChainNext );
				w->focusChainNext = b;
				if (!inMainWndProc) {
					inMainWndProc = TRUE;
					SetFocus( b->hWnd );
					if (b->type == B_BUTTON)
						InvalidateRect( b->hWnd, NULL, TRUE );
					inMainWndProc = FALSE;
				}
			}
			return 0L;
		}
		/* Not a Draw control */
		MessageBeep( MB_ICONHAND );
		break;					   

	case WM_ENABLE:
		if (wParam == 1) {		/* WIN32??? */
			hWnd2 = SetFocus( hWnd );
		}		 
		break;

	case WM_F1DOWN:
		if ((hWnd2 = GetActiveWindow()) == hWnd ||
			(inx=GetWindowWord(hWnd2,0)) < CONTROL_BASE || inx > controlMap_da.cnt )
			return DefWindowProc( hWnd, message, wParam, lParam );
		b=controlMap(inx-CONTROL_BASE).b;
		if (!b)
			break;
		closeBalloonHelp();
		wHelp( b->helpStr );
		return 0L;

	case WM_SETCURSOR:
		/*if (any buttons down)
			break;*/
		wSetCursor( curCursor );
		if (!mswAllowBalloonHelp)
			break;
		if (IsIconic(mswHWnd))
			break;
		b = getControlFromCursor(hWnd, NULL);
		if ( b == balloonControlButton )
				break;
		if ( /*(!IsWindowEnabled(hWnd))*/ GetActiveWindow() != hWnd ||
			 (!b) || b->type == B_DRAW || b->helpStr == NULL ) {
			closeBalloonHelp();
			break;
		}
		if ( b != balloonHelpButton )
			 closeBalloonHelp();
		if (balloonHelpState != balloonHelpIdle) {
			break;
		}
		balloonHelpTimer = SetTimer( mswHWnd, BALLOONHELP_TIMER,
				balloonHelpTimeOut, NULL );
		if (balloonHelpTimer == (UINT)0)
			break;
		balloonHelpState = balloonHelpWait;
		balloonHelpButton = b;
		break;

	case WM_SYSCOMMAND:
		inx = GetWindowWord( hWnd, 0 );
		if (inx < CONTROL_BASE || inx > controlMap_da.cnt)
			break;
		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;
		if (!w)
			break;
		if (w->type != W_POPUP) 
			break;
		if (w->busy)
			break;
		if ( (wParam&0xFFF0) != SC_CLOSE )
			break;
		if (w->winProc)
			w->winProc( w, wClose_e, w->data );
		wWinShow( w, FALSE );
		return 0L;

		

	case WM_CLOSE:
		inx = GetWindowWord( hWnd, 0 );
		if (inx < CONTROL_BASE || inx > controlMap_da.cnt)
			break;
		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;
		if (!w)
			break;
		if (w->type == W_MAIN) {
			/* It's the big one! */
			wExit( 0 );
			return 0L;
		}

	case WM_DESTROY:
		if ( hWnd == mswHWnd ) {
			PostQuitMessage(0L);
			return 0L;
		}
		break;

	case WM_TIMER:
		if (wParam == PAUSE_TIMER) {				/* WIN32??? */
			paused = FALSE;
			KillTimer( hWnd, pauseTimer );
		} else if (wParam == ALARM_TIMER) {
			KillTimer( mswHWnd, alarmTimer );
			alarmFunc();
		} else if (wParam == TRIGGER_TIMER) {
			KillTimer( mswHWnd, triggerTimer );
			triggerTimer = 0;
			if (triggerFunc)
				triggerFunc( triggerControl );
		} else if (wParam == BALLOONHELP_TIMER) {
			KillTimer( hWnd, balloonHelpTimer );
			balloonHelpTimer = (UINT)0;
			startBalloonHelp();
		}
		return 0L;

	case WM_MENUSELECT:
		mswAllowBalloonHelp = TRUE;
		closeBalloonHelp();
		break;

	case WM_WINDOWPOSCHANGED:
		if (hWnd == mswHWnd && !IsIconic(hWnd) && needToDoPendingShow) {
			for (w=winFirst; w; w=(wWin_p)w->next) {
				if (w->hWnd != mswHWnd &&
					w->pendingShow )
					ShowWindow( w->hWnd, SW_SHOW );
				w->pendingShow = FALSE;
			}
			needToDoPendingShow = FALSE;
		}
		break;

#ifdef CONTROL3D
	case WM_SYSCOLORCHANGE:
		Ctl3dColorChange();
		break;

#ifndef WIN32
	case WM_CTLCOLOR:
		hBrush = Ctl3dCtlColorEx( message, wParam, lParam );
		if (hBrush != (HBRUSH)0)
			return (long)(WORD)hBrush;
		else
			return DefWindowProc( hWnd, message, wParam, lParam );
#endif
#endif

	case 51:
		count51++;
		/*return NULL;*/

#ifdef LATER
	case WM_SETFOCUS:
		hDc = GetDC( hWnd );
		rc = RealizePalette( hDc );
		ReleaseDC( hWnd, hDc );
		inx = GetWindowWord( hWnd, 0 );
		if ( inx < CONTROL_BASE || inx > controlMap_da.cnt )
			break;
		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;
		if (!w)
			break;
		if (w->type != W_MAIN && w->type != W_POPUP)
			break;
		for (b=w->first; b; b=b->next) {
			if (b->hWnd && (b->type == B_BUTTON || b->type==B_DRAW)) {
				hDc = GetDC( b->hWnd );
				rc = RealizePalette( hDc );
				ReleaseDC( b->hWnd, hDc );
			}
		}
		break;
#endif

	case WM_PALETTECHANGED:
		if (wParam == (WPARAM)hWnd)
			return 0L;

	case WM_QUERYNEWPALETTE:
		if (mswPalette) {
			hDc = GetDC( hWnd );
			SelectPalette( hDc, mswPalette, 0 );
			inx = RealizePalette( hDc );
			ReleaseDC( hWnd, hDc );
			if (inx>0)
				InvalidateRect( hWnd, NULL, TRUE );
			return inx;
		}

	case WM_ACTIVATE:
		if ( LOWORD(wParam) == WA_INACTIVE )
			closeBalloonHelp();
		break;
		
	case WM_HSCROLL:
	case WM_VSCROLL:
		b = getControlFromCursor( hWnd, NULL );
		if (!b)
			break;
		/*mswSetFocus( b );*/
		ret = 0L;
		if (!inMainWndProc) {
			inMainWndProc = TRUE;
			if (mswCallBacks[b->type] != NULL &&
				mswCallBacks[b->type]->messageProc) {
				ret = mswCallBacks[b->type]->messageProc( b, hWnd, message, wParam, lParam );
			}
			inMainWndProc = FALSE;
		}
		return ret;
		
   case WM_LBUTTONDOWN:
   case WM_MOUSEMOVE:
   case WM_LBUTTONUP:
		b = getControlFromCursor( hWnd, NULL );
		if (!b)
			break;
		/*mswSetFocus( b );*/
		ret = 0L;
		if (!inMainWndProc) {
			inMainWndProc = TRUE;
			if (mswCallBacks[b->type] != NULL &&
				mswCallBacks[b->type]->messageProc) {
				ret = mswCallBacks[b->type]->messageProc( b, hWnd, message, wParam, lParam );
			}
			inMainWndProc = FALSE;
		}
		return ret;
				
	default:
		;
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}

/*
 *****************************************************************************
 *
 * INIT
 *
 *****************************************************************************
 */


static BOOL InitApplication( HINSTANCE hinstCurrent )
{
	WNDCLASS wc;

	wc.style = 0L;
	wc.lpfnWndProc = MainWndProc;

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 4;
	wc.hInstance = hinstCurrent;			
	wc.hIcon = LoadIcon( hinstCurrent, "MSWAPPICON" );
	wc.hCursor = NULL; /*LoadCursor( NULL, IDC_ARROW );*/
#ifdef CONTROL3D
	wc.hbrBackground = CreateSolidBrush( GetSysColor(COLOR_BTNFACE) );
#else
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
#endif
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "MswMainWindow";
	if (!RegisterClass(&wc)) {
		mswFail("RegisterClass(MainWindow)");
		return FALSE;
	}

	wc.style = CS_SAVEBITS;
	wc.lpfnWndProc = MainWndProc;

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 8;
	wc.hInstance = hinstCurrent;
	wc.hIcon = LoadIcon( NULL, "wAppIcon" );
	wc.hCursor = NULL; /*LoadCursor( NULL, IDC_ARROW );*/
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
	wc.lpszMenuName = "GenericMenu";
	wc.lpszClassName = "MswPopUpWindow";
	if (!RegisterClass(&wc)) {
		mswFail("RegisterClass(PopUpWindow)");
		return FALSE;
	}

	wc.style = CS_SAVEBITS;
	wc.lpfnWndProc = DefWindowProc;

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 4;
	wc.hInstance = hinstCurrent;
	wc.hIcon = 0;
	wc.hCursor = 0;
	wc.hbrBackground = CreateSolidBrush( RGB(255, 255, 0) );
	/*wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);*/
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "MswBalloonHelp";
	if (!RegisterClass(&wc)) {
		mswFail("RegisterClass(BalloonHelp)");
		return FALSE;
		}
 
	wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	wc.lpfnWndProc = mswDrawPush;
	wc.lpszClassName = mswDrawWindowClassName;
	wc.cbWndExtra = 4;
	if (!RegisterClass(&wc)) {
		mswFail("RegisterClass(drawClass)");
		return FALSE;
	}
	return TRUE;
}


HHOOK filterFunc_h;

int FAR PASCAL filterFunc( int nCode, WORD wParam, DWORD lParam )
{
	LPMSG lpmsg = (LPMSG)lParam;
	if ((nCode == MSGF_DIALOGBOX || nCode == MSGF_MENU) &&
		lpmsg->message == WM_KEYDOWN && lpmsg->wParam == VK_F1 ) {
		PostMessage(mswWin->hWnd, WM_F1DOWN, nCode, 0L );
	}
	CallNextHookEx( filterFunc_h, nCode, wParam, lParam );
	return 0;
}



int PASCAL WinMain( HINSTANCE hinstCurrent, HINSTANCE hinstPrevious, LPSTR lpszCmdLine, int nCmdShow )
{
	MSG msg;
#ifndef WIN32
	HOOKPROC filterFunc_pi;
#endif
#ifdef CONTROL3D
	BOOL rc;
#endif
	HDC hDc;
	char * argv[20], * cp;
	int argc = 0;
	TEXTMETRIC tm;
	DWORD dw;

#ifdef CONTROL3D
	rc = Ctl3dRegister( hinstCurrent );
	rc = Ctl3dAutoSubclass( hinstCurrent );
	rc = Ctl3dColorChange();
#endif

	if (!hinstPrevious)
		if (!InitApplication(hinstCurrent))
			return FALSE;

	mswHInst = hinstCurrent;

	mTitleH = GetSystemMetrics( SM_CYCAPTION ) - 1;
	mFixBorderW = GetSystemMetrics( SM_CXBORDER );
	mFixBorderH = GetSystemMetrics( SM_CYBORDER );
	mResizeBorderW = GetSystemMetrics( SM_CXFRAME );
	mResizeBorderH = GetSystemMetrics( SM_CYFRAME );
	mMenuH = GetSystemMetrics( SM_CYMENU ) + 1;
	screenWidth = GetSystemMetrics( SM_CXSCREEN );
	screenHeight = GetSystemMetrics( SM_CYSCREEN );
#ifdef WIN32
	mswLabelFont = GetStockObject( DEFAULT_GUI_FONT );
#else
	mswLabelFont = GetStockObject( ANSI_VAR_FONT );
#endif

	hDc = GetDC( 0 );
	mswScale = GetDeviceCaps( hDc, LOGPIXELSX ) / 96.0;
	if ( mswScale < 1.0 )
		mswScale = 1.0;
	GetTextMetrics( hDc, &tm );
	mswEditHeight = tm.tmHeight + 8;
	dw = GetTextExtent( hDc, "AXqypj", 6 );
	mswEditHeight = HIWORD(dw)+2;
	ReleaseDC( 0, hDc );

#ifdef HELPSTR
	helpStrF = fopen( "C:\\dave\\helpstr.lst", "w" );
#endif

	argv[argc++] = "app";
	for ( cp=(char*)lpszCmdLine; *cp; ) {
		char termChar = ' ';
		while ( *cp == ' ' ) cp++;
		if ( *cp == '"' ) {
			termChar = '"';
			cp++;
		}
		argv[argc++] = cp;
		for ( ; *cp && *cp != termChar; cp++ );
		*cp++ = 0;
	}
	mswWin = wMain( argc, (char**)argv );
	if (mswWin == NULL)
		return 0;

	balloonHelpHWnd = CreateWindow( "MswBalloonHelp", "BalloonHelp",
				WS_POPUP|WS_BORDER,
				0, 0, 80, 40, mswHWnd, NULL, mswHInst, NULL );
	if (balloonHelpHWnd == (HWND)0) {
		mswFail( "CreateWindow(BALLOONHELP)" );
	} else {
		hDc = GetDC( balloonHelpHWnd );
		SetBkColor( hDc, RGB( 255, 255, 0 ) );
		/* We need to remember this because the hDc gets changed somehow,
		/* and we when we select the oldFont back in we don't get newFont */
		balloonHelpNewFont = CreateFont( - balloonHelpFontSize, 0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0, balloonHelpFaceName );
		balloonHelpOldFont = SelectObject( hDc, balloonHelpNewFont );
		ReleaseDC( balloonHelpHWnd, hDc );
	}

#ifndef WIN32
	/* F1 works without this on WIN32 */
	filterFunc_pi = MakeProcInstance( filterFunc, hinstCurrent );
	filterFunc_h = SetWindowsHookEx( WH_MSGFILTER, filterFunc_pi, hinstCurrent, GetCurrentTask() );
#endif

	mswRedrawAll();
	SetCursor( LoadCursor( NULL, IDC_ARROW ) );
#ifdef DOTRANSACCEL
	hMswAccel = LoadAccelerators( hinstCurrent, "msw-accel" );
#endif
	while (GetMessage( &msg, NULL, 0, 0 )) {
		if (
#ifdef DOTRANSACCEL
			 (!TranslateAccelerator( mswWin->hWnd, hMswAccel, &msg )) &&
#endif
			 (!mswTranslateAccelerator( mswWin->hWnd, &msg )) ) {
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}

#ifdef CONTROL3D
	Ctl3dUnregister( hinstCurrent );
#endif


	return msg.wParam;
}

BOOL FAR PASCAL About( HWND hDlg, WORD message, WPARAM wParam, LPARAM lParam )
{
	switch (message) {
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:		
		if (WCMD_PARAM_ID == IDOK || WCMD_PARAM_ID == IDCANCEL) {
			EndDialog( hDlg,TRUE );
			return TRUE;
		}
		break;
	}
	return FALSE;
}

