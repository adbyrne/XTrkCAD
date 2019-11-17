/** \file cparalle.c
 * PARALLEL
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

#include "ccurve.h"
#include "cstraigh.h"
#include "cundo.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"
#include "layout.h"

static struct {
		track_p Trk;
		coOrd orig;
		track_p anchor_Trk;
		} Dpa;

static DIST_T parSeparation = 1.0;
static long parType;

enum PAR_TYPE_E { PAR_TRACK, PAR_LINE };
static char * parTypeLabels[] = { N_("Track"), N_("Line"), NULL };

static paramFloatRange_t r_0o1_100 = { 0.0, 100.0, 100 };
static paramData_t parSepPLs[] = {
#define parSepPD (parSepPLs[1])
	{   PD_RADIO, &parType, "type", 0, parTypeLabels, N_("Output Type"), BC_HORZ|BC_NONE },
	{	PD_FLOAT, &parSeparation, "separation", PDO_DIM|PDO_NOPREF, &r_0o1_100, N_("Separation") } };
static paramGroup_t parSepPG = { "parallel", 0, parSepPLs, sizeof parSepPLs/sizeof parSepPLs[0] };


static STATUS_T CmdParallel( wAction_t action, coOrd pos )
{

	DIST_T d;
	track_p t=NULL;
	coOrd p;
	static coOrd p0, p1;
	ANGLE_T a;
	track_p t0, t1;
	EPINX_T ep0=-1, ep1=-1;
	wControl_p controls[3];
	char * labels[2];
	long save_options;

	switch (action) {

	case C_START:
		if (parSepPLs[0].control==NULL) {
			ParamCreateControls( &parSepPG, NULL );
		}
		sprintf( message, "parallel-separation-%s", curScaleName );
		parSeparation = ceil(13.0*12.0/curScaleRatio);
		wPrefGetFloat( "misc", message, &parSeparation, parSeparation );
		parType = PAR_TRACK;
		ParamLoadControls( &parSepPG );
		ParamGroupRecord( &parSepPG );
		controls[0] = parSepPD.control;
		controls[1] = parSepPLs[0].control;
		controls[2] = NULL;
		labels[0] = N_("Separation");
		labels[1] = N_("Type:");
		InfoSubstituteControls( controls, labels );
		Dpa.anchor_Trk = NULL;
		tempSegs_da.cnt = 0;
		/*InfoMessage( "Select track" );*/
		return C_CONTINUE;

	case wActionMove:
		tempSegs_da.cnt = 0;
		Dpa.anchor_Trk = NULL;
		Dpa.anchor_Trk = OnTrack( &pos, FALSE, TRUE );
		if (!Dpa.anchor_Trk) {
			return C_CONTINUE;
		}
		if (Dpa.anchor_Trk && !CheckTrackLayerSilent( Dpa.anchor_Trk ) ) {
			return C_CONTINUE;
		}
		if (!QueryTrack(Dpa.anchor_Trk, Q_CAN_PARALLEL)) {
			return C_CONTINUE;
		}
		DrawTrack(Dpa.anchor_Trk,&mainD,wDrawColorBlueHighlight);    //Special color means THICK3 as well
		break;
	case C_DOWN:
		Dpa.anchor_Trk = NULL;
		tempSegs_da.cnt = 0;
		if ( parSeparation < 0.0 ) {
			ErrorMessage( MSG_PARALLEL_SEP_GTR_0 );
			return C_ERROR;
		}

		controls[0] = parSepPD.control;
		controls[1] = parSepPLs[0].control;
		controls[2] = NULL;
		labels[0] = N_("Separation");
		labels[1] = N_("Type:");
		InfoSubstituteControls( controls, labels );
		ParamLoadData( &parSepPG );
		Dpa.orig = pos;
		Dpa.Trk = OnTrack( &Dpa.orig, TRUE, TRUE );
		if (!Dpa.Trk) {
				return C_CONTINUE;
		}
		if ( !QueryTrack( Dpa.Trk, Q_CAN_PARALLEL ) ) {
			Dpa.Trk = NULL;
			InfoMessage(_(" Track doesn't support parallel"));
			return C_CONTINUE;
		}
		if (parSeparation == 0.0) {
			DIST_T orig_gauge = GetTrkGauge( Dpa.Trk );
			DIST_T new_gauge = GetScaleTrackGauge(GetLayoutCurScale());
			if (orig_gauge == new_gauge) {
				ErrorMessage( MSG_PARALLEL_SEP_GTR_0 );
					return C_ERROR;
			}
			parSeparation = fabs(orig_gauge/2-new_gauge/2);
		}
		/* in case query has changed things (eg joint) */
		/* 
		 * this seems to cause problems so I commented it out
		 * until further investigation shows the necessity
		 */
		//Dpa.Trk = OnTrack( &Dpa.orig, TRUE, TRUE ); 
		tempSegs_da.cnt = 0;

	case C_MOVE:
		MainRedraw();
		if (Dpa.Trk == NULL) return C_CONTINUE;
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		tempSegs_da.cnt = 0;
		if ( !MakeParallelTrack( Dpa.Trk, pos, parSeparation, NULL, &p0, &p1, parType == PAR_TRACK ) ) {
			Dpa.Trk = NULL;
			tempD.options = save_options;
			return C_CONTINUE;
		}
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		return C_CONTINUE;

	case C_UP:
		Dpa.anchor_Trk = NULL;
		if (Dpa.Trk == NULL) return C_CONTINUE;
		DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorWhite );
		p = p0;
		tempSegs_da.cnt = 0;
		if ((t0=OnTrack( &p, FALSE, TRUE )) != NULL) {
			ep0 = PickEndPoint( p, t0 );
			if ( GetTrkEndTrk(t0,ep0) != NULL ) {
				t0 = NULL;
			} else {
				p = GetTrkEndPos( t0, ep0 );
				d = FindDistance( p, p0 );
				if ( d > connectDistance )
					t0 = NULL;
			}
		}
		p = p1;
		if ((t1=OnTrack( &p, FALSE, TRUE )) != NULL) {
			ep1 = PickEndPoint( p, t1 );
			if ( GetTrkEndTrk(t1,ep1) != NULL ) {
				t1 = NULL;
			} else {
				p = GetTrkEndPos( t1, ep1 );
				d = FindDistance( p, p1 );
				if ( d > connectDistance )
					t1 = NULL;
			}
		}
		UndoStart( _("Create Parallel Track"), "newParallel" );
		if ( !MakeParallelTrack( Dpa.Trk, pos, parSeparation, &t, NULL, NULL, parType == PAR_TRACK ) ) {
			tempSegs_da.cnt = 0;
			MainRedraw();
			MapRedraw();
			return C_TERMINATE;
		}
		if (GetTrkGauge( Dpa.Trk )> parSeparation)
				SetTrkNoTies(t, TRUE);
		//CopyAttributes( Dpa.Trk, t );    Don't force scale or track width or Layer
		SetTrkBits(t,(GetTrkBits(t)&TB_HIDEDESC) | (GetTrkBits(Dpa.Trk)&~TB_HIDEDESC));

		if ( t0 ) {
			a = NormalizeAngle( GetTrkEndAngle( t0, ep0 ) - GetTrkEndAngle( t, 0 ) + (180.0+connectAngle/2.0) ); 
			if (a < connectAngle) {
				DrawEndPt( &mainD, t0, ep0, wDrawColorWhite );
				ConnectTracks( t0, ep0, t, 0 );
				DrawEndPt( &mainD, t0, ep0, wDrawColorBlack );
			}
		}
		if ( t1 ) {
			a = NormalizeAngle( GetTrkEndAngle( t1, ep1 ) - GetTrkEndAngle( t, 1 ) + (180.0+connectAngle/2.0) );
			if (a < connectAngle) {
				DrawEndPt( &mainD, t1, ep1, wDrawColorWhite );
				ConnectTracks( t1, ep1, t, 1 );
				DrawEndPt( &mainD, t1, ep1, wDrawColorBlack );
			}
		}
		DrawNewTrack( t );
		UndoEnd();
		InfoSubstituteControls( NULL, NULL );
		sprintf( message, "parallel-separation-%s", curScaleName );
		wPrefSetFloat( "misc", message, parSeparation );
		tempSegs_da.cnt = 0;
		MainRedraw();
		MapRedraw();
		return C_TERMINATE;

	case C_REDRAW:
		if (Dpa.anchor_Trk) {
			DrawTrack(Dpa.anchor_Trk,&mainD,wDrawColorBlueHighlight);    //Special color means THICK3 as well
		}
		if (tempSegs_da.cnt>0) {
			DrawSegs( &mainD, zero, 0.0, &tempSegs(0), tempSegs_da.cnt, trackGauge, wDrawColorBlack );
		}
		return C_CONTINUE;

	case C_CANCEL:
		Dpa.anchor_Trk = NULL;
		tempSegs_da.cnt = 0;
		InfoSubstituteControls( NULL, NULL );
		return C_TERMINATE;

	}
	return C_CONTINUE;
}


#include "bitmaps/parallel.xpm"

EXPORT void InitCmdParallel( wMenu_p menu )
{
	AddMenuButton( menu, CmdParallel, "cmdParallel", _("Parallel"), wIconCreatePixMap(parallel_xpm), LEVEL0_50, IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_PARALLEL, NULL );
	ParamRegister( &parSepPG );
}
