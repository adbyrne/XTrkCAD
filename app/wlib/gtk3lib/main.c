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
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "gtkint.h"
#include "i18n.h"

#include "resources.h"
#include "symbols.h"

#define XTRKCAD_APPL_ID "org.xtrackcad.wlib"

static char *appName;  /**< application name */
char *wExecutableName; /**< full path to executable, taken from argv[0] */

static GtkApplication *app;
static int myargc;    /**< count of command line options */
static char **myargv; /**< command line options */
static wLogCallBack_p logCallBack; /**< gets GLib's messages, see wSetLogCallBack() */

/**
 * Initialize the application name for later use
 *
 * \param _appName IN Name of application
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

/**
 * Pass GLib's and GTK's messages (warnings and worse) to the application
 * too. On Windows nobody sees stderr, so the application puts them in its
 * log file.
 *
 * \param _logCallBack IN gets each message as one formatted line, NULL to stop
 */

void
wSetLogCallBack(wLogCallBack_p _logCallBack)
{
	logCallBack = _logCallBack;
}

/**
 * GLib log writer: hand the message to the application's log callback, then
 * to GLib's default writer. Aborting on fatal messages stays with GLib.
 *
 * \param logLevel IN level of the message
 * \param fields IN the message's fields
 * \param nFields IN number of fields
 * \param userData IN not used
 * \return result of GLib's default writer
 */

static GLogWriterOutput
LogWriter(GLogLevelFlags logLevel, const GLogField *fields, gsize nFields,
          gpointer userData)
{
	static GRecMutex mutex;
	static gboolean busy; /* writing to the log may cause messages itself */

	if (logCallBack && (logLevel & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
	                                G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE))) {
		g_rec_mutex_lock(&mutex);
		if (!busy) {
			gchar *line = g_log_writer_format_fields(logLevel, fields, nFields,
			              FALSE);
			busy = TRUE;
			logCallBack(line);
			busy = FALSE;
			g_free(line);
		}
		g_rec_mutex_unlock(&mutex);
	}

	return g_log_writer_default(logLevel, fields, nFields, userData);
}

/**
 * Load CSS definitions from resource. Name of the CSS-file is xtrackcad.css
 *
 * \todo Connect signal parsing-error and show / log errors
 *
 */

static void
LoadStyles(void)
{
	GtkCssProvider* cssProvider = gtk_css_provider_new();

	gtk_css_provider_load_from_resource(cssProvider,
	                                    XTRKCAD_RESOURCE_PATH CSS_FILENAME);

	gtk_style_context_add_provider_for_screen(
	        gdk_screen_get_default(), GTK_STYLE_PROVIDER(cssProvider),
	        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
activate(GtkApplication *application, gpointer user_data)
{
	// signal is not handled by xtrackcad, so a dummy is needed
}

static void
startup(GtkApplication *application )
{
	g_resources_register(wlib_get_resource());
	g_resources_register(symbols_get_resource());

	// debugging aid: show files in the resource
	// GError *error = NULL;
	// char** children = g_resource_enumerate_children(wlib_get_resource(), "/", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);

	// for (int i = 0; children[i] != NULL; i++) {
	// 	printf("%s\n", children[i]);
	// }
	// g_strfreev(children);

	// children = g_resource_enumerate_children(symbols_get_resource(), "/", G_RESOURCE_LOOKUP_FLAGS_NONE, &error);
	//
	//for (int i = 0; children[i] != NULL; i++) {
	//	printf("%s\n", children[i]);
	//}

	//g_strfreev(children);

	// load css

	// system dialogs should not show buttons in titlebar for consistency with the custom dialogs
	GtkSettings *settings = gtk_settings_get_default();
	g_object_set(settings, "gtk-dialogs-use-header", FALSE, NULL);

	LoadStyles();

	wMain(myargc, myargv );

	wPrefFlush("");

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
	return( 0 );
}

/*
 *******************************************************************************
 *
 * Main
 *
 *******************************************************************************
 */

int main(int argc, char *argv[])
{
	int status;

	if (getenv("GTKLIB_NOLOCALE") == 0) {
		setlocale(LC_ALL, "en_US");
	}

	// XTRKCAD_APP_ID env var set when running via flatpak to avoid
	// DBus.Error.ServiceUnknown
	const char *app_id = getenv("XTRKCAD_APP_ID");
	if (!app_id || !*app_id) {
		app_id = XTRKCAD_APPL_ID;
	}

	// The version option must work without a display (Development Issues
	// #40), so handle it before GTK is initialized. Options start with '/'
	// on Windows (win32/getopt.c).
#ifdef WINDOWS
	const char *versionOption = "/V";
#else
	const char *versionOption = "-V";
#endif
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], versionOption) == 0) {
			wPrintVersion();
			return 0;
		}
	}

	g_log_set_writer_func(LogWriter, NULL, NULL);

	gtk_init(&argc, &argv);

	app = gtk_application_new(app_id, G_APPLICATION_NON_UNIQUE);

	g_signal_connect(app, "command-line", G_CALLBACK(command_line), NULL );
	g_signal_connect(app, "startup", G_CALLBACK(startup), NULL);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

	myargc = argc;
	myargv = argv;
	wExecutableName = argv[0];

	status = g_application_run(G_APPLICATION(app), 0, NULL);

	g_object_unref(app);

	return status;
}

