/* file misc.c
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



#include "menu.h"
#include "common.h"
#include "compound.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "include/problemrep.h"
#include "smalldlg.h"
#include "common-ui.h"
#include "ctrain.h"

static paramData_t menuPLs[101] = { { PD_LONG, &toolbarSet, "toolbarset" }, {
		PD_LONG, &curTurnoutEp, "cur-turnout-ep"
	}
};
static paramGroup_t menuPG = { "misc", PGO_RECORD, menuPLs, 2 };

static void InitCmdExport( void );

EXPORT wMenu_p demoM;
EXPORT wMenu_p popup1M, popup2M;
static wMenu_p popup1aM, popup2aM;
static wMenu_p popup1mM, popup2mM;
EXPORT wButton_p undoB;
EXPORT wButton_p redoB;
EXPORT wButton_p zoomUpB;
EXPORT wButton_p zoomDownB;
EXPORT wButton_p zoomExtentsB;
EXPORT wButton_p mapShowB;
static wButton_p magnetsB;
EXPORT wMenuToggle_p mapShowMI;
static wMenuToggle_p magnetsMI;
EXPORT wMenuList_p winList_mi;
EXPORT wMenuList_p fileList_ml;
EXPORT wMenuToggle_p snapGridEnableMI;
EXPORT wMenuToggle_p snapGridShowMI;


/*--------------------------------------------------------------------*/
typedef struct {
	char * label;
	wMenu_p menu;
} menuTrace_t, *menuTrace_p;
static dynArr_t menuTrace_da;
#define menuTrace(N) DYNARR_N( menuTrace_t, menuTrace_da, N )

static void DoMenuTrace(wMenu_p menu, const char * label, void * data)
{
	/*printf( "MENUTRACE: %s/%s\n", (char*)data, label );*/
	if (recordF) {
		fprintf(recordF, "MOUSE 1 %0.3f %0.3f\n", oldMarker.x, oldMarker.y);
		fprintf(recordF, "MENU %0.3f %0.3f \"%s\" \"%s\"\n", oldMarker.x,
		        oldMarker.y, (char*) data, label);
	}
}

EXPORT wMenu_p MenuRegister(const char * label)
{
	wMenu_p m;
	menuTrace_p mt;
	m = wMenuPopupCreate(mainW, label);
	DYNARR_APPEND(menuTrace_t, menuTrace_da, 10);
	mt = &menuTrace(menuTrace_da.cnt - 1);
	mt->label = strdup(label);
	mt->menu = m;
	wMenuSetTraceCallBack(m, DoMenuTrace, mt->label);
	return m;
}

static void MenuPlayback(char * line)
{
	char * menuName, *itemName;
	coOrd pos;
	menuTrace_p mt;

	if (!GetArgs(line, "pqq", &pos, &menuName, &itemName)) {
		return;
	}
	for (mt = &menuTrace(0); mt < &menuTrace(menuTrace_da.cnt); mt++) {
		if (strcmp(mt->label, menuName) == 0) {
			MovePlaybackCursor(&mainD, pos, FALSE, NULL);
			oldMarker = cmdMenuPos = pos;
			wMenuAction(mt->menu, _(itemName));
			return;
		}
	}
	CHECKMSG( FALSE, ("menuPlayback: %s not found", menuName) );
}


/*****************************************************************************
 *
 * TOOLBAR
 *
 */


/*
 * These array control the choices available in the Toolbar setup.
 * For each choice, the text is given and the respective mask is
 * specified in the following array.
 * Note: text and choices must be given in the same order.
 */
static char *AllToolbarLabels[] = { N_("File Buttons"), N_("Print Buttons"), N_("Import/Export Buttons"),
                                    N_("Zoom Buttons"), N_("Undo Buttons"), N_("Easement Button"), N_("SnapGrid Buttons"),
                                    N_("Create Track Buttons"), N_("Layout Control Elements"),
                                    N_("Modify Track Buttons"), N_("Properties/Select"),
                                    N_("Track Group Buttons"), N_("Train Group Buttons"),
                                    N_("Create Misc Buttons"), N_("Ruler Button"),
                                    N_("Layer Buttons"), N_("Hot Bar"),
                                    NULL
                                  };
static long AllToolbarMasks[] = { 1 << BG_FILE, 1<< BG_PRINT, 1<< BG_EXPORTIMPORT,
                                  1<< BG_ZOOM, 1<< BG_UNDO, 1<< BG_EASE, 1 << BG_SNAP, 1 << BG_TRKCRT,
                                  1<< BG_CONTROL, 1<< BG_TRKMOD, 1 << BG_SELECT, 1 << BG_TRKGRP, 1 << BG_TRAIN,
                                  1<< BG_MISCCRT, 1<< BG_RULER, 1 << BG_LAYER, 1 << BG_HOTBAR
                                };

static wMenuToggle_p AllToolbarMI[ COUNT( AllToolbarMasks ) ];

static void ToolbarAction(void * data)
{
	int inx = (int)VP2L(data);
	CHECK( inx >=0 && inx < COUNT( AllToolbarMasks ) );
	wBool_t set = wMenuToggleGet( AllToolbarMI[inx] );
	long mask = AllToolbarMasks[inx];
	if (set) {
		toolbarSet |= mask;
	} else {
		toolbarSet &= ~mask;
	}
	wPrefSetInteger( "misc", "toolbarset", toolbarSet );
	MainProc( mainW, wResize_e, NULL, NULL );
	if (recordF)
		fprintf(recordF, "PARAMETER %s %s %ld", "misc", "toolbarset",
		        toolbarSet);
}

/**
 * Create the Toolbar configuration submenu. Based on two arrays of descriptions and
 * masks, the toolbar submenu is created dynamically.
 *
 * \param toolbarM IN menu to which the toogles will be added
 */

static void CreateToolbarM(wMenu_p toolbarM)
{
	int inx, cnt;
	long *masks;
	char **labels;
	wBool_t set;

	cnt = COUNT(AllToolbarMasks);
	masks = AllToolbarMasks;
	labels = AllToolbarLabels;
	for (inx = 0; inx < cnt; inx++, masks++, labels++) {
		set = (toolbarSet & *masks) != 0;
		AllToolbarMI[inx] = wMenuToggleCreate(toolbarM, "toolbarM", _(*labels), 0, set,
		                                      ToolbarAction, I2VP(inx));
	}
}


/*****************************************************************************
 *
 * ELEVATION HELPERS
 *
 */


static wWin_p addElevW;
#define addElevF (wFloat_p)addElevPD.control
static DIST_T addElevValueV;
static void DoAddElev(void * unused);

static paramFloatRange_t rn1000_1000 = { -1000.0, 1000.0 };
static paramData_t addElevPLs[] = { {
		PD_FLOAT, &addElevValueV, "value",
		PDO_NOPREF|PDO_DIM, &rn1000_1000, NULL, 0
	}
};
static paramGroup_t addElevPG = { "addElev", 0, addElevPLs, COUNT( addElevPLs ) };

static void DoAddElev(void * unused)
{
	ParamLoadData(&addElevPG);
	AddElevations(addElevValueV);
	wHide(addElevW);
}

static void ShowAddElevations(void * unused)
{
	if (selectedTrackCount <= 0) {
		ErrorMessage(MSG_NO_SELECTED_TRK);
		return;
	}
	if (addElevW == NULL)
		addElevW = ParamCreateDialog(&addElevPG,
		                             MakeWindowTitle(_("Change Elevations")), _("Change"), DoAddElev,
		                             wHide, FALSE, NULL, 0, NULL);
	wShow(addElevW);
}

/*****************************************************************************
 *
 * MOVE/ROTATE/INDEX HELPERS
 *
 */


static wWin_p rotateW;
static wWin_p indexW;
static wWin_p moveW;
static double rotateValue;
static char trackIndex[STR_LONG_SIZE];
static coOrd moveValue;
static rotateDialogCallBack_t rotateDialogCallBack;
static indexDialogCallBack_t indexDialogCallBack;
static moveDialogCallBack_t moveDialogCallBack;

static void RotateEnterOk(void * unused);

static paramFloatRange_t rn360_360 = { -360.0, 360.0, 80 };
static paramData_t rotatePLs[] = { { PD_FLOAT, &rotateValue, "rotate", PDO_NOPREF|PDO_ANGLE|PDO_NORECORD, &rn360_360, N_("Angle:") } };
static paramGroup_t rotatePG = { "rotate", 0, rotatePLs, COUNT( rotatePLs ) };

static void IndexEnterOk(void * unused);
static paramData_t indexPLs[] = {
	{ PD_STRING, &trackIndex, "select",	PDO_NOPREF|PDO_NORECORD|PDO_STRINGLIMITLENGTH, I2VP(STR_SIZE-1), N_("Indexes:"), 0, 0, sizeof(trackIndex) }
};
static paramGroup_t indexPG = { "index", 0, indexPLs, COUNT( indexPLs ) };

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static void MoveEnterOk(void * unused);
static paramData_t movePLs[] = {
	{ PD_FLOAT, &moveValue.x, "moveX", PDO_NOPREF|PDO_DIM|PDO_NORECORD, &r_1000_1000, N_("Move X:") },
	{ PD_FLOAT, &moveValue.y, "moveY", PDO_NOPREF|PDO_DIM|PDO_NORECORD, &r_1000_1000, N_("Move Y:") }
};
static paramGroup_t movePG = { "move", 0, movePLs, COUNT( movePLs ) };

static void StartRotateDialog(void * funcVP)
{
	rotateDialogCallBack_t func = funcVP;
	if (rotateW == NULL)
		rotateW = ParamCreateDialog(&rotatePG, MakeWindowTitle(_("Rotate")),
		                            _("Ok"), RotateEnterOk, wHide, FALSE, NULL, 0, NULL);
	ParamLoadControls(&rotatePG);
	rotateDialogCallBack = func;
	wShow(rotateW);
}

static void StartIndexDialog(void * funcVP)
{
	indexDialogCallBack_t func = funcVP;
	if (indexW == NULL)
		indexW = ParamCreateDialog(&indexPG, MakeWindowTitle(_("Select Index")),
		                           _("Ok"), IndexEnterOk, wHide, FALSE, NULL, 0, NULL);
	ParamLoadControls(&indexPG);
	indexDialogCallBack = func;
	trackIndex[0] = '\0';
	wShow(indexW);
}

static void StartMoveDialog(void * funcVP)
{
	moveDialogCallBack_t func = funcVP;
	if (moveW == NULL)
		moveW = ParamCreateDialog(&movePG, MakeWindowTitle(_("Move")), _("Ok"),
		                          MoveEnterOk, wHide, FALSE, NULL, 0, NULL);
	ParamLoadControls(&movePG);
	moveDialogCallBack = func;
	moveValue = zero;
	wShow(moveW);
}

static void MoveEnterOk(void * unused)
{
	ParamLoadData(&movePG);
	moveDialogCallBack(&moveValue);
	wHide(moveW);
}

static void IndexEnterOk(void * unused)
{
	ParamLoadData(&indexPG);
	indexDialogCallBack(trackIndex);
	wHide(indexW);
}

static void RotateEnterOk(void * unused)
{
	ParamLoadData(&rotatePG);
	if (angleSystem == ANGLE_POLAR) {
		rotateDialogCallBack(I2VP(rotateValue * 1000));
	} else {
		rotateDialogCallBack(I2VP(rotateValue * 1000));
	}
	wHide(rotateW);
}


EXPORT void AddMoveMenu(wMenu_p m, moveDialogCallBack_t func)
{
	wMenuPushCreate(m, "", _("Enter Move ..."), 0,
	                StartMoveDialog, func);
}

EXPORT void AddIndexMenu(wMenu_p m, indexDialogCallBack_t func)
{
	wMenuPushCreate(m, "cmdSelectIndex", _("Select Track Index ..."), 0,
	                StartIndexDialog, func);
}

//All values multipled by 100 to support decimal points from PD_FLOAT
EXPORT void AddRotateMenu(wMenu_p m, rotateDialogCallBack_t func)
{
	wMenuPushCreate(m, "", _("180 "), 0, func, I2VP(180000));
	wMenuPushCreate(m, "", _("90  CW"), 0, func, I2VP(90000));
	wMenuPushCreate(m, "", _("45  CW"), 0, func, I2VP(45000));
	wMenuPushCreate(m, "", _("30  CW"), 0, func, I2VP(30000));
	wMenuPushCreate(m, "", _("15  CW"), 0, func, I2VP(15000));
	wMenuPushCreate(m, "", _("15  CCW"), 0, func, I2VP(360000 - 15000));
	wMenuPushCreate(m, "", _("30  CCW"), 0, func, I2VP(360000 - 30000));
	wMenuPushCreate(m, "", _("45  CCW"), 0, func, I2VP(360000 - 45000));
	wMenuPushCreate(m, "", _("90  CCW"), 0, func, I2VP(360000 - 90000));
	wMenuPushCreate(m, "", _("Enter Angle ..."), 0,
	                StartRotateDialog, func);
}

/*****************************************************************************
 *
 * MISC HELPERS
 *
 */

static void ChkLoad(void * unused)
{
	Confirm(_("Load"), DoLoad);
}

static void ChkExamples( void * unused )
{
	Confirm(_("examples"), DoExamples);
}

static void ChkRevert(void * unused)
{
	int rc;

	if (changed) {
		rc = wNoticeEx(NT_WARNING,
		               _("Do you want to return to the last saved state?\n\n"
		                 "Revert will cause all changes done since last save to be lost."),
		               _("&Revert"), _("&Cancel"));
		if (rc) {
			/* load the file */
			char *filename = GetLayoutFullPath();
			LoadTracks(1, &filename, I2VP(1));   //Keep background
		}
	}
}

static char * fileListPathName;
static void AfterFileList(void)
{
	DoFileList(0, NULL, fileListPathName);
}

static void ChkFileList(int index, const char * label, void * data)
{
	fileListPathName = (char*) data;
	Confirm(_("Load"), AfterFileList);
}


static void DoCommandBIndirect(void * cmdInxP)
{
	wIndex_t cmdInx;
	cmdInx = *(wIndex_t*) cmdInxP;
	DoCommandB(I2VP(cmdInx));
}

/**
 * Set magnets state
 */
EXPORT int MagneticSnap(int state)
{
	int oldState = magneticSnap;
	magneticSnap = state;
	wPrefSetInteger("misc", "magnets", magneticSnap);
	wMenuToggleSet(magnetsMI, magneticSnap);
	wButtonSetBusy(magnetsB, (wBool_t) magneticSnap);
	return oldState;
}

/**
 * Toggle magnets on/off
 */
static void MagneticSnapToggle(void * unused)
{
	MagneticSnap(!magneticSnap);
}


EXPORT void SelectFont(void * unused)
{
	wSelectFont(_("XTrackCAD Font"));
}


EXPORT long stickySet = 0;
static wWin_p stickyW;
static const char * stickyLabels[33];
static paramData_t stickyPLs[] = { {
		PD_TOGGLE, &stickySet, "set", 0,
		stickyLabels
	}
};
static paramGroup_t stickyPG = { "sticky", PGO_RECORD, stickyPLs,
                                 COUNT( stickyPLs )
                               };

static void StickyOk(void * unused)
{
	wHide(stickyW);
}


EXPORT void DoSticky(void * unused)
{
	if (!stickyW)
		stickyW = ParamCreateDialog(&stickyPG,
		                            MakeWindowTitle(_("Sticky Commands")), _("Ok"), StickyOk, wHide,
		                            TRUE, NULL, 0, NULL);
	ParamLoadControls(&stickyPG);
	wShow(stickyW);
}

/*****************************************************************************
 *
 * DEBUG DIALOG
 *
 */

static wWin_p debugW;

static int debugCnt = 0;
static paramIntegerRange_t r0_100 = { 0, 100, 80 };
static void DebugOk(void * unused);
static paramData_t debugPLs[30];
static paramData_t p0[] = {
	{ PD_BUTTON, TestMallocs, "test", PDO_DLGHORZ, NULL, N_("Test Mallocs") }
};
static long debug_values[30];
static int debug_index[30];

static paramGroup_t debugPG = { "debug", 0, debugPLs, 0 };

static void DebugOk(void * unused)
{
	for (int i = 0; i<debugCnt; i++) {
		logTable(debug_index[i]).level = debug_values[i];
	}
	wHide(debugW);
}

static void CreateDebugW(void)
{
	debugPG.paramCnt = debugCnt+1;
	ParamRegister(&debugPG);
	debugW = ParamCreateDialog(&debugPG, MakeWindowTitle(_("Debug")), _("Ok"),
	                           DebugOk, wHide, FALSE, NULL, 0, NULL);
	wHide(debugW);
}

static void InitDebug(const char * label, long * valueP)
{
	CHECK( debugCnt+1 < COUNT( debugPLs ) );
	memset(&debugPLs[debugCnt+1], 0, sizeof debugPLs[debugCnt]);
	debugPLs[debugCnt+1].type = PD_LONG;
	debugPLs[debugCnt+1].valueP = valueP;
	debugPLs[debugCnt+1].nameStr = label;
	debugPLs[debugCnt+1].winData = &r0_100;
	debugPLs[debugCnt+1].winLabel = label;
	debugCnt++;
}

static void DebugInit(void * unused)
{

	if (!debugW) {
		debugPLs[0] = p0[0];
		BOOL_T default_line = FALSE;
		debugCnt = 0;    //Reset to start building the dynamic dialog over again
		int i = 0;
		for ( int inx=0; inx<logTable_da.cnt; inx++ ) {
			if (logTable(inx).name[0]) {
				debug_values[i] = logTable(inx).level;
				debug_index[i] = inx;
				InitDebug(logTable(inx).name,&debug_values[i]);
				i++;
			} else {
				if (!default_line) {
					debug_values[i] = logTable(inx).level;
					debug_index[i] = inx;
					InitDebug("Default Trace",&debug_values[i]);
					i++;
					default_line = TRUE;
				}
			}
		}

		//ParamCreateControls( &debugPG, NULL );
		CreateDebugW();
	}
	ParamLoadControls( &debugPG );
	wShow(debugW);
}


/*****************************************************************************
 *
 * ENABLE MENUS
 *
 */

EXPORT void EnableMenus( void )
{
	int inx;
	BOOL_T enable;
	for (inx = 0; inx < menuPG.paramCnt; inx++) {
		if (menuPG.paramPtr[inx].control == NULL) {
			continue;
		}
		if ((menuPG.paramPtr[inx].option & IC_SELECTED) && selectedTrackCount <= 0) {
			enable = FALSE;
		} else if ((programMode == MODE_TRAIN
		            && (menuPG.paramPtr[inx].option & (IC_MODETRAIN_TOO | IC_MODETRAIN_ONLY))
		            == 0)
		           || (programMode != MODE_TRAIN
		               && (menuPG.paramPtr[inx].option & IC_MODETRAIN_ONLY) != 0)) {
			enable = FALSE;
		} else {
			enable = TRUE;
		}
		wMenuPushEnable((wMenuPush_p) menuPG.paramPtr[inx].control, enable);
	}
}
/*****************************************************************************
 *
 * RECENT MESSAGES LIST
 *
 */

static wMenuList_p messageList_ml;
#define MESSAGE_LIST_EMPTY			N_("No Messages")

EXPORT void MessageListAppend(
        char * cp1,
        const char * msgSrc )
{
	static wBool_t messageListIsEmpty = TRUE;
	if ( messageListIsEmpty ) {
		wMenuListDelete(messageList_ml, _(MESSAGE_LIST_EMPTY));
		messageListIsEmpty = FALSE;
	}
	wMenuListAdd(messageList_ml, 0, cp1, _(msgSrc));
}


static void ShowMessageHelp(int index, const char * label, void * data)
{
	char msgKey[STR_SIZE], *cp, *msgSrc;
	msgSrc = (char*) data;
	if (!msgSrc) {
		return;
	}
	cp = strchr(msgSrc, '\t');
	if (cp == NULL) {
		sprintf(msgKey, _("No help for %s"), msgSrc);
		wNoticeEx( NT_INFORMATION, msgKey, _("Ok"), NULL);
		return;
	}
	memcpy(msgKey, msgSrc, cp - msgSrc);
	msgKey[cp - msgSrc] = 0;
	wHelp(msgKey);
}

/*****************************************************************************
 *
 * Balloon Help
 *
 */

// defined in ${BUILDIR}/app/bin/bllnhlp.c
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

EXPORT const char * GetBalloonHelpStr(const char * helpKey)
{
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
		if ( balloonHelpCnts[cnt] == 0 ) {
			fprintf( stderr, "unused BH %s\n", balloonHelp[cnt].name );
		}
}
#endif

/*****************************************************************************
 *
 * CREATE MENUS
 *
 */


EXPORT wButton_p AddToolbarButton(const char * helpStr, wIcon_p icon,
                                  long options,
                                  wButtonCallBack_p action, void * context)
{
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

#include "bitmaps/down.xpm3"
static const char * buttonGroupMenuTitle;
static const char * buttonGroupHelpKey;
static const char * buttonGroupStickyLabel;
static wMenu_p buttonGroupPopupM;
static int stickyCnt = 0;

EXPORT void ButtonGroupBegin(const char * menuTitle, const char * helpKey,
                             const char * stickyLabel)
{
	buttonGroupMenuTitle = menuTitle;
	buttonGroupHelpKey = helpKey;
	buttonGroupStickyLabel = stickyLabel;
	buttonGroupPopupM = NULL;
}

EXPORT void ButtonGroupEnd(void)
{
	buttonGroupMenuTitle = NULL;
	buttonGroupHelpKey = NULL;
	buttonGroupPopupM = NULL;
}

EXPORT wIndex_t AddMenuButton(wMenu_p menu, procCommand_t command,
                              const char * helpKey, const char * nameStr, wIcon_p icon, int reqLevel,
                              long options, long acclKey, void * context)
{
	wIndex_t buttInx = -1;
	wIndex_t cmdInx;
	wBool_t newButtonGroup = FALSE;
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
		}
		if (buttonGroupMenuTitle != NULL && buttonGroupPopupM == NULL) {
			if (openbuttIcon == NULL) {
				openbuttIcon = wIconCreatePixMap(down_xpm3[iconSize]);
			}
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
	long stickyMask = 0;
	wMenuPush_p cmdMenus[NUM_CMDMENUS] = { NULL, NULL, NULL, NULL };
	if (nameStr[0] != '\0') {
		if (options & IC_STICKY) {
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
			stickyMask = 1L<<(stickyCnt-1);
			if ( ( options & IC_INITNOTSTICKY ) == 0 ) {
				stickySet |= stickyMask;
			}
		}
		if (buttonGroupPopupM) {
			cmdMenus[0] = wMenuPushCreate(buttonGroupPopupM,
			                              helpKey, GetBalloonHelpStr(helpKey), 0, DoCommandB,
			                              I2VP(commandCnt));
			tm = commandsSubmenu;
			p1m = popup1Submenu;
			p2m = popup2Submenu;
		} else {
			tm = menu;
			p1m = (options & IC_POPUP2) ? popup1aM : (options & IC_POPUP3) ? popup1mM :
			      popup1M;
			p2m = (options & IC_POPUP2) ? popup2aM : (options & IC_POPUP3) ? popup2mM :
			      popup2M;
		}
		cmdMenus[1] = wMenuPushCreate(tm, helpKey, nameStr, acclKey,
		                              DoCommandB, I2VP(commandCnt));
		if ((options & (IC_POPUP | IC_POPUP2 | IC_POPUP3))) {
			if (!(options & IC_SELECTED)) {
				cmdMenus[2] = wMenuPushCreate(p1m, helpKey, nameStr,
				                              0, DoCommandB, I2VP(commandCnt));
			}
			cmdMenus[3] = wMenuPushCreate(p2m, helpKey, nameStr, 0,
			                              DoCommandB, I2VP(commandCnt));
		}
	}

	cmdInx = AddCommand(command, helpKey, nameStr, icon, reqLevel, options,
	                    acclKey, buttInx, stickyMask, cmdMenus, context);
	return cmdInx;
}


static void MiscMenuItemCreate(wMenu_p m1, wMenu_p m2, const char * name,
                               const char * label, long acclKey, void * func, long option, void * context)
{
	wMenuPush_p mp;
	mp = wMenuPushCreate(m1, name, label, acclKey, ParamMenuPush,
	                     &menuPLs[menuPG.paramCnt]);
	if (m2)
		wMenuPushCreate(m2, name, label, acclKey, ParamMenuPush,
		                &menuPLs[menuPG.paramCnt]);
	menuPLs[menuPG.paramCnt].control = (wControl_p) mp;
	menuPLs[menuPG.paramCnt].type = PD_MENUITEM;
	menuPLs[menuPG.paramCnt].valueP = func;
	menuPLs[menuPG.paramCnt].nameStr = name;
	menuPLs[menuPG.paramCnt].option = option;
	menuPLs[menuPG.paramCnt].context = context;

	if (name) {
		GetBalloonHelpStr(name);
	}
	menuPG.paramCnt++;
}


#include "bitmaps/zoom-in.xpm3"
#include "bitmaps/zoom-choose.xpm3"
#include "bitmaps/zoom-out.xpm3"
#include "bitmaps/zoom-extent.xpm3"
#include "bitmaps/undo.xpm3"
#include "bitmaps/redo.xpm3"
// #include "bitmaps/partlist.xpm" // unused
#include "bitmaps/doc-export.xpm3"
#include "bitmaps/doc-export-bmap.xpm3"
#include "bitmaps/doc-export-dxf.xpm3"
#if XTRKCAD_CREATE_SVG
#include "bitmaps/doc-export-svg.xpm3"
#endif
#include "bitmaps/doc-import.xpm3"
#include "bitmaps/doc-import-mod.xpm3"
#include "bitmaps/doc-new.xpm3"
#include "bitmaps/doc-save.xpm3"
#include "bitmaps/doc-open.xpm3"
// #include "bitmaps/doc-print.xpm"
#include "bitmaps/doc-setup.xpm3"
#include "bitmaps/parameter.xpm3"
#include "bitmaps/map.xpm3"
#include "bitmaps/magnet.xpm3"

//static wMenu_p toolbarM;
static addButtonCallBack_t paramFilesCallback;

EXPORT void CreateMenus(void)
{
	wMenu_p fileM, editM, viewM, optionM, windowM, macroM, helpM, toolbarM,
	        manageM, addM, changeM, drawM;
	wMenu_p zoomM, zoomSubM;

	wMenuPush_p zoomInM, zoomOutM, zoomExtentsM;

	wPrefGetInteger("DialogItem", "pref-iconsize", (long *) &iconSize, 0);

	wSetBalloonHelp( balloonHelp );
	fileM = wMenuBarAdd(mainW, "menuFile", _("&File"));
	editM = wMenuBarAdd(mainW, "menuEdit", _("&Edit"));
	viewM = wMenuBarAdd(mainW, "menuView", _("&View"));
	addM = wMenuBarAdd(mainW, "menuAdd", _("&Add"));
	changeM = wMenuBarAdd(mainW, "menuChange", _("&Change"));
	drawM = wMenuBarAdd(mainW, "menuDraw", _("&Draw"));
	manageM = wMenuBarAdd(mainW, "menuManage", _("&Manage"));
	optionM = wMenuBarAdd(mainW, "menuOption", _("&Options"));
	macroM = wMenuBarAdd(mainW, "menuMacro", _("&Macro"));
	windowM = wMenuBarAdd(mainW, "menuWindow", _("&Window"));
	helpM = wMenuBarAdd(mainW, "menuHelp", _("&Help"));

	/*
	 * POPUP MENUS
	 */
	/* Select Commands */
	/* Select All */
	/* Select All Current */

	/* Common View Commands Menu */
	/* Zoom In/Out */
	/* Snap Grid Menu */
	/* Show/Hide Map */
	/* Show/Hide Background */

	/* Selected Commands */
	/*--------------*/
	/* DeSelect All */
	/* Select All */
	/* Select All Current */
	/*--------------*/
	/* Quick Move */
	/* Quick Rotate */
	/* Quick Align */
	/*--------------*/
	/* Move To Current Layer */
	/* Move/Rotate Cmds */
	/* Cut/Paste/Delete */
	/* Group/Un-group Selected */
	/*----------*/
	/* Thick/Thin */
	/* Bridge/Roadbed/Tunnel */
	/* Ties/NoTies */
	/*-----------*/
	/* More Commands */

	popup1M = wMenuPopupCreate(mainW, _("Context Commands"));
	popup2M = wMenuPopupCreate(mainW, _("Shift Context Commands"));
	MiscMenuItemCreate(popup1M, popup2M, "cmdUndo", _("Undo"), 0,
	                   UndoUndo, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdRedo", _("Redo"), 0,
	                   UndoRedo, 0, NULL);
	/* Zoom */
	wMenuPushCreate(popup1M, "cmdZoomIn", _("Zoom In"), 0,
	                DoZoomUp, I2VP(1));
	wMenuPushCreate(popup2M, "cmdZoomIn", _("Zoom In"), 0,
	                DoZoomUp, I2VP(1));
	wMenuPushCreate(popup1M, "cmdZoomOut", _("Zoom Out"), 0,
	                DoZoomDown, I2VP(1));
	wMenuPushCreate(popup2M, "cmdZoomOut", _("Zoom Out"), 0,
	                DoZoomDown, I2VP(1));
	wMenuPushCreate(popup1M, "cmdZoomExtents", _("Zoom Extents"), 0,
	                DoZoomExtents, I2VP(1));
	wMenuPushCreate(popup2M, "cmdZoomExtents", _("Zoom Extents"), 0,
	                DoZoomExtents, I2VP(1));
	/* Display */
	MiscMenuItemCreate(popup1M, popup2M, "cmdGridEnable", _("Enable SnapGrid"),
	                   0, SnapGridEnable, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdGridShow", _("SnapGrid Show"), 0,
	                   SnapGridShow, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdMagneticSnap",
	                   _(" Enable Magnetic Snap"), 0,
	                   MagneticSnapToggle, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdMapShow", _("Show/Hide Map"), 0,
	                   MapWindowToggleShow, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdBackgroundShow",
	                   _("Show/Hide Background"), 0,
	                   BackgroundToggleShow, 0, NULL);
	wMenuSeparatorCreate(popup1M);
	wMenuSeparatorCreate(popup2M);
	/* Copy/Paste */
	MiscMenuItemCreate(popup2M, NULL, "cmdCut", _("Cut"), 0,
	                   EditCut, 0, NULL);
	MiscMenuItemCreate(popup2M, NULL, "cmdCopy", _("Copy"), 0,
	                   EditCopy, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdPaste", _("Paste"), 0,
	                   EditPaste, 0, NULL);
	MiscMenuItemCreate(popup2M, NULL, "cmdClone", _("Clone"), 0,
	                   EditClone, 0, NULL);
	/*Select*/
	MiscMenuItemCreate(popup1M, popup2M, "cmdSelectAll", _("Select All"), 0,
	                   (wMenuCallBack_p) SetAllTrackSelect, 0, I2VP(1));
	MiscMenuItemCreate(popup1M, popup2M, "cmdSelectCurrentLayer",
	                   _("Select Current Layer"), 0,
	                   SelectCurrentLayer, 0, NULL);
	MiscMenuItemCreate(popup2M, NULL, "cmdDeselectAll", _("Deselect All"), 0,
	                   (wMenuCallBack_p) SetAllTrackSelect, 0, I2VP(FALSE));
	wMenuPushCreate(popup1M, "cmdSelectIndex", _("Select Track Index..."), 0,
	                StartIndexDialog, &SelectByIndex);
	wMenuPushCreate(popup2M, "cmdSelectIndex", _("Select Track Index..."), 0,
	                StartIndexDialog, &SelectByIndex);
	/* Modify */
	wMenuPushCreate(popup2M, "cmdMove", _("Move"), 0,
	                DoCommandBIndirect, &moveCmdInx);
	wMenuPushCreate(popup2M, "cmdRotate", _("Rotate"), 0,
	                DoCommandBIndirect, &rotateCmdInx);
	wMenuSeparatorCreate(popup1M);
	wMenuSeparatorCreate(popup2M);
	MiscMenuItemCreate(popup2M, NULL, "cmdDelete", _("Delete"), 0,
	                   (wMenuCallBack_p) SelectDelete, 0, NULL);
	wMenuSeparatorCreate(popup2M);
	popup1aM = wMenuMenuCreate(popup1M, "", _("Add..."));
	popup2aM = wMenuMenuCreate(popup2M, "", _("Add..."));
	wMenuSeparatorCreate(popup2M);
	wMenuSeparatorCreate(popup1M);
	popup1mM = wMenuMenuCreate(popup1M, "", _("More..."));
	popup2mM = wMenuMenuCreate(popup2M, "", _("More..."));

	/*
	 * FILE MENU
	 */
	MiscMenuItemCreate(fileM, NULL, "menuFile-clear", _("&New ..."), ACCL_NEW,
	                   DoClear, 0, NULL);
	wMenuPushCreate(fileM, "menuFile-load", _("&Open ..."), ACCL_OPEN,
	                ChkLoad, NULL);
	wMenuSeparatorCreate(fileM);

	wMenuPushCreate(fileM, "menuFile-save", _("&Save"), ACCL_SAVE,
	                DoSave, NULL);
	wMenuPushCreate(fileM, "menuFile-saveAs", _("Save &As ..."), ACCL_SAVEAS,
	                DoSaveAs, NULL);
	wMenuPushCreate(fileM, "menuFile-revert", _("Revert"), ACCL_REVERT,
	                ChkRevert, NULL);
	wMenuSeparatorCreate(fileM);

	cmdGroup = BG_FILE;
	AddToolbarButton("menuFile-clear", wIconCreatePixMap(doc_new_xpm3[iconSize]),
	                 IC_MODETRAIN_TOO, DoClear, NULL);
	AddToolbarButton("menuFile-load", wIconCreatePixMap(doc_open_xpm3[iconSize]),
	                 IC_MODETRAIN_TOO, ChkLoad, NULL);
	AddToolbarButton("menuFile-save", wIconCreatePixMap(doc_save_xpm3[iconSize]),
	                 IC_MODETRAIN_TOO, DoSave, NULL);

	cmdGroup = BG_PRINT;
	MiscMenuItemCreate(fileM, NULL, "printSetup", _("P&rint Setup ..."),
	                   ACCL_PRINTSETUP, (wMenuCallBack_p) wPrintSetup, 0,
	                   I2VP(0));
	InitCmdPrint(fileM);
	AddToolbarButton("menuFile-setup", wIconCreatePixMap(doc_setup_xpm3[iconSize]),
	                 IC_MODETRAIN_TOO, (wMenuCallBack_p) wPrintSetup, I2VP(0));

	wMenuSeparatorCreate(fileM);
	MiscMenuItemCreate(fileM, NULL, "cmdImport", _("&Import"), ACCL_IMPORT,
	                   DoImport, 0, I2VP(0));
	MiscMenuItemCreate(fileM, NULL, "cmdImportModule", _("Import &Module"),
	                   ACCL_IMPORT_MOD,
	                   DoImport, 0, I2VP(1));
	MiscMenuItemCreate(fileM, NULL, "cmdOutputbitmap", _("Export to &Bitmap"),
	                   ACCL_PRINTBM, OutputBitMapInit(), 0,
	                   NULL);
	MiscMenuItemCreate(fileM, NULL, "cmdExport", _("E&xport"), ACCL_EXPORT,
	                   DoExport, IC_SELECTED, NULL);
	MiscMenuItemCreate(fileM, NULL, "cmdExportDXF", _("Export D&XF"),
	                   ACCL_EXPORTDXF, DoExportDXF, IC_SELECTED,
	                   NULL);
#if XTRKCAD_CREATE_SVG
	MiscMenuItemCreate( fileM, NULL, "cmdExportSVG", _("Export S&VG"),
	                    ACCL_EXPORTSVG, DoExportSVG, IC_SELECTED, NULL);
#endif
	wMenuSeparatorCreate(fileM);

	paramFilesCallback = ParamFilesInit();
	MiscMenuItemCreate(fileM, NULL, "cmdPrmfile", _("Parameter &Files ..."),
	                   ACCL_PARAMFILES, paramFilesCallback, 0, NULL);
	MiscMenuItemCreate(fileM, NULL, "cmdFileNote", _("No&tes ..."), ACCL_NOTES,
	                   DoNote, 0, NULL);

	wMenuSeparatorCreate(fileM);
	fileList_ml = wMenuListCreate(fileM, "menuFileList", NUM_FILELIST,
	                              ChkFileList);
	wMenuSeparatorCreate(fileM);
	wMenuPushCreate(fileM, "menuFile-quit", _("E&xit"), 0,
	                DoQuit, NULL);

	InitCmdExport();

	AddToolbarButton("menuFile-parameter",
	                 wIconCreatePixMap(parameter_xpm3[iconSize]),
	                 IC_MODETRAIN_TOO, paramFilesCallback, NULL);

	cmdGroup = BG_ZOOM;
	zoomUpB = AddToolbarButton("cmdZoomIn",
	                           wIconCreatePixMap(zoom_in_xpm3[iconSize]),
	                           IC_MODETRAIN_TOO, DoZoomUp, NULL);
	zoomM = wMenuPopupCreate(mainW, "");
	AddToolbarButton("cmdZoom", wIconCreatePixMap(zoom_choose_xpm3[iconSize]),
	                 IC_MODETRAIN_TOO,
	                 (wButtonCallBack_p) wMenuPopupShow, zoomM);
	zoomDownB = AddToolbarButton("cmdZoomOut",
	                             wIconCreatePixMap(zoom_out_xpm3[iconSize]),
	                             IC_MODETRAIN_TOO, DoZoomDown, NULL);
	zoomExtentsB = AddToolbarButton("cmdZoomExtent",
	                                wIconCreatePixMap(zoom_extent_xpm3[iconSize]),
	                                IC_MODETRAIN_TOO, DoZoomExtents, NULL);

	cmdGroup = BG_UNDO;
	undoB = AddToolbarButton("cmdUndo", wIconCreatePixMap(undo_xpm3[iconSize]), 0,
	                         UndoUndo, NULL);
	redoB = AddToolbarButton("cmdRedo", wIconCreatePixMap(redo_xpm3[iconSize]), 0,
	                         UndoRedo, NULL);

	wControlActive((wControl_p) undoB, FALSE);
	wControlActive((wControl_p) redoB, FALSE);
	InitCmdUndo();

	/*
	 * EDIT MENU
	 */
	MiscMenuItemCreate(editM, NULL, "cmdUndo", _("&Undo"), ACCL_UNDO,
	                   UndoUndo, 0, NULL);
	MiscMenuItemCreate(editM, NULL, "cmdRedo", _("R&edo"), ACCL_REDO,
	                   UndoRedo, 0, NULL);
	wMenuSeparatorCreate(editM);
	MiscMenuItemCreate(editM, NULL, "cmdCut", _("Cu&t"), ACCL_CUT,
	                   EditCut, IC_SELECTED, NULL);
	MiscMenuItemCreate(editM, NULL, "cmdCopy", _("&Copy"), ACCL_COPY,
	                   EditCopy, IC_SELECTED, NULL);
	MiscMenuItemCreate(editM, NULL, "cmdPaste", _("&Paste"), ACCL_PASTE,
	                   EditPaste, 0, NULL);
	MiscMenuItemCreate(editM, NULL, "cmdClone", _("C&lone"), ACCL_CLONE,
	                   EditClone, 0, NULL);
	MiscMenuItemCreate(editM, NULL, "cmdDelete", _("De&lete"), ACCL_DELETE,
	                   (wMenuCallBack_p) SelectDelete, IC_SELECTED, NULL);
	MiscMenuItemCreate(editM, NULL, "cmdMoveToCurrentLayer",
	                   _("Move To Current Layer"), ACCL_MOVCURLAYER,
	                   MoveSelectedTracksToCurrentLayer,
	                   IC_SELECTED, NULL);
	wMenuSeparatorCreate( editM );
	menuPLs[menuPG.paramCnt].context = I2VP(1);
	MiscMenuItemCreate( editM, NULL, "cmdSelectAll", _("Select &All"),
	                    ACCL_SELECTALL, (wMenuCallBack_p)SetAllTrackSelect, 0, I2VP(TRUE) );
	MiscMenuItemCreate( editM, NULL, "cmdSelectCurrentLayer",
	                    _("Select Current Layer"), ACCL_SETCURLAYER, SelectCurrentLayer, 0, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdSelectByIndex", _("Select By Index"), 0L,
	                    StartIndexDialog, 0, &SelectByIndex );
	MiscMenuItemCreate( editM, NULL, "cmdDeselectAll", _("&Deselect All"),
	                    ACCL_DESELECTALL, (wMenuCallBack_p)SetAllTrackSelect, 0, I2VP(FALSE) );
	MiscMenuItemCreate( editM, NULL,  "cmdSelectInvert", _("&Invert Selection"), 0L,
	                    InvertTrackSelect, 0, NULL);
	MiscMenuItemCreate( editM, NULL,  "cmdSelectOrphaned",
	                    _("Select Stranded Track"), 0L, OrphanedTrackSelect, 0, NULL);
	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdTunnel", _("Tu&nnel"), ACCL_TUNNEL,
	                    SelectTunnel, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdBridge", _("B&ridge"), ACCL_BRIDGE,
	                    SelectBridge, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdRoadbed", _("&Roadbed"), ACCL_ROADBED,
	                    SelectRoadbed, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdTies", _("Ties/NoTies"), ACCL_TIES,
	                    SelectTies, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdAbove", _("Move to &Front"), ACCL_ABOVE,
	                    SelectAbove, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdBelow", _("Move to &Back"), ACCL_BELOW,
	                    SelectBelow, IC_SELECTED, NULL);

	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdWidth0", _("Thin Tracks"), ACCL_THIN,
	                    SelectTrackWidth, IC_SELECTED, I2VP(0) );
	MiscMenuItemCreate( editM, NULL, "cmdWidth2", _("Medium Tracks"), ACCL_MEDIUM,
	                    SelectTrackWidth, IC_SELECTED, I2VP(2) );
	MiscMenuItemCreate( editM, NULL, "cmdWidth3", _("Thick Tracks"), ACCL_THICK,
	                    SelectTrackWidth, IC_SELECTED, I2VP(3) );

	/*
	 * VIEW MENU
	 */

	zoomInM = wMenuPushCreate(viewM, "menuEdit-zoomIn", _("Zoom &In"),
	                          ACCL_ZOOMIN, DoZoomUp, I2VP(1));
	zoomSubM = wMenuMenuCreate(viewM, "menuEdit-zoomTo", _("&Zoom"));
	zoomOutM = wMenuPushCreate(viewM, "menuEdit-zoomOut", _("Zoom &Out"),
	                           ACCL_ZOOMOUT, DoZoomDown, I2VP(1));
	zoomExtentsM = wMenuPushCreate(viewM, "menuEdit-zoomExtents",
	                               _("Zoom &Extents"),
	                               0, DoZoomExtents, I2VP(0));
	wMenuSeparatorCreate(viewM);

	InitCmdZoom(zoomM, zoomSubM, NULL, NULL);

	/* these menu choices and toolbar buttons are synonymous and should be treated as such */
	wControlLinkedSet((wControl_p) zoomInM, (wControl_p) zoomUpB);
	wControlLinkedSet((wControl_p) zoomOutM, (wControl_p) zoomDownB);
	wControlLinkedSet((wControl_p) zoomExtentsM, (wControl_p) zoomExtentsB);

	wMenuPushCreate(viewM, "menuEdit-redraw", _("&Redraw"), ACCL_REDRAW,
	                (wMenuCallBack_p) MainRedraw, NULL);
	wMenuPushCreate(viewM, "menuEdit-redraw", _("Redraw All"), ACCL_REDRAWALL,
	                (wMenuCallBack_p) DoRedraw, NULL);
	wMenuSeparatorCreate(viewM);

	snapGridEnableMI = wMenuToggleCreate(viewM, "cmdGridEnable",
	                                     _("Enable SnapGrid"), ACCL_SNAPENABLE, 0,
	                                     SnapGridEnable, NULL);
	snapGridShowMI = wMenuToggleCreate(viewM, "cmdGridShow", _("Show SnapGrid"),
	                                   ACCL_SNAPSHOW,
	                                   FALSE, SnapGridShow, NULL);
	InitGrid(viewM);

	// visibility toggle for anchors
	// get the start value
	long anchors_long;
	wPrefGetInteger("misc", "anchors", &anchors_long, 1);
	magneticSnap = anchors_long ? TRUE : FALSE;
	magnetsMI = wMenuToggleCreate(viewM, "cmdMagneticSnap",
	                              _("Enable Magnetic Snap"),
	                              0, magneticSnap,
	                              MagneticSnapToggle, NULL);

	// visibility toggle for map window
	// get the start value
	long mapVisible_long;
	wPrefGetInteger("misc", "mapVisible", (long *) &mapVisible_long, 1);
	mapVisible = mapVisible_long ? TRUE : FALSE;
	mapShowMI = wMenuToggleCreate(viewM, "cmdMapShow", _("Show/Hide Map"),
	                              ACCL_MAPSHOW, mapVisible,
	                              MapWindowToggleShow, NULL);

	wMenuSeparatorCreate(viewM);

	toolbarM = wMenuMenuCreate(viewM, "toolbarM", _("&Tool Bar"));
	CreateToolbarM(toolbarM);

	cmdGroup = BG_EASE;
	InitCmdEasement();

	cmdGroup = BG_SNAP;
	InitSnapGridButtons();
	magnetsB = AddToolbarButton("cmdMagneticSnap",
	                            wIconCreatePixMap(magnet_xpm3[iconSize]),
	                            IC_MODETRAIN_TOO, MagneticSnapToggle, NULL);
	wControlLinkedSet((wControl_p) magnetsMI, (wControl_p) magnetsB);
	wButtonSetBusy(magnetsB, (wBool_t) magneticSnap);

	mapShowB = AddToolbarButton("cmdMapShow", wIconCreatePixMap(map_xpm3[iconSize]),
	                            IC_MODETRAIN_TOO, MapWindowToggleShow, NULL);
	wControlLinkedSet((wControl_p) mapShowMI, (wControl_p) mapShowB);
	wButtonSetBusy(mapShowB, (wBool_t) mapVisible);

	/*
	 * ADD MENU
	 */

	cmdGroup = BG_TRKCRT | BG_BIGGAP;
	InitCmdStraight(addM);
	InitCmdCurve(addM);
	InitCmdParallel(addM);
	InitCmdTurnout(addM);
	InitCmdHandLaidTurnout(addM);
	InitCmdStruct(addM);
	InitCmdHelix(addM);
	InitCmdTurntable(addM);

	cmdGroup = BG_CONTROL;
	ButtonGroupBegin( _("Control Element"), "cmdControlElements",
	                  _("Control Element") );
	InitCmdBlock(addM);
	InitCmdSwitchMotor(addM);
	InitCmdSignal(addM);
	InitCmdControl(addM);
	InitCmdSensor(addM);
	ButtonGroupEnd();

	/*
	 * CHANGE MENU
	 */
	cmdGroup = BG_SELECT;
	InitCmdDescribe(changeM);
	InitCmdSelect(changeM);
	InitCmdPan(viewM);

	wMenuSeparatorCreate(changeM);

	cmdGroup = BG_TRKGRP;
	InitCmdMove(changeM);
	InitCmdMoveDescription(changeM);
	InitCmdDelete();
	InitCmdTunnel();
	InitCmdTies();
	InitCmdBridge();
	InitCmdRoadbed();
	InitCmdAboveBelow();

	cmdGroup = BG_TRKMOD;
	InitCmdModify(changeM);
	InitCmdCornu(changeM);

	MiscMenuItemCreate(changeM, NULL, "cmdRescale", _("Change Scale"), 0,
	                   DoRescale, IC_SELECTED, NULL);


	wMenuSeparatorCreate(changeM);

	InitCmdJoin(changeM);
	InitCmdSplit(changeM);

	wMenuSeparatorCreate(changeM);

	InitCmdPull(changeM);

	wMenuSeparatorCreate(changeM);

	MiscMenuItemCreate(changeM, NULL, "cmdAddElevations",
	                   _("Raise/Lower Elevations"), ACCL_CHGELEV,
	                   ShowAddElevations, IC_SELECTED,
	                   NULL);
	InitCmdElevation(changeM);
	InitCmdProfile(changeM);

	MiscMenuItemCreate(changeM, NULL, "cmdClearElevations",
	                   _("Clear Elevations"), ACCL_CLRELEV,
	                   ClearElevations, IC_SELECTED, NULL);
	MiscMenuItemCreate(changeM, NULL, "cmdElevation", _("Recompute Elevations"),
	                   0, RecomputeElevations, 0, NULL);
	ParamRegister(&addElevPG);

	/*
	 * DRAW MENU
	 */
	cmdGroup = BG_MISCCRT;
	InitCmdDraw(drawM);
	InitCmdText(drawM);
	InitTrkNote(drawM);

	cmdGroup = BG_RULER;
	InitCmdRuler(drawM);

	/*
	 * OPTION MENU
	 */
	MiscMenuItemCreate(optionM, NULL, "cmdLayout", _("L&ayout ..."),
	                   ACCL_LAYOUTW, LayoutInit(), IC_MODETRAIN_TOO, NULL);
	MiscMenuItemCreate(optionM, NULL, "cmdDisplay", _("&Display ..."),
	                   ACCL_DISPLAYW, DisplayInit(), IC_MODETRAIN_TOO, NULL);
	MiscMenuItemCreate(optionM, NULL, "cmdCmdopt", _("Co&mmand ..."),
	                   ACCL_CMDOPTW, CmdoptInit(), IC_MODETRAIN_TOO, NULL);
	MiscMenuItemCreate(optionM, NULL, "cmdEasement", _("&Easements ..."),
	                   ACCL_EASEW, DoEasementRedir,
	                   IC_MODETRAIN_TOO, NULL);
	MiscMenuItemCreate(optionM, NULL, "fontSelW", _("&Fonts ..."), ACCL_FONTW,
	                   SelectFont, IC_MODETRAIN_TOO, NULL);
	MiscMenuItemCreate(optionM, NULL, "cmdSticky", _("Stic&ky ..."),
	                   ACCL_STICKY, DoSticky, IC_MODETRAIN_TOO,
	                   NULL);
	if (extraButtons) {
		menuPLs[menuPG.paramCnt].context = debugW;
		MiscMenuItemCreate(optionM, NULL, "cmdDebug", _("&Debug ..."), 0,
		                   DebugInit, IC_MODETRAIN_TOO, NULL);
	}
	MiscMenuItemCreate(optionM, NULL, "cmdPref", _("&Preferences ..."),
	                   ACCL_PREFERENCES, PrefInit(), IC_MODETRAIN_TOO, NULL);
	MiscMenuItemCreate(optionM, NULL, "cmdColor", _("&Colors ..."), ACCL_COLORW,
	                   ColorInit(), IC_MODETRAIN_TOO, NULL);

	/*
	 * MACRO MENU
	 */
	wMenuPushCreate(macroM, "cmdRecord", _("&Record ..."), ACCL_RECORD,
	                DoRecord, NULL);
	wMenuPushCreate(macroM, "cmdDemo", _("&Play Back ..."), ACCL_PLAYBACK,
	                DoPlayBack, NULL);

	/*
	 * WINDOW MENU
	 */
	wMenuPushCreate(windowM, "menuWindow", _("Main window"), 0,
	                (wMenuCallBack_p) wShow, mainW);
	winList_mi = wMenuListCreate(windowM, "menuWindow", -1, DoShowWindow);

	/*
	 * HELP MENU
	 */

	/* main help window */
	wMenuAddHelp(helpM);

	/* help on recent messages */
	wMenuSeparatorCreate(helpM);
	wMenu_p messageListM = wMenuMenuCreate(helpM, "menuHelpRecentMessages",
	                                       _("Recent Messages"));
	messageList_ml = wMenuListCreate(messageListM, "messageListM", 10,
	                                 ShowMessageHelp);
	wMenuListAdd(messageList_ml, 0, _(MESSAGE_LIST_EMPTY), NULL);
	wMenuPushCreate(helpM, "menuHelpProblemrep", _("Collect Problem Info"), 0,
		DoProblemCollect, NULL);

	/* tip of the day */
	wMenuSeparatorCreate( helpM );
	wMenuPushCreate( helpM, "cmdTip", _("Tip of the Day..."), 0, ShowTip,
	                 I2VP(SHOWTIP_FORCESHOW | SHOWTIP_NEXTTIP));
	demoM = wMenuMenuCreate( helpM, "cmdDemo", _("&Demos") );
	wMenuPushCreate( helpM, "cmdExamples", _("Examples..."), 0, ChkExamples, NULL);

	/* about window */
	wMenuSeparatorCreate(helpM);
	wMenuPushCreate(helpM, "about", _("About"), 0,
	                CreateAboutW, NULL);

	/*
	 * MANAGE MENU
	 */

	cmdGroup = BG_TRAIN | BG_BIGGAP;
	InitCmdTrain(manageM);
	wMenuSeparatorCreate(manageM);

	InitNewTurn(
	        wMenuMenuCreate(manageM, "cmdTurnoutNew",
	                        _("Tur&nout Designer...")));

	MiscMenuItemCreate(manageM, NULL, "cmdContmgm",
	                   _("Layout &Control Elements"), ACCL_CONTMGM,
	                   ControlMgrInit(), 0, NULL);
	MiscMenuItemCreate(manageM, NULL, "cmdGroup", _("&Group"), ACCL_GROUP,
	                   DoGroup, IC_SELECTED, NULL);
	MiscMenuItemCreate(manageM, NULL, "cmdUngroup", _("&Ungroup"), ACCL_UNGROUP,
	                   DoUngroup, IC_SELECTED, NULL);

	MiscMenuItemCreate(manageM, NULL, "cmdCustmgm",
	                   _("Custom defined parts..."), ACCL_CUSTMGM, CustomMgrInit(),
	                   0, NULL);
	MiscMenuItemCreate(manageM, NULL, "cmdRefreshCompound",
	                   _("Update Turnouts and Structures"), 0,
	                   DoRefreshCompound, 0, NULL);

	MiscMenuItemCreate(manageM, NULL, "cmdCarInventory", _("Car Inventory"),
	                   ACCL_CARINV, DoCarDlg, IC_MODETRAIN_TOO,
	                   NULL);

	wMenuSeparatorCreate(manageM);

	MiscMenuItemCreate(manageM, NULL, "cmdLayer", _("Layers ..."), ACCL_LAYERS,
	                   InitLayersDialog(), 0, NULL);
	wMenuSeparatorCreate(manageM);

	MiscMenuItemCreate(manageM, NULL, "cmdEnumerate", _("Parts &List ..."),
	                   ACCL_PARTSLIST, EnumerateTracks, 0,
	                   NULL);
	MiscMenuItemCreate(manageM, NULL, "cmdPricelist", _("Price List..."),
	                   ACCL_PRICELIST, PriceListInit(), 0, NULL);

	cmdGroup = BG_LAYER | BG_BIGGAP;

	InitCmdSelect2(changeM);
	InitCmdDescribe2(changeM);
	InitCmdPan2(changeM);

	InitLayers();

	cmdGroup = BG_HOTBAR;
	InitHotBar();

	InitBenchDialog();

	ParamRegister(&rotatePG);
	ParamRegister(&movePG);
	ParamRegister(&indexPG);

	// stickySet is initialized by AddMenuButton based on IC_STICKY flag
	// Now check to see if there is saved value
	wPrefGetInteger( "DialogItem", "sticky-set", &stickySet, stickySet );
	ParamRegister(&stickyPG);
}


static void InitCmdExport(void)
{
	ButtonGroupBegin( _("Import/Export"), "cmdExportImportSetCmd",
	                  _("Import/Export") );
	cmdGroup = BG_EXPORTIMPORT;
	AddToolbarButton("cmdExport", wIconCreatePixMap(doc_export_xpm3[iconSize]),
	                 IC_SELECTED | IC_ACCLKEY, DoExport, NULL);
	AddToolbarButton("cmdExportDXF",
	                 wIconCreatePixMap(doc_export_dxf_xpm3[iconSize]),
	                 IC_SELECTED | IC_ACCLKEY, DoExportDXF, I2VP(1));
	AddToolbarButton("cmdExportBmap",
	                 wIconCreatePixMap(doc_export_bmap_xpm3[iconSize]), IC_ACCLKEY,
	                 OutputBitMapInit(), NULL);
#if XTRKCAD_CREATE_SVG
	AddToolbarButton("cmdExportSVG",
	                 wIconCreatePixMap(doc_export_svg_xpm3[iconSize]),
	                 IC_ACCLKEY, DoExportSVG, NULL); // IC_SELECTED |
#endif
	AddToolbarButton("cmdImport", wIconCreatePixMap(doc_import_xpm3[iconSize]),
	                 IC_ACCLKEY,
	                 DoImport, I2VP(0));
	AddToolbarButton("cmdImportModule",
	                 wIconCreatePixMap(doc_import_mod_xpm3[iconSize]), IC_ACCLKEY,
	                 DoImport, I2VP(1));
	ButtonGroupEnd();
	ParamRegister( &menuPG );
	AddPlaybackProc( "MENU", MenuPlayback, NULL );
}
