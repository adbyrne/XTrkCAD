/** \file main.c
 * Main function and initialization stuff
 *
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005,2012 Dave Bullis Martin Fischer
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

#include <locale.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

#include "resources.h"

static char *appName;		/**< application name */
static GtkApplication *app;
static int argc;			/**< count of command line options */
static char **argv;			/**< command line options */

/**
 * Initialize the application name for later use
 *
 * \param _appName IN Name of application
 * \return
 */

void
wInitAppName(char *_appName)
{
	appName = g_strdup( _appName );
}

char *
wlibGetAppName()
{
	return( appName );
}

GtkApplication *
wlibGetApp()
{
	return(app);
}

static char* style = ".statusbar { background-image: none; background-color: rgb(192, 192, 192); color: white;}";

static void
LoadStyles(void)
{	
    	GtkCssProvider* cssProvider = gtk_css_provider_new();
	GError* error = NULL;
	
	gtk_css_provider_load_from_data(cssProvider, style, strlen(style), &error);
	//gtk_css_provider_load_from_resource(cssProvider,
	//	XTRKCAD_RESOURCE_PATH
	//	"xtrackcad.css");

 	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), 
		cssProvider, 
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

 }

static void 
startup(GtkApplication *app)
{	
	wWindow_p window;

	g_resources_register(wlib_get_resource());

	// load css
	LoadStyles();

	window = wMain(argc, argv );
	
	g_strfreev(argv);
}

/**
 * Activate the application by calling the main program after registering
 * the binary resources
 *
 * \param app 			see activate signal
 * \param user_data 	unused
 */

static void
activate(GtkApplication* app, gpointer user_data)
{
    g_resources_register(wlib_get_resource());
}

/**
 * Get the command line parameters and make them available to the main program.
 * argc and argv are retrieved and stored in globals for further usage
 *
 * \param self 	 	see command_line signal
 * \param cmdLine 	see command_line signal
 * \param user_data not used
 * \return gint always 0 as parameters aren't checked
 */

static gint
command_line( GApplication* self, GApplicationCommandLine* cmdLine,
               gpointer user_data )
{
	argv = g_application_command_line_get_arguments(
	               cmdLine,
	               &argc);

	return( 0 );
}

/*
 *******************************************************************************
 *
 * Main
 *
 *******************************************************************************
 */

int main( int argc, char *argv[] )
{
	int status;

	if ( getenv( "GTKLIB_NOLOCALE" ) == 0 ) {
		setlocale( LC_ALL, "en_US" );
	}

	app = gtk_application_new("org.xtrackcad.wlib",
	                           G_APPLICATION_HANDLES_COMMAND_LINE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	g_signal_connect(app, "startup", G_CALLBACK(startup), NULL);
	g_signal_connect(app, "command-line", G_CALLBACK(command_line), NULL );

	status = g_application_run(G_APPLICATION (app), argc, argv);

	g_object_unref (app);

	return status;
}
