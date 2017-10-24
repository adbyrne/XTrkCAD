/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tstraigh.c,v 1.2 2008-01-20 23:29:15 mni77 Exp $
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

#include "track.h"
#include "cstraigh.h"
#include "i18n.h"

/*******************************************************************************
 *
 * STRAIGHT
 *
 */

static TRKTYP_T T_STRAIGHT = -1;


/****************************************
 *
 * UTILITIES
 *
 */


void AdjustStraightEndPt( track_p t, EPINX_T inx, coOrd pos )
{
	if (GetTrkType(t) != T_STRAIGHT) {
		AbortProg( "AdjustLIneEndPt( %d, %d ) not on STRAIGHT %d\n",
				GetTrkIndex(t), inx, GetTrkType(t) );
		return;
	}
	UndoModify( t );
#ifdef VERBOSE
lprintf("adjustStraightEndPt T%d[%d] p=[%0.3f %0.3f]\n",
		GetTrkIndex(t), inx, pos.x, pos.y );
#endif
	SetTrkEndPoint( t, inx, pos, GetTrkEndAngle(t,inx));
	ComputeBoundingBox( t );
	CheckTrackLength( t );
}

/****************************************
 *
 * GENERIC FUNCTIONS
 *
 */

static struct {
		coOrd endPt[2];
		DIST_T elev[2];
		FLOAT_T length;
		ANGLE_T angle;
		FLOAT_T grade;
		descPivot_t pivot;
		LAYER_T layerNumber;
		} strData;
typedef enum { E0, Z0, E1, Z1, LN, AN, GR, PV, LY } strDesc_e;
static descData_t strDesc[] = {
/*E0*/	{ DESC_POS, N_("End Pt 1: X"), &strData.endPt[0] },
/*Z0*/	{ DESC_DIM, N_("Z"), &strData.elev[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X"), &strData.endPt[1] },
/*Z1*/	{ DESC_DIM, N_("Z"), &strData.elev[1] },
/*LN*/	{ DESC_DIM, N_("Length"), &strData.length },
/*AN*/	{ DESC_ANGLE, N_("Angle"), &strData.angle },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &strData.grade },
/*PV*/	{ DESC_PIVOT, N_("Pivot"), &strData.pivot },
/*LY*/	{ DESC_LAYER, N_("Layer"), &strData.layerNumber },
		{ DESC_NULL } };



EXPORT BOOL_T UpdateDescStraight(
		int inx,
		int e0,
		int e1,
		int ln,
		int an,
		descData_p desc,
		long pivot )
{
	coOrd mid;
	if ( inx == e0 || inx == e1 ) {
		*(DIST_T*)desc[ln].valueP = FindDistance( *(coOrd*)desc[e0].valueP, *(coOrd*)desc[e1].valueP );
		*(ANGLE_T*)desc[an].valueP = FindAngle( *(coOrd*)desc[e0].valueP, *(coOrd*)desc[e1].valueP );
		if ( inx == e0 )
			desc[e1].mode |= DESC_CHANGE;
		else
			desc[e0].mode |= DESC_CHANGE;
		desc[ln].mode |= DESC_CHANGE;
		desc[an].mode |= DESC_CHANGE;
	} else if ( inx == ln || inx == an ) {
		if ( inx == ln && *(DIST_T*)desc[ln].valueP <= minLength ) {
			ErrorMessage( MSG_OBJECT_TOO_SHORT );
			*(DIST_T*)desc[ln].valueP = FindDistance( *(coOrd*)desc[e0].valueP, *(coOrd*)desc[e1].valueP );
			desc[ln].mode |= DESC_CHANGE;
			return FALSE;
		}
		switch (pivot) {
		case DESC_PIVOT_FIRST:
			Translate( (coOrd*)desc[e1].valueP, *(coOrd*)desc[e0].valueP, *(ANGLE_T*)desc[an].valueP, *(DIST_T*)desc[ln].valueP );
			desc[e1].mode |= DESC_CHANGE;
			break;
		case DESC_PIVOT_SECOND:
			Translate( (coOrd*)desc[e0].valueP, *(coOrd*)desc[e1].valueP, *(ANGLE_T*)desc[an].valueP+180.0, *(DIST_T*)desc[ln].valueP );
			desc[e0].mode |= DESC_CHANGE;
			break;
		case DESC_PIVOT_MID:
			mid.x = (((coOrd*)desc[e0].valueP)->x+((coOrd*)desc[e1].valueP)->x)/2.0;
			mid.y = (((coOrd*)desc[e0].valueP)->y+((coOrd*)desc[e1].valueP)->y)/2.0;
			Translate( (coOrd*)desc[e0].valueP, mid, *(ANGLE_T*)desc[an].valueP+180.0, *(DIST_T*)desc[ln].valueP/2.0 );
			Translate( (coOrd*)desc[e1].valueP, mid, *(ANGLE_T*)desc[an].valueP, *(DIST_T*)desc[ln].valueP/2.0 );
			desc[e0].mode |= DESC_CHANGE;
			desc[e1].mode |= DESC_CHANGE;
			break;
		default:
			break;
		}
	} else {
		return FALSE;
	}
	return TRUE;
}


static void UpdateStraight( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	EPINX_T ep;
	switch ( inx ) {
	case E0:
	case E1:
	case LN:
	case AN:
		if ( ! UpdateDescStraight( inx, E0, E1, LN, AN, strDesc, strData.pivot ) )
			return;
		break;
	case Z0:
	case Z1:
		ep = (inx==Z0?0:1);
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), strData.elev[ep], NULL );
		ComputeElev( trk, 1-ep, FALSE, &strData.elev[1-ep], NULL );
		if ( strData.length > minLength )
			strData.grade = fabs( (strData.elev[0]-strData.elev[1])/strData.length )*100.0;
		else
			strData.grade = 0.0;
		strDesc[GR].mode |= DESC_CHANGE;
		strDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
		/*return;*/
		break;
#ifdef LATER
		update = UpdateDescStraight( 0, &strDesc[E0], &strDesc[E1], &strDesc[LN], &strDesc[AN], strData.pivot );
		break;
	case E1:
		update = UpdateDescStraight( 1, &strDesc[E0], &strDesc[E1], &strDesc[LN], &strDesc[AN], strData.pivot );
		break;
	case E1:
		strData.length = FindDistance( strData.endPt[0], strData.endPt[1] );
		strData.angle = FindAngle( strData.endPt[0], strData.endPt[1] );
		strDesc[1-inx].mode |= DESC_CHANGE;
		strDesc[LN].mode |= DESC_CHANGE;
		strDesc[AN].mode |= DESC_CHANGE;
		break;
	case LN:
		if ( strData.length < minLength ) {
			ErrorMessage( );
			strData.length = FindDistance( strData.endPt[0], strData.endPt[1] );
			strDesc[LN].mode |= DESC_CHANGE;
			break;
		}
	case AN:
		switch (strData.pivot) {
		case DESC_PIVOT_FIRST:
			Translate( &strData.endPt[1], strData.endPt[0], strData.angle, strData.length );
			strDesc[E1].mode |= DESC_CHANGE;
			break;
		case DESC_PIVOT_SECOND:
			Translate( &strData.endPt[0], strData.endPt[1], strData.angle+180.0, strData.length );
			strDesc[E0].mode |= DESC_CHANGE;
			break;
		case DESC_PIVOT_MID:
			mid.x = (strData.endPt[0].x+strData.endPt[1].x)/2.0;
			mid.y = (strData.endPt[0].y+strData.endPt[1].y)/2.0;
			Translate( &strData.endPt[0], mid, strData.angle+180.0, strData.length/2.0 );
			Translate( &strData.endPt[1], mid, strData.angle, strData.length/2.0 );
			strDesc[E0].mode |= DESC_CHANGE;
			strDesc[E1].mode |= DESC_CHANGE;
			break;
		default:
			break;
		}
		break;
#endif
	case LY:
		SetTrkLayer( trk, strData.layerNumber);
		break;
	default:
		return;
	}
	UndrawNewTrack( trk );
	if ( GetTrkEndTrk(trk,0) == NULL )
		SetTrkEndPoint( trk, 0, strData.endPt[0], NormalizeAngle(strData.angle+180.0) );
	if ( GetTrkEndTrk(trk,1) == NULL )
		SetTrkEndPoint( trk, 1, strData.endPt[1], strData.angle );
	ComputeBoundingBox( trk );
	DrawNewTrack( trk ); 
}

static void DescribeStraight( track_p trk, char * str, CSIZE_T len )
{
	int fix0, fix1;
	sprintf( str, _("Straight Track(%d): Layer=%d Length=%s EP=[%0.3f,%0.3f A%0.3f] [%0.3f,%0.3f A%0.3f]"), GetTrkIndex(trk),
		GetTrkLayer(trk)+1,
		FormatDistance(FindDistance( GetTrkEndPos(trk,0), GetTrkEndPos(trk,1) )),
		GetTrkEndPosXY(trk,0), GetTrkEndAngle(trk,0),
		GetTrkEndPosXY(trk,1), GetTrkEndAngle(trk,1) );
	fix0 = GetTrkEndTrk(trk,0)!=NULL;
	fix1 = GetTrkEndTrk(trk,1)!=NULL;
	strData.endPt[0] = GetTrkEndPos(trk,0);
	strData.endPt[1] = GetTrkEndPos(trk,1);
	ComputeElev( trk, 0, FALSE, &strData.elev[0], NULL );
	ComputeElev( trk, 1, FALSE, &strData.elev[1], NULL );
	strData.length = FindDistance( strData.endPt[0], strData.endPt[1] );
	strData.layerNumber = GetTrkLayer(trk);
	if ( strData.length > minLength )
		strData.grade = fabs( (strData.elev[0]-strData.elev[1])/strData.length )*100.0;
	else
		strData.grade = 0.0;
	strData.angle = FindAngle( strData.endPt[0], strData.endPt[1] );
	strDesc[E0].mode =
	strDesc[E1].mode = (fix0|fix1)?DESC_RO:0;
	strDesc[Z0].mode = (EndPtIsDefinedElev(trk,0)?0:DESC_RO)|DESC_NOREDRAW;
	strDesc[Z1].mode = (EndPtIsDefinedElev(trk,1)?0:DESC_RO)|DESC_NOREDRAW;
	strDesc[GR].mode = DESC_RO;
	strDesc[LN].mode = (fix0&fix1)?DESC_RO:0;
	strDesc[AN].mode = (fix0|fix1)?DESC_RO:0;
	strDesc[PV].mode = (fix0|fix1)?DESC_IGNORE:0;
	strDesc[LY].mode = DESC_NOREDRAW;
	strData.pivot = (fix0&fix1)?DESC_PIVOT_NONE:
				  fix0?DESC_PIVOT_FIRST:
				  fix1?DESC_PIVOT_SECOND:
				  DESC_PIVOT_MID;
	DoDescribe( _("Straight Track"), trk, strDesc, UpdateStraight );
}

static DIST_T DistanceStraight( track_p t, coOrd * p )
{
	return LineDistance( p, GetTrkEndPos(t,0), GetTrkEndPos(t,1) );
}

static void DrawStraight( track_p t, drawCmd_p d, wDrawColor color )
{
	long widthOptions = DTS_LEFT|DTS_RIGHT|DTS_TIES;
	if (GetTrkWidth(t) == 2)
		widthOptions |= DTS_THICK2;
	if (GetTrkWidth(t) == 3)
		widthOptions |= DTS_THICK3;
	DrawStraightTrack( d, GetTrkEndPos(t,0), GetTrkEndPos(t,1),
				GetTrkEndAngle(t,0),
				t, GetTrkGauge(t), color, widthOptions );
	if ( (d->funcs->options & wDrawOptTemp) == 0 && (d->options & DC_QUICK) == 0 ) {
		DrawEndPt( d, t, 0, color );
		DrawEndPt( d, t, 1, color );
	}
}

static void DeleteStraight( track_p t )
{
}

static BOOL_T WriteStraight( track_p t, FILE * f )
{
	BOOL_T rc = TRUE;
	rc &= fprintf(f, "STRAIGHT %d %d %ld 0 0 %s %d\n",
				GetTrkIndex(t), GetTrkLayer(t), (long)GetTrkWidth(t),
				GetTrkScaleName(t), GetTrkVisible(t) )>0;
	rc &= WriteEndPt( f, t, 0 );
	rc &= WriteEndPt( f, t, 1 );
	rc &= fprintf(f, "\tEND\n" )>0;
	return rc;
}

static void ReadStraight( char * line )
{
	track_p trk;
	wIndex_t index;
	BOOL_T visible;
	char scale[10];
	wIndex_t layer;
	long options;

	if ( !GetArgs( line+8, paramVersion<3?"dXZsd":"dLl00sd", &index, &layer, &options, scale, &visible ) )
		return;
	trk = NewTrack( index, T_STRAIGHT, 0, 0 );
	SetTrkScale( trk, LookupScale(scale) );
	SetTrkVisible(trk, visible);
	SetTrkLayer(trk, layer);
	SetTrkWidth( trk, (int)(options&3) );
	ReadSegs();
	SetEndPts( trk, 2 );
	ComputeBoundingBox( trk );
}

static void MoveStraight( track_p trk, coOrd orig )
{
	ComputeBoundingBox( trk );
}

static void RotateStraight( track_p trk, coOrd orig, ANGLE_T angle )
{
	ComputeBoundingBox( trk );
}

static void RescaleStraight( track_p trk, FLOAT_T ratio )
{
}

static int AuditStraight( track_p trk, char * msg )
{
	if (FindDistance( GetTrkEndPos(trk,0), GetTrkEndPos(trk,1) ) < 0.01) {
		sprintf( msg, "T%d: short track\n", GetTrkIndex(trk) );
		return FALSE;
	} else {
		return TRUE;
	}
}


static ANGLE_T GetAngleStraight( track_p trk, coOrd pos, EPINX_T *ep0, EPINX_T *ep1 )
{
	if ( ep0 ) *ep0 = 0;
	if ( ep1 ) *ep1 = 1;
	return GetTrkEndAngle( trk, 0 );
}


static BOOL_T SplitStraight( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	track_p trk1;

	trk1 = NewStraightTrack( GetTrkEndPos(trk,ep), pos );
	AdjustStraightEndPt( trk, ep, pos );
	*leftover = trk1;
	*ep0 = 1;
	*ep1 = 0;
	return TRUE;
}


static BOOL_T TraverseStraight( traverseTrack_p trvTrk, DIST_T * distR )
{
	coOrd pos[2];
	ANGLE_T angle0, angle;
	DIST_T dist;
	track_p trk = trvTrk->trk;
	EPINX_T ep;

	pos[0] = GetTrkEndPos(trk,0);
	pos[1] = GetTrkEndPos(trk,1);
	angle0 = FindAngle( pos[0], pos[1] );
	angle = NormalizeAngle( angle0-trvTrk->angle );
	trvTrk->angle = angle0;
	if ( angle < 270 && angle > 90 ) {
		ep = 0;
		trvTrk->angle = NormalizeAngle( trvTrk->angle + 180.0 );
	} else {
		ep = 1;
	}
	dist = FindDistance( trvTrk->pos, pos[ep] );
	if ( dist > *distR ) {
		Translate( &trvTrk->pos, pos[ep], NormalizeAngle(trvTrk->angle+180.0), dist-*distR ); 
		*distR = 0;
	} else {
		trvTrk->pos = pos[ep];
		*distR -= dist;
		trvTrk->trk = GetTrkEndTrk( trk, ep );
	}
	return TRUE;
}


static BOOL_T EnumerateStraight( track_p trk )
{
	DIST_T d;
	if (trk != NULL) {
		d = FindDistance( GetTrkEndPos( trk, 0 ), GetTrkEndPos( trk, 1 ) );
		ScaleLengthIncrement( GetTrkScale(trk), d );
	}
	return TRUE;
}

static BOOL_T TrimStraight( track_p trk, EPINX_T ep, DIST_T dist )
{
	DIST_T d;
	ANGLE_T a;
	coOrd p1, pos;
	a = NormalizeAngle( GetTrkEndAngle(trk,ep) + 180.0 );
	Translate( &pos, GetTrkEndPos(trk,ep), a, dist );
	p1 = GetTrkEndPos( trk, 1-ep );
	d = FindDistance( pos, p1 );
	if (dist < FindDistance( GetTrkEndPos(trk,0), GetTrkEndPos(trk,1) ) &&
					d > minLength ) {
					UndrawNewTrack( trk );
					AdjustStraightEndPt( trk, ep, pos );
					DrawNewTrack( trk );
	} else
		DeleteTrack( trk, FALSE );
	return TRUE;
}


BOOL_T ExtendStraightToJoin(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	coOrd off;
	ANGLE_T a;
	track_p trk0x, trk1x, trk2;
	EPINX_T ep0x=-1, ep1x=-1;
	coOrd pos0, pos1;
	ANGLE_T a0, a1, aa;

	a0 = GetTrkEndAngle( trk0, ep0 );
	a1 = GetTrkEndAngle( trk1, ep1 );
	a = NormalizeAngle( a0 - a1 + 180.0 + connectAngle/2.0 );
	pos0 = GetTrkEndPos( trk0, (GetTrkType(trk0) == T_STRAIGHT)?1-ep0:ep0 );
	off = pos1 = GetTrkEndPos( trk1, (GetTrkType(trk1) == T_STRAIGHT)?1-ep1:ep1 );
	Rotate( &off, pos0, -a0 );
	off.x -= pos0.x;

	if ( a >= connectAngle || 
		 !IsClose( fabs(off.x) ) ||
		 off.y-pos0.y <= connectDistance ) {
		return FALSE;
	}

	if ( GetTrkType(trk0) != T_STRAIGHT &&
		 GetTrkType(trk1) != T_STRAIGHT ) {
		aa = FindAngle( pos0, pos1 );
		aa = NormalizeAngle( aa-a0+connectAngle/2.0);
		if (aa > connectAngle)
			return FALSE;
	}
	UndoStart( _("Extending Straight Track"), "ExtendStraightToJoin( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	UndoModify( trk0 );
	UndoModify( trk1 );
	trk2 = trk0x = trk1x = NULL;
	if ( GetTrkType(trk0) == T_STRAIGHT ) {
		pos0 = GetTrkEndPos( trk0, 1-ep0 );
		trk0x = GetTrkEndTrk( trk0, 1-ep0 );
		if (trk0x) {
			ep0x = GetEndPtConnectedToMe( trk0x, trk0 );
			DisconnectTracks( trk0, 1-ep0, trk0x, ep0x );
		}
		trk2 = trk0;
		UndrawNewTrack( trk2 );
	} else {
		trk0x = trk0;
		ep0x = ep0;
		DrawEndPt( &mainD, trk0, ep0, wDrawColorWhite );
	}
	if ( GetTrkType(trk1) == T_STRAIGHT ) {
		pos1 = GetTrkEndPos( trk1, 1-ep1 );
		trk1x = GetTrkEndTrk( trk1, 1-ep1 );
		if (trk1x) {
			ep1x = GetEndPtConnectedToMe( trk1x, trk1 );
			DisconnectTracks( trk1, 1-ep1, trk1x, ep1x );
		}
		if (trk2) {
			DeleteTrack( trk1, TRUE );
		} else {
			trk2 = trk1;
			UndrawNewTrack( trk2 );
		}
	} else {
		trk1x = trk1;
		ep1x = ep1;
		DrawEndPt( &mainD, trk1, ep1, wDrawColorWhite );
	}

	if (trk2) {
		SetTrkEndPoint( trk2, 0, pos0, NormalizeAngle(a0+180.0) );
		SetTrkEndPoint( trk2, 1, pos1, NormalizeAngle(a1+180.0) );
		ComputeBoundingBox( trk2 );
	} else {
		trk2 = NewStraightTrack( pos0, pos1 );
	}
	if (trk0x) {
		ConnectTracks( trk2, 0, trk0x, ep0x );
	}
	if (trk1x) {
		ConnectTracks( trk2, 1, trk1x, ep1x );
	}
	DrawNewTrack( trk2 );

	return TRUE;
}


static STATUS_T ModifyStraight( track_p trk, wAction_t action, coOrd pos )
{
	static EPINX_T ep;
	static BOOL_T valid;
	DIST_T d;

	switch ( action ) {
	case C_DOWN:
		ep = PickUnconnectedEndPoint( pos, trk );
		if (ep == -1)
			return C_ERROR;
		UndrawNewTrack( trk );
		tempSegs(0).type = SEG_STRTRK;
		tempSegs(0).width = 0;
		tempSegs(0).u.l.pos[0] = GetTrkEndPos( trk, 1-ep );
		tempSegs_da.cnt = 1;
		InfoMessage( _("Drag to change track length") );

	case C_MOVE:
		d = FindDistance( tempSegs(0).u.l.pos[0], pos );
		valid = TRUE;
		if ( d <= minLength ) {
			if (action == C_MOVE)
				ErrorMessage( MSG_TRK_TOO_SHORT, _("Straight "), PutDim(fabs(minLength-d)) );
			valid = FALSE;
			return C_CONTINUE;
		}
		Translate( &tempSegs(0).u.l.pos[1], tempSegs(0).u.l.pos[0], GetTrkEndAngle( trk, ep ), d );
		tempSegs_da.cnt = 1;
		if (action == C_MOVE)
			InfoMessage( _("Straight: Length=%s Angle=%0.3f"),
					FormatDistance( d ), PutAngle( GetTrkEndAngle( trk, ep ) ) );
        MainRedraw();
		return C_CONTINUE;

	case C_UP:
		if (valid)
			AdjustStraightEndPt( trk, ep, tempSegs(0).u.l.pos[1] );
		tempSegs_da.cnt = 0;
        DrawNewTrack( trk );
        MainRedraw();
		return C_TERMINATE;

	default:
		;
	}
	return C_ERROR;
}


static DIST_T GetLengthStraight( track_p trk )
{
	return FindDistance( GetTrkEndPos(trk,0), GetTrkEndPos(trk,1) );
}


static BOOL_T GetParamsStraight( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	params->type = curveTypeStraight;
	if ( inx == PARAMS_PARALLEL ) {
		params->ep = 0;
	} else if (inx == PARAMS_CORNU ){
		params->ep = PickEndPoint( pos, trk);
	} else {
		params->ep = PickUnconnectedEndPointSilent( pos, trk );
	}
    if (params->ep == -1)
			return FALSE;
	params->lineOrig = GetTrkEndPos(trk,1-params->ep);
	params->lineEnd = GetTrkEndPos(trk,params->ep);
	params->len = FindDistance( params->lineOrig, params->lineEnd );
	params->track_angle = FindAngle( params->lineOrig, params->lineEnd);
	params->angle = GetTrkEndAngle(trk,params->ep);
	params->arcR = 0.0;
	return TRUE;
}


static BOOL_T MoveEndPtStraight( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d0 )
{
	if ( NormalizeAngle( FindAngle( GetTrkEndPos(*trk,1-*ep), pos ) -
							 GetTrkEndAngle(*trk,*ep) + 0.5 ) > 1.0 ) {
			ErrorMessage( MSG_MOVED_BEYOND_END_TRK );
			return FALSE;
	}
	Translate( &pos, pos, GetTrkEndAngle(*trk,*ep)+180, d0 );
	AdjustStraightEndPt( *trk, *ep, pos );
	return TRUE;
}


static BOOL_T QueryStraight( track_p trk, int query )
{
	switch ( query ) {
	case Q_CAN_PARALLEL:
	case Q_CAN_MODIFYRADIUS:
	case Q_CAN_GROUP:
	case Q_ISTRACK:
	case Q_CAN_EXTEND:
		return TRUE;
	default:
		return FALSE;
	}
}


static void FlipStraight(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	ComputeBoundingBox( trk );
}


static BOOL_T MakeParallelStraight(
		track_p trk,
		coOrd pos,
		DIST_T sep,
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R )
{
	ANGLE_T angle = GetTrkEndAngle(trk,1);
	coOrd p0, p1;
	if ( NormalizeAngle( FindAngle( GetTrkEndPos(trk,0), pos ) - GetTrkEndAngle(trk,1) ) < 180.0 )
		angle += 90;
	else
		angle -= 90;
	Translate( &p0, GetTrkEndPos(trk,0), angle, sep );
	Translate( &p1, GetTrkEndPos(trk,1), angle, sep );
	if ( newTrkR ) {
		*newTrkR = NewStraightTrack( p0, p1 );
	} else {
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 1;
		tempSegs(0).type = SEG_STRTRK;
		tempSegs(0).u.l.pos[0] = p0;
		tempSegs(0).u.l.pos[1] = p1;
	}
	if ( p0R ) *p0R = p0;
	if ( p1R ) *p1R = p1;
	return TRUE;
}


static trackCmd_t straightCmds = {
		"STRAIGHT",
		DrawStraight,
		DistanceStraight,
		DescribeStraight,
		DeleteStraight,
		WriteStraight,
		ReadStraight,
		MoveStraight,
		RotateStraight,
		RescaleStraight,
		AuditStraight,
		GetAngleStraight,
		SplitStraight,
		TraverseStraight,
		EnumerateStraight,
		NULL,			/* redraw */
		TrimStraight,
		ExtendStraightToJoin,
		ModifyStraight,
		GetLengthStraight,
		GetParamsStraight,
		MoveEndPtStraight,
		QueryStraight,
		NULL,			/* ungroup */
		FlipStraight,
		NULL,
		NULL,
		NULL,
		MakeParallelStraight };


EXPORT void StraightSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	ANGLE_T a0, a1, a2;
	DIST_T d, d0, d1;
	coOrd p0, p1;

	switch( cmd ) {

	case SEGPROC_TRAVERSE1:
		a1 = FindAngle( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		a2 = NormalizeAngle( a1-data->traverse1.angle );
		data->traverse1.backwards = ((a2 < 270) && (a2 > 90));
		data->traverse1.dist = FindDistance( segPtr->u.l.pos[data->traverse1.backwards?1:0], data->traverse1.pos );
		data->traverse1.reverse_seg = FALSE;
		break;

	case SEGPROC_TRAVERSE2:
		d = FindDistance( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		if ( d >= data->traverse2.dist ) {
			a1 = FindAngle( segPtr->u.l.pos[data->traverse2.segDir], segPtr->u.l.pos[1-data->traverse2.segDir] );
			Translate( &data->traverse2.pos, segPtr->u.l.pos[data->traverse2.segDir], a1, data->traverse2.dist );
			data->traverse2.dist = 0;
			data->traverse2.angle = a1;
		} else {
			a1 = FindAngle( segPtr->u.l.pos[data->traverse2.segDir], segPtr->u.l.pos[1-data->traverse2.segDir] );
			Translate( &data->traverse2.pos, segPtr->u.l.pos[data->traverse2.segDir], a1, d );
			data->traverse2.dist -= d;
			data->traverse2.angle = a1;
		}
		break;

	case SEGPROC_DRAWROADBEDSIDE:
		d0 = FindDistance( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		a0 = FindAngle( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		d1 = d0*data->drawRoadbedSide.first/32.0;
		Translate( &p0, segPtr->u.l.pos[0], a0, d1 );
		Translate( &p0, p0, a0+data->drawRoadbedSide.side, data->drawRoadbedSide.roadbedWidth/2.0 );
		d1 = d0*data->drawRoadbedSide.last/32.0;
		Translate( &p1, segPtr->u.l.pos[0], a0, d1 );
		Translate( &p1, p1, a0+data->drawRoadbedSide.side, data->drawRoadbedSide.roadbedWidth/2.0 );
		REORIGIN1( p0, data->drawRoadbedSide.angle, data->drawRoadbedSide.orig );
		REORIGIN1( p1, data->drawRoadbedSide.angle, data->drawRoadbedSide.orig );
		DrawLine( data->drawRoadbedSide.d, p0, p1, data->drawRoadbedSide.rbw, data->drawRoadbedSide.color );
		break;

	case SEGPROC_DISTANCE:
		data->distance.dd = LineDistance( &data->distance.pos1, segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		break;

	case SEGPROC_FLIP:
		p0 = segPtr->u.l.pos[0];
		segPtr->u.l.pos[0] = segPtr->u.l.pos[1];
		segPtr->u.l.pos[1] = p0;
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewStraightTrack( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		data->newTrack.ep[0] = 0;
		data->newTrack.ep[1] = 1;
		break;

	case SEGPROC_LENGTH:
		data->length.length = FindDistance( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		break;

	case SEGPROC_SPLIT:
		d = FindDistance( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		data->split.length[0] = FindDistance( segPtr->u.l.pos[0], data->split.pos );
		if ( data->split.length[0] <= d ) {
			data->split.length[1] = d-data->split.length[0];
		} else {
			data->split.length[0] = d;
			data->split.length[1] = 0.0;
		}
		Translate( &p0, segPtr->u.l.pos[0], FindAngle( segPtr->u.l.pos[0], segPtr->u.l.pos[1] ), data->split.length[0] );
		data->split.newSeg[0] = *segPtr;
		data->split.newSeg[1] = *segPtr;
		data->split.newSeg[0].u.l.pos[1] = data->split.newSeg[1].u.l.pos[0] = p0;
		break;
	/*
	 * Note GetAngle always gives a positive angle because p0 is always left of p1
	 */
	case SEGPROC_GETANGLE:
		data->getAngle.angle = FindAngle( segPtr->u.l.pos[0], segPtr->u.l.pos[1] );
		data->getAngle.radius = 0.0;
		break;
	}
}


/****************************************
 *
 * GRAPHICS EDITING
 *
 */


track_p NewStraightTrack( coOrd p0, coOrd p1 )
{
	track_p t;
	ANGLE_T a;
	t = NewTrack( 0, T_STRAIGHT, 2, 0 );
	SetTrkScale( t, curScaleInx );
	a = FindAngle( p1, p0 );
	SetTrkEndPoint( t, 0, p0, a );
	SetTrkEndPoint( t, 1, p1, NormalizeAngle( a+180.0 ) );
	ComputeBoundingBox( t );
	CheckTrackLength( t );
	return t;
}




void InitTrkStraight( void )
{
	T_STRAIGHT = InitObject( &straightCmds );
}
