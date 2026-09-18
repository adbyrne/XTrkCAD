/**
 * \file   toolbarctrl.c
 * \brief
 *
 * \author Martin Fischer
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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

#include <wlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "wrapbox/eggwrapbox.h"

#include "gtkint.h"
#include "i18n.h"

/**
 * Create the toolbar container of the main window and place inside the container passed as
 * parameter.
 *
 * \param container IN the container
 * \return a WrapBox Container for the toolbar widgets
 */

GtkWidget*
wlibToolbarCreate(GtkWidget* container)
{
	GtkWidget* toolbar;

	toolbar = egg_wrap_box_new(EGG_WRAP_ALLOCATE_FREE,
	                           EGG_WRAP_BOX_SPREAD_START,
	                           EGG_WRAP_BOX_SPREAD_START,
	                           0, 0);
	egg_wrap_box_set_minimum_line_children(EGG_WRAP_BOX(toolbar), 15);
	egg_wrap_box_set_natural_line_children(EGG_WRAP_BOX(toolbar), 60);
	egg_wrap_box_set_horizontal_spacing(EGG_WRAP_BOX(toolbar), 0);
	gtk_widget_set_name(toolbar, "toolbar");
	gtk_widget_set_hexpand(toolbar, TRUE);
	gtk_box_pack_start(GTK_BOX(container), toolbar, FALSE, FALSE, 6);
	gtk_widget_show_all(toolbar);

	return(GTK_WIDGET(toolbar));
}


void
wlibSetAbutStyle(GtkWidget* button)
{
	GtkStyleContext* styleContext;

	styleContext = gtk_widget_get_style_context(GTK_WIDGET(button));
	gtk_style_context_add_class(styleContext, "toolbar-button-arrow");

	return;
}


void wlibAddButtonToToolbar(wControl_p buttonControl, const char* helpStr)
{
	wControl_p parent = buttonControl->parent;
	GtkStyleContext* styleContext = gtk_widget_get_style_context(GTK_WIDGET(
	                                        buttonControl->widget));
	gtk_style_context_add_class(styleContext, "toolbar-button");

	egg_wrap_box_insert_child(EGG_WRAP_BOX(parent->attributes.window.toolbar),
	                          buttonControl->widget, -1, 0);
	gtk_widget_show_all(buttonControl->widget);

	wlibAddTooltip(buttonControl->widget, NULL, helpStr);
}