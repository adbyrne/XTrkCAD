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

#include <stdint.h>
#include <string.h>

#include "common.h"
#include "utility.h"
#include "cundo.h"
#include "i18n.h"
#include "messages.h"
#include "param.h"
#include "fileio.h"
#include "cselect.h"
#include "track.h"

EXPORT wIndex_t describeCmdInx;
EXPORT BOOL_T inDescribeCmd;

extern wIndex_t selectCmdInx;
extern wIndex_t joinCmdInx;
extern wIndex_t modifyCmdInx;

static wWin_p describeWin;
static paramGroup_t *describeCurrentPG;
static wButton_p describeokB;
static wButton_p describeCancelB;
static wButton_p describeHelpB;


static track_p descTrk;
static descData_p descData;
static descUpdate_t descUpdateFunc;
static coOrd descOrig, descSize;
static POS_T descBorder;
static wDrawColor descColor = 0;
static BOOL_T descUndoStarted;
static BOOL_T descNeedDrawHilite;
static wPos_t describeW_posy;
static int describe_row;
static wPos_t describeCmdButtonEnd;

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
    { PD_STRING, NULL, "S1", PDO_NOPREF, (void*)300 },
    { PD_STRING, NULL, "S2", PDO_NOPREF, (void*)300 },
    { PD_STRING, NULL, "S3", PDO_NOPREF, (void*)300 },
    { PD_STRING, NULL, "S4", PDO_NOPREF, (void*)300 },
#define I_STRING_N		I_STRING_0+4

#define I_LAYER_0		I_STRING_N
    { PD_DROPLIST, NULL, "Y1", PDO_NOPREF, (void*)150, NULL, 0 },
#define I_LAYER_N		I_LAYER_0+1

#define I_COLOR_0		I_LAYER_N
    { PD_COLORLIST, NULL, "C1", PDO_NOPREF, NULL, N_("Color"), BC_HORZ|BC_NOBORDER },
#define I_COLOR_N		I_COLOR_0+1

#define I_LIST_0		I_COLOR_N
    { PD_DROPLIST, NULL, "L1", PDO_NOPREF, (void*)150, NULL, 0 },
    { PD_DROPLIST, NULL, "L2", PDO_NOPREF, (void*)150, NULL, 0 },
	{ PD_DROPLIST, NULL, "L3", PDO_NOPREF, (void*)150, NULL, 0 },
	{ PD_DROPLIST, NULL, "L4", PDO_NOPREF, (void*)150, NULL, 0 },
#define I_LIST_N		I_LIST_0+4

#define I_EDITLIST_0	I_LIST_N
    { PD_DROPLIST, NULL, "LE1", PDO_NOPREF, (void*)150, NULL, BL_EDITABLE },
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

static paramGroup_t describePG = { "describe", 0, describePLs, sizeof describePLs/sizeof describePLs[0] };

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
    wPos_t x, y, w, h;

    if (descNeedDrawHilite == FALSE) {
        return;
    }

    if (descColor==0) {
        descColor = wDrawColorGray(87);
    }

    w = (wPos_t)((descSize.x/mainD.scale)*mainD.dpi+0.5);
    h = (wPos_t)((descSize.y/mainD.scale)*mainD.dpi+0.5);
    mainD.CoOrd2Pix(&mainD,descOrig,&x,&y);
    wDrawFilledRectangle(tempD.d, x, y, w, h, selected?descColor:wDrawColorBlue, wDrawOptTemp|wDrawOptTransparent);
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
    descUpdateFunc(descTrk, ddp-descData, descData, FALSE);

    if (descTrk) {
        GetBoundingBox(descTrk, &hi, &lo);

        if (OFF_D(mapD.orig, mapD.size, descOrig, descSize)) {
            ErrorMessage(MSG_MOVE_OUT_OF_BOUNDS);
        }
    }

    if ((ddp->mode&DESC_NOREDRAW) == 0) {
        descOrig = lo;
        descSize = hi;
        descOrig.x -= descBorder;
        descOrig.y -= descBorder;
        descSize.x -= descOrig.x-descBorder;
        descSize.y -= descOrig.y-descBorder;
    }


    for (inx = 0; inx < pg->paramCnt; inx++) {
        if ((pg->paramPtr[inx].option & PDO_DLGIGNORE) != 0) {
            continue;
        }

        ddp = (descData_p)pg->paramPtr[inx].context;

        if ((ddp->mode&DESC_IGNORE) != 0) {
            continue;
        }

        if ((ddp->mode&DESC_CHANGE) == 0) {
        	if ((ddp->mode&DESC_CHANGE2) == 0)
        		continue;
        }

        ddp->mode &= ~DESC_CHANGE;
        if (ddp->type == DESC_POS) {			//POS Has two fields
        	if (ddp->mode&DESC_CHANGE2) {
        		ddp->mode &= ~DESC_CHANGE2;		//Second time
        	} else {
        		ddp->mode |= DESC_CHANGE2;		//First time
        	}
        }
        ParamLoadControl(pg, inx);
    }
}


static void DescOk(void * junk)
{
    wHide(describeWin);

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

static parameterType ConvertDescType(descType type) {
	return descTypeMap[type].pd_type;
}

static int GetDescTypeOption(descType type) {
	return descTypeMap[type].option;
}

static wControl_p AllocateButt(descData_p ddp, void * valueP, char * label,
                               wPos_t sep)
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


            if (sep && describePLs[inx].control) {
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

            //Call

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



/**
 * Creation and modification of the Describe dialog box is handled here. As the number
 * of values for a track element depends on the specific type, this dialog is dynamically
 * updated to show the needed parameters only
 *
 * Each describe user creates both a description struct and a .glade file whose toplevel widget is a revealer.
 * The names of the elements are "template_id-fieldname". The Top-level Revealer is "template_id.reveal".
 * Any fields that are optional within a template have a revealer "template_id-fieldname.reveal"
 *
 * This toplevel revealer is added to the describe.contentsbox box and all the reveals children are hidden except the template_id in question.
 * So eventually the window will contain a copy of all the different describe objects UIs. They were not included in one file because it would be too difficult to edit
 *
 * \param IN title Description of the selected part, shown in window title bar
 * \param IN template-id the name of the describe template to add to the window
 * \param IN trk Track element to be described
 * \param IN data the descData pointer
 * \param IN updateproc to proc to be run when updates occur
 *
 */

typedef struct {
	char * template_name;
	paramGroup_t * template_pg;
} sub_window_t, * sub_window_p;

static dynArr_t sub_windows;



/*
 * Setup a PGlist, filling out the paramData based on the DescData
 */
paramGroup_t * CreatePGList(char *tile, char *subtitle, descData_p data) {
		paramGroup_t * pg;
		pg = calloc(1,sizeof(paramGroup_t));
		pg->nameStr = "describe";
		pg->template_id = strdup(subtitle);
		pg->options = PGO_DIALOGTEMPLATE;
		pg->okB = describeokB;
		pg->cancelB = describeCancelB;
		pg->helpB = describeHelpB;

		int rows = 1;
		descData_t * describe;
		for (describe=data; (describe->type != DESC_NULL); describe++ ) {
			if (describe->type == DESC_POS) {
				rows = rows + 1;
			}
			rows++;
		}
		pg->paramCnt = 0;
		pg->paramPtr = calloc(rows,sizeof(paramData_t));
		int row = 0;
		paramData_t * param = pg->paramPtr;
		for (describe=data; (describe->type != DESC_NULL); describe++ ) {
			char name[STR_SHORT_SIZE];
			int inx = descTypeMap[describe->type].first;
			/* Copy the parts from the default list for this type */
			memcpy(&param[row],&describePLs[inx],sizeof(paramData_t));

			param[row].group = pg;
			param[row].type = ConvertDescType(describe->type);
			param[row].option = GetDescTypeOption(describe->type);
			param[row].option &= ~PDO_DLGIGNORE;
			param[row].context = describe;
			describe->param0 = &param[row];
			if (describe->type == DESC_POS) {
				sprintf(name,"%sx",describe->helpStr);
				param[row].nameStr = strdup(name);
				row++;
				/* Copy the parts from the default list for this type */
				memcpy(&param[row],&describePLs[inx],sizeof(paramData_t));
				param[row].group = pg;
				param[row].type = ConvertDescType(describe->type);
				param[row].option = GetDescTypeOption(describe->type);
				param[row].option &= ~PDO_DLGIGNORE;
				param[row].context = describe;
				sprintf(name,"%sy",describe->helpStr);
				param[row].nameStr = strdup(name);
				describe->param1 = &param[row];
			} else {
				sprintf(name,"%s",describe->helpStr);
				param[row].nameStr = strdup(name);
			}
			row++;
		}
		pg->paramCnt = row;
		return pg;
}

void DoDescribe(char * title, char * template_id, track_p trk, descData_p data, descUpdate_t update)
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

    wBool_t created = FALSE;

    if (wUITemplates()) {
		/* Deal with creating and re-using a parmlist per Describe Type */
		for (int i=0;i<sub_windows.cnt && (!pg); i++) {
			if ( strcmp( (((sub_window_t *)(sub_windows).ptr)[i]).template_name,template_id )==0 ) {
				pg = (((sub_window_t *)(sub_windows).ptr)[i]).template_pg;
			}
		}
		sub_window_p sub_window;
		/* New template, so set up PG parmlist and add to directory */
		if (!pg) {
			pg = CreatePGList(title, template_id, data);
			DYNARR_APPEND( sub_window_t, sub_windows, 5 );
			sub_window = &DYNARR_N(sub_window_t, sub_windows, sub_windows.cnt-1);
			sub_window->template_name = strdup(template_id);
			sub_window->template_pg = pg;
			created = TRUE;
			pg->win = describeWin;  /* Set up to use same window */
		}
    }

    if (pg->win == NULL || created) {   /* Same sub-template as last time? */
    	 pg->template_id = template_id;     /*Remember the template_id for Create */
    	 long opts = F_RECALLPOS|F_USETEMPLATE|F_DESCTEMPLATE;
    	 if (pg->win)
    		 opts |= F_DESCADDTEMPLATE;
        /* SDB 5.13.2005 */
        ParamCreateDialog(pg, _("Description"), _("Done"), DescOk,
                          (paramActionCancelProc) DescribeCancel,
                          TRUE, NULL, opts,
                          DescribeUpdate);
        describeCmdButtonEnd = wControlBelow((wControl_p)pg->helpB);
        describeWin = pg->win;
    } else {
    	pg->template_id = template_id;
    	describeWin = pg->win;
    }

    /* Hide all controls */
    paramData_t * describeData = pg->paramPtr;
    for (inx=0; inx<pg->paramCnt; inx++) {
        describeData[inx].option = PDO_DLGIGNORE;
        if (describeData[inx].control)
        	wControlShow(describeData[inx].control, FALSE);
    }

    wlibHideAllRevealsExcept(pg->win,template_id);

    ro_mode = (GetLayerFrozen(GetTrkLayer(trk))?DESC_RO:0);

    if (ro_mode)
        for (ddp=data; ddp->type != DESC_NULL; ddp++) {
            if (ddp->mode&DESC_IGNORE) {
                continue;
            }

            ddp->mode |= ro_mode;
        }

    for (ddp=data; ddp->type != DESC_NULL; ddp++) {
        if (ddp->mode&DESC_IGNORE) {
            continue;
        }

        label = _(ddp->label);
        ddp->posy = describeW_posy;
        ddp->grid_row0 = describe_row;
        ddp->grid_col0 = 1;
        ddp->control0 = NULL;    /* Used to know if POS x or y */
        ddp->control0 = AllocateButt(ddp, ddp->valueP, label,
                                     (ddp->type == DESC_POS?3:3));
        wControlActive(ddp->control0, ((ddp->mode|ro_mode)&DESC_RO)==0);
        wControlShow(ddp->control0,TRUE);

        switch (ddp->type) {
        case DESC_POS:

            ddp->control1 = AllocateButt(ddp,
                                         &((coOrd*)(ddp->valueP))->y,
                                         NULL,
                                         0);
            wControlActive(ddp->control1, ((ddp->mode|ro_mode)&DESC_RO)==0);
            wControlShow(ddp->control1,TRUE);

            ddp->grid_row1 = describe_row;
            ddp->grid_col1 = 3;
            break;

        case DESC_LAYER:
            wListClear((wList_p)ddp->control0);  // Rebuild list on each invocation

            for (inx = 0; inx<NUM_LAYERS; inx++) {
                char *layerFormattedName;
                layerFormattedName = FormatLayerName(editableLayerList[inx]);
                wListAddValue((wList_p)ddp->control0, layerFormattedName, NULL, (void*)(long)inx);
                free(layerFormattedName);
            }

            *(int *)(ddp->valueP) = SearchEditableLayerList(*(int *)(ddp->valueP));
            layerValue = (int *)(ddp->valueP);
            break;

        default:
            break;
        }
    }

    ParamLayoutDialog(pg);
    ParamLoadControls(pg);
    sprintf(message, "%s (T%d)", title, GetTrkIndex(trk));
    wWinSetTitle(pg->win, message);
    wShow(pg->win);
    wlibRedraw(pg->win);
    describeCurrentPG = pg;
    describeokB = pg->okB;
    describeCancelB = pg->cancelB;
    describeHelpB = pg->helpB;
}


static void DescChange(long changes)
{
    if ((changes&CHANGE_UNITS) && describeWin && wWinIsVisible(describeWin)) {
        ParamLoadControls(describeCurrentPG);
    }
}

/*****************************************************************************
 *
 * SIMPLE DESCRIPTION
 *
 */


EXPORT void DescribeCancel(void)
{
    if (describeWin && wWinIsVisible(describeWin)) {
        if (descTrk) {
        	if (!IsTrackDeleted(descTrk))
        		descUpdateFunc(descTrk, -1, descData, TRUE);
        	descTrk = NULL;

        }

        wHide(describeWin);

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
                                   _("Properties"), wIconCreatePixMap(describe_xpm),
                                   LEVEL0, IC_CANCEL|IC_POPUP|IC_WANT_MOVE|IC_CMDMENU, ACCL_DESCRIBE, NULL);
    RegisterChangeNotification(DescChange);
    ParamRegister(&describePG);
}
void InitCmdDescribe2(wMenu_p menu)
{
    descPopupM = MenuRegister( "Describe Context Menu" );
    wMenuPushCreate(descPopupM, "cmdSelectMode", GetBalloonHelpStr("cmdSelectMode"), 0, DoCommandB, (void*) (intptr_t) selectCmdInx);
    wMenuPushCreate(descPopupM, "cmdModifyMode", GetBalloonHelpStr("cmdModifyMode"), 0, DoCommandB, (void*) (intptr_t) modifyCmdInx);
    wMenuPushCreate(descPopupM, "cmdPanMode", GetBalloonHelpStr("cmdPanMode"), 0, DoCommandB, (void*) (intptr_t) panCmdInx);

}
