/** \file cturntbl.c
 * TURNTABLE
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
#include <string.h>

#include "cstraigh.h"
#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

static TRKTYP_T T_TURNTABLE = -1;


struct extraData {
		coOrd pos;
		DIST_T radius;
		EPINX_T currEp;
		BOOL_T reverse;
		};

static DIST_T turntableDiameter = 1.0;

EXPORT ANGLE_T turntableAngle = 0.0;

static paramFloatRange_t r1_100 = { 1.0, 100.0, 100 };
static paramData_t turntablePLs[] = {
#define turntableDiameterPD		(turntablePLs[0])
	{	PD_FLOAT, &turntableDiameter, "diameter", PDO_DIM|PDO_NOPREF, &r1_100, N_("Diameter") } };
static paramGroup_t turntablePG = { "turntable", 0, turntablePLs, sizeof turntablePLs/sizeof turntablePLs[0] };


static BOOL_T ValidateTurntablePosition(
		track_p trk )
{
	struct extraData * xx = GetTrkExtraData(trk);
	EPINX_T ep, epCnt = GetTrkEndPtCnt(trk);
	
	if ( epCnt <= 0 )
		return FALSE;
	ep = xx->currEp;
	do {
		if ( GetTrkEndTrk(trk,ep) ) {
			xx->currEp = ep;
			return TRUE;
		}
		ep++;
		if ( ep >= epCnt )
			ep = 0;
	} while ( ep != xx->currEp );
	return FALSE;
}


static void ComputeTurntableBoundingBox( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	coOrd hi, lo;
	hi.x = xx->pos.x+xx->radius;
	lo.x = xx->pos.x-xx->radius;
	hi.y = xx->pos.y+xx->radius;
	lo.y = xx->pos.y-xx->radius;
	SetBoundingBox( trk, hi, lo );
}

static track_p NewTurntable( coOrd p, DIST_T r )
{
	track_p t;
	struct extraData *xx;
	t = NewTrack( 0, T_TURNTABLE, 0, sizeof *xx );
	xx = GetTrkExtraData(t);
	xx->pos = p;
	xx->radius = r;
	xx->currEp = 0;
	xx->reverse = 0;
	ComputeTurntableBoundingBox( t );
	return t;
}

#ifdef LATER
-static void PruneTurntable( track_p trk )
-{
-	 EPINX_T inx0;
-	 EPINX_T inx1;
-	 for (inx0=inx1=0; inx0<trk->endCnt; inx0++) {
-		if (GetTrkEndTrk(trk,inx0) == NULL) {
-			continue;
-		} else {
-			if (inx0 != inx1) {
-				trk->endPt[inx1] = GetTrkEndTrk(trk,inx0);
-			}
-			inx1++;
-		}
-	 }
-	 trk->endPt = Realloc( trk->endPt, inx1*sizeof trk->endPt[0] );
-	 trk->endCnt = inx1;
-}
#endif

static ANGLE_T ConstrainTurntableAngle( track_p trk, coOrd pos )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a, al, ah, aa, aaa;
	EPINX_T inx, cnt;

	a = FindAngle( xx->pos, pos );
	cnt = GetTrkEndPtCnt(trk);
	if ( cnt == 0 || turntableAngle == 0.0 )
		return a;
	ah = 360.0;
	al = 360.0;
	for ( inx = 0; inx<cnt; inx++ ) {
		if (GetTrkEndTrk(trk,inx) == NULL)
			continue;
		aa = NormalizeAngle( GetTrkEndAngle(trk,inx) - a );
		if (aa < al)
			al = aa;
		aa = 360 - aa;
		if (aa < ah)
			ah = aa;
	}
	if (al+ah>361)
		return a;
	if ( (al+ah) < turntableAngle*2.0 ) {
		ErrorMessage( MSG_NO_ROOM_BTW_TRKS );
		aaa = -1;
	} else if ( al <= turntableAngle)
		aaa = NormalizeAngle( a - ( turntableAngle - al ) );
	else if ( ah <= turntableAngle)
		aaa = NormalizeAngle( a + ( turntableAngle - ah ) );
	else
		aaa = a;
#ifdef VERBOSE
	Lprintf( "CTA( %0.3f ) [ %0.3f .. %0.3f ] = %0.3f\n", a, ah, al, aaa );
#endif
	return aaa;
}

static EPINX_T NewTurntableEndPt( track_p trk, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
	EPINX_T ep = GetTrkEndPtCnt(trk);
	coOrd pos;
	SetTrkEndPtCnt( trk, ep+1 );
	PointOnCircle( &pos, xx->pos, xx->radius, angle );
	SetTrkEndPoint( trk, ep, pos, angle );
	return ep;
}

static void TurntableGetCenter( track_p trk, coOrd * center, DIST_T * radius)
{
	struct extraData *xx = GetTrkExtraData(trk);
	*center = xx->pos;
	*radius = xx->radius;
}

static void DrawTurntable( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);
	coOrd p0, p1;
	EPINX_T ep;
	long widthOptions = DTS_TIES;

	if ( !ValidateTurntablePosition(t) ) {
		p0.y = p1.y = xx->pos.y;
		p0.x = xx->pos.x-xx->radius;
		p1.x = xx->pos.x+xx->radius;
	} else {
		p0 = GetTrkEndPos( t, xx->currEp );
		Translate( &p1, xx->pos, GetTrkEndAngle(t,xx->currEp)+180.0, xx->radius );
	}
	if (color == wDrawColorBlack)
		color = normalColor;
	DrawArc( d, xx->pos, xx->radius, 0.0, 360.0, 0, 0, color );
	if ( programMode != MODE_DESIGN )
		return;
	if ( (d->options&DC_QUICK) == 0 ) {
		DrawStraightTrack( d, p0, p1, FindAngle(p0,p1), t, GetTrkGauge(t), color, widthOptions );
		for ( ep=0; ep<GetTrkEndPtCnt(t); ep++ ) {
			if (GetTrkEndTrk(t,ep) != NULL )
				DrawEndPt( d, t, ep, color );
		}
	}
	if ( ((d->funcs->options&wDrawOptTemp)==0) &&
		 (labelWhen == 2 || (labelWhen == 1 && (d->options&DC_PRINT))) &&
		 labelScale >= d->scale ) {
		LabelLengths( d, t, color );
	}
}

static DIST_T DistanceTurntable( track_p trk, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T d;
	ANGLE_T a;
	coOrd pos0, pos1;

	d = FindDistance( xx->pos, *p ) - xx->radius;
	if (d < 0.0)
		d = 0.0;
	if ( programMode == MODE_DESIGN ) {
		a = FindAngle( xx->pos, *p );
		Translate( p, xx->pos, a, d+xx->radius );
	} else {
		if ( !ValidateTurntablePosition(trk) )
			return 100000.0;
		pos0 = GetTrkEndPos(trk,xx->currEp);
		Translate( &pos1, xx->pos, GetTrkEndAngle(trk,xx->currEp)+180.0, xx->radius );
		LineDistance( p, pos0, pos1 );
	}
	return d;
}

static struct {
		coOrd orig;
		DIST_T diameter;
		long epCnt;
		unsigned int layerNumber;
		} trntblData;
typedef enum { OR, RA, EC, LY } trntblDesc_e;
static descData_t trntblDesc[] = {
/*OR*/	{ DESC_POS, N_("Origin: X"), &trntblData.orig },
/*RA*/	{ DESC_DIM, N_("Diameter"), &trntblData.diameter },
/*EC*/	{ DESC_LONG, N_("# EndPt"), &trntblData.epCnt },
/*LY*/	{ DESC_LAYER, N_("Layer"), &trntblData.layerNumber },
		{ DESC_NULL } };


static void UpdateTurntable( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	struct extraData *xx = GetTrkExtraData(trk);

	if ( inx == -1 )
		return;
	UndrawNewTrack( trk );
	switch ( inx ) {
	case OR:
		xx->pos = trntblData.orig;
		break;
	case RA:
		if ( trntblData.diameter > 2.0 )
			xx->radius = trntblData.diameter/2.0;
		break;
	case LY:
		SetTrkLayer( trk, trntblData.layerNumber );
		break;
	default:
		break;
	}
	ComputeTurntableBoundingBox( trk );
	DrawNewTrack( trk );
}


static void DescribeTurntable( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	sprintf( str, _("Turntable(%d): Layer=%d Center=[%s %s] Diameter=%s #EP=%d"),
				GetTrkIndex(trk), GetTrkLayer(trk)+1,
				FormatDistance(xx->pos.x), FormatDistance(xx->pos.y),
				FormatDistance(xx->radius * 2.0), GetTrkEndPtCnt(trk) );

	trntblData.orig = xx->pos;
	trntblData.diameter = xx->radius*2.0;
	trntblData.epCnt = GetTrkEndPtCnt(trk);
	trntblData.layerNumber = GetTrkLayer(trk);

	trntblDesc[OR].mode =
	trntblDesc[RA].mode =
		trntblData.epCnt>0?DESC_RO:0;
	trntblDesc[EC].mode = DESC_RO;
	trntblDesc[LY].mode = DESC_NOREDRAW;
	DoDescribe( _("Turntable"), trk, trntblDesc, UpdateTurntable );
}

static void DeleteTurntable( track_p t )
{
}

static BOOL_T WriteTurntable( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	EPINX_T ep;
	BOOL_T rc = TRUE;
	rc &= fprintf(f, "TURNTABLE %d %d 0 0 0 %s %d %0.6f %0.6f 0 %0.6f %d\n",
		GetTrkIndex(t), GetTrkLayer(t), GetTrkScaleName(t), GetTrkVisible(t),
				xx->pos.x, xx->pos.y, xx->radius, xx->currEp )>0;
	for (ep=0; ep<GetTrkEndPtCnt(t); ep++)
		rc &= WriteEndPt( f, t, ep );
	rc &= fprintf(f, "\tEND\n")>0;
	return rc;
}

static void ReadTurntable( char * line )
{
	track_p trk;
	struct extraData *xx;
	TRKINX_T index;
	BOOL_T visible;
	DIST_T r;
	coOrd p;
	DIST_T elev;
	char scale[10];
	wIndex_t layer;
	int currEp;

	if ( !GetArgs( line+10,
				paramVersion<3?"dXsdpYfX":
				paramVersion<9?"dL000sdpYfX":
				paramVersion<10?"dL000sdpffX":
				"dL000sdpffd",
		&index, &layer, scale, &visible, &p, &elev, &r, &currEp ))
		return;
	trk = NewTrack( index, T_TURNTABLE, 0, sizeof *xx );
	ReadSegs();
	SetEndPts( trk, 0 );
	xx = GetTrkExtraData(trk);
	SetTrkVisible(trk, visible);
	SetTrkScale(trk, LookupScale( scale ) );
	SetTrkLayer(trk, layer);
	xx->pos = p;
	xx->radius = r;
	xx->currEp = currEp;
	xx->reverse = 0;
	ComputeTurntableBoundingBox( trk );
}

static void MoveTurntable( track_p trk, coOrd orig )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->pos.x += orig.x;
	xx->pos.y += orig.y;
	ComputeTurntableBoundingBox( trk );
}

static void RotateTurntable( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
	Rotate( &xx->pos, orig, angle );
	ComputeTurntableBoundingBox( trk );
}

static void RescaleTurntable( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->pos.x *= ratio;
	xx->pos.y *= ratio;
}

static ANGLE_T GetAngleTurntable( track_p trk, coOrd pos, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	if ( programMode == MODE_DESIGN ) {
		return FindAngle( xx->pos, pos );
	} else {
		if ( !ValidateTurntablePosition( trk ) )
			return 90.0;
		else
			return GetTrkEndAngle( trk, xx->currEp );
	}
}


static BOOL_T SplitTurntable( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T *ep0, EPINX_T *ep1 )
{
	if (leftover)
		*leftover = NULL;
	ErrorMessage( MSG_CANT_SPLIT_TRK, "Turntable" );
	return FALSE;
}


static BOOL_T FindTurntableEndPt(
		track_p trk,
		ANGLE_T *angleR,
		EPINX_T *epR,
		BOOL_T *reverseR )
{
	EPINX_T ep, ep0, epCnt=GetTrkEndPtCnt(trk);
	ANGLE_T angle=*angleR, angle0, angle1;
	for (ep=0,ep0=-1,epCnt=GetTrkEndPtCnt(trk),angle0=370.0; ep<epCnt; ep++) {
		if ( (GetTrkEndTrk(trk,ep)) == NULL )
			continue;
		angle1 = GetTrkEndAngle(trk,ep);
		angle1 = NormalizeAngle(angle1-angle);
		if ( angle1 > 180.0 )
			 angle1 = 360.0-angle1;
		if ( angle1 < angle0 ) {
			*epR = ep;
			*reverseR = FALSE;
			angle0 = angle1;
		}
#ifdef LATER
		if ( angle1 > 90.0 ) {
			angle1 = 180.0-angle1;
			if ( angle1 < angle0 ) {
				*epR = ep;
				*reverseR = TRUE;
				angle0 = angle1;
			}
		}
#endif
	}
	if ( angle0 < 360.0 ) {
		*angleR = angle0;
		return TRUE;
	} else {
		return FALSE;
	}
}



static BOOL_T CheckTraverseTurntable(
		track_p trk,
		coOrd pos )
{
	struct extraData * xx = GetTrkExtraData(trk);
	ANGLE_T angle;

	if ( !ValidateTurntablePosition( trk ) )
		return FALSE;
	angle = FindAngle( xx->pos, pos ) - GetTrkEndAngle( trk, xx->currEp )+connectAngle/2.0;
	if ( angle <= connectAngle	|| 
		 ( angle >= 180.0 && angle <= 180+connectAngle ) )
		return TRUE;
	return FALSE;
}


static BOOL_T TraverseTurntable(
		traverseTrack_p trvTrk,
		DIST_T * distR )
{
	track_p trk = trvTrk->trk;
	struct extraData * xx = GetTrkExtraData(trk);
	coOrd pos0;
	DIST_T dist, dist1;
	ANGLE_T angle, angle1;
	EPINX_T ep;
	BOOL_T reverse;

	if ( !ValidateTurntablePosition( trk ) )
		return FALSE;
	dist = FindDistance( xx->pos, trvTrk->pos );
	pos0 = GetTrkEndPos( trk, xx->currEp );
	angle = FindAngle( pos0, xx->pos );
	if ( NormalizeAngle( angle-trvTrk->angle+90 ) < 180 ) {
		angle1 = angle;
	} else {
		angle1 = NormalizeAngle( angle+180.0 );
	}
	if ( dist > xx->radius*0.9 ) {
		angle = NormalizeAngle( angle-trvTrk->angle );
		if ( ( angle < 90.0 && angle > connectAngle ) ||
			 ( angle > 270.0 && angle < 360.0-connectAngle ) )
			return FALSE;
	}
	trvTrk->angle = angle1;
	angle = FindAngle( trvTrk->pos, xx->pos );
	if ( NormalizeAngle( angle-angle1+90.0 ) < 180 ) {
		if ( dist > *distR ) {
			Translate( &trvTrk->pos, xx->pos, angle1+180.0, dist-*distR );
			*distR = 0;
			return TRUE;
		} else {
			*distR -= dist;
			dist = 0.0;
		}
	}
	dist1 = xx->radius-dist;
	if ( dist1 > *distR ) {
		Translate( &trvTrk->pos, xx->pos, angle1, dist+*distR );
		*distR = 0.0;
		return TRUE;
	}
	Translate( &trvTrk->pos, xx->pos, angle1, xx->radius );
	*distR -= dist1;
	if ( FindTurntableEndPt( trk, &angle1, &ep, &reverse ) && angle1 < connectAngle ) {
		trk = GetTrkEndTrk(trk,ep);
	} else {
		trk = NULL;
	}
	trvTrk->trk = trk;
	return TRUE;
}


static BOOL_T EnumerateTurntable( track_p trk )
{
	struct extraData *xx;
	static dynArr_t turntables_da;
#define turntables(N) DYNARR_N( FLOAT_T, turntables_da, N )
	int inx;
	char tmp[40];
	if ( trk != NULL ) {
		xx = GetTrkExtraData(trk);
		DYNARR_APPEND( FLOAT_T, turntables_da, 10 );
		turntables(turntables_da.cnt-1) = xx->radius*2.0;
		sprintf( tmp, "Turntable, diameter %s", FormatDistance(turntables(turntables_da.cnt-1)) );
		inx = strlen( tmp );
		if ( inx > (int)enumerateMaxDescLen )
			enumerateMaxDescLen = inx;
	} else {
		for (inx=0; inx<turntables_da.cnt; inx++) {
			sprintf( tmp, "Turntable, diameter %s", FormatDistance(turntables(inx)) );
			EnumerateList( 1, 0.0, tmp );
		}
		DYNARR_RESET( FLOAT_T, turntables_da );
	}
	return TRUE;   
}


static STATUS_T ModifyTurntable( track_p trk, wAction_t action, coOrd pos )
{
	static coOrd ttCenter;
	static DIST_T ttRadius;
	static ANGLE_T angle;
	static BOOL_T valid;

	DIST_T r;
	EPINX_T ep;
	track_p trk1;

	switch ( action ) {
	case C_DOWN:
		TurntableGetCenter( trk, &ttCenter, &ttRadius );
		tempSegs(0).type = SEG_STRTRK;
		tempSegs(0).width = 0;
		InfoMessage( _("Drag to create stall track") );

	case C_MOVE:
		valid = FALSE;
		if ( (angle = ConstrainTurntableAngle( trk, pos )) < 0.0) {
			;
		} else if ((r=FindDistance( ttCenter, pos )) < ttRadius) {
			ErrorMessage( MSG_POINT_INSIDE_TURNTABLE );
		} else if ( (r-ttRadius) <= minLength ) {
			if (action == C_MOVE)
				ErrorMessage( MSG_TRK_TOO_SHORT, "Stall ", PutDim(fabs(minLength-(r-ttRadius))) );
		} else {
			Translate( &tempSegs(0).u.l.pos[0], ttCenter, angle, ttRadius );
			Translate( &tempSegs(0).u.l.pos[1], ttCenter, angle, r );
			if (action == C_MOVE)
				InfoMessage( _("Straight Track: Length=%s Angle=%0.3f"),
						FormatDistance( r-ttRadius ), PutAngle( angle ) );
			tempSegs_da.cnt = 1;
			valid = TRUE;
		}
		return C_CONTINUE;

	case C_UP:
		if (!valid)
			return C_TERMINATE;
		ep = NewTurntableEndPt( trk, angle );
		trk1 = NewStraightTrack( tempSegs(0).u.l.pos[0], tempSegs(0).u.l.pos[1] );
		CopyAttributes( trk, trk1 );
		ConnectTracks( trk, ep, trk1, 0 );
		DrawNewTrack( trk1 );
		return C_TERMINATE;

	default:
		;
	}
	return C_ERROR;
}

EXPORT BOOL_T ConnectTurntableTracks(
		track_p trk1,
		EPINX_T ep1,
		track_p trk2,
		EPINX_T ep2 ) {
	coOrd center, pos;
	DIST_T radius;
	ANGLE_T angle = ConstrainTurntableAngle( trk1, GetTrkEndPos(trk2,ep2) );
	pos = GetTrkEndPos(trk2,ep2);
	if (GetTrkEndAngle(trk2,ep2) - angle < connectAngle) {
		TurntableGetCenter( trk1, &center, &radius );
		if (FindDistance(center,pos)-radius < connectDistance) {
			EPINX_T ep = NewTurntableEndPt(trk1,angle);
			ConnectTracks( trk1, ep, trk2, ep2 );
			return TRUE;
		}
	}
	return FALSE;
}


static BOOL_T GetParamsTurntable( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	coOrd center;
	DIST_T radius;

	if (inx == PARAMS_1ST_JOIN) {
		ErrorMessage( MSG_JOIN_TURNTABLE );
		return FALSE;
	}
	params->type = curveTypeStraight;
	params->ep = -1;
	params->angle = ConstrainTurntableAngle( trk, pos );
	if (params->angle < 0.0)
		 return FALSE;
	TurntableGetCenter( trk, &center, &radius );
	PointOnCircle( &params->lineOrig, center, radius, params->angle );
	params->lineEnd = params->lineOrig;
	params->len = 0.0;
	params->arcR = 0.0;
	params->ttcenter = center;	//Turntable
	params->ttradius = radius;	//Turntable
	return TRUE;
}


static BOOL_T MoveEndPtTurntable( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d0 )
{
	coOrd posCen;
	DIST_T r;
	ANGLE_T angle0;
	DIST_T d;
	track_p trk1;

	TurntableGetCenter( *trk, &posCen, &r );
	angle0 = FindAngle( posCen, pos );
	d = FindDistance( posCen, pos );
	if (d0 > 0.0) {
		d -= d0;
		Translate( &pos, pos, angle0+180, d0 );
	}
	if (small((r-d)/2)) {
		Translate( &pos, posCen, angle0+180, r);   //Make radius equal if close
	} else if (d < r) {
		ErrorMessage( MSG_POINT_INSIDE_TURNTABLE );
		return FALSE;
	}
	//Look for empty slot
	BOOL_T found = FALSE;
	for (*ep=0; *ep<GetTrkEndPtCnt(*trk); *ep=*ep+1) {
		if ( (GetTrkEndTrk(*trk,*ep)) == NULL )
			found = TRUE;
			break;
	}
	if (!found)
		*ep = NewTurntableEndPt(*trk,angle0);
	else {
		struct extraData *xx = GetTrkExtraData(*trk);
		coOrd pos1;
		PointOnCircle( &pos1, xx->pos, xx->radius, angle0 );
		SetTrkEndPoint(*trk, *ep, pos1, angle0);   //Reuse
	}
	if ((d-r) > connectDistance) {
		trk1 = NewStraightTrack( GetTrkEndPos(*trk,*ep), pos );
		CopyAttributes( *trk, trk1 );
		ConnectTracks( *trk, *ep, trk1, 0 );
		*trk = trk1;
		*ep = 1;
		DrawNewTrack( *trk );
	}
	return TRUE;
}


static BOOL_T QueryTurntable( track_p trk, int query )
{
	switch ( query ) {
	case Q_REFRESH_JOIN_PARAMS_ON_MOVE:
	case Q_CANNOT_PLACE_TURNOUT:
	case Q_DONT_DRAW_ENDPOINT:
	case Q_CAN_NEXT_POSITION:
	case Q_ISTRACK:
	case Q_NOT_PLACE_FROGPOINTS:
	case Q_MODIFY_REDRAW_DONT_UNDRAW_TRACK:
	case Q_CAN_ADD_ENDPOINTS:
	case Q_MODIFY_CANT_SPLIT:
	case Q_CAN_EXTEND:
		return TRUE;
	default:
		return FALSE;
	}
}


static void FlipTurntable(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->pos, orig, angle );
	ComputeBoundingBox( trk );
}


static void DrawTurntablePositionIndicator( track_p trk, wDrawColor color )
{
	struct extraData * xx = GetTrkExtraData(trk);
	coOrd pos0, pos1;
	ANGLE_T angle;
	
	if ( !ValidateTurntablePosition(trk) )
		return;
	pos0 = GetTrkEndPos(trk,xx->currEp);
	angle = FindAngle( xx->pos, pos0 );
	PointOnCircle( &pos1, xx->pos, xx->radius, angle+180.0 );
	DrawLine( &mainD, pos0, pos1, 3, color );
}

static void AdvanceTurntablePositionIndicator(
		track_p trk,
		coOrd pos,
		coOrd * posR,
		ANGLE_T * angleR )
{
	struct extraData * xx = GetTrkExtraData(trk);
	EPINX_T ep;
	ANGLE_T angle0, angle1;
	BOOL_T reverse;

	angle1 = FindAngle( xx->pos, pos );
	if ( !FindTurntableEndPt( trk, &angle1, &ep, &reverse ) )
		return;
	DrawTurntablePositionIndicator( trk, wDrawColorWhite );
	angle0 = GetTrkEndAngle(trk,xx->currEp);
	if ( ep == xx->currEp ) {
		Rotate( posR, xx->pos, 180.0 );
		if ( xx->reverse ) {
			angle1 = angle0;
			xx->reverse = FALSE;
		} else {
			angle1 = NormalizeAngle( angle0+180.0 );
			xx->reverse = TRUE;
		}
	} else {
		angle1 = GetTrkEndAngle(trk,ep);
		Rotate( posR, xx->pos, angle1-angle0 );
		xx->reverse = FALSE;
	}
	*angleR = angle1;
	xx->currEp = ep;
	DrawTurntablePositionIndicator( trk, selectedColor );
}


static trackCmd_t turntableCmds = {
		"TURNTABLE",
		DrawTurntable,
		DistanceTurntable,
		DescribeTurntable,
		DeleteTurntable,
		WriteTurntable,
		ReadTurntable,
		MoveTurntable,
		RotateTurntable,
		RescaleTurntable,
		NULL,	/* audit */
		GetAngleTurntable,
		SplitTurntable, /* split */
		TraverseTurntable,
		EnumerateTurntable,
		NULL,	/* redraw */
		NULL,	/* trim */
		NULL,	/* merge */
		ModifyTurntable,
		NULL,	/* getLength */
		GetParamsTurntable,
		MoveEndPtTurntable,
		QueryTurntable,
		NULL,	/* ungroup */
		FlipTurntable,
		DrawTurntablePositionIndicator,
		AdvanceTurntablePositionIndicator,
		CheckTraverseTurntable };


static STATUS_T CmdTurntable( wAction_t action, coOrd pos )
{
	track_p t;
	static coOrd pos0;
	wControl_p controls[2];
	char * labels[1];

	switch (action) {

	case C_START:
		if (turntableDiameterPD.control==NULL)
			ParamCreateControls( &turntablePG, NULL );
		sprintf( message, "turntable-diameter-%s", curScaleName );
		turntableDiameter = ceil(80.0*12.0/curScaleRatio);
		wPrefGetFloat( "misc", message, &turntableDiameter, turntableDiameter );
		ParamLoadControls( &turntablePG );
		ParamGroupRecord( &turntablePG );
		controls[0] = turntableDiameterPD.control;
		controls[1] = NULL;
		labels[0] = N_("Diameter");
		InfoSubstituteControls( controls, labels );
		/*InfoMessage( "Place Turntable");*/
		return C_CONTINUE;

	case C_DOWN:
		SnapPos( &pos );
		if ( turntableDiameter <= 0.0 ) {
			ErrorMessage( MSG_TURNTABLE_DIAM_GTR_0 );
			return C_ERROR;
		}
		controls[0] = turntableDiameterPD.control;
		controls[1] = NULL;
		labels[0] = N_("Diameter");
		InfoSubstituteControls( controls, labels );
		ParamLoadData( &turntablePG );
		pos0 = pos;
		DrawArc( &tempD, pos0, turntableDiameter/2.0, 0.0, 360.0, 0, 0, wDrawColorBlack );
		return C_CONTINUE;

	case C_MOVE:
		DrawArc( &tempD, pos0, turntableDiameter/2.0, 0.0, 360.0, 0, 0, wDrawColorBlack );
		SnapPos( &pos );
		pos0 = pos;
		DrawArc( &tempD, pos0, turntableDiameter/2.0, 0.0, 360.0, 0, 0, wDrawColorBlack );
		return C_CONTINUE;

	case C_UP:
		DrawArc( &tempD, pos0, turntableDiameter/2.0, 0.0, 360.0, 0, 0, wDrawColorBlack );
		SnapPos( &pos );
		UndoStart( _("Create Turntable"), "NewTurntable" );
		t = NewTurntable( pos, turntableDiameter/2.0 );
		UndoEnd();
		DrawNewTrack(t);
		InfoSubstituteControls( NULL, NULL );
		sprintf( message, "turntable-diameter-%s", curScaleName );
		wPrefSetFloat( "misc", message, turntableDiameter );
		return C_TERMINATE;

	case C_REDRAW:
		DrawArc( &tempD, pos0, turntableDiameter/2.0, 0.0, 360.0, 0, 0, wDrawColorBlack );
		return C_CONTINUE;

	case C_CANCEL:
		InfoSubstituteControls( NULL, NULL );
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}


#include "bitmaps/turntbl.xpm"


EXPORT void InitCmdTurntable( wMenu_p menu )
{
	AddMenuButton( menu, CmdTurntable, "cmdTurntable", _("Turntable"), wIconCreatePixMap(turntbl_xpm), LEVEL0_50, IC_STICKY, ACCL_TURNTABLE, NULL );
}


EXPORT void InitTrkTurntable( void )
{
	T_TURNTABLE = InitObject( &turntableCmds );

	ParamRegister( &turntablePG );
}
