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

#include <gtk/gtk.h>

// Trace low level drawing actions
int iDrawLog = 0;
long lDrawCnt = 0;


#include "gtkint.h"

#define gtkAddHelpString( a, b ) wlibAddHelpString( a, b )


// Hack to do TempRedraw or MainRedraw
// For Windows only
wBool_t wDrawDoTempDraw = TRUE;

struct wDrawBitMap_t {
	int w;
	int h;
	int x;
	int y;
	const unsigned char * bits;
};

struct wDraw_t psPrint_d;

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
#define INMAPY(D,Y)	(((D)->h-1) - (Y))
#define OUTMAPX(D,X)	(X)
#define OUTMAPY(D,Y)	(((D)->h-1) - (Y))


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

	long tcolor = color;
	gcolor->red = ((color&0xFF0000)>>16)/255.0;
	gcolor->green = ((color&0xFF00)>>8)/255.0;
	gcolor->blue  = (color&0xFF)/255.0;
	gcolor->alpha = 1.0;
	cairo_set_source_rgba(cairo, gcolor->red, gcolor->green, gcolor->blue, 1.0 );
	if ( iDrawLog >= 3 )
		printf( "%ld: GtkDrawGetColor( %lx: R%0.3f G%0.3f B%0.3f\n",
		        lDrawCnt++, color, gcolor->red, gcolor->green, gcolor->blue );
	return tcolor;
}

static cairo_t* gtkDrawCreateCairoContext(
        wDraw_p bd,
        cairo_surface_t * surface,
        wDrawWidth width,
        wDrawLineType_e lineType,
        wDrawColor color,
        wDrawOpts opts )
{
	if ( iDrawLog >= 4 )
		printf( "%ld: gtkDrawCreateCairoContext %s Color:%6lx Clip:%d [%ldx%ld] (%ld+%ld)\n",
		        lDrawCnt++, (opts&wDrawOptTemp)?"Temp":"Main", color,
		        bd->clip_set,
		        bd->realX, bd->realY, bd->w, bd->h );
	cairo_t * cairo;

	if (surface) {
		cairo = cairo_create(surface);
	} else {
		if (opts & wDrawOptTemp) {
			if ( ! bd->bTempMode ) {
				printf( "Temp draw in Main Mode. Contact Developers. See %s:%d\n",
				        "gtkdraw-cairo.c", __LINE__+1 );
			}
			/* Temp Draw In Main Mode:
				You are seeing this message because there is a wDraw*() call on tempD but you are not in the context of TempRedraw()
				Typically this happens when Cmd<Object>() is processing a C_DOWN or C_MOVE action and it writes directly to tempD
				Instead it sould set some state which allows c_redraw to do the actual drawing
				If you set a break point on the printf you'll see the offending wDraw*() call in the traceback
				It should be sufficient to remove that draw code or move it to C_REDRAW
				This is not fatal but the draw will be ineffective because the next TempRedraw() will erase the temp surface
				before the expose event can copy (or bitblt) it
			*/
			if ( iDrawLog > 4 ) {
				printf( "%ld: cairo_create temp\n", lDrawCnt++ );
			}
			cairo = cairo_create(bd->temp_surface);
		} else {
			if ( bd->bTempMode ) {
				printf( "Main draw in Temp Mode. Contact Developers. See %s:%d\n",
				        "gtkdraw-cairo.c", __LINE__+1 );
			}
			/* Main Draw In Temp Mode:
				You are seeing this message because there is a wDraw*() call on mainD but you are in the context of TempRedraw()
				Typically this happens when C_REDRAW action calls wDraw*() on mainD, in which case it should be writing to tempD.
				Or the wDraw*() call should be removed if it is redundant.
				If you set a break point on the printf you'll see the offending wDraw*() call in the traceback
				This is not fatal but could result in garbage being left on the screen if the command is cancelled.
			*/
			if ( iDrawLog > 4 ) {
				printf( "%ld: cairo_create main\n", lDrawCnt++ );
			}
			cairo = cairo_create(bd->surface);
		}
	}

	width = width ? abs(width) : 1;
	cairo_set_line_width(cairo, width);

	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_MITER);

	switch(lineType) {
	case wDrawLineSolid: {
		cairo_set_dash(cairo, 0, 0, 0);
		break;
	}
	case wDrawLineDash: {
		double dashes[] = { 5, 3 };
		static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash(cairo, dashes, len_dashes, 0);
		break;
	}
	case wDrawLineDot: {
		double dashes[] = { 1, 2 };
		static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash(cairo, dashes, len_dashes, 0);
		break;
	}
	case wDrawLineDashDot: {
		double dashes[] = { 5, 2, 1, 2 };
		static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash(cairo, dashes, len_dashes, 0);
		break;
	}
	case wDrawLineDashDotDot: {
		double dashes[] = { 5, 2, 1, 2, 1, 2 };
		static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash(cairo, dashes, len_dashes, 0);
		break;
	}
	case wDrawLineCenter: {
		double dashes[] = { 8, 3, 5, 3};
		static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash(cairo, dashes, len_dashes, 0.0);
		break;
	}
	case wDrawLinePhantom: {
		double dashes[] = { 8, 3, 5, 3, 5, 3};
		static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash(cairo, dashes, len_dashes, 0.0);
		break;
	}
	}
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	GdkRGBA gcolor;
	bd->lastColor = GtkDrawSetColor( cairo, color, &gcolor );
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);

	if (bd->clip_set) {
		cairo_rectangle(cairo,bd->rect.x,bd->rect.y,bd->rect.width,bd->rect.height);
		cairo_clip(cairo);
	}

	return cairo;
}


wBool_t wDrawSetTempMode(
        wDraw_p bd,
        wBool_t bTemp )
{
	wBool_t ret = bd->bTempMode;
	bd->bTempMode = bTemp;
	if ( ret == FALSE && bTemp == TRUE ) {
		// Main to Temp drawing
		wDrawClearTemp( bd );
	}
	return ret;
}

static cairo_t* gtkDrawDestroyCairoContext(cairo_t *cairo)
{
	cairo_destroy(cairo);
	return NULL;
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
        wDraw_p bd,
        wBool_t delay )
{
	cairo_rectangle_int_t update_rect;

	if ( (!delay) && bd->delayUpdate ) {
		update_rect.x = 0;
		update_rect.y = 0;
		update_rect.width = bd->w;
		update_rect.height = bd->h;
		cairo_region_t * cairo_region = cairo_region_create_rectangle(&update_rect);
		gtk_widget_queue_draw_region(bd->widget, cairo_region);
		cairo_region_destroy(cairo_region);
		gtk_widget_queue_draw(bd->widget);
	}
	bd->delayUpdate = delay;
}


void wDrawLine(
        wDraw_p bd,
        wDrawPix_t x0, wDrawPix_t y0,
        wDrawPix_t x1, wDrawPix_t y1,
        wDrawWidth width,
        wDrawLineType_e lineType,
        wDrawColor color,
        wDrawOpts opts )
{

	if ( bd == &psPrint_d ) {
		wlibBasicDrawLine( bd, x0, y0, x1, y1, width,
		                   MINLINEWIDTHPRINT, lineType, color, opts );
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {
		wlibBasicDrawLine( bd, x0, y0, x1, y1, width,
		                   MINLINEWIDTHBITMAP, lineType, color, opts );
		return;
	}

	x0 = INMAPX(bd,x0);
	y0 = INMAPY(bd,y0);
	x1 = INMAPX(bd,x1);
	y1 = INMAPY(bd,y1);

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, width, lineType, color,
	                 opts);
	cairo_move_to(cairo, x0 + 0.5, y0 + 0.5);
	cairo_line_to(cairo, x1 + 0.5, y1 + 0.5);
	cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget) {
		gtk_widget_queue_draw_area(bd->widget,x0>x1?x1:x0,y0>y1?y1:y0,fabs(x1-x0)+1,
		                           fabs(y1-y0)+1);
	}

}

/**
 * Draw an arc around a specified center
 *
 * \param drawControl IN ?
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
        wDraw_p bd,
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

	if ( bd == &psPrint_d ) {
		wlibBasicDrawArc( bd, x0, y0, r, angle0, angle1, drawCenter, width,
		                  MINLINEWIDTHPRINT, lineType, color, opts );
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {
		wlibBasicDrawArc( bd, x0, y0, r, angle0, angle1, drawCenter, width,
		                  MINLINEWIDTHBITMAP, lineType, color, opts );
		return;
	}

	if (r < 6.0/75.0) { return; }
	x = INMAPX(bd,x0-r);
	y = INMAPY(bd,y0+r);
	w = 2*r;
	h = 2*r;

	// now create the new arc
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, width, lineType, color,
	                 opts);
	cairo_new_path(cairo);

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
	cairo_stroke(cairo);

	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(bd->widget,x,y,w,h);
	}



}

void wDrawPoint(
        wDraw_p bd,
        wDrawPix_t x0, wDrawPix_t y0,
        wDrawColor color,
        wDrawOpts opts )
{

	if ( bd == &psPrint_d ) {
		/*psPrintArc( x0, y0, r, angle0, angle1, drawCenter, width, lineType, color, opts );*/
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {

		return;
	}
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                 opts);
	cairo_new_path(cairo);
	cairo_arc(cairo, INMAPX(bd, x0), INMAPY(bd, y0), 0.75, 0, 2 * M_PI);
	cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(bd->widget,INMAPX(bd,x0-0.75),INMAPY(bd,y0+0.75),2,
		                           2);
	}

}

/*******************************************************************************
 *
 * Strings
 *
 ******************************************************************************/

void wDrawString(
        wDraw_p bd,
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

	if ( bd == &psPrint_d ) {
		wlibBasicDrawString( bd, x, y, a, (char *) s, fp, fs,
		                     MINLINEWIDTHPRINT, MINLINEWIDTHPRINT,
		                     color, opts );
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {
		wlibBasicDrawString( bd, x, y, a, (char *) s, fp, fs,
		                     MINLINEWIDTHBITMAP, MINLINEWIDTHBITMAP,
		                     color, opts );
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y);

	/* draw text */
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                 opts);

	layout = wlibFontCreatePangoLayout(bd->widget, cairo, fp, fs, s,
	                                   (wDrawPix_t *) &w, (wDrawPix_t *) &h,
	                                   (wDrawPix_t *) &ascent, (wDrawPix_t *) &descent, (wDrawPix_t *) &baseline);

	/* cairo does not support the old method of text removal by overwrite; force always write here and
	   refresh on cancel event */
	GdkRGBA gcolor;
	GtkDrawSetColor( cairo, color, &gcolor );

	cairo_translate( cairo, x, y );
	cairo_rotate( cairo, angle );
	cairo_translate( cairo, 0, -baseline);

	cairo_move_to(cairo, 0, 0);

	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);
	wlibFontDestroyPangoLayout(layout);
	cairo_restore( cairo );
	gtkDrawDestroyCairoContext(cairo);

	if (bd->delayUpdate || bd->widget == NULL) { return; }

	/* recalculate the area to be updated
	 * for simplicity sake I added plain text height ascent and descent,
	 * mathematically correct would be to use the trigonometrical functions as well
	 */
	update_rect.x      = (gint) x - 2;
	update_rect.y      = (gint) y - (gint) (baseline + descent) - 2;
	update_rect.width  = (gint) (w * cos( angle ) + h * sin(angle))+2;
	update_rect.height = (gint) (h * sin( angle ) + w * cos(angle))+2;

	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(bd->widget, update_rect.x, update_rect.y,
		                           update_rect.width, update_rect.height);
	}

}

void wDrawGetTextSize(
        wDrawPix_t *w,
        wDrawPix_t *h,
        wDrawPix_t *d,
        wDrawPix_t *a,
        wDraw_p bd,
        const char * s,
        wFont_p fp,
        wFontSize_t fs )
{
	wDrawPix_t textWidth;
	wDrawPix_t textHeight;
	wDrawPix_t ascent;
	wDrawPix_t descent;
	wDrawPix_t baseline;

	*w = 0;
	*h = 0;

	/* draw text */
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid,
	                 wDrawColorBlack, bd->bTempMode?wDrawOptTemp:0 );

	cairo_identity_matrix(cairo);

	wlibFontDestroyPangoLayout(
	        wlibFontCreatePangoLayout(bd->widget, cairo, fp, fs, s,
	                                  &textWidth, &textHeight,
	                                  &ascent, &descent, &baseline) );

	*w = textWidth;
	*h = textHeight;
	*a = ascent;
	//*d = textHeight-ascent;
	*d = descent;

	if (debugWindow >= 3) {
		fprintf(stderr, "text metrics: w=%0.1f, h=%0.1f, d=%0.1f\n", *w, *h, *d);
	}

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
	if ( (opt & wDrawOptTransparent) != 0 ) {
		if ( (opt & wDrawOptTemp) == 0 ) {
			cairo_set_source_rgb(cairo, 0,0,0);
			cairo_set_operator(cairo, CAIRO_OPERATOR_DIFFERENCE);
			cairo_fill_preserve(cairo);
		}
		GdkRGBA gcolor;
		GtkDrawSetColor( cairo, color, &gcolor );
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_stroke_preserve(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba(cairo, gcolor.red, gcolor.green, gcolor.blue, 0.3);
	}
	cairo_fill(cairo);
}



void wDrawFilledRectangle(
        wDraw_p bd,
        wDrawPix_t x,
        wDrawPix_t y,
        wDrawPix_t w,
        wDrawPix_t h,
        wDrawColor color,
        wDrawOpts opt )
{

	if ( bd == &psPrint_d ) {
		wlibBasicDrawFillRectangle( bd, x, y, w, h, color, opt );
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {
		wlibBasicDrawFillRectangle( bd, x, y, w, h, color, opt );
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y)-h;

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                 opt);

	cairo_move_to(cairo, x, y);
	cairo_rel_line_to(cairo, w, 0);
	cairo_rel_line_to(cairo, 0, h);
	cairo_rel_line_to(cairo, -w, 0);
	cairo_rel_line_to(cairo, 0, -h);
	wlibDrawFilled( cairo, color, opt );

	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(GTK_WIDGET(bd->widget),w>0?x:x+w,h>0?y:y+h,fabs(w)+1,
		                           fabs(h)+1);
	}

}

void wDrawPolygon(
        wDraw_p bd,
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

	if ( bd == &psPrint_d ) {
		wlibBasicDrawFillPolygon( bd, p, type, cnt, color, opt, fill, open );
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {
		wlibBasicDrawFillPolygon( bd, p, type, cnt, color, opt, fill, open );
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
	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(GTK_WIDGET(bd->widget),min_x,min_y,max_x-min_x+1,
		                           max_y-min_y+1);  //Ensure positive width
	}

}

void wDrawFilledCircle(
        wDraw_p bd,
        wDrawPix_t x0,
        wDrawPix_t y0,
        wDrawPix_t r,
        wDrawColor color,
        wDrawOpts opt )
{
	int x, y, w, h;

	if ( bd == &psPrint_d ) {
		wlibBasicDrawFillCircle( bd, x0, y0, r, color, opt );
		return;
	}

	if(bd->drawDestination == EXPORTBITMAP) {
		wlibBasicDrawFillCircle( bd, x0, y0, r, color, opt );
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

	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw_area(bd->widget,x,y,w,h);
	}

}

void wDrawClearTemp(wDraw_p bd)
{
	//Wipe out temp space with 0 alpha (transparent)

	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawClearTemp %ld+%ld\n", lDrawCnt++, bd->w,
		        bd->h  );
	}
	cairo_t* cairo = cairo_create(bd->temp_surface);

	cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (cairo, CAIRO_OPERATOR_SOURCE);
	cairo_move_to(cairo, 0, 0);
	cairo_rel_line_to(cairo, bd->w, 0);
	cairo_rel_line_to(cairo, 0, bd->h);
	cairo_rel_line_to(cairo, -bd->w, 0);
	cairo_fill(cairo);
	cairo_destroy(cairo);

	if (bd->widget && !bd->delayUpdate) {
		gtk_widget_queue_draw(bd->widget);
	}
}

void wDrawClear(
        wDraw_p bd )
{
	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawClear %ld+%ld\n", lDrawCnt++, bd->w,
		        bd->h  );
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid,
	                 wDrawColorWhite, 0);
	cairo_move_to(cairo, 0, 0);
	cairo_rel_line_to(cairo, bd->w, 0);
	cairo_rel_line_to(cairo, 0, bd->h);
	cairo_rel_line_to(cairo, -bd->w, 0);
	cairo_fill(cairo);
	gtkDrawDestroyCairoContext(cairo);
	gtk_widget_queue_draw(bd->widget);
	wDrawClearTemp(bd);
}

void * wDrawGetContext(
        wDraw_p bd )
{
	return bd->context;
}

/*******************************************************************************
 *
 * Bit Maps
 *
*******************************************************************************/


wDrawBitMap_p wDrawBitMapCreate(
        wDraw_p bd,
        int w,
        int h,
        int x,
        int y,
        const unsigned char * fbits )
{
	wDrawBitMap_p bm;

	bm = (wDrawBitMap_p)malloc( sizeof *bm );
	bm->w = w;
	bm->h = h;
	/*bm->pixmap = gtkMakeIcon( NULL, fbits, w, h, wDrawColorBlack, &bm->mask );*/
	bm->bits = fbits;
	bm->x = x;
	bm->y = y;
	return bm;
}


void wDrawBitMap(
        wDraw_p bd,
        wDrawBitMap_p bm,
        wDrawPix_t x, wDrawPix_t y,
        wDrawColor color,
        wDrawOpts opts )
{
	GdkRectangle update_rect;

	int i, j, wb;
	wDrawPix_t xx, yy;
	wControl_p b = (wControl_p)bd;
	GdkPixbuf * gdk_pixbuf, * cairo_pixbuf;
	cairo_surface_t * surface = NULL;

	x = INMAPX( bd, x-bm->x );
	y = INMAPY( bd, y-bm->y )-bm->h;
	wb = (bm->w+7)/8;


	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color,
	                 opts);
	cairo_pixbuf = bd->pixbuf;


	for ( i=0; i<bm->w; i++ )
		for ( j=0; j<bm->h; j++ )
			if ( bm->bits[ j*wb+(i>>3) ] & (1<<(i&07)) ) {
				xx = x+i;
				yy = y+j;
#ifdef CURSOR_SURFACE
				if ( 0 <= xx && xx < bd->w &&
				     0 <= yy && yy < bd->h ) {
					gdk_pixbuf = bd->pixbuf;
					b = (wControl_p)bd;
				} else if ( (opts&wDrawOptNoClip) != 0 ) {
					xx += bd->realX;
					yy += bd->realY;
					b = wlibGetControlFromPos( bd->parent, xx, yy );
					if ( b ) {
						if ( b->type == B_DRAW ) {
							gdk_pixbuf = ((wDraw_p)b)->pixbuf;
						} else {
							gdk_pixbuf = gdk_pixbuf_get_from_window(gtk_widget_get_window(bd->widget),xx,yy,
							                                        bd->w, bd->h);
						}
						xx -= b->realX;
						yy -= b->realY;
					} else {
						gdk_pixbuf = gdk_pixbuf_get_from_window(gtk_widget_get_window(bd->widget),xx,yy,
						                                        bd->w, bd->h);
					}
				} else {
					continue;
				}

				if (new_widget != widget) {
					if (cairo) {
						cairo_destroy(cairo);
					}
					cairo = NULL;
					if (widget && (widget != bd->parent->widget)) {
						gtk_widget_queue_draw(GTK_WIDGET(widget));
					}
					if ( (opts&wDrawOptCursor) || (opts&wDrawOptCursorRmv)
					     || (opts&wDrawOptCursorQuit)) {
						if (!b) { b = (wControl_p)(bd->parent->widget); }
						cairo = CreateCursorSurface(b,&b->cursor_surface, b->w, b->h, color, opts);
						widget = b->widget;
						gc = NULL;
						if ((opts&wDrawOptCursorRmv) || (opts&wDrawOptCursorQuit)) {
							b->cursor_surface.show = FALSE;
						} else {
							b->cursor_surface.show = TRUE;
						}
					} else {
						continue;
					}
					widget = new_widget;
				}
				if ((opts&wDrawOptCursorQuit) || (opts&wDrawOptCursorQuit) ) { continue; }
#endif
				cairo_rectangle(cairo, xx, yy, 1, 1);
				cairo_fill(cairo);
				if ( b && b->type == B_DRAW ) {
					gtk_widget_queue_draw_area( bd->widget, xx-1, yy-1, 3, 3 );
				}
			}
	gtkDrawDestroyCairoContext(cairo);
	gtk_widget_queue_draw(bd->widget);
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
        wDraw_p bd,
        wWinPix_t w,
        wWinPix_t h, void * redraw)
{
	wBool_t repaint;
	if (bd == NULL) {
		fprintf(stderr,"resizeDraw: no client data\n");
		return;
	}
	if ( iDrawLog >= 1 ) {
		printf( "%ld: wDrawSetSize %ld+%ld %s\n", lDrawCnt, w, h, redraw?"Redraw":"" );
	}
	// if (drawControl->fromTemplate && !(drawControl->option&BD_RESIZEABLE)) {
	// 	GtkAllocation alloc;
	// 	gtk_widget_get_allocation(drawControl->widget, &alloc);
	// 	w = alloc.width;
	// 	h = alloc.height;
	// }

	/* Negative values crashes the program */
	if (w < 0 || h < 0) {
		return;
	}

	repaint = (w != bd->w || h != bd->h);
	bd->w = w;
	bd->h = h;
	gtk_widget_set_size_request( bd->widget, w, h );
	if (repaint) {
		if (bd->surface) {
			cairo_surface_destroy(bd->surface);
		}
		bd->surface = gdk_window_create_similar_surface(gtk_widget_get_window (
		                        bd->widget), CAIRO_CONTENT_COLOR_ALPHA, w, h);

		if (bd->temp_surface) {
			cairo_surface_destroy(bd->temp_surface);
		}
		bd->temp_surface = gdk_window_create_similar_surface(gtk_widget_get_window (
		                           bd->widget), CAIRO_CONTENT_COLOR_ALPHA, w, h);


		wDrawClear( bd );
		if (!redraw) {
			bd->redraw( bd, bd->context, w, h );
		}
	}
	/*wRedraw( drawControl );*/
	gtk_widget_queue_draw(bd->widget);
}


void wDrawGetSize(
        wDraw_p bd,
        wWinPix_t *w,
        wWinPix_t *h )
{
	if (bd->widget) {
		wlibControlGetSize( (wControl_p)bd );
	}
	*w = bd->w-2;
	*h = bd->h-2;
}

/**
 * Return the resolution of a device in dpi
 *
 * \param d IN the device
 * \return    the resolution in dpi
 */

double wDrawGetDPI(
        wDraw_p d )
{
	//if (d == &psPrint_d)
	//return 1440.0;
	//else
	return d->dpi;
}


double wDrawGetMaxRadius(
        wDraw_p d )
{
	if (d == &psPrint_d) {
		return 10e9;
	} else {
		return 32767.0;
	}
}


void wDrawClip(
        wDraw_p d,
        wDrawPix_t x,
        wDrawPix_t y,
        wDrawPix_t w,
        wDrawPix_t h )
{
	d->rect.width = w;
	d->rect.height = h;
	d->rect.x = INMAPX( d, x );
	d->rect.y = INMAPY( d, y ) - d->rect.height;
	d->clip_set = TRUE;

}

/*******************************************************************************
 *
 * Background
 *
 ******************************************************************************/
int wDrawSetBackground(    wDraw_p bd, char * path, char ** error)
{

	GError *err = NULL;

	if (bd->background) {
		g_object_unref(bd->background);
	}

	if (path) {
		bd->background = gdk_pixbuf_new_from_file (path, &err);
		if (!bd->background) {
			*error = err->message;
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
wDrawCloneBackground(wDraw_p from, wDraw_p to)
{
	if (from->background) {
		to->background = from->background;
	} else {
		to->background = NULL;
	}
}

void wDrawShowBackground( wDraw_p drawControl, wWinPix_t pos_x, wWinPix_t pos_y,
                          wWinPix_t size, wAngle_t angle, int screen)
{

	if (drawControl->background) {
		cairo_t* cairo = gtkDrawCreateCairoContext(drawControl, NULL, 0, wDrawLineSolid,
		                 wDrawColorWhite, drawControl->bTempMode?wDrawOptTemp:0 );
		cairo_save(cairo);
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
		posy = (double)drawControl->h-((pixels_height*fabs(cos(rad))+pixels_width*fabs(sin(
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




