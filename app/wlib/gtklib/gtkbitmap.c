/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkbitmap.c,v 1.5 2009-09-27 04:28:03 dspagnol Exp $
 */
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2009 Daniel Spagnol
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
#include <stdlib.h>

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
 * \param iconP  IN icon to use
 * \return    the control
 */

wControl_p
wBitmapCreate( wWin_p parent, wPos_t x, wPos_t y, long options, wIcon_p iconP )
{
	wBitmap_p bt;
	GdkPixbuf *pixbuf;
	
	bt = gtkAlloc( parent, B_BITMAP, x, y, NULL, sizeof *bt, NULL );
	bt->w = iconP->w;
	bt->h = iconP->h;
	bt->option = options;
	
	/*
	 * Depending on the platform, parent->widget->window might still be null 
	 * at this point. The window allocation should be forced before creating
	 * the pixmap.
	 */
	if ( parent->widget->window == NULL )
		gtk_widget_realize( parent->widget ); /* force allocation, if pending */

	pixbuf = gdk_pixbuf_new_from_xpm_data( (const char**)iconP->bits );
	GtkWidget *image = gtk_image_new_from_pixbuf( pixbuf );
	
   gtk_widget_show( image );
	
	bt->widget = gtk_fixed_new();
	gtk_container_add( GTK_CONTAINER(bt->widget), image );	
	gtk_widget_show( bt->widget );
		
	gtkComputePos( (wControl_p)bt );
	gtkControlGetSize( (wControl_p)bt );
	gtk_fixed_put( GTK_FIXED( parent->widget ), bt->widget, bt->realX, bt->realY );
	
	g_object_unref( pixbuf );
	
	return( (wControl_p)bt );
}

