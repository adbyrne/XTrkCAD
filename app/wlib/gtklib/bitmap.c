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

struct wBitmap_t {
	WOBJ_COMMON
};

/**
 * Create a static control for displaying a bitmap.
 *
 * \param parent IN parent window
 * \param x, y   IN position in parent window
 * \param option IN ignored for now
 * \param iconP  IN icon to use in XPM format
 * \return    the control
 */

wControl_p
wBitmapCreate( wWin_p parent, wPos_t x, wPos_t y, long options, wIcon_p iconP )
{
	wBitmap_p bt;
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	
	bt = wlibAlloc( parent, B_BITMAP, x, y, NULL, sizeof *bt, NULL );
	bt->w = iconP->w;
	bt->h = iconP->h;
	bt->option = options;
	
	/*
	 * Depending on the platform, parent->widget->window might still be null 
	 * at this point. The window allocation should be forced before creating
	 * the pixmap.
	 */
	if ( gtk_widget_get_window( parent->widget ) == NULL )
		gtk_widget_realize( parent->widget ); /* force allocation, if pending */
	
	/* create the bitmap from supplied xpm data */
	pixbuf = gdk_pixbuf_new_from_xpm_data( (const char **)iconP->bits );
	image = gtk_image_new_from_pixbuf( pixbuf );
	gtk_widget_show( image );
	g_object_unref( (gpointer)pixbuf );
	
	bt->widget = gtk_fixed_new();
	gtk_widget_show( bt->widget );
	gtk_container_add( GTK_CONTAINER(bt->widget), image );
	
	wlibComputePos( (wControl_p)bt );
	wlibControlGetSize( (wControl_p)bt );
	gtk_fixed_put( GTK_FIXED( parent->widget ), bt->widget, bt->realX, bt->realY );
	
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

wIcon_p wIconCreateBitMap( wPos_t w, wPos_t h, const char * bits, wDrawColor color )
{
	wIcon_p ip;
	ip = (wIcon_p)malloc( sizeof *ip );
	ip->gtkIconType = gtkIcon_bitmap;
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
	ip->gtkIconType = gtkIcon_pixmap;
	ip->w = 0;
	ip->h = 0;
	ip->color = 0;
	ip->bits = pm;
	return ip;
}

/**
 * Set the color a two-tone icon
 * 
 * \param ip IN icon handle
 * \param color IN color to use
 */

void wIconSetColor( wIcon_p ip, wDrawColor color )
{
	ip->color = color;
}

