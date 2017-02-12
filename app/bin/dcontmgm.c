/** \file dcontmgm.c
 * Manage layout control elements
 */

/* -*- C -*- ****************************************************************
 *
 *  System        : 
 *  Module        : 
 *  Created By    : Robert Heller
 *  Created       : Thu Jan 5 10:52:12 2017
 *  Last Modified : <170123.1206>
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
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * 
 *
 ****************************************************************************/

static const char rcsid[] = "@(#) : $Id$";



#include "track.h"
#include <errno.h>
#include "i18n.h"

#ifdef WINDOWS
#include <io.h>
#define F_OK	(0)
#define W_OK	(2)
#define access	_access
#endif

/*****************************************************************************
 *
 * Control List Management
 *
 */

static void ControlEdit( void * action );
static void ControlDelete( void * action );
static void ControlDone( void * action );
static wPos_t controlListWidths[] = { 18, 100, 150 };
static const char * controlListTitles[] = { "", N_("Name"),
	N_("Tracks") };
static paramListData_t controlListData = { 10, 400, 3, controlListWidths, controlListTitles };
static paramData_t controlPLs[] = {
#define I_CONTROLLIST	(0)
#define controlSelL		((wList_p)controlPLs[I_CONTROLLIST].control)
	{	PD_LIST, NULL, "inx", PDO_DLGRESETMARGIN|PDO_DLGRESIZE, &controlListData, NULL, BL_MANY },
#define I_CONTROLEDIT	(1)
	{	PD_BUTTON, (void*)ControlEdit, "edit", PDO_DLGCMDBUTTON, NULL, N_("Edit") },
#define I_CONTROLDEL		(2)
    {	PD_BUTTON, (void*)ControlDelete, "delete", 0, NULL, N_("Delete") },
  } ;
static paramGroup_t controlPG = { "contmgm", 0, controlPLs, sizeof controlPLs/sizeof controlPLs[0] };


typedef struct {
		contMgmCallBack_p proc;
		void * data;
		wIcon_p icon;
		} contMgmContext_t, *contMgmContext_p;

static void ControlDlgUpdate(
		paramGroup_p pg,
		int inx,
		void *valueP )
{
    contMgmContext_p context = NULL;
    wIndex_t selcnt = wListGetSelectedCount( (wList_p)controlPLs[0].control );
    wIndex_t linx, lcnt;

    if ( inx != I_CONTROLLIST ) return;
    ParamControlActive( &controlPG, I_CONTROLEDIT, selcnt>0 );
    ParamControlActive( &controlPG, I_CONTROLDEL, selcnt>0 );
}

static void ControlEdit( void * action )
{
	contMgmContext_p context = NULL;
	wIndex_t selcnt = wListGetSelectedCount( (wList_p)controlPLs[0].control );
	wIndex_t inx, cnt;

	if ( selcnt != 1 )
		return;
	cnt = wListGetCount( (wList_p)controlPLs[0].control );
	for ( inx=0;
		  inx<cnt && wListGetItemSelected( (wList_p)controlPLs[0].control, inx ) != TRUE;
		  inx++ );
	if ( inx >= cnt )
		return;
	context = (contMgmContext_p)wListGetItemContext( controlSelL, inx );
	if ( context == NULL )
		return;
	context->proc( CONTMGM_DO_EDIT, context->data );
#ifdef OBSOLETE
	context->proc( CONTMGM_GET_TITLE, context->data );
	wListSetValues( controlSelL, inx, message, context->icon, context );
#endif
}


static void ControlDelete( void * action )
{
	wIndex_t selcnt = wListGetSelectedCount( (wList_p)controlPLs[0].control );
	wIndex_t inx, cnt;
	contMgmContext_p context = NULL;

	if ( selcnt <= 0 )
		return;
	if ( (!NoticeMessage2( 1, _("Are you sure you want to delete the %d control element(s)"), _("Yes"), _("No"), selcnt ) ) )
		return;
        cnt = wListGetCount( (wList_p)controlPLs[0].control );
        UndoStart( _("Control Elements"), "delete" );
	for ( inx=0; inx<cnt; inx++ ) {
		if ( !wListGetItemSelected( (wList_p)controlPLs[0].control, inx ) )
			continue;
		context = (contMgmContext_p)wListGetItemContext( controlSelL, inx );
		context->proc( CONTMGM_DO_DELETE, context->data );
		MyFree( context );
		wListDelete( controlSelL, inx );
		inx--;
		cnt--;
	}
        UndoEnd();
	DoChangeNotification( CHANGE_PARAMS );
}

static void ControlDone( void * action )
{
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
	contMgmContext_t curContext;

	curInx = wListGetIndex( controlSelL );
	curContext.proc = NULL;
	curContext.data = NULL;
	curContext.icon = NULL;
	if ( curInx >= 0 ) {
		context = (contMgmContext_p)wListGetItemContext( controlSelL, curInx );
		if ( context != NULL )
			curContext = *context;
	}
	cnt = wListGetCount( controlSelL );
	for ( curInx=0; curInx<cnt; curInx++ ) {
		context = (contMgmContext_p)wListGetItemContext( controlSelL, curInx );
		if ( context )
			MyFree( context );
	}
	curInx = wListGetIndex( controlSelL );
	wControlShow( (wControl_p)controlSelL, FALSE );
	wListClear( controlSelL );

	BlockMgmLoad();
	SwitchmotorMgmLoad();

#ifdef LATER
	curInx = 0;
	cnt = wListGetCount( controlSelL );
	if ( curContext.proc != NULL ) {
		for ( curInx=0; curInx<cnt; curInx++ ) {
			context = (contMgmContext_p)wListGetItemContext( controlSelL, curInx );
			if ( context &&
				 context->proc == curContext.proc &&
				 context->data == curContext.data )
				break;
		}
	}
	if ( curInx >= cnt )
		curInx = (cnt>0?0:-1);

	wListSetIndex( controlSelL, curInx );
	tempL = curInx;
#endif
	tempL = -1;
	ControlDlgUpdate( &controlPG, I_CONTROLLIST, &tempL );
	wControlShow( (wControl_p)controlSelL, TRUE );
}


static void ContMgmChange( long changes )
{
	if (changes) {
		if (changed) {
			changed = 1;
			checkPtMark = 1;
		}
	}
	if ((changes&CHANGE_PARAMS) == 0 ||
		controlPG.win == NULL || !wWinIsVisible(controlPG.win) )
		return;

	LoadControlMgmList();
}



static void DoControlMgr( void * junk )
{
    if (controlPG.win == NULL) {
        ParamCreateDialog( &controlPG, MakeWindowTitle(_("Manage Layout Control Elements")), _("Done"), ControlDone, NULL, TRUE, NULL, F_RESIZE|F_RECALLSIZE|F_BLOCK, ControlDlgUpdate );
    } else {
        wListClear( controlSelL );
    }
    /*ParamLoadControls( &controlPG );*/
    /*ParamGroupRecord( &controlPG );*/
    LoadControlMgmList();
    wShow( controlPG.win );
}


EXPORT addButtonCallBack_t ControlMgrInit( void )
{
    ParamRegister( &controlPG );
    /*ParamRegister( &contMgmContentsPG );*/
    RegisterChangeNotification( ContMgmChange );
    return &DoControlMgr;
}
