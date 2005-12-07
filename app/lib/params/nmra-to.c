/*
 *	$Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/lib/params/nmra-to.c,v 1.1 2005-12-07 15:48:05 rc-flyer Exp $
 */


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifndef _MSDOS
#include <unistd.h>
#else
#define M_PI 3.14159265358979323846
#define strncasecmp strnicmp
#endif
#include <stdlib.h>

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

    q0.x = q0.y = 0.0;
    if (argc != 12) {
	fprintf(stderr,"Usage: %s SCALE DESC TG P2 P4 P6 P8 P11 P18 P20 P21\n", argv[0]);
	exit(1);
    }
    argv++;
    scale = *argv++;
    desc = *argv++;
    tg = atof(*argv++);
    q1.x = getval(*argv++);
    q1.y = getval(*argv++);
    pr = getval(*argv++);
    l = getval(*argv++);
    crr = getval(*argv++);
    fa = getval(*argv++);
    tl = getval(*argv++);
    hl = getval(*argv++);

    t = floor(fa);
    fa = t + (fa-t)/60*100;

    q2.x = l-tl;
    q2.y = tg-tl*TAN(fa);
    q3.x = l+hl;
    q3.y = tg+hl*SIN(fa);
    computeCurve( q0, q1, -pr, &q1c, &a10, &a11 );
    computeCurve( q1, q2, -crr, &q2c, &a20, &a21 );

    printf("#NMRA-Std TO %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f\n",
	q1.x, q1.y, pr, l, crr, fa, tl, hl );

    printf("TURNOUT %s \"NMRA %s\t#%s Right\t%sR\"\n", scale, scale, desc, desc);
    printf("\tP \"Normal\" 1\n");
    printf("\tP \"Reverse\" 2 3 4\n");
    printf("\tE 0.000000 0.000000 270.000000\n");
    printf("\tE %0.6f 0.000000 90.000000\n", l+hl);
    printf("\tE %0.6f %0.6f %0.6f\n", q3.x, -q3.y, 90.0+fa);
    printf("\tS 0 0 0.000000 0.000000 %0.6f 0.000000\n", l+hl);
    printf("\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", pr, q1c.x, -q1c.y, normalizeAngle(180-a10-a11), a11 );
    printf("\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", crr, q2c.x, -q2c.y, normalizeAngle(180-a20-a21), a21 );
    printf("\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", q2.x, -q2.y, q3.x, -q3.y );
    printf("\tEND\n");

    printf("TURNOUT %s \"NMRA %s\t#%s Left\t%sL\"\n", scale, scale, desc, desc);
    printf("\tP \"Normal\" 1\n");
    printf("\tP \"Reverse\" 2 3 4\n");
    printf("\tE 0.000000 0.000000 270.000000\n");
    printf("\tE %0.6f 0.000000 90.000000\n", l+hl);
    printf("\tE %0.6f %0.6f %0.6f\n", q3.x, q3.y, 90.0-fa);
    printf("\tS 0 0 0.000000 0.000000 %0.6f 0.000000\n", l+hl);
    printf("\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", -pr, q1c.x, q1c.y, a10, a11 );
    printf("\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", -crr, q2c.x, q2c.y, a20, a21 );
    printf("\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", q2.x, q2.y, q3.x, q3.y );
    printf("\tEND\n");
    exit(0);
}
