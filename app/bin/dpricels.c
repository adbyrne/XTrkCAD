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

#include "compound.h"
#include "custom.h"
#include "dynstring.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "form.h"

static wControl_p priceListW;

static turnoutInfo_t * priceListCurrent;

static void PriceListOk( void * action );
static void PriceListUpdate();
DIST_T priceListCostV;
char priceListEntryV[STR_SIZE];
DIST_T priceListFlexLengthV;
DIST_T priceListFlexCostV;

static paramFloatRange_t priceListCostData = { 0.0, 9999.99, 80 };
static wWinPix_t priceListColumnWidths[] = { -60, 200 };
static const char * priceListColumnTitles[] = { N_("Price"), N_("Item") };
static paramListData_t priceListListData = { 10, 400, 2, priceListColumnWidths, priceListColumnTitles };
static paramFloatRange_t priceListFlexData = { 0.0, 999.99, 80 };
static paramData_t priceListPLs[] = {
#define I_PRICELSLIST			(0)
#define priceListSelL			(priceListPLs[I_PRICELSLIST].control)
	{	PD_LIST, NULL, "inx", PDO_DLGRESIZE|PDO_NOPREF|PDO_NOPSHUPD, &priceListListData },
#define I_PRICELSFLEXLEN		(1)
	{	PD_FLOAT, &priceListFlexLengthV, "flexlen", PDO_NOPREF|PDO_NOPSHUPD|PDO_DIM|PDO_DLGRESETMARGIN, &priceListFlexData, N_("Flex Track") },
	{	PD_MESSAGE, N_("costs"), "costs", PDO_DLGHORZ },
#define I_PRICELSFLEXCOST		(2)
	{	PD_FLOAT, &priceListFlexCostV, "flexcost", PDO_NOPREF|PDO_NOPSHUPD|PDO_DLGHORZ, &priceListFlexData }
};
static paramGroup_t priceListPG = { "pricelist", PGO_FULLDIALOGFROMBUILDER, priceListPLs, COUNT( priceListPLs ) };


static void PriceListUpdate()
{
	DIST_T oldPrice;
	ParamLoadData( &priceListPG );
	if (priceListCurrent == NULL) {
		return;
	}
	FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO,
	                     priceListCurrent->title );
	wPrefGetFloat( "price list", message, &oldPrice, 0.0 );
	if (oldPrice == priceListCostV) {
		return;
	}
	wPrefSetFloat( "price list", message, priceListCostV );
	FormatCompoundTitle( listLabels|LABEL_COST, priceListCurrent->title );
	if (message[0] != '\0') {
		wListSetValues( priceListSelL, wListGetIndex(priceListSelL), message, NULL,
		                priceListCurrent );
	}
}


static void PriceListOk( void * action )
{
	PriceListUpdate();
	sprintf( message, "price list %s", curScaleName );
	wPrefSetFloat( message, "flex length", priceListFlexLengthV );
	wPrefSetFloat( message, "flex cost", priceListFlexCostV );
	wHide( priceListW );
}


static void PriceListSel(
        turnoutInfo_t * to )
{
	//FLOAT_T price;
	//PriceListUpdate();
	//priceListCurrent = to;
	//if (priceListCurrent == NULL) {
	//	return;
	//}
	//FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO,
	//                     priceListCurrent->title );
	//wPrefGetFloat( "price list", message, &price, 0.00 );
	//priceListCostV = price;
	//strcpy( priceListEntryV, message );
	//ParamLoadControl( &priceListPG, I_PRICELSCOST );
	//ParamLoadControl( &priceListPG, I_PRICELSENTRY );
}


static void PriceListChange( long changes )
{
	DynString section;

	DynStringMalloc(&section, 32);

	turnoutInfo_t * to1, * to2;
	if ((changes & (CHANGE_SCALE|CHANGE_PARAMS)) == 0 ||
	    priceListW == NULL || !wWinIsVisible( priceListW ) ) {
		return;
	}
	wListClear( priceListSelL );
	to1 = TurnoutAdd( listLabels|LABEL_COST, GetLayoutCurScale(), priceListSelL,
	                  NULL, -1 );
	to2 = StructAdd( listLabels|LABEL_COST, GetLayoutCurScale(), priceListSelL,
	                 NULL );
	if (to1 == NULL) {
		to1 = to2;
	}
	priceListCurrent = NULL;
	if (to1) {
		PriceListSel( to1 );
	}
	if ((changes & CHANGE_SCALE) == 0) {
		return;
	}

	DynStringPrintf(&section, "price list %s", curScaleName);
	wPrefGetFloat( DynStringToCStr(&section), "flex length", &priceListFlexLengthV, 0.0 );
	wPrefGetFloat(DynStringToCStr(&section), "flex cost", &priceListFlexCostV, 0.0 );
	ParamLoadControls( &priceListPG );

	DynStringFree(&section);
}


static void PriceListDlgUpdate(
        paramGroup_p pg,
        int inx,
        void * valueP )
{
	turnoutInfo_t * to;
	switch( inx ) {

	case I_PRICELSLIST:
		to = (turnoutInfo_t*)wListGetItemContext( pg->paramPtr[inx].control,
		                (wIndex_t)*(long*)valueP );
		PriceListSel( to );
		break;
	}
}


static void DoPriceList( void * junk )
{
	if (priceListW == NULL) {
		priceListW = FormCreateDialog( &priceListPG, MakeWindowTitle(_("Price List")),
		                                _("Done"), PriceListOk, 
										NULL, NULL,
										TRUE, 
										F_RESIZE,
		                                PriceListDlgUpdate );

		wListSetColumnEditable(priceListSelL, "pricerenderer", priceListPLs + I_PRICELSLIST);

	}
	wShow( priceListW );
	PriceListChange( CHANGE_SCALE|CHANGE_PARAMS );
}


EXPORT addButtonCallBack_t PriceListInit( void )
{
	ParamRegister( &priceListPG );
	return &DoPriceList;
}
