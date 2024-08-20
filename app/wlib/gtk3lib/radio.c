/**
 * \file   radio.c
 * \brief  Radio button
 *
 * \author mf
 * \date   May 2024
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2024 Dave Bullis
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
#include <gdk/gdk.h>

#include "gtkint.h"

/**
 * Get the state of a group of buttons. If the group consists of
 * radio buttons, the return value is the index of the selected button
 * or -1 for none. If toggle buttons are checked, a bit is set for each
 * button that is active.
 *
 * \param bc IN
 * \returns state of group
 */

static long radioGetValue(
        GtkRadioButton *button)
{
	long inx = -1;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
		GSList* group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));
		inx = g_slist_length(group) - g_slist_index(group, button) - 1;
	}

	return inx;
}

/**
 * Set the active radio button in a group
 *
 * \param bc IN button group
 * \param value IN index of active button
 */

void wRadioSetValue(
        wControl_p bc,		/* Radio box */
        long value)		/* Value */
{
	GList* children;
	GList* child;
	long inx;

	children = gtk_container_get_children(GTK_CONTAINER(bc->widget));
	for (child = children, inx = 0; child; child = child->next, inx++) {
		if (inx == value) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data), TRUE);
		}
	}
	if (children) {
		g_list_free(children);
	}
}

/**
 * Get the active button from a group of radio buttons
 *
 * \param bc IN
 * \returns
 */

long wRadioGetValue(
        wControl_p bc)		/* Radio box */
{
	return radioGetValue(GTK_RADIO_BUTTON(bc->widget));
}

/**
 * Signal handler for button selection in radio buttons and toggle
 * button group
 *
 * \param widget IN the button group
 * \param b IN user context (button group????)
 * \returns always 1
 */

static int radioChoice(
        GtkWidget* widget,
        gpointer b)
{
	wControl_p bc = (wControl_p)b;
	long value = radioGetValue(GTK_RADIO_BUTTON(widget));
	struct radio *rcontrol;

	rcontrol = CONTROL_GET_ATTRIBUTES_PTR(bc, radio);

	if (value != -1) {
		if (rcontrol->valueP) {
			*rcontrol->valueP = value;
		}

		if (rcontrol->action) {
			rcontrol->action(value, bc->context);
		}
	}
	return 1;
}

/**
 * Create a group of radio buttons.
 *
 *  * ### Usage in dialogs
 *
 * - Generated: yes
 *
 * ### Options
 * BC_HORIZONTAL
 * : align buttons in horizontal direction, 
 * 
 * BC_NOBORDER
 * : do not draw a frame around buttons
 * 
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param labels IN Labels
 * \param valueP IN Selected value
 * \param action IN Callback
 * \param context IN User context 
 * \returns radio button widget
 */

wControl_p wRadioCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char* helpStr,
        const char* labelStr,
        long	option,
        const char* const* labels,
        long* valueP,
        wChoiceCallBack_p action,
        void* context)
{
	wControl_p b;
	struct radio* rcontrol;
	b = wlibControlNew(B_RADIO, parent, helpStr, context);
	rcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, radio);
	rcontrol->action = action;
	rcontrol->valueP = valueP;

	if (HASDIALOGBUILDER(parent)) {
		/** \todo use builder */

	} else {
		GtkRadioButton* butt0 = NULL;
		const char* const* label;

		if (option & BC_HORIZONTAL) {
			b->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		} else {
			b->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
		}

		if (b->widget == 0) {
			abort();
		}

		if (!(option & BC_NOBORDER)) {
			GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(
			                                   b->widget));
			gtk_style_context_add_class(context, "framed");
		}

		gtk_box_set_homogeneous(GTK_BOX(b->widget), FALSE);

		for (label = labels; *label; label++) {
			GtkWidget *butt;

			butt = gtk_radio_button_new_with_label_from_widget(
			               butt0, *label);
			butt0 = GTK_RADIO_BUTTON(butt);

			gtk_box_pack_start(GTK_BOX(b->widget), butt, TRUE, TRUE, 0);
			gtk_widget_show(butt);
			g_signal_connect(G_OBJECT(butt), "clicked",
			                 G_CALLBACK(radioChoice), b);
			wlibAddHelpString(butt, helpStr);
			wlibAddTooltip(butt, parent->name, helpStr);
		}

		if (option & BB_DEFAULT) {
			gtk_widget_set_can_default(b->widget, TRUE);
			gtk_widget_grab_default(b->widget);
		}

		if (valueP) {
			wRadioSetValue(b, *valueP);
		}

		wlibBasicGridAttach(parent, b->widget, x, y, 1, 1);

		gtk_widget_show_all(b->widget);

		if (labelStr) {
		    wlibAddLabel((wControl_p)b, x-1, y, labelStr);
		}
	}
	return b;
}



