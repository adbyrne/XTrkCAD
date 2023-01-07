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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "cundo.h"
#include "compound.h"
#include "cselect.h"
#include "track.h"
#include "draw.h"
#include "fileio.h"
#include "common-ui.h"

static wMenu_p splitPopupM[2];
static wMenuToggle_p splitPopupMI[2][4];
static track_p splitTrkTrk[2];
static EPINX_T splitTrkEP[2];
static BOOL_T splitTrkFlip;
static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

static void ChangeSplitEPMode( wBool_t set, void * mode )
{
	long imode = VP2L(mode);
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
		if ( (imode&1) != 0 ) {
			option |= EPOPT_GAPPED;
		}
		SetTrkEndOption( splitTrkTrk[inx], splitTrkEP[inx], option );
		imode >>= 1;
	}
	DrawEndPt( &mainD, splitTrkTrk[0], splitTrkEP[0], wDrawColorBlack );
	DrawEndPt( &mainD, splitTrkTrk[1], splitTrkEP[1], wDrawColorBlack );
}

static void CreateSplitAnchorAngle(coOrd pos, track_p t, BOOL_T end, ANGLE_T a,
                                   BOOL_T trim)
{
	DIST_T d = tempD.scale*0.1;
	DIST_T w = tempD.scale/tempD.dpi*4;
	int i;
	if (!end) {
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).color = wDrawColorBlue;
		Translate(&anchors(i).u.l.pos[0],pos,a,trim?2*GetTrkGauge(t):GetTrkGauge(t));
		Translate(&anchors(i).u.l.pos[1],pos,a,trim?2*-GetTrkGauge(t):-GetTrkGauge(t));
		anchors(i).width = w;
	} else {
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).color = wDrawColorBlue;
		Translate(&anchors(i).u.l.pos[0],pos,a,GetTrkGauge(t));
		Translate(&anchors(i).u.l.pos[0],anchors(i).u.l.pos[0],a+90,d);
		Translate(&anchors(i).u.l.pos[1],pos,a,-GetTrkGauge(t));
		Translate(&anchors(i).u.l.pos[1],anchors(i).u.l.pos[1],a+90,-d);
		anchors(i).width = w;
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).color = wDrawColorBlue;
		Translate(&anchors(i).u.l.pos[0],pos,a,GetTrkGauge(t));
		Translate(&anchors(i).u.l.pos[0],anchors(i).u.l.pos[0],a+90,-d);
		Translate(&anchors(i).u.l.pos[1],pos,a,-GetTrkGauge(t));
		Translate(&anchors(i).u.l.pos[1],anchors(i).u.l.pos[1],a+90,d);
		anchors(i).width = w;
	}
}

static void CreateSplitAnchor(coOrd pos, track_p t, BOOL_T end)
{
	ANGLE_T a = NormalizeAngle(GetAngleAtPoint(t,pos,NULL,NULL)+90.0);
	CreateSplitAnchorAngle(pos,t,end,a,FALSE);
}

static void CreateTrimAnchorLeg(coOrd pos, ANGLE_T a, track_p t)
{
	DIST_T w = tempD.scale/tempD.dpi*4;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_STRLIN;
	anchors(i).color = wDrawColorBlue;
	anchors(i).u.l.pos[0] = pos;
	Translate(&anchors(i).u.l.pos[1],pos,a,GetTrkGauge(t)*2);
	anchors(i).width = w;

}

static void CreateTrimAnchor(coOrd pos, track_p t, track_p s, coOrd cursor)
{
	ANGLE_T a = NormalizeAngle(GetAngleAtPoint(s,pos,NULL,NULL));
	CreateSplitAnchorAngle(pos,t,FALSE,a,TRUE);
	ANGLE_T aa = FindAngle(pos,cursor);
	ANGLE_T d = DifferenceBetweenAngles(a,aa);
	CreateTrimAnchorLeg(pos,a+(d>0?90:-90),t);
}

static STATUS_T CmdSplitTrack( wAction_t action, coOrd pos )
{
	track_p trk0, trk1;
	EPINX_T ep0 = 0;
//	int oldTrackCount;
	int inx, mode, quad;
	ANGLE_T angle;

	switch (action) {
	case C_START:
		InfoMessage( _("Select track to split") );
		DYNARR_RESET(trkSeg_t,anchors_da);
		SetAllTrackSelect( FALSE );
	/* no break */
	case C_DOWN:
	case C_MOVE:
		return C_CONTINUE;
		break;
	case C_UP:
		onTrackInSplit = TRUE;
		trk0 = OnTrack( &pos, FALSE, TRUE );
		if ( trk0 != NULL) {
			if (!CheckTrackLayer( trk0 ) ) {
				onTrackInSplit = FALSE;
				return C_TERMINATE;
			}
			ep0 = PickEndPoint( pos, trk0 );
			if (IsClose(FindDistance(GetTrkEndPos(trk0,ep0),pos))
			    && (GetTrkEndTrk(trk0,ep0)!=NULL)) {
				pos = GetTrkEndPos(trk0,ep0);
			} else {
				if (!IsTrack(trk0) ||
				    !QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
					onTrackInSplit = FALSE;
					InfoMessage(_("Can't Split that Track Object"));
					return C_CONTINUE;
				}
			}
			onTrackInSplit = FALSE;
			if (ep0 < 0) {
				return C_CONTINUE;
			}
			UndoStart( _("Split Track"), "SplitTrack( T%d[%d] )", GetTrkIndex(trk0), ep0 );
//			oldTrackCount = trackCount;
			SplitTrack( trk0, pos, ep0, &trk1, FALSE );
			UndoEnd();
			return C_TERMINATE;
		} else if ((trk0 = OnTrack( &pos, FALSE, FALSE))!=NULL
		           && CheckTrackLayerSilent( trk0 )) {
			if (!QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
				onTrackInSplit = FALSE;
				InfoMessage(_("Can't Split that Draw Object"));
				return C_CONTINUE;
			}
			onTrackInSplit = FALSE;
			UndoStart( _("Split Track"), "SplitTrack( T%d[%d] )", GetTrkIndex(trk0), ep0 );
//			oldTrackCount = trackCount;
			SplitTrack( trk0, pos, ep0, &trk1, FALSE );
			UndoEnd();
			return C_TERMINATE;
		} else {
			InfoMessage(_("No Track to Split"));
			wBeep();
		}
		onTrackInSplit = FALSE;
		return C_TERMINATE;
		break;
	case C_CMDMENU:
		splitTrkTrk[0] = OnTrack( &pos, TRUE, TRUE );
		if ( splitTrkTrk[0] == NULL ) {
			return C_CONTINUE;
		}
		if ( splitPopupM[0] == NULL ) {
			splitPopupM[0] = MenuRegister( "End Point Mode R-L" );
			splitPopupMI[0][0] = wMenuToggleCreate( splitPopupM[0], "", _("None"), 0, TRUE,
			                                        ChangeSplitEPMode, I2VP(0) );
			splitPopupMI[0][1] = wMenuToggleCreate( splitPopupM[0], "", _("Left"), 0, FALSE,
			                                        ChangeSplitEPMode, I2VP(1) );
			splitPopupMI[0][2] = wMenuToggleCreate( splitPopupM[0], "", _("Right"), 0,
			                                        FALSE, ChangeSplitEPMode, I2VP(2) );
			splitPopupMI[0][3] = wMenuToggleCreate( splitPopupM[0], "", _("Both"), 0, FALSE,
			                                        ChangeSplitEPMode, I2VP(3) );
			splitPopupM[1] = MenuRegister( "End Point Mode T-B" );
			splitPopupMI[1][0] = wMenuToggleCreate( splitPopupM[1], "", _("None"), 0, TRUE,
			                                        ChangeSplitEPMode, I2VP(0) );
			splitPopupMI[1][1] = wMenuToggleCreate( splitPopupM[1], "", _("Top"), 0, FALSE,
			                                        ChangeSplitEPMode, I2VP(1) );
			splitPopupMI[1][2] = wMenuToggleCreate( splitPopupM[1], "", _("Bottom"), 0,
			                                        FALSE, ChangeSplitEPMode, I2VP(2) );
			splitPopupMI[1][3] = wMenuToggleCreate( splitPopupM[1], "", _("Both"), 0, FALSE,
			                                        ChangeSplitEPMode, I2VP(3) );
		}
		splitTrkEP[0] = PickEndPoint( pos, splitTrkTrk[0] );
		angle = NormalizeAngle(GetTrkEndAngle( splitTrkTrk[0], splitTrkEP[0] ));
		if ( angle <= 45.0 ) {
			quad = 0;
		} else if ( angle <= 135.0 ) {
			quad = 1;
		} else if ( angle <= 225.0 ) {
			quad = 2;
		} else if ( angle <= 315.0 ) {
			quad = 3;
		} else {
			quad = 0;
		}
		splitTrkFlip = (quad<2);
		if ( (splitTrkTrk[1] = GetTrkEndTrk( splitTrkTrk[0],
		                                     splitTrkEP[0] ) ) == NULL ) {
			ErrorMessage( MSG_BAD_BLOCKGAP );
			return C_CONTINUE;
		}
		splitTrkEP[1] = GetEndPtConnectedToMe( splitTrkTrk[1], splitTrkTrk[0] );
		mode = 0;
		if ( GetTrkEndOption( splitTrkTrk[1-splitTrkFlip],
		                      splitTrkEP[1-splitTrkFlip] ) & EPOPT_GAPPED ) {
			mode |= 2;
		}
		if ( GetTrkEndOption( splitTrkTrk[splitTrkFlip],
		                      splitTrkEP[splitTrkFlip] ) & EPOPT_GAPPED ) {
			mode |= 1;
		}
		for ( inx=0; inx<4; inx++ ) {
			wMenuToggleSet( splitPopupMI[quad&1][inx], mode == inx );
		}
		menuPos = pos;
		wMenuPopupShow( splitPopupM[quad&1] );
		break;
	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		onTrackInSplit = TRUE;
		if ((trk0 = OnTrack( &pos, FALSE, TRUE ))!=NULL
		    && CheckTrackLayerSilent( trk0 )) {
			ep0 = PickEndPoint( pos, trk0 );
			if ( ep0 < 0 ) {
				break;
			}
			if (IsClose(FindDistance(GetTrkEndPos(trk0,ep0),pos))
			    && (GetTrkEndTrk(trk0,ep0)!=NULL)) {
				CreateSplitAnchor(GetTrkEndPos(trk0,ep0),trk0,TRUE);
			} else if (QueryTrack(trk0,Q_IS_TURNOUT)) {
				if ((MyGetKeyState()&WKEY_SHIFT) != 0 ) {
					if (SplitTurnoutCheck(trk0,pos,ep0,NULL,NULL,NULL,TRUE,&pos,&angle)) {
						angle = NormalizeAngle(angle+90);
						CreateSplitAnchorAngle(pos,trk0,FALSE,angle,FALSE);
					}
				} else {
					CreateSplitAnchor(GetTrkEndPos(trk0,ep0),trk0,TRUE);
				}
				break;
			} else if (QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
				CreateSplitAnchor(pos,trk0,FALSE);
			}
		} else {
			if ((trk0 = OnTrack( &pos, FALSE, FALSE))!=NULL
			    && CheckTrackLayerSilent( trk0 )) {
				if (QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
					CreateSplitAnchor(pos,trk0, FALSE);
				}
			}
		}
		onTrackInSplit = FALSE;

		break;
	case C_REDRAW:
		if (anchors_da.cnt) {
			DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge,
			          wDrawColorBlack );
		}
		break;
	}

	return C_CONTINUE;
}

static STATUS_T CmdSplitDraw( wAction_t action, coOrd pos )
{
	track_p trk0, trk1;
	EPINX_T ep0 = 0;
//	int oldTrackCount;

	switch (action) {
	case C_START:
		InfoMessage( _("Select draw to split") );
		DYNARR_RESET(trkSeg_t,anchors_da);
		SetAllTrackSelect( FALSE );
	/* no break */
	case C_DOWN:
	case C_MOVE:
		return C_CONTINUE;
		break;
	case C_UP:
		onTrackInSplit = TRUE;
		if ((trk0 = OnTrack( &pos, FALSE, FALSE))!=NULL
		    && CheckTrackLayerSilent( trk0 )) {
			if (IsTrack(trk0)) { return C_CONTINUE; }
			if (!QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
				onTrackInSplit = FALSE;
				InfoMessage(_("Can't Split that Draw Object"));
				return C_CONTINUE;
			}
			onTrackInSplit = FALSE;
			UndoStart( _("Split Draw"), "SplitDraw( T%d[%d] )", GetTrkIndex(trk0), ep0 );
//			oldTrackCount = trackCount;
			SplitTrack( trk0, pos, ep0, &trk1, FALSE );
			UndoEnd();
			return C_TERMINATE;
		} else {
			InfoMessage(_("No Draw to Split"));
			wBeep();
		}
		onTrackInSplit = FALSE;
		return C_TERMINATE;
		break;
	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		onTrackInSplit = TRUE;
		if ((trk0 = OnTrack( &pos, FALSE, FALSE))!=NULL
		    && CheckTrackLayerSilent( trk0 )) {
			if (IsTrack(trk0)) { break; }
			if (QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
				CreateSplitAnchor(pos,trk0, FALSE);
			}
		}
		onTrackInSplit = FALSE;
		break;
	case C_REDRAW:
		if (anchors_da.cnt) {
			DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge,
			          wDrawColorBlack );
		}
		break;
	}

	return C_CONTINUE;
}

typedef enum {TRIM_NONE, TRIM_LINE} TrimState_e;

static STATUS_T CmdTrimDraw( wAction_t action, coOrd pos )
{
	track_p trk0, trk1, trk2;
	EPINX_T ep0 = 0;
	static TrimState_e trimState;
	static track_p trimLine;
	static track_p trk;

	switch (action&0xFF) {
	case C_START:
		InfoMessage( _("Select the draw object to Trim to") );
		DYNARR_RESET(trkSeg_t,anchors_da);
		trimState = TRIM_NONE;
		trimLine = NULL;
		trk = NULL;
		SetAllTrackSelect( FALSE );
	/* no break */
	case C_DOWN:
	case C_MOVE:
		return C_CONTINUE;
		break;
	case C_UP:
		if (trimState == TRIM_NONE) {
			if ((trk0 = OnTrack( &pos, FALSE, FALSE))!=NULL
			    && CheckTrackLayerSilent( trk0 )) {
				if (IsTrack(trk0))  {
					InfoMessage(_("Can't Trim with a Track"));
					return C_CONTINUE;
				}
				trimState = TRIM_LINE;
				trimLine = trk0;
				InfoMessage( _("Select an intersecting draw object to Trim") );
				return C_CONTINUE;
			} else { return C_CONTINUE; }
		}
		if (!trimLine) {
			InfoMessage(_("No Draw to Trim with"));
			wBeep();
			return C_TERMINATE;
		}
		coOrd pos1 = pos;
		if ((trk1 = OnTrackIgnore(&pos1,FALSE,FALSE,trimLine))!=NULL) {
			if (IsTrack(trk1)) {
				InfoMessage(_("Can't Split a track object"));
				wBeep();
				return C_CONTINUE;
			}
			if (!QueryTrack(trk1,Q_MODIFY_CAN_SPLIT)) {
				onTrackInSplit = FALSE;
				InfoMessage(_("Can't Split that Draw Object"));
				return C_CONTINUE;
			}
			pos1 = pos;
			if (IsClose(GetTrkDistance(trimLine,&pos1)*4)) {
				if ( IsClose(GetTrkDistance(trk1,&pos1)*4)) {
					//Iterate twice
					for (int i=0; i<2; i++) {
						GetTrkDistance(trimLine,&pos1);
						GetTrkDistance(trk1,&pos1);
					}
				} else { return C_CONTINUE; }
			} else {
				return C_CONTINUE;
			}
		} else { return C_CONTINUE; }

		ANGLE_T a = GetAngleAtPoint(trk1,pos1,NULL,NULL);
		ANGLE_T aa = DifferenceBetweenAngles(a,FindAngle(pos1,pos));
		if (fabs(aa)<90 ) { ep0 = 1; }
		else { ep0 = 0; }

		UndoStart( _("Trim Draw"), "TrimDraw( T%d[%d] )", GetTrkIndex(trimLine), ep0 );
		SplitTrack( trk1, pos1, ep0, &trk2, FALSE );
		if (trk2 ) { DeleteTrack(trk2, FALSE); }
		UndoEnd();
		MainRedraw();
		InfoMessage( _("Select another draw object to Trim, or Space to Deselect") );
		return C_CONTINUE;
		break;
	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		trk = NULL;
		if (trimState == TRIM_NONE) {
			if ((trk0 = OnTrack( &pos, FALSE, FALSE))!=NULL
			    && CheckTrackLayerSilent( trk0 )) {
				if (IsTrack(trk0)) { break; }
				if (QueryTrack(trk0,Q_MODIFY_CAN_SPLIT)) {
					trk = trk0;
				}
			}
		}
		if (trimState == TRIM_LINE) {
			coOrd pos1=pos;
			if ((trk1 = OnTrackIgnore(&pos1,FALSE,FALSE,trimLine))!=NULL) {
				if (IsTrack(trk1)) {
					return C_CONTINUE;
				}
				pos1 = pos;
				if (IsClose(GetTrkDistance(trimLine,&pos1)*4)) {
					if (IsClose(GetTrkDistance(trk1,&pos1)*4)) {
						//Iterate Twice
						for (int i=0; i<2; i++) {
							GetTrkDistance(trimLine,&pos1);
							GetTrkDistance(trk1,&pos1);
						}
						CreateTrimAnchor(pos1, trk1, trimLine, pos);
					}
				}
			}
		}
		break;
	case C_REDRAW:
		if (trk) {
			DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
		}
		if (trimLine) {
			DrawTrack(trimLine,&tempD,selectedColor);
		}
		if (anchors_da.cnt) {
			DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge,
			          wDrawColorBlack );
		}
		break;
	case C_TEXT:
		if (action>>8 != ' ' && action>>8 != 13 ) {
			return C_CONTINUE;
		}
		trimLine = NULL;
		trk = NULL;
		trimState = TRIM_NONE;
		InfoMessage("");
		return C_TERMINATE;
	default: ;
	}

	return C_CONTINUE;
}


#include "bitmaps/split.xpm3"
#include "bitmaps/split-draw.xpm3"
#include "bitmaps/trim.xpm3"

void InitCmdSplit( wMenu_p menu )
{
	ButtonGroupBegin( _("Split"), "cmdSplitSetCmd", _("Split") );
	AddMenuButton( menu, CmdSplitTrack, "cmdSplitTrack", _("Split Track"),
	               wIconCreatePixMap(split_xpm3[iconSize]), LEVEL0_50,
	               IC_STICKY|IC_POPUP|IC_CMDMENU|IC_WANT_MOVE, ACCL_SPLIT,  NULL);
	AddMenuButton( menu, CmdSplitDraw, "cmdSplitDraw", _("Split Draw"),
	               wIconCreatePixMap(split_draw_xpm3[iconSize]), LEVEL0_50,
	               IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_SPLITDRAW, NULL);
	AddMenuButton( menu, CmdTrimDraw, "cmdTrimDraw", _("Trim Draw"),
	               wIconCreatePixMap(trim_xpm3[iconSize]), LEVEL0_50,
	               IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_TRIMDRAW, NULL);
	ButtonGroupEnd();
}

