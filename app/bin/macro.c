/** \file macro.c
 *
 * Macros
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

#include "common-ui.h"
#include "common.h"
#include "compound.h"
#include "cundo.h"
#include "custom.h"
#include "directory.h"
#include "draw.h"
#include "fileio.h"
#include "form.h"
#include "include/stringxtc.h"
#include "include/toolbar.h"
#include "layout.h"
#include "misc.h"
#include "param.h"
#include "paths.h"
#include "track.h"
#include "version.h"

#ifdef UTFCONVERT
#include "include/utf8convert.h"
#endif // UTFCONVERT

EXPORT long adjTimer;
static void DemoInitValues(void);

/** @logcmd @showrefby `playbackcursor=n` `macro.c` */
static int log_playbackCursor = 0;
EXPORT BOOL_T paramTogglePlaybackHilite;
/** @logcmd @showrefby `playback=n` `macro.c` */
static int log_playback = 0;

/*****************************************************************************
 *
 * RECORD
 *
 */

EXPORT FILE *recordF;
static wControl_p recordW;
struct wFilSel_t *recordFile_fs;
static BOOL_T recordMouseMoves = TRUE;

static void DoRecordButton(void *context);
static paramTextData_t recordTextData = {50, 16};
static paramData_t recordPLs[] = {
#define I_RECSTOP (0)
#define recStopB (recordPLs[I_RECSTOP].control)
	{
		PD_BUTTON, DoRecordButton, "stop", PDO_NORECORD, NULL, N_("Stop"), 0,
		I2VP(0)
	},
#define I_RECMESSAGE (1)
#define recMsgB (recordPLs[I_RECMESSAGE].control)
	{
		PD_BUTTON, DoRecordButton, "message", PDO_NORECORD | PDO_DLGHORZ, NULL,
		N_("Message"), 0, I2VP(2)
	},
#define I_RECEND (2)
#define recEndB (recordPLs[I_RECEND].control)
	{
		PD_BUTTON, DoRecordButton, "end", PDO_NORECORD | PDO_DLGHORZ, NULL,
		N_("End"), 0, I2VP(4)
	},
#define I_RECTEXT (3)
#define recordT (recordPLs[I_RECTEXT].control)
	{
		PD_TEXT, NULL, "text", PDO_NORECORD | PDO_DLGRESIZE, &recordTextData, NULL,
		0
	}
};
static paramGroup_t recordPG = {"record", PGO_FULLDIALOGFROMBUILDER, recordPLs,
                                COUNT(recordPLs)
                               };

#ifndef WINDOWS
#include <sys/time.h>

static struct timeval lastTim = {0, 0};
static void ComputePause(void)
{
	struct timeval tim;
	long secs;
	long msecs;
	gettimeofday(&tim, NULL);
	secs = tim.tv_sec - lastTim.tv_sec;
	if (secs > 10 || secs < 0) {
		return;
	}
	msecs = secs * 1000 + (tim.tv_usec - lastTim.tv_usec) / 1000;
	if (msecs > 5000) {
		msecs = 5000;
	}
	if (msecs > 1) {
		fprintf(recordF, "PAUSE %ld\n", msecs);
	}
	lastTim = tim;
}
#else
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>

static struct __timeb64 lastTim;
static void ComputePause(void)
{
	struct __timeb64 tim;
	long secs, msecs;
	_ftime(&tim);
	secs = (long)(tim.time - lastTim.time);
	if (secs > 10 || secs < 0) {
		return;
	}
	msecs = secs * 1000;
	if (tim.millitm >= lastTim.millitm) {
		msecs += (tim.millitm - lastTim.millitm);
	} else {
		msecs -= (lastTim.millitm - tim.millitm);
	}
	if (msecs > 5000) {
		msecs = 5000;
	}
	if (msecs > 1) {
		fprintf(recordF, "PAUSE %ld\n", msecs);
	}
	lastTim = tim;
}
#endif

EXPORT void RecordMouse(char *name, wAction_t action, POS_T px, POS_T py)
{
	int keyState;
	if (action == C_MOVE || action == C_RMOVE || (action & 0xFF) == C_TEXT) {
		ComputePause();
	} else if (action == C_DOWN || action == C_RDOWN)
#ifndef WINDOWS
		gettimeofday(&lastTim, NULL);
#else
		_ftime(&lastTim);
#endif
	if (action == wActionMove && !recordMouseMoves) {
		return;
	}
	keyState = wGetKeyState();
	if (keyState) {
		fprintf(recordF, "KEYSTATE %d\n", keyState);
	}
	fprintf(recordF, "%s %d %0.3f %0.3f\n", name, (int)action, px, py);
	fflush(recordF);
}

EXPORT int StartRecord(int cnt, char **pathName, void *context)
{
	time_t clock;

	CHECK(pathName != NULL);
	CHECK(cnt == 1);

	SetCurrentPath(MACROPATHKEY, pathName[0]);
	recordF = fopen(pathName[0], "w");
	if (recordF == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Recording"),
		              pathName[0], strerror(errno));
		return FALSE;
	}
	time(&clock);
	fprintf(recordF, "# %s Version: %s, Date: %s\n", sProdName, sVersion,
	        ctime(&clock));
	fprintf(recordF, "VERSION %d\n", iParamVersion);
	fprintf(recordF, "ROOMSIZE %0.1f x %0.1f\n", mapD.size.x, mapD.size.y);
	fprintf(recordF, "SCALE %s\n", curScaleName);
	fprintf(recordF, "ORIG %0.3f %0.3f %0.3f\n", mainD.scale, mainD.orig.x,
	        mainD.orig.y);
	if (logClock != 0) {
		fprintf(recordF, "# LOG CLOCK %s\n", ctime(&logClock));
	}
	if (logTable_da.cnt > 11) {
		lprintf("StartRecord( %s ) @ %s\n", pathName, ctime(&clock));
	}
	FormStartRecord(recordF);
	WriteTracks(recordF, TRUE);
	WriteLayers(recordF);
	fprintf(recordF, "REDRAW\n");
	fflush(recordF);
	wTextClear(recordT);
	wShow(recordW);
	Reset();
	wControlActive((wControl_p)recEndB, FALSE);
	return TRUE;
}

static void DoRecordButton(void *context)
{
	static wBool_t recordingMessage = FALSE;
	char *cp;
	int len;

	switch ((int)VP2L(context)) {
	case 0: /* Stop */
		fprintf(recordF, "CLEAR\nMESSAGE\n");
		fprintf(recordF, N_("End of Playback.  Hit Step to exit\n"));
		fprintf(recordF, "%s\nSTEP\n", END_MESSAGE);
		fclose(recordF);
		recordF = NULL;
		FormStartRecord(NULL);
		wHide(recordW);
		break;

	case 1: /* Step */
		fprintf(recordF, "STEP\n");
		break;

	case 4: /* End */
		if (recordingMessage) {
			len = wTextGetSize(recordT);
			if (len == 0) {
				break;
			}
			cp = (char *)MyMalloc(len + 2);
			wTextGetText(recordT, cp, len);
			if (cp[len - 1] == '\n') {
				len--;
			}
			cp[len] = '\0';
			fprintf(recordF, "%s\n%s\nSTEP\n", cp, END_MESSAGE);
			MyFree(cp);
			recordingMessage = FALSE;
		}
		wTextSetReadonly(recordT, TRUE);
		fflush(recordF);
		wControlActive(recStopB, TRUE);
		wControlActive(recMsgB, TRUE);
		wControlActive(recEndB, FALSE);
		wWinSetBusy(mainW, FALSE);
		break;

	case 2: /* Message */
		fprintf(recordF, "MESSAGE\n");
		wTextSetReadonly(recordT, FALSE);
		wTextClear(recordT);
		recordingMessage = TRUE;
		wControlActive(recStopB, FALSE);
		wControlActive(recMsgB, FALSE);
		wControlActive(recEndB, TRUE);
		wWinSetBusy(mainW, TRUE);
		break;

	case 3: /* Pause */
		fprintf(recordF, "BIGPAUSE\n");
		fflush(recordF);
		break;

	case 5: /* CLEAR */
		fprintf(recordF, "CLEAR\n");
		fflush(recordF);
		wTextClear(recordT);
		break;

	default:;
	}
}

EXPORT void DoRecord(void *context)
{
	if (recordW == NULL) {
		char *title = MakeWindowTitle(_("Record"));
		recordW = FormCreateDialog(&recordPG, title, _("Finish"), FormButtonOk,
		                           NULL, NULL, FALSE, F_RESIZE, NULL);
		recordFile_fs = wFilSelCreate(mainW, FS_SAVE, 0, title, sRecordFilePattern,
		                              StartRecord, NULL);
	}
	wTextClear(recordT);
	wFilSelect(recordFile_fs, GetCurrentPath(MACROPATHKEY));
}

/*****************************************************************************
 *
 * PLAYBACK MOUSE
 *
 */

static drawCmd_p playbackD = NULL;
static wDrawBitMap_p playbackBm = NULL;
static wDrawColor playbackColor;
static coOrd playbackPos;
static wBool_t bDoFlash = FALSE;
static wDrawColor flashColor;

static wDrawColor rightDragColor;
static wDrawColor leftDragColor;
static wDrawBitMap_p arrow0_bm;
static wDrawBitMap_p arrow0_shift_bm;
static wDrawBitMap_p arrow0_ctl_bm;
static wDrawBitMap_p arrow3_bm;
static wDrawBitMap_p arrow3_shift_bm;
static wDrawBitMap_p arrow3_ctl_bm;
static wDrawBitMap_p arrows_bm;
static wDrawBitMap_p arrowr3_bm;
static wDrawBitMap_p arrowr3_shift_bm;
static wDrawBitMap_p arrowr3_ctl_bm;
static wDrawBitMap_p flash_bm;

static long flashTO = 120;
static DIST_T PixelsPerStep = 5;
static long stepTO = 100;
EXPORT unsigned long
playbackTimer; /** if >0 performance measurement in progress */

static wBool_t didPause;
static wBool_t flashTwice = FALSE;

int DBMCount = 0;

#define DRAWALL
typedef enum {
	FLASH_PLUS,
	FLASH_MINUS,
	REDRAW,
	CLEAR,
	DRAW,
	RESET,
	ORIG,
	MOVE_PLYBCK1,
	MOVE_PLYBCK2,
	MOVE_PLYBCK3,
	MOVE_PLYBCK4,
	QUIT
} DrawBitMap_e;

char *DrawBitMapToString(DrawBitMap_e dbm)
{
	switch (dbm) {
	case FLASH_PLUS:
		return "Flsh+";
	case FLASH_MINUS:
		return "Flsh-";
	case REDRAW:
		return "Redraw";
	case CLEAR:
		return "Clr";
	case DRAW:
		return "Draw";
	case RESET:
		return "RESET";
	case ORIG:
		return "ORIG";
	case MOVE_PLYBCK1:
		return "MPBC1";
	case MOVE_PLYBCK2:
		return "MPBC2";
	case MOVE_PLYBCK3:
		return "MPBC3";
	case MOVE_PLYBCK4:
		return "MPBC4";
	case QUIT:
		return "Quit";
	default:
		return "";
	}
}

static void MacroDrawBitMap(DrawBitMap_e dbm, wDrawBitMap_p bm, coOrd pos,
                            wDrawColor color)
{
	DrawBitMap(playbackD, pos, bm, color);
	//	wFlush();

	LOG(log_playbackCursor, 2,
	    ("%s %d DrawBitMap( %p %p [%0.3f %0.3f] %d )\n", DrawBitMapToString(dbm),
	     DBMCount++, playbackD->d, bm, pos, color));
}

static void Flash(wDrawColor color)
{
	bDoFlash = TRUE;
	flashColor = color;
}

EXPORT long playbackDelay = 100;
static long playbackSpeed = 2;

static void SetPlaybackSpeed(wIndex_t inx)
{
	switch (inx) {
	case 0:
		playbackDelay = 500;
		break;
	case 1:
		playbackDelay = 200;
		break;
	default:
	case 2:
		playbackDelay = 100;
		break;
	case 3:
		playbackDelay = 50;
		break;
	case 4:
		playbackDelay = 15;
		break;
	case 5:
		playbackDelay = 0;
		break;
	}
	playbackSpeed = inx;
}

/**
 * Draw (or flash) the simulated cursor bitmap at the current playback
 * position, then `wFlush()` so it's actually visible before the next
 * scripted step. A no-op unless `inPlayback` and a playback cursor bitmap
 * are both set.
 *
 * Called from `DrawTempContent()` (`draw.c`), deliberately *after* that
 * function's own `wDrawStart`/`wDrawFinish` batch has already closed, not
 * from within it -- see that function's doc comment for why (GTK3 issue
 * #21: `wFlush()` pumps the GTK main loop synchronously, and doing that
 * from inside an open batch let a delayed cold-start layout event reenter
 * drawing and corrupt the shared Cairo context).
 */
EXPORT void RedrawPlaybackCursor()
{
	if (playbackD && playbackBm && inPlayback) {
		unsigned long options = playbackD->options;
		playbackD->options |= DC_TEMP;
		wBool_t bTemp = wDrawSetTempMode(playbackD->d, TRUE);
		if (bDoFlash && playbackTimer == 0) {
			MacroDrawBitMap(FLASH_PLUS, flash_bm, playbackPos, flashColor);
			wDrawSetTempMode(playbackD->d, FALSE);
			/* Pacing pause for the click-target flash animation -- skipped
			 * under bRunTests (-T), same reasoning as the sibling
			 * wPause(bRunTests ? 0 : 1000) in PlaybackMain() below. Flash()
			 * fires on essentially every scripted MOUSE click across every
			 * demo, so left unconditional this was one of the largest
			 * unconditional-pause contributors to total -T runtime. */
			wPause(bRunTests ? 0 : flashTO * 2);
			wDrawSetTempMode(playbackD->d, TRUE);
			if (flashTwice) {
				MacroDrawBitMap(FLASH_PLUS, flash_bm, playbackPos, flashColor);
				wDrawSetTempMode(playbackD->d, FALSE);
				wPause(bRunTests ? 0 : flashTO * 2);
				wDrawSetTempMode(playbackD->d, TRUE);
			}
			bDoFlash = FALSE;
		}
		MacroDrawBitMap(DRAW, playbackBm, playbackPos, playbackColor);
		wDrawSetTempMode(playbackD->d, bTemp);
		playbackD->options = options;
		wFlush();
	}
}

static void MoveCursor(drawCmd_p d, playbackProc proc, wAction_t action,
                       coOrd pos, wDrawBitMap_p bm, wDrawColor color)
{
	DIST_T dist;
	coOrd dpos;
	int i, steps;

	if (d == NULL) {
		return;
	}

	if (playbackTimer == 0 /*&& !didPause*/) {
		playbackBm = bm;
		playbackColor = color;
		dist = FindDistance(playbackPos, pos);
		steps = (int)(dist / (PixelsPerStep * d->scale / d->dpi)) + 1;
		LOG(log_playbackCursor, 1,
		    ("PBC: [%0.3f %0.3f] - [%0.3f %0.3f] Dist:%0.3f Steps:%d\n",
		     playbackPos.x, playbackPos.y, pos.x, pos.y, dist, steps));

		dpos.x = (pos.x - playbackPos.x) / steps;
		dpos.y = (pos.y - playbackPos.y) / steps;

		for (i = 1; i <= steps; i++) {
			playbackPos.x += dpos.x;
			playbackPos.y += dpos.y;
			if (proc != NULL) {
				proc(action, playbackPos);
			} else {
				TempRedraw();
			}
			if (d->d == mainD.d) {
				InfoPos(playbackPos);
				wFlush();
			}
			// Simple mouse moves happen twice as fast
			wPause(stepTO * playbackDelay / 100 / (action == wActionMove ? 2 : 1));

			if (!inPlayback) {
				return;
			}
		}
	}
	playbackPos = pos;
}

static void PlaybackCursor(drawCmd_p d, playbackProc proc, wAction_t action,
                           coOrd pos, wDrawColor color)
{
	wDrawBitMap_p bm = playbackBm;
	playbackD = d;
	long time0, time1;

	time0 = wGetTimer();

	switch (action & 0xFF) {

	case wActionMove:
		bm = ((MyGetKeyState() & WKEY_SHIFT) ? arrow0_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL)
		      ? arrow0_ctl_bm
		      : arrow0_bm); // 0 is normal, shift, ctrl
		MoveCursor(d, proc, wActionMove, pos, bm, wDrawColorBlack);
		break;

	case C_DOWN:
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow0_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow0_ctl_bm
		      : arrow0_bm);
		MoveCursor(d, proc, wActionMove, pos, bm, wDrawColorBlack); // Go to spot
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow3_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow3_ctl_bm
		      : arrow3_bm);
		Flash(playbackColor = rightDragColor);
		proc(action, pos);
		__attribute__((fallthrough));

	case C_MOVE:
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow3_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow3_ctl_bm
		      : arrow3_bm);
		MoveCursor(d, proc, C_MOVE, pos, bm, rightDragColor);
		break;

	case C_UP:
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow3_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow3_ctl_bm
		      : arrow0_bm);
		MoveCursor(d, proc, C_MOVE, pos, bm, rightDragColor);
		Flash(rightDragColor);
		proc(action, pos);
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow0_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow0_ctl_bm
		      : arrow0_bm);
		MoveCursor(d, NULL, 0, pos, bm, wDrawColorBlack);
		break;

	case C_RDOWN:
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow0_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow0_ctl_bm
		      : arrow0_bm);
		MoveCursor(d, proc, wActionMove, pos, bm, wDrawColorBlack); // Go to spot
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrowr3_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrowr3_ctl_bm
		      : arrowr3_bm);
		Flash(playbackColor = leftDragColor);
		proc(action, pos);
		__attribute__((fallthrough));

	case C_RMOVE:
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrowr3_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrowr3_ctl_bm
		      : arrowr3_bm);
		MoveCursor(d, proc, C_RMOVE, pos, bm, leftDragColor);
		break;

	case C_RUP:
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrowr3_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrowr3_ctl_bm
		      : arrowr3_bm);
		MoveCursor(d, proc, C_RMOVE, pos, bm, leftDragColor);
		Flash(leftDragColor);
		proc(action, pos);
		bm = ((MyGetKeyState() & WKEY_SHIFT)  ? arrow0_shift_bm
		      : (MyGetKeyState() & WKEY_CTRL) ? arrow0_ctl_bm
		      : arrow0_bm);
		MoveCursor(d, NULL, 0, pos, bm, wDrawColorBlack);
		break;

	case C_REDRAW:
		proc(action, pos); // Send Redraw to functions
		playbackD = &tempD;
		playbackPos = pos;
		MacroDrawBitMap(REDRAW, playbackBm, playbackPos, playbackColor);
		break;

	case C_TEXT:
		proc(action, pos);
		//		char c = action>>8;
		bm = playbackBm;
		break;

	default:
		bm = playbackBm;
	}

	playbackBm = bm;
	time1 = wGetTimer();
	adjTimer += (time1 - time0);
}

EXPORT void PlaybackMouse(playbackProc proc, drawCmd_p d, wAction_t action,
                          coOrd pos, wDrawColor color)
{
	PlaybackCursor(d, proc, action, pos, wDrawColorBlack);
	didPause = FALSE;
}

// #define MOVECURSORTOCOMMANDBUTTON
//  Defining this shows the cursor move towards the active toolbar button
//  However it only is displayed over the main canvas, we can't draw over the
//  toolbar

/**
 * Highlight (and, if `MOVECURSORTOCOMMANDBUTTON` is defined, animate the
 * simulated cursor moving to) a toolbar button during demo playback, e.g.
 * from `PlaybackButtonMouse()` selecting a command. The pacing pause is
 * skipped under `bRunTests` (-T) -- nothing is watching a regression run,
 * and at one full second per `COMMAND` step this was unconditionally
 * padding every `-T` run's total time, same reasoning as the sibling
 * `wPause(bRunTests ? 0 : 1000)` a few hundred lines below in
 * `PlaybackMain()`.
 *
 * \param d IN the draw context the cursor animation would draw into
 * \param pos IN target position (only used by the `MOVECURSORTOCOMMANDBUTTON` path)
 * \param direct IN whether to draw/pause directly (only used by the `MOVECURSORTOCOMMANDBUTTON` path)
 * \param control IN the toolbar button control to highlight
 */
EXPORT void MovePlaybackCursor(drawCmd_p d, coOrd pos, wBool_t direct,
                               wControl_p control)
{
#ifdef MOVECURSORTOCOMMANDBUTTON
	// Show the cursor clicking on the command button
	// Not possile with current structure
	playbackD = &tempD;
	if (!direct) {
		MoveCursor(d, NULL, wActionMove, pos, arrow0_bm, wDrawColorBlack);
	}
	unsigned long options = d->options;
	d->options |= DC_TEMP;
	wBool_t bTemp = wDrawSetTempMode(d->d, TRUE);
	DoCurCommand(C_REDRAW, zero);
	MacroDrawBitMap(MOVE_PLYBCK1, arrow0_bm, pos, wDrawColorBlack);
	MacroDrawBitMap(MOVE_PLYBCK2, arrow3_bm, pos, rightDragColor);

	Flash(rightDragColor);
	if (direct) {
		wControlHilite(control, TRUE);
	}
	MacroDrawBitMap(MOVE_PLYBCK3, arrow3_bm, pos, rightDragColor);
	MacroDrawBitMap(MOVE_PLYBCK4, arrow0_bm, pos, wDrawColorBlack);
	if (direct) {
		wPause(bRunTests ? 0 : 1000);
		wControlHilite(control, FALSE);
	}
	wDrawSetTempMode(d->d, bTemp);
	d->options = options;
#else
	// Just hilight the button
	if (control) {
		wControlHilite(control, TRUE);
		wPause(bRunTests ? 0 : 1000);
		wControlHilite(control, FALSE);
	}
#endif
}

/*****************************************************************************
 *
 * PLAYBACK
 *
 */

EXPORT wBool_t inPlayback;
EXPORT wBool_t inPlaybackQuit;

EXPORT wControl_p demoW;
EXPORT int curDemo = -1;

typedef struct {
	char *title;
	char *fileName;
} demoList_t;
static dynArr_t demoList_da;
#define demoList(N) DYNARR_N(demoList_t, demoList_da, N)
static struct wFilSel_t *playbackFile_fs;

typedef struct {
	char *label;
	playbackProc_p proc;
	void *data;
} playbackProc_t;
static dynArr_t playbackProc_da;
#define playbackProc(N) DYNARR_N(playbackProc_t, playbackProc_da, N)

static coOrd oldRoomSize;
static coOrd oldMainOrig;
static coOrd oldMainSize;
static DIST_T oldMainScale;
static char *oldScaleName;
static int oldMagneticSnap;
static wBool_t backgroundShown;

static wBool_t pauseDemo = FALSE;
static long bigPause = 2000;
#ifdef DEMOPAUSE
static wButton_p demoPause;
#endif
static BOOL_T playbackNonStop = FALSE;

static int playbackKeyState;

static void DoDemoButton(void *context);
static paramTextData_t demoTextData = {50, 16};
static paramData_t demoPLs[] = {
#define I_DEMOSTEP (0)
#define demoStep (demoPLs[I_DEMOSTEP].control)
	{
		PD_BUTTON, DoDemoButton, "step", PDO_NORECORD, NULL, NULL, BB_DEFAULT,
		I2VP(0)
	},
#define I_DEMONEXT (1)
#define demoNext (demoPLs[I_DEMONEXT].control)
	{
		PD_BUTTON, DoDemoButton, "next", PDO_NORECORD | PDO_DLGHORZ, NULL, NULL, 0,
		I2VP(1)
	},
#define I_DEMOSPEED (2)
#define demoSpeedL (demoPLs[I_DEMOSPEED].control)
	{
		PD_COMBOLIST, &playbackSpeed, "speed",
		PDO_NORECORD | PDO_LISTINDEX | PDO_DLGHORZ, I2VP(80), NULL
	},
#define I_DEMOTEXT (3)
#define demoT (demoPLs[I_DEMOTEXT].control)
	{
		PD_TEXT, NULL, "text", PDO_NORECORD | PDO_DLGRESIZE, &demoTextData, NULL,
		BT_CHARUNITS | BO_READONLY
	}
};
static paramGroup_t demoPG = {"demo", PGO_FULLDIALOGFROMBUILDER, demoPLs,
                              COUNT(demoPLs)
                             };

EXPORT int MyGetKeyState(void)
{
	if (inPlayback) {
		return playbackKeyState;
	} else {
		return wGetKeyState();
	}
}

EXPORT void AddPlaybackProc(char *label, playbackProc_p proc, void *data)
{
	DYNARR_APPEND(playbackProc_t, playbackProc_da, 10);
	playbackProc(playbackProc_da.cnt - 1).label = MyStrdup(label);
	playbackProc(playbackProc_da.cnt - 1).proc = proc;
	playbackProc(playbackProc_da.cnt - 1).data = data;
}

static void PlaybackQuit(void)
{
	long playbackSpeed1 = playbackSpeed;
	LOG(log_playback, 2, ("Playback QUIT\n"));
	if (paramFile) {
		fclose(paramFile);
	}
	paramFile = NULL;
	if ( inPlaybackQuit ) {
		return;
	}
	inPlaybackQuit = TRUE;
	wPrefReset();
	wHide(demoW);
	wWinSetBusy(mainW, FALSE);
	wWinSetBusy(mapW, FALSE);
	ParamRestoreAll();
	magneticSnap = oldMagneticSnap;
	RestoreLayers();

	if(backgroundShown) {
		BackgroundToggleShow(NULL);
	}

	mainD.scale = oldMainScale;
	mainD.size = oldMainSize;
	mainD.orig = oldMainOrig;
	SetRoomSize(oldRoomSize);
	tempD.orig = mainD.orig;
	tempD.size = mainD.size;
	tempD.scale = mainD.scale;
	Reset();
	ClearTracks();
	checkPtMark = changed = 0;
	RestoreTrackState();
	DoSetScale(oldScaleName);
	inPlaybackQuit = FALSE;
	DoChangeNotification(CHANGE_ALL);
	CloseDemoWindows();
	curDemo = -1;
	wPrefSetInteger("misc", "playbackspeed", playbackSpeed);
	playbackNonStop = FALSE;
	playbackSpeed = playbackSpeed1;
	UndoResume();
	wWinBlockEnable(TRUE);
	wResetKeyStateFromButton();
}

static int documentEnable = 0;
static int documentAutoSnapshot = 0;

static drawCmd_t snapshot_d = {
	NULL,       &screenDrawFuncs, 0,         16.0,     0,
	{0.0, 0.0}, {1.0, 1.0},       Pix2CoOrd, CoOrd2Pix
};
static int documentSnapshotNum = 1;
static int documentCopy = 0;
static FILE *documentFile;
static BOOL_T snapshotMouse = FALSE;

EXPORT void TakeSnapshot(drawCmd_t *d)
{
	char *cp;
	wWinPix_t ix, iy;
	if (d->dpi < 0) {
		d->dpi = mainD.dpi;
	}
	if (d->scale < 0) {
		d->scale = mainD.scale;
	}
	if (d->orig.x < 0 || d->orig.y < 0) {
		d->orig = mainD.orig;
	}
	if (d->size.x < 0 || d->size.y < 0) {
		d->size = mainD.size;
	}
	ix = (wWinPix_t)(d->dpi * d->size.x / d->scale);
	iy = (wWinPix_t)(d->dpi * d->size.y / d->scale);
	d->d = wBitmapCreate(ix, iy, 8);
	if (d->d == NULL) {
		return;
	}
	DrawTracks(d, d->scale, d->orig, d->size);
	// cppcheck-suppress knownConditionTrueFalse
	// snapshotMouse is a never-wired-up feature flag (always FALSE); the
	// mouse-cursor bitmap is never drawn on playback snapshots today.
	if (snapshotMouse && playbackBm) {
		DrawBitMap(d, playbackPos, playbackBm, playbackColor);
	}
	coOrd p0, s1;
	DIST_T off = 0.02;
	p0.x = off * d->scale;
	p0.y = off * d->scale;
	s1.x = d->size.x - off * 2 * d->scale;
	s1.y = d->size.y - off * 2 * d->scale;
	DrawRectangle(d, p0, s1, wDrawColorBlack, DRAW_CLOSED);
	strcpy(message, paramFileName);
	cp = message + strlen(message) - 4;
	sprintf(cp, "-%4.4d.png", documentSnapshotNum);
	wBitmapWriteFile(d->d, message);
	wBitmapDelete(d->d);
	documentSnapshotNum++;
	if (documentCopy && documentFile) {
		cp = FindFilename(message);
		cp[strlen(cp) - 4] = 0;
		fprintf(documentFile, "\n?G%s\n", cp);
	}
}

/** @logcmd @showrefby `visualdiff=n` `macro.c` */
static int log_visualdiff = 0;

/**
 * Render the current layout to an offscreen bitmap and save it as a PNG at
 * an explicit path. Used by DoRegression()'s -d visualdiff hook to capture
 * a picture of what a REGRESSION block saved/checked, for visually diffing
 * two versions' output against each other (a text diff of track coordinates
 * can miss a change that's obvious in a picture -- see SF #667/#26, where a
 * fixture regen silently dropped a join's connector tracks and the numeric
 * diff alone didn't make that visible).
 *
 * Kept independent of TakeSnapshot() above rather than sharing its body --
 * TakeSnapshot() drives an existing, currently-unreachable (documentEnable
 * is hardcoded off) auto-numbered-illustration feature, and this avoids any
 * risk of perturbing that code path's behavior for the sake of DRYing up
 * two call sites of a debug-only feature.
 *
 * \param pngPath IN full path to write the PNG to
 */
static void SaveRegressionSnapshot(const char *pngPath)
{
	drawCmd_t d = snapshot_d;
	wWinPix_t ix, iy;
	track_p trk = NULL;
	coOrd hi, lo, bot = {0.0, 0.0}, top = {0.0, 0.0};
	BOOL_T first = TRUE;
	const DIST_T margin = 1.0;
	const wWinPix_t maxDim = 4000;

	// Deliberately not mainD.orig/mainD.size: those track the live window's
	// current viewport, which is unreliable at an arbitrary point mid
	// headless -T playback (observed empirically: mainD.size.y went negative
	// mid-run, producing an invalid bitmap height that made
	// gdk_pixbuf_get_from_surface fail its assertion, which made
	// wBitmapWriteFile pop a NoticeMessage -- a modal dialog with no
	// headless-mode awareness, hanging forever under Xvfb, the same bug
	// class as AbortProg/UndoFail before RegressionTests.cmake's fixes).
	// Computing our own bounding box from the actual track data is
	// self-contained and can't be thrown off by transient view state.
	while (TrackIterate(&trk)) {
		GetBoundingBox(trk, &hi, &lo);
		if (first) {
			first = FALSE;
			bot = lo;
			top = hi;
		} else {
			if (lo.x < bot.x) {
				bot.x = lo.x;
			}
			if (lo.y < bot.y) {
				bot.y = lo.y;
			}
			if (hi.x > top.x) {
				top.x = hi.x;
			}
			if (hi.y > top.y) {
				top.y = hi.y;
			}
		}
	}
	if (first) {
		return; /* no tracks to snapshot */
	}

	d.dpi = mainD.dpi;
	d.scale = mainD.scale > 0.0 ? mainD.scale : 8.0;
	d.orig.x = bot.x - margin;
	d.orig.y = bot.y - margin;
	d.size.x = (top.x - bot.x) + margin * 2;
	d.size.y = (top.y - bot.y) + margin * 2;

	ix = (wWinPix_t)(d.dpi * d.size.x / d.scale);
	iy = (wWinPix_t)(d.dpi * d.size.y / d.scale);
	if (ix > maxDim || iy > maxDim) {
		DIST_T grow = (ix > iy) ? ((DIST_T)ix / maxDim) : ((DIST_T)iy / maxDim);
		d.scale *= grow;
		ix = (wWinPix_t)(d.dpi * d.size.x / d.scale);
		iy = (wWinPix_t)(d.dpi * d.size.y / d.scale);
	}
	// EXPORTBITMAP, not TakeSnapshot()'s literal 8 -- wBitmapCreate() only
	// creates a real cairo surface when (arg & EXPORTBITMAP); TakeSnapshot()'s
	// own `8` doesn't have that bit set (EXPORTBITMAP == 1), so it silently
	// takes the no-op branch and leaves draw->surface unset. Confirmed via
	// dbitmap.c's working "Export Bitmap" feature, the only other caller,
	// which does pass EXPORTBITMAP. Pre-existing bug in TakeSnapshot() too,
	// left as-is there since that whole feature is unreachable dead code
	// (documentEnable hardcoded off) -- not fixing unrelated dead code here.
	d.d = wBitmapCreate(ix, iy, EXPORTBITMAP);
	if (d.d == NULL) {
		return;
	}
	DrawTracks(&d, d.scale, d.orig, d.size);
	coOrd p0, s1;
	DIST_T off = 0.02;
	p0.x = off * d.scale;
	p0.y = off * d.scale;
	s1.x = d.size.x - off * 2 * d.scale;
	s1.y = d.size.y - off * 2 * d.scale;
	DrawRectangle(&d, p0, s1, wDrawColorBlack, DRAW_CLOSED);
	wBitmapWriteFile(d.d, pngPath);
	wBitmapDelete(d.d);
}

/**
 * Build `<workingDir>/visualdiff/<basename of demoFile, minus its
 * extension>-<suffix>.png` (creating the visualdiff directory if needed) and
 * save a snapshot there. Shared by DoRegression()'s per-REGRESSION-block
 * hook and the end-of-playback "final view" hook below.
 *
 * \param demoFile IN full path of the demo file currently playing
 * \param suffix IN distinguishes multiple snapshots of the same demo, e.g.
 *        "L92" for the REGRESSION block at line 92, or "final" for the
 *        end-of-playback snapshot
 */
static void SaveVisualDiffSnapshot(char *demoFile, const char *suffix)
{
	char *visualDiffDir = NULL;
	char *pngPath = NULL;
	char baseName[STR_SIZE];
	char pngName[STR_SIZE + 32];
	char *ext;
	strncpy(baseName, FindFilename(demoFile), sizeof(baseName) - 1);
	baseName[sizeof(baseName) - 1] = '\0';
	ext = strrchr(baseName, '.');
	if (ext) {
		*ext = '\0';
	}
	snprintf(pngName, sizeof(pngName), "%s-%s.png", baseName, suffix);
	MakeFullpath(&visualDiffDir, workingDir, "visualdiff", NULL);
	SafeCreateDir(visualDiffDir);
	MakeFullpath(&pngPath, visualDiffDir, pngName, NULL);
	SaveRegressionSnapshot(pngPath);
	free(visualDiffDir);
	free(pngPath);
}

/*
 * Regression test
 */
/** @logcmd @showrefby `regression=n` `macro.c` */
static int log_regression = 0;
static int nRegressionFail = 0;

static BOOL_T DoRegression(char *sFileName)
{
	typedef enum {
		REGRESSION_NONE,
		REGRESSION_CHECK,
		REGRESSION_QUIET,
		REGRESSION_SAVE
	} E_REGRESSION;
	E_REGRESSION eRegression = REGRESSION_NONE;
	long oldParamVersion;
	long regressVersion;
	FILE *fRegression;
	char *sRegressionFile = NULL;
	//	wBool_t bWroteActualTracks;
	eRegression = log_regression > 0 ? logTable(log_regression).level : 0;
	char *cp;
	regressVersion = strtol(paramLine + 16, &cp, 10);
	if (cp == paramLine + 16) {
		regressVersion = PARAMVERSION;
	}
	LOG(log_regression, 1,
	    ("REGRESSION %s %d %s:%d %s\n",
	     eRegression == REGRESSION_SAVE ? "SAVE" : "CHECK", regressVersion,
	     sFileName, paramLineNum, cp));
	MakeFullpath(&sRegressionFile, workingDir, "xtrkcad.regress", NULL);
	switch (eRegression) {
	case REGRESSION_SAVE:
		fRegression = fopen(sRegressionFile, "a");
		if (fRegression == NULL) {
			NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Regression"),
			              sFileName, strerror(errno));
		} else {
			fprintf(fRegression, "REGRESSION START %d %s\n", PARAMVERSION, cp);
			fprintf(fRegression, "# %s - %d\n", sFileName, paramLineNum);
			WriteTracks(fRegression, FALSE);
			fprintf(fRegression, "REGRESSION END\n");
			fclose(fRegression);
		}
		while (fgets(paramLine, STR_LONG_SIZE, paramFile) != NULL) {
			if (strncmp(paramLine, "REGRESSION END", 14) == 0) {
				break;
			}
		}
		break;
	case REGRESSION_CHECK:
	case REGRESSION_QUIET:
		oldParamVersion = paramVersion;
		int nFail = CheckRegressionResult(regressVersion, sFileName,
		                                  eRegression == REGRESSION_QUIET);
		paramVersion = oldParamVersion;
		if (nFail < 0) {
			return FALSE;
		}
		nRegressionFail += nFail;
		break;
	case REGRESSION_NONE:
	default:
		while (GetNextLine()) {
			if (strncmp(paramLine, "REGRESSION END", 14) == 0) {
				break;
			}
		}
		break;
	}
	if (log_visualdiff > 0 && eRegression != REGRESSION_NONE) {
		char suffix[16];
		snprintf(suffix, sizeof(suffix), "L%d", paramLineNum);
		SaveVisualDiffSnapshot(sFileName, suffix);
	}
	free(sRegressionFile);

	return TRUE;
}

static void EnableButtons(BOOL_T enable)
{
	LOG(log_playback, 2,
	    ("EnableButtons( %d ), inPlayback:%d\n", enable, inPlayback));
	wButtonSetBusy(demoStep, !enable);
	wButtonSetBusy(demoNext, !enable);
	wControlActive((wControl_p)demoStep, enable);
	wControlActive((wControl_p)demoNext, enable);
#ifdef DEMOPAUSE
	wButtonSetBusy(demoPause, enable);
#endif
}

EXPORT void PlaybackMessage(char *line)
{
	char *cp;
	wTextAppend(demoT, _(line));
	if (documentCopy && documentFile) {
		if (strncmp(line, "__________", 10) != 0) {
			for (cp = line; *cp; cp++) {
				switch (*cp) {
				case '<':
					fprintf(documentFile, "$B");
					break;
				case '>':
					fprintf(documentFile, "$");
					break;
				default:
					fprintf(documentFile, "%c", *cp);
				}
			}
		}
	}
}

static void PlaybackSetup(void)
{
	SaveTrackState();
	EnableButtons(TRUE);

	if((backgroundShown = GetLayoutBackGroundVisible())) {
		BackgroundToggleShow(NULL);
	}

	SetPlaybackSpeed((wIndex_t)playbackSpeed);
	wListSetIndex(demoSpeedL, (wIndex_t)playbackSpeed);
	wTextClear(demoT);
	wShow(demoW);
	wFlush();
	wPrefFlush("");
	wWinSetBusy(mainW, TRUE);
	wWinSetBusy(mapW, TRUE);
	ParamSaveAll();
	paramLineNum = 0;
	oldRoomSize = mapD.size;
	oldMainOrig = mainD.orig;
	oldMainSize = mainD.size;
	oldMainScale = mainD.scale;
	oldScaleName = curScaleName;
	playbackPos = zero;
	Reset();
	paramVersion = -1;
	playbackColor = wDrawColorBlack;
	paramTogglePlaybackHilite = FALSE;
	CompoundClearDemoDefns();
	SaveLayers();
	nRegressionFail = 0;
}

static void SetInPlayback(wBool_t state) { inPlayback = state; }

static void Playback(void)
{
	POS_T x, y;
	POS_T zoom;
	wIndex_t inx;
	long timeout;
	static enum { pauseCmd, mouseCmd, otherCmd } thisCmd /*, lastCmd*/;
	size_t len;
	static wBool_t demoWinOnTop = FALSE;
	coOrd roomSize;
	char *cp, *cq;
	char *demoFileName = NULL;

	if (inPlayback) {
		LOG(log_playback, 2, ("Playback RECURSE\n"));
		return;
	}
	useCurrentLayer = FALSE;
	SetInPlayback(TRUE);
	EnableButtons(FALSE);
	//	lastCmd = otherCmd;
	playbackTimer = 0;
	ParamTurnOffDelays(false);
	if (demoWinOnTop) {
		wWinTop(mainW);
		demoWinOnTop = FALSE;
	}
	SetCLocale();
	LOG(log_playback, 2, ("Playback START\n"));
	while (TRUE) {
		if (!inPlayback)
			// User pressed Quit
		{
			break;
		}
		if (paramFile == NULL ||
		    fgets(paramLine, STR_LONG_SIZE, paramFile) == NULL) {
			// Capture the demo that just finished's end state before
			// CloseDemoWindows()/Reset() below clear it. paramFileName is
			// NULL on the very first iteration (no demo has run yet), and
			// still refers to the just-finished demo here -- it isn't
			// reassigned to the next demo until later in this same block.
			if (log_visualdiff > 0 && paramFileName != NULL) {
				SaveVisualDiffSnapshot(paramFileName, "final");
			}
			paramTogglePlaybackHilite = FALSE;
			CloseDemoWindows();
			if (paramFile) {
				fclose(paramFile);
				paramFile = NULL;
			}
			if (documentFile) {
				fclose(documentFile);
				documentFile = NULL;
			}
			Reset();
			if (curDemo < 0 || curDemo >= demoList_da.cnt) {
				break;
			}
			demoFileName = strdup(demoList(curDemo).fileName);
			paramFile = fopen(demoFileName, "r");
			LOG(log_playback, 1, ("Playback Open %s\n", demoFileName));
			if (paramFile == NULL) {
				NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Demo"),
				              demoFileName, strerror(errno));
				SetInPlayback(FALSE);
				SetUserLocale();
				LOG(log_playback, 2, ("Playback OPENFAIL RETURN\n"));
				return;
			}

			paramFileName = strdup(demoFileName);
			playbackColor = wDrawColorBlack;
			paramLineNum = 0;
			wWinSetTitle(demoW, demoList(curDemo).title);
			curDemo++;
			ClearTracks();
			UndoSuspend();
			wWinBlockEnable(FALSE);
			checkPtMark = 0;
			DoChangeNotification(CHANGE_ALL);
			CompoundClearDemoDefns();
			if (fgets(paramLine, STR_LONG_SIZE, paramFile) == NULL) {
				NoticeMessage(MSG_CANT_READ_DEMO, _("Continue"), NULL, sProdName,
				              demoFileName);
				fclose(paramFile);
				paramFile = NULL;
				SetInPlayback(FALSE);
				LOG(log_playback, 2, ("Playback READFAIL RETURN\n"));
				SetUserLocale();
				return;
			}
			free(demoFileName);
			demoFileName = NULL;
		}
		if (paramLineNum == 0) {
			documentSnapshotNum = 1;
			if (documentEnable) {
				strcpy(message, paramFileName);
				cp = message + strlen(message) - 4;
				strcpy(cp, ".hlpsrc");
				documentFile = fopen(message, "w");
				documentCopy = TRUE;
			}
		}
		thisCmd = otherCmd;
		paramLineNum++;
		Stripcr(paramLine);
		if (paramLine[0] == '#') {
			/* comment */
			continue;
		} else if (paramLine[0] == 0) {
			/* empty paramLine */
			continue;
		} else if (ReadTrack(paramLine)) {
			LOG(log_playback, 3, ("%3d: ReadTrack %s\n", paramLineNum, paramLine));
			if (paramFile == NULL) {
				SetInPlayback(FALSE);
				break;
			}
			continue;
		}
		LOG(log_playback, 3, ("%3d: %s\n", paramLineNum, paramLine));
		// N=5 (strlen("STEP")+1) is a deliberate exact-match idiom requiring
		// paramLine[4]=='\0' too; paramLine is always a valid terminated
		// string (fgets + Stripcr()), so no OOB read risk either way.
		// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
		if (strncmp(paramLine, "STEP", 5) == 0) {
			paramTogglePlaybackHilite = TRUE;
			wWinTop(demoW);
			demoWinOnTop = TRUE;
			didPause = FALSE;
			EnableButtons(TRUE);
			if (!demoWinOnTop) {
				wWinTop(demoW);
				demoWinOnTop = TRUE;
			}
			if (documentAutoSnapshot) {
				snapshot_d.dpi = snapshot_d.scale = snapshot_d.orig.x =
				snapshot_d.orig.y = snapshot_d.size.x = snapshot_d.size.y = -1;
				TakeSnapshot(&snapshot_d);
			}
			if (playbackNonStop) {
				// bRunTests (-T): skip the human-observable pacing pause,
				// nothing is watching. Interactive Shift-held auto-play
				// (also playbackNonStop) keeps its full 1s pause.
				wPause(bRunTests ? 0 : 1000);
				EnableButtons(FALSE);
			} else {
				SetInPlayback(FALSE);
				SetUserLocale();
				LOG(log_playback, 2, ("Playback STEP RETURN\n"));
				return;
			}
			continue;
		}
		if (strncmp(paramLine, "CLEAR", 5) == 0) {
			wTextClear(demoT);
		} else if (strncmp(paramLine, "MESSAGE", 7) == 0) {
			didPause = FALSE;
			wWinTop(demoW);
			demoWinOnTop = TRUE;
			while ((fgets(paramLine, STR_LONG_SIZE, paramFile)) != NULL) {
				paramLineNum++;
#ifdef UTFCONVERT
				ConvertUTF8ToSystem(paramLine);
#endif
				if (IsEND(END_MESSAGE)) {
					break;
				}
				if (strncmp(paramLine, "STEP", 3) == 0) {
					wWinTop(demoW);
					demoWinOnTop = TRUE;
					EnableButtons(TRUE);
					SetInPlayback(FALSE);
					SetUserLocale();
					LOG(log_playback, 2, ("Playback  MESSAGE STEP RETURN\n"));
					return;
				}
				PlaybackMessage(paramLine);
			}
		} else if (strncmp(paramLine, "ROOMSIZE ", 9) == 0) {
			if (ParseRoomSize(paramLine + 9, &roomSize)) {
				SetRoomSize(roomSize);
			}
		} else if (strncmp(paramLine, "SCALE ", 6) == 0) {
			DoSetScale(paramLine + 6);
		} else if (strncmp(paramLine, "REDRAW", 6) == 0) {
			ResolveIndex();
			RecomputeElevations(NULL);
			DoRedraw();
			/*DoChangeNotification( CHANGE_ALL );*/
		} else if (strncmp(paramLine, "COMMAND ", 8) == 0) {
			paramTogglePlaybackHilite = FALSE;
			PlaybackCommand(paramLine, paramLineNum);
		} else if (strncmp(paramLine, "RESET", 5) == 0) {
			paramTogglePlaybackHilite = TRUE;
			InfoMessage("Esc Key Pressed");
			ConfirmReset(TRUE);
		} else if (strncmp(paramLine, "VERSION", 7) == 0) {
			paramVersion = atol(paramLine + 8);
			if (paramVersion > iParamVersion) {
				NoticeMessage(MSG_PLAYBACK_VERSION_UPGRADE, _("Ok"), NULL, paramVersion,
				              iParamVersion, sProdName);
				break;
			}
			if (paramVersion < iMinParamVersion) {
				NoticeMessage(MSG_PLAYBACK_VERSION_DOWNGRADE, _("Ok"), NULL,
				              paramVersion, iMinParamVersion, sProdName);
				break;
			}
		} else if (strncmp(paramLine, "ORIG ", 5) == 0) {
			if (!GetArgs(paramLine + 5, "fff", &zoom, &x, &y)) {
				continue;
			}
			if (zoom == 0.0) {
				double scale_x = mapD.size.x / (mainD.size.x / mainD.scale);
				double scale_y = mapD.size.y / (mainD.size.y / mainD.scale);
				if (scale_x < scale_y) {
					scale_x = scale_y;
				}
				scale_x = ceil(scale_x);
				if (scale_x < 1) {
					scale_x = 1;
				}
				if (scale_x > MAX_MAIN_SCALE) {
					scale_x = MAX_MAIN_SCALE;
				}
				zoom = scale_x;
			}
			mainD.scale = zoom;
			InfoMessage("Zoom Set to %.0f", zoom);
			mainD.orig.x = x;
			mainD.orig.y = y;
			SetMainSize();
			tempD.orig = mainD.orig;
			tempD.size = mainD.size;
			tempD.scale = mainD.scale;

			DoRedraw();

		} else if (strncmp(paramLine, "PAUSE ", 6) == 0) {
			paramTogglePlaybackHilite = TRUE;
			didPause = TRUE;

			if (!GetArgs(paramLine + 6, "l", &timeout)) {
				continue;
			}
			if (timeout > 10000) {
				timeout = 1000;
			}
			if (playbackTimer == 0) {
				wPause(timeout * playbackDelay / 100);
			}
			wFlush();
			if (demoWinOnTop) {
				wWinTop(mainW);
				demoWinOnTop = FALSE;
			}
		} else if (strncmp(paramLine, "BIGPAUSE ", 6) == 0) {
			paramTogglePlaybackHilite = TRUE;
			didPause = FALSE;

			if (playbackTimer == 0) {
				timeout = bigPause * playbackDelay / 100;
				if (timeout <= dragTimeout) {
					timeout = dragTimeout + 1;
				}
				wPause(timeout);
			}
		} else if (strncmp(paramLine, "KEYSTATE ", 9) == 0) {
			if (strchr("0123456789", paramLine[9])) {
				playbackKeyState = atoi(paramLine + 9);
			} else {
				playbackKeyState = 0;
				if (strchr(paramLine + 9, 'S')) {
					playbackKeyState |= WKEY_SHIFT;
				}
				if (strchr(paramLine + 9, 'C')) {
					playbackKeyState |= WKEY_CTRL;
				}
				if (strchr(paramLine + 9, 'A')) {
					playbackKeyState |= WKEY_ALT;
				}
			}
		} else if (strncmp(paramLine, "TIMESTART", 9) == 0) {
			playbackTimer = wGetTimer();
			ParamTurnOffDelays(true);
		} else if (strncmp(paramLine, "TIMEEND", 7) == 0) {
			if (playbackTimer == 0) {
				NoticeMessage(MSG_PLAYBACK_TIMEEND, _("Ok"), NULL);
			} else {
				playbackTimer = wGetTimer() - playbackTimer;
				sprintf(message, _("Elapsed time %lu\n"), playbackTimer);
				wTextAppend(demoT, message);
				playbackTimer = 0;
				ParamTurnOffDelays(false);
			}
		} else if (strncmp(paramLine, "MEMSTATS", 8) == 0) {
			wTextAppend(demoT, wMemStats());
			wTextAppend(demoT, "\n");
		} else if (strncmp(paramLine, "SNAPSHOT", 8) == 0) {
			if (!documentEnable) {
				continue;
			}
			snapshot_d.dpi = snapshot_d.scale = snapshot_d.orig.x =
			snapshot_d.orig.y = snapshot_d.size.x = snapshot_d.size.y = -1;
			cp = paramLine + 8;
			while (*cp && isspace((unsigned char)*cp)) {
				cp++;
			}
			if (snapshot_d.dpi = strtod(cp, &cq), cp == cq) {
				snapshot_d.dpi = -1;
			} else if (snapshot_d.scale = strtod(cq, &cp), cp == cq) {
				snapshot_d.scale = -1;
			} else if (snapshot_d.orig.x = strtod(cp, &cq), cp == cq) {
				snapshot_d.orig.x = -1;
			} else if (snapshot_d.orig.y = strtod(cq, &cp), cp == cq) {
				snapshot_d.orig.y = -1;
			} else if (snapshot_d.size.x = strtod(cp, &cq), cp == cq) {
				snapshot_d.size.x = -1;
			} else if (snapshot_d.size.y = strtod(cq, &cp), cp == cq) {
				snapshot_d.size.y = -1;
			}
			TakeSnapshot(&snapshot_d);
		} else if (strncmp(paramLine, "DOCUMENT ON", 11) == 0) {
			documentCopy = documentEnable;
		} else if (strncmp(paramLine, "DOCUMENT OFF", 12) == 0) {
			documentCopy = FALSE;
		} else if (strncmp(paramLine, "DOCUMENT COPY", 13) == 0) {
			while ((fgets(paramLine, STR_LONG_SIZE, paramFile)) != NULL) {
				paramLineNum++;
				if (IsEND(END_MESSAGE)) {
					break;
				}
				if (documentCopy && documentFile) {
					fprintf(documentFile, "%s", paramLine);
				}
			}
		} else if (strncmp(paramLine, "DEMOINIT", 8) == 0) {
			DemoInitValues();
		} else if (strncmp(paramLine, "REGRESSION START", 16) == 0) {
			DoRegression(curDemo < 1 ? paramFileName
			             : demoList(curDemo - 1).fileName);
		} else {
			if (strncmp(paramLine, "MOUSE ", 6) == 0) {
				thisCmd = mouseCmd;
			}
			if (strncmp(paramLine, "MAP ", 4) == 0) {
				thisCmd = mouseCmd;
			}
#ifdef UTFCONVERT
			ConvertUTF8ToSystem(paramLine);
#endif
			for (inx = 0; inx < playbackProc_da.cnt; inx++) {
				len = strlen(playbackProc(inx).label);
				if (strncmp(paramLine, playbackProc(inx).label, len) == 0) {
					if (playbackProc(inx).data == NULL) {
						while (paramLine[len] == ' ') {
							len++;
						}
						playbackProc(inx).proc(paramLine + len);
					} else {
						playbackProc(inx).proc((char *)playbackProc(inx).data);
					}
					break;
				}
			}
			if (thisCmd == mouseCmd) {
				EnableButtons(FALSE);
				playbackKeyState = 0;
			}
			if (inx == playbackProc_da.cnt) {
				NoticeMessage(MSG_PLAYBACK_UNK_CMD, _("Ok"), NULL, paramLineNum,
				              paramLine);
			}
		}
		//		lastCmd = thisCmd;
		wFlush();
		if (pauseDemo) {
			EnableButtons(TRUE);
			pauseDemo = FALSE;
			SetInPlayback(FALSE);
			SetUserLocale();
			LOG(log_playback, 2, ("Playback RETURN PAUSE\n"));
			return;
		}
	}
	if (paramFile) {
		fclose(paramFile);
		paramFile = NULL;
	}
	if (documentFile) {
		fclose(documentFile);
		documentFile = NULL;
	}
	SetInPlayback(FALSE);
	PlaybackQuit();
	SetUserLocale();
	LOG(log_playback, 2, ("Playback RETURN\n"));
}

static int StartPlayback(int cnt, char **pathName, void *context)
{
	CHECK(pathName != NULL);
	CHECK(cnt == 1);

	SetCurrentPath(MACROPATHKEY, pathName[0]);
	paramFile = fopen(pathName[0], "r");
	if (paramFile == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Playback"),
		              pathName[0], strerror(errno));
		return FALSE;
	}

	paramFileName = strdup(pathName[0]);

	PlaybackSetup();
	curDemo = -1;
	UndoSuspend();
	wWinBlockEnable(FALSE);
	Playback();
	free(paramFileName);
	paramFileName = NULL;

	return TRUE;
}


static void DemoFinish(paramGroup_p demoPGp)
{
	if ( inPlayback ) {
		SetInPlayback(FALSE);
	} else {
		// We're waiting for the user to press 'Step'
		PlaybackQuit();
	}
	SetUserLocale();
}


static void DoDemoButton(void *command)
{
	switch (VP2L(command)) {
	case 0:
		/* step */
		playbackNonStop = (wGetKeyStateFromButton() & WKEY_SHIFT) != 0;
		Playback();
		break;
	case 1:
		if (curDemo == -1) {
			DoSaveAs(NULL);
		} else {
			if (inPlayback) {
				// Shouldn't get here
				LOG(log_playback, 2, ("Playback RETURN NEXT\n"));
				break;
			}
			/* next */
			if (paramFile) {
				fclose(paramFile);
			}
			paramFile = NULL;
			wTextClear(demoT);
			if ((wGetKeyStateFromButton() & WKEY_SHIFT) != 0) {
				if (curDemo >= 2) {
					curDemo -= 2;
				} else {
					curDemo = 0;
				}
			}
			Playback();
		}
		break;

	case 2:
		/* pause */
		pauseDemo = TRUE;
		break;

	case 3:
		/* quit */
		DemoFinish( &demoPG );
		break;

	default:;
	}
}

static void DemoDlgUpdate(paramGroup_cp pg, int inx, void *valueP)
{
	if (inx != I_DEMOSPEED) {
		return;
	}
	SetPlaybackSpeed((wIndex_t) * (long *)valueP);
}

static void CreateDemoW(void)
{
	char *title = MakeWindowTitle(_("Demo"));
	demoW = FormCreateDialog(&demoPG, title, N_("Quit"), DemoFinish, NULL, NULL,
	                         FALSE, F_RESIZE, DemoDlgUpdate);

	wComboBoxAddValue(demoSpeedL, _("Slowest"), I2VP(0));
	wComboBoxAddValue(demoSpeedL, _("Slow"), I2VP(1));
	wComboBoxAddValue(demoSpeedL, _("Normal"), I2VP(2));
	wComboBoxAddValue(demoSpeedL, _("Fast"), I2VP(3));
	wComboBoxAddValue(demoSpeedL, _("Faster"), I2VP(4));
	wComboBoxAddValue(demoSpeedL, _("Fastest"), I2VP(5));
	wComboBoxSetIndex(demoSpeedL, (wIndex_t)playbackSpeed);
	playbackFile_fs = wFilSelCreate(mainW, FS_LOAD, 0, title, sRecordFilePattern,
	                                StartPlayback, NULL);
}

EXPORT void DoPlayBack(void *context)
{
	if (demoW == NULL) {
		CreateDemoW();
	}
	wButtonSetLabel(demoNext, _("Save"));
	wFilSelect(playbackFile_fs, GetCurrentPath(MACROPATHKEY));
}

/*****************************************************************************
 *
 * DEMO
 *
 */

static char *demoInitParams[] = {
	"layout title1 XTrackCAD", "layout title2 Demo", "GROUP layout",
	"display tunnels 1", "display endpt 2", "display labelenable 0",
	"display description-fontsize 48", "display labelscale 8",
	"display layoutlabels 6", "display tworailscale 16", "display tiedraw 0",
	"pref mingridspacing 5",
	//	"pref balloonhelp 1",
	"display hotbarlabels 1", "display mapscale 64", "display livemap 0",
	"display carhotbarlabels 1", "display hideTrainsInTunnels 0",
	"GROUP display", "pref turntable-angle 15.00", "cmdopt preselect 1",
	"pref coupling-speed-max 100", "cmdopt rightclickmode 0", "GROUP cmdopt",
	"pref checkpoint 0", "pref units 0", "pref dstfmt 1", "pref anglesystem 0",
	"pref minlength 0.100", "pref connectdistance 0.100",
	"pref connectangle 1.000", "pref dragpixels 20", "pref dragtimeout 500",
	"display autoPan 0", "display listlabels 7", "layout mintrackradius 1.000",
	"layout maxtrackgrade 5.000", "display trainpause 300", "GROUP pref",
	"rgbcolor snapgrid 65280", "rgbcolor marker 16711680", "rgbcolor border 0",
	"rgbcolor crossmajor 16711680", "rgbcolor crossminor 255",
	"rgbcolor normal 0", "rgbcolor selected 16711680",
	"rgbcolor profile 16711935", "rgbcolor exception 16711808",
	"rgbcolor tie 16744448", "GROUP rgbcolor", "easement val 0.000",
	"easement r 0.000", "easement x 0.000", "easement l 0.000",
	"GROUP easement", "grid horzspacing 12.000", "grid horzdivision 12",
	"grid horzenable 0", "grid vertspacing 12.000", "grid vertdivision 12",
	"grid vertenable 0", "grid origx 0.000", "grid origy 0.000",
	"grid origa 0.000", "grid show 0", "GROUP grid",
	//	"misc toolbarset 65535",
	//	"misc cur-turnout-ep 0",
	"GROUP misc", "sticky set 67108863", /* 0x3ffffff - all */
	"GROUP sticky",
	//	"newFixedTrack hide 0",
	//	"layer button-count 10",
	"cmdopt selectmode 0", "cmdopt selectzero 1",
	//	"rescale change-dim 0",
	NULL
};

static void DemoInitValues(void)
{
	int inx;
	char **cpp;
	static playbackProc_p paramPlaybackProc = NULL;
	static coOrd roomSize = {96.0, 48.0};
	char scaleName[10];
	if (paramPlaybackProc == NULL) {
		for (inx = 0; inx < playbackProc_da.cnt; inx++) {
			if (strncmp("PARAMETER", playbackProc(inx).label, 9) == 0) {
				paramPlaybackProc = playbackProc(inx).proc;
				break;
			}
		}
	}
	SetRoomSize(roomSize);
	strcpy(scaleName, "DEMO");
	DoSetScale(scaleName);
	if (paramPlaybackProc == NULL) {
		wNoticeWithIcon(NT_INFORMATION, _("Can not find PARAMETER playback proc"),
		                _("Ok"), NULL);
		return;
	}
	for (cpp = demoInitParams; *cpp; cpp++) {
		paramPlaybackProc(*cpp);
	}
	// Have to do this manually
	oldMagneticSnap = MagneticSnap(TRUE);
}

static void DoDemo(void *demoNumber)
{

	if (demoW == NULL) {
		CreateDemoW();
	}
	wButtonSetLabel(demoNext, _("Next"));
	curDemo = (int)VP2L(demoNumber);
	if (curDemo < 0 || curDemo >= demoList_da.cnt) {
		NoticeMessage(MSG_DEMO_BAD_NUM, _("Ok"), NULL, curDemo);
		return;
	}
	PlaybackSetup();
	playbackNonStop = (wGetKeyStateFromButton() & WKEY_SHIFT) != 0;
	paramFile = NULL;
	Playback();
}

static BOOL_T ReadDemo(char *line)
{
	static wMenu_p m = NULL;
	char *cp;
	char *path;

	if (m == NULL) {
		m = demoM;
	}

	if (strncmp(line, "DEMOGROUP ", 10) == 0) {
		m = wMenuMenuCreate(demoM, NULL, _(line + 10));

	} else if (strncmp(line, "DEMO ", 5) == 0) {
		if (line[5] != '"') {
			goto error;
		}
		cp = line + 6;
		while (*cp && *cp != '"') {
			cp++;
		}
		if (!*cp) {
			goto error;
		}
		*cp++ = '\0';
		while (*cp && *cp == ' ') {
			cp++;
		}
		if (strlen(cp) == 0) {
			goto error;
		}
		DYNARR_APPEND(demoList_t, demoList_da, 10);
		demoList(demoList_da.cnt - 1).title = MyStrdup(_(line + 6));
		MakeFullpath(&path, libDir, "demos", cp, NULL);
		demoList(demoList_da.cnt - 1).fileName = path;
		wMenuPushCreate(m, NULL, _(line + 6), 0, DoDemo, I2VP(demoList_da.cnt - 1));
	}
	return TRUE;
error:
	InputError("Expected 'DEMO \"<Demo Name>\" <File Name>'", TRUE);
	return FALSE;
}

// static void ParamPlayback( char * );
// static void ParamCheck( char * );
/**
 * Filter demoList_da down to just the demo(s) named in target, matched
 * case-insensitively against each demo's file basename, with or without
 * the .xtr extension. target may name more than one demo, comma-separated;
 * matches run in the order given. Exits the process with an error listing
 * the valid names if nothing matches, rather than silently falling back to
 * running everything.
 *
 * \param target IN comma-separated demo name(s), e.g. "dmease,dmhelix.xtr"
 */
static void FilterDemoList(const char *target)
{
	dynArr_t filtered;
	char *names, *name, *savePtr;
	int i;
	BOOL_T anyMatched = FALSE;

	DYNARR_INIT(demoList_t, filtered);
	names = MyStrdup(target);
	for (name = strtok_r(names, ",", &savePtr); name != NULL;
	     name = strtok_r(NULL, ",", &savePtr)) {
		BOOL_T matched = FALSE;
		for (i = 0; i < demoList_da.cnt; i++) {
			const char *base = FindFilename(demoList(i).fileName);
			size_t nameLen = strlen(name);
			if (strcasecmp(base, name) == 0 ||
			    (strncasecmp(base, name, nameLen) == 0 &&
			     strcasecmp(base + nameLen, ".xtr") == 0)) {
				DYNARR_APPEND(demoList_t, filtered, 5);
				DYNARR_LAST(demoList_t, filtered) = demoList(i);
				matched = TRUE;
			}
		}
		if (!matched) {
			lprintf("-T: no demo matches '%s'\n", name);
		}
		anyMatched |= matched;
	}
	MyFree(names);
	if (!anyMatched) {
		lprintf("-T: none of the requested demo(s) matched. Valid names:\n");
		for (i = 0; i < demoList_da.cnt; i++) {
			lprintf("  %s\n", FindFilename(demoList(i).fileName));
		}
		exit(1);
	}
	demoList_da = filtered;
}

/**
 * Run regression tests. If target is non-NULL, only the named demo(s) run
 * (see FilterDemoList) instead of the full suite in xtrkcad.xtq.
 *
 * \param target IN NULL for the full suite, else a comma-separated demo list
 * return	number of failed tests
 */
EXPORT int RegressionTestAll(const char *target)
{
	playbackNonStop = TRUE;
	SetPlaybackSpeed(5);
	CreateDemoW();
	if (target != NULL) {
		FilterDemoList(target);
	}
	curDemo = 0;
	PlaybackSetup();
	Playback();
	return nRegressionFail;
}

/****************************************************************************
 *
 *
 *
 */

static BOOL_T paramCheckShowErrors = TRUE;
static int paramCheckErrorCount = 0;

static BOOL_T disablePlaybackDelays = FALSE;

void SimulateButtonClick(wControl_p control)
{
	if (!disablePlaybackDelays && control) {
		wControlHilite(control, TRUE);
		wFlush();
		wPause(500);
		wControlHilite(control, FALSE);
		wFlush();
	}
}

static void ParamPlayback(char *line)
{
	paramGroup_cp pg;
	paramData_p p;
	long valL;
	FLOAT_T valF, valF1;
	size_t len, len1, len2;
	wIndex_t inx;
	void *listContext, *itemContext;
	long rgb;
	wDrawColor dc;
	wButton_p button;
	paramDrawData_t *ddp;
	wAction_t a;
	coOrd pos;
	char *valS;
#ifdef DESCRIBENOTIMPL
	// TODO Remove once describe is implemented
	if (strncmp(line, "describe ", 9) == 0) {
		printf("DESCRIBENOTIMPL: %s\n", line);
		return;
	}
#endif
	if (strncmp(line, "GROUP ", 6) == 0) {
#ifdef PGPROC
		pg = DialogGroupFind(line + 6);
		if (pg == NULL) {
			return;
		}
		//		for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
		//			pg = paramGroups(inx);
		//			if ( pg->name && strncmp( line+6, pg->name, strlen(
		// pg->name ) ) == 0 ) {
		if (pg->proc) {
			pg->proc(PGACT_PARAM, pg->action);
		}
		pg->action = 0;
//			}
//		}
#endif
		return;
	}

	if ( wToggleGroupExists( line ) ) {
		wBool_t bActive = wToggleGroupGetActive( line );
		bActive = ! bActive;
		wToggleGroupSetActive( line, bActive );
		return;
	}

	pg = DialogGroupFind(line);
	if (pg != NULL) {

		//	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
		//	  pg = paramGroups(inx);
		//	  if ( pg->nameStr == NULL )
		//		  continue;
		//	  len1 = strlen( pg->nameStr );
		//	  if ( strncmp( pg->nameStr, line, len1 ) != 0 ||
		//		   line[len1] != ' ' )
		//		  continue;
		len1 = strlen(pg->nameStr);
		for (p = pg->paramPtr, inx = 0; inx < pg->paramCnt; p++, inx++) {
			if (p->nameStr == NULL) {
				continue;
			}
			len2 = strlen(p->nameStr);
			if (strncmp(p->nameStr, line + len1 + 1, len2) != 0 ||
			    (line[len1 + 1 + len2] != ' ' && line[len1 + 1 + len2] != '\0')) {
				continue;
			}
			len = len1 + 1 + len2 + 1;
			if (p->type != PD_DRAW && p->type != PD_MESSAGE && p->type != PD_MENU &&
			    p->type != PD_MENUITEM)
				if (p->control) {
					wControlHilite(p->control, TRUE);
				}
			switch (p->type) {
			case PD_BUTTON:
				if (p->valueP) {
					((wButtonCallBack_p)(p->valueP))(p->context);
				}
				if (playbackTimer == 0) {
					SimulateButtonClick(p->control);
				}
				break;
			case PD_LONG:
				valL = atol(line + len);
				if (p->valueP) {
					*(long *)p->valueP = valL;
				}
				if (p->control) {
					wEntrySetValue((wEntry_p)p->control, FormatLong(valL));
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_RADIO:
				valL = atol(line + len);
				if (p->valueP) {
					*(long *)p->valueP = valL;
				}
				if (p->control) {
					wRadioSetValue((wChoice_p)p->control, valL);
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_TOGGLE:
				valL = atol(line + len);
				if (p->valueP) {
					*(long *)p->valueP = valL;
				}
				if (p->control) {
					wToggleSetValue((wChoice_p)p->control, valL);
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_LIST:
			case PD_DROPLIST:
			case PD_COMBOLIST:
				line += len;
				valL = strtol(line, &valS, 10);
				if (valS) {
					valS++;
				} else {
					valS = "";
				}
				if (p->control != NULL) {
					if ((p->option & PDO_LISTINDEX) == 0) {
						if (valL < 0) {
							wListSetValue((wList_p)p->control, valS);
						} else {
							valL = wListFindValue((wList_p)p->control, valS);
							if (valL < 0) {
								NoticeMessage(MSG_PLAYBACK_LISTENTRY, _("Ok"), NULL, line);
								break;
							}
							wListSetIndex((wList_p)p->control, (wIndex_t)valL);
						}
					} else {
						wListSetIndex((wList_p)p->control, (wIndex_t)valL);
					}
					wFlush();
					wListGetValues((wList_p)p->control, message, sizeof message,
					               &listContext, &itemContext);
				} else if ((p->option & PDO_LISTINDEX) == 0) {
					break;
				}
				if (p->valueP) {
					*(wIndex_t *)p->valueP = (wIndex_t)valL;
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_COLORLIST:
				line += len;
				rgb = atol(line);
				dc = wDrawFindColor(rgb);
				if (p->control) {
					wColorSelectButtonSetColor((wButton_p)p->control, dc);
				}
				if (p->valueP) {
					*(wDrawColor *)p->valueP = dc;
				}
				if (pg->changeProc) {
					/* COLORNOP */
					pg->changeProc(pg, inx, &valL);
				}
				break;
			case PD_FLOAT:
				SetCLocale();
				valF = valF1 = atof(line + len);
				SetUserLocale();
				if (p->valueP) {
					*(FLOAT_T *)p->valueP = valF;
				}
				if (p->option & PDO_DIM) {
					if (p->option & PDO_SMALLDIM) {
						valS = FormatSmallDistance(valF);
					} else {
						valS = FormatDistance(valF);
					}
				} else {
					if (p->option & PDO_ANGLE)
						valF1 =
						        NormalizeAngle((angleSystem == ANGLE_POLAR) ? valF1 : -valF1);
					valS = FormatFloat(valF);
				}
				if (p->control) {
					wEntrySetValue((wEntry_p)p->control, valS);
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, &valF);
				}
				break;
			case PD_STRING:
			case PD_TEXT:
				line += len;
				while (*line == ' ') {
					line++;
				}
				Stripcr(line);
				if (p->valueP) {
					strcpy((char *)p->valueP, line);
				}
				if (p->control) {
					if (p->type == PD_STRING) {
						wEntrySetValue((wEntry_p)p->control, line);
						p->bInvalid = (p->option & PDO_NOTBLANK) && strlen(line) == 0;
					} else {
						wTextClear((wText_p)p->control);
						wTextAppend((wText_p)p->control, line);
					}
					wFlush();
				}
				if (pg->changeProc) {
					pg->changeProc(pg, inx, line);
				}
				break;
			case PD_DRAW:
				ddp = (paramDrawData_t *)p->winData;
				if (ddp->action == NULL) {
					break;
				}
				a = (wAction_t)strtol(line + len, &line, 10);
				pos.x = strtod(line, &line);
				pos.y = strtod(line, NULL);
				PlaybackMouse(ddp->action, ddp->d, a, pos, drawColorBlack);
				break;
			case PD_MESSAGE:
			case PD_MENU:
			case PD_BITMAP:
				break;
			case PD_MENUITEM:
				if (p->valueP) {
					if ((p->option & IC_PLAYBACK_PUSH) != 0) {
						PlaybackButtonMouse((wIndex_t)VP2L(p->context));
					}
					((wButtonCallBack_p)(p->valueP))(p->context);
				}
				break;
			case PD_SCALE:
			case PD_NOTEBOOK:
			case PD_TAG:
				break;
			}
			if (p->type != PD_DRAW && p->type != PD_MESSAGE && p->type != PD_MENU &&
			    p->type != PD_MENUITEM)
				if (p->control) {
					wControlHilite(p->control, FALSE);
				}
#ifdef HUH
			pg->action |= p->change;
#endif
			return;
		}
		button = NULL;
		if (strcmp("ok", line + len1 + 1) == 0) {
			if (pg->okB) {
				wControlHilite((wControl_p)pg->okB, TRUE);
			}
			if (pg->okProc) {
				pg->okProc(pg);
			}
			button = pg->okB;
		} else if (strcmp("cancel", line + len1 + 1) == 0) {
			if (pg->cancelB) {
				wControlHilite((wControl_p)pg->cancelB, TRUE);
			}
			if (pg->cancelProc) {
				pg->cancelProc(pg);
			}
			button = pg->cancelB;
		}
		if (playbackTimer == 0) {
			SimulateButtonClick(button);
		}
		if (button) {
			wControlHilite((wControl_p)button, FALSE);
		}
		if (!button) {
			// Headless -T run: NoticeMessage() is a modal dialog that would hang
			// forever with nothing to click it. A group matched but no field/ok/
			// cancel token did -- most likely a stale index from a descData_t
			// layout change (see cdescribe.c's CreateDescribeField); log instead
			// of hanging so this is at least visible in the test output.
			if (bRunTests) {
				lprintf( "PLAYBACK: unknown PARAM (matched group '%s', no field/ok/cancel): %s\n",
				         pg->nameStr, line );
			} else {
				NoticeMessage("Unknown PARAM: %s", _("Ok"), NULL, line);
			}
		}
		return;
	}
	if (bRunTests) {
		lprintf( "PLAYBACK: unknown PARAM (no matching dialog group): %s\n", line );
	} else {
		NoticeMessage("Unknown PARAM: %s", _("Ok"), NULL, line);
	}
}

static void ParamCheck(char *line)
{
	paramGroup_cp pg;
	paramData_p p;
	long valL;
	FLOAT_T valF, diffF;
	size_t len, len1, len2;
	wIndex_t inx;
	void *listContext, *itemContext;
	char *valS;
	char *expVal = NULL, *actVal = NULL;
	char expNum[20], actNum[20];
	BOOL_T hasError = FALSE;
	FILE *f;

	pg = DialogGroupFind(line);
	if (pg == NULL) {
		return;
	}
	//	for ( inx=0; inx<paramGroups_da.cnt; inx++ ) {
	//	  pg = paramGroups(inx);
	//	  if ( pg->nameStr == NULL )
	//		  continue;
	len1 = strlen(pg->nameStr);
	//	  if ( strncmp( pg->nameStr, line, len1 ) != 0 ||
	//		   line[len1] != ' ' )
	//		  continue;
	for (p = pg->paramPtr, inx = 0; inx < pg->paramCnt; p++, inx++) {
		if (p->nameStr == NULL) {
			continue;
		}
		len2 = strlen(p->nameStr);
		if (strncmp(p->nameStr, line + len1 + 1, len2) != 0 ||
		    (line[len1 + 1 + len2] != ' ' && line[len1 + 1 + len2] != '\0')) {
			continue;
		}
		if (p->valueP == NULL) {
			return;
		}
		len = len1 + 1 + len2 + 1;
		switch (p->type) {
		case PD_BUTTON:
			break;
		case PD_LONG:
		case PD_RADIO:
		case PD_TOGGLE:
			valL = atol(line + len);
			if (*(long *)p->valueP != valL) {
				sprintf(expNum, "%ld", valL);
				sprintf(actNum, "%ld", *(long *)p->valueP);
				expVal = expNum;
				actVal = actNum;
				hasError = TRUE;
			}
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			line += len;
			if (p->control == NULL) {
				break;
			}
			valL = strtol(line, &valS, 10);
			if (valS) {
				if (valS[0] == ' ') {
					valS++;
				}
			} else {
				valS = "";
			}
			if ((p->option & PDO_LISTINDEX) != 0) {
				if (*(long *)p->valueP != valL) {
					sprintf(expNum, "%ld", valL);
					sprintf(actNum, "%d", *(wIndex_t *)p->valueP);
					expVal = expNum;
					actVal = actNum;
					hasError = TRUE;
				}
			} else {
				wListGetValues((wList_p)p->control, message, sizeof message,
				               &listContext, &itemContext);
				if (strcasecmp(message, valS) != 0) {
					expVal = valS;
					actVal = message;
					hasError = TRUE;
				}
			}
			break;
		case PD_COLORLIST:
			break;
		case PD_FLOAT:
			valF = atof(line + len);
			diffF = fabs(*(FLOAT_T *)p->valueP - valF);
			if (diffF > 0.001) {
				sprintf(expNum, "%0.3f", valF);
				sprintf(actNum, "%0.3f", *(FLOAT_T *)p->valueP);
				expVal = expNum;
				actVal = actNum;
				hasError = TRUE;
			}
			break;
		case PD_STRING:
			line += len;
			while (*line == ' ') {
				line++;
			}
			wEntryGetValue((wEntry_p)p->control);
			if (strcasecmp(line, (char *)p->valueP) != 0) {
				expVal = line;
				actVal = (char *)p->valueP;
				hasError = TRUE;
			}
			break;
		case PD_DRAW:
		case PD_MESSAGE:
		case PD_TEXT:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
			break;
		case PD_SCALE:
		case PD_NOTEBOOK:
		case PD_TAG:
			break;
		}
		if (hasError) {
			f = fopen("error.log", "a");
			if (f == NULL) {
				NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, "PARAMCHECK LOG",
				              "error.log", strerror(errno));
			} else {
				fprintf(f, "CHECK: %s:%d: %s-%s: exp: %s, act=%s\n", paramFileName,
				        paramLineNum, pg->nameStr, p->nameStr, expVal, actVal);
				fclose(f);
			}
			if (paramCheckShowErrors)
				NoticeMessage("CHECK: %d: %s-%s: exp: %s, act=%s", _("Ok"), NULL,
				              paramLineNum, pg->nameStr, p->nameStr, expVal, actVal);
			paramCheckErrorCount++;
		}
		return;
		//	 }
	}
	if (bRunTests) {
		lprintf( "PLAYBACK: unknown PARAMCHECK (no matching dialog group): %s\n",
		         line );
	} else {
		NoticeMessage("Unknown PARAMCHECK: %s", _("Ok"), NULL, line);
	}
}

static long ParamIntRestore(paramGroup_cp pg, int class)
{
	long change = 0;
	int inx;
	paramData_p p;
	FLOAT_T valR;
	char *valS;
	paramOldData_t *oldP;

	for (p = pg->paramPtr, inx = 0; p < &pg->paramPtr[pg->paramCnt]; p++, inx++) {
		oldP = (class == 0) ? &p->oldD : &p->demoD;
		if ((p->option & PDO_DLGIGNORE) != 0) {
			continue;
		}
		if (p->valueP == NULL) {
			continue;
		}
		switch (p->type) {
		case PD_LONG:
			if (*(long *)p->valueP != oldP->l) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
				*(long *)p->valueP = oldP->l;
				if (p->control) {
					wEntrySetValue((wEntry_p)p->control, FormatLong(oldP->l));
				}
				change |= (1L << inx);
			}
			break;
		case PD_RADIO:
			if (*(long *)p->valueP != oldP->l) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
				*(long *)p->valueP = oldP->l;
				if (p->control) {
					wRadioSetValue((wChoice_p)p->control, oldP->l);
				}
				change |= (1L << inx);
			}
			break;
		case PD_TOGGLE:
			if (*(long *)p->valueP != oldP->l) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
				*(long *)p->valueP = oldP->l;
				if (p->control) {
					wToggleSetValue((wChoice_p)p->control, oldP->l);
				}
				change |= (1L << inx);
			}
			break;
		case PD_LIST:
		case PD_DROPLIST:
		case PD_COMBOLIST:
			if (*(wIndex_t *)p->valueP != (wIndex_t)oldP->l) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
				*(wIndex_t *)p->valueP = (wIndex_t)oldP->l;
				if (p->control) {
					wListSetIndex((wList_p)p->control, (wIndex_t)oldP->l);
				}
				change |= (1L << inx);
			}
			break;
		case PD_COLORLIST:
			if (*(wDrawColor *)p->valueP != oldP->dc) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
				*(wDrawColor *)p->valueP = oldP->dc;
				if (p->control)
					wColorSelectButtonSetColor((wButton_p)p->control,
					                           oldP->dc); /* COLORNOP */
				change |= (1L << inx);
			}
			break;
		case PD_FLOAT:
			if (*(FLOAT_T *)p->valueP != oldP->f) {
				/*if ((p->option&PDO_NORSTUPD)==0)*/
				*(FLOAT_T *)p->valueP = oldP->f;
				if (p->control) {
					valR = oldP->f;
					if (p->option & PDO_DIM) {
						if (p->option & PDO_SMALLDIM) {
							valS = FormatSmallDistance(valR);
						} else {
							valS = FormatDistance(valR);
						}
					} else {
						if (p->option & PDO_ANGLE)
							valR =
							        NormalizeAngle((angleSystem == ANGLE_POLAR) ? valR : -valR);
						valS = FormatFloat(valR);
					}
					wEntrySetValue((wEntry_p)p->control, valS);
				}
				change |= (1L << inx);
			}
			break;
		case PD_STRING:
			if (oldP->s && strcmp((char *)p->valueP, oldP->s) != 0) {
				((char *)p->valueP)[0] = '\0';
				strncat((char *)p->valueP, oldP->s, p->max_string - 1);
				if (p->control) {
					wEntrySetValue((wEntry_p)p->control, (char *)p->valueP);
				}
				change |= (1L << inx);
			}
			break;
		case PD_MESSAGE:
		case PD_BUTTON:
		case PD_DRAW:
		case PD_TEXT:
		case PD_MENU:
		case PD_MENUITEM:
		case PD_BITMAP:
			break;
		default:
			break;
		}
	}
#ifdef PGPROC
	if (pg->proc) {
		pg->proc(PGACT_RESTORE, change);
	}
#endif
	return change;
}

static void ParamIntSave(paramGroup_cp pg, int class)
{
	paramData_p p;
	paramOldData_t *oldP;

	for (p = pg->paramPtr; p < &pg->paramPtr[pg->paramCnt]; p++) {
		oldP = (class == 0) ? &p->oldD : &p->demoD;
		if (p->valueP) {
			switch (p->type) {
			case PD_LONG:
			case PD_RADIO:
			case PD_TOGGLE:
				oldP->l = *(long *)p->valueP;
				break;
			case PD_LIST:
			case PD_DROPLIST:
			case PD_COMBOLIST:
				oldP->l = *(wIndex_t *)p->valueP;
				break;
			case PD_COLORLIST:
				oldP->dc = *(wDrawColor *)p->valueP;
				break;
			case PD_FLOAT:
				oldP->f = *(FLOAT_T *)p->valueP;
				break;
			case PD_STRING:
				if (oldP->s) {
					MyFree(oldP->s);
				}
				oldP->s = MyStrdup((char *)p->valueP);
				break;
			case PD_MESSAGE:
			case PD_BUTTON:
			case PD_DRAW:
			case PD_TEXT:
			case PD_MENU:
			case PD_MENUITEM:
			case PD_BITMAP:
				break;
			default:
				break;
			}
		}
	}
}

EXPORT void ParamRestoreAll(void)
{
	for (paramGroup_cp *ppg = DialogGroupIter(NULL); ppg;
	     ppg = DialogGroupIter(ppg)) {
		ParamIntRestore(*ppg, 1);
	}
	if (paramCheckErrorCount > 0) {
		NoticeMessage("PARAMCHECK: %d errors", "Ok", NULL, paramCheckErrorCount);
	}
}

EXPORT void ParamSaveAll(void)
{
	for (paramGroup_cp *ppg = DialogGroupIter(NULL); ppg;
	     ppg = DialogGroupIter(ppg)) {
		ParamIntSave(*ppg, 1);
		((paramGroup_p)(*ppg))->action = 0; // CAST_AWAY_CONST - TODO why?
	}
	paramCheckErrorCount = 0;
}

/**
 * Inform about file operation in progress. While files are read, some
 * operations in the params library must be disabled
 *
 * \param state	TRUE if file operation starts, FALSE when done
 */

static BOOL_T bInReadTracks = FALSE;

EXPORT void ParamSetInReadTracks(BOOL_T state) { bInReadTracks = state; }

EXPORT void ParamTurnOffDelays(BOOL_T disable)
{
	disablePlaybackDelays = disable;
}

void ParamDlgProc(wWin_p win, winProcEvent e, void *refresh, void *data)
{
	paramGroup_cp pg = (paramGroup_cp)data;
	switch (e) {
	case wClose_e:
		if (pg->changeProc) {
			pg->changeProc(pg, -1, NULL);
		}
		if ((pg->options & PGO_NODEFAULTPROC) == 0) {
			DefaultProc(win, wClose_e, data);
		}
		break;
	case wResize_e:
		if ((wControl_p)win == mapW) {
			if (!bInReadTracks) {
				pg->changeProc(pg, wResize_e, NULL);
			}
		}
		break;
	default:
		break;
	}
}

EXPORT BOOL_T MacroInit(void)
{
	AddParam("DEMOGROUP ", ReadDemo);
	AddParam("DEMO ", ReadDemo);
	AddPlaybackProc("PARAMETER", ParamPlayback, NULL);
	AddPlaybackProc("PARAMCHECK", ParamCheck, NULL);

	recordMouseMoves = (getenv("XTRKCADNORECORDMOUSEMOVES") == NULL);

	rightDragColor = drawColorRed;
	leftDragColor = drawColorBlue;

	arrow0_bm =
	        wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH, "arrow0.png");
	arrow0_shift_bm = wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH,
	                                    "arrow0_shift.png");
	arrow0_ctl_bm = wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH,
	                                  "arrow0_ctl.png");
	arrow3_bm =
	        wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH, "arrow3.png");
	arrow3_shift_bm = wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH,
	                                    "arrow3_shift.png");
	arrow3_ctl_bm = wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH,
	                                  "arrow3_ctl.png");
	arrowr3_bm =
	        wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH, "arrowr3.png");
	arrowr3_shift_bm = wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH,
	                                     "arrowr3_shift.png");
	arrowr3_ctl_bm = wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH,
	                                   "arrowr3_ctl.png");
	arrows_bm =
	        wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH, "arrows.png");
	flash_bm =
	        wDrawBitMapCreate(mainD.d, 12, 12, XTRKCAD_SYMBOLS_PATH, "flash.png");

	FormRegister(&recordPG);
	FormRegister(&demoPG);

	log_playbackCursor = LogFindIndex("playbackcursor");
	log_regression = LogFindIndex("regression");
	log_playback = LogFindIndex("playback");
	log_visualdiff = LogFindIndex("visualdiff");

	return TRUE;
}
