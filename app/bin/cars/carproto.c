/**
 * \file   carproto.c
 * \brief  Manage car prototype data
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

#include <stdio.h>

#include "common.h"
#include "draw.h"
#include "fileio.h"
#include "paramfile.h"
#include "trkseg.h"

#include "listelem.h"
#include "carsprivate.h"

static int CmpCarProto(void* key, void* elem);

static int log_carPart;

dynArr_t carProto_da;

nameLongMap_t typeListMap[N_TYPELISTMAP] = {
	{ N_("Diesel Loco"),  10101 },
	{ N_("Steam Loco"),  10201 },
	{ N_("Elect Loco"),  10301 },
	{ N_("Freight Car"),  30100 },
	{ N_("Psngr Car"),  50100 },
	{ N_("M-O-W"),  70100 },
	{ N_("Other"),  90100 }
};

static BOOL_T carProtoListChanged;

static pts_t dummyOutlineSegPts[5];
static trkSeg_t dummyOutlineSegs;


void CarProtoDlgCreateDummyOutline(
        int* segCntP,
        trkSeg_p* segPtrP,
        BOOL_T isLoco,
        DIST_T length,
        DIST_T width,
        wDrawColor color)
{
	trkSeg_p segPtr;
	pts_t* pts;
	DIST_T length2;

	*segCntP = 1;
	segPtr = *segPtrP = &dummyOutlineSegs;

	segPtr->type = SEG_FILPOLY;
	segPtr->color = color;
	segPtr->lineWidth = 0;
	segPtr->u.p.cnt = isLoco ? 5 : 4;
	segPtr->u.p.pts = pts = dummyOutlineSegPts;
	segPtr->u.p.orig.x = 0;
	segPtr->u.p.orig.y = 0;
	segPtr->u.p.angle = 0;
	length2 = length;
	if (isLoco) {
		pts->pt.x = length;
		pts->pt.y = width / 2.0;
		pts->pt_type = 0;
		pts++;
		length2 -= width / 2.0;
	}
	pts->pt.x = length2;
	pts->pt.y = 0.0;
	pts->pt_type = 0;
	pts++;
	pts->pt.x = 0.0;
	pts->pt.y = 0.0;
	pts->pt_type = 0;
	pts++;
	pts->pt.x = 0.0;
	pts->pt.y = width;
	pts->pt_type = 0;
	pts++;
	pts->pt.x = length2;
	pts->pt.y = width;
	pts->pt_type = 0;
}

int CarProtoFindTypeCode(
        long code)
{
	int inx;
	for (inx = 0; inx < N_TYPELISTMAP; inx++) {
		if (typeListMap[inx].value > code) {
			if (inx == 0) {
				return N_TYPELISTMAP - 1;
			} else {
				return inx - 1;
			}
		}
	}
	return N_TYPELISTMAP - 1;
}


static int CmpCarProto(
        void* key,
        void* elem)
{
	char* key_val = key;
	carProto_p elem_val = elem;
	return strcasecmp(key_val, elem_val->desc);
}


carProto_p CarProtoFind(char* desc)
{
	return LookupListElem(&carProto_da, desc, CmpCarProto, 0);
}

carProto_p CarProtoLookup(
        char* desc,
        BOOL_T createMissing,
        BOOL_T isLoco,
        DIST_T length,
        DIST_T width)
{
	carProto_p proto;
	trkSeg_p segPtr;
	proto = LookupListElem(&carProto_da, desc, CmpCarProto,
	                       createMissing ? sizeof * proto : 0);
	if (proto == NULL) {
		return NULL;
	}
	if (proto->desc == NULL) {
		proto->desc = MyStrdup(desc);
		proto->contentsLabel = "Car Prototype";
		proto->paramFileIndex = PARAM_LAYOUT;
		proto->options = (isLoco ? CAR_DESC_IS_LOCO : 0);
		proto->dim.carLength = length;
		proto->dim.carWidth = width;
		proto->dim.truckCenter = length - 2.0 * 59.0;
		proto->dim.coupledLength = length + 2.0 * 16.0;
		CarProtoDlgCreateDummyOutline(&proto->segCnt, &segPtr, isLoco, length, width,
		                              drawColorBlue);
		proto->segPtr = (trkSeg_p)memdup(segPtr,
		                                 (sizeof * (trkSeg_p)0) * proto->segCnt);
		CloneFilledDraw(proto->segCnt, proto->segPtr, FALSE);
		GetSegBounds(zero, 0.0, proto->segCnt, proto->segPtr, &proto->orig,
		             &proto->size);
		carProtoListChanged = TRUE;
	}
	return proto;
}

carProto_p CarProtoNew(
        carProto_p proto,
        int paramFileIndex,
        char* desc,
        long options,
        long type,
        const carDim_t* dim,
        wIndex_t segCnt,
        trkSeg_p segPtr)
{
	if (proto == NULL) {
		proto = LookupListElem(&carProto_da, desc, CmpCarProto, sizeof * proto);
		if (proto->desc != NULL) {
			if (proto->paramFileIndex == PARAM_CUSTOM &&
			    paramFileIndex != PARAM_CUSTOM) {
				return proto;
			}
		}
	}
	if (proto->desc != NULL) {
		MyFree(proto->desc);
	}
	proto->desc = MyStrdup(desc);
	proto->contentsLabel = "Car Prototype";
	proto->paramFileIndex = paramFileIndex;
	proto->options = options;
	proto->type = type;
	proto->dim = *dim;
	proto->segCnt = segCnt;

	proto->segPtr = (trkSeg_p)memdup(segPtr,
	                                 (sizeof * (trkSeg_p)0) * proto->segCnt);
	CloneFilledDraw(proto->segCnt, proto->segPtr, FALSE);
	GetSegBounds(zero, 0.0, proto->segCnt, proto->segPtr, &proto->orig,
	             &proto->size);
	carProtoListChanged = TRUE;
	return proto;
}


void CarProtoDelete(carProto_p protoP)
{
	if (protoP == NULL) {
		return;
	}
	RemoveListElem(&carProto_da, protoP);
	if (protoP->desc) {
		MyFree(protoP->desc);
	}
	if (protoP->segPtr) {
		MyFree(protoP->segPtr);
	}
	MyFree(protoP);
}


/**
* Delete all car prototype definitions that came from a specific parameter file.
* Due to the way the definitions are loaded from file it is safe to
* assume that they form a contiguous block in the array.
*
* \param [IN] fileIndex parameter file
*/

void DeleteCarProto(int fileIndex)
{
	// Iterate backwards to avoid index shifting problems
	for (int i = carProto_da.cnt - 1; i >= 0; i--) {
		if (carProto(i)->paramFileIndex == fileIndex) {
			CarProtoDelete(carProto(i));
			// CarProtoDelete handles removal from array automatically
		}
	}
}


static BOOL_T CarProtoRead(char* line)
{
	char* desc;
	long options;
	long type;
	carDim_t dim;
	long longCenterOffset;

	if (!GetArgs(line + 9, "qllff0lff",
	             &desc, &options, &type, &dim.carLength, &dim.carWidth, &longCenterOffset,
	             &dim.truckCenter, &dim.coupledLength)) {
		return FALSE;
	}
	dim.truckCenterOffset = longCenterOffset / 1000.0;
	if (!ReadSegs()) {
		return FALSE;
	}
	CarProtoNew(NULL, curParamFileIndex, desc, options, type, &dim,
	            tempSegs_da.cnt, &tempSegs(0));
	FreeFilledDraw(tempSegs_da.cnt, &tempSegs(0));
	MyFree(desc);
	return TRUE;
}


BOOL_T CarProtoWrite(FILE* f,const carProto_t* proto)
{
	BOOL_T rc = TRUE;

	SetCLocale();

	long longCenterOffset = (long)(proto->dim.truckCenterOffset * 1000);

	rc &= fprintf(f, "CARPROTO \"%s\" %ld %ld %0.3f %0.3f 0 %ld %0.3f %0.3f\n",
	              PutTitle(proto->desc), proto->options, proto->type, proto->dim.carLength,
	              proto->dim.carWidth, longCenterOffset, proto->dim.truckCenter,
	              proto->dim.coupledLength) > 0;
	rc &= WriteSegs(f, proto->segCnt, proto->segPtr);

	SetUserLocale();

	return rc;
}

BOOL_T CarProtoCustomSave(FILE* f)
{
	BOOL_T rc = TRUE;

	for (int inx = 0; inx < carProto_da.cnt; inx++) {
		const carProto_t* proto;
		proto = carProto(inx);
		if (proto->paramFileIndex == PARAM_CUSTOM) {
			rc &= CarProtoWrite(f, proto);
		}
	}
	return rc;
}

enum paramFileState
GetCarProtoCompatibility(int paramFileIndex, SCALEINX_T scaleIndex) {
	int i;
	enum paramFileState ret = PARAMFILE_NOTUSABLE;

	if (!IsParamValid(paramFileIndex))
	{
		return(PARAMFILE_UNLOADED);
	}

	for (i = 0; i < carProto_da.cnt; i++)
	{
		const carProto_t* carProto = carProto(i);
		if (carProto->paramFileIndex == paramFileIndex) {
			ret = PARAMFILE_FIT;
			break;
		}
	}
	return(ret);
}

/*
 *  Draw Car Parts
 */


#define BW (8)
#define TW (45)
#define SI (30)
#define SO (37)
static coOrd truckOutline[] = {
	{ -TW, -SO },
	{  TW, -SO },
	{  TW, -SI },
	{  BW, -SI },
	{  BW,  SI },
	{  TW,  SI },
	{  TW,  SO },
	{ -TW,  SO },
	{ -TW,  SI },
	{ -BW,  SI },
	{ -BW, -SI },
	{ -TW, -SI }
};
#define WO ((56.6-2)/2)
#define WI ((56.6-12)/2)
#define Wd (36/2)
#define AW (8/2)
static coOrd wheelOutline[] = {
	{ -Wd, -WO },

	{ -AW, -WO },
	{ -AW, -SI },
	{  AW, -SI },
	{  AW, -WO },

	{  Wd, -WO },
	{  Wd, -WI },
	{  AW, -WI },
	{  AW,  WI },
	{  Wd,  WI },
	{  Wd,  WO },

	{  AW,  WO },
	{  AW,  SI },
	{ -AW,  SI },
	{ -AW,  WO },

	{ -Wd,  WO },
	{ -Wd,  WI },
	{ -AW,  WI },
	{ -AW, -WI },

	{ -Wd, -WI }
};


void CarProtoDrawTruck(
        drawCmd_t* d,
        DIST_T width,
        FLOAT_T ratio,
        coOrd pos,
        ANGLE_T angle)
{
	coOrd p[24], pp;
	wDrawColor color = wDrawColorBlack;

	memcpy(p, truckOutline, sizeof truckOutline);
	RescalePts(COUNT(truckOutline), p, 1.0, width / 56.5);
	RescalePts(COUNT(truckOutline), p, ratio, ratio);
	RotatePts(COUNT(truckOutline), p, zero, angle);
	MovePts(COUNT(truckOutline), p, pos);
	DrawPoly(d, COUNT(truckOutline), p, NULL, color, 0, DRAW_FILL);
	pp.x = -70 / 2;
	pp.y = 0;
	memcpy(p, wheelOutline, sizeof wheelOutline);
	RescalePts(COUNT(wheelOutline), p, 1.0, width / 56.5);
	MovePts(COUNT(wheelOutline), p, pp);
	RescalePts(COUNT(wheelOutline), p, ratio, ratio);
	RotatePts(COUNT(wheelOutline), p, zero, angle);
	MovePts(COUNT(wheelOutline), p, pos);
	DrawPoly(d, COUNT(wheelOutline), p, NULL, color, 0, DRAW_FILL);
	pp.x = 70 / 2;
	memcpy(p, wheelOutline, sizeof wheelOutline);
	RescalePts(COUNT(wheelOutline), p, 1.0, width / 56.5);
	MovePts(COUNT(wheelOutline), p, pp);
	RescalePts(COUNT(wheelOutline), p, ratio, ratio);
	RotatePts(COUNT(wheelOutline), p, zero, angle);
	MovePts(COUNT(wheelOutline), p, pos);
	DrawPoly(d, COUNT(wheelOutline), p, NULL, color, 0, DRAW_FILL);
}


static coOrd couplerOutline[] = {
	{ 0, 2.5 },
	{ 0, -2.5 },
	{ 0, -2.5 },
	{ 3, -7 },
	{ 14, -5 },
	{ 14, 2 },
	{ 12, 2 },
	{ 12, -2 },
	{ 9, -2 },
	{ 9, 3 },
	{ 13, 6 },
	{ 13, 7 },
	{ 6, 7 },
	{ 0, 2.5 }
};

void CarProtoDrawCoupler(
        drawCmd_t* d,
        DIST_T length,
        FLOAT_T ratio,
        coOrd pos,
        ANGLE_T angle)
{
	coOrd p[24], pp;
	wDrawColor color = wDrawColorBlack;

	length /= ratio;
	if (length < 12.0) {
		return;
	}
	memcpy(p, couplerOutline, sizeof couplerOutline);
	p[0].x = p[1].x = -(length - 12.0);
	pp.x = length - 12.0;
	pp.y = 0;
	/**   \TODO - if length > 6 then draw Sills */
	MovePts(COUNT(couplerOutline), p, pp);
	RescalePts(COUNT(couplerOutline), p, ratio, ratio);
	RotatePts(COUNT(couplerOutline), p, zero, angle - 90.0);
	MovePts(COUNT(couplerOutline), p, pos);
	DrawPoly(d, COUNT(couplerOutline), p, NULL, color, 0, DRAW_FILL);
}


int CarProtoCustMgmProc( int cmd, void * data )
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
		CarDlgUpdProto( );
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
	default:
		LOG( log_carPart, 1, ( "unexpected cmd %d in CarProtoCustMgmProc\n", cmd ) )
		break;
	}
	return FALSE;
}

void
InitCarProto(void)
{
	AddParam( "CARPROTO ", CarProtoRead );

	log_carPart = LogFindIndex("carPart");
}
