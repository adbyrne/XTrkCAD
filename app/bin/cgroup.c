/** \file cgroup.c
 * Compound tracks: Group
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

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "tbezier.h"
#include "tcornu.h"
#include "common.h"
#include "messages.h"
#include "param.h"
#include "shrtpath.h"
#include "track.h"
#include "utility.h"


/*****************************************************************************
 *
 * Ungroup / Group
 *
 */

static int log_group=-1;

static dynArr_t pathPtr_da;
#define pathPtr(N)				DYNARR_N( char, pathPtr_da, N )

static char groupManuf[STR_SIZE];
static char groupDesc[STR_SIZE];
static char groupPartno[STR_SIZE];
static char groupTitle[STR_SIZE];
static int groupCompoundCount = 0;

extern TRKTYP_T T_BZRTRK;
extern TRKTYP_T T_BZRLIN;
extern TRKTYP_T T_CORNU;

typedef struct {
		int segInx;
		EPINX_T segEP;
		int inx;
		track_p trk;
		} mergePt_t;
static dynArr_t mergePt_da;
#define mergePt(N) DYNARR_N( mergePt_t, mergePt_da, N )
static void AddMergePt(
		int segInx,
		EPINX_T segEP )
{
	int inx;
	mergePt_t * mp;
	for ( inx=0; inx<mergePt_da.cnt; inx++ ) {
		mp = &mergePt(inx);
		if ( mp->segInx == segInx &&
			 mp->segEP == segEP )
			return;
	}
	DYNARR_APPEND( mergePt_t, mergePt_da, 10 );
	mp = &mergePt(mergePt_da.cnt-1);
	mp->segInx = segInx;
	mp->segEP = segEP;
	mp->inx = mergePt_da.cnt-1;
LOG( log_group, 2, ( "  MergePt: %d.%d\n", segInx, segEP ) );
}


static EPINX_T FindEP(
		EPINX_T epCnt,
		trkEndPt_p endPts,
		coOrd pos )
{
	DIST_T dist;
	EPINX_T ep;
	for ( ep=0; ep<epCnt; ep++ ) {
		dist = FindDistance( pos, endPts[ep].pos );
		if ( dist < connectDistance )
			return ep;
	}
	return -1;
}


static void SegOnMP(
		int segInx,
		int mpInx,
		int segCnt,
		int * map )
{
	int inx;
	mergePt_t * mp;
	if ( map[segInx] < 0 ) {
LOG( log_group, 2, ( "  S%d: on MP%d\n", segInx, mpInx ) );
		map[segInx] = mpInx;
		return;
	}
LOG( log_group, 2, ( "  S%d: remapping MP%d to MP%d\n", segInx, mpInx, map[segInx] ) );
	for ( inx=0; inx<segCnt; inx++ )
		if ( map[inx] == mpInx )
			map[inx] = map[segInx];
	for ( inx=0; inx<mergePt_da.cnt; inx++ ) {
		if ( inx == map[segInx] )
			continue;
		mp = &mergePt(inx);
		if ( mp->inx == mpInx )
			mp->inx = map[segInx];
	}
}


static void GroupCopyTitle(
		char * title )
{
	char *mP, *nP, *pP;
	int mL, nL, pL;

	ParseCompoundTitle( title, &mP, &mL, &nP, &nL, &pP, &pL );
	if ( strncmp( nP, "Ungrouped ", 10 ) == 0 ) {
		nP += 10;
		nL -= 10;
	}
	if ( ++groupCompoundCount == 1 ) {
		strncpy( groupManuf, mP, mL );
		groupManuf[mL] = '\0';
		strncpy( groupDesc, nP, nL );
		groupDesc[nL] = '\0';
		strncpy( groupPartno, pP, pL );
		groupPartno[pL] = '\0';
	} else {
		if ( mL != (int)strlen( groupManuf ) ||
			 strncmp( groupManuf, mP, mL ) != 0 )
			groupManuf[0] = '\0';
		if ( nL != (int)strlen( groupDesc ) ||
			 strncmp( groupDesc, nP, nL ) != 0 )
			groupDesc[0] = '\0';
		if ( pL != (int)strlen( groupPartno ) ||
			 strncmp( groupPartno, pP, pL ) != 0 )
			groupPartno[0] = '\0';
	}
}


EXPORT void UngroupCompound(
		track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	struct extraData *xx1;
	trkSeg_p sp;
	track_p trk0, trk1;
	int segCnt, segInx, segInx1;
	EPINX_T ep, epCnt, epCnt1=0, segEP, segEP1, eps[2];
	char * cp;
	coOrd pos, orig, size;
	ANGLE_T angle;
	int inx;
	int off;
	mergePt_t * mp;
	trkEndPt_p epp;
	segProcData_t segProcData;
	static dynArr_t refCount_da;
#define refCount(N) DYNARR_N( int, refCount_da, N )
	typedef struct {
		track_p trk;
		EPINX_T ep[2];
		} segTrack_t;
#define segTrack(N) DYNARR_N( segTrack_t, segTrack_da, N )
	static dynArr_t segTrack_da;
	segTrack_t * stp, * stp1;
	BOOL_T turnoutChanged;

	DYNARR_RESET( mergePt_t, mergePt_da );
	DYNARR_RESET( int, refCount_da );
	DYNARR_RESET( segTrack_t, segTrack_da );
	GroupCopyTitle( xtitle(xx) );

#ifdef LATER
	for ( sp=sq=xx->segs; sp<&xx->segs[xx->segCnt]; sp++ ) {
		if ( IsSegTrack(sp) ) {
			*sq = *sp;
			sq++;
		} else {
			trk1 = MakeDrawFromSeg( xx->orig, xx->angle, sp );
			if ( trk1 ) {
				SetTrkBits( trk1, TB_SELECTED );
				DrawNewTrack( trk1 );
			}
		}
	}
	if ( GetTrkEndPtCnt(trk) <= 0 ) {
		UndoDelete( trk );
		return;
	}
#endif

LOG( log_group, 1, ( "Ungroup( T%d )\n", GetTrkIndex(trk) ) );
	epCnt = GetTrkEndPtCnt(trk);
	for ( segCnt=0; segCnt<xx->segCnt&&IsSegTrack(&xx->segs[segCnt]); segCnt++ );
	ASSERT( (epCnt==0) == (segCnt==0) );
	turnoutChanged = FALSE;
	if ( epCnt > 0 ) {
		turnoutChanged = TRUE;

		/* 1: collect EPs
		 */
		DYNARR_SET( trkEndPt_t, tempEndPts_da, epCnt );
		DYNARR_SET( segTrack_t, segTrack_da, segCnt );
		memset( segTrack_da.ptr, 0, segCnt * sizeof segTrack(0) );
		for ( ep=0; ep<epCnt; ep++ ) {
			epp = &tempEndPts(ep);
			epp->pos = GetTrkEndPos( trk, ep );
			epp->angle = GetTrkEndAngle( trk, ep );
			Rotate( &epp->pos, xx->orig, -xx->angle );
			epp->pos.x -= xx->orig.x;
			epp->pos.y -= xx->orig.y;
			epp->track = GetTrkEndTrk( trk, ep );
			if ( epp->track )
				epp->index = GetEndPtConnectedToMe( epp->track, trk );
			else
				epp->index = -1;
LOG( log_group, 1, ( " EP%d = [%0.3f %0.3f] A%0.3f T%d.%d\n", ep, epp->pos.x, epp->pos.y, epp->angle, epp->track?GetTrkIndex(epp->track):-1, epp->track?epp->index:-1 ) );
		}

		/* 3: Count number of times each segment is referenced
		 *    If the refcount differs between adjacent segments
		 *       add segment with smaller count to mergePts
		 *    Treat EndPts as a phantom segment with inx above segCnt
		 *    Path ends that don't map onto a real EndPt (bumpers) get a fake EP
		 */
		DYNARR_SET( int, refCount_da, segCnt+epCnt );
		memset( refCount_da.ptr, 0, refCount_da.cnt * sizeof *(int*)0 );
		cp = (char *)xx->paths;
		while ( cp[0] ) {
			cp += strlen(cp)+1;
			while ( cp[0] ) {
				GetSegInxEP( cp[0], &segInx, &segEP );
				pos = GetSegEndPt( xx->segs+segInx, segEP, FALSE, NULL );
				segInx1 = FindEP( tempEndPts_da.cnt, &tempEndPts(0), pos );
				if ( segInx1 >= 0 ) {
					segInx1 += segCnt;
					refCount(segInx1)++;
				} else {
					DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
					DYNARR_APPEND( int, refCount_da, 10 );
					epp = &tempEndPts(tempEndPts_da.cnt-1);
					epp->pos = pos;
					epp->angle = 0;
					segInx1 = refCount_da.cnt-1;
					refCount(segInx1) = 2;
				}
				segEP1 = 0;
				while ( cp[0] ) {
					GetSegInxEP( cp[0], &segInx, &segEP );
					refCount(segInx)++;
					if ( refCount(segInx) > refCount(segInx1) )
						AddMergePt( segInx, segEP );
					if ( refCount(segInx1) > refCount(segInx) )
						AddMergePt( segInx1, segEP1 );
					segInx1 = segInx;
					segEP1 = 1-segEP;
					cp++;
				}
				GetSegInxEP( cp[-1], &segInx, &segEP );
				pos = GetSegEndPt( xx->segs+segInx, 1-segEP, FALSE, NULL );
				segInx = FindEP( tempEndPts_da.cnt, &tempEndPts(0), pos );
				if ( segInx >= 0 ) {
					segInx += segCnt;
					refCount(segInx)++;
				} else {
					DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
					DYNARR_APPEND( int, refCount_da, 10 );
					epp = &tempEndPts(tempEndPts_da.cnt-1);
					epp->pos = pos;
					epp->angle = 0;
					segInx = refCount_da.cnt-1;
					refCount(segInx) = 2;
				}
				if ( refCount(segInx) > refCount(segInx1) ) {
					AddMergePt( segInx, 0 );
				}
				cp++;
			}
			cp++;
		}
		epCnt1 = tempEndPts_da.cnt;
	
		/* 4: For each path element, map segment to a mergePt if the adjacent segment
		 *    and EP is a mergePt
		 *    If segment is already mapped then merge mergePts
		 */
		DYNARR_SET( int, refCount_da, segCnt );
		memset( refCount_da.ptr, -1, segCnt * sizeof *(int*)0 );
		cp = (char *)xx->paths;
		while ( cp[0] ) {
			cp += strlen(cp)+1;
			while ( cp[0] ) {
				GetSegInxEP( cp[0], &segInx, &segEP );
				pos = GetSegEndPt( xx->segs+segInx, segEP, FALSE, NULL );
				/*REORIGIN1( pos, xx->angle, xx->orig );*/
				segInx1 = FindEP( tempEndPts_da.cnt, &tempEndPts(0), pos );
				if ( segInx1 >= 0 ) {
					segInx1 += segCnt;
				}
				segEP1 = 0;
				while ( cp[0] ) {
					GetSegInxEP( cp[0], &segInx, &segEP );
					if ( segInx1 >= 0 ) {
						for ( inx=0; inx<mergePt_da.cnt; inx++ ) {
							mp = &mergePt(inx);
							if ( mp->segInx == segInx1 && mp->segEP == segEP1 ) {
								SegOnMP( segInx, mp->inx, segCnt, &refCount(0) );
							}
							if ( mp->segInx == segInx && mp->segEP == segEP ) {
								SegOnMP( segInx1, mp->inx, segCnt, &refCount(0) );
							}
						}
					}
					segInx1 = segInx;
					segEP1 = 1-segEP;
					cp++;
				}
				GetSegInxEP( cp[-1], &segInx, &segEP );
				pos = GetSegEndPt( xx->segs+segInx, 1-segEP, FALSE, NULL );
				/*REORIGIN1( pos, xx->angle, xx->orig );*/
				segInx = FindEP( tempEndPts_da.cnt, &tempEndPts(0), pos );
				if ( segInx >= 0 ) {
					segInx += segCnt;
					for ( inx=0; inx<mergePt_da.cnt; inx++ ) {
						mp = &mergePt(inx);
						if ( mp->segInx == segInx && mp->segEP == 0 ) {
							SegOnMP( segInx1, mp->inx, segCnt, &refCount(0) );
						}
					}
				}
				cp++;
			}
			cp++;
		}

		/* 5: Check is all segments are on the same mergePt, which means there is nothing to do
		 */
		if ( mergePt_da.cnt > 0 ) {
			for ( segInx=0; segInx<segCnt; segInx++ )
				if ( refCount(segInx) != mergePt(0).inx )
					break;
			if ( segInx == segCnt ) {
				/* all segments on same turnout, nothing we can do here */
				turnoutChanged = FALSE;
				if ( segCnt == xx->segCnt ) {
					/* no non-track segments to remove */
					return;
				}
			}
		}
	}

	/* 6: disconnect, undraw, remove non-track segs, return if there is nothing else to do
	 */
	wDrawDelayUpdate( mainD.d, TRUE );
	if ( turnoutChanged ) {
		for ( ep=0; ep<epCnt; ep++ ) {
			epp = &tempEndPts(ep);
			if ( epp->track ) {
				DrawEndPt( &mainD, epp->track, epp->index, wDrawColorWhite );
				DrawEndPt( &mainD, trk, ep, wDrawColorWhite );
				DisconnectTracks( trk, ep, epp->track, epp->index );
			}
		}
	}
	UndrawNewTrack(trk);
	for ( sp=xx->segs; sp<&xx->segs[xx->segCnt]; sp++ ) {
		if ( ! IsSegTrack(sp) ) {
			trk1 = MakeDrawFromSeg( xx->orig, xx->angle, sp );
			if ( trk1 ) {
				SetTrkBits( trk1, TB_SELECTED );
				DrawNewTrack( trk1 );
			}
		}
	}
	if ( !turnoutChanged ) {
		if ( epCnt <= 0 ) {
			trackCount--;
			UndoDelete( trk );
		} else {
			UndoModify( trk );
			xx->segCnt = segCnt;
			DrawNewTrack( trk );
		}
		wDrawDelayUpdate( mainD.d, FALSE );
		return;
	}

	/* 7: for each valid mergePt, create a new turnout
	 */
	for ( inx=0; inx<mergePt_da.cnt; inx++ ) {
		mp = &mergePt(inx);
		if ( mp->inx != inx )
			continue;
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		DYNARR_SET( trkEndPt_t, tempEndPts_da, epCnt1 );
		DYNARR_RESET( char, pathPtr_da );
		for ( segInx=0; segInx<segCnt; segInx++ ) {
			if ( refCount(segInx) == inx ) {
				DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
				tempSegs(tempSegs_da.cnt-1) = xx->segs[segInx];
				sprintf( message, "P%d", segInx );
				off = pathPtr_da.cnt;
				DYNARR_SET( char, pathPtr_da, off+(int)strlen(message)+4 );
				strcpy( &pathPtr(off), message );
				off = pathPtr_da.cnt-3;
				pathPtr(off+0) = (char)tempSegs_da.cnt;
				pathPtr(off+1) = '\0';
				pathPtr(off+2) = '\0';
				for ( ep=0; ep<2; ep++ ) {
					pos = GetSegEndPt( xx->segs+segInx, ep, FALSE, &angle );
					segEP = FindEP( epCnt1, &tempEndPts(0), pos );
					if ( segEP >= 0 && segEP >= epCnt && segEP < epCnt1 ) {
						/* was a bumper: no EP */
						eps[ep] = -1;
						continue;
					}
					REORIGIN1( pos, xx->angle, xx->orig );
					angle = NormalizeAngle( xx->angle+angle );
					eps[ep] = FindEP( tempEndPts_da.cnt-epCnt1, &tempEndPts(epCnt1), pos );
					if ( eps[ep] < 0 ) {
						DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
						eps[ep] = tempEndPts_da.cnt-1-epCnt1;
						epp = &tempEndPts(tempEndPts_da.cnt-1);
						memset( epp, 0, sizeof *epp );
						epp->pos = pos;
						epp->angle = angle;
					}
				}
				segTrack(segInx).ep[0] = eps[0];
				segTrack(segInx).ep[1] = eps[1];
			}
		}
		DYNARR_SET( char, pathPtr_da, pathPtr_da.cnt+1 );
		pathPtr(pathPtr_da.cnt-1) = '\0';
		if ( tempSegs_da.cnt == 0 ) {
			AbortProg( "tempSegs_da.cnt == 0" );
			continue;
		}
		GetSegBounds( zero, 0, tempSegs_da.cnt, &tempSegs(0), &orig, &size );
		orig.x = -orig.x;
		orig.y = -orig.y;
		MoveSegs( tempSegs_da.cnt, &tempSegs(0), orig );
		Rotate( &orig, zero, xx->angle );
		orig.x = xx->orig.x - orig.x;
		orig.y = xx->orig.y - orig.y;
		trk1 = NewCompound( T_TURNOUT, 0, orig, xx->angle, xx->title, tempEndPts_da.cnt-epCnt1, &tempEndPts(epCnt1), pathPtr_da.cnt, &pathPtr(0), tempSegs_da.cnt, &tempSegs(0) );
		xx1 = GetTrkExtraData(trk1);
		xx1->ungrouped = TRUE;

		SetTrkVisible( trk1, TRUE );
		SetTrkBits( trk1, TB_SELECTED );
		for ( segInx=0; segInx<segCnt; segInx++ ) {
			if ( refCount(segInx) == inx ) {
				segTrack(segInx).trk = trk1;
			}
		}
		mp->trk = trk1;
	}

	/* 8: for remaining segments, create simple tracks
	 */
	for ( segInx=0; segInx<segCnt; segInx++ ) {
		if ( refCount(segInx) >= 0 ) continue;
		SegProc( SEGPROC_NEWTRACK, xx->segs+segInx, &segProcData );
		SetTrkScale( segProcData.newTrack.trk, GetTrkScale(trk) );
		SetTrkBits( segProcData.newTrack.trk, TB_SELECTED );
		MoveTrack( segProcData.newTrack.trk, xx->orig );
		RotateTrack( segProcData.newTrack.trk, xx->orig, xx->angle );
		segTrack(segInx).trk = segProcData.newTrack.trk;
		segTrack(segInx).ep[0] = segProcData.newTrack.ep[0];
		segTrack(segInx).ep[1] = segProcData.newTrack.ep[1];
	}

	/* 9: reconnect tracks
	 */
	cp = (char *)xx->paths;
	while ( cp[0] ) {
		cp += strlen(cp)+1;
		while ( cp[0] ) {
			/* joint EP to this segment */
			GetSegInxEP( cp[0], &segInx, &segEP );
			stp = &segTrack(segInx);
			ep = FindEP( epCnt, &tempEndPts(0), GetSegEndPt( xx->segs+segInx, segEP, FALSE, NULL ) );
			if ( ep >= 0 ) {
				epp = &tempEndPts(ep);
				if ( epp->track ) {
					ConnectTracks( stp->trk, stp->ep[segEP], epp->track, epp->index );
					DrawEndPt( &mainD, epp->track, epp->index, GetTrkColor(epp->track,&mainD) );
					epp->track = NULL;
				}
			}
			stp1 = stp;
			segEP1 = 1-segEP;
			cp++;
			while ( cp[0] ) {
				GetSegInxEP( cp[0], &segInx, &segEP );
				stp = &segTrack(segInx);
				trk0 = GetTrkEndTrk( stp->trk, stp->ep[segEP] );
				trk1 = GetTrkEndTrk( stp1->trk, stp1->ep[segEP1] );
				if ( trk0 == NULL ) {
					if ( trk1 != NULL )
						AbortProg( "ungroup: seg half connected" );
					ConnectTracks( stp->trk, stp->ep[segEP], stp1->trk, stp1->ep[segEP1] );
				} else {
					if ( trk1 != stp->trk || stp1->trk != trk0 )
						AbortProg( "ungroup: last seg not connected to curr" );
				}
				stp1 = stp;
				segEP1 = 1-segEP;
				cp++;
			}
			/* joint EP to last segment */
			ep = FindEP( epCnt, &tempEndPts(0), GetSegEndPt( xx->segs+segInx, segEP1, FALSE, NULL ) );
			if ( ep > 0 ) {
				epp = &tempEndPts(ep);
				if ( epp->track ) {
					ConnectTracks( stp1->trk, stp1->ep[segEP1], epp->track, epp->index );
					DrawEndPt( &mainD, epp->track, epp->index, wDrawColorWhite );
					epp->track = NULL;
				}
			}
			cp++;
		}
		cp++;
	}

	/* 10: cleanup: delete old track, draw new tracks
	 */
	UndoDelete( trk );
	trackCount--;
	for ( segInx=0; segInx<segCnt; segInx++ ) {
		if ( refCount(segInx) >= 0 ) {
			mp = &mergePt( refCount(segInx) );
			if ( mp->trk ) {
				DrawNewTrack( mp->trk );
				mp->trk = NULL;
			}
		} else {
			DrawNewTrack( segTrack(segInx).trk );
		}
	}
	wDrawDelayUpdate( mainD.d, FALSE );
}




EXPORT void DoUngroup( void )
{
	track_p trk = NULL;
	int ungroupCnt;
	int oldTrackCount;
	TRKINX_T lastTrackIndex;

	if ( log_group < 0 )
		log_group = LogFindIndex( "group" );
	groupManuf[0] = 0;
	groupDesc[0] = 0;
	groupPartno[0] = 0;
	ungroupCnt = 0;
	oldTrackCount = trackCount;
	UndoStart( _("Ungroup Object"), "Ungroup Objects" );
	lastTrackIndex = max_index;
	groupCompoundCount = 0;
	while ( TrackIterate( &trk ) ) {
		if ( GetTrkSelected( trk ) && GetTrkIndex(trk) <= lastTrackIndex ) {
			oldTrackCount = trackCount;
			UngroupTrack( trk );
			if ( oldTrackCount != trackCount )
				ungroupCnt++;
		}
	}
	if ( ungroupCnt )
		InfoMessage( _("%d objects ungrouped"), ungroupCnt );
	else
		InfoMessage( _("No objects ungrouped") );
}



static drawCmd_t groupD = {
		NULL, &tempSegDrawFuncs, DC_GROUP, 1, 0.0, {0.0, 0.0}, {0.0, 0.0}, Pix2CoOrd, CoOrd2Pix };
static long groupSegCnt;
static long groupReplace;
char * groupReplaceLabels[] = { N_("Replace with new group?"), NULL };

static wWin_p groupW;
static paramIntegerRange_t r0_999999 = { 0, 999999 };
static paramData_t groupPLs[] = {
/*0*/ { PD_STRING, groupManuf, "manuf", PDO_NOPREF, (void*)350, N_("Manufacturer") },
/*1*/ { PD_STRING, groupDesc, "desc", PDO_NOPREF, (void*)230, N_("Description") },
/*2*/ { PD_STRING, groupPartno, "partno", PDO_NOPREF|PDO_DLGHORZ|PDO_DLGIGNORELABELWIDTH, (void*)100, N_("#") },
/*3*/ { PD_LONG, &groupSegCnt, "segcnt", PDO_NOPREF, &r0_999999, N_("# Segments"), BO_READONLY },
/*4*/ { PD_TOGGLE, &groupReplace, "replace", 0, groupReplaceLabels, "", BC_HORZ|BC_NOBORDER } };
static paramGroup_t groupPG = { "group", 0, groupPLs, sizeof groupPLs/sizeof groupPLs[0] };


typedef struct {
		track_p trk;
		int segStart;
		int segEnd;
		} groupTrk_t, * groupTrk_p;
static dynArr_t groupTrk_da;
#define groupTrk(N) DYNARR_N( groupTrk_t, groupTrk_da, N )
typedef struct {
		int groupInx;
		EPINX_T ep1, ep2;
		PATHPTR_T path;
		BOOL_T flip;
		} pathElem_t, *pathElem_p;
typedef struct {
		int pathElemStart;
		int pathElemEnd;
		EPINX_T ep1, ep2;
		int conflicts;
		BOOL_T inGroup;
		BOOL_T done;
		} path_t, *path_p;
static dynArr_t path_da;
#define path(N) DYNARR_N( path_t, path_da, N )
static dynArr_t pathElem_da;
#define pathElem(N) DYNARR_N( pathElem_t, pathElem_da, N )
static int pathElemStart;


static BOOL_T CheckTurnoutEndPoint(
		trkSeg_p segs,
		coOrd pos,
		int end )
{
	coOrd pos1;
	DIST_T d;
	pos1 = GetSegEndPt( segs, end, FALSE, NULL );
	d = FindDistance( pos, pos1 );
	return ( d < connectDistance );
}

static char * FindPathBtwEP(
		track_p trk,
		EPINX_T ep1,
		EPINX_T ep2,
		BOOL_T * flip )
{
	struct extraData * xx = GetTrkExtraData( trk );
	char * cp, *cp0;
	int epN;
	coOrd pos1, pos2;
	int segInx;
	EPINX_T segEP;
 
	if ( GetTrkType(trk) != T_TURNOUT ) {
		if ( ep1+ep2 != 1 )
			AbortProg( "findPathBtwEP" );
		*flip = ( ep1 == 1 );
		if (GetTrkType(trk) == T_CORNU ) { 			// Cornu doesn't have a path but lots of segs!
			cp = CreateSegPathList(trk);
LOG( log_group, 2, ( " Group: Cornu path:%s \n", cp ) )
			return cp;
		}
		return "\1\0\0";
	}
	cp = (char *)xx->paths;
	pos1 = GetTrkEndPos(trk,ep1);
	Rotate( &pos1, xx->orig, -xx->angle );
	pos1.x -= xx->orig.x;
	pos1.y -= xx->orig.y;
	pos2 = GetTrkEndPos(trk,ep2);
	Rotate( &pos2, xx->orig, -xx->angle );
	pos2.x -= xx->orig.x;
	pos2.y -= xx->orig.y;
	while ( cp[0] ) {
		cp += strlen(cp)+1;
		while ( cp[0] ) {
			cp0 = cp;
			epN = -1;
			GetSegInxEP( cp[0], &segInx, &segEP );
			if ( CheckTurnoutEndPoint( &xx->segs[segInx], pos1, segEP ) )
				epN = 1;
			else if ( CheckTurnoutEndPoint( &xx->segs[segInx], pos2, segEP ) )
				epN = 0;
			cp += strlen(cp);
			if ( epN != -1 ) {
				GetSegInxEP( cp[-1], &segInx, &segEP );
				if ( CheckTurnoutEndPoint( &xx->segs[segInx], epN==0?pos1:pos2, 1-segEP ) ) {
					*flip = epN==0;
					return cp0;
				}
			}
			cp++;
		}
		cp++;
	}
	return NULL;
}


static int GroupShortestPathFunc(
		SPTF_CMD cmd,
		track_p trk,
		EPINX_T ep1,
		EPINX_T ep2,
		DIST_T dist,
		void * data )
{
	track_p trk1;
	path_t *pp;
	pathElem_t *ppp;
	BOOL_T flip;
	int inx;
	EPINX_T ep;
	coOrd pos1, pos2;
	ANGLE_T angle, ang1, ang2;

	switch ( cmd ) {
	case SPTC_MATCH:
		if ( !GetTrkSelected(trk) )
			return 0;
		trk1 = GetTrkEndTrk(trk,ep1);
		if ( trk1 == NULL )
			return 1;
		if ( !GetTrkSelected(trk1) )
			return 1;
		return 0;

	case SPTC_MATCHANY:
		return -1;

	case SPTC_ADD_TRK:
if (log_shortPath<=0||logTable(log_shortPath).level<4) LOG( log_group, 2, ( "     T%d[%d]\n", GetTrkIndex(trk), ep2 ) )
		DYNARR_APPEND( pathElem_t, pathElem_da, 10 );
		ppp = &pathElem(pathElem_da.cnt-1); 
		for ( inx=0; inx<groupTrk_da.cnt; inx++ ) {
			if ( groupTrk(inx).trk == trk ) {
				ppp->groupInx = inx;
				ppp->ep1 = ep1;
				ppp->ep2 = ep2;
				ppp->path = (PATHPTR_T)FindPathBtwEP( trk, ep1, ep2, &ppp->flip );
				return 0;
			}
		}
		AbortProg( "GroupShortestPathFunc(SPTC_ADD_TRK, T%d) - track not in group", GetTrkIndex(trk) );

	case SPTC_TERMINATE:
		ppp = &pathElem(pathElemStart);
		trk = groupTrk(ppp->groupInx).trk;
		pos1 = GetTrkEndPos( trk, ppp->ep2 );
		ang1 = GetTrkEndAngle( trk, ppp->ep2 );
		ppp = &pathElem(pathElem_da.cnt-1);
		trk = groupTrk(ppp->groupInx).trk;
		pos2 = GetTrkEndPos( trk, ppp->ep1 );
		ang2 = GetTrkEndAngle( trk, ppp->ep1 );
		ep1 = ep2 = -1;
		for ( ep=0; ep<tempEndPts_da.cnt; ep++ ) {
			if ( ep1 < 0 ) {
				dist = FindDistance( pos1, tempEndPts(ep).pos );
				angle = NormalizeAngle( ang1 - tempEndPts(ep).angle + connectAngle/2.0 );
				if ( dist < connectDistance && angle < connectAngle )
					ep1 = ep;
			}
			if ( ep2 < 0 ) {
				dist = FindDistance( pos2, tempEndPts(ep).pos );
				angle = NormalizeAngle( ang2 - tempEndPts(ep).angle + connectAngle/2.0 );
				if ( dist < connectDistance && angle < connectAngle )
					ep2 = ep;
			}
		}
		if ( ep1<0 || ep2<0 ) {
LOG( log_group, 2, ( " Remove: ep not found\n" ) )
			pathElem_da.cnt = pathElemStart;
			return 0;
		}
		for ( inx=0; inx<path_da.cnt; inx++ ) {
			pp = &path(inx);
			if ( ( ep1 < 0 || ( pp->ep1 == ep1 || pp->ep2 == ep1 ) ) &&
				 ( ep2 < 0 || ( pp->ep1 == ep2 || pp->ep2 == ep2 ) ) ) {
LOG( log_group, 2, ( " Remove: duplicate path P%d\n", inx ) )
				pathElem_da.cnt = pathElemStart;
				return 0;
			}
		}
		DYNARR_APPEND( path_t, path_da, 10 );
		pp = &path(path_da.cnt-1);
		memset( pp, 0, sizeof *pp );
		pp->pathElemStart = pathElemStart;
		pp->pathElemEnd = pathElem_da.cnt-1;
		pp->ep1 = ep1;
		pp->ep2 = ep2;
		pathElemStart = pathElem_da.cnt;
LOG( log_group, 2, ( " Keep\n" ) )
		return 0;

	case SPTC_IGNNXTTRK:
		if ( !GetTrkSelected(trk) )
			return 1;
		if ( ep1 == ep2 )
			return 1;
		if ( GetTrkEndPtCnt(trk) == 2 )
			return 0;
		if ( GetTrkType(trk) != T_TURNOUT )
			AbortProg( "GroupShortestPathFunc(IGNNXTTRK,T%d:%d,%d)", GetTrkIndex(trk), ep1, ep2 );
		return FindPathBtwEP( trk, ep2, ep1, &flip ) == NULL;

	case SPTC_VALID:
		return 1;

	}
	return 0;
}


static int CmpGroupOrder(
		const void * ptr1,
		const void * ptr2 )
{
	int inx1 = *(int*)ptr1;
	int inx2 = *(int*)ptr2;
	return path(inx1).conflicts-path(inx2).conflicts;
}

static coOrd endPtOrig;
static ANGLE_T endPtAngle;
static int CmpEndPtAngle(
		const void * ptr1,
		const void * ptr2 )
{
	ANGLE_T angle;
	trkEndPt_p epp1 = (trkEndPt_p)ptr1;
	trkEndPt_p epp2 = (trkEndPt_p)ptr2;
	
	angle = NormalizeAngle(FindAngle(endPtOrig,epp1->pos)-endPtAngle) - NormalizeAngle(FindAngle(endPtOrig,epp2->pos)-endPtAngle);
	return (int)angle;
}


static int ConflictPaths(
		path_p path0,
		path_p path1 )
{
	/* do these paths share an EP? */
	if ( path0->ep1 == path1->ep1 ) return TRUE;
	if ( path0->ep1 == path1->ep2 ) return TRUE;
	if ( path0->ep2 == path1->ep1 ) return TRUE;
	if ( path0->ep2 == path1->ep2 ) return TRUE;
	return FALSE;
}


static BOOL_T CheckPathEndPt(
		track_p trk,
		char cc,
		EPINX_T ep )
{
	struct extraData *xx = GetTrkExtraData(trk);
	wIndex_t segInx;
	EPINX_T segEP, epCnt;
	DIST_T d;
	coOrd pos;

	GetSegInxEP( cc, &segInx, &segEP );
	if ( ep ) segEP = 1-segEP;
	pos = GetSegEndPt( &xx->segs[segInx], segEP, FALSE, NULL );
	REORIGIN1( pos, xx->angle, xx->orig );
	epCnt = GetTrkEndPtCnt(trk);
	for ( ep=0; ep<epCnt; ep++ ) {
		d = FindDistance( pos, GetTrkEndPos( trk, ep ) );
		if ( d < connectDistance )
			return TRUE;
	}
	return FALSE;
}

static BOOL_T CheckForBumper(
		track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	char * cp;
	cp = (char *)xx->paths;
	while ( cp[0] ) {
		cp += strlen(cp)+1;
		while ( cp[0] ) {
			if ( !CheckPathEndPt( trk, cp[0], 0 ) ) return FALSE;
			while ( cp[0] )
				cp++;
			if ( !CheckPathEndPt( trk, cp[-1], 1 ) ) return FALSE;
			cp++;
		}
		cp++;
	}
	return TRUE;
}


static void GroupOk( void * junk )
{
	struct extraData *xx = NULL;
	turnoutInfo_t * to;
	int inx;
	EPINX_T ep, epCnt, epN;
	coOrd orig, size;
	long oldOptions;
	FILE * f = NULL;
	BOOL_T rc = TRUE;
	track_p trk, trk1;
	path_t * pp, *ppN;
	pathElem_p ppp;
	groupTrk_p groupP;
	BOOL_T flip, flip1, allDone;
	DIST_T dist;
	ANGLE_T angle, angleN;
	pathElem_t pathElemTemp;
	char * cp=NULL;
#ifdef SEGMAP
	pathElem_p ppp1, ppp2;
	int segInx1, segInx2;
	coOrd pos1, pos2;
	static dynArr_t segMap_da;
#define segMap(I,J)		DYNARR_N( char, segMap_da, (2*(I)+0)*trackSegs_da.cnt+(J) )
#define segAcc(I,J)		DYNARR_N( char, segMap_da, (2*(I)+1)*trackSegs_da.cnt+(J) )
#define segSum(I,J)		DYNARR_N( char, segMap_da, (2*(groupTrk_da.cnt)+0)*trackSegs_da.cnt+(J) )
#endif
	static dynArr_t trackSegs_da;
#define trackSegs(N) DYNARR_N( trkSeg_t, trackSegs_da, N )
	trkSeg_p segPtr;
	int segCnt;
	static dynArr_t conflictMap_da;
#define conflictMap( I, J )		DYNARR_N( int, conflictMap_da, (I)*(path_da.cnt)+(J) )
#define segFlip( N )			DYNARR_N( int, conflictMap_da, (N) )
	static dynArr_t groupOrder_da;
#define groupOrder( N )			DYNARR_N( int, groupOrder_da, N )
	static dynArr_t groupMap_da;
#define groupMap( I, J )		DYNARR_N( int, groupMap_da, (I)*(path_da.cnt+1)+(J) )
	int groupCnt;
	int pinx, pinx2, ginx, ginx2, gpinx2;
	trkEndPt_p endPtP;
	PATHPTR_T path;
	int pathLen;
	signed char pathChar;
	char *oldLocale = NULL;

#ifdef SEGMAP
	DYNARR_RESET( char, segMap_da );
#endif
	DYNARR_RESET( trkSeg_t, trackSegs_da );
	DYNARR_RESET( trkSeg_t, tempSegs_da );
	DYNARR_RESET( groupTrk_t, groupTrk_da );
	DYNARR_RESET( path_t, path_da );
	DYNARR_RESET( pathElem_t, pathElem_da );
	DYNARR_RESET( trkEndPt_t, tempEndPts_da );
	DYNARR_RESET( char, pathPtr_da );

	ParamUpdate( &groupPG );
	if ( groupManuf[0]==0 || groupDesc[0]==0 || groupPartno[0]==0 ) {
		NoticeMessage2( 0, MSG_GROUP_NONBLANK, _("Ok"), NULL );
		return;
	}
	sprintf( message, "%s\t%s\t%s", groupManuf, groupDesc, groupPartno );
	if ( strcmp( message, groupTitle ) != 0 ) {
		if ( FindCompound( FIND_TURNOUT|FIND_STRUCT, curScaleName, message ) )
			if ( !NoticeMessage2( 1, MSG_TODSGN_REPLACE, _("Yes"), _("No") ) )
				return;
		strcpy( groupTitle, message );
	}

	wDrawDelayUpdate( mainD.d, TRUE );
	/*
	 * Collect tracks
	 */
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if ( GetTrkSelected( trk ) ) {
			if ( IsTrack(trk) ) {
				DYNARR_APPEND( groupTrk_t, groupTrk_da, 10 );
				groupP = &groupTrk(groupTrk_da.cnt-1);
				groupP->trk = trk;
				groupP->segStart = trackSegs_da.cnt;
				if ( GetTrkType(trk) == T_TURNOUT ) {
					xx = GetTrkExtraData(trk);
					for ( pinx=0; pinx<xx->segCnt; pinx++ ) {
						segPtr = &xx->segs[pinx];
						if ( IsSegTrack(segPtr) ) {
							DYNARR_APPEND( trkSeg_t, trackSegs_da, 10 );
							trackSegs(trackSegs_da.cnt-1) = *segPtr;
							RotateSegs( 1, &trackSegs(trackSegs_da.cnt-1), zero, xx->angle );
							MoveSegs( 1, &trackSegs(trackSegs_da.cnt-1), xx->orig );
						} else {
							DrawSegs( &groupD, xx->orig, xx->angle, segPtr, 1, trackGauge, wDrawColorBlack );
						}
					}
				} else if (GetTrkType(trk) == T_BEZIER || GetTrkType(trk) == T_BZRLIN ) {
					DYNARR_APPEND(trkSeg_t, trackSegs_da, 10);
					segPtr = &trackSegs(trackSegs_da.cnt-1);
					GetBezierSegmentFromTrack(trk,segPtr);
				} else if (GetTrkType(trk) == T_CORNU) {
					GetBezierSegmentsFromCornu(trk,&trackSegs_da);  //Only give back Bezier - cant be undone
				} else {
					segCnt = tempSegs_da.cnt;
					oldOptions = groupD.options;
					groupD.options |= (DC_QUICK|DC_SIMPLE|DC_SEGTRACK);
					DrawTrack( trk, &groupD, wDrawColorBlack );
					groupD.options = oldOptions;
					DYNARR_APPEND( trkSeg_t, trackSegs_da, 10 );
					segPtr = &trackSegs(trackSegs_da.cnt-1);
					*segPtr = tempSegs( segCnt );
					if ( tempSegs_da.cnt != segCnt+1 ||
						 !IsSegTrack(segPtr) ) {
						NoticeMessage2( 0, MSG_CANNOT_GROUP_TRACK, _("Ok"), NULL );
						wHide( groupW );
						return;
					}
					tempSegs_da.cnt = segCnt;
				}
				groupP->segEnd = trackSegs_da.cnt-1;
			} else {
				DrawTrack( trk, &groupD, wDrawColorBlack );
			}
		}
	}

	if ( groupTrk_da.cnt>0 ) {
		if ( groupTrk_da.cnt > 128 ) {
			NoticeMessage( MSG_TOOMANYSEGSINGROUP, _("Ok"), NULL );
			wDrawDelayUpdate( mainD.d, FALSE );
			wHide( groupW );
			return;
		}

		/*
		 * Collect EndPts and find paths
		 */
		pathElemStart = 0;
		endPtOrig = zero;
		for ( inx=0; inx<groupTrk_da.cnt; inx++ ) {
			trk = groupTrk(inx).trk;
			epCnt = GetTrkEndPtCnt(trk);
			for ( ep=0; ep<epCnt; ep++ ) {
				trk1 = GetTrkEndTrk(trk,ep);
				if ( trk1 == NULL || !GetTrkSelected(trk1) ) {
					/* boundary EP */
					for ( epN=0; epN<tempEndPts_da.cnt; epN++ ) {
						dist = FindDistance( GetTrkEndPos(trk,ep), tempEndPts(epN).pos );
						angle = NormalizeAngle( GetTrkEndAngle(trk,ep) - tempEndPts(epN).angle + connectAngle/2.0 );
						if ( dist < connectDistance && angle < connectAngle )
							break;
					}
					if ( epN>=tempEndPts_da.cnt ) {
						DYNARR_APPEND( trkEndPt_t, tempEndPts_da, 10 );
						endPtP = &tempEndPts(tempEndPts_da.cnt-1);
						memset( endPtP, 0, sizeof *endPtP );
						endPtP->pos = GetTrkEndPos(trk,ep);
						endPtP->angle = GetTrkEndAngle(trk,ep);
						endPtP->track = trk1;
						endPtP->index = (trk1?GetEndPtConnectedToMe(trk1,trk):-1);
						endPtOrig.x += endPtP->pos.x;
						endPtOrig.y += endPtP->pos.y;
					}
				}
			}
		}
		if ( tempEndPts_da.cnt <= 0 ) {
			NoticeMessage( _("No endpts"), _("Ok"), NULL );
			wDrawDelayUpdate( mainD.d, FALSE );
			wHide( groupW );
			return;
		}
		if ( groupTrk_da.cnt == 1 && GetTrkType( groupTrk(0).trk ) == T_TURNOUT ) {
			path = xx->paths;
			pathLen = xx->pathLen;
			goto groupSimpleTurnout;
		}

		/* Make sure no turnouts in groupTrk list have a path end which is not an EndPt */
		for ( inx=0; inx<groupTrk_da.cnt; inx++ ) {
			trk = groupTrk(0).trk;
			if ( GetTrkType( trk ) == T_TURNOUT ) {
				if ( GetTrkEndPtCnt( trk ) < 2 ) {
					cp = MSG_CANT_GROUP_BUMPER1;
					break;
				}
				if ( !CheckForBumper( trk ) ) {
					cp = MSG_CANT_GROUP_BUMPER2;
					break;
				}
			}
		}
		if ( inx < groupTrk_da.cnt ) {
			NoticeMessage2( 0, cp, _("Ok"), NULL, GetTrkIndex( trk ) );
			DrawTrack( trk, &mainD, wDrawColorWhite );
			ClrTrkBits( trk, TB_SELECTED );
			/* TODO redraw the endpt of the trks this one is connected to */
			DrawTrack( trk, &mainD, wDrawColorBlack );
			wDrawDelayUpdate( mainD.d, FALSE );
			wHide( groupW );
			return;
		}

		/*
		 * Sort EndPts by angle
		 */
		endPtOrig.x /= tempEndPts_da.cnt;
		endPtOrig.y /= tempEndPts_da.cnt;
		angleN = 270.0;
		epN = -1;
		for ( ep=0; ep<tempEndPts_da.cnt; ep++ ) {
			angle = FindAngle(endPtOrig,tempEndPts(ep).pos);
			if ( fabs(angle-270.0) < angleN ) {
				epN = ep;
				angleN = fabs(angle-270.0);
				endPtAngle = angle;
			}
		}
		qsort( tempEndPts_da.ptr, tempEndPts_da.cnt, sizeof *endPtP, CmpEndPtAngle );
		if ( NormalizeAngle( tempEndPts(0).angle - tempEndPts(tempEndPts_da.cnt-1).angle ) >
			 NormalizeAngle( tempEndPts(1).angle - tempEndPts(0).angle ) ) {
#ifdef LATER
		if ( endPtAngle-FindAngle(endPtOrig,tempEndPts(tempEndPts_da.cnt-1).pos) >
			 FindAngle(endPtOrig,tempEndPts(1).pos)-endPtAngle ) {
#endif
			for ( ep=1; ep<(tempEndPts_da.cnt+1)/2; ep++ ) {
				trkEndPt_t tempEndPt;
				tempEndPt = tempEndPts(ep);
				tempEndPts(ep) = tempEndPts(tempEndPts_da.cnt-ep);
				tempEndPts(tempEndPts_da.cnt-ep) = tempEndPt;
			}
		}

		/*
		 * Find shortest Paths
		 */
		for ( inx=0; inx<groupTrk_da.cnt; inx++ ) {
			trk = groupTrk(inx).trk;
			epCnt = GetTrkEndPtCnt(trk);
			for ( ep=0; ep<epCnt; ep++ ) {
				trk1 = GetTrkEndTrk(trk,ep);
				if ( trk1 == NULL || !GetTrkSelected(trk1) ) {
					/* boundary EP */
					rc = FindShortestPath( trk, ep, FALSE, GroupShortestPathFunc, NULL );
				}
			}
		}

		/*
		 * Flip paths so they align
		 */
		if ( path_da.cnt == 0 ) {
			NoticeMessage( _("No paths"), _("Ok"), NULL );
			wDrawDelayUpdate( mainD.d, FALSE );
			wHide( groupW );
			return;
		}
		allDone = FALSE;
		path(0).done = TRUE;
		while ( !allDone ) {
			allDone = TRUE;
			inx = -1;
			for ( pinx=0; pinx<path_da.cnt; pinx++ ) {
				pp = &path(pinx);
				if ( pp->done ) continue;
				for ( pinx2=0; pinx2<path_da.cnt; pinx2++ ) {
					if ( pinx2==pinx ) continue;
					ppN = &path(pinx2);
					if ( pp->ep1 == ppN->ep1 ||
						 pp->ep2 == ppN->ep2 ) {
						pp->done = TRUE;
						allDone = FALSE;
LOG( log_group, 1, ( "P%d aligns with P%d\n", pinx, pinx2 ) );
						break;
					}
					if ( pp->ep1 == ppN->ep2 ||
						 pp->ep2 == ppN->ep1 ) {
						pp->done = TRUE;
						allDone = FALSE;
LOG( log_group, 1, ( "P%d aligns flipped with P%d\n", pinx, pinx2 ) );
						inx = (pp->pathElemStart+pp->pathElemEnd-1)/2;
						for ( ginx=pp->pathElemStart,ginx2=pp->pathElemEnd; ginx<=inx; ginx++,ginx2-- ) {
							pathElemTemp = pathElem(ginx);
							pathElem(ginx) = pathElem(ginx2);
							pathElem(ginx2) = pathElemTemp;
						}
						for ( ginx=pp->pathElemStart; ginx<=pp->pathElemEnd; ginx++ ) {
							ppp = &pathElem(ginx);
							ep = ppp->ep1;
							ppp->ep1 = ppp->ep2;
							ppp->ep2 = ep;
							ppp->flip = !ppp->flip;
						}
						ep = pp->ep1;
						pp->ep1 = pp->ep2;
						pp->ep2 = ep;
						break;
					}
				}
				if ( inx<0 && !pp->done )
					inx = pinx;
			}
			if ( allDone && inx>=0 ) {
				allDone = FALSE;
				path(inx).done = TRUE;
			} 
		}
if ( log_group >= 1 && logTable(log_group).level > log_group ) {
	LogPrintf( "Group Paths\n" );
	for ( pinx=0; pinx<path_da.cnt; pinx++ ) {
		pp = &path(pinx);
		LogPrintf( "P%2d:%d.%d ", pinx, pp->ep1, pp->ep2 );
		for ( pinx2=pp->pathElemEnd; pinx2>=pp->pathElemStart; pinx2-- ) {
			ppp = &pathElem(pinx2);
			LogPrintf( " %sT%d:%d.%d", ppp->flip?"-":"", GetTrkIndex(groupTrk(ppp->groupInx).trk), ppp->ep1, ppp->ep2 );
		}
		LogPrintf( "\n" );
	}
}

#ifdef SEGMAP
		DYNARR_SET( char, segMap_da, 2 * trackSegs_da.cnt * path_da.cnt + 2 );
		memset( segMap_da.ptr, 0, segMap_da.max * sizeof segMap(0,0) );
		for ( inx=0; inx<path_da.cnt; inx++ ) {
			pp = &path(inx);
			for ( inx2=pp->pathElem_da.cnt-1; inx2>=0; inx2-- ) {
				ppp = &pathElem(pp->pathElemStart+inx2);
				groupP = &groupTrk(ppp->groupInx);
				if ( GetTrkEndPtCnt(groupP->trk) == 2 ) {
					segMap(inx,groupP->segStart) = 1;
					continue;
				}
				cp = ppp->path;
				if ( cp == NULL )
					continue;
				segInx1 = cp[0]-1;
				for ( ; *cp; cp++ )
					segMap(inx,groupP->segInx+cp[0]-1) = 1;
				segInx2 = cp[-1]-1;
				pos1 = GetSegEndPt( &trackSegs(groupP->segInx+segInx1), ppp->flip?1:0, FALSE, NULL );
				pos2 = GetSegEndPt( &trackSegs(groupP->segInx+segInx2), ppp->flip?0:1, FALSE, NULL );
				for ( inx3=0; inx3<groupP->segCnt; inx3++ ) {
					if ( inx3 == segInx1 || inx3 == segInx2 ) continue;
					if ( segMap(inx,groupP->segInx+inx3) != 0 ) continue;
					if ( CheckTurnoutEndPoint( &trackSegs(groupP->segInx+inx3), pos1, 0 ) )
						segMap(inx,inx3) = 2;
					else if ( CheckTurnoutEndPoint( &trackSegs(groupP->segInx+inx3), pos2, 0 ) )
						segMap(inx,groupP->segInx+inx3) = 2;
				}
			}
		}
if ( log_group >= 1 && logTable(log_group).level > log_group ) {
	LogPrintf( "Path to Segment Map\n   ");
	for ( inx=0; inx<groupTrk_da.cnt; inx++ ) {
		groupP = &groupTrk(inx);
		LogPrintf( "%2d", GetTrkIndex(groupP->trk) );
		for ( inx2=1; inx2<groupP->segCnt; inx2++ ) LogPrintf( "--" );
	}
	LogPrintf( "\n   " );
	for ( inx=0; inx<groupTrk_da.cnt; inx++ ) {
		groupP = &groupTrk(inx);
		for ( inx2=0; inx2<groupP->segCnt; inx2++ )
			LogPrintf( "%2d", inx2+1 );
	}
	LogPrintf( "\n" );
	for ( inx=0; inx<path_da.cnt; inx++ ) {
		LogPrintf( "%2d ", inx );
		for ( inx2=0; inx2<trackSegs_da.cnt; inx2++ )
			LogPrintf( "%2d", segMap(inx,inx2) );
		LogPrintf("\n");
	}
}
#endif

		/*
		 * Create Conflict Map
		 */
		DYNARR_SET( int, conflictMap_da, path_da.cnt*path_da.cnt );
		memset( conflictMap_da.ptr, 0, conflictMap_da.max * sizeof conflictMap(0,0) );
		for ( pinx=0; pinx<path_da.cnt; pinx++ ) {
			for ( pinx2=pinx+1; pinx2<path_da.cnt; pinx2++ ) {
				if ( ConflictPaths( &path(pinx), &path(pinx2) ) ) {
					conflictMap( pinx, pinx2 ) = conflictMap( pinx2, pinx ) = TRUE;
					path(pinx).conflicts++;
					path(pinx2).conflicts++;
				}
			}
		}

		/*
		 * Sort Paths by number of conflicts
		 */
		DYNARR_SET( int, groupOrder_da, path_da.cnt );
		for ( pinx=0; pinx<path_da.cnt; pinx++ ) groupOrder(pinx) = pinx;
		qsort( groupOrder_da.ptr, path_da.cnt, sizeof groupOrder(0), CmpGroupOrder );

		/*
		 * Group Paths, 1st pass: 
		 */
		DYNARR_SET( int, groupMap_da, path_da.cnt*(path_da.cnt+1) );
		memset( groupMap_da.ptr, -1, groupMap_da.max * sizeof groupMap(0,0) );
		groupCnt = 0;
		for ( pinx=0; pinx<path_da.cnt; pinx++ ) {
			pp = &path(groupOrder(pinx));
			if ( pp->inGroup ) continue;
			pp->inGroup = TRUE;
			groupCnt++;
			groupMap( groupCnt-1, 0 ) = groupOrder(pinx);
			ginx = 1;
			for ( pinx2=pinx+1; pinx2<path_da.cnt; pinx2++ ) {
				gpinx2 = groupOrder(pinx2);
				if ( path(gpinx2).inGroup ) continue;
				for ( ginx2=0; ginx2<ginx && !conflictMap(groupMap(groupCnt-1,ginx2),gpinx2); ginx2++ );
				if ( ginx2<ginx ) continue;
				path(gpinx2).inGroup = TRUE;
				groupMap( groupCnt-1, ginx++ ) = gpinx2;
			}
		}

		/*
		 * Group Paths: 2nd pass:
		 */
		for ( pinx=0; pinx<groupCnt; pinx++ ) {
			for ( ginx=0; groupMap(pinx,ginx)>=0; ginx++ );
			for ( pinx2=0; pinx2<path_da.cnt; pinx2++ ) {
				gpinx2 = groupOrder(pinx2);
				for ( ginx2=0; ginx2<ginx && groupMap(pinx,ginx2)!=gpinx2; ginx2++ );
				if ( ginx2<ginx ) continue;		/* already on list */
				for ( ginx2=0; ginx2<ginx && !conflictMap(groupMap(pinx,ginx2),gpinx2); ginx2++ );
				if ( ginx2<ginx ) continue;		/* conflicts with someone on list */
				groupMap(pinx,ginx++) = gpinx2;
			}
		}

if ( log_group >= 1 && logTable(log_group).level > log_group ) {
	LogPrintf( "Group Map\n");
	for ( pinx=0; pinx<groupCnt; pinx++ ) {
		LogPrintf( "G%d:", pinx );
		for ( ginx=0; groupMap(pinx,ginx) >= 0; ginx++ )
			LogPrintf( " %d", groupMap(pinx,ginx) );
		LogPrintf( "\n" );
	}
}

#ifdef SEGMAP
	for ( inx=0; inx<path_da.cnt; inx++ ) {
		for ( inx2=0; inx2<tempSegs_da.cnt; inx2++ ) {
		groupInx = 0;
		memset( &SegTotal(0), 0, tempSegs_da.cnt * sizeof SegAcc(0) );
		while (1) {
			memcpy( &SegAcc(0), &SegTotal(0), tempSegs_da.cnt * sizeof SegAcc(0) );
			collision = FALSE;
			for ( inx=0; inx<path_da.cnt; inx++ ) {
				pp = path(0);
				if ( pp->groupInx < 0 ) continue;
				for ( inx2=0; inx2<tempSegs_da.cnt; inx2++ ) {
					if ( !segMap(inx,inx2) ) continue;
					if ( SegAcc(inx2) ) {
						collision = TRUE;
						break;
					}
					SegAcc(inx2) = TRUE;
				}
			}
			if ( collision )
		}
#endif

		/*
		 * Count number of times each segment is used as flipped
		 */
		DYNARR_SET( int, conflictMap_da, trackSegs_da.cnt );
		memset( &segFlip(0), 0, trackSegs_da.cnt * sizeof segFlip(0) );
		for ( pinx=0; pinx<pathElem_da.cnt; pinx++ ) {
			ppp = &pathElem(pinx);
			for ( path=ppp->path; *path; path++ ) {
				inx = *path;
				if ( inx<0 )
					inx = - inx;
				if ( inx > trackSegs_da.cnt )
					AbortProg( "inx > trackSegs_da.cnt" );
				flip = *path<0;
				if ( ppp->flip )
					flip = !flip;
				inx += groupTrk(ppp->groupInx).segStart - 1;
				if ( !flip )
					segFlip(inx)++;
				else
					segFlip(inx)--;
			}
		}

		/*
		 * Flip each segment that is used as flipped more than not
		 */
		for ( pinx=0; pinx<trackSegs_da.cnt; pinx++ ) {
			if ( segFlip(pinx) < 0 ) {
LOG( log_group, 1, ( "Flipping Segment %d\n", pinx+1 ) );
					SegProc( SEGPROC_FLIP, &trackSegs(pinx), NULL );
			}
		}

		/*
		 * Output Path lists
		 */
		for ( pinx=0; pinx<groupCnt; pinx++ ) {
			sprintf( message, "P%d", pinx );
			inx = pathPtr_da.cnt;
			DYNARR_SET( char, pathPtr_da, inx+(int)strlen(message)+1 );
			memcpy( &pathPtr(inx), message, pathPtr_da.cnt-inx );
			for ( ginx=0; groupMap(pinx,ginx) >= 0; ginx++ ) {
				pp = &path(groupMap(pinx,ginx));
				for ( pinx2=pp->pathElemEnd; pinx2>=pp->pathElemStart; pinx2-- ) {
					ppp = &pathElem( pinx2 );
					groupP = &groupTrk( ppp->groupInx );
					path = ppp->path;
					flip = ppp->flip;
					if ( path == NULL )
						AbortProg( "Missing Path T%d:%d.%d", GetTrkIndex(groupP->trk), ppp->ep2, ppp->ep1 );
					if ( flip ) path += strlen((char *)path)-1;
					while ( *path && (path >= ppp->path) ) {      //Add Guard for flip backwards
						DYNARR_APPEND( char, pathPtr_da, 10 );
						pathChar = *path;
						flip1 = flip;
						if ( pathChar < 0 ) {
							flip1 = !flip;
							pathChar = - pathChar;
						}
						pathChar = groupP->segStart+pathChar;
						if ( segFlip(pathChar-1)<0 )
							flip1 = ! flip1;
						if ( flip1 ) pathChar = - pathChar;
						pathPtr(pathPtr_da.cnt-1) = pathChar;
						path += (flip?-1:1);
					}
				}
				DYNARR_APPEND( char, pathPtr_da, 10 );
				pathPtr(pathPtr_da.cnt-1) = 0;
			}
			DYNARR_APPEND( char, pathPtr_da, 10 );
			pathPtr(pathPtr_da.cnt-1) = 0;
		}
		DYNARR_APPEND( char, pathPtr_da, 10 );
		pathPtr(pathPtr_da.cnt-1) = 0;
		path = (PATHPTR_T)&pathPtr(0);
		pathLen = pathPtr_da.cnt;

groupSimpleTurnout:
		/*
		 * Copy and Reorigin Segments
		 */
		if ( tempSegs_da.cnt > 0 ) {
			inx = trackSegs_da.cnt;
			DYNARR_SET( trkSeg_t, trackSegs_da, trackSegs_da.cnt+tempSegs_da.cnt );
			memcpy( &trackSegs(inx), tempSegs_da.ptr, tempSegs_da.cnt*sizeof trackSegs(0) );
			CloneFilledDraw( tempSegs_da.cnt, &trackSegs(inx), TRUE );
		}
		GetSegBounds( zero, 0, trackSegs_da.cnt, &trackSegs(0), &orig, &size );
		orig.x = - tempEndPts(0).pos.x;
		orig.y = - tempEndPts(0).pos.y;
		MoveSegs( trackSegs_da.cnt, &trackSegs(0), orig );
		for ( ep=0; ep<tempEndPts_da.cnt; ep++ ) {
			tempEndPts(ep).pos.x += orig.x;
			tempEndPts(ep).pos.y += orig.y;
		}

		/*
		 * Final: create new definition
		 */
		CheckPaths( trackSegs_da.cnt, &trackSegs(0), path );
		to = CreateNewTurnout( curScaleName, groupTitle, trackSegs_da.cnt, &trackSegs(0), pathLen, path, tempEndPts_da.cnt, &tempEndPts(0), TRUE );
#ifdef LATER
		if ( xx )
			to->customInfo = xx->customInfo;
#endif
		f = OpenCustom("a");
		if (f && to) {
			oldLocale = SaveLocale("C");
			rc &= fprintf( f, "TURNOUT %s \"%s\"\n", curScaleName, PutTitle(to->title) )>0;
#ifdef LATER
			if ( to->customInfo )
				rc &= fprintf( f, "\tU %s\n", to->customInfo )>0;
#endif
			rc &= WriteCompoundPathsEndPtsSegs( f, path, trackSegs_da.cnt, &trackSegs(0), tempEndPts_da.cnt, &tempEndPts(0) );
		}
		if ( groupReplace ) {
			UndoStart( _("Group Tracks"), "group" );
			orig.x = - orig.x;
			orig.y = - orig.y;
			for ( ep=0; ep<tempEndPts_da.cnt; ep++ ) {
				endPtP = &tempEndPts(ep);
				if ( endPtP->track ) {
					trk = GetTrkEndTrk( endPtP->track, endPtP->index );
					epN = GetEndPtConnectedToMe( trk, endPtP->track );
					DrawEndPt( &mainD, endPtP->track, endPtP->index, wDrawColorWhite );
					DrawEndPt( &mainD, trk, epN, wDrawColorWhite );
					DisconnectTracks( trk, epN, endPtP->track, endPtP->index );
				}
				endPtP->pos.x += orig.x;
				endPtP->pos.y += orig.y;
			}
			trk = NULL;
			while ( TrackIterate( &trk ) ) {
				if ( GetTrkSelected( trk ) ) {
					DrawTrack( trk, &mainD, wDrawColorWhite );
					UndoDelete( trk );
					trackCount--;
				}
			}
			trk = NewCompound( T_TURNOUT, 0, orig, 0.0, to->title, tempEndPts_da.cnt, &tempEndPts(0), pathLen, (char *)path, trackSegs_da.cnt, &trackSegs(0) );
			SetTrkVisible( trk, TRUE );

			SetTrkVisible( trk, TRUE );
			for ( ep=0; ep<tempEndPts_da.cnt; ep++ ) {
				if ( tempEndPts(ep).track ) {
					ConnectTracks( trk, ep, tempEndPts(ep).track, (EPINX_T)tempEndPts(ep).index );
					DrawEndPt( &mainD, tempEndPts(ep).track, (EPINX_T)tempEndPts(ep).index, GetTrkColor( tempEndPts(ep).track, &mainD ) );
				}
			}
			DrawNewTrack( trk );
			EnableCommands();
		}
	} else {
		CloneFilledDraw( tempSegs_da.cnt, &tempSegs(0), TRUE );
		GetSegBounds( zero, 0, tempSegs_da.cnt, &tempSegs(0), &orig, &size );
		orig.x = - orig.x;
		orig.y = - orig.y;
		MoveSegs( tempSegs_da.cnt, &tempSegs(0), orig );
		to = CreateNewStructure( curScaleName, groupTitle, tempSegs_da.cnt, &tempSegs(0), TRUE );
		f = OpenCustom("a");
		if (f && to) {
			oldLocale = SaveLocale("C");
			rc &= fprintf( f, "STRUCTURE %s \"%s\"\n", curScaleName, PutTitle(groupTitle) )>0;
#ifdef LATER
			if ( to->customInfo )
				rc &= fprintf( f, "\tU %s\n", to->customInfo )>0;
#endif
			rc &= WriteSegs( f, tempSegs_da.cnt, &tempSegs(0) );
		}
		if ( groupReplace ) {
			UndoStart( _("Group Tracks"), "group" );
			trk = NULL;
			while ( TrackIterate( &trk ) ) {
				if ( GetTrkSelected( trk ) ) {
					DrawTrack( trk, &mainD, wDrawColorWhite );
					UndoDelete( trk );
					trackCount--;
				}
			}
			orig.x = - orig.x;
			orig.y = - orig.y;
			trk = NewCompound( T_STRUCTURE, 0, orig, 0.0, groupTitle, 0, NULL, 0, "", tempSegs_da.cnt, &tempSegs(0) );
			SetTrkVisible( trk, TRUE );
			DrawNewTrack( trk );
			EnableCommands();
		}
	}
	if (f) fclose(f);
	RestoreLocale(oldLocale);
	DoChangeNotification( CHANGE_PARAMS );
	wHide( groupW );
	wDrawDelayUpdate( mainD.d, FALSE );
	groupDesc[0] = '\0';
	groupPartno[0] = '\0';
}


EXPORT void DoGroup( void )
{
	track_p trk = NULL;
	struct extraData *xx;
	TRKTYP_T trkType;
	xx = NULL;
	groupSegCnt = 0;
	groupCompoundCount = 0;

	while ( TrackIterate( &trk ) ) {
		if ( GetTrkSelected( trk ) ) {
			trkType = GetTrkType(trk);
			if ( trkType == T_TURNOUT || trkType == T_STRUCTURE ) {
				xx = GetTrkExtraData(trk);
				groupSegCnt += xx->segCnt;
				GroupCopyTitle( xtitle(xx) );
			} else
				groupSegCnt += 1;
		}
	}
	if ( groupSegCnt <= 0 ) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	sprintf( groupTitle, "%s\t%s\t%s", groupManuf, groupDesc, groupPartno );
	if ( log_group < 0 )
		log_group = LogFindIndex( "group" );
	if ( !groupW ) {
		ParamRegister( &groupPG );
		groupW = ParamCreateDialog( &groupPG, MakeWindowTitle(_("Group Objects")), _("Ok"), GroupOk, wHide, TRUE, NULL, F_BLOCK, NULL );
		groupD.dpi = mainD.dpi;
	}
	ParamLoadControls( &groupPG );
	wShow( groupW );
}

