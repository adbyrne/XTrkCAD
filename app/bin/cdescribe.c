/** \file cdescribe.c
 * Handling of the 'Describe' dialog
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

#include "common.h"
#include "cundo.h"
#include "fileio.h"
#include "icons.h"
#include "cselect.h"
#include "track.h"
#include "common-ui.h"
#include "draw.h"
#include "note.h"
#include "form.h"

/** @logcmd @showrefby `describe=n` `cdescribe.c` */
static int log_describe = 0;

static paramGroup_t * describePG;
EXPORT wIndex_t describeCmdInx;
EXPORT BOOL_T inDescribeCmd;
static track_p descTrk;
/** Track currently under the cursor while the Describe command is active
 * (set on every wActionMove, not just C_DOWN) -- see ClearDescribe(). */
static track_p trk;

static descData_p descData;
static descUpdate_t descUpdateFunc;
static coOrd descOrig, descSize;
static POS_T descBorder;
static wDrawColor descColor = 0;
EXPORT BOOL_T descUndoStarted;
static BOOL_T descNeedDrawHilite;
EXPORT char * descTitle = "<>";
static char descTitleBuf[STR_SIZE];	/* backing store for descTitle */

static wMenu_p descPopupM;

/**
 * The Layer dropdown does not list every layer, so its index is not a layer
 * number: descLayerMap[] maps each dropdown index to its layer. The dropdown
 * edits descLayerInx, which DescribeApplyLayer() applies on Done (the control
 * is PDO_NOPSHACT, see GTK3 Issue 14). The track type's layer field is left
 * holding a real layer number.
 */
static unsigned int descLayerMap[NUM_LAYERS];
static int descLayerCnt;		/**< number of entries in descLayerMap */
static wIndex_t descLayerInx;		/**< dropdown index */

static paramFloatRange_t rdata = { 0, 0, 10, PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW };
static paramIntegerRange_t idata = { 0, 0, 10, PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW };
static paramTextData_t tdata = { 30, 15 };
static char * pivotLabels[] = { N_("First"), N_("Middle"), N_("End"), NULL };
static char * boxLabels[] = { "", NULL };


/**
 * Fill descLayerMap with the layers the dropdown offers: the track's own
 * layer only if it is frozen (read-only), otherwise every layer that is not
 * frozen.
 *
 * \param layer IN the track's layer
 * \param ro IN TRUE if the track's layer is frozen
 * \return the dropdown index of \a layer
 */
static wIndex_t BuildDescLayerMap(unsigned int layer, BOOL_T ro)
{
	wIndex_t inx = 0;

	descLayerCnt = 0;
	for (unsigned int l = 0; l < NUM_LAYERS; l++) {
		if (ro ? (l != layer) : GetLayerFrozen(l)) {
			continue;
		}
		if (l == layer) {
			inx = descLayerCnt;
		}
		descLayerMap[descLayerCnt++] = l;
	}
	return inx;
}

static void DrawDescHilite(BOOL_T selected)
{
	if (descNeedDrawHilite == FALSE) {
		return;
	}

	if (descColor==0) {
		descColor = wDrawColorGray(87);
	}
	DrawRectangle(&tempD, descOrig, descSize, selected?descColor:wDrawColorBlue,
	              DRAW_TRANSPARENT);
}

/**
 * Set the highlight rectangle to the track's bounding box plus descBorder.
 *
 * \param trk IN track to highlight
 */
static void SetDescHiliteBox(track_p trk)
{
	coOrd hi, lo;

	GetBoundingBox(trk, &hi, &lo);
	descOrig.x = lo.x - descBorder;
	descOrig.y = lo.y - descBorder;
	descSize.x = hi.x - lo.x + 2*descBorder;
	descSize.y = hi.y - lo.y + 2*descBorder;
}



static void DescribeUpdate(
        paramGroup_p pg,
        int inx,
        void * data)
{
	descData_p ddp;

	if (inx < 0) {
		return;
	}

	ddp = (descData_p)pg->paramPtr[inx].context;

	if ((ddp->mode&(DESC_RO|DESC_IGNORE)) != 0) {
		/* For POS3D: allow Z (control2) edit even when XY is RO.
		 * DESC_POS3D: untested, see track.h */
		if (ddp->type != DESC_POS3D || pg->paramPtr[inx].control != ddp->control2
		    || (ddp->mode & DESC_Z_IGNORE)) {
			return;
		}
	}

	if (ddp->type == DESC_PIVOT) {
		return;
	}

	LOG( log_describe, 3, ( "DescribeUpdate( %s %d:%s\n",
	                        pg->nameStr, inx, (inx >= 0)?(ddp->label): ""));

	if (!descTrk) {
		// A late focus-out after Done: must not open an undo that
		// nothing would close.
		return;
	}

	if (!descUndoStarted) {
		UndoStart( descTitle, "Change Track" );
		descUndoStarted = TRUE;
	}

	UndoModify(descTrk);
	descUpdateFunc(descTrk, (int)(ddp-descData), descData, FALSE);

	if (descTrk) {
		if ((ddp->mode&DESC_NOREDRAW) == 0) {
			SetDescHiliteBox(descTrk);
		}

		if (OFF_D(mapD.orig, mapD.size, descOrig, descSize)) {
			ErrorMessage(MSG_MOVE_OUT_OF_BOUNDS);
		}
	}


	for (inx = 0; inx < pg->paramCnt; inx++) {
		if ((pg->paramPtr[inx].option & PDO_DLGIGNORE) != 0) {
			continue;
		}

		ddp = (descData_p)pg->paramPtr[inx].context;

		if ((ddp->mode&DESC_IGNORE) != 0) {
			continue;
		}

		if ((ddp->mode & (DESC_CHANGE|DESC_CHANGE2|DESC_CHANGE3)) == 0) {
			continue;
		}

		wControlActive( ddp->control0,
		                (ddp->mode&DESC_RO)?FALSE:TRUE);
		ddp->mode &= ~DESC_CHANGE;
		if (ddp->type == DESC_POS || ddp->type == DESC_POS3D) {
			if (ddp->mode & DESC_CHANGE3) {
				ddp->mode &= ~DESC_CHANGE3;		//Third time (POS3D only)
			} else if (ddp->mode & DESC_CHANGE2) {
				ddp->mode &= ~DESC_CHANGE2;		//Second time
				if (ddp->type == DESC_POS3D) {
					ddp->mode |= DESC_CHANGE3;        //Queue third
				}
			} else {
				ddp->mode |= DESC_CHANGE2;		//First time
			}
		}

		FormLoadSingleControl(describePG, inx);
	}
}


/**
 * Move the described track to the layer picked in the dropdown, if that
 * differs from its current layer, as part of the Describe undo.
 */
static void DescribeApplyLayer(void)
{
	for (descData_p ddp = descData; ddp->type != DESC_NULL; ddp++) {
		if (ddp->type != DESC_LAYER) {
			continue;
		}
		if ((ddp->mode & (DESC_RO|DESC_IGNORE)) != 0
		    || descLayerInx < 0 || descLayerInx >= descLayerCnt) {
			return;
		}
		unsigned int layer = descLayerMap[descLayerInx];
		if (layer == GetTrkLayer(descTrk)) {
			return;
		}
		if (!descUndoStarted) {
			UndoStart( descTitle, "Change Track" );
			descUndoStarted = TRUE;
		}
		UndoModify(descTrk);
		SetTrkLayer(descTrk, layer);
		return;
	}
}


EXPORT void DescribeDone(void * junk)
{
	if (descTrk && describePG && describePG->win
	    && wWinIsVisible(describePG->win)) {
		// Dialog buttons act on button-press, before GTK moves the
		// focus. A field still being edited would then only be
		// committed by a focus-out after descTrk is cleared, and its
		// value lost. Moving the focus now commits it while descTrk
		// is still valid.
		wControlSetFocus((wControl_p)describePG->cancelB);
	}

	if (descTrk) {
		CHECK(!IsTrackDeleted(descTrk));
		DescribeApplyLayer();
		// TODO_CANCEL last arg could be true
		if ( descUpdateFunc && descTrk && GetTrkType(descTrk) != T_NOTE ) {
			descUpdateFunc(descTrk, -1, descData, !descUndoStarted);
		}
		descTrk = NULL;
	}

	if (describePG && describePG->win && wWinIsVisible(describePG->win)) {
		wHide(describePG->win);
	}
	if (descUndoStarted) {
		UndoEnd();
		descUndoStarted = FALSE;
	}
	descNeedDrawHilite = FALSE;
	describePG = NULL;
}

/**
 * Reset the Describe command's cursor-tracking state (SF #785). \c trk is
 * set on every mouse move while Describe is the active command (not just on
 * a click) and is otherwise never cleared, so it can be left pointing at a
 * track from whatever file was previously loaded -- if a new file is then
 * loaded while Describe is still the active command, a redraw dispatched
 * before the user moves the mouse again (\c C_REDRAW's \c else \c if
 * (trk) branch) dereferences that stale pointer into memory \c
 * ClearTracks() already freed. Confirmed via gdb (both a captured core
 * dump and a live attach on the same repro: Describe a track by hovering
 * over it, Save As, then File > Open a different file) that this, not \c
 * descTrk (already reset correctly elsewhere), is the dangling pointer.
 * Called from ClearTracks(), matching ClearNote()'s existing precedent for
 * per-module state a new file load can invalidate.
 */
EXPORT void ClearDescribe(void)
{
	trk = NULL;
}


static dynArr_t	descGroup_da;
#define descGroup(N) DYNARR_N( paramGroup_p, descGroup_da, N)
static dynArr_t pd_da;

static paramData_p CreateDescribeField(
        parameterType type,
        paramGroup_p pg,
        descData_p ddp )
{
	DYNARR_APPEND( paramData_t, pd_da, 10 );
	paramData_p pd = &DYNARR_LAST( paramData_t, pd_da );
	memset( pd, 0, sizeof *pd );
	pd->type = type;
	char sIndex[12];
	snprintf( sIndex, sizeof sIndex, "%d", pd_da.cnt );
	pd->nameStr = MyStrdup( sIndex );
	pd->valueP = ddp->valueP;
	pd->winLabel = ddp->label;
	pd->group = pg;
	pd->option |= PDO_NOPREF;
	char * sType;
	switch( type ) {
	case PD_FLOAT:
		pd->winData = &rdata;
		sType = "Float";
		break;
	case PD_LONG:
		pd->winData = &idata;
		sType = "Long";
		break;
	case PD_TEXT:
		sType = "Text";
		pd->winData = &tdata;
		break;
	case PD_STRING:
		pd->winData = I2VP(30);
		sType = "String";
		break;
	default:
		sType = "Other";
		break;
	}
	LOG( log_describe, 2, ( "    %s %s \"%s\"\n", sIndex, sType, pd->winLabel ) );
	return pd;
}

static paramGroup_p CreateDescribeDialog(
        char * sTitle,
        descData_p data)
{
	// Create a param group
	DYNARR_APPEND( paramGroup_p, descGroup_da, 10)
	paramGroup_p pg = (paramGroup_p)MyMalloc( sizeof *pg );
	DYNARR_LAST( paramGroup_p, descGroup_da ) = pg;
	memset( pg, 0, sizeof *pg );
	pg->nameStr = sTitle;
	LOG( log_describe, 2, ( "CreateDescribeDialog %s\n", sTitle ) );
	DYNARR_RESET( paramData_t, pd_da );
	for (descData_p ddp=data; ddp->type != DESC_NULL; ddp++) {
		paramData_p pdp = NULL;
		switch( ddp->type ) {
		case DESC_POS:
			pdp = CreateDescribeField( PD_FLOAT, pg, ddp );
			pdp->option |= PDO_SAMEROW | PDO_NEWSAMEROW;
			pdp->context = (void*)ddp;
			pdp = CreateDescribeField( PD_FLOAT, pg, ddp );
			pdp->winLabel = NULL;
			pdp->option |= PDO_SAMEROW;
			// Point to 2nd double of a coOrd
			pdp->valueP = (char *)pdp[-1].valueP + offsetof( coOrd, y );
			break;
		case DESC_POS3D:	/* untested, see track.h */
			pdp = CreateDescribeField( PD_FLOAT, pg, ddp );
			pdp->option |= PDO_SAMEROW | PDO_NEWSAMEROW;
			pdp->context = (void*)ddp;
			pdp = CreateDescribeField( PD_FLOAT, pg, ddp );
			pdp->winLabel = NULL;
			pdp->option |= PDO_SAMEROW;
			pdp->context = (void*)ddp;
			pdp->valueP = (char *)pdp[-1].valueP + offsetof( coOrd, y );
			pdp = CreateDescribeField( PD_FLOAT, pg, ddp );
			pdp->winLabel = NULL;
			pdp->option |=  PDO_SAMEROW | PDO_DIM;
			pdp->valueP = ddp->valueP2;
			break;
		case DESC_FLOAT:
		case DESC_DIM:
		case DESC_ANGLE:
			pdp = CreateDescribeField( PD_FLOAT, pg, ddp );
			if ( ddp->type == DESC_DIM ) {
				pdp->option |= PDO_DIM;
			} else if ( ddp->type == DESC_ANGLE ) {
				pdp->option |= PDO_ANGLE;
			}
			break;
		case DESC_LONG:
			pdp = CreateDescribeField( PD_LONG, pg, ddp );
			break;
		case DESC_COLOR:
			pdp = CreateDescribeField( PD_COLORLIST, pg, ddp );
			break;
		case DESC_PIVOT:
			pdp = CreateDescribeField( PD_RADIO, pg, ddp );
			pdp->winData = pivotLabels;
			pdp->winLabel =  N_("Lock");
			pdp->winOption |= BC_HORIZONTAL|BC_NOBORDER;
			break;
		case DESC_LAYER:
			pdp = CreateDescribeField( PD_COMBOLIST, pg, ddp );
			pdp->option |= PDO_LISTINDEX|PDO_NOPSHACT;
			pdp->valueP = &descLayerInx;
			break;
		case DESC_STRING:
			pdp = CreateDescribeField( PD_STRING, pg, ddp );
			pdp->max_string = ddp->max_string/10+1;
			break;
		case DESC_TEXT:
			pdp = CreateDescribeField( PD_TEXT, pg, ddp );
			pdp->option |= PDO_DLGNOLABELALIGN;
			// TODO compute max_string
			pdp->max_string = 100;
			break;
		case DESC_LIST:
			pdp = CreateDescribeField( PD_COMBOLIST, pg, ddp );
			pdp->option |= PDO_LISTINDEX;
			break;
		case DESC_EDITABLELIST:
			pdp = CreateDescribeField( PD_COMBOLIST, pg, ddp );
			pdp->winOption |= BL_EDITABLE;
			break;
		case DESC_BOXED:
			pdp = CreateDescribeField( PD_TOGGLE, pg, ddp );
			pdp->winData = boxLabels;
			pdp->winOption |= BC_HORIZONTAL|BC_NOBORDER;
			break;
		default:
			break;
		}
		CHECK( pdp );
		// cppcheck-suppress nullPointerRedundantCheck
		// Every descType case above sets pdp via CreateDescribeField(), which
		// always returns non-NULL; the switch is exhaustive over descType, so
		// pdp can never be NULL here despite the CHECK above.
		pdp->context = (void*)ddp;
	}
	pg->paramPtr = memdup( pd_da.ptr, pd_da.cnt * sizeof *pg->paramPtr );
	pg->paramCnt = pd_da.cnt;
	return pg;
}


void DoDescribe(char * title, track_p track, descData_p data,
                descUpdate_t update)
{

	if (!inDescribeCmd) {
		return;
	}

	// The caller passes an untranslated type name (N_("Straight Track"),
	// ...). Register and look the dialog group up by a stable,
	// locale-independent key (sKey); sTitle is the translated string, used
	// only for what the user sees. Keying the lookup on the translated
	// title instead left the group registered as e.g. "Describe
	// Gleisgerade", so the English "PARAMETER Describe Straight Track ..."
	// lines in the bundled demo scripts never matched under a non-English
	// locale and the edits were silently dropped.
	char sKey[STR_SIZE];
	char sTitle[STR_SIZE];
	snprintf( sKey, sizeof sKey, "Describe %s", title );
	snprintf( sTitle, sizeof sTitle, _("Describe %s"), _(title) );
	descTitle = strcpy( descTitleBuf, sTitle );
	paramGroup_p pg = NULL;
	for ( int inx = 0; inx < descGroup_da.cnt; inx++ ) {
		pg = descGroup( inx );
		if ( strcmp( sKey, pg->nameStr ) == 0 ) {
			break;
		}
		pg = NULL;
	}

	if ( pg == NULL ) {
		// No: Create a new dialog for it
		pg = CreateDescribeDialog( MyStrdup( sKey ), data );
		FormCreateDialog( pg, sTitle,
		                  //_("Done"), DescribeDone,
		                  NULL, NULL,
		                  _("Done"), FormCancel_Reset,
		                  TRUE,
		                  F_RECALLPOS|PD_F_ALT_CANCELLABEL,
		                  DescribeUpdate);

		// Copy control[01] to desc
		paramData_p pdp = pg->paramPtr;
		int ddx = 0;
		int pdx = 0;
		int inx = 0;
		for (descData_p ddp=data; ddp->type != DESC_NULL; ddp++, ddx++ ) {
			LOG( log_describe, 3, ( "%d.0: %d<-%d %p \n", inx++, ddx, pdx, pdp->control ) );
			ddp->control0 = pdp->control;
			pdp++;
			pdx++;
			if ( ddp->type == DESC_POS || ddp->type == DESC_POS3D ) {
				LOG( log_describe, 3, ( " .1: %d<-%d %p\n", ddx, pdx, pdp->control ) );
				ddp->control1 = pdp->control;
				pdp++;
				pdx++;
			}
			if ( ddp->type == DESC_POS3D ) {
				LOG( log_describe, 3, ( " .2: %d<-%d %p\n", ddx, pdx, pdp->control ) );
				ddp->control2 = pdp->control;
				pdp++;
				pdx++;
			}
		}
		FormRegister(pg);
	}

	if ( describePG ) {
		// We are done with old dialog
		LOG( log_describe, 2, ( "Desc Done\n" ) );
	}

	descTrk = track;
	descData = data;
	descUpdateFunc = update;


	int inx;
	descData_p ddp;
	int ro_mode;
	ro_mode = (GetLayerFrozen(GetTrkLayer(track))?DESC_RO:0);

	if (ro_mode) {
		LOG( log_describe, 3, ( "DoDescribe-RO-layer: %s\n", sKey ) );
		for (ddp=data; ddp->type != DESC_NULL; ddp++) {
			if (ddp->mode&DESC_IGNORE) {
				continue;
			}

			ddp->mode |= DESC_RO;
		}
	}

	for (ddp=data; ddp->type != DESC_NULL; ddp++) {
		if (ddp->mode&DESC_IGNORE) {
			wControlShow( ddp->control0, FALSE );
			if ( ddp->type == DESC_POS || ddp->type == DESC_POS3D ) {
				wControlShow( ddp->control1, FALSE );
			}
			if ( ddp->type == DESC_POS3D ) {
				wControlShow( ddp->control2, FALSE );
			}
			LOG( log_describe, 3, ( "Dodescribe-IGNORE-pd: %s.%s\n", sKey, ddp->label ) );
			continue;
		}

		if (ddp->type != DESC_LAYER) {
			wControlActive(ddp->control0,
			               (ddp->mode&DESC_RO)?FALSE:TRUE);
		}

		wControlShow( ddp->control0, TRUE );
		switch (ddp->type) {
		case DESC_POS:
			wControlShow( ddp->control1, TRUE );
			wControlActive(ddp->control1,
			               (ddp->mode&DESC_RO)?FALSE:TRUE);
			break;
		case DESC_POS3D:	/* untested, see track.h */
			wControlShow( ddp->control1, TRUE );
			wControlActive(ddp->control1,
			               (ddp->mode&DESC_RO)?FALSE:TRUE);
			wControlShow( ddp->control2, (ddp->mode&DESC_Z_IGNORE)?FALSE:TRUE );
			wControlActive(ddp->control2,
			               (ddp->mode&DESC_Z_RO)?FALSE:TRUE);
			break;

		case DESC_LAYER:
			wListClear((wList_p)ddp->control0);  // Rebuild list on each invocation
			descLayerInx = BuildDescLayerMap(*(unsigned int *)ddp->valueP,
			                                 ro_mode);
			for (inx = 0; inx < descLayerCnt; inx++) {
				char *layerFormattedName;
				layerFormattedName = FormatLayerName(descLayerMap[inx]);
				wComboBoxAddValue((wList_p)ddp->control0, layerFormattedName,
				                  I2VP(inx));
				free(layerFormattedName);
			}
			wControlActive(ddp->control0, !ro_mode);
			break;

		default:
			break;
		}
	}

	describePG = pg;
	FormLoadControls(describePG);
	sprintf(message, "%s (T%d)", sTitle, GetTrkIndex(track));
	wWinSetTitle(describePG->win, message);
	wShow(describePG->win);
}

static void DescChange(long changes)
{
	if ((changes&CHANGE_UNITS) && describePG && describePG->win
	    && wWinIsVisible(describePG->win)) {
		FormLoadControls(describePG);
	}
}

/*****************************************************************************
 *
 * SIMPLE DESCRIPTION
 *
 */




EXPORT STATUS_T CmdDescribe(wAction_t action, coOrd pos)
{
	char msg[STR_SIZE];

	switch (action) {
	case C_START:
		InfoMessage(_("Click on object for Properties +Shift for Frozen"));
		wSetCursor(mainD.d,wCursorQuestion);
		descUndoStarted = FALSE;
		trk = NULL;
		descTrk = NULL;
		return C_CONTINUE;

	case wActionMove:
		trk = OnTrack(&pos, FALSE, FALSE);
		if (trk && GetLayerFrozen(GetTrkLayer(trk))
		    && !(MyGetKeyState() & WKEY_SHIFT)) {
			trk = NULL;
			return C_CONTINUE;
		}
		return C_CONTINUE;


	case C_DOWN:
		trk = OnTrack(&pos, FALSE, FALSE);
		if (trk == NULL) {
			// Not a track - ignore
			return C_CONTINUE;
		}
#ifdef TODO_CANCEL
		if ( trk == descTrk ) {
			// Same track - ignore
			return C_CONTINUE;
		}
#endif
		InfoMessage( "" );
		DescribeDone( NULL );
		if ( trk == NULL ) {
			// This should not happen.
			// Somebody is stomping on trk
			// Unreproducible.
			printf( "CmdDescribe: trk is NULL!\n" );
			return C_CONTINUE;
		}
		if (GetLayerFrozen(GetTrkLayer(trk)) && !(MyGetKeyState()& WKEY_SHIFT)) {
			InfoMessage(_("Track is Frozen, Add Shift to Describe"));
			trk = NULL;
			return C_CONTINUE;
		}
		descBorder = mainD.scale*0.1;

		if (descBorder < trackGauge) {
			descBorder = trackGauge;
		}

		inDescribeCmd = TRUE;
		SetDescHiliteBox(trk);
		descNeedDrawHilite = TRUE;
		DescribeTrack(trk, msg, 255);
		inDescribeCmd = FALSE;
		InfoMessage(msg);
		// Ugly code: but Describe Notes do not continue like other objects
		if ( GetTrkType( trk ) != T_NOTE ) {
			descTrk = trk;
		} else {
			descTrk = NULL;
		}
		trk = NULL;
		return C_CONTINUE;

	case C_REDRAW:

		if (describePG && describePG->win && wWinIsVisible(describePG->win)
		    && descTrk) {
			descNeedDrawHilite = TRUE;
			SetDescHiliteBox(descTrk);

			DrawDescHilite(TRUE);

			if (descTrk && QueryTrack(descTrk, Q_IS_DRAW)) {
				DrawOriginAnchor(descTrk);
			}
		} else if (trk) {
			DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
		}


		break;

	case C_CANCEL:
		DescribeDone( NULL );
		wSetCursor(mainD.d,defaultCursor);
		return C_CONTINUE;

	case C_CMDMENU:
		menuPos = pos;
		if (!trk) { wMenuPopupShow(descPopupM); }
		return C_CONTINUE;

	case C_FINISH:
		return C_CONTINUE;
	default:
		LOG( log_describe, 1, ( "unexpected action %d in CmdDescribe\n", action ) )
		break;
	}


	return C_CONTINUE;
}

void InitCmdDescribe(wMenu_p menu)
{
	describeCmdInx = AddMenuButton(menu, CmdDescribe, "cmdDescribe",
	                               _("Properties"), CreateToolbarIconFromResource("describe.png"),
	                               LEVEL0, IC_CANCEL|IC_POPUP|IC_WANT_MOVE|IC_CMDMENU|IC_TOGGLE, ACCL_DESCRIBE,
	                               NULL);
	RegisterChangeNotification(DescChange);
	log_describe = LogFindIndex( "describe" );
}
void InitCmdDescribe2(wMenu_p menu)
{
	descPopupM = MenuRegister( "Properties Context Menu" );
	wMenuPushCreate(descPopupM, "cmdSelectMode", GetBalloonHelpStr("cmdSelectMode"),
	                0, DoCommandB, I2VP(selectCmdInx));
	wMenuPushCreate(descPopupM, "cmdModifyMode", GetBalloonHelpStr("cmdModifyMode"),
	                0, DoCommandB, I2VP(modifyCmdInx));
	wMenuPushCreate(descPopupM, "cmdPanMode", GetBalloonHelpStr("cmdPanMode"), 0,
	                DoCommandB, I2VP(panCmdInx));

}
