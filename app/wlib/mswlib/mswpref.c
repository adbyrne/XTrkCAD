#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <stdio.h>
#include "mswint.h"
#include <shlobj.h>
#include <Shlwapi.h>

#if _MSC_VER >=1400
#define stricmp _stricmp
#endif

char * mswStrdup( const char * );
static char appLibDirName[MAX_PATH];
static char appWorkDirName[MAX_PATH];

/**
 * Get the location of the shared files (parameters, help file, etc. ): This location is
 * derived from the modulename, ie. the directory where the exe is installed.
 * For an instalaltion directory of somedir/bin/xtrkcad.exe the library directory is
 * somedir/share/xtrkcad/
 */

const char * wGetAppLibDir( void )
{
	char *cp;
	char module_name[MAX_PATH];

	if (appLibDirName[0] != '\0') {
		return appLibDirName;
	}

	GetModuleFileName( mswHInst, module_name, sizeof module_name );
	cp = strrchr( module_name, '\\' );
	if (cp) {
		*cp = '\0';
	}

#ifdef XTRKCAD_CMAKE_BUILD
	strncpy(appLibDirName, module_name, sizeof(appLibDirName));
	size_t len = sizeof(appLibDirName)-strlen(appLibDirName)-1;
	strncat(appLibDirName, "\\..\\share\\xtrkcad", len);
	_fullpath( appLibDirName, appLibDirName, MAX_PATH );
	return appLibDirName;
#endif

	strncpy(appLibDirName, module_name, sizeof(appLibDirName));
	appLibDirName[sizeof(appLibDirName)-1] = '\0';
	return appLibDirName;
}


/**
 * Gets the working directory for the application. At least the INI file is stored here.
 * The working directory can be specified manually by creating a file called xtrkcad0.ini
 * in the application lib dir (the directory where the .EXE is located).
 *
 * [workdir]
 *		path=somepath
 *
 * when somepath is set to the keyword "installdir", the install directory for the EXE is
 * used.
 *
 * If no xtrkcad0.ini could be found, the user settings directory (appdata) is used.
 *
 */
const char * wGetAppWorkDir( void )
{
	char *cp;
	int rc;
	if ( appWorkDirName[0] != 0 ) {
		return appWorkDirName;
	}
	wGetAppLibDir();
	snprintf( mswTmpBuff, sizeof(mswTmpBuff), "%s\\xtrkcad0.ini", appLibDirName );
	rc = GetPrivateProfileString( "workdir", "path", "", appWorkDirName,
	                              sizeof appWorkDirName, mswTmpBuff );
	if ( rc!=0 ) {
		if ( stricmp( appWorkDirName, "installdir" ) == 0 ) {
			strncpy( appWorkDirName, appLibDirName, sizeof(appWorkDirName) );
			appWorkDirName[sizeof(appWorkDirName)-1] = '\0';
		} else {
			cp = &appWorkDirName[strlen(appWorkDirName)-1];
			while (cp>appWorkDirName && *cp == '\\') { *cp-- = 0; }
		}
		return appWorkDirName;
	}

	if (SHGetSpecialFolderPath( NULL, mswTmpBuff, CSIDL_APPDATA, 0 ) == 0 ) {
		wNoticeEx( NT_ERROR, "Cannot get user's profile directory", "Exit", NULL );
		wExit(0);
	} else {
		snprintf( appWorkDirName, sizeof(appWorkDirName), "%s\\%s", mswTmpBuff,
		          "XTrackCad" );
		if( !PathIsDirectory( appWorkDirName )) {
			if( !CreateDirectory( appWorkDirName, NULL )) {
				wNoticeEx( NT_ERROR, "Cannot create user's profile directory", "Exit", NULL );
				wExit(0);
			}
		}
	}

	return appWorkDirName;
}

/** Get the user's Documents directory.  
 *
 * \return    pointer to the user's Documents directory
 */

const char *wGetUserHomeDir( void )
{
	if (SHGetSpecialFolderPath( NULL, mswTmpBuff, CSIDL_PERSONAL, 0 ) == 0 ) {
		wNoticeEx( NT_ERROR, "Cannot get user's documents directory", "Exit", NULL );
		wExit(0);
		return( NULL );
	} else {
		return( mswTmpBuff );
	}
}

typedef struct {
	char * section;
	char * name;
	BOOL_T present;
	BOOL_T dirty;
	char * val;
} prefs_t;

static dynArr_t prefs_da;
#define prefs(N) DYNARR_N(prefs_t,prefs_da,N)

void wPrefSetString( const char * section, const char * name,
                     const char * sval )
{
	prefs_t * p;

	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if ( strcmp( p->section, section ) == 0 && strcmp( p->name, name ) == 0 ) {
			if (p->val) {
				free(p->val);
			}
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

void wPrefsLoad(char * name)
{
	prefs_t *p;
	for (int i= 0; i<prefs_da.cnt; i++) {
		p = &prefs(i);
		if (!name || !name[0]) { name = mswProfileFile; }
		int rc = GetPrivateProfileString( p->section, p->name, "", mswTmpBuff,
		                                  sizeof mswTmpBuff, name );
		if (rc==0) {
			continue;
		}
		p->val = mswStrdup(mswTmpBuff);
	}
}

char * wPrefGetStringBasic( const char * section, const char * name )
{
	prefs_t * p;
	int rc;

	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if ( strcmp( p->section, section ) == 0 && strcmp( p->name, name ) == 0 ) {
			return p->val;
		}
	}

	rc = GetPrivateProfileString( section, name, "", mswTmpBuff, sizeof mswTmpBuff,
	                              mswProfileFile );
	if (rc==0) {
		return NULL;
	}
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

	snprintf( tmp, sizeof(tmp), "%ld", lval );
	wPrefSetString( section, name, tmp );
}


wBool_t wPrefGetIntegerBasic(
        const char * section,
        const char * name,
        long *res,
        long def )
{
	const char * cp;
	char * cp1;

	cp = wPrefGetStringBasic( section, name );
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

	snprintf(tmp, sizeof(tmp), "%0.6f", lval );
	wPrefSetString( section, name, tmp );
}


wBool_t wPrefGetFloatBasic(
        const char * section,			/* Section */
        const char * name,			/* Name */
        double * res,			/* Address of result */
        double def )			/* Default value */
/*
*/
{
	const char * cp;
	char * cp1;

	cp = wPrefGetStringBasic( section, name );
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


void wPrefFlush( char * name )
{
	prefs_t * p;

	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if (name && name[0]) {
			WritePrivateProfileString( p->section, p->name, p->val, name );
		} else if (p->dirty) {
			WritePrivateProfileString( p->section, p->name, p->val, mswProfileFile );
		}
	}
	if (name && name[0]) {
		WritePrivateProfileString( NULL, NULL, NULL, name );
	} else {
		WritePrivateProfileString( NULL, NULL, NULL, mswProfileFile );
	}
}


void wPrefReset(
        void )
/*
*/
{
	prefs_t * p;

	for (p=&prefs(0); p<&prefs(prefs_da.cnt); p++) {
		if (p->section) {
			free( p->section );
		}
		if (p->name) {
			free( p->name );
		}
		if (p->val) {
			free( p->val );
		}
	}
	prefs_da.cnt = 0;
}

/**
 * Split a line from the config file ie. an ini-file into separate tokens. The
 * line is split into sections, name of value and value following. Pointers
 * to the respective token are returned. These are zero-terminated. 
 * If a token is not present, NULL is returned instead. 
 * The input line is modified.
 * 
 * \param line		input line, modified during excution of function
 * \param section	section if present
 * \param name		name of config value if present
 * \param value		name of value if present
 */

void
wPrefTokenize(char* line, char** section, char** name, char** value)
{
	*section = NULL;
	*name = NULL;
	*value = NULL;

	if (*line == '[') {
		*section = strtok(line, "[]");
	} else {
		*name = strtok(line, "=");
		*value = strtok(NULL, "\n");		
	}
}

/**
 * A valid line for a config file is created from the individual elements. 
 * Values not need for specific statement are ignored. Eg. when section is 
 * present, name and value are not used. 
 * The caller has to make sure, that the return buffer is large enough.
 * 
 * \param section	section, returned inside squared brackets
 * \param name		name, left side of '='
 * \param value		value, right side of '='
 * \param result	pointer to buffer for formated line. 
 */

void
wPrefFormatLine(char* section, char* name, char* value, char* result)
{
	if (section) {
		sprintf(result, "[%s]", section);
	}
	else {
		sprintf(result, "%s=%s", name, value);
	}
}

