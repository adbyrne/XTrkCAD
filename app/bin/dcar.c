/** \file dcar.c
 * TRAIN
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

#ifndef WINDOWS
#include <errno.h>
#endif
#include <ctype.h>

#include <stdint.h>

#include "track.h"
#include "ctrain.h"
#include "i18n.h"
#include "fileio.h"

static int log_carList;
static int log_carInvList;
static int log_carDlgState;
static int log_carDlgList;

static paramFloatRange_t r0_99999 = { 0, 99999, 80 };
static paramIntegerRange_t i1_999999999 = { 1, 999999999, 80, PDO_NORANGECHECK_HIGH };
static paramIntegerRange_t i1_9999 = { 1, 9999, 50 };
static char * isLocoLabels[] = { "", 0 };
static char * cplrModeLabels[] = { N_("Truck"), N_("Body"), 0 };
static BOOL_T carProtoListChanged;
static void CarInvListAdd( carItem_p item );
static void CarInvListUpdate( carItem_p item );

#define T_MANUF			(0)
#define T_PROTO			(1)
#define T_DESC			(2)
#define T_PART			(3)
#define T_ROADNAME		(4)
#define T_REPMARK		(5)
#define T_NUMBER		(6)

typedef struct {
		char * name;
		long value;
		} nameLongMap_t;


#define CAR_DESC_COUPLER_MODE_BODY	(1L<<0)
#define CAR_DESC_IS_LOCO			(1L<<1)
#define CAR_DESC_IS_LOCO_MASTER		(1L<<2)
#define CAR_ITEM_HASNOTES			(1L<<8)
#define CAR_ITEM_ONLAYOUT			(1L<<9)

#define CAR_DESC_BITS				(0x000000FF)
#define CAR_ITEM_BITS				(0x0000FF00)


typedef struct carProto_t * carProto_p;

typedef struct {
		DIST_T			carLength;
		DIST_T			carWidth;
		DIST_T			truckCenter;
		DIST_T			coupledLength;
		} carDim_t;
typedef struct {
		char			* number;
		FLOAT_T			purchPrice;
		FLOAT_T			currPrice;
		long			condition;
		long			purchDate;
		long			serviceDate;
		char			* notes;
		} carData_t;

struct carItem_t {
		long			index;
		SCALEINX_T		scaleInx;
		char			* contentsLabel;
		char			* title;
		carProto_p		proto;
		DIST_T			barScale;
		wDrawColor		color;
		long			options;
		long			type;
		carDim_t		dim;
		carData_t		data;
		wIndex_t		segCnt;
		trkSeg_p		segPtr;
		track_p			car;
		coOrd			pos;
		ANGLE_T			angle;
		};


/*
 *  Utilities
 */



typedef struct {
		char * ptr;
		int len;
		} tabString_t, *tabString_p;


static void TabStringExtract(
		char * string,
		int count,
		tabString_t * tabs )
{
	int inx;
	char * next = string;

	for ( inx=0; inx<count; inx++ ) {
		tabs[inx].ptr = string;
		if ( next )
			 next = strchr( string, '\t' );
		if ( next ) {
			tabs[inx].len = next-string;
			string = next+1;
		} else {
			tabs[inx].len = strlen( string );
			string += tabs[inx].len;
		}
	}
	if ( tabs[T_MANUF].len == 0 ) {
		tabs[T_MANUF].len = 7;
		tabs[T_MANUF].ptr = N_("Unknown");
	}
}


static char * TabStringDup(
		tabString_t * tab )
{
	char * ret;
	ret = MyMalloc( tab->len+1 );
	memcpy( ret, tab->ptr, tab->len );
	ret[tab->len] = '\0';
	return ret;
}


static char * TabStringCpy(
		char * dst,
		tabString_t * tab )
{
	memcpy( dst, tab->ptr, tab->len );
	dst[tab->len] = '\0';
	return dst+tab->len;
}


static int TabStringCmp(
		char * src,
		tabString_t * tab )
{
	int srclen = strlen(src);
	int len = srclen;
	int rc;
	if ( len > tab->len )
		len = tab->len;
	rc = strncasecmp( src, tab->ptr, len );
	if ( rc != 0 || srclen == tab->len )
		return rc;
	else if ( srclen > tab->len )
		return 1;
	else
		return -1;
}


static long TabGetLong(
		tabString_t * tab )
{
	char old_c;
	long val;
	if ( tab->len <= 0 )
		return 0;
	old_c = tab->ptr[tab->len];
	tab->ptr[tab->len] = '\0';
	val = atol( tab->ptr );
	tab->ptr[tab->len] = old_c;
	return val;
}


static FLOAT_T TabGetFloat(
		tabString_t * tab )
{
	char old_c;
	FLOAT_T val;
	if ( tab->len <= 0 )
		return 0.0;
	old_c = tab->ptr[tab->len];
	tab->ptr[tab->len] = '\0';
	val = atof( tab->ptr );
	tab->ptr[tab->len] = old_c;
	return val;
}


static void RotatePts(
		int cnt,
		coOrd * pts,
		coOrd orig,
		ANGLE_T angle )
{
	int inx;
	for ( inx=0; inx<cnt; inx++ ) {
		Rotate( &pts[inx], orig, angle );
	}
}


static void RescalePts(
		int cnt,
		coOrd * pts,
		FLOAT_T scale_x,
		FLOAT_T scale_y )
{
	int inx;
	for ( inx=0; inx<cnt; inx++ ) {
		pts[inx].x *= scale_x;
		pts[inx].y *= scale_y;
	}
}


static int lookupListIndex;
static void * LookupListElem(
		dynArr_t * da,
		void * key,
		int (*cmpFunc)( void *, void * ),
		int elem_size )
{
	int hi, lo, mid, rc;
	lo = 0;
	hi = da->cnt-1;
	while (lo <= hi ) {
		mid = (lo+hi)/2;
		rc = cmpFunc( key, DYNARR_N(void*,*da,mid) );
		if ( rc == 0 ) {
			lookupListIndex = mid;
			return DYNARR_N(void*,*da,mid);
		}
		if ( rc > 0 )
			lo = mid+1;
		else
			hi = mid-1;
	}
	if ( elem_size == 0 ) {
		lookupListIndex = -1;
		return NULL;
	}
	DYNARR_APPEND( void*, *da, 10 );
	for ( mid=da->cnt-1; mid>lo; mid-- )
		DYNARR_N(void*,*da,mid) = DYNARR_N(void*,*da,mid-1);
	DYNARR_N(void*,*da,lo) = (void*)MyMalloc(elem_size);
	memset( DYNARR_N(void*,*da,lo), 0, elem_size );
	lookupListIndex = lo;
	return DYNARR_N(void*,*da,lo);
}

static void RemoveListElem(
		dynArr_t * da,
		void * elem )
{
	int inx;
	for ( inx=0; inx<da->cnt; inx++ )
		if ( DYNARR_N(void*,*da,inx) == elem )
			break;
	if ( inx>=da->cnt )
		AbortProg( "removeListElem" );
	for ( inx++; inx<da->cnt; inx++ )
		DYNARR_N(void*,*da,inx-1) = DYNARR_N(void*,*da,inx);
	da->cnt--;
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
		{ -TW, -SI } };
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

		{ -Wd, -WI } };

static void MovePts(
		int cnt,
		coOrd * pts,
		coOrd orig )
{
	int inx;
	for ( inx=0; inx<cnt; inx++ ) {
		pts[inx].x += orig.x;
		pts[inx].y += orig.y;
	}
}


static void CarProtoDrawTruck(
		drawCmd_t * d,
		DIST_T width,
		FLOAT_T ratio,
		coOrd pos,
		ANGLE_T angle )
{
	coOrd p[24], pp;
	wDrawColor color = wDrawColorBlack;

	memcpy( p, truckOutline, sizeof truckOutline );
	RescalePts( sizeof truckOutline/sizeof truckOutline[0], p, 1.0, width/56.5 );
	RescalePts( sizeof wheelOutline/sizeof wheelOutline[0], p, ratio, ratio );
	RotatePts( sizeof wheelOutline/sizeof wheelOutline[0], p, zero, angle );
	MovePts( sizeof truckOutline/sizeof truckOutline[0], p, pos );
	DrawFillPoly( d, sizeof truckOutline/sizeof truckOutline[0], p, color );
	pp.x = -70/2;
	pp.y = 0;
	memcpy( p, wheelOutline, sizeof wheelOutline );
	RescalePts( sizeof wheelOutline/sizeof wheelOutline[0], p, 1.0, width/56.5 );
	MovePts( sizeof wheelOutline/sizeof wheelOutline[0], p, pp );
	RescalePts( sizeof wheelOutline/sizeof wheelOutline[0], p, ratio, ratio );
	RotatePts( sizeof wheelOutline/sizeof wheelOutline[0], p, zero, angle );
	MovePts( sizeof wheelOutline/sizeof wheelOutline[0], p, pos );
	DrawFillPoly( d, sizeof wheelOutline/sizeof wheelOutline[0], p, color );
	pp.x = 70/2;
	memcpy( p, wheelOutline, sizeof wheelOutline );
	RescalePts( sizeof wheelOutline/sizeof wheelOutline[0], p, 1.0, width/56.5 );
	MovePts( sizeof wheelOutline/sizeof wheelOutline[0], p, pp );
	RescalePts( sizeof wheelOutline/sizeof wheelOutline[0], p, ratio, ratio );
	RotatePts( sizeof wheelOutline/sizeof wheelOutline[0], p, zero, angle );
	MovePts( sizeof wheelOutline/sizeof wheelOutline[0], p, pos );
	DrawFillPoly( d, sizeof wheelOutline/sizeof wheelOutline[0], p, color );
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
		{ 0, 2.5 } };
static void CarProtoDrawCoupler(
		drawCmd_t * d,
		DIST_T length,
		FLOAT_T ratio,
		coOrd pos,
		ANGLE_T angle )
{
	coOrd p[24], pp;
	wDrawColor color = wDrawColorBlack;

	length /= ratio;
	if ( length < 12.0 )
		return;
	memcpy( p, couplerOutline, sizeof couplerOutline );
	p[0].x = p[1].x = -(length-12.0);
	pp.x = length-12.0;
	pp.y = 0;
/* TODO - if length > 6 then draw Sills */
#ifdef FUTURE
	if ( angle == 270.0 ) {
		pos.x -= (length-12.0);
		for ( inx=0; inx<sizeof couplerOutline/sizeof couplerOutline[0]; inx++ ) {
			p[inx].x = -p[inx].x;
			p[inx].y = -p[inx].y;
		}
	} else {
		pos.x += (length-12.0);
	}
#endif
	MovePts( sizeof couplerOutline/sizeof couplerOutline[0], p, pp );
	RescalePts( sizeof couplerOutline/sizeof couplerOutline[0], p, ratio, ratio );
	RotatePts( sizeof couplerOutline/sizeof couplerOutline[0], p, zero, angle-90.0 );
	MovePts( sizeof couplerOutline/sizeof couplerOutline[0], p, pos );
	DrawFillPoly( d, sizeof couplerOutline/sizeof couplerOutline[0], p, color );
}



/*
 *  Car Proto
 */


struct carProto_t;
typedef struct carProto_t carProto_t;

struct carProto_t {
		char            * contentsLabel;
		wIndex_t        paramFileIndex;
		char            * desc;
		long            options;
		long            type;
		carDim_t        dim;
		int             segCnt;
		trkSeg_p        segPtr;
		coOrd           size;
		coOrd           orig;
		};

static dynArr_t carProto_da;
#define carProto(N) DYNARR_N( carProto_t*, carProto_da, N )

#define N_TYPELISTMAP           (7)
static nameLongMap_t typeListMap[N_TYPELISTMAP] = {
			{ N_("Diesel Loco"),  10101 },
			{ N_("Steam Loco"),  10201 },
			{ N_("Elect Loco"),  10301 },
			{ N_("Freight Car"),  30100 },
			{ N_("Psngr Car"),  50100 },
			{ N_("M-O-W"),  70100 },
			{ N_("Other"),  90100 } };

static trkSeg_p carProtoSegPtr;
static int carProtoSegCnt;


static coOrd dummyOutlineSegPts[5];
static trkSeg_t dummyOutlineSegs;
static void CarProtoDlgCreateDummyOutline(
		int * segCntP,
		trkSeg_p * segPtrP,
		BOOL_T isLoco,
		DIST_T length,
		DIST_T width,
		wDrawColor color )
{
	trkSeg_p segPtr;
	coOrd * pts;
	DIST_T length2;

	*segCntP = 1;
	segPtr = *segPtrP = &dummyOutlineSegs;

	segPtr->type = SEG_FILPOLY;
	segPtr->color = color;
	segPtr->width = 0;
	segPtr->u.p.cnt = isLoco?5:4;
	segPtr->u.p.pts = pts = dummyOutlineSegPts;
	segPtr->u.p.orig.x = 0;
	segPtr->u.p.orig.y = 0;
	segPtr->u.p.angle = 0;
	length2 = length;
	if ( isLoco ) {
		pts->x = length;
		pts->y = width/2.0;
		pts++;
		length2 -= width/2.0;
	}
	pts->x = length2;
	pts->y = 0.0;
	pts++;
	pts->x = 0.0;
	pts->y = 0.0;
	pts++;
	pts->x = 0.0;
	pts->y = width;
	pts++;
	pts->x = length2;
	pts->y = width;
}


static int CarProtoFindTypeCode(
		long code )
{
	int inx;
	for ( inx=0; inx<N_TYPELISTMAP; inx++ ) {
		if ( typeListMap[inx].value > code ) {
			if ( inx == 0 )
				return N_TYPELISTMAP-1;
			else
				return inx-1;
		}
	}
	return N_TYPELISTMAP-1;
}


static int CmpCarProto(
		void * key,
		void * elem )
{
	char * key_val=key;
	carProto_p elem_val=elem;
	return strcasecmp( key_val, elem_val->desc );
}


static carProto_p CarProtoFind(
		char * desc )
{
	return LookupListElem( &carProto_da, desc, CmpCarProto, 0 );
}


static carProto_p CarProtoLookup(
		char * desc,
		BOOL_T createMissing,
		BOOL_T isLoco,
		DIST_T length,
		DIST_T width )
{
	carProto_p proto;
	trkSeg_p segPtr;
	proto = LookupListElem( &carProto_da, desc, CmpCarProto, createMissing?sizeof *proto:0 );
	if ( proto == NULL )
		return NULL;
	if ( proto->desc == NULL ) {
		proto->desc = MyStrdup(desc);
		proto->contentsLabel = "Car Prototype";
		proto->paramFileIndex = PARAM_LAYOUT;
		proto->options = (isLoco?CAR_DESC_IS_LOCO:0);
		proto->dim.carLength = length;
		proto->dim.carWidth = width;
		proto->dim.truckCenter = length - 2.0*59.0;
		proto->dim.coupledLength = length + 2.0*16.0;
		CarProtoDlgCreateDummyOutline( &proto->segCnt, &segPtr, isLoco, length, width, drawColorBlue );
		proto->segPtr = (trkSeg_p)memdup( segPtr, (sizeof *(trkSeg_p)0) * proto->segCnt );
		CloneFilledDraw( proto->segCnt, proto->segPtr, FALSE );
		GetSegBounds( zero, 0.0, proto->segCnt, proto->segPtr, &proto->orig, &proto->size );
		carProtoListChanged = TRUE;
	}
	return proto;
}


static carProto_p CarProtoNew(
		carProto_p proto,
		int paramFileIndex,
		char * desc,
		long options,
		long type,
		carDim_t * dim,
		wIndex_t segCnt,
		trkSeg_p segPtr )
{
	if ( proto == NULL ) {
		proto = LookupListElem( &carProto_da, desc, CmpCarProto, sizeof *(carProto_p)0 );
		if ( proto->desc != NULL ) {
			if ( proto->paramFileIndex == PARAM_CUSTOM &&
				 paramFileIndex != PARAM_CUSTOM )
				return proto;
		}
	}
	if ( proto->desc != NULL ) {
		MyFree( proto->desc );
	}
	proto->desc = MyStrdup(desc);
	proto->contentsLabel = "Car Prototype";
	proto->paramFileIndex = paramFileIndex;
	proto->options = options;
	proto->type = type;
	proto->dim = *dim;
	proto->segCnt = segCnt;
	proto->segPtr = (trkSeg_p)memdup( segPtr, (sizeof *(trkSeg_p)0) * proto->segCnt );
	CloneFilledDraw( proto->segCnt, proto->segPtr, FALSE );
	GetSegBounds( zero, 0.0, proto->segCnt, proto->segPtr, &proto->orig, &proto->size );
	carProtoListChanged = TRUE;
	return proto;
}


static void CarProtoDelete(
		carProto_p protoP )
{
	if ( protoP == NULL )
		return;
	RemoveListElem( &carProto_da, protoP );
	if ( protoP->desc )
		MyFree( protoP->desc );
	MyFree( protoP );
}


static BOOL_T CarProtoRead(
		char * line )
{
	char * desc;
	long options;
	long type;
	carDim_t dim;

	if ( !GetArgs( line+9, "qllff00ff",
		&desc, &options, &type, &dim.carLength, &dim.carWidth, &dim.truckCenter, &dim.coupledLength ) )
		return FALSE;
	if ( !ReadSegs() )
		return FALSE;
	CarProtoNew( NULL, curParamFileIndex, desc, options, type, &dim, tempSegs_da.cnt, &tempSegs(0) );
	return TRUE;
}


static BOOL_T CarProtoWrite(
		FILE * f,
		carProto_t * proto )
{
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");

	rc &= fprintf( f, "CARPROTO \"%s\" %ld %ld %0.3f %0.3f 0 0 %0.3f %0.3f\n",
		PutTitle(proto->desc), proto->options, proto->type, proto->dim.carLength, proto->dim.carWidth, proto->dim.truckCenter, proto->dim.coupledLength )>0;
	rc &= WriteSegs( f, proto->segCnt, proto->segPtr );

	RestoreLocale(oldLocale);

	return rc;
}



static BOOL_T CarProtoCustomSave(
		FILE * f )
{
	int inx;
	carProto_t * proto;
	BOOL_T rc = TRUE;

	for ( inx=0; inx<carProto_da.cnt; inx++ ) {
		proto = carProto(inx);
		if ( proto->paramFileIndex == PARAM_CUSTOM )
			rc &= CarProtoWrite( f, proto );
	}
	return rc;
}


/*
 *  Car Desc
 */

struct carPart_t;
typedef struct carPart_t carPart_t;
typedef carPart_t * carPart_p;
struct carPartParent_t;
typedef struct carPartParent_t carPartParent_t;
typedef carPartParent_t * carPartParent_p;

typedef struct {
		char * name;
		int len;
		} cmp_key_t;

typedef struct {
		tabString_t manuf;
		tabString_t proto;
		SCALEINX_T scale;
		} cmp_partparent_t;
struct carPartParent_t {
		char * manuf;
		char * proto;
		SCALEINX_T scale;
		dynArr_t parts_da;
		};
struct carPart_t {
		carPartParent_p parent;
		wIndex_t paramFileIndex;
		char * title;
		long options;
		long type;
		carDim_t dim;
		wDrawColor color;
		char * partnoP;
		int partnoL;
		};
static dynArr_t carPartParent_da;
#define carPartParent(N)	DYNARR_N(carPartParent_p, carPartParent_da, N)
#define carPart(P,N)		DYNARR_N(carPart_p, (P)->parts_da, N)
struct roadnameMap_t;
typedef struct roadnameMap_t roadnameMap_t;
typedef roadnameMap_t * roadnameMap_p;
struct roadnameMap_t {
		char * roadname;
		char * repmark;
		};
static dynArr_t roadnameMap_da;
#define roadnameMap(N) DYNARR_N(roadnameMap_p, roadnameMap_da, N)
static BOOL_T roadnameMapChanged;
static long carPartChangeLevel = 0;



static int Cmp_part(
		void * key,
		void * elem )
{
	carPart_p cmp_key=key;
	carPart_p part_elem=elem;
	int rc;
	int len;

	len = min( cmp_key->partnoL, part_elem->partnoL );
	rc = strncasecmp( cmp_key->partnoP, part_elem->partnoP, len+1 );
	if ( rc != 0 )
		return rc;
	if ( cmp_key->paramFileIndex == part_elem->paramFileIndex )
		return 0;
	if ( cmp_key->paramFileIndex == PARAM_DEMO )
		return -1;
	if ( part_elem->paramFileIndex == PARAM_DEMO )
		return 1;
	if ( cmp_key->paramFileIndex == PARAM_CUSTOM )
		return -1;
	if ( part_elem->paramFileIndex == PARAM_CUSTOM )
		return 1;
	if ( cmp_key->paramFileIndex == PARAM_LAYOUT )
		return 1;
	if ( part_elem->paramFileIndex == PARAM_LAYOUT )
		return -1;
	if ( cmp_key->paramFileIndex > part_elem->paramFileIndex )
		return -1;
	else
		return 1;
}


static int Cmp_partparent(
		void * key,
		void * elem )
{
	cmp_partparent_t * cmp_key=key;
	carPartParent_p part_elem=elem;
	int rc;

	rc = - TabStringCmp( part_elem->manuf, &cmp_key->manuf );
	if ( rc != 0 )
		return rc;
	rc = cmp_key->scale - part_elem->scale;
	if ( rc != 0 )
		return rc;
	rc = - TabStringCmp( part_elem->proto, &cmp_key->proto );
	return rc;
}


static int Cmp_roadnameMap(
		void * key,
		void * elem )
{
	cmp_key_t * cmp_key=key;
	roadnameMap_p roadname_elem=elem;
	int rc;

	rc = strncasecmp( cmp_key->name, roadname_elem->roadname, cmp_key->len );
	if ( rc == 0 && roadname_elem->roadname[cmp_key->len] )
		return -1;
	return rc;
}


static roadnameMap_p LoadRoadnameList(
		tabString_p roadnameTab,
		tabString_p repmarkTab )
{
	cmp_key_t cmp_key;
	roadnameMap_p roadnameMapP;

	lookupListIndex = -1;
	if ( roadnameTab->len<=0 )
		return NULL;
	if ( TabStringCmp( "undecorated", roadnameTab ) == 0 )
		return NULL;

	cmp_key.name = roadnameTab->ptr;
	cmp_key.len = roadnameTab->len;
	roadnameMapP = LookupListElem( &roadnameMap_da, &cmp_key, Cmp_roadnameMap, sizeof *(roadnameMap_p)0 );
	if ( roadnameMapP->roadname == NULL ) {
		roadnameMapP->roadname = TabStringDup(roadnameTab);
		roadnameMapP->repmark = TabStringDup(repmarkTab);
		roadnameMapChanged = TRUE;
	} else if ( repmarkTab->len > 0 &&
				( roadnameMapP->repmark == NULL || roadnameMapP->repmark[0] == '\0' ) ) {
		roadnameMapP->repmark = TabStringDup(repmarkTab);
		roadnameMapChanged = TRUE;
	}
	return roadnameMapP;
}


static carPart_p CarPartFind(
		char * manufP,
		int manufL,
		char * partnoP,
		int partnoL,
		SCALEINX_T scale )
{
	wIndex_t inx1, inx2;
	carPart_p partP;
	carPartParent_p parentP;
	for ( inx1=0; inx1<carPartParent_da.cnt; inx1++ ) {
		parentP = carPartParent(inx1);
		if ( manufL == (int)strlen(parentP->manuf) &&
			 strncasecmp( manufP, parentP->manuf, manufL ) == 0 &&
			 scale == parentP->scale ) {
			for ( inx2=0; inx2<parentP->parts_da.cnt; inx2++ ) {
				partP = carPart( parentP, inx2 );
				if ( partnoL == partP->partnoL &&
					 strncasecmp( partnoP, partP->partnoP, partnoL ) == 0 ) {
					return partP;
				}
			 }
		}
	}
	return NULL;
}




static void CarPartParentDelete(
		carPartParent_p parentP )
{
	RemoveListElem( &carPartParent_da, parentP );
	MyFree( parentP->manuf );
	MyFree( parentP->proto );
	MyFree( parentP );
}


static void CarPartUnlink(
		carPart_p partP )
{
	carPartParent_p parentP = partP->parent;
	RemoveListElem( &parentP->parts_da, partP );
	if ( parentP->parts_da.cnt <= 0 ) {
		CarPartParentDelete( parentP );
	}
}


static carPartParent_p CarPartParentNew(
		char * manufP,
		int manufL,
		char *protoP,
		int protoL,
		SCALEINX_T scale )
{
	carPartParent_p parentP;
	cmp_partparent_t cmp_key;
	cmp_key.manuf.ptr = manufP;
	cmp_key.manuf.len = manufL;
	cmp_key.proto.ptr = protoP;
	cmp_key.proto.len = protoL;
	cmp_key.scale = scale;
	parentP = (carPartParent_p)LookupListElem( &carPartParent_da, &cmp_key, Cmp_partparent, sizeof * parentP);
	if ( parentP->manuf == NULL ) {
		parentP->manuf = (char*)MyMalloc( manufL+1 );
		memcpy( parentP->manuf, manufP, manufL );
		parentP->manuf[manufL] = '\0';
		parentP->proto = (char*)MyMalloc( protoL+1 );
		memcpy( parentP->proto, protoP, protoL );
		parentP->proto[protoL] = '\0';
		parentP->scale = scale;
	}
	return parentP;
}


static carPart_p CarPartNew(
		carPart_p partP,
		int paramFileIndex,
		SCALEINX_T scaleInx,
		char * title,
		long options,
		long type,
		carDim_t *dim,
		wDrawColor color)
{
	carPartParent_p parentP;
	carPart_t cmp_key;
	tabString_t tabs[7];

	TabStringExtract( title, 7, tabs );
	if ( TabStringCmp( "Undecorated", &tabs[T_MANUF] ) == 0 ||
		 TabStringCmp( "Custom", &tabs[T_MANUF] ) == 0 ||
		 tabs[T_PART].len == 0 )
		return NULL;
	if ( tabs[T_PROTO].len == 0 )
		return NULL;
	if ( partP == NULL ) {
		partP = CarPartFind( tabs[T_MANUF].ptr, tabs[T_MANUF].len, tabs[T_PART].ptr, tabs[T_PART].len, scaleInx );
		if ( partP != NULL &&
			 partP->paramFileIndex == PARAM_CUSTOM &&
			 paramFileIndex != PARAM_CUSTOM )
			return partP;
LOG( log_carList, 2, ( "new car part: %s (%d) at %d\n", title, paramFileIndex, lookupListIndex ) )
	}
	if ( partP != NULL ) {
		CarPartUnlink( partP );
		if ( partP->title != NULL )
			MyFree( partP->title );
LOG( log_carList, 2, ( "upd car part: %s (%d)\n", title, paramFileIndex ) )
	}
	LoadRoadnameList( &tabs[T_ROADNAME], &tabs[T_REPMARK] );
	parentP = CarPartParentNew( tabs[T_MANUF].ptr, tabs[T_MANUF].len, tabs[T_PROTO].ptr, tabs[T_PROTO].len, scaleInx );
	cmp_key.title = title;
	cmp_key.parent = parentP;
	cmp_key.paramFileIndex = paramFileIndex;
	cmp_key.options = options;
	cmp_key.type = type;
	cmp_key.dim = *dim;
	cmp_key.color = color;
	cmp_key.partnoP = tabs[T_PART].ptr;
	cmp_key.partnoL = tabs[T_PART].len;
	partP = (carPart_p)LookupListElem( &parentP->parts_da, &cmp_key, Cmp_part, sizeof * partP );
	if ( partP->title != NULL )
		MyFree( partP->title );
	*partP = cmp_key;
	sprintf( message, "\t\t%s", tabs[2].ptr );
	partP->title = MyStrdup( message );
	partP->partnoP = partP->title + 2+tabs[2].len+1;;
	partP->partnoL = tabs[T_PART].len;
	return partP;
}


static void CarPartDelete(
		carPart_p partP )
{
	if ( partP == NULL )
		return;
	CarPartUnlink( partP );
	if ( partP->title )
		MyFree( partP->title );
	MyFree( partP );
}


static BOOL_T CarPartRead(
		char * line )
{
	char scale[10];
	long options;
	long type;
	char * title;
	carDim_t dim;
	long rgb;

	if ( !GetArgs( line+8, "sqllff00ffl",
		scale, &title, &options, &type, &dim.carLength, &dim.carWidth, &dim.truckCenter, &dim.coupledLength, &rgb ) )
		return FALSE;
	CarPartNew( NULL, curParamFileIndex, LookupScale(scale), title, options, type, &dim, wDrawFindColor(rgb) );
	MyFree( title );
	return TRUE;
}


static BOOL_T CarPartWrite(
		FILE * f,
		carPart_p partP )
{
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;
	carPartParent_p parentP=partP->parent;
	tabString_t tabs[7];

	oldLocale = SaveLocale("C");

	TabStringExtract( partP->title, 7, tabs );
	sprintf( message, "%s\t%s\t%.*s\t%.*s\t%.*s\t%.*s\t%.*s",
		parentP->manuf, parentP->proto,
		tabs[T_DESC].len, tabs[T_DESC].ptr,
		tabs[T_PART].len, tabs[T_PART].ptr,
		tabs[T_ROADNAME].len, tabs[T_ROADNAME].ptr,
		tabs[T_REPMARK].len, tabs[T_REPMARK].ptr,
		tabs[T_NUMBER].len, tabs[T_NUMBER].ptr );
	rc &= fprintf( f, "CARPART %s \"%s\"", GetScaleName(partP->parent->scale), PutTitle(message) )>0;
	rc &= fprintf( f, " %ld %ld %0.3f %0.3f 0 0 %0.3f %0.3f %ld\n",
		partP->options, partP->type, partP->dim.carLength, partP->dim.carWidth, partP->dim.truckCenter, partP->dim.coupledLength, wDrawGetRGB(partP->color) )>0;

	RestoreLocale(oldLocale);

	return rc;
}



static BOOL_T CarDescCustomSave(
		FILE * f )
{
	int parentX;
	carPartParent_p parentP;
	int partX;
	carPart_p partP;
	BOOL_T rc = TRUE;

	for ( parentX=0; parentX<carPartParent_da.cnt; parentX++ ) {
		parentP = carPartParent(parentX);
		for ( partX=0; partX<parentP->parts_da.cnt; partX++ ) {
			partP = carPart(parentP,partX);
			if ( partP->paramFileIndex == PARAM_CUSTOM )
				rc &= CarPartWrite(f, partP );
		}
	}
	return rc;
}



/*
 *  Car Item
 */

static dynArr_t carItemInfo_da;
#define carItemInfo(N) DYNARR_N( carItem_t*, carItemInfo_da, N )

#define N_CONDLISTMAP			(6)
static nameLongMap_t condListMap[N_CONDLISTMAP] = {
			{ N_("N/A"), 0 },
			{ N_("Mint"), 100 },
			{ N_("Excellent"), 80 },
			{ N_("Good"), 60 },
			{ N_("Fair"), 40 },
			{ N_("Poor"), 20 } };


static wIndex_t MapCondition(
		long conditionValue )
{
		if ( conditionValue < 10 )
			return 0;
		else if ( conditionValue < 30 )
			return 5;
		else if ( conditionValue < 50 )
			return 4;
		else if ( conditionValue < 70 )
			return 3;
		else if ( conditionValue < 90 )
			return 2;
		else
			return 1;
}


static carItem_p CarItemNew(
		carItem_p item,
		int paramFileIndex,
		long itemIndex,
		SCALEINX_T scale,
		char * title,
		long options,
		long type,
		carDim_t *dim,
		wDrawColor color,
		FLOAT_T purchPrice,
		FLOAT_T currPrice,
		long condition,
		long purchDate,
		long serviceDate )
{
	carPart_p partP;
	tabString_t tabs[7];

	TabStringExtract( title, 7, tabs );
	if ( paramFileIndex != PARAM_CUSTOM ) {
		partP = CarPartFind( tabs[T_MANUF].ptr, tabs[T_MANUF].len, tabs[T_PART].ptr, tabs[T_PART].len, scale );
		if ( partP == NULL ) {
			CarPartNew( NULL, PARAM_LAYOUT, scale, title, options, type, dim, color );
		}
	}

	if ( item == NULL ) {
		DYNARR_APPEND( carItem_t*, carItemInfo_da, 10 );
		item = (carItem_t*)MyMalloc( sizeof * item );
		carItemInfo(carItemInfo_da.cnt-1) = item;
	} else {
		if ( item->title ) MyFree( item->title );
		if ( item->data.number ) MyFree( item->data.number );
	}
	item->index = itemIndex;
	item->scaleInx = scale;
	item->title = MyStrdup(title);
	item->contentsLabel = "Car Item";
	item->barScale = curBarScale>0?curBarScale:(60.0*12.0/curScaleRatio);
	item->options = options;
	item->type = type;
	item->dim = *dim;
	item->color = color;
	if ( tabs[T_REPMARK].len>0 || tabs[T_NUMBER].len>0 ) {
		sprintf( message, "%.*s%s%.*s", tabs[T_REPMARK].len, tabs[T_REPMARK].ptr, (tabs[T_REPMARK].len>0&&tabs[T_NUMBER].len>0)?" ":"", tabs[T_NUMBER].len, tabs[T_NUMBER].ptr );
	} else {
		sprintf( message, "#%ld", item->index );
	}
	item->data.number = MyStrdup( message );
	item->data.purchPrice = purchPrice;
	item->data.currPrice = currPrice;
	item->data.condition = condition;
	item->data.purchDate = purchDate;
	item->data.serviceDate = serviceDate;
	item->data.notes = NULL;
	item->segCnt = 0;
	item->segPtr = NULL;
	LoadRoadnameList( &tabs[T_ROADNAME], &tabs[T_REPMARK] );
	return item;
}


EXPORT BOOL_T CarItemRead(
		char * line )
{
	long itemIndex;
	char scale[10];
	char * title;
	long options;
	long type;
	carDim_t dim;
	long rgb;
	FLOAT_T purchPrice = 0;
	FLOAT_T currPrice = 0;
	long condition = 0;
	long purchDate = 0;
	long serviceDate = 0;
	int len, siz;
	static dynArr_t buffer_da;
	carItem_p item;
	char * cp;
	wIndex_t layer;
	coOrd pos;
	ANGLE_T angle;
	wIndex_t index;

	if ( !GetArgs( line+4, "lsqll" "ff00ffl" "fflll000000c",
		&itemIndex, scale, &title, &options, &type,
		&dim.carLength, &dim.carWidth, &dim.truckCenter, &dim.coupledLength, &rgb,
		&purchPrice, &currPrice, &condition, &purchDate, &serviceDate, &cp ) )
		return FALSE;
	if ( (options&CAR_ITEM_HASNOTES) ) {
		DYNARR_SET( char, buffer_da, 0 );
		while ( (line=GetNextLine()) && strncmp( line, "    END", 7 ) != 0 ) {
			siz = buffer_da.cnt;
			len = strlen( line );
			DYNARR_SET( char, buffer_da, siz+len+1 );
			memcpy( &((char*)buffer_da.ptr)[siz], line, len );
			((char*)buffer_da.ptr)[siz+len] = '\n';
		}
		DYNARR_APPEND( char, buffer_da, 1 );
		((char*)buffer_da.ptr)[buffer_da.cnt-1] = 0;
	}
	item = CarItemNew( NULL, curParamFileIndex, itemIndex, LookupScale(scale), title,
				options&(CAR_DESC_BITS|CAR_ITEM_BITS), type, &dim, wDrawFindColor(rgb),
				purchPrice, currPrice, condition, purchDate, serviceDate );
	if ( (options&CAR_ITEM_HASNOTES) )
		item->data.notes = MyStrdup( (char*)buffer_da.ptr );
	MyFree(title);
	if ( (options&CAR_ITEM_ONLAYOUT) ) {
		if ( !GetArgs( cp, "dLpf",
				&index, &layer, &pos, &angle ) )
			return FALSE;
		item->car = NewCar( index, item, pos, angle );
		SetTrkLayer( item->car, layer );
		ReadSegs();
		SetEndPts( item->car, 2 );
		ComputeBoundingBox( item->car );
	}
	return TRUE;
}


static BOOL_T CarItemWrite(
		FILE * f,
		carItem_t * item,
		BOOL_T layout )
{
	long options = (item->options&CAR_DESC_BITS);
	coOrd pos;
	ANGLE_T angle;
	BOOL_T rc = TRUE;
	char *oldLocale = NULL;

	oldLocale = SaveLocale("C");

	if ( item->data.notes && item->data.notes[0] )
		options |= CAR_ITEM_HASNOTES;
	if ( layout && item->car && !IsTrackDeleted(item->car) )
		options |= CAR_ITEM_ONLAYOUT;
	rc &= fprintf( f, "CAR %ld %s \"%s\" %ld %ld %0.3f %0.3f 0 0 %0.3f %0.3f %ld %0.3f %0.3f %ld %ld %ld 0 0 0 0 0 0",
		item->index, GetScaleName(item->scaleInx), PutTitle(item->title),
		options, item->type,
		item->dim.carLength, item->dim.carWidth, item->dim.truckCenter, item->dim.coupledLength, wDrawGetRGB(item->color),
		item->data.purchPrice, item->data.currPrice, item->data.condition, item->data.purchDate, item->data.serviceDate )>0;
	if ( ( options&CAR_ITEM_ONLAYOUT) ) {
		CarGetPos( item->car, &pos, &angle );
		rc &= fprintf( f, " %d %d %0.3f %0.3f %0.3f",
				GetTrkIndex(item->car), GetTrkLayer(item->car), pos.x, pos.y, angle )>0;
	}
	rc &= fprintf( f, "\n" )>0;
	if ( (options&CAR_ITEM_HASNOTES) ) {
		rc &= fprintf( f, "%s\n", item->data.notes )>0;
		rc &= fprintf( f, "    END\n" )>0;
	}
	if ( (options&CAR_ITEM_ONLAYOUT) ) {
		rc &= WriteEndPt( f, item->car, 0 );
		rc &= WriteEndPt( f, item->car, 1 );
		rc &= fprintf( f, "\tEND\n" )>0;
	}

	RestoreLocale(oldLocale);

	return rc;
}



EXPORT carItem_p CarItemFind(
		long itemInx )
{
	if ( itemInx >= 0 && itemInx < carItemInfo_da.cnt )
		return carItemInfo(itemInx);
	else
		return NULL;
}


EXPORT long CarItemFindIndex(
		carItem_p item )
{
	long inx;
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ )
		if ( carItemInfo(inx) == item )
			return inx;
	AbortProg( "carItemFindIndex" );
	return -1;
}


EXPORT void CarItemGetSegs(
		carItem_p item )
{
	coOrd orig;
	carProto_p protoP;
	tabString_t tabs[7];
	trkSeg_t * segPtr;
	DIST_T ratio = GetScaleRatio(item->scaleInx);

	TabStringExtract( item->title, 7, tabs );
	TabStringCpy( message, &tabs[T_PROTO] );
	protoP = CarProtoLookup( message, FALSE, FALSE, 0.0, 0.0 );
	if ( protoP != NULL ) {
		item->segCnt = protoP->segCnt;
		segPtr = protoP->segPtr;
		orig = protoP->orig;
	} else {
		CarProtoDlgCreateDummyOutline( &item->segCnt, &segPtr, (item->options&CAR_DESC_IS_LOCO)!=0, item->dim.carLength, item->dim.carWidth, item->color );
		orig = zero;
	}
	item->segPtr = (trkSeg_p)MyMalloc( item->segCnt * sizeof *(segPtr) );
	memcpy( item->segPtr, segPtr, item->segCnt * sizeof *(segPtr) );
	CloneFilledDraw( item->segCnt, item->segPtr, FALSE );
	if ( protoP ) {
		orig.x = -orig.x;
		orig.y = -orig.y;
		MoveSegs( item->segCnt, item->segPtr, orig );
		RescaleSegs( item->segCnt, item->segPtr, item->dim.carLength/protoP->size.x, item->dim.carWidth/protoP->size.y, 1/ratio );
		RecolorSegs( item->segCnt, item->segPtr, item->color );
	}
}


EXPORT BOOL_T WriteCars(
		FILE * f )
{
	int inx;
	BOOL_T rc = TRUE;
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ )
		rc &= CarItemWrite( f, carItemInfo(inx), TRUE );
	return rc;
}


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
static long newCarInx;
static paramData_t newCarPLs[] = {
	{ PD_DROPLIST, &newCarInx, "index", PDO_DLGWIDE, (void*)400, N_("Item") } };
static paramGroup_t newCarPG = { "train-newcar", 0, newCarPLs, sizeof newCarPLs/sizeof newCarPLs[0] };
EXPORT wControl_p newCarControls[2];
static char newCarLabel1[STR_SIZE];
static char * newCarLabels[2] = { newCarLabel1, NULL };

static dynArr_t carItemHotbar_da;
#define carItemHotbar(N)			DYNARR_N( carItem_p, carItemHotbar_da, N )


static int Cmp_carHotbar(
		const void * ptr1,
		const void * ptr2 )
{
	carItem_p item1 = *(carItem_p*)ptr1;
	carItem_p item2 = *(carItem_p*)ptr2;
	tabString_t tabs1[7], tabs2[7];
	int rc;
	long mode;

	TabStringExtract( item1->title, 7, tabs1 );
	TabStringExtract( item2->title, 7, tabs2 );
	for ( mode=carHotbarModes[carHotbarModeInx],rc=0; mode!=0&&rc==0; mode>>=4 ) {
		switch ( mode&0x000F ) {
		case 4:
			rc = (int)(item1->index-item2->index);
			break;
		case 1:
			rc = strncasecmp( tabs1[T_MANUF].ptr, tabs2[T_MANUF].ptr, max(tabs1[T_MANUF].len,tabs2[T_MANUF].len) );
			break;
		case 3:
			rc = strncasecmp( tabs1[T_PART].ptr, tabs2[T_PART].ptr, max(tabs1[T_PART].len,tabs2[T_PART].len) );
			break;
		case 2:
			if ( item1->type < item2->type )
				rc = -1;
			else if ( item1->type > item2->type )
				rc = 1;
			else
				rc = strncasecmp( tabs1[T_PROTO].ptr, tabs2[T_PROTO].ptr, max(tabs1[T_PROTO].len,tabs2[T_PROTO].len) );
			break;
		}
	}
	return rc;
}


static void CarItemHotbarUpdate(
		paramGroup_p pg,
		int inx,
		void * data )
{
	wIndex_t carItemInx;
	carItem_p item;
	if ( inx == 0 ) {
		carItemInx = (wIndex_t)*(long*)data;
		if ( carItemInx < 0 )
			return;
		carItemInx = (wIndex_t)(long)wListGetItemContext( (wList_p)pg->paramPtr[inx].control, carItemInx );
		item = carItemHotbar(carItemInx);
		if ( item != NULL )
			currCarItemPtr = item;
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


EXPORT char * CarItemDescribe(
		carItem_p item,
		long mode,
		long * index )
{
	tabString_t tabs[7];
	char * cp;
	static char desc[STR_LONG_SIZE];

	TabStringExtract( item->title, 7, tabs );
	cp = desc;
	if ( mode != -1 ) {
		sprintf( cp, "%ld ", item->index );
		cp = desc+strlen(cp);
	}
	if ( (mode&0xF)!=1 && ((mode>>4)&0xF)!=1 && ((mode>>8)&0xF)!=1 && ((mode>>12)&0xF)!=1 ) {
		cp = TabStringCpy( cp, &tabs[T_MANUF] );
		*cp++ = ' ';
	}
	if ( (mode&0xF)!=3 && ((mode>>4)&0xF)!=3 && ((mode>>8)&0xF)!=3 && ((mode>>12)&0xF)!=3 ) {
		cp = TabStringCpy( cp, &tabs[T_PART] );
		*cp++ = ' ';
	}
	if ( (mode&0xF)!=2 && ((mode>>4)&0xF)!=2 && ((mode>>8)&0xF)!=2 && ((mode>>12)&0xF)!=2 ) {
		cp = TabStringCpy( cp, &tabs[T_PROTO] );
		*cp++ = ' ';
	}
	if ( tabs[T_DESC].len > 0 ) {
		cp = TabStringCpy( cp, &tabs[T_DESC] );
		*cp++ = ' ';
	}
	if ( mode != -1 ) {
		if ( tabs[T_REPMARK].len > 0 ) {
			cp = TabStringCpy( cp, &tabs[T_REPMARK] );
			*cp++ = ' ';
		} else if ( tabs[T_ROADNAME].len > 0 ) {
			cp = TabStringCpy( cp, &tabs[T_ROADNAME] );
			*cp++ = ' ';
		}
		if ( tabs[T_NUMBER].len > 0 ) {
			cp = TabStringCpy( cp, &tabs[T_NUMBER] );
			*cp++ = ' ';
		}
	}
	*--cp = '\0';
	if ( index != NULL )
		*index = item->index;
	return desc;
}


EXPORT void CarItemLoadList( void * junk )
{
	wIndex_t inx;
	carItem_p item;
	char * cp;
	wPos_t w, h;

	DYNARR_SET( carItem_t*, carItemHotbar_da, carItemInfo_da.cnt );
	memcpy( carItemHotbar_da.ptr, carItemInfo_da.ptr, carItemInfo_da.cnt * sizeof item );
	wListClear( (wList_p)newCarPLs[0].control );
	for ( inx=0; inx<carItemHotbar_da.cnt; inx++ ) {
		item = carItemHotbar(inx);
		if ( item->car && !IsTrackDeleted(item->car) )
			continue;
		cp = CarItemDescribe( item, 0, NULL );
		wListAddValue( (wList_p)newCarPLs[0].control, cp, NULL, (void*)(intptr_t)inx );
	}
	/*wListSetValue( (wList_p)newCarPLs[0].control, "Select a car" );*/
	wListSetIndex( (wList_p)newCarPLs[0].control, 0 );
	strcpy( newCarLabel1, _("Select") );
	ParamLoadControl( &newCarPG, 0 );
	InfoSubstituteControls( newCarControls, newCarLabels );
	wWinGetSize( mainW, &w, &h );
	w -= wControlGetPosX( newCarControls[0] ) + 4;
	if ( w > 20 )
		wListSetSize( (wList_p)newCarControls[0], w, wControlGetHeight( newCarControls[0] ) );
}


static char * CarItemHotbarProc(
		hotBarProc_e op,
		void * data,
		drawCmd_p d,
		coOrd * origP )
{
	wIndex_t carItemInx = (wIndex_t)(long)data;
	carItem_p item;
	wIndex_t inx;
	long mode;
	char * cp;
	wPos_t w, h;

	item = carItemHotbar(carItemInx);
	if ( item == NULL )
		return NULL;
	switch ( op ) {
	case HB_SELECT:
		currCarItemPtr = item;
		mode = carHotbarModes[carHotbarModeInx];
		if ( (mode&0xF000) == 0 ) {
			wListClear( (wList_p)newCarPLs[0].control );
			for ( inx=carItemInx;
				  inx<carItemHotbar_da.cnt && ( inx==carItemInx || Cmp_carHotbar(&carItemHotbar(carItemInx),&carItemHotbar(inx))==0 );
				  inx++ ) {
				item = carItemHotbar(inx);
				if ( item->car && !IsTrackDeleted(item->car) )
					continue;
				cp = CarItemDescribe( item, mode, NULL );
				wListAddValue( (wList_p)newCarPLs[0].control, cp, NULL, (void*)(intptr_t)inx );
			}
			/*wListSetValue( (wList_p)newCarPLs[0].control, "Select a car" );*/
			wListSetIndex( (wList_p)newCarPLs[0].control, 0 );
			cp = CarItemHotbarProc( HB_BARTITLE, (void*)(intptr_t)carItemInx, NULL, NULL );
			strncpy( newCarLabel1, cp, sizeof newCarLabel1 );
			ParamLoadControls( &newCarPG );
			ParamGroupRecord( &newCarPG );

			InfoSubstituteControls( newCarControls, newCarLabels );
			wWinGetSize( mainW, &w, &h );
			w -= wControlGetPosX( newCarControls[0] ) + 4;
			if ( w > 20 )
				wListSetSize( (wList_p)newCarControls[0], w, wControlGetHeight( newCarControls[0] ) );
		} else {
			InfoSubstituteControls( NULL, NULL );
			cp = CarItemDescribe( item, 0, NULL );
			InfoMessage( cp );
		}
		break;
	case HB_LISTTITLE:
	case HB_BARTITLE:
		return FormatCarTitle( item, carHotbarModes[carHotbarModeInx] );
	case HB_FULLTITLE:
		return item->title;
	case HB_DRAW:
		if ( item->segCnt == 0 )
			CarItemGetSegs( item );
		DrawSegs( d, *origP, 0.0, item->segPtr, item->segCnt, trackGauge, wDrawColorBlack );
		return NULL;
	}
	return NULL;
}


EXPORT int CarAvailableCount( void )
{
	wIndex_t inx;
	int cnt = 0;
	carItem_t * item;
	for ( inx=0; inx < carItemHotbar_da.cnt; inx ++ ) {
		item = carItemHotbar(inx);
		if ( item->scaleInx != curScaleInx )
			continue;
		cnt++;
	}
	return cnt;
}


EXPORT void AddHotBarCarDesc( void )
{
	wIndex_t inx;
	carItem_t * item0, * item1;
	coOrd orig;
	coOrd size;

	DYNARR_SET( carItem_t*, carItemHotbar_da, carItemInfo_da.cnt );
	memcpy( carItemHotbar_da.ptr, carItemInfo_da.ptr, carItemInfo_da.cnt * sizeof item0 );
	qsort( carItemHotbar_da.ptr, carItemHotbar_da.cnt, sizeof item0, Cmp_carHotbar );
	for ( inx=0,item0=NULL; inx < carItemHotbar_da.cnt; inx ++ ) {
		item1 = carItemHotbar(inx);
		if ( item1->car && !IsTrackDeleted(item1->car) )
			continue;
		if ( item1->scaleInx != curScaleInx )
			continue;
		if ( (carHotbarModes[carHotbarModeInx]&0xF000)!=0 || ( item0 == NULL || Cmp_carHotbar( &item0, &item1 ) != 0 ) ) {
#ifdef DESCFIX
			orig.x = - item->orig.x;
			orig.y = - item->orig.y;
#endif
			orig = zero;
			size.x = item1->dim.carLength;
			size.y = item1->dim.carWidth;
			AddHotBarElement( FormatCarTitle( item1, carHotbarContents[carHotbarModeInx] ), size, orig, FALSE, (60.0*12.0/curScaleRatio), (void*)(intptr_t)inx, CarItemHotbarProc );
		}
		item0 = item1;
	}
}


EXPORT coOrd CarItemFindCouplerMountPoint(
		carItem_p item,
		traverseTrack_t trvTrk,
		int dir )
{
	DIST_T couplerOffset;
	coOrd pos;

	if ( dir )
		FlipTraverseTrack( &trvTrk );
	if ( trvTrk.trk == NULL || (item->options&CAR_DESC_COUPLER_MODE_BODY)!=0 ) {
		couplerOffset = (item->dim.carLength-(item->dim.coupledLength-item->dim.carLength))/2.0;
		Translate( &pos, trvTrk.pos, trvTrk.angle, couplerOffset );
	} else {
		TraverseTrack2( &trvTrk, item->dim.truckCenter/2.0 );
		/*Translate( &pos1, trvTrk.pos, trvTrk.angle, item->dim.truckCenter/2.0 );*/
		couplerOffset = item->dim.carLength - (item->dim.truckCenter+item->dim.coupledLength)/2.0;
		Translate( &pos, trvTrk.pos, trvTrk.angle, couplerOffset );
	}
	return pos;
}


EXPORT void CarItemSize(
		carItem_p item,
		coOrd * size )
{
	size->x = item->dim.carLength;
	size->y = item->dim.carWidth;
}


EXPORT char * CarItemNumber(
		carItem_p item )
{
	return item->data.number;
}


static DIST_T CarItemTruckCenter(
		carItem_p item )
{
	return item->dim.truckCenter;
}


EXPORT DIST_T CarItemCoupledLength(
		carItem_p item )
{
	return item->dim.coupledLength;
}


EXPORT BOOL_T CarItemIsLoco(
		carItem_p item )
{
	return (item->options&CAR_DESC_IS_LOCO) == (CAR_DESC_IS_LOCO);
}


EXPORT BOOL_T CarItemIsLocoMaster(
		carItem_p item )
{
	return (item->options&(CAR_DESC_IS_LOCO|CAR_DESC_IS_LOCO_MASTER)) == (CAR_DESC_IS_LOCO|CAR_DESC_IS_LOCO_MASTER);
}


EXPORT void CarItemSetLocoMaster(
		carItem_p item,
		BOOL_T locoIsMaster )
{
	if ( locoIsMaster )
		item->options |= CAR_DESC_IS_LOCO_MASTER;
	else
		item->options &= ~CAR_DESC_IS_LOCO_MASTER;
}


EXPORT void CarItemSetTrack(
		carItem_p item,
		track_p trk )
{
	item->car = trk;
	if ( trk != NULL )
		SetTrkScale( trk, item->scaleInx );
}

static DIST_T CarItemCouplerLength(
		carItem_p item,
		int dir )
{
	return item->dim.coupledLength-item->dim.carLength;
}


EXPORT void CarItemPlace(
		carItem_p item,
		traverseTrack_p trvTrk,
		DIST_T * dists )
{
	DIST_T dist;
	traverseTrack_t trks[2];

	dist = CarItemTruckCenter(item)/2.0;
	trks[0] = trks[1] = *trvTrk;
	TraverseTrack2( &trks[0], dist );
	TraverseTrack2( &trks[1], -dist );
	item->pos.x = (trks[0].pos.x+trks[1].pos.x)/2.0;
	item->pos.y = (trks[0].pos.y+trks[1].pos.y)/2.0;
	item->angle = FindAngle( trks[1].pos, trks[0].pos );
	dists[0] = dists[1] = CarItemCoupledLength(item)/2.0;
}



static int drawCarTrucks = 0;
EXPORT void CarItemDraw(
		drawCmd_p d,
		carItem_p item,
		wDrawColor color,
		int direction,
		BOOL_T locoIsMaster,
		vector_t *coupler )
{
	coOrd size, pos, pos2;
	DIST_T length;
	wFont_p fp;
	wDrawWidth width;
	trkSeg_t simpleSegs[1];
	coOrd simplePts[4];
	int dir;
	DIST_T rad;
	static int couplerLineWidth = 3;
	DIST_T scale2rail;

	CarItemSize( item, &size );
	if ( d->scale >= ((d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale) ) {
		simplePts[0].x = simplePts[3].x = -size.x/2.0;
		simplePts[1].x = simplePts[2].x = size.x/2.0;
		simplePts[0].y = simplePts[1].y = -size.y/2.0;
		simplePts[2].y = simplePts[3].y = size.y/2.0;
		simpleSegs[0].type = SEG_FILPOLY;
		simpleSegs[0].color = item->color;
		simpleSegs[0].width = 0;
		simpleSegs[0].u.p.cnt = 4;
		simpleSegs[0].u.p.pts = simplePts;
		simpleSegs[0].u.p.orig = zero;
		simpleSegs[0].u.p.angle = 0.0;
		DrawSegs( d, item->pos, item->angle-90.0, simpleSegs, 1, 0.0, color );
	} else {
		if ( item->segCnt == 0 )
			CarItemGetSegs( item );
		Translate( &pos, item->pos, item->angle, -size.x/2.0 );
		Translate( &pos, pos, item->angle-90, -size.y/2.0 );
		DrawSegs( d, pos, item->angle-90.0, item->segPtr, item->segCnt, 0.0, color );
	}

	if ( drawCarTrucks ) {
		length = item->dim.truckCenter/2.0;
		Translate( &pos, item->pos, item->angle, length );
		DrawArc( d, pos, trackGauge/2.0, 0.0, 360.0, FALSE, 0, color );
		Translate( &pos, item->pos, item->angle+180, length );
		DrawArc( d, pos, trackGauge/2.0, 0.0, 360.0, FALSE, 0, color );
	}

	if ( (labelEnable&LABELENABLE_CARS) ) {
		fp = wStandardFont( F_HELV, FALSE, FALSE );
		DrawBoxedString( BOX_BACKGROUND, d, item->pos, item->data.number, fp, (wFontSize_t)descriptionFontSize, color, 0.0 );
	}

	/* draw loco head light */
	if ( (item->options&CAR_DESC_IS_LOCO)!=0 ) {
		Translate( &pos, item->pos, item->angle+(direction?180.0:0.0), size.x/2.0-trackGauge/2.0 );
		if ( locoIsMaster ) {
			DrawFillCircle( d, pos, trackGauge/2.0, (color==wDrawColorBlack?drawColorGold:color) );
		} else {
			width = (wDrawWidth)floor( trackGauge/8.0 * d->dpi / d->scale );
			DrawArc( d, pos, trackGauge/2.0, 0.0, 360.0, FALSE, width, (color==wDrawColorBlack?drawColorGold:color) );
		}
	}

	/* draw coupler */
	scale2rail = ((d->options&DC_PRINT)?(twoRailScale*2+1):twoRailScale);
	if ( d->scale >= scale2rail )
		return;
	scale2rail /= 2;
	rad = trackGauge/8.0;
	for ( dir=0; dir<2; dir++ ) {
		Translate( &pos, coupler[dir].pos, coupler[dir].angle, CarItemCouplerLength(item,dir) );
		DrawLine( d, coupler[dir].pos, pos, couplerLineWidth, color );
		if ( d->scale < scale2rail ) {
			/*DrawFillCircle( d, p0, rad, dir==0?color:selectedColor );*/
			Translate( &pos2, pos, coupler[dir].angle+90.0, trackGauge/3 );
			DrawLine( d, pos2, pos, couplerLineWidth, color );
		}
	}
}


EXPORT void CarItemUpdate(
		carItem_p item )
{
	DoChangeNotification( CHANGE_SCALE );
}

/*
 * Car Item/Part Dlg
 */

static int carDlgChanged;

static SCALEINX_T carDlgScaleInx;
static carItem_p carDlgUpdateItemPtr;
static carPart_p carDlgUpdatePartPtr;
static carProto_p carDlgUpdateProtoPtr;
static carPart_p carDlgNewPartPtr;
static carProto_p carDlgNewProtoPtr;

static BOOL_T carDlgFlipToggle;

static wIndex_t carDlgManufInx;
static char carDlgManufStr[STR_SIZE];
static wIndex_t carDlgKindInx;
static wIndex_t carDlgProtoInx;
static char carDlgProtoStr[STR_SIZE];
static wIndex_t carDlgPartnoInx;
static char carDlgPartnoStr[STR_SIZE];
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

static long carDlgItemIndex = 1;
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
		DC_NOCLIP,
		1.0,
		0.0,
		{ 0, 0 }, { 0, 0 },
		Pix2CoOrd, CoOrd2Pix };
static void CarDlgRedraw(void);
static paramDrawData_t carDlgDrawData = { 455, 100, (wDrawRedrawCallBack_p)CarDlgRedraw, NULL, &carDlgD };
static paramTextData_t notesData = { 440, 100 };
static char *multinumLabels[] = { N_("Sequential"), N_("Repeated"), NULL };
static void CarDlgNewProto( void );
static void CarDlgUpdate( paramGroup_p, int, void * );
static void CarDlgNewDesc( void );
static void CarDlgNewProto( void );

static paramData_t carDlgPLs[] = {
#define A                       (0)
#define I_CD_MANUF_LIST         (A+0)
	{ PD_DROPLIST, &carDlgManufInx, "manuf", PDO_NOPREF, (void*)350, N_("Manufacturer"), BL_EDITABLE },
#define I_CD_PROTOTYPE_STR      (A+1)
	{ PD_STRING, &carDlgProtoStr, "prototype", PDO_NOPREF, (void*)350, N_("Prototype") },
#define I_CD_PROTOKIND_LIST     (A+2)
	{ PD_DROPLIST, &carDlgKindInx, "protokind-list", PDO_NOPREF, (void*)125, N_("Prototype"), 0 },
#define I_CD_PROTOTYPE_LIST     (A+3)
	{ PD_DROPLIST, &carDlgProtoInx, "prototype-list", PDO_NOPREF|PDO_DLGHORZ, (void*)(225-3), NULL, 0 },
#define I_CD_TYPE_LIST          (A+4)
	{ PD_DROPLIST, &carDlgTypeInx, "type", PDO_NOPREF, (void*)350, N_("Type"), 0 },
#define I_CD_PARTNO_LIST        (A+5)
	{ PD_DROPLIST, &carDlgPartnoInx, "partno-list", PDO_NOPREF, (void*)350, N_("Part"), BL_EDITABLE },
#define I_CD_PARTNO_STR         (A+6)
	{ PD_STRING, &carDlgPartnoStr, "partno", PDO_NOPREF, (void*)350, N_("Part Number") },
#define I_CD_ISLOCO             (A+7)
	{ PD_TOGGLE, &carDlgIsLoco, "isLoco", PDO_NOPREF|PDO_DLGWIDE, isLocoLabels, N_("Loco?"), BC_HORZ|BC_NOBORDER },
#define I_CD_DESC_STR           (A+8)
	{ PD_STRING, &carDlgDescStr, "desc", PDO_NOPREF, (void*)350, N_("Description"), 0 },
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
	{ PD_DROPLIST, &carDlgRoadnameInx, "road", PDO_NOPREF|PDO_DLGWIDE, (void*)350, N_("Road"), BL_EDITABLE },
#define I_CD_REPMARK            (B+1)
	{ PD_STRING, carDlgRepmarkStr, "repmark", PDO_NOPREF, (void*)60, N_("Reporting Mark") },
#define I_CD_NUMBER             (B+2)
	{ PD_STRING, carDlgNumberStr, "number", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, (void*)80, N_("Number") },
#define I_CD_BODYCOLOR          (B+3)
	{ PD_COLORLIST, &carDlgBodyColor, "bodyColor", PDO_DLGWIDE|PDO_DLGHORZ, NULL, N_("Color") },
#define I_CD_CARLENGTH          (B+4)
	{ PD_FLOAT, &carDlgDim.carLength, "carLength", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE, &r0_99999, N_("Car Length") },
#define I_CD_CARWIDTH           (B+5)
	{ PD_FLOAT, &carDlgDim.carWidth, "carWidth", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, &r0_99999, N_("Width") },
#define I_CD_TRKCENTER          (B+6)
	{ PD_FLOAT, &carDlgDim.truckCenter, "trkCenter", PDO_DIM|PDO_NOPREF, &r0_99999, N_("Truck Centers") },
#define I_CD_CPLRMNT            (B+7)
	{ PD_RADIO, &carDlgCouplerMount, "cplrMount", PDO_NOPREF|PDO_DLGHORZ|PDO_DLGWIDE, cplrModeLabels, N_("Coupler Mount"), BC_HORZ|BC_NOBORDER },
#define I_CD_CPLDLEN            (B+8)
	{ PD_FLOAT, &carDlgDim.coupledLength, "cpldLen", PDO_DIM|PDO_NOPREF, &r0_99999, N_("Coupled Length") },
#define I_CD_CPLRLEN            (B+9)
	{ PD_FLOAT, &carDlgCouplerLength, "cplrLen", PDO_DIM|PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, &r0_99999, N_("Coupler Length") },
#define I_CD_CANVAS             (B+10)
	{ PD_DRAW, NULL, "canvas", PDO_NOPSHUPD|PDO_DLGWIDE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGBOXEND|PDO_DLGRESIZE, &carDlgDrawData, NULL, 0 },

#define C                       (B+11)
#define I_CD_ITEMINDEX          (C+0)
	{ PD_LONG, &carDlgItemIndex, "index", PDO_NOPREF|PDO_DLGWIDE, &i1_999999999, N_("Index"), 0 },
#define I_CD_PURPRC             (C+1)
	{ PD_STRING, &carDlgPurchPriceStr, "purchPrice", PDO_NOPREF|PDO_DLGWIDE, (void*)50, N_("Purchase Price"), 0, &carDlgPurchPrice },
#define I_CD_CURPRC             (C+2)
	{ PD_STRING, &carDlgCurrPriceStr, "currPrice", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, (void*)50, N_("Current Price"), 0, &carDlgCurrPrice },
#define I_CD_COND               (C+3)
	{ PD_DROPLIST, &carDlgConditionInx, "condition", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, (void*)90, N_("Condition") },
#define I_CD_PURDAT             (C+4)
	{ PD_STRING, &carDlgPurchDateStr, "purchDate",  PDO_NOPREF|PDO_DLGWIDE, (void*)80, N_("Purchase Date"), 0, &carDlgPurchDate },
#define I_CD_SRVDAT             (C+5)
	{ PD_STRING, &carDlgServiceDateStr, "serviceDate",  PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, (void*)80, N_("Service Date"), 0, &carDlgServiceDate },
#define I_CD_QTY                (C+6)
	{ PD_LONG, &carDlgQuantity, "quantity", PDO_NOPREF|PDO_DLGWIDE, &i1_9999, N_("Quantity") },
#define I_CD_MLTNUM             (C+7)
	{ PD_RADIO, &carDlgMultiNum, "multinum", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGHORZ, multinumLabels, N_("Numbers"), BC_HORZ|BC_NOBORDER },
#define I_CD_NOTES              (C+8)
	{ PD_TEXT, NULL, "notes", PDO_NOPREF|PDO_DLGWIDE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN, &notesData, N_("Notes") },

#define D                       (C+9)
#define I_CD_MSG                (D+0)
	{ PD_MESSAGE, NULL, NULL, PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN|PDO_DLGBOXEND, (void*)450 },
#define I_CD_NEW                (D+1)
	{ PD_MENU, NULL, "new-menu", PDO_DLGCMDBUTTON, NULL, N_("New"), 0, (void*)0 },
	{ PD_MENUITEM, (void*)CarDlgNewDesc, "new-part-mi", 0, NULL, N_("Car Part"), 0, (void*)0 },
	{ PD_MENUITEM, (void*)CarDlgNewProto, "new-proto-mi", 0, NULL, N_("Car Prototype"), 0, (void*)0 },
#define I_CD_NEWPROTO           (D+4)
	{ PD_BUTTON, (void*)CarDlgNewProto, "new", PDO_DLGCMDBUTTON, NULL, N_("New"), 0, (void*)0 } };

static paramGroup_t carDlgPG = { "carpart", 0, carDlgPLs, sizeof carDlgPLs/sizeof carDlgPLs[0] };


static dynArr_t carDlgSegs_da;
#define carDlgSegs(N) DYNARR_N( trkSeg_t, carDlgSegs_da, N )


typedef enum {
		T_ItemSel, T_ItemEnter, T_ProtoSel, T_ProtoEnter, T_PartnoSel, T_PartnoEnter } carDlgTransistion_e;
static char *carDlgTransistion_s[] = {
		"ItemSel", "ItemEnter", "ProtoSel", "ProtoEnter", "PartnoSel", "PartnoEnter" };
typedef enum {
		S_Error,
		S_ItemSel, S_ItemEnter, S_PartnoSel, S_PartnoEnter, S_ProtoSel } carDlgState_e;
static char *carDlgState_s[] = {
		"Error",
		"ItemSel", "ItemEnter", "PartnoSel", "PartnoEnter", "ProtoSel" };
typedef enum {
		A_Return,
		A_SError,
		A_Else,
		A_SItemSel,
		A_SItemEnter,
		A_SPartnoSel,
		A_SPartnoEnter,
		A_SProtoSel,
		A_IsCustom,
		A_IsNewPart,
		A_IsNewProto,
		A_LoadDataFromPartList,
		A_LoadDimsFromStack,
		A_LoadManufListForScale,
		A_LoadManufListAll,
		A_LoadProtoListForManuf,
		A_LoadProtoListAll,
		A_LoadPartnoList,
		A_LoadLists,
		A_LoadDimsFromProtoList,
		A_ConvertDimsToProto,
		A_Redraw,
		A_ClrManuf,
		A_ClrPartnoStr,
		A_ClrNumberStr,
		A_LoadProtoStrFromList,
		A_ShowPartnoList,
		A_HidePartnoList,
		A_PushDims,
		A_PopDims,
		A_PopTitleAndTypeinx,
		A_PopCouplerLength,
		A_ShowControls,
		A_LoadInfoFromUpdateItem,
		A_LoadDataFromUpdatePart,
		A_InitProto,
		A_RecallCouplerLength,
		A_Last
		} carDlgAction_e;
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
/*T_PartnoEnter*/{ A_SItemEnter, A_LoadProtoListAll, A_HidePartnoList } },

/*A_SItemEnter*/{
/*T_ItemSel*/    { A_SItemSel, A_LoadProtoListForManuf, A_LoadPartnoList, A_LoadDataFromPartList, A_Redraw, A_ShowPartnoList },
/*T_ItemEnter*/  { A_Return },
/*T_ProtoSel*/   { A_LoadDimsFromProtoList, A_Redraw },
/*T_ProtoEnter*/ { A_SError },
/*T_PartnoSel*/  { A_SError },
/*T_PartnoEnter*/{ A_Return } },

/*A_SPartnoSel*/{
/*T_ItemSel*/   { A_SPartnoSel },
/*T_ItemEnter*/ { A_SPartnoSel },
/*T_ProtoSel*/   { A_SPartnoSel, A_LoadDimsFromProtoList, A_Redraw },
/*T_ProtoEnter*/ { A_SError },
/*T_PartnoSel*/  { A_SError } },

/*A_SPartnoEnter*/{
/*T_ItemSel*/   { A_SPartnoSel },
/*T_ItemEnter*/ { A_SPartnoEnter },
/*T_ProtoSel*/   { A_SPartnoEnter, A_LoadDimsFromProtoList, A_Redraw },
/*T_ProtoEnter*/ { A_SError },
/*T_PartnoSel*/  { A_SError },
/*T_PartnoEnter*/{ A_SPartnoEnter } },

/*A_SProtoSel*/{
/*T_ItemSel*/   { A_SError },
/*T_ItemEnter*/ { A_SError },
/*T_ProtoSel*/   { A_SError },
/*T_ProtoEnter*/ { A_SProtoSel },
/*T_PartnoSel*/  { A_SError },
/*T_PartnoEnter*/{ A_SError } } };

static carDlgAction_e itemNewActions[] = {
				A_RecallCouplerLength,
				A_LoadLists,
				A_IsCustom, 2+3,
						A_LoadDimsFromProtoList, A_ClrPartnoStr, A_ClrNumberStr,
				A_Else, 1,
						A_LoadDataFromPartList,
				A_ShowControls, A_Return };
static carDlgAction_e itemUpdActions[] = { A_LoadInfoFromUpdateItem, /*A_LoadManufListForScale,
				A_IsCustom, 5,
						A_LoadProtoListAll, A_HidePartnoList, A_SItemEnter,
				A_Else, 5,
						A_LoadProtoListForManuf, A_LoadPartnoList, A_LoadDataFromPartList, A_ShowPartnoList, A_SItemSel,*/
				A_ShowControls, A_Return };

static carDlgAction_e partNewActions[] = { A_RecallCouplerLength, A_LoadManufListAll, A_LoadProtoListAll, A_ClrPartnoStr, A_ClrNumberStr, A_SPartnoSel, A_LoadDimsFromProtoList, A_ShowControls, A_Redraw, A_Return };
static carDlgAction_e partUpdActions[] = { A_LoadDataFromUpdatePart, A_SPartnoSel, A_ShowControls, A_Return };

static carDlgAction_e protoNewActions[] = { A_InitProto, A_SProtoSel, A_ShowControls, A_Return };
static carDlgAction_e protoUpdActions[] = { A_InitProto, A_SProtoSel, A_ShowControls, A_Return };

static carDlgAction_e item2partActions[] = {
				A_PushDims, A_LoadManufListAll, A_LoadProtoListAll,
				A_IsCustom, 0+1,
						A_ClrManuf,
				A_SPartnoSel,
				A_ShowControls, A_Return };
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
				A_Return };

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
				A_Return };

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
				A_Return };


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

	if ( partP == NULL ) return;
	carDlgDim = partP->dim;
	carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
	sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr, GetScaleName(carDlgScaleInx) );
	wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
	carDlgIsLoco = (partP->options&CAR_DESC_IS_LOCO)?1:0;
	carDlgBodyColor = partP->color;
	ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
	ParamLoadControl( &carDlgPG, I_CD_CARWIDTH );
	ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
	ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
	wColorSelectButtonSetColor( (wButton_p)carDlgPLs[I_CD_BODYCOLOR].control, *(wDrawColor*)carDlgPLs[I_CD_BODYCOLOR].valueP );
	TabStringExtract( partP->title, 7, tabs );
}


static void CarDlgLoadDimsFromProto( carProto_p protoP )
{
	DIST_T ratio = GetScaleRatio(carDlgScaleInx);
	carDlgDim.carLength = protoP->dim.carLength/ratio;
	carDlgDim.carWidth = protoP->dim.carWidth/ratio;
	carDlgDim.truckCenter = protoP->dim.truckCenter/ratio;
	carDlgDim.coupledLength = carDlgDim.carLength + carDlgCouplerLength*2;
	/*carDlgCouplerLength = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;*/
	carDlgIsLoco = (protoP->options&CAR_DESC_IS_LOCO)?1:0;
	ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
	ParamLoadControl( &carDlgPG, I_CD_CARWIDTH );
	ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
	ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
}


static void CarDlgRedraw( void )
{
	wPos_t w, h;
	DIST_T ww, hh;
	DIST_T scale_w, scale_h;
	coOrd orig, pos, size;
	carProto_p protoP;
	FLOAT_T ratio;
	int segCnt;
	trkSeg_p segPtr;

	if ( S_PROTO )
		ratio = 1;
	else
		ratio = 1/GetScaleRatio(carDlgScaleInx);
	wDrawClear( carDlgD.d );
	if ( carDlgDim.carLength <= 0 || carDlgDim.carWidth <= 0 )
		return;
	FreeFilledDraw( carDlgSegs_da.cnt, &carDlgSegs(0) );
	if ( !S_PROTO ) {
		if ( carDlgProtoInx < 0 ||
			 (protoP = CarProtoLookup( carDlgProtoStr, FALSE, FALSE, 0.0, 0.0 )) == NULL ||
			 protoP->segCnt == 0 ) {
			CarProtoDlgCreateDummyOutline( &segCnt, &segPtr, (BOOL_T)carDlgIsLoco, carDlgDim.carLength, carDlgDim.carWidth, carDlgBodyColor );
		} else {
			segCnt = protoP->segCnt;
			segPtr = protoP->segPtr;
		}
	} else {
		if ( carProtoSegCnt <= 0 ) {
			CarProtoDlgCreateDummyOutline( &segCnt, &segPtr, (BOOL_T)carDlgIsLoco, carDlgDim.carLength, carDlgDim.carWidth, drawColorBlue );
		} else {
			segCnt = carProtoSegCnt;
			segPtr = carProtoSegPtr;
		}
	}
	DYNARR_SET( trkSeg_t, carDlgSegs_da, segCnt );
	memcpy( &carDlgSegs(0), segPtr, segCnt * sizeof *(trkSeg_t*)0 );
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
	if ( scale_w > scale_h )
		carDlgD.scale = scale_w;
	else
		carDlgD.scale = scale_h;
	orig.x = 0.50*carDlgD.scale;
	orig.y = 0.25*carDlgD.scale;
	DrawSegs( &carDlgD, orig, 0.0, &carDlgSegs(0), carDlgSegs_da.cnt, 0.0, wDrawColorBlack );
	pos.y = orig.y+carDlgDim.carWidth/2.0;

	if ( carDlgDim.truckCenter > 0.0 ) {
		pos.x = orig.x+(carDlgDim.carLength-carDlgDim.truckCenter)/2.0;
		CarProtoDrawTruck( &carDlgD, trackGauge*curScaleRatio, ratio, pos, 0.0 );
		pos.x = orig.x+(carDlgDim.carLength+carDlgDim.truckCenter)/2.0;
		CarProtoDrawTruck( &carDlgD, trackGauge*curScaleRatio, ratio, pos, 0.0 );
	}
	if ( carDlgDim.coupledLength > carDlgDim.carLength ) {
		pos.x = orig.x;
		CarProtoDrawCoupler( &carDlgD, (carDlgDim.coupledLength-carDlgDim.carLength)/2.0, ratio, pos, 270.0 );
		pos.x = orig.x+carDlgDim.carLength;
		CarProtoDrawCoupler( &carDlgD, (carDlgDim.coupledLength-carDlgDim.carLength)/2.0, ratio, pos, 90.0 );
	}
}



static void CarDlgLoadRoadnameList( void )
/* Loads RoadnameList.
 * Set carDlgRoadnameInx to entry matching carDlgRoadnameStr (if found)
 * Otherwise not set
 */
{
	wIndex_t inx;
	roadnameMap_p roadnameMapP;

	if ( !roadnameMapChanged ) return;
	wListClear( (wList_p)carDlgPLs[I_CD_ROADNAME_LIST].control );
	wListAddValue( (wList_p)carDlgPLs[I_CD_ROADNAME_LIST].control, _("Undecorated"), NULL, NULL );
	for ( inx=0; inx<roadnameMap_da.cnt; inx++ ) {
		roadnameMapP = DYNARR_N(roadnameMap_p, roadnameMap_da, inx);
		wListAddValue( (wList_p)carDlgPLs[I_CD_ROADNAME_LIST].control, roadnameMapP->roadname, NULL, roadnameMapP );
		if ( strcasecmp( carDlgRoadnameStr, roadnameMapP->roadname )==0 )
			carDlgRoadnameInx = inx+1;
	}
	roadnameMapChanged = FALSE;
}


static BOOL_T CheckAvail(
		carPartParent_p parentP )
{
	wIndex_t inx;
	carPart_p partP;
	for ( inx=0; inx<parentP->parts_da.cnt; inx++ ) {
		partP = carPart(parentP,inx);
		if ( IsParamValid(partP->paramFileIndex) )
			return TRUE;
	}
	return FALSE;
}


static BOOL_T CarDlgLoadManufList(
		BOOL_T bLoadAll,
		BOOL_T bInclCustomUnknown,
		SCALEINX_T scale )
{
	carPartParent_p manufP, manufP1;
	wIndex_t inx, listInx=-1;
	BOOL_T found = TRUE;
	char * firstName = NULL;

LOG( log_carDlgList, 3, ( "CarDlgLoadManufList( %s, %s, %d )\n    carDlgManufStr=\"%s\"\n", bLoadAll?"TRUE":"FALSE", bInclCustomUnknown?"TRUE":"FALSE", scale, carDlgManufStr ) )
	carDlgManufInx = -1;
	manufP1 = NULL;
	wListClear( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control );
		for ( inx=0; inx<carPartParent_da.cnt; inx++ ) {
			manufP = carPartParent(inx);
			if ( manufP1!=NULL && strcasecmp( manufP1->manuf, manufP->manuf ) == 0 )
				continue;
			if ( bLoadAll==FALSE && manufP->scale != scale )
				continue;
			if ( !CheckAvail(manufP) )
				continue;
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, manufP->manuf, NULL, (void*)manufP );
			if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0' || strcasecmp( carDlgManufStr, manufP->manuf ) == 0 ) ) {
LOG( log_carDlgList, 4, ( "    found manufStr (inx=%d, listInx=%d)\n", inx, listInx ) )
				carDlgManufInx = listInx;
				if ( carDlgManufStr[0] == '\0' ) strcpy( carDlgManufStr, manufP->manuf );
			}
			if ( firstName == NULL )
				firstName = manufP->manuf;
			manufP1 = manufP;
		}
		if ( bInclCustomUnknown ) {
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, _("Custom"), NULL, (void*)NULL );
			if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0' || strcasecmp( carDlgManufStr, "Custom" ) == 0 ) ) {
LOG( log_carDlgList, 4, ( "    found Cus manufStr (inx=%d, listInx=%d)\n", inx, listInx ) )
				carDlgManufInx = listInx;
				if ( carDlgManufStr[0] == '\0' ) strcpy( carDlgManufStr, _("Custom") );
			}
			if ( firstName == NULL )
				firstName = "Custom";
			wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, _("Unknown"), NULL, (void*)NULL );
			if ( carDlgManufInx < 0 && ( carDlgManufStr[0] == '\0' || strcasecmp( carDlgManufStr, "Unknown" ) == 0 ) ) {
LOG( log_carDlgList, 4, ( "    found Unk manufStr (inx=%d, listInx=%d)\n", inx, listInx ) )
				carDlgManufInx = listInx;
				if ( carDlgManufStr[0] == '\0' ) strcpy( carDlgManufStr, _("Unknown") );
			}
		}
		if ( carDlgManufInx < 0 ) {
			found = FALSE;
			if ( firstName != NULL ) {
LOG( log_carDlgList, 4, ( "    didn't find manufStr, using [0] = %s\n", firstName ) )
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
LOG( log_carDlgList, 3, ( "CarDlgLoadProtoList( %s, %d, %s )\n    carDlgProtoStr=\"%s\", carDlgTypeInx=%d\n", manuf?manuf:"NULL", scale, loadTypeList?"TRUE":"FALSE", carDlgProtoStr, carDlgTypeInx ) )
	if ( manuf==NULL ) {
		if ( carProto_da.cnt <= 0 ) return FALSE;
		if ( listTypeInx < 0 && carDlgProtoStr[0] && (protoP=CarProtoFind(carDlgProtoStr)) )
			listTypeInx = CarProtoFindTypeCode(protoP->type);
		if ( listTypeInx < 0 )
			listTypeInx = CarProtoFindTypeCode(carProto(0)->type);
		for ( inx=0; inx<carProto_da.cnt; inx++ ) {
			protoP = carProto(inx);
			currTypeInx = CarProtoFindTypeCode(protoP->type);
			typeCount[currTypeInx]++;
			if ( carDlgTypeInx >= 0 &&
				 listTypeInx != carDlgTypeInx &&
				 currTypeInx == carDlgTypeInx ) {
LOG( log_carDlgList, 4, ( "    found typeinx, reset list (old=%d)\n", listTypeInx ) )
				wListClear( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control );
				listTypeInx = carDlgTypeInx;
				carDlgProtoInx = -1;
				firstName = NULL;
			}
			if ( currTypeInx != listTypeInx ) continue;
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, protoP->desc, NULL, (void*)protoP );
			if ( carDlgProtoInx < 0 && carDlgProtoStr[0] && strcasecmp( carDlgProtoStr, protoP->desc ) == 0 ) {
LOG( log_carDlgList, 4, ( "    found protoStr (inx=%d, listInx=%d)\n", inx, listInx ) )
				carDlgProtoInx = listInx;
				if ( carDlgProtoStr[0] == '\0' ) strcpy( carDlgProtoStr, protoP->desc );
			}
			if ( firstName == NULL )
				firstName = protoP->desc;
		}
	} else {
		for ( inx=0; inx<carPartParent_da.cnt; inx++ ) {
			parentP = carPartParent(inx);
			if ( strcasecmp( manuf, parentP->manuf ) != 0 ||
					 scale != parentP->scale )
				continue;
			if ( !CheckAvail(parentP) )
				continue;
			found = FALSE;
			for ( inx1=0; inx1<parentP->parts_da.cnt; inx1++ ) {
				partP = carPart( parentP, inx1 );
				currTypeInx = CarProtoFindTypeCode(partP->type);
				typeCount[currTypeInx]++;
				if ( listTypeInx < 0 )
					listTypeInx = currTypeInx;
				if ( carDlgTypeInx >= 0 &&
					 listTypeInx != carDlgTypeInx &&
					 currTypeInx == carDlgTypeInx ) {
LOG( log_carDlgList, 4, ( "    found typeinx, reset list (old=%d)\n", listTypeInx ) )
					wListClear( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control );
					listTypeInx = carDlgTypeInx;
					carDlgProtoInx = -1;
					firstName = NULL;
				}
				if ( listTypeInx == currTypeInx )
					found = TRUE;
			}
			if ( !found )
				continue;
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, parentP->proto, NULL, (void*)parentP );
			if ( carDlgProtoInx < 0 && ( carDlgProtoStr[0] == '\0' || strcasecmp( carDlgProtoStr, parentP->proto ) == 0 ) ) {
LOG( log_carDlgList, 4, ( "    found protoStr (inx=%d, listInx=%d)\n", inx, listInx ) )
				carDlgProtoInx = listInx;
				if ( carDlgProtoStr[0] == '\0' ) {
					strcpy( carDlgProtoStr, parentP->proto );
				}
			}
			if ( firstName == NULL )
				firstName = parentP->proto;
		}
	}

	found = TRUE;
	if ( carDlgProtoInx < 0 ) {
		found = FALSE;
		if ( firstName != NULL ) {
LOG( log_carDlgList, 4, ( "    didn't find protoStr, using [0] = %s\n", firstName ) )
			carDlgProtoInx = 0;
			strcpy( carDlgProtoStr, firstName );
		}
	}
	wListSetIndex( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );

	if ( loadTypeList ) {
LOG( log_carDlgList, 4, ( "    loading typelist\n" ) )
	wListClear( (wList_p)carDlgPLs[I_CD_PROTOKIND_LIST].control );
	for ( currTypeInx=0; currTypeInx<N_TYPELISTMAP; currTypeInx++ ) {
		if ( typeCount[currTypeInx] > 0 ) {
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOKIND_LIST].control, _(typeListMap[currTypeInx].name), NULL, (void*)(intptr_t)currTypeInx );
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
			listInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, message, NULL, (void*)partP );
			if ( carDlgPartnoInx<0 &&
				 (carDlgPartnoStr[0]?TabStringCmp( carDlgPartnoStr, &tabs[T_PART] ) == 0:TRUE) ) {
				carDlgPartnoInx = listInx;
				found = TRUE;
				selPartP = partP;
			}
			if ( selPartP == NULL )
				selPartP = partP;
			lastPart = *partP;
		}
	}
	if ( selPartP == NULL ) {
		carDlgPartnoStr[0] = '\0';
		carDlgDescStr[0] = '\0';
	} else {
		if ( carDlgPartnoInx<0 )
			carDlgPartnoInx = 0;
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

	if ( tabs ) TabStringCpy( carDlgManufStr, &tabs[T_MANUF] );
	if ( strcasecmp( carDlgManufStr, "unknown" ) == 0 ||
		 strcasecmp( carDlgManufStr, "custom" ) == 0 ) {
		loadCustomUnknown = TRUE;
		/*isItem = FALSE;*/
	}
	if ( (!CarDlgLoadManufList( !isItem, loadCustomUnknown, scale )) && tabs ) {
		TabStringCpy( carDlgManufStr, &tabs[T_MANUF] );
		carDlgManufInx = wListAddValue( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufStr, NULL, (void*)NULL );
		isItem = FALSE;
	}
	if ( isItem ) {
		parentP = (carPartParent_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
		if ( parentP ) {
			if ( tabs ) TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] );
			if ( CarDlgLoadProtoList( carDlgManufStr, scale, TRUE ) || !tabs ) {
				parentP = (carPartParent_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
				if ( parentP ) {
					if ( tabs ) TabStringCpy( carDlgPartnoStr, &tabs[T_PART] );
					if ( CarDlgLoadPartList( parentP ) || ( (!tabs) && carDlgPartnoInx>=0 ) ) {
						return TRUE;
					}
				}
			}
		}
	}
	if ( tabs ) TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] );
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
		CarProtoDlgCreateDummyOutline( &carProtoSegCnt, &carProtoSegPtr, (BOOL_T)carDlgIsLoco, protoTmp.dim.carLength, protoTmp.dim.carWidth, drawColorBlue );
		protoTmp.segCnt = carProtoSegCnt;
		protoTmp.segPtr = carProtoSegPtr;
		GetSegBounds( zero, 0.0, carProtoSegCnt, carProtoSegPtr, &protoTmp.orig, &protoTmp.size );
		TabStringCpy( carDlgProtoStr, &tabs[T_PROTO] );
		carDlgProtoInx = wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoStr, NULL, &protoTmp );/*??*/
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
	ParamControlShow( &carDlgPG, I_CD_MLTNUM,				S_ITEM && carDlgUpdateItemPtr==NULL && carDlgDispMode==0 );
	ParamControlShow( &carDlgPG, I_CD_QTY,					S_ITEM && carDlgUpdateItemPtr==NULL && carDlgDispMode==0 );

	ParamControlShow( &carDlgPG, I_CD_ROADNAME_LIST,		S_PART || ( S_ITEM && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_REPMARK,				S_PART || ( S_ITEM && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_NUMBER,				S_PART || ( S_ITEM && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_BODYCOLOR,			S_PART || ( S_ITEM && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_CARLENGTH,			!( S_ITEM && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_CARWIDTH,				!( S_ITEM && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_TRKCENTER,			!( S_ITEM && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_CANVAS,				!( S_ITEM && carDlgDispMode==0 ) );
	ParamControlShow( &carDlgPG, I_CD_CPLRLEN,				S_PART || ( S_ITEM && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_CPLDLEN,				S_PART || ( S_ITEM && carDlgDispMode==1 ) );
	ParamControlShow( &carDlgPG, I_CD_CPLRMNT,				S_PART || ( S_ITEM && carDlgDispMode==1 ) );

	ParamControlShow( &carDlgPG, I_CD_DISPMODE,				S_ITEM );

	ParamControlShow( &carDlgPG, I_CD_TYPE_LIST,			S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_FLIP,					S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_DESC_STR,				S_PART || (currState==S_ItemEnter) );
	ParamControlShow( &carDlgPG, I_CD_IMPORT,				S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_RESET,				S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PARTNO_STR,			S_PART || (currState==S_ItemEnter) );
	ParamControlShow( &carDlgPG, I_CD_PARTNO_LIST,			(currState==S_ItemSel) );
	ParamControlShow( &carDlgPG, I_CD_ISLOCO,				S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PROTOKIND_LIST,		!S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PROTOTYPE_LIST,		!S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_PROTOTYPE_STR,		S_PROTO );
	ParamControlShow( &carDlgPG, I_CD_MANUF_LIST,			!S_PROTO );

	/*ParamControlActive( &carDlgPG, I_CD_PROTOTYPE_STR,	S_PROTO && carDlgUpdateProtoPtr==NULL );*/
	ParamControlActive( &carDlgPG, I_CD_ITEMINDEX,			S_ITEM && carDlgUpdateItemPtr==NULL );
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
			sprintf( message, _("Update %s Scale Car Part"), GetScaleName( carDlgScaleInx ) );
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
	BOOL_T reload[sizeof carDlgPLs/sizeof carDlgPLs[0]];
#define RELOAD_DIMS \
		reload[I_CD_CARLENGTH] = reload[I_CD_CARWIDTH] = reload[I_CD_CPLDLEN] = \
		reload[I_CD_TRKCENTER] = reload[I_CD_CPLRLEN] = TRUE
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
			for ( inx=0; inx<sizeof carDlgPLs/sizeof carDlgPLs[0]; inx++ )
				if ( reload[inx] )
					ParamLoadControl( &carDlgPG, inx );
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
			if ( currState != S_ItemEnter )
				actions += offset;
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
			partP = (carPart_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoInx );
			if ( partP != NULL ){
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
			parentP = (carPartParent_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_MANUF_LIST].control, carDlgManufInx );
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
			parentP = (carPartParent_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			CarDlgLoadPartList( parentP );
			reload[I_CD_PARTNO_LIST] = TRUE;
			break;
		case A_LoadLists:
			if ( CarDlgLoadLists( TRUE, NULL, carDlgScaleInx ) )
				currState = S_ItemSel;
			else
				currState = S_ItemEnter;
			break;
		case A_LoadDimsFromProtoList:
			protoP = (carProto_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
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
			RELOAD_DIMS;
			break;
		case A_Redraw:
			CarDlgRedraw();
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
			wListGetValues( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoStr, sizeof carDlgProtoStr, NULL, NULL );
#ifdef LATER
			protoP = (carProto_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
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
			if ( carDlgStkPtr >= CARDLG_STK_SIZE )
				AbortProg( "carDlgNewDesc: CARDLG_STK_SIZE" );
			carDlgStk[carDlgStkPtr].dim = carDlgDim;
			carDlgStk[carDlgStkPtr].couplerLength = carDlgCouplerLength;
			carDlgStk[carDlgStkPtr].state = currState;
			carDlgStk[carDlgStkPtr].changed = carDlgChanged;
			carDlgStk[carDlgStkPtr].typeInx = carDlgTypeInx;
			if ( currState == S_ItemSel && carDlgPartnoInx >= 0 )
				carDlgStk[carDlgStkPtr].partP = (carPart_p)wListGetItemContext( (wList_p)carDlgPLs[I_CD_PARTNO_LIST].control, carDlgPartnoInx );
			else
				carDlgStk[carDlgStkPtr].partP = NULL;
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
			sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr, GetScaleName(carDlgScaleInx) );
			wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
			carDlgCouplerMount = (carDlgUpdateItemPtr->options&CAR_DESC_COUPLER_MODE_BODY)!=0;
			carDlgIsLoco = (carDlgUpdateItemPtr->options&CAR_DESC_IS_LOCO)!=0;
			carDlgPurchPrice = carDlgUpdateItemPtr->data.purchPrice;
			sprintf( carDlgPurchPriceStr, "%0.2f", carDlgPurchPrice );
			carDlgCurrPrice = carDlgUpdateItemPtr->data.currPrice;
			sprintf( carDlgCurrPriceStr, "%0.2f", carDlgCurrPrice );
			carDlgCondition = carDlgUpdateItemPtr->data.condition;
			carDlgConditionInx = MapCondition( carDlgUpdateItemPtr->data.condition );
			carDlgPurchDate = carDlgUpdateItemPtr->data.purchDate;
			if ( carDlgPurchDate )
				sprintf( carDlgPurchDateStr, "%ld", carDlgPurchDate );
			else
				carDlgPurchDateStr[0] = '\0';
			carDlgServiceDate = carDlgUpdateItemPtr->data.serviceDate;
			if ( carDlgServiceDate )
				sprintf( carDlgServiceDateStr, "%ld", carDlgServiceDate );
			else
				carDlgServiceDateStr[0] = '\0';
			wTextClear( (wText_p)carDlgPLs[I_CD_NOTES].control );
			if ( carDlgUpdateItemPtr->data.notes ) {
			strncpy( message, carDlgUpdateItemPtr->data.notes, sizeof message );
			message[sizeof message - 1] = '\0';
			for ( cp=message; *cp; cp++ )
				if ( *cp == '\n' ) *cp = ' ';
				wTextAppend( (wText_p)carDlgPLs[I_CD_NOTES].control, message );
			}
			LoadRoadnameList( &tabs[T_ROADNAME], &tabs[T_REPMARK] );
			CarDlgLoadRoadnameList();
			carDlgRoadnameInx = lookupListIndex+1;
			memset( reload, 1, sizeof reload );

			if ( CarDlgLoadLists( TRUE, tabs, carDlgScaleInx ) )
				currState = S_ItemSel;
			else
				currState = S_ItemEnter;
			break;
		case A_LoadDataFromUpdatePart:
			carDlgScaleInx = carDlgUpdatePartPtr->parent->scale;
			TabStringExtract( carDlgUpdatePartPtr->title, 7, tabs );
			tabs[T_MANUF].ptr = carDlgUpdatePartPtr->parent->manuf;
			tabs[T_MANUF].len = strlen(carDlgUpdatePartPtr->parent->manuf);
			tabs[T_PROTO].ptr = carDlgUpdatePartPtr->parent->proto;
			tabs[T_PROTO].len = strlen(carDlgUpdatePartPtr->parent->proto);
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
				carDlgIsLoco = (typeListMap[carDlgTypeInx].value&1);
			} else {
				strcpy( carDlgProtoStr , carDlgUpdateProtoPtr->desc );
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
			sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr, GetScaleName(carDlgScaleInx) );
			carDlgCouplerLength = 16.0/GetScaleRatio(carDlgScaleInx);
			wPrefGetFloat( carDlgPG.nameStr, message, &carDlgCouplerLength, carDlgCouplerLength );
			break;
		default:
			AbortProg( "carDlgDoActions: bad action" );
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
LOG( log_carDlgState, 1, ( "S_%s[T_%s]\n", carDlgState_s[currState], carDlgTransistion_s[transistion] ) )
	CarDlgDoStateActions( stateMachine[currState][transistion] );
}


static BOOL_T CheckCarDlgItemIndex( long * index )
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
	BOOL_T ok;
	DIST_T len;
	BOOL_T checkTruckCenter = FALSE;
	cmp_key_t cmp_key;
	coOrd orig, size, size2;
	carPartParent_p parentP;
	static DIST_T carDlgTruckOffset;
	static long carDlgClock;
	static long carDlgCarLengthClock;
	static long carDlgTruckCenterClock;
	static long carDlgCoupledLengthClock;
	static long carDlgCouplerLengthClock;

	ratio = (S_PROTO?1.0:GetScaleRatio(carDlgScaleInx));

LOG( log_carDlgState, 3, ( "CarDlgUpdate( %d )\n", inx ) )

	switch ( inx ) {

	case -1:
		if ( carDlgDim.truckCenter > 0 && carDlgDim.carLength > carDlgDim.truckCenter )
			carDlgTruckOffset = carDlgDim.carLength - carDlgDim.truckCenter;
		else
			carDlgTruckOffset = 0;
		carDlgCarLengthClock = carDlgCoupledLengthClock = carDlgTruckCenterClock = carDlgCouplerLengthClock = carDlgClock = 0;
		redraw = TRUE;
		break;

	case I_CD_MANUF_LIST:
		carDlgChanged++;
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgManufStr, sizeof carDlgManufStr, NULL, NULL );
		if ( carDlgManufInx < 0 ||
			 wListGetItemContext( (wList_p)pg->paramPtr[inx].control, carDlgManufInx ) == NULL )
			CarDlgStateMachine( T_ItemEnter );
#ifdef LATER
		else if ( strcasecmp( carDlgManufStr, "unknown" ) == 0 ||
				 strcasecmp( carDlgManufStr, "custom" ) == 0 )
			CarDlgStateMachine( T_ItemEnter );
#endif
		else
			CarDlgStateMachine( T_ItemSel );
		/*ParamControlShow( &carDlgPG, I_CD_MANUF_LIST, TRUE );*/
		break;

	case I_CD_PROTOKIND_LIST:
		carDlgChanged++;
		carDlgTypeInx = (int)(long)wListGetItemContext( (wList_p)pg->paramPtr[inx].control, carDlgKindInx );
		if ( S_PART || (currState==S_ItemEnter) ) {
			CarDlgLoadProtoList( NULL, 0, FALSE );
		} else {
			parentP = NULL;
			if ( carDlgProtoInx >= 0 )
				parentP = (carPartParent_p)wListGetItemContext( (wList_p)pg->paramPtr[I_CD_PROTOTYPE_LIST].control, carDlgProtoInx );
			CarDlgLoadProtoList( carDlgManufStr, (parentP?parentP->scale:0), FALSE );
		}
		CarDlgStateMachine( T_ProtoSel );
		break;

	case I_CD_PROTOTYPE_LIST:
		carDlgChanged++;
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgProtoStr, sizeof carDlgProtoStr, NULL, NULL );
		CarDlgStateMachine( T_ProtoSel );
		break;

	case I_CD_PARTNO_LIST:
		carDlgChanged++;
		wListGetValues( (wList_p)pg->paramPtr[inx].control, carDlgPartnoStr, sizeof carDlgPartnoStr, NULL, NULL );
		if ( carDlgPartnoInx >= 0 ) {
			CarDlgStateMachine( T_PartnoSel );
		} else {
			CarDlgStateMachine( T_PartnoEnter );
			wControlSetFocus( pg->paramPtr[I_CD_PARTNO_STR].control );
		}
		break;

	case I_CD_DISPMODE:
		for ( inx=B; inx<C; inx++ )
			ParamControlShow( &carDlgPG, inx, carDlgDispMode==1 );
		for ( inx=C; inx<D; inx++ )
			ParamControlShow( &carDlgPG, inx, carDlgDispMode==0 );
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
		   roadnameMapP = (roadnameMap_p)wListGetItemContext( (wList_p)pg->paramPtr[I_CD_ROADNAME_LIST].control, (wIndex_t)*(long*)valueP );
		   strcpy( carDlgRoadnameStr, roadnameMapP->roadname );
		} else {
			wListGetValues( (wList_p)pg->paramPtr[I_CD_ROADNAME_LIST].control, carDlgRoadnameStr, sizeof carDlgRoadnameStr, NULL, NULL );
			cmp_key.name = carDlgRoadnameStr;
			cmp_key.len = strlen(carDlgRoadnameStr);
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
		} else if ( carDlgCouplerLength != 0 && ( carDlgDim.coupledLength == 0 || carDlgCouplerLengthClock >= carDlgCoupledLengthClock ) ) {
			len = carDlgDim.carLength+carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.coupledLength = len;
				ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
			}
			carDlgCarLengthClock = ++carDlgClock;
		} else if ( carDlgDim.coupledLength != 0 && ( carDlgCouplerLength == 0 || carDlgCoupledLengthClock > carDlgCouplerLengthClock ) ) {
			len = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
			if ( len > 0 ) {
				carDlgCouplerLength = len;
				ParamLoadControl( &carDlgPG, I_CD_CPLRLEN );
				if ( !S_PROTO ) {
					sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr, GetScaleName(carDlgScaleInx) );
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
		} else if ( carDlgDim.carLength != 0 && ( carDlgCouplerLength == 0 || carDlgCarLengthClock > carDlgCouplerLengthClock ) ) {
			len = (carDlgDim.coupledLength-carDlgDim.carLength)/2.0;
			if ( len > 0 ) {
				carDlgCouplerLength = len;
				ParamLoadControl( &carDlgPG, I_CD_CPLRLEN );
				if ( !S_PROTO ) {
					sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr, GetScaleName(carDlgScaleInx) );
					wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
				}
			}
			carDlgCoupledLengthClock = ++carDlgClock;
		} else if ( carDlgCouplerLength != 0 && ( carDlgDim.carLength == 0 || carDlgCouplerLengthClock >= carDlgCarLengthClock ) ) {
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
		} else if ( carDlgDim.carLength != 0 && ( carDlgDim.coupledLength == 0 || carDlgCarLengthClock >= carDlgCoupledLengthClock ) ) {
			len = carDlgDim.carLength+carDlgCouplerLength*2.0;
			if ( len > 0 ) {
				carDlgDim.coupledLength = carDlgDim.carLength+carDlgCouplerLength*2.0;
				ParamLoadControl( &carDlgPG, I_CD_CPLDLEN );
			}
			carDlgCouplerLengthClock = ++carDlgClock;
		} else if ( carDlgDim.coupledLength != 0 && ( carDlgDim.carLength == 0 || carDlgCoupledLengthClock > carDlgCarLengthClock ) ) {
			len = carDlgCouplerLength-carDlgDim.coupledLength*2.0;
			if ( len > 0 ) {
				carDlgDim.carLength = carDlgCouplerLength-carDlgDim.coupledLength*2.0;
				ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
				checkTruckCenter = TRUE;
			}
			carDlgCouplerLengthClock = ++carDlgClock;
		}
		if ( !S_PROTO ) {
			 sprintf( message, "%s-%s", carDlgPLs[I_CD_CPLRLEN].nameStr, GetScaleName(carDlgScaleInx) );
			 wPrefSetFloat( carDlgPG.nameStr, message, carDlgCouplerLength );
		}
		redraw = TRUE;
		break;

	case I_CD_CARWIDTH:
		carDlgChanged++;
		if ( carDlgDim.carLength < 30/ratio ) return;
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

	case I_CD_TRKCENTER:
		carDlgChanged++;
		if ( carDlgDim.truckCenter == 0 ) {
			carDlgTruckOffset = 0;
		} else if ( carDlgDim.truckCenter < 100/ratio /*&& carDlgDim.carLength == 0.0*/ ) {
			return;
		} else if ( carDlgDim.carLength > carDlgDim.truckCenter ) {
			carDlgTruckOffset = carDlgDim.carLength - carDlgDim.truckCenter;
		} else {
			carDlgTruckOffset = 0;
		}
		redraw = TRUE;
		break;

	case I_CD_QTY:
		wControlActive( carDlgPLs[I_CD_MLTNUM].control, carDlgQuantity>1 );
		break;

	case I_CD_PURPRC:
	case I_CD_CURPRC:
		carDlgChanged++;
		*(FLOAT_T*)(pg->paramPtr[inx].context) = strtod( (char*)pg->paramPtr[inx].valueP, &cp );
		if ( cp==NULL || *cp!='\0' )
			*(FLOAT_T*)(pg->paramPtr[inx].context) = -1;
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
		cp = (char*)pg->paramPtr[inx].valueP;
		if ( *cp ) {
			valL = strtol( cp, &cq, 10 );
			if ( cq==NULL || *cq!='\0' ) {
				cp = N_("Enter a 8 digit numeric date");
			} else if ( valL != 0 ) {
				if ( strlen(cp) != 8 ) {
					cp = N_("Enter a 8 digit date");
				} else if ( valL < 19000101 || valL > 21991231 ) {
					cp = N_("Enter a date between 19000101 and 21991231");
				} else {
					d = valL % 100;
					m = (valL / 100) % 100;
					if ( m < 1 || m > 12 ) {
						cp = N_("Invalid month");
					} else if ( d < 1 || d > 31 ) {
						cp = N_("Invalid day");
					} else {
						cp = NULL;
					}
				}
			}
			if ( cp ) {
				valL = 0;
			}
		} else {
			cp = NULL;
			valL = 0;
		}
		wControlSetBalloon( pg->paramPtr[inx].control, 0, -5, _(cp) );
		*(long*)(pg->paramPtr[inx].context) = valL;
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
		carProtoSegPtr = (trkSeg_t*)tempSegs_da.ptr;
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
		RescaleSegs( carProtoSegCnt, carProtoSegPtr, size2.x/size.x, size2.y/size.y, curScaleRatio );
		carDlgDim.carLength = size2.x;
		carDlgDim.carWidth = size2.y;
		carDlgDim.coupledLength = carDlgDim.carLength + 32;
		if ( carDlgDim.carLength > 120 ) {
			carDlgDim.truckCenter = carDlgDim.carLength - 120;
			carDlgTruckOffset = carDlgDim.carLength - carDlgDim.truckCenter;
		} else {
			carDlgDim.truckCenter = 0;
			carDlgTruckOffset = 0;
		}
		carDlgFlipToggle = FALSE;
		ParamLoadControl( &carDlgPG, I_CD_CARLENGTH );
		ParamLoadControl( &carDlgPG, I_CD_CARWIDTH );
		ParamLoadControl( &carDlgPG, I_CD_CPLRLEN );
		ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
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
		if ( carDlgTruckOffset > 0 ) {
			carDlgDim.truckCenter = carDlgDim.carLength - carDlgTruckOffset;
		} else {
			carDlgDim.truckCenter = carDlgDim.carLength * 0.75;
		}
		ParamLoadControl( &carDlgPG, I_CD_TRKCENTER );
	}

	ok = FALSE;
	if ( S_PROTO && carDlgProtoStr[0] == '\0' )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter a Prototype name") );
	else if ( S_PART && carDlgManufStr[0] == '\0' )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Select or Enter a Manufacturer") );
	else if ( S_PART && carDlgPartnoStr[0] == '\0' )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter a Part Number") );
	else if ( carDlgDim.carLength <= 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter the Car Length") );
	else if ( carDlgDim.carWidth <= 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter the Car Width") );
	else if ( carDlgDim.truckCenter <= 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter the Truck Centers") );
	else if ( carDlgDim.truckCenter >= carDlgDim.carLength )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Truck Centers must be less than Car Length") );
	else if ( (!S_PROTO) && ( carDlgDim.coupledLength <= 0 || carDlgCouplerLength <= 0 ) )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter the Coupled Length or Coupler Length") );
	else if ( S_PROTO && carDlgDim.coupledLength <= 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter the Coupled Length") );
	else if ( S_ITEM && carDlgItemIndex <= 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Enter a item Index") );
	else if ( S_ITEM && carDlgPurchPrice < 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Purchase Price is not valid") );
	else if ( S_ITEM && carDlgCurrPrice < 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Current Price is not valid") );
	else if ( S_ITEM && carDlgPurchDate < 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Purchase Date is not valid") );
	else if ( S_ITEM && carDlgServiceDate < 0 )
		ParamLoadMessage( &carDlgPG, I_CD_MSG, _("Service Date is not valid") );
	else if ( S_ITEM && carDlgUpdateItemPtr==NULL &&
			( valL = carDlgItemIndex , !CheckCarDlgItemIndex(&carDlgItemIndex) ) ) {
		sprintf( message, _("Item Index %ld duplicated an existing item: updated to new value"), valL );
		ParamLoadControl( &carDlgPG, I_CD_ITEMINDEX );
		ParamLoadMessage( &carDlgPG, I_CD_MSG, message );
		ok = TRUE;
	} else {
		ParamLoadMessage( pg, I_CD_MSG, "" );
		ok = TRUE;
	}

	if ( redraw )
		CarDlgRedraw();

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
	if ( S_ITEM )
		CarDlgDoStateActions( item2protoActions );
	else
		CarDlgDoStateActions( part2protoActions );
	carDlgChanged = 0;
}


static void CarDlgClose( wWin_p win )
{
	carDlgState_e oldState;

	if ( carDlgChanged ) {
		if ( !inPlayback ) {
			if ( NoticeMessage( MSG_CARDESC_CHANGED, _("Yes"), _("No") ) <= 0 )
				return;
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
			if ( S_PART )
				CarDlgDoStateActions( proto2partActions );
			else
				CarDlgDoStateActions( proto2itemActions );
		else
				CarDlgDoStateActions( part2itemActions );
	} else {
		wTextClear( (wText_p)carDlgPLs[I_CD_NOTES].control );
		wHide( carDlgPG.win );
	}
}


static void CarDlgOk( void * junk )
{
	long options = 0;
	int len;
	FILE * f;
	long number;
	char * cp;
	long count;
	tabString_t tabs[7];
	char title[STR_LONG_SIZE];
	carItem_p itemP=NULL;
	carPart_p partP=NULL;
	carProto_p protoP;
	BOOL_T reloadRoadnameList = FALSE;
	char *oldLocale = NULL;

LOG( log_carDlgState, 3, ( "CarDlgOk()\n" ) )

	/*ParamUpdate( &carDlgPG );*/
	if ( carDlgDim.carLength <= 0.0 ||
		 carDlgDim.carWidth <= 0.0 ||
		 carDlgDim.truckCenter <= 0.0 ||
		 carDlgDim.coupledLength <= 0.0 ) {
		NoticeMessage( MSG_CARDESC_VALUE_ZERO, _("Ok"), NULL );
		return;
	}
	if ( carDlgDim.carLength <= carDlgDim.carWidth ) {
		NoticeMessage( MSG_CARDESC_BAD_DIM_VALUE, _("Ok"), NULL );
		return;
	}
	if ( carDlgDim.coupledLength <= carDlgDim.carLength ) {
		NoticeMessage( MSG_CARDESC_BAD_COUPLER_LENGTH_VALUE, _("Ok"), NULL );
		return;
	}

	if ( S_ITEM && carDlgUpdateItemPtr==NULL && !CheckCarDlgItemIndex(&carDlgItemIndex) ) {
		NoticeMessage( MSG_CARITEM_BAD_INDEX, _("Ok"), NULL );
		ParamLoadControl( &carDlgPG, I_CD_ITEMINDEX );
		return;
	}

	if ( (!S_PROTO) && carDlgCouplerMount != 0 )
		options |= CAR_DESC_COUPLER_MODE_BODY;
	if ( carDlgIsLoco == 1 )
		options |= CAR_DESC_IS_LOCO;

	if ( S_ITEM ) {
		len = wTextGetSize( (wText_p)carDlgPLs[I_CD_NOTES].control );
		sprintf( title, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr, carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr, carDlgNumberStr );
		partP = NULL;
		if ( ( carDlgManufInx < 0 || carDlgPartnoInx < 0 ) && carDlgPartnoStr[0] ) {
			partP = CarPartFind( carDlgManufStr, strlen(carDlgManufStr), carDlgPartnoStr, strlen(carDlgPartnoStr), carDlgScaleInx );
			if ( partP != NULL &&
				 NoticeMessage( MSG_CARPART_DUPNAME, _("Yes"), _("No") ) <= 0 )
				return;
			partP = CarPartNew( NULL, PARAM_CUSTOM, carDlgScaleInx, title, options, typeListMap[carDlgTypeInx].value, &carDlgDim, carDlgBodyColor );
			if ( partP != NULL ) {
				if ( ( f = OpenCustom("a") ) ) {
					oldLocale = SaveLocale("C");
					CarPartWrite( f, partP );
					fclose(f);
					RestoreLocale(oldLocale);
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
						sprintf( title, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr, carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr, carDlgNumberStr );
					}
				}
			}
			if ( len > 0 ) {
				if ( itemP->data.notes )
					itemP->data.notes = MyRealloc( itemP->data.notes, len+2 );
				else
					itemP->data.notes = MyMalloc( len+2 );
				itemP->data.notes = (char*)MyMalloc( len+2 );
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
		if ( carDlgUpdateItemPtr==NULL )
			CarInvListAdd( itemP );
		else
			CarInvListUpdate( itemP );
		changed++;
		SetWindowTitle();
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
				(carDlgRepmarkStr?carDlgRepmarkStr:carDlgRoadnameStr), carDlgNumberStr );
		carDlgQuantity = 1;
		ParamLoadControl( &carDlgPG, I_CD_QTY );

	} else if ( S_PART ) {
		if ( strcasecmp( carDlgRoadnameStr, "undecorated" ) == 0 ) {
			carDlgRoadnameStr[0] = '\0';
			carDlgRepmarkStr[0] = '\0';
		}
		if ( carDlgUpdatePartPtr==NULL ) {
			partP = CarPartFind( carDlgManufStr, strlen(carDlgManufStr), carDlgPartnoStr, strlen(carDlgPartnoStr), carDlgScaleInx );
			if ( partP != NULL &&
				 NoticeMessage( MSG_CARPART_DUPNAME, _("Yes"), _("No") ) <= 0 )
				return;
		}
		sprintf( message, "%s\t%s\t%s\t%s\t%s\t%s\t%s", carDlgManufStr, carDlgProtoStr, carDlgDescStr, carDlgPartnoStr, carDlgRoadnameStr, carDlgRepmarkStr, carDlgNumberStr );
		carDlgNewPartPtr = CarPartNew( carDlgUpdatePartPtr, PARAM_CUSTOM, carDlgScaleInx, message, options, typeListMap[carDlgTypeInx].value,
					&carDlgDim, carDlgBodyColor );
		if ( carDlgNewPartPtr != NULL && ( f = OpenCustom("a") ) ) {
			oldLocale = SaveLocale("C");
				CarPartWrite( f, carDlgNewPartPtr );
				fclose(f);
				RestoreLocale(oldLocale);
		}
		reloadRoadnameList = TRUE;
		sprintf( message, _("%s Part: %s %s %s %s %s %s"), carDlgUpdatePartPtr==NULL?_("Added new"):_("Updated"), carDlgManufStr, carDlgPartnoStr, carDlgProtoStr, carDlgDescStr, carDlgRepmarkStr?carDlgRepmarkStr:carDlgRoadnameStr, carDlgNumberStr );

	} else if ( S_PROTO ) {
		if ( carDlgUpdateProtoPtr==NULL ) {
			protoP = CarProtoFind( carDlgProtoStr );
			if ( protoP != NULL &&
				 NoticeMessage( MSG_CARPROTO_DUPNAME, _("Yes"), _("No") ) <= 0 )
				return;
		}
		carDlgNewProtoPtr = CarProtoNew( carDlgUpdateProtoPtr, PARAM_CUSTOM, carDlgProtoStr, options, typeListMap[carDlgTypeInx].value, &carDlgDim, carDlgSegs_da.cnt, &carDlgSegs(0) );
		if ( (f = OpenCustom("a") ) ) {
			oldLocale = SaveLocale("C");
			CarProtoWrite( f, carDlgNewProtoPtr );
			fclose(f);
			RestoreLocale(oldLocale);
		}
		sprintf( message, _("%s Prototype: %s%s."),
				carDlgUpdateProtoPtr==NULL?_("Added new"):_("Updated"), carDlgProtoStr,
				carDlgUpdateProtoPtr==NULL?_(". Enter new values or press Close"):"" );
	}

	if ( reloadRoadnameList ) {
		tabs[0].ptr = carDlgRoadnameStr;
		tabs[0].len = strlen(carDlgRoadnameStr);
		tabs[1].ptr = carDlgRepmarkStr;
		tabs[1].len = strlen(carDlgRepmarkStr);
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
				if ( CarDlgLoadLists( TRUE, tabs, curScaleInx ) )
					currState = S_ItemSel;
				else
					currState = S_ItemEnter;
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
			if ( cp && *cp == 0 && number > 0 )
				sprintf( carDlgPartnoStr, "%ld", number+1 );
			else
				carDlgPartnoStr[0] = '\0';
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
		wPos_t currX,
		wPos_t *xx,
		wPos_t *yy )
{
	static wPos_t col2pos = 0;
	wPos_t y0, y1;

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
	case I_CD_CPLRMNT:
	case I_CD_CPLRLEN:
	case I_CD_CARWIDTH:
		if ( col2pos == 0 )
			col2pos = wLabelWidth( _("Coupler Length") )+20;
		*xx = wControlBeside(carDlgPLs[inx-1].control) + col2pos;
		break;
	case I_CD_DESC_STR:
		*yy = wControlBelow(carDlgPLs[I_CD_PARTNO_STR].control) + 3;
		break;
	case I_CD_CPLDLEN:
		*yy = wControlBelow(carDlgPLs[I_CD_TRKCENTER].control) + 3;
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


static void DoCarPartDlg( carDlgAction_e *actions )
{
	paramData_t * pd;
	int inx;

	if ( carDlgPG.win == NULL ) {
		ParamCreateDialog( &carDlgPG, MakeWindowTitle(_("New Car Part")), _("Add"), CarDlgOk, CarDlgClose, TRUE, CarDlgLayout, F_BLOCK|PD_F_ALT_CANCELLABEL, CarDlgUpdate );

		if ( carDlgDim.carWidth==0 )
			carDlgDim.carWidth = 12.0*10.0/curScaleRatio;

		for ( pd=carDlgPG.paramPtr; pd<&carDlgPG.paramPtr[carDlgPG.paramCnt]; pd++ ) {
			 if ( pd->type == PD_FLOAT && pd->valueP ) {
				sprintf( message, "%s-%s", pd->nameStr, curScaleName );
				wPrefGetFloat( carDlgPG.nameStr, message, (FLOAT_T*)pd->valueP, *(FLOAT_T*)pd->valueP );
			}
		}
		roadnameMapChanged = TRUE;

		for ( inx=0; inx<N_CONDLISTMAP; inx++ )
			wListAddValue( (wList_p)carDlgPLs[I_CD_COND].control, _(condListMap[inx].name), NULL, (void*)condListMap[inx].value );

		for ( inx=0; inx<N_TYPELISTMAP; inx++ )
			wListAddValue( (wList_p)carDlgPLs[I_CD_TYPE_LIST].control, _(typeListMap[inx].name), NULL, (void*)typeListMap[inx].value );

		for ( inx=0; inx<N_TYPELISTMAP; inx++ )
			wListAddValue( (wList_p)carDlgPLs[I_CD_PROTOKIND_LIST].control, _(typeListMap[inx].name), NULL, (void*)typeListMap[inx].value );

		wTextSetReadonly( (wText_p)carDlgPLs[I_CD_NOTES].control, FALSE );
	}

	wPrefGetInteger( "misc", "last-car-item-index", &carDlgItemIndex, 1 );
	CheckCarDlgItemIndex(&carDlgItemIndex);
	CarDlgLoadRoadnameList();
	carProtoSegCnt = 0;
	carProtoSegPtr = NULL;
	carDlgScaleInx = curScaleInx;
	carDlgFlipToggle = FALSE;
	carDlgChanged = 0;

	CarDlgDoStateActions( actions );

	/*CarDlgShowControls();*/

#ifdef LATER
if ( logTable(log_carList).level >= 1 ) {
	int inx;
	carPart_p partP;
	for ( inx=0; inx<carPart_da.cnt; inx++ ) {
		partP = carPart(inx);
		LogPrintf( "%d %s %d\n", inx, partP->title, partP->paramFileIndex );
	}
}
#endif
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

/*
 * Car Inventory List
 */

static wIndex_t carInvInx;

static wIndex_t carInvSort[] = { 0, 1, 2, 3 };
#define N_SORT			(sizeof carInvSort/sizeof carInvSort[0])

static void CarInvDlgAdd( void );
static void CarInvDlgEdit( void );
static void CarInvDlgDelete( void );
static void CarInvDlgImportCsv( void );
static void CarInvDlgExportCsv( void );
static void CarInvDlgSaveText( void );
static void CarInvListLoad( void );

static wPos_t carInvColumnWidths[] = {
		-40, 30, 100, -50, 50, 130, 120, 100,
		-50, -50, 60, 55, 55, 40, 200 };
static const char * carInvColumnTitles[] = {
	N_("Index"), N_("Scale"), N_("Manufacturer"), N_("Part No"), N_("Type"),
	N_("Description"), N_("Roadname"), N_("Rep Marks"), N_("Purc Price"),
	N_("Curr Price"), N_("Condition"), N_("Purc Date"), N_("Srvc Date"),
	N_("Locat'n"), N_("Notes") };
static char * sortOrders[] = {
	N_("Index"), N_("Scale"), N_("Manufacturer"), N_("Part No"), N_("Type"),
	N_("Description"), N_("Roadname"), N_("RepMarks"), N_("Purch Price"),
	N_("Curr Price"), N_("Condition"), N_("Purch Date"), N_("Service Date") };
#define S_INDEX			(0)
#define S_SCALE			(1)
#define S_MANUF			(2)
#define S_PARTNO		(3)
#define S_TYPE			(4)
#define S_DESC			(5)
#define S_ROADNAME		(6)
#define S_REPMARKS		(7)
#define S_PURCHPRICE	(8)
#define S_CURRPRICE		(9)
#define S_CONDITION		(10)
#define S_PURCHDATE		(11)
#define S_SRVDATE		(12)
static paramListData_t carInvListData = { 30, 600, sizeof carInvColumnTitles/sizeof carInvColumnTitles[0], carInvColumnWidths, carInvColumnTitles };
static paramData_t carInvPLs[] = {
#define I_CI_SORT		(0)
	{ PD_DROPLIST, &carInvSort[0], "sort1", PDO_LISTINDEX|0, (void*)110, N_("Sort By") },
	{ PD_DROPLIST, &carInvSort[1], "sort2", PDO_LISTINDEX|PDO_DLGHORZ, (void*)110, "" },
	{ PD_DROPLIST, &carInvSort[2], "sort3", PDO_LISTINDEX|PDO_DLGHORZ, (void*)110, "" },
	{ PD_DROPLIST, &carInvSort[3], "sort4", PDO_LISTINDEX|PDO_DLGHORZ, (void*)110, "" },
#define S				(4)
#define I_CI_LIST		(S+0)
	{ PD_LIST, &carInvInx, "list", PDO_LISTINDEX|PDO_DLGRESIZE|PDO_DLGNOLABELALIGN|PDO_DLGRESETMARGIN, &carInvListData, NULL, BO_READONLY|BL_MANY },
#define I_CI_EDIT		(S+1)
	{ PD_BUTTON, (void*)CarInvDlgEdit, "edit", PDO_DLGCMDBUTTON, NULL, N_("Edit") },
#define I_CI_ADD		(S+2)
	{ PD_BUTTON, (void*)CarInvDlgAdd, "add", 0, NULL, N_("Add"), 0, 0 },
#define I_CI_DELETE		(S+3)
	{ PD_BUTTON, (void*)CarInvDlgDelete, "delete", PDO_DLGWIDE, NULL, N_("Delete") },
#define I_CI_IMPORT_CSV	(S+4)
	{ PD_BUTTON, (void*)CarInvDlgImportCsv, "import", PDO_DLGWIDE, NULL, N_("Import") },
#define I_CI_EXPORT_CSV	(S+5)
	{ PD_BUTTON, (void*)CarInvDlgExportCsv, "export", 0, NULL, N_("Export") },
#define I_CI_PRINT		(S+6)
	{ PD_BUTTON, (void*)CarInvDlgSaveText, "savetext", 0, NULL, N_("List") } };
static paramGroup_t carInvPG = { "carinv", 0, carInvPLs, sizeof carInvPLs/sizeof carInvPLs[0] };

static carItem_p CarInvDlgFindCurrentItem( void )
{
	wIndex_t selcnt = wListGetSelectedCount( (wList_p)carInvPLs[I_CI_LIST].control );
	wIndex_t inx, cnt;

	if ( selcnt != 1 ) return NULL;
	cnt = wListGetCount( (wList_p)carInvPLs[I_CI_LIST].control );
	for ( inx=0; inx<cnt; inx++ )
		if ( wListGetItemSelected( (wList_p)carInvPLs[I_CI_LIST].control, inx ) )
			break;
	if ( inx>=cnt ) return NULL;
	return (carItem_p)wListGetItemContext( (wList_p)carInvPLs[I_CI_LIST].control, inx );
}


static void CarInvDlgFind( void * junk )
{
	carItem_p item = CarInvDlgFindCurrentItem();
	coOrd pos;
	ANGLE_T angle;
	if ( item == NULL || item->car == NULL || IsTrackDeleted(item->car) ) return;
	CarGetPos( item->car, &pos, &angle );
	CarSetVisible( item->car );
	DrawMapBoundingBox( FALSE );
	mainCenter = pos;
	mainD.orig.x = pos.x-mainD.size.x/2;;
	mainD.orig.y = pos.y-mainD.size.y/2;;
	MainRedraw();
	DrawMapBoundingBox( TRUE );
}


static void CarInvDlgAdd( void )
{
	if ( carProto_da.cnt <= 0 ) {
		NoticeMessage( MSG_NO_CARPROTO, _("Ok"), NULL );
		return;
	}
	carDlgUpdateItemPtr = NULL;
	DoCarPartDlg( itemNewActions );
}


static void CarInvDlgEdit( void )
{
	carDlgUpdateItemPtr = CarInvDlgFindCurrentItem();
	if ( carDlgUpdateItemPtr == NULL )
		return;
	DoCarPartDlg( itemUpdActions );
}


static void CarInvDlgDelete( void )
{
	carItem_p item;
	wIndex_t inx, inx1, cnt, selcnt;

	selcnt = wListGetSelectedCount( (wList_p)carInvPLs[I_CI_LIST].control );
	if ( selcnt == 0 )
		return;
	if ( NoticeMessage( MSG_CARINV_DELETE_CONFIRM, _("Yes"), _("No"), selcnt ) <= 0 )
		return;
	cnt = wListGetCount( (wList_p)carInvPLs[I_CI_LIST].control );
	for ( inx=0; inx<cnt; inx++ ) {
		if ( !wListGetItemSelected( (wList_p)carInvPLs[I_CI_LIST].control, inx ) )
			continue;
		item = (carItem_p)wListGetItemContext( (wList_p)carInvPLs[I_CI_LIST].control, inx );
		if ( item == NULL )
			continue;
		if ( item->car && !IsTrackDeleted(item->car) )
			continue;
		wListDelete( (wList_p)carInvPLs[I_CI_LIST].control, inx );
		if ( item->title ) MyFree( item->title );
		if ( item->data.number ) MyFree( item->data.number );
		MyFree( item );
		for ( inx1=inx; inx1<carItemInfo_da.cnt-1; inx1++ )
			carItemInfo(inx1) = carItemInfo(inx1+1);
		carItemInfo_da.cnt -= 1;
		inx--;
		cnt--;
	}
	changed++;
	SetWindowTitle();
	carInvInx = -1;
	ParamLoadControl( &carInvPG, I_CI_LIST );
	ParamControlActive( &carInvPG, I_CI_EDIT, FALSE );
	ParamControlActive( &carInvPG, I_CI_DELETE, FALSE );
	ParamControlActive( &carInvPG, I_CI_EXPORT_CSV, carItemInfo_da.cnt > 0 );
	ParamDialogOkActive( &carInvPG, FALSE );
}


static int CarInvSaveText(
		const char * pathName,
		const char * fileName,
		void * data )
{
	FILE * f;
	carItem_p item;
	int inx;
	int widths[9], width;
	tabString_t tabs[7];
	char * cp0, * cp1;
	int len;

	if ( pathName == NULL )
		return TRUE;
	SetCurDir( pathName, fileName );
	f = fopen( pathName, "w" );
	if ( f == NULL ) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Car Inventory"), fileName, strerror(errno) );
		return FALSE;
	}

	memset( widths, 0, sizeof widths );
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ ) {
		item = carItemInfo(inx);
		TabStringExtract( item->title, 7, tabs );
		sprintf( message, "%ld", item->index );
		width = strlen( message );
		if ( width > widths[0] ) widths[0] = width;
		width = strlen(GetScaleName(item->scaleInx)) + 1 + tabs[T_MANUF].len + 1 + tabs[T_PART].len;
		if ( width > widths[1] ) widths[1] = width;
		if ( tabs[T_PROTO].len > widths[2] ) widths[2] = tabs[T_PROTO].len;
		width = tabs[T_REPMARK].len + tabs[T_NUMBER].len;
		if ( tabs[T_REPMARK].len > 0 && tabs[T_NUMBER].len > 0 )
			width += 1;
		if ( width > widths[3] ) widths[3] = width;
		if ( item->data.purchDate > 0 ) widths[4] = 8;
		if ( item->data.purchPrice > 0 ) {
			sprintf( message, "%0.2f", item->data.purchPrice );
			width = strlen(message);
			if ( width > widths[5] ) widths[5] = width;
		}
		if ( item->data.condition != 0 )
			widths[6] = 5;
		if ( item->data.currPrice > 0 ) {
			sprintf( message, "%0.2f", item->data.currPrice );
			width = strlen(message);
			if ( width > widths[7] ) widths[7] = width;
		}
		if ( item->data.serviceDate > 0 ) widths[8] = 8;
	}
	fprintf( f, "%-*.*s %-*.*s %-*.*s %-*.*s", widths[0], widths[0], "#", widths[1], widths[1], "Part", widths[2], widths[2], "Description", widths[3], widths[3], "Rep Mark" );
	if ( widths[4] ) fprintf( f, " %-*.*s", widths[4], widths[4], "PurDate" );
	if ( widths[5] ) fprintf( f, " %-*.*s", widths[5], widths[5], "PurPrice" );
	if ( widths[6] ) fprintf( f, " %-*.*s", widths[6], widths[6], "Cond" );
	if ( widths[7] ) fprintf( f, " %-*.*s", widths[7], widths[7], "CurPrice" );
	if ( widths[8] ) fprintf( f, " %-*.*s", widths[8], widths[8], "SrvDate" );
	fprintf( f, "\n" );

	for ( inx=0; inx<carItemInfo_da.cnt; inx++ ) {
		item = carItemInfo(inx);
		TabStringExtract( item->title, 7, tabs );
		sprintf( message, "%ld", item->index );
		fprintf( f, "%.*s", widths[0], message );
		width = tabs[T_MANUF].len + 1 + tabs[T_PART].len;
		sprintf( message, "%s %.*s %.*s", GetScaleName(item->scaleInx), tabs[T_MANUF].len, tabs[T_MANUF].ptr, tabs[T_PART].len, tabs[T_PART].ptr );
		fprintf( f, " %-*s", widths[1], message );
		fprintf( f, " %-*.*s", widths[2], tabs[T_PROTO].len, tabs[T_PROTO].ptr );
		width = tabs[T_REPMARK].len + tabs[T_NUMBER].len;
		sprintf( message, "%.*s%s%.*s", tabs[T_REPMARK].len, tabs[T_REPMARK].ptr, (tabs[T_REPMARK].len > 0 && tabs[T_NUMBER].len > 0)?" ":"", tabs[T_NUMBER].len, tabs[T_NUMBER].ptr );
		fprintf( f, " %-*s", widths[3], message );
		if ( widths[4] > 0 ) {
			if ( item->data.purchDate > 0 ) {
				sprintf( message, "%ld", item->data.purchDate );
				fprintf( f, " %*.*s", widths[4], widths[4], message );
			} else {
				fprintf( f, " %*s", widths[4], " " );
			}
		}
		if ( widths[5] > 0 ) {
			if ( item->data.purchPrice > 0 ) {
				sprintf( message, "%0.2f", item->data.purchPrice );
				fprintf( f, " %*.*s", widths[5], widths[5], message );
			} else {
				fprintf( f, " %*s", widths[5], " " );
			}
		}
		if ( widths[6] > 0 ) {
			if ( item->data.condition != 0 ) {
				fprintf( f, " %-*.*s", widths[6], widths[6], condListMap[MapCondition(item->data.condition)].name );
			} else {
				fprintf( f, " %*s", widths[6], " " );
			}
		}
		if ( widths[7] > 0 ) {
			if ( item->data.purchPrice > 0 ) {
				sprintf( message, "%0.2f", item->data.purchPrice );
				fprintf( f, " %*.*s", widths[7], widths[7], message );
			} else {
				fprintf( f, " %*s", widths[7], " " );
			}
		}
		if ( widths[8] > 0 ) {
			if ( item->data.serviceDate > 0 ) {
				sprintf( message, "%ld", item->data.serviceDate );
				fprintf( f, " %*.*s", widths[8], widths[8], message );
			} else {
				fprintf( f, " %*s", widths[8], " " );
			}
		}
		fprintf( f, "\n" );
		if ( item->data.notes ) {
			cp0 = item->data.notes;
			while ( 1 ) {
				cp1 = strchr( cp0, '\n' );
				if ( cp1 ) {
					len = cp1-cp0;
				} else {
					len = strlen( cp0 );
					if ( len == 0 )
						break;
				}
				fprintf( f, "%*.*s %*.*s\n", widths[0], widths[0], " ", len, len, cp0 );
				if ( cp1 == NULL )
					break;
				cp0 = cp1+1;
			}
		}
	}
	fclose( f );
	return TRUE;
}


static struct wFilSel_t * carInvSaveText_fs;
static void CarInvDlgSaveText( void )
{
	if ( carInvSaveText_fs == NULL )
		carInvSaveText_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("List Cars"),
				"Text|*.txt", CarInvSaveText, NULL );
	wFilSelect( carInvSaveText_fs, curDirName );
}


static char *carCsvColumnTitles[] = {
		"Index", "Scale", "Manufacturer", "Type", "Partno", "Prototype",
		"Description", "Roadname", "Repmark", "Number", "Options", "CarLength",
		"CarWidth", "CoupledLength", "TruckCenter", "Color", "PurchPrice",
		"CurrPrice", "Condition", "PurchDate", "ServiceDate", "Notes" };
#define M_INDEX			(0)
#define M_SCALE			(1)
#define M_MANUF			(2)
#define M_TYPE			(3)
#define M_PARTNO		(4)
#define M_PROTO			(5)
#define M_DESC			(6)
#define M_ROADNAME		(7)
#define M_REPMARK		(8)
#define M_NUMBER		(9)
#define M_OPTIONS		(10)
#define M_CARLENGTH		(11)
#define M_CARWIDTH		(12)
#define M_CPLDLENGTH	(13)
#define M_TRKCENTER		(14)
#define M_COLOR			(15)
#define M_PURCHPRICE	(16)
#define M_CURRPRICE		(17)
#define M_CONDITION		(18)
#define M_PURCHDATE		(19)
#define M_SRVDATE		(20)
#define M_NOTES			(21)


static int ParseCsvLine(
		char * line,
		int max_elem,
		tabString_t * tabs,
		int * map )
{
	int elem = 0;
	char * cp, * cq, * ptr;
	int rc, len;

	cp = line;
	for ( cq=cp+strlen(cp)-1; cq>cp&&isspace((unsigned char)*cq); cq-- );
	cq[1] = '\0';
	for ( elem=0; elem<max_elem; elem++ ) {
		tabs[elem].ptr = "";
		tabs[elem].len = 0;
	}
	elem = 0;
	while ( *cp && elem < max_elem ) {
		while ( *cp == ' ' ) cp++;
		if ( *cp == ',' ) {
			ptr = "";
			len = 0;
		} else if ( *cp == '"' ) {
			cp++;
			ptr = cq = cp;
			while (1) {
				while ( *cp!='"' ) {
					if ( *cp == '\0' ) {
						rc = NoticeMessage( MSG_CARIMP_EOL, _("Continue"), _("Stop"), ptr );
						return (rc<1)?-1:elem;
					}
					*cq++ = *cp++;
				}
				cp++;
				if ( *cp!='"' ) break;
				*cq++ = *cp++;
			}
			if ( *cp && *cp != ',' ) {
				rc = NoticeMessage( MSG_CARIMP_MISSING_COMMA, _("Continue"), _("Stop"), ptr );
				return (rc<1)?-1:elem;
			}
			len = cq-ptr;
		} else {
			ptr = cp;
			while ( *cp && *cp != ',' ) { cp++; }
			len = cp-ptr;
		}
		if ( map[elem] >= 0 ) {
		   tabs[map[elem]].ptr = ptr;
		   tabs[map[elem]].len = len;
		}
		if ( *cp ) cp++;
		elem++;
	}
	return elem;
}


static int CarInvImportCsv(
		const char * pathName,
		const char * fileName,
		void * data )
{
	FILE * f;
	carItem_p item;
	tabString_t tabs[40], partTabs[7];
	int map[40];
	int i, j, cnt, numCol, len, rc;
	char * cp, * cq;
	long type = 0;
	char title[STR_LONG_SIZE];
	long index, options, color, condition, purchDate, srvcDate;
	carDim_t dim;
	FLOAT_T purchPrice, currPrice;
	int duplicateIndexError = 0;
	SCALEINX_T scale;
	carPart_p partP;
	int requiredCols;
	char *oldLocale = NULL;

	if ( pathName == NULL )
		return TRUE;
	SetCurDir( pathName, fileName );
	f = fopen( pathName, "r" );
	if ( f == NULL ) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Import Cars"), fileName, strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale("C");

	if ( fgets( message, sizeof message, f ) == NULL ) {
		NoticeMessage( MSG_CARIMP_NO_DATA, _("Continue"), NULL );
		fclose( f );
		RestoreLocale(oldLocale);
		return FALSE;
	}
	for ( j=0; j<40; j++ ) map[j] = j;
	numCol = ParseCsvLine( message, 40, tabs, map );
	if ( numCol <= 0 ) {
		fclose( f );
		RestoreLocale(oldLocale);
		return FALSE;
	}
	for ( j=0; j<40; j++ ) map[j] = -1;
	requiredCols = 0;
	for ( i=0; i<numCol; i++ ) {
		for ( j=0; j<sizeof carCsvColumnTitles/sizeof carCsvColumnTitles[0]; j++ ) {
			if ( TabStringCmp( carCsvColumnTitles[j], &tabs[i] ) == 0 ) {
				if ( map[i] >= 0 ) {
					NoticeMessage( MSG_CARIMP_DUP_COLUMNS, _("Continue"), NULL, carCsvColumnTitles[j] );
					fclose( f );
					RestoreLocale(oldLocale);
					return FALSE;
				}
				map[i] = j;
				/*j = sizeof carCsvColumnTitles/sizeof carCsvColumnTitles[0];*/
				if ( j == M_SCALE || j == M_PROTO || j == M_MANUF || j == M_PARTNO )
					requiredCols++;
			}
		}
		if ( map[i] == -1 ) {
			tabs[i].ptr[tabs[i].len] = '\0';
			NoticeMessage( MSG_CARIMP_IGNORED_COLUMN, _("Continue"), NULL, tabs[i].ptr );
			tabs[i].ptr[tabs[i].len] = ',';
		}
	}
	if ( requiredCols != 4 ) {
		NoticeMessage( MSG_CARIMP_MISSING_COLUMNS, _("Continue"), NULL );
		fclose( f );
		RestoreLocale(oldLocale);
		return FALSE;
	}
	while ( fgets( message, sizeof message, f ) != NULL ) {
		cnt = ParseCsvLine( message, 40, tabs, map );
		if ( cnt > numCol ) cnt = numCol;
		tabs[M_SCALE].ptr[tabs[M_SCALE].len] = '\0';
		scale = LookupScale( tabs[M_SCALE].ptr );
		tabs[M_SCALE].ptr[tabs[M_SCALE].len] = ',';
		index = TabGetLong( &tabs[M_INDEX] );
		if ( index == 0 ) {
			CheckCarDlgItemIndex( &carDlgItemIndex );
			index = carDlgItemIndex;
		} else {
			carDlgItemIndex = index;
			if ( !CheckCarDlgItemIndex(&index) ) {
				if ( !duplicateIndexError ) {
					NoticeMessage( MSG_CARIMP_DUP_INDEX, _("Ok"), NULL );
					duplicateIndexError++;
				}
				carDlgItemIndex = index;
			}
		}
#ifdef OBSOLETE
		if ( TabStringCmp( "Unknown", &tabs[M_MANUF] ) != 0 &&
			 TabStringCmp( "Custom", &tabs[M_MANUF] ) != 0 ) {
			if ( tabs[M_PARTNO].len == 0 ) {
				rc = NoticeMessage( MSG_CARIMP_MISSING_PARTNO, _("Continue"), _("Stop"), tabs[M_MANUF].ptr );
				if ( rc <= 0 ) {
					fclose( f );
					RestoreLocale(oldLocale);
					return FALSE;
				}
				continue;
			}
		}
#endif
		dim.carLength = TabGetFloat( &tabs[M_CARLENGTH] );
		dim.carWidth = TabGetFloat( &tabs[M_CARWIDTH] );
		dim.coupledLength = TabGetFloat( &tabs[M_CPLDLENGTH] );
		dim.truckCenter = TabGetFloat( &tabs[M_TRKCENTER] );
		partP = NULL;
		if ( tabs[M_MANUF].len > 0 && tabs[M_PARTNO].len > 0 )
			partP = CarPartFind( tabs[M_MANUF].ptr, tabs[M_MANUF].len, tabs[M_PARTNO].ptr, tabs[M_PARTNO].len, scale );
		if ( partP ) {
			TabStringExtract( partP->title, 7, partTabs );
			if ( tabs[M_PROTO].len == 0 && partTabs[T_PROTO].len > 0 ) { tabs[M_PROTO].ptr = partTabs[T_PROTO].ptr; tabs[M_PROTO].len = partTabs[T_PROTO].len; }
			if ( tabs[M_DESC].len == 0 && partTabs[T_DESC].len > 0 ) { tabs[M_DESC].ptr = partTabs[T_DESC].ptr; tabs[M_DESC].len = partTabs[T_DESC].len; }
			if ( tabs[M_ROADNAME].len == 0 && partTabs[T_ROADNAME].len > 0 ) { tabs[M_ROADNAME].ptr = partTabs[T_ROADNAME].ptr; tabs[M_ROADNAME].len = partTabs[T_ROADNAME].len; }
			if ( tabs[M_REPMARK].len == 0 && partTabs[T_REPMARK].len > 0 ) { tabs[M_REPMARK].ptr = partTabs[T_REPMARK].ptr; tabs[M_REPMARK].len = partTabs[T_REPMARK].len; }
			if ( tabs[M_NUMBER].len == 0 && partTabs[T_NUMBER].len > 0 ) { tabs[M_NUMBER].ptr = partTabs[T_NUMBER].ptr; tabs[M_NUMBER].len = partTabs[T_NUMBER].len; }
			if ( dim.carLength <= 0 ) dim.carLength = partP->dim.carLength;
			if ( dim.carWidth <= 0 ) dim.carWidth = partP->dim.carWidth;
			if ( dim.coupledLength <= 0 ) dim.coupledLength = partP->dim.coupledLength;
			if ( dim.truckCenter <= 0 ) dim.truckCenter = partP->dim.truckCenter;
		}
		cp = TabStringCpy( title, &tabs[M_MANUF] );
		*cp++ = '\t';
		cp = TabStringCpy( cp, &tabs[M_PROTO] );
		*cp++ = '\t';
		cp = TabStringCpy( cp, &tabs[M_DESC] );
		*cp++ = '\t';
		cp = TabStringCpy( cp, &tabs[M_PARTNO] );
		*cp++ = '\t';
		cp = TabStringCpy( cp, &tabs[M_ROADNAME] );
		*cp++ = '\t';
		cp = TabStringCpy( cp, &tabs[M_REPMARK] );
		*cp++ = '\t';
		cp = TabStringCpy( cp, &tabs[M_NUMBER] );
		*cp = '\0';
		options = TabGetLong( &tabs[M_OPTIONS] );
		type = TabGetLong( &tabs[M_TYPE] );
		color = TabGetLong( &tabs[M_COLOR] );
		purchPrice = TabGetFloat( &tabs[M_PURCHPRICE] );
		currPrice = TabGetFloat( &tabs[M_CURRPRICE] );
		condition = TabGetLong( &tabs[M_CONDITION] );
		purchDate = TabGetLong( &tabs[M_PURCHDATE] );
		srvcDate = TabGetLong( &tabs[M_SRVDATE] );
		if ( dim.carLength <= 0 || dim.carWidth <= 0 || dim.coupledLength <= 0 || dim.truckCenter <= 0 ) {
			rc = NoticeMessage( MSG_CARIMP_MISSING_DIMS, _("Yes"), _("No"), message );
			if ( rc <= 0 ) {
				fclose( f );
				RestoreLocale(oldLocale);
				return FALSE;
			}
			continue;
		}
		item = CarItemNew( NULL, PARAM_CUSTOM, index, scale, title, options, type,
				&dim, wDrawFindColor(color),
				purchPrice, currPrice, condition, purchDate, srvcDate );
		if ( tabs[M_NOTES].len > 0 ) {
			item->data.notes = cp = MyMalloc( tabs[M_NOTES].len+1 );
			for ( cq=tabs[M_NOTES].ptr,len=tabs[M_NOTES].len; *cq&&len; ) {
				if ( strncmp( cq, "<NL>", 4 ) == 0 ) {
					*cp++ = '\n';
					cq += 4;
					len -= 4;
				} else {
					*cp++ = *cq++;
					len -= 1;
				}
			}
		}
		changed++;
		SetWindowTitle();
	}
	fclose( f );
	RestoreLocale(oldLocale);
	CarInvListLoad();
	return TRUE;
}



static struct wFilSel_t * carInvImportCsv_fs;
static void CarInvDlgImportCsv( void )
{
	if ( carInvImportCsv_fs == NULL )
		carInvImportCsv_fs = wFilSelCreate( mainW, FS_LOAD, 0, _("Import Cars"),
				_("Comma-Separated-Values|*.csv"), CarInvImportCsv, NULL );
	wFilSelect( carInvImportCsv_fs, curDirName );
}


static void CsvFormatString(
		FILE * f,
		char * str,
		int len,
		char * sep )
{
	while ( str && len>0 && str[len-1]=='\n' ) len--;
	if ( *str && len ) {
		fputc( '"', f );
		for ( ; *str && len; str++,len-- ) {
			if ( !iscntrl((unsigned char) *str ) ) {
				if ( *str == '"' )
					fputc( '"', f );
				fputc( *str, f );
			} else if ( *str == '\n' && str[1] && len > 1 ) {
				fprintf( f, "<NL>" );
			}
		}
		fputc( '"', f );
	}
	fprintf( f, "%s", sep );
}


static void CsvFormatLong(
		FILE * f,
		long val,
		char * sep )
{
	if ( val != 0 )
		fprintf( f, "%ld", val );
	fprintf( f, "%s", sep );
}


static void CsvFormatFloat(
		FILE * f,
		FLOAT_T val,
		int digits,
		char * sep )
{
	if ( val != 0.0 )
		fprintf( f, "%0.*f", digits, val );
	fprintf( f, "%s", sep );
}


static int CarInvExportCsv(
		const char * pathName,
		const char * fileName,
		void * data )
{
	FILE * f;
	carItem_p item;
	long inx;
	tabString_t tabs[7];
	char * sp;
	char *oldLocale = NULL;

	if ( pathName == NULL )
		return TRUE;
	SetCurDir( pathName, fileName );
	f = fopen( pathName, "w" );
	if ( f == NULL ) {
		NoticeMessage( MSG_OPEN_FAIL, _("Continue"), NULL, _("Export Cars"), fileName, strerror(errno) );
		return FALSE;
	}

	oldLocale = SaveLocale("C");

	for ( inx=0; inx<sizeof carCsvColumnTitles/sizeof carCsvColumnTitles[0]; inx++ ) {
		CsvFormatString( f, carCsvColumnTitles[inx], strlen(carCsvColumnTitles[inx]), inx<(sizeof carCsvColumnTitles/sizeof carCsvColumnTitles[0])-1?",":"\n" );
	}
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ ) {
		item = carItemInfo( inx );
		TabStringExtract( item->title, 7, tabs );
		CsvFormatLong( f, item->index, "," );
		sp = GetScaleName(item->scaleInx);
		CsvFormatString( f, sp, strlen(sp), "," );
		CsvFormatString( f, tabs[T_MANUF].ptr, tabs[T_MANUF].len, "," );
		CsvFormatLong( f, item->type, "," );
		CsvFormatString( f, tabs[T_PART].ptr, tabs[T_PART].len, "," );
		CsvFormatString( f, tabs[T_PROTO].ptr, tabs[T_PROTO].len, "," );
		CsvFormatString( f, tabs[T_DESC].ptr, tabs[T_DESC].len, "," );
		CsvFormatString( f, tabs[T_ROADNAME].ptr, tabs[T_ROADNAME].len, "," );
		CsvFormatString( f, tabs[T_REPMARK].ptr, tabs[T_REPMARK].len, "," );
		CsvFormatString( f, tabs[T_NUMBER].ptr, tabs[T_NUMBER].len, "," );
		CsvFormatLong( f, item->options, "," );
		CsvFormatFloat( f, item->dim.carLength, 3, "," );
		CsvFormatFloat( f, item->dim.carWidth, 3, "," );
		CsvFormatFloat( f, item->dim.coupledLength, 3, "," );
		CsvFormatFloat( f, item->dim.truckCenter, 3, "," );
		CsvFormatLong( f, wDrawGetRGB(item->color), "," );
		CsvFormatFloat( f, item->data.purchPrice, 2, "," );
		CsvFormatFloat( f, item->data.currPrice, 2, "," );
		CsvFormatLong( f, item->data.condition, "," );
		CsvFormatLong( f, item->data.purchDate, "," );
		CsvFormatLong( f, item->data.serviceDate, "," );
		if ( item->data.notes )
			CsvFormatString( f, item->data.notes, strlen(item->data.notes), "\n" );
		else
			CsvFormatString( f, "", strlen(""), "\n" );
	}
	fclose( f );
	RestoreLocale(oldLocale);
	return TRUE;
}


static struct wFilSel_t * carInvExportCsv_fs;
static void CarInvDlgExportCsv( void )
{
	if ( carItemInfo_da.cnt <= 0 )
		return;
	if ( carInvExportCsv_fs == NULL )
		carInvExportCsv_fs = wFilSelCreate( mainW, FS_SAVE, 0, _("Export Cars"),
				_("Comma-Separated-Values|*.csv"), CarInvExportCsv, NULL );
	wFilSelect( carInvExportCsv_fs, curDirName );
}


static void CarInvLoadItem(
		carItem_p item )
{
/* "Index", "Scale", "Manufacturer", "Type", "Part No", "Description", "Roadname", "RepMarks",
   "Purch Price", "Curr Price", "Condition", "Purch Date", "Service Date", "Location", "Notes" */
	char *condition;
	char *location;
	char *manuf;
	char *road;
	char notes[100];
	tabString_t tabs[7];

	TabStringExtract( item->title, 7, tabs );
	if ( item->data.notes ) {
		strncpy( notes, item->data.notes, sizeof notes - 1 );
		notes[sizeof notes - 1] = '\0';
	} else {
		notes[0] = '\0';
	}
	condition =
		(item->data.condition < 10) ? N_("N/A"):
		(item->data.condition < 30) ? N_("Poor"):
		(item->data.condition < 50) ? N_("Fair"):
		(item->data.condition < 70) ? N_("Good"):
		(item->data.condition < 90) ? N_("Excellent"):
		N_("Mint");

	if ( item->car && !IsTrackDeleted(item->car) )
		location = N_("Layout");
	else
		location = N_("Shelf");

	manuf = TabStringDup(&tabs[T_MANUF]);
	road = TabStringDup(&tabs[T_ROADNAME]);
	sprintf( message, "%ld\t%s\t%s\t%.*s\t%s\t%.*s%s%.*s\t%s\t%.*s%s%.*s\t%0.2f\t%0.2f\t%s\t%ld\t%ld\t%s\t%s",
				item->index, GetScaleName(item->scaleInx),
				_(manuf),
				tabs[T_PART].len, tabs[T_PART].ptr,
				_(typeListMap[CarProtoFindTypeCode(item->type)].name),
				tabs[T_PROTO].len, tabs[T_PROTO].ptr,
				(tabs[T_PROTO].len>0 && tabs[T_DESC].len)?"/":"",
				tabs[T_DESC].len, tabs[T_DESC].ptr,
				_(road),
				tabs[T_REPMARK].len, tabs[T_REPMARK].ptr,
				(tabs[T_REPMARK].len>0&&tabs[T_NUMBER].len>0)?" ":"",
				tabs[T_NUMBER].len, tabs[T_NUMBER].ptr,
				item->data.purchPrice, item->data.currPrice, _(condition), item->data.purchDate, item->data.serviceDate, _(location), notes );
	if (manuf) MyFree(manuf);
	if (road) MyFree(road);
	wListAddValue( (wList_p)carInvPLs[I_CI_LIST].control, message, NULL, item );
}


static int Cmp_carInvItem(
		const void * ptr1,
		const void * ptr2 )
{
	carItem_p item1 = *(carItem_p*)ptr1;
	carItem_p item2 = *(carItem_p*)ptr2;
	tabString_t tabs1[7], tabs2[7];
	int inx;
	int rc;

	TabStringExtract( item1->title, 7, tabs1 );
	TabStringExtract( item2->title, 7, tabs2 );
	for ( inx=0,rc=0; inx<N_SORT&&rc==0; inx++ ) {
		switch ( carInvSort[inx] ) {
		case S_INDEX:
			rc = (int)(item1->index-item2->index);
			break;
		case S_SCALE:
			rc = (int)(item1->scaleInx-item2->scaleInx);
		case S_MANUF:
			rc = strncasecmp( tabs1[T_MANUF].ptr, tabs2[T_MANUF].ptr, max(tabs1[T_MANUF].len,tabs2[T_MANUF].len) );
			break;
		case S_TYPE:
			rc = (int)(item1->type-item2->type);
			break;
		case S_PARTNO:
			rc = strncasecmp( tabs1[T_PART].ptr, tabs2[T_PART].ptr, max(tabs1[T_PART].len,tabs2[T_PART].len) );
			break;
		case S_DESC:
			rc = strncasecmp( tabs1[T_PROTO].ptr, tabs2[T_PROTO].ptr, max(tabs1[T_PROTO].len,tabs2[T_PROTO].len) );
			if ( rc != 0 )
				break;
			rc = strncasecmp( tabs1[T_DESC].ptr, tabs2[T_DESC].ptr, max(tabs1[T_DESC].len,tabs2[T_DESC].len) );
			break;
		case S_ROADNAME:
			rc = strncasecmp( tabs1[T_ROADNAME].ptr, tabs2[T_ROADNAME].ptr, max(tabs1[T_ROADNAME].len,tabs2[T_ROADNAME].len) );
			break;
		case S_REPMARKS:
			rc = strncasecmp( tabs1[T_REPMARK].ptr, tabs2[T_REPMARK].ptr, max(tabs1[T_REPMARK].len,tabs2[T_REPMARK].len) );
			break;
		case S_PURCHPRICE:
			rc = (int)(item1->data.purchPrice-item2->data.purchPrice);
			break;
		case S_CURRPRICE:
			rc = (int)(item1->data.currPrice-item2->data.currPrice);
			break;
		case S_CONDITION:
			rc = (int)(item1->data.condition-item2->data.condition);
			break;
		case S_PURCHDATE:
			rc = (int)(item1->data.purchDate-item2->data.purchDate);
			break;
		case S_SRVDATE:
			rc = (int)(item1->data.serviceDate-item2->data.serviceDate);
			break;
		default:
			break;
		}
	}
	return rc;
}

static void CarInvListLoad( void )
{
	int inx;
	carItem_p item;

	qsort( carItemInfo_da.ptr, carItemInfo_da.cnt, sizeof item, Cmp_carInvItem );
	ParamControlShow( &carInvPG, I_CI_LIST, FALSE );
	wListClear( (wList_p)carInvPLs[I_CI_LIST].control );
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ ) {
		item = carItemInfo(inx);
		CarInvLoadItem( item );
	}
	ParamControlShow( &carInvPG, I_CI_LIST, TRUE );
	ParamControlActive( &carInvPG, I_CI_EDIT, FALSE );
	ParamControlActive( &carInvPG, I_CI_DELETE, FALSE );
	ParamControlActive( &carInvPG, I_CI_EXPORT_CSV, carItemInfo_da.cnt > 0 );
	ParamDialogOkActive( &carInvPG, FALSE );
}


static void CarInvDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	carItem_p item = NULL;
	wIndex_t cnt, selinx, selcnt;
	wBool_t enableDelete;

	if ( inx >= I_CI_SORT && inx < I_CI_SORT+N_SORT ) {
		item = CarInvDlgFindCurrentItem();
		CarInvListLoad();
		if ( item ) {
			carInvInx = (wIndex_t)CarItemFindIndex( item );
			if ( carInvInx >= 0 )
				ParamLoadControl( &carInvPG, I_CI_LIST );
		}
	} else if ( inx == I_CI_LIST ) {
		cnt = wListGetCount( (wList_p)carInvPLs[I_CI_LIST].control );
		enableDelete = TRUE;
		for ( selinx=selcnt=0; selinx<cnt; selinx++ ) {
			if ( wListGetItemSelected( (wList_p)carInvPLs[I_CI_LIST].control, selinx ) ) {
				selcnt++;
				item = (carItem_p)wListGetItemContext( (wList_p)carInvPLs[I_CI_LIST].control, selinx );
				if ( item && item->car && !IsTrackDeleted( item->car ) ) {
					enableDelete = FALSE;
					break;
				}
			}
		}
		item = CarInvDlgFindCurrentItem();
		ParamDialogOkActive( pg, selcnt==1 && item && item->car && !IsTrackDeleted(item->car) );
		ParamControlActive( &carInvPG, I_CI_EDIT, selcnt==1 && item && (item->car==NULL || IsTrackDeleted(item->car)) );
		ParamControlActive( &carInvPG, I_CI_DELETE, selcnt>0 && enableDelete );
	}
}


static void CarInvListAdd(
		carItem_p item )
{
	CarInvListLoad();
	carInvInx = (wIndex_t)CarItemFindIndex( item );
	if ( carInvInx >= 0 ) {
		ParamLoadControl( &carInvPG, I_CI_LIST );
	}
}


static void CarInvListUpdate(
		carItem_p item )
{
	CarInvListLoad();
	carInvInx = (wIndex_t)CarItemFindIndex( item );
	if ( carInvInx >= 0 ) {
		ParamLoadControl( &carInvPG, I_CI_LIST );
	}
}


EXPORT void DoCarDlg( void )
{
	int inx, inx2;
	if ( carInvPG.win == NULL ) {
		ParamCreateDialog( &carInvPG, MakeWindowTitle(_("Car Inventory")), _("Find"), CarInvDlgFind, wHide, TRUE, NULL, F_BLOCK|F_RESIZE|F_RECALLSIZE|PD_F_ALT_CANCELLABEL, CarInvDlgUpdate );
		for ( inx=I_CI_SORT; inx<I_CI_SORT+N_SORT; inx++ ) {
			for ( inx2=0; inx2<sizeof sortOrders/sizeof sortOrders[0]; inx2++ ) {
				wListAddValue( (wList_p)carInvPLs[inx].control, _(sortOrders[inx2]), NULL, NULL );
				ParamLoadControl( &carInvPG, inx );
			}
		}
		ParamDialogOkActive( &carInvPG, FALSE );
	}
	CarInvListLoad();
	wShow( carInvPG.win );
}


static void CarDlgChange( long changes )
{
	if ( (changes&CHANGE_SCALE) ) {
		carPartChangeLevel = 0;
		carDlgCouplerLength = 0.0;
	}
}


EXPORT void ClearCars( void )
{
	int inx;
	for ( inx=0; inx<carItemInfo_da.cnt; inx++ )
		MyFree( carItemInfo(inx) );
	carItemInfo_da.cnt = 0;
	carItemInfo_da.max = 0;
	if ( carItemInfo_da.ptr )
		MyFree( carItemInfo_da.ptr );
	carItemInfo_da.ptr = NULL;
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
	carItemInfo_da.cnt = carItemInfo_da.max = 0;
	carItemInfo_da.ptr = NULL;
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
	carDlgBodyColor = wDrawFindColor( wRGB(255,128,0) );
	ParamRegister( &carDlgPG );
	ParamRegister( &carInvPG );
	RegisterChangeNotification( CarDlgChange );
	AddParam( "CARPROTO ", CarProtoRead );
	AddParam( "CARPART ", CarPartRead );
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
		if ( partP == NULL )
			return FALSE;
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
		if ( tabs[T_REPMARK].len == 0 )
			rd_inx = T_ROADNAME;
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
		if ( protoP == NULL )
			return FALSE;
		carDlgUpdateProtoPtr = protoP;
		DoCarPartDlg( protoUpdActions );
		return TRUE;
	case CUSTMGM_CAN_DELETE:
		return TRUE;
	case CUSTMGM_DO_DELETE:
		CarProtoDelete( protoP );
		return TRUE;
	case CUSTMGM_GET_TITLE:
		sprintf( message, "\t%s\t\t%s\t%s", _("Prototype"), _(typeListMap[CarProtoFindTypeCode(protoP->type)].name), protoP->desc );
		return TRUE;
	}
	return FALSE;
}


#include "bitmaps/carpart.xpm"
#include "bitmaps/carproto.xpm"

EXPORT void CarCustMgmLoad( void )
{
	long parentX, partX, protoX;
	carPartParent_p parentP;
	carPart_p partP;
	carProto_p carProtoP;
	static wIcon_p carpartI = NULL;
	static wIcon_p carprotoI = NULL;

	if ( carpartI == NULL )
		carpartI = wIconCreatePixMap( carpart_xpm );
	if ( carprotoI == NULL )
		carprotoI = wIconCreatePixMap( carproto_xpm );

	for ( parentX=0; parentX<carPartParent_da.cnt; parentX++ ) {
		parentP = carPartParent(parentX);
		for ( partX=0; partX<parentP->parts_da.cnt; partX++ ) {
			partP = carPart(parentP,partX);
			if ( partP->paramFileIndex != PARAM_CUSTOM )
				continue;
			CustMgmLoad( carpartI, CarPartCustMgmProc, (void*)partP );
		}
	}

	for ( protoX=0; protoX<carProto_da.cnt; protoX++ ) {
		carProtoP = carProto(protoX);
		if ( carProtoP->paramFileIndex != PARAM_CUSTOM )
			continue;
		if (carProtoP->paramFileIndex == PARAM_CUSTOM) {
			CustMgmLoad( carprotoI, CarProtoCustMgmProc, (void*)carProtoP );
		}
	}
}
