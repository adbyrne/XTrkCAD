/** \file careditdlg.c
 * Car Edit Dialog
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

#include <errno.h>

#include "cselect.h"
#include "ctrain.h"
#include "custom.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "paramfile.h"
#include "common-ui.h"
#include "form.h"

#include "include/cars.h"
#include "listelem.h"
#include "misc.h"
#include "tabstring.h"
#include "carsprivate.h"
#include "wlib.h"

static int log_carDlgState=10;
static int log_carDlgEdit=10;

trkSeg_p carProtoSegPtr;
int carProtoSegCnt;

/**
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Resolution model (Phase B, rolling-stock dialog rework). The old
 * transition-table interpreter (CarDlgDoActions/newStateTable) is gone --
 * each UI event now calls its handful of helper functions directly. The
 * state names/levels (carDlgState_e), the 2-slot create/return stack
 * (carDlgStk), and CarDlgOk's per-level commit branches are unchanged from
 * before; only the *dispatch* mechanism was replaced.
 *
 * resCatalog/resProto make explicit whether the typed manufacturer+part /
 * prototype resolves to an existing catalog entry (RS_RESOLVED), not match
 * anything yet (RS_NEW -- fields stay open for inline creation), or not have
 * enough typed to tell (RS_UNRESOLVED). They are computed from the same
 * CarPartFind/CarProtoFind/"found" results the old code already used.
 * carDlgState_e itself now only tracks *level* (CarDlgStateLevel/CUR_LEVEL);
 * the S_ItemEnter/S_PartnoEnter pseudo-states this comment used to describe
 * were retired once every reader migrated to resCatalog/resModelManuf/
 * resItemManuf (see their declarations below) -- currState is never set to
 * anything but S_Error/S_ItemSel/S_PartnoSel/S_ProtoSel now.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static paramFloatRange_t r0d001_99999 = { 0.001, 99999, 80 };
static paramFloatRange_t r9999_9999 = {-99999, 99999, 80};
static paramIntegerRange_t i1_999999999 = { 1, 999999999, 80, PDO_NORANGECHECK_HIGH };
static paramIntegerRange_t i1_9999 = { 1, 9999, 50 };
static char * cplrModeLabels[] = { N_("Truck"), N_("Body"), 0 };

extern dynArr_t carItemInfo_da;

/*
 * Car Item/Part Dlg
 */

int carDlgChanged;

SCALEINX_T carDlgScaleInx;
carItem_p carDlgUpdateItemPtr;
carPart_p carDlgUpdatePartPtr;
carProto_p carDlgUpdateProtoPtr;
static carPart_p carDlgNewPartPtr;
static carProto_p carDlgNewProtoPtr;

static resState_e resCatalog = RS_UNRESOLVED;   /* car flow only */
static resState_e resProto   =
        RS_UNRESOLVED;   /* car flow + direct part-edit flow */
/* LVL_MODEL only: does the typed Manufacturer field currently match a real
 * manufacturer? Sticky once RS_RESOLVED (an already-matched model edit
 * doesn't flip back just because the field later stops matching) -- distinct
 * from resCatalog, which at LVL_MODEL already carries a different signal
 * (whether this whole catalog model is itself new, driving the "partno-new"
 * hint label). Reset to RS_RESOLVED at every LVL_MODEL entry point (the same
 * places that set currState = S_PartnoSel), matching the old S_PartnoSel
 * starting state this replaces. */
static resState_e resModelManuf = RS_RESOLVED;

/* LVL_CAR only: was the currently-loaded car's manuf+part unresolved at the
 * moment it was loaded/created (RS_NEW), as opposed to a mismatch the user
 * is transiently typing right now? Deliberately NOT the same signal as
 * resCatalog: the I_CD_MANUF_LIST "changed" handler's own !matched branch
 * sets resCatalog to RS_UNRESOLVED on every mismatched keystroke, which
 * would make a plain resCatalog==RS_NEW check flicker away as soon as the
 * user typed a second unmatching character. resItemManuf instead mirrors
 * the old currState (S_ItemSel/S_ItemEnter) load-time value exactly: set
 * once at every LVL_CAR entry point (paired with resCatalog there) and
 * consumed+cleared to RS_RESOLVED the first time I_CD_MANUF_LIST resolves a
 * match (see enteringFromUnmatched below) -- untouched by intermediate
 * mismatch keystrokes in between, just like currState was. */
static resState_e resItemManuf = RS_RESOLVED;


/* Reentrancy guard for the resolution logic below. CarDlgLoadProtoList()/
 * CarDlgLoadManufList()/CarDlgLoadPartList() clear+repopulate combo boxes
 * (wComboBoxClear/wComboBoxSetIndex), which fires GTK "changed" signals
 * synchronously for whichever control's active row actually moves -- that
 * can re-enter CarDlgUpdate() for a *different* control while the outer
 * resolution call is still in progress, clobbering shared statics like
 * carDlgTypeInx before the outer call reads them back. The old table-driven
 * state machine had this same guard (g_carDlgSMContext.in_transition); it
 * must be preserved here for the same reason. */
static BOOL_T carDlgResolving = FALSE;

static BOOL_T carDlgFlipToggle;

static wIndex_t carDlgManufInx;
static char carDlgManufStr[STR_SIZE];
static wIndex_t carDlgKindInx;
static wIndex_t carDlgProtoInx;
static char carDlgProtoStr[STR_SIZE] =
        "Prototype";	// Make sure we have something in ProtoStr
static wIndex_t carDlgPartnoInx;
static char carDlgPartnoStr[STR_SIZE] = "0";		// and in PartnoStr
static char carDlgDescStr[STR_SIZE];

static wIndex_t carDlgRoadnameInx;
static char carDlgRoadnameStr[STR_SIZE];
static char carDlgRepmarkStr[STR_SIZE];
/* Set once the user directly edits the Reporting Mark field; gates the
 * Road->Mark seed in I_CD_ROADNAME_LIST below so it never overwrites a
 * user-entered Mark. Reset per dialog session in DoCarPartDlg(). */
static BOOL_T carDlgRepmarkTouched;
static char carDlgNumberStr[STR_SIZE];
static wDrawColor carDlgBodyColor;
static wIndex_t carDlgTypeInx;

static carDim_t carDlgDim;
static DIST_T carDlgCouplerLength;
static long carDlgCouplerMount;

long carDlgItemIndex = 1;
static FLOAT_T carDlgPurchPrice;
static char carDlgPurchPriceStr[STR_SIZE];
static FLOAT_T carDlgCurrPrice;
static char carDlgCurrPriceStr[STR_SIZE];
static wIndex_t carDlgConditionInx;
static long carDlgCondition;
static long carDlgPurchDate;
static char carDlgPurchDateStr[STR_SIZE];
static long carDlgServiceDate;
static char carDlgServiceDateStr[STR_SIZE];
static long carDlgQuantity = 1;
static long carDlgMultiNum;

static drawCmd_t carDlgD = {
	NULL,
	&screenDrawFuncs,
	0,
	1.0,
	0.0,
	{ 0, 0 }, { 0, 0 },
	Pix2CoOrd, CoOrd2Pix
};
static void CarDlgRedraw( wDraw_p d, void * context, wWinPix_t x, wWinPix_t y );

static paramDrawData_t carDlgDrawData = { 500, 120, CarDlgRedraw, NULL, &carDlgD };
static paramTextData_t notesData = { 500, 156 };
static char *multinumLabels[] = { N_("Sequential"), N_("Repeated"), NULL };
static void CarDlgUpdate( paramGroup_p, int, void * );
static void CarDlgNewDesc( void );
static void CarDlgManufFocusOut( void );
static void CarDlgModelFocusOut( void );
static void CarDlgProtoFocusOut( void );

typedef enum {
	I_CD_MANUF_LIST,
	I_CD_MANUF_NEW,
	I_CD_PROTOKIND_LIST,
	I_CD_PROTOTYPE_LIST,
	I_CD_PROTOTYPE_NEW,
	I_CD_PARTNO_LIST,
	I_CD_PARTNO_NEW,
	I_CD_DESC_STR,
	I_CD_IMPORT,
	I_CD_RESET,
	I_CD_FLIP,
	I_CD_IDENTITY_EXP,
	I_CD_GEOMETRY_EXP,
	I_CD_OWNERSHIP_EXP,
	I_CD_SHARED_WARNING,
	I_CD_ROADNAME_LIST,
	I_CD_REPMARK,
	I_CD_NUMBER,
	I_CD_BODYCOLOR,
	I_CD_CARLENGTH,
	I_CD_CARWIDTH,
	I_CD_TRKCENTER,
	I_CD_TRKOFFSET,
	I_CD_CPLRMNT,
	I_CD_CPLDLEN,
	I_CD_CPLRLEN,
	I_CD_CANVAS,
	I_CD_ITEMINDEX,
	I_CD_PURPRC,
	I_CD_CURPRC,
	I_CD_COND,
	I_CD_PURDAT,
	I_CD_SRVDAT,
	I_CD_QTY,
	I_CD_MLTNUM,
	I_CD_NOTES,
	I_CD_MSG
} carDlgFieldIndex_e;

static paramData_t carDlgPLs[] = {
	[I_CD_MANUF_LIST] =
	{ PD_COMBOLIST, &carDlgManufInx, "manuf", PDO_NOPREF, I2VP(350), N_("Manufacturer"), BL_EDITABLE|BL_FOCUSOUT },
	[I_CD_MANUF_NEW] =
	{ PD_MESSAGE, NULL, "manuf-new", PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN, I2VP(50) },
	[I_CD_PROTOKIND_LIST] =
	{ PD_COMBOLIST, &carDlgKindInx, "protokind-list", PDO_NOPREF, I2VP(125), N_("Prototype"), 0 },
	[I_CD_PROTOTYPE_LIST] =
	{ PD_COMBOLIST, &carDlgProtoInx, "prototype", PDO_NOPREF|PDO_DLGHORZ|PDO_NOTBLANK, I2VP(225-3), NULL, BL_EDITABLE|BL_FOCUSOUT },
	[I_CD_PROTOTYPE_NEW] =
	{ PD_MESSAGE, NULL, "prototype-new", PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN, I2VP(50) },
	[I_CD_PARTNO_LIST] =
	{ PD_COMBOLIST, &carDlgPartnoInx, "partno-list", PDO_NOPREF|PDO_NOTBLANK, I2VP(350), N_("Part"), BL_EDITABLE|BL_FOCUSOUT },
	[I_CD_PARTNO_NEW] =
	{ PD_MESSAGE, NULL, "partno-new", PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN, I2VP(50) },
	[I_CD_DESC_STR] =
	{ PD_STRING, &carDlgDescStr, "desc", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(35), N_("Description"), 0, 0, sizeof(carDlgDescStr)},
	[I_CD_IMPORT] =
	{ PD_BUTTON, NULL, "import", 0, 0, N_("Import") },
	[I_CD_RESET] =
	{ PD_BUTTON, NULL, "reset", PDO_DLGHORZ, 0, N_("Reset") },
	[I_CD_FLIP] =
	{ PD_BUTTON, NULL, "flip", PDO_DLGHORZ|PDO_DLGWIDE|PDO_DLGBOXEND, 0, N_("Flip") },

	[I_CD_IDENTITY_EXP] =
	{ PD_EXPANDER, NULL, "identity-summary-revealer", 0 },
	[I_CD_GEOMETRY_EXP] =
	{ PD_EXPANDER, NULL, "geometry-revealer", 0 },
	[I_CD_OWNERSHIP_EXP] =
	{ PD_EXPANDER, NULL, "ownership-panel-revealer", 0 },

	[I_CD_SHARED_WARNING] =
	{ PD_MESSAGE, NULL, "shared-warning-label", PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN, I2VP(450) },

	[I_CD_ROADNAME_LIST] =
	{ PD_COMBOLIST, &carDlgRoadnameInx, "road", PDO_NOPREF|PDO_DLGWIDE, I2VP(350), N_("Road"), BL_EDITABLE },
	[I_CD_REPMARK] =
	{ PD_STRING, carDlgRepmarkStr, "repmark", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(6), N_("Reporting Mark"), 0, 0, sizeof(carDlgRepmarkStr)},
	[I_CD_NUMBER] =
	{ PD_STRING, carDlgNumberStr, "number", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ|PDO_STRINGLIMITLENGTH, I2VP(12), N_("Number"), 0, 0, sizeof(carDlgNumberStr)},
	[I_CD_BODYCOLOR] =
	{ PD_COLORLIST, &carDlgBodyColor, "bodyColor", PDO_DLGWIDE|PDO_DLGHORZ, NULL, N_("Color") },
	[I_CD_CARLENGTH] =
	{ PD_FLOAT, &carDlgDim.carLength, "carLength", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE, &r0d001_99999, N_("Car Length") },
	[I_CD_CARWIDTH] =
	{ PD_FLOAT, &carDlgDim.carWidth, "carWidth", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, &r0d001_99999, N_("Width") },
	[I_CD_TRKCENTER] =
	{ PD_FLOAT, &carDlgDim.truckCenter, "trkCenter", PDO_DIM|PDO_NOPREF, &r0d001_99999, N_("Truck Centers") },
	[I_CD_TRKOFFSET] =
	{ PD_FLOAT, &carDlgDim.truckCenterOffset, "trkCenterOffset", PDO_DIM|PDO_NOPREF|PDO_DLGHORZ|PDO_DLGWIDE, &r9999_9999, N_("Center Offset") },
	[I_CD_CPLRMNT] =
	{ PD_RADIO, &carDlgCouplerMount, "cplrMount", PDO_NOPREF, cplrModeLabels, N_("Coupler Mount"), BC_HORIZONTAL|BC_NOBORDER },
	[I_CD_CPLDLEN] =
	{ PD_FLOAT, &carDlgDim.coupledLength, "cpldLen", PDO_DIM|PDO_NOPREF, &r0d001_99999, N_("Coupled Length") },
	[I_CD_CPLRLEN] =
	{ PD_FLOAT, &carDlgCouplerLength, "cplrLen", PDO_DIM|PDO_NOPREF|PDO_DLGHORZ, &r0d001_99999, N_("Coupler Length") },
	[I_CD_CANVAS] =
	{ PD_DRAW, NULL, "canvas", PDO_NOPSHUPD|PDO_DLGWIDE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGBOXEND|PDO_DLGRESIZE, &carDlgDrawData, NULL, 0 },

	[I_CD_ITEMINDEX] =
	{ PD_LONG, &carDlgItemIndex, "index", PDO_NOPREF|PDO_DLGWIDE, &i1_999999999, N_("Index"), 0 },
	[I_CD_PURPRC] =
	{ PD_STRING, &carDlgPurchPriceStr, "purchPrice", PDO_NOPREF|PDO_DLGWIDE|PDO_STRINGLIMITLENGTH, I2VP(10), N_("Purchase Price"), 0, &carDlgPurchPrice, sizeof(carDlgPurchPriceStr) },
	[I_CD_CURPRC] =
	{ PD_STRING, &carDlgCurrPriceStr, "currPrice", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ|PDO_STRINGLIMITLENGTH, I2VP(10), N_("Current Price"), 0, &carDlgCurrPrice, sizeof(carDlgCurrPriceStr) },
	[I_CD_COND] =
	{ PD_COMBOLIST, &carDlgConditionInx, "condition", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, I2VP(90), N_("Condition") },
	[I_CD_PURDAT] =
	{ PD_STRING, &carDlgPurchDateStr, "purchDate",  PDO_NOPREF|PDO_DLGWIDE|PDO_STRINGLIMITLENGTH, I2VP(10), N_("Purchase Date"), 0, &carDlgPurchDate, sizeof(carDlgPurchDateStr) },
	[I_CD_SRVDAT] =
	{ PD_STRING, &carDlgServiceDateStr, "serviceDate",  PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ|PDO_STRINGLIMITLENGTH, I2VP(10), N_("Service Date"), 0, &carDlgServiceDate, sizeof(carDlgServiceDateStr) },
	[I_CD_QTY] =
	{ PD_LONG, &carDlgQuantity, "quantity", PDO_NOPREF|PDO_DLGWIDE, &i1_9999, N_("Quantity") },
	[I_CD_MLTNUM] =
	{ PD_RADIO, &carDlgMultiNum, "multinum", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, NULL, N_("Numbers"), BC_HORIZONTAL|BC_NOBORDER },
	[I_CD_NOTES] =
	{ PD_TEXT, NULL, "notes", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &notesData, N_("Notes") },

	[I_CD_MSG] =
	{ PD_MESSAGE, NULL, "message", PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGBOXEND, I2VP(450) }
};

static paramGroup_t carDlgPG = { "carpart", PGO_FULLDIALOGFROMBUILDER, carDlgPLs, COUNT( carDlgPLs ) };

static dynArr_t carDlgSegs_da;
#define carDlgSegs(N) DYNARR_N( trkSeg_t, carDlgSegs_da, N )

#define CARDLG_STK_SIZE (2)
int carDlgStkPtr = 0;
struct {
	carDim_t dim;
	DIST_T couplerLength;
	carDlgState_e state;
	int changed;
	carPart_p partP;
	wIndex_t typeInx;
} carDlgStk[CARDLG_STK_SIZE];

static carDlgState_e currState = S_Error;
#define S_ITEM (CUR_LEVEL==LVL_CAR)
#define S_PART (currState==S_PartnoSel)
#define S_PROTO (currState==S_ProtoSel)
/* True whenever the dialog is authoring a prototype's own outline: either
 * directly (LVL_PROTO) or implicitly while creating a catalog model whose
 * Prototype field hasn't resolved to an existing one (nested, LVL_MODEL) --
 * see CarDlgRedraw, the I_CD_IMPORT/I_CD_RESET/I_CD_FLIP handlers, and
 * CarDlgBuildProtoSegs, all of which need to treat these two cases alike. */
#define CARDLG_AUTHORING_PROTO_OUTLINE ( S_PROTO || ( S_PART && resProto == RS_NEW ) )

static void CarDlgLoadDimsFromPart( carPart_p partP )
{
	tabString_t tabs[7];

	if ( partP == NULL ) { return; }
	carDlgDim = partP->dim;
	carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
	sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
	         GetScaleName(carDlgScaleInx) );
	wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
	carDlgBodyColor = partP->color;
	FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
	FormLoadSingleControl( &carDlgPG, I_CD_CARWIDTH );
	FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
	FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
	wColorSelectButtonSetColor( (wButton_p)carDlgPLs[I_CD_BODYCOLOR].control,
	                            *(wDrawColor*)carDlgPLs[I_CD_BODYCOLOR].valueP );
	TabStringExtract( partP->title, 7, tabs );
}

static void CarDlgLoadDimsFromProto( carProto_p protoP )
{
	DIST_T ratio = GetScaleRatio(carDlgScaleInx);
	carDlgDim.carLength = protoP->dim.carLength/ratio;
	carDlgDim.carWidth = protoP->dim.carWidth/ratio;
	carDlgDim.truckCenter = protoP->dim.truckCenter/ratio;
	carDlgDim.truckCenterOffset = protoP->dim.truckCenterOffset/ratio;
	carDlgDim.coupledLength = carDlgDim.carLength + carDlgCouplerLength*2;
	/*carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;*/
	FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
	FormLoadSingleControl( &carDlgPG, I_CD_CARWIDTH );
	FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
	FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
}

static void CarDlgRedraw(
        wDraw_p d, void * context, wWinPix_t x, wWinPix_t y )
{
	wWinPix_t w, h;
	DIST_T ww, hh;
	DIST_T scale_w, scale_h;
	coOrd orig, pos, size;
	carProto_p protoP;
	FLOAT_T ratio;
	int segCnt;
	trkSeg_p segPtr;

	if ( S_PROTO ) {
		ratio = 1;
	} else {
		ratio = 1/GetScaleRatio(carDlgScaleInx);
	}
	wDrawClear( carDlgD.d );
	if ( carDlgDim.carLength <= 0 || carDlgDim.carWidth <= 0 ) {
		return;
	}
	FreeFilledDraw( carDlgSegs_da.cnt, &carDlgSegs(0) );
	if ( !CARDLG_AUTHORING_PROTO_OUTLINE ) {
		if ( carDlgProtoInx < 0 ||
		     (protoP = CarProtoLookup( carDlgProtoStr, FALSE, FALSE, 0.0, 0.0 )) == NULL ||
		     protoP->segCnt == 0 ) {
			CarProtoDlgCreateDummyOutline( &segCnt, &segPtr,
			                               IsLocoType(typeListMap[carDlgTypeInx].value),
			                               carDlgDim.carLength, carDlgDim.carWidth, carDlgBodyColor );
		} else {
			segCnt = protoP->segCnt;
			segPtr = protoP->segPtr;
		}
	} else {
		if ( carProtoSegCnt <= 0 ) {
			CarProtoDlgCreateDummyOutline( &segCnt, &segPtr,
			                               IsLocoType(typeListMap[carDlgTypeInx].value),
			                               carDlgDim.carLength, carDlgDim.carWidth,
			                               S_PROTO ? drawColorBlue : carDlgBodyColor );
		} else {
			segCnt = carProtoSegCnt;
			segPtr = carProtoSegPtr;
		}
	}
	DYNARR_SET( trkSeg_t, carDlgSegs_da, segCnt );
	memcpy( &carDlgSegs(0), segPtr, segCnt * sizeof carDlgSegs(0) );
	CloneFilledDraw( carDlgSegs_da.cnt, &carDlgSegs(0), TRUE );
	GetSegBounds( zero, 0.0, carDlgSegs_da.cnt, &carDlgSegs(0), &orig, &size );
	scale_w = carDlgDim.carLength/size.x;
	scale_h = carDlgDim.carWidth/size.y;
	RescaleSegs( carDlgSegs_da.cnt, &carDlgSegs(0), scale_w, scale_h, ratio );
	if ( CARDLG_AUTHORING_PROTO_OUTLINE && carDlgFlipToggle ) {
		pos.x = carDlgDim.carLength/2.0;
		pos.y = carDlgDim.carWidth/2.0;
		RotateSegs( carDlgSegs_da.cnt, &carDlgSegs(0), pos, 180.0 );
	}
	if ( !S_PROTO ) {
		RecolorSegs( carDlgSegs_da.cnt, &carDlgSegs(0), carDlgBodyColor );
	}

	wDrawGetSize( carDlgD.d, &w, &h );
	ww = w/carDlgD.dpi-1.0;
	hh = h/carDlgD.dpi-0.5;
	scale_w = carDlgDim.carLength/ww;
	scale_h = carDlgDim.carWidth/hh;
	if ( scale_w > scale_h ) {
		carDlgD.scale = scale_w;
	} else {
		carDlgD.scale = scale_h;
	}
	orig.x = 0.50*carDlgD.scale;
	orig.y = 0.25*carDlgD.scale;
	DrawSegsDA( &carDlgD, NULL, orig, 0.0, &carDlgSegs_da, 0.0, wDrawColorBlack,
	            0 );
	pos.y = orig.y+carDlgDim.carWidth/2.0;

	if ( carDlgDim.truckCenter > 0.0 ) {
		pos.x = orig.x+(carDlgDim.carLength-carDlgDim.truckCenter)/2.0
		        -carDlgDim.truckCenterOffset;
		CarProtoDrawTruck( &carDlgD, trackGauge*curScaleRatio, ratio, pos, 0.0 );
		pos.x = orig.x+(carDlgDim.carLength+carDlgDim.truckCenter)/2.0
		        -carDlgDim.truckCenterOffset;
		CarProtoDrawTruck( &carDlgD, trackGauge*curScaleRatio, ratio, pos, 0.0 );
	}
	if ( carDlgDim.coupledLength > carDlgDim.carLength ) {
		pos.x = orig.x;
		CarProtoDrawCoupler( &carDlgD,
		                     (carDlgDim.coupledLength-carDlgDim.carLength)/2.0, ratio, pos, 270.0 );
		pos.x = orig.x+carDlgDim.carLength;
		CarProtoDrawCoupler( &carDlgD,
		                     (carDlgDim.coupledLength-carDlgDim.carLength)/2.0, ratio, pos, 90.0 );
	}
}


extern dynArr_t roadnameMap_da;
static void CarDlgLoadRoadnameList( void )
/* Loads RoadnameList.
 * Set carDlgRoadnameInx to entry matching carDlgRoadnameStr (if found)
 * Otherwise not set
 */
{
	wIndex_t inx;

	if ( !roadnameMapChanged ) { return; }
	wComboBoxClear( carDlgPLs[I_CD_ROADNAME_LIST].control );
	wComboBoxAddValue( carDlgPLs[I_CD_ROADNAME_LIST].control, _("Undecorated"),
	                   NULL );
	for ( inx=0; inx<roadnameMap_da.cnt; inx++ ) {
		roadnameMap_p roadnameMapP = DYNARR_N(roadnameMap_p, roadnameMap_da, inx);
		wComboBoxAddValue( carDlgPLs[I_CD_ROADNAME_LIST].control,
		                   roadnameMapP->roadname, roadnameMapP );
		if ( strcasecmp( carDlgRoadnameStr, roadnameMapP->roadname )==0 ) {
			carDlgRoadnameInx = inx+1;
		}
	}
	roadnameMapChanged = FALSE;
}

extern dynArr_t carPartParent_da;
extern dynArr_t carProto_da;

static BOOL_T CarDlgLoadManufList(
        BOOL_T bLoadAll,
        BOOL_T bInclCustomUnknown,
        SCALEINX_T scale )
{
	carPartParent_p manufP, manufP1;
	wIndex_t inx, listInx=-1;
	BOOL_T found = TRUE;
	char * firstName = NULL;

	LOG( log_carDlgEdit, 3,
	     ( "CarDlgLoadManufList( %s, %s, %d )\n    carDlgManufStr=\"%s\"\n",
	       bLoadAll?"TRUE":"FALSE", bInclCustomUnknown?"TRUE":"FALSE", scale,
	       carDlgManufStr ) )
	carDlgManufInx = -1;
	manufP1 = NULL;
	wComboBoxClear( carDlgPLs[I_CD_MANUF_LIST].control );
	for ( inx=0; inx<carPartParent_da.cnt; inx++ ) {
		manufP = carPartParent(inx);
		if ( manufP1!=NULL && strcasecmp( manufP1->manuf, manufP->manuf ) == 0 ) {
			continue;
		}
		if ( bLoadAll==FALSE && manufP->scale != scale ) {
			continue;
		}
		if ( !CheckAvail(manufP) ) {
			continue;
		}
		listInx = wComboBoxAddValue( carDlgPLs[I_CD_MANUF_LIST].control,
		                             manufP->manuf, manufP );
		if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0'
		                             || strcasecmp( carDlgManufStr, manufP->manuf ) == 0 ) ) {
			LOG( log_carDlgEdit, 4, ( "    found manufStr (inx=%d, listInx=%d)\n", inx,
			                          listInx ) )
			carDlgManufInx = listInx;
			if ( carDlgManufStr[0] == '\0' ) { strcpy( carDlgManufStr, manufP->manuf ); }
		}
		if ( firstName == NULL ) {
			firstName = manufP->manuf;
		}
		manufP1 = manufP;
	}
	if ( bInclCustomUnknown ) {
		listInx = wComboBoxAddValue( carDlgPLs[I_CD_MANUF_LIST].control,
		                             _("Custom"), NULL );
		if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0'
		                             || strcasecmp( carDlgManufStr, "Custom" ) == 0 ) ) {
			LOG( log_carDlgEdit, 4, ( "    found Cus manufStr (inx=%d, listInx=%d)\n", inx,
			                          listInx ) )
			carDlgManufInx = listInx;
			if ( carDlgManufStr[0] == '\0' ) { strcpy( carDlgManufStr, _("Custom") ); }
		}
		if ( firstName == NULL ) {
			firstName = "Custom";
		}
		wComboBoxAddValue( carDlgPLs[I_CD_MANUF_LIST].control, _("Unknown"),
		                   NULL );
		if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0'
		                             || strcasecmp( carDlgManufStr, "Unknown" ) == 0 ) ) {
			LOG( log_carDlgEdit, 4, ( "    found Unk manufStr (inx=%d, listInx=%d)\n", inx,
			                          listInx ) )
			carDlgManufInx = listInx;
			if ( carDlgManufStr[0] == '\0' ) { strcpy( carDlgManufStr, _("Unknown") ); }
		}
	}
	if ( carDlgManufInx < 0 ) {
		found = FALSE;
		if ( firstName != NULL ) {
			LOG( log_carDlgEdit, 4, ( "    didn't find manufStr, using [0] = %s\n",
			                          firstName ) )
			carDlgManufInx = 0;
			strcpy( carDlgManufStr, firstName );
		}
	}
	return found;
}


static BOOL_T CarDlgLoadProtoList(
        char * manuf,
        SCALEINX_T scale,
        BOOL_T loadTypeList )
{
	carPartParent_p parentP;
	wIndex_t inx, listInx;
	BOOL_T found;
	carProto_p protoP;
	char * firstName;
	int typeCount[N_TYPELISTMAP];
	int listTypeInx, currTypeInx;

	listTypeInx = ( carDlgTypeInx >= 0 ) ? carDlgTypeInx : -1;
	carDlgProtoInx = -1;
	firstName = NULL;

	wComboBoxClear( carDlgPLs[I_CD_PROTOTYPE_LIST].control );
	memset( typeCount, 0, N_TYPELISTMAP * sizeof typeCount[0] );
	LOG( log_carDlgEdit, 3,
	     ( "CarDlgLoadProtoList( %s, %d, %s )\n    carDlgProtoStr=\"%s\", carDlgTypeInx=%d\n",
	       manuf?manuf:"NULL", scale, loadTypeList?"TRUE":"FALSE", carDlgProtoStr,
	       carDlgTypeInx ) )
	if ( manuf==NULL ) {
		if ( carProto_da.cnt <= 0 ) { return FALSE; }
		if ( listTypeInx < 0 && carDlgProtoStr[0]
		     && (protoP=CarProtoFind(carDlgProtoStr)) ) {
			listTypeInx = CarProtoFindTypeCode(protoP->type);
		}
		if ( listTypeInx < 0 ) {
			listTypeInx = CarProtoFindTypeCode(carProto(0)->type);
		}
		for ( inx=0; inx<carProto_da.cnt; inx++ ) {
			protoP = carProto(inx);
			currTypeInx = CarProtoFindTypeCode(protoP->type);
			typeCount[currTypeInx]++;
			if ( carDlgTypeInx >= 0 &&
			     listTypeInx != carDlgTypeInx &&
			     currTypeInx == carDlgTypeInx ) {
				LOG( log_carDlgEdit, 4, ( "    found typeinx, reset list (old=%d)\n",
				                          listTypeInx ) )
				wComboBoxClear( carDlgPLs[I_CD_PROTOTYPE_LIST].control );
				listTypeInx = carDlgTypeInx;
				carDlgProtoInx = -1;
				firstName = NULL;
			}
			if ( currTypeInx != listTypeInx ) { continue; }
			listInx = wComboBoxAddValue( carDlgPLs[I_CD_PROTOTYPE_LIST].control,
			                             protoP->desc, protoP );
			if ( carDlgProtoInx < 0 && carDlgProtoStr[0]
			     && strcasecmp( carDlgProtoStr, protoP->desc ) == 0 ) {
				LOG( log_carDlgEdit, 4, ( "    found protoStr (inx=%d, listInx=%d)\n", inx,
				                          listInx ) )
				carDlgProtoInx = listInx;
				if ( carDlgProtoStr[0] == '\0' ) { strcpy( carDlgProtoStr, protoP->desc ); }
			}
			if ( firstName == NULL ) {
				firstName = protoP->desc;
			}
		}
	} else {
		for ( inx=0; inx<carPartParent_da.cnt; inx++ ) {
			parentP = carPartParent(inx);
			if ( strcasecmp( manuf, parentP->manuf ) != 0 ||
			     scale != parentP->scale ) {
				continue;
			}
			if ( !CheckAvail(parentP) ) {
				continue;
			}
			found = FALSE;
			for ( wIndex_t inx1=0; inx1<parentP->parts_da.cnt; inx1++ ) {
				carPart_p partP = carPart( parentP, inx1 );
				currTypeInx = CarProtoFindTypeCode(partP->type);
				typeCount[currTypeInx]++;
				if ( listTypeInx < 0 ) {
					listTypeInx = currTypeInx;
				}
				if ( carDlgTypeInx >= 0 &&
				     listTypeInx != carDlgTypeInx &&
				     currTypeInx == carDlgTypeInx ) {
					LOG( log_carDlgEdit, 4, ( "    found typeinx, reset list (old=%d)\n",
					                          listTypeInx ) )
					wComboBoxClear( carDlgPLs[I_CD_PROTOTYPE_LIST].control );
					listTypeInx = carDlgTypeInx;
					carDlgProtoInx = -1;
					firstName = NULL;
				}
				if ( listTypeInx == currTypeInx ) {
					found = TRUE;
				}
			}
			if ( !found ) {
				continue;
			}
			listInx = wComboBoxAddValue( carDlgPLs[I_CD_PROTOTYPE_LIST].control,
			                             parentP->proto, parentP );
			if ( carDlgProtoInx < 0 && ( carDlgProtoStr[0] == '\0'
			                             || strcasecmp( carDlgProtoStr, parentP->proto ) == 0 ) ) {
				LOG( log_carDlgEdit, 4, ( "    found protoStr (inx=%d, listInx=%d)\n", inx,
				                          listInx ) )
				carDlgProtoInx = listInx;
				if ( carDlgProtoStr[0] == '\0' ) {
					strcpy( carDlgProtoStr, parentP->proto );
				}
			}
			if ( firstName == NULL ) {
				firstName = parentP->proto;
			}
		}
	}

	found = TRUE;
	if ( carDlgProtoInx < 0 ) {
		found = FALSE;
		/* Only default to the first list entry when nothing was typed yet.
		 * A non-empty carDlgProtoStr that matched nothing in this manuf/
		 * protokind's list is a deliberate new prototype name (e.g. typing
		 * one in while creating a new car) -- it must survive this reload
		 * unresolved (carDlgProtoInx stays -1) rather than being silently
		 * replaced by an unrelated existing name. */
		if ( carDlgProtoStr[0] == '\0' && firstName != NULL ) {
			LOG( log_carDlgEdit, 4, ( "    didn't find protoStr, using [0] = %s\n",
			                          firstName ) )
			carDlgProtoInx = 0;
			strcpy( carDlgProtoStr, firstName );
		}
	}
	wComboBoxSetIndex( carDlgPLs[I_CD_PROTOTYPE_LIST].control,
	                   carDlgProtoInx );

	if ( loadTypeList ) {
		/* Always offer the full picklist here, not just the categories
		 * typeCount found in this manuf's existing catalog scan -- a
		 * manuf/scale combination whose catalog happens not to include a
		 * given category yet must still let the user pick it (and, if no
		 * prototype matches, fall through to the auto-create-prototype path)
		 * rather than silently hiding it as if that category can never apply
		 * to this manufacturer. */
		LOG( log_carDlgEdit, 4, ( "    loading typelist\n" ) )
		wComboBoxClear( carDlgPLs[I_CD_PROTOKIND_LIST].control );
		for ( currTypeInx=0; currTypeInx<N_TYPELISTMAP; currTypeInx++ ) {
			listInx = wComboBoxAddValue( carDlgPLs[I_CD_PROTOKIND_LIST].control,
			                             _(typeListMap[currTypeInx].name), I2VP(currTypeInx) );
			if ( currTypeInx == listTypeInx ) {
				LOG( log_carDlgEdit, 4, ( "        current = %d\n", listInx ) )
				carDlgKindInx = listInx;
			}
		}
	}

	return found;
}


/**
 * Populate protokind-list with the full, unfiltered typeListMap.
 * Used while authoring a prototype from scratch (S_ProtoSel), where there
 * is no catalog data to scan/filter against and every category must be
 * selectable. Contrast with CarDlgLoadProtoList()'s loadTypeList handling,
 * which fills protokind-list with only the categories found in a catalog
 * scan (plus the item's current type).
 */
static void CarDlgLoadProtoKindListFull( void )
{
	wComboBoxClear( carDlgPLs[I_CD_PROTOKIND_LIST].control );
	for ( wIndex_t inx = 0; inx < N_TYPELISTMAP; inx++ ) {
		wComboBoxAddValue( carDlgPLs[I_CD_PROTOKIND_LIST].control,
		                   _(typeListMap[inx].name), I2VP(inx) );
	}
	carDlgKindInx = carDlgTypeInx;
}


static BOOL_T CarDlgLoadPartList( carPartParent_p parentP )
/* Loads PartList from parentP
 * Set carDlgPartnoInx to entry matching carDlgPartnoStr (if set and found)
 * Otherwise set carDlgPartnoInx and carDlgPartnoStr to 1st entry on list
 * Set carDlgDescStr to found entry
 */
{
	wIndex_t listInx;
	wIndex_t inx;
	carPart_p partP;
	carPart_t lastPart;
	tabString_t tabs[7];
	BOOL_T found;
	carPart_p selPartP;
	char partOnly[STR_SIZE];

	carDlgPartnoInx = -1;
	wComboBoxClear( carDlgPLs[I_CD_PARTNO_LIST].control );
	if ( parentP==NULL ) {
		carDlgPartnoStr[0] = '\0';
		carDlgDescStr[0] = '\0';
		return FALSE;
	}
	found = FALSE;
	selPartP = NULL;
	lastPart.title = NULL;
	for ( inx=0; inx<parentP->parts_da.cnt; inx++ ) {
		partP = carPart(parentP,inx);
		TabStringExtract( partP->title, 7, tabs );
		/* List (and, since partno-list is a has-entry combo, the entry box
		 * too) shows the Part number only -- Description is its own field
		 * (I_CD_DESC_STR), not folded in here. Previously this built a
		 * composite "Part Desc Road/Mark Number" string (ConstructPartDesc,
		 * removed) and used *that* as the combo value, so selecting/typing
		 * a part visibly dumped the description into the part-number box. */
		TabStringCpy( partOnly, &tabs[T_PART] );
		lastPart.paramFileIndex = partP->paramFileIndex;
		if ( partOnly[0] && IsParamValid(partP->paramFileIndex) &&
		     ( lastPart.title == NULL || Cmp_part( &lastPart, partP ) != 0 ) ) {
			listInx = wComboBoxAddValue( carDlgPLs[I_CD_PARTNO_LIST].control, partOnly,
			                             partP );
			if ( carDlgPartnoInx<0 &&
			     (carDlgPartnoStr[0]?TabStringCmp( carDlgPartnoStr,
			                                       &tabs[T_PART] ) == 0:TRUE) ) {
				carDlgPartnoInx = listInx;
				found = TRUE;
				selPartP = partP;
			}
			if ( selPartP == NULL ) {
				selPartP = partP;
			}
			lastPart = *partP;
		}
	}
	if ( selPartP == NULL ) {
		carDlgPartnoStr[0] = '\0';
		carDlgDescStr[0] = '\0';
	} else {
		if ( carDlgPartnoInx<0 ) {
			carDlgPartnoInx = 0;
		}
		TabStringExtract( selPartP->title, 7, tabs );
		TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
		TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
	}
	return found;
}


static void CarDlgLoadPart(
        carPart_p partP )
{
	tabString_t tabs[7];
	roadnameMap_p roadnameMapP;
	CarDlgLoadDimsFromPart( partP );
	carDlgBodyColor = partP->color;
	carDlgTypeInx = CarProtoFindTypeCode( partP->type );
	TabStringExtract( partP->title, 7, tabs );
	TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
	TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
	roadnameMapP = LoadRoadnameList( &tabs[T_ROADNAME], &tabs[T_REPMARK] );
	carDlgRoadnameInx = lookupListIndex+1;
	if ( roadnameMapP ) {
		TabStringCpy( carDlgRoadnameStr, &tabs[T_ROADNAME] );
		CarDlgLoadRoadnameList();
		TabStringCpy( carDlgRepmarkStr, &tabs[T_REPMARK] );
	} else {
		carDlgRoadnameInx = 0;
		strcpy( carDlgRoadnameStr, _("Undecorated") );
		carDlgRepmarkStr[0] = '\0';
	}
	TabStringCpy( carDlgNumberStr, &tabs[T_NUMBER] );
	carDlgBodyColor = partP->color;
}


static BOOL_T CarDlgLoadLists(
        BOOL_T isItem,
        tabString_t * tabs,
        SCALEINX_T scale )
{
	BOOL_T loadCustomUnknown = isItem;
	static carProto_t protoTmp;
	static char protoTmpDesc[STR_SIZE];

	if ( tabs ) { TabStringCpy( carDlgManufStr, &tabs[T_MANUF] ); }
	if ( strcasecmp( carDlgManufStr, "unknown" ) == 0 ||
	     strcasecmp( carDlgManufStr, "custom" ) == 0 ) {
		loadCustomUnknown = TRUE;
		/*isItem = FALSE;*/
	}
	if ( (!CarDlgLoadManufList( !isItem, loadCustomUnknown, scale )) && tabs ) {
		TabStringCpy( carDlgManufStr, &tabs[T_MANUF] );
		carDlgManufInx = wComboBoxAddValue( carDlgPLs[I_CD_MANUF_LIST].control,
		                                    carDlgManufStr, NULL );
		isItem = FALSE;
	}
	if ( isItem ) {
		carPartParent_p parentP = (carPartParent_p)wComboBoxGetItemContext(
		                                  carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
		if ( parentP ) {
			if ( tabs ) { TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] ); }
			if ( CarDlgLoadProtoList( carDlgManufStr, scale, TRUE ) || !tabs ) {
				parentP = (carPartParent_p)wComboBoxGetItemContext(
				                  carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
				if ( parentP ) {
					if ( tabs ) { TabStringCpy( carDlgPartnoStr, &tabs[T_PART] ); }
					if ( CarDlgLoadPartList( parentP ) || ( (!tabs) && carDlgPartnoInx>=0 ) ) {
						return TRUE;
					}
				}
			}
		}
	}
	if ( tabs ) { TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] ); }
	if ( !CarDlgLoadProtoList( NULL, 0, TRUE ) && tabs ) {
		/* create dummy proto */
		DIST_T ratio = GetScaleRatio( scale );
		protoTmp.contentsLabel = "temporary";
		protoTmp.paramFileIndex = PARAM_LAYOUT;
		strcpy( protoTmpDesc, carDlgProtoStr );
		protoTmp.desc = protoTmpDesc;
		protoTmp.type = typeListMap[carDlgTypeInx].value;
		protoTmp.dim.carWidth = carDlgDim.carWidth*ratio;
		protoTmp.dim.carLength = carDlgDim.carLength*ratio;
		protoTmp.dim.coupledLength = carDlgDim.coupledLength*ratio;
		protoTmp.dim.truckCenter = carDlgDim.truckCenter*ratio;
		protoTmp.dim.truckCenterOffset = carDlgDim.truckCenterOffset*ratio;
		CarProtoDlgCreateDummyOutline( &carProtoSegCnt, &carProtoSegPtr,
		                               IsLocoType(protoTmp.type), protoTmp.dim.carLength,
		                               protoTmp.dim.carWidth, drawColorBlue );
		protoTmp.segCnt = carProtoSegCnt;
		protoTmp.segPtr = carProtoSegPtr;
		GetSegBounds( zero, 0.0, carProtoSegCnt, carProtoSegPtr, &protoTmp.orig,
		              &protoTmp.size );
		TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] );
		carDlgProtoInx = wComboBoxAddValue( carDlgPLs[I_CD_PROTOTYPE_LIST].control,
		                                    carDlgProtoStr, &protoTmp );/*??*/
	}
	carDlgPartnoInx = -1;
	if ( tabs ) {
		TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
		TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
		/* No catalog part backs this item (custom/orphaned manuf or
		 * prototype) -- add it as its own selectable entry, same as the
		 * dummy-proto fallback above, instead of leaving the list empty. */
		wComboBoxClear( carDlgPLs[I_CD_PARTNO_LIST].control );
		carDlgPartnoInx = wComboBoxAddValue( carDlgPLs[I_CD_PARTNO_LIST].control,
		                                     carDlgPartnoStr, NULL );
	}
	return FALSE;
}


/* ---- level model + visibility table + breadcrumb + button label ---------- */

static void        CarDlgUpdatePanels( void );
static void        CarDlgSetTitleAndOk( void );
static const char *CarDlgOkLabel( const char *verb, int level );

/* carDlgLevel_e + prototype now live in carsprivate.h (shared with
 * careditlogic.c's CarValidateIdentity). */
carDlgLevel_e CarDlgStateLevel( carDlgState_e st )
{
	switch ( st ) {
	case S_ProtoSel:   return LVL_PROTO;
	case S_PartnoSel:  return LVL_MODEL;
	case S_ItemSel:    return LVL_CAR;
	default:           return LVL_NONE;
	}
}
#define CUR_LEVEL   CarDlgStateLevel( currState )

/* True whenever this Ok press will create/edit a car -- either directly at
 * LVL_CAR, or nested inside the "New catalog model" panel (carDlgStkPtr>0),
 * where Ok now finishes the whole car in one press (see CarDlgOk's early pop
 * to LVL_CAR before committing). Car-only fields (ownership block, item
 * index, quantity) need to be reachable while nested too, or there would be
 * no way to ever set them for a car created this way. A *direct*, non-nested
 * edit/create of a catalog model (CarDlgAddDesc/CarDlgUpdPart) has
 * carDlgStkPtr==0 and stays excluded, matching CarDlgOk's own S_PART (not
 * S_ITEM) branch for that case. */
#define CUR_COMMITS_CAR ( CUR_LEVEL == LVL_CAR || \
                          ( CUR_LEVEL == LVL_MODEL && carDlgStkPtr > 0 ) )

static const char *carDlgLevelLabel[3] = {
	/* LVL_PROTO */ N_("prototype"),
	/* LVL_MODEL */ N_("catalog model"),
	/* LVL_CAR   */ N_("car"),
};

#define V_PROTO        (1u << 0)
#define V_MODEL        (1u << 1)
#define V_CAR_INFO     (1u << 2)     /* car, ownership/roster fields */
#define V_CAR_GEOM     (1u << 3)     /* geometry present at car level (was V_CAR_CUSTOM) */
#define V_CAR          (V_CAR_INFO | V_CAR_GEOM)          /* both always shown at LVL_CAR */
#define V_PHYS         (V_PROTO | V_MODEL | V_CAR_GEOM)
#define V_CATALOG      (V_MODEL | V_CAR_GEOM)

typedef struct { int fieldIx; unsigned showMask; } carDlgVis_t;

static const carDlgVis_t carDlgVisTable[] = {
	{ I_CD_MANUF_LIST,      V_MODEL | V_CAR },
	{ I_CD_PROTOKIND_LIST,  V_MODEL | V_CAR | V_PROTO },
	{ I_CD_PROTOTYPE_LIST,  V_MODEL | V_CAR | V_PROTO },
	{ I_CD_ROADNAME_LIST,   V_CATALOG       },
	{ I_CD_REPMARK,         V_CATALOG       },
	{ I_CD_NUMBER,          V_CATALOG       },
	{ I_CD_BODYCOLOR,       V_CATALOG       },
	{ I_CD_CPLRLEN,         V_CATALOG       },
	{ I_CD_CPLDLEN,         V_CATALOG       },
	{ I_CD_CPLRMNT,         V_CATALOG       },
	{ I_CD_CARLENGTH,       V_PHYS          },
	{ I_CD_CARWIDTH,        V_PHYS          },
	{ I_CD_TRKCENTER,       V_PHYS          },
	{ I_CD_TRKOFFSET,       V_CAR_GEOM    },  /* car-only per spec §3/§4 */
	{ I_CD_CANVAS,          V_PHYS          },
	{ I_CD_ITEMINDEX,       V_CAR_INFO      },
	{ I_CD_PURPRC,          V_CAR_INFO      },
	{ I_CD_CURPRC,          V_CAR_INFO      },
	{ I_CD_COND,            V_CAR_INFO      },
	{ I_CD_PURDAT,          V_CAR_INFO      },
	{ I_CD_SRVDAT,          V_CAR_INFO      },
	{ I_CD_NOTES,           V_CAR_INFO      },
};

static unsigned CarDlgCurrentSituation( void )
{
	switch ( CUR_LEVEL ) {
	case LVL_PROTO: return V_PROTO;
	case LVL_MODEL:
		/* Nested inside a car-creation flow (carDlgStkPtr>0): Ok finishes the
		 * whole car from here (see CUR_COMMITS_CAR), so the car-only fields
		 * (item index, quantity, ownership block) must be reachable too --
		 * otherwise there is no way to ever set them for a car created this
		 * way. A direct, non-nested catalog-model edit stays V_MODEL only. */
		return V_MODEL | ( carDlgStkPtr > 0 ? V_CAR_INFO : 0 );
	case LVL_CAR:   return V_CAR;
	default:        return 0u;
	}
}

/* ---- reveal decision (spec §4/§5): pins + resolution-edge auto-reveal ----
 * Per-session state, reset in DoCarPartDlg(). A revealer's *Pinned bit goes
 * TRUE the moment the user toggles it (see the I_CD_*_EXP cases in
 * CarDlgUpdate); once pinned, program auto-reveal for that surface is
 * suppressed for the rest of the session -- user always wins (R3). The edge
 * detector (resCatalogPrev) makes geometry spring open only on a
 * RESOLVED->NEW transition, never every pass and never while resolving, so
 * auto never fights the user within a situation (R4). */
static BOOL_T geomPinned, ownPinned, identPinned;
static resState_e resCatalogPrev = RS_UNRESOLVED;

/* TRUE if any geometry field is currently invalid/blocking commit -- forces
 * the geometry revealer open regardless of the auto/collapse decision (R5),
 * so a user can never be blocked by an error inside a collapsed panel. The
 * dimension fields set bInvalid in their CarDlgUpdate cases. */
static BOOL_T CarDlgGeomHasInvalid( void )
{
	static const int geomIx[] = {
		I_CD_CARLENGTH, I_CD_CARWIDTH, I_CD_TRKCENTER, I_CD_TRKOFFSET,
		I_CD_CPLDLEN, I_CD_CPLRLEN, I_CD_CPLRMNT,
	};
	for ( unsigned i = 0; i < COUNT( geomIx ); i++ ) {
		if ( carDlgPLs[geomIx[i]].bInvalid ) { return TRUE; }
	}
	return FALSE;
}

static void CarDlgApplyReveals( void )
{
	unsigned sit = CarDlgCurrentSituation();

	if ( !geomPinned ) {
		BOOL_T carResolved = ( CUR_LEVEL == LVL_CAR && resCatalog == RS_RESOLVED );
		BOOL_T open = ( ( sit & V_PHYS ) != 0 ) && !carResolved;
		/* open on the RESOLVED->NEW edge; never auto-reverse within a
		 * situation, never while resolving */
		if ( !carDlgResolving &&
		     resCatalogPrev == RS_RESOLVED && resCatalog == RS_NEW ) {
			open = TRUE;
		}
		if ( CarDlgGeomHasInvalid() ) { open = TRUE; }   /* force-open if blocking */
		FormGroupExpanderShow( &carDlgPG, "geometry-revealer", open );
	}
	if ( !ownPinned ) {
		FormGroupExpanderShow( &carDlgPG, "ownership-panel-revealer",
		                       CUR_COMMITS_CAR );
	}
	if ( !identPinned ) {
		FormGroupExpanderShow( &carDlgPG, "identity-summary-revealer",
		                       !( CUR_COMMITS_CAR && carDlgUpdateItemPtr != NULL ) );
	}

	if ( !carDlgResolving ) {
		resCatalogPrev = resCatalog;   /* advance edge detector last */
	}
}

/* Panels: the ownership/info block (index, prices, dates, notes -- car-only)
 * and the nested inset panel wrapping the catalog+geometry fields (the
 * shared widget set reused across LVL_PROTO/LVL_MODEL/LVL_CAR).
 *
 * Ownership is revealed only at LVL_CAR: those fields describe an owned
 * item and have no meaning while creating/editing a catalog model or
 * prototype in isolation.
 *
 * The nested panel's *content* (catalog+geometry fields) is revealed
 * whenever those fields are relevant at all, mirroring the V_PHYS mask --
 * that has to stay this broad, since these are the same fields shown for
 * an ordinary Add/Edit Car at LVL_CAR (carDlgStkPtr==0 there), not just a
 * nested create sub-flow. What actually distinguishes "this is a nested
 * create panel" is `nested` (carDlgStkPtr>0): only then does the frame draw
 * a border, show a title naming the level being created, and show the
 * "will be created on commit" hint -- a direct top-level edit
 * (CarDlgUpdPart/CarDlgUpdProto/CarDlgAddProto/CarDlgAddDesc, or an
 * ordinary resolved car) gets the fields with no surrounding chrome at
 * all, same as a plain grouping looked before Phase C. */
static void CarDlgUpdatePanels( void )
{
	unsigned sit = CarDlgCurrentSituation();
	BOOL_T nested = ( carDlgStkPtr > 0 );

	/* Presence: whole panel absent when irrelevant in this state (must use
	 * FormControlShow -- FormGroupExpanderShow only collapses the body and
	 * leaves the header/labels). Open/collapsed is a separate axis, decided
	 * by CarDlgApplyReveals (pins + resolution-edge auto-reveal). */
	FormControlShow( &carDlgPG, I_CD_GEOMETRY_EXP,  ( sit & V_PHYS ) != 0 );
	FormControlShow( &carDlgPG, I_CD_OWNERSHIP_EXP, CUR_COMMITS_CAR );
	FormControlShow( &carDlgPG, I_CD_IDENTITY_EXP,  TRUE );

	/* Catalog-identity sub-group (manuf, partno, desc, road, mark, number,
	 * color) lives in a plain revealer inside the identity expander: present
	 * at model + car, absent at proto. Wrapping them means "not present at
	 * proto" hides labels and entries together, instead of the vis-table
	 * hiding only the (param-controlled) entries and leaving stray .ui labels.
	 * protokind/prototype stay outside it (universal spine, shown at proto). */
	FormGroupReveal( &carDlgPG, "catalog-identity-revealer",
	                 ( sit & (V_MODEL | V_CAR) ) != 0 );

	/* Open/collapsed decision for the three expanders (pins + edge auto). */
	CarDlgApplyReveals();

	/* Non-blocking notice (spec §11) for the two entry points that mutate a
	 * shared catalog model/prototype record in place (CarDlgUpdPart/
	 * CarDlgUpdProto, identified by carDlgUpdate{Part,Proto}Ptr != NULL and
	 * !nested). shared-warning stays a plain GtkRevealer (never pinnable). */
	{
		BOOL_T editingSharedPart = ( CUR_LEVEL == LVL_MODEL && !nested &&
		                             carDlgUpdatePartPtr != NULL );
		BOOL_T editingSharedProto = ( CUR_LEVEL == LVL_PROTO && !nested &&
		                              carDlgUpdateProtoPtr != NULL );
		FormGroupReveal( &carDlgPG, "shared-warning-revealer",
		                 editingSharedPart || editingSharedProto );
		if ( editingSharedPart ) {
			FormLoadMessage( &carDlgPG, I_CD_SHARED_WARNING, _(
			                         "Changes here only apply to cars added or re-pointed to this model from now on. Existing cars keep their own saved values.") );
		} else if ( editingSharedProto ) {
			FormLoadMessage( &carDlgPG, I_CD_SHARED_WARNING, _(
			                         "Dimension changes here only apply going forward. Outline/shape changes will also apply to existing cars using this prototype, once the layout is reloaded.") );
		}
	}
}

/* OK-button label: leading glyph marks a real commit, verb names the action,
 * object names the level. Glyph is UTF-8 in the label text (wButtonSetLabel
 * -> gtk_label; no BO_ICON / no toolkit change needed).
 *   U+2714 (E2 9C 94) commit
 * There used to be a second, "commit-and-return" framing for the nested
 * "New catalog model" panel's own Ok, back when pressing it staged the
 * model and left a second, separate car-level Ok press still to come. Ok
 * there now finishes the whole car in one press (see CarDlgOk's staging
 * branch), so every Ok button is a real, final commit -- callers describing
 * that nested case now just pass LVL_CAR here instead. */
#define GLYPH_COMMIT   "\xe2\x9c\x94 "

static const char *CarDlgOkLabel( const char *verb, int level )
{
	static char buf[STR_SIZE];
	const char *obj = _(carDlgLevelLabel[level]);

	sprintf( buf, "%s%s %s", GLYPH_COMMIT, verb, obj );
	return buf;
}

/* window title + OK label (sentence case; "catalog model" rename) */
static void CarDlgSetTitleAndOk( void )
{
	switch ( CUR_LEVEL ) {
	case LVL_CAR:
		if ( carDlgUpdateItemPtr == NULL ) {
			sprintf( message, _("New %s scale car"), GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Add"), LVL_CAR ) );
		} else {
			sprintf( message, _("Update %s scale car"), GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Update"), LVL_CAR ) );
		}
		wWinSetTitle( carDlgPG.win, message );
		break;
	case LVL_MODEL:
		if ( carDlgStkPtr > 0 ) {
			/* Nested "New catalog model" panel: its Ok now finishes the
			 * whole car in this one press (see CarDlgOk's staging branch),
			 * so both the title and the button should say what it actually
			 * does -- add/update the car -- not "catalog model", which reads
			 * as if a separate car-level commit still followed. */
			if ( carDlgUpdateItemPtr == NULL ) {
				sprintf( message, _("New %s scale car"), GetScaleName( carDlgScaleInx ) );
				wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Add"), LVL_CAR ) );
			} else {
				sprintf( message, _("Update %s scale car"), GetScaleName( carDlgScaleInx ) );
				wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Update"), LVL_CAR ) );
			}
		} else if ( carDlgUpdatePartPtr == NULL ) {
			sprintf( message, _("New %s scale catalog model"),
			         GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Add"), LVL_MODEL ) );
		} else {
			sprintf( message, _("Update %s scale catalog model"),
			         GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Update"), LVL_MODEL ) );
		}
		wWinSetTitle( carDlgPG.win, message );
		break;
	case LVL_PROTO:
		if ( carDlgUpdateProtoPtr == NULL ) {
			wWinSetTitle( carDlgPG.win, _("New prototype") );
			wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Add"), LVL_PROTO ) );
		} else {
			wWinSetTitle( carDlgPG.win, _("Update prototype") );
			wButtonSetLabel( carDlgPG.okB, CarDlgOkLabel( _("Update"), LVL_PROTO ) );
		}
		break;
	default:
		break;
	}
}

/* Terse "what OK will create" line, shown above the OK button. Computed from
 * the SAME makePart/makeProto predicates CarDlgOk commits from, so it can't
 * drift from the actual fan-out. Generic (never names objects) to avoid
 * per-keystroke jitter; vocabulary matches the window titles ("catalog
 * model", not "part"). Spec §8. */
static const char *CarDlgCommitSummary( void )
{
	static char buf[STR_SIZE];
	BOOL_T makePart  = ( ( carDlgManufInx < 0 || carDlgPartnoInx < 0 )
	                     && carDlgPartnoStr[0] );
	BOOL_T makeProto = ( carDlgProtoInx < 0 && carDlgProtoStr[0]
	                     && CarProtoFind( carDlgProtoStr ) == NULL );

	if ( CUR_LEVEL == LVL_PROTO ) {
		strcpy( buf, ( carDlgUpdateProtoPtr != NULL ) ? _("Updates this prototype")
		        : _("Creates a prototype") );
	} else if ( CUR_LEVEL == LVL_MODEL && carDlgStkPtr == 0 ) {
		/* Direct, non-nested catalog-model edit/create (CarDlgAddDesc/
		 * CarDlgUpdPart) -- no car involved. Nested (carDlgStkPtr>0) falls
		 * through to the LVL_CAR text below instead: CarDlgOk's early pop
		 * means Ok from here finishes the whole car, not just this model. */
		strcpy( buf, ( carDlgUpdatePartPtr != NULL ) ? _("Updates this catalog model")
		        : _("Creates a catalog model") );
		if ( makeProto ) { strcat( buf, _(", plus a new prototype") ); }
		strcat( buf, "." );
		return buf;
	} else {   /* LVL_CAR, or nested LVL_MODEL finishing the car */
		BOOL_T upd = ( carDlgUpdateItemPtr != NULL );
		long   n   = upd ? 1 : carDlgQuantity;
		if ( upd )        { strcpy( buf, _("Updates this car") ); }
		else if ( n > 1 ) { sprintf( buf, _("Creates %ld cars"), n ); }
		else              { strcpy( buf, _("Creates a car") ); }

		if ( makeProto && makePart ) {
			strcat( buf, _(", plus a new prototype and catalog model") );
		} else if ( makePart ) {
			strcat( buf, _(", plus a new catalog model") );
		} else if ( makeProto ) {
			strcat( buf, _(", plus a new prototype") );
		}
	}
	strcat( buf, "." );
	return buf;
}

static void CarDlgShowControls( void )
{
	unsigned sit = CarDlgCurrentSituation();
	int i;

	for ( i = 0; i < (int)COUNT(carDlgVisTable); i++ ) {
		const carDlgVis_t *v = &carDlgVisTable[i];
		FormControlShow( &carDlgPG, v->fieldIx, ( v->showMask & sit ) != 0 );
	}

	/* QTY/MLTNUM: visible only when creating a NEW car */
	FormControlShow( &carDlgPG, I_CD_QTY,
	                 ( sit & V_CAR_INFO ) && carDlgUpdateItemPtr == NULL );
	FormControlShow( &carDlgPG, I_CD_MLTNUM,
	                 ( sit & V_CAR_INFO ) && carDlgUpdateItemPtr == NULL );

	FormControlShow( &carDlgPG, I_CD_DESC_STR,
	                 CUR_LEVEL == LVL_MODEL || S_ITEM );
	FormControlShow( &carDlgPG, I_CD_PARTNO_LIST,
	                 CUR_LEVEL == LVL_MODEL || CUR_LEVEL == LVL_CAR );

	/* Outline-authoring controls: always at LVL_PROTO, and also while
	 * nested inside the Model panel creating a genuinely new (unresolved)
	 * prototype -- see CARDLG_AUTHORING_PROTO_OUTLINE. */
	FormControlShow( &carDlgPG, I_CD_IMPORT, CARDLG_AUTHORING_PROTO_OUTLINE );
	FormControlShow( &carDlgPG, I_CD_RESET,  CARDLG_AUTHORING_PROTO_OUTLINE );
	FormControlShow( &carDlgPG, I_CD_FLIP,   CARDLG_AUTHORING_PROTO_OUTLINE );

	FormControlActive( &carDlgPG, I_CD_ITEMINDEX,
	                   CUR_COMMITS_CAR && carDlgUpdateItemPtr == NULL );
	FormControlActive( &carDlgPG, I_CD_MLTNUM,
	                   CUR_COMMITS_CAR && carDlgQuantity > 1 );
	FormControlActive( &carDlgPG, I_CD_IMPORT, selectedTrackCount > 0 );

	/* No read-only lock here (see hg history / project memory for the
	 * removed carResolved gate): §11 deliberately avoids fork/reference-
	 * count machinery. CarDlgOk already gives the right semantics for free
	 * -- editing geometry while resolved only ever rewrites this car's own
	 * denormalized carDim_t copy (CarPartNew/CarProtoNew are only called
	 * when carDlgManufInx/carDlgPartnoInx/carDlgProtoInx is unresolved), and
	 * changing Manufacturer/Part to a non-matching value naturally creates a
	 * new catalog model (with a new prototype auto-seeded if needed) using
	 * the dialog's current geometry. Direct editing of an existing model/
	 * prototype (CarDlgUpdPart/CarDlgUpdProto) is the dedicated
	 * shared-definition editor and mutates the record in place, as
	 * intended. */

	/* I_CD_MSG is set to the commit disclosure line after CarDlgUpdatePanels
	 * below (was cleared here; the disclosure now provides the resting text). */

	CarDlgUpdatePanels();
	CarDlgSetTitleAndOk();

	FormLoadControls( &carDlgPG );
	FormDialogOkActive( &carDlgPG, CUR_LEVEL != LVL_NONE &&
	                    !( CUR_LEVEL == LVL_CAR &&
	                       ( resCatalog == RS_UNRESOLVED || resProto == RS_UNRESOLVED ) ) );

	/* I_CD_MSG (commit disclosure vs transient status) is arbitrated in
	 * CarDlgUpdate's message block; the CarDlgUpdate(-1) call below reaches it
	 * and sets the resting disclosure text. */
	CarDlgUpdate( &carDlgPG, -1, NULL );
}


/* ---- direct replacements for the old action-array bodies ---------------
 * Each of these is a literal transcription of what the corresponding
 * carDlgAction_e case in the old CarDlgDoActions did (see hg history for
 * that function if the mapping is ever in doubt) -- kept as small helpers
 * only where the original action was reused from more than one call site.
 * ------------------------------------------------------------------------ */

static void CarDlgActLoadDataFromPartList( void )
{
	carPart_p partP = (carPart_p)wComboBoxGetItemContext(
	                          carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoInx );
	if ( partP != NULL ) {
		CarDlgLoadPart( partP );
		FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
		FormLoadSingleControl( &carDlgPG, I_CD_CARWIDTH );
		FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
		FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
		FormLoadSingleControl( &carDlgPG, I_CD_TRKOFFSET );
		FormLoadSingleControl( &carDlgPG, I_CD_CPLRLEN );
		FormLoadSingleControl( &carDlgPG, I_CD_PARTNO_LIST );
		FormLoadSingleControl( &carDlgPG, I_CD_DESC_STR );
		/* CarDlgActShowPartnoList() (fired when a Manufacturer is matched
		 * from an unresolved car) greys this field out until a part
		 * actually resolves -- re-enable it now that we're genuinely
		 * displaying a resolved part's Description, or it stays greyed
		 * even though the text underneath is correct. */
		FormControlActive( &carDlgPG, I_CD_DESC_STR, TRUE );
		FormLoadSingleControl( &carDlgPG, I_CD_ROADNAME_LIST );
		FormLoadSingleControl( &carDlgPG, I_CD_REPMARK );
		FormLoadSingleControl( &carDlgPG, I_CD_NUMBER );
		FormLoadSingleControl( &carDlgPG, I_CD_BODYCOLOR );
	}
}

static void CarDlgActLoadProtoListForManuf( void )
{
	carPartParent_p parentP = (carPartParent_p)wComboBoxGetItemContext(
	                                  carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
	CarDlgLoadProtoList( parentP->manuf, parentP->scale, TRUE );
	FormLoadSingleControl( &carDlgPG, I_CD_PROTOKIND_LIST );
	FormLoadSingleControl( &carDlgPG, I_CD_PROTOTYPE_LIST );
}

static void CarDlgActLoadProtoListAll( void )
{
	CarDlgLoadProtoList( NULL, 0, TRUE );
	FormLoadSingleControl( &carDlgPG, I_CD_PROTOKIND_LIST );
	FormLoadSingleControl( &carDlgPG, I_CD_PROTOTYPE_LIST );
}

static void CarDlgActLoadPartnoList( void )
{
	/* Find the catalog parent for the CURRENT manuf + selected proto + scale.
	 * The prototype list's row context can't be trusted here: when the list
	 * is loaded unfiltered (all prototypes, so a new catalog model can be
	 * authored from any existing prototype) its rows carry a carProto*, not a
	 * carPartParent* -- using that context directly loaded parts against the
	 * wrong struct (or the wrong manufacturer's catalog). A carPartParent is
	 * keyed by (manuf, proto, scale), so look it up explicitly from the
	 * resolved manuf and the selected prototype name. */
	carPartParent_p parentP = NULL;
	carPartParent_p manufParent = (carPartParent_p)wComboBoxGetItemContext(
	                                      carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
	if ( manufParent != NULL && carDlgProtoStr[0] ) {
		wIndex_t i;
		for ( i = 0; i < carPartParent_da.cnt; i++ ) {
			carPartParent_p p = carPartParent(i);
			if ( p->scale == manufParent->scale &&
			     strcasecmp( p->manuf, manufParent->manuf ) == 0 &&
			     strcasecmp( p->proto, carDlgProtoStr ) == 0 ) {
				parentP = p;
				break;
			}
		}
	}
	CarDlgLoadPartList(
	        parentP );  /* NULL -> empty list (new model), which is correct */
	FormLoadSingleControl( &carDlgPG, I_CD_PARTNO_LIST );
}

static void CarDlgActLoadDimsFromProtoList( void )
{
	carProto_p protoP = (carProto_p)wComboBoxGetItemContext(
	                            carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
	if ( protoP ) {
		CarDlgLoadDimsFromProto( protoP );
		carDlgTypeInx = CarProtoFindTypeCode( protoP->type );
	} else {
		/* No resolved prototype to read dims/type from (new/unmatched
		 * name) -- default the dims, but leave carDlgTypeInx alone. It
		 * already holds whatever category the user picked via the
		 * protokind combo (or a restored value from carDlgStk); forcing
		 * it back to 0 here silently discarded that choice and made
		 * every new, unresolved car/catalog model commit as "Diesel
		 * Loco" regardless of the selected kind. */
		DIST_T ratio = GetScaleRatio( carDlgScaleInx );
		carDlgDim.carLength = 50*12/ratio;
		carDlgDim.carWidth = 10*12/ratio;
		carDlgDim.coupledLength = carDlgDim.carLength+carDlgCouplerLength*2;
		carDlgDim.truckCenter = carDlgDim.carLength-59.0*2.0/ratio;
	}
	FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
	FormLoadSingleControl( &carDlgPG, I_CD_CARWIDTH );
	FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
	FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
	FormLoadSingleControl( &carDlgPG, I_CD_TRKOFFSET );
	FormLoadSingleControl( &carDlgPG, I_CD_CPLRLEN );
	if ( S_PROTO ) {
		carDlgKindInx = carDlgTypeInx;
		FormLoadSingleControl( &carDlgPG, I_CD_PROTOKIND_LIST );
	}
}

static void CarDlgActClrPartnoStr( void )
{
	carDlgPartnoStr[0] = '\0';
	carDlgDescStr[0] = '\0';
	wListSetValue( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, "" );
	carDlgPartnoInx = -1;
	FormLoadSingleControl( &carDlgPG, I_CD_DESC_STR );
}

static void CarDlgActClrNumberStr( void )
{
	carDlgNumberStr[0] = '\0';
	FormLoadSingleControl( &carDlgPG, I_CD_NUMBER );
}

/* Description is populated from whichever catalog part ends up resolved,
 * not typed directly -- grey it out (not hide it) while a Manufacturer has
 * matched but no Part Number has been picked yet, so there's nothing to
 * show. Greyed rather than hidden so the field doesn't appear/disappear as
 * the user works through Manufacturer -> Prototype -> Part Number. */
static void CarDlgActShowPartnoList( void )
{
	FormLoadSingleControl( &carDlgPG, I_CD_PARTNO_LIST );
	FormControlActive( &carDlgPG, I_CD_DESC_STR, FALSE );
}

/* Counterpart for the Manufacturer "+" (brand-new, uncatalogued
 * manufacturer): there's no catalog to resolve a Part Number/Description
 * from, so Description becomes a free-text field instead. */
static void CarDlgActHidePartnoList( void )
{
	FormLoadSingleControl( &carDlgPG, I_CD_PARTNO_LIST );
	FormLoadSingleControl( &carDlgPG, I_CD_DESC_STR );
	FormControlActive( &carDlgPG, I_CD_DESC_STR, TRUE );
}

static void CarDlgRecallCouplerLength( void )
{
	sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
	         GetScaleName(carDlgScaleInx) );
	carDlgCouplerLength = 16.0/GetScaleRatio(carDlgScaleInx);
	wPrefGetFloat( carDlgPG.nameStr, message, &carDlgCouplerLength,
	               carDlgCouplerLength );
}

/* Push the current dims/state onto carDlgStk before entering a nested
 * "New Catalog Model" create-panel (see CarDlgNewDesc); CarDlgClose pops
 * it back off on Cancel. */
static void CarDlgPushDims( void )
{
	CHECK( carDlgStkPtr < CARDLG_STK_SIZE );
	carDlgStk[carDlgStkPtr].dim = carDlgDim;
	carDlgStk[carDlgStkPtr].couplerLength = carDlgCouplerLength;
	carDlgStk[carDlgStkPtr].state = currState;
	carDlgStk[carDlgStkPtr].changed = carDlgChanged;
	carDlgStk[carDlgStkPtr].typeInx = carDlgTypeInx;
	if ( currState == S_ItemSel && carDlgPartnoInx >= 0 ) {
		carDlgStk[carDlgStkPtr].partP = (carPart_p)wComboBoxGetItemContext(
		                                        carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoInx );
	} else {
		carDlgStk[carDlgStkPtr].partP = NULL;
	}
	carDlgStkPtr++;
}

static void CarDlgActLoadInfoFromUpdateItem( void )
{
	tabString_t tabs[7];

	carDlgScaleInx = carDlgUpdateItemPtr->scaleInx;
	carDlgItemIndex = carDlgUpdateItemPtr->index;
	TabStringExtract( carDlgUpdateItemPtr->title, 7, tabs );
	TabStringCpy( carDlgManufStr, &tabs[T_MANUF] );
	TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] );
	TabStringCpy( carDlgRoadnameStr, &tabs[T_ROADNAME] );
	TabStringCpy( carDlgRepmarkStr, &tabs[T_REPMARK] );
	TabStringCpy( carDlgNumberStr, &tabs[T_NUMBER] );
	carDlgDim = carDlgUpdateItemPtr->dim;
	carDlgBodyColor = carDlgUpdateItemPtr->color;
	carDlgTypeInx = CarProtoFindTypeCode( carDlgUpdateItemPtr->type );
	carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
	sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
	         GetScaleName(carDlgScaleInx) );
	wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
	carDlgCouplerMount = (carDlgUpdateItemPtr->options&CAR_DESC_COUPLER_MODE_BODY)!=
	                     0;
	carDlgPurchPrice = carDlgUpdateItemPtr->data.purchPrice;
	snprintf( carDlgPurchPriceStr, STR_SIZE, "%0.2f", carDlgPurchPrice );
	carDlgCurrPrice = carDlgUpdateItemPtr->data.currPrice;
	snprintf( carDlgCurrPriceStr, STR_SIZE, "%0.2f", carDlgCurrPrice );
	carDlgCondition = carDlgUpdateItemPtr->data.condition;
	carDlgConditionInx = MapCondition( carDlgUpdateItemPtr->data.condition );
	carDlgPurchDate = carDlgUpdateItemPtr->data.purchDate;
	if ( carDlgPurchDate ) {
		sprintf( carDlgPurchDateStr, "%ld", carDlgPurchDate );
	} else {
		carDlgPurchDateStr[0] = '\0';
	}
	carDlgServiceDate = carDlgUpdateItemPtr->data.serviceDate;
	if ( carDlgServiceDate ) {
		sprintf( carDlgServiceDateStr, "%ld", carDlgServiceDate );
	} else {
		carDlgServiceDateStr[0] = '\0';
	}
	wTextClear( (wText_p)carDlgPLs[I_CD_NOTES].control );
	if ( carDlgUpdateItemPtr->data.notes ) {
		strncpy( message, carDlgUpdateItemPtr->data.notes, sizeof message );
		message[sizeof message - 1] = '\0';
		for ( char * cp=message; *cp; cp++ ) {
			if ( *cp == '\n' ) { *cp = ' '; }
		}
		wTextAppend( (wText_p)carDlgPLs[I_CD_NOTES].control, message );
	}
	LoadRoadnameList( &tabs[T_ROADNAME], &tabs[T_REPMARK] );
	CarDlgLoadRoadnameList();
	carDlgRoadnameInx = lookupListIndex+1;

	/* Level stays LVL_CAR either way; resCatalog alone now carries whether
	 * the loaded item's manuf/partno resolved to an existing catalog part. */
	currState = S_ItemSel;
	resCatalog = CarDlgLoadLists( TRUE, tabs,
	                              carDlgScaleInx ) ? RS_RESOLVED : RS_NEW;
	resItemManuf = resCatalog;
	/* Unlike resCatalog above, resProto is otherwise left holding whatever
	 * this file-scope static was last set to by a previous dialog use (it
	 * is never reset on a fresh CarDlgUpdItem() open) -- if that happened
	 * to be RS_UNRESOLVED, CarDlgUpdate's S_ITEM gate
	 * ("resCatalog==RS_UNRESOLVED || resProto==RS_UNRESOLVED") disables Ok
	 * even though this item's Prototype is perfectly valid. Recompute it
	 * from the just-loaded fields, same as CarDlgActLoadDataFromUpdatePart
	 * already does for the direct part-edit flow. */
	resProto = CarDlgResState( carDlgProtoInx, carDlgProtoStr );
	FormLoadControls( &carDlgPG );
}

static void CarDlgActLoadDataFromUpdatePart( void )
{
	tabString_t tabs[7];

	carDlgScaleInx = carDlgUpdatePartPtr->parent->scale;
	TabStringExtract( carDlgUpdatePartPtr->title, 7, tabs );
	tabs[T_MANUF].ptr = carDlgUpdatePartPtr->parent->manuf;
	tabs[T_MANUF].len = (int)strlen(carDlgUpdatePartPtr->parent->manuf);
	tabs[T_PROTO].ptr = carDlgUpdatePartPtr->parent->proto;
	tabs[T_PROTO].len = (int)strlen(carDlgUpdatePartPtr->parent->proto);
	CarDlgLoadLists( FALSE, tabs, carDlgScaleInx );
	CarDlgLoadPart( carDlgUpdatePartPtr );
	resProto = CarDlgResState( carDlgProtoInx, carDlgProtoStr );
	FormLoadControls( &carDlgPG );
}

static void CarDlgActInitProto( void )
{
	/* I_CD_PROTOTYPE_LIST's bound index -- FormLoadControls() (called via
	 * CarDlgShowControls() below) unconditionally does wComboBoxSetIndex()
	 * for every PD_COMBOLIST control, and since this has-entry combo has
	 * gtk_combo_box_set_entry_text_column() set, that resyncs the entry's
	 * displayed text to whichever row a stale carDlgProtoInx points at --
	 * clobbering the carDlgProtoStr set below with leftover state from
	 * whatever dialog last used this combo. Reset it here so there's no
	 * row to (wrongly) resync to; CarDlgUpdProto/CarDlgAddProto also
	 * explicitly re-push carDlgProtoStr via wListSetValue afterwards. */
	carDlgProtoInx = -1;
	if ( carDlgUpdateProtoPtr==NULL ) {
		carDlgProtoStr[0] = 0;
		carDlgDim.carLength = 50*12;
		carDlgDim.carWidth = 10*12;
		carDlgDim.coupledLength = carDlgDim.carLength+16.0*2.0;
		carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
		carDlgDim.truckCenter = carDlgDim.carLength-59.0*2.0;
		carDlgDim.truckCenterOffset = 0;
	} else {
		strcpy( carDlgProtoStr, carDlgUpdateProtoPtr->desc );
		carDlgDim = carDlgUpdateProtoPtr->dim;
		carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
		carDlgTypeInx = CarProtoFindTypeCode( carDlgUpdateProtoPtr->type );
		carProtoSegCnt = carDlgUpdateProtoPtr->segCnt;
		carProtoSegPtr = carDlgUpdateProtoPtr->segPtr;
		currState = S_ProtoSel;
	}
	FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
	FormLoadSingleControl( &carDlgPG, I_CD_CARWIDTH );
	FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
	FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
	FormLoadSingleControl( &carDlgPG, I_CD_TRKOFFSET );
	FormLoadSingleControl( &carDlgPG, I_CD_CPLRLEN );
}


BOOL_T CheckCarDlgItemIndex( long * index )
{
	BOOL_T found = TRUE;
	BOOL_T updated = FALSE;

	int inx;
	carItem_p item;
	while ( found ) {
		found = FALSE;
		for ( inx=0; inx<carItemInfo_da.cnt; inx++ ) {
			item = carItemInfo(inx);
			if ( item->index == *index ) {
				(*index)++;
				found = TRUE;
				updated = TRUE;
				break;
			}
		}
	}
	return !updated;
}

void
CarDlgError(wBool_t ok, paramData_p p, char *msg)
{
	p->bInvalid = !ok;
	FormHilite(p->group->win, p->control, !ok);
	wTooltipSetText(p->group->win, msg);
}


static const struct {
	int      control;      /* I_CD_MANUF_LIST etc. */
	unsigned bits;         /* welche CAR_IDENT_* dieses Feld betreffen */
	const char *reason;    /* Tooltip, wenn eins davon gesetzt ist */
} carIdentFieldErr[] = {
	{
		I_CD_MANUF_LIST,     CAR_IDENT_MANUF_EMPTY,
		N_("Select or enter a manufacturer")
	},
	{
		I_CD_PARTNO_LIST,    CAR_IDENT_PARTNO_EMPTY,
		N_("Select or enter a part number")
	},
	{
		I_CD_PROTOTYPE_LIST, CAR_IDENT_PROTO_EMPTY,
		N_("Select or enter a prototype")
	},
};

char *carDateResult_s[] = {
	[CAR_DATE_OK] = "",
	[CAR_DATE_EMPTY] = "",
	[CAR_DATE_NOT_NUMERIC] = N_("Enter a 8 digit numeric date (yyyymmdd)"),
	[CAR_DATE_WRONG_LENGTH] = N_("Enter a 8 digit numeric date (yyyymmdd)"),
	[CAR_DATE_OUT_OF_RANGE] = N_("Enter a date between 19000101 and 21991231"),
	[CAR_DATE_BAD_MONTH] = N_("Invalid month"),
	[CAR_DATE_BAD_DAY] = N_("Invalid day")
};


static const struct {
	int      control;
	unsigned bit;
	const char *reason;   /* fester MSG_*-Text, kein Laufzeit-Aufbau (i18n) */
} carDimsFieldErr[] = {
	{ I_CD_CARLENGTH, CAR_DIMS_LENGTH_BAD,    MSG_CARDESC_BAD_DIM_VALUE },
	{ I_CD_CARWIDTH,  CAR_DIMS_WIDTH_BAD,     MSG_CARDESC_BAD_DIM_VALUE },
	{ I_CD_TRKCENTER, CAR_DIMS_TRKCENTER_BAD, MSG_CARDESC_BAD_DIM_VALUE },
	{ I_CD_TRKOFFSET, CAR_DIMS_TRKOFFSET_BAD, MSG_CARDESC_VALUE_ZERO },
	{ I_CD_CPLDLEN,   CAR_DIMS_CPLDLEN_BAD,   MSG_CARDESC_BAD_COUPLER_LENGTH_VALUE },
};

static void CarDlgUpdateSummaries( paramGroup_p pg )
{
	char geom[128];
	char ident[4 * STR_SIZE + 16];
	char *cp;

	/* Geometry summary: "40ft × 10ft, TC 30ft" — omit zero dims. */
	cp = geom;
	*cp = '\0';
	if ( carDlgDim.carLength > 0.0 ) {
		cp += sprintf( cp, "%s", FormatDistance( carDlgDim.carLength ) );
	}
	if ( carDlgDim.carWidth > 0.0 )
		cp += sprintf( cp, "%s%s", ( cp != geom ) ? " \303\227 " : "",
		               FormatDistance( carDlgDim.carWidth ) );
	if ( carDlgDim.truckCenter > 0.0 )
		cp += sprintf( cp, "%s%s", ( cp != geom ) ? ", TC " : "TC ",
		               FormatDistance( carDlgDim.truckCenter ) );

	/* Identity summary: Prototype, Manufacturer, PartNo, Road — omit empties. */
	cp = ident;
	*cp = '\0';
	if ( carDlgProtoStr[0] ) { cp += sprintf( cp, "%s", carDlgProtoStr ); }
	if ( carDlgManufStr[0] ) { cp += sprintf( cp, "%s%s", ( cp != ident ) ? ", " : "", carDlgManufStr ); }
	if ( carDlgPartnoStr[0] ) { cp += sprintf( cp, "%s%s", ( cp != ident ) ? ", " : "", carDlgPartnoStr ); }
	if ( carDlgRoadnameStr[0] ) { cp += sprintf( cp, "%s%s", ( cp != ident ) ? ", " : "", carDlgRoadnameStr ); }

	FormGroupExpanderSetSummary( pg, "geometry-revealer", geom );
	FormGroupExpanderSetSummary( pg, "identity-summary-revealer", ident );
	FormGroupExpanderSetSummary( pg, "ownership-panel-revealer",
	                             _("Model car data") );
}

static void CarDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	BOOL_T redraw = FALSE;
	roadnameMap_p roadnameMapP;
	long valL;
	FLOAT_T ratio;
	BOOL_T ok = TRUE;
	DIST_T len;
	BOOL_T checkTruckCenter = FALSE;
	cmp_key_t cmp_key;
	coOrd orig, size, size2;
	carPartParent_p parentP;
	static long carDlgClock;
	static long carDlgCarLengthClock;
	static long carDlgTruckCenterClock;
	static long carDlgCoupledLengthClock;
	static long carDlgCouplerLengthClock;

	ratio = (S_PROTO?1.0:GetScaleRatio(carDlgScaleInx));

	LOG( log_carDlgState, 3, ( "CarDlgUpdate( %d )\n", inx ) )

	switch ( inx ) {

	case -1:
		carDlgCarLengthClock = carDlgCoupledLengthClock = carDlgTruckCenterClock =
		carDlgCouplerLengthClock = carDlgClock = 0;
		redraw = TRUE;
		break;

	/* User toggled an expander header. The shim routes it here as an ordinary
	 * param change (program FormGroupExpanderShow never reaches this, per the
	 * wExpanderShow guard), so reaching these cases means a genuine user
	 * action -> pin the surface so auto-reveal stops fighting the user for
	 * the session (spec R3). The valL (1 expanded / 0 collapsed) itself is
	 * irrelevant to pinning; any user toggle pins. */
	case I_CD_GEOMETRY_EXP:  geomPinned  = TRUE; break;
	case I_CD_OWNERSHIP_EXP: ownPinned   = TRUE; break;
	case I_CD_IDENTITY_EXP:  identPinned = TRUE; break;

	case I_CD_MANUF_LIST:
		if ( valueP == NULL ) { CarDlgManufFocusOut(); break; }
		if ( carDlgResolving ) { break; }
		carDlgResolving = TRUE;
		/* Typing/picking an identity field (manuf/protokind/prototype/partno)
		 * is resolving *which* catalog record is being edited, not editing
		 * its data -- unlike the dimension/price/condition/etc. cases below,
		 * it must not mark the dialog dirty. Doing so was the real cause of
		 * the double-Cancel bug: the parent car-level's carDlgChanged went
		 * nonzero from typing the identity fields alone (no car data
		 * touched), then CarDlgPushDims() froze that stale count onto the
		 * stack, and CarDlgClose's pop restored it -- so cancelling back out
		 * of the nested "New catalog model" panel hit a spurious "unsaved
		 * changes?" prompt on the second Cancel press. */
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgManufStr,
		                sizeof carDlgManufStr, NULL, NULL );
		/* wListGetValues() hardcodes the index to -1 whenever the entry is
		 * being typed into (list.c's has-entry "editted" branch never
		 * re-checks the text against existing rows) -- re-derive the real
		 * index here so retyping an exact match is recognized as resolved,
		 * not just picking it from the dropdown. */
		carDlgManufInx = wListFindValue( pg->paramPtr[inx].control, carDlgManufStr );
		{
			BOOL_T matched = ( carDlgManufInx >= 0 &&
			                   wComboBoxGetItemContext( pg->paramPtr[inx].control,
			                           carDlgManufInx ) != NULL );
			if ( currState == S_PartnoSel ) {
				/* direct catalog-model edit: manuf combo carries no reload
				 * here, only resModelManuf (matches the old no-op transition
				 * rows for the retired S_PartnoSel/S_PartnoEnter pair). */
				if ( resModelManuf != RS_RESOLVED ) {
					resModelManuf = matched ? RS_RESOLVED : RS_NEW;
				}
				carDlgResolving = FALSE;
				break;
			}
			if ( !matched ) {
				/* Typing a mismatch no longer silently enters "unresolved
				 * car" mode -- it just leaves the model/part list empty and
				 * blocks commit (see CarDlgOk's RS_UNRESOLVED gate) until the
				 * user either matches an existing manufacturer or confirms
				 * by tabbing away from the Manufacturer field (CarDlgManufFocusOut). */
				CarDlgLoadPartList( NULL );
				FormLoadSingleControl( &carDlgPG, I_CD_PARTNO_LIST );
				FormLoadSingleControl( &carDlgPG, I_CD_DESC_STR );
				resCatalog = RS_UNRESOLVED;
			} else {
				/* resItemManuf, not resCatalog: see its declaration comment --
				 * resCatalog's own RS_UNRESOLVED write in the !matched branch
				 * above would make a plain resCatalog==RS_NEW check flicker
				 * away after a second mismatched keystroke. */
				BOOL_T enteringFromUnmatched = ( resItemManuf == RS_NEW );
				currState = S_ItemSel;
				resItemManuf = RS_RESOLVED;
				CarDlgActLoadProtoListForManuf();
				CarDlgActLoadPartnoList();
				CarDlgActLoadDataFromPartList();
				redraw = TRUE;
				if ( enteringFromUnmatched ) {
					CarDlgActShowPartnoList();
				}
				resCatalog = CarDlgResState( carDlgPartnoInx, carDlgPartnoStr );
				/* A real manufacturer is now selected -- clear the "new
				 * manufacturer" hint CarDlgManufFocusOut may have set while it
				 * was still unmatched (that handler only clears it on its own
				 * focus-out path, not on a later matching selection here). */
				FormControlShow( &carDlgPG, I_CD_MANUF_NEW, FALSE );
			}
		}
		carDlgResolving = FALSE;
		break;

	case I_CD_PROTOKIND_LIST:
		if ( carDlgResolving ) { break; }
		carDlgResolving = TRUE;
		/* see the comment in I_CD_MANUF_LIST above -- identity resolution,
		 * not a data edit, must not mark the dialog dirty. */
		{
			int newTypeInx = (int)VP2L(wComboBoxGetItemContext(
			                                   pg->paramPtr[inx].control, carDlgKindInx ));
			if ( newTypeInx == carDlgTypeInx ) {
				/* wComboBoxSetIndex() forces a "changed" signal on every
				 * reload, even when the selection didn't actually move.
				 * Without this guard, reloading protokind-list (e.g. while
				 * populating the dialog for Edit Item) re-triggers a global
				 * proto-list rescan below and clobbers the prototype/partno
				 * lists that were just correctly populated. */
				carDlgResolving = FALSE;
				break;
			}
			carDlgTypeInx = newTypeInx;
		}
		if ( S_PROTO ) {
			redraw = TRUE;
			carDlgResolving = FALSE;
			break;
		}
		if ( CUR_LEVEL == LVL_MODEL ||
		     ( CUR_LEVEL == LVL_CAR && resCatalog != RS_RESOLVED ) ) {
			CarDlgLoadProtoList( NULL, 0, FALSE );
		} else {
			parentP = NULL;
			if ( carDlgProtoInx >= 0 ) {
				parentP = (carPartParent_p)wComboBoxGetItemContext(
				                  pg->paramPtr[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			}
			CarDlgLoadProtoList( carDlgManufStr, (parentP?parentP->scale:0), FALSE );
		}
		/* T_ProtoSel self-transition: only S_ItemSel has a resolved manuf
		 * to reload the partno list/data from; LVL_MODEL (S_PartnoSel) just
		 * reloads dims. */
		if ( currState == S_ItemSel ) {
			CarDlgActLoadPartnoList();
			CarDlgActLoadDataFromPartList();
		} else {
			CarDlgActLoadDimsFromProtoList();
		}
		resProto = CarDlgResState( carDlgProtoInx, carDlgProtoStr );
		redraw = TRUE;
		carDlgResolving = FALSE;
		break;

	case I_CD_PROTOTYPE_LIST:
		if ( valueP == NULL ) { CarDlgProtoFocusOut(); break; }
		if ( carDlgResolving ) { break; }
		carDlgResolving = TRUE;
		/* see the comment in I_CD_MANUF_LIST above -- identity resolution,
		 * not a data edit, must not mark the dialog dirty. */
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgProtoStr,
		                sizeof carDlgProtoStr, NULL, NULL );
		/* see the matching comment in I_CD_MANUF_LIST above */
		carDlgProtoInx = wListFindValue( pg->paramPtr[inx].control, carDlgProtoStr );
		if ( !S_PROTO ) {
			/* Selecting a prototype must re-fill the part catalog for the
			 * current manuf+proto whenever the manufacturer is resolved --
			 * not only when the car was already fully resolved (resCatalog ==
			 * RS_RESOLVED). On a fresh open the pre-populated manuf resolves
			 * but the car itself starts unresolved (resCatalog == RS_NEW, no
			 * part chosen yet); without this, picking a fitting prototype
			 * left the partno list unsearched until the user perturbed manuf.
			 * A resolved manuf has a non-NULL combo context. */
			BOOL_T manufResolved =
			        ( carDlgManufInx >= 0 &&
			          wComboBoxGetItemContext( pg->paramPtr[I_CD_MANUF_LIST].control,
			                                   carDlgManufInx ) != NULL );
			if ( manufResolved && CUR_LEVEL == LVL_CAR ) {
				CarDlgActLoadPartnoList();
				CarDlgActLoadDataFromPartList();
				resCatalog = CarDlgResState( carDlgPartnoInx, carDlgPartnoStr );
			} else {
				CarDlgActLoadDimsFromProtoList();
			}
			resProto = CarDlgResState( carDlgProtoInx, carDlgProtoStr );
			redraw = TRUE;
		}
		carDlgResolving = FALSE;
		break;

	case I_CD_PARTNO_LIST:
		if ( valueP == NULL ) { CarDlgModelFocusOut(); break; }
		if ( carDlgResolving ) { break; }
		carDlgResolving = TRUE;
		/* see the comment in I_CD_MANUF_LIST above -- identity resolution,
		 * not a data edit, must not mark the dialog dirty. */
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgPartnoStr,
		                sizeof carDlgPartnoStr, NULL, NULL );
		/* see the matching comment in I_CD_MANUF_LIST above */
		carDlgPartnoInx = wListFindValue( pg->paramPtr[inx].control, carDlgPartnoStr );
		if ( !S_PART ) {
			if ( carDlgPartnoInx >= 0 ) {
				/* T_PartnoSel: only reachable with a real matched partno at
				 * LVL_CAR (currState == S_ItemSel); any other currState here
				 * is defensive S_Error, not expected to fire from the UI
				 * (matches original). */
				if ( currState == S_ItemSel ) {
					CarDlgActLoadDataFromPartList();
					redraw = TRUE;
				} else {
					currState = S_Error;
				}
				resCatalog = RS_RESOLVED;
			} else {
				/* Typing a mismatched part number no longer silently enters
				 * "unresolved car" mode -- see CarDlgManufFocusOut's
				 * RS_UNRESOLVED gate above; this model/part combination just
				 * isn't confirmed until the field loses focus. */
				resCatalog = RS_UNRESOLVED;
			}
		}
		carDlgResolving = FALSE;
		break;

	case I_CD_ROADNAME_LIST:
		carDlgChanged++;
		roadnameMapP = NULL;
		if ( *(long*)valueP == 0 ) {
			roadnameMapP = NULL;
			carDlgRoadnameStr[0] = '\0';
		} else if ( *(long*)valueP > 0 ) {
			roadnameMapP = (roadnameMap_p)wComboBoxGetItemContext(
			                       pg->paramPtr[I_CD_ROADNAME_LIST].control, (wIndex_t)*(long*)valueP );
			strcpy( carDlgRoadnameStr, roadnameMapP->roadname );
		} else {
			wListGetValues( (wList_p)pg->paramPtr[I_CD_ROADNAME_LIST].control,
			                carDlgRoadnameStr, sizeof carDlgRoadnameStr, NULL, NULL );
			cmp_key.name = carDlgRoadnameStr;
			cmp_key.len = (int)strlen(carDlgRoadnameStr);
			roadnameMapP = LookupListElem( &roadnameMap_da, &cmp_key, Cmp_roadnameMap, 0 );
		}
		/* Seed Mark from the Road->Mark lookup only while it's untouched
		 * (spec §7) -- once the user has typed their own Mark, changing
		 * Roadname must not silently discard it. */
		if ( !carDlgRepmarkTouched ) {
			if ( roadnameMapP ) {
				strcpy( carDlgRepmarkStr, roadnameMapP->repmark );
			} else {
				carDlgRepmarkStr[0] = '\0';
			}
			FormLoadSingleControl( pg, I_CD_REPMARK );
		}
		break;

	case I_CD_REPMARK:
		carDlgRepmarkTouched = TRUE;
		break;

	case I_CD_CARLENGTH:
		carDlgChanged++;
		if ( carDlgDim.carLength == 0.0 ) {
			carDlgCarLengthClock = 0;
		} else if ( carDlgDim.carLength < 100/ratio ) {
			return;
		} else if ( carDlgCouplerLength != 0 && ( carDlgDim.coupledLength == 0
		                || carDlgCouplerLengthClock >= carDlgCoupledLengthClock ) ) {
			len = carDlgDim.carLength+carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.coupledLength = len;
				FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
			}
			carDlgCarLengthClock = ++carDlgClock;
		} else if ( carDlgDim.coupledLength != 0 && ( carDlgCouplerLength == 0
		                || carDlgCoupledLengthClock > carDlgCouplerLengthClock ) ) {
			len = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
			if ( len > 0 ) {
				carDlgCouplerLength = len;
				FormLoadSingleControl( &carDlgPG, I_CD_CPLRLEN );
				if ( !S_PROTO ) {
					sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
					         GetScaleName(carDlgScaleInx) );
					wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
				}
			}
			carDlgCarLengthClock = ++carDlgClock;
		}
		checkTruckCenter = TRUE;
		redraw = TRUE;
		break;

	case I_CD_CPLDLEN:
		carDlgChanged++;
		if ( carDlgDim.coupledLength == 0 ) {
			carDlgCoupledLengthClock = 0;
		} else if ( carDlgDim.coupledLength < 100/ratio ) {
			return;
		} else if ( carDlgDim.carLength != 0 && ( carDlgCouplerLength == 0
		                || carDlgCarLengthClock > carDlgCouplerLengthClock ) ) {
			len = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
			if ( len > 0 ) {
				carDlgCouplerLength = len;
				FormLoadSingleControl( &carDlgPG, I_CD_CPLRLEN );
				if ( !S_PROTO ) {
					sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
					         GetScaleName(carDlgScaleInx) );
					wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
				}
			}
			carDlgCoupledLengthClock = ++carDlgClock;
		} else if ( carDlgCouplerLength != 0 && ( carDlgDim.carLength == 0
		                || carDlgCouplerLengthClock >= carDlgCarLengthClock ) ) {
			len = carDlgDim.coupledLength-carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.carLength = len;
				FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
				checkTruckCenter = TRUE;
			}
			carDlgCoupledLengthClock = ++carDlgClock;
		}
		redraw = TRUE;
		break;

	case I_CD_CPLRLEN:
		carDlgChanged++;
		if ( carDlgCouplerLength == 0 ) {
			carDlgCouplerLengthClock = 0;
			redraw = TRUE;
			break;
		} else if ( carDlgCouplerLength < 1/ratio ) {
			return;
		} else if ( carDlgDim.carLength != 0 && ( carDlgDim.coupledLength == 0
		                || carDlgCarLengthClock >= carDlgCoupledLengthClock ) ) {
			len = carDlgDim.carLength+carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.coupledLength = len;
				FormLoadSingleControl( &carDlgPG, I_CD_CPLDLEN );
			}
			carDlgCouplerLengthClock = ++carDlgClock;
		} else if ( carDlgDim.coupledLength != 0 && ( carDlgDim.carLength == 0
		                || carDlgCoupledLengthClock > carDlgCarLengthClock ) ) {
			len = carDlgDim.coupledLength-carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.carLength = len;
				FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
				checkTruckCenter = TRUE;
			}
			carDlgCouplerLengthClock = ++carDlgClock;
		}
		if ( !S_PROTO ) {
			sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
			         GetScaleName(carDlgScaleInx) );
			wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
		}
		redraw = TRUE;
		break;

	case I_CD_CARWIDTH:
		carDlgChanged++;
		if ( carDlgDim.carLength < 30/ratio ) { return; }
		redraw = TRUE;
		break;

	case I_CD_BODYCOLOR:
		carDlgChanged++;
		RecolorSegs( carDlgSegs_da.cnt, &carDlgSegs(0), carDlgBodyColor );
		redraw = TRUE;
		break;

	case I_CD_TRKOFFSET:
		carDlgChanged++;
#ifdef TRUCK_OFFSET
		if ( carDlgDim.truckCenterOffset == 0 ) {
			carDlgTruckOffsetL = carDlgDim.truckCenter/2;
			carDlgTruckOffsetR = carDlgTruckOffsetL;
		} else if (carDlgDim.carLength - carDlgDim.truckCenter > 2*fabs(
		                   carDlgDim.truckCenterOffset)) {
			carDlgTruckOffsetL = (carDlgDim.carLength-carDlgDim.truckCenter)/2 -
			                     carDlgDim.truckCenterOffset;
			carDlgTruckOffsetR = (carDlgDim.carLength-carDlgDim.truckCenter)/2 +
			                     carDlgDim.truckCenterOffset;
		} else {
			carDlgTruckOffsetL = 0;
			carDlgTruckOffsetR = 0;
		}
#endif
		if ( 2*carDlgDim.truckCenterOffset > carDlgDim.carLength -
		     carDlgDim.truckCenter) {
			ok = FALSE;
			CarDlgError( ok, &carDlgPLs[I_CD_TRKOFFSET],
			             _("Truck Center Offset plus Truck Centers must be less than Car Length") );
		}
		redraw = TRUE;
		break;

	case I_CD_TRKCENTER:
		carDlgChanged++;
		redraw = TRUE;
		break;

	case I_CD_QTY:
		wControlActive( carDlgPLs[I_CD_MLTNUM].control, carDlgQuantity>1 );
		break;

	case I_CD_PURPRC:
	case I_CD_CURPRC: {
		wBool_t ok;
		char carPriceResult[STR_SIZE] = "";
		carDlgChanged++;

		ok = CarValidatePrice(pg->paramPtr[inx].valueP,
		                      (FLOAT_T*)(pg->paramPtr[inx].context));
		if(!ok) {
			snprintf( carPriceResult, STR_SIZE, N_("%s not valid"),
			          pg->paramPtr[inx].winLabel );
		}
		FormErrorState( &pg->paramPtr[inx], ok, carPriceResult );

		break;
	}
	case I_CD_COND:
		carDlgChanged++;
		carDlgCondition =
		        (carDlgConditionInx==0)?0:
		        (carDlgConditionInx==1)?100:
		        (carDlgConditionInx==2)?80:
		        (carDlgConditionInx==3)?60:
		        (carDlgConditionInx==4)?40:20;
		break;

	case I_CD_PURDAT:
	case I_CD_SRVDAT: {
		carDateResult_e result;
		carDlgChanged++;

		result = CarValidateDate((char*)pg->paramPtr[inx].valueP, &valL);
		if(result == CAR_DATE_OK || result == CAR_DATE_EMPTY) {
			FormErrorState(&pg->paramPtr[inx], TRUE, "");
		} else {
			FormErrorState( &pg->paramPtr[inx], FALSE, carDateResult_s[result] );
		}

		if (inx == I_CD_PURDAT) {
			carDlgPurchDate = valL;
		} else {
			carDlgServiceDate = valL;
		}
		break;
	}
	case I_CD_IMPORT:
		carDlgChanged++;
		WriteSelectedTracksToTempSegs();
		carProtoSegCnt = tempSegs_da.cnt;
		carProtoSegPtr = &tempSegs(0);
		CloneFilledDraw( carProtoSegCnt, carProtoSegPtr, TRUE );
		GetSegBounds( zero, 0.0, carProtoSegCnt, carProtoSegPtr, &orig, &size );
		if ( size.x <= 0.0 ||
		     size.y <= 0.0 ||
		     size.x < size.y ) {
			NoticeMessage( MSG_CARPROTO_BADSEGS, _("Ok"), NULL );
			return;
		}
		orig.x = -orig.x;
		orig.y = -orig.y;
		MoveSegs( carProtoSegCnt, carProtoSegPtr, orig );
		/* At LVL_PROTO, carDlgDim holds real-world inches, so the selected
		 * tracks (drawn at the layout's model scale) get scaled up by
		 * curScaleRatio. Nested inside the Model panel (authoring a new
		 * prototype's outline for an unresolved Prototype field),
		 * carDlgDim holds model-scale inches instead -- use the on-layout
		 * size as-is (no rounding to whole real-world inches either, since
		 * model-scale sizes are routinely fractional). CarDlgBuildProtoSegs
		 * rescales this model-scale outline back up to real-world size at
		 * commit time, same as it already does for carDlgDim itself via
		 * SeedProtoDimsFromCatalog. */
		if ( S_PROTO ) {
			size2.x = floor(size.x*curScaleRatio+0.5);
			size2.y = floor(size.y*curScaleRatio+0.5);
		} else {
			size2.x = size.x;
			size2.y = size.y;
		}
		RescaleSegs( carProtoSegCnt, carProtoSegPtr, size2.x/size.x, size2.y/size.y,
		             S_PROTO ? curScaleRatio : 1.0 );
		carDlgDim.carLength = size2.x;
		carDlgDim.carWidth = size2.y;
		carDlgDim.coupledLength = carDlgDim.carLength + 32;
		carDlgFlipToggle = FALSE;
		FormLoadSingleControl( &carDlgPG, I_CD_CARLENGTH );
		FormLoadSingleControl( &carDlgPG, I_CD_CARWIDTH );
		FormLoadSingleControl( &carDlgPG, I_CD_CPLRLEN );
		FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
		FormLoadSingleControl( &carDlgPG, I_CD_TRKOFFSET );
		redraw = TRUE;
		break;

	case I_CD_RESET:
		carDlgChanged++;
		carProtoSegCnt = 0;
		redraw = TRUE;
		break;

	case I_CD_FLIP:
		carDlgChanged++;
		carDlgFlipToggle = ! carDlgFlipToggle;
		redraw = TRUE;
		break;

	default:
		LOG( log_carDlgState, 1, ( "unexpected inx %d in CarDlgUpdate\n", inx ) )
		break;
	}

	if ( checkTruckCenter && carDlgDim.carLength > 0 ) {
		FormLoadSingleControl( &carDlgPG, I_CD_TRKCENTER );
		FormLoadSingleControl( &carDlgPG, I_CD_TRKOFFSET );
	}

	/* Message arbitration runs at the tail of every CarDlgUpdate, including
	 * the re-entrant calls that combobox list-repopulation fires via GTK
	 * "changed" signals. Those re-entrant calls arrive with carDlgResolving
	 * set and transient state, and would overwrite the disclosure with empty
	 * in a loop. Skip them: only a settled (non-resolving) update sets the
	 * message. */
	if ( !carDlgResolving ) {
		if ( S_PART && carDlgManufStr[0] == '\0' ) {
			FormLoadMessage( &carDlgPG, I_CD_MSG, _("Select or Enter a Manufacturer") );
		} else if ( S_ITEM && carDlgUpdateItemPtr==NULL &&
		            ( valL = carDlgItemIndex, !CheckCarDlgItemIndex(&carDlgItemIndex) ) ) {
			sprintf( message,
			         _("Item Index %ld duplicated an existing item: updated to new value"), valL );
			FormLoadSingleControl( &carDlgPG, I_CD_ITEMINDEX );
			FormLoadMessage( &carDlgPG, I_CD_MSG, message );
			ok = TRUE;
		} else {
			/* No transient status: show the commit disclosure as the resting
			 * text (spec §8). */
			FormLoadMessage( pg, I_CD_MSG, CarDlgCommitSummary() );
			ok = TRUE;
		}
		CarDlgUpdateSummaries( pg );          /* live expander summaries (R2) */
	}

	if ( redraw ) {
		CarDlgRedraw( carDlgD.d, NULL, 0, 0 );
	}
	if ( CUR_LEVEL != LVL_NONE ) {
		unsigned resultIdent;
		resultIdent = CarValidateIdentity( CUR_LEVEL, resCatalog, resProto,
		                                   carDlgManufStr, carDlgProtoStr, carDlgPartnoStr );
		for ( int i = 0; i < COUNT(carIdentFieldErr); i++ ) {
			wBool_t bad = ( resultIdent & carIdentFieldErr[i].bits ) != 0;
			FormErrorState( &carDlgPLs[carIdentFieldErr[i].control],
			                !bad, carIdentFieldErr[i].reason );
		}
		if ( resultIdent != 0 ) {
			ok = FALSE;
		}
	}

	/* Dims validation applies at all levels (catalogue model and cars), so no
	 * state guard is required. Flags each invalid field individually and blocks the commit. */
	{
		unsigned resultDims = CarValidateDims( &carDlgDim );

		for ( int i = 0; i < COUNT(carDimsFieldErr); i++ ) {
			wBool_t bad = ( resultDims & carDimsFieldErr[i].bit ) != 0;
			FormErrorState( &carDlgPLs[carDimsFieldErr[i].control],
			                !bad, carDimsFieldErr[i].reason );
		}
		if ( resultDims != 0 ) {
			ok = FALSE;
		}
	}

	/* Prototype only makes sense once a Manufacturer is resolved (matched,
	 * or confirmed via its own "+") -- carDlgManufInx>=0 is the same
	 * "resolved enough" test already used for partno/proto elsewhere in
	 * this file. Outside the car level the manuf step doesn't apply (the
	 * nested Model/Proto panels carry their own manuf context already). */
	FormControlActive( pg, I_CD_PROTOKIND_LIST, !S_ITEM || carDlgManufInx >= 0 );
	FormControlActive( pg, I_CD_PROTOTYPE_LIST, !S_ITEM || carDlgManufInx >= 0 );

	FormDialogOkActive( pg, ok );
}



static void CarDlgNewDesc( void )
{
	carDlgNewPartPtr = NULL;
	carDlgNewProtoPtr = NULL;
	carDlgUpdatePartPtr = NULL;
	carDlgNumberStr[0] = '\0';
	FormLoadSingleControl( &carDlgPG, I_CD_NUMBER );

	/* Reachable at LVL_CAR whether the car is currently resolved (resCatalog
	 * == RS_RESOLVED) or not (manufacturer typed/added but no catalog model
	 * matched yet, including via the Manufacturer "+" icon) -- both
	 * legitimately open the nested "New catalog model" panel per the
	 * filtered-picker/explicit-create design. */
	if ( CUR_LEVEL == LVL_CAR && !carDlgResolving ) {
		char savedManufStr[STR_SIZE];
		char savedProtoStr[STR_SIZE];
		carDlgResolving = TRUE;
		CarDlgPushDims();
		strcpy( savedManufStr, carDlgManufStr );
		/* bInclCustomUnknown=FALSE: a Catalog Model's identity *is* a real
		 * manufacturer's part -- CarPartNew() refuses (returns NULL) to
		 * create one with manufacturer "Custom"/"Unknown", so those must
		 * not be offered here. They're only valid for the car/item level,
		 * where CarPartNew returning NULL is already handled gracefully
		 * (the car is saved without a backing catalog part). */
		CarDlgLoadManufList( TRUE, FALSE, carDlgScaleInx );
		if ( strcasecmp( savedManufStr, carDlgManufStr ) != 0 ) {
			/* Typed/added manufacturer isn't yet a real catalog entry, or
			 * was left blank entirely -- CarDlgLoadManufList()'s "nothing
			 * matched" fallback silently substitutes the first known
			 * manufacturer; restore whatever the user actually had (typed
			 * text, or nothing) instead of discarding it. */
			strcpy( carDlgManufStr, savedManufStr );
			carDlgManufInx = -1;
			wListSetValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control,
			               carDlgManufStr );
		}
		FormLoadSingleControl( &carDlgPG, I_CD_MANUF_LIST );

		strcpy( savedProtoStr, carDlgProtoStr );
		CarDlgActLoadProtoListAll();
		if ( strcasecmp( savedProtoStr, carDlgProtoStr ) != 0 ) {
			/* same "nothing matched"/blank fallback problem as manuf above,
			 * in CarDlgLoadProtoList() -- same fix. */
			strcpy( carDlgProtoStr, savedProtoStr );
			carDlgProtoInx = -1;
			wListSetValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
			               carDlgProtoStr );
		}
		currState = S_PartnoSel;
		resCatalog = RS_NEW;
		resModelManuf = RS_RESOLVED;
		/* resProto is otherwise only refreshed by the I_CD_PROTOTYPE_LIST
		 * changed-handler/CarDlgProtoFocusOut, neither of which has fired
		 * yet on first entering this nested panel -- compute it right away
		 * so CarDlgShowControls() below shows Import/Reset/Flip immediately
		 * for a genuinely new (unresolved) prototype instead of only after
		 * the user first touches the Prototype field. */
		resProto = CarDlgResState( carDlgProtoInx, carDlgProtoStr );
		/* fresh nested Model creation: don't inherit an outline drawn for a
		 * previous, abandoned one earlier in this same dialog session (see
		 * CARDLG_AUTHORING_PROTO_OUTLINE/CarDlgBuildProtoSegs). */
		carProtoSegCnt = 0;
		carDlgFlipToggle = FALSE;
		CarDlgShowControls();
		carDlgResolving = FALSE;
	}

	carDlgChanged = 0;
}


/* Manufacturer combo loses focus (LVL_CAR only): confirm a typed, unmatched
 * manufacturer name as a new one, in place -- no nested panel, no state
 * change. The name only becomes a real carPartParent_t once a part is
 * actually committed under it (CarPartNew, unchanged); until then this is
 * just a picklist entry so the rest of the dialog treats it as legitimate.
 * The "manuf-new" hint label is shown only for the entry actually created
 * here, and hidden again once the field holds nothing or a real match. */
static void CarDlgManufFocusOut( void )
{
	if ( CUR_LEVEL != LVL_CAR || carDlgResolving ) { return; }

	wListGetValues( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufStr,
	                sizeof carDlgManufStr, NULL, NULL );
	if ( carDlgManufStr[0] == '\0' ) {
		FormControlShow( &carDlgPG, I_CD_MANUF_NEW, FALSE );
		return;
	}
	if ( strcasecmp( carDlgManufStr, "Custom" ) == 0 ||
	     strcasecmp( carDlgManufStr, "Unknown" ) == 0 ) {
		/* sentinel entries added unconditionally by CarDlgLoadManufList()
		 * with a NULL context (bInclCustomUnknown) -- they always already
		 * exist as combo rows, so the context==NULL test below can't be
		 * used to tell them apart from a genuinely new typed name. */
		FormControlShow( &carDlgPG, I_CD_MANUF_NEW, FALSE );
		return;
	}
	if ( carDlgManufInx >= 0 &&
	     wComboBoxGetItemContext( carDlgPLs[I_CD_MANUF_LIST].control,
	                              carDlgManufInx ) != NULL ) {
		/* already a real, matched manufacturer -- nothing to add */
		FormControlShow( &carDlgPG, I_CD_MANUF_NEW, FALSE );
		return;
	}

	carDlgResolving = TRUE;
	carDlgManufInx = wComboBoxAddValue( carDlgPLs[I_CD_MANUF_LIST].control,
	                                    carDlgManufStr, NULL );
	wComboBoxSetIndex( carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );

	/* new manufacturer has no catalog yet -- model/part list is legitimately
	 * empty, and prototypes aren't filtered to any (nonexistent) catalog. */
	CarDlgActLoadProtoListAll();
	CarDlgActClrPartnoStr();
	CarDlgActClrNumberStr();
	CarDlgActLoadDimsFromProtoList();
	CarDlgActHidePartnoList();
	resCatalog = RS_NEW;
	/* Arm the same one-shot flag the load-time sites set (see
	 * enteringFromUnmatched in I_CD_MANUF_LIST above) -- without this, typing
	 * an unmatched manufacturer here and later correcting it to a real one
	 * would repopulate the proto/partno lists but never auto-show them. */
	resItemManuf = RS_NEW;

	CarDlgShowControls();
	FormControlShow( &carDlgPG, I_CD_MANUF_NEW, TRUE );
	carDlgResolving = FALSE;
}

/* Part/Model combo loses focus (LVL_CAR only, regardless of whether the car
 * is currently resolved): open the nested "New catalog model" panel. Delegates to
 * CarDlgNewDesc(), which (as of this phase) preserves whatever manufacturer
 * text/selection is already in place rather than blanking it, and which
 * only actually opens the panel (setting resCatalog to RS_NEW) when its own
 * state guard is met -- the "partno-new" hint label mirrors that outcome.
 * An empty field is untouched, not a new model -- leave it alone rather
 * than launching the nested panel. */
static void CarDlgModelFocusOut( void )
{
	if ( carDlgResolving ) { return; }
	wListGetValues( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoStr,
	                sizeof carDlgPartnoStr, NULL, NULL );
	if ( carDlgPartnoStr[0] == '\0' ) {
		FormControlShow( &carDlgPG, I_CD_PARTNO_NEW, FALSE );
		return;
	}
	if ( CUR_LEVEL == LVL_CAR && resCatalog == RS_RESOLVED ) {
		/* Already matched an existing catalog part (e.g. auto-populated by
		 * picking a real Manufacturer) -- CarDlgNewDesc() unconditionally
		 * opens the "New catalog model" flow and forces resCatalog = RS_NEW,
		 * which would spuriously flag an already-resolved part as new. */
		FormControlShow( &carDlgPG, I_CD_PARTNO_NEW, FALSE );
		return;
	}
	CarDlgNewDesc();
	FormControlShow( &carDlgPG, I_CD_PARTNO_NEW, resCatalog == RS_NEW );
}

/* Prototype combo loses focus: resProto already tracks match/no-match on
 * every keystroke via the I_CD_PROTOTYPE_LIST case above (but only while
 * !S_PROTO -- at LVL_PROTO the field isn't resolved against anything, see
 * that case), and CarDlgOk's S_PART commit branch auto-creates the prototype
 * from the model's entered geometry if it's still unresolved. This handler
 * mirrors that into the "prototype-new" hint label, and -- only inside the
 * nested "New catalog model" panel (LVL_MODEL), where a "+" icon used to
 * live -- refreshes the panel's visible controls (protokind-list is already
 * shown at LVL_MODEL per carDlgVisTable). An empty field is untouched, not a
 * new prototype -- force resProto back to unresolved rather than trusting
 * whatever the last keystroke (or the panel's own init) left it as. */
static void CarDlgProtoFocusOut( void )
{
	if ( carDlgResolving ) { return; }
	if ( !S_PROTO ) {
		wListGetValues( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoStr,
		                sizeof carDlgProtoStr, NULL, NULL );
		if ( carDlgProtoStr[0] == '\0' ) {
			resProto = RS_UNRESOLVED;
		}
		FormControlShow( &carDlgPG, I_CD_PROTOTYPE_NEW, resProto == RS_NEW );
	}
	if ( CUR_LEVEL != LVL_MODEL ) { return; }
	CarDlgShowControls();
}


static void CarDlgClose( wWin_p win )
{
	tabString_t tabs[7];

	if ( carDlgChanged ) {
		if ( !inPlayback ) {
			if ( NoticeMessage( MSG_CARDESC_CHANGED, _("Yes"), _("No") ) <= 0 ) {
				return;
			}
		} else {
			PlaybackMessage( "Car Desc Changed\n" );
		}
	}
	if ( carDlgStkPtr > 0 && !carDlgResolving ) {
		carDlgResolving = TRUE;
		carDlgStkPtr--;
		currState = carDlgStk[carDlgStkPtr].state;
		carDlgChanged = carDlgStk[carDlgStkPtr].changed;

		/* T_NewPartDone: returning from nested catalog-model creation
		 * (only ever reachable with currState restored to S_ItemSel; the
		 * "New Prototype" nested sub-flow that used to push a second level
		 * on top of this one -- oldState==S_ProtoSel -- has been removed,
		 * so this is the only kind of pop CarDlgClose ever handles now). */
		if ( currState == S_ItemSel ) {
			if ( carDlgNewPartPtr != NULL ) {
				TabStringExtract( carDlgNewPartPtr->title, 7, tabs );
				TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
				TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
				wListSetValue( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoStr );
				FormLoadSingleControl( &carDlgPG, I_CD_DESC_STR );
			} else {
				if ( carDlgStk[carDlgStkPtr].partP ) {
					TabStringExtract( carDlgStk[carDlgStkPtr].partP->title, 7, tabs );
					strcpy( carDlgManufStr, carDlgStk[carDlgStkPtr].partP->parent->manuf );
					strcpy( carDlgProtoStr, carDlgStk[carDlgStkPtr].partP->parent->proto );
					TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
					TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
				}
				carDlgTypeInx = carDlgStk[carDlgStkPtr].typeInx;
			}
			currState = S_ItemSel;
			if ( CarDlgLoadLists( TRUE, NULL, carDlgScaleInx ) ) {
				resCatalog = RS_RESOLVED;
			} else {
				resCatalog = RS_NEW;
				CarDlgActLoadDimsFromProtoList();
			}
			resItemManuf = resCatalog;
			CarDlgShowControls();
		}
		carDlgResolving = FALSE;
	} else if ( carDlgStkPtr == 0 ) {
		wTextClear( (wText_p)carDlgPLs[I_CD_NOTES].control );
		wHide( carDlgPG.win );
	}
}

/* Cancel button. Unlike CarDlgClose's pop-one-level-per-call semantics
 * (reused internally after a successful Ok commit at a nested level, to
 * return to the still-open parent screen), pressing Cancel abandons the
 * whole flow -- the nested "New catalog model"/"New prototype" panel AND
 * its parent -- in a single click, matching ordinary Cancel-button
 * expectations. It still warns first if genuine data was entered anywhere
 * in the flow: carDlgChanged only counts real data edits (dimensions,
 * price, condition, dates, road name, import/reset/flip -- see the
 * per-case comments in CarDlgUpdate); identity-field navigation
 * (manuf/protokind/prototype/partno) no longer bumps it, so opening the
 * nested panel and only typing a name no longer triggers this. Every
 * pushed level's frozen count (carDlgStk[i].changed) must be checked too,
 * not just the current innermost one, so real changes made before
 * descending into the nested panel still get flagged. */
static void CarDlgCancel( wWin_p win )
{
	wIndex_t i;
	BOOL_T anyChanged = carDlgChanged != 0;

	for ( i = 0; i < carDlgStkPtr; i++ ) {
		anyChanged |= ( carDlgStk[i].changed != 0 );
	}

	if ( anyChanged ) {
		if ( !inPlayback ) {
			if ( NoticeMessage( MSG_CARDESC_CHANGED, _("Yes"), _("No") ) <= 0 ) {
				return;
			}
		} else {
			PlaybackMessage( "Car Desc Changed\n" );
		}
	}

	carDlgStkPtr = 0;
	wTextClear( (wText_p)carDlgPLs[I_CD_NOTES].control );
	wHide( carDlgPG.win );
}

/* Rewrites the whole custom param file from the current live registries
 * (same mechanism as the "Manage Custom" dialog's Done button,
 * dcustmgm.c:CustomDone), instead of appending just the one changed
 * record. A part/prototype rename changes that record's on-file key, so a
 * plain append leaves the superseded line behind under its old key --
 * harmless in memory (CarPartNew/CarProtoNew coalesce same-key lines on
 * read and drop the unlinked old struct), but on the next reload both keys
 * come back as independent, duplicate live records. Doing a full compacting
 * rewrite on every catalog commit keeps the file always matching the
 * in-memory state, at the cost of rewriting the whole (typically small)
 * custom file -- including compound/turnout entries -- on every car/proto/
 * part edit, not just renames. */
static BOOL_T CarDlgSaveCustom( void )
{
	FILE * f = OpenCustom("w");
	if ( f == NULL ) {
		return FALSE;
	}
	SetCLocale();
	CompoundCustomSave(f);
	CarCustomSave(f);
	fclose(f);
	SetUserLocale();
	return TRUE;
}


/* ---------------------------------------------------------------------------
 * Notes extraction (UTF-8 / GTK3 correct).
 *
 * wTextGetSize() returns strlen()+1 -- byte length incl. the NUL -- and GTK3
 * text is UTF-8, so a trailing multibyte character (umlaut, Nordic glyph) must
 * not be indexed by the returned size. Extract once, terminate on the real
 * byte length (strlen), guarantee exactly one trailing '\n'. Returns an owned
 * buffer, or NULL for "no notes" (which clears any notes on the item).
 *
 * This replaces the old inline block that allocated (len+2)*sizeof(wchar_t)
 * and wrote the terminator at the size-denominated index, which embedded a
 * stray NUL (text\0\n\0) for any note lacking a trailing newline.
 * ------------------------------------------------------------------------- */
static char *CarDlgExtractNotes( void )
{
	int sz = wTextGetSize( (wText_p)carDlgPLs[I_CD_NOTES].control ); /* >= 1 */
	char *note;
	size_t n;

	if ( sz <= 1 ) {
		return NULL;
	}
	note = (char *)MyMalloc( sz + 1 );            /* +1 room for appended '\n' */
	wTextGetText( (wText_p)carDlgPLs[I_CD_NOTES].control, note, sz );
	note[sz - 1] = '\0';                          /* ensure termination */
	n = strlen( note );                           /* real UTF-8 byte length */
	if ( n == 0 ) {
		MyFree( note );
		return NULL;
	}
	if ( note[n - 1] != '\n' ) {
		note[n]     = '\n';
		note[n + 1] = '\0';
	}
	return note;
}

/* Prepare the owned prototype outline for a newly-created proto, matching the
 * seg source CarDlgLoadDims uses for the current state (see CARDLG_AUTHORING_
 * PROTO_OUTLINE). On the authoring path (S_PROTO, or S_PART with a new proto)
 * carDlgSegs_da already holds the fitted, real-world, flip-applied outline --
 * copy it verbatim. Otherwise scale the model-scale authoring outline
 * carProtoSegPtr up to real-world and apply flip (the former CarDlgAutoCreate-
 * Proto transform). Falls back to a generated dummy box when no outline
 * exists. Always yields an OWNED heap buffer (CommitStaged frees it) and sets
 * *segCntP. */
static trkSeg_p CarDlgBuildProtoSegs( const carDim_t *protoDim, long typeVal,
                                      wIndex_t *segCntP )
{
	trkSeg_p owned;

	if ( CARDLG_AUTHORING_PROTO_OUTLINE ) {
		/* carDlgSegs_da: already fitted to protoDim, real-world, flip applied. */
		wIndex_t cnt = carDlgSegs_da.cnt;
		*segCntP = cnt;
		owned = (trkSeg_p)memdup( &carDlgSegs(0), cnt * sizeof carDlgSegs(0) );
		return owned;
	}

	if ( carProtoSegCnt > 0 ) {
		DIST_T ratio = GetScaleRatio( carDlgScaleInx );
		wIndex_t cnt = carProtoSegCnt;
		owned = (trkSeg_p)memdup( carProtoSegPtr, cnt * sizeof *carProtoSegPtr );
		RescaleSegs( cnt, owned, ratio, ratio, ratio );
		if ( carDlgFlipToggle ) {
			coOrd pos;
			pos.x = protoDim->carLength / 2.0;
			pos.y = protoDim->carWidth  / 2.0;
			RotateSegs( cnt, owned, pos, 180.0 );
		}
		*segCntP = cnt;
		return owned;
	} else {
		int dumCnt;
		trkSeg_p dumPtr;
		CarProtoDlgCreateDummyOutline( &dumCnt, &dumPtr,
		                               IsLocoType( typeVal ),
		                               protoDim->carLength, protoDim->carWidth,
		                               carDlgBodyColor );
		*segCntP = dumCnt;
		owned = (trkSeg_p)memdup( dumPtr, dumCnt * sizeof *dumPtr );
		return owned;
	}
}

/* Thin persistence hook passed to CommitStaged: forwards to CarDlgSaveCustom.
 * The void* ctx is unused (CarDlgSaveCustom reads globals). */
static BOOL_T CarDlgCommitSave( void *ctx )
{
	(void)ctx;
	return CarDlgSaveCustom();
}

static void CarDlgOk( void * unused )
{
	long options = 0;
	tabString_t tabs[7];
	unsigned resultIdent;
	commitPlan_t plan;
	commitResult_t result;
	BOOL_T reloadRoadnameList = FALSE;
	BOOL_T ok;

	LOG( log_carDlgState, 3, ( "CarDlgOk()\n" ) )

	FormUpdate(&carDlgPG);

	/* Nested "New catalog model" commit (carDlgStkPtr>0, currState==
	 * S_PartnoSel): its Ok button reads "Add/Update Car" (CarDlgSetTitleAndOk)
	 * because it now finishes the whole car in this one press, not just the
	 * catalog model. Pop back to the car level up front so every check/branch
	 * below this point (all keyed on currState/CUR_LEVEL/S_ITEM) runs exactly
	 * as it already does for an ordinary "Add Car" commit with an unresolved
	 * manuf/partno -- the still-unmatched fields entered in the nested panel
	 * auto-create the catalog part (and prototype, if that's unresolved too)
	 * via the same plan.makePart/plan.makeProto logic below, and plan.makeItems
	 * (only ever set in the S_ITEM branch) finally creates the car itself,
	 * which the old nested S_PART-only commit never did. */
	if ( currState == S_PartnoSel && carDlgStkPtr > 0 ) {
		carDlgStkPtr--;
		currState = S_ItemSel;
	}

	if ( CarValidateDims( &carDlgDim ) != 0 ) {
		return;   /* shouldn't get here: Ok is disabled while dims are invalid */
	}

	if (S_ITEM && carDlgUpdateItemPtr == NULL &&
	    !CheckCarDlgItemIndex(&carDlgItemIndex)) {
		NoticeMessage(MSG_CARITEM_BAD_INDEX, _("Ok"), NULL);
		FormLoadSingleControl(&carDlgPG, I_CD_ITEMINDEX);
		return;
	}

	if (S_ITEM && (carDlgPLs[I_CD_PURDAT].bInvalid ||
	               carDlgPLs[I_CD_SRVDAT].bInvalid ||
	               carDlgPLs[I_CD_PURPRC].bInvalid ||
	               carDlgPLs[I_CD_CURPRC].bInvalid)) {
		return;
	}

	resultIdent = CarValidateIdentity(CUR_LEVEL, resCatalog, resProto,
	                                  carDlgManufStr, carDlgProtoStr, carDlgPartnoStr);
	for (int i = 0; i < COUNT(carIdentFieldErr); i++) {
		wBool_t bad = (resultIdent & carIdentFieldErr[i].bits) != 0;
		FormErrorState(&carDlgPLs[carIdentFieldErr[i].control], !bad,
		               carIdentFieldErr[i].reason);
	}
	if (resultIdent != 0) {
		return;
	}

	if ( (!S_PROTO) && carDlgCouplerMount != 0 ) {
		options |= CAR_DESC_COUPLER_MODE_BODY;
	}

	/* ---- duplicate-name prompts (per state, may abort) ------------------ */
	if ( S_ITEM &&
	     ( carDlgManufInx < 0 || carDlgPartnoInx < 0 ) && carDlgPartnoStr[0] ) {
		carPart_p dup = CarPartFind( carDlgManufStr, (int)strlen(carDlgManufStr),
		                             carDlgPartnoStr, (int)strlen(carDlgPartnoStr),
		                             carDlgScaleInx );
		if ( dup != NULL &&
		     NoticeMessage( MSG_CARPART_DUPNAME, _("Yes"), _("No") ) <= 0 ) {
			return;
		}
	}
	if ( S_PART && carDlgUpdatePartPtr == NULL ) {
		carPart_p dup = CarPartFind( carDlgManufStr, (int)strlen(carDlgManufStr),
		                             carDlgPartnoStr, (int)strlen(carDlgPartnoStr),
		                             carDlgScaleInx );
		if ( dup != NULL &&
		     NoticeMessage( MSG_CARPART_DUPNAME, _("Yes"), _("No") ) <= 0 ) {
			return;
		}
	}
	if ( S_PROTO && carDlgUpdateProtoPtr == NULL ) {
		carProto_p dup = CarProtoFind( carDlgProtoStr );
		if ( dup != NULL &&
		     NoticeMessage( MSG_CARPROTO_DUPNAME, _("Yes"), _("No") ) <= 0 ) {
			return;
		}
	}

	/* A catalog part being created/edited (directly at S_PART, or via an
	 * S_ITEM commit that auto-creates one -- including the nested Model
	 * commit popped above) carries no repmark for an "undecorated" roadname. */
	if ( ( S_PART || ( S_ITEM && ( carDlgManufInx < 0 || carDlgPartnoInx < 0 ) &&
	                   carDlgPartnoStr[0] ) ) &&
	     strcasecmp( carDlgRoadnameStr, "undecorated" ) == 0 ) {
		carDlgRoadnameStr[0] = '\0';
		carDlgRepmarkStr[0] = '\0';
	}

	/* ---- build the commit plan ------------------------------------------ */
	memset( &plan, 0, sizeof plan );
	plan.scaleInx   = carDlgScaleInx;
	plan.options    = options;
	plan.type       = typeListMap[carDlgTypeInx].value;
	plan.dim        = carDlgDim;
	plan.color      = carDlgBodyColor;
	plan.manuf      = carDlgManufStr;
	plan.protoName  = carDlgProtoStr;
	plan.descField  = carDlgDescStr;
	plan.partno     = carDlgPartnoStr;
	plan.roadname   = carDlgRoadnameStr;
	plan.repmark    = carDlgRepmarkStr;
	plan.number     = carDlgNumberStr;

	/* Which objects does this press create/edit? */
	if ( S_ITEM ) {
		/* A car, plus an auto-created part/proto when their fields are new. */
		plan.makeItems     = TRUE;
		plan.updateItemPtr = carDlgUpdateItemPtr;
		plan.quantity      = ( carDlgUpdateItemPtr != NULL ) ? 1 : carDlgQuantity;
		plan.itemIndexStart= carDlgItemIndex;
		plan.autoNumber    = ( carDlgUpdateItemPtr == NULL &&
		                       carDlgQuantity > 1 && carDlgMultiNum == 1 );
		plan.purchPrice    = carDlgPurchPrice;
		plan.currPrice     = carDlgCurrPrice;
		plan.condition     = carDlgCondition;
		plan.purchDate     = carDlgPurchDate;
		plan.serviceDate   = carDlgServiceDate;
		plan.notesText     = CarDlgExtractNotes();   /* owned; freed below */
		/* auto-create catalog part when manuf/partno are unresolved */
		plan.makePart = ( ( carDlgManufInx < 0 || carDlgPartnoInx < 0 ) &&
		                  carDlgPartnoStr[0] );
		plan.updatePartPtr = NULL;
		/* auto-create prototype when the proto field is unresolved+typed */
		plan.makeProto = ( carDlgProtoInx < 0 && carDlgProtoStr[0] &&
		                   CarProtoFind( carDlgProtoStr ) == NULL );
		plan.updateProtoPtr = NULL;
	} else if ( S_PART ) {
		plan.makePart      = TRUE;
		plan.updatePartPtr = carDlgUpdatePartPtr;
		plan.makeProto = ( carDlgProtoInx < 0 && carDlgProtoStr[0] &&
		                   carDlgUpdatePartPtr == NULL &&
		                   CarProtoFind( carDlgProtoStr ) == NULL );
		plan.updateProtoPtr = NULL;
	} else if ( S_PROTO ) {
		plan.makeProto      = TRUE;
		plan.updateProtoPtr = carDlgUpdateProtoPtr;
		plan.protoName      = carDlgProtoStr;
	}

	/* Prototype dims + owned outline, when creating one. protoDim is real
	 * world; S_PROTO authors directly in real world (dim already is), the
	 * car/part paths scale the model-scale dim up. */
	if ( plan.makeProto ) {
		if ( S_PROTO ) {
			plan.protoDim = carDlgDim;
		} else {
			plan.protoDim = SeedProtoDimsFromCatalog( carDlgDim, carDlgScaleInx );
		}
		plan.protoSegPtr = CarDlgBuildProtoSegs( &plan.protoDim, plan.type,
		                   &plan.protoSegCnt );
	}

	/* ---- commit --------------------------------------------------------- *
	 * Persist the custom parts/proto file only when this commit actually
	 * creates or edits a custom object (proto or part). A car-only add
	 * (existing part, no new proto) touches no custom params -- passing a
	 * NULL save hook skips the rewrite, matching the old needCustomSave
	 * gate. The roster car itself is layout data, persisted via
	 * SetFileChanged below, not by the custom-file save. */
	{
		commitSaveFn_t saveHook =
		        ( plan.makeProto || plan.makePart ) ? CarDlgCommitSave : NULL;
		ok = CommitStaged( &plan, saveHook, NULL, &result );
	}

	if ( plan.notesText != NULL ) {
		MyFree( (void *)plan.notesText );
	}

	if ( !ok ) {
		/* CommitStaged rolled everything back and did not persist (the only
		 * realistic cause is OpenCustom failing in the save hook). The old
		 * code ignored save failures silently and left objects created; the
		 * atomic path instead creates nothing. Leave the dialog open with no
		 * success message so the user can retry; no dedicated failure message
		 * constant exists, and inventing one is out of scope for this change. */
		return;
	}

	/* ---- post-commit: message, roadname reload, notifications ----------- */
	reloadRoadnameList = ( S_ITEM || S_PART );

	if ( S_ITEM ) {
		SetFileChanged();   /* the roster car is layout data */
		if ( carDlgUpdateItemPtr == NULL ) {
			/* index advanced by CommitStaged; persist last + reload control */
			wPrefSetInteger( "misc", "last-car-item-index", result.nextItemIndex - 1 );
			carDlgItemIndex = result.nextItemIndex;
			CheckCarDlgItemIndex( &carDlgItemIndex );
			FormLoadSingleControl( &carDlgPG, I_CD_ITEMINDEX );
			if ( carDlgQuantity > 1 ) {
				sprintf( message, _("Added %ld new Cars"), carDlgQuantity );
			} else {
				strcpy( message, _("Added new Car") );
			}
		} else {
			strcpy( message, _("Updated Car") );
		}
		sprintf( message+strlen(message), "%s: %s %s %s %s %s %s",
		         (result.partP?_(" and Part"):""),
		         carDlgManufStr, carDlgPartnoStr, carDlgProtoStr, carDlgDescStr,
		         (carDlgRepmarkStr[0]?carDlgRepmarkStr:carDlgRoadnameStr), carDlgNumberStr );
		carDlgQuantity = 1;
		FormLoadSingleControl( &carDlgPG, I_CD_QTY );
	} else if ( S_PART ) {
		carDlgNewPartPtr = result.partP;
		sprintf( message, _("%s Part: %s %s %s %s %s %s"),
		         carDlgUpdatePartPtr==NULL?_("Added new"):_("Updated"), carDlgManufStr,
		         carDlgPartnoStr, carDlgProtoStr, carDlgDescStr,
		         carDlgRepmarkStr[0]?carDlgRepmarkStr:carDlgRoadnameStr, carDlgNumberStr );
	} else if ( S_PROTO ) {
		carDlgNewProtoPtr = result.protoP;
		sprintf( message, _("%s Prototype: %s%s."),
		         carDlgUpdateProtoPtr==NULL?_("Added new"):_("Updated"), carDlgProtoStr,
		         carDlgUpdateProtoPtr==NULL?_(". Enter new values or press Close"):"" );
	}

	if ( reloadRoadnameList ) {
		tabs[0].ptr = carDlgRoadnameStr;
		tabs[0].len = (int)strlen(carDlgRoadnameStr);
		tabs[1].ptr = carDlgRepmarkStr;
		tabs[1].len = (int)strlen(carDlgRepmarkStr);
		LoadRoadnameList( &tabs[0], &tabs[1] );
		CarDlgLoadRoadnameList();
		FormLoadSingleControl( &carDlgPG, I_CD_ROADNAME_LIST );
	}

	FormLoadMessage( &carDlgPG, I_CD_MSG, message );
	DoChangeNotification( CHANGE_PARAMS );
	carDlgChanged = 0;

	/* ---- per-state "stay open for next entry" tail (early returns) ------ */
	if ( S_ITEM ) {
		if ( carDlgUpdateItemPtr == NULL ) {
			if ( result.partP ) {
				char title[STR_LONG_SIZE*2];
				sprintf( title, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr,
				         carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr,
				         carDlgNumberStr );
				TabStringExtract( title, 7, tabs );
				currState = S_ItemSel;
				resCatalog = CarDlgLoadLists( TRUE, tabs, GetLayoutCurScale()) ?
				             RS_RESOLVED : RS_NEW;
				resItemManuf = resCatalog;
				FormLoadSingleControl( &carDlgPG, I_CD_MANUF_LIST );
				FormLoadSingleControl( &carDlgPG, I_CD_PROTOKIND_LIST );
				FormLoadSingleControl( &carDlgPG, I_CD_PROTOTYPE_LIST );
				FormLoadSingleControl( &carDlgPG, I_CD_PARTNO_LIST );
				FormLoadSingleControl( &carDlgPG, I_CD_DESC_STR );
				FormControlShow( &carDlgPG, I_CD_DESC_STR, carDlgPartnoInx<0 );
			} else if ( carDlgManufInx == -1 ) {
				carDlgManufStr[0] = '\0';
			}
			return;
		}
	} else if ( S_PART ) {
		if ( carDlgUpdatePartPtr == NULL ) {
			SetNextPartno(carDlgPartnoStr);
			wListSetValue( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoStr );
			carDlgNumberStr[0] = '\0';
			FormLoadSingleControl( &carDlgPG, I_CD_NUMBER );
			return;
		}
	} else if ( S_PROTO ) {
		if ( carDlgUpdateProtoPtr == NULL ) {
			carDlgProtoStr[0] = '\0';
			FormLoadSingleControl( &carDlgPG, I_CD_PROTOTYPE_LIST );
			return;
		}
	}
	CarDlgClose( carDlgPG.win );
}



#ifdef TODO_UNUSED
static void CarDlgLayout(
        paramData_t * pd,
        int inx,
        wWinPix_t currX,
        wWinPix_t *xx,
        wWinPix_t *yy )
{
	static wWinPix_t col2pos = 0;
	wWinPix_t y0, y1;

	switch (inx) {
	case I_CD_PROTOTYPE_STR:
	case I_CD_PARTNO_STR:
	case I_CD_ISLOCO:
	case I_CD_IMPORT:
	case I_CD_TYPE_LIST:
		*yy = wControlGetPosY(carDlgPLs[inx-1].control);
		break;
	case I_CD_NEWPROTO:
		*yy = wControlGetPosY(carDlgPLs[I_CD_NEW].control);
		break;
	case I_CD_CPLRLEN:
	case I_CD_CARWIDTH:
		if ( col2pos == 0 ) {
			col2pos = wLabelWidth( _("Coupler Length") )+20;
		}
		*xx = wControlBeside(carDlgPLs[inx-1].control) + col2pos;
		break;
	case I_CD_DESC_STR:
		*yy = wControlBelow(carDlgPLs[I_CD_PARTNO_STR].control) + 3;
		break;
	case I_CD_CPLRMNT:
		*yy = wControlBelow(carDlgPLs[I_CD_TRKOFFSET].control) + 3;
		break;
	case I_CD_CPLDLEN:
		*yy = wControlBelow(carDlgPLs[I_CD_CPLRMNT].control) + 3;
		break;
	case I_CD_CANVAS:
		*yy = wControlBelow(carDlgPLs[I_CD_CPLDLEN].control)+5;
		break;
	case I_CD_ITEMINDEX:
		*yy = wControlGetPosY(carDlgPLs[I_CD_ROADNAME_LIST].control);
		break;
	case I_CD_MSG:
		y0 = wControlBelow(carDlgPLs[I_CD_ITEMINDEX-1].control);
		y1 = wControlBelow(carDlgPLs[I_CD_MSG-1].control);
		*yy = ((y0>y1)?y0:y1) + 10;
		break;
	}
}
#endif


void DoCarPartDlg( void )
{
	paramData_t * pd;

	if ( carDlgPG.win == NULL ) {
		FormCreateDialog( &carDlgPG,
		                  MakeWindowTitle(_("New Car Part")),
		                  _("Add"), CarDlgOk,
		                  NULL, FormCancel_Custom( CarDlgCancel ),
		                  TRUE,
		                  F_BLOCK|F_RESIZE|F_RECALLSIZE|PD_F_ALT_CANCELLABEL,
		                  CarDlgUpdate );

		if ( carDlgDim.carWidth==0 ) {
			carDlgDim.carWidth = 12.0*10.0/curScaleRatio;
		}

		for ( pd=carDlgPG.paramPtr; pd<&carDlgPG.paramPtr[carDlgPG.paramCnt]; pd++ ) {
			if ( pd->type == PD_FLOAT && pd->valueP ) {
				sprintf( message, "%s-%s", pd->nameStr, curScaleName );
				wPrefGetFloat( carDlgPG.nameStr, message, (FLOAT_T*)pd->valueP,
				               *(FLOAT_T*)pd->valueP );
			}
		}
		roadnameMapChanged = TRUE;

		for ( int inx=0; inx<N_CONDLISTMAP; inx++ ) {
			wComboBoxAddValue( carDlgPLs[I_CD_COND].control, _(condListMap[inx].name),
			                   I2VP(condListMap[inx].value) );
		}

		for ( int inx=0; inx<N_TYPELISTMAP; inx++ ) {
			wComboBoxAddValue( carDlgPLs[I_CD_PROTOKIND_LIST].control,
			                   _(typeListMap[inx].name), I2VP(typeListMap[inx].value) );
		}

		wTextSetReadonly( carDlgPLs[I_CD_NOTES].control, FALSE );
	}

	wPrefGetInteger( "misc", "last-car-item-index", &carDlgItemIndex, 1 );
	CheckCarDlgItemIndex(&carDlgItemIndex);
	CarDlgLoadRoadnameList();
	carProtoSegCnt = 0;
	carProtoSegPtr = NULL;
	carDlgScaleInx = GetLayoutCurScale();
	carDlgFlipToggle = FALSE;
	carDlgChanged = 0;
	carDlgRepmarkTouched = FALSE;
	/* reveal state is per-session: clear all user pins and the resolution
	 * edge detector so each open starts from the program defaults (R3/R4). */
	geomPinned = ownPinned = identPinned = FALSE;
	resCatalogPrev = RS_UNRESOLVED;
	for ( paramData_p p=carDlgPLs; p < carDlgPLs + COUNT( carDlgPLs ); p++ ) {
		p->bInvalid = FALSE;
	}

	/* the "-new" hint labels are only ever turned on by the *FocusOut
	 * handlers below, in response to this session's own typing -- a value
	 * loaded here from a previous car/model/prototype is by definition
	 * already resolved, so start every dialog session with them hidden
	 * rather than carrying over whatever a prior session last set (the
	 * widgets are reused across opens, not recreated). */
	FormControlShow( &carDlgPG, I_CD_MANUF_NEW, FALSE );
	FormControlShow( &carDlgPG, I_CD_PROTOTYPE_NEW, FALSE );
	FormControlShow( &carDlgPG, I_CD_PARTNO_NEW, FALSE );
}

EXPORT void CarDlgAddProto( void )
{
	carDlgTypeInx = 0;
	carDlgUpdateProtoPtr = NULL;
	DoCarPartDlg();

	/* T_InitProto/T_InitProtoUpd: CarDlgActInitProto() itself already sets
	 * currState=S_ProtoSel for the update case; A_SProtoSel did it
	 * unconditionally for both, so it's set explicitly here too.
	 *
	 * carDlgResolving guards the whole init sequence: the list-repopulate
	 * calls below (CarDlgLoadProtoKindListFull/FormLoadSingleControl) clear
	 * and refill combo boxes, which fires a synchronous GTK "changed" signal
	 * re-entering CarDlgUpdate() while currState/carDlg*Inx still hold
	 * whatever was left over from the dialog's *previous* use -- without the
	 * guard that reentrant call can run a mismatched-context path (e.g. read
	 * a stale combo-box item context as the wrong pointer type) and crash or
	 * corrupt the lists being built here. Same guard already used by
	 * CarDlgNewDesc/CarDlgClose. */
	carDlgResolving = TRUE;
	CarDlgActInitProto();
	currState = S_ProtoSel;
	CarDlgLoadProtoKindListFull();
	FormLoadSingleControl( &carDlgPG, I_CD_PROTOKIND_LIST );
	CarDlgShowControls();
	/* see the matching comment in CarDlgActInitProto -- FormLoadControls()
	 * inside CarDlgShowControls() may have just resynced this has-entry
	 * combo's text to some stale row; force the real value back in. */
	wListSetValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
	               carDlgProtoStr );
	carDlgResolving = FALSE;
	/* Several of the FormLoadControls()/FormLoadSingleControl() calls above
	 * touch widgets (dims, coupler length, ...) whose "changed" handlers
	 * bump carDlgChanged unconditionally (see careditdlg.c's dimension
	 * cases), not gated by carDlgResolving like the combo cases are --
	 * without this, a freshly-opened Add Prototype dialog already looks
	 * "changed" and Cancel needs an extra click through the discard-changes
	 * prompt before it actually closes. Same fix already applied in
	 * CarDlgNewDesc(). */
	carDlgChanged = 0;

	CarDlgUpdate( &carDlgPG, -1,
	              NULL );  /* settled pass: fill summaries+disclosure at open */
	wShow(carDlgPG.win);
}

void CarDlgUpdProto(void)
{
	DoCarPartDlg();

	carDlgResolving = TRUE;
	CarDlgActInitProto();
	currState = S_ProtoSel;
	CarDlgLoadProtoKindListFull();
	FormLoadSingleControl( &carDlgPG, I_CD_PROTOKIND_LIST );
	CarDlgShowControls();
	/* see the matching comment in CarDlgActInitProto -- FormLoadControls()
	 * inside CarDlgShowControls() may have just resynced this has-entry
	 * combo's text to some stale row; force the real value back in. */
	wListSetValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
	               carDlgProtoStr );
	carDlgResolving = FALSE;

	CarDlgUpdate( &carDlgPG, -1,
	              NULL );  /* settled pass: fill summaries+disclosure at open */
	wShow(carDlgPG.win);
}

void CarDlgUpdPart(void)
{
	DoCarPartDlg();

	carDlgResolving = TRUE;
	CarDlgActLoadDataFromUpdatePart();
	currState = S_PartnoSel;
	resModelManuf = RS_RESOLVED;
	CarDlgShowControls();
	carDlgResolving = FALSE;

	CarDlgUpdate( &carDlgPG, -1,
	              NULL );  /* settled pass: fill summaries+disclosure at open */
	wShow(carDlgPG.win);
}

EXPORT void CarDlgAddDesc( void )
{
	if ( carProto_da.cnt <= 0 ) {
		NoticeMessage( MSG_NO_CARPROTO, _("Ok"), NULL );
		return;
	}
	carDlgUpdatePartPtr = NULL;
	carDlgNumberStr[0] = '\0';
	FormLoadSingleControl( &carDlgPG, I_CD_NUMBER );

	DoCarPartDlg();

	carDlgResolving = TRUE;
	CarDlgRecallCouplerLength();
	/* bInclCustomUnknown=FALSE: see the matching comment in CarDlgNewDesc --
	 * a directly-created Catalog Model always needs a real manufacturer. */
	CarDlgLoadManufList( TRUE, FALSE, carDlgScaleInx );
	FormLoadSingleControl( &carDlgPG, I_CD_MANUF_LIST );
	CarDlgActLoadProtoListAll();
	CarDlgActClrPartnoStr();
	CarDlgActClrNumberStr();
	currState = S_PartnoSel;
	resModelManuf = RS_RESOLVED;
	CarDlgActLoadDimsFromProtoList();
	CarDlgShowControls();
	carDlgResolving = FALSE;
	/* see the matching comment in CarDlgAddProto -- same unconditional
	 * carDlgChanged++ in the dimension-field cases, same fix. */
	carDlgChanged = 0;
	CarDlgRedraw( carDlgD.d, NULL, 0, 0 );

	CarDlgUpdate( &carDlgPG, -1,
	              NULL );  /* settled pass: fill summaries+disclosure at open */
	wShow(carDlgPG.win);
}

EXPORT void CarDlgAddItem(void)
{
	DoCarPartDlg();

	carDlgResolving = TRUE;
	CarDlgRecallCouplerLength();
	currState = S_ItemSel;
	resCatalog = CarDlgLoadLists( TRUE, NULL,
	                              carDlgScaleInx ) ? RS_RESOLVED : RS_NEW;
	resItemManuf = resCatalog;
	if ( resCatalog == RS_RESOLVED ) {
		CarDlgActLoadDataFromPartList();
	} else {
		CarDlgActLoadDimsFromProtoList();
		CarDlgActClrPartnoStr();
		CarDlgActClrNumberStr();
	}
	CarDlgShowControls();
	carDlgResolving = FALSE;
	/* see the matching comment in CarDlgAddProto -- same unconditional
	 * carDlgChanged++ in the dimension-field cases, same fix. Left stale
	 * here, this also poisons CarDlgPushDims()'s saved carDlgChanged when
	 * the user immediately opens the nested "New Catalog Model" panel,
	 * making Cancel's pop-back-to-car-level land on a spurious "changed"
	 * state and need a second Cancel through the discard prompt. */
	carDlgChanged = 0;

	CarDlgUpdate( &carDlgPG, -1,
	              NULL );  /* settled pass: fill summaries+disclosure at open */
	wShow(carDlgPG.win);
}


EXPORT void CarDlgUpdItem(void)
{
	DoCarPartDlg();

	carDlgResolving = TRUE;
	CarDlgActLoadInfoFromUpdateItem();
	CarDlgShowControls();
	carDlgResolving = FALSE;

	CarDlgUpdate( &carDlgPG, -1,
	              NULL );  /* settled pass: fill summaries+disclosure at open */
	wShow(carDlgPG.win);
}


static void CarDlgChange( long changes )
{
	if ( (changes&CHANGE_SCALE) ) {
		carDlgCouplerLength = 0.0;
	}
}

void
InitCarEditDlg()
{
	carDlgBodyColor = wDrawFindColor( wRGB(255,128,0) );
	FormRegister( &carDlgPG );

	RegisterChangeNotification( CarDlgChange );

	currState = S_Error;
	resCatalog = RS_UNRESOLVED;
	resProto = RS_UNRESOLVED;

	log_carDlgState = LogFindIndex( "carDlgState" );
	log_carDlgEdit = LogFindIndex("carDlgEdit");
}
