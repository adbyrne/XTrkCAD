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
#include "draw.h"


#include "bitmaps/bmendpt.xbm"
#include "bitmaps/bma0.xbm"
#include "bitmaps/bma45.xbm"
#include "bitmaps/bma90.xbm"
#include "bitmaps/bma135.xbm"

#define SETMOVEMODE "MOVEMODE"

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

 long quickMove = 0;
 BOOL_T importMove = 0;
 int incrementalDrawLimit = 20;
 static int microCount = 0;

static dynArr_t tlist_da;

#define Tlist(N) DYNARR_N( track_p, tlist_da, N )
#define TlistAppend( T ) \
		{ DYNARR_APPEND( track_p, tlist_da, 10 );\
		  Tlist(tlist_da.cnt-1) = T; }
static track_p *tlist2 = NULL;

static wMenu_p selectPopup1M;
static wMenu_p selectPopup2M;

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
}

void static CreateRotateAnchor(coOrd pos) {
	DIST_T d = tempD.scale*0.15;
	DYNARR_APPEND(trkSeg_t,anchors_da,1);
	int i = anchors_da.cnt-1;
	anchors(i).type = SEG_CRVLIN;
	anchors(i).width = 0.5;
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
}

void static CreateMoveAnchor(coOrd pos) {
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,0,TRUE,wDrawColorBlue);
	DYNARR_SET(trkSeg_t,anchors_da,anchors_da.cnt+5);
	DrawArrowHeads(&DYNARR_N(trkSeg_t,anchors_da,anchors_da.cnt-5),pos,90,TRUE,wDrawColorBlue);
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
				if (!doRedraw)
					DrawTrackAndEndPts( trk, wDrawColorWhite );
				if (select)
					SetTrkBits( trk, TB_SELECTED );
				else
					ClrTrkBits( trk, TB_SELECTED );
				if (!doRedraw)
					DrawTrackAndEndPts( trk, wDrawColorBlack );
			}
		}
	}
	SelectedTrackCountChange();
	if (doRedraw) {
		MainRedraw();
		MapRedraw();
	} else {
		wDrawDelayUpdate( mainD.d, FALSE );
	}
}

/* Invert selected state of all visible objects.
 *
 * \param none
 * \return none
 */
 
EXPORT void InvertTrackSelect( void *ptr )
{
	track_p trk;

	trk = NULL;
	while ( TrackIterate( &trk ) ) {
		if (GetLayerVisible( GetTrkLayer( trk ))) {
			if (GetTrkSelected(trk))
			{
				ClrTrkBits( trk, TB_SELECTED );
				selectedTrackCount--;
			}	
			else	
				SetTrkBits( trk, TB_SELECTED );
				selectedTrackCount++;
		}
	}
	
	SelectedTrackCountChange();
	MainRedraw();
	MapRedraw();
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
		if( GetLayerVisible( GetTrkLayer( trk ))) {
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
	SelectedTrackCountChange();
	MainRedraw();
	MapRedraw();
}


static void SelectOneTrack(
		track_p trk,
		wBool_t selected )
{
		DrawTrackAndEndPts( trk, wDrawColorWhite );
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

static void SelectConnectedTracks(
		track_p trk )
{
	track_p trk1;
	int inx;
	EPINX_T ep;
	tlist_da.cnt = 0;
	TlistAppend( trk );
	InfoCount( 0 );
	wDrawDelayUpdate( mainD.d, FALSE );
	for (inx=0; inx<tlist_da.cnt; inx++) {
		if ( inx > 0 && selectedTrackCount == 0 )
			return;
		trk = Tlist(inx);
		if (inx!=0 && 
			GetTrkSelected(trk))
			continue;
		for (ep=0; ep<GetTrkEndPtCnt(trk); ep++) {
			trk1 = GetTrkEndTrk( trk, ep );
			if (trk1 && (!GetTrkSelected(trk1)) && GetLayerVisible( GetTrkLayer( trk1 )) ) {
				TlistAppend( trk1 )
			}
		}
		if (!GetTrkSelected(trk)) {
			SelectOneTrack( trk, TRUE );
			InfoCount( inx+1 );
		}
		SetTrkBits(trk, TB_SELECTED);
	}
	wDrawDelayUpdate( mainD.d, TRUE );	
	wFlush();	
	InfoCount( trackCount );
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


EXPORT void SelectDelete( void )
{
	if (SelectedTracksAreFrozen())
		return;
	if (selectedTrackCount>0) {
		UndoStart( _("Delete Tracks"), "delete" );
		wDrawDelayUpdate( mainD.d, TRUE );
		wDrawDelayUpdate( mapD.d, TRUE );
		DoSelectedTracks( DeleteTrack );
		MainRedraw();
		MapRedraw();
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
		ClrTrkBits( trk, TB_VISIBLE|(drawTunnel==0?TB_SELECTED:0) );
	} else {
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
		MainRedraw();
		MapRedraw();
	} else {
		ErrorMessage( MSG_NO_SELECTED_TRK );
	}
}


static drawCmd_t tempSegsD = {
		NULL, &tempSegDrawFuncs, DC_GROUP, 1, 0.0, {0.0, 0.0}, {0.0, 0.0}, Pix2CoOrd, CoOrd2Pix };
EXPORT void WriteSelectedTracksToTempSegs( void )
{
	track_p trk;
	long oldOptions;
	DYNARR_RESET( trkSeg_t, tempSegs_da );
	tempSegsD.dpi = mainD.dpi;
	oldOptions = tempSegDrawFuncs.options;
	tempSegDrawFuncs.options = wDrawOptTemp;
	for ( trk=NULL; TrackIterate(&trk); ) {
		if ( GetTrkSelected( trk ) ) {
			if ( IsTrack( trk ) )
				continue;
			ClrTrkBits( trk, TB_SELECTED );
			DrawTrack( trk, &tempSegsD, wDrawColorBlack );
			SetTrkBits( trk, TB_SELECTED );
		}
	}
	tempSegDrawFuncs.options = oldOptions;
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


#define MOVE_NORMAL		(0)
#define MOVE_FAST		(1)
#define MOVE_QUICK		(2)
static char *quickMoveMsgs[] = {
		N_("Draw moving track normally"),
		N_("Draw moving track simply"),
		N_("Draw moving track as end-points") };
static wMenuToggle_p quickMove1M[3];
static wMenuToggle_p quickMove2M[3];

static void ChangeQuickMove( wBool_t set, void * mode )
{
	long inx;
	quickMove = (long)mode;
	InfoMessage( quickMoveMsgs[quickMove] );
	DoChangeNotification( CHANGE_CMDOPT );
	for (inx = 0; inx<3; inx++) {
		wMenuToggleSet( quickMove1M[inx], quickMove == inx );
		wMenuToggleSet( quickMove2M[inx], quickMove == inx );
	}
}

EXPORT void UpdateQuickMove( void * junk )
{
	long inx;
	for (inx = 0; inx<3; inx++) {
		wMenuToggleSet( quickMove1M[inx], quickMove == inx );
		wMenuToggleSet( quickMove2M[inx], quickMove == inx );
	}
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
		DrawTrack( trk, d, color );
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

static coOrd moveOrig;
static ANGLE_T moveAngle;

static coOrd moveD_hi, moveD_lo;

static drawCmd_t moveD = {
		NULL, &tempDrawFuncs, DC_SIMPLE, 1, 0.0, {0.0, 0.0}, {0.0, 0.0}, Pix2CoOrd, CoOrd2Pix };




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
		if (quickMove == MOVE_FAST)
			moveD.options |= DC_QUICK;
		for ( inx = 0; inx<tlist_da.cnt; inx++ ) {
			trk = tlist2[inx];
			if (trk) {
				GetBoundingBox( trk, &hi, &lo );
				if (lo.x <= moveD_hi.x && hi.x >= moveD_lo.x &&
					lo.y <= moveD_hi.y && hi.y >= moveD_lo.y ) {
					if (quickMove != MOVE_QUICK)
#if defined(WINDOWS) && ! defined(WIN32)
						if ( tempSegs_da.cnt+100 > 65500 / sizeof(*(trkSeg_p)NULL) ) {
							ErrorMessage( MSG_TOO_MANY_SEL_TRKS );

							quickMove = MOVE_QUICK;
						} else
#endif
						if (!QueryTrack(trk,Q_IS_CORNU))
							DrawTrack( trk, &moveD, wDrawColorBlack );
					}
					tlist2[inx] = NULL;
					movedCnt++;
				}
		}
		moveD.options &= ~DC_QUICK;

	InfoCount( movedCnt );
	/*wDrawDelayUpdate( moveD.d, FALSE );*/
}

static void AddEndCornus() {
	for (int i=0;i<tlist_da.cnt;i++) {
		track_p trk = DYNARR_N(track_p,tlist_da,i);
		track_p tc;
		for (int j=GetTrkEndPtCnt(trk)-1;j>=0;j--) {
			tc = GetTrkEndTrk(trk,j);
			if (tc && !GetTrkSelected(tc) && QueryTrack(tc,Q_IS_CORNU) && !QueryTrack(trk,Q_IS_CORNU)) {  //On end and cornu
				SetTrkBits(tc,TB_SELECTED);
				DYNARR_APPEND(track_p,tlist_da,1);	//Add to selected list
				DYNARR_LAST(track_p,tlist_da) = tc;
			}
		}
	}
}


static void GetMovedTracks( BOOL_T undraw )
{
	wSetCursor( mainD.d, wCursorWait );
	DYNARR_RESET( track_p, tlist_da );
	DoSelectedTracks( AddSelectedTrack );
	AddEndCornus();							//Include Cornus that are attached at ends of selected
	tlist2 = (track_p*)MyRealloc( tlist2, (tlist_da.cnt+1) * sizeof *(track_p*)0 );
	if (tlist_da.ptr)
		memcpy( tlist2, tlist_da.ptr, (tlist_da.cnt) * sizeof *(track_p*)0 );
	tlist2[tlist_da.cnt] = NULL;
	DYNARR_RESET( trkSeg_p, tempSegs_da );
	moveD = mainD;
	moveD.funcs = &tempSegDrawFuncs;
	moveD.options = DC_SIMPLE;
	tempSegDrawFuncs.options = wDrawOptTemp;
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
		/*DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt,
						trackGauge, wDrawColorBlack );*/
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
	track_p other;
	EPINX_T i;
	coOrd pos;
	wDrawBitMap_p bm;
	ANGLE_T a;
	int ia;
	dynArr_t cornu_segs;

	if ( quickMove != MOVE_QUICK) {
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
						if (GetTrkEndTrk(trk,i) && GetTrkSelected(GetTrkEndTrk(trk,i))) {
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

					DrawSegs(&tempD, zero, 0.0, cornu_p,cornu_segs.cnt,
							0.0, wDrawColorBlack );
				}

			}

		}
		return;
	}
	for ( inx=0; inx<tlist_da.cnt; inx++ ) {
		trk = Tlist(inx);
		if (tlist2[inx] != NULL)
			continue;
		for (i=GetTrkEndPtCnt(trk)-1; i>=0; i--) {
			pos = GetTrkEndPos(trk,i);
			if (!move0B) {
				Rotate( &pos, zero, moveAngle );
			}
			pos.x += moveOrig.x;
			pos.y += moveOrig.y;
			if ((other=GetTrkEndTrk(trk,i)) == NULL ||
				!GetTrkSelected(other)) {
				bm = endpt_bm;
			} else if (other != NULL && GetTrkIndex(trk) < GetTrkIndex(other)) {
				a = GetTrkEndAngle(trk,i)+22.5;
				if (!move0B)
					a += moveAngle;
				a = NormalizeAngle( a );
				if (a>=180.0)
					a -= 180.0;
				ia = (int)(a/45.0);
				bm = angle_bm[ia];
			} else {
				continue;
			}
			if ( !OFF_MAIND( pos, pos ) )
				DrawBitMap( &tempD, pos, bm, selectedColor );
		}
	}
}



static void MoveTracks(
		BOOL_T eraseFirst,
		BOOL_T move,
		BOOL_T rotate,
		coOrd base,
		coOrd orig,
		ANGLE_T angle )
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
		DrawMapBoundingBox( FALSE );
		if (eraseFirst)
			DrawSelectedTracksD( &mainD, wDrawColorWhite );
		DrawSelectedTracksD( &mapD, wDrawColorWhite );
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
							MapRedraw();
							MainRedraw();
							return;
						}
					}
				}
			}
	    }

		InfoCount( inx );
#ifdef LATER
		if (tlist_da.cnt <= incrementalDrawLimit)
			DrawNewTrack( trk );
#endif
	}
	if (tlist_da.cnt > incrementalDrawLimit) {
		DoRedraw();
	} else {
		DrawSelectedTracksD( &mainD, wDrawColorBlack );
		DrawSelectedTracksD( &mapD, wDrawColorBlack );
		DrawMapBoundingBox( TRUE );
	}
	wSetCursor( mainD.d, defaultCursor );
	UndoEnd();
	tempSegDrawFuncs.options = 0;
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
		MoveTracks( TRUE, TRUE, TRUE, base, orig, angle );
		UndrawNewTrack( trk0 );
		UndrawNewTrack( trk1 );
		ConnectTracks( trk0, ep0, trk1, ep1 );
		DrawNewTrack( trk0 );
		DrawNewTrack( trk1 );
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

	switch( action&0xFF) {

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
			break;

		case wActionMove:
			DYNARR_RESET(trkSeg_t,anchors_da);
			CreateMoveAnchor(pos);
			if (anchors_da.cnt)
						DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			break;
		case C_DOWN:
			DYNARR_RESET(trkSeg_t,anchors_da);
			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			UndoStart( _("Move Tracks"), "move" );
			base = zero;
			orig = pos;
			GetMovedTracks(quickMove != MOVE_QUICK);
			SetMoveD( TRUE, base, 0.0 );
			//DrawMovedTracks();
			drawCount = 0;
			state = 1;
            MainRedraw();
            MapRedraw();
			return C_CONTINUE;
		case C_MOVE:
			DYNARR_RESET(trkSeg_t,anchors_da);
			ep1=-1;
			ep2=-1;
			drawEnable = enableMoveDraw;
			DrawMovedTracks();
			base.x = pos.x - orig.x;
			base.y = pos.y - orig.y;
			SnapPos( &base );
			SetMoveD( TRUE, base, 0.0 );

			//

			if (FindEndIntersection(base,zero,0.0,&t1,&ep1,&t2,&ep2)) {
				coOrd pos2 = GetTrkEndPos(t2,ep2);
				pos2.x +=base.x;
				pos2.y +=base.y;
				CreateEndAnchor(pos2,FALSE);
				CreateEndAnchor(GetTrkEndPos(t1,ep1),TRUE);
			}
			//DrawMovedTracks();
#ifdef DRAWCOUNT
			InfoMessage( "   [%s %s] #%ld", FormatDistance(base.x), FormatDistance(base.y), drawCount );
#else
			InfoMessage( "   [%s %s]", FormatDistance(base.x), FormatDistance(base.y) );
#endif
			drawEnable = TRUE;
            MainRedraw();
            MapRedraw();
			return C_CONTINUE;
		case C_UP:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 0;
			//DrawMovedTracks();
			FreeTempStrings();
			if (t1 && ep1>=0 && t2 && ep2>=0) {
				MoveToJoin(t2,ep2,t1,ep1);
			} else {
				MoveTracks( quickMove==MOVE_QUICK, TRUE, FALSE, base, zero, 0.0 );
			}
			ep1 = -1;
			ep2 = -1;
			return C_TERMINATE;

		case C_CMDMENU:
			wMenuPopupShow( selectPopup1M );
			return C_CONTINUE;

		case C_REDRAW:
			/* DO_REDRAW */
			if (anchors_da.cnt)
				DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			if ( state == 0 )
				break;
			DrawSelectedTracksD( &mainD, wDrawColorWhite );
			DrawMovedTracks();


			break;

		case wActionExtKey:
			if (state) return C_CONTINUE;
			if (SelectedTracksAreFrozen()) return C_TERMINATE;
			if ((MyGetKeyState() &
					(WKEY_SHIFT | WKEY_CTRL)) == (WKEY_SHIFT | WKEY_CTRL)) {
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
			GetMovedTracks(quickMove!=MOVE_QUICK);
			UndoStart( _("Move Tracks"), "move" );
			SetMoveD( TRUE, base, 0.0 );
			DrawSelectedTracksD( &mainD, wDrawColorWhite );
			MoveTracks( quickMove==MOVE_QUICK, TRUE, FALSE, base, zero, 0.0 );
			++microCount;
			if (microCount>5) {
				microCount = 0;
				MainRedraw();
				MapRedraw();
			}
			return C_CONTINUE;
			}
			break;

		default:
			break;
	}
	return C_CONTINUE;
}


wMenuPush_p rotateAlignMI;
wMenuPush_p descriptionMI;
static int rotateAlignState = 0;

static void RotateAlign( BOOL_T align )
{
	rotateAlignState = 0;
	if (align) {
		rotateAlignState = 1;
		InfoMessage( _("Click on selected object to align") );
	}
}

static STATUS_T CmdRotate(
		wAction_t action,
		coOrd pos )
{
	static coOrd base;
	static coOrd orig;
	static ANGLE_T angle;
	static BOOL_T drawnAngle;
	static ANGLE_T baseAngle;
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
			if (anchors_da.cnt)
					DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			break;
		case C_DOWN:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 1;
			if (SelectedTracksAreFrozen()) {
				return C_TERMINATE;
			}
			UndoStart( _("Rotate Tracks"), "rotate" );
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
				GetMovedTracks(FALSE);
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
					//DrawMovedTracks();
				}
			}
            MainRedraw();
            MapRedraw();
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
				//DrawMovedTracks();
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
				//DrawMovedTracks();
                MainRedraw();
                MapRedraw();
				return C_CONTINUE;
			}
			if ( FindDistance( orig, pos ) > (6.0/75.0)*mainD.scale ) {
				drawEnable = enableMoveDraw;
				if (drawnAngle) {
					DrawLine( &tempD, base, orig, 0, wDrawColorBlack );
					DrawMovedTracks();
				} else if (quickMove != MOVE_QUICK) {
					DrawSelectedTracksD( &mainD, wDrawColorWhite );
				}
				angle = FindAngle( orig, pos );
				if (!drawnAngle) {
					baseAngle = angle;
					drawnAngle = TRUE;
				}
				base = pos;
				angle = NormalizeAngle( angle-baseAngle );
				if ( MyGetKeyState()&WKEY_SHIFT ) {
					angle = NormalizeAngle(floor((angle+7.5)/15.0)*15.0);
					Translate( &base, orig, angle, FindDistance(orig,pos) );
				}
				DrawLine( &tempD, base, orig, 0, wDrawColorBlack );
				SetMoveD( FALSE, orig, angle );
				if (FindEndIntersection(zero,orig,angle,&t1,&ep1,&t2,&ep2)) {
					coOrd pos2 = GetTrkEndPos(t2,ep2);
					coOrd pos1 = GetTrkEndPos(t1,ep1);
					Rotate(&pos2,orig,angle);
					CreateEndAnchor(pos2,FALSE);
					CreateEndAnchor(pos1,TRUE);
				}
				//DrawMovedTracks();
#ifdef DRAWCOUNT
				InfoMessage( _("   Angle %0.3f #%ld"), angle, drawCount );
#else
				InfoMessage( _("   Angle %0.3f"), angle );
#endif
				wFlush();
				drawEnable = TRUE;
			}
            MainRedraw();
            MapRedraw();
			return C_CONTINUE;
		case C_UP:
			DYNARR_RESET(trkSeg_t,anchors_da);
			state = 0;
			if (t1 && ep1>=0 && t2 && ep2>=0) {
				MoveToJoin(t2,ep2,t1,ep1);
				CleanSegs(&tempSegs_da);
			} else {
				if ( rotateAlignState == 1 ) {
					if ( trk && GetTrkSelected(trk) ) {
						InfoMessage( _("Click on the 2nd Unselected object") );
						rotateAlignState = 2;
					}
					return C_CONTINUE;
				}
				CleanSegs(&tempSegs_da);
				if ( rotateAlignState == 2 ) {
					//DrawMovedTracks();
					MoveTracks( quickMove==MOVE_QUICK, FALSE, TRUE, zero, orig, angle );
					rotateAlignState = 0;
				} else if (drawnAngle) {
					DrawLine( &tempD, base, orig, 0, wDrawColorBlack );
					//DrawMovedTracks();
					MoveTracks( quickMove==MOVE_QUICK, FALSE, TRUE, zero, orig, angle );
				}
			}
			UndoEnd();
            MainRedraw();
            MapRedraw();
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
			wMenuPopupShow( selectPopup2M );
			return C_CONTINUE;

		case C_REDRAW:
			if (anchors_da.cnt)
				DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
			/* DO_REDRAW */
			if ( state == 0 )
				break;
			if ( rotateAlignState != 2 )
				DrawLine( &tempD, base, orig, 0, wDrawColorBlack );
			DrawSelectedTracksD( &mainD, wDrawColorWhite );
			DrawMovedTracks();
			break;

		}
	return C_CONTINUE;
}

static void QuickMove( void* pos) {
	coOrd move_pos = *(coOrd*)pos;
	if ( SelectedTracksAreFrozen() )
		return;
	wDrawDelayUpdate( mainD.d, TRUE );
	GetMovedTracks(FALSE);
	DrawSelectedTracksD( &mainD, wDrawColorWhite );
	UndoStart( _("Move Tracks"), "Move Tracks" );
	MoveTracks( quickMove==MOVE_QUICK, TRUE, FALSE, move_pos, zero, 0.0 );
	wDrawDelayUpdate( mainD.d, FALSE );
	MainRedraw();
	MapRedraw();
}

static void QuickRotate( void* pangle )
{
	ANGLE_T angle = (ANGLE_T)(long)pangle;
	if ( SelectedTracksAreFrozen() )
		return;
	wDrawDelayUpdate( mainD.d, TRUE );
	GetMovedTracks(FALSE);
	DrawSelectedTracksD( &mainD, wDrawColorWhite );
	UndoStart( _("Rotate Tracks"), "Rotate Tracks" );
	MoveTracks( quickMove==MOVE_QUICK, FALSE, TRUE, zero, cmdMenuPos, angle );
	wDrawDelayUpdate( mainD.d, FALSE );
	MainRedraw();
	MapRedraw();
}


static wMenu_p moveDescM;
static wMenuToggle_p moveDescMI;
static track_p moveDescTrk;
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
		EPINX_T ep1, ep;
		BOOL_T hidden_t, hidden;
		int mode;
		if (hidden) *hidden_o = FALSE;
		while ( TrackIterate( &trk1 ) ) {
			if ( !GetLayerVisible(GetTrkLayer(trk1)) )
				continue;
			if ( (!GetTrkVisible(trk1)) && drawTunnel==0 )
				continue;
			if ( (labelEnable&LABELENABLE_ENDPT_ELEV)!=0) {
				for ( ep1=0; ep1<GetTrkEndPtCnt(trk1); ep1++ ) {
					d = EndPtDescriptionDistance( pos, trk1, ep1, FALSE, NULL );  //No hidden
					if ( d < dd ) {
						dd = d;
						trk = trk1;
						ep = ep1;
						mode = 0;
						hidden = FALSE;
					}
				}
			}
			if ( !QueryTrack( trk1, Q_HAS_DESC ) )
				continue;
			if ((labelEnable&LABELENABLE_TRKDESC)==0)
				continue;
			if ( ( GetTrkBits( trk1 ) & TB_HIDEDESC ) != 0 ) {
				if ( !show_hidden ) continue;
			}
			d = CompoundDescriptionDistance( pos, trk1, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 1;
				hidden = hidden_t;
			}
			d = CurveDescriptionDistance( pos, trk1, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 2;
				hidden = hidden_t;
			}
			d = CornuDescriptionDistance( pos, trk1, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 3;
				hidden = hidden_t;
			}
			d = BezierDescriptionDistance( pos, trk1, show_hidden, &hidden_t );
			if ( d < dd ) {
				dd = d;
				trk = trk1;
				ep = -1;
				mode = 4;
				hidden = hidden_t;
			}
		}
		if (trk != NULL) {       //Only when close to a label not anywhere on layout!
			if (ep_o) *ep_o = ep;
			if (mode_o) *mode_o = mode;
			if (hidden_o) *hidden_o = hidden;
			return trk;
		}
		else return NULL;
}

STATUS_T CmdMoveDescription(
		wAction_t action,
		coOrd pos )
{
	static track_p trk;
	static EPINX_T ep;
	track_p trk1;
	EPINX_T ep1;
	DIST_T d, dd;
	static BOOL_T hidden;
	static int mode;

	switch (action) {
	case C_START:
		if ( labelWhen < 2 || mainD.scale > labelScale ||
			 (labelEnable&(LABELENABLE_TRKDESC|LABELENABLE_ENDPT_ELEV))==0 ) {
			ErrorMessage( MSG_DESC_NOT_VISIBLE );
			return C_TERMINATE;
		}
		trk = NULL;
		hidden = FALSE;
		mode = -1;
		InfoMessage( _("Select and drag a description") );
		break;
	case wActionMove:
		if ( labelWhen < 2 || mainD.scale > labelScale ) return C_CONTINUE;
		if ((trk=FindTrackDescription(pos,&ep,&mode,TRUE,&hidden))!=NULL) {
			if (mode==0) {
				if (!hidden)
					DrawEndPt2( &mainD, trk, ep, wDrawColorBlue );
			} else {
				if (hidden) {
					DrawTrack(trk,&mainD,wDrawColorAqua);
					InfoMessage(_("Hidden description"));
				} else
					DrawTrack(trk,&mainD,wDrawColorBlue);
			}
		}
		MainRedraw();
		break;
	case C_DOWN:
		if (( labelWhen < 2 || mainD.scale > labelScale ) ||
		 (labelEnable&(LABELENABLE_TRKDESC|LABELENABLE_ENDPT_ELEV))==0 ) {
			 	 ErrorMessage( MSG_DESC_NOT_VISIBLE );
			return C_TERMINATE;
		 }
		trk = FindTrackDescription(pos,&ep,&mode,TRUE,&hidden);
		if (trk != NULL ) {
			if (hidden) {
				if (mode == 0) return C_CONTINUE;
				InfoMessage(_("Hidden Label - Drag to reveal"));
			} else {
				InfoMessage(_("Drag label"));
			}
			UndoStart( _("Move Label"), "Modedesc( T%d )", GetTrkIndex(trk) );
			UndoModify( trk );
		}
		/* no break */
	case C_MOVE:
		if ( labelWhen < 2 || mainD.scale > labelScale )
			return C_TERMINATE;
		if (trk != NULL) {
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
		}
		hidden = FALSE;
		MainRedraw();
		break;
	case C_UP:
		trk = NULL;
		InfoMessage(_("To Hide, use Context Menu"));
		MainRedraw();
		break;
	case C_REDRAW:
		if (trk) {
			if (mode==0) {
				DrawEndPt2( &mainD, trk, ep, hidden?wDrawColorAqua:wDrawColorBlue );
			} else {
				if (( GetTrkBits( trk ) & TB_HIDEDESC ) != 0 )
					DrawTrack(trk,&mainD,wDrawColorAqua);
				else
					DrawTrack(trk,&mainD,wDrawColorBlue);
			}
		}
		break;
	case C_CMDMENU:
		if (trk == NULL)
			moveDescTrk = OnTrack( &pos, TRUE, FALSE );
		else
			moveDescTrk = trk;
		if ( moveDescTrk == NULL ) break;
		if ( ! QueryTrack( moveDescTrk, Q_HAS_DESC ) ) break;
		if ( moveDescM == NULL ) {
			moveDescM = MenuRegister( "Move Desc Toggle" );
			moveDescMI = wMenuToggleCreate( moveDescM, "", _("Show Description"), 0, TRUE, ChangeDescFlag, NULL );
		}
		wMenuToggleSet( moveDescMI, ( GetTrkBits( moveDescTrk ) & TB_HIDEDESC ) == 0 );
		wMenuPopupShow( moveDescM );
		MainRedraw();
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
		DrawMapBoundingBox( FALSE );
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
		DrawMapBoundingBox( TRUE );
	}
	wSetCursor( mainD.d, defaultCursor );
	UndoEnd();
	InfoCount( trackCount );
    MainRedraw();
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
			DrawLine( &tempD, pos0, pos1, 0, wDrawColorBlack );
            MainRedraw();
            MapRedraw();
			return C_CONTINUE;
		case C_MOVE:
			DrawLine( &tempD, pos0, pos1, 0, wDrawColorBlack );
			pos1 = pos;
			DrawLine( &tempD, pos0, pos1, 0, wDrawColorBlack );
			InfoMessage( _("Angle %0.2f"), FindAngle( pos0, pos1 ) );
            MainRedraw();
            MapRedraw();
			return C_CONTINUE;
		case C_UP:
			DrawLine( &tempD, pos0, pos1, 0, wDrawColorBlack );
			UndoStart( _("Flip Tracks"), "flip" );
			FlipTracks( pos0, FindAngle( pos0, pos1 ) );
			state = 0;
            MainRedraw();
            MapRedraw();
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

static STATUS_T SelectArea(
		wAction_t action,
		coOrd pos )
{
	static coOrd pos0;
	static int state;
	static coOrd base, size, lo, hi;
	static BOOL_T add;
	int cnt;

	track_p trk;

	switch (action) {

	case C_START:
		state = 0;
		return C_CONTINUE;

	case C_DOWN:
	case C_RDOWN:
		pos0 = pos;
		add = (action == C_DOWN);
		return C_CONTINUE;

	case C_MOVE:
	case C_RMOVE:
		if (state == 0) {
			state = 1;
		} else {
			//DrawHilight( &mainD, base, size );
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
		DrawHilight( &mainD, base, size, action == C_MOVE );
		return C_CONTINUE;

	case C_UP:
	case C_RUP:
		if (state == 1) {
			state = 0;
			DrawHilight( &mainD, base, size, action == C_UP );
			cnt = 0;
			trk = NULL;
			if (action==C_UP) SetAllTrackSelect( FALSE );							//Remove all tracks first
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
						if (cnt > incrementalDrawLimit) {
							selectedTrackCount += (action==C_UP?1:-1);
							if (action==C_UP)
								SetTrkBits( trk, TB_SELECTED );
							else
								ClrTrkBits( trk, TB_SELECTED );
						} else {
							SelectOneTrack( trk, action==C_UP );
						}
					}
				}
			}
			SelectedTrackCountChange();
			MainRedraw();
		}
		return C_CONTINUE;

	case C_CANCEL:
		if (state == 1) {
			DrawHilight( &mainD, base, size, add);
			state = 0;
		}
		break;

	case C_REDRAW:
		if (state == 0)
			break;
		DrawHilight( &mainD, base, size, add );
		break;

	}
	return C_CONTINUE;
}



static STATUS_T SelectTrack( 
		coOrd pos )
{
	track_p trk;
	char msg[STR_SIZE];

	if ((trk = OnTrack( &pos, FALSE, FALSE )) == NULL) {
		SetAllTrackSelect( FALSE );							//Unselect all
		return C_CONTINUE;
	}
	DescribeTrack( trk, msg, sizeof msg );
	InfoMessage( msg );
	if (MyGetKeyState() & WKEY_SHIFT) {						//All track up to
		SelectConnectedTracks( trk );
	} else if (MyGetKeyState() & WKEY_CTRL) {
		SelectOneTrack( trk, !GetTrkSelected(trk) );
	} else {
		SetAllTrackSelect( FALSE );							//Just this Track
		SelectOneTrack( trk, !GetTrkSelected(trk) );
	}

	return C_CONTINUE;
}

static STATUS_T Activate( coOrd pos) {
	track_p trk;
	if ((trk = OnTrack( &pos, TRUE, FALSE )) == NULL) {
				return C_CONTINUE;
	}
	ActivateTrack(trk);

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

void DrawHighlightBoxes() {
	track_p ts = NULL;
	while ( TrackIterate( &ts ) ) {
		if ( !GetLayerVisible( GetTrkLayer( ts))) continue;
		if (!GetTrkSelected(ts)) continue;
		coOrd hi,lo;
		GetBoundingBox(ts, &hi, &lo);
		coOrd hilite,size;
		hilite = lo;
		size.x = hi.x-lo.x;
		size.y = hi.y-lo.y;
		DIST_T w,h;
		w = (wPos_t)((size.x/mainD.scale)*mainD.dpi+0.5+10);
		h = (wPos_t)((size.y/mainD.scale)*mainD.dpi+0.5+10);
		wPos_t x, y;
		mainD.CoOrd2Pix(&mainD,hilite,&x,&y);
		wDrawFilledRectangle(mainD.d, x-5, y-5, w, h, wDrawColorPowderedBlue, wDrawOptTemp);
	}

}


static STATUS_T CmdSelect(
		wAction_t action,
		coOrd pos )
{
	static enum { AREA, MOVE } mode;
	static BOOL_T doingMove = FALSE;
	static BOOL_T doingRotate = FALSE;
	STATUS_T rc=C_CONTINUE;
	track_p t;

	if ( action == C_DOWN || action == C_RDOWN ) {
		mode = AREA;
		if (MyGetKeyState() & (WKEY_SHIFT|WKEY_CTRL)) {
			mode = MOVE;
		}
	}

	switch (action) {
	case C_START:
		InfoMessage( _("Select track") );
		importMove = FALSE;
		doingMove = FALSE;
		doingRotate = FALSE;
		SelectArea( action, pos );
		wMenuPushEnable( rotateAlignMI, FALSE );
		wSetCursor(mainD.d,defaultCursor);
		mode = AREA;
		MainRedraw();
		break;

	case wActionMove:
		DYNARR_RESET(trkSeg_t,anchors_da);
		coOrd p = pos;
		t = OnTrack( &p, FALSE, FALSE );
		track_p ht;
		if ((selectedTrackCount==0) && (t == NULL)) return C_CONTINUE;
		if (selectedTrackCount>0) {
			if ((ht = IsInsideABox(pos)) != NULL) {
				if ((MyGetKeyState()&WKEY_CTRL)) {
					CreateRotateAnchor(pos);
				} else if ((MyGetKeyState()&WKEY_SHIFT)) {
					CreateMoveAnchor(pos);
				}
			}
			if (anchors_da.cnt)
					DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		} if (!t) return C_CONTINUE;
		if (!GetTrkSelected(t)) DrawTrack(t,&mainD,wDrawColorBlue);
		break;

	case C_DOWN:
	case C_RDOWN:
	case C_RMOVE:
	case C_RUP:
	case C_UP:
	case C_MOVE:
		switch (mode) {
		case MOVE:
			if (SelectedTracksAreFrozen()) {
				rc = C_TERMINATE;
				doingMove = FALSE;
				doingRotate = FALSE;
			} else if ((action >= C_DOWN && action <= C_UP) && (doingRotate || (MyGetKeyState()&WKEY_CTRL)))	{
				RotateAlign( FALSE );
				rc = CmdRotate( action, pos );
				doingMove = FALSE;
				doingRotate = TRUE;
			} else if ((action >= C_DOWN && action <= C_UP) && (doingMove || (MyGetKeyState()&WKEY_SHIFT))) {
				rc = CmdMove( action, pos );
				doingMove = TRUE;
				doingRotate = FALSE;
			}
			if (action == C_UP) {
				doingMove = FALSE;
				doingRotate = FALSE;
				mode = AREA;
				InfoMessage(_("Select Track, +Ctrl to Add, +Shift to add joined"));
			}
			break;
		case AREA:
			rc = SelectArea( action, pos );

			break;
		}
		return rc;
		break;
	case C_REDRAW:
		if (doingMove) {
			rc = CmdMove( C_REDRAW, pos );
		} else if (doingRotate) {
			rc = CmdRotate( C_REDRAW, pos );
		} else if (anchors_da.cnt) {
			DrawSegs( &mainD, zero, 0.0, &anchors(0), anchors_da.cnt, trackGauge, wDrawColorBlack );
		}
		if (mode==AREA)
			rc = SelectArea( action, pos );
		DrawHighlightBoxes();
		return rc;

	case C_LCLICK:
		switch (mode) {
		case MOVE:
		case AREA:
			rc = SelectTrack( pos );
			MainRedraw();
			return rc;
		}
		mode = AREA;
		break;

	case C_LDOUBLE:
		switch (mode) {
			case AREA:
				return Activate(pos);
			default:
				break;
		}
		break;
	case C_CMDMENU:
		if (selectedTrackCount <= 0) {
			wMenuPopupShow( selectPopup1M );
		} else {
			coOrd base = pos;
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
			if ((trk)) {
				wMenuPushEnable(descriptionMI, QueryTrack( trk, Q_HAS_DESC ));
			}
			wMenuPopupShow( selectPopup2M );
		}
		return C_CONTINUE;
	}
	return C_CONTINUE;
}


#include "bitmaps/select.xpm"
#include "bitmaps/delete.xpm"
#include "bitmaps/tunnel.xpm"
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

}


EXPORT void InitCmdSelect( wMenu_p menu )
{
	selectCmdInx = AddMenuButton( menu, CmdSelect, "cmdSelect", _("Select"), wIconCreatePixMap(select_xpm),
				LEVEL0, IC_CANCEL|IC_POPUP|IC_LCLICK|IC_CMDMENU|IC_WANT_MOVE, ACCL_SELECT, NULL );
	endpt_bm = wDrawBitMapCreate( mainD.d, bmendpt_width, bmendpt_width, 7, 7, bmendpt_bits );
	angle_bm[0] = wDrawBitMapCreate( mainD.d, bma90_width, bma90_width, 7, 7, bma90_bits );
	angle_bm[1] = wDrawBitMapCreate( mainD.d, bma135_width, bma135_width, 7, 7, bma135_bits );
	angle_bm[2] = wDrawBitMapCreate( mainD.d, bma0_width, bma0_width, 7, 7, bma0_bits );
	angle_bm[3] = wDrawBitMapCreate( mainD.d, bma45_width, bma45_width, 7, 7, bma45_bits );
	AddPlaybackProc( SETMOVEMODE, (playbackProc_p)SetMoveMode, NULL );
	wPrefGetInteger( "draw", "movemode", &moveMode, MAXMOVEMODE );
	if (moveMode > MAXMOVEMODE || moveMode < 0)
		moveMode = MAXMOVEMODE;

	selectPopup1M = MenuRegister( "Move Draw Mode" );
	quickMove1M[0] = wMenuToggleCreate( selectPopup1M, "", _("Normal"), 0, quickMove==0, ChangeQuickMove, (void *) 0 );
	quickMove1M[1] = wMenuToggleCreate( selectPopup1M, "", _("Simple"), 0, quickMove==1, ChangeQuickMove, (void *) 1 );
	quickMove1M[2] = wMenuToggleCreate( selectPopup1M, "", _("End Points"), 0, quickMove==2, ChangeQuickMove, (void *) 2 );
	selectPopup2M = MenuRegister( "Move Draw Mode " );
	wMenuSeparatorCreate( selectPopup2M );
	AddMoveMenu( selectPopup2M, QuickMove);
	wMenuSeparatorCreate( selectPopup2M );
	AddRotateMenu( selectPopup2M, QuickRotate );
	wMenuSeparatorCreate( selectPopup2M );
	descriptionMI = wMenuPushCreate(selectPopup2M, "cmdMoveLabel", _("Move Description"), 0, (wMenuCallBack_p)moveDescription, NULL);
	rotateAlignMI = wMenuPushCreate( selectPopup2M, "", _("Align"), 0, (wMenuCallBack_p)RotateAlign, (void* ) 1 );
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
#ifdef LATER
	tunnelCmdInx = AddButton( "cmdTunnel", _("Tunnel"),
		(addButtonCallBack_t)SelectTunnel, NULL, IC_SELECTED|IC_POPUP, NULL, LEVEL0_50, ACCL_TUNNEL,
		(wControl_p)wButtonCreate(mainW, 0, 0, "cmdTunnel", (char*)bm_p, BO_ICON, 0, (wButtonCallBack_p)SelectTunnel, 0 ) );
#endif
}


EXPORT void InitCmdMoveDescription( wMenu_p menu )
{
	AddMenuButton( menu, CmdMoveDescription, "cmdMoveLabel", _("Move Description"), wIconCreatePixMap(movedesc_xpm),
				LEVEL0, IC_STICKY|IC_POPUP|IC_CMDMENU|IC_WANT_MOVE, ACCL_MOVEDESC, NULL );
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
