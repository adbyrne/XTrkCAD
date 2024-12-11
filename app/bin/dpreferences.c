/**
 * \file   dpreferences.c
 * \brief  Preferences dialog
 */

 /*  XTrackCad - Model Railroad CAD
  *  Copyright (C) 2005, 2024 Dave Bullis
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

#include <wlib.h>

#include "custom.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "track.h"

static wControl_p prefW;
static long displayUnits;
static wIndex_t distanceFormatInx;

static paramIntegerRange_t i1_100 = { 1, 100 };
static paramIntegerRange_t i0_99 = { 0, 99 };
static paramIntegerRange_t i0_10000 = { 0, 10000 };
static paramIntegerRange_t i1_1000 = { 1, 1000 };
static paramIntegerRange_t i10_100 = { 10, 100 };
static paramFloatRange_t r0o1_1 = { 0.1, 1.0 };
static paramFloatRange_t r0_180 = { 0.0, 180.0 };
static paramFloatRange_t r1_10 = { 1.0, 10.0 };

static void UpdatePrefD(void);
static void UpdateChkPtInterval(long);
static void UpdateAutoSaveInterval(long);

static char* iconSizeLabels[] = { N_("16 px"), N_("24 px"), N_("32 px"), NULL };
static char* unitsLabels[] = { N_("English"), N_("Metric"), NULL };
static char* angleSystemLabels[] = { N_("Polar"), N_("Cartesian"), NULL };
static char* enableBalloonHelpLabels[] = { N_("Balloon Help"), NULL };
static char* enableFlexTrackLabels[] = { N_("Show FlexTrack in HotBar"), NULL };
static char* enableAudioLabels[] = { N_("Enable audio signals"), NULL };
static char* startOptions[] = { N_("Load Last Layout"), N_("Start New Layout"), NULL };

static paramData_t prefPLs[] = {
	{ PD_RADIO, &iconSize, "iconsize", PDO_NOPSHUPD, iconSizeLabels, N_("Icon Size"), BC_HORIZONTAL, I2VP(CHANGE_ICONSIZE) },
	{ PD_RADIO, &angleSystem, "anglesystem", PDO_NOPSHUPD, angleSystemLabels, N_("Angles"), BC_HORIZONTAL },
#define I_UNITS			(2)
	{ PD_RADIO, &units, "units", PDO_NOPSHUPD | PDO_NOUPDACT, unitsLabels, N_("Units"), BC_HORIZONTAL, I2VP(CHANGE_MAIN | CHANGE_UNITS) },
#define I_DSTFMT		(3)
	{ PD_COMBOLIST, &distanceFormatInx, "dstfmt", PDO_DIM | PDO_NOPSHUPD | PDO_LISTINDEX, I2VP(150), N_("Length Format"), 0, I2VP(CHANGE_MAIN | CHANGE_UNITS) },
	{ PD_FLOAT, &minLength, "minlength", PDO_DIM | PDO_SMALLDIM | PDO_NOPSHUPD, &r0o1_1, N_("Min Track Length") },
	{ PD_FLOAT, &connectDistance, "connectdistance", PDO_DIM | PDO_SMALLDIM | PDO_NOPSHUPD, &r0o1_1, N_("Connection Distance"), },
	{ PD_FLOAT, &connectAngle, "connectangle", PDO_NOPSHUPD, &r1_10, N_("Connection Angle") },
	{ PD_FLOAT, &turntableAngle, "turntable-angle", PDO_NOPSHUPD, &r0_180, N_("Turntable Angle") },
	{ PD_LONG, &maxCouplingSpeed, "coupling-speed-max", PDO_NOPSHUPD, &i10_100, N_("Max Coupling Speed"), 0 },
	{ PD_TOGGLE, &enableBalloonHelp, "balloonhelp", PDO_NOPSHUPD, enableBalloonHelpLabels, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &enableAudio, "setaudio", PDO_NOPSHUPD, enableAudioLabels, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &showFlexTrack, "showflextrack", PDO_NOPSHUPD, enableFlexTrackLabels, "", BC_HORIZONTAL},
	{ PD_LONG, &dragPixels, "dragpixels", PDO_NOPSHUPD | PDO_DRAW, &i1_1000, N_("Drag Distance") },
	{ PD_LONG, &dragTimeout, "dragtimeout", PDO_NOPSHUPD | PDO_DRAW, &i1_1000, N_("Drag Timeout") },
	{ PD_LONG, &minGridSpacing, "mingridspacing", PDO_NOPSHUPD | PDO_DRAW, &i1_100, N_("Min Grid Spacing"), 0, 0 },
#define I_CHKPT		(15)
	{ PD_LONG, &checkPtInterval, "checkpoint", PDO_NOPSHUPD | PDO_FILE, &i0_10000, N_("Check Point Frequency") },
#define I_AUTOSAVE		(16)
	{ PD_LONG, &autosaveChkPoints, "autosave", PDO_NOPSHUPD | PDO_FILE, &i0_99, N_("Autosave Checkpoint Frequency") },
	{ PD_RADIO, &onStartup, "onstartup", PDO_NOPSHUPD, startOptions, N_("On Program Startup"), 0, NULL }
};
static paramGroup_t prefPG = { "pref", PGO_RECORD | PGO_PREFMISC, prefPLs, COUNT(prefPLs) };


typedef struct {
	char* name;
	long fmt;
} dstFmts_t;
static dstFmts_t englishDstFmts[] = {
	{ N_("999.999"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 3 },
	{ N_("999.999999"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 6 },
	{ N_("999.99999"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 5 },
	{ N_("999.9999"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 4 },
	{ N_("999.999"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 3 },
	{ N_("999.99"),				DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 2 },
	{ N_("999.9"),				DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 1 },
	{ N_("999 7/8"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_FRC | 3 },
	{ N_("999 63/64"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_FRC | 6 },
	{ N_("999' 11.999\""),		DISTFMT_FMT_SHRT | DISTFMT_FRACT_NUM | 3 },
	{ N_("999' 11.99\""),		DISTFMT_FMT_SHRT | DISTFMT_FRACT_NUM | 2 },
	{ N_("999' 11.9\""),		DISTFMT_FMT_SHRT | DISTFMT_FRACT_NUM | 1 },
	{ N_("999' 11 7/8\""),		DISTFMT_FMT_SHRT | DISTFMT_FRACT_FRC | 3 },
	{ N_("999' 11 63/64\""),	DISTFMT_FMT_SHRT | DISTFMT_FRACT_FRC | 6 },
	{ N_("999ft 11.999in"),		DISTFMT_FMT_LONG | DISTFMT_FRACT_NUM | 3 },
	{ N_("999ft 11.99in"),		DISTFMT_FMT_LONG | DISTFMT_FRACT_NUM | 2 },
	{ N_("999ft 11.9in"),		DISTFMT_FMT_LONG | DISTFMT_FRACT_NUM | 1 },
	{ N_("999ft 11 7/8in"),		DISTFMT_FMT_LONG | DISTFMT_FRACT_FRC | 3 },
	{ N_("999ft 11 63/64in"),	DISTFMT_FMT_LONG | DISTFMT_FRACT_FRC | 6 },
	{ NULL, 0 }
};
static dstFmts_t metricDstFmts[] = {
	{ N_("999.999"),			DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 3 },
	{ N_("999.99"),				DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 2 },
	{ N_("999.9"),				DISTFMT_FMT_NONE | DISTFMT_FRACT_NUM | 1 },
	{ N_("999.999mm"),			DISTFMT_FMT_MM | DISTFMT_FRACT_NUM | 3 },
	{ N_("999.99mm"),			DISTFMT_FMT_MM | DISTFMT_FRACT_NUM | 2 },
	{ N_("999.9mm"),			DISTFMT_FMT_MM | DISTFMT_FRACT_NUM | 1 },
	{ N_("999.999cm"),			DISTFMT_FMT_CM | DISTFMT_FRACT_NUM | 3 },
	{ N_("999.99cm"),			DISTFMT_FMT_CM | DISTFMT_FRACT_NUM | 2 },
	{ N_("999.9cm"),			DISTFMT_FMT_CM | DISTFMT_FRACT_NUM | 1 },
	{ N_("999.999m"),			DISTFMT_FMT_M | DISTFMT_FRACT_NUM | 3 },
	{ N_("999.99m"),			DISTFMT_FMT_M | DISTFMT_FRACT_NUM | 2 },
	{ N_("999.9m"),				DISTFMT_FMT_M | DISTFMT_FRACT_NUM | 1 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 },
	{ NULL, 0 }
};
static dstFmts_t* dstFmts[] = { englishDstFmts, metricDstFmts };

void UpdateAutoSaveInterval(long value)
{
	autosaveChkPoints = value;
	ParamLoadControl(&prefPG, I_AUTOSAVE);
	ParamLoadControl(&prefPG, I_CHKPT);
}

void UpdateChkPtInterval(long value)
{
	checkPtInterval = value;
	ParamLoadControl(&prefPG, I_AUTOSAVE);
	ParamLoadControl(&prefPG, I_CHKPT);
}

/**
 * Load the selection list for number formats with the appropriate list of variants.
 */

static void LoadDstFmtList(void)
{
	int inx;
	wListClear(prefPLs[I_DSTFMT].control);
	for (inx = 0; dstFmts[units][inx].name; inx++) {
		wListAddValue(prefPLs[I_DSTFMT].control, _(dstFmts[units][inx].name),
			NULL, I2VP(dstFmts[units][inx].fmt));
	}
}

/**
* Handle changing of measurement system. The list of number formats is loaded
* and the first entry is selected as default value.
*/

static void UpdatePrefD(void)
{
	long newUnits, oldUnits;
	int inx;

	if (prefW == NULL || (!wWinIsVisible(prefW))
		|| prefPLs[I_UNITS].control == NULL) {
		return;
	}
	newUnits = wRadioGetValue(prefPLs[I_UNITS].control);
	if (newUnits != displayUnits) {
		oldUnits = units;
		units = newUnits;
		LoadDstFmtList();
		distanceFormatInx = 0;

		for (inx = 0; inx < COUNT(prefPLs); inx++) {
			if ((prefPLs[inx].option & PDO_DIM)) {
				ParamLoadControl(&prefPG, inx);
			}
		}

		units = oldUnits;
		displayUnits = newUnits;
	}
	return;
}

/**
 * Handle changes of the measurement format.
 */

static void UpdateMeasureFmt()
{
	int inx;

	distanceFormatInx = wListGetIndex(prefPLs[I_DSTFMT].control);
	units = wRadioGetValue(prefPLs[I_UNITS].control);

	for (inx = 0; inx < COUNT(prefPLs); inx++) {
		if ((prefPLs[inx].option & PDO_DIM)) {
			ParamLoadControl(&prefPG, inx);
		}
	}
}

static void OptionDlgUpdate(
	paramGroup_p pg,
	int inx,
	void* valueP)
{
	if (inx < 0) { return; }
	if (pg->paramPtr[inx].valueP == &enableBalloonHelp) {
		wEnableBalloonHelp((wBool_t) * (long*)valueP);
	}
	else {
		//if (pg->paramPtr[inx].valueP == &labelEnable) {
		//	long new_labels = wRadioGetValue((wChoice_p)pg->paramPtr[inx].control);
		//	labelEnable = new_labels;
		//	//ParamLoadControl(&displayPG, labelSelect);
		//}
		if (pg->paramPtr[inx].valueP == &units) {
			UpdatePrefD();
		}
		if (pg->paramPtr[inx].valueP == &distanceFormatInx) {
			UpdateMeasureFmt();
		}
		if (pg->paramPtr[inx].valueP == &showFlexTrack) {
			DoChangeNotification(CHANGE_PARAMS | CHANGE_TOOLBAR);
		}
		if (pg->paramPtr[inx].valueP == &checkPtInterval) {
			checkPtInterval = *(long*)valueP;
			if (checkPtInterval == 0) {
				wWinPix_t h = wControlGetHeight(pg->paramPtr[inx].control);
				wControlSetBalloon(pg->paramPtr[inx].control, 0, h * 3 / 4,
					_("Turning off AutoSave"));
				UpdateAutoSaveInterval(0);
			}
			else {
				wControlSetBalloon(pg->paramPtr[inx].control, 0, 0, NULL);
			}
		}
		if (pg->paramPtr[inx].valueP == &autosaveChkPoints) {
			autosaveChkPoints = *(long*)valueP;
			if (checkPtInterval == 0 && autosaveChkPoints > 0) {
				wWinPix_t h = wControlGetHeight(pg->paramPtr[inx].control);
				wControlSetBalloon(pg->paramPtr[inx].control, 0, -h * 3 / 4,
					_("Turning on CheckPointing"));
				UpdateChkPtInterval(10);
			}
			else {
				wControlSetBalloon(pg->paramPtr[inx].control, 0, 0, NULL);
			}

		}

	}
}


static void PrefOk(void* junk)
{
	wBool_t resetValuesLow = FALSE, resetValuesHigh = FALSE;
	long changes;
	changes = GetChanges(&prefPG);
	if (connectAngle < 1.0) {
		connectAngle = 1.0;
		resetValuesLow = TRUE;
	}
	else if (connectAngle > 10.0) {
		connectAngle = 10.0;
		resetValuesHigh = TRUE;
	}
	if (connectDistance < 0.1) {
		connectDistance = 0.1;
		resetValuesLow = TRUE;
	}
	else if (connectDistance > 1.0) {
		connectDistance = 1.0;
		resetValuesHigh = TRUE;
	}
	if (minLength < 0.1) {
		minLength = 0.1;
		resetValuesLow = TRUE;
	}
	else if (minLength > 1.0) {
		minLength = 1.0;
		resetValuesHigh = TRUE;
	}
	if (resetValuesLow) {
		NoticeMessage2(0, MSG_CONN_PARAMS_TOO_SMALL, _("Ok"), NULL);
	}
	if (resetValuesHigh) {
		NoticeMessage2(0, MSG_CONN_PARAMS_TOO_BIG, _("Ok"), NULL);
	}

	if (changes & CHANGE_ICONSIZE) {
		NoticeMessage(MSG_ICON_SIZE_RESTART, _("Ok"), NULL);
	}

	wPrefSetInteger("misc", "audio", enableAudio);
	wSetAudio(enableAudio);

	wHide(prefW);
	DoChangeNotification(changes);
}



static void DoPref(void* junk)
{
	if (prefW == NULL) {
		prefW = ParamCreateDialog(&prefPG, MakeWindowTitle(_("Preferences")), _("Ok"),
			PrefOk, ParamCancel_Restore, TRUE, NULL, 0, OptionDlgUpdate);
		LoadDstFmtList();
	}
	ParamLoadControls(&prefPG);
	displayUnits = units;
	wShow(prefW);
}


EXPORT addButtonCallBack_t PrefInit(void)
{
	ParamRegister(&prefPG);
	if (connectAngle < 1.0) {
		connectAngle = 1.0;
	}
	if (connectDistance < 0.1) {
		connectDistance = 0.1;
	}
	if (minLength < 0.1) {
		minLength = 0.1;
	}
	return &DoPref;
}


EXPORT long GetDistanceFormat(void)
{
	while (dstFmts[units][distanceFormatInx].name == NULL) {
		distanceFormatInx--;
	}
	return dstFmts[units][distanceFormatInx].fmt;
}

