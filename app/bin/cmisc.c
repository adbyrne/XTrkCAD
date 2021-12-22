/** \file cmisc.c
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "common.h"
#include "cundo.h"
#include "param.h"
#include "fileio.h"
#include "cselect.h"
#include "track.h"
#include "common-ui.h"

EXPORT wIndex_t describeCmdInx;
EXPORT BOOL_T inDescribeCmd;

extern wIndex_t selectCmdInx;
extern wIndex_t joinCmdInx;
extern wIndex_t modifyCmdInx;

static track_p descTrk;
static descData_p descData;
static descUpdate_t descUpdateFunc;
static coOrd descOrig, descSize;
static POS_T descBorder;
static wDrawColor descColor = 0;
static BOOL_T descUndoStarted;
static BOOL_T descNeedDrawHilite;
static wWinPix_t describeW_posy;
static int describe_row;
static wWinPix_t describeCmdButtonEnd;

static wMenu_p descPopupM;

static unsigned int editableLayerList[NUM_LAYERS];		/**< list of non-frozen layers */
static int * layerValue;								/**pointer to current Layer (int *) */

static paramFloatRange_t rdata = { 0, 0, 100, PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW };
static paramIntegerRange_t idata = { 0, 0, 100, PDO_NORANGECHECK_HIGH|PDO_NORANGECHECK_LOW };
static paramTextData_t tdata = { 300, 150 };
static char * pivotLabels[] = { N_("First"), N_("Middle"), N_("End"), NULL };
static char * boxLabels[] = { "", NULL };
static paramData_t describePLs[] = {
#define I_FLOAT_0		(0)
    { PD_FLOAT, NULL, "F1", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F2", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F3", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F4", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F5", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F6", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F7", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F8", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F9", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F10", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F11", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F12", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F13", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F14", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F15", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F16", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F17", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F18", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F19", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F20", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F21", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F22", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F23", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F24", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F25", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F26", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F27", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F28", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F29", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F30", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F31", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F32", PDO_NOPREF, &rdata },
    { PD_FLOAT, NULL, "F33", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F34", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F35", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F36", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F37", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F38", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F39", PDO_NOPREF, &rdata },
	{ PD_FLOAT, NULL, "F40", PDO_NOPREF, &rdata },
#define I_FLOAT_N		I_FLOAT_0+40

#define I_LONG_0		I_FLOAT_N
    { PD_LONG, NULL, "I1", PDO_NOPREF, &idata },
    { PD_LONG, NULL, "I2", PDO_NOPREF, &idata },
    { PD_LONG, NULL, "I3", PDO_NOPREF, &idata },
    { PD_LONG, NULL, "I4", PDO_NOPREF, &idata },
    { PD_LONG, NULL, "I5", PDO_NOPREF, &idata },
#define I_LONG_N		I_LONG_0+5

#define I_STRING_0		I_LONG_N
    { PD_STRING, NULL, "S1", PDO_NOPREF, I2VP(300) },
    { PD_STRING, NULL, "S2", PDO_NOPREF, I2VP(300) },
    { PD_STRING, NULL, "S3", PDO_NOPREF, I2VP(300) },
    { PD_STRING, NULL, "S4", PDO_NOPREF, I2VP(300) },
#define I_STRING_N		I_STRING_0+4

#define I_LAYER_0		I_STRING_N
    { PD_DROPLIST, NULL, "Y1", PDO_NOPREF, I2VP(150), NULL, 0 },
#define I_LAYER_N		I_LAYER_0+1

#define I_COLOR_0		I_LAYER_N
    { PD_COLORLIST, NULL, "C1", PDO_NOPREF, NULL, N_("Color"), BC_HORZ|BC_NOBORDER },
#define I_COLOR_N		I_COLOR_0+1

#define I_LIST_0		I_COLOR_N
    { PD_DROPLIST, NULL, "L1", PDO_NOPREF, I2VP(150), NULL, 0 },
    { PD_DROPLIST, NULL, "L2", PDO_NOPREF, I2VP(150), NULL, 0 },
	{ PD_DROPLIST, NULL, "L3", PDO_NOPREF, I2VP(150), NULL, 0 },
	{ PD_DROPLIST, NULL, "L4", PDO_NOPREF, I2VP(150), NULL, 0 },
#define I_LIST_N		I_LIST_0+4

#define I_EDITLIST_0	I_LIST_N
    { PD_DROPLIST, NULL, "LE1", PDO_NOPREF, I2VP(150), NULL, BL_EDITABLE },
#define I_EDITLIST_N	I_EDITLIST_0+1

#define I_TEXT_0		I_EDITLIST_N
    { PD_TEXT, NULL, "T1", PDO_NOPREF, &tdata, NULL, BT_HSCROLL },
#define I_TEXT_N		I_TEXT_0+1

#define I_PIVOT_0		I_TEXT_N
    { PD_RADIO, NULL, "P1", PDO_NOPREF, pivotLabels, N_("Lock"), BC_HORZ|BC_NOBORDER, 0 },
#define I_PIVOT_N		I_PIVOT_0+1

#define I_TOGGLE_0      I_PIVOT_N
    { PD_TOGGLE, NULL, "boxed1", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
	{ PD_TOGGLE, NULL, "boxed2", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
	{ PD_TOGGLE, NULL, "boxed3", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
	{ PD_TOGGLE, NULL, "boxed4", PDO_NOPREF|PDO_DLGHORZ, boxLabels, N_("Boxed"), BC_HORZ|BC_NOBORDER },
#define I_TOGGLE_N 		I_TOGGLE_0+4
};

static paramGroup_t describePG = { "describe", 0, describePLs, COUNT( describePLs ) };

/**
 * A mapping table is used to map the index in the dropdown list to the layer
 * number. While usually this is a 1:1 translation, the values differ if there
 * are frozen layer. Frozen layers are not shown in the dropdown list.
 */

void
CreateEditableLayersList()
{
    int i = 0;
    int j = 0;

    while (i < NUM_LAYERS) {
        if (!GetLayerFrozen(i)) {
            editableLayerList[j++] = i;
        }

        i++;
    }
}

/**
 * Search a layer in the list of editable layers.
 *
 * \param IN layer layer to search
 * \return the index into the list
 */

static int
SearchEditableLayerList(unsigned int layer)
{
    int i;

    for (i = 0; i < NUM_LAYERS; i++) {
        if (editableLayerList[i] == layer) {
            return (i);
        }
    }

    return (-1);
}

static void DrawDescHilite(BOOL_T selected)
{
    if (descNeedDrawHilite == FALSE) {
        return;
    }

    if (descColor==0) {
        descColor = wDrawColorGray(87);
    }
    DrawRectangle(&tempD, descOrig, descSize, selected?descColor:wDrawColorBlue, DRAW_TRANSPARENT);
}



static void DescribeUpdate(
    paramGroup_p pg,
    int inx,
    void * data)
{
    coOrd hi, lo;
    descData_p ddp;

    if (inx < 0) {
        return;
    }

    ddp = (descData_p)pg->paramPtr[inx].context;

    if ((ddp->mode&(DESC_RO|DESC_IGNORE)) != 0) {
        return;
    }

    if (ddp->type == DESC_PIVOT) {
        return;
    }

    if (!descUndoStarted) {
        UndoStart(_("Change Track"), "Change Track");
        descUndoStarted = TRUE;
    }

    if (!descTrk) {
        return;    // In case timer pops after OK
    }

    UndoModify(descTrk);
    descUpdateFunc(descTrk, (int)(ddp-descData), descData, FALSE);

    if (descTrk) {
        GetBoundingBox(descTrk, &hi, &lo);
        if ((ddp->mode&DESC_NOREDRAW) == 0) {
            descOrig = lo;
            descSize = hi;
            descOrig.x -= descBorder;
            descOrig.y -= descBorder;
            descSize.x -= descOrig.x-descBorder;
            descSize.y -= descOrig.y-descBorder;
        }


        if (OFF_D(mapD.orig, mapD.size, descOrig, descSize)) {
            ErrorMessage(MSG_MOVE_OUT_OF_BOUNDS);
        }
    }


    for (inx = 0; inx < COUNT( describePLs ); inx++) {
        if ((describePLs[inx].option & PDO_DLGIGNORE) != 0) {
            continue;
        }

        ddp = (descData_p)describePLs[inx].context;

        if ((ddp->mode&DESC_IGNORE) != 0) {
            continue;
        }

        if ((ddp->mode&DESC_CHANGE) == 0) {
        	if ((ddp->mode&DESC_CHANGE2) == 0)
        		continue;
        }

        if (ddp->mode&DESC_RO) {
        	wControlActive(ddp->control0, FALSE);
        } else {
        	wControlActive(ddp->control0, TRUE);
        }

        ddp->mode &= ~DESC_CHANGE;
        if (ddp->type == DESC_POS) {			//POS Has two fields
        	if (ddp->mode&DESC_CHANGE2) {
        		ddp->mode &= ~DESC_CHANGE2;		//Second time
        	} else {
        		ddp->mode |= DESC_CHANGE2;		//First time
        	}
        }

        ParamLoadControl(&describePG, inx);
    }
}


static void DescOk(void * junk)
{
    wHide(describePG.win);

    if (layerValue && *layerValue>=0) {
    	SetTrkLayer(descTrk, editableLayerList[*layerValue]);  //int found that is really in the parm controls.
    }
    layerValue = NULL; 									   // wipe out reference
    descUpdateFunc(descTrk, -1, descData, !descUndoStarted);
    descTrk = NULL;

    if (descUndoStarted) {
        UndoEnd();
        descUndoStarted = FALSE;
    }

    descNeedDrawHilite = FALSE;
    Reset(); // DescOk
}


static struct {
    parameterType pd_type;
    long option;
    int first;
    int last;
} descTypeMap[] = {
    /*NULL*/		{ 0, 0 },
    /*POS*/			{ PD_FLOAT, PDO_DIM,   I_FLOAT_0, I_FLOAT_N },
    /*FLOAT*/		{ PD_FLOAT, 0,		   I_FLOAT_0, I_FLOAT_N },
    /*ANGLE*/		{ PD_FLOAT, PDO_ANGLE, I_FLOAT_0, I_FLOAT_N },
    /*LONG*/		{ PD_LONG,	0,		   I_LONG_0, I_LONG_N },
    /*COLOR*/		{ PD_COLORLIST,	0,	   I_COLOR_0, I_COLOR_N },
    /*DIM*/			{ PD_FLOAT, PDO_DIM,   I_FLOAT_0, I_FLOAT_N },
    /*PIVOT*/		{ PD_RADIO, 0,		   I_PIVOT_0, I_PIVOT_N },
    /*LAYER*/		{ PD_DROPLIST,PDO_LISTINDEX,	   I_LAYER_0, I_LAYER_N },
    /*STRING*/		{ PD_STRING,0,		   I_STRING_0, I_STRING_N },
    /*TEXT*/		{ PD_TEXT,	PDO_DLGNOLABELALIGN, I_TEXT_0, I_TEXT_N },
    /*LIST*/		{ PD_DROPLIST, PDO_LISTINDEX,	   I_LIST_0, I_LIST_N },
    /*EDITABLELIST*/{ PD_DROPLIST, 0,	   I_EDITLIST_0, I_EDITLIST_N },
	/*BOXED*/      	{ PD_TOGGLE, 0,	       I_TOGGLE_0, I_TOGGLE_N },
};

/**
 * An unused param element is selected from the list of pre-defined param elements and initialized
 * for an element specific param. 
 * 
 * \param ddp Element specific param
 * \param valueP the value pointer used by the element
 * \param label the label assigned by the element
 * \param sep ?
 * \return the selected widget
 */
 
static wControl_p AssignParamToDescribeDialog(descData_p ddp, void * valueP, char * label,
                               wWinPix_t sep)
{
    int inx;
    paramData_t * param;
    /*Check to see if we already set up the linkage*/
    if (ddp->param0) {		// Has a link in the first and possibly second part
    	if (!ddp->control0) {
			param = ((paramData_t *)ddp->param0);
    	} else {
    		param = ((paramData_t *)ddp->param1);
    	}
		param->valueP = valueP;
		param->context = ddp;
		param->option = descTypeMap[ddp->type].option;

		if (ddp->type == DESC_POS) {
			char helpStr[STR_SHORT_SIZE];
			if (!ddp->control0)
				sprintf(helpStr, "%sx",ddp->helpStr);
			else
				sprintf(helpStr, "%sy",ddp->helpStr);
			param->assigned_helpStr = strdup(helpStr);
		} else {
			param->assigned_helpStr = strdup(ddp->helpStr);
		}

		if ((ddp->type == DESC_STRING) && ddp->max_string) {
			param->max_string = ddp->max_string;
			param->option |= PDO_STRINGLIMITLENGTH;
		}

		return param->control;
    }

    for (inx = descTypeMap[ddp->type].first; inx<descTypeMap[ddp->type].last;
            inx++) {
        if ((describePLs[inx].option & PDO_DLGIGNORE) != 0) {
            describePLs[inx].option = descTypeMap[ddp->type].option;

            if (describeW_posy > describeCmdButtonEnd) {
                describePLs[inx].option |= PDO_DLGUNDERCMDBUTT;
            }

            if (sep) {
            	describeW_posy += wControlGetHeight(describePLs[inx].control) + sep;
            	describe_row++;
            }
            describePLs[inx].context = ddp;
            describePLs[inx].valueP = valueP;

            if (describePLs[inx].assigned_helpStr)
            	free(describePLs[inx].assigned_helpStr);
            if (ddp->type == DESC_POS) {
            	char helpStr[STR_SHORT_SIZE];
            	if (!ddp->control0)
            		sprintf(helpStr, "%sx",ddp->helpStr);
            	else
            		sprintf(helpStr, "%sy",ddp->helpStr);
            	describePLs[inx].assigned_helpStr = strdup(helpStr);
            } else
            	describePLs[inx].assigned_helpStr = strdup(ddp->helpStr);

            if ((ddp->type == DESC_STRING) && ddp->max_string) {
            	describePLs[inx].max_string = ddp->max_string;
            	describePLs[inx].option |= PDO_STRINGLIMITLENGTH;
            }

            if (label && ddp->type != DESC_TEXT) {
                wControlSetLabel(describePLs[inx].control, label);
                describePLs[inx].winLabel = label;
            } else {
            	wControlSetLabel(describePLs[inx].control, "");
            	describePLs[inx].winLabel = "";
            }

            return describePLs[inx].control;
        }
    }

    AbortProg("AssignParamToDescribeDialog: can't find %d", ddp->type);
    return NULL;
}


static void DescribeLayout(
    paramData_t * pd,
    int inx,
    wWinPix_t colX,
    wWinPix_t * x,
    wWinPix_t * y)
{
    descData_p ddp;
    wWinPix_t w, h;

    if (inx < 0) {
        return;
    }

    if (pd->context == NULL) {
        return;
    }

    ddp = (descData_p)pd->context;
    *y = ddp->posy;

    if (ddp->type == DESC_POS &&
            ddp->control0 != pd->control) {
        *x += wControlGetWidth(pd->control) + 3;
    } else if (ddp->type == DESC_TEXT) {
        w = tdata.width;
        h = tdata.height;
        wTextSetSize((wText_p)pd->control, w, h);
    }

    wControlShow(pd->control, TRUE);
}


/**
 * Creation and modification of the Describe dialog box is handled here. As the number
 * of values for a track element depends on the specific type, this dialog is dynamically
 * updated to hsow the changable parameters only
 *
 * \param IN title Description of the selected part, shown in window title bar
 * \param IN template-id the name of the describe template to add to the window
 * \param IN trk Track element to be described
 * \param IN data
 * \param IN update
 *
 */

static wList_p setLayerL;
void DoDescribe(char * title, char *template_id, track_p trk, descData_p data, descUpdate_t update)
{
    int inx;
    descData_p ddp;
    char * label;
    int ro_mode;
    paramGroup_t * pg = NULL;

    if (!inDescribeCmd) {
        return;
    }

    CreateEditableLayersList();
    descTrk = trk;
    descData = data;
    descUpdateFunc = update;
    describeW_posy = 0;
    describe_row = 1;

    if (describePG.win == NULL) {
        /* SDB 5.13.2005 */
        ParamCreateDialog(&describePG, _("Description"), _("Done"), DescOk,
                          (paramActionCancelProc) DescribeCancel,
                          TRUE, DescribeLayout, F_RECALLPOS,
                          DescribeUpdate);
        describeCmdButtonEnd = wControlBelow((wControl_p)describePG.helpB);
    }

    for (inx=0; inx<COUNT( describePLs ); inx++) {
        describePLs[inx].option = PDO_DLGIGNORE;
        wControlShow(describePLs[inx].control, FALSE);
    }

    wlibHideAllRevealsExcept(pg->win,template_id);

    ro_mode = (GetLayerFrozen(GetTrkLayer(trk))?DESC_RO:0);

    if (ro_mode)
        for (ddp=data; ddp->type != DESC_NULL; ddp++) {
            if (ddp->mode&DESC_IGNORE) {
                continue;
            }

            ddp->mode |= DESC_RO;
        }

    for (ddp=data; ddp->type != DESC_NULL; ddp++) {
        if (ddp->mode&DESC_IGNORE) {
            continue;
        }

        label = _(ddp->label);
        ddp->posy = describeW_posy;
        ddp->control0 = AssignParamToDescribeDialog(ddp, ddp->valueP, label,
                                     (ddp->type == DESC_POS?3:3));
        if (ddp->type != DESC_LAYER)
        	wControlActive(ddp->control0, (!(ddp->mode&DESC_RO)));

        switch (ddp->type) {
        case DESC_POS:
            ddp->control1 = AssignParamToDescribeDialog(ddp,
                                         &((coOrd*)(ddp->valueP))->y,
                                         NULL,
                                         0);
            wControlActive(ddp->control1, (!(ddp->mode&DESC_RO)));
            break;

        case DESC_LAYER:
            wListClear((wList_p)ddp->control0);  // Rebuild list on each invocation

            if (ro_mode) {
            	char *layerFormattedName;
            	layerFormattedName = FormatLayerName(*(int *)(ddp->valueP));
            	wListAddValue((wList_p)ddp->control0, layerFormattedName, NULL, I2VP(inx));
                free(layerFormattedName);
                *(int *)(ddp->valueP) = 0;
                layerValue = (int *)(ddp->valueP);
                wControlActive(ddp->control0, FALSE);
            } else {
				for (inx = 0; inx<NUM_LAYERS; inx++) {
					char *layerFormattedName;
					layerFormattedName = FormatLayerName(editableLayerList[inx]);
					wListAddValue((wList_p)ddp->control0, layerFormattedName, NULL, I2VP(inx));
					free(layerFormattedName);
				}

				*(int *)(ddp->valueP) = SearchEditableLayerList(*(int *)(ddp->valueP));
				layerValue = (int *)(ddp->valueP);
				wControlActive(ddp->control0, TRUE);
            }

            *(int *)(ddp->valueP) = SearchEditableLayerList(*(int *)(ddp->valueP));
            layerValue = (int *)(ddp->valueP);
            break;

        default:
            break;
        }
    }

    ParamLayoutDialog(&describePG);
    ParamLoadControls(&describePG);
    sprintf(message, "%s (T%d)", title, GetTrkIndex(trk));
    wWinSetTitle(describePG.win, message);
    wShow(describePG.win);
}


static void DescChange(long changes)
{
	if ((changes&CHANGE_UNITS) && describePG.win && wWinIsVisible(describePG.win)) {
        ParamLoadControls(&describePG);
    }
}

/*****************************************************************************
 *
 * SIMPLE DESCRIPTION
 *
 */


EXPORT void DescribeCancel(void)
{
    if (describePG.win && wWinIsVisible(describePG.win)) {
        if (descTrk) {
        	ASSERT(!IsTrackDeleted(descTrk));
       		descUpdateFunc(descTrk, -1, descData, TRUE);
        	descTrk = NULL;

        }

        wHide(describePG.win);

        if (descUndoStarted) {
            UndoEnd();
            descUndoStarted = FALSE;
        }
    }

    descNeedDrawHilite = FALSE;
}


EXPORT STATUS_T CmdDescribe(wAction_t action, coOrd pos)
{
    static track_p trk;
    char msg[STR_SIZE];

    switch (action) {
    case C_START:
        InfoMessage(_("Select track to describe +Shift for Frozen"));
        wSetCursor(mainD.d,wCursorQuestion);
        descUndoStarted = FALSE;
        trk = NULL;
        return C_CONTINUE;

    case wActionMove:
    	trk = OnTrack(&pos, FALSE, FALSE);
    	if (trk && GetLayerFrozen(GetTrkLayer(trk)) && !(MyGetKeyState() & WKEY_SHIFT)) {
			trk = NULL;
			return C_CONTINUE;
		}
    	return C_CONTINUE;


    case C_DOWN:
        if ((trk = OnTrack(&pos, FALSE, FALSE)) != NULL) {
        	if (GetLayerFrozen(GetTrkLayer(trk)) && !(MyGetKeyState()& WKEY_SHIFT)) {
        		InfoMessage("Track is Frozen, Add Shift to Describe");
        		trk = NULL;
        		return C_CONTINUE;
        	}
            if (describePG.win && wWinIsVisible(describePG.win) && descTrk) {
                descUpdateFunc(descTrk, -1, descData, TRUE);
                descTrk = NULL;
            }

            descBorder = mainD.scale*0.1;

            if (descBorder < trackGauge) {
                descBorder = trackGauge;
            }

            inDescribeCmd = TRUE;
            GetBoundingBox(trk, &descSize, &descOrig);
            descOrig.x -= descBorder;
            descOrig.y -= descBorder;
            descSize.x -= descOrig.x-descBorder;
            descSize.y -= descOrig.y-descBorder;
            descNeedDrawHilite = TRUE;
            DescribeTrack(trk, msg, 255);
            inDescribeCmd = FALSE;
            InfoMessage(msg);
            trk = NULL;
        } else {
            InfoMessage("");
        }

        return C_CONTINUE;

    case C_REDRAW:

        if (describePG.win && wWinIsVisible(describePG.win) && descTrk) {
        	descNeedDrawHilite = TRUE;
        	coOrd lo,hi;
        	GetBoundingBox(descTrk,&hi,&lo);
			descOrig = lo;
			descSize = hi;
			descOrig.x -= descBorder;
			descOrig.y -= descBorder;
			descSize.x -= descOrig.x-descBorder;
			descSize.y -= descOrig.y-descBorder;

            DrawDescHilite(TRUE);

            if (descTrk && QueryTrack(descTrk, Q_IS_DRAW)) {
				DrawOriginAnchor(descTrk);
			}
        } else if (trk){
        	DrawTrack(trk,&tempD,wDrawColorPreviewSelected);
        }


        break;

    case C_CANCEL:
        DescribeCancel();
        wSetCursor(mainD.d,defaultCursor);
        return C_CONTINUE;

    case C_CMDMENU:
    	menuPos = pos;
    	if (!trk) wMenuPopupShow(descPopupM);
    	return C_CONTINUE;
    }


    return C_CONTINUE;
}



#include "bitmaps/describe.xpm"

extern wIndex_t selectCmdInx;
extern wIndex_t modifyCmdInx;
extern wIndex_t panCmdInx;

void InitCmdDescribe(wMenu_p menu)
{
    describeCmdInx = AddMenuButton(menu, CmdDescribe, "cmdDescribe",
                                   _("Properties"), wIconCreatePixMap(describe_xpm[iconSize]),
                                   LEVEL0, IC_CANCEL|IC_POPUP|IC_WANT_MOVE|IC_CMDMENU, ACCL_DESCRIBE, NULL);
    RegisterChangeNotification(DescChange);
    ParamRegister(&describePG);
}
void InitCmdDescribe2(wMenu_p menu)
{
    descPopupM = MenuRegister( "Describe Context Menu" );
    wMenuPushCreate(descPopupM, "cmdSelectMode", GetBalloonHelpStr("cmdSelectMode"), 0, DoCommandB, I2VP(selectCmdInx));
    wMenuPushCreate(descPopupM, "cmdModifyMode", GetBalloonHelpStr("cmdModifyMode"), 0, DoCommandB, I2VP(modifyCmdInx));
    wMenuPushCreate(descPopupM, "cmdPanMode", GetBalloonHelpStr("cmdPanMode"), 0, DoCommandB, I2VP(panCmdInx));

}
