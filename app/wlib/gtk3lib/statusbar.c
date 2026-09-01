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
 * \param[in] parent Handle of parent window
 * \param[in] labelStr ???
 * \param[in] message message to display ( null terminated )
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
 * Set height of message text
 *
 * \todo Are text properties actually used? remove parameter flags if not
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

void wStatusSetVisibleControlSet(wControl_p mainWindow,
                                 const char* controlsName)
{
	GtkStack* stack;

	stack = GTK_STACK(wlibWidgetFromIdWarn(mainWindow, "stack"));

	if (g_ascii_strcasecmp(controlsName, gtk_stack_get_visible_child_name(stack))) {
		gtk_stack_set_visible_child_name(stack, controlsName);
	}
}
