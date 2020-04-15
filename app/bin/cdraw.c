/** \file cdraw.c
 * Drawing of geometric elements
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

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "ccurve.h"
#include "cbezier.h"
#include "drawgeom.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "misc.h"

extern TRKTYP_T T_BZRLIN;

static wMenu_p drawModDelMI;
static wMenu_p drawModLinMI;
static wMenuPush_p drawModDel;
static wMenuPush_p drawModSmooth;
static wMenuPush_p drawModVertex;
static wMenuPush_p drawModRound;
static wMenuPush_p drawModriginMode;
static wMenuPush_p drawModPointsMode;
static wMenuPush_p drawModOrigin;
static wMenuPush_p drawModLast;
static wMenuPush_p drawModCenter;
static wMenuPush_p drawModClose;
static wMenuPush_p drawModOpen;
static wMenuPush_p drawModFill;
static wMenuPush_p drawModEmpty;
static wMenuPush_p drawModSolid;
static wMenuPush_p drawModDot;
static wMenuPush_p drawModDash;
static wMenuPush_p drawModDashDot;
static wMenuPush_p drawModDashDotDot;

extern void wSetSelectedFontSize(int size);

static long fontSizeList[] = {
		4, 5, 6, 7, 8, 10, 12, 14, 16, 18, 20, 24, 28, 32, 36,
		40, 48, 56, 64, 72, 80, 90, 100, 120, 140, 160, 180,
		200, 250, 300, 350, 400, 450, 500 };

EXPORT void LoadFontSizeList(
	wList_p list,
	long curFontSize)
{
	wIndex_t curInx = 0, inx1;
	int inx;
	wListClear(list);
	for (inx = 0; inx < sizeof fontSizeList / sizeof fontSizeList[0]; inx++)
	{
		if ((inx == 0 || curFontSize > fontSizeList[inx - 1]) &&
			(curFontSize < fontSizeList[inx]))
		{
			sprintf(message, "%ld", curFontSize);
			curInx = wListAddValue(list, message, NULL, (void*)curFontSize);
		}
		sprintf(message, "%ld", fontSizeList[inx]);
		inx1 = wListAddValue(list, message, NULL, (void*)fontSizeList[inx]);
		if (curFontSize == fontSizeList[inx])
			curInx = inx1;
	}
	if (curFontSize > fontSizeList[(sizeof fontSizeList / sizeof fontSizeList[0]) - 1])
	{
		sprintf(message, "%ld", curFontSize);
		curInx = wListAddValue(list, message, NULL, (void*)curFontSize);
	}
	wListSetIndex(list, curInx);
	wFlush();
}

long GetFontSize(wIndex_t inx)
{
	return(fontSizeList[inx]);
}

long GetFontSizeIndex(long size)
{
	int i;
	for (i = 0; i < sizeof fontSizeList / sizeof fontSizeList[0]; i++)
	{
		if (fontSizeList[i] == size)
			return(i);
	}
	return(-1);
}

EXPORT void UpdateFontSizeList(
		long * fontSizeR,
		wList_p list,
		wIndex_t listInx )
{
	long fontSize;

	if ( listInx >= 0 ) {
		*fontSizeR = (long)wListGetItemContext( list, listInx );
	} else {
		wListGetValues( list, message, sizeof message, NULL, NULL );
		if ( message[0] != '\0' ) {
			fontSize = atol( message );
			if ( fontSize <= 0 ) {
				NoticeMessage( _("Font Size must be > 0"), _("Ok"), NULL );
				sprintf( message, "%ld", *fontSizeR );
				wListSetValue( list, message );
			} else {
				if ( fontSize <= 500 || NoticeMessage( MSG_LARGE_FONT, _("Yes"), _("No") ) > 0 ) {
				
					*fontSizeR = fontSize;
					/* inform gtkfont dialog from change */
					wSetSelectedFontSize((int)fontSize);
					/*LoadFontSizeList( list, *fontSizeR );*/
				} else {
					sprintf( message, "%ld", *fontSizeR );
					wListSetValue( list, message );
				}
			}
		}
	}
}

/*******************************************************************************
 *
 * DRAW
 *
 */


struct extraData {
		coOrd orig;
		ANGLE_T angle;
		drawLineType_e lineType;
		wIndex_t segCnt;
		trkSeg_t segs[1];
		};

static TRKTYP_T T_DRAW = -1;
static track_p ignoredTableEdge;
static track_p ignoredDraw;


static void ComputeDrawBoundingBox( track_p t )
{
	struct extraData * xx = GetTrkExtraData(t);
	coOrd lo, hi;

	GetSegBounds( xx->orig, xx->angle, xx->segCnt, xx->segs, &lo, &hi );
	hi.x += lo.x;
	hi.y += lo.y;
	SetBoundingBox( t, hi, lo );
}


static track_p MakeDrawFromSeg1(
		wIndex_t index,
		coOrd pos,
		ANGLE_T angle,
		trkSeg_p sp )
{
	struct extraData * xx;
	track_p trk;
	if ( sp->type == ' ' )
		return NULL;
	trk = NewTrack( index, T_DRAW, 0, sizeof *xx );
	xx = GetTrkExtraData( trk );
	xx->orig = pos;
	xx->angle = angle;
	xx->segCnt = 1;
	xx->lineType = DRAWLINESOLID;
	memcpy( xx->segs, sp, sizeof *(trkSeg_p)0 );

	if (xx->segs[0].type == SEG_POLY ||
		xx->segs[0].type == SEG_FILPOLY ) {
		xx->segs[0].u.p.pts = (pts_t*)MyMalloc( (sp->u.p.cnt) * sizeof (pts_t) );
		memcpy(xx->segs[0].u.p.pts, sp->u.p.pts, sp->u.p.cnt * sizeof (pts_t) );
	}
	if (xx->segs[0].type == SEG_TEXT) {
		xx->segs[0].u.t.string = MyStrdup(sp->u.t.string);
	}
	ComputeDrawBoundingBox( trk );
	return trk;
}

EXPORT track_p MakeDrawFromSeg(
		coOrd pos,
		ANGLE_T angle,
		trkSeg_p sp )
{
	return MakeDrawFromSeg1( 0, pos, angle, sp );
}

/* Only straight, curved and PolyLine */
EXPORT track_p MakePolyLineFromSegs(
		coOrd pos,
		ANGLE_T angle,
		dynArr_t * segsArr)
{
	struct extraData * xx;
	track_p trk;
	trk = NewTrack( 0, T_DRAW, 0, sizeof *xx );
	xx = GetTrkExtraData( trk );
	xx->orig = pos;
	xx->angle = angle;
	xx->lineType = DRAWLINESOLID;
	xx->segCnt = 1;
	xx->segs[0].type = SEG_POLY;
	xx->segs[0].width = 0;
	xx->segs[0].u.p.polyType = POLYLINE;
	xx->segs[0].color = wDrawColorBlack;
	coOrd last;
	BOOL_T first = TRUE;
	int cnt = 0;
	for (int i=0;i<segsArr->cnt;i++) {
		trkSeg_p sp = &DYNARR_N(trkSeg_t,*segsArr,i);
		if (sp->type == SEG_BEZLIN || sp->type == SEG_BEZTRK ) {
			for (int j=0;j<sp->bezSegs.cnt;j++) {
				trkSeg_p spb = &DYNARR_N(trkSeg_t,sp->bezSegs,j);
				if (spb->type == SEG_STRLIN || spb->type == SEG_STRTRK) {
					if (!first && IsClose(FindDistance(spb->u.l.pos[0], last)))
						cnt++;
					else
						cnt=cnt+2;
					last = spb->u.l.pos[1];
					first = FALSE;
				}
				else if (spb->type == SEG_CRVLIN || spb->type == SEG_CRVTRK) {
					coOrd this;
					if (spb->u.c.radius > 0)
						Translate(&this, spb->u.c.center, spb->u.c.a0, fabs(spb->u.c.radius));
					else
						Translate(&this, spb->u.c.center, spb->u.c.a0+spb->u.c.a1, fabs(spb->u.c.radius));
					if (first || !IsClose(FindDistance(this, last))) {
						cnt++;									//Add first point
					}
					cnt += (int)floor(spb->u.c.a1/22.5)+1 ;				//Add a point for each 1/8 of a circle
					if (spb->u.c.radius > 0)
						Translate(&last, spb->u.c.center, spb->u.c.a0+spb->u.c.a1, fabs(spb->u.c.radius));
					else
						Translate(&last, spb->u.c.center, spb->u.c.a0, fabs(spb->u.c.radius));
					first = FALSE;
				}
			}
		}
		else if (sp->type == SEG_STRLIN || sp->type == SEG_STRTRK) {
			if (!first && IsClose(FindDistance(sp->u.l.pos[0], last)))
				cnt++;
			else
				cnt=cnt+2;
			last = sp->u.l.pos[1];
			first = FALSE;
		}
		else if (sp->type == SEG_CRVLIN || sp->type == SEG_CRVTRK) {
			coOrd this;
			if (sp->u.c.radius > 0)
				Translate(&this, sp->u.c.center, sp->u.c.a0, fabs(sp->u.c.radius));
			else
				Translate(&this, sp->u.c.center, sp->u.c.a0+sp->u.c.a1, fabs(sp->u.c.radius));
			if (first || !IsClose(FindDistance(this, last))) {
				cnt++;									//Add first point
			}
			cnt += (int)floor(sp->u.c.a1/22.5)+1 ;				//Add a point for each 1/8 of a circle
			if (sp->u.c.radius > 0)
				Translate(&last, sp->u.c.center, sp->u.c.a0+sp->u.c.a1, fabs(sp->u.c.radius));
			else
				Translate(&last, sp->u.c.center, sp->u.c.a0, fabs(sp->u.c.radius));
			first = FALSE;
		}
		else if (sp->type == SEG_POLY) {
			if (!first && IsClose(FindDistance(sp->u.p.pts[0].pt, last)))
				cnt = cnt + sp->u.p.cnt-1;
			else
				cnt = cnt + sp->u.p.cnt;
			last = sp->u.p.pts[sp->u.p.cnt-1].pt;
			first = FALSE;
		}
	}
	xx->segs[0].u.p.cnt = cnt;
	xx->segs[0].u.p.pts = (pts_t*)MyMalloc( (cnt) * sizeof (pts_t) );
	first = TRUE;
	int j =0;
	for (int i=0;i<segsArr->cnt;i++) {
		trkSeg_p sp = &DYNARR_N(trkSeg_t,*segsArr,i);
		if (sp->type == SEG_BEZLIN || sp->type == SEG_BEZTRK ) {
			for (int l=0;l<sp->bezSegs.cnt;l++) {
				trkSeg_p spb = &DYNARR_N(trkSeg_t,sp->bezSegs,l);
				if (spb->type == SEG_STRLIN || spb->type == SEG_STRTRK) {
					if (first || !IsClose(FindDistance(spb->u.l.pos[0], last))) {
						xx->segs[0].u.p.pts[j].pt = spb->u.l.pos[0];
						xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
						j++;
					}
					xx->segs[0].u.p.pts[j].pt = last = spb->u.l.pos[1];
					xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
					last = xx->segs[0].u.p.pts[j].pt;
					j ++;
					first = FALSE;
				}
				if (spb->type == SEG_CRVLIN || spb->type == SEG_CRVTRK) {
					coOrd this;
					if (spb->u.c.radius>0)
						Translate(&this, spb->u.c.center, spb->u.c.a0, fabs(spb->u.c.radius));
					else
						Translate(&this, spb->u.c.center, spb->u.c.a0+spb->u.c.a1, fabs(spb->u.c.radius));
					if (first || !IsClose(FindDistance(this, last))) {
						xx->segs[0].u.p.pts[j].pt= this;
						xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
						j++;
					}
					int slices = (int)floor(spb->u.c.a1/22.5);
					for (int k=1; k<=slices;k++) {
						if (spb->u.c.radius>0)
							Translate(&xx->segs[0].u.p.pts[j].pt, spb->u.c.center, spb->u.c.a0+(k*(spb->u.c.a1/(slices+1))), fabs(spb->u.c.radius));
						else
							Translate(&xx->segs[0].u.p.pts[j].pt, spb->u.c.center, spb->u.c.a0+((slices+1-k)*(spb->u.c.a1/(slices+1))), fabs(spb->u.c.radius));
						xx->segs[0].u.p.pts[j].pt_type = wPolyLineSmooth;
						j++;
					}
					if (spb->u.c.radius>0)
						Translate(&xx->segs[0].u.p.pts[j].pt, spb->u.c.center, spb->u.c.a0+spb->u.c.a1, fabs(spb->u.c.radius));
					else
						Translate(&xx->segs[0].u.p.pts[j].pt, spb->u.c.center, spb->u.c.a0, fabs(spb->u.c.radius));

					xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
					last = xx->segs[0].u.p.pts[j].pt;
					j++;
					first = FALSE;
				}
			}
		}
		if (sp->type == SEG_STRLIN || sp->type == SEG_STRTRK) {
			if (first || !IsClose(FindDistance(sp->u.l.pos[0], last))) {
				xx->segs[0].u.p.pts[j].pt = sp->u.l.pos[0];
				xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
				j++;
			}
			xx->segs[0].u.p.pts[j].pt = last = sp->u.l.pos[1];
			xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
			last = xx->segs[0].u.p.pts[j].pt;
			j++;
			first = FALSE;
		}
		if (sp->type == SEG_CRVLIN || sp->type == SEG_CRVTRK) {
			coOrd this;
			if (sp->u.c.radius>0)
				Translate(&this, sp->u.c.center, sp->u.c.a0, fabs(sp->u.c.radius));
			else
				Translate(&this, sp->u.c.center, sp->u.c.a0+sp->u.c.a1, fabs(sp->u.c.radius));
			if (first || !IsClose(FindDistance(this, last))) {
				xx->segs[0].u.p.pts[j].pt= this;
				xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
				j++;
			}
			int slices = (int)floor(sp->u.c.a1/22.5);
			for (int k=1; k<=slices;k++) {
				if (sp->u.c.radius>0)
					Translate(&xx->segs[0].u.p.pts[j].pt, sp->u.c.center, sp->u.c.a0+(k*(sp->u.c.a1/(slices+1))), fabs(sp->u.c.radius));
				else
					Translate(&xx->segs[0].u.p.pts[j].pt, sp->u.c.center, sp->u.c.a0+((slices+1-k)*(sp->u.c.a1/(slices+1))), fabs(sp->u.c.radius));
				xx->segs[0].u.p.pts[j].pt_type = wPolyLineSmooth;
				j++;
			}
			if (sp->u.c.radius>0)
				Translate(&xx->segs[0].u.p.pts[j].pt, sp->u.c.center, sp->u.c.a0+sp->u.c.a1, fabs(sp->u.c.radius));
			else
				Translate(&xx->segs[0].u.p.pts[j].pt, sp->u.c.center, sp->u.c.a0, fabs(sp->u.c.radius));

			xx->segs[0].u.p.pts[j].pt_type = wPolyLineStraight;
			last = xx->segs[0].u.p.pts[j].pt;
			j++;
			first = FALSE;
		}
		if (sp->type == SEG_POLY) {
			if (first || !IsClose(FindDistance(xx->segs[0].u.p.pts[j].pt, last))) {
				xx->segs[0].u.p.pts[j] = sp->u.p.pts[0];
				j++;
			}
			memcpy(&xx->segs[0].u.p.pts[j],&sp->u.p.pts[1], sp->u.p.cnt-1 * sizeof (pts_t));
			last = xx->segs[0].u.p.pts[sp->u.p.cnt-1].pt;
			j +=sp->u.p.cnt;
			first = FALSE;
		}
		ASSERT(j<=cnt);

	}

	ComputeDrawBoundingBox( trk );
	return trk;
}


static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

void static CreateOriginAnchor(coOrd origin, wBool_t trans_selected) {
		double d = tempD.scale*0.15;
		DYNARR_APPEND(trkSeg_t,anchors_da,2);
		int i = anchors_da.cnt-1;
		coOrd p0,p1;
		Translate(&p0,origin,0,d*4);
		Translate(&p1,origin,0,-d*4);
		anchors(i).type = SEG_STRLIN;
		anchors(i).u.l.pos[0] = p0;
		anchors(i).u.l.pos[1] = p1;
		anchors(i).color = wDrawColorBlue;
		anchors(i).width = 0;
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		Translate(&p0,origin,90,d*4);
		Translate(&p1,origin,90,-d*4);
		i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).u.l.pos[0] = p0;
		anchors(i).u.l.pos[1] = p1;
		anchors(i).color = wDrawColorBlue;
		anchors(i).width = 0;
}

EXPORT void DrawOriginAnchor(track_p trk) {
	if (!trk || GetTrkType(trk) != T_DRAW) return;
	struct extraData * xx = GetTrkExtraData(trk);
	if ((xx->orig.x != 0.0) || (xx->orig.y !=0.0) ) {
		DYNARR_RESET(trkSeg_t,anchors_da);
		CreateOriginAnchor(xx->orig,FALSE);
		DrawSegs(&tempD, zero, 0.0, anchors_da.ptr, anchors_da.cnt, trackGauge, wDrawColorBlue);
	}
}



static DIST_T DistanceDraw( track_p t, coOrd * p )
{
	struct extraData * xx = GetTrkExtraData(t);
	if ( ignoredTableEdge == t && xx->segs[0].type == SEG_TBLEDGE )
		return 100000.0;
	if ( ignoredDraw == t )
		return 100000.0;
	return DistanceSegs( xx->orig, xx->angle, xx->segCnt, xx->segs, p, NULL );
}


static struct {
		coOrd endPt[4];
		coOrd origin;
		coOrd oldOrigin;
		coOrd oldE0;
		coOrd oldE1;
		FLOAT_T length;
		FLOAT_T height;
		FLOAT_T width;
		coOrd center;
		DIST_T radius;
		ANGLE_T angle0;
		ANGLE_T angle1;
		ANGLE_T angle;
		ANGLE_T rotate_angle;
		ANGLE_T oldAngle;
		long pointCount;
		long lineWidth;
		BOOL_T boxed;
		BOOL_T filled;
		BOOL_T open;
		BOOL_T lock_origin;
		wDrawColor color;
		wIndex_t benchChoice;
		wIndex_t benchOrient;
		wIndex_t dimenSize;
		descPivot_t pivot;
		wIndex_t fontSizeInx;
		char text[STR_LONG_SIZE];
		unsigned int layer;
		wIndex_t lineType;
		} drawData;
typedef enum { E0, E1, PP, CE, AL, A1, A2, RD, LN, HT, WT, LK, OI, RA, VC, LW, LT, CO, FL, OP, BX, BE, OR, DS, TP, TA, TS, TX, PV, LY } drawDesc_e;
static descData_t drawDesc[] = {
/*E0*/	{ DESC_POS, N_("End Pt 1: X,Y"), &drawData.endPt[0] },
/*E1*/	{ DESC_POS, N_("End Pt 2: X,Y"), &drawData.endPt[1] },
/*PP*/  { DESC_POS, N_("First Point: X,Y"), &drawData.endPt[0] },
/*CE*/	{ DESC_POS, N_("Center: X,Y"), &drawData.center },
/*AL*/	{ DESC_FLOAT, N_("Angle"), &drawData.angle },
/*A1*/	{ DESC_ANGLE, N_("CCW Angle"), &drawData.angle0 },
/*A2*/	{ DESC_ANGLE, N_("CW Angle"), &drawData.angle1 },
/*RD*/	{ DESC_DIM, N_("Radius"), &drawData.radius },
/*LN*/	{ DESC_DIM, N_("Length"), &drawData.length },
/*HT*/  { DESC_DIM, N_("Height"), &drawData.height },
/*WT*/ 	{ DESC_DIM, N_("Width"), &drawData.width },
/*LK*/  { DESC_BOXED, N_("Lock Origin Offset"), &drawData.lock_origin},
/*OI*/  { DESC_POS, N_("Rot Origin: X,Y"), &drawData.origin },
/*RA*/  { DESC_FLOAT, N_("Rotate Angle"), &drawData.angle },
/*VC*/	{ DESC_LONG, N_("Point Count"), &drawData.pointCount },
/*LW*/	{ DESC_LONG, N_("Line Width"), &drawData.lineWidth },
/*LT*/  { DESC_LIST, N_("Line Type"), &drawData.lineType },
/*CO*/	{ DESC_COLOR, N_("Color"), &drawData.color },
/*FL*/	{ DESC_BOXED, N_("Filled"), &drawData.filled },
/*OP*/  { DESC_BOXED, N_("Open End"), &drawData.open },
/*BX*/  { DESC_BOXED, N_("Boxed"), &drawData.boxed },
/*BE*/	{ DESC_LIST, N_("Lumber"), &drawData.benchChoice },
/*OR*/	{ DESC_LIST, N_("Orientation"), &drawData.benchOrient },
/*DS*/	{ DESC_LIST, N_("Size"), &drawData.dimenSize },
/*TP*/	{ DESC_POS, N_("Origin: X,Y"), &drawData.endPt[0] },
/*TA*/	{ DESC_FLOAT, N_("Angle"), &drawData.angle },
/*TS*/	{ DESC_EDITABLELIST, N_("Font Size"), &drawData.fontSizeInx },
/*TX*/	{ DESC_TEXT, N_("Text"), &drawData.text },
/*PV*/	{ DESC_PIVOT, N_("Pivot"), &drawData.pivot },
/*LY*/	{ DESC_LAYER, N_("Layer"), &drawData.layer },
		{ DESC_NULL } };
static int drawSegInx;

#define UNREORIGIN( Q, P, A, O ) { \
		(Q) = (P); \
		(Q).x -= (O).x; \
		(Q).y -= (O).y; \
		if ( (A) != 0.0 ) \
			Rotate( &(Q), zero, -(A) ); \
		}

/*
 * Notes -
 *
 * In V5.1, Origin was always {0,0} and Angle 0.0 after editing a Draw object.
 * This did not allow for the use of the objects in other contexts (such as Signal Arms).
 *
 * In V5.2 -
 *
 * OI - Origin will be adjusted if it is locked to remain relative to the end point - this equally applies when moving the object points.
 * If not locked, the object points will be set relative to the new origin value,
 * so that the object remains at the same place as the user specifies.
 * If the edit starts with origin {0,0}, it will be set unlocked, otherwise set locked.
 *
 * AL- Angle will be set to 0.0 when the object is modified. The points of the objects will be rotated so that
 * rotated and adjusted so they don't need rotation to lie where the user left them.
 *
 */
static void UpdateDraw( track_p trk, int inx, descData_p descUpd, BOOL_T final )
{
	struct extraData *xx = GetTrkExtraData(trk);
	trkSeg_p segPtr;
	coOrd mid;
	long fontSize;

	if ( drawSegInx==-1 )
		return;
	segPtr = &xx->segs[drawSegInx];
	if ( inx == -1 ) {
		if (segPtr->type != SEG_TEXT) return;
		else inx = TX;  //Always look at TextField for SEG_TEXT on "Done"
	}
	UndrawNewTrack( trk );
	coOrd pt;
	coOrd off;
	switch ( inx ) {
	case LW:
		segPtr->width = drawData.lineWidth/mainD.dpi;
		break;
	case CO:
		segPtr->color = drawData.color;
		break;
	case E0:
	case E1:
		if ( inx == E0 ) {
			coOrd off;
			off.x = drawData.endPt[0].x - drawData.oldE0.x;
			off.y = drawData.endPt[0].y - drawData.oldE0.y;
			if (drawData.lock_origin) {
				xx->orig.x +=off.x;
				xx->orig.y +=off.y;
				drawDesc[OI].mode |= DESC_CHANGE;
			} else {
				switch(segPtr->type) {					//E0 does not alter length - translates
					case SEG_STRLIN:
					case SEG_DIMLIN:
					case SEG_BENCH:
					case SEG_TBLEDGE:
						UNREORIGIN( segPtr->u.l.pos[0], drawData.endPt[0], xx->angle, xx->orig );
						drawData.endPt[1].x = off.x+drawData.endPt[1].x;
						drawData.endPt[1].y = off.y+drawData.endPt[1].y;
						UNREORIGIN( segPtr->u.l.pos[1], drawData.endPt[1], xx->angle, xx->orig );
						drawDesc[E1].mode |= DESC_CHANGE;
						break;
					case SEG_CRVLIN:
					case SEG_FILCRCL:
						UNREORIGIN( segPtr->u.c.center, drawData.endPt[0], xx->angle, xx->orig );
						break;
					case SEG_TEXT:
						UNREORIGIN( segPtr->u.t.pos, drawData.endPt[0], xx->angle, xx->orig );
						break;
					case SEG_POLY:
					case SEG_FILPOLY:
						break;  //Note not used by POLYGONS
					default:;
				}
			}
		} else {												//E1 - alters length
			off.x = drawData.endPt[1].x - drawData.oldE1.x;
			off.y = drawData.endPt[1].y - drawData.oldE1.y;
			drawDesc[E1].mode |= DESC_CHANGE;
			if (drawData.lock_origin) {
				xx->orig.x +=off.x;
				xx->orig.y +=off.y;
				drawDesc[OI].mode |= DESC_CHANGE;
			} else {
				UNREORIGIN( segPtr->u.l.pos[1], drawData.endPt[1], xx->angle, xx->orig );
			}
		}
		drawData.length = FindDistance( drawData.endPt[0], drawData.endPt[1] );
		drawDesc[LN].mode |= DESC_CHANGE;
		break;
	case OI:
		off.x = drawData.origin.x - drawData.oldOrigin.x;
		off.y = drawData.origin.y - drawData.oldOrigin.y;
		xx->orig = drawData.origin;
		if (!drawData.lock_origin)  {
			switch(segPtr->type) {
				case SEG_POLY:
				case SEG_FILPOLY:
					for (int i=0;i<segPtr->u.p.cnt;i++) {
						REORIGIN(   pt, segPtr->u.p.pts[i].pt, xx->angle, drawData.oldOrigin);
						UNREORIGIN( segPtr->u.p.pts[i].pt, pt, xx->angle, xx->orig );
					}
				break;
				case SEG_STRLIN:
				case SEG_DIMLIN:
				case SEG_BENCH:
				case SEG_TBLEDGE:
					for (int i=0;i<2;i++) {
						UNREORIGIN( segPtr->u.l.pos[i], drawData.endPt[i], xx->angle, xx->orig );
					}
					break;
				case SEG_CRVLIN:
				case SEG_FILCRCL:
					UNREORIGIN( segPtr->u.c.center, drawData.center, xx->angle, xx->orig );
					break;
				case SEG_TEXT:
					UNREORIGIN( segPtr->u.t.pos, drawData.endPt[0], xx->angle, xx->orig );
					break;
				default:;
			}
		} else {
			drawData.endPt[0].x += off.x;
			drawData.endPt[0].y += off.y;
			switch(segPtr->type) {
				case SEG_STRLIN:
				case SEG_DIMLIN:
				case SEG_BENCH:
				case SEG_TBLEDGE:
					drawDesc[E0].mode |= DESC_CHANGE;
					UNREORIGIN( segPtr->u.l.pos[0], drawData.endPt[0], xx->angle, xx->orig );
					drawData.endPt[1].x = off.x+drawData.endPt[1].x;
					drawData.endPt[1].y = off.y+drawData.endPt[1].y;
					UNREORIGIN( segPtr->u.l.pos[1], drawData.endPt[1], xx->angle, xx->orig );
					drawDesc[E1].mode |= DESC_CHANGE;
					break;
				case SEG_CRVLIN:
				case SEG_FILCRCL:
					UNREORIGIN( segPtr->u.c.center, drawData.endPt[0], xx->angle, xx->orig );
					drawDesc[E0].mode |= DESC_CHANGE;
					break;
				case SEG_TEXT:
					UNREORIGIN( segPtr->u.t.pos, drawData.endPt[0], xx->angle, xx->orig );
					drawDesc[E0].mode |= DESC_CHANGE;
					break;
				case SEG_POLY:
				case SEG_FILPOLY:
					for (int i=0;i<segPtr->u.p.cnt;i++) {
						REORIGIN(   pt, segPtr->u.p.pts[i].pt, xx->angle, drawData.oldOrigin);
						pt.x += off.x;
						pt.y += off.y;
						UNREORIGIN( segPtr->u.p.pts[i].pt, pt, xx->angle, xx->orig );
					}
					drawDesc[PP].mode |= DESC_CHANGE;
					break;
				default:;
			}
		}
		break;
	case HT:
	case WT:
		if ((segPtr->type == SEG_POLY) || (segPtr->type == SEG_FILPOLY)) {
			if (segPtr->u.p.polyType == RECTANGLE) {
				if (inx == HT) {
					ANGLE_T angle = NormalizeAngle(FindAngle(drawData.endPt[0],drawData.endPt[3]));
					Translate( &drawData.endPt[3], drawData.endPt[0], angle, drawData.height);
					UNREORIGIN( segPtr->u.p.pts[3].pt, drawData.endPt[3], xx->angle, xx->orig );
					Translate( &drawData.endPt[2], drawData.endPt[1], angle, drawData.height);
					UNREORIGIN( segPtr->u.p.pts[2].pt, drawData.endPt[2], xx->angle, xx->orig );
				} else {
					ANGLE_T angle = NormalizeAngle(FindAngle(drawData.endPt[0],drawData.endPt[1]));;
					Translate( &drawData.endPt[1], drawData.endPt[0], angle, drawData.width);
					UNREORIGIN( segPtr->u.p.pts[1].pt, drawData.endPt[1], xx->angle, xx->orig );
					Translate( &drawData.endPt[2], drawData.endPt[3], angle, drawData.width);
					UNREORIGIN( segPtr->u.p.pts[2].pt, drawData.endPt[2], xx->angle, xx->orig );
				}
				drawDesc[E0].mode |= DESC_CHANGE;
			}
		}
		break;
	case RA:;
		ANGLE_T angle = NormalizeAngle(drawData.rotate_angle);
		switch(segPtr->type) {
			case SEG_POLY:
			case SEG_FILPOLY:
				for (int i=0;i<segPtr->u.p.cnt;i++) {
					REORIGIN(pt,segPtr->u.p.pts[i].pt, angle, xx->orig);
					if (i == 0) drawData.endPt[0] = pt;
					UNREORIGIN(segPtr->u.p.pts[i].pt, pt, 0.0, xx->orig);
				}
				drawDesc[PP].mode |= DESC_CHANGE;
				break;
			case SEG_CRVLIN:;
				coOrd end0, end1;
				Translate(&end0,segPtr->u.c.center,segPtr->u.c.a0,segPtr->u.c.radius);
				Translate(&end1,segPtr->u.c.center,segPtr->u.c.a0+segPtr->u.c.a1,segPtr->u.c.radius);
				REORIGIN(end0, end0, angle, xx->orig );
				REORIGIN(end1, end1, angle, xx->orig );
				REORIGIN( drawData.center,segPtr->u.c.center, angle, xx->orig );
				drawData.angle0 = FindAngle( drawData.center, end0);
				drawData.angle1 = FindAngle( drawData.center, end1);
				drawDesc[CE].mode |= DESC_CHANGE;
				drawDesc[A1].mode |= DESC_CHANGE;
				drawDesc[A2].mode |= DESC_CHANGE;
				/*no break*/
			case SEG_FILCRCL:
				REORIGIN( drawData.center,segPtr->u.c.center, angle, xx->orig );
				UNREORIGIN( segPtr->u.c.center, drawData.center, 0.0, xx->orig);  //Remove angle
				drawDesc[CE].mode |= DESC_CHANGE;
				break;
			case SEG_STRLIN:
			case SEG_DIMLIN:
			case SEG_BENCH:
			case SEG_TBLEDGE:
				for (int i=0;i<2;i++) {
					REORIGIN( drawData.endPt[i], segPtr->u.l.pos[i], angle, xx->orig );
					UNREORIGIN(segPtr->u.l.pos[i], drawData.endPt[i], 0.0, xx->orig );
				}
				drawDesc[E0].mode |= DESC_CHANGE;
				drawDesc[E1].mode |= DESC_CHANGE;
				break;
			case SEG_TEXT:

				break;
			default:;
		}
		xx->angle = drawData.rotate_angle = 0.0;
		drawDesc[RA].mode |= DESC_CHANGE;
		break;
	case AL:;
		angle = NormalizeAngle(drawData.angle);
		switch(segPtr->type) {
			case SEG_POLY:
			case SEG_FILPOLY:
				break;			//Doesn't Use
			case SEG_CRVLIN:;
				break;			//Doesn't Use
			case SEG_FILCRCL:
				break;			//Doesn't Use
			case SEG_STRLIN:
			case SEG_DIMLIN:
			case SEG_BENCH:
			case SEG_TBLEDGE:
				Translate(&drawData.endPt[1],drawData.endPt[0],angle,drawData.length);
				UNREORIGIN(segPtr->u.l.pos[1], drawData.endPt[1], xx->angle, xx->orig );
				drawDesc[E1].mode |= DESC_CHANGE;
				break;
			case SEG_TEXT:
			    break; //Doesnt Use
			default:;
		}
		break;
	case LN:
		if ( drawData.length <= minLength ) {
			ErrorMessage( MSG_OBJECT_TOO_SHORT );
			if ( segPtr->type != SEG_CRVLIN ) {
				drawData.length = FindDistance( drawData.endPt[0], drawData.endPt[1] );
			} else {
				drawData.length = fabs(segPtr->u.c.radius)*2*M_PI*segPtr->u.c.a1/360.0;
			}
			drawDesc[LN].mode |= DESC_CHANGE;
			break;
		}
		if ( segPtr->type != SEG_CRVLIN  ) {
			switch ( drawData.pivot ) {
			case DESC_PIVOT_FIRST:
				Translate( &drawData.endPt[1], drawData.endPt[0], drawData.angle, drawData.length );
				UNREORIGIN( segPtr->u.l.pos[1], drawData.endPt[1], xx->angle, xx->orig );
				drawDesc[E1].mode |= DESC_CHANGE;
				break;
			case DESC_PIVOT_SECOND:
				Translate( &drawData.endPt[0], drawData.endPt[1], drawData.angle+180.0, drawData.length );
				UNREORIGIN( segPtr->u.l.pos[0], drawData.endPt[0], xx->angle, xx->orig );
				drawDesc[E0].mode |= DESC_CHANGE;
				break;
			case DESC_PIVOT_MID:
				mid.x = (drawData.endPt[0].x+drawData.endPt[1].x)/2.0;
				mid.y = (drawData.endPt[0].y+drawData.endPt[1].y)/2.0;
				Translate( &drawData.endPt[0], mid, drawData.angle+180.0, drawData.length/2.0 );
				Translate( &drawData.endPt[1], mid, drawData.angle, drawData.length/2.0 );
				UNREORIGIN( segPtr->u.l.pos[0], drawData.endPt[0], xx->angle, xx->orig );
				UNREORIGIN( segPtr->u.l.pos[1], drawData.endPt[1], xx->angle, xx->orig );
				drawDesc[E0].mode |= DESC_CHANGE;
				drawDesc[E1].mode |= DESC_CHANGE;
				break;
			default:
				break;
			}
		} else {

			if ( drawData.angle < 0.0 || drawData.angle >= 360.0 ) {
				ErrorMessage( MSG_CURVE_OUT_OF_RANGE );
				drawData.angle = segPtr->u.c.a1;
				drawDesc[AL].mode |= DESC_CHANGE;
			} else {
				segPtr->u.c.a0 = NormalizeAngle( segPtr->u.c.a0+segPtr->u.c.a1/2.0-drawData.angle/2.0);
				segPtr->u.c.a1 = drawData.angle;
				drawData.angle0 = NormalizeAngle( segPtr->u.c.a0+xx->angle );
				drawData.angle1 = NormalizeAngle( drawData.angle0+segPtr->u.c.a1 );
				drawDesc[A1].mode |= DESC_CHANGE;
				drawDesc[A2].mode |= DESC_CHANGE;
			}
		}
		break;
	case CE:
		UNREORIGIN( segPtr->u.c.center, drawData.center, xx->angle, xx->orig );
		break;
	case RD:
		if ( drawData.pivot == DESC_PIVOT_FIRST ) {
			Translate( &segPtr->u.c.center, segPtr->u.c.center, segPtr->u.c.a0, segPtr->u.c.radius-drawData.radius );
		} else if ( drawData.pivot == DESC_PIVOT_SECOND ) {
			Translate( &segPtr->u.c.center, segPtr->u.c.center, segPtr->u.c.a0+segPtr->u.c.a1, segPtr->u.c.radius-drawData.radius );
		} else {
			Translate( &segPtr->u.c.center, segPtr->u.c.center, (segPtr->u.c.a0+segPtr->u.c.a1)/2.0, segPtr->u.c.radius-drawData.radius );
		}
		drawDesc[CE].mode |= DESC_CHANGE;
		segPtr->u.c.radius = drawData.radius;
		drawDesc[LN].mode |= DESC_CHANGE;
		break;
	case A1:
		switch ( drawData.pivot ) {
			case DESC_PIVOT_FIRST:
				segPtr->u.c.a1 = drawData.angle;
				drawData.angle1 = NormalizeAngle( drawData.angle0+segPtr->u.c.a1 );
				drawDesc[A2].mode |= DESC_CHANGE;
				break;
			case DESC_PIVOT_SECOND:
				segPtr->u.c.a0 = NormalizeAngle( segPtr->u.c.a1+segPtr->u.c.a0-drawData.angle);
				segPtr->u.c.a1 = drawData.angle;
				drawData.angle0 = NormalizeAngle( segPtr->u.c.a0+xx->angle );
				drawData.angle1 = NormalizeAngle( drawData.angle0+segPtr->u.c.a1 );
				drawDesc[A1].mode |= DESC_CHANGE;
				drawDesc[A2].mode |= DESC_CHANGE;
				break;
			case DESC_PIVOT_MID:
				segPtr->u.c.a0 = NormalizeAngle( segPtr->u.c.a0+segPtr->u.c.a1/2.0-drawData.angle/2.0);
				segPtr->u.c.a1 = drawData.angle;
				drawData.angle0 = NormalizeAngle( segPtr->u.c.a0+xx->angle );
				drawData.angle1 = NormalizeAngle( drawData.angle0+segPtr->u.c.a1 );
				drawDesc[A1].mode |= DESC_CHANGE;
				drawDesc[A2].mode |= DESC_CHANGE;
				break;
			default:
				break;
		}
		break;
	case A2:
		segPtr->u.c.a0 = NormalizeAngle( drawData.angle1-segPtr->u.c.a1-xx->angle );
		drawData.angle0 = NormalizeAngle( segPtr->u.c.a0+xx->angle );
		drawDesc[A1].mode |= DESC_CHANGE;
		break;
	case BE:
		BenchUpdateOrientationList( (long)wListGetItemContext((wList_p)drawDesc[BE].control0, drawData.benchChoice ), (wList_p)drawDesc[OR].control0 );
		if ( drawData.benchOrient < wListGetCount( (wList_p)drawDesc[OR].control0 ) )
			wListSetIndex( (wList_p)drawDesc[OR].control0, drawData.benchOrient );
		else
			drawData.benchOrient = 0;
		segPtr->u.l.option = GetBenchData( (long)wListGetItemContext((wList_p)drawDesc[BE].control0, drawData.benchChoice ), drawData.benchOrient );
		break;
	case OR:
		segPtr->u.l.option = GetBenchData( (long)wListGetItemContext((wList_p)drawDesc[BE].control0, drawData.benchChoice ), drawData.benchOrient );
		break;
	case DS:
		segPtr->u.l.option = drawData.dimenSize;
		break;
	case TP:
		UNREORIGIN( segPtr->u.t.pos, drawData.endPt[0], xx->angle, xx->orig );
		break;
	case PP:
		off.x = drawData.endPt[0].x - drawData.oldE0.x;
		off.y = drawData.endPt[0].y - drawData.oldE0.y;
		if (drawData.lock_origin) {
			xx->orig.x +=off.x;
			xx->orig.y +=off.y;
			drawData.origin = xx->orig;
			drawDesc[OI].mode |= DESC_CHANGE;
			drawDesc[E0].mode |= DESC_CHANGE;
			break;
		} else {
			for (int i=0;i<segPtr->u.p.cnt;i++) {
				REORIGIN( pt, segPtr->u.p.pts[i].pt, xx->angle, xx->orig );
				pt.x += off.x;
				pt.y += off.y;
				if (i<5) drawData.endPt[i] = pt;
				UNREORIGIN( segPtr->u.p.pts[i].pt, pt, 0.0, xx->orig );
			}
			xx->angle = 0.0;
			drawDesc[AL].mode |= DESC_CHANGE;
		}
		break;
	case TA:
		//segPtr->u.t.angle = NormalizeAngle( drawData.angle );
		xx->angle = NormalizeAngle( drawData.angle );
		break;
	case TS:
		fontSize = (long)segPtr->u.t.fontSize;
		UpdateFontSizeList( &fontSize, (wList_p)drawDesc[TS].control0, drawData.fontSizeInx );
		segPtr->u.t.fontSize = fontSize;
		break;
	case FL:
		if (segPtr->type == SEG_POLY && drawData.open) {
			drawData.filled = FALSE;
			drawDesc[FL].mode |= DESC_CHANGE;
			break;
		}
		if(drawData.filled) {
			if (segPtr->type == SEG_POLY) segPtr->type = SEG_FILPOLY;
			if (segPtr->type == SEG_CRVLIN) segPtr->type = SEG_FILCRCL;
		} else {
			if (segPtr->type == SEG_FILPOLY) segPtr->type = SEG_POLY;
			if (segPtr->type == SEG_FILCRCL) {
				segPtr->type = SEG_CRVLIN;
				segPtr->u.c.a0 = 0.0;
				segPtr->u.c.a1 = 360.0;
			}
		}
		break;
	case OP:
		if (drawData.filled || (segPtr->type != SEG_POLY)) {
			drawData.open = FALSE;
			drawDesc[OP].mode |= DESC_CHANGE;
			break;
		}
		if (drawData.open) {
			if (segPtr->type == SEG_POLY && segPtr->u.p.polyType == FREEFORM) segPtr->u.p.polyType = POLYLINE;
		} else {
			if (segPtr->type == SEG_POLY && segPtr->u.p.polyType == POLYLINE) segPtr->u.p.polyType = FREEFORM;
		}
		break;
	case BX:
		segPtr->u.t.boxed = drawData.boxed;
		break;
	case TX:
		if ( wTextGetModified((wText_p)drawDesc[TX].control0 )) {
			int len = wTextGetSize((wText_p)drawDesc[TX].control0);
			MyFree( segPtr->u.t.string );
			segPtr->u.t.string = (char *)MyMalloc(len+1);
			wTextGetText((wText_p)drawDesc[TX].control0, segPtr->u.t.string, len+1);
			segPtr->u.t.string[len] = '\0';				//Make sure of null term
		}

		break;
	case LY:
		SetTrkLayer( trk, drawData.layer);
		break;
	case LK:
		break;
	case LT:
		xx->lineType = drawData.lineType;
		break;
	default:
		AbortProg( "bad op" );
	}
	drawData.oldE0 = drawData.endPt[0];
	drawData.oldE1 = drawData.endPt[1];
	drawData.oldAngle = drawData.angle;
	drawData.oldOrigin = drawData.origin;
	ComputeDrawBoundingBox( trk );
	DrawNewTrack( trk );
	TempRedraw(); // UpdateDraw
}

extern BOOL_T inDescribeCmd;

static void DescribeDraw( track_p trk, char * str, CSIZE_T len )
{
	struct extraData *xx = GetTrkExtraData(trk);
	coOrd pos = oldMarker;
	trkSeg_p segPtr;
	int inx;
	char * title = NULL;
	char * polyType = NULL;


	DistanceSegs( xx->orig, xx->angle, xx->segCnt, xx->segs, &pos, &drawSegInx );
	if ( drawSegInx==-1 )
		return;
	segPtr = &xx->segs[drawSegInx];
	for ( inx=0; inx<sizeof drawDesc/sizeof drawDesc[0]; inx++ ) {
		drawDesc[inx].mode = DESC_IGNORE;
		drawDesc[inx].control0 = NULL;
	}
	drawData.color = segPtr->color;
	drawData.layer = GetTrkLayer(trk);
	drawDesc[CO].mode = 0;
	drawData.lineWidth = (long)floor(segPtr->width*mainD.dpi+0.5);
	drawDesc[LW].mode = 0;
	drawDesc[LY].mode = DESC_NOREDRAW;
	drawDesc[BE].mode =
	drawDesc[OR].mode =
	drawDesc[LT].mode =
	drawDesc[DS].mode = DESC_IGNORE;
	drawData.pivot = DESC_PIVOT_MID;

	if ((xx->orig.x == 0.0) && (xx->orig.y == 0.0)) drawData.lock_origin = FALSE;
	else drawData.lock_origin = TRUE;

	drawData.rotate_angle = xx->angle;

	drawDesc[LK].mode = 0;

	switch ( segPtr->type ) {
	case SEG_STRLIN:
	case SEG_DIMLIN:
	case SEG_BENCH:
	case SEG_TBLEDGE:
		REORIGIN( drawData.endPt[0], segPtr->u.l.pos[0], xx->angle, xx->orig );
		REORIGIN( drawData.endPt[1], segPtr->u.l.pos[1], xx->angle, xx->orig );
		drawData.length = FindDistance( drawData.endPt[0], drawData.endPt[1] );
		drawData.angle = FindAngle( drawData.endPt[0], drawData.endPt[1] );
		drawData.origin = xx->orig;
		drawDesc[LN].mode =
		drawDesc[AL].mode =
		drawDesc[PV].mode = 0;
		drawDesc[E0].mode =
		drawDesc[OI].mode = 0;
		drawDesc[E1].mode = 0;
		drawDesc[RA].mode = 0;
		switch (segPtr->type) {
		case SEG_STRLIN:
			title = _("Straight Line");
			drawDesc[LT].mode = 0;
			drawData.lineType = (wIndex_t)xx->lineType;
			break;
		case SEG_DIMLIN:
			title = _("Dimension Line");
			drawDesc[CO].mode =
			drawDesc[LW].mode =
			drawDesc[LK].mode =
			drawDesc[OI].mode =
			drawDesc[RA].mode = DESC_IGNORE;
			drawData.dimenSize = (wIndex_t)segPtr->u.l.option;
			drawDesc[DS].mode = 0;
			break;
		case SEG_BENCH:
			title = _("Lumber");
			drawDesc[LK].mode =
			drawDesc[OI].mode =
			drawDesc[RA].mode =
			drawDesc[LW].mode = DESC_IGNORE;
			drawDesc[BE].mode =
			drawDesc[OR].mode = 0;
			drawData.benchChoice = GetBenchListIndex( segPtr->u.l.option );
			drawData.benchOrient = (wIndex_t)(segPtr->u.l.option&0xFF);
			break;
		case SEG_TBLEDGE:
			title = _("Table Edge");
			drawDesc[LK].mode =
			drawDesc[OI].mode =
			drawDesc[RA].mode = DESC_IGNORE;
			drawDesc[CO].mode = DESC_IGNORE;
			drawDesc[LW].mode = DESC_IGNORE;
			break;
		}
		break;
	case SEG_CRVLIN:
		REORIGIN( drawData.center, segPtr->u.c.center, xx->angle, xx->orig );
		drawData.radius = fabs(segPtr->u.c.radius);
		drawData.origin = xx->orig;
		drawDesc[OI].mode = 0;
		drawDesc[RA].mode =
		drawDesc[CE].mode =
		drawDesc[RD].mode = 0;
		drawDesc[LT].mode = 0;
		drawData.lineType = (wIndex_t)xx->lineType;
		if ( segPtr->u.c.a1 >= 360.0 ) {
			title = _("Circle");
			drawDesc[FL].mode = 0;
			drawData.filled = FALSE;
		} else {
			drawData.angle = segPtr->u.c.a1;
			drawData.angle0 = NormalizeAngle( segPtr->u.c.a0+xx->angle );
			drawData.angle1 = NormalizeAngle( drawData.angle0+drawData.angle );
			drawDesc[A1].mode =
			drawDesc[A2].mode = 0;
			drawDesc[PV].mode = 0;
			title = _("Curved Line");
		}
		break;
	case SEG_FILCRCL:
		REORIGIN( drawData.center, segPtr->u.c.center, xx->angle, xx->orig );
		drawData.radius = fabs(segPtr->u.c.radius);
		drawData.origin = xx->orig;
		drawDesc[OI].mode =
		drawDesc[RA].mode =
		drawDesc[FL].mode = 0;
		drawData.filled = TRUE;
		drawDesc[CE].mode =
		drawDesc[RD].mode = 0;
		drawDesc[PV].mode = 0;
		drawDesc[OI].mode = 0;
		drawDesc[LW].mode = DESC_IGNORE;
		title = _("Filled Circle");
		break;
	case SEG_POLY:
		REORIGIN(drawData.endPt[0],segPtr->u.p.pts[0].pt, xx->angle, xx->orig);
		drawDesc[PP].mode = 0;
		drawData.pointCount = segPtr->u.p.cnt;
		drawDesc[VC].mode = DESC_RO;
		drawData.filled = FALSE;
		drawDesc[FL].mode = 0;
		drawData.angle = 0.0;
		drawDesc[RA].mode = 0;
		drawData.origin = xx->orig;
		drawDesc[OI].mode = 0;
		drawData.open=FALSE;
		drawDesc[OP].mode = 0;
		drawDesc[LT].mode = 0;
		drawData.lineType = (wIndex_t)xx->lineType;
		switch (segPtr->u.p.polyType) {
			case RECTANGLE:
				title = _("Rectangle");
				drawDesc[OP].mode = DESC_IGNORE;
				drawDesc[VC].mode = DESC_IGNORE;
				drawData.width = FindDistance(segPtr->u.p.pts[0].pt, segPtr->u.p.pts[1].pt);
				drawDesc[WT].mode = 0;
				drawData.height = FindDistance(segPtr->u.p.pts[0].pt, segPtr->u.p.pts[3].pt);
				drawDesc[HT].mode = 0;
				for(int i=0;i<4;i++) {
					REORIGIN( drawData.endPt[i], segPtr->u.p.pts[i].pt, xx->angle, xx->orig );
				}
				drawDesc[E0].mode = DESC_IGNORE;
				drawData.origin = xx->orig;
				break;
			case POLYLINE:
				title = _("Polyline");
				drawData.open=TRUE;
				break;
			default:
				title = _("Polygon");
		}
		break;
	case SEG_FILPOLY:
		REORIGIN(drawData.endPt[0],segPtr->u.p.pts[0].pt, xx->angle, xx->orig);
		drawDesc[PP].mode = 0;
		drawData.pointCount = segPtr->u.p.cnt;
		drawDesc[VC].mode = DESC_RO;
		drawData.filled = TRUE;
		drawDesc[FL].mode = 0;
		drawDesc[LW].mode = DESC_IGNORE;
		drawData.angle = xx->angle;
		drawDesc[RA].mode = 0;
		drawData.origin = xx->orig;
		drawDesc[OI].mode = DESC_RO;
		drawData.open = FALSE;
		switch (segPtr->u.p.polyType) {
			case RECTANGLE:
				title =_("Filled Rectangle");
				drawDesc[VC].mode = DESC_IGNORE;
				drawData.width = FindDistance(segPtr->u.p.pts[0].pt, segPtr->u.p.pts[1].pt);
				drawDesc[WT].mode = 0;
				drawData.height = FindDistance(segPtr->u.p.pts[0].pt, segPtr->u.p.pts[3].pt);
				drawDesc[HT].mode = 0;
				for(int i=0;i<4;i++) {
					REORIGIN( drawData.endPt[i], segPtr->u.p.pts[i].pt, xx->angle, xx->orig );
				}
				drawDesc[E0].mode = DESC_IGNORE;
				drawData.origin = xx->orig;
				break;
			default:
				title = _("Filled Polygon");
		}
		break;
	case SEG_TEXT:
		REORIGIN( drawData.endPt[0], segPtr->u.t.pos, xx->angle, xx->orig );
		drawData.angle = NormalizeAngle( xx->angle );
		strncpy( drawData.text, segPtr->u.t.string, sizeof drawData.text );
		drawData.text[sizeof drawData.text-1] ='\0';
		drawData.boxed = segPtr->u.t.boxed;
		drawData.origin = xx->orig;
		drawDesc[E0].mode =
		drawDesc[TP].mode =
		drawDesc[TS].mode =
		drawDesc[TX].mode = 
		drawDesc[TA].mode =
		drawDesc[BX].mode =
		drawDesc[RA].mode =
	    drawDesc[OI].mode = 0;
        drawDesc[CO].mode = 0;  /*Allow Text color setting*/
		drawDesc[LW].mode = DESC_IGNORE;
		title = _("Text");
		break;
	default:
		;
	}

	snprintf( str, len, _("%s(%d) Layer=%d"), title, GetTrkIndex(trk), GetTrkLayer(trk)+1 );

	if (!inDescribeCmd) return;

	drawData.oldE0 = drawData.endPt[0];
	drawData.oldE1 = drawData.endPt[1];
	drawData.oldAngle = drawData.angle;
	drawData.oldOrigin = drawData.origin;



	DoDescribe( title, trk, drawDesc, UpdateDraw );
	if ( segPtr->type==SEG_BENCH && drawDesc[BE].control0!=NULL && drawDesc[OR].control0!=NULL) {
		BenchLoadLists( (wList_p)drawDesc[BE].control0, (wList_p)drawDesc[OR].control0 );
		wListSetIndex( (wList_p)drawDesc[BE].control0, drawData.benchChoice );
		BenchUpdateOrientationList( (long)wListGetItemContext((wList_p)drawDesc[BE].control0, drawData.benchChoice ), (wList_p)drawDesc[OR].control0 );
		wListSetIndex( (wList_p)drawDesc[OR].control0, drawData.benchOrient );
	}
	if ( (segPtr->type==SEG_STRLIN || segPtr->type==SEG_CRVLIN || segPtr->type==SEG_POLY) && drawDesc[LT].control0!=NULL) {
		wListClear( (wList_p)drawDesc[LT].control0 );
		wListAddValue( (wList_p)drawDesc[LT].control0, _("Solid"), NULL, (void*)0 );
		wListAddValue( (wList_p)drawDesc[LT].control0, _("Dash"), NULL, (void*)1 );
		wListAddValue( (wList_p)drawDesc[LT].control0, _("Dot"), NULL, (void*)2 );
		wListAddValue( (wList_p)drawDesc[LT].control0, _("DashDot"), NULL, (void*)3 );
		wListAddValue( (wList_p)drawDesc[LT].control0, _("DashDotDot"), NULL, (void*)4 );
		wListSetIndex( (wList_p)drawDesc[LT].control0, drawData.lineType );
	}
	if ( segPtr->type==SEG_DIMLIN && drawDesc[DS].control0!=NULL ) {
		wListClear( (wList_p)drawDesc[DS].control0 );
		wListAddValue( (wList_p)drawDesc[DS].control0, _("Tiny"), NULL, (void*)0 );
		wListAddValue( (wList_p)drawDesc[DS].control0, _("Small"), NULL, (void*)1 );
		wListAddValue( (wList_p)drawDesc[DS].control0, _("Medium"), NULL, (void*)2 );
		wListAddValue( (wList_p)drawDesc[DS].control0, _("Large"), NULL, (void*)3 );
		wListSetIndex( (wList_p)drawDesc[DS].control0, drawData.dimenSize );
	}
	if ( segPtr->type==SEG_TEXT && drawDesc[TS].control0!=NULL ) {
		LoadFontSizeList( (wList_p)drawDesc[TS].control0, (long)segPtr->u.t.fontSize );
	}
}


static void DrawDraw( track_p t, drawCmd_p d, wDrawColor color )
{
	struct extraData * xx = GetTrkExtraData(t);
	unsigned long NotSolid = ~(DC_NOTSOLIDLINE);
	d->options &= NotSolid;
	if (xx->lineType == DRAWLINESOLID) {}
	else if (xx->lineType == DRAWLINEDASH) d->options |= DC_DASH;
	else if (xx->lineType == DRAWLINEDOT) d->options |= DC_DOT;
	else if (xx->lineType == DRAWLINEDASHDOT) d->options |= DC_DASHDOT;
	else if (xx->lineType == DRAWLINEDASHDOTDOT) d->options |= DC_DASHDOTDOT;
	DrawSegs( d, xx->orig, xx->angle, xx->segs, xx->segCnt, 0.0, color );
	d->options = d->options&~(DC_NOTSOLIDLINE);
}


static void DeleteDraw( track_p t )
{
	/* Get rid of points if specified */
	struct extraData * xx = GetTrkExtraData(t);
	if (xx->segs[0].type == SEG_POLY ||
			xx->segs[0].type == SEG_FILPOLY) {
		MyFree(xx->segs[0].u.p.pts);
		xx->segs[0].u.p.pts = NULL;
	}
}


static BOOL_T WriteDraw( track_p t, FILE * f )
{
	struct extraData * xx = GetTrkExtraData(t);
	BOOL_T rc = TRUE;
	rc &= fprintf(f, "DRAW %d %d 0 0 0 %0.6f %0.6f 0 %0.6f\n", GetTrkIndex(t), GetTrkLayer(t),
				xx->orig.x, xx->orig.y, xx->angle )>0;
	rc &= WriteSegs( f, xx->segCnt, xx->segs );
	return rc;
}


static void ReadDraw( char * header )
{
	track_p trk;
	wIndex_t index;
	coOrd orig;
	DIST_T elev;
	ANGLE_T angle;
	wIndex_t layer;
	int lineType;
	struct extraData * xx;

	if ( !GetArgs( header+5, paramVersion<3?"dXXpYf":paramVersion<9?"dLX00pYf":"dLd00pff",
				&index, &layer, &lineType, &orig, &elev, &angle ) )
		return;
	ReadSegs();
	if (tempSegs_da.cnt == 1) {
		trk = MakeDrawFromSeg1( index, orig, angle, &tempSegs(0) );
		SetTrkLayer( trk, layer );
	} else {
		trk = NewTrack( index, T_DRAW, 0, sizeof *xx + (tempSegs_da.cnt-1) * sizeof *(trkSeg_p)0 );
		SetTrkLayer( trk, layer );
		xx = GetTrkExtraData(trk);
		xx->orig = orig;
		xx->angle = angle;
		xx->segCnt = tempSegs_da.cnt;
		xx->lineType = lineType;
		memcpy( xx->segs, tempSegs_da.ptr, tempSegs_da.cnt * sizeof *(trkSeg_p)0 );
		ComputeDrawBoundingBox( trk );
	}
}


static void MoveDraw( track_p trk, coOrd orig )
{
	struct extraData * xx = GetTrkExtraData(trk);
	xx->orig.x += orig.x;
	xx->orig.y += orig.y;
	ComputeDrawBoundingBox( trk );
}


static void RotateDraw( track_p trk, coOrd orig, ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	Rotate( &xx->orig, orig, angle );
	xx->angle = NormalizeAngle( xx->angle + angle );
	ComputeDrawBoundingBox( trk );
}


static void RescaleDraw( track_p trk, FLOAT_T ratio )
{
	struct extraData * xx = GetTrkExtraData(trk);
	xx->orig.x *= ratio;
	xx->orig.y *= ratio;
	RescaleSegs( xx->segCnt, xx->segs, ratio, ratio, ratio );
}

static void DoConvertFill(void) {

}

static drawModContext_t drawModCmdContext = {
		InfoMessage,
		DoRedraw,
		&mainD};


static BOOL_T infoSubst = FALSE;

static paramIntegerRange_t i0_100 = { 0, 100, 25 };
static paramFloatRange_t r1_10000 = { 1, 10000 };
static paramFloatRange_t r0_10000 = { 0, 10000 };
static paramFloatRange_t r10000_10000 = {-10000, 10000};
static paramFloatRange_t r360_360 = { -360, 360, 80 };
static paramFloatRange_t r0_360 = { 0, 360, 80 };
static paramData_t drawModPLs[] = {

#define drawModLengthPD			(drawModPLs[0])
	{ PD_FLOAT, &drawModCmdContext.length, "Length", PDO_DIM|PDO_NORECORD|BO_ENTER, &r0_10000, N_("Length") },
#define drawModRelAnglePD			(drawModPLs[1])
#define drawModRelAngle           1
	{ PD_FLOAT, &drawModCmdContext.rel_angle, "Rel Angle", PDO_NORECORD|BO_ENTER, &r360_360, N_("Relative Angle") },
#define drawModWidthPD		(drawModPLs[2])
	{ PD_FLOAT, &drawModCmdContext.width, "Width", PDO_DIM|PDO_NORECORD|BO_ENTER, &r0_10000, N_("Width") },
#define drawModHeightPD		(drawModPLs[3])
	{ PD_FLOAT, &drawModCmdContext.height, "Height", PDO_DIM|PDO_NORECORD|BO_ENTER, &r0_10000, N_("Height") },
#define drawModRadiusPD		(drawModPLs[4])
#define drawModRadius           4
	{ PD_FLOAT, &drawModCmdContext.radius, "Radius", PDO_DIM|PDO_NORECORD|BO_ENTER, &r10000_10000, N_("Radius") },
#define drawModArcAnglePD		(drawModPLs[5])
	{ PD_FLOAT, &drawModCmdContext.arc_angle, "ArcAngle", PDO_NORECORD|BO_ENTER, &r360_360, N_("Arc Angle") },
#define drawModRotAnglePD		(drawModPLs[6])
	{ PD_FLOAT, &drawModCmdContext.rot_angle, "Rot Angle", PDO_NORECORD|BO_ENTER, &r0_360, N_("Rotate Angle") },
#define drawModRotCenterXPD		(drawModPLs[7])
#define drawModRotCenterInx      7
	{ PD_FLOAT, &drawModCmdContext.rot_center.x, "Rot Center X,Y", PDO_NORECORD|BO_ENTER, &r0_10000, N_("Rot Center X") },
#define drawModRotCenterYPD		(drawModPLs[8])
	{ PD_FLOAT, &drawModCmdContext.rot_center.y, " ", PDO_NORECORD|BO_ENTER, &r0_10000, N_("Rot Center Y") },

};
static paramGroup_t drawModPG = { "drawMod", 0, drawModPLs, sizeof drawModPLs/sizeof drawModPLs[0] };

static void DrawModDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	 DrawGeomModify(C_UPDATE,zero,&drawModCmdContext);
	 ParamLoadControl(&drawModPG,drawModRotCenterInx-1);	  	//Make sure the angle is updated in case center moved
	 ParamLoadControl(&drawModPG,drawModRadius);			 	// Make sure Radius updated
	 ParamLoadControl(&drawModPG,drawModRelAngle);				//Relative Angle as well
	 MainRedraw();

}

static STATUS_T ModifyDraw( track_p trk, wAction_t action, coOrd pos )
{
	struct extraData * xx = GetTrkExtraData(trk);
	STATUS_T rc = C_CONTINUE;

	wControl_p controls[5];				//Always needs a NULL last entry
	char * labels[4];

	drawModCmdContext.trk = trk;
	drawModCmdContext.orig = xx->orig;
	drawModCmdContext.angle = xx->angle;
	drawModCmdContext.segCnt = xx->segCnt;
	drawModCmdContext.segPtr = xx->segs;
	drawModCmdContext.selected = GetTrkSelected(trk);


	switch(action&0xFF) {     //Remove Text value
	case C_START:
		drawModCmdContext.type = xx->segs[0].type;
		switch(drawModCmdContext.type) {
			case SEG_POLY:
			case SEG_FILPOLY:
				drawModCmdContext.filled = (drawModCmdContext.type==SEG_FILPOLY)?TRUE:FALSE;
				drawModCmdContext.subtype = xx->segs[0].u.p.polyType;
				drawModCmdContext.open = (drawModCmdContext.subtype==POLYLINE)?TRUE:FALSE;
				break;
			default:
				break;

		}
		drawModCmdContext.rot_moved = FALSE;
		drawModCmdContext.rotate_state = FALSE;

		infoSubst = FALSE;
		rc = DrawGeomModify( C_START, pos, &drawModCmdContext );
		break;
	case C_DOWN:
		rc = DrawGeomModify( C_DOWN, pos, &drawModCmdContext );
		if ( infoSubst ) {
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		break;
	case C_LDOUBLE:
		rc = DrawGeomModify( C_LDOUBLE, pos, &drawModCmdContext );
		if ( infoSubst ) {
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		break;
	case wActionMove:
		rc = DrawGeomModify( action, pos, &drawModCmdContext );
		break;
	case C_REDRAW:
		rc = DrawGeomModify( action, pos, &drawModCmdContext );
		break;
	case C_MOVE:
		ignoredDraw = trk;
		rc = DrawGeomModify( action, pos, &drawModCmdContext );
		ignoredDraw = NULL;
		break;
	case C_UP:
		ignoredDraw = trk;
		rc = DrawGeomModify( action, pos, &drawModCmdContext );
		ignoredDraw = NULL;
		ComputeDrawBoundingBox( trk );
		if (drawModCmdContext.state == MOD_AFTER_PT) {
			switch(drawModCmdContext.type) {
			case SEG_POLY:
			case SEG_FILPOLY:
				if (xx->segs[0].u.p.polyType != RECTANGLE) {
					if (drawModCmdContext.prev_inx >= 0) {
						controls[0] = drawModLengthPD.control;
						controls[1] = drawModRelAnglePD.control;
						controls[2] = NULL;
						labels[0] = N_("Seg Lth");
						labels[1] = N_("Rel Ang");
						ParamLoadControls( &drawModPG );
						InfoSubstituteControls( controls, labels );
						drawModLengthPD.option &= ~PDO_NORECORD;
						drawModRelAnglePD.option &= ~PDO_NORECORD;
						infoSubst = TRUE;
					}
				} else  {
					controls[0] = drawModWidthPD.control;
					controls[1] = drawModHeightPD.control;
					controls[2] = NULL;
					labels[0] = N_("Width");
					labels[1] = N_("Height");
					ParamLoadControls( &drawModPG );
					InfoSubstituteControls( controls, labels );
					drawModWidthPD.option &= ~PDO_NORECORD;
					drawModHeightPD.option &= ~PDO_NORECORD;
					infoSubst = TRUE;
				}
			break;
			case SEG_STRLIN:
			case SEG_BENCH:
			case SEG_DIMLIN:
			case SEG_TBLEDGE:
				controls[0] = drawModLengthPD.control;
				controls[1] = drawModRelAnglePD.control;
				controls[2] = NULL;
				labels[0] = N_("Length");
				labels[1] = N_("Angle");
				ParamLoadControls( &drawModPG );
				InfoSubstituteControls( controls, labels );
				drawModLengthPD.option &= ~PDO_NORECORD;
				drawModRelAnglePD.option &= ~PDO_NORECORD;
				infoSubst = TRUE;
			break;
			case SEG_CRVLIN:
			case SEG_FILCRCL:
				controls[0] = drawModRadiusPD.control;
				controls[1] = NULL;
				labels[0] = N_("Radius");
				if ((drawModCmdContext.type == SEG_CRVLIN) && xx->segs[0].u.c.a1>0.0 && xx->segs[0].u.c.a1 <360.0) {
					controls[1] = drawModArcAnglePD.control;
					controls[2] = NULL;
					labels[1] = N_("Arc Angle");
				}
				ParamLoadControls( &drawModPG );
				InfoSubstituteControls( controls, labels );
				drawModArcAnglePD.option &= ~PDO_NORECORD;
				if (drawModCmdContext.type == SEG_CRVLIN)
					drawModArcAnglePD.option &= ~PDO_NORECORD;
				infoSubst = TRUE;
				break;
			default:
				InfoSubstituteControls( NULL, NULL );
				infoSubst = FALSE;
			break;
			}
		} else {
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		break;
	case C_CMDMENU:
		wMenuPopupShow( drawModDelMI );
		wMenuPushEnable( drawModPointsMode,drawModCmdContext.rotate_state);
		wMenuPushEnable( drawModriginMode,!drawModCmdContext.rotate_state);
		wMenuPushEnable( drawModRound, FALSE);
		wMenuPushEnable( drawModVertex, FALSE);
		wMenuPushEnable( drawModSmooth, FALSE);
		wMenuPushEnable( drawModDel, FALSE);
		wMenuPushEnable( drawModFill, FALSE);
		wMenuPushEnable( drawModEmpty, FALSE);
		wMenuPushEnable( drawModClose, FALSE);
		wMenuPushEnable( drawModOpen, FALSE);
		wMenuPushEnable( drawModSolid, TRUE);
		wMenuPushEnable( drawModDot, TRUE);
		wMenuPushEnable( drawModDash, TRUE);
		wMenuPushEnable( drawModDashDot, TRUE);
		wMenuPushEnable( drawModDashDotDot, TRUE);
		if (!drawModCmdContext.rotate_state && (drawModCmdContext.type == SEG_POLY || drawModCmdContext.type == SEG_FILPOLY)) {
			wMenuPushEnable( drawModDel,drawModCmdContext.prev_inx>=0);
			if ((!drawModCmdContext.open && drawModCmdContext.prev_inx>=0) ||
					((drawModCmdContext.prev_inx>0) && (drawModCmdContext.prev_inx<drawModCmdContext.max_inx))) {
				wMenuPushEnable( drawModRound,TRUE);
				wMenuPushEnable( drawModVertex, TRUE);
				wMenuPushEnable( drawModSmooth, TRUE);
			}
			wMenuPushEnable( drawModFill, (!drawModCmdContext.open) && (!drawModCmdContext.filled));
			wMenuPushEnable( drawModEmpty, (!drawModCmdContext.open) && (drawModCmdContext.filled));
			wMenuPushEnable( drawModClose, drawModCmdContext.open);
			wMenuPushEnable( drawModOpen, !drawModCmdContext.open);
		}
		wMenuPushEnable( drawModOrigin,drawModCmdContext.rotate_state);
		wMenuPushEnable( drawModLast,drawModCmdContext.rotate_state && (drawModCmdContext.prev_inx>=0));
		wMenuPushEnable( drawModCenter,drawModCmdContext.rotate_state);
		break;
	case C_TEXT:
		ignoredDraw = trk ;
		rc = DrawGeomModify( action, pos, &drawModCmdContext  );
		if ( infoSubst ) {
					InfoSubstituteControls( NULL, NULL );
					infoSubst = FALSE;
		}
		ignoredDraw = NULL;
		if (rc == C_CONTINUE) break;
		/* no break*/
	case C_FINISH:
		ignoredDraw = trk;
		rc = DrawGeomModify( C_FINISH, pos, &drawModCmdContext  );
		xx->angle = drawModCmdContext.angle;
		xx->orig = drawModCmdContext.orig;
		ignoredDraw = NULL;
		ComputeDrawBoundingBox( trk );
		DYNARR_RESET(trkSeg_t,tempSegs_da);
		if ( infoSubst ) {
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		break;
	case C_CANCEL:
	case C_CONFIRM:
	case C_TERMINATE:
		rc = DrawGeomModify( action, pos, &drawModCmdContext  );
		drawModCmdContext.state = MOD_NONE;
		DYNARR_RESET(trkSeg_t,tempSegs_da);
		if ( infoSubst ) {
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		break;

	default:

		break;
	}
	return rc;
}


static void UngroupDraw( track_p trk )
{
	struct extraData * xx = GetTrkExtraData(trk);
	int inx;
	if ( xx->segCnt <= 1 )
		return;
	DeleteTrack( trk, FALSE );
	for ( inx=0; inx<xx->segCnt; inx++ ) {
		trk = MakeDrawFromSeg( xx->orig, xx->angle, &xx->segs[inx] );
		if ( trk ) {
			SetTrkBits( trk, TB_SELECTED );
			DrawNewTrack( trk );
		}
	}
}


static ANGLE_T GetAngleDraw(
		track_p trk,
		coOrd pos,
		EPINX_T * ep0,
		EPINX_T * ep1 )
{
	struct extraData * xx = GetTrkExtraData(trk);
	ANGLE_T angle;

	pos.x -= xx->orig.x;
	pos.y -= xx->orig.y;
	Rotate( &pos, zero, -xx->angle );
	angle = GetAngleSegs( xx->segCnt, xx->segs, &pos, NULL, NULL, NULL, NULL, NULL);
	if ( ep0 ) *ep0 = -1;
	if ( ep1 ) *ep1 = -1;
	return NormalizeAngle( angle + xx->angle );
}



static BOOL_T EnumerateDraw(
		track_p trk )
{
	struct extraData * xx;
	int inx;
	trkSeg_p segPtr;

	if ( trk ) {
		xx = GetTrkExtraData(trk);
		if ( xx->segCnt < 1 )
			return TRUE;
		for ( inx=0; inx<xx->segCnt; inx++ ) {
			segPtr = &xx->segs[inx];
			if ( segPtr->type == SEG_BENCH ) {
				CountBench( segPtr->u.l.option, FindDistance( segPtr->u.l.pos[0], segPtr->u.l.pos[1] ) );
			}
		}
	} else {
		TotalBench();
	}
	return TRUE;
}


static void FlipDraw(
		track_p trk,
		coOrd orig,
		ANGLE_T angle )
{
	struct extraData * xx = GetTrkExtraData(trk);
	FlipPoint( &xx->orig, orig, angle );
	xx->angle = NormalizeAngle( 2*angle - xx->angle + 180.0 );
	FlipSegs( xx->segCnt, xx->segs, zero, angle );
	ComputeDrawBoundingBox( trk );
}

static BOOL_T StoreDraw(
		track_p trk,
		void **data,
		long * len)
{
	struct extraData * xx = GetTrkExtraData(trk);
	if (xx->segs[0].type == SEG_POLY ||
		xx->segs[0].type == SEG_FILPOLY) {
		*data = xx->segs[0].u.p.pts;
		*len = xx->segs[0].u.p.cnt* sizeof (pts_t);
		return TRUE;
	}
	return FALSE;
}

static BOOL_T ReplayDraw(
		track_p trk,
		void * data,
		long len)
{
	struct extraData * xx = GetTrkExtraData(trk);
	if (xx->segs[0].type == SEG_POLY ||
		xx->segs[0].type == SEG_FILPOLY) {
		xx->segs[0].u.p.pts = MyMalloc(len);
		memcpy(xx->segs[0].u.p.pts,data,len);
		return TRUE;
	}
	return FALSE;
}

static BOOL_T QueryDraw( track_p trk, int query )
{
	struct extraData * xx = GetTrkExtraData(trk);
	switch(query) {
	case Q_IS_DRAW:
		return TRUE;
	case Q_IS_POLY:
		if ((xx->segs[0].type == SEG_POLY) || (xx->segs[0].type == SEG_FILPOLY) ) {
			return TRUE;
		}
		else
			return FALSE;
	case Q_IS_TEXT:
		if (xx->segs[0].type== SEG_TEXT) return TRUE;
		else return FALSE;
	default:
		return FALSE;
	}
}

static wBool_t CompareDraw( track_cp trk1, track_cp trk2 )
{
	struct extraData *xx1 = GetTrkExtraData( trk1 );
	struct extraData *xx2 = GetTrkExtraData( trk2 );
	char * cp = message + strlen(message);
	REGRESS_CHECK_POS( "Orig", xx1, xx2, orig )
	REGRESS_CHECK_ANGLE( "Angle", xx1, xx2, angle )
	REGRESS_CHECK_INT( "LineType", xx1, xx2, lineType )
	return CompareSegs( xx1->segs, xx1->segCnt, xx2->segs, xx2->segCnt );
}

static trackCmd_t drawCmds = {
		"DRAW",
		DrawDraw,
		DistanceDraw,
		DescribeDraw,
		DeleteDraw,
		WriteDraw,
		ReadDraw,
		MoveDraw,
		RotateDraw,
		RescaleDraw,
		NULL,
		GetAngleDraw, /* getAngle */
		NULL, /* split */
		NULL, /* traverse */
		EnumerateDraw,
		NULL, /* redraw */
		NULL, /* trim */
		NULL, /* merge */
		ModifyDraw,
		NULL, /* getLength */
		NULL, /* getTrackParams */
		NULL, /* moveEndPt */
		QueryDraw, /* query */
		UngroupDraw,
		FlipDraw,
		NULL,
		NULL,
		NULL,
		NULL, /*Parallel*/
		NULL,
		NULL, /*MakeSegs*/
		ReplayDraw,
		StoreDraw,
		NULL,
		CompareDraw
		};

EXPORT BOOL_T OnTableEdgeEndPt( track_p trk, coOrd * pos )
{
	track_p trk1;
	struct extraData *xx;
	coOrd pos1 = *pos;

	ignoredTableEdge = trk;
	if ((trk1 = OnTrack( &pos1, FALSE, FALSE )) != NULL &&
		 GetTrkType(trk1) == T_DRAW) {
		ignoredTableEdge = NULL;
		xx = GetTrkExtraData(trk1);
		if (xx->segCnt < 1)
			return FALSE;
		if (xx->segs[0].type == SEG_TBLEDGE) {
			if ( IsClose( FindDistance( *pos, xx->segs[0].u.l.pos[0] ) ) ) {
				*pos = xx->segs[0].u.l.pos[0];
				return TRUE;
			} else if ( IsClose( FindDistance( *pos, xx->segs[0].u.l.pos[1] ) ) ) {
				*pos = xx->segs[0].u.l.pos[1];
				return TRUE;
			}
		}
	}
	ignoredTableEdge = NULL;
	return FALSE;
}

EXPORT BOOL_T GetClosestEndPt( track_p trk, coOrd * pos)
{
	struct extraData *xx;

	if (GetTrkType(trk) == T_DRAW) {
		ignoredTableEdge = NULL;
		xx = GetTrkExtraData(trk);
		if (xx->segCnt < 1)
			return FALSE;
		DIST_T dd0,dd1;
		coOrd p00,p0,p1;
		p00 = *pos;
		if (GetTrkType(trk) == T_DRAW) {
			Rotate(&p00,xx->orig,-xx->angle);
			p00.x -= xx->orig.x;
			p00.y -= xx->orig.y;
			switch (xx->segs[0].type) {
				case SEG_CRVLIN:
					PointOnCircle( &p0, xx->segs[0].u.c.center, fabs(xx->segs[0].u.c.radius), xx->segs[0].u.c.a0 );
					dd0 = FindDistance( p00, p0);
					PointOnCircle( &p1, xx->segs[0].u.c.center, fabs(xx->segs[0].u.c.radius), xx->segs[0].u.c.a0 + xx->segs[0].u.c.a1);
					dd1 = FindDistance( p00, p1);
				break;
				case SEG_STRLIN:
					dd0 = FindDistance( p00, xx->segs[0].u.l.pos[0]);
					p0 = xx->segs[0].u.l.pos[0];
					dd1 = FindDistance( p00, xx->segs[0].u.l.pos[1]);
					p1 = xx->segs[0].u.l.pos[1];
				break;
				case SEG_BEZLIN:
					dd0 = FindDistance( p00, xx->segs[0].u.b.pos[0]);
					p0 = xx->segs[0].u.b.pos[0];
					dd1 = FindDistance( p00, xx->segs[0].u.b.pos[3]);
					p1 = xx->segs[0].u.b.pos[3];
				break;
				default:
					return FALSE;
			}
			p0.x += xx->orig.x;
			p0.y += xx->orig.y;
			Rotate(&p0,xx->orig,xx->angle);
			p1.x += xx->orig.x;
			p1.y += xx->orig.y;
			Rotate(&p1,xx->orig,xx->angle);
		} else if (GetTrkType(trk) == T_BZRLIN) {
			coOrd p0,p1;
			xx = GetTrkExtraData(trk);
			p0 = xx->segs[0].u.b.pos[0];
			p1 = xx->segs[0].u.b.pos[3];
			dd0 = FindDistance(p00,p0);
			dd1 = FindDistance(p00,p1);
		} else return FALSE;
		if (dd0>dd1) {
			* pos = p1;
			return TRUE;

		} else {
			* pos = p0;
			return TRUE;
		}
	}
	return FALSE;
}


static drawContext_t drawCmdContext = {
		InfoMessage,
		DoRedraw,
		&mainD,
		OP_LINE };

static wIndex_t benchChoice;
static wIndex_t benchOrient;
static wIndex_t dimArrowSize;
wDrawColor lineColor = 1;
long lineWidth = 0;
static wDrawColor benchColor;



static paramData_t drawPLs[] = {
#define drawLineWidthPD				(drawPLs[0])
	{ PD_LONG, &drawCmdContext.line_Width, "linewidth", PDO_NORECORD, &i0_100, N_("Line Width") },
#define drawColorPD				(drawPLs[1])
	{ PD_COLORLIST, &lineColor, "linecolor", PDO_NORECORD, NULL, N_("Color") },
#define drawBenchColorPD		(drawPLs[2])
	{ PD_COLORLIST, &benchColor, "benchcolor", PDO_NORECORD, NULL, N_("Color") },
#define drawBenchChoicePD		(drawPLs[3])
#ifdef WINDOWS
	{ PD_DROPLIST, &benchChoice, "benchlist", PDO_NOPREF|PDO_NORECORD|PDO_LISTINDEX, (void*)120, N_("Lumber Type") },
#else    
    { PD_DROPLIST, &benchChoice, "benchlist", PDO_NOPREF|PDO_NORECORD|PDO_LISTINDEX, (void*)145, N_("Lumber Type") },
#endif    
#define drawBenchOrientPD		(drawPLs[4])
#ifdef WINDOWS
	{ PD_DROPLIST, &benchOrient, "benchorient", PDO_NOPREF|PDO_NORECORD|PDO_LISTINDEX, (void*)45, "", 0 },
#else
	{ PD_DROPLIST, &benchOrient, "benchorient", PDO_NOPREF|PDO_NORECORD|PDO_LISTINDEX, (void*)105, "", 0 },
#endif
#define drawDimArrowSizePD		(drawPLs[5])
	{ PD_DROPLIST, &dimArrowSize, "arrowsize", PDO_NORECORD|PDO_LISTINDEX, (void*)80, N_("Size") },
#define drawLengthPD			(drawPLs[6])
	{ PD_FLOAT, &drawCmdContext.length, "Length", PDO_DIM|PDO_NORECORD|BO_ENTER, &r0_10000, N_("Length") },
#define drawWidthPD				(drawPLs[7])
	{ PD_FLOAT, &drawCmdContext.width, "BoxWidth", PDO_DIM|PDO_NORECORD|BO_ENTER, &r0_10000, N_("Box Width") },
#define drawAnglePD				(drawPLs[8])
#define drawAngleInx					8
	{ PD_FLOAT, &drawCmdContext.angle, "Angle", PDO_NORECORD|BO_ENTER, &r360_360, N_("Angle") },
#define drawRadiusPD            (drawPLs[9])
	{ PD_FLOAT, &drawCmdContext.radius, "Radius", PDO_DIM|PDO_NORECORD|BO_ENTER, &r0_10000, N_("Radius") },
#define drawLineTypePD			(drawPLs[10])
	{ PD_DROPLIST, &drawCmdContext.lineType, "Type", PDO_DIM|PDO_NORECORD|BO_ENTER, (void*)0, N_("Line Type") },
};
static paramGroup_t drawPG = { "draw", 0, drawPLs, sizeof drawPLs/sizeof drawPLs[0] };

static char * objectName[] = {
		N_("Straight"),
		N_("Dimension"),
		N_("Lumber"),
		N_("Table Edge"),
		N_("Curved"),
		N_("Curved"),
		N_("Curved"),
		N_("Curved"),
		N_("Circle"),
		N_("Circle"),
		N_("Circle"),
		N_("Box"),
		N_("Polygon"),
		N_("Filled Circle"),
		N_("Filled Circle"),
		N_("Filled Circle"),
		N_("Filled Box"),
		N_("Filled Polygon"),
		N_("Bezier Line"),
		N_("Polyline"),
		NULL};

static STATUS_T CmdDraw( wAction_t action, coOrd pos )

{
	static BOOL_T infoSubst = FALSE;
	wControl_p controls[5];				//Always needs a NULL last entry
	char * labels[4];
	static char labelName[40];

	wAction_t act2 = (action&0xFF) | (bezCmdCreateLine<<8);

	switch (action&0xFF) {

	case C_START:
		ParamLoadControls( &drawPG );
		/*drawContext = &drawCmdContext;*/
		drawLineWidthPD.option |= PDO_NORECORD;
		drawColorPD.option |= PDO_NORECORD;
		drawBenchColorPD.option |= PDO_NORECORD;
		drawBenchChoicePD.option |= PDO_NORECORD;
		drawBenchOrientPD.option |= PDO_NORECORD;
		drawDimArrowSizePD.option |= PDO_NORECORD;
		drawCmdContext.Op = (wIndex_t)(long)commandContext;
		if ( drawCmdContext.Op < 0 || drawCmdContext.Op > OP_LAST ) {
			NoticeMessage( "cmdDraw: Op %d", _("Ok"), NULL, drawCmdContext.Op );
			drawCmdContext.Op = OP_LINE;
		}
		/*DrawGeomOp( (void*)(drawCmdContext.Op>=0?drawCmdContext.Op:OP_LINE) );*/
		infoSubst = TRUE;
		switch( drawCmdContext.Op ) {
		case OP_LINE:
		case OP_CURVE1:
		case OP_CURVE2:
		case OP_CURVE3:
		case OP_CURVE4:
		case OP_CIRCLE2:
		case OP_CIRCLE3:
		case OP_BEZLIN:
		case OP_BOX:
		case OP_POLY:
		case OP_POLYLINE:
			controls[0] = drawLineWidthPD.control;
			controls[1] = drawColorPD.control;
			controls[2] = drawLineTypePD.control;
			controls[2] = NULL;
			sprintf( labelName, _("%s Line Width"), _(objectName[drawCmdContext.Op]) );
			labels[0] = labelName;
			labels[1] = N_("Color");
			labels[2] = N_("Type");
			if ( wListGetCount( (wList_p)drawLineTypePD.control ) == 0 ) {
				wListAddValue( (wList_p)drawLineTypePD.control, _("Solid"), NULL, NULL );
				wListAddValue( (wList_p)drawLineTypePD.control, _("Dot"), NULL, NULL );
				wListAddValue( (wList_p)drawLineTypePD.control, _("Dash"), NULL, NULL );
				wListAddValue( (wList_p)drawLineTypePD.control, _("Dash-Dot"), NULL, NULL );
				wListAddValue( (wList_p)drawLineTypePD.control, _("Dash-Dot-Dot"), NULL, NULL );
			}
			InfoSubstituteControls( controls, labels );
			drawLineWidthPD.option &= ~PDO_NORECORD;
			drawColorPD.option &= ~PDO_NORECORD;
			drawLineTypePD.option &= ~PDO_NORECORD;
			break;
		case OP_FILLCIRCLE2:
		case OP_FILLCIRCLE3:
		case OP_FILLBOX:
		case OP_FILLPOLY:
			controls[0] = drawColorPD.control;
			controls[1] = NULL;
			sprintf( labelName, _("%s Color"), _(objectName[drawCmdContext.Op]) );
			labels[0] = labelName;
			ParamLoadControls( &drawPG );
			InfoSubstituteControls( controls, labels );
			drawColorPD.option &= ~PDO_NORECORD;
			break;
		case OP_BENCH:
			controls[0] = drawBenchChoicePD.control;
			controls[1] = drawBenchOrientPD.control;
			controls[2] = drawBenchColorPD.control;
			controls[3] = NULL;
			labels[0] = N_("Lumber Type");
			labels[1] = "";
			labels[2] = N_("Color");
			if ( wListGetCount( (wList_p)drawBenchChoicePD.control ) == 0 )
				BenchLoadLists( (wList_p)drawBenchChoicePD.control, (wList_p)drawBenchOrientPD.control );

			ParamLoadControls( &drawPG );
			BenchUpdateOrientationList( (long)wListGetItemContext( (wList_p)drawBenchChoicePD.control, benchChoice ), (wList_p)drawBenchOrientPD.control );
			wListSetIndex( (wList_p)drawBenchOrientPD.control, benchOrient );
			InfoSubstituteControls( controls, labels );
			drawBenchColorPD.option &= ~PDO_NORECORD;
			drawBenchChoicePD.option &= ~PDO_NORECORD;
			drawBenchOrientPD.option &= ~PDO_NORECORD;
			drawLengthPD.option &= ~PDO_NORECORD;
			break;
		case OP_DIMLINE:
			controls[0] = drawDimArrowSizePD.control;
			controls[1] = NULL;
			labels[0] = N_("Dimension Line Size");
			if ( wListGetCount( (wList_p)drawDimArrowSizePD.control ) == 0 ) {
				wListAddValue( (wList_p)drawDimArrowSizePD.control, _("Tiny"), NULL, NULL );
				wListAddValue( (wList_p)drawDimArrowSizePD.control, _("Small"), NULL, NULL );
				wListAddValue( (wList_p)drawDimArrowSizePD.control, _("Medium"), NULL, NULL );
				wListAddValue( (wList_p)drawDimArrowSizePD.control, _("Large"), NULL, NULL );
			}
			ParamLoadControls( &drawPG );
			InfoSubstituteControls( controls, labels );
			drawDimArrowSizePD.option &= ~PDO_NORECORD;
			break;
		case OP_TBLEDGE:
			InfoMessage( _("Drag to create Table Edge") );
			break;
		default:
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		ParamGroupRecord( &drawPG );
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		DrawGeomMouse( C_START, pos, &drawCmdContext);
		return C_CONTINUE;

	case wActionLDown:
		ParamLoadData( &drawPG );
		if (drawCmdContext.Op == OP_BEZLIN) {
			act2 = action | (bezCmdCreateLine<<8);
			return CmdBezCurve(act2, pos);
		}
		if ( drawCmdContext.Op == OP_BENCH ) {
			drawCmdContext.benchOption = GetBenchData( (long)wListGetItemContext((wList_p)drawBenchChoicePD.control, benchChoice ), benchOrient );
			drawCmdContext.Color = benchColor;

		} else if ( drawCmdContext.Op == OP_DIMLINE ) {
			drawCmdContext.Color = wDrawColorBlack;
			drawCmdContext.benchOption = dimArrowSize;
		} else if ( drawCmdContext.Op == OP_TBLEDGE ) {
			drawCmdContext.Color = wDrawColorBlack;
		} else {
			drawCmdContext.Color = lineColor;
		}
		if ( infoSubst ) {
			InfoSubstituteControls( NULL, NULL );
			infoSubst = FALSE;
		}
		/* no break */
	case wActionLDrag:
		ParamLoadData( &drawPG );
		/* no break */
	case wActionMove:
	case wActionRDown:
	case wActionRDrag:
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		if (!((MyGetKeyState() & WKEY_ALT) != magneticSnap)) {
			SnapPos( &pos );
		}
		return DrawGeomMouse( action, pos, &drawCmdContext);
	case wActionLUp:
	case wActionRUp:
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		//if (!((MyGetKeyState() & WKEY_SHIFT) != 0)) {
		//		SnapPos( &pos );   Remove Snap at end of action - it will have been imposed in Geom if needed
		//}
		int rc = DrawGeomMouse( action, pos, &drawCmdContext);
		// Put up text entry boxes ready for updates if the result was continue
		if (rc == C_CONTINUE) {
			switch( drawCmdContext.Op ) {
			case OP_CIRCLE1:
			case OP_CIRCLE2:
			case OP_CIRCLE3:
			case OP_FILLCIRCLE1:
			case OP_FILLCIRCLE2:
			case OP_FILLCIRCLE3:
				controls[0] = drawRadiusPD.control;
				controls[1] = NULL;
				labels[0] = N_("Radius");
				ParamLoadControls( &drawPG );
				InfoSubstituteControls( controls, labels );
				drawRadiusPD.option &= ~PDO_NORECORD;
				infoSubst = TRUE;
				break;
			case OP_CURVE1:
			case OP_CURVE2:
			case OP_CURVE3:
			case OP_CURVE4:
				if (drawCmdContext.ArcData.type == curveTypeCurve) {
					controls[0] = drawRadiusPD.control;
					controls[1] = drawAnglePD.control;
					controls[2] = NULL;
					labels[0] = N_("Radius");
					labels[1] = N_("Arc Angle");
				} else {
					controls[0] = drawLengthPD.control;
					controls[1] = drawAnglePD.control;
					controls[2] = NULL;
					labels[0] = N_("Length");
					labels[1] = N_("Angle");
				}
				ParamLoadControls( &drawPG );
				InfoSubstituteControls( controls, labels );
				drawLengthPD.option &= ~PDO_NORECORD;
				drawRadiusPD.option &= ~PDO_NORECORD;
				drawAnglePD.option &= ~PDO_NORECORD;
				infoSubst = TRUE;
				break;
			case OP_LINE:
			case OP_BENCH:
			case OP_TBLEDGE:
			case OP_POLY:
			case OP_FILLPOLY:
			case OP_POLYLINE:
				controls[0] = drawLengthPD.control;
				controls[1] = drawAnglePD.control;
				controls[2] = NULL;
				labels[0] = N_("Seg Length");
				if (drawCmdContext.Op == OP_LINE || drawCmdContext.Op  == OP_BENCH || drawCmdContext.Op == OP_TBLEDGE)
					labels[1] = N_("Angle");
				else if (drawCmdContext.index > 0 )
					labels[1] = N_("Rel Angle");
				else
					labels[1] = N_("Angle");
				ParamLoadControls( &drawPG );
				InfoSubstituteControls( controls, labels );
				drawLengthPD.option &= ~PDO_NORECORD;
				drawAnglePD.option &= ~PDO_NORECORD;
				infoSubst = TRUE;
				break;
			case OP_BOX:
			case OP_FILLBOX:
				controls[0] = drawLengthPD.control;
				controls[1] = drawWidthPD.control;
				controls[2] = NULL;
				labels[0] = N_("Box Length");
				labels[1] = N_("Box Width");
				ParamLoadControls( &drawPG );
				InfoSubstituteControls( controls, labels );
				drawLengthPD.option &= ~PDO_NORECORD;
				drawWidthPD.option &= ~PDO_NORECORD;
				infoSubst = TRUE;
				break;
			default:
				break;
			}
		}
		return rc;

	case C_CANCEL:
		InfoSubstituteControls( NULL, NULL );
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		return DrawGeomMouse( action, pos, &drawCmdContext);
	case C_TEXT:
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(action, pos);
		return DrawGeomMouse( action, pos, &drawCmdContext);
	case C_OK:
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		return DrawGeomMouse( (0x0D<<8|wActionText), pos, &drawCmdContext);

		/*DrawOk( NULL );*/

	case C_FINISH:
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		return DrawGeomMouse( (0x0D<<8|wActionText), pos, &drawCmdContext);
		/*DrawOk( NULL );*/

	case C_REDRAW:
		if (drawCmdContext.Op == OP_BEZLIN) return CmdBezCurve(act2, pos);
		return DrawGeomMouse( action, pos, &drawCmdContext);

	case C_CMDMENU:
		if (drawCmdContext.Op == OP_BEZLIN) return C_CONTINUE;
		return DrawGeomMouse( action, pos, &drawCmdContext);

	default:
		return C_CONTINUE;
	}
}

#include "bitmaps/dline.xpm"
#include "bitmaps/ddimlin.xpm"
#include "bitmaps/dbench.xpm"
#include "bitmaps/dtbledge.xpm"
#include "bitmaps/dcurve1.xpm"
#include "bitmaps/dcurve2.xpm"
#include "bitmaps/dcurve3.xpm"
#include "bitmaps/dcurve4.xpm"
/*#include "bitmaps/dcircle1.xpm"*/
#include "bitmaps/dcircle2.xpm"
#include "bitmaps/dcircle3.xpm"
/*#include "bitmaps/dflcrcl1.xpm"*/
#include "bitmaps/dflcrcl2.xpm"
#include "bitmaps/dflcrcl3.xpm"
#include "bitmaps/dbox.xpm"
#include "bitmaps/dfilbox.xpm"
#include "bitmaps/dpoly.xpm"
#include "bitmaps/dfilpoly.xpm"
#include "bitmaps/dbezier.xpm"
#include "bitmaps/dpolyline.xpm"

typedef struct {
		char **xpm;
		int OP;
		char * shortName;
		char * cmdName;
		char * helpKey;
		long acclKey;
		} drawData_t;

static drawData_t dlineCmds[] = {
		{ dline_xpm, OP_LINE, N_("Line"), N_("Draw Line"), "cmdDrawLine", ACCL_DRAWLINE },
		{ ddimlin_xpm, OP_DIMLINE, N_("Dimension Line"), N_("Draw Dimension Line"), "cmdDrawDimLine", ACCL_DRAWDIMLINE },
		{ dbench_xpm, OP_BENCH, N_("Benchwork"), N_("Draw Benchwork"), "cmdDrawBench", ACCL_DRAWBENCH },
		{ dtbledge_xpm, OP_TBLEDGE, N_("Table Edge"), N_("Draw Table Edge"), "cmdDrawTableEdge", ACCL_DRAWTBLEDGE } };
static drawData_t dcurveCmds[] = {
		{ dcurve1_xpm, OP_CURVE1, N_("Curve End"), N_("Draw Curve from End"), "cmdDrawCurveEndPt", ACCL_DRAWCURVE1 },
		{ dcurve2_xpm, OP_CURVE2, N_("Curve Tangent"), N_("Draw Curve from Tangent"), "cmdDrawCurveTangent", ACCL_DRAWCURVE2 },
		{ dcurve3_xpm, OP_CURVE3, N_("Curve Center"), N_("Draw Curve from Center"), "cmdDrawCurveCenter", ACCL_DRAWCURVE3 },
		{ dcurve4_xpm, OP_CURVE4, N_("Curve Chord"), N_("Draw Curve from Chord"), "cmdDrawCurveChord", ACCL_DRAWCURVE4 },
		{ dbezier_xpm, OP_BEZLIN, N_("Bezier Curve"), N_("Draw Bezier"), "cmdDrawBezierCurve", ACCL_DRAWBEZLINE } };
static drawData_t dcircleCmds[] = {
		/*{ dcircle1_xpm, OP_CIRCLE1, "Circle Fixed Radius", "Draw Fixed Radius Circle", "cmdDrawCircleFixedRadius", ACCL_DRAWCIRCLE1 },*/
		{ dcircle3_xpm, OP_CIRCLE3, N_("Circle Tangent"), N_("Draw Circle from Tangent"), "cmdDrawCircleTangent", ACCL_DRAWCIRCLE2 },
		{ dcircle2_xpm, OP_CIRCLE2, N_("Circle Center"), N_("Draw Circle from Center"), "cmdDrawCircleCenter", ACCL_DRAWCIRCLE3 },
		/*{ dflcrcl1_xpm, OP_FILLCIRCLE1, "Circle Filled Fixed Radius", "Draw Fixed Radius Filled Circle", "cmdDrawFilledCircleFixedRadius", ACCL_DRAWFILLCIRCLE1 },*/
		{ dflcrcl3_xpm, OP_FILLCIRCLE3, N_("Circle Filled Tangent"), N_("Draw Filled Circle from Tangent"), "cmdDrawFilledCircleTangent", ACCL_DRAWFILLCIRCLE2 },
		{ dflcrcl2_xpm, OP_FILLCIRCLE2, N_("Circle Filled Center"), N_("Draw Filled Circle from Center"), "cmdDrawFilledCircleCenter", ACCL_DRAWFILLCIRCLE3 } };
static drawData_t dshapeCmds[] = {
		{ dbox_xpm, OP_BOX, N_("Box"), N_("Draw Box"), "cmdDrawBox", ACCL_DRAWBOX },
		{ dfilbox_xpm, OP_FILLBOX, N_("Filled Box"), N_("Draw Filled Box"), "cmdDrawFilledBox", ACCL_DRAWFILLBOX },
		{ dpoly_xpm, OP_POLY, N_("Polygon"), N_("Draw Polygon"), "cmdDrawPolygon", ACCL_DRAWPOLY },
		{ dfilpoly_xpm, OP_FILLPOLY, N_("Filled Polygon"), N_("Draw Filled Polygon"), "cmdDrawFilledPolygon", ACCL_DRAWFILLPOLYGON },
		{ dpolyline_xpm, OP_POLYLINE, N_("PolyLine"), N_("Draw PolyLine"), "cmdDrawPolyline", ACCL_DRAWPOLYLINE },
};

typedef struct {
		char * helpKey;
		char * menuTitle;
		char * stickyLabel;
		int cnt;
		drawData_t * data;
		long acclKey;
		wIndex_t cmdInx;
		int curr;
		} drawStuff_t;
static drawStuff_t drawStuff[4];


static drawStuff_t drawStuff[4] = {
		{ "cmdDrawLineSetCmd", N_("Straight Objects"), N_("Draw Straight Objects"), 4, dlineCmds },
		{ "cmdDrawCurveSetCmd", N_("Curved Lines"), N_("Draw Curved Lines"), 5, dcurveCmds },
		{ "cmdDrawCircleSetCmd", N_("Circle Lines"), N_("Draw Circles"), 4, dcircleCmds },
		{ "cmdDrawShapeSetCmd", N_("Shapes"), N_("Draw Shapes"), 5, dshapeCmds} };
		

static void ChangeDraw( long changes )
{
	wIndex_t choice, orient;
	if ( changes & CHANGE_UNITS ) {
		if ( drawBenchChoicePD.control && drawBenchOrientPD.control ) {
			choice = wListGetIndex( (wList_p)drawBenchChoicePD.control );
			orient = wListGetIndex( (wList_p)drawBenchOrientPD.control );
			BenchLoadLists( (wList_p)drawBenchChoicePD.control, (wList_p)drawBenchOrientPD.control );
			wListSetIndex( (wList_p)drawBenchChoicePD.control, choice );
			wListSetIndex( (wList_p)drawBenchOrientPD.control, orient );
		}
	}
}



static void DrawDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	if (inx==3) {
		if (drawCmdContext.Op == OP_BEZLIN) {
			if ( (inx == 0  && pg->paramPtr[inx].valueP == &drawCmdContext.line_Width) ||
				 (inx == 1 && pg->paramPtr[inx].valueP == &lineColor))
			   {
				lineWidth = drawCmdContext.line_Width;
				UpdateParms(lineColor, lineWidth);
			   }
		}
	}
	if (inx >=6 ) {
		if (drawCmdContext.Op == OP_CIRCLE1 ||
			drawCmdContext.Op == OP_FILLCIRCLE1 ||
			drawCmdContext.Op == OP_CIRCLE2 ||
			drawCmdContext.Op == OP_FILLCIRCLE2 ||
			drawCmdContext.Op == OP_CIRCLE3 ||
			drawCmdContext.Op == OP_FILLCIRCLE3) {
			coOrd pos = zero;
			DrawGeomMouse(C_UPDATE,pos,&drawCmdContext);
		}
		if (drawCmdContext.Op == OP_CURVE1 ||
			drawCmdContext.Op == OP_CURVE2 ||
			drawCmdContext.Op == OP_CURVE3 ||
			drawCmdContext.Op == OP_CURVE4 	) {
			coOrd pos = zero;
			DrawGeomMouse(C_UPDATE,pos,&drawCmdContext);
		}
		if (drawCmdContext.Op == OP_LINE ||
			drawCmdContext.Op == OP_BENCH||
			drawCmdContext.Op == OP_TBLEDGE) {
			coOrd pos = zero;
			DrawGeomMouse(C_UPDATE,pos,&drawCmdContext);
		}

		if (drawCmdContext.Op == OP_BOX ||
			drawCmdContext.Op == OP_FILLBOX	){
			coOrd pos = zero;
			DrawGeomMouse(C_UPDATE,pos,&drawCmdContext);
		}

		if (drawCmdContext.Op == OP_POLY ||
			drawCmdContext.Op == OP_FILLPOLY ||
			drawCmdContext.Op == OP_POLYLINE) {
			coOrd pos = zero;
			DrawGeomMouse(C_UPDATE,pos,&drawCmdContext);
		}
		ParamLoadControl(&drawPG,drawAngleInx);				//Force Angle change out
		//if (pg->paramPtr[inx].enter_pressed) {
		//	coOrd pos = zero;
		//	DrawGeomMouse((0x0D<<8)|(C_TEXT&0xFF),pos,&drawCmdContext);
		//	CmdDraw(C_START,pos);
		//}
	}

	if ( inx >= 0 && pg->paramPtr[inx].valueP == &benchChoice )
		BenchUpdateOrientationList( (long)wListGetItemContext( (wList_p)drawBenchChoicePD.control, (wIndex_t)*(long*)valueP ), (wList_p)drawBenchOrientPD.control );
}

EXPORT void InitCmdDraw( wMenu_p menu )
{
	int inx1, inx2;
	drawStuff_t * dsp;
	drawData_t * ddp;
	wIcon_p icon;

	drawCmdContext.Color = wDrawColorBlack;
	lineColor = wDrawColorBlack;
	benchColor = wDrawFindColor( wRGB(255,192,0) );
	ParamCreateControls( &drawPG, DrawDlgUpdate );

	ParamCreateControls( &drawModPG, DrawModDlgUpdate) ;

	for ( inx1=0; inx1<4; inx1++ ) {
		dsp = &drawStuff[inx1];
		ButtonGroupBegin( _(dsp->menuTitle), dsp->helpKey, _(dsp->stickyLabel) );
		for ( inx2=0; inx2<dsp->cnt; inx2++ ) {
			ddp = &dsp->data[inx2];
			icon = wIconCreatePixMap( ddp->xpm );
			AddMenuButton( menu, CmdDraw, ddp->helpKey, _(ddp->cmdName), icon, LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ddp->acclKey, (void *)(intptr_t)ddp->OP );
		}
		ButtonGroupEnd();
	}

	ParamRegister( &drawPG );
	RegisterChangeNotification( ChangeDraw );

}


BOOL_T ReadTableEdge( char * line )
{
	track_p trk;
	TRKINX_T index;
	DIST_T elev0, elev1;
	trkSeg_t seg;
	wIndex_t layer;

	if ( !GetArgs( line, paramVersion<3?"dXpYpY":paramVersion<9?"dL000pYpY":"dL000pfpf",
				&index, &layer, &seg.u.l.pos[0], &elev0, &seg.u.l.pos[1], &elev1 ) )
		return FALSE;
	seg.type = SEG_TBLEDGE;
	seg.color = wDrawColorBlack;
	seg.width = 0;
	trk = MakeDrawFromSeg1( index, zero, 0.0, &seg );
	SetTrkLayer(trk, layer);
	return TRUE;
}

/**
 * Create a new segment for text. The data are stored in a trk structure.
 * Storage for the string is dynamically allocated. 
 *
 * \param index IN of new element
 * \param pos IN coordinates of element
 * \param angle IN orientation 
 * \param text IN text itself
 * \param textSize IN font size in pts
 * \param color IN text color
 * \return    the newly allocated trk structure
 */

EXPORT track_p NewText(
		wIndex_t index,
		coOrd pos,
		ANGLE_T angle,
		char * text,
		CSIZE_T textSize,
        wDrawColor color,
		BOOL_T boxed)
{
	trkSeg_t tempSeg;
	track_p trk;
	tempSeg.type = SEG_TEXT;
	tempSeg.color = color;
	tempSeg.width = 0;
	tempSeg.u.t.pos = zero;
	tempSeg.u.t.angle = angle;
	tempSeg.u.t.fontP = NULL;
	tempSeg.u.t.fontSize = textSize;
	tempSeg.u.t.string = MyStrdup( text );
	tempSeg.u.t.boxed = boxed;
	trk = MakeDrawFromSeg1( index, pos, angle, &tempSeg );
	return trk;
}

EXPORT BOOL_T ReadText( char * line )
{
	coOrd pos;
	CSIZE_T textSize;
	char * text;
	wIndex_t index;
	wIndex_t layer;
	track_p trk;
	ANGLE_T angle;
    wDrawColor color = wDrawColorBlack;
    if ( paramVersion<3 ) {
        if (!GetArgs( line, "XXpYql", &index, &layer, &pos, &angle, &text, &textSize ))
            return FALSE;
    } else if (paramVersion<9 ) {
        if (!GetArgs(line, "dL000pYql", &index, &layer, &pos, &angle, &text, &textSize))
            return FALSE;
    } else {
        if (!GetArgs(line, "dLl00pfql", &index, &layer, &color, &pos, &angle, &text, &textSize ))
            return FALSE;
    }

    char * old = text;
    text = ConvertFromEscapedText(text);
    MyFree(old);

	trk = NewText( index, pos, angle, text, textSize, color, FALSE );
	SetTrkLayer( trk, layer );
	MyFree(text);
	return TRUE;
}

void MenuMode(int mode) {
	if ( infoSubst ) {
		InfoSubstituteControls( NULL, NULL );
		infoSubst = FALSE;
	}
	if (mode == 1) {
		DrawGeomOriginMove(C_START,zero,&drawModCmdContext);
		InfoMessage("Origin Mode");
	} else  {
		DrawGeomModify(C_START,zero,&drawModCmdContext);
		InfoMessage("Points Mode");
	}
}

void MenuEnter(int key) {
	int action;
	action = C_TEXT;
	action |= key<<8;
	if (drawModCmdContext.rotate_state)
		DrawGeomOriginMove(action,zero,&drawModCmdContext);
	else
		DrawGeomModify(action,zero,&drawModCmdContext);
}

void MenuLine(int key) {
	struct extraData * xx = GetTrkExtraData(drawModCmdContext.trk);
	if ( drawModCmdContext.type==SEG_STRLIN || drawModCmdContext.type==SEG_CRVLIN || drawModCmdContext.type==SEG_POLY ) {
		switch(key) {
		case '0':
			xx->lineType = DRAWLINESOLID;
			break;
		case '1':
			xx->lineType = DRAWLINEDASH;
			break;
		case '2':
			xx->lineType = DRAWLINEDOT;
			break;
		case '3':
			xx->lineType = DRAWLINEDASHDOT;
			break;
		case '4':
			xx->lineType = DRAWLINEDASHDOTDOT;
			break;
		}
		MainRedraw(); // MenuLine
	}
}

EXPORT void SetLineType( track_p trk, int width ) {
	if (QueryTrack(trk, Q_IS_DRAW)) {
		struct extraData * xx = GetTrkExtraData(trk);
		if ( xx->segs[0].type==SEG_STRLIN || xx->segs[0].type==SEG_CRVLIN || xx->segs[0].type==SEG_POLY) {
			switch(width) {
			case 0:
				xx->lineType = DRAWLINESOLID;
				break;
			case 1:
				xx->lineType = DRAWLINEDASH;
				break;
			case 2:
				xx->lineType = DRAWLINEDOT;
				break;
			case 3:
				xx->lineType = DRAWLINEDASHDOT;
				break;
			case 4:
				xx->lineType = DRAWLINEDASHDOTDOT;
				break;
			}
		}
	}
}

EXPORT void InitTrkDraw( void )
{
	T_DRAW = InitObject( &drawCmds );
	AddParam( "TABLEEDGE", ReadTableEdge );
	AddParam( "TEXT", ReadText );

	drawModDelMI = MenuRegister( "Modify Draw Edit Menu" );
	drawModClose = wMenuPushCreate( drawModDelMI, "", _("Close Polygon - 'c'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'c');
	drawModOpen = wMenuPushCreate( drawModDelMI, "", _("Make PolyLine - 'l'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'l');
	drawModFill = wMenuPushCreate( drawModDelMI, "", _("Fill Polygon - 'f'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'f');
	drawModEmpty = wMenuPushCreate( drawModDelMI, "", _("Empty Polygon - 'e'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'e');
	wMenuSeparatorCreate( drawModDelMI );
	drawModPointsMode = wMenuPushCreate( drawModDelMI, "", _("Points Mode - 'p'"), 0, (wMenuCallBack_p)MenuMode, (void*) 0 );
	drawModDel = wMenuPushCreate( drawModDelMI, "", _("Delete Selected Point - 'Del'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 127 );
	drawModVertex = wMenuPushCreate( drawModDelMI, "", _("Vertex Point - 'v'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'v' );
	drawModRound =  wMenuPushCreate( drawModDelMI, "", _("Round Corner - 'r'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'r' );
	drawModSmooth =  wMenuPushCreate( drawModDelMI, "", _("Smooth Corner - 's'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 's' );
	wMenuSeparatorCreate( drawModDelMI );
	drawModLinMI = wMenuMenuCreate( drawModDelMI, "", _("LineType...") );
	drawModSolid =  wMenuPushCreate( drawModLinMI, "", _("Solid Line"), 0, (wMenuCallBack_p)MenuLine, (void*) '0' );
	drawModDot =  wMenuPushCreate( drawModLinMI, "", _("Dashed Line"), 0, (wMenuCallBack_p)MenuLine, (void*) '1' );
	drawModDash =  wMenuPushCreate( drawModLinMI, "", _("Dotted Line"), 0, (wMenuCallBack_p)MenuLine, (void*) '2' );
	drawModDashDot =  wMenuPushCreate( drawModLinMI, "", _("Dash-Dot Line"), 0, (wMenuCallBack_p)MenuLine, (void*) '3' );
	drawModDashDotDot =  wMenuPushCreate( drawModLinMI, "", _("Dash-Dot-Dot Line"), 0, (wMenuCallBack_p)MenuLine, (void*) '4' );
	wMenuSeparatorCreate( drawModDelMI );
	drawModriginMode = wMenuPushCreate( drawModDelMI, "", _("Origin Mode - 'o'"), 0, (wMenuCallBack_p)MenuMode, (void*) 1 );
	drawModOrigin = wMenuPushCreate( drawModDelMI, "", _("Reset Origin - '0'"), 0, (wMenuCallBack_p)MenuEnter, (void*) '0' );
	drawModLast = wMenuPushCreate( drawModDelMI, "", _("Origin to Selected - 'l'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'l' );
	drawModCenter = wMenuPushCreate( drawModDelMI, "", _("Origin to Centroid - 'c'"), 0, (wMenuCallBack_p)MenuEnter, (void*) 'c');

}
