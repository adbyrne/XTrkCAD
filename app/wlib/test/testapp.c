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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <stdio.h>
#include "wlib.h"

#define APPNAME "testapp"
#define WINDOWTITLE "Test Application"

// #define TEST_ARGV
// #define TEST_SPLASH
#define TEST_PULLDOWNMENU

#define TRUE  (1)
#define FALSE (0)

long dontHideCursor = 0;

/**
 *	doFile: callback funtion for file submenu 
 */

void doFile( void * cmd )
{
   switch ((int)cmd) {
		case 1:
			break;				/* push test */
		case 2:
			break;				/* toggle test */
		case 3: 		
			break;				/* Radio test */
   	case 0:					/* 'Quit ' */
			wExit( 0 );			/* terminate application */
    }
}

void TestMenu( wWindow_p mainW)
{
	wMenu_p menu1;
	wMenu_p menu2;

	/* add a submenu */ 	
    menu1 = wMenuBarAdd( mainW, 		/* parent window */
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
    menu2 = wMenuBarAdd( mainW, 		/* parent window */
						 NULL, 			/* help topic */
						"Help" 			/* submenu title */
					    );	

	wMenuToggleCreate(menu2,
				NULL,
				"Active",
				0,
				1, 
				doFile,
				(void *)2
				);

	wMenuToggleCreate(menu2,
				NULL,
				"Inactive",
				0,
				0, 
				doFile,
				(void *)2
				);

	wMenuToggle_p mt = wMenuToggleCreate(menu2,
				NULL,
				"Disabled",
				0,
				0, 
				doFile,
				(void *)2
				);

	wMenuToggleEnable( mt, FALSE );

	wMenuSeparatorCreate( menu2 );
	
	wMenuRadioCreate(menu2,
				NULL,
				"Radio 1",
				0,
				doFile,
				(void *)3
				);

	wMenuRadioCreate(menu2,
				NULL,
				"Radio 2",
				0,
				doFile,
				(void *)3
				);

	wMenuRadioCreate(menu2,
				NULL,
				"Radio 3",
				0,
				doFile,
				(void *)3
				);

}

wWindow_p wMain( int argc, char * argv[] )
{

	wWindow_p mainW;
#ifdef TEST_ARGV
	printf("testapp: argc: %d\n", argc);

	for(int i = 0; i< argc; i++)
		printf("%s\n", argv[i]);
#endif

	wInitAppName(APPNAME);

	printf("%s\n", wGetUserHomeDir());

#ifdef TEST_SPLASH
	/* add a splash window */
	wCreateSplash( WINDOWTITLE,			/* name of application to show */
						"1.0"						/* application version information */
					 );	

	wFlush();									/* make sure splash window is shown */
#endif

	/* create main window */	
    mainW = wWinMainCreate( APPNAME, 	/* application name  */
	 								800, 			/* position x */
									600, 			/* position y */
									"Help", 		/* help topic */
									WINDOWTITLE, /* window title */
									APPNAME, 	/* window name */	
									F_RESIZE|F_MENUBAR, /* options */
									NULL, 		/* window callback function */
									NULL 			/* pointer to user data */
									);

	// wWinShow( mainW, FALSE );

#ifdef TEST_PULLDOWNMENU
	TestMenu(mainW);
#endif

	
#ifdef TEST_SPLASH	
	char buffer[ 80 ];

	for( int i = 2; i > 0; i-- ) {
		sprintf(buffer, "Countdown %d", i );
		wSetSplashInfo( buffer );
		wPause( 1000L );
	}
 
	wDestroySplash();							/* remove the splash window again */
#endif	

	return mainW;
}
