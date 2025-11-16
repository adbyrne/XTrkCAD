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

#include "include/cars.h"
#include "listelem.h"
#include "tabstring.h"
#include "carsprivate.h"

static int log_carList;

void SetCarDlgFieldInvalid(int index, BOOL_T bInvalid);

dynArr_t carItemHotbar_da;
static char* CarItemHotbarProc(hotBarProc_e op, void* data, drawCmd_p d,
                               coOrd* origP);

static char* FormatCarTitle(carItem_p item, long mode);


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
	for (mode = carHotbarModes[carHotbarModeInx], rc = 0; mode != 0
	     && rc == 0; mode >>= 4) {
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
			} else if (item1->type > item2->type) {
				rc = 1;
			} else {
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

	LOG(log_carList, 1, ("AddHotBarCarDesc/load carItemHB: carItemHB.cnt:%d\n",
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
		if (FIT_NONE == CompatibleScale(FIT_CAR, item1->scaleInx,
		                                GetLayoutCurScale())) {
			continue;
		}
		if ((carHotbarModes[carHotbarModeInx] & 0xF000) != 0 || (item0 == NULL
		                || Cmp_carHotbar(&item0, &item1) != 0)) {
			orig = zero;
			size.x = item1->dim.carLength;
			size.y = item1->dim.carWidth;
			LOG(log_carList, 1, ("AddHotBarElement( %d: %s\n", inx, item1->title));
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
	// InfoSubstituteControls(newCarControls, newCarLabels);

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

			// InfoSubstituteControls(newCarControls, newCarLabels);
			wWinGetSize(mainW, &w, &h);
			w -= wControlGetPosX(newCarControls[0]) + 4;
			if (w > 20) {
				wListSetSize((wList_p)newCarControls[0], w,
				             wControlGetHeight(newCarControls[0]));
			}
		} else {
			// InfoSubstituteControls(NULL, NULL);
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
	carItemInfo_da = savedCarState.carItemInfo_da;
}


EXPORT void InitCarDlg( void )
{
	log_carList = LogFindIndex( "carList" );

	InitCarEditDlg();
	InitCarInvDlg();
	InitCarPart();
	InitCarProto();

	ParamRegister( &newCarPG );
	ParamCreateControls( &newCarPG, CarItemHotbarUpdate );
	newCarControls[0] = newCarPLs[0].control;


}

/*****************************************************************************
 *
 * Custom Management Support
 *
 */



//#include "bitmaps/carpart.image1"
//#include "bitmaps/carproto.image1"

EXPORT void CarCustMgmLoad( void )
{
	long parentX, partX, protoX;
	carPartParent_p parentP;
	carPart_p partP;
	carProto_p carProtoP;
	static wIcon_p carpartI = NULL;
	static wIcon_p carprotoI = NULL;

	if ( carpartI == NULL ) {
		//carpartI = wIconCreatePixMap( carpart_image1 );
	}
	if ( carprotoI == NULL ) {
		//carprotoI = wIconCreatePixMap( carproto_image1 );
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

