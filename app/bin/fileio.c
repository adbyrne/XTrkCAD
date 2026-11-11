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
#include "note.h"
#include "include/notenames.h"
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

/**
 * Create or truncate a file for writing with an explicit mode, owner read
 * and write, instead of leaving it all to the umask as fopen() does
 * (CodeQL cpp/world-writable-file-creation). As FOpenRestricted() in
 * genmessages.c.
 *
 * \param path IN the file
 * \return the open file, or NULL
 */
static FILE * FOpenWriteRestricted( const char * path )
{
#ifdef WINDOWS
	int fd = _open( path, _O_CREAT | _O_WRONLY | _O_TRUNC,
	                _S_IREAD | _S_IWRITE );
	return fd < 0 ? NULL : _fdopen( fd, "w" );
#else
	int fd = open( path, O_CREAT | O_WRONLY | O_TRUNC, 0644 );
	return fd < 0 ? NULL : fdopen( fd, "w" );
#endif
}

/**
 * Open a file for appending, creating it with the same explicit mode as
 * FOpenWriteRestricted(), instead of fopen()'s umask-only default.
 *
 * \param path IN the file
 * \return the open file, or NULL
 */
EXPORT FILE * FOpenAppendRestricted( const char * path )
{
#ifdef WINDOWS
	int fd = _open( path, _O_CREAT | _O_WRONLY | _O_APPEND,
	                _S_IREAD | _S_IWRITE );
	return fd < 0 ? NULL : _fdopen( fd, "a" );
#else
	int fd = open( path, O_CREAT | O_WRONLY | O_APPEND, 0644 );
	return fd < 0 ? NULL : fdopen( fd, "a" );
#endif
}

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


/* GetNextLine(), IsEND() and ReadMultilineText() are in filelines.c. */

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
 * \param[in] complain  if FALSE error messages are suppressed
 *
 * \return FALSE in case of load error
 */

/** TRUE if the last ReadTrackFile() stopped early: the file ended inside an
 * object, or the user chose Stop after an input error. */
static BOOL_T readTrackFileIncomplete = FALSE;

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
			ret = InputError( "Line too long", TRUE );
			if ( !ret ) {
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
				// Not read: the caller must not treat it as loaded, or Save would
				// write the empty layout over the newer file.
				ret = FALSE;
				break;
			}
			if ( paramVersion < iMinParamVersion ) {
				NoticeMessage( MSG_BAD_FILE_VERSION, _("Ok"), NULL, paramVersion,
				               iMinParamVersion, sProdName );
				ret = FALSE;
				break;
			}
		} else if (!full) {
			ret = InputError( "unknown command", TRUE );
			if ( !ret ) {
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
				ret = InputError( "ROOMSIZE: bad value", TRUE );
				if ( !ret ) {
					break;
				}
			}
		} else if (strncmp( paramLine, "SCALE ", 6 ) == 0) {
			if ( !DoSetScale( paramLine+6 ) ) {
				ret = InputError( "SCALE: bad value", TRUE );
				if ( !ret ) {
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
		} else if (strncmp( paramLine, "LAYERGROUP ", 11 ) == 0) {
			ReadLayerGroups( paramLine+11 );
		} else if (strncmp( paramLine, "MANAGENOTES ", 12 ) == 0) {
			ReadNoteNames( paramLine+12 );
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
	readTrackFileIncomplete = (paramFile == NULL);
	if (paramFile) {
		fclose(paramFile);
		paramFile = NULL;
	}

	/* Layer Groups (SF #222 phase 0, SF #782): a full layout load whose
	 * file predates Layer Groups gets its old per-layer Linked-Layers
	 * data migrated into groups exactly once, here, after every LAYERS/
	 * LAYERGROUP line has already been read. */
	if ( full && paramVersion < 13 ) {
		MigrateLayerLinksToGroups();
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
	NoteNameResetAll();
	checkPtMark = changed = 0;
	if (!data) {
		LayoutBackGroundInit(
		        TRUE);        //Keep values of background -> will be overridden by archive
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
			// No usable manifest: the layout is named like the archive, with
			// .xtc for .xtce. Drop the last letter: shifting the extension left
			// made it "tce", so the layout was never found (Development Issues #75).
			if (strcmp(extOfFile, ZIPFILETYPEEXTENSION )==0) {
				extOfFile[strlen(extOfFile)-1] = '\0';
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
	BOOL_T loaded = loadXTC
	                && ReadTrackFile( full_path, FindFilename( fileName[0]), TRUE, TRUE, TRUE )
	                && !readTrackFileIncomplete;
	if (loaded) {

		//nameOfFile = NULL;
		//extOfFile = NULL;
		SetCurrentPath( LAYOUTPATHKEY, copyOfFileName );
		SetLayoutFullPath(copyOfFileName);
		SetWindowTitle();

		AddToRecentFiles(nameOfFile, fileName[0]);
		// The work the checkpoint held has been replaced. Not before: after
		// a failed load the checkpoint is the only copy of it.
		CleanupCheckpointFiles();

		ResolveIndex();
		LOG( log_timereadfile, 1, ( "Read time (%s) = %lu mS \n", fileName[0],
		                            wGetTimer()-time0 ) );
		RecomputeElevations(NULL);
		AttachTrains();
		DoChangeNotification( CHANGE_ALL );
		LayerSetCounts();
	} else {
		/* The previous layout is already gone. Don't keep a partial layout
		 * that could be saved over a good file. */
		ClearTracks();
		ResetLayers();
		NoteNameResetAll();
		checkPtMark = changed = 0;
		SetLayoutFullPath("");
		SetWindowTitle();
		DoChangeNotification( CHANGE_MAIN|CHANGE_MAP );
	}

	MyFree(copyOfFileName);
	free(full_path);
	full_path = NULL;

	UpdateLayerDlg(curLayer);

	UndoResume();
	Reset();
	wSetCursor( mainD.d, defaultCursor );
	if (!loaded) {
		InfoMessage( _("The layout file could not be loaded") );
	}
	return TRUE;
}

void AddToRecentFiles(const char* displayFileName, const char* fullFileName)
{
	if (!bExample && (displayFileName != NULL)) {
		const char* copyFile = MyStrdup(fullFileName);
		const char* listName = FindFilename(strdup(fullFileName));
		wMenuListAdd(fileList_ml, 0, listName, copyFile);
		RecentFilesMenuUpdate();

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
	rc &= WriteNoteNames( f );
	rc &= WriteMainNote( f );
	rc &= WriteTracks( f, TRUE );
	rc &= fprintf(f, "%s\n", END_TRK_FILE)>0;
	// Writes are buffered, so a full disk may only show up in fclose(), and
	// fprintf() can succeed while an earlier write failed (Development Issues
	// #75). Check both, or a failed save is reported as done.
	int err = rc ? 0 : errno;
	if ( ferror(f) ) {
		rc = FALSE;
		if ( !err ) { err = errno; }
	}
	if ( fclose(f) != 0 ) {
		rc = FALSE;
		if ( !err ) { err = errno; }
	}
	if ( !rc ) {
		NoticeMessage( MSG_WRITE_FAILURE, _("Ok"), NULL, strerror(err ? err : EIO),
		               fileName );
	}
	bReadOnly = FALSE;

	if ( rc ) {
		checkPtMark = changed;
	}
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
		RecentFilesMenuUpdate();
		checkPtMark = changed = 0;

		SetLayoutFullPath(fileName[0]);
		// Only now is the checkpoint obsolete. After a failed save it may
		// be the only copy of the work left, as saving empties the old
		// file first; after Cancel nothing was saved at all.
		CleanupCheckpointFiles();
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
	for (int i = ARCHIVE_READ; i <= ARCHIVE_WRITE; ++i) {
		char* tempDir = GetZipDirectoryName(i);
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
	RecentFilesMenuUpdate();
}



/**
 * Compare two saved layouts, leaving out the first line, which holds the date,
 * and MAPSCALE, which follows from the window size. Reports the first
 * difference.
 *
 * \param path1 IN first layout
 * \param path2 IN second layout
 * \return TRUE if they're the same
 */
static BOOL_T SavedLayoutsMatch( const char * path1, const char * path2 )
{
	static char line1[STR_HUGE_SIZE], line2[STR_HUGE_SIZE];
	FILE * f1 = fopen( path1, "r" );
	FILE * f2 = fopen( path2, "r" );
	BOOL_T match = FALSE;
	if ( f1 && f2 ) {
		long lineNum = 0;
		while ( TRUE ) {
			char * p1 = fgets( line1, sizeof line1, f1 );
			char * p2 = fgets( line2, sizeof line2, f2 );
			lineNum++;
			if ( p1 == NULL || p2 == NULL ) {
				match = ( p1 == p2 );
				if ( !match ) {
					lprintf( "  FAIL: %s and %s: different length at line %ld\n",
					         path1, path2, lineNum );
				}
				break;
			}
			if ( lineNum > 1 && strncmp( line1, "MAPSCALE ", 9 ) != 0
			     && strcmp( line1, line2 ) != 0 ) {
				lprintf( "  FAIL: %s and %s differ at line %ld:\n  %s  %s",
				         path1, path2, lineNum, line1, line2 );
				break;
			}
		}
	} else {
		lprintf( "  FAIL: can't open %s or %s\n", path1, path2 );
	}
	if ( f1 ) {
		fclose( f1 );
	}
	if ( f2 ) {
		fclose( f2 );
	}
	return match;
}

/**
 * Load a layout and return its track count, or -1 if it didn't load.
 */
static int RoundTripLoad( char * path )
{
	int count = 0;
	track_p trk;
	// LoadTracks() would stop at a notice with nobody to answer it
	if ( access( path, R_OK ) != 0 ) {
		lprintf( "  FAIL: %s is missing\n", path );
		return -1;
	}
	if ( !LoadTracks( 1, &path, NULL ) ) {
		lprintf( "  FAIL: can't load %s\n", path );
		return -1;
	}
	TRK_ITERATE( trk ) {
		count++;
	}
	return count;
}

/**
 * Regression test for -T<layout>: load a layout, save it, load the saved copy
 * and save again. The two saves have to match, and every load has to give the
 * same number of tracks. The same goes for the archive format (.xtce), saved
 * both elsewhere and in the working directory, where CreateArchive() builds
 * it. It's a check that nothing gets lost or changed by saving, whichever
 * version of the program wrote the original.
 *
 * \param pathName IN the layout
 * \return number of failed checks
 */
EXPORT int LayoutRoundTripTest( const char * pathName )
{
	char * dir = NULL;
	char * save1 = NULL, * save2 = NULL, * archive = NULL, * save3 = NULL;
	char * archive2 = NULL;
	char * source = MyStrdup( pathName );
	int nFail = 0;
	int count1, count2 = -1, count3 = -1;

	MakeFullpath( &dir, workingDir, "roundtrip", NULL );
	SafeCreateDir( dir );
	MakeFullpath( &save1, dir, "roundtrip1.xtc", NULL );
	MakeFullpath( &save2, dir, "roundtrip2.xtc", NULL );
	MakeFullpath( &archive, dir, "roundtrip.xtce", NULL );
	MakeFullpath( &save3, dir, "roundtrip3.xtc", NULL );
	MakeFullpath( &archive2, workingDir, "roundtrip.xtce", NULL );

	count1 = RoundTripLoad( source );
	// Which file Save would now write to: none, if the load failed
	char * current = GetLayoutFullPath();
	lprintf( "%s: current layout \"%s\"\n", source, current ? current : "" );
	if ( count1 <= 0 ) {
		lprintf( "  FAIL: %s: %d tracks\n", source, count1 );
		return 1;
	}
	lprintf( "%s: %d tracks\n", source, count1 );
	// The second archive save replaces the first, as saving a layout again does
	if ( !SaveTracks( 1, &save1, NULL ) || !SaveTracks( 1, &archive, NULL )
	     || !SaveTracks( 1, &archive, NULL ) || !SaveTracks( 1, &archive2, NULL )
	     || !SaveTracks( 1, &archive2, NULL ) ) {
		lprintf( "  FAIL: can't save\n" );
		return 1;
	}

	count2 = RoundTripLoad( save1 );
	if ( count2 != count1 ) {
		lprintf( "  FAIL: saved layout has %d tracks\n", count2 );
		nFail++;
	}
	if ( !SaveTracks( 1, &save2, NULL ) || !SavedLayoutsMatch( save1, save2 ) ) {
		nFail++;
	}

	count3 = RoundTripLoad( archive );
	if ( count3 != count1 ) {
		lprintf( "  FAIL: saved archive has %d tracks\n", count3 );
		nFail++;
	}
	if ( !SaveTracks( 1, &save3, NULL ) || !SavedLayoutsMatch( save1, save3 ) ) {
		nFail++;
	}

	if ( RoundTripLoad( archive2 ) != count1 ) {
		lprintf( "  FAIL: archive in the working directory is wrong\n" );
		nFail++;
	}

	free( dir );
	free( archive2 );
	free( save1 );
	free( save2 );
	free( archive );
	free( save3 );
	MyFree( source );
	return nFail;
}

/**
 * Clear the layout as File > New does (DoClearAfter(), misc.c), without the
 * dialogs: what a crash leaves before the restart restores a checkpoint.
 */
static void CheckpointTestClear( void )
{
	Reset();
	ClearTracks();
	ResetLayers();
	checkPtMark = changed = 0;
	SetLayoutFullPath( "" );
}

/**
 * Count the tracks of the layout in memory.
 */
static int CheckpointTestCount( void )
{
	int count = 0;
	track_p trk;
	TRK_ITERATE( trk ) {
		count++;
	}
	return count;
}

/**
 * Regression test for -Tcheckpoint:&lt;layout&gt;: the checkpoint is the only
 * copy of unsaved work after a crash. Load a layout and save it as the
 * reference, then check that:
 * - TryCheckPoint() writes a checkpoint once checkPtInterval changes have
 *   been made, and not before, and that it matches the reference;
 * - the next checkpoint keeps the previous one as a backup;
 * - LoadCheckpoint(), as the restart after a crash calls it, restores the
 *   layout: under its old name ("Resume") or under none ("Resume with New
 *   Name"), counting as changed either way;
 * - a save that fails, or a Save or Open dialog that is cancelled, leaves
 *   the checkpoint in place, and a save that succeeds removes it.
 *
 * \param pathName IN the layout
 * \return number of failed checks
 */
EXPORT int LayoutCheckpointTest( const char * pathName )
{
	char * source = MyStrdup( pathName );
	char * dir = NULL, * reference = NULL, * restored = NULL;
	char * backup = NULL, * missing = NULL;
	char backupName[STR_LONG_SIZE];
	long savedInterval = checkPtInterval;
	long savedAutosave = autosaveChkPoints;
	int nFail = 0;
	int count = RoundTripLoad( source );

	if ( count <= 0 ) {
		MyFree( source );
		return 1;
	}
	lprintf( "%s: %d tracks\n", source, count );
	MakeFullpath( &dir, workingDir, "checkpoint", NULL );
	SafeCreateDir( dir );
	MakeFullpath( &reference, dir, "reference.xtc", NULL );
	MakeFullpath( &restored, dir, "restored.xtc", NULL );
	MakeFullpath( &missing, dir, "missing", "reference.xtc", NULL );
	if ( !SaveTracks( 1, &reference, NULL ) ) {
		lprintf( "  FAIL: can't save %s\n", reference );
		nFail++;
		goto done;
	}
	SaveState();            // "lastlayout", which Resume reopens
	CleanupCheckpointFiles();
	if ( ExistsCheckpoint() ) {
		lprintf( "  FAIL: a checkpoint is left from before the test\n" );
		nFail++;
	}

	checkPtInterval = 10;
	autosaveChkPoints = 0;
	checkPtMark = 0;
	changed = 9;
	TryCheckPoint();
	if ( ExistsCheckpoint() ) {
		lprintf( "  FAIL: checkpoint written after %d of %ld changes\n",
		         changed, checkPtInterval );
		nFail++;
	}
	changed = 10;
	TryCheckPoint();
	if ( !ExistsCheckpoint() ) {
		lprintf( "  FAIL: no checkpoint after %ld changes\n", checkPtInterval );
		nFail++;
		goto done;
	}
	if ( !SavedLayoutsMatch( reference, checkPtFileName1 ) ) {
		lprintf( "  FAIL: the checkpoint isn't the layout\n" );
		nFail++;
	}

	// The second checkpoint keeps the first as <layout>01.bkp
	changed = 20;
	TryCheckPoint();
	sprintf( backupName, "%s01.bkp", GetLayoutFilename() );
	MakeFullpath( &backup, workingDir, backupName, NULL );
	if ( access( backup, F_OK ) != 0 ) {
		lprintf( "  FAIL: no backup %s of the previous checkpoint\n", backup );
		nFail++;
	} else if ( !SavedLayoutsMatch( reference, backup ) ) {
		lprintf( "  FAIL: the backup isn't the previous checkpoint\n" );
		nFail++;
	}

	// The program crashed; the restart offers the checkpoint. Resume with
	// New Name first: the checkpoint has to be there for the steps after it.
	CheckpointTestClear();
	LoadCheckpoint( FALSE );
	if ( CheckpointTestCount() != count ) {
		lprintf( "  FAIL: Resume with New Name restored %d of %d tracks\n",
		         CheckpointTestCount(), count );
		nFail++;
	}
	if ( GetLayoutFullPath()[0] != '\0' ) {
		lprintf( "  FAIL: Resume with New Name kept the name \"%s\"\n",
		         GetLayoutFullPath() );
		nFail++;
	}

	// ... or Resume
	CheckpointTestClear();
	LoadCheckpoint( TRUE );
	if ( CheckpointTestCount() != count ) {
		lprintf( "  FAIL: Resume restored %d of %d tracks\n",
		         CheckpointTestCount(), count );
		nFail++;
	}
	if ( strcmp( GetLayoutFullPath(), reference ) != 0 ) {
		lprintf( "  FAIL: Resume named the layout \"%s\", not %s\n",
		         GetLayoutFullPath(), reference );
		nFail++;
	}
	if ( changed <= 0 ) {
		lprintf( "  FAIL: the restored layout doesn't count as changed\n" );
		nFail++;
	}

	// A save that fails keeps the checkpoint: by then the old file may
	// already be emptied, and the checkpoint is the only copy left
	SetLayoutFullPath( missing );
	bReadOnly = FALSE;
	DoSave( NULL );
	if ( !ExistsCheckpoint() ) {
		lprintf( "  FAIL: a failed save removed the checkpoint\n" );
		nFail++;
	}

	// So does Cancel in the Save dialog of a layout with no name
	SetLayoutFullPath( "" );
	wFilSelSetPlayback( "" );
	DoSave( NULL );
	if ( !ExistsCheckpoint() ) {
		lprintf( "  FAIL: cancelling Save removed the checkpoint\n" );
		nFail++;
	}

	// ... and Cancel in the Open dialog
	wFilSelSetPlayback( "" );
	DoLoad();
	if ( !ExistsCheckpoint() ) {
		lprintf( "  FAIL: cancelling Open removed the checkpoint\n" );
		nFail++;
	}

	// A save that works makes the checkpoint obsolete, and has to hold
	// what the checkpoint restored
	SetLayoutFullPath( restored );
	bReadOnly = FALSE;
	DoSave( NULL );
	if ( ExistsCheckpoint() ) {
		lprintf( "  FAIL: the checkpoint is still there after a save\n" );
		nFail++;
	}
	if ( !SavedLayoutsMatch( reference, restored ) ) {
		lprintf( "  FAIL: the restored layout isn't the layout\n" );
		nFail++;
	}

done:
	checkPtInterval = savedInterval;
	autosaveChkPoints = savedAutosave;
	free( dir );
	free( reference );
	free( restored );
	free( backup );
	free( missing );
	MyFree( source );
	return nFail;
}

/**
 * Save the layout in memory to path and compare it with reference.
 */
static BOOL_T ClipboardTestMatches( char * path, const char * reference,
                                    const char * step )
{
	if ( !SaveTracks( 1, &path, NULL ) || !SavedLayoutsMatch( reference, path ) ) {
		lprintf( "  FAIL: after %s the layout isn't the original\n", step );
		return FALSE;
	}
	return TRUE;
}

/**
 * Select every object but the cars, which the clipboard leaves out.
 */
static void ClipboardTestSelectAll( void )
{
	track_p trk;
	SetAllTrackSelect( TRUE );
	TRK_ITERATE( trk ) {
		if ( QueryTrack( trk, Q_ISTRAIN ) ) {
			ClrTrkBits( trk, TB_SELECTED );
		}
	}
	SelectRecount();
}

/**
 * Count the endpoints of the tracks from first on that are connected to
 * another track, and those connected to a track before first.
 */
static void ClipboardTestConnections( track_p first, int * connected,
                                      int * outside )
{
	*connected = *outside = 0;
	for ( track_p trk = first; trk; trk = GetNextTrack( trk ) ) {
		if ( IsTrackDeleted( trk ) ) {
			continue;
		}
		for ( EPINX_T ep = 0; ep < GetTrkEndPtCnt( trk ); ep++ ) {
			track_p other = GetTrkEndTrk( trk, ep );
			if ( other == NULL ) {
				continue;
			}
			(*connected)++;
			if ( GetTrkIndex( other ) < GetTrkIndex( first ) ) {
				(*outside)++;
			}
		}
	}
}

/**
 * Regression test for -Tclipboard:&lt;layout&gt;: Copy, Cut, Paste and Clone
 * on the whole layout, with Undo and Redo. Load the layout, save it as the
 * reference, select everything, then check that:
 * - Copy and Paste add a copy of every object, selected, and Undo takes it
 *   away again;
 * - Cut leaves nothing, Undo brings the layout back and Redo cuts it again;
 * - Clone puts a copy of every object at one offset from it, connected
 *   among themselves as the originals are and to none of the originals.
 *
 * \param pathName IN the layout
 * \return number of failed checks
 */
EXPORT int LayoutClipboardTest( const char * pathName )
{
	char * source = MyStrdup( pathName );
	char * dir = NULL, * reference = NULL, * work = NULL;
	int nFail = 0;
	int count = RoundTripLoad( source );
	long selected;

	if ( count <= 0 ) {
		MyFree( source );
		return 1;
	}
	lprintf( "%s: %d tracks\n", source, count );
	MakeFullpath( &dir, workingDir, "clipboard", NULL );
	SafeCreateDir( dir );
	MakeFullpath( &reference, dir, "reference.xtc", NULL );
	MakeFullpath( &work, dir, "work.xtc", NULL );
	if ( !SaveTracks( 1, &reference, NULL ) ) {
		lprintf( "  FAIL: can't save %s\n", reference );
		nFail++;
		goto done;
	}

	// Copy and Paste. The clipboard leaves out cars (ExportTracks()).
	SetAllTrackSelect( TRUE );
	selected = 0;
	track_p car;
	TRK_ITERATE( car ) {
		if ( GetTrkSelected( car ) && !QueryTrack( car, Q_ISTRAIN ) ) {
			selected++;
		}
	}
	if ( selected <= 0 ) {
		lprintf( "  FAIL: Select All selected nothing\n" );
		nFail++;
		goto done;
	}
	EditCopy( NULL );
	if ( !editStatus || access( clipBoardN, R_OK ) != 0 ) {
		lprintf( "  FAIL: Copy wrote no clipboard file\n" );
		nFail++;
		goto done;
	}
	EditPaste( NULL );
	if ( !editStatus || CheckpointTestCount() != count + selected ) {
		lprintf( "  FAIL: Paste of %ld objects gave %d, not %ld\n", selected,
		         CheckpointTestCount(), count + selected );
		nFail++;
	}
	if ( selectedTrackCount != selected ) {
		lprintf( "  FAIL: %ld of the %ld pasted objects are selected\n",
		         selectedTrackCount, selected );
		nFail++;
	}
	UndoUndo( NULL );
	if ( CheckpointTestCount() != count ) {
		lprintf( "  FAIL: Undo of Paste left %d objects, not %d\n",
		         CheckpointTestCount(), count );
		nFail++;
	}
	nFail += !ClipboardTestMatches( work, reference, "Paste and Undo" );

	// Cut, Undo, Redo. Delete, so Cut, only works in Select mode. Cars are
	// left out: Cut takes them off the layout, but Copy doesn't copy them.
	DoCommandB( I2VP( selectCmdInx ) );
	ClipboardTestSelectAll();
	EditCut( NULL );
	if ( !editStatus || CheckpointTestCount() != count - selected ) {
		lprintf( "  FAIL: Cut of %ld objects left %d, not %ld\n", selected,
		         CheckpointTestCount(), count - selected );
		nFail++;
	}
	UndoUndo( NULL );
	if ( CheckpointTestCount() != count ) {
		lprintf( "  FAIL: Undo of Cut gave %d objects, not %d\n",
		         CheckpointTestCount(), count );
		nFail++;
	}
	nFail += !ClipboardTestMatches( work, reference, "Cut and Undo" );
	UndoRedo( NULL );
	if ( CheckpointTestCount() != count - selected ) {
		lprintf( "  FAIL: Redo of Cut left %d objects, not %ld\n",
		         CheckpointTestCount(), count - selected );
		nFail++;
	}
	UndoUndo( NULL );

	// Clone: a copy of everything at one offset, connected as the original
	track_p last = NULL, trk;
	int connectedBefore, outsideBefore, connectedClone, outsideClone;
	TRK_ITERATE( trk ) {
		last = trk;
	}
	ClipboardTestConnections( GetFirstTrack(), &connectedBefore,
	                          &outsideBefore );
	ClipboardTestSelectAll();
	EditClone( NULL );

	if ( !editStatus || CheckpointTestCount() != count + selected ) {
		lprintf( "  FAIL: Clone of %ld objects gave %d, not %ld\n", selected,
		         CheckpointTestCount(), count + selected );
		nFail++;
	} else if ( last != NULL && GetNextTrack( last ) != NULL ) {
		track_p orig = GetFirstTrack();
		track_p clone = GetNextTrack( last );
		coOrd offset = zero;
		BOOL_T first = TRUE;
		int mismatch = 0;
		ClipboardTestConnections( clone, &connectedClone, &outsideClone );
		if ( connectedClone != connectedBefore || outsideClone != 0 ) {
			lprintf( "  FAIL: the clone has %d connected endpoints, %d of them to"
			         " the original; the original has %d\n", connectedClone,
			         outsideClone, connectedBefore );
			nFail++;
		}
		for ( ; orig && clone && orig != GetNextTrack( last );
		      orig = GetNextTrack( orig ), clone = GetNextTrack( clone ) ) {
			coOrd hi0, lo0, hi1, lo1;
			GetBoundingBox( orig, &hi0, &lo0 );
			GetBoundingBox( clone, &hi1, &lo1 );
			// A block or switch motor has no position of its own, only
			// its tracks or turnout. A switch motor's box is computed from
			// its turnout when it's read and doesn't follow when the
			// turnout moves (MoveSwitchMotor() is empty): a question for
			// the dev ML, not checked here.
			const char * type = GetTrkTypeName( orig );
			if ( strcmp( type, "BLOCK" ) == 0
			     || strcmp( type, "SWITCHMOTOR" ) == 0 ) {
				continue;
			}
			if ( first ) {
				offset.x = lo1.x - lo0.x;
				offset.y = lo1.y - lo0.y;
				first = FALSE;
			}
			if ( fabs( lo1.x - lo0.x - offset.x ) > 0.001
			     || fabs( lo1.y - lo0.y - offset.y ) > 0.001
			     || fabs( hi1.x - hi0.x - offset.x ) > 0.001
			     || fabs( hi1.y - hi0.y - offset.y ) > 0.001 ) {
				if ( mismatch++ == 0 ) {
					lprintf( "  FAIL: clone T%d [%0.3f %0.3f]-[%0.3f %0.3f] isn't at"
					         " the offset [%0.3f %0.3f] from T%d [%0.3f %0.3f]-[%0.3f %0.3f]\n",
					         GetTrkIndex( clone ), lo1.x, lo1.y, hi1.x, hi1.y,
					         offset.x, offset.y, GetTrkIndex( orig ),
					         lo0.x, lo0.y, hi0.x, hi0.y );
				}
			}
		}
		if ( mismatch ) {
			lprintf( "  FAIL: %d clones aren't at the offset\n", mismatch );
			nFail++;
		}
	}
	UndoUndo( NULL );
	nFail += !ClipboardTestMatches( work, reference, "Clone and Undo" );

done:
	free( dir );
	free( reference );
	free( work );
	MyFree( source );
	return nFail;
}

/**
 * Export the selection to path through File > Export, as if the user had
 * picked path in the file dialog.
 */
static void XtiTestExport( const char * path )
{
	wFilSelSetPlayback( path );
	DoExport( NULL );
}

/**
 * Import path through File > Import or Import Module.
 */
static void XtiTestImport( const char * path, BOOL_T module )
{
	wFilSelSetPlayback( path );
	if ( module ) {
		DoImportModule( NULL );
	} else {
		DoImportObjects( NULL );
	}
}

/**
 * Regression test for -Txti:&lt;layout&gt;: File > Export and Import of the
 * .xti format. Load the layout, save it as the reference, select every object
 * but the cars and export them, then check that:
 * - imported into an empty layout, every object comes back, selected, and
 *   exporting them again gives the same file: an export holds positions
 *   relative to the selection, so that is the same objects at the same
 *   places relative to each other;
 * - imported into the layout itself, the copies get new indexes, connect
 *   among themselves as the originals do and to none of them, and Undo
 *   gives back the layout as saved;
 * - imported as a module, they go on one new layer, named after the file
 *   and marked as a module.
 *
 * \param pathName IN the layout
 * \return number of failed checks
 */
EXPORT int LayoutXtiTest( const char * pathName )
{
	char * source = MyStrdup( pathName );
	char * dir = NULL, * reference = NULL, * work = NULL;
	char * export1 = NULL, * export2 = NULL;
	int nFail = 0;
	int count = RoundTripLoad( source );
	long selected = 0;
	track_p trk, last = NULL;

	if ( count <= 0 ) {
		MyFree( source );
		return 1;
	}
	lprintf( "%s: %d tracks\n", source, count );
	MakeFullpath( &dir, workingDir, "xti", NULL );
	SafeCreateDir( dir );
	MakeFullpath( &reference, dir, "reference.xtc", NULL );
	MakeFullpath( &work, dir, "work.xtc", NULL );
	MakeFullpath( &export1, dir, "export1.xti", NULL );
	MakeFullpath( &export2, dir, "export2.xti", NULL );
	if ( !SaveTracks( 1, &reference, NULL ) ) {
		lprintf( "  FAIL: can't save %s\n", reference );
		nFail++;
		goto done;
	}

	ClipboardTestSelectAll();
	selected = selectedTrackCount;
	XtiTestExport( export1 );
	if ( access( export1, R_OK ) != 0 ) {
		lprintf( "  FAIL: Export wrote no file\n" );
		nFail++;
		goto done;
	}

	// Into an empty layout, and out again
	CheckpointTestClear();
	XtiTestImport( export1, FALSE );
	if ( CheckpointTestCount() != selected || selectedTrackCount != selected ) {
		lprintf( "  FAIL: Import into an empty layout gave %d objects, %ld"
		         " selected, not %ld\n", CheckpointTestCount(), selectedTrackCount,
		         selected );
		nFail++;
	}
	XtiTestExport( export2 );
	if ( !SavedLayoutsMatch( export1, export2 ) ) {
		lprintf( "  FAIL: exporting the imported objects gave another file\n" );
		nFail++;
	}

	// Into the layout itself
	if ( RoundTripLoad( reference ) != count ) {
		lprintf( "  FAIL: can't reload %s\n", reference );
		nFail++;
		goto done;
	}
	TRK_ITERATE( trk ) {
		last = trk;
	}
	int connectedBefore, outsideBefore, connectedImport, outsideImport;
	ClipboardTestConnections( GetFirstTrack(), &connectedBefore, &outsideBefore );
	XtiTestImport( export1, FALSE );
	if ( CheckpointTestCount() != count + selected
	     || selectedTrackCount != selected ) {
		lprintf( "  FAIL: Import into the layout gave %d objects, %ld selected;"
		         " expected %ld, %ld\n", CheckpointTestCount(), selectedTrackCount,
		         count + selected, selected );
		nFail++;
	} else if ( last != NULL && GetNextTrack( last ) != NULL ) {
		ClipboardTestConnections( GetNextTrack( last ), &connectedImport,
		                          &outsideImport );
		// Cars aren't exported: their couplings to each other aren't either
		int connectedCars = 0;
		TRK_ITERATE( trk ) {
			if ( trk == GetNextTrack( last ) ) {
				break;
			}
			if ( QueryTrack( trk, Q_ISTRAIN ) ) {
				for ( EPINX_T ep = 0; ep < GetTrkEndPtCnt( trk ); ep++ ) {
					if ( GetTrkEndTrk( trk, ep ) ) {
						connectedCars++;
					}
				}
			}
		}
		if ( connectedImport != connectedBefore - connectedCars
		     || outsideImport != 0 ) {
			lprintf( "  FAIL: the import has %d connected endpoints, %d of them"
			         " to the layout; the layout has %d without cars\n",
			         connectedImport, outsideImport,
			         connectedBefore - connectedCars );
			nFail++;
		}
	}
	UndoUndo( NULL );
	if ( CheckpointTestCount() != count ) {
		lprintf( "  FAIL: Undo of Import left %d objects, not %d\n",
		         CheckpointTestCount(), count );
		nFail++;
	}
	nFail += !ClipboardTestMatches( work, reference, "Import and Undo" );

	// As a module
	if ( RoundTripLoad( reference ) != count ) {
		lprintf( "  FAIL: can't reload %s\n", reference );
		nFail++;
		goto done;
	}
	TRK_ITERATE( trk ) {
		last = trk;
	}
	XtiTestImport( export1, TRUE );
	if ( CheckpointTestCount() != count + selected ) {
		lprintf( "  FAIL: Import Module gave %d objects, not %ld\n",
		         CheckpointTestCount(), count + selected );
		nFail++;
	} else if ( last != NULL && GetNextTrack( last ) != NULL ) {
		unsigned int layer = GetTrkLayer( GetNextTrack( last ) );
		char expected[STR_SIZE];
		int elsewhere = 0;
		sprintf( expected, _( "Module - %s" ), FindFilename( export1 ) );
		for ( trk = GetNextTrack( last ); trk; trk = GetNextTrack( trk ) ) {
			if ( !IsTrackDeleted( trk ) && GetTrkLayer( trk ) != layer ) {
				elsewhere++;
			}
		}
		if ( elsewhere ) {
			lprintf( "  FAIL: %d imported objects aren't on the module's layer\n",
			         elsewhere );
			nFail++;
		}
		if ( strcmp( GetLayerName( layer ), expected ) != 0
		     || !GetLayerModule( layer ) ) {
			lprintf( "  FAIL: the module's layer %u is \"%s\"%s, not a module"
			         " layer \"%s\"\n", layer + 1, GetLayerName( layer ),
			         GetLayerModule( layer ) ? " (module)" : "", expected );
			nFail++;
		}
	}

done:
	free( dir );
	free( reference );
	free( work );
	free( export1 );
	free( export2 );
	MyFree( source );
	return nFail;
}

/**
 * Regression test for -Tdescribe:&lt;layout&gt;: load a layout, then open
 * Properties on every object and press Done without changing anything
 * (DescribeAllTracks()). No dialog may start an Undo step, and every object
 * has to match what it was before, as the demo regression check compares
 * them (CheckRegressionResult()). Drawn objects are compared by where they
 * are, not by how they're stored: Properties stores a drawn object's
 * rotation in its points, which doesn't move it (CompareDraw()).
 *
 * \param pathName IN the layout
 * \return number of failed checks
 */
EXPORT int LayoutDescribeTest( const char * pathName )
{
	char * source = MyStrdup( pathName );
	char * expected = NULL;
	int nFail = 0;
	int count = RoundTripLoad( source );

	if ( count <= 0 ) {
		MyFree( source );
		return 1;
	}
	lprintf( "%s: %d tracks\n", source, count );
	// Start from a saved and reloaded copy, so the layout in memory holds
	// only what a layout file can: a turntable, for one, keeps endpoints
	// without a track that it doesn't write.
	char * saved = NULL;
	MakeFullpath( &saved, workingDir, "describe.xtc", NULL );
	if ( !SaveTracks( 1, &saved, NULL ) || RoundTripLoad( saved ) <= 0 ) {
		lprintf( "  FAIL: can't save and reload %s\n", saved );
		free( saved );
		MyFree( source );
		return 1;
	}
	free( saved );
	MakeFullpath( &expected, workingDir, "describe.expected", NULL );
	FILE * f = FOpenWriteRestricted( expected );
	if ( f == NULL ) {
		lprintf( "  FAIL: can't write %s\n", expected );
		nFail++;
	} else {
		// Written and read back in the C locale, as a layout file is: with a
		// decimal-comma locale (de_DE) the numbers came out as 2,42 and were
		// read back into the wrong fields (dev ML #4641)
		SetCLocale();
		WriteTracks( f, FALSE );
		fprintf( f, "REGRESSION END\n" );
		fclose( f );
		SetUserLocale();

		int nUndo = DescribeAllTracks();
		if ( nUndo > 0 ) {
			lprintf( "  FAIL: %d dialogs started an Undo step\n", nUndo );
			nFail++;
		}
		// Reading the expected objects mustn't record them for Undo, as
		// playback, where the demo check runs, doesn't either
		UndoSuspend();
		paramFile = fopen( expected, "r" );
		paramLineNum = 0;
		SetCLocale();
		int nDiff = CheckRegressionResult( PARAMVERSION, expected, TRUE );
		SetUserLocale();
		if ( paramFile ) {
			fclose( paramFile );
			paramFile = NULL;
		}
		UndoResume();
		if ( nDiff != 0 ) {
			lprintf( "  FAIL: %d objects changed\n", nDiff );
			nFail++;
		}
	}
	free( expected );
	MyFree( source );
	return nFail;
}

/**
 * Regression test for -Texport:&lt;layout&gt;: load a layout, then print it to
 * export.pdf and export it to export.svg, export.dxf and export.png, in the
 * working directory, through the normal code, and import the DXF file back.
 * tools/check-export-test.sh checks the files. A bitmap export to export.bmp,
 * a name the bitmap export can't write, has to fail without writing a file;
 * RegressionTests.cmake checks which notice it showed.
 *
 * \param pathName IN the layout
 * \return number of failed checks
 */
EXPORT int LayoutExportTest( const char * pathName )
{
	char * source = MyStrdup( pathName );
	char * pdf = NULL, * svg = NULL, * dxf = NULL, * png = NULL;
	char * bmp = NULL;
	int nFail = 0;
	int count = RoundTripLoad( source );

	if ( count <= 0 ) {
		MyFree( source );
		return 1;
	}
	MakeFullpath( &pdf, workingDir, "export.pdf", NULL );
	MakeFullpath( &svg, workingDir, "export.svg", NULL );
	MakeFullpath( &dxf, workingDir, "export.dxf", NULL );
	MakeFullpath( &png, workingDir, "export.png", NULL );
	MakeFullpath( &bmp, workingDir, "export.bmp", NULL );

	PrintAllPagesToFile( pdf );
	if ( !ExportBitmapToFile( png ) ) {
		lprintf( "  FAIL: bitmap export\n" );
		nFail++;
	}
	remove( bmp );
	if ( ExportBitmapToFile( bmp ) ) {
		lprintf( "  FAIL: bitmap export to a .bmp file reported success\n" );
		nFail++;
	}
	if ( access( bmp, F_OK ) == 0 ) {
		lprintf( "  FAIL: bitmap export wrote a .bmp file\n" );
		nFail++;
	}
#if XTRKCAD_CREATE_SVG
	if ( !ExportSVGToFile( svg ) ) {
		lprintf( "  FAIL: SVG export\n" );
		nFail++;
	}
#endif
	SetAllTrackSelect( TRUE );
	if ( !ExportDxfToFile( dxf ) ) {
		lprintf( "  FAIL: DXF export\n" );
		nFail++;
	}
	SetAllTrackSelect( FALSE );

	track_p trk;
	int countImported = 0;
	if ( !ImportDxfFromFile( dxf ) ) {
		lprintf( "  FAIL: DXF import\n" );
		nFail++;
	}
	TRK_ITERATE( trk ) {
		countImported++;
	}
	if ( countImported <= count ) {
		lprintf( "  FAIL: DXF import added nothing: %d tracks before, %d after\n",
		         count, countImported );
		nFail++;
	}

	free( pdf );
	free( svg );
	free( dxf );
	free( png );
	free( bmp );
	MyFree( source );
	return nFail;
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
