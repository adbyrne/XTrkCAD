/** \file gtksplash.c
 * Handling of the Splash Window functions
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtksplash.c,v 1.5 2009-05-31 14:48:58 m_fischer Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2007 Martin Fischer
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "gtkint.h"

#define LOGOFILENAME "logo.bmp"

static GtkWidget *window;	/** splash window handle */
static GtkWidget *message;	/** window handle for progress message */

/**
 * Create the splash window shown during startup. The function loads the logo 
 * bitmap and displays the program name and version as passed. 
 *
 * \param IN  appName the product name to be shown
 * \param IN  appVer  the product version to be shown
 * \return    TRUE if window was created, FALSE if an error occured
 */

int
wCreateSplash( char *appName, char *appVer )
{
   GtkWidget *vbox;
   GtkWidget *image;
   GtkWidget *label;
	char *temp;
	char logoPath[BUFSIZ];

	/* create the basic window */
   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated( GTK_WINDOW (window), FALSE );
   gtk_window_set_title (GTK_WINDOW (window), appName); 
   gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
   gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
   gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
#if GTK_MINOR_VERSION > 5
   gtk_window_set_focus_on_map (GTK_WINDOW (window), FALSE);
#endif

   vbox = gtk_vbox_new (FALSE, 0);
   gtk_widget_show (vbox);
   gtk_container_add (GTK_CONTAINER (window), vbox);

   /* add the logo image to the top of the splash window */
   sprintf( logoPath, "%s/" LOGOFILENAME, wGetAppLibDir());
   image = gtk_image_new_from_file ( logoPath );
   gtk_widget_show (image);
   gtk_box_pack_start (GTK_BOX (vbox), image, TRUE, TRUE, 0);
   gtk_misc_set_alignment (GTK_MISC (image), 0, 0);

   /* put the product name into the window */
	
	temp = malloc( strlen( appName ) + strlen( appVer ) + 2 );
	if( !temp )
		return( FALSE );
		
	sprintf( temp, "%s %s", appName, appVer );	
	
   label = gtk_label_new ( temp );
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
   gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_FILL);
   gtk_label_set_selectable (GTK_LABEL (label), FALSE);
   gtk_misc_set_padding (GTK_MISC (label), 6, 2);

	free( temp );

   label = gtk_label_new ("Application is starting...");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
   gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
   gtk_misc_set_padding (GTK_MISC (label), 6, 2);
#if GTK_MINOR_VERSION > 5   
   gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
#endif   
   message = label;	

	gtk_widget_show( window );
	
   return( TRUE );
}

/**
 * Update the progress message inside the splash window
 * msg	text message to display
 * return nonzero if ok
 */ 

int 
wSetSplashInfo( char *msg )
{
	if( msg ) {
		gtk_label_set_text( (GtkLabel *)message, msg );
		wFlush();
		return TRUE;
	}
	return FALSE;
}

/**
 * Destroy the splash window.
 *
 */

void 
wDestroySplash( void )
{
	/* kill window */
	gtk_widget_destroy( window );
	return;
}
