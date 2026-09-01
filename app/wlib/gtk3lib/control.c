/** \file control.c
 * Control Utilities
 */
/*
 * Copyright 2016 Martin Fischer <m_fischer@sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

#define HILITECLASS "hilite"

wControl_p
wlibControlNew(wType_e type, wControl_p parent, const char *name, void* context)
{

	wControl_p newControl = g_malloc0(sizeof(struct control));

	newControl->type = type;
	newControl->context = context;
	newControl->parent = parent;
	newControl->name = name;

	return(newControl);
}
/**
 * Cause the control \a b to be displayed or hidden.
 * Used to hide control (such as a list) while it is being updated.
 *
 * \param b IN Control
 * \param show IN TRUE for visible
 */

void wControlShow(
        wControl_p b,
        wBool_t show)
{
	if (b->type == B_LINES) {
		wlibLineShow((wLine_p)b, show);
		return;
	}

	if (b->widget == NULL) {
		abort();
	}

	if (show) {
		gtk_widget_show(b->widget);

		if (b->label) {
			gtk_widget_show(b->label);
		}
	} else {
		gtk_widget_hide(b->widget);

		if (b->label) {
			gtk_widget_hide(b->label);
		}
	}
}

/**
 * Cause the control \a b to be marked active or inactive.
 * Inactive controls donot respond to actions.
 *
 * \param b IN Control
 * \param active IN TRUE for active
 */

void wControlActive(
        wControl_p b,
        int active)
{
	g_assert(b->widget != NULL);

	if (b->type == B_LIST ) {

		gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(b->widget)), active);
		gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(b->widget),
		                                     active?GTK_SENSITIVITY_ON:GTK_SENSITIVITY_OFF);

	} else {

		gtk_widget_set_sensitive(GTK_WIDGET(b->widget), active);

	}
}

/**
 * Get width of a control
 *
 * \param b IN Control
 * \returns width
 */

wWinPix_t wControlGetWidth(
        wControl_p b)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation(b->widget, &allocation);

	return allocation.width;
}

/**
 * Get height of a control
 *
 * \param b IN Control
 * \returns height
 */

wWinPix_t wControlGetHeight(
        wControl_p b)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation(b->widget, &allocation);

	return allocation.height;
}

/**
 * Get y position of a control
 *
 * \param b IN Control
 * \returns position
 */

// Adjust vertical size
// Probably to account for some dialog components
// The 68 px is really trying to absorb the vertical gap between the top of the
// toplevel window and the top of the main drawing area (menu bar + toolbar +
// hotbar), so that the resulting pixel pair meanssomething when fed through
//  mainD.Pix2CoOrd().

static int wControlHeightAdjustment = 68;

void wControlGetPos(
        wControl_p b,		/* Control */
        wWinPix_t *px,
        wWinPix_t *py)
{
	*px = *py = 0;
	if ( b->parent == NULL ) { return;}

	// get position of parent and child
	GtkAllocation allocationParent;
	gtk_widget_get_allocation(b->parent->widget, &allocationParent);

	GtkAllocation allocationChild;
	gtk_widget_get_allocation(b->widget, &allocationChild);

	// Flip vertical
	allocationChild.y = allocationParent.height - allocationChild.y;

	// find center of widget
	*px = allocationChild.x + allocationChild.width/2;
	*py = allocationChild.y + allocationChild.height/2 - wControlHeightAdjustment;
}


/**
 * Set the context ie. additional user attributes for a control
 *
 * \param b IN control
 * \param context IN user date
 */

void wControlSetContext(
        wControl_p b,
        void * context)
{
	b->context = context;
}

void wControlSetFocus(
        wControl_p b)
{
	if (b == NULL || b->widget == NULL) { return; }
	gtk_widget_grab_focus(b->widget);
}

/**
 * Highlight a control by giving it a colored frame.
 * \param control IN the control
 * \param hilite IN TRUE for highlighting on, FALSE off
 */

void wControlHilite(
        wControl_p control,
        wBool_t hilite)
{
	if ( control == NULL ) { return; }
	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(
	                                   control->widget));
	if (hilite) {
		gtk_style_context_add_class(context, HILITECLASS);
	} else {
		gtk_style_context_remove_class(context, HILITECLASS);
	}
}

void wControlSetCustomTooltip(wControl_p control, char* tooltip)
{
	g_assert(control);

	control->customTooltip = tooltip;
}
