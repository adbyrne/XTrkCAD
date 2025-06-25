#ifndef PARTS_H
#define PARTS_H

//#include <glib.h> // For G_TYPE_STRING, G_TYPE_INT, etc.

#include <stdbool.h>
#include <wlib.h>

// Structure to represent a single part's data
typedef struct {
    char* manufacturer;
    char* description;
    char* partno;
    bool is_invalid;
    char * price;
    int count;
} Part;

DataStore* PartListStoreNew(void* selectChange, const char* editRenderer,
    void (*edited)(int row, char* newvalue));
void PartListAddToListbox(wControl_p list, DataStore* store);
int PartListStoreAddPart(DataStore * store, const Part* part);
void PartListStoreGetPart(DataStore* store, unsigned index, Part* part);
//void PartListUpdatePart(DataStore* store, GtkTreeIter* iter, const Part* part);
//void PartListRemovePart(DataStore* store, GtkTreeIter* iter);
void PartListStoreClear(DataStore* store);

bool PartListUpdatePrice(DataStore* store, unsigned index, char * price, bool invalid);

#endif // !PARTS_H

