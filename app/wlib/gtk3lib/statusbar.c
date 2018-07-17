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


#include <stdlib.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

struct wStatus_t {
    WOBJ_COMMON
    GtkWidget * labelWidget;
    const char * message;
    wPos_t labelWidth;
};

/**
 * Set the message text
 *
 * \param b IN widget
 * \param arg IN new text
 * \return
 */

void wStatusSetValue(
    wStatus_p b,
    const char * arg)
{
    if (b->widget == 0) {
        abort();
    }

    if (gtk_entry_get_max_length(GTK_ENTRY(b->labelWidget))<strlen(arg)) {
        gtk_entry_set_max_length(GTK_ENTRY(b->labelWidget), strlen(arg));
        gtk_entry_set_width_chars(GTK_ENTRY(b->labelWidget), strlen(arg));
    }

    gtk_entry_set_text(GTK_ENTRY(b->labelWidget), wlibConvertInput(arg));
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

wStatus_p wStatusCreate(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* labelStr,
    wPos_t	width,
    const char	*message)
{
    wStatus_p b;
    GtkRequisition requisition;
    b = (wStatus_p)wlibAlloc(parent, B_STATUS, x, y, NULL, sizeof *b, NULL);
    wlibComputePos((wControl_p)b);
    b->message = message;
    b->labelWidth = width;
    b->labelWidget = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(b->labelWidget), FALSE);
    gtk_entry_set_has_frame(GTK_ENTRY(b->labelWidget), FALSE);
    gtk_widget_set_can_focus(b->labelWidget, FALSE);
    gtk_entry_set_text(GTK_ENTRY(b->labelWidget),
                       message?wlibConvertInput(message):"");

    b->widget = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(b->widget), b->labelWidget);
    wlibControlGetSize((wControl_p)b);
    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    gtk_widget_show(b->widget);
    gtk_widget_show(b->labelWidget);
    wlibAddButton((wControl_p)b);

    return b;
}

/**
 * Get the anticipated length of a message field
 *
 * \param testString IN string that should fit into the message box
 * \return expected width of message box
 */

wPos_t
wStatusGetWidth(const char *testString)
{
    GtkWidget *entry;
    GtkRequisition min_req, nat_req;

    entry = gtk_entry_new();
    g_object_ref_sink(entry);

    gtk_entry_set_has_frame(GTK_ENTRY(entry), FALSE);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), strlen(testString));
    gtk_entry_set_max_length(GTK_ENTRY(entry), strlen(testString));

    gtk_widget_get_preferred_size(entry, &min_req, &nat_req);

    gtk_widget_destroy(entry);
    g_object_unref(entry);

    return (nat_req.width+8);
}

/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */

wPos_t wStatusGetHeight(
    long flags)
{
    GtkWidget * temp;

    if (!(flags&COMBOBOX)) {
		temp = gtk_entry_new();	 //To get size of text itself
        gtk_entry_set_has_frame(GTK_ENTRY(temp), FALSE);
    } else {
        temp = gtk_combo_box_text_new();    //to get max size of an object in infoBar
    }
    g_object_ref_sink(temp);

    if (wMessageSetFont(flags))	{
    	if (flags & BM_LARGE) {
    		gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(temp), "largeLabel");
    	} else {
    		gtk_widget_class_set_css_name(GTK_WIDGET_CLASS(temp), "smallLabel");
    	}
    }

    if (flags&1L) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(temp),"Test");
    }

    GtkRequisition temp_min_req, temp_nat_req;
    gtk_widget_get_preferred_size(temp,&temp_min_req, &temp_nat_req);
    //g_object_ref_sink(temp);
    //g_object_unref(temp);
    gtk_widget_destroy(temp);
    return temp_nat_req.height;
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wStatusSetWidth(
    wStatus_p b,
    wPos_t width)
{
    b->labelWidth = width;
    gtk_widget_set_size_request(b->widget, width, -1);
}
