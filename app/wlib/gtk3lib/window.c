/** \file window.c
 * Basic window handling stuff.
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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#define MIN_WIDTH 100
#define MIN_HEIGHT 100

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"

#define MIN_WIN_WIDTH 150
#define MIN_WIN_HEIGHT 150

#define MIN_WIN_WIDTH_MAIN 400
#define MIN_WIN_HEIGHT_MAIN 400

#define SECTIONWINDOWSIZE  "gtklib window size"
#define SECTIONWINDOWPOS   "gtklib window pos"

extern wBool_t listHelpStrings;

static wBool_t gtkBlockEnabled = TRUE;


/*
 *****************************************************************************
 *
 * Window Utilities
 *
 *****************************************************************************
 */


/**
 * Returns the dimensions of \a win.
 *
 * \param win IN window handle
 * \param width OUT width of window
 * \param height OUT height of window minus menu bar size
 */

void wWinGetSize(
        wControl_p win,		/* Window */
        wWinPix_t * width,		/* Returned window width */
        wWinPix_t * height)	/* Returned window height */
{
	GtkRequisition requisition;

	gtk_widget_get_preferred_size(win->widget, NULL, &requisition);

	*width = requisition.width;
	*height = requisition.height;
}

/**
 * Sets the dimensions of window
 *
 * \param win IN window
 * \param width IN new width
 * \param height IN new height
 */

void wWinSetSize(
        wControl_p win,		/* Window */
        wWinPix_t width,		/* Window width */
        wWinPix_t height)		/* Window height */
{
	if (win->attributes.window.option&F_RESIZE) {
		gtk_window_resize(GTK_WINDOW(win->widget), width, height);
	} else {
		width = width ? width : -1;
		height = height ? height : -1;
		gtk_widget_set_size_request(win->widget, width, height);
	}
}

/**
 * Constrain the window's aspect ratio to match the layout dimensions.
 */
void wWinSetAspectRatio(wControl_p win, wWinPix_t x, wWinPix_t y)
{
	if (!win) {
		return;
	}

	GdkGeometry geo;
	geo.min_aspect = (gdouble)x / (gdouble)y;
	geo.max_aspect = geo.min_aspect;

	gtk_window_set_geometry_hints(
	        GTK_WINDOW(win->widget),
	        NULL,                          /* geometry_widget - not needed */
	        &geo,
	        GDK_HINT_ASPECT
	);
}

/**
 * Shows or hides window \a win.
 *
 * \param win IN window
 * \param show IN visibility state
 */

void wWinShow(
        wControl_p win,		/* Window */
        wBool_t show)		/* Command */
{
	wBool_t newState = show & ~DONTGRABFOCUS;
	if (newState) {

		gtk_widget_show(win->widget);

	} else {
		gtk_widget_hide(win->widget);
	}

}

/**
 * Block windows against user interactions. Done during demo mode etc.
 *
 * \param enabled IN blocked if TRUE
 */

void wWinBlockEnable(
        wBool_t enabled)
{
	gtkBlockEnabled = enabled;
}

/**
 * Returns whether the window is visible.
 *
 * \todo this is a dummy, use GTK to query the visibility
 *
 * \param win IN window
 * \return    TRUE if visible, FALSE otherwise
 */

wBool_t wWinIsVisible(
        wControl_p win)
{
	return(TRUE);

}

/**
 * Sets the title of \a win to \a title.
 *
 * \param win IN window
 * \param title IN new title
 */

void wWinSetTitle(
        wControl_p win,		/* Window */
        const char * title)		/* New title */
{
	gtk_window_set_title(GTK_WINDOW(win->widget), title);
}

/**
 * Sets the window \a win to busy or not busy. Sets the cursor accordingly
 *
 * \param win IN window
 * \param busy IN TRUE if busy, FALSE otherwise
 */

void wWinSetBusy(
        wControl_p win,		/* Window */
        wBool_t busy)		/* Command */
{
	GdkCursor * cursor;

	g_assert(win->widget != NULL);

	if (busy) {
		GdkDisplay * display = gdk_display_get_default();
		cursor = gdk_cursor_new_for_display(display,GDK_WATCH);
	} else {
		cursor = NULL;
	}

	gdk_window_set_cursor(gtk_widget_get_window(win->widget), cursor);

	if (cursor) {
		g_object_unref(cursor);
	}

	gtk_widget_set_sensitive(GTK_WIDGET(win->widget), busy==0);
}


/**
 * Returns the Title of \a win.
 *
 * \param win IN window
 * \return    pointer to window title
 */

const char * wWinGetTitle(
        wControl_p win)			/* Window */
{
	return gtk_window_get_title(GTK_WINDOW(win->widget));
}


void wSetGeometry(wControl_p win, wWinPix_t min_width, wWinPix_t max_width,
                  wWinPix_t min_height, wWinPix_t max_height, wWinPix_t base_width,
                  wWinPix_t base_height, double aspect_ratio )
{
	GdkGeometry hints;
	GdkWindowHints hintMask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
	hints.min_width = min_width;
	hints.max_width = max_width;
	hints.min_height = min_height;
	hints.max_height = max_height;
	hints.min_aspect = hints.max_aspect = aspect_ratio;
	hints.base_width = base_width;
	hints.base_height = base_height;
	if( base_width != -1 && base_height != -1 ) {
		hintMask |= GDK_HINT_BASE_SIZE;
	}

	if(aspect_ratio > -1.0 ) {
		hintMask |= GDK_HINT_ASPECT;
	}

	gtk_window_set_geometry_hints(
	        GTK_WINDOW(win->widget),
	        win->widget,
	        &hints,
	        hintMask);
}

/**
 * Terminates the applicaton. Before closing the main window
 * call back is called with wQuit_e.
 *
 * \param rc IN exit code
 *
 * \todo Move to a more appropriate file (main or appwindow?)
 */


void wExit(int rc)		/* Application return code */
{
	wPrefFlush("");

	// this should work but doesn't
	//g_application_quit(G_APPLICATION(wlibGetApp()));

	// use brute force instead
	exit(rc);

}
