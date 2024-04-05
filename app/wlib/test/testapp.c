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

#define FILEPATTERN "All XTrackCAD Files (*.xtc,*.xtce)|*.xtc;*.xtce|" \
		               "XTrackCAD Trackplan (*.xtc)|*.xtc|" \
		               "XTrackCAD Extended Trackplan (*.xtce)|*.xtce|" \
		               "All Files (*)|*"

// #define TEST_ARGV
// #define TEST_SPLASH
#define TEST_PULLDOWNMENU
#define TEST_STATUSBAR

#define TRUE  (1)
#define FALSE (0)

long dontHideCursor = 0;

wMenuList_p menuList;
wStatus_p statusMsg;

/**
 *	doFile: callback funtion for file submenu 
 */

void doFile( void * cmd )
{
	static int recent = 1;
	char buffer[20];

      switch ((int)cmd) {
		case 1:
			sprintf(buffer, "Recent %d", recent++);
			wMenuListAdd(menuList, 0, buffer, "data for recent x");
			break;				/* push test */
		case 2:
//			printf("Menu toggle changed!\n");
			wStatusSetValue(statusMsg, "Menu toggle changed!");
			break;				/* toggle test */
		case 3: 
//			printf("Active radio button changed!\n");
			wStatusSetValue(statusMsg, "Active radio button changed!");
			break;				/* Radio test */
   		case 0:					/* 'Quit ' */
			wExit( 0 );			/* terminate application */
    }
}

/**
 * RecentusedCallback.
 * 
 * \param data	pointer to item data 
 */

void
RecentUsedCallback(int unused, char *label, char* data)
{
	printf("Recent used: %s - %s\n", label, data);
}


static struct wFilSel_t* loadFile_fs = NULL;
static struct wFilSel_t* saveFile_fs = NULL;
static struct wFilSel_t* pictureFile_fs = NULL;

int LoadData(
	int cnt,
	char** fileName,
	void* data)
{
	for (int i = 0; i < cnt; i++) {
		printf("File no. %d is %s\n", i, fileName[i]);
	}
	return(0);
}

void
OpenForLoad(void* data)
{
	wFilSelect(loadFile_fs, ".");
}

void
OpenPictures(void* data)
{
	wFilSelect(pictureFile_fs, ".");
}

void
OpenForSave(void* data)
{
	wFilSelect(saveFile_fs, ".");
}



void
LoadFileSelector(wWindow_p mainW, wMenu_p parent)
{
	wMenuPushCreate(parent, "menuFile-load", "Open ...", WCTL+'o',
		OpenForLoad, NULL);

	if (loadFile_fs == NULL) 
		loadFile_fs = wFilSelCreate(mainW, FS_LOAD, FSO_MULTIPLEFILES, "Open Tracks",
			FILEPATTERN, LoadData, NULL);
}

void
PixmapFileSelector(wWindow_p mainW, wMenu_p parent)
{
	wMenuPushCreate(parent, "menuFile-load", "Open Pictures...", WCTL + 'p',
		OpenPictures, NULL);

	if(pictureFile_fs == NULL)
		pictureFile_fs = wFilSelCreate(mainW, FS_LOAD, FSO_PICTURES, "Open Pictures",
			NULL, LoadData, NULL);
}

void
SaveFileSelector(wWindow_p mainW, wMenu_p parent)
{
	wMenuPushCreate(parent, "menuFile-save", "Save ...", WCTL + 's',
		OpenForSave, NULL);

	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate(mainW, FS_SAVE, 0, "_Save Tracks",
			FILEPATTERN, LoadData, NULL);
}

void TestMenu( wWindow_p mainW)
{
	wMenu_p menu1;
	wMenu_p menu2;
	wMenu_p menu3;
	wMenu_p menu4;

	/* add a submenu */ 	
    menu1 = wMenuBarAdd( mainW, 		/* parent window */
						NULL, 			/* help topic */
						"_File" 			/* submenu title */
						);	

 	LoadFileSelector(mainW, menu1);
	PixmapFileSelector(mainW, menu1);
	SaveFileSelector(mainW, menu1);

	wMenuSeparatorCreate(menu1);

	/* create a menuitem in submenu */
	wMenuPushCreate( 	menu1, 				/* parent menu */
							NULL, 				/* help topic */
							"Add to MRU", 				/* submenu title */
							WCTL+'a', 					/* accelerator key */
							doFile, 				/* callback funtion */
							(void*)1 			/* pointer to user data */
						 );									
	
	menu4 = wMenuMenuCreate(menu1, NULL, "Recently used");
 	menuList = wMenuListCreate(menu4, NULL, 10, RecentUsedCallback);

	/* create a separator before 'Quit' */	
 
	wMenuSeparatorCreate( menu1 );
	
	/* create a menuitem in submenu */
	wMenuPushCreate( 	menu1, 				/* parent menu */
							NULL, 				/* help topic */
							"_Quit", 				/* submenu title */
							WALT+'x',					/* accelerator key */
							doFile, 				/* callback funtion */
							(void*)0 			/* pointer to user data */
						 );									

	/* create a second submenu */
    menu2 = wMenuBarAdd( mainW, 		/* parent window */
						 NULL, 			/* help topic */
						"_Checks" 			/* submenu title */
					    );	

	wMenuToggleCreate(menu2,
				NULL,
				"Active",
				WALT+'a',
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

	menu3 = wMenuMenuCreate(menu2, NULL, "Radio Buttons");
	
	wMenuRadioCreate(menu3,
				NULL,
				"Radio 1",
				WCTL+'1',
				doFile,
				(void *)3
				);

	wMenuRadioCreate(menu3,
				NULL,
				"Radio 2",
				WCTL + '2',
				doFile,
				(void *)3
				);

	wMenuRadioCreate(menu3,
				NULL,
				"Radio 3",
				WCTL + '3',
				doFile,
				(void *)3
				);

 }

void
TestStatusbar(wWindow_p mainWindow)
{
	wStatusCreate(mainWindow,
		"statusbarPosX",
		"X: 0");

	wStatusCreate(mainWindow,
		"statusbarPosY",
		"Y: 0");

	wStatusCreate(mainWindow,
		"statusbarScale",
		"Scale: 1/87");

	statusMsg = wStatusCreate(mainWindow,
		"statusbarMessage",
		"Statusbar was created successfully...");

}
static
bool mainCallBack(wWindow_p window, winProcEvent ev, void *data1, void *data2 )
{
	int result;
	switch (ev) {
	case wClose_e:
  		result = wNotice3("Close application without saving?", "_Yes", "_Save", "_ No");
		printf("Exit warning result=%d\n", result);
		if (result != 1)
			return TRUE;
		else
			return FALSE;
		break;
	default:
		return FALSE;
	}
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

#ifdef PATH

	printf("%s\n", wGetUserHomeDir());
	printf("%s\n", wGetAppLibDir());

#endif // PATH

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
									mainCallBack, 		/* window callback function */
									NULL 			/* pointer to user data */
									);

	// wWinShow( mainW, FALSE );

#ifdef TEST_PULLDOWNMENU
	TestMenu(mainW);
#endif

#ifdef TEST_STATUSBAR
	TestStatusbar(mainW);
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
