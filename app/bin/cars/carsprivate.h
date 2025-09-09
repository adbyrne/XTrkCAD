#ifndef HAVE_CARSPRIVATE_H
#define HAVE_CARS_PRIVATE_H

#include "common.h"
#include "draw.h"
#include "param.h"
#include "tabstring.h"

// transform_pt.c

void RotatePts(int cnt, coOrd* pts, coOrd orig, ANGLE_T angle);
void RescalePts(int cnt, coOrd* pts, FLOAT_T scale_x, FLOAT_T scale_y);
void MovePts(int cnt, coOrd* pts, coOrd orig);

#define N_TYPELISTMAP           (7)


// carproto
struct carDim_s {
	DIST_T			carLength;
	DIST_T			carWidth;
	DIST_T			truckCenter;
	DIST_T          truckCenterOffset;
	DIST_T			coupledLength;
};
typedef struct carDim_s carDim_t;

struct carProto_s {
	char* contentsLabel;
	wIndex_t        paramFileIndex;
	char* desc;
	long            options;
	long            type;
	carDim_t        dim;
	int             segCnt;
	trkSeg_p        segPtr;
	coOrd           size;
	coOrd           orig;
};
typedef struct carProto_s carProto_t;
typedef struct carProto_s* carProto_p;

struct carPartParent_s {
	char* manuf;
	char* proto;
	SCALEINX_T scale;
	dynArr_t parts_da;
};

typedef struct carPartParent_s carPartParent_t;
typedef struct carPartParent_s* carPartParent_p;

//struct carPart_s {
//	carPartParent_p parent;
//	wIndex_t paramFileIndex;
//	char* title;
//	long options;
//	long type;
//	carDim_t dim;
//	wDrawColor color;
//	char* partnoP;
//	int partnoL;
//};

typedef struct {
	char* name;
	int len;
} cmp_key_t;

struct nameLongMap_s {
	char* name;
	long value;
};

wIndex_t MapCondition(long conditionValue);

typedef  struct nameLongMap_s nameLongMap_t;
typedef nameLongMap_t* nameLongMap_p;
extern nameLongMap_t typeListMap[];

extern dynArr_t carProto_da;

void CarProtoDlgCreateDummyOutline(int* segCntP, trkSeg_p* segPtrP,
                                   BOOL_T isLoco, DIST_T length, DIST_T width, wDrawColor color);
carProto_p CarProtoFind(char* desc);
int CarProtoFindTypeCode(long code);
carProto_p CarProtoLookup(char* desc, BOOL_T createMissing, BOOL_T isLoco,
                          DIST_T length, DIST_T width);
carProto_p CarProtoNew(carProto_p proto, int paramFileIndex, char* desc,
                       long options, long type, const carDim_t* dim, wIndex_t segCnt, trkSeg_p segPtr);
BOOL_T CarProtoRead(char* line);
BOOL_T CarProtoWrite(FILE* f, const carProto_t* proto);
BOOL_T CarProtoCustomSave(FILE* f);

void CarProtoDrawTruck(
        drawCmd_t* d,
        DIST_T width,
        FLOAT_T ratio,
        coOrd pos,
        ANGLE_T angle);

void CarProtoDrawCoupler(
        drawCmd_t* d,
        DIST_T length,
        FLOAT_T ratio,
        coOrd pos,
        ANGLE_T angle);


// carpart

typedef struct roadnameMap_s* roadnameMap_p;

roadnameMap_p LoadRoadnameList(tabString_p roadnameTab, tabString_p repmarkTab);

struct carPart_s {
	carPartParent_p parent;
	wIndex_t paramFileIndex;
	char* title;
	long options;
	long type;
	carDim_t dim;
	wDrawColor color;
	char* partnoP;
	int partnoL;
};

typedef struct carPart_s carPart_t;
typedef struct carPart_s *carPart_p;


struct roadnameMap_s {
	char* roadname;
	char* repmark;
};

typedef struct roadnameMap_s roadnameMap_t;
typedef struct carDim_s* carDim_p;

int Cmp_part(void* key, void* elem);
int Cmp_roadnameMap(void* key, void* elem);
void CarPartDelete(carPart_p partP);
carPart_p CarPartFind(char* manufP, int manufL, char* partnoP, int partnoL,
                      SCALEINX_T scale);
carPart_p CarPartNew(carPart_p partP, int paramFileIndex, SCALEINX_T scaleInx,
                     char* title, long options, long type, const carDim_p dim, wDrawColor color);
BOOL_T CarDescCustomSave(FILE* f);
BOOL_T CarPartWrite(FILE* f, carPart_p partP);
BOOL_T CarPartRead(char* line);
BOOL_T CheckAvail(carPartParent_p parentP);

#define carPartParent(N)	DYNARR_N(carPartParent_p, carPartParent_da, N)
#define carPart(P,N)		DYNARR_N(carPart_p, (P)->parts_da, N)
#define carProto(N) DYNARR_N( carProto_t*, carProto_da, N )

struct carItem_s;
typedef struct carItem_s carItem_t;

#define CAR_DESC_COUPLER_MODE_BODY	(1L<<0)
#define CAR_DESC_IS_LOCO			(1L<<1)
#define CAR_DESC_IS_LOCO_MASTER		(1L<<2)
#define CAR_ITEM_HASNOTES			(1L<<8)
#define CAR_ITEM_ONLAYOUT			(1L<<9)

#define CAR_DESC_BITS				(0x000000FF)
#define CAR_ITEM_BITS				(0x0000FF00)

//caritem
typedef struct {
	char* number;
	FLOAT_T			purchPrice;
	FLOAT_T			currPrice;
	long			condition;
	long			purchDate;
	long			serviceDate;
	char* notes;
} carData_t;

struct carItem_s {
	long			index;
	SCALEINX_T		scaleInx;
	char* contentsLabel;
	char* title;
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

typedef struct carItem_s carItem_t;
typedef struct carItem_s *carItem_p;

extern dynArr_t carItemInfo_da;
extern carItem_p carDlgUpdateItemPtr;
#define carItemInfo(N) DYNARR_N( carItem_t*, carItemInfo_da, N )

#define carItemHotbar(N)			DYNARR_N( carItem_p, carItemHotbar_da, N )

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
        long serviceDate);

#define N_CONDLISTMAP			(6)
nameLongMap_t condListMap[];

EXPORT long CarItemFindIndex(carItem_p item);
void CarItemGetSegs(carItem_p item);

extern paramGroup_t carInvPG;
void CarInvListAdd(carItem_p item);
void CarInvListUpdate(carItem_p item);

typedef enum carDlgAction {
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

extern carDlgAction_e itemNewActions[];
extern carDlgAction_e itemUpdActions[];
extern long carDlgItemIndex;
extern int log_carDlgDims;

BOOL_T CheckCarDlgItemIndex(long* index);
void DoCarPartDlg(carDlgAction_e* actions);


#endif // !HAVE_CARSPRIVATE_H

