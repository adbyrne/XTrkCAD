/** \file turnout.c
 * Turnout drawing
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

#include "common.h"
#include "common-ui.h"
#include "compound.h"
#include "custom.h"
#include "draw.h"
#include "track.h"
#include "trackx.h"

/* Draw turnout data */

/** 
 * The types of turnouts that get enhanced drawing methods
 */
enum dtoType {
	DTO_INVALID,
	DTO_NORMAL,
	DTO_THREE,
	DTO_WYE,

	DTO_CURVED,

	DTO_XING,
	DTO_XNG9,
	DTO_SSLIP,
	DTO_DSLIP,

	DTO_LCROSS,
	DTO_RCROSS,
	DTO_DCROSS
}; 

// Define to plot control points (DTO_NORMAL, DTO_CURVED, DTO_XING, DTO_LCROSS)
// #define DTO_DEBUG DTO_XING

#define DTO_DIM 4  // Maximum number of paths
#define DTO_SEGS 24 // Maximum number of control points

static struct DrawToData_t {
	TRKINX_T index;
	enum dtoType toType;
	track_p trk;
	tieData_p td;
	int bridge; 
	int roadbed;
	int endCnt;
	int pathCnt;
	int routeCnt;
	int strCnt;
	int crvCnt;
	int rgtCnt;
	int lftCnt;
	int strPath;
	int str2Path;
	int crvPath;
	int crv2Path;
	int origCnt;
	int origins[DTO_DIM];
	coOrd midPt;
	struct extraDataCompound_t* xx;
} dtod;

struct DrawTo_t {
	int n;
	trkSeg_p trkSeg[DTO_SEGS];
	coOrd base[DTO_SEGS];
	coOrd baseLast;
	DIST_T dy[DTO_SEGS];
	ANGLE_T angle;
	ANGLE_T crvAngle;
	coOrd pts[DTO_SEGS];
	coOrd ptsLast;
	char type;
};

static struct DrawTo_t dto[DTO_DIM];



/****************************************
 *
 * Draw Turnout Roadbed
 *
 */

int roadbedOnScreen = 0;


void DrawTurnoutRoadbedSide(drawCmd_p d, wDrawColor color, coOrd orig, ANGLE_T angle, trkSeg_p sp, ANGLE_T side, int first, int last)
{
	segProcData_t data;
	if (last <= first)
		return;
	data.drawRoadbedSide.first = first;
	data.drawRoadbedSide.last = last;
	data.drawRoadbedSide.side = side;
	data.drawRoadbedSide.roadbedWidth = roadbedWidth;
	data.drawRoadbedSide.rbw = (wDrawWidth)floor(roadbedLineWidth * (d->dpi / d->scale) + 0.5);
	data.drawRoadbedSide.orig = orig;
	data.drawRoadbedSide.angle = angle;
	data.drawRoadbedSide.color = color;
	data.drawRoadbedSide.d = d;
	SegProc(SEGPROC_DRAWROADBEDSIDE, sp, &data);
}


static void ComputeAndDrawTurnoutRoadbedSide(
	drawCmd_p d,
	wDrawColor color,
	coOrd orig,
	ANGLE_T angle,
	trkSeg_p segPtr,
	int segCnt,
	int segInx,
	ANGLE_T side)
{
	unsigned long res, res1;
	int b0, b1;
	res = ComputeTurnoutRoadbedSide(segPtr, segCnt, segInx, side, roadbedWidth);
	if (res == 0L) {
	}
	else if (res == 0xFFFFFFFF) {
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], side, 0, 32);
	}
	else {
		for (b0 = 0, res1 = 0x00000001; res1 && (res1 & res); b0++, res1 <<= 1);
		for (b1 = 32, res1 = 0x80000000; res1 && (res1 & res); b1--, res1 >>= 1);
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], side, 0, b0);
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], side, b1, 32);
	}
}


static void DrawTurnoutRoadbed(
	drawCmd_p d,
	wDrawColor color,
	coOrd orig,
	ANGLE_T angle,
	trkSeg_p segPtr,
	int segCnt)
{
	int inx, trkCnt = 0, segInx = 0;
	for (inx = 0; inx < segCnt; inx++) {
		if (IsSegTrack(&segPtr[inx])) {
			segInx = inx;
			trkCnt++;
			if (trkCnt > 1)
				break;
		}
	}
	if (trkCnt == 0)
		return;
	if (trkCnt == 1) {
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], +90, 0, 32);
		DrawTurnoutRoadbedSide(d, color, orig, angle, &segPtr[segInx], -90, 0, 32);
	}
	else {
		for (inx = 0; inx < segCnt; inx++) {
			if (IsSegTrack(&segPtr[inx])) {
				ComputeAndDrawTurnoutRoadbedSide(d, color, orig, angle, segPtr, segCnt, inx, +90);
				ComputeAndDrawTurnoutRoadbedSide(d, color, orig, angle, segPtr, segCnt, inx, -90);
			}
		}
	}
}


/****************************************
 *
 * TURNOUT DRAWING
 *
 */

 /**
 * Get the paths from the turnout definition. Puts the results into static dto structure.
 * Curved segments are broken up into short sections of the lesser of 5 degrees or 5 * tie spacing. 
 *
 * \param trk track_p pointer to a track
 * \param xx  pointer to the extraDataCompound struct
 *
 * \returns the number of paths
 */
int GetTurnoutPaths(track_p trk, struct extraDataCompound_t* xx) {
	wIndex_t segInx;
	wIndex_t segEP;

	int i;
	ANGLE_T a0, a1, aa0, aa1;
	DIST_T r, len;
	coOrd p0, p1;

	PATHPTR_T pp;

	dtod.trk = trk;
	dtod.index = trk->index;
	dtod.xx = xx;

	dtod.td = GetLayerTieData(GetTrkLayer(trk));

	int pathCnt = 0, routeCnt = 0;

	for (i = 0; i < DTO_DIM; i++)
		dto[i].n = 0;

	// Validate that the first segment starts at (0, 0)
	// and if STR p1.y == 0, if CRV angle == 0 or angle == 180
	GetSegInxEP(1, &segInx, &segEP);
	trkSeg_p segPtr = &xx->segs[segInx];
	switch (segPtr->type) {
	case SEG_STRTRK:
		p0 = segPtr->u.l.pos[0];
		p1 = segPtr->u.l.pos[1];
		if ((FindDistance(p0, zero) > EPSILON) || (fabs(p1.y) > EPSILON))
			return -1;
		break;
	case SEG_CRVTRK:
		r = fabs(segPtr->u.c.radius);
		a0 = segPtr->u.c.a0;
		a1 = segPtr->u.c.a1;

		if (segPtr->u.c.radius > 0) {
			aa0 = a0;
		}
		else {
			aa0 = a0 + a1;
		}
		PointOnCircle(&p0, segPtr->u.c.center, r, aa0);
		if ((FindDistance(p0, zero) > EPSILON)
			|| ((fabs(aa0 - 180) > EPSILON) && (fabs(aa0) > EPSILON)))
			return -1;
		break;
	}

	pp = GetPaths(trk);
	while (pp[0]) {
		pp += strlen((char*)pp) + 1;

		ANGLE_T angle = 0;
		while (pp[0]) {
			if (pathCnt < DTO_DIM)
				dto[pathCnt].type = 'S';
			while (pp[0]) {
				GetSegInxEP(pp[0], &segInx, &segEP);
				// trkSeg_p 
				segPtr = &xx->segs[segInx];
				switch (segPtr->type) {
				case SEG_STRTRK:
					p0 = segPtr->u.l.pos[0];
					p1 = segPtr->u.l.pos[1];

					wIndex_t n = dto[pathCnt].n;
					dto[pathCnt].trkSeg[n] = segPtr;
					dto[pathCnt].base[n] = p0;
					n++;
					dto[pathCnt].trkSeg[n] = segPtr;
					dto[pathCnt].base[n] = p1;
					// n++;
					dto[pathCnt].n = n;

					if (n >= DTO_SEGS - 1) return -1;

					break;
				case SEG_CRVTRK:
					r = fabs(segPtr->u.c.radius);

					dto[pathCnt].type = segPtr->u.c.center.y < 0 ? 'R' : 'L';

					a0 = segPtr->u.c.a0;
					a1 = segPtr->u.c.a1;

					angle += a1;

					len = D2R(a1) * r;
					// Every 5 degrees or 5 * tie spacing
					int cnt = (int)floor(a1 / 5.0);
					int cnt2 = (int)floor(len / 5 / dtod.td->spacing);
					if (cnt2 > cnt) cnt = cnt2;
					if (cnt <= 0) cnt = 1;

					aa1 = a1 / cnt;
					if (dto[pathCnt].type == 'R') {
						aa0 = a0;
					}
					else {
						aa0 = a0 + a1;
						aa1 = -aa1;
					}
					PointOnCircle(&p0, segPtr->u.c.center, r, aa0);
					n = dto[pathCnt].n;
					dto[pathCnt].trkSeg[n] = segPtr;
					dto[pathCnt].base[n] = p0;
					n++;
					dto[pathCnt].n = n;

					while (cnt > 0) {
						aa0 += aa1;
						PointOnCircle(&p0, segPtr->u.c.center, r, aa0);

						// n = dto[pathCnt].n;
						dto[pathCnt].trkSeg[n] = segPtr;
						dto[pathCnt].base[n] = p0;
						n++;

						if (n >= DTO_SEGS - 1) return -1;

						cnt--;
					}
					n--; // remove that last point count
					dto[pathCnt].n = n;
				}
				pp++;
			}
			// Include the last point
			dto[pathCnt].crvAngle = angle;
			dto[pathCnt].n++;

			pathCnt++;
			if (pathCnt > DTO_DIM) return -1;
			pp++;
		}
		routeCnt++;
		pp++;
	}
	dtod.pathCnt = pathCnt;
	dtod.routeCnt = routeCnt;
	dtod.endCnt = trk->endCnt;

	// Guard value: n < DTO_SEGS - 2
	for (i = 0; i < pathCnt; i++)
		dto[i].pts[dto[i].n].x = DIST_INF;

	return pathCnt;
}

/**
* Sets the turnout type if compatible with enhanced drawing methods. The data is
* from the path data saved in dtod and dto by GetTurnoutPaths. The turnout type is
* stored in the dtod.toType. DTO_INVALID (0) if the enhanced methods cannot handle
* it.
*/
void GetTurnoutType() {
	dtod.strPath = -1;
	dtod.str2Path = -1;
	dtod.crvPath = -1;
	dtod.crv2Path = -1;

	dtod.toType = DTO_INVALID;

	int strCnt = 0, crvCnt = 0, lftCnt = 0, rgtCnt = 0;
//	enum dtoType toType = DTO_INVALID;
	int i, j;

	// Count path origins
	dtod.origCnt = 1;
	dtod.origins[0] = 0;

	for (i = 1; i < dtod.pathCnt; i++) {
		int eq = 0;
		for (j = 0; j < i; j++) {
			if (CoOrdEqual(dto[dtod.origins[j]].base[0], dto[i].base[0]))
				eq++;
		}
		if (eq == 0) {
			dtod.origins[dtod.origCnt] = i;
			dtod.origCnt++;
		}

		if (dtod.origCnt > 4)
			return;
	}

	// Determine the path type
	for (i = 0; i < dtod.pathCnt; i++) {
		switch (dto[i].type) {
		case 'S':
			strCnt++;
			if (strCnt == 1)
				dtod.strPath = i;
			else
				dtod.str2Path = i;
			break;
		case 'L':
			lftCnt++;
			crvCnt++;
			if (crvCnt == 1)
				dtod.crvPath = i;
			else
				dtod.crv2Path = i;
			break;
		case 'R':
			rgtCnt++;
			crvCnt++;
			if (crvCnt == 1)
				dtod.crvPath = i;
			else
				dtod.crv2Path = i;
			break;
		}
	}

	dtod.strCnt = strCnt;
	dtod.crvCnt = crvCnt;
	dtod.lftCnt = lftCnt;
	dtod.rgtCnt = rgtCnt;

	// Normal two- or three-way turnout, or a curved turnout
	if (dtod.origCnt == 1) {
		if (dtod.pathCnt == 2) {
			if (strCnt == 1 && crvCnt == 1) {
				dtod.toType = DTO_NORMAL;
			}
			else if ((strCnt == 0) && ((lftCnt == 2) || (rgtCnt == 2))) {
				// Assumes outer curve is [0] and inner is [1]
				if ((dto[0].crvAngle <= 20) && (dto[1].crvAngle - dto[0].crvAngle <= 15))
					dtod.toType = DTO_CURVED;
			}
			else if (lftCnt == 1 && rgtCnt == 1) {
				dtod.toType = DTO_WYE;
			}
		}
		else if ((dtod.pathCnt == 3) && (strCnt == 1)
			&& (lftCnt == 1) && (rgtCnt == 1)) {
			dtod.toType = DTO_THREE;
		}
	}
	else
		// Crossing, single- and double-slip
		if ((dtod.origCnt == 2) && (dtod.endCnt == 4)
			&& strCnt == 2) {

			ANGLE_T a0, a1, a2;
			a1 = FindAngle(dto[dtod.strPath].base[0], dto[dtod.strPath].base[1]);
			a2 = FindAngle(dto[dtod.str2Path].base[0], dto[dtod.str2Path].base[1]);
			// Swap the ends of the strPath if large angle
			if((a1 > 180.0) && (dto[dtod.strPath].n == 2))
			{
				coOrd tmp = dto[dtod.strPath].base[0];
				dto[dtod.strPath].base[0] = dto[dtod.strPath].base[1];
				dto[dtod.strPath].base[1] = tmp;

				i = dto[dtod.strPath].n - 1;
				tmp = dto[dtod.strPath].pts[0];
				dto[dtod.strPath].pts[0] = dto[dtod.strPath].pts[i];
				dto[dtod.strPath].pts[i] = tmp;

				a1 = a1 - 180.0; 
				dto[dtod.strPath].angle = a1;
			}
			// Swap the ends of the str2Path if large angle
			if((a2 > 180.0) && (dto[dtod.str2Path].n == 2))
			{
				coOrd tmp = dto[dtod.str2Path].base[0];
				dto[dtod.str2Path].base[0] = dto[dtod.str2Path].base[1];
				dto[dtod.str2Path].base[1] = tmp;
				
				i = dto[dtod.str2Path].n - 1;
				tmp = dto[dtod.str2Path].pts[0];
				dto[dtod.str2Path].pts[0] = dto[dtod.str2Path].pts[i];
				dto[dtod.str2Path].pts[i] = tmp;

				a2 = a2 - 180.0; 
				dto[dtod.str2Path].angle = a2;
			}
			a0 = DifferenceBetweenAngles(a1, a2);
			if(a0 < 0)
			{
				int tmp = dtod.strPath;
				dtod.strPath = dtod.str2Path;
				dtod.str2Path = tmp;
				a0 = NormalizeAngle(-a0);
			}
			if ((a0 > 90.0) || (a0 < 0.0))
				return;

			coOrd p1 = dto[dtod.strPath].base[0];
			coOrd p2 = dto[dtod.str2Path].base[0];
			coOrd pos = zero;
			int intersect = FindIntersection(&pos, p1, a1, p2, a2);

			if (intersect) {
				if(strCnt == 2 && dtod.pathCnt == 2){
					if((a0 <= 61) && (a0 >= -61))
						dtod.toType = DTO_XING;
					else
						dtod.toType = DTO_XNG9;
				}
				else if(dtod.pathCnt == 3 && (lftCnt == 1 || rgtCnt == 1)){
					dtod.toType = DTO_SSLIP;
				}
				else if(dtod.pathCnt == 4 && lftCnt == 1 && rgtCnt == 1){
					dtod.toType = DTO_DSLIP;
				}
			}
			// No intersect, it could be a crossover
			else if (strCnt == 2) {
				if (dtod.pathCnt == 4 && lftCnt == 1 && rgtCnt == 1) {
					dtod.toType = DTO_DCROSS;
				}
				else if(dtod.pathCnt == 3){
					// Perverse test because the cross paths go Left then Right, for example
					if(lftCnt == 1){
						dtod.toType = DTO_RCROSS;
					}
					else if(rgtCnt == 1){
						dtod.toType = DTO_LCROSS;
					}
					else{
						dtod.toType = DTO_INVALID;
					}
				}
			}
		}
}

#if 0
/**
 * Draw Layout lines and points
 * 
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 */
static void DrawDtoLayout(
	drawCmd_p d,
	SCALEINX_T scaleInx
)
{
#ifdef DTO_DEBUG
	// Draw the points and lines from dto
	double r = dtod.td->width / 2;
	// if (r < 1) r = 1;

	int i, j;
	for (i = 0; i < DTO_DIM; i++) {
		for (j = 0; j < dto[i].n; j++) {
			DrawFillCircle(d, dto[i].pts[j], r, drawColorPurple);
			if (j < dto[i].n - 1)
				DrawLine(d, dto[i].pts[j], dto[i].pts[j + 1], 0, drawColorPurple);
		}
	}
#endif
}
#endif

/**
* Use the coOrds to build a polygon and draw the bridge fill. Note that the coordinates are 
* passed as pairs, and rearranged into a polygon with the 1,2,4,3 order. 
* 
* \param d The drawing object
* \param c The color
* \param b1 The first coordinate
* \param b2 The second coordinate
* \param b3 The third coordinate
* \param b4 The fourth coordinate
*/
static void DrawFill(
	drawCmd_p d, 
	wDrawColor c,
	coOrd b1,
	coOrd b2,
	coOrd b3,
	coOrd b4
	) 
{
	coOrd p[4] = {b1, b2, b4, b3};
	DrawPoly(d,4,p,NULL,c,0,DRAW_FILL ); 
}

/**
* Draw Bridge parapets and background or Roadbed for a turnout
* 
* \param d The drawing object
* \param fillType 0 for bridge, 1 for roadbed
* \param path1 The first path
* \param path2 The second path
*/
static void DrawTurnoutFill(
    drawCmd_p d,
	int fillType, 
    int path1,
    int path2
)
{
    DIST_T trackGauge = GetTrkGauge(dtod.trk);
    wDrawWidth width2 = (wDrawWidth)round((2.0 * d->dpi) / BASE_DPI);
	if (d->options&DC_PRINT)
		width2 = (wDrawWidth)round(d->dpi / BASE_DPI);

	wDrawColor color = (fillType==0?bridgeColor:roadbedColor);
	double fillWidth = 1.5;
	coOrd b1,b2,b3,b4,b5,b6;
    ANGLE_T angle = dtod.xx->angle,a = 0.0;
    int i,j,i1,i2;
    i1 = path1;
    i2 = path2;
    if(dto[i1].base[dto[i1].n - 1].y < dto[i2].base[dto[i2].n - 1].y) {
        i1 = path2;
        i2 = path1;
        // a = -a;
    }

    if(dtod.toType == DTO_THREE) {
        i = dtod.strPath;
        DIST_T dy = fabs(dto[i].dy[0]) + trackGauge * fillWidth;
        b1 = dto[i].pts[0];
        Translate(&b3,b1,(angle + a),dy);
        b1 = dto[i].pts[dto[i].n - 1];
        Translate(&b4,b1,(angle + a),dy);
        b2 = dto[i].pts[0];
        Translate(&b5,b2,(angle + a),-dy);
        b2 = dto[i].pts[dto[i].n - 1];
        Translate(&b6,b2,(angle + a),-dy);

        // Draw the background
        DrawFill(d,color,b3,b4,b5,b6);
    }

	for(i = i1; 1; i = i2,a = 180.0) {
		DIST_T dy = fabs(dto[i].dy[0]) + trackGauge * fillWidth;
		b1 = dto[i].pts[0];
		Translate(&b3,b1,(angle + a),dy);
		Translate(&b5,b1,(angle + a),-(dy * 0.75));
		for(j = 1; j < dto[i].n; j++) {
			dy = fabs(dto[i].dy[j]) + trackGauge * fillWidth;
			b2 = dto[i].pts[j];
			Translate(&b4,b2,(angle + a),dy);
			Translate(&b6,b2,(angle + a),-(dy * 0.75));

			// Draw the background
			DrawFill(d,color,b3,b4,b5,b6);

			// Draw the bridge edge
			if(fillType==0) {
				DrawLine( d,b3,b4,width2,drawColorBlack );
			}

			b1 = b2;
			b3 = b4;
			b5 = b6;
		}

		if(i == i2)
			break;
	}

    EPINX_T ep;
    coOrd p;
    track_p trk1;
    coOrd p0,p1;

	// Bridge parapet ends
	if(fillType==0) {
		for(ep = 0; ep < 3; ep++) {
			trk1 = GetTrkEndTrk(dtod.trk,ep);

			if((trk1) && (!GetTrkBridge(trk1))) {

				p = GetTrkEndPos(dtod.trk,ep);
				a = GetTrkEndAngle(dtod.trk,ep) + 90.0;

				int i = (dtod.lftCnt > 0) && (dtod.rgtCnt == 0) ? 2 : 1;
				if(ep != i) {
					Translate(&p0,p,a,trackGauge * 1.5);
					Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
					DrawLine(d,p0,p1,width2,drawColorBlack);
				}
				if(ep != (3 - i)) {
					Translate(&p0,p,a,-trackGauge * 1.5);
					Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
					DrawLine(d,p0,p1,width2,drawColorBlack);
				}
			}
		}
	}
}

/**
* Draw Bridge parapets and background or Roadbed for a cross-over
* 
* \param d The drawing object
* \param fillType 0 for bridge, 1 for roadbed
* \param path1 The first path, straight
* \param path2 The second path, straight
*/
static void DrawCrossFill(
	drawCmd_p d, 
	int fillType, 
	int path1,
	int path2
)
{
	DIST_T trackGauge = GetTrkGauge(dtod.trk);
	wDrawWidth width2 = (wDrawWidth)round((2.0 * d->dpi)/BASE_DPI);
	if (d->options&DC_PRINT)
		width2 = (wDrawWidth)round(d->dpi / BASE_DPI);

	wDrawColor color = (fillType==0?bridgeColor:roadbedColor);
	double fillWidth = 1.5;
	coOrd b1, b2, b3, b4, b5, b6;
	ANGLE_T angle = dtod.xx->angle, a = 0.0;
	int i1, i2;
	i1 = path1;
	i2 = path2;
	if(dto[i1].base[dto[i1].n - 1].y < dto[i2].base[dto[i2].n - 1].y) {
		i1 = path2;
		i2 = path1;
		// a = -a;
	}

	DIST_T dy = fabs(dto[i1].dy[0]) + trackGauge * fillWidth;
	b1 = dto[i1].pts[0];
	Translate(&b3,b1,(angle + a),dy);
	b1 = dto[i1].pts[dto[i1].n-1];
	Translate(&b4,b1,(angle + a),dy);
	b2 = dto[i2].pts[0];
	Translate(&b5,b2,(angle + a),-dy);
	b2 = dto[i2].pts[dto[i2].n-1];
	Translate(&b6,b2,(angle + a),-dy);

	// Draw the background
	DrawFill(d, color, b3, b4, b5, b6);

	// Draw the bridge edges
	if(fillType == 0) {
		DrawLine(d,b3,b4,width2,drawColorBlack);
		DrawLine(d,b5,b6,width2,drawColorBlack);
	}

    EPINX_T ep;
    coOrd p;
    track_p trk1;
    coOrd p0,p1;

	// Bridge parapet ends
	if(fillType==0) {
		for(ep = 0; ep < 4; ep++) {
			trk1 = GetTrkEndTrk(dtod.trk,ep);

			if((trk1) && (!GetTrkBridge(trk1))) {
				p = GetTrkEndPos(dtod.trk,ep);
				a = GetTrkEndAngle(dtod.trk,ep) + 90.0;

				if((ep == 1) || (ep == 2)) {
					Translate(&p0,p,a,trackGauge * 1.5);
					Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
					DrawLine(d,p0,p1,width2,drawColorBlack);
				}
				if((ep == 0) || (ep == 3)) {
					Translate(&p0,p,a,-trackGauge * 1.5);
					Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
					DrawLine(d,p0,p1,width2,drawColorBlack);
				}
			}
		}
	}
}

/**
* Draw Bridge parapets and background or Roadbed for a crossing
* 
* \param d The drawing object
* \param fillType 0 for bridge, 1 for roadbed
* \param path1 The first path
* \param path2 The second path
*/
static void DrawXingFill(
	drawCmd_p d, 
	int fillType, 
	int path1,
	int path2
)
{
	DIST_T trackGauge = GetTrkGauge(dtod.trk);
	wDrawWidth width2 = (wDrawWidth)round((2.0 * d->dpi)/BASE_DPI);
	if (d->options&DC_PRINT)
		width2 = (wDrawWidth)round(d->dpi / BASE_DPI);

	double fillWidth = 1.5;
	coOrd b0, b1, b2, b3, b4, b5, b6;
	int i, j, i1, i2;
	i1 = dtod.strPath;
	i2 = dtod.str2Path;

	// Fill both straight sections
	wDrawWidth width3 = (wDrawWidth)round(trackGauge * 2 * fillWidth * d->dpi / d->scale); 
	wDrawColor color = (fillType==0?bridgeColor:roadbedColor);
	b1 = dto[i1].pts[0];
	b2 = dto[i1].pts[dto[i1].n-1];
	DrawLine(d,b1,b2,width3,color);
	b1 = dto[i2].pts[0];
	b2 = dto[i2].pts[dto[i1].n-1];
	DrawLine(d,b1,b2,width3,color);

	i1 = path1;
	i2 = path2;
	if(dto[i1].base[dto[i1].n - 1].y < dto[i2].base[dto[i2].n - 1].y) {
		i1 = path2;
		i2 = path1;
	}

	// Handle curved sections for slips
	BOOL_T hasLeft = 0, hasRgt = 0;
	ANGLE_T angle = dtod.xx->angle, a = 0.0;
	for(i = i1; 1; i = i2,a = 180.0) {
		DIST_T dy = fabs(dto[i].dy[0]) + trackGauge * fillWidth;
		b1 = dto[i].pts[0];
		Translate(&b3,b1,(angle + a),dy);
		Translate(&b5,b1,(angle + a),-(dy * 0.75));
		if(dto[i].type != 'S') {
			if(dto[i].type == 'L')
				hasLeft = 1;
			else if(dto[i].type == 'R')
				hasRgt = 1;
			for(j = 1; j < dto[i].n; j++) {
				dy = fabs(dto[i].dy[j]) + trackGauge * fillWidth;
				b2 = dto[i].pts[j];
				Translate(&b4,b2,(angle + a),dy);
				Translate(&b6,b2,(angle + a),-(dy * 0.75));

				// Draw the background
				DrawFill(d,color,b3,b4,b5,b6);

				// Draw the bridge edge
				if(fillType==0) {
					DrawLine( d,b3,b4,width2,drawColorBlack );
				}
				b1 = b2;
				b3 = b4;
				b5 = b6;
			}
		}
		if(i == i2)
			break;
	}

	if(dtod.strPath >= 0 && dtod.str2Path >= 0) {
		i1 = dtod.strPath;
		i2 = dtod.str2Path;
		if(!hasRgt&&fillType==0) {
			DIST_T dy = trackGauge * 1.5;
			ANGLE_T a1, a2;
			b1 = dto[i1].pts[0];
            a1 = dto[i1].angle + 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i2].pts[dto[i2].n - 1];
            a2 = dto[i2].angle + 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1-90.0, b4, a2-90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);
		}

		if(!hasLeft&&fillType==0) {
			DIST_T dy = trackGauge * 1.5;
			ANGLE_T a1, a2;
			b1 = dto[i2].pts[0];
			a1 = dto[i2].angle - 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i1].pts[dto[i1].n - 1];
			a2 = dto[i1].angle - 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1+90.0, b4, a2+90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);
		}

		if(dtod.toType == DTO_XNG9 && fillType==0) {
			DIST_T dy = trackGauge * 1.5;
			ANGLE_T a1, a2;
			b1 = dto[i1].pts[dto[i1].n - 1];
			a1 = dto[i1].angle + 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i2].pts[dto[i2].n - 1];
			a2 = dto[i2].angle - 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1-90.0, b4, a2+90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);

			b1 = dto[i1].pts[0];
			a1 = dto[i1].angle - 90;
			Translate(&b3,b1,a1,dy);

			b2 = dto[i2].pts[0];
			a2 = dto[i2].angle + 90;
			Translate(&b4,b2,a2,dy);

			FindIntersection(&b0, b3, a1+90.0, b4, a2-90.0);

			// Draw the bridge edge
			DrawLine(d,b3,b0,width2,drawColorBlack);
			DrawLine(d,b0,b4,width2,drawColorBlack);
		}
	}

    // Bridge wings
    EPINX_T ep;
    coOrd p;
    track_p trk1;
    coOrd p0,p1;

	if(fillType==0) {
		for(ep = 0; ep < 4; ep++) {
			trk1 = GetTrkEndTrk(dtod.trk,ep);

			if((trk1) && (!GetTrkBridge(trk1))) {
				p = GetTrkEndPos(dtod.trk,ep);
				a = GetTrkEndAngle(dtod.trk,ep) + 90.0;

				if((dtod.toType == DTO_XNG9) || (ep == 2) || (ep == 3)) {
					Translate(&p0,p,a,trackGauge * 1.5);
					Translate(&p1,p0,a - 45.0,trackGauge * 1.5);
					DrawLine(d,p0,p1,width2,drawColorBlack);
				}
				if((dtod.toType == DTO_XNG9) || (ep == 0) || (ep == 1)) {
					Translate(&p0,p,a,-trackGauge * 1.5);
					Translate(&p1,p0,a + 45.0,-trackGauge * 1.5);
					DrawLine(d,p0,p1,width2,drawColorBlack);
				}
			}
		}
	}
}

/**
 * Init Normal Turnout data structure
 * Calculates the dy value of each segment
 * Sets pts values REORIGIN base to actual position and angle
 * Save often used last base and last point coOrd
 */
static void DrawDtoInit()
{
	struct extraDataCompound_t* xx = dtod.xx;
	coOrd p1;
	int i, j;

	for(i = 0; i < DTO_DIM; i++) {
		int n = dto[i].n;
		for(j = 0; j < n; j++) {
			REORIGIN(p1,dto[i].base[j],xx->angle,xx->orig);
			dto[i].pts[j] = p1;
			if(j < n - 1)
				dto[i].dy[j] = (dto[i].base[j + 1].y - dto[i].base[j].y) / (dto[i].base[j + 1].x - dto[i].base[j].x);
		}
		dto[i].ptsLast = dto[i].pts[n - 1];
		dto[i].baseLast = dto[i].base[n - 1];
	}
}

/**
 * Draw Normal (Single Origin) Turnout Bridge and Ties. Uses the static dto and dtod structures.
 *
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 * \param color The tie color. If black the color is read from the global tieColor.
 */
static void DrawNormalTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	DIST_T len;
	coOrd pos;
	int cnt;
	ANGLE_T angle;
	coOrd s1, s2, p1, p2, q1, q2;
	int s0, p0, q0;
	ANGLE_T a0;

	if (color == wDrawColorBlack)
		color = tieColor;

//	DIST_T trackGauge = GetTrkGauge(dtod.trk);

	DrawDtoInit();

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_NORMAL) DrawDtoLayout(d, scaleInx);
#endif

	int strPath = dtod.strPath, othPath = 0, secPath = 1;
	int toType = dtod.toType;
//	int first = 1;

	switch (toType) {
	case DTO_NORMAL:
		othPath = 1 - strPath;
		secPath = strPath;
		break;
	case DTO_WYE:
		// strPath = 2;
		othPath = 0; secPath = 1;
		break;
	case DTO_THREE:
		switch (strPath) {
		case 0:
			othPath = 1; secPath = 2;
			break;
		case 1:
			othPath = 0; secPath = 2;
			break;
		case 2:
			othPath = 0; secPath = 1;
			break;
		}
		break;
	}

	if(dtod.bridge) {
		DrawTurnoutFill(d,0,othPath,secPath);
	}
	else 
		if(dtod.roadbed) {
			DrawTurnoutFill( d,1,othPath,secPath );
		}
	if (omitTies)
		return;

	// Straight vector for tie angle
	if (toType == DTO_WYE) {
		s1 = dto[othPath].pts[0];
		s2 = MidPtCoOrd(dto[othPath].ptsLast, dto[secPath].ptsLast);
	}
	else {
		s1 = dto[strPath].pts[0];
		s2 = dto[strPath].ptsLast;
	}
	// Diverging vector(s)
	p1 = dto[othPath].pts[0];
	p2 = dto[othPath].ptsLast;
	q1 = dto[secPath].pts[0];
	q2 = dto[secPath].ptsLast;

	len = FindDistance(s1, s2);
	angle = FindAngle(s1, s2); // The straight segment

	cnt = (int)floor(len / dtod.td->spacing + 0.5);
	if (cnt > 0) {
		int pn = dto[othPath].n;
		int qn = dto[secPath].n;
		DIST_T dx = len / cnt;
		s0 = p0 = q0 = 0;
		DIST_T tdlen = dtod.td->length;
		DIST_T tdmax = (toType == DTO_WYE) ? 2.0 * tdlen : 2.5 * tdlen;
		DIST_T tdwid = dtod.td->width;
		DIST_T px = len, dlenx = dx / 2;

		cnt = cnt > 1 ? cnt - 1 : 1;
		for (px = dlenx; cnt; cnt--, px += dx) {
			if (px >= dto[othPath].base[p0 + 1].x) p0++;
			if (px >= dto[secPath].base[q0 + 1].x) q0++;
			if (p0 >= pn || q0 >= qn)
				break;

			if ((px + dx >= dto[othPath].baseLast.x) || (px + dx >= dto[secPath].baseLast.x)) {
				break;
			}

			DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
			DIST_T dy2 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
			tdlen = dtod.td->length + fabs(dy1) + fabs(dy2);
			if (tdlen > tdmax)
				break;

			DIST_T dy = dy1 + dy2;
			Translate(&pos, s1, angle, px);
			Translate(&pos, pos, (angle - 90.0), dy / 2);

			DrawTie(d, pos, angle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		// Asymmetric? Use longer ties for remaining two tracks (strPath, othPath)
		DIST_T sx = px; // Save these values for second code block
		int s0 = p0;
		if((dtod.toType == DTO_THREE) && (px + dx >= dto[secPath].baseLast.x)){
			for ( ; cnt; cnt--, px += dx) {
				if (px >= dto[othPath].base[p0 + 1].x) p0++;
				// if (px >= dto[secPath].base[q0 + 1].x) q0++;
				if (p0 >= pn)
					break;

				if (px + dx >= dto[othPath].baseLast.x) {
					break;
				}

				DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
				tdlen = dtod.td->length + fabs(dy1);
				if (tdlen > tdmax)
					break;

				DIST_T dy = dy1;
				Translate(&pos, s1, angle, px);
				Translate(&pos, pos, (angle - 90.0), dy / 2);

				DrawTie(d, pos, angle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
			}
		}

		// Draw remaining ties, if any
		if (px + dx < dto[othPath].baseLast.x){
			p1 = dto[othPath].pts[p0];
			p2 = dto[othPath].ptsLast;
			angle = FindAngle(p1, p2);
			a0 = FindAngle(dto[othPath].base[p0], dto[othPath].baseLast);
			DIST_T lenr = (dto[othPath].baseLast.x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&p1, p2, angle, -lenr);
			DrawStraightTies(d, dtod.td, p1, p2, color);
		}
		else {
			p1 = dto[othPath].pts[pn - 2];
			a0 = FindAngle(p1, p2);
			Translate(&pos, p2, a0, -dx / 2);
			DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}
		// Restore saved values
		if(dtod.toType == DTO_THREE){
			px = sx;
			p0 = s0;
		}

		// Asymmetric? Use longer ties for remaining two tracks (strPath, secPath)
		if((dtod.toType == DTO_THREE) && (px + dx >= dto[othPath].baseLast.x)){
			for ( ; cnt; cnt--, px += dx) {
				// if (px >= dto[othPath].base[p0 + 1].x) p0++;
				if (px >= dto[secPath].base[q0 + 1].x) q0++;
				if (q0 >= qn)
					break;

				if (px + dx >= dto[secPath].baseLast.x) {
					break;
				}

				DIST_T dy1 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
				tdlen = dtod.td->length + fabs(dy1);
				if (tdlen > tdmax)
					break;

				DIST_T dy = dy1;
				Translate(&pos, s1, angle, px);
				Translate(&pos, pos, (angle - 90.0), dy / 2);

				DrawTie(d, pos, angle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
			}
		}
		if (px + dx < dto[secPath].baseLast.x) {
			q1 = dto[secPath].pts[q0];
			q2 = dto[secPath].ptsLast;
			angle = FindAngle(q1, q2);
			a0 = FindAngle(dto[secPath].base[q0], dto[secPath].baseLast);
			DIST_T lenr = (dto[secPath].baseLast.x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&q1, q2, angle, -lenr);
			DrawStraightTies(d, dtod.td, q1, q2, color);
		}
		else {
			q1 = dto[secPath].pts[qn - 2];
			a0 = FindAngle(q1, q2);
			Translate(&pos, q2, a0, -dx / 2);
			DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		// Final ties at end
		if (dtod.toType == DTO_THREE) {

			int n = (int)(dto[strPath].baseLast.x);
			if (px + dx < len) {
				angle = FindAngle(s1, s2);
				DIST_T lenr = len - px + dlenx;
				Translate(&s1, s2, angle, -lenr);
				DrawStraightTies(d, dtod.td, s1, s2, color);
			}
			else {
				n = dto[strPath].n;
				s1 = dto[strPath].pts[n - 2];
				a0 = FindAngle(s1, s2);
				Translate(&pos, s2, a0, -dx / 2);
				DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
			}
		}
	}
}

/**
 * Draw Curved (Single Origin) Turnout Bridge and Ties. Uses the static dto and dtod structures.
 *
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 * \param color The tie color. If black the color is read from the global tieColor.
 */
static void DrawCurvedTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	DIST_T len, r;
	coOrd pos;
	int cnt;
	ANGLE_T angle, dang;
	coOrd center;
	coOrd p1, p2, q1, q2;
	ANGLE_T a0, a1, a2;
	struct extraDataCompound_t* xx = dtod.xx;

	if (color == wDrawColorBlack)
		color = tieColor;

	DrawDtoInit();

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_CURVED) DrawDtoLayout(d, scaleInx);
#endif

	int othPath = 0, secPath = 1;
//	int toType = dtod.toType;

	if(dtod.bridge) {
		DrawTurnoutFill(d,0,othPath,secPath);
	}
	else
		if(dtod.roadbed) {
			DrawTurnoutFill(d,1,othPath,secPath);
		}
	if (omitTies)
		return;

	// Save the ending coordinates
	coOrd othEnd = zero, secEnd = zero;

	trkSeg_p trk;
	DIST_T tdlen = dtod.td->length, tdmax = tdlen * 2.5;
	DIST_T tdspc = dtod.td->spacing, tdspc2 = tdspc / 2.0;
	DIST_T tdwid = dtod.td->width;
//	double rdot = tdwid / 2;

	int pn = dto[othPath].n;
	int qn = dto[secPath].n;
	int p0 = 0, q0 = 0;
	DIST_T px = 0, qx = 0, dy = 0, dy1 = 0, dy2 = 0;

	double cosAdj = 1.0;

	angle = 0;
	px = tdspc2;
	qx = tdspc2;
	int segs = max(dto[othPath].n, dto[secPath].n);
	for (; segs > 0; segs--) {

		if (px >= dto[othPath].base[p0 + 1].x)
			p0++;
		if (qx >= dto[secPath].base[q0 + 1].x)
			q0++;
		if ((p0 >= pn - 1) || (q0 >= qn - 1)) {
			break;
		}

		trk = dto[othPath].trkSeg[p0];
		if (trk->type == SEG_CRVTRK) {

			center = trk->u.c.center;
			r = fabs(trk->u.c.radius);
			a0 = NormalizeAngle(trk->u.c.a0 + dtod.xx->angle);
			a1 = trk->u.c.a1;

			pos = center;
			REORIGIN(center, pos, xx->angle, xx->orig);

			len = r * D2R(a1);
			cnt = (int)floor(len / tdspc + 0.5);
			if (len - tdspc * cnt >= tdspc2) {
				cnt++;
			}
			DIST_T tdlen = dtod.td->length;
//			DIST_T dx = len / cnt, dx2 = dx / 2;

			if (cnt != 0) {
				dang = (len / cnt) * 360 / (2 * M_PI * r);
				DIST_T dx = len / cnt, dx2 = dx / 2;

				if (dto[othPath].type == 'R') {
					a2 = a0 + dang / 2;
				}
				else {
					a2 = a0 + a1 - dang / 2;
					dang = -dang;
				}
				angle += fabs(dang / 2);

				cosAdj = fabs(cos(D2R(angle)));
				px += dx2 * cosAdj;
				qx += dx2 * cosAdj;

				for (; cnt; cnt--, a2 += dang, angle += dang) {
					if (px >= dto[othPath].base[p0 + 1].x)
						p0++;
					if (qx >= dto[secPath].base[q0 + 1].x)
						q0++;
					if ((p0 >= pn - 1) || (q0 >= qn - 1)) {
						break;
					}

					coOrd e1, e2;
					PointOnCircle(&e1, center, r, a2);

					q1 = dto[secPath].pts[q0];
					q2 = dto[secPath].pts[q0 + 1];
					FindIntersection(&e2, e1, a2, q1, FindAngle(q1, q2));

					dy = FindDistance(e1, e2);
					DIST_T tlen = tdlen + dy;

					if (tlen > tdmax) {
						break;
					}

					Translate(&pos, e1, a2, -dy / 2);
					DrawTie(d, pos, angle + xx->angle + 90, tlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

					// Assures that these ends are the last point drawn before break
					othEnd = e1;
					secEnd = e2;

					cosAdj = fabs(cos(D2R(angle)));
					if (cnt > 1) {
						px += dx * cosAdj;
						qx += dx * cosAdj;
					}
					else {
						px += dx2 * cosAdj;
						qx += dx2 * cosAdj;
					}
				}
			}
		}
		else {
			cosAdj = fabs(cos(D2R(angle)));

			p1 = dto[othPath].base[p0];
			p2 = dto[othPath].base[p0 + 1];
			len = FindDistance(p1, p2);
			cnt = (int)floor(len / tdspc + 0.6);
			if (cnt > 0) {
				DIST_T dx = len / cnt/*, dx2 = dx / 2*/;

				for (; cnt; cnt--) {
					if (px >= dto[othPath].base[p0 + 1].x)
						p0++;
					if (qx >= dto[secPath].base[q0 + 1].x)
						q0++;
					if ((p0 >= pn - 1) || (q0 >= qn - 1)) {
						break;
					}

					p1 = dto[othPath].base[p0];
					p2 = dto[othPath].base[p0 + 1];

					if ((px >= dto[othPath].baseLast.x)
						|| (qx >= dto[secPath].baseLast.x)) {
						break;
					}

					dy1 = dto[secPath].base[q0].y + (qx - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					dy2 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy = dy1 - dy2;
					DIST_T tlen = tdlen + fabs(cosAdj * dy);
					if (tlen > tdmax) {
						break;
					}

					q1 = dto[secPath].pts[q0];
					q2 = dto[secPath].pts[q0 + 1];
					a1 = FindAngle(q1, q2);
					DIST_T xlen = qx - dto[secPath].base[q0].x;
					Translate(&pos, q1, a1, xlen);
					secEnd = pos;

					q1 = dto[othPath].pts[p0];
					q2 = dto[othPath].pts[p0 + 1];
					a1 = FindAngle(q1, q2);
					xlen = px - dto[othPath].base[p0].x;
					Translate(&pos, q1, a1, xlen);
					othEnd = pos;

					Translate(&pos, pos, (a1 - 90.0), dy / 2);
					DrawTie(d, pos, a1, tlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

					cosAdj = fabs(cos(D2R(angle)));
					px += dx * cosAdj;
					qx += dx * cosAdj;
				}
			}
			else {
				break;
			}
		}
	}

#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_CURVED) {
		DrawFillCircle(d, othEnd, rdot, drawColorGreen);
		DrawFillCircle(d, secEnd, rdot, drawColorGreen);

		DrawFillCircle(d, dto[othPath].pts[p0], rdot, drawColorBlue);
		DrawFillCircle(d, dto[secPath].pts[q0], rdot, drawColorBlue);
	}
#endif

	// Draw remaining ties, if any
	p1 = othEnd;
	p2 = dto[othPath].ptsLast;
	a0 = FindAngle(p1, p2);
	len = FindDistance(p1, p2);
	if (len >= 2 * tdspc) {
		Translate(&p1, p1, a0, tdspc2);
		DrawStraightTies(d, dtod.td, p1, p2, color);
	}
	else if (len > tdspc2) { 
		Translate(&p2, p2, a0, -tdspc2);
		DrawTie(d, p2, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}

	q1 = secEnd;
	q2 = dto[secPath].ptsLast;
	a0 = FindAngle(q1, q2);
	len = FindDistance(q1, q2);
	if (len >= 2 * tdspc) {
		Translate(&q1, q1, a0, tdspc2);
		DrawStraightTies(d, dtod.td, q1, q2, color);
	}
	else if (len > tdspc2) {
		Translate(&q2, q2, a0, -tdspc2);
		DrawTie(d, q2, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
}

/**
  * Draw Crossing and Slip Turnout Bridge and Ties - Uses the static dto and dtod structures.
  *
  * \param d The drawing object
  * \param scaleInx The layout/track scale index
  * \param color The tie color. If black the color is read from the global tieColor.
  */
static void DrawXingTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	DIST_T len;
	coOrd pos;
	int cnt;
	ANGLE_T cAngle;

	if (color == wDrawColorBlack)
		color = tieColor;

	coOrd c1, c2, s1, s2, p1, p2, q1;
	int p0, q0;
	ANGLE_T a0, a1, a2;
	int strPath = dtod.strPath, str2Path = dtod.str2Path;

	struct extraDataCompound_t* xx = dtod.xx;

	DrawDtoInit();

	dto[strPath].angle = FindAngle(dto[strPath].pts[0], dto[strPath].ptsLast);
	dto[str2Path].angle = FindAngle(dto[str2Path].pts[0], dto[str2Path].ptsLast);

	int othPath = strPath, secPath = str2Path;
	int toType = dtod.toType;

	int i, j;
	switch (toType) {
	case DTO_XING:
	case DTO_XNG9:
		break;
	case DTO_SSLIP:
		for (i = 0; i < dtod.pathCnt; i++) {
			if (dto[i].type == 'L' || dto[i].type == 'R') {
				secPath = i;
				break;
			}
		}
		break;
	case DTO_DSLIP:
		for (i = 0; i < dtod.pathCnt; i++) {
			if (dto[i].type == 'L') {
				othPath = i;
			}
			else if (dto[i].type == 'R') {
				secPath = i;
			}
		}
		break;
	}

	if(dtod.bridge) {
		DrawXingFill(d,0,othPath,secPath);
	}
	else
		if(dtod.roadbed) {
			DrawXingFill(d,1,othPath,secPath);
		}

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_XING) DrawDtoLayout(d, scaleInx);
#endif

	if (omitTies)
		return;

	DIST_T tdlen = dtod.td->length, tdmax = 2.0 * tdlen;
	DIST_T tdwid = dtod.td->width;
	DIST_T tdspc = dtod.td->spacing, tdspc2 = tdspc / 2;

	// Midpoint
	p1 = dto[strPath].pts[0];
	a1 = dto[strPath].angle;

	q1 = dto[str2Path].pts[0];
	a2 = dto[str2Path].angle;

	FindIntersection(&pos, p1, a1, q1, a2);
	dtod.midPt = pos;

#ifdef DTO_DEBUG
	if(DTO_DEBUG == DTO_XING)
	{
		double r = td->width / 2;
		DrawFillCircle(d,p1,r,drawColorPurple);
		DrawFillCircle(d,q1,r,drawColorPurple);
		DrawFillCircle(d,dtod.midPt,r,drawColorPurple);
	}
#endif

	// Tie length adjust
	double dAngle = fabs(DifferenceBetweenAngles(a1, a2));
	double magic = 1.0;

	// Short circuit the complex code for this simple case
	if (toType == DTO_XNG9) {
		p1 = dto[strPath].pts[0];
		p2 = dto[strPath].ptsLast;
		DrawStraightTies(d, dtod.td, p1, p2, color);

		p1 = dto[str2Path].pts[0];
		p2 = dto[str2Path].ptsLast;
		
		// Omit the center ties
		magic = 1 / cos(D2R(90 - dAngle));
		DIST_T tdadj = (tdlen / 2) * magic;
		DIST_T tdadj2 = tdspc2 * magic;

		dAngle = (dAngle - 90) / 2;
		Translate(&pos, dtod.midPt, a2, -tdadj - tdadj2);
		DrawTie(d, pos, a2 - dAngle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

		Translate(&pos, dtod.midPt, a2, -tdadj - tdspc);
		DrawStraightTies(d, dtod.td, p1, pos, color);

		Translate(&pos, dtod.midPt, a2, tdadj + tdadj2);
		DrawTie(d, pos, a2 - dAngle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

		Translate(&pos, dtod.midPt, a2, tdadj + tdspc);
		DrawStraightTies(d, dtod.td, pos, p2, color);
		return;
	}

	// Straight vector for tie angle
	s1 = MidPtCoOrd(dto[strPath].base[0], dto[str2Path].base[0]);
	s2 = MidPtCoOrd(dto[strPath].baseLast, dto[str2Path].baseLast);

	// Rotate base coordinates so that the tie line is aligned with x-axis and origin is at zero
	cAngle = FindAngle(s1, s2);
	for (i = 0; i < DTO_DIM; i++)
		for (j = 0; j < dto[i].n; j++) {
			dto[i].base[j].x -= s1.x;
			dto[i].base[j].y -= s1.y;
			Rotate(&dto[i].base[j], zero, (90.0 - cAngle));
		}

	for (i = 0; i < DTO_DIM; i++) {
		for (j = 0; j < dto[i].n - 1; j++) {
			dto[i].dy[j] = (dto[i].base[j + 1].y - dto[i].base[j].y) / (dto[i].base[j + 1].x - dto[i].base[j].x);
		}
		if (dto[i].type == 'S')
			dto[i].angle = FindAngle(dto[i].pts[0], dto[i].ptsLast);
	}

	// Tie center line in drawing coordinates
	REORIGIN(c1, s1, xx->angle, xx->orig);
	REORIGIN(c2, s2, xx->angle, xx->orig);
	cAngle = FindAngle(c1, c2);

	int pn = dto[othPath].n;
	int qn = dto[secPath].n;

	// Tie length adjust
	magic = 1 / cos(0.5 * D2R(dAngle));
	// Extra ties length adjust
	double magic2 =	1.0 / cos(0.5 * D2R(dAngle)); 

	// Draw right half
	len = FindDistance(dtod.midPt, c2);
	cnt = (int)floor(len / dtod.td->spacing + 0.5);
	if (cnt <= 0)
		return;

	DIST_T dx = len / cnt;
	p0 = q0 = 0;
	DIST_T dx2 = dx / 2;
	DIST_T px = len + dx2;
	DIST_T lenx = 0;

	while (p0 < pn && px > dto[othPath].base[p0 + 1].x) p0++;
	while (q0 < qn && px > dto[secPath].base[q0 + 1].x) q0++;
	while (p0 < pn && q0 < qn) {
		if (px > dto[othPath].base[p0 + 1].x) p0++;
		if (px > dto[secPath].base[q0 + 1].x) q0++;
		if (p0 >= pn || q0 >= qn)
			break;
		// Dont use baseLast, as base coOrds have been rotated
		if ((px + dx >= dto[othPath].base[pn - 1].x)
			|| (px + dx >= dto[secPath].base[qn - 1].x)) {
			break;
		}

		DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
		DIST_T dy2 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
		tdlen = (dtod.td->length + fabs(dy1) + fabs(dy2)) * magic;
		if(tdlen > tdmax)
		{
			if(dAngle >= 30)
			{
				DIST_T dy = (dy1 + dy2) / 2;
				Translate(&pos,dtod.midPt,cAngle,px - len);
				Translate(&pos,pos,(cAngle - 90.0),dy);
				DrawTie(d,pos,cAngle,tdlen - dtod.td->length * magic,tdwid,color,tieDrawMode == TIEDRAWMODE_SOLID);
				lenx += dx2 * magic2;
			}
			break;
		}

		DIST_T dy = (dy1 + dy2) / 2;
		Translate(&pos, dtod.midPt, cAngle, px - len);
		Translate(&pos, pos, (cAngle - 90.0), dy);
		DrawTie(d, pos, cAngle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

		px += dx;
		lenx += dx;
	}

	p1 = dtod.midPt;
	p2 = dto[strPath].ptsLast;
	DIST_T lenr = FindDistance(p1, p2) - lenx * magic2;
	a0 = dto[strPath].angle;
	if (lenr > dx) {
		Translate(&pos, p2, a0, -lenr);
		DrawStraightTies(d, dtod.td, pos, p2, color);
	}
	else {
		Translate(&pos, p2, a0, -dx2);
		DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}

	// p1 = dtod.midPt;
	p2 = dto[str2Path].ptsLast;
	lenr = FindDistance(p1, p2) - lenx * magic2;
	a0 = dto[str2Path].angle;
	if (lenr > dx) {
		Translate(&pos, p2, a0, -lenr);
		DrawStraightTies(d, dtod.td, pos, p2, color);
	}
	else {
		Translate(&pos, p2, a0, -dx2);
		DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}

	// Draw left half
	// Change the straight path used
	if (dtod.toType == DTO_SSLIP) {
		othPath = str2Path;
	}

	len = FindDistance(c1, dtod.midPt);
	cnt = (int)floor(len / dtod.td->spacing + 0.5);
	if (cnt <= 0)
		return;

	p0 = q0 = 0;
	tdlen = dtod.td->length;

	dx = len / cnt;
	dx2 = dx / 2;
	px = len - dx2;
	lenx = 0;

	while (p0 < pn && px > dto[othPath].base[p0 + 1].x) p0++;
	while (q0 < qn && px > dto[secPath].base[q0 + 1].x) q0++;
	while (p0 >= 0 && q0 >= 0) {
		if (px < dto[othPath].base[p0].x) p0--;
		if (px < dto[secPath].base[q0].x) q0--;
		if (p0 < 0 || q0 < 0)
			break;

		if ((px - dx < dto[othPath].base[0].x)
			|| (px - dx < dto[secPath].base[0].x)) {
			break;
		}

		DIST_T dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
		DIST_T dy2 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
		tdlen = (dtod.td->length + fabs(dy1) + fabs(dy2)) * magic;
		if(tdlen > tdmax)
		{
			if(dAngle >= 30)
			{
				DIST_T dy = (dy1 + dy2) / 2;
				Translate(&pos,dtod.midPt,cAngle,px - len);
				Translate(&pos,pos,(cAngle - 90.0),dy);
				DrawTie(d,pos,cAngle,tdlen - dtod.td->length * magic,tdwid,color,tieDrawMode == TIEDRAWMODE_SOLID);
				lenx += dx2 * magic2;
			}
			break;
		}

		DIST_T dy = (dy1 + dy2) / 2;
		Translate(&pos, dtod.midPt, cAngle, px - len);
		Translate(&pos, pos, (cAngle - 90.0), dy);
		DrawTie(d, pos, cAngle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

		px -= dx;
		lenx += dx;
	}

	p1 = dto[strPath].pts[0];
	p2 = dtod.midPt;
	a0 = dto[strPath].angle;
	lenr = FindDistance(p1, p2) - lenx * magic2;
	if (lenr > dx) {
		Translate(&pos, p1, a0, lenr);
		DrawStraightTies(d, dtod.td, p1, pos, color);
	}
	else {
		Translate(&pos, p1, a0, dx2);
		DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
	p1 = dto[str2Path].pts[0];
	// p2 = dtod.midPt;
	a0 = dto[str2Path].angle;
	lenr = FindDistance(p1, p2) - lenx * magic2;
	if (lenr > dx) {
		Translate(&pos, p1, a0, lenr);
		DrawStraightTies(d, dtod.td, p1, pos, color);
	}
	else {
		Translate(&pos, p1, a0, dx2);
		DrawTie(d, pos, a0, dtod.td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
	}
}

/**
 * Draw Crossover (Two Origin) Turnout Bridge and Ties. Uses the static dto and dtod structures.
 *
 * \param d The drawing object
 * \param scaleInx The layout/track scale index
 * \param color The tie color. If black the color is read from the global tieColor.
 */
static void DrawCrossTurnout(
	drawCmd_p d,
	SCALEINX_T scaleInx,
	BOOL_T omitTies,
	wDrawColor color)
{
	DIST_T len, dx;
	coOrd pos;
	int cnt;
	ANGLE_T angle;

	if (color == wDrawColorBlack)
		color = tieColor;

//	struct extraDataCompound_t* xx = dtod.xx;

	DrawDtoInit();

	// draw the points
#ifdef DTO_DEBUG
	if (DTO_DEBUG == DTO_LCROSS) DrawDtoLayout(d, scaleInx);
#endif

	int strPath = dtod.strPath, str2Path = dtod.str2Path;
	// Bad assumption
	int othPath = 2, secPath = 2;
	if (dtod.pathCnt == 4) secPath = 3;

	dto[strPath].angle = FindAngle(dto[strPath].pts[0], dto[strPath].ptsLast);
	dto[str2Path].angle = FindAngle(dto[str2Path].pts[0], dto[str2Path].ptsLast);

	if(dtod.bridge) {
		DrawCrossFill(d,0,strPath,str2Path);
	}
	else
		if(dtod.roadbed) {
			DrawCrossFill(d,1,strPath,str2Path);
		}
	if (omitTies)
		return;

	coOrd s1, s2, t1, t2, p1, p2, q1, q2;
	int s0, t0, p0, q0;

	int sn = dto[strPath].n;
	int tn = dto[str2Path].n;
	int pn = dto[othPath].n;
	int qn = dto[secPath].n;

	s1 = dto[strPath].pts[0];
	s2 = dto[strPath].ptsLast;
	t1 = dto[str2Path].pts[0];
	t2 = dto[str2Path].ptsLast;
	angle = dto[strPath].angle; 

	p1 = dto[othPath].base[0];
	p2 = dto[othPath].baseLast;
	q1 = dto[secPath].base[0];
	q2 = dto[secPath].baseLast;

	len = FindDistance(s1, s2);
	angle = dto[strPath].angle;

	cnt = (int)floor(len / dtod.td->spacing + 0.5);
	if (cnt > 0) {
		DIST_T px = 0;
		DIST_T dy, dy1, dy2;
		int cflag = 0;
		dy = dto[str2Path].base[0].y - dto[strPath].base[0].y;

		dx = len / cnt;
		s0 = t0 = p0 = q0 = 0;
		DIST_T tdlen = dtod.td->length;
		DIST_T tdwid = dtod.td->width;
		DIST_T dlenx = dx / 2;

		DIST_T px1 = len / 2 - dlenx * 5,
			px2 = len / 2 + dlenx * 4;

		for (px = dlenx; cnt; cnt--, px += dx) {
			if (px >= dto[strPath].base[s0 + 1].x) s0++;
			if (px >= dto[str2Path].base[t0 + 1].x) t0++;
			if (px >= dto[othPath].base[p0 + 1].x) p0++;
			if (px >= dto[secPath].base[q0 + 1].x) q0++;
			if (s0 >= sn || t0 >= tn || p0 >= pn || q0 >= qn)
				break;

			if ((px >= dto[strPath].baseLast.x)
				|| (px >= dto[str2Path].baseLast.x)) {
				break;
			}

			dy1 = dy2 = 0;
			cflag = 0;
			if (px < px1) {
				switch (dtod.toType) {
				case DTO_DCROSS:
					dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy2 = dy - dto[secPath].base[q0].y - (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					break;
				case DTO_LCROSS:
					dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy2 = 0;
					break;
				case DTO_RCROSS:
					dy1 = 0;
					dy2 = dy - dto[secPath].base[q0].y - (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					break;
				default:
					break;
				}
			}
			else if (px < px2) {
				dy1 = (dto[str2Path].base[s0].y - dto[strPath].base[t0].y);
				dy2 = 0;
				cflag = 1;
			}
			else {
				switch (dtod.toType) {
				case DTO_DCROSS:
					dy1 = dto[secPath].base[q0].y + (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					dy2 = dy - dto[othPath].base[p0].y - (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					break;
				case DTO_LCROSS:
					dy1 = 0;
					dy2 = dy - dto[secPath].base[q0].y - (px - dto[secPath].base[q0].x) * dto[secPath].dy[q0];
					break;
				case DTO_RCROSS:
					dy1 = dto[othPath].base[p0].y + (px - dto[othPath].base[p0].x) * dto[othPath].dy[p0];
					dy2 = 0;
					break;
				default:
					break;
				}
			}

			if (fabs(dy1) + fabs(dy2) >= dy) {
				dy1 = (dto[str2Path].base[s0].y - dto[strPath].base[t0].y);
				dy2 = 0;
				cflag = 1;
			}

			tdlen = dtod.td->length + fabs(dy1);
			Translate(&pos, s1, angle, px);
			Translate(&pos, pos, (angle - 90.0), dy1 / 2);
			DrawTie(d, pos, angle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);

			if (!cflag) {
				tdlen = dtod.td->length + fabs(dy2);
				Translate(&pos, t1, angle, px);
				Translate(&pos, pos, (angle - 90.0), -dy2 / 2);
				DrawTie(d, pos, angle, tdlen, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
			}
		}
		return;

		// Draw remaining ties, if any
		// Currently by definition, there won't be any
		/*
		if (px + dx < dto[strPath].baseLast.x) {
			p1 = dto[strPath].pts[p0];
			p2 = dto[strPath].ptsLast;
			angle = FindAngle(p1, p2);
			a0 = FindAngle(dto[strPath].base[p0], dto[strPath].baseLast);
			DIST_T lenr = (dto[strPath].baseLast.x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&p1, p2, angle, -lenr);
			DrawStraightTies(d, dtod.td, p1, p2, color);
		}
		else {
			p1 = dto[strPath].pts[pn - 2];
			a0 = FindAngle(p1, p2);
			Translate(&pos, p2, a0, -dx / 2);
			DrawTie(d, pos, a0, td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}

		if (px + dx < dto[str2Path].baseLast.x) {
			q1 = dto[str2Path].pts[q0];
			q2 = dto[str2Path].ptsLast;
			angle = FindAngle(q1, q2);
			a0 = FindAngle(dto[str2Path].base[q0], dto[str2Path].baseLast);
			DIST_T lenr = (dto[str2Path].baseLast.x - px + dlenx) / cos(D2R(90.0 - a0));
			Translate(&q1, q2, angle, -lenr);
			DrawStraightTies(d, dtod.td, q1, q2, color);
		}
		else {
			q1 = dto[str2Path].pts[qn - 2];
			a0 = FindAngle(q1, q2);
			Translate(&pos, q2, a0, -dx / 2);
			DrawTie(d, pos, a0, td->length, tdwid, color, tieDrawMode == TIEDRAWMODE_SOLID);
		}
		*/
	}
}

/**
  * Draw all turnout components: ties, rail, roadbed, etc. 
  *
  * The turnout is checked to see if the enhanced methods can be used. 
  * If so the ties are drawn and the TB_NOTIES bit is set so that the 
  * rails and such are drawn on top of the ties. Similarly the bridge 
  * and roadbed bits are cleared so the enhanced method can be used. 
  * Those bits are restored to the previous state before return.
  *
  * \param trk Pointer to the track object
  * \param d The drawing object
  * \param color The turnout color.
  */
EXPORT void DrawTurnout(
	track_p trk,
	drawCmd_p d,
	wDrawColor color)
{
	struct extraDataCompound_t* xx = GET_EXTRA_DATA(trk, T_TURNOUT, extraDataCompound_t);
	wIndex_t i;
	long widthOptions = 0;
	SCALEINX_T scaleInx = GetTrkScale(trk);
	DIST_T scale2rail = (d->options & DC_PRINT) ? (twoRailScale * 2 + 1) : twoRailScale;
	BOOL_T omitTies = !DoDrawTies(d, trk) || (d->scale > scale2rail) || ((d->options & DC_SIMPLE) != 0); // || (scaleInx == 0);

	widthOptions = DTS_LEFT | DTS_RIGHT;

	// Save these values
	int noTies = GetTrkNoTies(trk);
	int bridge = GetTrkBridge(trk);
	int roadbed = GetTrkRoadbed(trk);

	long skip = 0;
	/** @prefs [Preference] NormalTurnoutDraw=1 to skip enhanced drawing methods */
	wPrefGetInteger("Preference", "NormalTurnoutDraw", (long *) &skip, 0);

	int pathCnt = (skip == 0 ? GetTurnoutPaths(trk, xx) : 0);

	if ( (pathCnt > 1) && (pathCnt <= DTO_DIM)
		&& (trk->endCnt <= 4)
		&& (xx->special == TOnormal) )
		{

		dtod.bridge = bridge; 
		dtod.roadbed = roadbed;

//		int strPath = -1;
		GetTurnoutType();

		if (dtod.toType != DTO_INVALID) {

			switch (dtod.toType)
			{
			case DTO_NORMAL:
			case DTO_THREE:
			case DTO_WYE:
				DrawNormalTurnout(d, scaleInx, omitTies, color);
				break;
			case DTO_CURVED:
				DrawCurvedTurnout(d, scaleInx, omitTies, color);
				break;
			case DTO_XING:
			case DTO_XNG9:
			case DTO_SSLIP:
			case DTO_DSLIP:
				DrawXingTurnout(d, scaleInx, omitTies, color);
				break;
			case DTO_LCROSS:
			case DTO_RCROSS:
			case DTO_DCROSS:
				DrawCrossTurnout(d, scaleInx, omitTies, color);
				break;
			default:
				break;
			}
			// Ignore these settings
			SetTrkNoTies(trk, 1); 
			ClrTrkBits( trk,TB_BRIDGE );
			ClrTrkBits(trk, TB_ROADBED);
		}
	}

	// Begin standard DrawTurnout code to draw rails or centerline
	// no curve center for turnouts, leave centerline for sectional curved
	long opts = widthOptions | (xx->segCnt > 1 ? DTS_NOCENTER : 0);
	DrawSegsO(d, trk, xx->orig, xx->angle, xx->segs, xx->segCnt, GetTrkGauge(trk), color, opts);  


	for (i = 0; i < GetTrkEndPtCnt(trk); i++) {
		DrawEndPt(d, trk, i, color);
	}
	if ((d->options & DC_SIMPLE) == 0 &&
		(labelWhen == 2 || (labelWhen == 1 && (d->options & DC_PRINT))) &&
		labelScale >= d->scale &&
		(GetTrkBits(trk) & TB_HIDEDESC) == 0) {
		DrawCompoundDescription(trk, d, color);
		if (!xx->handlaid)
			LabelLengths(d, trk, color);
	}
	if (roadbedWidth > GetTrkGauge(trk) &&
		(((d->options & DC_PRINT) && d->scale <= (twoRailScale * 2 + 1) / 2.0) ||
			(roadbedOnScreen && d->scale <= twoRailScale)))
		DrawTurnoutRoadbed(d, color, xx->orig, xx->angle, xx->segs, xx->segCnt);

	// Restore these settings
	if (noTies == 0) ClrTrkBits(trk, TB_NOTIES);
	if (bridge) SetTrkBits(trk, TB_BRIDGE);
	if (roadbed) SetTrkBits(trk, TB_ROADBED);
}
