/** \file message.c
 * Message line
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

/*
 *****************************************************************************
 *
 * Message Boxes
 *
 *****************************************************************************
 */

//struct wMessage_t {
//	GtkWidget * labelWidget;
//};

/**
 * Set the message text
 *
 * \param b IN widget
 * \param arg IN new text
 * \return
 */

void wMessageSetValue(
        wControl_p b,
        const char * arg)
{
	if (b->widget == NULL) {
		abort();
	}

	gtk_label_set_text(GTK_LABEL(b->widget), wlibConvertInput(arg));
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wMessageSetWidth(
        wControl_p b,
        wWinPix_t width)
{

	gtk_widget_set_size_request(b->widget, width, -1);
}


void wMessageSetLength(wControl_p control, size_t length)
{
	gtk_label_set_width_chars(GTK_LABEL(control->widget), (int)length);
}
/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */
static int fonts_set = 0;

wWinPix_t wMessageGetHeight(
        long flags)
{

	GtkWidget * temp;

	if (!(flags&COMBOBOX)) {
		temp = gtk_label_new("Test");	 //To get size of text itself
	} else {
		temp = gtk_combo_box_text_new();    //to get max size of an object in infoBar
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(temp),"Test");
	}

	if (wMessageSetFont(flags))	{
		if (!fonts_set) {
			GtkStyleContext *context;
			GtkCssProvider *smallProvider = gtk_css_provider_new();
			GtkCssProvider *largeProvider = gtk_css_provider_new();
			/* get the current font descriptor */
			context = gtk_widget_get_style_context(temp);
			static const char smallStyle[] = " .smallLabel { font-size: 70% } ";

			gtk_css_provider_load_from_data (smallProvider,
			                                 smallStyle, -1, NULL);
			gtk_style_context_add_provider(context,
			                               GTK_STYLE_PROVIDER(smallProvider),
			                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

			static const char largeStyle[] = " .largeLabel{ font-size: 140% }  ";

			gtk_css_provider_load_from_data (largeProvider,
			                                 largeStyle, -1, NULL);
			gtk_style_context_add_provider(context,
			                               GTK_STYLE_PROVIDER(largeProvider),
			                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);


			fonts_set = 1;

		}

		/* set the new font size */
		GtkStyleContext * context = gtk_widget_get_style_context(GTK_WIDGET(temp));
		if (flags & BM_LARGE) {
			gtk_style_context_add_class(context, "largeLabel");
		} else {
			gtk_style_context_add_class(context, "smallLabel");
		}

	}

	GtkRequisition min_requisition,natural_requisition;
	gtk_widget_get_preferred_size (temp,&min_requisition,&natural_requisition);
	g_object_ref_sink(temp);
	gtk_widget_destroy(temp);
	return natural_requisition.height;
}

/**
 * Create a widget for a simple text.
 * 
 * ### Usage in dialogs
 *
 * - runtime supported
 * - builder supported
 *
 * ### Options
 * Default
 * : center label in normal font
 *
 * BM_LARGE
 * : use large font
 *
 * BM_SMALL
 * : use small font
 *
 * BM_ALIGNRIGHT
 * : Right align label
 *
 * BM_ALIGNLEFT
 * : Left align label
 *
 * ### CSS
 *
 * .largeLabel
 * : Style for large font
 *
 * .smallLabel
 * : Style for small font
 *
 * \param IN parent		Handle of parent window
 * \param IN x			position in x direction
 * \param IN y			position in y direction
 * \param IN labelStr	identifier
 * \param IN width		horizontal column span of widget
 * \param IN message	message to display ( null terminated )
 * \param IN flags		display options
 * \return handle for created widget
 */

wControl_p wMessageCreateEx(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char* labelStr,
        wWinPix_t	width,
        const char* message,
        long flags)
{
	wControl_p b;

	b = wlibControlNew(B_MESSAGE, parent, NULL, NULL);
	

	if (HASDIALOGBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, labelStr);
	} else {
		b->widget = gtk_label_new(message);

		/* do we need to set a special font? */
		if (wMessageSetFont(flags)) {
			/* set the new font size */
			GtkStyleContext* context = gtk_widget_get_style_context(
				GTK_WIDGET(b->widget));
			if (flags & BM_LARGE) {
				gtk_style_context_add_class(context, "largeLabel");
			} else {
				gtk_style_context_add_class(context, "smallLabel");
			}
		}

		wlibBasicGridAttach(parent, b->widget, x, y, width, 1);

		if (flags & BM_ALIGNRIGHT) {
			gtk_label_set_xalign(GTK_LABEL(b->widget), 1.0);
		}
		if (flags & BM_ALIGNLEFT) {
			gtk_label_set_xalign(GTK_LABEL(b->widget), 0.0);
		}

		gtk_widget_show(b->widget);
	}

	return b;
}

/**
 * Get the anticipated length of a message field
 *
 * \param testString IN string that should fit into the message box
 * \return expected width of message box
 */

wWinPix_t
wMessageGetWidth(const char *testString)
{
//	GtkWidget *entry;
//	GtkRequisition requisition;

	return( wLabelWidth(testString));
//    entry = gtk_entry_new();
//    g_object_ref_sink(entry);
//
//    gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
//    gtk_entry_set_width_chars(GTK_ENTRY(entry), strlen(testString));
//    gtk_entry_set_max_length(GTK_ENTRY(entry), strlen(testString));
//
//    gtk_widget_size_request(entry, &requisition);
//
//    gtk_widget_destroy(entry);
//    g_object_unref(entry);
//
//    return (requisition.width+8);
}

