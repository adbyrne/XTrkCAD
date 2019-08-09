



/** \file ccornu.c
 * Cornu Command. Draw or modify a Cornu Easement Track.
 */
/*  XTrkCad - Model Railroad CAD
 *
 * Cornu curves are a family of mathematically defined curves that define spirals that Euler spirals and elastica come from.
 *
 * They have the useful property for us that curvature increases linearly along the curve which
 * means the acceleration towards the center of the curve also increases evenly. Railways have long understood that
 * smoothly changing the radius is key to passenger comfort and reduced derailments. The railway versions of these
 * curves were called variously called easements, Talbot or Euler spirals.
 *
 * In XTrackCAD often want to change radius smoothly between two tracks whose end position, angle and curvature are known.
 *
 * Finding the right part(s) of the Cornu to fit the gap (if one is available) is mathematically complex,
 * but fortunately Raph Levien published a PhD thesis on this together with his mathematical libraries as
 * open source. He was doing work on font design where the Cornu shapes make beautiful smooth fonts. He
 * was faced with the reality, though, that graphics packages do not include these shapes as native objects
 * and so his solution was to produce a set of Bezier curves that approximate the solution.
 *
 * We already have a tool that can produce a set of arcs and straight line to approximate a Bezier - so that in the end
 * for an easement track we will have an array of Bezier, each of which is an array of Arcs and Lines. The tool will
 * use the approximations for most of its work.
 *
 * The inputs for the Cornu are the end points, angles and radii. To match the Cornu algorithm's expectations we input
 * these as a set of knots (points on lines). One point is always the desired end point and the other two are picked
 * direct the code to derive the other two end conditions. By specifying that the desired end point is a "one-way"
 * knot we ensure that the result has smooth ends of either zero or fixed radius.
 *
 * When reading back the output, we simply ignore the results before the first end point knot and after the last.
 *
 * Because we are mathematically deriving the output, we can alter the end conditions and recalculate. This allows
 * support of modify for Cornu Easements and also movement of tracks that are connected via a Cornu easement to another track.
 *
 *  Note that unlike the existing Easements in XTrkCAD, the degree of sharpness (the rate of change of curvature)
 *  is derived not defined. By adjusting the ends, one can have an infinite set of sharpness solutions.
 *
 * Cornu will not find a solution for every set of input conditions, or may propose one that is impractical such as
 * huge loops or tiny curves. These are mathematically correct, but not useful. In these cases the answer is to change the
 * end conditions (more space between the ends, different angles or different radii).
 *
 * Note that every time we change the Cornu end points we have to recalculate the Bezier approximation,
 * which recalculates the arc approximations, but that still means that the majority of the time we are using the approximation.
 *
 * Cornus do not have cusps, but can result in smooth loops. If there is too much looping, the code will reject the easement.
 *
 * This program is built and founded upon Raph Levien's seminal work and relies on an adaptation of his Cornu library.
 * As with the Bezier work, it also relies on the pages about Bezier curves that PoMax put up at https://pomax.github.io/bezierinfo
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
#include "spiro.h"
#include "spiroentrypoints.h"
#include "bezctx_xtrkcad.h"
#include "draw.h"
#include "ccurve.h"
#include "ccornu.h"
#include "tcornu.h"
#include "tbezier.h"
#include "cstraigh.h"
#include "drawgeom.h"
#include "cjoin.h"
#include "i18n.h"
#include "common.h"
#include "utility.h"
#include "math.h"
#include "param.h"
#include "layout.h"
#include "cundo.h"
#include "messages.h"
#include "cselect.h"
#include "fileio.h"

extern drawCmd_t tempD;
extern TRKTYP_T T_BEZIER;
extern TRKTYP_T T_CORNU;



/*
 * STATE INFO
 */
enum Cornu_States { NONE,
	POS_1,
	LOC_2,
	POS_2,
	PICK_POINT,
	POINT_PICKED,
	TRACK_SELECTED };


static struct {
		enum Cornu_States state;
		coOrd pos[2];
		int number_of_points;
        int selectEndPoint;
        int selectMidPoint;
        int prevSelected;
        wDrawColor color;
        DIST_T width;
		track_p trk[2];
		EPINX_T ep[2];
		DIST_T radius[2];
		ANGLE_T angle[2];
		ANGLE_T arcA0[2];
		ANGLE_T arcA1[2];
		coOrd center[2];
		curveType_e trackType[2];

		BOOL_T extend[2];
		trkSeg_t extendSeg[2];

		trkSeg_t ep1Segs[2];
		int ep1Segs_da_cnt;
		trkSeg_t ep2Segs[2];
		int ep2Segs_da_cnt;
		dynArr_t crvSegs_da;
		int crvSegs_da_cnt;
		trkSeg_t trk1Seg;
		trkSeg_t trk2Seg;
		track_p selectTrack;
		DIST_T minRadius;
		BOOL_T circleorHelix[2];
		DIST_T trackGauge;

		int cmdType;

		dynArr_t midSegs;

		dynArr_t mid_points;
		dynArr_t tracks;
		BOOL_T ends[2];

		bezctx * bezc;
		} Da;

int createMidPoint(dynArr_t * ap,
				 coOrd pos0,     //end on curve
				 BOOL_T point_selected,
				 BOOL_T point_selectable,
				 BOOL_T track_modifyable
				  )
{
	DIST_T d, w;
	d = tempD.scale*0.25;
	w = tempD.scale/tempD.dpi; /*double width*/

	DYNARR_APPEND(trkSeg_t,*ap,1);

	trkSeg_p sp = &DYNARR_LAST(trkSeg_t,*ap);

	sp->u.c.center = pos0;
	sp->u.c.a0 = 0.0;
	sp->u.c.a1 = 360.0;
	sp->u.c.radius = d/2;
	sp->type = point_selected?SEG_FILCRCL:SEG_CRVLIN;
	sp->width = w;
	sp->color = drawColorBlack;

	return 1;

}


/**
 * Draw a EndPoint.
 * A Cornu end Point has a filled circle surrounded by another circle for endpoint
 */
int createEndPoint(
                     trkSeg_t sp[],   //seg pointer for up to 2 trkSegs (ends and line)
                     coOrd pos0,     //end on curve
				     BOOL_T point_selected,
					 BOOL_T point_selectable,
					 BOOL_T track_modifyable
                      )
{
    DIST_T d, w;
    d = tempD.scale*0.25;
    w = tempD.scale/tempD.dpi; /*double width*/
    if (point_selectable) {
		sp[1].u.c.center = pos0;
		sp[1].u.c.a0 = 0.0;
		sp[1].u.c.a1 = 360.0;
		sp[1].u.c.radius = d/2;
		sp[1].type = SEG_CRVLIN;
		sp[1].width = w;
		sp[1].color = point_selected?drawColorRed:drawColorBlack;
    }
    sp[0].u.c.center = pos0;
    sp[0].u.c.a0 = 0.0;
    sp[0].u.c.a1 = 360.0;
    sp[0].width = w;
    sp[0].u.c.radius = d/4;
    sp[0].color = (point_selected>=0)?drawColorRed:drawColorBlack;
    if (track_modifyable)
        sp[0].type = SEG_CRVLIN;
    else
        sp[0].type = SEG_FILCRCL;
    if (point_selectable)  {
        sp[1].u.c.center = pos0;
        sp[1].u.c.a0 = 0.0;
        sp[1].u.c.a1 = 360.0;
        sp[1].u.c.radius = d/2;
        sp[1].type = SEG_CRVLIN;
        sp[1].width = w;
        sp[1].color = drawColorRed;
        return 2;
    }
    return 1;
}


/*
 * Add element to DYNARR pointed to by caller from segment handed in
 */
void addSegCornu(dynArr_t * const array_p, trkSeg_p seg) {
	trkSeg_p s;


	DYNARR_APPEND(trkSeg_t, * array_p, 10);          //Adds 1 to cnt
	s = &DYNARR_N(trkSeg_t,* array_p,array_p->cnt-1);
	s->type = seg->type;
	s->bezSegs.max = 0;
	s->bezSegs.cnt = 0;
	if (s->bezSegs.ptr) MyFree(s->bezSegs.ptr);
	s->bezSegs.ptr = NULL;
	s->color = seg->color;
	s->width = seg->width;
	if ((s->type == SEG_BEZLIN || s->type == SEG_BEZTRK) && seg->bezSegs.cnt) {
		s->u.b.angle0 = seg->u.b.angle0;  //Copy all the rest
		s->u.b.angle3 = seg->u.b.angle3;
		s->u.b.length = seg->u.b.length;
		s->u.b.minRadius = seg->u.b.minRadius;
		for (int i=0;i<4;i++) s->u.b.pos[i] = seg->u.b.pos[i];
		s->u.b.radius0 = seg->u.b.radius3;
		for (int i = 0; i<seg->bezSegs.cnt; i++) {
			addSegCornu(&s->bezSegs, (((trkSeg_p)seg->bezSegs.ptr) + i)); //recurse for copying embedded Beziers as in Cornu joint
		}
	} else {
		s->u = seg->u;
	}
}
EXPORT void SetKnots(spiro_cp knots[], coOrd posk[], char type[], int count) {
	for (int i = 0; i < count; i++) {
		knots[i].x = posk[i].x;
		knots[i].y = posk[i].y;
		knots[i].ty = type[i];
	}
}

typedef struct {
	coOrd pos;
	char ty;
} points_t;

// Take in extra points within Cornu
// G2 (position only k1'' = k2'' = 0); Also Cornu <-> Cornu
// G4 (position only - splitable for Cornu - a G4 point) k1''= k2''

BOOL_T CallCornuM(dynArr_t extra_points, BOOL_T end[2], coOrd pos[2], cornuParm_t * cp, dynArr_t * array_p, BOOL_T spots) {
	array_p->cnt = 0;
	//Create LH knots
	//Find remote end point of track, create start knot
	int ends[2];
	ends[0] = (end[0]?2:0); ends[1] = (end[0]?3:1)+extra_points.cnt;
	dynArr_t spiro_cp_array;
	spiro_cp * knots;
	coOrd * posk;
	char * type;
	posk = MyMalloc((6+extra_points.cnt)*sizeof(coOrd));
	knots = MyMalloc((6+extra_points.cnt)*sizeof(spiro_cp));
	type = MyMalloc((6+extra_points.cnt)*sizeof(char));
	BOOL_T back;
	ANGLE_T angle1;

	if (Da.bezc) free(Da.bezc);

	Da.bezc = new_bezctx_xtrkcad(array_p,ends,spots);

	coOrd pos0 = pos[0];

	if (end[0]) {
		type[0] = SPIRO_OPEN_CONTOUR;
		type[1] = SPIRO_G2;
		type[2] = SPIRO_RIGHT;
		if (cp->radius[0] == 0.0) {
			Translate(&posk[0],pos0,cp->angle[0],10);
			Translate(&posk[1],pos0,cp->angle[0],5);
		} else {
			angle1 = FindAngle(cp->center[0],pos[0]);
			if (NormalizeAngle(angle1 - cp->angle[0])<180) back = TRUE;
			else back = FALSE;
			posk[0] = pos[0];
			Rotate(&posk[0],cp->center[0],(back)?-10:10);
			posk[1] = pos[0];
			Rotate(&posk[1],cp->center[0],(back)?-5:5);
		}
		posk[2] = pos[0];
	} else {
		type[0] = SPIRO_OPEN_CONTOUR;
		posk[0] = pos[0];
	}

    for (int i=0;i<extra_points.cnt;i++) {
    	posk[(end[0]?3:1)+i] = DYNARR_N(coOrd,extra_points,i);
    	type[(end[0]?3:1)+i] = SPIRO_G4;
    }

    posk[(end[0]?3:1)+extra_points.cnt] = pos[1];
	coOrd pos1 = pos[1];

	if (end[1]) {
		type[(end[0]?3:1)+extra_points.cnt] = SPIRO_LEFT;
		type[(end[0]?3:1)+extra_points.cnt+1] = SPIRO_G2;
		type[(end[0]?3:1)+extra_points.cnt+2] = SPIRO_END_OPEN_CONTOUR;
		if (cp->radius[1] == 0.0) {
			Translate(&posk[(end[0]?3:1)+extra_points.cnt+1],pos1,cp->angle[1],5);
			Translate(&posk[(end[0]?3:1)+extra_points.cnt+2],pos1,cp->angle[1],10);
		} else {
			angle1 = FindAngle(cp->center[1],pos[1]);
			if (NormalizeAngle(angle1 - cp->angle[1])>180) back = TRUE;
			else back = FALSE;
			posk[(end[0]?3:1)+extra_points.cnt+1] = pos[1];
			Rotate(&posk[(end[0]?3:1)+extra_points.cnt+1],cp->center[1],(back)?5:-5);
			posk[(end[0]?3:1)+extra_points.cnt+2] = pos[1];
			Rotate(&posk[(end[0]?3:1)+extra_points.cnt+2],cp->center[1],(back)?10:-10);
		}
	} else {
		type[(end[0]?3:1)+extra_points.cnt] = SPIRO_END_OPEN_CONTOUR;
	}
	SetKnots(knots, posk, type, ((end[0]?3:1)+(end[1]?3:1)+extra_points.cnt));
	TaggedSpiroCPsToBezier(knots,Da.bezc);
	MyFree(posk);
	MyFree(knots);
	MyFree(type);
	if (!bezctx_xtrkcad_close(Da.bezc)) {
		return FALSE;
	}
	return TRUE;
}

EXPORT BOOL_T CallCornu0(coOrd pos[2], coOrd center[2], ANGLE_T angle[2], DIST_T radius[2], dynArr_t * array_p, BOOL_T spots) {
	array_p->cnt = 0;
	//Create LH knots
	//Find remote end point of track, create start knot
	int ends[2];
	ends[0] = 2; ends[1] = 3;
	spiro_cp knots[6];
	coOrd posk[6];
	char type[6];
	BOOL_T back;
	ANGLE_T angle1;

	if (Da.bezc) free(Da.bezc);

	Da.bezc = new_bezctx_xtrkcad(array_p,ends,spots);

	coOrd pos0 = pos[0];
	type[0] = SPIRO_OPEN_CONTOUR;


	if (radius[0] == 0.0) {
		Translate(&posk[0],pos0,angle[0],10);
		Translate(&posk[1],pos0,angle[0],5);
	} else {
		angle1 = FindAngle(center[0],pos[0]);
		if (NormalizeAngle(angle1 - angle[0])<180) back = TRUE;
		else back = FALSE;
		posk[0] = pos[0];
		Rotate(&posk[0],center[0],(back)?-10:10);
		posk[1] = pos[0];
		Rotate(&posk[1],center[0],(back)?-5:5);
	}
	type[1] = SPIRO_G2;
	posk[2] = pos[0];
	type[2] = SPIRO_RIGHT;

	posk[3] = pos[1];
	type[3] = SPIRO_LEFT;

	coOrd pos1 = pos[1];

	if (radius[1] == 0.0 ) {
		Translate(&posk[4],pos1,angle[1],5);
		Translate(&posk[5],pos1,angle[1],10);
	} else {
		angle1 = FindAngle(center[1],pos[1]);
		if (NormalizeAngle(angle1 - angle[1])>180) back = TRUE;
		else back = FALSE;
		posk[4] = pos[1];
		Rotate(&posk[4],center[1],(back)?5:-5);
		posk[5] = pos[1];
		Rotate(&posk[5],center[1],(back)?10:-10);
	}
	type[4] = SPIRO_G2;
	type[5] = SPIRO_END_OPEN_CONTOUR;

	SetKnots(knots, posk, type, 6);
	TaggedSpiroCPsToBezier(knots,Da.bezc);
	if (!bezctx_xtrkcad_close(Da.bezc)) {
		return FALSE;
	}
	return TRUE;
}

/*
 * Set up the call to Cornu0. Take the conditions of the two ends from the connected tracks.
 */
BOOL_T CallCornu(coOrd pos[2], track_p trk[2], EPINX_T ep[2], dynArr_t * array_p, cornuParm_t * cp) {

	trackParams_t params;
	ANGLE_T angle;
	for (int i=0;i<2;i++) {
		if (trk[i]) {
			if (!GetTrackParams(PARAMS_CORNU,trk[i],pos[i],&params)) return FALSE;
			cp->pos[i] = pos[i];
			if (ep && ep[i]>=0) angle = GetTrkEndAngle(trk[i],ep[i]);
			else angle = params.angle; //Turntable only
			if (Da.circleorHelix[i]) { //Helix/Circle only
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
				if (ep && ep[i]>=0) cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));
			} else if (params.type == curveTypeStraight) {
				cp->angle[i] = NormalizeAngle(angle+180);       //Because end always backwards
				cp->radius[i] = 0.0;
			} else if ((params.type == curveTypeCornu || params.type == curveTypeBezier) && params.arcR == 0.0 ) {
				cp->radius[i] = 0.0;
				if (ep && ep[i]>=0) cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));  //Use point not end
			} else if (params.type == curveTypeCurve) {
				cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
			} else if ((params.type == curveTypeCornu || params.type == curveTypeBezier) && params.arcR != 0.0 ){
				if (ep && ep[i]>=0) cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
			} else {
				cp->angle[i] = NormalizeAngle(angle+180);             //Unknown - treat like straight
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
			}
		}
	}

	return CallCornu0(pos,cp->center,cp->angle,cp->radius,array_p,ep?TRUE:FALSE);
}



/*
 * Draw Cornu while editing it. It consists of up to five elements - the ends, the curve and one or two End Points.
 *
 */

EXPORT void DrawCornuCurve(
							trkSeg_p first_trk,
							trkSeg_p point1,
							int ep1Segs_cnt,
							trkSeg_p curveSegs,
							int crvSegs_cnt,
							trkSeg_p point2,
							int ep2Segs_cnt,
							trkSeg_p second_trk,
							trkSeg_p extend1_trk,
							trkSeg_p extend2_trk,
							trkSeg_p mids,
							int midSegs_cnt,
							wDrawColor color
					) {
	long oldDrawOptions = tempD.funcs->options;
	tempD.funcs->options = wDrawOptTemp;
	long oldOptions = tempD.options;
	tempD.options = DC_TICKS;
	tempD.orig = mainD.orig;
	tempD.angle = mainD.angle;
	if (first_trk)
		DrawSegs( &tempD, zero, 0.0, first_trk, 1, Da.trackGauge, drawColorBlack );
	if (crvSegs_cnt>0 && curveSegs)
		DrawSegs( &tempD, zero, 0.0, curveSegs, crvSegs_cnt, Da.trackGauge, color );
	if (second_trk)
		DrawSegs( &tempD, zero, 0.0, second_trk, 1, Da.trackGauge, drawColorBlack );
	if (ep1Segs_cnt>0 && point1)
		DrawSegs( &tempD, zero, 0.0, point1, ep1Segs_cnt, Da.trackGauge, drawColorBlack );
	if (ep2Segs_cnt>0 && point2)
		DrawSegs( &tempD, zero, 0.0, point2, ep2Segs_cnt, Da.trackGauge, drawColorBlack );
	if (midSegs_cnt>0 && mids)
		DrawSegs( &tempD, zero, 0.0, mids, midSegs_cnt, Da.trackGauge, drawColorBlack );
	if (extend1_trk)
		DrawSegs( &tempD, zero, 0.0, extend1_trk, 1, Da.trackGauge, drawColorBlack);
	if (extend2_trk)
		DrawSegs( &tempD, zero, 0.0, extend2_trk, 1, Da.trackGauge, drawColorBlack);
	tempD.funcs->options = oldDrawOptions;
	tempD.options = oldOptions;

}

/*
 * If Track, make it red if the radius is below minimum
 */
void DrawTempCornu() {


  DrawCornuCurve(&Da.trk1Seg,
		  	  	  &Da.ep1Segs[0],Da.ep1Segs_da_cnt,
				  (trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da_cnt,
				  &Da.ep2Segs[0],Da.ep2Segs_da_cnt,
				  &Da.trk2Seg,
				  Da.extend[0]?&Da.extendSeg[0]:NULL,
				  Da.extend[1]?&Da.extendSeg[1]:NULL,
				  (trkSeg_t *)Da.midSegs.ptr,Da.midSegs.cnt,
				  Da.minRadius<(GetLayoutMinTrackRadius()-EPSILON)?exceptionColor:normalColor);

}

void CreateBothEnds(int selectEndPoint, int selectMidPoint ) {
	BOOL_T selectable[2],modifyable[2];
	selectable[0] = !Da.trk[0] || (
			Da.trk[0] && !QueryTrack(Da.trk[0],Q_IS_CORNU) && !QueryTrack(Da.trk[0],Q_CAN_MODIFY_CONTROL_POINTS));
	modifyable[0] = !Da.trk[0] || (
			Da.trk[0] && QueryTrack(Da.trk[0],Q_CORNU_CAN_MODIFY));
	selectable[1] = !Da.trk[1] || (
			Da.trk[1] && !QueryTrack(Da.trk[1],Q_IS_CORNU) && !QueryTrack(Da.trk[1],Q_CAN_MODIFY_CONTROL_POINTS));
	modifyable[1] = !Da.trk[1] || (
			Da.trk[1] && QueryTrack(Da.trk[1],Q_CORNU_CAN_MODIFY));
	if (selectEndPoint == -1) {
		Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],FALSE,selectable[0],modifyable[0]);
		Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs, Da.pos[1],FALSE,selectable[1],modifyable[1]);
	} else {
		Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],selectEndPoint == 0,selectable[0],modifyable[0]);
		Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs, Da.pos[1],selectEndPoint == 1,selectable[1],modifyable[1]);
	}
	DYNARR_RESET(trkSeg_t,Da.midSegs);
	for (int i=0;i<Da.mid_points.cnt;i++) {
		createMidPoint(&Da.midSegs, DYNARR_N(coOrd,Da.mid_points,i),selectMidPoint == i,TRUE, TRUE );
	}
	if (Da.radius[0] >=0.0) Da.ends[0] = TRUE;
	else Da.ends[0] = FALSE;
	if (Da.radius[1] >=0.0) Da.ends[1] = TRUE;
	else Da.ends[1] = FALSE;
}

BOOL_T GetConnectedTrackParms(track_p t, const coOrd pos, int end, EPINX_T track_end, wBool_t extend) {
	trackParams_t trackParams;
	coOrd pos1;
	if (extend) pos1 = GetTrkEndPos(t,track_end);
	else pos1 = pos;
	if (!GetTrackParams(PARAMS_CORNU, t, pos1, &trackParams)) return FALSE;
	Da.radius[end] = 0.0;
	Da.center[end] = zero;
	Da.circleorHelix[end] = FALSE;
	Da.trackType[end] = trackParams.type;
	if (trackParams.type == curveTypeCurve) {
		Da.arcA0[end] = trackParams.arcA0;
		Da.arcA1[end] = trackParams.arcA1;
		Da.radius[end] = trackParams.arcR;
		Da.center[end] = trackParams.arcP;
		if (trackParams.circleOrHelix) {
			Da.circleorHelix[end] = TRUE;
			Da.angle[end] = trackParams.track_angle;	  //For Now
		} else {
			Da.angle[end] = NormalizeAngle(GetTrkEndAngle(t,track_end)+180);
			//Da.angle[end] = NormalizeAngle(trackParams.track_angle + (track_end?180:0));
		}
	} else if (trackParams.type == curveTypeBezier)  {
		Da.angle[end] = NormalizeAngle(trackParams.track_angle+(track_end?180:0));
		if (trackParams.arcR == 0) {
			Da.radius[end] = 0;
			Da.center[end] = zero;
		} else {
			Da.arcA0[end] = trackParams.arcA0;
			Da.arcA1[end] = trackParams.arcA1;
			Da.radius[end] = trackParams.arcR;
			Da.center[end] = trackParams.arcP;
		}
	} else if (trackParams.type == curveTypeCornu) {
		int ep = trackParams.ep;
		Da.angle[end] = NormalizeAngle(trackParams.cornuAngle[ep]+180);
		Da.radius[end] = trackParams.cornuRadius[ep];
		Da.pos[end] = trackParams.cornuEnd[ep];
		Da.center[end] = trackParams.cornuCenter[ep];
	} else if (trackParams.type == curveTypeStraight) {
		if (Da.ep[end]>=0)
			Da.angle[end] = NormalizeAngle(GetTrkEndAngle(t,track_end)+180);  //Ignore params.angle because it gives from nearest end
		else {
			Da.angle[end] = NormalizeAngle(trackParams.angle+180);            //Turntable
			Da.pos[end] = trackParams.lineEnd;								  //End moved to constrain angle
		}
	}
	return TRUE;
}

void CorrectHelixAngles() {
	if ( Da.circleorHelix[0] ) {
		Da.ep[0] = PickArcEndPt( Da.center[0], Da.pos[0], Da.pos[1] );
		if (Da.ep[0] == 1) Da.angle[0] = NormalizeAngle(Da.angle[0]+180);
	}
	if ( Da.circleorHelix[1] ) {
		Da.ep[1] = PickArcEndPt( Da.center[1], Da.pos[1], Da.pos[0] );
		if (Da.ep[1] == 1) Da.angle[1] = NormalizeAngle(Da.angle[1]+180);
	}
}

BOOL_T CheckHelix(track_p trk) {
	if ( Da.trk[0] && QueryTrack(Da.trk[0],Q_HAS_VARIABLE_ENDPOINTS)) {
		track_p t = GetTrkEndTrk(Da.trk[0],Da.ep[0]);
		if ( t != NULL && t != trk)  {
			ErrorMessage( MSG_TRK_ALREADY_CONN, _("First") );
			return FALSE;
		}
	}
	if ( Da.trk[1] && QueryTrack(Da.trk[1],Q_HAS_VARIABLE_ENDPOINTS)) {
		track_p t = GetTrkEndTrk(Da.trk[1],Da.ep[1]);
		if ( t != NULL && t != trk)  {
			ErrorMessage( MSG_TRK_ALREADY_CONN, _("Second") );
			return FALSE;
		}
	}
	return TRUE;
}

void SetUpCornuParms(cornuParm_t * cp) {
		cp->center[0] = Da.center[0];
		cp->angle[0] = Da.angle[0];
		cp->radius[0] = Da.radius[0];
		cp->center[1] = Da.center[1];
		cp->angle[1] = Da.angle[1];
		cp->radius[1] = Da.radius[1];
}

track_p CreateCornuFromPoints(coOrd pos[2],BOOL_T track_end[2]) {
		coOrd center[2];
		DIST_T radius[2];
		ANGLE_T angle[2];
		dynArr_t segs;
		BOOL_T back, neg;
		cornuParm_t new;
		int inx,subinx;
		coOrd pos_temp[2];

		for (int i=0;i<2;i++) {
			pos_temp[i] = pos[i];

			if (!track_end[i] || (Da.radius[i]==-1.0)) {

				angle[i] = GetAngleSegs(Da.crvSegs_da.cnt,(trkSeg_t *)(Da.crvSegs_da.ptr),&pos_temp[i],&inx,NULL,&back,&subinx,&neg);

				trkSeg_p segPtr = &DYNARR_N(trkSeg_t, Da.crvSegs_da, inx);

				if (segPtr->type == SEG_BEZTRK)
					segPtr = &DYNARR_N(trkSeg_t, segPtr->bezSegs, subinx);

				if (i==0) {
					if (neg==back) angle[i] = NormalizeAngle(angle[i]+180);
				} else {
					if (!(neg==back)) angle[i] = NormalizeAngle(angle[i]+180);
				}

				if (segPtr->type == SEG_STRTRK) {
					radius[i] = 0.0;
					center[i] = zero;
				} else if (segPtr->type == SEG_CRVTRK) {
					center[i] = segPtr->u.c.center;
					radius[i] = fabs(segPtr->u.c.radius);
				}
			} else {
				pos[i] = Da.pos[i];
				radius[i] = Da.radius[i];
				center[i] = Da.center[i];
				angle[i] = Da.angle[i];
				neg = FALSE;
				back = FALSE;
			}
		}
		new.pos[0] = pos[0];
		new.pos[1] = pos[1];
		new.angle[0] = angle[0];
		new.angle[1] = angle[1];
		new.center[0] = center[0];
		new.center[1] = center[1];
		new.radius[0] = radius[0];
		new.radius[1] = radius[1];

		track_p trk1 = NewCornuTrack(new.pos,new.center,new.angle,new.radius,NULL,0);
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
			return NULL;
		}
		return trk1;
}


struct extraData {
				cornuData_t cornuData;
		};


/*
 * AdjustCornuCurve
 *
 * Called to adjust the curve either when creating it or modifying it
 * States are "PICK_POINT" and "POINT_PICKED" and "TRACK_SELECTED".
 *
 * In PICK_POINT, the user can select an end-point to drag and release in POINT_PICKED. They can also
 * hit Enter (which saves the changes) or ESC (which cancels them).
 *
 * Deal with extended tracks from ends.
 *
 */
EXPORT STATUS_T AdjustCornuCurve(
		wAction_t action,
		coOrd pos,
		cornuMessageProc message )
{
	track_p t;
	DIST_T d;
	ANGLE_T a, a2;
	DIST_T dd;
	EPINX_T ep;
	cornuParm_t cp;


	Da.cmdType = (long)commandContext;


	if (Da.state != PICK_POINT && Da.state != POINT_PICKED && Da.state != TRACK_SELECTED) return C_CONTINUE;

	switch ( action & 0xFF) {

	case C_START:
			Da.selectEndPoint = -1;
			Da.selectMidPoint = -1;
			Da.prevSelected = -1;
			Da.extend[0] = FALSE;
			Da.extend[1] = FALSE;
			CreateBothEnds(Da.selectEndPoint, Da.selectMidPoint);
			Da.crvSegs_da.cnt = 0;
			SetUpCornuParms(&cp);

			if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
			else Da.crvSegs_da_cnt = 0;
			Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
			InfoMessage( _("Select Point, or Add Point") );
			DrawTempCornu();
			return C_CONTINUE;

	case C_DOWN:
		if (Da.state != PICK_POINT) return C_CONTINUE;
		dd = 10000.0;
		Da.selectEndPoint = -1;
		Da.selectMidPoint = -1;
		for (int i=0;i<2;i++) {
			d = FindDistance(Da.pos[i],pos);
			if (d < dd) {
				dd = d;
				Da.selectEndPoint = i;
			}
		}
		if (!IsClose(dd) ) Da.selectEndPoint = -1;
		for (int i=0;i<Da.mid_points.cnt;i++) {
			d = FindDistance(DYNARR_N(coOrd,Da.mid_points,i),pos);
			if (d < dd) {
				dd = d;
				Da.selectMidPoint = i;
				Da.selectEndPoint = -1;
			}
		}
		if (!IsClose(dd) ) Da.selectMidPoint = -1;
		if (Da.selectMidPoint ==-1 && Da.selectEndPoint ==-1) {
			coOrd temp_pos = pos;
			if (IsClose(DistanceSegs(zero,0.0,Da.crvSegs_da.cnt,(trkSeg_p)Da.crvSegs_da.ptr,&temp_pos,NULL))) {
				//Add Point between two other points
				//Find closest two points along Track
				int closest = 0;
				if (Da.mid_points.cnt>0) {
					coOrd near = pos;
					coOrd last = Da.pos[0];
					DIST_T dd = LineDistance(&near,last,DYNARR_N(coOrd,Da.mid_points,0));
					for (int i=0;i<Da.mid_points.cnt-1;i++) {
						near = pos;
						d = LineDistance(&near,DYNARR_N(coOrd,Da.mid_points,i),DYNARR_N(coOrd,Da.mid_points,i+1));
						if (d < dd) {
							dd = d;
							closest = i+1;
						}
					}
					d = LineDistance(&near,DYNARR_N(coOrd,Da.mid_points,Da.mid_points.cnt-1),Da.pos[1]);
					if (d<dd) closest = Da.mid_points.cnt;
				}
				DYNARR_APPEND(coOrd,Da.mid_points,1);
				for (int i=Da.mid_points.cnt-1;i>closest;i--) {
					DYNARR_N(coOrd,Da.mid_points,i) = DYNARR_N(coOrd ,Da.mid_points,i-1);
				}
				DYNARR_N(coOrd,Da.mid_points,closest) = pos;
				Da.selectMidPoint = closest;
				Da.number_of_points++;
			} else {
				wBeep();
				InfoMessage("Add Point Is Not on Track");
				return C_CONTINUE;
			}
		}
		if (Da.selectEndPoint == -1  && Da.selectMidPoint == -1) {
			wBeep();
			InfoMessage( _("Not close enough to point, reselect") );
			return C_CONTINUE;
		} else {
			if (Da.selectEndPoint >=0 ) {
				pos = Da.pos[Da.selectEndPoint];
				if (Da.trk[Da.selectEndPoint]) {
					InfoMessage( _("Drag along end track"));
				} else
					InfoMessage( _("Drag to move, +Shift to drag out end track from Cornu"));
			} else {
			if (Da.selectMidPoint >=0 )
				pos = DYNARR_N(coOrd,Da.mid_points,Da.selectMidPoint);
				InfoMessage( _("Drag point to new location, Delete to remove"));
			}
			Da.state = POINT_PICKED;
		}
		DrawTempCornu();   //wipe out
		CreateBothEnds(Da.selectEndPoint,Da.selectMidPoint);
		SetUpCornuParms(&cp);
		if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		else Da.crvSegs_da_cnt = 0;
		Da.minRadius = CornuMinRadius(Da.pos, Da.crvSegs_da);
		DrawTempCornu();
		return C_CONTINUE;

	case C_MOVE:
		if (Da.state != POINT_PICKED) {
			InfoMessage(_("Pick any circle to adjust, or Add - Enter to accept, Esc to cancel"));
			return C_CONTINUE;
		}
		if (Da.selectEndPoint >= 0) {
			//If locked, reset pos to be on line from other track
			int sel = Da.selectEndPoint?1:0;
			coOrd pos2 = pos;
			BOOL_T inside = FALSE;
			if (Da.trk[sel]) {
				if (OnTrack(&pos,FALSE,TRUE) == Da.trk[sel]) {		//And the pos is on it
					inside = TRUE;
					if (!QueryTrack(Da.trk[Da.selectEndPoint],Q_CORNU_CAN_MODIFY)) {  //Turnouts
						InfoMessage(_("Track can't be split"));
						inside = FALSE;
						if (Da.ep[sel]>=0)											//Ignore if turntable
							Da.pos[sel] = pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
						return C_CONTINUE;
					}
				} else {
					pos = pos2;				//Put Back to original position as outside track
				}
				if (!inside) {
					ANGLE_T diff = NormalizeAngle(GetTrkEndAngle(Da.trk[sel],Da.ep[sel])-FindAngle(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),pos));
					if (diff>90.0 && diff<270.0) {
						Da.pos[sel] = pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
						return C_CONTINUE;
					}
				}
				// Stop the user extending right through the other track
				if (Da.ep[sel]>=0 && QueryTrack(Da.trk[sel],Q_CORNU_CAN_MODIFY)) { //For non-turnouts
					if ((!QueryTrack(Da.trk[sel],Q_CAN_ADD_ENDPOINTS))        // But Not Helix or Circle
					&& (!QueryTrack(Da.trk[sel],Q_HAS_VARIABLE_ENDPOINTS))) { // Not a Turntable
						DIST_T ab = FindDistance(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]));
						DIST_T ac = FindDistance(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),pos);
						DIST_T cb = FindDistance(GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]),pos);
						if (cb<minLength) {
							InfoMessage(_("Too close to other end of selected Track"));
							return C_CONTINUE;
						}
						if ((ac>=cb) && (ac>=ab)) { //Closer to far end and as long as the track
							pos = GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]);  //Make other end of track
						}
					}
				} else if (inside) {
					InfoMessage(_("Can't move end inside a Turnout"));		//Turnouts are stuck to end-point
					Da.pos[sel] = pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
					CreateBothEnds(Da.selectEndPoint,Da.selectMidPoint);
					return C_CONTINUE;
				}
			}
			DrawTempCornu();
			Da.extend[sel] = FALSE;
			if(!Da.trk[sel]) {							//Cornu with no ends
				if (Da.selectTrack) {					//There is already a track
					if (inside) {
						Da.pos[sel] = pos;
						struct extraData *xx = GetTrkExtraData(Da.selectTrack);
						Da.center[sel] = xx->cornuData.c[sel];			//Reset center
						Da.extend[sel] = FALSE;
					}
					if (!inside && ((MyGetKeyState() & WKEY_SHIFT) != 0)) {  //Shift extends out old track
						coOrd pos2 = Da.pos[sel];
						SetUpCornuParms(&cp);
						CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,FALSE);
						GetCornuParmsTemp(&Da.crvSegs_da, sel, &pos2, &Da.center[sel], &Da.angle[sel],  &Da.radius[sel] );
						struct extraData *xx = GetTrkExtraData(Da.selectTrack);
						if (Da.radius[sel] == 0)  {				//Straight
							Da.extendSeg[sel].type = SEG_STRTRK;
							Da.extendSeg[sel].width = 0;
							Da.extendSeg[sel].color = wDrawColorBlack;
							Da.extendSeg[sel].u.l.pos[1-sel] = Da.pos[sel];
							d = FindDistance( Da.extendSeg[sel].u.l.pos[1-sel], pos );
							a = NormalizeAngle(Da.angle[sel]-FindAngle(pos,Da.pos[sel]));
							if (cos(D2R(a))<=0) {
								Translate( &Da.extendSeg[sel].u.l.pos[sel],
											Da.extendSeg[sel].u.l.pos[1-sel],
											Da.angle[sel], - d * cos(D2R(a)));
								pos = Da.extendSeg[sel].u.l.pos[sel];
								Da.extend[sel] = TRUE;
							}
						} else {								//Curve
							Da.extendSeg[sel].type = SEG_CRVTRK;
							Da.extendSeg[sel].width = 0;
							Da.extendSeg[sel].color = wDrawColorBlack;
							Da.extendSeg[sel].u.c.center = Da.center[sel];
							//coOrd offset;
							//offset.x = Da.pos[sel].x-xx->cornuData.pos[sel].x;
							//offset.y = Da.pos[sel].y-xx->cornuData.pos[sel].y;
							//Da.extendSeg[sel].u.c.center.x =xx->cornuData.c[sel].x+offset.x;
							//Da.extendSeg[sel].u.c.center.y =xx->cornuData.c[sel].y+offset.y;
							Da.extendSeg[sel].u.c.radius = Da.radius[sel];
							a = FindAngle( Da.center[sel], pos );
							PointOnCircle( &pos, Da.extendSeg[sel].u.c.center, Da.radius[sel], a );
							a2 = FindAngle(Da.extendSeg[sel].u.c.center,Da.pos[sel]);
							if (((Da.angle[sel] < 180) && (a2>90 && a2<270)) ||
								((Da.angle[sel] > 180) && (a2<90 || a2>270))) {
								Da.extendSeg[sel].u.c.a0 = a;
								Da.extendSeg[sel].u.c.a1 = NormalizeAngle(a2-a);
							} else {
								Da.extendSeg[sel].u.c.a0 = a2;
								Da.extendSeg[sel].u.c.a1 = NormalizeAngle(a-a2);
							}
							if (Da.extendSeg[sel].u.c.a1 == 0 || Da.extendSeg[sel].u.c.a1 >180 )
								Da.extend[sel] = FALSE;
							else
								Da.extend[sel] = TRUE;
						}
						//if (Da.extend[sel] == FALSE) {          	// Not extending - so trim along our own Cornu
						//	GetCornuParmsTemp(&Da.crvSegs_da, sel, &pos, &Da.center[sel], &Da.angle[sel],  &Da.radius[sel] );
						//	Da.pos[sel] = pos;
						//}
					} else {									//Just move end (no shift)
						Da.pos[sel] = pos;
						if (Da.selectTrack) {					//Track already exists
							coOrd offset;
							struct extraData *xx = GetTrkExtraData(Da.selectTrack);
							offset.x = Da.pos[sel].x-xx->cornuData.pos[sel].x;
							offset.y = Da.pos[sel].y-xx->cornuData.pos[sel].y;
							Da.center[sel].x = xx->cornuData.c[sel].x+offset.x;
							Da.center[sel].y = xx->cornuData.c[sel].y+offset.y;
						}										//No Track, no end so radius = 0;
					}
				} else {
					Da.pos[sel] = pos;
				}
			} else {									//Cornu with ends
				if (inside) Da.pos[sel] = pos;
				if (!GetConnectedTrackParms(Da.trk[sel],pos,sel,Da.ep[sel],inside?FALSE:TRUE)) {
					DrawTempCornu();
					wBeep();
					return C_CONTINUE;											//Stop drawing
				}
				CorrectHelixAngles();
				if (!inside) {										 //Extend the track
					if (Da.trackType[sel] == curveTypeStraight) {    //Extend with a straight
						Da.extendSeg[sel].type = SEG_STRTRK;
						Da.extendSeg[sel].width = 0;
						Da.extendSeg[sel].color = wDrawColorBlack;
						if (Da.ep[sel]>=0) {
							Da.extendSeg[sel].u.l.pos[0] = GetTrkEndPos( Da.trk[sel], Da.ep[sel] );
							a = NormalizeAngle(Da.angle[sel]-FindAngle(pos,GetTrkEndPos(Da.trk[sel],Da.ep[sel])));
						} else {												//Turntable when unconnected
							Da.extendSeg[sel].u.l.pos[0] = Da.pos[sel];
							a = NormalizeAngle(Da.angle[sel]-FindAngle(pos,Da.pos[sel]));
						}
						// Remove any extend in opposite direction for Turntable/Turnouts
						if ((QueryTrack(Da.trk[sel],Q_CAN_ADD_ENDPOINTS)  && Da.ep[sel]>=0)
								&& (!QueryTrack(Da.trk[sel],Q_CORNU_CAN_MODIFY))
								&& (a>90 && a<270)) {
							Da.extend[sel] = FALSE;    //Turntable with point and extension is other side of well
							Da.pos[sel] = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
						} else {
							Da.extend[sel] = TRUE;
							d = FindDistance( Da.extendSeg[sel].u.l.pos[0], pos );
							Translate( &Da.extendSeg[sel].u.l.pos[1],
									Da.extendSeg[sel].u.l.pos[0],
									Da.angle[sel], -d * cos(D2R(a)));
							Da.pos[sel] = pos = Da.extendSeg[sel].u.l.pos[1];
						}
					} else if (Da.trackType[sel] == curveTypeCurve) {       //Extend with temp curve
						Da.extendSeg[sel].type = SEG_CRVTRK;
						Da.extendSeg[sel].width = 0;
						Da.extendSeg[sel].color = wDrawColorBlack;
						Da.extendSeg[sel].u.c.center = Da.center[sel];
						Da.extendSeg[sel].u.c.radius = Da.radius[sel];
						a = FindAngle( Da.center[sel], pos );
						PointOnCircle( &pos, Da.center[sel], Da.radius[sel], a );
						a2 = FindAngle(Da.center[sel],GetTrkEndPos(Da.trk[sel],Da.ep[sel]));
						if ((Da.angle[sel] < 180 && (a2>90 && a2 <270))  ||
								(Da.angle[sel] > 180 && (a2<90 || a2 >270))) {
							Da.extendSeg[sel].u.c.a0 = a2;
							Da.extendSeg[sel].u.c.a1 = NormalizeAngle(a-a2);
						} else {
							Da.extendSeg[sel].u.c.a0 = a;
							Da.extendSeg[sel].u.c.a1 = NormalizeAngle(a2-a);
						}
						if (Da.extendSeg[sel].u.c.a1 == 0.0 || Da.extendSeg[sel].u.c.a1 >180
								|| (Da.extendSeg[sel].u.c.a0 >= Da.arcA0[sel] && Da.extendSeg[sel].u.c.a0 < Da.arcA0[sel]+Da.arcA1[sel]
									&& Da.extendSeg[sel].u.c.a0 + Da.extendSeg[sel].u.c.a1 <= Da.arcA0[sel] + Da.arcA1[sel])
							) {
							Da.extend[sel] = FALSE;
							Da.pos[sel] = pos;
						} else {
							Da.extend[sel] = TRUE;
							Da.pos[sel] = pos;
						}

					} else {	//Bezier and Cornu that we are joining TO can't extend
						wBeep();
						InfoMessage(_("Can't extend connected Bezier or Cornu"));
						pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
						return C_CONTINUE;
					}
				} else Da.pos[sel] = pos;
			}
		} else if (Da.selectMidPoint >=0){
			DrawTempCornu();
			DYNARR_N(coOrd,Da.mid_points,Da.selectMidPoint) = pos;
		}
		CreateBothEnds(Da.selectEndPoint,Da.selectMidPoint);
		SetUpCornuParms(&cp);    //In case we want to use these because the ends are not on the track

		if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		else Da.crvSegs_da_cnt = 0;
		Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
		DIST_T rin = Da.radius[0];
		InfoMessage( _("Cornu : Min Radius=%s MaxRateofCurveChange/Scale=%s Length=%s Winding Arc=%s"),
									FormatDistance(Da.minRadius),
									FormatFloat(CornuMaxRateofChangeofCurvature(Da.pos,Da.crvSegs_da,&rin)*GetScaleRatio(GetLayoutCurScale())),
									FormatDistance(CornuLength(Da.pos,Da.crvSegs_da)),
									FormatDistance(CornuTotalWindingArc(Da.pos,Da.crvSegs_da)));
		DrawTempCornu();
		return C_CONTINUE;

	case C_UP:
		if (Da.state != POINT_PICKED) return C_CONTINUE;
		ep = 0;
		DrawTempCornu();  //wipe out
		if (Da.selectMidPoint!=-1) Da.prevSelected = Da.selectMidPoint;
		Da.selectEndPoint = -1; Da.selectMidPoint = -1;
		CreateBothEnds(Da.selectEndPoint,Da.prevSelected);
		SetUpCornuParms(&cp);
		if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		else Da.crvSegs_da_cnt = 0;
		Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
		InfoMessage(_("Pick on point to adjust it along track - Delete to remove, Enter to confirm, ESC to abort"));
		DrawTempCornu();
		Da.state = PICK_POINT;
		return C_CONTINUE;

	case C_TEXT:
		//Delete or backspace deletes last selected index
		if (action>>8 == 127 || action>>8 == 8) {
			if ((Da.state == PICK_POINT) && Da.prevSelected !=-1) {
				for (int i=Da.prevSelected;i<Da.mid_points.cnt;i++) {
					DYNARR_N(coOrd,Da.mid_points,i) = DYNARR_N(coOrd,Da.mid_points,i+1);
				}
				Da.mid_points.cnt--;
			}
			Da.prevSelected = -1;
			CreateBothEnds(Da.selectEndPoint,Da.selectMidPoint);
			cornuParm_t cp;
			SetUpCornuParms(&cp);
			if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
			else Da.crvSegs_da_cnt = 0;
			MainRedraw();
			MapRedraw();
			return C_CONTINUE;
		}
		return C_CONTINUE;

	case C_OK:                            //C_OK is not called by Modify.
		if ( Da.state == PICK_POINT ) {
			Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
			if (CornuTotalWindingArc(Da.pos,Da.crvSegs_da)>4*360) {
				wBeep();
				InfoMessage(_("Cornu has too complex shape - adjust end pts"));
				return C_CONTINUE;
			}
			if (!CheckHelix(NULL)) {
				wBeep();
				return C_CONTINUE;
			}
			for (int i=0;i<2;i++) {
				if (Da.trk[i] && !(QueryTrack(Da.trk[i],Q_CAN_ADD_ENDPOINTS))) {        // Not Turntable
					if (FindDistance(Da.pos[i],GetTrkEndPos(Da.trk[i],1-Da.ep[i])) < minLength) {
					wBeep();
					InfoMessage(_("Cornu point %d too close to other end of connect track - reposition it"),i+1);
					return C_CONTINUE;
					}
				}
			}
			DrawTempCornu();
			UndoStart( _("Create Cornu"),"newCornu curves");
			BOOL_T end_point[2];
			end_point[0] = TRUE;
			end_point[1] = FALSE;
			coOrd sub_pos[2];
			sub_pos[0] = Da.pos[0];
			if (Da.radius[0] == -1) end_point[0] = FALSE;
			track_p first_trk= NULL,trk1=NULL,trk2 = NULL;

			for (int i=0;i<Da.mid_points.cnt;i++) {
				sub_pos[1] = DYNARR_N(coOrd,Da.mid_points,i);
				if ((trk1 = CreateCornuFromPoints(sub_pos,end_point))== NULL) return C_TERMINATE;
				if (Da.trk[0]) {
					CopyAttributes( Da.trk[0], trk1 );
				} else if (Da.trk[1]) {
					CopyAttributes( Da.trk[1], trk1 );
				} else {
					SetTrkScale( trk1, GetLayoutCurScale() );
					SetTrkBits( trk1, TB_HIDEDESC );
				}
				DrawNewTrack(trk1);
				if (first_trk == NULL) first_trk = trk1;
				if (trk2) ConnectTracks(trk1,0,trk2,1);
				trk2 = trk1;
				end_point[0] = FALSE;
				sub_pos[0] = DYNARR_N(coOrd,Da.mid_points,i);
			}
			sub_pos[1] = Da.pos[1];
			end_point[1] = TRUE;
			if (Da.radius[1] == -1) end_point[1] = FALSE;
			if ((trk1 = CreateCornuFromPoints(sub_pos,end_point)) == NULL) return C_TERMINATE;
			DrawNewTrack(trk1);
			if (Da.trk[0]) {
				CopyAttributes( Da.trk[0], trk1 );
			} else if (Da.trk[1]){
				CopyAttributes( Da.trk[1], trk1 );
			} else {
				SetTrkScale( trk1, GetLayoutCurScale() );
				SetTrkBits( trk1, TB_HIDEDESC );
			}
			if (trk2) ConnectTracks(trk1,0,trk2,1);
			if (first_trk == NULL) first_trk = trk1;
			//t = NewCornuTrack( Da.pos, Da.center, Da.angle, Da.radius,(trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);

			for (int i=0;i<2;i++) {
				if (Da.trk[i]) {
					UndoModify(Da.trk[i]);
					MoveEndPt(&Da.trk[i],&Da.ep[i],Da.pos[i],0);
					if ((GetTrkType(Da.trk[i])==T_BEZIER) || (GetTrkType(Da.trk[i])==T_CORNU)) {     //Bezier split position not precise, so readjust Cornu
						GetConnectedTrackParms(Da.trk[i],GetTrkEndPos(Da.trk[i],Da.ep[i]),i,Da.ep[i],FALSE);
						ANGLE_T endAngle = NormalizeAngle(GetTrkEndAngle(Da.trk[i],Da.ep[i])+180);
						SetCornuEndPt(i==0?first_trk:trk1,i,GetTrkEndPos(Da.trk[i],Da.ep[i]),Da.center[i],endAngle,Da.radius[i]);
					}
					if (Da.ep[i]>=0)
						ConnectTracks(Da.trk[i],Da.ep[i],i==0?first_trk:trk1,i);
				}
			}
			UndoEnd();
			Da.state = NONE;
			MainRedraw();
			MapRedraw();
			return C_TERMINATE;
		}
		return C_CONTINUE;

	case C_REDRAW:
		DrawTempCornu();
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}


}


/**
 * CmdCornuModify
 *
 * Called from Modify Command - this function deals with the real (old) track and calls AdjustCornuCurve to tune up the new one
 * Sequence is this -
 * - The C_START is called from CmdModify C_DOWN action if a track has being selected. The old track is hidden, the editable one is shown.
 * - C_MOVES will be ignored until a C_UP ends the track selection and moves the state to PICK_POINT,
 * - C_DOWN then hides the track and shows the Cornu circles. Selects a point (if close enough and available) and the state moves to POINT_PICKED
 * - C_MOVE drags the point around modifying the curve
 * - C_UP puts the state back to PICK_POINT (pick another)
 * - C_OK (Enter/Space) creates the new track, deletes the old and shows the changed track.
 * - C_CANCEL (Esc) sets the state to NONE and reshows the original track unchanged.
 *
 */
STATUS_T CmdCornuModify (track_p trk, wAction_t action, coOrd pos, DIST_T trackG ) {
	track_p t;
	struct extraData *xx = GetTrkExtraData(trk);

	Da.trackGauge = trackG;

	switch (action&0xFF) {
	case C_START:
		Da.state = NONE;
		DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
		Da.ep1Segs_da_cnt = 0;
		Da.ep2Segs_da_cnt = 0;
		Da.crvSegs_da_cnt = 0;
		Da.midSegs.cnt = 0;
		Da.extend[0] = FALSE;
	    Da.extend[1] = FALSE;
		Da.selectEndPoint = -1;
		Da.selectTrack = NULL;
		DYNARR_RESET(coOrd,Da.mid_points);
		DYNARR_RESET(track_p,Da.tracks);


		Da.selectTrack = trk;
		DYNARR_APPEND(track_p,Da.tracks,1);
		DYNARR_LAST(track_p,Da.tracks) = trk;
	    Da.trk[0] = GetTrkEndTrk( trk, 0 );
	    track_p prior = trk;
		if (Da.trk[0]) Da.ep[0] = GetEndPtConnectedToMe(Da.trk[0],trk);
		EPINX_T ep0 = 0;
		//Move down the LHS adding tracks until no more Cornu
		while (Da.trk[0] && QueryTrack(Da.trk[0],Q_IS_CORNU)) {
			prior = Da.trk[0];
			ep0 = 1-Da.ep[0];
			DYNARR_APPEND(track_p,Da.tracks,1);
			DYNARR_LAST(track_p,Da.tracks) = prior;
			DYNARR_APPEND(coOrd,Da.mid_points,1);
			for (int i=0;i<Da.mid_points.cnt-1;i++) {
				DYNARR_N(coOrd,Da.mid_points,i+1) = DYNARR_N(coOrd,Da.mid_points,i);
			}
			DYNARR_N(coOrd,Da.mid_points,0) = GetTrkEndPos(prior,1-ep0);
			Da.trk[0] = GetTrkEndTrk( prior, ep0 );
			if (Da.trk[0]) Da.ep[0] = GetEndPtConnectedToMe(Da.trk[0],prior);
			else Da.ep[0] = -1;
		}

		if (prior) {
			struct extraData *xx0 = GetTrkExtraData(prior);
			Da.pos[0] = xx0->cornuData.pos[ep0];              //Copy parms from FIRST CORNU trk
			Da.radius[0] = xx0->cornuData.r[ep0];
			Da.angle[0] = xx0->cornuData.a[ep0];
			Da.center[0] = xx0->cornuData.c[ep0];
		}

		//Move to RHS

		Da.trk[1] = GetTrkEndTrk( trk, 1 );
		track_p next = trk;
		EPINX_T ep1 = 1;
		if (Da.trk[1]) Da.ep[1] = GetEndPtConnectedToMe(Da.trk[1],trk);
		//Move down RHS adding tracks until no more Cornu
		while (Da.trk[1] && QueryTrack(Da.trk[1],Q_IS_CORNU)) {
			next = Da.trk[1];
			ep1 = 1-Da.ep[1];
			DYNARR_APPEND(track_p,Da.tracks,1);
			DYNARR_LAST(track_p,Da.tracks) = next;
			DYNARR_APPEND(coOrd,Da.mid_points,1);
			DYNARR_LAST(coOrd,Da.mid_points) = GetTrkEndPos(next,1-ep1);
			Da.trk[1] = GetTrkEndTrk( next, ep1 );
			if (Da.trk[1]) Da.ep[1] = GetEndPtConnectedToMe(Da.trk[1],next);
		}

		if (next) {
			struct extraData *xx1 = GetTrkExtraData(next);
			Da.pos[1] = xx1->cornuData.pos[ep1];              //Copy parms from LAST CORNU trk
			Da.radius[1] = xx1->cornuData.r[ep1];
			Da.angle[1] = xx1->cornuData.a[ep1];
			Da.center[1] = xx1->cornuData.c[ep1];
		}

	    //if ((Da.trk[0] && (!QueryTrack(Da.trk[0],Q_CORNU_CAN_MODIFY) && !QueryTrack(Da.trk[0],Q_CAN_EXTEND))) &&
		//	(Da.trk[1] && (!QueryTrack(Da.trk[1],Q_CORNU_CAN_MODIFY) && !QueryTrack(Da.trk[1],Q_CAN_EXTEND)))) {
		//	wBeep();
		//	ErrorMessage("Both Ends of this Cornu are UnAdjustable");
		//	return C_TERMINATE;
		//}

		InfoMessage(_("Now Select or Add (+Shift) a Point"));
		Da.state = TRACK_SELECTED;
		for (int i=0;i<Da.tracks.cnt;i++) {
			DrawTrack(DYNARR_N(track_p,Da.tracks,i),&mainD,wDrawColorWhite);  //Wipe out real tracks, draw replacement
		}
		return AdjustCornuCurve(C_START, pos, InfoMessage);

	case C_DOWN:
		if (Da.state == TRACK_SELECTED) return C_CONTINUE;                   //Ignore until first up
		return AdjustCornuCurve(C_DOWN, pos, InfoMessage);


	case C_MOVE:
		if (Da.state == TRACK_SELECTED) return C_CONTINUE;                   //Ignore until first up and down
		return AdjustCornuCurve(C_MOVE, pos, InfoMessage);

	case C_UP:
		if (Da.state == TRACK_SELECTED) {
			Da.state = PICK_POINT;                                           //First time up, next time pick a point
		}
		return AdjustCornuCurve(C_UP, pos, InfoMessage);					//Run Adjust

	case C_TEXT:
		//Delete or backspace deletes last selected index
		if (action>>8 == 127 || action>>8 == 8) {
			return AdjustCornuCurve(action, pos, InfoMessage);
		}
		//Space bar or enter means done
		if ((action>>8) != 32)
			return C_CONTINUE;
		/* no break */
	case C_OK:
		if (Da.state != PICK_POINT) {										//Too early - abandon
			InfoMessage(_("No changes made"));
			Da.state = NONE;
			//DYNARR_FREE(trkSeg_t,Da.crvSegs_da);
			return C_CANCEL;
		}
		if (!CheckHelix(trk)) {
			wBeep();
			return C_CONTINUE;
		}
		for (int i=0;i<2;i++) {
			if (Da.trk[i] &&
				!(QueryTrack(Da.trk[i],Q_CAN_ADD_ENDPOINTS))) {        // Not Turntable
				if (FindDistance(Da.pos[i],GetTrkEndPos(Da.trk[i],1-Da.ep[i])) < minLength) {
					wBeep();
					InfoMessage(_("Cornu end %d too close to other end of connect track - reposition it"),i+1);
					return C_CONTINUE;
				}
			}
		}
		UndoStart( _("Modify Cornu"), "newCornu - CR" );
		for (int i=0;i<2;i++) {
			if (!Da.trk[i] && Da.extend[i]) {
				if (Da.extendSeg[i].type == SEG_STRTRK) {
					Da.trk[i] = NewStraightTrack(Da.extendSeg[i].u.l.pos[0],Da.extendSeg[i].u.l.pos[1]);
					if (Da.trk[i]) Da.ep[i] = 1-i;
				} else {
					Da.trk[i] = NewCurvedTrack(Da.extendSeg[i].u.c.center,fabs(Da.extendSeg[i].u.c.radius),
							Da.extendSeg[i].u.c.a0,Da.extendSeg[i].u.c.a1,FALSE);
					if (FindDistance(GetTrkEndPos(Da.trk[i],0),Da.pos[i])<=connectDistance) {
						Da.ep[i] = 0;
					} else Da.ep[i] = 1;
				}
				if (!Da.trk[i]) {
					wBeep();
					InfoMessage(_("Cornu Extension Create Failed for end %d"),i);
					Da.state = NONE;
					return C_TERMINATE;
				}

			}
		}
		BOOL_T end_point[2];
		end_point[0] = TRUE;
		end_point[1] = FALSE;
		coOrd sub_pos[2];
		sub_pos[0] = Da.pos[0];

		track_p first_trk= NULL,trk1=NULL,trk2 = NULL;
		for (int i=0;i<Da.mid_points.cnt;i++) {
			sub_pos[1] = DYNARR_N(coOrd,Da.mid_points,i);
			if ((trk1 = CreateCornuFromPoints(sub_pos, end_point))== NULL) {
				wBeep();
				InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
									Da.pos[0].x,Da.pos[0].y,
									Da.pos[1].x,Da.pos[1].y,
									Da.center[0].x,Da.center[0].y,
									Da.center[1].x,Da.center[1].y,
									Da.angle[0],Da.angle[1],
									FormatDistance(Da.radius[0]),FormatDistance(Da.radius[1]));
				UndoUndo();
				Da.state = NONE;
				MainRedraw();
				MapRedraw();
				return C_TERMINATE;
			}
			if (Da.trk[0]) {
				CopyAttributes( Da.trk[0], trk1 );
			} else if (Da.trk[1]) {
				CopyAttributes( Da.trk[1], trk1 );
			} else {
				SetTrkScale( trk1, GetLayoutCurScale() );
				SetTrkBits( trk1, TB_HIDEDESC );
			}
			DrawNewTrack(trk1);
			if (first_trk == NULL) first_trk = trk1;
			if (trk2) ConnectTracks(trk1,0,trk2,1);
			trk2 = trk1;
			end_point[0] = FALSE;
			sub_pos[0] = DYNARR_N(coOrd,Da.mid_points,i);
		}
		sub_pos[1] = Da.pos[1];
		end_point[1] = TRUE;
		if ((trk1 = CreateCornuFromPoints(sub_pos,end_point)) == NULL) {
			wBeep();
			InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
								Da.pos[0].x,Da.pos[0].y,
								Da.pos[1].x,Da.pos[1].y,
								Da.center[0].x,Da.center[0].y,
								Da.center[1].x,Da.center[1].y,
								Da.angle[0],Da.angle[1],
								FormatDistance(Da.radius[0]),FormatDistance(Da.radius[1]));
			UndoUndo();
			Da.state = NONE;
			MainRedraw();
			MapRedraw();
			return C_TERMINATE;
		}
		DrawNewTrack(trk1);
		if (Da.trk[0]) {
			CopyAttributes( Da.trk[0], trk1 );
		} else if (Da.trk[1]) {
			CopyAttributes( Da.trk[1], trk1 );
		} else {
			SetTrkScale( trk1, GetLayoutCurScale() );
			SetTrkBits( trk1, TB_HIDEDESC );
		}
		if (trk2) ConnectTracks(trk1,0,trk2,1);
		if (first_trk == NULL) first_trk = trk1;

		Da.state = NONE;       //Must do before Delete

		for (int i=0;i<Da.tracks.cnt;i++) {
			DeleteTrack(DYNARR_N(track_p,Da.tracks,i), TRUE);
		}

		if (Da.trk[0]) UndoModify(Da.trk[0]);
		if (Da.trk[1]) UndoModify(Da.trk[1]);

		for (int i=0;i<2;i++) {										//Attach new track
			if (Da.trk[i] && Da.ep[i] != -1) {						//Like the old track
				if (MoveEndPt(&Da.trk[i],&Da.ep[i],Da.pos[i],0)) {
					if (GetTrkType(Da.trk[i])==T_BEZIER) {     //Bezier split position not precise, so readjust Cornu
						GetConnectedTrackParms(Da.trk[i],GetTrkEndPos(Da.trk[i],Da.ep[i]),i,Da.ep[i],FALSE);
						ANGLE_T endAngle = NormalizeAngle(GetTrkEndAngle(Da.trk[i],Da.ep[i])+180);
						SetCornuEndPt(i==0?first_trk:trk1,i,GetTrkEndPos(Da.trk[i],Da.ep[i]),Da.center[i],endAngle,Da.radius[i]);
					}
					if (Da.ep[i]>= 0)
						ConnectTracks(i==0?first_trk:trk1,i,Da.trk[i],Da.ep[i]);
				} else {
					UndoUndo();
					wBeep();
					InfoMessage(_("Connected Track End Adjust for end %d failed"),i);
					return C_TERMINATE;
				}
			}
		}
		UndoEnd();
		MainRedraw();
		MapRedraw();
		Da.state = NONE;
		//DYNARR_FREE(trkSeg_t,Da.crvSegs_da)
		return C_TERMINATE;

	case C_CANCEL:
		InfoMessage(_("Modify Cornu Cancelled"));
		Da.state = NONE;
		//DYNARR_FREE(trkSeg_t,Da.crvSegs_da);
		MainRedraw();
		MapRedraw();
		return C_TERMINATE;

	case C_REDRAW:
		if (Da.state != NONE)
			DrawTrack(Da.selectTrack,&mainD,wDrawColorWhite);
		return AdjustCornuCurve(C_REDRAW, pos, InfoMessage);
	}

	return C_CONTINUE;

}

/*
 * Find length by adding up the underlying segments. The segments can be straights, curves or bezier.
 */
DIST_T CornuLength(coOrd pos[4],dynArr_t segs) {

	DIST_T dd = 0.0;
	if (segs.cnt == 0 ) return dd;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs(t.u.c.radius*D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd +=CornuLength(t.u.b.pos,t.bezSegs);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK ) {
			dd += FindDistance(t.u.l.pos[0],t.u.l.pos[1]);
		}
	}
	return dd;
}

DIST_T CornuOffsetLength(dynArr_t segs, double offset) {
	DIST_T dd = 0.0;
	if (segs.cnt == 0 ) return dd;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs((t.u.c.radius+(t.u.c.radius>0?offset:-offset))*D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd +=CornuOffsetLength(t.bezSegs,offset);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK ) {
			dd += FindDistance(t.u.l.pos[0],t.u.l.pos[1]);
		}
	}
	return dd;
}

DIST_T CornuMinRadius(coOrd pos[4],dynArr_t segs) {
	DIST_T r = 100000.0, rr;
	if (segs.cnt == 0 ) return r;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr = fabs(t.u.c.radius);
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rr = CornuMinRadius(t.u.b.pos, t.bezSegs);
		} else rr = 100000.00;
		if (rr<r) r = rr;
	}
	return r;
}

DIST_T CornuTotalWindingArc(coOrd pos[4],dynArr_t segs) {
	DIST_T rr = 0;
	if (segs.cnt == 0 ) return 0;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr += t.u.c.a1;
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rr += CornuTotalWindingArc(t.u.b.pos, t.bezSegs);
		}
	}
	return rr;
}

DIST_T CornuMaxRateofChangeofCurvature(coOrd pos[4], dynArr_t segs, DIST_T * last_c) {
	DIST_T r_max = 0.0, rc, lc = 0.0;
	lc = * last_c;
	segProcData_t segProcData;
	if (segs.cnt == 0 ) return r_max;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_FILCRCL) continue;
		SegProc(SEGPROC_LENGTH,&t,&segProcData);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rc = fabs(1/fabs(t.u.c.radius) - lc)/segProcData.length.length/2;
			lc = 1/fabs(t.u.c.radius);
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rc = CornuMaxRateofChangeofCurvature(t.u.b.pos, t.bezSegs,&lc);  //recurse
		} else {
			rc = fabs(0.0-lc)/segProcData.length.length/2;
			lc = 0.0;
		}
		if (rc > r_max) r_max = rc;
	}
	* last_c = lc;
	return r_max;
}

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

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

/*
 * Create a Cornu Curve Track
 * Sequence is
 * 1. Place 1st End
 * 2. Place 2nd End
 * 3 to n. Select and drag around points until done
 * n+1. Confirm with enter or Cancel with Esc
 */
STATUS_T CmdCornu( wAction_t action, coOrd pos )
{
	track_p t;
	cornuParm_t cp;

	Da.color = lineColor;
	Da.width = (double)lineWidth/mainD.dpi;

	Da.trackGauge = trackGauge;
	Da.selectTrack = NULL;

	Da.cmdType = (long)commandContext;

	switch (action&0xFF) {

	case C_START:
		Da.state = NONE;
		Da.selectEndPoint = -1;
		Da.selectMidPoint = -1;
		for (int i=0;i<2;i++) {
			Da.pos[i] = zero;
		}
		Da.trk[0] = Da.trk[1] = NULL;
		//tempD.orig = mainD.orig;

		DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
		Da.ep1Segs_da_cnt = 0;
		Da.ep2Segs_da_cnt = 0;
		Da.crvSegs_da_cnt = 0;
		Da.midSegs.cnt = 0;
		DYNARR_RESET(coOrd,Da.mid_points);
		DYNARR_RESET(track_p,Da.tracks);
		DYNARR_RESET(trkSeg_t,anchors_da);
		Da.extend[0] = FALSE;
		Da.extend[1] = FALSE;
		if (Da.cmdType == cornuCmdCreateTrack)
			InfoMessage( _("Left click - Start Cornu track") );
		else {
			if (selectedTrackCount==0)
				InfoMessage( _("Left click - join with Cornu track") );
			else
				InfoMessage( _("Left click - join with Cornu track, Shift Left click - move to join") );
		}
		return C_CONTINUE;

	case C_DOWN:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if ( Da.state == NONE || Da.state == LOC_2) {   //Set the first or second point
			coOrd p = pos;
			t = NULL;
			int end = 0;
			if (Da.state != NONE) end=1;
			EPINX_T ep = -1;
			//Lock to endpoint if one is available and under pointer
		    if ((t = OnTrack(&p, FALSE, TRUE)) != NULL) {
		    	if (QueryTrack(t,Q_HAS_VARIABLE_ENDPOINTS)) {    //Circle/Helix find if there is an open slot and where
		    		if ((GetTrkEndTrk(t,0) != NULL) && (GetTrkEndTrk(t,1) != NULL)) {
						InfoMessage(_("Helix Already Connected"));
						return C_CONTINUE;
					}
					ep = -1;                                            //Not a real ep yet
		    	} else ep = PickUnconnectedEndPointSilent(p, t);		//EP
				if (ep>=0 && QueryTrack(t,Q_CAN_ADD_ENDPOINTS)) ep=-1;  		//Don't attach to Turntable
				if ( ep==-1 && (!QueryTrack(t,Q_CAN_ADD_ENDPOINTS) && !QueryTrack(t,Q_HAS_VARIABLE_ENDPOINTS))) {  //No endpoints and not Turntable or Helix/Circle
					wBeep();
					InfoMessage(_("No Valid end point on that track"));
					return C_CONTINUE;
				}
				if (GetTrkScale(t) != (char)GetLayoutCurScale()) {
					wBeep();
					InfoMessage(_("Track is different scale"));
					return C_CONTINUE;
				}
			}
			if (ep>=0 && t) {				//Real end point, real track
				Da.trk[end] = t;
				Da.ep[end] = ep;           // Note: -1 for Turntable or Circle

				pos = GetTrkEndPos(t,ep);
				Da.pos[end] = pos;
				InfoMessage( _("Place 2nd end point of Cornu track on track with an unconnected end-point") );
			} else if (t == NULL) {      //end not on Track, OK for CreateCornu -> empty end point
				pos = p;	//Reset to initial
				if (Da.cmdType == cornuCmdCreateTrack) {
					Da.trk[end] = NULL;
					Da.pos[end] = pos;
					Da.radius[end] = -1.0;   //No End Rad for open
					if (Da.state == NONE ) {
						Da.state = POS_1;
						Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0], FALSE,TRUE,TRUE);
						DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
						InfoMessage( _("Unlocked - Move 1st end point of Cornu track") );
						return C_CONTINUE;
					}
					Da.state = POINT_PICKED;    //Now this is second end and it is open
					Da.selectEndPoint = 1;
					Da.mid_points.cnt=0;
					CreateBothEnds(1,-1);
					SetUpCornuParms(&cp);
					if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE))
							Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
					DrawTempCornu();
					InfoMessage( _("Unlocked - Move 2nd end point of Cornu track") );
					return C_CONTINUE;
				}
				wBeep();
				InfoMessage(_("No Unconnected Track End there"));  //Not creating a Cornu - Join can't be open
				return C_CONTINUE;
			}   //Either a real end or a track but no end

			if (Da.state == NONE) {

				if (!GetConnectedTrackParms(t, pos, 0, Da.ep[0],FALSE)) {  //Must get parms
					Da.trk[0] = NULL;
					Da.ep[0] = -1;
					wBeep();
					InfoMessage(_("No Valid Track End there"));
					return C_CONTINUE;
				}
				if (ep<0) {
						Da.trk[0] = t;
						Da.pos[0] = pos;
				}
				Da.state = POS_1;
				Da.selectEndPoint = 0;        //Select first end point
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0], FALSE, !QueryTrack(Da.trk[0],Q_IS_CORNU),QueryTrack(Da.trk[0],Q_CORNU_CAN_MODIFY));
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
				InfoMessage( _("Locked - Move 1st end point of Cornu track along track 1") );
			} else {					//Second Point
				if (Da.trk[0] == t) {
				   ErrorMessage( MSG_JOIN_CORNU_SAME );
				   Da.trk[1] = NULL;
				   Da.ep[1] = -1;
				   return C_CONTINUE;
				}
				if (!GetConnectedTrackParms(t, pos, 1, Da.ep[1],FALSE)) {
					Da.trk[1] = NULL;								//Turntable Fail
					wBeep();
					InfoMessage(_("No Valid Track End there"));
					return C_CONTINUE;
				}
				if (ep<1) {
					Da.trk[1] = t;
					Da.pos[1] = pos;
				}
				CorrectHelixAngles();
				Da.selectEndPoint = 1;			//Select second end point
				Da.state = POINT_PICKED;
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack); //Wipe out initial Arm
				CreateBothEnds(1,-1);
				if (CallCornu(Da.pos,Da.trk,Da.ep,&Da.crvSegs_da, &cp))
						Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
				DrawTempCornu();
				InfoMessage( _("Locked - Move 2nd end point of Cornu track along track 2") );
			}
			return C_CONTINUE;
		} else  {					//This is after both ends exist
			return AdjustCornuCurve( action&0xFF, pos, InfoMessage );
		}
		return C_CONTINUE;


	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		EPINX_T ep = -1;
		if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT) {
			//Lock to endpoint if one is available and under pointer
			if ((t = OnTrack(&pos, FALSE, TRUE)) != NULL) {
				if (QueryTrack(t,Q_HAS_VARIABLE_ENDPOINTS)) {    //Circle/Helix find if there is an open slot and where
					if ((GetTrkEndTrk(t,0) != NULL) && (GetTrkEndTrk(t,1) != NULL)) {
						return C_CONTINUE;
					}
					ep = -1;                                            //Not a real ep yet
				} else ep = PickUnconnectedEndPointSilent(pos, t);		//EP
				if (ep>=0 && QueryTrack(t,Q_CAN_ADD_ENDPOINTS)) ep=-1;  		//Don't attach to Turntable
				if ( ep==-1 && (!QueryTrack(t,Q_CAN_ADD_ENDPOINTS) && !QueryTrack(t,Q_HAS_VARIABLE_ENDPOINTS))) {  //No endpoints and not Turntable or Helix/Circle
					return C_CONTINUE;
				}
				if (GetTrkScale(t) != (char)GetLayoutCurScale()) {
					return C_CONTINUE;
				}
			}
		}
		if (ep>=0) {
			pos = GetTrkEndPos(t,ep);
			CreateEndAnchor(pos,TRUE);
		}
		if (anchors_da.cnt)
			DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );

		return C_CONTINUE;
			
	case C_MOVE:
		if (Da.state == NONE) {    //First point not created
			if (Da.cmdType == cornuCmdCreateTrack) {
				InfoMessage("Place 1st end point of Cornu track");
			} else
				InfoMessage("Place 1st end point of Cornu track on unconnected end-point");
			return C_CONTINUE;
		}
		if (Da.state == POS_1) {	//First point has been created
			if (Da.cmdType == cornuCmdCreateTrack && !Da.trk[0]) {  //OK for CreateCornu -> No track selected
				Da.selectEndPoint = 0;
				Da.pos[0] = pos;			//Move end as dictated
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],TRUE,TRUE,TRUE);
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
				return C_CONTINUE;
			}
			EPINX_T ep = 0;
			BOOL_T found = FALSE;
			int end = Da.state==POS_1?0:1;
			if(!QueryTrack(Da.trk[0],Q_CORNU_CAN_MODIFY) && !QueryTrack(Da.trk[0],Q_CAN_ADD_ENDPOINTS)) {
				InfoMessage(_("Can't Split - So Locked to End Point"));
				return C_CONTINUE;
			}
			if (Da.trk[0] != OnTrack(&pos, FALSE, TRUE)) {
				wBeep();
				InfoMessage(_("Point not on track 1"));
				Da.state = POS_1;
				Da.selectEndPoint = 0;
				return C_CONTINUE;
			}
			t = Da.trk[0];
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
			if (!GetConnectedTrackParms(t, pos, ep, Da.ep[ep],FALSE)) {
				Da.state = POS_1;
				Da.selectEndPoint = 0;
				return C_CONTINUE;
			}
			Da.pos[ep] = pos;
			Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],TRUE,!QueryTrack(Da.trk[0],Q_IS_CORNU),QueryTrack(Da.trk[0],Q_CORNU_CAN_MODIFY));
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
		} else {	//Second Point Has Been Created
			return AdjustCornuCurve( action&0xFF, pos, InfoMessage );
		}
		return C_CONTINUE;

	case C_UP:
		if (Da.state == POS_1 && (Da.cmdType == cornuCmdCreateTrack || Da.trk[0])) {
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
			Da.state = LOC_2;
			Da.selectEndPoint = -1;
			if (Da.cmdType == cornuCmdCreateTrack) {
				InfoMessage( _("Pick other end of Cornu") );
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],FALSE,TRUE,TRUE);
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
				return C_CONTINUE;
			}
			InfoMessage( _("Put other end of Cornu on a track with an unconnected end point") );
			if (Da.trk[0])
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0], FALSE,!QueryTrack(Da.trk[0],Q_IS_CORNU),QueryTrack(Da.trk[0],Q_CORNU_CAN_MODIFY));
			else
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0], FALSE,TRUE,TRUE);
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,NULL,0,drawColorBlack);
			return C_CONTINUE;
		} else {
			return AdjustCornuCurve( action&0xFF, pos, InfoMessage );
		}
		return C_CONTINUE;
		break;
	case C_TEXT:
			if (Da.state != PICK_POINT)	return C_CONTINUE;
			if ((action>>8 == 127) || (action>>8 == 8))   //
				return AdjustCornuCurve(action, pos, InfoMessage);
			if (!(action>>8 == 32 )) //Space is same as Enter.
				return C_CONTINUE;
			/* no break */
    case C_OK:
    	if (Da.state != PICK_POINT) return C_CONTINUE;
        return AdjustCornuCurve( C_OK, pos, InfoMessage);

	case C_REDRAW:
		if ( Da.state != NONE ) {
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,Da.ep2Segs,Da.ep2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, NULL,
					Da.extend[0]?&Da.extendSeg[0]:NULL,Da.extend[1]?&Da.extendSeg[1]:NULL,(trkSeg_t *)Da.midSegs.ptr,Da.midSegs.cnt,Da.color);
		}
		if (anchors_da.cnt)
					DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_CANCEL:
		if (Da.state != NONE) {
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,Da.ep2Segs,Da.ep2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, NULL,
					Da.extend[0]?&Da.extendSeg[0]:NULL,Da.extend[1]?&Da.extendSeg[1]:NULL,(trkSeg_t *)Da.midSegs.ptr,Da.midSegs.cnt,Da.color);
			Da.ep1Segs_da_cnt = 0;
			Da.ep2Segs_da_cnt = 0;
			Da.crvSegs_da_cnt = 0;
			for (int i=0;i<2;i++) {
				Da.radius[i] = 0.0;
				Da.angle[i] = 0.0;
				Da.center[i] = zero;
				Da.trk[i] = NULL;
				Da.ep[i] = -1;
				Da.pos[i] = zero;
			}
			//DYNARR_FREE(trkSeg_t,Da.crvSegs_da);
		}
		Da.state = NONE;
		Da.cmdType = cornuCmdNone;
		return C_CONTINUE;
		
	default:

	return C_CONTINUE;
	}
	return C_CONTINUE;
}

BOOL_T GetTracksFromCornuTrack(track_p trk, track_p newTracks[2]) {
	trkSeg_t seg_temp;
	track_p trk_old = NULL;
	newTracks[0] = NULL, newTracks[1] = NULL;
	struct extraData * xx = GetTrkExtraData(trk);
	if (!IsTrack(trk)) return FALSE;
	for (int i=0; i<xx->cornuData.arcSegs.cnt;i++) {
		track_p bezTrack[2];
		bezTrack[0] = NULL, bezTrack[1] = NULL;
		trkSeg_p seg = &DYNARR_N(trkSeg_t,xx->cornuData.arcSegs,i);
		if (seg->type == SEG_BEZTRK) {
			DYNARR_RESET(trkSeg_t,seg->bezSegs);
			FixUpBezierSeg(seg->u.b.pos,seg,TRUE);
			GetTracksFromBezierSegment(seg,bezTrack);
			if (newTracks[0] == NULL) newTracks[0] = bezTrack[0];
			newTracks[1] = bezTrack[1];
			if (trk_old) {
				for (int i=0;i<2;i++) {
					if (GetTrkEndTrk(trk_old,i)==NULL) {
						coOrd pos = GetTrkEndPos(trk_old,i);
						EPINX_T ep_n = PickUnconnectedEndPoint(pos,bezTrack[0]);
						if (connectDistance >= FindDistance(GetTrkEndPos(trk_old,i),GetTrkEndPos(bezTrack[0],ep_n))) {
							ConnectTracks(trk_old,i,bezTrack[0],ep_n);
							break;
						}
					}
				}
			}
			trk_old = newTracks[1];
		} else {
			track_p new_trk;
			if (seg->type == SEG_CRVTRK)
				new_trk = NewCurvedTrack(seg->u.c.center,seg->u.c.radius,seg->u.c.a0,seg->u.c.a1,0);
			else if (seg->type == SEG_STRTRK)
				new_trk = NewStraightTrack(seg->u.l.pos[0],seg->u.l.pos[1]);
			if (newTracks[0] == NULL) newTracks[0] = new_trk;
			newTracks[1] = new_trk;
			if (trk_old) {
				for (int i=0;i<2;i++) {
					if (GetTrkEndTrk(trk_old,i)==NULL) {
						coOrd pos = GetTrkEndPos(trk_old,i);
						EPINX_T ep_n = PickUnconnectedEndPoint(pos,new_trk);
						if (connectDistance >= FindDistance(GetTrkEndPos(trk_old,i),GetTrkEndPos(new_trk,ep_n))) {
							ConnectTracks(trk_old,i,new_trk,ep_n);
							break;
						}
					}
				}
			}
			trk_old = new_trk;
		}
	}
	return TRUE;

}

static STATUS_T CmdConvertTo(
		wAction_t action,
		coOrd pos )
{
	track_p trk,trk1,trk2;
	cornuParm_t cp;
	switch (action) {

	case C_LCLICK:
			if ((trk = OnTrack(&pos,TRUE,TRUE))!=NULL) {
				SetTrkBits(trk,TB_SELECTED);
				selectedTrackCount = 1;
			} else {
				wBeep();
				InfoMessage( _("Not on a Track") );
				return C_CONTINUE;
			}

			/* no break */
	case C_START:
			if (selectedTrackCount==0) {
				InfoMessage( _("Select a Track To Convert") );
				return C_CONTINUE;
			}
			else if (selectedTrackCount>1) {
				if (NoticeMessage(_("Convert all Selected Tracks to Cornu Tracks?"), _("Yes"), _("No"))<=0) {
					SetAllTrackSelect(FALSE);
					return C_TERMINATE;
				}
			}
			UndoStart( _("Convert Cornu"),"newCornu curves");
			trk = NULL;
			DYNARR_RESET(track_p,Da.tracks);
			while ( TrackIterate( &trk ) ) {
				if (!GetTrkSelected( trk )) continue;		//Only selected
				if (!QueryTrack(trk, Q_CORNU_CAN_MODIFY) &&  //Not Fixed Track/Turnout/Turntable
					!QueryTrack( trk, Q_IGNORE_EASEMENT_ON_EXTEND )) { //But Yes to Easement
					continue;
				}
				DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
				Da.ep1Segs_da_cnt = 0;
				Da.ep2Segs_da_cnt = 0;
				Da.midSegs.cnt = 0;
				Da.extend[0] = FALSE;
				Da.extend[1] = FALSE;
				Da.selectEndPoint = -1;
				Da.selectTrack = NULL;
				DYNARR_RESET(coOrd,Da.mid_points);
				ClrTrkBits( trk, TB_SELECTED );          //Done with this one
				Da.selectTrack = trk;
				DYNARR_APPEND(track_p,Da.tracks,1);
				DYNARR_LAST(track_p,Da.tracks) = trk;
				Da.trk[0] = GetTrkEndTrk( trk, 0 );
				track_p prior = trk;
				if (Da.trk[0]) Da.ep[0] = GetEndPtConnectedToMe(Da.trk[0],trk);
				else Da.ep[0] = -1;
				EPINX_T ep0 = 0;
				//Move down the LHS adding tracks until no more Selected or not modifyable
				while (Da.trk[0] && GetTrkSelected( Da.trk[0]) && IsTrack(trk) && (QueryTrack(trk, Q_CORNU_CAN_MODIFY) || QueryTrack(trk, Q_IS_CORNU)) ) {
					prior = Da.trk[0];
					ep0 = 1-Da.ep[0];
					ClrTrkBits( Da.trk[0], TB_SELECTED );          //Done with this one
					if (selectedTrackCount>0) selectedTrackCount--;
					DYNARR_APPEND(track_p,Da.tracks,1);
					DYNARR_LAST(track_p,Da.tracks) = prior;
					DYNARR_APPEND(coOrd,Da.mid_points,1);
					for (int i=0;i<Da.mid_points.cnt-1;i++) {
						DYNARR_N(coOrd,Da.mid_points,i+1) = DYNARR_N(coOrd,Da.mid_points,i);
					}
					DYNARR_N(coOrd,Da.mid_points,0) = GetTrkEndPos(prior,1-ep0);
					Da.trk[0] = GetTrkEndTrk( prior, ep0 );
					if (Da.trk[0]) Da.ep[0] = GetEndPtConnectedToMe(Da.trk[0],prior);
					else Da.ep[0] = -1;
				}
				Da.radius[0] = -1.0; //Initialize with no end
				Da.ends[0] = FALSE;
				Da.center[0] = zero;
				Da.pos[0] = GetTrkEndPos(prior,ep0);
				if (Da.trk[0] && Da.ep[0]>=0) {
					GetConnectedTrackParms(Da.trk[0],GetTrkEndPos(Da.trk[0],Da.ep[0]),0,Da.ep[0],FALSE);
				}

				//Move to RHS

				Da.trk[1] = GetTrkEndTrk( trk, 1 );
				track_p next = trk;
				EPINX_T ep1 = 1;
				if (Da.trk[1]) Da.ep[1] = GetEndPtConnectedToMe(Da.trk[1],trk);
				else Da.ep[1] = -1;
				//Move down RHS adding tracks until no more Selected or not modifyable
				while (Da.trk[1] && GetTrkSelected( Da.trk[1]) && (QueryTrack(trk, Q_CORNU_CAN_MODIFY) || QueryTrack(trk, Q_IS_CORNU))) {
					next = Da.trk[1];
					ep1 = 1-Da.ep[1];
					if (selectedTrackCount>0) selectedTrackCount--;
					ClrTrkBits( Da.trk[1], TB_SELECTED );          //Done with this one
					DYNARR_APPEND(track_p,Da.tracks,1);
					DYNARR_LAST(track_p,Da.tracks) = next;
					DYNARR_APPEND(coOrd,Da.mid_points,1);
					DYNARR_LAST(coOrd,Da.mid_points) = GetTrkEndPos(next,1-ep1);
					Da.trk[1] = GetTrkEndTrk( next, ep1 );
					if (Da.trk[1]) Da.ep[1] = GetEndPtConnectedToMe(Da.trk[1],next);
				}
				Da.radius[1] = -1.0; //Initialize with no end
				Da.ends[1] = FALSE;
				Da.center[1] = zero;
				Da.pos[1] = GetTrkEndPos(next,ep1);
				if (Da.trk[1] && Da.ep[1]>=0) {
					GetConnectedTrackParms(Da.trk[1],GetTrkEndPos(Da.trk[1],Da.ep[1]),1,Da.ep[1],FALSE);
				}
				SetUpCornuParms(&cp);
				if (CallCornuM(Da.mid_points,Da.ends,Da.pos,&cp,&Da.crvSegs_da,TRUE)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
				else continue;   //Checks that a solution can be found

				// Do the deed - Create a replacement Cornu

				for (int i = 0; i<Da.tracks.cnt;i++) {
					DrawTrack(DYNARR_N(track_p,Da.tracks,i),&mainD,wDrawColorWhite);  //Wipe out real tracks, draw replacement
				}

				BOOL_T end_point[2];
				end_point[0] = TRUE;
				end_point[1] = FALSE;
				coOrd sub_pos[2];
				sub_pos[0] = Da.pos[0];
				if (Da.radius[0] == -1) end_point[0] = FALSE;
				track_p first_trk= NULL,trk1=NULL,trk2 = NULL;

				for (int i=0;i<Da.mid_points.cnt;i++) {
					sub_pos[1] = DYNARR_N(coOrd,Da.mid_points,i);
					if ((trk1 = CreateCornuFromPoints(sub_pos,end_point))== NULL) continue;
					if (Da.trk[0]) {
						CopyAttributes( Da.trk[0], trk1 );
					} else if (Da.trk[1]) {
						CopyAttributes( Da.trk[1], trk1 );
					} else {
						SetTrkScale( trk1, GetLayoutCurScale() );
						SetTrkBits( trk1, TB_HIDEDESC );
					}
					DrawNewTrack(trk1);
					if (first_trk == NULL) first_trk = trk1;
					if (trk2) ConnectTracks(trk1,0,trk2,1);
					trk2 = trk1;
					end_point[0] = FALSE;
					sub_pos[0] = DYNARR_N(coOrd,Da.mid_points,i);
				}
				sub_pos[1] = Da.pos[1];
				end_point[1] = TRUE;
				if (Da.radius[1] == -1) end_point[1] = FALSE;
				if ((trk1 = CreateCornuFromPoints(sub_pos,end_point)) == NULL) continue;
				DrawNewTrack(trk1);
				if (Da.trk[0]) {
					CopyAttributes( Da.trk[0], trk1 );
				} else if (Da.trk[1]){
					CopyAttributes( Da.trk[1], trk1 );
				} else {
					SetTrkScale( trk1, GetLayoutCurScale() );
					SetTrkBits( trk1, TB_HIDEDESC );
				}
				if (trk2) ConnectTracks(trk1,0,trk2,1);
				if (first_trk == NULL) first_trk = trk1;

				for (int i=0;i<2;i++) {
					if (Da.ep[i]>=0 && Da.trk[i]) {
						track_p trk_old = GetTrkEndTrk(Da.trk[i],Da.ep[i]);
						EPINX_T old_ep = GetEndPtConnectedToMe(trk_old,Da.trk[i]);
						DisconnectTracks(Da.trk[i],Da.ep[i],trk_old,old_ep);
						if (Da.ep[i]>=0 && Da.trk[i])
							ConnectTracks(Da.trk[i],Da.ep[i],i==0?first_trk:trk1,i);
					}
				}

			}			//Find next track
			//Get rid of old tracks
			for (int i = 0; i<Da.tracks.cnt;i++) {
				DeleteTrack(DYNARR_N(track_p,Da.tracks,i),FALSE);
			}

			SetAllTrackSelect(FALSE);
			UndoEnd();  //Stop accumulating

			return C_TERMINATE;

		case C_REDRAW:
			return C_CONTINUE;

		case C_CANCEL:
			return C_TERMINATE;

		case C_OK:
			return C_TERMINATE;

		case C_CONFIRM:
			return C_CONTINUE;

		default:
			return C_CONTINUE;
		}
}
static STATUS_T CmdConvertFrom(
		wAction_t action,
		coOrd pos )
{
	track_p trk,trk1,trk2;
	cornuParm_t cp;
	switch (action) {

		case C_LCLICK:
			if ((trk = OnTrack(&pos,TRUE,TRUE))!=NULL) {
				SetTrkBits(trk,TB_SELECTED);
				selectedTrackCount = 1;
			} else {
				wBeep();
				InfoMessage( _("Not on a Track") );
				return C_CONTINUE;
			}
			/* no break */
		case C_START:
			if (selectedTrackCount==0) {
				InfoMessage( _("Select a Cornu or Bezier Track To Convert to Fixed") );
				return C_CONTINUE;
			}
			else if (selectedTrackCount>1) {
				 if (NoticeMessage(_("Convert all Selected Tracks to Fixed Tracks?"), _("Yes"), _("No"))<=0) {
					SetAllTrackSelect(FALSE);
					return C_TERMINATE;
				}
			}
			dynArr_t trackSegs_da;
			DYNARR_RESET(trkSeg_t,trackSegs_da);
			trkSeg_t segPtr;
			trk1 = NULL;
			trk2 = NULL;
			UndoStart( _("Convert Bezier and Cornu"),"Try to convert all selected tracks");
			track_p tracks[2];
			DYNARR_RESET(track_p,Da.tracks);
			while ( TrackIterate( &trk1 ) ) {
				if ( GetTrkSelected( trk1 ) && IsTrack( trk1 ) ) {
					//Only Cornu or Bezier
					tracks[0] = NULL, tracks[1] = NULL;
					if (selectedTrackCount>0) selectedTrackCount--;
					ClrTrkBits( trk1, TB_SELECTED );          //Done with this one
					if (GetTrkType(trk1) == T_CORNU) {
						GetTracksFromCornuTrack(trk1,tracks);
						DYNARR_APPEND(track_p,Da.tracks,1);
						DYNARR_LAST(track_p,Da.tracks) = trk1;
					} else if (GetTrkType(trk1) == T_BEZIER) {
						GetTracksFromBezierTrack(trk1,tracks);
						DYNARR_APPEND(track_p,Da.tracks,1);
						DYNARR_LAST(track_p,Da.tracks) = trk1;
					} else continue;
					for (int i=0;i<2;i++) {
						track_p trk2 = GetTrkEndTrk(trk1,i);
						if (trk2) {
							EPINX_T ep1 = GetEndPtConnectedToMe( trk2, trk1 );
							DisconnectTracks(trk2,ep1,trk1,i);
							pos = GetTrkEndPos(trk2,ep1);
							for (int j=0;j<2;j++) {
								EPINX_T ep2 = PickEndPoint( pos, tracks[j] );
								coOrd ep_pos = GetTrkEndPos(tracks[j],ep2);
								if (connectDistance>=FindDistance(pos,ep_pos)) {
									ConnectTracks(trk2,ep1,tracks[j],ep2);
									break;
								}
							}
						}
					}
				}
			}
			for (int i = 0; i<Da.tracks.cnt;i++) {
				DeleteTrack(DYNARR_N(track_p,Da.tracks,i),FALSE);
			}
			SetAllTrackSelect(FALSE);
			UndoEnd();
			return C_TERMINATE;

		case C_REDRAW:
			return C_CONTINUE;

		case C_CANCEL:
			return C_TERMINATE;

		case C_OK:
			return C_TERMINATE;

		case C_CONFIRM:
			return C_CONTINUE;

		default:
			return C_CONTINUE;
		}
}

#include "bitmaps/convertto.xpm"
#include "bitmaps/convertfr.xpm"

EXPORT void InitCmdCornu( wMenu_p menu )
{	
	ButtonGroupBegin( _("Convert"), "cmdConvertSetCmd", _("Convert") );
	AddMenuButton( menu, CmdConvertTo, "cmdConvertTo", _("Convert To Cornu"), wIconCreatePixMap(convertto_xpm), LEVEL0_50, IC_STICKY|IC_LCLICK|IC_POPUP2,ACCL_CONVERTTO, NULL );
	AddMenuButton( menu, CmdConvertFrom, "cmdConvertFrom", _("Convert From Cornu"), wIconCreatePixMap(convertfr_xpm), LEVEL0_50, IC_STICKY|IC_LCLICK|IC_POPUP2,ACCL_CONVERTFR, NULL );
	ButtonGroupEnd();
}
