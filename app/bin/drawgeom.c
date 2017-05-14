
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

#include <stdarg.h>
#include "track.h"
#include "ccurve.h"
#include "cbezier.h"
#include "compound.h"
#include "drawgeom.h"
#include "i18n.h"

/*EXPORT drawContext_t * drawContext;*/
static long drawGeomCurveMode;

#define contextSegs(N) DYNARR_N( trkSeg_t, context->Segs_da, N )



static dynArr_t points_da;
#define points(N) DYNARR_N( coOrd, points_da, N )

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
	pts = (coOrd*)MyMalloc( (cnt+1) * sizeof *(coOrd*)NULL );
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
		drawContext_t *context )
{
	static int lastValid = FALSE;
	static coOrd pos0, pos0x, pos1, lastPos;
	trkSeg_p segPtr;
	coOrd *pts;
	int inx;
	DIST_T width;
	static int segCnt;
	DIST_T d;
	BOOL_T createTrack;
	long oldOptions;

	width = context->Width/context->D->dpi;

	switch (action&0xFF) {

	case C_START:
		context->State = 0;
		context->Changed = FALSE;
		segCnt = 0;
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		return C_CONTINUE;

	case wActionMove:
		return C_CONTINUE;

	case wActionLDown:
		context->Started = TRUE;
		if ((context->Op == OP_CURVE1 || context->Op == OP_CURVE2 || context->Op == OP_CURVE3 || context->Op == OP_CURVE4) && context->State == 1) {
			;
		} else {
			if ( (MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL )
				OnTrack( &pos, FALSE, FALSE );
			pos0 = pos;
			pos1 = pos;
		}
		switch (context->Op) {
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
			if ( lastValid && ( MyGetKeyState() & WKEY_SHIFT ) ) {
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
			if ( lastValid && ( MyGetKeyState() & WKEY_SHIFT ) ) {
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
		case OP_BEZLIN:
			CmdBezCurve(C_DOWN, pos );
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
		case OP_BEZLIN:
			CmdBezCurve(C_MOVE, pos );
			break;
		case OP_TBLEDGE:
			OnTableEdgeEndPt( NULL, &pos1 );
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
			tempSegs(0).u.l.pos[1] = pos1;
			context->message( _("Length = %s, Angle = %0.2f"),
						FormatDistance(FindDistance( pos0, pos1 )),
						PutAngle(FindAngle( pos0, pos1 )) );
			tempSegs_da.cnt = 1;
			break;
		case OP_POLY:
		case OP_FILLPOLY:
			tempSegs(tempSegs_da.cnt-1).type = SEG_STRLIN;
			tempSegs(tempSegs_da.cnt-1).u.l.pos[1] = pos;
			context->message( _("Length = %s, Angle = %0.2f"),
						FormatDistance(FindDistance( tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos )),
						PutAngle(FindAngle( tempSegs(tempSegs_da.cnt-1).u.l.pos[0], pos )) );
			break;
		case OP_CURVE1: case OP_CURVE2: case OP_CURVE3: case OP_CURVE4:
			if (context->State == 0) {
				pos0x = pos;
				CreateCurve( C_MOVE, pos, TRUE, context->Color, width, drawGeomCurveMode, context->message );
			} else {
				PlotCurve( drawGeomCurveMode, pos0, pos0x, pos1, &context->ArcData, FALSE );
				tempSegs(0).color = context->Color;
				tempSegs(0).width = width;
				if (context->ArcData.type == curveTypeStraight) {
					tempSegs(0).type = SEG_STRLIN;
					tempSegs(0).u.l.pos[0] = pos0;
					tempSegs(0).u.l.pos[1] = context->ArcData.pos1;
					tempSegs_da.cnt = 1;
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
		return C_CONTINUE;

	case wActionLUp:
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		if (context->Op != OP_POLY && context->Op != OP_FILLPOLY)
			DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		lastValid = FALSE;
		createTrack = FALSE;
		switch ( context->Op ) {
		case OP_BEZLIN:
			CmdBezCurve(C_MOVE, pos );
			break;
		case OP_LINE:
		case OP_DIMLINE:
		case OP_BENCH:
		case OP_TBLEDGE:
			lastValid = TRUE;
			lastPos = pos1;
			break;
		case OP_CURVE1: case OP_CURVE2: case OP_CURVE3: case OP_CURVE4:
			if (context->State == 0) {
				context->State = 1;
				context->ArcAngle = FindAngle( pos0, pos1 );
				pos0x = pos1;
				CreateCurve( C_UP, pos, TRUE, context->Color, width, drawGeomCurveMode, context->message );
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
			/*drawContext = context;
			DrawGeomOp( (void*)context->Op );*/
			break;
		case OP_BOX:
		case OP_FILLBOX:
			if ( context->Op == OP_FILLBOX ) {
				pts = (coOrd*)MyMalloc( 4 * sizeof *(coOrd*)NULL );
				for ( inx=0; inx<4; inx++ )
					pts[inx] = tempSegs(inx).u.l.pos[0];
				tempSegs(0).type = SEG_FILPOLY;
				tempSegs(0).u.p.cnt = 4;
				tempSegs(0).u.p.pts = pts;
				tempSegs(0).u.p.angle = 0.0;
				tempSegs(0).u.p.orig = zero;
				tempSegs_da.cnt = 1;
			}
			/*drawContext = context;
			DrawGeomOp( (void*)context->Op );*/
			break;
		case OP_POLY:
		case OP_FILLPOLY:
			segCnt = tempSegs_da.cnt;
			context->D->funcs->options = oldOptions;
			return C_CONTINUE;
		}
		context->Started = FALSE;
		context->Changed = TRUE;
		/*CheckOk();*/
		context->D->funcs->options = oldOptions;
		DrawGeomOk();
		return C_TERMINATE;

	case wActionText:
		if (context->Op == OP_BEZLIN) {
			return CmdBezCurve(C_TEXT,pos);
		}
		if ( ((action>>8)&0xFF) == 0x0D ||
			 ((action>>8)&0xFF) == ' ' ) {
			EndPoly(context, segCnt);
			context->State = 0;
		}
		return C_TERMINATE;

	case C_CANCEL:
		if (context->Op == OP_BEZLIN) {
			return CmdBezCurve(C_CANCEL, pos);
		}
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		context->D->funcs->options = oldOptions;
		tempSegs_da.cnt = 0;
		context->message( "" );
		context->Changed = FALSE;
		lastValid = FALSE;
		return C_TERMINATE;

	case C_REDRAW:
		oldOptions = context->D->funcs->options;
		context->D->funcs->options |= wDrawOptTemp;
		DrawSegs( context->D, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		context->D->funcs->options = oldOptions;
		return C_CONTINUE;
		
	default:
		return C_CONTINUE;
	}
}


STATUS_T DrawGeomModify(
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
	static wIndex_t segInx;
	static EPINX_T segEp;
	static ANGLE_T segA1;
	static int polyInx;
	int inx;
	DIST_T d, dd;
	coOrd * newPts;
	int mergePoints;

	switch ( action ) {
	case C_DOWN:
		segInx = -1;
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
			tempSegs(0).u.c.radius = segPtr[segInx].u.c.radius;
			if (segPtr[segInx].u.c.a1 >= 360.0) {
				tempSegs(0).u.c.a0 = 0.0;
				tempSegs(0).u.c.a1 = 360.0;
			} else {
				tempSegs(0).u.c.a0 = NormalizeAngle( segPtr[segInx].u.c.a0+angle );
				tempSegs(0).u.c.a1 = segPtr[segInx].u.c.a1;
				segA1 = NormalizeAngle( segPtr[segInx].u.c.a0 + segPtr[segInx].u.c.a1 + angle );
				PointOnCircle( &p0, pc, segPtr[segInx].u.c.radius, segPtr[segInx].u.c.a0+angle );
				PointOnCircle( &p1, pc, segPtr[segInx].u.c.radius, segPtr[segInx].u.c.a0+segPtr[segInx].u.c.a1+angle );
			}
			
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			tempSegs(0).type = segPtr[segInx].type;
			tempSegs(0).u.p.cnt = segPtr[segInx].u.p.cnt;
			tempSegs(0).u.p.angle = 0.0;
			tempSegs(0).u.p.orig = zero;
			DYNARR_SET( coOrd, points_da, segPtr[segInx].u.p.cnt+1 );
			tempSegs(0).u.p.pts = &points(0);
			d = 10000;
			polyInx = 0;
			for ( inx=0; inx<segPtr[segInx].u.p.cnt; inx++ ) {
				REORIGIN( points(inx), segPtr[segInx].u.p.pts[inx], angle, orig );
			}
			for ( inx=0; inx<segPtr[segInx].u.p.cnt; inx++ ) {
				p0 = pos;
				dd = LineDistance( &p0, points( inx==0?segPtr[segInx].u.p.cnt-1:inx-1), points( inx ) );
				if ( d > dd ) {
					d = dd;
					polyInx = inx;
				}
			}
			inx = (polyInx==0?segPtr[segInx].u.p.cnt-1:polyInx-1);
			d = FindDistance( points(inx), pos );
			dd = FindDistance( points(inx), points(polyInx) );
			if ( d < 0.25*dd ) {
				polyInx = inx;
			} else if ( d > 0.75*dd ) {
				;
			} else {
				tempSegs(0).u.p.cnt++;
				for (inx=points_da.cnt-1; inx>polyInx; inx-- ) {
					points(inx) = points(inx-1);
				}
/*fprintf( stderr, "Inserting vertix before %d\n", polyInx );*/
			}
			points(polyInx) = pos;
			p1=p0;
			break;
		default:
			ASSERT( FALSE ); /* CHECKME */
		case SEG_TEXT:
			segInx = -1;
			return C_ERROR;
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
		tempSegs_da.cnt = 1;
		return C_CONTINUE;
	case C_MOVE:
		if (segInx == -1)
			return C_ERROR;
		if ( ( MyGetKeyState() & WKEY_SHIFT ) &&
			 (tempSegs(0).type == SEG_STRLIN || tempSegs(0).type == SEG_DIMLIN || tempSegs(0).type == SEG_BENCH || tempSegs(0).type == SEG_TBLEDGE) ) {
			d = FindDistance( pos, tempSegs(0).u.l.pos[1-segEp] );
			Translate( &pos, tempSegs(0).u.l.pos[1-segEp], segA1, d );
		} else if ( (MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_CTRL ) {
			OnTrack( &pos, FALSE, FALSE );
		}
		switch (tempSegs(0).type) {
		case SEG_TBLEDGE:

		case SEG_STRLIN:
		case SEG_DIMLIN:
		case SEG_BENCH:
			tempSegs(0).u.l.pos[segEp] = pos;
			InfoMessage( _("Length = %0.3f Angle = %0.3f"), FindDistance( tempSegs(0).u.l.pos[segEp], tempSegs(0).u.l.pos[1-segEp] ), FindAngle( tempSegs(0).u.l.pos[1-segEp], tempSegs(0).u.l.pos[segEp] ) );
			break;
			pos.x -= orig.x;
			pos.y -= orig.y;
			pos.x -= orig.x;
			pos.y -= orig.y;
			Rotate( &pos, zero, -angle );
			Rotate( &pos, zero, -angle );
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
			points(polyInx) = pos;
			break;
		default:
			;
		}
		tempSegs_da.cnt = 1;
		return C_CONTINUE;
	case C_UP:
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
				segPtr[segInx].u.c.radius = tempSegs(0).u.c.radius;
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
			mergePoints = FALSE;
			if ( IsClose( FindDistance( pos, points( polyInx==0?tempSegs(0).u.p.cnt-1:polyInx-1 ) ) ) ||
				 IsClose( FindDistance( pos, points( (polyInx==tempSegs(0).u.p.cnt-1)?0:polyInx+1 ) ) ) ) {
				mergePoints = TRUE;
				if (segPtr[segInx].u.p.cnt <= 3) {
					ErrorMessage( MSG_POLY_SHAPES_3_SIDES );
					break;
				}
			}

			newPts = (coOrd*)MyMalloc( tempSegs(0).u.p.cnt * sizeof *(coOrd*)0 );
			memcpy( newPts, segPtr[segInx].u.p.pts, (segPtr[segInx].u.p.cnt) * sizeof *(coOrd*)0 );
			segPtr[segInx].u.p.pts = newPts;

			if ( tempSegs(0).u.p.cnt > segPtr[segInx].u.p.cnt ) {
				ASSERT( tempSegs(0).u.p.cnt == segPtr[segInx].u.p.cnt+1 );
				for (inx=tempSegs(0).u.p.cnt-1; inx>polyInx; inx--)
					segPtr[segInx].u.p.pts[inx] = segPtr[segInx].u.p.pts[inx-1];
				segPtr[segInx].u.p.cnt++;
			}
		
			pos = points(polyInx);
			if ( mergePoints ) {
				for (inx=polyInx+1; inx<points_da.cnt; inx++)
					segPtr[segInx].u.p.pts[inx-1] = segPtr[segInx].u.p.pts[inx];
				segPtr[segInx].u.p.cnt--;
/*fprintf( stderr, "Merging with vertix %d\n", polyInx );*/
				break;
			}
			pos.x -= orig.x;
			pos.y -= orig.y;
			Rotate( &pos, zero, -angle );
			segPtr[segInx].u.p.pts[polyInx] = pos;
			break;
		default:
			;
		}
		return C_TERMINATE;
	default:
		;
	}
	return C_ERROR;
}
