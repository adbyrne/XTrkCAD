/** \file dease.c
 * Easement Button Hdlrs
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

#include "common.h"
#include "ccurve.h"
#include "cjoin.h"
#include "cstraigh.h"
#include "custom.h"
#include "fileio.h"
#include "param.h"
#include "track.h"

EXPORT DIST_T easementVal = 0.0;
EXPORT DIST_T easeR = 0.0;
EXPORT DIST_T easeL = 0.0;

static wControl_p easementB;

static DIST_T easeX = 0.0;

static DIST_T Rvalues[3];
static DIST_T Lvalues[3];

static DIST_T oldEasementVal;

static wIcon_p enone_bm;
static wIcon_p esharp_bm;
static wIcon_p egtsharp_bm;
static wIcon_p eltsharp_bm;
static wIcon_p enormal_bm;
static wIcon_p eltbroad_bm;
static wIcon_p ebroad_bm;
static wIcon_p egtbroad_bm;
static wIcon_p ecornu_bm;

/****************************************
 *
 * EASEMENTW
 *
 */

static wWin_p easementW;

static void EasementSel( long );
static void SetEasement( DIST_T, void * );
static void EasementOk( void );
static void EasementCancel( void );

static char *easementChoiceLabels[] = { N_("None"), N_("Sharp"), N_("Normal"), N_("Broad"), N_("Cornu"), NULL };
static paramFloatRange_t r0n1_100 = { -1.0, 100.0, 60 };
static paramFloatRange_t r0_100 = { 0.0, 100.0, 60 };
static paramFloatRange_t r0_200 = { 0.0, 200.0, 60 };
static paramFloatRange_t r0_10 = { 0.0, 10.0, 60 };
static long easeM;
static paramData_t easementPLs[] = {
#define I_EASEVAL		(0)
	{	PD_FLOAT, &easementVal, "val", PDO_NOPSHUPD, &r0n1_100, N_("Value") },
	{	PD_FLOAT, &easeR, "r", PDO_DIM|PDO_DLGRESETMARGIN, &r0_200, N_("R"), BO_READONLY },
	{	PD_FLOAT, &easeX, "x", PDO_DIM|PDO_DLGHORZ, &r0_10, N_("X"), BO_READONLY },
	{	PD_FLOAT, &easeL, "l", PDO_DIM|PDO_DLGHORZ, &r0_100, N_("L"), BO_READONLY },
#define I_EASESEL		(4)
	{	PD_RADIO, &easeM, "radio", PDO_DIM|PDO_NORECORD|PDO_NOPREF|PDO_DLGRESETMARGIN, easementChoiceLabels, NULL, BC_HORIZONTAL }
};
static paramGroup_t easementPG = { "easement", PGO_RECORD, easementPLs, COUNT( easementPLs ) };


static void SetEasement(
        DIST_T val,
        void * update )
/*
 * Set transition-curve parameters (R and L).
 */
{
	DIST_T z;
	long selVal = -1;
	wIcon_p bm;

	if (val < 0.0) {
		easeX = easeR = easeL = 0.0;
		selVal = 4;
		val = -1;
		bm = ecornu_bm;
	} else {
		if (val == 0.0) {
			easeX = easeR = easeL = 0.0;
			selVal = 0;
			val = 0;
			bm = enone_bm;
		} else if (val <= 1.0) {
			if (val < 0.21) { val = 0.21; }   //Eliminate values that give negative radii
			z = 1.0/val - 1.0;
			easeR = Rvalues[1] - z * (Rvalues[1] - Rvalues[0]);
			easeL = Lvalues[1] - z * (Lvalues[1] - Lvalues[0]);
			if (easeR != 0.0) {
				easeX = easeL*easeL/(24*easeR);
			} else {
				easeX = 0.0;
			}
			if (val == 1.0) {
				selVal = 2;
				bm = enormal_bm;
			} else if (val == 0.5) {
				selVal = 1;
				bm = esharp_bm;
			} else if (val < 0.5) {
				bm = eltsharp_bm;
				selVal = 1;
			} else {
				selVal = 1;
				bm = egtsharp_bm;
			}
		} else {
			z = val - 1.0;
			easeR = Rvalues[1] + z * (Rvalues[2] - Rvalues[1]);
			easeL = Lvalues[1] + z * (Lvalues[2] - Lvalues[1]);
			if (easeR != 0.0) {
				easeX = easeL*easeL/(24*easeR);
			} else {
				easeX = 0.0;
			}
			if (val == 2.0) {
				selVal = 3;
				bm = ebroad_bm;
			} else if (val < 2.0) {
				selVal = 3;
				bm = eltbroad_bm;
			} else {
				selVal = 3;
				bm = egtbroad_bm;
			}
		}
	}

	easeR = (floor(easeR*100.0))/100.0;
	easementVal = val;
	if (easementW && wWinIsVisible(easementW)) {
		ParamLoadControls( &easementPG );
		if (update) {
			easeM = selVal;
			ParamLoadControl( &easementPG, I_EASESEL );
		}
	}
	/*ParamChange( &easeValPD );*/

	if (easementB) {
		wButtonSetIcon( easementB, (char*)bm );
	}
}


static void EasementOk( void )
{
	ParamLoadData( &easementPG );
	SetEasement( easementVal, I2VP(FALSE) );
	wHide( easementW );
}


static void EasementCancel( void )
{
	SetEasement( easementVal = oldEasementVal, I2VP(FALSE) );
	wHide( easementW );
}


static void EasementSel(
        long arg )
/*
 * Handle transition-curve parameter selection.
 */
{
	DIST_T val;
	switch (arg) {
	case 0:
		val = 0;
		break;
	case 1:
		val = 0.5;
		break;
	case 2:
		val = 1.0;
		break;
	case 3:
		val = 2.0;
		break;
	case 4:
		val = -1.0;
		break;
	default:
		CHECKMSG( FALSE, ( "easementSel: bad value %ld", arg) );
		val = 0.0;
		break;
	}
	SetEasement( val, I2VP(FALSE) );
}


static void EasementDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	switch (inx) {
	case I_EASEVAL:
		SetEasement( *(FLOAT_T*)valueP, I2VP(1) );
		break;
	case I_EASESEL:
		EasementSel( *(long*)valueP );
		break;
	}
}


static void LayoutEasementW(
        paramData_t * pd,
        int inx,
        wWinPix_t colX,
        wWinPix_t * x,
        wWinPix_t * y )
{
	if ( inx == 2 ) {
		wControlSetPos( easementPLs[0].control, *x,
		                wControlGetPosY(easementPLs[0].control) );
	}
}


static void DoEasement( void * unused )
{
	if (easementW == NULL) {
		easementW = ParamCreateDialog( &easementPG, MakeWindowTitle(_("Easement")),
		                               _("Ok"), (paramActionOkProc)EasementOk, (paramActionCancelProc)EasementCancel,
		                               TRUE, LayoutEasementW, 0, EasementDlgUpdate );
		SetEasement( easementVal, I2VP(TRUE) );
	}
	oldEasementVal = easementVal;
	wShow( easementW );
	SetEasement( easementVal, I2VP(TRUE) );
}


static void EasementChange( long changes )
/*
 * Handle change of scale. Load new parameters.
 */
{
	if (changes&(CHANGE_SCALE|CHANGE_UNITS)) {
		GetScaleEasementValues( Rvalues, Lvalues );
		SetEasement( easementVal, I2VP(TRUE) );
	}
}


#include "bitmaps/ease-none.xpm3"
#include "bitmaps/ease-sharp.xpm3"
#include "bitmaps/ease-gt-sharp.xpm3"
#include "bitmaps/ease-lt-sharp.xpm3"
#include "bitmaps/ease-normal.xpm3"
#include "bitmaps/ease-broad.xpm3"
#include "bitmaps/ease-gt-broad.xpm3"
#include "bitmaps/ease-lt-broad.xpm3"
#include "bitmaps/ease-cornu.xpm3"


EXPORT addButtonCallBack_t EasementInit( void )
{
	ParamRegister( &easementPG );

	enone_bm = wIconCreatePixMap( ease_none_xpm3[iconSize] );
	eltsharp_bm = wIconCreatePixMap( ease_lt_sharp_xpm3[iconSize] );
	esharp_bm = wIconCreatePixMap( ease_sharp_xpm3[iconSize] );
	egtsharp_bm = wIconCreatePixMap( ease_gt_sharp_xpm3[iconSize] );
	enormal_bm = wIconCreatePixMap( ease_normal_xpm3[iconSize] );
	eltbroad_bm = wIconCreatePixMap( ease_lt_broad_xpm3[iconSize] );
	ebroad_bm = wIconCreatePixMap( ease_broad_xpm3[iconSize] );
	egtbroad_bm = wIconCreatePixMap( ease_gt_broad_xpm3[iconSize] );
	ecornu_bm = wIconCreatePixMap( ease_cornu_xpm3[iconSize] );
	easementB = AddToolbarButton( "cmdEasement", enone_bm, 0, DoEasementRedir,
	                              NULL );

	RegisterChangeNotification( EasementChange );
	return &DoEasement;
}

