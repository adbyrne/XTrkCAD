/**
* \file	button.c
* \brief	Buttons
*
* \author mf
* \date   May 2024
*/
/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2024 Martin Fischer
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
#include "wrapbox/eggwrapbox.h"

#include "gtkint.h"
#include "i18n.h"

static wBool_t blockCallback;
#define SetNoCallbackOnClick() (blockCallback = TRUE)
#define SetCallbackOnClick() (blockCallback = FALSE)
#define DoCallbackOnClick() (blockCallback == FALSE)

/*
 *****************************************************************************
 *
 * Simple Buttons
 *
 *****************************************************************************
 */

/**
 * Set the status of the button
 *
 * \param bb    IN the button
 * \param value IN TRUE for pressed in, FALSE for raised
 */

void wButtonSetBusy(wControl_p bb, int value)
{
	SetNoCallbackOnClick();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bb->widget), value);
	SetCallbackOnClick();
}


static GtkWidget *
AddPixbufToButton(GtkWidget* button, GdkPixbuf* pixbuf)
{
	GtkWidget* image;

	image = gtk_image_new_from_pixbuf(pixbuf);
	gtk_container_add(GTK_CONTAINER(button), image);
	gtk_widget_show(image);

	g_object_unref((gpointer)pixbuf);

	return(image);
}

static void
DestroyImage(GtkWidget* image, gpointer unused)
{
	gtk_widget_destroy(image);
}


static void
RemovePixbuf(GtkWidget *button)
{
	if (GTK_IS_BIN(button)) {
		GtkWidget* child = gtk_bin_get_child(GTK_BIN(button));
		if (GTK_IS_IMAGE(child)) {
			gtk_image_clear(GTK_IMAGE(child));
		}
	}
}

bool
IsNewIcon(wIcon_p new, wIcon_p old)
{

	if (new->gtkIconType == ICON_PIXBUF_FROM_RESOURCE) {
		if (g_strcmp0(new->filename, old->filename)) {
			return(true);
		}
	}

	if (new->gtkIconType == ICON_PIXBUF_FROM_TEXT) {
		if ((new->color != old->color) || g_strcmp0(new->text, old->text)) {
			return(true);
		}
	}

	return(false);
}

/**
 * Replace the icon of a button.
 *
 * \param bb		IN button handle
 * \param iconData	IN icon data
 */

void wButtonSetIcon(wControl_p control, wIcon_p icon)
{
	GdkPixbuf* pixbuf = NULL;

	if (control->attributes.button.icon) {
		if (!IsNewIcon(icon, control->attributes.button.icon)) {
			return;
		}
		RemovePixbuf(control->widget);
	}

	pixbuf = icon->bits;

	if (pixbuf) {
		AddPixbufToButton(control->widget, pixbuf);

		control->attributes.button.icon = icon;
	}
}

/**
 * Change the label of a button. This can be used to set the text
 *
 * \param bb		IN button handle
 * \param labelStr	IN new label string
 *
 * \todo icons in XBM format
 */

void wButtonSetLabel(wControl_p bb, const char * labelStr)
{
	gtk_button_set_label(GTK_BUTTON(bb->widget), labelStr);
}

/**
 * Perform the user callback function
 *
 * \param bb IN button handle
 */

void wlibButtonDoAction(
        wControl_p bb)
{
	if (bb->attributes.button.action) {
		bb->attributes.button.action(bb->context);
	}
}

/**
 * Signal handler for button click
 * \param control IN the control or NULL for autorepeat
 * \param value IN the button handle (same as control???)
 */

static void buttonClick(
        GtkWidget* widget,
        gpointer value)
{
	struct button* b = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)value), button);

	if (b->action && DoCallbackOnClick()) {
		b->action(((wControl_p)value)->context);
	}
}


/**
 * Called after expose event default hander - allows the button to be outlined
 */
static wBool_t drawButton(
        GtkWidget* widget,
        cairo_t* cr,
        gpointer g)
{
	return wControlExpose(widget, cr, (wControl_p)g);
}


#define ISDIALOGACTION(options) ((options&BB_HELP)||(options&BB_CANCEL)||(option&BB_DEFAULT))
/**
 * Create a button
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: yes
 * - builder: yes
 *
 * ### Options
 * BB_DEFAULT
 * : set button as default for dialog
 * BO_ICON
 * : use an icon instead of label, label must point to a xpm in memory
 *
 * \param parent IN parent window
 * \param x IN X-position
 * \param y IN Y-position
 * \param helpStr IN Help string
 * \param labelStr IN Label
 * \param option IN Options
 * \param width IN Width of button
 * \param action IN Callback
 * \param styleContext IN User styleContext
 * \returns button control
 *
 * \todo replace XBM format (layer buttons) or add support in buttons.
 * layer buttons are created in dlayer.c
 *
 */

wControl_p wButtonCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char* helpStr,
        const char* labelStr,
        long 	option,
        wWinPix_t 	width,
        wButtonCallBack_p action,
        void* context)
{
	wControl_p b;
	struct button* button;

	b = wlibControlNew(B_BUTTON, parent, helpStr, context);
	button = CONTROL_GET_ATTRIBUTES_PTR(b, button);
	button->action = action;

	if (ISDEFINEDINBUILDER(parent) || ISDIALOGACTION(option)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
		if (labelStr) {
			wButtonSetLabel(b, labelStr);
		}
	} else {
		b->widget = GTK_WIDGET(gtk_toggle_button_new());

		if (width > 0) {
			gtk_widget_set_size_request(b->widget, width, -1);
		}

		if (labelStr) {
			if (option & BO_ICON) {
				if (((wIcon_p)labelStr)->bits == NULL) {
					printf("Invalid icon for %s\n", helpStr);
				}
				wButtonSetIcon(b, (wIcon_p)labelStr);
			} else {
				wButtonSetLabel(b, labelStr);
			}
		}

		wlibBasicGridAttach(parent, b->widget, x, y, 1, 1);

		if (option & BB_DEFAULT) {
			gtk_widget_set_can_default(b->widget, TRUE);
			gtk_widget_grab_default(b->widget);
			gtk_window_set_default(GTK_WINDOW(parent->widget), b->widget);
		}
	}
	gtk_widget_show_all(b->widget);
	g_signal_connect(G_OBJECT(b->widget), "clicked",
	                 G_CALLBACK(buttonClick), b);

	wlibAddTooltip(b->widget, parent->name, helpStr);
//	wlibAddTooltip(b->widget, helpStr);

	return b;
}

static char* down16[] = {
	"7 4 5 1",
	" 	c None",
	".	c #666666",
	"+	c #959595",
	"@	c #6C6C6C",
	"#	c #676767",
	".......",
	"+.....+",
	" +.@#+ ",
	"  +.+  "
};

static GtkWidget*
SetAbutStyle(GtkWidget* button)
{
	GtkStyleContext* styleContext;

	styleContext = gtk_widget_get_style_context(GTK_WIDGET(button));
	gtk_style_context_add_class(styleContext, "toolbar-button-arrow");

	return(button);
}

/**
 * Create a toolbar button
 *
 * ### Usage in dialogs
 *
 * - runtime: yes
 * - builder: not required
 *
 * ### Options
 * BO_GAP
 * : leave some space after the button. Technically this is an invisible
 * separator
 *
 * \param parent IN		application main window
 * \param x,y  IN		unused
 * \param helpStr IN	help string
 * \param icon IN		pointer to icon (XPM)
 * \param option IN		options
 * \param width IN		unused
 * \param action IN		callback
 * \param styleContext IN		user styleContext as styleContext
 * \returns button control
 *
 */

wControl_p wButtonCreateForToolbar(
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
	GtkStyleContext* styleContext;

	g_assert(parent->type == W_MAIN);

	buttonControl = wlibControlNew(B_BUTTON, parent, helpStr, context);
	buttonAttributes = CONTROL_GET_ATTRIBUTES_PTR(buttonControl,button);
	buttonAttributes->action = action;
	buttonControl->widget = GTK_WIDGET(gtk_toggle_button_new());

	if (icon->bits) {
		wButtonSetIcon(buttonControl, icon);
	} else {
		printf("Invalid icon for toolbar button %s\n", helpStr);
	}


	/** \todo BO_ABUT should be renamed to BO_OVERFLOW_MENU and be included with the button */
	if (option & BO_ABUT) {
		SetAbutStyle(buttonControl->widget);
	}

	styleContext = gtk_widget_get_style_context(GTK_WIDGET(buttonControl->widget));
	gtk_style_context_add_class(styleContext, "toolbar-button");
	//if (option & BO_GAP) {
	//	gtk_style_context_add_class(styleContext, "toolbar-button-gap");
	//}

	egg_wrap_box_insert_child(EGG_WRAP_BOX(parent->attributes.window.toolbar),
	                          buttonControl->widget, -1, 0);
	gtk_widget_show_all(buttonControl->widget);

	g_signal_connect(G_OBJECT(buttonControl->widget), "clicked",
	                 G_CALLBACK(buttonClick), buttonControl);

	wlibAddTooltip(buttonControl->widget,NULL, helpStr);
	//wlibAddTooltip(buttonControl->widget, helpStr);

	return buttonControl;
}







