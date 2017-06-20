/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/tcornu.h,v 1.1 2005-12-07 15:47:36 rc-flyer Exp $
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
typedef struct {
		coOrd pos[2];
		coOrd c[2];
		ANGLE_T a[2];
		DIST_T r[2];
		DIST_T minCurveRadius;
		DIST_T maxRateofChange;
		DIST_T length;
		ANGLE_T windingAngle;
		dynArr_t arcSegs;
		coOrd descriptionOff;
		} cornuData_t;

typedef struct {
		coOrd pos[2];
		DIST_T radius[2];		//0.0 if straight
		ANGLE_T angle[2];		//Set if straight
		coOrd center[2];		//Set if radius >0
		} cornuParm_t;


double CornuMaxCurve(coOrd[2],ANGLE_T[2],DIST_T[2]);
double BezierMathMinRadius(coOrd[4]);
coOrd BezierMathFindNearestPoint(coOrd *, coOrd[4] , int );
track_p NewCornuTrack(coOrd pos[2], coOrd center[2], ANGLE_T angle[2], DIST_T radius[2], trkSeg_t * tempsegs, int count);
DIST_T CornuDistance( coOrd *, coOrd[2], ANGLE_T[2], DIST_T[2], trkSeg_t * ,int , double * );
BOOL_T FixUpCornu(coOrd pos[2], track_p [2], EPINX_T ep[2], struct extraData* xx);
BOOL_T FixUpCornu0(coOrd pos[2], coOrd center[2], ANGLE_T angle[2], DIST_T radius[2], struct extraData* xx);
BOOL_T GetCornuSegmentsFromTrack(track_p, trkSeg_p);
BOOL_T SetCornuEndPt(track_p trk, EPINX_T inx, coOrd pos, coOrd center, ANGLE_T angle, DIST_T radius);
BOOL_T RebuildCornu (track_p trk);

BOOL_T CallCornu(coOrd[2],track_p[2],EPINX_T[2],dynArr_t *,cornuParm_t *);
BOOL_T CallCornu0(coOrd[2], coOrd[2], ANGLE_T[2], DIST_T[2], dynArr_t *,BOOL_T);

BOOL_T GetBezierSegmentsFromCornu(track_p, dynArr_t *);



