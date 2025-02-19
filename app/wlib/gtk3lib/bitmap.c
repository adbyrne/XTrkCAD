/** \file bitmap.c
 * Bitmap creation
 */
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2009 Daniel Spagnol, 2013 Martin Fischer
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
#include "gtkint.h"

#include "xtrkcad-config.h"

#define PNGFORMAT "png"
#define JPEGFORMAT "jpeg"

 /**
  * Get the Extension part of a filename
  *
  * /param fname the filename
  *
  * /return char* point to the extension
  */

static const char*
GetExtension(const char* fname)
{
	const char* end = fname + strlen(fname);

	while (end > fname && *end != '.') {
		--end;
	}
	return(end + 1);
}


/**
 * Create a static control for displaying a bitmap.
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: yes
 * - builder: no
 *
 * ### Options
 * 
 * \param parent IN parent window
 * \param x, y   IN position in parent window
 * \param option IN ignored for now
 * \param iconP  IN icon to use in XPM format
 * \return    the control
 */

wControl_p
wBitmapViewCreate( wControl_p parent, wWinPix_t x, wWinPix_t y, long options, 
	const wIcon_p iconP )
{
	wControl_p bt;
	struct bitmap* bm;
	GdkPixbuf *pixbuf = NULL;

	g_assert(iconP->gtkIconType == ICON_PIXBUF_FROM_RESOURCE ||
			iconP->gtkIconType == ICON_PIXBUF_FROM_TEXT);
	
	bt = wlibControlNew(B_BITMAP, parent, NULL, NULL);
	bm = CONTROL_GET_ATTRIBUTES_PTR(bt, bitmap);
	
	/* create the bitmap from supplied image attributes */
		pixbuf = iconP->bits;
		bt->widget = gtk_image_new_from_pixbuf(pixbuf);
		gtk_widget_show(bt->widget);

		wlibBasicGridAttach(parent, bt->widget, x, y, 1, 1);

	return( (wControl_p)bt );
}

wIcon_p wIconCreatePixBufFromResource(const char *prefix, const char *filename)
{
	wIcon_p ip;
	GError* error = NULL;

	ip = (wIcon_p)g_malloc0(sizeof * ip);
	if (ip) {
		gchar* path;
		path = g_strconcat(prefix,
			filename,
			NULL);

		ip->gtkIconType = ICON_PIXBUF_FROM_RESOURCE;
		ip->bits = gdk_pixbuf_new_from_resource(path, &error);
		ip->filename = g_strdup(path);

		if (error) {
			fprintf(stderr, "Error reading icon: %s\n", error->message);
			g_error_free(error);
		}

		g_free(path);
	} 
	return ip;
}

/**
 * Set the color a two-tone icon
(??)
 * \param ip IN icon handle
 * \param color IN color to use
 */

void wIconSetColor( wIcon_p ip, wDrawColor color )
{
	wlibFTLabelChangeColor(ip, color);
}

static const char *
CheckFileFormat(const char* fileName)
{
	const char* fileFormat = GetExtension(fileName);
	const char* writeFormat = NULL;

	if (!strcasecmp(fileFormat, PNGFORMAT)) {
		writeFormat = PNGFORMAT;
	}
	if (!strcasecmp(fileFormat, "jpg") ||
		!strcasecmp(fileFormat, "jpeg")) {
		writeFormat = JPEGFORMAT;
	}

	if (!writeFormat) {
		wNoticeWithIcon(NT_ERROR, "WriteBitMap: invalid file format!", "Ok", NULL);
	}

	return(writeFormat);
}
/**
* Export as bitmap file.
*
* \param d IN the drawing area ?
* \param fileName IN  fully qualified filename for the bitmap file.
* \return    TRUE on success, FALSE on error
*/

wBool_t wBitmapWriteFile(wControl_p drawingControl, const char* fileName)
{
	GdkPixbuf* pixbuf;
	GError* error = NULL;
	gboolean res;
	struct draw *drawArea = CONTROL_GET_ATTRIBUTES_PTR(drawingControl, draw);
	const char* writeFormat;

	writeFormat = CheckFileFormat(fileName);

	pixbuf = gdk_pixbuf_get_from_surface(drawArea->surface, 0, 0, drawArea->width, drawArea->height);

	if (!pixbuf) {
		wNoticeWithIcon(NT_ERROR, "WriteBitMap: pixbuf_get failed", "Ok", NULL);
		return FALSE;
	}

	res = gdk_pixbuf_save(pixbuf, fileName, writeFormat, &error, NULL);

	if (res == FALSE) {
		wNoticeWithIcon(NT_ERROR, "WriteBitMap: pixbuf_save failed", "Ok", NULL);
		return FALSE;
	}

	g_object_unref(pixbuf);
	return TRUE;
}

wControl_p 
wBitmapCreate(wWinPix_t w, wWinPix_t h, int arg)
{
	
	wControl_p bd;
	struct draw* draw;

	bd = (wControl_p)wlibControlNew(B_DRAW, NULL, NULL, NULL); 
	draw = CONTROL_GET_ATTRIBUTES_PTR(bd, draw);
	
	//bd->lastColor = -1;

	double dpi;

	wPrefGetFloat(PREFSECTION, DPISET, &dpi, 96.0);

	draw->dpi = dpi;
	//draw->maxW = 
	draw->width = w;
	//draw->maxH = 
	draw->height = h;
	//draw->clip_set = FALSE;
	draw->scale_adjust = 1.0;
	draw->scale_text = 1.0;

	if (arg & EXPORTBITMAP) {
		draw->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
		if (draw->surface == NULL) {
			wNoticeWithIcon(NT_ERROR, "image_ surface_create failed", "Ok", NULL);
			return NULL;
		}
		draw->cr = cairo_create(draw->surface);
		if (draw->cr == NULL) {
			wNoticeWithIcon(NT_ERROR, "image_ surface_create failed", "Ok", NULL);
			return NULL;
		}
		draw->drawDestination = EXPORTBITMAP;

		// correct origin of coordinates top left -> bottom left
		cairo_translate(draw->cr, 0, h);
		cairo_scale(draw->cr, 1.0, -1.0);

		wlibBasicClear(draw);
	}
	else {
		//bd->pixbuf = gdk_pixbuf_get_from_window(gtk_widget_get_window(GTK_WIDGET(
		//	gtkMainW->gtkwin)), 0, 0, w, h);
		//if (bd->pixbuf == NULL) {
		//	wNoticeWithIcon(NT_ERROR, "CreateBitMap: pixmap_new failed", "Ok", NULL);
		//	return FALSE;
		//}
		//bd->drawDestination = 0;
		//wDrawClear(bd);

	}
	return bd;
	
}


wBool_t wBitmapDelete(wControl_p d)
{
	g_assert(d->type == B_DRAW);

	struct draw* drawArea = CONTROL_GET_ATTRIBUTES_PTR(d, draw);

	if (drawArea->drawDestination == EXPORTBITMAP) {
		cairo_destroy(drawArea->cr);
		drawArea->cr = NULL;
		cairo_surface_destroy(drawArea->surface);
		drawArea->surface = NULL;
	}
	else {
		g_object_unref(drawArea->pixbuf);
		drawArea->pixbuf = NULL;
	}

//	drawArea->clip_set = FALSE;
	return TRUE;
}

