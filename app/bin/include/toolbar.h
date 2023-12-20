/*****************************************************************//**
 * \file   toolbar.h
 * \brief  Header for toolbar functions
 *********************************************************************/

#ifndef TOOLBAR_H
#define TOOLBAR_H
 /*
  * Command groups
  */
#define BG_SELECT		(0)
#define BG_ZOOM			(1)
#define BG_UNDO			(2)
#define BG_EASE			(3)
#define BG_TRKCRT		(4)
#define BG_TRKMOD		(5)
#define BG_TRKGRP		(6)
#define BG_MISCCRT		(7)
#define BG_RULER		(18)
#define BG_LAYER		(9)
#define BG_HOTBAR		(10)
#define BG_SNAP			(11)
#define BG_TRAIN		(12)
#define BG_COUNT		(13)
#define BG_FILE			(14)
#define BG_CONTROL		(15)
#define BG_EXPORTIMPORT (16)
#define BG_PRINT		(17)

void InitToolbar(void);
EXPORT void ToolbarLayout(void* data);
EXPORT void DoToolbar(void* unused);
EXPORT bool ToolbarIsGroupVisible(int group);
EXPORT wWinPix_t ToolbarGetHeight(void);
EXPORT void ToolbarSetHeight(wWinPix_t newHeight);

EXPORT void ToolbarButtonBusy(wIndex_t button, wBool_t busy);
EXPORT void ToolbarButtonEnable(wIndex_t button, wBool_t enable);
EXPORT void ToolbarButtonEnableIfSelect(bool selected);
EXPORT void ToolbarButtonCommandLink(wIndex_t button, int command);
EXPORT void ToolbarUpdateButton(wIndex_t button, wIndex_t command,
    char* icon, const char* helpKey, void* context);
EXPORT void ToolbarButtonPlayback(wIndex_t buttonInx);
EXPORT void ToolbarLoadConfig(void);
EXPORT void ToolbarControlAdd(wControl_p control, long options, int cmdGroup);


wIndex_t AddCommand(procCommand_t cmdProc, const char* helpKey,
    const char* nameStr, wIcon_p icon, int reqLevel, long options, long acclKey,
    wIndex_t buttInx, long stickyMask, wMenuPush_p cmdMenus[NUM_CMDMENUS],
    void* context);

void PlaybackButtonMouse(wIndex_t);
void PlaybackCommand(const char*, wIndex_t);
#endif
