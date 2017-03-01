/** \file gtkhelp.c
 * Balloon help ( tooltips) and main help functions.
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkhelp.c,v 1.12 2009-10-02 04:30:32 dspagnol Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis and (C) 2007 Martin Fischer
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
#include <dirent.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <stdint.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"


/*
 ******************************************************************************
 *
 * Help
 *
 ******************************************************************************
 */

wBool_t listHelpStrings = FALSE;
wBool_t listMissingHelpStrings = FALSE;
static char HelpDataKey[] = "HelpDataKey";

static GtkWidget * balloonF;
static GtkWidget * balloonPI;
static wBalloonHelp_t * balloonHelpStrings;
static int enableBalloonHelp = 1;
static const char * balloonMsg;
static wControl_p balloonB;
static wPos_t balloonDx, balloonDy;
static wBool_t balloonVisible = FALSE;
static wControl_p balloonHelpB;
static GtkTooltips * tooltips;


void wSetBalloonHelp( wBalloonHelp_t * bh )
{
	balloonHelpStrings = bh;
	if (!tooltips)
		tooltips = gtk_tooltips_new();
}

void wEnableBalloonHelp( int enable )
{
	enableBalloonHelp = enable;
	if (tooltips) {
		if (enable)
			gtk_tooltips_enable( tooltips );
		else
			gtk_tooltips_disable( tooltips );
	}
}


void wControlSetBalloon( wControl_p b, wPos_t dx, wPos_t dy, const char * msg )
{
	PangoLayout * layout;
	wPos_t x, y;
	wPos_t w, h;
	wPos_t xx, yy;
	const char * msgConverted;	
	if (balloonVisible && balloonB == b &&
		balloonDx == dx && balloonDy == dy && balloonMsg == msg )
		return;

	if ( msg == NULL ) {
		if ( balloonF != NULL ) {
			gtk_widget_hide( balloonF );
			balloonVisible = FALSE;
		}
		balloonMsg = "";
		return;
	}
	msgConverted = gtkConvertInput(msg);
	if ( balloonF == NULL ) {
		balloonF = gtk_window_new( GTK_WINDOW_POPUP );
		gtk_window_set_policy( GTK_WINDOW (balloonF), FALSE, FALSE, TRUE );
		gtk_widget_realize( balloonF );
		balloonPI = gtk_label_new(msgConverted);
		gtk_container_add( GTK_CONTAINER(balloonF), balloonPI );
		gtk_container_border_width( GTK_CONTAINER(balloonF), 1 );
		gtk_widget_show( balloonPI );
	} else {
		gtk_label_set( GTK_LABEL(balloonPI), msgConverted );
	}
	balloonDx = dx;
	balloonDy = dy;
	balloonB = b;
	balloonMsg = msg;
	gtk_widget_hide( balloonF );
    layout = gtk_widget_create_pango_layout( balloonPI, msgConverted );
    pango_layout_get_pixel_size( layout, &w, &h );
	g_object_unref(G_OBJECT(layout));
	h = 16;
	gdk_window_get_position( GTK_WIDGET(b->parent->gtkwin)->window, &x, &y );
	gdk_window_get_origin( GTK_WIDGET(b->parent->gtkwin)->window, &x, &y );
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
	gtk_widget_set_usize( balloonPI, w, h );
	gtk_widget_set_usize( balloonF, w+2, h+2 );
	gtk_widget_show( balloonF );
	gtk_widget_set_uposition( balloonF, x, y );
	/*gtk_widget_popup( balloonF, x, y );*/
	gdk_draw_rectangle( balloonF->window, balloonF->style->fg_gc[GTK_STATE_NORMAL], FALSE, 0, 0, w+1, h+1 );
	gtk_widget_queue_resize( GTK_WIDGET(balloonF) );
	/*gtk_widget_set_uposition( balloonF, x, y );*/
	balloonVisible = TRUE;
}


void wControlSetBalloonText(
		wControl_p b,
		const char * label )
{
	const char * helpStr;
	if ( b->widget == NULL) abort();
	helpStr = (char*)gtk_object_get_data( GTK_OBJECT(b->widget), HelpDataKey );
	if ( helpStr == NULL ) helpStr = "NoHelp";
	if (tooltips)
		gtk_tooltips_set_tip( tooltips, b->widget, label, helpStr );
}


EXPORT void gtkHelpHideBalloon( void )
{
	if (balloonF != NULL && balloonVisible) {
		gtk_widget_hide( balloonF );
		balloonVisible = FALSE;
	}
}

#ifdef XV_help
static Notify_value showBalloonHelp( Notify_client client, int which )
{
	wControlSetBalloon( balloonHelpB, 0, 0, balloonHelpString );
	return NOTIFY_DONE;
}
#endif

static wWin_p balloonHelp_w;
static wPos_t balloonHelp_x;
static wPos_t balloonHelp_y;
static void prepareBalloonHelp( wWin_p w, wPos_t x, wPos_t y )
{
#ifdef XV
	wControl_p b;
	char * hs;
	int appNameLen = strlen( wAppName ) + 1;
	if (w == NULL)
		return;
#ifdef LATER
	if (!enableBalloonHelp)
		return;
#endif
	if (!balloonHelpStrings)
		return;

	balloonHelp_w = w;
	balloonHelp_x = x;
	balloonHelp_y = y;

	for ( b=w->first; b; b=b->next ) {
		switch ( b->type ) {
		case B_BUTTON:
		case B_CANCEL:
		case B_TEXT:
		case B_INTEGER:
		case B_LIST:
		case B_DROPLIST:
		case B_COMBOLIST:
		case B_RADIO:
		case B_TOGGLE:
		case B_DRAW:
		case B_MULTITEXT:
			if (x >= b->realX && x < b->realX+b->w &&
				y >= b->realY && y < b->realY+b->h ) {
				hs = (char*)gtk_get( b->panel_item, XV_HELP_DATA );
				if ( hs ) {
					hs += appNameLen;
					for ( currBalloonHelp = balloonHelpStrings; currBalloonHelp->name && strcmp(currBalloonHelp->name,hs) != 0; currBalloonHelp++ );
					if (currBalloonHelp->name && balloonHelpB != b && currBalloonHelp->value ) {
						balloonHelpB = b;
						balloonHelpString = currBalloonHelp->value;
						if (enableBalloonHelp) {
							wControlSetBalloon( balloonHelpB, 0, 0, balloonHelpString );
							/*setTimer( balloonHelpTimeOut, showBalloonHelp );*/
						} else {
							/*resetBalloonHelp();*/
						}
					}
					return;
				}
			}
		default:
			;
		}
	}
	cancelTimer( showBalloonHelp );
	resetBalloonHelp();
#endif
}


void wBalloonHelpUpdate( void )
{
	balloonHelpB = NULL;
	balloonMsg = NULL;
	prepareBalloonHelp( balloonHelp_w, balloonHelp_x, balloonHelp_y );
}


void gtkAddHelpString(
		GtkWidget * widget,
		const char * helpStr )
{
	int rc;
	char *string;
	wBalloonHelp_t * bhp;
	rc = 0;
	if (helpStr==NULL || *helpStr==0)
		return;
	if ( balloonHelpStrings == NULL )
		return;
	for ( bhp = balloonHelpStrings; bhp->name && strcmp(bhp->name,helpStr) != 0; bhp++ );
	if (listMissingHelpStrings && !bhp->name) {
		printf( "Missing Help String: %s\n", helpStr );
		return;
	}
	string = malloc( strlen(wAppName) + 5 + strlen(helpStr) + 1 );
	sprintf( string, "%sHelp/%s", wAppName, helpStr );
	if (tooltips)
		gtk_tooltips_set_tip( tooltips, widget, _(bhp->value), string );
	gtk_object_set_data( GTK_OBJECT( widget ), HelpDataKey, string );
	if (listHelpStrings)
		printf( "HELPSTR - %s\n", string );
}


EXPORT const char * wControlGetHelp(
		wControl_p b )		/* Control */
/*
Returns the help topic string associated with <b>.
*/
{
	const char * helpStr;
	helpStr = (char*)gtk_object_get_data( GTK_OBJECT(b->widget), HelpDataKey );
	return helpStr;
}


EXPORT void wControlSetHelp(
	 wControl_p b,		/* Control */
	 const char * help )		/* topic string */
/*
Set the help topic string for <b> to <help>.
*/
{
	const char * helpStr;
	if (b->widget == 0) abort();
	helpStr = wControlGetHelp(b);
	if (tooltips)
		gtk_tooltips_set_tip( tooltips, b->widget, help, helpStr );
}



/**
 * Handle the commands issued from the Help drop-down. Currently, we only have a table 
 * of contents, but search etc. might be added in the future.
 *
 * \PARAM data IN command value 
 *
 */
 
static void 
DoHelpMenu( void *data )
{
	int func = (intptr_t)data;
	
	switch( func )
	{
		case 1:
			wHelp( "index" );
			break;
		default:
			break;
	}
	
	return;		
}

/**
 * Add the entries for Help to the drop-down.
 * 
 * \PARAM m IN handle of drop-down
 *
 */

void wMenuAddHelp( wMenu_p m )
{
	wMenuPushCreate( m, NULL, _("&Contents"), 0, DoHelpMenu, (void*)1 );
}
