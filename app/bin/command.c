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
#include <form.h>
#include "track.h"
#include "common-ui.h"
#include "menu.h"
#include "include/toolbar.h"

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
	wControl_p menu[NUM_CMDMENUS];
	void * context;
} commandList[COMMAND_MAX];


EXPORT int commandCnt = 0;

static wIndex_t curCommand = 0;

/** @logcmd @showrefby `command=n` `command.c` */
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
 *
 * \param mode		application mode
 * \param options	availability options
 * \return			true for enabled, false if disabled
 */



EXPORT bool IsCommandEnabled(long mode, long options)
{
	if (options & IC_MODETRAIN_TOO) {
		return true; // Fast path: always allow when IC_MODETRAIN_TOO is set
	}

	if (mode == MODE_DESIGN && !(options & IC_MODETRAIN_ONLY)) {
		return true; // Design mode OK when ONLY flag is NOT set
	}

	if (mode == MODE_TRAIN && (options & IC_MODETRAIN_ONLY)) {
		return true; // Train mode OK when ONLY flag IS set
	}
	/*
		if (((mode == MODE_DESIGN) || (options & IC_MODETRAIN_ONLY) ||
			(options & IC_MODETRAIN_TOO)) &&
			((mode == MODE_TRAIN) || !(options & IC_MODETRAIN_ONLY)) ||
			(options & IC_MODETRAIN_TOO)) {
			return true;
		}
		else {
			return false;
		}
	*/

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
				if (commandList[inx].buttInx >= 0) {
					ToolbarButtonEnable(commandList[inx].buttInx, enable );
				}
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
	if (commandList[curCommand].buttInx >= 0) {
		ToolbarButtonBusy(commandList[curCommand].buttInx, FALSE);
	}
	curCommand = (preSelect ? selectCmdInx : describeCmdInx);
	wSetCursor(mainD.d, preSelect ? defaultCursor : wCursorQuestion);
	commandContext = commandList[curCommand].context;
	if (commandList[curCommand].buttInx >= 0) {
		ToolbarButtonBusy(commandList[curCommand].buttInx, TRUE);
	}

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

/**
 * Disambiguate a click from a drag for commands that opt in via `IC_LCLICK`.
 *
 * A `C_DOWN` is always held back (returns FALSE, `time0`/`pos0` recorded);
 * the decision is made on the next `C_MOVE` or `C_UP`. If a `C_MOVE` arrives
 * more than `dragTimeout` ms after the down, or more than `dragDistance`
 * units away from it, it's relabeled to `C_DOWN` and treated as a genuine
 * drag-start; a `C_UP` that arrives without a promoting `C_MOVE` first is
 * relabeled to `C_LCLICK` (a plain click).
 *
 * This only works correctly when every `C_MOVE`/`C_UP` this function sees
 * genuinely belongs to the in-progress click -- `time0`/`pos0` are file-
 * scope statics, not per-command state. During `-T` demo playback, a
 * spurious real event reaching `DoMouse()` (see `MainLayout()`'s
 * `!inPlayback` guard on its temp-draw-refresh block) can inject a
 * `C_MOVE` at an unrelated position mid-click; since `dragDistance` is
 * usually the tighter of the two promotion checks and playback's own
 * synthesized `C_MOVE`s intentionally reuse the click's exact position
 * (distance 0), a *real* stray event landing anywhere else is exactly the
 * kind of large `distDelta` that wrongly promotes the click to a drag
 * (GTK3 issue #21 -- confirmed via `-d command=2`, see the `LOG` calls
 * below).
 *
 * \param action IN/OUT the action being dispatched; may be relabeled
 * \param pos IN/OUT the position; reset to the original down position if
 *            relabeled to C_DOWN
 * \param checkLeft IN whether the current command opted into left-click
 *                  disambiguation (`IC_LCLICK`); if false this is a no-op
 *                  for left-button actions
 * \param checkRight IN same as checkLeft, for the right button
 * \return TRUE if action should be dispatched to the current command as-is
 *         (possibly relabeled), FALSE if it should be swallowed this round
 */
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
				LOG(log_command, 2,
				    ("CheckClick: C_MOVE promoted to C_DOWN (drag) -- timeDelta=%ld/%ld distDelta=%0.3f/%0.3f pos=[%0.3f %0.3f] pos0=[%0.3f %0.3f]\n",
				     timeDelta, dragTimeout, distDelta, dragDistance, pos->x, pos->y, pos0.x,
				     pos0.y));
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
			LOG(log_command, 2,
			    ("CheckClick: C_UP -> C_LCLICK -- timeDelta=%ld pos=[%0.3f %0.3f]\n",
			     timeDelta, pos->x, pos->y));
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
	default:
		LOG( log_command, 1, ( "unexpected *action %d in CheckClick\n", *action ) )
		break;
	}
	return TRUE;
}

/**
 * Dispatch a mouse/key action to the current command's `cmdProc`, applying
 * a few cross-cutting checks first: `IC_WANT_MOVE`/`IC_WANT_MODKEYS` opt-in,
 * `CheckClick()`'s click-vs-drag disambiguation for commands that opted
 * into `IC_LCLICK`, and right-click menu handling. On a "bExit" outcome
 * (opted out, or the click is still being held back pending drag/click
 * resolution) the command never sees this call at all -- only a
 * `TempRedraw()` happens instead.
 *
 * `action` may be relabeled by `CheckClick()` before the command sees it:
 * a `C_DOWN` is always held back initially; a later `C_MOVE` either stays
 * held back or gets relabeled to `C_DOWN` (drag-start); a `C_UP` without a
 * promoting `C_MOVE` first gets relabeled to `C_LCLICK` (plain click). See
 * `CheckClick()`'s doc comment -- this relabeling is exactly what a
 * spurious, non-scripted `DoMouse()` call during `-T` demo playback can
 * throw off (GTK3 issue #21).
 *
 * \param action IN the action to dispatch (may be relabeled internally,
 *               see above)
 * \param pos IN the position associated with the action
 * \return the current command's own return value, or C_CONTINUE if the
 *         command was never actually called this round
 */
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
	if ((rc == C_TERMINATE )
	    && IsCurCommandSticky() ) {
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
			Reset();
			break;
		default:
			LOG( log_command, 1, ( "unexpected rc %d in DoCurCommand\n", rc ) )
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
				        wNoticeWithIcon( NT_WARNING,
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
				return C_ERROR;
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
	if (commandList[curCommand].buttInx >= 0) {
		ToolbarButtonBusy(commandList[curCommand].buttInx, FALSE);
	}

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
		                    curCommand,commandList[curCommand].icon,
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
		if (rc == C_TERMINATE) {
			InfoMessage("");
		}
		Reset();
		break;
	default:
		LOG( log_command, 1, ( "unexpected rc %d in DoCommandB\n", rc ) )
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
                           wIndex_t buttInx, long stickyMask, wControl_p cmdMenus[NUM_CMDMENUS],
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
	int inx;
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
			PlaybackButtonMouse(commandList[inx].buttInx);
		}
		if (strcmp(line + 8, "Undo") == 0) {
			if (buttInx > 0 && playbackTimer == 0) {
				SimulateButtonClick( ToolbarButtonGetControl(buttInx) );
			}
			UndoUndo(NULL);
		} else if (strcmp(line + 8, "Redo") == 0) {
			if (buttInx >= 0 && playbackTimer == 0) {
				SimulateButtonClick( ToolbarButtonGetControl(buttInx) );
			}
			UndoRedo(NULL);
		} else {
			if (buttInx >= 0 && playbackTimer == 0) {
				SimulateButtonClick( ToolbarButtonGetControl(buttInx) );
			}
			DoCommandB(I2VP(inx));
		}
	}
}

/*--------------------------------------------------------------------*/


EXPORT BOOL_T IsCurCommandSticky(void)
{
	if ((commandList[curCommand].options & IC_STICKY) != 0) {
		BOOL_T stickyConfig = commandList[curCommand].stickyMask & stickySet;
		BOOL_T stickyUser = ToolbarGetButtonSticky(commandList[curCommand].buttInx);
		return stickyConfig | stickyUser;
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
	//Get command options

	wPrefGetInteger("cmdopt", "preselect", &preSelect, preSelect);
	wPrefGetInteger("cmdopt", "rightclickmode", &rightClickMode,
	                rightClickMode);
	wPrefGetInteger("cmdopt", "selectmode", &selectMode, selectMode);
	wPrefGetInteger("cmdopt", "selectzero", &selectZero, selectZero);

	commandContext = commandList[curCommand].context;
	log_command = LogFindIndex( "command" );

}

