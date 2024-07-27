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


/**
 * Set the string value in a string entry field
 *
 * \param b 	IN widget to be updated
 * \param arg 	IN new string value
 * \return
 */

void wEntrySetValue(
        wControl_p b,
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
        wControl_p b,
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
        wControl_p b)
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

static gboolean entryActivated(
        GtkEntry *widget,
        wControl_p b)
{
	const char *s;
	const char * output = "\n";
	struct entry* entry = NULL;

	if ( !b ) {
		return( FALSE );
	}

	entry = WLIB_GET_DATA_PTR(b, entry);

	s = wEntryGetValue(b);

	if (entry->valueP) {
		strcpy(entry->valueP, s);
	}

	if (entry->action) {
		//b->enter_pressed = TRUE;
		entry->action( output, b->context);
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
wlibEntrySetValid(wControl_p entry, bool valid)
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

static int entryFocusOutEvent(
        GtkEntry *widget,
        GdkEvent * event,
        wControl_p b)
{
	struct entry* entry = WLIB_GET_DATA_PTR(b, entry);
	bool isOK = TRUE;

	if (entry->action) {
		const char *s;
		s = gtk_entry_get_text(GTK_ENTRY(b->widget));

		isOK = entry->action(s, b->context);
		wlibEntrySetValid(b, isOK);
	}
	if (isOK && entry->valueP) {
		g_strlcpy(entry->valueP, wEntryGetValue(b), entry->valueL);
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
 * : set entry field to read only. This flag takes priority over the property
 * set in a builder file.
 *
 * \param 	parent	IN	parent widget
 * \param 	x		IN	x position
 * \param 	y		IN	y position
 * \param 	helpStr	IN	help anchor
 * \param 	labelStr IN label
 * \param	option	IN	option
 * \param	width	IN	width of entry field
 * \param	valueP	IN	initial value
 * \param	fieldLength	IN 	maximum length of entry in chars
 * \param	action	IN	application callback function
 * \param 	data	IN	application context data
 * \return  the created widget
 */

wControl_p wEntryCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char 	 *helpStr,
        const char	 *labelStr,
        long	option,
        wWinPix_t	width,
        char	*valueP,
        wIndex_t fieldLength,
        wEntryCallBack_p action,
        void 	*data)
{
	wControl_p b;
	struct entry* entry;

	// create and initialize the widget
	b = wlibControlNew(B_TEXT, parent, helpStr, data);
	entry = WLIB_GET_DATA_PTR(b, entry);
	entry->valueP = valueP;
	entry->action = action;
	entry->valueL = fieldLength;

	if (HASDIALOGBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
 	} else {
		// create the gtk entry field and set maximum length if desired
		b->widget = (GtkWidget*)gtk_entry_new();
		if (b->widget == NULL) { abort(); }

		if (fieldLength) {
			gtk_entry_set_max_length(GTK_ENTRY(b->widget), fieldLength);
		}

		// set minimum size for widget
		if (width) {
			gtk_entry_set_width_chars(GTK_ENTRY(b->widget), width);
		}
		// if desired, place a label in front of the created widget
		if (labelStr)
			wlibAddLabel((wControl_p)b, x-1, y, labelStr);

		wlibBasicGridAttach(parent, b->widget, x, y, 1, 1);
		// show
		gtk_widget_show(b->widget);
	}
	// link into help
	wlibAddHelpString(b->widget, helpStr);
  	wlibAddTooltip(b->widget, parent->name, helpStr);

	g_signal_connect(G_OBJECT(b->widget), "focus-out-event",
	                 G_CALLBACK(entryFocusOutEvent), b);

	gtk_widget_add_events(b->widget, GDK_FOCUS_CHANGE_MASK);

	if (option & BO_READONLY) {
		gtk_editable_set_editable(GTK_EDITABLE(b->widget), FALSE);
	}
	// set the default text
	if (entry->valueP) {
		wEntrySetValue(b, entry->valueP);
	}

	return b;
}
