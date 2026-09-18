/**
 * \file   separator.c
 * \brief  Separator control
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2026 Martin Fischer
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>

#include <wlib.h>
#include "gtkint.h"

/**
 * Create a separator specifically for toolbar use.
 *
 * \param parent Parent window (should be toolbar)
 * \param width Width in pixels (0 for default)
 * \return Control pointer, or NULL on failure
 */

wControl_p wSeparatorCreateForToolbar(wControl_p parent, int width)
{
	wControl_p control;
	GtkToolItem *separator;
	GtkStyleContext* styleContext;

	// Allocate control structure
	control = wlibControlNew(B_SEPARATOR, parent, NULL, NULL);
	if (!control) {
		return NULL;
	}

	// Create GTK separator tool item
	separator = gtk_separator_tool_item_new();
	if (!separator) {
		g_free(control);
		return NULL;
	}

	styleContext = gtk_widget_get_style_context(GTK_WIDGET(separator));
	gtk_style_context_add_class(styleContext, "toolbar-separator");

	// Make it invisible (just for spacing)
	gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator), FALSE);
	gtk_tool_item_set_homogeneous(separator, FALSE);


	// Set custom width if specified
	if (width > 0) {
		gtk_widget_set_size_request(GTK_WIDGET(separator), width, -1);
	} else {
		// Default spacing
		gtk_widget_set_size_request(GTK_WIDGET(separator), 2, -1);
	}

	// Initialize control structure
	control->type = B_SEPARATOR;
	control->parent = parent;
	control->widget = GTK_WIDGET(separator);

	gtk_widget_show(GTK_WIDGET(separator));
	wlibAddButtonToToolbar(  control, NULL);

	return control;
}