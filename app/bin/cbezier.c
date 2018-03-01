/** \file cbezier.c
 * Bezier Command. Draw or modify a Bezier (Track or Line).
 */
 /*  XTrkCad - Model Railroad CAD
 *
 * Cubic Bezier curves have a definitional representation as an a set of four points.
 * The first and fourth are the end points, while the middle two are control points.
 * The control points positions define the angle at the ends and by their relative positions the overall
 * curvature. This representation is a familiar approach for those who know drawing programs such as Adobe
 * Illustrator or CorelDraw.
 *
 * In XTrackCAD, the Bezier form is also represented and drawn as a set of
 * joined circular arcs that approximate the Bezier form within a small tolerance. This is because
 * many of the operations we need to do are either computationally difficult or
 * impossible using the Bezier equations. For example, creating a parallel Bezier
 * which is necessary to draw a track with two lines or sleepers has no easy, stable solution.
 * But the program is already able to do these tasks for straight lines and curves.
 *
 * Note that every time we change the Bezier points we have to recalculate the arc approximation,
 * but that means that the majority of the time we are using the simpler approximation.
 *
 * We do not allow Bezier curves that have loops or cusps as they make no sense for tracks and
 * can easily be approximated for lines with multiple unaligned Bezier curves.
 *
 * This program borrows from particular ideas about converting Bezier curves that Pomax placed into
 * open source. The originals in Javascript can be found at github.com/Pomax.
 * The web pages that explain many other techniques are located at https://pomax.github.io/bezierinfo
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
#include "cbezier.h"
#include "tbezier.h"
#include "cstraigh.h"
#include "drawgeom.h"
#include "cjoin.h"
#include "i18n.h"
#include "common.h"
#include "wcolors.h"
#include "math.h"
#include "utility.h"
#include "param.h"
#include "fileio.h"
#include "layout.h"
#include "cundo.h"

extern drawCmd_t tempD;


/*
 * STATE INFO
 */
enum Bezier_States { NONE,
	POS_1,
	CONTROL_ARM_1,
	POS_2,
	CONTROL_ARM_2,
	PICK_POINT,
	POINT_PICKED,
	TRACK_SELECTED };

typedef struct {
		curveData_t curveData;
		double start;
		double end;
		coOrd pos0;
		coOrd pos1;
		} bCurveData_t;

static struct {
		enum Bezier_States state;
		coOrd pos[4];
        int selectPoint;
        wDrawColor color;
        DIST_T width;
		track_p trk[2];
		EPINX_T ep[2];
		dynArr_t crvSegs_da;
		int crvSegs_da_cnt;
		trkSeg_t cp1Segs_da[4];
		int cp1Segs_da_cnt;
		trkSeg_t cp2Segs_da[4];
		int cp2Segs_da_cnt;
		BOOL_T unlocked;
		track_p selectTrack;
		BOOL_T track;
		DIST_T minRadius;
		} Da;



/**
 * Draw a ControlArm.
 * A control arm has two filled or unfilled circles for endpoints and a straight line between them.
 * If the end or control point is not selectable we don't mark it with a circle.
 * If a selectable end or control point is unlocked place a filled circle on it, otherwise an empty circle.
 * A red color indicates that this arm, end or control point is "active" as it was selected.
 */
int createControlArm(
                     trkSeg_t sp[],   //seg pointer for up to 3 trkSegs (ends and line)
                     coOrd pos0,     //end on curve
                     coOrd pos1,     // control point at other end of line
				     BOOL_T track,   // isTrack()? (otherwise Line)
				     BOOL_T selectable, 	// can this arm be selected?
                     BOOL_T cp_direction_locked, //isFixed to track
				     int point_selected, //number of point 0, 1 or -1
					 wDrawColor color   //drawColorBlack or drawColorWhite
                      )
{
    DIST_T d, w;
    d = tempD.scale*0.25;
    w = tempD.scale/tempD.dpi; /*double width*/
    sp[0].u.l.pos[0] = pos0;
    sp[0].u.l.pos[1] = pos1;
    sp[0].type = SEG_STRLIN;
    sp[0].width = w;
    sp[0].color = (point_selected>=0)?drawColorRed:drawColorBlack;
    int n = 0;
    if (selectable) {
    	for (int j=0;j<2;j++) {
    		if (j==0 && cp_direction_locked) continue;   //Don't show select circle if end locked
    		n++;
    		sp[n].u.c.center = j==0?pos0:pos1;
    		sp[n].u.c.radius = d/4;
    		sp[n].width = w;
    		sp[n].color = (j==point_selected)?drawColorRed:drawColorBlack;
    		if (j==point_selected && cp_direction_locked) {
    			sp[n].type = SEG_FILCRCL;
    		} else {
    			sp[n].type = SEG_CRVLIN;
    			sp[n].u.c.a0 = 0.0;
    			sp[n].u.c.a1 = 360.0;
    		}
    	}
    }
    return n+1;
}

coOrd getPoint(coOrd pos[4], double s) {
	double mt = 1-s;
	double a = mt*mt*mt;
	double b = mt*mt*s*3;
	double c = mt*s*s*3;
	double d = s*s*s;
	coOrd ret;
	ret.x = a*pos[0].x + b*pos[1].x + c*pos[2].x + d*pos[3].x;
	ret.y = a*pos[0].y + b*pos[1].y + c*pos[2].y + d*pos[3].y;
	return ret;
}
/*
 * Get Error between a Bezier and an arc centered at pc that goes from start to end
 *
 * Because the curve is defined to pass through the start and the end and the middle, the test is
 * to see how much of an error there is between those points. If the sum of the errors is off by more \
 * than 0.5 pixels - that will mean it is not a good fit.
 *
 */
double BezError(coOrd pos[4], coOrd center, coOrd start_point, double start, double end) {
	double quarter = (end - start) / 4;  // take point at 1/4 and 3/4 and check
	coOrd c1 = getPoint(pos, start + quarter);
	coOrd c2 = getPoint(pos, end - quarter);
	double ref = FindDistance(center, start_point); //radius
	double d1  = FindDistance(center, c1);          // distance to quarter
	double d2  = FindDistance(center, c2);          // distance to three quarters
	return fabs(d1-ref) + fabs(d2-ref);               //total error at quarter points
};

/*
 * Get distance between a point and a line segment
 */

double DistanceToLineSegment(coOrd p, coOrd l1, coOrd l2) {
	double A = p.x - l1.x;
    double B = p.y - l1.y;
    double C = l2.x - l1.x;
    double D = l2.y - l1.y;

    double dot = A * C + B * D;
    double len_sq = C * C + D * D;
    double param = -1;
    if (len_sq != 0) //non 0 length line
    	param = dot / len_sq;

    double xx, yy;

    if (param < 0) {      // zero length line or beyond end use point 1
    	xx = l1.x;
    	yy = l1.y;
    }  else if (param > 1) {  // beyond point 2 end of line segment
    	xx = l2.x;
    	yy = l2.y;
    }  else {					// In the middle
    	xx = l1.x + param * C;
    	yy = l1.y + param * D;
    }

    double dx = p.x - xx;        //distance to perpendicular (or end point)
    double dy = p.y - yy;
    return sqrt(dx * dx + dy * dy);
}

/*
 * Get Error between a straight line segment and the Bezier curve.
 * Sum distance to straight line of quarter points.
 */

double BezErrorLine(coOrd pos[4], coOrd start_point, coOrd end_point, double start, double end) {
	double quarter = (end - start) / 4;  // take point at 1/4 and 3/4 and check
	coOrd c1 = getPoint(pos, start + quarter);
	coOrd c2 = getPoint(pos, end - quarter);
	double d1 = DistanceToLineSegment(c1, start_point, end_point);
	double d2 = DistanceToLineSegment(c2, start_point, end_point);
	return fabs(d1)+fabs(d2);
}

/*
 * Add element to DYNARR pointed to by caller from segment handed in
 */
void addSegBezier(dynArr_t * const array_p, trkSeg_p seg) {
	trkSeg_p s;


	DYNARR_APPEND(trkSeg_t, * array_p, 1);          //Adds 1 to cnt
	s = &DYNARR_N(trkSeg_t,* array_p,array_p->cnt-1);
	s->type = seg->type;
	s->color = seg->color;
	s->width = seg->width;
	s->bezSegs.cnt = 0;
	if (s->bezSegs.ptr) MyFree(s->bezSegs.ptr);
	s->bezSegs.ptr=NULL;
	s->bezSegs.max = 0;
	if ((s->type == SEG_BEZLIN || s->type == SEG_BEZTRK) && seg->bezSegs.cnt) {
		s->u.b.angle0 = seg->u.b.angle0;  //Copy all the rest
		s->u.b.angle3 = seg->u.b.angle3;
		s->u.b.length = seg->u.b.length;
		s->u.b.minRadius = seg->u.b.minRadius;
		for (int i=0;i<4;i++) s->u.b.pos[i] = seg->u.b.pos[i];
		s->u.b.radius0 = seg->u.b.radius3;
		s->bezSegs.cnt = 0;
		if (s->bezSegs.ptr) MyFree(s->bezSegs.ptr);
		s->bezSegs.max = 0;
		s->bezSegs.ptr = NULL; //Make sure new space as addr copied in earlier from seg
		for (int i = 0; i<seg->bezSegs.cnt; i++) {
			addSegBezier(&s->bezSegs,(((trkSeg_p)seg->bezSegs.ptr)+i)); //recurse for copying embedded Beziers as in Cornu joint
		}
	} else {
		s->u = seg->u;
	}
}

enum BezierType {PLAIN, LOOP, CUSP, INFLECTION, DOUBLEINFLECTION, LINE, ENDS, COINCIDENT } bType;

/*
 * Analyse Bezier.
 *
 * Using results from Maureen C. Stone of XeroxParc and Tony deRose of U of Washington
 * characterise the curve type and find out what features it has.
 * We will eliminate cusps and loops as not useful forms. Line, Plain, Inflection and DoubleInflection are ok.
 *
 */
EXPORT enum BezierType AnalyseCurve(coOrd inpos[4], double *Rfx, double *Rfy, double *cusp) {

	*Rfx = *Rfy = 0;
	if (Da.track && inpos[0].x == inpos[3].x && inpos[0].y == inpos[3].y ) {
		return ENDS;
	}

	DIST_T d01 = FindDistance(inpos[0],inpos[1]);
	DIST_T d12 = FindDistance(inpos[1],inpos[2]);
	DIST_T d02 = FindDistance(inpos[0],inpos[2]);
	if (d01+d12 == d02) {								//straight
		DIST_T d23 = FindDistance(inpos[2],inpos[3]);
		DIST_T d03 = FindDistance(inpos[0],inpos[3]);
		if (d02+d23 == d03) return LINE;
	}
	int common_points = 0;
	for (int i=0;i<3;i++) {
		if (inpos[i].x == inpos[i+1].x && inpos[i].y == inpos[i+1].y) common_points++;
	}
	for (int i=0;i<2;i++) {
		if (inpos[i].x == inpos[i+2].x && inpos[i].y == inpos[i+2].y) common_points++;
	}

	if (common_points>2) {
		return COINCIDENT;
	}

	coOrd pos[4];
	coOrd offset2, offset = inpos[0];

	for (int i=0;i<4;i++) {     				//move to zero origin
		pos[i].x = inpos[i].x-offset.x;
		pos[i].y = inpos[i].y-offset.y;
	}

	offset2.x = -offset.x + pos[3].x;
	offset2.y = -offset.y + pos[3].y;
	if (pos[1].y == 0.0) {   					//flip order of points
		for (int i=0;i<4;i++) {
			coOrd temp_pos = pos[i];
			pos[i].x = pos[3-i].x - offset2.x;
			pos[i].y = pos[3-i].y - offset2.y;
			pos[3-i] = temp_pos;
		}
		if (pos[1].y == 0.0) {					//Both ways round the second point has no y left after translation
			return PLAIN;
		}
	}
	double f21 = (pos[2].y)/(pos[1].y);
	double f31 = (pos[3].y)/(pos[1].y);
	if (fabs(pos[2].x-(pos[1].x*f21)) <0.0001) return PLAIN;   //defend against divide by zero
	double fx = (pos[3].x-(pos[1].x*f31))/(pos[2].x-(pos[1].x*f21));
	double fy = f31+(1-f21)*fx;
	*Rfx = fx;
	*Rfy = fy;
	*cusp = fabs(fy - (-(fx*fx)+2*fx+3)/4);

	if (fy > 1.0) return INFLECTION;
	if (fx >= 1.0) return PLAIN;
	if (fabs(fy - (-(fx*fx)+2*fx+3)/4) <0.100) return CUSP;
	if (fy < (-(fx*fx)+2*fx+3)/4) {
		if (fx <= 0.0 && fy >= (3*fx-(fx*fx))/3) return LOOP;
		if (fx > 0.0 && fy >= (sqrt(3*(4*fx-fx*fx))-fx)/2) return LOOP;
		return PLAIN;
	}

	return DOUBLEINFLECTION;
}

/*
 * ConvertToArcs
 * Take a Bezier curve and turn it into a set of circular arcs, such that the error between the arc and the
 * Bezier is under 0.5 pixels at maxiumum zoom.
 *
 * This enables us to use normal methods (operating over the array of arcs)
 * to perform actions on the Bezier and also to export it to DXF.
 *
 */
EXPORT BOOL_T ConvertToArcs (coOrd pos[4], dynArr_t * segs, BOOL_T track, wDrawColor color, DIST_T width) {
	 double t_s = 0.0, t_e = 1.0;
	 double errorThreshold = 0.05;
	 bCurveData_t prev_arc;
	 prev_arc.end = 0.0;
	 bCurveData_t arc;
	 segs->cnt = 0; //wipe out
	 BOOL_T safety;
	 int col = 0;

	 double prev_e = 0.0;
	      // we do a binary search to find the "good `t` closest to no-longer-good"
	 do {
	     safety=FALSE;
	     // step 1: start with the maximum possible arc length
	      t_e = 1.0;
	     // points:
	     coOrd start_point, mid_point, end_point;
	     // booleans:
	     BOOL_T curr_good = FALSE, prev_good = FALSE, done = FALSE;
	      // numbers:
	      double t_m, step = 0;
	      // step 2: find the best possible arc
	      do {						// !done
	          prev_good = curr_good;   //remember last time
	          t_m = (t_s + t_e)/2;
	          step++;
	          start_point = getPoint(pos, t_s); //Start of arc
	          mid_point = getPoint(pos, t_m);  //Middle of trial arc
	          end_point = getPoint(pos, t_e);  //End of trial Arc

	          PlotCurve( crvCmdFromChord, start_point, end_point, mid_point,
	        		  	  &(arc.curveData), TRUE );  //Find Arc through three points

	          arc.start = t_s; 		//remember start
	          arc.end = t_e;			//remember end
	          arc.pos0 = start_point; //remember start point (used for Straight)
	          arc.pos1 = end_point;	// Remember end point (used for Straight)

	          if (arc.curveData.type == curveTypeStraight) {
	          	  double error = BezErrorLine(pos,start_point,end_point, t_s, t_e);
	          	  curr_good = (error <= errorThreshold/2);
	          	  arc.curveData.a0 = FindAngle(start_point,end_point);
	          	  arc.curveData.a1 = FindAngle(end_point,start_point);

	          } else if (arc.curveData.type == curveTypeNone) {
	        	  return FALSE;			//Something wrong
	          } else {
	        	  double error = BezError(pos, arc.curveData.curvePos, start_point, t_s, t_e);
	          	  curr_good = (error <= errorThreshold/2);
	          };

	          done = prev_good && !curr_good; //Was better than this last time?
	          if(!done) {
	          	  // this arc is fine: we can move 'e' up to see if we can find a wider arc
	          	  if(curr_good) {
	          		  prev_e = t_e;		  //remember good end only
	          		  prev_arc = arc;
	          		  // if e is already at max, then we're done for this arc.
	          		  if (t_e >= 1.0) {
	          			  // make sure we cap at t=1
	          			  arc.end = prev_e = 1.0;
	          			  // if we capped the arc segment to t=1 we also need to make sure that
	          			  // the arc's end angle is correct with respect to the bezier end point.
	          			  if (t_e > 1.0) {
	          				  if (arc.curveData.type != curveTypeStraight) {
	          					  coOrd d;
	          					  d.x = arc.curveData.curvePos.x + fabs(arc.curveData.curveRadius) * cos(D2R(arc.curveData.a1));
	          					  d.y = arc.curveData.curvePos.y + fabs(arc.curveData.curveRadius) * sin(D2R(arc.curveData.a1));

	          					  arc.curveData.a1 += FindAngle(d, getPoint(pos,1.0));
	          					  t_e = 1.0;
	          				  }
	          			  }
	          			  prev_arc = arc;
	          			  done = TRUE;
	          			  break;
	          		  }
	          		  // if not, move it up by half the iteration distance or to end
	          		  t_e = t_e + (t_e-t_s)/2;
	          		  if (t_e > 1.0) t_e = 1.0;
	          	  }
	          	  // this is a bad arc: we need to move 'e' down to find a good arc
	          	  else {
	          		  t_e = t_m;
	          	  }
	          }         // If !Done end
	        } while(!done && safety++<100);
	        if(safety>=100) {
	          return FALSE;				//Failed to make into arcs
	        }
	        prev_arc = prev_arc.end==0.0?arc:prev_arc;
	        trkSeg_t curveSeg;  			//Now set up tempSeg to copy into array
	        curveSeg.width = track?0:width;
	        if ( prev_arc.curveData.type == curveTypeCurve ) {
	        		if (track)
	        			curveSeg.color = (fabs(prev_arc.curveData.curveRadius)<(GetLayoutMinTrackRadius()-EPSILON))?exceptionColor:normalColor;
	        		else
	        			curveSeg.color = color;
	        		curveSeg.type = track?SEG_CRVTRK:SEG_CRVLIN;
	        		curveSeg.u.c.a0 = prev_arc.curveData.a0;
	        		curveSeg.u.c.a1 = prev_arc.curveData.a1;
	        		curveSeg.u.c.center = prev_arc.curveData.curvePos;
	        		if (prev_arc.curveData.negative)
	        			curveSeg.u.c.radius = -prev_arc.curveData.curveRadius;
	        		else
	        			curveSeg.u.c.radius = prev_arc.curveData.curveRadius;
	        } else {											//Straight Line because all points co-linear
	        	curveSeg.type = track?SEG_STRTRK:SEG_STRLIN;
	        	if (track)
	        		curveSeg.color = wDrawColorBlack;
	        	else
	        		curveSeg.color = color;
	        	curveSeg.u.l.angle = prev_arc.curveData.a1;
	        	curveSeg.u.l.pos[0] = prev_arc.pos0;
	        	curveSeg.u.l.pos[1] = prev_arc.pos1;
	        	curveSeg.u.l.option = 0;
	        }
	        addSegBezier(segs, &curveSeg);		//Add to array of segs used
	        t_s = prev_e;
	        col++;
	      } while(prev_e < 1.0);

	      return TRUE;
};
/*
 * Draw Bezier while editing it. It consists of three elements - the curve and one or two control arms.
 *
 */

EXPORT void DrawBezCurve(trkSeg_p control_arm1,
					int cp1Segs_cnt,
					trkSeg_p control_arm2,
					int cp2Segs_cnt,
					trkSeg_p curveSegs,
					int crvSegs_cnt,
					wDrawColor color
					) {
	long oldDrawOptions = tempD.funcs->options;
	tempD.funcs->options = wDrawOptTemp;
	long oldOptions = tempD.options;
	tempD.options = DC_TICKS;
	tempD.orig = mainD.orig;
	tempD.angle = mainD.angle;
	if (crvSegs_cnt && curveSegs)
		DrawSegs( &tempD, zero, 0.0, curveSegs, crvSegs_cnt, trackGauge, color );
	if (cp1Segs_cnt && control_arm1)
		DrawSegs( &tempD, zero, 0.0, control_arm1, cp1Segs_cnt, trackGauge, drawColorBlack );
	if (cp2Segs_cnt && control_arm2)
		DrawSegs( &tempD, zero, 0.0, control_arm2, cp2Segs_cnt, trackGauge, drawColorBlack );
	tempD.funcs->options = oldDrawOptions;
	tempD.options = oldOptions;

}

/*
 * If Track, make it red if the radius is below minimum
 */
void DrawTempBezier(BOOL_T track) {
  if (track) DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,Da.cp2Segs_da,Da.cp2Segs_da_cnt, (trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da_cnt,Da.minRadius<(GetLayoutMinTrackRadius()-EPSILON)?exceptionColor:normalColor);
  else
	DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,Da.cp2Segs_da,Da.cp2Segs_da_cnt, (trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da_cnt,drawColorBlack); //Add Second Arm
}

void CreateBothControlArms(int selectPoint, BOOL_T track) {
	if (selectPoint == -1) {
		Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0],
				Da.pos[1], track, TRUE, Da.trk[0]!=NULL, -1,
				drawColorBlack);
		Da.cp2Segs_da_cnt = createControlArm(Da.cp2Segs_da, Da.pos[3],
				Da.pos[2], track, TRUE, Da.trk[1]!=NULL, -1,
				drawColorBlack);
	} else if (selectPoint == 0 || selectPoint == 1) {
		Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0],
				Da.pos[1], track, TRUE, Da.trk[0]!=NULL, selectPoint,
				drawColorBlack);
		Da.cp2Segs_da_cnt = createControlArm(Da.cp2Segs_da, Da.pos[3],
				Da.pos[2], track, FALSE, Da.trk[1]!=NULL, -1,
				drawColorBlack);
	} else {
		Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0],
				Da.pos[1], track, FALSE, Da.trk[0]!=NULL, -1,
				drawColorBlack);
		Da.cp2Segs_da_cnt = createControlArm(Da.cp2Segs_da, Da.pos[3],
				Da.pos[2], track, TRUE, Da.trk[1]!=NULL,
				3-selectPoint, drawColorBlack);
	}
}

/*
 * AdjustBezCurve
 *
 * Called to adjust the curve either when creating it or modifying it
 * States are "PICK_POINT" and "POINT_PICKED" and "TRACK_SELECTED".
 *
 * In PICK_POINT, the user can select an end-point to drag and release in POINT_PICKED. They can also
 * hit Enter (which saves the changes) or ESC (which cancels them).
 *
 * Only those points which can be picked are shown with circles - locked end-points are not shown.
 *
 * SHIFT at release will lock , re-locking any end-points that are aligned with like items at the same position
 * (Track to unconnected Track, Line to any Line end).
 *
 */
EXPORT STATUS_T AdjustBezCurve(
		wAction_t action,
		coOrd pos,
		BOOL_T track,
		wDrawColor color,
		DIST_T width,
		bezMessageProc message )
{
	track_p t;
	DIST_T d;
	ANGLE_T angle1, angle2;
	static coOrd pos0, pos3, p;
	enum BezierType b;
	DIST_T dd;
	EPINX_T ep;
	double fx, fy, cusp;
	int controlArm = -1;


	if (Da.state != PICK_POINT && Da.state != POINT_PICKED && Da.state != TRACK_SELECTED) return C_CONTINUE;

	switch ( action & 0xFF) {

	case C_START:
			Da.selectPoint = -1;
			CreateBothControlArms(Da.selectPoint, track);
			if (ConvertToArcs(Da.pos,&Da.crvSegs_da,track,color,Da.width)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
			Da.minRadius = BezierMinRadius(Da.pos,Da.crvSegs_da);
			Da.unlocked = FALSE;
			if (track)
				InfoMessage( _("Select End-Point - Ctrl unlocks end-point") );
			else
				InfoMessage( _("Select End-Point") );
			DrawTempBezier(Da.track);
			return C_CONTINUE;

	case C_DOWN:
		if (Da.state != PICK_POINT) return C_CONTINUE;
		dd = 10000.0;
		Da.selectPoint = -1;
		DrawTempBezier(Da.track);   //wipe out
		for (int i=0;i<4;i++) {
			d = FindDistance(Da.pos[i],pos);
			if (d < dd) {
				if (i==0 && Da.trk[0]) continue;
				if (i==3 && Da.trk[1]) continue;   //ignore locked points
				dd = d;
				Da.selectPoint = i;
			}

		}
		if (!IsClose(dd) )	Da.selectPoint = -1;
		if (Da.selectPoint == -1) {
			InfoMessage( _("Not close enough to any valid, selectable point, reselect") );
			DrawTempBezier(Da.track);
			return C_CONTINUE;
		} else {
			pos = Da.pos[Da.selectPoint];
			Da.state = POINT_PICKED;
			InfoMessage( _("Drag point %d to new location and release it"),Da.selectPoint+1 );
		}
		CreateBothControlArms(Da.selectPoint, track);
		if (ConvertToArcs(Da.pos, &Da.crvSegs_da, track, color,Da.width)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		Da.minRadius = BezierMinRadius(Da.pos, Da.crvSegs_da);
		DrawTempBezier(Da.track);
		return C_CONTINUE;

	case C_MOVE:
		if (Da.state != POINT_PICKED) {
			InfoMessage(_("Pick any circle to adjust it - Enter to confirm, ESC to abort"));
			return C_CONTINUE;
		}
		//If locked, reset pos to be on line from other track
		DrawTempBezier(Da.track);   //wipe out
		if (Da.selectPoint == 1 || Da.selectPoint == 2) {  //CPs
			int controlArm = Da.selectPoint-1;			//Snap to direction of track
			if (Da.trk[controlArm]) {
				angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[controlArm], Da.ep[controlArm]));
				angle2 = NormalizeAngle(FindAngle(pos, Da.pos[Da.selectPoint==1?0:3])-angle1);
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos[Da.selectPoint==1?0:3], angle1, -FindDistance( Da.pos[Da.selectPoint==1?0:3], pos )*cos(D2R(angle2)) );
				else pos = Da.pos[Da.selectPoint==1?0:3];
			} // Dont Snap control points
		} else SnapPos(&pos);
		Da.pos[Da.selectPoint] = pos;
		CreateBothControlArms(Da.selectPoint, track);
		if (ConvertToArcs(Da.pos,&Da.crvSegs_da,track, color, Da.width)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		Da.minRadius = BezierMinRadius(Da.pos,Da.crvSegs_da);
		if (Da.track) {
			b = AnalyseCurve(Da.pos,&fx,&fy,&cusp);
			if (b==ENDS) {
				wBeep();
				InfoMessage(_("Bezier Curve Invalid has identical end points Change End Point"),b==CUSP?"Cusp":"Loop");
			} else if ( b == CUSP || b == LOOP) {
				wBeep();
				InfoMessage(_("Bezier Curve Invalid has %s Change End Point"),b==CUSP?"Cusp":"Loop");
			} else if ( b == COINCIDENT ) {
				wBeep();
				InfoMessage(_("Bezier Curve Invalid has three co-incident points"),b==CUSP?"Cusp":"Loop");
			} else if ( b == LINE ) {
				InfoMessage(_("Bezier is Straight Line"));
			} else
				InfoMessage( _("Bezier %s : Min Radius=%s Length=%s fx=%0.3f fy=%0.3f cusp=%0.3f"),track?"Track":"Line",
													FormatDistance(Da.minRadius>=100000?0:Da.minRadius),
													FormatDistance(BezierLength(Da.pos,Da.crvSegs_da)),fx,fy,cusp);
		} else
				InfoMessage( _("Bezier %s : Min Radius=%s Length=%s"),track?"Track":"Line",
									FormatDistance(Da.minRadius>=100000?0:Da.minRadius),
									FormatDistance(BezierLength(Da.pos,Da.crvSegs_da)));
		DrawTempBezier(Da.track);
		return C_CONTINUE;

	case C_UP:
		if (Da.state != POINT_PICKED) return C_CONTINUE;
		//Take last pos and decide if it should be snapped to a track because SHIFT is held (pos0 and pos3)
		ep = 0;
		BOOL_T found = FALSE;

		DrawTempBezier(Da.track);  //wipe out

		p = pos;

		if (track && (Da.selectPoint == 0 || Da.selectPoint == 3)) {  //EPs
			if ((MyGetKeyState() & WKEY_SHIFT) != 0) {   //Snap Track
				if ((t = OnTrackIgnore(&p, FALSE, TRUE, Da.selectTrack)) != NULL) { //Snap to endPoint
					ep = PickUnconnectedEndPointSilent(p, t);
					if (ep != -1) {
						Da.trk[Da.selectPoint/3] = t;
						Da.ep[Da.selectPoint/3] = ep;
						pos0 = Da.pos[(Da.selectPoint == 0)?1:2];
						pos = GetTrkEndPos(t, ep);
						found = TRUE;
					}
				} else {
					wBeep();
					InfoMessage(_("No unconnected End Point to lock to"));
				}
			}
		}
		if (found) {
			angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[Da.selectPoint/3], Da.ep[Da.selectPoint/3]));
			angle2 = NormalizeAngle(FindAngle(pos, pos0)-angle1);
			Translate(&Da.pos[Da.selectPoint==0?1:2], Da.pos[Da.selectPoint==0?0:3], angle1, FindDistance(Da.pos[Da.selectPoint==0?1:2],pos)*cos(D2R(angle2)));
		}
		Da.selectPoint = -1;
		CreateBothControlArms(Da.selectPoint,track);
		if (ConvertToArcs(Da.pos,&Da.crvSegs_da,track,color,Da.width)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
		Da.minRadius = BezierMinRadius(Da.pos,Da.crvSegs_da);
		if (Da.track) {
			b = AnalyseCurve(Da.pos,&fx,&fy,&cusp);
			if (b==ENDS) {
				wBeep();
				InfoMessage(_("Bezier Curve Invalid has identical end points Change End Point"),b==CUSP?"Cusp":"Loop");
			} else if ( b == CUSP || b == LOOP) {
				wBeep();
				InfoMessage(_("Bezier Curve Invalid has %s Change End Point"),b==CUSP?"Cusp":"Loop");
			} else if ( b == COINCIDENT ) {
				wBeep();
				InfoMessage(_("Bezier Curve Invalid has three co-incident points"),b==CUSP?"Cusp":"Loop");
			} else if ( b == LINE) {
				InfoMessage(_("Bezier Curve is Straight Line"));
			}
				InfoMessage(_("Pick any circle to adjust it - Enter to confirm, ESC to abort"));
		} else
			InfoMessage(_("Pick any circle to adjust it - Enter to confirm, ESC to abort"));
		DrawTempBezier(Da.track);
		Da.state = PICK_POINT;

		return C_CONTINUE;

	case C_OK:                            //C_OK is not called by Modify.
		if ( Da.state == PICK_POINT ) {
			char c = (unsigned char)(action >> 8);
			if (Da.track && Da.pos[0].x == Da.pos[3].x && Da.pos[0].y == Da.pos[3].y ) {
				wBeep();
				ErrorMessage(_("Invalid Bezier Track - end points are identical"));
				return C_CONTINUE;
			}
			if (Da.track) {
				b = AnalyseCurve(Da.pos,&fx,&fy,&cusp);
				if ( b == CUSP || b == LOOP ) {
					wBeep();
					ErrorMessage(_("Invalid Bezier Curve has a %s - Adjust"),b==CUSP?"Cusp":"Loop");
					return C_CONTINUE;
				} else if (b==COINCIDENT) {
					wBeep();
					ErrorMessage(_("Invalid Bezier Curve has three coincident points - Adjust"));
					return C_CONTINUE;
				} else if(b==ENDS) {
					ErrorMessage(_("Invalid Bezier Track - end points are identical"));
					return C_CONTINUE;
				}
			}
			Da.minRadius = BezierMinRadius(Da.pos,Da.crvSegs_da);
			DrawTempBezier(Da.track);
			UndoStart( _("Create Bezier"), "newBezier - CR" );
			if (Da.track) {
				t = NewBezierTrack( Da.pos, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);
				for (int i=0;i<2;i++)
							if (Da.trk[i] != NULL) ConnectAbuttingTracks(t,i,Da.trk[i],Da.ep[i]);
			}
			else t = NewBezierLine(Da.pos, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt,color,width);
			UndoEnd();
			if (Da.crvSegs_da.ptr) MyFree(Da.crvSegs_da.ptr);
			Da.crvSegs_da.ptr = NULL;
			Da.crvSegs_da.cnt = 0;
			Da.crvSegs_da.max = 0;
			DrawNewTrack(t);
			Da.state = NONE;
			MainRedraw();
			MapRedraw();
			return C_TERMINATE;

		}
		return C_CONTINUE;

	case C_REDRAW:
		DrawTempBezier(Da.track);
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}


}

struct extraData {
				BezierData_t bezierData;
		};

/*
 * CmdBezModify
 *
 * Called from Modify Command - this function deals with the real (old) track and calls AdjustBezCurve to tune up the new one
 * Sequence is this -
 * - The C_START is called from CmdModify C_DOWN action if a track has being selected. The old track is hidden, the editable one is shown.
 * - C_MOVES will be ignored until a C_UP ends the track selection and moves the state to PICK_POINT,
 * - C_DOWN then hides the track and shows the Bezier handles version. Selects a point (if close enough and available) and the state moves to POINT_PICKED
 * - C_MOVE drags the point around modifying the curve
 * - C_UP puts the state back to PICK_POINT (pick another)
 * - C_OK (Enter/Space) creates the new track, deletes the old and shows the changed track.
 * - C_CANCEL (Esc) sets the state to NONE and reshows the original track unchanged.
 *
 * Note: Available points are shown - if a Bezier track is attached to its neighbor, only the control point on that side is selectable.
 * Any free end-point can be locked to a unconnected end point using SHIFT during drag.
 */
STATUS_T CmdBezModify (track_p trk, wAction_t action, coOrd pos) {
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
		Da.cp1Segs_da_cnt = 0;
		Da.cp2Segs_da_cnt = 0;
		Da.selectPoint = -1;
		Da.selectTrack = NULL;

		if (IsTrack(trk)) Da.track = TRUE;
		else Da.track = FALSE;

		Da.selectTrack = trk;
	    Da.trk[0] = GetTrkEndTrk( trk, 0 );
		if (Da.trk[0]) Da.ep[0] = GetEndPtConnectedToMe(Da.trk[0],trk);
		Da.trk[1] = GetTrkEndTrk( trk, 1 );
		if (Da.trk[1]) Da.ep[1] = GetEndPtConnectedToMe(Da.trk[1],trk);

	    for (int i=0;i<4;i++) Da.pos[i] = xx->bezierData.pos[i];              //Copy parms from old trk
		InfoMessage(_("%s picked - now select a Point"),track?"Track":"Line");
		Da.state = TRACK_SELECTED;
		DrawTrack(Da.selectTrack,&mainD,wDrawColorWhite);                    //Wipe out real track, draw replacement
		return AdjustBezCurve(C_START, pos, Da.track, xx->bezierData.segsColor, xx->bezierData.segsWidth, InfoMessage);

	case C_DOWN:
		if (Da.state == TRACK_SELECTED) return C_CONTINUE;                   //Ignore until first up
		return AdjustBezCurve(C_DOWN, pos, Da.track, xx->bezierData.segsColor, xx->bezierData.segsWidth, InfoMessage);


	case C_MOVE:
		if (Da.state == TRACK_SELECTED) return C_CONTINUE;                   //Ignore until first up and down
		return AdjustBezCurve(C_MOVE, pos, Da.track, xx->bezierData.segsColor, xx->bezierData.segsWidth, InfoMessage);

	case C_UP:
		if (Da.state == TRACK_SELECTED) {
			Da.state = PICK_POINT;                                           //First time up, next time pick a point
		}
		return AdjustBezCurve(C_UP, pos, Da.track, xx->bezierData.segsColor, xx->bezierData.segsWidth,  InfoMessage);					//Run Adjust

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
		UndoStart( _("Modify Bezier"), "newBezier - CR" );
		if (Da.track) t = NewBezierTrack( Da.pos, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt);
		else t = NewBezierLine(Da.pos, (trkSeg_p)Da.crvSegs_da.ptr, Da.crvSegs_da.cnt,xx->bezierData.segsColor,xx->bezierData.segsWidth);

		DeleteTrack(trk, TRUE);

		if (Da.track) {
			for (int i=0;i<2;i++) {										//Attach new track
				if (Da.trk[i] != NULL && Da.ep[i] != -1) {								//Like the old track
					ConnectAbuttingTracks(t,i,Da.trk[i],Da.ep[i]);
				}
			}
		}
		UndoEnd();
		InfoMessage(_("Modify Bezier Complete - select another"));
		Da.state = NONE;
		return C_TERMINATE;

	case C_CANCEL:
		InfoMessage(_("Modify Bezier Cancelled"));
		Da.state = NONE;
		MainRedraw();
		MapRedraw();
		return C_TERMINATE;

	case C_REDRAW:
		return AdjustBezCurve(C_REDRAW, pos, Da.track, xx->bezierData.segsColor, xx->bezierData.segsWidth, InfoMessage);
	}

	return C_CONTINUE;

}

/*
 * Find length by adding up the underlying segments. The segments can be straights, curves or bezier.
 */
DIST_T BezierLength(coOrd pos[4],dynArr_t segs) {

	DIST_T dd = 0.0;
	if (segs.cnt == 0 ) return dd;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			dd += fabs(t.u.c.radius*D2R(t.u.c.a1));
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			dd +=BezierLength(t.u.b.pos,t.bezSegs);
		} else if (t.type == SEG_STRLIN || t.type == SEG_STRTRK ) {
			dd += FindDistance(t.u.l.pos[0],t.u.l.pos[1]);
		}
	}
	return dd;
}

DIST_T BezierMinRadius(coOrd pos[4],dynArr_t segs) {
	DIST_T r = 100000.0, rr;
	if (segs.cnt == 0 ) return r;
	for (int i = 0;i<segs.cnt;i++) {
		trkSeg_t t = DYNARR_N(trkSeg_t, segs, i);
		if (t.type == SEG_CRVTRK || t.type == SEG_CRVLIN) {
			rr = fabs(t.u.c.radius);
		} else if (t.type == SEG_BEZLIN || t.type == SEG_BEZTRK) {
			rr = BezierMinRadius(t.u.b.pos, t.bezSegs);
		} else rr = 100000.00;
		if (rr<r) r = rr;
	}
	return r;
}

/*
 * Create a Bezier Curve (Track or Line)
 * Sequence is
 * 1. Place 1st End (snapping if needed)
 * 2.Drag out 1st Control Arm (in one direction if snapped)
 * 3.Place 2nd End (again snapping)
 * 4.Drag out 2nd Control Arm (constrained if snapped)
 * 5 to n. Select and drag around points until done
 * n+1. Confirm with enter or Cancel with Esc
 */
STATUS_T CmdBezCurve( wAction_t action, coOrd pos )
{
	track_p t;
	static int segCnt;
	STATUS_T rc = C_CONTINUE;
	long curveMode = 0;
	long cmd;
	if (action>>8) {
		cmd = action>>8;
	} else cmd = (long)commandContext;

	Da.color = lineColor;
	Da.width = (double)lineWidth/mainD.dpi;

	switch (action&0xFF) {

	case C_START:

		Da.track = (cmd == bezCmdModifyTrack || cmd == bezCmdCreateTrack)?TRUE:FALSE;

		Da.state = POS_1;
		Da. selectPoint = -1;
		for (int i=0;i<4;i++) {
			Da.pos[i] = zero;
		}
		Da.trk[0] = Da.trk[1] = NULL;
		//tempD.orig = mainD.orig;

		DYNARR_RESET(trkSeg_t,Da.crvSegs_da);
		Da.cp1Segs_da_cnt = 0;
		Da.cp2Segs_da_cnt = 0;
		InfoMessage( _("Place 1st end point of Bezier + Shift -> snap to %s end"), Da.track?"Unconnected Track":"Line" );
		return C_CONTINUE;


	case C_DOWN:
		if ( Da.state == POS_1 || Da.state == POS_2) {   //Set the first or third point
			coOrd p = pos;
			BOOL_T found = FALSE;
			int end = Da.state==POS_1?0:1;
			EPINX_T ep;
			if (Da.track) {
				if ((MyGetKeyState() & WKEY_SHIFT) != 0) {   //Snap Track
					if ((t = OnTrack(&p, FALSE, TRUE)) != NULL) {
						ep = PickUnconnectedEndPointSilent(p, t);
						if (ep != -1) {
							Da.trk[end] = t;
							Da.ep[end] = ep;
							pos = GetTrkEndPos(t, ep);
							found = TRUE;
						}
					}
					if (!found) {
						wBeep();
						InfoMessage(_("Shift used, but no Unconnected Track End there"));
						return C_CONTINUE;
					}
				}
			} else {													//Snap Bez Line to Lines
				if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
					if ((t = OnTrack(&p,FALSE, FALSE)) != NULL) {
						if (GetClosestEndPt(t,&p)) {
							pos = p;
							found = TRUE;
						}
					} else {
						wBeep();
						InfoMessage(_("Shift used, but no Line End there"));
						return C_CONTINUE;
					}
				}
			}
			if (!found) SnapPos( &pos );
			if (Da.state == POS_1) {
				Da.pos[0] = pos;
				Da.pos[1] = pos;
				Da.state = CONTROL_ARM_1;  //Draw the first control arm
				Da.selectPoint = 1;
				InfoMessage( _("Drag end of first Control Arm") );
				Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0], Da.pos[1], Da.track,TRUE,Da.trk[1]!=NULL,1,wDrawColorBlack);
				DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			} else {
				Da.pos[3] = pos;  //2nd End Point
				Da.pos[2] = pos;  //2nd Ctl Point
				Da.state = POINT_PICKED; // Drag out the second control arm
				Da.selectPoint = 2;
				InfoMessage( _("Drag end of second Control Arm") );
				DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack); //Wipe out initial Arm
				Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0], Da.pos[1], Da.track,FALSE,Da.trk[0]!=NULL,-1,wDrawColorBlack);
				Da.cp2Segs_da_cnt = createControlArm(Da.cp2Segs_da, Da.pos[3], Da.pos[2], Da.track,TRUE,Da.trk[1]!=NULL,1,wDrawColorBlack);
				if (ConvertToArcs(Da.pos,&Da.crvSegs_da,Da.track,Da.color,Da.width)) Da.crvSegs_da_cnt = Da.crvSegs_da.cnt;
				DrawTempBezier(Da.track);
			}
			return C_CONTINUE;
		} else  {
			return AdjustBezCurve( action&0xFF, pos, Da.track, Da.color, Da.width, InfoMessage );
		}
		return C_CONTINUE;
			
	case C_MOVE:
		if (Da.state == POS_1) {
			InfoMessage( _("Place 1st end point of Bezier + Shift -> snap to %s end"), Da.track?"Unconnected Track":"Line" );
			return C_CONTINUE;
		}
		if (Da.state == POS_2) {
			InfoMessage( _("Select other end of Bezier, +Shift -> snap to %s end"), Da.track?"Unconnected Track":"Line" );
		}
		if (Da.state == CONTROL_ARM_1 ) {
			DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			if (Da.trk[0]) {
				EPINX_T ep = 0;
				ANGLE_T angle1,angle2;
				angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[0],Da.ep[0]));
				angle2 = NormalizeAngle(FindAngle(pos, Da.pos[0])-angle1);
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos[0], angle1, -FindDistance( Da.pos[0], pos )*cos(D2R(angle2)));
				else pos = Da.pos[0];
			} // Don't Snap control points
			Da.pos[1] = pos;
			Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0], Da.pos[1], Da.track, TRUE, Da.trk[0]!=NULL, 1, wDrawColorBlack);
			DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
		} else {
			return AdjustBezCurve( action&0xFF, pos, Da.track, Da.color, Da.width, InfoMessage );
		}
		return C_CONTINUE;

	case C_UP:
		if (Da.state == CONTROL_ARM_1) {
			DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			if (Da.trk[0]) {
				EPINX_T ep = Da.ep[0];
				ANGLE_T angle1,angle2;
				angle1 = NormalizeAngle(GetTrkEndAngle(Da.trk[0],Da.ep[0]));
				angle2 = NormalizeAngle(FindAngle(pos, Da.pos[0])-angle1);
				if (angle2 > 90.0 && angle2 < 270.0)
					Translate( &pos, Da.pos[0], angle1, -FindDistance( Da.pos[0], pos )*cos(D2R(angle2)));
				else pos = Da.pos[0];
			} // Don't Snap control points
			Da.pos[1] = pos;
			if (FindDistance(Da.pos[0],Da.pos[1]) <=minLength) {
				InfoMessage( _("Control Arm 1 is too short, try again") );
				Da.state = POS_1;
				return C_CONTINUE;
			}
			Da.state = POS_2;
			InfoMessage( _("Select other end of Bezier, +Shift -> snaps to %s end"), Da.track?"Unconnected Track":"Line" );
			Da.cp1Segs_da_cnt = createControlArm(Da.cp1Segs_da, Da.pos[0], Da.pos[1], Da.track, FALSE, Da.trk[0]!=NULL, -1, wDrawColorBlack);
			DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,NULL,0,NULL,0,drawColorBlack);
			return C_CONTINUE;
		} else {
			return AdjustBezCurve( action&0xFF, pos, Da.track, Da.color, Da.width, InfoMessage );
		}
	case C_TEXT:
			if (Da.state != PICK_POINT || (action>>8) != ' ')  //Space is same as Enter.
				return C_CONTINUE;
			/* no break */
    case C_OK:
    	if (Da.state != PICK_POINT) return C_CONTINUE;
        return AdjustBezCurve( C_OK, pos, Da.track, Da.color, Da.width, InfoMessage);

	case C_REDRAW:
		if ( Da.state != NONE ) {

			DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,Da.cp2Segs_da,Da.cp2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, Da.color);
		}
		return C_CONTINUE;

	case C_CANCEL:
		if (Da.state != NONE) {
			DrawBezCurve(Da.cp1Segs_da,Da.cp1Segs_da_cnt,Da.cp2Segs_da,Da.cp2Segs_da_cnt,(trkSeg_t *)Da.crvSegs_da.ptr,Da.crvSegs_da.cnt, Da.color);
			Da.cp1Segs_da_cnt = 0;
			Da.cp2Segs_da_cnt = 0;
			Da.crvSegs_da_cnt = 0;
			for (int i=0;i<2;i++) {
				Da.trk[i] = NULL;
				Da.ep[i] = -1;
			}
			if (Da.crvSegs_da.ptr) MyFree(Da.crvSegs_da.ptr);
			Da.crvSegs_da.ptr = NULL;
			Da.crvSegs_da.cnt = 0;
			Da.crvSegs_da.max = 0;
		}
		Da.state = NONE;
		return C_CONTINUE;
		
	default:

	return C_CONTINUE;
	}

}

void UpdateParms(wDrawColor color,long width) {
	DrawTempBezier(Da.track);
	Da.color = lineColor;
	Da.width = (double)lineWidth/mainD.dpi;
	if (Da.crvSegs_da.cnt) {
		ConvertToArcs(Da.pos,&Da.crvSegs_da,Da.track,Da.color,Da.width);
	}
	DrawTempBezier(Da.track);
}


#include "bitmaps/bezier.xpm"
#include "bitmaps/dbezier.xpm"

EXPORT void InitCmdBezier( wMenu_p menu )
{	

}
