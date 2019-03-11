
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

#define points(N) DYNARR_N( coOrd, points_da, N )
#define anchors(N) DYNARR_N( trkSeg_t, anchors_da, N)

static void EndPoly( drawContext_t * context, int cnt )
{
	trkSeg_p segPtr;
	track_p trk;
	long oldOptions;
	coOrd * pts;
	int inx;

	if (context->State==0 || cnt == 0)
		return;
	
	oldOptions = context->D->funcs->options;
	context->D->funcs->options |= wDrawOptTemp;
	DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
	context->D->funcs->options = oldOptions;

	if (IsClose(FindDistance(tempSegs(0).u.l.pos[0], tempSegs(cnt-1).u.l.pos[1] )))
		cnt--;
	if ( cnt < 2 ) {
		tempSegs_da.cnt = 0;
		ErrorMessage( MSG_POLY_SHAPES_3_SIDES );
		return;
	}
	pts = (coOrd*)MyMalloc( (cnt+1) * sizeof (coOrd) );
	for ( inx=0; inx<cnt; inx++ )
		pts[inx] = tempSegs(inx).u.l.pos[0];
	pts[cnt] = tempSegs(cnt-1).u.l.pos[1];
	DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
	segPtr = &tempSegs(0);
	segPtr->type = ( context->Op == OP_POLY ? SEG_POLY: SEG_FILPOLY );
	segPtr->u.p.cnt = cnt+1;
	segPtr->u.p.pts = pts;
	segPtr->u.p.angle = 0.0;
	segPtr->u.p.orig = zero;
	segPtr->u.p.polyType = FREEFORM;
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

/**
 * Create and draw a graphics primitive (lines, arc, circle). The complete handling of mouse 
 * movements and clicks during the editing process is done here. 
 *
 * \param action IN mouse action 
 * \param pos IN position of mouse pointer
 * \param context IN/OUT parameters for drawing op 
 * \return next command state
 */

STATUS_T DrawGeomMouse(
		wAction_t action,
		coOrd pos,
		drawContext_t *context)
{
	static int lastValid = FALSE;
	static coOrd pos0, pos0x, pos1, lastPos;
	trkSeg_p segPtr;
	coOrd *pts;
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
		if (context->State == 0 || context->State == 1) return C_TERMINATE;
		switch (context->Op) {
		case OP_LINE:
		case OP_BENCH:
		case OP_TBLEDGE:
			a1 = FindAngle(pos0,pos1);
			Translate(&tempSegs(0).u.l.pos[1],tempSegs(0).u.l.pos[0],context->angle,context->length);
			lastPos = pos1 = tempSegs(0).u.l.pos[1];
			tempSegs_da.cnt = 1;
			segCnt = 1;
		break;
		case OP_BOX:
		case OP_FILLBOX:
			pts = tempSegs(0).u.p.pts;
			a1 = FindAngle(pts[0],pts[1]);
			Translate(&pts[1],pts[0],a1,context->length);
			a2 = FindAngle(pts[0],pts[3]);
			Translate(&pts[2],pts[1],a2,context->width);
			Translate(&pts[3],pts[0],a2,context->width);
			tempSegs_da.cnt = 1;
			segCnt = 1;
		break;
		case OP_POLY:
		case OP_FILLPOLY:
			segCnt = tempSegs_da.cnt;
			Translate(&tempSegs(segCnt-1).u.l.pos[1],tempSegs(segCnt-1).u.l.pos[0],context->angle,context->length);
			pos1 = lastPos = tempSegs(segCnt-1).u.l.pos[1];
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
		DYNARR_SET( trkSeg_t, anchors_da, 1 );
		return C_CONTINUE;

	case wActionMove:
		if (context->State == 0) {
			switch (context->Op) {  	//Snap pos to nearest line end point if this is end and just shift is depressed for lines and some curves
				case OP_LINE:
				case OP_CURVE1:
					if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
						coOrd p = pos;
						track_p t;
						if ((t=OnTrack(&p,FALSE,FALSE))) {
							if (GetClosestEndPt(t,&p)) {
								d = tempD.scale*0.15;
								anchors(0).type = SEG_FILCRCL;
								anchors(0).color = wDrawColorBlue;
								anchors(0).u.c.center = p;
								anchors(0).u.c.radius = d/2;
								anchors(0).u.c.a0 = 0.0;
								anchors(0).u.c.a1 = 360.0;
								anchors(0).width = 0;
								anchors_da.cnt = 1;
							} else {
								anchors_da.cnt = 0;
							}
						}
					};
					break;
				default:
					;
			}
		}
		return C_CONTINUE;

	case wActionLDown:
		if (context->State == 2) {
			context->D->funcs->options = oldOptions;
			if ((context->Op == OP_POLY || context->Op == OP_FILLPOLY)) {
				EndPoly(context, segCnt);
			} else {
				tempSegs_da.cnt = segCnt;
				context->D->funcs->options = oldOptions;
				DrawGeomOk();

			}
			anchors_da.cnt = 0;
			MainRedraw();
			context->State = 0;
			//MainRedraw();
		}
		context->Started = TRUE;
		line_angle = 90.0;
		if (context->State == 0) {		//First Down only
			switch (context->Op) {  	//Snap pos to nearest line end point if this is end and just shift is depressed for lines and some curves
				case OP_LINE:
				case OP_CURVE1:
					if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
						coOrd p = pos;
						track_p t;
						if ((t=OnTrack(&p,FALSE,FALSE))) {
							if (GetClosestEndPt(t,&p)) {
								pos = p;
								EPINX_T ep1,ep2;
								line_angle = GetAngleAtPoint(t,pos,&ep1,&ep2);
							}
						}
					};
					break;
				default:
					;
			}
		}
		if ((context->Op == OP_CURVE1 || context->Op == OP_CURVE2 || context->Op == OP_CURVE3 || context->Op == OP_CURVE4) && context->State == 1) {
			;
		} else {
			if ( (MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL )   // Control snaps to nearest track (not necessarily the end)
				OnTrack( &pos, FALSE, FALSE );
			pos0 = pos;
			pos1 = pos;
		}

		switch (context->Op) {
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
			if ( lastValid && ( MyGetKeyState() & WKEY_CTRL ) ) {
				pos = pos0 = lastPos;
			}
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
			context->message( _("Drag to place next end point") );
			break;
		case OP_TBLEDGE:
			if ( lastValid && ( MyGetKeyState() & WKEY_CTRL ) ) {
				pos = pos0 = lastPos;
			}
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
			} else {
				tempSegs_da.cnt = segCnt;
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
		case OP_BOX:
			DYNARR_SET( trkSeg_t, tempSegs_da, 4 );
			for ( inx=0; inx<4; inx++ ) {
				tempSegs(inx).type = SEG_STRLIN;
				tempSegs(inx).color = context->Color;
				tempSegs(inx).width = width;
				tempSegs(inx).u.l.pos[0] = tempSegs(inx).u.l.pos[1] = pos;
			}
			tempSegs_da.cnt = 0;
			context->message( _("Drag set box size") );
			break;
		case OP_POLY:
		case OP_FILLPOLY:
			tempSegs_da.cnt = segCnt;
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			segPtr = &tempSegs(tempSegs_da.cnt-1);
			segPtr->type = SEG_STRLIN;
			segPtr->color = context->Color;
			segPtr->width = (context->Op==OP_POLY?width:0);
			if ( tempSegs_da.cnt == 1 ) {
				segPtr->u.l.pos[0] = pos;
			} else {
				segPtr->u.l.pos[0] = segPtr[-1].u.l.pos[1];
			}
			segPtr->u.l.pos[1] = pos;
			context->State = 1;
			oldOptions = context->D->funcs->options;
			context->D->funcs->options |= wDrawOptTemp;
			DrawSegs( context->D, zero, 0.0, &tempSegs(tempSegs_da.cnt-1), 1, trackGauge, wDrawColorBlack );
			context->D->funcs->options = oldOptions;
			break;
		}
		return C_CONTINUE;

	case wActionLDrag:
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		if (context->Op == OP_POLY || context->Op == OP_FILLPOLY)
			DrawSegs( context->D, zero, 0.0, &tempSegs(tempSegs_da.cnt-1), 1, trackGauge, wDrawColorBlack );
		else
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		if ( (MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL )
			OnTrack( &pos, FALSE, FALSE );
		pos1 = pos;
		switch (context->Op) {
		case OP_TBLEDGE:
			OnTableEdgeEndPt( NULL, &pos1 );
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
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
			}
			tempSegs(0).u.l.pos[1] = pos1;
			context->message( _("Length = %s, Angle = %0.2f"),
						FormatDistance(FindDistance( pos0, pos1 )),
						PutAngle(FindAngle( pos0, pos1 )) );
			tempSegs_da.cnt = 1;
			segCnt = 1;
			break;
		case OP_POLY:
		case OP_FILLPOLY:
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
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
				//Snap to closing 90 degree intersect if close
				if (tempSegs_da.cnt > 1) {
					coOrd intersect;
					if (FindIntersection(&intersect,tempSegs(0).u.l.pos[0],initial_angle+90.0,tempSegs(tempSegs_da.cnt-2).u.l.pos[1],last_angle+90.0)) {
						d = FindDistance(intersect,pos);
						if (IsClose(d)) {
							pos = intersect;
							d = tempD.scale*0.15;
							anchors(0).type = SEG_FILCRCL;
							anchors(0).color = wDrawColorBlue;
							anchors(0).u.c.center = intersect;
							anchors(0).u.c.radius = d/2;
							anchors(0).u.c.a0 = 0.0;
							anchors(0).u.c.a1 = 360.0;
							tempSegs(0).width = 0;
						} else anchors_da.cnt = 0;
					}
				}
			}
			tempSegs(tempSegs_da.cnt-1).type = SEG_STRLIN;
			tempSegs(tempSegs_da.cnt-1).u.l.pos[1] = pos;
			context->message( _("Length = %s, Angle = %0.2f"),
						FormatDistance(FindDistance( tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos )),
						PutAngle(FindAngle( tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos )) );
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
					segCnt = 1;
					context->message( _("Straight Line: Length=%s Angle=%0.3f"),
							FormatDistance(FindDistance( pos0, context->ArcData.pos1 )),
							PutAngle(FindAngle( pos0, context->ArcData.pos1 )) );
				} else if (context->ArcData.type == curveTypeNone) {
					tempSegs_da.cnt = 0;
					segCnt = 0;
					context->message( _("Back") );
				} else if (context->ArcData.type == curveTypeCurve) {
					tempSegs(0).type = SEG_CRVLIN;
					tempSegs(0).u.c.center = context->ArcData.curvePos;
					tempSegs(0).u.c.radius = context->ArcData.curveRadius;
					tempSegs(0).u.c.a0 = context->ArcData.a0;
					tempSegs(0).u.c.a1 = context->ArcData.a1;
					tempSegs_da.cnt = 1;
					segCnt = 1;
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
			break;
		case OP_CIRCLE1:
		case OP_FILLCIRCLE1:
			break;
		case OP_CIRCLE2:
		case OP_FILLCIRCLE2:
			tempSegs(0).u.c.center = pos1;
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
		if (context->Op == OP_POLY || context->Op == OP_FILLPOLY)
			DrawSegs( context->D, zero, 0.0, &tempSegs(tempSegs_da.cnt-1), 1, trackGauge, wDrawColorBlack );
		else
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		context->D->funcs->options = oldOptions;
		if (context->Op == OP_DIMLINE) MainRedraw();   //Wipe Out Text
		return C_CONTINUE;

	case wActionLUp:
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		if (context->Op != OP_POLY && context->Op != OP_FILLPOLY)
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		lastValid = FALSE;
		createTrack = FALSE;
		if ((context->State == 0 && (context->Op == OP_LINE )) ||  //first point release for line,
			(context->State == 1 && context->Op == OP_CURVE1)) {   //second point for curve from end
			switch (context->Op) {  	//Snap pos to nearest line end point if this is on a line and just shift is depressed for lines and some curves
				case OP_CURVE1:
				case OP_LINE:
					if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
						coOrd p = pos1;
						track_p t;
						if ((t=OnTrack(&p,FALSE,FALSE))) {
							if (GetClosestEndPt(t,&p)) {
								pos1 = p;
								if (context->Op == OP_LINE) {
									tempSegs(0).u.l.pos[1] = p;
								} else {
									PlotCurve( drawGeomCurveMode, pos0, pos0x, pos1, &context->ArcData, FALSE );
									if (context->ArcData.type == curveTypeStraight) {
										tempSegs(0).type = SEG_STRLIN;
										tempSegs(0).u.l.pos[0] = pos0;
										tempSegs(0).u.l.pos[1] = context->ArcData.pos1;
										tempSegs_da.cnt = 1;
									} else if (context->ArcData.type == curveTypeNone) {
										tempSegs_da.cnt = 0;
									} else if (context->ArcData.type == curveTypeCurve) {
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
					};
					break;
				default:
				;
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
			break;
		case OP_CURVE1: case OP_CURVE2: case OP_CURVE3: case OP_CURVE4:
			if (context->State == 0) {
				context->State = 1;
				context->ArcAngle = FindAngle( pos0, pos1 );
				pos0x = pos1;
				CreateCurve( C_UP, pos, FALSE, context->Color, width, drawGeomCurveMode, context->message );
				DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
				segCnt = tempSegs_da.cnt;
				context->message( _("Drag on Red arrows to adjust curve") );
				context->D->funcs->options = oldOptions;
				return C_CONTINUE;
			} else {
				tempSegs_da.cnt = 0;
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
				} else if (context->ArcData.type == curveTypeStraight) {
					tempSegs_da.cnt = 1;
					segPtr = &tempSegs(0);
					segPtr->type = SEG_STRLIN;
					segPtr->color = context->Color;
					segPtr->width = width;
					segPtr->u.l.pos[0] = pos0;
					segPtr->u.l.pos[1] = pos1;
				} else {
					tempSegs_da.cnt = 0;
				}
				context->State = 0;
				lastValid = TRUE;
				lastPos = pos1;
				segCnt = 1;
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
			/*drawContext = context;
			DrawGeomOp( (void*)context->Op );*/
			break;
		case OP_BOX:
		case OP_FILLBOX:
			pts = (coOrd*)MyMalloc( 4 * sizeof (coOrd) );
			for ( inx=0; inx<4; inx++ )
				pts[inx] = tempSegs(inx).u.l.pos[0];
			tempSegs(0).type = (context->Op == OP_FILLBOX)?SEG_FILPOLY:SEG_POLY;
			tempSegs(0).u.p.cnt = 4;
			tempSegs(0).u.p.pts = pts;
			tempSegs(0).u.p.angle = 0.0;
			tempSegs(0).u.p.orig = zero;
			tempSegs(0).u.p.polyType = RECTANGLE;
			tempSegs_da.cnt = 1;
			/*drawContext = context;
			DrawGeomOp( (void*)context->Op );*/
			context->length = FindDistance(pts[0],pts[1]);
			context->width = FindDistance(pts[3],pts[0]);
			context->State = 2;
			break;
		case OP_POLY:
		case OP_FILLPOLY:
			segCnt = tempSegs_da.cnt;
			context->D->funcs->options = oldOptions;
			context->length = FindDistance(tempSegs(tempSegs_da.cnt-1).u.l.pos[0],tempSegs(tempSegs_da.cnt-1).u.l.pos[1]);
			context->angle = FindAngle(tempSegs(tempSegs_da.cnt-1).u.l.pos[0],tempSegs(tempSegs_da.cnt-1).u.l.pos[1]);
			context->State = 1;
			segCnt = tempSegs_da.cnt;
			return C_CONTINUE;
		}
		context->Started = FALSE;
		context->Changed = TRUE;
		/*CheckOk();*/
		segCnt = tempSegs_da.cnt;
		if (context->State == 2 && IsCurCommandSticky())
			return C_CONTINUE;

		context->D->funcs->options = oldOptions;
		DrawGeomOk();
		segCnt = 0;
		context->State = 0;
		return C_TERMINATE;

	case wActionText:
		if ( ((action>>8)&0xFF) == 0x0D ||
			 ((action>>8)&0xFF) == ' ' ) {
			if ((context->Op == OP_POLY || context->Op == OP_FILLPOLY))
				EndPoly(context, segCnt);
			else {
				context->D->funcs->options = oldOptions;
				if (context->State == 2)
					tempSegs_da.cnt = segCnt;
				DrawGeomOk();
			}
			context->State = 0;
		}
		return C_TERMINATE;

	case C_CANCEL:

		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		context->D->funcs->options = oldOptions;
		tempSegs_da.cnt = 0;
		context->message( "" );
		context->Changed = FALSE;
		context->State = 0;
		lastValid = FALSE;
		return C_TERMINATE;

	case C_REDRAW:
		if (context->State !=0) {
			oldOptions = context->D->funcs->options;
			context->D->funcs->options |= wDrawOptTemp;
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
			context->D->funcs->options = oldOptions;
		}
		if (anchors_da.cnt > 0) {
			DrawSegs(context->D, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		}
		return C_CONTINUE;
		
	default:
		return C_CONTINUE;
	}
}
static int polyInx;

typedef enum {POLY_NONE, POLY_SELECTED,POLYPOINT_SELECTED} PolyState_e;

void static CreatePolyAnchors(coOrd *add, int index, int last, int next) {
		DYNARR_RESET(trkSeg_t,anchors_da);
		double d = tempD.scale*0.15;
		coOrd p;
		for ( int inx=0; inx<points_da.cnt; inx++ ) {
			DYNARR_APPEND(trkSeg_t,anchors_da,3);
			//Ignore all points except last, next and index if last point index is set
			if ((last != -1) && (next != -1))
					if ((last != inx) && (next != inx) && (index != inx)) continue;
			anchors(inx).type = (index == inx)?SEG_FILCRCL:SEG_CRVLIN;
			anchors(inx).u.c.a0 = 0.0;
			anchors(inx).u.c.a1 = 360.0;
			anchors(inx).color = wDrawColorBlue;
			anchors(inx).u.c.radius = d/2;
			anchors(inx).u.c.center = points(inx);
		}
		if (add) {
			DYNARR_APPEND(trkSeg_t,anchors_da,1);
			int inx = anchors_da.cnt-1;
			anchors(inx).type = SEG_CRVLIN;
			anchors(inx).u.c.a0 = 0.0;
			anchors(inx).u.c.a1 = 360.0;
			anchors(inx).color = wDrawColorRed;
			anchors(inx).u.c.radius = d;
			anchors(inx).u.c.center = *add;
		}
}

/*
 * State model for PolyModify
 * Poly_selected
 * - (if last_inx !=-1, else delete/backspace deletes last_inx)
 * - (if pointer over point, selects - polypoint_elected, else if between points, add point, else nothing)
 * PolyPoint_selected (can drag point around, if dropped over another left or right point, deletes it)
 *
 */
STATUS_T DrawGeomPolyModify(
				track_p trk,
				coOrd orig,
				ANGLE_T angle,
				wIndex_t segCnt,
				trkSeg_p segPtr,
				wAction_t action,
				coOrd pos,
				wBool_t selected) {
	double d;
	static int segInx;
	static int prev_inx;
	static PolyState_e polyState = POLY_NONE;
	static wDrawColor save_color;
	int prior_pnt, next_pnt, orig_pnt;
	ANGLE_T prior_angle, next_angle, line_angle;
	switch ( action&0xFF ) {
		case C_START:
			DistanceSegs( orig, angle, segCnt, segPtr, &pos, &segInx );
			if (segInx == -1)
				return C_ERROR;
			if (segPtr[segInx].type != SEG_POLY && segPtr[segInx].type != SEG_FILPOLY)
				return C_ERROR;
			prev_inx = -1;
			polyState = POLY_SELECTED;
			polyInx = -1;
			//Copy points
			DYNARR_RESET( coOrd, points_da);
			for (int inx=0;inx<segPtr->u.p.cnt;inx++) {
				DYNARR_APPEND(coOrd, points_da,3);
				REORIGIN( points(inx), segPtr[segInx].u.p.pts[inx], angle, orig );
			}
			//Show points
			tempSegs_da.cnt = 1;
			tempSegs(0).width = segPtr->width;
			save_color = segPtr->color;
			tempSegs(0).color = wDrawColorRed;
			tempSegs(0).type = segPtr[segInx].type;
			tempSegs(0).u.p.cnt = segPtr[segInx].u.p.cnt;
			tempSegs(0).u.p.angle = 0.0;
			tempSegs(0).u.p.orig = zero;
			tempSegs(0).u.p.polyType = segPtr[segInx].u.p.polyType;
			tempSegs(0).u.p.pts = &points(0);
			CreatePolyAnchors(NULL, -1, -1, -1);
			MainRedraw();
			return C_CONTINUE;
		case C_DOWN:
			d = 10000.0;
			polyInx = -1;
			coOrd p0;
			double dd;
			int inx;
			//Select Point (polyInx)
			for ( int inx=0; inx<points_da.cnt; inx++ ) {
				p0 = pos;
				dd = LineDistance( &p0, points( inx==0?points_da.cnt-1:inx-1), points( inx ) );
				if ( d > dd ) {
					d = dd;
					polyInx = inx;
				}
			}
			if (!IsClose(d)) return C_CONTINUE; //Not close to line
			polyState = POLYPOINT_SELECTED;
			inx = polyInx==0?points_da.cnt-1:polyInx-1;
			//Find if the point is to be added
			d = FindDistance( points(inx), pos );
			dd = FindDistance( points(inx), points(polyInx) );
			if ( d < 0.25*dd ) {
				polyInx = inx;
			} else if ( d > 0.75*dd ) {
				;
			} else {
				DYNARR_APPEND(coOrd,points_da,1);
				tempSegs(0).u.p.pts = &points(0);
				for (inx=points_da.cnt-1; inx>polyInx; inx-- ) {
					points(inx) = points(inx-1);
				}
				tempSegs(0).u.p.cnt = points_da.cnt;
			}
			//Work out before and after point
			points(polyInx) = pos;
			int last_inx = -1, next_inx = -1;
			if (polyInx == 0) {
				last_inx = points_da.cnt-1;
			} else {
				last_inx = polyInx-1;
			}
			if (polyInx == points_da.cnt-1) {
				next_inx = 0;
			} else {
				next_inx = polyInx+1;
			}
			//Show three anchors only
			CreatePolyAnchors(NULL, polyInx, last_inx, next_inx);
			MainRedraw();
			return C_CONTINUE;
		case C_MOVE:
			tempSegs_da.cnt = 1;
			if (polyState != POLYPOINT_SELECTED) {
				return C_CONTINUE;
			}
			//Moving with Point Selected
			if (polyInx<0) return C_ERROR;
			//Which is first, which last
			last_inx = -1; next_inx = -1;
			if (polyInx == 0) {
				last_inx = points_da.cnt-1;
			} else if (polyInx == 1) {
				last_inx = 0;
			} else {
				last_inx = polyInx-1;
			}
			if (polyInx == points_da.cnt-1) {
				next_inx = 0;
			} else if (polyInx == points_da.cnt-2){
				next_inx = points_da.cnt-1;
			} else {
				next_inx = polyInx+1;
			}
			//Lock to 90 degrees first/last point with SHIFT
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT ) {
				ANGLE_T last_angle,next_angle;
				coOrd last_point,next_point;
				if (polyInx == 0) {
					last_point = points(points_da.cnt-1);
					last_angle = FindAngle(points(points_da.cnt-2),last_point);
				} else if (polyInx == 1) {
					last_point = points(0);
					last_angle = FindAngle(points(points_da.cnt-1),last_point);
				} else {
					last_point = points(polyInx-1);
					last_angle = FindAngle(points(polyInx-2),last_point);
				}
				if (polyInx == points_da.cnt-1) {
					next_point = points(0);
					next_angle = FindAngle(next_point,points(1));
				} else if (polyInx == points_da.cnt-2){
					next_point = points(points_da.cnt-1);
					next_angle = FindAngle(next_point,points(0));
				} else {
					next_point = points(polyInx+1);
					next_angle = FindAngle(next_point,points(polyInx+2));
				}
				//Snap to Right-Angle from previous or from 0
				DIST_T l = FindDistance(last_point, pos);
				ANGLE_T angle2 = NormalizeAngle(FindAngle(last_point, pos)-last_angle);
				int quad = (int)(angle2+45.0)/90.0;
				if (tempSegs_da.cnt != 1 && (quad == 2)) {
					pos = last_point;
				} else if (quad == 1 || quad == 3) {
					l = fabs(l*cos(D2R(((quad==1)?last_angle+90.0:last_angle-90.0)-FindAngle(pos,last_point))));
					Translate( &pos, last_point, NormalizeAngle((quad==1)?last_angle+90.0:last_angle-90.0), l );
				} else {
					l = fabs(l*cos(D2R(((quad==0||quad==4)?last_angle:last_angle+180.0)-FindAngle(pos,last_point))));
					Translate( &pos, last_point, NormalizeAngle((quad==0||quad==4)?last_angle:last_angle+180.0), l );
				}
				coOrd intersect;
				if (FindIntersection(&intersect,last_point,last_angle+90.0,next_point,next_angle+90.0)) {
					d = FindDistance(intersect,pos);
					if (IsClose(d)) pos = intersect;
				}
				InfoMessage( _("Length = %s, Last_Angle = %0.2f"),
						FormatDistance(FindDistance(pos,last_point)),
						PutAngle(FindAngle(pos,last_point)));
			}
			points(polyInx) = pos;
			CreatePolyAnchors(NULL, polyInx, last_inx, next_inx);
			MainRedraw();
			return C_CONTINUE;
		case C_UP:
			if (segInx == -1 || polyState != POLYPOINT_SELECTED)
				return C_CONTINUE;   //Didn't get a point selected/added
			polyState = POLY_SELECTED;  //Return to base state
			CreatePolyAnchors(NULL, polyInx, -1, -1);  //Show last selection
			prev_inx = polyInx;
			polyInx = -1;
			MainRedraw();
			return C_CONTINUE;
		case C_TEXT:
			//Delete or backspace deletes last selected index
			if (action>>8 == 127 || action>>8 == 8) {
				if (polyState == POLY_SELECTED && prev_inx >=0) {
					if (segPtr[segInx].u.p.cnt <= 3) {
						ErrorMessage( MSG_POLY_SHAPES_3_SIDES );
						return C_CONTINUE;
					}
					for (inx=prev_inx+1; inx<points_da.cnt; inx++)
						points(inx-1) = points(inx);
					points_da.cnt--;
					tempSegs(0).u.p.cnt = points_da.cnt;
				}
				prev_inx = -1;
				polyInx = -1;
				polyState = POLY_SELECTED;
				CreatePolyAnchors(NULL, -1, -1, -1);
				MainRedraw();
				return C_CONTINUE;
			}
			if (action>>8 != 32 && action>>8 != 13) return C_CONTINUE;
			//copy changes back into track
			coOrd * oldPts = segPtr[segInx].u.p.pts;
			void * newPts = (coOrd*)MyMalloc( points_da.cnt * sizeof (coOrd) );
			segPtr[segInx].u.p.pts = newPts;
			for (int i=0; i<points_da.cnt; i++) {
				pos = points(i);
				pos.x -= orig.x;
				pos.y -= orig.y;
				Rotate( &pos, zero, -angle );
				segPtr->u.p.pts[i] = pos;
			}
			segPtr->u.p.cnt = points_da.cnt;
			MyFree(oldPts);
			polyState = POLY_NONE;
			DYNARR_RESET(trkSeg_t,anchors_da);
			DYNARR_RESET(trkSeg_t,tempSegs_da);
			MainRedraw();
			return C_TERMINATE;
		case C_REDRAW:
			if (polyState == POLY_NONE) return C_CONTINUE;
			DrawTrack(trk, &mainD, wDrawColorWhite);
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt,trackGauge, wDrawColorBlack);
			DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			break;
		default:
			;
		}
		return C_CONTINUE;
}


STATUS_T DrawGeomModify(
		track_p trk,
		coOrd orig,
		ANGLE_T angle,
		wIndex_t segCnt,
		trkSeg_p segPtr,
		wAction_t action,
		coOrd pos,
		wBool_t selected)
{
	ANGLE_T a;
	coOrd p0, p1, pc;
	static coOrd start_pos;
	static wIndex_t segInx;
	static EPINX_T segEp;
	static ANGLE_T segA1;
	static int inx_other, inx_line, inx_origin;
	static BOOL_T corner_mode;
	static BOOL_T polyMode;
	int inx, inx1, inx2;
	DIST_T d, d1, d2, dd;
	coOrd * newPts = NULL;
	int mergePoints;
	tempSegs_da.cnt = 1;
	wDrawColor save_color;
	switch ( action ) {
	case C_START:
		segInx = -1;
		polyMode = FALSE;
		DistanceSegs( orig, angle, segCnt, segPtr, &pos, &segInx );
		if (segInx == -1)
			return C_ERROR;
		if ((segPtr[segInx].type == SEG_POLY || segPtr[segInx].type == SEG_FILPOLY)
				&& (segPtr[segInx].u.p.polyType !=RECTANGLE)) {
			polyMode = TRUE;
			return DrawGeomPolyModify(trk,orig,angle,segCnt,segPtr,action,pos,selected);
		}
		return C_CONTINUE;
	case C_DOWN:
		if (polyMode) return DrawGeomPolyModify(trk,orig,angle,segCnt,segPtr,action,pos,selected);
		segInx = -1;
		corner_mode = FALSE;
		DistanceSegs( orig, angle, segCnt, segPtr, &pos, &segInx );
		if (segInx == -1)
			return C_ERROR;
		tempSegs(0).width = segPtr[segInx].width;
		tempSegs(0).color = segPtr[segInx].color;
		switch ( segPtr[segInx].type ) {
		case SEG_TBLEDGE:
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
			REORIGIN( p0, segPtr[segInx].u.l.pos[0], angle, orig );
			REORIGIN( p1, segPtr[segInx].u.l.pos[1], angle, orig );
			tempSegs(0).type = segPtr[segInx].type;
			tempSegs(0).u.l.pos[0] = p0;
			tempSegs(0).u.l.pos[1] = p1;
			tempSegs(0).u.l.option = segPtr[segInx].u.l.option;
			segA1 = FindAngle( p1, p0 );
			break;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			REORIGIN( pc, segPtr[segInx].u.c.center, angle, orig )
			tempSegs(0).type = segPtr[segInx].type;
			tempSegs(0).u.c.center = pc;
			tempSegs(0).u.c.radius = fabs(segPtr[segInx].u.c.radius);
			if (segPtr[segInx].u.c.a1 >= 360.0) {
				tempSegs(0).u.c.a0 = 0.0;
				tempSegs(0).u.c.a1 = 360.0;
			} else {
				tempSegs(0).u.c.a0 = NormalizeAngle( segPtr[segInx].u.c.a0+angle );
				tempSegs(0).u.c.a1 = segPtr[segInx].u.c.a1;
				segA1 = NormalizeAngle( segPtr[segInx].u.c.a0 + segPtr[segInx].u.c.a1 + angle );
				PointOnCircle( &p0, pc, fabs(segPtr[segInx].u.c.radius), segPtr[segInx].u.c.a0+angle );
				PointOnCircle( &p1, pc, fabs(segPtr[segInx].u.c.radius), segPtr[segInx].u.c.a0+segPtr[segInx].u.c.a1+angle );
			}
			
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			tempSegs(0).type = segPtr[segInx].type;
			tempSegs(0).u.p.cnt = segPtr[segInx].u.p.cnt;
			tempSegs(0).u.p.angle = 0.0;
			tempSegs(0).u.p.orig = zero;
			tempSegs(0).u.p.polyType = segPtr[segInx].u.p.polyType;
			tempSegs_da.cnt = 1;
			DYNARR_RESET( coOrd, points_da);
			for (int inx=0;inx<segPtr->u.p.cnt;inx++) {
				DYNARR_APPEND(coOrd, points_da,3);
				REORIGIN( points(inx), segPtr[segInx].u.p.pts[inx], angle, orig );
			}
			tempSegs(0).u.p.pts = &points(0);
			tempSegs(0).u.p.cnt = points_da.cnt;
			d = 10000;
			polyInx = 0;
			for ( inx=0; inx<segPtr[segInx].u.p.cnt; inx++ ) {
				p0 = pos;
				dd = LineDistance( &p0, points( inx==0?segPtr[segInx].u.p.cnt-1:inx-1), points( inx ) );
				if ( d > dd ) {
					d = dd;
					polyInx = inx;
				}
			}
			d1 = FindDistance( points(polyInx), pos );
			d2 = FindDistance( points(polyInx==0?segPtr[segInx].u.p.cnt-1:polyInx-1), pos );
			if (d2<d1) {
				inx_line = polyInx;
				polyInx = polyInx==0?segPtr[segInx].u.p.cnt-1:polyInx-1;
			} else {
				inx_line = polyInx==0?segPtr[segInx].u.p.cnt-1:polyInx-1;
			}
			//polyInx is closest point
			inx1 = (polyInx==0?3:polyInx-1);  //Prev point
			inx2 = (polyInx==3?0:polyInx+1);  //Next Point
			inx_origin = (inx2==3?0:inx2+1);  //Opposite point
			if ( IsClose(d2) || IsClose(d1) ) {
				corner_mode = TRUE;
				pos = points(polyInx);
				start_pos = pos;
				DrawArrowHeads( &tempSegs(1), pos, FindAngle(points(polyInx),points(inx_origin)), TRUE, wDrawColorRed );
				tempSegs_da.cnt = 6;
				InfoMessage( _("Drag to Move Corner Point"));
			} else {
				corner_mode = FALSE;
				start_pos = pos;
				pos.x = (points(polyInx).x + points(inx_line).x)/2;
				pos.y = (points(polyInx).y + points(inx_line).y)/2;
				DrawArrowHeads( &tempSegs(1), pos, FindAngle(points(polyInx),pos)+90, TRUE, wDrawColorRed );
				tempSegs_da.cnt = 6;
				InfoMessage( _("Drag to Move Edge "));
			}
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
			switch ( segPtr[segInx].type ) {
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
		return C_CONTINUE;
	case C_MOVE:
		if (polyMode) return DrawGeomPolyModify(trk,orig,angle,segCnt,segPtr,action,pos,selected);
		switch (tempSegs(0).type) {
		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
		case SEG_TBLEDGE:
			if ( MyGetKeyState() & WKEY_SHIFT) {
				d = FindDistance( pos, tempSegs(0).u.l.pos[1-segEp] );
				Translate( &pos, tempSegs(0).u.l.pos[1-segEp], segA1, d );
			} else if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL ) {
				OnTrack( &pos, FALSE, FALSE );
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
			tempSegs(0).u.l.pos[segEp] = pos;
			InfoMessage( _("Length = %0.3f Angle = %0.3f"), FindDistance( tempSegs(0).u.l.pos[segEp], tempSegs(0).u.l.pos[1-segEp] ), FindAngle( tempSegs(0).u.l.pos[1-segEp], tempSegs(0).u.l.pos[segEp] ) );
			break;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			if (tempSegs(0).u.c.a1 >= 360.0) {
				tempSegs(0).u.c.radius = FindDistance( tempSegs(0).u.c.center, pos );
			} else {
				a = FindAngle( tempSegs(0).u.c.center, pos );
				if (segEp==0) {
					tempSegs(0).u.c.a1 = NormalizeAngle(segA1-a);
					tempSegs(0).u.c.a0 = a;
				} else {
					tempSegs(0).u.c.a1 = NormalizeAngle(a-tempSegs(0).u.c.a0);
				}
			}
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			if (!corner_mode) {
				/* Constrain movement to be perpendicular */
				d = FindDistance(start_pos, pos);
				line_angle = NormalizeAngle(FindAngle(points(inx_line),points(polyInx))-90);
				a = FindAngle(pos,start_pos);
				Translate( &pos, start_pos, line_angle, - d*cos(D2R(line_angle-a)));
			}
			d = FindDistance(start_pos,pos);
			a = FindAngle(start_pos, pos);
			start_pos = pos;
			prior_pnt = (polyInx == 0)?3:polyInx-1;
			next_pnt = (polyInx == 3)?0:polyInx+1;
			orig_pnt = (prior_pnt == 0)?3:prior_pnt-1;
			Translate( &points(polyInx), points(polyInx), a, d);
			d = FindDistance(points(orig_pnt),points(polyInx));
			a = FindAngle(points(orig_pnt),points(polyInx));
			prior_angle = FindAngle(points(orig_pnt),points(prior_pnt));
			Translate( &points(prior_pnt), points(orig_pnt), prior_angle, d*cos(D2R(prior_angle-a)));
			next_angle = FindAngle(points(orig_pnt),points(next_pnt));
			Translate( &points(next_pnt), points(orig_pnt), next_angle, d*cos(D2R(next_angle-a)));
			if (!corner_mode) {
				pos.x = (points(polyInx).x + points(inx_line).x)/2;
				pos.y = (points(polyInx).y + points(inx_line).y)/2;
				DrawArrowHeads( &tempSegs(1), pos, FindAngle(points(polyInx),points(inx_line))+90, TRUE, wDrawColorRed );
				tempSegs_da.cnt = 6;
				InfoMessage( _("Drag to Move Edge"));
			} else {
				pos = points(polyInx);
				DrawArrowHeads( &tempSegs(1), pos, FindAngle(points(polyInx),points(inx_origin)), TRUE, wDrawColorRed );
				tempSegs_da.cnt = 6;
				InfoMessage( _("Drag to Move Corner Point"));
			}
			break;
		default:
			;
		}

		return C_CONTINUE;
	case C_UP:
		if (polyMode) return DrawGeomPolyModify(trk,orig,angle,segCnt,segPtr,action,pos,selected);
		if (segInx == -1)
			return C_CONTINUE;
		switch (tempSegs(0).type) {
		case SEG_TBLEDGE:

		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
			pos = tempSegs(0).u.l.pos[segEp];
			pos.x -= orig.x;
			pos.y -= orig.y;
			Rotate( &pos, zero, -angle );
			segPtr[segInx].u.l.pos[segEp] = pos;
			break;
		case SEG_CRVLIN:
		case SEG_FILCRCL:
			if ( tempSegs(0).u.c.a1 >= 360.0 ) {
				segPtr[segInx].u.c.radius = fabs(tempSegs(0).u.c.radius);
			} else {
				a = FindAngle( tempSegs(0).u.c.center, pos );
				a = NormalizeAngle( a-angle );
				segPtr[segInx].u.c.a1 = tempSegs(0).u.c.a1;
				if (segEp == 0) {
					segPtr[segInx].u.c.a0 = a;
				}
			}
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			for (int i=0;i<4;i++) {
				pos = points(i);
				pos.x -= orig.x;
				pos.y -= orig.y;
				Rotate( &pos, zero, -angle );
				segPtr[segInx].u.p.pts[i] = pos;
			}
			break;
		default:
			;
		}
		return C_TERMINATE;
	case C_TEXT:
		if (polyMode) return DrawGeomPolyModify(trk,orig,angle,segCnt,segPtr,action,pos,selected);
		return C_CONTINUE;
	default:
		if (polyMode) return DrawGeomPolyModify(trk,orig,angle,segCnt,segPtr,action,pos,selected);
		;
	}
	return C_ERROR;
}
