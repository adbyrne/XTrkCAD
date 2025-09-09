/** \file dcar.c
 * Car Management
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

#include "cselect.h"
#include "ctrain.h"
#include "custom.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "include/paramfile.h"
#include "common-ui.h"

#include "include/cars.h"
#include "listelem.h"
#include "tabstring.h"
//#include "transform_pts.h"
#include "carsprivate.h"


static int log_carInvList;
static int log_carDlgState = 10;
static int log_carDlgList;
int log_carDlgDims;
extern int log_carList;


//static paramFloatRange_t r0_99999 = { 0, 99999, 80 };
static paramFloatRange_t r0d001_99999 = { 0.001, 99999, 80 };
static paramFloatRange_t r9999_9999 = {-99999, 99999, 80};
static paramIntegerRange_t i1_999999999 = { 1, 999999999, 80, PDO_NORANGECHECK_HIGH };
static paramIntegerRange_t i1_9999 = { 1, 9999, 50 };
static char * isLocoLabels[] = { "", 0 };
static char * cplrModeLabels[] = { N_("Truck"), N_("Body"), 0 };



extern dynArr_t carItemInfo_da;
#define carItemInfo(N) DYNARR_N( carItem_t*, carItemInfo_da, N )
dynArr_t carItemHotbar_da;
#define carItemHotbar(N)			DYNARR_N( carItem_p, carItemHotbar_da, N )
static char* CarItemHotbarProc(hotBarProc_e op, void* data, drawCmd_p d, coOrd* origP);

static char* FormatCarTitle(carItem_p item, long mode);
//typedef struct {
//	char * name;
//	long value;
//} nameLongMap_t;

extern trkSeg_p carProtoSegPtr;
extern int carProtoSegCnt;

extern BOOL roadnameMapChanged;

EXPORT BOOL_T CarCustomSave(
        FILE * f )
{
	BOOL_T rc = TRUE;
	rc &= CarProtoCustomSave( f );
	rc &= CarDescCustomSave( f );
	return rc;
}


/*
 * Car Item Select
 */

EXPORT carItem_p currCarItemPtr;
EXPORT long carHotbarModeInx = 1;
static long carHotbarModes[] = { 0x0002, 0x0012, 0x0312, 0x4312, 0x0021, 0x0321, 0x4321 };
static long carHotbarContents[] = { 0x0005, 0x0002, 0x0012, 0x0012, 0x0001, 0x0021, 0x0021 };



static int Cmp_carHotbar(
	const void* ptr1,
	const void* ptr2)
{
	carItem_p item1 = *(carItem_p*)ptr1;
	carItem_p item2 = *(carItem_p*)ptr2;
	tabString_t tabs1[7], tabs2[7];
	int rc;
	long mode;

	TabStringExtract(item1->title, 7, tabs1);
	TabStringExtract(item2->title, 7, tabs2);
	for (mode = carHotbarModes[carHotbarModeInx], rc = 0; mode != 0 && rc == 0; mode >>= 4) {
		switch (mode & 0x000F) {
		case 4:
			rc = (int)(item1->index - item2->index);
			break;
		case 1:
			rc = strncasecmp(tabs1[T_MANUF].ptr, tabs2[T_MANUF].ptr,
				max(tabs1[T_MANUF].len, tabs2[T_MANUF].len));
			break;
		case 3:
			rc = strncasecmp(tabs1[T_PART].ptr, tabs2[T_PART].ptr, max(tabs1[T_PART].len,
				tabs2[T_PART].len));
			break;
		case 2:
			if (item1->type < item2->type) {
				rc = -1;
			}
			else if (item1->type > item2->type) {
				rc = 1;
			}
			else {
				rc = strncasecmp(tabs1[T_PROTO].ptr, tabs2[T_PROTO].ptr,
					max(tabs1[T_PROTO].len, tabs2[T_PROTO].len));
			}
			break;
		}
	}
	return rc;
}
EXPORT void AddHotBarCarDesc(void)
{
	wIndex_t inx;
	carItem_t* item0, * item1;
	coOrd orig;
	coOrd size;

	LOG(log_carDlgDims, 1, ("AddHotBarCarDesc/load carItemHB: carItemHB.cnt:%d\n",
		carItemInfo_da.cnt));
	DYNARR_SET(carItem_t*, carItemHotbar_da, carItemInfo_da.cnt);
	memcpy(&carItemHotbar(0), &carItemInfo(0),
		carItemInfo_da.cnt * sizeof carItemHotbar(0));
	qsort(&carItemHotbar(0), carItemHotbar_da.cnt, sizeof carItemHotbar(0),
		Cmp_carHotbar);
	for (inx = 0, item0 = NULL; inx < carItemHotbar_da.cnt; inx++) {
		item1 = carItemHotbar(inx);
		if (item1->car && !IsTrackDeleted(item1->car)) {
			continue;
		}
		if (FIT_NONE == CompatibleScale(FIT_CAR, item1->scaleInx, GetLayoutCurScale())) {
			continue;
		}
		if ((carHotbarModes[carHotbarModeInx] & 0xF000) != 0 || (item0 == NULL
			|| Cmp_carHotbar(&item0, &item1) != 0)) {
#ifdef DESCFIX
			orig.x = -item->orig.x;
			orig.y = -item->orig.y;
#endif
			orig = zero;
			size.x = item1->dim.carLength;
			size.y = item1->dim.carWidth;
			LOG(log_carDlgDims, 1, ("AddHotBarElement( %d: %s\n", inx, item1->title));
			AddHotBarElement(FormatCarTitle(item1, carHotbarContents[carHotbarModeInx]),
				size, orig, FALSE, FALSE, (60.0 * 12.0 / curScaleRatio), I2VP(inx),
				CarItemHotbarProc);
		}
		item0 = item1;
	}
}

static long newCarInx;
static paramData_t newCarPLs[] = {
	{ PD_DROPLIST, &newCarInx, "index", PDO_DLGWIDE, I2VP(400), N_("Item") }
};
static paramGroup_t newCarPG = { "train-newcar", 0, newCarPLs, COUNT(newCarPLs) };
EXPORT wControl_p newCarControls[2];
static char newCarLabel1[STR_SIZE];
static char* newCarLabels[2] = { newCarLabel1, NULL };

void CarUpdateHotbarList()
{
	wWinPix_t w, h;

	/** \todo extract the following code to its own function and put into dcar.c */
	wListClear((wList_p)newCarPLs[0].control);
	for (int inx = 0; inx < carItemHotbar_da.cnt; inx++) {
		carItem_p item;
		char* cp;
		item = carItemHotbar(inx);
		if (item->car && !IsTrackDeleted(item->car)) {
			continue;
		}
		cp = CarItemDescribe(item, 0, NULL);
		wListAddValue((wList_p)newCarPLs[0].control, cp, NULL, I2VP(inx));
	}
	/*wListSetValue( (wList_p)newCarPLs[0].control, "Select a car" );*/
	wListSetIndex((wList_p)newCarPLs[0].control, 0);
	strcpy(newCarLabel1, _("Select"));
	ParamLoadControl(&newCarPG, 0);
	InfoSubstituteControls(newCarControls, newCarLabels);

	wWinGetSize(mainW, &w, &h);
	w -= wControlGetPosX(newCarControls[0]) + 4;
	if (w > 20) {
		wListSetSize((wList_p)newCarControls[0], w,
			wControlGetHeight(newCarControls[0]));
	}
}


static char* CarItemHotbarProc(
	hotBarProc_e op,
	void* data,
	drawCmd_p d,
	coOrd* origP)
{
	wIndex_t carItemInx = (wIndex_t)VP2L(data);
	carItem_p item;
	wIndex_t inx;
	long mode;
	char* cp;
	wWinPix_t w, h;

	CHECK(carItemInx < carItemHotbar_da.cnt);
	item = carItemHotbar(carItemInx);
	if (item == NULL) {
		return NULL;
	}
	switch (op) {
	case HB_SELECT:
		currCarItemPtr = item;
		mode = carHotbarModes[carHotbarModeInx];
		if ((mode & 0xF000) == 0) {
			wListClear((wList_p)newCarPLs[0].control);
			for (inx = carItemInx;
				inx < carItemHotbar_da.cnt && (inx == carItemInx
					|| Cmp_carHotbar(&carItemHotbar(carItemInx), &carItemHotbar(inx)) == 0);
				inx++) {
				item = carItemHotbar(inx);
				if (item->car && !IsTrackDeleted(item->car)) {
					continue;
				}
				cp = CarItemDescribe(item, mode, NULL);
				wListAddValue((wList_p)newCarPLs[0].control, cp, NULL, I2VP(inx));
			}
			/*wListSetValue( (wList_p)newCarPLs[0].control, "Select a car" );*/
			wListSetIndex((wList_p)newCarPLs[0].control, 0);
			cp = CarItemHotbarProc(HB_BARTITLE, I2VP(carItemInx), NULL, NULL);
			strncpy(newCarLabel1, cp, sizeof(newCarLabel1) - 1);
			newCarLabel1[sizeof(newCarLabel1) - 1] = 0;
			ParamLoadControls(&newCarPG);
			ParamGroupRecord(&newCarPG);

			InfoSubstituteControls(newCarControls, newCarLabels);
			wWinGetSize(mainW, &w, &h);
			w -= wControlGetPosX(newCarControls[0]) + 4;
			if (w > 20) {
				wListSetSize((wList_p)newCarControls[0], w,
					wControlGetHeight(newCarControls[0]));
			}
		}
		else {
			InfoSubstituteControls(NULL, NULL);
			cp = CarItemDescribe(item, 0, NULL);
			InfoMessage(cp);
		}
		break;
	case HB_LISTTITLE:
	case HB_BARTITLE:
		return FormatCarTitle(item, carHotbarModes[carHotbarModeInx]);
	case HB_FULLTITLE:
		return item->title;
	case HB_DRAW:
		if (item->segCnt == 0) {
			CarItemGetSegs(item);
		}
		DrawSegs(d, *origP, 0.0, item->segPtr, item->segCnt, trackGauge,
			wDrawColorBlack);
		return NULL;
	}
	return NULL;
}


static void CarItemHotbarUpdate(
	paramGroup_p pg,
	int inx,
	void* data)
{
	wIndex_t carItemInx;
	carItem_p item;
	if (inx == 0) {
		carItemInx = (wIndex_t) * (long*)data;
		if (carItemInx < 0) {
			return;
		}
		carItemInx = (wIndex_t)VP2L(wListGetItemContext((wList_p)
			pg->paramPtr[inx].control, carItemInx));
		CHECK(carItemInx < carItemHotbar_da.cnt);
		item = carItemHotbar(carItemInx);
		if (item != NULL) {
			currCarItemPtr = item;
		}
	}
}



static char * FormatCarTitle(
        carItem_p item,
        long mode )
{
	tabString_t tabs[7];
	char * cp;
	TabStringExtract( item->title, 7, tabs );
	cp = message;
	for ( ; mode!=0; mode>>=4 ) {
		switch ( mode&0x000F ) {
		case 1:
			cp = TabStringCpy( cp, &tabs[T_MANUF] );
			break;
		case 2:
			cp = TabStringCpy( cp, &tabs[T_PROTO] );
			break;
		case 3:
			cp = TabStringCpy( cp, &tabs[T_PART] );
			break;
		case 4:
			sprintf( cp, "%ld ", item->index );
			cp += strlen(cp);
			break;
		case 5:
			strcpy( cp, typeListMap[CarProtoFindTypeCode(item->type)].name );
			cp += strlen(cp);
			break;
		}
		*cp++ = '/';
	}
	*--cp = '\0';
	return message;
}


EXPORT int CarAvailableCount( void )
{
	wIndex_t inx;
	int cnt = 0;
	carItem_t * item;
	for ( inx=0; inx < carItemHotbar_da.cnt; inx ++ ) {
		item = carItemHotbar(inx);
		if (FIT_NONE == CompatibleScale( FIT_CAR, item->scaleInx,
		                                 GetLayoutCurScale())) {
			continue;
		}
		cnt++;
	}
	return cnt;
}




EXPORT BOOL_T StoreCarItem (carItem_p item, void **data,long *len)
{

	*data = item;
	*len = sizeof (carItem_t);
	return TRUE;

}

EXPORT BOOL_T ReplayCarItem(carItem_p item, void *data,long len)
{


	item->pos = ((carItem_t *)data)->pos;
	item->angle = ((carItem_t *)data)->angle;
	return TRUE;

}
EXPORT void CarItemUpdate(
        carItem_p item )
{
	DoChangeNotification( CHANGE_SCALE );
}

/*
 * Car Item/Part Dlg
 */

int carDlgChanged;

SCALEINX_T carDlgScaleInx;
carItem_p carDlgUpdateItemPtr;
static carPart_p carDlgUpdatePartPtr;
static carProto_p carDlgUpdateProtoPtr;
static carPart_p carDlgNewPartPtr;
static carProto_p carDlgNewProtoPtr;

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

static long carDlgDispMode;
static wIndex_t carDlgRoadnameInx;
static char carDlgRoadnameStr[STR_SIZE];
static char carDlgRepmarkStr[STR_SIZE];
static char carDlgNumberStr[STR_SIZE];
static wDrawColor carDlgBodyColor;
static long carDlgIsLoco;
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

static char *dispmodeLabels[] = { N_("Information"), N_("Customize"), NULL };
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
static void CarDlgNewProto( void );
static void CarDlgUpdate( paramGroup_p, int, void * );
static void CarDlgNewDesc( void );
static void CarDlgNewProto( void );

static paramData_t carDlgPLs[] = {
#define A                       (0)
#define I_CD_MANUF_LIST         (A+0)
	{ PD_DROPLIST, &carDlgManufInx, "manuf", PDO_NOPREF, I2VP(350), N_("Manufacturer"), BL_EDITABLE },
#define I_CD_PROTOTYPE_STR      (A+1)
	{ PD_STRING, &carDlgProtoStr, "prototype", PDO_NOPREF|PDO_NOTBLANK, I2VP(350), N_("Prototype"), 0, 0, sizeof(carDlgProtoStr)},
#define I_CD_PROTOKIND_LIST     (A+2)
	{ PD_DROPLIST, &carDlgKindInx, "protokind-list", PDO_NOPREF, I2VP(125), N_("Prototype"), 0 },
#define I_CD_PROTOTYPE_LIST     (A+3)
	{ PD_DROPLIST, &carDlgProtoInx, "prototype-list", PDO_NOPREF|PDO_DLGHORZ, I2VP(225-3), NULL, 0 },
#define I_CD_TYPE_LIST          (A+4)
	{ PD_DROPLIST, &carDlgTypeInx, "type", PDO_NOPREF, I2VP(350), N_("Type"), 0 },
#define I_CD_PARTNO_LIST        (A+5)
	{ PD_DROPLIST, &carDlgPartnoInx, "partno-list", PDO_NOPREF, I2VP(350), N_("Part"), BL_EDITABLE },
#define I_CD_PARTNO_STR         (A+6)
	{ PD_STRING, &carDlgPartnoStr, "partno", PDO_NOPREF|PDO_NOTBLANK, I2VP(350), N_("Part Number"), 0, 0, sizeof(carDlgPartnoStr)},
#define I_CD_ISLOCO             (A+7)
	{ PD_TOGGLE, &carDlgIsLoco, "isLoco", PDO_NOPREF|PDO_DLGWIDE, isLocoLabels, N_("Loco?"), BC_HORZ|BC_NOBORDER },
#define I_CD_DESC_STR           (A+8)
	{ PD_STRING, &carDlgDescStr, "desc", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(350), N_("Description"), 0, 0, sizeof(carDlgDescStr)},
#define I_CD_IMPORT             (A+9)
	{ PD_BUTTON, NULL, "import", 0, 0, N_("Import") },
#define I_CD_RESET              (A+10)
	{ PD_BUTTON, NULL, "reset", PDO_DLGHORZ, 0, N_("Reset") },
#define I_CD_FLIP               (A+11)
	{ PD_BUTTON, NULL, "flip", PDO_DLGHORZ|PDO_DLGWIDE|PDO_DLGBOXEND, 0, N_("Flip") },

#define I_CD_DISPMODE           (A+12)
	{ PD_RADIO, &carDlgDispMode, "dispmode", PDO_NOPREF|PDO_DLGWIDE, dispmodeLabels, N_("Mode"), BC_HORZ|BC_NOBORDER },

#define B                       (A+13)
#define I_CD_ROADNAME_LIST      (B+0)
	{ PD_DROPLIST, &carDlgRoadnameInx, "road", PDO_NOPREF|PDO_DLGWIDE, I2VP(350), N_("Road"), BL_EDITABLE },
#define I_CD_REPMARK            (B+1)
	{ PD_STRING, carDlgRepmarkStr, "repmark", PDO_NOPREF|PDO_STRINGLIMITLENGTH, I2VP(60), N_("Reporting Mark"), 0, 0, sizeof(carDlgRepmarkStr)},
#define I_CD_NUMBER             (B+2)
	{ PD_STRING, carDlgNumberStr, "number", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ|PDO_STRINGLIMITLENGTH, I2VP(80), N_("Number"), 0, 0, sizeof(carDlgNumberStr)},
#define I_CD_BODYCOLOR          (B+3)
	{ PD_COLORLIST, &carDlgBodyColor, "bodyColor", PDO_DLGWIDE|PDO_DLGHORZ, NULL, N_("Color") },
#define I_CD_CARLENGTH          (B+4)
	{ PD_FLOAT, &carDlgDim.carLength, "carLength", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE, &r0d001_99999, N_("Car Length") },
#define I_CD_CARWIDTH           (B+5)
	{ PD_FLOAT, &carDlgDim.carWidth, "carWidth", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, &r0d001_99999, N_("Width") },
#define I_CD_TRKCENTER          (B+6)
	{ PD_FLOAT, &carDlgDim.truckCenter, "trkCenter", PDO_DIM|PDO_NOPREF, &r0d001_99999, N_("Truck Centers") },
#define I_CD_TRKOFFSET			(B+7)
	{ PD_FLOAT, &carDlgDim.truckCenterOffset, "trkCenterOffset", PDO_DIM|PDO_NOPREF|PDO_DLGHORZ|PDO_DLGWIDE, &r9999_9999, N_("Center Offset") },
#define I_CD_CPLRMNT            (B+8)
	{ PD_RADIO, &carDlgCouplerMount, "cplrMount", PDO_NOPREF, cplrModeLabels, N_("Coupler Mount"), BC_HORZ|BC_NOBORDER },
#define I_CD_CPLDLEN            (B+9)
	{ PD_FLOAT, &carDlgDim.coupledLength, "cpldLen", PDO_DIM|PDO_NOPREF, &r0d001_99999, N_("Coupled Length") },
#define I_CD_CPLRLEN            (B+10)
	{ PD_FLOAT, &carDlgCouplerLength, "cplrLen", PDO_DIM|PDO_NOPREF|PDO_DLGHORZ, &r0d001_99999, N_("Coupler Length") },
#define I_CD_CANVAS             (B+11)
	{ PD_DRAW, NULL, "canvas", PDO_NOPSHUPD|PDO_DLGWIDE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGBOXEND|PDO_DLGRESIZE, &carDlgDrawData, NULL, 0 },

#define C                       (B+12)
#define I_CD_ITEMINDEX          (C+0)
	{ PD_LONG, &carDlgItemIndex, "index", PDO_NOPREF|PDO_DLGWIDE, &i1_999999999, N_("Index"), 0 },
#define I_CD_PURPRC             (C+1)
	{ PD_STRING, &carDlgPurchPriceStr, "purchPrice", PDO_NOPREF|PDO_DLGWIDE|PDO_STRINGLIMITLENGTH, I2VP(50), N_("Purchase Price"), 0, &carDlgPurchPrice, sizeof(carDlgPurchPriceStr) },
#define I_CD_CURPRC             (C+2)
	{ PD_STRING, &carDlgCurrPriceStr, "currPrice", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ|PDO_STRINGLIMITLENGTH, I2VP(50), N_("Current Price"), 0, &carDlgCurrPrice, sizeof(carDlgCurrPriceStr) },
#define I_CD_COND               (C+3)
	{ PD_DROPLIST, &carDlgConditionInx, "condition", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, I2VP(90), N_("Condition") },
#define I_CD_PURDAT             (C+4)
	{ PD_STRING, &carDlgPurchDateStr, "purchDate",  PDO_NOPREF|PDO_DLGWIDE|PDO_STRINGLIMITLENGTH, I2VP(80), N_("Purchase Date"), 0, &carDlgPurchDate, sizeof(carDlgPurchDateStr) },
#define I_CD_SRVDAT             (C+5)
	{ PD_STRING, &carDlgServiceDateStr, "serviceDate",  PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ|PDO_STRINGLIMITLENGTH, I2VP(80), N_("Service Date"), 0, &carDlgServiceDate, sizeof(carDlgServiceDateStr) },
#define I_CD_QTY                (C+6)
	{ PD_LONG, &carDlgQuantity, "quantity", PDO_NOPREF|PDO_DLGWIDE, &i1_9999, N_("Quantity") },
#define I_CD_MLTNUM             (C+7)
	{ PD_RADIO, &carDlgMultiNum, "multinum", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, multinumLabels, N_("Numbers"), BC_HORZ|BC_NOBORDER },
#define I_CD_NOTES              (C+8)
	{ PD_TEXT, NULL, "notes", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &notesData, N_("Notes") },

#define D                       (C+9)
#define I_CD_MSG                (D+0)
	{ PD_MESSAGE, NULL, NULL, PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGBOXEND, I2VP(450) },
#define I_CD_NEW                (D+1)
	{ PD_MENU, NULL, "new-menu", PDO_DLGCMDBUTTON, NULL, N_("New"), 0, I2VP(0) },
	{ PD_MENUITEM, CarDlgNewDesc, "new-part-mi", 0, NULL, N_("Car Part"), 0, I2VP(0) },
	{ PD_MENUITEM, CarDlgNewProto, "new-proto-mi", 0, NULL, N_("Car Prototype"), 0, I2VP(0) },
#define I_CD_NEWPROTO           (D+4)
	{ PD_BUTTON, CarDlgNewProto, "new", PDO_DLGCMDBUTTON, NULL, N_("New"), 0, I2VP(0) }
};

static paramGroup_t carDlgPG = { "carpart", 0, carDlgPLs, COUNT( carDlgPLs ) };


static dynArr_t carDlgSegs_da;
#define carDlgSegs(N) DYNARR_N( trkSeg_t, carDlgSegs_da, N )


typedef enum {
	T_ItemSel, T_ItemEnter, T_ProtoSel, T_ProtoEnter, T_PartnoSel, T_PartnoEnter
} carDlgTransistion_e;
static char *carDlgTransistion_s[] = {
	"ItemSel", "ItemEnter", "ProtoSel", "ProtoEnter", "PartnoSel", "PartnoEnter"
};
typedef enum {
	S_Error,
	S_ItemSel, S_ItemEnter, S_PartnoSel, S_PartnoEnter, S_ProtoSel
} carDlgState_e;
static char *carDlgState_s[] = {
	"Error",
	"ItemSel", "ItemEnter", "PartnoSel", "PartnoEnter", "ProtoSel"
};
static char *carDlgAction_s[] = {
	"Return",
	"SError",
	"Else",
	"SItemSel",
	"SItemEnter",
	"SPartnoSel",
	"SPartnoEnter",
	"SProtoSel",
	"IsCustom",
	"IsNewPart",
	"IsNewProto",
	"LoadDataFromPartList",
	"LoadDimsFromStack",
	"LoadManufListForScale",
	"LoadManufListAll",
	"LoadProtoListForManuf",
	"LoadProtoListAll",
	"LoadPartnoList",
	"LoadLists",
	"LoadDimsFromProtoList",
	"ConvertDimsToProto",
	"Redraw",
	"ClrManuf",
	"ClrPartnoStr",
	"ClrNumberStr",
	"LoadProtoStrFromList",
	"ShowPartnoList",
	"HidePartnoList",
	"PushDims",
	"PopDims",
	"PopTitleAndTypeinx",
	"PopCouplerLength",
	"ShowControls",
	"LoadInfoFromUpdateItem",
	"LoadDataFromUpdatePart",
	"InitProto",
	"RecallCouplerLength",
	"Last"
};
static carDlgAction_e stateMachine[7][7][10] = {
	/* A_SError */{   {A_SError}, {A_SError}, {A_SError}, {A_SError}, {A_SError}, {A_SError}, {A_SError} },

	/*A_SItemSel*/{
		/*T_ItemSel*/    { A_LoadProtoListForManuf, A_LoadPartnoList, A_LoadDataFromPartList, A_Redraw },
		/*T_ItemEnter*/  { A_SItemEnter, A_LoadProtoListAll, A_ClrPartnoStr, A_ClrNumberStr, A_LoadDimsFromProtoList, A_Redraw, A_HidePartnoList },
		/*T_ProtoSel*/   { A_LoadPartnoList, A_LoadDataFromPartList, A_Redraw },
		/*T_ProtoEnter*/ { A_SError },
		/*T_PartnoSel*/  { A_LoadDataFromPartList, A_Redraw },
		/*T_PartnoEnter*/{ A_SItemEnter, A_LoadProtoListAll, A_HidePartnoList }
	},

	/*A_SItemEnter*/{
		/*T_ItemSel*/    { A_SItemSel, A_LoadProtoListForManuf, A_LoadPartnoList, A_LoadDataFromPartList, A_Redraw, A_ShowPartnoList },
		/*T_ItemEnter*/  { A_Return },
		/*T_ProtoSel*/   { A_LoadDimsFromProtoList, A_Redraw },
		/*T_ProtoEnter*/ { A_SError },
		/*T_PartnoSel*/  { A_SError },
		/*T_PartnoEnter*/{ A_Return }
	},

	/*A_SPartnoSel*/{
		/*T_ItemSel*/   { A_SPartnoSel },
		/*T_ItemEnter*/ { A_SPartnoSel },
		/*T_ProtoSel*/   { A_SPartnoSel, A_LoadDimsFromProtoList, A_Redraw },
		/*T_ProtoEnter*/ { A_SError },
		/*T_PartnoSel*/  { A_SError }
	},

	/*A_SPartnoEnter*/{
		/*T_ItemSel*/   { A_SPartnoSel },
		/*T_ItemEnter*/ { A_SPartnoEnter },
		/*T_ProtoSel*/   { A_SPartnoEnter, A_LoadDimsFromProtoList, A_Redraw },
		/*T_ProtoEnter*/ { A_SError },
		/*T_PartnoSel*/  { A_SError },
		/*T_PartnoEnter*/{ A_SPartnoEnter }
	},

	/*A_SProtoSel*/{
		/*T_ItemSel*/   { A_SError },
		/*T_ItemEnter*/ { A_SError },
		/*T_ProtoSel*/   { A_SError },
		/*T_ProtoEnter*/ { A_SProtoSel },
		/*T_PartnoSel*/  { A_SError },
		/*T_PartnoEnter*/{ A_SError }
	}
};

carDlgAction_e itemNewActions[] = {
	A_RecallCouplerLength,
	A_LoadLists,
	A_IsCustom, 2+3,
	A_LoadDimsFromProtoList, A_ClrPartnoStr, A_ClrNumberStr,
	A_Else, 1,
	A_LoadDataFromPartList,
	A_ShowControls, A_Return
};
carDlgAction_e itemUpdActions[] = { A_LoadInfoFromUpdateItem, /*A_LoadManufListForScale,
				A_IsCustom, 5,
						A_LoadProtoListAll, A_HidePartnoList, A_SItemEnter,
				A_Else, 5,
						A_LoadProtoListForManuf, A_LoadPartnoList, A_LoadDataFromPartList, A_ShowPartnoList, A_SItemSel,*/
                                           A_ShowControls, A_Return
                                         };

static carDlgAction_e partNewActions[] = { A_RecallCouplerLength, A_LoadManufListAll, A_LoadProtoListAll, A_ClrPartnoStr, A_ClrNumberStr, A_SPartnoSel, A_LoadDimsFromProtoList, A_ShowControls, A_Redraw, A_Return };
static carDlgAction_e partUpdActions[] = { A_LoadDataFromUpdatePart, A_SPartnoSel, A_ShowControls, A_Return };

static carDlgAction_e protoNewActions[] = { A_InitProto, A_SProtoSel, A_ShowControls, A_Return };
static carDlgAction_e protoUpdActions[] = { A_InitProto, A_SProtoSel, A_ShowControls, A_Return };

static carDlgAction_e item2partActions[] = {
	A_PushDims, A_LoadManufListAll, A_LoadProtoListAll,
	A_IsCustom, 0+1,
	A_ClrManuf,
	A_SPartnoSel,
	A_ShowControls, A_Return
};
static carDlgAction_e part2itemActions[] = {
	A_IsNewPart, 2+0,
	A_Else, 1,
	A_PopTitleAndTypeinx,
	A_LoadLists,
	A_IsCustom, 2+1,
	A_LoadDimsFromProtoList,
	A_Else, 1,
	A_LoadDataFromPartList,
#ifdef LATER
	A_IsNewPart, 2+0,
	A_Else, 1,
	A_LoadDimsFromStack,
#endif
	A_ShowControls,
	A_Return
};

static carDlgAction_e item2protoActions[] = { A_PushDims, A_ConvertDimsToProto, A_SProtoSel, A_ShowControls, A_Return };
static carDlgAction_e proto2itemActions[] = {
	A_IsCustom, 2+2+3,
	A_IsNewProto, 2+3,
	A_LoadProtoListAll,
	A_PopCouplerLength,
	A_LoadDimsFromProtoList,
	A_Else, 2,
	A_LoadDimsFromStack,
	A_LoadProtoStrFromList,
	A_ShowControls,
	A_Return
};

static carDlgAction_e part2protoActions[] = { A_PushDims, A_ConvertDimsToProto, A_SProtoSel, A_ShowControls, A_Return };
static carDlgAction_e proto2partActions[] = {
	A_IsNewProto, 2+3,
	A_LoadProtoListAll,
	A_PopCouplerLength,
	A_LoadDimsFromProtoList,
	A_Else, 2,
	A_LoadDimsFromStack,
	A_LoadProtoStrFromList,
	A_ShowControls,
	A_Return
};


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
#define S_ITEM (currState==S_ItemSel||currState==S_ItemEnter)
#define S_PART (currState==S_PartnoSel)
#define S_PROTO (currState==S_ProtoSel)



static void CarDlgLoadDimsFromPart( carPart_p partP )
{
	tabString_t tabs[7];

	if ( partP == NULL ) { return; }
	carDlgDim = partP->dim;
	carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
	sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
	         GetScaleName(carDlgScaleInx) );
	wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
	carDlgIsLoco = (partP->options&CAR_DESC_IS_LOCO)?1:0;
	carDlgBodyColor = partP->color;
	ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
	ParamLoadControl( &carDlgPG, I_CD_CARWIDTH );
	ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
	ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
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
	carDlgIsLoco = (protoP->options&CAR_DESC_IS_LOCO)?1:0;
	ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
	ParamLoadControl( &carDlgPG, I_CD_CARWIDTH );
	ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
	ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
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
	if ( !S_PROTO ) {
		if ( carDlgProtoInx < 0 ||
		     (protoP = CarProtoLookup( carDlgProtoStr, FALSE, FALSE, 0.0, 0.0 )) == NULL ||
		     protoP->segCnt == 0 ) {
			CarProtoDlgCreateDummyOutline( &segCnt, &segPtr, (BOOL_T)carDlgIsLoco,
			                               carDlgDim.carLength, carDlgDim.carWidth, carDlgBodyColor );
		} else {
			segCnt = protoP->segCnt;
			segPtr = protoP->segPtr;
		}
	} else {
		if ( carProtoSegCnt <= 0 ) {
			CarProtoDlgCreateDummyOutline( &segCnt, &segPtr, (BOOL_T)carDlgIsLoco,
			                               carDlgDim.carLength, carDlgDim.carWidth, drawColorBlue );
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
	if ( !S_PROTO ) {
		RecolorSegs( carDlgSegs_da.cnt, &carDlgSegs(0), carDlgBodyColor );
	} else {
		if ( carDlgFlipToggle ) {
			pos.x = carDlgDim.carLength/2.0;
			pos.y = carDlgDim.carWidth/2.0;
			RotateSegs( carDlgSegs_da.cnt, &carDlgSegs(0), pos, 180.0 );
		}
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
	roadnameMap_p roadnameMapP;

	if ( !roadnameMapChanged ) { return; }
	wListClear( (wList_p)carDlgPLs[I_CD_ROADNAME_LIST].control );
	wListAddValue( (wList_p)carDlgPLs[I_CD_ROADNAME_LIST].control, _("Undecorated"),
	               NULL, NULL );
	for ( inx=0; inx<roadnameMap_da.cnt; inx++ ) {
		roadnameMapP = DYNARR_N(roadnameMap_p, roadnameMap_da, inx);
		wListAddValue( (wList_p)carDlgPLs[I_CD_ROADNAME_LIST].control,
		               roadnameMapP->roadname, NULL, roadnameMapP );
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

	LOG( log_carDlgList, 3,
	     ( "CarDlgLoadManufList( %s, %s, %d )\n    carDlgManufStr=\"%s\"\n",
	       bLoadAll?"TRUE":"FALSE", bInclCustomUnknown?"TRUE":"FALSE", scale,
	       carDlgManufStr ) )
	carDlgManufInx = -1;
	manufP1 = NULL;
	wListClear( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control );
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
		listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control,
		                         manufP->manuf, NULL, manufP );
		if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0'
		                             || strcasecmp( carDlgManufStr, manufP->manuf ) == 0 ) ) {
			LOG( log_carDlgList, 4, ( "    found manufStr (inx=%d, listInx=%d)\n", inx,
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
		listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control,
		                         _("Custom"), NULL, NULL );
		if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0'
		                             || strcasecmp( carDlgManufStr, "Custom" ) == 0 ) ) {
			LOG( log_carDlgList, 4, ( "    found Cus manufStr (inx=%d, listInx=%d)\n", inx,
			                          listInx ) )
			carDlgManufInx = listInx;
			if ( carDlgManufStr[0] == '\0' ) { strcpy( carDlgManufStr, _("Custom") ); }
		}
		if ( firstName == NULL ) {
			firstName = "Custom";
		}
		wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, _("Unknown"), NULL,
		               NULL );
		if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0'
		                             || strcasecmp( carDlgManufStr, "Unknown" ) == 0 ) ) {
			LOG( log_carDlgList, 4, ( "    found Unk manufStr (inx=%d, listInx=%d)\n", inx,
			                          listInx ) )
			carDlgManufInx = listInx;
			if ( carDlgManufStr[0] == '\0' ) { strcpy( carDlgManufStr, _("Unknown") ); }
		}
	}
	if ( carDlgManufInx < 0 ) {
		found = FALSE;
		if ( firstName != NULL ) {
			LOG( log_carDlgList, 4, ( "    didn't find manufStr, using [0] = %s\n",
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
	wIndex_t inx, listInx, inx1;
	BOOL_T found;
	carProto_p protoP;
	carPart_p partP;
	char * firstName;
	int typeCount[N_TYPELISTMAP];
	int listTypeInx, currTypeInx;

	listTypeInx = -1;
	carDlgProtoInx = -1;
	firstName = NULL;

	wListClear( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control );
	memset( typeCount, 0, N_TYPELISTMAP * sizeof typeCount[0] );
	LOG( log_carDlgList, 3,
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
				LOG( log_carDlgList, 4, ( "    found typeinx, reset list (old=%d)\n",
				                          listTypeInx ) )
				wListClear( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control );
				listTypeInx = carDlgTypeInx;
				carDlgProtoInx = -1;
				firstName = NULL;
			}
			if ( currTypeInx != listTypeInx ) { continue; }
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
			                         protoP->desc, NULL, protoP );
			if ( carDlgProtoInx < 0 && carDlgProtoStr[0]
			     && strcasecmp( carDlgProtoStr, protoP->desc ) == 0 ) {
				LOG( log_carDlgList, 4, ( "    found protoStr (inx=%d, listInx=%d)\n", inx,
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
			for ( inx1=0; inx1<parentP->parts_da.cnt; inx1++ ) {
				partP = carPart( parentP, inx1 );
				currTypeInx = CarProtoFindTypeCode(partP->type);
				typeCount[currTypeInx]++;
				if ( listTypeInx < 0 ) {
					listTypeInx = currTypeInx;
				}
				if ( carDlgTypeInx >= 0 &&
				     listTypeInx != carDlgTypeInx &&
				     currTypeInx == carDlgTypeInx ) {
					LOG( log_carDlgList, 4, ( "    found typeinx, reset list (old=%d)\n",
					                          listTypeInx ) )
					wListClear( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control );
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
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
			                         parentP->proto, NULL, parentP );
			if ( carDlgProtoInx < 0 && ( carDlgProtoStr[0] == '\0'
			                             || strcasecmp( carDlgProtoStr, parentP->proto ) == 0 ) ) {
				LOG( log_carDlgList, 4, ( "    found protoStr (inx=%d, listInx=%d)\n", inx,
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
		if ( firstName != NULL ) {
			LOG( log_carDlgList, 4, ( "    didn't find protoStr, using [0] = %s\n",
			                          firstName ) )
			carDlgProtoInx = 0;
			strcpy( carDlgProtoStr, firstName );
		}
	}
	wListSetIndex( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
	               carDlgProtoInx );

	if ( loadTypeList ) {
		LOG( log_carDlgList, 4, ( "    loading typelist\n" ) )
		wListClear( (wList_p)carDlgPLs[I_CD_PROTOKIND_LIST].control );
		for ( currTypeInx=0; currTypeInx<N_TYPELISTMAP; currTypeInx++ ) {
			if ( typeCount[currTypeInx] > 0 ) {
				listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOKIND_LIST].control,
				                         _(typeListMap[currTypeInx].name), NULL, I2VP(currTypeInx) );
				if ( currTypeInx == listTypeInx ) {
					LOG( log_carDlgList, 4, ( "        current = %d\n", listInx ) )
					carDlgKindInx = listInx;
				}
			}
		}
	}

	return found;
}


static void ConstructPartDesc(
        tabString_t * tabs )
{
	char * cp;
	cp = message;
	*cp = '\0';
	if ( tabs[T_PART].len ) {
		cp = TabStringCpy( cp, &tabs[T_PART] );
		*cp++ = ' ';
	}
	if ( tabs[T_DESC].len ) {
		cp = TabStringCpy( cp, &tabs[T_DESC] );
		*cp++ = ' ';
	}
	if ( tabs[T_REPMARK].len ) {
		cp = TabStringCpy( cp, &tabs[T_REPMARK] );
		*cp++ = ' ';
	} else if ( tabs[T_ROADNAME].len ) {
		cp = TabStringCpy( cp, &tabs[T_ROADNAME] );
		*cp++ = ' ';
	} else {
		strcpy( cp, _("Undecorated ") );
		cp += strlen( cp );
	}
	if ( tabs[T_NUMBER].len ) {
		cp = TabStringCpy( cp, &tabs[T_NUMBER] );
		*cp++ = ' ';
	}
	*cp = '\0';
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

	carDlgPartnoInx = -1;
	wListClear( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control );
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
		ConstructPartDesc( tabs );
		lastPart.paramFileIndex = partP->paramFileIndex;
		if ( message[0] && IsParamValid(partP->paramFileIndex) &&
		     ( lastPart.title == NULL || Cmp_part( &lastPart, partP ) != 0 ) ) {
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, message,
			                         NULL, partP );
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
	carDlgIsLoco = ((partP->type)&1)!=0;
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
	DIST_T ratio;
	carPartParent_p parentP;
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
		carDlgManufInx = wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control,
		                                carDlgManufStr, NULL, NULL );
		isItem = FALSE;
	}
	if ( isItem ) {
		parentP = (carPartParent_p)wListGetItemContext( (wList_p)
		                carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
		if ( parentP ) {
			if ( tabs ) { TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] ); }
			if ( CarDlgLoadProtoList( carDlgManufStr, scale, TRUE ) || !tabs ) {
				parentP = (carPartParent_p)wListGetItemContext( (wList_p)
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
		ratio = GetScaleRatio( scale );
		protoTmp.contentsLabel = "temporary";
		protoTmp.paramFileIndex = PARAM_LAYOUT;
		strcpy( protoTmpDesc, carDlgProtoStr );
		protoTmp.desc = protoTmpDesc;
		protoTmp.options = (carDlgIsLoco?CAR_DESC_IS_LOCO:0);
		protoTmp.type = typeListMap[carDlgTypeInx].value;
		protoTmp.dim.carWidth = carDlgDim.carWidth*ratio;
		protoTmp.dim.carLength = carDlgDim.carLength*ratio;
		protoTmp.dim.coupledLength = carDlgDim.coupledLength*ratio;
		protoTmp.dim.truckCenter = carDlgDim.truckCenter*ratio;
		protoTmp.dim.truckCenterOffset = carDlgDim.truckCenterOffset*ratio;
		CarProtoDlgCreateDummyOutline( &carProtoSegCnt, &carProtoSegPtr,
		                               (BOOL_T)carDlgIsLoco, protoTmp.dim.carLength, protoTmp.dim.carWidth,
		                               drawColorBlue );
		protoTmp.segCnt = carProtoSegCnt;
		protoTmp.segPtr = carProtoSegPtr;
		GetSegBounds( zero, 0.0, carProtoSegCnt, carProtoSegPtr, &protoTmp.orig,
		              &protoTmp.size );
		TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] );
		carDlgProtoInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control,
		                                carDlgProtoStr, NULL, &protoTmp );/*??*/
	}
	carDlgPartnoInx = -1;
	if ( tabs ) {
		TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
		TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
	}
	return FALSE;
}


static void CarDlgShowControls( void )
{


	/*ParamControlActive( &carDlgPG, I_CD_MANUF_LIST,		S_ITEM||(S_PART&&carDlgUpdatePartPtr) );*/

	ParamControlShow( &carDlgPG, I_CD_NEW,					S_ITEM );
	ParamControlShow( &carDlgPG, I_CD_NEWPROTO,				S_PART );

	ParamControlShow( &carDlgPG, I_CD_ITEMINDEX,			S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_PURPRC,				S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_CURPRC,				S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_COND,					S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_PURDAT,				S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_SRVDAT,				S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_NOTES,				S_ITEM && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_MLTNUM,				S_ITEM
	                  && carDlgUpdateItemPtr==NULL && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_QTY,					S_ITEM && carDlgUpdateItemPtr==NULL
	                  && carDlgDispMode==0 );

	ParamControlShow( &carDlgPG, I_CD_ROADNAME_LIST,		S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_REPMARK,				S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_NUMBER,				S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_BODYCOLOR,			S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_CARLENGTH,			!( S_ITEM
	                  && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_CARWIDTH,				!( S_ITEM
	                  && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_TRKCENTER,			!( S_ITEM
	                  && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_TRKOFFSET,            !( S_ITEM
	                  && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_CANVAS,				!( S_ITEM && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_CPLRLEN,				S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_CPLDLEN,				S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_CPLRMNT,				S_PART || ( S_ITEM
	                  && carDlgDispMode==1 ) );

	ParamControlShow( &carDlgPG, I_CD_DISPMODE,				S_ITEM );

	ParamControlShow( &carDlgPG, I_CD_TYPE_LIST,			S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_FLIP,					S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_DESC_STR,				S_PART
	                  || (currState==S_ItemEnter) );
	ParamControlShow( &carDlgPG, I_CD_IMPORT,				S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_RESET,				S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PARTNO_STR,			S_PART
	                  || (currState==S_ItemEnter) );
	ParamControlShow( &carDlgPG, I_CD_PARTNO_LIST,			(currState==S_ItemSel) );
	ParamControlShow( &carDlgPG, I_CD_ISLOCO,				S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PROTOKIND_LIST,		!S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PROTOTYPE_LIST,		!S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PROTOTYPE_STR,		S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_MANUF_LIST,			!S_PROTO );

	/*ParamControlActive( &carDlgPG, I_CD_PROTOTYPE_STR,	S_PROTO && carDlgUpdateProtoPtr==NULL );*/
	ParamControlActive( &carDlgPG, I_CD_ITEMINDEX,			S_ITEM
	                    && carDlgUpdateItemPtr==NULL );
	ParamControlActive( &carDlgPG, I_CD_MLTNUM,				S_ITEM && carDlgQuantity>1 );
	ParamControlActive( &carDlgPG, I_CD_IMPORT,				selectedTrackCount > 0 );

	ParamLoadMessage( &carDlgPG, I_CD_MSG, "" );

	if ( S_ITEM ) {
		if ( carDlgUpdateItemPtr == NULL ) {
			sprintf( message, _("New %s Scale Car"), GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, _("Add") );
		} else {
			sprintf( message, _("Update %s Scale Car"), GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, _("Update") );
		}
		wWinSetTitle( carDlgPG.win, message );
	} else if ( S_PART ) {
		if ( carDlgUpdatePartPtr == NULL ) {
			sprintf( message, _("New %s Scale Car Part"), GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, _("Add") );
		} else {
			sprintf( message, _("Update %s Scale Car Part"),
			         GetScaleName( carDlgScaleInx ) );
			wButtonSetLabel( carDlgPG.okB, _("Update") );
		}
		wWinSetTitle( carDlgPG.win, message );
	} else if ( S_PROTO ) {
		if ( carDlgUpdateProtoPtr == NULL ) {
			wWinSetTitle( carDlgPG.win, _("New Prototype") );
			wButtonSetLabel( carDlgPG.okB, _("Add") );
		} else {
			wWinSetTitle( carDlgPG.win, _("Update Prototype") );
			wButtonSetLabel( carDlgPG.okB, _("Update") );
		}
	}

	ParamLoadControls( &carDlgPG );

	ParamDialogOkActive( &carDlgPG, S_ITEM );
	CarDlgUpdate( &carDlgPG, -1, NULL );
}



static void CarDlgDoActions(
        carDlgAction_e * actions )
{
	carPart_p partP;
	carPartParent_p parentP;
	carProto_p protoP;
	wIndex_t inx;
	int offset;
	DIST_T ratio;
	tabString_t tabs[7];
	char * cp;
	BOOL_T reload[COUNT( carDlgPLs )];
#define RELOAD_DIMS \
		reload[I_CD_CARLENGTH] = reload[I_CD_CARWIDTH] = reload[I_CD_CPLDLEN] = \
		reload[I_CD_TRKCENTER] = reload[I_CD_TRKOFFSET] = reload[I_CD_CPLRLEN] = TRUE
#define RELOAD_PARTDATA \
		RELOAD_DIMS; \
		reload[I_CD_PARTNO_STR] = reload[I_CD_DESC_STR] = \
		reload[I_CD_ROADNAME_LIST] = reload[I_CD_REPMARK] = \
		reload[I_CD_NUMBER] = reload[I_CD_BODYCOLOR] = TRUE
#define RELOAD_LISTS \
		reload[I_CD_MANUF_LIST] = \
		reload[I_CD_PROTOKIND_LIST] = \
		reload[I_CD_PROTOTYPE_LIST] = \
		reload[I_CD_PARTNO_LIST] = TRUE

	memset( reload, 0, sizeof reload );
	while ( 1 ) {
		LOG( log_carDlgState, 2, ( "Action = %s\n", carDlgAction_s[*actions] ) )
		switch ( *actions++ ) {
		case A_Return:
			for ( inx=0; inx<COUNT( carDlgPLs ); inx++ )
				if ( reload[inx] ) {
					ParamLoadControl( &carDlgPG, inx );
				}
			return;
		case A_SError:
			currState = S_Error;
			break;
		case A_Else:
			offset = (int)*actions++;
			actions += offset;
			break;
		case A_SItemSel:
			currState = S_ItemSel;
			break;
		case A_SItemEnter:
			currState = S_ItemEnter;
			break;
		case A_SPartnoSel:
			currState = S_PartnoSel;
			break;
		case A_SPartnoEnter:
			currState = S_PartnoEnter;
			break;
		case A_SProtoSel:
			currState = S_ProtoSel;
			break;
		case A_IsCustom:
			offset = (int)*actions++;
			if ( currState != S_ItemEnter ) {
				actions += offset;
			}
			break;
		case A_IsNewPart:
			offset = (int)*actions++;
			if (carDlgNewPartPtr==NULL) {
				actions += offset;
			} else {
				TabStringExtract( carDlgNewPartPtr->title, 7, tabs );
				TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
				TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
				reload[I_CD_PARTNO_STR] = reload[I_CD_DESC_STR] = TRUE;
			}
			break;
		case A_IsNewProto:
			offset = (int)*actions++;
			if (carDlgNewProtoPtr==NULL) {
				actions += offset;
			} else {
				strcpy( carDlgProtoStr, carDlgNewProtoPtr->desc );
			}
			break;
		case A_LoadDataFromPartList:
			partP = (carPart_p)wListGetItemContext( (wList_p)
			                                        carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoInx );
			if ( partP != NULL ) {
				CarDlgLoadPart(partP);
				RELOAD_PARTDATA;
				RELOAD_PARTDATA;
			}
			break;
		case A_LoadDimsFromStack:
			carDlgDim = carDlgStk[carDlgStkPtr].dim;
			carDlgCouplerLength = carDlgStk[carDlgStkPtr].couplerLength;
			carDlgTypeInx = carDlgStk[carDlgStkPtr].typeInx;
			carDlgIsLoco = (typeListMap[carDlgTypeInx].value&1) != 0;
			RELOAD_DIMS;
			break;
		case A_LoadManufListForScale:
			CarDlgLoadManufList( FALSE, TRUE, carDlgScaleInx );
			reload[I_CD_MANUF_LIST] = TRUE;
			break;
		case A_LoadManufListAll:
			CarDlgLoadManufList( TRUE, FALSE, carDlgScaleInx );
			reload[I_CD_MANUF_LIST] = TRUE;
			break;
		case A_LoadProtoListForManuf:
			parentP = (carPartParent_p)wListGetItemContext( (wList_p)
			                carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
			CarDlgLoadProtoList( parentP->manuf, parentP->scale, TRUE );
			reload[I_CD_PROTOKIND_LIST] = TRUE;
			reload[I_CD_PROTOTYPE_LIST] = TRUE;
			break;
		case A_LoadProtoListAll:
			CarDlgLoadProtoList( NULL, 0, TRUE );
			reload[I_CD_PROTOKIND_LIST] = TRUE;
			reload[I_CD_PROTOTYPE_LIST] = TRUE;
			break;
		case A_LoadPartnoList:
			parentP = (carPartParent_p)wListGetItemContext( (wList_p)
			                carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			CarDlgLoadPartList( parentP );
			reload[I_CD_PARTNO_LIST] = TRUE;
			break;
		case A_LoadLists:
			if ( CarDlgLoadLists( TRUE, NULL, carDlgScaleInx ) ) {
				currState = S_ItemSel;
			} else {
				currState = S_ItemEnter;
			}
			break;
		case A_LoadDimsFromProtoList:
			protoP = (carProto_p)wListGetItemContext( (wList_p)
			                carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			if ( protoP ) {
				CarDlgLoadDimsFromProto( protoP );
				carDlgTypeInx = CarProtoFindTypeCode( protoP->type );
				carDlgIsLoco = (protoP->options&CAR_DESC_IS_LOCO)!=0;
			} else {
				ratio = GetScaleRatio( carDlgScaleInx );
				carDlgDim.carLength = 50*12/ratio;
				carDlgDim.carWidth = 10*12/ratio;
				carDlgDim.coupledLength = carDlgDim.carLength+carDlgCouplerLength*2;
				carDlgDim.truckCenter = carDlgDim.carLength-59.0*2.0/ratio;

				carDlgTypeInx = 0;
				carDlgIsLoco = (typeListMap[0].value&1);
			}
			RELOAD_DIMS;
			reload[I_CD_TYPE_LIST] = reload[I_CD_ISLOCO] = TRUE;
			break;
		case A_ConvertDimsToProto:
			ratio = GetScaleRatio( carDlgScaleInx );
			carDlgDim.carLength *= ratio;
			carDlgDim.carWidth *= ratio;
			carDlgCouplerLength = 16.0;
			carDlgDim.coupledLength = carDlgDim.carLength + 2 * carDlgCouplerLength;
			carDlgDim.truckCenter *= ratio;
			carDlgDim.truckCenterOffset *= ratio;
			RELOAD_DIMS;
			break;
		case A_Redraw:
			CarDlgRedraw( carDlgD.d, NULL, 0, 0 );
			break;
		case A_ClrManuf:
			carDlgManufStr[0] = '\0';
			wListSetValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, "" );
			carDlgManufInx = -1;
			break;
		case A_ClrPartnoStr:
			carDlgPartnoStr[0] = '\0';
			carDlgDescStr[0] = '\0';
			reload[I_CD_PARTNO_STR] = reload[I_CD_DESC_STR] = TRUE;
			break;
		case A_ClrNumberStr:
			carDlgNumberStr[0] = '\0';
			reload[I_CD_NUMBER] = TRUE;
			break;
		case A_LoadProtoStrFromList:
			wListGetValues( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoStr,
			                sizeof carDlgProtoStr, NULL, NULL );
#ifdef LATER
			protoP = (carProto_p)wListGetItemContext( (wList_p)
			                carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			if ( protoP ) {
				carDlgTypeInx = CarProtoFindTypeCode( protoP->type );
				carDlgIsLoco = (protoP->options&CAR_DESC_IS_LOCO)!=0;
			}
#endif
			break;
		case A_ShowPartnoList:
			reload[I_CD_PARTNO_LIST] = TRUE;
			ParamControlShow( &carDlgPG, I_CD_PARTNO_LIST, TRUE );
			ParamControlShow( &carDlgPG, I_CD_DESC_STR, FALSE );
			ParamControlShow( &carDlgPG, I_CD_PARTNO_STR, FALSE );
			break;
		case A_HidePartnoList:
			reload[I_CD_PARTNO_STR] = reload[I_CD_DESC_STR] = TRUE;
			ParamControlShow( &carDlgPG, I_CD_PARTNO_LIST, FALSE );
			ParamControlShow( &carDlgPG, I_CD_DESC_STR, TRUE );
			ParamControlShow( &carDlgPG, I_CD_PARTNO_STR, TRUE );
			break;
		case A_PushDims:
			CHECK( carDlgStkPtr < CARDLG_STK_SIZE );
			carDlgStk[carDlgStkPtr].dim = carDlgDim;
			carDlgStk[carDlgStkPtr].couplerLength = carDlgCouplerLength;
			carDlgStk[carDlgStkPtr].state = currState;
			carDlgStk[carDlgStkPtr].changed = carDlgChanged;
			carDlgStk[carDlgStkPtr].typeInx = carDlgTypeInx;
			if ( currState == S_ItemSel && carDlgPartnoInx >= 0 ) {
				carDlgStk[carDlgStkPtr].partP = (carPart_p)wListGetItemContext( (
				                                        wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoInx );
			} else {
				carDlgStk[carDlgStkPtr].partP = NULL;
			}
			carDlgStkPtr++;
			break;
		case A_PopDims:
			break;
		case A_PopTitleAndTypeinx:
			if ( carDlgStk[carDlgStkPtr].partP ) {
				TabStringExtract( carDlgStk[carDlgStkPtr].partP->title, 7, tabs );
				strcpy( carDlgManufStr, carDlgStk[carDlgStkPtr].partP->parent->manuf );
				strcpy( carDlgProtoStr, carDlgStk[carDlgStkPtr].partP->parent->proto );
				TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
				TabStringCpy( carDlgDescStr, &tabs[T_DESC] );
			}
			carDlgTypeInx = carDlgStk[carDlgStkPtr].typeInx;
			break;
		case A_PopCouplerLength:
			carDlgCouplerLength = carDlgStk[carDlgStkPtr].couplerLength;
			break;
		case A_ShowControls:
			CarDlgShowControls();
			break;
		case A_LoadInfoFromUpdateItem:
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
			carDlgIsLoco = (carDlgUpdateItemPtr->type&1)!=0;
			carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
			sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
			         GetScaleName(carDlgScaleInx) );
			wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
			carDlgCouplerMount = (carDlgUpdateItemPtr->options&CAR_DESC_COUPLER_MODE_BODY)!=
			                     0;
			carDlgIsLoco = (carDlgUpdateItemPtr->options&CAR_DESC_IS_LOCO)!=0;
			carDlgPurchPrice = carDlgUpdateItemPtr->data.purchPrice;
			sprintf( carDlgPurchPriceStr, "%0.2f", carDlgPurchPrice );
			carDlgCurrPrice = carDlgUpdateItemPtr->data.currPrice;
			sprintf( carDlgCurrPriceStr, "%0.2f", carDlgCurrPrice );
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
				for ( cp=message; *cp; cp++ ) {
					if ( *cp == '\n' ) { *cp = ' '; }
				}
				wTextAppend( (wText_p)carDlgPLs[I_CD_NOTES].control, message );
			}
			LoadRoadnameList( &tabs[T_ROADNAME], &tabs[T_REPMARK] );
			CarDlgLoadRoadnameList();
			carDlgRoadnameInx = lookupListIndex+1;
			memset( reload, 1, sizeof reload );

			if ( CarDlgLoadLists( TRUE, tabs, carDlgScaleInx ) ) {
				currState = S_ItemSel;
			} else {
				currState = S_ItemEnter;
			}
			break;
		case A_LoadDataFromUpdatePart:
			carDlgScaleInx = carDlgUpdatePartPtr->parent->scale;
			TabStringExtract( carDlgUpdatePartPtr->title, 7, tabs );
			tabs[T_MANUF].ptr = carDlgUpdatePartPtr->parent->manuf;
			tabs[T_MANUF].len = (int)strlen(carDlgUpdatePartPtr->parent->manuf);
			tabs[T_PROTO].ptr = carDlgUpdatePartPtr->parent->proto;
			tabs[T_PROTO].len = (int)strlen(carDlgUpdatePartPtr->parent->proto);
			CarDlgLoadLists( FALSE, tabs, carDlgScaleInx );
			CarDlgLoadPart( carDlgUpdatePartPtr );
			RELOAD_LISTS;
			RELOAD_DIMS;
			RELOAD_PARTDATA;
			break;
		case A_InitProto:
			if ( carDlgUpdateProtoPtr==NULL ) {
				carDlgProtoStr[0] = 0;
				carDlgDim.carLength = 50*12;
				carDlgDim.carWidth = 10*12;
				carDlgDim.coupledLength = carDlgDim.carLength+16.0*2.0;
				carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
				carDlgDim.truckCenter = carDlgDim.carLength-59.0*2.0;
				carDlgDim.truckCenterOffset = 0;
				carDlgIsLoco = (typeListMap[carDlgTypeInx].value&1);
			} else {
				strcpy( carDlgProtoStr, carDlgUpdateProtoPtr->desc );
				carDlgDim = carDlgUpdateProtoPtr->dim;
				carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
				carDlgIsLoco = (carDlgUpdateProtoPtr->options&CAR_DESC_IS_LOCO)!=0;
				carDlgTypeInx = CarProtoFindTypeCode( carDlgUpdateProtoPtr->type );
				carProtoSegCnt = carDlgUpdateProtoPtr->segCnt;
				carProtoSegPtr = carDlgUpdateProtoPtr->segPtr;
				currState = S_ProtoSel;
			}
			RELOAD_DIMS;
			break;
		case A_RecallCouplerLength:
			sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr,
			         GetScaleName(carDlgScaleInx) );
			carDlgCouplerLength = 16.0/GetScaleRatio(carDlgScaleInx);
			wPrefGetFloat( carDlgPG.nameStr, message, &carDlgCouplerLength,
			               carDlgCouplerLength );
			break;
		default:
			CHECKMSG( FALSE, ( "carDlgDoActions: bad action %d", (int)(actions[-1]) ) );
			break;
		}
	}
}


static void CarDlgDoStateActions(
        carDlgAction_e * actions )
{
	CarDlgDoActions( actions );
	LOG( log_carDlgState, 1, ( " ==> S_%s\n", carDlgState_s[currState] ) )
}

static void CarDlgStateMachine(
        carDlgTransistion_e transistion )
{
	LOG( log_carDlgState, 1, ( "S_%s[T_%s]\n", carDlgState_s[currState],
	                           carDlgTransistion_s[transistion] ) )
	CarDlgDoStateActions( stateMachine[currState][transistion] );
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


void CarDlgError(
        wBool_t ok,
        paramData_p p,
        char * msg )
{
	p->bInvalid = !ok;
	ParamHilite( p->group->win, p->control, !ok );
	wWinPix_t h = wControlGetHeight(p->control);
	wControlSetBalloon( p->control, 0, -h*3/4, ok?NULL:msg );
}


static void CarDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	BOOL_T redraw = FALSE;
	roadnameMap_p roadnameMapP;
	char * cp, *cq;
	long valL, d, m;
	FLOAT_T ratio;
	BOOL_T ok = TRUE;
	DIST_T len;
	BOOL_T checkTruckCenter = FALSE;
	cmp_key_t cmp_key;
	coOrd orig, size, size2;
	carPartParent_p parentP;
#ifdef TRUCK_OFFSET
	static DIST_T carDlgTruckOffsetL;
	static DIST_T carDlgTruckOffsetR;
#endif
	static long carDlgClock;
	static long carDlgCarLengthClock;
	static long carDlgTruckCenterClock;
	static long carDlgCoupledLengthClock;
	static long carDlgCouplerLengthClock;

	ratio = (S_PROTO?1.0:GetScaleRatio(carDlgScaleInx));

	LOG( log_carDlgState, 3, ( "CarDlgUpdate( %d )\n", inx ) )

	switch ( inx ) {

	case -1:
#ifdef TRUCK_OFFSET
		if ( carDlgDim.truckCenter > 0
		     && carDlgDim.carLength > carDlgDim.truckCenter ) {
			carDlgTruckOffsetL = (carDlgDim.carLength - carDlgDim.truckCenter)/2 -
			                     carDlgDim.truckCenterOffset;
			carDlgTruckOffsetR = (carDlgDim.carLength - carDlgDim.truckCenter)/2 +
			                     carDlgDim.truckCenterOffset;
		} else {
			carDlgTruckOffsetL = 0;
			carDlgTruckOffsetR = 0;
		}
#endif

		carDlgCarLengthClock = carDlgCoupledLengthClock = carDlgTruckCenterClock =
		                               carDlgCouplerLengthClock = carDlgClock = 0;
#ifdef TRUCK_OFFSET
		LOG( log_carDlgDims, 1,
		     ( "truckCenter(%d): trkCenter:%0.3f, .carLength:%0.3f = trackOffset: %0.3f %0.3f\n",
		       carDlgDim.truckCenter,
		       carDlgDim.carLength,
		       carDlgTruckOffsetL,
		       carDlgTruckOffsetR ) );
#endif
		redraw = TRUE;
		break;

	case I_CD_MANUF_LIST:
		carDlgChanged++;
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgManufStr,
		                sizeof carDlgManufStr, NULL, NULL );
		if ( carDlgManufInx < 0 ||
		     wListGetItemContext( (wList_p)pg->paramPtr[inx].control,
		                          carDlgManufInx ) == NULL ) {
			CarDlgStateMachine( T_ItemEnter );
		}
#ifdef LATER
		else if ( strcasecmp( carDlgManufStr, "unknown" ) == 0 ||
		          strcasecmp( carDlgManufStr, "custom" ) == 0 ) {
			CarDlgStateMachine( T_ItemEnter );
		}
#endif
		else {
			CarDlgStateMachine( T_ItemSel );
		}
		/*ParamControlShow( &carDlgPG, I_CD_MANUF_LIST, TRUE );*/
		break;

	case I_CD_PROTOKIND_LIST:
		carDlgChanged++;
		carDlgTypeInx = (int)VP2L(wListGetItemContext( (wList_p)
		                          pg->paramPtr[inx].control, carDlgKindInx ));
		if ( S_PART || (currState==S_ItemEnter) ) {
			CarDlgLoadProtoList( NULL, 0, FALSE );
		} else {
			parentP = NULL;
			if ( carDlgProtoInx >= 0 ) {
				parentP = (carPartParent_p)wListGetItemContext( (wList_p)
				                pg->paramPtr[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			}
			CarDlgLoadProtoList( carDlgManufStr, (parentP?parentP->scale:0), FALSE );
		}
		CarDlgStateMachine( T_ProtoSel );
		break;

	case I_CD_PROTOTYPE_LIST:
		carDlgChanged++;
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgProtoStr,
		                sizeof carDlgProtoStr, NULL, NULL );
		CarDlgStateMachine( T_ProtoSel );
		break;

	case I_CD_PARTNO_LIST:
		carDlgChanged++;
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgPartnoStr,
		                sizeof carDlgPartnoStr, NULL, NULL );
		if ( carDlgPartnoInx >= 0 ) {
			CarDlgStateMachine( T_PartnoSel );
		} else {
			CarDlgStateMachine( T_PartnoEnter );
			wControlSetFocus( pg->paramPtr[I_CD_PARTNO_STR].control );
		}
		break;

	case I_CD_DISPMODE:

		if ( !ParamCheckInputs( &carDlgPG, carDlgPLs[I_CD_DISPMODE].control ) ) {
			carDlgDispMode = 1-carDlgDispMode;
			ParamLoadControl( &carDlgPG, I_CD_DISPMODE );
			break;
		}
		for ( inx=B; inx<C; inx++ ) {
			ParamControlShow( &carDlgPG, inx, carDlgDispMode==1 );
		}
		for ( inx=C; inx<D; inx++ ) {
			ParamControlShow( &carDlgPG, inx, carDlgDispMode==0 );
		}
		if ( carDlgDispMode == 0 && carDlgUpdateItemPtr != NULL ) {
			ParamControlShow( &carDlgPG, I_CD_QTY, FALSE );
			ParamControlShow( &carDlgPG, I_CD_MLTNUM, FALSE );
		}
		redraw = carDlgDispMode==1;
		break;

	case I_CD_ROADNAME_LIST:
		carDlgChanged++;
		roadnameMapP = NULL;
		if ( *(long*)valueP == 0 ) {
			roadnameMapP = NULL;
			carDlgRoadnameStr[0] = '\0';
		} else if ( *(long*)valueP > 0 ) {
			roadnameMapP = (roadnameMap_p)wListGetItemContext( (wList_p)
			                pg->paramPtr[I_CD_ROADNAME_LIST].control, (wIndex_t)*(long*)valueP );
			strcpy( carDlgRoadnameStr, roadnameMapP->roadname );
		} else {
			wListGetValues( (wList_p)pg->paramPtr[I_CD_ROADNAME_LIST].control,
			                carDlgRoadnameStr, sizeof carDlgRoadnameStr, NULL, NULL );
			cmp_key.name = carDlgRoadnameStr;
			cmp_key.len = (int)strlen(carDlgRoadnameStr);
			roadnameMapP = LookupListElem( &roadnameMap_da, &cmp_key, Cmp_roadnameMap, 0 );
		}
		if ( roadnameMapP ) {
			strcpy( carDlgRepmarkStr, roadnameMapP->repmark );
		} else {
			carDlgRepmarkStr[0] = '\0';
		}
		ParamLoadControl( pg, I_CD_REPMARK );
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
				ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
			}
			carDlgCarLengthClock = ++carDlgClock;
		} else if ( carDlgDim.coupledLength != 0 && ( carDlgCouplerLength == 0
		                || carDlgCoupledLengthClock > carDlgCouplerLengthClock ) ) {
			len = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
			if ( len > 0 ) {
				carDlgCouplerLength = len;
				ParamLoadControl( &carDlgPG, I_CD_CPLRLEN );
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
				ParamLoadControl( &carDlgPG, I_CD_CPLRLEN );
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
				ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
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
				ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
			}
			carDlgCouplerLengthClock = ++carDlgClock;
		} else if ( carDlgDim.coupledLength != 0 && ( carDlgDim.carLength == 0
		                || carDlgCoupledLengthClock > carDlgCarLengthClock ) ) {
			len = carDlgDim.coupledLength-carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.carLength = len;
				ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
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

	case I_CD_ISLOCO:
		carDlgChanged++;
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
#ifdef TRUCK_OFFSET
		LOG( log_carDlgDims, 1,
		     ("I_CD_TRKOFFSET: len:%0.3f, center:%0.3f, cenoff:%0.3f = offset:%0.3f %0.3f\n",
		      carDlgDim.carLength,
		      carDlgDim.truckCenter,
		      carDlgDim.truckCenterOffset,
		      carDlgTruckOffsetL,
		      carDlgTruckOffsetR ) );
#endif
		break;

	case I_CD_TRKCENTER:
		carDlgChanged++;
#ifdef TRUCK_OFFSET
		if ( carDlgDim.truckCenter == 0 ||
		     carDlgDim.truckCenterOffset == 0.0 ) {
			carDlgTruckOffsetL = 0;
			carDlgTruckOffsetR = 0;
		} else if ( carDlgDim.truckCenter <
		            100/ratio /*&& carDlgDim.carLength == 0.0*/ ) {
			carDlgTruckOffsetL = 0;
			carDlgTruckOffsetR = 0;
			return;
		} else if ( carDlgDim.carLength - carDlgDim.truckCenter > 2*fabs(
		                    carDlgDim.truckCenterOffset) ) {
			carDlgTruckOffsetL = (carDlgDim.carLength-carDlgDim.truckCenter)/2 -
			                     carDlgDim.truckCenterOffset;
			carDlgTruckOffsetR = (carDlgDim.carLength-carDlgDim.truckCenter)/2 +
			                     carDlgDim.truckCenterOffset;
		} else {
			carDlgTruckOffsetL = 0;
			carDlgTruckOffsetR = 0;
		}
		LOG( log_carDlgDims, 1,
		     ("I_CD_TRKCENTER: len:%0.3f, center:%0.3f, cenoff:%0.3f = offset:%0.3f %0.3f\n",
		      carDlgDim.carLength,
		      carDlgDim.truckCenter,
		      carDlgDim.truckCenterOffset,
		      carDlgTruckOffsetL,
		      carDlgTruckOffsetR ) );
#endif
		redraw = TRUE;
		break;

	case I_CD_QTY:
		wControlActive( carDlgPLs[I_CD_MLTNUM].control, carDlgQuantity>1 );
		break;

	case I_CD_PURPRC:
	case I_CD_CURPRC:
		carDlgChanged++;
		*(FLOAT_T*)(pg->paramPtr[inx].context) = strtod( (char*)
		                pg->paramPtr[inx].valueP, &cp );
		if ( cp==NULL || *cp!='\0' ) {
			*(FLOAT_T*)(pg->paramPtr[inx].context) = -1;
			ok = FALSE;
			sprintf( message, "%s not valid", pg->paramPtr[inx].winLabel );
		}
		CarDlgError( ok, &pg->paramPtr[inx], message );
		break;

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
	case I_CD_SRVDAT:
		carDlgChanged++;
		for ( cp = (char*)pg->paramPtr[inx].valueP; *cp
		      && isspace(*(unsigned char*)cp); cp++ );
		if ( *cp ) {
			valL = strtol( cp, &cq, 10 );
			if ( cq==NULL || *cq !='\0' ) {
				cp = N_("Enter a 8 digit numeric date (yyyymmdd)");
				ok = FALSE;
			} else {
				if ( strlen(cp) != 8  || valL == 0) {
					cp = N_("Enter a 8 digit numeric date (yyyymmdd)");
					ok = FALSE;
				} else if ( valL < 19000101 || valL > 21991231 ) {
					cp = N_("Enter a date between 19000101 and 21991231");
					ok = FALSE;
				} else {
					d = valL % 100;
					m = (valL / 100) % 100;
					if ( m < 1 || m > 12 ) {
						cp = N_("Invalid month");
						ok = FALSE;
					} else if ( d < 1 || d > 31 ) {
						cp = N_("Invalid day");
						ok = FALSE;
					} else {
						cp = NULL;
					}
				}
			}
			if ( !ok ) {
				valL = -1;
			}
		} else {
			cp = NULL;
			valL = 0;
		}
		CarDlgError( ok, &pg->paramPtr[inx], cp );
		if (inx == I_CD_PURDAT) {
			carDlgPurchDate = valL;
		} else {
			carDlgServiceDate = valL;
		}
		break;

	case I_CD_TYPE_LIST:
		carDlgChanged++;
		carDlgIsLoco = (typeListMap[carDlgTypeInx].value&1);
		ParamLoadControl( &carDlgPG, I_CD_ISLOCO );
		redraw = TRUE;
		break;

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
		size2.x = floor(size.x*curScaleRatio+0.5);
		size2.y = floor(size.y*curScaleRatio+0.5);
		RescaleSegs( carProtoSegCnt, carProtoSegPtr, size2.x/size.x, size2.y/size.y,
		             curScaleRatio );
		carDlgDim.carLength = size2.x;
		carDlgDim.carWidth = size2.y;
		carDlgDim.coupledLength = carDlgDim.carLength + 32;
#ifdef TRUCK_OFFSET
		if ( carDlgDim.carLength > 120 ) {
			carDlgDim.truckCenter = carDlgDim.carLength - 120;
			carDlgTruckOffsetL = (carDlgDim.carLength - carDlgDim.truckCenter)/2;
			carDlgTruckOffsetR = (carDlgDim.carLength - carDlgDim.truckCenter)/2;
		} else {
			carDlgDim.truckCenter = 0;
			carDlgTruckOffsetL = 0;
			carDlgTruckOffsetR = 0;
		}
#endif
		carDlgFlipToggle = FALSE;
		ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
		ParamLoadControl( &carDlgPG, I_CD_CARWIDTH );
		ParamLoadControl( &carDlgPG, I_CD_CPLRLEN );
		ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
		ParamLoadControl( &carDlgPG, I_CD_TRKOFFSET );
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

	}

	if ( checkTruckCenter && carDlgDim.carLength > 0 ) {
#ifdef TRUCK_OFFSET
		DIST_T offL = (carDlgDim.carLength - carDlgDim.truckCenter)/2 -
		              carDlgDim.truckCenterOffset;
		DIST_T offR = (carDlgDim.carLength - carDlgDim.truckCenter)/2 +
		              carDlgDim.truckCenterOffset;
		LOG( log_carDlgDims, 1, ( "OFF %0.3f %0.3f %0.3f %0.3f\n",
		                          carDlgTruckOffsetL, carDlgTruckOffsetR, offL, offR ) );

		if ( carDlgTruckOffsetL > 0 || carDlgTruckOffsetR > 0 ) {
			carDlgDim.truckCenter = carDlgDim.carLength - ( carDlgTruckOffsetL +
			                        carDlgTruckOffsetR );
			carDlgDim.truckCenterOffset = (carDlgTruckOffsetR - carDlgTruckOffsetL)/2;
		} else {
			carDlgDim.truckCenter = carDlgDim.carLength * 0.75;
			carDlgDim.truckCenterOffset = 0;
		}
		LOG( log_carDlgDims, 1,
		     ( "checkTruckCenter length:%0.3f, offset %0.3f, %0.3f = truckCenter:%0.3f, truckCenterOffset:%0.3f\n",
		       carDlgDim.carLength, carDlgTruckOffsetL, carDlgTruckOffsetR,
		       carDlgDim.truckCenter, carDlgDim.truckCenterOffset ) );
#endif
		ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
		ParamLoadControl( &carDlgPG, I_CD_TRKOFFSET );
	}

	if ( S_PART && carDlgManufStr[0] == '\0' ) {
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Select or Enter a Manufacturer") );
	} else if ( S_ITEM && carDlgUpdateItemPtr==NULL &&
	            ( valL = carDlgItemIndex, !CheckCarDlgItemIndex(&carDlgItemIndex) ) ) {
		sprintf( message,
		         _("Item Index %ld duplicated an existing item: updated to new value"), valL );
		ParamLoadControl( &carDlgPG, I_CD_ITEMINDEX );
		ParamLoadMessage( &carDlgPG, I_CD_MSG, message );
		ok = TRUE;
	} else {
		ParamLoadMessage( pg, I_CD_MSG, "" );
		ok = TRUE;
	}

	if ( redraw ) {
		CarDlgRedraw( carDlgD.d, NULL, 0, 0 );
	}

	ParamDialogOkActive( pg, ok );
}



static void CarDlgNewDesc( void )
{
	carDlgNewPartPtr = NULL;
	carDlgNewProtoPtr = NULL;
	carDlgUpdatePartPtr = NULL;
	carDlgNumberStr[0] = '\0';
	ParamLoadControl( &carDlgPG, I_CD_NUMBER );
	CarDlgDoStateActions( item2partActions );
	carDlgChanged = 0;
}


static void CarDlgNewProto( void )
{
	carProto_p protoP = CarProtoFind( carDlgProtoStr );
	if ( protoP != NULL ) {
		carProtoSegCnt = protoP->segCnt;;
		carProtoSegPtr = protoP->segPtr;;
	} else {
		carProtoSegCnt = 0;
		carProtoSegPtr = NULL;
	}
	carDlgUpdateProtoPtr = NULL;
	carDlgNewProtoPtr = NULL;
	if ( S_ITEM ) {
		CarDlgDoStateActions( item2protoActions );
	} else {
		CarDlgDoStateActions( part2protoActions );
	}
	carDlgChanged = 0;
}


static void CarDlgClose( wWin_p win )
{
	carDlgState_e oldState;

	if ( carDlgChanged ) {
		if ( !inPlayback ) {
			if ( NoticeMessage( MSG_CARDESC_CHANGED, _("Yes"), _("No") ) <= 0 ) {
				return;
			}
		} else {
			PlaybackMessage( "Car Desc Changed\n" );
		}
	}
	if ( carDlgStkPtr > 0 ) {
		carDlgStkPtr--;
		oldState = currState;
		currState = carDlgStk[carDlgStkPtr].state;
		carDlgChanged = carDlgStk[carDlgStkPtr].changed;
		if ( oldState == S_ProtoSel )
			if ( S_PART ) {
				CarDlgDoStateActions( proto2partActions );
			} else {
				CarDlgDoStateActions( proto2itemActions );
			} else {
			CarDlgDoStateActions( part2itemActions );
		}
	} else {
		wTextClear( (wText_p)carDlgPLs[I_CD_NOTES].control );
		wHide( carDlgPG.win );
	}
}


static void CarDlgOk( void * unused )
{
	long options = 0;
	int len;
	FILE * f;
	long number;
	char * cp;
	long count;
	tabString_t tabs[7];
	char title[STR_LONG_SIZE*2];
	carItem_p itemP=NULL;
	carPart_p partP=NULL;
	carProto_p protoP;
	BOOL_T reloadRoadnameList = FALSE;

	LOG( log_carDlgState, 3, ( "CarDlgOk()\n" ) )

	ParamUpdate(&carDlgPG);

	/*ParamUpdate( &carDlgPG );*/
	if ( carDlgDim.carLength <= 0.0 ||
	     carDlgDim.carWidth <= 0.0 ||
	     carDlgDim.truckCenter <= 0.0 ||
	     carDlgDim.truckCenterOffset < 0.0 ||
	     carDlgDim.coupledLength <= 0.0 ) {
		NoticeMessage( MSG_CARDESC_VALUE_ZERO, _("Ok"), NULL );
		return;
	}
	if ( carDlgDim.carLength <= carDlgDim.carWidth ||
	     carDlgDim.truckCenter >= carDlgDim.carLength ) {
		NoticeMessage( MSG_CARDESC_BAD_DIM_VALUE, _("Ok"), NULL );
		return;
	}
	if ( carDlgDim.coupledLength <= carDlgDim.carLength ) {
		NoticeMessage( MSG_CARDESC_BAD_COUPLER_LENGTH_VALUE, _("Ok"), NULL );
		return;
	}

	if ( S_ITEM && carDlgUpdateItemPtr==NULL
	     && !CheckCarDlgItemIndex(&carDlgItemIndex) ) {
		NoticeMessage( MSG_CARITEM_BAD_INDEX, _("Ok"), NULL );
		ParamLoadControl( &carDlgPG, I_CD_ITEMINDEX );
		return;
	}

	if ( S_ITEM && (carDlgPurchDate<0 || carDlgServiceDate<0 || carDlgPurchPrice <0
	                || carDlgCurrPrice<0)) { return; }

	if ( S_PROTO && carDlgProtoStr[0] == '\0' ) { return; }

	if ( S_PART && (carDlgManufStr[0] == '\0' || carDlgPartnoStr[0] == '\0')) { return; }

	if ( S_ITEM && carDlgItemIndex <= 0 ) { return; }

	if ( (!S_PROTO) && carDlgCouplerMount != 0 ) {
		options |= CAR_DESC_COUPLER_MODE_BODY;
	}
	if ( carDlgIsLoco == 1 ) {
		options |= CAR_DESC_IS_LOCO;
	}

	if ( S_ITEM ) {
		len = wTextGetSize( (wText_p)carDlgPLs[I_CD_NOTES].control );
		sprintf( title, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr,
		         carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr,
		         carDlgNumberStr );
		partP = NULL;
		if ( ( carDlgManufInx < 0 || carDlgPartnoInx < 0 ) && carDlgPartnoStr[0] ) {
			partP = CarPartFind( carDlgManufStr, (int)strlen(carDlgManufStr),
			                     carDlgPartnoStr, (int)strlen(carDlgPartnoStr), carDlgScaleInx );
			if ( partP != NULL &&
			     NoticeMessage( MSG_CARPART_DUPNAME, _("Yes"), _("No") ) <= 0 ) {
				return;
			}
			partP = CarPartNew( NULL, PARAM_CUSTOM, carDlgScaleInx, title, options,
			                    typeListMap[carDlgTypeInx].value, &carDlgDim, carDlgBodyColor );
			if ( partP != NULL ) {
				if ( ( f = OpenCustom("a") ) ) {
					SetCLocale();
					CarPartWrite( f, partP );
					fclose(f);
					SetUserLocale();
				}
			}
		}
		if ( carDlgUpdateItemPtr!=NULL ) {
			carDlgQuantity = 1;
		}
		for ( count=0; count<carDlgQuantity; count++ ) {
			itemP = CarItemNew( carDlgUpdateItemPtr,
			                    PARAM_CUSTOM, carDlgItemIndex,
			                    carDlgScaleInx, title, options, typeListMap[carDlgTypeInx].value,
			                    &carDlgDim, carDlgBodyColor,
			                    carDlgPurchPrice, carDlgCurrPrice, carDlgCondition,
			                    carDlgPurchDate, carDlgServiceDate );
			if ( carDlgUpdateItemPtr==NULL ) {
				wPrefSetInteger( "misc", "last-car-item-index", carDlgItemIndex );
				carDlgItemIndex++;
				CheckCarDlgItemIndex(&carDlgItemIndex);
				ParamLoadControl( &carDlgPG, I_CD_ITEMINDEX );
				if ( carDlgQuantity>1 && carDlgMultiNum==0 ) {
					number = strtol( carDlgNumberStr, &cp, 10 );
					if ( cp && *cp == 0 && number > 0 ) {
						sprintf( carDlgNumberStr, "%ld", number+1 );
						sprintf( title, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr,
						         carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr,
						         carDlgNumberStr );
					}
				}
			}
			if ( len > 0 ) {
				if ( itemP->data.notes ) {
					itemP->data.notes = MyRealloc( itemP->data.notes, (len+2) * sizeof(wchar_t) );
				} else {
					itemP->data.notes = MyMalloc( (len+2) * sizeof(wchar_t) );
				}
				// itemP->data.notes = (char*)MyMalloc( (len+2) * sizeof(wchar_t) );
				wTextGetText( (wText_p)carDlgPLs[I_CD_NOTES].control, itemP->data.notes, len );
				if ( itemP->data.notes[len-1] != '\n' ) {
					itemP->data.notes[len] = '\n';
					itemP->data.notes[len+1] = '\0';
				} else {
					itemP->data.notes[len] = '\0';
				}
			} else if ( itemP->data.notes ) {
				MyFree( itemP->data.notes );
				itemP->data.notes = NULL;
			}
		}
		if ( carDlgUpdateItemPtr==NULL ) {
			CarInvListAdd( itemP );
		} else {
			CarInvListUpdate( itemP );
		}
		SetFileChanged();
		reloadRoadnameList = TRUE;
		if ( carDlgUpdateItemPtr==NULL ) {
			if ( carDlgQuantity > 1 ) {
				sprintf( message, _("Added %ld new Cars"), carDlgQuantity );
			} else {
				strcpy( message, _("Added new Car") );
			}
		} else {
			strcpy( message, _("Updated Car") );
		}
		sprintf( message+strlen(message), "%s: %s %s %s %s %s %s",
		         (partP?_(" and Part"):""),
		         carDlgManufStr, carDlgPartnoStr, carDlgProtoStr, carDlgDescStr,
		         (carDlgRepmarkStr[ 0 ]?carDlgRepmarkStr:carDlgRoadnameStr), carDlgNumberStr );
		carDlgQuantity = 1;
		ParamLoadControl( &carDlgPG, I_CD_QTY );

	} else if ( S_PART ) {
		if ( strcasecmp( carDlgRoadnameStr, "undecorated" ) == 0 ) {
			carDlgRoadnameStr[0] = '\0';
			carDlgRepmarkStr[0] = '\0';
		}
		if ( carDlgUpdatePartPtr==NULL ) {
			partP = CarPartFind( carDlgManufStr, (int)strlen(carDlgManufStr),
			                     carDlgPartnoStr, (int)strlen(carDlgPartnoStr), carDlgScaleInx );
			if ( partP != NULL &&
			     NoticeMessage( MSG_CARPART_DUPNAME, _("Yes"), _("No") ) <= 0 ) {
				return;
			}
		}
		sprintf( message, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr,
		         carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr,
		         carDlgNumberStr );
		carDlgNewPartPtr = CarPartNew( carDlgUpdatePartPtr, PARAM_CUSTOM,
		                               carDlgScaleInx, message, options, typeListMap[carDlgTypeInx].value,
		                               &carDlgDim, carDlgBodyColor );
		if ( carDlgNewPartPtr != NULL && ( f = OpenCustom("a") ) ) {
			SetCLocale();
			CarPartWrite( f, carDlgNewPartPtr );
			fclose(f);
			SetUserLocale();
		}
		reloadRoadnameList = TRUE;
		sprintf( message, _("%s Part: %s %s %s %s %s %s"),
		         carDlgUpdatePartPtr==NULL?_("Added new"):_("Updated"), carDlgManufStr,
		         carDlgPartnoStr, carDlgProtoStr, carDlgDescStr,
		         carDlgRepmarkStr[ 0 ]?carDlgRepmarkStr:carDlgRoadnameStr, carDlgNumberStr );

	} else if ( S_PROTO ) {
		if ( carDlgUpdateProtoPtr==NULL ) {
			protoP = CarProtoFind( carDlgProtoStr );
			if ( protoP != NULL &&
			     NoticeMessage( MSG_CARPROTO_DUPNAME, _("Yes"), _("No") ) <= 0 ) {
				return;
			}
		}
		carDlgNewProtoPtr = CarProtoNew( carDlgUpdateProtoPtr, PARAM_CUSTOM,
		                                 carDlgProtoStr, options, typeListMap[carDlgTypeInx].value, &carDlgDim,
		                                 carDlgSegs_da.cnt, &carDlgSegs(0) );
		if ( (f = OpenCustom("a") ) ) {
			SetCLocale();
			CarProtoWrite( f, carDlgNewProtoPtr );
			fclose(f);
			SetUserLocale();
		}
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
		ParamLoadControl( &carDlgPG, I_CD_ROADNAME_LIST );
	}

	ParamLoadMessage( &carDlgPG, I_CD_MSG, message );

	DoChangeNotification( CHANGE_PARAMS );

	carDlgChanged = 0;
	if ( S_ITEM ) {
		if ( carDlgUpdateItemPtr==NULL ) {
			if ( partP ) {
				TabStringExtract( title, 7, tabs );
				if ( CarDlgLoadLists( TRUE, tabs, GetLayoutCurScale()) ) {
					currState = S_ItemSel;
				} else {
					currState = S_ItemEnter;
				}
				ParamLoadControl( &carDlgPG, I_CD_MANUF_LIST );
				ParamLoadControl( &carDlgPG, I_CD_PROTOKIND_LIST );
				ParamLoadControl( &carDlgPG, I_CD_PROTOTYPE_LIST );
				ParamLoadControl( &carDlgPG, I_CD_PARTNO_LIST );
				ParamLoadControl( &carDlgPG, I_CD_PARTNO_STR );
				ParamLoadControl( &carDlgPG, I_CD_DESC_STR );
				ParamControlShow( &carDlgPG, I_CD_PARTNO_LIST, carDlgPartnoInx>=0 );
				ParamControlShow( &carDlgPG, I_CD_PARTNO_STR, carDlgPartnoInx<0 );
				ParamControlShow( &carDlgPG, I_CD_DESC_STR, carDlgPartnoInx<0 );
			} else if ( carDlgManufInx == -1 ) {
				carDlgManufStr[0] = '\0';
			}
			return;
		}
	} else if ( S_PART ) {
		if ( carDlgUpdatePartPtr==NULL ) {
			number = strtol( carDlgPartnoStr, &cp, 10 );
			if ( cp && *cp == 0 && number > 0 ) {
				sprintf( carDlgPartnoStr, "%ld", number+1 );
			} else {
				carDlgPartnoStr[0] = '\0';
			}
			carDlgNumberStr[0] = '\0';
			ParamLoadControl( &carDlgPG, I_CD_PARTNO_STR );
			ParamLoadControl( &carDlgPG, I_CD_NUMBER );
			return;
		}
	} else if ( S_PROTO ) {
		if ( carDlgUpdateProtoPtr==NULL ) {
			carDlgProtoStr[0] = '\0';
			ParamLoadControl( &carDlgPG, I_CD_PROTOTYPE_STR );
			return;
		}
	}
	CarDlgClose( carDlgPG.win );
}



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
	case C:
		*yy = wControlGetPosY(carDlgPLs[B].control);
		break;
	case I_CD_MSG:
		y0 = wControlBelow(carDlgPLs[C-1].control);
		y1 = wControlBelow(carDlgPLs[D-1].control);
		*yy = ((y0>y1)?y0:y1) + 10;
		break;
	}
}


void DoCarPartDlg( carDlgAction_e *actions )
{
	paramData_t * pd;
	int inx;

	if ( carDlgPG.win == NULL ) {
		ParamCreateDialog( &carDlgPG, MakeWindowTitle(_("New Car Part")), _("Add"),
		                   CarDlgOk, ParamCancel_Custom( CarDlgClose ),
		                   TRUE, CarDlgLayout,
		                   F_BLOCK|F_RESIZE|F_RECALLSIZE|PD_F_ALT_CANCELLABEL, CarDlgUpdate );

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

		for ( inx=0; inx<N_CONDLISTMAP; inx++ ) {
			wListAddValue( (wList_p)carDlgPLs[I_CD_COND].control, _(condListMap[inx].name),
			               NULL, I2VP(condListMap[inx].value) );
		}

		for ( inx=0; inx<N_TYPELISTMAP; inx++ ) {
			wListAddValue( (wList_p)carDlgPLs[I_CD_TYPE_LIST].control,
			               _(typeListMap[inx].name), NULL, I2VP(typeListMap[inx].value) );
		}

		for ( inx=0; inx<N_TYPELISTMAP; inx++ ) {
			wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOKIND_LIST].control,
			               _(typeListMap[inx].name), NULL, I2VP(typeListMap[inx].value) );
		}

		wTextSetReadonly( (wText_p)carDlgPLs[I_CD_NOTES].control, FALSE );
	}

	wPrefGetInteger( "misc", "last-car-item-index", &carDlgItemIndex, 1 );
	CheckCarDlgItemIndex(&carDlgItemIndex);
	CarDlgLoadRoadnameList();
	carProtoSegCnt = 0;
	carProtoSegPtr = NULL;
	carDlgScaleInx = GetLayoutCurScale();
	carDlgFlipToggle = FALSE;
	carDlgChanged = 0;
	for ( paramData_p p=carDlgPLs; p < carDlgPLs + COUNT( carDlgPLs ); p++ ) {
		p->bInvalid = FALSE;
	}

	CarDlgDoStateActions( actions );

	wShow( carDlgPG.win );
}


EXPORT void CarDlgAddProto( void )
{
	/*carDlgPrototypeStr[0] = 0; */
	carDlgTypeInx = 0;
	carDlgUpdateProtoPtr = NULL;
	DoCarPartDlg( protoNewActions );
}

EXPORT void CarDlgAddDesc( void )
{
	if ( carProto_da.cnt <= 0 ) {
		NoticeMessage( MSG_NO_CARPROTO, _("Ok"), NULL );
		return;
	}
	carDlgIsLoco = FALSE;
	carDlgUpdatePartPtr = NULL;
	carDlgNumberStr[0] = '\0';
	ParamLoadControl( &carDlgPG, I_CD_NUMBER );
	DoCarPartDlg( partNewActions );
}














static void CarDlgChange( long changes )
{
	if ( (changes&CHANGE_SCALE) ) {
		carDlgCouplerLength = 0.0;
	}
}


EXPORT void ClearCars( void )
{
	int inx;
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ ) {
		MyFree( carItemInfo(inx) );
	}
	DYNARR_FREE( carItem_t*, carItemInfo_da );
}


static struct {
	dynArr_t carProto_da;
	dynArr_t carPartParent_da;
	dynArr_t carItemInfo_da;
} savedCarState;

EXPORT void SaveCarState( void )
{
	savedCarState.carProto_da = carProto_da;
	savedCarState.carPartParent_da = carPartParent_da;
	savedCarState.carItemInfo_da = carItemInfo_da;
	DYNARR_INIT( carItem_t*, carItemInfo_da );
}


EXPORT void RestoreCarState( void )
{
#ifdef LATER
	carProto_da = savedCarState.carProto_da;
	carPartParent_da = savedCarState.carPartParent_da;
#endif
	carItemInfo_da = savedCarState.carItemInfo_da;
}



EXPORT void InitCarDlg( void )
{
	log_carList = LogFindIndex( "carList" );
	log_carInvList = LogFindIndex( "carInvList" );
	log_carDlgState = LogFindIndex( "carDlgState" );
	log_carDlgList = LogFindIndex( "carDlgList" );
	log_carDlgDims = LogFindIndex( "carDlgDims" );
	carDlgBodyColor = wDrawFindColor( wRGB(255,128,0) );
	ParamRegister( &carDlgPG );
	ParamRegister( &carInvPG );
	RegisterChangeNotification( CarDlgChange );
	AddParam( "CARPROTO ", CarProtoRead );
	AddParam( "CARPART ", CarPartRead);
	ParamRegister( &newCarPG );
	ParamCreateControls( &newCarPG, CarItemHotbarUpdate );
	newCarControls[0] = newCarPLs[0].control;
}

/*****************************************************************************
 *
 * Custom Management Support
 *
 */

static int CarPartCustMgmProc(
        int cmd,
        void * data )
{
	tabString_t tabs[7];
	int rd_inx;

	carPart_p partP = (carPart_p)data;
	switch ( cmd ) {
	case CUSTMGM_DO_COPYTO:
		return CarPartWrite( customMgmF, partP );
	case CUSTMGM_CAN_EDIT:
		return TRUE;
	case CUSTMGM_DO_EDIT:
		if ( partP == NULL ) {
			return FALSE;
		}
		carDlgUpdatePartPtr = partP;
		DoCarPartDlg( partUpdActions );
		return TRUE;
	case CUSTMGM_CAN_DELETE:
		return TRUE;
	case CUSTMGM_DO_DELETE:
		CarPartDelete( partP );
		return TRUE;
	case CUSTMGM_GET_TITLE:
		TabStringExtract( partP->title, 7, tabs );
		rd_inx = T_REPMARK;
		if ( tabs[T_REPMARK].len == 0 ) {
			rd_inx = T_ROADNAME;
		}
		sprintf( message, "\t%s\t%s\t%.*s\t%s%s%.*s%s%.*s%s%.*s",
		         partP->parent->manuf,
		         GetScaleName(partP->parent->scale),
		         tabs[T_PART].len, tabs[T_PART].ptr,
		         partP->parent->proto,
		         tabs[T_DESC].len?", ":"", tabs[T_DESC].len, tabs[T_DESC].ptr,
		         tabs[rd_inx].len?", ":"", tabs[rd_inx].len, tabs[rd_inx].ptr,
		         tabs[T_NUMBER].len?" ":"", tabs[T_NUMBER].len, tabs[T_NUMBER].ptr );
		return TRUE;
	}
	return FALSE;
}


static int CarProtoCustMgmProc(
        int cmd,
        void * data )
{
	carProto_p protoP = (carProto_p)data;
	switch ( cmd ) {
	case CUSTMGM_DO_COPYTO:
		return CarProtoWrite( customMgmF, protoP );
	case CUSTMGM_CAN_EDIT:
		return TRUE;
	case CUSTMGM_DO_EDIT:
		if ( protoP == NULL ) {
			return FALSE;
		}
		carDlgUpdateProtoPtr = protoP;
		DoCarPartDlg( protoUpdActions );
		return TRUE;
	case CUSTMGM_CAN_DELETE:
		return TRUE;
	case CUSTMGM_DO_DELETE:
		CarProtoDelete( protoP );
		return TRUE;
	case CUSTMGM_GET_TITLE:
		sprintf( message, "\t%s\t\t%s\t%s", _("Prototype"),
		         _(typeListMap[CarProtoFindTypeCode(protoP->type)].name), protoP->desc );
		return TRUE;
	}
	return FALSE;
}


#include "bitmaps/carpart.image1"
#include "bitmaps/carproto.image1"

EXPORT void CarCustMgmLoad( void )
{
	long parentX, partX, protoX;
	carPartParent_p parentP;
	carPart_p partP;
	carProto_p carProtoP;
	static wIcon_p carpartI = NULL;
	static wIcon_p carprotoI = NULL;

	if ( carpartI == NULL ) {
		carpartI = wIconCreatePixMap( carpart_image1 );
	}
	if ( carprotoI == NULL ) {
		carprotoI = wIconCreatePixMap( carproto_image1 );
	}

	for ( parentX=0; parentX<carPartParent_da.cnt; parentX++ ) {
		parentP = carPartParent(parentX);
		for ( partX=0; partX<parentP->parts_da.cnt; partX++ ) {
			partP = carPart(parentP,partX);
			if ( partP->paramFileIndex != PARAM_CUSTOM ) {
				continue;
			}
			CustMgmLoad( carpartI, CarPartCustMgmProc, partP );
		}
	}

	for ( protoX=0; protoX<carProto_da.cnt; protoX++ ) {
		carProtoP = carProto(protoX);
		if ( carProtoP->paramFileIndex != PARAM_CUSTOM ) {
			continue;
		}
		if (carProtoP->paramFileIndex == PARAM_CUSTOM) {
			CustMgmLoad( carprotoI, CarProtoCustMgmProc, carProtoP );
		}
	}
}
