/* \file ctodesgn.c
 * T_TURNOUT Designer
 *
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

#ifdef WINDOWS
#include <stdlib.h>
#endif

#include <stdint.h>
#include <ctype.h>
#include <math.h>

#include "ccurve.h"
#include "compound.h"
#include "cstraigh.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"
#include "utility.h"

#define TURNOUTDESIGNER			"CTURNOUT DESIGNER"



/*****************************************
 *
 *   TURNOUT DESIGNER
 *
 */


#define NTO_REGULAR		(1)
#define NTO_CURVED		(2)
#define NTO_WYE			(3)
#define NTO_3WAY		(4)
#define NTO_CROSSING	(5)
#define NTO_S_SLIP		(6)
#define NTO_D_SLIP		(7)
#define NTO_R_CROSSOVER	(8)
#define NTO_L_CROSSOVER	(9)
#define NTO_D_CROSSOVER	(10)
#define NTO_STR_SECTION	(11)
#define NTO_CRV_SECTION	(12)
#define NTO_BUMPER		(13)
#define NTO_TURNTABLE	(14)

#define FLOAT			(1)


typedef struct {
		struct {
			wPos_t x, y;
		} pos;
		int index;
		char * winLabel;
		char * printLabel;
		enum { Dim_e, Frog_e, Angle_e } mode;
		} toDesignFloat_t;

typedef struct {
		PATHPTR_T paths;
		char * segOrder;
		} toDesignSchema_t;

typedef struct {
		int type;
		char * label;
		int strCnt;
		int lineCnt;
		wLines_t * lines;
		int floatCnt;
		toDesignFloat_t * floats;
		toDesignSchema_t * paths;
		int angleModeCnt;
		wLine_p lineC;
		} toDesignDesc_t;

static wWin_p newTurnW;
static FLOAT_T newTurnLen0;
static FLOAT_T newTurnLen1;
static FLOAT_T newTurnOff1;
static FLOAT_T newTurnAngle1;
static FLOAT_T newTurnLen2;
static FLOAT_T newTurnOff2;
static FLOAT_T newTurnAngle2;
static long newTurnAngleMode = 1;
static char newTurnRightDesc[STR_SIZE], newTurnLeftDesc[STR_SIZE];
static char newTurnRightPartno[STR_SIZE], newTurnLeftPartno[STR_SIZE];
static char newTurnManufacturer[STR_SIZE];
static char *newTurnAngleModeLabels[] = { N_("Frog #"), N_("Degrees"), NULL };
static DIST_T newTurnRoadbedWidth;
static long newTurnRoadbedLineWidth = 0;
static wDrawColor roadbedColor;
static DIST_T newTurnTrackGauge;
static char * newTurnScaleName;
static paramFloatRange_t r0_10000 = { 0, 10000, 80 };
static paramFloatRange_t r0_360 = { 0, 360, 80 };
static paramFloatRange_t r0_100 = { 0, 100, 80 };
static paramIntegerRange_t i0_100 = { 0, 100, 40 };
static void NewTurnOk( void * );
static void ShowTurnoutDesigner( void * );


static coOrd points[20];
static DIST_T radii[10] = { 0.0 };

#define POSX(X) ((wPos_t)((X)*newTurnout_d.dpi))
#define POSY(Y) ((wPos_t)((Y)*newTurnout_d.dpi))

static paramData_t turnDesignPLs[] = {
#define I_TOLENGTH			(0)
#define I_TO_FIRST_FLOAT	(0)
	{ PD_FLOAT, &newTurnLen1, "len1", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
	{ PD_FLOAT, &newTurnLen2, "len2", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
	{ PD_FLOAT, &newTurnLen0, "len0", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
#define I_TOOFFSET			(3)
	{ PD_FLOAT, &newTurnOff1, "off1", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Offset") },
	{ PD_FLOAT, &newTurnOff2, "off2", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Offset") },
#define I_TOANGLE			(5)
	{ PD_FLOAT, &newTurnAngle1, "angle1", PDO_DLGIGNORELABELWIDTH, &r0_360, N_("Angle") },
#define I_TO_LAST_FLOAT		(6)
	{ PD_FLOAT, &newTurnAngle2, "angle2", PDO_DLGIGNORELABELWIDTH, &r0_360, N_("Angle") },
#define I_TOMANUF			(7)
	{ PD_STRING, &newTurnManufacturer, "manuf", 0, NULL, N_("Manufacturer") },
#define I_TOLDESC			(8)
	{ PD_STRING, &newTurnLeftDesc, "desc1", 0, NULL, N_("Left Description") },
	{ PD_STRING, &newTurnLeftPartno, "partno1", PDO_DLGHORZ, NULL, N_(" #") },
#define I_TORDESC			(10)
	{ PD_STRING, &newTurnRightDesc, "desc2", 0, NULL, N_("Right Description") },
	{ PD_STRING, &newTurnRightPartno, "partno2", PDO_DLGHORZ, NULL, N_(" #") },
	{ PD_FLOAT, &newTurnRoadbedWidth, "roadbedWidth", PDO_DIM, &r0_100, N_("Roadbed Width") },
	{ PD_LONG, &newTurnRoadbedLineWidth, "roadbedLineWidth", PDO_DLGHORZ, &i0_100, N_("Line Width") },
	{ PD_COLORLIST, &roadbedColor, "color", PDO_DLGHORZ|PDO_DLGBOXEND, NULL, N_("Color") },
	{ PD_BUTTON, (void*)NewTurnOk, "done", PDO_DLGCMDBUTTON, NULL, N_("Ok") },
	{ PD_BUTTON, (void*)wPrintSetup, "printsetup", 0, NULL, N_("Print Setup") },
#define I_TOANGMODE			(17)
	{ PD_RADIO, &newTurnAngleMode, "angleMode", 0, newTurnAngleModeLabels }
	};

#ifndef MKTURNOUT
static paramGroup_t turnDesignPG = { "turnoutNew", 0, turnDesignPLs, sizeof turnDesignPLs/sizeof turnDesignPLs[0] };

static turnoutInfo_t * customTurnout1, * customTurnout2;
static BOOL_T includeNontrackSegments;
#endif

#ifdef MKTURNOUT
int doCustomInfoLine = 1;
int doRoadBed = 0;
char specialLine[256];
#endif

static toDesignDesc_t * curDesign;

/*
 * Regular Turnouts
 */


static wLines_t RegLines[] = {
#include "toreg.lin"
		};
static toDesignFloat_t RegFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Diverging Length"), Dim_e },
{ { 400, 28 }, I_TOANGLE+0, N_("Angle"), N_("Diverging Angle"), Frog_e },
{ { 325, 68 }, I_TOOFFSET+0, N_("Offset"), N_("Diverging Offset"), Dim_e },
{ { 100, 120 }, I_TOLENGTH+2, N_("Length"), N_("Overall Length"), Dim_e },
		};
static signed char RegPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 3, 4, 0, 0, 0 };
static toDesignSchema_t RegSchema = {
		RegPaths,
		"030" "310" "341" "420" };
static toDesignDesc_t RegDesc = {
		NTO_REGULAR,
		N_("Regular Turnout"),
		2,
		sizeof RegLines/sizeof RegLines[0], RegLines,
		sizeof RegFloats/sizeof RegFloats[0], RegFloats,
		&RegSchema, 1 };

static wLines_t CrvLines[] = {
#include "tocrv.lin"
		};
static toDesignFloat_t CrvFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Inner Length"), Dim_e },
{ { 375, 12 }, I_TOANGLE+0, N_("Angle"), N_("Inner Angle"), Frog_e },
{ { 375, 34 }, I_TOOFFSET+0, N_("Offset"), N_("Inner Offset"), Dim_e },
{ { 400, 62 }, I_TOANGLE+1, N_("Angle"), N_("Outer Angle"), Frog_e },
{ { 400, 84 }, I_TOOFFSET+1, N_("Offset"), N_("Outer Offset"), Dim_e },
{ { 175, 120 }, I_TOLENGTH+1, N_("Length"), N_("Outer Length"), Dim_e } };
static signed char Crv1Paths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 4, 5, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 2, 3, 0, 0, 0 };
static toDesignSchema_t Crv1Schema = {
		Crv1Paths,
		"030" "341" "410" "362" "620" };
static signed char Crv2Paths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 4, 5, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 6, 2, 3, 0, 0, 0 };
static toDesignSchema_t Crv2Schema = {
		Crv2Paths,
		"050" "341" "410" "562" "620" "530" };
static signed char Crv3Paths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 6, 4, 5, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 2, 3, 0, 0, 0 };
static toDesignSchema_t Crv3Schema = {
		Crv3Paths,
		"030" "341" "410" "562" "620" "350" };

static toDesignDesc_t CrvDesc = {
		NTO_CURVED,
		N_("Curved Turnout"),
		2,
		sizeof CrvLines/sizeof CrvLines[0], CrvLines,
		sizeof CrvFloats/sizeof CrvFloats[0], CrvFloats,
		&Crv1Schema, 1 };


static wLines_t WyeLines[] = {
#include "towye.lin"
		};
static toDesignFloat_t WyeFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Left Length"), Dim_e },
{ { 400, 28 }, I_TOANGLE+0, N_("Angle"), N_("Left Angle"), Frog_e },
{ { 325, 68 }, I_TOOFFSET+0, N_("Offset"), N_("Left Offset"), Dim_e },
{ { 325, 115 }, I_TOOFFSET+1, N_("Offset"), N_("Right Offset"), Dim_e },
{ { 400, 153 }, I_TOANGLE+1, N_("Angle"), N_("Right Angle"), Frog_e },
{ { 175, 170 }, I_TOLENGTH+1, N_("Length"), N_("Right Length"), Dim_e },
		};
static signed char Wye1Paths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 4, 5, 0, 0, 0 };
static toDesignSchema_t Wye1Schema = {
		Wye1Paths,
		"030" "341" "410" "362" "620" };
static signed char Wye2Paths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 4, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 5, 6, 0, 0, 0 };
static toDesignSchema_t Wye2Schema = {
		Wye2Paths,
		"050" "530" "341" "410" "562" "620" };
static signed char Wye3Paths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 4, 5, 6, 0, 0, 0 };
static toDesignSchema_t Wye3Schema = {
		Wye3Paths,
		"030" "341" "410" "350" "562" "620" };
static toDesignDesc_t WyeDesc = {
		NTO_WYE,
		N_("Wye Turnout"),
		1,
		sizeof WyeLines/sizeof WyeLines[0], WyeLines,
		sizeof WyeFloats/sizeof WyeFloats[0], WyeFloats,
		NULL, 1 };

static wLines_t ThreewayLines[] = {
#include "to3way.lin"
		};
static toDesignFloat_t ThreewayFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Left Length"), Dim_e },
{ { 400, 28 }, I_TOANGLE+0, N_("Angle"), N_("Left Angle"), Frog_e },
{ { 325, 68 }, I_TOOFFSET+0, N_("Offset"), N_("Left Offset"), Dim_e },
{ { 100, 90 }, I_TOLENGTH+2, N_("Length"), N_("Length"), Dim_e },
{ { 325, 115 }, I_TOOFFSET+1, N_("Offset"), N_("Right Offset"), Dim_e },
{ { 400, 153 }, I_TOANGLE+1, N_("Angle"), N_("Right Angle"), Frog_e },
{ { 175, 170 }, I_TOLENGTH+1, N_("Length"), N_("Right Length"), Dim_e },
		};
static signed char Tri1Paths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 0, 0,
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 6, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 4, 5, 0, 0, 0 };
static toDesignSchema_t Tri1Schema = {
		Tri1Paths,
		"030" "341" "410" "362" "620" "370" };
static signed char Tri2Paths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 4, 0, 0,
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 7, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 5, 6, 0, 0, 0 };
static toDesignSchema_t Tri2Schema = {
		Tri2Paths,
		"050" "530" "341" "410" "562" "620" "370" };
static signed char Tri3Paths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 0, 0,
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 4, 7, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 4, 5, 6, 0, 0, 0 };
static toDesignSchema_t Tri3Schema = {
		Tri3Paths,
		"030" "341" "410" "350" "562" "620" "570" };
static toDesignDesc_t ThreewayDesc = {
		NTO_3WAY,
		N_("3-way Turnout"),
		1,
		sizeof ThreewayLines/sizeof ThreewayLines[0], ThreewayLines,
		sizeof ThreewayFloats/sizeof ThreewayFloats[0], ThreewayFloats,
		NULL, 1 };

static wLines_t CrossingLines[] = {
#include "toxing.lin"
		};
static toDesignFloat_t CrossingFloats[] = {
{ { 329, 30 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e },
{ { 370, 90 }, I_TOANGLE+0, N_("Angle"), N_("Angle"), Frog_e },
{ { 329, 150 }, I_TOLENGTH+1, N_("Length"), N_("Length"), Dim_e } };
static signed char CrossingPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 0, 2, 0, 0, 0 };
static toDesignSchema_t CrossingSchema = {
		CrossingPaths,
		"010" "230" };
static toDesignDesc_t CrossingDesc = {
		NTO_CROSSING,
		N_("Crossing"),
		1,
		sizeof CrossingLines/sizeof CrossingLines[0], CrossingLines,
		sizeof CrossingFloats/sizeof CrossingFloats[0], CrossingFloats,
		&CrossingSchema, 1 };

static wLines_t SingleSlipLines[] = {
#include "tosslip.lin"
		};
static toDesignFloat_t SingleSlipFloats[] = {
{ { 329, 30 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e },
{ { 370, 90 }, I_TOANGLE+0, N_("Angle"), N_("Angle"), Frog_e },
{ { 329, 155 }, I_TOLENGTH+1, N_("Length"), N_("Length"), Dim_e } };
static signed char SingleSlipPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 0, 3, 4, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 5, 4, 0, 0, 0 };
static toDesignSchema_t SingleSlipSchema = {
		SingleSlipPaths,
		"040" "410" "250" "530" "451" };
static toDesignDesc_t SingleSlipDesc = {
		NTO_S_SLIP,
		N_("Single Slipswitch"),
		1,
		sizeof SingleSlipLines/sizeof SingleSlipLines[0], SingleSlipLines,
		sizeof SingleSlipFloats/sizeof SingleSlipFloats[0], SingleSlipFloats,
		&SingleSlipSchema, 1 };

static wLines_t DoubleSlipLines[] = {
#include "todslip.lin"
		};
static toDesignFloat_t DoubleSlipFloats[] = {
{ { 329, 30 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e },
{ { 370, 90 }, I_TOANGLE+0, N_("Angle"), N_("Angle"), Frog_e },
{ { 329, 155 }, I_TOLENGTH+1, N_("Length"), N_("Length"), Dim_e } };
static signed char DoubleSlipPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 3, 0, 4, 5, 6, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 7, 6, 0, 4, 8, 3, 0, 0, 0 };
static toDesignSchema_t DoubleSlipSchema = {
		DoubleSlipPaths,
		"040" "460" "610" "270" "750" "530" "451" "762" };
static toDesignDesc_t DoubleSlipDesc = {
		NTO_D_SLIP,
		N_("Double Slipswitch"),
		1,
		sizeof DoubleSlipLines/sizeof DoubleSlipLines[0], DoubleSlipLines,
		sizeof DoubleSlipFloats/sizeof DoubleSlipFloats[0], DoubleSlipFloats,
		&DoubleSlipSchema, 1 };

static wLines_t RightCrossoverLines[] = {
#include "torcross.lin"
		};
static toDesignFloat_t RightCrossoverFloats[] = {
{ { 200, 10 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e },
{ { 90, 85 }, I_TOOFFSET+0, N_("Separation"), N_("Separation"), Dim_e } };
static signed char RightCrossoverPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 0, 3, 4, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 3, 5, 6, 7, 2, 0, 0, 0 };
static toDesignSchema_t RightCrossoverSchema = {
		RightCrossoverPaths,
		"060" "610" "280" "830" "892" "970" "761" };
static toDesignDesc_t RightCrossoverDesc = {
		NTO_R_CROSSOVER,
		N_("Right Crossover"),
		1,
		sizeof RightCrossoverLines/sizeof RightCrossoverLines[0], RightCrossoverLines,
		sizeof RightCrossoverFloats/sizeof RightCrossoverFloats[0], RightCrossoverFloats,
		&RightCrossoverSchema, 0 };

static wLines_t LeftCrossoverLines[] = {
#include "tolcross.lin"
		};
static toDesignFloat_t LeftCrossoverFloats[] = {
{ { 200, 10 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e },
{ { 90, 85 }, I_TOOFFSET+0, N_("Separation"), N_("Separation"), Dim_e } };
static signed char LeftCrossoverPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 0, 3, 4, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 5, 6, 7, 4, 0, 0, 0 };
static toDesignSchema_t LeftCrossoverSchema = {
		LeftCrossoverPaths,
		"040" "410" "2A0" "A30" "451" "5B0" "BA2" };
static toDesignDesc_t LeftCrossoverDesc = {
		NTO_L_CROSSOVER,
		N_("Left Crossover"),
		1,
		sizeof LeftCrossoverLines/sizeof LeftCrossoverLines[0], LeftCrossoverLines,
		sizeof LeftCrossoverFloats/sizeof LeftCrossoverFloats[0], LeftCrossoverFloats,
		&LeftCrossoverSchema, 0 };

static wLines_t DoubleCrossoverLines[] = {
#include "todcross.lin"
		};
static toDesignFloat_t DoubleCrossoverFloats[] = {
{ { 200, 10 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e },
{ { 90, 85 }, I_TOOFFSET+0, N_("Separation"), N_("Separation"), Dim_e } };
static signed char DoubleCrossoverPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 2, 3, 0, 4, 5, 6, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 7, 8, 9, 6, 0, 4, 10, 11, 12, 3, 0, 0, 0 };
static toDesignSchema_t DoubleCrossoverSchema = {
		DoubleCrossoverPaths,
		"040" "460" "610" "280" "8A0" "A30" "451" "5B0" "BA2" "892" "970" "761" };
static toDesignDesc_t DoubleCrossoverDesc = {
		NTO_D_CROSSOVER,
		N_("Double Crossover"),
		1,
		sizeof DoubleCrossoverLines/sizeof DoubleCrossoverLines[0], DoubleCrossoverLines,
		sizeof DoubleCrossoverFloats/sizeof DoubleCrossoverFloats[0], DoubleCrossoverFloats,
		&DoubleCrossoverSchema, 0 };

static wLines_t StrSectionLines[] = {
#include "tostrsct.lin"
		};
static toDesignFloat_t StrSectionFloats[] = {
{ { 200, 10 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e } };
static signed char StrSectionPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 0, 0, 0 };
static toDesignSchema_t StrSectionSchema = {
		StrSectionPaths,
		"010" };
static toDesignDesc_t StrSectionDesc = {
		NTO_STR_SECTION,
		N_("Straight Section"),
		1,
		sizeof StrSectionLines/sizeof StrSectionLines[0], StrSectionLines,
		sizeof StrSectionFloats/sizeof StrSectionFloats[0], StrSectionFloats,
		&StrSectionSchema, 0 };

static wLines_t CrvSectionLines[] = {
#include "tocrvsct.lin"
		};
static toDesignFloat_t CrvSectionFloats[] = {
{ { 225, 90 }, I_TOLENGTH+0, N_("Radius"), N_("Radius"), Dim_e },
{ { 225, 140}, I_TOANGLE+0, N_("Angle (Degrees)"), N_("Angle"), Angle_e } };
static signed char CrvSectionPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 0, 0, 0 };
static toDesignSchema_t CrvSectionSchema = {
		CrvSectionPaths,
		"011" };
static toDesignDesc_t CrvSectionDesc = {
		NTO_CRV_SECTION,
		N_("Curved Section"),
		1,
		sizeof CrvSectionLines/sizeof CrvSectionLines[0], CrvSectionLines,
		sizeof CrvSectionFloats/sizeof CrvSectionFloats[0], CrvSectionFloats,
		&CrvSectionSchema, 0 };

#ifdef LATER
static wLines_t BumperLines[] = {
#include "tostrsct.lin"
		};
static toDesignFloat_t BumperFloats[] = {
{ { 200, 10 }, I_TOLENGTH+0, N_("Length"), N_("Length"), Dim_e } };
static signed char BumperPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 0, 0, 0 };
static toDesignSchema_t BumperSchema = {
		BumperPaths,
		"010" };
static toDesignDesc_t BumperDesc = {
		NTO_BUMPER,
		N_("Bumper Section"),
		1,
		sizeof StrSectionLines/sizeof StrSectionLines[0], StrSectionLines,
		sizeof BumperFloats/sizeof BumperFloats[0], BumperFloats,
		&BumperSchema, 0 };

static wLines_t TurntableLines[] = {
#include "tostrsct.lin"
		};
static toDesignFloat_t TurntableFloats[] = {
{ { 200, 10 }, I_TOOFFSET+0, N_("Offset"), N_("Count"), 0 },
{ { 200, 10 }, I_TOLENGTH+0, N_("Length"), N_("Radius1"), Dim_e },
{ { 200, 10 }, I_TOLENGTH+1, N_("Length"), N_("Radius2"), Dim_e } };
static signed char TurntablePaths[] = {
		'1', 0, 1, 0, 0,
		'2', 0, 2, 0, 0,
		'3', 0, 3, 0, 0,
		'4', 0, 4, 0, 0,
		'5', 0, 5, 0, 0,
		'6', 0, 6, 0, 0,
		'7', 0, 7, 0, 0,
		'8', 0, 8, 0, 0,
		'9', 0, 9, 0, 0,
		'1', '0', 0, 10, 0, 0,
		'1', '1', 0, 11, 0, 0,
		'1', '2', 0, 12, 0, 0,
		'1', '3', 0, 13, 0, 0,
		'1', '4', 0, 14, 0, 0,
		'1', '5', 0, 15, 0, 0,
		'1', '6', 0, 16, 0, 0,
		'1', '7', 0, 17, 0, 0,
		'1', '8', 0, 18, 0, 0,
		'1', '9', 0, 19, 0, 0,
		'2', '0', 0, 20, 0, 0,
		'2', '1', 0, 21, 0, 0,
		'2', '2', 0, 22, 0, 0,
		'2', '3', 0, 23, 0, 0,
		'2', '4', 0, 24, 0, 0,
		'2', '5', 0, 25, 0, 0,
		'2', '6', 0, 26, 0, 0,
		'2', '7', 0, 27, 0, 0,
		'2', '8', 0, 28, 0, 0,
		'2', '9', 0, 29, 0, 0,
		'3', '0', 0, 30, 0, 0,
		'3', '1', 0, 31, 0, 0,
		'3', '2', 0, 32, 0, 0,
		'3', '3', 0, 33, 0, 0,
		'3', '4', 0, 34, 0, 0,
		'3', '5', 0, 35, 0, 0,
		'3', '6', 0, 36, 0, 0,
		'3', '7', 0, 37, 0, 0,
		'3', '8', 0, 38, 0, 0,
		'3', '9', 0, 39, 0, 0,
		'4', '0', 0, 40, 0, 0,
		'4', '1', 0, 41, 0, 0,
		'4', '2', 0, 42, 0, 0,
		'4', '3', 0, 43, 0, 0,
		'4', '4', 0, 44, 0, 0,
		'4', '5', 0, 45, 0, 0,
		'4', '6', 0, 46, 0, 0,
		'4', '7', 0, 47, 0, 0,
		'4', '8', 0, 48, 0, 0,
		'4', '9', 0, 49, 0, 0,
		'5', '0', 0, 50, 0, 0,
		'5', '1', 0, 51, 0, 0,
		'5', '2', 0, 52, 0, 0,
		'5', '3', 0, 53, 0, 0,
		'5', '4', 0, 54, 0, 0,
		'5', '5', 0, 55, 0, 0,
		'5', '6', 0, 56, 0, 0,
		'5', '7', 0, 57, 0, 0,
		'5', '8', 0, 58, 0, 0,
		'5', '9', 0, 59, 0, 0,
		'6', '0', 0, 60, 0, 0,
		'6', '1', 0, 61, 0, 0,
		'6', '2', 0, 62, 0, 0,
		'6', '3', 0, 63, 0, 0,
		'6', '4', 0, 64, 0, 0,
		'6', '5', 0, 65, 0, 0,
		'6', '6', 0, 66, 0, 0,
		'6', '7', 0, 67, 0, 0,
		'6', '8', 0, 68, 0, 0,
		'6', '9', 0, 69, 0, 0,
		'7', '0', 0, 70, 0, 0,
		'7', '1', 0, 71, 0, 0,
		'7', '2', 0, 72, 0, 0,
		 0 };
static toDesignSchema_t TurntableSchema = {
		TurntablePaths,
		"010" "020" "030" "040" "050" "060" "070" "080" "090" "0A0" "0B0" };
static toDesignDesc_t TurntableDesc = {
		NTO_TURNTABLE,
		N_("Turntable Section"),
		1,
		sizeof StrSectionLines/sizeof StrSectionLines[0], StrSectionLines,
		sizeof TurntableFloats/sizeof TurntableFloats[0], TurntableFloats,
		&TurntableSchema, 0 };
#endif

#ifndef MKTURNOUT
static toDesignDesc_t * designDescs[] = {
		&RegDesc,
		&CrvDesc,
		&WyeDesc,
		&ThreewayDesc,
		&CrossingDesc,
		&SingleSlipDesc,
		&DoubleSlipDesc,
		&RightCrossoverDesc,
		&LeftCrossoverDesc,
		&DoubleCrossoverDesc,
		&StrSectionDesc,
		&CrvSectionDesc };
#endif

/**************************************************************************
 *
 *  Compute Roadbed
 *
 */

int debugComputeRoadbed = 0;
#ifdef LATER
typedef struct {
		int start;
		unsigned long bits;
		unsigned long mask;
		int width;
		} searchTable_t;
static searchTable_t searchTable[] = {
		{ 0,    0xFFFF0000, 0xFFFF0000, 32000} ,
		{ 32,   0x0000FFFF, 0x0000FFFF, 32000} ,

		{ 16,   0x00FFFF00, 0x00FFFF00, 16} ,

		{ 8,    0x0FF00000, 0x0FF00000, 8} ,
		{ 24,   0x00000FF0, 0x00000FF0, 8} ,

		{ 4,    0x3C000000, 0x3C000000, 4} ,
		{ 12,   0x003C0000, 0x003C0000, 4} ,
		{ 20,   0x00003C00, 0x00003C00, 4} ,
		{ 28,   0x0000003C, 0x0000003C, 4} ,

		{ 2,    0x60000000, 0x60000000, 2} ,
		{ 6,    0x06000000, 0x06000000, 2},
		{ 10,   0x00600000, 0x00600000, 2},
		{ 14,   0x00060000, 0x00060000, 2},
		{ 18,   0x00006000, 0x00006000, 2},
		{ 22,   0x00000600, 0x00000600, 2},
		{ 26,   0x00000060, 0x00000060, 2},
		{ 30,   0x00000006, 0x00000006, 2},

		{ 1,    0x40000000, 0x60000000, 1},
		{ 3,    0x10000000, 0x30000000, 1},
		{ 5,    0x04000000, 0x06000000, 1},
		{ 7,    0x01000000, 0x03000000, 1},
		{ 9,    0x00400000, 0x00600000, 1},
		{ 11,   0x00100000, 0x00300000, 1},
		{ 13,   0x00040000, 0x00060000, 1},
		{ 15,   0x00010000, 0x00030000, 1},
		{ 17,   0x00004000, 0x00006000, 1},
		{ 19,   0x00001000, 0x00003000, 1},
		{ 21,   0x00000400, 0x00000600, 1},
		{ 23,   0x00000100, 0x00000300, 1},
		{ 25,   0x00000040, 0x00000060, 1},
		{ 27,   0x00000010, 0x00000030, 1},
		{ 29,   0x00000004, 0x00000006, 1},
		{ 31,   0x00000001, 0x00000003, 1}};
#endif


double LineSegDistance( coOrd p, coOrd p0, coOrd p1 )
{
	double d, a;
	coOrd pp, zero;
	zero.x = zero.y = (POS_T)0.0;
	d = FindDistance( p0, p1 );
	a = FindAngle( p0, p1 );
	pp.x = p.x-p0.x;
	pp.y = p.y-p0.y;
	Rotate( &pp, zero, -a );
	if (pp.y < 0.0-EPSILON) {
		return FindDistance( p, p0 );
	} else if (pp.y > d+EPSILON ) {
		return FindDistance( p, p1 );
	} else {
		return pp.x>=0? pp.x : -pp.x;
	}
}



double CircleSegDistance( coOrd p, coOrd c, double r, double a0, double a1 )
{
	double d, d0, d1;
	double a,aa;
	coOrd p1;

	d = FindDistance( c, p );
	a = FindAngle( c, p );
	aa = NormalizeAngle( a - a0 );
	d -= r;
	if ( aa <= a1 ) {
		return d>=0 ? d : -d;
	}
	PointOnCircle( &p1, c, r, a0 );
	d0 = FindDistance( p, p1 );
	PointOnCircle( &p1, c, r, a0+a1 );
	d1 = FindDistance( p, p1 );
	if (d0 < d1)
		return d0;
	else
		return d1;
}


BOOL_T HittestTurnoutRoadbed(
		trkSeg_p segPtr,
		int segCnt,
		int segInx,
		ANGLE_T side,
		int fraction,
		DIST_T roadbedWidth )
{
	ANGLE_T a;
	DIST_T d;
	int inx;
	trkSeg_p sp;
	coOrd p0, p1;
	DIST_T dd;
	int closest;

	sp = &segPtr[segInx];
	if (sp->type == SEG_STRTRK) {
		d = FindDistance( sp->u.l.pos[0], sp->u.l.pos[1] );
		a = FindAngle( sp->u.l.pos[0], sp->u.l.pos[1] );
		d *= (fraction*2+1)/64.0;
		Translate( &p0, sp->u.l.pos[0], a, d );
		Translate( &p0, p0, a+side, roadbedWidth/2.0 );
	} else {
		d = sp->u.c.radius;
		if ( d < 0 ) {
			d = -d;
			fraction = 31-fraction;
		}
		a = sp->u.c.a0 + sp->u.c.a1*(fraction*2+1)/64.0;
		if (side>0)
			d += roadbedWidth/2.0;
		else
			d -= roadbedWidth/2.0;
		PointOnCircle( &p0, sp->u.c.center, d, a );
	}
	dd = 100000.0;
	closest = -1;
	for (inx=0; inx<segCnt; inx++) {
		sp = &segPtr[inx];
		p1 = p0;
		switch( sp->type ) {
		case SEG_STRTRK:
			d = LineSegDistance( p1, sp->u.l.pos[0], sp->u.l.pos[1] );
			break;
		case SEG_CRVTRK:
			d = CircleSegDistance( p1, sp->u.c.center, fabs(sp->u.c.radius), sp->u.c.a0, sp->u.c.a1 );
			break;
		default:
			continue;
		}
#ifdef LATER
		if (inx==segInx)
			d *= .999;
#endif
		if ( d < dd ) {
			dd = d;
			closest = inx;
		}
	}
	if (closest == segInx)
		return FALSE;
	else
		return TRUE;
}

#ifdef LATER
EXPORT long ComputeTurnoutRoadbedSide(
		trkSeg_p segPtr,
		int segCnt,
		int segInx,
		ANGLE_T side,
		DIST_T roadbedWidth )
{
	DIST_T length;
	int rbw;
	unsigned long res, res1;
	searchTable_t * p;
	double where;
	trkSeg_p sp;

	sp = &segPtr[segInx];
	if (sp->type == SEG_STRTRK)
		length = FindDistance( sp->u.l.pos[0], sp->u.l.pos[1] );
	else
		length = (fabs(sp->u.c.radius) + (side>0?roadbedWidth/2.0:-roadbedWidth/2.0) ) * 2 * M_PI * sp->u.c.a1 / 360.0;
	rbw = (int)(roadbedWidth/length*32/2);
/*printf( "L=%0.3f G=%0.3f [%0.3f %0.3f] RBW=%d\n", length, gapWidth, first, last, rbw );*/
	res = 0xFF0000FF;
	for ( p=searchTable; p<&searchTable[sizeof searchTable/sizeof searchTable[0]]; p++) {
		if ( (p->width < rbw && res==0xFFFFFFFF) || res==0 )
			break;
		res1 = (p->mask & res);
		where = p->start*length/32.0;
		if (p->width >= rbw || (res1!=p->mask && res1!=0)) {
			if (HittestTurnoutRoadbed(segPtr, segCnt, segInx, side, p->start)) {
						res &= ~p->bits;
if (debugComputeRoadbed>=1) printf( "res=%08lx *p={%02d %08lx %08lx %02d} res1=%08lx W=%0.3f HIT\n", res, p->start, p->bits, p->mask, p->width, res1, where );
			} else {
						res |= p->bits;
if (debugComputeRoadbed>=1) printf( "res=%08lx *p={%02d %08lx %08lx %02d} res1=%08lx W=%0.3f MISS\n", res, p->start, p->bits, p->mask, p->width, res1, where );
			}
		} else {
if (debugComputeRoadbed>=2) printf( "res=%08lx *p={%02d %08lx %08lx %02d} res1=%08lx W=%0.3f SKIP\n", res, p->start, p->bits, p->mask, p->width, res1, where );
		}
	}
if (debugComputeRoadbed>=1) printf( "res=%08lx\n", res );
	return res;
}
#endif


EXPORT long ComputeTurnoutRoadbedSide(
		trkSeg_p segPtr,
		int segCnt,
		int segInx,
		ANGLE_T side,
		DIST_T roadbedWidth )
{
	trkSeg_p sp;
	DIST_T length;
	int bitWidth;
	unsigned long res, mask;
	int hit0, hit1, inx0, inx1;
	int i, j, k, hitx;

	sp = &segPtr[segInx];
	if (sp->type == SEG_STRTRK)
		length = FindDistance( sp->u.l.pos[0], sp->u.l.pos[1] );
	else
		length = (fabs(sp->u.c.radius) + (side>0?roadbedWidth/2.0:-roadbedWidth/2.0) ) * 2 * M_PI * sp->u.c.a1 / 360.0;
	bitWidth = (int)floor(roadbedWidth*32/length);
	if ( bitWidth > 31 )
		bitWidth = 31;
	else if ( bitWidth <= 0 )
		bitWidth = 2;
	res = 0;
	mask = (1<<bitWidth)-1;
	hit0 = HittestTurnoutRoadbed( segPtr, segCnt, segInx, side, 0, roadbedWidth );
	inx0 = 0;
	inx1 = bitWidth;
if ( debugComputeRoadbed>=3 ) printf( "bW=%d HT[0]=%d\n", bitWidth, hit0 );
	while ( 1 ) {
		if ( inx1 > 31 )
			inx1 = 31;
		hit1 = HittestTurnoutRoadbed( segPtr, segCnt, segInx, side, inx1, roadbedWidth );
if ( debugComputeRoadbed>=3 ) printf( "     HT[%d]=%d\n", inx1, hit1 );
		if ( hit0 != hit1 ) {
			i=inx0;
			j=inx1;
			while ( j-i >= 2 ) {
				k = (i+j)/2;
				hitx = HittestTurnoutRoadbed( segPtr, segCnt, segInx, side, k, roadbedWidth );
if ( debugComputeRoadbed>=3 ) printf( "     .HT[%d]=%d\n", k, hitx );
				if ( hitx == hit0 )
					i = k;
				else
					j = k;
			}
			if ( !hit0 ) {
				res |= ((1<<(i-inx0+1))-1)<<inx0;
			} else {
				res |= ((1<<(inx1-j))-1)<<j;
			}
		} else if ( !hit1 ) {
			res |= mask;
		}
if ( debugComputeRoadbed>=3 ) printf( "  res=%lx\n", res );
		if ( inx1 >= 31 ) {
			if ( !hit1 )
				res |= 0x80000000;
			break;
		}
		mask <<= bitWidth;
		inx0 = inx1;
		inx1 += bitWidth;
		hit0 = hit1;
	}
if ( debugComputeRoadbed>=2 ) printf( "S%d %c    res=%lx\n", segInx, side>0?'+':'-', res );
	return res;
}


static BOOL_T IsNear( coOrd p0, coOrd p1 )
{
	DIST_T d;
	d = FindDistance( p0, p1 );
	return d < 0.05;
}


static void AddRoadbedPieces(
		int inx,
		ANGLE_T side,
		int first,
		int last )
{
	DIST_T d0, d1;
	ANGLE_T a0, a1;
	coOrd p0, p1;
	trkSeg_p sp, sq;
#ifdef MKTURNOUT
#define _DPI (76.0)
#else
#define _DPI mainD.dpi
#endif

	if (last<=first)
		return;
	sp = &tempSegs(inx);
	if ( sp->type == SEG_STRTRK ) {
		d0 = FindDistance( sp->u.l.pos[0], sp->u.l.pos[1] );
		a0 = FindAngle( sp->u.l.pos[0], sp->u.l.pos[1] );
		d1 = d0*first/32.0;
		Translate( &p0, sp->u.l.pos[0], a0, d1 );
		Translate( &p0, p0, a0+side, newTurnRoadbedWidth/2.0 );
		d1 = d0*last/32.0;
		Translate( &p1, sp->u.l.pos[0], a0, d1 );
		Translate( &p1, p1, a0+side, newTurnRoadbedWidth/2.0 );
		if ( first==0 || last==32 ) {
			for ( sq=&tempSegs(0); sq<&tempSegs(tempSegs_da.cnt); sq++ ) {
				if ( sq->type == SEG_STRLIN ) {
					a1 = FindAngle( sq->u.l.pos[0], sq->u.l.pos[1] );
					a1 = NormalizeAngle( a1-a0+0.5 );
					if ( first==0 ) {
						if ( a1 < 1.0 && IsNear( p0, sq->u.l.pos[1] ) ) {
							 sq->u.l.pos[1] = p1;
							 return;
						} else if ( a1 > 180.0 && a1 < 181.0 && IsNear( p0, sq->u.l.pos[0] ) ) {
							 sq->u.l.pos[0] = p1;
							 return;
						}
					}
					if ( last==32 ) {
						if ( a1 < 1.0 && IsNear( p1, sq->u.l.pos[0] ) ) {
							 sq->u.l.pos[0] = p0;
							 return;
						} else if ( a1 > 180.0 && a1 < 181.0 && IsNear( p1, sq->u.l.pos[1] ) ) {
							 sq->u.l.pos[1] = p0;
							 return;
						}
					}
				}
			}
		}
	}
	DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
	sp = &tempSegs(inx);
	sq = &tempSegs(tempSegs_da.cnt-1);
	sq->width = newTurnRoadbedLineWidth/(_DPI);
	sq->color = roadbedColor;
	if (sp->type == SEG_STRTRK) {
		sq->type = SEG_STRLIN;
		sq->u.l.pos[0] = p0;
		sq->u.l.pos[1] = p1;
	} else {
		d0 = sp->u.c.radius;
		if ( d0 > 0 ) {
			a0 = NormalizeAngle( sp->u.c.a0 + sp->u.c.a1*first/32.0 );
		} else {
			d0 = -d0;
			a0 = NormalizeAngle( sp->u.c.a0 + sp->u.c.a1*(32-last)/32.0 );
		}
		a1 = sp->u.c.a1*(last-first)/32.0;
		if (side>0)
			d0 += newTurnRoadbedWidth/2.0;
		else
			d0 -= newTurnRoadbedWidth/2.0;
		sq->type = SEG_CRVLIN;
		sq->u.c.center = sp->u.c.center;
		sq->u.c.radius = d0;
		sq->u.c.a0 = a0;
		sq->u.c.a1 = a1;
	}
}


static void AddRoadbedToOneSide(
		int trkCnt,
		int inx,
		ANGLE_T side )
{
	unsigned long res, res1;
	int b0, b1;

	res = ComputeTurnoutRoadbedSide( &tempSegs(0), trkCnt, inx, side, newTurnRoadbedWidth );
	if ( res == 0L ) {
		return;
	} else if ( res == 0xFFFFFFFF ) {
		AddRoadbedPieces( inx, side, 0, 32 );
	} else {
		for ( b0=0, res1=0x00000001; res1&&(res1&res); b0++,res1<<=1 );
		for ( b1=32,res1=0x80000000; res1&&(res1&res); b1--,res1>>=1 );
		AddRoadbedPieces( inx, side, 0, b0 );
		AddRoadbedPieces( inx, side, b1, 32 );
	}
}


static void AddRoadbed( void )
{
	int trkCnt, inx;
	trkSeg_p sp;
	if ( newTurnRoadbedWidth < newTurnTrackGauge )
		return;
	trkCnt = tempSegs_da.cnt;
	for ( inx=0; inx<trkCnt; inx++ ) {
		sp = &tempSegs(inx);
		if ( sp->type!=SEG_STRTRK && sp->type!=SEG_CRVTRK )
			continue;
		AddRoadbedToOneSide( trkCnt, inx, +90 );
		AddRoadbedToOneSide( trkCnt, inx, -90 );
	}
}


/*********************************************************************
 *
 * Functions
 *
 */

static BOOL_T ComputeCurve(
		coOrd *p0, coOrd *p1, DIST_T *radius,
		DIST_T len, DIST_T off, ANGLE_T angle )
{
	coOrd Pf;
	coOrd Px, Pc;
	DIST_T d;

	Pf.x = len;
	Pf.y = off;
	p0->x = p0->y = 0.0;
	/*lprintf( "Angle = %0.3f\n", angle );*/
	FindIntersection( &Px, *p0, 90.0, Pf, 90.0-angle );
	d = FindDistance( Px, Pf )-newTurnTrackGauge;
	if (Px.x < newTurnTrackGauge || d < 0.0) {
		NoticeMessage( MSG_TODSGN_NO_CONVERGE, _("Ok"), NULL );
		return FALSE;
	}
	if (Px.x-newTurnTrackGauge < d)
		d = Px.x-newTurnTrackGauge;
	*radius = d * cos( D2R(angle/2.0) ) / sin( D2R(angle/2.0) );

	p0->x = Px.x - *radius * sin( D2R(angle/2.0) ) / cos( D2R(angle/2.0) );
	Translate( &Pc, *p0, 0.0, *radius );
	PointOnCircle( p1, Pc, *radius, 180.0-angle );

	return TRUE;
}



static toDesignSchema_t * LoadSegs(
		toDesignDesc_t * dp,
		wBool_t loadPoints,
		wIndex_t * pathLenP )
{
	wIndex_t s;
	int i, p, p0, p1;
	DIST_T d;
#ifndef MKTURNOUT
	wIndex_t pathLen;
#endif
	toDesignSchema_t * pp;
	char *segOrder;
	coOrd pos;
	wIndex_t segCnt;
	ANGLE_T angle1, angle2;
	trkSeg_p segPtr;

	DYNARR_RESET( trkSeg_t, tempSegs_da );
	angle1 = newTurnAngle1;
	angle2 = newTurnAngle2;
	if ( newTurnAngleMode == 0 && dp->type != NTO_CRV_SECTION ) {
		/* convert from Frog Num to degrees */
		if ( angle1 > 0 )
			angle1 = R2D(asin(1.0 / angle1));
		if ( angle2 > 0 )
			angle2 = R2D(asin(1.0 / angle2));
	}

	pp = dp->paths;
	if (loadPoints) {
		DYNARR_RESET( trkEndPt_t, tempEndPts_da );
		for ( i=0; i<dp->floatCnt; i++ )
			if ( *(FLOAT_T*)(turnDesignPLs[dp->floats[i].index].valueP) == 0.0 ) {
				NoticeMessage( MSG_TODSGN_VALUES_GTR_0, _("Ok"), NULL );
				return NULL;
			}

		switch (dp->type) {
		case NTO_REGULAR:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 3 );
			if ( !ComputeCurve( &points[3], &points[4], &radii[0],
				(newTurnLen1), (newTurnOff1), angle1 ) )
				return NULL;
			radii[0] = - radii[0];
			points[0].x = points[0].y = points[1].y = 0.0;
			points[1].x = (newTurnLen0);
			points[2].y = (newTurnOff1);
			points[2].x = (newTurnLen1);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 90.0-angle1;
			break;

		case NTO_CURVED:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 3 );
			if ( !ComputeCurve( &points[3], &points[4], &radii[0],
				(newTurnLen1), (newTurnOff1), angle1 ) )
				return NULL;
			if ( !ComputeCurve( &points[5], &points[6], &radii[1],
				(newTurnLen2), (newTurnOff2), angle2 ) )
				return NULL;
			d = points[3].x - points[5].x;
			if ( d < -0.10 )
				pp = &Crv3Schema;
			else if ( d > 0.10 )
				pp = &Crv2Schema;
			else
				pp = &Crv1Schema;
			radii[0] = - radii[0];
			radii[1] = - radii[1];
			points[0].x = points[0].y = 0.0;
			points[1].y = (newTurnOff1); points[1].x = (newTurnLen1);
			points[2].y = (newTurnOff2); points[2].x = (newTurnLen2);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(2).pos = points[1]; tempEndPts(2).angle = 90.0-angle1;
			tempEndPts(1).pos = points[2]; tempEndPts(1).angle = 90.0-angle2;
			break;

		case NTO_WYE:
		case NTO_3WAY:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, (dp->type==NTO_3WAY)?4:3 );
			if ( !ComputeCurve( &points[3], &points[4], &radii[0],
						(newTurnLen1), (newTurnOff1), angle1 ) )
				return NULL;
			if ( !ComputeCurve( &points[5], &points[6], &radii[1],
						(newTurnLen2), (newTurnOff2), angle2 ) )
				return NULL;
			points[5].y = - points[5].y;
			points[6].y = - points[6].y;
			radii[0] = - radii[0];
			points[0].x = points[0].y = 0.0;
			points[1].y = (newTurnOff1);
			points[1].x = (newTurnLen1);
			points[2].y = -(newTurnOff2);
			points[2].x = (newTurnLen2);
			points[7].y = 0;
			points[7].x = (newTurnLen0);
			d = points[3].x - points[5].x;
			if ( d < -0.10 ) {
				pp = (dp->type==NTO_3WAY ? &Tri3Schema : &Wye3Schema );
			} else if ( d > 0.10 ) {
				pp = (dp->type==NTO_3WAY ? &Tri2Schema : &Wye2Schema );
			} else {
				pp = (dp->type==NTO_3WAY ? &Tri1Schema : &Wye1Schema );
			}
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0-angle1;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 90.0+angle2;
			if (dp->type == NTO_3WAY) {
				tempEndPts(3).pos = points[7]; tempEndPts(3).angle = 90.0;
			}
			break;

		case NTO_D_SLIP:
		case NTO_S_SLIP:
		case NTO_CROSSING:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 4 );
			points[0].x = points[0].y = points[1].y = 0.0;
			points[1].x = (newTurnLen1);
			pos.y = 0; pos.x = (newTurnLen1)/2.0;
			Translate( &points[3], pos, 90.0+angle1, (newTurnLen2)/2.0 );
			points[2].y = - points[3].y;
			points[2].x = (newTurnLen1)-points[3].x;
			if (dp->type != NTO_CROSSING) {
				Translate( &pos, points[3], 90.0+angle1, -newTurnTrackGauge );
				if (!ComputeCurve( &points[4], &points[5], &radii[0],
						pos.x, fabs(pos.y), angle1 )) /*???*/
					return NULL;
				radii[1] = - radii[0];
				points[5].y = - points[5].y;
				points[6].y = 0; points[6].x = (newTurnLen1)-points[4].x;
				points[7].y = -points[5].y;
				points[7].x = (newTurnLen1)-points[5].x;
			}
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 270.0+angle1;
			tempEndPts(3).pos = points[3]; tempEndPts(3).angle = 90.0+angle1;
			break;

		case NTO_R_CROSSOVER:
		case NTO_L_CROSSOVER:
		case NTO_D_CROSSOVER:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 4 );
			d = (newTurnLen1)/2.0 - newTurnTrackGauge;
			if (d < 0.0) {
				NoticeMessage( MSG_TODSGN_CROSSOVER_TOO_SHORT, _("Ok"), NULL );
				return NULL;
			}
			angle1 = R2D( atan2( (newTurnOff1), d ) );
			points[0].y = 0.0; points[0].x = 0.0;
			points[1].y = 0.0; points[1].x = (newTurnLen1);
			points[2].y = (newTurnOff1); points[2].x = 0.0;
			points[3].y = (newTurnOff1); points[3].x = (newTurnLen1);
			if (!ComputeCurve( &points[4], &points[5], &radii[1],
				(newTurnLen1)/2.0, (newTurnOff1)/2.0, angle1 ) )
				return NULL;
			radii[0] = - radii[1];
			points[6].y = 0.0; points[6].x = (newTurnLen1)-points[4].x;
			points[7].y = points[5].y; points[7].x = (newTurnLen1)-points[5].x;
			points[8].y = (newTurnOff1); points[8].x = points[4].x;
			points[9].y = (newTurnOff1)-points[5].y; points[9].x = points[5].x;
			points[10].y = (newTurnOff1); points[10].x = points[6].x;
			points[11].y = points[9].y; points[11].x = points[7].x;
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 270.0;
			tempEndPts(3).pos = points[3]; tempEndPts(3).angle = 90.0;
			break;

		case NTO_STR_SECTION:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 2 );
			points[0].y = points[0].x = 0;
			points[1].y = 0/*(newTurnOff1)*/; points[1].x = (newTurnLen1);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			break;

		case NTO_CRV_SECTION:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 2 );
			points[0].y = points[0].x = 0;
			points[1].y = (newTurnLen1) * (1.0 - cos( D2R(angle1) ) );
			points[1].x = (newTurnLen1) * sin( D2R(angle1) );
			radii[0] = -(newTurnLen1);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0-angle1;
			break;

		case NTO_BUMPER:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 1 );
			points[0].y = points[0].x = 0;
			points[1].y = 0/*(newTurnOff1)*/; points[1].x = (newTurnLen1);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			break;

		default:
			;
		}
	} else {
		switch (dp->type) {
		case NTO_CURVED:
			d = points[3].x - points[5].x;
			if ( d < -0.10 )
				pp = &Crv3Schema;
			else if ( d > 0.10 )
				pp = &Crv2Schema;
			else
				pp = &Crv1Schema;
			break;
		}
	}

	segOrder = pp->segOrder;
	segCnt = strlen( segOrder );
	if (segCnt%3 != 0)
		AbortProg( dp->label );
	segCnt /= 3;
	DYNARR_SET( trkSeg_t, tempSegs_da, segCnt );
	tempSegs_da.cnt = segCnt;
	memset( &tempSegs(0), 0, segCnt * sizeof tempSegs(0) );
	for ( s=0; s<segCnt; s++ ) {
		segPtr = &tempSegs(s);
		segPtr->color = wDrawColorBlack;
		if (*segOrder <= '9')
			p0 = *segOrder++ - '0';
		else
			p0 = *segOrder++ - 'A' + 10;
		if (*segOrder <= '9')
			p1 = *segOrder++ - '0';
		else
			p1 = *segOrder++ - 'A' + 10;
		p = *segOrder++ - '0';
		if (p != 0) {
			segPtr->type = SEG_CRVTRK;
			ComputeCurvedSeg( segPtr, radii[p-1], points[p0], points[p1] );
		} else {
			segPtr->type = SEG_STRTRK;
			segPtr->u.l.pos[0] = points[p0];
			segPtr->u.l.pos[1] = points[p1];
		}
	}

	AddRoadbed();

#ifndef MKTURNOUT
	if ( (pathLen=CheckPaths( segCnt, &tempSegs(0), pp->paths )) < 0 )
		return NULL;

	if (pathLenP)
		*pathLenP = pathLen;
#endif
	return pp;
}


static void CopyNonTracks( turnoutInfo_t * to )
{
	trkSeg_p sp0;
	for ( sp0=to->segs; sp0<&to->segs[to->segCnt]; sp0++ ) {
		if ( sp0->type != SEG_STRTRK && sp0->type != SEG_CRVTRK ) {
			DYNARR_APPEND( trkSeg_t, tempSegs_da, 10 );
			tempSegs(tempSegs_da.cnt-1) = *sp0;
		}
	}
}


#ifndef MKTURNOUT
static void NewTurnPrint(
		void * junk )
{
	coOrd pos, p0, p1;
	WDOUBLE_T px, py;
	int i, j, ii, jj, p;
	EPINX_T ep;
	wFont_p fp;
	coOrd orig, size;
	toDesignSchema_t * pp;
	POS_T tmp;
	FLOAT_T tmpR;
	static drawCmd_t newTurnout_d = {
		NULL,
		&printDrawFuncs,
		DC_PRINT,
		1.0,
		0.0,
		{ 0.0, 0.0 },
		{ 0.0, 0.0 },
		Pix2CoOrd, CoOrd2Pix };

	if ((pp=LoadSegs( curDesign, TRUE, NULL )) == NULL)
		return;
	if (includeNontrackSegments && customTurnout1)
		CopyNonTracks( customTurnout1 );

	GetSegBounds( zero, 0.0, tempSegs_da.cnt, &tempSegs(0), &orig, &size );
	tmp = orig.x; orig.x = orig.y; orig.y = tmp;
#ifdef LATER
	size.x = 0.0; size.y = 0.0;
	orig.x = 0.0; orig.y = 0.0;
	for ( i=0; i<tempSegs_da.cnt; i++ ) {
		segPtr = &tempSegs(i);
		switch (segPtr->type) {
		case SEG_STRLIN:
		case SEG_STRTRK:
			pos[0] = segPtr->u.l.pos[0];
			pos[1] = segPtr->u.l.pos[1];
			break;
		case SEG_CRVTRK:
		case SEG_CRVLIN:
			PointOnCircle( &pos[0], segPtr->u.c.center, segPtr->u.c.radius,
						segPtr->u.c.a0 );
			PointOnCircle( &pos[1], segPtr->u.c.center, segPtr->u.c.radius,
						segPtr->u.c.a0+segPtr->u.c.a1 );
		}
		for ( ep=0; ep<2; ep++ ) {
			if (pos[ep].x < orig.x)
				orig.x = pos[ep].x;
			if (pos[ep].x > size.x)
				size.x = pos[ep].x;
			if (pos[ep].y < orig.y)
				orig.y = pos[ep].y;
			if (pos[ep].y > size.y)
				size.y = pos[ep].y;
		}
	}

	size.x -= orig.x;
	size.y -= orig.y;
#endif

	fp = wStandardFont( F_TIMES, FALSE, FALSE );
	wPrintGetPageSize( &px, &py );
	newTurnout_d.size.x = px;
	newTurnout_d.size.y = py;
	ii = (int)(size.y/newTurnout_d.size.x)+1;
	jj = (int)(size.x/newTurnout_d.size.y)+1;
	if ( !wPrintDocStart( sTurnoutDesignerW, ii*jj, NULL ) )
		return;
#ifdef LATER
	orig.x -= (0.5);
	orig.y -= (jj*newTurnout_d.size.y-size.y)/2.0;
#endif
	orig.x = - ( size.y + orig.x + newTurnTrackGauge/2.0 + 0.5 );
	orig.y -= (0.5);
	for ( i=0, newTurnout_d.orig.x=orig.x; i<ii;
		  i++, newTurnout_d.orig.x+=newTurnout_d.size.x ) {
		for ( j=0, newTurnout_d.orig.y=orig.y; j<jj;
			  j++, newTurnout_d.orig.y+=newTurnout_d.size.y ) {
			newTurnout_d.d = wPrintPageStart();
			newTurnout_d.dpi = wDrawGetDPI(newTurnout_d.d);

			sprintf( message, "%s", sProdName );
			wDrawString( newTurnout_d.d, POSX(3.0),
						POSY(6.75), 0.0, message, fp, 40,
						wDrawColorBlack, 0 );
			sprintf( message, _("%s Designer"), _(curDesign->label) );
			wDrawString( newTurnout_d.d, POSX(3.0),
						POSY(6.25), 0.0, message, fp, 30,
						wDrawColorBlack, 0 );
			sprintf( message, _("%s %d x %d (of %d x %d)"), _("Page"), i+1, j+1, ii, jj );
			wDrawString( newTurnout_d.d, POSX(3.0),
						POSY(5.75), 0.0, message, fp, 20,
						wDrawColorBlack, 0 );

			for ( p=0; p<curDesign->floatCnt; p++ ) {
				tmpR = *(FLOAT_T*)(turnDesignPLs[curDesign->floats[p].index].valueP);
				sprintf( message, "%s: %s",
						(curDesign->floats[p].mode!=Frog_e||newTurnAngleMode!=0)?_(curDesign->floats[p].printLabel):_("Frog Number"),
						curDesign->floats[p].mode==Dim_e?
							 FormatDistance(tmpR):
							 FormatFloat(tmpR) );
				wDrawString( newTurnout_d.d, POSX(3.0),
							  POSY(5.50-p*0.25), 0.0,
							  message, fp, 20, wDrawColorBlack, 0 );
			}
			if (newTurnLeftDesc[0] || newTurnLeftPartno[0]) {
				sprintf( message, "%s %s %s", newTurnManufacturer, newTurnLeftPartno, newTurnLeftDesc );
				wDrawString( newTurnout_d.d, POSX(3.0),
							  POSY(5.50-curDesign->floatCnt*0.25), 0.0,
							  message, fp, 20, wDrawColorBlack, 0 );
			}
			if (newTurnRightDesc[0] || newTurnRightPartno[0]) {
				sprintf( message, "%s %s %s", newTurnManufacturer, newTurnRightPartno, newTurnRightDesc );
				wDrawString( newTurnout_d.d, POSX(3.0),
							  POSY(5.50-curDesign->floatCnt*0.25-0.25), 0.0,
							  message, fp, 20, wDrawColorBlack, 0 );
			}

			wDrawLine( newTurnout_d.d, POSX(0), POSY(0),
						POSX(newTurnout_d.size.x), POSY(0), 0, wDrawLineSolid,
						wDrawColorBlack, 0 );
			wDrawLine( newTurnout_d.d, POSX(newTurnout_d.size.x), POSY(0.0),
						POSX(newTurnout_d.size.x), POSY(newTurnout_d.size.y), 0,
						wDrawLineSolid, wDrawColorBlack, 0 );
			wDrawLine( newTurnout_d.d, POSX(newTurnout_d.size.x), POSY(newTurnout_d.size.y),
						POSX(0.0), POSY(newTurnout_d.size.y), 0, wDrawLineSolid,
						wDrawColorBlack, 0 );
			wDrawLine( newTurnout_d.d, POSX(0.0), POSY(newTurnout_d.size.y),
						POSX(0.0), POSX(0.0), 0, wDrawLineSolid, wDrawColorBlack, 0 );

			DrawSegs( &newTurnout_d, zero, 270.0, &tempSegs(0), tempSegs_da.cnt, newTurnTrackGauge, wDrawColorBlack );

			for ( ep=0; ep<tempEndPts_da.cnt; ep++ ) {
				pos.x = -tempEndPts(ep).pos.y;
				pos.y = tempEndPts(ep).pos.x;
				Translate( &p0, pos, tempEndPts(ep).angle+90+270.0,
						newTurnTrackGauge );
				Translate( &p1, pos, tempEndPts(ep).angle+270+270.0,
						newTurnTrackGauge );
				DrawLine( &newTurnout_d, p0, p1, 0, wDrawColorBlack );
				Translate( &p0, pos, tempEndPts(ep).angle+270.0,
						newTurnout_d.size.y/2.0 );
				DrawStraightTrack( &newTurnout_d, pos, p0,
						tempEndPts(ep).angle+270.0,
						NULL, newTurnTrackGauge, wDrawColorBlack, 0 );
			}

			if ( !wPrintPageEnd( newTurnout_d.d ) )
				goto quitPrinting;
		}
	}
quitPrinting:
	wPrintDocEnd();
}
#endif

static void NewTurnOk( void * context )
{
	FILE * f;
	toDesignSchema_t * pp;
	wIndex_t pathLen;
	int i;
	BOOL_T foundR=FALSE;
	char * cp;
#ifndef MKTURNOUT
	turnoutInfo_t *to;
#endif
	FLOAT_T flt;
	wIndex_t segCnt;
	char * customInfoP;
	char *oldLocale = NULL;

	if ((pp=LoadSegs( curDesign, TRUE, &pathLen )) == NULL)
		return;

	if ( (curDesign->strCnt >= 1 && newTurnLeftDesc[0] == 0) ||
		 (curDesign->strCnt >= 2 && newTurnRightDesc[0] == 0) ) {
		NoticeMessage( MSG_TODSGN_DESC_NONBLANK, _("Ok"), NULL );
		return;
	}

	BuildTrimedTitle( message, "\t", newTurnManufacturer, newTurnLeftDesc, newTurnLeftPartno );
#ifndef MKTURNOUT
	if ( customTurnout1 == NULL &&
		 ( foundR || FindCompound( FIND_TURNOUT, newTurnScaleName, message ) ) ) {
		if ( !NoticeMessage( MSG_TODSGN_REPLACE, _("Yes"), _("No") ) )
			return;
	}
	oldLocale = SaveLocale("C");
#endif

	f = OpenCustom("a");

	sprintf( tempCustom, "\"%s\" \"%s\" \"",
				curDesign->label, "" );
	cp = tempCustom + strlen(tempCustom);
	cp = Strcpytrimed( cp, newTurnManufacturer, TRUE );
	strcpy( cp, "\" \"" );
	cp += 3;
	cp = Strcpytrimed( cp, newTurnLeftDesc, TRUE );
	strcpy( cp, "\" \"" );
	cp += 3;
	cp = Strcpytrimed( cp, newTurnLeftPartno, TRUE );
	strcpy( cp, "\"" );
	cp += 1;
	if (curDesign->type == NTO_REGULAR || curDesign->type == NTO_CURVED) {
		strcpy( cp, " \"" );
		cp += 2;
		cp = Strcpytrimed( cp, newTurnRightDesc, TRUE );
		strcpy( cp, "\" \"" );
		cp += 3;
		cp = Strcpytrimed( cp, newTurnRightPartno, TRUE );
		strcpy( cp, "\"" );
		cp += 1;
	}
	if ( cp-tempCustom > sizeof tempCustom )
		AbortProg( "Custom line overflow" );
	for ( i=0; i<curDesign->floatCnt; i++ ) {
		flt = *(FLOAT_T*)(turnDesignPLs[curDesign->floats[i].index].valueP);
		switch( curDesign->floats[i].mode ) {
		case Dim_e:
			flt = ( flt );
			break;
		case Frog_e:
			if (newTurnAngleMode == 0 && flt > 0.0)
				flt = R2D(asin(1.0/flt));
			break;
		case Angle_e:
			break;
		}
		sprintf( cp, " %0.6f", flt );
		cp += strlen(cp);
	}
	sprintf( cp, " %0.6f %0.6f %ld", newTurnRoadbedWidth, newTurnRoadbedLineWidth/(_DPI), wDrawGetRGB(roadbedColor) );
	customInfoP = MyStrdup( tempCustom );
	strcpy( tempCustom, message );

	segCnt = tempSegs_da.cnt;
#ifndef MKTURNOUT
	if (includeNontrackSegments && customTurnout1)
		CopyNonTracks( customTurnout1 );
	if ( customTurnout1 )
		customTurnout1->segCnt = 0;
	to = CreateNewTurnout( newTurnScaleName, tempCustom, tempSegs_da.cnt, &tempSegs(0),
						pathLen, pp->paths, tempEndPts_da.cnt, &tempEndPts(0), FALSE );
	to->customInfo = customInfoP;
#endif
	if (f) {
		fprintf( f, "TURNOUT %s \"%s\"\n", newTurnScaleName, PutTitle(tempCustom) );
#ifdef MKTURNOUT
		if (doCustomInfoLine)
#endif
		fprintf( f, "\tU %s\n", customInfoP );
		WriteCompoundPathsEndPtsSegs( f, pp->paths, tempSegs_da.cnt, &tempSegs(0),
				tempEndPts_da.cnt, &tempEndPts(0) );
	}

	switch (curDesign->type) {
	case NTO_REGULAR:
		points[2].y = - points[2].y;
		points[4].y = - points[4].y;
		radii[0] = - radii[0];
		LoadSegs( curDesign, FALSE, &pathLen );
		tempEndPts(2).pos.y = - tempEndPts(2).pos.y;
		tempEndPts(2).angle = 180.0 - tempEndPts(2).angle;
		BuildTrimedTitle( tempCustom, "\t", newTurnManufacturer, newTurnRightDesc, newTurnRightPartno );
		tempSegs_da.cnt = segCnt;
#ifndef MKTURNOUT
		if (includeNontrackSegments && customTurnout2)
			CopyNonTracks( customTurnout2 );
		if ( customTurnout2 )
			customTurnout2->segCnt = 0;
		to = CreateNewTurnout( newTurnScaleName, tempCustom, tempSegs_da.cnt, &tempSegs(0),
						pathLen, pp->paths, tempEndPts_da.cnt, &tempEndPts(0), FALSE );
		to->customInfo = customInfoP;
#endif
		if (f) {
			fprintf( f, "TURNOUT %s \"%s\"\n", newTurnScaleName, PutTitle(tempCustom) );
#ifdef MKTURNOUT
			if (doCustomInfoLine)
#endif
			fprintf( f, "\tU %s\n", customInfoP );
			WriteCompoundPathsEndPtsSegs( f, pp->paths, tempSegs_da.cnt, &tempSegs(0), tempEndPts_da.cnt, &tempEndPts(0) );
		}
		break;
	case NTO_CURVED:
		points[1].y = - points[1].y;
		points[2].y = - points[2].y;
		points[4].y = - points[4].y;
		points[6].y = - points[6].y;
		radii[0] = - radii[0];
		radii[1] = - radii[1];
		LoadSegs( curDesign, FALSE, &pathLen );
		tempEndPts(1).pos.y = - tempEndPts(1).pos.y;
		tempEndPts(1).angle = 180.0 - tempEndPts(1).angle;
		tempEndPts(2).pos.y = - tempEndPts(2).pos.y;
		tempEndPts(2).angle = 180.0 - tempEndPts(2).angle;
		BuildTrimedTitle( tempCustom, "\t", newTurnManufacturer, newTurnRightDesc, newTurnRightPartno );
		tempSegs_da.cnt = segCnt;
#ifndef MKTURNOUT
		if (includeNontrackSegments && customTurnout2)
			CopyNonTracks( customTurnout2 );
		if ( customTurnout2 )
			customTurnout2->segCnt = 0;
		to = CreateNewTurnout( newTurnScaleName, tempCustom, tempSegs_da.cnt, &tempSegs(0),
						pathLen, pp->paths, tempEndPts_da.cnt, &tempEndPts(0), FALSE );
		to->customInfo = customInfoP;
#endif
		if (f) {
			fprintf( f, "TURNOUT %s \"%s\"\n", newTurnScaleName, PutTitle(tempCustom) );
#ifdef MKTURNOUT
			if (doCustomInfoLine)
#endif
			fprintf( f, "\tU %s\n", customInfoP );
			WriteCompoundPathsEndPtsSegs( f, pp->paths, tempSegs_da.cnt, &tempSegs(0), tempEndPts_da.cnt, &tempEndPts(0) );
		}
		break;
	default:
		;
	}
	tempCustom[0] = '\0';

#ifndef MKTURNOUT
	if (f)
		fclose(f);
	RestoreLocale(oldLocale);
	includeNontrackSegments = TRUE;
	wHide( newTurnW );
	DoChangeNotification( CHANGE_PARAMS );

#endif

}


#ifndef MKTURNOUT
static void NewTurnCancel( wWin_p win )
{
	wHide( newTurnW );
	includeNontrackSegments = TRUE;
}



static wPos_t turnDesignWidth;
static wPos_t turnDesignHeight;

static void TurnDesignLayout(
		paramData_t * pd,
		int index,
		wPos_t colX,
		wPos_t * w,
		wPos_t * h )
{
	wPos_t inx;
	if ( curDesign == NULL )
		return;
	if ( index >= I_TO_FIRST_FLOAT && index <= I_TO_LAST_FLOAT ) {
		for ( inx=0; inx<curDesign->floatCnt; inx++ ) {
			if ( index == curDesign->floats[inx].index ) {
				*w = curDesign->floats[inx].pos.x;
				*h = curDesign->floats[inx].pos.y;
				return;
			}
		}
		AbortProg( "turnDesignLayout: bad index = %d", index );
	} else if ( index == I_TOMANUF ) {
		*h = turnDesignHeight + 10;
	}
}


static void SetupTurnoutDesignerW( toDesignDesc_t * newDesign )
{
	static wPos_t partnoWidth;
	int inx;
	wPos_t w, h, ctlH;

	if ( newTurnW == NULL ) {
		partnoWidth = wLabelWidth( "999-99999-9999" );
		turnDesignPLs[I_TOLDESC+1].winData =
		turnDesignPLs[I_TORDESC+1].winData =
			(void*)(intptr_t)partnoWidth;
		partnoWidth += wLabelWidth( " # " );
		newTurnW = ParamCreateDialog( &turnDesignPG, _("Turnout Designer"), _("Print"), NewTurnPrint, NewTurnCancel, TRUE, TurnDesignLayout, F_BLOCK, NULL );
		for ( inx=0; inx<(sizeof designDescs/sizeof designDescs[0]); inx++ ) {
			designDescs[inx]->lineC = wLineCreate( turnDesignPG.win, NULL, designDescs[inx]->lineCnt, designDescs[inx]->lines );
			wControlShow( (wControl_p)designDescs[inx]->lineC, FALSE );
		}
	}
	if ( curDesign != newDesign ) {
		if ( curDesign )
			wControlShow( (wControl_p)curDesign->lineC, FALSE );
		curDesign = newDesign;
		sprintf( message, _("%s %s Designer"), sProdName, _(curDesign->label) );
		wWinSetTitle( newTurnW, message );
		for ( inx=I_TO_FIRST_FLOAT; inx<=I_TO_LAST_FLOAT; inx++ ) {
			turnDesignPLs[inx].option |= PDO_DLGIGNORE;
			wControlShow( turnDesignPLs[inx].control, FALSE );
		}
		for ( inx=0; inx<curDesign->floatCnt; inx++ ) {
			turnDesignPLs[curDesign->floats[inx].index].option &= ~PDO_DLGIGNORE;
			wControlSetLabel( turnDesignPLs[curDesign->floats[inx].index].control, _(curDesign->floats[inx].winLabel) );
			wControlShow( turnDesignPLs[curDesign->floats[inx].index].control, TRUE );
		}
		wControlShow( turnDesignPLs[I_TORDESC+0].control, curDesign->strCnt>1 );
		wControlShow( turnDesignPLs[I_TORDESC+1].control, curDesign->strCnt>1 );
		wControlShow( (wControl_p)curDesign->lineC, TRUE );

		turnDesignWidth = turnDesignHeight = 0;
		for (inx=0;inx<curDesign->lineCnt;inx++) {
			if (curDesign->lines[inx].x0 > turnDesignWidth)
				turnDesignWidth = curDesign->lines[inx].x0;
			if (curDesign->lines[inx].x1 > turnDesignWidth)
				turnDesignWidth = curDesign->lines[inx].x1;
			if (curDesign->lines[inx].y0 > turnDesignHeight)
				turnDesignHeight = curDesign->lines[inx].y0;
			if (curDesign->lines[inx].y1 > turnDesignHeight)
				turnDesignHeight = curDesign->lines[inx].y1;
		}
		ctlH = wControlGetHeight( turnDesignPLs[I_TO_FIRST_FLOAT].control );
		for ( inx=0; inx<curDesign->floatCnt; inx++ ) {
			w = curDesign->floats[inx].pos.x + 80;
			h = curDesign->floats[inx].pos.y + ctlH;
			if (turnDesignWidth < w)
				turnDesignWidth = w;
			if (turnDesignHeight < h)
				turnDesignHeight = h;
		}
		if ( curDesign->strCnt > 1 ) {
			w = wLabelWidth( _("Right Description") );
			wControlSetLabel( turnDesignPLs[I_TOLDESC].control, _("Left Description") );
			turnDesignPLs[I_TOLDESC].winLabel = N_("Left Description");
			turnDesignPLs[I_TORDESC+0].option &= ~PDO_DLGIGNORE;
			turnDesignPLs[I_TORDESC+1].option &= ~PDO_DLGIGNORE;
		} else {
			w = wLabelWidth( _("Manufacturer") );
			wControlSetLabel( turnDesignPLs[I_TOLDESC].control, _("Description") );
			turnDesignPLs[I_TOLDESC].winLabel = N_("Description");
			turnDesignPLs[I_TORDESC+0].option |= PDO_DLGIGNORE;
			turnDesignPLs[I_TORDESC+1].option |= PDO_DLGIGNORE;
		}
		if ( curDesign->angleModeCnt > 0 ) {
			turnDesignPLs[I_TOANGMODE].option &= ~PDO_DLGIGNORE;
			wControlShow( turnDesignPLs[I_TOANGMODE].control, TRUE );
		} else {
			turnDesignPLs[I_TOANGMODE].option |= PDO_DLGIGNORE;
			wControlShow( turnDesignPLs[I_TOANGMODE].control, FALSE );
		}

		w = turnDesignWidth-w;
		wStringSetWidth( (wString_p)turnDesignPLs[I_TOMANUF].control, w );
		w -= partnoWidth;
		wStringSetWidth( (wString_p)turnDesignPLs[I_TOLDESC].control, w );
		wStringSetWidth( (wString_p)turnDesignPLs[I_TORDESC].control, w );
		ParamLayoutDialog( &turnDesignPG );
	}
}


static void ShowTurnoutDesigner( void * context )
{
	if (recordF)
		fprintf( recordF, TURNOUTDESIGNER " SHOW %s\n", ((toDesignDesc_t*)context)->label );
	newTurnScaleName = curScaleName;
	newTurnTrackGauge = trackGauge;
	SetupTurnoutDesignerW( (toDesignDesc_t*)context );
	newTurnRightDesc[0] = '\0';
	newTurnRightPartno[0] = '\0';
	newTurnLeftDesc[0] = '\0';
	newTurnLeftPartno[0] = '\0';
	newTurnLen0 =
	newTurnOff1 = newTurnLen1 = newTurnAngle1 =
	newTurnOff2 = newTurnLen2 = newTurnAngle2 = 0.0;
	ParamLoadControls( &turnDesignPG );
	ParamGroupRecord( &turnDesignPG );
	customTurnout1 = NULL;
	customTurnout2 = NULL;
	wShow( newTurnW );
}


static BOOL_T NotClose( DIST_T d )
{
	return d < -0.001 || d > 0.001;
}


EXPORT void EditCustomTurnout( turnoutInfo_t * to, turnoutInfo_t * to1 )
{
	int i;
	toDesignDesc_t * dp;
	char * type, * name, *cp, *mfg, *descL, *partL, *descR, *partR;
	wIndex_t pathLen;
	long rgb;
	trkSeg_p sp0, sp1;
	BOOL_T segsDiff;
	DIST_T width;

	if ( ! GetArgs( to->customInfo, "qqqqqc", &type, &name, &mfg, &descL, &partL, &cp ) )
		return;
	for ( i=0; i<(sizeof designDescs/sizeof designDescs[0]); i++ ) {
		dp = designDescs[i];
		if ( strcmp( type, dp->label ) == 0 ) {
			break;
		}
	}
	if ( i >= (sizeof designDescs/sizeof designDescs[0]) )
		return;

	SetupTurnoutDesignerW(dp);
	newTurnTrackGauge = GetScaleTrackGauge( to->scaleInx );
	newTurnScaleName = GetScaleName( to->scaleInx );
	strcpy( newTurnManufacturer, mfg );
	strcpy( newTurnLeftDesc, descL );
	strcpy( newTurnLeftPartno, partL );
	if (dp->type == NTO_REGULAR || dp->type == NTO_CURVED) {
		if ( ! GetArgs( cp, "qqc", &descR, &partR, &cp ))
			return;
		strcpy( newTurnRightDesc, descR );
		strcpy( newTurnRightPartno, partR );
	} else {
		descR = partR = "";
	}
	for ( i=0; i<dp->floatCnt; i++ ) {
		if ( ! GetArgs( cp, "fc", turnDesignPLs[dp->floats[i].index].valueP, &cp ) )
			return;
		switch (dp->floats[i].mode) {
		case Dim_e:
			/* *dp->floats[i].valueP = PutDim( *dp->floats[i].valueP ); */
			break;
		case Frog_e:
			if (newTurnAngleMode == 0) {
				if ( *(FLOAT_T*)(turnDesignPLs[dp->floats[i].index].valueP) > 0.0 )
					 *(FLOAT_T*)(turnDesignPLs[dp->floats[i].index].valueP) = 1.0/sin(D2R(*(FLOAT_T*)(turnDesignPLs[dp->floats[i].index].valueP)));
			}
			break;
		case Angle_e:
			break;
		}
	}
	rgb = 0;
	if ( cp && GetArgs( cp, "ffl", &newTurnRoadbedWidth, &width, &rgb ) ) {
		roadbedColor = wDrawFindColor(rgb);
		newTurnRoadbedLineWidth = (long)floor(width*mainD.dpi+0.5);
	} else {
		newTurnRoadbedWidth = 0;
		newTurnRoadbedLineWidth = 0;
		roadbedColor = wDrawColorBlack;
	}

	customTurnout1 = to;
	customTurnout2 = to1;

	segsDiff = FALSE;
	if ( to ) {
		LoadSegs( dp, TRUE, &pathLen );
		segsDiff = FALSE;
		if ( to->segCnt == tempSegs_da.cnt ) {
			for ( sp0=to->segs,sp1=&tempSegs(0); (!segsDiff) && sp0<&to->segs[to->segCnt]; sp0++,sp1++ ) {
				switch (sp0->type) {
				case SEG_STRLIN:
					if (sp0->type != sp1->type ||
						sp0->color != sp1->color ||
						NotClose(sp0->width-width) ||
						NotClose(sp0->u.l.pos[0].x-sp1->u.l.pos[0].x) ||
						NotClose(sp0->u.l.pos[0].y-sp1->u.l.pos[0].y) ||
						NotClose(sp0->u.l.pos[1].x-sp1->u.l.pos[1].x) ||
						NotClose(sp0->u.l.pos[1].y-sp1->u.l.pos[1].y) )
							 segsDiff = TRUE;
					break;
				case SEG_CRVLIN:
					if (sp0->type != sp1->type ||
						sp0->color != sp1->color ||
						NotClose(sp0->width-width) ||
						NotClose(sp0->u.c.center.x-sp1->u.c.center.x) ||
						NotClose(sp0->u.c.center.y-sp1->u.c.center.y) ||
						NotClose(sp0->u.c.radius-sp1->u.c.radius) ||
						NotClose(sp0->u.c.a0-sp1->u.c.a0) ||
						NotClose(sp0->u.c.a1-sp1->u.c.a1) )
							 segsDiff = TRUE;
					break;
				case SEG_STRTRK:
				case SEG_CRVTRK:
					break;
				default:
					segsDiff = TRUE;
				}
			}
		} else {
			for ( sp0=to->segs; (!segsDiff) && sp0<&to->segs[to->segCnt]; sp0++ ) {
				if ( sp0->type != SEG_STRTRK && sp0->type != SEG_CRVTRK )
					segsDiff = TRUE;
			}
		}
	}
	if ( (!segsDiff) && to1 && (dp->type==NTO_REGULAR||dp->type==NTO_CURVED) ) {
		if ( dp->type==NTO_REGULAR ) {
			points[2].y = - points[2].y;
			points[4].y = - points[4].y;
			radii[0] = - radii[0];
		} else {
			points[1].y = - points[1].y;
			points[2].y = - points[2].y;
			points[4].y = - points[4].y;
			points[6].y = - points[6].y;
			radii[0] = - radii[0];
			radii[1] = - radii[1];
		}
		LoadSegs( dp, FALSE, &pathLen );
		if ( dp->type==NTO_REGULAR ) {
			points[2].y = - points[2].y;
			points[4].y = - points[4].y;
			radii[0] = - radii[0];
		} else {
			points[1].y = - points[1].y;
			points[2].y = - points[2].y;
			points[4].y = - points[4].y;
			points[6].y = - points[6].y;
			radii[0] = - radii[0];
			radii[1] = - radii[1];
		}
		segsDiff = FALSE;
		if ( to1->segCnt == tempSegs_da.cnt ) {
			for ( sp0=to1->segs,sp1=&tempSegs(0); (!segsDiff) && sp0<&to1->segs[to1->segCnt]; sp0++,sp1++ ) {
				switch (sp0->type) {
				case SEG_STRLIN:
					if (sp0->type != sp1->type ||
						sp0->color != sp1->color ||
						NotClose(sp0->width-width) ||
						NotClose(sp0->u.l.pos[0].x-sp1->u.l.pos[0].x) ||
						NotClose(sp0->u.l.pos[0].y-sp1->u.l.pos[0].y) ||
						NotClose(sp0->u.l.pos[1].x-sp1->u.l.pos[1].x) ||
						NotClose(sp0->u.l.pos[1].y-sp1->u.l.pos[1].y) )
							 segsDiff = TRUE;
					break;
				case SEG_CRVLIN:
					if (sp0->type != sp1->type ||
						sp0->color != sp1->color ||
						NotClose(sp0->width-width) ||
						NotClose(sp0->u.c.center.x-sp1->u.c.center.x) ||
						NotClose(sp0->u.c.center.y-sp1->u.c.center.y) ||
						NotClose(sp0->u.c.radius-sp1->u.c.radius) ||
						NotClose(sp0->u.c.a0-sp1->u.c.a0) ||
						NotClose(sp0->u.c.a1-sp1->u.c.a1) )
							 segsDiff = TRUE;
					break;
				case SEG_STRTRK:
				case SEG_CRVTRK:
					break;
				default:
					segsDiff = TRUE;
				}
			}
		} else {
			for ( sp0=to1->segs; (!segsDiff) && sp0<&to1->segs[to1->segCnt]; sp0++ ) {
				if ( sp0->type != SEG_STRTRK && sp0->type != SEG_CRVTRK )
					segsDiff = TRUE;
			}
		}
	}

	includeNontrackSegments = TRUE;
	if ( segsDiff ) {
		if ( NoticeMessage( MSG_SEGMENTS_DIFFER, _("Yes"), _("No") ) <= 0 ) {
			includeNontrackSegments = FALSE;
		}
	} else {
		includeNontrackSegments = FALSE;
	}
	/*if (recordF)
		fprintf( recordF, TURNOUTDESIGNER " SHOW %s\n", dp->label );*/
	ParamLoadControls( &turnDesignPG );
	ParamGroupRecord( &turnDesignPG );
	wShow( newTurnW );
}


EXPORT void InitNewTurn( wMenu_p m )
{
	int i;
	ParamRegister( &turnDesignPG );
	for ( i=0; i<(sizeof designDescs/sizeof designDescs[0]); i++ ) {
		wMenuPushCreate( m, NULL, _(designDescs[i]->label), 0,
				ShowTurnoutDesigner, (void*)designDescs[i] );
		sprintf( message, "%s SHOW %s", TURNOUTDESIGNER, designDescs[i]->label );
		AddPlaybackProc( message, (playbackProc_p)ShowTurnoutDesigner, designDescs[i] );
	}
	roadbedColor = wDrawColorBlack;
	includeNontrackSegments = TRUE;
}
#endif

#ifdef MKTURNOUT


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

char message[1024];
char * curScaleName;
double trackGauge;
long units = 0;
wDrawColor drawColorBlack;
long roadbedColorRGB = 0;

EXPORT void AbortProg(
		char * msg,
		... )
{
	static BOOL_T abort2 = FALSE;
//	int rc;
	va_list ap;
	va_start( ap, msg );
	vsprintf( message, msg, ap );
	va_end( ap );
	fprintf( stderr, "%s", message );
	abort();
}

void * MyRealloc( void * ptr, long size )
{
	return realloc( ptr, size );
}

EXPORT char * MyStrdup( const char * str )
{
	char * ret;
	ret = (char*)malloc( strlen( str ) + 1 );
	strcpy( ret, str );
	return ret;
}


int NoticeMessage( char * msg, char * yes, char * no, ... )
{
	/*fprintf( stderr, "%s\n", msg );*/
	return 0;
}

FILE * OpenCustom( char * mode)
{
	return stdout;
}

void wPrintSetup( wPrintSetupCallBack_p notused )
{
}

EXPORT void ComputeCurvedSeg(
		trkSeg_p s,
		DIST_T radius,
		coOrd p0,
		coOrd p1 )
{
	DIST_T d;
	ANGLE_T a, aa, aaa;
	s->u.c.radius = radius;
	d = FindDistance( p0, p1 )/2.0;
	a = FindAngle( p0, p1 );
	if (radius > 0) {
		aa = R2D(asin( d/radius ));
		aaa = a + (90.0 - aa);
		Translate( &s->u.c.center, p0, aaa, radius );
		s->u.c.a0 = NormalizeAngle( aaa + 180.0 );
		s->u.c.a1 = aa*2.0;
	} else {
		aa = R2D(asin( d/(-radius) ));
		aaa = a - (90.0 - aa);
		Translate( &s->u.c.center, p0, aaa, -radius );
		s->u.c.a0 = NormalizeAngle( aaa + 180.0 - aa *2.0 );
		s->u.c.a1 = aa*2.0;
	}
}

EXPORT char * Strcpytrimed( char * dst, char * src, BOOL_T double_quotes )
{
	char * cp;
	while (*src && isspace((unsigned char)*src) ) src++;
	if (!*src)
		return dst;
	cp = src+strlen(src)-1;
	while ( cp>src && isspace((unsigned char)*cp) ) cp--;
	while ( src<=cp ) {
		if (*src == '"' && double_quotes)
			*dst++ = '"';
		*dst++ = *src++;
	}
	*dst = '\0';
	return dst;
}


EXPORT char * BuildTrimedTitle( char * cp, char * sep, char * mfg, char * desc, char * partno )
{
	cp = Strcpytrimed( cp, mfg, FALSE );
	strcpy( cp, sep );
	cp += strlen(cp);
	cp = Strcpytrimed( cp, desc, FALSE );
	strcpy( cp, sep );
	cp += strlen(cp);
	cp = Strcpytrimed( cp, partno, FALSE );
	return cp;
}


EXPORT char * PutTitle( char * cp )
{
	static char title[STR_SIZE];
	char * tp = title;
	while (*cp) {
		if (*cp == '\"') {
			*tp++ = '\"';
			*tp++ = '\"';
		} else {
			*tp++ = *cp;
		}
		cp++;
	}
	*tp = '\0';
	return title;
}


long wDrawGetRGB(
		wDrawColor color )
{
	return roadbedColorRGB;
}

EXPORT BOOL_T WriteSegs(
		FILE * f,
		wIndex_t segCnt,
		trkSeg_p segs )
{
	int i, j;
	BOOL_T rc = TRUE;
	for ( i=0; i<segCnt; i++ ) {
		switch ( segs[i].type ) {
		case SEG_STRLIN:
		case SEG_STRTRK:
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f %0.6f %0.6f\n",
				segs[i].type, (segs[i].type==SEG_STRTRK?0:roadbedColorRGB), segs[i].width,
				segs[i].u.l.pos[0].x, segs[i].u.l.pos[0].y,
				segs[i].u.l.pos[1].x, segs[i].u.l.pos[1].y )>0;
			break;
		case SEG_CRVTRK:
		case SEG_CRVLIN:
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f %0.6f %0.6f %0.6f\n",
				segs[i].type, (segs[i].type==SEG_CRVTRK?0:roadbedColorRGB), segs[i].width,
				segs[i].u.c.radius,
				segs[i].u.c.center.x, segs[i].u.c.center.y,
				segs[i].u.c.a0, segs[i].u.c.a1 )>0;
			break;
		case SEG_FILCRCL:
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f %0.6f\n",
				segs[i].type, roadbedColorRGB, segs[i].width,
				segs[i].u.c.radius,
				segs[i].u.c.center.x, segs[i].u.c.center.y )>0;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			rc &= fprintf( f, "\t%c %ld %0.6f %d\n",
				segs[i].type, roadbedColorRGB, segs[i].width,
				segs[i].u.p.cnt )>0;
			for ( j=0; j<segs[i].u.p.cnt; j++ )
				rc &= fprintf( f, "\t\t%0.6f %0.6f\n",
						segs[i].u.p.pts[j].x, segs[i].u.p.pts[j].y )>0;
			break;
		}
	}
	rc &= fprintf( f, "\tEND\n" )>0;
	return rc;
}

BOOL_T WriteCompoundPathsEndPtsSegs(
		FILE * f,
		PATHPTR_T paths,
		wIndex_t segCnt,
		trkSeg_p segs,
		EPINX_T endPtCnt,
		trkEndPt_t * endPts )
{
	int i;
	PATHPTR_T pp;
	BOOL_T rc = TRUE;
	for ( pp=paths; *pp; pp+=2 ) {
		rc &= fprintf( f, "\tP \"%s\"", (char*)pp )>0;
		for ( pp+=strlen((char*)pp)+1; pp[0]!=0||pp[1]!=0; pp++ )
			rc &= fprintf( f, " %d", *pp )>0;
		rc &= fprintf( f, "\n" )>0;
	}
	for ( i=0; i<endPtCnt; i++ )
		rc &= fprintf( f, "\tE %0.6f %0.6f %0.6f\n",
				endPts[i].pos.x, endPts[i].pos.y, endPts[i].angle )>0;
#ifdef MKTURNOUT
	if ( specialLine[0] )
		rc &= fprintf( f, "%s\n", specialLine );
#endif
	rc &= WriteSegs( f, segCnt, segs );
	return rc;
}


void Usage( int argc, char **argv )
{
	int inx;
	for (inx=1;inx<argc;inx++)
		fprintf( stderr, "%s ", argv[inx] );
	fprintf( stderr,
"\nUsage: [-m] [-u] [-r#] [-c#] [-l#]\n"
"  <SCL> <MNF> B <DSC> <PNO> <LEN> # Create bumper\n"
"  <SCL> <MNF> S <DSC> <PNO> <LEN> # Create straight track\n"
"  <SCL> <MNF> J <DSC> <PNO> <LEN1> <LEN2> # Create adjustable track\n"
"  <SCL> <MNF> C <DSC> <PNO> <RAD> <ANG> # Create curved track\n"
"  <SCL> <MNF> R <LDSC> <LPNO> <RDSC> <RPNO> <LEN2> <ANG> <OFF> <LEN1> # Create Regular Turnout\n"
"  <SCL> <MNF> Q <LDSC> <LPNO> <RDSC> <RPNO> <RAD> <ANG> <LEN> # Create Radial Turnout\n"
"  <SCL> <MNF> V <LDSC> <LPNO> <RDSC> <RPNO> <LEN1> <ANG1> <OFF1> <LEN2> <ANG2> <OFF2> # Create Curved Turnout\n"
"  <SCL> <MNF> W <LDSC> <LPNO> <RDSC> <RPNO> <RAD1> <ANG2> <RAD2> <ANG2> # Create Radial Curved Turnout\n"
"  <SCL> <MNF> Y <LDSC> <LPNO> <RDSC> <RPNO> <LENL> <ANGL> <OFFL> <LENR> <ANGR> <OFFR> # Create Wye Turnout\n"
"  <SCL> <MNF> 3 <DSC> <PNO> <LEN0> <LENL> <ANGL> <OFFL> <LENR> <ANGR> <OFFR> # Create 3-Way Turnout\n"
"  <SCL> <MNF> X <DSC> <PNO> <LEN1> <ANG> <LEN2> # Create Crossing\n"
"  <SCL> <MNF> 1 <DSC> <PNO> <LEN1> <ANG> <LEN2> # Create Single Slipswitch\n"
"  <SCL> <MNF> 2 <DSC> <PNO> <LEN1> <ANG> <LEN2> # Create Double Slipswitch\n"
"  <SCL> <MNF> D <DSC> <PNO> <LEN> <OFF> # Create Double Crossover\n"
"  <SCL> <MNF> T <DSC> <PNO> <CNT> <IN-DIAM> <OUT-DIAM> # Create TurnTable\n"
);
		exit(1);
}

struct {
		char * scale;
		double trackGauge;
		} scaleMap[] = {
		{ "N", 0.3531 },
		{ "HO", 0.6486 },
		{ "O", 1.1770 },
		{ "HOm", 0.472440 },
		{ "G", 1.770 }
	 };



int main ( int argc, char * argv[] )
{
//	char * cmd;
	double radius, radius2;
	int inx, cnt;
	double ang, x0, y0, x1, y1;
	char **argv0;
	int argc0;

	argc0 = argc;
	argv0 = argv;
	doCustomInfoLine = FALSE;
	argv++;

	if (argc < 7) {
		Usage(argc0,argv0);
	}

	while ( argv[0][0] == '-' ) {
		switch (argv[0][1]) {
		case 'm':
			units = UNITS_METRIC;
			break;
		case 'u':
			doCustomInfoLine = TRUE;
			break;
		case 'r':
			doRoadBed = TRUE;
			if (argv[0][2] == '\0')
				Usage(argc0,argv0);
			newTurnRoadbedWidth = atof(&argv[0][2]);
			roadbedColorRGB = 0;
			roadbedColor = 0;
			newTurnRoadbedLineWidth = 0;
			break;
		case 'c':
			roadbedColorRGB = atol(&argv[0][2]);
			break;
		case 'l':
			newTurnRoadbedLineWidth = atol(&argv[0][2]);
			break;
		default:
			fprintf( stderr, "Unknown option: %s\n", argv[0] );
		}
		argv++;
		argc--;
	}

	newTurnScaleName = curScaleName = *argv++;
	trackGauge = 0.0;
	for ( inx=0; inx<sizeof scaleMap/sizeof scaleMap[0]; inx++ ) {
		if (strcmp( curScaleName, scaleMap[inx].scale ) == 0 ) {
			newTurnTrackGauge = trackGauge = scaleMap[inx].trackGauge;
			break;
		}
	}
	if (trackGauge == 0.0) {
		fprintf( stderr, "Unknown scale: %s\n", curScaleName );
		exit(1);
	}
	strcpy( newTurnManufacturer, *argv++ );
	specialLine[0] = '\0';
	switch (tolower((unsigned char)(*argv++)[0])) {
	case 'b':
		if (argc != 7) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		curDesign = &StrSectionDesc;
		NewTurnOk( &StrSectionDesc );
		break;
	case 's':
		if (argc != 7) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		curDesign = &StrSectionDesc;
		NewTurnOk( &StrSectionDesc );
		break;
	case 'j':
		if (argc != 8) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnLen2 = GetDim(atof( *argv++ ));
		sprintf( specialLine, "\tX adjustable %0.6f %0.6f", newTurnLen1, newTurnLen2 );
		curDesign = &StrSectionDesc;
		NewTurnOk( &StrSectionDesc );
		break;
	case 'c':
		if (argc != 8) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		curDesign = &CrvSectionDesc;
		NewTurnOk( &CrvSectionDesc );
		break;
	case 'r':
		if (argc != 12) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		newTurnLen0 = GetDim(atof( *argv++ ));
		curDesign = &RegDesc;
		NewTurnOk( &RegDesc );
		break;
	case 'q':
		if (argc != 11) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		radius = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnLen1 = radius * sin(D2R(newTurnAngle1));
		newTurnOff1 = radius * (1-cos(D2R(newTurnAngle1)));
		curDesign = &RegDesc;
		NewTurnOk( &RegDesc );
		break;
	case 'v':
		if (argc != 14) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		newTurnLen2 = GetDim(atof( *argv++ ));
		newTurnAngle2 = atof( *argv++ );
		newTurnOff2 = GetDim(atof( *argv++ ));
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		curDesign = &CrvDesc;
		NewTurnOk( &CrvDesc );
		break;
	case 'w':
		if (argc != 12) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		radius = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnLen1 = radius * sin(D2R(newTurnAngle1));
		newTurnOff1 = radius * (1-cos(D2R(newTurnAngle1)));
		radius = GetDim(atof( *argv++ ));
		newTurnAngle2 = atof( *argv++ );
		newTurnLen2 = radius * sin(D2R(newTurnAngle2));
		newTurnOff2 = radius * (1-cos(D2R(newTurnAngle2)));
		curDesign = &CrvDesc;
		NewTurnOk( &CrvDesc );
		break;
	case 'y':
		if (argc != 14) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		newTurnLen2 = GetDim(atof( *argv++ ));
		newTurnAngle2 = atof( *argv++ );
		newTurnOff2 = GetDim(atof( *argv++ ));
		curDesign = &WyeDesc;
		NewTurnOk( &WyeDesc );
		break;
	case '3':
		if (argc != 13) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		newTurnLen2 = GetDim(atof( *argv++ ));
		newTurnAngle2 = atof( *argv++ );
		newTurnOff2 = GetDim(atof( *argv++ ));
		curDesign = &ThreewayDesc;
		NewTurnOk( &ThreewayDesc );
		break;
	case 'x':
		if (argc<9) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnLen2 = GetDim(atof( *argv++ ));
		curDesign = &CrossingDesc;
		NewTurnOk( &CrossingDesc );
		break;
	case '1':
		if (argc<9) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnLen2 = GetDim(atof( *argv++ ));
		curDesign = &SingleSlipDesc;
		NewTurnOk( &SingleSlipDesc );
		break;
	case '2':
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		if (argc<9) Usage(argc0,argv0);
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnLen2 = GetDim(atof( *argv++ ));
		curDesign = &DoubleSlipDesc;
		NewTurnOk( &DoubleSlipDesc );
		break;
	case 'd':
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		if (argc<8) Usage(argc0,argv0);
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnOff1 = GetDim(atof( *argv++ ));
		curDesign = &DoubleCrossoverDesc;
		NewTurnOk( &DoubleCrossoverDesc );
		break;
	case 't':
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		if (argc<9) Usage(argc0,argv0);
		cnt = atoi( *argv++ )/2;
		radius = GetDim(atof( *argv++ ))/2.0;
		radius2 = GetDim(atof( *argv++ ))/2.0;
		BuildTrimedTitle( message, "\t", newTurnManufacturer, newTurnLeftDesc, newTurnLeftPartno );
		fprintf( stdout, "TURNOUT %s \"%s\"\n", curScaleName, PutTitle(message) );
		for (inx=0; inx<cnt; inx++) {
			fprintf( stdout, "\tP \"%d\" %d %d %d\n", inx+1, inx*3+1, inx*3+2, inx*3+3 );
		}
		for (inx=0; inx<cnt; inx++) {
			fprintf( stdout, "\tP \"%d\" %d %d %d\n", inx+1+cnt, -(inx*3+3), -(inx*3+2), -(inx*3+1) );
		}
		for (inx=0; inx<cnt; inx++) {
			ang = inx*180.0/cnt;
			x0 = radius2 * sin(D2R(ang));
			y0 = radius2 * cos(D2R(ang));
			fprintf( stdout, "\tE %0.6f %0.6f %0.6f\n", x0, y0, ang );
			fprintf( stdout, "\tE %0.6f %0.6f %0.6f\n", -x0, -y0, ang+180.0 );
		}
		for (inx=0; inx<cnt; inx++) {
			ang = inx*180.0/cnt;
			x0 = radius2 * sin(D2R(ang));
			y0 = radius2 * cos(D2R(ang));
			x1 = radius * sin(D2R(ang));
			y1 = radius * cos(D2R(ang));
			fprintf( stdout, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", x0, y0, x1, y1 );
			fprintf( stdout, "\tS 16777215 0 %0.6f %0.6f %0.6f %0.6f\n", x1, y1, -x1, -y1 );
			fprintf( stdout, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", -x1, -y1, -x0, -y0 );
		}
		fprintf( stdout, "\tA 16711680 0 %0.6f 0.000000 0.000000 0.000000 360.000000\n", radius2 );
		fprintf( stdout, "\tA 16711680 0 %0.6f 0.000000 0.000000 0.000000 360.000000\n", radius );
		fprintf( stdout, "\tEND\n" );
		break;
	default:
		fprintf( stderr, "Invalid command: %s\n", argv[-1] );
		exit(1);
	}
	exit(0);
}
#endif
