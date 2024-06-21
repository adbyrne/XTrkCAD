/**
 * \file   entry.c
 * \brief  Single line entry widget
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <glib-object.h>

#include "gtkint.h"

/*
 *****************************************************************************
 *
 * Text Boxes
 *
 *****************************************************************************
 */

struct wEntry_t {
	wType_e	type;				//< type
	GtkWidget* widget;			//< the entry widget
	void* data;					//< context data
	char* valueP;				//< location of entered value
	unsigned valueL;			//< maximum length of entered value
	wEntryCallBack_p action;	//< callback
};

/**
 * Set the string value in a string entry field
 *
 * \param b 	IN widget to be updated
 * \param arg 	IN new string value
 * \return
 */

void wEntrySetValue(
        wEntry_p b,
        const char *arg)
{
	if (b->widget == NULL) {
		abort();
	}

	// the contents should not be changed programatically while
	// the user is editing it
	// TODO: find replacement  for signal_block
	if( !(gtk_widget_has_focus(b->widget))) {

		gtk_entry_set_text(GTK_ENTRY(b->widget), arg);

	}
}

/**
 * Set the width of the entry field
 *
 * \param b 	IN widget to be updated
 * \param w 	IN new width for w chars
 * \return
 */

void wEntrySetWidth(
        wEntry_p b,
        wWinPix_t w)
{
	gtk_entry_set_width_chars(GTK_ENTRY(b->widget), w);
}

/**
 * Return the entered value
 *
 * \param b IN entry field
 * \return   the entered text
 */

const char *wEntryGetValue(
        wEntry_p b)
{
	if ( !b->widget ) {
		abort();
	}

	return gtk_entry_get_text(GTK_ENTRY(b->widget));
}

/**
 * Signal handler for 'activate' signal: enter pressed - callback with the
 * current value and then select the whole default value
 *
 * \param widget 	IN the edit field
 * \param b 		IN the widget data structure
 * \return
 *
 * \todo Check necessity probably used by BO_ENTER
 */

static gboolean stringActivated(
        GtkEntry *widget,
        wEntry_p b)
{
	const char *s;
	const char * output = "\n";

	if ( !b ) {
		return( FALSE );
	}

	s = wEntryGetValue(b);

	if (b->valueP) {
		strcpy(b->valueP, s);
	}

	if (b->action) {
		//b->enter_pressed = TRUE;
		b->action( output, b->data);
	}

	// select the complete default value to make editing it easier
	gtk_editable_select_region( GTK_EDITABLE( widget ), 0, -1 );
	return( TRUE );
}

/**
 * Visually set the entry field to show whether the valus entered is valid.
 *
 * \param entry	IN	entry field
 * \param valid	IN	true = no error indication, false indicate error (see CSS)
 */

static void
wlibEntrySetValid(wEntry_p entry, bool valid)
{
	GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(
	                                   entry->widget));
	if (valid) {
		gtk_style_context_remove_class(context, "error");
	} else {
		gtk_style_context_add_class(context, "error");
	}
}

/**
 * Signal handler for changes in an entry field
 *
 * \param widget 		IN
 * \param entry field 	IN
 * \return
 */

static int stringFocusOutEvent(
        GtkEntry *widget,
        GdkEvent * event,
        wEntry_p b)
{
	if (b->action) {
		const char *s;
		s = gtk_entry_get_text(GTK_ENTRY(b->widget));

		bool isOK = b->action(s, b->data);
		wlibEntrySetValid(b, isOK);
	}
	if (b->valueP) {
		g_strlcpy(b->valueP, wEntryGetValue(b), b->valueL);
	}
	return FALSE;
}

/**
 * Create a single line entry field for a string value
 *
 * ### Usage in dialogs, created by
 *
 * - runtime: yes
 * - builder: yes
 *
 * ### Options
 * BO_READONLY
 * : set entry field to read only
 *
 * \param 	parent	IN	parent widget
 * \param 	x		IN	x position
 * \param 	y		IN	y position
 * \param 	helpStr	IN	help anchor
 * \param 	labelStr IN label
 * \param	option	IN	option
 * \param	width	IN	width of entry field
 * \param	valueP	IN	initial value
 * \param	valueL	IN 	maximum length of entry in chars
 * \param	action	IN	application callback function
 * \param 	data	IN	application context data
 * \return  the created widget
 */

wEntry_p wEntryCreate(
        wWindow_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char 	 *helpStr,
        const char	 *labelStr,
        long	option,
        wWinPix_t	width,
        char	*valueP,
        wIndex_t valueL,
        wEntryCallBack_p action,
        void 	*data)
{
	wEntry_p b;

	// create and initialize the widget

	b = g_malloc0(sizeof(struct wEntry_t));
	b->type = B_TEXT;
	b->valueP = valueP;
	b->action = action;
	b->valueL = valueL;

	if (parent->builder) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
 	} else {
		// create the gtk entry field and set maximum length if desired
		b->widget = (GtkWidget*)gtk_entry_new();
		if (b->widget == NULL) { abort(); }

		if (valueL) {
			gtk_entry_set_max_length(GTK_ENTRY(b->widget), valueL);
		}

		// set minimum size for widget
		if (width) {
			gtk_entry_set_width_chars(GTK_ENTRY(b->widget), width);
		}
		// if desired, place a label in front of the created widget
		//if (labelStr)
		//	b->labelW = wlibAddLabel((wControl_p)b, labelStr);

		if (option & BO_READONLY) {
			gtk_editable_set_editable(GTK_EDITABLE(b->widget), FALSE);
		}

		wlibBasicGridAttach(parent, b->widget, x, y, 1, 1);
		// show
		gtk_widget_show(b->widget);
	}
	// link into help
	wlibAddHelpString(b->widget, helpStr);
 	wlibAddTooltip(b->widget, parent->name, helpStr);

	g_signal_connect(G_OBJECT(b->widget), "focus-out-event",
	                 G_CALLBACK(stringFocusOutEvent), b);

	gtk_widget_add_events(b->widget, GDK_FOCUS_CHANGE_MASK);

	// set the default text
	if (b->valueP) {
		wEntrySetValue(b, b->valueP);
	}

	return b;
}
