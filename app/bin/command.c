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
#include "common.h"
#include "cselect.h"
#include "cundo.h"
#include "draw.h"
#include "fileio.h"
#include "track.h"
#include "common-ui.h"
#include "menu.h"

/*****************************************************************************
 *
 * COMMAND
 *
 */

#define COMMAND_MAX (250)

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


EXPORT int commandCnt = 0;

static wIndex_t curCommand = 0;

static int log_command;


EXPORT long preSelect = 0;	/**< default command 0 = Describe 1 = Select */
EXPORT long rightClickMode = 0;
EXPORT void * commandContext;
EXPORT coOrd cmdMenuPos;

/*--------------------------------------------------------------------*/
EXPORT const char* GetCurCommandName()
{
	return commandList[curCommand].helpKey;
}

/** 
 * Decide whether command is available in the current application mode.
 * Basically track modifications are not available in Train Mode, file 
 * operations are always available and train control ops are available in
 * train mode only. 
 * The specific logic was developed with the help of Wolfram Alpha:
 * CNF | ((NOT m) OR o OR t) AND(m OR (NOT o) OR t)
 * 
 * \param mode		application mode
 * \param options	availability options
 * \return			true for enabled, false if disabled	
 */

EXPORT bool IsCommandEnabled(long mode, long options)
{
	if (((mode == MODE_DESIGN) || (options & IC_MODETRAIN_ONLY) ||
		(options & IC_MODETRAIN_TOO)) &&
		((mode == MODE_TRAIN) || !(options & IC_MODETRAIN_ONLY)) ||
		(options & IC_MODETRAIN_TOO)) {
		return true;
	}

	return false;
}

EXPORT void EnableCommands(void)
{
	int inx, minx;
	wBool_t enable;

	LOG(log_command, 5,
	    ( "COMMAND enable S%d M%d\n", selectedTrackCount, programMode ))
	for (inx = 0; inx < commandCnt; inx++) {
		if (commandList[inx].buttInx) {
			if ((commandList[inx].options & IC_SELECTED)
			    && selectedTrackCount <= 0) {
				enable = FALSE;
			} else if (IsCommandEnabled(programMode, commandList[inx].options )) {
				enable = TRUE;
			} else {
				enable = FALSE;
			}
			if (commandList[inx].enabled != enable) {
				if (commandList[inx].buttInx >= 0)
					ToolbarButtonEnable(commandList[inx].buttInx, enable );
				for (minx = 0; minx < NUM_CMDMENUS; minx++)
					if (commandList[inx].menu[minx]) {
						wMenuPushEnable(commandList[inx].menu[minx], enable);
					}
				commandList[inx].enabled = enable;
			}
		}
	}

	EnableMenus();

	ToolbarButtonEnableIfSelect(selectedTrackCount > 0);
}

EXPORT wIndex_t GetCurrentCommand()
{
	return curCommand;
}

EXPORT void Reset(void)
{
	if (recordF) {
		fprintf(recordF, "RESET\n");
		fflush(recordF);
	}
	LOG(log_command, 2,
	    ( "COMMAND CANCEL %s\n", commandList[curCommand].helpKey ))
	commandList[curCommand].cmdProc( C_CANCEL, zero);
	if (commandList[curCommand].buttInx >= 0)
		ToolbarButtonBusy(commandList[curCommand].buttInx, FALSE);
	curCommand = (preSelect ? selectCmdInx : describeCmdInx);
	wSetCursor(mainD.d, preSelect ? defaultCursor : wCursorQuestion);
	commandContext = commandList[curCommand].context;
	if (commandList[curCommand].buttInx >= 0)
		ToolbarButtonBusy(commandList[curCommand].buttInx, TRUE);

	DYNARR_RESET( trkSeg_t, tempSegs_da );

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
                         BOOL_T checkRight)
{
	static long time0;
	static coOrd pos0;
	long time1;
	long timeDelta;
	DIST_T distDelta;

	switch (*action) {
	case C_LDOUBLE:
		if (!checkLeft) {
			return TRUE;
		}
		time0 = 0;
		break;
	case C_DOWN:
		if (!checkLeft) {
			return TRUE;
		}
		time0 = wGetTimer() - adjTimer;
		pos0 = *pos;
		return FALSE;
	case C_MOVE:
		if (!checkLeft) {
			return TRUE;
		}
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
		if (!checkLeft) {
			return TRUE;
		}
		if (time0 != 0) {
			time1 = wGetTimer() - adjTimer;
			timeDelta = time1 - time0;
			distDelta = FindDistance(*pos, pos0);
			time0 = 0;
			*action = C_LCLICK;
		}
		break;
	case C_RDOWN:
		if (!checkRight) {
			return TRUE;
		}
		time0 = wGetTimer() - adjTimer;
		pos0 = *pos;
		return FALSE;
	case C_RMOVE:
		if (!checkRight) {
			return TRUE;
		}
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
		if (!checkRight) {
			return TRUE;
		}
		if (time0 != 0) {
			time0 = 0;
			*action = C_RCLICK;
		}
		break;
	}
	return TRUE;
}

EXPORT wBool_t DoCurCommand(wAction_t action, coOrd pos)
{
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
	    ( "COMMAND MOUSE %s %d @ %0.3f %0.3f\n", commandList[curCommand].helpKey,
	      (int)action, pos.x, pos.y ))
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
		if (rc== C_TERMINATE) { MainRedraw(); }
		else { TempRedraw(); } // DoCurCommand: postcommand
		break;
	default:
		break;
	}
	if ((rc == C_TERMINATE || rc == C_INFO)
	    && (commandList[curCommand].options & IC_STICKY)
	    && (commandList[curCommand].stickyMask & stickySet)) {
		DYNARR_RESET( trkSeg_t, tempSegs_da );
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
EXPORT int ConfirmReset(BOOL_T retry)
{
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

EXPORT void DoCommandB(void * data)
{
	wIndex_t inx = (wIndex_t)VP2L(data);
	STATUS_T rc;
	static coOrd pos = { 0, 0 };
	static int inDoCommandB = FALSE;
	wIndex_t buttInx;

	if (inDoCommandB) {
		return;
	}
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
			if (rc == 1) {
				commandList[curCommand].cmdProc( C_OK, zero);
			} else if (rc == -1) {
				inDoCommandB = FALSE;
				return;
			}
		}
		LOG(log_command, 3,
		    ( "COMMAND CANCEL %s\n", commandList[curCommand].helpKey ))
		commandList[curCommand].cmdProc( C_CANCEL, pos);
		DYNARR_RESET( trkSeg_t, tempSegs_da );
	} else {
		LOG(log_command, 3,
		    ( "COMMAND FINISH %s\n", commandList[curCommand].helpKey ))
		rc = commandList[curCommand].cmdProc( C_FINISH, zero);
	}
	if (commandList[curCommand].buttInx >= 0)
		ToolbarButtonBusy(commandList[curCommand].buttInx, FALSE);

	if (recordF) {
		fprintf(recordF, "COMMAND %s\n", commandList[inx].helpKey + 3);
		fflush(recordF);
	}

	curCommand = inx;
	commandContext = commandList[curCommand].context;
	// update the toolbar icon when a sub-command is selected (eg. circle
	// vs. filled circle)

	if (commandList[curCommand].buttInx >= 0) {
			ToolbarUpdateButton(commandList[curCommand].buttInx,
				curCommand, commandList[curCommand].icon,
				commandList[curCommand].helpKey, I2VP(curCommand));
		ToolbarButtonBusy(commandList[curCommand].buttInx, TRUE);
	}
		
	LOG(log_command, 1,
		("COMMAND START %s\n", commandList[curCommand].helpKey));
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
		if (rc == C_TERMINATE) {
			InfoMessage("");
		}
		Reset();
		break;
	}
	inDoCommandB = FALSE;
}


/***************************************************************************
 *
 *
 *
 */

EXPORT BOOL_T CommandEnabled(wIndex_t cmdInx)
{
	return commandList[cmdInx].enabled;
}


EXPORT wIndex_t AddCommand(procCommand_t cmdProc, const char * helpKey,
                           const char * nameStr, wIcon_p icon, int reqLevel, long options, long acclKey,
                           wIndex_t buttInx, long stickyMask, wMenuPush_p cmdMenus[NUM_CMDMENUS],
                           void * context)
{
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
	commandList[commandCnt].buttInx = buttInx;
	commandList[commandCnt].stickyMask = stickyMask;
	commandList[commandCnt].menu[0] = cmdMenus[0];
	commandList[commandCnt].menu[1] = cmdMenus[1];
	commandList[commandCnt].menu[2] = cmdMenus[2];
	commandList[commandCnt].menu[3] = cmdMenus[3];

	ToolbarButtonCommandLink(buttInx, commandCnt);

	commandCnt++;
	return commandCnt - 1;
}




EXPORT void PlaybackCommand(const char * line, wIndex_t lineNum)
{
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
		buttInx = commandList[inx].buttInx;
		if ((commandList[inx].buttInx) >= 0) {
			ToolbarButtonPlayback(commandList[inx].buttInx);
		}
		if (strcmp(line + 8, "Undo") == 0) {
			if (buttInx > 0 && playbackTimer == 0) {
				ToolbarButtonBusy(buttInx, TRUE);
				wFlush();
				wPause(500);
				ToolbarButtonBusy(buttInx, FALSE);
				wFlush();
			}
			UndoUndo(NULL);
		} else if (strcmp(line + 8, "Redo") == 0) {
			if (buttInx >= 0 && playbackTimer == 0) {
				ToolbarButtonBusy(buttInx, TRUE);
				wFlush();
				wPause(500);
				ToolbarButtonBusy(buttInx, FALSE);
				wFlush();
			}
			UndoRedo(NULL);
		} else {
			if (buttInx >= 0 && playbackTimer == 0) {
				ToolbarButtonBusy(buttInx, TRUE);
				wFlush();
				wPause(500);
				ToolbarButtonBusy(buttInx, FALSE);
				wFlush();
			}
			DoCommandB(I2VP(inx));
		}
	}
}

/*--------------------------------------------------------------------*/


EXPORT BOOL_T IsCurCommandSticky(void)
{
	if ((commandList[curCommand].options & IC_STICKY) != 0
	    && (commandList[curCommand].stickyMask & stickySet) != 0) {
		return TRUE;
	}
	return FALSE;
}

EXPORT void ResetIfNotSticky(void)
{
	if ((commandList[curCommand].options & IC_STICKY) == 0
	    || (commandList[curCommand].stickyMask & stickySet) == 0) {
		Reset();
	}
}


/*--------------------------------------------------------------------*/
EXPORT void CommandInit( void )
{
	curCommand = describeCmdInx;
	commandContext = commandList[curCommand].context;
	log_command = LogFindIndex( "command" );
}

