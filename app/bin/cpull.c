/** \file cpull.c
 * Pull and Tighten commands
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

#include "cselect.h"
#include "compound.h"
#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "track.h"
#include "utility.h"

int debugPull = 0;

ANGLE_T maxA = 15.0;
DIST_T maxD = 3.0;
ANGLE_T littleA = 1.0;
DIST_T littleD = 0.1;

static double factorX=10, factorY=100, factorA=0.2;
typedef struct {
		double X, Y, A, T;
		} cost_t;
static cost_t sumCosts;
static cost_t maxCosts;
static int maxCostsInx;
typedef struct {
		coOrd p[2];
		ANGLE_T a[2];
		ANGLE_T angle;
		DIST_T dist;
		track_p trk;
		EPINX_T ep[2];
		cost_t costs[2];
		double contrib;
		coOrd pp;
		} section_t, *section_p;
static dynArr_t section_da;
#define section(N) DYNARR_N( section_t, section_da, N )
static double contribL, contribR;


typedef enum { freeEnd, connectedEnd, loopEnd } ending_e;

/*
 * Utilities
 */
static ending_e GetConnectedTracks(
		track_p trk,
		EPINX_T ep,
		track_p endTrk,
		EPINX_T endEp )
{
	track_p trk1;
	EPINX_T ep1, ep2;
	section_p sp;
	while (1) {
		if (trk == endTrk) {
			ep2 = endEp;
			trk1 = NULL;
		} else {
			ep2 = GetNextTrk( trk, ep, &trk1, &ep1, 0 );
			if (trk1 == NULL)
				return freeEnd;
		}
		if ( ep2 >= 0 ) {
			int inx;
			for (inx=0;inx<section_da.cnt;inx++) {
				if ( section(inx).trk == trk ) {
					AbortProg("GetConnectedTracks(T%d already selected)", GetTrkIndex(trk));
				}
			}
		}
		DYNARR_APPEND( section_t, section_da, 10 );
		sp = &section(section_da.cnt-1);
		sp->trk = trk;
		sp->ep[0] = ep;
		sp->ep[1] = ep2;
		sp->p[0] = GetTrkEndPos(trk,ep);
		sp->costs[0].X = sp->costs[0].Y = sp->costs[0].A = sp->costs[0].T =
		sp->costs[1].X = sp->costs[1].Y = sp->costs[1].A = sp->costs[1].T =0.0;
		sp->a[0] = GetTrkEndAngle(trk,ep);
		sp->a[1] = 0;
		if (ep2 < 0)
			return connectedEnd;
		sp->p[1] = GetTrkEndPos(trk,ep2);
		sp->dist = FindDistance( GetTrkEndPos(trk,ep), GetTrkEndPos(trk,ep2) );
		sp->angle = NormalizeAngle( GetTrkEndAngle(trk,ep2)-GetTrkEndAngle(trk,ep) );
		sp->a[1] = GetTrkEndAngle(trk,ep2);
		if (trk == endTrk)
			return loopEnd;
		trk = trk1;
		ep = ep1;
	}
}

/*
 * Simple move to connect
 */
static void MoveConnectedTracks(
		track_p trk1,
		EPINX_T ep1,
		coOrd pos,
		ANGLE_T angle )
{
	EPINX_T ep, ep2;
	track_p trk;
	coOrd p;
	ANGLE_T a;

	while (1) {
		p = GetTrkEndPos( trk1, ep1 );
		p.x = pos.x - p.x;
		p.y = pos.y - p.y;
		a = angle - GetTrkEndAngle( trk1, ep1 );
		UndoModify( trk1 );
		UndrawNewTrack( trk1 );
		MoveTrack( trk1, p );
		RotateTrack( trk1, pos, a );
		DrawNewTrack( trk1 );
		ep2 = GetNextTrk( trk1, ep1, &trk, &ep, 0 );
		if (trk==NULL)
			return;
		if (ep2 < 0)
			AbortProg("MoveConnectedTracks(T%d rooted)", GetTrkIndex(trk1));
		angle = NormalizeAngle(GetTrkEndAngle( trk1, ep2 )+180.0);
		pos = GetTrkEndPos( trk1, ep2 );
		trk1 = trk;
		ep1 = ep;
	}
}


/*
 * Helpers for complex case
 */
static void ReverseSectionList(
		int start, int end )
{
	int up, down;
	section_t tmpUp, tmpDown;
	EPINX_T tmpEp;
	coOrd tmpPos;
	ANGLE_T tmpA;
	for (down=start,up=end-1; down<=up; down++,up-- ) {
		tmpUp = section(up);
		tmpEp=tmpUp.ep[0]; tmpUp.ep[0]=tmpUp.ep[1]; tmpUp.ep[1]=tmpEp;
		tmpPos=tmpUp.p[0]; tmpUp.p[0]=tmpUp.p[1]; tmpUp.p[1]=tmpPos;
		tmpA=tmpUp.a[0]; tmpUp.a[0]=tmpUp.a[1]; tmpUp.a[1]=tmpA;
		tmpUp.angle = NormalizeAngle( 360.0-tmpUp.angle );
		tmpDown = section(down);
		tmpEp=tmpDown.ep[0]; tmpDown.ep[0]=tmpDown.ep[1]; tmpDown.ep[1]=tmpEp;
		tmpPos=tmpDown.p[0]; tmpDown.p[0]=tmpDown.p[1]; tmpDown.p[1]=tmpPos;
		tmpA=tmpDown.a[0]; tmpDown.a[0]=tmpDown.a[1]; tmpDown.a[1]=tmpA;
		tmpDown.angle = NormalizeAngle( 360.0-tmpDown.angle );
		section(up) = tmpDown;
		section(down) = tmpUp;
	}
}


/*
 * Evaluators
 */


#define ANGLE_FAULT		(1<<0)
#define DIST_FAULT		(1<<1)

static int CheckConnections( void )
{
	section_p sp;
	int rc;
	int inx;
	DIST_T dist;
	ANGLE_T angle;
	rc = 0;
	for (inx = 1; inx<section_da.cnt; inx++) {
		sp = &section(inx);
		dist = FindDistance( sp[0].p[0], sp[-1].p[1] );
		angle = NormalizeAngle( sp[0].a[0] - sp[-1].a[1] + 180.0 + connectAngle/2 );
		if (dist > connectDistance)
			rc |= DIST_FAULT;
		if (angle > connectAngle)
			rc |= ANGLE_FAULT;
	}
	return rc;
}


static void ComputeCost(
		coOrd p,
		ANGLE_T a,
		section_p sp )
{
	ANGLE_T da;
	coOrd pp;
	da = NormalizeAngle( sp->a[0]+180.0-a );
	if (da>180)
		da = 360.0-a;
	sp->costs[0].A = da*factorA;
	pp = sp->p[0];
	Rotate( &pp, p, -a );
	pp.x -= p.x;
	pp.y -= p.y;
	sp->costs[0].X = fabs(pp.y*factorX);
	sp->costs[0].Y = fabs(pp.x*factorY);
	if ( pp.x < -0.010 )
		sp->costs[0].X *= 100;
	sp->costs[0].T = sp->costs[0].X+sp->costs[0].Y;
}


static void ComputeCosts( void )
{
	int inx;
	section_p sp;
	maxCosts.A = maxCosts.X = maxCosts.Y = maxCosts.T = 0.0;
	sumCosts.A = sumCosts.X = sumCosts.Y = sumCosts.T = 0.0;
	maxCostsInx = -1;
	for (inx=1; inx<section_da.cnt; inx++) {
		sp = &section(inx);
		ComputeCost( sp[-1].p[1], sp[-1].a[1], sp );
if (debugPull) {
/*printf("%2d:  X=%0.3f Y=%0.3f A=%0.3f T=%0.3f\n", inx, sp->costs[0].X, sp->costs[0].Y, sp->costs[0].A, sp->costs[0].T );*/
}
		sumCosts.A += sp->costs[0].A;
		sumCosts.X += sp->costs[0].X;
		sumCosts.Y += sp->costs[0].Y;
		sumCosts.T += sp->costs[0].T;
		if ( sp->costs[0].T > maxCosts.T ) {
			maxCosts.A = sp->costs[0].A;
			maxCosts.X = sp->costs[0].X;
			maxCosts.Y = sp->costs[0].Y;
			maxCosts.T = sp->costs[0].T;
			maxCostsInx = inx;
		}
	}
}


static double ComputeContrib(
		DIST_T dist,
		ANGLE_T angle,
		int start,
		int end,
		EPINX_T ep )
{
	int inx;
	section_p sp;
	ANGLE_T a;
	double contrib = 0.0;
	for (inx=start; inx<=end; inx++ ) {
		sp = &section(inx);
		a = NormalizeAngle(angle - sp->a[ep] + 180.0);
		sp->contrib = (a>270.0||a<90.0)?fabs(cos(a)):0.0;
		contrib += sp->contrib;
	}
	return contrib;
}


static void ComputeContribs( coOrd *rp1 )
{
	section_p sp = &section(maxCostsInx);
	double aveX=sumCosts.X/section_da.cnt, aveY=sumCosts.Y/section_da.cnt;
	ANGLE_T angle;
	DIST_T dist;
	coOrd p0=sp[0].p[0], p1=sp[-1].p[1];

	Rotate( &p1, p0, -sp[0].a[0] );
	p1.x -= p0.x;
	p1.y -= p0.y;
	if (sp->costs[0].X > 0.000001 && sp->costs[0].X > aveX)
		p1.y *= 1-aveX/sp->costs[0].X;
	else
		p1.y = 0.0;
	if (sp->costs[0].Y > 0.000001 && sp->costs[0].Y > aveY)
		p1.x *= 1-aveY/sp->costs[0].Y;
	else
		p1.x = 0.0;
	Rotate( &p1, zero, sp[0].a[0] );
	dist = FindDistance( zero, p1 );
	angle = FindAngle( zero, p1 );
	contribL = ComputeContrib( dist, NormalizeAngle(angle+180.0), 1, maxCostsInx-1, 0 );
	contribR = ComputeContrib( dist, angle, maxCostsInx, section_da.cnt-2, 1 );

	if (debugPull) {
		printf( "Minx=%d D=%0.3f A=%0.3f X=%0.3f Y=%0.3f L=%0.3f R=%0.3f\n",
			maxCostsInx, dist, angle, p1.x, p1.y, contribL, contribR );
		sp = &section(0);
		printf( " 0[%d]                                                     [%0.3f %0.3f] [%0.3f %0.3f]\n",
			GetTrkIndex(sp->trk), 
			sp[0].p[0].x, sp[0].p[0].y, sp[0].p[1].x, sp[0].p[1].y );
	}
	*rp1 = p1;
}


/*
 * Shufflers
 */
static void AdjustSection(
		section_p sp,
		coOrd amount )
{
	sp->p[0].x += amount.x;
	sp->p[0].y += amount.y;
	sp->p[1].x += amount.x;
	sp->p[1].y += amount.y;
}


static void AdjustSections( coOrd p1 )
/* adjust end point to lower the costs of this joint to average
 */
{
	double contrib;
	section_p sp;
	int inx;

	contrib = 0.0;
	for ( inx=1; inx<maxCostsInx; inx++ ) {
		sp = &section(inx);
		contrib += sp->contrib;
		sp->pp.x = -(p1.x*contrib/(contribL+contribR));
		sp->pp.y = -(p1.y*contrib/(contribL+contribR));
		AdjustSection( sp, sp->pp );
	}
	contrib = 0.0;
	for ( inx=section_da.cnt-1; inx>=maxCostsInx; inx-- ) {
		sp = &section(inx);
		contrib += sp->contrib;
		sp->pp.x = p1.x*contrib/(contribL+contribR);
		sp->pp.y = p1.y*contrib/(contribL+contribR);
		AdjustSection( sp, sp->pp );
	}
}


static void DumpSections( void )
{
	section_p sp;
	int inx;
	DIST_T dist;
	for (inx = 1; inx<section_da.cnt; inx++) {
		sp = &section(inx);
		dist = FindDistance( sp[0].p[0], sp[-1].p[1] );
		printf( "%2d[%d] X%0.3f Y%0.3f A%0.3f T%0.3f C%0.3f x%0.3f y%0.3f [%0.3f %0.3f] [%0.3f %0.3f] dd%0.3f da%0.3f\n",
			inx, GetTrkIndex(sp->trk), sp->costs[0].X, sp->costs[0].Y, sp->costs[0].A, sp->costs[0].T,
			sp->contrib, sp->pp.x, sp->pp.y,
			sp[0].p[0].x, sp[0].p[0].y, sp[0].p[1].x, sp[0].p[1].y,
			dist,
			(dist>0.001)?NormalizeAngle( FindAngle( sp[0].p[0], sp[-1].p[1] ) - sp[0].a[0] ):0.0 );
	}
	printf("== X%0.3f Y%0.3f A%0.3f T%0.3f\n", sumCosts.X, sumCosts.Y, sumCosts.A, sumCosts.T );
}


/*
 * Controller
 */
static int iterCnt = 5;
static int MinimizeCosts( void )
{
	int inx;
	int rc = 0;
	coOrd p1;
	if (section_da.cnt <= 0)
		return FALSE;
	for (inx=0; inx<iterCnt; inx++) {
		rc = CheckConnections();
		ComputeCosts();
		if (maxCostsInx<0) 
			return TRUE;
		ComputeContribs( &p1 );
		if (contribR+contribL <= 0.001)
			return rc;
		if (maxCosts.T*1.1 < sumCosts.T/(contribR+contribL) && rc)
			/* our work is done */
			return rc;
		AdjustSections( p1 );
		if (debugPull)
			DumpSections();
	}
	return rc;
}


/*
 * Doit
 */
static void MoveSectionTracks( void )
{
	int inx, cnt;
	section_p sp;
	coOrd amount, oldPos;
	cnt = 0;
	for (inx=1; inx<section_da.cnt-1; inx++) {
		sp = &section(inx);
		oldPos = GetTrkEndPos( sp->trk, sp->ep[0] );
		amount.x = sp->p[0].x-oldPos.x;
		amount.y = sp->p[0].y-oldPos.y;
if (debugPull) {
printf("%2d: X%0.3f Y%0.3f\n", inx, amount.x, amount.y );
}
		if (fabs(amount.x)>0.001 || fabs(amount.y)>0.001) {
			UndrawNewTrack( sp->trk );
			UndoModify( sp->trk );
			MoveTrack( sp->trk, amount );
			DrawNewTrack( sp->trk );
			cnt++;
		}
	}
	InfoMessage( _("%d tracks moved"), cnt );
}


static void PullTracks(
		track_p trk1,
		EPINX_T ep1,
		track_p trk2,
		EPINX_T ep2 )
{
	ending_e e1, e2;
	DIST_T d;
	ANGLE_T a;
	coOrd p1, p2;
	ANGLE_T a1, a2;
	coOrd p;
	int cnt1, cnt2;
	int rc;

	if (QueryTrack(trk1,Q_CAN_ADD_ENDPOINTS)) {
		ConnectTurntableTracks(trk1, ep1, trk2, ep2 );
		return;
	}

	if (ep1<0 || ep2<0) return;

	if (ConnectAbuttingTracks( trk1, ep1, trk2, ep2 ))
		return;

	if (ConnectAdjustableTracks( trk1, ep1, trk2, ep2 ))
		return;



	p1 = GetTrkEndPos( trk1, ep1 );
	p2 = GetTrkEndPos( trk2, ep2 );
	a1 = GetTrkEndAngle( trk1, ep1 );
	a2 = GetTrkEndAngle( trk2, ep2 );
	d = FindDistance( p1, p2 );
	a = NormalizeAngle( a1 - a2 + 180 + maxA/2.0 );
	if ( d > maxD || a > maxA ) {
		ErrorMessage( MSG_TOO_FAR_APART_DIVERGE );
		return;
	}
	UndoStart( _("Pull Tracks"), "PullTracks(T%d[%d] T%d[%d] D%0.3f A%0.3F )", GetTrkIndex(trk1), ep1, GetTrkIndex(trk2), ep2, d, a );
	
	DYNARR_RESET( section_t, section_da );
	e1 = e2 = GetConnectedTracks( trk1, ep1, trk2, ep2 );
	cnt1 = section_da.cnt;
	if ( e1 != loopEnd ) {
		e2 = GetConnectedTracks( trk2, ep2, trk1, ep1 );
	} 
	cnt2 = section_da.cnt - cnt1;
	if ( e1 == freeEnd && e2 == freeEnd ) {
		p.x = (p1.x+p2.x)/2.0;
		p.y = (p1.y+p2.y)/2.0;
		a = NormalizeAngle( (a1-(a2+180.0)) );
		if ( a < 180.0 )
			a = NormalizeAngle(a1 + a/2.0);
		else 
			a = NormalizeAngle(a1 - (360-a)/2.0);
		MoveConnectedTracks( trk1, ep1, p, a );
		MoveConnectedTracks( trk2, ep2, p, a+180.0 );
	} else if ( e1 == freeEnd ) {
		MoveConnectedTracks( trk1, ep1, p2, a2+180.0 );
	} else if ( e2 == freeEnd ) {
		MoveConnectedTracks( trk2, ep2, p1, a1+180.0 );
	} else {
		if ( e1 == loopEnd ) {
			if (section_da.cnt <= 3) {
				NoticeMessage( MSG_PULL_FEW_SECTIONS, _("Ok"), NULL );
				return;
			}
			cnt1 = section_da.cnt/2;
			ReverseSectionList( cnt1+1, (int)section_da.cnt );
			ReverseSectionList( 0, cnt1+1 );
			DYNARR_APPEND( section_t, section_da, 10 );
			section(section_da.cnt-1) = section(0);
		} else {
			ReverseSectionList( 0, cnt1 );
		}
		if ((rc=MinimizeCosts())==0) {
			MoveSectionTracks();
		} else {
			if (rc == DIST_FAULT) {
				NoticeMessage( MSG_PULL_ERROR_1, _("Ok"), NULL );
			} else if (rc == ANGLE_FAULT) {
				NoticeMessage( MSG_PULL_ERROR_2, _("Ok"), NULL );
			} else {
				NoticeMessage( MSG_PULL_ERROR_3, _("Ok"), NULL );
			}
			return;
		}
	}
	UndoModify( trk1 );
	UndoModify( trk2 );
	DrawEndPt( &mainD, trk1, ep1, wDrawColorWhite );
	DrawEndPt( &mainD, trk2, ep2, wDrawColorWhite );
	ConnectTracks( trk1, ep1, trk2, ep2 );
	DrawEndPt( &mainD, trk1, ep1, wDrawColorBlack );
	DrawEndPt( &mainD, trk2, ep2, wDrawColorBlack );
}



/*
 * Tighten tracks
 */

static void TightenTracks(
		track_p trk,
		EPINX_T ep )
{
	track_p trk1;
	EPINX_T ep1, ep2;
	coOrd p0, p1;
	ANGLE_T a0, a1;
	int cnt;
	UndoStart(_("Tighten Tracks"), "TightenTracks(T%d[%d])", GetTrkIndex(trk), ep );
	while ( (ep2=GetNextTrk(trk,ep,&trk1,&ep1,0)) >= 0 && trk1 != NULL ) {
		trk = trk1;
		ep = ep1;
	}
	trk1 = GetTrkEndTrk( trk, ep );
	if (trk1 == NULL)
		return;
	ep1 = GetEndPtConnectedToMe( trk1, trk );
	cnt = 0;
	while(1) {
		p0 = GetTrkEndPos( trk, ep );
		a0 = NormalizeAngle( GetTrkEndAngle( trk, ep ) + 180.0 );
		p1 = GetTrkEndPos( trk1, ep1 );
		a1 = GetTrkEndAngle( trk1, ep1 );
		p1.x = p0.x - p1.x;
		p1.y = p0.y - p1.y;
		a1 = NormalizeAngle( a0-a1 );
if (debugPull) {
printf("T%d [%0.3f %0.3f %0.3f]\n", GetTrkIndex(trk1), p1.x, p1.y, a1 );
}
		if ( FindDistance( zero, p1 ) > 0.001 || ( a1 > 0.05 && a1 < 365.95 ) ) {
			UndrawNewTrack( trk1 );
			UndoModify( trk1 );
			MoveTrack( trk1, p1 );
			RotateTrack( trk1, p1, a1 );
			DrawNewTrack( trk1 );
			cnt++;
		}
		trk = trk1;
		ep = GetNextTrk( trk, ep1, &trk1, &ep1, 0 );
		if (trk1 == NULL)
			break;
		if (ep<0)
			AbortProg( "tightenTracks: can't happen" );
	}
	InfoMessage( _("%d tracks moved"), cnt );
}


static STATUS_T CmdPull(
		wAction_t action,
		coOrd pos )
{

	static track_p trk1;
	static EPINX_T ep1;
	track_p trk2;
	EPINX_T ep2;
	static BOOL_T turntable;

	int countTracksR0 = 0, countTracksR1 = 0, possibleEndPoints = 0;
	BOOL_T found = FALSE;
	ANGLE_T a;
	DIST_T d;

	switch (action) {

	case C_START:
		if (selectedTrackCount==0)
			InfoMessage( _("Select first End-Point to connect") );
		else
			InfoMessage( _("Select first End-Point to connect, or Right-Click for connecting selected Tracks") );
		trk1 = NULL;
		turntable = FALSE;
		return C_CONTINUE;

	case C_LCLICK:
		if ( (MyGetKeyState() & WKEY_SHIFT) == 0 ) {
			if (trk1 == NULL) {
				if ((trk1 = OnTrack( &pos, TRUE, FALSE )) != NULL) {
					if ((ep1 = PickUnconnectedEndPoint( pos, trk1 )) < 0) {
						if (QueryTrack(trk1, Q_CAN_ADD_ENDPOINTS)) {
							turntable = TRUE;
							ep1 = -1;
						} else trk1 = NULL;
					} else {
						InfoMessage( _("Select second End-Point to connect") );
					}

				}
			} else {
				if ((trk2 = OnTrack( &pos, TRUE, FALSE )) != NULL) {
					if ((ep2 = PickUnconnectedEndPoint( pos, trk2 )) >= 0 ) {
						PullTracks( trk1, ep1, trk2, ep2 );
						trk1 = NULL;
						inError = TRUE;
						return C_TERMINATE;
					}
					if (!turntable && QueryTrack(trk2, Q_CAN_ADD_ENDPOINTS)) {
						ep2 = -1;
						turntable = TRUE;
						PullTracks( trk2, ep2, trk1, ep1);
						trk1 = NULL;
						inError = TRUE;
						turntable = FALSE;
						return C_TERMINATE;
					}
				}
			}
		} else {
			trk1 = OnTrack( &pos, TRUE, FALSE );
			if (trk1 == NULL)
				return C_CONTINUE;
			ep1 = PickUnconnectedEndPoint( pos, trk1 );
			if ( ep1 < 0 )
				return C_CONTINUE;
			TightenTracks( trk1, ep1 );
			trk1 = NULL;
			inError = TRUE;
			return C_TERMINATE;
		}
		return C_CONTINUE;

	case C_RCLICK:
		if (selectedTrackCount==0) {
			ErrorMessage(_("Connect Multiple Tracks - Select multiple tracks to join first"));
			return C_CONTINUE;
		}
		if (NoticeMessage(_("Try to Connect all Selected Tracks?"), _("Yes"), _("No"))<=0) return C_CONTINUE;
		trk1 = NULL;
		trk2 = NULL;
		UndoStart( _("ReConnect"),"Try to reconnect all selected tracks");
		for (int i=0;i<2;i++) {  // Try twice - in case later joins help earlier ones and to try close ones first
			while ( TrackIterate( &trk1 ) ) {
				found = FALSE;
				if ( GetTrkSelected( trk1 ) ) {
					for (ep1=0; ep1<GetTrkEndPtCnt(trk1); ep1++) {
						if (!GetTrkEndTrk( trk1, ep1 )) {
							trk2 = NULL;
							while (!found && TrackIterate(&trk2) ) {
								if (trk1 == trk2) continue;
								for (ep2=0; ep2<GetTrkEndPtCnt(trk2); ep2++) {
									if (GetTrkEndTrk( trk2, ep2 )) continue;
									d = FindDistance(GetTrkEndPos(trk1,ep1),GetTrkEndPos(trk2,ep2));
									a = NormalizeAngle( 180+GetTrkEndAngle( trk1, ep1 ) - GetTrkEndAngle( trk2, ep2 )+(connectAngle/2.0));
									// Take two passes. In round one favor closer connections. In round two try anything.
									if ( (i==0 && (d < connectDistance) && (a < connectAngle)) ||
											(i>0 && (d<3.0 && a<7.5))) {    // Match PullTracks criteria in round 2
										PullTracks(trk1,ep1,trk2,ep2);
										if (GetTrkEndTrk( trk2, ep2 )) {
											found = TRUE;
											if (i==0)
												countTracksR0++;
											else
												countTracksR1++;
											break;               //Stop looking
										} else if (i==1) possibleEndPoints++;
									}
								}
							}
							if (found) break;  //Next EndPoint
						}
					}
				}
			}
		}
		UndoEnd();
		NoticeMessage(_("Round 1 %d and Round 2 %d tracks connected, %d close pairs of end Points were not connected"), _("OK"), NULL, countTracksR0, countTracksR1, possibleEndPoints);
		return C_TERMINATE;

	case C_REDRAW:
		return C_CONTINUE;

	case C_CANCEL:
		return C_TERMINATE;

	case C_OK:
		return C_TERMINATE;

	case C_CONFIRM:
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}



#include "bitmaps/pull.xpm"

void InitCmdPull( wMenu_p menu )
{
	AddMenuButton( menu, CmdPull, "cmdConnect", _("Connect Two Tracks"), wIconCreatePixMap(pull_xpm), LEVEL0_50, IC_STICKY|IC_LCLICK|IC_POPUP2|IC_RCLICK, ACCL_CONNECT, NULL );
}
