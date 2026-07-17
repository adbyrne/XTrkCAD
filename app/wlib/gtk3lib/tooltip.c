/** \file tooltip.c
 * Code for tooltip help functions
 *
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C)  2012 Martin Fischer
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
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

wBool_t listHelpStrings = FALSE;
wBool_t listMissingHelpStrings = FALSE;

static wTooltip_t * tooltipsTexts;
static int tooltipsCount;

/**
 * Initialize tooltip array
 *
 * \param bh IN pointer to list of tooltips
 * \param count IN number of tooltips
 */

void wInitTooltip( wTooltip_t * bh, unsigned int count )
{
	tooltipsTexts = bh;
	tooltipsCount = count;
}

/**
 * Set the help topic string for \a b to \a help.
 *
 * \param b IN control
 * \param newTip IN tip
 */

void wTooltipSetText(
        wControl_p b,
        const char * newTip )
{
	g_assert(b->widget != NULL);

	gtk_widget_set_tooltip_text( b->widget, newTip );
}

/**
 * A recursive binary search function. It returns location of x in
 * given array arr[l..r] is present, otherwise -1
 * Taken from http://www.geeksforgeeks.org/binary-search/ and modified
 *
 * \param arr IN array to search
 * \param l IN starting index
 * \param r IN highest index in array
 * \param key IN key to search
 * \return index if found, -1 otherwise
 */

static int binarySearch(wTooltip_t arr[], int l, int r, const char* key)
{
	if (r >= l) {
		int mid = l + (r - l) / 2;
		int res = strcmp(key, arr[mid].name);

		// If the element is present at the middle itself
		if (!res) {
			return mid;
		}

		// If the array size is 1
		if (r == 0) {
			return -1;
		}

		// If element is smaller than mid, then it can only be present
		// in left subarray
		if (res < 0) {
			return binarySearch(arr, l, mid - 1, key);
		}

		// Else the element can only be present in right subarray
		return binarySearch(arr, mid + 1, r, key);
	}

	// We reach here when element is not present in array
	return -1;
}

/**
 * Search for the widgets tooltip in the list of tooltips. Performs a binary
 * search on the array.
 *
 * \param	field	IN the id of the widget, usually \<dialogname\>-\<widgetname\>
 * \return	index of the tooltip
 */

static const char*
FindTooltip(const char* field)
{
	int res;

	if (!tooltipsCount) {
		return(NULL);
	}

	res = binarySearch(tooltipsTexts, 0,
	                   tooltipsCount,
	                   field);

	if (res == -1) {
		return(NULL);
	} else {
		return(tooltipsTexts[res].value);
	}
}


void wTooltipSet(wControl_p control, const char *dialog, const char* dialogItem)
{
	g_assert(control != NULL);
	g_assert(dialogItem != NULL);

	wlibAddTooltip(control->widget, dialog, dialogItem);
}

/**
 * Add tooltip to a widget.
 *
 * \param widget		IN widget
 * \param dialog		IN name of dialog
 * \param dialogItem	IN name of item within dialog
 */

void wlibAddTooltip(
        GtkWidget * widget,
        const char *dialog,
        const char * dialogItem )
{
	const char *tooltipText;
	const char* searchKey;
	GString *tooltip = g_string_new(NULL);

	if (dialogItem==NULL || *dialogItem==0) {
		return;
	}
	if ( tooltipsTexts == NULL ) {
		return;
	}

	if (dialog) {
		g_string_printf(tooltip, "%s-%s", dialog, dialogItem);
		searchKey = tooltip->str;
	} else {
		searchKey = dialogItem;
	}

	// search for the helpStr, tooltipText points to the entry when found
	tooltipText = FindTooltip(searchKey);

	if (listMissingHelpStrings && !tooltipText) {
		printf( "Missing Help String: %s\n", searchKey );
		g_string_free(tooltip, TRUE);
		return;
	}
	if(tooltipText) {
		gtk_widget_set_tooltip_text( widget, wlibConvertInput(_(tooltipText)) );
	}

	g_string_free(tooltip, TRUE);
}

