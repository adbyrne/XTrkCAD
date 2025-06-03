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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ccurve.h"
#include "cstraigh.h"
#include "cselect.h"
#include "cundo.h"
#include "form.h"
#include "track.h"
#include "layout.h"
#include "common-ui.h"
#include "icons.h"

static struct {
	track_p Trk;
	coOrd orig;
	track_p anchor_Trk;
} Dpa;

static DIST_T parSeparation = 1.0;
static double parSepFactor = 0.0;
static long parType = 0;

enum PAR_TYPE_E { PAR_TRACK, PAR_LINE };

static paramFloatRange_t r_0o1_100 = { 0.0, 100.0, 100 };
static paramFloatRange_t r_0_10 = { 0.0, 10.0 };

static paramData_t parTrackPLs[] = {
#define parSepPD (parTrackPLs[0])
#define parSepI 0
	{	PD_FLOAT, &parSeparation, "separation", PDO_DIM, &r_0o1_100, NULL },
#define parFactorPD (parTrackPLs[1])
#define parFactorI 1
	{   PD_FLOAT, &parSepFactor, "factor", 0, &r_0_10, NULL }
};
static paramGroup_t parTrackPG = { "parallel", PGO_FULLDIALOGFROMBUILDER, parTrackPLs, COUNT( parTrackPLs ) };

static paramData_t parLinePLs[] = {
//#define parSepPD (parSepPLs[0])
#define parLineSepI 0
	{	PD_FLOAT, &parSeparation, "separation", PDO_DIM, &r_0o1_100, NULL },
};
static paramGroup_t parLinePG = { "parallelLine", PGO_FULLDIALOGFROMBUILDER, parLinePLs, COUNT(parLinePLs) };

static void
ReadSeparation(long type)
{
	if (type == PAR_TRACK) {
		sprintf(message, "separation-%s", curScaleName);
		parSeparation = ceil(13.0 * 12.0 / curScaleRatio);
	}
	else {
		sprintf(message, "line-separation-%s", curScaleName);
		parSeparation = 5.0 * 12.0 / curScaleRatio;
	}

	wPrefGetFloat("parallel", message, &parSeparation, parSeparation);
}

static void
SaveSeparation(long type)
{
	if (type == PAR_TRACK) {
		sprintf(message, "separation-%s", curScaleName);
	}
	else {
		sprintf(message, "line-separation-%s", curScaleName);
	}
	wPrefSetFloat("parallel", message, parSeparation);
}


static STATUS_T CmdParallel(wAction_t action, coOrd pos)
{
	DIST_T d;
	track_p t=NULL;
	coOrd p;
	static coOrd p0, p1;
	ANGLE_T a;
	track_p t0, t1;
	EPINX_T ep0=-1, ep1=-1;
	paramGroup_p parGroup;
	paramData_p parPLs;

	static DIST_T parRFactor;

	parType = VP2L(commandContext);
	if (parType == PAR_TRACK) {
		parGroup = &parTrackPG;
	}
	else {
		parGroup = &parLinePG;
	}
	parPLs = parGroup->paramPtr;

	switch (action&0xFF) {

	case C_START:

		if (parPLs[0].control==NULL) {
			FormCreateControls(parGroup);
		}
	
		ReadSeparation(parType);
			
		FormLoadControls(parGroup);
		ParamGroupRecord(parGroup);
		//parSepPD.option |= PDO_NORECORD;
		//parFactorPD.option |= PDO_NORECORD;

		InfoSetControls(mainW, parGroup->nameStr);
		//parSepPD.option &= ~PDO_NORECORD;
		//parFactorPD.option &= ~PDO_NORECORD;
		Dpa.anchor_Trk = NULL;
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		SetAllTrackSelect( FALSE );
		return C_CONTINUE;

	case wActionMove:
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		Dpa.anchor_Trk = NULL;
		if (parType == PAR_TRACK) {
			Dpa.anchor_Trk = OnTrack(&pos, FALSE, TRUE);
		} else {
			Dpa.anchor_Trk = OnTrack(&pos, FALSE, FALSE);
		}

		if (!Dpa.anchor_Trk) {
			return C_CONTINUE;
		}
		if (Dpa.anchor_Trk && !CheckTrackLayerSilent(Dpa.anchor_Trk)) {
			Dpa.anchor_Trk = NULL;
			return C_CONTINUE;
		}
		if (!QueryTrack(Dpa.anchor_Trk, Q_CAN_PARALLEL)) {
			Dpa.anchor_Trk = NULL;
			return C_CONTINUE;
		}
		break;
	case C_DOWN:
		Dpa.anchor_Trk = NULL;
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		if (parSeparation < 0.0) {
			ErrorMessage(MSG_PARALLEL_SEP_GTR_0);
			return C_ERROR;
		}

		InfoSetControls(mainW, parGroup->nameStr);
		ParamLoadData(parGroup);
		Dpa.orig = pos;
		if (parType == PAR_TRACK) {
			Dpa.Trk = OnTrack(&pos, FALSE, TRUE);
		} else {
			Dpa.Trk = OnTrack(&pos, FALSE, FALSE);        //Also lines for line
		}
		if (!Dpa.Trk) {
			return C_CONTINUE;
		}
		if (!QueryTrack(Dpa.Trk, Q_CAN_PARALLEL)) {
			Dpa.Trk = NULL;
			InfoMessage(_(" Track/Line doesn't support parallel"));
			wBeep();
			return C_CONTINUE;
		}

		parRFactor = (2864.0*(double)parSepFactor)/curScaleRatio;

		if ((parType == PAR_TRACK) && (parSeparation == 0.0)) {
			DIST_T orig_gauge = GetTrkGauge(Dpa.Trk);
			DIST_T new_gauge = GetScaleTrackGauge(GetLayoutCurScale());
			if (orig_gauge == new_gauge) {
				ErrorMessage(MSG_PARALLEL_SEP_GTR_0);
				return C_ERROR;
			}
			parSeparation = fabs(orig_gauge/2-new_gauge/2);
			parRFactor = 0.0;
		} else if (parType != PAR_TRACK) {
			parRFactor = 0.0;
		}
		/* in case query has changed things (eg joint) */
		/*
		 * this seems to cause problems so I commented it out
		 * until further investigation shows the necessity
		 */
		//Dpa.Trk = OnTrack( &Dpa.orig, TRUE, TRUE );
		DYNARR_RESET( trkSeg_t, tempSegs_da );
	/* no break */

	case C_MOVE:
		if (Dpa.Trk == NULL) {
			return C_CONTINUE;
		}
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		if (!MakeParallelTrack(Dpa.Trk, pos, parSeparation, parRFactor, NULL, &p0, &p1,
		                       parType == PAR_TRACK)) {
			Dpa.Trk = NULL;
			return C_CONTINUE;
		}
		return C_CONTINUE;

	case C_UP:
		Dpa.anchor_Trk = NULL;
		if (Dpa.Trk == NULL) {
			return C_CONTINUE;
		}
		t0=t1=NULL;
		if (parType == PAR_TRACK) {
			p = p0;
			DYNARR_RESET( trkSeg_t, tempSegs_da );
			if ((t0=OnTrack(&p, FALSE, TRUE)) != NULL) {
				ep0 = PickEndPoint(p, t0);
				if (ep0 < 0 || GetTrkEndTrk(t0,ep0) != NULL) {
					t0 = NULL;
				} else {
					p = GetTrkEndPos(t0, ep0);
					d = FindDistance(p, p0);
					if (d > connectDistance) {
						t0 = NULL;
					}
				}
			}
			p = p1;
			if ((t1=OnTrack(&p, FALSE, TRUE)) != NULL) {
				ep1 = PickEndPoint(p, t1);
				if (ep1 < 0 || GetTrkEndTrk(t1,ep1) != NULL) {
					t1 = NULL;
				} else {
					p = GetTrkEndPos(t1, ep1);
					d = FindDistance(p, p1);
					if (d > connectDistance) {
						t1 = NULL;
					}
				}
			}
		}
		UndoStart(_("Create Parallel Track"), "newParallel");
		if (!MakeParallelTrack(Dpa.Trk, pos, parSeparation, parRFactor, &t, NULL, NULL,
		                       parType == PAR_TRACK)) {
			DYNARR_RESET( trkSeg_t, tempSegs_da );
			return C_TERMINATE;
		}
		if (parType == PAR_TRACK) {
			if (GetTrkGauge(Dpa.Trk)> parSeparation) {
				SetTrkNoTies(t, TRUE);
			}

			SetTrkBits(t,(GetTrkBits(t)&TB_HIDEDESC) | (GetTrkBits(Dpa.Trk)&~TB_HIDEDESC));

			if (t0) {
				a = NormalizeAngle(GetTrkEndAngle(t0, ep0) - GetTrkEndAngle(t,
				                   0) + (180.0+connectAngle/2.0));
				if (a < connectAngle) {
					DrawEndPt(&mainD, t0, ep0, wDrawColorWhite);
					ConnectTracks(t0, ep0, t, 0);
					DrawEndPt(&mainD, t0, ep0, wDrawColorBlack);
				}
			}
			if (t1) {
				a = NormalizeAngle(GetTrkEndAngle(t1, ep1) - GetTrkEndAngle(t,
				                   1) + (180.0+connectAngle/2.0));
				if (a < connectAngle) {
					DrawEndPt(&mainD, t1, ep1, wDrawColorWhite);
					ConnectTracks(t1, ep1, t, 1);
					DrawEndPt(&mainD, t1, ep1, wDrawColorBlack);
				}
			}
		}
		DrawNewTrack(t);
		UndoEnd();
		InfoDefaultControls();
		SaveSeparation(parType);

		DYNARR_RESET( trkSeg_t, tempSegs_da );
		return C_TERMINATE;

	case C_REDRAW:
		if (Dpa.anchor_Trk) {
			DrawTrack(Dpa.anchor_Trk,&tempD,
			          wDrawColorPreviewSelected);    //Special color means THICK3 as well
		}
		DrawSegsDA( &tempD, NULL, zero, 0.0, &tempSegs_da, trackGauge, wDrawColorBlack,
		            0 );
		return C_CONTINUE;

	case C_CANCEL:
		Dpa.anchor_Trk = NULL;
		DYNARR_RESET( trkSeg_t, tempSegs_da );
		InfoDefaultControls();
		return C_TERMINATE;

	}
	return C_CONTINUE;
}

EXPORT void InitCmdParallel( wMenu_p menu )
{
	ButtonGroupBegin( _("Parallel"), "cmdParallelSetCmd", _("Parallel") );
	AddMenuButton( menu, CmdParallel, "cmdParallelTrack", _("Parallel Track"),
		CreateToolbarIconFromResource("parallel.png"), LEVEL0_50,
	               IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_PARALLEL, I2VP(0) );
	AddMenuButton( menu, CmdParallel, "cmdParallelLine", _("Parallel Line"),
		CreateToolbarIconFromResource("parallel-line.png"), LEVEL0_50,
	               IC_STICKY|IC_POPUP|IC_WANT_MOVE, ACCL_PARALLEL, I2VP(1) );
	ButtonGroupEnd();
	ParamRegister( &parTrackPG );
	ParamRegister(&parLinePG);
}
