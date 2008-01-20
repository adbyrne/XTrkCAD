/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/bin/dpricels.c,v 1.2 2008-01-20 23:29:15 mni77 Exp $
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

#include "track.h"
#include "compound.h"
#include "i18n.h"

/*****************************************************************************
 *
 * Price List Dialog
 *
 */

static wWin_p priceListW;

static turnoutInfo_t * priceListCurrent;

static void PriceListOk( void * action );
static void PriceListUpdate();
DIST_T priceListCostV;
char priceListEntryV[STR_SIZE];
DIST_T priceListFlexLengthV;
DIST_T priceListFlexCostV;

static paramFloatRange_t priceListCostData = { 0.0, 9999.99, 80 };
static wPos_t priceListColumnWidths[] = { -60, 200 };
static const char * priceListColumnTitles[] = { N_("Price"), N_("Item") };
static paramListData_t priceListListData = { 10, 400, 2, priceListColumnWidths, priceListColumnTitles };
static paramFloatRange_t priceListFlexData = { 0.0, 999.99, 80 };
static paramData_t priceListPLs[] = {
#define I_PRICELSCOST			(0)
#define priceListCostF			((wFloat_p)priceListPLs[I_PRICELSCOST].control)
	{	PD_FLOAT, &priceListCostV, "cost", PDO_NOPREF|PDO_NOPSHUPD, &priceListCostData },
#define I_PRICELSENTRY			(1)
#define priceListEntryS			((wString_p)priceListPLs[I_PRICELSENTRY].control)
	{	PD_STRING, &priceListEntryV, "entry", PDO_NOPREF|PDO_NOPSHUPD|PDO_DLGHORZ, (void*)(400-80-3), NULL, BO_READONLY },
#define I_PRICELSLIST			(2)
#define priceListSelL			((wList_p)priceListPLs[I_PRICELSLIST].control)
	{	PD_LIST, NULL, "inx", PDO_NOPREF|PDO_NOPSHUPD, &priceListListData },
#define I_PRICELSFLEXLEN		(3)
	{	PD_FLOAT, &priceListFlexLengthV, "flexlen", PDO_NOPREF|PDO_NOPSHUPD|PDO_DIM|PDO_DLGRESETMARGIN, &priceListFlexData, N_("Flex Track") },
	{	PD_MESSAGE, N_("costs"), NULL, PDO_DLGHORZ },
#define I_PRICELSFLEXCOST		(6)
	{	PD_FLOAT, &priceListFlexCostV, "flexcost", PDO_NOPREF|PDO_NOPSHUPD|PDO_DLGHORZ, &priceListFlexData } };
static paramGroup_t priceListPG = { "pricelist", 0, priceListPLs, sizeof priceListPLs/sizeof priceListPLs[0] };


static void PriceListUpdate()
{
	DIST_T oldPrice;
	ParamLoadData( &priceListPG );
	if (priceListCurrent == NULL)
		return;
	FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO, priceListCurrent->title );
	wPrefGetFloat( "price list", message, &oldPrice, 0.0 );
	if (oldPrice == priceListCostV)
		return;
	wPrefSetFloat( "price list", message, priceListCostV );
	FormatCompoundTitle( listLabels|LABEL_COST, priceListCurrent->title );
	if (message[0] != '\0')
		wListSetValues( priceListSelL, wListGetIndex(priceListSelL), message, NULL, priceListCurrent );
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
	FLOAT_T price;
	PriceListUpdate();
	priceListCurrent = to;
	if (priceListCurrent == NULL)
		return;
	FormatCompoundTitle( LABEL_MANUF|LABEL_DESCR|LABEL_PARTNO, priceListCurrent->title );
	wPrefGetFloat( "price list", message, &price, 0.00 );
	priceListCostV = price;
	strcpy( priceListEntryV, message );
	ParamLoadControl( &priceListPG, I_PRICELSCOST );
	ParamLoadControl( &priceListPG, I_PRICELSENTRY );
}


static void PriceListChange( long changes )
{
	turnoutInfo_t * to1, * to2;
	if ((changes & (CHANGE_SCALE|CHANGE_PARAMS)) == 0 ||
		priceListW == NULL || !wWinIsVisible( priceListW ) ) 
		return;
	wListClear( priceListSelL );
	to1 = TurnoutAdd( listLabels|LABEL_COST, curScaleInx, priceListSelL, NULL, -1 );
	to2 = StructAdd( listLabels|LABEL_COST, curScaleInx, priceListSelL, NULL );
	if (to1 == NULL)
		to1 = to2;
	priceListCurrent = NULL;
	if (to1)
		PriceListSel( to1 );
	if ((changes & CHANGE_SCALE) == 0)
		return;
	sprintf( message, "price list %s", curScaleName );
	wPrefGetFloat( message, "flex length", &priceListFlexLengthV, 0.0 );
	wPrefGetFloat( message, "flex cost", &priceListFlexCostV, 0.0 );
	ParamLoadControls( &priceListPG );
}


static void PriceListDlgUpdate(
		paramGroup_p pg,
		int inx,
		void * valueP )
{
	turnoutInfo_t * to;
	switch( inx ) {
	case I_PRICELSCOST:
		PriceListUpdate();
		break;
	case I_PRICELSLIST:
		to = (turnoutInfo_t*)wListGetItemContext( (wList_p)pg->paramPtr[inx].control, (wIndex_t)*(long*)valueP );
		PriceListSel( to );
		break;
	}
}


static void DoPriceList( void * junk )
{
	if (priceListW == NULL)
		priceListW = ParamCreateDialog( &priceListPG, MakeWindowTitle(_("Price List")), _("Done"), PriceListOk, NULL, TRUE, NULL, 0, PriceListDlgUpdate );
	wShow( priceListW );
	PriceListChange( CHANGE_SCALE|CHANGE_PARAMS );
}


EXPORT addButtonCallBack_t PriceListInit( void )
{
	ParamRegister( &priceListPG );
	return &DoPriceList;
}
