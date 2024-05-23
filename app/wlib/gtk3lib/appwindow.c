/** \file appwindow.c
 * Create and handle the application's main window
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#define MIN_WIDTH 100
#define MIN_HEIGHT 100

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#include "wrapbox/eggwrapbox.h"

#include "gtkint.h"

#include "xtrkcad-config.h"

static struct wWindow_t *appMainWindow;
GtkWidget *
wlibAppWinGetMain()
{
	return(appMainWindow->gtkWindow);
}

GtkAccelGroup* 
wlibAppWinGetAccelGroup()
{
	return(appMainWindow->accelGroup);
}

GtkContainer *
wlibAppWinGetStatusbar()
{
	return(appMainWindow->statusbar);
}

static gboolean
on_widget_deleted(GtkWidget* window, GdkEvent* event, gpointer userData)
{
	if (appMainWindow->winProc) {
		bool rc = appMainWindow->winProc(appMainWindow, wClose_e, userData, NULL);
 		if (!rc) {
			wPrefFlush("");
		}
		return(rc);
	}

	wPrefFlush("");
	return FALSE;
}

/**
 * Initialize the application's main window. This function does the necessary
 * initialization of the application including creation of the main window.
 *
 * \param name IN internal name of the application. Used for filenames etc.
 * \param x    IN Initial window width
 * \param y    IN Initial window height
 * \param helpStr IN Help topic string
 * \param labelStr IN window title
 * \param nameStr IN Window name
 * \param option IN options for window creation
 * \param winProc IN pointer to main window procedure
 * \param data IN User context
 * \return    window handle or NULL on error
 */

 wWindow_p wWinMainCreate(
	 const char *name,		 /* Application name */
	 wWinPix_t x,			 /* Initial window width */
	 wWinPix_t y,			 /* Initial window height */
	 const char *helpStr,	 /* Help topic string */
	 const char *labelStr,	 /* Window title */
	 const char *nameStr,	 /* Window name */
	 long option,			 /* Options */
	 wWinCallBack_p winProc, /* Call back function */
	 void *data)			 /* User context */
 {
	 char *pos;
	 long isMaximized;

	 pos = strchr(name, ';');

	 if (pos)
	 {
		 /* if found, split application name and configuration name */
		 strcpy(wConfigName, pos + 1);
	 }
	 else
	 {
		 /* if not found, application name and configuration name are same */
		 strcpy(wConfigName, name);
	 }

	 wDrawColorWhite = wDrawFindColor(0xFFFFFF);
	 wDrawColorBlack = wDrawFindColor(0x000000);

	 appMainWindow = g_malloc(sizeof(struct wWindow_t));
	 appMainWindow->helpTopic = g_strdup(helpStr);
	 appMainWindow->name = g_strdup(nameStr);
	 appMainWindow->winProc = winProc;

	 appMainWindow->builder = gtk_builder_new_from_resource(
		 XTRKCAD_RESOURCE_PATH
		 "appwindow.ui");

	 appMainWindow->gtkWindow = GTK_WIDGET(gtk_builder_get_object(appMainWindow->builder,
																  "main"));

	 // this is the main application window
	 gtk_application_add_window(wlibGetApp(),
								GTK_WINDOW(appMainWindow->gtkWindow));

	 // create the accelerator group
	 appMainWindow->accelGroup = gtk_accel_group_new();
	 gtk_window_add_accel_group(GTK_WINDOW(appMainWindow->gtkWindow),
		 appMainWindow->accelGroup);

	 gtk_window_set_title(GTK_WINDOW(appMainWindow->gtkWindow), labelStr);

	 if (option & F_MENUBAR)
	 {
		 appMainWindow->menubar = GTK_WIDGET(gtk_builder_get_object(appMainWindow->builder,
																	"menubar"));
	 }

	 GtkContainer *toolbarContainer;
	 toolbarContainer = GTK_CONTAINER(gtk_builder_get_object(appMainWindow->builder,
															 "toolbar"));
	 if (toolbarContainer)
	 {
		 //        appMainWindow->toolbar =  createToolbar(toolbarContainer);
	 }

	 GtkContainer *statusbar = GTK_CONTAINER(gtk_builder_get_object(appMainWindow->builder,
		 "statusbar"));
	 {
		 appMainWindow->statusbar = statusbar;
	 }

	 GtkDrawingArea* drawingArea = GTK_DRAWING_AREA(gtk_builder_get_object(appMainWindow->builder, "maindraw"));

	 g_signal_connect(G_OBJECT(appMainWindow->gtkWindow),
		 "delete-event", G_CALLBACK(on_widget_deleted), NULL);

	 gtk_widget_show_all(appMainWindow->gtkWindow);
	 return appMainWindow;
 }
