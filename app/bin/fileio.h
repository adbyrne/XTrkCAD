/** \file fileio.h
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

#ifndef FILEIO_H
#define FILEIO_H

#include "common.h"

extern FILE * paramFile;
extern char *paramFileName;
extern wIndex_t paramLineNum;
extern char paramLine[STR_HUGE_SIZE];
extern char * curContents;
extern char * curSubContents;
#define PARAM_DEMO (-1)

typedef void (*playbackProc_p)( char * );
typedef BOOL_T (*readParam_t) ( char * );
typedef BOOL_T (*deleteParam_t) (void *param);

extern const char * workingDir;
extern const char * libDir;

extern wBool_t bReadOnly;
extern wBool_t bExample;

// Processing an input file, objects may be incomplete so avoid some ops (MapRedraw)
extern wBool_t bInReadTracks;

#define PARAM_CUSTOM	(-2)
#define PARAM_LAYOUT	(-3)
extern int curParamFileIndex;

extern unsigned long playbackTimer;

extern wBool_t executableOk;

extern FILE * recordF;
extern wBool_t inPlayback;
extern wBool_t inPlaybackQuit;
extern wWin_p demoW;
extern int curDemo;

#define ZIPFILETYPEEXTENSION "xtce"

#define PARAM_SUBDIR "params"

#define LAYOUTPATHKEY "layout"
#define BITMAPPATHKEY "bitmap"
#define BACKGROUNDPATHKEY "images"
#define DXFPATHKEY "dxf"
#define SVGPATHKEY "svg"
#define PARTLISTPATHKEY "parts"
#define CARSPATHKEY "cars"
#define PARAMETERPATHKEY "params"
#define IMPORTPATHKEY "import"
#define MACROPATHKEY "macro"
#define CUSTOMPATHKEY "custom"
#define ARCHIVEPATHKEY "archive"

typedef struct {
	char * name;
	readParam_t proc;
} paramProc_t;
extern dynArr_t paramProc_da;
#define paramProc(N) DYNARR_N( paramProc_t, paramProc_da, N )

void Stripcr( char * );
char * GetNextLine( void );

#define END_TRK_FILE	"END$TRACKS"
#define END_BLOCK	"END$BLOCK"
#define END_SIGNAL	"END$SIGNAL"
#define END_SEGS	"END$SEGS"
#define END_MESSAGE	"END$MESSAGE"
wBool_t IsEND( char * sEnd );

BOOL_T GetArgs( char *, const char *, ... );
char * ReadMultilineText();
BOOL_T ParseRoomSize( char *, coOrd * );
int InputError( char *, BOOL_T, ... );
void SyntaxError( char *, wIndex_t, wIndex_t );

void AddParam( char *name, readParam_t proc );

FILE * OpenCustom( char * );

void SetWindowTitle( void );
char * PutTitle( char * cp );

void ParamFileListLoad(int paramFileCnt, dynArr_t *paramFiles);
void DoParamFiles(void * unused);

int LoadTracks( int cnt, char **fileName, void *data );

void SaveState( void );
void DoSave( void * doAfterSaveVP );
void DoSaveAs( void * doAfterSaveVP );
void DoLoad( void );
void DoExamples( void );
void DoFileList( int, char *, void * );
void TryCheckPoint( void );
void CleanupCheckpointFiles( void );
void CleanupTempArchive( void );
int ExistsCheckpoint( void );
int LoadCheckpoint( BOOL_T );
void DoImport( void * typeVP );
void DoExport( void * unused );
void DoExportDXF( void * unused );
#if XTRKCAD_CREATE_SVG
void DoExportSVG( void * unused );
#endif
extern wBool_t editStatus; // Status of last Edit* command
void EditCopy( void * unused );
void EditCut( void * unused );
void EditPaste( void * unused );
void EditClone( void * unused );


void DoRecord( void * );
void AddPlaybackProc( char *, playbackProc_p, void * );
EXPORT void TakeSnapshot( drawCmd_p );
void PlaybackMessage( char * );
void DoPlayBack( void * );
int MyGetKeyState( void );

int RegLevel( void );
void ReadKey( void );
void PopupRegister( void * );

void LoadFileList( void );
void FileInit( void );

BOOL_T MacroInit( void );

int Copyfile( const char * fn1, const char * fn2);
void SetCLocale();
void SetUserLocale();

// Parameter file search
void DoSearchParams(void * unused);

#endif
