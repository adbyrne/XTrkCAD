/*
 *	$Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/params/nmra-to.c,v 1.5 2008-06-09 19:34:06 m_fischer Exp $
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifndef WINDOWS
#include <unistd.h>
#else
#define M_PI 3.14159265358979323846
#define strncasecmp strnicmp
#endif
#include <stdlib.h>

#define DELIMITER " \t\r\n"
#define BUFSIZE 1024

#define SIN(A) sin(D2R(A))
#define COS(A) cos(D2R(A))
#define TAN(A) tan(D2R(A))

typedef struct {	/* a co-ordinate */
	double x;
	double y;
	} coOrd;

double normalizeAngle( double angle )
/* make sure <angle> is >= 0.0 and < 360.0 */
{
    while (angle<0) angle += 360.0;
    while (angle>=360) angle -= 360.0;
    return angle;
}

double D2R( double angle )
/* convert degrees to radians: for trig functions */
{
    return angle/180.0 * M_PI;
}

double R2D( double R )
/* concert radians to degrees */
{
    return normalizeAngle( R * 360.0 / (M_PI*2) );
}


double findDistance( coOrd p0, coOrd p1 )
/* find distance between two points */
{
    double dx = p1.x-p0.x, dy = p1.y-p0.y;
    return sqrt( dx*dx + dy*dy );
}

int small(double v )
/* is <v> close to 0.0 */
{
    return (fabs(v) < 0.0001);
}

double findAngle( coOrd p0, coOrd p1 )
/* find angle between two points */
{
    double dx = p1.x-p0.x, dy = p1.y-p0.y;
    if (small(dx)) {
        if (dy >=0) return 0.0;
        else return 180.0;
    }
    if (small(dy)) {
        if (dx >=0) return 90.0;
        else return 270.0;
    }
    return R2D(atan2( dx,dy ));
}


/* description of a curve */
typedef struct {
	char type;
	coOrd pos[2];
	double radius, a0, a1;
	coOrd center;
	} line_t;


void translate( coOrd *res, coOrd orig, double a, double d )
{
    res->x = orig.x + d * sin( D2R(a) );
    res->y = orig.y + d * cos( D2R(a) );
}


static void computeCurve( coOrd pos0, coOrd pos1, double radius, coOrd * center, double * a0, double * a1 )
/* translate between curves described by 2 end-points and a radius to
   a curve described by a center, radius and angles.
*/
{
    double d, a, aa, aaa, s;

    d = findDistance( pos0, pos1 )/2.0;
    a = findAngle( pos0, pos1 );
    s = fabs(d/radius);
    if (s > 1.0)
	s = 1.0;
    aa = R2D(asin( s ));
    if (radius > 0) {
        aaa = a + (90.0 - aa);
        *a0 = normalizeAngle( aaa + 180.0 );
        translate( center, pos0, aaa, radius );
    } else {
        aaa = a - (90.0 - aa);
	*a0 = normalizeAngle( aaa + 180.0 - aa *2.0 );
        translate( center, pos0, aaa, -radius );
    }
    *a1 = aa*2.0;
}


double X( double v )
{
    if ( -0.000001 < v && v < 0.000001 )
	return 0.0;
    else
	return v;
}

double getval( char * arg )
{
    char *cp;
    double a,b,c;
    a = strtod( arg, &cp );
    if (*cp == '.')
	return atof( arg );
    if (*cp == '\0')
	return a;
    if (*cp == '/') {
	c = strtod( cp+1, &arg );
	return a/c;
    }
    if (*cp != '-') {
	fprintf( stderr, "expected '-': %s\n", arg );
	exit(1);
    }
    b = strtod( cp+1, &arg );
    if (*arg != '/') {
	fprintf( stderr, "expected '/': %s\n", cp );
	exit(1);
    }
    c = strtod( arg+1, &cp );
    return a + b/c;
}


int main ( int argc, char * argv[] )
/* main: handle options, open files */
{
    double tg, pr, l, crr, fa, tl, hl, t;
    char * scale;
    char * desc;
    double a10, a11, a20, a21;
    coOrd q0, q1, q2, q3, q1c, q2c;

	char *buffer = malloc( BUFSIZE );
	FILE *fIn, *fOut;

    q0.x = q0.y = 0.0;

	if( argc != 3 )
	{
		fprintf( stderr, 
			     "Usage: %1 nmraturnoutdata paramfile\n\n"
				 "The data file is read line by line and turnout defimitions\n"
				 "are created in the param file.\n\n",
				 argv[ 0 ] );
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
		printf( "Creating %s\n", buffer + strlen("CONTENTS " ) );
		fputs( buffer, fOut );
	}
	while(fgets(buffer, BUFSIZE, fIn ))
	{
		if( buffer[ 0 ] == '#' ) {
			fputs( buffer, fOut );
			continue;
		}

	    scale = strtok( buffer, DELIMITER );
		desc = strtok( NULL, DELIMITER );
		tg = atof(strtok( NULL, DELIMITER ));
		q1.x = getval(strtok( NULL, DELIMITER ));
		q1.y = getval(strtok( NULL, DELIMITER ));
		pr = getval(strtok( NULL, DELIMITER ));
		l = getval(strtok( NULL, DELIMITER ));
		crr = getval(strtok( NULL, DELIMITER ));
		fa = getval(strtok( NULL, DELIMITER ));
		tl = getval(strtok( NULL, DELIMITER ));
		hl = getval(strtok( NULL, DELIMITER ));

		t = floor(fa);
		fa = t + (fa-t)/60*100;

		q2.x = l-tl;
		q2.y = tg-tl*TAN(fa);
		q3.x = l+hl;
		q3.y = tg+hl*SIN(fa);
		computeCurve( q0, q1, -pr, &q1c, &a10, &a11 );
		computeCurve( q1, q2, -crr, &q2c, &a20, &a21 );

		fprintf( fOut, "#NMRA-Std TO %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f\n",
			q1.x, q1.y, pr, l, crr, fa, tl, hl );

		fprintf( fOut, "TURNOUT %s \"NMRA %s\t#%s Right\t%sR\"\n", scale, scale, desc, desc);
		fprintf( fOut, "\tP \"Normal\" 1\n");
		fprintf( fOut, "\tP \"Reverse\" 2 3 4\n");
		fprintf( fOut, "\tE 0.000000 0.000000 270.000000\n");
		fprintf( fOut, "\tE %0.6f 0.000000 90.000000\n", l+hl);
		fprintf( fOut, "\tE %0.6f %0.6f %0.6f\n", q3.x, -q3.y, 90.0+fa);
		fprintf( fOut, "\tS 0 0 0.000000 0.000000 %0.6f 0.000000\n", l+hl);
		fprintf( fOut, "\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", pr, q1c.x, -q1c.y, normalizeAngle(180-a10-a11), a11 );
		fprintf( fOut, "\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", crr, q2c.x, -q2c.y, normalizeAngle(180-a20-a21), a21 );
		fprintf( fOut, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", q2.x, -q2.y, q3.x, -q3.y );
		fprintf( fOut, "\tEND\n");

		fprintf( fOut, "TURNOUT %s \"NMRA %s\t#%s Left\t%sL\"\n", scale, scale, desc, desc);
		fprintf( fOut, "\tP \"Normal\" 1\n");
		fprintf( fOut, "\tP \"Reverse\" 2 3 4\n");
		fprintf( fOut, "\tE 0.000000 0.000000 270.000000\n");
		fprintf( fOut, "\tE %0.6f 0.000000 90.000000\n", l+hl);
		fprintf( fOut, "\tE %0.6f %0.6f %0.6f\n", q3.x, q3.y, 90.0-fa);
		fprintf( fOut, "\tS 0 0 0.000000 0.000000 %0.6f 0.000000\n", l+hl);
		fprintf( fOut, "\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", -pr, q1c.x, q1c.y, a10, a11 );
		fprintf( fOut, "\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", -crr, q2c.x, q2c.y, a20, a21 );
		fprintf( fOut, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", q2.x, q2.y, q3.x, q3.y );
		fprintf( fOut, "\tEND\n");
	}
    exit(0);
}
