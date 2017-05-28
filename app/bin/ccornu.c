/*
 *  XTrkCad - Model Railroad CAD
 *
 * Cornu Command. Draw or modify a Cornu Easement Track.
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
 * but fortunately Ralph Levien published a PhD thesis on this together with his mathematical libraries as
 * open source. He was doing work on font design where the Cornu shapes make beautiful smooth fonts. He
 * was faced with the reality, though that graphics packages do not include these shapes as native objects
 * and so his solution was to produce a set of Bezier curves that approximate the solution.
 *
 * We already have a tool that can produce a set of arcs and straight line to approximate a Bezier - so that in the end
 * for an easement track we will have an array of Bezier, each of which is an array of Arcs and Lines. The tool will
 * use the approximations for most of its work.
 *
 * The inputs for the Cornu are the end points, angles and radii. To match the Cornu algorithm's expectations we input
 * these as a set of knots (points on lines). One point is always the desired end point and the other two are picked
 * direct the code to derive the other two end conditions. To make it easy we pick the far end point of the joined
 * curve and a point midway between these two (on the curve). By specifying that the desired end point is a "one-way"
 * knot we ensure that the result has smooth ends of either zero or fixed radius.
 *
 * For producing parallel tracks we may introduce one additional knot at the middle of the gap at the right central position.
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
 * which recalculates the arc approximations, but that means that the majority of the time we are using the approximation.
 *
 * Cornus do not have cusps, but can result in smooth loops. It is not yet determined if we can automatically eliminate these.
 *
 * This program is built and founded upon Ralph Levien's seminal work and relies on an adaptation of his Cornu library.
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


/*
 * STATE INFO
 */
enum { NONE,
	POS_1,
	POS_2,
	PICK_POINT,
	POINT_PICKED,
	TRACK_SELECTED } Cornu_States;

static struct {
		enum CornuStates state;
		coOrd pos[2];
        int selectPoint;
        wDrawColor color;
        DIST_T width;
		track_p trk[2];
		EPINX_T ep[2];
		DIST_T radius[2];
		dynArr_t ep1Segs_da;
		int ep1Segs_da_cnt;
		dynArr_t ep2Segs_da;
		int ep2Segs_da_cnt;
		dynArr_t crvSegs_da;
		int crvSegs_da_cnt;
		trkSeg_t trk1Seg;
		trkSeg_t trk2Seg;
		track_p selectTrack;
		DIST_T minRadius;
		DIST_T maxRadiusChange;
		} Da;



/*
 * Draw a EndPoint.
 * A Cornu end Point has a filled circle surrounded by another circle for endpoint
 */
int createEndPoint(
                     trkSeg_t sp[],   //seg pointer for up to 2 trkSegs (ends and line)
                     coOrd pos0,     //end on curve
				     BOOL_T point_selected
                      )
{

	coOrd p0, p1;
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
    sp[1].type = SEG_FILCRCL;
    sp[1].width = w;
    sp[1].color = (point_selected>=0)?drawColorRed:drawColorBlack;
    return 2;
}


/*
 * Add element to DYNARR pointed to by caller from segment handed in
 */
void addSeg(dynArr_t * const array_p, trkSeg_p seg) {
	trkSeg_p s;


	DYNARR_APPEND(trkSeg_t, * array_p, 10);          //Adds 1 to cnt
	s = &DYNARR_N(trkSeg_t,* array_p,array_p->cnt-1);
	s->type = seg->type;
	s->color = seg->color;
	s->width = seg->width;
	if ((s->type == SEG_BEZLIN || s->type == SEG_BEZTRK) && seg->bezSegs.cnt) {
		s->u.b.angle0 = seg->u.b.angle0;  //Copy all the rest
		s->u.b.angle3 = seg->u.b.angle3;
		s->u.b.length = seg->u.b.length;
		s->u.b.minRadius = seg->u.b.minRadius;
		for (int i=0;i<4;i++) s->u.b.pos[i] = seg->u.b.pos[i];
		s->u.b.radius0 = seg->u.b.radius3;
		s->bezSegs.cnt = s->bezSegs.max = 0;
		s->bezSegs.ptr = NULL; //Make sure new space as addr copied in earlier from seg
		for (int i = 0; i<seg->bezSegs.cnt; i++) {
			addSeg(&s->bezSegs,((trkSeg_p)&seg->bezSegs.ptr[i])); //recurse for copying embedded Beziers as in Cornu joint
		}
	} else {
		s->u = seg->u;
	}
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
	tempD.funcs->options = oldDrawOptions;
	tempD.options = oldOptions;

}

/*
 * If Track, make it red if the radius is below minimum
 */
void DrawTempCornu() {


  DrawCornuCurve(Da.trk1Seg,
		  	  	  Da.ep1Segs_da,Da.ep1Segs_da_cnt,
				  (trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da_cnt,
				  Da.ep2Segs_da,Da.ep2Segs_da_cnt,
				  Da.trk2Seg,
				  Da.minRadius<minTrackRadius?drawColorRed:drawColorBlack);

}

void CreateBothEnds(int selectPoint) {
	if (selectPoint == -1) {
		Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs_da, Da.pos[0],FALSE);
		Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs_da, Da.pos[1],FALSE);
	} else if (selectPoint == 0 || selectPoint == 1) {
		Da.ep1Segs_da_cnt = createControlArm(Da.ep1Segs_da, Da.pos[0],selectPoint == 0);
		Da.ep2Segs_da_cnt = createControlArm(Da.ep2Segs_da, Da.pos[1],selectPoint == 1);
	} else {
		Da.ep1Segs_da_cnt = createControlArm(Da.ep1Segs_da, Da.pos[0],FALSE);
		Da.ep2Segs_da_cnt = createControlArm(Da.ep2Segs_da, Da.pos[3],FALSE);
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
		bezMessageProc message )
{
	track_p t;
	DIST_T d;
	ANGLE_T a, angle1, angle2;
	static coOrd pos0, pos3, p;
	int inx;
	DIST_T dd;
	EPINX_T ep;
	double fx, fy, cusp;
	int controlArm = -1;


	if (Da.state != PICK_POINT && Da.state != POINT_PICKED && Da.state != TRACK_SELECTED) return C_CONTINUE;

	switch ( action & 0xFF) {

	case C_START:
			Da.selectPoint = -1;
			CreateBothEnds(Da.selectPoint);
			if (CallCornu(Da.pos,&Da.crvSegs_da)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
			Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
			InfoMessage( _("Select End-Point") );
			DrawTempCornu();
			return C_CONTINUE;

	case C_DOWN:
		if (Da.state != PICK_POINT) return C_CONTINUE;
		dd = 10000.0;
		Da.selectPoint = -1;
		DrawTempCornu();   //wipe out
		for (int i=0;i<2;i++) {
			d = FindDistance(Da.pos[i],pos);
			if (d < dd) {
				dd = d;
				Da.selectPoint = i;
			}

		}
		if (!IsClose(dd) )	Da.selectPoint = -1;
		if (Da.selectPoint == -1) {
			InfoMessage( _("Not close enough to any valid, selectable point, reselect") );
			DrawTempCornu();
			return C_CONTINUE;
		} else {
			pos = Da.pos[Da.selectPoint];
			Da.state = POINT_PICKED;
			InfoMessage( _("Drag point %d to new location and release it"),Da.selectPoint+1 );
		}
		CreateBothEnds(Da.selectPoint);
		if (CallCornu(Da.pos, Da.trk, Da.ep, Da.radius, &Da.crvSegs_da)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		Da.minRadius = CornuMinRadius(Da.pos, Da.crvSegs_da);
		DrawTempCornu();
		return C_CONTINUE;

	case C_MOVE:
		if (Da.state != POINT_PICKED) return C_CONTINUE;
		//If locked, reset pos to be on line from other track
		DrawTempCornu();   //wipe out
		if (Da.selectPoint == 1 || Da.selectPoint == 2) {  //CPs
			int controlArm = Da.selectPoint-1;			//Snap to direction of track
			if (Da.trk[controlArm]) {
				angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[controlArm], Da.ep[controlArm]));
				angle2 = NormalizeAngle(FindAngle(pos, Da.pos[Da.selectPoint==1?0:3])-angle1);
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos[Da.selectPoint==1?0:3], angle1, -FindDistance( Da.pos[Da.selectPoint==1?0:3], pos )*cos(D2R(angle2)) );
				else pos = Da.pos[Da.selectPoint==1?0:3];
			} else SnapPos(&pos);
		} else SnapPos(&pos);
		Da.pos[Da.selectPoint] = pos;
		CreateBothEnds(Da.selectPoint);
		if (CallCornu(Da.pos,Da.trk, Da.ep, Da.radius, &Da.crvSegs_da)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
		InfoMessage( _("Cornu : Min Radius=%s Length=%s"),
									FormatDistance(Da.minRadius),
									FormatDistance(CornuLength(Da.pos,Da.crvSegs_da)));
		DrawTempCornu();
		return C_CONTINUE;

	case C_UP:
		if (Da.state != POINT_PICKED) return C_CONTINUE;
		ep = 0;
		BOOL_T found = FALSE;

		DrawTempCornu();  //wipe out


		if (found) {
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[controlArm], Da.ep[controlArm]));
			angle2 = NormalizeAngle(FindAngle(pos, pos0)-angle1);
			Translate(&Da.pos[Da.selectPoint==0?0:3], Da.pos[Da.selectPoint==0?0:3], angle1, -FindDistance(Da.pos[Da.selectPoint==0?0:3],pos)*cos(D2R(angle2)));
		}
		Da.selectPoint = -1;
		CreateBothEnds(Da.selectPoint);
		if (CallCornu(Da.pos,Da.trk, Da.ep, Da.radius,&Da.crvSegs_da)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
		InfoMessage(_("Pick any circle to adjust it - Enter to confirm, ESC to abort"));
		DrawTempCornu();
		Da.state = PICK_POINT;

		return C_CONTINUE;

	case C_OK:                            //C_OK is not called by Modify.
		if ( Da.state == PICK_POINT ) {
			char c = (unsigned char)(action >> 8);
			Da.minRadius = CornuMinRadius(Da.pos,Da.crvSegs_da);
			DrawTempCornu();
			UndoStart( _("Create Cornu"), "newCornu - CR" );
			t = NewCornuTrack( Da.pos, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);
				for (int i=0;i<2;i++)
							if (Da.trk[i] != NULL) ConnectAbuttingTracks(t,i,Da.trk[i],Da.ep[i]);

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
				CornuData_t cornuData;
		};

/*
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
	char c;
	long cmd;
	EPINX_T ep[2];

	trackParams_t params;
	enum BezierType b;

	struct extraData *xx = GetTrkExtraData(trk);
	cmd = (long)commandContext;


	switch (action) {
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
	    	Da.trk[i] = xx->cornuData.trk[i];
	    	Da.ep[i] = xx->cornuData.ep[i];
	    	Da.radius[i] = xx->cornuData.radius[i];
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
				if ((action>>8) != ' ')
					return C_CONTINUE;
					/* no break */
	case C_OK:
		if (Da.state != PICK_POINT) {										//Too early - abandon
			InfoMessage(_("No changes made"));
			Da.state = NONE;
			return C_CANCEL;
		}
		UndoStart( _("Modify Cornu"), "newCornu - CR" );
		t = NewCornuTrack( Da.pos, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);

		DeleteTrack(trk, TRUE);

		for (int i=0;i<2;i++) {										//Attach new track
			if (Da.trk[i] != NULL && Da.ep[i] != -1) {								//Like the old track
				ConnectAbuttingTracks(t,i,Da.trk[i],Da.ep[i]);
			}
		}
		UndoEnd();
		InfoMessage(_("Modify Cornu Complete - select another"));
		Da.state = NONE;
		return C_TERMINATE;

	case C_CANCEL:
		InfoMessage(_("Modify Cornu Cancelled"));
		Da.state = NONE;
		MainRedraw();
		return C_TERMINATE;
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
			dd += t.u.c.radius*D2R(NormalizeAngle(t.u.c.a0-t.u.c.a1));
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
	DIST_T d;
	static int segCnt;
	STATUS_T rc = C_CONTINUE;
	long curveMode = 0;
	long cmd;
	if (action>>8) {
		cmd = action>>8;
	} else cmd = (long)commandContext;

	Da.color = lineColor;
	Da.width = (double)lineWidth;

	switch (action&0xFF) {

	case C_START:

		Da.state = POS_1;
		Da. selectPoint = -1;
		for (int i=0;i<4;i++) {
			Da.pos[i] = zero;
		}
		Da.trk[0] = Da.trk[1] = NULL;
		//tempD.orig = mainD.orig;

		DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
		Da.ep1Segs_da_cnt = 0;
		Da.ep2Segs_da_cnt = 0;
		Da.trk1Seg = NULL;
		Da.trk2Seg = NULL;
		InfoMessage( _("Place 1st end point of Cornu track") );
		return C_CONTINUE;


	case C_DOWN:
		if ( Da.state == POS_1 || Da.state == POS_2) {   //Set the first or third point
			coOrd p = pos;
			BOOL_T found = FALSE;
			int end = Da.state==POS_1?0:1;
			EPINX_T ep;
		    if ((t = OnTrack(&p, TRUE, TRUE)) != NULL) {
				ep = PickUnconnectedEndPoint(p, t);
				if (ep != -1) {
					Da.trk[end] = t;
					Da.ep[end] = ep;
					pos = GetTrkEndPos(t, ep);
					found = TRUE;
				}
			}
			if (!found) {
				wBeep();
				InfoMessage(_("No Unconnected Track End there"));
				return C_CONTINUE;
			}
			if (Da.state == POS_1) {
				Da.pos[0] = pos;
				Da.state = POS_2;
				Da.selectPoint = 1;
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs_da, Da.pos[0], TRUE);
				DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			} else {
				Da.pos[1] = pos;  //2nd End Point
				Da.state = POINT_PICKED; // Drag out the second control arm
				Da.selectPoint = 2;
				DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack); //Wipe out initial Arm
				Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs_da, Da.pos[0], FALSE);
				Da.ep2Segs_da_cnt = createEndPoint(Da.ep2Segs_da, Da.pos[1], TRUE);
				if (CallCornu(Da.pos,&Da.crvSegs_da))
						Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
				DrawTempCornu();
			}
			return C_CONTINUE;
		} else  {
			return AdjustCornuCurve( action&0xFF, pos, Da.color, Da.width, InfoMessage );
		}
		return C_CONTINUE;
			
	case C_MOVE:

		if (Da.state == POS_1 ) {
			DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			EPINX_T ep = 0;
			ANGLE_T angle1,angle2;
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[0],Da.ep[0]));
			angle2 = NormalizeAngle(FindAngle(pos, Da.pos[0])-angle1);
			if (angle2 > 90.0 && angle2 < 270.0)
				Translate( &pos, Da.pos[0], angle1, -FindDistance( Da.pos[0], pos )*cos(D2R(angle2)));
			else pos = Da.pos[0];
			Da.pos[1] = pos;
			Da.ep1Segs_da_cnt = createEndPoint(Da.ep1Segs_da, Da.pos[0]);
			DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
		} else {
			return AdjustCornuCurve( action&0xFF, pos, Da.color, Da.width, InfoMessage );
		}
		return C_CONTINUE;

	case C_UP:
		if (Da.state == POS_1) {
		if (Da.trk)
			DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			EPINX_T ep = Da.ep[0];
				ANGLE_T angle1,angle2;
				angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[0],Da.ep[0]));
				angle2 = NormalizeAngle(FindAngle(pos, Da.pos[0])-angle1);
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos[0], angle1, -FindDistance( Da.pos[0], pos )*cos(D2R(angle2)));
				else pos = Da.pos[0];
			Da.pos[1] = pos;
			Da.state = POS_2;
			InfoMessage( _("Select other end of Cornu") );
			Da.ep1Segs_da_cnt = createControlArm(Da.ep1Segs_da, Da.pos[0], Da.pos[1], FALSE, Da.trk[0]!=NULL, -1, wDrawColorBlack);
			DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			return C_CONTINUE;
		} else {
			return AdjustCornuCurve( action&0xFF, pos, Da.color, Da.width, InfoMessage );
		}
	case C_TEXT:
			if (Da.state != PICK_POINT || (action>>8) != ' ')  //Space is same as Enter.
				return C_CONTINUE;
			/* no break */
    case C_OK:
    	if (Da.state != PICK_POINT) return C_CONTINUE;
        return AdjustCornuCurve( C_OK, pos, Da.color, Da.width, InfoMessage);

	case C_REDRAW:
		if ( Da.state != NONE ) {
			DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,Da.ep2Segs_da,Da.ep2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, Da.color);
		}
		return C_CONTINUE;

	case C_CANCEL:
		if (Da.state != NONE) {
			DrawCornuCurve(Da.ep1Segs_da,Da.ep1Segs_da_cnt,Da.ep2Segs_da,Da.ep2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, Da.color);
			Da.ep1Segs_da_cnt = 0;
			Da.ep2Segs_da_cnt = 0;
			Da.crvSegs_da_cnt = 0;
			for (int i=0;i<2;i++) {
				Da.trk[i] = NULL;
				Da.ep[i] = -1;
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
