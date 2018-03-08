/** \file tcurve.c
 * CURVE
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

#include <assert.h>
#include <math.h>

#include "ccurve.h"
#include "cjoin.h"
#include "cstraigh.h"
#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

static TRKTYP_T T_CURVE = -1;

struct extraData {
		coOrd pos;
		DIST_T radius;
		BOOL_T circle;
		long helixTurns;
		coOrd descriptionOff;
		};
#define xpos extraData->pos
#define xradius extraData->radius
#define xcircle extraData->circle

static int log_curve = 0;
static int log_curveSegs = 0;

static DIST_T GetLengthCurve( track_p );

/****************************************
 *
 * UTILITIES
 *
 */

static void GetCurveAngles( ANGLE_T *a0, ANGLE_T *a1, track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	assert( trk != NULL );
	if (xx->circle != TRUE) {
		*a0 = NormalizeAngle( GetTrkEndAngle(trk,0) + 90 );
		*a1 = NormalizeAngle(
				GetTrkEndAngle(trk,1) - GetTrkEndAngle(trk,0) + 180 );
	} else {
		*a0 = 0.0;
		*a1 = 360.0;
	}
LOG( log_curve, 4, ( "getCurveAngles: = %0.3f %0.3f\n", *a0, *a1 ) )
}

static void SetCurveAngles( track_p p, ANGLE_T a0, ANGLE_T a1, struct extraData * xx )
{
	coOrd pos0, pos1;
	xx->circle = (a0 == 0.0 && a1 == 0.0);
	PointOnCircle( &pos0, xx->pos, xx->radius, a0 );
	PointOnCircle( &pos1, xx->pos, xx->radius, a0+a1 );
	SetTrkEndPoint( p, 0, pos0, NormalizeAngle(a0-90.0) );
	SetTrkEndPoint( p, 1, pos1, NormalizeAngle(a0+a1+90.0) );
}

static void ComputeCurveBoundingBox( track_p trk, struct extraData * xx )
{
	coOrd p = xx->pos;
	DIST_T r = xx->radius;
	ANGLE_T a0, a1, aa;
	POS_T x0, x1, y0, y1;
	coOrd hi, lo;

	GetCurveAngles( &a0, &a1, trk );
	if ( xx->helixTurns > 0 ) {
		a0 = 0.0;
		a1 = 360.0;
	}
	aa = a0+a1;
	x0 = r * sin(D2R(a0));
	x1 = r * sin(D2R(aa));
	y0 = r * cos(D2R(a0));
	y1 = r * cos(D2R(aa));
	hi.y = p.y + ((aa>=360.0) ? (r) : max(y0,y1));
	lo.y = p.y + (((a0>180.0?aa-180.0:aa+180.0)>=360.0) ? (-r) : min(y0,y1));
	hi.x = p.x + (((a0> 90.0?aa- 90.0:aa+270.0)>=360.0) ? (r) : max(x0,x1));
	lo.x = p.x + (((a0>270.0?aa-270.0:aa+ 90.0)>=360.0) ? (-r) : min(x0,x1));
	SetBoundingBox( trk, hi, lo );
}

static void AdjustCurveEndPt( track_p t, EPINX_T inx, ANGLE_T a )
{
	struct extraData *xx = GetTrkExtraData(t);
	coOrd pos;
	ANGLE_T aa;
	if (GetTrkType(t) != T_CURVE) {
		AbortProg( "AdjustCurveEndPt( %d, %d ) not on CURVE %d",
				GetTrkIndex(t), inx, GetTrkType(t) );
		return;
	}
	UndoModify( t );
LOG( log_curve, 1, ( "adjustCurveEndPt T%d[%d] a=%0.3f\n", GetTrkIndex(t), inx, a ) )
	aa = a = NormalizeAngle(a);
	a += inx==0?90.0:-90.0;
	(void)PointOnCircle( &pos, xx->pos, xx->radius, a );
	SetTrkEndPoint( t, inx, pos, aa );
	if (xx->circle) {
		(void)PointOnCircle( &pos, xx->pos, xx->radius, aa );
		SetTrkEndPoint( t, 1-inx, pos, a );
		xx->circle = 0;
	}
LOG( log_curve, 1, ( "    E0:[%0.3f %0.3f] A%0.3f, E1:[%0.3f %0.3f] A%0.3f\n",
				GetTrkEndPosXY(t,0), GetTrkEndAngle(t,0),
				GetTrkEndPosXY(t,1), GetTrkEndAngle(t,1) ) )
	ComputeCurveBoundingBox( t, xx );
	CheckTrackLength( t );
}

static void GetTrkCurveCenter( track_p t, coOrd *p, DIST_T *r )
{
	struct extraData *xx = GetTrkExtraData(t);
	*p = xx->pos;
	*r = xx->radius;
}

BOOL_T IsCurveCircle( track_p t )
{
	struct extraData *xx;
	if ( GetTrkType(t) != T_CURVE )
		return FALSE;
	xx = GetTrkExtraData(t);
	return xx->circle || xx->helixTurns>0;
}


BOOL_T GetCurveMiddle( track_p trk, coOrd * pos )
{
	struct extraData *xx;
	ANGLE_T a0, a1;
	if ( GetTrkType(trk) != T_CURVE )
		return FALSE;
	xx = GetTrkExtraData(trk);
	if (xx->circle || xx->helixTurns>0) {
		PointOnCircle( pos, xx->pos, xx->radius, 0 );
	} else {
		GetCurveAngles( &a0, &a1, trk );
		PointOnCircle( pos, xx->pos, xx->radius, a0+a1/2 );
	}
	return TRUE;
}

DIST_T CurveDescriptionDistance(
		coOrd pos,
		track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	coOrd p1;
	FLOAT_T ratio;
	ANGLE_T a, a0, a1;

	if ( GetTrkType( trk ) != T_CURVE || ( GetTrkBits( trk ) & TB_HIDEDESC ) != 0 )
		return 100000;
	if ( xx->helixTurns > 0 ) {
		p1.x = xx->pos.x + xx->descriptionOff.x;
		p1.y = xx->pos.y + xx->descriptionOff.y;
	} else {
		GetCurveAngles( &a0, &a1, trk );
		ratio = ( xx->descriptionOff.x + 1.0 ) / 2.0;
		a = a0 + ratio * a1;
		ratio = ( xx->descriptionOff.y + 1.0 ) / 2.0;
		Translate( &p1, xx->pos, a, xx->radius * ratio );
	}
	return FindDistance( p1, pos );
}


static void DrawCurveDescription(
		track_p trk,
		drawCmd_p d,
		wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(trk);
	wFont_p fp;
	coOrd pos, p0, p1;
	DIST_T elev0, elev1, dist, grade=0, sep=0;
	BOOL_T elevValid;
	ANGLE_T a, a0, a1;
	FLOAT_T ratio;

	if (layoutLabels == 0)
		return;
	if ((labelEnable&LABELENABLE_TRKDESC)==0)
		return;

	if ( xx->helixTurns > 0 ) {
		pos = xx->pos;
		pos.x += xx->descriptionOff.x;
		pos.y += xx->descriptionOff.y;
		dist = GetLengthCurve( trk );
		elevValid = FALSE;
		if ( (!xx->circle) &&
			 ComputeElev( trk, 0, FALSE, &elev0, NULL ) &&
			 ComputeElev( trk, 1, FALSE, &elev1, NULL ) ) {
			if( elev0 == elev1 )
				elevValid = FALSE;
			else {
				elevValid = TRUE;
				grade = fabs((elev1-elev0)/dist);
				sep = grade*(xx->radius*M_PI*2.0);
			}
		}
		fp = wStandardFont( F_TIMES, FALSE, FALSE );
		if (elevValid)
			sprintf( message, _("Helix: turns=%ld length=%s grade=%0.1f%% sep=%s"),
				xx->helixTurns,
				FormatDistance(dist),
				grade*100.0,
				FormatDistance(sep) );
		else
			sprintf( message, _("Helix: turns=%ld length=%s"),
				xx->helixTurns,
				FormatDistance(dist) );
		DrawBoxedString( BOX_BOX, d, pos, message, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
	} else {
		dist = trackGauge/2.0;
		DrawArc( d, xx->pos, dist, 0.0, 360.0, FALSE, 0, color );
		Translate( &p0, xx->pos, 90.0, dist );
		Translate( &p1, xx->pos, 270.0, dist );
		DrawLine( d, p0, p1, 0, color );
		Translate( &p0, xx->pos, 0.0, dist );
		Translate( &p1, xx->pos, 180.0, dist );
		DrawLine( d, p0, p1, 0, color );
		GetCurveAngles( &a0, &a1, trk );
		ratio = ( xx->descriptionOff.x + 1.0 ) / 2.0;
		a = a0 + ratio * a1;
		PointOnCircle( &p0, xx->pos, xx->radius, a );
		sprintf( message, "R %s", FormatDistance( xx->radius ) );
		ratio = ( xx->descriptionOff.y + 1.0 ) / 2.0;
		DrawDimLine( d, xx->pos, p0, message, (wFontSize_t)descriptionFontSize, ratio, 0, color, 0x11 );
	}
}


STATUS_T CurveDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	struct extraData *xx = GetTrkExtraData(trk);
	static coOrd p0,p1;
	static BOOL_T editMode;
	wDrawColor color;
	ANGLE_T a, a0, a1;
	DIST_T d;

	p0 = xx->pos;

	switch (action) {
	case C_DOWN:
	case C_MOVE:
	case C_UP:
		editMode = TRUE;
		color = GetTrkColor( trk, &mainD );
		if ( xx->helixTurns > 0 ) {
			xx->descriptionOff.x = (pos.x-xx->pos.x);
			xx->descriptionOff.y = (pos.y-xx->pos.y);
			p1 = pos;
			if (action != C_UP)
				DrawLine( &tempD, p0, p1, 0, wDrawColorBlack );
		} else {
			p1 = pos;
			GetCurveAngles( &a0, &a1, trk );
			if ( a1 < 1 ) a1 = 1.0;
			a = FindAngle( xx->pos, pos );
			if ( ! IsCurveCircle( trk ) ) {
				a = NormalizeAngle( a - a0 );
				if ( a > a1 ) {
					if ( a < a1 + ( 360.0 - a1 ) / 2 ) {
						a = a1;
					} else {
						a = 0.0;
					}
				}
			}
			xx->descriptionOff.x = ( a / a1 ) * 2.0 - 1.0;
			d = FindDistance( xx->pos, pos ) / xx->radius;
			if ( d > 0.9 )
				d = 0.9;
			if ( d < 0.1 )
				d = 0.1;
			xx->descriptionOff.y = d * 2.0 - 1.0;
			GetCurveAngles( &a0, &a1, trk );
			a = a0 + (0.5 * a1);
			PointOnCircle( &p0, xx->pos, xx->radius/2, a );
		}
		if (action == C_UP) editMode = FALSE;
		MainRedraw();
		MapRedraw();
		return action==C_UP?C_TERMINATE:C_CONTINUE;

	case C_REDRAW:
		if (editMode) {
			DrawLine( &tempD, p1, p0, 0, wDrawColorBlack );
		}
		break;
		
	}
	return C_CONTINUE;
}

/****************************************
 *
 * GENERIC FUNCTIONS
 *
 */

static struct {
		coOrd endPt[2];
		FLOAT_T elev[2];
		FLOAT_T length;
		coOrd center;
		DIST_T radius;
		long turns;
		DIST_T separation;
		ANGLE_T angle0;
		ANGLE_T angle1;
		ANGLE_T angle;
		FLOAT_T grade;
		descPivot_t pivot;
		unsigned int layerNumber;
		} crvData;
typedef enum { E0, Z0, E1, Z1, CE, RA, TU, SE, LN, AL, A1, A2, GR, PV, LY } crvDesc_e;
static descData_t crvDesc[] = {
/*E0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &crvData.endPt[0] },
/*Z0*/	{ DESC_DIM, N_("Z"), &crvData.elev[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &crvData.endPt[1] },
/*Z1*/	{ DESC_DIM, N_("Z"), &crvData.elev[1] },
/*CE*/	{ DESC_POS, N_("Center: X,Y"), &crvData.center },
/*RA*/	{ DESC_DIM, N_("Radius"), &crvData.radius },
/*TU*/	{ DESC_LONG, N_("Turns"), &crvData.turns },
/*SE*/	{ DESC_DIM, N_("Separation"), &crvData.separation },
/*LN*/	{ DESC_DIM, N_("Length"), &crvData.length },
/*AL*/	{ DESC_FLOAT, N_("Angular Length"), &crvData.angle },
/*A1*/	{ DESC_ANGLE, N_("CCW Angle"), &crvData.angle0 },
/*A2*/	{ DESC_ANGLE, N_("CW Angle"), &crvData.angle1 },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &crvData.grade },
/*PV*/	{ DESC_PIVOT, N_("Pivot"), &crvData.pivot },
/*LY*/	{ DESC_LAYER, N_("Layer"), &crvData.layerNumber },
		{ DESC_NULL } };

static void UpdateCurve( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	struct extraData *xx = GetTrkExtraData(trk);
	BOOL_T updateEndPts;
	ANGLE_T a0, a1;
	EPINX_T ep;
	struct extraData xx0;
	FLOAT_T turns;

	if ( inx == -1 )
		return;
	xx0 = *xx;
	updateEndPts = FALSE;
	GetCurveAngles( &a0, &a1, trk );
	switch ( inx ) {
	case CE:
		xx0.pos = crvData.center;
		updateEndPts = TRUE;
		break;
	case RA:
		if ( crvData.radius <= 0 ) {
			ErrorMessage( MSG_RADIUS_GTR_0 );
			crvData.radius = xx0.radius;
			crvDesc[RA].mode |= DESC_CHANGE;
		} else if (crvData.radius > 10000) {
			ErrorMessage( MSG_RADIUS_GTR_10000 );
			crvData.radius = xx0.radius;
			crvDesc[RA].mode |= DESC_CHANGE;
		} else {
			if ( crvData.pivot == DESC_PIVOT_FIRST || GetTrkEndTrk(trk,0) ) {
				Translate( &xx0.pos, xx0.pos, a0, xx0.radius-crvData.radius );
			} else if ( crvData.pivot == DESC_PIVOT_SECOND || GetTrkEndTrk(trk,1) ) {
				Translate( &xx0.pos, xx0.pos, a0+a1, xx0.radius-crvData.radius );
			} else {
				Translate( &xx0.pos, xx0.pos, a0+a1/2.0, xx0.radius-crvData.radius );
			}
			crvDesc[CE].mode |= DESC_CHANGE;
			xx0.radius = crvData.radius;
			crvDesc[LN].mode |= DESC_CHANGE;
			updateEndPts = TRUE;
		}
		break;
	case TU:
		if ( crvData.turns <= 0 ) {
			ErrorMessage( MSG_HELIX_TURNS_GTR_0 );
			crvData.turns = xx0.helixTurns;
			crvDesc[TU].mode |= DESC_CHANGE;
		} else {
			xx0.helixTurns = crvData.turns;
			crvDesc[LN].mode |= DESC_CHANGE;
			updateEndPts = TRUE;
			crvDesc[SE].mode |= DESC_CHANGE;
			crvDesc[GR].mode |= DESC_CHANGE;
		}
		break;
	case AL:
		if ( crvData.angle <= 0.0 || crvData.angle >= 360.0 ) {
			ErrorMessage( MSG_CURVE_OUT_OF_RANGE );
			crvData.angle = a1;
			crvDesc[AL].mode |= DESC_CHANGE;
		} else {
			if ( crvData.pivot == DESC_PIVOT_FIRST || GetTrkEndTrk(trk,0) ) {
				a1 = crvData.angle;
				crvData.angle1 = NormalizeAngle( a0+a1 );
				crvDesc[A2].mode |= DESC_CHANGE;
			} else if ( crvData.pivot == DESC_PIVOT_SECOND || GetTrkEndTrk(trk,1) ) {
				a0 = NormalizeAngle( a0+a1-crvData.angle );
				a1 = crvData.angle;
				crvData.angle0 = NormalizeAngle( a0 );
				crvDesc[A1].mode |= DESC_CHANGE;
			} else {
				a0 = NormalizeAngle( a0+a1/2.0-crvData.angle/2.0);
				a1 = crvData.angle;
				crvData.angle0 = NormalizeAngle( a0 );
				crvData.angle1 = NormalizeAngle( a0+a1 );
				crvDesc[A1].mode |= DESC_CHANGE;
				crvDesc[A2].mode |= DESC_CHANGE;
			}
			crvDesc[LN].mode |= DESC_CHANGE;
			updateEndPts = TRUE;
		}
		break;
	case A1:
		a0 = crvData.angle0 = NormalizeAngle( crvData.angle0 );
		a1 = NormalizeAngle( crvData.angle1-crvData.angle0 );
		if ( a1 <= 0.0 ) {
			ErrorMessage( MSG_CURVE_OUT_OF_RANGE );
		} else {
			updateEndPts = TRUE;
			crvData.angle = a1;
			crvDesc[AL].mode |= DESC_CHANGE;
			crvDesc[LN].mode |= DESC_CHANGE;
		}
		break;
	case A2:
		a1 = NormalizeAngle( crvData.angle1-crvData.angle0 );
		if ( a1 <= 0.0 ) {
			ErrorMessage( MSG_CURVE_OUT_OF_RANGE );
		} else {
			updateEndPts = TRUE;
			crvData.angle = a1;
			crvDesc[AL].mode |= DESC_CHANGE;
			crvDesc[LN].mode |= DESC_CHANGE;
		}
		break;
	case Z0:
	case Z1:
		ep = (inx==Z0?0:1);
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), crvData.elev[ep], NULL );
		ComputeElev( trk, 1-ep, FALSE, &crvData.elev[1-ep], NULL );
		if ( crvData.length > minLength )
			crvData.grade = fabs( (crvData.elev[0]-crvData.elev[1])/crvData.length )*100.0;
		else
			crvData.grade = 0.0;
		crvDesc[GR].mode |= DESC_CHANGE;
		crvDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
		if ( xx->helixTurns > 0 ) {
			turns = crvData.length/(2*M_PI*crvData.radius);
			crvData.separation = fabs(crvData.elev[0]-crvData.elev[1])/turns;
			crvDesc[SE].mode |= DESC_CHANGE;
		}
		return;
	case LY:
		SetTrkLayer( trk, crvData.layerNumber);
		break;
	default:
		AbortProg( "updateCurve: Bad inx %d", inx );
	}
	UndrawNewTrack( trk );
	*xx = xx0;
	if (updateEndPts) {
		if ( GetTrkEndTrk(trk,0) == NULL ) {
			(void)PointOnCircle( &crvData.endPt[0], xx0.pos, xx0.radius, a0 );
			SetTrkEndPoint( trk, 0, crvData.endPt[0], NormalizeAngle( a0-90.0 ) );
			crvDesc[E0].mode |= DESC_CHANGE;
		}
		if ( GetTrkEndTrk(trk,1) == NULL ) {
			(void)PointOnCircle( &crvData.endPt[1], xx0.pos, xx0.radius, a0+a1 );
			SetTrkEndPoint( trk, 1, crvData.endPt[1], NormalizeAngle( a0+a1+90.0 ) );
			crvDesc[E1].mode |= DESC_CHANGE;
		}
	}
	crvData.length = GetLengthCurve( trk );

	if ( crvDesc[SE].mode&DESC_CHANGE ) {
		DrawCurveDescription( trk, &mainD, wDrawColorWhite );
		DrawCurveDescription( trk, &mainD, wDrawColorBlack );
		turns = crvData.length/(2*M_PI*crvData.radius);
		crvData.separation = fabs(crvData.elev[0]-crvData.elev[1])/turns;
		if ( crvData.length > minLength )
			crvData.grade = fabs( (crvData.elev[0]-crvData.elev[1])/crvData.length )*100.0;
		else
			crvData.grade = 0.0;
		crvDesc[GR].mode |= DESC_CHANGE;
	}

	ComputeCurveBoundingBox( trk, xx );
	DrawNewTrack( trk );
}

static void DescribeCurve( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a0, a1;
	DIST_T d;
	int fix0, fix1;
	FLOAT_T turns;

	GetCurveAngles( &a0, &a1, trk );
	d = xx->radius * 2.0 * M_PI * a1 / 360.0;
	if (xx->helixTurns > 0) {
		d += (xx->helixTurns-(xx->circle?1:0)) * xx->radius * 2.0 * M_PI;
		sprintf( str, _("Helix Track(%d): Layer=%d Radius=%s Turns=%ld Length=%s Center=[%s,%s] EP=[%0.3f,%0.3f A%0.3f] [%0.3f,%0.3f A%0.3f]"),
				GetTrkIndex(trk),
				GetTrkLayer(trk)+1,
				FormatDistance(xx->radius),
				xx->helixTurns,
				FormatDistance(d),
				FormatDistance(xx->pos.x), FormatDistance(xx->pos.y),
				GetTrkEndPosXY(trk,0), GetTrkEndAngle(trk,0),
				GetTrkEndPosXY(trk,1), GetTrkEndAngle(trk,1) );
	} else {
		sprintf( str, _("Curved Track(%d): Layer=%d Radius=%s Length=%s Center=[%s,%s] EP=[%0.3f,%0.3f A%0.3f] [%0.3f,%0.3f A%0.3f]"),
				GetTrkIndex(trk),
				GetTrkLayer(trk)+1,
				FormatDistance(xx->radius),
				FormatDistance(d),
				FormatDistance(xx->pos.x), FormatDistance(xx->pos.y),
				GetTrkEndPosXY(trk,0), GetTrkEndAngle(trk,0),
				GetTrkEndPosXY(trk,1), GetTrkEndAngle(trk,1) );
	}

	fix0 = GetTrkEndTrk(trk,0)!=NULL;
	fix1 = GetTrkEndTrk(trk,1)!=NULL;

	crvData.endPt[0] = GetTrkEndPos(trk,0);
	crvData.endPt[1] = GetTrkEndPos(trk,1);
	crvData.length = GetLengthCurve(trk);
	crvData.center = xx->pos;
	crvData.radius = xx->radius;
	crvData.turns = xx->helixTurns;
	crvData.angle0 = NormalizeAngle( a0 );
	crvData.angle1 = NormalizeAngle( a0+a1);
	crvData.angle = a1;
	crvData.layerNumber = GetTrkLayer(trk);
	if ( !xx->circle ) {
		ComputeElev( trk, 0, FALSE, &crvData.elev[0], NULL );
		ComputeElev( trk, 1, FALSE, &crvData.elev[1], NULL );
	} else {
		crvData.elev[0] = crvData.elev[1] = 0;
	}
	ComputeElev( trk, 0, FALSE, &crvData.elev[0], NULL );
	ComputeElev( trk, 1, FALSE, &crvData.elev[1], NULL );
	if ( crvData.length > minLength )
		crvData.grade = fabs( (crvData.elev[0]-crvData.elev[1])/crvData.length )*100.0;
	else
		crvData.grade = 0.0;
	if ( xx->helixTurns > 0 ) {
		turns = crvData.length/(2*M_PI*crvData.radius);
		crvData.separation = fabs(crvData.elev[0]-crvData.elev[1])/turns;
		crvDesc[SE].mode |= DESC_CHANGE;
	}

	crvDesc[E0].mode =
	crvDesc[E1].mode = 
	crvDesc[LN].mode =
		DESC_RO;
	crvDesc[Z0].mode = (EndPtIsDefinedElev(trk,0)?0:DESC_RO)|DESC_NOREDRAW;
	crvDesc[Z1].mode = (EndPtIsDefinedElev(trk,1)?0:DESC_RO)|DESC_NOREDRAW;
	crvDesc[GR].mode = DESC_RO;
	crvDesc[CE].mode = (fix0|fix1)?DESC_RO:0;
	crvDesc[RA].mode =
	crvDesc[AL].mode =
		(fix0&fix1)?DESC_RO:0;
	crvDesc[TU].mode = DESC_NOREDRAW;
	crvDesc[A1].mode = fix0?DESC_RO:0;
	crvDesc[A2].mode = fix1?DESC_RO:0;
	crvDesc[PV].mode = (fix0|fix1)?DESC_IGNORE:0;
	crvDesc[LY].mode = DESC_NOREDRAW;
	crvData.pivot = (fix0&fix1)?DESC_PIVOT_NONE:
					fix0?DESC_PIVOT_FIRST:
					fix1?DESC_PIVOT_SECOND:
					DESC_PIVOT_MID;

	crvDesc[SE].mode |= DESC_IGNORE;
	if ( xx->circle ) {
		crvDesc[E0].mode |= DESC_IGNORE;
		crvDesc[Z0].mode |= DESC_IGNORE;
		crvDesc[E1].mode |= DESC_IGNORE;
		crvDesc[Z1].mode |= DESC_IGNORE;
		crvDesc[AL].mode |= DESC_IGNORE;
		crvDesc[A1].mode |= DESC_IGNORE;
		crvDesc[A2].mode |= DESC_IGNORE;
		crvDesc[PV].mode |= DESC_IGNORE;
	}
		
	if ( xx->helixTurns ) {
		if ( !xx->circle )
			crvDesc[SE].mode = DESC_RO;
		DoDescribe( _("Helix Track"), trk, crvDesc, UpdateCurve );
	} else if ( xx->circle ) {
		crvDesc[TU].mode |= DESC_IGNORE;
		DoDescribe( _("Circle Track"), trk, crvDesc, UpdateCurve );
	} else {
		crvDesc[TU].mode |= DESC_IGNORE;
		DoDescribe( _("Curved Track"), trk, crvDesc, UpdateCurve );
	}
}

static DIST_T DistanceCurve( track_p t, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(t);
	ANGLE_T a0, a1;
	DIST_T d;
	GetCurveAngles( &a0, &a1, t );
	if ( xx->helixTurns > 0 ) {
		a0 = 0.0;
		a1 = 360.0;
	}
	d = CircleDistance( p, xx->pos, xx->radius, a0, a1 );
	return d;
}

static void DrawCurve( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);
	ANGLE_T a0, a1;
	track_p tt = t;
	long widthOptions = DTS_LEFT|DTS_RIGHT|DTS_TIES;

	if (GetTrkWidth(t) == 2)
		widthOptions |= DTS_THICK2;
	if (GetTrkWidth(t) == 3)
		widthOptions |= DTS_THICK3;
	GetCurveAngles( &a0, &a1, t );
	if (xx->circle) {
		tt = NULL;
	}
	if (xx->helixTurns > 0) {
		a0 = 0.0;
		a1 = 360.0;
	}
	if ( ((d->funcs->options&wDrawOptTemp)==0) &&
		 (labelWhen == 2 || (labelWhen == 1 && (d->options&DC_PRINT))) &&
		 labelScale >= d->scale &&
		 ( GetTrkBits( t ) & TB_HIDEDESC ) == 0 ) {
		DrawCurveDescription( t, d, color );
	}
	DrawCurvedTrack( d, xx->pos, xx->radius, a0, a1,
				GetTrkEndPos(t,0), GetTrkEndPos(t,1),
				t, GetTrkGauge(t), color, widthOptions );
	if ( (d->funcs->options & wDrawOptTemp) == 0 &&
		 (d->options&DC_QUICK) == 0 &&
		 (!IsCurveCircle(t)) ) {
		DrawEndPt( d, t, 0, color );
		DrawEndPt( d, t, 1, color );
	}
}

static void DeleteCurve( track_p t )
{
}

static BOOL_T WriteCurve( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	long options;
	BOOL_T rc = TRUE;
	options = GetTrkWidth(t) & 0x0F;
	if ( ( ( GetTrkBits(t) & TB_HIDEDESC ) != 0 ) == ( xx->helixTurns > 0 ) )
		options |= 0x80;
	rc &= fprintf(f, "CURVE %d %d %ld 0 0 %s %d %0.6f %0.6f 0 %0.6f %ld %0.6f %0.6f\n", 
		GetTrkIndex(t), GetTrkLayer(t), (long)options,
		GetTrkScaleName(t), GetTrkVisible(t), xx->pos.x, xx->pos.y, xx->radius,
		xx->helixTurns, xx->descriptionOff.x, xx->descriptionOff.y )>0;
	rc &= WriteEndPt( f, t, 0 );
	rc &= WriteEndPt( f, t, 1 );
	rc &= fprintf(f, "\tEND\n" )>0;
	return rc;
}

static void ReadCurve( char * line )
{
	struct extraData *xx;
	track_p t;
	wIndex_t index;
	BOOL_T visible;
	DIST_T r;
	coOrd p;
	DIST_T elev;
	char scale[10];
	wIndex_t layer;
	long options;
	char * cp = NULL;

	if (!GetArgs( line+6, paramVersion<3?"dXZsdpYfc":paramVersion<9?"dLl00sdpYfc":"dLl00sdpffc", 
		&index, &layer, &options, scale, &visible, &p, &elev, &r, &cp ) ) {
		return;
	}
	t = NewTrack( index, T_CURVE, 0, sizeof *xx );
	xx = GetTrkExtraData(t);
	SetTrkVisible(t, visible);
	SetTrkScale(t, LookupScale(scale));
	SetTrkLayer(t, layer );
	SetTrkWidth(t, (int)(options&3));
	xx->pos = p;
	xx->radius = r;
	xx->helixTurns = 0;
	xx->descriptionOff.x = xx->descriptionOff.y = 0.0;
	if (cp) {
		GetArgs( cp, "lp", &xx->helixTurns, &xx->descriptionOff );
	}
	if ( ( ( options & 0x80 ) != 0 ) == ( xx->helixTurns > 0 ) ) 
		SetTrkBits(t,TB_HIDEDESC);
	ReadSegs();
	SetEndPts(t,2);
	if (GetTrkEndAngle( t, 0 ) == 270.0 &&
		GetTrkEndAngle( t, 1 ) == 90.0 )
		xx->circle = TRUE;
	ComputeCurveBoundingBox( t, xx );
}

static void MoveCurve( track_p trk, coOrd orig )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->pos.x += orig.x;
	xx->pos.y += orig.y;
	ComputeCurveBoundingBox( trk, xx );
}

static void RotateCurve( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
	Rotate( &xx->pos, orig, angle );
	ComputeCurveBoundingBox( trk, xx );
}

static void RescaleCurve( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->pos.x *= ratio;
	xx->pos.y *= ratio;
	xx->radius *= ratio;
}

static ANGLE_T GetAngleCurve( track_p trk, coOrd pos, EPINX_T *ep0, EPINX_T *ep1 )
{
	coOrd center;
	DIST_T radius;
	if ( ep0 ) *ep0 = 0;
	if ( ep1 ) *ep1 = 1;
	GetTrkCurveCenter( trk, &center, &radius );
	return FindAngle( center, pos ) - 90.0;
}

static BOOL_T SplitCurve( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a, a0, a1;
	track_p trk1;

	if ( xx->helixTurns > 0 ) {
		ErrorMessage( MSG_CANT_SPLIT_TRK, _("Helix") );
		return FALSE;
	}
	a = FindAngle( xx->pos, pos );
	GetCurveAngles( &a0, &a1, trk );
	if (xx->circle) {
		a0 = a;
		a1 = 359;
		SetCurveAngles( trk, a0, a1, xx );
		*leftover = NULL;
		return TRUE;
	}
	if (ep == 0)
		a1 = NormalizeAngle(a-a0);
	else {
		a1 = NormalizeAngle(a0+a1-a);
		a0 = a;
	}
	trk1 = NewCurvedTrack( xx->pos, xx->radius, a0, a1, 0 );
	AdjustCurveEndPt( trk, ep, a+(ep==0?-90.0:90.0) );
	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;

	return TRUE;
}

static BOOL_T TraverseCurve( traverseTrack_p trvTrk, DIST_T * distR )
{
	track_p trk = trvTrk->trk;
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a, a0, a1, a2, a3;
	DIST_T arcDist;
	DIST_T circum;
	DIST_T dist;
	long turns;
	if ( xx->circle )
		return FALSE;
	circum = 2*M_PI*xx->radius;
	GetCurveAngles( &a0, &a1, trk );
	a2 = FindAngle( xx->pos, trvTrk->pos );
	a = NormalizeAngle( (a2-90.0) - trvTrk->angle );
	if ( xx->helixTurns <= 0 ) {
		if ( NormalizeAngle(a2-a0) > a1 ) {
			if ( NormalizeAngle( a2-(a0+a1/2.0+180.0 ) ) < 180.0 )
				a2 = a0;
			else
				a2 = NormalizeAngle(a0+a1);
		}
	}
	if ( a>270 || a<90 )
		arcDist = NormalizeAngle(a2-a0)/360.0*circum;
	else
		arcDist = NormalizeAngle(a0+a1-a2)/360.0*circum;
	if ( xx->helixTurns > 0 ) {
		turns = xx->helixTurns;
		if ( NormalizeAngle(a2-a0) > a1 )
			turns -= 1;
		dist = (a1/360.0+xx->helixTurns)*circum;
		if ( trvTrk->length < 0 ) {
			trvTrk->length = dist;
			trvTrk->dist = a1/360.0*circum - arcDist;
			while ( trvTrk->dist < 0 ) {
				if ( trvTrk->dist > -0.1 )
					trvTrk->dist = 0.0;
				else
					trvTrk->dist += circum;
			}
		} else {
			if ( trvTrk->length != dist ) {
				printf( "traverseCurve: trvTrk->length(%0.3f) != Dist(%0.3f)\n", trvTrk->length, dist );
				trvTrk->length = dist;
			}
			if ( trvTrk->length < trvTrk->dist ) {
				printf( "traverseCurve: trvTrk->length(%0.3f) < trvTrk->dist(%0.3f)\n", trvTrk->length, trvTrk->dist );
				trvTrk->dist = trvTrk->length;
			}
			a3 = trvTrk->dist/circum*360.0;
			if ( a>270 || a<90 )
				a3 = (a0+a1-a3);
			else
				a3 = (a0+a3);
			a3 = NormalizeAngle(a3);
			if ( NormalizeAngle(a2-a3+1.0) > 2.0 )
				printf( "traverseCurve: A2(%0.3f) != A3(%0.3f)\n", a2, a3 );
			turns = (int)((trvTrk->length-trvTrk->dist)/circum);
		}
		arcDist += turns * circum;
	}
	if ( a>270 || a<90 ) {
		/* CCW */
		if ( arcDist < *distR ) {
			PointOnCircle( &trvTrk->pos, xx->pos, xx->radius, a0 );
			*distR -= arcDist;
			trvTrk->angle = NormalizeAngle( a0-90.0 );
			trk = GetTrkEndTrk( trk, 0 );
		} else {
			trvTrk->dist += *distR;
			a2 -= *distR/circum*360.0;
			PointOnCircle( &trvTrk->pos, xx->pos, xx->radius, a2 );
			*distR = 0;
			trvTrk->angle = NormalizeAngle( a2-90.0 );
		}
	} else {
		/* CW */
		if ( arcDist < *distR ) {
			PointOnCircle( &trvTrk->pos, xx->pos, xx->radius, a0+a1 );
			*distR -= arcDist;
			trvTrk->angle = NormalizeAngle( a0+a1+90.0 );
			trk = GetTrkEndTrk( trk, 1 );
		} else {
			trvTrk->dist += *distR;
			a2 += *distR/circum*360.0;
			PointOnCircle( &trvTrk->pos, xx->pos, xx->radius, a2 );
			*distR = 0;
			trvTrk->angle = NormalizeAngle( a2+90.0 );
		}
	}
	trvTrk->trk = trk;
	return TRUE;
}


static BOOL_T EnumerateCurve( track_p trk )
{
	struct extraData *xx;
	ANGLE_T a0, a1;
	DIST_T d;
	if (trk != NULL) {
		xx = GetTrkExtraData(trk);
		GetCurveAngles( &a0, &a1, trk );
		d = xx->radius * 2.0 * M_PI * a1 / 360.0;
		if (xx->helixTurns > 0)
			d += (xx->helixTurns-(xx->circle?1:0)) * xx->radius * 2.0 * M_PI;
		ScaleLengthIncrement( GetTrkScale(trk), d );
	}
	return TRUE;
}

static BOOL_T TrimCurve( track_p trk, EPINX_T ep, DIST_T dist )
{
	DIST_T d;
	DIST_T radius;
	ANGLE_T a, aa;
	ANGLE_T a0, a1;
	coOrd pos, center;
	struct extraData *xx = GetTrkExtraData(trk);
	if (xx->helixTurns>0) {
		ErrorMessage( MSG_CANT_TRIM_HELIX );
		return FALSE;
	}
	a = NormalizeAngle( GetTrkEndAngle(trk,ep) + 180.0 );
	Translate( &pos, GetTrkEndPos(trk,ep), a, dist );
	GetTrkCurveCenter( trk, &center, &radius );
	GetCurveAngles( &a0, &a1, trk );
	a = FindAngle( center, pos );
	aa = NormalizeAngle(a - a0);
	d = radius * aa * 2.0*M_PI/360.0;
	if ( aa <= a1 && d > minLength ) {
		UndrawNewTrack( trk );
		AdjustCurveEndPt( trk, ep, a+(ep==0?-90.0:90.0) );
		DrawNewTrack( trk );
	} else
		DeleteTrack( trk, TRUE );
	return TRUE;
}

static BOOL_T MergeCurve(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	struct extraData *xx0 = GetTrkExtraData(trk0);
	struct extraData *xx1 = GetTrkExtraData(trk1);
	ANGLE_T a00, a01, a10, a11;
	DIST_T d;
	track_p trk2;
	EPINX_T ep2=-1;
	coOrd pos;

	if (ep0 == ep1)
		return FALSE;
	if ( IsCurveCircle(trk0) ||
		 IsCurveCircle(trk1) )
		return FALSE;
	if ( xx0->helixTurns > 0 ||
		 xx1->helixTurns > 0 )
		return FALSE;
	d = FindDistance( xx0->pos, xx1->pos );
	d += fabs( xx0->radius - xx1->radius );
	if ( d > connectDistance )
		return FALSE;

	GetCurveAngles( &a00, &a01, trk0 );
	GetCurveAngles( &a10, &a11, trk1 );

	UndoStart( _("Merge Curves"), "MergeCurve( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	UndoModify( trk0 );
	UndrawNewTrack( trk0 );
	trk2 = GetTrkEndTrk( trk1, 1-ep1 );
	if (trk2) {
		ep2 = GetEndPtConnectedToMe( trk2, trk1 );
		DisconnectTracks( trk1, 1-ep1, trk2, ep2 );
	}
	if (ep0 == 0) {
		(void)PointOnCircle( &pos, xx0->pos, xx0->radius, a10 );
		a10 = NormalizeAngle( a10-90.0 );
		SetTrkEndPoint( trk0, ep0, pos, a10 );
	} else {
		(void)PointOnCircle( &pos, xx0->pos, xx0->radius, a10+a11 );
		a10 = NormalizeAngle( a10+a11+90.0 );
		SetTrkEndPoint( trk0, ep0, pos, a10 );
	}
	DeleteTrack( trk1, FALSE );
	if (trk2) {
		ConnectTracks( trk0, ep0, trk2, ep2 );
	}
	DrawNewTrack( trk0 );
	ComputeCurveBoundingBox( trk0, GetTrkExtraData(trk0) );
	return TRUE;
}


static STATUS_T ModifyCurve( track_p trk, wAction_t action, coOrd pos )
{
	static BOOL_T arcTangent;
	static ANGLE_T arcA0, arcA1;
	static EPINX_T ep;
	static coOrd arcPos;
	static DIST_T arcRadius;
	static coOrd tangentOrig;
	static coOrd tangentEnd;
	static ANGLE_T angle;
	static easementData_t jointD;
	static BOOL_T valid;

	ANGLE_T a, aa1, aa2;
	DIST_T r, d;
	track_p trk1;
	struct extraData *xx = GetTrkExtraData(trk);

	switch ( action ) {

	case C_DOWN:
		arcTangent = FALSE;
		GetCurveAngles( &arcA0, &arcA1, trk );
		if ( arcA0 == 0.0 && arcA1 == 360.0 )
			return C_ERROR;
		if ( xx->helixTurns > 0 ) {
			return C_ERROR;
		}
		ep = PickUnconnectedEndPoint( pos, trk );
		if ( ep == -1 )
			return C_ERROR;
		GetTrkCurveCenter( trk, &arcPos, &arcRadius );
		UndrawNewTrack( trk );
		tempSegs(0).type = SEG_CRVTRK;
		tempSegs(0).width = 0;
		tempSegs(0).u.c.center = arcPos;
		tempSegs(0).u.c.radius = arcRadius;
		tempSegs(0).u.c.a0 = arcA0;
		tempSegs(0).u.c.a1 = arcA1;
		tempSegs_da.cnt = 1;
		InfoMessage( _("Drag to change angle or create tangent") );
	case C_MOVE:
		if (xx->helixTurns>0)
			return C_CONTINUE;
		valid = FALSE;
			a = FindAngle( arcPos, pos );
			r = FindDistance( arcPos, pos );
			if ( r > arcRadius*(arcTangent?1.0:1.10) ) {
				arcTangent = TRUE;
				if ( easeR > 0.0 && arcRadius < easeR ) {
					ErrorMessage( MSG_RADIUS_LSS_EASE_MIN,
						FormatDistance( arcRadius ), FormatDistance( easeR ) );
					return C_CONTINUE;
				}
				aa1 = 90.0-R2D( asin( arcRadius/r ) );
				aa2 = NormalizeAngle( a + (ep==0?aa1:-aa1) );
				PointOnCircle( &tangentOrig, arcPos, arcRadius, aa2 );
				if (ComputeJoint( ep==0?-arcRadius:+arcRadius, 0, &jointD ) == E_ERROR)
					return C_CONTINUE;
				tangentEnd = pos;
				if (jointD.x != 0.0) {
					Translate( &tangentOrig, tangentOrig, aa2, jointD.x );
					Translate( &tangentEnd, tangentEnd, aa2, jointD.x );
				}
				if (ep == 0) {
					tempSegs(0).u.c.a0 = aa2;
					tempSegs(0).u.c.a1 = NormalizeAngle( arcA0+arcA1-aa2 );
				} else {
					tempSegs(0).u.c.a1 = NormalizeAngle(aa2-arcA0);
				}
				d = arcRadius * tempSegs(0).u.c.a1 * 2.0*M_PI/360.0;
				d -= jointD.d0;
				if ( d <= minLength) {
					ErrorMessage( MSG_TRK_TOO_SHORT, _("Curved "), PutDim(fabs(minLength-d)) );
					return C_CONTINUE;
				}
				d = FindDistance( tangentOrig, tangentEnd );
				d -= jointD.d1;
				if ( d <= minLength) {
					ErrorMessage( MSG_TRK_TOO_SHORT, _("Tangent "), PutDim(fabs(minLength-d)) );
					return C_CONTINUE;
				}
				tempSegs(1).type = SEG_STRTRK;
				tempSegs(1).width = 0;
				tempSegs(1).u.l.pos[0] = tangentOrig;
				tempSegs(1).u.l.pos[1] = tangentEnd;
				tempSegs_da.cnt = 2;
				if (action == C_MOVE)
					InfoMessage( _("Tangent track: Length %s Angle %0.3f"),
						FormatDistance( d ),
						PutAngle( FindAngle( tangentOrig, tangentEnd ) ) );
			} else {
				arcTangent = FALSE;
				angle = NormalizeAngle( a +
						((ep==0)?-90:90));
				PointOnCircle( &pos, arcPos, arcRadius, a );
				if (ep != 0) {
					tempSegs(0).u.c.a0 = NormalizeAngle( GetTrkEndAngle(trk,0)+90.0 );
					tempSegs(0).u.c.a1 = NormalizeAngle( a-tempSegs(0).u.c.a0 );
				} else {
					tempSegs(0).u.c.a0 = a;
					tempSegs(0).u.c.a1 = NormalizeAngle( (GetTrkEndAngle(trk,1)-90.0) - a );
				}
				d = arcRadius*tempSegs(0).u.c.a1*2.0*M_PI/360.0;
				if ( d <= minLength ) {
					ErrorMessage( MSG_TRK_TOO_SHORT, _("Curved "), PutDim( fabs(minLength-d) ) );
					return C_CONTINUE;
				}
				tempSegs_da.cnt = 1;
				if (action == C_MOVE)
					InfoMessage( _("Curved: Radius=%s Length=%s Angle=%0.3f"),
								FormatDistance( arcRadius ), FormatDistance( d ),
								tempSegs(0).u.c.a1 );
			}
		valid = TRUE;
		return C_CONTINUE;
	case C_UP:
		if (xx->helixTurns>0)
			return C_CONTINUE;
		if (valid) {
			if (arcTangent) {
				trk1 = NewStraightTrack( tangentOrig, tangentEnd );
				CopyAttributes( trk, trk1 );
				/*UndrawNewTrack( trk );*/
				AdjustCurveEndPt( trk, ep, angle );
				JoinTracks( trk, ep, tangentOrig,
							trk1, 0, tangentOrig, &jointD );
				DrawNewTrack( trk1 );
			} else {
				AdjustCurveEndPt( trk, ep, angle );
			}
		}
		DrawNewTrack( trk );
		return C_TERMINATE;
	default:
		;
	}
	return C_ERROR;
}


static DIST_T GetLengthCurve( track_p trk )
{
	DIST_T dist, rad;
	ANGLE_T a0, a1;
	coOrd cen;
	struct extraData *xx = GetTrkExtraData(trk);

	GetTrkCurveCenter( trk, &cen, &rad );
	if (xx->circle)
		a1 = 360.0;
	else
		GetCurveAngles( &a0, &a1, trk );
	dist = (rad+GetTrkGauge(trk)/2.0)*a1*2.0*M_PI/360.0;
	if (xx->helixTurns>0)
		dist += (xx->helixTurns-(xx->circle?1:0)) * xx->radius * 2.0 * M_PI;
	return dist;
}


static BOOL_T GetParamsCurve( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	struct extraData *xx = GetTrkExtraData(trk);
	params->type = curveTypeCurve;
	GetTrkCurveCenter( trk, &params->arcP, &params->arcR);
	GetCurveAngles( &params->arcA0, &params->arcA1, trk );
	ANGLE_T angle1 = FindAngle(params->arcP,pos);

	params->track_angle = NormalizeAngle(FindAngle(params->arcP,pos)+90);

	if ( easeR > 0.0 && params->arcR < easeR ) {
		ErrorMessage( MSG_RADIUS_LSS_EASE_MIN,
						FormatDistance( params->arcR ), FormatDistance( easeR ) );
		return FALSE;
	}
	if ( inx == PARAMS_EXTEND && ( IsCurveCircle(trk) || xx->helixTurns > 0 ) ) {
		ErrorMessage( MSG_CANT_EXTEND_HELIX );
		return FALSE;
	}
	params->len = params->arcR * params->arcA1 *2.0*M_PI/360.0;
	if (xx->helixTurns > 0)
		params->len += (xx->helixTurns-(xx->circle?1:0)) * xx->radius * 2.0 * M_PI;
	params->helixTurns = xx->helixTurns;
	if ( inx == PARAMS_PARALLEL ) {
		params->ep = 0;
		if (xx->helixTurns > 0) {
			params->arcA0 = 0.0;
			params->arcA1 = 360.0;
		}
	} else {
		params->circleOrHelix = FALSE;
		if ( IsCurveCircle( trk ) ) {
			params->ep = PickArcEndPt( params->arcP, /*Dj.inp[0].*/pos, pos );
			params->angle = params->track_angle;
			params->circleOrHelix = TRUE;
			return TRUE;
		} else if (inx == PARAMS_CORNU ) {
			params->ep = PickEndPoint(pos, trk);
		} else {
			params->ep = PickUnconnectedEndPointSilent( pos, trk );
		}
		if (params->ep == -1)
			return FALSE;
		params->angle = GetTrkEndAngle(trk,params->ep); ;
	}
	return TRUE;
}


static BOOL_T MoveEndPtCurve( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d0 )
{
	coOrd posCen;
	DIST_T r;
	ANGLE_T angle0;
	ANGLE_T aa;
	
	GetTrkCurveCenter( *trk, &posCen, &r );
	angle0 = FindAngle( posCen, pos );
	aa = R2D( d0/r );
	if ( *ep==0 )
		angle0 += aa - 90.0;
	else
		angle0 -= aa - 90.0;
	AdjustCurveEndPt( *trk, *ep, angle0 );
	return TRUE;
}


static BOOL_T QueryCurve( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	switch ( query ) {
	case Q_CAN_PARALLEL:
	case Q_CAN_MODIFYRADIUS:
	case Q_CAN_GROUP:
	case Q_FLIP_ENDPTS:
	case Q_ISTRACK:
	case Q_HAS_DESC:
	case Q_CORNU_CAN_MODIFY:
	case Q_MODIFY_CAN_SPLIT:
		return TRUE;
		break;
	case Q_EXCEPTION:
		return xx->radius < GetLayoutMinTrackRadius() - EPSILON;
		break;
	case Q_NOT_PLACE_FROGPOINTS:
		return IsCurveCircle( trk );
		break;
	//case Q_CAN_EXTEND:
	//	if (xx->helixTurns > 0) return FALSE;
	//	return TRUE;
	//	break;
	case Q_CANNOT_PLACE_TURNOUT:
		return (xx->helixTurns > 0);
		break;
	case Q_HAS_VARIABLE_ENDPOINTS:
		if ((xx->helixTurns >0) || xx->circle) return TRUE;
		return FALSE;
		break;
	default:
		return FALSE;
	}
}


static void FlipCurve(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->pos, orig, angle );
	ComputeCurveBoundingBox( trk, xx );
}


static BOOL_T MakeParallelCurve(
		track_p trk,
		coOrd pos,
		DIST_T sep,
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R )
{
	struct extraData * xx = GetTrkExtraData(trk);
	struct extraData * xx1;
	DIST_T rad;
	ANGLE_T a0, a1;

	rad = FindDistance( pos, xx->pos );
	if ( rad > xx->radius )
		rad = xx->radius + sep;
	else
		rad = xx->radius - sep;
	GetCurveAngles( &a0, &a1, trk );
	if ( newTrkR ) {
		*newTrkR = NewCurvedTrack( xx->pos, rad, a0, a1, 0 );
		xx1 = GetTrkExtraData(*newTrkR);
		xx1->helixTurns = xx->helixTurns;
		xx1->circle = xx->circle;
		ComputeCurveBoundingBox( *newTrkR, xx1 );
	} else {
		if ( xx->helixTurns > 0) {
			a0 = 0;
			a1 = 360.0;
		}
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 1;
		tempSegs(0).type = SEG_CRVTRK;
		tempSegs(0).u.c.center = xx->pos;
		tempSegs(0).u.c.radius = rad;
		tempSegs(0).u.c.a0 = a0;
		tempSegs(0).u.c.a1 = a1;
	}
	if ( p0R ) PointOnCircle( p0R, xx->pos, rad, a0 );
	if ( p1R ) PointOnCircle( p1R, xx->pos, rad, a0+a1 );
	return TRUE;
}


static trackCmd_t curveCmds = {
		"CURVE",
		DrawCurve,
		DistanceCurve,
		DescribeCurve,
		DeleteCurve,
		WriteCurve,
		ReadCurve,
		MoveCurve,
		RotateCurve,
		RescaleCurve,
		NULL,
		GetAngleCurve,
		SplitCurve,
		TraverseCurve,
		EnumerateCurve,
		NULL,	/* redraw */
		TrimCurve,
		MergeCurve,
		ModifyCurve,
		GetLengthCurve,
		GetParamsCurve,
		MoveEndPtCurve,
		QueryCurve,
		NULL,	/* ungroup */
		FlipCurve,
		NULL,
		NULL,
		NULL,
		MakeParallelCurve };


EXPORT void CurveSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	ANGLE_T a0, a1, a2;
	DIST_T d, circum, d0;
	coOrd p0;
	wIndex_t s0, s1;

	switch (cmd) {

	case SEGPROC_TRAVERSE1:
		a1 = FindAngle( segPtr->u.c.center, data->traverse1.pos );
		a1 = NormalizeAngle(a1+90);               // ClockWise angle
		// work out within this segment which way we are going
		a2 = NormalizeAngle( a1 - data->traverse1.angle );
		data->traverse1.backwards = ((a2 < 270) && (a2 > 90 ));
		//Find angular distance from end opposite to direction of travel
		a2 = FindAngle( segPtr->u.c.center, data->traverse1.pos );
		//A segment may be fractionally too short - limit to angles within segment!
		int res = AngleInRange(a2,segPtr->u.c.a0,segPtr->u.c.a1);
		if (res == 1 ) {
LOG( log_curveSegs, 1, ("CrvSegsAngle miss A%0.3f S%0.3f E%0.3f R%d B%d \n",a2,segPtr->u.c.a0,segPtr->u.c.a1,res,data->traverse1.backwards))
			a2 = segPtr->u.c.a0;
		} else if (res == -1) {
LOG( log_curveSegs, 1, ("CrvSegsAngle miss A%0.3f S%0.3f E%0.3f R%d B%d \n",a2,segPtr->u.c.a0,segPtr->u.c.a1,res,data->traverse1.backwards))
			a2 = segPtr->u.c.a1+segPtr->u.c.a0;
		}
		//Fix issue of angles passing through zero -
		if ( !data->traverse1.backwards ) {
			a2 = NormalizeAngle(DifferenceBetweenAngles(segPtr->u.c.a0,a2));
		} else {
			a2 = NormalizeAngle(DifferenceBetweenAngles(a2,segPtr->u.c.a0+segPtr->u.c.a1));
		}

		//Make sure backwards means going towards EP0
		if (segPtr->u.c.radius<0) data->traverse1.backwards = !data->traverse1.backwards;
		data->traverse1.dist = a2/360.0*2*M_PI*fabs(segPtr->u.c.radius);  		//Distance from end in direction of travel
		data->traverse1.reverse_seg = ((segPtr->u.c.a0>=90) && (segPtr->u.c.a0<270));
		data->traverse1.negative = (segPtr->u.c.radius < 0);
		data->traverse1.segs_backwards = FALSE;
		data->traverse1.BezSegInx = 0;
LOG( log_curveSegs, 2, ("  CrvSegs D=%0.3f A%0.3f B%d \n",data->traverse1.dist,data->traverse1.backwards))
		break;

	case SEGPROC_TRAVERSE2:
		circum = 2*M_PI*segPtr->u.c.radius;
		if ( circum < 0 )
				circum = - circum;
		d = (segPtr->u.c.a1*circum)/360;
		if ( d > data->traverse2.dist ) {
			a2 = (data->traverse2.dist*360.0)/circum;
			data->traverse2.dist = 0;
		} else {
			a2 = segPtr->u.c.a1;
			data->traverse2.dist -= d;
		}
		if (segPtr->u.c.radius<0) data->traverse2.segDir  = !data->traverse2.segDir;
		if ( !data->traverse2.segDir ) {
			a2 = NormalizeAngle( segPtr->u.c.a0+a2 );
			a1 = NormalizeAngle(a2+90);
		} else {
			a2 = NormalizeAngle( segPtr->u.c.a0+segPtr->u.c.a1-a2 );
			a1 = NormalizeAngle(a2-90);
		}
		PointOnCircle( &data->traverse2.pos, segPtr->u.c.center, fabs(segPtr->u.c.radius), a2 );
		data->traverse2.angle = a1;

		break;

	case SEGPROC_DRAWROADBEDSIDE:
		REORIGIN( p0, segPtr->u.c.center, data->drawRoadbedSide.angle, data->drawRoadbedSide.orig );
		d0 = segPtr->u.c.radius;
		if ( d0 > 0 ) {
			a0 = NormalizeAngle( segPtr->u.c.a0 + segPtr->u.c.a1*data->drawRoadbedSide.first/32.0 + data->drawRoadbedSide.angle );
		} else {
			d0 = -d0;
			a0 = NormalizeAngle( segPtr->u.c.a0 + segPtr->u.c.a1*(32-data->drawRoadbedSide.last)/32.0 + data->drawRoadbedSide.angle );
		}
		a1 = segPtr->u.c.a1*(data->drawRoadbedSide.last-data->drawRoadbedSide.first)/32.0;
		if (data->drawRoadbedSide.side>0)
			d0 += data->drawRoadbedSide.roadbedWidth/2.0;
		else
			d0 -= data->drawRoadbedSide.roadbedWidth/2.0;
		DrawArc( data->drawRoadbedSide.d, p0, d0, a0, a1, FALSE, data->drawRoadbedSide.rbw, data->drawRoadbedSide.color );
		break;

	case SEGPROC_DISTANCE:
		data->distance.dd = CircleDistance( &data->distance.pos1, segPtr->u.c.center, fabs(segPtr->u.c.radius), segPtr->u.c.a0, segPtr->u.c.a1 );
		break;

	case SEGPROC_FLIP:
		segPtr->u.c.radius = - segPtr->u.c.radius;
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewCurvedTrack( segPtr->u.c.center, fabs(segPtr->u.c.radius), segPtr->u.c.a0, segPtr->u.c.a1, 0 );
		data->newTrack.ep[0] = (segPtr->u.c.radius>0?0:1);
		data->newTrack.ep[1] = 1-data->newTrack.ep[0];
		break;

	case SEGPROC_LENGTH:
		data->length.length = fabs(segPtr->u.c.radius) * segPtr->u.c.a1 * (2.0*M_PI/360.0);
		break;

	case SEGPROC_SPLIT:
		d = segPtr->u.c.a1/360.0 * 2*M_PI * fabs(segPtr->u.c.radius);
		a2 = FindAngle( segPtr->u.c.center, data->split.pos );
		a2 = NormalizeAngle( a2 - segPtr->u.c.a0 );
		if ( a2 > segPtr->u.c.a1 ) {
			if ( a2-segPtr->u.c.a1 < (360-segPtr->u.c.a1)/2.0 )
				a2 = segPtr->u.c.a1;
			else
				a2 = 0.0;
		}
		s0 = 0;
		if ( segPtr->u.c.radius<0 )
			s0 = 1-s0;
		s1 = 1-s0;
		data->split.length[s0] = a2/360.0 * 2*M_PI * fabs(segPtr->u.c.radius);
		data->split.length[s1] = d-data->split.length[s0];
		data->split.newSeg[0] = *segPtr;
		data->split.newSeg[1] = *segPtr;
		data->split.newSeg[s0].u.c.a1 = a2;
		data->split.newSeg[s1].u.c.a0 = NormalizeAngle( data->split.newSeg[s1].u.c.a0 + a2 );
		data->split.newSeg[s1].u.c.a1 -= a2;
		break;


	case SEGPROC_GETANGLE:
		data->getAngle.angle = NormalizeAngle( FindAngle( segPtr->u.c.center, data->getAngle.pos ) + 90 );
		data->getAngle.negative_radius = segPtr->u.c.radius<0;
		data->getAngle.radius = fabs(segPtr->u.c.radius);
		data->getAngle.center = segPtr->u.c.center;
		data->getAngle.backwards = segPtr->u.c.a0>=90 && segPtr->u.c.a0<270;
		if (data->getAngle.backwards) data->getAngle.angle = NormalizeAngle(data->getAngle.angle+180);
		break;
	}
}


/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */



EXPORT void PlotCurve(
		long mode,
		coOrd pos0,
		coOrd pos1,
		coOrd pos2,
		curveData_t * curveData,
		BOOL_T constrain )
{
	DIST_T d0, d2, r;
	ANGLE_T angle, a0, a1, a2;
	coOrd posx;

	switch ( mode ) {
	case crvCmdFromCornu:
	case crvCmdFromEP1:
		angle = FindAngle( pos0, pos1 );
		d0 = FindDistance( pos0, pos2 )/2.0;
		a0 = FindAngle( pos0, pos2 );
		a1 = NormalizeAngle( a0 - angle );
LOG( log_curve, 3, ( "P1 = [%0.3f %0.3f] D=%0.3f A0=%0.3f A1=%0.3f\n", pos2.x, pos2.y, d0, a0, a1 ) )
		if ((fabs(d0*sin(D2R(a1))) < (4.0/75.0)*mainD.scale) & (mode == crvCmdFromEP1)) {
LOG( log_curve, 3, ( "Straight: %0.3f < %0.3f\n", d0*sin(D2R(a1)), (4.0/75.0)*mainD.scale ) )
			curveData->pos1.x = pos0.x + d0*2.0*sin(D2R(angle));
			curveData->pos1.y = pos0.y + d0*2.0*cos(D2R(angle));
			curveData->type = curveTypeStraight;
		} else if (a1 >= 179.0 && a1 <= 181.0) { 
			curveData->type = curveTypeNone;
		} else {
			if (a1<180.0) {
				a2 = NormalizeAngle( angle + 90.0 );
				if (constrain)
					curveData->curveRadius = ConstrainR( d0/sin(D2R(a1)) );
				else
					curveData->curveRadius = d0/sin(D2R(a1));
			} else {
				a1 -= 360.0;
				a2 = NormalizeAngle( angle - 90.0 );
				if (constrain)
					curveData->curveRadius = ConstrainR( d0/sin(D2R(-a1)) );
				else
					curveData->curveRadius = d0/sin(D2R(-a1));
			}
			if (curveData->curveRadius > 1000 && mode == crvCmdFromEP1) {
				LOG( log_curve, 3, ( "Straight %0.3f > 1000\n", curveData->curveRadius ) )
				curveData->pos1.x = pos0.x + d0*2.0*sin(D2R(angle));
				curveData->pos1.y = pos0.y + d0*2.0*cos(D2R(angle));
				curveData->type = curveTypeStraight;
			} else {
				curveData->curvePos.x = pos0.x + curveData->curveRadius*sin(D2R(a2));
				curveData->curvePos.y = pos0.y + curveData->curveRadius*cos(D2R(a2));
				LOG( log_curve, 3, ( "Center = [%0.3f %0.3f] A1=%0.3f A2=%0.3f R=%0.3f\n",
					curveData->curvePos.x, curveData->curvePos.y, a1, a2, curveData->curveRadius ) )
				if (a1 > 0.0) {
					curveData->a0 = NormalizeAngle( a2-180 );
					curveData->a1 = a1 * 2.0;
					curveData->negative = FALSE;
				} else {
					curveData->a1 = (-a1) * 2.0;
					curveData->a0 = NormalizeAngle( a2-180-curveData->a1 );
					curveData->negative = TRUE;
				}
				curveData->type = curveTypeCurve;
			}
		}
		break;
	case crvCmdFromTangent:
	case crvCmdFromCenter:
		if ( mode == crvCmdFromCenter ) {
			curveData->curvePos = pos0;
			curveData->pos1 = pos1;
		} else {
			curveData->curvePos = pos1;
			curveData->pos1 = pos0;
		}
		curveData->curveRadius = FindDistance( pos0, pos1 );
		a0 = FindAngle( curveData->curvePos, curveData->pos1 );
		a1 = FindAngle( curveData->curvePos, pos2 );
		if ( NormalizeAngle(a1-a0) < 180 ) {
			curveData->a0 = a0;
			curveData->a1 = NormalizeAngle(a1-a0);
		} else {
			curveData->a0 = a1;
			curveData->a1 = NormalizeAngle(a0-a1);
		}
		curveData->type = curveTypeCurve;
		break;
	case crvCmdFromChord:
		curveData->pos1 = pos1;
		curveData->type = curveTypeStraight;
		a0 = FindAngle( pos1, pos0 );
		d0 = FindDistance( pos0, pos1 )/2.0;
		Rotate( &pos2, pos1, -a0 );
		pos2.x -= pos1.x;
		if ( fabs(pos2.x) < 0.01 )
			break;
		d2 = sqrt( d0*d0 + pos2.x*pos2.x )/2.0; 
		r = d2*d2*2.0/pos2.x;
		if ( r > 1000.0 )
			break;
		posx.x = (pos1.x+pos0.x)/2.0;
		posx.y = (pos1.y+pos0.y)/2.0;
		a0 -= 90.0;
		LOG( log_curve, 3, ( "CHORD: [%0.3f %0.3f] [%0.3f %0.3f] [%0.3f %0.3f] A0=%0.3f D0=%0.3f D2=%0.3f R=%0.3f\n", pos0.x, pos0.y, pos1.x, pos1.y, pos2.x, pos2.y, a0, d0, d2, r ) )
		Translate( &curveData->curvePos, posx, a0, r-pos2.x );
		curveData->curveRadius = fabs(r);
		a0 = FindAngle( curveData->curvePos, pos0 );
		a1 = FindAngle( curveData->curvePos, pos1 );
		if ( r > 0 ) {
			curveData->a0 = a0;
			curveData->a1 = NormalizeAngle(a1-a0);
			curveData->negative = FALSE;
		} else {
			curveData->a0 = a1;
			curveData->a1 = NormalizeAngle(a0-a1);
			curveData->negative = TRUE;
		}
		curveData->type = curveTypeCurve;
		break;
	}
}

EXPORT track_p NewCurvedTrack( coOrd pos, DIST_T r, ANGLE_T a0, ANGLE_T a1, long helixTurns )
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_CURVE, 2, sizeof *xx );
	xx = GetTrkExtraData(p);
	xx->pos = pos;
	xx->radius = r;
	xx->helixTurns = helixTurns;
	if ( helixTurns <= 0 )
		SetTrkBits( p, TB_HIDEDESC );
	SetCurveAngles( p, a0, a1, xx );
LOG( log_curve, 1, ( "NewCurvedTrack( %0.3f, %0.3f, %0.3f )  = %d\n", pos.x, pos.y, r, GetTrkIndex(p) ) )
	ComputeCurveBoundingBox( p, xx );
	CheckTrackLength( p );
	return p;
}



EXPORT void InitTrkCurve( void )
{
	T_CURVE = InitObject( &curveCmds );
	log_curve = LogFindIndex( "curve" );
	log_curveSegs = LogFindIndex( "curveSegs");
}
