/** \file listxtp.c
 * Create a contents list of all parameter files
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/tools/listxtp.c,v 1.2 2009-05-31 21:55:37 tshead Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2009 Martin Fischer
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "dirent.h"

#define TRUE 1
#define FALSE 0
#define MAX_FILES 500
#ifdef _WIN32
	#define WIKIFORMATOPTION "/w"
	#pragma warning( disable : 4996 )
#else
	#define WIKIFORMATOPTION "-w"
#endif

#define CONTENTSCOMMAND "CONTENTS"

#ifndef WIN32
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif // !WIN32

/*
 * needed for qsort
 */

int
Compare( const void *s1, const void * s2 )
{
	char *str1 = *(char **)s1;
	char *str2 = *(char **)s2;

	return( strcmp( str1, str2 ));
}


int
main( int argc, char **argv )
{
	char buffer[ 512 ];
	int found;
	DIR *d;
	struct dirent *ent;
	FILE *fh;
	struct stat buf;
	char filename[ 256 ];
	char path[ 256 ];
	char *results[MAX_FILES];
	int cnt = 0;
	int i;
	int bWiki = FALSE;

	/*
		this is a fast hack: there is one optional argument 
		if this was found, set the flag and remove it from the array. 
		As there is a maximum of one more argument, one simple assignment should be enough.
	*/

	if( argc >= 2 && !strcmp(argv[ 1 ], WIKIFORMATOPTION )) {
		bWiki = TRUE;
		if( argc > 2 ) {
			argv[ 1 ] = argv[ 2 ];
			argv[ 2 ] = NULL;
		}
		argc--;
	}

	/* 
	 * only other argument is the name of the directory to search through
	 */
	if( argc == 2 ) {
		strcpy( path,  argv[ 1 ] );
	} else {
		if( argc == 1 ) {
			strcpy( path, "." );
		} else {
			printf( "Invalid nummer of arguments. Execute with: listxtp "WIKIFORMATOPTION" [dir]\n" );
		}
	}

	/*
	 * open the directory
	 */
	d = opendir( path );
	if( !d ) {
		printf( "Directory %s not found!\n", path);
		exit( 1 );
	}

	/*
	 * get all files from the directory
	 */
	while( ent = readdir( d ))
	{
		/*
		 * create full file name and get the state for that file
		 */

		strcpy( filename, path );
		strcat( filename, "\\" );
		strcat( filename, ent->d_name );

		if( stat( filename, &buf ) == -1 ) {
			fprintf( stderr, "Error getting file state for %s\n", filename );
			exit( 1 );
		}
		/*
		 * ignore any directories
		 */
		if( buf.st_mode & S_IFDIR )
			continue;

		/*
		 * open the file and search for a line beginning with CONTENTS
		 */ 
		found = FALSE;
		fh = fopen( filename, "rt" );
		if( fh ) {
			while( !found ) {
				if( fgets( buffer, sizeof( buffer ), fh )) {
					if( !strnicmp( buffer, CONTENTSCOMMAND, strlen( CONTENTSCOMMAND ))) {
						/*
						 * if found, store the restof the line and the filename
						 */
						buffer[ strlen( buffer ) - 1 ] = '\0';
						sprintf( buffer, "%s (%s)", buffer + strlen( CONTENTSCOMMAND ) + 1, ent->d_name );
						results[ cnt ] = malloc( strlen( buffer ) + 1 );
						strcpy( results[ cnt ], buffer );
						cnt++;
						if( cnt == MAX_FILES ) {
							fprintf( stderr, "Error: too many files\n" );
							exit( 1 );
						}
						found = TRUE;
					}
				} else {
					fprintf( stderr, "Nothing found in %s\n", filename );
					found = TRUE;
				}
			}
			fclose( fh );
		} else {
			fprintf( stderr, "Error opening %s\n", filename );
		}
	}

	/*
	 * sort the list that was created
	 */
	qsort( (void *)results, (size_t )cnt, sizeof( char *), Compare );

	/*
	 * print the results. If Wiki option was set, format the line for usage
	 * in the Wiki
	 */
	for( i = 0; i < cnt; i++) {
		if( bWiki ) {
			printf("~-\"\"%s\"\"\n", results[ i ] );
			if( (i < cnt - 1) && *results[ i ] != *results[ i + 1 ] )
				printf( "\n" );
		} else {
			printf( "%s\n", results[ i ]);
		}
	}
	return( 0 );
}
