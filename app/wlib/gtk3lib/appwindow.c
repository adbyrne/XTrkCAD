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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
// #include <gdk/gdkkeysyms.h>

#include "gtkint.h"

#include "xtrkcad-config.h"

static wControl_p appMainWindow;

/**
 * Get the application's main window.
 *
 * \return pointer to window information
 */

GtkWidget *
wlibAppWinGetMain()
{
	return(appMainWindow->widget);
}

/**
 * Get the key accelarator group for the application. It is created during
 * startup of the appl
 *
 * \return GTK handle of acc group
 */

GtkAccelGroup*
wlibAppWinGetAccelGroup()
{
	return(appMainWindow->attributes.window.accelGroup);
}

/**
 * Get the container for the statusbar.
 *
 * \return GTK handle for the container
 */

GtkContainer *
wlibAppWinGetStatusbar()
{
	return(appMainWindow->attributes.window.statusbar);
}


/**
 * Handle the destroy event for the main window. Calls the windows callback
 * function that allows the destroy operation to be cancelled
 *
 * \param window see GTK3 docs
 * \param event
 * \param userData
 * \return
 */

static gboolean
on_widget_deleted(GtkWidget* window, GdkEvent* event, gpointer userData)
{
	if (appMainWindow->attributes.window.winProc) {
		bool rc = appMainWindow->attributes.window.winProc(appMainWindow, 
			wClose_e, userData, NULL);
		if (!rc) {
			wPrefFlush(NULL);
		}
		return(rc);
	}

	wPrefFlush("");
	return FALSE;
}

/**
 * This signal handler sets the maximum height of the scrolled window to the
 * height needed by the toolbar.
 *
 * \param self, param allocation IN see GTK3 documentation
 * \param user_data	IN the scrolled window
 */

void
signalSizeAlloc(GtkWidget* self,
                GtkAllocation* allocation,
                gpointer user_data)
{
// 	allocation->height = 0x10;
	
	printf("sizeAlloc: %d\n", allocation->height);
 	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(user_data),
	        allocation->height);

	return(FALSE);
}

/**
 * Create the toolbar area of the main window. This is placed inside the
 * scrolled window passed as the parameter. To make sure that only the
 * minimum necessary height is allocated to that scrolled window, a signal
 * handler is set up.
 *
 * \param toolbarScrolled IN the scrolled window
 * \return a FlowboxContainer for the toolbar widgets
 */

GtkWidget *
CreateToolbar(GtkScrolledWindow* scrolled)
{
	GtkFlowBox* flowbox;
	GtkWidget* viewport;

	flowbox = GTK_FLOW_BOX(gtk_builder_get_object(
		appMainWindow->attributes.window.builder,"toolbar"));
	g_signal_connect(flowbox, "size-allocate", G_CALLBACK(signalSizeAlloc),
	                 scrolled);

	return(GTK_WIDGET(flowbox));
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
 * \param attributes IN User context
 * \return    window handle or NULL on error
 */

wControl_p wWinMainCreate(
        const char *name,		 /* Application name */
        wWinPix_t x,			 /* Initial window width */
        wWinPix_t y,			 /* Initial window height */
        const char *helpStr,	 /* Help topic string */
        const char *labelStr,	 /* Window title */
        const char *nameStr,	 /* Window name */
        long option,			 /* Options */
        wWinCallBack_p winProc, /* Call back function */
        void *attributes)			 /* User context */
{
	char *pos;
	struct window* wcontrol;

	pos = strchr(name, ';');

	if (pos) {
		/* if found, split application name and configuration name */
		strcpy(wConfigName, pos + 1);
	} else {
		/* if not found, application name and configuration name are same */
		strcpy(wConfigName, name);
	}

	wDrawColorWhite = wDrawFindColor(0xFFFFFF);
	wDrawColorBlack = wDrawFindColor(0x000000);

	appMainWindow = g_malloc0(sizeof(struct wWindow_t));
	appMainWindow = wlibControlNew(W_MAIN, NULL, nameStr, attributes);

//	appMainWindow->helpTopic = g_strdup(helpStr);
//	appMainWindow->name = g_strdup(nameStr);
	wcontrol = CONTROL_GET_ATTRIBUTES_PTR(appMainWindow, window);
	wcontrol->winProc = winProc;
	wcontrol->option = option;

	wcontrol->builder = gtk_builder_new_from_resource(
	                                 XTRKCAD_RESOURCE_PATH
	                                 "appwindow.ui");

	appMainWindow->widget = GTK_WIDGET(gtk_builder_get_object(
	                wcontrol->builder,
	                "main"));

	// this is the main application window
	gtk_application_add_window(wlibGetApp(),
	                           GTK_WINDOW(appMainWindow->widget));

	// create the accelerator group
	wcontrol->accelGroup = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(appMainWindow->widget),
	                           wcontrol->accelGroup);

	gtk_window_set_title(GTK_WINDOW(appMainWindow->widget), labelStr);

	if (option & F_MENUBAR) {
		wcontrol->menubar = GTK_WIDGET(gtk_builder_get_object(
		                wcontrol->builder,
		                "menubar"));
	}

	GtkScrolledWindow *scrolled;
	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(wcontrol->builder,
	                               "toolbarWindow"));
	if (scrolled) {
		wcontrol->toolbar =  CreateToolbar(scrolled);
	}

	GtkContainer *statusbar = GTK_CONTAINER(gtk_builder_get_object(
	                wcontrol->builder,
	                "statusbar"));
	{
		wcontrol->statusbar = statusbar;
	}

	//GtkDrawingArea* drawingArea = GTK_DRAWING_AREA(gtk_builder_get_object(
	//                                      wcontrol->builder, "maindraw"));

	g_signal_connect(G_OBJECT(appMainWindow->widget),
	                 "delete-event", G_CALLBACK(on_widget_deleted), NULL);

	gtk_widget_show_all(appMainWindow->widget);
	return appMainWindow;
}
