/** \file revealer.c
 * GtkRevealer / GtkFrame helpers for builder-defined nested panels
 */
/*
 * Copyright 2026 Martin Fischer <m_fischer@sf.net>
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
 */

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

#define PANELINVALIDCLASS "panel-invalid"

/**
 * Show or hide a builder-defined GtkRevealer's child.
 *
 * \param win   IN window control the revealer was loaded from
 * \param id    IN builder id of the GtkRevealer
 * \param reveal IN TRUE to reveal, FALSE to hide
 */

void
wRevealerShow(wControl_p win, const char *id, wBool_t reveal)
{
	GtkWidget *widget = wlibWidgetFromIdWarn(win, id);
	if (!widget) { return; }
	gtk_revealer_set_reveal_child(GTK_REVEALER(widget), reveal);
}

/**
 * Flag or clear the error state of a builder-defined GtkFrame, mirroring
 * wControlHilite's "hilite" CSS class pattern.
 *
 * \param win   IN window control the frame was loaded from
 * \param id    IN builder id of the GtkFrame
 * \param error IN TRUE to flag, FALSE to clear
 */

void
wFrameSetError(wControl_p win, const char *id, wBool_t error)
{
	GtkWidget *widget = wlibWidgetFromIdWarn(win, id);
	GtkStyleContext *context;
	if (!widget) { return; }
	context = gtk_widget_get_style_context(widget);
	if (error) {
		gtk_style_context_add_class(context, PANELINVALIDCLASS);
	} else {
		gtk_style_context_remove_class(context, PANELINVALIDCLASS);
	}
}

/**
 * Toggle a builder-defined GtkFrame's border/shadow between drawn and flat --
 * used to make the frame look like a plain, unwrapped block when it isn't
 * actually acting as a nested create-panel.
 *
 * \param win   IN window control the frame was loaded from
 * \param id    IN builder id of the GtkFrame
 * \param shown IN TRUE for a drawn border (GTK_SHADOW_IN), FALSE for flat
 */

void
wFrameSetShadow(wControl_p win, const char *id, wBool_t shown)
{
	GtkWidget *widget = wlibWidgetFromIdWarn(win, id);
	if (!widget) { return; }
	if (GTK_IS_FRAME(widget)) {
		gtk_frame_set_shadow_type(GTK_FRAME(widget),
		                          shown ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
	}
}

/**
 * Set the label text of a builder-defined GtkFrame or GtkLabel.
 *
 * \param win   IN window control the widget was loaded from
 * \param id    IN builder id of the GtkFrame or GtkLabel
 * \param text  IN new label text
 */

void
wFrameSetLabel(wControl_p win, const char *id, const char *text)
{
	GtkWidget *widget = wlibWidgetFromIdWarn(win, id);
	if (!widget) { return; }
	if (GTK_IS_FRAME(widget)) {
		gtk_frame_set_label(GTK_FRAME(widget), text);
	} else if (GTK_IS_LABEL(widget)) {
		gtk_label_set_text(GTK_LABEL(widget), text);
	}
}
