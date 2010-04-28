/** \file wpref.c Handle loading and saving preferences.
 * 
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/wpref.c,v 1.15 2010-04-28 04:04:38 dspagnol Exp $
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
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

#include "wlib.h"
#include "dynarr.h"
#include "i18n.h"

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

#ifdef XTRKCAD_CMAKE_BUILD
#include "xtrkcad-config.h"
#endif

extern char wAppName[];
extern char wConfigName[];
static char appLibDir[BUFSIZ];
static char appWorkDir[BUFSIZ];
static char userHomeDir[BUFSIZ];

EXPORT void wInitAppName(char *appName)
{
	strcpy(wAppName, appName);
}

/*
 *******************************************************************************
 *
 * Get Dir Names
 *
 *******************************************************************************
 */


EXPORT const char * wGetAppLibDir( void )
/** Find the directory where configuration files, help, demos etc are installed. 
 *  The search order is:
 *  1. Directory specified by the XTRKCADLIB environment variable
 *  2. Directory specified by XTRKCAD_INSTALL_PREFIX/share/xtrkcad
 *  3. /usr/lib/xtrkcad
 *  4. /usr/local/lib/xtrkcad
 *  
 *  \return pointer to directory name
 */
{
	char * cp, *ep;
	char msg[BUFSIZ*2];
	char envvar[80];
	struct stat buf;

	if (appLibDir[0] != '\0') {
		return appLibDir;
	}

	for (cp=wAppName,ep=envvar; *cp; cp++,ep++)
		*ep = toupper(*cp);
	strcpy( ep, "LIB" );
	ep = getenv( envvar );
	if (ep != NULL) {
		if ((stat( ep, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
			strncpy( appLibDir, ep, sizeof appLibDir );
			return appLibDir;
		}
	}

#ifdef XTRKCAD_CMAKE_BUILD
	strcpy(appLibDir, XTRKCAD_INSTALL_PREFIX);
	strcat(appLibDir, "/share/");
	strcat(appLibDir, wAppName);

	if ((stat( appLibDir, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
		return appLibDir;
	}
#endif

	strcpy( appLibDir, "/usr/lib/" );
	strcat( appLibDir, wAppName );
	if ((stat( appLibDir, &buf) == 0 ) && S_ISDIR( buf.st_mode)) {
		return appLibDir;
	}

	strcpy( appLibDir, "/usr/local/lib/" );
	strcat( appLibDir, wAppName );
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
		XTRKCAD_INSTALL_PREFIX, wAppName, wAppName, envvar );
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


EXPORT const char * wGetAppWorkDir(
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
	sprintf( appWorkDir, "%s/.%s", homeDir, wAppName );
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
			sprintf( appEtcConfig, "/etc/%s.rc", wAppName );
			
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

EXPORT const char *wGetUserHomeDir( void )
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
wBool_t prefInitted = FALSE;

static void readPrefs( void )
{
	char tmp[BUFSIZ], *sp, *np, *vp, *cp;
	const char * workDir;
	FILE * prefFile;
	prefs_t * p;

	prefInitted = TRUE;
	workDir = wGetAppWorkDir();
	sprintf( tmp, "%s/%s.rc", workDir, wConfigName );
	prefFile = fopen( tmp, "r" );
	if (prefFile == NULL)
		return;
	while ( ( fgets(tmp, sizeof tmp, prefFile) ) != NULL ) {
		sp = tmp;
		while ( *sp==' ' || *sp=='\t' ) sp++;
		if ( *sp == '\n' || *sp == '#' )
			continue;
		np = strchr( sp, '.' );
		if (np == NULL) {
			wNoticeEx( NT_INFORMATION, tmp, _("Continue"), NULL );
			continue;
		}
		*np++ = '\0';
		while ( *np==' ' || *np=='\t' ) np++;
		vp = strchr( np, ':' );
		if (vp == NULL) {
			wNoticeEx( NT_INFORMATION, tmp, _("Continue"), NULL );
			continue;
		}
		*vp++ = '\0';
		while ( *vp==' ' || *vp=='\t' ) vp++;
		cp = vp + strlen(vp) -1;
		while ( cp >= vp && (*cp=='\n' || *cp==' ' || *cp=='\t') ) cp--;
		cp[1] = '\0';
		DYNARR_APPEND( prefs_t, prefs_da, 10 );
		p = &prefs(prefs_da.cnt-1);
		p->name = strdup(np);
		p->section = strdup(sp);
		p->dirty = FALSE;
		p->val = strdup(vp);
	}
	fclose( prefFile );
}

/**
 * Store a string in the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param sval IN value to save
 */

EXPORT void wPrefSetString(
		const char * section,		/* Section */
		const char * name,		/* Name */
		const char * sval )		/* Value */
{
	prefs_t * p;

	if (!prefInitted)
		readPrefs();
	
	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if ( strcmp( p->section, section ) == 0 && strcmp( p->name, name ) == 0 ) {
			if (p->val)
				free(p->val);
			p->dirty = TRUE;
			p->val = strdup( sval );
			return;
		}
	}
	DYNARR_APPEND( prefs_t, prefs_da, 10 );
	p = &prefs(prefs_da.cnt-1);
	p->name = strdup(name);
	p->section = strdup(section);
	p->dirty = TRUE;
	p->val = strdup(sval);
}


EXPORT const char * wPrefGetString(	
		const char * section,			/* Section */
		const char * name )			/* Name */
/*
*/
{
	prefs_t * p;

	if (!prefInitted)
		readPrefs();
	
	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if ( strcmp( p->section, section ) == 0 && strcmp( p->name, name ) == 0 ) {
			return p->val;
		}
	}
	return NULL;
}

/**
 * Store an integer value in the user preferences.
 *
 * \param section IN section in preferences file
 * \param name IN name of parameter
 * \param lval IN value to save
 */

EXPORT void wPrefSetInteger(
		const char * section,		/* Section */
		const char * name,		/* Name */
		long lval )		/* Value */
{
	char tmp[20];

	sprintf(tmp, "%ld", lval );
	wPrefSetString( section, name, tmp );
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

EXPORT wBool_t wPrefGetInteger(
		const char * section,		/* Section */
		const char * name,		/* Name */
		long * res,		/* Address of result */
		long def )		/* Default value */
{
	const char * cp;
    char *cp1;

	cp = wPrefGetString( section, name );
	if (cp == NULL) {
		*res = def;
		return FALSE;
	}
	*res = strtol(cp,&cp1,0);
	if (cp==cp1) {
		*res = def;
		return FALSE;
	}
	return TRUE;
}

/**
 * Save a float value in the preferences file. 
 *
 * \param section IN the file section into which the value should be saved
 * \param name IN the name of the preference
 * \param lval IN the value
 */

EXPORT void wPrefSetFloat(
		const char * section,		/* Section */
		const char * name,		/* Name */
		double lval )		/* Value */
{
	char tmp[20];

	sprintf(tmp, "%0.6f", lval );
	wPrefSetString( section, name, tmp );
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


EXPORT wBool_t wPrefGetFloat(
		const char * section,		/* Section */
		const char * name,		/* Name */
		double * res,		/* Address of result */
		double def )		/* Default value */
{
	const char * cp;
    char *cp1;

	cp = wPrefGetString( section, name );
	if (cp == NULL) {
		*res = def;
		return FALSE;
	}
	*res = strtod(cp, &cp1);
	if (cp == cp1) {
		*res = def;
		return FALSE;
	}
	return TRUE;
}


EXPORT const char * wPrefGetSectionItem(
		const char * sectionName,
		wIndex_t * index,
		const char ** name )
{
	prefs_t * p;

	if (!prefInitted)
		readPrefs();
	
	if ( *index >= prefs_da.cnt )
		return NULL;

	for (p=&prefs((*index)++); p<&prefs(prefs_da.cnt); p++,(*index)++) {
		if ( strcmp( p->section, sectionName ) == 0 ) {
			if ( name )
				*name = p->name;
			return p->val;
		}
	}
	return NULL;
}

/**
 * Save the configuration to a file. The config parameters are held and updated in an array.
 * To make the settings persistant, this function has to be called. 
 *
 */

EXPORT void wPrefFlush(
		void )
/*
*/
{
	prefs_t * p;
	char tmp[BUFSIZ];
    const char *workDir;
	FILE * prefFile;

	if (!prefInitted)
		return;
	
	workDir = wGetAppWorkDir();
	sprintf( tmp, "%s/%s.rc", workDir, wConfigName );
	prefFile = fopen( tmp, "w" );
	if (prefFile == NULL)
		return;

	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		fprintf( prefFile,  "%s.%s: %s\n", p->section, p->name, p->val );
	}
	fclose( prefFile );
}


EXPORT void wPrefReset(
		void )
/*
*/
{
	prefs_t * p;

	prefInitted = FALSE;
	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if (p->section)
			free( p->section );
		if (p->name)
			free( p->name );
		if (p->val)
			free( p->val );
	}
	prefs_da.cnt = 0;
}
