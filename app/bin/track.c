/** \file track.c
 * Track 
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

#include "ccurve.h"
#include "cjoin.h"
#include "compound.h"
#include "cselect.h"
#include "cstraigh.h"
#include "cundo.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "trackx.h"
#include "trkendpt.h"
#include "misc.h"
#include "ctrain.h"
#include "common-ui.h"
#include "version.h"

#include <inttypes.h>

#include <stdint.h>

#define SLOG_FMT "0x%.12" PRIxPTR

EXPORT char tempSpecial[4096];

/** @logcmd @showrefby track=n track.c */
static int log_track = 0;
/** @logcmd @showrefby endPt=n track.c */
static int log_endPt = 0;
/** @logcmd @showrefby readTracks=n track.c */
static int log_readTracks = 0;
/** @logcmd @showrefby timedrawtracks=n track.c */
static int log_timedrawtracks = 0;

// Enable trkType checks on extraData*_t
#define CHECK_EXTRA_DATA

/*****************************************************************************
 *
 * VARIABLES
 *
 */

#define DRAW_TUNNEL_NONE		(0)

#define CLOSETOTHEEDGE			(10)		/**< minimum distance between paste position and edge of window */

EXPORT wIndex_t trackCount;

EXPORT long drawEndPtV = 2;
EXPORT long drawUnconnectedEndPt = 0;		/**< How do we draw Unconnected EndPts */

EXPORT long centerDrawMode = FALSE;			/**< flag to control drawing of circle centers */


EXPORT signed char * pathPtr;
EXPORT int pathCnt = 0;
EXPORT int pathMax = 0;

static dynArr_t trackCmds_da;
#define trackCmds(N) DYNARR_N( trackCmd_t*, trackCmds_da, N )

EXPORT BOOL_T useCurrentLayer = FALSE;

EXPORT unsigned int curTrackLayer;

EXPORT coOrd descriptionOff;

EXPORT DIST_T roadbedWidth = 0.0;
EXPORT DIST_T roadbedLineWidth = 3.0/BASE_DPI;

//EXPORT DIST_T minTrackRadius;
//EXPORT DIST_T maxTrackGrade = 5.0;

static int suspendElevUpdates = FALSE;

static track_p * importTrack;

EXPORT BOOL_T onTrackInSplit = FALSE;

static BOOL_T inDrawTracks;

EXPORT wBool_t bFreeTrack = FALSE;


/*****************************************************************************
 *
 * 
 *
 */
EXPORT void ActivateTrack( track_cp trk) {
	int inx = GetTrkType(trk);
	if (trackCmds( inx )->activate != NULL)
		trackCmds( inx )->activate (trk);

}


EXPORT void DescribeTrack( track_cp trk, char * str, CSIZE_T len )
{

	trackCmds( GetTrkType(trk) )->describe ( trk, str, len );
	/*epCnt = GetTrkEndPtCnt(trk);
	if (debugTrack >= 2)
		for (ep=0; epCnt; ep++)
			 PrintEndPt( logFile, trk, ep );???*/
}


EXPORT DIST_T GetTrkDistance( track_cp trk, coOrd * pos )
{
	return fabs(trackCmds( GetTrkType(trk) )->distance( trk, pos ));
}

/**
 * Check whether the track passed as parameter is close to an existing piece. Track
 * pieces that aren't visible (in a tunnel or on an invisble layer) can be ignored,
 * depending on flag. If there is a track closeby, the passed track is moved to that 
 * position. This implements the snap feature.
 *
 * \param fp IN/OUT the old and the new position
 * \param complain IN show error message if there is no other piece of track
 * \param track IN 
 * \param ignoreHidden IN decide whether hidden track is ignored or not
 * \return   NULL if there is no track, pointer to track otherwise 
 */

EXPORT track_p OnTrack2( coOrd * fp, BOOL_T complain, BOOL_T track, BOOL_T ignoreHidden, track_p t )
{
	track_p trk;
	DIST_T distance, closestDistance = 1000000;
	track_p closestTrack = NULL;
	coOrd p, closestPos, q0, q1;

	q0 = q1 = * fp;
	q0.x -= 1.0;
	q1.x += 1.0;
	q0.y -= 1.0;
	q1.y += 1.0;
	TRK_ITERATE( trk ) {
		if ( track && !IsTrack(trk) )
			continue;
		if (trk == t) continue;
		if (trk->hi.x < q0.x ||
			trk->lo.x > q1.x ||
			trk->hi.y < q0.y ||
			trk->lo.y > q1.y )
			continue;
		if ( ignoreHidden ) {
			if ( (!GetTrkVisible(trk)) && drawTunnel == DRAW_TUNNEL_NONE)
				continue;
			if ( !GetLayerVisible( GetTrkLayer( trk ) ) )
				continue;
		}
		p = *fp;
		distance = trackCmds( GetTrkType(trk) )->distance( trk, &p );
		if (fabs(distance) <= fabs(closestDistance)) { //Make the last (highest) preferred
			closestDistance = distance;
			closestTrack = trk;
			closestPos = p;
		}
	}
	if (closestTrack && closestDistance <0 ) closestDistance = 0.0;  //Turntable was closest - inside well
	if (closestTrack && ((closestDistance <= mainD.scale*0.25) || (closestDistance <= trackGauge*2.0) )) {
		*fp = closestPos;
		return closestTrack;
	}
	if (complain) {
		ErrorMessage( MSG_PT_IS_NOT_TRK, FormatDistance(fp->x), FormatDistance(fp->y) );
	}
	return NULL;
}

/**
 * Check whether the track passed as parameter is close to an existing piece. Track
 * pieces that aren't visible (in a tunnel or on an invisble layer) are ignored,
 * This function is basically a wrapper function to OnTrack2().
 */


EXPORT track_p OnTrack( coOrd * fp, BOOL_T complain, BOOL_T track )
{
	return OnTrack2( fp, complain, track, TRUE, NULL );
}

EXPORT track_p OnTrackIgnore (coOrd * fp, BOOL_T complain, BOOL_T track, track_p t ) {
	return OnTrack2(fp, complain, track, TRUE, t);
}


EXPORT BOOL_T CheckTrackLayer( track_p trk )
{
	if (GetLayerFrozen( GetTrkLayer( trk ) ) ) {
		ErrorMessage( MSG_CANT_MODIFY_FROZEN_TRK );
		return FALSE;
	} else if (GetLayerModule( GetTrkLayer( trk ) ) ) {
		ErrorMessage( MSG_CANT_MODIFY_MODULE_TRK );
		return FALSE;
	} else
		return TRUE;
}

EXPORT BOOL_T CheckTrackLayerSilent( track_p trk )
{
	if (GetLayerFrozen( GetTrkLayer( trk ) ) ) {
		return FALSE;
	} else if (GetLayerModule( GetTrkLayer( trk ) ) ) {
		return FALSE;
	} else
		return TRUE;
}

/******************************************************************************
 *
 * PARTS LIST
 *
 */


EXPORT void EnumerateTracks( void * unused )
{
	track_p trk;
	TRKINX_T inx;

	enumerateMaxDescLen = (int)strlen("Description");

	BOOL_T content = FALSE;

	TRK_ITERATE( trk ) {
		/* 
		 *	process track piece if none are selected (list all) or if it is one of the
		 *	selected pieces (list only selected )
		 */
		if ((!selectedTrackCount || GetTrkSelected(trk)) && trackCmds(trk->type)->enumerate != NULL) {
			if (trackCmds(trk->type)->enumerate( trk )==TRUE) content = TRUE;
		}
	}

	if (content == FALSE) {
		wBeep();
		if (selectedTrackCount == 0)
			InfoMessage(_("No track or structure pieces are present in layout"));
		else
			InfoMessage(_("No track or structure pieces are selected"));
		return;
	}

	EnumerateStart();

	for (inx=1; inx<trackCmds_da.cnt; inx++)
		if (trackCmds(inx)->enumerate != NULL)
			trackCmds(inx)->enumerate( NULL );

	EnumerateEnd();
}

/*****************************************************************************
 *
 * NOTRACK
 *
 */

static void AbortNoTrack( void )
{
	CHECKMSG( FALSE, ( "No Track Op called" ) );
}

static trackCmd_t notrackCmds = {
		"NOTRACK",
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack,
		(void*)AbortNoTrack };

EXPORT TRKTYP_T InitObject( trackCmd_t * cmds )
{
	DYNARR_APPEND( trackCmd_t*, trackCmds_da, 10 );
	trackCmds(trackCmds_da.cnt-1) = cmds;
	return trackCmds_da.cnt-1;
}


EXPORT TRKTYP_T T_NOTRACK = -1;

EXPORT void InitTrkTrack( void )
{
	T_NOTRACK = InitObject( &notrackCmds );
	log_track = LogFindIndex( "track" );
	log_endPt = LogFindIndex( "endPt" );
	log_readTracks = LogFindIndex( "readTracks" );
	log_timedrawtracks = LogFindIndex( "timedrawtracks" );
}

/*****************************************************************************
 *
 * TRACK FIELD ACCESS
 *
 */



EXPORT TRKINX_T GetTrkIndex( track_p trk )
{
	return trk->index;
}

EXPORT TRKTYP_T GetTrkType( track_p trk )
{
	CHECK( trk->type != T_NOTRACK && !IsTrackDeleted(trk) );
	return trk->type;
}

EXPORT SCALEINX_T GetTrkScale( track_p trk )
{
	if ( trk )
		return (SCALEINX_T)trk->scale;
	return 0;
}

EXPORT void SetTrkScale( track_p trk, SCALEINX_T si )
{
	trk->scale = (char)si;
}

EXPORT unsigned int GetTrkLayer( track_p trk )
{
	return trk->layer;
}

EXPORT tieData_t GetTrkTieData( track_p trk )
{
	if (!GetLayerUseDefault(GetTrkLayer(trk))) {
		return GetLayerTieData(GetTrkLayer(trk));
	}
	return GetScaleTieData(GetTrkScale(trk));
}

EXPORT void SetBoundingBox( track_p trk, coOrd hi, coOrd lo )
{
	trk->hi.x = (float)hi.x;
	trk->hi.y = (float)hi.y;
	trk->lo.x = (float)lo.x;
	trk->lo.y = (float)lo.y;
}


EXPORT void GetBoundingBox( track_p trk, coOrd *hi, coOrd *lo )
{
	hi->x = (POS_T)trk->hi.x;
	hi->y = (POS_T)trk->hi.y;
	lo->x = (POS_T)trk->lo.x;
	lo->y = (POS_T)trk->lo.y;
}

EXPORT EPINX_T GetTrkEndPtCnt( track_cp trk )
{
	return trk->endCnt;
}


EXPORT trkEndPt_p GetTrkEndPt(track_cp trk, EPINX_T ep )
{
	CHECK( ep < GetTrkEndPtCnt(trk) );
	return EndPtIndex( trk->endPt, ep );
}


EXPORT struct extraDataBase_t * GetTrkExtraData( track_cp trk, TRKTYP_T trkType )
{
//printf( "GTXD T%d TY%d\n", GetTrkIndex(trk), trkType );
	if ( IsTrackDeleted(trk) ) {
		// We've been called by FreeTracks() which is called from
		// - ClearTracks to remove all tracks
		// - DoRegression to remove expected track
		// - UndoStart / UndoDelete
		// Anywhere else: needs investigation
		if ( bFreeTrack == FALSE )
			printf( "GetExtraData T%d is deleted!\n", trk->index );
		return trk->extraData;
	}
#ifdef CHECK_EXTRA_DATA
	CHECK( trk->extraData );
	CHECK( trk->type == trk->extraData->trkType );
	CHECK( trkType == T_NOTRACK || trk->type == trkType );
#endif
	return trk->extraData;
}


EXPORT void ResizeExtraData( track_p trk, CSIZE_T newSize )
{
	trk->extraData = MyRealloc( trk->extraData, newSize );
	trk->extraSize = newSize;
}


EXPORT DIST_T GetTrkGauge(
	track_cp trk )
{
	if (trk)
		return GetScaleTrackGauge( GetTrkScale( trk ) );
	else
		return trackGauge;
}

EXPORT int GetTrkWidth( track_p trk )
{
	return (int)trk->width;
}

EXPORT void SetTrkWidth( track_p trk, int width )
{
	trk->width = (unsigned int)width;
}

EXPORT int GetTrkBits( track_p trk )
{
	if (trk)
		return trk->bits;
	else
		return 0;
}

EXPORT int SetTrkBits( track_p trk, int bits )
{
	int oldBits = trk->bits;
	trk->bits |= bits;
	return oldBits;
}

EXPORT int ClrTrkBits( track_p trk, int bits )
{
	int oldBits = trk->bits;
	trk->bits &= ~bits;
	return oldBits;
}

EXPORT BOOL_T IsTrackDeleted( track_p trk )
{
	return trk->deleted;
}


EXPORT void SetTrkEndPtCnt( track_p trk, EPINX_T cnt )
{
	EPINX_T oldCnt = trk->endCnt;
	trk->endCnt = cnt;
	trk->endPt = MyRealloc( trk->endPt, EndPtSize(trk->endCnt) );
	if (oldCnt < cnt) {
		memset( GetTrkEndPt( trk, oldCnt ), 0, EndPtSize(cnt-oldCnt) );
		for ( EPINX_T ep = oldCnt; ep<cnt; ep++ ) {
			// Set .index to -1
			SetEndPtTrack( GetTrkEndPt( trk, ep ), NULL );
		}
	}
}


/**
 * Set the layer for a track. 
 *
 * \param trk IN the layer to change the layer for
 * \param layer IN  the new layer for the track
 */
void SetTrkLayer( track_p trk, int layer )
{
	DecrementLayerObjects(trk->layer);

	if (useCurrentLayer)
		trk->layer = (unsigned int)curLayer;
	else
		trk->layer = (unsigned int)layer;

	IncrementLayerObjects(trk->layer);
}



EXPORT int ClrAllTrkBits( int bits )
{
	return ClrAllTrkBitsRedraw( bits, FALSE );
}


EXPORT int ClrAllTrkBitsRedraw( int bits, wBool_t bRedraw )
{
	track_p trk;
	int cnt = 0;
	TRK_ITERATE( trk ) {
		if (trk->bits&bits) {
			cnt++;
			trk->bits &= ~bits;
			if ( bRedraw )
				DrawNewTrack( trk );
		}
	}
	return cnt;
}



EXPORT void SetTrkElev( track_p trk, int mode, DIST_T elev )
{
	SetTrkBits( trk, TB_ELEVPATH );
	trk->elev = elev;
	trk->elevMode = mode;
	ClrEndPtElevCache( trk->endCnt, trk->endPt );
}


EXPORT int GetTrkElevMode( track_p trk )
{
	return trk->elevMode;
}

EXPORT DIST_T GetTrkElev( track_p trk )
{
	return trk->elev;
}


EXPORT void ClearElevPath( void )
{
	track_p trk;
	TRK_ITERATE( trk ) {
		ClrTrkBits( trk, TB_ELEVPATH );
		trk->elev = 0.0;
	}
}


EXPORT BOOL_T GetTrkOnElevPath( track_p trk, DIST_T * elev )
{
	if (trk->bits&TB_ELEVPATH) {
		if ( elev ) *elev = trk->elev;
		return TRUE;
	} else {
		return FALSE;
	}
}


EXPORT void CopyAttributes( track_p src, track_p dst )
{
	SetTrkScale( dst, GetTrkScale( src ) );
	dst->bits = (dst->bits&TB_HIDEDESC) | (src->bits&~TB_HIDEDESC);
	SetTrkWidth( dst, GetTrkWidth( src ) );
	dst->layer = GetTrkLayer( src );
}

/*****************************************************************************
 *
 * ENDPOINTS
 *
 */


EXPORT EPINX_T PickEndPoint( coOrd p, track_cp trk )
{
	EPINX_T inx, i;
	DIST_T d, dd;
	coOrd pos;
	if (trk->endCnt <= 0)
		return -1;
	if ( onTrackInSplit && trk->endCnt > 2 ) {
		if (GetTrkType(trk) == T_TURNOUT)
			return TurnoutPickEndPt( p, trk );
	}
	d = FindDistance( p, GetEndPtPos( trk->endPt ) );
	inx = 0;
	for ( i=1; i<trk->endCnt; i++ ) {
		pos = GetEndPtPos( EndPtIndex( trk->endPt, i ) );
		dd=FindDistance(p, pos);
		if (dd < d) {
			d = dd;
			inx = i;
		}
	}
	return inx;
}


EXPORT EPINX_T PickUnconnectedEndPoint( coOrd p, track_cp trk )
{
	EPINX_T inx, i;
	DIST_T d=0, dd;
	coOrd pos;
	inx = -1;

	for ( i=0; i<trk->endCnt; i++ ) {
		trkEndPt_p epp = EndPtIndex( trk->endPt, i );
		if (GetEndPtTrack( epp ) == NULL) {
			pos = GetEndPtPos( epp  );
			dd=FindDistance(p, pos);
			if (inx == -1 || dd <= d) {
				d = dd;
				inx = i;
			}
		}
	}

	if (inx == -1)
		ErrorMessage( MSG_NO_UNCONN_EP );
	return inx;
}

EXPORT EPINX_T PickUnconnectedEndPointSilent( coOrd p, track_cp trk )
{
	EPINX_T inx, i;
	DIST_T d=DIST_INF, dd;
	coOrd pos;
	inx = -1;

	for ( i=0; i<trk->endCnt; i++ ) {
		trkEndPt_p epp = EndPtIndex( trk->endPt, i );
		if (GetEndPtTrack( epp ) == NULL) {
			pos = GetEndPtPos( epp );
			dd=FindDistance(p, pos);
			if (inx == -1 || dd <= d) {
				d = dd;
				inx = i;
			}
		}
	}

	return inx;
}



EXPORT EPINX_T GetEndPtConnectedToMe( track_p trk, track_p me )
{
	EPINX_T ep;
	for (ep=0; ep<trk->endCnt; ep++)
		if (GetEndPtTrack( EndPtIndex( trk->endPt, ep ) ) == me)
			return ep;
	return -1;
}

EXPORT EPINX_T GetNearestEndPtConnectedToMe( track_p trk, track_p me, coOrd pos) {
	EPINX_T ep, found = -1;
	DIST_T d = DIST_INF;
	DIST_T dd;
		for (ep=0; ep<trk->endCnt; ep++) {
			trkEndPt_p epp = EndPtIndex( trk->endPt, ep );
			if (GetEndPtTrack( epp ) == me) {
				coOrd pos1 = GetEndPtPos( epp );
				dd = FindDistance(pos, pos1 );
				if (dd<d) {
					found = ep;
					d = dd;
				}
			}
		}
		return found;
}


EXPORT void SetEndPts( track_p trk, EPINX_T cnt )
{
LOG1( log_readTracks, ( "SetEndPts( T%d, %d )\n", trk->index, cnt ) )
	if (cnt > 0 && TempEndPtsCount() != cnt) {
		InputError( "Incorrect number of End Points for track, read %d, expected %d.\n", FALSE, TempEndPtsCount(), cnt );
		return;
	}
	trk->endCnt = TempEndPtsCount();
	if ( trk->endCnt <= 0 ) {
		trk->endPt = NULL;
	} else {
		trk->endPt = (trkEndPt_p)memdup( TempEndPt(0), EndPtSize( trk->endCnt ) );
	}
}


EXPORT void MoveTrack( track_p trk, coOrd orig )
{
	EPINX_T ep;
	for (ep=0; ep<trk->endCnt; ep++) {
		trkEndPt_p epp =  EndPtIndex( trk->endPt, ep );
		coOrd pos = GetEndPtPos( epp );
		pos.x += orig.x;
		pos.y += orig.y;
		SetEndPt( epp, pos, GetEndPtAngle( epp ) );
	}
	trackCmds( trk->type )->move( trk, orig );
}


EXPORT void RotateTrack( track_p trk, coOrd orig, ANGLE_T angle )
{
	EPINX_T ep;
	if ( trackCmds( trk->type )->rotate == NULL )
			return;
	for (ep=0; ep<trk->endCnt; ep++) {
		trkEndPt_p epp = EndPtIndex( trk->endPt, ep );
		coOrd pos = GetEndPtPos( epp );
		Rotate( &pos, orig, angle );
		SetEndPt( epp, pos, NormalizeAngle( GetEndPtAngle(epp) + angle ) );
	}
	trackCmds( trk->type )->rotate( trk, orig, angle );
}


EXPORT void RescaleTrack( track_p trk, FLOAT_T ratio, coOrd shift )
{
	EPINX_T ep;
	if ( trackCmds( trk->type )->rescale == NULL )
		return;
	for (ep=0; ep<trk->endCnt; ep++) {
		trkEndPt_p epp = EndPtIndex( trk->endPt, ep );
		coOrd pos = GetEndPtPos( epp );
		pos.x *= ratio;
		pos.y *= ratio;
		SetEndPt( epp, pos, GetEndPtAngle(epp) );
	}
	trackCmds( trk->type )->rescale( trk, ratio );
	MoveTrack( trk, shift );
}


EXPORT void FlipPoint(
		coOrd * pos,
		coOrd orig,
		ANGLE_T angle )
{
	Rotate( pos, orig, -angle );
	pos->x = 2*orig.x - pos->x;
	Rotate( pos, orig, angle );
}


EXPORT void FlipTrack(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	EPINX_T ep;

	for ( ep=0; ep<trk->endCnt; ep++ ) {
		trkEndPt_p epp = EndPtIndex( trk->endPt, ep );
		coOrd pos1 = GetEndPtPos( epp );
		FlipPoint( &pos1, orig, angle );
		ANGLE_T angle1 = GetEndPtAngle( epp );
		angle1 = NormalizeAngle( 2*angle - angle1 );
		SetEndPt( epp, pos1, angle1 );
	}
	if ( trackCmds(trk->type)->flip )
		trackCmds(trk->type)->flip( trk, orig, angle );
	if ( QueryTrack( trk, Q_FLIP_ENDPTS ) ) {
		SwapEndPts( trk->endPt, 0, 1 );
	}
}


EXPORT EPINX_T GetNextTrk( 
		track_p trk1,
		EPINX_T ep1,
		track_p *Rtrk,
		EPINX_T *Rep,
		int mode )
{
	EPINX_T ep, epCnt = GetTrkEndPtCnt(trk1), epRet=-1;
	track_p trk;

	*Rtrk = NULL;
	*Rep = 0;
	for (ep=0; ep<epCnt; ep++) {
		if (ep==ep1)
			continue;
		trk = GetTrkEndTrk( trk1, ep );
		if (trk==NULL) {
#ifdef LATER
			if (isElev)
				epRet = ep;
#endif
			continue;
		}
		if ( (mode&GNTignoreIgnore) &&
			((GetTrkEndElevMode(trk1,ep) == ELEV_IGNORE)) ) {
			continue;
		}
		if (*Rtrk != NULL)
			return -1;
		*Rtrk = trk;
		*Rep = GetEndPtConnectedToMe( trk, trk1 );
		epRet = ep;
	}
	return epRet;
}

EXPORT BOOL_T MakeParallelTrack(
		track_p trk,
		coOrd pos,
		DIST_T dist,
		DIST_T factor,
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R,
		BOOL_T track)
{
	if ( trackCmds(trk->type)->makeParallel )
		return trackCmds(trk->type)->makeParallel( trk, pos, dist, factor, newTrkR, p0R, p1R, track);
	return FALSE;
}

EXPORT BOOL_T RebuildTrackSegs(
		track_p trk)
{
	if (trackCmds(trk->type)->rebuildSegs)
		return trackCmds(trk->type)->rebuildSegs(trk);
	return FALSE;
}

EXPORT BOOL_T ReplayTrackData(
		track_p trk,
		void * data,
		long length)
{
	if (trackCmds(trk->type)->replayData)
			return trackCmds(trk->type)->replayData(trk,data,length);
		return FALSE;
}

EXPORT BOOL_T StoreTrackData(
		track_p trk,
		void ** data,
		long * length)
{
	if (trackCmds(trk->type)->storeData)
		return trackCmds(trk->type)->storeData(trk,data,length);
			return FALSE;
}



/*****************************************************************************
 *
 * LIST MANAGEMENT
 *
 */


EXPORT track_p to_first = NULL;
EXPORT TRKINX_T max_index = 0;
EXPORT track_p * to_last = &to_first;

EXPORT track_p GetFirstTrack()
{
	return to_first;
}

EXPORT track_p GetNextTrack( track_p trk )
{
	return trk->next;
}



static struct {
		track_p first;
		track_p *last;
		wIndex_t count;
		wIndex_t changed;
		TRKINX_T max_index;
		} savedTrackState;


EXPORT void RenumberTracks( void )
{
	track_p trk;
	max_index = 0;
	for (trk=to_first; trk!=NULL; trk=trk->next) {
		trk->index = ++max_index;
	}
}


EXPORT track_p NewTrack( TRKINX_T index, TRKTYP_T type, EPINX_T endCnt, CSIZE_T extraSize )
{
	track_p trk;
	trk = (track_p ) MyMalloc( sizeof *trk );
	*to_last = trk;
	to_last = &trk->next;
	trk->next = NULL;
	if (index<=0) {
		index = ++max_index;
	} else if (max_index < index) {
		max_index = index;
	}
LOG( log_track, 1, ( "NewTrack( T%d, t%d, E%d, X%ld)\n", index, type, endCnt, extraSize ) )
	trk->index = index;
	trk->type = type;
	trk->layer = curLayer;
	trk->scale = (char)GetLayerScale(curLayer); // (char)GetLayoutCurScale();
	trk->bits = TB_VISIBLE;
	trk->elevMode = ELEV_ALONE;
	trk->elev = 0;
	trk->hi.x = trk->hi.y = trk->lo.x = trk->lo.y = (float)0.0;
	trk->endCnt = endCnt;
	if (endCnt) {
		trk->endPt = (trkEndPt_p)MyMalloc( EndPtSize(endCnt) );
		for ( EPINX_T ep = 0; ep<endCnt; ep++ ) {
			SetEndPtTrack( GetTrkEndPt( trk, ep ), NULL );
		}
	} else {
		trk->endPt = NULL;
	}
	if (extraSize) {
		trk->extraData = (struct extraDataBase_t*)MyMalloc( extraSize );
		trk->extraData->trkType = type;
	} else
		trk->extraData = NULL;
	trk->extraSize = extraSize;
	UndoNew( trk );
	trackCount++;
	IncrementLayerObjects(curLayer);
	InfoCount( trackCount );
	return trk;
}


EXPORT void FreeTrack( track_p trk )
{
	bFreeTrack = TRUE;
	trackCmds(trk->type)->deleteTrk( trk );
	if (trk->endPt)
		MyFree(trk->endPt);
	if (trk->extraData)
		MyFree(trk->extraData);
	MyFree(trk);
	bFreeTrack = FALSE;
}


EXPORT void ClearTracks( void )
{
	track_p curr, next;
	UndoClear();
	ClearNote();
	for (curr = to_first; curr; curr=next) {
		next = curr->next;
		FreeTrack( curr );
	}
	to_first = NULL;
	to_last = &to_first;
	max_index = 0;
	changed = checkPtMark = 0;
	trackCount = 0;
	ClearCars();
	InfoCount( trackCount );
}


EXPORT track_p FindTrack( TRKINX_T index )
{
	track_p trk;
	TRK_ITERATE(trk) {
		if (trk->index == index) return trk;
	}
	return NULL;
}


EXPORT void ResolveIndex( void )
{
	track_p trk;
	EPINX_T ep;
	TRK_ITERATE(trk) {
		LOG (log_track, 1, ( "ResNextTrack( T%d, t%d, E%d, X%ld)\n", trk->index, trk->type, trk->endCnt, trk->extraSize ));
		for (ep=0; ep<trk->endCnt; ep++) {
			trkEndPt_p epp = GetTrkEndPt( trk, ep );
			TRKINX_T index = GetEndPtIndex( epp );
			if (index >= 0) {
				track_p track = FindTrack( index );
				if (track == NULL) {
					int rc = NoticeMessage( MSG_RESOLV_INDEX_BAD_TRK, _("Continue"), _("Quit"), trk->index, ep, index );
					if ( rc != 1 )
						return;
				}
				SetEndPtTrack( epp, track );
			}
		}
                ResolveBlockTrack (trk);
                ResolveSwitchmotorTurnout (trk);
        }
	AuditTracks( "readTracks" );
}


EXPORT BOOL_T DeleteTrack( track_p trk, BOOL_T all )
{
	EPINX_T i, ep2;
	track_p trk2;
LOG( log_track, 4, ( "DeleteTrack(T%d)\n", GetTrkIndex(trk) ) )
	if (all) {
		if (!QueryTrack(trk,Q_CANNOT_BE_ON_END)) {
			for (i=0;i<trk->endCnt;i++) {
				trkEndPt_p epp = EndPtIndex( trk->endPt, i );
				trk2 = GetEndPtTrack(epp);
				if (trk2 != NULL) {
					if (QueryTrack(trk2,Q_CANNOT_BE_ON_END)) {
						DeleteTrack( trk2, FALSE );
					}
				}
			}
		}
	}
	/* If Car, simulate Remove Car -> uncouple and mark deleted (no Undo) */
	if (QueryTrack(trk,Q_ISTRAIN)) {
		trk->deleted = TRUE;
#ifdef LATER
		int dir;
		for (dir=0; dir<2; dir++) {
		    if (GetTrkEndTrk(trk,dir)) {
		    	track_p car = GetTrkEndTrk(trk,dir);
		    	for (int dir2=0;dir2<2; dir2++) {
		    		if (car->endPt[dir2].track == trk) car->endPt[dir2].track = NULL;
		    	}
		        trk->endPt[dir].track = NULL;
		    }
		}
#endif
		return TRUE;
	}
	for (i=0;i<trk->endCnt;i++) {
		trkEndPt_p epp = EndPtIndex( trk->endPt, i );
		if ((trk2=GetEndPtTrack(epp)) != NULL) {
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			DisconnectTracks( trk2, ep2, trk, i );
			if ( QueryTrack(trk,Q_CANNOT_BE_ON_END) )
				UndoJoint( trk2, ep2, trk, i );
		}
	}
    CheckDeleteSwitchmotor( trk );
    CheckDeleteBlock( trk );
    CheckCarTraverse( trk );
	DecrementLayerObjects(trk->layer);
	trackCount--;
	AuditTracks( "deleteTrack T%d", trk->index);
	UndoDelete(trk);					/**< Attention: trk is invalidated during that call */
	InfoCount( trackCount );
	return TRUE;
}

EXPORT void SaveTrackState( void )
{
	savedTrackState.first = to_first;
	savedTrackState.last = to_last;
	savedTrackState.count = trackCount;
	savedTrackState.changed = changed;
	savedTrackState.max_index = max_index;
	to_first = NULL;
	to_last = &to_first;
	trackCount = 0;
	changed = 0;
	max_index = 0;
	SaveCarState();
	InfoCount( trackCount );
}

EXPORT void RestoreTrackState( void )
{
	to_first = savedTrackState.first;
	to_last = savedTrackState.last;
	trackCount = savedTrackState.count;
	changed = savedTrackState.changed;
	max_index = savedTrackState.max_index;
	RestoreCarState();
	InfoCount( trackCount );
}


BOOL_T TrackIterate( track_p * trk )
{
	track_p trk1;
	if (!*trk)
		trk1 = to_first;
	else
		trk1 = (*trk)->next;
	while (trk1 && IsTrackDeleted(trk1))
		trk1 = trk1->next;
	*trk = trk1;
	return trk1 != NULL;
}


/*****************************************************************************
*
* REGRESSION
*
*/

wBool_t IsPosClose( coOrd pos1, coOrd pos2 ) {
	DIST_T d = FindDistance( pos1, pos2 );
	return d < 0.1;
}


wBool_t IsAngleClose( ANGLE_T angle1, ANGLE_T angle2 )
{
	ANGLE_T angle = NormalizeAngle( angle1 - angle2 );
	if (angle > 180)
		angle = 360-angle;
	return angle < 0.01;
}


wBool_t IsDistClose( DIST_T dist1, DIST_T dist2 )
{
	DIST_T dist = fabs( dist1 - dist2 );
	return dist < 0.01;
}

wBool_t IsWidthClose( DIST_T dist1, DIST_T dist2 )
{
	// width is computed by pixels/dpi
	// problem is when widths are computed on platforms with differing dpi
	DIST_T dist = fabs( dist1 - dist2 );
	if ( dist < 0.05 )
		return TRUE;
// TODO: This assumes the demo file was written with DPI=72
//       Note: BASE_DPI is 75 so we fudge on dist (was < 0.01)
	dist1 *= mainD.dpi/BASE_DPI;
	dist = fabs( dist1 - dist2 );
	if ( dist < 0.05 )
		return TRUE;
	return FALSE;
}

wBool_t IsColorClose( wDrawColor color1, wDrawColor color2 )
{
	long rgb1 = wDrawGetRGB( color1 );
	long rgb2 = wDrawGetRGB( color2 );
	int r1 = (rgb1 >> 16) & 0xff;
	int g1 = (rgb1 >> 8) & 0xff;
	int b1 = rgb1 & 0xff;
	int r2 = (rgb2 >> 16) & 0xff;
	int g2 = (rgb2 >> 8) & 0xff;
	int b2 = rgb2 & 0xff;
	long diff = abs(r1-r2) + abs(g1-g2) + abs(b1-b2);
	return (diff < 7);
}

static wBool_t CompareTrack( track_cp trk1, track_cp trk2 )
{
//	wBool_t rc = FALSE;
	if ( trk1 == NULL ) {
		sprintf( message, "Compare: T%d not found\n", trk2->index );
		return FALSE;
	}
	sprintf( message, "Compare T:%d - ", GetTrkIndex(trk1) );
	char * cp = message+strlen(message);
	REGRESS_CHECK_INT( "Type", trk1, trk2, type ) 
	REGRESS_CHECK_INT( "Index", trk1, trk2, index ) 
	REGRESS_CHECK_INT( "Layer", trk1, trk2, layer )
	REGRESS_CHECK_INT( "Scale", trk1, trk2, scale )
	REGRESS_CHECK_INT( "EndPtCnt", trk1, trk2, endCnt )
	char * cq = cp-2;
	for ( int inx=0; inx<GetTrkEndPtCnt( trk1 ); inx++ ) {
		cp = cq;
		sprintf( cp, "EP:%d - ", inx );
		cp += strlen(cp);
		if ( ! CompareEndPt( cp, trk1, trk2, inx ) ) {
			return FALSE;
		}
	}
	if ( trackCmds( GetTrkType( trk1 ) )->compare == NULL )
		return TRUE;
	return trackCmds( GetTrkType( trk1 ) )->compare( trk1, trk2 );
}

EXPORT int CheckRegressionResult( long regressVersion,char * sFileName, wBool_t bQuiet )
{
	wBool_t bWroteActualTracks = FALSE;
	int nFail = 0;
	FILE * fRegression = NULL;
	char * sRegressionFile = NULL;
	track_p to_first_save = to_first;
	track_p* to_last_save = to_last;
	MakeFullpath( &sRegressionFile, workingDir, "xtrkcad.regress", NULL );

	while ( GetNextLine() ) {
		if ( paramLine[0] == '#' )
			continue;
		// Read Expected track
		to_first = NULL;
		to_last = &to_first;
		paramVersion = regressVersion;
		if ( !ReadTrack( paramLine ) ) {
			if ( paramFile == NULL )
				return -1;
			break;
		}
		if ( to_first == NULL ) {
			// Something bad happened
			break;
		}
		track_cp tExpected = to_first;
		to_first = to_first_save;
		// Find corresponding Actual track
		track_cp tActual = FindTrack( GetTrkIndex( tExpected ) );
		strcat( message, "Regression " );
		if ( ! CompareTrack( tActual, tExpected ) ) {
			nFail++;
			// Actual doesn't match Expected
			lprintf( "  FAIL: %s\n", message);
			fRegression = fopen( sRegressionFile, "a" );
			if ( fRegression == NULL ) {
				NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Regression"), sRegressionFile, strerror(errno) );
				break;
			}
			fprintf( fRegression, "REGRESSION FAIL %d\n",
				PARAMVERSION );
			fprintf( fRegression, "# %s - %d\n", sFileName, paramLineNum );
			fprintf( fRegression, "# %s", message );
			if ( !bWroteActualTracks ) {
				// Print Actual tracks
				fprintf( fRegression, "Actual Tracks\n" );
				paramVersion = PARAMVERSION;
				WriteTracks( fRegression, FALSE );
				bWroteActualTracks = TRUE;
			}
			// Print Expected track
			to_first = tExpected;
			fprintf( fRegression, "Expected Track\n" );
			WriteTracks( fRegression, FALSE );
			fclose( fRegression );
			strcat( message, "Continue test?" );
			if ( ! bQuiet ) {
				int rc = wNoticeEx( NT_ERROR, message, _("Stop"), _("Continue") );
				if ( !rc ) {
					while ( GetNextLine() &&
						strncmp( paramLine, "REGRESSION END", 14 ) != 0 )
						;
					break;
				}
			}
		}
		// Delete Expected track
		to_first = tExpected;
		to_last = &to_first;
		FreeTrack( tExpected );
	}
	to_first = to_first_save;
	to_last = to_last_save;
	if ( strncmp( paramLine, "REGRESSION END", 14 ) != 0 )
		InputError( "Expected REGRESSION END", TRUE );
	return nFail;
}


/*****************************************************************************
*
* LAYER
*
*/

/**
 * @brief Add 1 to track layer numbers that are greater than or equal to New Layer
 * @param newLayer 
*/
EXPORT void TrackInsertLayer( int newLayer ){
	track_p trk;

	TRK_ITERATE( trk ) {
		int layer = GetTrkLayer(trk);
		if (layer >= newLayer) {
			SetTrkLayer(trk, layer + 1);
		}
	}
}

/**
* @brief Subtract 1 from track layer numbers that are greater than Removed Layer
* @param removeLayer 
*/
EXPORT void TrackDeleteLayer( int removeLayer ){
	track_p trk;

	TRK_ITERATE( trk ) {
		int layer = GetTrkLayer(trk);
		if (layer > removeLayer) {
			SetTrkLayer(trk, layer - 1);
		}
	}
}

/*****************************************************************************
 *
 * ABOVE / BELOW
 *
 */

static void ExciseSelectedTracks( track_p * pxtrk, track_p * pltrk )
{
	track_p trk, *ptrk;
	for (ptrk=&to_first; *ptrk!=NULL; ) {
		trk = *ptrk;
		if (!GetTrkSelected(trk)) {
			ptrk = &(*ptrk)->next;
			continue;
		}
		CHECK( !IsTrackDeleted(trk) );
		UndoModify( *ptrk );
		UndoModify( trk );
		*ptrk = trk->next;
		*pltrk = *pxtrk = trk;
		pxtrk = &trk->next;
		trk->next = NULL;
	}
	to_last = ptrk;
}


EXPORT void SelectAbove( void * unused )
{
	track_p xtrk, ltrk;
	if (selectedTrackCount<=0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	UndoStart( _("Move Objects Above"), "above" );
	xtrk = NULL;
	ExciseSelectedTracks( &xtrk, &ltrk );
	if (xtrk) {
		*to_last = xtrk;
		to_last = &ltrk->next;
	}
	UndoEnd();
	DrawSelectedTracks( &mainD, false );
}


EXPORT void SelectBelow( void * unused )
{
	track_p xtrk, ltrk, trk;
	coOrd lo, hi, lowest, highest;
	if (selectedTrackCount<=0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	UndoStart( _("Mode Objects Below"), "below" );
	xtrk = NULL;
	ExciseSelectedTracks( &xtrk, &ltrk );
	if (xtrk) {
		for ( trk=xtrk; trk; trk=trk->next ) {
			if (trk==xtrk) {
				GetBoundingBox( trk, &highest, &lowest );
			} else {
				GetBoundingBox( trk, &hi, &lo );
				if (highest.x < hi.x)
					highest.x = hi.x;
				if (highest.y < hi.y)
					highest.y = hi.y;
				if (lowest.x > lo.x)
					lowest.x = lo.x;
				if (lowest.y > lo.y)
					lowest.y = lo.y;
			}
			ClrTrkBits( trk, TB_SELECTED );
		}
		ltrk->next = to_first;
		to_first = xtrk;
		highest.x -= lowest.x;
		highest.y -= lowest.y;
		DrawTracks( &mainD, 0.0, lowest, highest );
	}
	UndoEnd();
}


#include "bitmaps/top.xpm"
#include "bitmaps/bottom.xpm"

EXPORT void InitCmdAboveBelow( void )
{
	wIcon_p bm_p;
	bm_p = wIconCreatePixMap( top_xpm[iconSize] );
	AddToolbarButton( "cmdAbove", bm_p, IC_SELECTED|IC_POPUP, SelectAbove, NULL );
	bm_p = wIconCreatePixMap( bottom_xpm[iconSize] );
	AddToolbarButton( "cmdBelow", bm_p, IC_SELECTED|IC_POPUP, SelectBelow, NULL );
}

/*****************************************************************************
 *
 * INPUT / OUTPUT
 *
 */


static int bsearchRead = 0;
static trackCmd_t **sortedCmds = NULL;
static int CompareCmds( const void * a, const void * b )
{
	return strcmp( (*(trackCmd_t**)a)->name, (*(trackCmd_t**)b)->name );
}

EXPORT BOOL_T ReadTrack( char * line )
{
	TRKINX_T inx, lo, hi;
	int cmp;
	if (strncmp( paramLine, "TABLEEDGE ", 10 ) == 0) {
		ReadTableEdge( paramLine+10 );
		return TRUE;
	}
	if (strncmp( paramLine, "TEXT ", 5 ) == 0) {
		ReadText( paramLine+5 );
		return TRUE;
	}

if (bsearchRead) {
	if (sortedCmds == NULL) {
		sortedCmds = (trackCmd_t**)MyMalloc( (trackCmds_da.cnt-1) * sizeof *(trackCmd_t*)0 );
		for (inx=1; inx<trackCmds_da.cnt; inx++)
			sortedCmds[inx-1] = trackCmds(inx);
		qsort( sortedCmds, trackCmds_da.cnt-1, sizeof *(trackCmd_t**)0, CompareCmds );
	}

	lo = 0;
	hi = trackCmds_da.cnt-2;
	do {
		inx = (lo+hi)/2;
		cmp = strncmp( line, sortedCmds[inx]->name, strlen(sortedCmds[inx]->name) );
		if (cmp == 0) {
			return sortedCmds[inx]->read(line);
		} else if (cmp < 0) {
			hi = inx-1;
		} else {
			lo = inx+1;
		}
	} while ( lo <= hi );
} else {
	for (inx=1; inx<trackCmds_da.cnt; inx++) {
		 if (strncmp( line, trackCmds(inx)->name, strlen(trackCmds(inx)->name) ) == 0 ) {
			trackCmds(inx)->read( line );
			// Return TRUE means we found the object type and processed it
			// Any errors will be handled by the callee's:
			// Either skip the definition (ReadSegs) or skip the remainder of the file (InputError)
			return TRUE;
		 }
	}
}
	// Object type not found
	return FALSE;
}


EXPORT BOOL_T WriteTracks( FILE * f, wBool_t bFull )
{
	track_p trk;
	BOOL_T rc = TRUE;
	if ( bFull )
		RenumberTracks();
	if ( !bFull )
		bWriteEndPtDirectIndex = TRUE;
	TRK_ITERATE( trk ) {
		rc &= trackCmds(GetTrkType(trk))->write( trk, f );
	}
	bWriteEndPtDirectIndex = FALSE;
	if ( bFull )
		rc &= WriteCars( f );
	return rc;
}



EXPORT void ImportStart( void )
{
	importTrack = to_last;
}




EXPORT void ImportEnd( coOrd offset, wBool_t import, wBool_t inPlace )
{
	track_p to_firstOld;
	wIndex_t trackCountOld;
	track_p trk;
	coOrd pos;
	wDrawPix_t x, y;
	wWinPix_t ww, hh;
	wBool_t offscreen = FALSE;

	double xmin = 0.0;
	double xmax = 0.0;
	double ymin = 0.0;
	double ymax = 0.0;

	// get the current mouse position
	GetMousePosition( &x, &y );
	mainD.Pix2CoOrd( &mainD, x, y, &pos );
	

	// get the size of the drawing area
	wDrawGetSize( mainD.d, &ww, &hh );

	coOrd middle_screen;
	wDrawPix_t mx,my;

	mx = ww/2.0;
	my = hh/2.0;

	mainD.Pix2CoOrd( &mainD, mx, my, &middle_screen );


	for ( trk=*importTrack; trk; trk=trk->next ) {
		CHECK(!IsTrackDeleted(trk)); // Export ignores deleted tracks
		if (trk->hi.y > ymax ) ymax = trk->hi.y;
		if (trk->lo.y < ymin ) ymin = trk->lo.y;
		if (trk->hi.x > xmax ) xmax = trk->hi.x;
		if (trk->lo.x < xmin ) xmin = trk->lo.x;
	}

	coOrd size = {xmax-xmin,ymax-ymin};



	if (import) {
		offset = zero;
	} else if (!inPlace) {
		//If cursor is off drawing area - cursor is in middle screen
		if ((x<LBORDER) || (x>(ww-RBORDER)) || (y<BBORDER) || (y>(hh-TBORDER))) {
			pos.x = middle_screen.x;
			pos.y = middle_screen.y;
		}
		offset.x += pos.x;
		offset.y += pos.y;
	}

	coOrd middle_object;

	middle_object.x = offset.x + (size.x/2);
	middle_object.y = offset.y + (size.y/2);

	wDrawPix_t ox,oy;
	mainD.CoOrd2Pix( &mainD, middle_object, &ox, &oy );

	if ((ox<0) || (ox>ww) || (oy<0) || (oy>hh) ) offscreen = TRUE;

	to_firstOld = to_first;
	to_first = *importTrack;
	trackCountOld = trackCount;
	ResolveIndex();
	to_first = to_firstOld;
	RenumberTracks();

	// move the imported track into place
	for ( trk=*importTrack; trk; trk=trk->next ) {
		CHECK( !IsTrackDeleted(trk) );
		coOrd move;
		move.x = offset.x;
		move.y = offset.y;
		MoveTrack( trk, move );// mainD.orig );
		trk->bits |= TB_SELECTED;
		DrawTrack( trk, &mainD, wDrawColorBlack );
	}
	importTrack = NULL; 
	trackCount = trackCountOld;
	InfoCount( trackCount );
	// Pan screen if needed to center of new
	if (offscreen) {
		panCenter = middle_object;
		PanHere(I2VP(0));
	}
}

/*******
 * Move Selected Tracks to origin zero and write out
 *******/
EXPORT BOOL_T ExportTracks( FILE * f, coOrd * offset )
{
	track_p trk;
	coOrd xlat,orig;
	
	bWriteEndPtExporting = TRUE;
	orig = mapD.size;
	max_index = 0;
	TRK_ITERATE(trk) {
		if ( GetTrkSelected(trk) ) {
			if (QueryTrack(trk,Q_ISTRAIN)) continue; //Don't bother with CARs
			if (trk->lo.x < orig.x)
				orig.x = trk->lo.x;
			if (trk->lo.y < orig.y)
				orig.y = trk->lo.y;
			trk->index = ++max_index;
		}
	}

	*offset = orig;

	xlat.x = - orig.x;
	xlat.y = - orig.y;
	TRK_ITERATE( trk ) {
		if ( GetTrkSelected(trk) ) {
			if (QueryTrack(trk,Q_ISTRAIN)) continue; //Don't bother with CARs
			MoveTrack( trk, xlat );
			trackCmds(GetTrkType(trk))->write( trk, f );
			MoveTrack( trk, orig );
		}
	}
	RenumberTracks();
	bWriteEndPtExporting = FALSE;
	return TRUE;
}

/*******************************************************************************
 *
 * AUDIT
 *
 */


#define SET_BIT( set, bit ) set[bit>>3] |= (1<<(bit&7))
#define BIT_SET( set, bit ) (set[bit>>3] & (1<<(bit&7)))

static FILE * auditFile = NULL;
static BOOL_T auditStop = TRUE;
static int auditCount = 0;
static int auditIgnore = FALSE;

static void AuditDebug( void )
{
}

static void AuditPrint( char * msg )
{
	time_t clock;
	if (auditFile == NULL) {
		char *path;
		MakeFullpath(&path, workingDir, sAuditF, NULL);
		auditFile = fopen( path, "a+" );
		free(path);
		if (auditFile == NULL) {
			NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Audit"), message, strerror(errno) );
			auditIgnore = TRUE;
			return;
		}
		time(&clock);
		fprintf(auditFile,"\n#==== TRACK AUDIT FAILED\n#==== %s", ctime(&clock) );
		fprintf(auditFile,"#==== %s\n\n", msg );
		auditCount = 0;
		auditIgnore = FALSE;
	}
	fprintf(auditFile, "# " );
	fprintf(auditFile, "%s", msg );
	if (auditIgnore)
		return;
	NoticeMessage( MSG_AUDIT_PRINT_MSG, _("Ok"), NULL, msg );
	if (++auditCount>10) {
		if (NoticeMessage( MSG_AUDIT_PRINT_IGNORE, _("Yes"), _("No") ) )
			auditIgnore = TRUE;
		auditCount = 0;
	}
}


EXPORT void CheckTrackLength( track_cp trk )
{
	DIST_T dist;
	
	if (trackCmds(trk->type)->getLength) {
		dist = trackCmds(trk->type)->getLength( trk );
	} else {
		ErrorMessage( MSG_CTL_UNK_TYPE, trk->type );
		return;
	}

	if ( dist < minLength ) {
		ErrorMessage( MSG_CTL_SHORT_TRK, dist );
	}
}


EXPORT void AuditTracks( char * event, ... )
{
	va_list ap;
	static char used[4096];
	wIndex_t i,j;
	track_p trk, tn;
	BOOL_T (*auditCmd)( track_p, char * );
	char msg[STR_SIZE], *msgp;

	va_start( ap, event );
	vsprintf( msg, event, ap );
	va_end( ap );
	msgp = msg+strlen(msg);
	*msgp++ = '\n';

	trackCount = 0;
	for (i=0;i<sizeof used;i++) {
		used[i] = 0;
	}
	if (*to_last) {
		sprintf( msgp, "*to_last is not NULL ("SLOG_FMT")", (uintptr_t)*to_last );
		AuditPrint( msg );
	}
	TRK_ITERATE( trk ) {
		trackCount++;
		if (trk->type == T_NOTRACK) {
			sprintf( msgp, "T%d: type is NOTRACK", trk->index );
			AuditPrint( msg );
			continue;
		}
		if (trk->index > max_index) {
			sprintf( msgp, "T%d: index bigger than max %d\n", trk->index, max_index );
			AuditPrint( msg );
		}
		if ((auditCmd = trackCmds( trk->type )->audit) != NULL) {
			if (!auditCmd( trk, msgp ))
				AuditPrint( msg );
		}
		if (trk->index < 8*sizeof used) {
			if (BIT_SET(used,trk->index)) {
				sprintf( msgp, "T%d: index used again\n", trk->index );
				AuditPrint( msg );
			}
			SET_BIT(used, trk->index);
		}
		for (i=0; i<trk->endCnt; i++) {
			trkEndPt_p epp = EndPtIndex( trk->endPt, i );
			if ( (tn = GetEndPtTrack(epp)) != NULL ) {
				if (IsTrackDeleted(tn)) {
					sprintf( msgp, "T%d[%d]: T%d is deleted\n", trk->index, i, tn->index );
					AuditPrint( msg );
					SetEndPtTrack( epp, NULL );
				} else {
					for (j=0;j<tn->endCnt;j++) {
						if (GetEndPtTrack( EndPtIndex( tn->endPt, j ) ) == trk) {
							goto nextEndPt;
						}
					}
					sprintf( msgp, "T%d[%d]: T%d doesn\'t point back\n", trk->index, i, tn->index );
					AuditPrint( msg );
					SetEndPtTrack( epp, NULL );
				}
			}
nextEndPt:;
		}
		if (!trk->next) {
			if (to_last != &trk->next) {
				sprintf( msgp, "last track (T%d @ "SLOG_FMT") is not to_last ("SLOG_FMT")\n",
						trk->index, (uintptr_t)trk, (uintptr_t)to_last );
				AuditPrint( msg );
			}
		}
	}
	InfoCount( trackCount );
	if (auditFile != NULL) {
		if (auditStop) {
			if (NoticeMessage( MSG_AUDIT_WRITE_FILE, _("Yes"), _("No"))) {
				fprintf( auditFile, "# before undo\n" );
				WriteTracks(auditFile, TRUE);
				Rdump( auditFile );
				if (strcmp("undoUndo",event)==0) {
					fprintf( auditFile, "# failure in undo\n" );
				} else {
					UndoUndo( NULL );
					if ( undoStatus ) {				
						fprintf( auditFile, "# after undo\n" );
						WriteTracks(auditFile, TRUE);
						Rdump( auditFile );
					} else {
						fprintf( auditFile, "# undo stack is empty\n" );
					}
				}
			}
		}
		if (NoticeMessage( MSG_AUDIT_ABORT, _("Yes"), _("No"))) {
			AuditDebug();
			exit(1);
		}
		fclose(auditFile);
		auditFile = NULL;
	}
}


EXPORT void ComputeRectBoundingBox( track_p trk, coOrd p0, coOrd p1 )
{
	trk->lo.x = (float)min(p0.x, p1.x);
	trk->lo.y = (float)min(p0.y, p1.y);
	trk->hi.x = (float)max(p0.x, p1.x);
	trk->hi.y = (float)max(p0.y, p1.y);
}


EXPORT void ComputeBoundingBox( track_p trk )
{
	EPINX_T i;

	CHECK( trk->endCnt > 0 );

	coOrd pos = GetEndPtPos( trk->endPt );
	trk->hi.x = trk->lo.x = (float)pos.x;
	trk->hi.y = trk->lo.y = (float)pos.y;
	for ( i=1; i<trk->endCnt; i++ ) {
		pos = GetEndPtPos( EndPtIndex( trk->endPt, i ) );
		if (pos.x > trk->hi.x)
			trk->hi.x = (float)pos.x;
		if (pos.y > trk->hi.y)
			trk->hi.y = (float)pos.y;
		if (pos.x < trk->lo.x)
			trk->lo.x = (float)pos.x;
		if (pos.y < trk->lo.y)
			trk->lo.y = (float)pos.y;
	}
}


/*****************************************************************************
 *
 * TRACK SPLICING ETC
 *
 */


//static DIST_T distanceEpsilon = 0.0;
//static ANGLE_T angleEpsilon = 0.0;

EXPORT int ConnectTracks( track_p trk0, EPINX_T inx0, track_p trk1, EPINX_T inx1 )
{
	DIST_T d;
	ANGLE_T a;
	coOrd pos0, pos1;
	trkEndPt_p epp0 = EndPtIndex( trk0->endPt, inx0 );
	trkEndPt_p epp1 = EndPtIndex( trk1->endPt, inx1 );

	if (QueryTrack(trk0,Q_ISTRAIN)) {
		if (!QueryTrack(trk1,Q_ISTRAIN)) {
			NoticeMessage( _("Connecting a car to a non-car T%d T%d"), _("Continue"), NULL, GetTrkIndex(trk0), GetTrkIndex(trk1) );
			return -1;
		}
		SetEndPtTrack( epp0, trk1 );
		SetEndPtTrack( epp1, trk0 );
		return 0;
	}

	if ( !IsTrack(trk0) ) {
		NoticeMessage( _("Connecting a non-track(%d) to (%d)"), _("Continue"), NULL, GetTrkIndex(trk0), GetTrkIndex(trk1) );
		return -1;
	}
	if ( !IsTrack(trk1) ) {
		NoticeMessage( _("Connecting a non-track(%d) to (%d)"), _("Continue"), NULL, GetTrkIndex(trk1), GetTrkIndex(trk0) );
		return -1;
	}
	pos0 = GetEndPtPos( epp0 );
	pos1 = GetEndPtPos( epp1 );
LOG( log_track, 3, ( "ConnectTracks( T%d[%d] @ [%0.3f, %0.3f] = T%d[%d] @ [%0.3f %0.3f]\n", trk0->index, inx0, pos0.x, pos0.y, trk1->index, inx1, pos1.x, pos1.y ) )
	d = FindDistance( pos0, pos1 );
	a = fabs(DifferenceBetweenAngles( GetEndPtAngle(epp0),
					  GetEndPtAngle(epp1) + 180.0 ));
	if (d > connectDistance || (a > connectAngle ) ) {
		LOG( log_endPt, 1, ( "connectTracks: T%d[%d] T%d[%d] d=%0.3f a=%0.3f\n",
				trk0->index, inx0, trk1->index, inx1, d, a ) );
		NoticeMessage( MSG_CONNECT_TRK, _("Continue"), NULL, trk0->index, inx0, trk1->index, inx1, d, a );
		return -1; /* Stop connecting out of alignment tracks! */
	}
	UndoModify( trk0 );
	UndoModify( trk1 );
	if (!suspendElevUpdates)
		SetTrkElevModes( TRUE, trk0, inx0, trk1, inx1 );
	SetEndPtTrack( epp0, trk1 );
	SetEndPtTrack( epp1, trk0 );
	AuditTracks( "connectTracks T%d[%d], T%d[%d]", trk0->index, inx0, trk1->index, inx1 );
	return 0;
}


EXPORT void DisconnectTracks( track_p trk1, EPINX_T ep1, track_p trk2, EPINX_T ep2 )
{
	trkEndPt_p epp1 = EndPtIndex( trk1->endPt, ep1 );
	trkEndPt_p epp2 = EndPtIndex( trk2->endPt, ep2 );
	// Check tracks are connected
	CHECK( GetEndPtTrack(epp1) == trk2 );
	CHECK( GetEndPtTrack(epp2) == trk1 );
	if (QueryTrack(trk1,Q_ISTRAIN)) {
		if (!QueryTrack(trk2,Q_ISTRAIN)) {
			NoticeMessage( _("Disconnecting a car from a non-car T%d T%d"), _("Continue"), NULL, GetTrkIndex(trk1), GetTrkIndex(trk2) );
			return;
		}
		SetEndPtTrack( epp1, NULL );
		SetEndPtTrack( epp2, NULL );
		return;
	}

	UndoModify( trk1 );
	UndoModify( trk2 );
	SetEndPtTrack( epp1, NULL );
	SetEndPtTrack( epp2, NULL );
	if (!suspendElevUpdates)
		SetTrkElevModes( FALSE, trk1, ep1, trk2, ep2 );
}


EXPORT BOOL_T ConnectAbuttingTracks(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	DIST_T d;
	ANGLE_T a;
	d = FindDistance( GetTrkEndPos(trk0,ep0),
					  GetTrkEndPos(trk1,ep1 ) );
	a = NormalizeAngle( GetTrkEndAngle(trk0,ep0) -
						GetTrkEndAngle(trk1,ep1) +
						(180.0+connectAngle/2.0) );
	if ( a < connectAngle && 
		 d < connectDistance ) {
		UndoStart( _("Join Abutting Tracks"), "ConnectAbuttingTracks( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
		DrawEndPt( &mainD, trk0, ep0, wDrawColorWhite );
		DrawEndPt( &mainD, trk1, ep1, wDrawColorWhite );
		ConnectTracks( trk0, ep0,
					   trk1, ep1 );
		DrawEndPt( &mainD, trk0, ep0, wDrawColorBlack );
		DrawEndPt( &mainD, trk1, ep1, wDrawColorBlack );
		UndoEnd();
		return TRUE;
	}
	return FALSE;
}


EXPORT ANGLE_T GetAngleAtPoint( track_p trk, coOrd pos, EPINX_T *ep0, EPINX_T *ep1 )
{
	ANGLE_T (*getAngleCmd)( track_p, coOrd, EPINX_T *, EPINX_T * );

	if ((getAngleCmd = trackCmds(trk->type)->getAngle) != NULL)
		return getAngleCmd( trk, pos, ep0, ep1 );
	else {
		NoticeMessage( MSG_GAAP_BAD_TYPE, _("Continue"), NULL, trk->type, trk->index );
		return 0;
	}
}


EXPORT BOOL_T SplitTrack( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, BOOL_T disconnect )
{
	DIST_T d;
	track_p trk0, trk2, trkl;
	EPINX_T epl, ep0, ep1, ep2=-1, epCnt;
	BOOL_T rc;
	BOOL_T (*splitCmd)( track_p, coOrd, EPINX_T, track_p *, EPINX_T *, EPINX_T * );
	coOrd pos0;

	if (!IsTrack(trk)) {
		if ((splitCmd = trackCmds(trk->type)->split) == NULL) return FALSE;
		UndrawNewTrack( trk );
	    UndoModify( trk );
		rc = splitCmd( trk, pos, ep, leftover, &epl, &ep1 );
		if (*leftover) {
			SetTrkLayer(*leftover,GetTrkLayer( trk ));
			DrawNewTrack( *leftover );
		}
		DrawNewTrack( trk );
		return rc;
	}

	trk0 = trk;
	epl = ep;
	epCnt = GetTrkEndPtCnt(trk);
	*leftover = NULL;
LOG( log_track, 2, ( "SplitTrack( T%d[%d], (%0.3f %0.3f)\n", trk->index, ep, pos.x, pos.y ) )

	trkEndPt_p epp = EndPtIndex( trk->endPt, ep );
	pos0 = GetEndPtPos( epp );
	if (((splitCmd = trackCmds(trk->type)->split) == NULL)) {
		if (!(FindDistance( pos0, pos) <= minLength)) {
			ErrorMessage( MSG_CANT_SPLIT_TRK, trackCmds(trk->type)->name );
			return FALSE;
		}
	}
	UndrawNewTrack( trk );
	UndoModify( trk );
	if ((d = FindDistance( pos0, pos )) <= minLength) {
		/* easy: just disconnect */
		trk2 = GetEndPtTrack(epp);
		if (trk2 != NULL) {
			UndrawNewTrack( trk2 );
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			if (ep2 < 0)
				return FALSE;
			DisconnectTracks( trk, ep, trk2, ep2 );
			LOG( log_track, 2, ( "    at endPt with T%d[%d]\n", trk2->index, ep2 ) )
			DrawNewTrack( trk2 );
		} else {
			LOG( log_track, 2, ( "    at endPt (no connection)\n") )
		}
		DrawNewTrack( trk );


	} else if ( epCnt == 2 &&
				(d = FindDistance( GetEndPtPos(EndPtIndex(trk->endPt,1-ep)), pos )) <= minLength) {
		/* easy: just disconnect */
		if ((trk2=GetEndPtTrack(EndPtIndex(trk->endPt,1-ep))) != NULL) {
			UndrawNewTrack( trk2 );
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			if (ep2 < 0)
				return FALSE;
			DisconnectTracks( trk, 1-ep, trk2, ep2 );
			LOG( log_track, 2, ( "    at endPt with T%d[%d]\n", trk2->index, ep2 ) )
			DrawNewTrack( trk2 );

		} else {

			LOG( log_track, 2, ( "    at endPt (no connection)\n") )
		}
		DrawNewTrack( trk );
	} else {
		/* TODO circle's don't have ep's */
		trk2 = GetTrkEndTrk( trk, ep );
		if ( !disconnect )
			suspendElevUpdates = TRUE;
		if (trk2 != NULL) {
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			DisconnectTracks( trk, ep, trk2, ep2 );
		}
		rc = splitCmd( trk, pos, ep, leftover, &epl, &ep1 );
		if (!rc) {
			if ( trk2 != NULL )
				ConnectTracks( trk, ep, trk2, ep2 );
			suspendElevUpdates = FALSE;
			DrawNewTrack( trk );
			return FALSE;
		}
		ClrTrkElev( trk );
		if (*leftover) {
			trkl = *leftover;
			ep0 = epl;
			if ( !disconnect )
				ConnectTracks( trk, ep, trkl, epl );
			ep0 = 1-epl;
			while ( 1 ) {
				CopyAttributes( trk, trkl );
				ClrTrkElev( trkl );
				trk0 = GetTrkEndTrk(trkl,ep0);
				if ( trk0 == NULL || trk0->type == T_TURNOUT )
					break;
				ep0 = 1-GetEndPtConnectedToMe(trk0,trkl);
				trkl = trk0;
			}
			if (trk2)
				ConnectTracks( trkl, ep0, trk2, ep2 );
			LOG( log_track, 2, ( "    midTrack (leftover = T%d)\n", (trkl)->index ) )
		}
		suspendElevUpdates = FALSE;
		DrawNewTrack( trk );
		if (*leftover) {
			trkl = *leftover;
			ep0 = 1-epl;
			while ( 1 ) {
				DrawNewTrack( trkl );
				trk0 = GetTrkEndTrk(trkl,ep0);
				if ( trk0 == NULL || trk0 == trk2 || trk0->type == T_TURNOUT)
					break;
				ep0 = 1-GetEndPtConnectedToMe(trk0,trkl);
				trkl = trk0;
			}
		}
	}
	return TRUE;
}


EXPORT BOOL_T TraverseTrack(
		traverseTrack_p trvTrk,
		DIST_T * distR )
{
	track_p oldTrk;
	EPINX_T ep;

	while ( *distR > 0.0 && trvTrk->trk ) {
		if ( trackCmds((trvTrk->trk)->type)->traverse == NULL )
			return FALSE;
		oldTrk = trvTrk->trk;
		if ( !trackCmds((trvTrk->trk)->type)->traverse( trvTrk, distR ) )
			return FALSE;
		if ( *distR <= 0.0 )
			return TRUE;
		if ( !trvTrk->trk )
			return FALSE;
		ep = GetNearestEndPtConnectedToMe( trvTrk->trk, oldTrk, trvTrk->pos );
		if ( ep != -1 ) {
			trvTrk->pos = GetTrkEndPos( trvTrk->trk, ep );
			trvTrk->angle = NormalizeAngle( GetTrkEndAngle( trvTrk->trk, ep ) + 180.0 );
		}
		if ( trackCmds((trvTrk->trk)->type)->checkTraverse &&
			 !trackCmds((trvTrk->trk)->type)->checkTraverse( trvTrk->trk, trvTrk->pos ) )
			return FALSE;
		trvTrk->length = -1;
		trvTrk->dist = 0.0;

	}
	return TRUE;
}


EXPORT BOOL_T RemoveTrack( track_p * trk, EPINX_T * ep, DIST_T *dist )
{
	DIST_T dist1;
	track_p trk1;
	EPINX_T ep1=-1;
	while ( *dist > 0.0 ) {
		if (trackCmds((*trk)->type)->getLength == NULL)
			return FALSE;
		if (GetTrkEndPtCnt(*trk) != 2)
			return FALSE;
		dist1 = trackCmds((*trk)->type)->getLength(*trk);
		if ( dist1 > *dist )
			break;
		*dist -= dist1;
		trk1 = GetTrkEndTrk( *trk, 1-*ep );
		if (trk1)
			ep1 = GetEndPtConnectedToMe( trk1, *trk );
		DeleteTrack( *trk, FALSE );
		if (!trk1)
			return FALSE;
		*trk = trk1;
		*ep = ep1;
	}
	dist1 = *dist;
	*dist = 0.0;
	return TrimTrack( *trk, *ep, dist1, zero, 0.0, 0.0, zero );
}


EXPORT BOOL_T TrimTrack( track_p trk, EPINX_T ep, DIST_T dist, coOrd pos, ANGLE_T angle, DIST_T radius, coOrd center )
{
	if (trackCmds(trk->type)->trim)
		return trackCmds(trk->type)->trim( trk, ep, dist, pos, angle, radius, center );
	else
		return FALSE;
}


EXPORT BOOL_T MergeTracks( track_p trk0, EPINX_T ep0, track_p trk1, EPINX_T ep1 )
{
	if (trk0->type == trk1->type &&
		trackCmds(trk0->type)->merge)
		return trackCmds(trk0->type)->merge( trk0, ep0, trk1, ep1 );
	else
		return FALSE;
}

EXPORT STATUS_T ExtendTrackFromOrig( track_p trk, wAction_t action, coOrd pos )
{
	static EPINX_T ep;
	static coOrd end_pos;
	static BOOL_T valid;
	DIST_T d;
	track_p trk1;
	trackParams_t params;
	static wBool_t curved;
	static ANGLE_T end_angle;

	switch ( action ) {
	case C_DOWN:
		ep = PickUnconnectedEndPoint( pos, trk );
		if ( ep == -1 )
			return C_ERROR;
		pos = GetTrkEndPos(trk,ep);
		if (!GetTrackParams(PARAMS_CORNU,trk,pos,&params)) return C_ERROR;
		end_pos = pos;
		if (params.type == curveTypeCurve) {
			curved = TRUE;
			tempSegs(0).type = SEG_CRVTRK;
			tempSegs(0).width = 0;
			tempSegs(0).u.c.radius = params.arcR;
			tempSegs(0).u.c.center = params.arcP;
			tempSegs(0).u.c.a0 = FindAngle(params.arcP,GetTrkEndPos(trk,ep));
			tempSegs(0).u.c.a1 = 0;
		} else {
			curved = FALSE;
			tempSegs(0).type = SEG_STRTRK;
			tempSegs(0).width = 0;
			tempSegs(0).u.l.pos[0] = tempSegs(0).u.l.pos[1] = GetTrkEndPos( trk, ep );
		}
		valid = FALSE;
		InfoMessage( _("Drag to change track length") );
		return C_CONTINUE;
		/*no break*/
	case C_MOVE:
		if (curved) {
			//Normalize pos
			PointOnCircle( &pos, tempSegs(0).u.c.center, tempSegs(0).u.c.radius, FindAngle(tempSegs(0).u.c.center,pos) );
			ANGLE_T a = FindAngle(tempSegs(0).u.c.center,pos)-FindAngle(tempSegs(0).u.c.center,end_pos);
			d = fabs(a)*2*M_PI/360*tempSegs(0).u.c.radius;
			if ( d <= minLength ) {
				if (action == C_MOVE)
					ErrorMessage( MSG_TRK_TOO_SHORT, _("Connecting "), PutDim(fabs(minLength-d)) );
				valid = FALSE;
				return C_CONTINUE;
			}
			//Restrict to outside track
			ANGLE_T diff = NormalizeAngle(GetTrkEndAngle(trk, ep)-FindAngle(end_pos,pos));
			if (diff>90.0 && diff<270.0) {
				valid = FALSE;
				tempSegs(0).u.c.a1 = 0;
				tempSegs(0).u.c.a0 = end_angle;
				InfoMessage( _("Inside turnout track"));
				return C_CONTINUE;
			}
			end_angle = GetTrkEndAngle( trk, ep );
			a = FindAngle(tempSegs(0).u.c.center, pos );
			PointOnCircle( &pos, tempSegs(0).u.c.center, tempSegs(0).u.c.radius, a );
			ANGLE_T a2 = FindAngle(tempSegs(0).u.c.center,end_pos);
			if ((end_angle > 180 && (a2>90 && a2 <270))  ||
					(end_angle < 180 && (a2<90 || a2 >270))) {
				tempSegs(0).u.c.a0 = a2;
				tempSegs(0).u.c.a1 = NormalizeAngle(a-a2);
			} else {
				tempSegs(0).u.c.a0 = a;
				tempSegs(0).u.c.a1 = NormalizeAngle(a2-a);
			}
			tempSegs_da.cnt = 1;
			valid = TRUE;
			if (action == C_MOVE)
				InfoMessage( _("Curve: Length=%s Radius=%0.3f Arc=%0.3f"),
						FormatDistance( d ), FormatDistance(tempSegs(0).u.c.radius), PutAngle( tempSegs(0).u.c.a1 ));
			return C_CONTINUE;
		} else {
			d = FindDistance( end_pos, pos );
			valid = TRUE;
			if ( d <= minLength ) {
				if (action == C_MOVE)
					ErrorMessage( MSG_TRK_TOO_SHORT, _("Connecting "), PutDim(fabs(minLength-d)) );
				valid = FALSE;
				return C_CONTINUE;
			}
			ANGLE_T diff = NormalizeAngle(GetTrkEndAngle( trk, ep )-FindAngle(end_pos, pos));
			if (diff>=90.0 && diff<=270.0) {
				valid = FALSE;
				tempSegs(0).u.c.a1 = 0;
				tempSegs(0).u.c.a0 = end_angle;
				InfoMessage( _("Inside turnout track"));
				return C_CONTINUE;
			}
			Translate( &tempSegs(0).u.l.pos[1], tempSegs(0).u.l.pos[0], GetTrkEndAngle( trk, ep ), d );
			tempSegs_da.cnt = 1;
			if (action == C_MOVE)
				InfoMessage( _("Straight: Length=%s Angle=%0.3f"),
						FormatDistance( d ), PutAngle( GetTrkEndAngle( trk, ep ) ) );
		}
		return C_CONTINUE;

	case C_UP:
		if (!valid)
			return C_TERMINATE;
		UndrawNewTrack( trk );
		EPINX_T jp;
		if (curved) {
			trk1 = NewCurvedTrack(tempSegs(0).u.c.center, tempSegs(0).u.c.radius, tempSegs(0).u.c.a0, tempSegs(0).u.c.a1, 0);
			jp = PickUnconnectedEndPoint(end_pos,trk1);
		} else {
			trk1 = NewStraightTrack( tempSegs(0).u.l.pos[0], tempSegs(0).u.l.pos[1] );
			jp = 0;
		}
		CopyAttributes( trk, trk1 );
		ConnectTracks( trk, ep, trk1, jp );
		DrawNewTrack( trk );
		DrawNewTrack( trk1 );
		return C_TERMINATE;

	default:
		;
	}
	return C_ERROR;
}

EXPORT STATUS_T ExtendStraightFromOrig( track_p trk, wAction_t action, coOrd pos )
{
	static EPINX_T ep;
	static BOOL_T valid;
	DIST_T d;
	track_p trk1;

	switch ( action ) {
	case C_DOWN:
		ep = PickUnconnectedEndPoint( pos, trk );
		if ( ep == -1 )
			return C_ERROR;
		tempSegs(0).type = SEG_STRTRK;
		tempSegs(0).width = 0;
		tempSegs(0).u.l.pos[0] = GetTrkEndPos( trk, ep );
		InfoMessage( _("Drag to change track length") );

	case C_MOVE:
		d = FindDistance( tempSegs(0).u.l.pos[0], pos );
		valid = TRUE;
		if ( d <= minLength ) {
			if (action == C_MOVE)
				ErrorMessage( MSG_TRK_TOO_SHORT, _("Connecting "), PutDim(fabs(minLength-d)) );
			valid = FALSE;
			return C_CONTINUE;
		}
		Translate( &tempSegs(0).u.l.pos[1], tempSegs(0).u.l.pos[0], GetTrkEndAngle( trk, ep ), d );
		tempSegs_da.cnt = 1;
		if (action == C_MOVE)
			InfoMessage( _("Straight: Length=%s Angle=%0.3f"),
					FormatDistance( d ), PutAngle( GetTrkEndAngle( trk, ep ) ) );
		return C_CONTINUE;

	case C_UP:
		if (!valid)
			return C_TERMINATE;
		UndrawNewTrack( trk );
		trk1 = NewStraightTrack( tempSegs(0).u.l.pos[0], tempSegs(0).u.l.pos[1] );
		CopyAttributes( trk, trk1 );
		ConnectTracks( trk, ep, trk1, 0 );
		DrawNewTrack( trk );
		DrawNewTrack( trk1 );
		return C_TERMINATE;

	default:
		;
	}
	return C_ERROR;
}


EXPORT STATUS_T ModifyTrack( track_p trk, wAction_t action, coOrd pos )
{
	if ( trackCmds(trk->type)->modify ) {
		ClrTrkElev( trk );
		return trackCmds(trk->type)->modify( trk, action, pos );
	} else {
		return C_TERMINATE;
	}
}


EXPORT BOOL_T GetTrackParams( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	if ( trackCmds(trk->type)->getTrackParams ) {
		return trackCmds(trk->type)->getTrackParams( inx, trk, pos, params );
	} else {
		CHECK( FALSE ); /* CHECKME */
		return FALSE;
	}
}


EXPORT BOOL_T MoveEndPt( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d )
{
	if ( trackCmds((*trk)->type)->moveEndPt ) {
		return trackCmds((*trk)->type)->moveEndPt( trk, ep, pos, d );
	} else {
		ErrorMessage( MSG_MEP_INV_TRK, GetTrkType(*trk) );
		return FALSE;
	}
}


EXPORT BOOL_T QueryTrack( track_p trk, int query )
{
	if ( trackCmds(trk->type)->query ) {
		return trackCmds(trk->type)->query( trk, query );
	} else {
		return FALSE;
	}
}


EXPORT BOOL_T IsTrack( track_p trk )
{
	return ( trk && QueryTrack( trk, Q_ISTRACK ) );
}


EXPORT void UngroupTrack( track_p trk )
{
	if ( trackCmds(trk->type)->ungroup ) {
		trackCmds(trk->type)->ungroup( trk );
	}
}


EXPORT char * GetTrkTypeName( track_p trk )
{
	return trackCmds(trk->type)->name;
}

/*
 * Note Misnomer - this function also gets the normal length - enumerate deals with flextrack length
 */

EXPORT DIST_T GetFlexLength( track_p trk0, EPINX_T ep, coOrd * pos )
{
	track_p trk = trk0, trk1;
	EPINX_T ep1;
	DIST_T d, dd;

	d = 0.0;
	while(1) {
		trk1 = GetTrkEndTrk( trk, ep );
		if (trk1 == NULL)
			break;
		if (trk1 == trk0)
			break;
		ep1 = GetEndPtConnectedToMe( trk1, trk );
		if (ep1 < 0 || ep1 > 1)
			break;
		if (trackCmds(trk1->type)->getLength == NULL)
			break;
		dd = trackCmds(trk1->type)->getLength(trk1);
		if (dd <= 0.0)
			break;
		d += dd;
		trk = trk1;
		ep = 1-ep1;
		if (d>DIST_INF)
			break;
	}
	*pos = GetTrkEndPos( trk, ep );
	return d;
}


EXPORT DIST_T GetTrkLength( track_p trk, EPINX_T ep0, EPINX_T ep1 )
{
	coOrd pos0, pos1;
	DIST_T d;
	if (ep0 == ep1)
		return 0.0;
	else if (trackCmds(trk->type)->getLength != NULL) {
		d = trackCmds(trk->type)->getLength(trk);
		if (ep1==-1)
			d = d/2.0;
		return d;
	} else {
		pos0 = GetTrkEndPos(trk,ep0);
		if (ep1==-1) {
			// Usual case for asking about distance to center of turnout for grades
			if (trk->type==T_TURNOUT) {
				trackParams_t trackParamsData;
				trackParamsData.ep = ep0;
				if (trackCmds(trk->type)->getTrackParams != NULL) {
					//Find distance to centroid of end points * 2 or actual length if epCnt < 3
					trackCmds(trk->type)->getTrackParams(PARAMS_TURNOUT,trk,pos0,&trackParamsData);
					return trackParamsData.len/2.0;
				}
			}
			pos1.x = (trk->hi.x+trk->lo.x)/2.0;
			pos1.y = (trk->hi.y+trk->lo.y)/2.0;
		} else {
			if (trk->type==T_TURNOUT) {
				pos1 = GetTrkEndPos(trk,ep1);
				trackParams_t trackParamsData;
				trackParamsData.ep = ep0;
				if (trackCmds(trk->type)->getTrackParams != NULL) {
					//Find distance via centroid of end points or actual length if epCnt < 3
					trackCmds(trk->type)->getTrackParams(PARAMS_TURNOUT,trk,pos0,&trackParamsData);
					d = trackParamsData.len/2.0;
					trackParamsData.ep = ep1;
					trackCmds(trk->type)->getTrackParams(PARAMS_TURNOUT,trk,pos1,&trackParamsData);
					d += trackParamsData.len/2.0;
					return d;
				}
			}
			pos1 = GetTrkEndPos(trk,ep1);
		}
		pos1.x -= pos0.x;
		pos1.y -= pos0.y;
		Rotate( &pos1, zero, -GetTrkEndAngle(trk,ep0) );
		return fabs(pos1.y);
	}
}
/*#define DRAW_TUNNEL_NONE		(0)*/
#define DRAW_TUNNEL_DASH		(1)
#define DRAW_TUNNEL_SOLID		(2)
EXPORT long drawTunnel = DRAW_TUNNEL_DASH;

/******************************************************************************
 *
 * SIMPLE DRAWING
 *
 */

EXPORT long tieDrawMode = TIEDRAWMODE_SOLID;
EXPORT wDrawColor tieColor;
EXPORT wDrawColor bridgeColor;
EXPORT wDrawColor roadbedColor;

/**
 * Centerline drawing test
 *  
 * \param 		   d    drawing context
 * \return	true for centerline, false if no centerline to draw
 */

EXPORT BOOL_T
hasTrackCenterline( drawCmd_p d )
{
	// for printing, drawing of center line depends on the scale
	if( d->options & DC_CENTERLINE && d->options & DC_PRINT ) {
		if( d->scale <= ( twoRailScale * 2.0 + 1.0 ) / 2.0 ) {
			return true;
		} else {
			return false;
		}
	}

	// all other cases of explicit centerline option (ie. bitmap)
	if( d->options & DC_CENTERLINE ) {
		return true;
	}

	// if zoomed in beyond 1:1 draw centerline when not doing a simple draw
	if( ( d->scale <= 1.0 ) && !( d->options & DC_SIMPLE ) ) {
		return true;
	}

	return false;
}

EXPORT wBool_t 
DoDrawTies( drawCmd_p d, track_cp trk )
{
	DIST_T scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;
	if ( !trk )
		return FALSE;
	if (GetTrkNoTies(trk))
		return FALSE;	//No Ties for this Track
	if ( tieDrawMode == TIEDRAWMODE_NONE )
		return FALSE;
	if ( d == &mapD )
		return FALSE;
	if ( d->scale >= scale2rail )
		return FALSE;
	if ( !(GetTrkVisible(trk) || drawTunnel==DRAW_TUNNEL_SOLID) )
		return FALSE;
	return TRUE;
}

EXPORT void DrawTie(
		drawCmd_p d,
		coOrd pos,
		ANGLE_T angle,
		DIST_T length,
		DIST_T width,
		wDrawColor color,
		BOOL_T solid )
{
	coOrd lo, hi;
	coOrd p[4];
	int t[4];

	length /= 2;
	width /= 2;
	lo = hi = pos;
	lo.x -= length;
	lo.y -= length;
	hi.x += length;
	hi.y += length;
	angle += 90;

	Translate( &p[0], pos, angle, length );
	Translate( &p[1], p[0], angle+90, width );
	Translate( &p[0], p[0], angle-90, width );
	Translate( &p[2], pos, angle+180, length );
	Translate( &p[3], p[2], angle-90, width );
	Translate( &p[2], p[2], angle+90, width );

	for (int i=0;i<4;i++) {
		t[i] = 0;
	}

	if ( d == &mainD ) {
		lo.x -= RBORDER/mainD.dpi*mainD.scale;
		lo.y -= TBORDER/mainD.dpi*mainD.scale;
		hi.x += LBORDER/mainD.dpi*mainD.scale;
		hi.y += BBORDER/mainD.dpi*mainD.scale;
		if ( OFF_D( d->orig, d->size, lo, hi ) )
			return;
	}
	if ( solid ) {
		DrawPoly( d, 4, p, t, color, 0, DRAW_FILL );
	} else {
		DrawPoly( d, 4, p, t, color, 0, DRAW_CLOSED);
	}
}


EXPORT wDrawColor GetTrkColor( track_p trk, drawCmd_p d )
{
	DIST_T len, elev0, elev1;
	ANGLE_T grade = 0.0;

	if ( IsTrack( trk ) && GetTrkEndPtCnt(trk) == 2 ) {
		ComputeElev( trk, 0, FALSE, &elev0, NULL, FALSE );
		len = GetTrkLength( trk, 0, 1 );
		ComputeElev( trk, 1, FALSE, &elev1, NULL, FALSE );
		if (len>0.1)
			grade = fabs( (elev1-elev0)/len)*100.0;
	}
	if ( (d->options&(DC_SIMPLE|DC_SEGTRACK)) != 0 )
		return wDrawColorBlack;
	if ( grade > GetLayerMaxTrackGrade(GetTrkLayer(trk)))
		return exceptionColor;
	if ( QueryTrack( trk, Q_EXCEPTION ) )
		return exceptionColor;
	if ( (d->options&(DC_PRINT)) == 0 ) {
		if (GetTrkBits(trk)&TB_PROFILEPATH)
			return profilePathColor;
		if ((d->options&DC_PRINT)==0 && GetTrkSelected(trk) && d == &tempD)
			return selectedColor;
	}
	if ( (IsTrack(trk)?(colorTrack):(colorDraw)) ) {
		unsigned int iLayer = GetTrkLayer( trk );
		if (GetLayerUseColor( iLayer ) )
			return GetLayerColor( iLayer );
	}
	return wDrawColorBlack;
}


EXPORT void DrawTrack( track_cp trk, drawCmd_p d, wDrawColor color )
{
	TRKTYP_T trkTyp;

	// Hack for WINDOWS
	if ( trk->bits & TB_UNDRAWN )
		return;
	trkTyp = GetTrkType(trk);
	curTrackLayer = GetTrkLayer(trk);
	if (d != &mapD ) {
		if ( (!GetTrkVisible(trk)) ) {
			if ( drawTunnel==DRAW_TUNNEL_NONE )
				return;
			if ( drawTunnel==DRAW_TUNNEL_DASH )
				d->options |= DC_DASH;
		}
		if (color == wDrawColorBlack) {
			color = GetTrkColor( trk, d );
		}
		if (color == wDrawColorPreviewSelected || color == wDrawColorPreviewUnselected ) {
			d->options |= DC_THICK;
		}
	}

	if (d == &mapD && !GetLayerOnMap(curTrackLayer))
		return;
	if ( (IsTrack(trk)?(colorTrack):(colorDraw)) &&
		(d != &mapD) && (color == wDrawColorBlack) )
		if (GetLayerUseColor((unsigned int)curTrackLayer))
			color = GetLayerColor((unsigned int)curTrackLayer);
	trackCmds(trkTyp)->draw( trk, d, color );
	d->options &= ~DC_DASH;

	d->options &= ~DC_THICK;

	DrawTrackElev( trk, d, color!=wDrawColorWhite );
}


static void DrawATrack( track_cp trk, wDrawColor color )
{
	DrawTrack( trk, &mapD, color );
	DrawTrack( trk, &mainD, color );
}


EXPORT void DrawNewTrack( track_cp t )
{
	t->bits &= ~TB_UNDRAWN;
	DrawATrack( t, wDrawColorBlack );
}

EXPORT void UndrawNewTrack( track_cp t )
{
	DrawATrack( t, wDrawColorWhite );
	t->bits |= TB_UNDRAWN;
}

EXPORT int doDrawPositionIndicator = 1;
EXPORT void DrawPositionIndicators( void )
{
	track_p trk;
	coOrd hi, lo;
	if ( !doDrawPositionIndicator )
		return;
	TRK_ITERATE( trk ) {
		if ( trackCmds(trk->type)->drawPositionIndicator ) {
			if ( drawTunnel==DRAW_TUNNEL_NONE && (!GetTrkVisible(trk)) )
				continue;
			GetBoundingBox( trk, &hi, &lo );
			if ( OFF_MAIND( lo, hi ) )
				continue;
			if (!GetLayerVisible( GetTrkLayer(trk) ) )
				continue;
			trackCmds(trk->type)->drawPositionIndicator( trk, selectedColor );
		}
	}
}


EXPORT void AdvancePositionIndicator(
		track_p trk,
		coOrd pos,
		coOrd * posR,
		ANGLE_T * angleR )
{
	if ( trackCmds(trk->type)->advancePositionIndicator )
		trackCmds(trk->type)->advancePositionIndicator( trk, pos, posR, angleR );
}
/*****************************************************************************
 *
 * BASIC DRAWING
 *
 */

static void DrawUnconnectedEndPt( drawCmd_p d, coOrd p, ANGLE_T a, DIST_T trackGauge, wDrawColor color )
{
	coOrd p0, p1;
		Translate( &p0, p, a, trackGauge );
		Translate( &p1, p, a-180.0, trackGauge );
		DrawLine( d, p0, p1, 0, color );
		if (d->scale < 8 || drawUnconnectedEndPt > 0) {
			Translate( &p, p, a+90.0, 0.2 );
			Translate( &p0, p, a, trackGauge );
			Translate( &p1, p, a-180.0, trackGauge );
			DrawLine( d, p0, p1, (drawUnconnectedEndPt>0)?4:0, (color==wDrawColorWhite)?color:(drawUnconnectedEndPt>1)?exceptionColor:color );
		}
}


/**
 * Draw track endpoints. The correct track endpoint (connected, unconnected etc.)
 * is drawn to the track. In case the endpoint is on the transition into a
 * tunnel, a tunnel portal is drawn.
 *
 * \param d IN drawing functions to use (depends on print, draw to screen etc.)
 * \param trk IN track for which endpoints are drawn
 * \param ep IN index of endpoint to draw
 * \param color IN color to use
 */

EXPORT void DrawEndPt(
	drawCmd_p d,
	track_p trk,
	EPINX_T ep,
	wDrawColor color)
{
	coOrd p;
	ANGLE_T a;
	track_p trk1;
	coOrd p0,p1,p2;
	BOOL_T sepBoundary;
	BOOL_T showBridge = 1;
	DIST_T trackGauge;
	wDrawWidth width;
	wDrawWidth width2;

	if((d->options & (DC_SIMPLE | DC_SEGTRACK)) != 0)
		return;
	if(trk && QueryTrack(trk,Q_NODRAWENDPT))
		return;
	if(trk == NULL || ep < 0)
		return;

	// line width for the tunnel portal and bridge parapets, make sure it is rounded correctly
	width2 = (wDrawWidth)round((2.0 * d->dpi) / BASE_DPI);
	if ((d->options&DC_PRINT) && (d->dpi>2*BASE_DPI))
		width2 = (wDrawWidth)round(d->dpi / BASE_DPI);


	if(color == wDrawColorBlack)
		color = normalColor;

	if(((d->options & DC_PRINT) ? (labelScale * 2 + 1) : labelScale) >= d->scale)
		DrawEndElev(d,trk,ep,color);

	trk1 = GetTrkEndTrk(trk,ep);
	p = GetTrkEndPos(trk,ep);
	a = GetTrkEndAngle(trk,ep) + 90.0;

	trackGauge = GetTrkGauge(trk);
	if(trk1 == NULL) {
		DrawUnconnectedEndPt(d,p,a,trackGauge,color);
		return;
	}

	sepBoundary = FALSE;
	if(d->scale < ((d->options & DC_PRINT) ? (twoRailScale * 2 + 1) : twoRailScale))
	{
		// return;

		if(inDrawTracks && (d->options & DC_PRINT) == 0 && importTrack == NULL && GetTrkSelected(trk) && (!GetTrkSelected(trk1))) {
			DIST_T len;
			len = trackGauge * 2.0;
			if(len < 0.10 * d->scale)
				len = 0.10 * d->scale;
			long oldOptions = d->options;
			d->options &= ~DC_NOTSOLIDLINE;
			Translate(&p0,p,a + 45,len);
			Translate(&p1,p,a + 225,len);
			DrawLine(d,p0,p1,2,selectedColor);
			Translate(&p0,p,a - 45,len);
			Translate(&p1,p,a - 225,len);
			DrawLine(d,p0,p1,2,selectedColor);
			d->options = oldOptions;
			sepBoundary = TRUE;
		}
		else if((d->options & DC_PRINT) == 0 && importTrack == NULL && (!GetTrkSelected(trk)) && GetTrkSelected(trk1)) {
			sepBoundary = TRUE;
		}
	}

	// is the endpoint a transition into a tunnel?
	if(GetTrkVisible(trk) && (!GetTrkVisible(trk1))) {
		// yes, draw tunnel portal
		Translate(&p0,p,a,trackGauge);
		Translate(&p1,p,a + 180,trackGauge);
		DrawLine(d,p0,p1,width2,color);
		Translate(&p2,p0,a + 45,trackGauge / 2.0);
		DrawLine(d,p0,p2,width2,color);
		Translate(&p2,p1,a + 135,trackGauge / 2.0);
		DrawLine(d,p1,p2,width2,color);
		if(d == &mainD) {
			width = (wDrawWidth)ceil(trackGauge * d->dpi / 2.0 / d->scale);
			if(width > 1) {
				if((GetTrkEndOption(trk,ep) & EPOPT_GAPPED) != 0) {
					Translate(&p0,p,a,trackGauge);
					DrawLine(d,p0,p,width,color);
				}
				trk1 = GetTrkEndTrk(trk,ep);
				if(trk1) {
					ep = GetEndPtConnectedToMe(trk1,trk);
					if((GetTrkEndOption(trk1,ep) & EPOPT_GAPPED) != 0) {
						Translate(&p0,p,a + 180.0,trackGauge);
						DrawLine(d,p0,p,width,color);
					}
				}
			}
			showBridge = 0;
		}
	}
	else if((!GetTrkVisible(trk)) && GetTrkVisible(trk1)) {
		showBridge = 0;
	}
	else if(GetLayerVisible(GetTrkLayer(trk)) && !GetLayerVisible(GetTrkLayer(trk1))) {
		a -= 90.0;
		Translate(&p,p,a,trackGauge / 2.0);
		Translate(&p0,p,a - 135.0,trackGauge * 2.0);
		DrawLine(d,p0,p,width2,color);
		Translate(&p0,p,a + 135.0,trackGauge * 2.0);
		DrawLine(d,p0,p,width2,color);

		showBridge = 0;
	}
	else if(!GetLayerVisible(GetTrkLayer(trk)) && GetLayerVisible(GetTrkLayer(trk1))) {
		showBridge = 0;
	}
	else if(sepBoundary) {
		;
	}
	else if((drawEndPtV == 1 && (QueryTrack(trk,Q_DRAWENDPTV_1) || QueryTrack(trk1,Q_DRAWENDPTV_1))) || (drawEndPtV == 2)) {
		Translate(&p0,p,a,trackGauge);
		width = 0;
		if(d != &mapD && d != &tempD && (GetTrkEndOption(trk,ep) & EPOPT_GAPPED) != 0)
			width = (wDrawWidth)ceil(trackGauge * d->dpi / 2.0 / d->scale);
		DrawLine(d,p0,p,width,color);
	}
	else {
		;
	}

	if(showBridge && GetTrkBridge(trk) && (!GetTrkBridge(trk1))) {
		Translate(&p0,p,a,trackGauge * 1.5);
		Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
		DrawLine(d,p0,p1,width2,color);
		Translate(&p0,p,a,-trackGauge * 1.5);
		Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
		DrawLine(d,p0,p1,width2,color);
	}
}


EXPORT void DrawEndPt2(
		drawCmd_p d,
		track_p trk,
		EPINX_T ep,
		wDrawColor color )
{
	track_p trk1;
	EPINX_T ep1;
	DrawEndPt( d, trk, ep, color );
	trk1 = GetTrkEndTrk( trk, ep );
	if (trk1) {
		ep1 = GetEndPtConnectedToMe( trk1, trk );
		if (ep1>=0)
			DrawEndPt( d, trk1, ep1, color );
	}
}

EXPORT void DrawTracks( drawCmd_p d, DIST_T scale, coOrd orig, coOrd size )
{
	track_cp trk;
	TRKINX_T inx;
	wIndex_t count = 0;
	coOrd lo, hi;
	BOOL_T doSelectRecount = FALSE;
	unsigned long time0 = wGetTimer();
	
	inDrawTracks = TRUE;
	InfoCount( 0 );

	TRK_ITERATE( trk ) {
		if ( (d->options&DC_PRINT) != 0 &&
			 wPrintQuit() ) {
			inDrawTracks = FALSE;
			return;
		}
		if ( GetTrkSelected(trk) && 
			( (!GetLayerVisible(GetTrkLayer(trk))) ||
			  (drawTunnel==0 && !GetTrkVisible(trk)) ) ) {
			ClrTrkBits( trk, TB_SELECTED );
			doSelectRecount = TRUE;
		}
		GetBoundingBox( trk, &hi, &lo );
		if ( OFF_D( orig, size, lo, hi ) ||
			(d != &mapD && !GetLayerVisible( GetTrkLayer(trk) ) ) ||
			(d == &mapD && !GetLayerOnMap( GetTrkLayer(trk) ) ) )
			continue;
		DrawTrack( trk, d, wDrawColorBlack );
		count++;
		if (count%10 == 0) 
			InfoCount( count );
	}

	if (d == &mainD) {
		for (inx=1; inx<trackCmds_da.cnt; inx++)
			if (trackCmds(inx)->redraw != NULL)
				trackCmds(inx)->redraw();
		LOG( log_timedrawtracks, 1, ( "DrawTracks time = %lu mS\n", wGetTimer()-time0 ) );
	}
	InfoCount( trackCount );
	inDrawTracks = FALSE;
	if ( doSelectRecount )
		SelectRecount();
}


EXPORT void DrawSelectedTracks( drawCmd_p d, BOOL_T all )
{
	track_cp trk;
	wIndex_t count;

	count = 0;
	InfoCount( 0 );

	TRK_ITERATE( trk ) {
		if ( (all && GetLayerVisible(GetTrkLayer(trk))) || GetTrkSelected( trk ) ) {
			DrawTrack( trk, d, wDrawColorBlack );
			count++;
			if (count%10 == 0)
				InfoCount( count );
		}
	}
	InfoCount( trackCount );
	SelectRecount();
}


EXPORT void HilightElevations( BOOL_T hilight )
{
	track_p trk, trk1;
	EPINX_T ep;
	int mode;
	DIST_T elev;
	coOrd pos;
	DIST_T radius;

	radius = 0.05*mainD.scale;
	if ( radius < trackGauge/2.0 )
		radius = trackGauge/2.0;
	TRK_ITERATE( trk ) {
		for (ep=0;ep<GetTrkEndPtCnt(trk);ep++) {
			GetTrkEndElev( trk, ep, &mode, &elev );
			if ((mode&ELEV_MASK)==ELEV_DEF || (mode&ELEV_MASK)==ELEV_IGNORE) {
				if ((trk1=GetTrkEndTrk(trk,ep)) != NULL &&
					GetTrkIndex(trk1) < GetTrkIndex(trk))
					continue;
				if (drawTunnel == DRAW_TUNNEL_NONE && (!GetTrkVisible(trk)) && (trk1==NULL||!GetTrkVisible(trk1)) )
					continue;
				if ((!GetLayerVisible(GetTrkLayer(trk))) &&
					(trk1==NULL||!GetLayerVisible(GetTrkLayer(trk1))))
					continue;
				pos = GetTrkEndPos(trk,ep);
				if ( !OFF_MAIND( pos, pos ) ) {
					DrawFillCircle( &tempD, pos, radius,
						((mode&ELEV_MASK)==ELEV_DEF?elevColorDefined:elevColorIgnore) );
				}
			}
		}
	}
}


EXPORT void HilightSelectedEndPt( BOOL_T show, track_p trk, EPINX_T ep )
{
	coOrd pos;
	if (!trk || (ep==-1)) return;
	pos = GetTrkEndPos( trk, ep );
	if ( show == TRUE ) {
		pos = GetTrkEndPos( trk, ep );
		DrawFillCircle( &tempD, pos, 0.10*mainD.scale, selectedColor );
	} else 	 {
		pos = GetTrkEndPos( trk, ep );
		DrawFillCircle( &tempD, pos, 0.10*mainD.scale, wDrawColorWhite );
	}
}


EXPORT void LabelLengths( drawCmd_p d, track_p trk, wDrawColor color )
{
	wFont_p fp;
	wFontSize_t fs;
	EPINX_T i;
	coOrd p0, p1;
	DIST_T dist;
	char * msg;
	coOrd textsize;

	if ((labelEnable&LABELENABLE_LENGTHS)==0)
		return;
	fp = wStandardFont( F_HELV, FALSE, FALSE );
	fs = (float)descriptionFontSize/d->scale;
	for (i=0; i<GetTrkEndPtCnt(trk); i++) {
		p0 = GetTrkEndPos( trk, i );
		dist = GetFlexLength( trk, i, &p1 );
		if (dist < 0.1) 
			continue;
		if (dist < 3.0) {
			p0.x = (p0.x+p1.x)/2.0;
			p0.y = (p0.y+p1.y)/2.0;
		} else {
			Translate( &p0, p0, GetTrkEndAngle(trk,i), 0.25*d->scale );
		}
		msg = FormatDistance(dist);
		DrawTextSize( &mainD, msg, fp, fs, TRUE, &textsize );
		p0.x -= textsize.x/2.0;
		p0.y -= textsize.y/2.0;
		DrawString( d, p0, 0.0, msg, fp, fs*d->scale, color );
	}
}

EXPORT void AddTrkDetails(drawCmd_p d,track_p trk,coOrd pos, DIST_T length, wDrawColor color) {
	#define DESC_LENGTH 6.0;
	double division;
	division = length/DESC_LENGTH;
	division = ceil(division);
	DIST_T dist = length/division, dist1;
	traverseTrack_t tt;
	tt.trk = trk;
	tt.angle = GetTrkEndAngle(trk,0)+180.0;
	tt.pos = GetTrkEndPos(trk,0);

	dynArr_t pos_array;
	pos_array.max = 0;
	pos_array.cnt = 0;
	pos_array.ptr = NULL;

	typedef struct {
					coOrd pos;
					ANGLE_T angle;
				} pos_angle_t;

	DYNARR_SET(pos_angle_t,pos_array,(int)division+1);
	DYNARR_N(pos_angle_t,pos_array,0).pos = GetTrkEndPos(trk,0);
	DYNARR_N(pos_angle_t,pos_array,0).angle = NormalizeAngle(GetTrkEndAngle(trk,0)+180.0);
	for (int i=1;i<pos_array.cnt;i++) {
		tt.dist = dist;
		dist1 = dist;
		TraverseTrack(&tt,&dist1);
		if (dist1 > 0 || tt.trk != trk || IsClose(FindDistance(tt.pos,GetTrkEndPos(trk,1)))) {
			DYNARR_N(pos_angle_t,pos_array,i).pos = GetTrkEndPos(trk,1);
			DYNARR_N(pos_angle_t,pos_array,i).angle = GetTrkEndAngle(trk,1);
			pos_array.cnt = i;
			break;
		}
		DYNARR_N(pos_angle_t,pos_array,i).pos = tt.pos;
		DYNARR_N(pos_angle_t,pos_array,i).angle = tt.angle;
	}
	message[0]='\0';
	for (int i=0;i<pos_array.cnt;i++) {
		if (i==pos_array.cnt-1)
			sprintf( &message[strlen(message)], _("[%0.2f,%0.2f] A%0.2f"),PutDim(DYNARR_N(pos_angle_t,pos_array,i).pos.x),PutDim(DYNARR_N(pos_angle_t,pos_array,i).pos.y),DYNARR_N(pos_angle_t,pos_array,i).angle );
		else
			sprintf( &message[strlen(message)], _("[%0.2f,%0.2f] A%0.2f\n"),PutDim(DYNARR_N(pos_angle_t,pos_array,i).pos.x),PutDim(DYNARR_N(pos_angle_t,pos_array,i).pos.y),DYNARR_N(pos_angle_t,pos_array,i).angle);
	}
	wFont_p fp = wStandardFont( F_TIMES, FALSE, FALSE );
	DrawBoxedString(BOX_BOX,d,pos,message,fp,(wFontSize_t)descriptionFontSize,color,0.0);
	if (pos_array.ptr)
		MyFree(pos_array.ptr);
	pos_array.ptr = 0;
	pos_array.max = 0;
	pos_array.cnt = 0;
}

