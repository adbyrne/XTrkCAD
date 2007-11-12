
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
//#ifdef WINDOWS
#include <stdlib.h>
//#endif

#include <stdio.h>
#include <string.h>

#define I18NHEADERFILE "i18n.h"

typedef enum { MSWPOPUP, XVINFO, BALLOONHELP, HPJALIAS, ALIASREF, ALIASDEF, DEFINE, BALLOONHELPI18N } mode_e;

void remap_minus( char * cp )
{
    for ( ; *cp; cp++ )
	if ( *cp == '-' )
	    *cp = '_';
}

void process( mode_e mode, FILE * inFile, FILE * outFile )
{
	char line[256];
	char * cp;
	char * contents;
	char * alias;
	char * extHelp;
	int lineNum;
	int len;

	switch ( mode ) {
	case MSWPOPUP:
		break;
	case XVINFO:
		break;
	case BALLOONHELP:
	case BALLOONHELPI18N:
		fputs( "/*\n * DO NOT EDIT! This file has been automatically created by genhelp.\n * Changes to this file will be overwritten.\n */\n", outFile );
		fprintf( outFile, "#include <stdio.h>\n" );
		fprintf( outFile, "#include \"wlib.h\"\n" );
		if( mode == BALLOONHELPI18N )
			fprintf( outFile, "#include \"" I18NHEADERFILE "\"\n" );
			
		fprintf( outFile, "wBalloonHelp_t balloonHelp[] = {\n\n" );
		break;
	case HPJALIAS:
		fprintf( outFile, "[ALIAS]\r\n" );
		break;
	case ALIASREF:
		break;
	case ALIASDEF:
		break;
	case DEFINE:
		break;
	}

	lineNum = 0;
	while ( fgets( line, sizeof line, inFile ) != NULL ) {
		lineNum++;
		if (line[0] == '#')
			continue;
		len = (int)strlen( line );
		if (line[len-1] == '\n' ) len--;
		if (line[len-1] == '\r' ) len--;
		line[len] = '\0';
		if (len == 0)
			continue;
		contents = strchr( line, '\t' );
		if (contents == NULL) {
			fprintf( stderr, "Not tab on line %d\n%s\n", lineNum, line );
			continue;
		}
		*contents++ = '\0';
		alias = strchr( contents, '\t' );
		if (alias != NULL) {
			*alias++ = '\0';
			extHelp = strchr( alias, '\t' );
			if (extHelp != NULL) {
				*extHelp++ = '\0';
			}
		}
		switch ( mode ) {
		case MSWPOPUP:
			remap_minus( line );
			remap_minus( contents );
			fprintf( outFile, "\\page #{\\footnote _%s}\r\n", line );
			for ( cp=contents; *cp; cp++ ) {
				if ( (*cp) & 0x80 ) {
					fprintf( outFile, "\\'%2.2X", (unsigned char)*cp );
				} else {
					fprintf( outFile, "%c", *cp );
				}
			}
			fprintf( outFile, "\r\n" );
			break;
		case XVINFO:
			if ( *contents )
			    fprintf( outFile, ":%s\n%s\n", line, contents );
			break;
		case BALLOONHELP:
		case BALLOONHELPI18N:
			if ( *contents )
				if( mode == BALLOONHELP )
			   	fprintf( outFile, "\t{ \"%s\", \"%s\" },\n", line, contents );
				else
			   	fprintf( outFile, "\t{ \"%s\", N_(\"%s\") },\n", line, contents );					
			else
			    fprintf( outFile, "\t{ \"%s\" },\n", line );
			break;
		case HPJALIAS:
			if (alias && *alias) {
				remap_minus( line );
				remap_minus( alias );
				fprintf( outFile, "%s=%s\r\n", line, alias );
			}
			break;
		case ALIASREF:
			if (alias && *alias)
				fprintf( outFile, "%s\n", alias );
			break;
		case ALIASDEF:
			if (alias && *alias)
				fprintf( outFile, "%s\n", line );
			break;
		case DEFINE:
			fprintf( outFile, "%s\n", line );
			break;
		}
	}

	switch ( mode ) {
	case MSWPOPUP:
		break;
	case XVINFO:
		fprintf( outFile, ":\n" );
		break;
	case BALLOONHELP:
	case BALLOONHELPI18N:
		fprintf( outFile, "\n	{ NULL, NULL } };\n" );
		break;
	case HPJALIAS:
		break;
	case ALIASREF:
		break;
	case ALIASDEF:
		break;
	case DEFINE:
		break;
	}

}


int main ( int argc, char * argv[] )
{
	FILE * inFile, * outFile;
	mode_e mode;
	if ( argc != 4 ) {
		fprintf( stderr, "Usage: %s (-msw|-xv|-bh|-hpj|-ref) INFILE OUTFILE\n", argv[0] );
		exit(1);
	}
	if ( strcmp( argv[1], "-msw" ) == 0 )
		mode = MSWPOPUP;
	else if ( strcmp( argv[1], "-xv" ) == 0 )
		mode = XVINFO;
	else if ( strcmp( argv[1], "-bh" ) == 0 )
		mode = BALLOONHELP;
	else if ( strcmp( argv[1], "-bhi" ) == 0 )
		mode = BALLOONHELPI18N;
	else if ( strcmp( argv[1], "-hpj" ) == 0 )
		mode = HPJALIAS;
	else if ( strcmp( argv[1], "-aliasref" ) == 0 )
		mode = ALIASREF;
	else if ( strcmp( argv[1], "-aliasdef" ) == 0 )
		mode = ALIASDEF;
	else if ( strcmp( argv[1], "-define" ) == 0 )
		mode = DEFINE;
	else {
		fprintf( stderr, "Bad mode: %s\n", argv[1] );
		exit(1);
	}

	inFile = fopen( argv[2], "r" );
	if (inFile == NULL) {
		perror( argv[2] );
		exit(1);
	}
	outFile = fopen( argv[3], "w" );
	if (outFile == NULL) {
		perror( argv[3] );
		exit(1);
	}

	process( mode, inFile, outFile );
	exit(0);
}
