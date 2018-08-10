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

/*
 * Load the GTK builder object for this window. The filename is assumed to be the same as the nameStr and the window object id.
 *
 * \param IN winType passed through to wlibAlloc
 * \param IN labelStr the name that will be shown in the title of the window
 * \param IN nameStr the name to look up
 * \param IN option the creation options
 * \param INOUT data passed through to wlibAlloc
 * \return the window object pointer
 *
 * In the window object the builder field will be set with the loaded object and the gtkwin field populated
 */

wWin_p
wlibDialogFromTemplate( int winType, const char *labelStr, const char *nameStr, long option, void *data )
{
    wWin_p w;
    int h;
    GString *filename;
    w = wlibAlloc(NULL, winType, 0, 0, labelStr, sizeof *w, data);
    w->busy = TRUE;
    w->option = option;
	w->resizeTimer = 0;
    

    filename = wlibFileNameFromDialog( nameStr );
    
    w->template_id = strdup(nameStr);

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
    //w->widget = w->gtkwin;      /**<TODO: w->widget was used for the fixed grid, not needed anymore */
    g_string_free(filename, TRUE);
   
    return w;
}    
/**
 * GetWidgetFromName
 * \param IN win  			Window
 * \param IN dialogname  	The first part of name
 * \param IN suffix			The last part of the name
 * \param IN ignore_failure	If object can't be found, shall we continue?
 */
GtkWidget *
wlibGetWidgetFromName( wWin_p parent, const char *dialogname, const char *suffix, wBool_t ignore_failure )
{
    GString *id = g_string_new(dialogname);
    GtkWidget *widget;
    
    g_string_append_printf(id, ".%s", suffix );
    
   	widget = wlibWidgetFromId( parent, id->str );

   	if(!widget) {
		if (!ignore_failure) {
			GString *errorMessage = g_string_new("Could not find widget ");
			g_string_append( errorMessage, id->str);
			wNoticeEx( NT_ERROR,
				   errorMessage->str,
				   "OK",
				   NULL );
			g_string_free(errorMessage, TRUE);
			exit(1);
		} else {
			return NULL;
		}
	}
    
    g_string_free(id, TRUE);
    
    return( widget );
}

GtkWidget *
wlibWidgetFromIdWarn( wWin_p win, const char *id)
{
	GtkWidget * wi = wlibWidgetFromId(win,id);
	if (wi) return wi;
	GString *errorMessage = g_string_new("Could not load sub-widget ");
	g_string_append_printf(errorMessage, "%s", id);
	wNoticeEx( NT_ERROR,
		   errorMessage->str,
		   "OK",
		   NULL );
	g_string_free(errorMessage, TRUE);
	return NULL;
}

/*
 * Find the widget in the loaded template for this window.
 * When finding labels, errors can be ignored as this implies a fixed label
 * \param IN win Pointer to the window object
 * \param IN id  The name to be found
 * \param IN ignore Should we continue the program if the name can't be found?
 * \return the widget or NULL
 */

GtkWidget *
wlibWidgetFromId( wWin_p win, const char *id)
{
	GString *name = g_string_new(id);

    GObject * wi = gtk_builder_get_object(win->builder, name->str);
    g_string_free(name, TRUE);
    return (GtkWidget *)wi;
}

GtkWidget *
wlibAddHiddenContentFromTemplate( wWin_p win, const char *nameStr)
{
	GString *filename;
	filename = wlibFileNameFromDialog( nameStr );
	GError *error = NULL;
    int success = gtk_builder_add_from_file(win->builder, filename->str, &error);
    return NULL;
    if (success == 0) {
		GString *errorMessage = g_string_new("Could not load sub-widget ");
		if (error)
			g_string_append(errorMessage,error->message);
		wNoticeEx( NT_ERROR,
			   errorMessage->str,
			   "OK",
			   NULL );
        g_string_free(errorMessage, TRUE);
        g_clear_error (&error);
		exit(1);
    }
    GString * name = g_string_new(nameStr);
    GtkWidget * wi = (GtkWidget *)wlibGetWidgetFromName( win, name->str, "reveal", FALSE );
    g_string_free(name, TRUE);
    return wi;

}

