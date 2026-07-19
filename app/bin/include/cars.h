#ifndef HAVE_CARS_H
#define HAVE_CARS_H

#include "track.h"


typedef struct carProto_s* carProto_p;

void CarProtoDelete(carProto_p protoP);
void DeleteCarProto(int fileIndex);
enum paramFileState	GetCarProtoCompatibility(int paramFileIndex,
                SCALEINX_T scaleIndex);

typedef struct carPart_s* carPart_p;

void DeleteCarPart(int fileIndex);
enum paramFileState	GetCarPartCompatibility(int paramFileIndex,
                SCALEINX_T scaleIndex);

//struct carItem_s;
typedef struct carItem_s* carItem_p;
typedef struct vector_s* vector_p;
extern carItem_p currCarItemPtr;


DIST_T CarItemCoupledLength(carItem_p);
char* CarItemDescribe(carItem_p, long, long*);
void CarItemDraw(drawCmd_p, carItem_p, wDrawColor, int, BOOL_T, vector_p,
                 track_p);
void CarItemFindCouplerMountPoint(carItem_p, traverseTrack_t, coOrd[2]);
BOOL_T CarItemIsLoco(carItem_p);
void CarItemLoadList(void*);
char* CarItemNumber(carItem_p);
void CarItemPlace(carItem_p, traverseTrack_p, DIST_T*);
void CarItemPos(carItem_p, coOrd*);
BOOL_T CarItemRead(char*);
void CarItemSetNumber(carItem_p, char*);
void CarItemSetTrack(carItem_p, track_p);
void CarItemShelve(carItem_p);
void CarItemSize(carItem_p, coOrd*);
BOOL_T WriteCars(FILE*);
BOOL_T CarItemIsLocoMaster(carItem_p);
void CarItemSetLocoMaster(carItem_p, BOOL_T);


#include "ctrain.h"
#endif
