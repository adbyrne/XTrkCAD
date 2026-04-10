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
	g_assert(b != NULL);

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
	g_assert(b != NULL);

	gtk_widget_set_size_request(b->widget, width, -1);
}


void wMessageSetLength(wControl_p control, size_t length)
{
	g_assert(control != NULL);

	gtk_label_set_width_chars(GTK_LABEL(control->widget), (int)length);
}

/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */

wWinPix_t wMessageGetHeight(long flags)
{
	GtkWidget *temp;
	int text_width, text_height;
	PangoLayout *layout;

	temp = gtk_label_new("Test");

	/* the following code applies a different fontsize. It is not currently
	 needed so it is not tested. Use with care ;-)	*/
	if (wMessageSetFont(flags)) {
		PangoFontDescription *fontDesc;
		PangoContext *context;
		int fontSize;

		/* get font info via Pango context */
		context = gtk_widget_get_pango_context(temp);
		fontDesc = pango_font_description_copy(
		                   pango_context_get_font_description(context));

		fontSize = pango_font_description_get_size(fontDesc);

		if (flags & BM_LARGE) {
			pango_font_description_set_size(fontDesc, (int)(fontSize * 1.4));
		} else {
			pango_font_description_set_size(fontDesc, (int)(fontSize * 0.7));
		}

		/* override font via CSS provider instead of deprecated modify_font */
		char css[256];
		char *font_str = pango_font_description_to_string(fontDesc);
		snprintf(css, sizeof(css), "* { font: %s; }", font_str);
		g_free(font_str);

		GtkCssProvider *provider = gtk_css_provider_new();
		gtk_css_provider_load_from_data(provider, css, -1, NULL);
		gtk_style_context_add_provider(
		        gtk_widget_get_style_context(temp),
		        GTK_STYLE_PROVIDER(provider),
		        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(provider);

		pango_font_description_free(fontDesc);
	}

	layout = gtk_label_get_layout(GTK_LABEL(temp));
	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	g_object_ref_sink(temp);
	gtk_widget_destroy(temp);
	g_object_unref(temp);

	return text_height;
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

	if (ISDEFINEDINBUILDER(parent)) {
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


