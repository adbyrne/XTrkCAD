/** \file trkseg.h
 * 
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

#ifndef TRKSEG_H
#define TRKSEG_H

#include "common.h"


typedef enum { FREEFORM, RECTANGLE, POLYLINE
} PolyType_e;


typedef struct trkSeg_t {
		char type;
		wDrawColor color;
		DIST_T width;
		dynArr_t bezSegs;       //placed here to avoid overwrites
		union {
			struct {
				coOrd pos[4];
				ANGLE_T angle;
				long option;
			} l;
			struct {
				coOrd pos[4];
				ANGLE_T angle0;
				ANGLE_T angle3;
				DIST_T minRadius;
				DIST_T radius0;
				DIST_T radius3;
				DIST_T length;
			} b;
			struct {
				coOrd center;
				ANGLE_T a0, a1;
				DIST_T radius;
			} c;
			struct {
				coOrd pos;
				ANGLE_T angle;
				DIST_T R, L;
				DIST_T l0, l1;
				unsigned int flip:1;
				unsigned int negate:1;
				unsigned int Scurve:1;
			} j;
			struct {
				coOrd pos;
				ANGLE_T angle;
				wFont_p fontP;
				FONTSIZE_T fontSize;
				BOOL_T boxed;
				char * string;
			} t;
			struct {
				int cnt;
				pts_t * pts;
				coOrd orig;
				ANGLE_T angle;
				PolyType_e polyType;
			} p;
		} u;
		} trkSeg_t;
typedef struct trkSeg_t * trkSeg_p;

#define SEG_STRTRK		('S')
#define SEG_CRVTRK		('C')
#define SEG_BEZTRK      ('W')
#define SEG_STRLIN		('L')
#define SEG_CRVLIN		('A')
#define SEG_BEZLIN      ('H')
#define SEG_JNTTRK		('J')
#define SEG_FILCRCL		('G')
#define SEG_POLY		('Y')
#define SEG_FILPOLY		('F')
#define SEG_TEXT		('Z')
#define SEG_UNCEP		('E')
#define SEG_CONEP		('T')
#define SEG_PATH		('P')
#define SEG_SPEC		('X')
#define SEG_CUST		('U')
#define SEG_DOFF		('D')
#define SEG_BENCH		('B')
#define SEG_DIMLIN		('M')
#define SEG_TBLEDGE		('Q')

#define IsSegTrack( S ) ( (S)->type == SEG_STRTRK || (S)->type == SEG_CRVTRK || (S)->type == SEG_JNTTRK || (S)->type == SEG_BEZTRK)

extern dynArr_t tempSegs_da;

#define tempSegs(N) DYNARR_N( trkSeg_t, tempSegs_da, N )

extern char tempSpecial[4096];
extern char tempCustom[4096];

void ComputeCurvedSeg(
		trkSeg_p s,
		DIST_T radius,
		coOrd p0,
		coOrd p1 );

coOrd GetSegEndPt(
		trkSeg_p segPtr,
		EPINX_T ep,
		BOOL_T bounds,
		ANGLE_T * );

void GetTextBounds( coOrd, ANGLE_T, char *, FONTSIZE_T, coOrd *, coOrd * );
void GetSegBounds( coOrd, ANGLE_T, wIndex_t, trkSeg_p, coOrd *, coOrd * );
void MoveSegs( wIndex_t, trkSeg_p, coOrd );
void RotateSegs( wIndex_t, trkSeg_p, coOrd, ANGLE_T );
void FlipSegs( wIndex_t, trkSeg_p, coOrd, ANGLE_T );
void RescaleSegs( wIndex_t, trkSeg_p, DIST_T, DIST_T, DIST_T );
void CloneFilledDraw( wIndex_t, trkSeg_p, BOOL_T );
void FreeFilledDraw( wIndex_t, trkSeg_p );
DIST_T DistanceSegs( coOrd, ANGLE_T, wIndex_t, trkSeg_p, coOrd *, wIndex_t * );
ANGLE_T GetAngleSegs( wIndex_t, trkSeg_p, coOrd *, wIndex_t *, DIST_T *, BOOL_T *, wIndex_t *, BOOL_T * );
void RecolorSegs( wIndex_t, trkSeg_p, wDrawColor );
BOOL_T ReadSegs( void );
BOOL_T WriteSegs( FILE * f, wIndex_t segCnt, trkSeg_p segs );
BOOL_T WriteSegsEnd(FILE * f, wIndex_t segCnt, trkSeg_p segs, BOOL_T writeEnd);


typedef union {
		struct {
				coOrd pos;				/* IN the point to get to */
				ANGLE_T angle;			/* IN  is the angle that the object starts at (-ve for forwards) */
				DIST_T dist;			/* OUT is how far it is along the track to get to pos*/
				BOOL_T backwards;		/* OUT which way are we going? */
				BOOL_T reverse_seg;		/* OUT the seg is backwards curve */
				BOOL_T negative;		/* OUT the curve is negative */
				int BezSegInx;			/* OUT for Bezier Seg - the segment we are on in Bezier */
				BOOL_T segs_backwards;  /* OUT for Bezier Seg - the direction of the overall Bezier */
		} traverse1;				// Find dist between pos and end of track that starts with angle set backwards
		struct {
				BOOL_T segDir;			/* IN Direction to go in this seg*/
				int BezSegInx;			/* IN for Bezier Seg which element to start with*/
				BOOL_T segs_backwards;  /* IN for Bezier Seg  which way to go down the array*/
				DIST_T dist;			/* IN/OUT In = distance to go, Out = distance left */
				coOrd pos;				/* OUT = point reached if dist = 0 */
				ANGLE_T angle;			/* OUT = angle at point */
		} traverse2;			//Return distance left (or 0) and angle and pos when going dist from segDir end
		struct {
				int first, last;		/* IN */
				ANGLE_T side;
				DIST_T roadbedWidth;
				wDrawWidth rbw;
				coOrd orig;
				ANGLE_T angle;
				wDrawColor color;
				drawCmd_p d;
		} drawRoadbedSide;
		struct {
				coOrd pos1;				/* IN pos to find */
				DIST_T dd;				/* OUT distance from nearest point in seg to input pos */
		} distance;
		struct {
				track_p trk;			/* OUT */
				EPINX_T ep[2];
		} newTrack;
		struct {
				DIST_T length;			/* OUT */
		} length;
		struct {
				coOrd pos;				/* IN */
				DIST_T length[2];		/* OUT */
				trkSeg_t newSeg[2];
		} split;
		struct {
				coOrd pos;				/* IN Pos to find nearest to  - OUT found pos on curve*/
				ANGLE_T angle;			/* OUT angle at pos - (-ve if backwards)*/
				BOOL_T negative_radius; /* OUT Radius <0? */
				BOOL_T backwards;		/* OUT Seg is backwards */
				DIST_T radius;			/* OUT radius at pos */
				coOrd center;			/* OUT center of curvature at pos (0 = straight)*/
				int  bezSegInx;			/* OUT if a bezier proc, the index of the sub segment */
		} getAngle;			// Get pos on seg nearest, angle at that (-ve for forwards)
		} segProcData_t, *segProcData_p;
typedef enum {
		SEGPROC_TRAVERSE1,
		SEGPROC_TRAVERSE2,
		SEGPROC_DRAWROADBEDSIDE,
		SEGPROC_DISTANCE,
		SEGPROC_FLIP,
		SEGPROC_NEWTRACK,
		SEGPROC_LENGTH,
		SEGPROC_SPLIT,
		SEGPROC_GETANGLE
		} segProc_e;

void SegProc( segProc_e, trkSeg_p, segProcData_p );
void DrawDimLine( drawCmd_p, coOrd, coOrd, char *, wFontSize_t, FLOAT_T, wDrawWidth, wDrawColor, long );
void DrawSegs(
		drawCmd_p d,
		coOrd orig,
		ANGLE_T angle,
		trkSeg_p segPtr,
		wIndex_t segCnt,
		DIST_T trackGauge,
		wDrawColor color );
void DrawSegsO(
		drawCmd_p d,
		track_p trk,
		coOrd orig,
		ANGLE_T angle,
		trkSeg_p segPtr,
		wIndex_t segCnt,
		DIST_T trackGauge,
		wDrawColor color,
		long options );


void StraightSegProc( segProc_e, trkSeg_p, segProcData_p );
void CurveSegProc( segProc_e, trkSeg_p, segProcData_p );
void JointSegProc( segProc_e, trkSeg_p, segProcData_p );
void BezierSegProc( segProc_e, trkSeg_p, segProcData_p );   //Used in Cornu join
void CleanSegs( dynArr_t *);
void CopyPoly(trkSeg_p seg_p, wIndex_t segCnt);
wBool_t CompareSegs( trkSeg_p, int, trkSeg_p, int );
#endif
