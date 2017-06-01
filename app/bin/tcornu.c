/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tcornu.c,v 1.0 2015-07-01 tynewydd Exp $
 *
 * CORNU SPIRAL TRACK
 *
 * A cornu is a spiral arc defined by a polynomial that has the property
 * that curvature varies linearly with distance along the curve. It is a family
 * of curves that include Euler spirals.
 *
 * In order to be useful in XtrkCAD it is defined as a set of Bezier curves each of
 * which is defined as a set of circular arcs and lines.
 *
 * The derivation of the beziers is done by the cornu library which must be recalled
 * whenever a change is made in the end conditions.
 *
 * A cornu has a minimum radius and a maximum rate of change of radius.
 *
 */

/*  XTrkCad - Model Railroad CAD
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
#include "ccornu.h"
#include "ccurve.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "utility.h"
#include "i18n.h"

EXPORT TRKTYP_T T_CORNU = -1;


struct extraData {
		cornuData_t cornuData;
		};

static int log_cornu = 0;

static DIST_T GetLengthCornu( track_p );

/****************************************
 *
 * UTILITIES
 *
 */

/*
 * Run after any changes to the Cornu points
 */
EXPORT void FixUpCornu(coOrd pos[2], ANGLE_T angle[2], DIST_T radius[2], struct extraData* xx) {
	xx->cornuData.a[0] = angle[0];
	xx->cornuData.r[0] = radius[0];
	xx->cornuData.a[1] = angle[1];
	xx->cornuData.r[1] = radius[1];

	CallCornu(pos, angle, radius, &xx->cornuData.arcSegs);
	xx->cornuData.minCurveRadius = CornuMinRadius(pos, angle, radius,
			xx->cornuData.arcSegs);
	xx->cornuData.length = CornuLength(pos, angle, radius, xx->cornuData.arcSegs);
}


static void GetCornuAngles( ANGLE_T *a0, ANGLE_T *a1, track_p trk )
{
    struct extraData *xx = GetTrkExtraData(trk);
    assert( trk != NULL );
    
        *a0 = NormalizeAngle( GetTrkEndAngle(trk,0) );
        *a1 = NormalizeAngle( GetTrkEndAngle(trk,1)  );
    
    LOG( log_cornu, 4, ( "getCornuAngles: = %0.3f %0.3f\n", *a0, *a1 ) )
}


static void ComputeCornuBoundingBox( track_p trk, struct extraData * xx )
{
    coOrd hi, lo;
    hi.x = lo.x = xx->cornuData.pos[0].x;
    hi.y = lo.y = xx->cornuData.pos[0].y;
    

    hi.x = hi.x < xx->cornuData.pos[1].x ? xx->cornuData.pos[1].x : hi.x;
    hi.y = hi.y < xx->cornuData.pos[1].y ? xx->cornuData.pos[1].y : hi.y;
    lo.x = lo.x > xx->cornuData.pos[1].x ? xx->cornuData.pos[1].x : lo.x;
    lo.y = lo.y > xx->cornuData.pos[1].y ? xx->cornuData.pos[1].y : lo.y;

    SetBoundingBox( trk, hi, lo );
}


DIST_T CornuDescriptionDistance(
		coOrd pos,
		track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	coOrd p1;

	if ( GetTrkType( trk ) != T_CORNU || ( GetTrkBits( trk ) & TB_HIDEDESC ) != 0 )
		return 100000;
	
		p1.x = xx->cornuData.pos[0].x + (xx->cornuData.pos[3].x-xx->cornuData.pos[0].x)/2 + xx->cornuData.descriptionOff.x;
		p1.y = xx->cornuData.pos[0].y + (xx->cornuData.pos[3].y-xx->cornuData.pos[0].y)/2 + xx->cornuData.descriptionOff.y;
	
	return FindDistance( p1, pos );
}


static void DrawCornuDescription(
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
    pos.x = xx->cornuData.pos[0].x + (xx->cornuData.pos[3].x - xx->cornuData.pos[0].x)/2;
    pos.y = xx->cornuData.pos[0].y + (xx->cornuData.pos[3].y - xx->cornuData.pos[0].y)/2;
    pos.x += xx->cornuData.descriptionOff.x;
    pos.y += xx->cornuData.descriptionOff.y;
    fp = wStandardFont( F_TIMES, FALSE, FALSE );
    sprintf( message, _("Cornu Curve: length=%s min radius=%s"),
				FormatDistance(xx->cornuData.length), FormatDistance(xx->cornuData.minCurveRadius));
    DrawBoxedString( BOX_BOX, d, pos, message, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
}


STATUS_T CornuDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	struct extraData *xx = GetTrkExtraData(trk);
	static coOrd p0;
	wDrawColor color;
	ANGLE_T a, a0, a1;
	DIST_T d;
	if (GetTrkType(trk) != T_CORNU) return C_TERMINATE;

	switch (action) {
	case C_DOWN:
	case C_MOVE:
	case C_UP:
		color = GetTrkColor( trk, &mainD );
		DrawCornuDescription( trk, &mainD, color );
    
        if (action != C_DOWN)
            DrawLine( &mainD, xx->cornuData.pos[0], p0, 0, wDrawColorBlack );
        xx->cornuData.descriptionOff.x = p0.x - xx->cornuData.pos[0].x + (xx->cornuData.pos[3].x-xx->cornuData.pos[0].x)/2;
        xx->cornuData.descriptionOff.y = p0.y - xx->cornuData.pos[0].x + (xx->cornuData.pos[3].y-xx->cornuData.pos[0].y)/2;
        p0 = pos;
        if (action != C_UP)
            DrawLine( &mainD, xx->cornuData.pos[0], p0, 0, wDrawColorBlack );
        DrawCornuDescription( trk, &mainD, color );
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
		coOrd pos[2];
		ANGLE_T angle[2];
		DIST_T radius[2];
		FLOAT_T elev[2];
		FLOAT_T length;
		FLOAT_T grade;
		DIST_T minRadius;
		DIST_T maxRateOfChange;
		LAYER_T layerNumber;
		ANGLE_T angle[2];
		dynArr_t segs;
		long width;
		wDrawColor color;
		} cornData;
typedef enum { P0, A0, R0, Z0, P1, A1, R1, Z1, RA, LN, GR, LY, WI, CO } crvDesc_e;
static descData_t crvDesc[] = {
/*P0*/	{ DESC_POS, N_("End Pt 1: X"), &cornData.pos[0] },
/*Z0*/	{ DESC_DIM, N_("Z1"), &cornData.elev[0] },
/*A0*/  { DESC_ANGLE, N_("End Angle 1"), &cornData.angle[0] },
/*R0*/  { DESC_RADIUS, N_("Radius 1"), &cornData.radius[0] },
/*P1*/	{ DESC_POS, N_("End Pt 2: X"), &cornData.pos[3] },
/*Z3*/	{ DESC_DIM, N_("Z2"), &cornData.elev[1] },
/*A1*/  { DESC_ANGLE, N_("End Angle 2"), &cornData.angle[1] },
/*R1*/  { DESC_RADIUS, N_("Radius 2"), &cornData.radius[1] },
/*RA*/	{ DESC_DIM, N_("MinRadius"), &cornData.minRadius },
/*RR*/  { DESC_RADIUS, N_("MaxRateOfChangeOfRadius"), &cornData.maxRateOfChange },
/*LN*/	{ DESC_DIM, N_("Length"), &cornData.length },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &cornData.grade },
/*LY*/	{ DESC_LAYER, N_("Layer"), &cornData.layerNumber },
/*WI*/  { DESC_LONG, N_("Line Width"), &cornData.width},
/*CO*/  { DESC_COLOR, N_("Line Color"), &cornData.color},
		{ DESC_NULL } };

static void UpdateCornu( track_p trk, int inx, descData_p descUpd, BOOL_T final )
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
        xx->cornuData.pos[0] = cornData.pos[0];
        crvDesc[P0].mode |= DESC_CHANGE;
        /* no break */
    case P1:
    	if (GetTrkEndTrk(trk,0) && GetTrkEndTrk(trk,1)) break;
        updateEndPts = TRUE;
        xx->cornuData.pos[1]= cornData.pos[1];
        crvDesc[P1].mode |= DESC_CHANGE;
        break;
    case A0:
    	if (GetTrkEndTrk(trk,0)) break;
    	updateEndPts = TRUE;
    	xx->cornuData.a[0] = cornData.angle[0];
    	crvDesc[A0].mode |= DESC_CHANGE;
    case A1:
    	if (GetTrkEndTrk(trk,0) && GetTrkEndTrk(trk,1)) break;
    	updateEndPts = TRUE;
    	xx->cornuData.a[1]= cornData.angle[1];
    	crvDesc[A1].mode |= DESC_CHANGE;
    	break;
    case R0:
        if (GetTrkEndTrk(trk,0)) break;
        updateEndPts = TRUE;
        xx->cornuData.r[0] = cornData.radius[0];
        crvDesc[A0].mode |= DESC_CHANGE;
    case R1:
        if (GetTrkEndTrk(trk,0) && GetTrkEndTrk(trk,1)) break;
        updateEndPts = TRUE;
        xx->cornuData.r[1]= cornData.radius[1];
        crvDesc[A1].mode |= DESC_CHANGE;
        break;
    case Z0:
	case Z1:
		ep = (inx==Z0?0:1);
		UpdateTrkEndElev( trk, ep, GetTrkEndElevUnmaskedMode(trk,ep), cornData.elev[ep], NULL );
		ComputeElev( trk, 1-ep, FALSE, &cornData.elev[1-ep], NULL );
		if ( cornData.length > minLength )
			cornData.grade = fabs( (cornData.elev[0]-cornData.elev[1])/cornData.length )*100.0;
		else
			cornData.grade = 0.0;
		crvDesc[GR].mode |= DESC_CHANGE;
		crvDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
        return;
	case LY:
		SetTrkLayer( trk, cornData.layerNumber);
		break;
	default:
		AbortProg( "updateCornu: Bad inx %d", inx );
	}
	CallCornu(xx->cornuData.pos, xx->cornuData.a, xx->cornuData.r, &xx->cornuData.arcSegs);
	if (updateEndPts) {
			if ( GetTrkEndTrk(trk,0) == NULL ) {
				SetTrkEndPoint( trk, 0, cornData.pos[0], xx->cornuData.a[0]);
				crvDesc[A0].mode |= DESC_CHANGE;
			}
			if ( GetTrkEndTrk(trk,1) == NULL ) {
				SetTrkEndPoint( trk, 1, cornData.pos[3], xx->cornuData.a[1]);
				crvDesc[A1].mode |= DESC_CHANGE;
			}
	}

	//FixUpCornu(xx->bezierData.pos, xx, IsTrack(trk));
	ComputeCornuBoundingBox(trk, xx);
	DrawNewTrack( trk );
}

static void DescribeCornu( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a0, a1;
	DIST_T d;
	int fix0, fix1 = 0;
	FLOAT_T turns;

	
	d = xx->cornuData.length;
    sprintf( str, _("Cornu Track(%d): Layer=%d MinRadius=%s Length=%s EP=[%0.3f,%0.3f] [%0.3f,%0.3f]"),
    			GetTrkIndex(trk),
				GetTrkLayer(trk)+1,
				FormatDistance(xx->cornuData.minCurveRadius),
				FormatDistance(d),
				PutDim(xx->cornuData.pos[0].x),PutDim(xx->cornuData.pos[0].y),
                PutDim(xx->cornuData.pos[1].x),PutDim(xx->cornuData.pos[1].y)
                );

	if (GetTrkType(trk) == T_CORNU) {
		fix0 = GetTrkEndTrk(trk,0)!=NULL;
		fix1 = GetTrkEndTrk(trk,1)!=NULL;
	}

	cornData.length = GetLengthCornu(trk);
	cornData.radius = xx->cornuData.minCurveRadius;
    cornData.layerNumber = GetTrkLayer(trk);
    cornData.pos[0] = xx->cornuData.pos[0];
    cornData.pos[1] = xx->cornuData.pos[1];
    cornData.angle[0] = xx->cornuData.a[0];
    cornData.angle[1] = xx->cornuData.a[1];
    cornData.radius[0] = xx->cornuData.r[0];
    cornData.radius[1] = xx->cornuData.r[1];
    if (GetTrkType(trk) == T_CORNU) {
		ComputeElev( trk, 0, FALSE, &cornData.elev[0], NULL );
		ComputeElev( trk, 1, FALSE, &cornData.elev[1], NULL );
	
		if ( cornData.length > minLength )
			cornData.grade = fabs( (cornData.elev[0]-cornData.elev[1])/cornData.length )*100.0;
		else
			cornData.grade = 0.0;
    }

	crvDesc[P0].mode = fix0?DESC_RO:0;
	crvDesc[P1].mode = fix1?DESC_RO:0;
	crvDesc[LN].mode = DESC_RO;
    if (GetTrkType(trk) == T_CORNU) {
    	crvDesc[Z0].mode = EndPtIsDefinedElev(trk,0)?0:DESC_RO;
		crvDesc[Z1].mode = EndPtIsDefinedElev(trk,1)?0:DESC_RO;
    }
    else
    	crvDesc[Z0].mode = crvDesc[Z1].mode = DESC_IGNORE;
	crvDesc[A0].mode = DESC_RO;
	crvDesc[A1].mode = DESC_RO;
	crvDesc[R0].mode = DESC_RO;
	crvDesc[R1].mode = DESC_RO;
	crvDesc[GR].mode = DESC_RO;
    crvDesc[RA].mode = DESC_RO;
	crvDesc[LY].mode = DESC_NOREDRAW;
	
	DoDescribe( _("Cornu Track"), trk, crvDesc, UpdateCornu );


}

static DIST_T DistanceCornu( track_p t, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(t);
	double s;
	//return BezierMathDistance(p,xx->bezierData.pos,100, &s);

	ANGLE_T a0, a1;
	DIST_T d = 100000.0,dd;
	coOrd p2 = xx->cornuData.pos[0];    //Set initial point
	segProcData_t segProcData;
	for (int i = 0;i<xx->cornuData.arcSegs.cnt;i++) {
		segProcData.distance.pos1 = * p;
		SegProc(SEGPROC_DISTANCE,&DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i),&segProcData);
		if (segProcData.distance.dd<d) {
			d = segProcData.distance.dd;
			p2 = segProcData.distance.pos1;
		}
	}
    //d = BezierDistance( p, xx->bezierData.pos[0], xx->bezierData.pos[1], xx->bezierData.pos[2], xx->bezierData.pos[1], 100, NULL );
	* p = p2;
	return d;
}

static void DrawCornu( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData *xx = GetTrkExtraData(t);
		track_p tt = t;
	long widthOptions = DTS_LEFT|DTS_RIGHT;


	if (GetTrkType(t) == T_BZRLIN) {
		DrawSegsO(d,t,zero,0.0,xx->cornuData.arcSegs.ptr,xx->cornuData.arcSegs.cnt);
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
		DrawCornuDescription( t, d, color );
	}
	DIST_T scale2rail = (d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale;
	if ( tieDrawMode!=TIEDRAWMODE_NONE &&
			 d!=&mapD &&
			 (d->options&DC_TIES)!=0 &&
			 d->scale<scale2rail/2 )
		DrawSegsO(d,t,zero,0.0,xx->cornuData.arcSegs.ptr,xx->cornuData.arcSegs.cnt, GetTrkGauge(t), color, widthOptions|DTS_TIES);
	DrawSegsO(d,t,zero,0.0,xx->cornuData.arcSegs.ptr,xx->cornuData.arcSegs.cnt, GetTrkGauge(t), color, widthOptions);
	if ( (d->funcs->options & wDrawOptTemp) == 0 &&
		 (d->options&DC_QUICK) == 0 ) {
		DrawEndPt( d, t, 0, color );
		DrawEndPt( d, t, 1, color );
	}
}

void FreeSubSegs(trkSeg_t* s) {
	if (s.type == SEG_BEZTRK || s.type == SEG_BEZLIN) {
		if (s.bezSegs.ptr) {
			free(s.bezSegs.ptr);
		}
		s.bezSegs.max = 0;
		s.bezSegs.cnt = 0;
	}
}

static void DeleteCornu( track_p t )
{
	struct extraData *xx = GetTrkExtraData(t);

	for (int i=0;i<xx->cornuData.arcSegs.cnt;i++) {
		trkSeg_t s = DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i);
		FreeSubSegs(&s);
	}
	if (xx->cornuData.arcSegs.ptr && !xx->cornuData.arcSegs.max)
		free(xx->cornuData.arcSegs.ptr);
	xx->cornuData.arcSegs.max = 0;
	xx->cornuData.arcSegs.cnt = 0;
}

static BOOL_T WriteCornu( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	long options;
	BOOL_T rc = TRUE;
	BOOL_T track =(GetTrkType(t)==T_CORNU);
	options = GetTrkWidth(t) & 0x0F;
	rc &= fprintf(f, "%s %d %d %ld 0 0 %s %d %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f 0 %0.6f %0.6f \n",
		"CORNU",GetTrkIndex(t), GetTrkLayer(t), (long)options,
                  GetTrkScaleName(t), GetTrkVisible(t),
				  xx->cornuData.pos[0].x, xx->cornuData.pos[0].y,
				  xx->cornuData.a[0], xx->cornuData.r[0],
				  xx->cornuData.pos[1].x, xx->cornuData.pos[1].y,
				  xx->cornuData.a[1], xx->cornuData.r[1],
				  xx->cornuData.descriptionOff.x, xx->cornuData.descriptionOff.y )>0;
	if (track) {
			rc &= WriteEndPt( f, t, 0 );
			rc &= WriteEndPt( f, t, 1 );
		}
	rc &= WriteSegs( f, xx->cornuData.arcSegs.cnt, xx->cornuData.arcSegs.ptr );
	//rc &= fprintf(f, "\tEND\n" )>0;
	return rc;
}

static void ReadCornu( char * line )
{
	struct extraData *xx;
	track_p t;
	wIndex_t index;
	BOOL_T visible;
	DIST_T r0,r1;
	ANGLE_T a0,a1;
	coOrd p0, p1, dp;
	DIST_T elev;
	char scale[10];
	wIndex_t layer;
	long options;
	char * cp = NULL;
	unsigned long rgb;
	DIST_T width;

	if (!GetArgs( line+6, "dLl00sdparp0p",
		&index, &layer, &options, scale, &visible, &p0, &a0, &r0, &p1, &a1, &r1, &dp ) ) {
		return;
	}
	t = NewTrack( index, T_CORNU, 0, sizeof *xx );

	xx = GetTrkExtraData(t);
	SetTrkVisible(t, visible);
	SetTrkScale(t, LookupScale(scale));
	SetTrkLayer(t, layer );
	SetTrkWidth(t, (int)(options&0x0F));
	xx->cornuData.pos[0] = p0;
    xx->cornuData.pos[1] = p1;
    xx->cornuData.a[0] = a0;
    xx->cornuData.r[0] = r0;
    xx->cornuData.a[1] = a1;
    xx->cornuData.r[1] = r1;
    xx->cornuData.descriptionOff = dp;
    ReadSegs();
    FixUpCornu(xx->cornuData.pos,xx->cornuData.a, xx->cornuData.r, xx);
    ComputeCornuBoundingBox(t,xx);
		SetEndPts(t,2);
}

static void MoveCornu( track_p trk, coOrd orig )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
    for (int i=0;i<2;i++) {
        xx->cornuData.pos[i].x += orig.x;
        xx->cornuData.pos[i].y += orig.y;
    }
    FixUpCornu(xx->cornuData.pos, xx->cornuData.a, xx->cornuData.r, xx);
    ComputeCornuBoundingBox(trk,xx);

}

static void RotateCornu( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
    for (int i=0;i<2;i++) {
        Rotate( &xx->cornuData.pos[i], orig, angle );
        xx->cornuData.a[i] = NormalizeAngle(xx->cornuData.a[i]+angle);
    }

    FixUpCornu(xx->cornuData.pos, xx->cornuData.a, xx->cornuData.r, xx);
    ComputeCornuBoundingBox(trk,xx);

}

static void RescaleCornu( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
	xx->cornuData.pos[0].x *= ratio;
	xx->cornuData.pos[0].y *= ratio;
    xx->cornuData.pos[1].x *= ratio;
    xx->cornuData.pos[1].y *= ratio;

    FixUpCornu(xx->cornuData.pos, xx->cornuData.a, xx->cornuData.r, xx);
    ComputeCornuBoundingBox(trk,xx);

}

static void AdjustCornuEndPt( track_p trk, EPINX_T inx, coOrd pos ) {
    struct extraData *xx = GetTrkExtraData(trk);
    UndoModify(trk);
    if (inx ==0 ) {
        xx->cornuData.pos[0] = pos;
    }
    else {
        xx->cornuData.pos[1] = pos;
    }
    FixUpCornu(xx->cornuData.pos, xx->cornuData.a, xx->cornuData.r, xx);
    ComputeCornuBoundingBox(trk,xx);
    SetTrkEndPoint( trk, inx, pos, xx->cornuData.a[inx]);
}


/**
 * Split the Track at approximately the point pos.
 */
static BOOL_T SplitCornu( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	track_p trk1,trk2;
    double t;
    BOOL_T track;
    track = IsTrack(trk);
    
    cornuParm_t newl, newr;


    //TODO find right Seg and point, Create new Cornu from there to end, Adjust old

    double dd = DistanceCornu(trk, &pos);
    if (dd>minLength) return FALSE;
    

    CornuGetParameters(trk, pos, &newl);

    //CornuMathDistance(&pos, xx->bezierData.pos, 100, &t);  //Find t value

    //CornuSplit(xx->bezierData.pos, &newl[0], &newr[0], t);


    trk1 = NewCornuTrack(ep?newr:newl,NULL,0);

	UndoModify(trk);
	for (int i=0;i<4;i++) {
		xx->cornuData.pos[i] = ep?newl.pos[i]:newr.pos[i];
		xx->cornuData.a[i] = ep?newl.angle[i]:newr.angle[i];
		xx->cornuData.r[i] = ep?newl.radius[i]:newr.radius[i];
	}

    FixUpCornu(xx->cornuData.pos,xx->cornuData.a,xx->cornuData.r, xx);
    ComputeCornuBoundingBox(trk,xx);
    SetTrkEndPoint( trk, ep, xx->cornuData.pos[ep], xx->cornuData.a[ep]);

	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;

	return TRUE;
}

static int log_traverseCornu = 0;
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
static BOOL_T TraverseCornu( traverseTrack_p trvTrk, DIST_T * distR )
{
    track_p trk = trvTrk->trk;
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T dist = *distR;
	segProcData_t segProcData;
	BOOL_T backwards=FALSE;
	BOOL_T reverse_seg = FALSE;
	BOOL_T segs_backwards= FALSE;
	DIST_T d = 10000;
	coOrd pos0, pos1, pos2 = trvTrk->pos;
	ANGLE_T a1,a2;
	int inx,segInx = 0;
	EPINX_T ep, ep2;
	BOOL_T back;
	trkSeg_p segPtr = (trkSeg_p)xx->cornuData.arcSegs.ptr;

	a2 = GetAngleSegs(		  						//Find correct Segment and nearest point in it
				xx->cornuData.arcSegs.cnt,segPtr,
				&pos2, &segInx, &d , &back );   	//d = how far pos2 from old pos2 = trvTrk->pos

	if ( d > 10 ) {
			ErrorMessage( "traverseCornu: Position is not near track: %0.3f", d );
			return FALSE;   						//This means the input pos is not on or close to the track.
	}
	a1 = NormalizeAngle(a2-trvTrk->angle);						//Establish if we are going fwds or backwards globally
	if (!back) a1=NormalizeAngle(a1+180);						//(GetAngle Segs doesn't consider our L->R order for angles)
	if (a1 <270 && a1>90) {										//Must add 180 if the seg is reversed or inverted (but not both)
		segs_backwards = TRUE;
		ep = 0;
	} else {
		segs_backwards = FALSE;
		ep = 1;
	}

	segProcData.traverse1.pos = pos2;								//actual point on curve
	segProcData.traverse1.angle = NormalizeAngle(trvTrk->angle+180); //direction car is going for Traverse 1 has to be reversed...

	inx = segInx;
	while (inx >=0 && inx<xx->cornuData.arcSegs.cnt) {
		segPtr = (trkSeg_p)xx->cornuData.arcSegs.ptr+inx;  	//move in to the identified segment
		SegProc( SEGPROC_TRAVERSE1, segPtr, &segProcData );   	//Backwards or forwards for THIS segment - note that this can differ from segs_backward!!
		backwards = segProcData.traverse1.backwards;			//do we process this segment backwards (it is ?
		reverse_seg = segProcData.traverse1.reverse_seg;		//is it a backwards segment (we don't actually care as Traverse1 takes care of it)

		dist += segProcData.traverse1.dist;

		segProcData.traverse2.dist = dist;
		segProcData.traverse2.segDir = backwards;

		SegProc( SEGPROC_TRAVERSE2, segPtr, &segProcData );		//Angle at pos2
		if ( segProcData.traverse2.dist <= 0 ) {				//-ve or zero distance left over so stop there
			*distR = 0;
			trvTrk->pos = segProcData.traverse2.pos;
			trvTrk->angle = segProcData.traverse2.angle;
			return TRUE;
		}														//NOTE Traverse1 and Traverse2 are overlays so get all out before storing
		dist = segProcData.traverse2.dist;						//How far left?
		coOrd pos = segProcData.traverse2.pos;					//Will be at seg end
		ANGLE_T angle = segProcData.traverse2.angle;			//Angle of end
		segProcData.traverse1.angle = NormalizeAngle(angle+180);//Reverse to suit Traverse1
		segProcData.traverse1.pos = pos;
		inx = segs_backwards?inx-1:inx+1;						//Here's where the global segment direction comes in
LOG( log_traverseCornu, 1, ( " D%0.3f\n", dist ) )
	}
	*distR = dist;								//Tell caller what is left
												//Must be at one end or another
	trvTrk->pos = GetTrkEndPos(trk,ep);
	trvTrk->angle = NormalizeAngle(GetTrkEndAngle(trk, ep)+segs_backwards?180:0);
	trvTrk->trk = GetTrkEndTrk(trk,ep);						//go to next track
	if (trvTrk->trk==NULL) {
		trvTrk->pos = pos1;
	    return TRUE;
	}

LOG( log_traverseCornu, 1, ( "  -> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR ) )
	return TRUE;

}


static BOOL_T EnumerateCornu( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	ANGLE_T a0, a1;
	DIST_T d;
	if (trk != NULL) {
		xx = GetTrkExtraData(trk);
		d = xx->cornuData.minCurveRadius;
		ScaleLengthIncrement( GetTrkScale(trk), d );
	}
	return TRUE;
}

static BOOL_T MergeCornu(
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
	BOOL_T tracks = FALSE;

	if (IsTrack(trk0) && IsTrack(trk1) ) tracks = TRUE;
	if (GetTrkType(trk0) != GetTrkType(trk1)) return FALSE;

	if (ep0 == ep1)
		return FALSE;
    
	UndoStart( _("Merge Cornu"), "MergeCornu( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
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
		xx0->cornuData.pos[1] = xx1->cornuData.pos[1];
	} else {
		xx0->cornuData.pos[0] = xx1->cornuData.pos[0];
	}
	FixUpCornu(xx0->cornuData.pos,xx0->cornuData.a,xx0->cornuData.r, xx0);
	ComputeCornuBoundingBox(trk0,xx0);
	DeleteTrack( trk1, FALSE );
	if (trk2 && tracks) {
		if (ep0 == 1)
			SetTrkEndPoint( trk2, 1, xx0->cornuData.pos[0], xx0->cornuData.a[0]);
		else
			SetTrkEndPoint( trk2, 2, xx0->cornuData.pos[1], xx0->cornuData.a[1]);
		ConnectTracks( trk0, ep0, trk2, ep2 );
	}
	DrawNewTrack( trk0 );


	return TRUE;
}


static DIST_T GetLengthCornu( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T length = 0.0;
	segProcData_t segProcData;
	for(int i=0;i<xx->cornuData.arcSegs.cnt;i++) {
		SegProc(SEGPROC_LENGTH,&(DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i)), &segProcData);
		length += segProcData.length.length;
	}
	return length;
}


static BOOL_T GetParamsCornu( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	params->type = curveTypeCornu;
	struct extraData *xx = GetTrkExtraData(trk);
	for (int i=0;i<2;i++) {
		params->cornuEnd[i] = xx->cornuData.pos[i];
		params->cornuAngle[i] = xx->cornuData.a[i];
		params->cornuRadius[i] = xx->cornuData.r[i];
	}
	params->len = xx->cornuData.length;
	params->ep = PickUnconnectedEndPoint( pos, trk);
	if (params->ep == -1)
		return FALSE;
	return TRUE;

}



static BOOL_T QueryCornu( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	switch ( query ) {
	case Q_CAN_GROUP:
		return FALSE;
	case Q_FLIP_ENDPTS:
	case Q_HAS_DESC:
		return TRUE;
	case Q_EXCEPTION:
		return xx->cornuData.minCurveRadius < minTrackRadius;
	case Q_CAN_MODIFY_CONTROL_POINTS:
		return TRUE;
	case Q_ISTRACK:
		return TRUE;
	case Q_CAN_PARALLEL:
		return TRUE;
	default:
		return FALSE;
	}
}


static void FlipCornu(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->cornuData.pos[0], orig, angle );
	FlipPoint( &xx->cornuData.pos[1], orig, angle );
    &xx->cornuData.a[0]+=angle;

    FixUpCornu(xx->cornuData.pos,xx->cornuData.a, xx->cornuData.r,xx);
    ComputeCornuBoundingBox(trk,xx);

}

static ANGLE_T GetAngleCornu(
		track_p trk,
		coOrd pos,
		EPINX_T * ep0,
		EPINX_T * ep1 )
{
	struct extraData * xx = GetTrkExtraData(trk);
	ANGLE_T angle;
	int indx;
	angle = GetAngleSegs( xx->cornuData.arcSegs.cnt, (trkSeg_p)xx->cornuData.arcSegs.ptr, &pos, &indx, NULL, NULL );
	if ( ep0 ) *ep0 = -1;
	if ( ep1 ) *ep1 = -1;
	return angle;
}

BOOL_T GetCornuSegmentFromTrack(track_p trk, trkSeg_p seg_p) {
	struct extraData * xx = GetTrkExtraData(trk);

	seg_p->type = IsTrack(trk)?SEG_BEZTRK:SEG_BEZLIN;
	for (int i=0;i<4;i++) seg_p->u.b.pos[i] = xx->cornuData.pos[i];
	seg_p->bezSegs.cnt = 0;
	seg_p->bezSegs.max = 0;
	seg_p->bezSegs.ptr = NULL;
	FixUpCornuSeg(seg_p->u.b.pos,seg_p,seg_p->type == SEG_BEZTRK);


	return TRUE;

}


static BOOL_T MakeParallelCornu(
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
    ANGLE_T a,a2, na[2];
    DIST_T nr[2];

	//Produce cornu that is translated parallel to the existing Cornu
    // - not a precise result if the cornu end angles are not in the same general direction.
    // The expectation is that the user will have to adjust it - unless and until we produce
    // a new algo to adjust the control points to be parallel to the endpoints.
    
    a = xx->cornuData.a[0];
    p = pos;
    DistanceCornu(trk, &p);
    a2 = NormalizeAngle(FindAngle(pos,p)-a);
    //find parallel move x and y for points
    for (int i =0; i<2;i++) {
    	np[i] = xx->cornuData.pos[i];
    }

    if ( a2 > 180 ) {
        Translate(&np[0],np[0],a+90,sep);
        Translate(&np[1],np[1],a+90,sep);
        na[0]=xx->cornuData.a[0];
        na[1]=xx->cornuData.a[1];
        if (xx->cornuData.r[0])
        	nr[0]=xx->cornuData.r[0]+sep;
        if (xx->cornuData.r[1])
        	nr[1]=xx->cornuData.r[1]+sep;
    } else {
        Translate(&np[0],np[0],a-90,sep);
        Translate(&np[1],np[1],a-90,sep);
        na[0]=xx->cornuData.a[0];
        na[1]=xx->cornuData.a[1];
        if (xx->cornuData.r[0])
            nr[0]=xx->cornuData.r[0]-sep;
        if (xx->cornuData.r[1])
            nr[1]=xx->cornuData.r[1]-sep;
    }

	if ( newTrkR ) {
		*newTrkR = NewCornuTrack( np, na, nr, NULL, 0);
	} else {
		//Dont have a SEG_CORNU yet...
		return FALSE;
	}
	if ( p0R ) p0R = &np[0];
	if ( p1R ) p1R = &np[1];
	return TRUE;
}

/*
 * When an undo is run, the array of segs is missing - they are not saved to the Undo log. So Undo calls this routine to
 * ensure
 * - that the Segs are restored and
 * - other fields reset.
 */
BOOL_T RebuildCornu (track_p trk)
{
	struct extraData *xx;
	xx = GetTrkExtraData(trk);
	xx->cornuData.arcSegs.max = 0;
	xx->cornuData.arcSegs.cnt = 0;
	xx->cornuData.arcSegs.ptr = NULL;
	FixUpCornu(xx->cornuData.pos,xx->cornuData.a,xx->cornuData.r, xx);
	ComputeCornuBoundingBox(trk, xx);
	return TRUE;
}


static trackCmd_t cornuCmds = {
		"CORNU",
		DrawCornu,
		DistanceCornu,
		DescribeCornu,
		DeleteCornu,
		WriteCornu,
		ReadCornu,
		MoveCornu,
		RotateCornu,
		RescaleCornu,
		NULL,
		GetAngleCornu,
		SplitCornu,
		TraverseCornu,
		EnumerateCornu,
		NULL,	/* redraw */
		NULL,   /* trim   */
		MergeCornu,
		NULL,   /* modify */
		GetLengthCornu,
		GetParamsCornu,
		NULL, /* Move EndPt */
		QueryCornu,
		NULL,	/* ungroup */
		FlipCornu,
		NULL,
		NULL,
		NULL,
		MakeParallelCornu,
		NULL,
		RebuildCornu
		};


EXPORT void CornuSegProc(
		segProc_e cmd,
		trkSeg_p segPtr,
		segProcData_p data )
{
	ANGLE_T a0, a1, a2;
	DIST_T d, dd, circum, d0;
	coOrd p0,p2 ;
	wIndex_t s0, s1;
	EPINX_T ep;
	segProcData_t segProcData;
	trkSeg_p subSegsPtr;
	coOrd temp0,temp1,temp2,temp3;
	int inx,segInx;
	BOOL_T back, segs_backwards, backwards, reverse_seg;
#define bezSegs(N) DYNARR_N( trkSeg_t, segPtr->bezSegs, N )

    switch (cmd) {

	case SEGPROC_TRAVERSE1:
		if (segPtr->type != SEG_BEZTRK) {
			data->traverse1.dist = 0;
			return;
		}
		d = data->traverse1.dist;
		p0 = data->traverse1.pos;
		a2 = GetAngleSegs(segPtr->bezSegs.cnt,segPtr->bezSegs.ptr,&p0,&segInx,&d,&back); //Find right seg and pos
		if (d>10) {
			data->traverse1.dist = 0;
			return;
		}
		a1 = NormalizeAngle(a2-data->traverse1.angle);				//Establish if we are going fwds or backwards globally
		if (!back) a1=NormalizeAngle(a1+180);						//(GetAngle Segs doesn't consider our L->R order for angles)
			if (a1 <270 && a1>90) {									//Must add 180 if the seg is reversed or inverted (but not both)
				segs_backwards = TRUE;
				ep = 0;
	    } else  {
				segs_backwards = FALSE;
				ep = 1;
	    }
	    segProcData.traverse1.pos = data->traverse1.pos = p0;					//actual point on curve
		segProcData.traverse1.angle = NormalizeAngle(data->traverse1.angle+180); //direction car is going for Traverse 1 has to be reversed...

		inx = segInx;
		subSegsPtr = (trkSeg_p)segPtr->bezSegs.ptr+inx;
		SegProc( SEGPROC_TRAVERSE1, subSegsPtr, &segProcData );
		data->traverse1.backwards = segProcData.traverse1.backwards;
		data->traverse1.reverse_seg = segProcData.traverse1.reverse_seg;
		data->traverse1.dist = segProcData.traverse1.dist;					//Get last seg partial dist
		inx = segs_backwards?inx-1:inx+1;
		while( inx>=0 && inx<segPtr->bezSegs.cnt) {
		    subSegsPtr = (trkSeg_p)segPtr->bezSegs.ptr+inx;
			data->traverse1.dist += subSegsPtr->u.b.length;					//Add up total distance to get to here...
			inx = segs_backwards?inx+1:inx-1;
		}
		break;

	case SEGPROC_TRAVERSE2:
		if (segPtr->type != SEG_BEZTRK) return;							//Not SEG_BEZLIN
		if (data->traverse2.dist <= segPtr->u.b.length) {
			d = data->traverse2.segDir?segProcData.traverse2.dist:segPtr->u.b.length-segProcData.traverse2.dist; //reverse distance
			data->traverse2.angle = data->traverse2.segDir?NormalizeAngle(segProcData.traverse2.angle+180):segProcData.traverse2.angle;
			segProcData.traverse2.segDir = 0;     //Now can go forward as long as we flip angle at end
			segProcData.traverse2.dist = d;
			for (inx=0;inx<segPtr->bezSegs.cnt ;inx++) {     //Go from start to end
				CornuSegProc(SEGPROC_TRAVERSE2,&bezSegs(inx),&segProcData);
				if (segProcData.traverse2.dist<=0) {									//Done
					data->traverse2.angle = data->traverse2.segDir?NormalizeAngle(segProcData.traverse2.angle+180.0):segProcData.traverse2.angle; //Flip
					data->traverse2.dist = 0;
					data->traverse2.pos = segProcData.traverse2.pos;
					return;
				}
			}
		}
		data->traverse2.dist -=segPtr->u.b.length;  //we got here because the desired point is not inside the segment
    	if (data->traverse2.segDir) {
    		data->traverse2.pos = segPtr->u.b.pos[0];					// Backwards so point 0
    		data->traverse2.angle = NormalizeAngle(segPtr->u.b.angle0 +180);
    	} else {
    		data->traverse2.pos = segPtr->u.b.pos[3];					//Forwards so point 3
    		data->traverse2.angle = NormalizeAngle(segPtr->u.b.angle3 +180);
    	}
		break;

	case SEGPROC_DRAWROADBEDSIDE:
        //TODO - needs parallel bezier problem solved...
		break;

	case SEGPROC_DISTANCE:

		dd = FindDistance(data->distance.pos1,segPtr->u.b.pos[0]);   //Just find one distance
		p0 = data->distance.pos1;
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
		FixUpCornuSeg(segPtr->u.b.pos,segPtr,segPtr->type == SEG_BEZTRK);
		break;

	case SEGPROC_NEWTRACK:
		data->newTrack.trk = NewCornuTrack( segPtr->u.b.pos, (trkSeg_t *)segPtr->bezSegs.ptr, segPtr->bezSegs.cnt);
		break;

	case SEGPROC_LENGTH:
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
		subSegsPtr = (trkSeg_p) segPtr->bezSegs.ptr;
		coOrd pos = segProcData.getAngle.pos;
		data->getAngle.angle = GetAngleSegs(segPtr->bezSegs.cnt,subSegsPtr, &pos, &inx, NULL, &back);
										//Recurse for Bezier sub-segs (only straights and curves)
		data->getAngle.negative_radius = back;
		data->getAngle.backwards = segPtr->u.b.angle0>=90 && segPtr->u.b.angle0<270;
		break;
    
	}

}


/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */



EXPORT void PlotCornu(
		long mode,
		coOrd pos0,
		coOrd pos1,
		coOrd pos2,
		cornuData_t * CornuData,
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



track_p NewCornuTrack(coOrd pos[2], ANGLE_T angle[2], DIST_T radius[2], trkSeg_t * tempsegs, int count)
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_CORNU, 2, sizeof *xx );
	SetTrkScale( p, curScaleInx );
	xx = GetTrkExtraData(p);
    xx->cornuData.pos[0] = pos[0];
    xx->cornuData.pos[1] = pos[1];
    xx->cornuData.a[0] = angle[0];
    xx->cornuData.r[0] = radius[0];
    xx->cornuData.r[1] = radius[1];
    FixUpCornu(xx->cornuData.pos,xx->cornuData.a,xx->cornuData.r, xx);
LOG( log_cornu, 1, ( "NewCornuTrack( EP1 %0.3f, %0.3f, CP1 %0.3f, %0.3f, CP2 %0.3f, %0.3f, EP2 %0.3f, %0.3f )  = %d\n", pos[0].x, pos[0].y, pos[1].x, pos[1].y, pos[2].x, pos[2].y, pos[3].x, pos[3].y, GetTrkIndex(p) ) )
	ComputeCornuBoundingBox( p, xx );
	SetTrkEndPoint( p, 0, pos[0], xx->cornuData.a[0]);
	SetTrkEndPoint( p, 1, pos[1], xx->cornuData.a[1]);
	CheckTrackLength( p );
	return p;
}


EXPORT void InitTrkCornu( void )
{
	T_CORNU = InitObject( &cornuCmds );
	log_cornu = LogFindIndex( "Cornu" );
	log_traverseCornu = LogFindIndex( "traverseCornu" );
}


/**
 * Get Maximum Curvature
 */
extern double CornuMaxCurve(coOrd p[4]) {
    double max = 0;
    for (int t = 0;t<100;t++) {
        double curv = CornuCurvature(p, t/100, NULL);
        if (max<curv) max = curv;
    }
    return max;
}

/**
 * Get Minimum Radius
 */
extern double CornuMathMinRadius(coOrd p[4]) {
    double curv = CornuMaxCurve(p);
    if (curv >= 1000.0 || curv <= 0.001 ) return 0.0;
    return 1/curv;
}

