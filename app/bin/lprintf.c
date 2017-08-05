/** \file lprintf.c
 * Logging functions
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef WINDOWS
#include <time.h>
#else
#include <time.h>
#include <sys/timeb.h>
#endif

#include "custom.h"
#include "fileio.h"
#include "messages.h"
#include "paths.h"
#include "track.h"


/****************************************************************************
 *
 * LPRINTF
 *
 */


EXPORT dynArr_t logTable_da;

static FILE * logFile;
static char * logFileName;
EXPORT time_t logClock = 0;
static BOOL_T logInitted = FALSE;
static long logLineNumber;

static void LogInit( void )
{
	int inx;

	if ( logTable_da.cnt != 0 )
		return;
	DYNARR_SET( logTable_t, logTable_da, 11 );
	for ( inx=0; inx<=10; inx++ ) {
		logTable(inx).name = "";
		logTable(inx).level = inx;
	}
}

EXPORT void LogOpen( char * filename )
{
	time( &logClock );
	logFileName = filename;
	LogInit();
}


static void LogDoOpen( void )
{
	if ( logFileName == NULL ) {
#ifdef WINDOWS
		MakeFullpath(&logFileName, wGetAppWorkDir(), "xtclog.txt", NULL);
#else
		logFile = stdout;
#endif
	}

	if ( logFileName ) {
		logFile = fopen( logFileName, "a" );
		if ( logFile == NULL ) {
			NoticeMessage( MSG_OPEN_FAIL, "Continue", NULL, "Log", logFileName, strerror(errno) );
			perror( logFileName );
			return;
		}
	}
	fprintf( logFile, "# %s Version: %s, Date: %s\n", sProdName, sVersion, ctime(&logClock) );
	if ( recordF )
		fprintf( recordF, "# LOG CLOCK %s\n", ctime(&logClock) );
}

EXPORT void LogClose( void )
{
	time_t clock;
	if ( logFile ) {
		time(&clock);
		fprintf( logFile, "LOG END %s\n", ctime(&clock) );
		if ( logFile != stdout )
			fclose( logFile );
	 }
	 logFile = NULL;
}

EXPORT void LogSet( char * name, int level )
{
	LogInit();
	DYNARR_APPEND( logTable_t, logTable_da, 10 );
	logTable(logTable_da.cnt-1).name = MyStrdup( name );
	logTable(logTable_da.cnt-1).level = level;
}

EXPORT int LogFindIndex( char * name )
{
	int inx;
	for ( inx=11; inx<logTable_da.cnt; inx++ )
		if ( strcasecmp( logTable(inx).name, name ) == 0 )
			return inx;
	return 0;
}

EXPORT void LogPrintf(
		char * format,
		... )
{
	va_list ap;
	if (!logInitted) {
		LogDoOpen();
		logInitted = TRUE;
	}
	if ( logFile == NULL )
		return;
	logLineNumber++;
	if ( logLineNumber % 100 == 0 ) {
		if ( recordF ) {
			fprintf( recordF, "# LOG LINE %ld\n", logLineNumber );
			fprintf( logFile, "LOG LINE %ld\n", logLineNumber );
		}
	}
	va_start( ap, format );
	vfprintf( logFile, format, ap );
	va_end( ap );
	fflush( logFile );
}

