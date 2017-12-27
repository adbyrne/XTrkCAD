/*
xtrkcad_spiro - An adapter for Spiro splines within XtrkCAD.

Copyright (C) XtrkCad.org, based on the Spiro Toolkit of Ralph Levien.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

*/


#include "zmisc.h"
#include "common.h"
#include "bezctx.h"
#include "bezctx_xtrkcad.h"
#include "track.h"
#include "tbezier.h"
#include "i18n.h"
#include "math.h"
#include "utility.h"

#define trkSeg(N) DYNARR_N(trkSeg_t,* bc->segsArray, N );


typedef struct {
    bezctx base;
    dynArr_t * segsArray;
    BOOL_T track;
    BOOL_T is_open;
    BOOL_T has_NAN;
    BOOL_T draw_spots;
    coOrd last_pos;						// For moveTo
    int ends[2];						//Start and End knot number

} bezctx_xtrkcad;

static void
bezctx_xtrkcad_moveto(bezctx *z, double x, double y, int is_open) {
    bezctx_xtrkcad *bc = (bezctx_xtrkcad *)z;
    bc->last_pos.x = x;
    bc->last_pos.y = y;
    if (!(isfinite(x) && isfinite(y))) {
        	bc->has_NAN = TRUE;
        	return;
    }
}

static void
bezctx_xtrkcad_lineto(bezctx *z, double x, double y) {

	bezctx_xtrkcad *bc = (bezctx_xtrkcad *)z;
	if (!(isfinite(x) && isfinite(y))) {
	    bc->has_NAN = TRUE;
	}
	if (!bc->is_open || bc->has_NAN) {
		bc->last_pos.x = x;
		bc->last_pos.y = y;
		return;
	}
    DYNARR_APPEND(trkSeg_t,* bc->segsArray,10);
    trkSeg_p seg = &trkSeg(bc->segsArray->cnt-1);
    seg->u.l.pos[0].x = bc->last_pos.x;
    seg->u.l.pos[0].y = bc->last_pos.y;
    seg->u.l.pos[1].x = x;
    seg->u.l.pos[1].y = y;
    seg->u.l.option = 0;
    seg->width = 0.0;
    seg->color = wDrawColorBlack;
    seg->type = SEG_STRTRK;
    if (seg->bezSegs.ptr) MyFree(seg->bezSegs.ptr);
    seg->bezSegs.max =0;
    seg->bezSegs.cnt = 0;
    seg->bezSegs.ptr = NULL;
    seg->u.l.angle = FindAngle(seg->u.l.pos[0],seg->u.l.pos[1]);
    bc->last_pos.x = x;
    bc->last_pos.y = y;


}

static void
bezctx_xtrkcad_quadto(bezctx *z, double x1, double y1, double x2, double y2)
{
    bezctx_xtrkcad *bc = (bezctx_xtrkcad *)z;
    if ((!isfinite(x1) || !isfinite(y1)
    			|| !isfinite(x2) || !isfinite(y2))) {
    	bc->has_NAN = TRUE;
    }
    if (!bc->is_open || bc->has_NAN) {
    	 bc->last_pos.x = x2;
    	 bc->last_pos.y = y2;
    	return;
    }
    DYNARR_APPEND(trkSeg_t,* bc->segsArray,10);
    trkSeg_p seg = &trkSeg(bc->segsArray->cnt-1);
    seg->u.b.pos[0] = bc->last_pos;
    seg->u.b.pos[1].x = x1;
    seg->u.b.pos[1].y = y1;
    seg->u.b.pos[2].x = x1;
    seg->u.b.pos[2].y = y1;
    seg->u.b.pos[3].x = x2;
    seg->u.b.pos[3].y = y2;
    seg->width = 0.0;
    seg->color = wDrawColorBlack;
    seg->type = SEG_BEZTRK;
    if (seg->bezSegs.ptr) MyFree(seg->bezSegs.ptr);
    seg->bezSegs.max =0;
    seg->bezSegs.cnt = 0;
    seg->bezSegs.ptr = NULL;
    bc->last_pos.x = x2;
    bc->last_pos.y = y2;

    FixUpBezierSeg(seg->u.b.pos,seg,bc->track);
}

static void
 bezctx_xtrkcad_curveto(bezctx *z, double x1, double y1, double x2, double y2,
	       double x3, double y3)
{
	bezctx_xtrkcad *bc = (bezctx_xtrkcad *)z;
	if (!(isfinite(x1) && isfinite(y1)
			&& isfinite(x2) && isfinite(y2)
			&& isfinite(x3) && isfinite(y3))) {
		bc->has_NAN = TRUE;
	}
	if (!bc->is_open || bc->has_NAN) {
		bc->last_pos.x = x3;
		bc->last_pos.y = y3;
		return;
	}
	DYNARR_APPEND(trkSeg_t,* bc->segsArray,10);
	trkSeg_p seg = &trkSeg(bc->segsArray->cnt-1);
		seg->u.b.pos[0].x = bc->last_pos.x;
		seg->u.b.pos[0].y = bc->last_pos.y;
	    seg->u.b.pos[1].x = x1;
	    seg->u.b.pos[1].y = y1;
	    seg->u.b.pos[2].x = x2;
	    seg->u.b.pos[2].y = y2;
	    seg->u.b.pos[3].x = x3;
	    seg->u.b.pos[3].y = y3;
	    seg->width = 0.0;
	    seg->color = wDrawColorBlack;
	    seg->type = SEG_BEZTRK;
	    if (seg->bezSegs.ptr) MyFree(seg->bezSegs.ptr);
	    seg->bezSegs.max = 0;
	    seg->bezSegs.cnt = 0;
	    seg->bezSegs.ptr = NULL;
	    bc->last_pos.x = x3;
	    bc->last_pos.y = y3;

	    FixUpBezierSeg(seg->u.b.pos,seg,bc->track);

	 if (bc->draw_spots) {
		 DYNARR_APPEND(trkSeg_t,* bc->segsArray,10);
		 seg = &trkSeg(bc->segsArray->cnt-1);
	 	 seg->type=SEG_FILCRCL;
	 	 seg->u.c.center.x = bc->last_pos.x;
	 	 seg->u.c.center.y = bc->last_pos.y;
	 	 seg->u.c.radius = 0.5;
	 	 seg->width = 0.0;
	 	 seg->color = wDrawColorBlack;
	 }

}

void
bezctx_xtrkcad_mark_knot(bezctx *z, int knot_idx) {

	bezctx_xtrkcad *bc = (bezctx_xtrkcad *)z;
	if (knot_idx >= bc->ends[0]) bc->is_open = TRUE;    //Only worry about segs inside our gap
	if (knot_idx >= bc->ends[1]) bc->is_open = FALSE;

}



bezctx *
new_bezctx_xtrkcad(dynArr_t * segArray, int ends[2], BOOL_T spots) {

    bezctx_xtrkcad *result = znew(bezctx_xtrkcad, 1);

    result->segsArray = segArray;
    result->ends[0] = ends[0];
    result->ends[1] = ends[1];

    result->base.moveto = bezctx_xtrkcad_moveto;
    result->base.lineto = bezctx_xtrkcad_lineto;
    result->base.quadto = bezctx_xtrkcad_quadto;
    result->base.curveto = bezctx_xtrkcad_curveto;
    result->base.mark_knot = bezctx_xtrkcad_mark_knot;
    result->is_open = FALSE;
    result->has_NAN = FALSE;
    result->draw_spots = spots;
    result->track = TRUE;

    return &result->base;
}

BOOL_T bezctx_xtrkcad_close(bezctx *z) {
	bezctx_xtrkcad *bc = (bezctx_xtrkcad *)z;
	if (bc->has_NAN) return FALSE;
	return TRUE;
}


