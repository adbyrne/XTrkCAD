/** \file mkstruct.c
 * Build utility to create simple rectangular structure definitions from a data file. 
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/params/mkstruct.c,v 1.6 2008-06-10 20:27:21 m_fischer Exp $
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 
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
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 1024

#if _MSC_VER > 1300
	#define stricmp _stricmp
	#define strnicmp _strnicmp
#endif

#ifndef WIN32
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif // !WIN32

int main ( int argc, char * argv [] )
{
	long color = 0xFF00FF;
	double x;
	double y;
	int cm = 0;
	FILE *fIn, *fOut;
	int count = 0;
	char *buffer = malloc( BUFSIZE );
	char *desc;
	char *p1;
	char *p2 = malloc( BUFSIZE );
	char *scale;
	char *ptr;
	int err;

	if( argc != 3 )
	{
		fprintf( stderr, "Usage: mkstruct definitions.data param file\n\n"
						 "The data file is read line by line and structure defimitions\n"
						 "are created in the param file.\n\n"
						 "The file structure is:\n"
						 "scale \"description of structure\" x y [cm] [color=#rrggbb]\n\n"
						 "scale                    : scale of structure\n"
						 "description of structure : name, enclosed in double quotes\n"
						 "x                        : x dimension of structure\n"
						 "y                        : y dimension of structure\n"
						 "cm                       : dimensions are in centimeters (default: inch) (opt.)\n"
						 "color=#rrggbb            : color to use for structure (default: #FF00FF) (opt.)\n" );
		exit( 1 );
	}

	fIn = fopen( argv[ 1 ], "r" );
	if( !fIn ) {
		fprintf( stderr, "Could not open the definition %s\n", argv[ 1 ] );
		exit( 1 );
	}

	fOut = fopen( argv[ 2 ], "w" );
	if( !fOut ) {
		fprintf( stderr, "Could not create the structures in %s\n", argv[ 2 ] );
		exit( 1 );
	}

	if( fgets( buffer, BUFSIZE, fIn ))
	{
		fputs( buffer, fOut );
		printf( "Creating %s\n", buffer + 9 );
	}
	
	while(fgets(buffer, BUFSIZE, fIn ))
	{
		err = 0;
		scale = strtok( buffer, " \"" );
		desc = strtok( NULL, "\"" );

		if( scale == NULL && desc == NULL )
			err = 1;

		/* get the size information */
		x = atof( strtok( NULL, " " ));
		y = atof( strtok( NULL, " " ));

		if( x == 0 || y == 0 ) {
			err = 1;
		}
		/* try to get the next token */
		p1 = strtok( NULL, " \r\n\t" );

		/* we have an additional token, check it */
		if( p1 ) {
			ptr = strtok( NULL, " \r\n\t" );
			if( !stricmp( p1, "cm" )) {
				x /= 2.54;
				y /= 2.54;
				p1 = ptr;
			}
		} else {
			p1 = strtok( NULL, " " );
		}

		if( p1 && !strnicmp( p1, "color=", strlen( "color=" ))) {
			color = atoi( p1 + strlen( "color=#" ));
		}

		if( !err ) {
			fprintf( fOut, "STRUCTURE %s \"%s\"\n", scale, desc );

			fprintf( fOut, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, 0.0, 0.0, 0.0, x );
			fprintf( fOut, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, 0.0, x, y, x );
			fprintf( fOut, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, y, x, y, 0.0 );
			fprintf( fOut, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, y, 0.0, 0.0, 0.0 );
			fprintf( fOut, "\tEND\n");
		} else {
			fprintf( stderr, "Error in line %d\n", count );
			exit( 1 );
		}

		count++;
	}

	printf( "Created %d structures.\n", count );
	fclose( fIn );
	fclose( fOut );
	exit(0);
}
