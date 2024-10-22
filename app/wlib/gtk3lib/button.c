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

/**
 * Set the label of a button, does also allow to set an icon.
 * If BO_ICON is set, labelStr is interpreted as pointer to XPM or XBM images
 *
 * \param widget    IN  the button
 * \param option    IN
 * \param labelStr  IN  the pixel data or the label text
 * \param labelG    IN
 * \param imageG    IN
 *
 * \todo Check usage for large icons and for text labels
 * \todo is scaling of the icon still used?
 */

void wlibSetLabel(
        GtkWidget *widget,
        long option,
        const char * labelStr,
        GtkLabel * * labelG,
        GtkWidget * * imageG)
{
	wIcon_p bm;

	if (widget == 0) {
		abort();
	}

	if (labelStr) {
		if (option&BO_ICON) {
			GdkPixbuf *pixbuf;

			bm = (wIcon_p)labelStr;

			if (bm->gtkIconType == ICON_PIXMAP) {
				pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)bm->bits);
			} else {
				pixbuf = wlibPixbufFromXBM( bm );
			}
			double scaleicon;
			wPrefGetFloatBasic(PREFSECTION, LARGEICON, &scaleicon, 1.0);
			if (scaleicon<1.0) { scaleicon=1.0; }
			if (scaleicon>2.0) { scaleicon=2.0; }
			GdkPixbuf *pixbuf2 =
			        gdk_pixbuf_scale_simple(pixbuf,
			                                (int)(gdk_pixbuf_get_width(pixbuf)*scaleicon),
			                                (int)(gdk_pixbuf_get_height(pixbuf)*scaleicon),
			                                GDK_INTERP_BILINEAR);
			g_object_ref_sink(pixbuf);
			g_object_unref((gpointer)pixbuf);
			if (*imageG==NULL) {
				*imageG = gtk_image_new_from_pixbuf(pixbuf2);
				gtk_container_add(GTK_CONTAINER(widget), *imageG);
				gtk_widget_show(*imageG);
			} else {
				gtk_image_set_from_pixbuf(GTK_IMAGE(*imageG), pixbuf2);
			}
			g_object_ref_sink(pixbuf2);
			g_object_unref((gpointer)pixbuf2);
		} else {
			if (*labelG==NULL) {
				*labelG = (GtkLabel*)gtk_label_new(wlibConvertInput(labelStr));
				gtk_container_add(GTK_CONTAINER(widget), (GtkWidget*)*labelG);
				gtk_widget_show((GtkWidget*)*labelG);
			} else {
				gtk_label_set_text(*labelG, wlibConvertInput(labelStr));
			}
		}
	}
}

static 
GdkPixbuf *wlibNewPixbufFromXBM(wIcon_p ip)
{
	GdkPixbuf* pixbuf;
	char line0[40];
	char line2[40];

	const char* bits;
	long rgb;
	int wb;
	char** pixmapData;

	wb = (ip->w + 7) / 8;
	pixmapData = (char**)g_malloc((3 + ip->h) * sizeof * pixmapData);
	pixmapData[0] = line0;
	rgb = ip->color;
	sprintf(line0, " %ld %ld 2 1", ip->w, ip->h);
	sprintf(line2, "# c #%2.2lx%2.2lx%2.2lx", (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF,
		rgb & 0xFF);
	pixmapData[1] = ". c None s None";
	pixmapData[2] = line2;
	bits = ip->bits;

	for (int row = 0; row < ip->h; row++) {
		pixmapData[row + 3] = (char*)g_malloc((ip->w + 1) * sizeof * *pixmapData);

		for (int col = 0; col < ip->w; col++) {
			if (bits[row * wb + (col >> 3)] & (1 << (col & 07))) {
				pixmapData[row + 3][col] = '#';
			}
			else {
				pixmapData[row + 3][col] = '.';
			}
		}

		pixmapData[row + 3][ip->w] = 0;
	}

	pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)pixmapData);

	for (int row = 0; row < ip->h; row++) {
		g_free(pixmapData[row + 3]);	
	}

	return(pixbuf);
}



/**
 * Change the icon of a button.
 * The icon has to be in XPM format.
 *
 * \param bb		IN button handle
 * \param iconData	IN icon data
 *
 * \todo icons in XBM format
 */

void wButtonSetIcon(wControl_p bb, wIcon_p icon)
{
	GtkWidget* image;
	GdkPixbuf* pixbuf = NULL;

	if (icon->gtkIconType == ICON_BITMAP) {
		pixbuf = wlibNewPixbufFromXBM(icon);
	}

	if (icon->gtkIconType == ICON_PIXMAP) {
		pixbuf = gdk_pixbuf_new_from_xpm_data(icon->bits);
	}

	if (pixbuf) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_container_add(GTK_CONTAINER(bb->widget), image);
		g_object_ref_sink(pixbuf);
		g_object_unref((gpointer)pixbuf);

		if (image) {
			gtk_widget_show(image);
		}
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
 * \param widget IN the widget or NULL for autorepeat
 * \param value IN the button handle (same as widget???)
 */

static void buttonClick(
        GtkWidget *widget,
        gpointer value)
{
	struct button *b = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)value),button);

	if (b->action && DoCallbackOnClick()) {
		b->action(((wControl_p)value)->context);
	}
}


/**
 * Called after expose event default hander - allows the button to be outlined
 */
static wBool_t drawButton(
        GtkWidget *widget,
        cairo_t *cr,
        gpointer g)
{
	return wControlExpose(widget, cr, (wControl_p)g);
}

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
 * \returns button widget
 *
 * \todo replace XBM format (layer buttons) or add support in buttons.
 * layer buttons are created in dlayer.c
 *
 */

wControl_p wButtonCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char 	* helpStr,
        const char	* labelStr,
        long 	option,
        wWinPix_t 	width,
        wButtonCallBack_p action,
        void 	* context)
{
	wControl_p b;
	struct button* button; 

	b = wlibControlNew( B_BUTTON, parent, helpStr, context );
	button = CONTROL_GET_ATTRIBUTES_PTR(b, button);
	button->action = action;

	if (HASDIALOGBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
	} else {
		b->widget = GTK_WIDGET(gtk_toggle_button_new());

		if (width > 0) {
			gtk_widget_set_size_request(b->widget, width, -1);
		}

		if (labelStr) {
			if (option & BO_ICON) {
				wButtonSetIcon(b, (wIcon_p)labelStr);
			}
			else {
				wButtonSetLabel(b, labelStr);
			}
		}

		wlibBasicGridAttach(parent, b->widget, x, y, width, 1);

		if (option & BB_DEFAULT) {
			gtk_widget_set_can_default(b->widget, TRUE);
			gtk_widget_grab_default(b->widget);
			gtk_window_set_default(GTK_WINDOW(parent->widget), b->widget);
		}

		gtk_widget_show(b->widget);
	}

	g_signal_connect(G_OBJECT(b->widget), "clicked",
	                 G_CALLBACK(buttonClick), b);

	wlibAddHelpString(b->widget, helpStr);
	return b;
}

static
AllocateSquareHandler(GtkWidget* widget, GtkAllocation* allocation, gpointer unused)
{
	gint width = allocation->width;
	gint height = allocation->height;

	width = (width < height ? height : width);
	gtk_widget_set_size_request(widget, width, height);
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
"  +.+  " };

/**
 * Create a toolbar button
 *
 * ### Usage in dialogs
 *
 * - Generated: yes
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
 * \returns button widget
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
	GtkAspectFrame* aspectFrame;

	/**
	 * \todo make sure that parent is appmainwindow.
	 */

	buttonControl = wlibControlNew(B_BUTTON, parent, helpStr, context);
	buttonAttributes = CONTROL_GET_ATTRIBUTES_PTR(buttonControl,button);
	buttonAttributes->action = action;
	buttonControl->widget = GTK_WIDGET(gtk_toggle_button_new());

	wButtonSetIcon(buttonControl, icon);

	styleContext = gtk_widget_get_style_context(GTK_WIDGET(buttonControl->widget));
	gtk_style_context_add_class(styleContext, "toolbar-button");

	aspectFrame = GTK_ASPECT_FRAME(gtk_aspect_frame_new(NULL, 0.5, 0.5, 1.0, TRUE));
	gtk_widget_show(GTK_WIDGET(aspectFrame));
	gtk_container_add(GTK_CONTAINER(aspectFrame), buttonControl->widget);

	/** \todo BO_ABUT should be renamed to BO_OVERFLOW_MENU and be included with the button */
	if (option & BO_ABUT) {
		GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		GtkWidget* downArrowButton = gtk_button_new();
		GdkPixbuf* pixbuf;

		styleContext = gtk_widget_get_style_context(GTK_WIDGET(downArrowButton));
		gtk_style_context_add_class(styleContext, "toolbar-button-arrow");

		gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(aspectFrame), FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(box), downArrowButton, FALSE, FALSE, 0);
		gtk_widget_set_halign(downArrowButton, GTK_ALIGN_START);

		gtk_widget_show(downArrowButton);

		pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)down16);
		if (pixbuf) {
			GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
			gtk_container_add(GTK_CONTAINER(downArrowButton), image);
			g_object_unref((gpointer)pixbuf);
		}
		gtk_widget_show(box);
		egg_wrap_box_insert_child(EGG_WRAP_BOX(parent->attributes.window.toolbar), box, -1, 0);
	}
	else {
		egg_wrap_box_insert_child(EGG_WRAP_BOX(parent->attributes.window.toolbar), GTK_WIDGET(aspectFrame), -1, 0);
	}

	gtk_widget_show(buttonControl->widget);

	g_signal_connect(G_OBJECT(buttonControl->widget), "clicked",
		G_CALLBACK(buttonClick), buttonControl);

	if (option & BO_GAP) {
		GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);

		egg_wrap_box_insert_child(EGG_WRAP_BOX(parent->attributes.window.toolbar), separator, -1, 0);
		gtk_widget_show(separator);
	}

	wlibAddHelpString(buttonControl->widget, helpStr);
	wlibAddTooltip(buttonControl->widget, parent->name, helpStr);

	return buttonControl;
}







