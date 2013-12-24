#include <stdio.h>
#include <math.h>
#include "common.h"
#include "utility.h"

#include <string.h>
#include <stdlib.h>


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

#define GETMAXY \
		if (lp->y0 > maxY) maxY = lp->y0; \
		if (lp->y1 > maxY) maxY = lp->y1

static int trackSeparation = 20;
static int arrowHeadLength = 10;

static double FindCenter( 
		coOrd * pos,
		coOrd p0,
		coOrd p1,
		double radius )
{
	double d;
	double a0, a1;
	d = FindDistance( p0, p1 )/2.0;
	a0 = FindAngle( p0, p1 );
	a1 = NormalizeAngle(R2D(asin( d/radius )));
	if (a1 > 180)
		a1 -= 360;
	/*a0 = NormalizeAngle( a0 + (radius>0 ? +(90.0-a1) : -(90.0-a1) ) );*/
	a0 = NormalizeAngle( a0 + (90.0-a1) );
	Translate( pos, p0, a0, radius );
/*fprintf(stderr,"Center = %0.3f %0.3f\n", pos->x, pos->y );*/
	return a1*2.0;
}

static void buildDesignerLines( FILE * inf, FILE * outf )
{
	char line[80];
	int j;
	double num;
	double radius;
	coOrd p0, p1, q0, q1, pc;
	double a0, a1;
	double len;

	while ( fgets( line, sizeof line, inf ) != NULL ) {
		
		if ( strncmp( line, "ARROW", 5 ) == 0 ) {
			if ( sscanf( line, "ARROW, %lf, %lf, %lf, %lf",
						&p0.x, &p0.y, &p1.x, &p1.y ) != 4) {
				fprintf( stderr, "SYNTAX: %s", line );
				exit (1);
			}
			a0 = FindAngle( p1, p0 );
			fprintf( outf, "	{ 1, %ld, %ld, %ld, %ld },\n",
				(long)(p0.x+0.5), (long)(p0.y+0.5), (long)(p1.x+0.5), (long)(p1.y+0.5) );
			Translate( &p1, p0, a0+135, arrowHeadLength );
			fprintf( outf, "	{ 1, %ld, %ld, %ld, %ld },\n",
				(long)(p0.x+0.5), (long)(p0.y+0.5), (long)(p1.x+0.5), (long)(p1.y+0.5) );
			Translate( &p1, p0, a0-135, arrowHeadLength );
			fprintf( outf, "	{ 1, %ld, %ld, %ld, %ld },\n",
				(long)(p0.x+0.5), (long)(p0.y+0.5), (long)(p1.x+0.5), (long)(p1.y+0.5) );

		} else if ( strncmp( line, "LINE", 4 ) == 0 ) {
			if ( sscanf( line, "LINE, %lf, %lf, %lf, %lf",
						&p0.x, &p0.y, &p1.x, &p1.y ) != 4) {
				fprintf( stderr, "SYNTAX: %s", line );
				exit (1);
			}
			fprintf( outf, "	{ 1, %ld, %ld, %ld, %ld },\n",
				(long)(p0.x+0.5), (long)(p0.y+0.5), (long)(p1.x+0.5), (long)(p1.y+0.5) );

		} else if ( strncmp( line, "STRAIGHT", 8 ) == 0 ) {
			if ( sscanf( line, "STRAIGHT, %lf, %lf, %lf, %lf",
						&p0.x, &p0.y, &p1.x, &p1.y ) != 4) {
				fprintf( stderr, "SYNTAX: %s", line );
				exit (1);
			}
			a0 = FindAngle( p0, p1 );
			Translate( &q0, p0, a0+90, trackSeparation/2.0 );
			Translate( &q1, p1, a0+90, trackSeparation/2.0 );
			fprintf( outf, "	{ 3, %ld, %ld, %ld, %ld },\n",
				(long)(q0.x+0.5), (long)(q0.y+0.5), (long)(q1.x+0.5), (long)(q1.y+0.5) );
			Translate( &q0, p0, a0-90, trackSeparation/2.0 );
			Translate( &q1, p1, a0-90, trackSeparation/2.0 );
			fprintf( outf, "	{ 3, %ld, %ld, %ld, %ld },\n",
				(long)(q0.x+0.5), (long)(q0.y+0.5), (long)(q1.x+0.5), (long)(q1.y+0.5) );
		
		} else if ( strncmp( line, "CURVE", 5 ) == 0 ) {
			if ( sscanf( line, "CURVE, %lf, %lf, %lf, %lf, %lf",
						&p0.x, &p0.y, &p1.x, &p1.y, &radius ) != 5) {
				fprintf( stderr, "SYNTAX: %s", line );
				exit (1);
			}
			a1 = FindCenter( &pc, p0, p1, radius );
			a0 = FindAngle( pc, p0 );
/*fprintf(stderr, "A0 = %0.3f, A1 = %0.3f\n", a0, a1 );*/
			len = radius * M_PI * 2 * ( a1 / 360.0 );
			num = len/20;
			if (num < 0) num = - num;
			num++;
			a1 /= num;
			if (radius < 0)
				radius = -radius;
			for ( j=0; j<num; j++ ) {
/*fprintf( stderr, "A0 = %0.3f\n", a0 );*/
				Translate( &p0, pc, a0, radius+trackSeparation/2.0 );
				Translate( &p1, pc, a0+a1, radius+trackSeparation/2.0 );
				fprintf( outf, "		{ 3, %ld, %ld, %ld, %ld },\n",
						(long)(p0.x+0.5), (long)(p0.y+0.5), (long)(p1.x+0.5), (long)(p1.y+0.5) );
				Translate( &p0, pc, a0, radius-trackSeparation/2.0 );
				Translate( &p1, pc, a0+a1, radius-trackSeparation/2.0 );
				fprintf( outf, "		{ 3, %ld, %ld, %ld, %ld },\n",
						(long)(p0.x+0.5), (long)(p0.y+0.5), (long)(p1.x+0.5), (long)(p1.y+0.5) );
				a0 += a1;
				p0 = p1;
			}
		} else {
			fprintf( stderr, "SYNTAX2: %s", line );
		}
	}
}

int main( int argc, char * argv[] )
{
	buildDesignerLines( stdin, stdout );
	exit(0);
}
