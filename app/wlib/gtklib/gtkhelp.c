/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkhelp.c,v 1.1 2005-12-07 15:48:43 rc-flyer Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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

#include <X11/X.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"



wBool_t listHelpStrings = FALSE;
wBool_t listMissingHelpStrings = FALSE;
static char HelpDataKey[] = "HelpDataKey";


/*
 ******************************************************************************
 *
 * Help
 *
 ******************************************************************************
 */




#ifdef XV_help
typedef Notify_value (*setTimer_p)( Notify_client client, int which );
static void setTimer(
		long count,
		setTimer_p func )		/* milliseconds */
/*
Alarm for <count> milliseconds.
*/
{
	struct itimerval itimer;
	Notify_func rc;
	itimer.it_value.tv_sec = count / 1000;
	itimer.it_value.tv_usec = count % 1000;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;
	rc = notify_set_itimer_func( (Notify_client)gtkMainW->frame, func, ITIMER_REAL, &itimer, NULL );
}

static void cancelTimer(
		setTimer_p func )
{
	Notify_func rc;
	rc = notify_set_itimer_func( (Notify_client)gtkMainW->frame, NOTIFY_FUNC_NULL, ITIMER_REAL, NULL, NULL );
}
#endif


static GtkWidget * balloonF;
static GtkWidget * balloonPI;
static wBalloonHelp_t * balloonHelpStrings;
static int enableBalloonHelp = 1;
static const char * balloonMsg;
static wControl_p balloonB;
static wPos_t balloonDx, balloonDy;
static wBool_t balloonVisible = FALSE;
static wControl_p balloonHelpB;
#ifdef GTK
static const char * balloonHelpString;
static long balloonHelpTimeOut = 500;
static wBalloonHelp_t * currBalloonHelp;
#endif
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
#ifndef GTK1
	w = gdk_string_width( gtk_style_get_font(gtk_widget_get_style(balloonPI)), msg );
#else
	w = gdk_string_width( balloonPI->style->font, msgConverted );
#endif
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
		gtk_tooltips_set_tip( tooltips, widget, bhp->value, string );
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


static wBool_t helpInitted = FALSE;
static FILE * helpFile = NULL;
static wWin_p helpW = (wWin_p)NULL;
static wText_p helpTextB;


static wBool_t openHelpFile( void )
{
   const char * dirName;
   char tmp[BUFSIZ];

		dirName = wGetAppLibDir();
		if (dirName == NULL) {
			wNotice("Can't get app dir", "Ok", NULL );
			return FALSE;
		}
		strcpy( tmp, dirName );
		strcat( tmp, "/" );
		strcat( tmp, wAppName );
		strcat( tmp, ".help" );
		helpFile = fopen( tmp, "r" );
		if (helpFile == NULL) {
			wNotice( tmp , "Ok", NULL );
			return FALSE;
		}
		return TRUE;
}


static void winHelpProc( wWin_p win, winProcEvent e, void * data )
{
	switch (e) {
	case wQuit_e:
		wWinShow( helpW, FALSE );
		break;
	default:
		break;
	}
}

EXPORT void wHelp(
	const char * topic )		/* topic string */
/*
Invoke the help system to display help for <topic>.
*/
{
   wBool_t found = FALSE;
   char tmp[BUFSIZ];
   int level, i, len;

   if (!helpInitted) {
		helpInitted = TRUE;
 	if (helpFile == NULL && !openHelpFile() )
			return;
		helpW = wWinPopupCreate( NULL, 2, 2, NULL, "Help", "gtkhelp",
						F_AUTOSIZE, winHelpProc, NULL );
		helpTextB = wTextCreate( helpW, 0, -4, NULL, NULL, BO_READONLY|BT_CHARUNITS|BT_DOBOLD, 80, 24 );
   }
   if (helpFile == NULL)
		return;

   for ( level=0; topic[level] == '*'; level++ );
   sprintf( tmp, "Help - %s", topic+level+(level>0?1:0) );
   wWinSetTitle( helpW, tmp );
   wTextClear( helpTextB );
   gtkTextFreeze( helpTextB );
   fseek( helpFile, 0, SEEK_SET );
   while ( (!feof( helpFile )) &&
		   fgets( tmp, sizeof tmp, helpFile ) != NULL) {
		if (tmp[0] == '#')
			continue;
		if (tmp[0] == ':') {
			for ( i=0; tmp[i+1] == '*'; i++ );
			if (found && ( (level > 0 && i >= 1 && i <= level) || (level == 0 && i > 0) ) )
				break;
			len = strlen(topic);
			if (strncmp( topic, tmp+1, len ) == 0 &&
				(tmp[1+len]=='\0' || isspace(tmp[1+len]))) {
				found = TRUE;
			}
			continue;
		}
		if (found) {
			wTextAppend( helpTextB, tmp );
		}
	}
	gtkTextThaw( helpTextB );
	if ( !found ) {
		sprintf( tmp, "No help found for %s", topic+level+1 );
		wNotice( tmp, "Ok", NULL );
	} else {
		wTextSetPosition( helpTextB, 0 );
		wWinShow( helpW, TRUE );
	}
}

void wMenuAddHelp( wMenu_p m )
{
   char tmp[BUFSIZ];
   const char * name;
   wMenu_p menus[10];
   int i;

   if (m==NULL)
		return;
   if (helpFile == NULL)
		openHelpFile();
   if (helpFile == NULL)
		return;

   menus[0] = m;
   for (i=1;i<sizeof menus/sizeof menus[0];i++)
		menus[i] = NULL;
   fseek( helpFile, 0, SEEK_SET );
   while ( (!feof( helpFile )) &&
		   fgets( tmp, sizeof tmp, helpFile ) != NULL) {
		if (tmp[0] == '#')
			continue;
		i = strlen(tmp)-1;
		if (tmp[i] == '\n')
			tmp[i] = 0;
		if (tmp[0] == ':' && tmp[1] == '*' ) {
			for (i=0;tmp[i+2]=='*';i++);
			name = tmp+i+2;
			while (menus[i] == NULL) i--;
			if (name[0] == '+')
				menus[i+1] = wMenuMenuCreate( menus[i], NULL, strdup(name+1) );
			else if (name[0] == '-')
				wMenuPushCreate( menus[i], NULL, name+1, 0,
								 (wMenuCallBack_p)wHelp, (void*)strdup(tmp+1) );
		}
	}
}
