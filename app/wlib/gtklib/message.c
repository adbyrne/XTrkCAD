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

struct wMessage_t {
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

void wMessageSetValue(
    wMessage_p b,
    const char * arg)
{
    if (b->widget == 0) {
        abort();
    }

    gtk_label_set_text(GTK_LABEL(b->labelWidget), wlibConvertInput(arg));
}

/**
 * Set the width of the widget
 *
 * \param b IN widget
 * \param width IN  new width
 * \return
 */

void wMessageSetWidth(
    wMessage_p b,
    wPos_t width)
{
    b->labelWidth = width;
    gtk_widget_set_size_request(b->widget, width, -1);
}

/**
 * Get height of message text
 *
 * \param flags IN text properties (large or small size)
 * \return text height
 */

wPos_t wMessageGetHeight(
    long flags)
{
	GtkWidget * temp = gtk_combo_box_text_new();   //to get max size of an object in infoBar
	if (wMessageSetFont(flags))	{
		GtkStyle *style;
		PangoFontDescription *fontDesc;
		int fontSize;
	    /* get the current font descriptor */
	   style = gtk_widget_get_style(temp);
	   fontDesc = style->font_desc;
	   /* get the current font size */
	   fontSize = PANGO_PIXELS(pango_font_description_get_size(fontDesc));

	   /* calculate the new font size */
	   if (flags & BM_LARGE) {
	       pango_font_description_set_size(fontDesc, fontSize * 1.4 * PANGO_SCALE);
	   } else {
	       pango_font_description_set_size(fontDesc, fontSize * 0.7 * PANGO_SCALE);
	   }

	   /* set the new font size */
	   gtk_widget_modify_font(temp, fontDesc);
	}
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(temp),"Test");
	GtkRequisition temp_requisition;
	gtk_widget_size_request(temp,&temp_requisition);
	gtk_widget_destroy(temp);
    return temp_requisition.height;
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

 wMessage_p wMessageCreateEx(
    wWin_p	parent,
    wPos_t	x,
    wPos_t	y,
    const char 	* labelStr,
    wPos_t	width,
    const char	*message,
    long flags)
{
    wMessage_p b;
    GtkRequisition requisition;
    GtkStyle *style;
    PangoFontDescription *fontDesc;
    int fontSize;
    b = (wMessage_p)wlibAlloc(parent, B_MESSAGE, x, y, NULL, sizeof *b, NULL);
    wlibComputePos((wControl_p)b);
    b->message = message;
    b->labelWidth = width;
    b->labelWidget = gtk_label_new(message?wlibConvertInput(message):"");

    /* do we need to set a special font? */
    if (wMessageSetFont(flags))	{
        /* get the current font descriptor */
        style = gtk_widget_get_style(GTK_WIDGET(b->labelWidget));
        fontDesc = style->font_desc;
        /* get the current font size */
        fontSize = PANGO_PIXELS(pango_font_description_get_size(fontDesc));

        /* calculate the new font size */
        if (flags & BM_LARGE) {
            pango_font_description_set_size(fontDesc, fontSize * 1.4 * PANGO_SCALE);
        } else {
            pango_font_description_set_size(fontDesc, fontSize * 0.7 * PANGO_SCALE);
        }

        /* set the new font size */
        gtk_widget_modify_font((GtkWidget *)b->labelWidget, fontDesc);
    }

    b->widget = gtk_fixed_new();
    gtk_widget_size_request(GTK_WIDGET(b->labelWidget), &requisition);
    gtk_container_add(GTK_CONTAINER(b->widget), b->labelWidget);
    gtk_widget_set_size_request(b->widget, width?width:requisition.width,
                                requisition.height);
    wlibControlGetSize((wControl_p)b);
    gtk_fixed_put(GTK_FIXED(parent->widget), b->widget, b->realX, b->realY);
    gtk_widget_show(b->widget);
    gtk_widget_show(b->labelWidget);
    wlibAddButton((wControl_p)b);

    /* Reset font size to normal */
    if (wMessageSetFont(flags))	{
        pango_font_description_set_size(fontDesc, fontSize * PANGO_SCALE);
    }

    return b;
}
