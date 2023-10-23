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
#include "fileio.h"
#include "param.h"
#include "include/toolbar.h"

#define TOOLBARSET_INIT				(0xFFFF)
EXPORT long toolbarSet = TOOLBARSET_INIT;
EXPORT wWinPix_t toolbarHeight = 0;
static wWinPix_t toolbarWidth = 0;

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

/*
 * These array control the choices available in the Toolbar setup.
 * For each choice, the text is given and the respective mask is
 * specified in the following array.
 * Note: text and choices must be given in the same order.
 */

static char* AllToolbarLabels[] = { N_("File Buttons"), N_("Print Buttons"), N_("Import/Export Buttons"),
                                    N_("Zoom Buttons"), N_("Undo Buttons"), N_("Easement Button"), N_("SnapGrid Buttons"),
                                    N_("Create Track Buttons"), N_("Layout Control Elements"),
                                    N_("Modify Track Buttons"), N_("Properties/Select"),
                                    N_("Track Group Buttons"), N_("Train Group Buttons"),
                                    N_("Create Misc Buttons"), N_("Ruler Button"),
                                    N_("Layer Buttons"), N_("Hot Bar"),
                                    NULL
                                  };
static long AllToolbarMasks[] = { 1 << BG_FILE, 1 << BG_PRINT, 1 << BG_EXPORTIMPORT,
                                  1 << BG_ZOOM, 1 << BG_UNDO, 1 << BG_EASE, 1 << BG_SNAP, 1 << BG_TRKCRT,
                                  1 << BG_CONTROL, 1 << BG_TRKMOD, 1 << BG_SELECT, 1 << BG_TRKGRP, 1 << BG_TRAIN,
                                  1 << BG_MISCCRT, 1 << BG_RULER, 1 << BG_LAYER, 1 << BG_HOTBAR
                                };

static wMenuToggle_p AllToolbarMI[COUNT(AllToolbarMasks)];

static void ToolbarAction(void* data)
{
    int inx = (int)VP2L(data);
    CHECK(inx >= 0 && inx < COUNT(AllToolbarMasks));
    wBool_t set = wMenuToggleGet(AllToolbarMI[inx]);
    long mask = AllToolbarMasks[inx];
    if (set) {
        toolbarSet |= mask;
    }
    else {
        toolbarSet &= ~mask;
    }
    wPrefSetInteger("misc", "toolbarset", toolbarSet);
    MainProc(mainW, wResize_e, NULL, NULL);
    if (recordF)
        fprintf(recordF, "PARAMETER %s %s %ld", "misc", "toolbarset",
                toolbarSet);
}

/**
 * Create the Toolbar configuration submenu. Based on two arrays of 
 * descriptions and masks, the toolbar submenu is created dynamically.
 *
 * \param toolbarM IN menu to which the toogles will be added
 */

EXPORT void CreateToolbarM(wMenu_p toolbarM)
{
    int inx, cnt;
    long* masks;
    char** labels;
    wBool_t set;

    cnt = COUNT(AllToolbarMasks);
    masks = AllToolbarMasks;
    labels = AllToolbarLabels;
    for (inx = 0; inx < cnt; inx++, masks++, labels++) {
        set = (toolbarSet & *masks) != 0;
        AllToolbarMI[inx] = wMenuToggleCreate(toolbarM, 
            "toolbarM", _(*labels), 0, set, ToolbarAction, I2VP(inx));
    }
}


static void LayoutSetPos(wIndex_t inx)
{
    wWinPix_t w, h, offset;
    static wWinPix_t toolbarRowHeight = 0;
    static wWinPix_t width;
    static int lastGroup;
    static wWinPix_t gap;
    static int layerButtCnt;
    static int layerButtNumber;
    int currGroup;

    if (inx == 0) {
        lastGroup = 0;
        wWinGetSize(mainW, &width, &h);
        gap = 5;
        toolbarWidth = width - 20 + 5;
        layerButtCnt = 0;
        layerButtNumber = 0;
        toolbarHeight = 0;
    }

    if (buttonList[inx].control) {
        if (toolbarRowHeight <= 0) {
            toolbarRowHeight = wControlGetHeight(buttonList[inx].control);
        }

        currGroup = buttonList[inx].group & ~BG_BIGGAP;
        if (currGroup != lastGroup && (buttonList[inx].group & BG_BIGGAP)) {
            gap = 15;
        }
        if ((toolbarSet & (1 << currGroup))
                && (programMode != MODE_TRAIN
                    || (buttonList[inx].options
                        & (IC_MODETRAIN_TOO | IC_MODETRAIN_ONLY)))
                && (programMode == MODE_TRAIN
                    || (buttonList[inx].options & IC_MODETRAIN_ONLY) == 0)
                && ((buttonList[inx].group & ~BG_BIGGAP) != BG_LAYER
                    || layerButtCnt < layerCount)) {
            if (currGroup != lastGroup) {
                toolbarWidth += gap;
                lastGroup = currGroup;
                gap = 5;
            }
            w = wControlGetWidth(buttonList[inx].control);
            h = wControlGetHeight(buttonList[inx].control);
            if (h < toolbarRowHeight) {
                offset = (h - toolbarRowHeight) / 2;
                h = toolbarRowHeight;  //Uniform
            }
            else {
                offset = 0;
            }
            if (inx < buttonCnt - 1 && 
                (buttonList[inx + 1].options & IC_ABUT)) {
                    w += wControlGetWidth(buttonList[inx + 1].control);
            }
            if (toolbarWidth + w > width - 20) {
                toolbarWidth = 0;
                toolbarHeight += h + 5;
            }
            if ((currGroup == BG_LAYER) && layerButtNumber > 1
                    && GetLayerHidden(layerButtNumber - 2)) {
                wControlShow(buttonList[inx].control, FALSE);
                layerButtNumber++;
            }
            else {
                if (currGroup == BG_LAYER) {
                    if (layerButtNumber > 1) {
                        layerButtCnt++;    // Ignore List and Background
                    }
                    layerButtNumber++;
                }
                wControlSetPos(buttonList[inx].control, toolbarWidth,
                               toolbarHeight - (h + 5 + offset));
                buttonList[inx].x = toolbarWidth;
                buttonList[inx].y = toolbarHeight - (h + 5 + offset);
                toolbarWidth += wControlGetWidth(buttonList[inx].control);
                wControlShow(buttonList[inx].control, TRUE);
            }
        }
        else {
            wControlShow(buttonList[inx].control, FALSE);
        }
    }
}

EXPORT void LayoutToolBar(void* data)
{
    int inx;

    for (inx = 0; inx < buttonCnt; inx++) {
        LayoutSetPos(inx);
    }
    if (toolbarSet & (1 << BG_HOTBAR)) {
        LayoutHotBar(data);
    }
    else {
        HideHotBar();
    }
}

static void ToolbarChange(long changes)
{
    if ((changes & CHANGE_TOOLBAR)) {
        /*if ( !(changes&CHANGE_MAIN) )*/
        MainProc(mainW, wResize_e, NULL, NULL);
        /*else
         LayoutToolBar();*/
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


EXPORT void AddToolbarControl(wControl_p control, long options)
{
//	CHECK(buttonCnt < COMMAND_MAX - 1);
    buttonList[buttonCnt].enabled = TRUE;
    buttonList[buttonCnt].options = options;
    buttonList[buttonCnt].group = cmdGroup;
    buttonList[buttonCnt].x = 0;
    buttonList[buttonCnt].y = 0;
    buttonList[buttonCnt].control = control;
    buttonList[buttonCnt].cmdInx = -1;
    LayoutSetPos(buttonCnt);
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
 * Initialize toolbar functions.
 * 
 */

EXPORT void InitToolbar(void)
{
    RegisterChangeNotification(ToolbarChange);
}
