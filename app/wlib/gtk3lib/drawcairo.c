/** \file drawcairo.c
 * Basic drawing functions
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

#include "glib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef WIN32
#define _USE_MATH_DEFINES // for C
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <math.h>

#include <cairo.h>
#include <gtk/gtk.h>
#include "gdk-pixbuf/gdk-pixbuf.h"

// Trace low level drawing actions
int iDrawLog = 0;
long lDrawCnt = 0;

#include "gtkint.h"

// Hack to do TempRedraw or MainRedraw
// For Windows only
wBool_t wDrawDoTempDraw = TRUE;

struct wDrawBitMap_t {
	int w;
	int h;
	int x;
	int y;
	GdkPixbuf * pixbuf;
	cairo_pattern_t* pattern;
};

struct draw * psPrint_d;

#ifndef BATCH_LIMIT
#define BATCH_LIMIT 100
#endif
static unsigned int currCalls;
static cairo_t * cairoCtx;
static cairo_matrix_t base_matrix;

static void flush_path_if_open(struct draw *bd, cairo_t *cr);
static void setCairoDashStyle(cairo_t *cr, wDrawLineType_e lineType);
static cairo_t* prepare_drawing(struct draw *bd, wDrawWidth width,
                                wDrawLineType_e lineType,
                                wDrawColor color, wDrawOpts opts, int fill_mode);
static void finish_drawing(struct draw *bd, cairo_t *cr);

/*****************************************************************************
 *
 * MACROS
 *
 */

#define LBORDER (22)
#define RBORDER (6)
#define TBORDER (6)
#define BBORDER (20)

#define INMAPX(D,X)	(X)
#define INMAPY(D,Y)	(((D)->height-1) - (Y))
#define OUTMAPX(D,X)	(X)
#define OUTMAPY(D,Y)	(((D)->height-1) - (Y))


/*******************************************************************************
 *
 * Basic Drawing Functions
 *
*******************************************************************************/

static unsigned long GtkDrawSetColor(
        cairo_t * cairo,
        wDrawColor color,
        GdkRGBA *gcolor )
{

	gcolor->red = ((color&0xFF0000)>>16)/255.0;
	gcolor->green = ((color&0xFF00)>>8)/255.0;
	gcolor->blue  = (color&0xFF)/255.0;
	gcolor->alpha = 1.0;
	cairo_set_source_rgba(cairo, gcolor->red, gcolor->green, gcolor->blue, 1.0 );
	return color;
}

static cairo_t *gtkDrawCreateCairoContext(struct draw *bd,
                cairo_surface_t *surface,
                wDrawWidth width,
                wDrawLineType_e lineType,
                wDrawColor color, wDrawOpts opts)
{

	cairo_t *cairo;

	currCalls++;

	if (!cairoCtx) {
		if (surface) {
			cairo = cairo_create(surface);
		} else {
			if (opts & wDrawOptTemp) {
				if (!bd->bTempMode) {
					printf("Temp draw in Main Mode. Contact Developers. See %s:%d\n",
					       __FILE__, __LINE__ + 1);
				}
				/* Temp Draw In Main Mode:
				        You are seeing this message because there is a wDraw*() call on
				   tempD but you are not in the context of TempRedraw() Typically this
				   happens when Cmd<Object>() is processing a C_DOWN or C_MOVE action
				   and it writes directly to tempD Instead it sould set some state which
				   allows c_redraw to do the actual drawing If you set a break point on
				   the printf you'll see the offending wDraw*() call in the traceback It
				   should be sufficient to remove that draw code or move it to C_REDRAW
				        This is not fatal but the draw will be ineffective because the
				   next TempRedraw() will erase the temp surface before the expose event
				   can copy (or bitblt) it
				*/
				if (iDrawLog > 4) {
					printf("%ld: cairo_create temp\n", lDrawCnt++);
				}
				cairo = cairo_create(bd->temp_surface);
			} else {
				if (bd->bTempMode) {
					printf("Main draw in Temp Mode. Contact Developers. See %s:%d\n",
					       "gtkdraw-cairo.c", __LINE__ + 1);
				}
				/* Main Draw In Temp Mode:
				        You are seeing this message because there is a wDraw*() call on
				   mainD but you are in the context of TempRedraw() Typically this
				   happens when C_REDRAW action calls wDraw*() on mainD, in which case
				   it should be writing to tempD. Or the wDraw*() call should be removed
				   if it is redundant. If you set a break point on the printf you'll see
				   the offending wDraw*() call in the traceback This is not fatal but
				   could result in garbage being left on the screen if the command is
				   cancelled.
				*/
				if (iDrawLog > 4) {
					printf("%ld: cairo_create main\n", lDrawCnt++);
				}
				cairo = cairo_create(bd->surface);
			}

			cairo_set_line_cap(cairo, CAIRO_LINE_CAP_BUTT);
			cairo_set_line_join(cairo, CAIRO_LINE_JOIN_MITER);
			cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
			cairo_set_antialias(cairo, CAIRO_ANTIALIAS_FAST);

		}
	} else {
		/* Batching active: flush any pending path before legacy code
		 * changes cairo state (color, line width, dash, clip). */
		flush_path_if_open(bd, cairoCtx);
		cairo = cairoCtx;
		cairo_set_matrix(cairo, &base_matrix);

	}

	width = width ? abs(width) : 1;
	if (color == wDrawColorWhite) {
		width += 1; // Remove ghosts
	}
	cairo_set_line_width(cairo, width);


	setCairoDashStyle(cairo, lineType);


	GdkRGBA gcolor = wlibGetColor(color, TRUE);
	/*bd->lastColor = */
	GtkDrawSetColor(cairo, color, &gcolor);

	if (bd->clipregion) {
		cairo_rectangle(cairo, bd->clipregion->x, bd->clipregion->y,
		                bd->clipregion->w, bd->clipregion->h);
		cairo_clip(cairo);
	}



	return cairo;
}

wBool_t wDrawSetTempMode(
        wControl_p bd,
        wBool_t bTemp )
{

	wBool_t ret = bd->attributes.draw.bTempMode;
	bd->attributes.draw.bTempMode = bTemp;
	if ( ret == FALSE && bTemp == TRUE ) {
		// Main to Temp drawing
		wDrawClearTemp( bd );
	}
	return ret;
}

static cairo_t* gtkDrawDestroyCairoContext(cairo_t *cairo)
{
	if (cairo && cairo != cairoCtx) {
		cairo_destroy(cairo);
	}
	return NULL;
}

static void setCairoDashStyle(cairo_t *cr, wDrawLineType_e lineType)
{
	switch (lineType) {
	case wDrawLineSolid: cairo_set_dash(cr, 0, 0, 0); break;
	case wDrawLineDash:  { double d[] = {5, 3}; cairo_set_dash(cr, d, 2, 0); } break;
	case wDrawLineDot:   { double d[] = {1, 2}; cairo_set_dash(cr, d, 2, 0); } break;
	case wDrawLineDashDot: { double d[] = {5, 2, 1, 2}; cairo_set_dash(cr, d, 4, 0); } break;
	case wDrawLineDashDotDot: { double d[] = {5, 2, 1, 2, 1, 2}; cairo_set_dash(cr, d, 6, 0); } break;
	case wDrawLineCenter: { double d[] = {8, 3, 5, 3}; cairo_set_dash(cr, d, 4, 0.0); } break;
	case wDrawLinePhantom: { double d[] = {8, 3, 5, 3, 5, 3}; cairo_set_dash(cr, d, 6, 0.0); } break;
	}
}

static void flush_path_if_open(struct draw *bd, cairo_t *cr)
{
	if (bd->pathOpen) {
		if (bd->fill) {
			if ( iDrawLog >= 3 ) {
				printf( "%ld: flush batch: fill (%d ops)\n", lDrawCnt++, bd->batchCount );
			}
			cairo_fill(cr);
		} else {
			if ( iDrawLog >= 3 ) {
				printf( "%ld: flush batch: stroke (%d ops)\n", lDrawCnt++, bd->batchCount );
			}
			cairo_stroke(cr);
		}
		bd->pathOpen = FALSE;
		bd->batchCount = 0;
	}
}

/**
 * Prepare a cairo context for drawing.
 *
 * In batch mode (cairoCtx set by wDrawStart): reuses the shared context,
 * flushing the current path if draw attributes change or BATCH_LIMIT is hit.
 * In legacy mode: creates a one-shot cairo context via gtkDrawCreateCairoContext.
 *
 * fill_mode: TRUE for filled shapes, FALSE for stroked shapes.
 *            A change in fill_mode triggers a flush, so filled rects
 *            can be batched separately from stroked lines.
 */
static cairo_t*
prepare_drawing(struct draw *bd, wDrawWidth width, wDrawLineType_e lineType,
                wDrawColor color, wDrawOpts opts, int fill_mode)
{
	/* Legacy mode — no batching */
	if (!cairoCtx) {
		return gtkDrawCreateCairoContext(bd, NULL, width, lineType, color, opts);
	}

	/* Batch mode: flush if attributes changed or batch is full */
	if (bd->pathOpen &&
	    (bd->activeColor != (wDrawColor)color ||
	     bd->activeWidth != (double)width ||
	     bd->activeStyle != (wDrawLineType_e)lineType ||
	     bd->fill != fill_mode ||
	     bd->batchCount >= BATCH_LIMIT)) {
		flush_path_if_open(bd, cairoCtx);
	}

	/* Start a new batch segment if needed */
	if (!bd->pathOpen) {
		GdkRGBA gcolor;
		GtkDrawSetColor(cairoCtx, color, &gcolor);
		cairo_set_line_width(cairoCtx, width ? abs(width) : 1);
		setCairoDashStyle(cairoCtx, lineType);
		bd->activeColor = color;
		bd->activeWidth = width;
		bd->activeStyle = lineType;
		bd->fill = fill_mode;
		bd->pathOpen = TRUE;
	}
	return cairoCtx;
}

/**
 * Finish a single draw operation.
 *
 * In batch mode: increments the batch counter (actual stroke/fill deferred).
 * In legacy mode: destroys the one-shot cairo context immediately.
 */
static void finish_drawing(struct draw *bd, cairo_t *cr)
{
	if (cr == cairoCtx) {
		bd->batchCount++;
	} else {
		gtkDrawDestroyCairoContext(cr);
	}
}

#ifdef CURSOR_SURFACE
cairo_t* CreateCursorSurface(wControl_p ct, wSurface_p surface, wWinPix_t width,
                             wWinPix_t height, wDrawColor color, wDrawOpts opts)
{

	cairo_t * cairo = NULL;

	assert(surface);

	if ((opts&wDrawOptCursor) || (opts&wDrawOptCursorRmv)) {

		if ( surface->width != width || surface->height != height) {
			if (surface->surface) { cairo_surface_destroy(surface->surface); }
			surface->surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width,
			                   height );
			surface->width = width;
			surface->height = height;

		}

		cairo = gtkDrawCreateCairoCursorContext(ct,surface->surface,0,wDrawLineSolid,
		                                        color, opts);
		cairo_save(cairo);
		cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
		cairo_paint(cairo);
		cairo_restore(cairo);
		surface->show = TRUE;
		cairo_set_operator(cairo,CAIRO_OPERATOR_SOURCE);

	}

	return cairo;

}
#endif

void wDrawDelayUpdate(
        wControl_p bd,
        wBool_t delay )
{
	cairo_rectangle_int_t update_rect;

	if ( (!delay) && bd->attributes.draw.delayUpdate ) {
		if ( iDrawLog >= 1 ) { printf( "wDrawDelayUpdate( %d -> %d ) - update\n", bd->attributes.draw.delayUpdate, delay ); }
		update_rect.x = 0;
		update_rect.y = 0;
		wDrawGetSize(bd, (wWinPix_t *)&update_rect.width,
		             (wWinPix_t *)&update_rect.height);
		cairo_region_t * cairo_region = cairo_region_create_rectangle(&update_rect);
		gtk_widget_queue_draw_region(bd->widget, cairo_region);
		cairo_region_destroy(cairo_region);
		gtk_widget_queue_draw(bd->widget);
	} else {
		if ( iDrawLog >= 2 ) { printf( "wDrawDelayUpdate( %d -> %d ) - update\n", bd->attributes.draw.delayUpdate, delay ); }
	}
	bd->attributes.draw.delayUpdate = delay;
}


void wDrawLine(
        wControl_p drawingArea,
        wDrawPix_t x0, wDrawPix_t y0,
        wDrawPix_t x1, wDrawPix_t y1,
        wDrawWidth width,
        wDrawLineType_e lineType,
        wDrawColor color,
        wDrawOpts opts )
{
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		wBasicDrawLine(drawingArea, x0, y0, x1, y1, width,
		               MINLINEWIDTHBITMAP, lineType, color, opts);
		return;
	}

	x0 = INMAPX(bd,x0);
	y0 = INMAPY(bd,y0);
	x1 = INMAPX(bd,x1);
	y1 = INMAPY(bd,y1);

	cairo_t* cr = prepare_drawing(bd, width, lineType, color, opts, FALSE);

	cairo_move_to(cr, x0 + 0.5, y0 + 0.5);
	cairo_line_to(cr, x1 + 0.5, y1 + 0.5);

	if (cr != cairoCtx) {
		cairo_stroke(cr);
	}
	finish_drawing(bd, cr);

	if (drawingArea->widget && !bd->delayUpdate ) {
		if (!cairoCtx) {
			/* Legacy mode: stroke happened in finish_drawing, just
			 * invalidate the region. In batch mode the
			 * queue_draw_area is deferred to wDrawFinish. */
			gtk_widget_queue_draw_area(drawingArea->widget,
			                           x0>x1?x1:x0, y0>y1?y1:y0,
			                           fabs(x1-x0)+1, fabs(y1-y0)+1);
		}
	}

}

/**
 * Draw an arc around a specified center
 *
 * \param drawingArea IN ?
 * \param x0, y0 IN  center of arc
 * \param r IN radius
 * \param angle0, angle1 IN start and end angle
 * \param drawCenter draw marking for center
 * \param width line width
 * \param lineType
 * \param color color
 * \param opts ?
 */


void wDrawArc(
        wControl_p drawingArea,
        wDrawPix_t x0, wDrawPix_t y0,
        wDrawPix_t r,
        wAngle_t angle0,
        wAngle_t angle1,
        int drawCenter,
        wDrawWidth width,
        wDrawLineType_e lineType,
        wDrawColor color,
        wDrawOpts opts )
{
	int x, y, w, h;
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);

	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		wBasicDrawArc(drawingArea, x0, y0, r, angle0, angle1, drawCenter, width,
		              MINLINEWIDTHBITMAP, lineType, color, opts);
		return;
	}

	if (r < 6.0/75.0) { return; }
	x = INMAPX(bd,x0-r);
	y = INMAPY(bd,y0+r);
	w = 2*r;
	h = 2*r;

	// the batch handling looks different here, but it works like that
	// the arc is not a part of a batch
	// create the new arc
	cairo_t *cairo = prepare_drawing(bd, width, lineType, color, opts, 0);

	// and finish the old one
	flush_path_if_open(bd, cairo);

	// its center point marker
	if(drawCenter) {
		// draw a small crosshair to mark the center of the curve
		cairo_move_to(cairo,  INMAPX(bd, x0 - (CENTERMARK_LENGTH / 2)), INMAPY(bd,
		                y0 ));
		cairo_line_to(cairo, INMAPX(bd, x0 + (CENTERMARK_LENGTH / 2)), INMAPY(bd, y0 ));
		cairo_move_to(cairo, INMAPX(bd, x0), INMAPY(bd, y0 - (CENTERMARK_LENGTH / 2 )));
		cairo_line_to(cairo, INMAPX(bd, x0), INMAPY(bd, y0  + (CENTERMARK_LENGTH / 2)));
		cairo_new_sub_path( cairo );

	}

	// draw the curve itself
	cairo_arc_negative(cairo, INMAPX(bd, x0), INMAPY(bd, y0), r,
	                   (angle0 - 90 + angle1) * (M_PI / 180.0), (angle0 - 90) * (M_PI / 180.0));

	// and finish the batch
	bd->pathOpen = TRUE;
	flush_path_if_open(bd, cairo);

	if (cairo != cairoCtx) {
		gtkDrawDestroyCairoContext(cairo);
	}

	if (drawingArea->widget && !bd->delayUpdate) {
		if(!cairoCtx) {
			gtk_widget_queue_draw_area(drawingArea->widget,x,y,w,h);
		}
	}
}

void wDrawPoint(
        wControl_p drawingArea,
        wDrawPix_t x0, wDrawPix_t y0,
        wDrawColor color,
        wDrawOpts opts )
{
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		return;
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                 opts);
	cairo_new_path(cairo);
	cairo_arc(cairo, INMAPX(bd, x0), INMAPY(bd, y0), 0.75, 0, 2 * M_PI);
	cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);
	if (drawingArea->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(drawingArea->widget,INMAPX(bd,x0-0.75),INMAPY(bd,
		                           y0+0.75),2,
		                           2);
	}

}

/*******************************************************************************
 *
 * Strings
 *
 ******************************************************************************/

void wDrawString(
        wControl_p drawingArea,
        wDrawPix_t x, wDrawPix_t y,
        wAngle_t a,
        const char * s,
        wFont_p fp,
        wFontSize_t fs,
        wDrawColor color,
        wDrawOpts opts )
{
	PangoLayout *layout;
	GdkRectangle update_rect;
	wDrawPix_t w;
	wDrawPix_t h;
	wDrawPix_t ascent;
	wDrawPix_t descent;
	wDrawPix_t baseline;
	double angle = -M_PI * a / 180.0;

	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		wBasicDrawString(drawingArea, x, y, a, (char *) s, fp, fs,
		                 MINLINEWIDTHBITMAP, MINLINEWIDTHBITMAP, color, opts);
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y);

	/* Text is never batched. Flush pending paths, then draw directly. */
	cairo_t* cairo;
	if (cairoCtx) {
		flush_path_if_open(bd, cairoCtx);
		cairo = cairoCtx;
	} else {
		cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opts);
	}

	layout = wlibFontCreatePangoLayout(drawingArea->widget, cairo, fp, fs, s,
	                                   (wDrawPix_t *) &w, (wDrawPix_t *) &h,
	                                   (wDrawPix_t *) &ascent, (wDrawPix_t *) &descent, (wDrawPix_t *) &baseline);

	GdkRGBA gcolor;
	GtkDrawSetColor( cairo, color, &gcolor );

	cairo_save(cairo);
	cairo_translate( cairo, x, y );
	cairo_rotate( cairo, angle );
	cairo_translate( cairo, 0, -baseline);

	cairo_move_to(cairo, 0, 0);

	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);
	wlibFontDestroyPangoLayout(layout);
	cairo_restore( cairo );

	gtkDrawDestroyCairoContext(cairo);

	if (bd->delayUpdate || drawingArea->widget == NULL) { return; }

	/* recalculate the area to be updated
	 * for simplicity sake I added plain text height ascent and descent,
	 * mathematically correct would be to use the trigonometrical functions as well
	 */
	update_rect.x      = (gint) x - 2;
	update_rect.y      = (gint) y - (gint) (baseline + descent) - 2;
	update_rect.width  = (gint) (fabs(w * cos( angle )) + fabs(h * sin(angle)))+2;
	update_rect.height = (gint) (fabs(h * sin( angle )) + fabs(w * cos(angle)))+2;

	if (drawingArea->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(drawingArea->widget, update_rect.x, update_rect.y,
		                           update_rect.width, update_rect.height);
	}

}

void wDrawGetTextSize(
        wDrawPix_t *w,
        wDrawPix_t *h,
        wDrawPix_t *d,
        wDrawPix_t *a,
        wControl_p drawingArea,
        const char * s,
        wFont_p fp,
        wFontSize_t fs )
{
	wDrawPix_t textWidth;
	wDrawPix_t textHeight;
	wDrawPix_t ascent;
	wDrawPix_t descent;
	wDrawPix_t baseline;
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	*w = 0;
	*h = 0;

	/* draw text */
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid,
	                 wDrawColorBlack, bd->bTempMode?wDrawOptTemp:0 );

	cairo_save(cairo);
	cairo_identity_matrix(cairo);

	wlibFontDestroyPangoLayout(
	        wlibFontCreatePangoLayout(drawingArea->widget, cairo, fp, fs, s,
	                                  &textWidth, &textHeight,
	                                  &ascent, &descent, &baseline) );

	cairo_restore(cairo);

	*w = textWidth;
	*h = textHeight;
	*a = ascent;
	//*d = textHeight-ascent;
	*d = descent;

	gtkDrawDestroyCairoContext(cairo);
}


/*******************************************************************************
 *
 * Basic Drawing Functions
 *
*******************************************************************************/

static void wlibDrawFilled(
        cairo_t * cairo,
        wDrawColor color,
        wDrawOpts opt )
{
	cairo_operator_t saved_op = cairo_get_operator(cairo);
	if ( (opt & wDrawOptTransparent) != 0 ) {
		if ( (opt & wDrawOptTemp) == 0 ) {
			cairo_set_source_rgb(cairo, 0,0,0);
			cairo_set_operator(cairo, CAIRO_OPERATOR_DIFFERENCE);
			cairo_fill_preserve(cairo);
		}
		GdkRGBA gcolor;
		GtkDrawSetColor( cairo, color, &gcolor );
		cairo_stroke_preserve(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		if ( iDrawLog >= 1 ) {
			printf( "wlibDrawFilled( %0.3f %0.3f %0.3f %0.3f )\n",
			        gcolor.red, gcolor.green, gcolor.blue, 0.3);
		}
		cairo_set_source_rgba(cairo, gcolor.red, gcolor.green, gcolor.blue, 0.3);
	}
	cairo_fill(cairo);
	cairo_set_operator(cairo, saved_op);
}



void wDrawFilledRectangle(
        wControl_p drawingArea,
        wDrawPix_t x,
        wDrawPix_t y,
        wDrawPix_t w,
        wDrawPix_t h,
        wDrawColor color,
        wDrawOpts opt )
{
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		wBasicDrawFillRectangle(drawingArea, x, y, w, h, color, opt);
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y)-h;

	int is_transparent = (opt & wDrawOptTransparent) != 0;

	if (is_transparent) {
		/* Transparent fill uses special compositing that can't be batched.
		 * Flush pending paths, then draw with legacy wlibDrawFilled. */
		cairo_t* cairo;
		if (cairoCtx) {
			flush_path_if_open(bd, cairoCtx);
			cairo = cairoCtx;
		} else {
			cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opt);
		}
		cairo_move_to(cairo, x, y);
		cairo_rel_line_to(cairo, w, 0);
		cairo_rel_line_to(cairo, 0, h);
		cairo_rel_line_to(cairo, -w, 0);
		cairo_rel_line_to(cairo, 0, -h);
		wlibDrawFilled( cairo, color, opt );
		gtkDrawDestroyCairoContext(cairo);
	} else {
		/* Opaque fill: batchable — same color rects accumulate in one path */
		cairo_t* cr = prepare_drawing(bd, 0, wDrawLineSolid, color, opt, TRUE);
		cairo_rectangle(cr, x, y, w, h);
		finish_drawing(bd, cr);
	}

	if (drawingArea->widget && !bd->delayUpdate) {
		if (!cairoCtx) {
			gtk_widget_queue_draw_area(GTK_WIDGET(drawingArea->widget),
			                           w>0?x:x+w, h>0?y:y+h, fabs(w)+1, fabs(h)+1);
		}
	}
}

void wDrawPolygon(
        wControl_p drawingArea,
        wDrawPix_t p[][2],
        wPolyLine_e type[],
        int cnt,
        wDrawColor color,
        wDrawWidth dw,
        wDrawLineType_e lt,
        wDrawOpts opt,
        int fill,
        int open )
{
	static int maxCnt = 0;
	static GdkPoint *points;
	int i;
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		wBasicDrawFillPolygon(drawingArea, p, type, cnt, color, opt, fill, open);
		return;
	}

	if (cnt > maxCnt) {
		if (points == NULL) {
			points = (GdkPoint*)malloc( cnt*sizeof *points );
		} else {
			points = (GdkPoint*)realloc( points, cnt*sizeof *points );
		}
		if (points == NULL) {
			abort();
		}
		maxCnt = cnt;
	}
	wDrawPix_t min_x,max_x,min_y,max_y;
	min_x = max_x = INMAPX(bd,p[0][0]);
	min_y = max_y = INMAPY(bd,p[0][1]);
	for (i=0; i<cnt; i++) {
		points[i].x = INMAPX(bd,p[i][0]);
		points[i].y = INMAPY(bd,p[i][1]);
		if (points[i].x < min_x) { min_x = points[i].x; }
		if (points[i].x > max_x) { max_x = points[i].x; }
		if (points[i].y > max_y) { max_y = points[i].y; }
		if (points[i].y < min_y) { min_y = points[i].y; }
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, fill?0:dw,
	                 fill?wDrawLineSolid:lt, color, opt);

	for(i = 0; i < cnt; ++i) {
		int j = i-1;
		int k = i+1;
		if (j < 0) { j = cnt-1; }
		if (k > cnt-1) { k = 0; }
		GdkPoint mid0, mid1, mid3, mid4;
		// save is static because of an apparent compiler bug on Linux
		// This happens with RelWithDebInfo target
		// If the first segment is a line then save should = points[0]
		// However it becomes mid0 instead which causes the last corner to be misplaced.
		static GdkPoint save;
		double len0, len1;
		double d0x = (points[i].x-points[j].x);
		double d0y = (points[i].y-points[j].y);
		double d1x = (points[k].x-points[i].x);
		double d1y = (points[k].y-points[i].y);
		len0 = (d0x*d0x+d0y*d0y);
		len1 = (d1x*d1x+d1y*d1y);
		mid0.x = (d0x/2)+points[j].x;
		mid0.y = (d0y/2)+points[j].y;
		mid1.x = (d1x/2)+points[i].x;
		mid1.y = (d1y/2)+points[i].y;
		if (type && (type[i] == wPolyLineRound) && (len1>0) && (len0>0)) {
			double ratio = sqrt(len0/len1);
			if (len0 < len1) {
				mid1.x = ((d1x*ratio)/2)+points[i].x;
				mid1.y = ((d1y*ratio)/2)+points[i].y;
			} else {
				mid0.x = points[i].x-(d0x/(2*ratio));
				mid0.y = points[i].y-(d0y/(2*ratio));
			}
		}
		mid3.x = (points[i].x-mid0.x)/2+mid0.x;
		mid3.y = (points[i].y-mid0.y)/2+mid0.y;
		mid4.x = (mid1.x-points[i].x)/2+points[i].x;
		mid4.y = (mid1.y-points[i].y)/2+points[i].y;
		points[i].x = round(points[i].x)+0.5;
		points[i].y = round(points[i].y)+0.5;
		mid0.x = round(mid0.x)+0.5;
		mid0.y = round(mid0.y)+0.5;
		mid1.x = round(mid1.x)+0.5;
		mid1.y = round(mid1.y)+0.5;
		mid3.x = round(mid3.x)+0.5;
		mid3.y = round(mid3.y)+0.5;
		mid4.x = round(mid4.x)+0.5;
		mid4.y = round(mid4.y)+0.5;
		if(i==0) {
			if (!type || type[i] == wPolyLineStraight || open) {
				cairo_move_to(cairo, points[i].x, points[i].y);
				save = points[0];
			} else {
				cairo_move_to(cairo, mid0.x, mid0.y);
				if (type[i] == 1) {
					cairo_curve_to(cairo, points[i].x, points[i].y, points[i].x, points[i].y,
					               mid1.x, mid1.y);
				} else {
					cairo_curve_to(cairo, mid3.x, mid3.y, mid4.x, mid4.y, mid1.x, mid1.y);
				}
				save = mid0;
			}
		} else if (!type || type[i] == wPolyLineStraight || (open && (i==cnt-1))) {
			cairo_line_to(cairo, points[i].x, points[i].y);
		} else {
			cairo_line_to(cairo, mid0.x, mid0.y);
			if (type[i] == wPolyLineSmooth) {
				cairo_curve_to(cairo, points[i].x, points[i].y, points[i].x, points[i].y,
				               mid1.x, mid1.y);
			} else {
				cairo_curve_to(cairo, mid3.x, mid3.y, mid4.x, mid4.y, mid1.x, mid1.y);
			}
		}
		if ((i==cnt-1) && !open) {
			cairo_line_to(cairo, save.x, save.y);
		}
	}
	if (fill && !open) {
		wlibDrawFilled( cairo, color, opt );
	} else {
		cairo_stroke(cairo);
	}
	gtkDrawDestroyCairoContext(cairo);
	if (drawingArea->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(GTK_WIDGET(drawingArea->widget),min_x,min_y,
		                           max_x-min_x+1,
		                           max_y-min_y+1);  //Ensure positive width
	}

}

void wDrawFilledCircle(
        wControl_p drawingArea,
        wDrawPix_t x0,
        wDrawPix_t y0,
        wDrawPix_t r,
        wDrawColor color,
        wDrawOpts opt )
{
	int x, y, w, h;
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (bd->drawDestination == DIRECTCAIRO) {
		wBasicDrawFillCircle(drawingArea, x0, y0, r, color, opt);
		return;
	}

	x = INMAPX(bd,x0-r);
	y = INMAPY(bd,y0+r);
	w = 2*r;
	h = 2*r;

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                 opt);
	cairo_arc(cairo, INMAPX(bd, x0), INMAPY(bd, y0), r, 0, 2 * M_PI);
	wlibDrawFilled( cairo, color, opt );
	gtkDrawDestroyCairoContext(cairo);

	if (drawingArea->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(drawingArea->widget,x,y,w,h);
	}

}

void wDrawClearTemp(wControl_p drawingArea)
{
	//Wipe out temp space with 0 alpha (transparent)
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawClearTemp\n", lDrawCnt++  );
	}
	cairo_t* cairo = cairo_create(bd->temp_surface);

	cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (cairo, CAIRO_OPERATOR_SOURCE);

	cairo_paint(cairo);
	cairo_destroy(cairo);

	if (drawingArea->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw(drawingArea->widget);
	}
}

void wDrawClear( wControl_p drawingArea )
{
	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawClear\n", lDrawCnt++  );
	}

	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	cairo_t* cairo;

	cairo = cairo_create(bd->surface);

	/* Set surface to opaque color (r, g, b) */
	cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0 );
	cairo_paint(cairo);

	cairo_destroy(cairo);

	if (drawingArea->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw(drawingArea->widget);
	}
	wDrawClearTemp(drawingArea);
}

void * wDrawGetContext(
        wControl_p drawingArea )
{
	return drawingArea->context;
}

/*******************************************************************************
 *
 * Bit Maps
 *
*******************************************************************************/

#define SCALE_ONE_TO_ONE 1

static cairo_pattern_t *
CreatePatternFromPixbuf(GdkPixbuf* pixbuf)
{
	cairo_surface_t* surface;
	cairo_pattern_t* pattern;

	surface = gdk_cairo_surface_create_from_pixbuf(pixbuf, SCALE_ONE_TO_ONE, NULL);
	pattern = cairo_pattern_create_for_surface(surface);

	cairo_surface_destroy(surface);

	return(pattern);
}


wDrawBitMap_p wDrawBitMapCreate(
        wControl_p drawingArea,
        int x,
        int y,
        const char *prefix,
        const char *filename )
{
	GError* error = NULL;
	wDrawBitMap_p bm;

	g_assert(drawingArea->type == B_DRAW);

	bm = (wDrawBitMap_p)g_malloc0( sizeof *bm );
	if (bm) {
		gchar* path;
		path = g_strconcat(prefix,
		                   filename,
		                   NULL);

		bm->pixbuf = gdk_pixbuf_new_from_resource(path, &error);
		if (error) {
			fprintf(stderr, "Error reading icon: %s\n", error->message);
			g_error_free(error);
			bm->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
			gdk_pixbuf_fill(bm->pixbuf, 0x00000000);
			bm->w = 1;
			bm->h = 1;
		} else {
			bm->w = gdk_pixbuf_get_width(bm->pixbuf);
			bm->h = gdk_pixbuf_get_height(bm->pixbuf);
		}
		bm->x = x;
		bm->y = y;

		bm->pattern = CreatePatternFromPixbuf(bm->pixbuf);
		g_object_unref(bm->pixbuf);
		bm->pixbuf = NULL;

		if ( iDrawLog >= 1 ) {
			printf( "%ld: wDrawBitMapCreate( %d+%d %s )\n", lDrawCnt++, bm->x, bm->y,
			        path ) ;
		}
		g_free(path);
	}
	return bm;
}
/**
 * \brief Draw a bitmap onto a drawing area at the specified position.
 *
 * Renders a previously created bitmap (\ref wDrawBitMap_t) onto the given
 * drawing area using Cairo's mask operation, applying the specified color and
 * draw options. All opaque colors will be replaced with the new color,
 * transparent pixels will be left alone.
 * The bitmap's own hotspot offset (bm->x, bm->y) is subtracted
 * from the target coordinates before mapping to device pixels.
 *
 * \param[in] drawingArea  Handle to the target drawing control (must be of
 *                         type B_DRAW).
 * \param[in] bm           Pointer to the bitmap to draw, created by
 *                         \ref wDrawBitMapCreate.
 * \param[in] x            Horizontal position in drawing coordinates where
 *                         the bitmap hotspot is placed.
 * \param[in] y            Vertical position in drawing coordinates where the
 *                         bitmap hotspot is placed.
 * \param[in] color        Foreground color used when painting the bitmap mask.
 * \param[in] opts         Drawing options (e.g. \c wDrawOptTemp) that control
 *                         rendering behaviour.
 */

void wDrawBitMap(
        wControl_p drawingArea,
        wDrawBitMap_p bm,
        wDrawPix_t x, wDrawPix_t y,
        wDrawColor color,
        wDrawOpts opts )
{
	cairo_matrix_t matrix;

	struct draw *bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	x = INMAPX( bd, x-bm->x );
	y = INMAPY( bd, y-bm->y )-bm->h;

	cairo_t* cr = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                                        opts );

	cairo_save(cr);
	cairo_matrix_init_translate(&matrix, x, y);
	cairo_set_matrix(cr, &matrix);

	cairo_mask(cr, bm->pattern);
	cairo_restore(cr);

	gtkDrawDestroyCairoContext(cr);

	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawBitMap( %d+%d ) %0.1f+%0.1f ) \n", lDrawCnt++, bm->x, bm->y,
		        x, y ) ;
	}
}

/*******************************************************************************
 *
 * Event Handlers
 *
*******************************************************************************/



void wDrawSaveImage(
        wDraw_p bd )
{
	if ( bd->pixbufBackup ) {
		g_object_unref( bd->pixbufBackup );
	}
	bd->pixbufBackup = gdk_pixbuf_get_from_surface( bd->surface, 0, 0, bd->w,
	                   bd->h );

}


void wDrawRestoreImage(
        wDraw_p bd )
{
	if ( bd->pixbufBackup ) {

		cairo_t * cr;
		cr = cairo_create(bd->surface);
		gdk_cairo_set_source_pixbuf(cr, bd->pixbufBackup, 0, 0);
		cairo_paint(cr);
		cairo_destroy(cr);

		cr = NULL;

		if ( bd->delayUpdate || bd->widget == NULL ) { return; }
		gtk_widget_queue_draw_area( bd->widget, 0, 0, bd->w, bd->h );
	}
}

void wDrawSetSize(
        wControl_p drawingArea,
        wWinPix_t w,
        wWinPix_t h, void * redraw)
{
	if (drawingArea == NULL) {
		fprintf(stderr,"resizeDraw: no client data\n");
		return;
	}

	g_assert(drawingArea->type == B_DRAW);

	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawSetSize %ld+%ld %s\n", lDrawCnt, w, h, redraw?"Redraw":"" );
	}

	/* Negative values crashes the program */
	if (w < 0 || h < 0) {
		return;
	}
	gtk_widget_set_size_request(drawingArea->widget, w, h);
}


void wDrawGetSize(
        wControl_p drawingArea,
        wWinPix_t *w,
        wWinPix_t *h )
{
	if (drawingArea->widget) {
		wlibControlGetSize( (wControl_p)drawingArea );
	}

	GtkAllocation allocation;

	gtk_widget_get_allocation(drawingArea->widget, &allocation);
	*w = allocation.width;
	*h = allocation.height;
}

/**
 * Return the resolution of a device in dpi
 *
 * \param d IN the device
 * \return    the resolution in dpi
 */

double wDrawGetDPI(
        wControl_p d )
{
	return d->attributes.draw.dpi;
}


double wDrawGetMaxRadius( wControl_p drawingArea )
{
	return(32767.0);
}

/**
 *	Define a clipping region. The passed coordinates are stored and will be applied
 * 	when the cairo context for this drawing area is created
 *
 */

void wDrawClip(wControl_p drawingArea,
               wDrawPix_t x,
               wDrawPix_t y,
               wDrawPix_t w,
               wDrawPix_t h )
{
	struct draw *d = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if(!d->clipregion) {
		d->clipregion = g_malloc0(sizeof(struct rect));
	}

	d->clipregion->w = w;
	d->clipregion->h = h;
	d->clipregion->x = INMAPX( d, x );
	d->clipregion->y = INMAPY( d, y ) - d->clipregion->h;

}

void wDrawClipClear(wControl_p drawingArea)
{
	wWinPix_t totalW, totalH;
	struct draw *d = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	wDrawGetSize(drawingArea, &totalW, &totalH);
	wDrawClip(drawingArea, 0, 0, totalW, totalH);

	g_free(d->clipregion);
	d->clipregion = NULL;
}

/*******************************************************************************
 *
 * Background
 *
 ******************************************************************************/
int wDrawSetBackground(    wControl_p drawingArea, char * path, char ** error)
{
	GError *err = NULL;
	struct draw* bd = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	assert(drawingArea->type == B_DRAW);

	if (bd->background) {
		g_object_unref(bd->background);
	}

	if (path) {
		bd->background = gdk_pixbuf_new_from_file (path, &err);
		if (!bd->background) {
			*error = strdup(err->message);
			g_error_free(err);
			return -1;
		}
	} else {
		bd->background = NULL;
		return 1;
	}
	return 0;

}

/**
 * Use a loaded background in another context.
 *
 * \param from  context with background
 * \param to    context to get a reference to the existing background
 */

void
wDrawCloneBackground(wControl_p from, wControl_p to)
{
	struct draw* fromDraw = CONTROL_GET_ATTRIBUTES_PTR(from, draw);
	struct draw* toDraw = CONTROL_GET_ATTRIBUTES_PTR(to, draw);
	if (fromDraw->background) {
		toDraw->background = g_object_ref(fromDraw->background);
	} else {
		toDraw->background = NULL;
	}
}

void
wDrawUnrefBackground(wControl_p drawControl)
{
	struct draw* drawing = CONTROL_GET_ATTRIBUTES_PTR(drawControl, draw);

	if(drawing) {
		g_object_unref(drawing->background);
		drawing->background = NULL;
	}
}

int GetLBorder(void);
int GetBBorder(void);

void wDrawShowBackground( wControl_p drawingArea, wWinPix_t pos_x,
                          wWinPix_t pos_y,
                          wWinPix_t size, wAngle_t angle, int screen,
                          wDrawPix_t clip_x, wDrawPix_t clip_y,
                          wDrawPix_t clip_w, wDrawPix_t clip_h)
{
	struct draw* drawControl = CONTROL_GET_ATTRIBUTES_PTR(drawingArea, draw);
	g_assert(drawingArea->type == B_DRAW);

	if (drawControl->background) {
		cairo_t* cairo = gtkDrawCreateCairoContext(drawControl, NULL, 0, wDrawLineSolid,
		                 wDrawColorWhite, drawControl->bTempMode?wDrawOptTemp:0 );

		/* --- New Clipping Logic --- */
		// 1. Save the current state (including previous transformation/clip)
		cairo_save(cairo);

		int lb = GetLBorder();
		int bb = GetBBorder();
		cairo_rectangle(cairo, lb, 0, drawControl->width - lb,
		                drawControl->height - bb);
		cairo_clip(cairo);

		// 2. Set the clipping area to the "White Room"
		// We switch to identity matrix because room_x/y/w/h are screen pixels
		cairo_matrix_t current_matrix;
		cairo_get_matrix(cairo, &current_matrix); // Save current zoom matrix

		cairo_identity_matrix(cairo);
		cairo_rectangle(cairo, clip_x, clip_y, clip_w, clip_h);
		cairo_rectangle(cairo, lb, 0,
		                drawControl->width - lb,
		                drawControl->height - bb);
		cairo_clip(cairo);
		cairo_set_matrix(cairo, &current_matrix);

		/* -------------------------- */
		int pixels_width = gdk_pixbuf_get_width(drawControl->background);
		int pixels_height = gdk_pixbuf_get_height(drawControl->background);
		double scale;
		double posx,posy,width,sized;
		posx = (double)pos_x;
		posy = (double)pos_y;
		if (size == 0) {
			scale = 1.0;
		} else {
			sized = (double)size;
			width = (double)pixels_width;
			scale = sized/width;
		}
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		double rad = M_PI*(angle/180);
		posy = (double)drawControl->height-((pixels_height*fabs(cos(
		                rad))+pixels_width*fabs(sin(
		                                rad)))*scale)-posy;
		//width = (double)(pixels_width*scale);
		//height = (double)(pixels_height*scale);
		cairo_translate(cairo,posx,posy);
		cairo_scale(cairo, scale, scale);
		cairo_translate(cairo, fabs(pixels_width/2.0*cos(rad))+fabs(
		                        pixels_height/2.0*sin(rad)),
		                fabs(pixels_width/2.0*sin(rad))+fabs(pixels_height/2.0*cos(rad)));
		cairo_rotate(cairo, M_PI*(angle/180.0));
		// We need to clip around the image, or cairo will paint garbage attributes
		cairo_rectangle(cairo, -pixels_width/2.0, -pixels_height/2.0, pixels_width,
		                pixels_height);
		cairo_clip(cairo);
		gdk_cairo_set_source_pixbuf(cairo, drawControl->background, -pixels_width/2.0,
		                            -pixels_height/2.0);
		cairo_pattern_t *mask = cairo_pattern_create_rgba (1.0,1.0,1.0,
		                        (100.0-screen)/100.0);
		cairo_mask(cairo,mask);
		cairo_pattern_destroy(mask);
		cairo_restore(cairo);
		gtkDrawDestroyCairoContext(cairo);
	}

}

void
wDrawStart(wControl_p drawArea)
{
	struct draw* drawControl = CONTROL_GET_ATTRIBUTES_PTR(drawArea, draw);

	if (cairoCtx) {
		/* Nested wDrawStart — should not happen. Flush and clean up. */
		fprintf(stderr, "wDrawStart: nested call, flushing previous batch\n");
		flush_path_if_open(drawControl, cairoCtx);
		cairo_destroy(cairoCtx);
		cairoCtx = NULL;
	}

	if (drawControl->bTempMode) {
		cairoCtx = cairo_create(drawControl->temp_surface);
	} else {
		cairoCtx = cairo_create(drawControl->surface);
	}

	drawControl->activeColor = -1;
	drawControl->activeWidth = -1.0;
	drawControl->activeStyle = -1;
	drawControl->fill = FALSE;
	drawControl->pathOpen = FALSE;
	drawControl->batchCount = 0;

	cairo_set_line_cap(cairoCtx, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join(cairoCtx, CAIRO_LINE_JOIN_MITER);
	cairo_set_operator(cairoCtx, CAIRO_OPERATOR_SOURCE);
	cairo_set_antialias(cairoCtx, CAIRO_ANTIALIAS_FAST);

	/* Save the initial matrix so legacy code (gtkDrawCreateCairoContext)
	 * can restore it after operations that modify the transform. */
	cairo_get_matrix(cairoCtx, &base_matrix);

	currCalls = 0;

	if ( iDrawLog >= 2 ) {
		printf( "%ld: wDrawStart batch mode\n", lDrawCnt++ );
	}
}

void wDrawFinish(wControl_p drawArea)
{
	struct draw* drawControl = CONTROL_GET_ATTRIBUTES_PTR(drawArea, draw);

	if (cairoCtx) {
		flush_path_if_open(drawControl, cairoCtx);

		cairo_destroy(cairoCtx);
		cairoCtx = NULL;
	}

	drawControl->batchCount = 0;
	drawControl->pathOpen = FALSE;

	if ( iDrawLog >= 2 ) {
		printf( "%ld: wDrawFinish, %u calls batched\n", lDrawCnt++, currCalls );
	}

	/* Invalidate the full widget so all batched draws become visible */
	if (drawArea->widget && !drawControl->delayUpdate) {
		gtk_widget_queue_draw(drawArea->widget);
	}
}
