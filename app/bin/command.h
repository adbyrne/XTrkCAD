/** \file command.h
 * Application wide declarations and defines
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

#ifndef COMMAND_H
#define COMMAND_H

#include "common.h"

#define IC_STICKY               (1<<0)
#define IC_INITNOTSTICKY        (1<<1)
#define IC_CANCEL               (1<<2)
#define IC_MENU                 (1<<3)
#define IC_NORESTART            (1<<4)
#define IC_SELECTED             (1<<5)
#define IC_POPUP                (1<<6)
#define IC_LCLICK               (1<<7)
#define IC_RCLICK               (1<<8)
#define IC_CMDMENU              (1<<9)
#define IC_POPUP2               (1<<10)
#define IC_ABUT                 (1<<11)
#define IC_ACCLKEY              (1<<12)
#define IC_MODETRAIN_TOO        (1<<13)
#define IC_MODETRAIN_ONLY       (1<<14)
#define IC_WANT_MOVE            (1<<15)
#define IC_PLAYBACK_PUSH        (1<<16)
#define IC_WANT_MODKEYS         (1<<17)
#define IC_POPUP3				(1<<18)


/*
 * Command Action
 */
#define C_DOWN			wActionLDown
#define C_MOVE			wActionLDrag
#define C_UP			wActionLUp
#define C_RDOWN			wActionRDown
#define C_RMOVE			wActionRDrag
#define C_RUP			wActionRUp
#define C_TEXT			wActionText
#define C_WUP			wActionWheelUp
#define C_WDOWN			wActionWheelDown
#define C_LDOUBLE       wActionLDownDouble
#define C_MODKEY        wActionModKey
#define C_SCROLLUP	    wActionScrollUp
#define C_SCROLLDOWN    wActionScrollDown
#define C_SCROLLLEFT	wActionScrollLeft
#define C_SCROLLRIGHT   wActionScrollRight
#define C_INIT			(wActionLast+1)
#define C_START			(wActionLast+2)
#define C_REDRAW		(wActionLast+3)
#define C_CANCEL		(wActionLast+4)
#define C_OK			(wActionLast+5)
#define C_CONFIRM		(wActionLast+6)
#define C_LCLICK		(wActionLast+7)
#define C_RCLICK		(wActionLast+8)
#define C_CMDMENU		(wActionLast+9)
#define C_FINISH		(wActionLast+10)
#define C_UPDATE        (wActionLast+11)

#define C_CONTINUE		(100)
#define C_TERMINATE		(101)
#define C_INFO			(102)
#define C_ERROR			(103)

/*
 * Command Levels - obsolete
 */
#define LEVEL0			(0)
#define LEVEL0_50		(1)
#define LEVEL1			(2)
#define LEVEL2			(3)

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
#define BG_RULER		(8)
#define BG_LAYER		(9)
#define BG_HOTBAR		(10)
#define BG_SNAP			(11)
#define BG_TRAIN		(12)
#define BG_COUNT		(13)
#define BG_FILE			(14)
#define BG_CONTROL		(15)
#define BG_EXPORTIMPORT (16)
#define BG_PRINT		(17)
#define BG_BIGGAP		(1<<8)
extern int cmdGroup;


extern int buttonCnt;
extern int commandCnt;
extern int cmdGroup;
extern long toolbarSet;
extern wWinPix_t toolbarHeight;
extern long preSelect;
extern long rightClickMode;
extern void * commandContext;
extern coOrd cmdMenuPos;

const char * GetCurCommandName( void );
void EnableCommands( void );
wIndex_t GetCurrentCommand(void);
void Reset( void );
wBool_t DoCurCommand( wAction_t, coOrd );
int ConfirmReset( BOOL_T );
void DoCommandB( void * );
void LayoutToolBar( void * );
BOOL_T CommandEnabled( wIndex_t );
#define NUM_CMDMENUS (4)
wIndex_t AddCommand(procCommand_t cmdProc, const char * helpKey,
		const char * nameStr, wIcon_p icon, int reqLevel, long options, long acclKey,
		wIndex_t buttInx, long stickyMask, wMenuPush_p cmdMenus[NUM_CMDMENUS], void * context);
void AddToolbarControl( wControl_p, long );
void PlaybackButtonMouse( wIndex_t );
void PlaybackCommand( const char *, wIndex_t );
BOOL_T IsCurCommandSticky(void);
void ResetIfNotSticky( void );
void CommandInit( void );
#endif
