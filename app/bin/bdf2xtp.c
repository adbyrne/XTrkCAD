/** \file bdf2xtp
 *	Bdf2xtp translates .bdf files (which are source files for Winrail track libraries) to .xtp files
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

char helpStr[] =
"Bdf2xtp translates .bdf files (which are source files for Winrail track\n"
"libraries) to .xtp files (which are XTrkCad parameter files).\n"
"Bdf2xtp is a MS-DOS command and must be in run in a DOS box under MS-Windows.\n"
"\n"
"Usage: bdf2xtp OPTIONS SOURCE.BDF TARGET.XTP\n"
"\n"
"OPTIONS:\n"
"  -c CONTENTS	 description of contents\n"
"  -k COLOR		 color of non-track segments\n"
"  -s SCALE		 scale of turnouts (ie. HO HOn3 N O S ... )\n"
"  -v			 verbose - include .bdf source as comments in .xtp file\n"
"\n"
"For example:\n"
"  bdf2xtp -c \"Faller HO Structures\" -k ff0000 -s HO fallerh0.bdf fallerh0.xtp\n"
"\n"
"Turnouts are composed of rails (which are Black) and lines.  Structures are\n"
"composed of only lines.  By default lines are Purple but you change this with\n"
"the -k optioon.  The color is specified as a 6 digit hexidecimal value, where\n"
"the first 2 digits are the Red value, the middle 2 digits are the Green value\n"
"and the last 2 digits are the Blue value\n"
"  ff0000		 Red\n"
"  00ff00		 Green\n"
"  00ffff		 Yellow\n"
;

/* NOTES:
BDF files have a number of constructors for different types of turnouts
with different ways of describing the track segements that comprise them.
XTP files have a orthogonal description which is:
		TURNOUT ....	 header line
			P ...		 paths
			E ...		 endpoints
			S ...		 straight track segments
			C ...		 curved track segments
			L ...		 straight line segments
			A ...		 curved (arc) line segments
Structures are similar but with only L and A lines.

Paths describe the routing from one end-point to some other.
The routes are a sequence of indices (1-based) in the list of segments.
Some things (like crossings, crossovers and slip switches) have more than
one route for a path (which are then separated by 0:
	--1--+--2--+--3--
		  \	  /
		   4 5
			x
		   / \
		  /	  \
	--6--+--7--+--8--
The normal routes would be 1,2,3 and 6,7,8.
The reverse routes would be 1,4,8 and 6,5,3.
The path lines are:
   P "Normal" 1 2 3 0 6 7 8
   P "Reverse" 1 4 8 0 6 5 3
Paths are not currently being used but will be when you can run trains
on the layout.


Processing:
A table (tokens) describes each type of source line.
For each type the segments and end-points are computed and added to
lists (segs and endPoints).
When the END for a turnout is reached the Path lines are computed by
searching for routes between end-points through the segments.
Then the list of segments is written out to the output file.
*/



#define MAXSEG	(40)	/* Maximum number of segments in an object */

typedef struct {		/* a co-ordinate */
		double x;
		double y;
		} coOrd;

FILE * fin;				/* input file */
FILE * fout;			/* output file */
int inch;				/* metric or english units */
char * scale = NULL;	/* scale from command line */
int verbose = 0;		/* include source as comments? */
char line[1024];		/* input line buffer */
int lineCount;			/* source line number */
int lineLen;			/* source line length */
int inBody;				/* seen header? */
long color = 0x00FF00FF;/* default color */

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
	return (fabs(v) < 0.000000000001);
}

double findAngle( coOrd p0, coOrd p1 )
/* find angle between two points */
{
	double dx = p1.x-p0.x, dy = p1.y-p0.y;
	if (small(dx) && small(dy)) {
		if (dy >=0.0) return 0.0;
		else return 180.0;
	}
	return R2D(atan2( dx,dy ));
}


/* Where do we expect each input line? */
typedef enum {
		CLS_NULL,
		CLS_START,
		CLS_END,
		CLS_BODY
		} class_e;

/* Type of input line */
typedef enum {
		ACT_UNKNOWN,
		ACT_DONE,
		ACT_STRAIGHT,
		ACT_CURVE,
		ACT_TURNOUT_LEFT,
		ACT_TURNOUT_RIGHT,
		ACT_CURVEDTURNOUT_LEFT,
		ACT_CURVEDTURNOUT_RIGHT,
		ACT_THREEWAYTURNOUT,
		ACT_CROSSING_LEFT,
		ACT_CROSSING_RIGHT,
		ACT_DOUBLESLIP_LEFT,
		ACT_DOUBLESLIP_RIGHT,
		ACT_CROSSING_SYMMETRIC,
		ACT_DOUBLESLIP_SYMMETRIC,
		ACT_TURNTABLE,
		ACT_ENDTURNTABLE,
		ACT_TRANSFERTABLE,
		ACT_ENDTRANSFERTABLE,
		ACT_TRACK,
		ACT_STRUCTURE,
		ACT_ENDSTRUCTURE,

		ACT_FILL_POINT,
		ACT_LINE,
		ACT_CURVEDLINE,
		ACT_CIRCLE,
		ACT_DESCRIPTIONPOS,
		ACT_ARTICLENOPOS,
		ACT_CONNECTINGPOINT,
		ACT_STRAIGHTTRACK,
		ACT_CURVEDTRACK,
		ACT_STRAIGHT_BODY,
		ACT_CURVE_BODY,
		ACT_PRICE
		} action_e;

/* input line description */
typedef struct {
		char * name;	/* first token on line */
		class_e class;	/* where do we expect this? */
		action_e action;/* what type of line is it */
		char *args;		/* what else is on the line */
		} tokenDesc_t;

/* first token on each line tells what kind of line it is */
tokenDesc_t tokens[] = {

		{ "Straight", CLS_START, ACT_STRAIGHT, "SSNN" },
		{ "EndStraight", CLS_END, ACT_DONE, NULL },
		{ "Curve", CLS_START, ACT_CURVE, "SSNNN" },
		{ "EndCurve", CLS_END, ACT_DONE, NULL },
		{ "Turnout_Left", CLS_START, ACT_TURNOUT_LEFT, "SSN" },
		{ "Turnout_Right", CLS_START, ACT_TURNOUT_RIGHT, "SSN" },
		{ "EndTurnout", CLS_END, ACT_DONE, NULL },
		{ "CurvedTurnout_Left", CLS_START, ACT_CURVEDTURNOUT_LEFT, "SSN" },
		{ "CurvedTurnout_Right", CLS_START, ACT_CURVEDTURNOUT_RIGHT, "SSN" },
		{ "ThreeWayTurnout", CLS_START, ACT_THREEWAYTURNOUT, "SSN" },
		{ "Crossing_Left", CLS_START, ACT_CROSSING_LEFT, "SSNNNN" },
		{ "Crossing_Right", CLS_START, ACT_CROSSING_RIGHT, "SSNNNN" },
		{ "DoubleSlip_Left", CLS_START, ACT_DOUBLESLIP_LEFT, "SSNNNNN" },
		{ "DoubleSlip_Right", CLS_START, ACT_DOUBLESLIP_RIGHT, "SSNNNNN" },
		{ "Crossing_Symetric", CLS_START, ACT_CROSSING_SYMMETRIC, "SSNNN" },
		{ "DoubleSlip_Symetric", CLS_START, ACT_DOUBLESLIP_SYMMETRIC, "SSNNNN" },
		{ "EndCrossing", CLS_END, ACT_DONE, NULL },
		{ "Turntable", CLS_START, ACT_TURNTABLE, "SSNNNN" },
		{ "EndTurntable", CLS_END, ACT_ENDTURNTABLE, NULL },
		{ "TravellingPlatform", CLS_START, ACT_TRANSFERTABLE, "SSNNNNN" },
		{ "EndTravellingPlatform", CLS_END, ACT_ENDTRANSFERTABLE, NULL },
		{ "Track", CLS_START, ACT_TRACK, "SSN" },
		{ "EndTrack", CLS_END, ACT_DONE, NULL },
		{ "Structure", CLS_START, ACT_STRUCTURE, "SS" },
		{ "EndStructure", CLS_END, ACT_ENDSTRUCTURE, NULL },

		{ "FillPoint", CLS_BODY, ACT_FILL_POINT, "NNI" },
		{ "Line", CLS_BODY, ACT_LINE, "NNNN" },
		{ "CurvedLine", CLS_BODY, ACT_CURVEDLINE, "NNNNN" },
		{ "CurveLine", CLS_BODY, ACT_CURVEDLINE, "NNNNN" },
		{ "Circle", CLS_BODY, ACT_CIRCLE, "NNN" },
		{ "DescriptionPos", CLS_BODY, ACT_DESCRIPTIONPOS, "NN" },
		{ "ArticleNoPos", CLS_BODY, ACT_DESCRIPTIONPOS, "NN" },
		{ "ConnectingPoint", CLS_BODY, ACT_CONNECTINGPOINT, "NNN" },
		{ "StraightTrack", CLS_BODY, ACT_STRAIGHTTRACK, "NNNN" },
		{ "CurvedTrack", CLS_BODY, ACT_CURVEDTRACK, "NNNNN" },
		{ "Straight", CLS_BODY, ACT_STRAIGHT_BODY, "N" },
		{ "Curve", CLS_BODY, ACT_CURVE_BODY, "NNN" },
		{ "Price", CLS_BODY, ACT_PRICE, "N" },

		{ "Gerade", CLS_START, ACT_STRAIGHT, "SSNN" },
		{ "EndGerade", CLS_END, ACT_DONE, NULL },
		{ "Bogen", CLS_START, ACT_CURVE, "SSNNN" },
		{ "EndBogen", CLS_END, ACT_DONE, NULL },
		{ "Weiche_links", CLS_START, ACT_TURNOUT_LEFT, "SSN" },
		{ "Weiche_Rechts", CLS_START, ACT_TURNOUT_RIGHT, "SSN" },
		{ "EndWeiche", CLS_END, ACT_DONE, NULL },
		{ "Bogenweiche_Links", CLS_START, ACT_CURVEDTURNOUT_LEFT, "SSN" },
		{ "Bogenweiche_Rechts", CLS_START, ACT_CURVEDTURNOUT_RIGHT, "SSN" },
		{ "Dreiwegweiche", CLS_START, ACT_THREEWAYTURNOUT, "SSN" },
		{ "Kreuzung_Links", CLS_START, ACT_CROSSING_LEFT, "SSNNNN" },
		{ "Kreuzung_Rechts", CLS_START, ACT_CROSSING_RIGHT, "SSNNNN" },
		{ "DKW_Links", CLS_START, ACT_DOUBLESLIP_LEFT, "SSNNNNN" },
		{ "DKW_Rechts", CLS_START, ACT_DOUBLESLIP_RIGHT, "SSNNNNN" },
		{ "Kreuzung_Symmetrisch", CLS_START, ACT_CROSSING_SYMMETRIC, "SSNNN" },
		{ "DKW_Symmetrisch", CLS_START, ACT_DOUBLESLIP_SYMMETRIC, "SSNNNN" },
		{ "EndKreuzung", CLS_END, ACT_DONE, NULL },
		{ "Drehscheibe", CLS_START, ACT_TURNTABLE, "SSNNNN" },
		{ "EndDrehscheibe", CLS_END, ACT_ENDTURNTABLE, NULL },
		{ "Schiebebuehne", CLS_START, ACT_TRANSFERTABLE, "SSNNNNN" },
		{ "EndSchiebebuehne", CLS_END, ACT_ENDTRANSFERTABLE, NULL },
		{ "Schiene", CLS_START, ACT_TRACK, "SSN" },
		{ "EndSchiene", CLS_END, ACT_DONE, NULL },
		{ "Haus", CLS_START, ACT_STRUCTURE, "SS" },
		{ "EndHaus", CLS_END, ACT_ENDSTRUCTURE, NULL },

		{ "FuellPunkt", CLS_BODY, ACT_FILL_POINT, "NNI" },
		{ "Linie", CLS_BODY, ACT_LINE, "NNNN" },
		{ "Bogenlinie", CLS_BODY, ACT_CURVEDLINE, "NNNNN" },
		{ "Kreislinie", CLS_BODY, ACT_CIRCLE, "NNN" },
		{ "BezeichnungsPos", CLS_BODY, ACT_DESCRIPTIONPOS, "NN" },
		{ "ArtikelNrPos", CLS_BODY, ACT_DESCRIPTIONPOS, "NN" },
		{ "Anschlusspunkt", CLS_BODY, ACT_CONNECTINGPOINT, "NNN" },
		{ "GeradesGleis", CLS_BODY, ACT_STRAIGHTTRACK, "NNNN" },
		{ "BogenGleis", CLS_BODY, ACT_CURVEDTRACK, "NNNNN" },
		{ "Gerade", CLS_BODY, ACT_STRAIGHT_BODY, "N" },
		{ "Bogen", CLS_BODY, ACT_CURVE_BODY, "NNN" },
		{ "Preis", CLS_BODY, ACT_PRICE, "N" } };


/* argument description */
typedef union {
		char * string;
		double number;
		long integer;
		} arg_t;

/* description of a curve */
typedef struct {
		char type;
		coOrd pos[2];
		double radius, a0, a1;
		coOrd center;
		} line_t;

/* state info for the current object */
int curAction;
line_t lines[MAXSEG];
line_t *line_p;
char * name;
char * partNo;
double params[10];
int right = 0;

/* A XTrkCad End-Point */
typedef struct {
		int busy;
		coOrd pos;
		double a;
		} endPoint_t;
endPoint_t endPoints[MAXSEG];
endPoint_t *endPoint_p;

/* the segments */
typedef struct {
		double radius;
		coOrd pos[2];
		int mark;
		endPoint_t * ep[2];
		} segs_t;
segs_t segs[MAXSEG];
segs_t *seg_p;


/* the segment paths */
typedef struct {
		int index;
		int count;
		int segs[MAXSEG];
		} paths_t;
paths_t paths[MAXSEG];
paths_t *paths_p;

int curPath[MAXSEG];
int curPathInx;

char * pathNames[] =  {
		"Normal",
		"Reverse" };

int isclose( coOrd a, coOrd b )
{
	if ( fabs(a.x-b.x) < 0.1 &&
		 fabs(a.y-b.y) < 0.1 )
		return 1;
	else
		return 0;
}


void searchSegs( segs_t * sp, int ep )
/* Recursively search the segs looking for the next segement that begins
   where this (sp->pos[ep]) one ends.  We mark the ones we have already
   used (sp->mark).
   Returns when we can't continue.
   Leaves the path in curPath[]
*/
{
	segs_t *sp1;
	int inx;

	sp->mark = 1;
	curPath[curPathInx] = (ep==0?-((sp-segs)+1):((sp-segs)+1));
	if (sp->ep[ep] != NULL) {
		inx = abs(curPath[0]);
		if ( (sp-segs)+1 < inx )
			return;
		paths_p->index = 0;
		paths_p->count = curPathInx+1;
		for (inx=0;inx<=curPathInx;inx++)
			paths_p->segs[inx] = curPath[inx];
		paths_p++;
		return;
	}
	curPathInx++;
	for ( sp1 = segs; sp1<seg_p; sp1++ ) {
		if (!sp1->mark) {
			if ( isclose( sp->pos[ep], sp1->pos[0] ) )
				searchSegs( sp1, 1 );
			else if ( isclose( sp->pos[ep], sp1->pos[1] ) )
				searchSegs( sp1, 0 );
		}
	}
	curPathInx--;
}


void computePaths( void )
/* Generate the path lines.	 Search the segments for nonoverlapping
   routes between end-points.
 */
{
	char **name = pathNames;
	segs_t * sp, *sp1;
	endPoint_t *ep, *ep2;
	int inx;
	char bitmap[MAXSEG];
	paths_t * pp;
	int pathIndex;
	int pathCount;
	int firstPath;
	int segNo;
	int epNo;

	paths_p = paths;
	for ( sp = segs; sp<seg_p; sp++ ) {
		sp->ep[0] = sp->ep[1] = NULL;
		for ( ep = endPoints; ep<endPoint_p; ep++ ) {
			if ( isclose( ep->pos, sp->pos[0] ) ) {
				sp->ep[0] = ep;
			} else if ( isclose( ep->pos, sp->pos[1] ) ) {
				sp->ep[1] = ep;
			}
		}
	}
	for ( sp = segs; sp<seg_p; sp++ ) {
		for ( sp1 = segs; sp1<seg_p; sp1++ )
			sp1->mark = 0;
		curPathInx = 0;
		if ( sp->ep[0] ) {
			searchSegs( sp, 1 );
		} else if ( sp->ep[1] ) {
			searchSegs( sp, 0 );
		}
	}
	pathIndex = 0;
	pathCount = paths_p-paths;
	while (pathCount>0) {
		if (pathIndex < 2)
			fprintf( fout, "\tP \"%s\"", pathNames[pathIndex] );
		else
			fprintf( fout, "\tP \"%d\"", pathIndex+1 );
		pathIndex++;
		firstPath = 1;
		memset( bitmap, 0, sizeof bitmap );
		for ( ep = endPoints; ep<endPoint_p; ep++ ) {
			ep->busy = 0;
		}
		for (pp = paths; pp < paths_p; pp++) {
			if (pp->count == 0)
				continue;
			segNo = pp->segs[0];
			epNo = (segNo>0?0:1);
			ep = segs[abs(segNo)-1].ep[epNo];
			segNo = pp->segs[pp->count-1];
			epNo = (segNo>0?1:0);
			ep2 = segs[abs(segNo)-1].ep[epNo];
			if ( (ep && ep->busy) || (ep2 && ep2->busy) ) {
				goto nextPath;
			}
			if (ep) ep->busy = 1;
			if (ep2) ep2->busy = 1;
			for (inx=0; inx<pp->count; inx++) {
				segNo = abs(pp->segs[inx]);
				if (bitmap[segNo])
					goto nextPath;
			}
			if (!firstPath) {
				fprintf( fout, " 0");
			} else {
				firstPath = 0;
			}
			for (inx=0; inx<pp->count; inx++) {
				segNo = abs(pp->segs[inx]);
				bitmap[segNo] = 1;
				fprintf( fout, " %d", pp->segs[inx] );
			}
			pp->count = 0;
			pathCount--;
nextPath:
		;
		}
		fprintf( fout, "\n" );
	}
}


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


void generateTurnout( void )
/* Seen the END so pump out the the TURNOUT
   Write out the header and the segment descriptions.
 */
{
	segs_t *sp;
	line_t *lp;
	endPoint_t *ep;
	double d, a, aa, aaa, a0, a1;
	coOrd center;

	fprintf( fout, "TURNOUT %s \"%s %s\"\n", scale, partNo, name );
	computePaths();
	for (ep=endPoints; ep<endPoint_p; ep++)
		fprintf( fout, "\tE %0.6f %0.6f %0.6f\n",
						X(ep->pos.x), X(ep->pos.y), X(ep->a) );
	for (lp=lines; lp<line_p; lp++) {
		switch (lp->type) {
		case 'L':
			fprintf( fout, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color,
						X(lp->pos[0].x), X(lp->pos[0].y), X(lp->pos[1].x), X(lp->pos[1].y) );
			break;
		case 'A':
			fprintf( fout, "\tA %ld 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", color,
						X(lp->radius), X(lp->center.x), X(lp->center.y), X(lp->a0), X(lp->a1) );
			break;
		}
	}
	for (sp=segs; sp<seg_p; sp++)
		if (sp->radius == 0.0) {
			fprintf( fout, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n",
						X(sp->pos[0].x), X(sp->pos[0].y), X(sp->pos[1].x), X(sp->pos[1].y) );
		} else {
			computeCurve( sp->pos[0], sp->pos[1], sp->radius, &center, &a0, &a1 );
			fprintf( fout, "\tC 0 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n",
						X(sp->radius), X(center.x), X(center.y), X(a0), X(a1) );
		}
	fprintf( fout, "\tEND\n" );
}


void reset( tokenDesc_t * tp, arg_t *args )
/* Start of a new turnout or structure */
{
	int inx;
		curAction = tp->action;
		line_p = lines;
		seg_p = segs;
		endPoint_p = endPoints;
		partNo = strdup( args[0].string );
		name = strdup( args[1].string );
		for (inx=2; tp->args[inx]; inx++)
			params[inx-2] = args[inx].number;
}

double getDim( double value )
/* convert to inches from tenths of a an inch or millimeters. */
{
	if (inch)
		return value/10.0;
	else
		return value/25.4;
}


char * getLine( void )
/* Get a source line, trim CR/LF, handle comments */
{
	char * cp;
	while (1) {
		if (fgets(line, sizeof line, fin) == NULL)
			return NULL;
		lineCount++;
		lineLen = strlen(line);
		if (lineLen > 0 && line[lineLen-1] == '\n') {
			line[lineLen-1] = '\0';
			lineLen--;
		}
		if (lineLen > 0 && line[lineLen-1] == '\r') {
			line[lineLen-1] = '\0';
			lineLen--;
		}
		cp = strchr( line, ';');
		if (cp) {
			*cp = '\0';
			lineLen = cp-line;
		}
		cp = line;
		while ( isspace((unsigned char)*cp) ) {
			cp++;
			lineLen--;
		}
		if (lineLen <= 0)
			continue;
		if (verbose)
			fprintf( fout, "# %s\n", line );
		return cp;
	}
}


void flushInput( void )
/* Eat source until we see an END - error recovery */
{
	char *cp;
	while (cp=getLine()) {
		if (strncasecmp( cp, "End", 3 ) == 0 )
			break;
	}
	inBody = 0;
}


void process( tokenDesc_t * tp, arg_t *args )
/* process a tokenized line */
{

	int inx;
	int count;
	int endNo;
	double radius, radius2;
	static double angle;
	double length, length2;
	double width, width2, offset;
	double a0, a1;
	static char bits[128];
	int rc;
	char * cp;
	line_t * lp;
	coOrd pos0, pos1;
	static int threeway;

	switch (tp->action) {

	case ACT_DONE:
		generateTurnout();
		right = 0;
		threeway = 0;
		break;

	case ACT_STRAIGHT:
		reset( tp, args );
		seg_p->radius = 0.0;
		endPoint_p->pos.x = seg_p->pos[0].x = 0.0;
		endPoint_p->pos.y = seg_p->pos[0].y = 0.0;
		endPoint_p->a = 270.0;
		endPoint_p++;
		endPoint_p->pos.x = seg_p->pos[1].x = getDim(args[2].number);
		endPoint_p->pos.y = seg_p->pos[1].y = 0.0;
		endPoint_p->a = 90.0;
		endPoint_p++;
		seg_p++;
		break;

	case ACT_CURVE:
		reset( tp, args );
		radius = getDim(args[2].number);
		seg_p->radius = -radius;
		endPoint_p->pos.y = seg_p->pos[0].y = 0.0;
		endPoint_p->pos.x = seg_p->pos[0].x = 0.0;
		endPoint_p->a = 270.0;
		endPoint_p++;
		angle = args[3].number;
		endPoint_p->a = 90.0-angle;
		endPoint_p->pos.x = seg_p->pos[1].x = radius * sin(D2R(angle));
		endPoint_p->pos.y = seg_p->pos[1].y = radius * (1-cos(D2R(angle)));
		endPoint_p++;
		seg_p++;
		break;

	case ACT_TURNOUT_RIGHT:
		right = 1;
	case ACT_TURNOUT_LEFT:
		reset( tp, args );
		break;

	case ACT_CURVEDTURNOUT_RIGHT:
		right = 1;
	case ACT_CURVEDTURNOUT_LEFT:
		reset( tp, args );
		endPoint_p->pos.y = 0.0;
		endPoint_p->pos.x = 0.0;
		endPoint_p->a = 270.0;
		endPoint_p++;
		if ((cp=getLine())==NULL)
			return;
		if ((rc=sscanf( line, "%lf %lf", &radius, &angle ) ) != 2) {
			fprintf( stderr, "syntax error: %d: %s\n", lineCount, line );
			flushInput();
			return;
		}
		radius = getDim( radius );
		endPoint_p->pos.x = radius*sin(D2R(angle));
		endPoint_p->pos.y = radius*(1-cos(D2R(angle)));
		endPoint_p->a = 90.0-angle;
		seg_p->pos[0].y = 0;
		seg_p->pos[0].x = 0;
		seg_p->pos[1] = endPoint_p->pos;
		seg_p->radius = -radius;
		endPoint_p++;
		seg_p++;
		if ((cp=getLine())==NULL)
			return;
		if ((rc=sscanf( line, "%lf %lf", &radius2, &angle ) ) != 2) {
			fprintf( stderr, "syntax error: %d: %s\n", lineCount, line );
			flushInput();
			return;
		}
		radius2 = getDim( radius2 );
		endPoint_p->pos.x = radius*sin(D2R(angle)) + (radius2-radius);
		endPoint_p->pos.y = radius*(1-cos(D2R(angle)));
		endPoint_p->a = 90.0-angle;
		seg_p->pos[0] = seg_p[-1].pos[0];
		seg_p->pos[1].x = radius2-radius;
		seg_p->pos[1].y = 0;
		seg_p->radius = 0;
		seg_p++;
		seg_p->pos[0].x = radius2-radius;
		seg_p->pos[0].y = 0;
		seg_p->pos[1] = endPoint_p->pos;
		seg_p->radius = -radius;
		endPoint_p++;
		seg_p++;
		if (tp->action == ACT_CURVEDTURNOUT_RIGHT) {
			endPoint_p[-1].pos.y = -endPoint_p[-1].pos.y;
			endPoint_p[-1].a = 180.0-endPoint_p[-1].a;
			seg_p[-1].pos[0].y = -seg_p[-1].pos[0].y;
			seg_p[-1].pos[1].y = -seg_p[-1].pos[1].y;
			seg_p[-1].radius = -seg_p[-1].radius;
			endPoint_p[-2].pos.y = -endPoint_p[-2].pos.y;
			endPoint_p[-2].a = 180.0-endPoint_p[-2].a;
			seg_p[-3].pos[0].y = -seg_p[-3].pos[0].y;
			seg_p[-3].pos[1].y = -seg_p[-3].pos[1].y;
			seg_p[-3].radius = -seg_p[-3].radius;
		}
		break;
	case ACT_THREEWAYTURNOUT:
		reset( tp, args );
		threeway = 1;
		break;

	case ACT_CROSSING_LEFT:
	case ACT_CROSSING_RIGHT:
	case ACT_CROSSING_SYMMETRIC:
	case ACT_DOUBLESLIP_LEFT:
	case ACT_DOUBLESLIP_RIGHT:
	case ACT_DOUBLESLIP_SYMMETRIC:
		reset( tp, args );
		angle = args[3].number;
		length = getDim(args[4].number);
		seg_p->radius = 0.0;
		endPoint_p->pos.y = seg_p->pos[0].y = 0.0;
		endPoint_p->pos.x = seg_p->pos[0].x = 0.0;
		endPoint_p->a = 270.0;
		endPoint_p++;
		endPoint_p->pos.x = seg_p->pos[1].x = length;
		endPoint_p->pos.y = seg_p->pos[1].y = 0.0;
		endPoint_p->a = 90.0;
		endPoint_p++;
		seg_p++;
		length /= 2.0;
		if (tp->action == ACT_CROSSING_SYMMETRIC ||
			tp->action == ACT_DOUBLESLIP_SYMMETRIC) {
			length2 = length;
		} else {
			length2 = getDim( args[5].number )/2.0;
		}
		seg_p->radius = 0.0;
		endPoint_p->pos.x = seg_p->pos[0].x = length - length2*cos(D2R(angle));
		endPoint_p->pos.y = seg_p->pos[0].y = length2*sin(D2R(angle));
		endPoint_p->a = normalizeAngle(270.0+angle);
		endPoint_p++;
		endPoint_p->pos.x = seg_p->pos[1].x = length*2.0-seg_p->pos[0].x;
		endPoint_p->pos.y = seg_p->pos[1].y = -seg_p->pos[0].y;
		endPoint_p->a = normalizeAngle(90.0+angle);
		endPoint_p++;
		seg_p++;
		if (tp->action == ACT_CROSSING_RIGHT ||
			tp->action == ACT_DOUBLESLIP_RIGHT ) {
			endPoint_p[-1].pos.y = -endPoint_p[-1].pos.y;
			endPoint_p[-2].pos.y = -endPoint_p[-2].pos.y;
			seg_p[-1].pos[0].y = -seg_p[-1].pos[0].y;
			seg_p[-1].pos[1].y = -seg_p[-1].pos[1].y;
			endPoint_p[-1].a = normalizeAngle( 180.0 - endPoint_p[-1].a );
			endPoint_p[-2].a = normalizeAngle( 180.0 - endPoint_p[-2].a );
		}
		break;

	case ACT_TURNTABLE:
		reset( tp, args );
		if ((cp=getLine())==NULL)
			return;
		if ((rc=sscanf( line, "%lf %s", &angle, bits ) ) != 2) {
			fprintf( stderr, "syntax error: %d: %s\n", lineCount, line );
			flushInput();
			return;
		}
		fprintf( fout, "TURNOUT %s \"%s %s\"\n", scale, partNo,	 name );
		count = 360.0/angle;
		angle = 0;
		length = strlen( bits );
		if (length < count)
			count = length;
		length = getDim( args[3].number );
		length2 = getDim( args[5].number );
		endNo = 1;
		for ( inx=0; inx<count; inx++ ) {
			if (bits[inx]!='0') {
				fprintf( fout, "\tP \"%d\" %d\n", endNo, endNo );
				endNo++;
			}
		}
		for ( inx=0; inx<count; inx++ ) {
			angle = normalizeAngle( 90.0 - inx * ( 360.0 / count ) );
			if (bits[inx]!='0')
				fprintf( fout, "\tE %0.6f %0.6f %0.6f\n",
						X(length * sin(D2R(angle))),
						X(length * cos(D2R(angle))),
						X(angle) );
		}
		for ( inx=0; inx<count; inx++ ) {
			angle = normalizeAngle( 90.0 - inx * ( 360.0 / count ) );
			if (bits[inx]!='0')
				fprintf( fout, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n",
						X(length * sin(D2R(angle))),
						X(length * cos(D2R(angle))),
						X(length2 * sin(D2R(angle))),
						X(length2 * cos(D2R(angle))) );
		}
		fprintf( fout, "\tA %ld 0 %0.6f 0.000000 0.000000 0.000000 360.000000\n",
						color, length2 );
		if (length != length2)
			fprintf( fout, "\tA %ld 0 %0.6f 0.000000 0.000000 0.000000 360.000000\n",
						color, length );
		break;

	case ACT_ENDTURNTABLE:
		for (lp=lines; lp<line_p; lp++) {
			switch (lp->type) {
			case 'L':
				fprintf( fout, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color,
						X(lp->pos[0].x), X(lp->pos[0].y), X(lp->pos[1].x), X(lp->pos[1].y) );
				break;
			case 'A':
				fprintf( fout, "\tA %ld 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", color,
						X(lp->radius), X(lp->center.x), X(lp->center.y), X(lp->a0), X(lp->a1) );
				break;
			}
		}
		fprintf( fout, "\tEND\n" );
		break;

	case ACT_TRANSFERTABLE:
		reset( tp, args );
		fprintf( fout, "TURNOUT %s \"%s %s\"\n", scale, partNo,	 name );
		width = getDim(args[3].number);
		width2 = getDim(args[5].number);
		length = getDim( args[6].number);
		fprintf( fout, "\tL %ld 0 0.0000000 0.000000 0.000000 %0.6f\n", color, length );
		fprintf( fout, "\tL %ld 0 0.0000000 %0.6f %0.6f %0.6f\n", color, length, width, length );
		fprintf( fout, "\tL %ld 0 %0.6f %0.6f %0.6f 0.000000\n", color, width, length, width );
		fprintf( fout, "\tL %ld 0 %0.6f 0.0000000 0.000000 0.000000\n", color, width );
		fprintf( fout, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color,
				(width-width2)/2.0, 0.0, (width-width2)/2.0, length );
		fprintf( fout, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color,
				width-(width-width2)/2.0, 0.0, width-(width-width2)/2.0, length );
		if ((cp=getLine())==NULL)
			return;
		if ((rc=sscanf( line, "%lf %lf %s", &length2, &offset, bits ) ) != 3) {
			fprintf( stderr, "syntax error: %d: %s\n", lineCount, line );
			flushInput();
			return;
		}
		offset = getDim( offset );
		length2 = getDim( length2 );
		for (inx=0; bits[inx]; inx++) {
			if (bits[inx]=='1') {
				fprintf( fout, "\tE 0.000000 %0.6f 270.0\n",
						offset );
				fprintf( fout, "\tS 0 0 0.000000 %0.6f %0.6f %0.6f\n",
						offset, (width-width2)/2.0, offset );
			}
			offset += length2;
		}
		if ((cp=getLine())==NULL)
			return;
		if ((rc=sscanf( line, "%lf %lf %s", &length2, &offset, bits ) ) != 3) {
			fprintf( stderr, "syntax error: %d: %s\n", lineCount, line );
			flushInput();
			return;
		}
		offset = getDim( offset );
		length2 = getDim( length2 );
		for (inx=0; bits[inx]; inx++) {
			if (bits[inx]=='1') {
				fprintf( fout, "\tE %0.6f %0.6f 90.0\n",
						width, offset );
				fprintf( fout, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n",
						width-(width-width2)/2.0, offset, width, offset );
			}
			offset += length2;
		}
		fprintf( fout, "\tEND\n");
		break;

	case ACT_ENDTRANSFERTABLE:
		break;

	case ACT_TRACK:
		reset( tp, args );
		break;

	case ACT_STRUCTURE:
		reset( tp, args );
		break;

	case ACT_ENDSTRUCTURE:
		fprintf( fout, "STRUCTURE %s \"%s %s\"\n", scale, partNo,  name );
		for (lp=lines; lp<line_p; lp++) {
			switch (lp->type) {
			case 'L':
				fprintf( fout, "\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color,
						X(lp->pos[0].x), X(lp->pos[0].y), X(lp->pos[1].x), X(lp->pos[1].y) );
				break;
			case 'A':
				fprintf( fout, "\tA %ld 0 %0.6f %0.6f %0.6f %0.6f %0.6f\n", color,
						X(lp->radius), X(lp->center.x), X(lp->center.y), X(lp->a0), X(lp->a1) );
				break;
			}
		}
		fprintf( fout, "\tEND\n" );
		break;

	case ACT_FILL_POINT:
		break;

	case ACT_LINE:
		line_p->type = 'L';
		line_p->pos[0].x = getDim(args[0].number);
		line_p->pos[0].y = getDim(args[1].number);
		line_p->pos[1].x = getDim(args[2].number);
		line_p->pos[1].y = getDim(args[3].number);
		line_p++;
		break;

	case ACT_CURVEDLINE:
		line_p->type = 'A';
		pos0.x = getDim(args[0].number);
		pos0.y = getDim(args[1].number);
		line_p->radius = getDim(args[2].number);
		length2 = 2*line_p->radius*sin(D2R(args[3].number/2.0));
		angle = args[3].number/2.0 + args[4].number;
		pos1.x = pos0.x + length2*cos(D2R(angle));
		pos1.y = pos0.y + length2*sin(D2R(angle));
		computeCurve( pos0, pos1, line_p->radius, &line_p->center, &line_p->a0, &line_p->a1 );
		line_p++;
		break;

	case ACT_CIRCLE:
		line_p->type = 'A';
		line_p->center.x = getDim( args[0].number );
		line_p->center.y = getDim( args[1].number );
		line_p->radius = getDim( args[2].number );
		line_p->a0 = 0.0;
		line_p->a1 = 360.0;
		line_p++;
		break;

	case ACT_DESCRIPTIONPOS:
		break;

	case ACT_ARTICLENOPOS:
		break;

	case ACT_CONNECTINGPOINT:
		endPoint_p->pos.x = getDim(args[0].number);
		endPoint_p->pos.y = getDim(args[1].number);
		endPoint_p->a = normalizeAngle( 90.0 - args[2].number );
		endPoint_p++;
		break;

	case ACT_STRAIGHTTRACK:
		seg_p->radius = 0.0;
		seg_p->pos[0].x = getDim(args[0].number);
		seg_p->pos[0].y = getDim(args[1].number);
		seg_p->pos[1].x = getDim(args[2].number);
		seg_p->pos[1].y = getDim(args[3].number);
		seg_p++;
		break;

	case ACT_CURVEDTRACK:
		seg_p->pos[0].x = getDim(args[0].number);
		seg_p->pos[0].y = getDim(args[1].number);
		seg_p->radius = getDim(args[2].number);
		length2 = 2*seg_p->radius*sin(D2R(args[3].number/2.0));
		angle = 90.0-args[4].number - args[3].number/2.0;
		seg_p->pos[1].x = seg_p->pos[0].x + length2*sin(D2R(angle));
		seg_p->pos[1].y = seg_p->pos[0].y + length2*cos(D2R(angle));
		seg_p->radius = - seg_p->radius;
		seg_p++;
		break;

	case ACT_STRAIGHT_BODY:
		seg_p->radius = 0;
		seg_p->pos[0].x = 0.0;
		seg_p->pos[0].y = 0.0;
		seg_p->pos[1].x = getDim(args[0].number);
		seg_p->pos[1].y = 0.0;
		endPoint_p->pos = seg_p->pos[0];
		endPoint_p->a = 270.0;
		endPoint_p++;
		endPoint_p->pos = seg_p->pos[1];
		endPoint_p->a = 90.0;
		endPoint_p++;
		seg_p++;
		break;

	case ACT_CURVE_BODY:
		if (endPoint_p == endPoints) {
			endPoint_p->pos.y = 0.0;
			endPoint_p->pos.x = 0.0;
			endPoint_p->a = 270.0;
			endPoint_p++;
		}
		seg_p->radius = getDim(args[0].number);
		angle = args[1].number;
		seg_p->pos[0].x = getDim(args[2].number);
		seg_p->pos[0].y = 0.0;
		seg_p->pos[1].x = seg_p->pos[0].x + seg_p->radius * sin( D2R( angle ) );
		seg_p->pos[1].y = seg_p->radius * (1-cos( D2R( angle )) );
		if (right || (threeway && (seg_p-segs == 2)) ) {
			seg_p->pos[1].y = - seg_p->pos[1].y;
			angle = - angle;
		} else {
			seg_p->radius = - seg_p->radius;
		}
		endPoint_p->pos = seg_p->pos[1];
		endPoint_p->a = normalizeAngle( 90.0-angle );
		endPoint_p++;
		seg_p++;
		break;

	case ACT_PRICE:
		break;

	}
}


void parse( void )
/* parse a line:
   figure out what it is, read the arguments, call process()
 */
{
	char *cp, *cpp;
	char strings[512], *sp;
	int len;
	tokenDesc_t *tp;
	int tlen;
	arg_t args[10];
	int inx;

	inBody = 0;
	lineCount = 0;
	while ( cp=getLine() ) {
		if (strncasecmp( cp, "INCH", strlen("INCH") ) == 0) {
			inch++;
			continue;
		}
		for ( tp=tokens; tp<&tokens[ sizeof tokens / sizeof *tp ]; tp++ ){
			tlen = strlen(tp->name);
			if ( strncasecmp( cp, tp->name, tlen) != 0 )
				continue;
			if ( cp[tlen] != '\0' && cp[tlen] != ' '  && cp[tlen] != ',' )
				continue;
			if ( (inBody) == (tp->class==CLS_START) ) {
				continue;
			}
			cp += tlen+1;
			if (tp->args)
			for ( inx=0, sp=strings; tp->args[inx]; inx++ ) {
				if (*cp == '\0') {
					fprintf( stderr, "%d: unexpected end of line\n", lineCount );
					goto nextLine;
				}
				switch( tp->args[inx] ) {
				case 'S':
					args[inx].string = sp;
					while (isspace((unsigned char)*cp)) cp++;
					if (*cp != '"') {
						fprintf( stderr, "%d: expected a \": %s\n", lineCount, cp );
						goto nextLine;
					}
					cp++;
					while ( *cp ) {
						if ( *cp != '"' ) {
							*sp++ = *cp++;
						} else if ( cp[1] == '"' ) {
							*sp++ = '"';
							*sp++ = '"';
							cp += 2;
						} else {
							cp++;
							*sp++ = '\0';
							break;
						}
					}
					break;

				case 'N':
					args[inx].number = strtod( cp, &cpp );
					if (cpp == cp) {
						fprintf( stderr, "%d: expected a number: %s\n", lineCount, cp );
						goto nextLine;
					}
					cp = cpp;
					break;

				case 'I':
					args[inx].integer = strtol( cp, &cpp, 10 );
					if (cpp == cp) {
						fprintf( stderr, "%d: expected an integer: %s\n", lineCount, cp );
						goto nextLine;
					}
					cp = cpp;
					break;

				}
			}
			process( tp, args );
			if (tp->class == CLS_START)
				inBody = 1;
			else if (tp->class == CLS_END)
				inBody = 0;
			tp = NULL;
			break;
		}
		if (tp) {
			fprintf( stderr, "%d: Unknown token: %s\n", lineCount, cp );
		}
nextLine:
		;
	}
}


int main ( int argc, char * argv[] )
/* main: handle options, open files */
{
	char * contents = NULL;
	argv++;
	argc--;
	while (argc > 2) {
		if (strcmp(*argv,"-v")==0) {
			verbose++;
			argv++; argc--;
		} else if (strcmp( *argv, "-s" )==0) {
			argv++; argc--;
			scale = *argv;
			argv++; argc--;
		} else if (strcmp( *argv, "-c" )==0) {
			argv++; argc--;
			contents = *argv;
			argv++; argc--;
		} else if (strcmp( *argv, "-k" )==0) {
			argv++; argc--;
			color = strtol(*argv, NULL, 16);
			argv++; argc--;
		}
	}
	if (argc < 2) {
		fprintf( stderr, helpStr );
		exit(1);
	}
	if (scale == NULL) {
		fprintf( stderr, "scale must be defined\n" );
		exit(1);
	}

	if ( strcmp( argv[0], argv[1] ) == 0 ) {
		fprintf( stderr, "Input and output file names are the same!" );
		exit(1);
	}

	fin = fopen( *argv, "r" );
	if (fin == NULL) {
		perror(*argv);
		exit(1);
	}
	argv++;
	fout = fopen( *argv, "w" );
	if (fout == NULL) {
		perror(*argv);
		exit(1);
	}
	if (contents)
		fprintf( fout, "CONTENTS %s\n", contents );
	parse();
}
