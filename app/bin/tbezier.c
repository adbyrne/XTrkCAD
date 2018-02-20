/** \file tbezier.c
 *  XTrkCad - Model Railroad CAD
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
 *
 *****************************************************************************
 * BEZIER TRACK (and LINE)
 *
 * tbezier.c has all the Track functions (for both T_BEZIER and T_BEZLIN) but all the heavy-math-lifting is delegated to cbezier.c
 *
 * Both Bezier Tracks and Lines are defined with two end points and two control points. Each end and control point pair is joined by a control arm.
 * The angle between the control and end point (arm angle) determines the angle at the end point.
 * The way the curve moves in between the ends is driven by the relative lengths of the two control arms.
 * In general, lengthening one arm while keeping the other arm length fixed results in a curve that changes more slowly from the lengthened end and more swiftly from the other.
 * Very un-prototypical track curves are easy to draw with Bezier, so beware!
 *
 * Another large user of tbezier.c is the Cornu function by way of its support for Bezier segments, which are used to approximate Cornu curves.
 *
 * In XTrkcad, Bezier curves are rendered into a set of Curved and Straight segments for display. This set is also saved, although the code recalculates a fresh set upon reload.
 *
 */


#include "track.h"
#include "draw.h"
#include "tbezier.h"
#include "cbezier.h"
#include "ccurve.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "utility.h"
#include "i18n.h"
#include "param.h"
#include "math.h"
#include "string.h"
#include "cundo.h"
#include "layout.h"
#include "fileio.h"
#include "assert.h"

EXPORT TRKTYP_T T_BEZIER = -1;
EXPORT TRKTYP_T T_BZRLIN = -1;


struct extraData {
		BezierData_t bezierData;
		};

static int log_bezier = 0;

static DIST_T GetLengthBezier( track_p );

/****************************************
 *
 * UTILITIES
 *
 */

/*
 * Run after any changes to the Bezier points
 */
EXPORT void FixUpBezier(coOrd pos[4], struct extraData* xx, BOOL_T track) {
	xx->bezierData.a0 = NormalizeAngle(FindAngle(pos[1], pos[0]));
	xx->bezierData.a1 = NormalizeAngle(FindAngle(pos[2], pos[3]));

	ConvertToArcs(pos, &xx->bezierData.arcSegs, track, xx->bezierData.segsColor,
			xx->bezierData.segsWidth);
	xx->bezierData.minCurveRadius = BezierMinRadius(pos,
			xx->bezierData.arcSegs);
	xx->bezierData.length = BezierLength(pos, xx->bezierData.arcSegs);
}

/*
 * Run after any changes to the Bezier points for a Segment
 */
EXPORT void FixUpBezierSeg(coOrd pos[4], trkSeg_p p, BOOL_T track) {
	p->u.b.angle0 = NormalizeAngle(FindAngle(pos[1], pos[0]));
	p->u.b.angle3 = NormalizeAngle(FindAngle(pos[2], pos[3]));
	ConvertToArcs(pos, &p->bezSegs, track, p->color,
			p->width);
	p->u.b.minRadius = BezierMinRadius(pos,
			p->bezSegs);
	p->u.b.length = BezierLength(pos, p->bezSegs);
}

EXPORT void FixUpBezierSegs(trkSeg_p p,int segCnt) {
	for (int i=0;i<segCnt;i++,p++) {
		if (p->type == SEG_BEZTRK || p->type == SEG_BEZLIN) {
			FixUpBezierSeg(p->u.b.pos,p,p->type == SEG_BEZTRK);
		}
	}
}


static void GetBezierAngles( ANGLE_T *a0, ANGLE_T *a1, track_p trk )
{
    assert( trk != NULL );
    
        *a0 = NormalizeAngle( GetTrkEndAngle(trk,0) );
        *a1 = NormalizeAngle( GetTrkEndAngle(trk,1)  );
    
    LOG( log_bezier, 4, ( "getBezierAngles: = %0.3f %0.3f\n", *a0, *a1 ) )
}


static void ComputeBezierBoundingBox( track_p trk, struct extraData * xx )
{
    coOrd hi, lo;
    hi.x = lo.x = xx->bezierData.pos[0].x;
    hi.y = lo.y = xx->bezierData.pos[0].y;
    
    for (int i=1; i<=3;i++) {
        hi.x = hi.x < xx->bezierData.pos[i].x ? xx->bezierData.pos[i].x : hi.x;
        hi.y = hi.y < xx->bezierData.pos[i].y ? xx->bezierData.pos[i].y : hi.y;
        lo.x = lo.x > xx->bezierData.pos[i].x ? xx->bezierData.pos[i].x : lo.x;
        lo.y = lo.y > xx->bezierData.pos[i].y ? xx->bezierData.pos[i].y : lo.y;
    }
    SetBoundingBox( trk, hi, lo );
}


DIST_T BezierDescriptionDistance(
		coOrd pos,
		track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	coOrd p1;

	if ( GetTrkType( trk ) != T_BEZIER || ( GetTrkBits( trk ) & TB_HIDEDESC ) != 0 )
		return 100000;
	
		p1.x = xx->bezierData.pos[0].x + (xx->bezierData.pos[3].x-xx->bezierData.pos[0].x)/2 + xx->bezierData.descriptionOff.x;
		p1.y = xx->bezierData.pos[0].y + (xx->bezierData.pos[3].y-xx->bezierData.pos[0].y)/2 + xx->bezierData.descriptionOff.y;
	
	return FindDistance( p1, pos );
}


static void DrawBezierDescription(
		track_p trk,
		drawCmd_p d,
		wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(trk);
	wFont_p fp;
    coOrd pos;

	if (layoutLabels == 0)
		return;
	if ((labelEnable&LABELENABLE_TRKDESC)==0)
		return;
    pos.x = xx->bezierData.pos[0].x + (xx->bezierData.pos[3].x - xx->bezierData.pos[0].x)/2;
    pos.y = xx->bezierData.pos[0].y + (xx->bezierData.pos[3].y - xx->bezierData.pos[0].y)/2;
    pos.x += xx->bezierData.descriptionOff.x;
    pos.y += xx->bezierData.descriptionOff.y;
    fp = wStandardFont( F_TIMES, FALSE, FALSE );
    sprintf( message, _("Bezier Curve: length=%s min radius=%s"),
				FormatDistance(xx->bezierData.length), FormatDistance(xx->bezierData.minCurveRadius));
    DrawBoxedString( BOX_BOX, d, pos, message, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
}


STATUS_T BezierDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	struct extraData *xx = GetTrkExtraData(trk);
	static coOrd p0;
	wDrawColor color;
	if (GetTrkType(trk) != T_BEZIER) return C_TERMINATE;

	switch (action) {
	case C_DOWN:
	case C_MOVE:
	case C_UP:
		color = GetTrkColor( trk, &mainD );
		DrawBezierDescription( trk, &mainD, color );
    
        if (action != C_DOWN)
            DrawLine( &mainD, xx->bezierData.pos[0], p0, 0, wDrawColorBlack );
        xx->bezierData.descriptionOff.x = p0.x - xx->bezierData.pos[0].x + (xx->bezierData.pos[3].x-xx->bezierData.pos[0].x)/2;
        xx->bezierData.descriptionOff.y = p0.y - xx->bezierData.pos[0].x + (xx->bezierData.pos[3].y-xx->bezierData.pos[0].y)/2;
        p0 = pos;
        if (action != C_UP)
            DrawLine( &mainD, xx->bezierData.pos[0], p0, 0, wDrawColorBlack );
        DrawBezierDescription( trk, &mainD, color );
        MainRedraw();
        MapRedraw();
		return action==C_UP?C_TERMINATE:C_CONTINUE;

	case C_REDRAW:
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
		coOrd pos[4];
		FLOAT_T elev[2];
		FLOAT_T length;
		DIST_T minRadius;
		FLOAT_T grade;
		unsigned int layerNumber;
		ANGLE_T angle[2];
		DIST_T radius[2];
		coOrd center[2];
		dynArr_t segs;
		long width;
		wDrawColor color;
		} bezData;
typedef enum { P0, A0, R0, C0, Z0, CP1, CP2, P1, A1, R1, C1, Z1, RA, LN, GR, LY, WI, CO } crvDesc_e;
static descData_t bezDesc[] = {
/*P0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &bezData.pos[0] },
/*A0*/  { DESC_ANGLE, N_("End Angle"), &bezData.angle[0] },
/*R0*/  { DESC_DIM, N_("Radius"), &bezData.radius[0] },
/*C0*/	{ DESC_POS, N_("Center X,Y"), &bezData.center[0]},
/*Z0*/	{ DESC_DIM, N_("Z1"), &bezData.elev[0] },
/*CP1*/	{ DESC_POS, N_("Ctl Pt 1: X,Y"), &bezData.pos[1] },
/*CP2*/	{ DESC_POS, N_("Ctl Pt 2: X,Y"), &bezData.pos[2] },
/*P1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &bezData.pos[3] },
/*A1*/  { DESC_ANGLE, N_("End Angle"), &bezData.angle[1] },
/*R1*/  { DESC_DIM, N_("Radius"), &bezData.radius[1] },
/*C1*/	{ DESC_POS, N_("Center X,Y"), &bezData.center[1]},
/*Z1*/	{ DESC_DIM, N_("Z2"), &bezData.elev[1] },
/*RA*/	{ DESC_DIM, N_("MinRadius"), &bezData.radius },
/*LN*/	{ DESC_DIM, N_("Length"), &bezData.length },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &bezData.grade },
/*LY*/	{ DESC_LAYER, N_("Layer"), &bezData.layerNumber },
/*WI*/  { DESC_LONG, N_("Line Width"), &bezData.width},
/*CO*/  { DESC_COLOR, N_("Line Color"), &bezData.color},
		{ DESC_NULL } };

static void UpdateBezier( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	struct extraData *xx = GetTrkExtraData(trk);
	BOOL_T updateEndPts;
	EPINX_T ep;
	ANGLE_T angle1, angle2;

	if ( inx == -1 )
		return;
	updateEndPts = FALSE;
	switch ( inx ) {
    case P0:
        if (GetTrkEndTrk(trk,0)) break;
    	updateEndPts = TRUE;
        xx->bezierData.pos[0] = bezData.pos[0];
        bezDesc[P0].mode |= DESC_CHANGE;
        /* no break */
    case P1:
    	if (GetTrkEndTrk(trk,0) && GetTrkEndTrk(trk,1)) break;
        updateEndPts = TRUE;
        xx->bezierData.pos[3]= bezData.pos[3];
        bezDesc[P1].mode |= DESC_CHANGE;
        break;
    case A0:
    case A1:
    	break;
    case CP1:
    	if (GetTrkEndTrk(trk,0)) {
    		angle1 = NormalizeAngle(GetTrkEndAngle(trk,0));
    	    angle2 = NormalizeAngle(FindAngle(bezData.pos[1], xx->bezierData.pos[0])-angle1);
    		if (angle2 > 90.0 && angle2 < 270.0)
    		  Translate( &bezData.pos[1], xx->bezierData.pos[0], angle1, -FindDistance( xx->bezierData.pos[0], bezData.pos[1] )*cos(D2R(angle2)));
    	}
        xx->bezierData.pos[1] = bezData.pos[1];
        bezDesc[CP1].mode |= DESC_CHANGE;
        updateEndPts = TRUE;
        break;
    case CP2:
    	if (GetTrkEndTrk(trk,1)) {
    	    angle1 = NormalizeAngle(GetTrkEndAngle(trk,1));
    	    angle2 = NormalizeAngle(FindAngle(bezData.pos[2], xx->bezierData.pos[3])-angle1);
    	    if (angle2 > 90.0 && angle2 < 270.0)
    	    Translate( &bezData.pos[2], xx->bezierData.pos[3], angle1, -FindDistance( xx->bezierData.pos[3], bezData.pos[0] )*cos(D2R(angle2)));
    	}
        xx->bezierData.pos[2] = bezData.pos[2];
        bezDesc[CP2].mode |= DESC_CHANGE;
        updateEndPts = TRUE;
        break;
    case Z0:
	case Z1:
		ep = (inx==Z0?0:1);
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), bezData.elev[ep], NULL );
		ComputeElev( trk, 1-ep, FALSE, &bezData.elev[1-ep], NULL );
		if ( bezData.length > minLength )
			bezData.grade = fabs( (bezData.elev[0]-bezData.elev[1])/bezData.length )*100.0;
		else
			bezData.grade = 0.0;
		bezDesc[GR].mode |= DESC_CHANGE;
		bezDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
        return;
	case LY:
		SetTrkLayer( trk, bezData.layerNumber);
		break;
	case WI:
		xx->bezierData.segsWidth = bezData.width/mainD.dpi;
		break;
	case CO:
		xx->bezierData.segsColor = bezData.color;
		break;
	default:
		AbortProg( "updateBezier: Bad inx %d", inx );
	}
	ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, IsTrack(trk)?TRUE:FALSE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
	trackParams_t params;
	    for (int i=0;i<2;i++) {
	    	GetTrackParams(0,trk,xx->bezierData.pos[i],&params);
	    	bezData.radius[i] = params.arcR;
	    	bezData.center[i] = params.arcP;
	    }
	if (updateEndPts) {
			if ( GetTrkEndTrk(trk,0) == NULL ) {
				SetTrkEndPoint( trk, 0, bezData.pos[0], NormalizeAngle( FindAngle(bezData.pos[0], bezData.pos[1]) ) );
				bezData.angle[0] = GetTrkEndAngle(trk,0);
				bezDesc[A0].mode |= DESC_CHANGE;
				GetTrackParams(PARAMS_CORNU,trk,xx->bezierData.pos[0],&params);
			    bezData.radius[0] = params.arcR;
				bezData.center[0] = params.arcP;
			}
			if ( GetTrkEndTrk(trk,1) == NULL ) {
				SetTrkEndPoint( trk, 1, bezData.pos[3], NormalizeAngle( FindAngle(bezData.pos[2], bezData.pos[3]) ) );
				bezData.angle[1] = GetTrkEndAngle(trk,1);
				bezDesc[A1].mode |= DESC_CHANGE;
				GetTrackParams(PARAMS_CORNU,trk,xx->bezierData.pos[1],&params);
				bezData.radius[1] = params.arcR;
				bezData.center[1] = params.arcP;
			}
	}

	FixUpBezier(xx->bezierData.pos, xx, IsTrack(trk));
	ComputeBezierBoundingBox(trk, xx);
	DrawNewTrack( trk );
}

static void DescribeBezier( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T d;
	int fix0, fix1 = 0;
	
	d = xx->bezierData.length;
    sprintf( str, _("Bezier %s(%d): Layer=%u MinRadius=%s Length=%s EP=[%0.3f,%0.3f] [%0.3f,%0.3f] CP1=[%0.3f,%0.3f] CP2=[%0.3f, %0.3f]"),
				GetTrkType(trk)==T_BEZIER?"Track":"Line",
    			GetTrkIndex(trk),
				GetTrkLayer(trk)+1,
				FormatDistance(xx->bezierData.minCurveRadius),
				FormatDistance(d),
				PutDim(xx->bezierData.pos[0].x),PutDim(xx->bezierData.pos[0].y),
				PutDim(xx->bezierData.pos[3].x),PutDim(xx->bezierData.pos[3].y),
                PutDim(xx->bezierData.pos[1].x),PutDim(xx->bezierData.pos[1].y),
                PutDim(xx->bezierData.pos[2].x),PutDim(xx->bezierData.pos[2].y));

	if (GetTrkType(trk) == T_BEZIER) {
		fix0 = GetTrkEndTrk(trk,0)!=NULL;
		fix1 = GetTrkEndTrk(trk,1)!=NULL;
	}

	bezData.length = GetLengthBezier(trk);
	bezData.minRadius = xx->bezierData.minCurveRadius;
	if (bezData.minRadius >= 100000.00) bezData.minRadius = 0;
    bezData.layerNumber = GetTrkLayer(trk);
    bezData.pos[0] = xx->bezierData.pos[0];
    bezData.pos[1] = xx->bezierData.pos[1];
    bezData.pos[2] = xx->bezierData.pos[2];
    bezData.pos[3] = xx->bezierData.pos[3];
    bezData.angle[0] = xx->bezierData.a0;
    bezData.angle[1] = xx->bezierData.a1;
    trackParams_t params;
    GetTrackParams(PARAMS_CORNU,trk,xx->bezierData.pos[0],&params);
    bezData.radius[0] = params.arcR;
    bezData.center[0] = params.arcP;
    GetTrackParams(PARAMS_CORNU,trk,xx->bezierData.pos[3],&params);
    bezData.radius[1] = params.arcR;
    bezData.center[1] = params.arcP;

    if (GetTrkType(trk) == T_BEZIER) {
		ComputeElev( trk, 0, FALSE, &bezData.elev[0], NULL );
		ComputeElev( trk, 1, FALSE, &bezData.elev[1], NULL );
	
		if ( bezData.length > minLength )
			bezData.grade = fabs( (bezData.elev[0]-bezData.elev[1])/bezData.length )*100.0;
		else
			bezData.grade = 0.0;
    }

	bezDesc[P0].mode = fix0?DESC_RO:0;
	bezDesc[P1].mode = fix1?DESC_RO:0;
	bezDesc[LN].mode = DESC_RO;
    bezDesc[CP1].mode = 0;
    bezDesc[CP2].mode = 0;
    if (GetTrkType(trk) == T_BEZIER) {
    	bezDesc[Z0].mode = EndPtIsDefinedElev(trk,0)?0:DESC_RO;
		bezDesc[Z1].mode = EndPtIsDefinedElev(trk,1)?0:DESC_RO;
    }
    else
    	bezDesc[Z0].mode = bezDesc[Z1].mode = DESC_IGNORE;
	bezDesc[A0].mode = DESC_RO;
	bezDesc[A1].mode = DESC_RO;
	bezDesc[C0].mode = DESC_RO;
	bezDesc[C1].mode = DESC_RO;
	bezDesc[R0].mode = DESC_RO;
	bezDesc[R1].mode = DESC_RO;
	bezDesc[GR].mode = DESC_RO;
    bezDesc[RA].mode = DESC_RO;
	bezDesc[LY].mode = DESC_NOREDRAW;
	bezData.width = (long)floor(xx->bezierData.segsWidth*mainD.dpi+0.5);
	bezDesc[WI].mode = GetTrkType(trk) == T_BEZIER?DESC_IGNORE:0;
	bezData.color = xx->bezierData.segsColor;
	bezDesc[CO].mode = GetTrkType(trk) == T_BEZIER?DESC_IGNORE:0;
	
	if (GetTrkType(trk) == T_BEZIER)
		DoDescribe( _("Bezier Track"), trk, bezDesc, UpdateBezier );
	else
		DoDescribe( _("Bezier Line"), trk, bezDesc, UpdateBezier );

}

static DIST_T DistanceBezier( track_p t, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(t);

	DIST_T d = 100000.0;
	coOrd p2 = xx->bezierData.pos[0];    //Set initial point
	segProcData_t segProcData;
	for (int i = 0;i<xx->bezierData.arcSegs.cnt;i++) {
		segProcData.distance.pos1 = * p;
		SegProc(SEGPROC_DISTANCE,&DYNARR_N(trkSeg_t,xx->bezierData.arcSegs,i),&segProcData);
		if (segProcData.distance.dd<d) {
			d = segProcData.distance.dd;
			p2 = segProcData.distance.pos1;
		}
	}
    * p = p2;
	return d;
}

static void DrawBezier( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);
	long widthOptions = DTS_LEFT|DTS_RIGHT;


	if (GetTrkType(t) == T_BZRLIN) {
		DrawSegsO(d,t,zero,0.0,xx->bezierData.arcSegs.ptr,xx->bezierData.arcSegs.cnt, 0.0, color, 0);
		return;
	}

	if (GetTrkWidth(t) == 2)
		widthOptions |= DTS_THICK2;
	if (GetTrkWidth(t) == 3)
		widthOptions |= DTS_THICK3;
	

	if ( ((d->funcs->options&wDrawOptTemp)==0) &&
		 (labelWhen == 2 || (labelWhen == 1 && (d->options&DC_PRINT))) &&
		 labelScale >= d->scale &&
		 ( GetTrkBits( t ) & TB_HIDEDESC ) == 0 ) {
		DrawBezierDescription( t, d, color );
	}
	DIST_T scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;
	if ( tieDrawMode!=TIEDRAWMODE_NONE &&
			 d!=&mapD &&
			 (d->options&DC_TIES)!=0 &&
			 d->scale<scale2rail/2 )
		DrawSegsO(d,t,zero,0.0,xx->bezierData.arcSegs.ptr,xx->bezierData.arcSegs.cnt, GetTrkGauge(t), color, widthOptions|DTS_TIES);
	DrawSegsO(d,t,zero,0.0,xx->bezierData.arcSegs.ptr,xx->bezierData.arcSegs.cnt, GetTrkGauge(t), color, widthOptions);
	if ( (d->funcs->options & wDrawOptTemp) == 0 &&
		 (d->options&DC_QUICK) == 0 ) {
		DrawEndPt( d, t, 0, color );
		DrawEndPt( d, t, 1, color );
	}
}

static void DeleteBezier( track_p t )
{
	struct extraData *xx = GetTrkExtraData(t);

	for (int i=0;i<xx->bezierData.arcSegs.cnt;i++) {
		trkSeg_t s = DYNARR_N(trkSeg_t,xx->bezierData.arcSegs,i);
		if (s.type == SEG_BEZTRK || s.type == SEG_BEZLIN) {
			if (s.bezSegs.ptr) MyFree(s.bezSegs.ptr);
			s.bezSegs.max = 0;
			s.bezSegs.cnt = 0;
		}
	}
	if (xx->bezierData.arcSegs.ptr && !xx->bezierData.arcSegs.max)
		MyFree(xx->bezierData.arcSegs.ptr);
	xx->bezierData.arcSegs.max = 0;
	xx->bezierData.arcSegs.cnt = 0;
	xx->bezierData.arcSegs.ptr = NULL;
}

static BOOL_T WriteBezier( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	long options;
	BOOL_T rc = TRUE;
	BOOL_T track =(GetTrkType(t)==T_BEZIER);
	options = GetTrkWidth(t) & 0x0F;
	rc &= fprintf(f, "%s %d %u %ld %ld %0.6f %s %d %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f 0 %0.6f %0.6f \n",
		track?"BEZIER":"BZRLIN",GetTrkIndex(t), GetTrkLayer(t), (long)options, wDrawGetRGB(xx->bezierData.segsColor), xx->bezierData.segsWidth,
                  GetTrkScaleName(t), GetTrkVisible(t),
				  xx->bezierData.pos[0].x, xx->bezierData.pos[0].y,
				  xx->bezierData.pos[1].x, xx->bezierData.pos[1].y,
				  xx->bezierData.pos[2].x, xx->bezierData.pos[2].y,
				  xx->bezierData.pos[3].x, xx->bezierData.pos[3].y,
				  xx->bezierData.descriptionOff.x, xx->bezierData.descriptionOff.y )>0;
	if (track) {
			rc &= WriteEndPt( f, t, 0 );
			rc &= WriteEndPt( f, t, 1 );
		}
	rc &= WriteSegs( f, xx->bezierData.arcSegs.cnt, xx->bezierData.arcSegs.ptr );
	return rc;
}

static void ReadBezier( char * line )
{
	struct extraData *xx;
	track_p t;
	wIndex_t index;
	BOOL_T visible;
	coOrd p0, c1, c2, p1, dp;
	char scale[10];
	wIndex_t layer;
	long options;
	char * cp = NULL;
	unsigned long rgb;
	DIST_T width;

	if (!GetArgs( line+6, "dLluwsdpppp0p",
		&index, &layer, &options, &rgb, &width, scale, &visible, &p0, &c1, &c2, &p1, &dp ) ) {
		return;
	}
	if (strncmp(line,"BEZIER",6)==0)
		t = NewTrack( index, T_BEZIER, 0, sizeof *xx );
	else
		t = NewTrack( index, T_BZRLIN, 0, sizeof *xx );
	xx = GetTrkExtraData(t);
	SetTrkVisible(t, visible);
	SetTrkScale(t, LookupScale(scale));
	SetTrkLayer(t, layer );
	SetTrkWidth(t, (int)(options&0x0F));
	xx->bezierData.pos[0] = p0;
    xx->bezierData.pos[1] = c1;
    xx->bezierData.pos[2] = c2;
    xx->bezierData.pos[3] = p1;
    xx->bezierData.descriptionOff = dp;
    xx->bezierData.segsWidth = width;
    xx->bezierData.segsColor = wDrawFindColor( rgb );
    ReadSegs();
    FixUpBezier(xx->bezierData.pos,xx,GetTrkType(t) == T_BEZIER);
    ComputeBezierBoundingBox(t,xx);
    if (GetTrkType(t) == T_BEZIER) {
		SetEndPts(t,2);
	}
}

static void MoveBezier( track_p trk, coOrd orig )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
    for (int i=0;i<4;i++) {
        xx->bezierData.pos[i].x += orig.x;
        xx->bezierData.pos[i].y += orig.y;
    }
    FixUpBezier(xx->bezierData.pos,xx,IsTrack(trk));
    ComputeBezierBoundingBox(trk,xx);

}

static void RotateBezier( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
    for (int i=0;i<5;i++) {
        Rotate( &xx->bezierData.pos[i], orig, angle );
    }
    FixUpBezier(xx->bezierData.pos,xx,IsTrack(trk));
    ComputeBezierBoundingBox(trk,xx);

}

static void RescaleBezier( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
	xx->bezierData.pos[0].x *= ratio;
	xx->bezierData.pos[0].y *= ratio;
    xx->bezierData.pos[1].x *= ratio;
    xx->bezierData.pos[1].y *= ratio;
    xx->bezierData.pos[2].x *= ratio;
    xx->bezierData.pos[2].y *= ratio;
    xx->bezierData.pos[3].x *= ratio;
    xx->bezierData.pos[3].y *= ratio;
    FixUpBezier(xx->bezierData.pos,xx,IsTrack(trk));
    ComputeBezierBoundingBox(trk,xx);

}

EXPORT void AdjustBezierEndPt( track_p trk, EPINX_T inx, coOrd pos ) {
    struct extraData *xx = GetTrkExtraData(trk);
    UndoModify(trk);
    if (inx ==0 ) {
        xx->bezierData.pos[1].x += -xx->bezierData.pos[0].x+pos.x;
        xx->bezierData.pos[1].y += -xx->bezierData.pos[0].y+pos.y;
        xx->bezierData.pos[0] = pos;
    }
    else {
        xx->bezierData.pos[2].x += -xx->bezierData.pos[3].x+pos.x;
        xx->bezierData.pos[2].y += -xx->bezierData.pos[3].y+pos.y;
        xx->bezierData.pos[3] = pos;
    }
    FixUpBezier(xx->bezierData.pos, xx, IsTrack(trk));
    ComputeBezierBoundingBox(trk,xx);
    SetTrkEndPoint( trk, inx, pos, inx==0?xx->bezierData.a0:xx->bezierData.a1);
}


/**
 * Split the Track at approximately the point pos.
 */
static BOOL_T SplitBezier( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	track_p trk1;
    double t;
    BOOL_T track;
    track = IsTrack(trk);
    
    coOrd current[4], newl[4], newr[4];

    double dd = DistanceBezier(trk, &pos);
    if (dd>minLength) return FALSE;
    
    BezierMathDistance(&pos, xx->bezierData.pos, 500, &t);  //Find t value

    for (int i=0;i<4;i++) {
    	current[i] = xx->bezierData.pos[i];
    }

    BezierSplit(current, newl, newr, t);

    if (track) {
    	trk1 = NewBezierTrack(ep?newr:newl,NULL,0);
    } else
    	trk1 = NewBezierLine(ep?newr:newl,NULL,0, xx->bezierData.segsColor,xx->bezierData.segsWidth);
	UndoModify(trk);
    for (int i=0;i<4;i++) {
    	xx->bezierData.pos[i] = ep?newl[i]:newr[i];
    }
    FixUpBezier(xx->bezierData.pos,xx,track);
    ComputeBezierBoundingBox(trk,xx);
    SetTrkEndPoint( trk, ep, xx->bezierData.pos[ep?3:0], ep?xx->bezierData.a1:xx->bezierData.a0);

	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;

	return TRUE;
}

static int log_traverseBezier = 0;
static int log_bezierSegments = 0;
/*
 * TraverseBezier is used to position a train/car.
 * We find a new position and angle given a current pos, angle and a distance to travel.
 *
 * The output can be TRUE -> we have moved the point to a new point or to the start/end of the next track
 * 					FALSE -> we have not found that point because pos was not on/near the track
 *
 * 	If true we supply the remaining distance to go (always positive).
 *  We detect the movement direction by comparing the current angle to the angle of the track at the point.
 *
 *  Each segment may be processed forwards or in reverse (this really only applies to curved segments).
 *  So for each segment we call traverse1 to get the direction and extra distance to go to get to the current point
 *  and then use that for traverse2 to actually move to the new point
 *
 *  If we exceed the current point's segment we move on to the next until the end of this track or we have found the spot.
 *
 */
static BOOL_T TraverseBezier( traverseTrack_p trvTrk, DIST_T * distR )
{
    track_p trk = trvTrk->trk;
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T dist = *distR;
	segProcData_t segProcData;
	BOOL_T segs_backwards= FALSE;
	DIST_T d = 10000;
	coOrd pos2 = trvTrk->pos;
	ANGLE_T a1,a2;
	int inx,segInx = 0;
	EPINX_T ep;
	BOOL_T back,neg;
	trkSeg_p segPtr = (trkSeg_p)xx->bezierData.arcSegs.ptr;

	a2 = GetAngleSegs(		  						//Find correct Segment and nearest point in it
				xx->bezierData.arcSegs.cnt,segPtr,
				&pos2, &segInx, &d , &back, NULL, &neg );   	//d = how far pos2 from old pos2 = trvTrk->pos

	if ( d > 10 ) {
			ErrorMessage( "traverseBezier: Position is not near track: %0.3f", d );
			return FALSE;   						//This means the input pos is not on or close to the track.
	}
	if (back) a2 = NormalizeAngle(a2+180);		    //GetAngleSegs has reversed angle for backwards
	a1 = NormalizeAngle(a2-trvTrk->angle);						//Establish if we are going fwds or backwards globally
	if (a1 <270 && a1>90) {										//Must add 180 if the seg is reversed or inverted (but not both)
		segs_backwards = TRUE;
		ep = 0;
	} else {
		segs_backwards = FALSE;
		ep = 1;
	}
	if ( neg ) {
		segs_backwards = !segs_backwards;					//neg implies all the segs are reversed
		ep = 1-ep;											//other end
	}

	segProcData.traverse1.pos = pos2;							//actual point on curve
	segProcData.traverse1.angle = trvTrk->angle;                //direction car is going for Traverse 1 has to be reversed...
LOG( log_traverseBezier, 1, ( " TraverseBezier [%0.3f %0.3f] D%0.3f A%0.3f SB%d \n", trvTrk->pos.x, trvTrk->pos.y, dist, trvTrk->angle, segs_backwards ) )
	inx = segInx;
	while (inx >=0 && inx<xx->bezierData.arcSegs.cnt) {
		segPtr = (trkSeg_p)xx->bezierData.arcSegs.ptr+inx;  	//move in to the identified segment
		SegProc( SEGPROC_TRAVERSE1, segPtr, &segProcData );   	//Backwards or forwards for THIS segment - note that this can differ from segs_backward!!
		BOOL_T backwards = segProcData.traverse1.backwards;			//Are we going to EP0?
		BOOL_T reverse_seg = segProcData.traverse1.reverse_seg;		//is it a backwards segment (we don't actually care as Traverse1 takes care of it)

		dist += segProcData.traverse1.dist;
		segProcData.traverse2.dist = dist;
		segProcData.traverse2.segDir = backwards;
LOG( log_traverseBezier, 2, ( " TraverseBezierT1 D%0.3f B%d RS%d \n", dist, backwards, reverse_seg ) )
		SegProc( SEGPROC_TRAVERSE2, segPtr, &segProcData );		//Angle at pos2
		if ( segProcData.traverse2.dist <= 0 ) {				//-ve or zero distance left over so stop there
			*distR = 0;
			trvTrk->pos = segProcData.traverse2.pos;
			trvTrk->angle = segProcData.traverse2.angle;
LOG( log_traverseBezier, 1, ( "  -> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR ) )
			return TRUE;
		}														//NOTE Traverse1 and Traverse2 are overlays so get all out before storing
		dist = segProcData.traverse2.dist;						//How far left?
		coOrd pos = segProcData.traverse2.pos;					//Will be at seg end
		ANGLE_T angle = segProcData.traverse2.angle;			//Angle of end
		segProcData.traverse1.angle = angle; 					//Reverse to suit Traverse1
		segProcData.traverse1.pos = pos;
		inx = segs_backwards?inx-1:inx+1;						//Here's where the global segment direction comes in
LOG( log_traverseBezier, 2, ( " TraverseBezierL D%0.3f A%0.3f\n", dist, angle ) )
	}
	*distR = dist;								//Tell caller what is left
												//Must be at one end or another
	trvTrk->pos = GetTrkEndPos(trk,ep);
	trvTrk->angle = NormalizeAngle(GetTrkEndAngle(trk, ep)+(segs_backwards?180:0));
	trvTrk->trk = GetTrkEndTrk(trk,ep);						//go to next track
	if (trvTrk->trk==NULL) {
		trvTrk->pos = pos2;
	    return TRUE;
	}

LOG( log_traverseBezier, 1, ( "  -> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR ) )
	return TRUE;

}


static BOOL_T MergeBezier(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	struct extraData *xx0 = GetTrkExtraData(trk0);
	struct extraData *xx1 = GetTrkExtraData(trk1);
	track_p trk2 = NULL;
	EPINX_T ep2=-1;
	BOOL_T tracks = FALSE;

	if (IsTrack(trk0) && IsTrack(trk1) ) tracks = TRUE;
	if (GetTrkType(trk0) != GetTrkType(trk1)) return FALSE;

	if (ep0 == ep1)
		return FALSE;
    
	UndoStart( _("Merge Bezier"), "MergeBezier( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	UndoModify( trk0 );
	UndrawNewTrack( trk0 );
	if (tracks) {
		trk2 = GetTrkEndTrk( trk1, 1-ep1 );
		if (trk2) {
			ep2 = GetEndPtConnectedToMe( trk2, trk1 );
			DisconnectTracks( trk1, 1-ep1, trk2, ep2 );
		}
	}
	if (ep0 == 0) {
		xx0->bezierData.pos[3] = xx1->bezierData.pos[3];
		xx0->bezierData.pos[2] = xx1->bezierData.pos[2];
	} else {
		xx0->bezierData.pos[0] = xx1->bezierData.pos[0];
		xx0->bezierData.pos[1] = xx1->bezierData.pos[1];
	}
	FixUpBezier(xx0->bezierData.pos,xx0,tracks);
	ComputeBezierBoundingBox(trk0,xx0);
	DeleteTrack( trk1, FALSE );
	if (tracks && trk2) {
		if (ep0 == 1)
			SetTrkEndPoint( trk2, 1, xx0->bezierData.pos[0], xx0->bezierData.a0);
		else
			SetTrkEndPoint( trk2, 2, xx0->bezierData.pos[3], xx0->bezierData.a1);
		ConnectTracks( trk0, ep0, trk2, ep2 );
	}
	DrawNewTrack( trk0 );

	MainRedraw();
	MapRedraw();


	return TRUE;
}


static BOOL_T EnumerateBezier( track_p trk )
{

	if (trk != NULL) {
		DIST_T d;
		struct extraData *xx = GetTrkExtraData(trk);
		d = xx->bezierData.length;
		ScaleLengthIncrement( GetTrkScale(trk), d );
	}
	return TRUE;
}

static DIST_T GetLengthBezier( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T length = 0.0;
	segProcData_t segProcData;
	for(int i=0;i<xx->bezierData.arcSegs.cnt;i++) {
		SegProc(SEGPROC_LENGTH,&(DYNARR_N(trkSeg_t,xx->bezierData.arcSegs,i)), &segProcData);
		length += segProcData.length.length;
	}
	return length;
}


static BOOL_T GetParamsBezier( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	int segInx;
	BOOL_T back,negative;
	DIST_T d;

	params->type = curveTypeBezier;
	struct extraData *xx = GetTrkExtraData(trk);
	for (int i=0;i<4;i++) params->bezierPoints[i] = xx->bezierData.pos[i];
	params->len = xx->bezierData.length;
	params->track_angle = GetAngleSegs(		  						//Find correct Segment and nearest point in it
					xx->bezierData.arcSegs.cnt,xx->bezierData.arcSegs.ptr,
					&pos, &segInx, &d , &back, NULL, &negative );
    if ( negative != back ) params->track_angle = NormalizeAngle(params->track_angle+180);  //Bezier is in reverse
	trkSeg_p segPtr = &DYNARR_N(trkSeg_t,xx->bezierData.arcSegs,segInx);
	if (segPtr->type == SEG_STRLIN) {
		params->arcR = 0.0;
	} else {
		params->arcR = fabs(segPtr->u.c.radius);
		params->arcP = segPtr->u.c.center;
		params->arcA0 = segPtr->u.c.a0;
		params->arcA1 = segPtr->u.c.a1;
	}
	if ( inx == PARAMS_PARALLEL ) {
		params->ep = 0;
	} else if (inx == PARAMS_CORNU ){
		params->ep = PickEndPoint( pos, trk);
	} else {
		params->ep = PickUnconnectedEndPointSilent( pos, trk);
	}
	if (params->ep>=0)
		params->angle = GetTrkEndAngle(trk, params->ep);
	return TRUE;

}

static BOOL_T TrimBezier( track_p trk, EPINX_T ep, DIST_T dist ) {
	DeleteTrack(trk, TRUE);
	MainRedraw();
	MapRedraw();
	return TRUE;
}



static BOOL_T QueryBezier( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	switch ( query ) {
	case Q_CAN_GROUP:
		return FALSE;
		break;
	case Q_FLIP_ENDPTS:
	case Q_HAS_DESC:
		return TRUE;
		break;
	case Q_EXCEPTION:
		return GetTrkType(trk) == T_BEZIER?xx->bezierData.minCurveRadius < GetLayoutMinTrackRadius():FALSE;
		break;
	case Q_CAN_MODIFY_CONTROL_POINTS:
		return TRUE;
		break;
	case Q_CANNOT_PLACE_TURNOUT:
		return FALSE;
		break;
	case Q_ISTRACK:
		return GetTrkType(trk) == T_BEZIER?TRUE:FALSE;
		break;
	case Q_CAN_PARALLEL:
		return (GetTrkType(trk) == T_BEZIER);
		break;
	case Q_MODIFY_CAN_SPLIT:
	case Q_CORNU_CAN_MODIFY:
		return (GetTrkType(trk) == T_BEZIER);
	default:
		return FALSE;
	}
}


static void FlipBezier(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->bezierData.pos[0], orig, angle );
	FlipPoint( &xx->bezierData.pos[1], orig, angle );
    FlipPoint( &xx->bezierData.pos[2], orig, angle );
    FlipPoint( &xx->bezierData.pos[3], orig, angle );
    FixUpBezier(xx->bezierData.pos,xx,IsTrack(trk));
    ComputeBezierBoundingBox(trk,xx);

}

static ANGLE_T GetAngleBezier(
		track_p trk,
		coOrd pos,
		EPINX_T * ep0,
		EPINX_T * ep1 )
{
	struct extraData * xx = GetTrkExtraData(trk);
	ANGLE_T angle;
	BOOL_T back, neg;
	int indx;
	angle = GetAngleSegs( xx->bezierData.arcSegs.cnt, (trkSeg_p)xx->bezierData.arcSegs.ptr, &pos, &indx, NULL, &back, NULL, &neg );
	if (!back) angle = NormalizeAngle(angle+180);  //Make CCW
	if ( ep0 ) *ep0 = neg?1:0;
	if ( ep1 ) *ep1 = neg?0:1;
	return angle;
}

BOOL_T GetBezierSegmentFromTrack(track_p trk, trkSeg_p seg_p) {
	struct extraData * xx = GetTrkExtraData(trk);

	seg_p->type = IsTrack(trk)?SEG_BEZTRK:SEG_BEZLIN;
	for (int i=0;i<4;i++) seg_p->u.b.pos[i] = xx->bezierData.pos[i];
	seg_p->color = xx->bezierData.segsColor;
	seg_p->bezSegs.cnt = 0;
	if (seg_p->bezSegs.ptr) MyFree(seg_p->bezSegs.ptr);
	seg_p->bezSegs.max = 0;
	FixUpBezierSeg(seg_p->u.b.pos,seg_p,seg_p->type == SEG_BEZTRK);
	return TRUE;

}


static BOOL_T MakeParallelBezier(
		track_p trk,
		coOrd pos,
		DIST_T sep,
		track_p * newTrkR,
		coOrd * p0R,
		coOrd * p1R )
{
	struct extraData * xx = GetTrkExtraData(trk);
    coOrd np[4], p;
    ANGLE_T a,a2;

	//Produce bezier that is translated parallel to the existing Bezier
    // - not a precise result if the bezier end angles are not in the same general direction.
    // The expectation is that the user will have to adjust it - unless and until we produce
    // a new algo to adjust the control points to be parallel to the endpoints.
    
    a = FindAngle(xx->bezierData.pos[0],xx->bezierData.pos[3]);
    p = pos;
    DistanceBezier(trk, &p);
    a2 = NormalizeAngle(FindAngle(pos,p)-a);
    //find parallel move x and y for points
    for (int i =0; i<4;i++) {
    	np[i] = xx->bezierData.pos[i];
    }

    if ( a2 > 180 ) {
        Translate(&np[0],np[0],a+90,sep);
        Translate(&np[1],np[1],a+90,sep);
        Translate(&np[2],np[2],a+90,sep);
        Translate(&np[3],np[3],a+90,sep);
    } else {
        Translate(&np[0],np[0],a-90,sep);
        Translate(&np[1],np[1],a-90,sep);
        Translate(&np[2],np[2],a-90,sep);
        Translate(&np[3],np[3],a-90,sep);
    }

	if ( newTrkR ) {
		*newTrkR = NewBezierTrack( np, NULL, 0);
	} else {
		DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 1;
		tempSegs(0).type = SEG_BEZTRK;
		if (tempSegs(0).bezSegs.ptr) MyFree(tempSegs(0).bezSegs.ptr);
		tempSegs(0).bezSegs.max = 0;
		tempSegs(0).bezSegs.cnt = 0;
		for (int i=0;i<4;i++) tempSegs(0).u.b.pos[i] = np[i];
		FixUpBezierSeg(tempSegs(0).u.b.pos,&tempSegs(0),TRUE);
	}
	if ( p0R ) *p0R = np[0];
	if ( p1R ) *p1R = np[1];
	return TRUE;
}

/*
 * When an undo is run, the array of segs is missing - they are not saved to the Undo log. So Undo calls this routine to
 * ensure
 * - that the Segs are restored and
 * - other fields reset.
 */
BOOL_T RebuildBezier (track_p trk)
{
	struct extraData *xx;
	xx = GetTrkExtraData(trk);
	xx->bezierData.arcSegs.cnt = 0;
	FixUpBezier(xx->bezierData.pos,xx,IsTrack(trk));
	ComputeBezierBoundingBox(trk, xx);
	return TRUE;
}

BOOL_T MoveBezierEndPt ( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d0 ) {
	track_p trk2;
	struct extraData *xx;
	if (SplitTrack(*trk,pos,*ep,&trk2,TRUE)) {
		if (trk2) DeleteTrack(trk2,TRUE);
		xx = GetTrkExtraData(*trk);
		SetTrkEndPoint( *trk, *ep, *ep?xx->bezierData.pos[3]:xx->bezierData.pos[0], *ep?xx->bezierData.a1:xx->bezierData.a0 );
		MainRedraw();
		MapRedraw();
		return TRUE;
	}
	return FALSE;
}

static trackCmd_t bezlinCmds = {
		"BZRLIN",
		DrawBezier,
		DistanceBezier,
		DescribeBezier,
		DeleteBezier,
		WriteBezier,
		ReadBezier,
		MoveBezier,
		RotateBezier,
		RescaleBezier,
		NULL,
		GetAngleBezier,
		SplitBezier,
		NULL,
		NULL,
		NULL,	/* redraw */
		NULL,   /* trim   */
		MergeBezier,
		NULL,   /* modify */
		GetLengthBezier,
		GetParamsBezier,
		NULL, /* Move EndPt */
		QueryBezier,
		NULL,	/* ungroup */
		FlipBezier,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		RebuildBezier
		};

static trackCmd_t bezierCmds = {
		"BEZIER",
		DrawBezier,
		DistanceBezier,
		DescribeBezier,
		DeleteBezier,
		WriteBezier,
		ReadBezier,
		MoveBezier,
		RotateBezier,
		RescaleBezier,
		NULL,
		GetAngleBezier,
		SplitBezier,
		TraverseBezier,
		EnumerateBezier,
		NULL,	/* redraw */
		TrimBezier,   /* trim   */
		MergeBezier,
		NULL,   /* modify */
		GetLengthBezier,
		GetParamsBezier,
		MoveBezierEndPt, /* Move EndPt */
		QueryBezier,
		NULL,	/* ungroup */
		FlipBezier,
		NULL,
		NULL,
		NULL,
		MakeParallelBezier,
		NULL,
		RebuildBezier
		};


EXPORT void BezierSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	ANGLE_T a1, a2;
	DIST_T d, dd;
	coOrd p0,p2 ;
	segProcData_t segProcData;
	trkSeg_p subSegsPtr;
	coOrd temp0,temp1,temp2,temp3;
	int inx,segInx;
	BOOL_T back, segs_backwards, neg;
#define bezSegs(N) DYNARR_N( trkSeg_t, segPtr->bezSegs, N )

    switch (cmd) {

	case SEGPROC_TRAVERSE1:						//Work out how much extra dist and what direction
		if (segPtr->type != SEG_BEZTRK) {
			data->traverse1.dist = 0;
			return;
		}
		d = data->traverse1.dist;
		p0 = data->traverse1.pos;
LOG( log_bezierSegments, 1, ( "    BezTr1-Enter P[%0.3f %0.3f] A%0.3f\n", p0.x, p0.y, data->traverse1.angle ))
		a2 = GetAngleSegs(segPtr->bezSegs.cnt,segPtr->bezSegs.ptr,&p0,&segInx,&d,&back, NULL, &neg); //Find right seg and pos
		inx = segInx;
		data->traverse1.BezSegInx = segInx;
		data->traverse1.reverse_seg = FALSE;
		data->traverse1.backwards = FALSE;
		if (d>10) {
			data->traverse1.dist = 0;
			return;
		}

		if (back) a2 = NormalizeAngle(a2+180);
		a1 = NormalizeAngle(a2-data->traverse1.angle);				//Establish if we are going fwds or backwards globally
		if (a1<270 && a1>90) {								    	//Must add 180 if the seg is reversed or inverted (but not both)
			segs_backwards = TRUE;
	    } else  {
			segs_backwards = FALSE;
	    }
		if ( neg ) {
			segs_backwards = !segs_backwards;						//neg implies all the segs are reversed
		}
	    segProcData.traverse1.pos = data->traverse1.pos = p0;		  //actual point on curve
	    segProcData.traverse1.angle = data->traverse1.angle;          //Angle of car
 LOG( log_bezierSegments, 1, ( "    BezTr1-GSA I%d P[%0.3f %0.3f] N%d SB%d\n", segInx, p0.x, p0.y, neg, segs_backwards ))
		subSegsPtr = (trkSeg_p)segPtr->bezSegs.ptr+inx;
		SegProc( SEGPROC_TRAVERSE1, subSegsPtr, &segProcData );
		data->traverse1.reverse_seg = segProcData.traverse1.reverse_seg; //which way is curve (info)
		data->traverse1.backwards = segProcData.traverse1.backwards;     //Pass through Train direction
		data->traverse1.dist = segProcData.traverse1.dist;			     //Get last seg partial dist
		data->traverse1.segs_backwards = segs_backwards;				 //Get last
		data->traverse1.negative = segProcData.traverse1.negative;		 //Is curve flipped (info)
		data->traverse1.BezSegInx = inx;								 //Copy up Index
LOG( log_bezierSegments, 1, ( "    BezTr1-Exit -> A%0.3f B%d R%d N%d D%0.3f\n", a2, segProcData.traverse1.backwards, segProcData.traverse1.reverse_seg, segProcData.traverse1.negative, segProcData.traverse1.dist ))
		break;

	case SEGPROC_TRAVERSE2:
		if (segPtr->type != SEG_BEZTRK) return;							//Not SEG_BEZLIN
LOG( log_bezierSegments, 1, ( "    BezTr2-Enter D%0.3f SD%d SI%d SB%d\n", data->traverse2.dist, data->traverse2.segDir, data->traverse2.BezSegInx, data->traverse2.segs_backwards))
		if (data->traverse2.dist <= segPtr->u.b.length) {

			segProcData.traverse2.pos = data->traverse2.pos;
			DIST_T dist = data->traverse2.dist;
			segProcData.traverse2.dist = data->traverse2.dist;
			segProcData.traverse2.angle = data->traverse2.angle;
			segProcData.traverse2.segDir = data->traverse2.segDir;
			segs_backwards = data->traverse2.segs_backwards;
			BOOL_T backwards = data->traverse2.segDir;
			inx = data->traverse2.BezSegInx;							//Special from Traverse1
			while (inx>=0 && inx<segPtr->bezSegs.cnt) {
				subSegsPtr = (trkSeg_p)segPtr->bezSegs.ptr+inx;
				SegProc(SEGPROC_TRAVERSE2, subSegsPtr, &segProcData);
				if (segProcData.traverse2.dist<=0) {	    				//Done
					data->traverse2.angle = segProcData.traverse2.angle;
					data->traverse2.dist = 0;
					data->traverse2.pos = segProcData.traverse2.pos;
LOG( log_bezierSegments, 1, ( "    BezTr2-Exit1 -> A%0.3f P[%0.3f %0.3f] \n", data->traverse2.angle, data->traverse2.pos.x, data->traverse2.pos.y ))
					return;
				} else dist = segProcData.traverse2.dist;
				p2 = segProcData.traverse2.pos;
				a2 = segProcData.traverse2.angle;
LOG( log_bezierSegments, 2, ( "    BezTr2-Tr2 D%0.3f P[%0.3f %0.3f] A%0.3f\n", dist, p2.x, p2.y, a2 ))

				segProcData.traverse1.pos = p2;
				segProcData.traverse1.angle = a2;
				inx = segs_backwards?inx-1:inx+1;
				if (inx<0 || inx>=segPtr->bezSegs.cnt) break;
				subSegsPtr = (trkSeg_p)segPtr->bezSegs.ptr+inx;
				SegProc(SEGPROC_TRAVERSE1, subSegsPtr, &segProcData);
				BOOL_T reverse_seg = segProcData.traverse1.reverse_seg;        //For Info only
				backwards = segProcData.traverse1.backwards;
				dist += segProcData.traverse1.dist;             		//Add extra if needed - this is if we have to go from the other end of this seg
				segProcData.traverse2.dist = dist;    					//distance left
				segProcData.traverse2.segDir = backwards;				//which way
				segProcData.traverse2.pos = p2;
				segProcData.traverse2.angle = a2;
LOG( log_bezierSegments, 2, ( "    BezTr2-Loop A%0.3f P[%0.3f %0.3f] D%0.3f SI%d B%d RS%d\n", a2, p2.x, p2.y, dist, inx, backwards, reverse_seg ))
			}
			data->traverse2.dist = dist;
		} else data->traverse2.dist -= segPtr->u.b.length;  //we got here because the desired point is not inside the segment
    	if (segs_backwards) {
    		data->traverse2.pos = segPtr->u.b.pos[0];					// Backwards so point 0
    		data->traverse2.angle = segPtr->u.b.angle0;
    	} else {
    		data->traverse2.pos = segPtr->u.b.pos[3];					// Forwards so point 3
    		data->traverse2.angle = segPtr->u.b.angle3;
    	}
LOG( log_bezierSegments, 1, ( "    BezTr-Exit2 --> SI%d A%0.3f P[%0.3f %0.3f] D%0.3f\n", inx, data->traverse2.angle, data->traverse2.pos.x, data->traverse2.pos.y, data->traverse2.dist))
		break;

	case SEGPROC_DRAWROADBEDSIDE:
        //TODO - needs parallel bezier problem solved...
		break;

	case SEGPROC_DISTANCE:

		dd = 100000.00;   //Just find one distance
		p0 = data->distance.pos1;

		//initialize p2 
		p2 = segPtr->u.b.pos[0];
		for(int i=0;i<segPtr->bezSegs.cnt;i++) {
			segProcData.distance.pos1 = p0;
			SegProc(SEGPROC_DISTANCE,&(DYNARR_N(trkSeg_t,segPtr->bezSegs,i)),&segProcData);
			d = segProcData.distance.dd;
			if (d<dd) {
				dd = d;
				p2 = segProcData.distance.pos1;
			}
		}
		data->distance.dd = dd;
		data->distance.pos1 = p2;
		break;

	case SEGPROC_FLIP:

        temp0 = segPtr->u.b.pos[0];
        temp1 = segPtr->u.b.pos[1];
        temp2 = segPtr->u.b.pos[2];
		temp3 = segPtr->u.b.pos[3];
		segPtr->u.b.pos[0] = temp3;
		segPtr->u.b.pos[1] = temp2;
		segPtr->u.b.pos[2] = temp1;
		segPtr->u.b.pos[3] = temp0;
		FixUpBezierSeg(segPtr->u.b.pos,segPtr,segPtr->type == SEG_BEZTRK);
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewBezierTrack( segPtr->u.b.pos, (trkSeg_t *)segPtr->bezSegs.ptr, segPtr->bezSegs.cnt);
		data->newTrack.ep[0] = 0;
		data->newTrack.ep[1] = 1;
		break;

	case SEGPROC_LENGTH:
		data->length.length = 0;
		for(int i=0;i<segPtr->bezSegs.cnt;i++) {
			SegProc(cmd,&(DYNARR_N(trkSeg_t,segPtr->bezSegs,i)),&segProcData);
			data->length.length += segProcData.length.length;
		}
		break;

	case SEGPROC_SPLIT:
		//TODO Split
		break;

	case SEGPROC_GETANGLE:
		inx = 0;
		back = FALSE;
		subSegsPtr = (trkSeg_p) segPtr->bezSegs.ptr;
		coOrd pos = data->getAngle.pos;
LOG( log_bezierSegments, 1, ( "    BezGA-In  P[%0.3f %0.3f] \n", pos.x, pos.y))
		data->getAngle.angle = GetAngleSegs(segPtr->bezSegs.cnt,subSegsPtr, &pos, &inx, NULL, &back, NULL, NULL);
										//Recurse for Bezier sub-segs (only straights and curves)

		data->getAngle.negative_radius = FALSE;
		data->getAngle.backwards = back;
		data->getAngle.pos = pos;
		data->getAngle.bezSegInx = inx;
		subSegsPtr +=inx;
		if (subSegsPtr->type == SEG_CRVTRK || subSegsPtr->type == SEG_CRVLIN ) {
			data->getAngle.radius = fabs(subSegsPtr->u.c.radius);
			if (subSegsPtr->u.c.radius<0 ) data->getAngle.negative_radius = TRUE;
			data->getAngle.center = subSegsPtr->u.c.center;
		}
		else data->getAngle.radius = 0.0;
LOG( log_bezierSegments, 1, ( "    BezGA-Out SI%d A%0.3f P[%0.3f %0.3f] B%d\n", inx, data->getAngle.angle, pos.x, pos.y, back))
		break;
    
	}

}


/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */


track_p NewBezierTrack(coOrd pos[4], trkSeg_t * tempsegs, int count)
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_BEZIER, 2, sizeof *xx );
	xx = GetTrkExtraData(p);
    xx->bezierData.pos[0] = pos[0];
    xx->bezierData.pos[1] = pos[1];
    xx->bezierData.pos[2] = pos[2];
    xx->bezierData.pos[3] = pos[3];
    xx->bezierData.segsColor = wDrawColorBlack;
    xx->bezierData.segsWidth = 0;
    FixUpBezier(pos, xx, TRUE);
LOG( log_bezier, 1, ( "NewBezierTrack( EP1 %0.3f, %0.3f, CP1 %0.3f, %0.3f, CP2 %0.3f, %0.3f, EP2 %0.3f, %0.3f )  = %d\n", pos[0].x, pos[0].y, pos[1].x, pos[1].y, pos[2].x, pos[2].y, pos[3].x, pos[3].y, GetTrkIndex(p) ) )
	ComputeBezierBoundingBox( p, xx );
	SetTrkEndPoint( p, 0, pos[0], xx->bezierData.a0);
	SetTrkEndPoint( p, 1, pos[3], xx->bezierData.a1);
	CheckTrackLength( p );
	return p;
}

EXPORT track_p NewBezierLine( coOrd pos[4], trkSeg_t * tempsegs, int count, wDrawColor color, DIST_T width )
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_BZRLIN, 2, sizeof *xx );
	xx = GetTrkExtraData(p);
    xx->bezierData.pos[0] = pos[0];
    xx->bezierData.pos[1] = pos[1];
    xx->bezierData.pos[2] = pos[2];
    xx->bezierData.pos[3] = pos[3];
    xx->bezierData.segsColor = color;
    xx->bezierData.segsWidth = width;
    FixUpBezier(pos, xx, FALSE);
LOG( log_bezier, 1, ( "NewBezierLine( EP1 %0.3f, %0.3f, CP1 %0.3f, %0.3f, CP2 %0.3f, %0.3f, EP2 %0.3f, %0.3f)  = %d\n", pos[0].x, pos[0].y, pos[1].x, pos[1].y, pos[2].x, pos[2].y, pos[3].x, pos[3].y, GetTrkIndex(p) ) )
	ComputeBezierBoundingBox( p, xx );
	return p;
}



EXPORT void InitTrkBezier( void )
{
	T_BEZIER = InitObject( &bezierCmds );
	T_BZRLIN = InitObject( &bezlinCmds );
	log_bezier = LogFindIndex( "Bezier" );
	log_traverseBezier = LogFindIndex( "traverseBezier" );
	log_bezierSegments = LogFindIndex( "traverseBezierSegs");
}

/********************************************************************************
 *
 * Bezier Functions
 *
 ********************************************************************************/


/**
 * Return point on Bezier using "t" (from 0 to 1)
 */
extern coOrd BezierPointByParameter(coOrd p[4], double t)
{

    double a,b,c,d;
    double mt = 1-t;
    double mt2 = mt*mt;
    double t2 = t*t;

    a = mt2*mt;
    b = mt2*t*3;
    c = mt*t2*3;
    d = t*t2;

    coOrd o;
    o.x = a*p[0].x+b*p[1].x+c*p[2].x+d*p[3].x;
    o.y = a*p[0].y+b*p[1].y+c*p[2].y+d*p[3].y;

    return o;

}
/**
 * Find distance from point to Bezier. Return also the "t" value of that closest point.
 */
extern DIST_T BezierMathDistance( coOrd * pos, coOrd p[4], int segments, double * t_value)
{
    DIST_T dd = 10000.0;
    double t = 0.0;
    coOrd pt;
    coOrd save_pt = p[0];
    for (int i=0; i<=segments; i++) {
        pt = BezierPointByParameter(p, (double)i/segments);
        if (FindDistance(*pos,pt) < dd) {
        	dd = FindDistance(*pos,pt);
        	t = (double)i/segments;
        	save_pt = pt;
        }
    }
    if (t_value) *t_value = t;
    * pos = save_pt;
    return dd;
}

extern coOrd BezierMathFindNearestPoint(coOrd *pos, coOrd p[4], int segments) {
    double t = 0.0;
    BezierMathDistance(pos, p, segments, &t);
    return BezierPointByParameter(p, t);
}

void BezierSlice(coOrd input[], coOrd output[], double t) {
	coOrd p1,p12,p2,p23,p3,p34,p4;
	coOrd p123, p234, p1234;

	    p1 = input[0];
	    p2 = input[1];
	    p3 = input[2];
	    p4 = input[3];

	    p12.x = (p2.x-p1.x)*t+p1.x;
	    p12.y = (p2.y-p1.y)*t+p1.y;

	    p23.x = (p3.x-p2.x)*t+p2.x;
	    p23.y = (p3.y-p2.y)*t+p2.y;

	    p34.x = (p4.x-p3.x)*t+p3.x;
	    p34.y = (p4.y-p3.y)*t+p3.y;

	    p123.x = (p23.x-p12.x)*t+p12.x;
	    p123.y = (p23.y-p12.y)*t+p12.y;

	    p234.x = (p34.x-p23.x)*t+p23.x;
	    p234.y = (p34.y-p23.y)*t+p23.y;

	    p1234.x = (p234.x-p123.x)*t+p123.x;
	    p1234.y = (p234.y-p123.y)*t+p123.y;

	    output[0]= p1;
	    output[1] = p12;
	    output[2] = p123;
	    output[3] = p1234;

};

/**
 * Split bezier into two parts
 */
extern void BezierSplit(coOrd input[], coOrd left[], coOrd right[] , double t) {

	BezierSlice(input,left,t);

	coOrd back[4],backright[4];

	for (int i = 0;i<4;i++) {
		back[i] = input[3-i];
	}
	BezierSlice(back,backright,1-t);
	for (int i = 0;i<4;i++) {
		right[i] = backright[3-i];
	}

}


/**
 * If close enough (length of control polygon exceeds chord by < error) add length of polygon.
 * Else split and recurse
 */
double BezierAddLengthIfClose(coOrd start[4], double error) {
    coOrd left[4], right[4];                  /* bez poly splits */
    double len = 0.0;                         /* arc length */
    double chord;                             /* chord length */
    int index;                                /* misc counter */

    for (index = 0; index <= 2; index++)
        len = len + FindDistance(start[index],start[index+1]); //add up control polygon

    chord = FindDistance(start[0],start[3]); //find chord length

    if((len-chord) > error)  {					// If error too large -
        BezierSplit(start,left,right,0.5);               /* split in two */
        len = BezierAddLengthIfClose(left, error);        /* recurse left side */
        len += BezierAddLengthIfClose(right, error);       /* recurse right side */
    }
    return len;									// Add length of this curve

}

/**
 * Use recursive splitting to get close approximation ot length of bezier
 *
 */
extern double BezierMathLength(coOrd p[4], double error)
{
    if (error == 0.0) error = 0.01;
    return BezierAddLengthIfClose(p, error);  /* kick off recursion */

}

coOrd  BezierFirstDerivative(coOrd p[4], double t)
{
    //checkParameterT(t);

    double tSquared = t * t;
    double s0 = -3 + 6 * t - 3 * tSquared;
    double s1 = 3 - 12 * t + 9 * tSquared;
    double s2 = 6 * t - 9 * tSquared;
    double s3 = 3 * tSquared;
    double resultX = p[0].x * s0 + p[1].x * s1 + p[2].x * s2 + p[3].x * s3;
    double resultY = p[0].y * s0 + p[1].y * s1 + p[2].y * s2 + p[3].y * s3;

    coOrd v;

    v.x = resultX;
    v.y = resultY;
    return v;
}

/**
 * Gets 2nd derivate wrt t of a Bezier curve at a point

 */
coOrd BezierSecondDerivative(coOrd p[4], double t)
{
    //checkParameterT(t);

    double s0 = 6 - 6 * t;
    double s1 = -12 + 18 * t;
    double s2 = 6 - 18 * t;
    double s3 = 6 * t;
    double resultX = p[0].x * s0 + p[1].x * s1 + p[2].x * s2 + p[3].x * s3;
    double resultY = p[0].y * s0 + p[1].y * s1 + p[2].y * s2 + p[3].y * s3;

    coOrd v;
    v.x = resultX;
    v.y = resultY;
    return v;
}

/**
 * Get curvature of a Bezier at a point
*/
extern double BezierCurvature(coOrd p[4], double t, coOrd * center)
{
    //checkParameterT(t);

    coOrd d1 = BezierFirstDerivative(p, t);
    coOrd d2 = BezierSecondDerivative(p, t);

    if (center) {
        double curvnorm = (d1.x * d1.x + d1.y* d1.y)/(d1.x * d2.y - d2.x * d1.y);
        coOrd p = BezierPointByParameter(&p, t);
        center->x = p.x-d1.y*curvnorm;
        center->y = p.y+d1.x*curvnorm;
    }

    double r1 = sqrt(pow(d1.x * d1.x + d1.y* d1.y, 3.0));
    double r2 = fabs(d1.x * d2.y - d2.x * d1.y);
    return r2 / r1;
}

/**
 * Get Maximum Curvature
 */
extern double BezierMaxCurve(coOrd p[4]) {
    double max = 0;
    for (int t = 0;t<100;t++) {
        double curv = BezierCurvature(p, t/100, NULL);
        if (max<curv) max = curv;
    }
    return max;
}

/**
 * Get Minimum Radius
 */
extern double BezierMathMinRadius(coOrd p[4]) {
    double curv = BezierMaxCurve(p);
    if (curv >= 1000.0 || curv <= 0.001 ) return 0.0;
    return 1/curv;
}

