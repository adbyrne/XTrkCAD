/** \file wpref.c
 * Handle loading and saving preferences.
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
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE


#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "wlib.h"
#include "gtkint.h"
#include "dynarr.h"
#include "i18n.h"

#include "xtrkcad-config.h"

extern char wConfigName[];
static char appLibDir[BUFSIZ];
static char appWorkDir[BUFSIZ];
static char userHomeDir[BUFSIZ];

/*
 *******************************************************************************
 *
 * Get Dir Names
 *
 *******************************************************************************
 */


/** Find the directory where configuration files, help, demos etc are installed. 
 *  The search order is:
 *  1. Directory specified by the XTRKCADLIB environment variable
 *  2. Directory specified by XTRKCAD_INSTALL_PREFIX/share/xtrkcad
 *  3. /usr/lib/xtrkcad
 *  4. /usr/local/lib/xtrkcad
 *  
 *  \return pointer to directory name
 */

const char * wGetAppLibDir( void )
{
	char * cp, *ep;
	char msg[BUFSIZ*2];
	char envvar[80];
	struct stat buf;

	if (appLibDir[0] != '\0') {
		return appLibDir;
	}

	for (cp=wlibGetAppName(),ep=envvar; *cp; cp++,ep++)
		*ep = toupper(*cp);
	strcpy( ep, "LIB" );
	ep = getenv( envvar );
	if (ep != NULL) {
		if ((stat( ep, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
			strncpy( appLibDir, ep, sizeof appLibDir );
			return appLibDir;
		}
	}

	strcpy(appLibDir, XTRKCAD_INSTALL_PREFIX);
	strcat(appLibDir, "/share/");
	strcat(appLibDir, wlibGetAppName());

	if ((stat( appLibDir, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
		return appLibDir;
	}

	strcpy( appLibDir, "/usr/lib/" );
	strcat( appLibDir, wlibGetAppName() );
	if ((stat( appLibDir, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
		return appLibDir;
	}

	strcpy( appLibDir, "/usr/local/lib/" );
	strcat( appLibDir, wlibGetAppName() );
	if ((stat( appLibDir, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
		return appLibDir;
	}

	sprintf( msg,
		_("The required configuration files could not be located in the expected location.\n\n"
		"Usually this is an installation problem. Make sure that these files are installed in either \n"
		"  %s/share/xtrkcad or\n"
		"  /usr/lib/%s or\n"
		"  /usr/local/lib/%s\n"
		"If this is not possible, the environment variable %s must contain "
		"the name of the correct directory."),
		XTRKCAD_INSTALL_PREFIX, wlibGetAppName(), wlibGetAppName(), envvar );
	wNoticeEx( NT_ERROR, msg, _("Ok"), NULL );
	appLibDir[0] = '\0';
	wExit(0);
	return NULL;
}

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
	char tmp[BUFSIZ+20];
	char * homeDir;
	DIR *dirp;
	
	if (appWorkDir[0] != '\0')
		return appWorkDir;

	if ((homeDir = getenv( "HOME" )) == NULL) {
		wNoticeEx( NT_ERROR, _("HOME is not set"), _("Exit"), NULL);
		wExit(0);
	}
	sprintf( appWorkDir, "%s/.%s", homeDir, wlibGetAppName() );
	if ( (dirp = opendir(appWorkDir)) != NULL ) {
		closedir(dirp);
	} else {
		if ( mkdir( appWorkDir, 0777 ) == -1 ) {
			sprintf( tmp, _("Cannot create %s"), appWorkDir );
			wNoticeEx( NT_ERROR, tmp, _("Exit"), NULL );
			wExit(0);
		} else {
			/* 
			 * check for default configuration file and copy to 
			 * the workdir if it exists
			 */
			struct stat stFileInfo;
			char appEtcConfig[BUFSIZ];
			sprintf( appEtcConfig, "/etc/%s.rc", wlibGetAppName());
			
			if ( stat( appEtcConfig, &stFileInfo ) == 0 ) {
				char copyConfigCmd[(BUFSIZ * 2) + 3];
				sprintf( copyConfigCmd, "cp %s %s", appEtcConfig, appWorkDir );
				system( copyConfigCmd );
			}
		}
	}
	return appWorkDir;
}

/**
 * Get the user's home directory. The environment variable HOME is
 * assumed to contain the proper directory.
 *
 * \return    pointer to the user's home directory
 */

const char *wGetUserHomeDir( void )
{
	char *homeDir;
	
	if( userHomeDir[ 0 ] != '\0' )
		return userHomeDir;
		
	if ((homeDir = getenv( "HOME" )) == NULL) {
		wNoticeEx( NT_ERROR, _("HOME is not set"), _("Exit"), NULL);
		wExit(0);
	} else {
		strcpy( userHomeDir, homeDir );
	}	

	return userHomeDir;
}


/*
 *******************************************************************************
 *
 * Preferences
 *
 *******************************************************************************
 */

typedef struct {
		char * section;
		char * name;
		wBool_t present;
		wBool_t dirty;
		char * val;
		} prefs_t;
dynArr_t prefs_da;
#define prefs(N) DYNARR_N(prefs_t,prefs_da,N)

static wBool_t prefInitted = FALSE;
static GKeyFile *keyFile = NULL;
static gchar *stringBuffer = NULL;


static gchar *
BuildConfigFileName()
{
    gchar *tmp;
    gchar *result;
    const char * workDir;
    
    workDir = wGetAppWorkDir();
    tmp = g_build_filename (workDir,
                           wConfigName, 
                           NULL );
    
    result = g_strconcat( tmp, ".ini", NULL );
    g_free( tmp );
    
    return( result );
}    
/**
 * Read the configuration file into memory
 */

static void readPrefs( void )
{
    gchar *tmp;
    GError *error = NULL;
        
    prefInitted = TRUE;

    tmp = BuildConfigFileName();

    keyFile = g_key_file_new();
        
    if(keyFile) {
        g_key_file_load_from_file(keyFile,
                                  tmp,
                                  G_KEY_FILE_KEEP_COMMENTS,
                                  &error );
        if( error ) {
            if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
                g_warning ("Error loading key file: %s", error->message);
        }    
    }  
    g_free(tmp);
}

/**
 * Store a string in the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param sval IN value to save
 */

void wPrefSetString(
		const char * section,		/* Section */
		const char * name,		/* Name */
		const char * sval )		/* Value */
{
    if (!prefInitted)
	readPrefs();

    g_key_file_set_string( keyFile,
                           section,
                           name,
                           sval );
}

/**
 * Get a string from the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 */

char * wPrefGetStringBasic(
		const char * section,			/* Section */
		const char * name )			/* Name */
{
    GError *error = NULL;
    gchar *value;
    
    if (!prefInitted)
            readPrefs();

    value = g_key_file_get_string( keyFile,
                                   section,
                                   name,
                                   &error );
    
    // re-use the previously allocated result buffer
    if(stringBuffer) {
        g_free(stringBuffer);
    }
    stringBuffer = g_strdup( value );
    g_free( value );
    
    return( stringBuffer );
}

/**
 * Store an integer value in the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param lval IN value to save
 */

 void wPrefSetInteger(
		const char * section,		/* Section */
		const char * name,		/* Name */
		long lval )		/* Value */
{
    if (!prefInitted)
	readPrefs();
    
    g_key_file_set_int64( keyFile,
                           section,
                           name, 
                           lval );
}

/**
 * Read an integer value from the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param res OUT resulting value
 * \param default IN default value
 * \return TRUE if value differs from default, FALSE if the same
 */

wBool_t wPrefGetIntegerBasic(
		const char * section,		/* Section */
		const char * name,		/* Name */
		long * res,		/* Address of result */
		long def )		/* Default value */
{
    GError *error = NULL;
    gint64 value;
    
    if (!prefInitted)
	readPrefs();
    
    value = g_key_file_get_int64( keyFile,
                                  section,
                                  name,
                                  &error );
    if((error && (error->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND)) || value == def ) {
        *res = def;
        return( FALSE );
    } else {
        *res= value;
        return( TRUE );
    }   
}

/**
 * Save a float value in the preferences file. 
 *
 * \param section IN the file section into which the value should be saved
 * \param name IN the name of the preference
 * \param lval IN the value
 */

 void wPrefSetFloat(
		const char * section,		/* Section */
		const char * name,		/* Name */
		double lval )		/* Value */
{
    if (!prefInitted)
	readPrefs();
         
    g_key_file_set_double( keyFile,
                           section,
                           name, 
                           lval );     
}

/**
 * Read a float from the preferencesd file.
 *
 * \param section IN the file section from which the value should be read
 * \param name IN the name of the preference
 * \param res OUT pointer for the value
 * \param def IN	default value
 * \return TRUE if value was read, FALSE if default value is used
 */


wBool_t wPrefGetFloatBasic(
		const char * section,		/* Section */
		const char * name,		/* Name */
		double * res,		/* Address of result */
		double def )		/* Default value */
{
    GError *error = NULL;
    gdouble value;
    
    if (!prefInitted)
	readPrefs();
    
    value = g_key_file_get_double( keyFile,
                                   section,
                                   name,
                                   &error );
    if(error || value == def ) {
        *res = def;
        return( FALSE );
    } else {
        *res= value;
        return( TRUE );
    }   
}

/**
 * Save the configuration to a file. The config parameters are held and updated in an array.
 * To make the settings persistant, this function has to be called. 
 *
 */

void wPrefFlush(
		char * name )
{
	prefs_t * p;
	char tmp[BUFSIZ];
    const char *workDir;
	FILE * prefFile;

	if (!prefInitted)
		return;
	
	workDir = wGetAppWorkDir();
	if (name && name[0])
		sprintf( tmp, "%s", name );
	else
		sprintf( tmp, "%s/%s.rc", workDir, wConfigName );
	prefFile = fopen( tmp, "w" );
	if (prefFile == NULL)
		return;

	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if(p->val) {
			fprintf( prefFile,  "%s.%s: %s\n", p->section, p->name, p->val );
		}
	}
	fclose( prefFile );
}


/**
 * Clear the preferences from memory
 * \return  
 */

void wPrefReset(void )
{
    prefInitted = FALSE;
    g_key_file_free( keyFile );
}
