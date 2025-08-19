/** \file dcontmgm.c
 * Manage layout control elements
 */

/* -*- C -*- ****************************************************************
 *
 *  System        :
 *  Module        :
 *  Created By    : Robert Heller
 *  Created       : Thu Jan 5 10:52:12 2017
 *  Last Modified : <170411.1447>
 *
 *  Description
 *
 *     Control Element Mangment.  Control Elements are elements related to
 *     layout control: Blocks (occupency detection), Switchmotors (actuators
 *     to "throw" turnouts), and (eventually) signals.  These elements don't
 *     relate to "physical" items on the layout, but instead refer to the
 *     elements used by the layout control software.  These elements contain
 *     "scripts", which are really just textual items that provide information
 *     for the layout control software and provide a bridge between physical
 *     layout elements (like tracks or turnouts) and the layout control
 *     software.  These textual items could be actual software code or could
 *     be LCC Events (for I/O device elements on a LCC network) or DCC
 *     addresses for stationary decoders, etc.  XTrkCAD does not impose any
 *     sort of syntax or format -- that is left up to other software that might
 *     load and parse the XTrkCAD layout file.  All the XTrkCAD does is provide
 *     a unified place for this information to be stored and to provide a
 *     mapping (association) between this control information and the layout
 *     itself.
 *
 *
 *  Notes
 *
 *  History
 *
 ****************************************************************************
 *
 *    Copyright (C) 2017  Robert Heller D/B/A Deepwoods Software
 *			51 Locke Hill Road
 *			Wendell, MA 01379-9728
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *
 *
 ****************************************************************************/

//static const char rcsid[] = "@(#) : $Id$";

#include "cundo.h"
#include "custom.h"
#include "form.h"

/*****************************************************************************
 *
 * Control List Management
 *
 */

static void ControlEdit( void * action );
static void ControlDelete( void * action );
static void ControlDone( void * action );

static paramData_t controlPLs[] = {
#define I_CONTROLLIST	(0)
#define controlSelL		(controlPLs[I_CONTROLLIST].control)
	{	PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, NULL, NULL, BL_MANY},
#define I_CONTROLEDIT	(1)
	{	PD_BUTTON, ControlEdit, "edit", PDO_DLGCMDBUTTON, NULL, NULL },
#define I_CONTROLDEL		(2)
	{	PD_BUTTON, ControlDelete, "delete", 0, NULL, NULL },
} ;
static paramGroup_t controlPG = { "contmgm", PGO_FULLDIALOGFROMBUILDER, controlPLs, COUNT( controlPLs ) };


typedef struct {
	contMgmCallBack_p proc;
	void * data;
	wIcon_p icon;
} contMgmContext_t, *contMgmContext_p;

static BOOL_T AnyHILIGHT = FALSE;

static wBool_t ControlDlgUpdate(
        paramGroup_p pg,
        int inx,
        void *valueP )
{
	contMgmContext_p context = NULL;
	wIndex_t selcnt = wListGetSelectedCount( controlPLs[0].control );
	wIndex_t linx, lcnt;

	if ( inx != I_CONTROLLIST ) { return FALSE; }

	lcnt = wListGetCount( controlPLs[0].control );
	AnyHILIGHT = FALSE;
	for (linx=0; linx < lcnt; linx++ ) {
		context = (contMgmContext_p)wListGetItemContext(controlSelL, linx);

		if (wListGetItemSelected( controlPLs[0].control, linx ) == TRUE) {
			context->proc( CONTMGM_DO_HILIGHT, context->data );
			AnyHILIGHT = TRUE;
		} else {
			context->proc( CONTMGM_UN_HILIGHT, context->data );
		}
	}
	FormControlActive( &controlPG, I_CONTROLEDIT, selcnt>0 );
	FormControlActive( &controlPG, I_CONTROLDEL, selcnt>0 );

	return FALSE;
}

static void ControlEdit( void * action )
{
	contMgmContext_p context = NULL;
	wIndex_t selcnt = wListGetSelectedCount(controlPLs[0].control );
	wIndex_t inx, cnt;

	if ( selcnt != 1 ) {
		return;
	}
	cnt = wListGetCount(controlPLs[0].control );
	for ( inx=0;
	      inx<cnt && wListGetItemSelected(controlPLs[0].control, inx ) != TRUE;
	      inx++ );
	if ( inx >= cnt ) {
		return;
	}
	context = (contMgmContext_p)wListGetItemContext( controlSelL, inx );
	if ( context == NULL ) {
		return;
	}
	context->proc( CONTMGM_DO_EDIT, context->data );
	context->proc( CONTMGM_GET_TITLE, context->data );
	wListSetValues( controlSelL, inx, message, context->icon, context );
}


static void ControlDelete( void * action )
{
	wIndex_t selcnt = wListGetSelectedCount(controlPLs[0].control );
	wIndex_t inx, cnt;
	contMgmContext_p context = NULL;
	int deleted = 0;

	if ( selcnt <= 0 ) {
		return;
	}

	if ( (!NoticeMessage2( 1,
	                       _("Are you sure you want to delete the %d control element(s)"), _("Yes"),
	                       _("No"), selcnt ) ) ) {
		return;
	}
	cnt = wListGetCount(controlPLs[0].control );
	UndoStart( _("Control Elements"), "delete" );

	inx = 0;
	while( deleted < selcnt && inx < cnt) {
		if ( wListGetItemSelected(controlPLs[0].control, inx ) ) {
			context = (contMgmContext_p)wListGetItemContext(controlSelL, inx);
			context->proc( CONTMGM_DO_DELETE, context->data );
			MyFree(context);
			deleted++;
		}

		inx++;
	}

	wListDeleteSelected(controlSelL);

	UndoEnd();

	DoChangeNotification( CHANGE_PARAMS );
}

static void ControlDone( void * action )
{
	contMgmContext_p context = NULL;
	wIndex_t linx, lcnt;

	if (AnyHILIGHT) {
		lcnt = wListGetCount(controlPLs[0].control );
		for (linx=0;
		     linx < lcnt;
		     linx++ ) {
			context = (contMgmContext_p)wListGetItemContext( controlSelL, linx );
			context->proc( CONTMGM_UN_HILIGHT, context->data );
		}
	}
	wHide( controlPG.win );
}


EXPORT void ContMgmLoad(
        wIcon_p icon,
        contMgmCallBack_p proc,
        void * data )
{
	contMgmContext_p context;
	context = MyMalloc( sizeof *context );
	context->proc = proc;
	context->data = data;
	context->icon = icon;
	context->proc( CONTMGM_GET_TITLE, context->data );
	wListAddValue( controlSelL, message, icon, context );
}


static void LoadControlMgmList( void )
{
	wIndex_t curInx, cnt=0;
	long tempL;
	contMgmContext_p context;

	curInx = wListGetIndex( controlSelL );

	cnt = wListGetCount( controlSelL );
	for ( curInx=0; curInx<cnt; curInx++ ) {
		context = (contMgmContext_p)wListGetItemContext( controlSelL, curInx );
		if ( context ) {
			MyFree( context );
		}
	}
	curInx = wListGetIndex( controlSelL );
	wControlShow( (wControl_p)controlSelL, FALSE );
	wListClear( controlSelL );

	BlockMgmLoad();
	SwitchmotorMgmLoad();
	SignalMgmLoad();
	ControlMgmLoad();
	SensorMgmLoad();

	tempL = -1;
	ControlDlgUpdate( &controlPG, I_CONTROLLIST, &tempL );
	wControlShow( (wControl_p)controlSelL, TRUE );
}


static void ContMgmChange( long changes )
{
	if (changes) {
		if (changed) {
			changed = checkPtMark = 1;
		}
	}
	if ((changes&CHANGE_PARAMS) == 0 ||
	    controlPG.win == NULL || !wWinIsVisible(controlPG.win) ) {
		return;
	}
}



static void DoControlMgr( void * junk )
{
	if (controlPG.win == NULL) {
		FormCreateDialog( &controlPG,
		                  MakeWindowTitle(_("Manage Layout Control Elements")),
		                  NULL, ControlDone,
		                  NULL, ParamCancel_Current,
		                  TRUE, F_RESIZE|F_RECALLSIZE|F_BLOCK,
		                  ControlDlgUpdate );
	} else {
		wListClear( controlSelL );
	}
	FormLoadControls( &controlPG );
	FormGroupRecord( &controlPG );

	LoadControlMgmList();
	wShow( controlPG.win );
}


EXPORT addButtonCallBack_t ControlMgrInit( void )
{
	FormRegister( &controlPG );

	RegisterChangeNotification( ContMgmChange );
	return &DoControlMgr;
}
