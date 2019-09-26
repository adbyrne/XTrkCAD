
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
#include <stdarg.h>
#include <string.h>

#include "ccurve.h"
#include "cbezier.h"
#include "compound.h"
#include "cundo.h"
#include "drawgeom.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

static long drawGeomCurveMode;

#define contextSegs(N) DYNARR_N( trkSeg_t, context->Segs_da, N )

static dynArr_t points_da;
static dynArr_t anchors_da;
static dynArr_t select_da;

#define points(N) DYNARR_N( pts_t, points_da, N )
#define point_selected(N) DYNARR_N( wBool_t, select_da, N)
#define anchors(N) DYNARR_N( trkSeg_t, anchors_da, N)

static void EndPoly( drawContext_t * context, int cnt, wBool_t open)
{
	trkSeg_p segPtr;
	track_p trk;
	long oldOptions;
	pts_t * pts;
	int inx;

	if (context->State==0 || cnt == 0)
		return;
	
	oldOptions = context->D->funcs->options;
	context->D->funcs->options |= wDrawOptTemp;
	DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
	context->D->funcs->options = oldOptions;
	if ( cnt < 3 ) {
		tempSegs_da.cnt = 0;
		ErrorMessage( MSG_POLY_SHAPES_3_SIDES );
		return;
	}
	pts = (pts_t*)MyMalloc( (cnt) * sizeof (pts_t) );
	for ( inx=0; inx<cnt; inx++ ) {
		pts[inx].pt = tempSegs(inx).u.l.pos[0];
		pts[inx].pt_type = 0;
	}
	DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
	segPtr = &tempSegs(0);
	segPtr->type = ( (context->Op == OP_POLY || context->Op == OP_POLYLINE )? SEG_POLY:SEG_FILPOLY );
	segPtr->u.p.cnt = cnt;
	segPtr->u.p.pts = pts;
	segPtr->u.p.angle = 0.0;
	segPtr->u.p.orig = zero;
	segPtr->u.p.polyType = open?POLYLINE:FREEFORM;
	UndoStart( _("Create Lines"), "newDraw" );
	trk = MakeDrawFromSeg( zero, 0.0, segPtr );
	DrawNewTrack( trk );
	tempSegs_da.cnt = 0;
}



static void DrawGeomOk( void )
{
	track_p trk;
	int inx;

	if (tempSegs_da.cnt <= 0)
		return;
	UndoStart( _("Create Lines"), "newDraw" );
	for ( inx=0; inx<tempSegs_da.cnt; inx++ ) {
		trk = MakeDrawFromSeg( zero, 0.0, &tempSegs(inx) );
		DrawNewTrack( trk );
	}
	tempSegs_da.cnt = 0;
}

static void CreateEndAnchor(coOrd p, wBool_t lock) {
	DIST_T d = tempD.scale*0.15;

	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = lock?SEG_FILCRCL:SEG_CRVLIN;
	anchors(i).color = wDrawColorBlue;
	anchors(i).u.c.center = p;
	anchors(i).u.c.radius = d/2;
	anchors(i).u.c.a0 = 0.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).width = 0;
}

static void CreateSquareAnchor(coOrd p) {
	DIST_T d = tempD.scale*0.15;
	int i = anchors_da.cnt;
	DYNARR_SET(trkSeg_t,anchors_da,i+4);
	for (int j =0; j<4;j++) {
		anchors(i+j).type = SEG_STRLIN;
		anchors(i+j).color = wDrawColorBlue;
		anchors(i+j).width = 0;
	}
	anchors(i).u.l.pos[0].x = anchors(i+2).u.l.pos[1].x =
	anchors(i+3).u.l.pos[0].x = anchors(i+3).u.l.pos[1].x = p.x-d/2;

	anchors(i).u.l.pos[0].y = anchors(i).u.l.pos[1].y =
	anchors(i+1).u.l.pos[0].y =	anchors(i+3).u.l.pos[1].y = p.y-d/2;

	anchors(i).u.l.pos[1].x =
	anchors(i+1).u.l.pos[0].x = anchors(i+1).u.l.pos[1].x =
	anchors(i+2).u.l.pos[0].x = p.x+d/2;

	anchors(i+1).u.l.pos[1].y =
	anchors(i+2).u.l.pos[0].y = anchors(i+2).u.l.pos[1].y =
	anchors(i+3).u.l.pos[0].y = p.y+d/2;
}

/**
 * Create and draw a graphics primitive (lines, arc, circle). The complete handling of mouse 
 * movements and clicks during the editing process is done here. 
 *
 * \param action IN mouse action 
 * \param pos IN position of mouse pointer
 * \param context IN/OUT parameters for drawing op 
 * \return next command state
 *
 * Note - Poly supports both clicking points and/or dragging sides. Close is space or enter.
 * Note - This routine will be recalled to C_UPDATE the last action if the State is 2 and the user updates the dialog
 *
 */

STATUS_T DrawGeomMouse(
		wAction_t action,
		coOrd pos,
		drawContext_t *context)
{
	static int lastValid = FALSE;
	static wBool_t lock;
	static coOrd pos0, pos0x, pos1, lastPos;
	trkSeg_p segPtr;
	pts_t *pts;
	int inx;
	DIST_T width;
	static int segCnt;
	DIST_T d;
	ANGLE_T a1,a2;
	static ANGLE_T line_angle;
	BOOL_T createTrack;
	long oldOptions;

	width = context->line_Width/context->D->dpi;

	switch (action&0xFF) {

	case C_UPDATE:
		if (context->State == 0 ) return C_TERMINATE;
		if (context->Op != OP_POLY && context->Op != OP_FILLPOLY && context->Op != OP_POLYLINE && context->State == 1) return C_TERMINATE;
		switch (context->Op) {
		case OP_CIRCLE1:
		case OP_CIRCLE2:
		case OP_CIRCLE3:
		case OP_FILLCIRCLE1:
		case OP_FILLCIRCLE2:
		case OP_FILLCIRCLE3:
			tempSegs(0).u.c.radius = context->radius;
		break;
		case OP_CURVE1:
		case OP_CURVE2:
		case OP_CURVE3:
		case OP_CURVE4:
			if (context->ArcData.type == curveTypeCurve) {
				if (tempSegs(0).u.c.radius != context->radius) {
					coOrd end;
					Translate(&end,context->ArcData.curvePos,context->ArcData.a0,context->ArcData.curveRadius);
					tempSegs(0).u.c.radius = context->radius;
					Translate(&tempSegs(0).u.c.center,end,context->ArcData.a0+180,context->radius);
				}
				tempSegs(0).u.c.a1 = context->angle;
			} else
				Translate(&tempSegs(0).u.l.pos[1],tempSegs(0).u.l.pos[0],context->angle,context->length);
		break;
		case OP_LINE:
		case OP_BENCH:
		case OP_TBLEDGE:
			a1 = FindAngle(pos0,pos1);
			Translate(&tempSegs(0).u.l.pos[1],tempSegs(0).u.l.pos[0],context->angle,context->length);
			lastPos = pos1 = tempSegs(0).u.l.pos[1];
			tempSegs_da.cnt = 1;
			context->angle = NormalizeAngle(context->angle);
		break;
		case OP_BOX:
		case OP_FILLBOX:
			pts = tempSegs(0).u.p.pts;
			a1 = FindAngle(pts[0].pt,pts[1].pt);
			Translate(&pts[1].pt,pts[0].pt,a1,context->length);
			a2 = FindAngle(pts[0].pt,pts[3].pt);
			Translate(&pts[2].pt,pts[1].pt,a2,context->width);
			Translate(&pts[3].pt,pts[0].pt,a2,context->width);
			tempSegs_da.cnt = 1;
		break;
		case OP_POLY:
		case OP_FILLPOLY:
		case OP_POLYLINE:
			tempSegs_da.cnt = segCnt;
			if (segCnt>1) {
				ANGLE_T an = FindAngle(tempSegs(segCnt-2).u.l.pos[0],tempSegs(segCnt-2).u.l.pos[1]);
				an = an+context->angle;
				Translate(&tempSegs(segCnt-1).u.l.pos[1],tempSegs(segCnt-1).u.l.pos[0],an,context->length);
			} else {
				Translate(&tempSegs(0).u.l.pos[1],tempSegs(0).u.l.pos[0],context->angle,context->length);
			}
			pos1 = lastPos = tempSegs(segCnt-1).u.l.pos[1];
			context->angle = fabs(context->angle);
			if (context->angle >180) context->angle = context->angle - 180.0;
		break;
		default:
		break;
		}
		anchors_da.cnt = 0;
		MainRedraw();
		return C_CONTINUE;

	case C_START:
		context->State = 0;
		context->Changed = FALSE;
		segCnt = 0;
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		DYNARR_SET(trkSeg_t,anchors_da,5);
		DYNARR_RESET( trkSeg_t, anchors_da );
		lock = FALSE;
		InfoMessage(_("+Shift to lock to nearby objects"));
		return C_CONTINUE;

	case wActionMove:
		DYNARR_RESET( trkSeg_t, anchors_da );
		if (context->State == 0 || context->State ==2 ) {
			switch (context->Op) {  	//Snap pos to nearest line if this is end and just shift is depressed for lines and some curves
				case OP_CURVE2:
				case OP_CURVE4:
				case OP_LINE:
				case OP_DIMLINE:
				case OP_BENCH:
				case OP_POLY:
				case OP_FILLPOLY:
				case OP_POLYLINE:
					if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
						coOrd p = pos;
						track_p t;
						if ((t=OnTrack(&p,FALSE,FALSE))!=NULL) {
							CreateEndAnchor(p,TRUE);
						}
					};
					break;
				default:
					;
			}
		}
		if (anchors_da.cnt) {
			DrawSegs( context->D, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		}
		return C_CONTINUE;

	case wActionLDown:
		DYNARR_RESET( trkSeg_t, anchors_da );
		if (context->State == 2) {
			tempSegs_da.cnt = segCnt;
			if ((context->Op == OP_POLY || context->Op == OP_FILLPOLY || context->Op == OP_POLYLINE)) {
				EndPoly(context, segCnt, context->Op==OP_POLYLINE);
			} else {
				DrawGeomOk();
			}
			MainRedraw();
			segCnt = 0;
			anchors_da.cnt = 0;
			context->State = 0;
		}
		context->Started = TRUE;
		line_angle = 90.0;
		if ((context->Op == OP_CURVE1 && context->State == 1) ||
			(context->Op == OP_CURVE2 && context->State == 0) ||
			(context->Op == OP_CURVE4 && context->State != 2) ||
			(context->Op == OP_LINE) || (context->Op == OP_DIMLINE) ||
			(context->Op == OP_BENCH) ) {
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
				coOrd p = pos;
				track_p t;
				if ((t=OnTrack(&p,FALSE,FALSE))!=NULL) {
					EPINX_T ep1,ep2;
					line_angle = GetAngleAtPoint(t,pos,&ep1,&ep2);
					pos = p;
				}
			};
		}
		if ((context->Op == OP_CURVE1 || context->Op == OP_CURVE2 || context->Op == OP_CURVE3 || context->Op == OP_CURVE4) && context->State == 1) {
		;
		} else {
			pos0 = pos;
			pos1 = pos;
		}

		switch (context->Op) {
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
			DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
			switch (context->Op) {
			case OP_LINE: tempSegs(0).type = SEG_STRLIN; break;
			case OP_DIMLINE: tempSegs(0).type = SEG_DIMLIN; break;
			case OP_BENCH: tempSegs(0).type = SEG_BENCH; break;
			}
			tempSegs(0).color = context->Color;
			tempSegs(0).width = width;
			tempSegs(0).u.l.pos[0] = tempSegs(0).u.l.pos[1] = pos;
			if ( context->Op == OP_BENCH || context->Op == OP_DIMLINE ) {
				tempSegs(0).u.l.option = context->benchOption;
			} else {
				tempSegs(0).u.l.option = 0;
			}
			tempSegs_da.cnt = 0;
			context->message( _("Drag to next point, +Shift to lock to object, +Ctrl to lock to 90deg") );
			break;
		case OP_TBLEDGE:
			OnTableEdgeEndPt( NULL, &pos );
			DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
			tempSegs(0).type = SEG_TBLEDGE;
			tempSegs(0).color = context->Color;
			tempSegs(0).width = (mainD.scale<=16)?(3/context->D->dpi*context->D->scale):0;
			tempSegs(0).u.l.pos[0] = tempSegs(0).u.l.pos[1] = pos;
			tempSegs_da.cnt = 0;
			context->message( _("Drag to place next end point") );
			break;
		case OP_CURVE1: case OP_CURVE2: case OP_CURVE3: case OP_CURVE4:
			if (context->State == 0) {
				switch ( context->Op ) {
				case OP_CURVE1: drawGeomCurveMode = crvCmdFromEP1; break;
				case OP_CURVE2: drawGeomCurveMode = crvCmdFromTangent; break;
				case OP_CURVE3: drawGeomCurveMode = crvCmdFromCenter; break;
				case OP_CURVE4: drawGeomCurveMode = crvCmdFromChord; break;
				}
				CreateCurve( C_DOWN, pos, FALSE, context->Color, width, drawGeomCurveMode, context->message );
			}
			break;
		case OP_CIRCLE1:
		case OP_CIRCLE2:
		case OP_CIRCLE3:
		case OP_FILLCIRCLE1:
		case OP_FILLCIRCLE2:
		case OP_FILLCIRCLE3:
			DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
			tempSegs(0).type = SEG_CRVLIN;
			tempSegs(0).color = context->Color;
			if ( context->Op >= OP_CIRCLE1 && context->Op <= OP_CIRCLE3 )
				tempSegs(0).width = width;
			else
				tempSegs(0).width = 0;
			tempSegs(0).u.c.a0 = 0;
			tempSegs(0).u.c.a1 = 360;
			tempSegs(0).u.c.radius = 0;
			tempSegs(0).u.c.center = pos;
			context->message( _("Drag to set radius") );
			break;
		case OP_FILLBOX:
			width = 0;
			/* no break */
		case OP_BOX:
			DYNARR_SET( trkSeg_t, tempSegs_da, 4 );
			for ( inx=0; inx<4; inx++ ) {
				tempSegs(inx).type = SEG_STRLIN;
				tempSegs(inx).color = context->Color;
				tempSegs(inx).width = width;
				tempSegs(inx).u.l.pos[0] = tempSegs(inx).u.l.pos[1] = pos;
			}
			context->message( _("Drag set box size") );
			break;
		case OP_POLY:
		case OP_FILLPOLY:
		case OP_POLYLINE:
			tempSegs_da.cnt = segCnt;
			wBool_t first_spot = FALSE;
			if (segCnt == 1 && tempSegs(0).type == SEG_CRVLIN) {
				coOrd start;
				start = tempSegs(0).u.c.center;
				tempSegs(0).type = SEG_STRLIN;
				tempSegs(0).u.l.pos[0] = start;
				first_spot=TRUE;
			} else {
				DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			}
			segPtr = &tempSegs(tempSegs_da.cnt-1);
			segPtr->type = SEG_STRLIN;
			segPtr->color = context->Color;
			segPtr->width = (context->Op==OP_POLY?width:0);
			//End if over start
			if ( segCnt>2 && IsClose(FindDistance(tempSegs(0).u.l.pos[0], pos ))) {
				segPtr->u.l.pos[0] = tempSegs(segCnt-1).u.l.pos[1];
				segPtr->u.l.pos[1] = tempSegs(0).u.l.pos[0];
				EndPoly(context, tempSegs_da.cnt, context->Op==OP_POLYLINE);
				DYNARR_RESET(pts_t, points_da);
				DYNARR_RESET(trkSeg_t,tempSegs_da);
				context->State = 0;
				segCnt = 0;
				return C_TERMINATE;
			}
			if (!first_spot) {
				if ( tempSegs_da.cnt == 1) {
					segPtr->u.l.pos[0] = pos;
				} else {
					segPtr->u.l.pos[0] = segPtr[-1].u.l.pos[1];
				}
			}
			segPtr->u.l.pos[1] = pos;
			context->State = 1;
			oldOptions = context->D->funcs->options;
			DrawSegs( context->D, zero, 0.0, &tempSegs(tempSegs_da.cnt-1), 1, trackGauge, wDrawColorBlack );
			segCnt = tempSegs_da.cnt;
			context->message(_("+Shift - lock to close object, +Ctrl - lock to 90 deg"));
			break;
		}
		if (anchors_da.cnt)
			DrawSegs( context->D, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );

		return C_CONTINUE;

	case wActionLDrag:
		DYNARR_RESET(trkSeg_t, anchors_da );
		oldOptions = context->D->funcs->options;
		if (context->Op == OP_POLY || context->Op == OP_FILLPOLY || context->Op == OP_POLYLINE)
			DrawSegs( context->D, zero, 0.0, &tempSegs(tempSegs_da.cnt-1), 1, trackGauge, wDrawColorBlack );
		else
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		if ((context->Op == OP_CURVE1 && context->State == 1) ||
			(context->Op == OP_CURVE2 && context->State == 0) ||
			(context->Op == OP_CURVE4 && context->State != 2) ||
			(context->Op == OP_LINE) || (context->Op == OP_DIMLINE) ||
			(context->Op == OP_BENCH) ) {
			if ( (MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
				if (OnTrack( &pos, FALSE, FALSE )!=NULL)
					CreateEndAnchor(pos,TRUE);
			}
		}

		pos1 = pos;

		switch (context->Op) {
		case OP_TBLEDGE:
			OnTableEdgeEndPt( NULL, &pos1 );
			/* no break */
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL ) {
				//Snap to Right-Angle from previous or from 0
				DIST_T l = FindDistance(pos0, pos);
				ANGLE_T angle2 = NormalizeAngle(FindAngle(pos0, pos)-line_angle);
				int quad = (int)(angle2+45.0)/90.0;
				if (tempSegs_da.cnt != 1 && (quad == 2)) {
					pos1 = pos0;
				} else if (quad == 1 || quad == 3) {
					if (tempSegs_da.cnt != 1)
						l = fabs(l*cos(D2R(((quad==1)?line_angle+90.0:line_angle-90.0)-FindAngle(pos,pos0))));
					Translate( &pos1, pos0, NormalizeAngle(quad==1?line_angle+90.0:line_angle-90.0), l );
				} else {
					if (tempSegs_da.cnt != 1)
						l = fabs(l*cos(D2R(((quad==0||quad==4)?line_angle:line_angle+180.0)-FindAngle(pos,pos0))));
					Translate( &pos1, pos0, NormalizeAngle((quad==0||quad==4)?line_angle:line_angle+180.0), l );
				}
				CreateEndAnchor(pos1,TRUE);
			}
			tempSegs(0).u.l.pos[1] = pos1;
			context->message( _("Length = %s, Angle = %0.2f"),
						FormatDistance(FindDistance( pos0, pos1 )),
						PutAngle(FindAngle( pos0, pos1 )) );
			tempSegs_da.cnt = 1;
			if (anchors_da.cnt == 0) CreateEndAnchor(pos, FALSE);
			break;
		case OP_POLY:
		case OP_FILLPOLY:
		case OP_POLYLINE:
			tempSegs_da.cnt = segCnt;
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL ) {
				coOrd last_point;
				ANGLE_T last_angle, initial_angle;
				if (tempSegs_da.cnt == 1) {
					last_angle = 90.0;
					last_point = pos;
					initial_angle = 90.0;
				} else {
					last_point = tempSegs(tempSegs_da.cnt-2).u.l.pos[1];
					last_angle = FindAngle(tempSegs(tempSegs_da.cnt-2).u.l.pos[0],tempSegs(tempSegs_da.cnt-2).u.l.pos[1]);
					initial_angle = FindAngle(tempSegs(0).u.l.pos[0],tempSegs(0).u.l.pos[1]);
				}
				//Snap to Right-Angle from previous or from 0
				DIST_T l = FindDistance(tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos);
				ANGLE_T angle2 = NormalizeAngle(FindAngle(tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos)-last_angle);
				int quad = (int)(angle2+45.0)/90.0;
				if (tempSegs_da.cnt != 1 && (quad == 2)) {
					pos = tempSegs(tempSegs_da.cnt-1).u.l.pos[0];
				} else if (quad == 1 || quad == 3) {
					if (tempSegs_da.cnt != 1)
						l = fabs(l*cos(D2R(((quad==1)?last_angle+90.0:last_angle-90.0)-FindAngle(pos,last_point))));
					Translate( &pos, tempSegs(tempSegs_da.cnt-1).u.l.pos[0], NormalizeAngle(quad==1?last_angle+90.0:last_angle-90.0), l );
				} else {
					if (tempSegs_da.cnt != 1)
						l = fabs(l*cos(D2R(((quad==0||quad==4)?last_angle:last_angle+180.0)-FindAngle(pos,last_point))));
					Translate( &pos, tempSegs(tempSegs_da.cnt-1).u.l.pos[0], NormalizeAngle((quad==0||quad==4)?last_angle:last_angle+180.0), l );
				}
				CreateEndAnchor(pos,TRUE);
				//Show closing 90 degree intersect if close
				//if (tempSegs_da.cnt > 1) {
				//	coOrd intersect;
				//	if (FindIntersection(&intersect,tempSegs(0).u.l.pos[0],initial_angle+90.0,tempSegs(tempSegs_da.cnt-2).u.l.pos[1],last_angle+90.0)) {
				//		CreateSquareAnchor(intersect);
				//		d = FindDistance(intersect,pos);
				//		if (IsClose(d)) {
				//			pos = intersect;
				//		}
				//	}
				//}
			}
			//If there is any point on this line that will give a 90 degree return to the first point, show it
			if (tempSegs_da.cnt > 1) {
				coOrd intersect;
				ANGLE_T an_this = FindAngle(tempSegs(tempSegs_da.cnt-2).u.l.pos[1],pos);
				if (FindIntersection(&intersect,tempSegs(0).u.l.pos[0],an_this+90.0,tempSegs(tempSegs_da.cnt-2).u.l.pos[1],an_this)) {
					ANGLE_T an_inter = FindAngle(tempSegs(tempSegs_da.cnt-2).u.l.pos[1],intersect);
					if (fabs(DifferenceBetweenAngles(an_inter,an_this))<90.0) {
						CreateSquareAnchor(intersect);
						d = FindDistance(intersect,pos);
						if (IsClose(d)) {
							pos = intersect;
						}
					}
				}
			}
			tempSegs(tempSegs_da.cnt-1).type = SEG_STRLIN;
			tempSegs(tempSegs_da.cnt-1).u.l.pos[1] = pos;
			context->message( _("Length = %s, Angle = %0.2f"),
						FormatDistance(FindDistance( tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos )),
						PutAngle(FindAngle( tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos )) );
			segCnt = tempSegs_da.cnt;
			break;
		case OP_CURVE1: case OP_CURVE2: case OP_CURVE3: case OP_CURVE4:
			if (context->State == 0) {
				CreateCurve( C_MOVE, pos, FALSE, context->Color, width, drawGeomCurveMode, context->message );
				pos0x = pos;
			} else {
				PlotCurve( drawGeomCurveMode, pos0, pos0x, pos1, &context->ArcData, FALSE );
				tempSegs(0).color = context->Color;
				tempSegs(0).width = width;
				if (context->ArcData.type == curveTypeStraight) {
					tempSegs(0).type = SEG_STRLIN;
					tempSegs(0).u.l.pos[0] = pos0;
					tempSegs(0).u.l.pos[1] = context->ArcData.pos1;
					tempSegs_da.cnt = 1;
					CreateEndAnchor(pos0, FALSE);
					CreateEndAnchor(context->ArcData.pos1, FALSE);
					context->message( _("Straight Line: Length=%s Angle=%0.3f"),
							FormatDistance(FindDistance( pos0, context->ArcData.pos1 )),
							PutAngle(FindAngle( pos0, context->ArcData.pos1 )) );
				} else if (context->ArcData.type == curveTypeNone) {
					tempSegs_da.cnt = 0;
					context->message( _("Back") );
				} else if (context->ArcData.type == curveTypeCurve) {
					tempSegs(0).type = SEG_CRVLIN;
					tempSegs(0).u.c.center = context->ArcData.curvePos;
					tempSegs(0).u.c.radius = context->ArcData.curveRadius;
					tempSegs(0).u.c.a0 = context->ArcData.a0;
					tempSegs(0).u.c.a1 = context->ArcData.a1;
					tempSegs_da.cnt = 1;
					d = D2R(context->ArcData.a1);
					if (d < 0.0)
						d = 2*M_PI+d;
					if ( d*context->ArcData.curveRadius > mapD.size.x+mapD.size.y ) {
						ErrorMessage( MSG_CURVE_TOO_LARGE );
						tempSegs_da.cnt = 0;
						context->ArcData.type = curveTypeNone;
						context->D->funcs->options = oldOptions;
						return C_CONTINUE;
					}
					context->message( _("Curved Line: Radius=%s Angle=%0.3f Length=%s"),
							FormatDistance(context->ArcData.curveRadius), context->ArcData.a1,
							FormatDistance(context->ArcData.curveRadius*d) );
				}
			}
			if (anchors_da.cnt == 0) CreateEndAnchor(pos, FALSE);
			break;
		case OP_CIRCLE1:
		case OP_FILLCIRCLE1:
			break;
		case OP_CIRCLE2:
		case OP_FILLCIRCLE2:
			tempSegs(0).u.c.center = pos1;
			/* no break */
		case OP_CIRCLE3:
		case OP_FILLCIRCLE3:
			tempSegs(0).u.c.radius = FindDistance( pos0, pos1 );
			context->message( _("Radius = %s"),
						FormatDistance(FindDistance( pos0, pos1 )) );
			break;
		case OP_BOX:
		case OP_FILLBOX:
			tempSegs_da.cnt = 4;
			tempSegs(0).u.l.pos[1].x = tempSegs(1).u.l.pos[0].x = 
			tempSegs(1).u.l.pos[1].x = tempSegs(2).u.l.pos[0].x = pos.x;
			tempSegs(1).u.l.pos[1].y = tempSegs(2).u.l.pos[0].y = 
			tempSegs(2).u.l.pos[1].y = tempSegs(3).u.l.pos[0].y = pos.y;
			context->message( _("Width = %s, Height = %s"),
						FormatDistance(fabs(pos1.x - pos0.x)), FormatDistance(fabs(pos1.y - pos0.y)) );
			break;
		}
		context->D->funcs->options |= wDrawOptTemp;
		if (context->Op == OP_POLY || context->Op == OP_FILLPOLY || context->Op == OP_POLYLINE)
			DrawSegs( context->D, zero, 0.0, &tempSegs(tempSegs_da.cnt-1), 1, trackGauge, wDrawColorBlack );
		else
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		if (anchors_da.cnt)
			DrawSegs(context->D, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		context->D->funcs->options = oldOptions;
		//if (context->Op == OP_DIMLINE)
		//      MainRedraw();   //Wipe Out Text
		return C_CONTINUE;

	case wActionLUp:
		DYNARR_RESET(trkSeg_t, anchors_da );
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		if (context->Op != OP_POLY && context->Op != OP_FILLPOLY && context->Op != OP_POLYLINE)
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		lastValid = FALSE;
		createTrack = FALSE;
		if ((context->Op == OP_CURVE1 && context->State == 1) ||
			(context->Op == OP_CURVE2 && context->State == 0) ||
			(context->Op == OP_CURVE4 && context->State != 2) ||
			(context->Op == OP_LINE) || (context->Op == OP_DIMLINE) ||
			(context->Op == OP_BENCH) ) {
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
				coOrd p = pos1;
				track_p t;
				if ((t=OnTrack(&p,FALSE,FALSE))) {
					pos1 = p;
					if (context->Op == OP_LINE || context->Op == OP_DIMLINE ||  context->Op == OP_BENCH) {
						tempSegs(0).u.l.pos[1] = p;
					} else {
						PlotCurve( drawGeomCurveMode, pos0, pos0x, pos1, &context->ArcData, FALSE );
						if (context->ArcData.type == curveTypeStraight) {
							CleanSegs(&tempSegs_da);
							tempSegs(0).type = SEG_STRLIN;
							tempSegs(0).u.l.pos[0] = pos0;
							tempSegs(0).u.l.pos[1] = context->ArcData.pos1;
							tempSegs_da.cnt = 1;
						} else if (context->ArcData.type == curveTypeNone) {
							CleanSegs(&tempSegs_da);
						} else if (context->ArcData.type == curveTypeCurve) {
							CleanSegs(&tempSegs_da);
							tempSegs(0).type = SEG_CRVLIN;
							tempSegs(0).u.c.center = context->ArcData.curvePos;
							tempSegs(0).u.c.radius = context->ArcData.curveRadius;
							tempSegs(0).u.c.a0 = context->ArcData.a0;
							tempSegs(0).u.c.a1 = context->ArcData.a1;
							tempSegs_da.cnt = 1;
						}
					}

				}
			}
		}
		switch ( context->Op ) {
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
		case OP_TBLEDGE:
			lastValid = TRUE;
			lastPos = pos1;
			context->length = FindDistance(pos1,pos0);
			context->angle = FindAngle(pos0,pos1);
			context->State = 2;
			segCnt = tempSegs_da.cnt;
			break;
		case OP_CURVE1: case OP_CURVE2: case OP_CURVE3: case OP_CURVE4:
			if (context->State == 0) {
				context->State = 1;
				context->ArcAngle = FindAngle( pos0, pos1 );
				pos0x = pos1;
				CreateCurve( C_UP, pos, FALSE, context->Color, width, drawGeomCurveMode, context->message );
				DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
				context->message( _("Drag on Red arrows to adjust curve") );
				return C_CONTINUE;
			} else {
				CleanSegs(&tempSegs_da);
				if (context->ArcData.type == curveTypeCurve) {
					tempSegs_da.cnt = 1;
					segPtr = &tempSegs(0);
					segPtr->type = SEG_CRVLIN;
					segPtr->color = context->Color;
					segPtr->width = width;
					segPtr->u.c.center = context->ArcData.curvePos;
					segPtr->u.c.radius = context->ArcData.curveRadius;
					segPtr->u.c.a0 = context->ArcData.a0;
					segPtr->u.c.a1 = context->ArcData.a1;
					context->radius = context->ArcData.curveRadius;
					context->angle = context->ArcData.a1;
				} else if (context->ArcData.type == curveTypeStraight) {
					tempSegs_da.cnt = 1;
					segPtr = &tempSegs(0);
					segPtr->type = SEG_STRLIN;
					segPtr->color = context->Color;
					segPtr->width = width;
					segPtr->u.l.pos[0] = pos0;
					segPtr->u.l.pos[1] = pos1;
					context->radius = 0;
					context->length = FindDistance(pos0,pos1);
					context->angle = FindAngle(pos0,pos1);
				} else {
					tempSegs_da.cnt = 0;
				}
				lastValid = TRUE;
				lastPos = pos1;
				context->State = 2;
				/*drawContext = context;
				DrawGeomOp( (void*)context->Op );*/
			}
			break;
		case OP_CIRCLE1:
		case OP_CIRCLE2:
		case OP_CIRCLE3:
		case OP_FILLCIRCLE1:
		case OP_FILLCIRCLE2:
		case OP_FILLCIRCLE3:
			if ( context->Op>=OP_FILLCIRCLE1 && context->Op<=OP_FILLCIRCLE3 )
				tempSegs(0).type = SEG_FILCRCL;
			tempSegs_da.cnt = 1;
			context->State = 2;
			context->radius = tempSegs(0).u.c.radius;
			break;
		case OP_BOX:
		case OP_FILLBOX:
			pts = (pts_t*)MyMalloc( 4 * sizeof (pts_t) );
			for ( inx=0; inx<4; inx++ ) {
				pts[inx].pt = tempSegs(inx).u.l.pos[0];
				pts[inx].pt_type = 0;
			}
			tempSegs(0).type = (context->Op == OP_FILLBOX)?SEG_FILPOLY:SEG_POLY;
			tempSegs(0).u.p.cnt = 4;
			tempSegs(0).u.p.pts = pts;
			tempSegs(0).u.p.angle = 0.0;
			tempSegs(0).u.p.orig = zero;
			tempSegs(0).u.p.polyType = RECTANGLE;
			tempSegs_da.cnt = 1;
			/*drawContext = context;
			DrawGeomOp( (void*)context->Op );*/
			context->length = FindDistance(pts[0].pt,pts[1].pt);
			context->width = FindDistance(pts[3].pt,pts[0].pt);
			context->State = 2;
			segCnt = tempSegs_da.cnt;
			break;
		case OP_POLY:
		case OP_FILLPOLY:
		case OP_POLYLINE:
			tempSegs_da.cnt = segCnt;
			anchors_da.cnt=0;
			//End if close to start
			if ( segCnt>2 && IsClose(FindDistance(tempSegs(0).u.l.pos[0], pos))) {
				EndPoly(context, tempSegs_da.cnt, context->Op==OP_POLYLINE);
				DYNARR_RESET(pts_t, points_da);
				CleanSegs(&tempSegs_da);
				context->State = 0;
				segCnt = 0;
				return C_TERMINATE;
			}
			//If too short, remove last segment
			if (IsClose(FindDistance(tempSegs(segCnt-1).u.l.pos[0],pos))) {
				if (tempSegs_da.cnt>1) {
					--tempSegs_da.cnt;
					segCnt = tempSegs_da.cnt;
					wBeep();
				} else {   //First spot only
					tempSegs(0).color = wDrawColorRed;
					tempSegs(0).type = SEG_CRVLIN;
					tempSegs(0).u.c.a1 = 360;
					tempSegs(0).u.c.radius = tempD.scale*0.15/2;
					tempSegs(0).u.c.center = pos;
					segCnt = tempSegs_da.cnt;
				}
				return C_CONTINUE;
			}
			int text_inx = tempSegs_da.cnt-1;
			//tempSegs(tempSegs_da.cnt-1).u.l.pos[1] = pos;
			context->length = FindDistance(tempSegs(text_inx).u.l.pos[0],tempSegs(text_inx).u.l.pos[1]);
			if (text_inx>1) {
				ANGLE_T an = FindAngle(tempSegs(text_inx-1).u.l.pos[0],tempSegs(text_inx-1).u.l.pos[1]);
				context->angle = NormalizeAngle(FindAngle(tempSegs(text_inx).u.l.pos[0],tempSegs(text_inx).u.l.pos[1])-an);
			} else
				context->angle = FindAngle(tempSegs(1).u.l.pos[0],tempSegs(1).u.l.pos[1]);
			context->State = 1;
			context->index = text_inx;
			segCnt = tempSegs_da.cnt;
			return C_CONTINUE;
		}
		context->Started = FALSE;
		context->Changed = TRUE;					//Update screen shown
		/*CheckOk();*/
		if (context->State == 2 && IsCurCommandSticky()) {
			segCnt = tempSegs_da.cnt;
			MainRedraw();
			return C_CONTINUE;
		}
		context->D->funcs->options = oldOptions;
		DrawGeomOk();
		context->State = 0;
		context->Changed = FALSE;
		context->message("");
		return C_TERMINATE;

	case wActionText:
		DYNARR_RESET(trkSeg_t, anchors_da );
		if ( ((action>>8)&0xFF) == 0x0D ||
			 ((action>>8)&0xFF) == ' ' ) {
			if ((context->Op == OP_POLY) || (context->Op == OP_FILLPOLY) || (context->Op == OP_POLYLINE)) {
				tempSegs_da.cnt = segCnt;
				//If last segment wasn't just a point, add another starting on its end
				if (!IsClose(FindDistance(tempSegs(segCnt-1).u.l.pos[0],tempSegs(segCnt-1).u.l.pos[1]))) {
					DYNARR_APPEND(trkSeg_t,tempSegs_da,1);
					segPtr = &tempSegs(tempSegs_da.cnt-1);
					segPtr->type = SEG_STRLIN;
					segPtr->u.l.pos[0] = segPtr[-1].u.l.pos[1];
					segPtr->u.l.pos[1] = tempSegs(0).u.l.pos[0];
				}
                EndPoly(context, tempSegs_da.cnt, context->Op == OP_POLYLINE);
                DYNARR_RESET(pts_t, points_da);
				DYNARR_RESET(trkSeg_t,tempSegs_da);
			} else {
				if (context->State == 2)
					tempSegs_da.cnt = segCnt;
				DrawGeomOk();
			}
		}
		context->State = 0;
		segCnt = 0;
		return C_TERMINATE;

	case C_CANCEL:
		if (context->Changed) {				//If the update values were shown
			if (context->State == 2) {
				tempSegs_da.cnt = segCnt;
				DrawGeomOk();
			}
		}
		DYNARR_RESET(trkSeg_t, anchors_da );
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		context->D->funcs->options = oldOptions;
		tempSegs_da.cnt = 0;
		context->message( "" );
		context->Changed = FALSE;
		context->State = 0;
		segCnt = 0;
		lastValid = FALSE;
		return C_TERMINATE;

	case C_REDRAW:
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		if (context->State !=0) {
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		}
		if (anchors_da.cnt > 0) {
			DrawSegs(context->D, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		}
		context->D->funcs->options = oldOptions;
		return C_CONTINUE;
		
	case C_CMDMENU:

		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}
static int polyInx;
static int lineInx;
static int curveInx;

typedef enum {POLY_NONE, POLY_SELECTED, POLYPOINT_SELECTED} PolyState_e;
static PolyState_e polyState = POLY_NONE;
static coOrd rotate_origin;
static ANGLE_T rotate_angle;
static dynArr_t origin_da;


void static CreateCircleAnchor(wBool_t selected,coOrd center, DIST_T rad) {
	DYNARR_RESET(trkSeg_t,anchors_da);
	double d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,2);
	anchors(0).type = (selected)?SEG_FILCRCL:SEG_CRVLIN;
	anchors(0).u.c.a1 = 360.0;
	anchors(0).color = wDrawColorBlue;
	anchors(0).u.c.radius = d/2;
	PointOnCircle(&anchors(0).u.c.center,center,rad,315.0);
}

void static CreateLineAnchors(int index, coOrd p0, coOrd p1) {
	DYNARR_RESET(trkSeg_t,anchors_da);
	double d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,2);
	anchors(0).type = (index ==0)?SEG_FILCRCL:SEG_CRVLIN;
	anchors(0).u.c.a1 = 360.0;
	anchors(0).color = wDrawColorBlue;
	anchors(0).u.c.radius = d/2;
	anchors(0).u.c.center = p0;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	anchors(1).type = (index ==1)?SEG_FILCRCL:SEG_CRVLIN;
	anchors(1).u.c.a1 = 360.0;
	anchors(1).color = wDrawColorBlue;
	anchors(1).u.c.radius = d/2;
	anchors(1).u.c.center = p1;
}
void static CreateBoxAnchors(int index, pts_t pt[4]) {
	DYNARR_RESET(trkSeg_t,anchors_da);
	double d = tempD.scale*0.15;
	ANGLE_T a = FindAngle(pt[0].pt,pt[1].pt);
	for (int i=0;i<4;i++) {
		DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
		DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pt[i].pt,45.0+a+(90.0*(i)),TRUE,i==index?wDrawColorRed:wDrawColorBlue);
	}
	coOrd pp;
	for (int i=0;i<4;i++) {
		pp.x = (i==3?((((pt[0].pt.x - pt[i].pt.x)/2))+pt[i].pt.x):((pt[i+1].pt.x - pt[i].pt.x)/2)+pt[i].pt.x);
		pp.y = (i==3?((((pt[0].pt.y - pt[i].pt.y)/2))+pt[i].pt.y):((pt[i+1].pt.y - pt[i].pt.y)/2)+pt[i].pt.y);

		DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
		DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pp,90.0*(i-1)+a,TRUE,i==index?wDrawColorRed:wDrawColorBlue);
	}
}

void static CreateOriginAnchor(coOrd origin, wBool_t trans_selected) {
	double d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,2);
	int i = anchors_da.cnt-1;
	coOrd p0,p1;
	Translate(&p0,origin,0,d*4);
	Translate(&p1,origin,0,-d*4);
	anchors(i).type = SEG_STRLIN;
	anchors(i).u.l.pos[0] = p0;
	anchors(i).u.l.pos[1] = p1;
	anchors(i).color = wDrawColorBlue;
	anchors(i).width = 0;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	Translate(&p0,origin,90,d*4);
	Translate(&p1,origin,90,-d*4);
	i = anchors_da.cnt-1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).u.l.pos[0] = p0;
	anchors(i).u.l.pos[1] = p1;
	anchors(i).color = wDrawColorBlue;
	anchors(i).width = 0;
}

void static CreateCurveAnchors(int index, coOrd pm, coOrd pc, coOrd p0, coOrd p1) {
	DYNARR_RESET(trkSeg_t,anchors_da);
	double d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,9);
	anchors(0).type = (index ==0)?SEG_FILCRCL:SEG_CRVLIN;
	anchors(0).u.c.a1 = 360.0;
	anchors(0).color = wDrawColorBlue;
	anchors(0).u.c.radius = d/2;
	anchors(0).u.c.center = p0;
	anchors(0).width = 0;
	DYNARR_APPEND(trkSeg_t,anchors_da,8);
	anchors(1).type = (index ==1)?SEG_FILCRCL:SEG_CRVLIN;
	anchors(1).u.c.a1 = 360.0;
	anchors(1).color = wDrawColorBlue;
	anchors(1).u.c.radius = d/2;
	anchors(1).u.c.center = p1;
	anchors(1).width = 0;
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pm,FindAngle(pm,pc),TRUE,index==2?wDrawColorAqua:wDrawColorBlue);
}

void static CreatePolyAnchors(int index) {
		DYNARR_RESET(trkSeg_t,anchors_da);
		double d = tempD.scale*0.15;
		coOrd p;
		for ( int inx=0; inx<points_da.cnt; inx++ ) {
			DYNARR_APPEND(trkSeg_t,anchors_da,3);
			anchors(inx).type = point_selected(inx)?SEG_FILCRCL:SEG_CRVLIN;
			anchors(inx).u.c.a0 = 0.0;
			anchors(inx).u.c.a1 = 360.0;
			anchors(inx).color = wDrawColorBlue;
			anchors(inx).u.c.radius = d/2;
			anchors(inx).u.c.center = points(inx).pt;
		}
		if (index>=0) {
			DYNARR_APPEND(trkSeg_t,anchors_da,1);
			int inx = anchors_da.cnt-1;
			anchors(inx).type = SEG_STRLIN;
			anchors(inx).u.l.pos[0] = points(index==0?points_da.cnt-1:index-1).pt;
			anchors(inx).u.l.pos[1] = points(index).pt;
			anchors(inx).color = wDrawColorBlue;
			anchors(inx).width = 0;
			DYNARR_APPEND(trkSeg_t,anchors_da,1);
			inx = anchors_da.cnt-1;
			int index0 = index==0?points_da.cnt-1:index-1;
			ANGLE_T an0 = FindAngle(points(index0).pt, points(index).pt);
			ANGLE_T an1 = FindAngle(points(index0==0?points_da.cnt-1:index0-1).pt, points(index0).pt);
			anchors(inx).type = SEG_CRVLIN;
			if (DifferenceBetweenAngles(an0,an1)<=0) {
				anchors(inx).u.c.a1 = DifferenceBetweenAngles(an0,an1)-180;
				anchors(inx).u.c.a0 = an0;
			} else {
				anchors(inx).u.c.a1 = 180-DifferenceBetweenAngles(an0,an1);
				anchors(inx).u.c.a0 = NormalizeAngle(180+an1);
			}
			anchors(inx).color = wDrawColorBlue;
			anchors(inx).u.c.radius = d;
			anchors(inx).u.c.center = points(index0).pt;
		}
}

/*
 * Modify Polygons. Polygons have a variable number of nodes.
 *
 * Each point has an anchor and selecting the node allows it to be moved
 * Selecting a point between nodes adds a node ready for dragging
 * The last selected node can be deleted
 *
 */
STATUS_T DrawGeomPolyModify(
				wAction_t action,
				coOrd pos,
				drawModContext_t *context) {
	double d;
	static int selected_count;
	static int segInx;
	static int prev_inx;
	static wDrawColor save_color;
	int prior_pnt, next_pnt, orig_pnt;
	ANGLE_T prior_angle, next_angle, line_angle;
	static wBool_t drawnAngle;
	static double currentAngle;
	static double baseAngle;
	static BOOL_T lock;

	switch ( action&0xFF ) {
		case C_START:
			lock = FALSE;
			DistanceSegs( context->orig, context->angle, context->segCnt, context->segPtr, &pos, &segInx );
			if (segInx == -1)
				return C_ERROR;
			if (context->type != SEG_POLY && context->type != SEG_FILPOLY)
				return C_ERROR;
			prev_inx = -1;
			polyState = POLY_SELECTED;
			polyInx = -1;
			//Copy points
			DYNARR_RESET( pts_t, points_da);
			DYNARR_RESET( wBool_t, select_da);
			for (int inx=0;inx<context->segPtr->u.p.cnt;inx++) {
				DYNARR_APPEND(pts_t, points_da,3);
				DYNARR_APPEND(wBool_t,select_da,3);
				REORIGIN( points(inx).pt, context->segPtr[segInx].u.p.pts[inx].pt, context->angle, context->orig );
				points(inx).pt_type = context->segPtr[segInx].u.p.pts[inx].pt_type;
				point_selected(inx) = FALSE;
			}
			context->max_inx = points_da.cnt-1;
			selected_count=0;
			rotate_origin = context->orig;
			rotate_angle = context->angle;
			context->p0 = points(0).pt;
			context->p1 = points(1).pt;
			//Show points
			tempSegs_da.cnt = 1;
			tempSegs(0).width = context->segPtr->width;
			save_color = context->segPtr->color;
			tempSegs(0).color = wDrawColorRed;
			tempSegs(0).type = context->type;
			tempSegs(0).u.p.cnt = context->segPtr[segInx].u.p.cnt;
			tempSegs(0).u.p.angle = 0.0;
			tempSegs(0).u.p.orig = zero;
			tempSegs(0).u.p.polyType = context->segPtr[segInx].u.p.polyType;
			tempSegs(0).u.p.pts = &points(0);
			CreatePolyAnchors( -1);
			InfoMessage(_("Select Points, or use Context Menu"));
			//MainRedraw();
			return C_CONTINUE;
		case C_DOWN:
			d = 10000.0;
			polyInx = -1;
			coOrd p0;
			double dd;
			int inx;
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) != WKEY_SHIFT) {
				if (selected_count <2 ) {
					//Wipe out selection(s) if we don't have multiple already (i,e. move with >1 selected)
					for (int i=0;i<points_da.cnt;i++) {
						point_selected(i) = FALSE;
					}
					selected_count = 0;
				} else {
					for (int i=0;i<points_da.cnt;i++) {
						if (IsClose(FindDistance(pos,points(i).pt)) && point_selected(i)==TRUE) {
							point_selected(i) = FALSE;
							selected_count--;
						}
					}
				}
			}
			//Select Point (polyInx)
			for ( int inx=0; inx<points_da.cnt; inx++ ) {
				p0 = pos;
				dd = LineDistance( &p0, points( inx==0?points_da.cnt-1:inx-1).pt, points(inx).pt );
				if ( d > dd ) {
					d = dd;
					polyInx = inx;
				}
			}
			if (!IsClose(d)) {	//Not on/near object - de-select all points
				for (int i=0;i<points_da.cnt;i++) {
					point_selected(i) = FALSE;
				}
				polyInx = -1;
				selected_count = 0;
				CreatePolyAnchors( -1);
				MainRedraw();
				return C_CONTINUE; //Not close to any line
			}
			polyState = POLYPOINT_SELECTED;
			inx = polyInx==0?points_da.cnt-1:polyInx-1;
			//Find if the point is to be added
			d = FindDistance( points(inx).pt, pos );
			dd = FindDistance( points(inx).pt, points(polyInx).pt );
			if ( d < 0.25*dd ) {
				polyInx = inx;
			} else if ( d > 0.75*dd ) {
				;
			} else {
				if (selected_count == 0) {					//Only add a new point if no points are already selected!
					DYNARR_APPEND(wBool_t,select_da,1);
					for (int i=0;i<select_da.cnt;i++) {
						if (i == polyInx) point_selected(i) = TRUE;
						else point_selected(i) = FALSE;
					}
					selected_count=1;
					DYNARR_APPEND(pts_t,points_da,1);
					tempSegs(0).u.p.pts = &points(0);
					for (inx=points_da.cnt-1; inx>polyInx; inx-- ) {
						points(inx) = points(inx-1);
					}
					points(polyInx).pt_type = 0;
					tempSegs(0).u.p.cnt = points_da.cnt;
					context->max_inx = points_da.cnt-1;
				}
			}
			//If already selected (multiple points), not using shift (to add) select, and on object move to first point
			if (selected_count>0 && ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) != WKEY_SHIFT)) {
				for (int i=0; i<points_da.cnt;i++) {
					if (IsClose(FindDistance(pos,points(i).pt))) {

						point_selected(i) = FALSE;

					}
					if (point_selected(i) == TRUE) {
						polyInx = i;
					}
				}
			}
			pos = points(polyInx).pt;  		//Move to point
			if (point_selected(polyInx)) {					//Already selected
			} else {
				point_selected(polyInx) = TRUE;
				++selected_count;
			}
			//Work out before and after point
			int first_inx = -1;
			if (selected_count >0 && selected_count < points_da.cnt-2) {
				for (int i=0; i<points_da.cnt;i++) {
					if (point_selected(i)) {
						first_inx = i;
						break;
					}
				}
			}
			int last_inx = -1, next_inx = -1;
			ANGLE_T an1, an0;
			if (first_inx >=0) {
				if (first_inx == 0) {
					last_inx = points_da.cnt-1;
				} else {
					last_inx = first_inx-1;
				}
				if (first_inx == points_da.cnt-1) {
					next_inx = 0;
				} else {
					next_inx = first_inx+1;
				}
				context->length = FindDistance(points(last_inx).pt,points(first_inx).pt);
				an1 = FindAngle(points(last_inx).pt,points(first_inx).pt);
				an0 = FindAngle(points(last_inx==0?(points_da.cnt-1):(last_inx-1)).pt,points(last_inx).pt);
				if (DifferenceBetweenAngles(an0,an1)<=0) {
					context->rel_angle = 180+DifferenceBetweenAngles(an0,an1);
				} else {
					context->rel_angle = 180-DifferenceBetweenAngles(an0,an1);
				}
			} else {

			}
			context->prev_inx = first_inx;
			context->p0 = points(0).pt;
			context->p1 = points(1).pt;
			//Show three anchors only
			CreatePolyAnchors(first_inx);
			MainRedraw();
			return C_CONTINUE;
		case C_LDOUBLE:
			return C_CONTINUE;
		case C_MOVE:
			tempSegs_da.cnt = 1;
			if (polyState != POLYPOINT_SELECTED) {
				return C_CONTINUE;
			}
			//Moving with Point Selected
			if (polyInx<0) return C_ERROR;
			first_inx = -1;
			if (selected_count >0 && selected_count < points_da.cnt-2) {
				for (int i=0; i<points_da.cnt;i++) {
					if (point_selected(i)) {
						first_inx = i;
						break;
					}
				}
			}
			last_inx = -1;
			next_inx = -1;
			coOrd intersect;
			wBool_t show_intersect = FALSE;
			if (first_inx >=0) {
				if (first_inx == 0) {
					last_inx = points_da.cnt-1;
				} else {
					last_inx = first_inx-1;
				}
				if (first_inx == points_da.cnt-1) {
					next_inx = 0;
				} else {
					next_inx = first_inx+1;
				}
				//Lock to 90 degrees first/last point
				if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
					ANGLE_T last_angle,next_angle;
					coOrd last_point,next_point;
					if (first_inx == 0) {
						last_point = points(points_da.cnt-1).pt;
						last_angle = FindAngle(points(points_da.cnt-2).pt,last_point);
					} else if (first_inx == 1) {
						last_point = points(0).pt;
						last_angle = FindAngle(points(points_da.cnt-1).pt,last_point);
					} else {
						last_point = points(first_inx-1).pt;
						last_angle = FindAngle(points(first_inx-2).pt,last_point);
					}
					if (first_inx == points_da.cnt-1) {
						next_point = points(0).pt;
						next_angle = FindAngle(next_point,points(1).pt);
					} else if (first_inx == points_da.cnt-2){
						next_point = points(points_da.cnt-1).pt;
						next_angle = FindAngle(next_point,points(0).pt);
					} else {
						next_point = points(first_inx+1).pt;
						next_angle = FindAngle(next_point,points(first_inx+2).pt);
					}
					coOrd diff;
					diff.x = pos.x - points(polyInx).pt.x;
					diff.y = pos.y - points(polyInx).pt.y;
					coOrd pos_lock = points(first_inx).pt;
					pos_lock.x += diff.x;
					pos_lock.y += diff.y;
					//Snap to Right-Angle from previous or from 0
					DIST_T l = FindDistance(last_point, pos_lock);
					ANGLE_T angle2 = NormalizeAngle(FindAngle(last_point, pos_lock)-last_angle);
					int quad = (int)(angle2+45.0)/90.0;
					if (tempSegs_da.cnt != 1 && (quad == 2)) {
						pos_lock = last_point;
					} else if (quad == 1 || quad == 3) {
						l = fabs(l*cos(D2R(((quad==1)?last_angle+90.0:last_angle-90.0)-FindAngle(pos_lock,last_point))));
						Translate( &pos_lock, last_point, NormalizeAngle((quad==1)?last_angle+90.0:last_angle-90.0), l );
					} else {
						l = fabs(l*cos(D2R(((quad==0||quad==4)?last_angle:last_angle+180.0)-FindAngle(pos_lock,last_point))));
						Translate( &pos_lock, last_point, NormalizeAngle((quad==0||quad==4)?last_angle:last_angle+180.0), l );
					}
					diff.x = pos_lock.x - points(first_inx).pt.x;
					diff.y = pos_lock.y - points(first_inx).pt.y;
					pos.x 	= points(polyInx).pt.x+diff.x;
					pos.y 	= points(polyInx).pt.y+diff.y;
					if (selected_count<2) {
						if (FindIntersection(&intersect,last_point,last_angle+90.0,next_point,last_angle+180.0)) {
							show_intersect = TRUE;
						}
					}
					d = FindDistance(intersect,pos_lock);
					if (IsClose(d)) {
						pos = intersect;
					}
					InfoMessage( _("Length = %s, Last_Angle = %0.2f"),
							FormatDistance(FindDistance(pos_lock,last_point)),
							PutAngle(FindAngle(pos_lock,last_point)));

				}
			}
			context->prev_inx = first_inx;
			coOrd diff;
			diff.x = pos.x - points(polyInx).pt.x;
			diff.y = pos.y - points(polyInx).pt.y;
			//points(polyInx) = pos;
			for (int i=0;i<points_da.cnt;i++) {
				if (point_selected(i)) {
					points(i).pt.x = points(i).pt.x + diff.x;
					points(i).pt.y = points(i).pt.y + diff.y;
				}
			}
			if (first_inx>=0) {
				context->length = FindDistance(points(first_inx).pt,points(last_inx).pt);
				an1 = FindAngle(points(last_inx).pt,points(first_inx).pt);
				an0 = FindAngle(points(first_inx==0?(points_da.cnt-1):(first_inx-1)).pt,points(first_inx).pt);
				context->rel_angle = NormalizeAngle(180-(an1-an0));
			}
			CreatePolyAnchors(first_inx);
			if (show_intersect)
				CreateSquareAnchor(intersect);
			context->p0 = points(0).pt;
			context->p1 = points(1).pt;
			MainRedraw();
			return C_CONTINUE;
		case C_UP:
			context->prev_inx = -1;
			if (segInx == -1 || polyState != POLYPOINT_SELECTED)
				return C_CONTINUE;   //Didn't get a point selected/added
			polyState = POLY_SELECTED;  //Return to base state
			anchors_da.cnt = 0;
			CreatePolyAnchors(polyInx);  //Show last selection
			prev_inx = polyInx;
			for (int i=0;i<points_da.cnt;i++) {
				if (point_selected(i)) {
					first_inx = i;
					break;
				}
			}
			last_inx = first_inx==0?(points_da.cnt-1):(first_inx-1);
			if (first_inx>=0) {
				context->length = FindDistance(points(first_inx).pt,points(last_inx).pt);
				an1 = FindAngle(points(last_inx).pt,points(first_inx).pt);
				an0 = FindAngle(points(last_inx==0?(points_da.cnt-1):(last_inx-1)).pt,points(last_inx).pt);
				if (DifferenceBetweenAngles(an0,an1)<=0) {
					context->rel_angle = 180+DifferenceBetweenAngles(an0,an1);
				} else {
					context->rel_angle = 180-DifferenceBetweenAngles(an0,an1);
				}
			}
			context->prev_inx = first_inx;
			context->p0 = points(0).pt;
			context->p1 = points(1).pt;
			polyInx = -1;
			MainRedraw();
			return C_CONTINUE;
		case C_UPDATE:
			if (context->prev_inx>=0) {
				int last_index = context->prev_inx==0?(points_da.cnt-1):(context->prev_inx-1);
				an0 = FindAngle(points(last_index==0?(points_da.cnt-1):(last_index-1)).pt,points(last_index).pt);
				an1 = FindAngle(points(last_index).pt,points(context->prev_inx).pt);
				if (DifferenceBetweenAngles(an0,an1)<=0) {
					an1 = NormalizeAngle(an0-(180-context->rel_angle));
				} else {
					an1 = NormalizeAngle((180-context->rel_angle)+an0);
				}
				Translate(&points(prev_inx).pt,points(last_index).pt,an1,context->length);
			}
			context->rel_angle = fabs(context->rel_angle);
			if (context->rel_angle >180) context->rel_angle = context->rel_angle - 180.0;
			CreatePolyAnchors(prev_inx);
			context->p0 = points(0).pt;
			context->p1 = points(1).pt;
			MainRedraw();
			break;
		case C_TEXT:
			if (action>>8 == 'o') {  //"o" -> origin mode
				MenuMode(1);
				InfoMessage(_("Move Origin Mode"));
				MainRedraw();
				return C_CONTINUE;
			}
			if (((prev_inx>=0 && tempSegs(0).u.p.polyType != POLYLINE) || (prev_inx>=1 && prev_inx<=points_da.cnt-2)) &&
					((action>>8 == 's') || (action>>8 == 'v') || (action>>8 == 'r')))  {
				switch(action>>8) {
				case 's':
					points(context->prev_inx).pt_type = 1;
					break;
				case 'v':
					points(context->prev_inx).pt_type = 0;
					break;
				case 'r':
					points(context->prev_inx).pt_type = 2;
					break;
				default:
					return C_CONTINUE;
				}
				MainRedraw();
				return C_CONTINUE;
			}
			if ((action>>8 == 'c') && (tempSegs(0).type == SEG_POLY) && (tempSegs(0).u.p.polyType == POLYLINE) ) {
				tempSegs(0).u.p.polyType = FREEFORM;
				context->subtype=FREEFORM;
				context->open = FALSE;
				MainRedraw();
				return C_CONTINUE;
			}
			if ((action>>8 == 'l') && (tempSegs(0).type == SEG_POLY) && (tempSegs(0).u.p.polyType == FREEFORM)) {
				tempSegs(0).u.p.polyType = POLYLINE;
				context->subtype=POLYLINE;
				context->open = TRUE;
				MainRedraw();
				return C_CONTINUE;
			}
			if ((action>>8 == 'f') && (tempSegs(0).type == SEG_POLY) && (tempSegs(0).u.p.polyType != POLYLINE )) {
				tempSegs(0).type = SEG_FILPOLY;
				context->type =  SEG_FILPOLY;
				context->filled = TRUE;
				MainRedraw();
				return C_CONTINUE;
			}
			if ((action>>8 == 'e') && (tempSegs(0).type == SEG_FILPOLY) ) {
				tempSegs(0).type = SEG_POLY;
				context->type =  SEG_POLY;
				context->filled = FALSE;
				MainRedraw();
				return C_CONTINUE;
			}
			//Delete or backspace deletes last selected index
			if (action>>8 == 127 || action>>8 == 8) {
				if (polyState == POLY_SELECTED && prev_inx >=0) {
					if (selected_count >1) {
						ErrorMessage( MSG_POLY_MULTIPLE_SELECTED );
						return C_CONTINUE;
					}
					if (points_da.cnt <= 3) {
						ErrorMessage( MSG_POLY_SHAPES_3_SIDES );
						return C_CONTINUE;
					}
					for (int i=0;i<points_da.cnt;i++) {
						point_selected(i) = FALSE;
						if (i>=prev_inx && i<points_da.cnt-1)
							points(i) = points(i+1);
					}
					points_da.cnt--;
					select_da.cnt--;
					selected_count=0;
					tempSegs(0).u.p.cnt = points_da.cnt;
					context->max_inx = points_da.cnt-1;
				}
				prev_inx = -1;
				context->prev_inx = -1;
				polyInx = -1;
				polyState = POLY_SELECTED;
				CreatePolyAnchors( -1);
				InfoMessage(_("Point Deleted"));
				MainRedraw();
				return C_CONTINUE;
			}
			if (action>>8 != 32 && action>>8 != 13) return C_CONTINUE;
			/* no break */
		case C_FINISH:
			//copy changes back into track
			if (polyState != POLY_SELECTED) return C_TERMINATE;
			pts_t * oldPts = context->segPtr[segInx].u.p.pts;
			void * newPts = (pts_t*)MyMalloc( points_da.cnt * sizeof (pts_t) );
			context->segPtr[segInx].u.p.pts = newPts;
			context->segPtr->u.p.cnt = points_da.cnt;
			context->orig = rotate_origin;
			context->angle = rotate_angle;
			for (int i=0; i<points_da.cnt; i++) {
				pos = points(i).pt;
				pos.x -= context->orig.x;
				pos.y -= context->orig.y;
				Rotate( &pos, zero, -context->angle );
				context->segPtr[segInx].u.p.pts[i].pt = pos;
				context->segPtr[segInx].u.p.pts[i].pt_type = points(i).pt_type;
			}
			MyFree(oldPts);
			oldPts = NULL;
			polyState = POLY_NONE;
			DYNARR_RESET(trkSeg_t,anchors_da);
			DYNARR_RESET(trkSeg_t,tempSegs_da);
			return C_TERMINATE;
		case C_REDRAW:
			if (polyState == POLY_NONE) return C_CONTINUE;
			DrawTrack(context->trk, &mainD, wDrawColorWhite);
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt,trackGauge, wDrawColorBlack);
			DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			break;
		default:
			;
		}
		return C_CONTINUE;
}

void BuildCircleContext(drawModContext_t * context,int segInx) {
	context->radius = fabs(context->segPtr[segInx].u.c.radius);
	REORIGIN( context->pc, context->segPtr[segInx].u.c.center, context->angle, context->orig );
	PointOnCircle( &context->p0, context->segPtr[segInx].u.c.center, fabs(context->segPtr[segInx].u.c.radius), context->segPtr[segInx].u.c.a0 );
	REORIGIN( context->p0, context->p0, context->angle, context->orig );
	PointOnCircle( &context->p1, context->segPtr[segInx].u.c.center, fabs(context->segPtr[segInx].u.c.radius), context->segPtr[segInx].u.c.a0 + context->segPtr[segInx].u.c.a1);
	REORIGIN( context->p1, context->p1, context->angle, context->orig );
	if (context->segPtr[segInx].u.c.a1<360) {
		context->arc_angle = context->segPtr[segInx].u.c.a1;
		PointOnCircle( &context->pm, context->segPtr[segInx].u.c.center, fabs(context->segPtr[segInx].u.c.radius), context->segPtr[segInx].u.c.a0 + (context->segPtr[segInx].u.c.a1/2));
		REORIGIN( context->pm, context->pm, context->angle, context->orig );
		coOrd cm,pm;
		cm = context->pm;
		ANGLE_T a = FindAngle(context->p1,context->p0);
		Rotate(&cm,context->p1,-a );
		context->disp = cm.x-context->p1.x;
	} else {
		context->pm = context->p0;
		context->disp = 0;
		context->arc_angle = 360.0;
	}
}

void CreateSelectedAnchor(coOrd pos) {
	double d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int inx = anchors_da.cnt-1;
	anchors(inx).type = SEG_FILCRCL;
	anchors(inx).u.c.a0 = 0.0;
	anchors(inx).u.c.a1 = 360.0;
	anchors(inx).color = wDrawColorBlue;
	anchors(inx).u.c.radius = d/2;
	anchors(inx).u.c.center = pos;
}

/*
 * Rotate Object Dialogs.
 *
 *  Each Draw object has a rotation origin which all the points are offset from.
 *  Formerly this has been set to the origin, but it doesn't have to be. By setting
 *  to a point on the shape this allows assembly by aligning parts to a common point on a base object.
 *  The angle is always set to zero when the Modify finishes but can be altered via Describe.
 *
 *  First locate the origin, and then place a rotation arm which is (optionally) rotated about the origin.
 *  Also supports whole object translate using translate anchor.
 **/

STATUS_T DrawGeomOriginMove(
		wAction_t action,
		coOrd pos,
		drawModContext_t * context) {

	switch ( action&0xFF ) {
		case C_START:
			context->state = MOD_ORIGIN;
			context->rotate_state = TRUE;
			context->rot_moved = TRUE;
			DYNARR_RESET(trkSeg_t,anchors_da);
			CreateOriginAnchor(context->rot_center,FALSE);
			if ((tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) && (context->prev_inx>=0)) {
				CreateSelectedAnchor(points(context->prev_inx).pt);
			}
			MainRedraw();
			InfoMessage("Origin Mode: Place Origin, 0-4 or l, Enter or Esc");
			return C_CONTINUE;
			break;
		case C_DOWN:
			if (context->state == MOD_ORIGIN || context->state == MOD_AFTER_ORIG) {
				context->state = MOD_ORIGIN;
				DYNARR_RESET(trkSeg_t,anchors_da);
				if (IsClose(FindDistance(pos,context->rot_center))) {
					pos = context->rot_center;
				} else {
					context->rot_center = pos;
				}
				CreateOriginAnchor(context->rot_center, TRUE);
				if ((tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) && (context->prev_inx>=0)) {
					CreateSelectedAnchor(points(context->prev_inx).pt);
				}
			}
			MainRedraw();
			return C_CONTINUE;
			break;
		case C_MOVE:
			if (context->state == MOD_ORIGIN || context->state == MOD_AFTER_ORIG){
				context->rot_center = pos;
				DYNARR_RESET(trkSeg_t,anchors_da);
				CreateOriginAnchor(context->rot_center, TRUE);
				if ((tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) && (context->prev_inx>=0)) {
					CreateSelectedAnchor(points(context->prev_inx).pt);
				}
			}
			MainRedraw();
			return C_CONTINUE;
			break;
		case C_UP:
			DYNARR_RESET(trkSeg_t,anchors_da);
			if (context->state == MOD_ORIGIN) {
				context->state = MOD_AFTER_ORIG;
			}
			CreateOriginAnchor(context->rot_center,FALSE);
			if ((tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) && (context->prev_inx>=0)) {
				CreateSelectedAnchor(points(context->prev_inx).pt);
			}
			MainRedraw();
			return C_CONTINUE;
			break;
		case C_UPDATE:
			DYNARR_RESET(trkSeg_t,anchors_da);
			if (context->state == MOD_AFTER_ORIG) {
				if (context->rot_center.x != context->rel_center.x &&
					context->rot_center.y != context->rel_center.y ) {
					context->rel_center = context->rot_center;
					CreateOriginAnchor(context->rot_center, FALSE);
					if ((tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) && (context->prev_inx>=0)) {
						CreateSelectedAnchor(points(context->prev_inx).pt);
					}
				}
			}
			MainRedraw();
			return C_CONTINUE;
			break;
		case C_TEXT:
			if ((context->state == MOD_ORIGIN || context->state == MOD_AFTER_ORIG) &&
				((action>>8 >= '0' && action>>8 <= '1') || action>>8 == 'l' || action>>8 == 'c' || action>>8 == 'p')) {
				// 0,1,2,3,4 -> reset rot center
				if (action>>8 == '0') {
					context->rot_center = zero;
				} else if (action>>8 == '1') {
					context->rot_center = context->p0;
				} else if (action>>8 == '2') {
					context->rot_center = context->p1;
				} else if (tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) {
					if (action>>8 == '3' || action>>8 == '4') {
						context->rot_center = action>>8 == '3'?points(2).pt:points(3).pt;
					} else if (action>>8 == 'l') {   //"l" - last selected
						if (context->prev_inx !=-1) {
							context->rot_center = points(context->prev_inx).pt;
						}
					} else if (action>>8 == 'c') {
						context->rot_center = FindCentroid(points_da.cnt,&points(0));
					}
				}
				if (action>>8 == 'p') {     //"p" - points mode
					MenuMode(0);
					MainRedraw();
					return C_CONTINUE;
				}
				context->rel_center = context->rot_center;
				context->rot_angle = 0;
				DYNARR_RESET(trkSeg_t,anchors_da);
				context->state = MOD_AFTER_ORIG;
				CreateOriginAnchor(context->rot_center, FALSE);
				if ((tempSegs(0).type == SEG_POLY || tempSegs(0).type == SEG_FILPOLY) && (context->prev_inx>=0)) {
					CreateSelectedAnchor(points(context->prev_inx).pt);
				}
				MainRedraw();
				return C_CONTINUE;
			}
			break;
		case C_FINISH:
			context->rotate_state = FALSE;
			context->state = MOD_STARTED;
			return C_CONTINUE;
			break;
		default:
			break;
	}
	return C_CONTINUE;


}

/*
 * Base Modify function for Draw objects.
 */
STATUS_T DrawGeomModify(
		wAction_t action,
		coOrd pos,
		drawModContext_t * context)
{
	ANGLE_T a;
	coOrd p0, p1, pc, pm;
	static coOrd start_pos;
	static wIndex_t segInx;
	static EPINX_T segEp;
	static ANGLE_T segA1;
	static int inx_other, inx_line, inx_origin;
	static BOOL_T corner_mode;
	static BOOL_T polyMode;
	static ANGLE_T original_angle;
	int inx, inx1, inx2;
	DIST_T d, d1, d2, dd;
	coOrd * newPts = NULL;
	int mergePoints;
	tempSegs_da.cnt = 1;
	wDrawColor save_color;
	switch ( action&0xFF ) {
	case C_START:
		if (!context->rotate_state && !context->rot_moved) {
			context->rot_center.x = context->orig.x;
			context->rot_center.y = context->orig.y;
		}
		context->state = MOD_STARTED;
		context->rotate_state = FALSE;
		context->last_inx=-1;
		lineInx = -1;
		curveInx = -1;
		segInx = -1;
		polyMode = FALSE;
		DistanceSegs( context->orig, context->angle, context->segCnt, context->segPtr, &pos, &segInx );
		if (segInx == -1)
			return C_ERROR;
		context->type = context->segPtr[segInx].type;
		switch(context->type) {
			case SEG_TBLEDGE:
			case SEG_STRLIN:
			case SEG_DIMLIN:
			case SEG_BENCH:
				REORIGIN( p0, context->segPtr[segInx].u.l.pos[0], context->angle, context->orig );
				REORIGIN( p1, context->segPtr[segInx].u.l.pos[1], context->angle, context->orig );
				tempSegs(0).type = SEG_STRLIN;
				tempSegs(0).color = wDrawColorRed;
				tempSegs(0).u.l.pos[0] = p0;
				tempSegs(0).u.l.pos[1] = p1;
				tempSegs(0).width = 0;
				tempSegs_da.cnt = 1;
				context->p0 = p0;
				context->p1 = p1;
				CreateLineAnchors(-1,p0,p1);
				break;
			case SEG_CRVLIN:
			case SEG_FILCRCL:
				BuildCircleContext(context,segInx);
				tempSegs(0).type = SEG_CRVLIN;
				tempSegs(0).color = wDrawColorRed;
				tempSegs(0).u.c.center = context->pc;
				tempSegs(0).u.c.radius = context->radius;
				tempSegs(0).u.c.a0 = context->segPtr[segInx].u.c.a0;
				tempSegs(0).u.c.a1 = context->segPtr[segInx].u.c.a1;
				tempSegs(0).width = 0;
				tempSegs_da.cnt = 1;
				if (tempSegs(0).u.c.a1<360.0) {
					CreateCurveAnchors(-1,context->pm,context->pc,context->p0,context->p1);
				}
				context->last_inx = 2;
				break;
			case SEG_POLY:
			case SEG_FILPOLY:
				if (context->segPtr[segInx].u.p.polyType !=RECTANGLE) {
					polyMode = TRUE;
					return DrawGeomPolyModify(action,pos,context);
				} else {
					tempSegs(0).type = SEG_POLY;
					tempSegs(0).color = wDrawColorRed;
					DYNARR_RESET( pts_t, points_da);
					for (int inx=0;inx<context->segPtr->u.p.cnt;inx++) {
						DYNARR_APPEND(pts_t, points_da,3);
						REORIGIN( points(inx).pt, context->segPtr[segInx].u.p.pts[inx].pt, context->angle, context->orig );
					}
					tempSegs(0).u.p.pts = &points(0);
					tempSegs(0).u.p.cnt = points_da.cnt;
					tempSegs(0).width = 0;
					tempSegs_da.cnt = 1;
					context->p0 = points(0).pt;
					CreateBoxAnchors(-1,&context->segPtr[segInx].u.p.pts[0]);
					context->p0 = points(0).pt;
					context->p1 = points(1).pt;
				}
				break;
			default:
				;
		}
		InfoMessage("Points Mode - Select and drag Anchor Point");
		MainRedraw();
		return C_CONTINUE;
	case C_DOWN:
		if (context->rotate_state) return DrawGeomOriginMove(action,pos,context);
		if (polyMode) return DrawGeomPolyModify(action,pos,context);
		corner_mode = FALSE;
		segInx = 0;
		context->state = MOD_STARTED;
		tempSegs(0).width = context->segPtr[segInx].width;
		tempSegs(0).color = context->segPtr[segInx].color;
		switch ( context->type ) {
		case SEG_TBLEDGE:
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
			p0 = context->p0;
			p1 = context->p1;
			lineInx = -1;
			dd = FindDistance( p0, pos );
			if ( IsClose(dd)) {
				lineInx = 0;
			} else {
				dd = FindDistance( p1, pos );
				if ( IsClose(dd)) {
					lineInx = 1;
				}
			}
			if (lineInx < 0 ) {
				InfoMessage( _("Not Close to End of Line"));
			} else {
				InfoMessage("End Selected Drag to Reposition");
				context->state = MOD_SELECTED_PT;
			}
			tempSegs(0).type = context->type;
			tempSegs(0).u.l.pos[0] = p0;
			tempSegs(0).u.l.pos[1] = p1;
			tempSegs(0).u.l.option = context->segPtr[segInx].u.l.option;
			segA1 = FindAngle( p1, p0 );
			tempSegs_da.cnt = 1;
			CreateLineAnchors(lineInx,p0,p1);
			break;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			curveInx = -1;
			tempSegs(0).type = context->type;
			tempSegs(0).u.c.center = context->pc;
			tempSegs(0).u.c.radius = context->radius;
			if (context->segPtr[segInx].u.c.a1 >= 360.0) {
				tempSegs(0).u.c.a0 = 0.0;
				tempSegs(0).u.c.a1 = 360.0;
				InfoMessage("Drag to Change Radius");
			} else {
				p0 = context->p0;
				p1 = context->p1;
				pm = context->pm;
				pc = context->pc;
				tempSegs(0).u.c.a0 = FindAngle(context->pc,context->p0);
				tempSegs(0).u.c.a1 = DifferenceBetweenAngles(FindAngle(context->pc,context->p0),FindAngle(context->pc,context->p1));
				tempSegs(0).u.c.center = context->pc;
				tempSegs(0).u.c.radius = context->radius;
				curveInx = -1;
				dd = FindDistance( p0, pos );
				if ( IsClose(dd)) {
					curveInx = 0;
				} else {
					dd = FindDistance( p1, pos );
					if ( IsClose(dd)) {
						curveInx = 1;
					} else {
						dd = FindDistance( pm, pos );
						if ( IsClose(dd)) {
							curveInx = 2;
						}
					}
				}
				if (curveInx < 0) {
					InfoMessage( _("Not Close to Ends or Middle of Line, Reselect"));
					return C_CONTINUE;
				} else {
					if (curveInx <2 )
						InfoMessage("Drag to Move End, +CTL to lock to other objects");
					else
						InfoMessage("Drag to Change Radius");
				}
				tempSegs_da.cnt = 1;
				if (tempSegs(0).u.c.a1 < 360.0)
					CreateCurveAnchors(curveInx,pm,pc,p0,p1);
			}
			context->state = MOD_SELECTED_PT;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			d = 10000;
			polyInx = 0;
			for ( inx=0; inx<4; inx++ ) {
				p0 = pos;
				dd = LineDistance( &p0, points( inx==0?3:inx-1).pt, points( inx ).pt );
				if ( d > dd ) {
					d = dd;
					inx_line = inx;
				}
			}
			d1 = FindDistance( points(inx_line).pt, pos );
			d2 = FindDistance( points(inx_line==0?3:inx_line-1).pt, pos );
			if (d2<d1) {
				polyInx = inx_line==0?3:inx_line-1;
			} else {
				polyInx = inx_line;
			}
			//polyInx is closest point inx_line is the closest line
			inx1 = (polyInx==0?3:polyInx-1);  //Prev point
			inx2 = (polyInx==3?0:polyInx+1);  //Next Point
			inx_origin = (inx2==3?0:inx2+1);  //Opposite point
			if ( IsClose(d2) || IsClose(d1) ) {
				corner_mode = TRUE;
				start_pos = pos;
				DYNARR_SET(trkSeg_t,anchors_da,5);
				polyInx = inx_line;
				DrawArrowHeads( &anchors(0), pos, FindAngle(points(polyInx).pt,points(inx_origin).pt), TRUE, wDrawColorRed );
				InfoMessage( _("Drag to Move Corner Point"));
			} else {
				corner_mode = FALSE;
				start_pos = pos;
				pos.x = (points(inx_line).pt.x + points(inx_line==0?3:inx_line-1).pt.x)/2;
				pos.y = (points(inx_line).pt.y + points(inx_line==0?3:inx_line-1).pt.y)/2;
				DYNARR_SET(trkSeg_t,anchors_da,5);
				DrawArrowHeads( &anchors(0), pos, FindAngle(points(polyInx).pt,pos)+90, TRUE, wDrawColorRed );
				InfoMessage( _("Drag to Move Edge "));
			}
			context->state = MOD_SELECTED_PT;
			MainRedraw();
			return C_CONTINUE;
		case SEG_TEXT:
			segInx = -1;
			return C_ERROR;
		default:
			ASSERT( FALSE ); /* CHECKME */

		}
		if ( FindDistance( p0, pos ) < FindDistance( p1, pos ) )
			segEp = 0;
		else {
			segEp = 1;
			switch ( context->type ) {
			case SEG_TBLEDGE:
			case SEG_STRLIN:
			case SEG_DIMLIN:
			case SEG_BENCH:
				segA1 = NormalizeAngle( segA1 + 180.0 );
				break;
			default:
				;
			}
		}
		MainRedraw();
		return C_CONTINUE;
	case C_MOVE:
		if (context->rotate_state) return DrawGeomOriginMove(action,pos,context);

		if (polyMode) return DrawGeomPolyModify(action,pos,context);
		if (context->state != MOD_SELECTED_PT) return C_CONTINUE;
		switch (tempSegs(0).type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
			if ( (MyGetKeyState() & WKEY_SHIFT) != 0) {
				d = FindDistance( pos, tempSegs(0).u.l.pos[1-lineInx] );
				Translate( &pos, tempSegs(0).u.l.pos[1-lineInx], segA1, d );
			} else if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
				OnTrack( &pos, FALSE, FALSE );
				CreateEndAnchor(pos,TRUE);
			}
			break;
		default:
			break;
		}
		int prior_pnt, next_pnt, orig_pnt;
		ANGLE_T prior_angle, next_angle, line_angle;
		tempSegs_da.cnt = 1;
		switch (tempSegs(0).type) {
		case SEG_TBLEDGE:
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
			if (lineInx<0 || lineInx>1) return C_CONTINUE;
			tempSegs(0).u.l.pos[lineInx] = pos;
			InfoMessage( _("Length = %0.3f Angle = %0.3f"), FindDistance( tempSegs(0).u.l.pos[lineInx], tempSegs(0).u.l.pos[1-lineInx] ), FindAngle( tempSegs(0).u.l.pos[1-lineInx], tempSegs(0).u.l.pos[lineInx] ) );
			tempSegs_da.cnt = 1;
			context->p0 = tempSegs(0).u.l.pos[0];
			context->p1 = tempSegs(0).u.l.pos[1];
			p0 = context->p0;
			p1 = context->p1;
			CreateLineAnchors(lineInx, p0, p1);
			break;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			if (tempSegs(0).u.c.a1 >= 360.0) {
				tempSegs(0).u.c.radius = FindDistance( context->pc, pos );
			} else {
				if (context->state != MOD_SELECTED_PT) return C_CONTINUE;
				if (curveInx < 0 || curveInx > 2) return C_CONTINUE;
				p0 = context->p0;
				p1 = context->p1;
				pc = context->pc;
				pm = context->pm;
				if ( (MyGetKeyState() & WKEY_SHIFT) != 0) {
					//Preserve Radius, Change swept angle
					a = FindAngle( pc, pos );
					if (curveInx==0) {
						tempSegs(0).u.c.a1 = NormalizeAngle(FindAngle(pc,p1)-a);
						tempSegs(0).u.c.a0 = a;
					} else if (curveInx ==1) {
						tempSegs(0).u.c.a1 = NormalizeAngle(a-tempSegs(0).u.c.a0);
					}
					PointOnCircle(&p0,pc,context->radius,tempSegs(0).u.c.a0);
					PointOnCircle(&p1,pc,context->radius,tempSegs(0).u.c.a0+tempSegs(0).u.c.a1);
					context->p0 = p0;
					context->p1 = p1;
					PointOnCircle(&pm,pc,context->radius,tempSegs(0).u.c.a0+(tempSegs(0).u.c.a1/2));
					context->pm = pm;
				} else {
					if (curveInx == 0 || curveInx == 1) {
						p0 = context->p0;
						p1 = context->p1;
						/* Preserve Curve "Deflection" */
						d = context->disp;  	// Original Deflection
						if (curveInx == 0) {
							context->p0 = p0 = pos;
						} else {
							context->p1 = p1 = pos;
						}
						coOrd posx; //Middle of chord
						posx.x = (p1.x-p0.x)/2.0+p0.x;
						posx.y = (p1.y-p0.y)/2.0+p0.y;			//Middle of chord
						ANGLE_T a0 = FindAngle( p1, p0 );
						DIST_T d0 = FindDistance( p0, p1 )/2.0;
						DIST_T r = 1000.0;
						if ( fabs(d) >= 0.01 ) {
							d2 = sqrt( d0*d0 + d*d )/2.0;
							r = d2*d2*2.0/d;
							if ( fabs(r) > 1000.0 )
								r = ((r > 0)? 1 : -1) *1000.0;
						} else {
							r = ((r > 0) ? 1 : -1 ) *1000.0;
						}
						a0 -= 90.0;
						if (r<0) {
							coOrd pt = p0;
							p0 = p1;
							p1 = pt;
							a0 += 180.0;
						}
						context->radius = tempSegs(0).u.c.radius = fabs(r);
						Translate( &pc, posx, a0, fabs(r)-fabs(d) );
						context->pc = tempSegs(0).u.c.center = pc;
					    tempSegs(0).u.c.a0 = FindAngle(pc,p0);
					    context->arc_angle = tempSegs(0).u.c.a1 = NormalizeAngle(FindAngle(pc,p1)-FindAngle(pc,p0));
					    PointOnCircle(&pm,pc,context->radius,tempSegs(0).u.c.a0+(tempSegs(0).u.c.a1/2.0));
					    context->pm = pm;
					} else {
						//Change radius using chord
						p0 = context->p0;
						p1 = context->p1;
						pm = context->pm;
						ANGLE_T a0 = FindAngle( p1, p0 );
						DIST_T d0 = FindDistance( p0, p1 )/2.0;
						coOrd pos2 = pos;
						Rotate( &pos2, p1, -a0 );
						pos2.x -= p1.x;
						DIST_T r = 1000.0;
						if ( fabs(pos2.x) >= 0.01 ) {
							d2 = sqrt( d0*d0 + pos2.x*pos2.x )/2.0;
							r = d2*d2*2.0/pos2.x;
							if ( fabs(r) > 1000.0 )
								r = ((r > 0) ? 1 :  -1 ) *1000.0;
						} else {
							r = ((r > 0) ? 1 :  -1 ) *1000.0;
						}
						coOrd posx; //Middle of chord
						posx.x = (p1.x-p0.x)/2.0 + p0.x;
						posx.y = (p1.y-p0.y)/2.0 + p0.y;
						a0 -= 90.0;
						if (r<0) {
							coOrd pt = p0;
							p0 = p1;
							p1 = pt;
							a0 += 180.0;
						}
						Translate( &pc, posx, a0, fabs(r) - fabs(pos2.x) );
						context->pc = tempSegs(0).u.c.center = pc;
						context->radius = tempSegs(0).u.c.radius = fabs(r);
						a0 = FindAngle( pc, p0 );
						ANGLE_T a1 = FindAngle( pc, p1 );
						tempSegs(0).u.c.a0 = a0;
						tempSegs(0).u.c.a1 = NormalizeAngle(a1-a0);
						PointOnCircle(&pm,pc,context->radius,a0+(NormalizeAngle(a1-a0)/2));
						context->p0 = p0;
						context->p1 = p1;
						context->pm = pm;
						context->disp = pos2.x;
					}
				}
				if (tempSegs(0).u.c.a1 < 360.0)
					CreateCurveAnchors(curveInx,pm,pc,p0,p1);
			}
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			if (!corner_mode) {
				/* Constrain movement to be perpendicular */
				d = FindDistance(start_pos, pos);
				line_angle = NormalizeAngle(FindAngle(points(inx_line).pt,points(inx_line==3?0:inx_line+1).pt));
				a = FindAngle(pos,start_pos);
				Translate( &pos, start_pos, line_angle, - d*cos(D2R(line_angle-a)));
			}
			d = FindDistance(start_pos,pos);
			a = FindAngle(start_pos, pos);
			start_pos = pos;
			prior_pnt = (polyInx == 0)?3:polyInx-1;
			next_pnt = (polyInx == 3)?0:polyInx+1;
			orig_pnt = (prior_pnt == 0)?3:prior_pnt-1;
			Translate( &points(polyInx).pt, points(polyInx).pt, a, d);
			d = FindDistance(points(orig_pnt).pt,points(polyInx).pt);
			a = FindAngle(points(orig_pnt).pt,points(polyInx).pt);
			prior_angle = FindAngle(points(orig_pnt).pt,points(prior_pnt).pt);
			Translate( &points(prior_pnt).pt, points(orig_pnt).pt, prior_angle, d*cos(D2R(prior_angle-a)));
			next_angle = FindAngle(points(orig_pnt).pt,points(next_pnt).pt);
			Translate( &points(next_pnt).pt, points(orig_pnt).pt, next_angle, d*cos(D2R(next_angle-a)));
			if (!corner_mode) {
				pos.x = (points(inx_line).pt.x + points(inx_line==0?3:inx_line-1).pt.x)/2;
				pos.y = (points(inx_line).pt.y + points(inx_line==0?3:inx_line-1).pt.y)/2;
				DYNARR_SET(trkSeg_t,anchors_da,5);
				DrawArrowHeads( &anchors(0), pos, FindAngle(points(inx_line).pt,points(inx_line==0?3:inx_line-1).pt)+90, TRUE, wDrawColorRed );
				InfoMessage( _("Drag to Move Edge"));
			} else {
				pos = points(polyInx).pt;
				DYNARR_SET(trkSeg_t,anchors_da,5);
				DrawArrowHeads( &anchors(0), pos, FindAngle(points(polyInx).pt,points(inx_origin).pt), TRUE, wDrawColorRed );
				InfoMessage( _("Drag to Move Corner Point"));
			}
			context->p0 = points(0).pt;
			context->p1 = points(1).pt;
			break;
		default:
			;
		}
		MainRedraw();
		return C_CONTINUE;
	case C_UP:

		if (context->rotate_state) return DrawGeomOriginMove(action, pos, context);

		if (polyMode) {
			int rc;
			rc = DrawGeomPolyModify(action,pos,context);
			context->state = MOD_AFTER_PT;
			return rc;
		}

		if (segInx == -1)
			return C_CONTINUE;
		switch (tempSegs(0).type) {
		case SEG_TBLEDGE:
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
			p0 = context->p0;
			p1 = context->p1;
			context->angle = FindAngle(p0,p1);
			context->length = FindDistance(p0,p1);
			CreateLineAnchors(lineInx,p0,p1);
			context->last_inx = lineInx;
			break;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			if ( (tempSegs(0).type == SEG_FILCRCL) || (tempSegs(0).u.c.a1 == 360.0 || tempSegs(0).u.c.a1 == 0.0) ) {
				context->radius = fabs(tempSegs(0).u.c.radius);
				context->arc_angle = 360.0;
			} else {
				p0 = context->p0;
				p1 = context->p1;
				pc = context->pc;
				pm = context->pm;
				context->radius = fabs(tempSegs(0).u.c.radius);
				context->arc_angle = tempSegs(0).u.c.a1;
				CreateCurveAnchors(curveInx,pm,pc,p0,p1);
				a = FindAngle(p1,p0);
				Rotate(&pm,p1,-a);
				context->disp = pm.x-p1.x;
			}
			context->last_inx = curveInx;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			CreateBoxAnchors(-1,tempSegs(0).u.p.pts);
			context->width = FindDistance(tempSegs(0).u.p.pts[0].pt,tempSegs(0).u.p.pts[1].pt);
			context->height = FindDistance(tempSegs(0).u.p.pts[1].pt,tempSegs(0).u.p.pts[2].pt);
			context->last_inx = polyInx;
			break;
		default:
			;
		}
		context->state = MOD_AFTER_PT;
		curveInx = -1;
		lineInx = -1;
		polyInx = -1;
		InfoMessage("Enter/Space to Accept, ESC to Reject");
		MainRedraw();
		return C_CONTINUE;
	case C_UPDATE:
		if (context->rotate_state) return DrawGeomOriginMove(action, pos, context);

		if (polyMode) return DrawGeomPolyModify(action,pos,context);
		if (context->state == MOD_AFTER_PT) {
			switch (tempSegs(0).type) {
				case SEG_TBLEDGE:
				case SEG_STRLIN:
				case SEG_DIMLIN:
				case SEG_BENCH:
					context->angle = NormalizeAngle(context->angle);
					Translate(&tempSegs(0).u.l.pos[1],tempSegs(0).u.l.pos[0],context->angle,context->length);
					CreateLineAnchors(context->last_inx,tempSegs(0).u.l.pos[0],tempSegs(0).u.l.pos[1]);
					context->p0 = tempSegs(0).u.l.pos[0];
					context->p1 = tempSegs(0).u.l.pos[1];
					break;
				case SEG_CRVLIN:
				case SEG_FILCRCL:
					if (tempSegs(0).u.c.a1 == 360.0 || tempSegs(0).u.c.a1 == 0.0) {
						tempSegs(0).u.c.a1 = 360.0;
						tempSegs(0).u.c.radius = context->radius;
						Translate(&p0,tempSegs(0).u.c.center,tempSegs(0).u.c.a0,tempSegs(0).u.c.radius);
						context->p0 = p0;
						context->p1 = p0;
						context->pm = p0;
						context->pc = tempSegs(0).u.c.center;
						break;
					}
					if (context->radius < 0) { //swap ends
						context->radius = fabs(context->radius);
						p1 = context->p0;
						p0 = context->p1;
						a = FindAngle(context->pc,context->pm);
						Translate(&pm,context->pm,a+180,2*context->disp);
						Translate(&pc,pm,a,context->radius);
						context->pm = pm;
						context->pc = pc;
						context->p0 = p0;
						context->p1 = p1;
					} else {
						pm = context->pm;
						pc = context->pc;
						p0 = context->p0;
						p1 = context->p1;
					}
					if (context->last_inx == 0) {
						p1 = context->p1;
						pc = context->pc;
						a = FindAngle(p1,pc);
						Translate(&pc,p1,a,context->radius);
						context->pc = pc;
						PointOnCircle( &p0, context->pc, context->radius, NormalizeAngle(a+180-context->arc_angle) );
						context->p0 = p0;
						PointOnCircle( &pm, context->pc, context->radius, NormalizeAngle(a+180-(context->arc_angle/2)));
						context->pm = pm;
					} else if (context->last_inx == 1) {
						p0 = context->p0;
						pc = context->pc;
						a = FindAngle(p0,pc);
						Translate(&pc,p0,a,context->radius);
						context->pc = pc;
						PointOnCircle( &p1, context->pc, context->radius, NormalizeAngle(a+180+context->arc_angle) );
						context->p1 = p1;
						PointOnCircle( &pm, context->pc, context->radius, NormalizeAngle(a+180+(context->arc_angle/2)));
						context->pm = pm;
					} else {   // Middle if neither
						a = FindAngle(context->pm,context->pc);
						Translate(&pc,context->pm,a,context->radius);
						context->pc = pc;
						PointOnCircle( &p1, context->pc, context->radius, NormalizeAngle(FindAngle(context->pc,context->pm)+(context->arc_angle/2)) );
						PointOnCircle( &p0, context->pc, context->radius, NormalizeAngle(FindAngle(context->pc,context->pm)-(context->arc_angle/2)) );
						context->p1 = p1;
						context->p0 = p0;
					}
					a = FindAngle(p1,p0);
					Rotate(&pm,p1,-a);
					context->disp = pm.x-p1.x;
					tempSegs(0).u.c.center = context->pc;
					tempSegs(0).u.c.a0 = FindAngle(context->pc,context->p0);
					tempSegs(0).u.c.a1 = NormalizeAngle(FindAngle(context->pc,context->p1)-tempSegs(0).u.c.a0);
					if (tempSegs(0).u.c.a1 == 0.0) tempSegs(0).u.c.a1 = 360.0;
					tempSegs(0).u.c.radius = context->radius;
					CreateCurveAnchors(context->last_inx,context->pm,context->pc,context->p0,context->p1);
					break;
				case SEG_POLY:
				case SEG_FILPOLY:
					a = NormalizeAngle(FindAngle(points(0).pt,points(3).pt));
					Translate( &points(3).pt, points(0).pt, a, context->height);
					Translate( &points(2).pt, points(1).pt, a, context->height);
					a = NormalizeAngle(FindAngle(points(0).pt,points(1).pt));;
					Translate( &points(1).pt, points(0).pt, a, context->width);
					Translate( &points(2).pt, points(3).pt, a, context->width);
					CreateBoxAnchors(context->last_inx,&points(0));
					break;
				default:
					break;
			}
		}
		MainRedraw();
		break;
	case C_TEXT:
		if (context->rotate_state) DrawGeomOriginMove(action, pos, context);

		if (polyMode) return DrawGeomPolyModify(action,pos,context);

		if (action>>8 == 'o') {
			MenuMode(1);
		}

		if (action>>8 != 32 && action>>8 != 13) return C_CONTINUE;
		/* no break */
	case C_FINISH:
		if (polyMode) return DrawGeomPolyModify(action,pos,context);
		//copy changes back into track
		context->orig.x = context->rot_center.x;
		context->orig.y = context->rot_center.y;
		context->rot_moved = FALSE;
		context->angle = 0;
		switch (tempSegs(0).type) {
			case SEG_TBLEDGE:
			case SEG_STRLIN:
			case SEG_DIMLIN:
			case SEG_BENCH:
				for (int i=0;i<2;i++) {
					pos = i==0?context->p0:context->p1;
					pos.x -= context->rot_center.x;
					pos.y -= context->rot_center.y;
					context->segPtr[segInx].u.l.pos[i] = pos;
				}
				break;
			case SEG_CRVLIN:
			case SEG_FILCRCL:
				pc = context->pc;
				pc.x -= context->rot_center.x;
				pc.y -= context->rot_center.y;
				context->segPtr[segInx].u.c.center = pc;
				p0 = context->p0;
				p0.x -= context->rot_center.x;
				p0.y -= context->rot_center.y;
				p1 = context->p1;
				p1.x -= context->rot_center.x;
				p1.y -= context->rot_center.y;
				context->segPtr[segInx].u.c.a0 = FindAngle(pc,p0);
				context->segPtr[segInx].u.c.a1 = NormalizeAngle(FindAngle(pc,p1)-FindAngle(pc,p0));
				if (context->segPtr[segInx].u.c.a1 == 0) context->segPtr[segInx].u.c.a1 = 360.0;
				context->segPtr[segInx].u.c.radius = context->radius;
				break;
			case SEG_POLY:
			case SEG_FILPOLY:
				for (int i=0;i<4;i++) {
					pos = points(i).pt;
					pos.x -= context->rot_center.x;
					pos.y -= context->rot_center.y;
					context->segPtr[segInx].u.p.pts[i].pt = pos;
				}
				break;
			default:
				break;
		}
		context->state = MOD_NONE;
		context->rotate_state = FALSE;
		context->last_inx = -1;
		DYNARR_RESET(trkSeg_t,anchors_da);
		DYNARR_RESET(trkSeg_t,tempSegs_da);
		return C_TERMINATE;
	case C_REDRAW:
		if (polyMode) return DrawGeomPolyModify(action,pos,context);
		if (context->state == MOD_NONE) return C_CONTINUE;
		DrawTrack(context->trk,&mainD,wDrawColorWhite);
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack);
		DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		break;
	case C_CONFIRM:
	case C_TERMINATE:
		context->state = MOD_NONE;
		context->rotate_state = FALSE;
		context->rot_moved = FALSE;
		DYNARR_RESET(trkSeg_t,anchors_da);
		DYNARR_RESET(trkSeg_t,tempSegs_da);
		break;
	default:
		;
	}
	return C_ERROR;
}
