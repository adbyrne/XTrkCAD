/** \file paths.c
 * Find relevant directories
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis, 2023 Martin Fischer
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

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <glib.h>
#include <glib/gstdio.h>

#include "wlib.h"
#include "gtkint.h"
#include "i18n.h"

static char *appLibDir;
static char *appWorkDir;
static char *userHomeDir;

#define PATHOPTIONMAX (10)

/*
 *******************************************************************************
 *
 * Get Dir Names
 *
 *******************************************************************************
 */

/**
 * Check for existance of a directory
 *
 * \param name directory name
 * \return true directory exists
 * \return false directory does not exist or is a file
 */
static bool
IsExistingDirectory( char *name)
{
	struct stat dirState;

	if(( stat( name, &dirState) == 0 ) && S_ISDIR( dirState.st_mode)) {
		return true;
	} else {
		return false;
	}
}

/**
 * Find the directory where configuration files, help, demos etc are installed.
 *
 * Windows: the directory is expected to be at ..\share\xtrkcad
 * 
 * Unix: The search order is:
 *  1. Directory specified by the XTRKCADLIB environment variable
 *  2. relative path ../xtrkcad from the installation directory
 *  3. /usr/share/xtrkcad
 *  4. /usr/local/share/xtrkcad
 *  if no directory can be found, the program terminates
 *
 *  \return pointer to directory name
 */

#ifdef WIN32
const char* wGetAppLibDir(void)
{
	gchar* moduleDir;
	GString* libDir;

	if (appLibDir != NULL) {
		return appLibDir;
	}

	moduleDir = g_win32_get_package_installation_directory_of_module(NULL);
	libDir = g_string_new(moduleDir);
	g_string_append(libDir, "\\..\\share\\xtrkcad");
	appLibDir = g_canonicalize_filename(libDir->str, NULL);
	g_free(moduleDir);
	g_string_free(libDir, TRUE);

	return(appLibDir);
}
#else // WIN32
const char * wGetAppLibDir( void )
{
	char * cp, *ep;
	char *envvar;

	char *searchDirs[PATHOPTIONMAX];
	unsigned option = 0;

	if (appLibDir != NULL) {
		return appLibDir;
	}

	// option 1: search environment
	cp =wlibGetAppName();
	envvar = g_malloc(strlen(cp)+4);

	for (ep=envvar; *cp; cp++,ep++) {
		*ep = toupper(*cp);
	}
	strcpy( ep, "LIB" );
	ep = getenv( envvar );
	searchDirs[ option++ ] = g_strdup( ep );

	// option 2: relative path to share subdir
	ep = g_canonicalize_filename ("../share/", NULL);
	searchDirs[ option++ ]= g_strdup_printf("%s/%s", ep, wlibGetAppName());
	g_free(ep);

	// option 3: OS data directory
	// if BETA Version, prefer local data dir over system data dir
	if ( strstr( XTRKCAD_VERSION, "Beta" ) != NULL ) {
		searchDirs[option++] = g_strdup_printf("/usr/local/share/%s",
		                                       wlibGetAppName() );
		searchDirs[option++] = g_strdup_printf("/usr/share/%s",
		                                       wlibGetAppName() );
	} else {
		searchDirs[option++] = g_strdup_printf("/usr/share/%s",
		                                       wlibGetAppName() );
		searchDirs[option++] = g_strdup_printf("/usr/local/share/%s",
		                                       wlibGetAppName() );
	}

	searchDirs[option] = NULL;

	appLibDir = NULL;
	for(unsigned int i = 1; i < option; i++) {
		if(IsExistingDirectory(searchDirs[ i ])) {
			appLibDir = g_strdup(searchDirs[ i ]);
			break;
		} else {
			i++;
		}
	}

	if(!appLibDir) {
		gchar *msg;

		msg = g_strdup_printf(
		              _("The required configuration files could not be located in the "
		                "expected location.\n\nUsually this is an installation problem."
		                "Make sure that these files are installed in either \n"
		                "  %s or\n"
		                "  %s or\n"
		                "  %s\n"
		                "If this is not possible, the environment variable %s must "
		                "contain the name of the correct directory."),
		              searchDirs[1],
		              searchDirs[2],
		              searchDirs[3],
		              envvar);
		wNoticeEx(NT_ERROR, msg, _("Ok"), NULL);
		g_free(msg); // not necessary as program terminates anyway
		wExit(1);
	}

	for(unsigned int i = 0; i < option; i++) {
		g_free(searchDirs[ i ]);
	}
	return(appLibDir);
}
#endif //WIN32
/**
 * Get the working directory for the application. This directory is used for storing
 * internal files including rc files. If it doesn't exist, the directory is created
 * silently.
 *
 * \return    pointer to the working directory
 */

const char * wGetAppWorkDir(
        void )
{
	const char *homeDir;

	if (appWorkDir) {
		return appWorkDir;
	}

	homeDir = wGetUserHomeDir();

	appWorkDir = g_strdup_printf( "%s/.%s", homeDir, wlibGetAppName());

	if (!IsExistingDirectory(appWorkDir)) {
		if ( g_mkdir( appWorkDir, 0777 ) == -1 ) {
			gchar *tmp;
			tmp = g_strdup_printf(  _("Cannot create %s"), appWorkDir );
			wNoticeEx( NT_ERROR, tmp, _("Exit"), NULL );
			g_free(tmp);
			wExit(0);
		} else {
			/*
			 * check for default configuration file and copy to
			 * the workdir if it exists
			 */
			struct stat stFileInfo;
			gchar *appEtcConfig;
			gchar *copyConfigCmd;
			appEtcConfig = g_strdup_printf( "/etc/%s.ini", wlibGetAppName());

			if ( stat( appEtcConfig, &stFileInfo ) == 0 ) {
				copyConfigCmd = g_strdup_printf( "cp %s %s",
				                                 appEtcConfig,
				                                 appWorkDir );
				int rc = system( copyConfigCmd );
			}
			g_free(appEtcConfig);
			g_free(copyConfigCmd);
		}
	}
	return appWorkDir;
}

/**
 * Get the user's home directory. 
 *
 * \return    pointer to the user's home directory
 */

const char *wGetUserHomeDir( void )
{
	const char *homeDir;

	if( userHomeDir ) {
		return userHomeDir;
	}

	homeDir = g_get_home_dir();
 	if (homeDir == NULL) {
		wNoticeEx( NT_ERROR, _("HOME is not set"), _("Exit"), NULL);
		wExit(0);
	} else {
		userHomeDir = g_strdup( homeDir );
	}

	return userHomeDir;
}
