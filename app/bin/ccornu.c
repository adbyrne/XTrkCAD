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
#include "utility.h"
#include "math.h"
#include "param.h"
#include "layout.h"
#include "cundo.h"
#include "messages.h"

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

/*
 * Set up the call to Cornu0. Take the conditions of the two ends from the connected tracks.
 */
BOOL_T CallCornu(coOrd pos[2], track_p trk[2], EPINX_T ep[2], dynArr_t * array_p, cornuParm_t * cp) {

	trackParams_t params;
	BOOL_T ccw;
	ANGLE_T angle;
	for (int i=0;i<2;i++) {
		if (trk[i]) {
			if (!GetTrackParams(PARAMS_CORNU,trk[i],pos[i],&params)) return FALSE;
			cp->pos[i] = pos[i];
			if (Da.ep[i]>=0) angle = GetTrkEndAngle(trk[i],ep[i]);
			else angle = params.angle;         							//Turntable only
			if (params.type == curveTypeStraight) {
				cp->angle[i] = NormalizeAngle(angle+180);       //Because end always backwards
				cp->radius[i] = 0.0;
			} else if ((params.type == curveTypeCornu || params.type == curveTypeBezier) && params.arcR == 0.0 ) {
				cp->radius[i] = 0.0;
				cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));  //Use point not end
			} else if (params.type == curveTypeCurve) {
				cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
			} else if ((params.type == curveTypeCornu || params.type == curveTypeBezier) && params.arcR != 0.0 ){
				cp->angle[i] = NormalizeAngle(params.track_angle+(ep[i]?180:0));
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
			} else {
				cp->angle[i] = NormalizeAngle(angle+180);             //Unknown - treat like straight
				cp->radius[i] = params.arcR;
				cp->center[i] = params.arcP;
			}
		}
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
				  Da.minRadius<GetLayoutMinTrackRadius()?drawColorRed:drawColorBlack);

}

void CreateBothEnds(int selectPoint) {
	BOOL_T selectable[2];
	selectable[0] = !Da.trk[0] || !QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT);
	selectable[1] = !Da.trk[1] || !QueryTrack(Da.trk[1],Q_MODIFY_CANT_SPLIT);
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

BOOL_T GetConnectedTrackParms(track_p t, const coOrd pos, int end, EPINX_T track_end) {
	trackParams_t trackParams;
	if (!GetTrackParams(PARAMS_CORNU, t, pos, &trackParams)) return FALSE;
	Da.radius[end] = 0.0;
	Da.center[end] = zero;
	Da.trackType[end] = trackParams.type;
	if (trackParams.type == curveTypeCurve) {
		Da.arcA0[end] = trackParams.arcA0;
		Da.arcA1[end] = trackParams.arcA1;
		Da.radius[end] = trackParams.arcR;
		Da.center[end] = trackParams.arcP;
		Da.angle[end] = NormalizeAngle(trackParams.track_angle + (track_end?180:0));
	} else if (trackParams.type == curveTypeBezier || trackParams.type == curveTypeCornu) {
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

void SetUpCornuParms(cornuParm_t * cp) {
		cp->center[0] = Da.center[0];
		cp->angle[0] = Da.angle[0];
		cp->radius[0] = Da.radius[0];
		cp->center[1] = Da.center[1];
		cp->angle[1] = Da.angle[1];
		cp->radius[1] = Da.radius[1];
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
			SetUpCornuParms(&cp);
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
		if (!IsClose(dd) ) Da.selectPoint = -1;
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
		SetUpCornuParms(&cp);
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
		if (Da.trk[sel]) {										//There is a track
			if (OnTrack(&pos,FALSE,TRUE) == Da.trk[sel]) {		//And the pos is on it
				inside = TRUE;
				if (QueryTrack(Da.trk[Da.selectPoint],Q_MODIFY_CANT_SPLIT)) {  //Turnouts
					InfoMessage(_("Track can't be split"));
					if (Da.ep[sel]>=0)											//Ignore if turntable
						pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
				}
			} else {
				pos = pos2;				//Put Back to original position as outside track
			}
			if (Da.ep[sel]>=0 && (!QueryTrack(Da.trk[sel],Q_CAN_ADD_ENDPOINTS)
					|| !QueryTrack(Da.trk[sel],Q_MODIFY_CANT_SPLIT))) { //Not Turntable or Turnout
				DIST_T ab = FindDistance(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]));
				DIST_T ac = FindDistance(GetTrkEndPos(Da.trk[sel],Da.ep[sel]),pos);
				DIST_T cb = FindDistance(GetTrkEndPos(Da.trk[sel],1-Da.ep[sel]), pos);
				if (ac>(ab-minLength) && cb<ac) {
					InfoMessage(_("Too close to other end of selected Track"));
					return C_CONTINUE;
				}
			}
		}
		DrawTempCornu();   //wipe out old
		Da.extend[sel] = FALSE;
		if(!Da.trk[sel]) {							//Cornu with no ends
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
					pos = Da.extendSeg[sel].u.l.pos[1-sel];
					Da.extend[sel] = TRUE;
				}
			} else {								//Curve
				Da.extendSeg[sel].type = SEG_CRVTRK;
				Da.extendSeg[sel].width = 0;
				Da.extendSeg[sel].color = wDrawColorBlack;
				Da.extendSeg[sel].u.c.center = Da.center[sel];
				Da.extendSeg[sel].u.c.radius = Da.radius[sel];
				a = FindAngle( Da.center[sel], pos );
			    PointOnCircle( &pos, Da.center[sel], Da.radius[sel], a );
			    a2 = FindAngle(Da.center[sel],Da.pos[sel]);
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
		} else {									//Cornu with ends
			if (inside) Da.pos[sel] = pos;
			if (!GetConnectedTrackParms(Da.trk[sel],pos,sel,Da.ep[sel])) {
				DrawTempCornu();
				return C_CONTINUE;											//Stop drawing
			}
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
					if (((QueryTrack(Da.trk[sel],Q_CAN_ADD_ENDPOINTS)  && Da.ep[sel]>=0)
							|| QueryTrack(Da.trk[sel],Q_MODIFY_CANT_SPLIT))
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

				} else {								//Bezier and Cornu that we are joining TO can't extend
					DrawTempCornu();   //put back
					wBeep();
					InfoMessage(_("Must be on the %s Track"),Da.trackType[sel]==curveTypeBezier?"Bezier":Da.trackType[sel]==curveTypeCornu?"Cornu":"Unknown Type");
					pos = GetTrkEndPos(Da.trk[sel],Da.ep[sel]);
					return C_CONTINUE;
				}
			}
		}

		CreateBothEnds(Da.selectPoint);
		SetUpCornuParms(&cp);    //In case we want to use these because the ends are not on the track

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
		SetUpCornuParms(&cp);
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

			CopyAttributes( Da.trk[0], t );

			for (int i=0;i<2;i++) {
				UndoModify(Da.trk[i]);
				MoveEndPt(&Da.trk[i],&Da.ep[i],Da.pos[i],0);
				if (GetTrkType(Da.trk[i])==T_BEZIER || GetTrkType(Da.trk[i])==T_CORNU) {     //Bezier split position not precise, so readjust Cornu
					GetConnectedTrackParms(Da.trk[i],GetTrkEndPos(Da.trk[i],Da.ep[i]),i,Da.ep[i]);
					ANGLE_T endAngle = NormalizeAngle(GetTrkEndAngle(Da.trk[i],Da.ep[i])+180);
					SetCornuEndPt(t,i,GetTrkEndPos(Da.trk[i],Da.ep[i]),Da.center[i],endAngle,Da.radius[i]);
				}
				if (Da.ep[i]>=0)
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
		Da.extend[0] = FALSE;
	    Da.extend[1] = FALSE;
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


	    if ((Da.trk[0] && (QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT) && !QueryTrack(Da.trk[0],Q_CAN_EXTEND))) &&
	    	(Da.trk[1] && (QueryTrack(Da.trk[1],Q_MODIFY_CANT_SPLIT) && !QueryTrack(Da.trk[1],Q_CAN_EXTEND)))) {
	    	wBeep();
	    	ErrorMessage("Both Ends of this Cornu are UnAdjustable");
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
		for (int i=0;i<2;i++) {
			if (!Da.trk[i] && Da.extend[i]) {
				if (Da.extendSeg[i].type == SEG_STRTRK) {
					Da.trk[i] = NewStraightTrack(Da.extendSeg[i].u.l.pos[0],Da.extendSeg[i].u.l.pos[1]);
					if (Da.trk[i]) Da.ep[i] = 1-i;
				} else {
					Da.trk[i] = NewCurvedTrack(Da.extendSeg[i].u.c.center,fabs(Da.extendSeg[i].u.c.radius),
							Da.extendSeg[i].u.c.a0,Da.extendSeg[i].u.c.a1,FALSE);
					if (Da.angle[i]>180)
						Da.ep[i] = (Da.extendSeg[i].u.c.a0>90 && Da.extendSeg[i].u.c.a0<270)?0:1;
					else
						Da.ep[i] = (Da.extendSeg[i].u.c.a0>90 && Da.extendSeg[i].u.c.a0<270)?1:0;
				}
				if (!Da.trk[i]) {
					wBeep();
					InfoMessage(_("Cornu Extension Create Failed for end %d"),i);
					return C_TERMINATE;
				}

			}
		}
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

		if (Da.trk[0]) UndoModify(Da.trk[0]);
		if (Da.trk[1]) UndoModify(Da.trk[1]);

		for (int i=0;i<2;i++) {										//Attach new track
			if (Da.trk[i] && Da.ep[i] != -1) {						//Like the old track
				MoveEndPt(&Da.trk[i],&Da.ep[i],Da.pos[i],0);
				if (GetTrkType(Da.trk[i])==T_BEZIER) {     //Bezier split position not precise, so readjust Cornu
					GetConnectedTrackParms(Da.trk[i],GetTrkEndPos(Da.trk[i],Da.ep[i]),i,Da.ep[i]);
					ANGLE_T endAngle = NormalizeAngle(GetTrkEndAngle(Da.trk[i],Da.ep[i])+180);
					SetCornuEndPt(t,i,GetTrkEndPos(Da.trk[i],Da.ep[i]),Da.center[i],endAngle,Da.radius[i]);
				}
				if (Da.ep[i]>= 0)
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
	DIST_T r_max = 0.0, rc, lc, last_l = 0;
	lc = * last_c;
	segProcData_t segProcData;
	if (segs.cnt == 0 ) return r_max;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_FILCRCL) continue;
		SegProc(SEGPROC_LENGTH,&t,&segProcData);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rc = fabs(fabs(1/t.u.c.radius)- lc)/segProcData.length.length;
			lc = fabs(1/t.u.c.radius);
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
		Da.extend[0] = FALSE;
		Da.extend[1] = FALSE;
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
				if (ep>=0 && QueryTrack(t,Q_CAN_ADD_ENDPOINTS)) ep=-1;  //Ignore Turntable Unconnected
				if	((ep==-1 && !QueryTrack(t,Q_CAN_ADD_ENDPOINTS)) || (ep>=0 && FindDistance(p,GetTrkEndPos(t,ep))>2)) {
					wBeep();
					InfoMessage(_("No Unconnected end point there"));
					return C_CONTINUE;
				}
				Da.trk[end] = t;
				Da.ep[end] = ep;           // Note: -1 for Turntable
				if (ep ==-1) pos = p;
				else pos = GetTrkEndPos(t,ep);
				Da.pos[end] = pos;
				found = TRUE;
				InfoMessage( _("Place 2nd end point of Cornu track on unconnected end-point") );
			} else {
				wBeep();
				InfoMessage(_("No Unconnected Track End there"));
				return C_CONTINUE;
			}
			if (Da.state == NONE) {
				if (!GetConnectedTrackParms(t, pos, 0, Da.ep[0])) {
					Da.trk[0] = NULL;
					return C_CONTINUE;
				}
				Da.state = POS_1;
				Da.selectPoint = 0;
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs, Da.pos[end], TRUE,!QueryTrack(t,Q_MODIFY_CANT_SPLIT));
				DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
				InfoMessage( _("Move 1st end point of Cornu track along track 1") );
			} else {
				if ( Da.trk[0] == t) {
				   ErrorMessage( MSG_JOIN_CORNU_SAME );
				   Da.trk[1] = NULL;
				   return C_CONTINUE;
				}
				if (!GetConnectedTrackParms(t, pos, 1, Da.ep[1])) {
					Da.trk[1] = NULL;								//Turntable Fail
					return C_CONTINUE;
				}
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
			if(QueryTrack(Da.trk[0],Q_MODIFY_CANT_SPLIT) && !QueryTrack(Da.trk[0],Q_CAN_ADD_ENDPOINTS)) {
				InfoMessage(_("Can't Split - Locked to End Point"));
				return C_CONTINUE;
			}
			if (Da.trk[0] != OnTrack(&pos, FALSE, TRUE)) {
				wBeep();
				InfoMessage(_("Point not on track 1"));
				Da.state = POS_1;
				Da.selectPoint = 1;
				return C_CONTINUE;
			}
			t = Da.trk[0];
			DrawCornuCurve(NULL,Da.ep1Segs,Da.ep1Segs_da_cnt,NULL,0,NULL,0,NULL,NULL,NULL,drawColorBlack);
			if (!GetConnectedTrackParms(t, pos, ep, Da.ep[ep])) {
				Da.state = POS_1;
				Da.selectPoint = 1;
				return C_CONTINUE;
			}
			Da.pos[ep] = pos;
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
