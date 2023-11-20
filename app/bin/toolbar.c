/*****************************************************************//**
 * \file   toolbar.c
 * \brief  Toolbar specific functions and data
 *********************************************************************/

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005,2023 Dave Bullis, Martin Fischer
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
#include "custom.h"
#include "fileio.h"
#include "param.h"
#include "include/toolbar.h"

EXPORT void ToolbarLayout(void* unused);

struct sToolbarState {
    int previousGroup;          // control variable for change control ops
    int layerButton;            // number of layer controls shown
    wWinPix_t nextX;            // drawing position for next control
    wWinPix_t rowHeight;        // height of row
};

// local function prototypes
static void InitializeToolbarDialog(void);
static void ToolbarChange(long changes);
static void ToolbarOk(void* unused);
static void ToolbarButtonPlace(struct sToolbarState* tbState, wIndex_t inx);
static void SaveToolbarConfig(void);

// toolbar properties
static long toolbarSet;
static wWinPix_t toolbarHeight = 0;
EXPORT int cmdGroup;

#define TOOLBARSET_INIT				(0xFFFF)
#define TOOLBAR_SECTION "misc"
#define TOOLBAR_VARIABLE "toolbarset"

#define GROUP_DISTANCE (5)     // default distance between button groups
#define GROUP_BIG_DISTANCE (GROUP_DISTANCE * 3) // big gap
#define TOOLBAR_MARGIN (20)     // left and right margins of toolbar
#define FIXEDLAYERCONTROLS (2)  // the layer groups has two controls that are
                                // always visible (list and background)

/*
* Bit handling macros
* these macros do not change the passed values but return the result.
* so if you want to change the value it has to be assigned eg.
* bits = SETBIT(bits, 2);
* in order to set bit 2 of the variable "bits"
* 
*/ 
#define GETBIT(value, bitpos )     ((value) & (1UL << (bitpos)))
#define ISBITSET(value, bitpos )   (((value)&(1UL <<bitpos))!=0)
#define CLEARBIT(value, bitpos )   ((value) & ~(1UL <<(bitpos)))
#define SETBIT(value, bitpos)      ((value) | (1UL <<(bitpos)))

#define ISGROUPVISIBLE(group)   ISBITSET(toolbarSet, group)

// toolbar button list
#define BUTTON_MAX (250)
static struct {
    wControl_p control;
    wBool_t enabled;
    wWinPix_t x, y;
    long options;
    int group;
    wIndex_t cmdInx;
} buttonList[BUTTON_MAX];
EXPORT int buttonCnt = 0; // TODO-misc-refactor


// control the order of the button groups inside the toolbar

struct buttonGroups {
    char*   label;          // display label
    int     group;          // id of group
    bool    biggap;         // control distance to previous group
};

static struct buttonGroups allToolbarGroups[] = {
    {N_("File Buttons"),            BG_FILE, false},
    {N_("Print Buttons"),           BG_PRINT, false},
    {N_("Import/Export Buttons"),   BG_EXPORTIMPORT, false},
    {N_("Zoom Buttons"),            BG_ZOOM, false},
    {N_("Undo Buttons"),            BG_UNDO, false},
    {N_("Easement Button"),         BG_EASE, false},
    {N_("SnapGrid Buttons"),        BG_SNAP, false},
    {N_("Create Track Buttons"),    BG_TRKCRT, true},
    {N_("Layout Control Elements"), BG_CONTROL, true},
    {N_("Modify Track Buttons"),    BG_TRKMOD, false},
    {N_("Properties/Select"),       BG_SELECT, false},
    {N_("Track Group Buttons"),     BG_TRKGRP, false},
    {N_("Train Group Buttons"),     BG_TRAIN, true},
    {N_("Create Misc Buttons"),     BG_MISCCRT, false},
    {N_("Ruler Button"),            BG_RULER, false},
    {N_("Layer Buttons"),           BG_LAYER, true},
    {N_("Hot Bar"),                 BG_HOTBAR},
    {NULL, 0L}
};

#define COUNTTOOLBARGROUPS (COUNT(allToolbarGroups)-1)

// toolbar options dialog
static wWin_p toolbarW;
static unsigned long toggleSet;

// callbacks for button presses
static void SelectAllGroups(void* unused);
static void InvertSelection(void* unused);

static paramData_t toolbarPLs[] = {
    { PD_TOGGLE, &toggleSet, "toolbarset", 0, NULL},
#define I_SELECTALL     (1)
    { PD_BUTTON, SelectAllGroups, "selectall", PDO_DLGBOXEND, NULL, N_("Select All") },
#define I_INVERT        (2)
    { PD_BUTTON, InvertSelection, "invert", PDO_DLGHORZ, NULL, N_("Invert Selection")}
};

static paramGroup_t toolbarPG = { "toolbar", PGO_RECORD, toolbarPLs,
    COUNT(toolbarPLs)};

/**
 * Initialize the list of available options. The list of labels is created 
 * from the allToolbarGroups array. Memory allocated here
 * is never freed as it might be used when opening the dialog
 * 
 * \param unused
 */

static void
InitializeToolbarDialog(void)
{
	char** labels = MyMalloc((COUNT(allToolbarGroups)) * sizeof(char*));

    for (int i = 0; i < COUNT(allToolbarGroups); i++) {
		labels[i] = allToolbarGroups[i].label;
	}
	toolbarPLs[0].winData = labels;

    ParamRegister(&toolbarPG);
}

static void ToolbarChange(long changes)
{
    if ((changes & CHANGE_TOOLBAR)) {
        MainProc(mainW, wResize_e, NULL, NULL);
    }
}

/**
 * Handle button press to select all groups. Set all bits to 1, unused bits 
 * will be ignored
 * 
 * \param unused
 */
static void SelectAllGroups(void* unused)
{
    toggleSet = ~(0UL);

    ParamLoadControls(&toolbarPG);
}

/**
 * Handle button press to invert the current selection. Invert all bits by, 
 * XOR with 1s, unused bits will be ignored
 *
 * \param unused
 */
static void InvertSelection(void* unused)
{
    toggleSet ^= ~(0UL);

    ParamLoadControls(&toolbarPG);
}

/**
 * Handle the ok press. The bit pattern set up from the dialog is converted 
 * to the pattern used by the toolbar. Then the toolbar is refreshed.
 * 
 * \param unused
 */

static void ToolbarOk(void* unused)
{
    toolbarSet = 0;

    for (int i = 0; i < COUNTTOOLBARGROUPS; i++) {
        if (toggleSet & (1UL << i)) {
            toolbarSet = SETBIT(toolbarSet, allToolbarGroups[i].group);
        }
    }
    SaveToolbarConfig();
    ToolbarLayout(unused);
    MainProc(mainW, wResize_e, NULL, NULL);
    wHide(toolbarW);
}

/**
 * When selected from the menu the toolbar config dialog is opened. First lazy
 * initialization is done on first call. Then the toggle states are set from
 * the toolbar configuration bit pattern and the dialog is shown.
 * 
 * \param unused
 */

EXPORT void DoToolbar(void* unused)
{
    if (!toolbarW) {
        InitializeToolbarDialog();
        toolbarW = ParamCreateDialog(&toolbarPG,
            MakeWindowTitle(_("Toolbar Options")), _("OK"), ToolbarOk, wHide,
            TRUE, NULL, 0, NULL);
    }

    toggleSet = 0;
    for (int i = 0; i < COUNTTOOLBARGROUPS; i++) {
        if (ISBITSET(toolbarSet, allToolbarGroups[i].group)) {
            toggleSet = SETBIT(toggleSet, i);
        }
    }
    ParamLoadControls(&toolbarPG);
    wShow(toolbarW);
}

/**
 * Check whether button group is configured to be visible.
 * 
 * \param   group   single group to check
 * \return  true if visible
 */

EXPORT bool
ToolbarIsGroupVisible(int group)
{
    CHECK(group > 0);
    CHECK(group <= COUNTTOOLBARGROUPS);

    return(ISGROUPVISIBLE(group));
}

/**
 * Get the current height of the toolbar.
 * 
 * \return 
 */

EXPORT wWinPix_t
ToolbarGetHeight(void)
{
    return(toolbarHeight);
}

/**
 * .
 */

EXPORT void 
ToolbarSetHeight(wWinPix_t newHeight)
{
    toolbarHeight = newHeight;
}

/**
 * Buttons are visible when the command is enabled or when additional
 * layer buttons need to be shown.
 * 
 * \param inx
 */

bool
IsButtonVisible(int group, long mode, long options, long layerButtons)
{
    if (group == BG_LAYER) {
        if (layerButtons < layerCount+FIXEDLAYERCONTROLS)
            return true;
        else
            return false;
    }
    return(IsCommandEnabled(mode, options));
}

/**
 * Calculate the position and visibility of a button and display it.
 * 
 * \param inx   index into button list
 */

static void ToolbarButtonPlace(struct sToolbarState *tbState, wIndex_t inx)
{
    wWinPix_t w, h, offset;
    wWinPix_t width;
    wWinPix_t gap = GROUP_DISTANCE;
    int currentGroup = buttonList[inx].group;

    wWinGetSize(mainW, &width, &h);

    if (buttonList[inx].control) {
        if (tbState->rowHeight <= 0) {
            tbState->rowHeight = wControlGetHeight(buttonList[inx].control);
            toolbarHeight = tbState->rowHeight + 5;
        }

        if (currentGroup != tbState->previousGroup) {
            for (int i = 0; i < COUNTTOOLBARGROUPS; i++) {
                if (allToolbarGroups[i].group == currentGroup && 
                    allToolbarGroups[i].biggap) {
                    gap = GROUP_BIG_DISTANCE;
                }
            }
        }

        if ((ISGROUPVISIBLE(currentGroup)) &&
            IsButtonVisible(currentGroup, programMode, 
                buttonList[inx].options, tbState->layerButton ))
        {
            if (currentGroup != tbState->previousGroup) {
                tbState->nextX += gap;
                tbState->previousGroup = currentGroup;
            }
            w = wControlGetWidth(buttonList[inx].control);
            h = wControlGetHeight(buttonList[inx].control);
            if (h < tbState->rowHeight) {
                offset = (h - tbState->rowHeight) / 2;
                h = tbState->rowHeight;  //Uniform
            }
            else {
                offset = 0;
            }
            if (inx < buttonCnt - 1 && 
                (buttonList[inx + 1].options & IC_ABUT)) {
                    w += wControlGetWidth(buttonList[inx + 1].control);
            }
            if (tbState->nextX + w > width - TOOLBAR_MARGIN) {
                tbState->nextX = 5;
                toolbarHeight += h + 5;
            }
            if ((currentGroup == BG_LAYER) && 
                    tbState->layerButton >= FIXEDLAYERCONTROLS && 
                    GetLayerHidden(tbState->layerButton - FIXEDLAYERCONTROLS)) {
                wControlShow(buttonList[inx].control, FALSE);
                tbState->layerButton++;
            }
            else {
                wWinPix_t newX = tbState->nextX;
                wWinPix_t newY = toolbarHeight - (h + 5 + offset);

                // count number of shown layer buttons
                if (currentGroup == BG_LAYER) {
                    tbState->layerButton++;
                }
                if ((newX != buttonList[inx].x) || (newY != buttonList[inx].y)) {
                    wControlShow(buttonList[inx].control, FALSE);

                    wControlSetPos(buttonList[inx].control, newX,
                        newY);
                }
                buttonList[inx].x = newX;
                buttonList[inx].y = newY;
                tbState->nextX += wControlGetWidth(buttonList[inx].control);
                wControlShow(buttonList[inx].control, TRUE);
            }
        }
        else {
            wControlShow(buttonList[inx].control, FALSE);
        }
    }
}

EXPORT void ToolbarLayout(void* data)
{
	int inx;
    struct sToolbarState state = {
        .previousGroup = 0,
        .nextX = 0,
        .layerButton = 0,
        .rowHeight = 0,
    };

	for (inx = 0; inx < buttonCnt; inx++) {
		ToolbarButtonPlace(&state, inx);
	}

	if (ISBITSET(toolbarSet, BG_HOTBAR)) {
		LayoutHotBar(data);
	}
	else {
		HideHotBar();
	}
}

/**
 *  Set the 'pressed' state of a toolbar button.
 *
 * \param button    index into button list
 * \param busy      desired button state
 */

EXPORT void ToolbarButtonBusy(wIndex_t button, wBool_t busy)
{
    wButtonSetBusy((wButton_p)buttonList[button].control,
        busy);
}

/**
 * Set state of a toolbar button .
 *
 * \param button	index into button list
 * \param enable	desired state, FALSE if disabled, TRUE if enabled
 */

EXPORT void ToolbarButtonEnable(wIndex_t button, wBool_t enable)
{
    wControlActive(buttonList[button].control,
                   enable);
}

/**
 * Enable toolbar buttons that depend on selected track.
 *
 * \param selected true if any track is selected
 */

EXPORT void ToolbarButtonEnableIfSelect(bool selected)
{
    for (int inx = 0; inx < buttonCnt; inx++) {
        if (buttonList[inx].cmdInx < 0
                && (buttonList[inx].options & IC_SELECTED)) {
            ToolbarButtonEnable(inx, selected );
        }
    }
}

/**
 * Place a control onto the toolbar. The control is added to the toolbar
 * control list and initially hidden. Placement and visibility is controlled
 * by ToolbarButtonPlace()
 * 
 * \param control   the control to add
 * \param options   control options
 */

EXPORT void ToolbarControlAdd(wControl_p control, long options)
{
	CHECK(buttonCnt < BUTTON_MAX - 1);
    buttonList[buttonCnt].enabled = TRUE;
    buttonList[buttonCnt].options = options;
    buttonList[buttonCnt].group = cmdGroup;
    buttonList[buttonCnt].x = 0;
    buttonList[buttonCnt].y = 0;
    buttonList[buttonCnt].control = control;
    buttonList[buttonCnt].cmdInx = -1;
    wControlShow(control, FALSE);
    buttonCnt++;
}

/**
 * Link a command to a specific toolbar button.
 *
 * \param button	the button
 * \param command	command to activate when button is pressed
 * \return
 */

EXPORT void ToolbarButtonCommandLink(wIndex_t button, int command)
{
    if (button >= 0 && buttonList[button].cmdInx == -1) {
        // set button back-link
        buttonList[button].cmdInx = commandCnt;
    }
}

/**
 * Update the toolbar button for selected command eg. circle vs. filled
 * circle.
 *
 * \param button	toolbar button
 * \param command	current command
 * \param icon		new icon
 * \param helpKey	new help key
 * \param context	new command context
 */

EXPORT void ToolbarUpdateButton(wIndex_t button, wIndex_t command,
                                char * icon,
                                const char * helpKey,
                                void * context)
{
    if (buttonList[button].cmdInx != command) {
        wButtonSetLabel((wButton_p) buttonList[button].control,icon);
        wControlSetHelp(buttonList[button].control,
                        GetBalloonHelpStr(helpKey));
        wControlSetContext(buttonList[button].control,
                           context);
        buttonList[button].cmdInx = command;
    }
}

/*--------------------------------------------------------------------*/

/**
 * Handle simulated button press during playbook.
 * 
 * \param buttInx   selected button
 */
EXPORT void PlaybackButtonMouse(wIndex_t buttInx)
{
    wWinPix_t cmdX, cmdY;
    coOrd pos;

    if (buttInx < 0 || buttInx >= buttonCnt) {
        return;
    }
    if (buttonList[buttInx].control == NULL) {
        return;
    }
    cmdX = buttonList[buttInx].x + 17;
    cmdY = toolbarHeight - (buttonList[buttInx].y + 17)
           + (wWinPix_t)(mainD.size.y / mainD.scale * mainD.dpi) + 30;

    mainD.Pix2CoOrd(&mainD, cmdX, cmdY, &pos);
    MovePlaybackCursor(&mainD, pos, TRUE, buttonList[buttInx].control);
    if (playbackTimer == 0) {
        wButtonSetBusy((wButton_p)buttonList[buttInx].control, TRUE);
        wFlush();
        wPause(500);
        wButtonSetBusy((wButton_p)buttonList[buttInx].control, FALSE);
        wFlush();
    }
}

/**
 * Handle cursor positioning for toolbar buttons during playback .
 * 
 * \param buttonInx
 */

EXPORT void ToolbarButtonPlayback(wIndex_t buttonInx)
{
    wWinPix_t cmdX, cmdY;
    coOrd pos;

    cmdX = buttonList[buttonInx].x + 17;
    cmdY = toolbarHeight - (buttonList[buttonInx].y + 17)
           + (wWinPix_t)(mainD.size.y / mainD.scale * mainD.dpi) + 30;
    mainD.Pix2CoOrd(&mainD, cmdX, cmdY, &pos);
    MovePlaybackCursor(&mainD, pos, TRUE, buttonList[buttonInx].control);
}

/**
 * Save the toolbar setting to the configuration file.
 * 
 */

static void
SaveToolbarConfig(void)
{
    wPrefSetInteger(TOOLBAR_SECTION, TOOLBAR_VARIABLE, toolbarSet);
    if (recordF)
        fprintf(recordF, "PARAMETER %s %s %ld", TOOLBAR_SECTION, 
            TOOLBAR_VARIABLE, toolbarSet);

}

/**
 * Get the preferences for the toolbar from the configuration file. 
 * Bits unused are cleared just to be sure;
 * 
 */

EXPORT void
ToolbarLoadConfig(void)
{
    unsigned long maxToolbarSet = (1 << COUNTTOOLBARGROUPS) - 1;
    unsigned long toolbarSetIni;

    wPrefGetInteger(TOOLBAR_SECTION, TOOLBAR_VARIABLE, &toolbarSetIni, 
        TOOLBARSET_INIT);

    toolbarSet = toolbarSetIni & maxToolbarSet;

    // unused but saved to stay compatible
    wPrefSetInteger(TOOLBAR_SECTION, "max-toolbarset", maxToolbarSet);

    if (recordF)
        fprintf(recordF, "PARAMETER %s %s %lX -> %lX", TOOLBAR_SECTION,
            TOOLBAR_VARIABLE, toolbarSetIni, toolbarSet);
}

/**
 * Initialize toolbar functions.
 * 
 */

EXPORT void InitToolbar(void)
{
    RegisterChangeNotification(ToolbarChange);

    ToolbarLoadConfig();
}
