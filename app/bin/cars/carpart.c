/**
 * \file   carpart.c
 * \brief  Manage purchasable car parts
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
#include "fileio.h"
#include "include/paramfile.h"

#include "carsprivate.h"
#include "listelem.h"
#include "tabstring.h"

static int Cmp_partparent(void* key, void* elem);
static carPartParent_p CarPartParentNew(char* manufP, int manufL, char* protoP,
                                        int protoL, SCALEINX_T scale);
static void CarPartParentDelete(carPartParent_p parentP);
static void CarPartUnlink(carPart_p partP);

static int log_carPart;

dynArr_t carPartParent_da;

dynArr_t roadnameMap_da;
#define roadnameMap(N) DYNARR_N(roadnameMap_p, roadnameMap_da, N)
BOOL_T roadnameMapChanged;

typedef struct {
	tabString_t manuf;
	tabString_t proto;
	SCALEINX_T scale;
} cmp_partparent_t;


int Cmp_part(
        void* key,
        void* elem)
{
	carPart_p cmp_key = key;
	carPart_p part_elem = elem;
	int rc;
	int len;

	len = min(cmp_key->partnoL, part_elem->partnoL);
	rc = strncasecmp(cmp_key->partnoP, part_elem->partnoP, len + 1);
	if (rc != 0) {
		return rc;
	}
	if (cmp_key->paramFileIndex == part_elem->paramFileIndex) {
		return 0;
	}
	if (cmp_key->paramFileIndex == PARAM_DEMO) {
		return -1;
	}
	if (part_elem->paramFileIndex == PARAM_DEMO) {
		return 1;
	}
	if (cmp_key->paramFileIndex == PARAM_CUSTOM) {
		return -1;
	}
	if (part_elem->paramFileIndex == PARAM_CUSTOM) {
		return 1;
	}
	if (cmp_key->paramFileIndex == PARAM_LAYOUT) {
		return 1;
	}
	if (part_elem->paramFileIndex == PARAM_LAYOUT) {
		return -1;
	}
	if (cmp_key->paramFileIndex > part_elem->paramFileIndex) {
		return -1;
	} else {
		return 1;
	}
}

static int Cmp_partparent(
        void* key,
        void* elem)
{
	cmp_partparent_t* cmp_key = key;
	carPartParent_p part_elem = elem;
	int rc;

	rc = -TabStringCmp(part_elem->manuf, &cmp_key->manuf);
	if (rc != 0) {
		return rc;
	}
	rc = cmp_key->scale - part_elem->scale;
	if (rc != 0) {
		return rc;
	}
	rc = -TabStringCmp(part_elem->proto, &cmp_key->proto);
	return rc;
}


int Cmp_roadnameMap(void* key,void* elem)
{
	cmp_key_t* cmp_key = key;
	roadnameMap_p roadname_elem = elem;
	int rc;

	rc = strncasecmp(cmp_key->name, roadname_elem->roadname, cmp_key->len);
	if (rc == 0 && roadname_elem->roadname[cmp_key->len]) {
		return -1;
	}
	return rc;
}

roadnameMap_p LoadRoadnameList(
        tabString_p roadnameTab,
        tabString_p repmarkTab)
{
	cmp_key_t cmp_key;
	roadnameMap_p roadnameMapP;

	lookupListIndex = -1;
	if (roadnameTab->len == 0) {
		return NULL;
	}
	if (TabStringCmp("undecorated", roadnameTab) == 0) {
		return NULL;
	}

	cmp_key.name = roadnameTab->ptr;
	cmp_key.len = roadnameTab->len;
	roadnameMapP = LookupListElem(&roadnameMap_da, &cmp_key, Cmp_roadnameMap,
	                              sizeof(roadnameMap_t));
	if (roadnameMapP->roadname == NULL) {
		roadnameMapP->roadname = TabStringDup(roadnameTab);
		roadnameMapP->repmark = TabStringDup(repmarkTab);
		roadnameMapChanged = TRUE;
	} else if (repmarkTab->len > 0 &&
	           (roadnameMapP->repmark == NULL || roadnameMapP->repmark[0] == '\0')) {
		roadnameMapP->repmark = TabStringDup(repmarkTab);
		roadnameMapChanged = TRUE;
	}
	return roadnameMapP;
}

static carPartParent_p CarPartParentNew(
        char* manufP,
        int manufL,
        char* protoP,
        int protoL,
        SCALEINX_T scale)
{
	carPartParent_p parentP;
	cmp_partparent_t cmp_key;
	cmp_key.manuf.ptr = manufP;
	cmp_key.manuf.len = manufL;
	cmp_key.proto.ptr = protoP;
	cmp_key.proto.len = protoL;
	cmp_key.scale = scale;
	parentP = (carPartParent_p)LookupListElem(&carPartParent_da, &cmp_key,
	                Cmp_partparent, sizeof * parentP);
	if (parentP->manuf == NULL) {
		parentP->manuf = (char*)MyMalloc(manufL + 1);
		memcpy(parentP->manuf, manufP, manufL);
		parentP->manuf[manufL] = '\0';
		parentP->proto = (char*)MyMalloc(protoL + 1);
		memcpy(parentP->proto, protoP, protoL);
		parentP->proto[protoL] = '\0';
		parentP->scale = scale;
	}
	return parentP;
}

static void CarPartParentDelete(
        carPartParent_p parentP)
{
	RemoveListElem(&carPartParent_da, parentP);
	MyFree(parentP->manuf);
	MyFree(parentP->proto);
	MyFree(parentP);
}

static void CarPartUnlink(
        carPart_p partP)
{
	carPartParent_p parentP = partP->parent;
	RemoveListElem(&parentP->parts_da, partP);
	if (parentP->parts_da.cnt <= 0) {
		CarPartParentDelete(parentP);
	}
}

carPart_p CarPartFind(
        char* manufP,
        int manufL,
        char* partnoP,
        int partnoL,
        SCALEINX_T scale)
{
	for (wIndex_t inx1 = 0; inx1 < carPartParent_da.cnt; inx1++) {
		carPartParent_p parentP;
		parentP = carPartParent(inx1);
		if (manufL == (int)strlen(parentP->manuf) &&
		    strncasecmp(manufP, parentP->manuf, manufL) == 0 &&
		    scale == parentP->scale) {
			for (wIndex_t inx2 = 0; inx2 < parentP->parts_da.cnt; inx2++) {
				carPart_p partP;
				partP = carPart(parentP, inx2);
				if (partnoL == partP->partnoL &&
				    strncasecmp(partnoP, partP->partnoP, partnoL) == 0) {
					return partP;
				}
			}
		}
	}
	return NULL;
}

carPart_p CarPartNew(
        carPart_p partP,
        int paramFileIndex,
        SCALEINX_T scaleInx,
        char* title,
        long options,
        long type,
        const carDim_p dim,
        wDrawColor color)
{
	carPartParent_p parentP;
	carPart_t cmp_key;
	tabString_t tabs[7];

	TabStringExtract(title, 7, tabs);
	if (TabStringCmp("Undecorated", &tabs[T_MANUF]) == 0 ||
	    TabStringCmp("Custom", &tabs[T_MANUF]) == 0 ||
	    tabs[T_PART].len == 0) {
		return NULL;
	}
	if (tabs[T_PROTO].len == 0) {
		return NULL;
	}
	if (partP == NULL) {
		partP = CarPartFind(tabs[T_MANUF].ptr, tabs[T_MANUF].len, tabs[T_PART].ptr,
		                    tabs[T_PART].len, scaleInx);
		if (partP != NULL &&
		    partP->paramFileIndex == PARAM_CUSTOM &&
		    paramFileIndex != PARAM_CUSTOM) {
			return partP;
		}
		LOG(log_carPart, 2, ("new car part: %s (%d) at %d\n", title, paramFileIndex,
		                     lookupListIndex))
	}
	if (partP != NULL) {
		CarPartUnlink(partP);
		if (partP->title != NULL) {
			MyFree(partP->title);
		}
		LOG(log_carPart, 2, ("upd car part: %s (%d)\n", title, paramFileIndex))
	}
	LoadRoadnameList(&tabs[T_ROADNAME], &tabs[T_REPMARK]);
	parentP = CarPartParentNew(tabs[T_MANUF].ptr, tabs[T_MANUF].len,
	                           tabs[T_PROTO].ptr, tabs[T_PROTO].len, scaleInx);
	cmp_key.title = title;
	cmp_key.parent = parentP;
	cmp_key.paramFileIndex = paramFileIndex;
	cmp_key.options = options;
	cmp_key.type = type;
	cmp_key.dim = *dim;
	cmp_key.color = color;
	cmp_key.partnoP = tabs[T_PART].ptr;
	cmp_key.partnoL = tabs[T_PART].len;
	partP = (carPart_p)LookupListElem(&parentP->parts_da, &cmp_key, Cmp_part,
	                                  sizeof * partP);
	if (partP->title != NULL) {
		MyFree(partP->title);
	}
	*partP = cmp_key;
	sprintf(message, "\t\t%s", tabs[2].ptr);
	partP->title = MyStrdup(message);
	partP->partnoP = partP->title + 2 + tabs[2].len + 1;;
	partP->partnoL = tabs[T_PART].len;
	return partP;
}

static void CarPartDelete(carPart_p partP)
{
	if (partP == NULL) {
		return;
	}
	CarPartUnlink(partP);
	if (partP->title) {
		MyFree(partP->title);
	}
	MyFree(partP);
}

/**
* Delete all car part definitions that came from a specific parameter file.
* CarParts are stored in DYNARR for the specific car model. These DYNARRs
* are linked from CarPartParents, again DYNARRs. Thes parents are created
* from part definition and only contain manufacturer and type information.
*
* \param [IN] fileIndex parameter file
*/

void DeleteCarPart(int fileIndex)
{
	int inxParent = 0;

	while (inxParent < carPartParent_da.cnt) {
		carPartParent_p parentP = carPartParent(inxParent);

		// Iterate backwards to avoid index shifting when deleting
		for (int inx = parentP->parts_da.cnt - 1; inx >= 0; inx--) {
			carPart_p part = carPart(parentP, inx);
			if (part->paramFileIndex == fileIndex) {
				CarPartDelete(part);
			}
		}

		// Check if parent still exists (might have been deleted)
		if (inxParent < carPartParent_da.cnt &&
		    carPartParent(inxParent) == parentP) {
			inxParent++;
		}
		// If parent was deleted, don't increment (next parent shifted down)
	}
}


static BOOL_T CarPartRead(char* line)
{
	char scale[10];
	long options;
	long type;
	char* title;
	carDim_t dim;
	long rgb;
	long longCenterOffset;

	if (!GetArgs(line + 8, "sqllff0lffl",
	             scale, &title, &options, &type, &dim.carLength, &dim.carWidth,
	             &longCenterOffset, &dim.truckCenter, &dim.coupledLength, &rgb)) {
		return FALSE;
	}
	dim.truckCenterOffset = longCenterOffset / 1000.0;
	CarPartNew(NULL, curParamFileIndex, LookupScale(scale), title, options, type,
	           &dim, wDrawFindColor(rgb));
	MyFree(title);
	return TRUE;
}


BOOL_T CarPartWrite(
        FILE* f,
        carPart_p partP)
{
	BOOL_T rc = TRUE;
	const carPartParent_p parentP = partP->parent;
	tabString_t tabs[7];
	long longCenterOffset = (long)(partP->dim.truckCenterOffset * 1000);

	SetCLocale();

	TabStringExtract(partP->title, 7, tabs);
	sprintf(message, "%s\t%s\t%.*s\t%.*s\t%.*s\t%.*s\t%.*s",
	        parentP->manuf, parentP->proto,
	        tabs[T_DESC].len, tabs[T_DESC].ptr,
	        tabs[T_PART].len, tabs[T_PART].ptr,
	        tabs[T_ROADNAME].len, tabs[T_ROADNAME].ptr,
	        tabs[T_REPMARK].len, tabs[T_REPMARK].ptr,
	        tabs[T_NUMBER].len, tabs[T_NUMBER].ptr);
	rc &= fprintf(f, "CARPART %s \"%s\"", GetScaleName(partP->parent->scale),
	              PutTitle(message)) > 0;
	rc &= fprintf(f, " %ld %ld %0.3f %0.3f 0 %ld %0.3f %0.3f %ld\n",
	              partP->options, partP->type, partP->dim.carLength, partP->dim.carWidth,
	              longCenterOffset,
	              partP->dim.truckCenter, partP->dim.coupledLength,
	              wDrawGetRGB(partP->color)) > 0;

	SetUserLocale();

	return rc;
}

BOOL_T CarDescCustomSave(	FILE* f)
{
	int parentX;
	int partX;
	carPart_p partP;
	BOOL_T rc = TRUE;

	for (parentX = 0; parentX < carPartParent_da.cnt; parentX++) {
		carPartParent_p parentP;
		parentP = carPartParent(parentX);
		for (partX = 0; partX < parentP->parts_da.cnt; partX++) {
			partP = carPart(parentP, partX);
			if (partP->paramFileIndex == PARAM_CUSTOM) {
				rc &= CarPartWrite(f, partP);
			}
		}
	}
	return rc;
}

/**
 * Check the whether the parameter file has CARPARTS that are a fit or compatible
 * with the current state.
 *
 * \param paramFileIndex IN the parameter file
 * \param scaleIndex IN the scale to check against
 * \return the compatibility state of the the
 */
enum paramFileState
GetCarPartCompatibility(int paramFileIndex, SCALEINX_T scaleIndex) {
	enum paramFileState ret = PARAMFILE_NOTUSABLE;

	if (!IsParamValid(paramFileIndex))
	{
		return(PARAMFILE_UNLOADED);
	}

	for (int i = 0; i < carPartParent_da.cnt && ret != PARAMFILE_FIT; i++)
	{
		carPartParent_t* carPartParent = carPartParent(i);
		SCALE_FIT_T fit = CompatibleScale(FIT_CAR, carPartParent->scale, scaleIndex);
		if (fit == FIT_EXACT) {
			for (int j = 0; j < carPartParent->parts_da.cnt; j++) {
				const carPart_t* carPart = carPart(carPartParent, j);
				if (carPart->paramFileIndex == paramFileIndex) {
					ret = PARAMFILE_FIT;
					break;
				}
			}
		}
		if (fit == FIT_COMPATIBLE) {
			ret = PARAMFILE_COMPATIBLE;
		}
	}
	return(ret);
}

BOOL_T CheckAvail(
        carPartParent_p parentP)
{
	for (wIndex_t inx = 0; inx < parentP->parts_da.cnt; inx++) {
		carPart_p partP;
		partP = carPart(parentP, inx);
		if (IsParamValid(partP->paramFileIndex)) {
			return TRUE;
		}
	}
	return FALSE;
}

int CarPartCustMgmProc(int cmd, void * data )
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
		if ( partP == NULL ) {
			return FALSE;
		}
		carDlgUpdatePartPtr = partP;
		CarDlgUpdPart( );
		return TRUE;
	case CUSTMGM_CAN_DELETE:
		return TRUE;
	case CUSTMGM_DO_DELETE:
		CarPartDelete( partP );
		return TRUE;
	case CUSTMGM_GET_TITLE:
		TabStringExtract( partP->title, 7, tabs );
		rd_inx = T_REPMARK;
		if ( tabs[T_REPMARK].len == 0 ) {
			rd_inx = T_ROADNAME;
		}
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

void
InitCarPart(void)
{
	AddParam( "CARPART ", CarPartRead);
	log_carPart = LogFindIndex("carPart");
}
