#ifndef HAVE_LISTELEM_H
#define HAVE_LISTELEM_H
#include "common.h"

extern int lookupListIndex;

void* LookupListElem(dynArr_t* da, void* key, int(*cmpFunc)(void*, void*),
                     int elem_size);

void RemoveListElem(dynArr_t* da, void* elem);
#endif
