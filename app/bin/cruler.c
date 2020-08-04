/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cruler.c,v 1.4 2008-03-06 19:35:06 m_fischer Exp $
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

#include "cundo.h"
#include "fileio.h"
#include "i18n.h"
#include "param.h"
#include "track.h"
#include "utility.h"

#include <math.h>

#define AN_OFF (0)
#define AN_FIRST (1)
#define AN_SECOND (2)
#define AN_ON (3)


static struct {
		STATE_T state;
		coOrd pos0;
		coOrd pos1;
		coOrd pos2;
		int modifyingEnd;
		} An = { AN_OFF, { 0,0 }, { 0,0 } };


void DrawAngle(drawCmd_p d, coOrd p0, coOrd p1, coOrd p2, wDrawColor color) {
	char msg[512];
	trkSeg_t seg;
	if ((An.state != AN_OFF) && !IsClose(FindDistance(p0,p1))) {
		seg.type = SEG_STRLIN;
		seg.width = 0;
		seg.color = wDrawColorBlack;
		seg.u.l.pos[0] = p0;
		seg.u.l.pos[1] = p1;
		DrawSegs(d,zero,0.0,&seg,1,trackGauge,color);

		if (!(IsClose(FindDistance(p0,p2)))) {
			seg.type = SEG_STRLIN;
			seg.width = 0;
			seg.color = wDrawColorBlack;
			seg.u.l.pos[0] = p0;
			seg.u.l.pos[1] = p2;
			DrawSegs(d,zero,0.0,&seg,1,trackGauge,color);

			DIST_T r = min(FindDistance(p0,p2),FindDistance(p0,p1))/2;
			ANGLE_T a = DifferenceBetweenAngles(FindAngle(p0,p1),FindAngle(p0,p2));
			ANGLE_T a0;

			if (a>=0)
				a0 = FindAngle(p0,p1);
			else
				a0 = FindAngle(p0,p2);
			ANGLE_T a1 = fabs(DifferenceBetweenAngles(FindAngle(p0,p1),FindAngle(p0,p2)));
			seg.type = SEG_CRVLIN;
			seg.u.c.center = p0;
			seg.u.c.radius = r;
			seg.u.c.a0 = a0;
			seg.u.c.a1 = a1;
			DrawSegs(d,zero,0.0,&seg,1,trackGauge,color);

			coOrd p;
			Translate(&p, p0, a1/2+a0, r );
			seg.type = SEG_TEXT;
			seg.u.t.angle = 0.0;
			sprintf(msg,"RA: %0.3f",a1);
			seg.u.t.string = msg;
			seg.u.t.pos = p;
			seg.u.t.fontSize = 10.0*d->scale;
			seg.u.t.fontP = NULL;
			seg.u.t.boxed = FALSE;
			DrawSegs(d,zero,0.0,&seg,1,trackGauge,color);
		}
	}

}


static STATUS_T CmdAngle( wAction_t action, coOrd pos )
{
	switch (action) {

	case C_START:
		switch (An.state) {
		case AN_OFF:
			An.state = AN_ON;
			break;
		case AN_ON:
			An.state = AN_OFF;
		case AN_FIRST:
		case AN_SECOND:
			An.state = AN_OFF;
			break;
		}
		return C_CONTINUE;

	case C_DOWN:
		switch (An.state) {
		case AN_OFF:
		case AN_ON:
			An.pos0 = An.pos1 = An.pos2 = pos;
			An.state = AN_FIRST;
			InfoMessage( "Drag out base line" );
			break;
		case AN_FIRST:
			An.pos2 = pos;
			An.state = AN_SECOND;
		    InfoMessage( "Drag Angle" );
		    break;
		}
		return C_CONTINUE;

	case C_MOVE:
		//Lock to 90 degrees with CTRL
		if (MyGetKeyState()&WKEY_CTRL) {
			ANGLE_T line_angle;
			if (An.state == AN_FIRST) line_angle = 0.0;
			else line_angle = FindAngle(An.pos0,An.pos1);
			DIST_T l = FindDistance(An.pos0, pos);
			if (!IsClose(l)) {
				ANGLE_T angle2 = NormalizeAngle(FindAngle(An.pos0, pos)-line_angle);
				int oct = (int)((angle2 + 22.5) / 45.0);
				l = fabs(l*cos(D2R(angle2-oct*45)));
				Translate( &pos, An.pos0, oct*45.0+line_angle, l );
			}
		}
		switch (An.state) {
			case AN_FIRST:
				An.pos1 = An.pos2 = pos;
				break;
			case AN_SECOND:
				An.pos2 = pos;
				break;
			default:;
		}
		if (An.state == AN_FIRST)
			InfoMessage( "Base Angle %0.3f",FindAngle(An.pos0,An.pos1));
		if (An.state == AN_SECOND)
			InfoMessage( "Base Angle %0.3f, Relative Angle %0.3f",FindAngle(An.pos0,An.pos1),
					fabs(DifferenceBetweenAngles(FindAngle(An.pos0,An.pos1),FindAngle(An.pos0,An.pos2))));
		return C_CONTINUE;

	case C_UP:
		if (An.state == AN_SECOND) return C_TERMINATE;
		return C_CONTINUE;

	case C_REDRAW:
		if (An.state != AN_OFF) {
			if (!IsClose(FindDistance(An.pos1,An.pos2))) {
				DrawAngle( &tempD, An.pos0, An.pos1, An.pos2, wDrawColorBlack );
			}
		}
		return C_CONTINUE;

	case C_CANCEL:
		return C_TERMINATE;

	}
	return C_CONTINUE;

}


/*****************************************************************************
 *
 * RULER
 *
 */




#define DR_OFF (0)
#define DR_ON  (1)

static struct {
		STATE_T state;
		coOrd pos0;
		coOrd pos1;
		int modifyingEnd;
		} Dr = { DR_OFF, { 0,0 }, { 0,0 } };

void RulerRedraw( BOOL_T demo )
{
	if (Dr.state == DR_ON)
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
	if (demo) {
		Dr.state = DR_OFF;
		An.state = AN_OFF;
	}
	if (An.state != AN_OFF)
		DrawAngle( &tempD, An.pos0, An.pos1, An.pos2, wDrawColorBlack);

}

static STATUS_T CmdRuler( wAction_t action, coOrd pos )
{
	switch (action) {

	case C_START:
		switch (Dr.state) {
		case DR_OFF:
			Dr.state = DR_ON;
			InfoMessage( "%s", FormatDistance( FindDistance( Dr.pos0, Dr.pos1 ) ) );
			break;
		case DR_ON:
			Dr.state = DR_OFF;
			break;
		}
		return C_CONTINUE;

	case C_DOWN:
		Dr.pos0 = Dr.pos1 = pos;
		Dr.state = DR_ON;
		InfoMessage( "0.0" );
		return C_CONTINUE;

	case C_MOVE:
		Dr.pos1 = pos;
		InfoMessage( "%s", FormatDistance( FindDistance( Dr.pos0, Dr.pos1 ) ) );
		return C_CONTINUE;

	case C_UP:
		inError = TRUE;
		return C_TERMINATE;

	case C_REDRAW:
		if (Dr.state == DR_ON) {
			DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		}
		return C_CONTINUE;

	case C_CANCEL:
		return C_TERMINATE;

	}
	return C_CONTINUE;
}


STATUS_T ModifyRuler(
		wAction_t action,
		coOrd pos )
{
	switch (action&0xFF) {
	case C_DOWN:
		Dr.modifyingEnd = -1;
		if ( Dr.state != DR_ON )
			return C_ERROR;
		if ( FindDistance( pos, Dr.pos0 ) < mainD.scale*0.25 ) {
			Dr.modifyingEnd = 0;
		} else if ( FindDistance( pos, Dr.pos1 ) < mainD.scale*0.25 ) {
			Dr.modifyingEnd = 1;
		} else {
			return C_ERROR;
		}
	case C_MOVE:
		if ( Dr.modifyingEnd == 0 ) {
			Dr.pos0 = pos;
		} else {
			Dr.pos1 = pos;
		}
		InfoMessage( "%s", FormatDistance( FindDistance( Dr.pos0, Dr.pos1 ) ) );
		return C_CONTINUE;
	case C_UP:
		return C_CONTINUE;
	case C_REDRAW:
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		break;
	default:
		return C_ERROR;
	}
	return C_CONTINUE;
}


#include "bitmaps/ruler.xpm"
#include "bitmaps/angle.xpm"

void InitCmdRuler( wMenu_p menu )
{
	ButtonGroupBegin( _("Measurement"), "cmdMeasureSetCmd", _("Measurement") );
	AddMenuButton( menu, CmdRuler, "cmdRuler", _("Ruler"), wIconCreatePixMap(ruler_xpm), LEVEL0, IC_STICKY|IC_NORESTART, ACCL_RULER, NULL );
	AddMenuButton( menu, CmdAngle, "cmdAngle", _("Protractor"), wIconCreatePixMap(angle_xpm), LEVEL0, IC_STICKY|IC_NORESTART, ACCL_ANGLE, NULL );
	ButtonGroupEnd();
}
