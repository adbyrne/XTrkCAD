/** \file builder.c
 * Gtk.Builder functions
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"

/*
 * Copyright (C) 2018 Martin Fischer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/**
 * Create UI path and filename from dialog name. Returned filename has to be
 * g_string_freed by caller.
 * 
 * \param dialog IN name of dialog
 * \return filename 
 */


GString *
wlibFileNameFromDialog( const char *dialog )
{

#ifdef NDEBUG
    GString *filename = g_string_new(wGetAppLibDir());
#else
    char * cwd = malloc(PATH_MAX);
    getcwd(cwd, PATH_MAX );
    GString *filename = g_string_new( cwd);
#endif  //NDEBUG    
    g_string_append(filename, "/ui/");
    g_string_append(filename, dialog );
    g_string_append(filename, ".glade");

#ifndef NDEBUG
    free(cwd);
#endif    
    
    return( filename );
}


wWin_p
wlibDialogFromTemplate( int winType, const char *nameStr, long option, void *data )
{
    wWin_p w;
    int h;
    GString *filename;
    w = wlibAlloc(NULL, winType, 0, 0, NULL, sizeof *w, data);
    w->busy = TRUE;
    w->option = option;
	w->resizeTimer = 0;

    filename = wlibFileNameFromDialog( nameStr );
    
    w->builder = gtk_builder_new_from_file(filename->str);
    if( !w->builder ) {
        GString *errorMessage = g_string_new("Could not load ");
        g_string_append( errorMessage, filename->str);
        wNoticeEx( NT_ERROR, 
                   errorMessage->str,
                  "OK",
                  NULL );
        exit(1);
    }
    w->gtkwin = (GtkWidget *)gtk_builder_get_object(w->builder,
                                       nameStr);
    if (!w->gtkwin) {
    	GString *errorMessage = g_string_new("Could not find window object ");
    	        g_string_append( errorMessage, nameStr);
    	        wNoticeEx( NT_ERROR,
    	                   errorMessage->str,
    	                  "OK",
    	                  NULL );
    	        exit(1);
    }
    w->widget = w->gtkwin;      /**<TODO: w->widget was used for the fixed grid, not needed anymore */
    g_string_free(filename, TRUE);
   
    return w;
}    

GtkWidget *
wlibWidgetFromId( wWin_p win, char *id )
{
    return( (GtkWidget *)gtk_builder_get_object(win->builder, id));
}    
