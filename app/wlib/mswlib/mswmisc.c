/** \file mswmisc.c
 * Basic windows functions and main entry point for application.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2009 Martin Fischer
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

#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include <malloc.h> // required for heapinfo in function wMemStats
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <htmlhelp.h>
#include "mswint.h"
#include "i18n.h"
#include "FreeImage.h"

#if _MSC_VER > 1300
#define stricmp _stricmp
#define strnicmp _strnicmp
#define strdup _strdup
#endif

#define OFN_LONGFILENAMES		0x00200000L

char * mswStrdup(const char *);
const char * GetCurCommandName(void);

#define PAUSE_TIMER		(901)
#define ALARM_TIMER		(902)
#define BALLOONHELP_TIMER		(903)
#define TRIGGER_TIMER	(904)
#define CONTROLHILITEWIDTH (2)
#define CONTROLHILITECOLOR (RGB(0x3a,0x5f,0xcd))

#define WANT_LITTLE_LABEL_FONT

#ifndef WANT_LITTLE_LABEL_FONT
#define LABELFONTDECL
#define LABELFONTSELECT
#define LABELFONTRESET
#else
#define LABELFONTDECL	HFONT hFont;
#define LABELFONTRESET	SelectObject( hDc, hFont );
#define LABELFONTSELECT hFont = SelectObject( hDc, mswLabelFont );
#endif

/*
 * EXPORTED VARIABLES
 */

HINSTANCE mswHInst;
HWND mswHWnd = (HWND)0;

const char *mswDrawWindowClassName = "DRAWWINDOW";
char mswTmpBuff[1024];
int mswEditHeight;
int mswAllowBalloonHelp = TRUE;
HFONT mswOldTextFont;
HFONT mswLabelFont;
double mswScale = 1.0;

callBacks_t *mswCallBacks[CALLBACK_CNT];

void closeBalloonHelp(int inx);
static wControl_p getControlFromCursor(HWND, wWin_p *);

#ifdef BALLOON_TRACE
// To use:
// change logFile defn in lprintf.c from static to EXPORT
// Run with some debug flag set to ensure logFile is set
extern FILE * logFile;
#endif
/*
 * LOCAL VARIABLES
 */

struct wWin_t {
	WOBJ_COMMON
	int validGeometry;
	int min_width;
	int max_width;
	int min_height;
	int max_height;
	wWinPix_t lastX, lastY;
	wWinPix_t padX, padY;
	wControl_p first, last;
	wWinCallBack_p winProc;
	BOOL_T busy;
#ifdef OWNERICON
	HBITMAP wicon_bm;
	wWinPix_t wicon_w, wicon_h;
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

static wWin_p mswWin = NULL;
static wWin_p winFirst, winLast;

static long count51 = 0;

static UINT_PTR alarmTimer;
static UINT_PTR pauseTimer;
static UINT_PTR balloonHelpTimer = (UINT_PTR)0;
static UINT_PTR triggerTimer;

static UINT balloonHelpTimeOut = 500;
static wControl_p balloonHelpButton = NULL;
static enum { balloonHelpIdle, balloonHelpWait, balloonHelpShow } balloonHelpState
        = balloonHelpIdle;
static HWND balloonHelpHWnd = (HWND)0;
static int balloonHelpFontSize = 8;
static char balloonHelpFaceName[] = "MS Sans Serif";
static HFONT balloonHelpOldFont;
static HFONT balloonHelpNewFont;
static int balloonHelpEnable = TRUE;
static wControl_p balloonControlButton = NULL;

static BOOL_T helpInitted = FALSE;
static DWORD dwCookie;

#define CONTROL_BASE (1)
typedef struct {
	wControl_p b;
} controlMap_t;
static dynArr_t controlMap_da;
#define controlMap(N) DYNARR_N(controlMap_t,controlMap_da,N)


static char * appName;
static char * helpFile;
char *mswProfileFile;

static wBalloonHelp_t * balloonHelpStrings;

#ifdef HELPSTR
static FILE * helpStrF;
#endif
static int inMainWndProc = FALSE;

static wBool_t mswWinBlockEnabled = TRUE;

static FILE * dumpControlsF;
static int dumpControls;

extern char *userLocale;

// list of supported fileformats for image files
static char * filterImageFiles[] = { N_("All image files"),
                                     "*.gif;*.jpg;*.jpeg;*.png;*.tif;*.tiff",
                                     N_("GIF files (*.gif)"),
                                     "*.gif",
                                     N_("JPEG files (*.jpeg,*.jpg)"),
                                     "*.jpg;*.jpeg",
                                     N_("PNG files (*.png)"),
                                     "*.png",
                                     N_("TIFF files (*.tiff, *.tif)"),
                                     "*.tif;*.tiff",
                                     N_("All files (*)"),
                                     "*",
                                   };

static HICON hWindowIcon;

/*
 *****************************************************************************
 *
 * Internal Utility functions
 *
 *****************************************************************************
 */


DWORD GetTextExtent(
        HDC hDc,
        CHAR * str,
        UINT len)
{
	SIZE size;
	GetTextExtentPoint(hDc, str, len, &size);
	return size.cx + (size.cy<<16);
}


static char * controlNames[] = {
	"MAIN", "POPUP",
	"BUTTON", "STRING", "INTEGER", "FLOAT",
	"LIST", "DROPLIST", "COMBOLIST",
	"RADIO", "TOGGLE",
	"DRAW", "TEXT", "MESSAGE", "LINES",
	"MENUITEM", "CHOICEITEM", "BOX"
};

static void doDumpControls(void)
{
	wControl_p b;
	int inx;

	if (!dumpControls) {
		return;
	}

	if (!dumpControlsF) {
		dumpControlsF = fopen("controls.lst", "w");

		if (!dumpControlsF) {
			abort();
		}
	}

	for (inx=0; inx<controlMap_da.cnt-1; inx++) {
		b = controlMap(inx).b;

		if (b) {
			fprintf(dumpControlsF, "[%0.3d] [%p] %s %s %s\n", inx,
			        b->hWnd,
			        (b->type>=0&&b->type<=B_BOX?controlNames[b->type]:"NOTYPE"),
			        (b->labelStr?b->labelStr:"<NULL>"),
			        (b->helpStr?b->helpStr:"<NULL>"));
		} else {
			fprintf(dumpControlsF, "[%0.3d] <NULL>\n", inx);
		}
	}

	fflush(dumpControlsF);
	fclose(dumpControlsF);
	dumpControls = 0;
}

void mswFail(const char * where)
{
	sprintf(mswTmpBuff, "%s\n# Controls %d", where, controlMap_da.cnt);
	MessageBox(NULL, mswTmpBuff, "FAIL", MB_TASKMODAL|MB_OK);
	doDumpControls();
}
/*
static UINT curSysRes = 100;
static UINT curGdiRes = 100;
static UINT curUsrRes = 100;
static UINT curMinRes = 100;
*/

wControl_p mswMapIndex(INDEX_T inx)
{
	if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
		mswFail("mswMapIndex- bad index");
		exit(1);
	}

	return controlMap(inx-CONTROL_BASE).b;
}


void mswRepaintLabel(HWND hWnd, wControl_p b)
{
	HDC hDc;
	HBRUSH oldBrush, newBrush;
	RECT rect;
	DWORD dw;
	LABELFONTDECL

	if (b->labelStr) {
		hDc = GetDC(hWnd);
		LABELFONTSELECT
		newBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
		oldBrush = SelectObject(hDc, newBrush);
		dw = GetTextExtent(hDc, CAST_AWAY_CONST b->labelStr,
		                   (int)(strlen(b->labelStr)));
		rect.left = b->labelX;
		rect.top = b->labelY;
		rect.right = b->labelX + LOWORD(dw);
		rect.bottom = b->labelY + HIWORD(dw);
		FillRect(hDc, &rect, newBrush);
		DeleteObject(SelectObject(hDc, oldBrush));
		/*SetBkMode( hDc, OPAQUE );*/
		SetBkColor(hDc, GetSysColor(COLOR_BTNFACE));

		if (!TextOut(hDc, b->labelX, b->labelY, b->labelStr,
		             (int)(strlen(b->labelStr)))) {
			mswFail("Repainting text label");
		}

		LABELFONTRESET
		ReleaseDC(hWnd, hDc);
	}
}



int mswRegister(
        wControl_p w)
{
	int index;
	DYNARR_APPEND(controlMap_t, controlMap_da, 25);
	index = controlMap_da.cnt-1+CONTROL_BASE;
	controlMap(controlMap_da.cnt-1).b = (wControl_p)w;
	return index;
}


void mswUnregister(
        int index)
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
        int * index)
{
	wControl_p w = (wControl_p)calloc(1, size);

	if (w == NULL) {
		abort();
	}

	*index = mswRegister(w);
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
	w->hilite = FALSE;
	w->errStr = NULL;
	return w;
}


void mswComputePos(
        wControl_p b,
        wWinPix_t origX,
        wWinPix_t origY)
{
	wWin_p w = b->parent;

	if (origX >= 0) {
		b->x = origX;
	} else {
		b->x = w->lastX + (-origX) - 1;
	}

	if (origY >= 0) {
		b->y = origY;
	} else {
		b->y = w->lastY + (-origY) - 1;
	}

	b->labelX = b->x;
	b->labelY = b->y+2;

	if (b->labelStr) {
		size_t lab_l;
		HDC hDc;
		DWORD dw;
		LABELFONTDECL
		hDc = GetDC(w->hWnd);
		LABELFONTSELECT
		lab_l = strlen(b->labelStr);
		dw = GetTextExtent(hDc, CAST_AWAY_CONST b->labelStr, (UINT)lab_l);
		b->labelX -= LOWORD(dw) + 5;
		LABELFONTRESET
		ReleaseDC(w->hWnd, hDc);
	}
}

void mswAddButton(
        wControl_p b,
        BOOL_T paintLabel,
        const char * helpStr)
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
		AdjustWindowRect(&rect, w->style, (w->option&F_MENUBAR)?1:0);
		rect.bottom += mFixBorderH;

		if (!SetWindowPos(w->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
		                  rect.right-rect.left, rect.bottom-rect.top,
		                  SWP_NOMOVE)) {
			mswFail("SetWindowPos");
		}

		w->busy = FALSE;
	}

	if (paintLabel) {
		mswRepaintLabel(w->hWnd, (wControl_p)b);
	}

	if (helpStr == NULL) {
		return;
	}

	b->helpStr = mswStrdup(helpStr);
#ifdef HELPSTR

	if (helpStrF) {
		fprintf(helpStrF, "HELPSTR - %s\n", helpStr?helpStr:"<>");
	}

#endif
}


void mswResize(
        wWin_p w)
{
	wControl_p b;
	RECT rect;
	w->lastX = 0;
	w->lastY = 0;

	for (b=w->first; b; b=b->next) {
		if (w->lastX < (b->x + b->w)) {
			w->lastX = b->x + b->w;
		}

		if (w->lastY < (b->y + b->h)) {
			w->lastY = b->y + b->h;
		}
	}

	if (w->option&F_AUTOSIZE) {
		w->w = w->lastX;
		w->h = w->lastY;
		w->busy = TRUE;
		rect.left = 0;
		rect.top = 0;
		rect.right = w->w + w->padX;
		rect.bottom = w->h + w->padY;
		AdjustWindowRect(&rect, w->style, (w->option&F_MENUBAR)?1:0);
		rect.bottom += mFixBorderH;

		if (!SetWindowPos(w->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
		                  rect.right-rect.left, rect.bottom-rect.top,
		                  SWP_NOMOVE|SWP_NOZORDER)) {
			mswFail("SetWindowPos");
		}

		w->busy = FALSE;
	}
}



void mswChainFocus(
        wControl_p b)
{
	wWin_p w;
	w = b->parent;

	if (w->option&F_NOTAB) {
		return;
	}

	if (b->option&BO_NOTAB) {
		return;
	}

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
        wControl_p b)
{
	if (b && b->type != B_MENUITEM && b->focusChainNext) {
		b->parent->focusChainNext = b;
	}
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
        wWinPix_t *rw,
        wWinPix_t *rh,
        wWinPix_t *rx,
        wWinPix_t *ry,
        int *showCmd)
{
	char *cq;
	*showCmd = SW_SHOWNORMAL;

	if ((option&F_RECALLPOS) && nameStr) {
		int x, y, w, h;
		int xadj, yadj;
		const char *cp;
		int state;

		w = h = 0;
		xadj = 1;
		yadj = mTitleH + 1;
		if (option & F_RESIZE) {
			xadj += mResizeBorderW * 2;
			yadj += mResizeBorderH * 2;
		} else {
			xadj += mFixBorderW * 2;
			yadj += mFixBorderH * 2;
		}
		//if (option & F_MENUBAR) {
		//	yadj += mMenuH;
		//}

		if ((option & F_RESIZE) &&
		    (cp = wPrefGetStringBasic("msw window size", nameStr)) &&
		    (state = (int)strtol(cp, &cq, 10), cp != cq) &&  // state is not used
		    (cp = cq, w = (wWinPix_t)(strtod(cp, &cq)), cp != cq) &&
		    (cp = cq, h = (wWinPix_t)(strtod(cp, &cq)), cp != cq)
		   ) {
			if (w < 10) {
				w = 10;
			}

			if (h < 10) {
				h = 10;
			}

			if (w > screenWidth - xadj) {
				w = screenWidth - xadj;
			}

			if (h > screenHeight - yadj) {
				h = screenHeight - yadj;
			}

			*rw = w;
			*rh = h;
		}

		if ((cp = wPrefGetStringBasic("msw window pos", nameStr)) &&
		    (x = (wWinPix_t)(strtod(cp, &cq)), cp != cq) &&
		    (cp = cq, y = (wWinPix_t)(strtod(cp, &cq)), cp != cq)
		   ) {
			if (y < 0) {
				y = 0;
			}

			if (x < 0) {
				x = 0;
			}

			// Make sure we can see the dialog
			xadj += 100;
			yadj += 100;


			if (y + h > screenHeight - yadj) {
				y = screenHeight - yadj - h;
			}

			if (x + w > screenWidth - xadj) {
				x = screenWidth - xadj - w;
			}

			*rx = x;
			*ry = y;
		}
	}
}

/**
 * Set min and max dimensions for a window.
 *
 * \param min_width IN minimum width of window
 * \param max_width IN maximum width of window
 * \param min_height IN minimum height of window
 * \param max_height IN maximum height of window
 * \param base_width IN unused on Windows
 * \param base_height IN unused on Windows
 * \param aspect_ration IN unused on Windows
 */
void wSetGeometry(wWin_p win,
                  wWinPix_t min_width,
                  wWinPix_t max_width,
                  wWinPix_t min_height,
                  wWinPix_t max_height,
                  wWinPix_t base_width,
                  wWinPix_t base_height,
                  double aspect_ratio)
{
	win->validGeometry = TRUE;	//remember that geometry was set
	win->min_width = min_width;
	win->max_width = max_width;
	win->min_height = min_height;
	win->max_height = max_height;

	return;
}

/**
 * Create a window. Retrieves the saved size and position and restores the created window accordingly.
 *
 * \param hWnd IN parent window
 * \param typ  IN type of window (W_MAIN or W_POPUP)
 * \param option IN options for window creation
 * \param classname IN pre-registered window class
 * \param style    IN
 * \param labelStr IN window title
 * \param winProc IN callback procedure
 * \param w IN default window width
 * \param h IN default window height
 * \param data IN ??
 * \param nameStr IN name of window
 * \param pShowCmd IN/OUT window show option (maximize or show normal)
 * \return    window data structure
 */

static wWin_p winCommonCreate(
        HWND hWnd,
        int typ,
        long option,
        const char * className,
        long style,
        const char * labelStr,
        wWinCallBack_p winProc,
        wWinPix_t w,
        wWinPix_t h,
        void * data,
        const char * nameStr,
        int * pShowCmd)
{
	wWin_p win;
	int index;
	wWinPix_t ww, hh, xx, yy;
	RECT rect;
	win = (wWin_p)mswAlloc(NULL, typ, mswStrdup(labelStr), sizeof *win, data,
	                       &index);
	win->option = option;
	win->first = win->last = NULL;
	win->lastX = 0;
	win->lastY = 0;
	win->winProc = winProc;
	win->centerWin = TRUE;
	win->modalLevel = 0;
	win->busy = TRUE;
	ww = hh = xx = yy = CW_USEDEFAULT;
	getSavedSizeAndPos(option, nameStr, &ww, &hh, &xx, &yy, pShowCmd);

	if (typ == W_MAIN) {
		*pShowCmd = ((option & F_MAXIMIZE) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
	} else {
		*pShowCmd = SW_HIDE;    //Make sure to hide first
	}

	if (xx != CW_USEDEFAULT) {
		win->centerWin = FALSE;
	}

	if (option & F_RESIZE) {
		style |= WS_THICKFRAME;

		if (ww != CW_USEDEFAULT) {
			w = ww;
			h = hh;
			option &= ~F_AUTOSIZE;
			win->option = option;
		}
	}

	if (option & F_AUTOSIZE) {
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
	AdjustWindowRect(&rect, win->style, (win->option&F_MENUBAR)?1:0);
	rect.bottom += mFixBorderH;
	win->hWnd = CreateWindow(className, labelStr, style,
	                         xx, yy,
	                         rect.right-rect.left, rect.bottom-rect.top,
	                         hWnd, NULL,
	                         mswHInst, NULL);

	if (win->hWnd == (HWND)0) {
		mswFail("CreateWindow(POPUP)");
	} else {
		SetWindowWord(win->hWnd, 0, (WORD)index);
	}

	ShowWindow(win->hWnd, *pShowCmd);
	win->baseStyle = WS_GROUP;
	win->focusChainFirst = win->focusChainLast = win->focusChainNext = NULL;

	if (winFirst == NULL) {
		winFirst = winLast = win;
	} else {
		winLast->next = (wControl_p)win;
		winLast = win;
	}

	win->nameStr = mswStrdup(nameStr);

	//HICON hIcon1 = LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(101), IMAGE_ICON, 32, 32, LR_DEFAULTSIZE);
	SendMessage(win->hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hWindowIcon);
	SendMessage(win->hWnd, WM_SETICON, ICON_BIG, (LPARAM)hWindowIcon);

	if (typ == W_MAIN) {
		mswInitColorPalette();
	}

	return win;
}

void wInitAppName(char *_appName)
{
	appName = (char *)malloc(strlen(_appName) + 1);
	strcpy(appName, _appName);
}


/**
 * Initialize the application's main window. This function does the necessary initialization
 * of the application including creation of the main window.
 *
 * \param name IN internal name of the application. Used for filenames etc.
 * \param x    IN default width
 * \param y    IN default height
 * \param helpStr IN ??
 * \param labelStr IN window title
 * \param nameStr IN ??
 * \param option IN options for window creation
 * \param winProc IN pointer to main window procedure
 * \param data IN ??
 * \return    window handle or NULL on error
 */

wWin_p wWinMainCreate(
        const char * name,
        wWinPix_t x,
        wWinPix_t y,
        const char * helpStr,
        const char * labelStr,
        const char * nameStr,
        long option,
        wWinCallBack_p winProc,
        void * data)
{
	wWin_p w;
	const char * appDir;
	const char * libDir;
	int showCmd;
	int error;
	HDC hDc;
	TEXTMETRIC tm;
	char *pos;
	char * configName;
	long maximize;

	/* check for configuration name */
	if (pos = strchr(name, ';')) {
		/* if found, split application name and configuration name */
		configName = (char *)malloc(strlen(name) + 1);
		strcpy(configName, pos + 1);
	} else {
		/* if not found, application name and configuration name are same */
		configName  = (char*)malloc(strlen(name)+1);
		strcpy(configName, name);
	}

	appDir = wGetAppWorkDir();

	if (appDir == NULL) {
		free(configName);
		return NULL;
	}

	mswProfileFile = (char*)malloc(strlen(appDir)+1+strlen(configName)+1+3+1);
	wsprintf(mswProfileFile, "%s\\%s.ini", appDir, configName);
	free(configName);
	error = WritePrivateProfileString("mswtest", "test", "ok", mswProfileFile);

	if (error <= 0) {
		sprintf(mswTmpBuff,
		        "Can not write to %s.\nPlease make sure the directory exists and is writable",
		        mswProfileFile);
		wNoticeEx(NT_ERROR, mswTmpBuff, "Ok", NULL);
		return NULL;
	}

	libDir = wGetAppLibDir();
	/* length of path + \ + length of filename + . + length of extension + \0 */
	helpFile = (char*)malloc(strlen(libDir) + 1 + strlen(appName) + 1 + 3 + 1);
	wsprintf(helpFile, "%s\\%s.chm", libDir, appName);

	wPrefGetInteger("draw", "maximized", &maximize, 0L);
	option |= (maximize ? F_MAXIMIZE : 0);

	showCmd = SW_SHOW;
	w = winCommonCreate(NULL, W_MAIN, option|F_RESIZE, "MswMainWindow",
	                    WS_OVERLAPPEDWINDOW, labelStr, winProc, x, y, data,
	                    nameStr, &showCmd);
	mswHWnd = w->hWnd;

	SendMessage(mswHWnd, WM_SETFONT, (WPARAM)mswLabelFont, (LPARAM)0);
	hDc = GetDC(mswHWnd);
	GetTextMetrics(hDc, &tm);
	mswEditHeight = tm.tmHeight+2;
	ReleaseDC(mswHWnd, hDc);

	ShowWindow(mswHWnd, showCmd);
	UpdateWindow(mswHWnd);
	w->busy = FALSE;
	return w;
}

wWin_p wWinPopupCreate(
        wWin_p parent,
        wWinPix_t x,
        wWinPix_t y,
        const char * helpStr,
        const char * labelStr,
        const char * nameStr,
        long option,
        wWinCallBack_p winProc,
        void * data)
{
	wWin_p w;
	DWORD style;
	HMENU sysMenu;
	int showCmd;
	static DWORD popup = WS_POPUP;
	style = popup;
	style |= WS_BORDER | WS_CAPTION | WS_SYSMENU;
	w = winCommonCreate(parent?parent->hWnd:mswHWnd, W_POPUP, option,
	                    "MswPopUpWindow",
	                    style, labelStr, winProc, x, y, data, nameStr, &showCmd);
	w->helpStr = mswStrdup(helpStr);
	sysMenu = GetSystemMenu(w->hWnd, FALSE);

	if (sysMenu) {
		DeleteMenu(sysMenu, SC_RESTORE, MF_BYCOMMAND);
		/*DeleteMenu( sysMenu, SC_MOVE, MF_BYCOMMAND );*/
		/*DeleteMenu( sysMenu, SC_SIZE, MF_BYCOMMAND );*/
		DeleteMenu(sysMenu, SC_MINIMIZE, MF_BYCOMMAND);
		DeleteMenu(sysMenu, SC_MAXIMIZE, MF_BYCOMMAND);
		DeleteMenu(sysMenu, SC_TASKLIST, MF_BYCOMMAND);
		DeleteMenu(sysMenu, 4, MF_BYPOSITION);
	}

	w->busy = FALSE;
	return w;
}

void wWinSetBigIcon(
        wWin_p win,
        wIcon_p bm)
{
#ifdef OWNERICON
	win->wicon_w = bm->w;
	win->wicon_h = bm->h;
	win->wicon_bm = mswCreateBitMap(
	                        GetSysColor(COLOR_BTNTEXT), RGB(255, 255, 255), RGB(255, 255, 255),
	                        bm->w, bm->h, bm->bits);
#endif
}


void wWinSetSmallIcon(
        wWin_p win,
        wIcon_p bm)
{
#ifdef OWNERICON
	win->wicon_w = bm->w;
	win->wicon_h = bm->h;
	win->wicon_bm = mswCreateBitMap(
	                        GetSysColor(COLOR_BTNTEXT), RGB(255, 255, 255), RGB(255, 255, 255),
	                        bm->w, bm->h, bm->bits);
#endif
}


void wWinTop(
        wWin_p win)
{
	/*BringWindowToTop( win->hWnd );*/
	SetWindowPos(win->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	SetFocus(win->hWnd);
}


DWORD mswGetBaseStyle(wWin_p win)
{
	DWORD style;
	style = win->baseStyle;
	win->baseStyle = 0;
	return style;
}


static wAccelKey_e translateExtKey(UINT wParam)
{
	wAccelKey_e extChar;
	extChar = wAccelKey_None;

	switch (wParam) {
	case VK_DELETE:
		extChar = wAccelKey_Del;
		break;

	case VK_INSERT:
		extChar = wAccelKey_Ins;
		break;

	case VK_HOME:
		extChar = wAccelKey_Home;
		break;

	case VK_END:
		extChar = wAccelKey_End;
		break;

	case VK_PRIOR:
		extChar = wAccelKey_Pgup;
		break;

	case VK_NEXT:
		extChar = wAccelKey_Pgdn;
		break;

	case VK_UP:
		extChar = wAccelKey_Up;
		break;

	case VK_DOWN:
		extChar = wAccelKey_Down;
		break;

	case VK_RIGHT:
		extChar = wAccelKey_Right;
		break;

	case VK_LEFT:
		extChar = wAccelKey_Left;
		break;

	case VK_BACK:
		extChar = wAccelKey_Back;
		break;

	/*case VK_F1:	extChar = wAccelKey_F1; break;*/
	case VK_F2:
		extChar = wAccelKey_F2;
		break;

	case VK_F3:
		extChar = wAccelKey_F3;
		break;

	case VK_F4:
		extChar = wAccelKey_F4;
		break;

	case VK_F5:
		extChar = wAccelKey_F5;
		break;

	case VK_F6:
		extChar = wAccelKey_F6;
		break;

	case VK_F7:
		extChar = wAccelKey_F7;
		break;

	case VK_F8:
		extChar = wAccelKey_F8;
		break;

	case VK_F9:
		extChar = wAccelKey_F9;
		break;

	case VK_F10:
		extChar = wAccelKey_F10;
		break;

	case VK_F11:
		extChar = wAccelKey_F11;
		break;

	case VK_F12:
		extChar = wAccelKey_F12;
		break;
	}

	return extChar;
}


static long notModKey;
int mswTranslateAccelerator(
        HWND hWnd,
        LPMSG pMsg)
{
	long acclKey;
	long state;
	wWin_p win;
	wControl_p b;

	if (pMsg->message != WM_KEYDOWN) {
		return FALSE;
	}

	acclKey = (long)pMsg->wParam;
	b = getControlFromCursor(pMsg->hwnd, &win);

	if (win == NULL) {
		return 0;
	}

	if (b != NULL) {
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

	if (acclKey == (long)VK_F1) {
		closeBalloonHelp(1);

		if (!b && win) {
			wHelp(win->helpStr);
		} else {
			if (b->helpStr) {
				wHelp(b->helpStr);
			} else if (b->parent) {
				wHelp(b->parent->nameStr);
			}
		}

		return 1;
	}

	/*acclKey = translateExtKey( (WORD)acclKey );*/
	state = 0;

	if (GetKeyState(VK_CONTROL) & 0x1000) {
		state |= WKEY_CTRL;
	}

	if (GetKeyState(VK_MENU) & 0x1000) {
		state |= WKEY_ALT;
	}

	if (GetKeyState(VK_SHIFT) & 0x1000) {
		state |= WKEY_SHIFT;
	}

	state <<= 8;
	acclKey |= state;

	if (pMsg->wParam > 0x12) {
		notModKey = TRUE;
	}

	return mswMenuAccelerator(win, acclKey);
}

/*
 ******************************************************************************
 *
 * Window Utilities
 *
 ******************************************************************************
 */



void wGetDisplaySize(wWinPix_t * width, wWinPix_t * height)
{
	*width = screenWidth;
	*height = screenHeight;
}


void wWinGetSize(wWin_p w, wWinPix_t * width, wWinPix_t * height)
{
	RECT rect;
	GetWindowRect(w->hWnd, &rect);
	GetClientRect(w->hWnd, &rect);
	w->w = rect.right - rect.left;
	w->h = rect.bottom - rect.top;
	*width = w->w;
	*height = w->h;
}


void wWinSetSize(wWin_p w, wWinPix_t width, wWinPix_t height)
{
	RECT rect;
	w->w = width;
	w->h = height;
	rect.left = 0;
	rect.top = 0;
	rect.right = w->w /*+w->padX*/;
	rect.bottom = w->h /*+w->padY*/;
	AdjustWindowRect(&rect, w->style, (w->option&F_MENUBAR)?1:0);
	rect.bottom += mFixBorderH;

	if (!SetWindowPos(w->hWnd, HWND_TOP, CW_USEDEFAULT, CW_USEDEFAULT,
	                  rect.right-rect.left, rect.bottom-rect.top,
	                  SWP_NOMOVE|SWP_NOZORDER)) {
		mswFail("wWinSetSize");
	}

	InvalidateRect(w->hWnd, NULL, TRUE);
}


static int blocking;
static void blockingLoop(void)
{
	MSG msg;
	int myBlocking=blocking;

	while (blocking>=myBlocking && GetMessage(&msg, NULL, 0, 0)) {
		if (
#ifdef DOTRANSACCEL
		        (!TranslateAccelerator(mswWin->hWnd, hMswAccel, &msg)) &&
#endif
		        (!mswTranslateAccelerator(mswWin->hWnd, &msg))) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}


static void savePos(wWin_p win)
{
	wWinPix_t w, h;
	RECT rect;

	if (win->nameStr &&
	    IsWindowVisible(win->hWnd) /*&& !IsIconic( win->hWnd )*/) {
		GetWindowRect(win->hWnd, &rect);

		if (win->option&F_RECALLPOS) {
			char posStr[20];
			wsprintf(posStr, "%d %d",
			         rect.left,
			         rect.top);
			wPrefSetString("msw window pos", win->nameStr, posStr);

			if (win->option&F_RESIZE) {
				GetClientRect(win->hWnd, &rect);
				w = rect.right - rect.left;
				h = rect.bottom - rect.top;
				wsprintf(posStr, "%d %d %d",
				         0,						// unused
				         w, h);
				wPrefSetString("msw window size", win->nameStr, posStr);
			}
		}
	}
}


void wWinShow(
        wWin_p win,
        BOOL_T show)
{
	wWinPix_t x, y;
	wWin_p win1;
	win->busy = TRUE;

	if (show) {
		if (win->centerWin) {
			x = (screenWidth-win->w)/2;
			y = (screenHeight-win->h)/2;

			if (x<0) {
				y = 0;
			}

			if (x<0) {
				y = 0;
			}

			if (!SetWindowPos(win->hWnd, HWND_TOP, x, y,
			                  CW_USEDEFAULT, CW_USEDEFAULT,
			                  SWP_NOSIZE|SWP_NOZORDER)) {
				mswFail("wWinShow:SetWindowPos()");
			}
		}

		win->centerWin = FALSE;
		win->shown = TRUE;

		// Clear hilites
		for (wControl_p controlP = win->first; controlP; controlP = controlP->next) {
			controlP->hilite = FALSE;
		}

		if (mswHWnd == (HWND)0 || !IsIconic(mswHWnd)) {
			ShowWindow(win->hWnd, SW_SHOW);

			if (win->focusChainFirst) {
				SetFocus(win->focusChainFirst->hWnd);
			}

			win->pendingShow = FALSE;

			if (mswWinBlockEnabled && (win->option & F_BLOCK)) {
				blocking++;
				inMainWndProc = FALSE;

				for (win1 = winFirst; win1; win1=(wWin_p)win1->next) {
					if (win1->shown && win1 != win) {
						if (win1->modalLevel == 0) {
							EnableWindow(win1->hWnd, FALSE);
						}

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
		savePos(win);
		ShowWindow(win->hWnd, SW_HIDE);

		/*SetWindowPos( win->hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_HIDEWINDOW );*/
		if (mswWinBlockEnabled && (win->option & F_BLOCK)) {
			blocking--;

			for (win1 = winFirst; win1; win1=(wWin_p)win1->next) {
				if (win1->shown && win1 != win) {
					if (win1->modalLevel > 0) {
						win1->modalLevel--;
					}

					if (win1->modalLevel == 0) {
						EnableWindow(win1->hWnd, TRUE);
					}
				}
			}
		}

		savePos(win);
		win->pendingShow = FALSE;
		win->shown = FALSE;
	}

	win->busy = FALSE;
}


void wWinBlockEnable(
        wBool_t enabled)
{
	mswWinBlockEnabled = enabled;
}


wBool_t wWinIsVisible(
        wWin_p w)
{
	return IsWindowVisible(w->hWnd);
}

/**
* Returns whether the window is maximized.
*
* \param win IN window
* \return    TRUE if maximized, FALSE otherwise
*/

wBool_t wWinIsMaximized(wWin_p w)
{
	return (IsZoomed(w->hWnd));
}

void wWinSetTitle(
        wWin_p w,
        const char * title)
{
	SetWindowText(w->hWnd, title);
}


void wWinSetBusy(
        wWin_p w,
        BOOL_T busy)
{
	HMENU menuH;
	UINT uEnable;
	wControl_p b;
	w->isBusy = busy;
	menuH = GetMenu(w->hWnd);

	if (menuH) {
		int cnt, inx;
		uEnable = MF_BYPOSITION|(busy?MF_DISABLED:MF_ENABLED);
		cnt = GetMenuItemCount(menuH);

		for (inx=0; inx<cnt; inx++) {
			EnableMenuItem(menuH, inx, uEnable);
		}
	}

	for (b=w->first; b; b=b->next) {
		if ((b->option&BO_DISABLED)==0) {
			if (mswCallBacks[b->type] != NULL &&
			    mswCallBacks[b->type]->setBusyProc) {
				mswCallBacks[b->type]->setBusyProc(b, busy);
			} else {
				if (b->hWnd) {
					EnableWindow(b->hWnd, (BOOL)!busy);
				}
			}
		}
	}
}


const char * wWinGetTitle(
        wWin_p w)
{
	return w->labelStr;
}


void wWinClear(
        wWin_p win,
        wWinPix_t x,
        wWinPix_t y,
        wWinPix_t width,
        wWinPix_t height)
{
}


void wWinDoCancel(wWin_p win)
{
	wControl_p b;

	for (b=win->first; b; b=b->next) {
		if ((b->type == B_BUTTON) && (b->option & BB_CANCEL)) {
			mswButtPush(b);
		}
	}
}

unsigned long wGetTimer(void)
{
	return (unsigned long)GetTickCount();
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
        const char * help)
{
	if (b->helpStr) {
		free(CAST_AWAY_CONST b->helpStr);
	}

	if (help) {
		b->helpStr = mswStrdup(help);
	} else {
		b->helpStr = NULL;
	}
}


/**
 * Add control to circular list of synonymous controls. Synonymous controls are kept in sync by
 * calling wControlLinkedActive for one member of the list
 *
 * \param IN  first control
 * \param IN  second control
 * \return    none
 */

void wControlLinkedSet(wControl_p b1, wControl_p b2)
{
	b2->synonym = b1->synonym;

	if (b2->synonym == NULL) {
		b2->synonym = b1;
	}

	b1->synonym = b2;
}

/**
 * Activate/deactivate a group of synonymous controls.
 *
 * \param IN  control
 * \param IN  state
 * \return    none
 */

void wControlLinkedActive(wControl_p b, int active)
{
	wControl_p savePtr = b;

	if (savePtr->type == B_MENUITEM) {
		wMenuPushEnable((wMenuPush_p)savePtr, active);
	} else {
		wControlActive(savePtr, active);
	}

	savePtr = savePtr->synonym;

	while (savePtr && savePtr != b) {
		if (savePtr->type == B_MENUITEM) {
			wMenuPushEnable((wMenuPush_p)savePtr, active);
		} else {
			wControlActive(savePtr, active);
		}

		savePtr = savePtr->synonym;
	}
}

void wControlShow(wControl_p b, BOOL_T show)
{
	RECT rc;

	if (show) {
		if (mswCallBacks[b->type] != NULL &&
		    mswCallBacks[b->type]->repaintProc) {
			mswCallBacks[b->type]->repaintProc(b->parent->hWnd, b);
		}
	} else {
		if (b->labelStr) {
			rc.left = b->labelX;
			rc.right = b->x;
			rc.top = b->labelY;
			rc.bottom = b->labelY+b->h;
			InvalidateRect(((wControl_p)b->parent)->hWnd, &rc, TRUE);
		}
	}

	if (mswCallBacks[b->type] != NULL &&
	    mswCallBacks[b->type]->showProc) {
		mswCallBacks[b->type]->showProc(b, show);
	} else {
		ShowWindow(b->hWnd, show?SW_SHOW:SW_HIDE);
#ifdef SHOW_DOES_SETFOCUS

		if (show && (b->option&BO_READONLY)==0 && b->hWnd != GetFocus()) {
			hWnd = SetFocus(b->hWnd);
		}

#endif
	}

	b->shown = show;
}


void wControlSetFocus(
        wControl_p b)
{
	if (b->hWnd) {
		SetFocus(b->hWnd);
	}
}


void wControlActive(
        wControl_p b,
        int active)
{
	if (active) {
		b->option &= ~BO_DISABLED;
	} else {
		b->option |= BO_DISABLED;
	}

	if (b->parent->isBusy) {
		return;
	}

	if (mswCallBacks[b->type] != NULL &&
	    mswCallBacks[b->type]->setBusyProc) {
		mswCallBacks[b->type]->setBusyProc(b, !active);
	} else {
		EnableWindow(b->hWnd, (BOOL)active);
		InvalidateRect(b->hWnd, NULL, TRUE);
	}
}


const char * wControlGetHelp(wControl_p b)
{
	return b->helpStr;
}


wWinPix_t wLabelWidth(const char * labelStr)
{
	size_t lab_l;
	HDC hDc;
	DWORD dw;
	LABELFONTDECL
	hDc = GetDC(mswHWnd);
	lab_l = strlen(labelStr);
	LABELFONTSELECT
	dw = GetTextExtent(hDc, CAST_AWAY_CONST labelStr, (UINT)lab_l);
	LABELFONTRESET
	ReleaseDC(mswHWnd, hDc);
	return LOWORD(dw) + 5;
}


wWinPix_t wControlGetWidth(
        wControl_p b)			/* Control */
{
	return b->w;
}


wWinPix_t wControlGetHeight(
        wControl_p b)			/* Control */
{
	return b->h;
}


wWinPix_t wControlGetPosX(
        wControl_p b)			/* Control */
{
	return b->x;
}


wWinPix_t wControlGetPosY(
        wControl_p b)			/* Control */
{
	return b->y;
}


void wControlSetPos(
        wControl_p b,
        wWinPix_t x,
        wWinPix_t y)
{
	b->labelX = x;
	b->labelY = y+2;

	if (b->labelStr) {
		size_t lab_l;
		HDC hDc;
		DWORD dw;
		LABELFONTDECL
		hDc = GetDC(b->parent->hWnd);
		LABELFONTSELECT
		lab_l = strlen(b->labelStr);
		dw = GetTextExtent(hDc, CAST_AWAY_CONST b->labelStr, (UINT)lab_l);
		b->labelX -= LOWORD(dw) + 5;
		LABELFONTRESET
		ReleaseDC(b->parent->hWnd, hDc);
	}

	if (mswCallBacks[b->type] != NULL &&
	    mswCallBacks[b->type]->setPosProc) {
		mswCallBacks[b->type]->setPosProc(b, x, y);
	} else {
		b->x = x;
		b->y = y;

		if (b->hWnd)
			if (!SetWindowPos(b->hWnd, HWND_TOP, x, y,
			                  CW_USEDEFAULT, CW_USEDEFAULT,
			                  SWP_NOSIZE|SWP_NOZORDER)) {
				mswFail("wControlSetPos");
			}
	}
}


void wControlSetLabel(
        wControl_p b,
        const char * labelStr)
{
	if (b->type == B_RADIO ) {
		;
	} else {
		size_t lab_l;
		HDC hDc;
		DWORD dw;
		LABELFONTDECL
		hDc = GetDC(b->parent->hWnd);
		lab_l = strlen(labelStr);
		LABELFONTSELECT
		dw = GetTextExtent(hDc, CAST_AWAY_CONST labelStr, (UINT)lab_l);
		LABELFONTRESET
		b->labelX = b->x - LOWORD(dw) - 5;
		ReleaseDC(b->parent->hWnd, hDc);
		b->labelStr = mswStrdup(labelStr);

		if (b->type == B_BUTTON) {
			SetWindowText(b->hWnd, labelStr);
		}
	}
}


void wControlSetContext(
        wControl_p b,
        void * context)
{
	b->data = context;
}

void wControlHilite(
        wControl_p b,
        wBool_t hilite)
{
	HDC hDc;
	HPEN oldPen, newPen;
	int oldMode;
	LOGBRUSH logBrush = { BS_SOLID, CONTROLHILITECOLOR, (ULONG_PTR)NULL };

	if (b == NULL) {
		return;
	}

	if ((b->parent==NULL) || (!IsWindowVisible(b->parent->hWnd))
	    || (!IsWindowVisible(b->hWnd))) {
		b->hilite = FALSE;
		return;
	}

	if (b->hilite == hilite) {
		return;
	}
	b->hilite = hilite;

	hDc = GetDC(b->parent->hWnd);
	newPen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_BEVEL,
	                      CONTROLHILITEWIDTH,
	                      &logBrush,
	                      0,
	                      NULL);
	oldPen = SelectObject(hDc, newPen);
	oldMode = SetROP2(hDc, R2_NOTXORPEN);
	Rectangle(hDc,
	          b->x - CONTROLHILITEWIDTH - 1,
	          b->y - CONTROLHILITEWIDTH - 1,
	          b->x + b->w + CONTROLHILITEWIDTH + 1,
	          b->y + b->h + CONTROLHILITEWIDTH + 1);
	SetROP2(hDc, oldMode);
	SelectObject(hDc, oldPen);
	DeleteObject(newPen);
	ReleaseDC(b->parent->hWnd, hDc);
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
        int beep)
{
	HDC hDc;
	int oldRop;
	wWinPix_t h;
	RECT rect;
	LABELFONTDECL

	if (beep) {
		MessageBeep(0);
	}

	GetClientRect(w->hWnd, &rect);
	hDc = GetDC(w->hWnd);
	oldRop = SetROP2(hDc, R2_WHITE);
	h = w->h+2;
	Rectangle(hDc, 0, h, w->w, h);
	SetROP2(hDc, oldRop);
	LABELFONTSELECT
	TextOut(hDc, 0, h, msg, (int)(strlen(msg)));
	LABELFONTRESET
	ReleaseDC(w->hWnd, hDc);
}

/**
 * Open a document using an external application
 *
 * \param file
 * \return TRUE on success, FALSE on error
 *
 */
unsigned wOpenFileExternal(char *file)
{
	HINSTANCE res;

	res = ShellExecute(mswHWnd, "open", file, NULL, NULL, SW_SHOW);

	if ((UINT_PTR)res <= 32) {
		wNoticeEx(NT_ERROR, "Error when opening file!", "Cancel", NULL);
		return(FALSE);
	}

	return(TRUE);
}

void wExit(int rc)
{
	INDEX_T inx;
	wControl_p b;
	mswPutCustomColors();

	for (inx=controlMap_da.cnt-1; inx>=0; inx--) {
		b = controlMap(inx).b;

		if (b != NULL) {
			if (b->type == W_MAIN || b->type == W_POPUP) {
				wWin_p w = (wWin_p)b;
				savePos(w);

				if (w->winProc != NULL) {
					w->winProc(w, wQuit_e, NULL, w->data);
				}
			}
		}
	}

	wPrefFlush("");

	for (inx=controlMap_da.cnt-1; inx>=0; inx--) {
		b = controlMap(inx).b;

		if (b != NULL) {
			if (mswCallBacks[b->type] != NULL &&
			    mswCallBacks[b->type]->doneProc != NULL) {
				mswCallBacks[b->type]->doneProc(b);
			}
		}

		controlMap(inx).b = NULL;
	}

	deleteBitmaps();

	if (mswOldTextFont != (HFONT)0) {
		DeleteObject(mswOldTextFont);
	}

	if (helpInitted) {
		WinHelp(mswHWnd, helpFile, HELP_QUIT, (DWORD)0);
		helpInitted = FALSE;
	}

	if (balloonHelpHWnd) {
		HDC hDc;
		hDc = GetDC(balloonHelpHWnd);
		SelectObject(hDc, balloonHelpOldFont);
		DeleteObject(balloonHelpNewFont);
		ReleaseDC(balloonHelpHWnd, hDc);
	}

#ifdef HELPSTR
	fclose(helpStrF);
#endif
	DestroyWindow(mswHWnd);

	if (mswPalette) {
		DeleteObject(mswPalette);
		/*DeleteObject( mswPrintPalette );*/
	}
}


void wFlush(
        void)
{
	wWin_p win;
	inMainWndProc = FALSE;
	mswRepaintAll();

	for (win=winFirst; win; win=(wWin_p)win->next) {
		UpdateWindow(win->hWnd);
	}
}

void wUpdate(
        wWin_p	win)
{
	UpdateWindow(win->hWnd);
}

static wBool_t paused;
static wAlarmCallBack_p alarmFunc;
static setTriggerCallback_p triggerFunc;
static wControl_p triggerControl;

/**
 * Wait until the pause timer expires. During that time, the message loop is
 * handled and queued messages are processed
 */

static void pausedLoop(void)
{
	MSG msg;

	while (paused && GetMessage(&msg, NULL, 0, 0)) {
		if ((mswWin) && (!mswTranslateAccelerator(mswWin->hWnd, &msg))) {
			TranslateMessage(&msg);
		}

		DispatchMessage(&msg);
	}
}

/**
 *	Timer callback function for the pause timer. The only purpose of this
 *  timer proc is to clear the waiting flag and kill the timer itself.
 */
void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	if (idEvent == PAUSE_TIMER) {
		paused = FALSE;
		KillTimer(hWnd, PAUSE_TIMER);
	}
}

/**
 * Pause the application for a specified time.
 */

void wPause(long msec)
{
	paused = TRUE;

	if (msec > 65000L) {
		msec = 65000L;
	}

	pauseTimer = SetTimer(mswHWnd, PAUSE_TIMER, (UINT)msec, TimerProc);

	if (pauseTimer == 0) {
		mswFail("wPause: No timers");
	} else {
		pausedLoop();
	}
}


void wAlarm(
        long msec,
        wAlarmCallBack_p func)
{
	alarmFunc = func;

	if (msec > 65000L) {
		msec = 65000L;
	}

	alarmTimer = SetTimer(mswHWnd, ALARM_TIMER, (UINT)msec, NULL);

	if (alarmTimer == 0) {
		mswFail("wAlarm: No timers");
	}
}


void mswSetTrigger(
        wControl_p control,
        setTriggerCallback_p func)
{
	UINT msec = (UINT)500;
	triggerControl = control;
	triggerFunc = func;

	if (func == NULL && triggerTimer != 0) {
		KillTimer(mswHWnd, triggerTimer);
		triggerTimer = 0;
		return;
	}

	if (msec > 65000L) {
		msec = 65000L;
	}

	triggerTimer = SetTimer(mswHWnd, TRIGGER_TIMER, (UINT)msec, NULL);

	if (triggerTimer == 0) {
		mswFail("wAlarm: No timers");
	}
}


void wBeep(void)
{
	MessageBeep(MB_OK);
}

/**
 * Show a notification window with a yes/no reply and an icon.
 *
 * \param type IN type of message: Information, Warning, Error
 * \param msg  IN message to display
 * \param yes  IN text for accept button
 * \param no   IN text for cancel button
 * \return    True when accept was selected, false otherwise
 */

int wNoticeEx(
        int type,
        const char * msg,
        const char * yes,
        const char * no)
{
	int res;
	UINT flag;
	char *headline;

	switch (type) {
	case NT_INFORMATION:
		flag = MB_ICONINFORMATION;
		headline = _("Information");
		break;

	case NT_WARNING:
		flag = MB_ICONWARNING;
		headline = _("Warning");
		break;

	case NT_ERROR:
		flag = MB_ICONERROR;
		headline = _("Error");
		break;
	}

	res = MessageBox(mswHWnd, msg, headline,
	                 flag | MB_TASKMODAL|((no==NULL)?MB_OK:MB_YESNO));
	return res == IDOK || res == IDYES;
}

int wNotice(
        const char * msg,
        const char * yes,
        const char * no)
{
	int res;
	res = MessageBox(mswHWnd, msg, "Notice",
	                 MB_TASKMODAL|((no==NULL)?MB_OK:MB_YESNO));
	return res == IDOK || res == IDYES;
}

/**
 * Show a notification window with three choices and an icon.
 *
 * \param msg  IN message to display
 * \param yes  IN text for yes button
 * \param no   IN text for no button
 * \param cancel IN  text for cancel button
 * \return    1 for yes, -1 for no, 0 for cancel
 */


int wNotice3(
        const char * msg,
        const char * yes,
        const char * no,
        const char * cancel)
{
	int res;
	res = MessageBox(mswHWnd, msg, _("Warning"),
	                 MB_ICONWARNING | MB_TASKMODAL|MB_YESNOCANCEL);

	if (res == IDOK || res == IDYES) {
		return 1;
	} else if (res == IDNO) {
		return -1;
	} else {
		return 0;
	}
}

/**
 * Show help text for the given topic.
 *
 * \param  topic The topic. if NULL the index page is shown.
 */

void wHelp(
        const char * topic)
{
	char *pszHelpTopic;
	HWND hwndHelp;
	const char *theTopic = "index";

	if (topic) {
		theTopic = topic;
	}

	if (!helpInitted) {
		HtmlHelp(NULL, NULL, HH_INITIALIZE, (DWORD_PTR)&dwCookie) ;
		helpInitted = TRUE;
	}

	/*	             "c:\\help.chm::/intro.htm>mainwin", */
	/* attention: always adapt constant value (10) to needed number of formatting characters */
	pszHelpTopic = malloc(strlen(helpFile) + strlen(theTopic) + 10);
	assert(pszHelpTopic != NULL);
	sprintf(pszHelpTopic, "/%s.html", theTopic);
	hwndHelp = HtmlHelp(mswHWnd, helpFile, HH_DISPLAY_TOPIC,
	                    (DWORD_PTR)pszHelpTopic);

	if (!hwndHelp) {
		wNoticeEx(NT_ERROR, pszHelpTopic, "Ok", NULL);
	}

	free(pszHelpTopic);
}




void doHelpMenu(void * context)
{
	HH_FTS_QUERY ftsQuery;

	if (!helpInitted) {
		HtmlHelp(NULL, NULL, HH_INITIALIZE, (DWORD_PTR)&dwCookie) ;
		helpInitted = TRUE;
	}

	const char * topic;
	switch ((int)(INT_PTR)context) {
	case 1: /* Contents */
		HtmlHelp(mswHWnd, helpFile, HH_DISPLAY_TOC, (DWORD_PTR)NULL);
		break;

	case 2: /* Search */
		ftsQuery.cbStruct = sizeof(ftsQuery);
		ftsQuery.fExecute = FALSE;
		ftsQuery.fStemmedSearch = FALSE;
		ftsQuery.fTitleOnly = FALSE;
		ftsQuery.pszSearchQuery = NULL;
		ftsQuery.pszWindow = NULL;
		HtmlHelp(mswHWnd, helpFile, HH_DISPLAY_SEARCH,(DWORD_PTR)&ftsQuery);
		break;


	case 3: /*Context*/
		topic = GetCurCommandName();
		wHelp(topic);
		break;

	default:
		return;
	}

	helpInitted = TRUE;
}

void wDoAccelHelp(wAccelKey_e key, void * context)
{
	doHelpMenu(context);
}

void wMenuAddHelp(
        wMenu_p m)
{
	wMenuPushCreate(m, NULL, _("&Contents"), 0, doHelpMenu, (void*)1);
	wMenuPushCreate(m, NULL, _("&Search for Help on..."), 0, doHelpMenu, (void*)2);
	wMenuPushCreate(m, NULL, _("Co&mmand Context Help"), 0, doHelpMenu, (void*)3);
}


void wSetBalloonHelp(wBalloonHelp_t * bh)
{
	balloonHelpStrings = bh;
}


void wEnableBalloonHelp(int enable)
{
	balloonHelpEnable = enable;
}


void wBalloonHelpUpdate(void)
{
}


void wControlSetBalloonText(wControl_p b, const char * text)
{
	b->tipStr = mswStrdup(text);
}

void openBalloonHelp(wControl_p b, int dx, int dy)
{
	HDC hDc;
	DWORD extent;
	RECT rect;
	POINT pt;
	HFONT hFont;
	const char * msg;
	if (b->errStr) {
		msg = b->errStr;
	} else {
		msg = b->tipStr;
		if (!balloonHelpEnable) {
#ifdef BALLOON_TRACE
			fprintf(logFile, "openBalloon !Enable state %d\n", balloonHelpState);
			fflush(logFile);
#endif
			return;
		}
	}
#ifdef BALLOON_TRACE
	fprintf(logFile, "openBalloon %s state %d\n", msg, balloonHelpState);
	fflush(logFile);
#endif
	if (!balloonHelpHWnd) {
		return;
	}
	int w, h;
	hDc = GetDC(balloonHelpHWnd);
	hFont = SelectObject(hDc, mswLabelFont);
	extent = GetTextExtent(hDc, CAST_AWAY_CONST msg, (int)(strlen(msg)));
	w = LOWORD(extent);
	h = HIWORD(extent);

	if (b->type == B_RADIO ||
	    b->type == B_TOGGLE) {
		pt.y = b->h;
	} else {
		GetClientRect(b->hWnd, &rect);
		pt.y = rect.bottom;
	}

	pt.x = dx;
	pt.y -= dy;
	ClientToScreen(b->hWnd, &pt);

	if (pt.x + w + 2 > screenWidth) {
		pt.x = screenWidth - (w + 2);
	}

	if (pt.x < 0) {
		pt.x = 0;
	}

	SetWindowPos(balloonHelpHWnd, HWND_TOPMOST, pt.x, pt.y, w + 6, h + 4,
	             SWP_SHOWWINDOW | SWP_NOACTIVATE);
	if (!b->errStr) {
		SetBkColor(hDc, GetSysColor(COLOR_INFOBK));
		SetTextColor(hDc, GetSysColor(COLOR_INFOTEXT));
	} else {
		SetBkColor(hDc, GetSysColor(COLOR_HIGHLIGHT));
		SetTextColor(hDc, GetSysColor(COLOR_HIGHLIGHTTEXT));
	}
	TextOut(hDc, 2, 1, msg, (int)(strlen(msg)));
	SelectObject(hDc, hFont);
	ReleaseDC(balloonHelpHWnd, hDc);
	balloonHelpState = balloonHelpShow;
	balloonControlButton = b;
}



void startBalloonHelp(void)
{
	wBalloonHelp_t * bh;

	if (!balloonHelpButton->tipStr) {
		if (!balloonHelpStrings) {
			return;
		}
		for (bh = balloonHelpStrings; bh->name
		     && strcmp(bh->name, balloonHelpButton->helpStr) != 0; bh++);
		if (!bh->name || !bh->value) {
			balloonHelpButton->tipStr = _(balloonHelpButton->helpStr);
		} else {
			balloonHelpButton->tipStr = _(bh->value);
		}
	}

	openBalloonHelp(balloonHelpButton, 0, 0);
}


void closeBalloonHelp(int inx)
{
#ifdef BALLOON_TRACE
	fprintf(logFile, "closeBallonHelp %d state=%d\n", inx, balloonHelpState);
	fflush(logFile);
#endif
	if (balloonHelpTimer) {
		KillTimer(mswHWnd, balloonHelpTimer);
		balloonHelpTimer = (UINT_PTR)0;
	}

	if (balloonHelpState == balloonHelpShow)
		if (balloonHelpHWnd) {
			ShowWindow(balloonHelpHWnd, SW_HIDE);
		}

	balloonHelpState = balloonHelpIdle;
}


void wControlSetBalloon(wControl_p b, wWinPix_t dx, wWinPix_t dy,
                        const char * msg)
{
	if (msg) {
		if (b->errStr) {
			free(b->errStr);
		}
		b->errStr = mswStrdup(msg);
		openBalloonHelp(b, dx, dy);
	} else {
		if (b->errStr) {
			free(b->errStr);
		}
		b->errStr = NULL;
		closeBalloonHelp(2);
	}
}



int wGetKeyState(void)
{
	int rc, keyState;
	rc = 0;
	keyState = GetAsyncKeyState(VK_SHIFT);

	if (keyState & 0x8000) {
		rc |= WKEY_SHIFT;
	}

	keyState = GetAsyncKeyState(VK_CONTROL);

	if (keyState & 0x8000) {
		rc |= WKEY_CTRL;
	}

	keyState = GetAsyncKeyState(VK_MENU);

	if (keyState & 0x8000) {
		rc |= WKEY_ALT;
	}

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
        const char * mode)
{
	return fopen(fileName, mode);
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

#define SELECTEDFILENAME_BUFFERSIZE	(8*1024)	/**<estimated size in case all param files are selected */

char *
GetImageFileFormats(void)
{
	char *filter = malloc(2048);
	char *current = filter;
	char *message;

	for (int i = 0; i < sizeof(filterImageFiles) / sizeof(filterImageFiles[0]);
	     i += 2) {
		message = gettext(filterImageFiles[i]);
		strcpy(current, message);
		current += strlen(message) + 1;
		strcpy(current, filterImageFiles[i + 1]);
		current += strlen(current) + 1;
	}
	*current = '\0';
	return(filter);
}

/**
 * Run the file selector. After the selector is finished an array of filenames is
 * created. Each filename will be fully qualified. The array and the number of
 * filenames are passed to a callback function. This is similar to the argc argv c
 * convention in C. Once the callback returns, the allocated strings are free'd.
 *
 * \param fs IN the file selector dialog
 * \param dirName IN the initial directory presented
 * \return FALSE on error, TRUE if ok
 */

int wFilSelect(
        struct wFilSel_t * fs,
        const char * dirName)
{
	int rc;
	OPENFILENAME ofn;
	char **fileName;
	char *nextFileName;
	char *selFileName;
	int cntFiles;
	const char * ext;
	char defExt[4];
	int i;

	if (dirName == NULL ||
	    dirName[0] == '\0' ||
	    strcmp(dirName, ".") == 0) {
		dirName = wGetUserHomeDir();
	}
	memset(&ofn, 0, sizeof ofn);
	ofn.lStructSize = sizeof ofn;
	ofn.hwndOwner = mswHWnd;
	if (fs->option == FS_PICTURES) {
		ofn.lpstrFilter = GetImageFileFormats();
	} else {
		ofn.lpstrFilter = fs->extList;
	}

	ofn.nFilterIndex = 0;
	selFileName = malloc(SELECTEDFILENAME_BUFFERSIZE);
	memset(selFileName, '\0', SELECTEDFILENAME_BUFFERSIZE);
	ofn.lpstrFile = selFileName;
	ofn.nMaxFile = SELECTEDFILENAME_BUFFERSIZE;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = dirName;
	ofn.lpstrTitle = fs->title;
	ext = fs->extList + strlen(fs->extList)+1;

	if (*ext++ == '*' && *ext++ == '.') {
		for (i=0; i<3 && ext[i] && ext[i]!=';'; i++) {
			defExt[i] = ext[i];
		}

		defExt[i] = '\0';
	} else {
		defExt[0] = '\0';
	}

	ofn.lpstrDefExt = defExt;

	if (fs->option & FS_MULTIPLEFILES) {
		ofn.Flags = OFN_ALLOWMULTISELECT | OFN_LONGFILENAMES | OFN_EXPLORER;
	} else {
		ofn.Flags = OFN_LONGFILENAMES;
	}

	if (fs->mode == FS_LOAD) {
		ofn.Flags |= OFN_FILEMUSTEXIST;
		rc = GetOpenFileName(&ofn);
	} else if (fs->mode == FS_SAVE) {
		ofn.Flags |= OFN_OVERWRITEPROMPT;
		rc = GetSaveFileName(&ofn);
	} else if (fs->mode == FS_UPDATE) {
		rc = GetSaveFileName(&ofn);
	} else {
		return FALSE;
	}

	if (!rc) {
		return FALSE;
	}

	nextFileName = selFileName;
	selFileName[ofn.nFileOffset - 1] = '\0';
	cntFiles = 0;

	while (*nextFileName) {
		cntFiles++;
		nextFileName = nextFileName + strlen(nextFileName) + 1;
	}

	// strings were counted including the path on its own so reduce the count
	cntFiles--;
	// build up the array of filenames
	fileName = malloc(sizeof(nextFileName) * cntFiles);
	nextFileName = selFileName + ofn.nFileOffset;

	for (i=0; i < cntFiles; i++) {
		fileName[ i ] = malloc(strlen(selFileName) + strlen(nextFileName) + 2);
		strcpy(fileName[ i ], selFileName);
		strcat(fileName[ i ], FILE_SEP_CHAR);
		strcat(fileName[ i ], nextFileName);
		nextFileName = nextFileName + strlen(nextFileName) + 1;
	}

	fs->action(cntFiles, fileName, fs->data);

	for (i=0; i < cntFiles; i++) {
		free(fileName[ i ]);
	}

	free(fileName);
	free(selFileName);
	return TRUE;
}


struct wFilSel_t * wFilSelCreate(
        wWin_p parent,
        wFilSelMode_e mode,
        int option,
        const char * title,
        const char * extList,
        wFilSelCallBack_p action,
        void * data)
{
	char * cp;
	struct wFilSel_t * ret;
	size_t len;
	ret = (struct wFilSel_t*)malloc(sizeof *ret);
	ret->parent = parent;
	ret->mode = mode;
	ret->option = option;
	ret->title = mswStrdup(title);
	len = strlen(extList);
	ret->extList = (char*)malloc(len+2);
	strcpy(ret->extList,extList);

	for (cp=ret->extList; *cp; cp++) {
		if (*cp == '|') {
			*cp = '\0';
		}
	}

	*++cp = '\0';
	ret->action = action;
	ret->data = data;
	return ret;
}


const char * wMemStats(void)
{
	int rc;
	static char msg[80];
	long usedSize = 0;
	long usedCnt = 0;
	long freeSize = 0;
	long freeCnt = 0;
	_HEAPINFO heapinfo;
	heapinfo._pentry = NULL;

	while ((rc=_heapwalk(&heapinfo)) == _HEAPOK) {
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

	sprintf(msg, "Used: %ld(%ld), Free %ld(%ld)%s",
	        usedSize, usedCnt, freeSize, freeCnt,
	        (rc==_HEAPOK)?"":
	        (rc==_HEAPEMPTY)?"":
	        (rc==_HEAPBADBEGIN)?", BADBEGIN":
	        (rc==_HEAPEND)?"":
	        (rc==_HEAPBADPTR)?", BADPTR":
	        ", Unknown Heap Status");
	return msg;
}

/*
 *****************************************************************************
 *
 * Main
 *
 *****************************************************************************
 */

static wControl_p getControlFromCursor(HWND hWnd, wWin_p * winR)
{
	POINT pt;
	wWin_p w;
	wControl_p b;
	wIndex_t inx;

	if (winR) {
		*winR = NULL;
	}

	GetCursorPos(&pt);
	inx = GetWindowWord(hWnd, 0);

	if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
		/* Unknown control */
		return NULL;
	}

	w=(wWin_p)controlMap(inx-CONTROL_BASE).b;

	if (!w) {
		return NULL;
	}

	if (w->type != W_MAIN && w->type != W_POPUP) {
		return NULL;
	}

	if (winR) {
		*winR = w;
	}

	ScreenToClient(hWnd, &pt);

	for (b = w->first; b; b=b->next) {
		if (b->type == B_BOX || b->type == B_LINES) {
			continue;
		}

		if (b->hWnd == NULL) {
			continue;
		}

		if (IsWindowVisible(b->hWnd) == FALSE) {
			continue;
		}

		if (pt.x > b->x && pt.x < b->x+b->w &&
		    pt.y > b->y && pt.y < b->y+b->h) {
			return b;
		}
	}

	return b;
}

/**
 * Window function for the main window and all popup windows.
 *
 */

LRESULT
FAR
PASCAL
MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int inx;
	wWin_p w;
	wControl_p b, oldW;
	wWinPix_t newW, newH;
	RECT rect;
	PAINTSTRUCT ps;
	HWND hWnd2;
	LRESULT ret;
	HDC hDc;
	wAccelKey_e extChar;

	LPMINMAXINFO pMMI;
	switch (message) {
	case WM_GETMINMAXINFO:
		pMMI = (LPMINMAXINFO)lParam;
		inx = GetWindowWord(hWnd, 0);

		if (inx >= CONTROL_BASE && inx <= controlMap_da.cnt) {
			w = (wWin_p)controlMap(inx - CONTROL_BASE).b;
			if (w != NULL) {
				if (w->validGeometry) {
					pMMI->ptMaxTrackSize.x = w->max_width;
					pMMI->ptMaxTrackSize.y = w->max_height;
					pMMI->ptMinTrackSize.x = w->min_width;
					pMMI->ptMinTrackSize.y = w->min_height;
				}
			}
		}
		return (LRESULT)0;

	case WM_MOUSEWHEEL:
		inx = GetWindowWord(hWnd, 0);
		b = getControlFromCursor(hWnd, NULL);

		if (b && b->type == B_DRAW)
			if (mswCallBacks[b->type] != NULL &&
			    mswCallBacks[b->type]->messageProc)
				return mswCallBacks[b->type]->messageProc((wControl_p)b, hWnd,
				                message, wParam, lParam);

		return (LRESULT)0;

	case WM_COMMAND:
		closeBalloonHelp(3);
	case WM_DRAWITEM:
	case WM_MEASUREITEM:

		if (WCMD_PARAM_ID < CONTROL_BASE || WCMD_PARAM_ID > (WPARAM)controlMap_da.cnt) {
			break;
		}

		b = controlMap(WCMD_PARAM_ID-CONTROL_BASE).b;

		if (!b) {
			break;
		}

		if (b->type == B_BITMAP) {
			// draw the bitmap
			mswDrawIcon(((LPDRAWITEMSTRUCT)lParam)->hDC, 0, 0, (wIcon_p)(b->data), FALSE,
			            (COLORREF)0, (COLORREF)0);
			return (LRESULT)TRUE;
		} else {
			mswSetFocus(b);
			ret = 0;

			if (!inMainWndProc) {
				inMainWndProc = TRUE;

				if (mswCallBacks[b->type] != NULL &&
				    mswCallBacks[b->type]->messageProc) {
					ret = mswCallBacks[b->type]->messageProc(b, hWnd, message, wParam, lParam);
				}

				inMainWndProc = FALSE;
			}

			return (LRESULT)ret;
		}

	case WM_PAINT:
		inx = GetWindowWord(hWnd, 0);

		if (inx >= CONTROL_BASE && inx <= controlMap_da.cnt &&
		    (w = (wWin_p)controlMap(inx-CONTROL_BASE).b) &&
		    (w->type == W_MAIN || w->type == W_POPUP) &&
		    (!IsIconic(mswHWnd)) &&
		    (GetUpdateRect(hWnd, &rect, FALSE))) {
			BeginPaint(hWnd, &ps);

			for (b=w->first; b; b=b->next) {
				if (b->shown &&
				    mswCallBacks[b->type] != NULL &&
				    mswCallBacks[b->type]->repaintProc) {
					mswCallBacks[b->type]->repaintProc(hWnd, b);
				}
			}

			EndPaint(hWnd, &ps);
			return (LRESULT)1;
		}

		break;

	case WM_SIZE:
		inx = GetWindowWord(hWnd, 0);

		if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
			break;
		}

		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;

		if (!w) {
			break;
		}

		if (w->type != W_MAIN && w->type != W_POPUP) {
			break;
		}

		if (w->busy) {
			break;
		}

		switch (wParam) {
		case SIZE_MAXIMIZED:
		case SIZE_MINIMIZED:
		case SIZE_RESTORED:
			newW = LOWORD(lParam);
			newH = HIWORD(lParam);

			if (newW <= 0 || newH <= 0) {
				break;
			}

			if (newW == w->w && newH == w->h) {
				break;
			}

			GetWindowRect(w->hWnd, &rect);
			GetClientRect(w->hWnd, &rect);
			InvalidateRect(w->hWnd, NULL, TRUE);
			w->w = newW;
			w->h = newH;

			if (w->winProc) {
				w->winProc(w, wResize_e, NULL, w->data);
				w->winProc(w, wState_e, NULL, w->data);
			}

			break;

		default:
			break;
		}

		break;

	case WM_CHAR:
	case WM_KEYUP:
		inx = GetWindowWord(hWnd, 0);

		if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
			break;
		}

		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;

		if (!w) {
			break;
		}

		if (w->type != W_MAIN && w->type != W_POPUP) {
			if (mswCallBacks[w->type] != NULL &&
			    mswCallBacks[w->type]->messageProc)
				return mswCallBacks[w->type]->messageProc((wControl_p)w, hWnd,
				                message, wParam, lParam);

			break;
		}

		extChar = translateExtKey(WCMD_PARAM_ID);

		if (message == WM_KEYUP) {
			if (extChar == wAccelKey_None) {
				break;
			}

			if (extChar == wAccelKey_Back) {
				break;
			}
		}

		b = getControlFromCursor(hWnd, NULL);
		closeBalloonHelp(4);

		if (b && b->type == B_DRAW) {
			// Change Num keypad to a special code to emulate cursor keys
			if (wParam == VK_UP ||
			    wParam == VK_DOWN ||
			    wParam == VK_RIGHT ||
			    wParam == VK_LEFT ||
			    wParam == VK_INSERT ||
			    wParam == VK_DELETE) {
				if ((lParam & 0x1000000) == 0) { lParam |= 0x1000000; }
			}
			return SendMessage(b->hWnd, WM_CHAR, wParam, lParam);
		}

		switch (WCMD_PARAM_ID) {
		case 0x0D:

			/* CR - push default button */
			for (b=w->first; b; b=b->next) {
				if (b->type == B_BUTTON && (b->option & BB_DEFAULT) != 0) {
					inMainWndProc = TRUE;

					if (mswCallBacks[B_BUTTON] != NULL &&
					    mswCallBacks[B_BUTTON]->messageProc) {
						ret = mswCallBacks[B_BUTTON]->messageProc(b, b->hWnd,
						                WM_COMMAND, wParam, lParam);
					}

					inMainWndProc = FALSE;
					break;
				}
			}

			return (LRESULT)0;

		case 0x1B:

			/* ESC - push cancel button */
			for (b=w->first; b; b=b->next) {
				if (b->type == B_BUTTON && (b->option & BB_CANCEL) != 0) {
					inMainWndProc = TRUE;

					if (mswCallBacks[B_BUTTON] != NULL &&
					    mswCallBacks[B_BUTTON]->messageProc) {
						ret = mswCallBacks[B_BUTTON]->messageProc(b, b->hWnd,
						                WM_COMMAND, wParam, lParam);
					}

					inMainWndProc = FALSE;
					break;
				}
			}

			mswSetTrigger((wControl_p)TRIGGER_TIMER, NULL);
			return (LRESULT)0;

		case 0x20:

			/* SPC - push current button with focus */
			if ((b=w->focusChainNext) != NULL) {
				switch (b->type) {
				case B_BUTTON:
				case B_CHOICEITEM:
					inMainWndProc = TRUE;

					if (mswCallBacks[b->type] != NULL &&
					    mswCallBacks[b->type]->messageProc) {
						ret = mswCallBacks[b->type]->messageProc(b, b->hWnd,
						                WM_COMMAND, MAKELPARAM(LOWORD(wParam), BN_CLICKED), (LPARAM)(b->hWnd));
					}

					inMainWndProc = FALSE;
					break;
				}
			}

			return (LRESULT)0;

		case 0x09:

			/* TAB - jump to next control */
			if (w->focusChainNext) {
				for (b = w->focusChainNext->focusChainNext;
				     b!=w->focusChainNext;
				     b=b->focusChainNext) {
					if (IsWindowVisible(b->hWnd) && IsWindowEnabled(b->hWnd)) {
						break;
					}
				}

				oldW = w->focusChainNext;
				w->focusChainNext = b;

				if (!inMainWndProc) {
					inMainWndProc = TRUE;
					SetFocus(b->hWnd);

					/*					if( b->type == B_BUTTON)
											InvalidateRect( b->hWnd, NULL, TRUE ); */
					if (oldW->type == B_BUTTON) {
						InvalidateRect(oldW->hWnd, NULL, TRUE);
					}

					inMainWndProc = FALSE;
				}
			}

			return (LRESULT)0;
		}

		/* Not a Draw control */
		MessageBeep(MB_ICONHAND);
		return (LRESULT)0;
		break;

	case WM_ENABLE:
		if (wParam == (WPARAM)1) {
			hWnd2 = SetFocus(hWnd);
		}

		break;

	case WM_F1DOWN:
		if ((hWnd2 = GetActiveWindow()) == hWnd ||
		    (inx=GetWindowWord(hWnd2,0)) < CONTROL_BASE || inx > controlMap_da.cnt) {
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		b=controlMap(inx-CONTROL_BASE).b;

		if (!b) {
			break;
		}

		closeBalloonHelp(5);
		wHelp(b->helpStr);
		return (LRESULT)0;

	case WM_SETCURSOR:
		// Set normal Arrow cursor, DefWindowProc can override it
		SetCursor(LoadCursor(NULL, IDC_ARROW));

		if (!mswAllowBalloonHelp) {
			break;
		}

		if (IsIconic(mswHWnd)) {
			break;
		}

		b = getControlFromCursor(hWnd, NULL);

#ifdef BALLOON_TRACE
		fprintf(logFile, "SETCURSOR %s\n", b ? b->helpStr : "NULL"); fflush(logFile);
#endif
		if (b == balloonControlButton) {
			//closeBalloonHelp(61);
			break;
		}

		if (GetActiveWindow() != hWnd) {
			closeBalloonHelp(62);
			break;
		}
		if (!b) {
			closeBalloonHelp(63);
			break;
		}
		if (b->type == B_DRAW) {
			closeBalloonHelp(64);
			break;
		}
		if (b->helpStr == NULL) {
			closeBalloonHelp(65);
			break;
		}

		if (b != balloonHelpButton) {
			closeBalloonHelp(7);
		}
#ifdef BALLOON_TRACE
		fprintf(logFile, "SETCURSOR state %d\n", balloonHelpState); fflush(logFile);
#endif
		if (balloonHelpState != balloonHelpIdle) {
			break;
		}

		balloonHelpTimer = SetTimer(mswHWnd, BALLOONHELP_TIMER,
		                            balloonHelpTimeOut, NULL);

		if (balloonHelpTimer == (UINT_PTR)0) {
			break;
		}

		balloonHelpState = balloonHelpWait;
		balloonHelpButton = b;
		break;


	case WM_SYSCOMMAND:
		inx = GetWindowWord(hWnd, 0);

		if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
			break;
		}

		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;

		if (!w) {
			break;
		}

		if (w->type != W_POPUP) {
			break;
		}

		if (w->busy) {
			break;
		}

		if ((wParam&0xFFF0) != SC_CLOSE) {
			break;
		}

		if (w->winProc) {
			w->winProc(w, wClose_e, NULL, w->data);
		}

		wWinShow(w, FALSE);
		return (LRESULT)0;

	case WM_CLOSE:
		inx = GetWindowWord(hWnd, 0);

		if (inx < CONTROL_BASE || inx > controlMap_da.cnt) {
			break;
		}

		w = (wWin_p)controlMap(inx-CONTROL_BASE).b;

		if (!w) {
			break;
		}

		if (w->type == W_MAIN) {
			/* It's the big one! */
			/* call main window procedure for processing of shutdown */
			if (w->winProc) {
				(w->winProc(w, wClose_e, NULL, NULL));
			}

			return (LRESULT)0;
		}

	case WM_DESTROY:
		if (hWnd == mswHWnd) {
			PostQuitMessage(0L);
			return (LRESULT)0;
		}

		break;

	case WM_TIMER:
		if (wParam == ALARM_TIMER) {
			KillTimer(mswHWnd, alarmTimer);
			alarmFunc();
		} else if (wParam == TRIGGER_TIMER) {
			KillTimer(mswHWnd, triggerTimer);
			triggerTimer = 0;

			if (triggerFunc) {
				triggerFunc(triggerControl);
			}
		} else if (wParam == BALLOONHELP_TIMER) {
			KillTimer(hWnd, balloonHelpTimer);
			balloonHelpTimer = (UINT_PTR)0;
			startBalloonHelp();
		}

		return (LRESULT)0;

	case WM_MENUSELECT:
		mswAllowBalloonHelp = TRUE;
		closeBalloonHelp(8);
		break;

	case WM_WINDOWPOSCHANGED:
		if (hWnd == mswHWnd && !IsIconic(hWnd) && needToDoPendingShow) {
			for (w=winFirst; w; w=(wWin_p)w->next) {
				if (w->hWnd != mswHWnd &&
				    w->pendingShow) {
					ShowWindow(w->hWnd, SW_SHOW);
				}

				w->pendingShow = FALSE;
			}

			needToDoPendingShow = FALSE;
		}

		break;

	case 51:
		count51++;
	/*return NULL;*/


	case WM_PALETTECHANGED:
		if (wParam == (WPARAM)hWnd) {
			return (LRESULT)0;
		}

	case WM_QUERYNEWPALETTE:
		if (mswPalette) {
			hDc = GetDC(hWnd);
			SelectPalette(hDc, mswPalette, 0);
			inx = RealizePalette(hDc);
			ReleaseDC(hWnd, hDc);

			if (inx>0) {
				InvalidateRect(hWnd, NULL, TRUE);
			}

			return (LRESULT)inx;
		}

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			closeBalloonHelp(9);
		}

		break;

	case WM_HSCROLL:
	case WM_VSCROLL:
		b = getControlFromCursor(hWnd, NULL);

		if (!b) {
			break;
		}

		/*mswSetFocus( b );*/
		ret = 0L;

		if (!inMainWndProc) {
			inMainWndProc = TRUE;

			if (mswCallBacks[b->type] != NULL &&
			    mswCallBacks[b->type]->messageProc) {
				ret = mswCallBacks[b->type]->messageProc(b, hWnd, message, wParam, lParam);
			}

			inMainWndProc = FALSE;
		}

		return (LRESULT)ret;

	case WM_LBUTTONDOWN:
	case WM_MOUSEMOVE:
	case WM_LBUTTONUP:
		b = getControlFromCursor(hWnd, NULL);

		if (!b) {
			break;
		}

		/*mswSetFocus( b );*/
		ret = 0L;

		if (!inMainWndProc) {
			inMainWndProc = TRUE;

			if (mswCallBacks[b->type] != NULL &&
			    mswCallBacks[b->type]->messageProc) {
				ret = mswCallBacks[b->type]->messageProc(b, hWnd, message, wParam, lParam);
			}

			inMainWndProc = FALSE;
		}

		return (LRESULT)ret;

	default:
		;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

/*
 *****************************************************************************
 *
 * INIT
 *
 *****************************************************************************
 */

/**
 * Register window classes used by the application. These are the main window,
 * the popup windows, the tooltip window and the drawing area.
 *
 * \param hinstCurrent IN application instance
 * \return    FALSE in case of error, else TRUE
 */

static BOOL InitApplication(HINSTANCE hinstCurrent)
{
	WNDCLASS wc;
	wc.style = 0L;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 4;
	wc.hInstance = hinstCurrent;
	wc.hIcon = LoadIcon(hinstCurrent, "MSWAPPICON");
	wc.hCursor = NULL;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
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
	wc.hIcon = LoadIcon(NULL, "wAppIcon");
	wc.hCursor = NULL;
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
	wc.hbrBackground = CreateSolidBrush(GetSysColor(COLOR_INFOBK));
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "MswBalloonHelp";

	if (!RegisterClass(&wc)) {
		mswFail("RegisterClass(BalloonHelp)");
		return FALSE;
	}

	wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc = mswDrawPush;
	wc.lpszClassName = mswDrawWindowClassName;
	wc.cbWndExtra = 4;

	if (!RegisterClass(&wc)) {
		mswFail("RegisterClass(drawClass)");
		return FALSE;
	}

	return TRUE;
}

/**
 * Standard entry point for the app. Nothing special,
 * create some window classes, initialize some global
 * variables with system information, call the application main
 * and finally process the message queue.
 */

int PASCAL WinMain(HINSTANCE hinstCurrent, HINSTANCE hinstPrevious,
                   LPSTR lpszCmdLine, int nCmdShow)
{
	MSG msg;
	HDC hDc;
	char **argv;
	int argc;

	if (!hinstPrevious) {
		if (!InitApplication(hinstCurrent)) {
			return FALSE;
		}
	}

	mswHInst = hinstCurrent;
	mTitleH = GetSystemMetrics(SM_CYCAPTION) - 1;
	mFixBorderW = GetSystemMetrics(SM_CXBORDER);
	mFixBorderH = GetSystemMetrics(SM_CYBORDER);
	mResizeBorderW = GetSystemMetrics(SM_CXFRAME);
	mResizeBorderH = GetSystemMetrics(SM_CYFRAME);
	mMenuH = GetSystemMetrics(SM_CYMENU) + 1;
	screenWidth = GetSystemMetrics(SM_CXSCREEN);
	screenHeight = GetSystemMetrics(SM_CYSCREEN);
	mswLabelFont = GetStockObject(DEFAULT_GUI_FONT);
	hDc = GetDC(0);
	mswScale = GetDeviceCaps(hDc, LOGPIXELSX) / 96.0;
	hWindowIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));

	if (mswScale < 1.0) {
		mswScale = 1.0;
	}

	ReleaseDC(0, hDc);
	mswCreateCheckBitmaps();
	/*
	   get the command line parameters in standard C style and pass them to the main function. The
	   globals are predefined by Visual C
	*/
	argc = __argc;
	argv = __argv;
	mswWin = wMain(argc, (char**)argv);

	if (mswWin == NULL) {
		return 0;
	}

	balloonHelpHWnd = CreateWindow("MswBalloonHelp", "BalloonHelp",
	                               WS_POPUP|WS_BORDER,
	                               0, 0, 80, 40, mswHWnd, NULL, mswHInst, NULL);

	if (balloonHelpHWnd == (HWND)0) {
		mswFail("CreateWindow(BALLOONHELP)");
	} else {
		hDc = GetDC(balloonHelpHWnd);
		/* We need to remember this because the hDc gets changed somehow,
		/* and we when we select the oldFont back in we don't get newFont */
		balloonHelpNewFont = CreateFont(- balloonHelpFontSize, 0, 0, 0, 0, 0,
		                                0, 0, 0, 0, 0, 0, 0, balloonHelpFaceName);
		balloonHelpOldFont = SelectObject(hDc, balloonHelpNewFont);
		ReleaseDC(balloonHelpHWnd, hDc);
	}

	SetCursor(LoadCursor(NULL, IDC_ARROW));

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!mswTranslateAccelerator(mswWin->hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (helpInitted == TRUE) {
		HtmlHelp(NULL, NULL, HH_UNINITIALIZE, (DWORD)dwCookie);
	}

	return (int)msg.wParam;
}
