#ifndef HAVE_CARSPRIVATE_H
#define HAVE_CARSPRIVATE_H

#include "common.h"
#include "draw.h"
#include "param.h"
#include "tabstring.h"

// transform_pt.c

void RotatePts(int cnt, coOrd *pts, coOrd orig, ANGLE_T angle);
void RescalePts(int cnt, coOrd *pts, FLOAT_T scale_x, FLOAT_T scale_y);
void MovePts(int cnt, coOrd *pts, coOrd orig);

#define N_TYPELISTMAP (7)

// carproto
struct carDim_s {
	DIST_T carLength;
	DIST_T carWidth;
	DIST_T truckCenter;
	DIST_T truckCenterOffset;
	DIST_T coupledLength;
};
typedef struct carDim_s carDim_t;

struct carProto_s {
	char *contentsLabel;
	wIndex_t paramFileIndex;
	char *desc;
	long options;
	long type;
	carDim_t dim;
	int segCnt;
	trkSeg_p segPtr;
	coOrd size;
	coOrd orig;
};
typedef struct carProto_s carProto_t;
typedef struct carProto_s *carProto_p;

struct carPartParent_s {
	char *manuf;
	char *proto;
	SCALEINX_T scale;
	dynArr_t parts_da;
};

typedef struct carPartParent_s carPartParent_t;
typedef struct carPartParent_s *carPartParent_p;

typedef struct {
	char *name;
	int len;
} cmp_key_t;

struct nameLongMap_s {
	char *name;
	long value;
};
typedef struct nameLongMap_s nameLongMap_t;
typedef nameLongMap_t *nameLongMap_p;

extern nameLongMap_t typeListMap[];

/* Loco vs. non-loco is a band of the "type" code, not a stored bit: 1010x/
 * 1020x/1030x (Diesel/Steam/Elect Loco) fall in [10000,19999], every other
 * category is outside it. */
#define CAR_TYPE_LOCO_MIN (10000L)
#define CAR_TYPE_LOCO_MAX (19999L)
BOOL_T IsLocoType(long type);

wIndex_t MapCondition(long conditionValue);

extern dynArr_t carProto_da;
#define carProto(N) DYNARR_N(carProto_t *, carProto_da, N)

void CarProtoDlgCreateDummyOutline(int *segCntP, trkSeg_p *segPtrP,
                                   BOOL_T isLoco, DIST_T length, DIST_T width, wDrawColor color);
carProto_p CarProtoFind(char *desc);
int CarProtoFindTypeCode(long code);
carProto_p CarProtoLookup(char *desc, BOOL_T createMissing, BOOL_T isLoco,
                          DIST_T length, DIST_T width);
carProto_p CarProtoNew(carProto_p proto, int paramFileIndex, char *desc,
                       long options, long type, const carDim_t *dim, wIndex_t segCnt, trkSeg_p segPtr);
BOOL_T CarProtoWrite(FILE *f, const carProto_t *proto);
BOOL_T CarProtoCustomSave(FILE *f);
void CarProtoDelete(carProto_p protoP);

void CarProtoDrawTruck(
        drawCmd_t *d,
        DIST_T width,
        FLOAT_T ratio,
        coOrd pos,
        ANGLE_T angle);

void CarProtoDrawCoupler(
        drawCmd_t *d,
        DIST_T length,
        FLOAT_T ratio,
        coOrd pos,
        ANGLE_T angle);

void InitCarProto(void);
int CarProtoCustMgmProc(int cmd, void *data);
extern BOOL_T carProtoListChanged;

// carpart
extern BOOL_T roadnameMapChanged;
extern BOOL_T carPartListChanged;

struct roadnameMap_s {
	char *roadname;
	char *repmark;
};
typedef struct roadnameMap_s *roadnameMap_p;
typedef struct roadnameMap_s roadnameMap_t;

roadnameMap_p LoadRoadnameList(tabString_p roadnameTab, tabString_p repmarkTab);

struct carPart_s {
	carPartParent_p parent;
	wIndex_t paramFileIndex;
	char *title;
	long options;
	long type;
	carDim_t dim;
	wDrawColor color;
	char *partnoP;
	int partnoL;
};

typedef struct carPart_s carPart_t;
typedef struct carPart_s *carPart_p;

typedef struct carDim_s *carDim_p;

int Cmp_part(void *key, void *elem);
int Cmp_roadnameMap(void *key, void *elem);

carPart_p CarPartFind(char *manufP, int manufL, char *partnoP, int partnoL,
                      SCALEINX_T scale);
carPart_p CarPartNew(carPart_p partP, int paramFileIndex, SCALEINX_T scaleInx,
                     char *title, long options, long type, const carDim_p dim, wDrawColor color);
BOOL_T CarDescCustomSave(FILE *f);
BOOL_T CarPartWrite(FILE *f, carPart_p partP);
void CarPartDelete(carPart_p partP);

BOOL_T CheckAvail(carPartParent_p parentP);

void InitCarPart(void);

int CarPartCustMgmProc(int cmd, void *data);

extern dynArr_t carPartParent_da;
#define carPartParent(N) DYNARR_N(carPartParent_p, carPartParent_da, N)
#define carPart(P, N) DYNARR_N(carPart_p, (P)->parts_da, N)

// caritem
typedef struct {
	char *number;
	FLOAT_T purchPrice;
	FLOAT_T currPrice;
	long condition;
	long purchDate;
	long serviceDate;
	char *notes;
} carData_t;

struct carItem_s {
	long index;
	SCALEINX_T scaleInx;
	char *contentsLabel;
	char *title;
	carProto_p proto;
	DIST_T barScale;
	wDrawColor color;
	long options;
	long type;
	carDim_t dim;
	carData_t data;
	wIndex_t segCnt;
	trkSeg_p segPtr;
	track_p car;
	coOrd pos;
	ANGLE_T angle;
};

typedef struct carItem_s carItem_t;
typedef struct carItem_s *carItem_p;

#define CAR_DESC_COUPLER_MODE_BODY (1L << 0)
#define CAR_DESC_IS_LOCO (1L << 1)
#define CAR_DESC_IS_LOCO_MASTER (1L << 2)
#define CAR_ITEM_HASNOTES (1L << 8)
#define CAR_ITEM_ONLAYOUT (1L << 9)

#define CAR_DESC_BITS (0x000000FF)
#define CAR_ITEM_BITS (0x0000FF00)

extern dynArr_t carItemInfo_da;
extern carItem_p carDlgUpdateItemPtr;
/* Set by CarItemNew, consumed (and cleared) by carinvdlg.c's CHANGE_PARAMS
 * handler -- same poll-and-consume pattern as carProtoListChanged/
 * carPartListChanged, see CarCustMgmChanged in dcar.c. Lets the roster
 * dialog notice a car added/edited by the (non-modal) car editor while the
 * roster dialog itself stays open in the background. */
extern BOOL_T carItemListChanged;
#define carItemInfo(N) DYNARR_N(carItem_t *, carItemInfo_da, N)

#define carItemHotbar(N) DYNARR_N(carItem_p, carItemHotbar_da, N)

carItem_p CarItemNew(
        carItem_p item,
        int paramFileIndex,
        long itemIndex,
        SCALEINX_T scale,
        char *title,
        long options,
        long type,
        carDim_t *dim,
        wDrawColor color,
        FLOAT_T purchPrice,
        FLOAT_T currPrice,
        long condition,
        long purchDate,
        long serviceDate);

#define N_CONDLISTMAP (6)
extern nameLongMap_t condListMap[];

EXPORT long CarItemFindIndex(carItem_p item);
EXPORT void CarItemDelete(carItem_p item);
void CarItemGetSegs(carItem_p item);

long OffsetInchesToFile(double inches);
double OffsetFileToInches(long fileValue);

// carinv

extern paramGroup_t carInvPG;
void CarInvListAdd(carItem_p item);
void CarInvListUpdate(carItem_p item);

void InitCarInvDlg(void);

//careditlogic
typedef enum {
	S_Error,
	S_ItemSel, S_PartnoSel, S_ProtoSel
} carDlgState_e;

/* Moved from careditdlg.c so CarValidateIdentity (below) can take a level
 * instead of a state -- keeps careditlogic.c free of any careditdlg.c
 * dependency. CarDlgStateLevel's implementation stays in careditdlg.c. */
typedef enum {
	LVL_PROTO = 0,          /* real-world type            (S_ProtoSel)          */
	LVL_MODEL = 1,          /* manufacturer catalog model (S_PartnoSel)         */
	LVL_CAR   = 2,          /* owned roster car / item    (S_ItemSel/ItemEnter) */
	LVL_NONE  = -1
} carDlgLevel_e;

carDlgLevel_e CarDlgStateLevel( carDlgState_e st );

typedef enum {
	CAR_DATE_OK,
	CAR_DATE_EMPTY, /* leer -> valL = 0, kein Fehler */
	CAR_DATE_NOT_NUMERIC,
	CAR_DATE_WRONG_LENGTH,
	CAR_DATE_OUT_OF_RANGE,
	CAR_DATE_BAD_MONTH,
	CAR_DATE_BAD_DAY
} carDateResult_e;
carDateResult_e CarValidateDate(const char *str, long *result);

typedef enum { RS_UNRESOLVED, RS_RESOLVED, RS_NEW } resState_e;
resState_e CarDlgResState( wIndex_t inx, const char *str );
wBool_t CarValidatePrice(const char *str, FLOAT_T *price);
void SetNextPartno(char* partnoStr);
carDim_t SeedProtoDimsFromCatalog( carDim_t catalogDim, wIndex_t scaleInx );

#define CAR_DIMS_LENGTH_BAD        (1u << 0)   /* carLength */
#define CAR_DIMS_WIDTH_BAD         (1u << 1)   /* carWidth */
#define CAR_DIMS_TRKCENTER_BAD     (1u << 2)   /* truckCenter */
#define CAR_DIMS_TRKOFFSET_BAD     (1u << 3)   /* truckCenterOffset */
#define CAR_DIMS_CPLDLEN_BAD       (1u << 4)   /* coupledLength */

unsigned CarValidateDims( const carDim_t *dims );


#define CAR_IDENT_PROTO_EMPTY   (1u << 0)
#define CAR_IDENT_MANUF_EMPTY   (1u << 1)
#define CAR_IDENT_PARTNO_EMPTY	(1u << 2)
#define CAR_IDENT_CATALOG_UNRESOLVED (1u << 3)
#define CAR_IDENT_PROTO_UNRESOLVED (1u << 4)

unsigned CarValidateIdentity(
        carDlgLevel_e level, resState_e resCatalog, resState_e resProto,
        const char *manufStr, const char *protoStr, const char *partnoStr );


/* --- atomic commit (careditlogic.c) ------------------------------------ *
 * Staged, UI-free description of everything one press of the car dialog's OK
 * must create. CarDlgOk fills this from the live dialog fields (all string
 * members BORROWED from carDlg*Str except protoSegPtr, see below), then hands
 * it to CommitStaged, which creates the in-memory objects in dependency order
 * -- prototype, then catalog part, then N roster cars -- and, only once all of
 * them exist, persists via the injected save hook. Any failure at any step
 * unwinds everything THIS commit newly created, in reverse order, and reports
 * failure without persisting: a half-built car never reaches the arrays or
 * disk. An object is rollback-eligible only when THIS commit genuinely minted
 * it -- i.e. its make* flag is set, its updateXPtr is NULL (create, not edit),
 * AND CarProtoFind/CarPartFind found nothing before the create (not coalesced
 * onto an existing same-key record). Edited or coalesced-onto records are
 * never deleted on rollback; only freshly allocated ones are. */
typedef struct {
	/* prototype (created iff makeProto). protoName is BOTH the title's
	 * T_PROTO field and the prototype's desc -- one string. protoSegPtr is
	 * the one OWNED member: CarDlgOk hands over a heap buffer (always heap,
	 * even the dummy outline is memdup'd) and CommitStaged frees it in every
	 * return path. protoDim is already scaled to real-world by CarDlgOk. */
	BOOL_T     makeProto;
	char      *protoName;
	carDim_t   protoDim;
	wIndex_t   protoSegCnt;
	trkSeg_p   protoSegPtr;      /* OWNED; CommitStaged frees */
	carProto_p updateProtoPtr;   /* non-NULL: edit-in-place; never rolled back */

	/* part + item shared identity. manuf/partno are also the CarPartFind
	 * pre-existence key (desc is NOT part of the key, so it's title-only).
	 * The seven title fields join, in this order, to the tab string both
	 * CarPartNew and CarItemNew re-extract (careditdlg.c:2497):
	 *   manuf \t protoName \t descField \t partno \t roadname \t repmark \t number */
	char      *manuf;
	char      *descField;
	char      *partno;
	char      *roadname;
	char      *repmark;
	char      *number;          /* T_NUMBER; autoNumber bumps a COPY per car */

	BOOL_T     makePart;        /* resCatalog == RS_NEW && partno typed */
	carPart_p  updatePartPtr;    /* non-NULL: edit-in-place; never rolled back */

	SCALEINX_T scaleInx;
	long       options;
	long       type;            /* typeListMap[...].value, already resolved */
	carDim_t   dim;             /* model-scale dims for part + items */
	wDrawColor color;

	/* roster cars (created iff makeItems; count == quantity >= 1) */
	BOOL_T     makeItems;
	long       quantity;
	carItem_p  updateItemPtr;   /* non-NULL: edit-in-place; quantity forced to 1 */
	long       itemIndexStart;  /* car i gets itemIndexStart + i (create path) */
	BOOL_T     autoNumber;      /* quantity>1 && multiNum==0 && !editing: bump
	                             * trailing integer of `number` per car, but only
	                             * when it's a clean positive int (else all share) */

	/* per-item data, applied to every car in the loop. notesText is the
	 * finished note (UTF-8, single trailing '\n', extracted once by CarDlgOk);
	 * NULL/"" means clear any notes CarItemNew left on the item. */
	FLOAT_T    purchPrice;
	FLOAT_T    currPrice;
	long       condition;
	long       purchDate;
	long       serviceDate;
	const char *notesText;
} commitPlan_t;

/* Persistence hook: TRUE on success. CarDlgOk passes a thin wrapper over
 * CarDlgSaveCustom; tests pass a stub (counting calls / forcing FALSE) so
 * CommitStaged runs with no real file. Called at most once, after all
 * in-memory creation succeeds. */
typedef BOOL_T (*commitSaveFn_t)( void *ctx );

/* What CommitStaged created, for the caller's UI wrap-up (message text, list
 * refresh, roadname reload, prefs/index control). Zeroed on failure. protoP/
 * partP are non-NULL only when THIS commit minted them (not when coalesced
 * onto an existing record), so the caller must not assume them set just
 * because makeProto/makePart were requested. */
typedef struct {
	carProto_p protoP;
	carPart_p  partP;
	carItem_p  lastItemP;       /* last roster car created (for CarInvListAdd) */
	long       itemsCreated;
	long       nextItemIndex;   /* itemIndexStart + itemsCreated; caller writes
	                             * to prefs and reloads the index control (UI) */
} commitResult_t;

BOOL_T CommitStaged( const commitPlan_t *plan, commitSaveFn_t save,
                     void *saveCtx,
                     commitResult_t *out );

void CarProtoDelete( carProto_p protoP );  /* here for CommitStaged rollback */


// careditdlg


void CarDlgAddItem(void);
void CarDlgUpdItem(void);
extern long carDlgItemIndex;
extern int log_carDlgDims;

BOOL_T CheckCarDlgItemIndex(long *index);

// careditdlg
extern carPart_p carDlgUpdatePartPtr;
extern carProto_p carDlgUpdateProtoPtr;

void CarDlgUpdPart(void);
void CarDlgUpdProto(void);
void InitCarEditDlg(void);

#endif // !HAVE_CARSPRIVATE_H
