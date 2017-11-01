/** \file elev.c
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <math.h>

#include "ccurve.h"
#include "cundo.h"
#include "messages.h"
#include "param.h"
#include "shrtpath.h"
#include "track.h"
#include "utility.h"
#include "string.h"

EXPORT long oldElevationEvaluation = 0;
static int log_fillElev = 0;
static int log_dumpElev = 0;
static BOOL_T log_fillElev_initted;
static int checkTrk = 0;
static char * elevPrefix;

static void SetTrkOnElevPath( track_p trk, int mode, DIST_T elev );

typedef struct {
		track_p trk;
		EPINX_T ep;
		DIST_T len;
		} elist_t;
static dynArr_t elist_da;
#define elist(N) DYNARR_N( elist_t, elist_da, N )
#define elistAppend( T, E, L ) \
		{ DYNARR_APPEND( elist_t, elist_da, 10 );\
		  elist(elist_da.cnt-1).trk = T; elist(elist_da.cnt-1).len = L; elist(elist_da.cnt-1).ep = E; }

EPINX_T GetNextTrkOnPath( track_p trk, EPINX_T ep )
{
/* Get next track on Path:
   1 - there is only 1 connected (not counting ep)
   2 - there are >1 but only 1 on Path
   3 - one of them is PST:PSE or PET:PEE
*/
	EPINX_T ep2;
	track_p trkN;
	int epCnt = GetTrkEndPtCnt(trk);

	for ( ep2=0; ep2<epCnt; ep2++) {
		if ( ep2 == ep )
			continue;
		trkN = GetTrkEndTrk(trk,ep2);
		if (trkN && (GetTrkBits(trkN)&TB_PROFILEPATH))
			return ep2;
	}
	return -1;
}


EXPORT int FindDefinedElev(
		track_p trk,
		EPINX_T ep,
		int dir,
		BOOL_T onpath,
		DIST_T * Relev,
		DIST_T *Rdist )
{
	track_p trk0, trk1;
	EPINX_T ep0, ep1, ep2;
	DIST_T dist=0.0;

	if (dir) {
		trk1 = GetTrkEndTrk( trk, ep );
		if (trk1 == NULL)
			return FDE_END;
		ep = GetEndPtConnectedToMe( trk1, trk );
		trk = trk1;
	}
	trk0 = trk;
	ep0 = ep;
	while (1) {
		for (ep2=0; ep2<GetTrkEndPtCnt(trk); ep2++) {
			if ((trk!=trk0||ep2!=ep0)&&
				EndPtIsDefinedElev(trk,ep2) ) {
				dist += GetTrkLength( trk, ep, ep2 );
				*Relev = GetTrkEndElevHeight(trk,ep2);
				*Rdist = dist;
				return FDE_DEF;
			}
		}
		if (onpath) {
			ep2 = GetNextTrkOnPath( trk, ep );
			if ( ep2 >= 0 ) {
				trk1 = GetTrkEndTrk( trk, ep2 );
				ep1 = GetEndPtConnectedToMe( trk1, trk );
			} else {
				trk1 = NULL;
			}
		} else {
			ep2 = GetNextTrk( trk, ep, &trk1, &ep1, GNTignoreIgnore );
		}
		if (ep2 < 0) {
			if (trk1) {
				/* more than one branch */
				return FDE_UDF;
			} else {
				/* end of the line */
				return FDE_END;
			}
		}
		dist += GetTrkLength( trk, ep, ep2 );
		trk = trk1;
		ep = ep1;
		if (trk == trk0)
			return FDE_UDF;
	}
}


BOOL_T ComputeElev(
		track_p trk,
		EPINX_T ep,
		BOOL_T onpath,
		DIST_T *elevR,
		DIST_T *gradeR )
{
	DIST_T grade;
	DIST_T elev0, elev1, dist0, dist1;
	BOOL_T rc = FALSE;
if (oldElevationEvaluation) {
	int rc0, rc1;
	if (GetTrkEndElevMode(trk,ep) == ELEV_DEF) {
		if (elevR)
			*elevR = GetTrkEndElevHeight(trk,ep);
		if (gradeR)
			*gradeR = 0.0;
		return TRUE;
	}
	rc0 = FindDefinedElev( trk, ep, 0, onpath, &elev0, &dist0 );
	rc1 = FindDefinedElev( trk, ep, 1, onpath, &elev1, &dist1 );
	if ( rc0 == FDE_DEF && rc1 == FDE_DEF ) {
		if (dist0+dist1 > 0.1)
			grade = (elev1-elev0)/(dist0+dist1);
		else
			grade = 0.0;
		elev0 += grade*dist0;
		rc = TRUE;
	} else if ( rc0 == FDE_DEF && rc1 == FDE_END ) {
		grade = 0.0;
		rc = TRUE;
	} else if ( rc1 == FDE_DEF && rc0 == FDE_END ) {
		grade = 0.0;
		elev0 = elev1;
		rc = TRUE;
	} else if ( rc0 == FDE_END && rc1 == FDE_END ) {
		grade = 0.0;
		elev0 = 0.0;
		rc = TRUE;
	} else {
		grade = 0.0;
		elev0 = 0.0;
	}
} else {
	track_p trk1;
	EPINX_T ep1;
	grade = -1;
	rc = TRUE;
	if ( EndPtIsDefinedElev(trk,ep) ) {
		elev0 = GetTrkEndElevHeight(trk,ep);
		rc = FALSE;
	} else {
		elev0 = GetElevation( trk );
		dist0 = GetTrkLength( trk, ep, -1 );
		trk1 = GetTrkEndTrk( trk, ep );
		if (trk1!=NULL) {
			ep1 = GetEndPtConnectedToMe(trk1,trk);
			elev1 = GetElevation( trk1 );
			dist1 = GetTrkLength( trk1, ep1, -1 );
			if (dist0+dist1>0.1) {
				grade = (elev1-elev0)/(dist0+dist1);
				elev0 += grade*dist0;
			} else {
				elev0 = (elev0+elev1)/2.0;
				rc = FALSE;
			}
		} else {
			grade = 0.0;
		}
	}
}
	if ( elevR )
		*elevR = elev0;
	if ( gradeR )
		*gradeR = grade;
	return rc;
}



/*
 * DYNAMIC ELEVATION COMPUTATION
 *
 * Step 1: Find DefElev's marking the border of the area of interest (or all of them)
 */

typedef struct {
		track_p trk;
		EPINX_T ep;
		DIST_T elev;
		} defelev_t;
static dynArr_t defelev_da;
#define defelev(N) DYNARR_N( defelev_t, defelev_da, N )

static void FindDefElev( void )
{
	track_p trk;
	EPINX_T ep, cnt;
	defelev_t * dep;
	long time0 = wGetTimer();

	DYNARR_RESET( defelev_t, defelev_da );
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		cnt = GetTrkEndPtCnt( trk );
		for ( ep = 0; ep < cnt; ep++ ) {
			if ( !EndPtIsDefinedElev( trk, ep ) )
				continue;
			DYNARR_APPEND( defelev_t, defelev_da, 10 );
			dep = &defelev( defelev_da.cnt-1 );
			dep->trk = trk;
			dep->ep = ep;
			dep->elev = GetTrkEndElevHeight(trk,ep);
		}
	}
LOG( log_fillElev, 1, ( "%s: findDefElev [%d] (%ld)\n", elevPrefix, defelev_da.cnt, wGetTimer()-time0 ) )
}


static int foundNewDefElev;
static void FindAttachedDefElev( track_p trk, BOOL_T remove )
/* Find all DefElev's attached (directly or not) to this trk
 *   if 'remove' then clr ELEVPATH bit
 * Working:
 *      elist_da
 * Outputs:
 *      defelev_da       - list of DefElev
 *      foundNewDefElev  - found a DefElev not already on ElevPath
 */
{
	int i1;
	track_p trk1;
	EPINX_T ep, cnt;
	defelev_t * dep;

	foundNewDefElev = FALSE;
	DYNARR_RESET( elist_t, elist_da );
	elistAppend( trk, 0, 0 );
	SetTrkBits( trk, TB_PROCESSED );
	if (remove)
		ClrTrkBits( trk, TB_ELEVPATH );
	for ( i1=0; i1<elist_da.cnt; i1++ ) {
		trk = elist(i1).trk;
		if (!IsTrack(trk))
			continue;
		cnt = GetTrkEndPtCnt(trk);
		for ( ep=0; ep<cnt; ep++ ) {
			 if ( EndPtIsDefinedElev(trk,ep) ) {
				DYNARR_APPEND( defelev_t, defelev_da, 10 );
				dep = &defelev( defelev_da.cnt-1 );
				dep->trk = trk;
				dep->ep = ep;
				dep->elev = GetTrkEndElevHeight(trk,ep);
				if ( !(GetTrkBits(trk)&TB_ELEVPATH) ) {
					foundNewDefElev = TRUE;
				}
			} else {
				trk1 = GetTrkEndTrk(trk,ep);
				if (trk1) {
#ifdef LATER
					if ( defElevOnIsland == FALSE && (GetTrkBits(trk1)&TB_ELEVPATH) ) {
						/* so far this island is only connected to forks */
						DYNARR_APPEND( fork_t, fork_da, 10 );
						n = &fork(fork_da.cnt-1);
						n->trk = trk1;
						n->ep = ep;
						n->dist = dist;
						n->elev = dep->elev;
						n->ntrk = dep->trk;
						n->nep = dep->ep;
					}
#endif
					if ( !(GetTrkBits(trk1)&TB_PROCESSED) ) {
						elistAppend( trk1, 0, 0 );
						SetTrkBits( trk1, TB_PROCESSED );
						if (remove)
							ClrTrkBits( trk1, TB_ELEVPATH );
					}
				}
			}
		}
	}
}

static int FindObsoleteElevs( void )
{
	track_p trk;
	int cnt;
	long time0 = wGetTimer();

	ClrAllTrkBits( TB_PROCESSED );
	DYNARR_RESET( defelev_t, defelev_da );
	cnt = 0;
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if ( (!(GetTrkBits(trk)&(TB_ELEVPATH|TB_PROCESSED))) && IsTrack(trk) ) {
			cnt++;
			FindAttachedDefElev( trk, TRUE );
		}
	}
LOG( log_fillElev, 1, ( "%s: findObsoleteElevs [%d] (%ld)\n", elevPrefix, cnt, wGetTimer()-time0 ) )
	return cnt;
}




/*
 * DYNAMIC ELEVATION COMPUTATION
 *
 * Step 2: Find Forks on Shortest Path between the DefElev's found in Step 1
 */

typedef struct {
		track_p trk;
		EPINX_T ep;
		EPINX_T ep2;
		DIST_T dist;
		DIST_T elev;
		} fork_t;
static dynArr_t fork_da;
#define fork(N) DYNARR_N( fork_t, fork_da, N )

static int FillElevShortestPathFunc(
		SPTF_CMD cmd,
		track_p trk,
		EPINX_T ep,
		EPINX_T ep2,
		DIST_T dist,
		void * data )
{
	defelev_t * dep = (defelev_t *)data;
	track_p trk1;
	EPINX_T ep1, cnt, epRc;
	fork_t * n;

	switch ( cmd ) {
	case SPTC_MATCH:
		/*if ( (GetTrkBits(trk)&TB_PROCESSED) )
			epRc = 0;
		else*/ if ( (dep->trk!=trk || dep->ep!=ep) && EndPtIsDefinedElev(trk,ep))
			epRc = 1;
		else
			epRc = 0;
		break;

	case SPTC_MATCHANY:
		cnt = GetTrkEndPtCnt( trk );
		epRc = -1;
		/*if ( (GetTrkBits(trk)&TB_PROCESSED) )
			break;*/
		for ( ep1=0; ep1<cnt; ep1++ ) {
			if ( ep != ep1 && EndPtIsDefinedElev( trk, ep1 ) ) {
				epRc = ep1;
				break;
			}
		}
		break;

	case SPTC_ADD_TRK:
		if (!(GetTrkBits(trk)&TB_PROCESSED)) {
			SetTrkBits(trk, TB_PROCESSED);
			if ( EndPtIsDefinedElev( trk, ep ) ) {
if (log_shortPath<=0||logTable(log_shortPath).level<4) LOG( log_fillElev, 5, ( "    ADD_TRK: T%d:%d D=%0.1f -> DefElev\n", GetTrkIndex(trk), ep, dist ) )
LOG( log_shortPath, 4, ( "DefElev " ) )
			} else if ( GetNextTrk( trk, ep, &trk1, &ep1, GNTignoreIgnore ) < 0 && trk1 != NULL ) {
if (log_shortPath<=0||logTable(log_shortPath).level<4) LOG( log_fillElev, 4, ( "    ADD_TRK: T%d:%d D=%0.3f E=%0.3f -> Fork[%d]\n", GetTrkIndex(trk), ep, dist, dep->elev, fork_da.cnt ) )
LOG( log_shortPath, 4, ( "E:%0.3f Fork[%d] ",  dep->elev, fork_da.cnt ) )
				DYNARR_APPEND( fork_t, fork_da, 10 );
				n = &fork(fork_da.cnt-1);
				n->trk = trk;
				n->ep = ep;
				n->ep2 = ep2;
				n->dist = dist;
				n->elev = dep->elev;
#ifdef LATER
				n->ntrk = dep->trk;
				n->nep = dep->ep;
#endif
			} else {
LOG( log_shortPath, 4, ( "Normal " ) )
			}
		} else {
LOG( log_shortPath, 4, ( "Processed " ) )
		}
		return 0;

	case SPTC_TERMINATE:
		epRc = 0;
		break;

	case SPTC_IGNNXTTRK:
		if ( EndPtIsIgnoredElev(trk,ep2) ) {
LOG( log_shortPath, 4, ( "2 Ignore " ) )
			epRc = 1;
		} else if ( (!EndPtIsDefinedElev(trk,ep)) && GetTrkEndTrk(trk,ep)==NULL ) {
LOG( log_shortPath, 4, ( "1 Null && !DefElev " ) )
			epRc = 1;
		} else {
			epRc = 0;
		}
		break;

	case SPTC_VALID:
		epRc = (GetTrkBits(trk)&TB_PROCESSED)==0?1:0;
		break;

	default:
		epRc = 0;
		break;
	}
	return epRc;
}

static void FindForks( void )
/* Find the Shortest Path between all DevElev's (in defelev_da)
 *   and record all Forks (Turnouts with >2 connections) with distance to and elevation of the DefElev
 * Inputs:
 *      defelev_da      - list of DefElev to consider
 * Outputs:
 *      fork_da         - list of distances btw Forks and DefElev (plus other info)
 */
{
	int i;
	defelev_t * dep;
	int rc;
	long time0 = wGetTimer();

	DYNARR_RESET( fork_t, fork_da );
	for ( i=0; i<defelev_da.cnt; i++ ) {
		dep = &defelev(i);
			
		ClrAllTrkBits( TB_PROCESSED );
LOG( log_fillElev, 3, ( "   findForks from T%d:%d\n", GetTrkIndex(dep->trk), dep->ep ) )
		rc = FindShortestPath( dep->trk, dep->ep, FALSE, FillElevShortestPathFunc, dep );
	}
	ClrAllTrkBits( TB_PROCESSED );
LOG( log_fillElev, 1, ( "%s: findForks [%d] (%ld)\n", elevPrefix, fork_da.cnt, wGetTimer()-time0 ) )
}




/*
 * DYNAMIC ELEVATION COMPUTATION
 *
 * Step 3: Compute the elevation of each Fork based on the weighted sum of the elevations
 *   of the DefElev's that it is connected to.
 */

typedef struct {
		DIST_T elev;
		DIST_T dist;
		} elevdist_t;
static dynArr_t elevdist_da;
#define elevdist(N) DYNARR_N( elevdist_t, elevdist_da, N );

static DIST_T ComputeWeightedElev( DIST_T totalDist )
/* Compute weighted elevation
 * Inputs:
 *      totalDist       - total distance of all tracks between DevElev's
 *      elevdist_da     - elev + dist to DefElev from current node
 * Outputs:
 *      elev (returned)
 */
{
	int i2;
	DIST_T d2;
	elevdist_t * w;
	DIST_T e;

	/* Compute inverse weighted Distance (d2) */
	d2 = 0;
	for ( i2=0; i2<elevdist_da.cnt; i2++ ) {
		w = &elevdist(i2);
		if (w->dist < 0.001) {
			e = w->elev;
LOG( log_fillElev, 3, ( "       computeWeightedElev: close! D%0.3f E%0.3f\n", w->dist, e ) )
			return e;
		}
		d2 += totalDist/w->dist;
	}

	/* Compute weighted elevation (e) */
	e = 0;
	for ( i2=0; i2<elevdist_da.cnt; i2++ ) {
		w = &elevdist(i2);
		e += ((totalDist/w->dist)/d2)*w->elev;
	}

		if (log_fillElev >= 4) {
			for ( i2=0; i2<elevdist_da.cnt; i2++ ) {
				w = &elevdist(i2);
				lprintf( "         E%0.3f D%0.3f\n", w->elev, w->dist );
			}
		}
LOG( log_fillElev, 3, ( "       computeWeightedElev: E%0.3f\n", e ) )
	return e;
}


static int forkCnt;
static void ComputeForkElev( void )
/* Compute elevations of all Forks
 * Inputs:
 *      fork_da         - fork distance/elev data (overwritten)
 * Outputs:
 *      fork_da         - just .trk used for forks
 *      forkCnt         - numer of forks found
 */
{
	int i1, i2;
	fork_t * n1, * n2, * n3;
	track_p trk, trk1;
	EPINX_T ep, cnt;
	DIST_T d1, e;
	elevdist_t * w;
	BOOL_T singlePath;
	long time0 = wGetTimer();

	forkCnt = 0;
	for (i1=0; i1<fork_da.cnt; i1++) {
		n1 = &fork(i1);
		if ((trk=n1->trk)) {
			cnt = GetTrkEndPtCnt(n1->trk);
			if (cnt<=0)
				continue;

			/* collect dist/elev to connected DefElev points */
			d1 = 0;
			DYNARR_RESET( elevdist_t, elevdist_da );
			singlePath = TRUE;
			for (i2=i1; i2<fork_da.cnt; i2++) {
				n2 = &fork(i2);
				if (trk == n2->trk) {
					DYNARR_APPEND( elevdist_t, elevdist_da, 10 );
					w = &elevdist(elevdist_da.cnt-1);
					w->dist = n2->dist;
					w->elev = n2->elev;
					w->dist += GetTrkLength( n2->trk, n2->ep, -1 );
					n2->trk = NULL;
					d1 += w->dist;
					if ( ! ( ( n1->ep == n2->ep && n1->ep2 == n2->ep2 ) ||
							 ( n1->ep == n2->ep2 && n1->ep2 == n2->ep ) ) )
						singlePath = FALSE;
				}
			}

			/* Also check my EPs */
			for (ep=0; ep<cnt; ep++) {
				if ( (trk1=GetTrkEndTrk(trk,ep)) )
					SetTrkBits( trk1, TB_PROCESSED );
				if (!EndPtIsDefinedElev(trk,ep))
					continue;
				for (i2=i1; i2<fork_da.cnt; i2++) {
					n2 = &fork(i2);
					if (trk==n2->trk && ep==n2->ep)
						break;
				}
				if (i2 >= fork_da.cnt) {
					DYNARR_APPEND( elevdist_t, elevdist_da, 10 );
					w = &elevdist(elevdist_da.cnt-1);
					w->elev = GetTrkEndElevHeight(trk,ep);
					w->dist = GetTrkLength( trk, ep, -1 );
					d1 += w->dist;
					singlePath = FALSE;
				}
			}

			n3 = &fork(forkCnt);
			n3->trk = trk;
			if ( singlePath == TRUE ) {
				/* only 2 EP are connected to DefElevs, treat other EPs as ignored */
				n3->ep = n1->ep;
				n3->ep2 = n1->ep2;
			} else {
				e = ComputeWeightedElev( d1 );
				SetTrkOnElevPath( trk, ELEV_FORK, e );
				/* 3 or more EPs are to DefElevs */
				n3->ep = -1;
LOG( log_fillElev, 2, ( "  1  T%d E%0.3f\n", GetTrkIndex(trk), e ) )
			}
			forkCnt++;
		}
	}
LOG( log_fillElev, 1, ( "%s: computeForkElev [%d] (%ld)\n", elevPrefix, forkCnt, wGetTimer()-time0 ) )
}




/*
 * DYNAMIC ELEVATION COMPUTATION
 *
 * Step 4: Compute the elevation of tracks on the Shortest Path between Forks and DefElev's
 */

static void RedrawCompGradeElev( track_p trk, EPINX_T ep )
{
	int mode;
	coOrd pos;
	track_p trk1;
	mode = GetTrkEndElevMode( trk, ep );
	if ( mode == ELEV_COMP || mode == ELEV_GRADE ) {
		pos = GetTrkEndPos( trk, ep );
		if (!OFF_MAIND( pos, pos ) ) {
			trk1 = GetTrkEndTrk( trk, ep );
			if ( (trk1=GetTrkEndTrk(trk,ep)) && GetTrkIndex(trk1)<GetTrkIndex(trk) )
{
				ep = GetEndPtConnectedToMe( trk1, trk );
				trk = trk1;
			}
			DrawEndElev( &mainD, trk, ep, wDrawColorWhite );
			DrawEndElev( &mainD, trk, ep, wDrawColorBlack );
		}
	}
}


static void PropogateForkElev(
		track_p trk1,
		EPINX_T ep1,
		DIST_T d1,
		DIST_T e )
/* Propogate elev from fork connection
 *   The track list starting from trk1:ep1 ends at a DefElev or a Fork
 * Inputs:
 * Working:
 *      elist_da
 * Outputs:
 *      Sets trk elev
 */
{
	DIST_T d2;
	DIST_T e1;
	EPINX_T ep2, epN, cnt2;
	track_p trkN;
	fork_t * n1;
	int i2, i3;

				DYNARR_RESET( elist_t, elist_da );
				while (trk1) {
					if ( GetTrkIndex(trk1) == checkTrk )
						printf( "found btw forks\n" );
					if ( GetTrkOnElevPath( trk1, &e1 ) ) {
						d1 += GetTrkLength( trk1, ep1, -1 );
						goto nextStep;
					}
					cnt2 = GetTrkEndPtCnt(trk1);
					for ( ep2=0; ep2<cnt2; ep2++ ) {
						if ( ep2!=ep1 && EndPtIsDefinedElev(trk1,ep2) ) {
							e1 = GetTrkEndElevHeight( trk1, ep2 );
							d2 = GetTrkLength( trk1, ep1, ep2 )/2.0;
							d1 += d2;
							elistAppend( trk1, ep1, d1 );
							d1 += d2;
							goto nextStep;
						}
					}
					ep2 = GetNextTrk( trk1, ep1, &trkN, &epN, GNTignoreIgnore );
					if ( ep2<0 ) {
						/* is this really a fork? */
						for ( i2=0; i2<forkCnt; i2++ ) {
							n1 = &fork(i2);
							if ( trk1 == n1->trk && n1->ep >= 0 ) {
								/* no: make sure we are on the path */
								if ( n1->ep == ep1 )
									ep2 = n1->ep2;
								else if ( n1->ep2 == ep1 )
									ep2 = n1->ep;
								else
									return;
								trkN = GetTrkEndTrk(trk1,ep2);
								epN = GetEndPtConnectedToMe( trkN, trk1 );
								break;
							}
						}
						if ( i2 >= forkCnt )
							return;
					}
					d2 = GetTrkLength( trk1, ep1, ep2 )/2.0;
					d1 += d2;
					elistAppend( trk1, ep1, d1 );
					d1 += d2;
					trk1 = trkN;
					ep1 = epN;
				}
nextStep:
				ASSERT(d1>0.0);
				e1 = (e1-e)/d1;
				trk1 = NULL;
				i3 = elist_da.cnt;
				for (i2=0; i2<elist_da.cnt; i2++) {
					trk1 = elist(i2).trk;
					ep1 = elist(i2).ep;
					if ( GetTrkOnElevPath( trk1, &e1 ) ) {
						i3=i2;
						break;
					}
					d2 = elist(i2).len;
					SetTrkOnElevPath( trk1, ELEV_BRANCH, e+e1*d2 );
LOG( log_fillElev, 2, ( "  2  T%d E%0.3f\n", GetTrkIndex(trk1), e+e1*d2 ) )
				}
				for (i2=0; i2<i3; i2++) {
					trk1 = elist(i2).trk;
					ep1 = elist(i2).ep;
					RedrawCompGradeElev( trk1, ep1 );
				}
}

static void PropogateForkElevs( void )
/* For all Forks, For all connections not already processed or not on ElevPath do
 *    propogate elev along connection
 * Inputs:
 *      fork_da         - list of forks
 *      forkCnt         - number of forks
 * Working:
 *      elist_da (by subrtn)
 * Outputs:
 *      Sets trk elev (by subrtn)
 */
{
	int i1;
	fork_t * n1;
	track_p trk, trk1;
	EPINX_T ep, cnt, ep1;
	DIST_T d1, e;
	long time0 = wGetTimer();

	/* propogate elevs between forks */
	for ( i1=0; i1<forkCnt; i1++ ) {
		n1 = &fork(i1);
		if ( n1->ep >= 0 )
			continue;
		trk = n1->trk;
		GetTrkOnElevPath( trk, &e );
		cnt = GetTrkEndPtCnt(trk);
		for (ep=0; ep<cnt; ep++) {
			trk1 = GetTrkEndTrk(trk,ep);
			if ( trk1 && (GetTrkBits(trk1)&TB_PROCESSED) && !GetTrkOnElevPath(trk1,NULL) ) {
				/* should find a fork with a computed elev */
				ep1 = GetEndPtConnectedToMe( trk1, trk );
				if ( EndPtIsDefinedElev(trk,ep) ) {
					PropogateForkElev( trk1, ep1, 0, GetTrkEndElevHeight(trk,ep) );
				} else {
					d1 = GetTrkLength(trk,ep,-1);
					PropogateForkElev( trk1, ep1, d1, e );
				}
			}
		}
	}
LOG( log_fillElev, 1, ( "%s: propogateForkElev (%ld)\n", elevPrefix, wGetTimer()-time0 ) )
}

static void PropogateDefElevs( void )
/* Propogate Elev from DefElev (if not handled already)
 * Inputs:
 *      develev_da      - list of DefElev
 * Outputs:
 *      Set trk elev
 */
{
	int i1;
	defelev_t * dep;
	DIST_T e;
	long time0 = wGetTimer();

	/* propogate elevs between DefElev pts (not handled by propogateForkElevs) */
	for ( i1=0; i1<defelev_da.cnt; i1++ ) {
		dep = &defelev(i1);
		if (GetTrkOnElevPath( dep->trk, &e ))
			/* propogateForkElevs beat us to it */
			continue;
		e = GetTrkEndElevHeight( dep->trk, dep->ep );
		PropogateForkElev( dep->trk, dep->ep, 0, e );
	}
LOG( log_fillElev, 1, ( "%s: propogateDefElevs [%d] (%ld)\n", elevPrefix, defelev_da.cnt, wGetTimer()-time0 ) )
}




/*
 * DYNAMIC ELEVATION COMPUTATION
 *
 * Step 5: Remaining tracks form either Islands connected to zero or more DefElev, Forks
 *  or Branches (Pivots).  The elevation of these tracks is determined by the distance to
 *  the Pivots.  Tracks which are not connected to Pivots are labeled 'Alone' and have no
 *  elevation.
 */

typedef struct {
		track_p trk;
		coOrd pos;
		DIST_T elev;
		} pivot_t;
static dynArr_t pivot_da;
#define pivot(N) DYNARR_N(pivot_t, pivot_da, N)

static void SurveyIsland(
		track_p trk,
		BOOL_T stopAtElevPath )
/* Find the tracks in this island and the pivots of this island
 * Outputs:
 *      elist_da        - tracks in the island
 *      pivot_da        - pivots connecting island to tracks with some elev
 */
{
	int i1;
	track_p trk1;
	EPINX_T ep, cnt;
	pivot_t * pp;
	coOrd hi, lo;
	DIST_T elev;

	DYNARR_RESET( elist_t, elist_da );
	DYNARR_RESET( pivot_t, pivot_da );
	ClrAllTrkBits( TB_PROCESSED );
	elistAppend( trk, 0, 0 );
	SetTrkBits( trk, TB_PROCESSED );
	for ( i1=0; i1 < elist_da.cnt; i1++ ) {
		trk = elist(i1).trk;
		if ( GetTrkIndex(trk) == checkTrk )
			printf( "found in island\n" );
		cnt = GetTrkEndPtCnt(trk);
		for ( ep=0; ep<cnt; ep++ ) {
			trk1 = GetTrkEndTrk( trk, ep );
			if ( EndPtIsDefinedElev(trk,ep)) {
				DYNARR_APPEND( pivot_t, pivot_da, 10 );
				pp = &pivot(pivot_da.cnt-1);
				pp->trk = trk;
				pp->pos = GetTrkEndPos(trk,ep);
				pp->elev = GetTrkEndElevHeight(trk,ep);
			} else if ( stopAtElevPath && trk1 && GetTrkOnElevPath(trk1, &elev) ) {
				DYNARR_APPEND( pivot_t, pivot_da, 10 );
				pp = &pivot(pivot_da.cnt-1);
				pp->trk = trk1;
				GetBoundingBox( trk1, &hi, &lo );
				pp->pos.x = (hi.x+lo.x)/2.0;
				pp->pos.y = (hi.y+lo.y)/2.0;
				pp->elev = elev;
			} else if ( trk1 && !(GetTrkBits(trk1)&TB_PROCESSED) ) {
				SetTrkBits( trk1, TB_PROCESSED );
				elistAppend( trk1, 0, 0 );
			}
		}
	}
	ClrAllTrkBits( TB_PROCESSED );
}

static void ComputeIslandElev(
		track_p trk )
/* Compute elev of tracks connected to 'trk'
 *   An island is the set of tracks bounded by a DefElev EP or a track already on ElevPath
 * Inputs:
 * Working:
 *      elist_da
 * Outputs:
 *      pivot_da        - list of tracks on boundary of Island
 */
{
	int i1, i2;
	coOrd hi, lo, pos;
	pivot_t * pp;
	DIST_T elev;
	DIST_T totalDist;
	elevdist_t * w;
	int mode;
	EPINX_T ep, epCnt;

	SurveyIsland( trk, TRUE );

	for ( i1=0; i1 < elist_da.cnt; i1++ ) {
		trk = elist(i1).trk;
		if ( !IsTrack(trk) )
			continue;
		mode = ELEV_ISLAND;
		if (pivot_da.cnt == 0) {
			elev = 0;
			mode = ELEV_ALONE;
		} else if (pivot_da.cnt == 1) {
			elev = pivot(0).elev;
		} else {
			if ( !GetCurveMiddle( trk, &pos ) ) {
				GetBoundingBox( trk, &hi, &lo );
				pos.x = (hi.x+lo.x)/2.0;
				pos.y = (hi.y+lo.y)/2.0;
			}
			DYNARR_RESET( elevdist_t, elevdist_da );
			totalDist = 0;
			for ( i2=0; i2<pivot_da.cnt; i2++ ) {
				pp = &pivot(i2);
				DYNARR_APPEND( elevdist_t, elevdist_da, 10 );
				w = &elevdist(elevdist_da.cnt-1);
				w->elev = pp->elev;
				w->dist = FindDistance( pos, pp->pos );
				totalDist += w->dist;
			}
			elev = ComputeWeightedElev( totalDist );
		}
		SetTrkOnElevPath( trk, mode, elev );
LOG( log_fillElev, 1, ( "  3  T%d E%0.3f\n", GetTrkIndex(trk), elev ) )
	}

	for ( i1=0; i1<elist_da.cnt; i1++ ) {
		trk = elist(i1).trk;
		epCnt = GetTrkEndPtCnt( trk );
		for ( ep=0; ep<epCnt; ep++ ) {
			mode = GetTrkEndElevMode( trk, ep );
			if ( (mode == ELEV_GRADE || mode == ELEV_COMP) ) {
				RedrawCompGradeElev( trk, ep );
			}
		}
	}
}


static void FindIslandElevs( void )
{
	track_p trk;
	DIST_T elev;
	int islandCnt;
	long time0 = wGetTimer();

	trk = NULL;
	islandCnt = 0;
	while ( TrackIterate( &trk ) ) {
		if ( !GetTrkOnElevPath( trk, &elev ) ) {
			if (IsTrack(trk)) {
				ComputeIslandElev( trk );
				islandCnt++;
			}
		}
	}
LOG( log_fillElev, 1, ( "%s: findIslandElevs [%d] (%ld)\n", elevPrefix, islandCnt, wGetTimer()-time0 ) )
}

/*
 * DYNAMIC ELEVATION COMPUTATION
 * 
 * Drivers
 *
 */

EXPORT void RecomputeElevations( void )
{
	long time0 = wGetTimer();
	elevPrefix = "RECELV";
	if ( !log_fillElev_initted ) { log_fillElev = LogFindIndex( "fillElev" ); log_dumpElev = LogFindIndex( "dumpElev" ); log_fillElev_initted = TRUE; }
	ClearElevPath();
	FindDefElev();
	FindForks();
	ComputeForkElev();
	PropogateForkElevs();
	PropogateDefElevs();
	FindIslandElevs();
	MainRedraw();
LOG( log_fillElev, 1, ( "%s: Total (%ld)\n", elevPrefix, wGetTimer()-time0 ) )
	if ( log_dumpElev > 0 ) {
		track_p trk;
		DIST_T elev;
		for ( trk=NULL; TrackIterate( &trk ); ) {
			printf( "T%4.4d = ", GetTrkIndex(trk) );
			if ( GetTrkOnElevPath( trk, &elev ) )
				printf( "%d:%0.2f\n", GetTrkElevMode(trk), elev );
			else
				printf( "noelev\n" );
#ifdef LATER
		EPINX_T ep;
		int mode;
			for ( ep=0; ep<GetTrkEndPtCnt(trk); ep++ ) {
				mode = GetTrkEndElevMode( trk, ep );
				ComputeElev( trk, ep, FALSE, &elev, NULL );
				printf( "T%4.4d[%2.2d] = %s:%0.3f\n",
						GetTrkIndex(trk), ep,
						mode==ELEV_NONE?"None":mode==ELEV_DEF?"Def":mode==ELEV_COMP?"Comp":
						mode==ELEV_GRADE?"Grade":mode==ELEV_IGNORE?"Ignore":mode==ELEV_STATION?"Station":"???",
						elev );
			}
#endif
		}
	}
}


static BOOL_T needElevUpdate = FALSE;
EXPORT void UpdateAllElevations( void )
{
	int work;
	long time0 = wGetTimer();

	elevPrefix = "UPDELV";
	if ( !log_fillElev_initted ) { log_fillElev = LogFindIndex( "fillElev" ); log_dumpElev = LogFindIndex( "dumpElev" ); log_fillElev_initted = TRUE; }
	if (!needElevUpdate)
		return;
	work = FindObsoleteElevs();
	if (!work)
		return;
	FindForks();
	ComputeForkElev();
	PropogateForkElevs();
	PropogateDefElevs();
	FindIslandElevs();
	needElevUpdate = FALSE;
LOG( log_fillElev, 1, ( "%s: Total (%ld)\n", elevPrefix, wGetTimer()-time0 ) )
}


EXPORT DIST_T GetElevation( track_p trk )
{
	DIST_T elev;

	if ( !IsTrack(trk) ) {
		return 0;
	}
	if ( GetTrkOnElevPath(trk,&elev) ) {
		return elev;
	}

	elevPrefix = "GETELV";
	if ( !log_fillElev_initted ) { log_fillElev = LogFindIndex( "fillElev" ); log_dumpElev = LogFindIndex( "dumpElev" ); log_fillElev_initted = TRUE; }
	ClrAllTrkBits( TB_PROCESSED );
	DYNARR_RESET( defelev_t, defelev_da );
	FindAttachedDefElev( trk, TRUE );
	/* at least one DevElev to be processed */
	FindForks();
	ComputeForkElev();
	PropogateForkElevs();
	PropogateDefElevs();
	if ( GetTrkOnElevPath(trk,&elev) )
		return elev;

	ComputeIslandElev( trk );

	if ( GetTrkOnElevPath(trk,&elev) )
		return elev;

	printf( "GetElevation(T%d) failed\n", GetTrkIndex(trk) );
	return 0;
}


/*
 * DYNAMIC ELEVATION COMPUTATION
 * 
 * Utilities
 *
 */


EXPORT void ClrTrkElev( track_p trk )
{
	needElevUpdate = TRUE;
	DrawTrackElev( trk, &mainD, FALSE );
	ClrTrkBits( trk, TB_ELEVPATH );
}


static void PropogateElevMode( track_p trk, DIST_T elev, int mode )
{
	int i1;
	SurveyIsland( trk, FALSE );
	for ( i1=0; i1<elist_da.cnt; i1++ )
		SetTrkOnElevPath( elist(i1).trk, mode, elev );
}


static BOOL_T CheckForElevAlone( track_p trk )
{
	int i1;
	SurveyIsland( trk, FALSE );
	if ( pivot_da.cnt!=0 )
		return FALSE;
	for ( i1=0; i1<elist_da.cnt; i1++ )
		SetTrkOnElevPath( elist(i1).trk, ELEV_ALONE, 0.0 );
	return TRUE;
}


EXPORT void SetTrkElevModes( BOOL_T connect, track_p trk0, EPINX_T ep0, track_p trk1, EPINX_T ep1 )
{
	int mode0, mode1; 
	DIST_T elev, diff, elev0, elev1;
	char * station;
	BOOL_T update = TRUE;

	mode0 = GetTrkElevMode( trk0 );
	mode1 = GetTrkElevMode( trk1 );
	if ( mode0 == ELEV_ALONE && mode1 == ELEV_ALONE ) {
		update = FALSE;;
	} else if ( connect ) {
		if ( mode0 == ELEV_ALONE ) {
			ComputeElev( trk1, ep1, FALSE, &elev, NULL );
			PropogateElevMode( trk0, elev, ELEV_ISLAND );
			update = FALSE;
		} else if ( mode1 == ELEV_ALONE ) {
			ComputeElev( trk0, ep0, FALSE, &elev, NULL );
			PropogateElevMode( trk1, elev, ELEV_ISLAND );
			update = FALSE;
		}
	} else {
		if ( mode0 == ELEV_ISLAND ) {
			if (CheckForElevAlone( trk0 ))
				update = FALSE;
		} else if ( mode1 == ELEV_ISLAND ) {
			if (CheckForElevAlone( trk1 ))
				update = FALSE;
		}
	}

	if ( connect ) {
		mode0 = GetTrkEndElevMode( trk0, ep0 );
		mode1 = GetTrkEndElevMode( trk1, ep1 );
		elev = 0.0;
		station = NULL;
		if (mode0 == ELEV_DEF && mode1 == ELEV_DEF) {
			mode0 = GetTrkEndElevUnmaskedMode( trk0, ep0 ) | GetTrkEndElevUnmaskedMode( trk1, ep1 );
			elev0 = GetTrkEndElevHeight( trk0, ep0 );
			elev1 = GetTrkEndElevHeight( trk1, ep1 );
			elev = (elev0+elev1)/2.0;
			diff = fabs( elev0-elev1 );
			if (diff>0.1)
				ErrorMessage( MSG_JOIN_DIFFER_ELEV, PutDim(diff) );
		} else if (mode0 == ELEV_DEF) {
			mode0 = GetTrkEndElevUnmaskedMode( trk0, ep0 );
			elev = GetTrkEndElevHeight( trk0, ep0 );
		} else if (mode1 == ELEV_DEF) {
			mode1 = GetTrkEndElevUnmaskedMode( trk1, ep1 );
			elev = GetTrkEndElevHeight( trk1, ep1 );
		} else if (mode0 == ELEV_STATION) {
			station = GetTrkEndElevStation( trk0, ep0 );
		} else if (mode1 == ELEV_STATION) {
			station = GetTrkEndElevStation( trk1, ep1 );
			mode0 = mode1;
		} else if (mode0 == ELEV_GRADE) {
			;
		} else if (mode1 == ELEV_GRADE) {
			mode0 = mode1;
		} else if (mode0 == ELEV_COMP) {
			;
		} else if (mode1 == ELEV_COMP) {
			mode0 = mode1;
		} else {
			;
		}
		SetTrkEndElev( trk0, ep0, mode0, elev, station );
		SetTrkEndElev( trk1, ep1, mode0, elev, station );
	}

	if (update) {
		needElevUpdate = TRUE;
		ClrTrkElev( trk0 );
		ClrTrkElev( trk1 );
	}
}


EXPORT void UpdateTrkEndElev(
		track_p trk,
		EPINX_T ep,
		int newMode,
		DIST_T newElev,
		char * newStation )
{
	int oldMode;
	DIST_T oldElev;
	char * oldStation;
	BOOL_T changed = TRUE;
	track_p trk1;
	EPINX_T ep1;

	oldMode = GetTrkEndElevUnmaskedMode( trk, ep );
	if ( (oldMode&ELEV_MASK) == (newMode&ELEV_MASK) ) {
		switch ( (oldMode&ELEV_MASK) ) {
		case ELEV_DEF:
			oldElev = GetTrkEndElevHeight( trk, ep );
			if ( oldElev == newElev ) {
				if ( oldMode == newMode )
					return;
				changed = FALSE;
			}
			break;
		case ELEV_STATION:
			oldStation = GetTrkEndElevStation( trk, ep );
			if ( strcmp( oldStation, newStation ) == 0 ) {
				return;
			}
			break;
		default:
			return;
		}
	} else {
		changed = TRUE;
		if ( (newMode&ELEV_MASK)==ELEV_DEF || (oldMode&ELEV_MASK)==ELEV_DEF ||
			 (newMode&ELEV_MASK)==ELEV_IGNORE || (oldMode&ELEV_MASK)==ELEV_IGNORE )
			changed = TRUE;
	}
	UndoModify( trk );
	if ( (trk1 = GetTrkEndTrk( trk, ep )) ) {
		UndoModify( trk1 );
	}
	DrawEndPt2( &mainD, trk, ep, drawColorWhite );
	SetTrkEndElev( trk, ep, newMode, newElev, newStation );
	DrawEndPt2( &mainD, trk, ep, drawColorBlack );
	if ( changed ) {
		ClrTrkElev( trk );
		if ( trk1 ) {
			ep1 = GetEndPtConnectedToMe( trk1, trk );
			ClrTrkElev( trk1 );
		}
		UpdateAllElevations();
	}
}

static void SetTrkOnElevPath( track_p trk, int mode, DIST_T elev )
{
	BOOL_T redraw = FALSE;
	int oldMode = GetTrkElevMode( trk );
	DIST_T oldElev;
	coOrd hi, lo;

	if ( !GetTrkOnElevPath( trk, &oldElev ) )
		oldElev = 0.0;
	GetBoundingBox( trk, &hi, &lo );
	if ((labelEnable&LABELENABLE_TRACK_ELEV) &&
		labelScale >= mainD.scale &&
		(! OFF_MAIND( lo, hi ) ) &&
		(GetTrkVisible(trk) || drawTunnel!=0/*DRAW_TUNNEL_NONE*/) &&
		GetLayerVisible(GetTrkLayer(trk)) )
		redraw = TRUE;
	
	if ( (GetTrkBits(trk)&TB_ELEVPATH) && (oldElev == elev && oldMode == mode) )
		return;
	if ( redraw && (GetTrkBits(trk)&TB_ELEVPATH))
		DrawTrackElev( trk, &mainD, FALSE );
	SetTrkElev( trk, mode, elev );
	if ( redraw )
		DrawTrackElev( trk, &mainD, TRUE );
}


EXPORT void DrawTrackElev( track_cp trk, drawCmd_p d, BOOL_T drawIt )
{
	coOrd pos;
	wFont_p fp;
	wDrawColor color=(wDrawColor)0;
	DIST_T elev;
	coOrd lo, hi;

	if ( (!IsTrack(trk)) ||
		 (!(labelEnable&LABELENABLE_TRACK_ELEV)) ||
		 (d == &mapD) ||
		 (labelScale < d->scale) ||
		 (!GetTrkOnElevPath( trk, &elev )) ||
		 ((GetTrkBits(trk)&TB_ELEVPATH) == 0) ||
		 (d->funcs->options & wDrawOptTemp) != 0 ||
		 (d->options & DC_QUICK) != 0 )
		return;

	if ( !GetCurveMiddle( trk, &pos ) ) {
		GetBoundingBox( trk, &hi, &lo );
		pos.x = (hi.x+lo.x)/2.0;
		pos.y = (hi.y+lo.y)/2.0;
	}
	if ( d==&mainD && OFF_MAIND( pos, pos ) )
		return;

	switch ( GetTrkElevMode(trk) ) {
	case ELEV_FORK:
		color = drawColorBlue;
		break;
	case ELEV_BRANCH:
		color = drawColorPurple;
		break;
	case ELEV_ISLAND:
		color = drawColorGold;
		break;
	case ELEV_ALONE:
		return;
	}
	if ( !drawIt )
		color = wDrawColorWhite;
	sprintf( message, "%s", FormatDistance(elev));
	fp = wStandardFont( F_HELV, FALSE, FALSE );
	DrawBoxedString( BOX_INVERT, d, pos, message, fp, (wFontSize_t)descriptionFontSize, color, 0 );
}


