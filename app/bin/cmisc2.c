/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/cmisc2.c,v 1.4 2008-03-10 18:59:53 m_fischer Exp $
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
 * MISC2
 *
 */


static STATUS_T CmdBridge( wAction_t action, coOrd pos )
{
	switch (action) {
	case C_DOWN:
		return C_INFO;
		break;
	}
	return C_INFO;
}





#include "bitmaps/bridge.xbm"

void InitCmdMisc2( wMenu_p menu )
{
	if (extraButtons) {
		InitCommand( menu, CmdBridge, N_("Bridge"), bridge_bits, LEVEL2, IC_STICKY, ACCL_BRIDGE );
	}
}
