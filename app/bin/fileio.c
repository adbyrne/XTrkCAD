/** \file fileio.c
 * Loading and saving files. Handles trackplans, XTrackCAD exports and cut/paste
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

#include <stdlib.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#endif
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifdef WINDOWS
#include <io.h>
#include <windows.h>
	#if _MSC_VER >=1400
		#define strdup _strdup
	#endif
#else
#endif
#include <sys/stat.h>
#include <stdarg.h>
#include <locale.h>

#include <stdint.h>

#include <assert.h>

#include "track.h"
#include "version.h"
#include "common.h"
#include "utility.h"
#include "draw.h"
#include "misc.h"
#include "compound.h"
#include "i18n.h"

/*#define TIME_READTRACKFILE*/

EXPORT const char * workingDir;
EXPORT const char * libDir;

static char * customPath = NULL;
static char * customPathBak = NULL;

EXPORT char curPathName[STR_LONG_SIZE];
EXPORT char * curFileName;
EXPORT char curDirName[STR_LONG_SIZE];

EXPORT char * clipBoardN;

EXPORT wBool_t executableOk = FALSE;

static int log_paramFile;

/**
 * Get the directory from the current file and store it as current directory 
 * in a global variable and the preferences
 *
 * \param pathType IN possible enhancement for file type specific directorys
 * \param fileName IN fully qualified filename
 * \return 
 *
 * \todo split directory and keep directory part
 */

void SetCurrentPath(
	const char * pathType,
	const char * fileName )
{
	char *path;
	char *copy;

	assert( fileName != NULL );
	assert( pathType != NULL );

	copy = strdup( fileName );
	path = strrchr(copy, FILE_SEP_CHAR[ 0 ] );
	if ( path ) 
	{
		*path = '\0';
		strcpy( curDirName, copy );
		wPrefSetString( "file", "directory", curDirName );
	}    
	free( copy );
}

/**
 * Find the filename/extension piece in a fully qualified path
 *
 * \param path IN the full path
 * \return pointer to the filename part
 */

char *FindName( char *path )
{
	char *name; 
	name = strrchr( path, FILE_SEP_CHAR[0] );
	if (name) {
		name++;
	} else {
		name = path;
	}
	return(name );
}

#ifdef WINDOWS
#define rename( F1, F2 ) Copyfile( F1, F2 )

static int Copyfile( char * fn1, char * fn2 )
{
	FILE *f1, *f2;
	size_t size;
	f1 = fopen( fn1, "r" );
	if ( f1 == NULL )
		return 0;
	f2 = fopen( fn2, "w" );
	if ( f2 == NULL ) {
		fclose( f1 );
		return -1;
	}
	while ( (size=fread( message, 1, sizeof message, f1 )) > 0 )
		fwrite( message, size, 1, f2 );
	fclose( f1 );
	fclose( f2 );
	return 0;
}
#endif

/**
 * Save the old locale and set to new.
 *
 * \param newlocale IN the new locale to set
 * \return    pointer to the old locale
 */

char *
SaveLocale( char *newLocale )
{
	char *oldLocale;
	char *saveLocale = NULL;

	/* get old locale setting */
	oldLocale = setlocale(LC_ALL, NULL);

	/* allocate memory to save */
	if (oldLocale)
		saveLocale = strdup( oldLocale );

	setlocale(LC_ALL, newLocale );

	return( saveLocale );
}

/**
 * Restore a previously saved locale.
 *
 * \param locale IN return value from earlier call to SaveLocale
 */

void
RestoreLocale( char * locale )
{
	if( locale ) {
		setlocale( LC_ALL, locale );
		free( locale );
	}
}


/****************************************************************************
 *
 * PARAM FILE INPUT
 *
 */

EXPORT FILE * paramFile = NULL;
EXPORT char paramFileName[STR_LONG_SIZE];
EXPORT wIndex_t paramLineNum = 0;
EXPORT char paramLine[STR_LONG_SIZE];
EXPORT char * curContents;
EXPORT char * curSubContents;
static long paramCheckSum;

#define PARAM_DEMO (-1)

typedef struct {
		char * name;
		readParam_t proc;
		} paramProc_t;
static dynArr_t paramProc_da;
#define paramProc(N) DYNARR_N( paramProc_t, paramProc_da, N )


EXPORT void Stripcr( char * line )
{
	char * cp;
	cp = line + strlen(line);
	if (cp == line)
		return;
	cp--;
	if (*cp == '\n')
		*cp-- = '\0';
	if (cp >= line && *cp == '\r')
		*cp = '\0';
}

EXPORT void ParamCheckSumLine( char * line )
{
	long mult=1;
	while ( *line )
		paramCheckSum += (((long)(*line++))&0xFF)*(mult++);
}

EXPORT char * GetNextLine( void )
{
	if (!paramFile) {
		paramLine[0] = '\0';
		return NULL;
	}
	if (fgets( paramLine, sizeof paramLine, paramFile ) == NULL) {
		AbortProg( "Permature EOF on %s", paramFileName );
	}
	Stripcr( paramLine );
	ParamCheckSumLine( paramLine );
	paramLineNum++;
	return paramLine;
}


/**
 * Show an error message if problems occur during loading of a param or layout file.
 * The user has the choice to cancel the operation or to continue. If operation is
 * canceled the open file is closed.
 *
 * \param IN msg error message
 * \param IN showLine set to true if current line should be included in error message
 * \param IN ... variable number additional error information
 * \return TRUE to continue, FALSE to abort operation
 *
 */

EXPORT int InputError(
		char * msg,
		BOOL_T showLine,
		... )
{
	va_list ap;
	char * mp = message;
	int ret;

	mp += sprintf( message, "INPUT ERROR: %s:%d\n",
		paramFileName, paramLineNum );
	va_start( ap, showLine );
	mp += vsprintf( mp, msg, ap );
	va_end( ap );
	if (showLine) {
		*mp++ = '\n';
		strcpy( mp, paramLine );
	}
	strcat( mp, _("\nDo you want to continue?") );
	if (!(ret = wNoticeEx( NT_ERROR, message, _("Continue"), _("Stop") ))) {
		if ( paramFile )
			fclose(paramFile);
		paramFile = NULL;
	}
	return ret;
}


EXPORT void SyntaxError(
		char * event,
		wIndex_t actual,
		wIndex_t expected )
{
	InputError( "%s scan returned %d (expected %d)",
		TRUE, event, actual, expected );
}

/**
 * Parse a line in XTrackCAD's file format
 *
 * \param line IN line to parse
 * \param format IN ???
 *
 * \return FALSE in case of parsing error, TRUE on success
 */

EXPORT BOOL_T GetArgs(
		char * line,
		char * format,
		... )
{
	char * cp, * cq;
	int argNo;
	long * pl;
	unsigned long *pul;
	int * pi;
	FLOAT_T *pf;
	coOrd p, *pp;
	char * ps;
	char ** qp;
	va_list ap;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");

	cp = line;
	va_start( ap, format );
	for (argNo=1;*format;argNo++,format++) {
		while (isspace((unsigned char)*cp)) cp++;
		if (!*cp && strchr( "XZYzc", *format ) == NULL ) {
			RestoreLocale(oldLocale);
			InputError( "Arg %d: EOL unexpected", TRUE, argNo );
			return FALSE;
		}
		switch (*format) {
		case '0':
			(void)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'X':
			pi = va_arg( ap, int * );
			*pi = 0;
			break;
		case 'Z':
			pl = va_arg( ap, long * );
			*pl = 0;
			break;
		case 'Y':
			pf = va_arg( ap, FLOAT_T * );
			*pf = 0;
			break;
		case 'L':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'd':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'w':
			pf = va_arg( ap, FLOAT_T * );
			*pf = (FLOAT_T)strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			if (*cq == '.')
				*pf = strtod( cp, &cq );
			else
				*pf /= mainD.dpi;
			cp = cq;
			break;
		case 'u':
			pul = va_arg( ap, unsigned long * );
			*pul = strtoul( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'l':
			pl = va_arg( ap, long * );
			*pl = strtol( cp, &cq, 10 );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected integer", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'f':
			pf = va_arg( ap, FLOAT_T * );
			*pf = strtod( cp, &cq );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected float", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			break;
		case 'z':
			pf = va_arg( ap, FLOAT_T * );
#ifdef LATER
			if ( paramVersion >= 9 ) {
				*pf = strtod( cp, &cq );
				if (cp == cq) {
					RestoreLocale(oldLocale);
					InputError( "Arg %d: expected float", TRUE, argNo );
					return FALSE;
				}
				cp = cq;
			} else {
				*pf = 0.0;
			}
#endif
			*pf = 0.0;
			break;
		case 'p':
			pp = va_arg( ap, coOrd * );
			p.x = strtod( cp, &cq );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected float", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			p.y = strtod( cp, &cq );
			if (cp == cq) {
				RestoreLocale(oldLocale);
				InputError( "Arg %d: expected float", TRUE, argNo );
				return FALSE;
			}
			cp = cq;
			*pp = p;
			break;
		case 's':
			ps = va_arg( ap, char * );
			while (isspace((unsigned char)*cp)) cp++;
			while (*cp && !isspace((unsigned char)*cp)) *ps++ = *cp++;
			*ps++ = '\0';
			break;
		case 'q':
			qp = va_arg( ap, char * * );
			if (*cp != '\"')
				/* Stupid windows */
				cq = strchr( cp, '\"' );
			else
				cq = cp;
			if (cq!=NULL) {
				cp = cq;
				ps = &message[0];
				cp++;
				while (*cp) {
					if ( (ps-message)>=sizeof message)
						AbortProg( "Quoted title argument too long" );
					if (*cp == '\"') {
						if (*++cp == '\"') {
							*ps++ = '\"';
						} else {
							*ps = '\0';
							cp++;
							break;
						}
					} else {
						*ps++ = *cp;
					}
					cp++;
				}
				*ps = '\0';
			} else {
				message[0] = '\0';
			}
			*qp = (char*)MyStrdup(message);
			break;
		case 'c':
			qp = va_arg( ap, char * * );
			while (isspace((unsigned char)*cp)) cp++;
			if (*cp)
				*qp = cp;
			else
				*qp = NULL;
			break;
		default:
			AbortProg( "getArgs: bad format char" );
		}
	}
	va_end( ap );
	RestoreLocale(oldLocale);
	return TRUE;
}

EXPORT wBool_t ParseRoomSize(
		char * s,
		coOrd * roomSizeRet )
{
	coOrd size;
	char *cp;

	size.x = strtod( s, &cp );
	if (cp != s) {
		s = cp;
		while (isspace((unsigned char)*s)) s++;
		if (*s == 'x' || *s == 'X') {
			size.y = strtod( ++s, &cp );
			if (cp != s) {
#ifdef LATER
				if (units == UNITS_METRIC) {
					size.x /= 2.54;
					size.y /= 2.54;
				}
#endif
				*roomSizeRet = size;
				return TRUE;
			}
		}
	}
	return FALSE;
}


EXPORT void AddParam(
		char * name,
		readParam_t proc )
{
	DYNARR_APPEND( paramProc_t, paramProc_da, 10 );
	paramProc(paramProc_da.cnt-1).name = name;
	paramProc(paramProc_da.cnt-1).proc = proc;
}


EXPORT BOOL_T ReadParams(
		long key,
		const char * dirName,
		const char * fileName )
{
	FILE * oldFile;
	char *cp;
	wIndex_t oldLineNum;
	wIndex_t pc;
	long oldCheckSum;
	long checkSum=0;
	BOOL_T checkSummed;
	long paramVersion = -1;
	char *oldLocale = NULL;

	if (dirName) {
		strcpy( paramFileName, dirName );
		strcat( paramFileName, FILE_SEP_CHAR );
		strcat( paramFileName, fileName );
	} else {
		strcpy( paramFileName, fileName );
	}
	paramLineNum = 0;
	curBarScale = -1;
	curContents = strdup( fileName );
	curSubContents = curContents;

LOG1( log_paramFile, ("ReadParam( %s )\n", fileName ) )

	oldLocale = SaveLocale("C");

	paramFile = fopen( paramFileName, "r" );
	if (paramFile == NULL) {
		/* Reset the locale settings */
		RestoreLocale( oldLocale );

		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Parameter"), paramFileName, strerror(errno) );

		return FALSE;
	}
	paramCheckSum = key;
	paramLineNum = 0;
	checkSummed = FALSE;
	while ( paramFile && ( fgets(paramLine, 256, paramFile) ) != NULL ) {
		paramLineNum++;
		Stripcr( paramLine );
		if (strncmp( paramLine, "CHECKSUM ", 9 ) == 0) {
			checkSum = atol( paramLine+9 );
			checkSummed = TRUE;
			goto nextLine;
		}
		ParamCheckSumLine( paramLine );
		if (paramLine[0] == '#') {
			/* comment */
		} else if (paramLine[0] == 0) {
			/* empty paramLine */
		} else if (strncmp( paramLine, "INCLUDE ", 8 ) == 0) {
			cp = &paramLine[8];
			while (*cp && isspace((unsigned char)*cp)) cp++;
			if (!*cp) {
				InputError( "INCLUDE - no file name", TRUE );

				/* Close file and reset the locale settings */
				if (paramFile) fclose(paramFile);
				RestoreLocale( oldLocale );

				return FALSE;
			}
			oldFile = paramFile;
			oldLineNum = paramLineNum;
			oldCheckSum = paramCheckSum;
			ReadParams( key, dirName, cp );
			paramFile = oldFile;
			paramLineNum = oldLineNum;
			paramCheckSum = oldCheckSum;
			if (dirName) {
				strcpy( paramFileName, dirName );
				strcat( paramFileName, FILE_SEP_CHAR );
				strcat( paramFileName, fileName );
			} else {
				strcpy( paramFileName, fileName );
			}
		} else if (strncmp( paramLine, "CONTENTS ", 9) == 0 ) {
			curContents = MyStrdup( paramLine+9 );
			curSubContents = curContents;
		} else if (strncmp( paramLine, "SUBCONTENTS ", 12) == 0 ) {
			curSubContents = MyStrdup( paramLine+12 );
		} else if (strncmp( paramLine, "PARAM ", 6) == 0 ) {
			paramVersion = atol( paramLine+6 );
		} else {
			for (pc = 0; pc < paramProc_da.cnt; pc++ ) {
				if (strncmp( paramLine, paramProc(pc).name,
							 strlen(paramProc(pc).name)) == 0 ) {
					paramProc(pc).proc( paramLine );
					goto nextLine;
				}
			}
			InputError( "Unknown param line", TRUE );
		}
 nextLine:;
	}
	if ( key ) {
		if ( !checkSummed || checkSum != paramCheckSum ) {
			/* Close file and reset the locale settings */
			if (paramFile) fclose(paramFile);
			RestoreLocale( oldLocale );

			NoticeMessage( MSG_PROG_CORRUPTED, _("Ok"), NULL, paramFileName );

			return FALSE;
		}
	}
	if (paramFile)fclose( paramFile );

	RestoreLocale( oldLocale );

	return TRUE;
}


static void ReadCustom( void )
{
	FILE * f;
	customPath =
		(char*)MyMalloc( strlen(workingDir) + 1 + strlen(sCustomF) + 1 );
	sprintf( customPath, "%s%s%s", workingDir, FILE_SEP_CHAR, sCustomF );
	customPathBak = MyStrdup( customPath );
	customPathBak[ strlen(customPathBak)-1 ] = '1';
	f = fopen( customPath, "r" );
	if ( f != NULL ) {
		fclose( f );
		curParamFileIndex = PARAM_CUSTOM;
		ReadParams( 0, workingDir, sCustomF );
	}
}


/*
 * Open the file and then set the locale to "C". Old locale will be copied to
 * oldLocale. After the required file I/O is done, the caller must call
 * CloseCustom() with the same locale value that was returned in oldLocale by
 * this function.
 */
EXPORT FILE * OpenCustom( char *mode )
{
	FILE * ret = NULL;

	if (inPlayback)
		return NULL;
	if ( *mode == 'w' )
		rename( customPath, customPathBak );
	if (customPath) {
		ret = fopen( customPath, mode );
		if (ret == NULL) {
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Custom"), customPath, strerror(errno) );
		}
	}

	return ret;
}


EXPORT char * PutTitle( char * cp )
{
	static char title[STR_SIZE];
	char * tp = title;
	while (*cp && (tp-title)<=(sizeof title)-3) {
		if (*cp == '\"') {
			*tp++ = '\"';
			*tp++ = '\"';
		} else {
			*tp++ = *cp;
		}
		cp++;
	}
	if ( *cp )
		NoticeMessage( _("putTitle: title too long: %s"), _("Ok"), NULL, title );
	*tp = '\0';
	return title;
}

/**
 * Set the title of the main window. After loading a file or changing a design
 * this function is called to set the filename and the changed mark in the title
 * bar.
 */

void SetWindowTitle( void )
{
	if ( changed > 2 || inPlayback )
		return;
	sprintf( message, "%s%s - %s(%s)",
		(curFileName==NULL||curFileName[0]=='\0')?_("Unnamed Trackplan"):curFileName,
		changed>0?"*":"", sProdName, sVersion );
	wWinSetTitle( mainW, message );
}

/*****************************************************************************
 *
 * LOAD / SAVE TRACKS
 *
 */

static struct wFilSel_t * loadFile_fs;
static struct wFilSel_t * saveFile_fs;

static wWin_p checkPointingW;
static paramData_t checkPointingPLs[] = {
   {    PD_MESSAGE, N_("Check Pointing") } };
static paramGroup_t checkPointingPG = { "checkpoint", 0, checkPointingPLs, sizeof checkPointingPLs/sizeof checkPointingPLs[0] };

static char * checkPtFileName1;
static char * checkPtFileName2;

/** Read the layout design.
 *
 * \param IN pathName filename including directory
 * \param IN fileName pointer to filename part in pathName
 * \param IN full
 * \param IN noSetCurDir if FALSE current diurectory is changed to file location
 * \param IN complain  if FALSE error messages are supressed
 *
 * \return FALSE in case of load error
 */

static BOOL_T ReadTrackFile(
		const char * pathName,
		const char * fileName,
		BOOL_T full,
		BOOL_T noSetCurDir,
		BOOL_T complain )
{
	int count;
	coOrd roomSize;
	long scale;
	char * cp;
	char *oldLocale = NULL;
	int ret = TRUE;

	oldLocale = SaveLocale( "C" );

	paramFile = fopen( pathName, "r" );
	if (paramFile == NULL) {
		/* Reset the locale settings */
		RestoreLocale( oldLocale );

		if ( complain )
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, sProdName, pathName, strerror(errno) );

		return FALSE;
	}

	paramLineNum = 0;
	strcpy( paramFileName, fileName );

	InfoMessage("0");
	count = 0;
	while ( paramFile && ( fgets(paramLine, sizeof paramLine, paramFile) ) != NULL ) {
		count++;
		if (count%10 == 0) {
			InfoMessage( "%d", count );
			wFlush();
		}
		paramLineNum++;
		if (strlen(paramLine) == (sizeof paramLine) -1 &&
			paramLine[(sizeof paramLine)-1] != '\n') {
			if( !(ret = InputError( "Line too long", TRUE )))
				break;
		}
		Stripcr( paramLine );
		if (paramLine[0] == '#' ||
			paramLine[0] == '\n' ||
			paramLine[0] == '\0' ) {
			/* comment */
			continue;
		}

		if (ReadTrack( paramLine )) {

		} else if (strncmp( paramLine, "END", 3 ) == 0) {
			break;
		} else if (strncmp( paramLine, "VERSION ", 8 ) == 0) {
			paramVersion = strtol( paramLine+8, &cp, 10 );
			if (cp)
				while (*cp && isspace((unsigned char)*cp)) cp++;
			if ( paramVersion > iParamVersion ) {
				if (cp && *cp) {
					NoticeMessage( MSG_UPGRADE_VERSION1, _("Ok"), NULL, paramVersion, iParamVersion, sProdName, cp );
				} else {
					NoticeMessage( MSG_UPGRADE_VERSION2, _("Ok"), NULL, paramVersion, iParamVersion, sProdName );
				}
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_BAD_FILE_VERSION, _("Ok"), NULL, paramVersion, iMinParamVersion, sProdName );
				break;
			}
		} else if (!full) {
			if( !(ret = InputError( "unknown command", TRUE )))
				break;
		} else if (strncmp( paramLine, "TITLE1 ", 7 ) == 0) {
			strncpy( Title1, &paramLine[7], TITLEMAXLEN );
			Title1[ TITLEMAXLEN - 1 ] = '\0';
			/*wStringSetValue( title1PD.control, Title1 );*/
		} else if (strncmp( paramLine, "TITLE2 ", 7 ) == 0) {
			strncpy( Title2, &paramLine[7], TITLEMAXLEN );
			Title2[ TITLEMAXLEN - 1 ] = '\0';
			/*wStringSetValue( title2PD.control, Title2 );*/
		} else if (strncmp( paramLine, "ROOMSIZE", 8 ) == 0) {
			if ( ParseRoomSize( paramLine+8, &roomSize ) ) {
				SetRoomSize( roomSize );
				/*wFloatSetValue( roomSizeXPD.control, PutDim(roomSize.x) );*/
				/*wFloatSetValue( roomSizeYPD.control, PutDim(roomSize.y) );*/
			} else {
				if( !(ret = InputError( "ROOMSIZE: bad value", TRUE )))
					break;
			}
		} else if (strncmp( paramLine, "SCALE ", 6 ) == 0) {
			if ( !DoSetScale( paramLine+5 ) ) {
				if( !(ret = InputError( "SCALE: bad value", TRUE )))
					break;
			}
		} else if (strncmp( paramLine, "MAPSCALE ", 9 ) == 0) {
			scale = atol( paramLine+9 );
			if (scale > 1) {
				mapD.scale = mapScale = scale;
			}
		} else if (strncmp( paramLine, "LAYERS ", 7 ) == 0) {
			ReadLayers( paramLine+7 );
		} else {
			if( !(ret = InputError( "unknown command", TRUE )))
				break;
		}
	}

	if (paramFile)
		fclose(paramFile);

	if( ret ) {
		if (!noSetCurDir)
			SetCurrentPath( LAYOUTPATHKEY, fileName );

		if (full) {
			strcpy( curPathName, pathName );
			curFileName = &curPathName[fileName-pathName];
			SetWindowTitle();
		}
	}

	RestoreLocale( oldLocale );

	paramFile = NULL;
	InfoMessage( "%d", count );
	return ret;
}


EXPORT int LoadTracks(
		int cnt,
		char **fileName,
		void * data)
{
#ifdef TIME_READTRACKFILE
	long time0, time1;
#endif
	char *nameOfFile;

	assert( fileName != NULL );
	assert( cnt == 1 ); 
	//if (fileName == NULL || cnt == 0 )
	//	return TRUE;

	paramVersion = -1;
	wSetCursor( wCursorWait );
	Reset();
	ClearTracks();
	ResetLayers();
	checkPtMark = changed = 0;
	UndoSuspend();
	useCurrentLayer = FALSE;
#ifdef TIME_READTRACKFILE
	time0 = wGetTimer();
#endif
	nameOfFile = FindName( fileName[ 0 ] );

	if (ReadTrackFile( fileName[ 0 ], nameOfFile, TRUE, FALSE, TRUE )) {
		wMenuListAdd( fileList_ml, 0, nameOfFile, MyStrdup(fileName[0]) );
		ResolveIndex();
#ifdef TIME_READTRACKFILE
		time1 = wGetTimer();
		printf( "time= %ld ms \n", time1-time0 );
#endif
		RecomputeElevations();
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );
		DoUpdateTitles();
		LoadLayerLists();
	}
	UndoResume();
	Reset();
	wSetCursor( wCursorNormal );
	return TRUE;
}

/**
 * Load the layout specified by data. Filename may contain a full
 * path.
 * \param index IN ignored
 * \param label IN ignored
 * \param data IN path and filename 
 */

EXPORT void DoFileList(
		int index,
		char * label,
		void * data )
{
	char *pathName = (char*)data;

	LoadTracks( 1, &pathName, NULL );
}


static BOOL_T DoSaveTracks(
		const char * fileName )
{
	FILE * f;
	time_t clock;
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;

	oldLocale = SaveLocale( "C" );

	f = fopen( fileName, "w" );
	if (f==NULL) {
		RestoreLocale( oldLocale );

		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Track"), fileName, strerror(errno) );

		return FALSE;
	}
	wSetCursor( wCursorWait );
	time(&clock);
	rc &= fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) )>0;
	rc &= fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION )>0;
	Stripcr( Title1 );
	Stripcr( Title2 );
	rc &= fprintf(f, "TITLE1 %s\n", Title1 )>0;
	rc &= fprintf(f, "TITLE2 %s\n", Title2 )>0;
	rc &= fprintf(f, "MAPSCALE %ld\n", (long)mapD.scale )>0;
	rc &= fprintf(f, "ROOMSIZE %0.6f x %0.6f\n", mapD.size.x, mapD.size.y )>0;
	rc &= fprintf(f, "SCALE %s\n", curScaleName )>0;
	rc &= WriteLayers( f );
	rc &= WriteMainNote( f );
	rc &= WriteTracks( f );
	rc &= fprintf(f, "END\n")>0;
	if ( !rc )
		NoticeMessage( MSG_WRITE_FAILURE, _("Ok"), NULL, strerror(errno), fileName );
	fclose(f);

	RestoreLocale( oldLocale );

	checkPtMark = changed;
	wSetCursor( wCursorNormal );
	return rc;
}


static doSaveCallBack_p doAfterSave;

static int SaveTracks(
		int cnt,
		char **fileName,
		void * data )
{
	char *nameOfFile;

	assert( fileName != NULL );
	assert( cnt == 1 );

	SetCurrentPath( LAYOUTPATHKEY, fileName[ 0 ] );
	DoSaveTracks( fileName[ 0 ] );

	nameOfFile = FindName( fileName[ 0 ] );
	wMenuListAdd( fileList_ml, 0, nameOfFile, MyStrdup(fileName[ 0 ]) );
	checkPtMark = changed = 0;

	if (strcmp(curPathName, fileName[ 0 ]))
	  strcpy( curPathName, fileName[ 0 ] );
	curFileName = FindName( curPathName );
	
	if (doAfterSave)
		doAfterSave();
	doAfterSave = NULL;
	return TRUE;
}


EXPORT void DoSave( doSaveCallBack_p after )
{
	doAfterSave = after;
	if (curPathName[0] == '\0') {
		if (saveFile_fs == NULL)
			saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks"),
				sSourceFilePattern, SaveTracks, NULL );
		wFilSelect( saveFile_fs, curDirName );
	} else {
		char *temp = curPathName; 
		SaveTracks( 1, &temp, NULL );
	}
	SetWindowTitle();
}

EXPORT void DoSaveAs( doSaveCallBack_p after )
{
	doAfterSave = after;
	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks As"),
			sSourceFilePattern, SaveTracks, NULL );
	wFilSelect( saveFile_fs, curDirName );
	SetWindowTitle();
}

EXPORT void DoLoad( void )
{
	loadFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Open Tracks"),
		sSourceFilePattern, LoadTracks, NULL );
	wFilSelect( loadFile_fs, curDirName );
}


EXPORT void DoCheckPoint( void )
{
	int rc;

	if (checkPointingW == NULL) {
		ParamRegister( &checkPointingPG );
		checkPointingW = ParamCreateDialog( &checkPointingPG, MakeWindowTitle(_("Check Pointing")), NULL, NULL, NULL, FALSE, NULL, F_TOP|F_CENTER, NULL );
	}
	rename( checkPtFileName1, checkPtFileName2 );
	wShow( checkPointingW );
	rc = DoSaveTracks( checkPtFileName1 );

	/* could the check point file be written ok? */
	if( rc ) {
		/* yes, delete the backup copy of the checkpoint file */
		remove( checkPtFileName2 );
	} else {
		/* no, rename the backup copy back to the checkpoint file name */
		rename( checkPtFileName2, checkPtFileName1 );
	}
	wHide( checkPointingW );
}

/**
 * Remove all temporary files before exiting.When the program terminates
 * normally through the exit choice, files that are created temporarily are removed:
 * xtrkcad.ckp
 *
 * \param none
 * \return none
 *
 */

EXPORT void CleanupFiles( void )
{
	if( checkPtFileName1 )
		remove( checkPtFileName1 );
}

/**
 * Check for existance of checkpoint file. Existance of a checkpoint file means that XTrkCAD was not properly
 * terminated.
 *
 * \param none
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int ExistsCheckpoint( void )
{
	int len;
	char *pattern = sCheckPointF;
	char *search;

	struct stat fileStat;

	len = strlen( workingDir ) + 1 + strlen( sCheckPointF ) + 1;
	checkPtFileName1 = (char*)MyMalloc(len);
	sprintf( checkPtFileName1, "%s%s%s", workingDir, FILE_SEP_CHAR, sCheckPointF );
	checkPtFileName2 = (char*)MyMalloc(len);
	sprintf( checkPtFileName2, "%s%s%s", workingDir, FILE_SEP_CHAR, sCheckPoint1F );

	len = strlen( workingDir ) + 1 + strlen( pattern ) + 1;
	search = (char*)MyMalloc(len);
	sprintf( search, "%s%s%s", workingDir, FILE_SEP_CHAR, pattern );

	if( !stat( search, &fileStat ) ) {
		MyFree( search );
		return TRUE;
	} else {
		MyFree( search );
		return FALSE;
	}


#ifdef LATER
	DIR *dir;

	dir = opendir( search );
	MyFree( search );

	if( dir )	{
		closedir( dir );
		return TRUE;
	} else {
		return FALSE;
	}
#endif

}

/**
 * Load checkpoint file
 *
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int LoadCheckpoint( void )
{
	int len;
	char *search;

	paramVersion = -1;
	wSetCursor( wCursorWait );

	len = strlen( workingDir ) + 1 + strlen( sCheckPointF ) + 1;
	search = (char*)MyMalloc(len);
	sprintf( search, "%s%s%s", workingDir, FILE_SEP_CHAR, sCheckPointF );

	UndoSuspend();

	if (ReadTrackFile( search, search + strlen(search) - strlen( sCheckPointF ), TRUE, TRUE, TRUE )) {
		ResolveIndex();

		RecomputeElevations();
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );
		DoUpdateTitles();
	}

	Reset();
	UndoResume();

	wSetCursor( wCursorNormal );

	strcpy( curPathName, "" );
	curFileName = curPathName;
	SetWindowTitle();
	changed = TRUE;
	MyFree( search );
	return TRUE;
}

/*****************************************************************************
 *
 * IMPORT / EXPORT
 *
 */

static struct wFilSel_t * exportFile_fs;
static struct wFilSel_t * importFile_fs;


static int ImportTracks(
		int cnt,
		char **fileName,
		void * data )
{
	char *nameOfFile;
	long paramVersionOld = paramVersion;

	assert( fileName != NULL );
	assert( cnt == 1 );

	nameOfFile = FindName(fileName[ 0 ]);
	paramVersion = -1;
	wSetCursor( wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );
	ImportStart();
	UndoStart( _("Import Tracks"), "importTracks" );
	useCurrentLayer = TRUE;
	ReadTrackFile( fileName[ 0 ], nameOfFile, FALSE, FALSE, TRUE );
	ImportEnd();
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( wCursorNormal );
	paramVersion = paramVersionOld;
	importMove = TRUE;
	DoCommandB( (void*)(intptr_t)selectCmdInx );
	SelectRecount();
	return TRUE;
}


EXPORT void DoImport( void )
{
	if (importFile_fs == NULL)
		importFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Import Tracks"),
			sImportFilePattern, ImportTracks, NULL );

	wFilSelect( importFile_fs, curDirName );
}


/**
 * Export the selected track pieces
 *
 * \param cnt IN Count of filenames, should always be 1
 * \param fileName IN array of fileNames with cnt names
 * \param data IN unused
 * \return FALSE on error, TRUE on success
 */

static int DoExportTracks(
		int cnt,
		char **fileName,
		void * data )
{
	FILE * f;
	time_t clock;
	char *oldLocale = NULL;

	assert( fileName != NULL );
	assert( cnt == 1 );

	SetCurrentPath( IMPORTPATHKEY, fileName[ 0 ] );
	f = fopen( fileName[ 0 ], "w" );
	if (f==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Export"), fileName[0], strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale("C");

	wSetCursor( wCursorWait );
	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	ExportTracks( f );
	fprintf(f, "END\n");
	fclose(f);

	RestoreLocale( oldLocale );

	Reset();
	wSetCursor( wCursorNormal );
	UpdateAllElevations();
	return TRUE;
}


EXPORT void DoExport( void )
{
	if (selectedTrackCount <= 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	if (exportFile_fs == NULL)
		exportFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Export Tracks"),
				sImportFilePattern, DoExportTracks, NULL );

	wFilSelect( exportFile_fs, curDirName );
}



EXPORT BOOL_T EditCopy( void )
{
	FILE * f;
	time_t clock;
	char *oldLocale = NULL;

	if (selectedTrackCount <= 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return FALSE;
	}
	f = fopen( clipBoardN, "w" );
	if (f == NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Clipboard"), clipBoardN, strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale("C");

	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	ExportTracks(f);
	fprintf(f, "END\n");
	RestoreLocale(oldLocale);
	fclose(f);
	return TRUE;
}


EXPORT BOOL_T EditCut( void )
{
	if (!EditCopy())
		return FALSE;
	SelectDelete();
	return TRUE;
}

/**
 * Paste clipboard content. XTrackCAD uses a disk file as clipboard replacement. This file is read and the
 * content is inserted.
 *
 * \return    TRUE if success, FALSE on error (file not found)
 */

EXPORT BOOL_T EditPaste( void )
{
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");

	wSetCursor( wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );
	ImportStart();
	UndoStart( _("Paste"), "paste" );
	useCurrentLayer = TRUE;
	if ( !ReadTrackFile( clipBoardN, sClipboardF, FALSE, TRUE, FALSE ) ) {
		NoticeMessage( MSG_CANT_PASTE, _("Continue"), NULL );
		rc = FALSE;
	}
	ImportEnd();
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( wCursorNormal );
	importMove = TRUE;
	DoCommandB( (void*)(intptr_t)selectCmdInx );
	SelectRecount();
	UpdateAllElevations();
	RestoreLocale(oldLocale);
	return rc;
}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */

EXPORT void FileInit( void )
{
	const char * pref;

	if ( (libDir = wGetAppLibDir()) == NULL ) {
		abort();
	}
	if ( (workingDir = wGetAppWorkDir()) == NULL )
		AbortProg( "wGetAppWorkDir()" );

	pref = wPrefGetString( "file", "directory" );
	if (pref != NULL) {
		strcpy( curDirName, pref );
	} else {
		sprintf( curDirName, "%s%sexamples", libDir, FILE_SEP_CHAR );
	}
}

EXPORT BOOL_T ParamFileInit( void )
{
	curParamFileIndex = PARAM_DEMO;
	log_paramFile = LogFindIndex( "paramFile" );
	if ( ReadParams( lParamKey, libDir, sParamQF ) == FALSE )
		return FALSE;

	curParamFileIndex = PARAM_CUSTOM;
	if (lParamKey == 0) {
		ReadParamFiles();
		ReadCustom();
	}

	curPathName[0] = '\0';

	clipBoardN = (char*)MyMalloc( strlen(workingDir) + 1 + strlen(sClipboardF) + 1 );
	sprintf( clipBoardN, "%s%s%s", workingDir, FILE_SEP_CHAR, sClipboardF );
	return TRUE;

}
