/** \file dpricels.c
 * Price List Dialog
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdbool.h>

#include "compound.h"
#include "layout.h"
#include "form.h"
#include <parts.h>
#include <dynstring.h>
#include "paramfile.h"

#define PRICELIST_SECTION "price list"
#define PRICECOLUMN_RENDERER "pricerenderer"
#define FLEXTRACKLENGTH "flex length"
#define FLEXTRACKCOST "flex cost"

static void PriceEdited(int row, char* newvalue);
static void PriceListOk(void* action);
static void PriceListSave(void);

static wControl_p priceListW;
static DataStore* liststore;
static unsigned partListRows;
static unsigned errorRows;

static double priceListFlexLengthV;
static double priceListFlexCostV;

static paramFloatRange_t priceListCostData = { 0.0, 9999.99, 80 };
static paramFloatRange_t priceListFlexData = { 0.0, 999.99, 80 };

static paramData_t priceListPLs[] = {
#define I_PRICELSLIST			(0)
#define priceListSelL			(priceListPLs[I_PRICELSLIST].control)
	{	PD_LIST, NULL, "inx", PDO_DLGRESIZE|PDO_NOPREF|PDO_NOPSHUPD, NULL, NULL, BL_NODATASTORE},
#define I_PRICELSFLEXLEN		(1)
	{	PD_FLOAT, &priceListFlexLengthV, "flexlen", PDO_NOPREF/*|PDO_NOPSHUPD*/|PDO_DIM, &priceListFlexData, NULL },
#define I_PRICELSFLEXCOST		(2)
	{	PD_FLOAT, &priceListFlexCostV, "flexcost", PDO_NOPREF/*|PDO_NOPSHUPD*/, &priceListFlexData }
};
static paramGroup_t priceListPG = { "pricelist", PGO_FULLDIALOGFROMBUILDER, priceListPLs, COUNT( priceListPLs ) };


/**
 * Format price to two digits after the decimal separator. The returned string has to be MyFree'd
 * by the caller.
 */
static char* FormatPrice(DynString *out, double price)
{
	DynStringPrintf(out, "%.2f", price);

	return(DynStringToCStr(out));
}

/**
 * Convert string to double with error checking.
 *
 * \param input		input string
 * \param output	converted double or 0.0 on error
 * \return			true on success
 */

static bool ConvertToDouble(const char* input, double* output)
{
	char* end;
	double result = 0.0;
	errno = 0;

	result = strtod(input, &end);

	if ((result == 0.0 && (errno != 0 || end == input)) ||
	    *end != '\0') {
		return false;
	} else {
		*output = result;
		return(true);
	}
}

/**
 * Handler for edited price.
 *
 * \param row
 * \param newvalue
 */

static void
PriceEdited(int row, char* newvalue)
{
	double price = 0.0;
	bool check;

	check = ConvertToDouble(newvalue, &price);
	if (check) {
		if (price < priceListCostData.low || price >priceListCostData.high) {
			check = false;
		}
	}

	if (check) {
		DynString format;

		DynStringMalloc(&format, 16);
		bool oldInvalid;

		char* formattedPrice = FormatPrice(&format, price);

		oldInvalid = PartListUpdatePrice(liststore, row, formattedPrice, false);

		if (oldInvalid) {
			errorRows--;
			FormDialogOkActive(&priceListPG, errorRows == 0);
		}

		DynStringFree(&format);
	} else {
		PartListUpdatePrice(liststore, row, newvalue, true);
		errorRows++;
		FormDialogOkActive(&priceListPG, false);
	}
}

/**
 * Format the key for storing the price in the ini file. manufacturer and part no are optional
 */

static void
FormatKey(DynString* keyPtr, Part* part)
{
	if (part->manufacturer[0]) {

		DynStringPrintf(keyPtr, "%s %s %s",
		                part->manufacturer,
		                part->partno,
		                part->description);
	} else {
		DynStringPrintf(keyPtr, "%s",
		                part->description);
	}
}

/**
 * Save entered price to configuration using part informtion as key.
 */

static void
SavePrice(Part* this)
{
	double price;

	ConvertToDouble(this->price, &price);
	if (price != 0.0) {
		DynString key;
		DynStringMalloc(&key, 80);

		FormatKey(&key, this);

		wPrefSetFloat(PRICELIST_SECTION,
		              DynStringToCStr(&key),
		              price);

		DynStringFree(&key);
	}
}

/**
 * Iterate over parts list and save prices.
 *
 */

static void PriceListSave(void)
{
	for (unsigned int i = 0; i < partListRows; i++) {
		Part thisPart;

		PartListStoreGetPart(liststore, i, &thisPart);
		SavePrice(&thisPart);
	}
}

static void
FlexSaveCost(const char* scale)
{
	DynString section;

	DynStringMalloc(&section, 80);
	DynStringPrintf(&section, "price list %s", curScaleName);

	wPrefSetFloat(DynStringToCStr(&section),
	              FLEXTRACKLENGTH, priceListFlexLengthV);
	wPrefSetFloat(DynStringToCStr(&section),
	              FLEXTRACKCOST, priceListFlexCostV);

	DynStringFree(&section);
}


static void PriceListOk( void * action )
{
	PriceListSave();
	FlexSaveCost(curScaleName);

	wHide( priceListW );
}

static void
ReadPrice(Part *this, double *price)
{
	DynString key;
	DynStringMalloc(&key, 80);

	FormatKey(&key, this);
	wPrefGetFloat(PRICELIST_SECTION,
	              DynStringToCStr(&key),
	              price,
	              0.0);

	DynStringFree(&key);
}


/**
 * Format part data and add to the list.
 *
 * \param title
 */

static void
AddPartToList(char *title)
{
	Part *thisPart = MyMalloc(sizeof(Part));
	char* copyOfTitle = MyStrdup(title);
	char* nextToken = NULL;

	double price;
	DynString formattedPrice;
	DynStringMalloc(&formattedPrice, 16);

	// if first token is empty, no manufacturer wasspecified, start tokenizing with next word
	if (*copyOfTitle == '\t') {
		// no manufacturer
		thisPart->manufacturer = "";
		nextToken = copyOfTitle;
	} else {
		// manufacturer specified
		thisPart->manufacturer = strtok(copyOfTitle, "\t");
	}
	thisPart->description = strtok(nextToken, "\t");
	thisPart->partno = strtok(NULL, "\t");
	thisPart->is_invalid = false;

	ReadPrice(thisPart, &price);
	FormatPrice(&formattedPrice, price);

	thisPart->price = DynStringToCStr(&formattedPrice);

	partListRows = PartListStoreAddPart(liststore, thisPart);

	DynStringFree(&formattedPrice);
	MyFree(copyOfTitle);
	MyFree(thisPart);
}

static void
TurnoutList(SCALEINX_T scale)
{
	for (int inx = 0; inx < turnoutInfo_da.cnt; inx++) {
		turnoutInfo_t* to;

		to = turnoutInfo(inx);
		if (IsParamValid(to->paramFileIndex) &&
		    to->segCnt > 0 &&
		    (FIT_NONE != CompatibleScale(FIT_TURNOUT, to->scaleInx, scale))) {
			AddPartToList(to->title);
		}
	}
}

static void
StructureList(SCALEINX_T scale )
{
	for (int inx = 0; inx < structureInfo_da.cnt; inx++) {
		turnoutInfo_t* to;

		to = structureInfo(inx);
		if (IsParamValid(to->paramFileIndex) &&
		    to->segCnt > 0 &&
		    (FIT_NONE != CompatibleScale(FIT_STRUCTURE, to->scaleInx, scale)) &&
		    to->segCnt != 0) {

			AddPartToList(to->title);
		}
	}
}

static void
FlexGetCost(const char* scale)
{
	DynString section;

	DynStringMalloc(&section, 80);
	DynStringPrintf(&section, "price list %s", curScaleName);

	wPrefGetFloat(DynStringToCStr(&section),
	              FLEXTRACKLENGTH, &priceListFlexLengthV, 0.0);
	wPrefGetFloat(DynStringToCStr(&section),
	              FLEXTRACKCOST, &priceListFlexCostV, 0.0);

	FormLoadControls(&priceListPG);

	DynStringFree(&section);
}

static void
PriceListChange( long changes )
{
	if ((changes & (CHANGE_SCALE|CHANGE_PARAMS)) == 0 ||
	    priceListW == NULL || !wWinIsVisible( priceListW ) ) {
		return;
	}

	PartListStoreClear(liststore);

	TurnoutList(GetLayoutCurScale());
	StructureList(GetLayoutCurScale());

	if ((changes & CHANGE_SCALE) == 0) {
		return;
	}

	FlexGetCost(curScaleName);

}


static wBool_t
PriceListDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	paramData_p field = priceListPLs+inx;
	bool valid=true;

	switch( inx ) {
	case I_PRICELSFLEXCOST:
		valid = FormFloatRangeCheck(field, priceListFlexCostV);
		break;
	case I_PRICELSFLEXLEN:
		valid = FormFloatRangeCheck(field, priceListFlexLengthV);
		break;
	default:
		break;
	}

	FormDialogOkActive(pg, valid);

	return(valid);
}


static void
DoPriceList( void * junk )
{
	if (priceListW == NULL) {
		priceListW = FormCreateDialog( &priceListPG, NULL,
		                               _("Done"), PriceListOk,
		                               _("Cancel"), FormCancel_Current,
		                               TRUE, F_RESIZE, PriceListDlgUpdate );

		liststore = PartListStoreNew(NULL, PRICECOLUMN_RENDERER, PriceEdited);
		// connect list to liststore
		PartListAddToListbox(priceListPLs[I_PRICELSLIST].control, liststore);
	}

	PriceListChange( CHANGE_SCALE|CHANGE_PARAMS );

	wShow(priceListW);
}


EXPORT addButtonCallBack_t PriceListInit( void )
{
	FormRegister( &priceListPG );
	return &DoPriceList;
}
