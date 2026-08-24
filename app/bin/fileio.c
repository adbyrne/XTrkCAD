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
#include "form.h"

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

/** @logcmd @showrefby `timereadfile=n` `fileio.c` */
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


/* InputError() moved to getargs.c (2026-07-11) -- see that file for why. */

EXPORT void SyntaxError(
        char * event,
        wIndex_t actual,
        wIndex_t expected )
{
	InputError( "%s scan returned %d (expected %d)",
	            TRUE, event, actual, expected );
}


/* GetArgs() moved to getargs.c (2026-07-11) -- see that file for why. */



wBool_t IsEND( const char * sEnd )
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
 * \param[in] name command
 * \param[in] proc function for reading the parameter definition
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
	const char *filename;

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

/**
 * Compute checkPtFileName1/checkPtFileName2 if not already done. DoCheckPoint()
 * needs both set before it can save/rename a checkpoint; historically only
 * ExistsCheckpoint() computed them, as a side effect of its own unrelated
 * existence check, so any startup path that skipped calling ExistsCheckpoint()
 * (e.g. -T regression-test mode, which has no reason to probe for a resumable
 * checkpoint) left DoCheckPoint() trying to save to a NULL filename the first
 * time a checkpoint was actually due. Idempotent and cheap, so both callers
 * can call it unconditionally rather than relying on call order.
 */
static void EnsureCheckpointFileNames( void )
{
	MakeFullpath(&checkPtFileName1, workingDir, sCheckPointF, NULL);
	MakeFullpath(&checkPtFileName2, workingDir, sCheckPoint1F, NULL);
}

/** Read the layout design.
 *
 * \param[in] pathName filename including directory
 * \param[in] fileName pointer to filename part in pathName
 * \param[in] full
 * \param[in] noSetCurDir if FALSE current directory is changed to file location
 * \param[in] complain  if FALSE error messages are supressed
 *
 * \return FALSE in case of load error
 */

BOOL_T ReadTrackFile(
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

	bReadOnly = bExample || ( access( fileName[0], W_OK ) == -1 );

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

void AddToRecentFiles(const char* displayFileName, const char* fullFileName)
{
	if (!bExample && (displayFileName != NULL)) {
		const char* copyFile = MyStrdup(fullFileName);
		const char* listName = FindFilename(strdup(fullFileName));
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
        const char * label,
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
 * \param[in] name
 * \param[in] target_dir
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
	FormUpdatePrefs();

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
		examplesFile_fs = wFilSelCreate( mainW, FS_LOAD, FSO_SETFOLDERALWAYS,
		                                 _("Example Tracks"),
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

	EnsureCheckpointFileNames();

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
 * temporarily are removed: zip_in.\<pid\> and zip_out.\<pid\>
 *
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
 * \return TRUE if exists, FALSE otherwise
 *
 */

EXPORT int ExistsCheckpoint( void )
{
	struct stat fileStat;

	EnsureCheckpointFileNames();

	if ( !stat( checkPtFileName1, &fileStat ) ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * Load checkpoint file
 *
 * \param sameName IN TRUE reuse old filename
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
			const char * initialFile = (char*)wPrefGetString("misc", "lastlayout");
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
	const char *nameOfFile;
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

static void DoImport( const void * type )
{
	if (importFile_fs == NULL)
		importFile_fs = wFilSelCreate( mainW, FS_LOAD, 0,
		                               type == 0 ? _("Import Tracks") : _("Import Module"),
		                               sImportFilePattern, ImportTracks, NULL );

	wFilSelect( importFile_fs, GetCurrentPath(LAYOUTPATHKEY));
}

EXPORT void DoImportObjects( const void * unused )
{
	importAsModule = FALSE;
	DoImport( unused );
}

EXPORT void DoImportModule( const void * unused )
{
	importAsModule = TRUE;
	DoImport( unused );
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
