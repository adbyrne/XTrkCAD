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

#include "xtrkcad-config.h"

#define BASICBUILDER_RESOURCE "basicdialog"

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

GtkWidget *
wlibWidgetFromIdWarn(wControl_p win, const char *id)
{
	GtkWidget *wi = wlibWidgetFromId(win, id);
	if (!wi) {
		GString *errorMessage = g_string_new("Could not find widget with id: ");
		g_string_append_printf(errorMessage, "%s", id);
		wNoticeWithIcon(NT_ERROR,
		                errorMessage->str,
		                "OK",
		                NULL);
		g_string_free(errorMessage, TRUE);
		return NULL;
	} else {
		return wi;
	}
}

/*
 * Find the widget in the loaded template for this window.
 * When finding labels, errors can be ignored as this implies a fixed label
 * \param[in] win Pointer to the window object
 * \param[in] id  The name to be found
 * \param[in] ignore Should we continue the program if the name can't be found?
 * \return the widget or NULL
 */

GtkWidget *
wlibWidgetFromId( wControl_p win, const char *id)
{
	GObject * wi = gtk_builder_get_object(win->attributes.window.builder, id);
	return (GtkWidget *)wi;
}

/**
 * Load a window definition from a file and initialize the .
 *
 * \param window
 * \param nameStr
 * \param option
 * \return
 */

GtkWidget*
wlibCreateWindowFromBuilder(wControl_p window, const char* nameStr, long option)
{
	GtkWidget* dialog;
	GtkBuilder* builder;
	char* tempStr = NULL;
	const char* containerName = NULL;
	gchar* resourcePath;

	if (option & DO_FILESYSTEM) {
		// in case filename is given, load builder and create a name from
		// the base filename without extension
		resourcePath = g_strdup(nameStr);
		builder = gtk_builder_new_from_file(resourcePath);
		tempStr = g_path_get_basename(resourcePath);
		tempStr[strlen(tempStr) - 3] = '\0';
		nameStr = tempStr;
		containerName = nameStr;
	} else {
		containerName = (option & F_DEFINEDINBUILDER ? nameStr : BASICBUILDER_RESOURCE);
		resourcePath = g_strconcat(XTRKCAD_RESOURCE_PATH,
		                           containerName,
		                           ".ui",
		                           NULL);
		builder = gtk_builder_new_from_resource(resourcePath);
	}

	dialog = GTK_WIDGET(gtk_builder_get_object(builder, containerName));
	g_object_unref(dialog);
	g_free(resourcePath);
	resourcePath = NULL;

	window->attributes.window.builder = builder;
	window->widget = dialog;
	g_free(tempStr);

	return(dialog);
}
