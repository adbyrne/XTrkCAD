/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/track.c,v 1.7 2009-07-05 15:11:02 m_fischer Exp $
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

#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include "track.h"
#include "ccurve.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "compound.h"
#include "i18n.h"
#include "draw.h"

#ifndef TRACKDEP
#ifndef FASTTRACK
#include "trackx.h"
#endif
#endif

#ifndef WINDOWS
#include <errno.h>
#else
// starting from Visual Studio 2015 round is in the runtime library, fake otherwise
#if ( _MSC_VER < 1900 )
#define round(x) floor((x)+0.5)
#endif
#endif

static int log_track = 0;
static int log_endPt = 0;
static int log_readTracks = 0;

/*****************************************************************************
 *
 * VARIABLES
 *
 */

#define DRAW_TUNNEL_NONE		(0)

#define CLOSETOTHEEDGE			(10)		/**< minimum distance between paste position and edge of window */

EXPORT wIndex_t trackCount;

EXPORT long drawEndPtV = 2;

EXPORT long centerDrawMode = FALSE;			/**< flag to control drawing of circle centers */

static BOOL_T exportingTracks = FALSE;

EXPORT signed char * pathPtr;
EXPORT int pathCnt = 0;
EXPORT int pathMax = 0;

static dynArr_t trackCmds_da;
#define trackCmds(N) DYNARR_N( trackCmd_t*, trackCmds_da, N )

EXPORT BOOL_T useCurrentLayer = FALSE;

EXPORT LAYER_T curTrackLayer;

EXPORT coOrd descriptionOff;

EXPORT DIST_T roadbedWidth = 0.0;
EXPORT DIST_T roadbedLineWidth = 3.0/75.0;

EXPORT DIST_T minTrackRadius;
EXPORT DIST_T maxTrackGrade = 5.0;

static int suspendElevUpdates = FALSE;

static track_p * importTrack;

EXPORT BOOL_T onTrackInSplit;

static BOOL_T inDrawTracks;

#ifndef TRACKDEP

/*****************************************************************************
 *
 * 
 *
 */


EXPORT void DescribeTrack( track_cp trk, char * str, CSIZE_T len )
{
	trackCmds( GetTrkType(trk) )->describe ( trk, str, len );
	/*epCnt = GetTrkEndPtCnt(trk);
	if (debugTrack >= 2)
		for (ep=0; epCnt; ep++)
			 PrintEndPt( logFile, trk, ep );???*/
}


EXPORT DIST_T GetTrkDistance( track_cp trk, coOrd pos )
{
	return trackCmds( GetTrkType(trk) )->distance( trk, &pos );
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

EXPORT track_p OnTrack2( coOrd * fp, BOOL_T complain, BOOL_T track, BOOL_T ignoreHidden )
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
		if (distance < closestDistance) {
			closestDistance = distance;
			closestTrack = trk;
			closestPos = p;
		}
	}
	if (closestTrack && (closestDistance <= mainD.scale*0.25 || closestDistance <= trackGauge*2.0) ) {
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
	return OnTrack2( fp, complain, track, TRUE );
}


EXPORT BOOL_T CheckTrackLayer( track_p trk )
{
	if (GetLayerFrozen( GetTrkLayer( trk ) ) ) {
		ErrorMessage( MSG_CANT_MODIFY_FROZEN_TRK );
		return FALSE;
	} else {
		return TRUE;
	}
}

/******************************************************************************
 *
 * PARTS LIST
 *
 */


EXPORT void EnumerateTracks( void )
{
	track_p trk;
	TRKINX_T inx;

	enumerateMaxDescLen = strlen("Description");

	TRK_ITERATE( trk ) {
		/* 
		 *	process track piece if none are selected (list all) or if it is one of the
		 *	selected pieces (list only selected )
		 */
		if ((!selectedTrackCount || GetTrkSelected(trk)) && trackCmds(trk->type)->enumerate != NULL)
			trackCmds(trk->type)->enumerate( trk );
	}

	EnumerateStart();

	for (inx=1; inx<trackCmds_da.cnt; inx++)
		if (trackCmds(inx)->enumerate != NULL)
			trackCmds(inx)->enumerate( NULL );

	EnumerateEnd();
	Reset();
}

/*****************************************************************************
 *
 * NOTRACK
 *
 */

static void AbortNoTrack( void )
{
	AbortProg( "No Track Op called" );
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
}

/*****************************************************************************
 *
 * TRACK FIELD ACCESS
 *
 */


#ifndef FASTTRACK

EXPORT TRKINX_T GetTrkIndex( track_p trk )
{
	return trk->index;
}

EXPORT TRKTYP_T GetTrkType( track_p trk )
{
	ASSERT( trk->type != T_NOTRACK && !IsTrackDeleted(trk) );
	return trk->type;
}

EXPORT SCALEINX_T GetTrkScale( track_p trk )
{
	return (SCALEINX_T)trk->scale;
}

EXPORT void SetTrkScale( track_p trk, SCALEINX_T si )
{
	trk->scale = (char)si;
}

EXPORT LAYER_T GetTrkLayer( track_p trk )
{
	return trk->layer;
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

EXPORT struct extraData * GetTrkExtraData( track_cp trk )
{
	return trk->extraData;
}

EXPORT void SetTrkEndPoint( track_p trk, EPINX_T ep, coOrd pos, ANGLE_T angle )
{
	if (trk->endPt[ep].track != NULL) {
		AbortProg( "setTrkEndPoint: endPt is connected" );
	}
	trk->endPt[ep].pos = pos;
	trk->endPt[ep].angle = angle;
}

EXPORT coOrd GetTrkEndPos( track_p trk, EPINX_T e )
{
	return trk->endPt[e].pos;
}

EXPORT ANGLE_T GetTrkEndAngle( track_p trk, EPINX_T e )
{
	return trk->endPt[e].angle;
}

EXPORT track_p GetTrkEndTrk( track_p trk, EPINX_T e )
{
	return trk->endPt[e].track;
}

EXPORT long GetTrkEndOption( track_p trk, EPINX_T e )
{
	return trk->endPt[e].option;
}

EXPORT long SetTrkEndOption( track_p trk, EPINX_T e, long option )
{
	return trk->endPt[e].option = option;
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
	return trk->bits;
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
#endif

EXPORT void SetTrkEndElev( track_p trk, EPINX_T ep, int option, DIST_T height, char * station )
{
	track_p trk1;
	EPINX_T ep1;
	trk->endPt[ep].elev.option = option;
	if (EndPtIsDefinedElev(trk,ep)) {
		trk->endPt[ep].elev.u.height = height;
	} else if (EndPtIsStationElev(trk,ep)) {
		if (station == NULL)
			station = "";
		trk->endPt[ep].elev.u.name = MyStrdup(station);
	}
	if ( (trk1=GetTrkEndTrk(trk, ep)) != NULL ) {
		ep1 = GetEndPtConnectedToMe( trk1, trk );
		if (ep1 >= 0) {
			trk1->endPt[ep1].elev.option = option;
			trk1->endPt[ep1].elev.u.height = height;
			if (EndPtIsDefinedElev(trk1,ep1))
				trk1->endPt[ep1].elev.u.height = height;
			else if (EndPtIsStationElev(trk,ep))
				trk1->endPt[ep1].elev.u.name = MyStrdup(station);
		}
	}
}


EXPORT void GetTrkEndElev( track_p trk, EPINX_T e, int *option, DIST_T *height )
{
	*option = trk->endPt[e].elev.option;
	*height = trk->endPt[e].elev.u.height;
}


EXPORT int GetTrkEndElevUnmaskedMode( track_p trk, EPINX_T e )
{
	return trk->endPt[e].elev.option;
}


EXPORT int GetTrkEndElevMode( track_p trk, EPINX_T e )
{
	return trk->endPt[e].elev.option&ELEV_MASK;
}


EXPORT DIST_T GetTrkEndElevHeight( track_p trk, EPINX_T e )
{
	ASSERT( EndPtIsDefinedElev(trk,e) );
	return trk->endPt[e].elev.u.height;
}


EXPORT char * GetTrkEndElevStation( track_p trk, EPINX_T e )
{
	ASSERT( EndPtIsStationElev(trk,e) );
	if ( trk->endPt[e].elev.u.name == NULL )
		return "";
	else
		return trk->endPt[e].elev.u.name;
}


EXPORT void SetTrkEndPtCnt( track_p trk, EPINX_T cnt )
{
	EPINX_T oldCnt = trk->endCnt;
	trk->endCnt = cnt;
	if ((trk->endPt = MyRealloc( trk->endPt, trk->endCnt * sizeof trk->endPt[0] )) == NULL) {
		AbortProg("setTrkEndPtCnt: No memory" );
	}
	if (oldCnt < cnt)
		memset( &trk->endPt[oldCnt], 0, (cnt-oldCnt) * sizeof *trk->endPt );
}


EXPORT void SetTrkLayer( track_p trk, int layer )
{
	if (useCurrentLayer)
		trk->layer = (LAYER_T)curLayer;
	else
		trk->layer = (LAYER_T)layer;
}



EXPORT int ClrAllTrkBits( int bits )
{
	track_p trk;
	int cnt;
	cnt = 0;
	TRK_ITERATE( trk ) {
		if (trk->bits&bits)
			cnt++;
		trk->bits &= ~bits;
	}
	return cnt;
}



EXPORT void SetTrkElev( track_p trk, int mode, DIST_T elev )
{
	SetTrkBits( trk, TB_ELEVPATH );
	trk->elev = elev;
	trk->elevMode = mode;
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


EXPORT BOOL_T WriteEndPt( FILE * f, track_cp trk, EPINX_T ep )
{
	trkEndPt_p endPt = &trk->endPt[ep];
	BOOL_T rc = TRUE;
	long option;

	assert ( endPt != NULL );
	if (endPt->track == NULL ||
		( exportingTracks && !GetTrkSelected(endPt->track) ) ) {
		rc &= fprintf( f, "\tE " )>0;
	} else {
		rc &= fprintf( f, "\tT %d ", endPt->track->index )>0;
	}
	rc &= fprintf( f, "%0.6f %0.6f %0.6f", endPt->pos.x, endPt->pos.y, endPt->angle )>0; 
	option = (endPt->option<<8) | (endPt->elev.option&0xFF);
	if ( option != 0 ) {
		rc &= fprintf( f, " %ld %0.6f %0.6f", option, endPt->elev.doff.x, endPt->elev.doff.y )>0;
		if ( (endPt->elev.option&ELEV_MASK) != ELEV_NONE ) {
			switch ( endPt->elev.option&ELEV_MASK ) {
			case ELEV_DEF:
				rc &= fprintf( f, " %0.6f", endPt->elev.u.height )>0;
				break;
			case ELEV_STATION:
				rc &= fprintf( f, " \"%s\"", PutTitle( endPt->elev.u.name ) )>0;
				break;
			default:
				;
			}
		}
	}
	rc &= fprintf( f, "\n" )>0;
	return rc;
}


EXPORT EPINX_T PickEndPoint( coOrd p, track_cp trk )
{
	EPINX_T inx, i;
	DIST_T d, dd;
	coOrd pos;
	if (trk->endCnt <= 0)
		return -1;
	if ( onTrackInSplit && trk->endCnt > 2 )
		return TurnoutPickEndPt( p, trk );
	d = FindDistance( p, trk->endPt[0].pos );
	inx = 0;
	for ( i=1; i<trk->endCnt; i++ ) {
		pos = trk->endPt[i].pos;
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
		if (trk->endPt[i].track == NULL) {
			pos = trk->endPt[i].pos;
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


EXPORT EPINX_T GetEndPtConnectedToMe( track_p trk, track_p me )
{
	EPINX_T ep;
	for (ep=0; ep<trk->endCnt; ep++)
		if (trk->endPt[ep].track == me)
			return ep;
	return -1;
}


EXPORT void SetEndPts( track_p trk, EPINX_T cnt )
{
	EPINX_T inx;

LOG1( log_readTracks, ( "SetEndPts( T%d, %d )\n", trk->index, cnt ) )
	if (cnt > 0 && tempEndPts_da.cnt != cnt) {
		InputError( "Incorrect number of End Points for track, read %d, expected %d.\n", FALSE, tempEndPts_da.cnt, cnt );
		return;
	}
	if (tempEndPts_da.cnt) {
		trk->endPt = (trkEndPt_p)MyMalloc( tempEndPts_da.cnt * sizeof *trk->endPt );
	} else {
		trk->endPt = NULL;
	}
	for ( inx=0; inx<tempEndPts_da.cnt; inx++ ) {
		trk->endPt[inx].index = tempEndPts(inx).index;
		trk->endPt[inx].pos = tempEndPts(inx).pos;
		trk->endPt[inx].angle = tempEndPts(inx).angle;
		trk->endPt[inx].elev = tempEndPts(inx).elev;
		trk->endPt[inx].option = tempEndPts(inx).option;
	}
	trk->endCnt = tempEndPts_da.cnt;
}


EXPORT void MoveTrack( track_p trk, coOrd orig )
{
	EPINX_T ep;
	for (ep=0; ep<trk->endCnt; ep++) {
		trk->endPt[ep].pos.x += orig.x;
		trk->endPt[ep].pos.y += orig.y;
	}
	trackCmds( trk->type )->move( trk, orig );
}


EXPORT void RotateTrack( track_p trk, coOrd orig, ANGLE_T angle )
{
	EPINX_T ep;
	for (ep=0; ep<trk->endCnt; ep++) {
		Rotate( &trk->endPt[ep].pos, orig, angle );
		trk->endPt[ep].angle = NormalizeAngle( trk->endPt[ep].angle + angle );
	}
	trackCmds( trk->type )->rotate( trk, orig, angle );
}


EXPORT void RescaleTrack( track_p trk, FLOAT_T ratio, coOrd shift )
{
	EPINX_T ep;
	if ( trackCmds( trk->type )->rotate == NULL )
		return;
	for (ep=0; ep<trk->endCnt; ep++) {
		trk->endPt[ep].pos.x *= ratio;
		trk->endPt[ep].pos.y *= ratio;
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
	trkEndPt_t endPt;

	for ( ep=0; ep<trk->endCnt; ep++ ) {
		FlipPoint( &trk->endPt[ep].pos, orig, angle );
		trk->endPt[ep].angle = NormalizeAngle( 2*angle - trk->endPt[ep].angle );
	}
	if ( trackCmds(trk->type)->flip )
		trackCmds(trk->type)->flip( trk, orig, angle );
	if ( QueryTrack( trk, Q_FLIP_ENDPTS ) ) {
		endPt = trk->endPt[0];
		trk->endPt[0] = trk->endPt[1];
		trk->endPt[1] = endPt;
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
			 ((trk1->endPt[ep].elev.option&ELEV_MASK)==ELEV_IGNORE))
			continue;
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
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R )
{
	if ( trackCmds(trk->type)->makeParallel )
		return trackCmds(trk->type)->makeParallel( trk, pos, dist, newTrkR, p0R, p1R );
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
	EPINX_T ep;
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
	trk->scale = (char)curScaleInx;
	trk->bits = TB_VISIBLE;
	trk->elevMode = ELEV_ALONE;
	trk->elev = 0;
	trk->endCnt = endCnt;
	trk->hi.x = trk->hi.y = trk->lo.x = trk->lo.y = (float)0.0;
	if (endCnt) {
		trk->endPt = (trkEndPt_p)MyMalloc( endCnt * sizeof *trk->endPt );
		for ( ep = 0; ep < endCnt; ep++ )
			trk->endPt[ep].index = -1;
	} else
		trk->endPt = NULL;
	if (extraSize) {
		trk->extraData = MyMalloc( extraSize );
	} else
		trk->extraData = NULL;
	trk->extraSize = extraSize;
	UndoNew( trk );
	trackCount++;
	InfoCount( trackCount );
	return trk;
}


EXPORT void FreeTrack( track_p trk )
{
	trackCmds(trk->type)->delete( trk );
	if (trk->endPt)
		MyFree(trk->endPt);
	if (trk->extraData)
		MyFree(trk->extraData);
	MyFree(trk);
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
	changed = 0;
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
	TRK_ITERATE(trk)
		for (ep=0; ep<trk->endCnt; ep++)
			if (trk->endPt[ep].index >= 0) {
				trk->endPt[ep].track = FindTrack( trk->endPt[ep].index );
				if (trk->endPt[ep].track == NULL) {
					NoticeMessage( MSG_RESOLV_INDEX_BAD_TRK, _("Continue"), NULL, trk->index, ep, trk->endPt[ep].index );
				}
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
				if ((trk2=trk->endPt[i].track) != NULL) {
					if (QueryTrack(trk2,Q_CANNOT_BE_ON_END)) {
						DeleteTrack( trk2, FALSE );
					}
				}
			}
		}
	}
	UndrawNewTrack( trk );
	for (i=0;i<trk->endCnt;i++) {
		if ((trk2=trk->endPt[i].track) != NULL) {
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			/*UndrawNewTrack( trk2 );*/
			DrawEndPt( &mainD, trk2, ep2, wDrawColorWhite );
			DisconnectTracks( trk2, ep2, trk, i );
			/*DrawNewTrack( trk2 );*/
			if (!QueryTrack(trk2,Q_DONT_DRAW_ENDPOINT))
				DrawEndPt( &mainD, trk2, ep2, wDrawColorBlack );
			if ( QueryTrack(trk,Q_CANNOT_BE_ON_END) )
				UndoJoint( trk2, ep2, trk, i );
			ClrTrkElev( trk2 );
		}
	}
	UndoDelete( trk );
    MainRedraw();
	trackCount--;
	AuditTracks( "deleteTrack T%d", trk->index);
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
 * ABOVE / BELOW
 *
 */

static void ExciseSelectedTracks( track_p * pxtrk, track_p * pltrk )
{
	track_p trk, *ptrk;
	for (ptrk=&to_first; *ptrk!=NULL; ) {
		trk = *ptrk;
		if (IsTrackDeleted(trk) || !GetTrkSelected(trk)) {
			ptrk = &(*ptrk)->next;
			continue;
		}
		UndoModify( *ptrk );
		UndoModify( trk );
		*ptrk = trk->next;
		*pltrk = *pxtrk = trk;
		pxtrk = &trk->next;
		trk->next = NULL;
	}
	to_last = ptrk;
}


EXPORT void SelectAbove( void )
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
	DrawSelectedTracks( &mainD );
}


EXPORT void SelectBelow( void )
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


#include "bitmaps/above.xpm"
#include "bitmaps/below.xpm"

EXPORT void InitCmdAboveBelow( void )
{
	wIcon_p bm_p;
	bm_p = wIconCreatePixMap( above_xpm );
	AddToolbarButton( "cmdAbove", bm_p, IC_SELECTED, (addButtonCallBack_t)SelectAbove, NULL );
	bm_p = wIconCreatePixMap( below_xpm );
	AddToolbarButton( "cmdBelow", bm_p, IC_SELECTED, (addButtonCallBack_t)SelectBelow, NULL );
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
			sortedCmds[inx]->read(line);
			return TRUE;
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
			 return TRUE;
		 }
	}
}
	if (strncmp( paramLine, "TABLEEDGE ", 10 ) == 0)
		return ReadTableEdge( paramLine+10 );
	if (strncmp( paramLine, "TEXT ", 5 ) == 0)
		return ReadText( paramLine+5 );
	return FALSE;
}


EXPORT BOOL_T WriteTracks( FILE * f )
{
	track_p trk;
	BOOL_T rc = TRUE;
	RenumberTracks();
	TRK_ITERATE( trk ) {
		rc &= trackCmds(GetTrkType(trk))->write( trk, f );
	}
	rc &= WriteCars( f );
	return rc;
}



EXPORT void ImportStart( void )
{
	importTrack = to_last;
}


EXPORT void ImportEnd( void )
{
	track_p to_firstOld;
	wIndex_t trackCountOld;
	track_p trk;
	coOrd pos;
	wPos_t x, y;
	wPos_t ww, hh;
	double ymax = 0.0;

	// get the current mouse position
	GetMousePosition( &x, &y );
	mainD.Pix2CoOrd( &mainD, x, y, &pos );
	
	// get the size of the drawing area
	wDrawGetSize( mainD.d, &ww, &hh );

	// in case the pointer is close to the edge or above the drawing area
	// recalculate the destination position so the pasted part remains visible
	if( abs( y - hh ) < CLOSETOTHEEDGE  ) {
		for ( trk=*importTrack; trk; trk=trk->next ) {
			if (!IsTrackDeleted(trk) && trk->hi.y > ymax ) {
				ymax = trk->hi.y;
			}
		}
		pos.y -= ymax;
	}
	
	to_firstOld = to_first;
	to_first = *importTrack;
	trackCountOld = trackCount;
	ResolveIndex();
	to_first = to_firstOld;
	RenumberTracks();
	DrawMapBoundingBox( FALSE );

	// move the imported track into place
	for ( trk=*importTrack; trk; trk=trk->next ) if (!IsTrackDeleted(trk)) {
		MoveTrack( trk, pos );// mainD.orig );
		trk->bits |= TB_SELECTED;
		DrawTrack( trk, &mainD, wDrawColorBlack );
	}
	DrawMapBoundingBox( TRUE );
	importTrack = NULL; 
	trackCount = trackCountOld;
	InfoCount( trackCount );
}


EXPORT BOOL_T ExportTracks( FILE * f )
{
	track_p trk;
	coOrd xlat, orig;
	
	exportingTracks = TRUE;
	orig = mapD.size;
	max_index = 0;
	TRK_ITERATE(trk) {
		if ( GetTrkSelected(trk) ) {
			if (trk->lo.x < orig.x)
				orig.x = trk->lo.x;
			if (trk->lo.y < orig.y)
				orig.y = trk->lo.y;
			trk->index = ++max_index;
		}
	}
	orig.x -= trackGauge;
	orig.y -= trackGauge;
	xlat.x = - orig.x;
	xlat.y = - orig.y;
	TRK_ITERATE( trk ) {
		if ( GetTrkSelected(trk) ) {
			MoveTrack( trk, xlat );
			trackCmds(GetTrkType(trk))->write( trk, f );
			MoveTrack( trk, orig );
		}
	}
	RenumberTracks();
	exportingTracks = FALSE;
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
		sprintf( message, "%s%s%s", workingDir, FILE_SEP_CHAR, sAuditF );
		auditFile = fopen( message, "a+" );
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
		sprintf( msgp, "*to_last is not NULL (%lx)", (long)*to_last );
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
			if ( (tn = trk->endPt[i].track) != NULL ) {
				if (IsTrackDeleted(trk)) {
					sprintf( msgp, "T%d[%d]: T%d is deleted\n", trk->index, i, tn->index );
					AuditPrint( msg );
					trk->endPt[i].track = NULL;
				} else {
					for (j=0;j<tn->endCnt;j++)
						if (tn->endPt[j].track == trk)
							goto nextEndPt;
					sprintf( msgp, "T%d[%d]: T%d doesn\'t point back\n", trk->index, i, tn->index );
					AuditPrint( msg );
					trk->endPt[i].track = NULL;
				}
			}
nextEndPt:;
		}
		if (!trk->next) {
			if (to_last != &trk->next) {
				sprintf( msgp, "last track (T%d @ %lx) is not to_last (%lx)\n",
						trk->index, (long)trk, (long)to_last );
				AuditPrint( msg );
			}
		}
	}
	InfoCount( trackCount );
	if (auditFile != NULL) {
	if (auditStop)
		if (NoticeMessage( MSG_AUDIT_WRITE_FILE, _("Yes"), _("No"))) {
			fprintf( auditFile, "# before undo\n" );
			WriteTracks(auditFile);
			Rdump( auditFile );
			if (strcmp("undoUndo",event)==0) {
				fprintf( auditFile, "# failure in undo\n" );
			} else if (UndoUndo()) {
				fprintf( auditFile, "# after undo\n" );
				WriteTracks(auditFile);
				Rdump( auditFile );
			} else {
				fprintf( auditFile, "# undo stack is empty\n" );
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

	if (trk->endCnt <= 0)
		AbortProg("computeBoundingBox - endCnt<=0");

	trk->hi.x = trk->lo.x = (float)trk->endPt[0].pos.x;
	trk->hi.y = trk->lo.y = (float)trk->endPt[0].pos.y;
	for ( i=1; i<trk->endCnt; i++ ) {
		if (trk->endPt[i].pos.x > trk->hi.x)
			trk->hi.x = (float)trk->endPt[i].pos.x;
		if (trk->endPt[i].pos.y > trk->hi.y)
			trk->hi.y = (float)trk->endPt[i].pos.y;
		if (trk->endPt[i].pos.x < trk->lo.x)
			trk->lo.x = (float)trk->endPt[i].pos.x;
		if (trk->endPt[i].pos.y < trk->lo.y)
			trk->lo.y = (float)trk->endPt[i].pos.y;
	}
}



EXPORT DIST_T EndPtDescriptionDistance(
		coOrd pos,
		track_p trk,
		EPINX_T ep )
{
	elev_t *e;
	coOrd pos1;
	track_p trk1;
	e = &trk->endPt[ep].elev;
	if ((e->option&ELEV_MASK)==ELEV_NONE ||
		(e->option&ELEV_VISIBLE)==0 )
		return 100000;
	if ((trk1=GetTrkEndTrk(trk,ep)) && GetTrkIndex(trk1)<GetTrkIndex(trk))
		return 100000;
	/*REORIGIN( pos1, e->doff, GetTrkEndPos(trk,ep), GetTrkEndAngle(trk,ep) );*/
	pos1 = GetTrkEndPos(trk,ep);
	pos1.x += e->doff.x;
	pos1.y += e->doff.y;
	return FindDistance( pos1, pos );
}


EXPORT STATUS_T EndPtDescriptionMove(
		track_p trk,
		EPINX_T ep,
		wAction_t action,
		coOrd pos )
{
	static coOrd p0, p1;
	elev_t *e, *e1;
	wDrawColor color;
	track_p trk1;

	e = &trk->endPt[ep].elev;
	switch (action) {
	case C_DOWN:
		p0 = GetTrkEndPos(trk,ep);
		/*REORIGIN( p0, e->doff, GetTrkEndPos(trk,ep), GetTrkEndAngle(trk,ep) );*/
		
	case C_MOVE:
	case C_UP:
		if (action != C_DOWN)
			DrawLine( &tempD, p0, p1, 0, wDrawColorBlack );
		color = GetTrkColor( trk, &mainD );
		DrawEndElev( &tempD, trk, ep, color );
		p1 = pos;
		e->doff.x = (pos.x-p0.x);
		e->doff.y = (pos.y-p0.y);
		if ((trk1=GetTrkEndTrk(trk,ep))) {
			e1 = &trk1->endPt[GetEndPtConnectedToMe(trk1,trk)].elev;
			e1->doff = e->doff;
		}
		DrawEndElev( &tempD, trk, ep, color );
		if (action != C_UP)
			DrawLine( &tempD, p0, p1, 0, wDrawColorBlack );
        MainRedraw();
		return action==C_UP?C_TERMINATE:C_CONTINUE;

	case C_REDRAW:
		DrawLine( &tempD, p0, p1, 0, wDrawColorBlack );
		break;
	}
	return C_CONTINUE;
}


/*****************************************************************************
 *
 * TRACK SPLICING ETC
 *
 */


static DIST_T distanceEpsilon = 0.0;
static ANGLE_T angleEpsilon = 0.0;

EXPORT void LoosenTracks( void )
{
	track_p trk, trk1;
	EPINX_T ep0, ep1;
	ANGLE_T angle0, angle1;
	coOrd pos0, pos1;
	DIST_T d;
	ANGLE_T a;
	int count;

	count = 0;
	TRK_ITERATE(trk) {
		for (ep0=0; ep0<trk->endCnt; ep0++) {
			trk1 = GetTrkEndTrk( trk, ep0 );
			if (trk1 == NULL)
				continue;
			ASSERT( !IsTrackDeleted(trk1) );
			ep1 = GetEndPtConnectedToMe( trk1, trk );
			if (ep1 < 0)
				continue;
			pos0 = GetTrkEndPos( trk, ep0 );
			pos1 = GetTrkEndPos( trk1, ep1 );
			angle0 = GetTrkEndAngle( trk, ep0 );
			angle1 = GetTrkEndAngle( trk1, ep1 );
			d = FindDistance( pos0, pos1 );
			a = NormalizeAngle( 180+angle0-angle1+angleEpsilon );
			if (d > distanceEpsilon || a > angleEpsilon*2.0) {
				DisconnectTracks( trk, ep0, trk1, ep1 );
				count++;
				InfoMessage( _("%d Track(s) loosened"), count );
			}
		}
	}
	if (count)
		MainRedraw();
	else
		InfoMessage(_("No tracks loosened"));
}

EXPORT void ConnectTracks( track_p trk0, EPINX_T inx0, track_p trk1, EPINX_T inx1 )
{
	DIST_T d;
	ANGLE_T a;
	coOrd pos0, pos1;

	if ( !IsTrack(trk0) ) {
		NoticeMessage( _("Connecting a non-track(%d) to (%d)"), _("Continue"), NULL, GetTrkIndex(trk0), GetTrkIndex(trk1) );
		return;
	}
	if ( !IsTrack(trk1) ) {
		NoticeMessage( _("Connecting a non-track(%d) to (%d)"), _("Continue"), NULL, GetTrkIndex(trk1), GetTrkIndex(trk0) );
		return;
	}
	pos0 = trk0->endPt[inx0].pos;
	pos1 = trk1->endPt[inx1].pos;
LOG( log_track, 3, ( "ConnectTracks( T%d[%d] @ [%0.3f, %0.3f] = T%d[%d] @ [%0.3f %0.3f]\n", trk0->index, inx0, pos0.x, pos0.y, trk1->index, inx1, pos1.x, pos1.y ) )
	d = FindDistance( pos0, pos1 );
	a = NormalizeAngle( trk0->endPt[inx0].angle -
						trk1->endPt[inx1].angle + (180.0+connectAngle/2.0) );
	if (d > connectDistance || a > connectAngle || logTable(log_endPt).level>=1) {
#ifndef WINDOWS
		LogPrintf( "connectTracks: T%d[%d] T%d[%d] d=%0.3f a=%0.3f\n   %d ",
				trk0->index, inx0, trk1->index, inx1, d, a, trk0->index );
		/*PrintEndPt( logFile, trk0, 0 );
		PrintEndPt( logFile, trk0, 1 );???*/
		LogPrintf( "\n   %d ", trk1->index );
		/*PrintEndPt( logFile, trk1, 0 );
		PrintEndPt( logFile, trk1, 1 );???*/
		LogPrintf("\n");
#endif
		NoticeMessage( MSG_CONNECT_TRK, _("Continue"), NULL, trk0->index, inx0, trk1->index, inx1, d, a );
	}
	UndoModify( trk0 );
	UndoModify( trk1 );
	if (!suspendElevUpdates)
		SetTrkElevModes( TRUE, trk0, inx0, trk1, inx1 );
	trk0->endPt[inx0].track = trk1;
	trk1->endPt[inx1].track = trk0;
	AuditTracks( "connectTracks T%d[%d], T%d[%d]", trk0->index, inx0, trk1->index, inx1 );
}


EXPORT void DisconnectTracks( track_p trk1, EPINX_T ep1, track_p trk2, EPINX_T ep2 )
{
	if (trk1->endPt[ep1].track != trk2 ||
		trk2->endPt[ep2].track != trk1 )
		AbortProg("disconnectTracks: tracks not connected" );
	UndoModify( trk1 );
	UndoModify( trk2 );
	trk1->endPt[ep1].track = NULL;
	trk2->endPt[ep2].track = NULL;
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

	trk0 = trk;
	epl = ep;
	epCnt = GetTrkEndPtCnt(trk);
	*leftover = NULL;
LOG( log_track, 2, ( "SplitTrack( T%d[%d], (%0.3f %0.3f)\n", trk->index, ep, pos.x, pos.y ) )

	if ((splitCmd = trackCmds(trk->type)->split) == NULL) {
		ErrorMessage( MSG_CANT_SPLIT_TRK, trackCmds(trk->type)->name );
		return FALSE;
	}
	UndrawNewTrack( trk );
	UndoModify( trk );
	pos0 = trk->endPt[ep].pos;
	if ((d = FindDistance( pos0, pos )) <= minLength) {
		/* easy: just disconnect */
		if ((trk2=trk->endPt[ep].track) != NULL) {
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
		*leftover = trk2;
		DrawNewTrack( trk );

#ifdef LATER
	} else if ( IsTurnout(trk) ) {
			ErrorMessage( MSG_CANT_SPLIT_TRK, _("Turnout") );
			return FALSE;
#endif

	} else if ( epCnt == 2 &&
				(d = FindDistance( trk->endPt[1-ep].pos, pos )) <= minLength) {
		/* easy: just disconnect */
		if ((trk2=trk->endPt[1-ep].track) != NULL) {
			UndrawNewTrack( trk2 );
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			if (ep2 < 0)
				return FALSE;
			DisconnectTracks( trk, 1-ep, trk2, ep2 );
			LOG( log_track, 2, ( "    at endPt with T%d[%d]\n", trk2->index, ep2 ) )
			DrawNewTrack( trk2 );
#ifdef LATER
			*trk = trk2;
			*ep = ep1;
			*leftover = trk;
#endif
		} else {
#ifdef LATER
			*trk = NULL;
#endif
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
				ConnectTracks( trk, ep, trkl, ep0 );
			ep0 = 1-ep0;
			while ( 1 ) {
				CopyAttributes( trk, trkl );
				ClrTrkElev( trkl );
				trk0 = GetTrkEndTrk(trkl,ep0);
				if ( trk0 == NULL )
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
				if ( trk0 == NULL || trk0 == trk2 )
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
		ep = GetEndPtConnectedToMe( trvTrk->trk, oldTrk );
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
	return TrimTrack( *trk, *ep, dist1 );
}


EXPORT BOOL_T TrimTrack( track_p trk, EPINX_T ep, DIST_T dist )
{
	if (trackCmds(trk->type)->trim)
		return trackCmds(trk->type)->trim( trk, ep, dist );
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
		ASSERT( FALSE ); /* CHECKME */
#ifdef LATER
		switch ( inx ) {
		case PARAMS_1ST_JOIN:
		case PARAMS_2ND_JOIN:
			ErrorMessage( MSG_JOIN_TRK, (inx==PARAMS_1ST_JOIN?_("First"):_("Second")) );
			break;
		case PARAMS_EXTEND:
			ErrorMessage( MSG_CANT_EXTEND );
			break;
		case PARAMS_PARALLEL:
			ErrorMessage( MSG_INV_TRK_PARALLEL );
			break;
		default:
			ErrorMessage( MSG_INVALID_TRK );
		}
#endif
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
		if (d>1000000.0)
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
			d /= 2.0;
		return d;
	} else {
		pos0 = GetTrkEndPos(trk,ep0);
		if (ep1==-1) {
			pos1.x = (trk->hi.x+trk->lo.x)/2.0;
			pos1.y = (trk->hi.y+trk->lo.y)/2.0;
		} else {
			pos1 = GetTrkEndPos(trk,ep1);
		}
		pos1.x -= pos0.x;
		pos1.y -= pos0.y;
		Rotate( &pos1, zero, -GetTrkEndAngle(trk,ep0) );
		return fabs(pos1.y);
	}
}
#endif
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

EXPORT void DrawTie(
		drawCmd_p d,
		coOrd pos,
		ANGLE_T angle,
		DIST_T length,
		DIST_T width,
		wDrawColor color,
		BOOL_T solid )
{
	coOrd p[4], lo, hi;

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
#ifdef LATER
	lo = hi = p[0];
	for ( i=1; i<4; i++ ) {
		if ( p[i].x < lo.x ) lo.x = p[i].x;
		if ( p[i].y < lo.y ) lo.y = p[i].y;
		if ( p[i].x > hi.x ) hi.x = p[i].x;
		if ( p[i].y > hi.y ) hi.y = p[i].y;
	}
#endif
	if ( d == &mainD ) {
		lo.x -= RBORDER/mainD.dpi*mainD.scale;
		lo.y -= TBORDER/mainD.dpi*mainD.scale;
		hi.x += LBORDER/mainD.dpi*mainD.scale;
		hi.y += BBORDER/mainD.dpi*mainD.scale;
		if ( OFF_D( d->orig, d->size, lo, hi ) )
			return;
	}
	if ( solid ) {
		DrawFillPoly( d, 4, p, color );
	} else {
		DrawLine( d, p[0], p[1], 0, color );
		DrawLine( d, p[1], p[2], 0, color );
		DrawLine( d, p[2], p[3], 0, color );
		DrawLine( d, p[3], p[0], 0, color );
	}
}


EXPORT void DrawCurvedTies(
		drawCmd_p d,
		track_p trk,
		coOrd p,
		DIST_T r,
		ANGLE_T a0,
		ANGLE_T a1,
		wDrawColor color )
{
	tieData_p td = GetScaleTieData(GetTrkScale(trk));
	DIST_T len;
	ANGLE_T ang, dang;
	coOrd pos;
	int cnt;

	if ( (d->funcs->options&wDrawOptTemp) != 0 )
		return;
	if ( trk == NULL )
		return;
	if ( (!GetTrkVisible(trk)) && drawTunnel!=DRAW_TUNNEL_SOLID )
		return;
	if (color == wDrawColorBlack)
		color = tieColor;
	len = 2*M_PI*r*a1/360.0;
	cnt = (int)(len/td->spacing);
	if ( len-td->spacing*cnt-td->width > (td->spacing-td->width)/2 )
		cnt++;
	if ( cnt != 0 ) {
		dang = a1/cnt;
		for ( ang=a0+dang/2; cnt; cnt--,ang+=dang ) {
			PointOnCircle( &pos, p, r, ang );
			DrawTie( d, pos, ang+90, td->length, td->width, color, tieDrawMode==TIEDRAWMODE_SOLID );
		}
	}
}


EXPORT void DrawCurvedTrack(
		drawCmd_p d,
		coOrd p,
		DIST_T r,
		ANGLE_T a0,
		ANGLE_T a1,
		coOrd p0,
		coOrd p1,
		track_p trk,
		DIST_T trackGauge,
		wDrawColor color,
		long options )
{
	DIST_T scale2rail;
	wDrawWidth width=0;
	trkSeg_p segPtr;

	if ( (d->options&DC_SEGTRACK) ) {
		DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
		segPtr = &tempSegs(tempSegs_da.cnt-1);
		segPtr->type = SEG_CRVTRK;
		segPtr->width = 0;
		segPtr->color = wDrawColorBlack;
		segPtr->u.c.center = p;
		segPtr->u.c.a0 = a0;
		segPtr->u.c.a1 = a1;
		segPtr->u.c.radius = r;
		return;
	}

	scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;
	if (options&DTS_THICK2)
		width = 2;
	if (options&DTS_THICK3)
		width = 3;
#ifdef WINDOWS
	width *= (wDrawWidth)(d->dpi/mainD.dpi);
#else
	if (d->options&DC_PRINT)
		width *= 300/75;
#endif

LOG( log_track, 4, ( "DST( (%0.3f %0.3f) R%0.3f A%0.3f..%0.3f)\n",
				p.x, p.y, r, a0, a1 ) )
	if ( (options&DTS_TIES) != 0 && trk &&
		 tieDrawMode!=TIEDRAWMODE_NONE &&
		 d!=&mapD &&
		 (d->options&DC_TIES)!=0 &&
		 d->scale<scale2rail/2 )
		DrawCurvedTies( d, trk, p, r, a0, a1, color );
	if (color == wDrawColorBlack)
		color = normalColor;
	if ( d->scale >= scale2rail ) {
		DrawArc( d, p, r, a0, a1, ((d->scale<32) && centerDrawMode && !(options&DTS_NOCENTER)) ? 1 : 0, width, color );
	} else if (d->options & DC_QUICK) {
		DrawArc( d, p, r, a0, a1, ((d->scale<32) && centerDrawMode && !(options&DTS_NOCENTER)) ? 1 : 0, 0, color );
	} else {
		if ( (d->scale <= 1 && (d->options&DC_SIMPLE)==0) || (d->options&DC_CENTERLINE)!=0 ) {
			long options = d->options;
			d->options |= DC_DASH;
			DrawArc( d, p, r, a0, a1, 0, 0, color );
			d->options = options;
		}
		DrawArc( d, p, r+trackGauge/2.0, a0, a1, 0, width, color );
		DrawArc( d, p, r-trackGauge/2.0, a0, a1, (centerDrawMode && !(options&DTS_NOCENTER) ? 1: 0), width, color );
		if ( (d->options&DC_PRINT) && roadbedWidth > trackGauge && d->scale <= scale2rail/2 ) {
			 wDrawWidth rbw = (wDrawWidth)floor(roadbedLineWidth*(d->dpi/d->scale)+0.5);
			 if ( options&DTS_RIGHT ) {
				DrawArc( d, p, r+roadbedWidth/2.0, a0, a1, 0, rbw, color );
			 }
			 if ( options&DTS_LEFT ) {
				DrawArc( d, p, r-roadbedWidth/2.0, a0, a1, 0, rbw, color );
			 }
		}
	}
}


EXPORT void DrawStraightTies(
		drawCmd_p d,
		track_p trk,
		coOrd p0,
		coOrd p1,
		wDrawColor color )
{
	tieData_p td = GetScaleTieData(GetTrkScale(trk));
	DIST_T tieOff0=0.0, tieOff1=0.0;
	DIST_T len, dlen;
	coOrd pos;
	int cnt;
	ANGLE_T angle;

	if ( (d->funcs->options&wDrawOptTemp) != 0 )
		return;
	if ( trk == NULL )
		return;
	if ( (!GetTrkVisible(trk)) && drawTunnel!=DRAW_TUNNEL_SOLID )
		return;
	if ( color == wDrawColorBlack )
		color = tieColor;
	td = GetScaleTieData( GetTrkScale(trk) );
	len = FindDistance( p0, p1 );
	len -= tieOff0+tieOff1;
	angle = FindAngle( p0, p1 );
	cnt = (int)(len/td->spacing);
	if ( len-td->spacing*cnt-td->width > (td->spacing-td->width)/2 )
		cnt++;
	if ( cnt != 0 ) {
		dlen = len/cnt;
		for ( len=dlen/2; cnt; cnt--,len+=dlen ) {
			Translate( &pos, p0, angle, len );
			DrawTie( d, pos, angle, td->length, td->width, color, tieDrawMode==TIEDRAWMODE_SOLID );
		}
	}
}


EXPORT void DrawStraightTrack(
		drawCmd_p d,
		coOrd p0,
		coOrd p1,
		ANGLE_T angle,
		track_p trk,
		DIST_T trackGauge,
		wDrawColor color,
		long options )
{
	coOrd pp0, pp1;
	DIST_T scale2rail;
	wDrawWidth width=0;
	trkSeg_p segPtr;

	if ( (d->options&DC_SEGTRACK) ) {
		DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
		segPtr = &tempSegs(tempSegs_da.cnt-1);
		segPtr->type = SEG_STRTRK;
		segPtr->width = 0;
		segPtr->color = wDrawColorBlack;
		segPtr->u.l.pos[0] = p0;
		segPtr->u.l.pos[1] = p1;
		segPtr->u.l.angle = angle;
		segPtr->u.l.option = 0;
		return;
	}

	scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;

	if (options&DTS_THICK2)
		width = 2;
	if (options&DTS_THICK3)
		width = 3;
#ifdef WINDOWS
	width *= (wDrawWidth)(d->dpi/mainD.dpi);
#else
	if (d->options&DC_PRINT)
		width *= 300/75;
#endif
LOG( log_track, 4, ( "DST( (%0.3f %0.3f) .. (%0.3f..%0.3f)\n",
				p0.x, p0.y, p1.x, p1.y ) )
	if ( (options&DTS_TIES) != 0 && trk &&
		 tieDrawMode!=TIEDRAWMODE_NONE &&
		 d!=&mapD &&
		 (d->options&DC_TIES)!=0 &&
		 d->scale<scale2rail/2 )
		DrawStraightTies( d, trk, p0, p1, color );
	if (color == wDrawColorBlack)
		color = normalColor;
	if ( d->scale >= scale2rail ) {
		DrawLine( d, p0, p1, width, color );
	} else if (d->options&DC_QUICK) {
		DrawLine( d, p0, p1, 0, color );
	} else {
		if ( (d->scale <= 1 && (d->options&DC_SIMPLE)==0) || (d->options&DC_CENTERLINE)!=0 ) {
			long options = d->options;
			d->options |= DC_DASH;
			DrawLine( d, p0, p1, 0, color );
			d->options = options;
		}
		Translate( &pp0, p0, angle+90, trackGauge/2.0 );
		Translate( &pp1, p1, angle+90, trackGauge/2.0 );
		DrawLine( d, pp0, pp1, width, color );
		Translate( &pp0, p0, angle-90, trackGauge/2.0 );
		Translate( &pp1, p1, angle-90, trackGauge/2.0 );
		DrawLine( d, pp0, pp1, width, color );
		if ( (d->options&DC_PRINT) && roadbedWidth > trackGauge && d->scale <= scale2rail/2.0) {
			 wDrawWidth rbw = (wDrawWidth)floor(roadbedLineWidth*(d->dpi/d->scale)+0.5);
			 if ( options&DTS_RIGHT ) {
				Translate( &pp0, p0, angle+90, roadbedWidth/2.0 );
				Translate( &pp1, p1, angle+90, roadbedWidth/2.0 );
				DrawLine( d, pp0, pp1, rbw, color );
			 }
			 if ( options&DTS_LEFT ) {
				Translate( &pp0, p0, angle-90, roadbedWidth/2.0 );
				Translate( &pp1, p1, angle-90, roadbedWidth/2.0 );
				DrawLine( d, pp0, pp1, rbw, color );
			 }
		}
	}
}


EXPORT wDrawColor GetTrkColor( track_p trk, drawCmd_p d )
{
	DIST_T len, elev0, elev1;
	ANGLE_T grade = 0.0;

	if ( IsTrack( trk ) && GetTrkEndPtCnt(trk) == 2 ) {
		len = GetTrkLength( trk, 0, 1 );
		if (len>0.1) {
			ComputeElev( trk, 0, FALSE, &elev0, NULL );
			ComputeElev( trk, 1, FALSE, &elev1, NULL );
			grade = fabs( (elev1-elev0)/len )*100.0;
		}
	}
	if ( (d->options&(DC_GROUP)) == 0 ) {
		if ( grade > maxTrackGrade )
			return exceptionColor;
		if ( QueryTrack( trk, Q_EXCEPTION ) )
			return exceptionColor;
	}
	if ( (d->options&(DC_PRINT|DC_GROUP)) == 0 ) {
		if (GetTrkBits(trk)&TB_PROFILEPATH)
			return profilePathColor;
		if ((d->options&DC_PRINT)==0 && GetTrkSelected(trk))
			return selectedColor;
	}
	if ( (d->options&(DC_GROUP)) == 0 ) {
		if ( (IsTrack(trk)?(colorLayers&1):(colorLayers&2)) )
			return GetLayerColor((LAYER_T)curTrackLayer);
	}
	return wDrawColorBlack;
}


EXPORT void DrawTrack( track_cp trk, drawCmd_p d, wDrawColor color )
{
	DIST_T scale2rail;
	TRKTYP_T trkTyp;

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
	}
	if (d == &mapD && !GetLayerOnMap(curTrackLayer))
		return;
	if ( (IsTrack(trk)?(colorLayers&1):(colorLayers&2)) &&
		d != &mapD && color == wDrawColorBlack )
		color = GetLayerColor((LAYER_T)curTrackLayer);
	scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;
	if ( (!inDrawTracks) &&
		 tieDrawMode!=TIEDRAWMODE_NONE &&
		 d != &mapD &&
		 d->scale<scale2rail/2 &&
		 QueryTrack(trk, Q_ISTRACK) &&
		 (GetTrkVisible(trk) || drawTunnel==DRAW_TUNNEL_SOLID) ) {
		d->options |= DC_TIES;
	}
	trackCmds(trkTyp)->draw( trk, d, color );
	if ( (!inDrawTracks) ) {
		d->options &= ~DC_TIES;
	}
	d->options &= ~DC_DASH;

	DrawTrackElev( trk, d, color!=wDrawColorWhite );
}


static void DrawATrack( track_cp trk, wDrawColor color )
{
	DrawMapBoundingBox( FALSE );
	DrawTrack( trk, &mapD, color );
	DrawTrack( trk, &mainD, color );
	DrawMapBoundingBox( TRUE );
}


EXPORT void DrawNewTrack( track_cp t )
{
	DrawATrack( t, wDrawColorBlack );
}

EXPORT void UndrawNewTrack( track_cp t )
{
	DrawATrack( t, wDrawColorWhite );
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
		if (d->scale < 8) {
			Translate( &p, p, a+90.0, 0.2 );
			Translate( &p0, p, a, trackGauge );
			Translate( &p1, p, a-180.0, trackGauge );
			DrawLine( d, p0, p1, 0, color );
		}
}


EXPORT void DrawEndElev( drawCmd_p d, track_p trk, EPINX_T ep, wDrawColor color )
{
	coOrd pp;
	wFont_p fp;
	elev_t * elev;
	track_p trk1;
	DIST_T elev0, grade;
	ANGLE_T a=0;
	int style = BOX_BOX;
	BOOL_T gradeOk = TRUE;
	char *elevStr;

	if ((labelEnable&LABELENABLE_ENDPT_ELEV)==0)
		return;
	elev = &trk->endPt[ep].elev;		/* TRACKDEP */
	if ( (elev->option&ELEV_MASK)==ELEV_NONE ||
		 (elev->option&ELEV_VISIBLE)==0 )
		return;
	if ( (trk1=GetTrkEndTrk(trk,ep)) && GetTrkIndex(trk1)<GetTrkIndex(trk) )
		return;

	fp = wStandardFont( F_HELV, FALSE, FALSE );
	pp = GetTrkEndPos( trk, ep );
	switch ((elev->option&ELEV_MASK)) {
	case ELEV_COMP:
	case ELEV_GRADE:
		if ( color == wDrawColorWhite ) {
			elev0 = grade = elev->u.height;
		} else if ( !ComputeElev( trk, ep, FALSE, &elev0, &grade ) ) {
			elev0 = grade = 0;
			gradeOk = FALSE;
		}
		if ((elev->option&ELEV_MASK)==ELEV_COMP) {
			elevStr = FormatDistance(elev0);
			elev->u.height = elev0;
		} else if (gradeOk) {
			sprintf( message, "%0.1f%%", fabs(grade*100.0) );
			elevStr = message;
			a = GetTrkEndAngle( trk, ep );
			style = BOX_ARROW;
			if (grade <= -0.001)
				a = NormalizeAngle( a+180.0 );
			else if ( grade < 0.001 )
				style = BOX_BOX;
			elev->u.height = grade;
		} else {
			elevStr = "????%%";
		}
		break;
	case ELEV_DEF:
		elevStr = FormatDistance( elev->u.height);
		break;
	case ELEV_STATION:
		elevStr = elev->u.name;
		break;
	default:
		return;
	}
	pp.x += elev->doff.x;
	pp.y += elev->doff.y;
	DrawBoxedString( style, d, pp, elevStr, fp, (wFontSize_t)descriptionFontSize, color, a );
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
		wDrawColor color )
{
	coOrd p, pp;
	ANGLE_T a;
	track_p trk1;
	coOrd p0, p1, p2;
	BOOL_T sepBoundary;
	DIST_T trackGauge;
	wDrawWidth width;
	wDrawWidth width2;

	// line width for the tunnel portal, make sure it is rounded correctly
	width2 = (wDrawWidth)round((2.0 * d->dpi)/75.0);
	if (d->funcs->options&wDrawOptTemp)
		return;
	if ( trk && QueryTrack( trk, Q_NODRAWENDPT ) )
		return;

	if (trk == NULL || ep < 0)
		return;

	if (color == wDrawColorBlack)
		color = normalColor;

	if (labelScale >= d->scale)
		DrawEndElev( d, trk, ep, color );

	if ( d->scale >= ((d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale) )
		return;

	trk1 = GetTrkEndTrk(trk,ep);
	pp = p = GetTrkEndPos( trk, ep );
	a = GetTrkEndAngle( trk, ep ) + 90.0;

	trackGauge = GetTrkGauge(trk);
	if (trk1 == NULL) {
		DrawUnconnectedEndPt( d, p, a, trackGauge, color );
		return;
	}

	sepBoundary = FALSE;
	if ((d->options&DC_PRINT)==0 && importTrack == NULL && GetTrkSelected(trk) && (!GetTrkSelected(trk1))) {
		DIST_T len;
		len = trackGauge*2.0;
		if (len < 0.10*d->scale)
			len = 0.10*d->scale;
		Translate( &p0, p, a+45, len );
		Translate( &p1, p, a+225, len );
		DrawLine( &tempD, p0, p1, 0, selectedColor );
		Translate( &p0, p, a-45, len );
		Translate( &p1, p, a-225, len );
		DrawLine( &tempD, p0, p1, 0, selectedColor );
		sepBoundary = TRUE;
	} else if ((d->options&DC_PRINT)==0 && importTrack == NULL && (!GetTrkSelected(trk)) && GetTrkSelected(trk1)) {
		sepBoundary = TRUE;
	}

	// is the endpoint a transition into a tunnel?
	if (GetTrkVisible(trk) && (!GetTrkVisible(trk1))) {
		// yes, draw tunnel portal
		Translate( &p0, p, a, trackGauge );
		Translate( &p1, p, a+180, trackGauge );
		DrawLine( d, p0, p1, width2, color );
		Translate( &p2, p0, a+45, trackGauge/2.0 );
		DrawLine( d, p0, p2, width2, color );
		Translate( &p2, p1, a+135, trackGauge/2.0 );
		DrawLine( d, p1, p2, width2, color );
		if ( d == &mainD ) {
			width = (wDrawWidth)ceil(trackGauge*d->dpi/2.0/d->scale);
			if ( width > 1 ) {
				if ( (GetTrkEndOption(trk,ep)&EPOPT_GAPPED) != 0 ) {
					Translate( &p0, p, a, trackGauge );
					DrawLine( d, p0, p, width, color );
				}
				trk1 = GetTrkEndTrk(trk,ep);
				if ( trk1 ) {
					ep = GetEndPtConnectedToMe( trk1, trk );
					if ( (GetTrkEndOption(trk1,ep)&EPOPT_GAPPED) != 0 ) {
						Translate( &p0, p, a+180.0, trackGauge );
						DrawLine( d, p0, p, width, color );
					}
				}
			}
		}
	} else if ((!GetTrkVisible(trk)) && GetTrkVisible(trk1)) {
		;
	} else if ( GetLayerVisible( GetTrkLayer( trk ) ) && !GetLayerVisible( GetTrkLayer( trk1 ) ) ) {
		a -= 90.0;
		Translate( &p, p, a, trackGauge/2.0 );
		Translate( &p0, p, a-135.0, trackGauge*2.0 );
		DrawLine( d, p0, p, width2, color );
		Translate( &p0, p, a+135.0, trackGauge*2.0 );
		DrawLine( d, p0, p, width2, color );
	} else if ( !GetLayerVisible( GetTrkLayer( trk ) ) && GetLayerVisible( GetTrkLayer( trk1 ) ) ) {
		;
	} else if ( sepBoundary ) {
		;
	} else if ( (drawEndPtV == 1 && (QueryTrack(trk,Q_DRAWENDPTV_1) || QueryTrack(trk1,Q_DRAWENDPTV_1)) ) ||
				(drawEndPtV == 2) ) {
		Translate( &p0, p, a, trackGauge );
		width = 0;
		if ( d != &mapD && d != &tempD && (GetTrkEndOption(trk,ep)&EPOPT_GAPPED) != 0 )
			width = (wDrawWidth)ceil(trackGauge*d->dpi/2.0/d->scale);
		DrawLine( d, p0, p, width, color );
	} else {
		;
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
	wIndex_t count;
	coOrd lo, hi;
	BOOL_T doSelectRecount = FALSE;
	
	inDrawTracks = TRUE;
	count = 0;
	InfoCount( 0 );
	count = 0;
	d->options |= DC_TIES;
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
	d->options &= ~DC_TIES;

	if (d == &mainD) {
		for (inx=1; inx<trackCmds_da.cnt; inx++)
			if (trackCmds(inx)->redraw != NULL)
				trackCmds(inx)->redraw();
	}
	InfoCount( trackCount );
	inDrawTracks = FALSE;
	if ( doSelectRecount )
		SelectRecount();
}


EXPORT void RedrawLayer( LAYER_T l, BOOL_T draw )
{
	MainRedraw();
#ifdef LATER
	track_cp trk;
	track_cp trk1;
	EPINX_T ep;
	wIndex_t count;
	coOrd hi, lo;

	count = 0;
	InfoCount( 0 );
	TRK_ITERATE( trk ) {
		if (GetTrkLayer(trk) != l)
			continue;
		GetBoundingBox( trk, &hi, &lo );
		if ( !OFF_MAIND( lo, hi ) ) {
			if ( GetLayerVisible( l ) ) {
				DrawTrack( trk, &mainD, draw?wDrawColorBlack:wDrawColorWhite );
			}
			for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
				trk1 = GetTrkEndTrk( trk, ep );
				if ( trk1 && GetTrkLayer(trk1) != l && GetLayerVisible(GetTrkLayer(trk1)) ) {
					DrawEndPt( &mainD, trk1, GetEndPtConnectedToMe( trk1, trk ),
							draw?wDrawColorBlack:wDrawColorWhite );
				}
			}
			count++;
			if (count%10 == 0)
				InfoCount( count );
		}
		if (draw)
			ClrTrkBits( trk, TB_SELECTED );
	}
	InfoCount( trackCount );
	SelectRecount();
#endif
}


EXPORT void DrawSelectedTracks( drawCmd_p d )
{
	track_cp trk;
	wIndex_t count;

	count = 0;
	InfoCount( 0 );

	TRK_ITERATE( trk ) {
		if ( GetTrkSelected( trk ) ) {
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
	static long lastRedraw = -1;
	static BOOL_T lastHilight = FALSE;
	track_p trk, trk1;
	EPINX_T ep;
	int mode;
	DIST_T elev;
	coOrd pos;
	DIST_T radius;

	if (currRedraw > lastRedraw) {
		lastRedraw = currRedraw;
		lastHilight = FALSE;
	}
	if (lastHilight == hilight)
		return;
	radius = 0.05*mainD.scale;
	if ( radius < trackGauge/2.0 )
		radius = trackGauge/2.0;
	TRK_ITERATE( trk ) {
		for (ep=0;ep<GetTrkEndPtCnt(trk);ep++) {
			GetTrkEndElev( trk, ep, &mode, &elev );		/* TRACKDEP */
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
	lastHilight = hilight;
}


EXPORT void HilightSelectedEndPt( BOOL_T show, track_p trk, EPINX_T ep )
{
	static BOOL_T lastShow = FALSE;
	static long lastRedraw = -1;
	coOrd pos;
	if (trk == NULL)
		return;
	if (currRedraw > lastRedraw) {
		lastRedraw = currRedraw;
		lastShow = FALSE;
	}
	if (lastShow != show) {
		pos = GetTrkEndPos( trk, ep );
		DrawFillCircle( &tempD, pos, 0.10*mainD.scale, selectedColor );
		lastShow = show;
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
