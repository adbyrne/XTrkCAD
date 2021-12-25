/**\file basicdraw.c
 * Cairo drawing routines for noninteractive usage like bitmap export or print
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2009 Daniel Spagnol, 2013, 2021 Martin Fischer
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

#include <stdlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <math.h>

#include "gtkint.h"

//static double scale_adjust = 1.0;
//static double scale_text = 1.0;

static int iBasicLog = 1;

/**
 * Set the drawing color. The color table entry is fetched and converted
 * to RGBA format, where alpha is considered to be maximum, no transparency
 *
 * \param cr
 * \param color
 */

static void
BasicDrawSetColor (cairo_t *cr, wDrawColor color)
{
	GdkRGBA gcolor;

	gcolor = wlibGetColor (color, TRUE);
	cairo_set_source_rgba (cr, gcolor.red, gcolor.green, gcolor.blue, 1.0);
}

/**
 * Sets the line type, including style and width, making sure that a minimum
 * line width is used
 *
 * \param cr
 * \param lineWidth
 * \param lineType
 * \param opts
 */

static void
BasicDrawSetLineType (cairo_t *cr, double lineWidth, double minLineWidth,
                      wDrawLineType_e lineType, wDrawOpts opts,
                      double scale_adjust)
{
	double dashes[] = { DASH_LENGTH, 3 };		//Reduce gap in between dashes
	static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);

	if (lineWidth < 0.0) {
		lineWidth = P2I(-lineWidth) * 2.0 / scale_adjust;
	}

	// make sure that there is a minimum line width used
	if (lineWidth <= minLineWidth) {
		lineWidth = minLineWidth;
	}

	cairo_set_line_width (cr, lineWidth);
	switch (lineType) {
	case wDrawLineDot: {
		double dashes[] = { 1, 2, 1, 2 };
		static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash (cr, dashes, len_dashes, 0.0);
		break;
	}
	case wDrawLineDash: {
		double dashes[] = { DASH_LENGTH, 3 };//Reduce gap in between dashes
		static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash (cr, dashes, len_dashes, 0.0);
		break;
	}
	case wDrawLineDashDot: {
		double dashes[] = { 3, 2, 1, 2 };
		static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash (cr, dashes, len_dashes, 0.0);
		break;
	}
	case wDrawLineDashDotDot: {
		double dashes[] = { 3, 2, 1, 2, 1, 2 };
		static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash (cr, dashes, len_dashes, 0.0);
		break;
	}
	case wDrawLineCenter: {
		double dashes[] = { 1.5 * DASH_LENGTH, 3, DASH_LENGTH, 3 };
		static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash (cr, dashes, len_dashes, 0.0);
		break;
	}
	case wDrawLinePhantom: {
		double dashes[] = { 1.5 * DASH_LENGTH, 3, DASH_LENGTH, 3,
		                    DASH_LENGTH, 3
		                  };
		static int len_dashes = sizeof(dashes) / sizeof(dashes[0]);
		cairo_set_dash (cr, dashes, len_dashes, 0.0);
		break;
	}
	default:
		cairo_set_dash (cr, NULL, 0, 0.0);
	}
}

/**
 * Clear a bitmap
 * \param bd drawing handle
 */

void
wlibBasicClear (wDraw_p bd)
{
	if (iBasicLog >= 1) {
		printf ("wlibBasicClear %ld+%ld\n", bd->w, bd->h);
	}

	cairo_move_to (bd->cr, 0, 0);
	cairo_rel_line_to (bd->cr, bd->w, 0);
	cairo_rel_line_to (bd->cr, 0, bd->h);
	cairo_rel_line_to (bd->cr, -bd->w, 0);
	cairo_set_source_rgba (bd->cr, 1.0, 1.0, 1.0, 1.0);
	cairo_fill (bd->cr);
}

/**
 *
 * \param bd
 * \param x0
 * \param y0
 * \param x1
 * \param y1
 * \param width
 * \param lineType
 * \param color
 * \param opts
 */

void
wlibBasicDrawLine (wDraw_p bd, wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t x1,
                   wDrawPix_t y1, double width, double minWidth,
                   wDrawLineType_e lineType, wDrawColor color, wDrawOpts opts)
{
	BasicDrawSetColor (bd->cr, color);
	BasicDrawSetLineType (bd->cr,
	                      width,
	                      minWidth,
	                      lineType,
	                      opts,
	                      bd->scale_adjust);

	cairo_move_to (bd->cr, x0, y0);
	cairo_line_to (bd->cr, x1, y1);
	cairo_stroke (bd->cr);
}

/**
 * Draw an arc around a specified center
 *
 * \param bd
 * \param x0, y0 IN  center of arc
 * \param r IN radius
 * \param angle0, angle1 IN start and end angle
 * \param drawCenter draw marking for center
 * \param width line width
 * \param lineType
 * \param color color
 * \param opts ?
 */

void
wlibBasicDrawArc (wDraw_p bd, wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t r,
                  double angle0, double angle1, wBool_t drawCenter,
                  double width, double minWidth, wDrawLineType_e lineType,
                  wDrawColor color, wDrawOpts opts)
{
	BasicDrawSetColor (bd->cr, color);
	BasicDrawSetLineType (bd->cr,
	                      width,
	                      minWidth,
	                      lineType,
	                      opts,
	                      bd->scale_adjust);

	if (angle1 >= 360.0) {
		angle1 = 359.999;
	}

	angle1 = 90.0 - (angle0 + angle1);

	while (angle1 < 0.0) {
		angle1 += 360.0;
	}

	while (angle1 >= 360.0) {
		angle1 -= 360.0;
	}

	angle0 = 90.0 - angle0;

	while (angle0 < 0.0) {
		angle0 += 360.0;
	}

	while (angle0 >= 360.0) {
		angle0 -= 360.0;
	}

	// draw the curve
	cairo_arc (bd->cr, x0, y0, r, angle1 * M_PI / 180.0, angle0 * M_PI / 180.0);

	if (drawCenter) {
		// draw crosshair for center of curve
		cairo_move_to (bd->cr, x0 - CENTERMARK_LENGTH / 2, y0);
		cairo_line_to (bd->cr, x0 + CENTERMARK_LENGTH / 2, y0);
		cairo_move_to (bd->cr, x0, y0 - CENTERMARK_LENGTH / 2);
		cairo_line_to (bd->cr, x0, y0 + CENTERMARK_LENGTH / 2);
	}

	cairo_stroke (bd->cr);
}

/**
 * Print a string at the given position using specified font and text size.
 * The orientation of the y-axis in XTrackCAD is wrong for cairo. So for
 * all other print primitives a flip operation is done. As this would
 * also affect the string orientation, printing a string has to be
 * treated differently. The starting point is transformed, then the
 * string is rotated and scaled as needed. Finally the string position
 * translated to the starting point calculated previously. The same
 * solution would have to be applied to a bitmap should printing
 * bitmaps ever be implemented.
 *
 * \param x IN x position in pixels
 * \param y IN y position in pixels
 * \param a IN angle of baseline in degrees. Positive is clockwise, 0 is direction of positive x axis
 * \param s IN string to print
 * \param fp IN font
 * \param fs IN font size
 * \param color IN text color
 * \param opts IN ???
 * \return
 */

void
wlibBasicDrawString (wDraw_p bd, wDrawPix_t x, wDrawPix_t y, double a, char *s,
                     wFont_p fp, double fs, double width, double minWidth,
                     wDrawColor color, wDrawOpts opts)
{
	char *cp;
	double x0 = (double) x, y0 = (double) y;
	int text_height, text_width;
	double ascent;

	cairo_t *cr;
	cairo_matrix_t matrix;

	PangoLayout *layout;
	PangoFontDescription *desc;
	PangoFontMetrics *metrics;
	PangoContext *pcontext;

	cr = bd->cr;

	// get the current transformation matrix and transform the starting
	// point of the string

	cairo_save (cr);

	cairo_get_matrix (cr, &matrix);

	cairo_matrix_transform_point (&matrix, &x0, &y0);

	cairo_identity_matrix (cr);

	layout = pango_cairo_create_layout (cr);

	// set the correct font and size
	// \todo use a getter function instead of double conversion
	desc = pango_font_description_from_string (wlibFontTranslate (fp));

	pango_font_description_set_size (desc, fs * PANGO_SCALE * bd->scale_text);

	// render the string to a Pango layout
	pango_layout_set_font_description (layout, desc);

	gchar *utf8 = wlibConvertInput (s);

	pango_layout_set_text (layout, utf8, -1);
	pango_layout_set_width (layout, -1);
	pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	pango_layout_get_size (layout, &text_width, &text_height);

	text_width = text_width / PANGO_SCALE;
	text_height = text_height / PANGO_SCALE;

	// get the height of the string
	pcontext = pango_cairo_create_context (cr);
	metrics = pango_context_get_metrics (pcontext,
	                                     desc,
	                                     pango_context_get_language (pcontext));

	ascent = pango_font_metrics_get_ascent (metrics) / PANGO_SCALE;

	int baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

	cairo_translate (cr, x0, y0);
	cairo_rotate (cr, -a * M_PI / 180.0);
	cairo_translate (cr, 0, -baseline);

	cairo_move_to (cr, 0, 0);

	pango_cairo_update_layout (cr, layout);

	// set the color
	BasicDrawSetColor (cr, color);

	// and show the string
	if (!(opts & wDrawOutlineFont)) {
		pango_cairo_show_layout (cr, layout);
		cairo_stroke (cr);
	} else {
		PangoLayoutLine * line;
		line = pango_layout_get_line_readonly (layout, 0);
		BasicDrawSetLineType (cr,
		                      width,
		                      minWidth,
		                      wDrawLineSolid,
		                      0,
		                      bd->scale_adjust);
		pango_cairo_layout_line_path (cr, line);
		cairo_stroke (cr);
	}
	// free unused objects
	g_object_unref (layout);
	g_object_unref (pcontext);

	cairo_restore (cr);

}

/**
 * Print a filled rectangle
 *
 * \param bd
 * \param x0, y0 IN top left corner
 * \param x1, y1 IN bottom right corner
 * \param color IN fill color
 * \param opts IN options
 * \return
 */

void
wlibBasicDrawFillRectangle (wDraw_p bd, wDrawPix_t x0, wDrawPix_t y0,
                            wDrawPix_t x1, wDrawPix_t y1, wDrawColor color,
                            wDrawOpts opts)
{
	cairo_t *cr = bd->cr;
	double width = x0 - x1;
	double height = y0 - y1;

	BasicDrawSetColor (cr, color);

	cairo_rectangle (cr, x0, y0, width, height);

	cairo_fill (cr);
}

/**
 * Draw a filled polygon
 *
 * \param bd
 * \param p IN a list of x and y coordinates
 * \param cnt IN the number of points
 * \param color IN fill color
 * \param opts IN options
 * \paran fill IN Fill or not
 * \return
 */

void
wlibBasicDrawFillPolygon (wDraw_p bd, wDrawPix_t p[][2], wPolyLine_e type[],
                          int cnt, wDrawColor color, wDrawOpts opts, int fill,
                          int open)
{
	int inx;
	cairo_t *cr = bd->cr;

	BasicDrawSetColor (cr, color);

	wDrawPix_t mid0[2], mid1[2], mid2[2], mid3[2], mid4[2];

	for (inx = 0; inx < cnt; inx++) {
		int j = inx - 1;
		int k = inx + 1;
		if (j < 0) {
			j = cnt - 1;
		}
		if (k > cnt - 1) {
			k = 0;
		}
		double len0, len1;
		double d0x = (p[inx][0] - p[j][0]);
		double d0y = (p[inx][1] - p[j][1]);
		double d1x = (p[k][0] - p[inx][0]);
		double d1y = (p[k][1] - p[inx][1]);
		len0 = (d0x * d0x + d0y * d0y);
		len1 = (d1x * d1x + d1y * d1y);
		mid0[0] = (d0x / 2) + p[j][0];
		mid0[1] = (d0y / 2) + p[j][1];
		mid1[0] = (d1x / 2) + p[inx][0];
		mid1[1] = (d1y / 2) + p[inx][1];
		if (type && (type[inx] == wPolyLineRound) && (len1 > 0) && (len0 > 0)) {
			double ratio = sqrt (len0 / len1);
			if (len0 < len1) {
				mid1[0] = ((d1x * ratio) / 2) + p[inx][0];
				mid1[1] = ((d1y * ratio) / 2) + p[inx][1];
			} else {
				mid0[0] = p[inx][0] - (d0x / (2 * ratio));
				mid0[1] = p[inx][1] - (d0y / (2 * ratio));
			}
		}
		mid3[0] = (p[inx][0] - mid0[0]) / 2 + mid0[0];
		mid3[1] = (p[inx][1] - mid0[1]) / 2 + mid0[1];
		mid4[0] = (mid1[0] - p[inx][0]) / 2 + p[inx][0];
		mid4[1] = (mid1[1] - p[inx][1]) / 2 + p[inx][1];
		wDrawPix_t save[2];
		if (inx == 0) {
			if (!type || (type && type[0] == wPolyLineStraight) || open) {
				cairo_move_to (cr, p[0][0], p[0][1]);
				save[0] = p[0][0];
				save[1] = p[0][1];
			} else {
				cairo_move_to (cr, mid0[0], mid0[1]);
				if (type[inx] == wPolyLineSmooth)
					cairo_curve_to (cr,
					                p[inx][0],
					                p[inx][1],
					                p[inx][0],
					                p[inx][1],
					                mid1[0],
					                mid1[1]);
				else
					cairo_curve_to (cr,
					                mid3[0],
					                mid3[1],
					                mid4[0],
					                mid4[1],
					                mid1[0],
					                mid1[1]);
				save[0] = mid0[0];
				save[1] = mid0[1];
			}
		} else if (!type || (type && type[inx] == wPolyLineStraight)
		           || (open && (inx == cnt - 1))) {
			cairo_line_to (cr, p[inx][0], p[inx][1]);
		} else {
			cairo_line_to (cr, mid0[0], mid0[1]);
			if (type && type[inx] == wPolyLineSmooth)
				cairo_curve_to (cr,
				                p[inx][0],
				                p[inx][1],
				                p[inx][0],
				                p[inx][1],
				                mid1[0],
				                mid1[1]);
			else
				cairo_curve_to (cr,
				                mid3[0],
				                mid3[1],
				                mid4[0],
				                mid4[1],
				                mid1[0],
				                mid1[1]);
		}
		if ((inx == cnt - 1) && !open) {
			cairo_line_to (cr, save[0], save[1]);
		}
	}

	if (fill && !open) {
		cairo_fill (cr);
	} else {
		cairo_stroke (cr);
	}
}
/**
 * Draw a filled circle
 *
 * \param bd
 * \param x0, y0  IN coordinates of center (in pixels )
 * \param r IN radius
 * \param color IN fill color
 * \param opts IN options
 * \return
 */

void
wlibBasicDrawFillCircle (wDraw_p bd, wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t r,
                         wDrawColor color, wDrawOpts opts)
{
	BasicDrawSetColor (bd->cr, color);

	cairo_arc (bd->cr, x0, y0, r, 0.0, 2 * M_PI);

	cairo_fill (bd->cr);
}

