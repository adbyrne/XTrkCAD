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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef FILEIO_H
#define FILEIO_H

#include <stdio.h>

#include "common.h"
#include "misc.h"

FILE * paramFile;
extern char *paramFileName;
wIndex_t paramLineNum;
char paramLine[STR_LONG_SIZE];
char * curContents;
char * curSubContents;
#define PARAM_DEMO (-1)

typedef void (*playbackProc_p)( char * );
typedef BOOL_T (*readParam_t) ( char * );

extern const char * workingDir;
extern const char * libDir;

#define PARAM_CUSTOM	(-2)
#define PARAM_LAYOUT	(-3)
extern int curParamFileIndex;

extern unsigned long playbackTimer;

extern wBool_t executableOk;

extern FILE * recordF;
wBool_t inPlayback;
wBool_t inPlaybackQuit;
wWin_p demoW;
int curDemo;

wMenuList_p fileList_ml;

#define LAYOUTPATHKEY "layout"
#define BITMAPPATHKEY "bitmap"
#define DXFPATHKEY "dxf"
#define PARTLISTPATHKEY "parts"
#define CARSPATHKEY "cars"
#define PARAMETERPATHKEY "params"
#define IMPORTPATHKEY "import"
#define MACROPATHKEY "macro"
#define CUSTOMPATHKEY "custom"

void Stripcr( char * );
char * GetNextLine( void );

BOOL_T GetArgs( char *, char *, ... );
BOOL_T ParseRoomSize( char *, coOrd * );
int InputError( char *, BOOL_T, ... );
void SyntaxError( char *, wIndex_t, wIndex_t );

void AddParam( char *, readParam_t );

FILE * OpenCustom( char * );

#ifdef WINDOWS
#define fopen( FN, MODE ) wFileOpen( FN, MODE )
#endif

void SetWindowTitle( void );
char * PutTitle( char * cp );
wBool_t IsParamValid( int );
char * GetParamFileName( int );
void RememberParamFiles( void );
int LoadParamFile( int files, char **fileName, void *data );
void ReadParamFiles( void );
int LoadTracks( int cnt, char **fileName, void *data );
BOOL_T ReadParams( long, const char *, const char * );

typedef void (*doSaveCallBack_p)( void );
void DoSave( doSaveCallBack_p );
void DoSaveAs( doSaveCallBack_p );
void DoLoad( void );
void DoFileList( int, char *, void * );
void DoCheckPoint( void );
void CleanupFiles( void );
int ExistsCheckpoint( void );
int LoadCheckpoint( void );
void DoImport( void );
void DoExport( void );
void DoExportDXF( void );
BOOL_T EditCopy( void );
BOOL_T EditCut( void );
BOOL_T EditPaste( void );


void DoRecord( void * );
void AddPlaybackProc( char *, playbackProc_p, void * );
EXPORT void TakeSnapshot( drawCmd_t * );
void PlaybackMessage( char * );
void DoPlayBack( void * );
int MyGetKeyState( void );

int RegLevel( void );
void ReadKey( void );
void PopupRegister( void * );

void FileInit( void );
BOOL_T ParamFileInit( void );
BOOL_T MacroInit( void );

char *SaveLocale( char *newLocale );
void RestoreLocale( char * locale );

#endif
