/**
 * \file   radio.c
 * \brief  Radio button
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
 * Checks the state of a radio button. If the button is active, return value is the index of the
 * button in its group.
 *
 * \param button IN radio button
 * \returns index of button within group if active, -1 otherwise
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
	long inx = 0;

	children = gtk_container_get_children(GTK_CONTAINER(bc->widget));
	for (child = children; child; child = child->next) {
		if (GTK_IS_RADIO_BUTTON(child->data)) {
			if (inx == value) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data), TRUE);
			}
			inx++;
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
	GList* children;
	GList* child;
	long inx = 0;

	children = gtk_container_get_children(GTK_CONTAINER(bc->widget));
	for (child = children; child; child = child->next) {
		GtkToggleButton *currentButton = child->data;

		if (gtk_toggle_button_get_active(currentButton)) {
			break;
		}
		inx++;
	}
	return inx;
}


#define ISACTIVEBUTTON(index)  (index != -1)
/**
 * Signal handler for button selection in radio buttons
 *
 * \param widget IN the button group
 * \param b IN user context
 * \returns always 1
 */

static int radioChoice(
        GtkWidget* widget,
        gpointer b)
{

	wControl_p bc = (wControl_p)b;
	if (bc->type == B_RADIO &&
	    !(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))) {
		return 1;
	}
	long activeIndex = radioGetValue(GTK_RADIO_BUTTON(widget));

	if (ISACTIVEBUTTON(activeIndex)) {
		struct radio* rcontrol = CONTROL_GET_ATTRIBUTES_PTR(bc, radio);
		if ( rcontrol->recursion ) {
			return 1;
		}
		if (rcontrol->valueP) {
			*rcontrol->valueP = activeIndex;
		}

		if (rcontrol->action) {
			rcontrol->recursion++;
			rcontrol->action(activeIndex, bc->context);
			rcontrol->recursion--;
		}
	}
	return 1;
}

static void
SetSignalsToRadioButton(GtkWidget* radioButton, gpointer data)
{
	g_signal_connect(G_OBJECT(radioButton), "toggled",
	                 G_CALLBACK(radioChoice), data);
}

static void
SetHelpToRadioButton(GtkWidget* radioButton, wControl_p control,
                     const char* help)
{
	//wlibAddHelpString(radioButton, help);
	wlibAddTooltip(radioButton, control->parent->name, help);
}

static void
ConfigureButtons(GtkWidget *container, wControl_p control, const char *help )
{
	GList* children;
	GList* current;

	children = gtk_container_get_children(GTK_CONTAINER(container));
	for (current = children; current != NULL; current = g_list_next(current)) {
		SetSignalsToRadioButton(GTK_WIDGET(current->data), control);
		SetHelpToRadioButton(GTK_WIDGET(current->data), control, help);
	}

	g_list_free(children);

}

void RuntimeCreateRadiobuttons(long option, wControl_p b,
                               const char* const* labels, const char* labelStr, wWinPix_t x, wWinPix_t y,
                               wControl_p parent)
{
	GtkWidget* newRadioButton = NULL;
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
		GtkStyleContext* styleContext = gtk_widget_get_style_context(GTK_WIDGET(
		                                        b->widget));
		gtk_style_context_add_class(styleContext, "framed");
	}

	gtk_box_set_homogeneous(GTK_BOX(b->widget), FALSE);

	for (label = labels; *label; label++) {
		newRadioButton = gtk_radio_button_new_with_label_from_widget(
		                         GTK_RADIO_BUTTON(newRadioButton), *label);
		gtk_box_pack_start(GTK_BOX(b->widget), newRadioButton, TRUE, TRUE, 0);
	}

	if (labelStr) {
		wlibAddLabel((wControl_p)b, x - 1, y, labelStr);
	}

	wlibBasicGridAttach(parent, b->widget, x, y, 1, 1);
	gtk_widget_show_all(b->widget);
}

/**
 * Create a group of radio buttons.
 *
 *  * ### Usage in dialogs
 *
 * - Runtime: yes
 * - Builder: yes
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
	rcontrol->recursion = 0;

	if (ISDEFINEDINBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);

	} else {
		RuntimeCreateRadiobuttons(option, b, labels, labelStr, x, y, parent);
	}

	ConfigureButtons(b->widget, b, helpStr );

	if (valueP) {
		wRadioSetValue(b, *valueP);
	}

	return b;
}



