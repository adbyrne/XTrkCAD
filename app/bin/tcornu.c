/** \file tcornu.c
 *
 * CORNU SPIRAL TRACK
 *
 * A Cornu is a spiral arc defined by a polynomial that has the property
 * that curvature varies linearly with distance along the curve. It is a family
 * of curves that include Euler spirals.
 *
 * In order to be useful in XtrkCAD it is defined as a set of Bezier curves each of
 * which is defined as a set of circular arcs and lines.
 *
 * The derivation of the Beziers is done by the Cornu library which must be recalled
 * whenever a change is made in the end conditions.
 *
 * A cornu has a minimum radius and a maximum rate of change of radius.
 *
 * Acknowledgment is given to Dr. Raph Levien whose seminal PhD work on Cornu curves and
 * generous open-sourcing of his libraries both inspired and powers this function.
 *
 *
 *  XTrkCad - Model Railroad CAD
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
#include "tcornu.h"
#include "ccornu.h"
#include "ccurve.h"
#include "cstraigh.h"
#include "cjoin.h"
#include "utility.h"
#include "common.h"
#include "i18n.h"
#include "param.h"
#include "math.h"
#include "string.h"
#include "cundo.h"
#include "layout.h"
#include "fileio.h"
#include "assert.h"

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
void SetUpCornuParmFromTracks(track_p trk[2],cornuParm_t * cp, struct extraData* xx) {
	if (!trk[0]) {
		cp->center[0] = xx->cornuData.c[0];
		cp->angle[0] = xx->cornuData.a[0];
		cp->radius[0] = xx->cornuData.r[0];
	}
	if (!trk[1]) {
		cp->center[1] = xx->cornuData.c[1];
		cp->angle[1] = xx->cornuData.a[1];
		cp->radius[1] = xx->cornuData.r[1];
	}
}

EXPORT BOOL_T FixUpCornu(coOrd pos[2], track_p trk[2], EPINX_T ep[2], struct extraData* xx) {

	cornuParm_t cp;

	SetUpCornuParmFromTracks(trk,&cp,xx);

	if (!CallCornu(pos, trk, ep, &xx->cornuData.arcSegs, &cp)) return FALSE;

	xx->cornuData.r[0] = cp.radius[0];
	if (cp.radius[0]==0) {
		xx->cornuData.a[0] = cp.angle[0];
	} else {
		xx->cornuData.c[0] = cp.center[0];
	}
	xx->cornuData.r[1] = cp.radius[1];
	if (cp.radius[1]==0) {
		xx->cornuData.a[1] = cp.angle[1];
	} else {
		xx->cornuData.c[1] = cp.center[1];
	}

	xx->cornuData.minCurveRadius = CornuMinRadius(pos,xx->cornuData.arcSegs);
	xx->cornuData.windingAngle = CornuTotalWindingArc(pos,xx->cornuData.arcSegs);
	DIST_T last_c;
	if (xx->cornuData.r[0] == 0) last_c = 0;
		else last_c = 1/xx->cornuData.r[0];
	xx->cornuData.maxRateofChange = CornuMaxRateofChangeofCurvature(pos,xx->cornuData.arcSegs,&last_c);
	xx->cornuData.length = CornuLength(pos, xx->cornuData.arcSegs);
	return TRUE;
}

EXPORT BOOL_T FixUpCornu0(coOrd pos[2],coOrd center[2],ANGLE_T angle[2],DIST_T radius[2],struct extraData* xx) {
	DIST_T last_c;
	if (!CallCornu0(pos, center, angle, radius,&xx->cornuData.arcSegs,FALSE)) return FALSE;
	xx->cornuData.minCurveRadius = CornuMinRadius(pos,
				xx->cornuData.arcSegs);
	if (xx->cornuData.r[0] == 0) last_c = 0;
	else last_c = 1/xx->cornuData.r[0];
	xx->cornuData.maxRateofChange = CornuMaxRateofChangeofCurvature(pos,xx->cornuData.arcSegs,&last_c);
	xx->cornuData.length = CornuLength(pos, xx->cornuData.arcSegs);
	xx->cornuData.windingAngle = CornuTotalWindingArc(pos,xx->cornuData.arcSegs);
	return TRUE;
}

EXPORT char * CreateSegPathList(track_p trk) {
	char * cp = "\0\0";
	if (GetTrkType(trk) != T_CORNU) return cp;
	struct extraData *xx = GetTrkExtraData(trk);
	if (xx->cornuData.cornuPath) free(xx->cornuData.cornuPath);
	xx->cornuData.cornuPath = malloc(xx->cornuData.arcSegs.cnt+2);
	int j= 0;
	for (int i = 0;i<xx->cornuData.arcSegs.cnt;i++,j++) {
		xx->cornuData.cornuPath[j] = i+1;
	}
	xx->cornuData.cornuPath[j] = cp[0];
	xx->cornuData.cornuPath[j+1] = cp[0];
	return xx->cornuData.cornuPath;
}


static void GetCornuAngles( ANGLE_T *a0, ANGLE_T *a1, track_p trk )
{
    assert( trk != NULL );
    
        *a0 = NormalizeAngle( GetTrkEndAngle(trk,0) );
        *a1 = NormalizeAngle( GetTrkEndAngle(trk,1)  );
    
    LOG( log_cornu, 4, ( "getCornuAngles: = %0.3f %0.3f\n", *a0, *a1 ) )
}


static void ComputeCornuBoundingBox( track_p trk, struct extraData * xx )
{
    coOrd orig, size;

    GetSegBounds(zero,0,xx->cornuData.arcSegs.cnt,xx->cornuData.arcSegs.ptr, &orig, &size);

    coOrd hi, lo;
    
    lo.x = orig.x;
    lo.y = orig.y;
    hi.x = orig.x+size.x;
    hi.y = orig.y+size.y;

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
	
		p1.x = xx->cornuData.pos[0].x + ((xx->cornuData.pos[1].x-xx->cornuData.pos[0].x)/2) + xx->cornuData.descriptionOff.x;
		p1.y = xx->cornuData.pos[0].y + ((xx->cornuData.pos[1].y-xx->cornuData.pos[0].y)/2) + xx->cornuData.descriptionOff.y;
	
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
    pos.x = xx->cornuData.pos[0].x + ((xx->cornuData.pos[1].x - xx->cornuData.pos[0].x)/2);
    pos.y = xx->cornuData.pos[0].y + ((xx->cornuData.pos[1].y - xx->cornuData.pos[0].y)/2);
    pos.x += xx->cornuData.descriptionOff.x;
    pos.y += xx->cornuData.descriptionOff.y;
    fp = wStandardFont( F_TIMES, FALSE, FALSE );
    sprintf( message, _("Cornu Curve: length=%0.3f min radius=%0.3f"),
						xx->cornuData.length, xx->cornuData.minCurveRadius);
    DrawBoxedString( BOX_BOX, d, pos, message, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
}


STATUS_T CornuDescriptionMove(
		track_p trk,
		wAction_t action,
		coOrd pos )
{
	struct extraData *xx = GetTrkExtraData(trk);
	static coOrd p0,p1;
	static BOOL_T editState;
	wDrawColor color;

	if (GetTrkType(trk) != T_CORNU) return C_TERMINATE;

	p0.x = xx->cornuData.pos[0].x + ((xx->cornuData.pos[1].x - xx->cornuData.pos[0].x)/2);
	p0.y = xx->cornuData.pos[0].y + ((xx->cornuData.pos[1].y - xx->cornuData.pos[0].y)/2);

	switch (action) {
	case C_DOWN:
	case C_MOVE:
	case C_UP:
		editState = TRUE;
		p1 = pos;
		color = GetTrkColor( trk, &mainD );
        xx->cornuData.descriptionOff.x = pos.x - p0.x;
        xx->cornuData.descriptionOff.y = pos.y - p0.y;
        DrawCornuDescription( trk, &mainD, color );
        if (action == C_UP) {
        	editState = FALSE;
        }
		MainRedraw();
		MapRedraw();
		return action==C_UP?C_TERMINATE:C_CONTINUE;

	case C_REDRAW:
		if (editState)
			DrawLine( &mainD, p1, p0, 0, wDrawColorBlack );
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
		coOrd center[2];
		FLOAT_T elev[2];
		FLOAT_T length;
		FLOAT_T grade;
		DIST_T minRadius;
		DIST_T maxRateOfChange;
		ANGLE_T windingAngle;
		unsigned int layerNumber;
		dynArr_t segs;
		long width;
		wDrawColor color;
		} cornData;

typedef enum { P0, A0, R0, C0, Z0, P1, A1, R1, C1, Z1, RA, RR, WA, LN, GR, LY } cornuDesc_e;
static descData_t cornuDesc[] = {
/*P0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &cornData.pos[0] },
/*A0*/  { DESC_ANGLE, N_("End Angle"), &cornData.angle[0] },
/*R0*/  { DESC_DIM, N_("Radius "), &cornData.radius[0] },
/*C0*/	{ DESC_POS, N_("Center X,Y"), &cornData.center[0] },
/*Z0*/	{ DESC_DIM, N_("Z1"), &cornData.elev[0] },
/*P1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &cornData.pos[1] },
/*A1*/  { DESC_ANGLE, N_("End Angle"), &cornData.angle[1] },
/*R1*/  { DESC_DIM, N_("Radius"), &cornData.radius[1] },
/*C1*/	{ DESC_POS, N_("Center X,Y"), &cornData.center[1] },
/*Z1*/	{ DESC_DIM, N_("Z2"), &cornData.elev[1] },
/*RA*/	{ DESC_DIM, N_("Minimum Radius"), &cornData.minRadius },
/*RR*/  { DESC_DIM, N_("Maximum Rate Of Change Of Curvature"), &cornData.maxRateOfChange },
/*WA*/  { DESC_ANGLE, N_("Total Winding Angle"), &cornData.windingAngle },
/*LN*/	{ DESC_DIM, N_("Length"), &cornData.length },
/*GR*/	{ DESC_FLOAT, N_("Grade"), &cornData.grade },
/*LY*/	{ DESC_LAYER, N_("Layer"), &cornData.layerNumber },
		{ DESC_NULL } };


static void UpdateCornu( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	struct extraData *xx = GetTrkExtraData(trk);
	BOOL_T updateEndPts;
	EPINX_T ep;

	cornuParm_t cp;
	if ( inx == -1 )
		return;
	updateEndPts = FALSE;
	UndrawNewTrack(trk);
	switch ( inx ) {
	case P0:
		if (GetTrkEndTrk(trk,0)) break;
		updateEndPts = TRUE;
		xx->cornuData.pos[0] = cornData.pos[0];
		Translate(&xx->cornuData.c[0],xx->cornuData.pos[0],xx->cornuData.a[0]+90,xx->cornuData.r[0]);
		cornData.center[0] = xx->cornuData.c[0];
		cornuDesc[P0].mode |= DESC_CHANGE;
		cornuDesc[C0].mode |= DESC_CHANGE;
		/* no break */
	case P1:
		if (GetTrkEndTrk(trk,1)) break;
		updateEndPts = TRUE;
		xx->cornuData.pos[1]= cornData.pos[1];
		Translate(&xx->cornuData.c[1],xx->cornuData.pos[1],xx->cornuData.a[1]-90,xx->cornuData.r[1]);
		cornData.center[1] = xx->cornuData.c[1];
		cornuDesc[P1].mode |= DESC_CHANGE;
		cornuDesc[C1].mode |= DESC_CHANGE;
		break;
	case A0:
		if (GetTrkEndTrk(trk,0)) break;
		updateEndPts = TRUE;
		xx->cornuData.a[0] = cornData.angle[0];
		Translate(&xx->cornuData.c[0],xx->cornuData.pos[0],xx->cornuData.a[0]+90,xx->cornuData.r[0]);
		cornData.center[0] = xx->cornuData.c[0];
		cornuDesc[A0].mode |= DESC_CHANGE;
		cornuDesc[C0].mode |= DESC_CHANGE;
		break;
	case A1:
		if (GetTrkEndTrk(trk,1)) break;
		updateEndPts = TRUE;
		xx->cornuData.a[1]= cornData.angle[1];
		Translate(&xx->cornuData.c[1],xx->cornuData.pos[1],xx->cornuData.a[1]-90,xx->cornuData.r[1]);
		cornData.center[1] = xx->cornuData.c[1];
		cornuDesc[A1].mode |= DESC_CHANGE;
		cornuDesc[C1].mode |= DESC_CHANGE;
		break;
	case C0:
		if (GetTrkEndTrk(trk,0)) break;
		//updateEndPts = TRUE;
		//xx->cornuData.c[0] = cornData.center[0];
		//cornuDesc[C0].mode |= DESC_CHANGE;
		break;
	case C1:
		if (GetTrkEndTrk(trk,1)) break;
		//updateEndPts = TRUE;
		//xx->cornuData.c[1] = cornData.center[1];
		//cornuDesc[C1].mode |= DESC_CHANGE;
		break;
	case R0:
		if (GetTrkEndTrk(trk,0)) break;
		updateEndPts = TRUE;
		xx->cornuData.r[0] = cornData.radius[0];
		Translate(&xx->cornuData.c[0],xx->cornuData.pos[0],NormalizeAngle(xx->cornuData.a[0]+90),xx->cornuData.r[0]);
		cornData.center[0] = xx->cornuData.c[0];
		cornuDesc[R0].mode |= DESC_CHANGE;
		cornuDesc[C0].mode |= DESC_CHANGE;
		break;
	case R1:
		if (GetTrkEndTrk(trk,1)) break;
		updateEndPts = TRUE;
		xx->cornuData.r[1]= cornData.radius[1];
		Translate(&xx->cornuData.c[1],xx->cornuData.pos[1],NormalizeAngle(xx->cornuData.a[1]-90),xx->cornuData.r[1]);
		cornData.center[1] = xx->cornuData.c[1];
		cornuDesc[R1].mode |= DESC_CHANGE;
		cornuDesc[C1].mode |= DESC_CHANGE;
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
		cornuDesc[GR].mode |= DESC_CHANGE;
		cornuDesc[inx==Z0?Z1:Z0].mode |= DESC_CHANGE;
		return;
	case LY:
		SetTrkLayer( trk, cornData.layerNumber);
		break;
	default:
		AbortProg( "updateCornu: Bad inx %d", inx );
	}
	track_p tracks[2];
	tracks[0] = GetTrkEndTrk(trk,0);
	tracks[1] = GetTrkEndTrk(trk,1);

	if (updateEndPts) {
		if ( GetTrkEndTrk(trk,0) == NULL ) {
			SetTrkEndPoint( trk, 0, cornData.pos[0], xx->cornuData.a[0]);
			cornuDesc[A0].mode |= DESC_CHANGE;
		}
		if ( GetTrkEndTrk(trk,1) == NULL ) {
			SetTrkEndPoint( trk, 1, cornData.pos[1], xx->cornuData.a[1]);
			cornuDesc[A1].mode |= DESC_CHANGE;
		}
	}

	track_p ts[2];
	ts[0] = GetTrkEndTrk(trk,0);
	ts[1] = GetTrkEndTrk(trk,1);
	SetUpCornuParmFromTracks(ts,&cp,xx);
	CallCornu(xx->cornuData.pos, tracks, NULL, &xx->cornuData.arcSegs, &cp);

	//FixUpCornu(xx->bezierData.pos, xx, IsTrack(trk));
	ComputeCornuBoundingBox(trk, xx);
	DrawNewTrack( trk );
}


static void DescribeCornu( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T d;

	d = xx->cornuData.length;
    sprintf( str, _("Cornu Track(%d): Layer=%u MinRadius=%s Length=%s EP=[%0.3f,%0.3f] [%0.3f,%0.3f]"),
    			GetTrkIndex(trk),
				GetTrkLayer(trk)+1,
				FormatDistance(xx->cornuData.minCurveRadius),
				FormatDistance(d),
				PutDim(xx->cornuData.pos[0].x),PutDim(xx->cornuData.pos[0].y),
                PutDim(xx->cornuData.pos[1].x),PutDim(xx->cornuData.pos[1].y)
                );

	cornData.length = xx->cornuData.length;
	cornData.minRadius = xx->cornuData.minCurveRadius;
	cornData.maxRateOfChange = xx->cornuData.maxRateofChange;
	cornData.windingAngle = xx->cornuData.windingAngle;
    cornData.layerNumber = GetTrkLayer(trk);
    cornData.pos[0] = xx->cornuData.pos[0];
    cornData.pos[1] = xx->cornuData.pos[1];
    cornData.angle[0] = xx->cornuData.a[0];
    cornData.angle[1] = xx->cornuData.a[1];
    cornData.center[0] = xx->cornuData.c[0];
    cornData.center[1] = xx->cornuData.c[1];
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
    BOOL_T trk0 = (GetTrkEndTrk(trk,0)!=NULL);
    BOOL_T trk1 = (GetTrkEndTrk(trk,1)!=NULL);

	cornuDesc[P0].mode = !trk0?0:DESC_RO;
	cornuDesc[P1].mode = !trk1?0:DESC_RO;
	cornuDesc[LN].mode = DESC_RO;
    cornuDesc[Z0].mode = EndPtIsDefinedElev(trk,0)?0:DESC_RO;
    cornuDesc[Z1].mode = EndPtIsDefinedElev(trk,1)?0:DESC_RO;


	cornuDesc[A0].mode = !trk0?0:DESC_RO;
	cornuDesc[A1].mode = !trk1?0:DESC_RO;
	cornuDesc[C0].mode = DESC_RO;
	cornuDesc[C1].mode = DESC_RO;
	cornuDesc[R0].mode = !trk0?0:DESC_RO;
	cornuDesc[R1].mode = !trk1?0:DESC_RO;
	cornuDesc[GR].mode = DESC_RO;
    cornuDesc[RA].mode = DESC_RO;
    cornuDesc[RR].mode = DESC_RO;
    cornuDesc[WA].mode = DESC_RO;
	cornuDesc[LY].mode = DESC_NOREDRAW;

	DoDescribe( _("Cornu Track"), trk, cornuDesc, UpdateCornu );


}


static DIST_T DistanceCornu( track_p t, coOrd * p )
{
	struct extraData *xx = GetTrkExtraData(t);
	//return BezierMathDistance(p,xx->bezierData.pos,100, &s);

	DIST_T d = 100000.0;
	coOrd p2 = xx->cornuData.pos[0];    //Set initial point
	segProcData_t segProcData;
	for (int i = 0;i<xx->cornuData.arcSegs.cnt;i++) {
		trkSeg_t seg = DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i);
		if (seg.type == SEG_FILCRCL) continue;
		segProcData.distance.pos1 = * p;
		SegProc(SEGPROC_DISTANCE,&seg,&segProcData);
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
	long widthOptions = DTS_LEFT|DTS_RIGHT;

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
	if (s->type == SEG_BEZTRK || s->type == SEG_BEZLIN) {
		if (s->bezSegs.ptr) {
			MyFree(s->bezSegs.ptr);
		}
		s->bezSegs.max = 0;
		s->bezSegs.cnt = 0;
		s->bezSegs.ptr = NULL;
	}
}

static void DeleteCornu( track_p t )
{
	struct extraData *xx = GetTrkExtraData(t);

	for (int i=0;i<xx->cornuData.arcSegs.cnt;i++) {
		trkSeg_t s = DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i);
		FreeSubSegs(&s);
	}
	if (xx->cornuData.arcSegs.ptr)
		MyFree(xx->cornuData.arcSegs.ptr);
	xx->cornuData.arcSegs.max = 0;
	xx->cornuData.arcSegs.cnt = 0;
	xx->cornuData.arcSegs.ptr = NULL;
}

static BOOL_T WriteCornu( track_p t, FILE * f )
{
	struct extraData *xx = GetTrkExtraData(t);
	long options;
	BOOL_T rc = TRUE;
	BOOL_T track =(GetTrkType(t)==T_CORNU);
	options = GetTrkWidth(t) & 0x0F;
	if ( ( GetTrkBits(t) & TB_HIDEDESC ) == 0 ) options |= 0x80;
	rc &= fprintf(f, "%s %d %d %ld 0 0 %s %d %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f \n",
		"CORNU",GetTrkIndex(t), GetTrkLayer(t), (long)options,
                  GetTrkScaleName(t), GetTrkVisible(t),
				  xx->cornuData.pos[0].x, xx->cornuData.pos[0].y,
				  xx->cornuData.a[0],
				  xx->cornuData.r[0],
				  xx->cornuData.c[0].x,xx->cornuData.c[0].y,
				  xx->cornuData.pos[1].x, xx->cornuData.pos[1].y,
				  xx->cornuData.a[1],
				  xx->cornuData.r[1],
				  xx->cornuData.c[1].x,xx->cornuData.c[1].y )>0;
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
	coOrd p0, p1, c0, c1;
	char scale[10];
	wIndex_t layer;
	long options;
	char * cp = NULL;

	if (!GetArgs( line+6, "dLl00sdpffppffp",
		&index, &layer, &options, scale, &visible, &p0, &a0, &r0, &c0, &p1, &a1, &r1, &c1 ) ) {
		return;
	}
	t = NewTrack( index, T_CORNU, 0, sizeof *xx );

	xx = GetTrkExtraData(t);
	SetTrkVisible(t, visible);
	SetTrkScale(t, LookupScale(scale));
	SetTrkLayer(t, layer );
	SetTrkWidth(t, (int)(options&0x0F));
	if ( ( options & 0x80 ) == 0 )  SetTrkBits(t,TB_HIDEDESC);
	xx->cornuData.pos[0] = p0;
    xx->cornuData.pos[1] = p1;
    xx->cornuData.a[0] = a0;
    xx->cornuData.r[0] = r0;
    xx->cornuData.a[1] = a1;
    xx->cornuData.c[0] = c0;
    xx->cornuData.c[1] = c1;
    xx->cornuData.r[1] = r1;
    xx->cornuData.descriptionOff.x = xx->cornuData.descriptionOff.y = 0.0;
    ReadSegs();
    FixUpCornu0(xx->cornuData.pos,xx->cornuData.c,xx->cornuData.a, xx->cornuData.r, xx);
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
        xx->cornuData.c[i].x += orig.x;
        xx->cornuData.c[i].y += orig.y;
    }
    RebuildCornu(trk);
}

static void RotateCornu( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
    for (int i=0;i<2;i++) {
        Rotate( &xx->cornuData.pos[i], orig, angle );
        Rotate( &xx->cornuData.c[i], orig, angle);
        xx->cornuData.a[i] = NormalizeAngle(xx->cornuData.a[i]+angle);
    }
    RebuildCornu(trk);
}

static void RescaleCornu( track_p trk, FLOAT_T ratio )
{
	struct extraData *xx = GetTrkExtraData(trk);
	UndoModify(trk);
	for (int i=0;i<2;i++) {
		xx->cornuData.pos[i].x *= ratio;
		xx->cornuData.pos[i].y *= ratio;
	}
    RebuildCornu(trk);

}

EXPORT BOOL_T SetCornuEndPt(track_p trk, EPINX_T inx, coOrd pos, coOrd center, ANGLE_T angle, DIST_T radius) {
    struct extraData *xx = GetTrkExtraData(trk);

    xx->cornuData.pos[inx] = pos;
    xx->cornuData.c[inx] = center;
    xx->cornuData.a[inx] = angle;
    xx->cornuData.r[inx] = radius;
    if (!RebuildCornu(trk)) return FALSE;
    SetTrkEndPoint( trk, inx, xx->cornuData.pos[inx], xx->cornuData.a[inx]);
    return TRUE;
}


/**
 * Split the Track at approximately the point pos.
 */
static BOOL_T SplitCornu( track_p trk, coOrd pos, EPINX_T ep, track_p *leftover, EPINX_T * ep0, EPINX_T * ep1 )
{
	struct extraData *xx = GetTrkExtraData(trk);
	track_p trk1;
    DIST_T radius = 0.0;
    coOrd center;
    int inx;
    BOOL_T track;
    track = IsTrack(trk);
    
    cornuParm_t new;

    double dd = DistanceCornu(trk, &pos);
    if (dd>minLength) return FALSE;
    BOOL_T back, neg;
    
    ANGLE_T angle = GetAngleSegs(xx->cornuData.arcSegs.cnt,(trkSeg_t *)(xx->cornuData.arcSegs.ptr),&pos,&inx,NULL,&back,NULL,&neg);

    trkSeg_p segPtr = &DYNARR_N(trkSeg_t, xx->cornuData.arcSegs, inx);

    GetAngleSegs(segPtr->bezSegs.cnt,(trkSeg_t *)(segPtr->bezSegs.ptr),&pos,&inx,NULL,&back,NULL,&neg);
    segPtr = &DYNARR_N(trkSeg_t, segPtr->bezSegs, inx);

    if (segPtr->type == SEG_STRTRK) {
    	radius = 0.0;
    	center = zero;
    } else if (segPtr->type == SEG_CRVTRK) {
    	center = segPtr->u.c.center;
    	radius = fabs(segPtr->u.c.radius);
    }
    if (ep) {
    	new.pos[0] = pos;
    	new.pos[1] = xx->cornuData.pos[1];
    	new.angle[0] = NormalizeAngle(angle+(neg==back?180:0));
    	new.angle[1] = xx->cornuData.a[1];
    	new.center[0] = center;
    	new.center[1] = xx->cornuData.c[1];
    	new.radius[0] = radius;
    	new.radius[1] = xx->cornuData.r[1];
    } else {
    	new.pos[1] = pos;
    	new.pos[0] = xx->cornuData.pos[0];
    	new.angle[1] = NormalizeAngle(angle+(neg==back?0:180));
    	new.angle[0] = xx->cornuData.a[0];
    	new.center[1] = center;
    	new.center[0] = xx->cornuData.c[0];
    	new.radius[1] = radius;
    	new.radius[0] = xx->cornuData.r[0];
    }

    trk1 = NewCornuTrack(new.pos,new.center,new.angle,new.radius,NULL,0);
    if (trk1==NULL) {
    	wBeep();
    	InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
    			new.pos[0].x,new.pos[0].y,
    			new.pos[1].x,new.pos[1].y,
    			new.center[0].x,new.center[0].y,
    			new.center[1].x,new.center[1].y,
    			new.angle[0],new.angle[1],
    			FormatDistance(new.radius[0]),FormatDistance(new.radius[1]));
    	UndoEnd();
    	return FALSE;
    }

    UndoModify(trk);
    xx->cornuData.pos[ep] = pos;
    xx->cornuData.a[ep] = NormalizeAngle(new.angle[1-ep]+180);
    xx->cornuData.r[ep] = new.radius[1-ep];
    xx->cornuData.c[ep] = new.center[1-ep];

    RebuildCornu(trk);

    SetTrkEndPoint(trk, ep, xx->cornuData.pos[ep], xx->cornuData.a[ep]);

	*leftover = trk1;
	*ep0 = 1-ep;
	*ep1 = ep;

	return TRUE;
}

BOOL_T MoveCornuEndPt ( track_p *trk, EPINX_T *ep, coOrd pos, DIST_T d0 ) {
	track_p trk2;
	if (SplitTrack(*trk,pos,*ep,&trk2,TRUE)) {
		struct extraData *xx = GetTrkExtraData(*trk);
		if (trk2) DeleteTrack(trk2,TRUE);
		SetTrkEndPoint( *trk, *ep, *ep?xx->cornuData.pos[1]:xx->cornuData.pos[0], *ep?xx->cornuData.a[1]:xx->cornuData.a[0] );
		return TRUE;
	}
	return FALSE;
}
static int log_traverseCornu = 0;
/*
 * TraverseCornu is used to position a train/car.
 * We find a new position and angle given a current pos, angle and a distance to travel.
 *
 * The output can be TRUE -> we have moved the point to a new point or to the start/end of the next track
 * 					FALSE -> we have not found that point because pos was not on/near the track
 *
 * 	If true we supply the remaining distance to go (always positive).
 *  We detect the movement direction by comparing the current angle to the angle of the track at the point.
 *
 *  The entire Cornu may be reversed or forwards depending on the way it was drawn.
 *
 *  Each Bezier segment within that Cornu structure therefore may be processed forwards or in reverse.
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
	BOOL_T cornu_backwards= FALSE;
	BOOL_T neg = FALSE;
	DIST_T d = 10000;
	coOrd pos1 = trvTrk->pos, pos2 = trvTrk->pos;
	ANGLE_T a1,a2;
	int inx, segInx = 0;
	EPINX_T ep;
	BOOL_T back;
LOG( log_traverseCornu, 1, ( "TravCornu-In [%0.3f %0.3f] A%0.3f D%0.3f \n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR ))
	trkSeg_p segPtr = (trkSeg_p)xx->cornuData.arcSegs.ptr;

	a2 = GetAngleSegs(		  						//Find correct Segment and nearest point in it
				xx->cornuData.arcSegs.cnt,segPtr,
				&pos2, &segInx, &d , &back , NULL, &neg);   	//d = how far pos2 from old pos2 = trvTrk->pos

	if ( d > 10 ) {
			ErrorMessage( "traverseCornu: Position is not near track: %0.3f", d );
			return FALSE;   						//This means the input pos is not on or close to the track.
	}
	if (back) a2 = NormalizeAngle(a2+180);		    //If reverse segs - reverse angle
	a1 = NormalizeAngle(a2-trvTrk->angle);			//Establish if we are going fwds or backwards globally
	if (a1<270 && a1>90) {							//Must add 180 if the seg is reversed or inverted (but not both)
		cornu_backwards = TRUE;
		ep = 0;
	} else {
		cornu_backwards = FALSE;
		ep = 1;
	}
	if (neg) { 
		cornu_backwards = !cornu_backwards;			//Reversed direction
		ep = 1-ep;
	}
	segProcData.traverse1.pos = pos2;					//actual point on curve
	segProcData.traverse1.angle = trvTrk->angle;       //direction car is going for Traverse 1
LOG( log_traverseCornu, 1, ( "  TravCornu-GetSubA A%0.3f I%d N%d B%d CB%d\n", a2, segInx, neg, back, cornu_backwards ))
	inx = segInx;
	while (inx >=0 && inx<xx->cornuData.arcSegs.cnt) {
		segPtr = (trkSeg_p)xx->cornuData.arcSegs.ptr+inx;  	    //move in to the identified Bezier segment
		SegProc( SEGPROC_TRAVERSE1, segPtr, &segProcData );
		BOOL_T backwards = segProcData.traverse1.backwards;			//do we process this segment backwards?
		BOOL_T reverse_seg = segProcData.traverse1.reverse_seg;		//Info only
		int BezSegInx = segProcData.traverse1.BezSegInx;			//Which subSeg was it?
		BOOL_T segs_backwards = segProcData.traverse1.segs_backwards;

		dist += segProcData.traverse1.dist;						//Add in the part of the Bezier to get to pos

		segProcData.traverse2.dist = dist;						//Set up Traverse2
		segProcData.traverse2.segDir = backwards;
		segProcData.traverse2.BezSegInx = BezSegInx;
		segProcData.traverse2.segs_backwards = segs_backwards;
LOG( log_traverseCornu, 2, ( "  TravCornu-Tr1 SI%d D%0.3f B%d RS%d \n", BezSegInx, dist, backwards, reverse_seg ) )
		SegProc( SEGPROC_TRAVERSE2, segPtr, &segProcData );		//Angle at pos2
		if ( segProcData.traverse2.dist <= 0 ) {				//-ve or zero distance left over so stop there
			*distR = 0;
			trvTrk->pos = segProcData.traverse2.pos;			//Use finishing pos
			trvTrk->angle = segProcData.traverse2.angle;		//Use finishing angle
LOG( log_traverseCornu, 1, ( "TravCornu-Ex1 -> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR ) )
			return TRUE;
		}
		dist = segProcData.traverse2.dist;						//How far left?
		coOrd pos = segProcData.traverse2.pos;					//Will always be at a Bezseg end
		ANGLE_T angle = segProcData.traverse2.angle;			//Angle of end therefore

		segProcData.traverse1.angle = angle; 					//Set up Traverse1
		segProcData.traverse1.pos = pos;
		inx = cornu_backwards?inx-1:inx+1;						//Here's where the global segment direction comes in
LOG( log_traverseCornu, 2, ( "  TravCornu-Loop D%0.3f A%0.3f I%d \n", dist, angle, inx ) )
	}	
																//Ran out of Bez-Segs so punt to next Track
	*distR = dist;												//Tell caller what dist is left

	trvTrk->pos = GetTrkEndPos(trk,ep);							//Which end were we heading for?
	trvTrk->angle = NormalizeAngle(GetTrkEndAngle(trk, ep)+(cornu_backwards?180:0));
	trvTrk->trk = GetTrkEndTrk(trk,ep);							//go onto next track (or NULL)

	if (trvTrk->trk==NULL) {
		trvTrk->pos = pos1;
	    return TRUE;
	}
LOG( log_traverseCornu, 1, ( "TravCornu-Ex2 --> [%0.3f %0.3f] A%0.3f D%0.3f\n", trvTrk->pos.x, trvTrk->pos.y, trvTrk->angle, *distR ) )
	return TRUE;

}


static BOOL_T EnumerateCornu( track_p trk )
{

	if (trk != NULL) {
		struct extraData *xx = GetTrkExtraData(trk);
		DIST_T d;
		d = xx->cornuData.length;
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
	track_p trk_after,trk_before;
	EPINX_T ep_before,ep_after=-1;
	coOrd p[2];
	coOrd c[2];
	ANGLE_T a[2];
	DIST_T r[2];


	if (!IsTrack(trk0) || !IsTrack(trk1) ) return FALSE;
	if (GetTrkType(trk0) != GetTrkType(trk1)) return FALSE;
	if (GetEndPtConnectedToMe(trk0,trk1) != ep0) return FALSE;
	if (GetEndPtConnectedToMe(trk1,trk0) != ep1) return FALSE;

	if (ep0 == ep1)
		return FALSE;
    
	UndoStart( _("Merge Cornu"), "MergeCornu( T%d[%d] T%d[%d] )", GetTrkIndex(trk0), ep0, GetTrkIndex(trk1), ep1 );
	p[0] = xx0->cornuData.pos[0];
	p[1] = xx1->cornuData.pos[1];
	a[0] = xx0->cornuData.a[0];
	a[1] = xx1->cornuData.a[1];
	c[0] = xx0->cornuData.c[0];
	c[1] = xx1->cornuData.c[1];
	r[0] = xx0->cornuData.r[0];
	r[1] = xx1->cornuData.r[1];
	track_p trk3 = NewCornuTrack(p,c,a,r,NULL,0);
	if (trk3==NULL) {
		wBeep();
		InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
									p[0].x,p[0].y,
									p[1].x,p[1].y,
									c[0].x,c[0].y,
									c[1].x,c[1].y,
									a[0],a[1],
									FormatDistance(r[0]),FormatDistance(r[1]));
		UndoEnd();
		return FALSE;
	}

	UndoModify( trk0 );
	UndoModify( trk1 );
	UndrawNewTrack( trk0 );
	UndrawNewTrack( trk1 );
	trk_after = GetTrkEndTrk( trk1, 1-ep1 );
	if (trk_after) {
		ep_after = GetEndPtConnectedToMe( trk_after, trk1 );
		DisconnectTracks( trk1, 1-ep1, trk_after, ep_after );
	}
	trk_before = GetTrkEndTrk( trk0, 1-ep0 );
	if (trk_before) {
		ep_before = GetEndPtConnectedToMe( trk_before, trk0 );
		DisconnectTracks( trk0, 1-ep1, trk_before, ep_before );
	}

	DeleteTrack( trk1, TRUE );
	DeleteTrack( trk0, TRUE );
	if (trk_after) {
		SetTrkEndPoint( trk_after, ep_after, xx0->cornuData.pos[1], NormalizeAngle(xx0->cornuData.a[1]+180));
		ConnectTracks( trk3, 1, trk_after, ep_after);
	}
	if (trk_before) {
		SetTrkEndPoint( trk_before, ep_before, xx0->cornuData.pos[0], NormalizeAngle(xx0->cornuData.a[0]+180));
		ConnectTracks( trk3, 0, trk_before, ep_before);
	}
	DrawNewTrack( trk3 );
	UndoEnd();


	return TRUE;
}

BOOL_T GetBezierSegmentsFromCornu(track_p trk, dynArr_t * segs) {
	struct extraData * xx = GetTrkExtraData(trk);
	for (int i=0;i<xx->cornuData.arcSegs.cnt;i++) {
			DYNARR_APPEND(trkSeg_t, * segs, 10);
			trkSeg_p segPtr = &DYNARR_N(trkSeg_t,* segs,segs->cnt-1);
			segPtr->type = SEG_BEZTRK;
			segPtr->color = wDrawColorBlack;
			segPtr->width = 0;
			if (segPtr->bezSegs.ptr) MyFree(segPtr->bezSegs.ptr);
			segPtr->bezSegs.cnt = 0;
			segPtr->bezSegs.max = 0;
			segPtr->bezSegs.ptr = NULL;
			trkSeg_p p = (trkSeg_t *) xx->cornuData.arcSegs.ptr+i;
			for (int j=0;j<4;j++) segPtr->u.b.pos[j] = p->u.b.pos[j];
			FixUpBezierSeg(segPtr->u.b.pos,segPtr,TRUE);
	}
	return TRUE;
}

static DIST_T GetLengthCornu( track_p trk )
{
	struct extraData *xx = GetTrkExtraData(trk);
	DIST_T length = 0.0;
	segProcData_t segProcData;
	for(int i=0;i<xx->cornuData.arcSegs.cnt;i++) {
		trkSeg_t seg = DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i);
		if (seg.type == SEG_FILCRCL) continue;
		SegProc(SEGPROC_LENGTH, &seg, &segProcData);
		length += segProcData.length.length;
	}
	return length;
}


static BOOL_T GetParamsCornu( int inx, track_p trk, coOrd pos, trackParams_t * params )
{
	int segInx, segInx2;
	BOOL_T back, negative;
	DIST_T d;
	struct extraData *xx = GetTrkExtraData(trk);
	params->type = curveTypeCornu;
	params->track_angle = GetAngleSegs(		  						//Find correct Segment and nearest point in it
							xx->cornuData.arcSegs.cnt,xx->cornuData.arcSegs.ptr,
							&pos, &segInx, &d , &back, &segInx2, &negative );
	trkSeg_p segPtr = &DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,segInx);
	if (negative != back) params->track_angle = NormalizeAngle(params->track_angle+180);  //Cornu is in reverse
	if (segPtr->type == SEG_STRTRK) {
		params->arcR = 0.0;
	} else  if (segPtr->type == SEG_CRVTRK) {
		params->arcR = fabs(segPtr->u.c.radius);
		params->arcP = segPtr->u.c.center;
		params->arcA0 = segPtr->u.c.a0;
		params->arcA1 = segPtr->u.c.a1;
	} else if (segPtr->type == SEG_BEZTRK) {
		trkSeg_p segPtr2 = &DYNARR_N(trkSeg_t,segPtr->bezSegs,segInx2);
		if (segPtr2->type == SEG_STRTRK) {
			params->arcR = 0.0;
		} else if (segPtr2->type == SEG_CRVTRK) {
			params->arcR = fabs(segPtr2->u.c.radius);
			params->arcP = segPtr2->u.c.center;
			params->arcA0 = segPtr2->u.c.a0;
			params->arcA1 = segPtr2->u.c.a1;
		}
	}
	for (int i=0;i<2;i++) {
		params->cornuEnd[i] = xx->cornuData.pos[i];
		params->cornuAngle[i] = xx->cornuData.a[i];
		params->cornuRadius[i] = xx->cornuData.r[i];
		params->cornuCenter[i] = xx->cornuData.c[i];
	}
	params->len = xx->cornuData.length;
	if ( inx == PARAMS_PARALLEL ) {
			params->ep = 0;
	} else if (inx == PARAMS_CORNU) {
		params->ep = PickEndPoint( pos, trk);
	} else {
		params->ep = PickUnconnectedEndPointSilent( pos, trk );
	}
	if (params->ep>=0) {
		params->angle = GetTrkEndAngle(trk,params->ep);
	}

	return TRUE;

}



static BOOL_T QueryCornu( track_p trk, int query )
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
		return xx->cornuData.minCurveRadius < (GetLayoutMinTrackRadius()-EPSILON);
		break;
	case Q_IS_CORNU:
		return TRUE;
		break;
	case Q_ISTRACK:
		return TRUE;
		break;
	case Q_CAN_PARALLEL:
		return TRUE;
		break;
	// case Q_MODIFY_CANT_SPLIT: Remove Split Restriction
	// case Q_CANNOT_BE_ON_END: Remove Restriction - Can have Cornu with no ends
	case Q_CANNOT_PLACE_TURNOUT:
		return TRUE;
		break;
	case Q_IGNORE_EASEMENT_ON_EXTEND:
		return TRUE;
		break;
	case Q_MODIFY_CAN_SPLIT:
	case Q_CAN_EXTEND:
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
	FlipPoint( &xx->cornuData.c[0], orig, angle);
	FlipPoint( &xx->cornuData.c[1], orig, angle);
	xx->cornuData.a[0] = NormalizeAngle( 2*angle - xx->cornuData.a[0] );
	xx->cornuData.a[1] = NormalizeAngle( 2*angle - xx->cornuData.a[1] );

    RebuildCornu(trk);

}

static ANGLE_T GetAngleCornu(
		track_p trk,
		coOrd pos,
		EPINX_T * ep0,
		EPINX_T * ep1 )
{
	struct extraData * xx = GetTrkExtraData(trk);
	ANGLE_T angle;
	BOOL_T back, neg;
	int indx;
	angle = GetAngleSegs( xx->cornuData.arcSegs.cnt, (trkSeg_p)xx->cornuData.arcSegs.ptr, &pos, &indx, NULL, &back, NULL, &neg );
	if ( ep0 ) *ep0 = -1;
	if ( ep1 ) *ep1 = -1;
	return angle;
}

BOOL_T GetCornuSegmentFromTrack(track_p trk, trkSeg_p seg_p) {
	//TODO If we support Group
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
    coOrd np[4], p, nc[2];
    ANGLE_T atrk, diff_a, na[2];
    DIST_T nr[2];


	//Produce cornu that is translated parallel to the existing Cornu
    // - not a precise result if the cornu end angles are not in the same general direction.
    // The expectation is that the user will have to adjust it - unless and until we produce
    // a new algo to adjust the control points to be parallel to the endpoints.
    
    p = pos;
    DistanceCornu(trk, &p);  //Find nearest point on curve
    atrk = GetAngleSegs(xx->cornuData.arcSegs.cnt,(trkSeg_t *)(xx->cornuData.arcSegs.ptr),&p,NULL,NULL,NULL,NULL, NULL);
    diff_a = NormalizeAngle(FindAngle(pos,p)-atrk);  //we know it will be +/-90...
    //find parallel move x and y for points
    BOOL_T above = FALSE;
    if ( diff_a < 180 ) above = TRUE; //Above track
    if (xx->cornuData.a[0] <180) above = !above;
    Translate(&np[0],xx->cornuData.pos[0],xx->cornuData.a[0]+(above?90:-90),sep);
    Translate(&np[1],xx->cornuData.pos[1],xx->cornuData.a[1]+(above?-90:90),sep);
    na[0]=xx->cornuData.a[0];
    na[1]=xx->cornuData.a[1];
    if (xx->cornuData.r[0]) {
       //Find angle between center and end angle of track
       ANGLE_T ea0 =
        	   NormalizeAngle(FindAngle(xx->cornuData.c[0],xx->cornuData.pos[0])-xx->cornuData.a[0]);
       DIST_T sep0 = sep;
        	if (ea0>180) sep0 = -sep;
        	nr[0]=xx->cornuData.r[0]+(above?sep0:-sep0);           //Needs adjustment
        	nc[0]=xx->cornuData.c[0];
     } else {
        	nr[0] = 0;
        	nc[0] = zero;
     }

     if (xx->cornuData.r[1]) {
        	ANGLE_T ea1 =
        			NormalizeAngle(FindAngle(xx->cornuData.c[1],xx->cornuData.pos[1])-xx->cornuData.a[1]);
        	DIST_T sep1 = sep;
        	if (ea1<180) sep1 = -sep;
        	nr[1]=xx->cornuData.r[1]+(above?sep1:-sep1);            //Needs adjustment
        	nc[1]=xx->cornuData.c[1];
     } else {
        	nr[1] = 0;
        	nc[1] = zero;
     }

	if ( newTrkR ) {
		*newTrkR = NewCornuTrack( np, nc, na, nr, NULL, 0);
		if (*newTrkR==NULL) {
			wBeep();
			InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
						np[0].x,np[0].y,
						np[1].x,np[1].y,
						nc[0].x,nc[0].y,
						nc[1].x,nc[1].y,
						na[0],na[1],
						FormatDistance(nr[0]),FormatDistance(nr[1]));
				return FALSE;
			}

	} else {
		tempSegs_da.cnt = 0;
		CallCornu0(np,nc,na,nr,&tempSegs_da,FALSE);
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
EXPORT BOOL_T RebuildCornu (track_p trk)
{
	struct extraData *xx;
	xx = GetTrkExtraData(trk);
	xx->cornuData.arcSegs.max = 0;
	xx->cornuData.arcSegs.cnt = 0;
	//if (xx->cornuData.arcSegs.ptr) MyFree(xx->cornuData.arcSegs.ptr);
	xx->cornuData.arcSegs.ptr = NULL;
	if (!FixUpCornu0(xx->cornuData.pos,xx->cornuData.c,xx->cornuData.a,xx->cornuData.r, xx)) return FALSE;
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
		MoveCornuEndPt, /* Move EndPt */
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





/****************************************
 *
 * GRAPHICS COMMANDS
 *
 */




track_p NewCornuTrack(coOrd pos[2], coOrd center[2],ANGLE_T angle[2], DIST_T radius[2], trkSeg_t * tempsegs, int count)
{
	struct extraData *xx;
	track_p p;
	p = NewTrack( 0, T_CORNU, 2, sizeof *xx );
	xx = GetTrkExtraData(p);
    xx->cornuData.pos[0] = pos[0];
    xx->cornuData.pos[1] = pos[1];
    xx->cornuData.a[0] = angle[0];
    xx->cornuData.a[1] = angle[1];
    xx->cornuData.r[0] = radius[0];
    xx->cornuData.r[1] = radius[1];
    xx->cornuData.c[0] = center[0];
    xx->cornuData.c[1] = center[1];

    if (!FixUpCornu0(xx->cornuData.pos,xx->cornuData.c,xx->cornuData.a,xx->cornuData.r, xx)) {
    	ErrorMessage("Create Cornu Failed");
    	return NULL;
	}
LOG( log_cornu, 1, ( "NewCornuTrack( EP1 %0.3f, %0.3f, EP2 %0.3f, %0.3f )  = %d\n", pos[0].x, pos[0].y, pos[1].x, pos[1].y, GetTrkIndex(p) ) )
	ComputeCornuBoundingBox( p, xx );
	SetTrkEndPoint( p, 0, pos[0], xx->cornuData.a[0]);
	SetTrkEndPoint( p, 1, pos[1], xx->cornuData.a[1]);
	CheckTrackLength( p );
	SetTrkBits( p, TB_HIDEDESC );
	return p;
}


EXPORT void InitTrkCornu( void )
{
	T_CORNU = InitObject( &cornuCmds );
	log_cornu = LogFindIndex( "Cornu" );
	log_traverseCornu = LogFindIndex( "traverseCornu" );
}

