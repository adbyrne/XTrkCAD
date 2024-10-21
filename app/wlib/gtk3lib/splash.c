/** \file splash.c
 * Handling of the Splash Window functions
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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>

#include "gtkint.h"

#define LOGOFILENAME "logo.png"

static GtkWidget *window;	/**< splash window handle */
static GtkLabel *message;	/**< window handle for progress message */

/**
 * Create the splash window shown during startup. The function loads the logo
 * bitmap and displays the program name and version as passed.
 *
 * \param IN  appName the product name to be shown
 * \param IN  appVer  the product version to be shown
 * \return    TRUE if window was created, FALSE if an error occured
 */

int
wCreateSplash(char *appName, char *appVer)
{
	GtkWidget *grid;
	GtkWidget *image;
	GtkWidget* label;
	char *temp;
	char *logoPath;

	/* create the basic window */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_set_title(GTK_WINDOW(window), appName);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(window),
	                         GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

	gtk_window_set_focus_on_map(GTK_WINDOW(window), FALSE);

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
	gtk_widget_set_margin_bottom(GTK_WIDGET(grid), 12);

	gtk_widget_show(grid);
	gtk_container_add(GTK_CONTAINER(window), grid); 

	/* add the logo image to the top of the splash window */
	logoPath = g_strdup_printf("%s/%s", "/bitmaps", LOGOFILENAME);
	image = gtk_image_new_from_resource(logoPath);
	gtk_widget_show(image);
	g_free(logoPath);
	gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 1);

	/* put the product name into the window */
	temp = g_strdup_printf("%s %s", appName, appVer);
	label = gtk_label_new(temp);
	g_free(temp);

	gtk_widget_show(label);
	gtk_grid_attach_next_to(GTK_GRID(grid), label, image, GTK_POS_BOTTOM, 1, 1);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL);
	gtk_label_set_selectable(GTK_LABEL(label), FALSE);

	message = GTK_LABEL(gtk_label_new("Application is starting..."));
	gtk_widget_show(GTK_WIDGET(message));
	gtk_grid_attach_next_to(GTK_GRID(grid), GTK_WIDGET(message), label,
	                        GTK_POS_BOTTOM, 1, 1);
	gtk_label_set_line_wrap(message, FALSE);
	gtk_label_set_ellipsize(message, PANGO_ELLIPSIZE_END);

	gtk_widget_show(window);

	return (TRUE);
}

/**
 * Update the progress message inside the splash window
 * msg	text message to display
 * return nonzero if ok
 */

int
wSetSplashInfo(char *msg)
{
	if (!window) { return FALSE; }
	if (msg && message) {
		gtk_label_set_text(GTK_LABEL(message), msg);
		return TRUE;
	}

	return FALSE;
}

/**
 * Destroy the splash window.
 *
 */

void
wDestroySplash(void)
{
	/* kill window */
	if (window) { gtk_widget_destroy(window); }
	window = NULL;

	return;
}
