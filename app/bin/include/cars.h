#ifndef HAVE_CARS_H
#define HAVE_CARS_H

typedef struct carProto_s* carProto_p;

struct carItem_s;

typedef struct carItem_s* carItem_p;
extern carItem_p currCarItemPtr;
void CarProtoDelete(carProto_p protoP);
void DeleteCarProto(int fileIndex);
enum paramFileState	GetCarProtoCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);


typedef struct carPart_s* carPart_p;


void DeleteCarPart(int fileIndex);
enum paramFileState	GetCarPartCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);

void CarItemGetSegs(carItem_p item);

#endif
