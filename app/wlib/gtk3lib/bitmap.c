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
wBitmapCreate( wControl_p parent, wWinPix_t x, wWinPix_t y, long options, 
	const wIcon_p iconP )
{
	wControl_p bt;
	struct bitmap* bm;
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	
	bt = wlibControlNew(B_BITMAP, parent, NULL, NULL);
	bm = WLIB_GET_DATA_PTR(bt, bitmap);

	/*
	 * Depending on the platform, parent->widget->window might still be null 
	 * at this point. The window allocation should be forced before creating
	 * the pixmap.
	 * 
	 * \todo Is this assumption really true? Temporarily assume no
	 */
	//if ( gtk_widget_get_window( parent->gtkWindow ) == NULL )
	//	gtk_widget_realize( parent->gtkWindow ); /* force allocation, if pending */
	
	/* create the bitmap from supplied xpm data */
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

