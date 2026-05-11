/** \file mswstubs.c
 * Stub implementations for GTK3 wlib API functions not yet ported to mswlib.
 * Each stub logs a "not implemented" message and returns a safe no-op value.
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2025 Dave Bullis
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
#include <stddef.h>
#include "mswint.h"

/* ── Bitmap ─────────────────────────────────────────────────────────────── */

wBool_t wBitmapDelete(wControl_p b)
{
	mswFail("wBitmapDelete: not implemented");
	return FALSE;
}

wControl_p wBitmapViewCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
                              long options, const wIcon_p iconP)
{
	mswFail("wBitmapViewCreate: not implemented");
	return NULL;
}

wBool_t wBitmapWriteFile(wControl_p b, const char *filename)
{
	mswFail("wBitmapWriteFile: not implemented");
	return FALSE;
}

/* ── Buttons ─────────────────────────────────────────────────────────────── */

wControl_p wButtonCreateForToolbar(wControl_p w, wWinPix_t x, wWinPix_t y,
                                    const char *helpStr, wIcon_p icon,
                                    long option, wWinPix_t width,
                                    wButtonCallBack_p action, void *context)
{
	mswFail("wButtonCreateForToolbar: not implemented");
	return NULL;
}

void wButtonMakeSplit(wControl_p button, wMenu_p popupMenu)
{
	mswFail("wButtonMakeSplit: not implemented");
}

void wButtonSetIcon(wControl_p button, wIcon_p icon)
{
	mswFail("wButtonSetIcon: not implemented");
}

/* ── ComboBox ────────────────────────────────────────────────────────────── */

void wComboBoxClear(wControl_p control)
{
	mswFail("wComboBoxClear: not implemented");
}

wControl_p wComboBoxCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
                            const char *helpStr, const char *labelStr,
                            long option, long number, wWinPix_t width,
                            long *valueP, wListCallBack_p action,
                            void *attributes)
{
	mswFail("wComboBoxCreate: not implemented");
	return NULL;
}

wControl_p wComboBoxCreateForToolbar(wControl_p parent, const char *helpStr,
                                      const char *labelStr, long option,
                                      wWinPix_t width, long *valueP,
                                      wListCallBack_p action, void *context)
{
	mswFail("wComboBoxCreateForToolbar: not implemented");
	return NULL;
}

void wComboBoxSetIndex(wControl_p b, int row)
{
	mswFail("wComboBoxSetIndex: not implemented");
}

/* ── Controls (general) ──────────────────────────────────────────────────── */

wBool_t wControlGetActive(wControl_p control)
{
	mswFail("wControlGetActive: not implemented");
	return FALSE;
}

void wControlGetPos(wControl_p control, wWinPix_t *x, wWinPix_t *y)
{
	mswFail("wControlGetPos: not implemented");
	if (x) { *x = 0; }
	if (y) { *y = 0; }
}

void wControlSetCustomTooltip(wControl_p control,
                               char *tooltip)
{
	mswFail("wControlSetCustomTooltip: not implemented");
}

/* ── Dialog ──────────────────────────────────────────────────────────────── */

void wDialogButtonsConfigure(wControl_p dialog, const char *okLabel,
                               const char *cancelLabel, const char *helpLabel)
{
	mswFail("wDialogButtonsConfigure: not implemented");
}

void wDialogSaveSizePos(wControl_p dialog)
{
	mswFail("wDialogSaveSizePos: not implemented");
}

/* ── Drawing area ────────────────────────────────────────────────────────── */

void wDrawClipClear(wControl_p drawingArea)
{
	mswFail("wDrawClipClear: not implemented");
}

/* ── Entry ───────────────────────────────────────────────────────────────── */

wControl_p wEntryCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
                         const char *helpStr, const char *labelStr, long option,
                         wWinPix_t width, char *valueP, wIndex_t valueL,
                         wEntryCallBack_p action, void *context)
{
	mswFail("wEntryCreate: not implemented");
	return NULL;
}

void wEntrySetValue(wControl_p control, const char *value)
{
	mswFail("wEntrySetValue: not implemented");
}

/* ── Font ────────────────────────────────────────────────────────────────── */

int wFontGetCharHeight(wControl_p control, wFont_p font, double size)
{
	mswFail("wFontGetCharHeight: not implemented");
	return 0;
}

int wFontGetCharWidth(wControl_p control, wFont_p font, double size)
{
	mswFail("wFontGetCharWidth: not implemented");
	return 0;
}

double wFTLabelLoadFontFromFile(const char *filename)
{
	mswFail("wFTLabelLoadFontFromFile: not implemented");
	return 0.0;
}

/* ── Key state ───────────────────────────────────────────────────────────── */

int wGetKeyStateFromButton(void)
{
	return 0;
}

void wResetKeyStateFromButton(void)
{
}

/* ── Tooltip init ────────────────────────────────────────────────────────── */

void wInitTooltip(wTooltip_t *tooltips, unsigned int count)
{
	mswFail("wInitTooltip: not implemented");
}

/* ── List ────────────────────────────────────────────────────────────────── */

void wListDeleteSelected(wControl_p list)
{
	mswFail("wListDeleteSelected: not implemented");
}

void wListSetStore(wControl_p list, DataStore *liststore)
{
	mswFail("wListSetStore: not implemented");
}

wControl_p wListstoreCreate(wControl_p parent, const char *liststoreID)
{
	mswFail("wListstoreCreate: not implemented");
	return NULL;
}

/* ── Resources ───────────────────────────────────────────────────────────── */

wBool_t wLoadResourceFile(const char *filename)
{
	mswFail("wLoadResourceFile: not implemented");
	return FALSE;
}

/* ── Message ─────────────────────────────────────────────────────────────── */

void wMessageSetLength(wControl_p control, size_t length)
{
	mswFail("wMessageSetLength: not implemented");
}

/* ── Notice ──────────────────────────────────────────────────────────────── */

int wNoticeWithIcon(int type, const char *msg, const char *yes, const char *no)
{
	mswFail("wNoticeWithIcon: not implemented");
	return 0;
}

/* ── Notebook ────────────────────────────────────────────────────────────── */

wControl_p wNotebookCreate(wControl_p parent, const char *labelStr,
                            unsigned activePage, long flags)
{
	mswFail("wNotebookCreate: not implemented");
	return NULL;
}

int wNoteBookGetActivePage(wControl_p notebook)
{
	mswFail("wNoteBookGetActivePage: not implemented");
	return 0;
}

/* ── Scale ───────────────────────────────────────────────────────────────── */

wControl_p wScaleCreate(wControl_p parent, const char *id, double *valuePointer,
                         wScaleCallBack_p action, void *context)
{
	mswFail("wScaleCreate: not implemented");
	return NULL;
}

double wScaleGetValue(wControl_p scale)
{
	mswFail("wScaleGetValue: not implemented");
	return 0.0;
}

void wScaleSetValue(wControl_p scale, double value)
{
	mswFail("wScaleSetValue: not implemented");
}

/* ── Separator ───────────────────────────────────────────────────────────── */

wControl_p wSeparatorCreateForToolbar(wControl_p parent, int width)
{
	mswFail("wSeparatorCreateForToolbar: not implemented");
	return NULL;
}

/* ── Stack ───────────────────────────────────────────────────────────────── */

wControl_p wStackCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
                         const char *helpStr, const char *labelStr, long option,
                         wWinPix_t width, wButtonCallBack_p action,
                         void *context)
{
	mswFail("wStackCreate: not implemented");
	return NULL;
}

void wStackPageShow(wControl_p stack, const char *pageName)
{
	mswFail("wStackPageShow: not implemented");
}

/* ── Status bar ──────────────────────────────────────────────────────────── */

wWinPix_t wStatusSetRequiredHeight(wControl_p label, long flags)
{
	mswFail("wStatusSetRequiredHeight: not implemented");
	return 0;
}

void wStatusSetVisibleControlSet(wControl_p mainWindow,
                                  const char *controlsName)
{
	mswFail("wStatusSetVisibleControlSet: not implemented");
}

/* ── Sticky ──────────────────────────────────────────────────────────────── */

wControl_p wStickyCreateForToolbar(wControl_p parent, wWinPix_t x, wWinPix_t y,
                                    const char *helpStr, wIcon_p icon,
                                    long option, wWinPix_t width,
                                    wButtonCallBack_p action, void *context)
{
	mswFail("wStickyCreateForToolbar: not implemented");
	return NULL;
}

wBool_t wStickyGetSticky(wControl_p b)
{
	mswFail("wStickyGetSticky: not implemented");
	return FALSE;
}

/* ── Tag ─────────────────────────────────────────────────────────────────── */

wControl_p wTagCreate(wControl_p parent, const char *helpStr,
                       const char *labelStr, wButtonCallBack_p action,
                       void *context)
{
	mswFail("wTagCreate: not implemented");
	return NULL;
}

void wTagSetLabel(wControl_p tagControl, const char *text)
{
	mswFail("wTagSetLabel: not implemented");
}

/* ── Toggle (toolbar) ────────────────────────────────────────────────────── */

wControl_p wToggleCreateForToolbar(wControl_p parent, wWinPix_t x, wWinPix_t y,
                                    const char *helpStr, wIcon_p icon,
                                    long option, wWinPix_t width,
                                    wButtonCallBack_p action, void *context)
{
	mswFail("wToggleCreateForToolbar: not implemented");
	return NULL;
}

/* ── Tooltips ────────────────────────────────────────────────────────────── */

void wTooltipSet(wControl_p control, const char *dialog,
                  const char *dialogItem)
{
	mswFail("wTooltipSet: not implemented");
}

void wTooltipSetText(wControl_p control, const char *tooltipText)
{
	mswFail("wTooltipSetText: not implemented");
}

/* ── Window creation ─────────────────────────────────────────────────────── */

wControl_p wWinDialogCreate(wControl_p parent, const char *helpStr,
                              const char *titleStr, const char *nameStr,
                              long option, wWinCallBack_p winProc, void *context)
{
	mswFail("wWinDialogCreate: not implemented");
	return NULL;
}

wControl_p wWindowCreate(wControl_p parent, int winType, wWinPix_t x,
                          wWinPix_t y, const char *labelStr, const char *nameStr,
                          long option, wWinCallBack_p winProc, void *context)
{
	mswFail("wWindowCreate: not implemented");
	return NULL;
}

void wWinSetAspectRatio(wControl_p win, wWinPix_t x, wWinPix_t y)
{
	mswFail("wWinSetAspectRatio: not implemented");
}
