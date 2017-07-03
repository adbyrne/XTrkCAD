/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cstraigh.c,v 1.4 2008-03-06 19:35:06 m_fischer Exp $
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

#include "track.h"
#include "cstraigh.h"
#include "i18n.h"

/*******************************************************************************
 *
 * STRAIGHT
 *
 */

/*
 * STATE INFO
 */
static struct {
		coOrd pos0, pos1;
		track_p trk;
		EPINX_T ep;
		
		} Dl;


static STATUS_T CmdStraight( wAction_t action, coOrd pos )
{
	track_p t;
	DIST_T dist;
	coOrd p;

	switch (action) {

	case C_START:
		InfoMessage( _("Place 1st end point of Straight track + Shift -> snap to unconnected endpoint") );
		return C_CONTINUE;

	case C_DOWN:
		p = pos;
		BOOL_T found = FALSE;
		Dl.trk = NULL;
		if ((MyGetKeyState() & WKEY_SHIFT) != 0) {
			if ((t = OnTrack(&p, TRUE, TRUE)) != NULL) {
			   EPINX_T ep = PickUnconnectedEndPoint(p, t);
			   if (ep != -1) {
			   		Dl.trk = t;
			   		Dl.ep = ep;
			   		pos = GetTrkEndPos(t, ep);
			   		found = TRUE;
			   } else {
				   return C_CONTINUE;
			   }
			} else {
				ErrorMessage(_("Not on a Track"));
				return C_CONTINUE;
			}
		} 	
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
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
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
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_UP:
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
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
	case C_CANCEL:
		DrawSegs( &tempD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	default:
		return C_CONTINUE;
	}
}


#include "bitmaps/straight.xpm"

void InitCmdStraight( wMenu_p menu )
{
	AddMenuButton( menu, CmdStraight, "cmdStraight", _("Straight Track"), wIconCreatePixMap(straight_xpm), LEVEL0_50, IC_STICKY|IC_POPUP2, ACCL_STRAIGHT, NULL );
}
