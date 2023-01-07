/** \file gtkdraw-cairo.c
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

// Trace low level drawing actions
int iDrawLog = 0;

#include "gtkint.h"
#include "gdk/gdkkeysyms.h"

#define gtkAddHelpString( a, b ) wlibAddHelpString( a, b )

#define CENTERMARK_LENGTH (6)

static long drawVerbose = 0;

// Hack to do TempRedraw or MainRedraw
// For Windows only
wBool_t wDrawDoTempDraw = TRUE;

struct wDrawBitMap_t {
		int w;
		int h;
		int x;
		int y;
		const unsigned char * bits;
		GdkPixmap * pixmap;
		GdkBitmap * mask;
		};

//struct wDraw_t {
		//WOBJ_COMMON
		//void * context;
		//wDrawActionCallBack_p action;
		//wDrawRedrawCallBack_p redraw;

		//GdkPixmap * pixmap;
		//GdkPixmap * pixmapBackup;

		//double dpi;

		//GdkGC * gc;
		//wDrawWidth lineWidth;
		//wDrawOpts opts;
		//wWinPix_t maxW;
		//wWinPix_t maxH;
		//unsigned long lastColor;
		//wBool_t lastColorInverted;
		//const char * helpStr;

		//wWinPix_t lastX;
		//wWinPix_t lastY;

		//wBool_t delayUpdate;
		//};

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

static cairo_t* gtkDrawCreateCairoContext(
		wDraw_p bd,
		GdkDrawable * win,
		wDrawWidth width,
		wDrawLineType_e lineType,
		wDrawColor color,
		wDrawOpts opts )
{
	cairo_t* cairo;

	if (win)
		cairo = gdk_cairo_create(win);
	else {
		if (opts & wDrawOptTemp) {
			if ( ! bd->bTempMode )
				printf( "Temp draw in Main Mode. Contact Developers. See %s:%d\n", "gtkdraw-cario.c", __LINE__+1 );
/* Temp Draw In Main Mode:
	You are seeing this message because there is a wDraw*() call on tempD but you are not in the context of TempRedraw()
	Typically this happens when Cmd<Object>() is processing a C_DOWN or C_MOVE action and it writes directly to tempD
	Instead it sould set some state which allows c_redraw to do the actual drawing
	If you set a break point on the printf you'll see the offending wDraw*() call in the traceback
	It should be sufficient to remove that draw code or move it to C_REDRAW
	This is not fatal but the draw will be ineffective because the next TempRedraw() will erase the temp surface
	before the expose event can copy (or bitblt) it
*/
			cairo = cairo_create(bd->temp_surface);
		} else {
			if ( bd->bTempMode )
				printf( "Main draw in Temp Mode. Contact Developers. See %s:%d\n", "gtkdraw-cario.c", __LINE__+1 );
/* Main Draw In Temp Mode:
	You are seeing this message because there is a wDraw*() call on mainD but you are in the context of TempRedraw()
	Typically this happens when C_REDRAW action calls wDraw*() on mainD, in which case it should be writing to tempD.
	Or the wDraw*() call should be removed if it is redundant.
	If you set a break point on the printf you'll see the offending wDraw*() call in the traceback
	This is not fatal but could result in garbage being left on the screen if the command is cancelled.
*/
			cairo = gdk_cairo_create(bd->pixmap);
		}
	}

	width = width ? abs(width) : 1;
	if ( color == wDrawColorWhite )
		width += 1;  // Remove ghosts
	cairo_set_line_width(cairo, width);

	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_MITER);

	switch(lineType)
	{
		case wDrawLineSolid:
		{
			cairo_set_dash(cairo, 0, 0, 0);
			break;
		}
		case wDrawLineDash:
		{
			double dashes[] = { 5, 3 };
			static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
			cairo_set_dash(cairo, dashes, len_dashes, 0);
			break;
		}
		case wDrawLineDot:
		{
			double dashes[] = { 1, 2 };
			static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
			cairo_set_dash(cairo, dashes, len_dashes, 0);
			break;
		}
		case wDrawLineDashDot:
		{
			double dashes[] = { 5, 2, 1, 2 };
			static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
			cairo_set_dash(cairo, dashes, len_dashes, 0);
			break;
		}
		case wDrawLineDashDotDot:
		{
			double dashes[] = { 5, 2, 1, 2, 1, 2 };
			static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
			cairo_set_dash(cairo, dashes, len_dashes, 0);
			break;
		}
		case wDrawLineCenter:
		{
			double dashes[] = { 8, 3, 5, 3};
			static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
			cairo_set_dash(cairo, dashes, len_dashes, 0.0);
			break;
		}
		case wDrawLinePhantom:
		{
			double dashes[] = { 8, 3, 5, 3, 5, 3};
			static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);
			cairo_set_dash(cairo, dashes, len_dashes, 0.0);
			break;
		}

	}
	GdkColor * gcolor;


	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	gcolor = wlibGetColor(color, TRUE);

	bd->lastColor = color;

	cairo_set_source_rgb(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0);

	return cairo;
}

static cairo_t* gtkDrawDestroyCairoContext(cairo_t *cairo) {
	cairo_destroy(cairo);
	return NULL;
}


void wDrawDelayUpdate(
		wDraw_p bd,
		wBool_t delay )
{
	GdkRectangle update_rect;

	if ( (!delay) && bd->delayUpdate ) {
		update_rect.x = 0;
		update_rect.y = 0;
		update_rect.width = bd->w;
		update_rect.height = bd->h;
		gtk_widget_draw( bd->widget, &update_rect );
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
//	GdkGC * gc;
//	GdkRectangle update_rect;

	if ( bd == &psPrint_d ) {
		psPrintLine( x0, y0, x1, y1, width, lineType, color, opts );
		return;
	}
	x0 = INMAPX(bd,x0);
	y0 = INMAPY(bd,y0);
	x1 = INMAPX(bd,x1);
	y1 = INMAPY(bd,y1);

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, width, lineType, color, opts);
	cairo_move_to(cairo, x0 + 0.5, y0 + 0.5);
	cairo_line_to(cairo, x1 + 0.5, y1 + 0.5);
	cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget)
		gtk_widget_queue_draw(GTK_WIDGET(bd->widget)); //,x0,y0+1,x1,y1+1);

}

/**
 * Draw an arc around a specified center
 *
 * \param bd IN ?
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
		psPrintArc( x0, y0, r, angle0, angle1, drawCenter, width, lineType, color, opts );
		return;
	}

	if (r < 6.0/75.0) return;
	x = INMAPX(bd,x0-r);
	y = INMAPY(bd,y0+r);
	w = 2*r;
	h = 2*r;

	// now create the new arc
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, width, lineType, color, opts);
	cairo_new_path(cairo);

	// its center point marker
	if(drawCenter)
	{
		// draw a small crosshair to mark the center of the curve
		cairo_move_to(cairo,  INMAPX(bd, x0 - (CENTERMARK_LENGTH / 2)), INMAPY(bd, y0 ));
		cairo_line_to(cairo, INMAPX(bd, x0 + (CENTERMARK_LENGTH / 2)), INMAPY(bd, y0 ));
		cairo_move_to(cairo, INMAPX(bd, x0), INMAPY(bd, y0 - (CENTERMARK_LENGTH / 2 )));
		cairo_line_to(cairo, INMAPX(bd, x0) , INMAPY(bd, y0  + (CENTERMARK_LENGTH / 2)));
		cairo_new_sub_path( cairo );

	}

	// draw the curve itself
	cairo_arc_negative(cairo, INMAPX(bd, x0), INMAPY(bd, y0), r, (angle0 - 90 + angle1) * (M_PI / 180.0), (angle0 - 90) * (M_PI / 180.0));
	cairo_stroke(cairo);

	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget && !bd->delayUpdate)
			gtk_widget_queue_draw_area(bd->widget,x,y,w,h);



}

 void wDrawPoint(
		wDraw_p bd,
		wDrawPix_t x0, wDrawPix_t y0,
		wDrawColor color,
		wDrawOpts opts )
{
//	GdkRectangle update_rect;

	if ( bd == &psPrint_d ) {
		/*psPrintArc( x0, y0, r, angle0, angle1, drawCenter, width, lineType, color, opts );*/
		return;
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opts);
	cairo_new_path(cairo);
	cairo_arc(cairo, INMAPX(bd, x0), INMAPY(bd, y0), 0.75, 0, 2 * M_PI);
	cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget && !bd->delayUpdate)
		gtk_widget_queue_draw_area(bd->widget,INMAPX(bd,x0-0.75),INMAPY(bd,y0+0.75),2,2);

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
		psPrintString( x, y, a, (char *) s, fp, fs, color, opts );
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y);

	/* draw text */
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opts);

	cairo_save( cairo );
	cairo_identity_matrix(cairo);

	layout = wlibFontCreatePangoLayout(bd->widget, cairo, fp, fs, s,
			  &w, &h,
			  &ascent, &descent, &baseline);

	/* cairo does not support the old method of text removal by overwrite;
	 * if color is White, then overwrite old text with a White rectangle */
	GdkColor* const gcolor = wlibGetColor(color, TRUE);
	cairo_set_source_rgb(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0);

	cairo_translate( cairo, x, y );
	cairo_rotate( cairo, angle );
	cairo_translate( cairo, 0, -baseline);

	cairo_move_to(cairo, 0, 0);

	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);
	wlibFontDestroyPangoLayout(layout);
	cairo_restore( cairo );
	gtkDrawDestroyCairoContext(cairo);

	if (bd->delayUpdate || bd->widget == NULL) return;

	/* recalculate the area to be updated
	 * for simplicity sake I added plain text height ascent and descent,
	 * mathematically correct would be to use the trigonometrical functions as well
	 */
	update_rect.x      = (gint) x - 2;
	update_rect.y      = (gint) y - (gint) (baseline + descent) - 2;
	update_rect.width  = (gint) (w * cos( angle ) + h * sin(angle))+2;
	update_rect.height = (gint) (h * sin( angle ) + w * cos(angle))+2;
	gtk_widget_draw(bd->widget, &update_rect);
	if (bd->widget && !bd->delayUpdate)
		gtk_widget_queue_draw_area(bd->widget, update_rect.x, update_rect.y, update_rect.width, update_rect.height);
    
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
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, wDrawColorBlack, bd->bTempMode?wDrawOptTemp:0 );

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

	if (debugWindow >= 3)
		fprintf(stderr, "text metrics: w=%0.1f, h=%0.1f, d=%0.1f\n", *w, *h, *d);

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
		GdkColor * gcolor = wlibGetColor(color, TRUE);
		cairo_set_source_rgba(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0, 1.0);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_stroke_preserve(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0, 0.3);
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
//	GdkRectangle update_rect;

	if ( bd == &psPrint_d ) {
		psPrintFillRectangle( x, y, w, h, color, opt );
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y)-h;

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opt);

	cairo_move_to(cairo, x, y);
	cairo_rel_line_to(cairo, w, 0);
	cairo_rel_line_to(cairo, 0, h);
	cairo_rel_line_to(cairo, -w, 0);
	cairo_rel_line_to(cairo, 0, -h);
	wlibDrawFilled( cairo, color, opt );

	gtkDrawDestroyCairoContext(cairo);
	if (bd->widget && !bd->delayUpdate)
		gtk_widget_queue_draw_area(GTK_WIDGET(bd->widget),x,y,w,h);

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
		psPrintFillPolygon( p, type, cnt, color, opt, fill, open );
		return;
	}

		if (cnt > maxCnt) {
		if (points == NULL)
			points = (GdkPoint*)malloc( cnt*sizeof *points );
		else
			points = (GdkPoint*)realloc( points, cnt*sizeof *points );
		if (points == NULL)
			abort();
		maxCnt = cnt;
	}
	wDrawPix_t min_x,max_x,min_y,max_y;
	min_x = max_x = INMAPX(bd,p[0][0]);
	min_y = max_y = INMAPY(bd,p[0][1]);
    for (i=0; i<cnt; i++) {
    	points[i].x = INMAPX(bd,p[i][0]);
    	if (points[i].x < min_x) min_x = points[i].x;
    	if (points[i].y < min_y) min_y = points[i].y;
    	if (points[i].x > max_x) max_x = points[i].x;
    	if (points[i].y > max_y) max_y = points[i].y;
    	points[i].y = INMAPY(bd,p[i][1]);
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, fill?0:dw, fill?wDrawLineSolid:lt, color, opt);

	for(i = 0; i < cnt; ++i)
	{
		int j = i-1;
		int k = i+1;
		if (j < 0) j = cnt-1;
		if (k > cnt-1) k = 0;
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
				if (type[i] == 1)
					cairo_curve_to(cairo, points[i].x, points[i].y, points[i].x, points[i].y, mid1.x, mid1.y);
				else
					cairo_curve_to(cairo, mid3.x, mid3.y, mid4.x, mid4.y, mid1.x, mid1.y);
				save = mid0;
			}
		} else if (!type || type[i] == wPolyLineStraight || (open && (i==cnt-1))) {
			cairo_line_to(cairo, points[i].x, points[i].y);
		} else {
			cairo_line_to(cairo, mid0.x, mid0.y);
			if (type[i] == wPolyLineSmooth)
				cairo_curve_to(cairo, points[i].x, points[i].y, points[i].x, points[i].y, mid1.x, mid1.y);
			else
				cairo_curve_to(cairo, mid3.x, mid3.y, mid4.x, mid4.y, mid1.x, mid1.y);
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
	if (bd->widget && !bd->delayUpdate)
			gtk_widget_queue_draw_area(GTK_WIDGET(bd->widget),min_x,min_y,max_x-min_y,max_y-min_y);

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
		psPrintFillCircle( x0, y0, r, color, opt );
		return;
	}

	x = INMAPX(bd,x0-r);
	y = INMAPY(bd,y0+r);
	w = 2*r;
	h = 2*r;

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opt);
	cairo_arc(cairo, INMAPX(bd, x0), INMAPY(bd, y0), r, 0, 2 * M_PI);
	wlibDrawFilled( cairo, color, opt );
	gtkDrawDestroyCairoContext(cairo);

	if (bd->widget && !bd->delayUpdate)
			gtk_widget_queue_draw_area(GTK_WIDGET(bd->widget),x,y,w,h);

}

 void wDrawClearTemp(wDraw_p bd) {
	//Wipe out temp space with 0 alpha (transparent)

	static long cDCT = 0;
	if ( iDrawLog )
		printf( "wDrawClearTemp %ld\n", cDCT++ );
	cairo_t* cairo = cairo_create(bd->temp_surface);

	cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator (cairo, CAIRO_OPERATOR_SOURCE);
	cairo_move_to(cairo, 0, 0);
	cairo_rel_line_to(cairo, bd->w, 0);
	cairo_rel_line_to(cairo, 0, bd->h);
	cairo_rel_line_to(cairo, -bd->w, 0);
	cairo_fill(cairo);
	cairo_destroy(cairo);

	if (bd->widget && !bd->delayUpdate)
		gtk_widget_queue_draw(bd->widget);
 }

 void wDrawClear(
		wDraw_p bd )
{

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, wDrawColorWhite, 0);
	cairo_move_to(cairo, 0, 0);
	cairo_rel_line_to(cairo, bd->w, 0);
	cairo_rel_line_to(cairo, 0, bd->h);
	cairo_rel_line_to(cairo, -bd->w, 0);
	cairo_fill(cairo);
	if (bd->widget)
		gtk_widget_queue_draw(bd->widget);
	gtkDrawDestroyCairoContext(cairo);

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
	int i, j, wb;
	wDrawPix_t xx, yy;
	GtkWidget * widget = bd->widget;
	
	static long cDBM = 0;
	if ( iDrawLog )
		printf( "wDrawBitMap %ld\n", cDBM++ );

	x = INMAPX( bd, x-bm->x );
	y = INMAPY( bd, y-bm->y )-bm->h;
	wb = (bm->w+7)/8;

	cairo_t* cairo;

	cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opts);


	for ( i=0; i<bm->w; i++ )
		for ( j=0; j<bm->h; j++ )
			if ( bm->bits[ j*wb+(i>>3) ] & (1<<(i&07)) ) {
				xx = x+i;
				yy = y+j;
				cairo_rectangle(cairo, xx, yy, 1, 1);
				cairo_fill(cairo);
			}

	cairo_destroy(cairo);

	if (widget && !bd->delayUpdate)
		gtk_widget_queue_draw_area(GTK_WIDGET(widget), x, y, bm->w, bm->h);

}


/*******************************************************************************
 *
 * Event Handlers
 *
*******************************************************************************/



 void wDrawSaveImage(
		wDraw_p bd )
{
	cairo_t * cr;
	if ( bd->pixmapBackup ) {
		gdk_pixmap_unref( bd->pixmapBackup );
	}
	bd->pixmapBackup = gdk_pixmap_new( bd->widget->window, bd->w, bd->h, -1 );

	cr = gdk_cairo_create(bd->pixmapBackup);
	gdk_cairo_set_source_pixmap(cr, bd->pixmap, 0, 0);
    cairo_paint(cr);
	cairo_destroy(cr);

	cr = NULL;

}


 void wDrawRestoreImage(
		wDraw_p bd )
{
	GdkRectangle update_rect;
	if ( bd->pixmapBackup ) {

		cairo_t * cr;
		cr = gdk_cairo_create(bd->pixmap);
		gdk_cairo_set_source_pixmap(cr, bd->pixmapBackup, 0, 0);
		cairo_paint(cr);
		cairo_destroy(cr);

		cr = NULL;

		if ( bd->delayUpdate || bd->widget == NULL ) return;
		update_rect.x = 0;
		update_rect.y = 0;
		update_rect.width = bd->w;
		update_rect.height = bd->h;
		gtk_widget_draw( bd->widget, &update_rect );
	}
}


 void wDrawSetSize(
		wDraw_p bd,
		wWinPix_t w,
		wWinPix_t h , void * redraw)
{
	wBool_t repaint;
	if (bd == NULL) {
		fprintf(stderr,"resizeDraw: no client data\n");
		return;
	}

	/* Negative values crashes the program */
	if ( w <= 0 || h <= 0 ) {
		fprintf( stderr, "wDrawSetSize bad size %ldx%ld\n", w, h );
		if ( w <= 0 )
			w = 100;
		if ( h <= 0 )
			h = 100;
	}

	repaint = (w != bd->w || h != bd->h);
	bd->w = w;
	bd->h = h;
	gtk_widget_set_size_request( bd->widget, w, h );
	if (repaint)
	{
		if (bd->pixmap)
			gdk_pixmap_unref( bd->pixmap );
		bd->pixmap = gdk_pixmap_new( bd->widget->window, w, h, -1 );
		if (bd->temp_surface)
			cairo_surface_destroy( bd->temp_surface);
		bd->temp_surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, w,h );

		wDrawClear( bd );
		if (!redraw)
			bd->redraw( bd, bd->context, w, h );
	}
	/*wRedraw( bd )*/;
}


 void wDrawGetSize(
		wDraw_p bd,
		wWinPix_t *w,
		wWinPix_t *h )
{
	if (bd->widget)
		wlibControlGetSize( (wControl_p)bd );
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
	if (d == &psPrint_d)
		return 10e9;
	else
		return 32767.0;
}


 void wDrawClip(
		wDraw_p d,
		wDrawPix_t x,
		wDrawPix_t y,
		wDrawPix_t w,
		wDrawPix_t h )
{
	GdkRectangle rect;
	rect.width = (wWinPix_t)w;
	rect.height = (wWinPix_t)h;
	rect.x = (wWinPix_t)INMAPX( d, x );
	rect.y = (wWinPix_t)INMAPY( d, y ) - rect.height;
	gdk_gc_set_clip_rectangle( d->gc, &rect );

}


static gint draw_expose_event(
		GtkWidget *widget,
		GdkEventExpose *event,
		wDraw_p bd)
{
	static long cDEE = 0;
	if ( iDrawLog )
		printf( "draw_expose_event %ld %dx%d+%dx%d %ldx%ld+%ldx%ld\n", cDEE++,
			event->area.x, event->area.y, event->area.width, event->area.height,
			0L, bd->w, 0L, bd->h );

	cairo_t* cairo = gdk_cairo_create (widget->window);
	gdk_cairo_set_source_pixmap(cairo,bd->pixmap,0,0);
	cairo_rectangle(cairo,event->area.x, event->area.y,
					event->area.width, event->area.height);
	cairo_set_operator(cairo,CAIRO_OPERATOR_SOURCE);
	cairo_fill(cairo);

	cairo_set_source_surface(cairo,bd->temp_surface,0,0);
	cairo_rectangle(cairo,event->area.x, event->area.y,
				event->area.width, event->area.height);
	cairo_set_operator(cairo,CAIRO_OPERATOR_OVER);
	cairo_fill(cairo);

	cairo_destroy(cairo);

	return TRUE;
}


static gint draw_configure_event(
		GtkWidget *widget,
		GdkEventConfigure *event,
		wDraw_p bd)
{
	return TRUE;
}

static const char * actionNames[] = { "None", "Move", "LDown", "LDrag", "LUp", "RDown", "RDrag", "RUp", "Text", "ExtKey", "WUp", "WDown", "DblL", "ModK", "ScrU", "ScrD", "ScrL", "ScrR" };

/**
 * Handler for scroll events, ie mouse wheel activity
 */

static int scrollTimer;
static int timer_busy_count;
static wAction_t lastAction;

static int ScrollTimerPop(wDraw_p bd) {

		if (timer_busy_count>1) {
			 timer_busy_count = 0;
			 scrollTimer = 0;
		} else {
			timer_busy_count++;
			return TRUE;
		}
		if (drawVerbose >= 2)
			printf( "%s-Pop\n", actionNames[lastAction] );
		bd->action( bd, bd->context, lastAction, (wDrawPix_t)0, (wDrawPix_t)0 );

		return FALSE;
}


static gint draw_scroll_event(
		GtkWidget *widget,
		GdkEventScroll *event,
		wDraw_p bd)
{
	wAction_t action = 0;
	static int oldEventX = 0;
	static int oldEventY = 0;
	static int newEventX = 0;
	static int newEventY = 0;

	if (event->state & (GDK_SHIFT_MASK|GDK_BUTTON2_MASK|GDK_MOD1_MASK)) {

		newEventX = OUTMAPX(bd, event->x);
		newEventY = OUTMAPY(bd, event->y);
		oldEventX = OUTMAPX(bd, event->x_root);
		oldEventY = OUTMAPX(bd, event->y_root);

		switch( event->direction ) {
			case GDK_SCROLL_UP:
				if (event->state & GDK_CONTROL_MASK)
					action = wActionScrollRight;
				else
					action = wActionScrollUp;
				break;
			case GDK_SCROLL_DOWN:
				if (event->state & GDK_CONTROL_MASK)
					action = wActionScrollLeft;
				else
					action = wActionScrollDown;
				break;
			case GDK_SCROLL_LEFT:
				action = wActionScrollLeft;
				break;
			case GDK_SCROLL_RIGHT:
				action = wActionScrollRight;
				break;
			default:
				return TRUE;
				break;
		}

		if (drawVerbose >= 2)
			printf( "%sNew[%dx%d]Delta[%dx%d]\n", actionNames[action],
				newEventX, newEventY, oldEventX, oldEventY );



		if (scrollTimer) {					// Already have a timer
			lastAction = action;
                return TRUE;
            } else {
            	 lastAction = action;
            	 timer_busy_count = 0;
                 scrollTimer = g_timeout_add(25,(GSourceFunc)ScrollTimerPop,bd);   // 25ms delay
                 return TRUE;
            }


	} else {

		switch( event->direction ) {
		case GDK_SCROLL_UP:
			action = wActionWheelUp;
			break;
		case GDK_SCROLL_DOWN:
			action = wActionWheelDown;
			break;
		case GDK_SCROLL_LEFT:
			return TRUE;
			break;
		case GDK_SCROLL_RIGHT:
			return TRUE;
			break;
		default:
			break;
		}
	}

	if (action != 0) {
		if (drawVerbose >= 2)
			printf( "%s[%ldx%ld]\n", actionNames[action], bd->lastX, bd->lastY );
		bd->action( bd, bd->context, action, (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY);
	}

	return TRUE;
}



static gint draw_leave_event(
		GtkWidget *widget,
		GdkEvent * event )
{
	wlibHelpHideBalloon();
	return TRUE;
}


/**
 * Handler for mouse button clicks.
 */



static gint draw_button_event(
		GtkWidget *widget,
		GdkEventButton *event,
		wDraw_p bd )
{

	wAction_t action = 0;

	if (bd->action == NULL)
		return TRUE;

	bd->lastX = OUTMAPX(bd, event->x);
	bd->lastY = OUTMAPY(bd, event->y);

	if ()
	printf("Button Event %d %s \n",event->button, event->type == GDK_BUTTON_PRESS?"Press":"Release");

	switch ( event->button ) {
	case 1: /* left mouse button */
	case 2: /* middle mouse button */
		action = event->type==GDK_BUTTON_PRESS?wActionLDown:wActionLUp;
		if (event->type==GDK_2BUTTON_PRESS) action = wActionLDownDouble;
		/*bd->action( bd, bd->context, event->type==GDK_BUTTON_PRESS?wActionLDown:wActionLUp, (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );*/
		break;
	case 3: /* right mouse button */
		action = event->type==GDK_BUTTON_PRESS?wActionRDown:wActionRUp;
		/*bd->action( bd, bd->context, event->type==GDK_BUTTON_PRESS?wActionRDown:wActionRUp, (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );*/
		break;
	}
	if (action != 0) {
		if (drawVerbose >= 2)
			printf( "%s[%ldx%ld]\n", actionNames[action], bd->lastX, bd->lastY );
		bd->action( bd, bd->context, action, (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
	}

	if (!(bd->option & BD_NOFOCUS))
		gtk_widget_grab_focus( bd->widget );
	return TRUE;
}

static gint draw_motion_event(
		GtkWidget *widget,
		GdkEventMotion *event,
		wDraw_p bd )
{
	int x, y;
	GdkModifierType state;
	wAction_t action;

	if (bd->action == NULL)
		return TRUE;

	if (event->is_hint) {
		gdk_window_get_pointer (event->window, &x, &y, &state);
	} else {
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if (state & GDK_BUTTON1_MASK) {
		action = wActionLDrag;
	} else if (state & GDK_BUTTON2_MASK) {
		action = wActionLDrag;
	} else if (state & GDK_BUTTON3_MASK) {
			action = wActionRDrag;
	} else {
		action = wActionMove;
	}
	bd->lastX = OUTMAPX(bd, x);
	bd->lastY = OUTMAPY(bd, y);
	if (drawVerbose >= 2)
		printf( "%lx: %s[%ldx%ld] %s\n", (long)bd, actionNames[action], bd->lastX, bd->lastY, event->is_hint?"<Hint>":"<>" );
	bd->action( bd, bd->context, action, (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
	if (!(bd->option & BD_NOFOCUS))
		gtk_widget_grab_focus( bd->widget );
	return TRUE;
}

static gint draw_char_release_event(
		GtkWidget * widget,
		GdkEventKey *event,
		wDraw_p bd )
{
//		GdkModifierType modifiers;
		guint key = event->keyval;
		wModKey_e modKey = wModKey_None;
		switch (key) {
			case GDK_KEY_Alt_L:     modKey = wModKey_Alt; break;
			case GDK_KEY_Alt_R:     modKey = wModKey_Alt; break;
			case GDK_KEY_Shift_L:	modKey = wModKey_Shift; break;
			case GDK_KEY_Shift_R:	modKey = wModKey_Shift; break;
			case GDK_KEY_Control_L:	modKey = wModKey_Ctrl; break;
			case GDK_KEY_Control_R:	modKey = wModKey_Ctrl; break;
				default: ;
		}

		if (modKey!= wModKey_None && (bd->option & BD_MODKEYS)) {
			 bd->action(bd, bd->context, wActionModKey+((int)modKey<<8), (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
			 	 if (!(bd->option & BD_NOFOCUS))
			 		 gtk_widget_grab_focus( bd->widget );
			 	 return TRUE;
		} else {
			return FALSE;
		}
		return FALSE;
}


static gint draw_char_event(
		GtkWidget * widget,
		GdkEventKey *event,
		wDraw_p bd )
{
	GdkModifierType modifiers;
	guint key = event->keyval;
	wAccelKey_e extKey = wAccelKey_None;
	wModKey_e modKey = wModKey_None;
	switch (key) {
	case GDK_KEY_Escape:	key = 0x1B; break;
	case GDK_KEY_Return:
		modifiers = gtk_accelerator_get_default_mod_mask();
		if (((event->state & modifiers)==GDK_CONTROL_MASK) || ((event->state & modifiers)==GDK_MOD1_MASK))
			extKey = wAccelKey_LineFeed;  //If Return plus Control or Alt send in LineFeed
		key = 0x0D;
		break;
	case GDK_KEY_Linefeed:	key = 0x0A; break;
	case GDK_KEY_Tab:	key = 0x09; break;
	case GDK_KEY_BackSpace:	key = 0x08; break;
	case GDK_KEY_Delete:    extKey = wAccelKey_Del; break;
	case GDK_KEY_Insert:    extKey = wAccelKey_Ins; break;
	case GDK_KEY_Home:      extKey = wAccelKey_Home; break;
	case GDK_KEY_End:       extKey = wAccelKey_End; break;
	case GDK_KEY_Page_Up:   extKey = wAccelKey_Pgup; break;
	case GDK_KEY_Page_Down: extKey = wAccelKey_Pgdn; break;
	case GDK_KEY_Up:        extKey = wAccelKey_Up; break;
	case GDK_KEY_Down:      extKey = wAccelKey_Down; break;
	case GDK_KEY_Right:     extKey = wAccelKey_Right; break;
	case GDK_KEY_Left:      extKey = wAccelKey_Left; break;
	case GDK_KEY_F1:        extKey = wAccelKey_F1; break;
	case GDK_KEY_F2:        extKey = wAccelKey_F2; break;
	case GDK_KEY_F3:        extKey = wAccelKey_F3; break;
	case GDK_KEY_F4:        extKey = wAccelKey_F4; break;
	case GDK_KEY_F5:        extKey = wAccelKey_F5; break;
	case GDK_KEY_F6:        extKey = wAccelKey_F6; break;
	case GDK_KEY_F7:        extKey = wAccelKey_F7; break;
	case GDK_KEY_F8:        extKey = wAccelKey_F8; break;
	case GDK_KEY_F9:        extKey = wAccelKey_F9; break;
	case GDK_KEY_F10:       extKey = wAccelKey_F10; break;
	case GDK_KEY_F11:       extKey = wAccelKey_F11; break;
	case GDK_KEY_F12:       extKey = wAccelKey_F12; break;
	case GDK_KEY_Alt_L:     modKey = wModKey_Alt; break;
	case GDK_KEY_Alt_R:     modKey = wModKey_Alt; break;
	case GDK_KEY_Shift_L:	modKey = wModKey_Shift; break;
	case GDK_KEY_Shift_R:	modKey = wModKey_Shift; break;
	case GDK_KEY_Control_L:	modKey = wModKey_Ctrl; break;
	case GDK_KEY_Control_R:	modKey = wModKey_Ctrl; break;
		default: ;
	}

	if (extKey != wAccelKey_None) {
		if ( wlibFindAccelKey( event ) == NULL ) {
			bd->action( bd, bd->context, wActionExtKey + ((int)extKey<<8), (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
		}
		if (!(bd->option & BD_NOFOCUS))
				gtk_widget_grab_focus( bd->widget );
		return TRUE;
	} else if ((key >=wAccelKey_Up) && (key<=wAccelKey_Left) && bd->action) {
		bd->action( bd, bd->context, wActionText+(key<<8), (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
		if (!(bd->option & BD_NOFOCUS))
			gtk_widget_grab_focus( bd->widget );
		return TRUE;
	} else if (key <= 0xFF && (event->state&(GDK_CONTROL_MASK|GDK_MOD1_MASK)) == 0 && bd->action) {
		bd->action( bd, bd->context, wActionText+(key<<8), (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
		if (!(bd->option & BD_NOFOCUS))
				gtk_widget_grab_focus( bd->widget );
		return TRUE;
	} else if (modKey!= wModKey_None && (bd->option & BD_MODKEYS)) {
				bd->action(bd, bd->context, wActionModKey+((int)modKey<<8), (wDrawPix_t)bd->lastX, (wDrawPix_t)bd->lastY );
				if (!(bd->option & BD_NOFOCUS))
								gtk_widget_grab_focus( bd->widget );
				return TRUE;
	} else {
		return FALSE;
	}
}


/*******************************************************************************
 *
 * Create
 *
*******************************************************************************/



int XW = 0;
int XH = 0;
int xw, xh, cw, ch;

 wDraw_p wDrawCreate(
		wWin_p	parent,
		wWinPix_t	x,
		wWinPix_t	y,
		const char 	* helpStr,
		long	option,
		wWinPix_t	width,
		wWinPix_t	height,
		void	* context,
		wDrawRedrawCallBack_p redraw,
		wDrawActionCallBack_p action )
{
	wDraw_p bd;

	bd = (wDraw_p)wlibAlloc( parent,  B_DRAW, x, y, NULL, sizeof *bd, NULL );
	bd->option = option;
	bd->context = context;
	bd->redraw = redraw;
	bd->action = action;
	bd->bTempMode = FALSE;
	wlibComputePos( (wControl_p)bd );

	bd->widget = gtk_drawing_area_new();
	gtk_drawing_area_size( GTK_DRAWING_AREA(bd->widget), width, height );
	gtk_widget_set_size_request( GTK_WIDGET(bd->widget), width, height );
	gtk_signal_connect (GTK_OBJECT (bd->widget), "expose_event",
						   (GtkSignalFunc) draw_expose_event, bd);
	gtk_signal_connect (GTK_OBJECT(bd->widget),"configure_event",
						   (GtkSignalFunc) draw_configure_event, bd);
	gtk_signal_connect (GTK_OBJECT (bd->widget), "motion_notify_event",
						   (GtkSignalFunc) draw_motion_event, bd);
	gtk_signal_connect (GTK_OBJECT (bd->widget), "button_press_event",
						   (GtkSignalFunc) draw_button_event, bd);
	gtk_signal_connect (GTK_OBJECT (bd->widget), "button_release_event",
						   (GtkSignalFunc) draw_button_event, bd);
	gtk_signal_connect (GTK_OBJECT (bd->widget), "scroll_event",
						   (GtkSignalFunc) draw_scroll_event, bd);
	gtk_signal_connect_after (GTK_OBJECT (bd->widget), "key_press_event",
						   (GtkSignalFunc) draw_char_event, bd);
	gtk_signal_connect_after (GTK_OBJECT (bd->widget), "key_release_event",
							   (GtkSignalFunc) draw_char_release_event, bd);
	gtk_signal_connect (GTK_OBJECT (bd->widget), "leave_notify_event",
						   (GtkSignalFunc) draw_leave_event, bd);
	gtk_widget_set_can_focus(bd->widget,!(option & BD_NOFOCUS));
	//if (!(option & BD_NOFOCUS))
	//	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(bd->widget), GTK_CAN_FOCUS);
	gtk_widget_set_events (bd->widget, GDK_EXPOSURE_MASK
							  | GDK_LEAVE_NOTIFY_MASK
							  | GDK_BUTTON_PRESS_MASK
							  | GDK_BUTTON_RELEASE_MASK
							  | GDK_SCROLL_MASK
							  | GDK_POINTER_MOTION_MASK
							  | GDK_POINTER_MOTION_HINT_MASK
							  | GDK_KEY_PRESS_MASK
							  | GDK_KEY_RELEASE_MASK );
	bd->lastColor = -1;

	double dpi;

	wPrefGetFloat(PREFSECTION, DPISET, &dpi, 96.0);

	if ( width <= 0 || height <= 0 ) {
		fprintf( stderr, "wDrawCreate bad size %ldx%ld\n", width, height );
		if ( width <= 0 )
			width = 100;
		if ( height <= 0 )
			height = 100;
	}
	bd->dpi = dpi;
	bd->maxW = bd->w = width;
	bd->maxH = bd->h = height;

	gtk_fixed_put( GTK_FIXED(parent->widget), bd->widget, bd->realX, bd->realY );
	wlibControlGetSize( (wControl_p)bd );
	gtk_widget_realize( bd->widget );
	bd->pixmap = gdk_pixmap_new( bd->widget->window, width, height, -1 );
	bd->temp_surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width,height );
	wDrawClear(bd);
	bd->gc = gdk_gc_new( parent->gtkwin->window );
	gdk_gc_copy( bd->gc, parent->gtkwin->style->base_gc[GTK_STATE_NORMAL] );
{
	GdkCursor * cursor;
	cursor = gdk_cursor_new ( GDK_TCROSS );
	gdk_window_set_cursor ( bd->widget->window, cursor);
	gdk_cursor_destroy (cursor);
}
#ifdef LATER
	if (labelStr)
		bd->labelW = gtkAddLabel( (wControl_p)bd, labelStr );
#endif
	gtk_widget_show( bd->widget );
	wlibAddButton( (wControl_p)bd );
	gtkAddHelpString( bd->widget, helpStr );

	return bd;
}

/*******************************************************************************
 *
 * BitMaps
 *
*******************************************************************************/

wDraw_p wBitMapCreate(          wWinPix_t w, wWinPix_t h, int arg )
{
	wDraw_p bd;

	bd = (wDraw_p)wlibAlloc( gtkMainW,  B_DRAW, 0, 0, NULL, sizeof *bd, NULL );

	bd->lastColor = -1;

	double dpi;

    wPrefGetFloat(PREFSECTION, DPISET, &dpi, 96.0);

	bd->dpi = dpi;
	bd->maxW = bd->w = w;
	bd->maxH = bd->h = h;

	bd->pixmap = gdk_pixmap_new( gtkMainW->widget->window, w, h, -1 );
	bd->widget = gtk_pixmap_new(bd->pixmap, NULL);
	if ( bd->pixmap == NULL ) {
		wNoticeEx( NT_ERROR, "CreateBitMap: pixmap_new failed", "Ok", NULL );
		return FALSE;
	}
	bd->gc = gdk_gc_new( gtkMainW->gtkwin->window );
	if ( bd->gc == NULL ) {
		wNoticeEx( NT_ERROR, "CreateBitMap: gc_new failed", "Ok", NULL );
		return FALSE;
	}
	gdk_gc_copy( bd->gc, gtkMainW->gtkwin->style->base_gc[GTK_STATE_NORMAL] );

	wDrawClear( bd );
	return bd;
}


wBool_t wBitMapDelete(          wDraw_p d )
{
	gdk_pixmap_unref( d->pixmap );
	d->pixmap = NULL;
	return TRUE;
}

/*******************************************************************************
 *
 * Background
 *
 ******************************************************************************/
int wDrawSetBackground(    wDraw_p bd, char * path, char ** error) {

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

/**
* Draw background to screen. The background will be sized and rotated before being shown. The bitmap 
* is scaled so that the width is equal to size. The height is changed proportionally. 
*
* \param bd drawing context
* \param pos_x, pos_y bitmap position
* \param size desired width after scaling
* \param angle 
* \param screen visibility of bitmap in percent
*/

void wDrawShowBackground( wDraw_p bd, wWinPix_t pos_x, wWinPix_t pos_y, wWinPix_t size, wAngle_t angle, int screen) {

	if (bd->background) {
		cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, wDrawColorWhite, bd->bTempMode?wDrawOptTemp:0 );
		cairo_save(cairo);
		int pixels_width = gdk_pixbuf_get_width(bd->background);
		int pixels_height = gdk_pixbuf_get_height(bd->background);
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
		posy = (double)bd->h-((pixels_height*fabs(cos(rad))+pixels_width*fabs(sin(rad)))*scale)-posy;
		//width = (double)(pixels_width*scale);
		//height = (double)(pixels_height*scale);
		cairo_translate(cairo,posx,posy);
		cairo_scale(cairo, scale, scale);
		cairo_translate(cairo, fabs(pixels_width/2.0*cos(rad))+fabs(pixels_height/2.0*sin(rad)),
				fabs(pixels_width/2.0*sin(rad))+fabs(pixels_height/2.0*cos(rad)));
		cairo_rotate(cairo, M_PI*(angle/180.0));
		// We need to clip around the image, or cairo will paint garbage data
		cairo_rectangle(cairo, -pixels_width/2.0, -pixels_height/2.0, pixels_width, pixels_height);
		cairo_clip(cairo);
		gdk_cairo_set_source_pixbuf(cairo, bd->background, -pixels_width/2.0, -pixels_height/2.0);
		cairo_pattern_t *mask = cairo_pattern_create_rgba (1.0,1.0,1.0,(100.0-screen)/100.0);
		cairo_mask(cairo,mask);
		cairo_pattern_destroy(mask);
		cairo_restore(cairo);
		gtkDrawDestroyCairoContext(cairo);

		gtk_widget_queue_draw(bd->widget);
	}

}




