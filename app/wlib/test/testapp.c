/** \file testapp.c
 * Small test application to demonstrate functionality of the XTrkCad windowing library wlib
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/test/testapp.c,v 1.2 2007-09-14 16:17:24 m_fischer Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 
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
#include "wlib.h"

#define APPNAME "TESTAPP"
#define WINDOWTITLE "Test Application"

#define TRUE	(1)
#define FALSE (0)

/**
 *	doFile: callback funtion for file submenu 
 */

void doFile( void * cmd )
{
   switch ((int)cmd) {
		case 1:
			break;
   	case 0:					/* 'Quit ' */
			wExit( 0 );			/* terminate application */
    }
}


wWin_p wMain( int argc, char * argv[] )
{

	wWin_p mainW;
	wMenu_p menu1;
	wMenu_p menu2;
	int i;
	char buffer[ 80 ];

	/* add a splash window */
	wCreateSplash( WINDOWTITLE,			/* name of application to show */
						"1.0"						/* application version information */
					 );	

	wFlush();									/* make sure splash window is shown */

	/* create main window */	
    mainW = wWinMainCreate( APPNAME, 	/* application name  */
	 								200, 			/* position x */
									100, 			/* position y */
									"Help", 		/* help topic */
									WINDOWTITLE, /* window title */
									APPNAME, 	/* window name */	
									F_RESIZE|F_MENUBAR, /* options */
									NULL, 		/* window callback function */
									NULL 			/* pointer to user data */
									);

	wWinShow( mainW, FALSE );

	/* add a submenu */ 	
    menu1 = wMenuBarAdd( mainW, 			/* parent window */
								NULL, 			/* help topic */
								"File" 			/* submenu title */
							 );	

	/* create a menuitem in submenu */
	wMenuPushCreate( 	menu1, 				/* parent menu */
							NULL, 				/* help topic */
							"Test", 				/* submenu title */
							0, 					/* accelerator key */
							doFile, 				/* callback funtion */
							(void*)1 			/* pointer to user data */
						 );									
	

	/* create a separator before 'Quit' */	
	wMenuSeparatorCreate( menu1 );
	
	/* create a menuitem in submenu */
	wMenuPushCreate( 	menu1, 				/* parent menu */
							NULL, 				/* help topic */
							"Quit", 				/* submenu title */
							0, 					/* accelerator key */
							doFile, 				/* callback funtion */
							(void*)0 			/* pointer to user data */
						 );									
	
	/* create a second submenu */
    menu2 = wMenuBarAdd( mainW, 			/* parent window */
								NULL, 			/* help topic */
								"Help" 			/* submenu title */
							 );	

	for( i = 5; i > 0; i-- ) {
		sprintf(buffer, "Countdown %d", i );
		wSetSplashInfo( buffer );
		wPause( 1000L );
	}
 
	wWinShow( mainW, TRUE );
	wPause ( 2000L );
	wDestroySplash();							/* remove the splash window again */
	
	return mainW;
}
