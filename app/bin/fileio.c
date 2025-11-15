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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

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
#include "layout.h"
#include "manifest.h"
#include "misc.h"
#include "param.h"
#include "include/paramfile.h"
#include "include/paramfilelist.h"
#include "paths.h"
#include "include/stringxtc.h"
#include "track.h"
#include "version.h"
#include "dynstring.h"
#include "common-ui.h"
#include "ctrain.h"

#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

#define COPYBLOCKSIZE	1024

EXPORT const char * workingDir;
EXPORT const char * libDir;

EXPORT char * clipBoardN;
static coOrd paste_offset, cursor_offset;

EXPORT wBool_t bExample = FALSE;
EXPORT wBool_t bReadOnly = FALSE;

static int log_timereadfile = 0;

#ifdef WINDOWS
#define rename( F1, F2 ) Copyfile( F1, F2 )
#endif

EXPORT int Copyfile( const char * fn1, const char * fn2 )
{
	FILE *f1, *f2;
	size_t size;
	f1 = fopen( fn1, "r" );
	if ( f1 == NULL ) {
		return 0;
	}
	f2 = fopen( fn2, "w" );
	if ( f2 == NULL ) {
		fclose( f1 );
		return -1;
	}
	while ( (size=fread( message, 1, sizeof message, f1 )) > 0 ) {
		fwrite( message, size, 1, f2 );
	}
	fclose( f1 );
	fclose( f2 );
	return 0;
}


/****************************************************************************
 *
 * PARAM FILE INPUT
 *
 */

EXPORT FILE * paramFile = NULL;
char *paramFileName;
EXPORT wIndex_t paramLineNum = 0;
EXPORT char paramLine[STR_HUGE_SIZE];
EXPORT char * curContents;
EXPORT char * curSubContents;

#define PARAM_DEMO (-1)

#define RECENT_FILE_SECTION "recentfiles"
#define RECENT_FILE_KEY "file"

EXPORT dynArr_t paramProc_da;


EXPORT char * GetNextLine( void )
{
	if (!paramFile) {
		paramLine[0] = '\0';
		return NULL;
	}
	if (fgets( paramLine, sizeof paramLine, paramFile ) == NULL) {
		sprintf( message, "INPUT ERROR: premature EOF on %s", paramFileName );
		wNoticeWithIcon( NT_ERROR, message, _("Ok"), NULL );
		if ( paramFile ) {
			fclose( paramFile );
			paramFile = NULL;
		}
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
	if (!(ret = wNoticeWithIcon( NT_ERROR, message, _("Continue"), _("Stop") ))) {
		if ( paramFile ) {
			fclose(paramFile);
			paramFile = NULL;
		}
		if ( paramFileName ) {
			free( paramFileName );
			paramFileName = NULL;
		}
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
 * In the error case, InputError had been called which may have closed the input file (paramFile)
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
        const char * format,
        ... )
{
	char * cp, * cq;
	long * pl;
	unsigned long *pul;
	int * pi;
	FLOAT_T *pf;
	coOrd p, *pp;
	char * ps;
	char ** qp;
	va_list ap;
	char * sError = NULL;

	cp = line;
	va_start( ap, format );
	for ( ; sError==NULL && *format; format++ ) {
		while (isspace((unsigned char)*cp)) { cp++; }
		if (!*cp && strchr( "XZYzc", *format ) == NULL ) {
			sError = "EOL unexpected";
			break;
		}
		switch (*format) {
		case '0':
			(void)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
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
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'd':
			pi = va_arg( ap, int * );
			*pi = (int)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'w':
			pf = va_arg( ap, FLOAT_T * );
			*pf = (FLOAT_T)strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			if (*cq == '.') {
				*pf = strtod( cp, &cq );
			} else {
				*pf /= mainD.dpi;
			}
			cp = cq;
			break;
		case 'u':
			pul = va_arg( ap, unsigned long * );
			*pul = strtoul( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'l':
			pl = va_arg( ap, long * );
			*pl = strtol( cp, &cq, 10 );
			if (cp == cq) {
				sError = "%s: expected integer";
				break;
			}
			cp = cq;
			break;
		case 'f':
			pf = va_arg( ap, FLOAT_T * );
			*pf = strtod( cp, &cq );
			if (cp == cq) {
				sError = "%s: expected float";
				break;
			}
			cp = cq;
			break;
		case 'z':
			pf = va_arg( ap, FLOAT_T * );
			*pf = 0.0;
			break;
		case 'p':
			pp = va_arg( ap, coOrd * );
			p.x = strtod( cp, &cq );
			if (cp == cq) {
				sError = "%s: expected float";
				break;
			}
			cp = cq;
			p.y = strtod( cp, &cq );
			if (cp == cq) {
				sError = "%s: expected float";
				break;
			}
			cp = cq;
			*pp = p;
			break;
		case 's':
			ps = va_arg( ap, char * );
			while (isspace((unsigned char)*cp)) { cp++; }
			while (*cp && !isspace((unsigned char)*cp)) { *ps++ = *cp++; }
			*ps++ = '\0';
			break;
		case 'q':
			qp = va_arg( ap, char * * );
			if (*cp != '\"')
				/* Stupid windows */
			{
				cq = strchr( cp, '\"' );
			} else {
				cq = cp;
			}
			if (cq!=NULL) {
				cp = cq;
				ps = &message[0];
				cp++;
				while (*cp) {
					CHECK( (ps-message)<sizeof message );
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
#ifdef UTFCONVERT
			ConvertUTF8ToSystem(message);
#endif
			*qp = (char*)ConvertFromEscapedText(message);
			break;
		case 'c':
			qp = va_arg( ap, char * * );
			while (isspace((unsigned char)*cp)) { cp++; }
			if (*cp) {
				*qp = cp;
			} else {
				*qp = NULL;
			}
			break;
		default:
			CHECKMSG( FALSE, ( "getArgs: bad format char: %c", *format ) );
		}
	}
	va_end( ap );
	if ( sError ) {
		InputError( sError, TRUE, cp );
		return FALSE;
	}
	return TRUE;
}



wBool_t IsEND( char * sEnd )
{
	char * cp;
	wBool_t bAllowNakedENDs = paramVersion < VERSION_NONAKEDENDS;
	for ( cp = paramLine; *cp && (isspace( *cp ) || *cp == '\t'); cp++ );
	if ( strncmp( cp, sEnd, strlen(sEnd) ) == 0 ) {
		cp += strlen( sEnd );
	} else if ( bAllowNakedENDs && strncmp( cp, "END", 3 ) == 0 ) {
		cp += 3;
	} else {
		return FALSE;
	}
	for ( ; *cp && isspace( *cp ); cp++ );
	if ( *cp != '\0' ) {
		return FALSE;
	}
	return TRUE;
}


/**
 * Read the text for a note/car. Lines are read from the input file
 * until the END statement is found.
 *
 * \todo Handle premature end as an error
 *
 * \return pointer to string, has to be myfree'd by caller
 */

char *
ReadMultilineText()
{
	char *string;
	DynString noteText;
	DynStringMalloc(&noteText, 0);
	char *line;

	line = GetNextLine();

	while ( !IsEND("END") ) {
		DynStringCatCStr(&noteText, line);
		DynStringCatCStr(&noteText, "\n");
		line = GetNextLine();
	}
	string = MyStrdup(DynStringToCStr(&noteText));
	string[strlen(string) - 1] = '\0';

#ifdef UTFCONVERT
	if (wIsUTF8(string)) {
		ConvertUTF8ToSystem(string);
	}
#endif // UTFCONVERT

	DynStringFree(&noteText);
	return (string);
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
		while (isspace((unsigned char)*s)) { s++; }
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

/**
 * Parameter file parser definitions
 *
 * \param [IN] name command
 * \param [IN] proc function for reading the parameter definition
 * \param [IN] delete if not NULL function for freeing the definition
 */
EXPORT void AddParam(
        char * name,
        readParam_t proc)
{
	DYNARR_APPEND( paramProc_t, paramProc_da, 10 );
	paramProc(paramProc_da.cnt-1).name = name;
	paramProc(paramProc_da.cnt-1).proc = proc;
}

EXPORT char * PutTitle( char * cp )
{
	static char *title;
	char * tp;
	size_t cnt = strlen(cp) * 2 + 3;		// add 3 for quotes and terminating \0

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
	if ( *cp ) {
		NoticeMessage( _("putTitle: title too long: %s"), _("Ok"), NULL, title );
	}
	*tp = '\0';

#ifdef UTFCONVERT
	if (RequiresConvToUTF8(title)) {
		char *out = MyMalloc(cnt);
		wSystemToUTF8(title, out, (unsigned int)cnt);
		strcpy(title, out);
		MyFree(out);
	}
#endif // UTFCONVERT

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

	if ( changed > 2 || inPlayback ) {
		return;
	}

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

static char * checkPtFileName1;
static char * checkPtFileName2;
static char * checkPtFileNameBackup;

/** Read the layout design.
 *
 * \param IN pathName filename including directory
 * \param IN fileName pointer to filename part in pathName
 * \param IN full
 * \param IN noSetCurDir if FALSE current directory is changed to file location
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
	int ret = TRUE;

	paramFile = fopen( pathName, "r" );
	if (paramFile == NULL) {
		if ( complain ) {
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, sProdName, pathName,
			               strerror(errno) );
		}
		return FALSE;
	}

	ParamSetInReadTracks(TRUE);
	SetCLocale();
	checkPtFileNameBackup = NULL;
	paramLineNum = 0;
	paramFileName = strdup( fileName );

	InfoMessage("0", "0");
	count = 0;
	int skipLines = 0;
	BOOL_T skip = FALSE;
	while ( paramFile
	        && ( fgets(paramLine, sizeof paramLine, paramFile) ) != NULL ) {
		count++;
		BOOL_T old_skip = skip;
		skip = FALSE;
		if (count%10 == 0) {
			InfoMessage( "%d", count );
			wFlush();
		}
		paramLineNum++;
		if (strlen(paramLine) == (sizeof paramLine) -1 &&
		    paramLine[(sizeof paramLine)-1] != '\n') {
			if ( !(ret = InputError( "Line too long", TRUE ))) {
				break;
			}
		}
		Stripcr( paramLine );
		if (paramLine[0] == '#' ||
		    paramLine[0] == '\n' ||
		    paramLine[0] == '\0' ) {
			/* comment */
			continue;
		}

		if (ReadTrack( paramLine )) {
			continue;
		} else if (IsEND( END_TRK_FILE ) ) {
			break;
		} else if (strncmp( paramLine, "VERSION ", 8 ) == 0) {
			paramVersion = strtol( paramLine+8, &cp, 10 );
			if (cp)
				while (*cp && isspace((unsigned char)*cp)) { cp++; }
			if ( paramVersion > iParamVersion ) {
				if (cp && *cp) {
					NoticeMessage( MSG_UPGRADE_VERSION1, _("Ok"), NULL, paramVersion, iParamVersion,
					               sProdName, cp );
				} else {
					NoticeMessage( MSG_UPGRADE_VERSION2, _("Ok"), NULL, paramVersion, iParamVersion,
					               sProdName );
				}
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_BAD_FILE_VERSION, _("Ok"), NULL, paramVersion,
				               iMinParamVersion, sProdName );
				break;
			}
		} else if (!full) {
			if ( !(ret = InputError( "unknown command", TRUE ))) {
				break;
			}
		} else if (strncmp( paramLine, "TITLE1 ", 7 ) == 0) {
#ifdef UTFCONVERT
			ConvertUTF8ToSystem(paramLine + 7);
#endif // UTFCONVERT
			SetLayoutTitle(paramLine + 7);
		} else if (strncmp( paramLine, "TITLE2 ", 7 ) == 0) {
#ifdef UTFCONVERT
			ConvertUTF8ToSystem(paramLine + 7);
#endif // UTFCONVERT
			SetLayoutSubtitle(paramLine + 7);
		} else if (strncmp( paramLine, "ROOMSIZE", 8 ) == 0) {
			if ( ParseRoomSize( paramLine+8, &roomSize ) ) {
				SetRoomSize( roomSize );
				/*wFloatSetValue( roomSizeXPD.control, PutDim(roomSize.x) );*/
				/*wFloatSetValue( roomSizeYPD.control, PutDim(roomSize.y) );*/
			} else {
				if ( !(ret = InputError( "ROOMSIZE: bad value", TRUE ))) {
					break;
				}
			}
		} else if (strncmp( paramLine, "SCALE ", 6 ) == 0) {
			if ( !DoSetScale( paramLine+6 ) ) {
				if ( !(ret = InputError( "SCALE: bad value", TRUE ))) {
					break;
				}
			}
		} else if (strncmp( paramLine, "MAPSCALE ", 9 ) == 0) {
			scale = atol( paramLine+9 );
			if (scale > 1) {
				mapD.scale = scale;
			}
		} else if (strncmp( paramLine, "LAYERS ", 7 ) == 0) {
			ReadLayers( paramLine+7 );
		} else {
			if (!old_skip) {
				if (InputError(_("Unknown layout file object - skip until next good object?"),
				               TRUE)) {   //OK to carry on
					/* SKIP until next main line we recognize */
					skip = TRUE;
					skipLines++;
					continue;
				} else {
					break;    //Close File
				}
			} else { skip = TRUE; }
			skipLines++;
		}
	}

	ParamSetInReadTracks(FALSE);
	if (paramFile) {
		fclose(paramFile);
		paramFile = NULL;
	}

	if ( ret ) {
		if (!noSetCurDir) {
			SetCurrentPath( LAYOUTPATHKEY, fileName );
		}
	}

	if (skipLines>0) {
		NoticeMessage( MSG_LAYOUT_LINES_SKIPPED, _("Ok"), NULL, paramFileName,
		               skipLines);
	}

	paramFile = NULL;

	SetUserLocale();
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
	char *nameOfFile = NULL;

	char *extOfFile;

	CHECK( fileName != NULL );
	CHECK( cnt == 1 );

	nameOfFile = FindFilename(fileName[0]);

	// Make sure it exists and it is readable
	if (access(fileName[0], R_OK) != 0) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Track"), nameOfFile,
		              _("Not Found"));
		return FALSE;
	}

	if ( ! bExample ) {
		SetCurrentPath(LAYOUTPATHKEY, fileName[0]);
	}
	bReadOnly = bExample;
	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );
	Reset();
	ClearTracks();
	ResetLayers();
	checkPtMark = changed = 0;
	if (!data) {
		LayoutBackGroundInit(
		        TRUE);        //Keep values of background -> will be overriden by archive
	}
	UndoSuspend();
	useCurrentLayer = FALSE;

	/*
	 * Support zipped filetype
	 */
	extOfFile = FindFileExtension( nameOfFile);

//	BOOL_T zipped = FALSE;
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

			if (f) {
				fseek(f, 0, SEEK_END);
				length = ftell(f);
				fseek(f, 0, SEEK_SET);
				manifest = malloc(length + 1);
				if (manifest) {
					manifest[ fread(manifest, 1, length, f) ] = '\0';
				}
				fclose(f);
			} else {
				NoticeMessage(MSG_MANIFEST_OPEN_FAIL, _("Continue"), NULL, manifest_file);
			}
			free(manifest_file);

			char * arch_file = NULL;

			//Set filename to point to included .xtc file
			//Use the name inside manifest (this helps if a user renames the zip)
			if (manifest) {
				arch_file = ParseManifest(manifest, zip_input);
				free(manifest);
			}

			free(full_path);
			full_path = NULL;
			// If no manifest value use same name as the archive
			if (arch_file && arch_file[0]) {
				MakeFullpath(&full_path, zip_input, arch_file, NULL);
			} else {
				MakeFullpath(&full_path, zip_input, nameOfFile, NULL);
			}

			nameOfFile = FindFilename(full_path);
			extOfFile = FindFileExtension(full_path);
			if (strcmp(extOfFile, ZIPFILETYPEEXTENSION )==0) {
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
//		zipped = TRUE;

		free(zip_input);


	}

	if ( bExample ) {
		bReadOnly = TRUE;
	} else if ( access( fileName[0], W_OK ) == -1 ) {
		bReadOnly = TRUE;
	} else {
		bReadOnly = FALSE;
	}

	char *copyOfFileName = MyStrdup(fileName[0]);

	unsigned long time0 = wGetTimer();
	if (loadXTC
	    && ReadTrackFile( full_path, FindFilename( fileName[0]), TRUE, TRUE, TRUE )) {

		//nameOfFile = NULL;
		//extOfFile = NULL;
		SetCurrentPath( LAYOUTPATHKEY, copyOfFileName );
		SetLayoutFullPath(copyOfFileName);
		SetWindowTitle();

		AddToRecentFiles(nameOfFile, fileName[0]);

		ResolveIndex();
		LOG( log_timereadfile, 1, ( "Read time (%s) = %lu mS \n", fileName[0],
		                            wGetTimer()-time0 ) );
		RecomputeElevations(NULL);
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );
		LayerSetCounts();
	}

	MyFree(copyOfFileName);
	free(full_path);
	full_path = NULL;

	UpdateLayerDlg(curLayer);

	UndoResume();
	Reset();
	wSetCursor( mainD.d, defaultCursor );
	return TRUE;
}

void AddToRecentFiles(char* displayFileName, char* fullFileName)
{
	if (!bExample && (displayFileName != NULL)) {
		char* copyFile = MyStrdup(fullFileName);
		char* listName = FindFilename(strdup(fullFileName));
		wMenuListAdd(fileList_ml, 0, listName, copyFile);

		SaveRecentFileList();
	}
}

void
SaveRecentFileList()
{
	int count = wMenuListGetCount(fileList_ml);		//index is zero based, 
	DynString key;
 	DynStringMalloc(&key, 0);

	if (count) {
		for (int index = 0; index < count; index++) {
			void* attributes;

			if (wMenuListGet(fileList_ml, index, &attributes)) {
				DynStringPrintf(&key, RECENT_FILE_KEY "%d", count - index -1 );
				wPrefSetString(RECENT_FILE_SECTION, DynStringToCStr(&key), (char*)attributes);
			}
		}
	}
	DynStringFree(&key);
}

/**
 * Load the layout specified by data. Filename may contain a full
 * path.
 * \param index IN ignored
 * \param label IN if not NULL - during startup - set flag to not load background
 * \param data IN path and filename
 */

EXPORT void DoFileList(
        int index,
        char * label,
        void * data )
{
	char *pathName = (char*)data;
	bExample = FALSE;
	if (label) {
		LoadTracks( 1, &pathName, I2VP(1));
	} else {
		LoadTracks( 1, &pathName, NULL );
	}
}

static BOOL_T DoSaveTracks(
        const char * fileName )
{
	FILE * f;
	time_t clock;
	BOOL_T rc = TRUE;


	f = fopen( fileName, "w" );
	if (f==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Track"), fileName,
		               strerror(errno) );
		return FALSE;
	}
	SetCLocale();
	wSetCursor( mainD.d, wCursorWait );
	time(&clock);
	rc &= fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion,
	              ctime(&clock) )>0;
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
	rc &= fprintf(f, "%s\n", END_TRK_FILE)>0;
	if ( !rc ) {
		NoticeMessage( MSG_WRITE_FAILURE, _("Ok"), NULL, strerror(errno), fileName );
	}
	fclose(f);
	bReadOnly = FALSE;

	checkPtMark = changed;
	wSetCursor( mainD.d, defaultCursor );
	SetUserLocale();
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

/**
 * Save the layout to file. This function handles either cases, classic xtc
 * files as well as xtce zip archives.
 *
 * \param	cnt	Number of files, must be 1
 * \param	fileName name of destination file including extension (xtc or xtce)
 * \param	data unused
 *
 * \returns TRUE for success
 */

static int SaveTracks(
        int cnt,
        char** fileName,
        void* data)
{
	BOOL_T success = FALSE;

	CHECK(fileName != NULL);
	CHECK(cnt == 1);

	char* nameOfFile = FindFilename(fileName[0]);

	SetCurrentPath(LAYOUTPATHKEY, fileName[0]);

	//Support Archive zipped files

	char* extOfFile = FindFileExtension(fileName[0]);

	if (extOfFile && (strcmp(extOfFile, ZIPFILETYPEEXTENSION) == 0)) {

		char* ArchiveName;

		//Set filename to point to be the same as the included .xtc file.
		//This is also in the manifest - in case a user renames the archive file.

		char* zip_output = GetZipDirectoryName(ARCHIVE_WRITE);

		DeleteDirectory(zip_output);
		SafeCreateDir(zip_output);

		MakeFullpath(&ArchiveName, zip_output, nameOfFile, NULL);

		nameOfFile = FindFilename(ArchiveName);
		extOfFile = FindFileExtension(ArchiveName);

		if (extOfFile && strcmp(extOfFile, ZIPFILETYPEEXTENSION) == 0) {
			// Get rid of the 'e'
			extOfFile[3] = '\0';
		}

		char* DependencyDir;

		// The included files are placed (for now) into an includes directory -
		// TODO an array of includes with directories by type
		MakeFullpath(&DependencyDir, zip_output, "includes", NULL);

		SafeCreateDir(DependencyDir);

		char* background = GetLayoutBackGroundFullPath();

		// if used, get the background file
		// else ignore this step
		if (background && background[0]) {
			success = CopyDependency(background, DependencyDir);
		} else {
			background = NULL;
			success = TRUE;
		}

		if (success) {
			//The details are stored into the manifest - TODO use arrays for files, locations
			SetCLocale();
			char* json_Manifest = CreateManifest(nameOfFile, background, "includes");
			char* manifest_file;

			MakeFullpath(&manifest_file, zip_output, "manifest.json", NULL);

			FILE* fp = fopen(manifest_file, "wb");
			if (fp != NULL) {
				fputs(json_Manifest, fp);
				fclose(fp);
			} else {
				NoticeMessage(MSG_MANIFEST_FAIL, _("Continue"), NULL, manifest_file);
				success = FALSE;
			}
			SetUserLocale();

			free(manifest_file);
			free(json_Manifest);
		}

		success &= DoSaveTracks(ArchiveName);

		if (success) {
			if (CreateArchive(zip_output, fileName[0]) != TRUE) {
				NoticeMessage(MSG_ARCHIVE_FAIL, _("Continue"), NULL, fileName[0], zip_output);
			}
		}

		free(zip_output);
		free(ArchiveName);

	} else {
		success = DoSaveTracks(fileName[0]);
	}

	if (success) {
		nameOfFile = FindFilename(fileName[0]);
		wMenuListAdd(fileList_ml, 0, nameOfFile, MyStrdup(fileName[0]));
		checkPtMark = changed = 0;

		SetLayoutFullPath(fileName[0]);
	}

	if (doAfterSave) {
		doAfterSave();
	}
	return success;
}

/**
 * Save information about current files and some settings to preferences file.
 */

EXPORT void SaveState(void)
{
	wWinPix_t width, height;

	wWinGetSize(mainW, &width, &height);
	wPrefSetInteger("draw", "mainwidth", (int)width);
	wPrefSetInteger("draw", "mainheight", (int)height);
	SaveParamFileList();
	ParamUpdatePrefs();

	wPrefSetString( "misc", "lastlayout", GetLayoutFullPath());
	wPrefSetInteger( "misc", "lastlayoutexample", bExample );

 	wPrefFlush("");
}
static void SetAutoSave()
{
	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("AutoSave Tracks As"),
		                             sSourceFilePattern, SaveTracks, NULL );
	wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	checkPtMark = 1;
	SetWindowTitle();
	CleanupCheckpointFiles();  //Remove old checkpoint
	SaveState();

}

EXPORT void DoSave( void * doAfterSaveVP )
{
	doAfterSave = doAfterSaveVP;
	if ( bReadOnly || *(GetLayoutFilename()) == '\0') {
		if (saveFile_fs == NULL)
			saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks"),
			                             sSourceFilePattern, SaveTracks, NULL );
		wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
		checkPtMark = 1;
	} else {
		char *temp = GetLayoutFullPath();
		SaveTracks( 1, &temp, NULL );
	}
	SetWindowTitle();
	CleanupCheckpointFiles();  //Remove old checkpoint
	SaveState();
}

EXPORT void DoSaveAs( void * doAfterSaveVP )
{
	doAfterSave = doAfterSaveVP;
	if (saveFile_fs == NULL)
		saveFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Save Tracks As"),
		                             sSaveFilePattern, SaveTracks, NULL );
	wFilSelect( saveFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	checkPtMark = 1;
	SetWindowTitle();
	CleanupCheckpointFiles();  //Remove old checkpoint
	SaveState();
}

EXPORT void DoLoad( void )
{
	if (loadFile_fs == NULL)
		loadFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Open Tracks"),
		                             sSourceFilePattern, LoadTracks, NULL );
	bExample = FALSE;
	wFilSelect( loadFile_fs, GetCurrentPath(LAYOUTPATHKEY));
	paste_offset = zero;
	cursor_offset = zero;
	CleanupCheckpointFiles();  //Remove old checkpoint
	SaveState();
}


EXPORT void DoExamples( void )
{
	if (examplesFile_fs == NULL) {
		examplesFile_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Example Tracks"),
		                                 sSourceFilePattern, LoadTracks, NULL );
	}
	bExample = TRUE;
	sprintf( message, "%s" FILE_SEP_CHAR "examples" FILE_SEP_CHAR, libDir );
	wFilSelect( examplesFile_fs, message );
	CleanupCheckpointFiles();  //Remove old checkpoint
	SaveState();
}

static wIndex_t generations_count = 0;
wIndex_t max_generations_count = 10;
static char sCheckPointBF[STR_LONG_SIZE];


static void DoCheckPoint( void )
{
	int rc;

	if (!checkPtFileNameBackup || (changed <= checkPtInterval+1)) {
		sprintf(sCheckPointBF,"%s00.bkp",GetLayoutFilename());
		MakeFullpath(&checkPtFileNameBackup, workingDir, sCheckPointBF, NULL);
	}

	rename( checkPtFileName1, checkPtFileName2 );

	rc = DoSaveTracks( checkPtFileName1 );

	/* could the check point file be written ok? */
	if ( rc ) {
		/* yes, archive/delete the backup copy of the checkpoint file */
		if (checkPtFileNameBackup) {
			char * spot = strrchr(checkPtFileNameBackup,'.');
			if (spot && spot>checkPtFileNameBackup+3) {
				spot[-2]=generations_count/10+'0';
				spot[-1]=generations_count%10+'0';
			}
			generations_count++;
			if (((autosaveChkPoints == 0) && (generations_count > 5)) ||
			    ((autosaveChkPoints > 0) && (generations_count > autosaveChkPoints)) ) {
				generations_count = 0;
			}
			remove( checkPtFileNameBackup);
			rename( checkPtFileName2, checkPtFileNameBackup );
		} else {
			remove(checkPtFileName2);
		}
	} else {
		/* no, rename the backup copy back to the checkpoint file name */
		rename( checkPtFileName2, checkPtFileName1 );
	}

	wShow( mainW );
}


static wIndex_t autosave_count = 0;
EXPORT wIndex_t checkPtMark = 0;
EXPORT long checkPtInterval = 10;
EXPORT long autosaveChkPoints = 0;

EXPORT void TryCheckPoint()
{
	if (checkPtInterval > 0
	    && changed >= checkPtMark + (wIndex_t) checkPtInterval
	    && !inPlayback) {
		DoCheckPoint();
		checkPtMark = changed;

		autosave_count++;

		if ((autosaveChkPoints>0) && (autosave_count>=autosaveChkPoints)) {
			if ( bReadOnly || *(GetLayoutFilename()) == '\0') {
				SetAutoSave();
			} else {
				DoSave(NULL);
			}
			InfoMessage(_("File AutoSaved"));
			autosave_count = 0;
		}
	}
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

EXPORT void CleanupCheckpointFiles( void )
{
	if ( checkPtFileName1 ) {
		if (checkPtFileNameBackup) {
			remove( checkPtFileNameBackup );
			rename( checkPtFileName1, checkPtFileNameBackup );
		}
		remove( checkPtFileName1 );
	}

}

/**
 * Remove all temporary files used for archive handling. When the program terminates
 * normally through the exit choice, files and directories that were created
 * temporarily are removed: zip_in.<pid> and zip_out.<pid>
 *
 * \param none
 * \return none
 *
 */

EXPORT void CleanupTempArchive(void)
{
	char* tempDir;

	for (int i = ARCHIVE_READ; i <= ARCHIVE_WRITE; ++i) {
		tempDir = GetZipDirectoryName(i);
		if (tempDir) {
			DeleteDirectory(tempDir);
			free(tempDir);
		}
	}
}

/**
 * Check for existence of checkpoint file. Existence of a checkpoint file means that XTrkCAD was not properly
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

	if ( !stat( checkPtFileName1, &fileStat ) ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * Load checkpoint file
 *
 * \param if TRUE reuse old filename
 * \param filename returned
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int LoadCheckpoint( BOOL_T sameName )
{
	char *search;

	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );

	MakeFullpath(&search, workingDir, sCheckPointF, NULL);
	UndoSuspend();

	if (ReadTrackFile( search, search + strlen(search) - strlen( sCheckPointF ),
	                   TRUE, TRUE, TRUE )) {
		ResolveIndex();
		LayoutBackGroundInit(FALSE);    //Get Prior BackGround
		LayoutBackGroundSave();		    //Save Background Values

		if (sameName) {
			long iExample;
			char * initialFile = (char*)wPrefGetString("misc", "lastlayout");
			wPrefGetInteger("misc", "lastlayoutexample", &iExample, 0);
			bExample = (iExample == 1);
			if (initialFile && strlen(initialFile)) {
				SetCurrentPath( LAYOUTPATHKEY, initialFile );
				SetLayoutFullPath(initialFile);
			}
		} else { SetLayoutFullPath(""); }

		RecomputeElevations(NULL);
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );

	} else { SetLayoutFullPath(""); }

	LayerSetCounts();
	UpdateLayerDlg(curLayer);

	Reset();
	UndoResume();

	wSetCursor( mainD.d, defaultCursor );


	SetWindowTitle();
	checkPtMark = changed = 1;
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
static struct wFilSel_t * importDxf_fs;

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

	CHECK( fileName != NULL );
	CHECK( cnt == 1 );

	nameOfFile = FindFilename(fileName[ 0 ]);
	paramVersion = -1;
	wSetCursor( mainD.d, wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );
	int saveLayer = curLayer;
	int layer = 0;
	if (importAsModule) {
		layer = FindUnusedLayer(0);
		if (layer==-1) { return FALSE; }
		char LayerName[80];
		LayerName[0] = '\0';
		sprintf(LayerName,_("Module - %s"),nameOfFile);
		if (layer>=0) { SetCurrLayer(layer, NULL, 0, NULL, NULL); }
		SetLayerName(layer,LayerName);
	}
	ImportStart();
	UndoStart( _("Import Tracks"), "importTracks" );
	useCurrentLayer = TRUE;
	ReadTrackFile( fileName[ 0 ], nameOfFile, FALSE, FALSE, TRUE );
	ImportEnd(zero, TRUE, FALSE);
	if (importAsModule) { SetLayerModule(layer,TRUE); }
	useCurrentLayer = FALSE;
	SetCurrLayer(saveLayer, NULL, 0, NULL, NULL);
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( mainD.d, defaultCursor );
	paramVersion = paramVersionOld;
	DoCommandB( I2VP(selectCmdInx) );
	SelectRecount();
	return TRUE;
}

static void DoImport( void * type )
{
	if (importFile_fs == NULL)
		importFile_fs = wFilSelCreate( mainW, FS_LOAD, 0,
		                               type == 0 ? _("Import Tracks") : _("Import Module"),
		                               sImportFilePattern, ImportTracks, NULL );

	wFilSelect( importFile_fs, GetCurrentPath(LAYOUTPATHKEY));
}

EXPORT void DoImportObjects( void * unused )
{
	importAsModule = FALSE;
	DoImport( unused );
}

EXPORT void DoImportModule( void * unused )
{
	importAsModule = TRUE;
	DoImport( unused );
}

/*******************************************************************************
 *
 * Import DXF Dialog
 *
 */

static void ReadDxfFile(const char*, const char*, BOOL_T);

static int ImportDXF(
        int cnt,
        char** fileName,
        void* data)
{
	char* nameOfFile;
	long paramVersionOld = paramVersion;

	CHECK(fileName != NULL);
	CHECK(cnt == 1);

	nameOfFile = FindFilename(fileName[0]);
	paramVersion = -1;
	wSetCursor(mainD.d, wCursorWait);
	Reset();
	SetAllTrackSelect(FALSE);

	ImportStart();
	UndoStart(_("Import DXF"), "importDXF");
	useCurrentLayer = TRUE;

	ReadDxfFile(fileName[0], nameOfFile, true);

	ImportEnd(zero, TRUE, FALSE);

	/*DoRedraw();*/
	EnableCommands();
	wSetCursor(mainD.d, defaultCursor);
	paramVersion = paramVersionOld;
	DoCommandB(I2VP(selectCmdInx));
	SelectRecount();

	return TRUE;
}

static int importDxfTrack = 0;
static int importDxfXti = 0;
static int importDxfModule = 0;

static char* importDxfTrackLabels[] = { N_("Layer 0 Track"), NULL };
static char* importDxfXtiLabels[] = { N_("Import XTI"), NULL };
static char* importDxfModLabels[] = { N_("Import as Module"), NULL };

static paramData_t importDxfPLs[] = {
	/*0*/ { PD_TOGGLE, &importDxfTrack, "track", PDO_NOPREF, &importDxfTrackLabels, NULL, BC_NOBORDER },
	/*1*/ { PD_TOGGLE, &importDxfXti, "xti", PDO_NOPREF, &importDxfXtiLabels, NULL, BC_NOBORDER },
	/*2*/ { PD_TOGGLE, &importDxfModule, "mod", PDO_NOPREF, &importDxfModLabels, NULL, BC_NOBORDER }
};
static paramGroup_t importDxfPG = { "importDxf", 0, importDxfPLs, COUNT(importDxfPLs) };
static wWin_p importDxfW;

EXPORT void DoImportDXF(void* unused)
{
	if (!importDxfW) {
		ParamRegister(&importDxfPG);
		importDxfW = ParamCreateDialog(&importDxfPG, MakeWindowTitle(_("Import DXF")),
		                               _("Ok"), importDXF, ParamCancel_Current,
		                               TRUE, NULL, 0, NULL);
		// blockD.dpi = mainD.dpi;
	}
	ParamLoadControls(&importDxfPG);
	wShow(importDxfW);
}

EXPORT void importDXF(void* unused)
{
	wHide(importDxfW);

	if (importDxf_fs == NULL)
		importDxf_fs = wFilSelCreate(mainW, FS_LOAD, 0,
		                             _("Import DXF"),
		                             sDXFFilePattern, ImportDXF, NULL);

	wFilSelect(importDxf_fs, GetCurrentPath(LAYOUTPATHKEY));
}

char dxfCode[20];
char dxfValue[50];

// Hex values (RGB) for all the 256 ACI Color indexes
static long aci[256] = {
	0x000000, 0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF, //007
	0x414141, 0x808080, 0xFF0000, 0xFFAAAA, 0xBD0000, 0xBD7E7E, 0x810000, 0x815656, //015
	0x680000, 0x684545, 0x4F0000, 0x4F3535, 0xFF3F00, 0xFFBFAA, 0xBD2E00, 0xBD8D7E, //023
	0x811F00, 0x816056, 0x681900, 0x684E45, 0x4F1300, 0x4F3B35, 0xFF7F00, 0xFFD4AA, //031
	0xBD5E00, 0xBD9D7E, 0x814000, 0x816B56, 0x683400, 0x685645, 0x4F2700, 0x4F4235, //039
	0xFFBF00, 0xFFEAAA, 0xBD8D00, 0xBDAD7E, 0x816000, 0x817656, 0x684E00, 0x685F45, //047
	0x4F3B00, 0x4F4935, 0xFFFF00, 0xFFFFAA, 0xBDBD00, 0xBDBD7E, 0x818100, 0x818156, //055
	0x686800, 0x686845, 0x4F4F00, 0x4F4F35, 0xBFFF00, 0xEAFFAA, 0x8DBD00, 0xADBD7E, //063
	0x608100, 0x768156, 0x4E6800, 0x5F6845, 0x3B4F00, 0x494F35, 0x7FFF00, 0xD4FFAA, //071
	0x5EBD00, 0x9DBD7E, 0x408100, 0x6B8156, 0x346800, 0x566845, 0x274F00, 0x424F35, //079
	0x3FFF00, 0xBFFFAA, 0x2EBD00, 0x8DBD7E, 0x1F8100, 0x608156, 0x196800, 0x4E6845, //087
	0x134F00, 0x3B4F35, 0x00FF00, 0xAAFFAA, 0x00BD00, 0x7EBD7E, 0x008100, 0x568156, //095
	0x006800, 0x456845, 0x004F00, 0x354F35, 0x00FF3F, 0xAAFFBF, 0x00BD2E, 0x7EBD8D, //103
	0x00811F, 0x568160, 0x006819, 0x45684E, 0x004F13, 0x354F3B, 0x00FF7F, 0xAAFFD4, //111
	0x00BD5E, 0x7EBD9D, 0x008140, 0x56816B, 0x006834, 0x456856, 0x004F27, 0x354F42, //119
	0x00FFBF, 0xAAFFEA, 0x00BD8D, 0x7EBDAD, 0x008160, 0x568176, 0x00684E, 0x45685F, //127
	0x004F3B, 0x354F49, 0x00FFFF, 0xAAFFFF, 0x00BDBD, 0x7EBDBD, 0x008181, 0x568181, //135
	0x006868, 0x456868, 0x004F4F, 0x354F4F, 0x00BFFF, 0xAAEAFF, 0x008DBD, 0x7EADBD, //143
	0x006081, 0x567681, 0x004E68, 0x455F68, 0x003B4F, 0x35494F, 0x007FFF, 0xAAD4FF, //151
	0x005EBD, 0x7E9DBD, 0x004081, 0x566B81, 0x003468, 0x455668, 0x00274F, 0x35424F, //159
	0x003FFF, 0xAABFFF, 0x002EBD, 0x7E8DBD, 0x001F81, 0x566081, 0x001968, 0x454E68, //167
	0x00134F, 0x353B4F, 0x0000FF, 0xAAAAFF, 0x0000BD, 0x7E7EBD, 0x000081, 0x565681, //175
	0x000068, 0x454568, 0x00004F, 0x35354F, 0x3F00FF, 0xBFAAFF, 0x2E00BD, 0x8D7EBD, //183
	0x1F0081, 0x605681, 0x190068, 0x4E4568, 0x13004F, 0x3B354F, 0x7F00FF, 0xD4AAFF, //191
	0x5E00BD, 0x9D7EBD, 0x400081, 0x6B5681, 0x340068, 0x564568, 0x27004F, 0x42354F, //199
	0xBF00FF, 0xEAAAFF, 0x8D00BD, 0xAD7EBD, 0x600081, 0x765681, 0x4E0068, 0x5F4568, //207
	0x3B004F, 0x49354F, 0xFF00FF, 0xFFAAFF, 0xBD00BD, 0xBD7EBD, 0x810081, 0x815681, //215
	0x680068, 0x684568, 0x4F004F, 0x4F354F, 0xFF00BF, 0xFFAAEA, 0xBD008D, 0xBD7EAD, //223
	0x810060, 0x815676, 0x68004E, 0x68455F, 0x4F003B, 0x4F3549, 0xFF007F, 0xFFAAD4, //231
	0xBD005E, 0xBD7E9D, 0x810040, 0x81566B, 0x680034, 0x684556, 0x4F0027, 0x4F3542, //239
	0xFF003F, 0xFFAABF, 0xBD002E, 0xBD7E8D, 0x81001F, 0x815660, 0x680019, 0x68454E, //247
	0x4F0013, 0x4F353B, 0x333333, 0x505050, 0x696969, 0x828282, 0xBEBEBE, 0xFFFFFF  //255
};

// Read the code/value pair that DXF uses
static bool ReadDxfPair(FILE* dxfFile)
{
	memset(dxfCode, 0, sizeof(dxfCode));
	memset(dxfValue, 0, sizeof(dxfValue));

	if (fgets(dxfCode, sizeof dxfCode, dxfFile) != NULL) {
		if (fgets(dxfValue, sizeof dxfValue, dxfFile) != NULL) {
			return true;
		}
	}
	return false;
}

// Get the line type code from the DXF text
static wDrawLineType_e dxfLineType(char dxfValue[50])
{
	wDrawLineType_e lineType = wDrawLineSolid;

	if (strncmp(dxfValue, "continuous", 10) == 0) {
		lineType = wDrawLineSolid;
	} else if (strncmp(dxfValue, "divide", 6) == 0) {
		lineType = wDrawLineDashDotDot;
	} else if (strncmp(dxfValue, "dashdot", 7) == 0) {
		lineType = wDrawLineDashDot;
	} else if (strncmp(dxfValue, "dot", 3) == 0) {
		lineType = wDrawLineDot;
	} else if (strncmp(dxfValue, "dashed", 6) == 0) {
		lineType = wDrawLineDash;
	} else if ((strncmp(dxfValue, "border", 6) == 0) ||
	           (strncmp(dxfValue, "center", 6) == 0)) {
		lineType = wDrawLineCenter;
	}

	return lineType;
}

// These differ from the XTC version as it changes
// the angle from DXF to XTC at the same time.
static double toDegrees(double a)
{
	return 90.0 + a * 180.0 / M_PI;
}

static double toRadians(double a)
{
	return (90.0 - a) * M_PI / 180.0;
}

#define MAX_DXF_LAYER 50		// Layer list size
#define MAX_DXF_ENDPT 100		// End point list
#define DXF_OUTPUT_ALLOC 64		// Amount to grow the output

int DxfOutputAlloc;				// Current size
int DxfOutputCount;
char** DxfOutput;				// Track Output lines

// Allocate memory for a line of output and assign to output
// Reallocate array space if needed
static BOOL_T dxfAddOutput(char tmp[])
{
	size_t len = strlen(tmp) + 1;
	char* outpt = MyMalloc(len * sizeof(char));

	strncpy(outpt, tmp, len);

	if (DxfOutputCount == DxfOutputAlloc) {
		DxfOutputAlloc += DXF_OUTPUT_ALLOC;
		int mSize = DxfOutputAlloc * sizeof(char*);
		DxfOutput = MyRealloc(DxfOutput, mSize);
	}
	DxfOutput[DxfOutputCount++] = outpt;

	return TRUE;
}

#define TMP_SIZE 100		// tmp buffer size
#define DXF_DEBUG TRUE

static void dxfDebugMessage(int ok, const char *fileName)
{
	if (ok < 0) {
		NoticeMessage(MSG_DXF_LINE_ERROR, _("Ok"), NULL, fileName);
	} else if (ok > TMP_SIZE) {
		NoticeMessage(MSG_DXF_LINE_LEN, _("Ok"), NULL, fileName, ok, TMP_SIZE);
	}
}

// The main ReadDxfFile function
static void ReadDxfFile(
        const char* pathName,
        const char* fileName,
        BOOL_T complain)
{
	FILE* dxfFile;
	FILE* xtiFile;

	time_t clock;

	int count;

	char tmp[TMP_SIZE];
	int ok;

	char dxfSection[20];
	char dxfGroup[20];
	enum enumSection {
		eNone,
		eLine,
		eArc,
		eCircle,
		ePoly,
		eText
	};
	enum enumSection eSection;

	DxfOutputAlloc = DXF_OUTPUT_ALLOC;
	DxfOutputCount = 0;
	int mSize = DxfOutputAlloc * sizeof(char*);
	DxfOutput = MyMalloc(mSize);

	bool inEntities = false;
	bool inLayers = false;

	struct sLayer {
		int line;
		char* name;
		int color;
		int lineType;
		double thick;
	};
	struct sLayer layer[MAX_DXF_LAYER];


	struct sEndPt {
		int entity;
		int line;
		coOrd coord;
		double angle;
	};

	int endPtCount = 0;
	struct sEndPt endPt[MAX_DXF_ENDPT];


	int layerCount = 0;
	int entityCount = 0;

	char dxfLayer[50];

	int color = 0;
	int colorRGB = -1;
	wDrawLineType_e lineType;
	double thick = 0.0;
	double radius = 0.0;
	double startAngle = 0.0;
	double endAngle = 0.0;
	int closed = 0;
	double x1 = 0.0;
	double y1 = 0.0;
	double x2 = 0.0;
	double y2 = 0.0;
	int layerIdx = 0;
	int visibility = 2;

	double vrt_x[20];
	double vrt_y[20];
	int vertices = 0;

	bool dxfDirty = false;

	memset(dxfSection, 0, sizeof(dxfSection));

	dxfFile = fopen(pathName, "r");
	if (dxfFile == NULL) {
		if (complain) {
			NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, sProdName, pathName,
			              strerror(errno));
		}
		return;
	}

	// ParamSetInReadTracks(TRUE);
	SetCLocale();
	checkPtFileNameBackup = NULL;

	// Header
	time(&clock);
	sprintf(tmp, "#%s Version: %s, Date: %s", sProdName, sVersion, ctime(&clock));
	dxfAddOutput(tmp);
	sprintf(tmp, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION);
	dxfAddOutput(tmp);

	eSection = eNone;

	count = 0;
	while (dxfFile
	       && ReadDxfPair(dxfFile)) {
		count += 2;
		// BOOL_T old_skip = skip;
		// skip = FALSE;
		if (count % 10 == 0) {
			InfoMessage("%d", count);
			wFlush();
		}

		Stripcr(dxfCode);
		Stripcr(dxfValue);

		if (strncmp(dxfCode, "  0", 3) == 0) {
			// Write previous entity
			if (dxfDirty) {

				// Write the layer data into a list
				if (inLayers) {
					if (layerCount < MAX_DXF_LAYER) {
						layer[layerCount].line = count;
						size_t len = strlen(dxfGroup);
						layer[layerCount].name = MyMalloc((len + 1) * sizeof(char));
						strncpy(layer[layerCount].name, dxfGroup, len);
						layer[layerCount].color = (colorRGB >= 0 ? colorRGB : (color == 7 ? 0 :
						                           (color > 0 && color < 256 ? color : 0)));
						layer[layerCount].lineType = lineType;
						layer[layerCount].thick = thick;

						layerCount++;
					} else {
						NoticeMessage(MSG_TOO_MANY_LAYERS, _("Ok"), NULL, fileName, MAX_DXF_LAYER);
						return;
					}
				}

				// Write the Entity data into the list
				else if (inEntities) {
					BOOL_T isTrack = (strncmp(dxfLayer, layer[0].name, sizeof dxfLayer) == 0);

					layerIdx = curLayer;

					if (color < 0 || lineType < 0 || thick < 0) {
						for (int i = 0; i < layerCount; i++) {
							if (strncmp(dxfLayer, layer[i].name, sizeof dxfLayer) == 0) {
								if (color < 0) {
									color = layer[i].color;
								}
								if (lineType < 0) {
									lineType = layer[i].lineType;
								}
								if (thick < 0) {
									thick = layer[i].thick;
								}
							}
						}
					}

					// Create the entity. Start numbering with 1
					entityCount++;

					int useColor = (colorRGB >= 0 ? colorRGB : color);

					if (eSection == eLine) {
						// Save Entity data
						if (isTrack && importDxfTrack == 1) {
							// STRAIGHT index layer line-width 0 0 scale descshow&visibility&no_ties&bridge&roadbed Desc-x Desc-y
							//STRAIGHT 1 1 0 0 0 HO 2 0.000000 0.000000
							//	E4 0.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	T4 2 21.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	END$SEGS
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d 0 0 %s %d %f %f",
							              "STRAIGHT", entityCount, layerIdx, 0, curScaleName, visibility, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							// End points
							double a1 = NormalizeAngle(-toDegrees(atan2((y2 - y1), (x2 - x1))));
							double a2 = NormalizeAngle(180 + a1);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %f %f %f 0 0.0 0.0 0.0 0.0 0 0 0 0.000000",
							              "E4", x1, y1, a1);

							if (endPtCount < MAX_DXF_ENDPT) {
								endPt[endPtCount].entity = entityCount;
								endPt[endPtCount].line = DxfOutputCount;
								endPt[endPtCount].coord.x = x1; endPt[endPtCount].coord.y = y1;
								endPt[endPtCount].angle = a1;
								endPtCount++;
							} else {
								NoticeMessage(MSG_TOO_MANY_ENDPTS, _("Ok"), NULL, fileName, MAX_DXF_ENDPT);
								return;
							}
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %f %f %f 0 0.0 0.0 0.0 0.0 0 0 0 0.000000",
							              "E4", x2, y2, a2);

							if (endPtCount < MAX_DXF_ENDPT) {
								endPt[endPtCount].entity = entityCount;
								endPt[endPtCount].line = DxfOutputCount;
								endPt[endPtCount].coord.x = x2; endPt[endPtCount].coord.y = y2;
								endPt[endPtCount].angle = a2;
								endPtCount++;
							} else {
								NoticeMessage(MSG_TOO_MANY_ENDPTS, _("Ok"), NULL, fileName, MAX_DXF_ENDPT);
								return;
							}
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$SEGS");
							dxfAddOutput(tmp);
						} else {
							// DRAW 1 0 0 0 0 -2.000000 -4.500000 0 0.000000
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %f %f 0 %f",
							              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %d %f %f %f 0 %f %f 0",
							              "L3", useColor, thick, x1, y1, x2, y2);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$SEGS");
							dxfAddOutput(tmp);
						}
					} else if (eSection == eCircle) {
						// DRAW 1 0 0 0 0 -2.000000 -4.500000 0 0.000000
						ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %f %f 0 %f",
						              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
						dxfAddOutput(tmp);
						dxfDebugMessage(ok, fileName);

						ok = snprintf(tmp, TMP_SIZE, "\t%s %d %f %f %f %f 0",
						              "G3", useColor, thick, radius, x1, y1);
						dxfAddOutput(tmp);
						dxfDebugMessage(ok, fileName);

						ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$SEGS");
						dxfAddOutput(tmp);

					} else if (eSection == eArc) {
						// Save Entity data
						if (isTrack && importDxfTrack == 1) {
							//CURVE index layer line-width 0 0 scale visibility&no_ties&bridge&roadbed center-X centerY 0 radius helix-turns desc-X desc-Y
							//CURVE 2 1 0 0 0 OO 2 0.000000 12.000000 0 12.000000 0 0.000000 0.000000
							//	E4 0.000000 0.000000 270.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	T4 2 21.000000 0.000000 90.000000 0 0.0 0.0 0.0 0.0 0 0 0 0.000000
							//	END$SEGS

							// Save Entity data
							// End points
							//double end = NormalizeAngle(startAngle + endAngle);

							double xEndAngle = NormalizeAngle(-startAngle);
							double xStartAngle = NormalizeAngle(-endAngle);
							//double xCurveStart = NormalizeAngle(90.0 - endAngle);

							double x3 = x1 + radius * sin(toRadians(xStartAngle));
							double y3 = y1 - radius * cos(toRadians(xStartAngle));
							double x4 = x1 + radius * sin(toRadians(xEndAngle));
							double y4 = y1 - radius * cos(toRadians(xEndAngle));
							//double dx = x3 - x4;

							double e3 = NormalizeAngle(xStartAngle);
							double e4 = NormalizeAngle(xEndAngle - 180.0);
							//double xCurveAngle = NormalizeAngle(endAngle - startAngle);

							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d 0 0 %s %d %f %f 0 %f 0 %f %f",
							              "CURVE", entityCount, layerIdx, 0, curScaleName, visibility, x1, y1, radius,
							              0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %f %f %f 0 0.0 0.0 0.0 0.0 0 0 0 0.000000",
							              "E4", x3, y3, e3);

							if (endPtCount < MAX_DXF_ENDPT) {
								endPt[endPtCount].entity = entityCount;
								endPt[endPtCount].line = DxfOutputCount;
								endPt[endPtCount].coord.x = x3; endPt[endPtCount].coord.y = y3;
								endPt[endPtCount].angle = e3;
								endPtCount++;
							} else {
								NoticeMessage(MSG_TOO_MANY_ENDPTS, _("Ok"), NULL, fileName, MAX_DXF_ENDPT);
								return;
							}
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %f %f %f 0 0.0 0.0 0.0 0.0 0 0 0 0.000000",
							              "E4", x4, y4, e4);
							if (endPtCount < MAX_DXF_ENDPT) {
								endPt[endPtCount].entity = entityCount;
								endPt[endPtCount].line = DxfOutputCount;
								endPt[endPtCount].coord.x = x4; endPt[endPtCount].coord.y = y4;
								endPt[endPtCount].angle = e4;
								endPtCount++;
							} else {
								NoticeMessage(MSG_TOO_MANY_ENDPTS, _("Ok"), NULL, fileName, MAX_DXF_ENDPT);
								return;
							}
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$SEGS");
							dxfAddOutput(tmp);

						} else {
							double xStartAngle = NormalizeAngle(90.0 - endAngle);
							double xCurveAngle = endAngle - startAngle;

							// DRAW 1 0 0 0 0 -2.000000 -4.500000 0 0.000000
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %f %f 0 %f",
							              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s %d %f %f %f %f 0 %f %f",
							              "A3", useColor, thick, radius, x1, y1, xStartAngle, xCurveAngle);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$SEGS");
							dxfAddOutput(tmp);
						}
					} else

						if (eSection == ePoly) {
							// Save Entity data
							ok = snprintf(tmp, TMP_SIZE, "%s %d %d %d %d 0 %f %f 0 %f",
							              "DRAW", entityCount, layerIdx, lineType, 0, 0.0, 0.0, 0.0);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							// Detect polylines that are actually cloesd
							if (vertices > 1 && vrt_x[0] == vrt_x[vertices - 1]
							    && vrt_y[0] == vrt_y[vertices - 1]) {
								closed = 1;
								-- vertices;
							}

							if (closed == 1)
								ok = snprintf(tmp, TMP_SIZE, "\t%s %d %f %d 0",
								              "F4", useColor, thick, vertices);
							else
								ok = snprintf(tmp, TMP_SIZE, "\t%s %d %f %d 2",
								              "Y4", useColor, thick, vertices);
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

							for (int v = 0; v < vertices; v++) {
								ok = snprintf(tmp, TMP_SIZE, "\t\t%f %f 0", vrt_x[v], vrt_y[v]);
								dxfAddOutput(tmp);
							}

							ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$SEGS");
							dxfAddOutput(tmp);
							dxfDebugMessage(ok, fileName);

						} else

							if (eSection == eText) {
								// Process Text data
							}
				}

				dxfDirty = false;

				// Make sure previous values are not used
				vertices = 0;
				color = 0;
				colorRGB = -1;
				lineType = 0;
				thick = 0.0;
				radius = 0.0;
				startAngle = 0.0;
				endAngle = 0.0;
				closed = 0;
				x1 = 0.0;
				y1 = 0.0;
				x2 = 0.0;
				y2 = 0.0;
			}

			if (inEntities) {
				if (strncmp(dxfValue, "ARC", 3) == 0) {
					eSection = eArc;
				} else if (strncmp(dxfValue, "CIRCLE", 6) == 0) {
					eSection = eCircle;
				} else if (strncmp(dxfValue, "LINE", 4) == 0) {
					eSection = eLine;
				} else if ((strncmp(dxfValue, "POLYLINE", 8) == 0) ||
				           (strncmp(dxfValue, "LWPOLYLINE", 10) == 0)) {
					eSection = ePoly;
				} else if (strncmp(dxfValue, "MTEXT", 5) == 0) {
					eSection = eText;
				} else {
					eSection = eNone;
				}

				if (eSection != eNone) {
					strncpy(dxfSection, dxfValue, 20);
					dxfDirty = true;
				}
			} else {
				strncpy(dxfSection, dxfValue, sizeof dxfSection);
				strncpy(dxfGroup, "X", 2);

				//if (strncmp(dxfValue, "EOF", 3) == 0)
				//	break;
			}
		}

		if (strncmp(dxfCode, "  2", 3) == 0) {
			if ((inLayers) &&
			    (strncmp(dxfSection, "LAYER", 5) == 0)) {
				strncpy(dxfGroup, dxfValue, sizeof dxfGroup);
				dxfDirty = true;
			}

			// This signals the start of layer definitions
			if ((strncmp(dxfSection, "TABLE", 5) == 0) &&
			    (strncmp(dxfValue, "LAYER", 5) == 0)) {
				inLayers = true;
			}

			// This signals the start of the entity definitions
			if ((strncmp(dxfSection, "SECTION", 7) == 0) &&
			    (strncmp(dxfValue, "ENTITIES", 8) == 0)) {
				strncpy(dxfGroup, dxfValue, sizeof dxfGroup);
				inEntities = true;
				inLayers = false;
			}
		}

		// Mundane data
		if (strncmp(dxfCode, "  6", 3) == 0) {
			for (int i = 0; dxfValue[i]; i++) {
				dxfValue[i] = tolower(dxfValue[i]);
			}
			if (strncmp(dxfValue, "bylayer", 7) == 0) {
				lineType = -1;
			} else {
				lineType = dxfLineType(dxfValue);
			}
		} else if (strncmp(dxfCode, "  8", 3) == 0) {
			strncpy(dxfLayer, dxfValue, sizeof dxfLayer);
		} else if (strncmp(dxfCode, " 10", 3) == 0) {
			if (eSection == ePoly) {
				vrt_x[vertices] = strtod(dxfValue, NULL);
			} else {
				x1 = strtod(dxfValue, NULL);
			}
		} else if (strncmp(dxfCode, " 11", 3) == 0) {
			x2 = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 20", 3) == 0) {
			if (eSection == ePoly) {
				vrt_y[vertices] = strtod(dxfValue, NULL);
				vertices++;
			} else {
				y1 = strtod(dxfValue, NULL);
			}
		} else if (strncmp(dxfCode, " 21", 3) == 0) {
			y2 = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 39", 3) == 0) {
			if (strncmp(dxfValue, "bylayer", 7) == 0) {
				thick = -1;
			} else {
				thick = strtod(dxfValue, NULL) / 250;
			}
		} else if (strncmp(dxfCode, " 40", 3) == 0) {
			radius = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 50", 3) == 0) {
			startAngle = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 51", 3) == 0) {
			endAngle = strtod(dxfValue, NULL);
		} else if (strncmp(dxfCode, " 62", 3) == 0) {
			long c = strtol(dxfValue, NULL, 10);
			if ((c > 0) && (c < 256)) {
				if (c == 7) {
					color = 0;        // Black
				} else {
					color = aci[c];
				}
			} else {
				color = -1; // ByLayer
			}
		} else if (strncmp(dxfCode, " 70", 3) == 0) {
			closed = strtol(dxfValue, NULL, 10);
		} else if (strncmp(dxfCode, "370", 3) == 0) {
			thick = strtod(dxfValue, NULL);
			if (thick > 0) {
				thick = thick / 250;
			}
		} else if (strncmp(dxfCode, "420", 3) == 0) {
			colorRGB = strtol(dxfValue, NULL, 10);
		}
	}

	ok = snprintf(tmp, TMP_SIZE, "\t%s", "END$TRACKS");
	dxfAddOutput(tmp);

	// Find and fix connected end points
	int i;
	int j;
	for (i = 0; i < endPtCount; i++)
		for (j = 0; j < endPtCount; j++) {
			if (i != j) {
				// Check if [i] should be connected to [j]
				double d = FindDistance(endPt[i].coord, endPt[j].coord);
				if (d <= connectDistance) {
					int li = endPt[i].line;
					char substr[TMP_SIZE];
					strncpy(substr, DxfOutput[li], TMP_SIZE);
					sprintf(tmp, "\tT4 %d %s", endPt[j].entity, substr + 4);
					DxfOutput[li] = MyRealloc(DxfOutput[li], strlen(tmp));
					strncpy(DxfOutput[li], tmp, strlen(tmp));
				}
			}
		}

	if (dxfFile) {
		fclose(dxfFile);
		dxfFile = NULL;
	}

	// Change the extension to create the XTI file
	char* p = strstr(pathName, ".dxf");
	if (p == NULL) {
		p = strstr(pathName, ".DXF");        // stupid Windows
	}
	if (p != NULL) {
		memcpy(p, ".xti", 4);
	}

	xtiFile = fopen(pathName, "w");
	if (xtiFile == NULL) {
		if (complain) {
			NoticeMessage(MSG_OPEN_FAIL, _("Ok"), NULL, sProdName, pathName,
			              strerror(errno));
		}
		return;
	}
	for (int i = 0; i < DxfOutputCount; i++) {
		fprintf(xtiFile, "%s\n", DxfOutput[i]);
	}
	if (xtiFile) {
		fclose(xtiFile);
	}


	// Clean up
	SetUserLocale();

	for (int i = 0; i < DxfOutputCount; i++) {
		MyFree(DxfOutput[i]);
	}
	MyFree(DxfOutput);

	for (int i = 0; i < layerCount; i++) {
		MyFree(layer[i].name);
	}


	// Import the XTI file
	if (importDxfXti == 1) {

		int saveLayer = curLayer;
		int layer = 0;

		if (importDxfModule) {
			layer = FindUnusedLayer(0);
			if (layer == -1) {
				NoticeMessage(MSG_NO_EMPTY_LAYER, _("Ok"), NULL, pathName, 0);
				return;
			}
			char LayerName[80];
			LayerName[0] = '\0';
			sprintf(LayerName, _("Module - %s"), fileName);
			if (layer >= 0) { SetCurrLayer(layer, NULL, 0, NULL, NULL); }
			SetLayerName(layer, LayerName);
		}

		ParamSetInReadTracks(TRUE);
		BOOL_T ret = ReadTrackFile(pathName, fileName, FALSE, TRUE, TRUE);

		if (ret) {
			if (importDxfModule) { SetLayerModule(layer, TRUE); }
			useCurrentLayer = FALSE;
		}
		SetCurrLayer(saveLayer, NULL, 0, NULL, NULL);
	}

	InfoMessage("%d", count);
	return;
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

	CHECK( fileName != NULL );
	CHECK( cnt == 1 );

	SetCurrentPath( IMPORTPATHKEY, fileName[ 0 ] );
	f = fopen( fileName[ 0 ], "w" );
	if (f==NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Export"), fileName[0],
		               strerror(errno) );
		return FALSE;
	}

	SetCLocale();

	wSetCursor( mainD.d, wCursorWait );
	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	coOrd offset;
	ExportTracks( f, &offset );
	fprintf(f, "%s\n", END_TRK_FILE);
	fclose(f);

	SetUserLocale();

	Reset();
	wSetCursor( mainD.d, defaultCursor );
	UpdateAllElevations();
	return TRUE;
}


EXPORT void DoExport( void * unused )
{
	if (exportFile_fs == NULL)
		exportFile_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Export Tracks"),
		                               sImportFilePattern, DoExportTracks, NULL );

	wFilSelect( exportFile_fs, GetCurrentPath(LAYOUTPATHKEY));
}


EXPORT wBool_t editStatus = TRUE;

EXPORT void EditCopy( void * unused )
{
	editStatus = FALSE;
	FILE * f;
	time_t clock;

	if (selectedTrackCount <= 0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	f = fopen( clipBoardN, "w" );
	if (f == NULL) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Clipboard"), clipBoardN,
		               strerror(errno) );
		return;
	}

	SetCLocale();

	time(&clock);
	fprintf(f,"#%s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&clock) );
	fprintf(f, "VERSION %d %s\n", iParamVersion, PARAMVERSIONVERSION );
	ExportTracks(f, &paste_offset );
	fprintf(f, "%s\n", END_TRK_FILE );
	SetUserLocale();
	fclose(f);

	editStatus = TRUE;
}


EXPORT void EditCut( void * unused )
{
	EditCopy(NULL);
	if ( !editStatus ) { return; }
	SelectDelete();
}


/**
 * Paste clipboard content. XTrackCAD uses a disk file as clipboard replacement. This file is read and the
 * content is inserted.
 *
 * \return    TRUE if success, FALSE on error (file not found)
 */

static BOOL_T EditPastePlace( wBool_t inPlace )
{

	BOOL_T rc = TRUE;

	wSetCursor( mainD.d, wCursorWait );
	Reset();
	SetAllTrackSelect( FALSE );

	double offset = 20*mainD.scale/mainD.dpi;

	paste_offset.x += offset;
	paste_offset.y += offset;


	ImportStart();
	UndoStart( _("Paste"), "paste" );
	useCurrentLayer = TRUE;
	if ( !ReadTrackFile( clipBoardN, sClipboardF, FALSE, TRUE, FALSE ) ) {
		NoticeMessage( MSG_CANT_PASTE, _("Continue"), NULL );
		rc = FALSE;
	}
	if (inPlace) {
		ImportEnd(paste_offset, FALSE, TRUE);
	} else {
		ImportEnd(zero, FALSE, FALSE);
	}
	useCurrentLayer = FALSE;
	/*DoRedraw();*/
	EnableCommands();
	wSetCursor( mainD.d, defaultCursor );
	DoCommandB( I2VP(selectCmdInx) );
	SelectRecount();
	UpdateAllElevations();

	return rc;
}

EXPORT void EditPaste( void * unused )
{
	editStatus = EditPastePlace(FALSE);
}

EXPORT void EditClone( void * unused )
{
	EditCopy( NULL );
	if ( !editStatus ) { return; }
	editStatus = EditPastePlace(TRUE);
}

/*****************************************************************************
 *
 * INITIALIZATION
 *
 */


EXPORT void LoadFileList(void)
{
	DynString key;
	const char * cp;
	const char *fileName, *pathName;

	DynStringMalloc(&key, 0);

	for (int inx = 0; inx <NUM_FILELIST; inx++) {

		DynStringPrintf(&key, RECENT_FILE_KEY "%d", inx);
		cp = wPrefGetString(RECENT_FILE_SECTION, DynStringToCStr(&key));
		if (!cp) {
			continue;
		}
		pathName = MyStrdup(cp);
		fileName = FindFilename((char *) pathName);
		if (fileName) {
			wMenuListAdd(fileList_ml, 0, fileName, pathName);
		}
	}

	DynStringFree(&key);
}



EXPORT void FileInit( void )
{
	libDir = wGetAppLibDir();
	CHECK( libDir );
	workingDir = wGetAppWorkDir();
	CHECK( workingDir );

	SetLayoutFullPath("");
	MakeFullpath(&clipBoardN, workingDir, sClipboardF, NULL);
	LocaleInit();

	log_timereadfile = LogFindIndex( "timereadfile" );
}
