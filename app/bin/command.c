/* file command.c
 * Main routine and initialization for the application
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



#include "command.h"
#include "cjoin.h"
#include "common.h"
#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "misc.h"
#include "param.h"
#include "include/paramfilelist.h"
#include "paths.h"
#include "smalldlg.h"
#include "track.h"
#include "common-ui.h"
#include "ctrain.h"
#include "menu.h"

#include <inttypes.h>

extern paramGroup_t menuPG;

/*****************************************************************************
 *
 * COMMAND
 *
 */

#define COMMAND_MAX (180)
#define BUTTON_MAX (180)
#define NUM_CMDMENUS (4)

static struct {
	wControl_p control;
	wBool_t enabled;
	wWinPix_t x, y;
	long options;
	int group;
	wIndex_t cmdInx;
} buttonList[BUTTON_MAX];
static int buttonCnt = 0;

static struct {
	procCommand_t cmdProc;
	char * helpKey;
	wIndex_t buttInx;
	char * labelStr;
	wIcon_p icon;
	int reqLevel;
	wBool_t enabled;
	long options;
	long stickyMask;
	long acclKey;
	wMenuPush_p menu[NUM_CMDMENUS];
	void * context;
} commandList[COMMAND_MAX];


static int commandCnt = 0;

static wIndex_t curCommand = 0;

EXPORT int cmdGroup;

static int log_command;
static long stickySet = 0;
static long stickyCnt = 0;
static void Sticky( void * );
static const char * stickyLabels[33];

#define TOOLBARSET_INIT				(0xFFFF)
EXPORT long toolbarSet = TOOLBARSET_INIT;
EXPORT wWinPix_t toolbarHeight = 0;
static wWinPix_t toolbarWidth = 0;
EXPORT long preSelect = 0;	/**< default command 0 = Describe 1 = Select */
EXPORT long rightClickMode = 0;
EXPORT void * commandContext;
EXPORT coOrd cmdMenuPos;

/*--------------------------------------------------------------------*/

extern wBalloonHelp_t balloonHelp[];

#ifdef DEBUG
#define CHECK_BALLOONHELP
/*#define CHECK_UNUSED_BALLOONHELP*/
#endif
#ifdef CHECK_UNUSED_BALLOONHELP
static void ShowUnusedBalloonHelp(void);
#endif


#ifdef CHECK_UNUSED_BALLOONHELP
int * balloonHelpCnts;
#endif

EXPORT const char * GetBalloonHelpStr(const char * helpKey) {
	wBalloonHelp_t * bh;
#ifdef CHECK_UNUSED_BALLOONHELP
	if ( balloonHelpCnts == NULL ) {
		for ( bh=balloonHelp; bh->name; bh++ );
		balloonHelpCnts = (int*)malloc( (sizeof *(int*)0) * (bh-balloonHelp) );
		memset( balloonHelpCnts, 0, (sizeof *(int*)0) * (bh-balloonHelp) );
	}
#endif
	for (bh = balloonHelp; bh->name; bh++) {
		if (strcmp(bh->name, helpKey) == 0) {
#ifdef CHECK_UNUSED_BALLOONHELP
			balloonHelpCnts[(bh-balloonHelp)]++;
#endif
			return _(bh->value);
		}
	}
#ifdef CHECK_BALLOONHELP
	fprintf( stderr, _("No balloon help for %s\n"), helpKey );
#endif
	return _("No Help");
}

#ifdef CHECK_UNUSED_BALLOONHELP
static void ShowUnusedBalloonHelp( void )
{
	int cnt;
	for ( cnt=0; balloonHelp[cnt].name; cnt++ )
	if ( balloonHelpCnts[cnt] == 0 )
	fprintf( stderr, "unused BH %s\n", balloonHelp[cnt].name );
}
#endif

/*--------------------------------------------------------------------*/
EXPORT const char* GetCurCommandName() {
	return commandList[curCommand].helpKey;
}

EXPORT void EnableCommands(void) {
	int inx, minx;
	wBool_t enable;

	LOG(log_command, 5,
			( "COMMAND enable S%d M%d\n", selectedTrackCount, programMode ))
	for (inx = 0; inx < commandCnt; inx++) {
		if (commandList[inx].buttInx) {
			if ((commandList[inx].options & IC_SELECTED)
					&& selectedTrackCount <= 0)
				enable = FALSE;
			else if ((programMode == MODE_TRAIN
					&& (commandList[inx].options
							& (IC_MODETRAIN_TOO | IC_MODETRAIN_ONLY)) == 0)
					|| (programMode != MODE_TRAIN
							&& (commandList[inx].options & IC_MODETRAIN_ONLY)
									!= 0))
				enable = FALSE;
			else
				enable = TRUE;
			if (commandList[inx].enabled != enable) {
				if (commandList[inx].buttInx >= 0)
					wControlActive(buttonList[commandList[inx].buttInx].control,
							enable);
				for (minx = 0; minx < NUM_CMDMENUS; minx++)
					if (commandList[inx].menu[minx])
						wMenuPushEnable(commandList[inx].menu[minx], enable);
				commandList[inx].enabled = enable;
			}
		}
	}

	for (inx = 0; inx < menuPG.paramCnt; inx++) {
		if (menuPG.paramPtr[inx].control == NULL)
			continue;
		if ((menuPG.paramPtr[inx].option & IC_SELECTED) && selectedTrackCount <= 0)
			enable = FALSE;
		else if ((programMode == MODE_TRAIN
				&& (menuPG.paramPtr[inx].option & (IC_MODETRAIN_TOO | IC_MODETRAIN_ONLY))
						== 0)
				|| (programMode != MODE_TRAIN
						&& (menuPG.paramPtr[inx].option & IC_MODETRAIN_ONLY) != 0))
			enable = FALSE;
		else
			enable = TRUE;
		wMenuPushEnable((wMenuPush_p) menuPG.paramPtr[inx].control, enable);
	}

	for (inx = 0; inx < buttonCnt; inx++) {
		if (buttonList[inx].cmdInx < 0
				&& (buttonList[inx].options & IC_SELECTED))
			wControlActive(buttonList[inx].control, selectedTrackCount > 0);
	}
}

EXPORT wIndex_t GetCurrentCommand() {
	return curCommand;
}

EXPORT void Reset(void) {
	if (recordF) {
		fprintf(recordF, "RESET\n");
		fflush(recordF);
	}
	LOG(log_command, 2,
			( "COMMAND CANCEL %s\n", commandList[curCommand].helpKey ))
	commandList[curCommand].cmdProc( C_CANCEL, zero);
	if (commandList[curCommand].buttInx >= 0)
		wButtonSetBusy(
				(wButton_p) buttonList[commandList[curCommand].buttInx].control,
				FALSE);
	curCommand = (preSelect ? selectCmdInx : describeCmdInx);
	wSetCursor(mainD.d, preSelect ? defaultCursor : wCursorQuestion);
	commandContext = commandList[curCommand].context;
	if (commandList[curCommand].buttInx >= 0)
		wButtonSetBusy(
				(wButton_p) buttonList[commandList[curCommand].buttInx].control,
				TRUE);
	tempSegs_da.cnt = 0;

	TryCheckPoint();

	ClrAllTrkBits( TB_UNDRAWN );
	DoRedraw(); // Reset
	EnableCommands();
	ResetMouseState();
	LOG(log_command, 1,
			( "COMMAND RESET %s\n", commandList[curCommand].helpKey ))
	(void) commandList[curCommand].cmdProc( C_START, zero);
}

static BOOL_T CheckClick(wAction_t *action, coOrd *pos, BOOL_T checkLeft,
		BOOL_T checkRight) {
	static long time0;
	static coOrd pos0;
	long time1;
	long timeDelta;
	DIST_T distDelta;

	switch (*action) {
	case C_LDOUBLE:
		if (!checkLeft)
			return TRUE;
		time0 = 0;
		break;
	case C_DOWN:
		if (!checkLeft)
			return TRUE;
		time0 = wGetTimer() - adjTimer;
		pos0 = *pos;
		return FALSE;
	case C_MOVE:
		if (!checkLeft)
			return TRUE;
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance(*pos, pos0);
			if (timeDelta > dragTimeout || distDelta > dragDistance) {
				time0 = 0;
				*pos = pos0;
				*action = C_DOWN;
			} else {
				return FALSE;
			}
		}
		break;
	case C_UP:
		if (!checkLeft)
			return TRUE;
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance(*pos, pos0);
			time0 = 0;
			*action = C_LCLICK;
		}
		break;
	case C_RDOWN:
		if (!checkRight)
			return TRUE;
		time0 = wGetTimer() - adjTimer;
		pos0 = *pos;
		return FALSE;
	case C_RMOVE:
		if (!checkRight)
			return TRUE;
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance(*pos, pos0);
			if (timeDelta > dragTimeout || distDelta > dragDistance) {
				time0 = 0;
				*pos = pos0;
				*action = C_RDOWN;
			} else {
				return FALSE;
			}
		}
		break;
	case C_RUP:
		if (!checkRight)
			return TRUE;
		if (time0 != 0) {
			time0 = 0;
			*action = C_RCLICK;
		}
		break;
	}
	return TRUE;
}

EXPORT wBool_t DoCurCommand(wAction_t action, coOrd pos) {
	wAction_t rc;
	int mode;
	wBool_t bExit = FALSE;

	if (action == wActionMove) {
		if ((commandList[curCommand].options & IC_WANT_MOVE) == 0) {
			bExit = TRUE;
		}
	} else if ((action&0xFF) == wActionModKey) {
		if ((commandList[curCommand].options & IC_WANT_MODKEYS) == 0) {
			bExit = TRUE;
		}
	} else if (!CheckClick(&action, &pos,
			(int) (commandList[curCommand].options & IC_LCLICK), TRUE)) {
		bExit = TRUE;
	} else if (action == C_RCLICK
			&& (commandList[curCommand].options & IC_RCLICK) == 0) {
		if (!inPlayback) {
			mode = MyGetKeyState();
			if ((mode & (~WKEY_SHIFT)) != 0) {
				wBeep();
				bExit = TRUE;
			} else if (((mode & WKEY_SHIFT) == 0) == (rightClickMode == 0)) {
				if (selectedTrackCount > 0) {
					if (commandList[curCommand].options & IC_CMDMENU) {
					}
					wMenuPopupShow(popup2M);
				} else {
					wMenuPopupShow(popup1M);
				}
				bExit = TRUE;
			} else if ((commandList[curCommand].options & IC_CMDMENU)) {
				cmdMenuPos = pos;
				action = C_CMDMENU;
			} else {
				wBeep();
				bExit = TRUE;
			}
		} else {
			bExit = TRUE;
		}
	}
	if ( bExit ) {
		TempRedraw(); // DoCurCommand: precommand
		return C_CONTINUE;
	}

	LOG(log_command, 2,
			( "COMMAND MOUSE %s %d @ %0.3f %0.3f\n", commandList[curCommand].helpKey, (int)action, pos.x, pos.y ))
	rc = commandList[curCommand].cmdProc(action, pos);
	LOG(log_command, 4, ( "    COMMAND returns %d\n", rc ))
	switch ( action & 0xFF ) {
	case wActionMove:
	case wActionModKey:
	case C_DOWN:
	case C_MOVE:
	case C_UP:
	case C_RDOWN:
	case C_RMOVE:
	case C_RUP:
	case C_LCLICK:
	case C_RCLICK:
	case C_TEXT:
	case C_OK:
		if (rc== C_TERMINATE) MainRedraw();
		else TempRedraw(); // DoCurCommand: postcommand
		break;
	default:
		break;
	}
	if ((rc == C_TERMINATE || rc == C_INFO)
			&& (commandList[curCommand].options & IC_STICKY)
			&& (commandList[curCommand].stickyMask & stickySet)) {
		tempSegs_da.cnt = 0;
		UpdateAllElevations();
		if (commandList[curCommand].options & IC_NORESTART) {
			return C_CONTINUE;
		}
		//Make sure we checkpoint even sticky commands
		TryCheckPoint();
		LOG(log_command, 1,
				( "COMMAND START %s\n", commandList[curCommand].helpKey ))
		wSetCursor(mainD.d,defaultCursor);
		rc = commandList[curCommand].cmdProc( C_START, pos);
		LOG(log_command, 4, ( "    COMMAND returns %d\n", rc ))
		switch (rc) {
		case C_CONTINUE:
			break;
		case C_ERROR:
			Reset();
#ifdef VERBOSE
			lprintf( "Start returns Error");
#endif
			break;
		case C_TERMINATE:
			InfoMessage("");
		case C_INFO:
			Reset();
			break;
		}
	}
	return rc;
}

/*
 * \parm reset says if the user used Esc rather than undo/redo
 */
EXPORT int ConfirmReset(BOOL_T retry) {
	wAction_t rc;
	if (curCommand != describeCmdInx) {
		LOG(log_command, 3,
				( "COMMAND CONFIRM %s\n", commandList[curCommand].helpKey ))
		rc = commandList[curCommand].cmdProc( C_CONFIRM, zero);
		LOG(log_command, 4, ( "    COMMAND returns %d\n", rc ))
		if (rc == C_ERROR) {
			if (retry)
				rc =
						wNotice3(
								_(
										"Cancelling the current command will undo the changes\n"
												"you are currently making. Do you want to do the update instead?"),
								_("Yes"), _("No"), _("Cancel"));
			else
				rc =
						wNoticeEx( NT_WARNING,
								_(
										"Cancelling the current command will undo the changes\n"
												"you are currently making. Do you want to do the update instead?"),
								_("Yes"), _("No"));
			if (rc == 1) {
				LOG(log_command, 3,
						( "COMMAND OK %s\n", commandList[curCommand].helpKey ))
				commandList[curCommand].cmdProc( C_OK, zero);
				return C_OK;
			} else if (rc == -1) {
				return C_CANCEL;
			}
		} else if (rc == C_TERMINATE) {
			return C_TERMINATE;
		}
	}
	if (retry) {
		/* because user pressed esc */
		SetAllTrackSelect( FALSE);
	}
	Reset();
	LOG(log_command, 1,
			( "COMMAND RESET %s\n", commandList[curCommand].helpKey ))
	commandList[curCommand].cmdProc( C_START, zero);
	return C_CONTINUE;
}

EXPORT void DoCommandB(void * data) {
	wIndex_t inx = (wIndex_t)VP2L(data);
	STATUS_T rc;
	static coOrd pos = { 0, 0 };
	static int inDoCommandB = FALSE;
	wIndex_t buttInx;

	if (inDoCommandB)
		return;
	inDoCommandB = TRUE;

	if (inx < 0 || inx >= commandCnt) {
		CHECK(FALSE);
		inDoCommandB = FALSE;
		return;
	}

	if ((!inPlayback) && (!commandList[inx].enabled)) {
		ErrorMessage(MSG_COMMAND_DISABLED);
		inx = describeCmdInx;
	}

	InfoMessage("");
	if (curCommand != selectCmdInx) {
		LOG(log_command, 3,
				( "COMMAND FINISH %s\n", commandList[curCommand].helpKey ))
		rc = commandList[curCommand].cmdProc( C_FINISH, zero);
		LOG(log_command, 3,
				( "COMMAND CONFIRM %s\n", commandList[curCommand].helpKey ))
		rc = commandList[curCommand].cmdProc( C_CONFIRM, zero);
		LOG(log_command, 4, ( "    COMMAND returns %d\n", rc ))
		if (rc == C_ERROR) {
			rc = wNotice3(
					_("Cancelling the current command will undo the changes\n"
							"you are currently making. Do you want to update?"),
					_("Yes"), _("No"), _("Cancel"));
			if (rc == 1)
				commandList[curCommand].cmdProc( C_OK, zero);
			else if (rc == -1) {
				inDoCommandB = FALSE;
				return;
			}
		}
		LOG(log_command, 3,
				( "COMMAND CANCEL %s\n", commandList[curCommand].helpKey ))
		commandList[curCommand].cmdProc( C_CANCEL, pos);
		tempSegs_da.cnt = 0;
	} else {
		LOG(log_command, 3,
				( "COMMAND FINISH %s\n", commandList[curCommand].helpKey ))
		rc = commandList[curCommand].cmdProc( C_FINISH, zero);
	}
	if (commandList[curCommand].buttInx >= 0)
		wButtonSetBusy(
				(wButton_p) buttonList[commandList[curCommand].buttInx].control,
				FALSE);

	if (recordF) {
		fprintf(recordF, "COMMAND %s\n", commandList[inx].helpKey + 3);
		fflush(recordF);
	}

	curCommand = inx;
	commandContext = commandList[curCommand].context;
	if ((buttInx = commandList[curCommand].buttInx) >= 0) {
		if (buttonList[buttInx].cmdInx != curCommand) {
			wButtonSetLabel((wButton_p) buttonList[buttInx].control,
					(char*) commandList[curCommand].icon);
			wControlSetHelp(buttonList[buttInx].control,
					GetBalloonHelpStr(commandList[curCommand].helpKey));
			wControlSetContext(buttonList[buttInx].control,
					I2VP(curCommand));
			buttonList[buttInx].cmdInx = curCommand;
		}
		wButtonSetBusy(
				(wButton_p) buttonList[commandList[curCommand].buttInx].control,
				TRUE);
	}
	LOG(log_command, 1,
			( "COMMAND START %s\n", commandList[curCommand].helpKey ))
	wSetCursor(mainD.d,defaultCursor);
	rc = commandList[curCommand].cmdProc( C_START, pos);
	LOG(log_command, 4, ( "    COMMAND returns %d\n", rc ))
	TempRedraw(); // DoCommandB
	switch (rc) {
	case C_CONTINUE:
		break;
	case C_ERROR:
		Reset();
#ifdef VERBOSE
		lprintf( "Start returns Error");
#endif
		break;
	case C_TERMINATE:
	case C_INFO:
		if (rc == C_TERMINATE)
			InfoMessage("");
		Reset();
		break;
	}
	inDoCommandB = FALSE;
}

static void LayoutSetPos(wIndex_t inx) {
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
		if (toolbarRowHeight <= 0)
			toolbarRowHeight = wControlGetHeight(buttonList[inx].control);

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
			if (h<toolbarRowHeight) {
				offset = (h-toolbarRowHeight)/2;
				h = toolbarRowHeight;  //Uniform
			} else offset = 0;
			if (inx < buttonCnt - 1 && (buttonList[inx + 1].options & IC_ABUT))
				w += wControlGetWidth(buttonList[inx + 1].control);
			if (toolbarWidth + w > width - 20) {
				toolbarWidth = 0;
				toolbarHeight += h + 5;
			}
			if ((currGroup == BG_LAYER) && layerButtNumber>1 && GetLayerHidden(layerButtNumber-2) ) {
				wControlShow(buttonList[inx].control, FALSE);
				layerButtNumber++;
			} else {
				if (currGroup == BG_LAYER ) {
					if (layerButtNumber>1) layerButtCnt++; // Ignore List and Background
					layerButtNumber++;
				}
				wControlSetPos(buttonList[inx].control, toolbarWidth,
					toolbarHeight - (h + 5 +offset));
				buttonList[inx].x = toolbarWidth;
				buttonList[inx].y = toolbarHeight - (h + 5 + offset);
				toolbarWidth += wControlGetWidth(buttonList[inx].control);
				wControlShow(buttonList[inx].control, TRUE);
			}
		} else {
			wControlShow(buttonList[inx].control, FALSE);
		}
	}
}

EXPORT void LayoutToolBar( void * data )
{
	int inx;

	for (inx = 0; inx < buttonCnt; inx++) {
		LayoutSetPos(inx);
	}
	if (toolbarSet&(1<<BG_HOTBAR)) {
		LayoutHotBar(data);
	} else {
		HideHotBar();
	}
}

static void ToolbarChange(long changes) {
	if ((changes & CHANGE_TOOLBAR)) {
		/*if ( !(changes&CHANGE_MAIN) )*/
				MainProc( mainW, wResize_e, NULL, NULL );
		/*else
		 LayoutToolBar();*/
	}
}

/***************************************************************************
 *
 *
 *
 */

EXPORT BOOL_T CommandEnabled(wIndex_t cmdInx) {
	return commandList[cmdInx].enabled;
}

static wIndex_t AddCommand(procCommand_t cmdProc, const char * helpKey,
		const char * nameStr, wIcon_p icon, int reqLevel, long options, long acclKey,
		void * context) {
	CHECK( commandCnt < COMMAND_MAX - 1 );
	commandList[commandCnt].labelStr = MyStrdup(nameStr);
	commandList[commandCnt].helpKey = MyStrdup(helpKey);
	commandList[commandCnt].cmdProc = cmdProc;
	commandList[commandCnt].icon = icon;
	commandList[commandCnt].reqLevel = reqLevel;
	commandList[commandCnt].enabled = TRUE;
	commandList[commandCnt].options = options;
	commandList[commandCnt].acclKey = acclKey;
	commandList[commandCnt].context = context;
	commandList[commandCnt].buttInx = -1;
	commandList[commandCnt].menu[0] = NULL;
	commandList[commandCnt].menu[1] = NULL;
	commandList[commandCnt].menu[2] = NULL;
	commandList[commandCnt].menu[3] = NULL;
	commandCnt++;
	return commandCnt - 1;
}

EXPORT void AddToolbarControl(wControl_p control, long options) {
	CHECK( buttonCnt < COMMAND_MAX - 1 );
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

EXPORT wButton_p AddToolbarButton(const char * helpStr, wIcon_p icon, long options,
		wButtonCallBack_p action, void * context) {
	wButton_p bb;
	wIndex_t inx;

	GetBalloonHelpStr(helpStr);
	if (context == NULL) {
		for (inx = 0; inx < menuPG.paramCnt; inx++) {
			if (action != DoCommandB && menuPG.paramPtr[inx].valueP == I2VP(action)) {
				context = &menuPG.paramPtr[inx];
				action = ParamMenuPush;
				menuPG.paramPtr[inx].context = I2VP(buttonCnt);
				menuPG.paramPtr[inx].option |= IC_PLAYBACK_PUSH;
				break;
			}
		}
	}
	bb = wButtonCreate(mainW, 0, 0, helpStr, (char*) icon,
	BO_ICON/*|((options&IC_CANCEL)?BB_CANCEL:0)*/, 0, action, context);
	AddToolbarControl((wControl_p) bb, options);
	return bb;
}

EXPORT void PlaybackButtonMouse(wIndex_t buttInx) {
	wWinPix_t cmdX, cmdY;
	coOrd pos;

	if (buttInx < 0 || buttInx >= buttonCnt)
		return;
	if (buttonList[buttInx].control == NULL)
		return;
	cmdX = buttonList[buttInx].x + 17;
	cmdY = toolbarHeight - (buttonList[buttInx].y + 17)
			+ (wWinPix_t) (mainD.size.y / mainD.scale * mainD.dpi) + 30;

	mainD.Pix2CoOrd( &mainD, cmdX, cmdY, &pos );
	MovePlaybackCursor(&mainD, pos, TRUE, buttonList[buttInx].control);
	if (playbackTimer == 0) {
		wButtonSetBusy((wButton_p) buttonList[buttInx].control, TRUE);
		wFlush();
		wPause(500);
		wButtonSetBusy((wButton_p) buttonList[buttInx].control, FALSE);
		wFlush();
	}
}

#include "bitmaps/down.xpm"
static const char * buttonGroupMenuTitle;
static const char * buttonGroupHelpKey;
static const char * buttonGroupStickyLabel;
static wMenu_p buttonGroupPopupM;

EXPORT void ButtonGroupBegin(const char * menuTitle, const char * helpKey,
		const char * stickyLabel) {
	buttonGroupMenuTitle = menuTitle;
	buttonGroupHelpKey = helpKey;
	buttonGroupStickyLabel = stickyLabel;
	buttonGroupPopupM = NULL;
}

EXPORT void ButtonGroupEnd(void) {
	buttonGroupMenuTitle = NULL;
	buttonGroupHelpKey = NULL;
	buttonGroupPopupM = NULL;
}

EXPORT wIndex_t AddMenuButton(wMenu_p menu, procCommand_t command,
		const char * helpKey, const char * nameStr, wIcon_p icon, int reqLevel,
		long options, long acclKey, void * context) {
	wIndex_t buttInx = -1;
	wIndex_t cmdInx;
	BOOL_T newButtonGroup = FALSE;
	wMenu_p tm, p1m, p2m;
	static wIcon_p openbuttIcon = NULL;
	static wMenu_p commandsSubmenu;
	static wMenu_p popup1Submenu;
	static wMenu_p popup2Submenu;

	if (icon) {
		if (buttonGroupPopupM != NULL) {
			buttInx = buttonCnt - 2;
		} else {
			buttInx = buttonCnt;
			AddToolbarButton(helpKey, icon, options,
					DoCommandB,
					I2VP(commandCnt));
			buttonList[buttInx].cmdInx = commandCnt;
		}
		if (buttonGroupMenuTitle != NULL && buttonGroupPopupM == NULL) {
			if (openbuttIcon == NULL)
				openbuttIcon = wIconCreatePixMap(down_xpm[iconSize]);
			buttonGroupPopupM = wMenuPopupCreate(mainW, buttonGroupMenuTitle);
			AddToolbarButton(buttonGroupHelpKey, openbuttIcon, IC_ABUT,
					(wButtonCallBack_p) wMenuPopupShow,
					buttonGroupPopupM);
			newButtonGroup = TRUE;
			commandsSubmenu = wMenuMenuCreate(menu, "", buttonGroupMenuTitle);
			if (options & IC_POPUP2) {
				popup1Submenu = wMenuMenuCreate(popup1aM, "",	buttonGroupMenuTitle);
				popup2Submenu = wMenuMenuCreate(popup2aM, "",	buttonGroupMenuTitle);
			} else if (options & IC_POPUP3) {
				popup1Submenu= wMenuMenuCreate(popup1mM, "",	buttonGroupMenuTitle);
				popup2Submenu = wMenuMenuCreate(popup2mM, "",	buttonGroupMenuTitle);

			} else {
				popup1Submenu = wMenuMenuCreate(popup1M, "",	buttonGroupMenuTitle);
				popup2Submenu = wMenuMenuCreate(popup2M, "",	buttonGroupMenuTitle);
			}
		}
	}
	cmdInx = AddCommand(command, helpKey, nameStr, icon, reqLevel, options,
			acclKey, context);
	commandList[cmdInx].buttInx = buttInx;
	if (nameStr[0] == '\0')
		return cmdInx;
	if (commandList[cmdInx].options & IC_STICKY) {
		if (buttonGroupPopupM == NULL || newButtonGroup) {
			CHECK( stickyCnt <= 32 );
			stickyCnt++;
		}
		if (buttonGroupPopupM == NULL) {
			stickyLabels[stickyCnt - 1] = nameStr;
		} else {
			stickyLabels[stickyCnt - 1] = buttonGroupStickyLabel;
		}
		stickyLabels[stickyCnt] = NULL;
		long stickyMask = 1L<<(stickyCnt-1);
		commandList[cmdInx].stickyMask = stickyMask;
		if ( ( commandList[cmdInx].options & IC_INITNOTSTICKY ) == 0 )
			stickySet |= stickyMask;
	}
	if (buttonGroupPopupM) {
		commandList[cmdInx].menu[0] = wMenuPushCreate(buttonGroupPopupM,
				helpKey, GetBalloonHelpStr(helpKey), 0, DoCommandB,
				I2VP(cmdInx));
		tm = commandsSubmenu;
		p1m = popup1Submenu;
		p2m = popup2Submenu;
	} else {
		tm = menu;
		p1m = (options & IC_POPUP2) ? popup1aM : (options & IC_POPUP3) ? popup1mM : popup1M;
		p2m = (options & IC_POPUP2) ? popup2aM : (options & IC_POPUP3) ? popup2mM : popup2M;
	}
	commandList[cmdInx].menu[1] = wMenuPushCreate(tm, helpKey, nameStr, acclKey,
			DoCommandB, I2VP(cmdInx));
	if ((options & (IC_POPUP | IC_POPUP2 | IC_POPUP3))) {
		if (!(options & IC_SELECTED)) {
			commandList[cmdInx].menu[2] = wMenuPushCreate(p1m, helpKey, nameStr,
					0, DoCommandB, I2VP(cmdInx));
		}
		commandList[cmdInx].menu[3] = wMenuPushCreate(p2m, helpKey, nameStr, 0,
				DoCommandB, I2VP(cmdInx));
	}

	return cmdInx;
}

EXPORT wIndex_t InitCommand(wMenu_p menu, procCommand_t command, const char * nameStr,
		const char * bits, int reqLevel, long options, long acclKey) {
	char helpKey[STR_SHORT_SIZE];
	wIcon_p icon = NULL;
	if (bits)
		icon = wIconCreateBitMap(16, 16, bits, wDrawColorBlack);
	strcpy(helpKey, "cmd");
	strcat(helpKey, nameStr);
	return AddMenuButton(menu, command, helpKey, _(nameStr), icon, reqLevel,
			options, acclKey, NULL);
}

/*--------------------------------------------------------------------*/

EXPORT void PlaybackCommand(const char * line, wIndex_t lineNum) {
	size_t inx;
	wIndex_t buttInx;
	size_t len1, len2;
	len1 = strlen(line + 8);
	for (inx = 0; inx < commandCnt; inx++) {
		len2 = strlen(commandList[inx].helpKey + 3);
		if (len1 == len2
				&& strncmp(line + 8, commandList[inx].helpKey + 3, len2) == 0) {
			break;
		}
	}
	if (inx >= commandCnt) {
		fprintf(stderr, "Unknown playback COMMAND command %d : %s\n", lineNum,
				line);
	} else {
		wWinPix_t cmdX, cmdY;
		coOrd pos;
		if ((buttInx = commandList[inx].buttInx) >= 0) {
			cmdX = buttonList[buttInx].x + 17;
			cmdY = toolbarHeight - (buttonList[buttInx].y + 17)
					+ (wWinPix_t) (mainD.size.y / mainD.scale * mainD.dpi) + 30;
			mainD.Pix2CoOrd( &mainD, cmdX, cmdY, &pos );
			MovePlaybackCursor(&mainD, pos,TRUE,buttonList[buttInx].control);
		}
		if (strcmp(line + 8, "Undo") == 0) {
			if (buttInx > 0 && playbackTimer == 0) {
				wButtonSetBusy((wButton_p) buttonList[buttInx].control, TRUE);
				wFlush();
				wPause(500);
				wButtonSetBusy((wButton_p) buttonList[buttInx].control, FALSE);
				wFlush();
			}
			UndoUndo(NULL);
		} else if (strcmp(line + 8, "Redo") == 0) {
			if (buttInx >= 0 && playbackTimer == 0) {
				wButtonSetBusy((wButton_p) buttonList[buttInx].control, TRUE);
				wFlush();
				wPause(500);
				wButtonSetBusy((wButton_p) buttonList[buttInx].control, FALSE);
				wFlush();
			}
			UndoRedo(NULL);
		} else {
			if (buttInx >= 0 && playbackTimer == 0) {
				wButtonSetBusy((wButton_p) buttonList[buttInx].control, TRUE);
				wFlush();
				wPause(500);
				wButtonSetBusy((wButton_p) buttonList[buttInx].control, FALSE);
				wFlush();
			}
			DoCommandB(I2VP(inx));
		}
	}
}

/*--------------------------------------------------------------------*/


static wWin_p stickyW;

EXPORT BOOL_T IsCurCommandSticky(void) {
	if ((commandList[curCommand].options & IC_STICKY) != 0
		&& (commandList[curCommand].stickyMask & stickySet) != 0)
		return TRUE;
	return FALSE;
}

EXPORT void ResetIfNotSticky(void) {
	if ((commandList[curCommand].options & IC_STICKY) == 0
			|| (commandList[curCommand].stickyMask & stickySet) == 0)
		Reset();
}


static paramData_t stickyPLs[] = { { PD_TOGGLE, &stickySet, "set", 0,
		stickyLabels } };
static paramGroup_t stickyPG = { "sticky", PGO_RECORD, stickyPLs,
		COUNT( stickyPLs ) };

static void StickyOk(void * unused) {
	wHide(stickyW);
}


EXPORT void DoSticky(void * unused) {
	if (!stickyW)
		stickyW = ParamCreateDialog(&stickyPG,
				MakeWindowTitle(_("Sticky Commands")), _("Ok"), StickyOk, wHide,
				TRUE, NULL, 0, NULL);
	ParamLoadControls(&stickyPG);
	wShow(stickyW);
}

/*--------------------------------------------------------------------*/
EXPORT void CommandInit( void )
{
	wSetBalloonHelp( balloonHelp );
	curCommand = describeCmdInx;
	commandContext = commandList[curCommand].context;
	wPrefGetInteger( "DialogItem", "sticky-set", &stickySet, stickySet );
	ParamRegister(&stickyPG);
	log_command = LogFindIndex( "command" );
	RegisterChangeNotification(ToolbarChange);
}

