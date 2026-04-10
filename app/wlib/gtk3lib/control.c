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

#define  GTKCONTROLHILITEWIDTH (4)


wControl_p
wlibControlNew(wType_e type, wControl_p parent, const char *name, void* context)
{
	wControl_p newControl = g_malloc0(sizeof(struct control));

	newControl->type = type;
	newControl->context = context;
	newControl->parent = parent;
	newControl->name = name;

	return(newControl);
}
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
 * Return the active (GTK: sensitive) state of the widget.
 *
 * \param control
 * \return TRUE for active, FALSE is inactive
 */
wBool_t
wControlGetActive(wControl_p control)
{
	GtkStateFlags state = gtk_widget_get_state_flags(control->widget);

	return(!(state & GTK_STATE_FLAG_INSENSITIVE));
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
	g_assert(b->widget != NULL);

	if (b->type == B_LIST ) {

		gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(b->widget)), active);
		gtk_combo_box_set_button_sensitivity(GTK_COMBO_BOX(b->widget),
		                                     active?GTK_SENSITIVITY_ON:GTK_SENSITIVITY_OFF);

	} else {

		gtk_widget_set_sensitive(GTK_WIDGET(b->widget), active);

	}
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

wWinPix_t wLabelWidth(
        const char * label)
{
	printf("Not implemented wLabelWidth %s %d\n", __FILE__, __LINE__);
	//GtkWidget * widget;
	//GtkRequisition requisition;
	//widget = gtk_label_new(wlibConvertInput(label));
	//gtk_widget_get_preferred_size(widget, NULL, &requisition);
	//g_object_ref_sink (widget);
	//gtk_widget_destroy(widget);
	//g_object_unref(widget);
	//return requisition.width+8;
	return 0;
}

/**
 * Get width of a control
 *
 * \param b IN Control
 * \returns width
 */

wWinPix_t wControlGetWidth(
        wControl_p b)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation(b->widget, &allocation);

	return allocation.width;
}

/**
 * Get height of a control
 *
 * \param b IN Control
 * \returns height
 */

wWinPix_t wControlGetHeight(
        wControl_p b)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation(b->widget, &allocation);

	return allocation.height;
}

/**
 * Get x position of a control
 *
 * \param b IN Control
 * \returns position
 */

wWinPix_t wControlGetPosX(
        wControl_p b)		/* Control */
{
	gint x;
	gint y;
	gtk_window_get_position(GTK_WINDOW(b->widget), &x, &y);
	return x;
}

/**
 * Get y position of a control
 *
 * \param b IN Control
 * \returns position
 */

wWinPix_t wControlGetPosY(
        wControl_p b)		/* Control */
{
	gint x;
	gint y;
	gtk_window_get_position(GTK_WINDOW(b->widget), &x, &y);
	return y;
}


/**
 * Get y position of a control
 *
 * \param b IN Control
 * \returns position
 */

// Adjust vertical size
// Probably to account for some dialog components
// TODO - compute this dynamically
int wControlHeightAdjustment = 68;

void wControlGetPos(
        wControl_p b,		/* Control */
        wWinPix_t *px,
        wWinPix_t *py)
{
	*px = *py = 0;
	if ( b->parent == NULL ) { return;}

	// get position of parent and child
	GtkAllocation allocationParent;
	gtk_widget_get_allocation(b->parent->widget, &allocationParent);

	GtkAllocation allocationChild;
	gtk_widget_get_allocation(b->widget, &allocationChild);

	// Flip vertical
	allocationChild.y = allocationParent.height - allocationChild.y;

	// find center of widget
	*px = allocationChild.x + allocationChild.width/2;
	*py = allocationChild.y + allocationChild.height/2 - wControlHeightAdjustment;;
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
        wWinPix_t x,
        wWinPix_t y)
{
	printf("Not implemented wControlSetPos %s %d\n", __FILE__, __LINE__);
	//b->realX = x;
	//b->realY = y + BORDERSIZE + ((b->parent->option&F_MENUBAR)?b->parent->menu_height:0);

	//if (b->widget) {
	//    gtk_fixed_move(GTK_FIXED(b->parent->widget), b->widget, b->realX, b->realY);
	//}

	//if (b->label) {
	//	GtkRequisition requisition, reqwidget;
	//	gtk_widget_get_preferred_size(b->label, NULL, &requisition);
	//	if (b->widget)
	//	   	gtk_widget_get_preferred_size(b->widget, NULL, &reqwidget);
	//	else
	//	  	reqwidget.height = requisition.height;
	//    gtk_fixed_move(GTK_FIXED(b->parent->widget), b->label, b->realX-b->labelW,
	//                   b->realY+(reqwidget.height/2 - requisition.height/2));
	//}
}


/**
 * Set an integer as object data
 */

void
wControlSetObjectInt(wControl_p control, const char *key, int value )
{
	g_assert(control != NULL);
	g_assert(key != NULL && *key != '\0');

	g_object_set_data(G_OBJECT(control->widget), key, GINT_TO_POINTER(value));
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
	//GtkRequisition requisition,reqwidget;

	if (b->label) {
		gtk_label_set_text(GTK_LABEL(b->label), wlibConvertInput(labelStr));
		//gtk_widget_get_preferred_size(b->label, NULL, &requisition);
		//if (b->widget)
		//	gtk_widget_get_preferred_size(b->widget, NULL, &reqwidget);
		//else
		//	reqwidget.height = requisition.height;
		//b->labelW = requisition.width+8;
		//gtk_fixed_move(GTK_FIXED(b->parent->widget), b->label, b->realX-b->labelW,
		//               b->realY+(reqwidget.height/2 - requisition.height/2));
	} else {
		printf("Invalid call to wControlSetLabel()\n");
//        b->label = wlibAddLabel(b, labelStr);
	}
}

/**
 * Set the context ie. additional user attributes for a control
 *
 * \param b IN control
 * \param context IN user date
 */

void wControlSetContext(
        wControl_p b,
        void * context)
{
	b->context = context;
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

wBool_t wControlExpose (
        GtkWidget * widget,
        cairo_t * cr,
        wControl_p b
)
{
	printf("Not implemented wControlExpose %s %d\n", __FILE__, __LINE__);
	//GdkWindow * win = gtk_widget_get_window(b->widget);
//   int width = gtk_widget_get_allocated_width ( widget );
//   int height = gtk_widget_get_allocated_height ( widget );

	//if (b->outline) {
	//	cairo_set_source_rgb(cr, 0.23, 0.37, 0.80);
	//	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	//	cairo_set_line_width(cr, GTKCONTROLHILITEWIDTH);
	//	cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
	//	cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
	//	cairo_rectangle(cr, 2, 2, width - 2, height - 2 );
	//	cairo_stroke(cr);
	//}


	//cairo_destroy(cr);


	return FALSE;
}

/**
 * Highlight a control by giving it a colored frame.
 * \param b IN the control
 * \param hilite IN TRUE for highlighting on, FALSE off
 * \returns
 */

#define HILITECLASS "hilite"

void wControlHilite(
        wControl_p control,
        wBool_t hilite)
{
	if ( control == NULL ) { return; }
	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(
	                                   control->widget));
	if (hilite) {
		gtk_style_context_add_class(context, HILITECLASS);
	} else {
		gtk_style_context_remove_class(context, HILITECLASS);
	}
}

void wControlSetCustomTooltip(wControl_p control, char* tooltip)
{
	g_assert(control);

	control->customTooltip = tooltip;
}
