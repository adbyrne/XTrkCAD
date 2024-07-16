/** \file tooltip.c
 * Code for tooltip / balloon help functions
 * 
 * \todo Balloon.. should be replaced by Tooltip... for clarity
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

static wBalloonHelp_t * balloonHelpStrings;
static int tooltipsCount;

static int enableBalloonHelp = TRUE;

static GtkWidget * balloonF; 	/**< balloon help control for hotbar item */
static GtkWidget * balloonPI;

static char balloonMsg[100] = "";
static wControl_p balloonB;
static wWinPix_t balloonDx, balloonDy;
static wBool_t balloonVisible = FALSE;


/**
 * Hide the currently displayed Balloon Help.
 */
 
void
wlibHelpHideBalloon()
{
    wControlSetBalloon( balloonB, 0, 0, NULL );
}

/**
 * Initialize tooltip array
 *
 * \param bh IN pointer to list of tooltips
 * \return
 */

void wSetBalloonHelp( wBalloonHelp_t * bh )
{
	int i;
    balloonHelpStrings = bh;
	for (i = 0; balloonHelpStrings[i].name != NULL ; i++)
		;

	tooltipsCount = i;
}

/**
 * Switch tooltips on and off
 *
 * \param enable IN desired state (TRUE or FALSE )
 * \return
 */

void wEnableBalloonHelp( int enable )
{
    enableBalloonHelp = enable;
}

/**
 * Set the help topic string for <b> to <help>.
 *
 * \param b IN control
 * \param help IN tip
 */

void wControlSetHelp(
    wControl_p b,
    const char * help )
{
    wControlSetBalloonText( b, help );
}

/**
 * Set the help topic string for <b> to <help>.
 *
 * \param b IN control
 * \param help IN tip
 */

void wControlSetBalloonText(
    wControl_p b,
    const char * label )
{
    assert(b->widget != NULL);


    gtk_widget_set_tooltip_text( b->widget, label );
}

/**
 * Display some help text for a widget. This is a way to create
 * some custom tooltips for error messages and the hotbar part desc
 * \todo Replace with standard tooltip handling and custom window
 *
 * \param b IN widget
 * \param dx IN dx offset in x-direction
 * \param dy IN dy offset in y direction
 * \param msg IN text to display, if NULL tootip is hidden
 * \return
 */

void wControlSetBalloon( wControl_p b, wWinPix_t dx, wWinPix_t dy,
                         const char * msg )
{
	printf("Not implemented wControlSetBalloon() %s %d\n", __FILE__, __LINE__);
	//PangoLayout * layout;

	//gint x, y;
	//gint w, h;
	//wWinPix_t xx, yy;
	//const char * msgConverted;
	//GtkRequisition min_size, nat_size;

	///* return if there is nothing to do */
	//if (balloonVisible && balloonB == b &&
	//    balloonDx == dx && balloonDy == dy && msg != NULL && !balloonMsg[0])
	//	if (strcmp(msg,balloonMsg)==0) {
	//		return;
	//	}

	///* hide the tooltip */
	//if ( msg == NULL ) {
	//	if ( balloonF != NULL && balloonVisible) {
	//		gtk_widget_hide( balloonF );
	//		balloonVisible = FALSE;
	//	}
	//	balloonMsg[0] = '\0';
	//	return;
	//}
	//msgConverted = wlibConvertInput(msg);

	//if ( balloonF == NULL ) {
	//	//GtkWidget *alignment;

	//	balloonF = gtk_window_new( GTK_WINDOW_POPUP );
	//	gtk_window_set_type_hint( GTK_WINDOW( balloonF), GDK_WINDOW_TYPE_HINT_TOOLTIP );
	//	gtk_window_set_decorated (GTK_WINDOW (balloonF), FALSE );
	//	gtk_window_set_resizable( GTK_WINDOW (balloonF), FALSE );
	//	gtk_window_set_accept_focus(GTK_WINDOW( balloonF), FALSE);

	//	//GtkWidget * alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	//	//gtk_alignment_set_padding( GTK_ALIGNMENT(alignment), 6, 6, 6, 6 );

	//	//gtk_container_add (GTK_CONTAINER (balloonF), alignment);

	//	//gtk_widget_show (alignment);

	//	balloonPI = gtk_label_new(msgConverted);
	//	gtk_widget_set_halign(balloonPI, GTK_ALIGN_CENTER);
	//	gtk_widget_set_valign(balloonPI, GTK_ALIGN_CENTER);
	//	gtk_widget_set_hexpand(balloonPI,TRUE);
	//	gtk_widget_set_vexpand(balloonPI,TRUE);
	//	gtk_container_add( GTK_CONTAINER(balloonF), balloonPI );
	//}
	//gtk_label_set_text( GTK_LABEL(balloonPI), msgConverted );
	//gtk_widget_show_all( balloonPI );
	//gtk_widget_show_all( balloonF );

	//balloonDx = dx;
	//balloonDy = dy;
	//balloonB = b;
	//snprintf(balloonMsg, sizeof(balloonMsg), "%s", msg);
	//gtk_widget_get_preferred_size(balloonPI, &min_size, &nat_size);
	//w = nat_size.width;
	//h = nat_size.height;

	//gtk_window_get_position( GTK_WINDOW(b->parent->gtkwin), &x, &y);


	//x += b->realX + dx;
	//y += b->realY + b->h - dy;

	//GdkDisplay * display = gdk_display_get_default();

	//GdkMonitor * monitor = gdk_display_get_monitor_at_window(display,
	//                       gtk_widget_get_window(balloonF));

	//GdkRectangle rect;
	//gdk_monitor_get_geometry(monitor, &rect);

	//xx = rect.width;
	//yy = rect.height;
	//if ( x < 0 ) {
	//	x = 0;
	//} else if ( x+w > xx ) {
	//	x = xx - w;
	//}
	//if ( y < 0 ) {
	//	y = 0;
	//} else if ( y+h > yy ) {
	//	y = yy - h ;
	//}
	//gtk_window_move( GTK_WINDOW( balloonF ), x, y );
	//gtk_widget_show_all( balloonF );
	//gtk_widget_show( balloonPI );

	//balloonVisible = TRUE;
}

/**
*	still referenced but not needed any more
*/
void wBalloonHelpUpdate( void )
{
}

/**
 * Add tooltip and reference into help system to a widget
 *
 * \param widget IN widget
 * \param helpStr IN symbolic link into help system
 * 
 * \todo Remove adding tooltip text from here
 */

void wlibAddHelpString(
    GtkWidget * widget,
    const char * helpStr )
{
    char *string;
    char *wAppName = wlibGetAppName();
    wBalloonHelp_t * bhp;

    if (helpStr==NULL || *helpStr==0)
        return;
    if ( balloonHelpStrings == NULL )
        return;

    // search for the helpStr, bhp points to the entry when found
    for ( bhp = balloonHelpStrings; bhp->name && strcmp(bhp->name,helpStr) != 0; bhp++ )
        ;

    if (listMissingHelpStrings && !bhp->name) {
        printf( "Missing Help String: %s\n", helpStr );
        return;
    }

    string = malloc( strlen(wAppName) + 5 + strlen(helpStr) + 1 );
    sprintf( string, "%sHelp/%s", wAppName, helpStr );

	if(bhp->value)
		gtk_widget_set_tooltip_text( widget, wlibConvertInput(_(bhp->value)) );

    g_object_set_data( G_OBJECT( widget ), HELPDATAKEY, string );

    if (listHelpStrings)
        printf( "HELPSTR - %s\n", string );

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

static int binarySearch(wBalloonHelp_t arr[], int l, int r, char* key)
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
 * \param	field	IN the id of the widget, usually <dialogname>-<widgetname>
 * \return	index of the tooltip
 * 
 * \todo Add i18n (see commented out code line)
 */

static const char *
FindTooltip(gchar* field) {
	wBalloonHelp_t *bhp;
	int res;

	res = binarySearch(balloonHelpStrings, 0, 
		tooltipsCount, 
		field);

	if (res==-1) {
		return("No help found!");
	}
	else {

//		wlibConvertInput(_(balloonHelpStrings[res].value)));
		return(balloonHelpStrings[res].value);
	}
}

/**
 * Signal handler for tooltips. 
 * 
 * \todo check respective option for enable status of tooltips
 * 
 * \param self	
 * \param x
 * \param y
 * \param keyboard_mode
 * \param tooltip
 * \param user_data	pointer to the tooltip
 * \return 
 */

static gboolean
queryTooltipEvent(GtkWidget* self,
	gint x,
	gint y,
	gboolean keyboard_mode,
	GtkTooltip* tooltip,
	gpointer user_data)
{
	gtk_tooltip_set_text(tooltip, user_data);
	return(TRUE);
}

/**
 * Enables tooltip for the widget. The dialog name and the widget name are
 * concatenated. This string is used as search string for the list of 
 * tooltips. If found tooltip is enabled for the widget using the search 
 * result.
 * 
 * \param widget	IN	the widget
 * \param dialog	IN	name of parent dialog
 * \param field		IN	name of widget
 */

void
wlibAddTooltip(GtkWidget* widget, char* dialog, char* field)
{
	gchar* fieldId = g_strdup_printf("%s-%s", dialog, field);

	if (fieldId) {
		g_signal_connect(widget, "query-tooltip", 
			G_CALLBACK(queryTooltipEvent), FindTooltip(fieldId));
		gtk_widget_set_has_tooltip(widget, true);
	}

	g_free(fieldId);
}
