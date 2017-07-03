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
#include "cstraigh.h"
#include "drawgeom.h"
#include "cjoin.h"
#include "i18n.h"
#include "common.h"

extern drawCmd_t tempD;
extern TRKTYP_T T_BEZIER;


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
        int selectPoint;
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
		DIST_T maxRadiusChange;
		} Da;



/**
 * Draw a EndPoint.
 * A Cornu end Point has a filled circle surrounded by another circle for endpoint
 */
int createEndPoint(
                     trkSeg_t sp[],   //seg pointer for up to 2 trkSegs (ends and line)
                     coOrd pos0,     //end on curve
				     BOOL_T point_selected,
					 BOOL_T point_selectable
                      )
{
    DIST_T d, w;
    d = tempD.scale*0.25;
    w = tempD.scale/tempD.dpi; /*double width*/
    sp[0].u.c.center = pos0;
    sp[0].u.c.a0 = 0.0;
    sp[0].u.c.a1 = 360.0;
    sp[0].u.c.radius = d/2;
    sp[0].type = SEG_CRVLIN;
    sp[0].width = w;
    sp[0].color = (point_selected>=0)?drawColorRed:drawColorBlack;
    sp[1].u.c.center = pos0;
    sp[1].u.c.a0 = 0.0;
    sp[1].u.c.a1 = 360.0;
    sp[1].u.c.radius = d/4;
    if (point_selectable)
    	sp[1].type = SEG_FILCRCL;
    else
    	sp[1].type = SEG_CRVLIN;
    sp[1].width = w;
    sp[1].color = (point_selected>=0)?drawColorRed:drawColorBlack;
    return 2;
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
	s->bezSegs.ptr = NULL;
	s->bezSegs.cnt = 0;
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
EXPORT void SetKnots(spiro_cp knots[6], coOrd posk[6]) {
	for (int i = 0; i < 6; i++) {
		knots[i].x = posk[i].x;
		knots[i].y = posk[i].y;
	}
	knots[0].ty = SPIRO_OPEN_CONTOUR;
	knots[1].ty = SPIRO_G2;
	knots[2].ty = SPIRO_RIGHT;
	knots[3].ty = SPIRO_LEFT;
	knots[4].ty = SPIRO_G2;
	knots[5].ty = SPIRO_END_OPEN_CONTOUR;
}

BOOL_T CallCornu0(coOrd pos[2], coOrd center[2], ANGLE_T angle[2], DIST_T radius[2], dynArr_t * array_p, BOOL_T spots) {
	array_p->cnt = 0;
	//Create LH knots
	//Find remote end point of track, create start knot
	int ends[2];
	ends[0] = 2; ends[1] = 3;
	spiro_cp knots[6];
	coOrd posk[6];
	BOOL_T back;
	ANGLE_T angle1;

	bezctx * bezc = new_bezctx_xtrkcad(array_p,ends,spots);

	coOrd pos0 = pos[0];

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
	posk[2] = pos[0];

	posk[3] = pos[1];

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
	SetKnots(knots,posk);
	TaggedSpiroCPsToBezier(knots,bezc);
	if (!bezctx_xtrkcad_close(bezc)) {
		return FALSE;
	}
	return TRUE;
}


BOOL_T CallCornu(coOrd pos[2], track_p trk[2], EPINX_T ep[2], dynArr_t * array_p, cornuParm_t * cp) {

	trackParams_t params;
	BOOL_T ccw0, ccw1;
	ANGLE_T angle0, angle1;

	GetTrackParams(PARAMS_CORNU,trk[0],pos[0],&params);
	coOrd pos0 = pos[0];
	cp->pos[0] = pos0;
	angle0 = GetTrkEndAngle(trk[0],ep[0]);
	if (params.type == curveTypeStraight ) {
		cp->angle[0] = NormalizeAngle(angle0+180);
		cp->radius[0] = 0.0;
	} else if (params.type == curveTypeCurve ){
		if(params.arcA0>90 && params.arcA0<270) ccw0 = TRUE;
		else ccw0 = FALSE;
		cp->angle[0] = NormalizeAngle(FindAngle(params.arcP,pos[0]) + (ep[0]?-90:90));
		cp->radius[0] = params.arcR;
		cp->center[0] = params.arcP;
	} else {
		cp->angle[0] = NormalizeAngle(angle0+180);
		cp->radius[0] = params.arcR;
		cp->center[0] = params.arcP;
	}
	GetTrackParams(PARAMS_CORNU,trk[1],pos[1],&params);
	coOrd pos1 = pos[1];
	cp->pos[1] = pos1;
	angle1 = GetTrkEndAngle(trk[1],Da.ep[1]);
	if (params.type == curveTypeStraight ) {
		cp->angle[1] = NormalizeAngle(angle1+180);
		cp->radius[1] = 0.0;
	} else if (params.type == curveTypeCurve) {
		if(params.arcA0>90 && params.arcA0<270) ccw1 = TRUE;
		else ccw1 = FALSE;
		cp->angle[1] = NormalizeAngle(FindAngle(params.arcP,pos[1]) + (ep[1]?-90:90));
		cp->radius[1] = params.arcR;
		cp->center[1] = params.arcP;
	} else {
		cp->angle[1] = NormalizeAngle(angle1+180);
		cp->radius[1] = params.arcR;
		cp->center[1] = params.arcP;
	}

	return CallCornu0(pos,cp->center,cp->angle,cp->radius,array_p,TRUE);
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
							wDrawColor color
					) {
	long oldDrawOptions = tempD.funcs->options;
	tempD.funcs->options = wDrawOptTemp;
	long oldOptions = tempD.options;
	tempD.options = DC_TICKS;
	tempD.orig = mainD.orig;
	tempD.angle = mainD.angle;
	if (first_trk)
		DrawSegs( &tempD, zero, 0.0, first_trk, 1, trackGauge, drawColorBlack );
	if (crvSegs_cnt && curveSegs)
		DrawSegs( &tempD, zero, 0.0, curveSegs, crvSegs_cnt, trackGauge, color );
	if (second_trk)
		DrawSegs( &tempD, zero, 0.0, second_trk, 1, trackGauge, drawColorBlack );
	if (ep1Segs_cnt && point1)
		DrawSegs( &tempD, zero, 0.0, point1, ep1Segs_cnt, trackGauge, drawColorBlack );
	if (ep2Segs_cnt && point2)
		DrawSegs( &tempD, zero, 0.0, point2, ep2Segs_cnt, trackGauge, drawColorBlack );
	if (extend1_trk)
		DrawSegs( &tempD, zero, 0.0, extend1_trk, 1, trackGauge, drawColorBlack);
	if (extend2_trk)
		DrawSegs( &tempD, zero, 0.0, extend2_trk, 1, trackGauge, drawColorBlack);
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
				  Da.minRadius<minTrackRadius?drawColorRed:drawColorBlack);

}

void CreateBothEnds(int selectPoint) {
	BOOL_T selectable[2];
	selectable[0] = !QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT);
	selectable[1] = !QueryTrack(Da.trk[1],Q_MODIFY_CANT_SPLIT);
	if (selectPoint == -1) {
		Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],FALSE,selectable[0]);
		Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs, Da.pos[1],FALSE,selectable[1]);
	} else if (selectPoint == 0 || selectPoint == 1) {
		Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],selectPoint == 0,selectable[0]);
		Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs, Da.pos[1],selectPoint == 1,selectable[1]);
	} else {
		Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],FALSE,selectable[0]);
		Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs, Da.pos[1],FALSE,selectable[1]);
	}
}

void GetConnectedTrackParms(track_p t, const coOrd pos, int end, EPINX_T track_end) {
	trackParams_t trackParams;
	GetTrackParams(PARAMS_CORNU, t, pos, &trackParams);
	Da.radius[end] = 0.0;
	Da.center[end] = zero;
	Da.trackType[end] = trackParams.type;
	if (trackParams.type == curveTypeCurve) {
		Da.arcA0[end] = trackParams.arcA0;
		Da.arcA1[end] = trackParams.arcA1;
		Da.radius[end] = trackParams.arcR;
		Da.center[end] = trackParams.arcP;
		ANGLE_T angle1 = FindAngle(trackParams.arcP,pos);
		Da.angle[end] = NormalizeAngle(angle1+(track_end?-90:90));
						//Ignore params.angle which is always left to right
	} else if (trackParams.type == curveTypeBezier) {
		Da.angle[end] = trackParams.angle;
		if (trackParams.arcR == 0) {
			Da.radius[end] = 0;
			Da.angle[end] = trackParams.angle;
			Da.center[end] = zero;
		} else {
			Da.arcA0[end] = trackParams.arcA0;
			Da.arcA1[end] = trackParams.arcA1;
			Da.radius[end] = trackParams.arcR;
			Da.center[end] = trackParams.arcP;
		}
	} else if (trackParams.type == curveTypeCornu) {
			Da.radius[end] = trackParams.cornuRadius[end];
			Da.angle[end] = trackParams.cornuAngle[end];
			Da.center[end] = trackParams.cornuCenter[end];
	}
	else if (trackParams.type == curveTypeStraight) {
		Da.angle[end] = NormalizeAngle(GetTrkEndAngle(t,track_end)+180);  //Ignore params.angle because it gives from nearest end
	}

}

/*
 * AdjustCornuCurve
 *
 * Called to adjust the curve either when creating it or modifying it
 * States are "PICK_POINT" and "POINT_PICKED" and "TRACK_SELECTED".
 *
 * In PICK_POINT, the user can select an end-point to drag and release in POINT_PICKED. They can also
 * hit Enter (which saves the changes) or ESC (which cancels them).
 *
 * Only those points which can be picked are shown with circles.
 *
 */
EXPORT STATUS_T AdjustCornuCurve(
		wAction_t action,
		coOrd pos,
		cornuMessageProc message )
{
	track_p t;
	DIST_T d;
	ANGLE_T a;
	static coOrd pos0, pos3, p;
	DIST_T dd;
	EPINX_T ep;
	int controlArm = -1;
	cornuParm_t cp;


	if (Da.state != PICK_POINT && Da.state != POINT_PICKED && Da.state != TRACK_SELECTED) return C_CONTINUE;

	switch ( action & 0xFF) {

	case C_START:
			Da.selectPoint = -1;
			Da.extend[0] = FALSE;
			Da.extend[1] = FALSE;
			CreateBothEnds(Da.selectPoint);
			Da.crvSegs_da.max = 0;
			Da.crvSegs_da.cnt = 0;
			if (CallCornu(Da.pos,Da.trk,Da.ep,&Da.crvSegs_da,&cp)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
			else Da.crvSegs_da_cnt = 0;
			Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
			InfoMessage( _("Select End-Point") );
			DrawTempCornu();
			return C_CONTINUE;

	case C_DOWN:
		if (Da.state != PICK_POINT) return C_CONTINUE;
		dd = 10000.0;
		Da.selectPoint = -1;
		for (int i=0;i<2;i++) {
			d = FindDistance(Da.pos[i],pos);
			if (d < dd) {
				dd = d;
				Da.selectPoint = i;
			}
		}
		if (!IsClose(dd) )Da.selectPoint = -1;
		if (Da.selectPoint == -1) {
			wBeep();
			InfoMessage( _("Not close enough to end point, reselect") );
			return C_CONTINUE;
		} else {
			pos = Da.pos[Da.selectPoint];
			Da.state = POINT_PICKED;
			InfoMessage( _("Drag point %d to new location and release it"),Da.selectPoint+1 );
		}
		DrawTempCornu();   //wipe out
		CreateBothEnds(Da.selectPoint);
		if (CallCornu(Da.pos, Da.trk,Da.ep, &Da.crvSegs_da, &cp)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		else Da.crvSegs_da_cnt = 0;
		Da.minRadius = CornuMinRadius(Da.pos, Da.crvSegs_da);
		DrawTempCornu();
		return C_CONTINUE;

	case C_MOVE:
		if (Da.state != POINT_PICKED) {
			InfoMessage(_("Pick any circle to adjust it by dragging - Enter to accept, Esc to cancel"));
			return C_CONTINUE;
		}
		//If locked, reset pos to be on line from other track

		int sel = Da.selectPoint;
		coOrd pos2 = pos;
		BOOL_T inside = FALSE;
		if (OnTrack(&pos, FALSE, TRUE) == Da.trk[sel]) {
			inside = TRUE;
			if (QueryTrack(Da.trk[Da.selectPoint],Q_MODIFY_CANT_SPLIT)) {
				InfoMessage(_("Track can't be split"));
				pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
			}
		} else {
			pos = pos2;
		}
		DIST_T ab = FindDistance(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]));
		DIST_T ac = FindDistance(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),pos);
		DIST_T cb = FindDistance(GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]), pos);
		if (ac>(ab-minLength) && cb<ac) {
				wBeep();
				InfoMessage(_("Too close to other end of selected Track"));
				return C_CONTINUE;
		}
		DrawTempCornu();   //wipe out
		Da.extend[sel] = FALSE;
		if (inside) Da.pos[Da.selectPoint] = pos;
		GetConnectedTrackParms(Da.trk[sel],pos,sel,Da.ep[sel]);
		if (!inside && Da.trackType[sel] == curveTypeStraight) {    //Extend with a temp seg
			Da.extendSeg[sel].type = SEG_STRTRK;
			Da.extendSeg[sel].width = 0;
			Da.extendSeg[sel].color = wDrawColorBlack;
			Da.extendSeg[sel].u.l.pos[0] = GetTrkEndPos( Da.trk[sel], Da.ep[sel] );
			Da.extend[sel] = TRUE;
			d = FindDistance( Da.extendSeg[sel].u.l.pos[0], pos );
			a = NormalizeAngle(Da.angle[sel]-FindAngle(pos,GetTrkEndPos(Da.trk[sel],Da.ep[sel])));
			Translate( &Da.extendSeg[sel].u.l.pos[1],
						Da.extendSeg[sel].u.l.pos[0],
						Da.angle[sel], -d * cos(D2R(a)));
			Da.pos[sel] = pos = Da.extendSeg[sel].u.l.pos[1];
		} else if (!inside && Da.trackType[sel] == curveTypeCurve) {       //Extend with temp seg
			Da.extendSeg[sel].type = SEG_CRVTRK;
			Da.extendSeg[sel].width = 0;
			Da.extendSeg[sel].color = wDrawColorBlack;
			Da.extendSeg[sel].u.c.center = Da.center[sel];
			Da.extendSeg[sel].u.c.radius = Da.radius[sel];
			a = FindAngle( Da.center[sel], pos );
			PointOnCircle( &pos, Da.center[sel], Da.radius[sel], a );
			if (Da.ep[sel]!=0) {
				Da.extendSeg[sel].u.c.a0 = FindAngle(Da.center[sel],GetTrkEndPos(Da.trk[sel],Da.ep[sel]));
				Da.extendSeg[sel].u.c.a1 = NormalizeAngle(a-Da.extendSeg[sel].u.c.a0);
			} else {
				Da.extendSeg[sel].u.c.a0 = a;
				Da.extendSeg[sel].u.c.a1 =
						NormalizeAngle(FindAngle(Da.center[sel],GetTrkEndPos(Da.trk[sel],Da.ep[sel]))-a);
			}
			if (Da.extendSeg[sel].u.c.a1 == 0.0 ||
					(Da.extendSeg[sel].u.c.a0 >= Da.arcA0[sel] &&
					Da.extendSeg[sel].u.c.a0 <= Da.arcA0[sel] + Da.arcA1[sel])) {
				Da.extend[sel] = FALSE;
			} else {
				Da.extend[sel] = TRUE;
				Da.pos[sel] = pos;
			}
		} else if (!inside) {
			DrawTempCornu();   //put back
			wBeep();
			InfoMessage(_("Must be on the %s Track"),Da.trackType[sel]==curveTypeBezier?"Bezier":"Unknown Type");
			pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
			return C_CONTINUE;
		}
		CreateBothEnds(Da.selectPoint);
		if (CallCornu(Da.pos, Da.trk, Da.ep, &Da.crvSegs_da,&cp)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		else Da.crvSegs_da_cnt = 0;
		Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
		DIST_T rin = Da.radius[0];
		InfoMessage( _("Cornu : Min Radius=%s Max Rate of Radius Change=%s Length=%s Winding Arc=%s"),
									FormatDistance(Da.minRadius),
									FormatDistance(CornuMaxRateofChangeofCurvature(Da.pos,Da.crvSegs_da,&rin)),
									FormatDistance(CornuLength(Da.pos,Da.crvSegs_da)),
									FormatDistance(CornuTotalWindingArc(Da.pos,Da.crvSegs_da)));
		DrawTempCornu();
		return C_CONTINUE;

	case C_UP:
		if (Da.state != POINT_PICKED) return C_CONTINUE;
		ep = 0;
		BOOL_T found = FALSE;
		DrawTempCornu();  //wipe out
		Da.selectPoint = -1;
		CreateBothEnds(Da.selectPoint);
		if (CallCornu(Da.pos,Da.trk,Da.ep,&Da.crvSegs_da,&cp)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		else Da.crvSegs_da_cnt = 0;
		Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
		InfoMessage(_("Pick on point to adjust it along track - Enter to confirm, ESC to abort"));
		DrawTempCornu();
		Da.state = PICK_POINT;
		return C_CONTINUE;

	case C_OK:                            //C_OK is not called by Modify.
		if ( Da.state == PICK_POINT ) {
			char c = (unsigned char)(action >> 8);
			Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
			if (CornuTotalWindingArc(Da.pos,Da.crvSegs_da)>4*360) {
				wBeep();
				InfoMessage(_("Cornu has too complex shape - adjust end pts"));
				return C_CONTINUE;
			}
			DrawTempCornu();
			UndoStart( _("Create Cornu"),"newCornu curve");
			t = NewCornuTrack( Da.pos, Da.center, Da.angle, Da.radius,(trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);
			if (t==NULL) {
				wBeep();
				InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
						Da.pos[0].x,Da.pos[0].y,
						Da.pos[1].x,Da.pos[1].y,
						Da.center[0].x,Da.center[0].y,
						Da.center[1].x,Da.center[1].y,
						Da.angle[0],Da.angle[1],
						FormatDistance(Da.radius[0]),FormatDistance(Da.radius[1]));
				return C_TERMINATE;
			}

			for (int i=0;i<2;i++) {
				UndoModify(Da.trk[i]);
				MoveEndPt(&Da.trk[i],&Da.ep[i],Da.pos[i],0);
				if (GetTrkType(Da.trk[i])==T_BEZIER) {          //Bezier split position not precise, so readjust Cornu
					GetConnectedTrackParms(Da.trk[i],GetTrkEndPos(Da.trk[i],Da.ep[i]),i,Da.ep[i]);
					ANGLE_T endAngle = NormalizeAngle(GetTrkEndAngle(Da.trk[i],Da.ep[i])+180);
					SetCornuEndPt(t,i,GetTrkEndPos(Da.trk[i],Da.ep[i]),Da.center[i],endAngle,Da.radius[i]);
				}
				ConnectTracks(Da.trk[i],Da.ep[i],t,i);
			}
			UndoEnd();
			DrawNewTrack(t);
			Da.state = NONE;
			MainRedraw();
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

struct extraData {
				cornuData_t cornuData;
		};

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
STATUS_T CmdCornuModify (track_p trk, wAction_t action, coOrd pos) {
	BOOL_T track = TRUE;
	track_p t;
	double width = 1.0;
	long mode = 0;
	long cmd;

	struct extraData *xx = GetTrkExtraData(trk);
	cmd = (long)commandContext;


	switch (action&0xFF) {
	case C_START:
		Da.state = NONE;
		DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
		Da.ep1Segs_da_cnt = 0;
		Da.ep2Segs_da_cnt = 0;
		Da.selectPoint = -1;
		Da.selectTrack = NULL;


		Da.selectTrack = trk;
	    Da.trk[0] = GetTrkEndTrk( trk, 0 );
		if (Da.trk[0]) Da.ep[0] = GetEndPtConnectedToMe(Da.trk[0],trk);
		Da.trk[1] = GetTrkEndTrk( trk, 1 );
		if (Da.trk[1]) Da.ep[1] = GetEndPtConnectedToMe(Da.trk[1],trk);

	    for (int i=0;i<2;i++) {
	    	Da.pos[i] = xx->cornuData.pos[i];              //Copy parms from old trk
	    	Da.radius[i] = xx->cornuData.r[i];
	    	Da.angle[i] = xx->cornuData.a[i];
	    	Da.center[i] = xx->cornuData.c[i];
	    }

	    //TODO stop if both ends are un-adjustable (Turnouts)
	    if (QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT) && !QueryTrack(Da.trk[0],Q_CAN_EXTEND) &&
	    		QueryTrack(Da.trk[1],Q_MODIFY_CANT_SPLIT) && !QueryTrack(Da.trk[1],Q_CAN_EXTEND)) {
	    	wBeep();
	    	ErrorMessage("Neither End of this Cornu is Adjustable");
	    	return C_TERMINATE;
	    }

		InfoMessage(_("Track picked - now select a Point"));
		Da.state = TRACK_SELECTED;
		DrawTrack(Da.selectTrack,&mainD,wDrawColorWhite);                    //Wipe out real track, draw replacement
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
		if ((action>>8) != 32)
			return C_CONTINUE;
		/* no break */
	case C_OK:
		if (Da.state != PICK_POINT) {										//Too early - abandon
			InfoMessage(_("No changes made"));
			Da.state = NONE;
			return C_CANCEL;
		}
		UndoStart( _("Modify Cornu"), "newCornu - CR" );
		t = NewCornuTrack( Da.pos, Da.center, Da.angle, Da.radius, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);
		if (t==NULL) {
			wBeep();
			InfoMessage(_("Cornu Create Failed for p1[%0.3f,%0.3f] p2[%0.3f,%0.3f], c1[%0.3f,%0.3f] c2[%0.3f,%0.3f], a1=%0.3f a2=%0.3f, r1=%s r2=%s"),
								Da.pos[0].x,Da.pos[0].y,
								Da.pos[1].x,Da.pos[1].y,
								Da.center[0].x,Da.center[0].y,
								Da.center[1].x,Da.center[1].y,
								Da.angle[0],Da.angle[1],
								FormatDistance(Da.radius[0]),FormatDistance(Da.radius[1]));
			UndoUndo();
			MainRedraw();
			return C_TERMINATE;
		}

		DeleteTrack(trk, TRUE);

		UndoModify(Da.trk[0]);
		UndoModify(Da.trk[1]);

		for (int i=0;i<2;i++) {										//Attach new track
			if (Da.trk[i] != NULL && Da.ep[i] != -1) {								//Like the old track
				MoveEndPt(&Da.trk[i],&Da.ep[i],Da.pos[i],0);
				ConnectTracks(t,i,Da.trk[i],Da.ep[i]);
			}
		}
		UndoEnd();
		MainRedraw();
		Da.state = NONE;
		return C_TERMINATE;

	case C_CANCEL:
		InfoMessage(_("Modify Cornu Cancelled"));
		Da.state = NONE;
		MainRedraw();
		return C_TERMINATE;

	case C_REDRAW:
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

DIST_T CornuMinRadius(coOrd pos[4],dynArr_t segs) {
	DIST_T r = 100000.0, rr;
	if (segs.cnt == 0 ) return r;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr = t.u.c.radius;
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
	DIST_T r_max = 0.0, rc, lc, last_l = 0;
	lc = * last_c;
	segProcData_t segProcData;
	if (segs.cnt == 0 ) return r_max;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_FILCRCL) continue;
		SegProc(SEGPROC_LENGTH,&t,&segProcData);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rc = fabs((1/t.u.c.radius)- lc)/segProcData.length.length;
			lc = 1/t.u.c.radius;
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rc = CornuMaxRateofChangeofCurvature(t.u.b.pos, t.bezSegs,&lc);  //recurse
		} else {
			rc = fabs(0-lc)/segProcData.length.length;
			lc = 0;
		}
		if (rc>r_max) r_max = rc;
	}
	* last_c = lc;
	return r_max;
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
	static int segCnt;
	STATUS_T rc = C_CONTINUE;
	long curveMode = 0;
	long cmd;
	cornuParm_t cp;
	if (action>>8) {
		cmd = action>>8;
	} else cmd = (long)commandContext;


	Da.color = lineColor;
	Da.width = (double)lineWidth/mainD.dpi;

	switch (action&0xFF) {

	case C_START:
		Da.state = NONE;
		Da. selectPoint = -1;
		for (int i=0;i<4;i++) {
			Da.pos[i] = zero;
		}
		Da.trk[0] = Da.trk[1] = NULL;
		//tempD.orig = mainD.orig;

		DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
		Da.ep1Segs_da_cnt = 0;
		Da.ep2Segs_da_cnt = 0;
		InfoMessage( _("Place 1st end point of Cornu track on unconnected end-point") );
		return C_CONTINUE;

	case C_DOWN:
		if ( Da.state == NONE || Da.state == LOC_2) {   //Set the first or second point
			coOrd p = pos;
			BOOL_T found = FALSE;
			int end = Da.state==NONE?0:1;
			EPINX_T ep;
		    if ((t = OnTrack(&p, TRUE, TRUE)) != NULL) {
				ep = PickUnconnectedEndPointSilent(p, t);
				if (ep==-1 || FindDistance(p,GetTrkEndPos(t,ep))>2) {
					wBeep();
					InfoMessage(_("No Unconnected end point there"));
					return C_CONTINUE;
				}
				Da.trk[end] = t;
				Da.ep[end] = ep;
				pos = GetTrkEndPos(t,ep);
				Da.pos[end] = pos;
				found = TRUE;
				InfoMessage( _("Place 2nd end point of Cornu track on unconnected end-point") );
			} else {
				wBeep();
				InfoMessage(_("No Unconnected Track End there"));
				return C_CONTINUE;
			}
			if (Da.state == NONE) {
				GetConnectedTrackParms(t, pos, end, Da.ep[end]);
				Da.state = POS_1;
				Da.selectPoint = 0;
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[end], TRUE,!QueryTrack(t,Q_MODIFY_CANT_SPLIT));
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
				InfoMessage( _("Move 1st end point of Cornu track along track 1") );
			} else {
				if ( Da.trk[0] == t) {
				   ErrorMessage( MSG_JOIN_SAME );
				   Da.trk[1] = NULL;

				   return C_CONTINUE;
				}
				GetConnectedTrackParms(t, pos, 1, Da.ep[1]);
				Da.selectPoint = 1;
				Da.state = POINT_PICKED;
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack); //Wipe out initial Arm
				CreateBothEnds(1);
				if (CallCornu(Da.pos,Da.trk,Da.ep,&Da.crvSegs_da, &cp))
						Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
				DrawTempCornu();
				InfoMessage( _("Move 2nd end point of Cornu track along track 2") );
			}
			return C_CONTINUE;
		} else  {
			return AdjustCornuCurve( action&0xFF, pos, InfoMessage );
		}
		return C_CONTINUE;
			
	case C_MOVE:
		if (Da.state == NONE) {
			InfoMessage("Place 1st end point of Cornu track on unconnected end-point");
			return C_CONTINUE;
		}
		if (Da.state == POS_1) {
			EPINX_T ep = 0;
			BOOL_T found = FALSE;
			int end = Da.state==POS_1?0:1;
			if(QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT)) {
				InfoMessage(_("Can't Split - Locked to End Point"));
				return C_CONTINUE;
			}
			if ((t = OnTrack(&pos, FALSE, TRUE)) != Da.trk[0]) {
				wBeep();
				InfoMessage(_("Point not on track 1"));
				Da.state = POS_1;
				Da.selectPoint = 1;
				return C_CONTINUE;
			}
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
			Da.pos[ep] = pos;
			GetConnectedTrackParms(t, pos, ep, Da.ep[ep]);
			Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0],TRUE,!QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT));
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
		} else {
			return AdjustCornuCurve( action&0xFF, pos, InfoMessage );
		}
		return C_CONTINUE;

	case C_UP:
		if (Da.state == POS_1 && Da.trk[0]) {
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
			Da.state = LOC_2;
			InfoMessage( _("Put other end of Cornu on an unconnected end point") );
			Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[0], FALSE,!QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT));
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
			return C_CONTINUE;
		} else {
			return AdjustCornuCurve( action&0xFF, pos, InfoMessage );
		}
	case C_TEXT:
			if (Da.state != PICK_POINT || (action>>8) != 32)  //Space is same as Enter.
				return C_CONTINUE;
			/* no break */
    case C_OK:
    	if (Da.state != PICK_POINT) return C_CONTINUE;
        return AdjustCornuCurve( C_OK, pos, InfoMessage);

	case C_REDRAW:
		if ( Da.state != NONE ) {
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,Da.ep2Segs,Da.ep2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, NULL, &Da.extendSeg[0],&Da.extendSeg[1],Da.color);
		}
		return C_CONTINUE;

	case C_CANCEL:
		if (Da.state != NONE) {
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,Da.ep2Segs,Da.ep2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, NULL, &Da.extendSeg[0],&Da.extendSeg[1],Da.color);
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
		}
		Da.state = NONE;
		return C_CONTINUE;
		
	default:

	return C_CONTINUE;
	}

}


EXPORT void InitCmdCornu( wMenu_p menu )
{	

}
