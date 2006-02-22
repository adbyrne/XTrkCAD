/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/wpref.c,v 1.2 2006-02-22 19:20:11 m_fischer Exp $
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
#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

extern char wAppName[];
static char appLibDir[BUFSIZ];
static char appWorkDir[BUFSIZ];

/*
 *******************************************************************************
 *
 * Get Dir Names
 *
 *******************************************************************************
 */


EXPORT const char * wGetAppLibDir(
		void )
/*
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
		"%s cannot find the directory containing the configuration files.\n\n"
		"Make sure that these files installed are in either \n"
		"  /usr/lib/%s or\n"
		"  /usr/local/lib/%s\n"
		"If this is not possible, the environment variable %s must contain "
		"the name of the correct directory!",
		wAppName, wAppName, wAppName, envvar );
	wNotice( msg, "Ok", NULL );
	appLibDir[0] = '\0';
	wExit(0);
	return NULL;
}

EXPORT const char * wGetAppWorkDir(
		void )
/*
*/
{
	char tmp[BUFSIZ+20];
	char * homeDir;
	DIR *dirp;
	
	if (appWorkDir[0] != '\0')
		return appWorkDir;

	if ((homeDir = getenv( "HOME" )) == NULL) {
		wNotice( "HOME is not set", "Exit", NULL);
		wExit(0);
	}
	sprintf( appWorkDir, "%s/.%s", homeDir, wAppName );
	if ( (dirp = opendir(appWorkDir)) != NULL ) {
		closedir(dirp);
	} else {
		sprintf( tmp, "Creating %s", appWorkDir );
		if( !wNotice( tmp, "Ok", "Exit" ) ) {
			wExit(0);
		}
		if ( mkdir( appWorkDir, 0777 ) == -1 ) {
			sprintf( tmp, "Cannot create %s", appWorkDir );
			wNotice( tmp, "Exit", NULL );
			wExit(0);
		}
	}
	return appWorkDir;
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
	sprintf( tmp, "%s/%s.rc", workDir, wAppName );
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
			wNotice( tmp, "Continue", NULL );
			continue;
		}
		*np++ = '\0';
		while ( *np==' ' || *np=='\t' ) np++;
		vp = strchr( np, ':' );
		if (vp == NULL) {
			wNotice( tmp, "Continue", NULL );
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


EXPORT void wPrefSetString(
		const char * section,		/* Section */
		const char * name,		/* Name */
		const char * sval )		/* Value */
/*
*/
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


EXPORT void wPrefSetInteger(
		const char * section,		/* Section */
		const char * name,		/* Name */
		long lval )		/* Value */
/*
*/
{
	char tmp[20];

	sprintf(tmp, "%ld", lval );
	wPrefSetString( section, name, tmp );
}


EXPORT wBool_t wPrefGetInteger(
		const char * section,		/* Section */
		const char * name,		/* Name */
		long * res,		/* Address of result */
		long def )		/* Default value */
/*
*/
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


EXPORT void wPrefSetFloat(
		const char * section,		/* Section */
		const char * name,		/* Name */
		double lval )		/* Value */
/*
*/
{
	char tmp[20];

	sprintf(tmp, "%0.3f", lval );
	wPrefSetString( section, name, tmp );
}


EXPORT wBool_t wPrefGetFloat(
		const char * section,		/* Section */
		const char * name,		/* Name */
		double * res,		/* Address of result */
		double def )		/* Default value */
/*
*/
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
	sprintf( tmp, "%s/%s.rc", workDir, wAppName );
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
