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
#include <string.h>
#include "messages.h"

#include "ccurve.h"
#include "compound.h"
#include "cstraigh.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"

#include "param.h"
#include "track.h"
#include "utility.h"
#include "ccornu.h"
#include "cbezier.h"
#include "misc.h"

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
#define NTO_CORNU       (15)
#define NTO_CORNUWYE    (16)
#define NTO_CORNU3WAY   (17)

#define FLOAT			(1)


typedef struct {
		struct {
			wPos_t x, y;
		} pos;
		int index;
		char * winLabel;
		char * printLabel;
		enum { Dim_e, Frog_e, Angle_e, Rad_e } mode;
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
		wBool_t slipmode;
		} toDesignDesc_t;

static wWin_p newTurnW;

static FLOAT_T newTurnRad0;
static FLOAT_T newTurnAngle0;
static FLOAT_T newTurnLen0;
static FLOAT_T newTurnOff0;

static FLOAT_T newTurnRad1;
static FLOAT_T newTurnAngle1;
static FLOAT_T newTurnLen1;
static FLOAT_T newTurnOff1;

static FLOAT_T newTurnRad2;
static FLOAT_T newTurnAngle2;
static FLOAT_T newTurnLen2;
static FLOAT_T newTurnOff2;

static FLOAT_T newTurnRad3;
static FLOAT_T newTurnAngle3;
static FLOAT_T newTurnOff3;
static FLOAT_T newTurnLen3;

static FLOAT_T newTurnToeL;
static FLOAT_T newTurnToeR;

static long newTurnAngleMode = 1;
static long newTurnSlipMode = 0;
static char newTurnRightDesc[STR_SIZE], newTurnLeftDesc[STR_SIZE];
static char newTurnRightPartno[STR_SIZE], newTurnLeftPartno[STR_SIZE];
static char newTurnManufacturer[STR_SIZE];
static char *newTurnAngleModeLabels[] = { N_("Frog #"), N_("Degrees"), NULL };
static char *newTurnSlipModeLabels[] = { N_("Dual Path"), N_("Quad Path"), NULL };
static DIST_T newTurnRoadbedWidth;
static long newTurnRoadbedLineWidth = 0;
static wDrawColor roadbedColor;
static DIST_T newTurnTrackGauge;
static char * newTurnScaleName;
static paramFloatRange_t r0_10000 = { 0, 10000, 80 };
static paramFloatRange_t r_10000_10000 = {-10000, 10000, 80 };
static paramFloatRange_t r0_360 = { 0, 360, 80 };
static paramFloatRange_t r0_100 = { 0, 100, 80 };
static paramIntegerRange_t i0_100 = { 0, 100, 40 };
static void NewTurnOk( void * );
static void ShowTurnoutDesigner( void * );


static coOrd points[20];
static coOrd end_points[20];
static coOrd end_centers[20];
static double end_arcs[20];
static double end_angles[20];
static DIST_T radii[10];
static double angles[10];


#define POSX(X) ((wPos_t)((X)*newTurnout_d.dpi))
#define POSY(Y) ((wPos_t)((Y)*newTurnout_d.dpi))

static paramData_t turnDesignPLs[] = {
#define I_TOLENGTH			(0)
#define I_TO_FIRST_FLOAT	(0)
	{ PD_FLOAT, &newTurnLen0, "len0", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
	{ PD_FLOAT, &newTurnLen1, "len1", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
	{ PD_FLOAT, &newTurnLen2, "len2", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
	{ PD_FLOAT, &newTurnLen3, "len3", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
#define I_TOOFFSET			(4)
	{ PD_FLOAT, &newTurnOff0, "off0", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Offset") },
	{ PD_FLOAT, &newTurnOff1, "off1", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Offset") },
	{ PD_FLOAT, &newTurnOff2, "off2", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Offset") },
	{ PD_FLOAT, &newTurnOff3, "off3", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Offset") },
#define I_TORAD             (8)
	{ PD_FLOAT, &newTurnRad0, "rad0", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Radius") },
	{ PD_FLOAT, &newTurnRad1, "rad1", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Radius") },
    { PD_FLOAT, &newTurnRad2, "rad2", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Radius") },
	{ PD_FLOAT, &newTurnRad3, "rad3", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r_10000_10000, N_("Radius") },
#define I_TOTOELENGTH       (12)
	{ PD_FLOAT, &newTurnToeL, "toeL", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
	{ PD_FLOAT, &newTurnToeR, "toeR", PDO_DIM|PDO_DLGIGNORELABELWIDTH, &r0_10000, N_("Length") },
#define I_TOANGLE			(14)
	{ PD_FLOAT, &newTurnAngle0, "angle0", PDO_DLGIGNORELABELWIDTH, &r0_360, N_("Angle") },
	{ PD_FLOAT, &newTurnAngle1, "angle1", PDO_DLGIGNORELABELWIDTH, &r0_360, N_("Angle") },
	{ PD_FLOAT, &newTurnAngle2, "angle2", PDO_DLGIGNORELABELWIDTH, &r0_360, N_("Angle") },
#define I_TO_LAST_FLOAT		(17)
	{ PD_FLOAT, &newTurnAngle3, "angle3", PDO_DLGIGNORELABELWIDTH, &r0_360, N_("Angle") },
#define I_TOMANUF			(18)
	{ PD_STRING, &newTurnManufacturer, "manuf", PDO_STRINGLIMITLENGTH, NULL, N_("Manufacturer"), 0, 0, sizeof(newTurnManufacturer)},
#define I_TOLDESC			(19)
	{ PD_STRING, &newTurnLeftDesc, "desc1", PDO_STRINGLIMITLENGTH, NULL, N_("Left Description"), 0, 0, sizeof(newTurnLeftDesc)},
	{ PD_STRING, &newTurnLeftPartno, "partno1", PDO_DLGHORZ | PDO_STRINGLIMITLENGTH, NULL, N_(" #"), 0, 0, sizeof(newTurnLeftPartno)},
#define I_TORDESC			(21)
	{ PD_STRING, &newTurnRightDesc, "desc2", PDO_STRINGLIMITLENGTH, NULL, N_("Right Description"),0, 0, sizeof(newTurnRightDesc)},
	{ PD_STRING, &newTurnRightPartno, "partno2", PDO_DLGHORZ | PDO_STRINGLIMITLENGTH, NULL, N_(" #"),0, 0, sizeof(newTurnRightPartno)},
	{ PD_FLOAT, &newTurnRoadbedWidth, "roadbedWidth", PDO_DIM, &r0_100, N_("Roadbed Width") },
	{ PD_LONG, &newTurnRoadbedLineWidth, "roadbedLineWidth", PDO_DLGHORZ, &i0_100, N_("Line Width") },
	{ PD_COLORLIST, &roadbedColor, "color", PDO_DLGHORZ|PDO_DLGBOXEND, NULL, N_("Color") },
	{ PD_BUTTON, (void*)NewTurnOk, "done", PDO_DLGCMDBUTTON, NULL, N_("Ok") },
	{ PD_BUTTON, (void*)wPrintSetup, "printsetup", 0, NULL, N_("Print Setup") },
#define I_TOANGMODE			(28)
	{ PD_RADIO, &newTurnAngleMode, "angleMode", 0, newTurnAngleModeLabels },
#define I_TOSLIPMODE        (29)
	{ PD_RADIO, &newTurnSlipMode, "slipMode", 0, newTurnSlipModeLabels }
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
{ { 100, 120 }, I_TOLENGTH+1, N_("Length"), N_("Overall Length"), Dim_e },
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

static wLines_t CornuLines[] = {
#include "tocornu.lin"
		};
static toDesignFloat_t CornuFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Inner Length"), Dim_e },
{ { 375, 0 }, I_TOANGLE+0, N_("Angle"), N_("Inner Angle"), Frog_e },
{ { 375, 22 }, I_TOOFFSET+0, N_("Offset"), N_("Inner Offset"), Dim_e },
{ { 375, 44 }, I_TORAD+0, N_("Radius"), N_("Inner Radius"), Dim_e },
{ { 400, 62 }, I_TOANGLE+1, N_("Angle"), N_("Outer Angle"), Frog_e },
{ { 400, 84 }, I_TOOFFSET+1, N_("Offset"), N_("Outer Offset"), Dim_e },
{ { 400, 106 }, I_TORAD+1, N_("Radius"), N_("Outer Radius"), Dim_e },
{ { 175, 120 }, I_TOLENGTH+1, N_("Length"), N_("Outer Length"), Dim_e },
{ { 50, 90 }, I_TORAD+2, N_("Radius"), N_("Toe Radius"), Dim_e },
{ { 50, 40 }, I_TOTOELENGTH+0, N_("Length"), N_("Toe Length"), Dim_e } };
static signed char CornuPaths[] = {
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 4, 0, 0, 0,
		'R', 'e', 'v', 'e', 'r', 's', 'e', 0, 1, 2, 0, 0, 0, 0 };
static toDesignSchema_t CornuSchema = {
		CornuPaths,
		"033" "343" "413" };

static toDesignDesc_t CornuDesc = {
		NTO_CORNU,
		N_("Cornu Curved Turnout"),
		2,
		sizeof CornuLines/sizeof CornuLines[0], CornuLines,
		sizeof CornuFloats/sizeof CornuFloats[0], CornuFloats,
		&CornuSchema, 1 };

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

static wLines_t CornuWyeLines[] = {
#include "tocornuwye.lin"
		};
static toDesignFloat_t CornuWyeFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Left Length"), Dim_e },
{ { 400, 28 }, I_TOANGLE+0, N_("Angle"), N_("Left Angle"), Frog_e },
{ { 400, 48 }, I_TOOFFSET+0, N_("Offset"), N_("Left Offset"), Dim_e },
{ { 400, 68 }, I_TORAD+0, N_("Radius"), N_("Left Radius"), Dim_e },
{ { 400, 108 }, I_TORAD+1, N_("Radius"), N_("Right Radius"), Dim_e },
{ { 400, 128 }, I_TOOFFSET+1, N_("Offset"), N_("Right Offset"), Dim_e },
{ { 400, 148 }, I_TOANGLE+1, N_("Angle"), N_("Right Angle"), Frog_e },
{ { 175, 170 }, I_TOLENGTH+1, N_("Length"), N_("Right Length"), Dim_e },
{ { 80, 48 }, I_TOTOELENGTH+0, N_("Length"), N_("Toe Length"), Dim_e },
{ { 80, 28 }, I_TORAD+2, N_("Radius"), N_("Toe Radius"), Dim_e },
		};
static signed char CornuWyePaths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 4, 5, 0, 0, 0 }; /* Not Used */
static toDesignSchema_t CornuWyeSchema = {
		CornuWyePaths,
		"030" "341" "410" "362" "620" };  /* Not Used */
static toDesignDesc_t CornuWyeDesc = {
		NTO_CORNUWYE,
		N_("Cornu Wye Turnout"),
		1,
		sizeof CornuWyeLines/sizeof CornuWyeLines[0], CornuWyeLines,
		sizeof CornuWyeFloats/sizeof CornuWyeFloats[0], CornuWyeFloats,
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

static wLines_t CornuThreewayLines[] = {
#include "tocornu3way.lin"
		};
static toDesignFloat_t CornuThreewayFloats[] = {
{ { 175, 10 }, I_TOLENGTH+0, N_("Length"), N_("Left Length"), Dim_e },
{ { 380, 10 }, I_TOANGLE+0, N_("Angle"), N_("Left Angle"), Frog_e },
{ { 380, 50 }, I_TOOFFSET+0, N_("Offset"), N_("Left Offset"), Dim_e },
{ { 380, 30 }, I_TORAD+0, N_("Radius"), N_("Left Radius"), Dim_e },
{ { 130, 90 }, I_TOLENGTH+3, N_("Length"), N_("Center Length"), Dim_e },
{ { 400, 70 }, I_TOANGLE+3, N_("Angle"), N_("Center Angle"), Dim_e },
{ { 400, 90}, I_TOOFFSET+3, N_("Offset"), N_("Center Offset"), Dim_e },
{ { 400, 110 }, I_TORAD+3, N_("Radius"), N_("Center Radius"), Dim_e },
{ { 420, 150 }, I_TORAD+1, N_("Radius"), N_("Right Radius"), Dim_e },
{ { 420, 130 }, I_TOOFFSET+1, N_("Offset"), N_("Right Offset"), Dim_e },
{ { 420, 170 }, I_TOANGLE+1, N_("Angle"), N_("Right Angle"), Frog_e },
{ { 175, 170 }, I_TOLENGTH+1, N_("Length"), N_("Right Length"), Dim_e },
{ { 45, 50 }, I_TOTOELENGTH+0, N_("Length"), N_("Toe Length Left"), Dim_e },
{ { 55, 140 }, I_TOTOELENGTH+1, N_("Length"), N_("Toe Length Right"), Dim_e },
{ { 40, 105 }, I_TORAD+2, N_("Radius"), N_("Toe Radius"), Dim_e },
		};
static signed char CornuTriPaths[] = {
		'L', 'e', 'f', 't', 0, 1, 2, 3, 0, 0,
		'N', 'o', 'r', 'm', 'a', 'l', 0, 1, 6, 0, 0,
		'R', 'i', 'g', 'h', 't', 0, 1, 4, 5, 0, 0, 0 };
static toDesignSchema_t CornuTriSchema = {
		CornuTriPaths,
		"030" "341" "410" "362" "620" "370" };
static toDesignDesc_t CornuThreewayDesc = {
		NTO_CORNU3WAY,
		N_("Cornu 3-way Turnout"),
		1,
		sizeof CornuThreewayLines/sizeof CornuThreewayLines[0], CornuThreewayLines,
		sizeof CornuThreewayFloats/sizeof CornuThreewayFloats[0], CornuThreewayFloats,
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
static signed char DoubleSlipPaths2[] = {
		'C', 'r', 'o', 's', 's', '1', 0, 1, 2, 3, 0, 0,
		'C', 'r', 'o', 's', 's', '2', 0, 4, 5, 6, 0, 0,
		'S', 'l', 'i', 'p', '1', 0, 1, 7, 6, 0, 0,
		'S', 'l', 'i', 'p', '2', 0, 4, 8, 3, 0, 0, 0 };
static toDesignSchema_t DoubleSlipSchema = {
		DoubleSlipPaths,
		"040" "460" "610" "270" "750" "530" "451" "762" };
static toDesignSchema_t DoubleSlipSchema2 = {
		DoubleSlipPaths2,
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
		&CornuDesc,
		&WyeDesc,
		&CornuWyeDesc,
		&ThreewayDesc,
		&CornuThreewayDesc,
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

#ifndef MKTURNOUT
/* For Bezier Segs we need to duplicate the subSegs Array as well */
void AppendSegs(dynArr_t * target, dynArr_t * source) {

#define sourceSegs(N) DYNARR_N( trkSeg_t, *source, N )
#define targetSegs(N) DYNARR_N( trkSeg_t, *target, N )

	trkSeg_p src;

	for (int i=0;i<source->cnt; i++) {
		src = &sourceSegs(i);
		addSegBezier(target, src);
	}
}

/* Bezier Segs will have subSegs Array - free it before resetting the array */
void ClearSegs(dynArr_t * target) {
	for (int i=0;i<(*target).cnt;i++) {
		if (targetSegs(i).type == SEG_BEZTRK)
			if (targetSegs(i).bezSegs.ptr) MyFree(targetSegs(i).bezSegs.ptr);
		targetSegs(i).bezSegs.ptr = NULL;
		targetSegs(i).bezSegs.cnt = 0;
		targetSegs(i).bezSegs.max = 0;
	}
	DYNARR_RESET( trkSeg_t, *target );
}

BOOL_T CallCornuNoBez(coOrd pos[2], coOrd center[2], ANGLE_T angle[2], DIST_T radius[2], dynArr_t * array_p) {

	dynArr_t temp_array;
	DYNARR_RESET(trkSeg_t,temp_array);
	temp_array.ptr=0;
	temp_array.max=0;


	wBool_t rc = CallCornu0(pos,center,angle,radius, &temp_array, FALSE);

	if (!rc) return FALSE;

	for (int i=0;i<temp_array.cnt;i++) {
		trkSeg_p from_seg = &DYNARR_N(trkSeg_t,temp_array,i);
		if ((from_seg->type == SEG_BEZTRK) || (from_seg->type == SEG_BEZLIN)) {
			for (int j=0;j<from_seg->bezSegs.cnt;j++) {
				trkSeg_p sub_seg = &DYNARR_N(trkSeg_t,from_seg->bezSegs,j);
				DYNARR_APPEND(trkSeg_t,*array_p,5);
				trkSeg_p to_seg = &DYNARR_N(trkSeg_t,*array_p,(*array_p).cnt-1);
				to_seg->u = sub_seg->u;
				to_seg->type = sub_seg->type;
				to_seg->color = wDrawColorBlack;
				to_seg->width = sub_seg->width;
			}
		} else {
			DYNARR_APPEND(trkSeg_t,*array_p,5);
			trkSeg_p to_seg = &DYNARR_N(trkSeg_t,*array_p,(*array_p).cnt-1);
			to_seg->u = from_seg->u;
			to_seg->type = from_seg->type;
			to_seg->color = wDrawColorBlack;
			to_seg->width = from_seg->width;
		}
	}

	ClearSegs(&temp_array);

	return TRUE;
}

#endif


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
	ANGLE_T angle0, angle1, angle2, angle3;
	trkSeg_p segPtr;
	struct {
		coOrd pos[10];
		coOrd center[10];
		DIST_T radius[10];
		DIST_T angle[10];
	} cornuData;

	DYNARR_RESET( trkSeg_t, tempSegs_da );
	angle0 = newTurnAngle0;
	angle1 = newTurnAngle1;
	angle2 = newTurnAngle2;
	angle3 = newTurnAngle3;


	if ( newTurnAngleMode == 0 && dp->type != NTO_CRV_SECTION ) {
		/* convert from Frog Num to degrees */
		if ( angle0 > 0 )
			angle0 = R2D(asin(1.0 / angle0));
		if ( angle1 > 0 )
			angle1 = R2D(asin(1.0 / angle1));
		if ( angle2 > 0 )
			angle2 = R2D(asin(1.0 / angle2));
		if ( angle3 > 0 )
			angle3 = R2D(asin(1.0 / angle3));
	}

	pp = dp->paths;
	if (loadPoints) {
		DYNARR_RESET( trkEndPt_t, tempEndPts_da );
		for ( i=0; i<dp->floatCnt; i++ )
			if ( *(FLOAT_T*)(turnDesignPLs[dp->floats[i].index].valueP) == 0.0 )
				if (dp->type != NTO_CORNU &&
					dp->type != NTO_CORNUWYE &&
					dp->type != NTO_CORNU3WAY
						) {
					NoticeMessage( MSG_TODSGN_VALUES_GTR_0, _("Ok"), NULL );
					return NULL;
			}

		switch (dp->type) {
		case NTO_REGULAR:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 3 );
			if ( !ComputeCurve( &points[3], &points[4], &radii[0],
				(newTurnLen0), fabs(newTurnOff0), angle0 ) )
				return NULL;
			radii[0] = - radii[0];
			points[0].x = points[0].y = points[1].y = 0.0;
			points[1].x = (newTurnLen1);
			points[2].y = fabs(newTurnOff0);
			points[2].x = (newTurnLen0);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 90.0-angle0;
			break;

		case NTO_CURVED:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 3 );
			if ( !ComputeCurve( &points[3], &points[4], &radii[0],
				(newTurnLen0), fabs(newTurnOff0), angle0 ) )
				return NULL;
			if ( !ComputeCurve( &points[5], &points[6], &radii[1],
				(newTurnLen1), fabs(newTurnOff1), angle1 ) )
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
			points[1].y = fabs(newTurnOff0); points[1].x = (newTurnLen0);
			points[2].y = fabs(newTurnOff1); points[2].x = (newTurnLen1);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(2).pos = points[1]; tempEndPts(2).angle = 90.0-angle0;
			tempEndPts(1).pos = points[2]; tempEndPts(1).angle = 90.0-angle1;
			break;
#ifndef MKTURNOUT
		case NTO_CORNU:

			radii[0] =  fabs(newTurnRad2); /*Toe*/
			radii[1] =  fabs(newTurnRad0); /*Inner*/
			radii[2] =  fabs(newTurnRad1); /*Outer*/
			angles[0] = 0.0; 		  /*Base*/
			angles[1] = newTurnAngle0; /*Inner*/
			angles[2] = newTurnAngle1; /*Outer*/
			pp = &CornuSchema;
			points[0].x = points[0].y = 0.0;
			points[1].y = (newTurnOff0); points[1].x = (newTurnLen0); /*Inner*/
			points[2].y = (newTurnOff1); points[2].x = (newTurnLen1); /*Outer*/

			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(2).pos = points[1]; tempEndPts(2).angle = 90.0-angles[1];
			tempEndPts(1).pos = points[2]; tempEndPts(1).angle = 90.0-angles[2];

			break;
#endif
		case NTO_WYE:
		case NTO_3WAY:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, (dp->type==NTO_3WAY)?4:3 );
			if ( !ComputeCurve( &points[3], &points[4], &radii[0],
						(newTurnLen0), fabs(newTurnOff0), angle0 ) )
				return NULL;
			if ( !ComputeCurve( &points[5], &points[6], &radii[1],
						(newTurnLen1), fabs(newTurnOff1), angle1 ) )
				return NULL;
			points[5].y = - points[5].y;
			points[6].y = - points[6].y;
			radii[0] = - radii[0];
			points[0].x = points[0].y = 0.0;
			points[1].y = fabs(newTurnOff0);
			points[1].x = (newTurnLen0);
			points[2].y = -fabs(newTurnOff1);
			points[2].x = (newTurnLen1);
			points[7].y = 0;
			points[7].x = (newTurnLen2);
			d = points[3].x - points[5].x;
			if ( d < -0.10 ) {
				pp = (dp->type==NTO_3WAY ? &Tri3Schema : &Wye3Schema );
			} else if ( d > 0.10 ) {
				pp = (dp->type==NTO_3WAY ? &Tri2Schema : &Wye2Schema );
			} else {
				pp = (dp->type==NTO_3WAY ? &Tri1Schema : &Wye1Schema );
			}
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0-angle0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 90.0+angle1;
			if (dp->type == NTO_3WAY) {
				tempEndPts(3).pos = points[7]; tempEndPts(3).angle = 90.0;
			}
			break;
#ifndef MKTURNOUT
		case NTO_CORNUWYE:
		case NTO_CORNU3WAY:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, (dp->type==NTO_CORNU3WAY)?4:3 );

			/*
			 * Construct Wye and 3 Way Turnouts with Cornu curves
			 * 1. Establish where the joint(s) (Toes) are by using the main curve
			 * 2. Rebuild the segments into a single set using those points
			 * 3. Build Path statements to suit the segments
			 * ---------------------------------------------------------------------------------------------
			 *                        7				CornuData. Cheat Sheet - segment array parts
			 *           =============+				Note - 6-7 is at Toe2 and 8-9 at Toe1 if RH comes before LH
			 *         //    Toe 2                       - Toe2 and Toe1 are the same (no 2-3) if co-incident
			 * 0      6+    3  4                  5		 - Toe2, 2-3 and 4-5 all only exist for 3WAY not WYE
			 * +=====+ +=====+ +==================+		 - If zero radius at 0, curve starts at Toe 1
			 *       1 2      8+
			 *      Toe 1      \\         9
			 *                  =========+
			 *
			 * ---------------------------------------------------------------------------------------------
			 */

			radii[0] = (newTurnRad2); /*Base*/
			radii[1] = (newTurnRad0); /*Left*/
			radii[2] = (newTurnRad1); /*Right*/
			radii[3] = (newTurnRad3); /*Center*/
			angles[0] = 0.0; 		  /*Base*/
			angles[1] = newTurnAngle0; /*Left*/
			angles[2] = newTurnAngle1; /*Right*/
			angles[3] = newTurnAngle3; /*Center*/
			points[0].x = points[0].y = 0.0; /*Base*/
			points[1].y = (newTurnOff0);	/* Left */
			points[1].x = (newTurnLen0);
			points[2].y = -(newTurnOff1);   /* Right */
			points[2].x = (newTurnLen1);
			if (dp->type==NTO_CORNU3WAY) {
				points[3].y = (newTurnOff3);    /* Center */
				points[3].x = (newTurnLen3);
			}

			pp = (dp->type==NTO_CORNU3WAY ? &CornuTriSchema : &CornuWyeSchema );

			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;

			if (newTurnRad0<0.0) {
				tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0+angles[1];
			} else {
				tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0-angles[1];
			}
			if (newTurnRad1<0.0) {
				tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 90.0-angles[2];
			} else {
				tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 90.0+angles[2];
			}
			if (dp->type == NTO_CORNU3WAY) {
				if (newTurnRad3<0.0) {
					tempEndPts(3).pos = points[3]; tempEndPts(3).angle = 90.0+angles[3];
				} else {
					tempEndPts(3).pos = points[3]; tempEndPts(3).angle = 90.0-angles[3];
				}
			}

			DIST_T end_length = minLength/2;

			for (int i=0;i<((dp->type==NTO_CORNU3WAY)?4:3);i++) {
				if (radii[i] == 0.0) {
					Translate(&end_points[i], points[i], 90-angles[i]+(i==0?0:180), end_length);
					end_angles[i] = angles[i];
				} else {
					if (((i==0) && radii[0]>0.0) || ((i==1 || i==3) && radii[i]>0.0)|| ((i==2) && radii[i]<0.0))
						Translate(&end_centers[i], points[i], -angles[i], fabs(radii[i]));
					else
						Translate(&end_centers[i], points[i], angles[i], fabs(radii[i]));
					end_arcs[i] = (radii[i]>=0?1:-1)*R2D(end_length/fabs(radii[i]));
					end_points[i] = points[i];
					Rotate(&end_points[i],end_centers[i],((i==0||i==3)?-1:1)*end_arcs[i]);
					end_angles[i] = angles[i]-((i==0||i==3)?-1:1)*end_arcs[i];
				}
LogPrintf( "ctoDes0-%d: EP(%f,%f) NEP(%f,%f) EA(%f) NEA(%f) R(%f) ARC(%f) EC(%f,%f) \n",
					i+1,points[i].x,points[i].y,end_points[i].x,end_points[i].y,angles[i],end_angles[i],radii[i],end_arcs[i],
					end_centers[i].x,end_centers[i].y);
			}

			wBool_t LH_main = TRUE, LH_first = TRUE;

			cornuData.pos[0] = end_points[0]; /*Start*/
			if (dp->type == NTO_CORNU3WAY) {
				if (newTurnToeR < newTurnToeL) LH_first = FALSE;
				cornuData.pos[1] = end_points[3]; /*Center for First Time */
				cornuData.pos[5] = end_points[3]; /*Center for last time*/
			} else if (newTurnRad1>=0.0)  {
				cornuData.pos[1] = end_points[1];    /*Left is dominant curve */
				newTurnToeR = newTurnToeL;
			} else {
				cornuData.pos[1] = end_points[2];    /*Right is dominant */
				newTurnToeR = newTurnToeL;
				LH_main = FALSE;
			}

			cornuData.pos[7] = end_points[1]; /*Left*/
			cornuData.pos[9] = end_points[2]; /*Right*/
			if (dp->type == NTO_CORNU3WAY) {
				cornuData.pos[5] = end_points[3]; /*Center */
			}

			if (radii[0] == 0.0)  /* Base */
				cornuData.center[0] = zero;
			else {
				cornuData.center[0].x = end_points[0].x;
				cornuData.center[0].y = end_points[0].y + radii[0];
			}
			if (radii[1] == 0.0)  /* Left */
				cornuData.center[7] = zero;
			else if (radii[1] >0.0)
				Translate(&cornuData.center[7], cornuData.pos[7], -end_angles[1], radii[1]);
			else
				Translate(&cornuData.center[7], cornuData.pos[7], 180.0+end_angles[1], radii[1]);

			if (radii[2] == 0.0) /* Right */
				cornuData.center[9] = zero;
			else if (radii[2] >0.0)
				Translate(&cornuData.center[9], cornuData.pos[9], 180.0+end_angles[2], radii[2]);
			else
				Translate(&cornuData.center[9], cornuData.pos[9], -end_angles[2], radii[2]);

			if (dp->type == NTO_CORNU3WAY) {
				if (radii[3] == 0.0) /* Center */
					cornuData.center[5] = zero;
				else if (radii[3] >0.0)
					Translate(&cornuData.center[5], cornuData.pos[5], -end_angles[3], radii[3]);
				else
					Translate(&cornuData.center[5], cornuData.pos[5], 180.0+end_angles[3], radii[3]);
			}

			/* Set up for calculation of Toe(s) */

			if (dp->type == NTO_CORNU3WAY) {
				cornuData.center[1] = cornuData.center[5];	  /*For Toe1 calc  always use center */
				cornuData.center[3] = cornuData.center[5];    /*For Toe2 calc  always use center*/
			} else if (LH_main) {
				cornuData.center[1] = cornuData.center[7];    /* Dominant Curve Left */
			} else
				cornuData.center[1] = cornuData.center[9];	  /* Right */

			cornuData.angle[0] = 270.0;						 /*Always*/
			if (dp->type == NTO_CORNU3WAY) {
				cornuData.angle[1] = 90.0-end_angles[3];
				cornuData.angle[3] = 90.0-end_angles[3];
				cornuData.angle[5] = 90.0-end_angles[3]; 		/* Only used for 3way */
			} else if (LH_main) {
				cornuData.angle[1] = 90.0-end_angles[1];
			} else {
				cornuData.angle[1] = 90.0+end_angles[2];
			}
			cornuData.angle[7] = 90.0-end_angles[1]; 			/*Left*/
			cornuData.angle[9] = 90.0+end_angles[2]; 			/*Right*/

			cornuData.radius[0] = fabs(radii[0]);
			if (dp->type == NTO_CORNU3WAY) {
				cornuData.radius[1] = fabs(radii[3]);
				cornuData.radius[3] = fabs(radii[3]);
				cornuData.radius[5] = fabs(radii[3]);
			} else if (LH_main) {
				cornuData.radius[1] = fabs(radii[1]);
			} else {
				cornuData.radius[1] = fabs(radii[2]);
			}
			cornuData.radius[7] = fabs(radii[1]); 			/*Left*/
			cornuData.radius[9] = fabs(radii[2]); 			/*Right*/

			/* Ready to find Toe points */

			DYNARR_RESET( trkSeg_t, tempSegs_da );
			trkSeg_t * temp_p;
			temp_p = &tempSegs(0);


			DIST_T radius;
			coOrd center;
			ANGLE_T angle;
			int inx,subSeg;
			wBool_t back, neg;

			CallCornu0(&cornuData.pos[0],&cornuData.center[0],&cornuData.angle[0],&cornuData.radius[0],&tempSegs_da, FALSE);

			/* Override if a "Y" has zero radius at base to be a straight until the Toe
			 * We set the start of the curve to be at the Toe position */
			if (cornuData.radius[0] == 0.0) {
				pos.x = end_points[0].x+(LH_first?newTurnToeL:newTurnToeR);
				pos.y = 0.0;
				angle = 90.0;
				radius = 0.0;
				center = zero;

			} else {

				/*Find Toe 1 from curve */

				/*Get ToeAngle/Radius/Center for first toe */
				pos.x = end_points[0].x+(LH_first?newTurnToeL:newTurnToeR);
				pos.y = end_points[0].y; 				/* This will be close to but not on the curve */
				angle = GetAngleSegs(tempSegs_da.cnt,(trkSeg_t *)(tempSegs_da.ptr),&pos,&inx,NULL,&back,&subSeg,&neg);
				segPtr = &DYNARR_N(trkSeg_t, tempSegs_da, inx);

				if (segPtr->type == SEG_BEZTRK) {
					segPtr = &DYNARR_N(trkSeg_t,segPtr->bezSegs,subSeg);
				}

				if (segPtr->type == SEG_STRTRK) {
					radius = 0.0;
					center = zero;
				} else if (segPtr->type == SEG_CRVTRK) {
					center = segPtr->u.c.center;
					radius = fabs(segPtr->u.c.radius);
				}
			}

			/* Set up 2-3 even if we don't use it */
			cornuData.pos[1] = pos;
			cornuData.center[1] = center;
			cornuData.angle[1] = angle;
			cornuData.radius[1] = radius;

			cornuData.pos[2] = pos;
			cornuData.center[2] = center;
			cornuData.angle[2]  = NormalizeAngle(180.0+angle);
			cornuData.radius[2] = radius;

			if ((dp->type == NTO_CORNU3WAY) && (newTurnToeR!=newTurnToeL)) {
				if (LH_first) {
					cornuData.pos[6] = pos;
					cornuData.center[6] = center;
					cornuData.angle[6] = NormalizeAngle(180.0+angle);
					cornuData.radius[6] = radius;
				} else {
					cornuData.pos[8] = pos;
					cornuData.center[8] = center;
					cornuData.angle[8] = NormalizeAngle(180.0+angle);
					cornuData.radius[8] = radius;
				}
			} else {   /* Just one toe */
				cornuData.pos[8] = pos;
				cornuData.center[8] = center;
				cornuData.angle[8] = NormalizeAngle(180.0+angle);
				cornuData.radius[8] = radius;

				cornuData.pos[6] = pos;
				cornuData.center[6] = center;
				cornuData.angle[6] = NormalizeAngle(180.0+angle);
				cornuData.radius[6] = radius;
			}

			if (dp->type == NTO_CORNU3WAY) {
			  	if (newTurnToeR!=newTurnToeL) {
					/* Second Toe */
					pos.x = end_points[0].x+(LH_first?newTurnToeR:newTurnToeL);
					pos.y = end_points[0].y; 				/* This will be close to but not on the curve */
					angle = GetAngleSegs(tempSegs_da.cnt,(trkSeg_t *)(tempSegs_da.ptr),&pos,&inx,NULL,&back,&subSeg,&neg);
					segPtr = &DYNARR_N(trkSeg_t, tempSegs_da, inx);

					if (segPtr->type == SEG_BEZTRK) {
						segPtr = &DYNARR_N(trkSeg_t,segPtr->bezSegs,subSeg);
					}

					if (segPtr->type == SEG_STRTRK) {
						radius = 0.0;
						center = zero;
					} else if (segPtr->type == SEG_CRVTRK) {
						center = segPtr->u.c.center;
						radius = fabs(segPtr->u.c.radius);
					}
					cornuData.pos[3] = pos;
					cornuData.center[3] = center;
					cornuData.angle[3] = angle;
					cornuData.radius[3] = radius;

					cornuData.pos[4] = pos;
					cornuData.center[4] = center;
					cornuData.angle[4] = NormalizeAngle(180.0+angle);
					cornuData.radius[4] = radius;

					if (LH_first) {
						cornuData.pos[8] = pos;
						cornuData.center[8] = center;
						cornuData.angle[8] = NormalizeAngle(180.0+angle);
						cornuData.radius[8] = radius;

						cornuData.pos[4] = pos;
						cornuData.center[4] = center;
						cornuData.angle[4] = NormalizeAngle(180.0+angle);
						cornuData.radius[4] = radius;
					} else {
						cornuData.pos[6] = pos;
						cornuData.center[6] = center;
						cornuData.angle[6] = NormalizeAngle(180.0+angle);
						cornuData.radius[6] = radius;

						cornuData.pos[4] = pos;
						cornuData.center[4] = center;
						cornuData.angle[4] = NormalizeAngle(180.0+angle);
						cornuData.radius[4] = radius;
					}
				} else {   //Set next center start to same place
					cornuData.pos[4] = pos;
					cornuData.center[4] = center;
					cornuData.angle[4] = NormalizeAngle(180.0+angle);
					cornuData.radius[4] = radius;
				}
			}

			static dynArr_t cornuSegs_da;

			ClearSegs(&tempSegs_da);
			ClearSegs(&cornuSegs_da);

			int Toe1Seg = 0 , Toe2Seg = 0, CenterEndSeg = 0, LeftEndSeg = 0, RightEndSeg = 0;

			/* Override if at zero radius at base don't compute end */
			if (cornuData.radius[0] == 0.0) {
				DYNARR_APPEND(trkSeg_t,tempSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,tempSegs_da);
				temp_p->type = SEG_STRTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.l.pos[0] = zero;
				temp_p->u.l.pos[1] = cornuData.pos[0];
LogPrintf( "ctoDes1: P0(%f,%f) P1(%f,%f) \n",
				temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
			} else {
				DYNARR_APPEND(trkSeg_t,tempSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,tempSegs_da);
				temp_p->type = SEG_CRVTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.c.radius = fabs(radii[0]);;
				if (radii[0]>0.0)
					temp_p->u.c.a0 = FindAngle(end_centers[0],end_points[0]);
				else
					temp_p->u.c.a0 = FindAngle(end_centers[0],points[0]);
				temp_p->u.c.a1 = fabs(end_arcs[0]);
				temp_p->u.c.center = end_centers[0];
				coOrd rp0,rp1;
				Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
				Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
LogPrintf( "ctoDes1: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f), EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
		temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
		points[0].x,points[0].y,end_points[0].x,end_points[0].y,
		rp0.x,rp0.y,rp1.x,rp1.y);
			}

			if ((cornuData.pos[0].x != cornuData.pos[1].x) ||
				(cornuData.pos[0].y != cornuData.pos[1].y) )
				CallCornuNoBez(&cornuData.pos[0],&cornuData.center[0],&cornuData.angle[0],&cornuData.radius[0],&tempSegs_da);
			Toe1Seg = tempSegs_da.cnt;

			if (dp->type == NTO_CORNU3WAY) {
				if (newTurnToeR!=newTurnToeL) {
					/* Toe1 to Toe2 in tempSegs array */
					if ((cornuData.pos[2].x != cornuData.pos[3].x) ||
						(cornuData.pos[2].y != cornuData.pos[3].y) )
						CallCornuNoBez(&cornuData.pos[2],&cornuData.center[2],&cornuData.angle[2],&cornuData.radius[2],&cornuSegs_da);

					Toe2Seg = cornuSegs_da.cnt+Toe1Seg;
					/* Add to second cornu to tempSegs array */
					AppendSegs(&tempSegs_da,&cornuSegs_da);
					/* Get ready to reuse cornuSegs array*/
					ClearSegs(&cornuSegs_da);
				} else {
					Toe2Seg = Toe1Seg;				 //No Toe2
				}
				/* Toe2 to Center in cornuSegs array */
				CallCornuNoBez(&cornuData.pos[4],&cornuData.center[4],&cornuData.angle[4],&cornuData.radius[4],&cornuSegs_da);

				if (cornuData.radius[5] == 0.0) {
					DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
					temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
					temp_p->type = SEG_STRTRK;
					temp_p->color = wDrawColorBlack;
					temp_p->width = 0.0;
					temp_p->u.l.pos[0] = cornuData.pos[5];
					temp_p->u.l.pos[1] = end_points[3];
	LogPrintf( "ctoDes2: P0(%f,%f) P1(%f,%f) \n",
					temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
				} else {
					DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
					temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
					temp_p->type = SEG_CRVTRK;
					temp_p->color = wDrawColorBlack;
					temp_p->width = 0.0;
					temp_p->u.c.radius = fabs(radii[3]);
					if (radii[3]>0)
						temp_p->u.c.a0 = FindAngle(end_centers[3],points[3]);
					else
						temp_p->u.c.a0 = FindAngle(end_centers[3],end_points[3]);
					temp_p->u.c.a1 = fabs(end_arcs[3]);
					temp_p->u.c.center = end_centers[3];
					coOrd rp0,rp1;
					Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
					Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
	LogPrintf( "ctoDes2: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f) EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
						temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
						points[3].x,points[3].y,end_points[3].x,end_points[3].y,
						rp0.x,rp0.y,rp1.x,rp1.y);
				}

				CenterEndSeg = cornuSegs_da.cnt+Toe2Seg;
				/* Add to second cornu to tempSegs array */
				AppendSegs(&tempSegs_da,&cornuSegs_da);
				/* Get ready to reuse cornuSegs array*/
				ClearSegs(&cornuSegs_da);
			} else {
				CenterEndSeg = Toe2Seg = Toe1Seg;	//No Toe2, No Center
			}

			/* Left in cornuSegs array*/
			CallCornuNoBez(&cornuData.pos[6],&cornuData.center[6],&cornuData.angle[6],&cornuData.radius[6],&cornuSegs_da);

			if (cornuData.radius[7] == 0.0) {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_STRTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.l.pos[0] = cornuData.pos[7];
				temp_p->u.l.pos[1] = end_points[1];
LogPrintf( "ctoDes2: P0(%f,%f) P1(%f,%f) \n",
				temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
			} else {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_CRVTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.c.radius = fabs(radii[1]);
				if (radii[1]>0)
					temp_p->u.c.a0 = FindAngle(end_centers[1],points[1]);
				else
					temp_p->u.c.a0 = FindAngle(end_centers[1],end_points[1]);
				temp_p->u.c.a1 = fabs(end_arcs[1]);
				temp_p->u.c.center = end_centers[1];
				coOrd rp0,rp1;
				Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
				Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
LogPrintf( "ctoDes2: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f) EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
					temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
					points[1].x,points[1].y,end_points[1].x,end_points[1].y,
					rp0.x,rp0.y,rp1.x,rp1.y);
			}

			LeftEndSeg = cornuSegs_da.cnt+CenterEndSeg;

			/* Add to second cornu to tempSegs array */
			AppendSegs(&tempSegs_da,&cornuSegs_da);
			/* Get ready to reuse cornuSegs array*/
			ClearSegs(&cornuSegs_da);

			/* Right in cornuSegs array*/
			CallCornuNoBez(&cornuData.pos[8],&cornuData.center[8],&cornuData.angle[8],&cornuData.radius[8],&cornuSegs_da);

			if (cornuData.radius[9] == 0.0) {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_STRTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.l.pos[0] = cornuData.pos[9];
				temp_p->u.l.pos[1] = end_points[2];
LogPrintf( "ctoDes2: P0(%f,%f) P1(%f,%f) \n",
				temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
			} else {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_CRVTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.c.radius = fabs(radii[2]);
				if (radii[2]<0)
					temp_p->u.c.a0 = FindAngle(end_centers[2],points[2]);
				else
					temp_p->u.c.a0 = FindAngle(end_centers[2],end_points[2]);
				temp_p->u.c.a1 = fabs(end_arcs[2]);
				temp_p->u.c.center = end_centers[2];
				coOrd rp0,rp1;
				Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
				Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
LogPrintf( "ctoDes2: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f) EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
					temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
					points[2].x,points[2].y,end_points[2].x,end_points[2].y,
					rp0.x,rp0.y,rp1.x,rp1.y);
			}

			RightEndSeg = cornuSegs_da.cnt+LeftEndSeg;

			/*Add Third Part to tempSegs Array */
			AppendSegs(&tempSegs_da,&cornuSegs_da);
			/* Safety - clear out cornu Array */
			ClearSegs(&cornuSegs_da);

			if (tempSegs_da.cnt >128 ) {
				NoticeMessage( MSG_TODSGN_CORNU_TOO_COMPLEX, _("Ok"), NULL );
				return NULL;
			}

			/* Generate Paths */

			static char pathChar[512];
			if (dp->type == NTO_CORNU3WAY) {
				strcpy(pathChar,"Normal");  /* Also resets array */
				pathLen = strlen(pathChar)+1;
				for (uint8_t i=0;i<CenterEndSeg;i++) {
					pathChar[pathLen] = i+1;
					pathLen++;
				}
				pathChar[pathLen] = 0;
				pathLen++;
				pathChar[pathLen] = 0;
				pathLen++;
				sprintf(&pathChar[pathLen],"%s","Left");
				pathLen += strlen(&pathChar[pathLen])+1;
			} else {
				strcpy(pathChar,"Left");
				pathLen = strlen(pathChar)+1;
			}
			for (uint8_t i=0;i<Toe1Seg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}
			if ((dp->type == NTO_CORNU3WAY) && !LH_first && (newTurnToeR != newTurnToeL)) {
				for (uint8_t i=Toe1Seg;i<Toe2Seg;i++) {
					pathChar[pathLen] = i+1;
					pathLen++;
				}
			}

			for (uint8_t i=CenterEndSeg;i<LeftEndSeg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}

			pathChar[pathLen] = 0;
			pathLen++;
			pathChar[pathLen] = 0;
			pathLen++;

			sprintf(&pathChar[pathLen],"%s","Right");
			pathLen += strlen(&pathChar[pathLen])+1;

			for (uint8_t i=0;i<Toe1Seg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}
			if ((dp->type == NTO_CORNU3WAY) && LH_first && (newTurnToeR != newTurnToeL)) {
				for (uint8_t i=Toe1Seg;i<Toe2Seg;i++) {
					pathChar[pathLen] = i+1;
					pathLen++;
				}
			}
			for (uint8_t i=LeftEndSeg;i<RightEndSeg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}
			pathChar[pathLen] = 0;
			pathLen++;
			pathChar[pathLen] = 0;
			pathLen++;
			pathChar[pathLen] = 0;
			pathLen++;

			pp->paths = (signed char *)pathChar;
			segCnt = tempSegs_da.cnt;

			break;
#endif

		case NTO_D_SLIP:
		case NTO_S_SLIP:
		case NTO_CROSSING:
			if (dp->type == NTO_D_SLIP) {
				if  (newTurnSlipMode == 1)
					pp = &DoubleSlipSchema2;
				else
					pp = &DoubleSlipSchema;
			}
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 4 );
			points[0].x = points[0].y = points[1].y = 0.0;
			points[1].x = (newTurnLen0);
			pos.y = 0; pos.x = (newTurnLen0)/2.0;
			coOrd cpos = pos;
			Translate( &points[3], pos, 90.0+angle0, (newTurnLen1)/2.0 );
			points[2].y = - points[3].y;
			points[2].x = pos.x-(points[3].x-pos.x);
			if (dp->type != NTO_CROSSING) {
				Translate( &pos, points[3], 90.0+angle0, -newTurnTrackGauge );
				if (!ComputeCurve( &points[4], &points[5], &radii[0],
						pos.x, fabs(pos.y), angle0 )) /*???*/
					return NULL;
				radii[1] = - radii[0];
				points[5].y = - points[5].y;
				points[6].y = 0; points[6].x = cpos.x-(points[4].x-cpos.x);
				points[7].y = -points[5].y;
				points[7].x = cpos.x-(points[5].x-cpos.x);
			}
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 270.0+angle0;
			tempEndPts(3).pos = points[3]; tempEndPts(3).angle = 90.0+angle0;
			break;

		case NTO_R_CROSSOVER:
		case NTO_L_CROSSOVER:
		case NTO_D_CROSSOVER:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 4 );
			d = (newTurnLen0)/2.0 - newTurnTrackGauge;
			if (d < 0.0) {
				NoticeMessage( MSG_TODSGN_CROSSOVER_TOO_SHORT, _("Ok"), NULL );
				return NULL;
			}
			angle0 = R2D( atan2( fabs(newTurnOff0), d ) );
			points[0].y = 0.0; points[0].x = 0.0;
			points[1].y = 0.0; points[1].x = (newTurnLen0);
			points[2].y = fabs(newTurnOff0); points[2].x = 0.0;
			points[3].y = fabs(newTurnOff0); points[3].x = (newTurnLen0);
			if (!ComputeCurve( &points[4], &points[5], &radii[1],
				(newTurnLen0)/2.0, fabs(newTurnOff0)/2.0, angle0 ) )
				return NULL;
			radii[0] = - radii[1];
			points[6].y = 0.0; points[6].x = (newTurnLen0)-points[4].x;
			points[7].y = points[5].y; points[7].x = (newTurnLen0)-points[5].x;
			points[8].y = fabs(newTurnOff0); points[8].x = points[4].x;
			points[9].y = fabs(newTurnOff0)-points[5].y; points[9].x = points[5].x;
			points[10].y = fabs(newTurnOff0); points[10].x = points[6].x;
			points[11].y = points[9].y; points[11].x = points[7].x;
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			tempEndPts(2).pos = points[2]; tempEndPts(2).angle = 270.0;
			tempEndPts(3).pos = points[3]; tempEndPts(3).angle = 90.0;
			break;

		case NTO_STR_SECTION:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 2 );
			points[0].y = points[0].x = 0;
			points[1].y = 0/*(newTurnOff1)*/; points[1].x = (newTurnLen0);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0;
			break;

		case NTO_CRV_SECTION:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 2 );
			points[0].y = points[0].x = 0;
			points[1].y = (newTurnLen0) * (1.0 - cos( D2R(angle0) ) );
			points[1].x = (newTurnLen0) * sin( D2R(angle0) );
			radii[0] = -(newTurnLen0);
			tempEndPts(0).pos = points[0]; tempEndPts(0).angle = 270.0;
			tempEndPts(1).pos = points[1]; tempEndPts(1).angle = 90.0-angle0;
			break;

		case NTO_BUMPER:
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 1 );
			points[0].y = points[0].x = 0;
			points[1].y = 0/*(newTurnOff1)*/; points[1].x = (newTurnLen0);
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

#ifndef MKTURNOUT
	if(dp->type == NTO_CORNU) {
			DYNARR_SET( trkEndPt_t, tempEndPts_da, 3 );

			DIST_T end_length = minLength/2;

			// Adjust end_points to impose small fixed end segments

			for (int i=0;i<3;i++) {
				if (radii[i] == 0.0) {
					Translate(&end_points[i], points[i], 90-angles[i]+(i==0?0:180), end_length);
					end_angles[i] = angles[i];
				} else {
					Translate(&end_centers[i], points[i], -angles[i], radii[i]);
					end_arcs[i] = (radii[i]>=0?1:-1)*R2D(end_length/fabs(radii[i]));
					end_points[i] = points[i];
					Rotate(&end_points[i],end_centers[i],(i>0?1:-1)*end_arcs[i]);
					end_angles[i] = angles[i]-(i>0?1:-1)*end_arcs[i];
				}
LogPrintf( "ctoDes0-%d: EP(%f,%f) NEP(%f,%f) EA(%f) NEA(%f) R(%f) ARC(%f) EC(%f,%f) \n",
					i+1,points[i].x,points[i].y,end_points[i].x,end_points[i].y,angles[i],end_angles[i],radii[i],end_arcs[i],
					end_centers[i].x,end_centers[i].y);
			}


			cornuData.pos[0] = end_points[0]; /*Start*/
			cornuData.pos[1] = end_points[2]; /*Outer*/
			cornuData.pos[3] = end_points[2]; /*Outer for second time*/
			cornuData.pos[5] = end_points[1]; /*Inner*/


			if (radii[0] == 0.0)  /* Toe */
				cornuData.center[0] = zero;
			else {
				cornuData.center[0].x = end_points[0].x;
				cornuData.center[0].y = end_points[0].y + radii[0];
			}
			if (radii[1] == 0.0)  /* Inner */
				cornuData.center[5] = zero;
			else
				Translate(&cornuData.center[5], cornuData.pos[5], -end_angles[1], radii[1]);

			if (radii[2] == 0.0) /* Outer */
				cornuData.center[1] = zero;
			else
				Translate(&cornuData.center[1], cornuData.pos[1], -end_angles[2], radii[2]);
			cornuData.center[3] = cornuData.center[1];

			cornuData.angle[0] = 270.0;
			cornuData.angle[1] = 90.0-end_angles[2];
			cornuData.angle[3] = 90.0-end_angles[2];
			cornuData.angle[5] = 90.0-end_angles[1]; /*Inner*/

			cornuData.radius[0] = fabs(radii[0]);
			cornuData.radius[1] = fabs(radii[2]);
			cornuData.radius[3] = fabs(radii[2]);
			cornuData.radius[5] = fabs(radii[1]); /*Inner*/

			DYNARR_RESET( trkSeg_t, tempSegs_da );
			trkSeg_t * temp_p, * cornu_p;
			temp_p = &tempSegs(0);

			/*Map out the full outer curve */

			CallCornu0(&cornuData.pos[0],&cornuData.center[0],&cornuData.angle[0],&cornuData.radius[0],&tempSegs_da, FALSE);

			/*Get ToeAngle/Radius/Center */
			int inx,subSeg;
			wBool_t back, neg;
			DIST_T radius;
			coOrd center;
			pos.x = end_points[0].x+newTurnToeL;
			pos.y = end_points[0].y; 				/* This will be close to but not on the curve */
			ANGLE_T angle = GetAngleSegs(tempSegs_da.cnt,(trkSeg_t *)(tempSegs_da.ptr),&pos,&inx,NULL,&back,&subSeg,&neg);
			segPtr = &DYNARR_N(trkSeg_t, tempSegs_da, inx);

			if (segPtr->type == SEG_BEZTRK) {
				segPtr = &DYNARR_N(trkSeg_t,segPtr->bezSegs,subSeg);
			}

			if (segPtr->type == SEG_STRTRK) {
			    radius = 0.0;
			    center = zero;
			} else if (segPtr->type == SEG_CRVTRK) {
			    center = segPtr->u.c.center;
			    radius = fabs(segPtr->u.c.radius);
			}
			cornuData.pos[1] = pos;
			cornuData.center[1] = center;
			cornuData.angle[1] = angle;
			cornuData.radius[1] = radius;
			cornuData.pos[2] = pos;
			cornuData.center[2] = center;
			cornuData.angle[2] = NormalizeAngle(180.0+angle);
			cornuData.radius[2] = radius;
			cornuData.pos[4] = pos;
		    cornuData.center[4] = center;
			cornuData.angle[4] = NormalizeAngle(180.0+angle);
			cornuData.radius[4] = radius;

			static dynArr_t cornuSegs_da;

			ClearSegs(&tempSegs_da);
			ClearSegs(&cornuSegs_da);

			/* Override if at zero radius at base don't compute end */
			if (cornuData.radius[0] == 0.0) {
				DYNARR_APPEND(trkSeg_t,tempSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,tempSegs_da);
				temp_p->type = SEG_STRTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.l.pos[0] = zero;
				temp_p->u.l.pos[1] = cornuData.pos[1];
LogPrintf( "ctoDes1: P0(%f,%f) P1(%f,%f) \n",
				temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
			} else {
				DYNARR_APPEND(trkSeg_t,tempSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,tempSegs_da);
				temp_p->type = SEG_CRVTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.c.radius = fabs(radii[0]);;
				if (radii[0]>0.0)
					temp_p->u.c.a0 = FindAngle(end_centers[0],end_points[0]);
				else
					temp_p->u.c.a0 = FindAngle(end_centers[0],points[0]);
				temp_p->u.c.a1 = fabs(end_arcs[0]);
				temp_p->u.c.center = end_centers[0];
				coOrd rp0,rp1;
				Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
				Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
LogPrintf( "ctoDes1: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f), EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
		temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
		points[0].x,points[0].y,end_points[0].x,end_points[0].y,
		rp0.x,rp0.y,rp1.x,rp1.y);

				/* Base to Toe in tempSegs array */
				CallCornuNoBez(&cornuData.pos[0],&cornuData.center[0],& cornuData.angle[0],&cornuData.radius[0],&tempSegs_da);
			}

			int ToeSeg = tempSegs_da.cnt;

			/* Toe to Outer in cornuSegs array */
			CallCornuNoBez(&cornuData.pos[2],&cornuData.center[2],&cornuData.angle[2],&cornuData.radius[2],&cornuSegs_da);

			cornu_p = (trkSeg_p)cornuSegs_da.ptr;

			if (cornuData.radius[3] == 0.0) {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_STRTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.l.pos[0] = cornuData.pos[3];
				temp_p->u.l.pos[1] = end_points[2];
LogPrintf( "ctoDes2: P0(%f,%f) P1(%f,%f) \n",
				temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
			} else {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_CRVTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.c.radius = fabs(radii[2]);
				if (radii[2]>0)
					temp_p->u.c.a0 = FindAngle(end_centers[2],points[2]);
				else
					temp_p->u.c.a0 = FindAngle(end_centers[2],end_points[2]);
				temp_p->u.c.a1 = fabs(end_arcs[2]);
				temp_p->u.c.center = end_centers[2];
				coOrd rp0,rp1;
				Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
				Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
LogPrintf( "ctoDes2: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f) EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
					temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
					points[2].x,points[2].y,end_points[2].x,end_points[2].y,
					rp0.x,rp0.y,rp1.x,rp1.y);
			}

			int OuterEndSeg = cornuSegs_da.cnt + ToeSeg;

			/* Add to second cornu to tempSegs array */
			AppendSegs(&tempSegs_da,&cornuSegs_da);

			/* Get ready to reuse cornuSegs array*/
			ClearSegs(&cornuSegs_da);

			/* Toe to Inner in cornuSegs array*/
			CallCornuNoBez(&cornuData.pos[4],&cornuData.center[4],&cornuData.angle[4],&cornuData.radius[4],&cornuSegs_da);

			if (cornuData.radius[5] == 0.0) {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_STRTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.l.pos[0] = cornuData.pos[5];
				temp_p->u.l.pos[1] = points[1];
LogPrintf( "ctoDes3: P0(%f,%f) P1(%f,%f) \n",
				temp_p->u.l.pos[0].x,temp_p->u.l.pos[0].y,temp_p->u.l.pos[1].x,temp_p->u.l.pos[1].y  );
			} else {
				DYNARR_APPEND(trkSeg_t,cornuSegs_da,1);
				temp_p = &DYNARR_LAST(trkSeg_t,cornuSegs_da);
				temp_p->type = SEG_CRVTRK;
				temp_p->color = wDrawColorBlack;
				temp_p->width = 0.0;
				temp_p->u.c.radius = fabs(radii[1]);
				if (radii[1]>0)
					temp_p->u.c.a0 = FindAngle(end_centers[1],points[1]);
				else
					temp_p->u.c.a0 = FindAngle(end_centers[1],end_points[1]);
				temp_p->u.c.a1 = fabs(end_arcs[1]);
				temp_p->u.c.center = end_centers[1];
				coOrd rp0,rp1;
				Translate(&rp0,temp_p->u.c.center,temp_p->u.c.a0,temp_p->u.c.radius);
				Translate(&rp1,temp_p->u.c.center,temp_p->u.c.a0+temp_p->u.c.a1,temp_p->u.c.radius);
LogPrintf( "ctoDes3: R(%f) A0(%f) A1(%f) C(%f,%f) P(%f,%f) EP(%f,%f) RP0(%f,%f) RP1(%f,%f)\n",
					temp_p->u.c.radius,temp_p->u.c.a0,temp_p->u.c.a1,temp_p->u.c.center.x,temp_p->u.c.center.y,
					points[1].x,points[1].y,end_points[1].x,end_points[1].y,
					rp0.x,rp0.y,rp1.x,rp1.y);
			}

			int InnerEndSeg = cornuSegs_da.cnt + OuterEndSeg;

			/*Add Third Part to tempSegs Array */
			AppendSegs(&tempSegs_da,&cornuSegs_da);


			/* Safety - clear out cornu Array */
			ClearSegs(&cornuSegs_da);

			if (tempSegs_da.cnt >128 ) {
				NoticeMessage( MSG_TODSGN_CORNU_TOO_COMPLEX, _("Ok"), NULL );
				return NULL;
			}

			static char pathChar[512];
			strcpy(pathChar,"Normal");  /* Also resets array */

			pathLen = strlen(pathChar)+1;

			for (uint8_t i=0;i<OuterEndSeg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}
			pathChar[pathLen] = 0;
			pathLen++;
			pathChar[pathLen] = 0;
			pathLen++;

			sprintf(&pathChar[pathLen],"%s","Reverse");

			pathLen += strlen(&pathChar[pathLen])+1;
			for (uint8_t i=0;i<ToeSeg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}
			for (uint8_t i=OuterEndSeg;i<InnerEndSeg;i++) {
				pathChar[pathLen] = i+1;
				pathLen++;
			}
			pathChar[pathLen] = 0;
			pathLen++;
			pathChar[pathLen] = 0;
			pathLen++;
			pathChar[pathLen] = 0;
		    pathLen++;

			pp->paths = (signed char *)pathChar;
			segCnt = tempSegs_da.cnt;
	}
#endif

	if (!( (dp->type== NTO_CORNU) || (dp->type == NTO_CORNUWYE) || (dp->type == NTO_CORNU3WAY))) {
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
			if (p == 3) {
				/* cornu */
			} else if (p != 0) {
				segPtr->type = SEG_CRVTRK;
				ComputeCurvedSeg( segPtr, radii[p-1], points[p0], points[p1] );
			} else {
				segPtr->type = SEG_STRTRK;
				segPtr->u.l.pos[0] = points[p0];
				segPtr->u.l.pos[1] = points[p1];

			}
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
		if ( sp0->type != SEG_STRTRK && sp0->type != SEG_CRVTRK && sp0->type != SEG_BEZTRK ) {
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
	if (curDesign->type == NTO_REGULAR || curDesign->type == NTO_CURVED || curDesign->type == NTO_CORNU ) {
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
			case Rad_e:
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

	DIST_T * radii_ends = NULL;

	 if ((curDesign->type == NTO_CORNU) ||
		 (curDesign->type == NTO_CORNUWYE) ||
		 (curDesign->type == NTO_CORNU3WAY)) {
		 radii_ends = &radii[0];
	 }

	to = CreateNewTurnout( newTurnScaleName, tempCustom, tempSegs_da.cnt, &tempSegs(0),
						pathLen, pp->paths, tempEndPts_da.cnt, &tempEndPts(0), radii, FALSE );
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
		points[3].y = - points[3].y;
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
						pathLen, pp->paths, tempEndPts_da.cnt, &tempEndPts(0), NULL, FALSE );
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
	case NTO_CORNU:
		points[2].y = - points[2].y;
		points[1].y = - points[1].y;
		points[3].y = - points[3].y;
		points[4].y = - points[4].y;
		points[5].y = - points[5].y;
		points[6].y = - points[6].y;
		radii[0] = -radii[0];
		radii[1] = -radii[1];
		radii[2] = -radii[2];
		radii[3] = -radii[3];
		radii[4] = -radii[4];
		radii[5] = -radii[5];
		radii[6] = -radii[6];
		angles[0] = -angles[0];
		angles[1] = -angles[1];
		angles[2] = -angles[2];
		angles[3] = -angles[3];
		angles[4] = -angles[4];
		angles[5] = -angles[5];
		angles[6] = -angles[6];
		LoadSegs( curDesign, FALSE, &pathLen );
		tempEndPts(1).pos.y = - tempEndPts(1).pos.y;
		tempEndPts(1).angle = 180.0 - tempEndPts(1).angle;
		tempEndPts(2).pos.y = - tempEndPts(2).pos.y;
		tempEndPts(2).angle = 180.0 - tempEndPts(2).angle;
		BuildTrimedTitle( tempCustom, "\t", newTurnManufacturer, newTurnRightDesc, newTurnRightPartno );
		//tempSegs_da.cnt = segCnt;
#ifndef MKTURNOUT
		if (includeNontrackSegments && customTurnout2)
			CopyNonTracks( customTurnout2 );
		if ( customTurnout2 )
			customTurnout2->segCnt = 0;
		to = CreateNewTurnout( newTurnScaleName, tempCustom, tempSegs_da.cnt, &tempSegs(0),
						pathLen, pp->paths, tempEndPts_da.cnt, &tempEndPts(0), NULL, FALSE );
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
		if (curDesign->type == NTO_D_SLIP) {
			turnDesignPLs[I_TOSLIPMODE].option &= ~PDO_DLGIGNORE;
			wControlShow( turnDesignPLs[I_TOSLIPMODE].control, TRUE );
		} else {
			turnDesignPLs[I_TOSLIPMODE].option |= PDO_DLGIGNORE;
			wControlShow( turnDesignPLs[I_TOSLIPMODE].control, FALSE );
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
	wBool_t sameTurnout = FALSE;
	if (recordF)
		fprintf( recordF, TURNOUTDESIGNER " SHOW %s\n", ((toDesignDesc_t*)context)->label );
	newTurnScaleName = curScaleName;
	newTurnTrackGauge = trackGauge;
	if (context && (curDesign == context))
		sameTurnout = TRUE;
	SetupTurnoutDesignerW( (toDesignDesc_t*)context );
	if (!sameTurnout) {  /* Clear Values unless same as last time */
		newTurnRightDesc[0] = '\0';
		newTurnRightPartno[0] = '\0';
		newTurnLeftDesc[0] = '\0';
		newTurnLeftPartno[0] = '\0';
		newTurnOff0 = newTurnLen0 = newTurnAngle0 = newTurnRad0 =
		newTurnOff1 = newTurnLen1 = newTurnAngle1 = newTurnRad1 =
		newTurnOff2 = newTurnLen2 = newTurnAngle2 = newTurnRad2 =
		newTurnOff3 = newTurnLen3 = newTurnAngle3 = newTurnRad3 =
		newTurnToeL = newTurnToeR = 0.0;
	}
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
	if (dp->type == NTO_REGULAR || dp->type == NTO_CURVED || dp->type == NTO_CORNU) {
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
		case Rad_e:
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
				case SEG_BEZTRK:
					break;
				default:
					segsDiff = TRUE;
				}
			}
		} else {
			for ( sp0=to->segs; (!segsDiff) && sp0<&to->segs[to->segCnt]; sp0++ ) {
				if ( sp0->type != SEG_STRTRK && sp0->type != SEG_CRVTRK && sp0->type != SEG_BEZTRK)
					segsDiff = TRUE;
			}
		}
	}
	if ( (!segsDiff) && to1 && (dp->type==NTO_REGULAR||dp->type==NTO_CURVED||dp->type == NTO_CORNU) ) {
		if ( dp->type==NTO_REGULAR ) {
			points[2].y = - points[2].y;
			radii[0] = - radii[0];
		} else if (dp->type == NTO_CURVED) {
			points[1].y = - points[1].y;
			points[2].y = - points[2].y;
			radii[0] = - radii[0];
			radii[1] = - radii[1];
		} else {
			points[2].y = - points[2].y;
			points[1].y = - points[1].y;
			angles[1] = -angles[1];
			angles[2] = -angles[2];
			radii[0] = - radii[0];
			radii[1] = - radii[1];
		}
		LoadSegs( dp, FALSE, &pathLen );
		if ( dp->type==NTO_REGULAR ) {
			points[2].y = - points[2].y;
			radii[0] = - radii[0];
		} else if (dp->type == NTO_CURVED) {
			points[1].y = - points[1].y;
			points[2].y = - points[2].y;
			radii[0] = - radii[0];
			radii[1] = - radii[1];
		} else {
			points[2].y = - points[2].y;
			points[1].y = - points[1].y;
			angles[1] = -angles[1];
			angles[2] = -angles[2];
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
				case SEG_BEZTRK:
					break;
				default:
					segsDiff = TRUE;
				}
			}
		} else {
			for ( sp0=to1->segs; (!segsDiff) && sp0<&to1->segs[to1->segCnt]; sp0++ ) {
				if ( sp0->type != SEG_STRTRK && sp0->type != SEG_CRVTRK && sp0->type != SEG_BEZTRK)
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
				fabs(segs[i].u.c.radius),
				segs[i].u.c.center.x, segs[i].u.c.center.y,
				segs[i].u.c.a0, segs[i].u.c.a1 )>0;
			break;
		case SEG_FILCRCL:
			rc &= fprintf( f, "\t%c %ld %0.6f %0.6f %0.6f %0.6f\n",
				segs[i].type, roadbedColorRGB, segs[i].width,
				fabs(segs[i].u.c.radius),
				segs[i].u.c.center.x, segs[i].u.c.center.y )>0;
			break;
		case SEG_POLY:
		case SEG_FILPOLY:
			rc &= fprintf( f, "\t%c %ld %0.6f %d\n",
				segs[i].type, roadbedColorRGB, segs[i].width,
				segs[i].u.p.cnt )>0;
			for ( j=0; j<segs[i].u.p.cnt; j++ )
				rc &= fprintf( f, "\t\t%0.6f %0.6f\n",
						segs[i].u.p.pts[j].pt.x, segs[i].u.p.pts[j].pt.y )>0;
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
		newTurnLen0 = GetDim(atof( *argv++ ));
		curDesign = &StrSectionDesc;
		NewTurnOk( &StrSectionDesc );
		break;
	case 's':
		if (argc != 7) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		curDesign = &StrSectionDesc;
		NewTurnOk( &StrSectionDesc );
		break;
	case 'j':
		if (argc != 8) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnLen1 = GetDim(atof( *argv++ ));
		sprintf( specialLine, "\tX adjustable %0.6f %0.6f", newTurnLen0, newTurnLen1 );
		curDesign = &StrSectionDesc;
		NewTurnOk( &StrSectionDesc );
		break;
	case 'c':
		if (argc != 8) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		curDesign = &CrvSectionDesc;
		NewTurnOk( &CrvSectionDesc );
		break;
	case 'r':
		if (argc != 12) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnOff0 = GetDim(atof( *argv++ ));
		newTurnLen1 = GetDim(atof( *argv++ ));
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
		newTurnAngle0 = atof( *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnLen1 = radius * sin(D2R(newTurnAngle0));
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
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnOff0 = GetDim(atof( *argv++ ));
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
		newTurnAngle0 = atof( *argv++ );
		newTurnLen0 = radius * sin(D2R(newTurnAngle0));
		newTurnOff0 = radius * (1-cos(D2R(newTurnAngle0)));
		radius = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnLen1 = radius * sin(D2R(newTurnAngle1));
		newTurnOff1 = radius * (1-cos(D2R(newTurnAngle1)));
		curDesign = &CrvDesc;
		NewTurnOk( &CrvDesc );
		break;
	case 'y':
		if (argc != 14) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		strcpy( newTurnRightDesc, *argv++ );
		strcpy( newTurnRightPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnOff0 = GetDim(atof( *argv++ ));
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		curDesign = &WyeDesc;
		NewTurnOk( &WyeDesc );
		break;
	case '3':
		if (argc != 13) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen2 = GetDim(atof( *argv++ ));
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnOff0 = GetDim(atof( *argv++ ));
		newTurnLen1 = GetDim(atof( *argv++ ));
		newTurnAngle1 = atof( *argv++ );
		newTurnOff1 = GetDim(atof( *argv++ ));
		curDesign = &ThreewayDesc;
		NewTurnOk( &ThreewayDesc );
		break;
	case 'x':
		if (argc<9) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		curDesign = &CrossingDesc;
		NewTurnOk( &CrossingDesc );
		break;
	case '1':
		if (argc<9) Usage(argc0,argv0);
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		curDesign = &SingleSlipDesc;
		NewTurnOk( &SingleSlipDesc );
		break;
	case '2':
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		if (argc<9) Usage(argc0,argv0);
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnAngle0 = atof( *argv++ );
		newTurnLen1 = GetDim(atof( *argv++ ));
		curDesign = &DoubleSlipDesc;
		NewTurnOk( &DoubleSlipDesc );
		break;
	case 'd':
		strcpy( newTurnLeftDesc, *argv++ );
		strcpy( newTurnLeftPartno, *argv++ );
		if (argc<8) Usage(argc0,argv0);
		newTurnLen0 = GetDim(atof( *argv++ ));
		newTurnOff0 = GetDim(atof( *argv++ ));
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
			fprintf( stdout, "\tS 0 0 %0.6f %0.6f %0.6f %0.6f\n", x1, y1, -x1, -y1 );
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
