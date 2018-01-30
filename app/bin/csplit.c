/** \file csplit.c
 * SPLIT
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
#include "i18n.h"
#include "messages.h"
#include "track.h"
#include "utility.h"

static wMenu_p splitPopupM[2];
static wMenuToggle_p splitPopupMI[2][4];
static track_p splitTrkTrk[2];
static EPINX_T splitTrkEP[2];
static BOOL_T splitTrkFlip;

static void ChangeSplitEPMode( wBool_t set, void * mode )
{
	long imode = (long)mode;
	long option;
	int inx0, inx;

	UndoStart( _("Set Block Gaps"), "Set Block Gaps" );
	DrawEndPt( &mainD, splitTrkTrk[0], splitTrkEP[0], wDrawColorWhite );
	DrawEndPt( &mainD, splitTrkTrk[1], splitTrkEP[1], wDrawColorWhite );
	for ( inx0=0; inx0<2; inx0++ ) {
		inx = splitTrkFlip?1-inx0:inx0;
		UndoModify( splitTrkTrk[inx] );
		option = GetTrkEndOption( splitTrkTrk[inx], splitTrkEP[inx] );
		option &= ~EPOPT_GAPPED;
		if ( (imode&1) != 0 )
			option |= EPOPT_GAPPED;
		SetTrkEndOption( splitTrkTrk[inx], splitTrkEP[inx], option );
		imode >>= 1;
	}
	DrawEndPt( &mainD, splitTrkTrk[0], splitTrkEP[0], wDrawColorBlack );
	DrawEndPt( &mainD, splitTrkTrk[1], splitTrkEP[1], wDrawColorBlack );
}

static STATUS_T CmdSplitTrack( wAction_t action, coOrd pos )
{
	track_p trk0, trk1;
	EPINX_T ep0;
	int oldTrackCount;
	int inx, mode, quad;
	ANGLE_T angle;

	switch (action) {
	case C_START:
		InfoMessage( _("Select track to split") );
		/* no break */
	case C_DOWN:
	case C_MOVE:
		return C_CONTINUE;
		break;
	case C_UP:
		onTrackInSplit = TRUE;
		trk0 = OnTrack( &pos, TRUE, TRUE );
		if ( trk0 != NULL) {
			if (!CheckTrackLayer( trk0 ) ) {
				onTrackInSplit = FALSE;
				return C_TERMINATE;
			}
			if (!QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
				onTrackInSplit = FALSE;
				InfoMessage(_("Can't Split that Track"));
				return C_CONTINUE;
			}
			ep0 = PickEndPoint( pos, trk0 );
			onTrackInSplit = FALSE;
			if (ep0 < 0) {
				return C_CONTINUE;
			}
			UndoStart( _("Split Track"), "SplitTrack( T%d[%d] )", GetTrkIndex(trk0), ep0 );
			oldTrackCount = trackCount;
			SplitTrack( trk0, pos, ep0, &trk1, FALSE );
			UndoEnd();
			return C_TERMINATE;
		}
		onTrackInSplit = FALSE;
		return C_TERMINATE;
		break;
	case C_CMDMENU:
		splitTrkTrk[0] = OnTrack( &pos, TRUE, TRUE );
		if ( splitTrkTrk[0] == NULL )
			return C_CONTINUE;
		if ( splitPopupM[0] == NULL ) {
			splitPopupM[0] = MenuRegister( "End Point Mode R-L" );
			splitPopupMI[0][0] = wMenuToggleCreate( splitPopupM[0], "", _("None"), 0, TRUE, ChangeSplitEPMode, (void*)0 );
			splitPopupMI[0][1] = wMenuToggleCreate( splitPopupM[0], "", _("Left"), 0, FALSE, ChangeSplitEPMode, (void*)1 );
			splitPopupMI[0][2] = wMenuToggleCreate( splitPopupM[0], "", _("Right"), 0, FALSE, ChangeSplitEPMode, (void*)2 );
			splitPopupMI[0][3] = wMenuToggleCreate( splitPopupM[0], "", _("Both"), 0, FALSE, ChangeSplitEPMode, (void*)3 );
			splitPopupM[1] = MenuRegister( "End Point Mode T-B" );
			splitPopupMI[1][0] = wMenuToggleCreate( splitPopupM[1], "", _("None"), 0, TRUE, ChangeSplitEPMode, (void*)0 );
			splitPopupMI[1][1] = wMenuToggleCreate( splitPopupM[1], "", _("Top"), 0, FALSE, ChangeSplitEPMode, (void*)1 );
			splitPopupMI[1][2] = wMenuToggleCreate( splitPopupM[1], "", _("Bottom"), 0, FALSE, ChangeSplitEPMode, (void*)2 );
			splitPopupMI[1][3] = wMenuToggleCreate( splitPopupM[1], "", _("Both"), 0, FALSE, ChangeSplitEPMode, (void*)3 );
		}
		splitTrkEP[0] = PickEndPoint( pos, splitTrkTrk[0] );
		angle = NormalizeAngle(GetTrkEndAngle( splitTrkTrk[0], splitTrkEP[0] ));
		if ( angle <= 45.0 )
			quad = 0;
		else if ( angle <= 135.0 )
			quad = 1;
		else if ( angle <= 225.0 )
			quad = 2;
		else if ( angle <= 315.0 )
			quad = 3;
		else
			quad = 0;
		splitTrkFlip = (quad<2);
		if ( (splitTrkTrk[1] = GetTrkEndTrk( splitTrkTrk[0], splitTrkEP[0] ) ) == NULL ) {
			ErrorMessage( MSG_BAD_BLOCKGAP );
			return C_CONTINUE;
		}
		splitTrkEP[1] = GetEndPtConnectedToMe( splitTrkTrk[1], splitTrkTrk[0] );
		mode = 0;
		if ( GetTrkEndOption( splitTrkTrk[1-splitTrkFlip], splitTrkEP[1-splitTrkFlip] ) & EPOPT_GAPPED )
			mode |= 2;
		if ( GetTrkEndOption( splitTrkTrk[splitTrkFlip], splitTrkEP[splitTrkFlip] ) & EPOPT_GAPPED )
			mode |= 1;
		for ( inx=0; inx<4; inx++ )
			wMenuToggleSet( splitPopupMI[quad&1][inx], mode == inx );
		wMenuPopupShow( splitPopupM[quad&1] );
		break;
	}
	return C_CONTINUE;
}




#include "bitmaps/splittrk.xpm"

void InitCmdSplit( wMenu_p menu )
{
	AddMenuButton( menu, CmdSplitTrack, "cmdSplitTrack", _("Split Track"), wIconCreatePixMap(splittrk_xpm), LEVEL0_50, IC_STICKY|IC_POPUP|IC_CMDMENU, ACCL_SPLIT, NULL );
}

