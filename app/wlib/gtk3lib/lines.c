/** \file lines.c
 * Window for drawing simple line drawing
 */

/* 	XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis,
 *                2016 Martin Fischer <m_fischer@sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <cairo.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

/*
 *****************************************************************************
 *
 * Lines
 *
 *****************************************************************************
 */

struct wLine_t {
    WOBJ_COMMON
    wBool_t visible;
    int count;
    wLines_t * lines;
};

/**
 * Perform redrawing of the lines window
 *
 * \param b IN window handle
 * \return
 */

static void linesRepaint(wControl_p b)
{
    wLine_p bl = (wLine_p)(b);
    int i;
    wWin_p win = (wWin_p)(bl->parent);
    GdkWindow * window;
    cairo_t *cr;

    if (!bl->visible) {
        return;
    }
    cairo_region_t * rect;
    GdkDrawingContext * context;

    if (win->cr) {       //In draw event
    	cr = win->cr;
    } else {
		window = gtk_widget_get_window(win->widget);
		cairo_rectangle_int_t r;
		r.width = win->w;
		r.height = win->h;
		r.x = win->realX;
		r.y = win->realY;
		rect = cairo_region_create_rectangle(&r);

		context = gdk_window_begin_draw_frame(window, rect);
		cr = gdk_drawing_context_get_cairo_context(context);
    }
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);

    for (i=0; i<bl->count; i++) {
        cairo_set_line_width(cr, bl->lines[i].width);
        cairo_move_to(cr, bl->lines[i].x0, bl->lines[i].y0);
        cairo_line_to(cr, bl->lines[i].x1, bl->lines[i].y1);
        cairo_stroke(cr);
    }
    if (!win->cr) {				//In draw event
		cairo_destroy(cr);
		g_object_unref(rect);
		gdk_window_end_draw_frame(window,context);
		g_object_unref(window);
    }
}

/**
 * Set visibility state of lines window
 *
 * \param bl IN window
 * \param visible IN new visibility state
 * \return
 */

void wlibLineShow(
    wLine_p bl,
    wBool_t visible)
{
    bl->visible = visible;
}

/**
 * Create a window consisting of several lines
 *
 * \param parent IN parent window
 * \param labelStr IN  label - unused
 * \param count IN number of lines
 * \param lines IN list of line coordinates
 * \return handle of new window
 */

wLine_p wLineCreate(
    wWin_p	parent,
    const char	* labelStr,
    int	count,
    wLines_t * lines)
{
    wLine_p linesWindow;
    int i;
    linesWindow = (wLine_p)wlibAlloc(parent, B_LINES, 0, 0, labelStr,
                                    sizeof *linesWindow, NULL);
    linesWindow->visible = TRUE;
    linesWindow->count = count;
    linesWindow->lines = lines;
    linesWindow->w = linesWindow->h = 0;

    for (i=0; i<count; i++) {
        if (lines[i].x0 > linesWindow->w) {
            linesWindow->w = lines[i].x0;
        }

        if (lines[i].y0 > linesWindow->h) {
            linesWindow->h = lines[i].y0;
        }

        if (lines[i].x1 > linesWindow->w) {
            linesWindow->w = lines[i].x1;
        }

        if (lines[i].y1 > linesWindow->h) {
            linesWindow->h = lines[i].y1;
        }
    }

    linesWindow->repaintProc = linesRepaint;
    wlibAddButton((wControl_p)linesWindow);
    linesWindow->widget = NULL;
    return linesWindow;
}
