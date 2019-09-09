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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include "gtkint.h"
#include "gdk/gdkkeysyms.h"

#define gtkAddHelpString( a, b ) wlibAddHelpString( a, b )

#define CENTERMARK_LENGTH (6)

static long drawVerbose = 0;

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
		//wPos_t maxW;
		//wPos_t maxH;
		//unsigned long lastColor;
		//wBool_t lastColorInverted;
		//const char * helpStr;

		//wPos_t lastX;
		//wPos_t lastY;

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

static cairo_t* gtkDrawCreateCairoCursorContext(
		wControl_p ct,
		cairo_surface_t * surf,
		wDrawWidth width,
		wDrawLineType_e lineType,
		wDrawColor color,
		wDrawOpts opts )
{
	cairo_t* cairo;

	cairo = cairo_create(surf);

	width = width ? abs(width) : 1;
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
	}
	GdkColor * gcolor;


	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	gcolor = wlibGetColor(color, TRUE);

	if (ct->type == B_DRAW)  {
		wDraw_p bd = (wDraw_p)ct;
		bd->lastColor = color;
	}

	cairo_set_source_rgba(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0, 1.0);

	return cairo;
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
	else
		cairo = gdk_cairo_create(bd->pixmap);

	width = width ? abs(width) : 1;
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

cairo_t* CreateCursorSurface(wControl_p ct, wSurface_p surface, wPos_t width, wPos_t height, wDrawColor color, wDrawOpts opts) {

		cairo_t * cairo = NULL;

		if ((opts&wDrawOptCursor) || (opts&wDrawOptCursorRmv)) {

			if (surface!=NULL || surface->width != width || surface->height != height) {
				if (surface->surface) cairo_surface_destroy(surface->surface);
				surface->surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, width,height );
				surface->width = width;
				surface->height = height;

			}

			cairo = gtkDrawCreateCairoCursorContext(ct,surface->surface,0,wDrawLineSolid, color, opts);
			cairo_save(cairo);
			cairo_set_source_rgba(cairo, 0.0, 0.0, 0.0, 0.0);
			cairo_paint(cairo);
			cairo_restore(cairo);
			surface->show = TRUE;
			cairo_set_operator(cairo,CAIRO_OPERATOR_SOURCE);

		}

		return cairo;

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
		wPos_t x0, wPos_t y0,
		wPos_t x1, wPos_t y1,
		wDrawWidth width,
		wDrawLineType_e lineType,
		wDrawColor color,
		wDrawOpts opts )
{
	GdkGC * gc;
	GdkRectangle update_rect;

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
		wPos_t x0, wPos_t y0,
		wPos_t r,
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

}

 void wDrawPoint(
		wDraw_p bd,
		wPos_t x0, wPos_t y0,
		wDrawColor color,
		wDrawOpts opts )
{
	GdkRectangle update_rect;

	if ( bd == &psPrint_d ) {
		/*psPrintArc( x0, y0, r, angle0, angle1, drawCenter, width, lineType, color, opts );*/
		return;
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opts);
	cairo_new_path(cairo);
	cairo_arc(cairo, INMAPX(bd, x0), INMAPY(bd, y0), 0.75, 0, 2 * M_PI);
	cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);

}

/*******************************************************************************
 *
 * Strings
 *
 ******************************************************************************/

 void wDrawString(
		wDraw_p bd,
		wPos_t x, wPos_t y,
		wAngle_t a,
		const char * s,
		wFont_p fp,
		wFontSize_t fs,
		wDrawColor color,
		wDrawOpts opts )
{
	PangoLayout *layout;
	GdkRectangle update_rect;
	int w;
	int h;
	gint ascent;
	gint descent;
	gint baseline;
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
									  (int *) &w, (int *) &h,
									  (int *) &ascent, (int *) &descent, (int *) &baseline);

	/* cairo does not support the old method of text removal by overwrite; force always write here and
           refresh on cancel event */
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
    
}

 void wDrawGetTextSize(
		wPos_t *w,
		wPos_t *h,
		wPos_t *d,
		wDraw_p bd,
		const char * s,
		wFont_p fp,
		wFontSize_t fs )
{
	int textWidth;
	int textHeight;
	int ascent;
	int descent;
	int baseline;

	*w = 0;
	*h = 0;

	/* draw text */
	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, wDrawColorBlack, 0);

	cairo_identity_matrix(cairo);

	wlibFontDestroyPangoLayout(
		wlibFontCreatePangoLayout(bd->widget, cairo, fp, fs, s,
								 &textWidth, (int *) &textHeight,
								 (int *) &ascent, (int *) &descent, (int *) &baseline) );

	*w = (wPos_t) textWidth;
	*h = (wPos_t) textHeight;
	*d = (wPos_t) textHeight-ascent;

	if (debugWindow >= 3)
		fprintf(stderr, "text metrics: w=%d, h=%d, d=%d\n", *w, *h, *d);

	gtkDrawDestroyCairoContext(cairo);
}


/*******************************************************************************
 *
 * Basic Drawing Functions
 *
*******************************************************************************/

 void wDrawFilledRectangle(
		wDraw_p bd,
		wPos_t x,
		wPos_t y,
		wPos_t w,
		wPos_t h,
		wDrawColor color,
		wDrawOpts opt )
{
	GdkRectangle update_rect;

	if ( bd == &psPrint_d ) {
		psPrintFillRectangle( x, y, w, h, color, opt );
		return;
	}

	x = INMAPX(bd,x);
	y = INMAPY(bd,y)-h;

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opt);


	if (opt != wDrawOptOpaque) {
		cairo_move_to(cairo, x, y);
		cairo_rel_line_to(cairo, w, 0);
		cairo_rel_line_to(cairo, 0, h);
		cairo_rel_line_to(cairo, -w, 0);
		cairo_rel_line_to(cairo, 0, -h);
		cairo_set_source_rgb(cairo, 0,0,0);
		cairo_set_operator(cairo, CAIRO_OPERATOR_DIFFERENCE);
		cairo_fill(cairo);
		GdkColor * gcolor = wlibGetColor(color, TRUE);
		cairo_set_source_rgba(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0, 1.0);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_move_to(cairo, x, y);
		cairo_rel_line_to(cairo, w, 0);
		cairo_rel_line_to(cairo, 0, h);
		cairo_rel_line_to(cairo, -w, 0);
		cairo_rel_line_to(cairo, 0, -h);
		cairo_stroke(cairo);
		cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
		cairo_set_source_rgba(cairo, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0, 0.3);
	}
	cairo_move_to(cairo, x, y);
	cairo_rel_line_to(cairo, w, 0);
	cairo_rel_line_to(cairo, 0, h);
	cairo_rel_line_to(cairo, -w, 0);
	cairo_rel_line_to(cairo, 0, -h);
	cairo_fill(cairo);

	gtkDrawDestroyCairoContext(cairo);
	gtk_widget_queue_draw(GTK_WIDGET(bd->widget));

}

 void wDrawPolygon(
		wDraw_p bd,
		wPos_t p[][2],
		int type[],
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
    for (i=0; i<cnt; i++) {
    	points[i].x = INMAPX(bd,p[i][0]);
    	points[i].y = INMAPY(bd,p[i][1]);
	}

	cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, fill?0:dw, fill?lt:wDrawLineSolid, color, opt);

	for(i = 0; i < cnt; ++i)
	{
		int j = i-1;
		int k = i+1;
		if (j < 0) j = cnt-1;
		if (k > cnt-1) k = 0;
		GdkPoint mid0, mid1, mid3, mid4, save;
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
		if (type && (type[i] == 2) && (len1>0) && (len0>0)) {
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
			if (!type || type[i] == 0 || open) {
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
		} else if (!type || type[i] == 0 || (open && (i==cnt-1))) {
			cairo_line_to(cairo, points[i].x, points[i].y);
		} else {
			cairo_line_to(cairo, mid0.x, mid0.y);
			if (type[i] == 1)
				cairo_curve_to(cairo, points[i].x, points[i].y, points[i].x, points[i].y, mid1.x, mid1.y);
			else
				cairo_curve_to(cairo, mid3.x, mid3.y, mid4.x, mid4.y, mid1.x, mid1.y);
		}
		if ((i==cnt-1) && (!fill && !open)) {
			cairo_line_to(cairo, save.x, save.y);
		}
	}
	if (fill && !open) cairo_fill(cairo);
	else cairo_stroke(cairo);
	gtkDrawDestroyCairoContext(cairo);

}

 void wDrawFilledCircle(
		wDraw_p bd,
		wPos_t x0,
		wPos_t y0,
		wPos_t r,
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
	cairo_fill(cairo);
	gtkDrawDestroyCairoContext(cairo);

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
	gtk_widget_queue_draw(bd->widget);
	gtkDrawDestroyCairoContext(cairo);

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
		wPos_t x, wPos_t y,
		wDrawColor color,
		wDrawOpts opts )
{
	int i, j, wb;
	wPos_t xx, yy;
	wControl_p b;
	wWin_p win;
	GdkDrawable * gdk_drawable, * cairo_surface;
	GtkWidget * widget;

	x = INMAPX( bd, x-bm->x );
	y = INMAPY( bd, y-bm->y )-bm->h;
	wb = (bm->w+7)/8;

	cairo_t* cairo;

	if (opts&wDrawOptCursorRmv) color = wDrawColorWhite;   //Wipeout existing cursor draw (simplistic first)


	if ((opts&wDrawOptCursor) || (opts&wDrawOptCursorRmv) || (opts&wDrawOptCursorQuit)) {

		cairo = CreateCursorSurface((wControl_p)bd,&bd->cursor_surface, bd->w, bd->h, color, opts);

		if ((opts&wDrawOptCursorRmv) || (opts&wDrawOptCursorQuit)) {
			bd->cursor_surface.show = FALSE;
		} else bd->cursor_surface.show = TRUE;

		widget = bd->widget;


	} else {
		cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, color, opts);
		widget = bd->widget;
	}

	GtkWidget * new_widget = widget;
	GdkGC * gc = NULL;
	GdkWindow * gdk_window = NULL;

	win = bd->parent;

	for ( i=0; i<bm->w; i++ )
		for ( j=0; j<bm->h; j++ )
			if ( bm->bits[ j*wb+(i>>3) ] & (1<<(i&07)) ) {
				xx = x+i;
				yy = y+j;
				if ( 0 <= xx && xx < bd->w &&
					 0 <= yy && yy < bd->h ) {
					b = (wControl_p)bd;
				} else if ( (opts&wDrawOptNoClip) != 0 ) {
					xx += bd->realX;
					yy += bd->realY;
					b = wlibGetControlFromPos( bd->parent, xx, yy );
					if ( b) {
						xx -= b->realX;
						yy -= b->realY;
						new_widget = b->widget;
					} else {
						new_widget = bd->parent->widget;
					}
				} else {
					continue;
				}

				if (new_widget != widget) {
					if (cairo)
						cairo_destroy(cairo);
					cairo = NULL;
					if (widget && (widget != bd->parent->widget))
						gtk_widget_queue_draw(GTK_WIDGET(widget));
					if ( (opts&wDrawOptCursor) || (opts&wDrawOptCursorRmv) || (opts&wDrawOptCursorQuit)) {
						if (!b) b = (wControl_p)(bd->parent->widget);
						cairo = CreateCursorSurface(b,&b->cursor_surface, b->w, b->h, color, opts);
						widget = b->widget;
						gc = NULL;
						if ((opts&wDrawOptCursorRmv) || (opts&wDrawOptCursorQuit))
							b->cursor_surface.show = FALSE;
						else
							b->cursor_surface.show = TRUE;
					} else {
						continue;
					}
					widget = new_widget;
				}
				if ((opts&wDrawOptCursorQuit) || (opts&wDrawOptCursorQuit) ) continue;
				cairo_rectangle(cairo, xx, yy, 1, 1);
				cairo_fill(cairo);
			}

	cairo_destroy(cairo);

	if (widget)
		gtk_widget_queue_draw(GTK_WIDGET(widget));

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
		wPos_t w,
		wPos_t h , void * redraw)
{
	wBool_t repaint;
	if (bd == NULL) {
		fprintf(stderr,"resizeDraw: no client data\n");
		return;
	}

	/* Negative values crashes the program */
	if (w < 0 || h < 0)
		return;

	repaint = (w != bd->w || h != bd->h);
	bd->w = w;
	bd->h = h;
	gtk_widget_set_size_request( bd->widget, w, h );
	if (repaint)
	{
		if (bd->pixmap)
			gdk_pixmap_unref( bd->pixmap );
		bd->pixmap = gdk_pixmap_new( bd->widget->window, w, h, -1 );

		wDrawClear( bd );
		if (!redraw)
			bd->redraw( bd, bd->context, w, h );
	}
	/*wRedraw( bd )*/;
}


 void wDrawGetSize(
		wDraw_p bd,
		wPos_t *w,
		wPos_t *h )
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
		wPos_t x,
		wPos_t y,
		wPos_t w,
		wPos_t h )
{
	GdkRectangle rect;
	rect.width = w;
	rect.height = h;
	rect.x = INMAPX( d, x );
	rect.y = INMAPY( d, y ) - rect.height;
	gdk_gc_set_clip_rectangle( d->gc, &rect );

}


static gint draw_expose_event(
		GtkWidget *widget,
		GdkEventExpose *event,
		wDraw_p bd)
{

	cairo_t* cairo = gdk_cairo_create (widget->window);
	gdk_cairo_set_source_pixmap(cairo,bd->pixmap,event->area.x, event->area.y);
	cairo_rectangle(cairo,event->area.x, event->area.y,
					event->area.width, event->area.height);
	cairo_set_operator(cairo,CAIRO_OPERATOR_SOURCE);
	cairo_fill(cairo);

	if (bd->cursor_surface.surface && bd->cursor_surface.show) {
		cairo_set_source_surface(cairo,bd->cursor_surface.surface,event->area.x, event->area.y);
		cairo_set_operator(cairo,CAIRO_OPERATOR_OVER);
		cairo_rectangle(cairo,event->area.x, event->area.y,
				       event->area.width, event->area.height);
		cairo_fill(cairo);
	}
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

static const char * actionNames[] = { "None", "Move", "LDown", "LDrag", "LUp", "RDown", "RDrag", "RUp", "Text", "ExtKey", "WUp", "WDown" };

/**
 * Handler for scroll events, ie mouse wheel activity
 */

static gint draw_scroll_event(
		GtkWidget *widget,
		GdkEventScroll *event,
		wDraw_p bd)
{
	wAction_t action;

	switch( event->direction ) {
	case GDK_SCROLL_UP:
		action = wActionWheelUp;
		break;
	case GDK_SCROLL_DOWN:
		action = wActionWheelDown;
		break;
	default:
		action = 0;
		break;
	}

	if (action != 0) {
		if (drawVerbose >= 2)
			printf( "%s[%dx%d]\n", actionNames[action], bd->lastX, bd->lastY );
		bd->action( bd, bd->context, action, bd->lastX, bd->lastY );
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

	switch ( event->button ) {
	case 1: /* left mouse button */
		action = event->type==GDK_BUTTON_PRESS?wActionLDown:wActionLUp;
		if (event->type==GDK_2BUTTON_PRESS) action = wActionLDownDouble;
		/*bd->action( bd, bd->context, event->type==GDK_BUTTON_PRESS?wActionLDown:wActionLUp, bd->lastX, bd->lastY );*/
		break;
	case 3: /* right mouse button */
		action = event->type==GDK_BUTTON_PRESS?wActionRDown:wActionRUp;
		/*bd->action( bd, bd->context, event->type==GDK_BUTTON_PRESS?wActionRDown:wActionRUp, bd->lastX, bd->lastY );*/
		break;
	}
	if (action != 0) {
		if (drawVerbose >= 2)
			printf( "%s[%dx%d]\n", actionNames[action], bd->lastX, bd->lastY );
		bd->action( bd, bd->context, action, bd->lastX, bd->lastY );
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
	} else if (state & GDK_BUTTON3_MASK) {
		action = wActionRDrag;
	} else {
		action = wActionMove;
	}
	bd->lastX = OUTMAPX(bd, x);
	bd->lastY = OUTMAPY(bd, y);
	if (drawVerbose >= 2)
		printf( "%lx: %s[%dx%d] %s\n", (long)bd, actionNames[action], bd->lastX, bd->lastY, event->is_hint?"<Hint>":"<>" );
	bd->action( bd, bd->context, action, bd->lastX, bd->lastY );
	if (!(bd->option & BD_NOFOCUS))
		gtk_widget_grab_focus( bd->widget );
	return TRUE;
}


static gint draw_char_event(
		GtkWidget * widget,
		GdkEventKey *event,
		wDraw_p bd )
{
	GdkModifierType modifiers;
	guint key = event->keyval;
	wAccelKey_e extKey = wAccelKey_None;
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
	default: ;
	}

	if (extKey != wAccelKey_None) {
		if ( wlibFindAccelKey( event ) == NULL ) {
			bd->action( bd, bd->context, wActionExtKey + ((int)extKey<<8), bd->lastX, bd->lastY );
		}
		if (!(bd->option & BD_NOFOCUS))
				gtk_widget_grab_focus( bd->widget );
		return TRUE;
	} else if (key <= 0xFF && (event->state&(GDK_CONTROL_MASK|GDK_MOD1_MASK)) == 0 && bd->action) {
		bd->action( bd, bd->context, wActionText+(key<<8), bd->lastX, bd->lastY );
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
		wPos_t	x,
		wPos_t	y,
		const char 	* helpStr,
		long	option,
		wPos_t	width,
		wPos_t	height,
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
	gtk_signal_connect (GTK_OBJECT (bd->widget), "leave_notify_event",
						   (GtkSignalFunc) draw_leave_event, bd);
	gtk_widget_set_can_focus(bd->widget,!(option & BD_NOFOCUS));
	//if (!(option & BD_NOFOCUS))
	//	GTK_WIDGET_SET_FLAGS(GTK_WIDGET(bd->widget), GTK_CAN_FOCUS);
	gtk_widget_set_events (bd->widget, GDK_EXPOSURE_MASK
							  | GDK_LEAVE_NOTIFY_MASK
							  | GDK_BUTTON_PRESS_MASK
							  | GDK_BUTTON_RELEASE_MASK
/*							  | GDK_SCROLL_MASK */
							  | GDK_POINTER_MOTION_MASK
							  | GDK_POINTER_MOTION_HINT_MASK
							  | GDK_KEY_PRESS_MASK
							  | GDK_KEY_RELEASE_MASK );
	bd->lastColor = -1;
	bd->dpi = 75;
	bd->maxW = bd->w = width;
	bd->maxH = bd->h = height;

	gtk_fixed_put( GTK_FIXED(parent->widget), bd->widget, bd->realX, bd->realY );
	wlibControlGetSize( (wControl_p)bd );
	gtk_widget_realize( bd->widget );
	bd->pixmap = gdk_pixmap_new( bd->widget->window, width, height, -1 );
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

wDraw_p wBitMapCreate(          wPos_t w, wPos_t h, int arg )
{
	wDraw_p bd;

	bd = (wDraw_p)wlibAlloc( gtkMainW,  B_DRAW, 0, 0, NULL, sizeof *bd, NULL );

	bd->lastColor = -1;
	bd->dpi = 75;
	bd->maxW = bd->w = w;
	bd->maxH = bd->h = h;

	bd->pixmap = gdk_pixmap_new( gtkMainW->widget->window, w, h, -1 );
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

void wDrawShowBackground( wDraw_p bd, wPos_t pos_x, wPos_t pos_y, wPos_t size, wAngle_t angle, int screen) {

	if (bd->background) {
		cairo_t* cairo = gtkDrawCreateCairoContext(bd, NULL, 0, wDrawLineSolid, wDrawColorWhite, 0);
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
	}

}




