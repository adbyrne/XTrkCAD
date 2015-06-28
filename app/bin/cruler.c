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

#include "track.h"
#include "i18n.h"

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
	if (demo)
		Dr.state = DR_OFF;
}

static STATUS_T CmdRuler( wAction_t action, coOrd pos )
{
	switch (action) {

	case C_START:
		switch (Dr.state) {
		case DR_OFF:
			DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
			Dr.state = DR_ON;
			InfoMessage( "%s", FormatDistance( FindDistance( Dr.pos0, Dr.pos1 ) ) );
			break;
		case DR_ON:
			DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
			Dr.state = DR_OFF;
			break;
		}
        MainRedraw();
		return C_CONTINUE;

	case C_DOWN:
		if (Dr.state == DR_ON) {
			DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		}
		Dr.pos0 = Dr.pos1 = pos;
		Dr.state = DR_ON;
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		InfoMessage( "0.0" );
        MainRedraw();
		return C_CONTINUE;

	case C_MOVE:
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		Dr.pos1 = pos;
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		InfoMessage( "%s", FormatDistance( FindDistance( Dr.pos0, Dr.pos1 ) ) );
        MainRedraw();
		return C_CONTINUE;

	case C_UP:
		inError = TRUE;
        MainRedraw();
		return C_TERMINATE;

	case C_REDRAW:
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
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		if ( Dr.modifyingEnd == 0 ) {
			Dr.pos0 = pos;
		} else {
			Dr.pos1 = pos;
		}
		DrawRuler( &tempD, Dr.pos0, Dr.pos1, 0.0, TRUE, TRUE, wDrawColorBlack );
		InfoMessage( "%s", FormatDistance( FindDistance( Dr.pos0, Dr.pos1 ) ) );
        MainRedraw();
		return C_CONTINUE;
	case C_UP:
		return C_CONTINUE;
	default:
		return C_ERROR;
	}
}


#include "bitmaps/ruler.xpm"

void InitCmdRuler( wMenu_p menu )
{
	AddMenuButton( menu, CmdRuler, "cmdRuler", _("Ruler"), wIconCreatePixMap(ruler_xpm), LEVEL0, IC_STICKY|IC_NORESTART, ACCL_RULER, NULL );
}
