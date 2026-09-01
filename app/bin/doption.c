/** \file doption.c
 * Option dialogs
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
#include "custom.h"
#include "form.h"
#include "track.h"
#include "ctrain.h"
#include "i18n.h"

static paramIntegerRange_t i0_256 = { 0, 256 };
static paramIntegerRange_t i1_256 = { 1, 256 };
static paramIntegerRange_t i1_1000 = { 1, 1000 };
static paramIntegerRange_t i10_1000 = { 10, 1000 };
static paramFloatRange_t r1_1000 = { 1, 1000 };

long GetChanges( paramGroup_p pg )
{
	long changes = 0;
	long changedMask = FormUpdate(pg);

	for ( int inx=0; changedMask; inx++ ) {
		if ( changedMask&1 ) {
			changes |= VP2L(pg->paramPtr[inx].context);
		}
		changedMask >>= 1;
	}
	return changes;
}

static void OptionDlgUpdate(
        paramGroup_p pg,
        int inx,
        void* valueP)
{
	if (inx < 0) { return; }


}

/****************************************************************************
 *
 * Display Dialog
 *
 */

static wControl_p displayW;


#ifdef TODO_UNUSED
static char * hotBarLabelsLabels[] = { N_("Part No"), N_("Descr"), NULL };
static char * listLabelsLabels[] = { N_("Manuf"), N_("Part No"), N_("Descr"), NULL };
#endif


static paramData_t displayPLs[] = {
	{ PD_RADIO, &colorTrack, "color-track", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &colorDraw, "color-draw", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &drawTunnel, "tunnels", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &drawEndPtV, "endpt", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &drawUnconnectedEndPt, "unconnected-endpt", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &tieDrawMode, "tiedraw", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_RADIO, &centerDrawMode, "centerdraw", PDO_NOPSHUPD|PDO_DRAW, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN | CHANGE_MAP) },
	{ PD_LONG, &twoRailScale, "tworailscale", PDO_NOPSHUPD, &i1_256, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_FLOAT, &mapD.scale, "mapscale", PDO_NOPSHUPD, &r1_1000, NULL, 0, I2VP(CHANGE_MAP) },
	{ PD_TOGGLE, &dontHideCursor, "donthidecursor", PDO_NOPSHUPD, NULL, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &constrainMain, "constrainmain", PDO_NOPSHUPD, NULL, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &liveMap, "livemap", PDO_NOPSHUPD, NULL, "", BC_HORIZONTAL },
	{ PD_TOGGLE, &autoPan, "autoPan", PDO_NOPSHUPD, NULL, "", BC_HORIZONTAL },
#define labelSelect (13)
	{ PD_TOGGLE, &labelEnable, "labelenable", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_LONG, &labelScale, "labelscale", PDO_NOPSHUPD, &i0_256, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_LONG, &descriptionFontSize, "description-fontsize", PDO_NOPSHUPD, &i1_1000, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_TOGGLE, &hotBarLabels, "hotbarlabels", PDO_NOPSHUPD, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_TOOLBAR) },
	{ PD_TOGGLE, &layoutLabels, "layoutlabels", PDO_NOPSHUPD, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_MAIN) },
	{ PD_TOGGLE, &listLabels, "listlabels", PDO_NOPSHUPD, NULL, NULL, BC_HORIZONTAL, I2VP(CHANGE_PARAMS) },
	/* ATTENTION: update the define below if you add entries above */
#define I_HOTBARLABELS	(19)
	{ PD_COMBOLIST, &carHotbarModeInx, "carhotbarlabels", PDO_NOPSHUPD|PDO_DLGUNDERCMDBUTT|PDO_LISTINDEX, I2VP(250), N_("Car Labels"), 0, I2VP(CHANGE_SCALE) },
	{ PD_LONG, &trainPause, "trainpause", PDO_NOPSHUPD, &i10_1000, NULL, 0, 0 },
	{ PD_TOGGLE, &hideTrainsInTunnels, "hideTrainsInTunnels", PDO_NOPSHUPD, NULL, NULL, BC_HORIZONTAL }
};
static paramGroup_t displayPG = { "display", PGO_FULLDIALOGFROMBUILDER |PGO_RECORD, displayPLs, COUNT( displayPLs ) };


static void DisplayOk( void * junk )
{
	long changes;
	changes = GetChanges( &displayPG );
	wHide( displayW );
	DoChangeNotification(changes);
}



static void DoDisplay( void * junk )
{
	if (displayW == NULL) {
		displayW = FormCreateDialog( &displayPG, MakeWindowTitle(_("Display Options")),
		                             _("Ok"), DisplayOk,
		                             _("Cancel"), FormCancel_Restore,
		                             TRUE, 0, OptionDlgUpdate);
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control, _("Proto"),
		                  I2VP(0x0002) );
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control, _("Proto/Manuf"),
		                  I2VP(0x0012) );
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control,
		                  _("Proto/Manuf/Part Number"), I2VP(0x0312) );
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control,
		                  _("Proto/Manuf/Partno/Item"), I2VP(0x4312) );
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control, _("Manuf/Proto"),
		                  I2VP(0x0021) );
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control,
		                  _("Manuf/Proto/Part Number"), I2VP(0x0321) );
		wComboBoxAddValue(displayPLs[I_HOTBARLABELS].control,
		                  _("Manuf/Proto/Partno/Item"), I2VP(0x4321) );
	}

	FormLoadControls( &displayPG );
	wShow( displayW );
}


EXPORT addButtonCallBack_t DisplayInit( void )
{
	FormRegister( &displayPG );
#ifdef LATER
	RegisterChangeNotification( DisplayChange );
#endif
	return &DoDisplay;
}


/*****************************************************************************
 *
 *  Color
 *
 */

static wControl_p colorW;

static paramData_t colorPLs[] = {
	{ PD_COLORLIST, &snapGridColor, "snapgrid", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_GRID) },
	{ PD_COLORLIST, &markerColor, "marker", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_GRID) },
	{ PD_COLORLIST, &borderColor, "border", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &crossMajorColor, "crossmajor", PDO_NOPSHUPD, NULL, NULL, 0, 0 },
	{ PD_COLORLIST, &crossMinorColor, "crossminor", PDO_NOPSHUPD, NULL, NULL, 0, 0 },
	{ PD_COLORLIST, &normalColor, "normal", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN|CHANGE_PARAMS|CHANGE_MAP) },
	{ PD_COLORLIST, &selectedColor, "selected", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &profilePathColor, "profile", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &exceptionColor, "exception", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &tieColor, "tie", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &bridgeColor, "bridge", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) },
	{ PD_COLORLIST, &roadbedColor, "roadbed", PDO_NOPSHUPD, NULL, NULL, 0, I2VP(CHANGE_MAIN) }
};
static paramGroup_t colorPG = { "rgbcolor", PGO_RECORD|PGO_FULLDIALOGFROMBUILDER, colorPLs, COUNT( colorPLs ) };



static void ColorOk( void * junk )
{
	long changes;
	changes = GetChanges( &colorPG );
	wHide( colorW );
	if ( (changes&CHANGE_GRID) && GridIsVisible() ) {
		changes |= CHANGE_MAIN;
	}
	DoChangeNotification( changes );
}


static void DoColor( void * junk )
{
	if (colorW == NULL) {
		colorW = FormCreateDialog( &colorPG, MakeWindowTitle(_("Color")),
		                           _("Ok"), ColorOk,
		                           _("Cancel"), FormCancel_Restore,
		                           TRUE, 0, NULL);
	}
	FormLoadControls( &colorPG );
	wShow( colorW );
}


EXPORT addButtonCallBack_t ColorInit( void )
{
	FormRegister( &colorPG );
	return &DoColor;
}

