/** \file compound.h
 * Definitions and function prototypes for complex elements (eg. turnouts)
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

#ifndef COMPOUND_H
#define COMPOUND_H

#include "common.h"
#include "track.h" //- PATHPTR_T drawLineType_e

typedef enum { TOnormal, TOadjustable, TOpierInfo, TOpier, TOcarDesc, TOlast } TOspecial_e;

typedef struct {
		char * name;
		FLOAT_T height;
		} pierInfo_t;
typedef union {
			struct {
				FLOAT_T minD, maxD;
			} adjustable;
			struct {
				int cnt;
				pierInfo_t * info;
			} pierInfo;
			struct {
				FLOAT_T height;
				char * name;
			} pier;
		} turnoutInfo_u;
		
typedef struct turnoutInfo_t{
		SCALEINX_T scaleInx;
		char * title;
		coOrd orig;
		coOrd size;
		wIndex_t segCnt;
		trkSeg_p segs;
		wIndex_t endCnt;
		trkEndPt_p endPt;
		PATHPTR_T paths;
		int paramFileIndex;
		char * customInfo;
		DIST_T barScale;
		TOspecial_e special;
		turnoutInfo_u u;
		wBool_t pathOverRide;
		wBool_t pathNoCombine;
		char * contentsLabel;
		} turnoutInfo_t;


#define xtitle(X) \
		(X->title)

typedef struct extraDataCompound_t {
		extraDataBase_t base;
		coOrd orig;
		ANGLE_T angle;
		BOOL_T handlaid;
		BOOL_T flipped;
		BOOL_T ungrouped;
		BOOL_T split;
		BOOL_T pathOverRide;
		BOOL_T pathNoCombine;
		coOrd descriptionOrig;
		coOrd descriptionOff;
		coOrd descriptionSize;
		char * title;
		char * customInfo;
		TOspecial_e special;
		turnoutInfo_u u;
		PATHPTR_T paths;
		PATHPTR_T currPath;
		long currPathIndex;
		wIndex_t segCnt;
		trkSeg_p segs;
		drawLineType_e lineType;
		} extraDataCompound_t;

extern DIST_T curBarScale;
extern dynArr_t turnoutInfo_da;
extern dynArr_t structureInfo_da;
extern dynArr_t carDescInfo_da;
#define turnoutInfo(N) DYNARR_N( turnoutInfo_t *, turnoutInfo_da, N )
#define structureInfo(N) DYNARR_N( turnoutInfo_t *, structureInfo_da, N )
extern turnoutInfo_t * curTurnout;
extern turnoutInfo_t * curStructure;


#define ADJUSTABLE "adjustable"
#define PIER "pier"
#define CURVED "curvedends"

#define COMPOUND_OPTION_HANDLAID	(0x0008)
#define COMPOUND_OPTION_FLIPPED		(0x0010)
#define COMPOUND_OPTION_UNGROUPED	(0x0020)
#define COMPOUND_OPTION_SPLIT		(0x0040)
#define COMPOUND_OPTION_HIDEDESC	(0x0080)
#define COMPOUND_OPTION_PATH_OVERRIDE	(0x0100)
#define COMPOUND_OPTION_PATH_NOCOMBINE	(0x0200)


/* compound.c */
PATHPTR_T GetPaths( track_p trk );
wIndex_t GetPathsLength( PATHPTR_T paths );
void SetPaths( track_p trk, PATHPTR_T paths );
PATHPTR_T GetCurrPath( track_p trk );
long GetCurrPathIndex( track_p trk );
void SetCurrPathIndex( track_p trk, long position );
PATHPTR_T GetParamPaths( turnoutInfo_t * to );
void SetParamPaths( turnoutInfo_t * to, PATHPTR_T paths );

#define FIND_TURNOUT	(1<<11)
#define FIND_STRUCT		(1<<12)
void FormatCompoundTitle( long, char *); 
BOOL_T WriteCompoundPathsEndPtsSegs( FILE *, PATHPTR_T, wIndex_t, trkSeg_p, EPINX_T, trkEndPt_p);
void ParseCompoundTitle( char *, char **, int *, char **, int *, char **, int * );
void FormatCompoundTitle( long, char *);
void ComputeCompoundBoundingBox( track_p);
turnoutInfo_t * FindCompound( long, char *, char * );
char * CompoundGetTitle( turnoutInfo_t * );
void CompoundListLoadData( wList_p, turnoutInfo_t *, long );
void CompoundClearDemoDefns( void );
void SetDescriptionOrig( track_p );
void DrawCompoundDescription( track_p, drawCmd_p, wDrawColor );
DIST_T DistanceCompound( track_p, coOrd * );
void DescribeCompound( track_p, char *, CSIZE_T );
void DeleteCompound( track_p );
track_p NewCompound( TRKTYP_T, TRKINX_T, coOrd, ANGLE_T, char *, EPINX_T, trkEndPt_p, PATHPTR_T, wIndex_t, trkSeg_p );
BOOL_T WriteCompound( track_p, FILE * );
BOOL_T ReadCompound( char *, TRKTYP_T );
void MoveCompound( track_p, coOrd );
void RotateCompound( track_p, coOrd, ANGLE_T );
void RescaleCompound( track_p, FLOAT_T );
void FlipCompound( track_p, coOrd, ANGLE_T );
BOOL_T EnumerateCompound( track_p );
void SetCompoundLineType( track_p trk, int width );

/* cgroup.c */
void UngroupCompound( track_p );
void DoUngroup( void * unused );
void DoGroup( void * unused );

/* dcmpnd.c */
void UpdateTitleMark( char *, SCALEINX_T );
void DoUpdateTitles( void );
BOOL_T RefreshCompound( track_p, BOOL_T );
wIndex_t FindListItemByContext( wList_p, void *);


/* cturnout.c */
extern long curTurnoutEp;
EPINX_T TurnoutPickEndPt( coOrd p, track_p );
BOOL_T SplitTurnoutCheck(track_p,coOrd,EPINX_T ep,track_p *,EPINX_T *,EPINX_T *,BOOL_T check, coOrd *, ANGLE_T *);
void GetSegInxEP( signed char, int *, EPINX_T * );
void SetSegInxEP( signed char *, int, EPINX_T) ;
wIndex_t CheckPaths( wIndex_t, trkSeg_p, PATHPTR_T, char* );
turnoutInfo_t * CreateNewTurnout( char *, char *, wIndex_t, trkSeg_p, PATHPTR_T, EPINX_T, trkEndPt_p, wBool_t, long );
void DeleteTurnoutParams(int fileInx);
turnoutInfo_t * TurnoutAdd( long, SCALEINX_T, wList_p, coOrd *, EPINX_T );
STATUS_T CmdTurnoutAction( wAction_t, coOrd );
BOOL_T ConnectAdjustableTracks( track_p trk1, EPINX_T ep1, track_p trk2, EPINX_T ep2 );
track_p NewHandLaidTurnout( coOrd, ANGLE_T, coOrd, ANGLE_T, coOrd, ANGLE_T, ANGLE_T );
void NextTurnoutPosition( track_p trk );
enum paramFileState	GetTrackCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);
/* ctodesgn.c */
void EditCustomTurnout( turnoutInfo_t *, turnoutInfo_t * );
long ComputeTurnoutRoadbedSide( trkSeg_p, int, int, ANGLE_T, DIST_T );

/* cstruct.c */
turnoutInfo_t * CreateNewStructure( char *, char *, wIndex_t, trkSeg_p, BOOL_T ); 
enum paramFileState	GetStructureCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);
turnoutInfo_t * StructAdd( long, SCALEINX_T, wList_p, coOrd * ); 
STATUS_T CmdStructureAction( wAction_t, coOrd );
BOOL_T StructLoadCarDescList( wList_p );
void DeleteStructures(int fileIndex);


#endif
