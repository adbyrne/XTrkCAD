/*
 * \file   sticky.c
 * \brief  Sticky Buttons using the custim sticky_toggle_button class
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

///**
// * Get the state of a group of buttons. If the group consists of
// * radio buttons, the return value is the index of the selected button
// * or -1 for none. If toggle buttons are checked, a bit is set for each
// * button that is active.
// *
// * \param bc IN
// * \returns state of group
// */
//
//static long toggleGetValue(
//        wControl_p bc)
//{
//	GList* child;
//	GList* children;
//	long value = 0;
//	long inx;
//
//	for (children = child = gtk_container_get_children(GTK_CONTAINER(bc->widget)),
//	     inx = 0;
//	     child; child = child->next, inx++) {
//		if (gtk_toggle_button_get_active(child->data)) {
//			value |= (1 << inx);
//		}
//	}
//
//	if (children) {
//		g_list_free(children);
//	}
//
//	return value;
//}
//
///**
// * Set a group of toggle buttons from a bitfield. Each bit of the value is
// * used as the value for a toggle. The bit position is the order of the
// * toggle's creation inside the box.
// *
// * \param bc IN button group
// * \param value IN bitfield
// */
//
//void wToggleSetValue(
//        wControl_p bc,		/* Toggle box */
//        long value)		/* Values */
//{
//	GList* child, * children;
//	long inx;
//
//	for (children = child = gtk_container_get_children(GTK_CONTAINER(bc->widget)),
//	     inx = 0;
//	     child; child = child->next, inx++) {
//		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(child->data),
//		                             (value & (1 << inx)) != 0);
//	}
//
//	if (children) {
//		g_list_free(children);
//	}
//}
//
//
///**
// * Get the active buttons from a group of toggle buttons
// *
// * \param b IN
// * \returns
// */
//
//long wToggleGetValue(
//        wControl_p b)		/* Toggle box */
//{
//	return toggleGetValue(b);
//}


/**
 * Signal handler for button selection in radio buttons and toggle
 * button group
 *
 * \param widget IN the button group
 * \param b IN user context (button group????)
 * \returns always 1
 */

//static int toggled(
//        GtkWidget* widget,
//        gpointer b)
//{
//	wControl_p bc = (wControl_p)b;
//	struct toggle* tcontrol = CONTROL_GET_ATTRIBUTES_PTR(bc, toggle);
//	long value = toggleGetValue(bc);
//
//	if (tcontrol->valueP) {
//		*(tcontrol->valueP) = value;
//	}
//
//	if (tcontrol->action) {
//		tcontrol->action(value, bc->context);
//	}
//
//	return TRUE;
//}


/**
 * Create a group of toggle buttons.
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: yes
 * - builder: yes
 *
 * ### Options
 *
 * BC_HORIZONTAL
 * : buttons are arranged horizontally
 *
 * \param parent    IN parent window
 * \param x,y       IN position in grid
 * \param helpStr   IN Help string
 * \param labelStr  IN Label for group
 * \param option    IN Options
 * \param labels    IN Labels for individual buttons
 * \param valueP    IN Selected value
 * \param action    IN Callback
 * \param context   IN User context
 * \returns toggle button widget
 */
//
//wControl_p wToggleCreate(
//        wControl_p	parent,
//        wWinPix_t	x,
//        wWinPix_t	y,
//        const char* helpStr,
//        const char* labelStr,
//        long	option,
//        const char* const* labels,
//        long* valueP,
//        wChoiceCallBack_p action,
//        void* context)
//{
//	wControl_p b;
//	struct toggle* tcontrol;
//
//	b = wlibControlNew(B_TOGGLE, parent, helpStr, context);
//	tcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, toggle);
//	tcontrol->action = action;
//	tcontrol->valueP = valueP;
//
//	if (ISDEFINEDINBUILDER(parent)) {
//		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
//		GList* child, * children;
//
//		children = gtk_container_get_children(GTK_CONTAINER(b->widget));
//		for (child = children; child; child = child->next) {
//			g_signal_connect(G_OBJECT(child->data), "toggled",
//			                 G_CALLBACK(toggled), b);
//		}
//		if (children) {
//			g_list_free(children);
//		}
//	} else {
//
//		GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
//		gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroll), TRUE);
//		gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroll), TRUE);
//
//		if (option & BC_HORIZONTAL) {
//			b->widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
//		} else {
//			b->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
//		}
//
//		if (b->widget == 0) {
//			abort();
//		}
//
//		gtk_container_add(GTK_CONTAINER(scroll), b->widget);
//
//		if (!(option & BC_NOBORDER)) {
//			GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(
//			                                   b->widget));
//			gtk_style_context_add_class(context, "framed");
//		}
//		gtk_box_set_homogeneous(GTK_BOX(b->widget), FALSE);
//
//		for (const char* const* label = labels; *label; label++) {
//			GtkWidget* butt;
//
//			butt = gtk_check_button_new_with_label(_(*label));
//
//			gtk_box_pack_start(GTK_BOX(b->widget), butt, TRUE, TRUE, 0);
//
//			g_signal_connect(G_OBJECT(butt), "toggled",
//			                 G_CALLBACK(toggled), b);
//
//			wlibAddTooltip(butt, parent->name, helpStr);
//		}
//
//		if (valueP) {
//			wToggleSetValue(b, *valueP);
//		}
//
//		if (labelStr) {
//			wlibAddLabel((wControl_p)b, x-1, y, labelStr);
//		}
//
//		wlibBasicGridAttach(parent, scroll, x, y, 1, 1);
//
//		gtk_widget_show_all(scroll);
//	}
//	return b;
//}

static bool ignoreClick;

static GtkWidget *GetActionButton(wControl_p toolbarButton)
{
	GtkWidget *actionButton;

	if (wButtonIsSplitButton(toolbarButton)) {
		actionButton = GTK_WIDGET(g_object_get_data(G_OBJECT(toolbarButton->widget), "action-button"));
	} else {
		actionButton = toolbarButton->widget;
	}

	return(actionButton);
}
/**
 * Set the status of the button
 *
 * \param bb    IN the button
 * \param value IN TRUE for pressed in, FALSE for raised
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
 * \param styleContext IN		user styleContext as styleContext
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

	/** \todo BO_ABUT should be renamed to BO_OVERFLOW_MENU and be included with the button */
	if (option & BO_ABUT) {
		wlibSetAbutStyle(buttonControl->widget);
	}

	wlibAddButtonToToolbar(buttonControl, helpStr);

	g_signal_connect(G_OBJECT(buttonControl->widget), "clicked",
	                 G_CALLBACK(toolbarClicked), buttonControl);

	return buttonControl;
}