/** \file filelines.c
 * Line-based reading of the current input file: GetNextLine(), IsEND() and
 * ReadMultilineText().
 *
 * Extracted out of fileio.c so they can be unit tested (see
 * unittest/filelinestest.c) without fileio.c's dependency graph.
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

#include "common.h"
#include "dynstring.h"
#include "fileio.h"
#include "misc.h"
#include "include/paramfile.h"
#include "include/stringxtc.h"

#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

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
 * until the END statement is found. If the file ends first, GetNextLine()
 * has already told the user and closed the file; the text read so far is
 * returned.
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

	// At EOF GetNextLine() closes paramFile and line holds nothing new
	while ( paramFile && !IsEND("END") ) {
		DynStringCatCStr(&noteText, line);
		DynStringCatCStr(&noteText, "\n");
		line = GetNextLine();
	}
	string = MyStrdup(DynStringToCStr(&noteText));
	if (string[0]) {
		string[strlen(string) - 1] = '\0';
	}

#ifdef UTFCONVERT
	if (wIsUTF8(string)) {
		ConvertUTF8ToSystem(string);
	}
#endif // UTFCONVERT

	DynStringFree(&noteText);
	return (string);
}
