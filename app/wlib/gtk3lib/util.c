/** \file util.c
 * Some basic window functions
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"
#include "dynarr.h"
#include "i18n.h"

long debugWindow = 0;

char wConfigName[ 256 ];

const char * wNames[] = {
	"MAIN",
	"POPUP",
	"BUTT",
	"CANCEL",
	"POPUP",
	"TEXT",
	"INTEGER",
	"FLOAT",
	"LIST",
	"DROPLIST",
	"COMBOLIST",
	"RADIO",
	"TOGGLE",
	"DRAW",
	"MENU",
	"MULTITEXT",
	"MESSAGE",
	"LINES",
	"MENUITEM",
	"BOX"
};

static bool audioOn;

/*
 *****************************************************************************
 *
 * Internal Utility functions
 *
 *****************************************************************************
 */

/**
 * Add a label to an existing widget
 *
 * \param b IN widget
 * \param x,y IN position in grid
 * \param labelStr IN label to add
 */

void wlibAddLabel(wControl_p b, wWinPix_t x, wWinPix_t y, const char * labelStr)
{
	if (labelStr != NULL) {
		b->label = gtk_label_new(wlibConvertInput(labelStr));
		gtk_widget_set_halign(b->label, GTK_ALIGN_END);
		gtk_widget_show(b->label);
		wlibBasicGridAttach(b->parent, b->label, x, y, 1, 1);
	}
}


/**
 * Initialize the internal structure with the size of the widget
 *
 * \param b IN widget
 */

void wlibControlGetSize(
        wControl_p b)
{
	GtkRequisition requisition;
	gtk_widget_get_preferred_size(b->widget, NULL, &requisition);
}

/*
 *****************************************************************************
 *
 * Exported Utility Functions
 *
 *****************************************************************************
 */

/**
* Change audio setting.
*
*\param setting   true: beep is on
*/
void
wSetAudio(bool setting)
{
	audioOn = (setting > 0);
}

/**
 * Beep!
 */
void wBeep(void)
{
	if (audioOn) {
		gdk_display_beep(gdk_display_get_default());
	}
}

/**
 * Flushes all commands to the Window.
 */

void wFlush(
        void)
{
	while (gtk_events_pending()) {
		gtk_main_iteration_do(FALSE);
	}

	gdk_display_sync(gdk_display_get_default());
}

/**
 * Not implemented
 */

void wWinTop(wControl_p win)
{
}

extern long dontHideCursor;

/**
 * Set the cursor in GTK
 *
 * \param bd IN
 * \param cursor IN
 */

void wSetCursor(wControl_p bd, wCursor_t cursor)
{
	static GdkCursor * gdkcursors[wCursorQuestion+1];
	GdkCursor * gdkcursor;

	GdkWindow * gdkwindow = gdk_get_default_root_window();
	GdkDisplay * display = gdk_window_get_display(gdkwindow);
	if ((cursor == wCursorNone) && dontHideCursor) { return; }  //Ignore if we dont want to suppress
	if (!gdkcursors[cursor]) {
		switch(cursor) {
		case wCursorAppStart:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_WATCH);
			break;
		case wCursorHand:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_HAND2);
			break;
		case wCursorNo:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_X_CURSOR);
			break;
		case wCursorSizeAll:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_FLEUR);
			break;
		case wCursorSizeNESW:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_BOTTOM_LEFT_CORNER);
			break;
		case wCursorSizeNS:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_DOUBLE_ARROW);
			break;
		case wCursorSizeNWSE:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_BOTTOM_RIGHT_CORNER);
			break;
		case wCursorSizeWE:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_SB_H_DOUBLE_ARROW);
			break;
		case wCursorWait:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_WATCH);
			break;
		case wCursorIBeam:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_XTERM);
			break;
		case wCursorCross:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_TCROSS);
			break;
		case wCursorQuestion:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_QUESTION_ARROW);
			break;
		case wCursorNone:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_BLANK_CURSOR);
			break;
		case wCursorNormal:
		default:
			gdkcursor = gdk_cursor_new_for_display( display,GDK_LEFT_PTR);
			break;

		}
		gdkcursors[cursor] = gdkcursor;
	} else { gdkcursor = gdkcursors[cursor]; }

	gdk_window_set_cursor ( gtk_widget_get_window(bd->widget), gdkcursor);
}

/**
 * Not implemented
 * \returns
 */

const char * wMemStats(void)
{
	return "No stats available";
}

/**
 * Get the size of the screen
 *
 * \param w IN pointer to width
 * \param h IN pointer to height
 */

void wGetDisplaySize(wWinPix_t * w, wWinPix_t * h)
{
	GdkDisplay *display = gdk_display_get_default();
	GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
	GdkRectangle screen_geometry = { 0, 0, 0, 0 };

	// not every backend has a primary monitor, e.g. Wayland
	if (monitor == NULL) {
		monitor = gdk_display_get_monitor(display, 0);
	}
	if (monitor != NULL) {
		gdk_monitor_get_geometry(monitor, &screen_geometry);
	}

	*w = screen_geometry.width;
	*h = screen_geometry.height;
}


/**
 * Make text read from a file UTF-8, in place. Text that isn't valid UTF-8 is
 * taken to be cp1252, the Windows code page for western languages: layouts
 * made from the parameter files before 2017, and layouts saved on Windows
 * before 5.4.0, hold their non-ASCII text that way. A byte cp1252 doesn't
 * define is read as Latin-1, so every text converts. If the result doesn't
 * fit, the text is left as it was.
 *
 * \param text IN/OUT the text, NUL-terminated
 * \param size IN size of the buffer that holds it
 * \returns true if the text is UTF-8 now
 */

bool wTextToUTF8(char *text, size_t size)
{
	gchar *utf8;
	gsize len = 0;

	if (g_utf8_validate(text, -1, NULL)) {
		return true;
	}
	utf8 = g_convert(text, -1, "UTF-8", "CP1252", NULL, &len, NULL);
	if (utf8 == NULL) {
		utf8 = g_convert(text, -1, "UTF-8", "ISO-8859-1", NULL, &len, NULL);
	}
	if (utf8 == NULL || len >= size) {
		g_free(utf8);
		return false;
	}
	memcpy(text, utf8, len + 1);
	g_free(utf8);
	return true;
}

/**
 * Convert a string to UTF-8 for GTK. A string that isn't UTF-8 is read as
 * cp1252, the Windows code page old files were written in, as
 * wTextToUTF8() does; a byte cp1252 doesn't define as Latin-1. Before, every
 * such byte was taken as Latin-1, which gave the wrong characters for
 * cp1252's 0x80-0x9F (the euro sign, typographic quotes).
 *
 * \param inString IN string to convert
 * \returns the string itself if it is UTF-8, otherwise the converted copy,
 *          which stays valid until the next call (GTK runs in one thread)
 */

char * wlibConvertInput(const char * inString)
{
	static gchar *converted = NULL;

	if (g_utf8_validate(inString, -1, NULL)) {
		return (char *)inString;
	}

	g_free(converted);
	converted = g_convert(inString, -1, "UTF-8", "CP1252", NULL, NULL, NULL);
	if (converted == NULL) {
		converted = g_convert(inString, -1, "UTF-8", "ISO-8859-1", NULL, NULL,
		                      NULL);
	}
	if (converted == NULL) {
		// Never shows the bytes as they are: GTK only takes UTF-8
		converted = g_utf8_make_valid(inString, -1);
	}
	return (char *)converted;
}

/*-----------------------------------------------------------------*/


static dynArr_t accelData_da;
#define accelData(N) DYNARR_N( accelData_t, accelData_da, N )

static guint accelKeyMap[] = {
	0, /* wAccelKey_None, */
	GDK_KEY_Delete, /* wAccelKey_Del, */
	GDK_KEY_Insert, /* wAccelKey_Ins, */
	GDK_KEY_Home, /* wAccelKey_Home, */
	GDK_KEY_End, /* wAccelKey_End, */
	GDK_KEY_Page_Up, /* wAccelKey_Pgup, */
	GDK_KEY_Page_Down, /* wAccelKey_Pgdn, */
	GDK_KEY_Up, /* wAccelKey_Up, */
	GDK_KEY_Down, /* wAccelKey_Down, */
	GDK_KEY_Right, /* wAccelKey_Right, */
	GDK_KEY_Left, /* wAccelKey_Left, */
	GDK_KEY_BackSpace, /* wAccelKey_Back, */
	GDK_KEY_F1, /* wAccelKey_F1, */
	GDK_KEY_F2, /* wAccelKey_F2, */
	GDK_KEY_F3, /* wAccelKey_F3, */
	GDK_KEY_F4, /* wAccelKey_F4, */
	GDK_KEY_F5, /* wAccelKey_F5, */
	GDK_KEY_F6, /* wAccelKey_F6, */
	GDK_KEY_F7, /* wAccelKey_F7, */
	GDK_KEY_F8, /* wAccelKey_F8, */
	GDK_KEY_F9, /* wAccelKey_F9, */
	GDK_KEY_F10, /* wAccelKey_F10, */
	GDK_KEY_F11, /* wAccelKey_F11, */
	GDK_KEY_F12, /* wAccelKey_F12, */
	GDK_KEY_KP_Add, /* wAccelKey_Numpad_Add */
	GDK_KEY_KP_Subtract /* wAccelKey_Numpad_Subtract */
};

/**
 * Create an accelerator key
 *
 * \param key IN primary key stroke
 * \param modifier IN modifier (shift, ctrl, etc.)
 * \param action IN function to call
 * \param data IN data to pass to function
 */

void wAttachAccelKey(
        wAccelKey_e key,
        int modifier,
        wAccelKeyCallBack_p action,
        void * data)
{
	accelData_t * ad;

//    if (key < 1 || key > wAccelKey_F12) {
//        fprintf(stderr, "wAttachAccelKey(%d) out of range\n", (int) key);
//        return;
//    }

	DYNARR_APPEND(accelData_t, accelData_da, 10);
	ad = &accelData(accelData_da.cnt - 1);
	ad->key = key;
	ad->modifier = modifier;
	ad->action = action;
	ad->data = data;
}

/**
 * Check for accelerator definition a pressed key
 *
 * \param event IN key press event
 * \returns pointer to accel key structure, NULL if not existing
 */

struct accelData_t * wlibFindAccelKey(
        GdkEventKey * event)
{
	accelData_t * ad;
	int modifier = 0;

	if ((event->state & GDK_SHIFT_MASK)) {
		modifier |= WKEY_SHIFT;
	}

	if ((event->state & GDK_CONTROL_MASK)) {
		modifier |= WKEY_CTRL;
	}

	if ((event->state & GDK_MOD1_MASK)) {
		modifier |= WKEY_ALT;
	}

	for (ad = &accelData(0); ad<&accelData(accelData_da.cnt); ad++)
		if (event->keyval == accelKeyMap[ad->key] &&
		    modifier == ad->modifier) {
			return ad;
		}

	return NULL;
}

/**
 * Perform action when an accelerator key was pressed
 *
 * \param event IN key press event
 * \returns  TRUE if valid accelerator, FALSE if not
 */

wBool_t wlibHandleAccelKey(
        GdkEventKey *event)
{
	accelData_t * ad = wlibFindAccelKey(event);

	if (ad) {
		ad->action(ad->key, ad->data);
		return TRUE;
	}

	return FALSE;
}

/**
 * Add control to circular list of synonymous controls. Synonymous controls are kept in sync by
 * calling wControlLinkedActive for one member of the list
 *
 * \param b1 IN  first control
 * \param b2 IN  second control
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
 * \param b IN  control
 * \param active IN  state
 */

void wControlLinkedActive(wControl_p b, int active)
{
	wControl_p savePtr = b;

	if(!b) {
		return;
	}

	if (savePtr->type == B_MENUITEM) {
		wMenuPushEnable( savePtr, active);
	} else {
		wControlActive(savePtr, active);
	}

	savePtr = savePtr->synonym;

	while (savePtr && savePtr != b) {

		if (savePtr->type == B_MENUITEM) {
			wMenuPushEnable(savePtr, active);
		} else {
			wControlActive(savePtr, active);
		}

		savePtr = savePtr->synonym;
	}
}
