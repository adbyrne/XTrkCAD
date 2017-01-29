/** \file boxes.c
 * Window for drawing a rectangle
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

#include <cairo.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

struct wBox_t {
    WOBJ_COMMON
    wBoxType_e boxTyp;
};

#define B (1)
#define W (2)
#define SETCOLOR(ST, S, N ) \
		if (colors[ST][S][N] == B ) { \
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); \
		} else {	\
			cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); \
		}

/**
 * Set size of box window
 *
 * \param b IN window handle
 * \param w IN new width
 * \param h IN new height
 * \return
 *
 */

void wBoxSetSize(
    wBox_p b,	
    wPos_t w,	
    wPos_t h)	
{
    b->w = w;
    b->h = h;
}

/**
 * Draw the box
 * \todo too many strokes, remove and test
 *
 * \param win IN window handle
 * \param style IN frame style
 * \param x IN x position
 * \param y IN y position
 * \param w IN width
 * \param h IN height
 * \return
 */

void wlibDrawBox(
    wWin_p win,
    wBoxType_e style,
    wPos_t x,
    wPos_t y,
    wPos_t w,
    wPos_t h)
{
    wPos_t x0, y0, x1, y1;
    GdkDrawable * window;
    cairo_t *cr;
    static char colors[8][4][2] = {
        { /* ThinB  */ {B,0}, {B,0}, {B,0}, {B,0} },
        { /* ThinW  */ {W,0}, {W,0}, {W,0}, {W,0} },
        { /* AboveW */ {W,0}, {W,0}, {B,0}, {B,0} },
        { /* BelowW */ {B,0}, {B,0}, {W,0}, {W,0} },
        { /* ThickB */ {B,B}, {B,B}, {B,B}, {B,B} },
        { /* ThickW */ {W,W}, {W,W}, {W,W}, {W,W} },
        { /* RidgeW */ {W,B}, {W,B}, {B,W}, {B,W} },
        { /* TroughW*/ {B,W}, {B,W}, {W,B}, {W,B} }
    };
    window = gtk_widget_get_window(win->widget);
    cr = gdk_cairo_create(window);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_set_line_width(cr, 1.0);
    x0 = x;
    x1 = x+w;
    y0 = y;
    y1 = y+h;
    SETCOLOR(style, 0, 0);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x0, y1);
    cairo_stroke_preserve(cr);
    SETCOLOR(style, 1, 0);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x1, y0);
    cairo_stroke_preserve(cr);
    SETCOLOR(style, 2, 0);
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x0+1, y1);
    cairo_stroke_preserve(cr);
    SETCOLOR(style, 3, 0);
    cairo_move_to(cr, x1, y1-1);
    cairo_line_to(cr, x1, y0+1);
    cairo_stroke_preserve(cr);

    if (style < wBoxThickB) {
        return;
    }

    x0++;
    y0++;
    x1--;
    y1--;
    SETCOLOR(style, 0, 1);
    cairo_move_to(cr, x0, y0);
    cairo_line_to(cr, x0, y1);
    cairo_stroke_preserve(cr);
    SETCOLOR(style, 1, 1);
    cairo_move_to(cr, x0+1, y0);
    cairo_line_to(cr, x1, y0);
    cairo_stroke_preserve(cr);
    SETCOLOR(style, 2, 1);
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x0+1, y1);
    cairo_stroke_preserve(cr);
    SETCOLOR(style, 3, 1);
    cairo_move_to(cr, x1, y1-1);
    cairo_line_to(cr, x1, y0+1);
    cairo_stroke_preserve(cr);
    cairo_destroy(cr);
}

/**
 * Force repainting of box window
 *
 * \param b IN box window
 * \return
 */

static void boxRepaint(wControl_p b)
{
    wBox_p bb = (wBox_p)(b);
    wWin_p win = bb->parent;
    wlibDrawBox(win, bb->boxTyp, bb->realX, bb->realY, bb->w, bb->h);
}

/**
 * Create new box
 *
 * \param parent IN parent window
 * \param bx IN x position
 * \param by IN y position
 * \param labelStr IN label text (ignored)
 * \param boxTyp IN style
 * \param bw IN x width
 * \param by IN y width
 * \return window handle for newly created box
 */

wBox_p wBoxCreate(
    wWin_p	parent,
    wPos_t	bx,
    wPos_t	by,
    const char	* labelStr,
    wBoxType_e boxTyp,
    wPos_t	bw,
    wPos_t	bh)
{
    wBox_p b;
    b = (wBox_p)wlibAlloc(parent, B_BOX, bx, by, labelStr, sizeof *b, NULL);
    wlibComputePos((wControl_p)b);
    b->boxTyp = boxTyp;
    b->w = bw;
    b->h = bh;
    b->repaintProc = boxRepaint;
    wlibAddButton((wControl_p)b);
    return b;
}
