/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tbezier.c,v 1.0 2015-07-01 tynewydd Exp $
 *
 * BEZIER TRACK
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

#include "track.h"
#include "draw.h"
#include "cbezier.h"
#include "tbezier.h"
#include "ccurve.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "utility.h"
#include "i18n.h"

static TRKTYP_T T_BEZIER = -1;
static TRKTYP_T T_BZRLIN = -1;


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
EXPORT void FixUpBezier(coOrd pos[4], struct extraData* xx) {
	xx->bezierData.a0 = NormalizeAngle(FindAngle(pos[1], pos[0]));
	xx->bezierData.a1 = NormalizeAngle(FindAngle(pos[2], pos[3]));
	ConvertToArcs(pos, &xx->bezierData.arcSegs, TRUE, xx->bezierData.segsColor,
			xx->bezierData.segsWidth);
	xx->bezierData.minCurveRadius = BezierMinRadius(pos,
			xx->bezierData.arcSegs);
	xx->bezierData.length = BezierLength(pos, xx->bezierData.arcSegs);
}


static void GetBezierAngles( ANGLE_T *a0, ANGLE_T *a1, track_p trk )
{
    struct extraData *xx = GetTrkExtraData(trk);
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
	ANGLE_T a, a0, a1;
	DIST_T d;
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
		DIST_T radius;
		FLOAT_T grade;
		LAYER_T layerNumber;
		ANGLE_T angle[2];
		dynArr_t segs;
		long width;
		wDrawColor color;
		} bezData;
typedef enum { P0, Z0, A0, C1, C2, P1, Z1, A1, RA, LN, GR, LY, WI, CO } crvDesc_e;
static descData_t crvDesc[] = {
/*P0*/	{ DESC_POS, N_("End Pt 1: X"), &bezData.pos[0] },
/*Z0*/	{ DESC_DIM, N_("Z1"), &bezData.elev[0] },
/*A0*/  { DESC_ANGLE, N_("End Angle 1"), &bezData.angle[0] },
/*C1*/	{ DESC_POS, N_("Ctl Pt 1: X"), &bezData.pos[1] },
/*C2*/	{ DESC_POS, N_("Ctl Pt 2: X"), &bezData.pos[2] },
/*P1*/	{ DESC_POS, N_("End Pt 2: X"), &bezData.pos[3] },
/*Z3*/	{ DESC_DIM, N_("Z2"), &bezData.elev[1] },
/*A1*/  { DESC_ANGLE, N_("End Angle 2"), &bezData.angle[1] },
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
	ANGLE_T a0, a1;
	EPINX_T ep;
	FLOAT_T turns;
	ANGLE_T angle1, angle2;

	if ( inx == -1 )
		return;
	updateEndPts = FALSE;
	switch ( inx ) {
    case P0:
        if (GetTrkEndTrk(trk,0)) break;
    	updateEndPts = TRUE;
        xx->bezierData.pos[0] = bezData.pos[0];
        crvDesc[P0].mode |= DESC_CHANGE;
        /* no break */
    case P1:
    	if (GetTrkEndTrk(trk,0) && GetTrkEndTrk(trk,1)) break;
        updateEndPts = TRUE;
        xx->bezierData.pos[3]= bezData.pos[3];
        crvDesc[P1].mode |= DESC_CHANGE;
        break;
    case A0:
    case A1:
    	break;
    case C1:
    	if (GetTrkEndTrk(trk,0)) {
    		angle1 = NormalizeAngle(GetTrkEndAngle(trk,0));
    	    angle2 = NormalizeAngle(FindAngle(bezData.pos[1], xx->bezierData.pos[0])-angle1);
    		if (angle2 > 90.0 && angle2 < 270.0)
    		  Translate( &bezData.pos[1], xx->bezierData.pos[0], angle1, -FindDistance( xx->bezierData.pos[0], bezData.pos[1] )*cos(D2R(angle2)));
    	}
        xx->bezierData.pos[1] = bezData.pos[1];
        crvDesc[C1].mode |= DESC_CHANGE;
        break;
    case C2:
    	if (GetTrkEndTrk(trk,1)) {
    	    angle1 = NormalizeAngle(GetTrkEndAngle(trk,1));
    	    angle2 = NormalizeAngle(FindAngle(bezData.pos[2], xx->bezierData.pos[3])-angle1);
    	    if (angle2 > 90.0 && angle2 < 270.0)
    	    Translate( &bezData.pos[2], xx->bezierData.pos[3], angle1, -FindDistance( xx->bezierData.pos[3], bezData.pos[0] )*cos(D2R(angle2)));
    	}
        xx->bezierData.pos[2] = bezData.pos[2];
        crvDesc[C2].mode |= DESC_CHANGE;
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
		crvDesc[GR].mode |= DESC_CHANGE;
		crvDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
        return;
	case LY:
		SetTrkLayer( trk, bezData.layerNumber);
		break;
	case WI:
		xx->bezierData.segsWidth = bezData.width;
		break;
	case CO:
		xx->bezierData.segsColor = bezData.color;
		break;
	default:
		AbortProg( "updateBezier: Bad inx %d", inx );
	}
	ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, IsTrack(trk)?TRUE:FALSE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
	if (updateEndPts) {
			if ( GetTrkEndTrk(trk,0) == NULL ) {
				SetTrkEndPoint( trk, 0, bezData.pos[0], NormalizeAngle( FindAngle(bezData.pos[0], bezData.pos[1]) ) );
				crvDesc[A0].mode |= DESC_CHANGE;
			}
			if ( GetTrkEndTrk(trk,1) == NULL ) {
				SetTrkEndPoint( trk, 1, bezData.pos[3], NormalizeAngle( FindAngle(bezData.pos[2], bezData.pos[3]) ) );
				crvDesc[A1].mode |= DESC_CHANGE;
			}
	}

	FixUpBezier(xx->bezierData.pos, &xx);
	DrawNewTrack( trk );
}

static void DescribeBezier( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a0, a1;
	DIST_T d;
	int fix0, fix1 = 0;
	FLOAT_T turns;

	
	d = xx->bezierData.length;
    sprintf( str, _("Bezier %s(%d): Layer=%d MinRadius=%s Length=%s EP=[%0.3f,%0.3f] [%0.3f,%0.3f] CP1=[%0.3f,%0.3f] CP2=[%0.3f, %0.3f]"),
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
	bezData.radius = xx->bezierData.minCurveRadius;
    bezData.layerNumber = GetTrkLayer(trk);
    bezData.pos[0] = xx->bezierData.pos[0];
    crvData.pos[1] = xx->bezierData.pos[1];
    bezData.pos[2] = xx->bezierData.pos[2];
    bezData.pos[3] = xx->bezierData.pos[3];
    bezData.angle[0] = xx->bezierData.a0;
    crvData.angle[1] = xx->bezierData.a1;
    if (GetTrkType(trk) == T_BEZIER) {
		ComputeElev( trk, 0, FALSE, &bezData.elev[0], NULL );
		ComputeElev( trk, 1, FALSE, &bezData.elev[1], NULL );
	
		if ( bezData.length > minLength )
			bezData.grade = fabs( (bezData.elev[0]-bezData.elev[1])/bezData.length )*100.0;
		else
			bezData.grade = 0.0;
    }

	crvDesc[P0].mode = fix0?DESC_RO:0;
	crvDesc[P1].mode = fix1?DESC_RO:0;
	crvDesc[LN].mode = DESC_RO;
    crvDesc[C1].mode = 0;
    crvDesc[C2].mode = 0;
    if (GetTrkType(trk) == T_BEZIER) {
    	crvDesc[Z0].mode = EndPtIsDefinedElev(trk,0)?0:DESC_RO;
		crvDesc[Z1].mode = EndPtIsDefinedElev(trk,1)?0:DESC_RO;
    }
    else
    	crvDesc[Z0].mode = crvDesc[Z1].mode = DESC_IGNORE;
	crvDesc[A0].mode = DESC_RO;
	crvDesc[A1].mode = DESC_RO;
	crvDesc[GR].mode = DESC_RO;
    crvDesc[RA].mode = DESC_RO;
	crvDesc[LY].mode = DESC_NOREDRAW;
	crvDesc[WI].mode = GetTrkType(trk) == T_BEZIER?DESC_IGNORE:0;
	crvDesc[CO].mode = GetTrkType(trk) == T_BEZIER?DESC_IGNORE:0;
	
	if (GetTrkType(trk) == T_BEZIER)
		DoDescribe( _("Bezier Track"), trk, crvDesc, UpdateBezier );
	else
		DoDescribe( _("Bezier Line"), trk, crvDesc, UpdateBezier );

}

static DIST_T DistanceBezier( track_p t, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(t);
	double s;
	//return BezierMathDistance(p,xx->bezierData.pos,100, &s);

	ANGLE_T a0, a1;
	DIST_T d = 100000.0,dd;
	coOrd p2;
	segProcData_t segProcData;
	for (int i = 0;i<xx->bezierData.arcSegs.cnt;i++) {
		segProcData.distance.pos1 = * p;
		SegProc(SEGPROC_DISTANCE,&DYNARR_N(trkSeg_t,xx->bezierData.arcSegs,i),&segProcData);
		if (segProcData.distance.dd<d) {
			d = segProcData.distance.dd;
			p2 = segProcData.distance.pos1;
		}
	}
    //d = BezierDistance( p, xx->bezierData.pos[0], xx->bezierData.pos[1], xx->bezierData.pos[2], xx->bezierData.pos[1], 100, NULL );
	p = &p2;
	return d;
}

static void DrawBezier( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);
		track_p tt = t;
	long widthOptions = DTS_LEFT|DTS_RIGHT;


	if (GetTrkType(t) == T_BZRLIN) {
		DrawSegsO(&mainD,t,zero,0.0,xx->bezierData.arcSegs.ptr,xx->bezierData.arcSegs.cnt, 0.0, color, 0);
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
		DrawSegsO(&mainD,t,zero,0.0,xx->bezierData.arcSegs.ptr,xx->bezierData.arcSegs.cnt, GetTrkGauge(t), color, widthOptions|DTS_TIES);
	DrawSegsO(&mainD,t,zero,0.0,xx->bezierData.arcSegs.ptr,xx->bezierData.arcSegs.cnt, GetTrkGauge(t), color, widthOptions);
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
			if (s.u.b.segs.ptr) free(s.u.b.segs.ptr);
			s.u.b.segs.max = 0;
			s.u.b.segs.cnt = 0;
		}
	}
	if (xx->bezierData.arcSegs.ptr && xx->bezierData.arcSegs.max != 0)
		free(xx->bezierData.arcSegs.ptr);
	xx->bezierData.arcSegs.max = 0;
	xx->bezierData.arcSegs.cnt = 0;
}

static BOOL_T WriteBezier( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	long options;
	BOOL_T rc = TRUE;
	BOOL_T track =(GetTrkType(t)==T_BEZIER);
	options = GetTrkWidth(t) & 0x0F;
	rc &= fprintf(f, "%s %d %d %ld %ld %0.6f %s %d %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f 0 %0.6f %0.6f \n",
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
	rc &= fprintf(f, "\tEND\n" )>0;
	return rc;
}

static void ReadBezier( char * line )
{
	struct extraData *xx;
	track_p t;
	wIndex_t index;
	BOOL_T visible;
	DIST_T r;
	coOrd p0, c1, c2, p1, dp;
	DIST_T elev;
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
    ComputeBezierBoundingBox( t, xx );
    xx->bezierData.descriptionOff = dp;
    xx->bezierData.a0 = NormalizeAngle(FindAngle(c1,p0));
    xx->bezierData.a1 = NormalizeAngle(FindAngle(c2,p1));
    xx->bezierData.segsWidth = width;
    xx->bezierData.segsColor = wDrawFindColor( rgb );
    ReadSegs();
    if (GetTrkType(t) == T_BEZIER) {
		SetEndPts(t,2);
	}
	xx->bezierData.arcSegs.max = 0;
	ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, TRUE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
    xx->bezierData.minCurveRadius = BezierMinRadius(xx->bezierData.pos,xx->bezierData.arcSegs);
    xx->bezierData.length = BezierLength(xx->bezierData.pos,xx->bezierData.arcSegs);
}

static void MoveBezier( track_p trk, coOrd orig )
{
	struct extraData *xx = GetTrkExtraData(trk);
    for (int i=0;i<4;i++) {
        xx->bezierData.pos[i].x += orig.x;
        xx->bezierData.pos[i].y += orig.y;
    }
    ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, TRUE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
	ComputeBezierBoundingBox( trk, xx );
	SetTrkEndPoint( trk, 0, xx->bezierData.pos[0], NormalizeAngle(FindAngle(xx->bezierData.pos[0],xx->bezierData.pos[1])));
	SetTrkEndPoint( trk, 1, xx->bezierData.pos[3], NormalizeAngle(FindAngle(xx->bezierData.pos[3],xx->bezierData.pos[2])));
}

static void RotateBezier( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
    for (int i=0;i<5;i++) {
        Rotate( &xx->bezierData.pos[i], orig, angle );
    }
    ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, TRUE,xx->bezierData.segsColor, xx->bezierData.segsWidth);
	ComputeBezierBoundingBox( trk, xx );
	SetTrkEndPoint( trk, 0, xx->bezierData.pos[0], NormalizeAngle(FindAngle(xx->bezierData.pos[0],xx->bezierData.pos[1])));
	SetTrkEndPoint( trk, 1, xx->bezierData.pos[3], NormalizeAngle(FindAngle(xx->bezierData.pos[3],xx->bezierData.pos[2])));

}

static void RescaleBezier( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	xx->bezierData.pos[0].x *= ratio;
	xx->bezierData.pos[0].y *= ratio;
    xx->bezierData.pos[1].x *= ratio;
    xx->bezierData.pos[1].y *= ratio;
    xx->bezierData.pos[2].x *= ratio;
    xx->bezierData.pos[2].y *= ratio;
    xx->bezierData.pos[3].x *= ratio;
    xx->bezierData.pos[3].y *= ratio;
	xx->bezierData.minCurveRadius = BezierMinRadius(xx->bezierData.pos, xx->bezierData.arcSegs);
    xx->bezierData.length = BezierLength(xx->bezierData.pos, xx->bezierData.arcSegs);
    ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, TRUE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
    ComputeBezierBoundingBox(trk, xx);
    SetTrkEndPoint( trk, 0, xx->bezierData.pos[0], NormalizeAngle(FindAngle(xx->bezierData.pos[0],xx->bezierData.pos[1])));
    SetTrkEndPoint( trk, 1, xx->bezierData.pos[3], NormalizeAngle(FindAngle(xx->bezierData.pos[3],xx->bezierData.pos[2])));
}

/**
 * Split the Track at approximately the point pos.
 */
static BOOL_T SplitBezier( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	track_p trk1;
    double t;
    
    coOrd newl[4], newr[4];
    
    dynArr_t tempSegs;

    
    double dd = DistanceBezier(trk, &pos);
    if (dd>minLength) return FALSE;
    
    BezierSplit(xx->bezierData.pos, &newl[0], &newr[0], t);

    ConvertToArcs(newr,&tempSegs,TRUE,xx->bezierData.segsColor, xx->bezierData.segsWidth);
	trk1 = NewBezierTrack(newr,(trkSeg_t *)tempSegs.ptr,tempSegs.cnt);

	tempSegs.cnt = tempSegs.max = 0;
	free(tempSegs.ptr);
    
    for (int i=0;i<4;i++) {
        xx->bezierData.pos[i] = newl[i];
    }
    ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, TRUE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
    xx->bezierData.length = BezierLength(xx->bezierData.pos,xx->bezierData.arcSegs);
    xx->bezierData.minCurveRadius = BezierMinRadius(xx->bezierData.pos,xx->bezierData.arcSegs);
    ComputeBezierBoundingBox( trk, xx);
	
	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;

	return TRUE;
}

static BOOL_T TraverseBezier( traverseTrack_p trvTrk, DIST_T * distR )
{
    track_p trk = trvTrk->trk;
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a, a0, a1;
	DIST_T arcDist;
	DIST_T dist;
    double t;
    GetBezierAngles( &a0, &a1, trk );
    
	a = NormalizeAngle( (a1-90.0) - trvTrk->angle );
	if ( a>270 || a<90 ) {
		// CCW
		if ( xx->bezierData.length < *distR ) {
			trvTrk->pos = xx->bezierData.pos[3];
			*distR -= xx->bezierData.length;
			trvTrk->angle = NormalizeAngle( a0-90.0 );
			trk = GetTrkEndTrk( trk, 0 );
		} else {
			trvTrk->dist += *distR;
            t = 1-(double)*distR/xx->bezierData.length;
			trvTrk->pos = BezierPointByParameter(xx->bezierData.pos, t);
			*distR = 0;
            coOrd derivative = BezierFirstDerivative(xx->bezierData.pos, t );
			trvTrk->angle = atan2(  derivative.x, derivative.y );
		}
	} else {
		// CW //
		if ( xx->bezierData.length < *distR ) {
			trvTrk->pos = xx->bezierData.pos[0];
			*distR -= xx->bezierData.length;
			trvTrk->angle = NormalizeAngle( a0+a1+90.0 );
			trk = GetTrkEndTrk( trk, 1 );
		} else {
			trvTrk->dist += *distR;
            t = (double)*distR/xx->bezierData.length;
			trvTrk->pos = BezierPointByParameter(xx->bezierData.pos, t );
			*distR = 0;
            coOrd derivative = BezierFirstDerivative(xx->bezierData.pos, t );
			trvTrk->angle = atan2( derivative.x, derivative.y );
		}
	}
	trvTrk->trk = trk;
    
	return TRUE;

}


static BOOL_T EnumerateBezier( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a0, a1;
	DIST_T d;
	if (trk != NULL) {
		xx = GetTrkExtraData(trk);
		d = xx->bezierData.minCurveRadius;
		ScaleLengthIncrement( GetTrkScale(trk), d );
	}
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
	DIST_T d;
	track_p trk2;
	EPINX_T ep2=-1;
	coOrd pos;

	if (ep0 == ep1)
		return FALSE;
    
	UndoStart( _("Merge Bezier"), "MergeBezier( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	UndoModify( trk0 );
	UndrawNewTrack( trk0 );
	trk2 = GetTrkEndTrk( trk1, 1-ep1 );
	if (trk2) {
		ep2 = GetEndPtConnectedToMe( trk2, trk1 );
		DisconnectTracks( trk1, 1-ep1, trk2, ep2 );
	}
	if (ep0 == 0) {
        xx0->bezierData.pos[3] = xx1->bezierData.pos[3];
        xx0->bezierData.pos[2] = xx1->bezierData.pos[2];
	} else {
        xx0->bezierData.pos[0] = xx1->bezierData.pos[0];
        xx0->bezierData.pos[1] = xx1->bezierData.pos[1];
	}
	DeleteTrack( trk1, FALSE );
	if (trk2) {
		ConnectTracks( trk0, ep0, trk2, ep2 );
	}
	DrawNewTrack( trk0 );
	ConvertToArcs(xx0->bezierData.pos,&xx0->bezierData.arcSegs, TRUE, xx0->bezierData.segsColor, xx0->bezierData.segsWidth);
    xx0->bezierData.length = BezierLength(xx0->bezierData.pos,xx0->bezierData.arcSegs);
    xx0->bezierData.minCurveRadius = BezierMinRadius(xx0->bezierData.pos, xx0->bezierData.arcSegs);
	ComputeBezierBoundingBox( trk0, GetTrkExtraData(trk0) );
	return TRUE;
}


static STATUS_T ModifyBezier( track_p trk, wAction_t action, coOrd pos )
{
    static int state;

	DIST_T r, d;
    coOrd pos0, pos1;
    int selectedPos;
	track_p trk1;
    EPINX_T ep;
	struct extraData *xx = GetTrkExtraData(trk);
    
    if (GetTrkType(trk) != T_BEZIER) return C_ERROR;

	switch ( action ) {

    case C_DOWN:
    	CmdBezModify(trk,C_START,pos);
    	return CmdBezModify(trk,action,pos);
        break;
    case C_UP:
    case C_OK:
    case C_TEXT:
	case C_MOVE:
	case C_CANCEL:
		return CmdBezModify(trk,action,pos);
		break;
	}
	return C_ERROR;
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
	params->type = curveTypeBezier;
	struct extraData *xx = GetTrkExtraData(trk);
	for (int i=0;i<4;i++) params->bezierPoints[i] = xx->bezierData.pos[i];
	GetBezierAngles(&params->arcA0,&params->arcA1, trk);
	params->len = xx->bezierData.length;
	params->ep = PickUnconnectedEndPoint( pos, trk);
	if (params->ep == -1)
		return FALSE;
	return TRUE;

}

static void AdjustBezierEndPt( track_p t, EPINX_T inx, coOrd pos ) {
    struct extraData *xx = GetTrkExtraData(t);
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
    ConvertToArcs(xx->bezierData.pos, &xx->bezierData.arcSegs, TRUE, xx->bezierData.segsColor, xx->bezierData.segsWidth);
    xx->bezierData.length = BezierLength(xx->bezierData.pos, xx->bezierData.arcSegs);
    xx->bezierData.minCurveRadius = BezierMinRadius(xx->bezierData.pos,xx->bezierData.arcSegs);

    ANGLE_T angle = FindAngle(pos,inx==0?xx->bezierData.pos[1]:xx->bezierData.pos[2]);
    SetTrkEndPoint( t, inx, pos, angle);
}


static BOOL_T QueryBezier( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	switch ( query ) {
	case Q_CAN_GROUP:
	case Q_FLIP_ENDPTS:
	case Q_HAS_DESC:
		return TRUE;
	case Q_EXCEPTION:
		return xx->bezierData.minCurveRadius < minTrackRadius;
	case Q_CAN_MODIFY_CONTROL_POINTS:
		return TRUE;
	case Q_ISTRACK:
		return GetTrkType(trk) == T_BEZIER?TRUE:FALSE;
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
	ComputeBezierBoundingBox( trk, xx );
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
	struct extraData * xx1;
    coOrd np[4], p;
    ANGLE_T a,a2;

	//Produce bezier that is translated parallel to the existing Bezier
    // - not a precise result if the bezier end angles are not in the same general direction.
    
    a = FindAngle(xx->bezierData.pos[0],xx->bezierData.pos[3]);
    p = BezierMathFindNearestPoint(&pos, xx->bezierData.pos, 100);
    a2 = FindAngle(pos,p);
    //find parallel move x and y for points
    for (int i =0; i<4;i++) {
    	np[i] = xx->bezierData.pos[i];
    }
    if ( a2 < 180 ) {
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
		xx1 = GetTrkExtraData(*newTrkR);
		ComputeBezierBoundingBox( *newTrkR, xx1 );
	} else {
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 1;
		tempSegs(0).type = SEG_BEZTRK;
		for (int i=0;i<4;i++) tempSegs(0).u.b.pos[i] = np[i];
		ConvertToArcs(tempSegs(0).u.b.pos,&tempSegs(0).u.b.segs, TRUE,tempSegs(0).color, tempSegs(0).width);
		tempSegs(0).u.b.max_radius = BezierMinRadius(tempSegs(0).u.b.pos,tempSegs(0).u.b.segs);
		tempSegs(0).u.b.length = BezierLength(tempSegs(0).u.b.pos,tempSegs(0).u.b.segs);
	}
	if ( p0R ) p0R = &np[0];
	if ( p1R ) p1R = &np[1];
	return TRUE;
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
		NULL,
		SplitBezier,
		NULL,
		NULL,
		NULL,	/* redraw */
		NULL,   /* trim   */
		MergeBezier,
		ModifyBezier,
		GetLengthBezier,
		GetParamsBezier,
		NULL, /* Move EndPt */
		QueryBezier,
		NULL,	/* ungroup */
		FlipBezier,
		NULL,
		NULL,
		NULL,
		NULL };

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
		NULL,
		SplitBezier,
		TraverseBezier,
		EnumerateBezier,
		NULL,	/* redraw */
		NULL,   /* trim   */
		MergeBezier,
		ModifyBezier,
		GetLengthBezier,
		GetParamsBezier,
		NULL, /* Move EndPt */
		QueryBezier,
		NULL,	/* ungroup */
		FlipBezier,
		NULL,
		NULL,
		NULL,
		MakeParallelBezier };


EXPORT void BezierSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	ANGLE_T a0, a1, a2;
	DIST_T d, dd, circum, d0;
	coOrd p0,p2 ;
	wIndex_t s0, s1;
	segProcData_t segProcData;
	coOrd temp0,temp1,temp2,temp3;

    switch (cmd) {

	case SEGPROC_TRAVERSE1:
        //TODO workout distance to point on a curve
		break;

	case SEGPROC_TRAVERSE2:
        //TODO workout remaining distance to point on a curve
		break;

	case SEGPROC_DRAWROADBEDSIDE:
        //TODO - needs parallel bezier problem solved...
		break;

	case SEGPROC_DISTANCE:

		dd = FindDistance(data->distance.pos1,segPtr->u.b.pos[0]);   //Just find one distance
		p0 = data->distance.pos1;
		for(int i=0;i<segPtr->u.b.segs.cnt;i++) {
			segProcData.distance.pos1 = p0;
			SegProc(SEGPROC_DISTANCE,&(DYNARR_N(trkSeg_t,segPtr->u.b.segs,i)),&segProcData);
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

		ConvertToArcs(segPtr->u.b.pos,&segPtr->u.b.segs, TRUE,segPtr->color,segPtr->width);
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewBezierTrack( segPtr->u.b.pos, (trkSeg_t *)segPtr->u.b.segs.ptr, segPtr->u.b.segs.cnt);
		break;

	case SEGPROC_LENGTH:
		for(int i=0;i<segPtr->u.b.segs.cnt;i++) {
			SegProc(cmd,&(DYNARR_N(trkSeg_t,segPtr->u.b.segs,i)),&segProcData);
			data->length.length += segProcData.length.length;
		}
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
		data->getAngle.angle = segPtr->u.b.angle0;
		break;
    
	}

}


/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */



EXPORT void PlotBezier(
		long mode,
		coOrd pos0,
		coOrd pos1,
		coOrd pos2,
		BezierData_t * BezierData,
		BOOL_T constrain )
{
	DIST_T d0, d2, r;
	ANGLE_T angle, a0, a1, a2;
	coOrd posx;
    //TODO
    /*
	switch ( mode ) {
	case crvCmdFromEP1:
            
            
			}
     */
}



track_p NewBezierTrack(coOrd pos[4], trkSeg_t * tempsegs, int count)
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_BEZIER, 2, sizeof *xx );
	SetTrkScale( p, curScaleInx );
	xx = GetTrkExtraData(p);
    xx->bezierData.pos[0] = pos[0];
    xx->bezierData.pos[1] = pos[1];
    xx->bezierData.pos[2] = pos[2];
    xx->bezierData.pos[3] = pos[3];
    xx->bezierData.segsColor = wDrawColorBlack;
    xx->bezierData.segsWidth = 0;
    FixUpBezier(pos, xx);
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
    FixUpBezier(pos, xx);
LOG( log_bezier, 1, ( "NewBezierLine( EP1 %0.3f, %0.3f, CP1 %0.3f, %0.3f, CP2 %0.3f, %0.3f, EP2 %0.3f, %0.3f)  = %d\n", pos[0].x, pos[0].y, pos[1].x, pos[1].y, pos[2].x, pos[2].y, pos[3].x, pos[3].y, GetTrkIndex(p) ) )
	ComputeBezierBoundingBox( p, xx );
	return p;
}



EXPORT void InitTrkBezier( void )
{
	T_BEZIER = InitObject( &bezierCmds );
	T_BZRLIN = InitObject( &bezlinCmds );
	log_bezier = LogFindIndex( "Bezier" );
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
    //checkParameterT(t);

    double cx = 3 * (p[1].x - p[0].x);
    double bx = 3 * (p[2].x- p[1].x) - cx;
    double ax = p[3].x - p[0].x - cx - bx;

    double cy = 3 * (p[1].y - p[0].y);
    double by = 3 * (p[2].y - p[1].y) - cy;
    double ay = p[3].y - p[0].y - cy - by;

    double tSquared = t * t;
    double tCubed = tSquared * t;
    double resultX = (ax * tCubed) + (bx * tSquared) + (cx * t) + p[0].x;
    double resultY = (ay * tCubed) + (by * tSquared) + (cy * t) + p[1].y;
    coOrd c;
    c.x = resultX;
    c.y = resultY;
    return c;
}
/**
 * Find distance from point to Bezier. Return also the "t" value of that closest point.
 */
extern DIST_T BezierMathDistance( coOrd * pos, coOrd p[4], int segments, double * t_value)
{
    DIST_T dd = FindDistance(*pos,p[0]);
    double t = 0.0;
    coOrd pt;
    for (int i = 1;i<=segments;i++) {
        pt = BezierPointByParameter(p, (double)i/segments);
        if (FindDistance(*pos,pt) < dd) { dd=FindDistance(*pos,pt); t= (double)i/segments; }
    }
    if (t_value) *t_value = t;
    return dd;
}

extern coOrd BezierMathFindNearestPoint(coOrd *pos, coOrd p[4], int segments) {
    double t = 0.0;
    double dd = BezierMathDistance (pos, p, segments, &t);
    return BezierPointByParameter(p, t);
}

/**
 * Split bezier into two parts
 */
extern void BezierSplit(coOrd input[4], coOrd left[4], coOrd right[4] , double t) {

    int   i, j;                               /* Index variables  */
    coOrd  Vtemp[4][4];                      /* Triangle Matrix */
    /* Copy control points  */
    for (j =0; j <= 3; j++)
        Vtemp[0][j] = input[j];

    /* Triangle computation */
    for (i = 1; i <= 3; i++) {
        for (j =0 ; j <= 3 - i; j++) {
            Vtemp[i][j].x =
            (1-t) * Vtemp[i-1][j].x + t * Vtemp[i-1][j+1].x;
            Vtemp[i][j].y =
            (1-t) * Vtemp[i-1][j].y + t * Vtemp[i-1][j+1].y;

        }                                   /* end for i */
    }                                       /* end for j */
    for (j =0;j <=3;j++)
        left[j]  = Vtemp[j][0];

    for (j = 0;j <=3;j++)
        right[j] = Vtemp[3-j][j];

}


/**
 * If close enough (length of control polygon exceeds chord by < error) add length of polygon.
 * Else split and recurse
 */
void BezierAddLengthIfClose(coOrd start[4], double length, double error) {
    coOrd left[4], right[4];                  /* bez poly splits */
    double len = 0.0;                         /* arc length */
    double chord;                             /* chord length */
    int index;                                /* misc counter */

    for (index = 0; index <= 2; index++)
        len = len + FindDistance(start[index],start[index+1]); //add up control polygon

    chord = FindDistance(start[0],start[3]); //find chord length

    if((len-chord) > error)  {					// If error too large -
        BezierSplit(start,left,right,0.5);               /* split in two */
        BezierAddLengthIfClose(left, length, error);        /* recurse left side */
        BezierAddLengthIfClose(right, length, error);       /* recurse right side */
        return;
    }
    length = length + len;						// Add length of this curve
    return;

}

/**
 * Use recursive splitting to get close approximation ot length of bezier
 *
 */
extern double BezierMathLength(coOrd p[4], double error)
{
    double length; /* length of curve */
    if (error == 0.0) error = 0.01;
    BezierAddLengthIfClose(p, length, error);  /* kick off recursion */
    return length;                                  /* that's it! */

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

