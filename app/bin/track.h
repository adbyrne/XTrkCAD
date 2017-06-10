/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/track.h,v 1.3 2009-05-25 18:11:03 m_fischer Exp $
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

#ifndef TRACK_H
#define TRACK_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef WINDOWS
#include <unistd.h>
#endif
#include <math.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <string.h>

#include "wlib.h"
#include "common.h"
#include "utility.h"
#include "draw.h"
#include "misc.h"


extern TRKTYP_T T_NOTRACK;

struct track_t ;
typedef struct track_t * track_p;
typedef struct track_t * track_cp;
extern track_p tempTrack;
extern wIndex_t trackCount;
extern long drawTunnel;
extern long drawEndPtV;
extern long centerDrawMode;
extern wDrawColor selectedColor;
extern wDrawColor normalColor;
extern BOOL_T useCurrentLayer;
extern LAYER_T curTrackLayer;
extern coOrd descriptionOff;
extern DIST_T roadbedWidth;
extern DIST_T roadbedLineWidth;
extern long printCenterLines;
extern long drawElevations;
extern wDrawColor elevColorIgnore;
extern wDrawColor elevColorDefined;
extern DIST_T minTrackRadius;
extern DIST_T maxTrackGrade;
extern wDrawColor exceptionColor;
#define TIEDRAWMODE_NONE		(0)
#define TIEDRAWMODE_OUTLINE		(1)
#define TIEDRAWMODE_SOLID		(2)
extern long tieDrawMode;
extern wDrawColor tieColor;


extern TRKINX_T max_index;

typedef signed char * PATHPTR_T;
extern PATHPTR_T pathPtr;
extern int pathCnt;
extern int pathMax;

extern BOOL_T onTrackInSplit;

typedef enum { curveTypeNone, curveTypeCurve, curveTypeStraight, curveTypeBezier, curveTypeCornu } curveType_e;

#define PARAMS_1ST_JOIN (0)
#define PARAMS_2ND_JOIN (1)
#define PARAMS_EXTEND	(2)
#define PARAMS_PARALLEL (3)
#define PARAMS_BEZIER   (4)
#define PARAMS_CORNU    (5)

typedef struct {
		curveType_e type;
		EPINX_T ep;
		DIST_T len;
		ANGLE_T angle;
		coOrd lineOrig;
		coOrd lineEnd;
		coOrd arcP;
		DIST_T arcR;
		ANGLE_T arcA0, arcA1;
		long helixTurns;
		coOrd bezierPoints[4];
		coOrd cornuEnd[2];
		ANGLE_T cornuAngle[2];
		DIST_T cornuRadius[2];

		} trackParams_t;

#define Q_CANNOT_BE_ON_END				(1)
#define Q_IGNORE_EASEMENT_ON_EXTEND		(2)
#define Q_REFRESH_JOIN_PARAMS_ON_MOVE	(3)
#define Q_CANNOT_PLACE_TURNOUT			(4)
#define Q_DONT_DRAW_ENDPOINT			(5)
#define Q_DRAWENDPTV_1					(6)
#define Q_CAN_PARALLEL					(7)
#define Q_CAN_MODIFYRADIUS				(8)
#define Q_EXCEPTION						(9)
#define Q_CAN_GROUP						(10)
#define Q_FLIP_ENDPTS					(11)
#define Q_CAN_NEXT_POSITION				(12)
#define Q_NODRAWENDPT					(13)
#define Q_ISTRACK						(14)
#define Q_NOT_PLACE_FROGPOINTS			(15)
#define Q_HAS_DESC						(16)
#define Q_MODIFY_REDRAW_DONT_UNDRAW_TRACK (17)
#define Q_CAN_MODIFY_CONTROL_POINTS     (18)	// Is T_BEZIER or T_BEZLIN
#define Q_IS_CORNU						(19)	// Is T_CORNU
#define Q_MODIFY_CANT_SPLIT             (20)	// Is not able to be Split
#define Q_CAN_EXTEND					(21)    // Add extra curve or straight in CORNU MODIFY

typedef struct {
		track_p trk;							// IN Current Track OUT Next Track
		DIST_T length;							// IN How far to go
		DIST_T dist;							// OUT how far left = 0 if found
		coOrd pos;								// IN/OUT - where we are, where we will be						// IN/OUT - where we are now
		ANGLE_T angle;							// IN/OUT - angle now
		} traverseTrack_t, *traverseTrack_p;


typedef struct {
		char * name;
		void (*draw)( track_p, drawCmd_p, wDrawColor );
		DIST_T (*distance)( track_p, coOrd * );
		void (*describe)( track_p, char * line, CSIZE_T len );
		void (*delete)( track_p );
		BOOL_T (*write)( track_p, FILE * );
		void (*read)( char * );
		void (*move)( track_p, coOrd );
		void (*rotate)( track_p, coOrd, ANGLE_T );
		void (*rescale)( track_p, FLOAT_T );
		BOOL_T (*audit)( track_p, char * );
		ANGLE_T (*getAngle)( track_p, coOrd, EPINX_T *, EPINX_T * );
		BOOL_T (*split)( track_p, coOrd, EPINX_T, track_p *, EPINX_T *, EPINX_T * );
		BOOL_T (*traverse)( traverseTrack_p, DIST_T * );
		BOOL_T (*enumerate)( track_p );
		void (*redraw)( void );
		BOOL_T (*trim)( track_p, EPINX_T, DIST_T );
		BOOL_T (*merge)( track_p, EPINX_T, track_p, EPINX_T );
		STATUS_T (*modify)( track_p, wAction_t, coOrd );
		DIST_T (*getLength)( track_p );
		BOOL_T (*getTrackParams)( int, track_p, coOrd pos, trackParams_t * );
		BOOL_T (*moveEndPt)( track_p *, EPINX_T *, coOrd, DIST_T );
		BOOL_T (*query)( track_p, int );
		void (*ungroup)( track_p );
		void (*flip)( track_p, coOrd, ANGLE_T );
		void (*drawPositionIndicator)( track_p, wDrawColor );
		void (*advancePositionIndicator)( track_p, coOrd, coOrd *, ANGLE_T * );
		BOOL_T (*checkTraverse)( track_p, coOrd );
		BOOL_T (*makeParallel)( track_p, coOrd, DIST_T, track_p *, coOrd *, coOrd * );
		void (*drawDesc)( track_p, drawCmd_p, wDrawColor );
		BOOL_T (*rebuildSegs)(track_p);
		} trackCmd_t;


#define NOELEV (-10000.0)
typedef enum { ELEV_NONE, ELEV_DEF, ELEV_COMP, ELEV_GRADE, ELEV_IGNORE, ELEV_STATION } elevMode_e;
#define ELEV_MASK		0x07
#define ELEV_VISIBLE	0x08
typedef struct {
		int option;
		coOrd doff;
		union {
			DIST_T height;
			char * name;
		} u;
		} elev_t;
#define EPOPT_GAPPED	(1L<<0)
typedef struct {
		coOrd pos;
		ANGLE_T angle;
		TRKINX_T index;
		track_p track;
		elev_t elev;
		long option;
		} trkEndPt_t, * trkEndPt_p;

dynArr_t tempEndPts_da;
#define tempEndPts(N) DYNARR_N( trkEndPt_t, tempEndPts_da, N )


typedef struct {
		char type;
		wDrawColor color;
		DIST_T width;
		dynArr_t bezSegs;       //placed here to avoid overwrites
		union {
			struct {
				coOrd pos[2];
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
				char * string;
			} t;
			struct {
				int cnt;
				coOrd * pts;
				coOrd orig;
				ANGLE_T angle;
			} p;
		} u;
		} trkSeg_t, * trkSeg_p;

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

dynArr_t tempSegs_da;
#define tempSegs(N) DYNARR_N( trkSeg_t, tempSegs_da, N )

char tempSpecial[4096];
char tempCustom[4096];

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
ANGLE_T GetAngleSegs( wIndex_t, trkSeg_p, coOrd *, wIndex_t *, DIST_T *, BOOL_T * );
void RecolorSegs( wIndex_t, trkSeg_p, wDrawColor );

BOOL_T ReadSegs( void );
BOOL_T WriteSegs( FILE * f, wIndex_t segCnt, trkSeg_p segs );
BOOL_T WriteSegsEnd(FILE * f, wIndex_t segCnt, trkSeg_p segs, BOOL_T writeEnd);
typedef union {
		struct {
				coOrd pos;				/* IN the point on track to get to */
				ANGLE_T angle;			/* IN  is the angle that the object starts at */
				DIST_T dist;			/* OUT is how far it is along the track */
				BOOL_T backwards;		/* OUT */
				BOOL_T reverse_seg;		/* OUT */
		} traverse1;		//Find dist between pos and end of track that starts with angle set backwards
		struct {
				EPINX_T segDir;			/* IN Direction to go */
				DIST_T dist;			/* IN/OUT In = distance to go, Out = distance left */
				coOrd pos;				/* OUT = point reached if dist = 0 */
				ANGLE_T angle;			/* OUT = angle at point */
		} traverse2;		//Return distance left (or 0) and angle and pos when going dist from segDir end
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
				coOrd pos1;				/* IN/OUT */
				DIST_T dd;				/* OUT */
		} distance;
		struct {
				track_p trk;			/* OUT */
				EPINX_T ep[2];
		} newTrack;
		struct {
				DIST_T length;
		} length;
		struct {
				coOrd pos;				/* IN */
				DIST_T length[2];		/* OUT */
				trkSeg_t newSeg[2];
		} split;
		struct {
				coOrd pos;				/* IN OUT */
				ANGLE_T angle;			/* OUT */
				BOOL_T negative_radius; /* OUT */
				BOOL_T backwards;		/* OUT */
				DIST_T radius;			/* OUT */
				coOrd center;			/* OUT */
		} getAngle;
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
void StraightSegProc( segProc_e, trkSeg_p, segProcData_p );
void CurveSegProc( segProc_e, trkSeg_p, segProcData_p );
void JointSegProc( segProc_e, trkSeg_p, segProcData_p );
void BezierSegProc( segProc_e, trkSeg_p, segProcData_p );   //Used in Cornu join
void CleanSegs( dynArr_t *);



/* debug.c */
void SetDebug( char * );

#define TB_SELECTED		(1<<0)
#define TB_VISIBLE		(1<<1)
#define TB_PROFILEPATH	(1<<2)
#define TB_ELEVPATH		(1<<3)
#define TB_PROCESSED	(1<<4)
#define TB_SHRTPATH		(1<<5)
#define TB_HIDEDESC		(1<<6)
#define TB_CARATTACHED	(1<<7)
#define TB_TEMPBITS		(TB_PROFILEPATH|TB_PROCESSED)

/* track.c */
#ifdef FASTTRACK
#include "trackx.h"
#define GetTrkIndex( T )		((T)->index)
#define GetTrkType( T )			((T)->type)
#define GetTrkScale( T )		((T)->scale)
#define SetTrkScale( T, S )		(T)->scale = ((char)(S))
/*#define GetTrkSelected( T )	((T)->bits&TB_SELECTED)*/
/*#define GetTrkVisible( T )	((T)->bits&TB_VISIBLE)*/
/*#define SetTrkVisible( T, V ) ((V) ? (T)->bits |= TB_VISIBLE : (T)->bits &= !TB_VISIBLE)*/
#define GetTrkLayer( T )		((T)->layer)
#define SetBoundingBox( T, HI, LO ) \
								(T)->hi.x = (float)(HI).x; (T)->hi.y = (float)(HI).y; (T)->lo.x = (float)(LO).x; (T)->lo.x; (T)->lo.x = (float)(LO).y = (float)(LO).y
#define GetBoundingBox( T, HI, LO ) \
								(HI)->x = (POS_T)(T)->hi.x; (HI)->y = (POS_T)(T)->hi.y; (LO)->x = (POS_T)(T)->lo.x; (LO)->y = (POS_T)(T)->lo.y;
#define GetTrkEndPtCnt( T )		((T)->endCnt)
#define SetTrkEndPoint( T, I, PP, AA ) \
								Assert((T)->endPt[I].track); \
								(T)->endPt[I].pos = PP; \
								(T)->endPt[I].angle = AA
#define GetTrkEndTrk( T, I )	((T)->endPt[I].track)
#define GetTrkEndPos( T, I )	((T)->endPt[I].pos)
#define GetTrkEndPosXY( T, I )	PutDim((T)->endPt[I].pos.x), PutDim((T)->endPt[I].pos.y)
#define GetTrkEndAngle( T, I )	((T)->endPt[I].angle)
#define GetTrkEndOption( T, I ) ((T)->endPt[I].option)
#define SetTrkEndOption( T, I, O )		((T)->endPt[I].option=O)
#define GetTrkExtraData( T )	((T)->extraData)
#define GetTrkWidth( T )		(int)((T)->width)
#define SetTrkWidth( T, W )		(T)->width = (unsigned int)(W)
#define GetTrkBits(T)			((T)->bits)
#define SetTrkBits(T,V)			((T)->bits|=(V))
#define ClrTrkBits(T,V)			((T)->bits&=~(V))
#define IsTrackDeleted(T)		((T)->deleted)
#else
TRKINX_T GetTrkIndex( track_p );
TRKTYP_T GetTrkType( track_p );
SCALEINX_T GetTrkScale( track_p );
void SetTrkScale( track_p, SCALEINX_T );
BOOL_T GetTrkSelected( track_p );
BOOL_T GetTrkVisible( track_p );
void SetTrkVisible( track_p, BOOL_T );
LAYER_T GetTrkLayer( track_p );
void SetBoundingBox( track_p, coOrd, coOrd );
void GetBoundingBox( track_p, coOrd*, coOrd* );
EPINX_T GetTrkEndPtCnt( track_p );
void SetTrkEndPoint( track_p, EPINX_T, coOrd, ANGLE_T );
track_p GetTrkEndTrk( track_p, EPINX_T );
coOrd GetTrkEndPos( track_p, EPINX_T );
#define GetTrkEndPosXY( trk, ep ) PutDim(GetTrkEndPos(trk,ep).x), PutDim(GetTrkEndPos(trk,ep).y)
ANGLE_T GetTrkEndAngle( track_p, EPINX_T );
long GetTrkEndOption( track_p, EPINX_T );
long SetTrkEndOption( track_p, EPINX_T, long );
struct extraData * GetTrkExtraData( track_p );
int GetTrkWidth( track_p );
void SetTrkWidth( track_p, int );
int GetTrkBits( track_p );
int SetTrkBits( track_p, int );
int ClrTrkBits( track_p, int );
BOOL_T IsTrackDeleted( track_p );
#endif

#define GetTrkSelected(T)		(GetTrkBits(T)&TB_SELECTED)
#define GetTrkVisible(T)		(GetTrkBits(T)&TB_VISIBLE)
#define SetTrkVisible(T,V)		((V)?SetTrkBits(T,TB_VISIBLE):ClrTrkBits(T,TB_VISIBLE))
int ClrAllTrkBits( int );

void GetTrkEndElev( track_p trk, EPINX_T e, int *option, DIST_T *height );
void SetTrkEndElev( track_p, EPINX_T, int, DIST_T, char * );
int GetTrkEndElevMode( track_p, EPINX_T );
int GetTrkEndElevUnmaskedMode( track_p, EPINX_T );
DIST_T GetTrkEndElevHeight( track_p, EPINX_T );
char * GetTrkEndElevStation( track_p, EPINX_T );
#define EndPtIsDefinedElev( T, E ) (GetTrkEndElevMode(T,E)==ELEV_DEF)
#define EndPtIsIgnoredElev( T, E ) (GetTrkEndElevMode(T,E)==ELEV_IGNORE)
#define EndPtIsStationElev( T, E ) (GetTrkEndElevMode(T,E)==ELEV_STATION)
void SetTrkElev( track_p, int, DIST_T );
int GetTrkElevMode( track_p );
DIST_T GetTrkElev( track_p trk );
void ClearElevPath( void );
BOOL_T GetTrkOnElevPath( track_p, DIST_T * elev );
void SetTrkLayer( track_p, int );
BOOL_T CheckTrackLayer( track_p );
void CopyAttributes( track_p, track_p );

#define GetTrkGauge( T )		GetScaleTrackGauge(GetTrkScale(T))
#define GetTrkScaleName( T )	GetScaleName(GetTrkScale(T))
void SetTrkEndPtCnt( track_p, EPINX_T );
BOOL_T WriteEndPt( FILE *, track_cp, EPINX_T );
EPINX_T PickEndPoint( coOrd, track_cp );
EPINX_T PickUnconnectedEndPoint( coOrd, track_cp );
EPINX_T PickUnconnectedEndPointSilent( coOrd, track_cp );

void AuditTracks( char *, ... );
void CheckTrackLength( track_cp );
track_p NewTrack( wIndex_t, TRKTYP_T, EPINX_T, CSIZE_T );
void DescribeTrack( track_cp, char *, CSIZE_T );
EPINX_T GetEndPtConnectedToMe( track_p, track_p );
void SetEndPts( track_p, EPINX_T );
BOOL_T DeleteTrack( track_p, BOOL_T );

void MoveTrack( track_p, coOrd );
void RotateTrack( track_p, coOrd, ANGLE_T );
void RescaleTrack( track_p, FLOAT_T, coOrd );
#define GNTignoreIgnore (1<<0)
#define GNTfirstDefined (1<<1)
#define GNTonPath		(1<<2)
EPINX_T GetNextTrk( track_p, EPINX_T, track_p *, EPINX_T *, int );
EPINX_T GetNextTrkOnPath( track_p, EPINX_T );
#define FDE_DEF 0
#define FDE_UDF 1
#define FDE_END 2
int FindDefinedElev( track_p, EPINX_T, int, BOOL_T, DIST_T *, DIST_T *);
BOOL_T ComputeElev( track_p, EPINX_T, BOOL_T, DIST_T *, DIST_T * );

#define DTS_LEFT		(1<<0)
#define DTS_RIGHT		(1<<1)
#define DTS_THICK2		(1<<2)
#define DTS_THICK3		(1<<3)
#define DTS_TIES		(1<<4)
#define DTS_NOCENTER	(1<<5)

void DrawCurvedTies( drawCmd_p, track_p, coOrd, DIST_T, ANGLE_T, ANGLE_T, wDrawColor );
void DrawCurvedTrack( drawCmd_p, coOrd, DIST_T, ANGLE_T, ANGLE_T, coOrd, coOrd, track_p, DIST_T, wDrawColor, long );
void DrawStraightTies( drawCmd_p, track_p, coOrd, coOrd, wDrawColor );
void DrawStraightTrack( drawCmd_p, coOrd, coOrd, ANGLE_T, track_p, DIST_T, wDrawColor, long );

ANGLE_T GetAngleAtPoint( track_p, coOrd, EPINX_T *, EPINX_T * );
DIST_T GetTrkDistance( track_cp, coOrd );
track_p OnTrack( coOrd *, INT_T, BOOL_T );
track_p OnTrack2( coOrd *, INT_T, BOOL_T, BOOL_T );

void ComputeRectBoundingBox( track_p, coOrd, coOrd );
void ComputeBoundingBox( track_p );
void DrawEndPt( drawCmd_p, track_p, EPINX_T, wDrawColor ); 
void DrawEndPt2( drawCmd_p, track_p, EPINX_T, wDrawColor ); 
void DrawEndElev( drawCmd_p, track_p, EPINX_T, wDrawColor );
wDrawColor GetTrkColor( track_p, drawCmd_p );
void DrawTrack( track_cp, drawCmd_p, wDrawColor );
void DrawTracks( drawCmd_p, DIST_T, coOrd, coOrd );
void RedrawLayer( LAYER_T, BOOL_T );
void DrawNewTrack( track_cp );
void DrawOneTrack( track_cp, drawCmd_p );
void UndrawNewTrack( track_cp );
void DrawSelectedTracks( drawCmd_p );
void HilightElevations( BOOL_T );
void HilightSelectedEndPt( BOOL_T, track_p, EPINX_T );
DIST_T EndPtDescriptionDistance( coOrd, track_p, EPINX_T );
STATUS_T EndPtDescriptionMove( track_p, EPINX_T, wAction_t, coOrd );

track_p FindTrack( TRKINX_T );
void ResolveIndex( void );
void RenumberTracks( void );
BOOL_T ReadTrack( char * );
BOOL_T WriteTracks( FILE * );
BOOL_T ExportTracks( FILE * );
void ImportStart( void );
void ImportEnd( void );
void FreeTrack( track_p );
void ClearTracks( void );
BOOL_T TrackIterate( track_p * );

void LoosenTracks( void );

void SaveTrackState( void );
void RestoreTrackState( void );
void SaveCarState( void );
void RestoreCarState( void );
TRKTYP_T InitObject( trackCmd_t* );

void ConnectTracks( track_p, EPINX_T, track_p, EPINX_T );
BOOL_T ReconnectTrack( track_p, EPINX_T, track_p, EPINX_T );
void DisconnectTracks( track_p, EPINX_T, track_p, EPINX_T );
BOOL_T ConnectAbuttingTracks( track_p, EPINX_T, track_p, EPINX_T );
BOOL_T SplitTrack( track_p, coOrd, EPINX_T, track_p *leftover, BOOL_T );
BOOL_T TraverseTrack( traverseTrack_p, DIST_T * );
BOOL_T RemoveTrack( track_p*, EPINX_T*, DIST_T* );
BOOL_T TrimTrack( track_p, EPINX_T, DIST_T );
BOOL_T MergeTracks( track_p, EPINX_T, track_p, EPINX_T );
STATUS_T ExtendStraightFromOrig( track_p, wAction_t, coOrd );
STATUS_T ModifyTrack( track_p, wAction_t, coOrd );
BOOL_T GetTrackParams( int, track_p, coOrd, trackParams_t* );
BOOL_T MoveEndPt( track_p *, EPINX_T *, coOrd, DIST_T );
BOOL_T QueryTrack( track_p, int );
void UngroupTrack( track_p );
BOOL_T IsTrack( track_p );
char * GetTrkTypeName( track_p );
BOOL_T RebuildTrackSegs(track_p);

DIST_T GetFlexLength( track_p, EPINX_T, coOrd * );
void LabelLengths( drawCmd_p, track_p, wDrawColor );
DIST_T GetTrkLength( track_p, EPINX_T, EPINX_T );

void SelectAbove( void );
void SelectBelow( void );

void FlipPoint( coOrd*, coOrd, ANGLE_T );
void FlipTrack( track_p, coOrd, ANGLE_T );

void DrawPositionIndicators( void );
void AdvancePositionIndicator( track_p, coOrd, coOrd *, ANGLE_T * );

BOOL_T MakeParallelTrack( track_p, coOrd, DIST_T, track_p *, coOrd *, coOrd * );

#include "cundo.h"
#include "cselect.h"

/* cmisc.c */
wIndex_t describeCmdInx;
typedef enum { DESC_NULL, DESC_POS, DESC_FLOAT, DESC_ANGLE, DESC_LONG, DESC_COLOR, DESC_DIM, DESC_PIVOT, DESC_LAYER, DESC_STRING, DESC_TEXT, DESC_LIST, DESC_EDITABLELIST } descType;
#define DESC_RO			(1<<0)
#define DESC_IGNORE		(1<<1)
#define DESC_NOREDRAW	(1<<2)
#define DESC_CHANGE		(1<<8)
typedef enum { DESC_PIVOT_FIRST, DESC_PIVOT_MID, DESC_PIVOT_SECOND, DESC_PIVOT_NONE } descPivot_t;
#define DESC_PIVOT_1
typedef struct {
		coOrd pos;
		POS_T ang;
		} descEndPt_t;
typedef struct {
		descType type;
		char * label;
		void * valueP;
		int mode;
		wControl_p control0;
		wControl_p control1;
		wPos_t posy;
		} descData_t, * descData_p; 
typedef void (*descUpdate_t)( track_p, int, descData_p, BOOL_T );
void DoDescribe( char *, track_p, descData_p, descUpdate_t );
void DescribeCancel( void );
BOOL_T UpdateDescStraight( int, int, int, int, int, descData_p, long );

		
/* compound.c */
DIST_T CompoundDescriptionDistance( coOrd, track_p );
STATUS_T CompoundDescriptionMove( track_p, wAction_t, coOrd );

/* elev.c */
#define ELEV_FORK		(3)
#define ELEV_BRANCH		(2)
#define ELEV_ISLAND		(1)
#define ELEV_ALONE		(0)

long oldElevationEvaluation;
EPINX_T GetNextTrkOnPath( track_p trk, EPINX_T ep );
int FindDefinedElev( track_p, EPINX_T, int, BOOL_T, DIST_T *, DIST_T * );
BOOL_T ComputeElev( track_p, EPINX_T, BOOL_T, DIST_T *, DIST_T * );
void RecomputeElevations( void );
void UpdateAllElevations( void );
DIST_T GetElevation( track_p );
void ClrTrkElev( track_p );
void SetTrkElevModes( BOOL_T, track_p, EPINX_T, track_p, EPINX_T );
void UpdateTrkEndElev( track_p, EPINX_T, int, DIST_T, char * );
void DrawTrackElev( track_p, drawCmd_p, BOOL_T );

/* cdraw.c */
track_p MakeDrawFromSeg( coOrd, ANGLE_T, trkSeg_p );
BOOL_T OnTableEdgeEndPt( track_p, coOrd * );
BOOL_T ReadTableEdge( char * );
BOOL_T ReadText( char * );

/* chotbar.c */
extern DIST_T curBarScale;
void InitHotBar( void );
void HideHotBar( void );
void LayoutHotBar( void );
typedef enum { HB_SELECT, HB_DRAW, HB_LISTTITLE, HB_BARTITLE, HB_FULLTITLE } hotBarProc_e;
typedef char * (*hotBarProc_t)( hotBarProc_e, void *, drawCmd_p, coOrd * );
void AddHotBarElement( char *, coOrd, coOrd, BOOL_T, DIST_T, void *, hotBarProc_t );
void HotBarCancel( void );
void AddHotBarTurnouts( void );
void AddHotBarStructures( void );
void AddHotBarCarDesc( void );

/* cblock.c */
void CheckDeleteBlock( track_p t );
void ResolveBlockTrack ( track_p trk );
/* cswitchmotor.c */
void CheckDeleteSwitchmotor( track_p t );
void ResolveSwitchmotorTurnout ( track_p trk );

#endif

