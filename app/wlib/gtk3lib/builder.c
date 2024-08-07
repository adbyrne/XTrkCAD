/** \file builder.c
 * Gtk.Builder functions
 */

 /*  XTrkCad - Model Railroad CAD
  *  Copyright (C) 2005 Dave Bullis, 2018 Martin Fischer
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



#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "gtkint.h"

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
	gchar* cwd = g_get_current_dir();
	GString *filename = g_string_new(cwd);
	g_free(cwd);
#endif  //NDEBUG    
	g_string_append(filename, "/ui/");
	g_string_append(filename, dialog );
	g_string_append(filename, ".ui");

	return( filename );
}

/**
 * Check for the existance of a ui definition
 * 
 * \param name IN name of ui definition
 * \return true if exists, false otherwise
 */

bool
wlibExistsTemplate(const char *name)
{
	GString *filename;
	bool exists = false;

	filename = wlibFileNameFromDialog( name );
	if(g_file_test(filename->str, G_FILE_TEST_EXISTS)) {
		exists = true;
	}
	g_string_free(filename, true);

	return(exists);
}

/*
 * Load the GTK builder object for this window. The filename is assumed to be 
 * the same as the nameStr and the window object id.
 * In the window object the builder field will be set with the loaded object 
 * and the gtkwin field populated
 * 
 * \param IN winType passed through to wlibAlloc
 * \param IN labelStr the name that will be shown in the title of the window
 * \param IN nameStr the name to look up
 * \param IN option the creation options
 * \param INOUT attributes passed through to wlibAlloc
 * \return the window object pointer
 *
 * \todo Check signature for unused parameters
 */

wControl_p
wlibDialogFromTemplate( int winType, const char *labelStr, const char *nameStr,
                        long option, void *attributes )
{
	wControl_p w;
	GString *filename;
	struct window* dcontrol;

	w = wlibControlNew(winType, NULL, nameStr, attributes);
	dcontrol = CONTROL_GET_ATTRIBUTES_PTR(w, window);

	filename = wlibFileNameFromDialog( nameStr );

	dcontrol->builder = gtk_builder_new_from_file(filename->str);
	if( !dcontrol->builder ) {
		GString *errorMessage = g_string_new("Could not load ");
		g_string_append( errorMessage, filename->str);
		wNoticeEx( NT_ERROR,
		           errorMessage->str,
		           "OK",
		           NULL );
		exit(1);
	}
	w->widget = (GtkWidget *)gtk_builder_get_object(dcontrol->builder,
	                nameStr);
	if (!w->widget) {
		GString *errorMessage = g_string_new("Could not find window object ");
		g_string_append( errorMessage, nameStr);
		wNoticeEx( NT_ERROR,
		           errorMessage->str,
		           "OK",
		           NULL );
		exit(1);
	}
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
wlibGetWidgetFromName( wControl_p parent, const char *dialogname,
                       const char *suffix, wBool_t ignore_failure )

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
wlibWidgetFromIdWarn(wControl_p win, const char *id)
{
	GtkWidget *wi = wlibWidgetFromId(win, id);
	if (!wi)
	{
		GString *errorMessage = g_string_new("Could not find widget with id: ");
		g_string_append_printf(errorMessage, "%s", id);
		wNoticeEx(NT_ERROR,
				  errorMessage->str,
				  "OK",
				  NULL);
		g_string_free(errorMessage, TRUE);
		return NULL;
	}
	else
	{
		return wi;
	}
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
wlibWidgetFromId( wControl_p win, const char *id)
{
	GObject * wi = gtk_builder_get_object(win->attributes.window.builder, id);
	return (GtkWidget *)wi;
}

void
wlibAddContentFromTemplate( wWin_p win, const char *nameStr)
{
	GString *filename;
	filename = wlibFileNameFromDialog( nameStr );
	GError *error = NULL;
	int success = gtk_builder_add_from_file(win->builder, filename->str, &error);
	if (success == 0) {
		GString *errorMessage = g_string_new("Could not load sub-widget with name: ");
		if (error) {
			g_string_append(errorMessage,error->message);
		}
		wNoticeEx( NT_ERROR,
		           errorMessage->str,
		           "OK",
		           NULL );
		g_string_free(errorMessage, TRUE);
		g_clear_error (&error);
		exit(1);
	}

}
