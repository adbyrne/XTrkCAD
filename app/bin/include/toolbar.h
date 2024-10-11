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
#define BG_LAYER		(9)
#define BG_HOTBAR		(10)
#define BG_SNAP			(11)
#define BG_TRAIN		(12)
#define BG_COUNT		(13)
#define BG_FILE			(14)
#define BG_CONTROL		(15)
#define BG_EXPORTIMPORT (16)
#define BG_PRINT		(17)
#define BG_RULER		(18)
#define BG_BIGGAP       (1<<15)

extern void InitToolbar(void);
extern void ToolbarLayout(void* data);
extern void DoToolbar(void* unused);
extern bool ToolbarIsGroupVisible(int group);
extern wWinPix_t ToolbarGetHeight(void);
extern void ToolbarSetHeight(wWinPix_t newHeight);

extern void InitToolbar(void);
extern void ToolbarButtonBusy(wIndex_t button, wBool_t busy);
extern void ToolbarButtonEnable(wIndex_t button, wBool_t enable);
extern void ToolbarButtonEnableIfSelect(bool selected);
extern void ToolbarButtonCommandLink(wIndex_t button, int command);
extern void ToolbarUpdateButton(wIndex_t button, wIndex_t command,
    char* icon, const char* helpKey, void* context);
extern void ToolbarButtonPlayback(wIndex_t buttonInx);
extern void ToolbarLoadConfig(void);
extern void ToolbarControlAdd(wControl_p parent, wControl_p control, long options, int cmdGroup);


wIndex_t AddCommand(procCommand_t cmdProc, const char* helpKey,
    const char* nameStr, wIcon_p icon, int reqLevel, long options, long acclKey,
    wIndex_t buttInx, long stickyMask, wControl_p cmdMenus[NUM_CMDMENUS],
    void* context);

void PlaybackButtonMouse(wIndex_t);
void PlaybackCommand(const char*, wIndex_t);
#endif
