/** \file mswtext.c
* Text entry field
*/

/*  XTrkCad - Model Railroad CAD
*  Copyright (C) 2005 Dave Bullis
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
    "Courier"
};
static HFONT fixedTextFont, prevTextFont;

struct wText_t {
    WOBJ_COMMON
    HANDLE hText;
};

BOOL_T textPrintAbort = FALSE;


void wTextClear(
    wText_p b)
{
    long rc;
    rc = SendMessage(b->hWnd, EM_SETREADONLY, 0, 0L);
#ifdef WIN32
    rc = SendMessage(b->hWnd, EM_SETSEL, 0, -1);
#else
    rc = SendMessage(b->hWnd, EM_SETSEL, 1, MAKELONG(0, -1));
#endif
    rc = SendMessage(b->hWnd, WM_CLEAR, 0, 0L);

    if (b->option&BO_READONLY) {
        rc = SendMessage(b->hWnd, EM_SETREADONLY, 1, 0L);
    }
}

/**
 * Append text to a multiline text box:
 * For every \n a \r is added
 * the current text is retrieved from the control
 * the new text is appended and then set
 *
 * \param b IN text box handle
 * \param text IN text to add to text box
 * \return
 */

void wTextAppend(
    wText_p b,
    const char * text)
{
    char *cp;
    char *buffer;
    char *extText;
    int textSize;
    int len = strlen(text);

    if (!len) {
        return;
    }

    for (cp = (char *)text; *cp; cp++) {
        if (*cp == '\n') {
            len++;
        }
    }

    extText = malloc(len + 1 + 10);

    for (cp=extText; *text; cp++,text++) {
        if (*text == '\n') {
            *cp++ = '\r';
            *cp = '\n';
        } else {
            *cp = *text;
        }
    }

    *cp = '\0';
    textSize = GetWindowTextLength(b->hWnd);
    buffer = malloc((textSize + len + 1) * sizeof(char));

    if (buffer) {
        GetWindowText(b->hWnd, buffer, textSize + 1);
        strcat(buffer, extText);
        SetWindowText(b->hWnd, buffer);
        free(extText);
        free(buffer);
    } else {
        abort();
    }

    if (b->option&BO_READONLY) {
        SendMessage(b->hWnd, EM_SETREADONLY, 1, 0L);
    }
}


BOOL_T wTextSave(
    wText_p b,
    const char * fileName)
{
    FILE * f;
    int lc, l, len;
    char line[255];
    f = wFileOpen(fileName, "w");

    if (f == NULL) {
        MessageBox(((wControl_p)(b->parent))->hWnd, "TextSave", "", MB_OK|MB_ICONHAND);
        return FALSE;
    }

    lc = (int)SendMessage(b->hWnd, EM_GETLINECOUNT, 0, 0L);

    for (l=0; l<lc; l++) {
        *(WORD*)line = sizeof(line)-1;
        len = (int)SendMessage(b->hWnd, EM_GETLINE, l, (DWORD)(LPSTR)line);
        line[len] = '\0';
        fprintf(f, "%s\n", line);
    }

    fclose(f);
    return TRUE;
}


BOOL_T wTextPrint(
    wText_p b)
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
        MessageBox(((wControl_p)(b->parent))->hWnd, "Print", "Cannot print",
                   MB_OK|MB_ICONHAND);
        return FALSE;
    }

    docInfo.cbSize = sizeof(DOCINFO);
    docInfo.lpszDocName = "XTrkcad Log";
    docInfo.lpszOutput = (LPSTR)NULL;

    if (StartDoc(hDc, &docInfo) < 0) {
        MessageBox(((wControl_p)(b->parent))->hWnd, "Unable to start print job", NULL,
                   MB_OK|MB_ICONHAND);
        DeleteDC(hDc);
        return FALSE;
    }

    StartPage(hDc);
    GetTextMetrics(hDc, &textMetric);
    lineSpace = textMetric.tmHeight + textMetric.tmExternalLeading;
    linesPerPage = GetDeviceCaps(hDc, VERTRES) / lineSpace;
    currentLine = 1;
    lc = (int)SendMessage(b->hWnd, EM_GETLINECOUNT, 0, 0L);
    IOStatus = 0;

    for (l=0; l<lc; l++) {
        *(WORD*)line = sizeof(line)-1;
        len = (int)SendMessage(b->hWnd, EM_GETLINE, l, (DWORD)(LPSTR)line);
        TextOut(hDc, 0, currentLine*lineSpace, line, len);

        if (++currentLine > linesPerPage) {
            IOStatus = EndPage(hDc);
            if (IOStatus < 0 || textPrintAbort) {
                break;
            }
            StartPage(hDc);
			currentLine = 1;
		}
    }

    if (IOStatus >= 0 && !textPrintAbort) {
        EndPage(hDc);
        EndDoc(hDc);
    }

    DeleteDC(hDc);
    return TRUE;
}


wBool_t wTextGetModified(
    wText_p b)
{
    int rc;
    rc = (int)SendMessage(b->hWnd, EM_GETMODIFY, 0, 0L);
    return (wBool_t)rc;
}

int wTextGetSize(
    wText_p b)
{
    int lc, l, li, len=0;
    lc = (int)SendMessage(b->hWnd, EM_GETLINECOUNT, 0, 0L);

    for (l=0; l<lc ; l++) {
        li = (int)SendMessage(b->hWnd, EM_LINEINDEX, l, 0L);
        len += (int)SendMessage(b->hWnd, EM_LINELENGTH, l, 0L) + 1;
    }

    if (len == 1) {
        len = 0;
    }

    return len;
}

void wTextGetText(
    wText_p b,
    char * t,
    int s)
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
    wBool_t ro)
{
    if (ro) {
        b->option |= BO_READONLY;
    } else {
        b->option &= ~BO_READONLY;
    }

    SendMessage(b->hWnd, EM_SETREADONLY, ro, 0L);
}


void wTextSetSize(
    wText_p bt,
    wPos_t width,
    wPos_t height)
{
    bt->w = width;
    bt->h = height;

    if (!SetWindowPos(bt->hWnd, HWND_TOP, 0, 0,
                      bt->w, bt->h, SWP_NOMOVE|SWP_NOZORDER)) {
        mswFail("wTextSetSize: SetWindowPos");
    }
}


void wTextComputeSize(
    wText_p bt,
    int rows,
    int lines,
    wPos_t * w,
    wPos_t * h)
{
    static wPos_t scrollV_w = -1;
    static wPos_t scrollH_h = -1;
    HDC hDc;
    TEXTMETRIC metrics;

    if (scrollV_w < 0) {
        scrollV_w = GetSystemMetrics(SM_CXVSCROLL);
    }

    if (scrollH_h < 0) {
        scrollH_h = GetSystemMetrics(SM_CYHSCROLL);
    }

    hDc = GetDC(bt->hWnd);
    GetTextMetrics(hDc, &metrics);
    *w = rows * metrics.tmAveCharWidth + scrollV_w;
    *h = lines * (metrics.tmHeight + metrics.tmExternalLeading);
    ReleaseDC(bt->hWnd, hDc);

    if (bt->option&BT_HSCROLL) {
        *h += scrollH_h;
    }
}


void wTextSetPosition(
    wText_p bt,
    int pos)
{
    long rc;
    rc = SendMessage(bt->hWnd, EM_LINESCROLL, 0, MAKELONG(-65535, 0));
}

static void textDoneProc(wControl_p b)
{
    wText_p t = (wText_p)b;
    HDC hDc;
    hDc = GetDC(t->hWnd);
    SelectObject(hDc, mswOldTextFont);
    ReleaseDC(t->hWnd, hDc);
}

static callBacks_t textCallBacks = {
    mswRepaintLabel,
    textDoneProc,
    NULL
};

wText_p wTextCreate(
    wWin_p	parent,
    POS_T	x,
    POS_T	y,
    const char	* helpStr,
    const char	* labelStr,
    long	option,
    POS_T	width,
    POS_T	height)
{
    wText_p b;
    DWORD style;
    RECT rect;
    int index;
    b = mswAlloc(parent, B_TEXT, labelStr, sizeof *b, NULL, &index);
    mswComputePos((wControl_p)b, x, y);
    b->option = option;
    style = ES_MULTILINE | ES_LEFT | ES_AUTOVSCROLL | ES_WANTRETURN |
            WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL;
#ifdef BT_HSCROLL

    if (option & BT_HSCROLL) {
        style |= WS_HSCROLL | ES_AUTOHSCROLL;
    }

#endif
    /*	  if (option & BO_READONLY)
    		style |= ES_READONLY;*/
    b->hWnd = CreateWindow("EDIT", NULL,
                           style, b->x, b->y,
                           width, height,
                           ((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL);

    if (b->hWnd == NULL) {
        mswFail("CreateWindow(TEXT)");
        return b;
    }

#ifdef CONTROL3D
    Ctl3dSubclassCtl(b->hWnd);
#endif

    if (option & BT_FIXEDFONT) {
        if (fixedTextFont == (HFONT)0) {
            fixedTextFont =	 CreateFontIndirect(&fixedFont);
        }

        SendMessage(b->hWnd, WM_SETFONT, (WPARAM)fixedTextFont, (LPARAM)MAKELONG(1, 0));
    } else 	if (!mswThickFont) {
        SendMessage(b->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L);
    }

    b->hText = (HANDLE)SendMessage(b->hWnd, EM_GETHANDLE, 0, 0L);

    if (option & BT_CHARUNITS) {
        wPos_t w, h;
        wTextComputeSize(b, width, height, &w, &h);

        if (!SetWindowPos(b->hWnd, HWND_TOP, 0, 0,
                          w, h, SWP_NOMOVE|SWP_NOZORDER)) {
            mswFail("wTextCreate: SetWindowPos");
        }
    }

    GetWindowRect(b->hWnd, &rect);
    b->w = rect.right - rect.left;
    b->h = rect.bottom - rect.top;
    mswAddButton((wControl_p)b, FALSE, helpStr);
    mswCallBacks[B_TEXT] = &textCallBacks;
    return b;
}
