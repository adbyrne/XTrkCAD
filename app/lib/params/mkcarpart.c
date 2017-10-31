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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

int dumpUnknownRoadnames;
int updateRoadnames;
int newRoadnameCnt;


#if _MSC_VER > 1300
	#define stricmp _stricmp
	#define strnicmp _strnicmp
	#define strdup _strdup

#endif

#ifndef WIN32
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif // !WIN32

typedef struct {
	char * key;
	char * value;
	} map_element_t;
typedef struct {
	int max;
	int cnt;
	map_element_t * map;
	} map_t;

void readMap(
	char * mapFile,
	map_t * map )
{
	FILE * mapF;
	char line[256], *cp1;
	int len;
	mapF = fopen( mapFile, "r" );
	if ( mapF == NULL ) {
		perror( mapFile );
		exit(1);
	}
	while ( fgets( line, sizeof line, mapF ) != NULL ) {
		if ( map->cnt+1 > map->max ) {
			map->max += 10;
			map->map = (map_element_t*)realloc( map->map, map->max * sizeof *(map_element_t*)0 );
		}
		cp1 = strchr( line, '\t' );
		if ( cp1 == NULL ) {
			fprintf( stderr, "bad map line: %s\n", cp1 );
			continue;
		}
		while ( *cp1 == '\t' )
			*cp1++ = '\0';
		len = strlen( cp1 );
		if ( cp1[len-1] == '\n' )
			cp1[len-1] = '\0';
		map->map[map->cnt].key = strdup( line );
		map->map[map->cnt].value = strdup( cp1 );
		map->cnt++;
	}
	fclose( mapF );
}


struct roadname_t;
typedef struct roadname_t * roadname_p;
typedef struct roadname_t {
	roadname_p next;
	roadname_p alias;
	char * name;
	} roadname_t;
roadname_p roadnames;
roadname_p roadname_last;
roadname_p alias_last;
	
void readRoadnameMap(
	char * mapFile )
{
	FILE * mapF;
	char line[256];
	int len;
	int currLen = 0;
	roadname_p r_p;
	char * cp;
	mapF = fopen( mapFile, "r" );
	if ( mapF == NULL ) {
		perror( mapFile );
		exit(1);
	}
	while ( fgets( line, sizeof line, mapF ) != NULL ) {
		len = strlen( line );
		if ( line[len-1] == '\n' )
			line[--len] = '\0';
		if ( line[0] == '\0' || line[0] == '\n' || line[0] == '#' )
			continue;
		r_p = (roadname_p)malloc( sizeof *r_p );
		cp = line;
		if ( *cp != '\t' ) {
			cp = strchr( line, '\t' );
			if ( cp != NULL )
				*cp++ = '\0';
			r_p->next = NULL;
			r_p->alias = NULL;
			r_p->name = strdup( line );
			if ( roadnames == NULL )
				roadnames = r_p;
			else
				roadname_last->next = r_p;
			roadname_last = r_p;
			alias_last = r_p;
			r_p = (roadname_p)malloc( sizeof *r_p );
		} else {
			cp++;
		}
		r_p->next = NULL;
		r_p->alias = NULL;
		r_p->name = strdup(cp);
		alias_last->alias = r_p;
		alias_last = r_p;
	}
	fclose( mapF );
#ifdef LATER
	for ( r_p=roadnames; r_p; r_p=r_p->next ) {
		roadname_p r_p1;
		for ( r_p1=r_p; r_p1; r_p1=r_p1->alias )
			printf( "%s ", r_p1->name );
		printf("\n");
	}
#endif
}


char * lookupMap(
	map_t * map,
	char * key )
{
	int inx;
	for ( inx=0; inx<map->cnt; inx++ )
		if ( stricmp( key, map->map[inx].key ) == 0 )
			return map->map[inx].value;
	return NULL;
}

map_t colorMap;
map_t roadnameMap;

long lookupColor(
	char * colorS )
{
	if ( colorS == NULL || colorS[0] == '\0' )
		return 0x823F00;
	if ( !isdigit( colorS[0] ) )
		colorS = lookupMap( &colorMap, colorS );
	if ( colorS == NULL )
		return 0x823F00;
	return strtol( colorS, &colorS, 10 );
}


void capitalize(
	char * name )
{
	char * cp;
	if ( name[0] == '\0' ) return;
	name[0] = toupper( name[0] );
	for ( cp=name+1; *cp; cp++ )
		if ( ! isalpha(cp[-1]) )
			*cp = toupper( *cp );
		else
			*cp = tolower( *cp );
}


void canonicalize( char * name )
{
	char * cp, * cq;
	for ( cp=cq=name; *cp; cp++ ) {
		if ( *cp== '.' || *cp == ',' || *cp == '-' ) {
			*cp = ' ';
		} else if ( strnicmp( cp, " and ", 5 ) == 0 ) {
			cp[1] = '&';
			cp[2] = ' ';
			cp[3] = ' ';

		}
		if ( cp[0]!=' ' ||
		     ( cp!=name && cp[-1]!=' ' ) )
			*cq++ = *cp;
	}
	while ( cq[-1] == ' ' ) cq--;
	*cq++ = '\0';
}


void lookupRoadname(
	char * key,
	char * roadnameS,
	char * repmarkS )
{
	roadname_p r_p1, r_p2;
	canonicalize( key );
	if ( key == NULL || key[0] == '\0' || strnicmp( key, "undec", 5 ) == 0 ) {
		roadnameS[0] = '\0';
		repmarkS[0] = '\0';
		return;
	}
	for ( r_p1 = roadnames; r_p1; r_p1 = r_p1->next ) {
		for ( r_p2 = r_p1; r_p2; r_p2 = r_p2->alias ) {
			if ( stricmp( key, r_p2->name ) == 0 ) {
				if ( r_p1->name[0] != '?' )
					strcpy( repmarkS, r_p1->name );
				else
					repmarkS[0] = '\0';
				strcpy( roadnameS, r_p1->alias->name );
				return;
			}
		}
	}
	newRoadnameCnt++;
	strcpy( roadnameS, key );
	capitalize( roadnameS );
	repmarkS[0] = '\0';
	if ( dumpUnknownRoadnames )
		fprintf( stderr, "unknown roadname: %s\n", roadnameS );
	r_p2 = (roadname_p)malloc( sizeof *r_p2 );
	r_p2->name = strdup(roadnameS);
	r_p2->next = NULL;
	r_p2->alias = NULL;
	r_p1 = (roadname_p)malloc( sizeof *r_p2 );
	r_p1->next = NULL;
	r_p1->alias = r_p2;
	r_p1->name = "";
	roadname_last->next = r_p1;
	roadname_last = r_p1;
	alias_last = r_p2;
	if ( updateRoadnames ) {
		FILE * roadF;
		roadF = fopen( "roadname.tab", "a" );
		if ( roadF == NULL ) {
			perror( "roadname.tab" );
			updateRoadnames = 0;
			return;
		}
		fprintf( roadF, "?\t%s\n", roadnameS );
		fclose( roadF );
	}
}



void processFile(
	char * inFile,
	char * outFile )
{
	FILE * inF, * outF;
	char line[1024];
	char manuf[256];
	char proto[256];
	char desc[256];
	long color;
	char scale[256];
	double length;
	double width;
	double couplerLength;
	double truckCenter;
	double ratio = 0.0;
	int option = 0;
	int type = 30100;
	int lineNumber = 0;
	char roadnameS[256];
	char repmarkS[256];
	int len;
	int inx;
	char * cp, *cq;
	char * tab[20];
	char blanks[10];
	int partX = 1;
	int descX = 2;
	int roadX = 3;
	int numbX = 4;
	int colorX = 5;

	inF = fopen( inFile, "r" );
	if ( inF == NULL ) {
		perror( inFile );
		return;
	}
	outF = fopen( outFile, "w" );
	if ( outF == NULL ) {
		perror( outFile );
		return;
	}
	while ( fgets( line, sizeof line, inF )  != NULL ) {
		lineNumber++;
		if ( line[0] == '\n' || line[0] == '#' )
			continue;
		len = strlen(line);
		if ( line[len-1] == '\n' )
			line[len-1] = '\0';
		if ( strnicmp( line, "scale=", 6 ) == 0 ) {
			strcpy( scale, line+6 );
			if ( stricmp( scale, "N" ) == 0 )
				ratio = 160.0;
			else if ( stricmp( scale, "HO" ) == 0 )
				ratio = 87.1;
			else if ( stricmp( scale, "O" ) == 0 )
				ratio = 48.0;
			else if ( stricmp( scale, "S" ) == 0 )
				ratio = 64.0;
			else {
				fprintf( stderr, "%d: Unknown scale %s\n", lineNumber, scale );
				ratio = 87.1;
			}
			width = 120.0/ratio;
			couplerLength = 16.0/ratio;
		} else if ( strnicmp( line, "contents=", 9 ) == 0 ) {
			fprintf( outF, "CONTENTS %s\n", line+9 );
			printf( "Creating %s\n", line + 9 );
		} else if ( strnicmp( line, "order=", 6 ) == 0 ) {
			partX = descX = roadX = numbX = colorX = 0;
			for ( cp=line+6; *cp; cp++ ) {
				switch (*cp) {
				case '#': partX = cp-(line+6)+1; break;
				case 'd': case 'D': descX = cp-(line+6)+1; break;
				case 'r': case 'R': roadX = cp-(line+6)+1; break;
				case 'n': case 'N': numbX = cp-(line+6)+1; break;
				case '0': break;
				}
			}
		} else if ( strnicmp( line, "manuf=", 6 ) == 0 ) {
			strcpy( manuf, line+6 );
		} else if ( strnicmp( line, "type=", 5 ) == 0 ) {
			if ( stricmp( line+5, "diesel" ) == 0 ){
				option = 1;
				type = 10101;
			} else if ( stricmp( line+5, "steam" ) == 0 ){
				option = 1;
				type = 10201;
			} else if ( stricmp( line+5, "electric" ) == 0 ){
				option = 1;
				type = 10301;
			} else if ( stricmp( line+5, "freight" ) == 0 ){
				option = 0;
				type = 30100;
			} else if ( stricmp( line+5, "passenger" ) == 0 ){
				option = 0;
				type = 50100;
			} else if ( stricmp( line+5, "m-o-w" ) == 0 ){
				option = 0;
				type = 70100;
			} else if ( stricmp( line+5, "other" ) == 0 ){
				option = 0;
				type = 90100;
			} else  {
				fprintf( stderr, "%d: Unknown type: %s\n", lineNumber, line+5 );
			}
		} else if ( strnicmp( line, "proto=", 6 ) == 0 ) {
			strcpy( proto, line+6 );
			desc[0] = '\0';
		} else if ( strnicmp( line, "desc=", 5 ) == 0 ) {
			strcpy( desc, line+5 );
		} else if ( strnicmp( line, "length=", 7 ) == 0 ) {
			length = atof( line+7 );
			truckCenter = length * 0.75;
		} else if ( strnicmp( line, "protolength=", 12 ) == 0 ) {
			length = atof( line+12 );
			length = length / ratio;
			truckCenter = length * 0.75;
		} else if ( strnicmp( line, "width=", 6 ) == 0 ) {
			width = atof( line+6 );
		} else if ( strnicmp( line, "protowidth=", 11 ) == 0 ) {
			width = atof( line+11 );
			width = width * 12.0 / ratio;
		} else if ( strnicmp( line, "truckcenter=", 12 ) == 0 ) {
			truckCenter = atof( line+12 );
		} else if ( strnicmp( line, "couplerlength=", 14 ) == 0 ) {
			couplerLength = atof( line+14 );
		} else if ( strnicmp( line, "part=", 5 ) == 0 ) {
			if ( length == 0.000 )
				continue;
			cp = line+5;
			memset( blanks, 0, sizeof blanks );
			tab[0] = blanks;
			for ( cp=line+5,inx=1; cp; inx++ ) {
				tab[inx] = cp;
				cp = strchr( cp, '%' );
				if ( cp )
					*cp++ = '\0';
				while ( *tab[inx] == ' ' ) tab[inx]++;
				cq = tab[inx]+strlen(tab[inx]);
				while ( cq[-1] == ' ' ) cq--;
				*cq = '\0';
			}
			for ( ; inx<sizeof tab/sizeof tab[0]; inx++ ) {
				tab[inx] = blanks;
			}
			lookupRoadname( tab[roadX], roadnameS, repmarkS );
			color = lookupColor( tab[colorX] );
			capitalize( tab[descX] );
			fprintf( outF, "CARPART %s \"%s\t%s\t%s%s%s\t%s\t%s\t%s\t%s\" %d %d %0.3f %0.3f 0 0 %0.3f %0.3f %ld\n",
				scale, manuf, proto, desc, ((desc[0]&&tab[descX][0])?" ":""), tab[descX],
				tab[partX], roadnameS, repmarkS, tab[numbX],
				option, type, length, width, truckCenter, length+2*couplerLength, color );
		} else {
			fprintf( stderr, "%d: Unknown command: %s\n", lineNumber, line );
			exit(1);
		}
	}
	fclose( inF );
	fclose( outF );
}

int main ( int argc, char * argv[] )
{
	char *exename = argv[ 0 ];

	argv++;
	argc--;
	
	while ( argc > 0 && argv[0][0] == '-' ) {
		switch ( argv[0][1] ) {
		case 'r': dumpUnknownRoadnames++; break;
		case 'u': updateRoadnames++; break;
		}
		argv++;
		argc--;
	}

	if ( argc != 2 ) {
		fprintf( stderr, "Usage: %s [-r] <file>.car <file>.xtp\n", "mkcarpart" );
		exit(1);
	}

	readMap( "color.tab", &colorMap );
	readRoadnameMap( "roadname.tab" );
	processFile( argv[0], argv[1] );
	if ( newRoadnameCnt > 0 )
		fprintf( stderr, "%d new roadnames\n", newRoadnameCnt );
	return 0;
}

