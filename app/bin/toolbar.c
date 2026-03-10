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
#include <form.h>
#include "paths.h"
#include "track.h"
#include "include/toolbar.h"

int log_playbackbuttonmouse = 0;

//EXPORT void ToolbarLayout(void* unused);

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

#define TOOLBARSET_INIT				(0xFFFF)
#define TOOLBAR_SECTION "toolbar"
#define TOOLBAR_VISIBLE "toolbarset"
#define TOOLBAR_LAYER_BUTTONS "button-count"

#define FIXEDLAYERCONTROLS (3)  // the layer groups has three controls 
				// that are always visible (list, background and manage)

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
	//wWinPix_t x, y;
	long options;
	int group;
	wIndex_t cmdInx;
} buttonList[BUTTON_MAX];

EXPORT int buttonCnt = 0; // TODO-misc-refactor

// control the order of the button groups inside the toolbar

struct buttonGroups {
	int     group;          // id of group
	bool    biggap;         // control distance to previous group
};

static struct buttonGroups allToolbarGroups[] = {
	{BG_FILE, false},
	{BG_PRINT, false},
	{BG_EXPORTIMPORT, false},
	{BG_ZOOM, false},
	{BG_UNDO, false},
	{BG_EASE, false},
	{BG_SNAP, false},
	{BG_TRKCRT, true},
	{BG_CONTROL, true},
	{BG_TRKMOD, false},
	{BG_SELECT, false},
	{BG_TRKGRP, false},
	{BG_TRAIN, true},
	{BG_MISCCRT, false},
	{BG_RULER, false},
	{BG_LAYER, true},
	{BG_HOTBAR,false },
	{0L, false}
};

#define COUNTTOOLBARGROUPS (COUNT(allToolbarGroups)-1)
#define NUM_BUTTONS		(99)

// toolbar options dialog
static wControl_p toolbarW;
static unsigned long toggleSet;
long layerCount = 10;

static paramIntegerRange_t buttonRange = { 0, NUM_BUTTONS };

// callbacks for button presses
static void SelectAllGroups(void* unused);
static void InvertSelection(void* unused);

static paramData_t toolbarPLs[] = {
	{ PD_TOGGLE, &toggleSet, "toolbarset", 0, NULL},
#define I_SELECTALL     (1)
	{ PD_BUTTON, SelectAllGroups, "selectall"},
#define I_INVERT        (2)
	{ PD_BUTTON, InvertSelection, "invert"},
	{ PD_LONG, &layerCount, "button-count", 0L, &buttonRange, ""}
};

static paramGroup_t toolbarPG = { "toolbar", PGO_RECORD | PGO_FULLDIALOGFROMBUILDER, toolbarPLs,
                                  COUNT(toolbarPLs)
                                };

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
	FormRegister(&toolbarPG);
}

static void ToolbarChange(long changes)
{
	if ((changes & CHANGE_TOOLBAR)) {
		//MainProc(mainW, wResize_e, NULL, NULL);
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

	FormLoadControls(&toolbarPG);
}

/**
 * Handle button press to invert the current selection. Invert all bits by
 * XORing with 1s, unused bits will be ignored
 *
 * \param unused
 */
static void InvertSelection(void* unused)
{
	toggleSet ^= ~(0UL);

	FormLoadControls(&toolbarPG);
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
	//MainProc(mainW, wResize_e, NULL, NULL);
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
		toolbarW = FormCreateDialog(&toolbarPG, MakeWindowTitle(_("Toolbar Options")), 
			NULL, ToolbarOk, 
			NULL, FormCancel_Restore, 
			TRUE, 0, NULL);
	}

	toggleSet = 0;
	for (int i = 0; i < COUNTTOOLBARGROUPS; i++) {
		if (ISBITSET(toolbarSet, allToolbarGroups[i].group)) {
			toggleSet = SETBIT(toggleSet, i);
		}
	}
	FormLoadControls(&toolbarPG);
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
		if (layerButtons < layerCount+FIXEDLAYERCONTROLS) {
			return true;
		} else {
			return false;
		}
	}
	return(IsCommandEnabled(mode, options));
}

/**
 * Get visibility of a button and display it.
 *
 * \param inx   index into button list
 */

static void ToolbarButtonPlace(struct sToolbarState *tbState, wIndex_t inx)
{
	int currentGroup = buttonList[inx].group;

	if (buttonList[inx].control) {

		if ((ISGROUPVISIBLE(currentGroup)) &&
		    IsButtonVisible(currentGroup, programMode,
		                    buttonList[inx].options, tbState->layerButton )) {
			if (currentGroup != tbState->previousGroup) {
				tbState->previousGroup = currentGroup;
			}
			if ((currentGroup == BG_LAYER) &&
			    tbState->layerButton >= FIXEDLAYERCONTROLS &&
			    GetLayerHidden(tbState->layerButton - FIXEDLAYERCONTROLS)) {
				wControlShow(buttonList[inx].control, FALSE);
				tbState->layerButton++;
			} else {
				// count number of shown layer buttons
				if (currentGroup == BG_LAYER) {
					tbState->layerButton++;
				}
				wControlShow(buttonList[inx].control, TRUE);
			}
		} else {
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
		//LayoutHotBar(data);
	} else {
		HideHotBar();
	}
}

EXPORT wBool_t ToolbarGetButtonSticky(wIndex_t button)
{
	if ( button < 0 ) { return FALSE; }
	return wStickyGetSticky(buttonList[button].control);
}

/**
 *  Set the 'pressed' state of a toolbar button. Works for sticky button as well.
 *
 * \param button    index into button list
 * \param busy      desired button state
 */

EXPORT void ToolbarButtonBusy(wIndex_t button, wBool_t busy)
{
	wButtonSetBusy(buttonList[button].control, busy);
}

/**
 * Return the control for button
 *
 * \param button
 * \return the control
 */

EXPORT wControl_p ToolbarButtonGetControl( wIndex_t button )
{
	return buttonList[button].control;
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

EXPORT void ToolbarControlAdd(wControl_p control, long options, int cmdGroup)
{
	CHECK(buttonCnt < BUTTON_MAX - 1);
	buttonList[buttonCnt].enabled = TRUE;
	buttonList[buttonCnt].options = options;
	buttonList[buttonCnt].group = cmdGroup;
	//buttonList[buttonCnt].x = 0;
	//buttonList[buttonCnt].y = 0;
	buttonList[buttonCnt].control = control;
	buttonList[buttonCnt].cmdInx = -1;
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
                                wIcon_p icon,
                                const char * helpKey,
                                void * context)
{
	if (buttonList[button].cmdInx != command) {
		wButtonSetIcon(buttonList[button].control,icon);
		wTooltipSet(buttonList[button].control, NULL, helpKey);
		           
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
EXPORT void PlaybackButtonMouse(wIndex_t buttonInx)
{
	if (buttonInx < 0 || buttonInx >= buttonCnt) {
		return;
	}
	if (buttonList[buttonInx].control == NULL) {
		return;
	}
	wWinPix_t cmdX, cmdY;
	int cmdInx = buttonList[buttonInx].cmdInx;
	wControlGetPos( buttonList[buttonInx].control, &cmdX, &cmdY );
	coOrd pos;
	mainD.Pix2CoOrd(&mainD, cmdX, cmdY, &pos);
	LOG( log_playbackbuttonmouse, 1,
		( "PbBM: Butt:%d, Cmd:%d Zoom:%0.2f Orig:%0.3f x %0.3f Size:%0.3f x %0.3f @ %ld x %ld = pos: %0.3f x %0.3f\n",
			buttonInx, cmdInx,
			mainD.scale, mainD.orig.x, mainD.orig.y,
			mainD.size.x, mainD.size.y,
			cmdX, cmdY,
	     		pos.x, pos.y ) );
	// This hilites the control nd moves the cursor
	MovePlaybackCursor(&mainD, pos, TRUE, buttonList[buttonInx].control);
}


/**
 * Save the toolbar setting to the configuration file.
 *
 */

static void
SaveToolbarConfig(void)
{
	wPrefSetInteger(TOOLBAR_SECTION, TOOLBAR_VISIBLE, toolbarSet);
	if (recordF)
		fprintf(recordF, "PARAMETER %s %s %ld", TOOLBAR_SECTION,
		        TOOLBAR_VISIBLE, toolbarSet);

	wPrefSetInteger(TOOLBAR_SECTION, TOOLBAR_LAYER_BUTTONS, layerCount);

	if (recordF)
		fprintf(recordF, "PARAMETER %s %s %ld", TOOLBAR_SECTION,
			TOOLBAR_LAYER_BUTTONS, layerCount);

}

static char* resourceFiles[] = { "icons16.gresource", 
								 "icons24.gresource", 
								 "icons32.gresource" };

void
LoadIconResource(unsigned iconSize)
{
	char* pathToResourceFile = NULL;

	MakeFullpath(&pathToResourceFile, wGetAppLibDir(), resourceFiles[iconSize], NULL);
	if (pathToResourceFile) {
		printf("Loading icons from: %s\n", pathToResourceFile);

		wLoadResourceFile(pathToResourceFile);

		free(pathToResourceFile);
	}
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
	long toolbarSetIni;

	wPrefGetInteger(TOOLBAR_SECTION, TOOLBAR_VISIBLE, &toolbarSetIni,
	                TOOLBARSET_INIT);

	toolbarSet = (unsigned long)toolbarSetIni & maxToolbarSet;

	// unused but saved to stay compatible
	wPrefSetInteger(TOOLBAR_SECTION, "max-toolbarset", maxToolbarSet);

	if (recordF)
		fprintf(recordF, "PARAMETER %s %s %lX -> %lX", TOOLBAR_SECTION,
		        TOOLBAR_VISIBLE, toolbarSetIni, toolbarSet);


	wPrefGetInteger(TOOLBAR_SECTION, TOOLBAR_LAYER_BUTTONS, &layerCount, 10);
	if (recordF)
		fprintf(recordF, "PARAMETER %s %s -> %ld", TOOLBAR_SECTION,
			TOOLBAR_LAYER_BUTTONS, layerCount);

	wPrefGetInteger("pref", "iconsize", (long*)&iconSize, 0);
	if (recordF)
		fprintf(recordF, "PARAMETER %s %s -> %d", "pref",
			"iconsize", iconSize);

	LoadIconResource(iconSize);
}

/**
 * Initialize toolbar functions.
 *
 */

EXPORT void InitToolbar(void)
{
	RegisterChangeNotification(ToolbarChange);

	ToolbarLoadConfig();

	log_playbackbuttonmouse = LogFindIndex( "playbackbuttonmouse" );
}
