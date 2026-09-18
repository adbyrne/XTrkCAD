/*
 * \file   sticky.c
 * \brief  Sticky Buttons using the custom sticky_toggle_button class
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"
#include "stickytogglebutton.h"

static bool ignoreClick;

static GtkWidget *GetActionButton(wControl_p toolbarButton)
{
	GtkWidget *actionButton;

	if (wButtonIsSplitButton(toolbarButton)) {
		actionButton = GTK_WIDGET(g_object_get_data(G_OBJECT(toolbarButton->widget),
		                          "action-button"));
	} else {
		actionButton = toolbarButton->widget;
	}

	return(actionButton);
}
/**
 * Set the status of the button
 *
 * \param bb    IN the button
 * \param newState IN TRUE for pressed in, FALSE for raised
 */

void wStickySetBusy(wControl_p bb, int newState)
{
	GtkWidget * targetButton = GetActionButton(bb);
	gboolean currentState;


	currentState = gtk_toggle_button_get_active(targetButton);
	if (currentState != newState) {
		ignoreClick = true;
		gtk_toggle_button_set_active(targetButton, newState);
		ignoreClick = false;
	}
}

wBool_t wStickyGetSticky(wControl_p b)
{
	GtkWidget * targetButton = GetActionButton(b);

	return sticky_toggle_button_get_sticky(STICKY_TOGGLE_BUTTON(targetButton));
}

void wStickySetSticky(wControl_p b, wBool_t newSticky)
{
	GtkWidget * targetButton = GetActionButton(b);

	sticky_toggle_button_set_sticky(STICKY_TOGGLE_BUTTON(targetButton), newSticky);
}

static void toolbarClicked(StickyToggleButton* widget, gpointer value)
{
	struct button* b = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)value), button);

	if (b->action && !ignoreClick) {
		b->action(((wControl_p)value)->context);
	}
}

/**
 * Create a toolbar sticky button
 *
 * ### Usage in dialogs
 *
 * - runtime: yes
 * - builder: not required
 *
 * ### Options
 * BO_GAP
 * : leave some space after the button.
 *
 * \param parent IN		application main window
 * \param x,y  IN		unused
 * \param helpStr IN	help string
 * \param icon IN		pointer to icon
 * \param option IN		options
 * \param width IN		unused
 * \param action IN		callback
 * \param context IN		user context
 * \returns button control
 *
 */

wControl_p wStickyCreateForToolbar(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char* helpStr,
        wIcon_p icon,
        long 	option,
        wWinPix_t 	width,
        wButtonCallBack_p action,
        void* context)
{
	wControl_p buttonControl;
	struct button* buttonAttributes;

	g_assert(parent->type == W_MAIN);
	g_assert(icon->bits);

	buttonControl = wlibControlNew(B_STICKY, parent, helpStr, context);
	buttonAttributes = CONTROL_GET_ATTRIBUTES_PTR(buttonControl, button);
	buttonAttributes->action = action;
	buttonControl->widget = GTK_WIDGET(sticky_toggle_button_new());

	wButtonSetIcon(buttonControl, icon);

	if (option & BO_ABUT) {
		wlibSetAbutStyle(buttonControl->widget);
	}

	wlibAddButtonToToolbar(buttonControl, helpStr);

	g_signal_connect(G_OBJECT(buttonControl->widget), "clicked",
	                 G_CALLBACK(toolbarClicked), buttonControl);

	return buttonControl;
}