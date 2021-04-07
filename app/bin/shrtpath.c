/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/shrtpath.c,v 1.1 2005-12-07 15:46:54 rc-flyer Exp $
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

#include <string.h>

#include "shrtpath.h"
#include "track.h"

EXPORT int log_shortPath;
static int log_shortPathInitted;

/**************************************************************************
 *
 *  Dijkstra's shortest path
 *
 **************************************************************************/

typedef enum { Unknown, Working, Final } pathState_e;
typedef struct {
		pathState_e state;
		DIST_T dist;			/* Distance from root to entry */
		track_p contTrk;		/* continuation */
		EPINX_T contEP;
		int inxBack;			/* Previous node on shortest path */
		int inxTracks;			/* List of tracks along this path */
		int numTracks;
		} pathNode_t, *pathNode_p;

static dynArr_t pathNode_da;
#define pathNode(N) DYNARR_N( pathNode_t, pathNode_da, N )
typedef struct {
		track_p trk;
		EPINX_T ep1, ep2;
		DIST_T dist;
		} trackep_t, *trackep_p;
static dynArr_t trackep_da;
#define trackep(N) DYNARR_N( trackep_t, trackep_da, N )

static track_p shortPathTrk0, shortPathTrk1;
static EPINX_T shortPathEP0, shortPathEP1;


static int DoShortPathFunc( shortestPathFunc_p func, char * title, SPTF_CMD cmd, track_p trk, EPINX_T ep1, EPINX_T ep2, DIST_T dist, void * data )
{
	int rc;
LOG( log_shortPath, 4, ( "      %s: T%d:%d.%d D:%0.3f ", title, trk?GetTrkIndex(trk):-1, ep1, ep2, dist ) )
	rc = func( cmd, trk, ep1, ep2, dist, data );
LOG( log_shortPath, 4, ( "-> %d\n", rc ) )
	return rc;
}

static void DumpPaths( int pinx )
{
	pathNode_p pPath;
	trackep_p pTrackep;
	int tinx;

	lprintf("    Current = %d\n", pinx );
	for (pinx=0; pinx<pathNode_da.cnt; pinx++) {
		pPath = &pathNode(pinx);
		lprintf( "      %3d: S%c T%d:%d D%0.3f T%d:%d",
				pinx,
				pPath->state==Unknown?'U':pPath->state==Working?'W':pPath->state==Final?'F':'?',
				(pPath->contTrk?GetTrkIndex(pPath->contTrk):-1),
				pPath->contEP,
				pPath->dist,
				pPath->inxTracks,
				pPath->numTracks );
		if (pPath->inxBack>=0) {
			 lprintf(" B%d", pPath->inxBack );
		}
		lprintf("\n        ");
		for (tinx=0; tinx<pPath->numTracks; tinx++) {
			pTrackep = &trackep(pPath->inxTracks+tinx);
			lprintf( " T%d:%d-%d=%0.1f", GetTrkIndex(pTrackep->trk), pTrackep->ep1, pTrackep->ep2, pTrackep->dist );
		}
		lprintf("\n");
	}
}


static void AddTracksToPath(
		int inxCurr,
		shortestPathFunc_p func,
		void * data )
{
	pathNode_p pPath;
	int tinx;
	trackep_p pTrackep;

	while (inxCurr>=0) {
		pPath = &pathNode(inxCurr);
		for (tinx=pPath->numTracks-1;tinx>=0;tinx--) {
			pTrackep = &trackep(pPath->inxTracks+tinx);
			DoShortPathFunc( func, "ADDTRK", SPTC_ADD_TRK, pTrackep->trk, pTrackep->ep1, pTrackep->ep2, pTrackep->dist, data );
		}
		inxCurr = pPath->inxBack;
	}
}


static void AddTrackToNode(
		track_p trk,
		EPINX_T ep1,
		EPINX_T ep2,
		DIST_T dist)
{
	DYNARR_APPEND( trackep_t, trackep_da, 10 );
	trackep(trackep_da.cnt-1).trk = trk;
	trackep(trackep_da.cnt-1).ep1 = ep1;
	trackep(trackep_da.cnt-1).ep2 = ep2;
	trackep(trackep_da.cnt-1).dist = dist;
}


static BOOL_T AddPath(
		int inxCurr,
		track_p trk0,
		EPINX_T ep1,
		EPINX_T ep2,
		DIST_T dist,
		shortestPathFunc_p func,
		void * data )
{
	EPINX_T epN;
	track_p trk=trk0, trkN;
	EPINX_T epCnt;
	pathNode_p pNode;
	int startTrack;
	char * msg=NULL;

LOG( log_shortPath, 2, ( "  AddPath( T%d:%d.%d D=%0.3f B%d ) -> \n", GetTrkIndex(trk), ep1, ep2, dist, inxCurr ) )
	startTrack = trackep_da.cnt;
	while (1) {
		if ( ep2>=0 ) {
			AddTrackToNode( trk, ep1, ep2, dist );
			dist += GetTrkLength( trk, ep1, -1 ) + GetTrkLength( trk, ep2, -1 );
			if ( DoShortPathFunc( func, "MATCH", SPTC_MATCH, trk, ep2, ep1, dist, data ) ) {
				trk = NULL;
				ep1 = -1;
				msg = "";
				goto makeNode;
			}
			trkN = GetTrkEndTrk(trk,ep2);
			if ( trkN == NULL ) {
				/* dead end */
				msg = "dead end";
				goto skipNode;
			}
			if ( DoShortPathFunc( func, "IGNORE", SPTC_IGNNXTTRK, trk, ep2, ep1, dist, data ) ) {
				msg = "ignore end";
				goto skipNode;
			}
			ep1 = GetEndPtConnectedToMe( trkN, trk );
			trk = trkN;
			if ( (trk==shortPathTrk0 && ep1==shortPathEP0) || (trk==shortPathTrk1 && ep1==shortPathEP1) ) {
				msg = "wrap around";
				goto skipNode;
			}
		}
		epCnt = GetTrkEndPtCnt(trk);
		if ( epCnt < 2 ) {
			msg = "bumper track";
			goto skipNode;
		}
		if ( epCnt > 2 ) {
			if ( (epN=DoShortPathFunc( func, "MATCHANY", SPTC_MATCHANY, trk, ep1, -1, dist, data )) >= 0 ) {
				/* special match */
				/*dist += GetTrkLength( trk, ep1, epN );*/
				AddTrackToNode( trk, ep1, epN, dist );
				trk = NULL;
				ep1 = -1;
				msg = "ANY";
			}
			goto makeNode;
		}
		ep2 = 1-ep1;
	}

makeNode:
if ( trk ) {
LOG( log_shortPath, 2, ( "  ->  FORK: [%d] T%d:%d", pathNode_da.cnt, GetTrkIndex(trk), ep1 ) )
} else {
LOG( log_shortPath, 2, ( "  -> MATCH%s: [%d]", msg, pathNode_da.cnt ) )
}
LOG( log_shortPath, 2, ( " t%d D=%0.3f\n", startTrack, dist ) )

	DYNARR_APPEND( pathNode_t, pathNode_da, 10 );
	pNode = &pathNode(pathNode_da.cnt-1);
	pNode->state = Working;
	pNode->dist = dist;
	pNode->contTrk = trk;
	pNode->contEP = ep1;
	pNode->inxBack = inxCurr; 
	pNode->inxTracks = startTrack;
	pNode->numTracks = trackep_da.cnt-startTrack;
	if ( trk )
		SetTrkBits( trk, TB_SHRTPATH );
	return TRUE;

skipNode:
LOG( log_shortPath, 2, ( "  -> FAIL: %s @ T%d:%d.%d\n", msg, GetTrkIndex(trk), ep1, ep2 ) )
	trackep_da.cnt = startTrack;
	return FALSE;
}



int FindShortestPath(
		track_p trkN,
		EPINX_T epN,
		BOOL_T bidirectional,
		shortestPathFunc_p func,
		void * data )
{
	int inxCurr = 0;
	pathNode_p pCurr;
	pathNode_p pNext;
	int pinx=0;
	DIST_T minDist;
	int count;
	int rc = 0;
	EPINX_T ep2, epCnt, ep3;
	static dynArr_t ep_da;
	#define ep(N) DYNARR_N( pathNode_p, ep_da, N )

	DYNARR_RESET( pathNode_t, pathNode_da );
	DYNARR_RESET( trackep_t, trackep_da );
	count = 0;

	if ( !log_shortPathInitted ) {
		log_shortPath = LogFindIndex( "shortPath" );
		log_shortPathInitted = TRUE; 
	}

LOG( log_shortPath, 1, ( "FindShortestPath( T%d:%d, %s, ... )\n", GetTrkIndex(trkN), epN, bidirectional?"bidir":"unidir" ) )
	ClrAllTrkBits( TB_SHRTPATH );
	/* Note: trkN:epN is not tested for MATCH */
	shortPathTrk0 = trkN;
	shortPathEP0 = epN;
	shortPathTrk1 = GetTrkEndTrk( trkN, epN );
	if ( shortPathTrk1 != NULL )
		shortPathEP1 = GetEndPtConnectedToMe( shortPathTrk1, shortPathTrk0 );
	AddPath( -1, shortPathTrk0, shortPathEP0, -1, 0.0, func, data );
	if ( bidirectional && shortPathTrk1 != NULL )
		AddPath( -1, shortPathTrk1, shortPathEP1, -1, 0.0, func, data );

	while (1) {
		InfoMessage( "%d", ++count );

		/* select next final node */
		minDist = 0.0;
		inxCurr = -1;
		for (pinx=0; pinx<pathNode_da.cnt; pinx++) {
			 pNext = &pathNode(pinx);
			 if (pNext->state == Working &&
				(inxCurr < 0 || pNext->dist < minDist) ) {
				minDist = pathNode(pinx).dist;
				inxCurr = pinx;
			}
		}
		if ( inxCurr < 0 )
			break;
if (log_shortPath>=4) DumpPaths(inxCurr);
		pCurr = &pathNode(inxCurr);
		pCurr->state = Final;
		if ( pCurr->contTrk == NULL ) {
			if ( !DoShortPathFunc( func, "VALID", SPTC_VALID, trackep(pCurr->inxTracks+pCurr->numTracks-1).trk, trackep(pCurr->inxTracks+pCurr->numTracks-1).ep2, -1, 0.0, data ) )
				continue;
			AddTracksToPath( inxCurr, func, data );
			rc++;
			if ( DoShortPathFunc( func, "TERMINATE", SPTC_TERMINATE, trackep(pCurr->inxTracks+pCurr->numTracks-1).trk, trackep(pCurr->inxTracks+pCurr->numTracks-1).ep2, -1, 0.0, data ) )
				break;
		} else {
			epCnt = GetTrkEndPtCnt(pCurr->contTrk);
			DYNARR_SET( pathNode_p, ep_da, epCnt );
			memset( ep_da.ptr, 0, epCnt * sizeof pNext );
			if ( (GetTrkBits(pCurr->contTrk) & TB_SHRTPATH) ) {
				for ( pinx=0; pinx<pathNode_da.cnt; pinx++ ) {
					pNext = &pathNode(pinx);
					if ( pNext->contTrk == pCurr->contTrk ) {
						ep(pNext->contEP) = pNext;
					}
				}
			}
			for ( ep2=0; ep2<epCnt; ep2++ ) {
				pCurr = &pathNode(inxCurr);

				/* don't point back at myself */
				if ( pCurr->contEP == ep2 ) continue;
				/* no route to ep */
				if ( DoShortPathFunc( func, "IGNORE", SPTC_IGNNXTTRK, pCurr->contTrk, pCurr->contEP, ep2, pCurr->dist, data ) ) continue;
				/* somebody got here first */
				if ( ep(ep2) ) continue;
				/* there is already a path out via ep2 */
				for ( ep3=0; ep3<epCnt; ep3++ ) {
					if ( ep3==pCurr->contEP || ep3==ep2 ) continue;
					if ( ep(ep3) == NULL ) continue;
					if ( DoShortPathFunc( func, "IGNORE", SPTC_IGNNXTTRK, pCurr->contTrk, ep2, ep3, pCurr->dist, data ) ) continue;
					if ( ep(ep3)->state == Final ) break;
				}
				if ( ep3 < epCnt ) continue;
				AddPath( inxCurr, pCurr->contTrk, pCurr->contEP, ep2, pCurr->dist, func, data );
			}
		}
	}

if (log_shortPath>=1) DumpPaths(inxCurr);
	ClrAllTrkBits( TB_SHRTPATH );
	return rc;
}


