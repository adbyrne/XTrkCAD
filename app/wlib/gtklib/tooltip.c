/** \file tooltip.c
 * Code for tooltip / balloon help functions
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
static int enableBalloonHelp = TRUE;

static GtkWidget * balloonF; 	/**< balloon help control for hotbar item */
static GtkWidget * balloonPI;

static const char * balloonMsg = NULL;
static wControl_p balloonB;
static wPos_t balloonDx, balloonDy;
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
    balloonHelpStrings = bh;
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

void wControlSetBalloon( wControl_p b, wPos_t dx, wPos_t dy, const char * msg )
{
    PangoLayout * layout;

    wPos_t x, y;
    wPos_t w, h;
    wPos_t xx, yy;
    const char * msgConverted;
    GtkAllocation size;

    /* return if there is nothing to do */
    if (balloonVisible && balloonB == b &&
            balloonDx == dx && balloonDy == dy && msg != NULL && balloonMsg != NULL)
           if (strcmp(msg,balloonMsg)==0)
      		return;

    /* hide the tooltip */
    if ( msg == NULL ) {
        if ( balloonF != NULL ) {
            gtk_widget_hide( balloonF );
            balloonVisible = FALSE;
        }
        balloonMsg = "";
        return;
    }
    msgConverted = wlibConvertInput(msg);

    if ( balloonF == NULL ) {
		GtkWidget *alignment;
		
        balloonF = gtk_window_new( GTK_WINDOW_TOPLEVEL );
        gtk_window_set_type_hint( GTK_WINDOW( balloonF), GDK_WINDOW_TYPE_HINT_TOOLTIP );
        gtk_window_set_decorated (GTK_WINDOW (balloonF), FALSE );
        gtk_window_set_resizable( GTK_WINDOW (balloonF), FALSE );
            
		alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
		gtk_alignment_set_padding( GTK_ALIGNMENT(alignment), 6, 6, 6, 6 );
		
		gtk_container_add (GTK_CONTAINER (balloonF), alignment);
		gtk_widget_show (alignment);
        
        balloonPI = gtk_label_new(msgConverted);
        gtk_container_add( GTK_CONTAINER(alignment), balloonPI );
        gtk_widget_show( balloonPI );
    }
    gtk_label_set_text( GTK_LABEL(balloonPI), msgConverted );

    balloonDx = dx;
    balloonDy = dy;
    balloonB = b;
    balloonMsg = msg;
    gtk_widget_get_allocation(balloonPI, &size );
    w = size.width;
    h = size.height;

    gdk_window_get_origin( gtk_widget_get_window( GTK_WIDGET(b->parent->gtkwin)), &x, &y );

    x += b->realX + dx;
    y += b->realY + b->h - dy;
    xx = gdk_screen_width();
    yy = gdk_screen_height();
    if ( x < 0 ) {
        x = 0;
    } else if ( x+w > xx ) {
        x = xx - w;
    }
    if ( y < 0 ) {
        y = 0;
    } else if ( y+h > yy ) {
        y = yy - h ;
    }
    gtk_window_move( GTK_WINDOW( balloonF ), x, y );
    gtk_widget_show( balloonF );

    balloonVisible = TRUE;
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
