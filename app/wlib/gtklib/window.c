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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

wWin_p gtkMainW;

#define MIN_WIN_WIDTH 150
#define MIN_WIN_HEIGHT 150

#define MIN_WIN_WIDTH_MAIN 400
#define MIN_WIN_HEIGHT_MAIN 400

#define SECTIONWINDOWSIZE  "gtklib window size"
#define SECTIONWINDOWPOS   "gtklib window pos"

extern wBool_t listHelpStrings;

static wControl_p firstWin = NULL, lastWin;
static int keyState;
static wBool_t gtkBlockEnabled = TRUE;
static wBool_t maximize_at_next_show = FALSE;

/*
 *****************************************************************************
 *
 * Window Utilities
 *
 *****************************************************************************
 */

/**
 * Get the "monitor" size for the window (strictly the nearest or most used monitor)
 * Note in OSX there is one giant virtual monitor so this doesn't work to force resize down...
 *
 */

static GdkRectangle getMonitorDimensions(GtkWidget * widget) {

	GdkRectangle monitor_dimensions;

	GdkScreen *screen = NULL;

	int monitor;

	GtkWidget * toplevel = gtk_widget_get_toplevel(widget);

	if (gtk_widget_is_toplevel(GTK_WIDGET(toplevel)) &&
		gtk_widget_get_parent_window(GTK_WIDGET(toplevel))) {

		GdkWindow * window = GDK_WINDOW(gtk_widget_get_parent_window(GTK_WIDGET(toplevel)));

		screen = gdk_window_get_screen(GDK_WINDOW(window));

		monitor = gdk_screen_get_monitor_at_window(screen,GDK_WINDOW(window));

	} else {

		screen = gdk_screen_get_default();

		monitor = gdk_screen_get_primary_monitor(screen);

	}

	gdk_screen_get_monitor_geometry(screen,monitor,&monitor_dimensions);


	return monitor_dimensions;
}

/**
 * Get the window size from the resource (.rc) file.  The size is saved under the key
 * SECTIONWINDOWSIZE.window name
 *
 * \param win IN window
 * \param nameStr IN window name
 */

static void getWinSize(wWin_p win, const char * nameStr)
{
    int w=50, h=50;
    const char *cp;
    char *cp1, *cp2;


    /*
     * Clamp window to be no bigger than one monitor size (to start - the user can always maximize)
     */

    GdkRectangle monitor_dimensions = getMonitorDimensions(GTK_WIDGET(win->gtkwin));

    wPos_t maxDisplayWidth = monitor_dimensions.width-10;
    wPos_t maxDisplayHeight = monitor_dimensions.height-50;



    if ((win->option&F_RECALLSIZE) &&
            (win->option&F_RECALLPOS) &&
            (cp = wPrefGetString(SECTIONWINDOWSIZE, nameStr)) &&
            (w = strtod(cp, &cp1), cp != cp1) &&
            (h = strtod(cp1, &cp2), cp1 != cp2)) {
    	win->option &= ~F_AUTOSIZE;

		if (w < 50) {
			w = 50;
		}

		if (h < 50) {
			h = 50;
		}
    }

	if (w > maxDisplayWidth) w = maxDisplayWidth;
	if (h > maxDisplayHeight) h = maxDisplayHeight;

	if (w<MIN_WIDTH) w = MIN_WIDTH;
	if (h<MIN_HEIGHT) h = MIN_HEIGHT;

	win->w = win->origX = w;
	win->h = win->origY = h;

}

/**
 * Save the window size to the resource (.rc) file.  The size is saved under the key
 * SECTIONWINDOWSIZE.window name
 *
 * \param win IN window
 */

static void saveSize(wWin_p win)
{

    if ((win->option&F_RECALLSIZE) &&
            gtk_widget_get_visible(GTK_WIDGET(win->gtkwin))) {
        char pos_s[20];

        sprintf(pos_s, "%d %d", win->w,
                win->h-(BORDERSIZE + ((win->option&F_MENUBAR)?MENUH:0)));
        wPrefSetString(SECTIONWINDOWSIZE, win->nameStr, pos_s);
    }
}

/**
 * Get the window position from the resource (.rc) file.  The position is saved under the key
 * SECTIONWINDOWPOS.window name
 *
 * \param win IN window
 */

static void getPos(wWin_p win)
{
    char *cp1, *cp2;
    GdkRectangle monitor_dimensions = getMonitorDimensions(GTK_WIDGET(win->gtkwin));

    if ((win->option&F_RECALLPOS) && (!win->shown)) {
        const char *cp;

        if ((cp = wPrefGetString(SECTIONWINDOWPOS, win->nameStr))) {
            int x, y;

            x = strtod(cp, &cp1);

            if (cp == cp1) {
                return;
            }

            y = strtod(cp1, &cp2);

            if (cp2 == cp1) {
                return;
            }

            if (y > monitor_dimensions.height+monitor_dimensions.y-win->h) {
                y = monitor_dimensions.height+monitor_dimensions.y-win->h;
            }

            if (x > monitor_dimensions.width+monitor_dimensions.x-win->w) {
                x = monitor_dimensions.width+monitor_dimensions.x-win->w;
            }

            if (x <= 0) {
                x = 1;
            }

            if (y <= 0) {
                y = 1;
            }

            gtk_window_move(GTK_WINDOW(win->gtkwin), x, y);
            //gtk_window_resize(GTK_WINDOW(win->gtkwin), win->w, win->h);
        }
    }
}

/**
 * Save the window position to the resource (.rc) file.  The position is saved under the key
 * SECTIONWINDOWPOS.window name
 *
 * \param win IN window
 */

static void savePos(wWin_p win)
{
    int x, y;

    if ((win->option&F_RECALLPOS)) {
        char pos_s[20];

        gdk_window_get_position(gtk_widget_get_window(GTK_WIDGET(win->gtkwin)), &x, &y);
        x -= 5;
        y -= 25;
        sprintf(pos_s, "%d %d", x, y);
        wPrefSetString(SECTIONWINDOWPOS, win->nameStr, pos_s);
    }
}

/**
 * Returns the dimensions of <win>.
 *
 * \param win IN window handle
 * \param width OUT width of window
 * \param height OUT height of window minus menu bar size
 */

void wWinGetSize(
    wWin_p win,		/* Window */
    wPos_t * width,		/* Returned window width */
    wPos_t * height)	/* Returned window height */
{
    GtkRequisition requisition;
    wPos_t w, h;
    gtk_widget_size_request(win->gtkwin, &requisition);
    w = win->w;
    h = win->h;

    if (win->option&F_AUTOSIZE) {
        if (win->realX > w) {
            w = win->realX;
        }

        if (win->realY > h) {
            h = win->realY;
        }
    }

    *width = w;
    *height = h - BORDERSIZE - ((win->option&F_MENUBAR)?win->menu_height:0);
}

/**
 * Sets the dimensions of window
 *
 * \param win IN window
 * \param width IN new width
 * \param height IN new height
 */

void wWinSetSize(
    wWin_p win,		/* Window */
    wPos_t width,		/* Window width */
    wPos_t height)		/* Window height */
{
    win->busy = TRUE;
    win->w = width;
   win->h = height + BORDERSIZE + ((win->option&F_MENUBAR)?MENUH:0);
    if (win->option&F_RESIZE) {
       	gtk_window_resize(GTK_WINDOW(win->gtkwin), win->w, win->h);
    	gtk_widget_set_size_request(win->widget, win->w-10, win->h-10);
    }
    else {
    	gtk_widget_set_size_request(win->gtkwin, win->w, win->h);
    	gtk_widget_set_size_request(win->widget, win->w, win->h);
    }


    win->busy = FALSE;
}

/**
 * Shows or hides window <win>. If <win> is created with 'F_BLOCK' option then the applications other
 * windows are disabled and 'wWinShow' doesnot return until the window <win> is closed by calling
 * 'wWinShow(<win>,FALSE)'.
 *
 * \param win IN window
 * \param show IN visibility state
 */

void wWinShow(
    wWin_p win,		/* Window */
    wBool_t show)		/* Command */
{
    //GtkRequisition min_req, pref_req;

    if (debugWindow >= 2) {
        printf("Set Show %s\n", win->labelStr?win->labelStr:"No label");
    }

    if (win->widget == 0) {
        abort();
    }

    int width, height;

    if (show) {
        keyState = 0;
        getPos(win);

        if (!win->shown) {
			gtk_widget_show(win->gtkwin);
			gtk_widget_show(win->widget);
		}

        if (win->option & F_AUTOSIZE) {
        	GtkAllocation allocation;
        	GtkRequisition requistion;
        	gtk_widget_size_request(win->widget,&requistion);

        	width = win->w;
        	height = win->h;

            if (requistion.width != width || requistion.height != height ) {

				width = requistion.width;
				height = requistion.height;

            	win->w = width;
            	win->h = height;


            	gtk_window_set_resizable(GTK_WINDOW(win->gtkwin),TRUE);

                if (win->option&F_MENUBAR) {
                    gtk_widget_set_size_request(win->menubar, win->w-20, MENUH);

                    gtk_widget_get_allocation(win->menubar, &allocation);
                    win->menu_height = allocation.height;
                }
            }
            gtk_window_resize(GTK_WINDOW(win->gtkwin), width+10, height+10);
        }




        gdk_window_raise(gtk_widget_get_window(win->gtkwin));

        if (win->shown && win->modalLevel > 0) {
            gtk_widget_set_sensitive(GTK_WIDGET(win->gtkwin), TRUE);
        }

        win->shown = show;
        win->modalLevel = 0;

        if ((!gtkBlockEnabled) || (win->option & F_BLOCK) == 0) {
            wFlush();
        } else {
            wlibDoModal(win, TRUE);
        }
        if (maximize_at_next_show) {
        	gtk_window_maximize(GTK_WINDOW(win->gtkwin));
        	maximize_at_next_show = FALSE;
        }
    } else {
        wFlush();
        saveSize(win);
        savePos(win);
        win->shown = show;

        if (gtkBlockEnabled && (win->option & F_BLOCK) != 0) {
            wlibDoModal(win, FALSE);
        }

        gtk_widget_hide(win->gtkwin);
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
 * \param win IN window
 * \return    TRUE if visible, FALSE otherwise
 */

wBool_t wWinIsVisible(
    wWin_p win)
{
    return win->shown;
}

/**
 * Returns whether the window is maximized.
 *
 * \param win IN window
 * \return    TRUE if maximized, FALSE otherwise
 */

wBool_t wWinIsMaximized(wWin_p win)
{
    return win->maximize_initially;
}

/**
 * Sets the title of <win> to <title>.
 *
 * \param varname1 IN window
 * \param varname2 IN new title
 */

void wWinSetTitle(
    wWin_p win,		/* Window */
    const char * title)		/* New title */
{
    gtk_window_set_title(GTK_WINDOW(win->gtkwin), title);
}

/**
 * Sets the window <win> to busy or not busy. Sets the cursor accordingly
 *
 * \param varname1 IN window
 * \param varname2 IN TRUE if busy, FALSE otherwise
 */

void wWinSetBusy(
    wWin_p win,		/* Window */
    wBool_t busy)		/* Command */
{
    GdkCursor * cursor;

    if (win->gtkwin == 0) {
        abort();
    }

    if (busy) {
        cursor = gdk_cursor_new(GDK_WATCH);
    } else {
        cursor = NULL;
    }

    gdk_window_set_cursor(gtk_widget_get_window(win->gtkwin), cursor);

    if (cursor) {
        gdk_cursor_unref(cursor);
    }

    gtk_widget_set_sensitive(GTK_WIDGET(win->gtkwin), busy==0);
}

/**
 * Set the modality of window. All visible windows are disabled when
 * the window is modal. These windows are enabled again, when window
 * is not modal. Disabling can be performed several times and enabling
 * has to be repeated as well to re-enable a window.
 * \todo Give this recursive enabling/disabling some thought and remove
 *
 * \param win0 IN window
 * \param modal IN TRUE if window is application modal, FALSE otherwise
 * \return
 */

void wlibDoModal(
    wWin_p win0,
    wBool_t modal)
{
    wWin_p win;

    for (win=(wWin_p)firstWin; win; win=(wWin_p)win->next) {
        if (win->shown && win != win0) {
            if (modal) {
                if (win->modalLevel == 0) {
                    gtk_widget_set_sensitive(GTK_WIDGET(win->gtkwin), FALSE);
                }

                win->modalLevel++;
            } else {
                if (win->modalLevel > 0) {
                    win->modalLevel--;

                    if (win->modalLevel == 0) {
                        gtk_widget_set_sensitive(GTK_WIDGET(win->gtkwin), TRUE);
                    }
                }
            }

            if (win->modalLevel < 0) {
                fprintf(stderr, "DoModal: %s modalLevel < 0",
                        win->nameStr?win->nameStr:"<NULL>");
                abort();
            }
        }
    }

    if (modal) {
        gtk_main();
    } else {
        gtk_main_quit();
    }
}

/**
 * Returns the Title of <win>.
 *
 * \param win IN window
 * \return    pointer to window title
 */

const char * wWinGetTitle(
    wWin_p win)			/* Window */
{
    return win->labelStr;
}


void wWinClear(
    wWin_p win,
    wPos_t x,
    wPos_t y,
    wPos_t width,
    wPos_t height)
{
}


void wWinDoCancel(
    wWin_p win)
{
    wControl_p b;

    for (b=win->first; b; b=b->next) {
        if ((b->type == B_BUTTON) && (b->option & BB_CANCEL)) {
            wlibButtonDoAction((wButton_p)b);
        }
    }
}

/*
 ******************************************************************************
 *
 * Call Backs
 *
 ******************************************************************************
 */

static int window_redraw(
    wWin_p win,
    wBool_t doWinProc)
{
    wControl_p b;

    if (win==NULL) {
        return FALSE;
    }

    for (b=win->first; b != NULL; b = b->next) {
        if (b->repaintProc) {
            b->repaintProc(b);
        }
    }

    return FALSE;
}

static gint window_delete_event(
    GtkWidget *widget,
    GdkEvent *event,
    wWin_p win)
{
    wControl_p b;
    /* if you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal.  Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit ?'
     * type dialogs. */

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete_event". */

    for (b = win->first; b; b=b->next)
        if (b->doneProc) {
            b->doneProc(b);
        }

    if (win->winProc) {
        win->winProc(win, wClose_e, NULL, win->data);

        if (win != gtkMainW) {
            wWinShow(win, FALSE);
        }
    }

    return (TRUE);
}

static int fixed_expose_event(
    GtkWidget * widget,
    GdkEventExpose * event,
    wWin_p win)
{
	int rc;

    if (event->count==0) {
        rc = window_redraw(win, TRUE);
    } else {
        rc = FALSE;
    }
    cairo_t* cr = gdk_cairo_create (gtk_widget_get_window(widget));
#ifdef CURSOR_SURFACE
    if (win && win->cursor_surface.surface && win->cursor_surface.show) {
		cairo_set_source_surface(cr,win->cursor_surface.surface,event->area.x, event->area.y);
		cairo_set_operator(cr,CAIRO_OPERATOR_OVER);
		cairo_rectangle(cr,event->area.x, event->area.y,
				event->area.width, event->area.height);
		cairo_fill(cr);
	}
#endif
    return rc;
}

static int resizeTime(wWin_p win) {

	if (win->resizeW == win->w && win->resizeH == win->h) {  // If hasn't changed since last
		if (win->timer_idle_count>3) {
			win->winProc(win, wResize_e, NULL, win->data);  //Trigger Redraw on last occasion if one-third of a second has elapsed
			win->timer_idle_count = 0;
			win->resizeTimer = 0;
			win->timer_busy_count = 0;
			return FALSE;						 //Stop Timer and don't resize
		} else win->timer_idle_count++;
	}
	if (win->busy==FALSE && win->winProc) {   //Always drive once
		if (win->timer_busy_count>10) {
			 win->winProc(win, wResize_e, NULL, win->data); //Redraw if ten times we saw a change (1 sec)
			 win->timer_busy_count = 0;
		} else {
			win->winProc(win, wResize_e, (void*) 1, win->data); //No Redraw
			win->timer_busy_count++;
		}
	    win->resizeW = win->w;					//Remember this one
	    win->resizeH = win->h;
	}
	return TRUE;							//Will redrive after another timer interval
}

static int window_configure_event(
    GtkWidget * widget,
    GdkEventConfigure * event,
    wWin_p win)
{

    if (win==NULL) {
        return FALSE;
    }

    if (win->option&F_RESIZE) {
        if (event->width < 10 || event->height < 10) {
            return TRUE;
        }
        int w = win->w;
        int h = win->h;


        if (win->w != event->width || win->h != event->height) {
            win->w = event->width;
            win->h = event->height;

            if (win->w < MIN_WIN_WIDTH) {
                win->w = MIN_WIN_WIDTH;
            }

            if (win->h < MIN_WIN_HEIGHT) {
                win->h = MIN_WIN_HEIGHT;
            }

            if (win->option&F_MENUBAR) {
            	GtkAllocation allocation;
            	gtk_widget_get_allocation(win->menubar, &allocation);
            	win->menu_height= allocation.height;
                gtk_widget_set_size_request(win->menubar, win->w-20, win->menu_height);
            }
            if (win->resizeTimer) {					// Already have a timer
                 return FALSE;
            } else {
            	 win->resizeW = w;				//Remember where this started
            	 win->resizeH = h;
            	 win->timer_idle_count = 0;          //Start background timer on redraw
            	 win->timer_busy_count = 0;
                 win->resizeTimer = g_timeout_add(100,(GSourceFunc)resizeTime,win);   // 100ms delay
                 return FALSE;
            }
        }
    }

    return FALSE;
}

/**
 * Event handler for window state changes (maximize)
 * Handles maximize event by storing the new state in the internal structure and
 * calling the event handler
 *
 * \param widget gtk widget
 * \param event event information
 * \param win the wlib internal window data
 * \return TRUE if win is valid,
 */

gboolean window_state_event(
    GtkWidget *widget,
    GdkEventWindowState *event,
    wWin_p win)
{
    if (!win) {
        return (FALSE);
    }

    win->maximize_initially = FALSE;

    if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) {
        win->maximize_initially = TRUE;
    }

    if (win->busy==FALSE && win->winProc) {
        win->winProc(win, wState_e, NULL, win->data);
    }

    return TRUE;
}
/**
 * Get current state of shift, ctrl or alt keys.
 *
 * \return    or'ed value of WKEY_SHIFT, WKEY_CTRL and WKEY_ALT depending on state
 */

int wGetKeyState(void)
{
    return keyState;
}

wBool_t catch_shift_ctrl_alt_keys(
    GtkWidget * widget,
    GdkEventKey *event,
    void * data)
{
    int state = 0;

    switch (event->keyval ) {
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
        state |= WKEY_SHIFT;
        break;

    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
        state |= WKEY_CTRL;
        break;

    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
        state |= WKEY_ALT;
        break;
    }

    if (state != 0) {
        if (event->type == GDK_KEY_PRESS) {
            keyState |= state;
        } else {
            keyState &= ~state;
        }

        return TRUE;
    }

    return FALSE;
}

static gint window_char_event(
    GtkWidget * widget,
    GdkEventKey *event,
    wWin_p win)
{
    wControl_p bb;

    if (catch_shift_ctrl_alt_keys(widget, event, win)) {
        return FALSE;
    }

    if (event->type == GDK_KEY_RELEASE) {
        return FALSE;
    }

    if ( ( event->state & GDK_MODIFIER_MASK ) == 0 ) {
        if (event->keyval == GDK_KEY_Escape) {
            for (bb=win->first; bb; bb=bb->next) {
                if (bb->type == B_BUTTON && (bb->option&BB_CANCEL)) {
                    wlibButtonDoAction((wButton_p)bb);
                    return TRUE;
                }
            }
        }
    }

    if (wlibHandleAccelKey(event)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void wSetGeometry(wWin_p win, int min_width, int max_width, int min_height, int max_height, int base_width, int base_height, double aspect_ratio ) {
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
			GTK_WINDOW(win->gtkwin),
			win->gtkwin,
			&hints,
			hintMask);
}


/*
 *******************************************************************************
 *
 * Main and Popup Windows
 *
 *******************************************************************************
 */


/**
 * Create a window.
 * Default width and height are replaced by values stored in the configuration
 * file (.rc)
 *
 * \param parent IN parent window
 * \param winType IN type of window
 * \param x IN default width
 * \param y IN default height
 * \param labelStr IN window title
 * \param nameStr IN name of window
 * \param option IN misc options for placement and sizing of window
 * \param winProc IN window procedure
 * \param data IN additional data to pass to the window procedure
 * \return  the newly created window
 */

static wWin_p wWinCommonCreate(
    wWin_p parent,
    int winType,
    wPos_t x,
    wPos_t y,
    const char * labelStr,
    const char * nameStr,
    long option,
    wWinCallBack_p winProc,
    void * data)
{
    wWin_p w;
    int h;
    w = wlibAlloc(NULL, winType, x, y, labelStr, sizeof *w, data);
    w->busy = TRUE;
    w->option = option;
	w->resizeTimer = 0;


    h = BORDERSIZE;

    if (w->option&F_MENUBAR) {
        h += MENUH;
    }

    if (winType == W_MAIN) {
        w->gtkwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    } else {
        w->gtkwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);

        if (gtkMainW) {
            gtk_window_set_transient_for(GTK_WINDOW(w->gtkwin),
                                         GTK_WINDOW(gtkMainW->gtkwin));
        }
    }
    getWinSize(w, nameStr);
    if (winType != W_MAIN) {
            gtk_widget_set_app_paintable (w->gtkwin,TRUE);
    }

    if (option & F_HIDE) {
        gtk_widget_hide(w->gtkwin);
    }

    /* center window on top of parent window */
    if (option & F_CENTER) {
        gtk_window_set_position(GTK_WINDOW(w->gtkwin), GTK_WIN_POS_CENTER_ON_PARENT);
    }


    w->widget = gtk_fixed_new();

    if (w->widget == 0) {
        abort();
    }

    if (w->option&F_MENUBAR) {
        w->menubar = gtk_menu_bar_new();
        gtk_container_add(GTK_CONTAINER(w->widget), w->menubar);
        gtk_widget_show(w->menubar);
        GtkAllocation allocation;
        gtk_widget_get_allocation(w->menubar, &allocation);
        w->menu_height = allocation.height;
        gtk_widget_set_size_request(w->menubar, -1, w->menu_height);
    }

    gtk_container_add(GTK_CONTAINER(w->gtkwin), w->widget);




    if (w->option&F_AUTOSIZE) {
        w->realX = 0;
        w->w = MIN_WIN_WIDTH+20;
        w->realY = h;
        w->h = MIN_WIN_HEIGHT;
    } else if (w->origX != 0){
        w->realX = w->origX;
        w->realY = w->origY+h;

        w->default_size_x = w->w;
        w->default_size_y = w->h;
        //gtk_widget_set_size_request(w->widget, w->w-20, w->h);

        if (w->option&F_MENUBAR) {
            gtk_widget_set_size_request(w->menubar, w->w-20, MENUH);
        }
    }
    int scr_w, scr_h;
	wGetDisplaySize(&scr_w, &scr_h);
	if (scr_w < MIN_WIN_WIDTH) scr_w = MIN_WIN_WIDTH+10;
	if (scr_h < MIN_WIN_HEIGHT) scr_h = MIN_WIN_HEIGHT;
	if (winType != W_MAIN) {
		wSetGeometry(w, MIN_WIN_WIDTH, scr_w-10, MIN_WIN_HEIGHT, scr_h, -1, -1, -1);
	} else {
		if (scr_w < MIN_WIN_WIDTH_MAIN+10) scr_w = MIN_WIN_WIDTH_MAIN+200;
		if (scr_h < MIN_WIN_HEIGHT_MAIN+10) scr_h = MIN_WIN_HEIGHT_MAIN+200;
		wSetGeometry(w, MIN_WIN_WIDTH_MAIN, scr_w-10, MIN_WIN_HEIGHT_MAIN, scr_h-10, -1, -1, -1);
     }



    w->first = w->last = NULL;
    w->winProc = winProc;
    g_signal_connect(GTK_OBJECT(w->gtkwin), "delete_event",
                     G_CALLBACK(window_delete_event), w);
    g_signal_connect(GTK_OBJECT(w->widget), "expose_event",
                     G_CALLBACK(fixed_expose_event), w);
    g_signal_connect(GTK_OBJECT(w->gtkwin), "configure_event",
                     G_CALLBACK(window_configure_event), w);
    g_signal_connect(GTK_OBJECT(w->gtkwin), "window-state-event",
                     G_CALLBACK(window_state_event), w);
    g_signal_connect(GTK_OBJECT(w->gtkwin), "key_press_event",
                     G_CALLBACK(window_char_event), w);
    g_signal_connect(GTK_OBJECT(w->gtkwin), "key_release_event",
                     G_CALLBACK(window_char_event), w);
    gtk_widget_set_events(w->widget, GDK_EXPOSURE_MASK);
    gtk_widget_set_events(GTK_WIDGET(w->gtkwin),
                          GDK_EXPOSURE_MASK|GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK);

    if (w->option & F_RESIZE) {
        gtk_window_set_resizable(GTK_WINDOW(w->gtkwin), TRUE);
        gtk_window_resize(GTK_WINDOW(w->gtkwin), w->w, w->h);
    } else {
        gtk_window_set_resizable(GTK_WINDOW(w->gtkwin), FALSE);
    }

    w->lastX = 0;
    w->lastY = h;
    w->shown = FALSE;
    w->nameStr = nameStr?strdup(nameStr):NULL;

    if (labelStr) {
        gtk_window_set_title(GTK_WINDOW(w->gtkwin), labelStr);
    }

    if (listHelpStrings) {
        printf("WINDOW - %s\n", nameStr?nameStr:"<NULL>");
    }

    if (firstWin) {
        lastWin->next = (wControl_p)w;
    } else {
        firstWin = (wControl_p)w;
    }

    lastWin = (wControl_p)w;
    gtk_widget_show(w->widget);
    gtk_widget_realize(w->gtkwin);
    GtkAllocation allocation;
    gtk_widget_get_allocation(w->gtkwin, &allocation);
    w->menu_height = allocation.height;

    w->busy = FALSE;

    if (option&F_MAXIMIZE) {
    	maximize_at_next_show = TRUE;
    }

    return w;
}


/**
 * Initialize the application's main window. This function does the necessary initialization
 * of the application including creation of the main window.
 *
 * \param name IN internal name of the application. Used for filenames etc.
 * \param x    IN Initial window width
 * \param y    IN Initial window height
 * \param helpStr IN Help topic string
 * \param labelStr IN window title
 * \param nameStr IN Window name
 * \param option IN options for window creation
 * \param winProc IN pointer to main window procedure
 * \param data IN User context
 * \return    window handle or NULL on error
 */

wWin_p wWinMainCreate(
    const char * name,		/* Application name */
    wPos_t x,				/* Initial window width */
    wPos_t y,				/* Initial window height */
    const char * helpStr,	/* Help topic string */
    const char * labelStr,	/* Window title */
    const char * nameStr,	/* Window name */
    long option,			/* Options */
    wWinCallBack_p winProc,	/* Call back function */
    void * data)			/* User context */
{
    char *pos;
    long isMaximized;

    pos = strchr(name, ';');

    if (pos) {
        /* if found, split application name and configuration name */
        strcpy(wConfigName, pos + 1);
    } else {
        /* if not found, application name and configuration name are same */
        strcpy(wConfigName, name);
    }

    wPrefGetInteger("draw", "maximized", &isMaximized, 0);
    option = option | (isMaximized?F_MAXIMIZE:0);

    gtkMainW = wWinCommonCreate(NULL, W_MAIN, x, y, labelStr, nameStr, option,
                                winProc, data);

    wDrawColorWhite = wDrawFindColor(0xFFFFFF);
    wDrawColorBlack = wDrawFindColor(0x000000);
    return gtkMainW;
}

/**
 * Create a new popup window.
 *
 * \param parent IN Parent window (may be NULL)
 * \param x IN Initial window width
 * \param y IN Initial window height
 * \param helpStr IN Help topic string
 * \param labelStr IN Window title
 * \param nameStr IN Window name
 * \param option IN Options
 * \param winProc IN call back function
 * \param data IN User context information
 * \return    handle for new window
 */

wWin_p wWinPopupCreate(
    wWin_p parent,
    wPos_t x,
    wPos_t y,
    const char * helpStr,
    const char * labelStr,
    const char * nameStr,
    long option,
    wWinCallBack_p winProc,
    void * data)
{
    wWin_p win;

    if (parent == NULL) {
        if (gtkMainW == NULL) {
            abort();
        }

        parent = gtkMainW;
    }

    win = wWinCommonCreate(parent, W_POPUP, x, y, labelStr, nameStr, option,
                           winProc, data);
    return win;
}


/**
 * Terminates the applicaton with code <rc>. Before closing the main window
 * call back is called with wQuit_e. The program is terminated without exiting
 * the main message loop.
 *
 * \param rc IN exit code
 * \return    never returns
 */


void wExit(
    int rc)		/* Application return code */
{
    wWin_p win;

    for (win = (wWin_p)firstWin; win; win = (wWin_p)win->next) {
        if (gtk_widget_get_visible(GTK_WIDGET(win->gtkwin))) {
            saveSize(win);
            savePos(win);
        }
    }

    wPrefFlush();

    if (gtkMainW && gtkMainW->winProc != NULL) {
        gtkMainW->winProc(gtkMainW, wQuit_e, NULL, gtkMainW->data);
    }

    exit(rc);
}
