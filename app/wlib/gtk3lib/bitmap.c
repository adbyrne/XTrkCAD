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
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	
	bt = wlibControlNew(B_BITMAP, parent, NULL, NULL);
	bm = CONTROL_GET_ATTRIBUTES_PTR(bt, bitmap);

	/*
	 * Depending on the platform, parent->widget->window might still be null 
	 * at this point. The window allocation should be forced before creating
	 * the pixmap.
	 * 
	 * \todo Is this assumption really true? Temporarily assume no
	 */
	//if ( gtk_widget_get_window( parent->gtkWindow ) == NULL )
	//	gtk_widget_realize( parent->gtkWindow ); /* force allocation, if pending */
	
	/* create the bitmap from supplied xpm attributes */
	pixbuf = gdk_pixbuf_new_from_xpm_data( (const char **)iconP->bits );
	g_object_ref_sink(pixbuf);
	image = gtk_image_new_from_pixbuf( pixbuf );
	gtk_widget_show( image );
	g_object_unref( (gpointer)pixbuf );
		
	wlibBasicGridAttach(parent, bt->widget, x, y, 1, 1);
	
	return( (wControl_p)bt );
}

/**
 * Create a two-tone icon
 * 
 * \param w IN width of icon
 * \param h IN height of icon
 * \param bits IN bitmap
 * \param color IN color 
 * \returns icon handle
 */

wIcon_p wIconCreateBitMap( wWinPix_t w, wWinPix_t h, const char * bits, wDrawColor color )
{
	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	ip->gtkIconType = ICON_BITMAP;
	ip->w = w;
	ip->h = h;
	ip->color = color;
	ip->bits = bits;
	return ip;
}

/**
 * Create an icon from a pixmap
 * \param pm IN pixmap
 * \returns icon handle
 */

wIcon_p wIconCreatePixMap( char *pm[] )
{
	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	ip->gtkIconType = ICON_PIXMAP;
	ip->w = 0;
	ip->h = 0;
	ip->color = 0;
	ip->bits = pm;
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
	ip->color = color;
}

/**
* Export as bitmap file.
*
* \param d IN the drawing area ?
* \param fileName IN  fully qualified filename for the bitmap file.
* \return    TRUE on success, FALSE on error
*/

wBool_t wBitmapWriteFile(wDraw_p drawingArea, const char* fileName)
{
	GdkPixbuf* pixbuf;
	GError* error;
	gboolean res;
	const char* fileFormat = GetExtension(fileName);
	char* writeFormat = NULL;

	if (!strcasecmp(fileFormat, PNGFORMAT)) {
		writeFormat = PNGFORMAT;
	}
	if (!strcasecmp(fileFormat, "jpg") ||
		!strcasecmp(fileFormat, "jpeg")) {
		writeFormat = JPEGFORMAT;
	}

	if (!writeFormat) {
		wNoticeWithIcon(NT_ERROR, "WriteBitMap: invalid file format!", "Ok", NULL);
		return FALSE;
	}

	//pixbuf = gdk_pixbuf_get_from_drawable(NULL, (GdkWindow*)d->pixmap, NULL, 0, 0,
	//                                      0, 0, drawingArea->w, drawingArea->h);

	if (!pixbuf) {
		wNoticeWithIcon(NT_ERROR, "WriteBitMap: pixbuf_get failed", "Ok", NULL);
		return FALSE;
	}

	error = NULL;
	res = gdk_pixbuf_save(pixbuf, fileName, writeFormat, &error, NULL);

	if (res == FALSE) {
		wNoticeWithIcon(NT_ERROR, "WriteBitMap: pixbuf_save failed", "Ok", NULL);
		return FALSE;
	}

	g_object_ref_sink(pixbuf);
	g_object_unref(pixbuf);
	return TRUE;
}

wDraw_p wBitmapCreate(wWinPix_t w, wWinPix_t h, int arg)
{
	/*
	wDraw_p bd;

	bd = (wDraw_p)wlibAlloc(gtkMainW, B_DRAW, 0, 0, NULL, sizeof * bd, NULL);

	bd->lastColor = -1;

	double dpi;

	wPrefGetFloat(PREFSECTION, DPISET, &dpi, 96.0);

	bd->dpi = dpi;
	bd->maxW = bd->w = w;
	bd->maxH = bd->h = h;
	bd->clip_set = FALSE;
	bd->scale_adjust = 1.0;
	bd->scale_text = 1.0;

	if (arg & EXPORTBITMAP) {
		bd->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
		if (bd->surface == NULL) {
			wNoticeWithIcon(NT_ERROR, "image_ surface_create failed", "Ok", NULL);
			return NULL;
		}
		bd->cr = cairo_create(bd->surface);
		if (bd->cr == NULL) {
			wNoticeWithIcon(NT_ERROR, "image_ surface_create failed", "Ok", NULL);
			return NULL;
		}
		bd->drawDestination = EXPORTBITMAP;

		// correct origin of coordinates top left -> bottom left
		cairo_translate(bd->cr, 0, h);
		cairo_scale(bd->cr, 1.0, -1.0);

		wlibBasicClear(bd);
	}
	else {
		bd->pixbuf = gdk_pixbuf_get_from_window(gtk_widget_get_window(GTK_WIDGET(
			gtkMainW->gtkwin)), 0, 0, w, h);
		if (bd->pixbuf == NULL) {
			wNoticeWithIcon(NT_ERROR, "CreateBitMap: pixmap_new failed", "Ok", NULL);
			return FALSE;
		}
		bd->drawDestination = 0;
		wDrawClear(bd);

	}
	return bd;
	*/
	printf("Function wBitmapCreate is not implemented: %s %d\n", __FILE__, __LINE__);
	return(NULL);
}


wBool_t wBitmapDelete(wDraw_p d)
{
	if (d->drawDestination == EXPORTBITMAP) {
		cairo_destroy(d->cr);
		d->cr = NULL;
		cairo_surface_destroy(d->surface);
		d->surface = NULL;
	}
	else {
		g_object_unref(d->pixbuf);
		d->pixbuf = NULL;
	}
	d->clip_set = FALSE;
	return TRUE;
}

