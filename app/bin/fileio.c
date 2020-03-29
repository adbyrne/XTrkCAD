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
	#define W_OK (2)
	#define access	_access
	#include <windows.h>
#endif
#include <sys/stat.h>
#include <stdarg.h>
#include <locale.h>

#include <stdint.h>

#include <assert.h>

#include <cJSON.h>

#include "archive.h"
#include "common.h"
#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "directory.h"
#include "draw.h"
#include "fileio.h"
#include "fcntl.h"
#include "i18n.h"
#include "layout.h"
#include "manifest.h"
#include "messages.h"
#include "misc.h"
#include "param.h"
#include "include/paramfile.h"
#include "paths.h"
#include "track.h"
#include "utility.h"
#include "version.h"

#ifdef WINDOWS
#include "include/utf8convert.h"
#endif // WINDOWS


/*#define TIME_READTRACKFILE*/

#define COPYBLOCKSIZE	1024

EXPORT const char * workingDir;
EXPORT const char * libDir;


EXPORT char * clipBoardN;


EXPORT wBool_t bExample = FALSE;
EXPORT wBool_t bReadOnly = FALSE;

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
char *paramFileName;
EXPORT wIndex_t paramLineNum = 0;
EXPORT char paramLine[STR_LONG_SIZE];
EXPORT char * curContents;
EXPORT char * curSubContents;

#define PARAM_DEMO (-1)

dynArr_t paramProc_da;

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
 *
 * format chars are:
 * 0 - read a number and discard
 * X - no read, *pi = 0
 * Y - no read, *pf = 0L
 * Z - no read, *pl = 0.0
 * L - *pi = number
 * d - *pi = number
 * w - *pf = read a width
 * u - *pul = number
 * l - *pl = number
 * f - *pf = number
 * z - *pf = 0.0
 * p - *pp = ( number, number ) a coOrd
 * s - *ps = string
 * q - *ps = quoted string
 * c - *qp = position of next non-space char or NULL
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
#ifdef WINDOWS
			ConvertUTF8ToSystem(message);
#endif
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

EXPORT char * PutTitle( char * cp )
{
	static char *title;
	char * tp;
	unsigned cnt = strlen(cp) * 2 + 3;		// add 3 for quotes and terminating \0

	if (!title) {
		title = MyMalloc(cnt);
	} else {
		title = MyRealloc(title, cnt);
	}

	tp = title;

	while (*cp ) {
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

#ifdef WINDOWS
	if(RequiresConvToUTF8(title)) {
		char *out = MyMalloc(cnt);
		wSystemToUTF8(title, out, cnt);
		strcpy(title, out);
		MyFree(out);
	}
#endif // WINDOWS

	return title;
}

/**
 * Set the title of the main window. After loading a file or changing a design
 * this function is called to set the filename and the changed mark in the title
 * bar.
 */

void SetWindowTitle( void )
{
	char *filename;

	if ( changed > 2 || inPlayback )
		return;

	filename = GetLayoutFilename();
	sprintf( message, "%s%s%s - %s(%s)",
		(filename && filename[0])?filename: _("Unnamed Trackplan"),
		bReadOnly?_(" (R/O)"):"",
		changed>0?"*":"",
		sProdName, sVersion );
	wWinSetTitle( mainW, message );
}



/*****************************************************************************
 *
 * LOAD / SAVE TRACKS
 *
 */

static struct wFilSel_t * loadFile_fs = NULL;
static struct wFilSel_t * saveFile_fs = NULL;
static struct wFilSel_t * examplesFile_fs = NULL;

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
	paramFileName = strdup( fileName );

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
#ifdef WINDOWS
			ConvertUTF8ToSystem(paramLine + 7);
#endif // WINDOWS
			SetLayoutTitle(paramLine + 7);
		} else if (strncmp( paramLine, "TITLE2 ", 7 ) == 0) {
#ifdef WINDOWS
			ConvertUTF8ToSystem(paramLine + 7);
#endif // WINDOWS
			SetLayoutSubtitle(paramLine + 7);
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

	if (paramFile) {
		fclose(paramFile);
		paramFile = NULL;
	}

	if( ret ) {
		if (!noSetCurDir)
			SetCurrentPath( LAYOUTPATHKEY, fileName );
	}

	RestoreLocale( oldLocale );

	paramFile = NULL;

	free(paramFileName);
    paramFileName = NULL;
	InfoMessage( "%d", count );
	return ret;
}

int LoadTracks(
		int cnt,
		char **fileName,
		void * data)
{
#ifdef TIME_READTRACKFILE
	long time0, time1;
#endif
	char *nameOfFile = NULL;

	char *extOfFile;

	assert( fileName != NULL );
	assert( cnt == 1 );

	if ( ! bExample )
		SetCurrentPath(LAYOUTPATHKEY, fileName[0]);
	bReadOnly = bExample;
	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );
	Reset();
	ClearTracks();
	ResetLayers();
	checkPtMark = changed = 0;
	LayoutBackGroundInit(TRUE);   //Keep values of background -> will be overriden my archive
	UndoSuspend();
	useCurrentLayer = FALSE;
#ifdef TIME_READTRACKFILE
	time0 = wGetTimer();
#endif
	nameOfFile = FindFilename( fileName[ 0 ] );

 /*
  * Support zipped filetype
  */
	extOfFile = FindFileExtension( nameOfFile);

	BOOL_T zipped = FALSE;
	BOOL_T loadXTC = TRUE;
	char * full_path = strdup(fileName[0]);

	if (extOfFile && (strcmp(extOfFile, ZIPFILETYPEEXTENSION )==0)) {

		char * zip_input = GetZipDirectoryName(ARCHIVE_READ);

		//If zipped unpack file into temporary input dir (cleared and re-created)

		DeleteDirectory(zip_input);
		SafeCreateDir(zip_input);

		if (UnpackArchiveFor(fileName[0], nameOfFile, zip_input, FALSE)) {

			char * manifest_file;

			MakeFullpath(&manifest_file, zip_input, "manifest.json", NULL);

			char * manifest = 0;
			long length;

			FILE * f = fopen (manifest_file, "rb");

			if (f)
			{
			    fseek(f, 0, SEEK_END);
			    length = ftell(f);
			    fseek(f, 0, SEEK_SET);
			    manifest = malloc(length + 1);
			    if (manifest) {
			        fread(manifest, 1, length, f);
			        manifest[length] = '\0';
			    }
			    fclose(f);
			} else
			{
			    NoticeMessage(MSG_MANIFEST_OPEN_FAIL, _("Continue"), NULL, manifest_file);
			}
			free(manifest_file);

			char * arch_file = NULL;

			//Set filename to point to included .xtc file
			//Use the name inside manifest (this helps if a user renames the zip)
			if (manifest)
			{
			    arch_file = ParseManifest(manifest, zip_input);
				free(manifest);
			}

			free(full_path);
			full_path = NULL;
			// If no manifest value use same name as the archive
			if (arch_file && arch_file[0])
			{
			    MakeFullpath(&full_path, zip_input, arch_file, NULL);
			} else
			{
			    MakeFullpath(&full_path, zip_input, nameOfFile, NULL);
			}

			nameOfFile = FindFilename(full_path);
			extOfFile = FindFileExtension(full_path);
			if (strcmp(extOfFile, ZIPFILETYPEEXTENSION )==0)
			{
			    for (int i=0; i<4; i++) {
			        extOfFile[i] = extOfFile[i+1];
			    }
			}
			LOG(log_zip, 1, ("Zip-File %s \n", full_path))
			#if DEBUG
			    printf("File Path: %s \n", full_path);
			#endif
		} else {
			loadXTC = FALSE; // when unzipping fails, don't attempt loading the trackplan
		}
		zipped = TRUE;

		free(zip_input);


	}

	if ( bExample )
		bReadOnly = TRUE;
	else if ( access( fileName[0], W_OK ) == -1 )
		bReadOnly = TRUE;
	else
		bReadOnly = FALSE;

	char *copyOfFileName = MyStrdup(fileName[0]);

	if (loadXTC && ReadTrackFile( full_path, FindFilename( fileName[0]), TRUE, TRUE, TRUE )) {

		nameOfFile = NULL;
		extOfFile = NULL;
		SetCurrentPath( LAYOUTPATHKEY, copyOfFileName );
		SetLayoutFullPath(copyOfFileName);
		SetWindowTitle();

		if ( ! bExample && (nameOfFile != NULL) ) {
			char * copyFile = strdup(fileName[0]);
			char * listName = FindFilename(strdup(fileName[0]));  //Make sure the list name is new
			wMenuListAdd( fileList_ml, 0, listName, copyFile );
		}


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
		LayerSetCounts();
	}

	MyFree(copyOfFileName);
	free(full_path);
	full_path = NULL;

	UndoResume();
	Reset();
	wSetCursor( mainD.d, defaultCursor );
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
	bExample = FALSE;
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
	wSetCursor( mainD.d, wCursorWait );
	time(&clock);
	rc &= fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) )>0;
	rc &= fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION )>0;
	Stripcr( GetLayoutTitle() );
	Stripcr( GetLayoutSubtitle() );
	rc &= fprintf(f, "TITLE1 %s\n", GetLayoutTitle())>0;
	rc &= fprintf(f, "TITLE2 %s\n", GetLayoutSubtitle())>0;
	rc &= fprintf(f, "MAPSCALE %ld\n", (long)mapD.scale )>0;
	rc &= fprintf(f, "ROOMSIZE %0.6f x %0.6f\n", mapD.size.x, mapD.size.y )>0;
	rc &= fprintf(f, "SCALE %s\n", curScaleName )>0;
	rc &= WriteLayers( f );
	rc &= WriteMainNote( f );
	rc &= WriteTracks( f, TRUE );
	rc &= fprintf(f, "END\n")>0;
	if ( !rc )
		NoticeMessage( MSG_WRITE_FAILURE, _("Ok"), NULL, strerror(errno), fileName );
	fclose(f);
	bReadOnly = FALSE;

	RestoreLocale( oldLocale );

	checkPtMark = changed;
	wSetCursor( mainD.d, defaultCursor );
	return rc;
}

/************************************************
 * Copy Dependency - copy file into another directory
 *
 * \param IN name
 * \param IN target_dir
 *
 * \returns TRUE for success
 *
 */
static BOOL_T CopyDependency(char * name, char * target_dir)
{
    char * backname = FindFilename(name);
	BOOL_T copied = TRUE;
	FILE * source = fopen(name, "rb");

    if (source != NULL) {
        char * target_file;
        MakeFullpath(&target_file, target_dir, backname, NULL);
        FILE * target = fopen(target_file, "wb");

        if (target != NULL) {
            char *buffer = MyMalloc(COPYBLOCKSIZE);
            while (!feof(source)) {
                size_t bytes = fread(buffer, 1, sizeof(buffer), source);
                if (bytes) {
                    fwrite(buffer, 1, bytes, target);
                }
            }
			MyFree(buffer);
            LOG(log_zip, 1, ("Zip-Include %s into %s \n", name, target_file))
#if DEBUG
            printf("xtrkcad: Included file %s into %s \n",
                   name, target_file);
#endif
            fclose(target);
        } else {
            NoticeMessage(MSG_COPY_FAIL, _("Continue"), NULL, name, target_file);
            copied = FALSE;
        }
        free(target_file);
        fclose(source);
    } else {
        NoticeMessage(MSG_COPY_OPEN_FAIL, _("Continue"), NULL, name);
        copied = FALSE;
    }
    return copied;
}


static doSaveCallBack_p doAfterSave;



static int SaveTracks(
		int cnt,
		char **fileName,
		void * data )
{

	assert( fileName != NULL );
	assert( cnt == 1 );

	char *nameOfFile = FindFilename(fileName[0]);

	SetCurrentPath(LAYOUTPATHKEY, fileName[0]);

	//Support Archive zipped files

	char * extOfFile = FindFileExtension( fileName[0]);


	if (extOfFile && (strcmp(extOfFile,ZIPFILETYPEEXTENSION)==0)) {

		char * ArchiveName;

		//Set filename to point to be the same as the included .xtc file.
		//This is also in the manifest - in case a user renames the archive file.

		char * zip_output = GetZipDirectoryName(ARCHIVE_WRITE);

		DeleteDirectory(zip_output);
		SafeCreateDir(zip_output);

		MakeFullpath(&ArchiveName, zip_output, nameOfFile, NULL);

		nameOfFile = FindFilename(ArchiveName);
		extOfFile = FindFileExtension(ArchiveName);

		if (extOfFile && strcmp(extOfFile, ZIPFILETYPEEXTENSION)==0) {
			// Get rid of the 'e'
			extOfFile[3] = '\0';
		}

		char * DependencyDir;

		//The included files are placed (for now) into an includes directory - TODO an array of includes with directories by type
		MakeFullpath(&DependencyDir, zip_output, "includes", NULL);

		SafeCreateDir(DependencyDir);

		char * background = GetLayoutBackGroundFullPath();

		if (background && background[0])
			CopyDependency(background,DependencyDir);

		//The details are stored into the manifest - TODO use arrays for files, locations
		char *oldLocale = SaveLocale("C");
		char* json_Manifest = CreateManifest(nameOfFile, background, "includes");
		char * manifest_file;

		MakeFullpath(&manifest_file, zip_output, "manifest.json", NULL);

		FILE *fp = fopen(manifest_file, "wb");
		if (fp != NULL)
		{
		   fputs(json_Manifest, fp);
		   fclose(fp);
		} else {
			NoticeMessage( MSG_MANIFEST_FAIL, _("Continue"), NULL, manifest_file );
		}
		RestoreLocale(oldLocale);

		free(manifest_file);
		free(json_Manifest);

		DoSaveTracks( ArchiveName );

		if (CreateArchive(	zip_output,	fileName[0]) != TRUE) {
			NoticeMessage( MSG_ARCHIVE_FAIL, _("Continue"), NULL, fileName[0], zip_output );
		}
		free(zip_output);
		free(ArchiveName);

	} else

		DoSaveTracks( fileName[ 0 ] );

	nameOfFile = FindFilename( fileName[ 0 ] );
	wMenuListAdd( fileList_ml, 0, nameOfFile, MyStrdup(fileName[ 0 ]) );
	checkPtMark = changed = 0;

	SetLayoutFullPath(fileName[0]);

	if (doAfterSave)
		doAfterSave();
	doAfterSave = NULL;
	return TRUE;
}


EXPORT void DoSave( doSaveCallBack_p after )
{
	doAfterSave = after;
	if ( bReadOnly || *(GetLayoutFilename()) == '\0') {
		if (saveFile_fs == NULL)
			saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks"),
				sSourceFilePattern, SaveTracks, NULL );
		wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	} else {
		char *temp = GetLayoutFullPath();
		SaveTracks( 1, &temp, NULL );
	}
	SetWindowTitle();
	SaveState();
}

EXPORT void DoSaveAs( doSaveCallBack_p after )
{
	doAfterSave = after;
	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks As"),
			sSaveFilePattern, SaveTracks, NULL );
	wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	SetWindowTitle();
	SaveState();
}

EXPORT void DoLoad( void )
{
	if (loadFile_fs == NULL)
		loadFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Open Tracks"),
			sSourceFilePattern, LoadTracks, NULL );
	bExample = FALSE;
	wFilSelect( loadFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	SaveState();
}


EXPORT void DoExamples( void )
{
	if (examplesFile_fs == NULL) {
		static wBool_t bExample = TRUE;
		examplesFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Example Tracks"),
			sSourceFilePattern, LoadTracks, &bExample );
	}
	bExample = TRUE;
	sprintf( message, "%s" FILE_SEP_CHAR "examples" FILE_SEP_CHAR, libDir );
	wFilSelect( examplesFile_fs, message );
	SaveState();
}


EXPORT void DoCheckPoint( void )
{
	int rc;

	if (checkPointingW == NULL) {
		ParamRegister( &checkPointingPG );
		checkPointingW = ParamCreateDialog( &checkPointingPG, MakeWindowTitle(_("Check Pointing")), NULL, NULL, NULL, FALSE, NULL, F_TOP|F_CENTER, NULL );
	}
	rename( checkPtFileName1, checkPtFileName2 );
	//wShow( checkPointingW );
	rc = DoSaveTracks( checkPtFileName1 );

	/* could the check point file be written ok? */
	if( rc ) {
		/* yes, delete the backup copy of the checkpoint file */
		remove( checkPtFileName2 );
	} else {
		/* no, rename the backup copy back to the checkpoint file name */
		rename( checkPtFileName2, checkPtFileName1 );
	}
	//wHide( checkPointingW );
	wShow( mainW );
}

/**
 * Remove all temporary files before exiting. When the program terminates
 * normally through the exit choice, files and directories that were created
 * temporarily are removed: xtrkcad.ckp
 *
 * \param none
 * \return none
 *
 */

EXPORT void CleanupFiles( void )
{
	char *tempDir;

	if( checkPtFileName1 )
		remove( checkPtFileName1 );

	for (int i = ARCHIVE_READ; i <= ARCHIVE_WRITE; ++i) {
		tempDir = GetZipDirectoryName(i);
		if (tempDir) {
			DeleteDirectory(tempDir);
			free(tempDir);
		}
	}
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
	struct stat fileStat;

	MakeFullpath(&checkPtFileName1, workingDir, sCheckPointF, NULL);
	MakeFullpath(&checkPtFileName2, workingDir, sCheckPoint1F, NULL);

	if( !stat( checkPtFileName1, &fileStat ) ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * Load checkpoint file
 *
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int LoadCheckpoint( void )
{
	char *search;

	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );

	MakeFullpath(&search, workingDir, sCheckPointF, NULL);
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

	wSetCursor( mainD.d, defaultCursor );

	SetLayoutFullPath("");
	SetWindowTitle();
	changed = TRUE;
	free( search );
	return TRUE;
}

/*****************************************************************************
 *
 * IMPORT / EXPORT
 *
 */

static struct wFilSel_t * exportFile_fs;
static struct wFilSel_t * importFile_fs;

static int importAsModule;



/*******************************************************************************
 *
 * Import Layout Dialog
 *
 */



static int ImportTracks(
		int cnt,
		char **fileName,
		void * data )
{
	char *nameOfFile;
	long paramVersionOld = paramVersion;

	assert( fileName != NULL );
	assert( cnt == 1 );

	nameOfFile = FindFilename(fileName[ 0 ]);
	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );
	int saveLayer = curLayer;
	int layer;
	if (importAsModule) {
		layer = FindUnusedLayer(0);
		if (layer==-1) return FALSE;
		char LayerName[80];
		LayerName[0] = '\0';
		sprintf(LayerName,_("Module - %s"),nameOfFile);
		if (layer>=0) SetCurrLayer(layer, NULL, 0, NULL, NULL);
		SetLayerName(layer,LayerName);
	}
	ImportStart();
	UndoStart( _("Import Tracks"), "importTracks" );
	useCurrentLayer = TRUE;
	ReadTrackFile( fileName[ 0 ], nameOfFile, FALSE, FALSE, TRUE );
	ImportEnd();
	if (importAsModule) SetLayerModule(layer,TRUE);
	useCurrentLayer = FALSE;
	SetCurrLayer(saveLayer, NULL, 0, NULL, NULL);
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( mainD.d, defaultCursor );
	paramVersion = paramVersionOld;
	importMove = TRUE;
	DoCommandB( (void*)(intptr_t)selectCmdInx );
	SelectRecount();
	return TRUE;
}

EXPORT void DoImport( void * type )
{
	importAsModule = (int)(long)type;
	if (importFile_fs == NULL)
		importFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Import Tracks"),
			sImportFilePattern, ImportTracks, NULL );

	wFilSelect( importFile_fs, GetCurrentPath(LAYOUTPATHKEY));
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

	wSetCursor( mainD.d, wCursorWait );
	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	ExportTracks( f );
	fprintf(f, "END\n");
	fclose(f);

	RestoreLocale( oldLocale );

	Reset();
	wSetCursor( mainD.d, defaultCursor );
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

	wFilSelect( exportFile_fs, GetCurrentPath(LAYOUTPATHKEY));
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

	wSetCursor( mainD.d, wCursorWait );
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
	useCurrentLayer = FALSE;
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( mainD.d, defaultCursor );
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
	if ( (libDir = wGetAppLibDir()) == NULL ) {
		abort();
	}
	if ( (workingDir = wGetAppWorkDir()) == NULL )
		AbortProg( "wGetAppWorkDir()" );

	SetLayoutFullPath("");
		MakeFullpath(&clipBoardN, workingDir, sClipboardF, NULL);

}
