/** \file control.c
 * Control Utilities
 */
/* 
 * Copyright 2016 Martin Fischer <m_fischer@sf.net>
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

#include <stdio.h>
#include <stdlib.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

#define  GTKCONTROLHILITEWIDTH (3)

/**
 * Cause the control <b> to be displayed or hidden.
 * Used to hide control (such as a list) while it is being updated.
 *
 * \param b IN Control
 * \param show IN TRUE for visible
 */

void wControlShow(
    wControl_p b,
    wBool_t show)
{
    if (b->type == B_LINES) {
        wlibLineShow((wLine_p)b, show);
        return;
    }

    if (b->widget == NULL) {
        abort();
    }

    if (show) {
        gtk_widget_show(b->widget);

        if (b->label) {
            gtk_widget_show(b->label);
        }
    } else {
        gtk_widget_hide(b->widget);

        if (b->label) {
            gtk_widget_hide(b->label);
        }
    }
}

/**
 * Cause the control <b> to be marked active or inactive.
 * Inactive controls donot respond to actions.
 *
 * \param b IN Control
 * \param active IN TRUE for active
 */

void wControlActive(
    wControl_p b,
    int active)
{
    if (b->widget == NULL) {
        abort();
    }

    gtk_widget_set_sensitive(GTK_WIDGET(b->widget), active);
}

/**
 * Returns the width of <label>.
 * This is used for computing window layout.
 * Typically the width to the longest label is computed and used as
 * the X-position for <controls>.
 *
 * \param label IN label
 * \returns width of label including some space
*/

wPos_t wLabelWidth(
    const char * label)
{
    GtkWidget * widget;
    GtkRequisition requisition;
    widget = gtk_label_new(wlibConvertInput(label));
    gtk_widget_size_request(widget, &requisition);
    gtk_widget_destroy(widget);
    return requisition.width+8;
}

/**
 * Get width of a control
 *
 * \param b IN Control
 * \returns width
 */

wPos_t wControlGetWidth(
    wControl_p b)
{
    return b->w;
}

/**
 * Get height of a control
 *
 * \param b IN Control
 * \returns height
 */

wPos_t wControlGetHeight(
    wControl_p b)
{
    return b->h;
}

/**
 * Get x position of a control
 *
 * \param b IN Control
 * \returns position
 */

wPos_t wControlGetPosX(
    wControl_p b)		/* Control */
{
    return b->realX;
}

/**
 * Get y position of a control
 *
 * \param b IN Control
 * \returns position
 */

wPos_t wControlGetPosY(
    wControl_p b)		/* Control */
{
    return b->realY - BORDERSIZE - ((b->parent->option&F_MENUBAR)?b->parent->menu_height:0);
}

/**
 * Set the fixed position of a control within its parent window
 *
 * \param b IN control
 * \param x IN x position
 * \param y IN y position
 */

void wControlSetPos(
    wControl_p b,
    wPos_t x,
    wPos_t y)
{
    b->realX = x;
    b->realY = y + BORDERSIZE + ((b->parent->option&F_MENUBAR)?b->parent->menu_height:0);

    if (b->widget) {
        gtk_fixed_move(GTK_FIXED(b->parent->widget), b->widget, b->realX, b->realY);
    }

    if (b->label) {
    	GtkRequisition requisition, reqwidget;
    	gtk_widget_size_request(b->label, &requisition);
    	if (b->widget)
    	   	gtk_widget_size_request(b->widget, &reqwidget);
    	else
    	  	reqwidget.height = requisition.height;
        gtk_fixed_move(GTK_FIXED(b->parent->widget), b->label, b->realX-b->labelW,
                       b->realY+(reqwidget.height/2 - requisition.height/2));
    }
}

/**
 * Set the label for a control
 *
 * \param b IN control
 * \param labelStr IN the new label
 */

void wControlSetLabel(
    wControl_p b,
    const char * labelStr)
{
    GtkRequisition requisition,reqwidget;

    if (b->label) {
        gtk_label_set_text(GTK_LABEL(b->label), wlibConvertInput(labelStr));
        gtk_widget_size_request(b->label, &requisition);
        if (b->widget)
        	gtk_widget_size_request(b->widget, &reqwidget);
        else
        	reqwidget.height = requisition.height;
        b->labelW = requisition.width+8;
        gtk_fixed_move(GTK_FIXED(b->parent->widget), b->label, b->realX-b->labelW,
                       b->realY+(reqwidget.height/2 - requisition.height/2));
    } else {
        b->labelW = wlibAddLabel(b, labelStr);
    }
}

/**
 * Set the context ie. additional user data for a control
 *
 * \param b IN control
 * \param context IN user date
 */

void wControlSetContext(
    wControl_p b,
    void * context)
{
    b->data = context;
}

/**
 * Not implemented
 *
 * \param b IN
 */

void wControlSetFocus(
    wControl_p b)
{
}

/**
 * Draw a rectangle around a control
 * \param b IN the control
 * \param hilite IN unused
 * \returns
 *
 *
 */
void wControlHilite(
    wControl_p b,
    wBool_t hilite)
{
    cairo_t *cr;
    int off = GTKCONTROLHILITEWIDTH/2+1;

    if (b->widget == NULL) {
        return;
    }

    if (! gtk_widget_get_visible(b->widget)) {
        return;
    }

    if (! gtk_widget_get_visible(b->parent->widget)) {
        return;
    }

    cr = gdk_cairo_create(gtk_widget_get_window(b->parent->gtkwin));
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_operator(cr, CAIRO_OPERATOR_XOR);
    cairo_set_line_width(cr, GTKCONTROLHILITEWIDTH);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
    cairo_rectangle(cr,
                    b->realX - GTKCONTROLHILITEWIDTH,
                    b->realY - off,
                    b->w + GTKCONTROLHILITEWIDTH,
                    b->h + off + 1);
    cairo_stroke(cr);
    cairo_destroy(cr);
}
