/** \file statusbar.c
 * Status bar
 */

/* 	XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis,
 *                2017 Martin Fischer <m_fischer@sf.net>
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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

struct wStatus_t {
    GtkWidget * labelWidget;
};


/**
 * Set the message text
 *
 * \param b IN widget
 * \param arg IN new text
 * \return
 */

void wStatusSetValue(
    wControl_p b,
    const char * arg)
{
    if (!b || b->widget == 0) {
        return;
    }

    gtk_label_set_text(GTK_LABEL(b->widget), wlibConvertInput(arg));
}

/**
 * Create a window for a simple text.
 *
 * \param IN parent Handle of parent window
 * \param IN x position in x direction
 * \param IN y position in y direction
 * \param IN labelStr ???
 * \param IN width horizontal size of window
 * \param IN message message to display ( null terminated )
 * \param IN flags display options
 * \return handle for created window
 */

wControl_p wStatusCreate(
        wControl_p	parent,
        const char 	* labelStr,
        const char	*message)
{
	wControl_p b;

	b = wlibControlNew(B_STATUS, parent, NULL, NULL);

    b->widget = wlibWidgetFromIdWarn(parent, labelStr);

	gtk_label_set_text(GTK_LABEL(b->widget),
		                   message?wlibConvertInput(message):"");

	gtk_widget_show(b->widget);
	return b;
}

/**
 * Get the anticipated length of a message field
 *
 * \param testString IN string that should fit into the message box
 * \return expected width of message box
 */

wWinPix_t
wStatusGetWidth(const char *testString)
{
    printf("Function at line %d in %s is not implemented!\n", __LINE__, __FILE__);
    return(0);
 //   GtkWidget *entry;
 //   GtkRequisition min_req, nat_req;

 //   entry = gtk_entry_new();
 //   g_object_ref_sink(entry);

 //   gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
 //   gtk_entry_set_width_chars(GTK_ENTRY(entry), strlen(testString));
 //   gtk_entry_set_max_length(GTK_ENTRY(entry), strlen(testString));

 ////   gtk_widget_get_preferred_size(entry, NULL, &requisition);
 //   gtk_widget_get_preferred_size(entry, &min_req, &nat_req);
 //   gtk_widget_destroy(entry);
 //   g_object_unref(entry);

 //   return (nat_req.width);
}

/**
 * Set height of message text
 *
 * \todo Are text properties actually used? remove if not
 * 
 * \param label IN text label
 * \param flags IN text properties (large or small size)
 * 
 * \return text height
 */

wWinPix_t 
wStatusSetRequiredHeight(wControl_p label, long flags)
{
	PangoContext* pcontext = gtk_widget_get_pango_context(label->widget);
	PangoFontMetrics* fontMetrics;
	wWinPix_t height;
	
	fontMetrics = pango_context_get_metrics(pcontext, NULL, NULL);
	height = (wWinPix_t)fontMetrics->height / PANGO_SCALE;

	gtk_widget_set_size_request(label->widget, -1, height);

	pango_font_metrics_unref(fontMetrics);

	return(height);
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wStatusSetWidth(
    wControl_p b,
    wWinPix_t width)
{
	printf("Function at line %d in %s is not implemented!\n", __LINE__, __FILE__);
	return;

 //   b->labelWidth = width;
 //   gtk_widget_set_size_request(b->labelWidget, width, -1);
}
