/**
 * \file   caritem.c
 * \brief  User inventory
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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
#include "ctrain.h"
#include "fileio.h"
#include "trkseg.h"

#include "carsprivate.h"
#include "cars.h"

dynArr_t carItemInfo_da;


#define N_CONDLISTMAP			(6)
nameLongMap_t condListMap[N_CONDLISTMAP] = {
	{ N_("N/A"), 0 },
	{ N_("Mint"), 100 },
	{ N_("Excellent"), 80 },
	{ N_("Good"), 60 },
	{ N_("Fair"), 40 },
	{ N_("Poor"), 20 }
};

extern  int log_carDlgDims;

wIndex_t MapCondition(
        long conditionValue)
{
	if (conditionValue < 10) {
		return 0;
	} else if (conditionValue < 30) {
		return 5;
	} else if (conditionValue < 50) {
		return 4;
	} else if (conditionValue < 70) {
		return 3;
	} else if (conditionValue < 90) {
		return 2;
	} else {
		return 1;
	}
}


carItem_p CarItemNew(
        carItem_p item,
        int paramFileIndex,
        long itemIndex,
        SCALEINX_T scale,
        char* title,
        long options,
        long type,
        carDim_t* dim,
        wDrawColor color,
        FLOAT_T purchPrice,
        FLOAT_T currPrice,
        long condition,
        long purchDate,
        long serviceDate)
{
	tabString_t tabs[7];

	TabStringExtract(title, 7, tabs);
	if (paramFileIndex != PARAM_CUSTOM) {
		carPart_p partP;
		partP = CarPartFind(tabs[T_MANUF].ptr, tabs[T_MANUF].len, tabs[T_PART].ptr,
		                    tabs[T_PART].len, scale);
		if (partP == NULL) {
			CarPartNew(NULL, PARAM_LAYOUT, scale, title, options, type, dim, color);
		}
	}

	if (item == NULL) {
		DYNARR_APPEND(carItem_t*, carItemInfo_da, 10);
		item = (carItem_t*)MyMalloc(sizeof * item);
		carItemInfo(carItemInfo_da.cnt - 1) = item;
	} else {
		if (item->title) { MyFree(item->title); }
		if (item->data.number) { MyFree(item->data.number); }
	}
	item->index = itemIndex;
	item->scaleInx = scale;
	item->title = MyStrdup(title);
	item->contentsLabel = "Car Item";
	item->barScale = curBarScale > 0 ? curBarScale : (60.0 * 12.0 / curScaleRatio);
	item->options = options;
	item->type = type;
	item->dim = *dim;
	item->color = color;
	if (tabs[T_REPMARK].len > 0 || tabs[T_NUMBER].len > 0) {
		sprintf(message, "%.*s%s%.*s", tabs[T_REPMARK].len, tabs[T_REPMARK].ptr,
		        (tabs[T_REPMARK].len > 0
		         && tabs[T_NUMBER].len > 0) ? " " : "", tabs[T_NUMBER].len, tabs[T_NUMBER].ptr);
	} else {
		sprintf(message, "#%ld", item->index);
	}
	item->data.number = MyStrdup(message);
	item->data.purchPrice = purchPrice;
	item->data.currPrice = currPrice;
	item->data.condition = condition;
	item->data.purchDate = purchDate;
	item->data.serviceDate = serviceDate;
	item->data.notes = NULL;
	item->segCnt = 0;
	item->segPtr = NULL;
	LoadRoadnameList(&tabs[T_ROADNAME], &tabs[T_REPMARK]);
	return item;
}

EXPORT BOOL_T CarItemRead(
        char* line)
{
	long itemIndex;
	char scale[10];
	char* title;
	long options;
	long type;
	carDim_t dim;
	long rgb;
	FLOAT_T purchPrice = 0;
	FLOAT_T currPrice = 0;
	long condition = 0;
	long purchDate = 0;
	long serviceDate = 0;
	carItem_p item;
	char* cp;
	wIndex_t layer;
	coOrd pos;
	ANGLE_T angle;
	wIndex_t index;
	long longCenterOffset;
	char* sNote = NULL;

	if (!GetArgs(line + 4, "ls9qll" "ff0lffl" "fflll000000c",
	             &itemIndex, scale, &title, &options, &type,
	             &dim.carLength, &dim.carWidth, &longCenterOffset, &dim.truckCenter,
	             &dim.coupledLength, &rgb,
	             &purchPrice, &currPrice, &condition, &purchDate, &serviceDate, &cp)) {
		return FALSE;
	}
	dim.truckCenterOffset = longCenterOffset / 1000.0;
	if (paramVersion < VERSION_INLINENOTE) {
		if ((options & CAR_ITEM_HASNOTES)) {
			sNote = ReadMultilineText();
		}
	} else {
		if (!GetArgs(cp, "qc", &sNote, &cp)) {
			return FALSE;
		}
	}
	item = CarItemNew(NULL, curParamFileIndex, itemIndex, LookupScale(scale),
	                  title,
	                  options & (CAR_DESC_BITS | CAR_ITEM_BITS), type, &dim, wDrawFindColor(rgb),
	                  purchPrice, currPrice, condition, purchDate, serviceDate);
	if ((options & CAR_ITEM_HASNOTES)) {
		item->data.notes = sNote;
	}
	MyFree(title);
	if ((options & CAR_ITEM_ONLAYOUT)) {
		if (!GetArgs(cp, "dLpf",
		             &index, &layer, &pos, &angle)) {
			return FALSE;
		}
		if (!ReadSegs()) {
			return FALSE;
		}
		item->car = NewCar(index, item, pos, angle);
		SetTrkLayer(item->car, layer);
		SetEndPts(item->car, 2);
		ComputeBoundingBox(item->car);
	}
	return TRUE;
}


static BOOL_T CarItemWrite(
        FILE* f,
        carItem_t* item,
        BOOL_T layout)
{
	long options = (item->options & CAR_DESC_BITS);
	coOrd pos;
	ANGLE_T angle;
	BOOL_T rc = TRUE;
	long longCenterOffset = (long)(item->dim.truckCenterOffset * 1000);

	SetCLocale();

	if (item->data.notes && item->data.notes[0]) {
		options |= CAR_ITEM_HASNOTES;
	}
	if (layout && item->car && !IsTrackDeleted(item->car)) {
		options |= CAR_ITEM_ONLAYOUT;
	}
	rc &= fprintf(f,
	              "CAR %ld %s \"%s\" %ld %ld %0.3f %0.3f 0 %ld %0.3f %0.3f %ld %0.3f %0.3f %ld %ld %ld 0 0 0 0 0 0",
	              item->index, GetScaleName(item->scaleInx), PutTitle(item->title),
	              options, item->type,
	              item->dim.carLength, item->dim.carWidth, longCenterOffset,
	              item->dim.truckCenter, item->dim.coupledLength, wDrawGetRGB(item->color),
	              item->data.purchPrice, item->data.currPrice, item->data.condition,
	              item->data.purchDate, item->data.serviceDate) > 0;
	if ((options & CAR_ITEM_HASNOTES)) {
		char* sEscapedNote = ConvertToEscapedText(item->data.notes);
		rc &= fprintf(f, " \"%s\"", sEscapedNote) > 0;
		MyFree(sEscapedNote);
	} else {
		rc &= fprintf(f, " \"\"") > 0;
	}
	if ((options & CAR_ITEM_ONLAYOUT)) {
		CarGetPos(item->car, &pos, &angle);
		rc &= fprintf(f, " %d %u %0.3f %0.3f %0.3f\n",
		              GetTrkIndex(item->car), GetTrkLayer(item->car), pos.x, pos.y, angle) > 0;
		rc &= WriteEndPt(f, item->car, 0);
		rc &= WriteEndPt(f, item->car, 1);
		rc &= fprintf(f, "\t%s\n", END_SEGS) > 0;
	} else {
		rc &= fprintf(f, "\n") > 0;
	}

	SetUserLocale();

	return rc;
}


EXPORT BOOL_T WriteCars(
        FILE* f)
{
	int inx;
	BOOL_T rc = TRUE;
	for (inx = 0; inx < carItemInfo_da.cnt; inx++) {
		rc &= CarItemWrite(f, carItemInfo(inx), TRUE);
	}
	return rc;
}

EXPORT long CarItemFindIndex(
        carItem_p item)
{
	long inx;
	for (inx = 0; inx < carItemInfo_da.cnt; inx++)
		if (carItemInfo(inx) == item) {
			return inx;
		}
	CHECK(FALSE);
	return -1;
}


EXPORT void CarItemGetSegs(
        carItem_p item)
{
	coOrd orig;
	carProto_p protoP;
	tabString_t tabs[7];
	trkSeg_t* segPtr;
	DIST_T ratio = GetScaleRatio(item->scaleInx);

	TabStringExtract(item->title, 7, tabs);
	TabStringCpy(message, &tabs[T_PROTO]);
	protoP = CarProtoLookup(message, FALSE, FALSE, 0.0, 0.0);
	if (protoP != NULL) {
		item->segCnt = protoP->segCnt;
		segPtr = protoP->segPtr;
		orig = protoP->orig;
	} else {
		CarProtoDlgCreateDummyOutline(&item->segCnt, &segPtr,
		                              (item->options & CAR_DESC_IS_LOCO) != 0, item->dim.carLength,
		                              item->dim.carWidth,
		                              item->color);
		orig = zero;
	}
	item->segPtr = (trkSeg_p)MyMalloc(item->segCnt * sizeof * (segPtr));
	memcpy(item->segPtr, segPtr, item->segCnt * sizeof * (segPtr));
	CloneFilledDraw(item->segCnt, item->segPtr, FALSE);
	if (protoP) {
		orig.x = -orig.x;
		orig.y = -orig.y;
		MoveSegs(item->segCnt, item->segPtr, orig);
		RescaleSegs(item->segCnt, item->segPtr, item->dim.carLength / protoP->size.x,
		            item->dim.carWidth / protoP->size.y, 1 / ratio);
		RecolorSegs(item->segCnt, item->segPtr, item->color);
	}
}

EXPORT void CarItemFindCouplerMountPoint(
        carItem_p item,
        traverseTrack_t trvTrk0,
        coOrd pos[2])
{
	// We assume the coupler pivot is 'couplerLength' before the end of the car
	DIST_T couplerLength = (item->dim.coupledLength - item->dim.carLength) / 2.0;
	if (IsClose(item->dim.truckCenter)) {
		// Single truck/bogie
		DIST_T d = item->dim.carLength / 2.0 - couplerLength;
		Translate(&pos[0], trvTrk0.pos, trvTrk0.angle,
		          d + item->dim.truckCenterOffset);
		FlipTraverseTrack(&trvTrk0);
		Translate(&pos[1], trvTrk0.pos, trvTrk0.angle,
		          d - item->dim.truckCenterOffset);
		return;
	}
	// Find the pos of the 2 trucks
	// Note this is a slight simplification, we should use the car center, not the on-track position
	traverseTrack_t trvTrk1 = trvTrk0;
	TraverseTrack2(&trvTrk0,
	               item->dim.truckCenter / 2.0 + item->dim.truckCenterOffset);
	FlipTraverseTrack(&trvTrk1);
	TraverseTrack2(&trvTrk1,
	               item->dim.truckCenter / 2.0 - item->dim.truckCenterOffset);

	// Get the angle to translate from the truck
	ANGLE_T angle[2];
	if (trvTrk0.trk == NULL || (item->options & CAR_DESC_COUPLER_MODE_BODY) != 0) {
		// Body mount couplers
		// Angle is same as the car
		angle[0] = FindAngle(trvTrk1.pos, trvTrk0.pos);
		angle[1] = NormalizeAngle(angle[0] + 180.0);
	} else {
		// Truck mounted couplers
		// Angle is same as the trucks
		angle[0] = trvTrk0.angle;
		angle[1] = trvTrk1.angle;
	}

	// Get the distance to translate
	DIST_T d[2];
	d[0] = item->dim.carLength / 2.0 - couplerLength - (item->dim.truckCenter / 2.0
	                +
	                item->dim.truckCenterOffset);
	d[1] = item->dim.carLength / 2.0 - couplerLength - (item->dim.truckCenter / 2.0
	                -
	                item->dim.truckCenterOffset);

	// And translate
	Translate(&pos[0], trvTrk0.pos, angle[0], d[0]);
	Translate(&pos[1], trvTrk1.pos, angle[1], d[1]);

}

EXPORT void CarItemPos(
        carItem_p item,
        coOrd* pos)
{
	pos->x = item->pos.x;
	pos->y = item->pos.y;
}



EXPORT void CarItemSize(
        carItem_p item,
        coOrd* size)
{
	size->x = item->dim.carLength;
	size->y = item->dim.carWidth;
}

EXPORT void CarItemSetNumber(carItem_p item, char* number)
{
	if (item->data.number && number[0]) {
		MyFree(item->data.number);
	}
	if (number[0]) {
		item->data.number = MyStrdup(number);
	}
}


EXPORT char* CarItemNumber(
        carItem_p item)
{
	return item->data.number;
}

static DIST_T CarItemTruckCenter(
        carItem_p item)
{
	return item->dim.truckCenter;
}

static DIST_T CarItemTruckOffset(
        carItem_p item)
{
	return item->dim.truckCenterOffset;
}


EXPORT DIST_T CarItemCoupledLength(
        carItem_p item)
{
	return item->dim.coupledLength;
}


EXPORT BOOL_T CarItemIsLoco(carItem_p item)
{
	return (item->options & CAR_DESC_IS_LOCO) == (CAR_DESC_IS_LOCO);
}


EXPORT BOOL_T CarItemIsLocoMaster(
        carItem_p item)
{
	return (item->options & (CAR_DESC_IS_LOCO | CAR_DESC_IS_LOCO_MASTER)) ==
	       (CAR_DESC_IS_LOCO | CAR_DESC_IS_LOCO_MASTER);
}


EXPORT void CarItemSetLocoMaster(
        carItem_p item,
        BOOL_T locoIsMaster)
{
	if (locoIsMaster) {
		item->options |= CAR_DESC_IS_LOCO_MASTER;
	} else {
		item->options &= ~CAR_DESC_IS_LOCO_MASTER;
	}
}


EXPORT void CarItemSetTrack(
        carItem_p item,
        track_p trk)
{
	item->car = trk;
	if (trk != NULL) {
		SetTrkScale(trk, item->scaleInx);
	}
}

static DIST_T CarItemCouplerLength(
        carItem_p item,
        int dir)
{
	return item->dim.coupledLength - item->dim.carLength;
}


EXPORT char* CarItemDescribe(
        carItem_p item,
        long mode,
        long* index)
{
	tabString_t tabs[7];
	char* cp;
	static char desc[STR_LONG_SIZE];

	TabStringExtract(item->title, 7, tabs);
	cp = desc;
	if (mode != -1) {
		sprintf(cp, "%ld ", item->index);
		cp = desc + strlen(cp);
	}
	if ((mode & 0xF) != 1 && ((mode >> 4) & 0xF) != 1 && ((mode >> 8) & 0xF) != 1
	    && ((mode >> 12) & 0xF) != 1) {
		cp = TabStringCpy(cp, &tabs[T_MANUF]);
		*cp++ = ' ';
	}
	if ((mode & 0xF) != 3 && ((mode >> 4) & 0xF) != 3 && ((mode >> 8) & 0xF) != 3
	    && ((mode >> 12) & 0xF) != 3) {
		cp = TabStringCpy(cp, &tabs[T_PART]);
		*cp++ = ' ';
	}
	if ((mode & 0xF) != 2 && ((mode >> 4) & 0xF) != 2 && ((mode >> 8) & 0xF) != 2
	    && ((mode >> 12) & 0xF) != 2) {
		cp = TabStringCpy(cp, &tabs[T_PROTO]);
		*cp++ = ' ';
	}
	if (tabs[T_DESC].len > 0) {
		cp = TabStringCpy(cp, &tabs[T_DESC]);
		*cp++ = ' ';
	}
	if (mode != -1) {
		if (tabs[T_REPMARK].len > 0) {
			cp = TabStringCpy(cp, &tabs[T_REPMARK]);
			*cp++ = ' ';
		} else if (tabs[T_ROADNAME].len > 0) {
			cp = TabStringCpy(cp, &tabs[T_ROADNAME]);
			*cp++ = ' ';
		}
		if (tabs[T_NUMBER].len > 0) {
			cp = TabStringCpy(cp, &tabs[T_NUMBER]);
			*cp++ = ' ';
		}
	}
	*--cp = '\0';
	if (index != NULL) {
		*index = item->index;
	}
	return desc;
}


extern dynArr_t carItemHotbar_da;

EXPORT void CarItemLoadList(void* unused)
{
	DYNARR_SET(carItem_t*, carItemHotbar_da, carItemInfo_da.cnt);
	memcpy(&carItemHotbar(0), &carItemInfo(0),
	       carItemInfo_da.cnt * sizeof carItemHotbar(0));

	CarUpdateHotbarList();
}

/**
 * Move a CarItem from the layout to the Shelf
 *
 * \item car item to be moved
 */
EXPORT void CarItemShelve(
        carItem_p item)
{
	if (item->car == NULL || IsTrackDeleted(item->car)) { return; }
	DeleteTrack(item->car, FALSE);
	if (CarItemIsLoco(item)) {
		LocoListChangeEntry(item->car, NULL);
	}
	CarItemUpdate(item);
	HotBarCancel();
	// InfoSubstituteControls(NULL, NULL);
}

EXPORT void CarItemPlace(
        carItem_p item,
        traverseTrack_p trvTrk,
        DIST_T* dists)
{
	DIST_T dist;
	DIST_T offset;
	traverseTrack_t trks[2];

	dist = CarItemTruckCenter(item) / 2.0;
	offset = CarItemTruckOffset(
	                 item);				//Offset is the amount the truck centers are displaced
	trks[0] = trks[1] = *trvTrk;
	TraverseTrack2(&trks[0], dist + offset);
	TraverseTrack2(&trks[1], -dist + offset);
	item->angle = FindAngle(trks[1].pos, trks[0].pos);
	item->pos.x = (trks[0].pos.x + trks[1].pos.x) / 2.0;
	item->pos.y = (trks[0].pos.y + trks[1].pos.y) / 2.0;
	Translate(&item->pos, item->pos, item->angle,
	          -offset);  // Put truck center back along line by offset
	dists[0] = dists[1] = CarItemCoupledLength(item) / 2.0;
}

static int drawCarTrucks = 0;
EXPORT void CarItemDraw(
        drawCmd_p d,
        carItem_p item,
        wDrawColor color,
        int direction,
        BOOL_T locoIsMaster,
        vector_p coupler,
        track_p traverse)
{
	coOrd size, pos, pos2;
	DIST_T length;
	wFont_p fp;
	wDrawWidth width;
	trkSeg_t simpleSegs[1];
	pts_t simplePts[4];
	int dir;
	static int couplerLineWidth = 3;

	CarItemSize(item, &size);
	if (!DrawTwoRails(d, 1)) {
		simplePts[0].pt.x = simplePts[3].pt.x = -size.x / 2.0;
		simplePts[1].pt.x = simplePts[2].pt.x = size.x / 2.0;
		simplePts[0].pt.y = simplePts[1].pt.y = -size.y / 2.0;
		simplePts[2].pt.y = simplePts[3].pt.y = size.y / 2.0;
		simplePts[0].pt_type = 0;
		simplePts[1].pt_type = 0;
		simplePts[2].pt_type = 0;
		simplePts[3].pt_type = 0;
		simpleSegs[0].type = SEG_FILPOLY;
		simpleSegs[0].color = item->color;
		simpleSegs[0].lineWidth = 0;
		simpleSegs[0].u.p.cnt = 4;
		simpleSegs[0].u.p.pts = simplePts;
		simpleSegs[0].u.p.orig = zero;
		simpleSegs[0].u.p.angle = 0.0;
		DrawSegs(d, item->pos, item->angle - 90.0, simpleSegs, 1, 0.0, color);
	} else {
		if (item->segCnt == 0) {
			CarItemGetSegs(item);
		}
		Translate(&pos, item->pos, item->angle, -size.x / 2.0);
		Translate(&pos, pos, item->angle - 90, -size.y / 2.0);
		DrawSegs(d, pos, item->angle - 90.0, item->segPtr, item->segCnt, 0.0, color);
	}

	if (drawCarTrucks) {

		length = item->dim.truckCenter / 2.0;
		double offset = CarItemTruckOffset(item);
		Translate(&pos, item->pos, item->angle,
		          length + (direction ? offset : -offset));
		DrawArc(d, pos, trackGauge / 2.0, 0.0, 360.0, FALSE, 0, color);
		Translate(&pos, item->pos, item->angle + 180,
		          length + (direction ? -offset : offset));
		DrawArc(d, pos, trackGauge / 2.0, 0.0, 360.0, FALSE, 0, color);
	}

	if ((labelEnable & LABELENABLE_CARS)) {
		fp = wStandardFont(F_HELV, FALSE, FALSE);
		DrawBoxedString(BOX_BACKGROUND, d, item->pos, item->data.number, fp,
		                (wFontSize_t)descriptionFontSize, color, 0.0);
	}

	/* draw loco head light */
	if ((item->options & CAR_DESC_IS_LOCO) != 0) {
		Translate(&pos, item->pos, item->angle + (direction ? 180.0 : 0.0),
		          size.x / 2.0 - trackGauge / 2.0);
		if (locoIsMaster) {
			DrawFillCircle(d, pos, trackGauge / 2.0,
			               (color == wDrawColorBlack ? drawColorGold : color));
		} else {
			width = (wDrawWidth)floor(trackGauge / 8.0 * d->dpi / d->scale);
			DrawArc(d, pos, trackGauge / 2.0, 0.0, 360.0, FALSE, width,
			        (color == wDrawColorBlack ? drawColorGold : color));
		}
	}

	/* draw coupler */
	if (!DrawTwoRails(d, 0.5)) {
		return;
	}
	//	rad = trackGauge/8.0;
	for (dir = 0; dir < 2; dir++) {
		Translate(&pos, coupler[dir].pos, coupler[dir].angle,
		          CarItemCouplerLength(item, dir));
		DrawLine(d, coupler[dir].pos, pos, couplerLineWidth, color);
		if (DrawTwoRails(d, 1)) {
			/*DrawFillCircle( d, p0, rad, dir==0?color:selectedColor );*/
			Translate(&pos2, pos, coupler[dir].angle + 90.0, trackGauge / 3);
			DrawLine(d, pos2, pos, couplerLineWidth, color);
		}
	}
}
