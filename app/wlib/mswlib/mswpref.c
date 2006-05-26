#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include "mswint.h"

#if _MSC_VER >=1400
	#define stricmp _stricmp
#endif

char * mswStrdup( const char * );
static char appLibDirName[1024];
static char appWorkDirName[1024];


const char * wGetAppLibDir( void )
{
	char *cp;
	if ( appLibDirName[0] == 0 ) {
		GetModuleFileName( mswHInst, appLibDirName, sizeof appLibDirName );
		cp = strrchr( appLibDirName, '\\' );
		if (cp)
			*cp = '\0';
	}
	return appLibDirName;
}


const char * wGetAppWorkDir( void )
{
	char *cp;
	int rc;
	if ( appWorkDirName[0] != 0 ) {
		return appWorkDirName;
	}
	wGetAppLibDir();
	sprintf( mswTmpBuff, "%s\\xtrkcad0.ini", appLibDirName );
	rc = GetPrivateProfileString( "workdir", "path", "", appWorkDirName, sizeof appWorkDirName, mswTmpBuff );
	if ( rc!=0 ) {
		if ( stricmp( appWorkDirName, "installdir" ) == 0 ) {
			strcpy( appWorkDirName, appLibDirName );
		} else {
			cp = &appWorkDirName[strlen(appWorkDirName)-1];
			while (cp>appWorkDirName && *cp == '\\') *cp-- = 0;
		}
		return appWorkDirName;
	}
	if (GetWindowsDirectory( appWorkDirName, sizeof appWorkDirName ) == 0) {
			wNotice( "Cannot get windows working directory", "Exit", NULL );
			wExit(0);
	}
	return appWorkDirName;
}


typedef struct {
		char * section;
		char * name;
		BOOL_T present;
		BOOL_T dirty;
		char * val;
		} prefs_t;
dynArr_t prefs_da;
#define prefs(N) DYNARR_N(prefs_t,prefs_da,N)

void wPrefSetString( const char * section, const char * name, const char * sval )
{
	prefs_t * p;
	
	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if ( strcmp( p->section, section ) == 0 && strcmp( p->name, name ) == 0 ) {
			if (p->val)
				free(p->val);
			p->dirty = TRUE;
			p->val = mswStrdup( sval );
			return;
		}
	}
	DYNARR_APPEND( prefs_t, prefs_da, 10 );
	p = &prefs(prefs_da.cnt-1);
	p->name = mswStrdup(name);
	p->section = mswStrdup(section);
	p->dirty = TRUE;
	p->val = mswStrdup(sval);
}


const char * wPrefGetString( const char * section, const char * name )
{
	prefs_t * p;
	int rc;
	
	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if ( strcmp( p->section, section ) == 0 && strcmp( p->name, name ) == 0 ) {
			return p->val;
		}
	}
	rc = GetPrivateProfileString( section, name, "", mswTmpBuff, sizeof mswTmpBuff, mswProfileFile );
	if (rc==0)
		return NULL;
	DYNARR_APPEND( prefs_t, prefs_da, 10 );
	p = &prefs(prefs_da.cnt-1);
	p->name = mswStrdup(name);
	p->section = mswStrdup(section);
	p->dirty = FALSE;
	p->val = mswStrdup(mswTmpBuff);
	return p->val;
}


void wPrefSetInteger( const char * section, const char * name, long lval )
{
	char tmp[20];
	
	sprintf( tmp, "%ld", lval );
	wPrefSetString( section, name, tmp );
}


wBool_t wPrefGetInteger(
		const char * section,
		const char * name,
		long *res,
		long def )
{
	const char * cp;
        char * cp1;

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


void wPrefSetFloat(
		const char * section,			/* Section */
		const char * name,			/* Name */
		double lval )			/* Value */
/*
*/
{
	char tmp[20];

	sprintf(tmp, "%0.3f", lval );
	wPrefSetString( section, name, tmp );
}


wBool_t wPrefGetFloat(
		const char * section,			/* Section */
		const char * name,			/* Name */
		double * res,			/* Address of result */
		double def )			/* Default value */
/*
*/
{
	const char * cp;
        char * cp1;

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


void wPrefFlush( void )
{
	prefs_t * p;
	
	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
	  if ( p->dirty )
		   WritePrivateProfileString( p->section, p->name, p->val, mswProfileFile );
	}
	WritePrivateProfileString( NULL, NULL, NULL, mswProfileFile );
}


void wPrefReset(
		void )
/*
*/
{
	prefs_t * p;

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
