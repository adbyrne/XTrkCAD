/** \file cselect.c
 * Handle selecting / unselecting track and basic operations on the selection
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
#include <string.h>

#include "draw.h"
#include "ccurve.h"
#include "tcornu.h"
#include "tbezier.h"
#include "track.h"
#define PRIVATE_EXTRADATA
#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "cjoin.h"
#include "draw.h"
#include "misc.h"
#include "trackx.h"


#include "bitmaps/bmendpt.xbm"
#include "bitmaps/bma0.xbm"
#include "bitmaps/bma45.xbm"
#include "bitmaps/bma90.xbm"
#include "bitmaps/bma135.xbm"

#define SETMOVEMODE "MOVEMODE"

#define defaultCursor wCursorCross

EXPORT wIndex_t selectCmdInx;
EXPORT wIndex_t moveCmdInx;
EXPORT wIndex_t rotateCmdInx;

#define MAXMOVEMODE (3)
static long moveMode = MAXMOVEMODE;
static BOOL_T enableMoveDraw = TRUE;
static BOOL_T move0B;
struct extraData { char junk[2000]; };

static wDrawBitMap_p endpt_bm;
static wDrawBitMap_p angle_bm[4];
track_p IsInsideABox(coOrd pos);

static track_p moveDescTrk;
static coOrd moveDescPos;

int incrementalDrawLimit = 0;
static int microCount = 0;

static dynArr_t tlist_da;

#define Tlist(N) DYNARR_N( track_p, tlist_da, N )
#define TlistAppend( T ) \
		{ DYNARR_APPEND( track_p, tlist_da, 10 );\
		  Tlist(tlist_da.cnt-1) = T; }

BOOL_T TListSearch(track_p T) {
	for (int i=0;i<tlist_da.cnt-1;i++) { \
		if (Tlist(i) == T) return TRUE;
	}
	return FALSE;
}

static wMenu_p selectPopup1M;
static wMenu_p selectPopup1CM;
static wMenu_p selectPopup2M;
static wMenu_p selectPopup2CM;
static wMenu_p selectPopup2RM;
static wMenu_p selectPopup2TM;
static wMenu_p selectPopup2TYM;
static wMenuPush_p menuPushModify;
static wMenuPush_p rotateAlignMI;
static wMenuPush_p descriptionMI;
static wMenuPush_p hideMI;
static wMenuPush_p bridgeMI;
static wMenuPush_p tiesMI;


static BOOL_T doingAlign = FALSE;
static enum { AREA, MOVE } mode;

static void SelectOneTrack(
		track_p trk,
		wBool_t selected );
static void DrawSelectedTracksD( drawCmd_p d, wDrawColor color );

static dynArr_t anchors_da;
#define anchors(N) DYNARR_N(trkSeg_t,anchors_da,N)

void CreateArrowAnchor(coOrd pos,ANGLE_T a,DIST_T len) {
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		int i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).width = 0;
		anchors(i).u.l.pos[0] = pos;
		Translate(&anchors(i).u.l.pos[1],pos,NormalizeAngle(a+135),len);
		anchors(i).color = wDrawColorBlue;
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).width = 0;
		anchors(i).u.l.pos[0] = pos;
		Translate(&anchors(i).u.l.pos[1],pos,NormalizeAngle(a-135),len);
		anchors(i).color = wDrawColorBlue;
		wSetCursor(mainD.d,wCursorNone);
}

void static CreateRotateAnchor(coOrd pos) {
	DIST_T d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = d/8;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 180.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d*2;
	anchors(i).color = wDrawColorAqua;
	coOrd head;					//Arrows
	for (int j=0;j<3;j++) {
		Translate(&head,pos,j*120,d*2);
		CreateArrowAnchor(head,NormalizeAngle((j*120)+90),d);
	}
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = d/8;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 180.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d/16;
	anchors(i).color = wDrawColorAqua;
	wSetCursor(mainD.d,wCursorNone);
}

void static CreateModifyAnchor(coOrd pos) {
	DIST_T d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_FILCRCL;
	anchors(i).width = 0;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 180.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d/2;
	anchors(i).color = wDrawColorPowderedBlue;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = 0;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 180.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d;
	anchors(i).color = wDrawColorPowderedBlue;
	wSetCursor(mainD.d,wCursorNone);

}

void CreateDescribeAnchor(coOrd pos) {
	DIST_T d = tempD.scale*0.15;
	for (int j=0;j<2;j++) {
		pos.x += j*d*3/4;
		pos.y += j*d/2;
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		int i = anchors_da.cnt-1;
		anchors(i).type = SEG_CRVLIN;
		anchors(i).width = d/4;
		anchors(i).u.c.center = pos;
		anchors(i).u.c.a0 = 270.0;
		anchors(i).u.c.a1 = 270.0;
		anchors(i).u.c.radius = d*3/4;
		anchors(i).color = wDrawColorPowderedBlue;
		DYNARR_APPEND(trkSeg_t,anchors_da,1);
		i = anchors_da.cnt-1;
		anchors(i).type = SEG_STRLIN;
		anchors(i).width = d/4;
		Translate(&anchors(i).u.l.pos[0],pos,180.0,d*3/4);
		Translate(&anchors(i).u.l.pos[1],pos,180.0,d*1.5);
		anchors(i).color = wDrawColorPowderedBlue;
	}
	wSetCursor(mainD.d,wCursorNone);
}

void CreateActivateAnchor(coOrd pos) {
	DIST_T d = tempD.scale*0.15;
	coOrd c = pos;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = 0;
	c.x -= d*3/4;
	anchors(i).u.c.center = c;
	anchors(i).u.c.a0 = 0.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d;
	anchors(i).color = wDrawColorPowderedBlue;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	i = anchors_da.cnt-1;
	c.x += d*1.5;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = 0;
	anchors(i).u.c.center = pos;
	anchors(i).u.c.a0 = 0.0;
	anchors(i).u.c.a1 = 360.0;
	anchors(i).u.c.radius = d;
	anchors(i).color = wDrawColorPowderedBlue;
	wSetCursor(mainD.d,wCursorNone);
}

void static CreateMoveAnchor(coOrd pos) {
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,0,TRUE,wDrawColorBlue);
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,90,TRUE,wDrawColorBlue);
	wSetCursor(mainD.d,wCursorNone);
}

void CreateEndAnchor(coOrd p, wBool_t lock) {
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



/*****************************************************************************
 *
 * SELECT TRACKS
 *
 */

EXPORT long selectedTrackCount = 0;	/**< number of currently selected track components */

static void SelectedTrackCountChange( void )
{
	static long oldCount = 0;
	if (selectedTrackCount != oldCount) {
		if (oldCount == 0) {
			/* going non-0 */
			EnableCommands();
		} else if (selectedTrackCount == 0) {
			/* going 0 */
			EnableCommands();
		}
		oldCount = selectedTrackCount;
	}
}


static void DrawTrackAndEndPts(
		track_p trk,
		wDrawColor color )
{
	EPINX_T ep, ep2;
	track_p trk2;

	DrawTrack( trk, &mainD, color );
	for (ep=0;ep<GetTrkEndPtCnt(trk);ep++) {
		if ((trk2=GetTrkEndTrk(trk,ep)) != NULL) {
			ASSERT( !IsTrackDeleted(trk) );
			ep2 = GetEndPtConnectedToMe( trk2, trk );
			DrawEndPt( &mainD, trk2, ep2,
						(color==wDrawColorBlack && GetTrkSelected(trk2))?
								selectedColor:color );
		}
	}
}



static void RedrawSelectedTracksBoundary()
{
/* Truth table: 4 cases for a track trk, connected to trk1
 *           SELREDRAW
 * trk, trk1:  F, F   - No changes, nothing to draw
 *             T, F   - trk changes but trk1 didn't, flip drawing of select boundary marker
 *             F, T   - trk didn't change but trk1 did, handle redrawing when we get to 2nd track
 *             T, T   - both changed, but we don't need to redraw anything
 *                      unfortunately we will do a redundant redraw when we get to the 2nd track
 */
//	if (importTrack != NULL)
//		return;
	track_p trk;
	TRK_ITERATE( trk ) {
		if ( GetTrkBits(trk) & TB_SELREDRAW ) {
			// This track has changed
			for ( EPINX_T ep = 0; ep < GetTrkEndPtCnt(trk); ep++ ) {
				track_p trk1 = GetTrkEndTrk( trk, ep );
				if ( trk1 == NULL )
					continue;

//				if ( GetTrkIndex( trk ) < GetTrkIndex( trk1 )
//					continue;
				if ( ( GetTrkBits(trk1) & TB_SELREDRAW ) == 0 ) {
					// Connected track hasn't changed
					wDrawColor color = selectedColor;
					if ( ( GetTrkBits(trk) & TB_SELECTED ) == ( GetTrkBits(trk1) & TB_SELECTED ) ) {
						// There was a select boundary here, but not now.
						// Undraw old X
						color = wDrawColorWhite;
					}
					DIST_T len;
					coOrd p = GetTrkEndPos( trk, ep );
					ANGLE_T a = GetTrkEndAngle( trk, ep );
					coOrd p0, p1, p2;
					len = GetTrkGauge(trk)*2.0;
					if (len < 0.10*mainD.scale)
						len = 0.10*mainD.scale;
					Translate( &p1, p, a+45, len );
					Translate( &p2, p, a+225, len );
					DrawLine( &mainD, p1, p2, 2, color );
					Translate( &p1, p, a-45, len );
					Translate( &p2, p, a-225, len );
					DrawLine( &mainD, p1, p2, 2, color );
					if ( color == wDrawColorWhite ) {
						// Fill in holes by undraw cross
						DIST_T len2 = sqrt( GetTrkGauge(trk)*GetTrkGauge(trk)/2.0 );
						DIST_T len3 = 0.1*mainD.scale;
						color = GetTrkColor( trk, &mainD );
						if ( mainD.scale < twoRailScale ) {
							Translate( &p0, p, a-225, len2 );
							Translate( &p1, p0, a, len3 );
							Translate( &p2, p0, a+180, len3 );
							DrawLine( &mainD, p1, p2, GetTrkWidth(trk), color );
							Translate( &p0, p, a+225, len2 );
							Translate( &p1, p0, a, len3 );
							Translate( &p2, p0, a+180, len3 );
							DrawLine( &mainD, p1, p2, GetTrkWidth(trk), color );
							color = GetTrkColor( trk1, &mainD );
							Translate( &p0, p, a-45, len2 );
							Translate( &p1, p0, a, len3 );
							Translate( &p2, p0, a+180, len3 );
							DrawLine( &mainD, p1, p2, GetTrkWidth(trk1), color );
							Translate( &p0, p, a+45, len2 );
							Translate( &p1, p0, a, len3 );
							Translate( &p2, p0, a+180, len3 );
							DrawLine( &mainD, p1, p2, GetTrkWidth(trk1), color );
						} else {
							Translate( &p1, p, a, len3 );
							Translate( &p2, p, a+180, len3 );
							DrawLine( &mainD, p1, p2, GetTrkWidth(trk), color );
						}
					}
				}
			}
			ClrTrkBits( trk, TB_SELREDRAW );
		}
	}
}


EXPORT void SetAllTrackSelect( BOOL_T select )
{
	track_p trk;
	BOOL_T doRedraw = FALSE;

	if (select || selectedTrackCount > incrementalDrawLimit) {
		doRedraw = TRUE;
	} else {
		wDrawDelayUpdate( mainD.d, TRUE );
	}
	selectedTrackCount = 0;
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if ((!select) || GetLayerVisible( GetTrkLayer( trk ))) {
			if (select)
				selectedTrackCount++;
			if ((GetTrkSelected(trk)!=0) != select) {
				if (select)
					SetTrkBits( trk, TB_SELECTED );
				else
					ClrTrkBits( trk, TB_SELECTED );
				if (!doRedraw)
					SetTrkBits( trk, TB_SELREDRAW );
				DrawTrackAndEndPts( trk, wDrawColorBlack );
			}
		}
	}
	SelectedTrackCountChange();
	if (doRedraw) {
		MainRedraw(); // SetAllTrackSelect
	} else {
		RedrawSelectedTracksBoundary();
		wDrawDelayUpdate( mainD.d, FALSE );
	}
}

/* Invert selected state of all visible non-module objects.
 *
 * \param none
 * \return none
 */
 
EXPORT void InvertTrackSelect( void *ptr )
{
	track_p trk;

	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if (GetLayerVisible( GetTrkLayer( trk )) &&
			!GetLayerModule(GetTrkLayer( trk ))) {
			SelectOneTrack( trk, GetTrkSelected(trk)==0 );
		}
	}
	
	RedrawSelectedTracksBoundary();
	SelectedTrackCountChange();
	MainRedraw(); // InvertTrackSelect
}

/* Select orphaned (ie single) track pieces.
 *
 * \param none
 * \return none
 */
 
EXPORT void OrphanedTrackSelect( void *ptr )
{
	track_p trk;
	EPINX_T ep;
	int cnt ;
		
	trk = NULL;
	
	while( TrackIterate( &trk ) ) {
		cnt = 0;
		if( GetLayerVisible( GetTrkLayer( trk ) && !GetLayerModule(GetTrkLayer(trk)))) {
			for( ep = 0; ep < GetTrkEndPtCnt( trk ); ep++ ) {
				if( GetTrkEndTrk( trk, ep ) )
					cnt++;				
			}
			
			if( !cnt && GetTrkEndPtCnt( trk )) {
				SetTrkBits( trk, TB_SELECTED );
				DrawTrackAndEndPts( trk, wDrawColorBlack );
				selectedTrackCount++;			
			}		
		}
	}
	RedrawSelectedTracksBoundary();
	SelectedTrackCountChange();
	MainRedraw(); // OrphanTrackSelect
}

static void SelectOneTrack(
		track_p trk,
		wBool_t selected )
{
		BOOL_T bRedraw = (GetTrkSelected(trk) != 0) != selected;
		if ( !bRedraw ) {
			ClrTrkBits( trk, TB_SELREDRAW );
			return;
		}
		SetTrkBits( trk, TB_SELREDRAW );
		if (selected) {
			SetTrkBits( trk, TB_SELECTED );
			selectedTrackCount++;
		} else {
			ClrTrkBits( trk, TB_SELECTED );
			selectedTrackCount--;
		}
		SelectedTrackCountChange();
		DrawTrackAndEndPts( trk, wDrawColorBlack );
}


static void HighlightSelectedTracks(
		track_p trk_ignore, BOOL_T box, BOOL_T invert )
{
	track_p trk = NULL;
	if ( selectedTrackCount == 0 )
		return;
	while ( TrackIterate( &trk ) ) {
		if (trk == trk_ignore) continue;
	    if(GetTrkSelected(trk)) {
	    	if (!GetLayerVisible( GetTrkLayer( trk ))) continue;
	    	if (invert)
	    		DrawTrack(trk,&tempD,wDrawColorPreviewUnselected);
	    	else
	    		DrawTrack(trk,&tempD,wDrawColorPreviewSelected );
	    }
	}

}

static void SelectConnectedTracks(
		track_p trk, BOOL_T display_only )
{
	track_p trk1;
	int inx;
	EPINX_T ep;
	tlist_da.cnt = 0;
	TlistAppend( trk );
	InfoCount( 0 );
	if (!display_only) wDrawDelayUpdate( mainD.d, FALSE );
	for (inx=0; inx<tlist_da.cnt; inx++) {
		if ( inx > 0 && (selectedTrackCount == 0) && !display_only )
			return;
		trk = Tlist(inx);
		if (inx!=0 && 
			GetTrkSelected(trk)) {
			if (display_only)
				DrawTrack(trk,&tempD,wDrawColorPreviewSelected );
			continue;
		} else if (GetTrkSelected(trk)) {
			if (display_only)
				DrawTrack(trk,&tempD,wDrawColorPreviewUnselected);
			continue;
		}
		for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
			trk1 = GetTrkEndTrk( trk, ep );
			if (trk1 && !TListSearch(trk1) && GetLayerVisible( GetTrkLayer( trk1 ))) {
				if (GetTrkSelected(trk1)) {
					if (display_only) DrawTrack(trk1,&tempD,wDrawColorPreviewSelected );
				} else TlistAppend( trk1 );
			}
		}
		if (display_only) DrawTrack(trk,&tempD,wDrawColorPreviewSelected );
		else if (!GetTrkSelected(trk)) {
			if (GetLayerModule(GetTrkLayer(trk))) {
				continue;
			} else {
				SelectOneTrack( trk, TRUE );
				InfoCount( inx+1 );
			}
		}
	}
	if (!display_only) {
		RedrawSelectedTracksBoundary();
		wDrawDelayUpdate( mainD.d, TRUE );
		wFlush();
		InfoCount( trackCount );
	}
}

typedef void (*doModuleTrackCallBack_t)(track_p, BOOL_T);
static int DoModuleTracks( int moduleLayer, doModuleTrackCallBack_t doit, BOOL_T val)
{
	track_p trk;
	trk = NULL;
	int cnt = 0;
	while ( TrackIterate( &trk ) ) {
		if (GetTrkLayer(trk) == moduleLayer) {
			 doit( trk, val );
			 cnt++;
		}
	}
	return cnt;
}

static void DrawSingleTrack(track_p trk, BOOL_T bit) {
	DrawTrack(trk,&tempD,bit?wDrawColorPreviewSelected:wDrawColorPreviewUnselected);
}

typedef BOOL_T (*doSelectedTrackCallBack_t)(track_p, BOOL_T);
static void DoSelectedTracks( doSelectedTrackCallBack_t doit )
{
	track_p trk;
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if (GetTrkSelected(trk)) {
			if ( !doit( trk, TRUE ) ) {
				break;
			}
		}
	}
}


static BOOL_T SelectedTracksAreFrozen( void )
{
	track_p trk;
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if ( GetTrkSelected(trk) ) {
			if ( GetLayerFrozen( GetTrkLayer( trk ) ) ) {
				ErrorMessage( MSG_SEL_TRK_FROZEN );
				return TRUE;
			}
		}
	}
	return FALSE;
}


EXPORT void SelectTrackWidth( void* width )
{
	track_p trk;
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount<=0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	UndoStart( _("Change Track Width"), "trackwidth" );
	trk = NULL;
	wDrawDelayUpdate( mainD.d, TRUE );
	while ( TrackIterate( &trk ) ) {
		if (GetTrkSelected(trk)) {
			DrawTrackAndEndPts( trk, wDrawColorWhite );
			UndoModify( trk );
			SetTrkWidth( trk, (int)(long)width );
			DrawTrackAndEndPts( trk, wDrawColorBlack );
		}
	}
	wDrawDelayUpdate( mainD.d, FALSE );
	UndoEnd();
}

EXPORT void SelectLineType( void* width )
{
	track_p trk;
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount<=0) {
		ErrorMessage( MSG_NO_SELECTED_TRK );
		return;
	}
	UndoStart( _("Change Line Type"), "linetype" );
	trk = NULL;
	wDrawDelayUpdate( mainD.d, TRUE );
	while ( TrackIterate( &trk ) ) {
		if (GetTrkSelected(trk)) {
			UndoModify( trk );
			if (QueryTrack(trk, Q_CAN_MODIFY_CONTROL_POINTS))
				SetBezierLineType(trk, (int) (long) width);
			else if (QueryTrack(trk, Q_IS_DRAW))
				SetLineType( trk, (int)(long)width );
			else if (QueryTrack(trk, Q_IS_STRUCTURE)) {
				SetCompoundLineType(trk, (int)(long)width);
			}
		}
	}
	wDrawDelayUpdate( mainD.d, FALSE );
	UndoEnd();
}

static BOOL_T doingDouble;

EXPORT void SelectDelete( void )
{
	if (GetCurrentCommand() != selectCmdInx) return;
	if (doingDouble) return;

	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		UndoStart( _("Delete Tracks"), "delete" );
		wDrawDelayUpdate( mainD.d, TRUE );
		wDrawDelayUpdate( mapD.d, TRUE );
		DoSelectedTracks( DeleteTrack );
		DoRedraw(); // SelectDelete
		wDrawDelayUpdate( mainD.d, FALSE );
		wDrawDelayUpdate( mapD.d, FALSE );
		selectedTrackCount = 0;
		SelectedTrackCountChange();
		UndoEnd();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
}


BOOL_T flipHiddenDoSelectRecount;
static BOOL_T FlipHidden( track_p trk, BOOL_T junk )
{
	EPINX_T i;
	track_p trk2;

	DrawTrackAndEndPts( trk, wDrawColorWhite );
	/*UndrawNewTrack( trk );
	for (i=0; i<GetTrkEndPtCnt(trk); i++)
		if ((trk2=GetTrkEndTrk(trk,i)) != NULL) {
			UndrawNewTrack( trk2 );
		}*/
	UndoModify( trk );
	if ( drawTunnel == 0 )
		flipHiddenDoSelectRecount = TRUE;
	if (GetTrkVisible(trk)) {
		ClrTrkBits( trk, TB_VISIBLE|(drawTunnel==0?(TB_SELECTED|TB_SELREDRAW):0) );
		ClrTrkBits (trk, TB_BRIDGE);
		ClrTrkBits (trk, TB_NOTIES);
;	} else {
		SetTrkBits( trk, TB_VISIBLE );
	}
	/*DrawNewTrack( trk );*/
	DrawTrackAndEndPts( trk, wDrawColorBlack );
	for (i=0; i<GetTrkEndPtCnt(trk); i++)
		if ((trk2=GetTrkEndTrk(trk,i)) != NULL) {
			UndoModify( trk2 );
			/*DrawNewTrack( trk2 );*/
		}
	return TRUE;
}

static BOOL_T FlipBridge( track_p trk, BOOL_T junk )
{
	UndoModify( trk );
	if (GetTrkBridge(trk)) {
		ClrTrkBits( trk, TB_BRIDGE );
	} else {
		SetTrkBits( trk, TB_BRIDGE );
		SetTrkBits( trk, TB_VISIBLE);
	}
	return TRUE;
}

static BOOL_T FlipTies( track_p trk, BOOL_T junk )
{
	UndoModify( trk );
	if (GetTrkNoTies(trk)) {
		ClrTrkBits( trk, TB_NOTIES );
	} else {
		SetTrkBits( trk, TB_NOTIES );
		SetTrkBits( trk, TB_VISIBLE );
	}
	return TRUE;
}

EXPORT void SelectTunnel( void )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		flipHiddenDoSelectRecount = FALSE;
		UndoStart( _("Hide Tracks (Tunnel)"), "tunnel" );
		wDrawDelayUpdate( mainD.d, TRUE );
		DoSelectedTracks( FlipHidden );
		wDrawDelayUpdate( mainD.d, FALSE );
		UndoEnd();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
	if ( flipHiddenDoSelectRecount )
		SelectRecount();
}

EXPORT void SelectBridge( void )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		flipHiddenDoSelectRecount = FALSE;
		UndoStart( _("Bridge Tracks "), "bridge" );
		wDrawDelayUpdate( mainD.d, TRUE );
		DoSelectedTracks( FlipBridge );
		wDrawDelayUpdate( mainD.d, FALSE );
		UndoEnd();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
	MainRedraw(); // SelectBridge
}

EXPORT void SelectTies( void )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		flipHiddenDoSelectRecount = FALSE;
		UndoStart( _("Ties Tracks "), "noties" );
		wDrawDelayUpdate( mainD.d, TRUE );
		DoSelectedTracks( FlipTies );
		wDrawDelayUpdate( mainD.d, FALSE );
		UndoEnd();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
	MainRedraw(); // SelectTies
}

void SelectRecount( void )
{
	track_p trk;
	selectedTrackCount = 0;
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if (GetTrkSelected(trk)) {
			selectedTrackCount++;
		}
	}
	SelectedTrackCountChange();
}


static BOOL_T SetLayer( track_p trk, BOOL_T junk )
{
	UndoModify( trk );
	SetTrkLayer( trk, curLayer );
	return TRUE;
}

EXPORT void MoveSelectedTracksToCurrentLayer( void )
{
	if (SelectedTracksAreFrozen())
		return;
		if (selectedTrackCount>0) {
			UndoStart( _("Move To Current Layer"), "changeLayer" );
			DoSelectedTracks( SetLayer );
			UndoEnd();
		} else {
			ErrorMessage( MSG_NO_SELECTED_TRK );
		}
}

EXPORT void SelectCurrentLayer( void )
{
	track_p trk;
	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if ((!GetTrkSelected(trk)) && GetTrkLayer(trk) == curLayer ) {
			SelectOneTrack( trk, TRUE );
		}
	}
	RedrawSelectedTracksBoundary();
}


static BOOL_T ClearElevation( track_p trk, BOOL_T junk )
{
	EPINX_T ep;
	for ( ep=0; ep<GetTrkEndPtCnt(trk); ep++ ) {
		if (!EndPtIsIgnoredElev(trk,ep)) {
			DrawEndPt2( &mainD, trk, ep, wDrawColorWhite );
			SetTrkEndElev( trk, ep, ELEV_NONE, 0.0, NULL );
			ClrTrkElev( trk );
			DrawEndPt2( &mainD, trk, ep, wDrawColorBlack );
		}
	}
	return TRUE;
}

EXPORT void ClearElevations( void )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		UndoStart( _("Clear Elevations"), "clear elevations" );
		DoSelectedTracks( ClearElevation );
		UpdateAllElevations();
		UndoEnd();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
}


static DIST_T elevDelta;
static BOOL_T AddElevation( track_p trk, BOOL_T junk )
{
	track_p trk1;
	EPINX_T ep, ep1;
	int mode;
	DIST_T elev;

	for ( ep=0; ep<GetTrkEndPtCnt(trk); ep++ ) {
		if ((trk1=GetTrkEndTrk(trk,ep))) {
			ep1 = GetEndPtConnectedToMe( trk1, trk );
			if (ep1 >= 0) {
				if (GetTrkSelected(trk1) && GetTrkIndex(trk1)<GetTrkIndex(trk))
					continue;
			}
		}
		if (EndPtIsDefinedElev(trk,ep)) {
			DrawEndPt2( &mainD, trk, ep, wDrawColorWhite );
			mode = GetTrkEndElevUnmaskedMode(trk,ep);
			elev = GetTrkEndElevHeight(trk,ep);
			SetTrkEndElev( trk, ep, mode, elev+elevDelta, NULL );
			ClrTrkElev( trk );
			DrawEndPt2( &mainD, trk, ep, wDrawColorBlack );
		}
	}
	return TRUE;
}

EXPORT void AddElevations( DIST_T delta )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		elevDelta = delta;
		UndoStart( _("Add Elevations"), "add elevations" );
		DoSelectedTracks( AddElevation );
		UndoEnd();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
	UpdateAllElevations();
}


EXPORT void DoRefreshCompound( void )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		UndoStart( _("Refresh Compound"), "refresh compound" );
		DoSelectedTracks( RefreshCompound );
		RefreshCompound( NULL, FALSE );
		UndoEnd();
		MainRedraw(); // DoRefreshCompound
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
}


static drawCmd_t tempSegsD = {
		NULL, &tempSegDrawFuncs, 0, 1, 0.0, {0.0, 0.0}, {0.0, 0.0}, Pix2CoOrd, CoOrd2Pix };
EXPORT void WriteSelectedTracksToTempSegs( void )
{
	track_p trk;
	DYNARR_RESET( trkSeg_t, tempSegs_da );
	tempSegsD.dpi = mainD.dpi;
	for ( trk=NULL; TrackIterate(&trk); ) {
		if ( GetTrkSelected( trk ) ) {
			if ( IsTrack( trk ) )
				continue;
			ClrTrkBits( trk, TB_SELECTED );
			DrawTrack( trk, &tempSegsD, wDrawColorBlack );
			SetTrkBits( trk, TB_SELECTED );
		}
	}
}

static char rescaleFromScale[20];
static char rescaleFromGauge[20];

static char * rescaleToggleLabels[] = { N_("Scale"), N_("Ratio"), NULL };
static long rescaleMode;
static wIndex_t rescaleFromScaleInx;
static wIndex_t rescaleFromGaugeInx;
static wIndex_t rescaleToScaleInx;
static wIndex_t rescaleToGaugeInx;
static wIndex_t rescaleToInx;
static long rescaleNoChangeDim = FALSE;
static FLOAT_T rescalePercent;
static char * rescaleChangeDimLabels[] = { N_("Do not resize track"), NULL };
static paramFloatRange_t r0o001_10000 = { 0.001, 10000.0 };
static paramData_t rescalePLs[] = {
#define I_RESCALE_MODE		(0)
		{ PD_RADIO, &rescaleMode, "toggle", PDO_NOPREF, &rescaleToggleLabels, N_("Rescale by:"), BC_HORZ|BC_NOBORDER },
#define I_RESCALE_FROM_SCALE		(1)
		{ PD_STRING, rescaleFromScale, "fromS", PDO_NOPREF|PDO_STRINGLIMITLENGTH, (void*)100, N_("From:"),0, 0, sizeof(rescaleFromScale)},
#define I_RESCALE_FROM_GAUGE		(2)
		{ PD_STRING, rescaleFromGauge, "fromG", PDO_NOPREF|PDO_DLGHORZ | PDO_STRINGLIMITLENGTH, (void*)100, " / ", 0, 0, sizeof(rescaleFromGauge)},
#define I_RESCALE_TO_SCALE		   (3)
		{ PD_DROPLIST, &rescaleToScaleInx, "toS", PDO_NOPREF|PDO_LISTINDEX, (void *)100, N_("To: ") },
#define I_RESCALE_TO_GAUGE		   (4)
		{ PD_DROPLIST, &rescaleToGaugeInx, "toG", PDO_NOPREF|PDO_LISTINDEX|PDO_DLGHORZ, NULL, " / " },
#define I_RESCALE_CHANGE	(5)
		{ PD_TOGGLE, &rescaleNoChangeDim, "change-dim", 0, &rescaleChangeDimLabels, "", BC_HORZ|BC_NOBORDER },
#define I_RESCALE_PERCENT	(6)
		{ PD_FLOAT, &rescalePercent, "ratio", 0, &r0o001_10000, N_("Ratio") },
		{ PD_MESSAGE, "%", NULL, PDO_DLGHORZ } };
static paramGroup_t rescalePG = { "rescale", 0, rescalePLs, sizeof rescalePLs/sizeof rescalePLs[0] };


static long getboundsCount;
static coOrd getboundsLo, getboundsHi;

static BOOL_T GetboundsDoIt( track_p trk, BOOL_T junk )
{
	coOrd hi, lo;

	GetBoundingBox( trk, &hi, &lo );
	if ( getboundsCount == 0 ) {
		getboundsLo = lo;
		getboundsHi = hi;
	} else {
		if ( lo.x < getboundsLo.x ) getboundsLo.x = lo.x;
		if ( lo.y < getboundsLo.y ) getboundsLo.y = lo.y;
		if ( hi.x > getboundsHi.x ) getboundsHi.x = hi.x;
		if ( hi.y > getboundsHi.y ) getboundsHi.y = hi.y;
	}
	getboundsCount++;
	return TRUE;
}

static coOrd rescaleShift;
static BOOL_T RescaleDoIt( track_p trk, BOOL_T junk )
{
	EPINX_T ep, ep1;
	track_p trk1;
	UndrawNewTrack( trk );
	UndoModify(trk);
	if ( rescalePercent != 100.0 ) {
		for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
			if ((trk1 = GetTrkEndTrk(trk,ep)) != NULL &&
				!GetTrkSelected(trk1)) {
				ep1 = GetEndPtConnectedToMe( trk1, trk );
				DisconnectTracks( trk, ep, trk1, ep1 );
			}
		}
		/* should the track dimensions ie. length or radius be changed as well? */
		if( rescaleNoChangeDim == 0 )
			RescaleTrack( trk, rescalePercent/100.0, rescaleShift );
	}
	
	if ( rescaleMode==0 )
		SetTrkScale( trk, rescaleToInx );
	getboundsCount++;
	DrawNewTrack( trk );
	return TRUE;
}


static void RescaleDlgOk(
		void * junk )
{
	coOrd center, size;
	DIST_T d;
	FLOAT_T ratio = rescalePercent/100.0;

	UndoStart( _("Rescale Tracks"), "Rescale" );
	getboundsCount = 0;
	DoSelectedTracks( GetboundsDoIt );
	center.x = (getboundsLo.x+getboundsHi.x)/2.0;
	center.y = (getboundsLo.y+getboundsHi.y)/2.0;
	size.x = (getboundsHi.x-getboundsLo.x)/2.0*ratio;
	size.y = (getboundsHi.y-getboundsLo.y)/2.0*ratio;
	getboundsLo.x = center.x - size.x;
	getboundsLo.y = center.y - size.y;
	getboundsHi.x = center.x + size.x;
	getboundsHi.y = center.y + size.y;
	if ( getboundsLo.x < 0 ) {
		getboundsHi.x -= getboundsLo.x;
		getboundsLo.x = 0;
	} else if ( getboundsHi.x > mapD.size.x ) {
		d = getboundsHi.x - mapD.size.x;
		if ( getboundsLo.x < d )
			d = getboundsLo.x;
		getboundsHi.x -= d;
		getboundsLo.x -= d;
	}
	if ( getboundsLo.y < 0 ) {
		getboundsHi.y -= getboundsLo.y;
		getboundsLo.y = 0;
	} else if ( getboundsHi.y > mapD.size.y ) {
		d = getboundsHi.y - mapD.size.y;
		if ( getboundsLo.y < d )
			d = getboundsLo.y;
		getboundsHi.y -= d;
		getboundsLo.y -= d;
	}
	if ( rescaleNoChangeDim == 0 && 
	     (getboundsHi.x > mapD.size.x ||
		   getboundsHi.y > mapD.size.y )) {
		NoticeMessage( MSG_RESCALE_TOO_BIG, _("Ok"), NULL, FormatDistance(getboundsHi.x), FormatDistance(getboundsHi.y) );
	}
	rescaleShift.x = (getboundsLo.x+getboundsHi.x)/2.0 - center.x*ratio;
	rescaleShift.y = (getboundsLo.y+getboundsHi.y)/2.0 - center.y*ratio;
	
	rescaleToInx = GetScaleInx( rescaleToScaleInx, rescaleToGaugeInx );
	DoSelectedTracks( RescaleDoIt );

	// rescale the background if it exists and the layout is resized
	if (HasBackGround() && ratio != 1.0) {
		coOrd pos = GetLayoutBackGroundPos();
		double size = GetLayoutBackGroundSize();
		pos.x = ratio * pos.x + rescaleShift.x;
		pos.y = ratio * pos.y + rescaleShift.y;
		SetLayoutBackGroundPos(pos);

		size *= ratio;
		SetLayoutBackGroundSize(size);
	}
	DoRedraw();
	wHide( rescalePG.win );
}


static void RescaleDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	switch (inx) {
	case I_RESCALE_MODE:
		wControlShow( pg->paramPtr[I_RESCALE_FROM_SCALE].control, rescaleMode==0 );
		wControlActive( pg->paramPtr[I_RESCALE_FROM_SCALE].control, FALSE ); 
		wControlShow( pg->paramPtr[I_RESCALE_TO_SCALE].control, rescaleMode==0 );
		wControlShow( pg->paramPtr[I_RESCALE_FROM_GAUGE].control, rescaleMode==0 );
		wControlActive( pg->paramPtr[I_RESCALE_FROM_GAUGE].control, FALSE ); 
		wControlShow( pg->paramPtr[I_RESCALE_TO_GAUGE].control, rescaleMode==0 );
		wControlShow( pg->paramPtr[I_RESCALE_CHANGE].control, rescaleMode==0 );
		wControlActive( pg->paramPtr[I_RESCALE_PERCENT].control, rescaleMode==1 );
		if ( rescaleMode!=0 )
			break;
	case I_RESCALE_TO_SCALE:
		LoadGaugeList( (wList_p)rescalePLs[I_RESCALE_TO_GAUGE].control, *((int *)valueP) );
		rescaleToGaugeInx = 0;
		ParamLoadControl( pg, I_RESCALE_TO_GAUGE );
		ParamLoadControl( pg, I_RESCALE_TO_SCALE );		
		rescalePercent = GetScaleDescRatio(rescaleFromScaleInx)/GetScaleDescRatio(rescaleToScaleInx)*100.0;
		wControlActive( pg->paramPtr[I_RESCALE_CHANGE].control, (rescaleFromScaleInx != rescaleToScaleInx) );
		ParamLoadControl( pg, I_RESCALE_PERCENT );
		break;
	case I_RESCALE_TO_GAUGE:
		ParamLoadControl( pg, I_RESCALE_TO_GAUGE );
		break;
	case I_RESCALE_FROM_SCALE:
		ParamLoadControl( pg, I_RESCALE_FROM_SCALE );
		break;
	case I_RESCALE_FROM_GAUGE:	
		ParamLoadControl( pg, I_RESCALE_FROM_GAUGE );
		break;
	case I_RESCALE_CHANGE:
		ParamLoadControl( pg, I_RESCALE_CHANGE );
		break;
	case -1:
		break;
	}
	ParamDialogOkActive( pg, rescalePercent!=100.0 || rescaleFromGaugeInx != rescaleToGaugeInx );
}

/**
 * Get the scale gauge information for the selected track pieces.  
 * FIXME: special cases like tracks pieces with different gauges or scale need to be handled
 *
 * \param IN trk track element
 * \param IN junk
 * \return TRUE;
 */
 
static BOOL_T SelectedScaleGauge( track_p trk, BOOL_T junk )
{
	char *scaleName;
	SCALEINX_T scale;
	SCALEDESCINX_T scaleInx;
	GAUGEINX_T gaugeInx;
	
	scale = GetTrkScale( trk );
	scaleName = GetScaleName( scale );
	if( strcmp( scaleName, "*" )) {
		GetScaleGauge( scale, &scaleInx, &gaugeInx );
		strcpy( rescaleFromScale,GetScaleDesc( scaleInx ));
		strcpy( rescaleFromGauge, GetGaugeDesc( scaleInx, gaugeInx ));
		
		rescaleFromScaleInx = scaleInx;
		rescaleFromGaugeInx = gaugeInx;
		rescaleToScaleInx = scaleInx;
		rescaleToGaugeInx = gaugeInx;
	}	
	
	return TRUE;
}

/**
 * Bring up the rescale dialog. The dialog for rescaling the selected pieces
 * of track is created if necessary and shown. Handling of user input is done via
 * RescaleDlgUpdate()
 */

EXPORT void DoRescale( void )
{
	if ( rescalePG.win == NULL ) {
		ParamCreateDialog( &rescalePG, MakeWindowTitle(_("Rescale")), _("Ok"), RescaleDlgOk, wHide, TRUE, NULL, F_BLOCK, RescaleDlgUpdate );
		LoadScaleList( (wList_p)rescalePLs[I_RESCALE_TO_SCALE].control );
		LoadGaugeList( (wList_p)rescalePLs[I_RESCALE_TO_GAUGE].control, GetLayoutCurScaleDesc() ); /* set correct gauge list here */
		rescaleFromScaleInx = GetLayoutCurScale();
		rescaleToScaleInx = rescaleFromScaleInx;
		rescalePercent = 100.0;
	}

	DoSelectedTracks( SelectedScaleGauge );

	RescaleDlgUpdate( &rescalePG, I_RESCALE_MODE, &rescaleMode );
	RescaleDlgUpdate( &rescalePG, I_RESCALE_CHANGE, &rescaleMode );

	RescaleDlgUpdate( &rescalePG, I_RESCALE_FROM_GAUGE, rescaleFromGauge );
	RescaleDlgUpdate( &rescalePG, I_RESCALE_FROM_SCALE, rescaleFromScale );

	RescaleDlgUpdate( &rescalePG, I_RESCALE_TO_SCALE, &rescaleToScaleInx );
	RescaleDlgUpdate( &rescalePG, I_RESCALE_TO_GAUGE, &rescaleToGaugeInx );
	
	wShow( rescalePG.win );
}


static void DrawSelectedTracksD( drawCmd_p d, wDrawColor color )
{
	wIndex_t inx;
	track_p trk;
	coOrd lo, hi;
	/*wDrawDelayUpdate( d->d, TRUE );*/
	for (inx=0; inx<tlist_da.cnt; inx++) {
		trk = Tlist(inx);
		if (d != &mapD) {
			GetBoundingBox( trk, &hi, &lo );
			if ( OFF_D( d->orig, d->size, lo, hi ) )
				continue;
		}
		if (color != wDrawColorWhite)
			ClrTrkBits(trk, TB_UNDRAWN);
		DrawTrack( trk, d, color );
		if (color == wDrawColorWhite)
			SetTrkBits( trk, TB_UNDRAWN );
	}
	/*wDrawDelayUpdate( d->d, FALSE );*/
}

static BOOL_T AddSelectedTrack(
		track_p trk, BOOL_T junk )
{
	DYNARR_APPEND( track_p, tlist_da, 10 );
	DYNARR_LAST( track_p, tlist_da ) = trk;
	return TRUE;
}

static BOOL_T RemoveSelectedTrack(track_p trk) {

	for(int i=0;i<tlist_da.cnt; i++) {
		if (DYNARR_N(track_p,tlist_da,i) == trk) {
			for (int j=i;j<tlist_da.cnt-1;j++) {
				DYNARR_N(track_p,tlist_da,j) = DYNARR_N(track_p,tlist_da,j+1);
			}
			tlist_da.cnt--;
			return TRUE;
		}
	}
	return FALSE;
}

static coOrd moveOrig;
static ANGLE_T moveAngle;

static coOrd moveD_hi, moveD_lo;

static drawCmd_t moveD = {
		NULL, &tempSegDrawFuncs, DC_SIMPLE, 1, 0.0, {0.0, 0.0}, {0.0, 0.0}, Pix2CoOrd, CoOrd2Pix };




/* Draw selected (on-screen) tracks to tempSegs,
   and use drawSegs to draw them (moved/rotated) to mainD
   Incremently add new tracks as they scroll on-screen.
*/


static int movedCnt;
static void AccumulateTracks( void )
{
	wIndex_t inx;
	track_p trk;
	coOrd lo, hi;

	/*wDrawDelayUpdate( moveD.d, TRUE );*/
	movedCnt =0;
	for ( inx = 0; inx<tlist_da.cnt; inx++ ) {
		trk = Tlist(inx);
		if (trk) {
			GetBoundingBox( trk, &hi, &lo );
			if (lo.x <= moveD_hi.x && hi.x >= moveD_lo.x &&
				lo.y <= moveD_hi.y && hi.y >= moveD_lo.y ) {
					if (!QueryTrack(trk,Q_IS_CORNU))
						DrawTrack( trk, &moveD, wDrawColorBlack );
				}
				movedCnt++;
			}
	}
	InfoCount( movedCnt );
	/*wDrawDelayUpdate( moveD.d, FALSE );*/
}

static dynArr_t auto_select_da;

static void AddEndCornus() {
	for (int i=0;i<tlist_da.cnt;i++) {
		track_p trk = DYNARR_N(track_p,tlist_da,i);
		track_p tc;
		for (int j=GetTrkEndPtCnt(trk)-1;j>=0;j--) {
			tc = GetTrkEndTrk(trk,j);
			if (tc && !GetTrkSelected(tc) && QueryTrack(tc,Q_IS_CORNU) && !QueryTrack(trk,Q_IS_CORNU)) {  //On end and cornu
				SelectOneTrack( tc, TRUE );
				DYNARR_APPEND(track_p,tlist_da,1);	//Add to selected list
				DYNARR_LAST(track_p,tlist_da) = tc;
				DYNARR_APPEND(track_p,auto_select_da,1);
				DYNARR_LAST(track_p,auto_select_da) = tc;
			}
		}
	}
}

static void RemoveEndCornus() {
	track_p tc;
	for (int i=0;i<auto_select_da.cnt;i++) {
		tc = DYNARR_N(track_p,auto_select_da,i);
		SelectOneTrack( tc, FALSE );
		RemoveSelectedTrack(tc);
	}
	DYNARR_RESET(track_p,auto_select_da);
}


static void GetMovedTracks( BOOL_T undraw )
{
	wSetCursor( mainD.d, wCursorWait );
	DYNARR_RESET( track_p, tlist_da );
	DoSelectedTracks( AddSelectedTrack );
	AddEndCornus();							//Include Cornus that are attached at ends of selected
	DYNARR_RESET( trkSeg_p, tempSegs_da );
	moveOrig = mainD.orig;
	movedCnt = 0;
	InfoCount(0);
	wSetCursor( mainD.d, defaultCursor );
	moveD_hi = moveD_lo = mainD.orig;
	moveD_hi.x += mainD.size.x;
	moveD_hi.y += mainD.size.y;
	AccumulateTracks();
	if (undraw) {
		DrawSelectedTracksD( &mainD, wDrawColorWhite );
	}
}

static void SetMoveD( BOOL_T moveB, coOrd orig, ANGLE_T angle )
{
	int inx;

	moveOrig.x = orig.x;
	moveOrig.y = orig.y;
	moveAngle = angle;
	if (!moveB) {
		Rotate( &orig, zero, angle );
		moveOrig.x -= orig.x;
		moveOrig.y -= orig.y;
	}
	if (moveB) {
		moveD_lo.x = mainD.orig.x - orig.x;
		moveD_lo.y = mainD.orig.y - orig.y;
		moveD_hi = moveD_lo;
		moveD_hi.x += mainD.size.x;
		moveD_hi.y += mainD.size.y;
	} else {
		coOrd corner[3];
		corner[2].x = mainD.orig.x;
		corner[0].x = corner[1].x = mainD.orig.x + mainD.size.x;
		corner[0].y = mainD.orig.y;
		corner[1].y = corner[2].y = mainD.orig.y + mainD.size.y;
		moveD_hi = mainD.orig;
		Rotate( &moveD_hi, orig, -angle );
		moveD_lo = moveD_hi;
		for (inx=0;inx<3;inx++) {
			Rotate( &corner[inx], orig, -angle );
			if (corner[inx].x < moveD_lo.x)
				moveD_lo.x = corner[inx].x;
			if (corner[inx].y < moveD_lo.y)
				moveD_lo.y = corner[inx].y;
			if (corner[inx].x > moveD_hi.x)
				moveD_hi.x = corner[inx].x;
			if (corner[inx].y > moveD_hi.y)
				moveD_hi.y = corner[inx].y;
		}
	}
	AccumulateTracks();
}


static void DrawMovedTracks( void )
{
	int inx;
	track_p trk;
	dynArr_t cornu_segs;

	DrawSegs( &tempD, moveOrig, moveAngle, &tempSegs(0), tempSegs_da.cnt,
					0.0, wDrawColorBlack );

	for ( inx=0; inx<tlist_da.cnt; inx++ ) {
		trk = Tlist(inx);
		if (QueryTrack(trk,Q_IS_CORNU)) {
			DYNARR_RESET(trkSeg_t,cornu_segs);
			coOrd pos[2];
			DIST_T radius[2];
			ANGLE_T angle[2];
			coOrd center[2];
			trackParams_t trackParams;
			if (GetTrackParams(PARAMS_CORNU, trk, zero, &trackParams)) {
				for (int i=0;i<2;i++) {
					pos[i] = trackParams.cornuEnd[i];
					center[i] = trackParams.cornuCenter[i];
					angle[i] = trackParams.cornuAngle[i];
					radius[i] = trackParams.cornuRadius[i];
					if (!GetTrkEndTrk(trk,i) ||
						(GetTrkEndTrk(trk,i) && GetTrkSelected(GetTrkEndTrk(trk,i)))) {
						if (!move0B) {
							Rotate( &pos[i], zero, moveAngle );
							Rotate( &center[i],zero, moveAngle );
							angle[i] = NormalizeAngle(angle[i]+moveAngle);
						}
						pos[i].x += moveOrig.x;
						pos[i].y += moveOrig.y;
						center[i].x +=moveOrig.x;
						center[i].y +=moveOrig.y;
					}
				}
				CallCornu0(&pos[0],&center[0],&angle[0],&radius[0],&cornu_segs, FALSE);
				trkSeg_p cornu_p = &DYNARR_N(trkSeg_t,cornu_segs,0);

				DrawSegsO(&tempD, trk, zero, 0.0, cornu_p,cornu_segs.cnt,
						GetTrkGauge(trk), wDrawColorBlack, DTS_LEFT|DTS_RIGHT );
			}

		}

	}
	return;
}



static void MoveTracks(
		BOOL_T eraseFirst,
		BOOL_T move,
		BOOL_T rotate,
		coOrd base,
		coOrd orig,
		ANGLE_T angle,
		BOOL_T undo)
{
	track_p trk, trk1;
	EPINX_T ep, ep1;
	int inx;
	trackParams_t trackParms;
	ANGLE_T endAngle;
	DIST_T endRadius;
	coOrd endCenter;

	wSetCursor( mainD.d, wCursorWait );
	/*UndoStart( "Move/Rotate Tracks", "move/rotate" );*/
	if (tlist_da.cnt <= incrementalDrawLimit) {
		if (eraseFirst) {
			DrawSelectedTracksD( &mainD, wDrawColorWhite );
			DrawSelectedTracksD( &mapD, wDrawColorWhite );
		}
	}
	for ( inx=0; inx<tlist_da.cnt; inx++ ) {
		trk = Tlist(inx);
		UndoModify( trk );
		BOOL_T fixed_end;
		fixed_end = FALSE;
		if (QueryTrack(trk, Q_IS_CORNU)) {
			for (int i=0;i<2;i++) {
				track_p te;
				if ((te = GetTrkEndTrk(trk,i)) && !GetTrkSelected(te)) {
					fixed_end = TRUE;
				}
			}
		}

	    if (!fixed_end) {
			if (move)
				MoveTrack( trk, base );
			if (rotate)
				RotateTrack( trk, orig, angle );
			for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
				if ((trk1 = GetTrkEndTrk(trk,ep)) != NULL &&
						!GetTrkSelected(trk1)) {
					ep1 = GetEndPtConnectedToMe( trk1, trk );
					DisconnectTracks( trk, ep, trk1, ep1 );
					DrawEndPt( &mainD, trk1, ep1, wDrawColorBlack );
				}
			}
	    } else {
			if (QueryTrack(trk, Q_IS_CORNU)) {			//Cornu will be at the end of selected set
				for (int i=0;i<2;i++) {
					if ((trk1 = GetTrkEndTrk(trk,i)) && GetTrkSelected(trk1)) {
						ep1 = GetEndPtConnectedToMe( trk1, trk );
						DisconnectTracks(trk,i,trk1,ep1);
						GetTrackParams(PARAMS_CORNU,trk1,GetTrkEndPos(trk1,ep1),&trackParms);
						if (trackParms.type == curveTypeStraight) {
							endRadius = 0;
							endCenter = zero;
						} else {
							endRadius = trackParms.arcR;
							endCenter = trackParms.arcP;
						}
						DrawTrack(trk,&mainD,wDrawColorWhite);
						DrawTrack(trk,&mapD,wDrawColorWhite);
						endAngle = NormalizeAngle(GetTrkEndAngle(trk1,ep1)+180);
						if (SetCornuEndPt(trk,i,GetTrkEndPos(trk1,ep1),endCenter,endAngle,endRadius)) {
							ConnectTracks(trk,i,trk1,ep1);
							DrawTrack(trk,&mainD,wDrawColorBlack);
							DrawTrack(trk,&mapD,wDrawColorBlack);
						} else {
							DeleteTrack(trk,TRUE);
							ErrorMessage(_("Cornu too tight - it was deleted"));
							DoRedraw(); // MoveTracks: Cornu/delete
							continue;
						}
					} else if (!trk1) {									//No end track
						DrawTrack(trk,&mainD,wDrawColorWhite);
						DrawTrack(trk,&mapD,wDrawColorWhite);
						GetTrackParams(PARAMS_CORNU,trk,GetTrkEndPos(trk,i),&trackParms);
						if (move) {
							coOrd end_pos, end_center;
							end_pos = trackParms.cornuEnd[i];
							end_pos.x += base.x;
							end_pos.y += base.y;
							end_center = trackParms.cornuCenter[i];
							end_center.x += base.x;
							end_center.y += base.y;
							SetCornuEndPt(trk,i,end_pos,end_center,trackParms.cornuAngle[i],trackParms.cornuRadius[i]);
						}
						if (rotate) {
							coOrd end_pos, end_center;
							ANGLE_T end_angle;
							end_pos = trackParms.cornuEnd[i];
							end_center = trackParms.cornuCenter[i];
							Rotate(&end_pos, orig, angle);
							Rotate(&end_center, orig, angle);
							end_angle = NormalizeAngle( trackParms.cornuAngle[i] + angle );
							SetCornuEndPt(trk,i,end_pos,end_center,end_angle,trackParms.cornuRadius[i]);
						}
						DrawTrack(trk,&mainD,wDrawColorBlack);
						DrawTrack(trk,&mapD,wDrawColorBlack);
					}
				}
			}
	    }

		InfoCount( inx );
	}
	RemoveEndCornus();
	ClrAllTrkBits(TB_UNDRAWN);
	DoRedraw();
	wSetCursor( mainD.d, defaultCursor );
	if (undo) UndoEnd();
	InfoCount( trackCount );
}


void MoveToJoin(
		track_p trk0,
		EPINX_T ep0,
		track_p trk1,
		EPINX_T ep1 )
{
	coOrd orig;
	coOrd base;
	ANGLE_T angle;

	UndoStart( _("Move To Join"), "Move To Join" );
		base = GetTrkEndPos(trk0,ep0);
		orig = GetTrkEndPos(trk1, ep1 );
		base.x = orig.x - base.x;
		base.y = orig.y - base.y;
		angle = GetTrkEndAngle(trk1,ep1);
		angle -= GetTrkEndAngle(trk0,ep0);
		angle += 180.0;
		angle = NormalizeAngle( angle );
		GetMovedTracks( FALSE );
		MoveTracks( TRUE, TRUE, TRUE, base, orig, angle, TRUE );
		UndrawNewTrack( trk0 );
		UndrawNewTrack( trk1 );
		ConnectTracks( trk0, ep0, trk1, ep1 );
		DrawNewTrack( trk0 );
		DrawNewTrack( trk1 );
		RemoveEndCornus();
}

void FreeTempStrings() {
	for (int i = 0; i<tempSegs_da.cnt; i++) {
		if (tempSegs(i).type == SEG_TEXT) {
			if (tempSegs(i).u.t.string)
				MyFree(tempSegs(i).u.t.string);
			tempSegs(i).u.t.string = NULL;
		}
	}
}


wBool_t FindEndIntersection(coOrd base, coOrd orig, ANGLE_T angle, track_p * t1, EPINX_T * ep1, track_p * t2, EPINX_T * ep2) {
	*ep1 = -1;
	*ep2 = -1;
	*t1 = NULL;
	*t2 = NULL;
	for ( int inx=0; inx<tlist_da.cnt; inx++ ) {   //All selected
		track_p ts = Tlist(inx);
		for (int i=0; i<GetTrkEndPtCnt(ts); i++) { //All EndPoints
			track_p ct;
			if ((ct = GetTrkEndTrk(ts,i))!=NULL) {
				if (GetTrkSelected(ct) || QueryTrack(ts,Q_IS_CORNU)) continue;   // Another selected track or Cornu - ignore
			}

			coOrd pos1 = GetTrkEndPos(ts,i);
			if (angle != 0.0)
				Rotate(&pos1,orig,angle);
			else {
				pos1.x +=base.x;
				pos1.y +=base.y;
			}
			coOrd pos2;
			pos2 = pos1;
			track_p tt;
			if ((tt=OnTrackIgnore(&pos2,FALSE,TRUE,ts))!=NULL) {
				if (GetTrkGauge(ts) != GetTrkGauge(tt)) continue;	//Ignore if different gauges
				if (!GetTrkSelected(tt)) {							//Ignore if new track is selected
					EPINX_T epp = PickUnconnectedEndPointSilent(pos2, tt);
					if (epp>=0) {
						DIST_T d = FindDistance(pos1,GetTrkEndPos(tt,epp));
						if (IsClose(d)) {
							*ep1 = epp;
							*ep2 = i;
							*t1 = tt;
							*t2 = ts;
							return TRUE;
						}
					} else {
						epp = PickEndPoint(pos2,tt);      //Any close end point (even joined)
						if (epp>=0) {
							ct = GetTrkEndTrk(tt,epp);
							if (ct && GetTrkSelected(ct)) {  //Point is junction to selected track - so will be broken
								DIST_T d = FindDistance(pos1,GetTrkEndPos(tt,epp));
								if (IsClose(d)) {
									*ep1 = epp;
									*ep2 = i;
									*t1 = tt;
									*t2 = ts;
									return TRUE;
								}
							}
						}
					}
				}
			}
		}
	}
	return FALSE;
}

void DrawHighlightLayer(int layer) {
	track_p ts = NULL;
	BOOL_T initial = TRUE;
	coOrd layer_hi = zero,layer_lo = zero;
	while ( TrackIterate( &ts ) ) {
		if ( !GetLayerVisible( GetTrkLayer( ts))) continue;
		if (!GetTrkSelected(ts)) continue;
		if (GetTrkLayer(ts) != layer) continue;
		coOrd hi,lo;
		GetBoundingBox(ts, &hi, &lo);
		if (initial) {
			layer_hi = hi;
			layer_lo = lo;
			initial = FALSE;
		} else {
			if (layer_hi.x < hi.x ) layer_hi.x = hi.x;
			if (layer_hi.y < hi.y ) layer_hi.y = hi.y;
			if (layer_lo.x > lo.x ) layer_lo.x = lo.x;
			if (layer_lo.y > lo.y ) layer_lo.y = lo.y;
		}
	}
	wPos_t margin = (wPos_t)(10.5*mainD.scale/mainD.dpi);
	layer_hi.x +=margin;
	layer_hi.y +=margin;
	layer_lo.x -=margin;
	layer_lo.y -=margin;

	wPos_t rect[4][2];
	int type[4];
	coOrd top_left, bot_right;
	top_left.x = layer_lo.x; top_left.y = layer_hi.y;
	bot_right.x = layer_hi.x; bot_right.y = layer_lo.y;
	type[0] = type[1] = type[2] = type[3] = 0;
	mainD.CoOrd2Pix(&mainD,layer_lo,&rect[0][0],&rect[0][1]);
	mainD.CoOrd2Pix(&mainD,top_left,&rect[1][0],&rect[1][1]);
	mainD.CoOrd2Pix(&mainD,layer_hi,&rect[2][0],&rect[2][1]);
	mainD.CoOrd2Pix(&mainD,bot_right,&rect[3][0],&rect[3][1]);
	wDrawPolygon(tempD.d,rect,(wPolyLine_e *)type,4,wDrawColorPowderedBlue,0,wDrawLineDash,wDrawOptTemp,0,0);
}

void SetUpMenu2(coOrd pos, track_p trk) {
	wMenuPushEnable( menuPushModify,FALSE);
	wMenuPushEnable(descriptionMI,FALSE);
	wMenuPushEnable( rotateAlignMI, FALSE );
	wMenuPushEnable( hideMI, FALSE );
	wMenuPushEnable( bridgeMI, FALSE );
	wMenuPushEnable( tiesMI, FALSE );
	if ((trk) &&
		QueryTrack(trk,Q_CAN_ADD_ENDPOINTS)) {   //Turntable snap to center if within 1/4 radius
		trackParams_t trackParams;
		if (GetTrackParams(PARAMS_CORNU, trk, pos, &trackParams)) {
			DIST_T dist = FindDistance(pos, trackParams.ttcenter);
			if (dist < trackParams.ttradius/4) {
					cmdMenuPos = trackParams.ttcenter;
			}
		}
	}
	if (trk && !QueryTrack( trk, Q_IS_DRAW )) {
		wMenuPushEnable( hideMI, TRUE );
		wMenuPushEnable( bridgeMI, TRUE );
		wMenuPushEnable( tiesMI, TRUE );
	}
	if (trk) {
		wMenuPushEnable( menuPushModify,
					(QueryTrack( trk, Q_CAN_MODIFY_CONTROL_POINTS ) ||
					QueryTrack( trk, Q_IS_CORNU ) ||
					(QueryTrack( trk, Q_IS_DRAW ) && !QueryTrack( trk, Q_IS_TEXT )) ||
					QueryTrack( trk, Q_IS_ACTIVATEABLE)));
	}
	if ((trk)) {
		wMenuPushEnable(descriptionMI, QueryTrack( trk, Q_HAS_DESC ));
		moveDescTrk = trk;
		moveDescPos = pos;
	}
	if (selectedTrackCount>0)
		wMenuPushEnable( rotateAlignMI, TRUE );
}


static STATUS_T CmdMove(
		wAction_t action,
		coOrd pos )
{
	static coOrd base;
	static coOrd orig;
	static int state;

	static EPINX_T ep1;
	static EPINX_T ep2;
	static track_p t1;
	static track_p t2;
	static BOOL_T doingMove;

	switch( action & 0xFF) {

		case C_START:
			DYNARR_RESET(trkSeg_t,anchors_da);
			if (selectedTrackCount == 0) {
				ErrorMessage( MSG_NO_SELECTED_TRK );
				return C_TERMINATE;
			}
			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			InfoMessage( _("Drag to move selected tracks - Shift+Ctrl+Arrow micro-steps the move") );
			state = 0;
			ep1 = -1;
			ep2 = -1;
			doingMove = FALSE;
			break;

		case wActionMove:
			DYNARR_RESET(trkSeg_t,anchors_da);
			CreateMoveAnchor(pos);
			break;
		case C_DOWN:
			DYNARR_RESET(trkSeg_t,anchors_da);
			if (doingMove) {
				doingMove = FALSE;
				UndoEnd();
			}

			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			UndoStart( _("Move Tracks"), "move" );
			base = zero;
			orig = pos;
			DYNARR_RESET(track_p,auto_select_da);
			GetMovedTracks(TRUE);
			SetMoveD( TRUE, base, 0.0 );
			drawCount = 0;
			state = 1;
			return C_CONTINUE;
		case C_MOVE:
			DYNARR_RESET(trkSeg_t,anchors_da);
			ep1=-1;
			ep2=-1;
			drawEnable = enableMoveDraw;
			base.x = pos.x - orig.x;
			base.y = pos.y - orig.y;
			SnapPos( &base );
			SetMoveD( TRUE, base, 0.0 );
			if (((MyGetKeyState()&(WKEY_ALT)) == 0) == magneticSnap) {  // ALT
				if (FindEndIntersection(base,zero,0.0,&t1,&ep1,&t2,&ep2)) {
					coOrd pos2 = GetTrkEndPos(t2,ep2);
					pos2.x +=base.x;
					pos2.y +=base.y;
					CreateEndAnchor(pos2,FALSE);
					CreateEndAnchor(GetTrkEndPos(t1,ep1),TRUE);
				}
			}
#ifdef DRAWCOUNT
			InfoMessage( "   [%s %s] #%ld", FormatDistance(base.x), FormatDistance(base.y), drawCount );
#else
			InfoMessage( "   [%s %s]", FormatDistance(base.x), FormatDistance(base.y) );
#endif
			drawEnable = TRUE;
			return C_CONTINUE;
		case C_UP:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 0;
			FreeTempStrings();
			if (t1 && ep1>=0 && t2 && ep2>=0) {
				MoveToJoin(t2,ep2,t1,ep1);
			} else {
				MoveTracks( FALSE, TRUE, FALSE, base, zero, 0.0, TRUE );
			}
			ep1 = -1;
			ep2 = -1;
			RemoveEndCornus();
			tlist_da.cnt = 0;
			return C_TERMINATE;

		case C_CMDMENU:
			if (doingMove) UndoEnd();
			doingMove = FALSE;
			base = pos;
			track_p trk = OnTrack(&pos, FALSE, FALSE);  //Note pollutes pos if turntable
			if ((trk) &&
				QueryTrack(trk,Q_CAN_ADD_ENDPOINTS)) {   //Turntable snap to center if within 1/4 radius
				trackParams_t trackParams;
				if (GetTrackParams(PARAMS_CORNU, trk, pos, &trackParams)) {
					DIST_T dist = FindDistance(base, trackParams.ttcenter);
					if (dist < trackParams.ttradius/4) {
							cmdMenuPos = trackParams.ttcenter;
					}
				}
			}
			moveDescPos = pos;
			moveDescTrk = trk;
			SetUpMenu2(pos,trk);
			menuPos = pos;
			wMenuPopupShow( selectPopup2M );
			return C_CONTINUE;

		case C_TEXT:
			if ((action>>8) == 'c') {
				panCenter = pos;
				LOG( log_pan, 2, ( "PanCenter:Sel-%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
				PanHere((void*)0);
			}
			if ((action>>8) == 'e') {
				DoZoomExtents(0);
			}
			if ((action>>8) == '0' || (action>>8 == 'o')) {
				PanMenuEnter('o');
			}
			break;
		case C_REDRAW:
			/* DO_REDRAW */
			if (anchors_da.cnt)
				DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			if ( state == 0 )
				break;
			DrawMovedTracks();

			break;

		case wActionExtKey:
			if (state) return C_CONTINUE;
			if (SelectedTracksAreFrozen()) return C_TERMINATE;
			if ((MyGetKeyState() &
					(WKEY_SHIFT | WKEY_CTRL)) == (WKEY_SHIFT | WKEY_CTRL)) {  //Both
				base = zero;
				DIST_T w = tempD.scale/tempD.dpi;
				switch((wAccelKey_e) action>>8) {
					case wAccelKey_Up:
						base.y = w;
						break;
					case wAccelKey_Down:
						base.y = -w;
						break;
					case wAccelKey_Left:
						base.x = -w;
						break;
					case wAccelKey_Right:
						base.x = w;
						break;
					default:
						return C_CONTINUE;
						break;
				}

			drawEnable = enableMoveDraw;
			GetMovedTracks(TRUE);
			if (!doingMove) UndoStart( _("Move Tracks"), "move" );
			doingMove = TRUE;
			SetMoveD( TRUE, base, 0.0 );
			MoveTracks( FALSE, TRUE, FALSE, base, zero, 0.0, FALSE );
			++microCount;
			if (microCount>5) {
				microCount = 0;
				MainRedraw(); // Micro step move
			}
			RemoveEndCornus();
			return C_CONTINUE;
			}
			break;

		case C_FINISH:
			if (doingMove) {
				doingMove = FALSE;
				UndoEnd();
			}
			RemoveEndCornus();
			tlist_da.cnt = 0;
			break;
		case C_CONFIRM:
		case C_CANCEL:
			if (doingMove) {
				doingMove = FALSE;
				UndoUndo();
			}
			RemoveEndCornus();
			tlist_da.cnt = 0;
			break;
		default:
			break;
	}
	return C_CONTINUE;
}



static int rotateAlignState = 0;

static void RotateAlign( BOOL_T align )
{
	rotateAlignState = 0;
	if (align) {
		rotateAlignState = 1;
		doingAlign = TRUE;
		mode = MOVE;
		InfoMessage( _("Align: Click on a selected object to be aligned") );
	}
}

static STATUS_T CmdRotate(
		wAction_t action,
		coOrd pos )
{
	static coOrd base;
	static coOrd orig_base;
	static coOrd orig;
	static ANGLE_T angle;
	static BOOL_T drawnAngle;
	static ANGLE_T baseAngle;
	static BOOL_T clockwise;
	static BOOL_T direction_set;
	static track_p trk;
	ANGLE_T angle1;
	coOrd pos1;
	static int state;

	static EPINX_T ep1;
	static EPINX_T ep2;
	static track_p t1;
	static track_p t2;

	switch( action ) {

		case C_START:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 0;
			if (selectedTrackCount == 0) {
				ErrorMessage( MSG_NO_SELECTED_TRK );
				return C_TERMINATE;
			}
			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			InfoMessage( _("Drag to rotate selected tracks, Shift+RightClick for QuickRotate Menu") );
			wMenuPushEnable( rotateAlignMI, TRUE );
			rotateAlignState = 0;
			ep1 = -1;
			ep2 = -1;
			break;
		case wActionMove:
			DYNARR_RESET(trkSeg_t,anchors_da);
			CreateRotateAnchor(pos);
			break;
		case C_DOWN:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 1;
			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			UndoStart( _("Rotate Tracks"), "rotate" );
			DYNARR_RESET(track_p,auto_select_da);
			if ( rotateAlignState == 0 ) {
				drawnAngle = FALSE;
				angle = 0.0;
				base = orig = pos;
				trk = OnTrack(&pos, FALSE, FALSE);  //Note pollutes pos if turntable
				if ((trk) &&
					QueryTrack(trk,Q_CAN_ADD_ENDPOINTS)) {   //Turntable snap to center if within 1/4 radius
					trackParams_t trackParams;
					if (GetTrackParams(PARAMS_CORNU, trk, pos, &trackParams)) {
						DIST_T dist = FindDistance(base, trackParams.ttcenter);
						if (dist < trackParams.ttradius/4) {
								base = orig = trackParams.ttcenter;
								InfoMessage( _("Center of Rotation snapped to Turntable center") );
						}
					}
				}
				CreateRotateAnchor(orig);
				GetMovedTracks(TRUE);
				SetMoveD( FALSE, base, angle );

				/*DrawLine( &mainD, base, orig, 0, wDrawColorBlack );
				DrawMovedTracks(FALSE, orig, angle);*/
			} else {
				pos1 = pos;
				drawnAngle = FALSE;
				onTrackInSplit = TRUE;
				trk = OnTrack( &pos, TRUE, FALSE );
				onTrackInSplit = FALSE;
				if ( trk == NULL ) return C_CONTINUE;
				angle1 = NormalizeAngle( GetAngleAtPoint( trk, pos, NULL, NULL ) );
				if ( rotateAlignState == 1 ) {
					if ( !GetTrkSelected(trk) ) {
						NoticeMessage( MSG_1ST_TRACK_MUST_BE_SELECTED, _("Ok"), NULL );
					} else {
						base = pos;
						baseAngle = angle1;
						getboundsCount = 0;
						DoSelectedTracks( GetboundsDoIt );
						orig.x = (getboundsLo.x+getboundsHi.x)/2.0;
						orig.y = (getboundsLo.y+getboundsHi.y)/2.0;
/*printf( "orig = [%0.3f %0.3f], baseAngle = %0.3f\n", orig.x, orig.y, baseAngle );*/
					}
				} else {
					if ( GetTrkSelected(trk) ) {
						ErrorMessage( MSG_2ND_TRACK_MUST_BE_UNSELECTED );
						angle = 0;
					} else {
						angle = NormalizeAngle(angle1-baseAngle);
						//if ( angle > 90 && angle < 270 )
						//	angle = NormalizeAngle( angle + 180.0 );
						//if ( NormalizeAngle( FindAngle( base, pos1 ) - angle1 ) < 180.0 )
						//	angle = NormalizeAngle( angle + 180.0 );
/*printf( "angle 1 = %0.3f\n", angle );*/
						if ( angle1 > 180.0 ) angle1 -= 180.0;
						InfoMessage( _("Angle %0.3f"), angle1 );
					}
					GetMovedTracks(TRUE);
					SetMoveD( FALSE, orig, angle );
				}
			}
			return C_CONTINUE;
		case C_MOVE:
			DYNARR_RESET(trkSeg_t,anchors_da);
			ep1=-1;
			ep2=-1;
			if ( rotateAlignState == 1 )
				return C_CONTINUE;
			if ( rotateAlignState == 2 ) {
				pos1 = pos;
				onTrackInSplit = TRUE;
				trk = OnTrack( &pos, TRUE, FALSE );
				onTrackInSplit = FALSE;
				if ( trk == NULL )
					return C_CONTINUE;
				if ( GetTrkSelected(trk) ) {
					ErrorMessage( MSG_2ND_TRACK_MUST_BE_UNSELECTED );
					return C_CONTINUE;
				}
				angle1 = NormalizeAngle( GetAngleAtPoint( trk, pos, NULL, NULL ) );
				angle = NormalizeAngle(angle1-baseAngle);
				if ( angle > 90 && angle < 270 )
					angle = NormalizeAngle( angle + 180.0 );
				if ( NormalizeAngle( FindAngle( pos, pos1 ) - angle1 ) < 180.0 )
					angle = NormalizeAngle( angle + 180.0 );
				if ( angle1 > 180.0 ) angle1 -= 180.0;
				InfoMessage( _("Angle %0.3f"), angle1 );
				SetMoveD( FALSE, orig, angle );
/*printf( "angle 2 = %0.3f\n", angle );*/
				return C_CONTINUE;
			}
			ANGLE_T diff_angle = 0.0;
			base = pos;
			drawEnable = enableMoveDraw;
			if ( FindDistance( orig, pos ) > (20.0/75.0)*mainD.scale ) {
				ANGLE_T old_angle = angle;
				angle = FindAngle( orig, pos );
				if (!drawnAngle) {
					baseAngle = angle;
					drawnAngle = TRUE;
					direction_set = FALSE;
				} else {
					if (!direction_set) {
						if (DifferenceBetweenAngles(baseAngle,angle)>=0) clockwise = TRUE;
						else clockwise = FALSE;
						direction_set = TRUE;
					} else {
						if (clockwise) {
							if (DifferenceBetweenAngles(baseAngle,angle)<0 && fabs(DifferenceBetweenAngles(baseAngle, old_angle))<5)
								clockwise = FALSE;
						} else {
							if (DifferenceBetweenAngles(baseAngle,angle)>=0 && fabs(DifferenceBetweenAngles(baseAngle,old_angle))<5)
								clockwise = TRUE;
						}
					}
				}
				orig_base = base = pos;
				//angle = NormalizeAngle( angle-baseAngle );
				diff_angle = DifferenceBetweenAngles(baseAngle,angle);
				if ( (MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) == (WKEY_CTRL|WKEY_SHIFT) ) {  //Both Shift+Ctrl
					if (clockwise) {
						if (diff_angle<0) diff_angle+=360;
					} else {
						if (diff_angle>0) diff_angle-=360;
					}
					diff_angle = floor((diff_angle+7.5)/15.0)*15.0;
					angle = baseAngle+diff_angle;
				}
				Translate( &base, orig, angle, FindDistance(orig,pos) );  //Line one
				Translate( &orig_base,orig, baseAngle, FindDistance(orig,pos)<=(60.0/75.00*mainD.scale)?FindDistance(orig,pos):60.0/75.00*mainD.scale ); //Line two
				SetMoveD( FALSE, orig, NormalizeAngle( angle-baseAngle ) );
				if (((MyGetKeyState()&(WKEY_ALT)) == WKEY_ALT) != magneticSnap) {  //Just Shift
					if (FindEndIntersection(zero,orig,NormalizeAngle( angle-baseAngle ),&t1,&ep1,&t2,&ep2)) {
						coOrd pos2 = GetTrkEndPos(t2,ep2);
						coOrd pos1 = GetTrkEndPos(t1,ep1);
						Rotate(&pos2,orig,NormalizeAngle( angle-baseAngle ));
						CreateEndAnchor(pos2,FALSE);
						CreateEndAnchor(pos1,TRUE);
					}
				}

#ifdef DRAWCOUNT
				InfoMessage( _("Angle %0.3f #%ld"), fabs(diff_angle), drawCount );
#else
				InfoMessage( _("Angle %0.3f %s"), fabs(diff_angle), clockwise?"Clockwise":"Counter-Clockwise" );
#endif
				wFlush();
				drawEnable = TRUE;
			} else
				InfoMessage( _("Origin Set. Drag away to set start angle"));

			return C_CONTINUE;

		case C_UP:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 0;
			if (t1 && ep1>=0 && t2 && ep2>=0) {
				MoveToJoin(t2,ep2,t1,ep1);
				CleanSegs(&tempSegs_da);
				rotateAlignState = 0;
			} else {
				if ( rotateAlignState == 1 ) {
					if ( trk && GetTrkSelected(trk) ) {
						InfoMessage( _("Align: Click on the 2nd unselected object") );
						rotateAlignState = 2;
					}
					return C_CONTINUE;
				}
				CleanSegs(&tempSegs_da);
				if ( rotateAlignState == 2 ) {
					MoveTracks( FALSE, FALSE, TRUE, zero, orig, angle, TRUE );
					rotateAlignState = 0;
				} else if (drawnAngle) {
					MoveTracks( FALSE, FALSE, TRUE, zero, orig, NormalizeAngle( angle-baseAngle ), TRUE );
				}
			}
			UndoEnd();
			RemoveEndCornus();
			tlist_da.cnt = 0;
			return C_TERMINATE;

		case C_CMDMENU:
			base = pos;
			trk = OnTrack(&pos, FALSE, FALSE);  //Note pollutes pos if turntable
			if ((trk) &&
				QueryTrack(trk,Q_CAN_ADD_ENDPOINTS)) {   //Turntable snap to center if within 1/4 radius
				trackParams_t trackParams;
				if (GetTrackParams(PARAMS_CORNU, trk, pos, &trackParams)) {
					DIST_T dist = FindDistance(base, trackParams.ttcenter);
					if (dist < trackParams.ttradius/4) {
							cmdMenuPos = trackParams.ttcenter;
					}
				}
			}
			moveDescPos = pos;
			moveDescTrk = trk;
			SetUpMenu2(pos,trk);
			menuPos = pos;
			wMenuPopupShow( selectPopup2M );
			return C_CONTINUE;

		case C_TEXT:
			if ((action>>8) == 'd') {
				panCenter = pos;
				LOG( log_pan, 2, ( "PanCenter:Sel-%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
				PanHere((void*)0);
			}
			if ((action>>8) == 'e') {
				DoZoomExtents(0);
			}
			if ((action>>8) == '0' || (action>>8 == 'o')) {
				PanMenuEnter('o');
			}
			break;
		case C_REDRAW:
			if (anchors_da.cnt)
				DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			/* DO_REDRAW */
			if ( state == 0 )
				break;
			if ( rotateAlignState != 2 ) {
				DIST_T width = tempD.scale*0.15;
				DrawLine( &tempD, base, orig, 0, wDrawColorBlue );
				if (drawnAngle) {
					DrawLine( &tempD, orig_base, orig, width/2, wDrawColorBlue );
					ANGLE_T a = DifferenceBetweenAngles(FindAngle(orig, orig_base),FindAngle(orig, base));

					DIST_T dist = FindDistance(orig,base);
					if (dist>(60.0/75.0)*mainD.scale) dist = (60.0/75.0)*mainD.scale;

					if (direction_set) {
						if (clockwise) {
							if (a<0) a = a + 360;
							DrawArc( &tempD, orig, dist/2, FindAngle(orig,orig_base), a, FALSE, 0, wDrawColorBlue);
						} else {
							if (a>0) a = a - 360;
							DrawArc( &tempD, orig, dist/2, FindAngle(orig,base), fabs(a), FALSE, 0, wDrawColorBlue);
						}
						DIST_T d;
						d = mainD.scale*0.25;
						ANGLE_T arrow_a = NormalizeAngle(FindAngle(orig,orig_base)+a/2);
						coOrd arr1,arr2,arr3;
						Translate(&arr2,orig,arrow_a,dist/2);
						if (clockwise) arrow_a +=90;
						else arrow_a -=90;
						Translate(&arr1,arr2,arrow_a+135,d/2);
						Translate(&arr3,arr2,arrow_a-135,d/2);
						DrawLine( &tempD, arr1, arr2, 0, wDrawColorBlue );
						DrawLine( &tempD, arr2, arr3, 0, wDrawColorBlue );
					}
				}

			}
			DrawMovedTracks();
			break;

		}
	return C_CONTINUE;
}

static void QuickMove( void* pos) {
	coOrd move_pos = *(coOrd*)pos;
	DYNARR_RESET(track_p,auto_select_da);
	if ( SelectedTracksAreFrozen() )
		return;
	wDrawDelayUpdate( mainD.d, TRUE );
	GetMovedTracks(FALSE);
	UndoStart( _("Move Tracks"), "Move Tracks" );
	MoveTracks( TRUE, TRUE, FALSE, move_pos, zero, 0.0, TRUE );
	wDrawDelayUpdate( mainD.d, FALSE );
}

static void QuickRotate( void* pangle )
{
	ANGLE_T angle = (ANGLE_T)(long)pangle;
	DYNARR_RESET(track_p,auto_select_da);
	if ( SelectedTracksAreFrozen() )
		return;
	wDrawDelayUpdate( mainD.d, TRUE );
	GetMovedTracks(FALSE);
	//DrawSelectedTracksD( &mainD, wDrawColorWhite );
	UndoStart( _("Rotate Tracks"), "Rotate Tracks" );
	MoveTracks( TRUE, FALSE, TRUE, zero, cmdMenuPos, (double)angle/1000, TRUE);
	wDrawDelayUpdate( mainD.d, FALSE );
}


static wMenu_p moveDescM;
static wMenuToggle_p moveDescMI;
static wMenuToggle_p moveDetailDescMI;

static void ChangeDetailedFlag( wBool_t set, void * mode )
{
	wDrawDelayUpdate( mainD.d, TRUE );
	UndoStart( _("Toggle Detail"), "Modedetail( T%d )", GetTrkIndex(moveDescTrk) );
	UndoModify( moveDescTrk );
	UndrawNewTrack( moveDescTrk );
	if ( ( GetTrkBits( moveDescTrk ) & TB_DETAILDESC ) == 0 ) {
		ClrTrkBits( moveDescTrk, TB_HIDEDESC );
		SetTrkBits( moveDescTrk, TB_DETAILDESC );
	} else
		ClrTrkBits( moveDescTrk, TB_DETAILDESC );
	DrawNewTrack( moveDescTrk );
	wDrawDelayUpdate( mainD.d, FALSE );
}

static void ChangeDescFlag( wBool_t set, void * mode )
{
	wDrawDelayUpdate( mainD.d, TRUE );
	UndoStart( _("Toggle Label"), "Modedesc( T%d )", GetTrkIndex(moveDescTrk) );
	UndoModify( moveDescTrk );
	UndrawNewTrack( moveDescTrk );
	if ( ( GetTrkBits( moveDescTrk ) & TB_HIDEDESC ) == 0 )
		SetTrkBits( moveDescTrk, TB_HIDEDESC );
	else
		ClrTrkBits( moveDescTrk, TB_HIDEDESC );
	DrawNewTrack( moveDescTrk );
	wDrawDelayUpdate( mainD.d, FALSE );
}

track_p FindTrackDescription(coOrd pos, EPINX_T * ep_o, int * mode_o, BOOL_T show_hidden, BOOL_T * hidden_o)  {
	 	track_p trk = NULL;
		DIST_T d, dd = 10000;
		track_p trk1 = NULL;
		EPINX_T ep1=-1, ep=-1;
		BOOL_T hidden_t, hidden;
		coOrd dpos = pos;
		coOrd cpos;
		int mode = -1;
		while ( TrackIterate( &trk1 ) ) {
			if ( !GetLayerVisible(GetTrkLayer(trk1)) )
				continue;
			if ( (!GetTrkVisible(trk1)) && drawTunnel==0 )
				continue;
			if ( (labelEnable&LABELENABLE_ENDPT_ELEV)!=0 && *mode_o <= 0) {
				for ( ep1=0; ep1<GetTrkEndPtCnt(trk1); ep1++ ) {
					d = EndPtDescriptionDistance( pos, trk1, ep1, &dpos, FALSE, NULL );  //No hidden
					if ( d < dd ) {
						dd = d;
						trk = trk1;
						ep = ep1;
						mode = 0;
						hidden = FALSE;
						cpos= dpos;
					}
				}
			}
			if ( !QueryTrack( trk1, Q_HAS_DESC ) && (*mode_o > 0) )
				continue;
			if ((labelEnable&LABELENABLE_TRKDESC)==0)
				continue;
			if ( ( GetTrkBits( trk1 ) & TB_HIDEDESC ) != 0 ) {
				if ( !show_hidden ) continue;
			}
			d = CompoundDescriptionDistance( pos, trk1, &dpos, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 1;
				hidden = hidden_t;
				cpos = dpos;
			}
			d = CurveDescriptionDistance( pos, trk1, &dpos, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 2;
				hidden = hidden_t;
				cpos = dpos;
			}
			d = CornuDescriptionDistance( pos, trk1, &dpos, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 3;
				hidden = hidden_t;
				cpos = dpos;
			}
			d = BezierDescriptionDistance( pos, trk1, &dpos, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 4;
				hidden = hidden_t;
				cpos = dpos;
			}
			d = StraightDescriptionDistance( pos, trk1, &dpos, show_hidden, &hidden_t );
			if (d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 5;
				hidden = hidden_t;
				cpos = dpos;
			}
			d = JointDescriptionDistance( pos, trk1, &dpos, show_hidden, &hidden_t );
			if (d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 6;
				hidden = hidden_t;
				cpos = dpos;
			}

		}

		coOrd pos1 = pos;

		if ((trk != NULL) && IsClose(dd) ) {
			if (ep_o) *ep_o = ep;
			if (mode_o) *mode_o = mode;
			if (hidden_o) *hidden_o = hidden;
			return trk;
		} else {  // Return other track for description (not near to description but nearest to track)
			if ((trk1 = OnTrack(&pos1, FALSE, FALSE))==NULL) return NULL;
			if (!QueryTrack( trk1, Q_HAS_DESC )) return NULL;
			if (IsClose(FindDistance(pos,pos1))) {
				if (mode_o) *mode_o = -1;
				if (ep_o) *ep_o = -1;
				if (hidden_o) *hidden_o = GetTrkBits( trk1 ) & TB_HIDEDESC;
				return trk1;
			}
		}

		return NULL;
}

static long moveDescMode;

STATUS_T CmdMoveDescription(
		wAction_t action,
		coOrd pos )
{
	static track_p trk;
	static EPINX_T ep;
	static BOOL_T hidden;
	static int mode;
	BOOL_T bChanged;

	moveDescMode = (long)commandContext;   //Context 0 = everything, 1 means elevations, 2 means descriptions

	bChanged = FALSE;
	switch (action&0xFF) {
	case C_START:
		moveDescTrk = NULL;
		moveDescPos = zero;
		trk = NULL;
		hidden = FALSE;
		mode = -1;
		if ( labelWhen < 2 || mainD.scale > labelScale ||
			 (labelEnable&(LABELENABLE_TRKDESC|LABELENABLE_ENDPT_ELEV))==0 ) {
			ErrorMessage( MSG_DESC_NOT_VISIBLE );
			return C_ERROR;
		}
		InfoMessage( _("Select and drag a description") );
		break;
	case C_TEXT:
		if (!moveDescTrk) return C_CONTINUE;
		bChanged = FALSE;
		if (action>>8 == 's') {
			if ( ( GetTrkBits( moveDescTrk ) & TB_HIDEDESC) != 0 )
				bChanged = TRUE;
			ClrTrkBits( moveDescTrk, TB_HIDEDESC );
		} else if (action>>8 == 'h')  {
			if ( ( GetTrkBits( moveDescTrk ) & TB_HIDEDESC) == 0 )
				bChanged = TRUE;
			SetTrkBits( moveDescTrk, TB_HIDEDESC );
			ClrTrkBits( moveDescTrk, TB_DETAILDESC );
		} else if (action>>8 == 'd') {				//Toggle Detailed
			bChanged = TRUE;
			if ((GetTrkBits( moveDescTrk ) & TB_DETAILDESC) != 0)
				ClrTrkBits( moveDescTrk, TB_DETAILDESC);
			else {
				ClrTrkBits( moveDescTrk, TB_HIDEDESC );
				SetTrkBits( moveDescTrk, TB_DETAILDESC );
			}
		}
		if ( bChanged ) {
			// We should push the draw/undraw of the description down
			// but there is no clear way to do that
			MainRedraw(); // CmdMoveDescription
		}
		/*no break*/
	case wActionMove:
		if ( labelWhen < 2 || mainD.scale > labelScale ) return C_CONTINUE;
		mode = moveDescMode-1;   // -1 means everything, 0 means elevations only, 1 means descriptions only
		if ((trk=FindTrackDescription(pos,&ep,&mode,TRUE,&hidden))!=NULL) {
			if (mode==0) {
				InfoMessage(_("Elevation description"));
			} else {
				if (hidden) {
					InfoMessage(_("Hidden description - 's' to Show, 'd' Details"));
					moveDescTrk = trk;
					moveDescPos = pos;
				} else {
					InfoMessage(_("Shown description - 'h' to Hide"));
					moveDescTrk = trk;
					moveDescPos = pos;
				}
			}
			return C_CONTINUE;
		}
		InfoMessage( _("Select and drag a description") );
		break;
	case C_DOWN:
		if (( labelWhen < 2 || mainD.scale > labelScale ) ||
		 (labelEnable&(LABELENABLE_TRKDESC|LABELENABLE_ENDPT_ELEV))==0 ) {
			 	 ErrorMessage( MSG_DESC_NOT_VISIBLE );
			return C_ERROR;
		 }
		mode = moveDescMode-1;
		trk = FindTrackDescription(pos,&ep,&mode,TRUE,&hidden);
		if (trk == NULL )
			return C_CONTINUE;
		if (hidden) {
			ClrTrkBits( trk, TB_HIDEDESC );
			InfoMessage(_("Hidden Label - Drag to reveal"));
		} else {
			InfoMessage(_("Drag label"));
		}
		UndoStart( _("Move Label"), "Modedesc( T%d )", GetTrkIndex(trk) );
		UndoModify( trk );
		/* no break */
	case C_MOVE:
	case C_UP:
	case C_REDRAW:
		if ( labelWhen < 2 || mainD.scale > labelScale )
			return C_CONTINUE;
		if ( trk == NULL )
			return C_CONTINUE;
		if ( action == C_REDRAW ) {
			if (mode==0) {
				DrawEndPt2( &tempD, trk, ep, wDrawColorBlue );
			} else {
				if (hidden) {
					DrawTrack( trk,&tempD,wDrawColorAqua);
				} else {
					DrawTrack( trk,&tempD,wDrawColorBlue);
				}
			}
		}
		switch (mode) {
		case 0:
			return EndPtDescriptionMove( trk, ep, action, pos );
		case 1:
			return CompoundDescriptionMove( trk, action, pos );
		case 2:
			return CurveDescriptionMove( trk, action, pos );
		case 3:
			return CornuDescriptionMove( trk, action, pos );
		case 4:
			return BezierDescriptionMove( trk, action, pos );
		}
		hidden = FALSE;
		if ( action == C_UP ) {
			trk = NULL;
			InfoMessage(_("To Hide, use Context Menu"));
		}
		break;

	case C_CMDMENU:
		if (trk == NULL) {
			moveDescTrk = OnTrack( &pos, TRUE, FALSE );
			moveDescPos = pos;
		} else {
			moveDescTrk = trk;
			moveDescPos = pos;
		}
		if ( moveDescTrk == NULL ) break;
		if ( ! QueryTrack( moveDescTrk, Q_HAS_DESC ) ) break;
		if ( moveDescM == NULL ) {
			moveDescM = MenuRegister( "Move Desc Toggle" );
			moveDescMI = wMenuToggleCreate( moveDescM, "", _("Show/Hide Description"), 0, TRUE, ChangeDescFlag, NULL );
			moveDetailDescMI = wMenuToggleCreate( moveDescM, "", _("Toggle Detailed Description"), 0, TRUE, ChangeDetailedFlag, NULL );
		}
		wMenuToggleSet( moveDescMI, !( GetTrkBits( moveDescTrk ) & TB_HIDEDESC ) );
		wMenuToggleSet( moveDetailDescMI, ( GetTrkBits( moveDescTrk ) & TB_DETAILDESC ) );
		menuPos = pos;
		wMenuPopupShow( moveDescM );
		break;

	default:
		;
	}
		
	return C_CONTINUE;
}


static void FlipTracks(
		coOrd orig,
		ANGLE_T angle )
{
	track_p trk, trk1;
	EPINX_T ep, ep1;

	wSetCursor( mainD.d, wCursorWait );
	/*UndoStart( "Move/Rotate Tracks", "move/rotate" );*/
	if (selectedTrackCount <= incrementalDrawLimit) {
		wDrawDelayUpdate( mainD.d, TRUE );
		wDrawDelayUpdate( mapD.d, TRUE );
	}
	for ( trk=NULL; TrackIterate(&trk); ) {
		if ( !GetTrkSelected(trk) )
			continue;
		UndoModify( trk );
		if (selectedTrackCount <= incrementalDrawLimit) {
			 DrawTrack( trk, &mainD, wDrawColorWhite );
			 DrawTrack( trk, &mapD, wDrawColorWhite );
		}
		for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
			if ((trk1 = GetTrkEndTrk(trk,ep)) != NULL &&
				!GetTrkSelected(trk1)) {
				ep1 = GetEndPtConnectedToMe( trk1, trk );
				DisconnectTracks( trk, ep, trk1, ep1 );
				DrawEndPt( &mainD, trk1, ep1, wDrawColorBlack );
			}
		}
		FlipTrack( trk, orig, angle );
		if (selectedTrackCount <= incrementalDrawLimit) {
			 DrawTrack( trk, &mainD, wDrawColorBlack );
			 DrawTrack( trk, &mapD, wDrawColorBlack );
		}
	}
	if (selectedTrackCount > incrementalDrawLimit) {
		DoRedraw();
	} else {
		wDrawDelayUpdate( mainD.d, FALSE );
		wDrawDelayUpdate( mapD.d, FALSE );
	}
	wSetCursor( mainD.d, defaultCursor );
	UndoEnd();
	InfoCount( trackCount );
}


static STATUS_T CmdFlip(
		wAction_t action,
		coOrd pos )
{
	static coOrd pos0;
	static coOrd pos1;
	static int state;

	switch( action ) {

		case C_START:
			state = 0;
			if (selectedTrackCount == 0) {
				ErrorMessage( MSG_NO_SELECTED_TRK );
				return C_TERMINATE;
			}
			if (SelectedTracksAreFrozen())
				return C_TERMINATE;
			InfoMessage( _("Drag to mark mirror line") );
			break;
		case C_DOWN:
			state = 1;
			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			pos0 = pos1 = pos;
			return C_CONTINUE;
		case C_MOVE:
			pos1 = pos;
			InfoMessage( _("Angle %0.2f"), FindAngle( pos0, pos1 ) );
			return C_CONTINUE;
		case C_UP:
			UndoStart( _("Flip Tracks"), "flip" );
			FlipTracks( pos0, FindAngle( pos0, pos1 ) );
			state = 0;
			return C_TERMINATE;

#ifdef LATER
		case C_CANCEL:
#endif
		case C_REDRAW:
			if ( state == 0 )
				return C_CONTINUE;
			DrawLine( &tempD, pos0, pos1, 0, wDrawColorBlack );
			return C_CONTINUE;

		default:
			break;
	}
	return C_CONTINUE;
}

static BOOL_T SelectArea(
		wAction_t action,
		coOrd pos )
{
	static coOrd pos0;
	static int state;
	static coOrd base, size, lo, hi;
	static BOOL_T add;
	static BOOL_T subtract;
	int cnt;

	track_p trk;

	switch (action) {

	case C_START:
		state = 0;
		add = FALSE;
		subtract = FALSE;
		return FALSE;

	case C_DOWN:
	case C_RDOWN:
		pos0 = pos;
		add = (action == C_DOWN);
		subtract = (action == C_RDOWN);
		return TRUE;

	case C_MOVE:
	case C_RMOVE:
		if (state == 0) {
			state = 1;
		}
		base = pos0;
		size.x = pos.x - pos0.x;
		if (size.x < 0) {
			size.x = - size.x;
			base.x = pos.x;
		}
		size.y = pos.y - pos0.y;
		if (size.y < 0) {
			size.y = - size.y;
			base.y = pos.y;
		}
		return TRUE;

	case C_UP:
	case C_RUP:
		if (state == 1) {
			state = 0;
			add = (action == C_UP);
			subtract = (action == C_RUP);
			cnt = 0;
			trk = NULL;
			if (add && (selectMode == 0)) SetAllTrackSelect( FALSE );		//Remove all tracks first
			while ( TrackIterate( &trk ) ) {
				GetBoundingBox( trk, &hi, &lo );
				if (GetLayerVisible( GetTrkLayer( trk ) ) &&
					lo.x >= base.x && hi.x <= base.x+size.x &&
					lo.y >= base.y && hi.y <= base.y+size.y) {
					if ( (GetTrkSelected( trk )==0) == (action==C_UP) )
					  cnt++;
				}
			}
			trk = NULL;
			while ( TrackIterate( &trk ) ) {
				GetBoundingBox( trk, &hi, &lo );
				if (GetLayerVisible( GetTrkLayer( trk ) ) &&
					lo.x >= base.x && hi.x <= base.x+size.x &&
					lo.y >= base.y && hi.y <= base.y+size.y) {
					if ( (GetTrkSelected( trk )==0) == (action==C_UP) ) {
						if (GetLayerModule(GetTrkLayer(trk))) {
							if (add)
								DoModuleTracks(GetTrkLayer(trk),SelectOneTrack,TRUE);
							else
								DoModuleTracks(GetTrkLayer(trk),SelectOneTrack,FALSE);
						} else if (cnt > incrementalDrawLimit) {
							selectedTrackCount += (action==C_UP?1:-1);
							if (add)
								SetTrkBits( trk, TB_SELECTED );
							else
								ClrTrkBits( trk, TB_SELECTED );
						} else {
							SelectOneTrack( trk, add );
						}
					}
				}
			}
			add = FALSE;
			subtract = FALSE;
			if (cnt > incrementalDrawLimit) {
				MainRedraw(); // SelectArea C_UP
			} else {
				RedrawSelectedTracksBoundary();
			}
			SelectedTrackCountChange();
		}
		return FALSE;

	case C_CANCEL:
		state = 0;
		add = FALSE;
		subtract = FALSE;
		break;

	case C_REDRAW:
		if (state == 0)
			break;
		//Draw to-be selected tracks versus not.
		trk = NULL;
		if (selectMode == 0 && add) HighlightSelectedTracks(NULL, TRUE, TRUE);
		while ( TrackIterate( &trk ) ) {
			GetBoundingBox( trk, &hi, &lo );
			if (GetLayerVisible( GetTrkLayer( trk ) ) &&
				lo.x >= base.x && hi.x <= base.x+size.x &&
				lo.y >= base.y && hi.y <= base.y+size.y) {
				if (GetLayerModule(GetTrkLayer(trk))) {
					if (add)
						DoModuleTracks(GetTrkLayer(trk),DrawSingleTrack,TRUE);
					else if (subtract)
						DoModuleTracks(GetTrkLayer(trk),DrawSingleTrack,FALSE);
				} else {
					if (add) {
						if (selectMode == 0 && add)
							DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
						if (!GetTrkSelected(trk))
							DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
					}
					else if (subtract) {
						if (GetTrkSelected(trk))
							DrawTrack(trk,&tempD,wDrawColorPreviewUnselected);
					}
				}
			}
		}
		if (add || subtract) {
			DrawHilight( &tempD, base, size, add );
			return TRUE;
		}
		break;

	}
	return FALSE;
}

extern BOOL_T inDescribeCmd;
extern wIndex_t modifyCmdInx;
extern wIndex_t describeCmdInx;
extern wIndex_t panCmdInx;
extern wIndex_t trainCmdInx;

static STATUS_T SelectTrack( 
		coOrd pos )
{
	track_p trk;
	char msg[STR_SIZE];

	if (((trk = OnTrack( &pos, FALSE, FALSE )) == NULL) && selectZero) {   //If option set and !ctrl or unset and ctrl
		SetAllTrackSelect( FALSE );							//Unselect all
		return C_CONTINUE;
	}
	if (trk == NULL) return C_CONTINUE;
	inDescribeCmd = FALSE;
	DescribeTrack( trk, msg, sizeof msg );
	InfoMessage( msg );
	if (GetLayerModule(GetTrkLayer(trk))) {
		if (((MyGetKeyState() & WKEY_CTRL) && (selectMode==0)) || (!(MyGetKeyState() & WKEY_CTRL) && (selectMode==1)) )  {
			DoModuleTracks(GetTrkLayer(trk),SelectOneTrack,!GetTrkSelected(trk));
		} else {
			SetAllTrackSelect( FALSE );	 //Just this Track if selectMode = 0 and !CTRL or selectMode = 1 and CTRL
			DoModuleTracks(GetTrkLayer(trk),SelectOneTrack,TRUE);
		}
		RedrawSelectedTracksBoundary();
		return C_CONTINUE;
	}
	if (MyGetKeyState() & WKEY_SHIFT) {						//All track up to
		SelectConnectedTracks( trk, FALSE );
	} else if ((MyGetKeyState() & WKEY_CTRL) && (selectMode==0)) {
		SelectOneTrack( trk, !GetTrkSelected(trk) );
	} else if (!(MyGetKeyState() & WKEY_CTRL) && (selectMode==1)) {
		SelectOneTrack( trk, !GetTrkSelected(trk) );
	} else {
		SetAllTrackSelect( FALSE );							//Just this Track
		SelectOneTrack( trk, !GetTrkSelected(trk) );
	}
	RedrawSelectedTracksBoundary();

	return C_CONTINUE;
}

static STATUS_T Activate( coOrd pos) {
	track_p trk;
	if ((trk = OnTrack( &pos, TRUE, FALSE )) == NULL) {
				return C_CONTINUE;
	}
	if (GetLayerModule(GetTrkLayer(trk))) {
		return C_CONTINUE;
	}
	if (QueryTrack(trk,Q_IS_ACTIVATEABLE)) ActivateTrack(trk);

	return C_CONTINUE;

}

track_p IsInsideABox(coOrd pos) {
	track_p ts = NULL;
	while ( TrackIterate( &ts ) ) {
		if (!GetLayerVisible( GetTrkLayer( ts))) continue;
		if (!GetTrkSelected(ts)) continue;
		coOrd hi,lo;
		GetBoundingBox(ts, &hi, &lo);
		double boundary = mainD.scale*5/mainD.dpi;
		if ((pos.x>=lo.x-boundary && pos.x<=hi.x+boundary) && (pos.y>=lo.y-boundary && pos.y<=hi.y+boundary)) {
			return ts;
		}
	}
	return NULL;
}

void DrawHighlightBoxes(BOOL_T highlight_selected, BOOL_T select, track_p not_this) {
	track_p ts = NULL;
	coOrd origin,max;
	BOOL_T first = TRUE;
	while ( TrackIterate( &ts ) ) {
		if ( !GetLayerVisible( GetTrkLayer( ts))) continue;
		if (!GetTrkSelected(ts)) continue;
		if (GetLayerModule(GetTrkLayer(ts))) {
			DrawHighlightLayer(GetTrkLayer(ts));
		}
		coOrd hi,lo;
		if (highlight_selected && (ts != not_this)) DrawTrack(ts,&tempD,select?wDrawColorPreviewSelected:wDrawColorPreviewUnselected );
		GetBoundingBox(ts, &hi, &lo);
		if (first) {
			origin = lo;
			max = hi;
			first = FALSE;
		} else {
			if (lo.x <origin.x) origin.x = lo.x;
			if (lo.y <origin.y) origin.y = lo.y;
			if (hi.x >max.x) max.x = hi.x;
			if (hi.y >max.y) max.y = hi.y;
		}
	}
	if (!first) {
		coOrd size;
		size.x = max.x-origin.x;
		size.y = max.y-origin.y;
		wPos_t w,h;
		w = (wPos_t)((size.x/mainD.scale)*mainD.dpi+0.5+10);
		h = (wPos_t)((size.y/mainD.scale)*mainD.dpi+0.5+10);
		wPos_t x, y;
		tempD.CoOrd2Pix(&tempD,origin,&x,&y);
		wDrawFilledRectangle(tempD.d, x-5, y-5, w, h, wDrawColorPowderedBlue, wDrawOptTemp|wDrawOptTransparent);
	}

}

static STATUS_T CallModify(wAction_t action,
		coOrd pos ) {
	int rc = CmdModify(action,pos);
	if (rc != C_CONTINUE)
		doingDouble = FALSE;
	return rc;
}

static STATUS_T CallDescribe(wAction_t action, coOrd pos) {
	int rc = CmdDescribe(action, pos);
	return rc;
}

static void CallPushDescribe(void * func) {
	if (moveDescTrk) {
		CallDescribe(C_START, moveDescPos);
		CallDescribe(C_DOWN, moveDescPos);
		CallDescribe(C_UP, moveDescPos);
	}
	return;
}

static STATUS_T CmdSelect(wAction_t,coOrd);

static void CallPushModify(void * func) {
	if (moveDescTrk) {
		CmdSelect(C_LDOUBLE, moveDescPos);
	}
	return;
}

static STATUS_T CmdSelect(
		wAction_t action,
		coOrd pos )
{

	static BOOL_T doingMove;
	static BOOL_T doingRotate;



	STATUS_T rc=C_CONTINUE;
	static track_p trk = NULL;
	typedef enum {NOSHOW,SHOWMOVE,SHOWROTATE,SHOWMODIFY,SHOWACTIVATE} showType;
	static showType showMode;

	mode = AREA;
	if (doingAlign || doingRotate || doingMove )
		mode = MOVE;
	else {
		if ( (action == C_DOWN) || (action == C_RDOWN) || ((action&0xFF) == wActionExtKey) ) {
			mode = AREA;
			if ( ((action&0xFF) == wActionExtKey) || (						//Moves don't need to be in a box
				 ( MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL|WKEY_ALT)) && IsInsideABox(pos)) )  //But cursors do
			{
				mode = MOVE;
			}
		}
	}


	switch (action&0xFF) {
	case C_START:
		InfoMessage( _("Select track") );
		doingMove = FALSE;
		doingRotate = FALSE;
		doingAlign = FALSE;
		doingDouble = FALSE;
		showMode = NOSHOW;
		SelectArea( action, pos );
		wMenuPushEnable( rotateAlignMI, FALSE );
		wSetCursor(mainD.d,defaultCursor);
		mode = AREA;
		trk = NULL;
		break;

	case wActionModKey:
	case wActionMove:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		showMode = NOSHOW;
		DYNARR_RESET(trkSeg_t,anchors_da);
		coOrd p = pos;
		trk = OnTrack( &p, FALSE, FALSE );
		track_p ht;
		if ((selectedTrackCount==0) && (trk == NULL)) return C_CONTINUE;
		if (trk && !CheckTrackLayerSilent( trk ) ) {
			if (GetLayerFrozen(GetTrkLayer(trk)) ) {
				trk = NULL;
				InfoMessage(_("Track is in Frozen Layer"));
				return C_CONTINUE;
			}
		}
		if (selectedTrackCount>0) {
			if ((ht = IsInsideABox(pos)) != NULL) {
				if ((MyGetKeyState()&WKEY_SHIFT)) {
					CreateMoveAnchor(pos);
					showMode = SHOWMOVE;
				} else if ((MyGetKeyState()&WKEY_CTRL)) {
					CreateRotateAnchor(pos);
					showMode = SHOWROTATE;
				} else if (!GetLayerModule(GetTrkLayer(ht))) {
					if (QueryTrack( ht, Q_CAN_MODIFY_CONTROL_POINTS ) ||
					QueryTrack( ht, Q_IS_CORNU ) ||
					(QueryTrack( ht, Q_IS_DRAW ) && !QueryTrack( ht, Q_IS_TEXT))) {
						CreateModifyAnchor(pos);
						showMode = SHOWMODIFY;
					} else {
						if (QueryTrack(ht,Q_IS_ACTIVATEABLE)) {
							CreateActivateAnchor(pos);
							showMode = SHOWACTIVATE;
						} else wSetCursor(mainD.d,defaultCursor);
					}
				} else wSetCursor(mainD.d,defaultCursor);
			} else wSetCursor(mainD.d,defaultCursor);
		}
		break;

	case C_DOWN:
	case C_RDOWN:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		DYNARR_RESET(trkSeg_t,anchors_da);
		switch (mode) {
		rc = C_CONTINUE;
		case MOVE:
			if (SelectedTracksAreFrozen() || (selectedTrackCount==0)) {
				rc = C_TERMINATE;
				doingMove = FALSE;
			} else if ((MyGetKeyState()&(WKEY_CTRL|WKEY_SHIFT))==WKEY_CTRL) {
				doingRotate = TRUE;
				doingMove = FALSE;
				RotateAlign( FALSE );
				rc = CmdRotate( action, pos );
			} else if ((MyGetKeyState()&(WKEY_SHIFT|WKEY_CTRL))==WKEY_SHIFT) {
				doingMove = TRUE;
				doingRotate = FALSE;
				rc = CmdMove( action, pos );
			}
			break;
		case AREA:
			doingMove = FALSE;
			doingRotate = FALSE;
			SelectArea( action, pos );
			break;
		default: ;
		}
		trk = NULL;
		return rc;
		break;
	case wActionExtKey:
	case C_RMOVE:
	case C_MOVE:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		if ((action&0xFF) == wActionExtKey && ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL)) == (WKEY_SHIFT|WKEY_CTRL))) { //Both + arrow
			doingMove = TRUE;
			mode = MOVE;
		}
		DYNARR_RESET(trkSeg_t,anchors_da);
		switch (mode) {
		case MOVE:
			if (SelectedTracksAreFrozen() || (selectedTrackCount==0)) {
				rc = C_TERMINATE;
				tlist_da.cnt = 0;
				doingMove = FALSE;
				doingRotate = FALSE;
			} else if (doingRotate == TRUE) {
				RotateAlign( FALSE );
				rc = CmdRotate( action, pos );
			} else if (doingMove == TRUE) {
				rc = CmdMove( action, pos );
			}
			break;
		case AREA:
			doingMove = FALSE;
			doingRotate = FALSE;
			SelectArea( action, pos );
			break;
		default: ;
		}
		if ((action&0xFF) == wActionExtKey && ((MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL)) == (WKEY_SHIFT|WKEY_CTRL)))  //Both
			doingMove = FALSE;
		return rc;
		break;
	case C_RUP:
	case C_UP:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		DYNARR_RESET(trkSeg_t,anchors_da);
		switch (mode) {
		case MOVE:
			if (SelectedTracksAreFrozen() || (selectedTrackCount==0)) {
				rc = C_TERMINATE;
				doingMove = FALSE;
				doingRotate = FALSE;
			} else if (doingRotate == TRUE) {
				RotateAlign( FALSE );
				rc = CmdRotate( action, pos );
			} else if (doingMove == TRUE) {
				rc = CmdMove( action, pos );
			}
			break;
		case AREA:
			doingMove = FALSE;
			doingRotate = FALSE;
			SelectArea( action, pos );
			rc = C_CONTINUE;
			break;
		default: ;
		}
		doingMove = FALSE;
		doingRotate = FALSE;
		mode = AREA;
		return rc;
		break;

	case C_REDRAW:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		if (doingMove) {
			rc = CmdMove( C_REDRAW, pos );
		} else if (doingRotate) {
			rc = CmdRotate( C_REDRAW, pos );
		}

		//Once doing a move or a rotate, make an early exit
		if (doingMove || doingRotate) {
			if (anchors_da.cnt) {
				DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			}
			return C_CONTINUE;
		}
		BOOL_T AreaSelect = FALSE;
		// Draw the selected area, no-op if none selected
		if (mode==AREA) {
	    	AreaSelect = SelectArea( action, pos );
	    	if (AreaSelect) return C_CONTINUE;
		}

		// Highlight a whole Module's worth of tracks if we are hovering over one
		if (trk && GetLayerModule(GetTrkLayer(trk))) {
			if ( (selectMode == 1) && ((MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) != WKEY_CTRL) )
				DoModuleTracks(GetTrkLayer(trk),DrawSingleTrack,!GetTrkSelected(trk));   //Toggle
			else
				DoModuleTracks(GetTrkLayer(trk),DrawSingleTrack,TRUE);
			DrawHighlightLayer(GetTrkLayer(trk));
		}

		//Draw all existing highlight boxes only
		DrawHighlightBoxes(FALSE, FALSE, trk);

		// If not on a track, show all tracks as going to be de-selected if selectZero on
		if (!trk && selectZero ) {
			HighlightSelectedTracks(NULL, TRUE, TRUE);
		//Handle the SHIFT+ which means SelectAllConnected case
		} else if ( trk && !IsTrackDeleted(trk)) {
			if ((MyGetKeyState() & WKEY_SHIFT) )
				SelectConnectedTracks(trk, TRUE);        	 //Highlight all connected
			//Normal case - handle track we are hovering over
			else  {
				//Select=Add
				if (selectMode == 1) {
					if  ((MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) == WKEY_CTRL) {
						//Only Highlight if adding
						if (!GetTrkSelected(trk))
							DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
					} else {
						if (GetTrkSelected(trk))
							DrawTrack(trk,&tempD,wDrawColorPreviewUnselected);           //Toggle
						else
							DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
					}
				//Select=Only
				} else {
					if  ((MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) == WKEY_CTRL) {
						if (GetTrkSelected(trk))
							DrawTrack(trk,&tempD,wDrawColorPreviewUnselected);           //Toggle
						else
							DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
					} else {
						//Only Highlight if adding
						if (!GetTrkSelected(trk))
							DrawTrack(trk,&tempD,wDrawColorPreviewSelected );
					}
				}
			}
			// Now Highlight the rest of the tracks or Module
			if (GetLayerModule(GetTrkLayer(trk))) {
				if (selectMode == 1 && ((MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) != WKEY_CTRL) )
					DoModuleTracks(GetTrkLayer(trk),DrawSingleTrack,!GetTrkSelected(trk));             //Toggle
				else
					DoModuleTracks(GetTrkLayer(trk),DrawSingleTrack,TRUE);
				DrawHighlightLayer(GetTrkLayer(trk));
			} else {
				//Select=Add
				if (selectMode == 1) {
					if (((MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) == WKEY_CTRL))
						HighlightSelectedTracks(trk, TRUE, TRUE);
					//else
					//	HighlightSelectedTracks(trk, TRUE, FALSE);  Highlight all selected
				//Select=Only
				} else {
					if (((MyGetKeyState() & (WKEY_CTRL|WKEY_SHIFT)) != WKEY_CTRL))
						HighlightSelectedTracks(trk, TRUE, TRUE);
					//else
					//	HighlightSelectedTracks(trk, TRUE, TRUE); Highlight all selected
				}
			}
		}
		//Finally add the anchors for any actions or snaps
		if (anchors_da.cnt) {
			DrawSegs( &tempD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		}

		return rc;

	case C_LCLICK:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		switch (mode) {
		case MOVE:
		case AREA:
			if (doingAlign) {
				rc = CmdRotate (C_DOWN, pos);
				rc = CmdRotate (C_UP, pos);
			} else
				rc = SelectTrack( pos );
			doingRotate = FALSE;
			doingMove = FALSE;
			return rc;
		}
		mode = AREA;
		break;

	case C_LDOUBLE:
		if (doingDouble) {
			return C_CONTINUE;
		}
		switch (mode) {
			case AREA:
				if ((ht = OnTrack(&pos,FALSE,FALSE))!=NULL) {
					if (QueryTrack( ht, Q_CAN_MODIFY_CONTROL_POINTS ) ||
						QueryTrack( ht, Q_IS_CORNU ) ||
						(QueryTrack( ht, Q_IS_DRAW ) && !QueryTrack( ht, Q_IS_TEXT ))) {
						doingDouble = TRUE;
						CallModify(C_START,pos);
						if (doingDouble == FALSE) return C_CONTINUE;
						CallModify(C_LDOUBLE,pos);
					} else if (QueryTrack( ht, Q_IS_ACTIVATEABLE)){
						return Activate(pos);
					}
				}
				break;
			case MOVE:
			default:
				break;
		}
		break;

	case C_CMDMENU:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		menuPos = pos;
		if (selectedTrackCount <= 0) {
			wMenuPopupShow( selectPopup1M );
		} else {
		    track_p trk = OnTrack(&pos, FALSE, FALSE);  //Note pollutes pos if turntable
			SetUpMenu2(pos,trk);
			wMenuPopupShow( selectPopup2M );
		}
		return C_CONTINUE;
	case C_TEXT:
		if (doingDouble) {
			return CallModify(action,pos);
		}
		if ((action>>8) == 'c') {
			panCenter = pos;
			LOG( log_pan, 2, ( "PanCenter:Sel-%d %0.3f %0.3f\n", __LINE__, panCenter.x, panCenter.y ) );
			PanHere((void*)0);
		}
		if ((action>>8) == 'e') {
			DoZoomExtents(0);
		}
		if ((action>>8) == '0' || (action>>8 == 'o')) {
			PanMenuEnter('o');
		}
		if ((action>>8) == '?') {
			if((moveDescTrk = OnTrack(&pos,FALSE,FALSE)) != NULL)
				moveDescPos = pos;
				CallPushDescribe((void*)0);
			wSetCursor(mainD.d,defaultCursor);
			moveDescTrk = NULL;
		}
		break;
	case C_FINISH:
		if (doingMove) UndoEnd();
		doingDouble = FALSE;
		wSetCursor(mainD.d,defaultCursor);
		break;
	default:
		if (doingDouble) return CallModify(action, pos);
	}

	return C_CONTINUE;
}


#include "bitmaps/select.xpm"
#include "bitmaps/delete.xpm"
#include "bitmaps/tunnel.xpm"
#include "bitmaps/bridge.xpm"
#include "bitmaps/move.xpm"
#include "bitmaps/rotate.xpm"
#include "bitmaps/flip.xpm"
#include "bitmaps/movedesc.xpm"


static void SetMoveMode( char * line )
{
	long tmp = atol( line );
	moveMode = tmp & 0x0F;
	if (moveMode < 0 || moveMode > MAXMOVEMODE)
		moveMode = MAXMOVEMODE;
	enableMoveDraw = ((tmp&0x10) == 0);
}

static void moveDescription( void ) {
	if (!moveDescTrk) return;
	int hidden = GetTrkBits( moveDescTrk) &TB_HIDEDESC ;
	if (hidden)
		ClrTrkBits( moveDescTrk, TB_HIDEDESC );
	else
		SetTrkBits( moveDescTrk, TB_HIDEDESC );
	MainRedraw();
}


EXPORT void InitCmdSelect( wMenu_p menu )
{
	selectCmdInx = AddMenuButton( menu, CmdSelect, "cmdSelect", _("Select"), wIconCreatePixMap(select_xpm),
				LEVEL0, IC_CANCEL|IC_POPUP|IC_LCLICK|IC_CMDMENU|IC_WANT_MOVE|IC_WANT_MODKEYS, ACCL_SELECT, NULL );
}

extern wIndex_t trainCmdInx;

EXPORT void InitCmdSelect2( wMenu_p menu ) {


	endpt_bm = wDrawBitMapCreate( mainD.d, bmendpt_width, bmendpt_width, 7, 7, bmendpt_bits );
	angle_bm[0] = wDrawBitMapCreate( mainD.d, bma90_width, bma90_width, 7, 7, bma90_bits );
	angle_bm[1] = wDrawBitMapCreate( mainD.d, bma135_width, bma135_width, 7, 7, bma135_bits );
	angle_bm[2] = wDrawBitMapCreate( mainD.d, bma0_width, bma0_width, 7, 7, bma0_bits );
	angle_bm[3] = wDrawBitMapCreate( mainD.d, bma45_width, bma45_width, 7, 7, bma45_bits );
	AddPlaybackProc( SETMOVEMODE, (playbackProc_p)SetMoveMode, NULL );
	wPrefGetInteger( "draw", "movemode", &moveMode, MAXMOVEMODE );
	if (moveMode > MAXMOVEMODE || moveMode < 0)
		moveMode = MAXMOVEMODE;
	selectPopup1M = MenuRegister( "Select Mode Menu" );
	wMenuPushCreate(selectPopup1M, "", _("Undo"), 0,(wMenuCallBack_p) UndoUndo, (void *) 0);
	wMenuPushCreate(selectPopup1M, "", _("Redo"), 0,(wMenuCallBack_p) UndoRedo, (void *) 0);
	wMenuSeparatorCreate( selectPopup1M );
	wMenuPushCreate(selectPopup1M, "cmdDescribeMode", GetBalloonHelpStr("cmdModifyMode"), 0, DoCommandB, (void*) (intptr_t) modifyCmdInx);
	wMenuPushCreate(selectPopup1M, "cmdPanMode", GetBalloonHelpStr("cmdPanMode"), 0, DoCommandB, (void*) (intptr_t) panCmdInx);
	wMenuPushCreate(selectPopup1M, "cmdTrainMode", GetBalloonHelpStr("cmdTrainMode"), 0, DoCommandB, (void*) (intptr_t) trainCmdInx);
	wMenuSeparatorCreate( selectPopup1M );
	wMenuPushCreate(selectPopup1M, "", _("Zoom In"), 0,(wMenuCallBack_p) DoZoomUp, (void*) 1);
	wMenuPushCreate( selectPopup1M, "", _("Zoom to extents - 'e'"), 0, (wMenuCallBack_p)DoZoomExtents, (void*) 0);
	wMenu_p zoomPop1 = wMenuMenuCreate(selectPopup1M, "", _("&Zoom"));
	InitCmdZoom(NULL, NULL, zoomPop1, NULL);
	wMenuPushCreate(selectPopup1M, "", _("Zoom Out"), 0,	(wMenuCallBack_p) DoZoomDown, (void*) 1);
	wMenuPushCreate(selectPopup1M, "", _("Pan to Origin - 'o'/'0'"), 0,	(wMenuCallBack_p) PanMenuEnter, (void*) 'o');
	wMenuPushCreate(selectPopup1M, "", _("Pan Center Here - 'c'"), 0,	(wMenuCallBack_p) PanHere, (void*) 3);
	wMenuSeparatorCreate( selectPopup1M );
	wMenuPushCreate(selectPopup1M, "", _("Select All"), 0,(wMenuCallBack_p) SetAllTrackSelect, (void *) 1);
	wMenuPushCreate(selectPopup1M, "",_("Select Current Layer"), 0,(wMenuCallBack_p) SelectCurrentLayer, (void *) 0);
	wMenuSeparatorCreate( selectPopup1M );

	selectPopup2M = MenuRegister( "Track Selected Menu " );
	wMenuPushCreate(selectPopup2M, "", _("Undo"), 0,(wMenuCallBack_p) UndoUndo, (void *) 0);
	wMenuPushCreate(selectPopup2M, "", _("Redo"), 0,(wMenuCallBack_p) UndoRedo, (void *) 0);
	wMenuSeparatorCreate( selectPopup2M );
	wMenuPushCreate(selectPopup2M, "", _("Zoom In"), 0,(wMenuCallBack_p) DoZoomUp, (void*) 1);
	wMenuPushCreate(selectPopup2M, "", _("Zoom Out"), 0,	(wMenuCallBack_p) DoZoomDown, (void*) 1);
	wMenuPushCreate(selectPopup2M, "", _("Pan Center Here - 'c'"), 0,	(wMenuCallBack_p) PanHere, (void*) 3);
	wMenuSeparatorCreate( selectPopup2M );
	wMenuPushCreate(selectPopup2M, "", _("Deselect All"), 0, (wMenuCallBack_p) SetAllTrackSelect, (void *) 0);
	wMenuSeparatorCreate( selectPopup2M );
	wMenuPushCreate(selectPopup2M, "", _("Properties -'?'"), 0,(wMenuCallBack_p) CallPushDescribe, (void*)0);
	menuPushModify = wMenuPushCreate(selectPopup2M, "", _("Modify/Activate Track"), 0,(wMenuCallBack_p) CallPushModify, (void*)0);
	wMenuSeparatorCreate( selectPopup2M );
	wMenuPushCreate(selectPopup2M, "", _("Cut"), 0,(wMenuCallBack_p) EditCut, (void *) 0);
	wMenuPushCreate(selectPopup2M, "", _("Copy"), 0,(wMenuCallBack_p) EditCopy, (void *) 0);
	wMenuPushCreate(selectPopup2M,  "", _("Paste"), 0, (wMenuCallBack_p) EditPaste, (void *) 0);
	wMenuPushCreate(selectPopup2M,  "", _("Clone"), 0, (wMenuCallBack_p) EditClone, (void *) 0);
	AddMoveMenu( selectPopup2M, QuickMove);
	selectPopup2RM = wMenuMenuCreate(selectPopup2M, "", _("Rotate..."));
	AddRotateMenu( selectPopup2RM, QuickRotate );
	rotateAlignMI = wMenuPushCreate( selectPopup2RM, "", _("Align"), 0, (wMenuCallBack_p)RotateAlign, (void* ) 1 );
	wMenuSeparatorCreate( selectPopup2M );
	descriptionMI = wMenuPushCreate(selectPopup2M, "cmdMoveLabel", _("Show/Hide Description"), 0, (wMenuCallBack_p)moveDescription, (void*) 0);
	wMenuSeparatorCreate( selectPopup2M );
	hideMI = wMenuPushCreate(selectPopup2M, "", _("Hide/NoHide"), 0,(wMenuCallBack_p) SelectTunnel, (void *) 0);
	bridgeMI = wMenuPushCreate(selectPopup2M, "", _("Bridge/NoBridge"), 0,(wMenuCallBack_p) SelectBridge, (void *) 0);
	tiesMI = wMenuPushCreate(selectPopup2M, "", _("NoTies/Ties"), 0,(wMenuCallBack_p) SelectTies, (void *) 0);
	selectPopup2TM = wMenuMenuCreate(selectPopup2M, "", _("Thickness..."));
	wMenuPushCreate( selectPopup2TM, "", _("Thin Tracks"), 0, (void*)(wMenuCallBack_p)SelectTrackWidth, (void *)0 );
	wMenuPushCreate( selectPopup2TM, "", _("Medium Tracks"), 0, (void*)(wMenuCallBack_p)SelectTrackWidth, (void *)2 );
	wMenuPushCreate( selectPopup2TM, "", _("Thick Tracks"), 0, (void*)(wMenuCallBack_p)SelectTrackWidth, (void *)3 );
	selectPopup2TYM = wMenuMenuCreate( selectPopup2M, "", _("LineType...") );
	wMenuPushCreate( selectPopup2TYM, "", _("Solid Line"), 0, (wMenuCallBack_p)SelectLineType, (void*)0 );
	wMenuPushCreate( selectPopup2TYM, "", _("Dashed Line"), 0, (wMenuCallBack_p)SelectLineType, (void*)1 );
	wMenuPushCreate( selectPopup2TYM, "", _("Dotted Line"), 0, (wMenuCallBack_p)SelectLineType, (void*)2 );
	wMenuPushCreate( selectPopup2TYM, "", _("Dash-Dotted Line"), 0, (wMenuCallBack_p)SelectLineType, (void*)3 );
	wMenuPushCreate( selectPopup2TYM, "", _("Dash-Dot-Dotted Line"), 0, (wMenuCallBack_p)SelectLineType, (void*)4 );
	wMenuSeparatorCreate( selectPopup2M );
	wMenuPushCreate(selectPopup2M, "", _("Move To Front"), 0,(wMenuCallBack_p) SelectAbove,(void *) 0);
	wMenuPushCreate(selectPopup2M, "", _("Move To Back"), 0,(wMenuCallBack_p) SelectBelow, (void *) 0);
	wMenuSeparatorCreate( selectPopup2M );
	wMenuPushCreate(selectPopup2M, "", _("Group"), 0,(wMenuCallBack_p) DoGroup, (void *) 0);
	wMenuPushCreate(selectPopup2M, "", _("UnGroup"), 0,(wMenuCallBack_p) DoUngroup, (void *) 0);
	wMenuSeparatorCreate( selectPopup2M );

	ParamRegister( &rescalePG );
}



EXPORT void InitCmdDelete( void )
{
	wIcon_p icon;
	icon = wIconCreatePixMap( delete_xpm );
	AddToolbarButton( "cmdDelete", icon, IC_SELECTED, (wButtonCallBack_p)SelectDelete, 0 );
#ifdef WINDOWS
	wAttachAccelKey( wAccelKey_Del, 0, (wAccelKeyCallBack_p)SelectDelete, NULL );
#endif
}

EXPORT void InitCmdTunnel( void )
{
	wIcon_p icon;
	icon = wIconCreatePixMap( tunnel_xpm );
	AddToolbarButton( "cmdTunnel", icon, IC_SELECTED|IC_POPUP, (addButtonCallBack_t)SelectTunnel, NULL );
}

EXPORT void InitCmdBridge( void)
{
	wIcon_p icon;
	icon = wIconCreatePixMap( bridge_xpm );
	AddToolbarButton( "cmdBridge", icon, IC_SELECTED|IC_POPUP, (addButtonCallBack_t)SelectBridge, NULL );
}


EXPORT void InitCmdMoveDescription( wMenu_p menu )
{
	AddMenuButton( menu, CmdMoveDescription, "cmdMoveLabel", _("Move Description"), wIconCreatePixMap(movedesc_xpm),
				LEVEL0, IC_STICKY|IC_POPUP3|IC_CMDMENU|IC_WANT_MOVE, ACCL_MOVEDESC, (void*) 0 );
}


EXPORT void InitCmdMove( wMenu_p menu )
{
	moveCmdInx = AddMenuButton( menu, CmdMove, "cmdMove", _("Move"), wIconCreatePixMap(move_xpm),
				LEVEL0, IC_STICKY|IC_SELECTED|IC_CMDMENU|IC_WANT_MOVE, ACCL_MOVE, NULL );
	rotateCmdInx = AddMenuButton( menu, CmdRotate, "cmdRotate", _("Rotate"), wIconCreatePixMap(rotate_xpm),
				LEVEL0, IC_STICKY|IC_SELECTED|IC_CMDMENU|IC_WANT_MOVE, ACCL_ROTATE, NULL );
	/*flipCmdInx =*/ AddMenuButton( menu, CmdFlip, "cmdFlip", _("Flip"), wIconCreatePixMap(flip_xpm),
				LEVEL0, IC_STICKY|IC_SELECTED|IC_CMDMENU, ACCL_FLIP, NULL );
}
