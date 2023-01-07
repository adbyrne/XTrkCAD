/** \file main.c
 * Main function and initialization stuff
 *
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005,2012 Dave Bullis Martin Fischer
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
#include <locale.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"


static char *appName;		/**< application name */
char *wExecutableName;


/**
 * Initialize the application name for later use
 *
 * \param _appName IN Name of application
 * \return   
 */
 
void 
wInitAppName(char *_appName)
{
	appName = g_strdup( _appName );
}

char *
wlibGetAppName()
{
	return( appName );
}	

/*
 *******************************************************************************
 *
 * Main
 *
 *******************************************************************************
 */



int main( int argc, char *argv[] )
{
	wWin_p win;
	const char *ld;

	if ( getenv( "GTKLIB_NOLOCALE" ) == 0 )
		setlocale( LC_ALL, "en_US" );
	gtk_init( &argc, &argv );

	if ((win=wMain( argc, argv )) == NULL)
		exit(1);
    wExecutableName = argv[ 0 ];
	ld = wGetAppLibDir();
	
#ifdef WINDOWS
	
#else
	// set up help search path on unix boxes
	if (ld != NULL) {
		static char buff[BUFSIZ];
		const char *hp;

		sprintf( buff, "HELPPATH=/usr/lib/help:%s:", ld );
		if ( (hp = getenv("HELPPATH")) != NULL )
			strcat( buff, hp );
		putenv( buff );
	}
#endif
	
	if (!win->shown)
		wWinShow( win, TRUE );

	gtk_main();
	exit(0);
}
