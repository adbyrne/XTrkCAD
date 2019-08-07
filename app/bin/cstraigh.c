/** \file cstraigh.c
 * STRAIGHT
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

#include "cstraigh.h"
#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "layout.h"

/*
 * STATE INFO
 */
static struct {
		coOrd pos0, pos1;
		track_p trk;
		EPINX_T ep;
		BOOL_T down;
		} Dl;

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


static STATUS_T CmdStraight( wAction_t action, coOrd pos )
{
	track_p t;
	DIST_T dist;
	coOrd p;

	switch (action&0xFF) {

	case C_START:
		DYNARR_RESET(trkSeg_t,anchors_da);
		Dl.pos0=pos;
		Dl.pos1=pos;
		Dl.trk = NULL;
		Dl.ep=-1;
		Dl.down = FALSE;
		InfoMessage( _("Place 1st end point of straight track + Shift -> snap to unconnected endpoint") );
		return C_CONTINUE;

	case C_DOWN:
		DYNARR_RESET(trkSeg_t,anchors_da);
		p = pos;
		BOOL_T found = FALSE;
		Dl.trk = NULL;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			if ((t = OnTrack(&p, FALSE, TRUE)) != NULL) {
			   EPINX_T ep = PickUnconnectedEndPointSilent(p, t);
			   if (ep != -1) {
				   if (GetTrkScale(t) != (char)GetLayoutCurScale()) {
				   		wBeep();
				   		InfoMessage(_("Track is different scale"));
				   		return C_CONTINUE;
				   	}
			   		Dl.trk = t;
			   		Dl.ep = ep;
			   		pos = GetTrkEndPos(t, ep);
			   		found = TRUE;
			   } else {
				   InfoMessage(_("No unconnected end-point on track - Try again or release Shift and click"));
				   Dl.pos0=pos;
				   Dl.pos1=pos;
				   return C_CONTINUE;
			   }
			} else {
				InfoMessage(_("Not on a track - Try again or release Shift and click"));
				Dl.pos0=pos;
				Dl.pos1=pos;
				return C_CONTINUE;
			}
		}
		Dl.down = TRUE;	
		if (!found) SnapPos( &pos );
		Dl.pos0 = pos;
		InfoMessage( _("Drag to place 2nd end point") );
		DYNARR_SET( trkSeg_t, tempSegs_da, 1 );
		tempSegs(0).color = wDrawColorBlack;
		tempSegs(0).width = 0;
		tempSegs_da.cnt = 0;
		tempSegs(0).type = SEG_STRTRK;
		tempSegs(0).u.l.pos[0] = pos;
		return C_CONTINUE;

	case C_MOVE:
	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (!Dl.down) {
			if ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) == WKEY_SHIFT) {
				p = pos;
				if ((t = OnTrack(&p, FALSE, TRUE)) != NULL) {
					if (GetTrkScale(t) == (char)GetLayoutCurScale()) {
					   EPINX_T ep = PickUnconnectedEndPointSilent(pos, t);
					   if (ep != -1) {
							   CreateEndAnchor(GetTrkEndPos(t,ep),TRUE);
					   }
					}
				}
			}
			if (anchors_da.cnt)
				DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			return C_CONTINUE;
		}
		//DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		ANGLE_T angle, angle2;
		if (Dl.trk) {
			angle = NormalizeAngle(GetTrkEndAngle( Dl.trk, Dl.ep));
			angle2 = NormalizeAngle(FindAngle(pos, Dl.pos0)-angle);
			if (angle2 > 90.0 && angle2 < 270.0)
				Translate( &pos, Dl.pos0, angle, FindDistance( Dl.pos0, pos ) );
			else pos = Dl.pos0;	
		} else 	SnapPos( &pos );
		
		InfoMessage( _("Straight Track Length=%s Angle=%0.3f"),
				FormatDistance(FindDistance( Dl.pos0, pos )),
				PutAngle(FindAngle( Dl.pos0, pos )) );
		tempSegs(0).u.l.pos[1] = pos;
		tempSegs_da.cnt = 1;
		MainRedraw();
		//DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_UP:
		DYNARR_RESET(trkSeg_t,anchors_da);
		if (!Dl.down) return C_CONTINUE;
		//DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		tempSegs_da.cnt = 0;
		if (Dl.trk) {
			angle = NormalizeAngle(GetTrkEndAngle( Dl.trk, Dl.ep));
			angle2 = NormalizeAngle(FindAngle(pos, Dl.pos0)-angle);
			if (angle2 > 90.0 && angle2 < 270.0)
				Translate( &pos, Dl.pos0, angle, FindDistance( Dl.pos0, pos ));
			else pos = Dl.pos0;	
		} else 	SnapPos( &pos );
		if ((dist=FindDistance( Dl.pos0, pos )) <= minLength) {
		   ErrorMessage( MSG_TRK_TOO_SHORT, "Straight ", PutDim(fabs(minLength-dist)) );
		   return C_TERMINATE;
		}
		UndoStart( _("Create Straight Track"), "newStraight" );
		t = NewStraightTrack( Dl.pos0, pos );
		if (Dl.trk) {
			ConnectTracks(Dl.trk, Dl.ep, t, 0);
		}
		UndoEnd();
		DrawNewTrack(t);
		return C_TERMINATE;

	case C_REDRAW:
		if (anchors_da.cnt)
			DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		if (Dl.down)
			DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;
	case C_CANCEL:
		Dl.down = FALSE;
		MainRedraw();
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}


#include "bitmaps/straight.xpm"

void InitCmdStraight( wMenu_p menu )
{
	AddMenuButton( menu, CmdStraight, "cmdStraight", _("Straight Track"), wIconCreatePixMap(straight_xpm), LEVEL0_50, IC_STICKY|IC_POPUP2|IC_WANT_MOVE, ACCL_STRAIGHT, NULL );
}
