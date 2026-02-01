/* file menu.c
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
#include "form.h"
#include "icons.h"
#include "layout.h"
#include "mapwindow.h"
#include "paths.h"
#include "problemrep.h"
#include "smalldlg.h"
#include "common-ui.h"
#include "ctrain.h"

#include "toolbar.h"

static paramGroup_t menuPG;
static paramData_t menuPLs[101] = {
	{PD_LONG, NULL, "toolbarset", PDO_NOPREF, .group = &menuPG },
	{PD_LONG, &curTurnoutEp, "cur-turnout-ep", .group = &menuPG}
};
static paramGroup_t menuPG = { "misc", PGO_RECORD, menuPLs, 2 };

static void InitCmdExport( void );

EXPORT wMenu_p demoM;
EXPORT wMenu_p popup1M, popup2M;
static wMenu_p popup1aM, popup2aM;
static wMenu_p popup1mM, popup2mM;
wControl_p undoB;
wControl_p redoB;
wControl_p zoomUpB;
wControl_p zoomDownB;
wControl_p zoomExtentsB;
wControl_p mapShowB;
static wControl_p magnetsB;
EXPORT wControl_p mapShowMI;
static wMenuToggle_p magnetsMI;
EXPORT wControl_p winList_mi;
wControl_p fileList_ml;
EXPORT wMenuToggle_p snapGridEnableMI;
EXPORT wMenuToggle_p snapGridShowMI;

static int cmdGroup;
static void HandleCmdGroupChange(int currentGroup);
static wMenu_p buttonGroupPopupM = NULL;
/*--------------------------------------------------------------------*/
typedef struct {
	char * label;
	wMenu_p menu;
} menuTrace_t, *menuTrace_p;
static dynArr_t menuTrace_da;
#define menuTrace(N) DYNARR_N( menuTrace_t, menuTrace_da, N )

static void DoMenuTrace(wMenu_p menu, const char * label, const void * data)
{
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
	wMenuSetTraceCallBack(m, DoMenuTrace, strdup(label));
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
 * ELEVATION HELPERS
 *
 */


static wControl_p addElevW;
#define addElevF (wFloat_p)addElevPD.control
static DIST_T addElevValueV;
static void DoAddElev(void * unused);

static paramFloatRange_t rn1000_1000 = { -1000.0, 1000.0 };
static paramData_t addElevPLs[] = { {
		PD_FLOAT, &addElevValueV, "value",
		PDO_NOPREF|PDO_DIM, &rn1000_1000,
	}
};
static paramGroup_t addElevPG = { "addElev", 0, addElevPLs, COUNT( addElevPLs ) };

static void DoAddElev(void * unused)
{
	FormFetchData(&addElevPG);
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
		addElevW = FormCreateDialog(&addElevPG,
		                            NULL,
		                            NULL, DoAddElev,
		                            NULL, FormCancel_Current, FALSE, 0, NULL);
	wShow(addElevW);
}

/*****************************************************************************
 *
 * MOVE/ROTATE/INDEX HELPERS
 *
 */


static wControl_p rotateW;
static wControl_p indexW;
static wControl_p moveW;
static double rotateValue;
static char trackIndex[STR_LONG_SIZE];
static coOrd moveValue;
static rotateDialogCallBack_t rotateDialogCallBack;
static indexDialogCallBack_t indexDialogCallBack;
static moveDialogCallBack_t moveDialogCallBack;

static void RotateEnterOk(void * unused);

static paramFloatRange_t rn360_360 = { -360.0, 360.0, 20 };
static paramGroup_t rotatePG;
static paramData_t rotatePLs[] = { { PD_FLOAT, &rotateValue, "rotate", PDO_NOPREF|PDO_ANGLE|PDO_NORECORD, &rn360_360, N_("Angle:"), .group = &rotatePG } };
static paramGroup_t rotatePG = { "rotate", 0, rotatePLs, COUNT( rotatePLs ) };

static void IndexEnterOk(void * unused);

static paramGroup_t indexPG;
static paramData_t indexPLs[] = {
	{ PD_STRING, &trackIndex, "select",	PDO_NOPREF|PDO_NORECORD|PDO_STRINGLIMITLENGTH, I2VP(20), N_("Indexes:"), 0, 0, sizeof(trackIndex), .group=&indexPG }
};
static paramGroup_t indexPG = { "index", 0, indexPLs, COUNT( indexPLs ) };

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 20 };
static void MoveEnterOk(void * unused);

static paramGroup_t movePG;
static paramData_t movePLs[] = {
	{ PD_FLOAT, &moveValue.x, "moveX", PDO_NOPREF|PDO_DIM|PDO_NORECORD, &r_1000_1000, N_("Move X:"), .group=&movePG},
	{ PD_FLOAT, &moveValue.y, "moveY", PDO_NOPREF|PDO_DIM|PDO_NORECORD, &r_1000_1000, N_("Move Y:"), .group=&movePG}
};
static paramGroup_t movePG = { "move", 0, movePLs, COUNT( movePLs ) };

static void StartRotateDialog(void * funcVP)
{
	rotateDialogCallBack_t func = funcVP;
	if (rotateW == NULL)
		rotateW = FormCreateDialog(&rotatePG, MakeWindowTitle(_("Rotate")),
		                           _("Ok"), RotateEnterOk,
		                           _("Cancel"), FormCancel_Current, FALSE, 0, NULL);
	FormLoadControls(&rotatePG);
	rotateDialogCallBack = func;
	wShow(rotateW);
}

static void StartIndexDialog(void * funcVP)
{
	indexDialogCallBack_t func = funcVP;
	if (indexW == NULL)
		indexW = FormCreateDialog(&indexPG, MakeWindowTitle(_("Select Index")),
		                          _("Ok"), IndexEnterOk,
		                          _("Cancel"), FormCancel_Current, FALSE, 0, NULL);
	FormLoadControls(&indexPG);
	indexDialogCallBack = func;
	trackIndex[0] = '\0';
	wShow(indexW);
}

static void StartMoveDialog(void * funcVP)
{
	moveDialogCallBack_t func = funcVP;
	if (moveW == NULL)
		moveW = FormCreateDialog(&movePG, MakeWindowTitle(_("Move")),
		                         _("Ok"), MoveEnterOk,
		                         _("Cancel"), FormCancel_Current,
		                         FALSE, 0, NULL);

	FormLoadControls(&movePG);
	moveDialogCallBack = func;
	wShow(moveW);
}

static void MoveEnterOk(void * unused)
{
	FormFetchData(&movePG);
	if ( moveValue.x != 0.0 ||
	     moveValue.y != 0.0 ) {
		moveDialogCallBack(&moveValue);
	}
	wHide(moveW);
}

static void IndexEnterOk(void * unused)
{
	FormFetchData(&indexPG);
	indexDialogCallBack(trackIndex);
	wHide(indexW);
}

static void RotateEnterOk(void * unused)
{
	FormFetchData(&rotatePG);
	if ( rotateValue != 0.0 ) {
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
	if (changed) {
		int rc;
		rc = wNoticeWithIcon(NT_WARNING,
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

void ToggleSetInMenuToolbar(wControl_p menuItem, wControl_p toolbarButton,
                            wBool_t newState)
{
	wMenuToggleSet(menuItem, newState);

	if (toolbarButton) {
		wButtonSetBusy(toolbarButton, newState);
	}
}

/**
 * Set magnets state
 */
EXPORT int MagneticSnap(int state)
{
	int oldState = magneticSnap;
	magneticSnap = state;
	wPrefSetInteger("misc", "magnets", magneticSnap);

	ToggleSetInMenuToolbar(magnetsMI, magnetsB, magneticSnap);

	return oldState;
}

/**
 * Toggle magnets on/off. Additional flag to avoid recursive calls while
 * UI elements in toolbar and menu are sync'ed
 */
static void MagneticSnapToggle(void * unused)
{
	static int inTransition = FALSE;
	if (!inTransition) {
		inTransition = TRUE;
		MagneticSnap(!magneticSnap);
		inTransition = FALSE;
	}
}


EXPORT void SelectFont(void * unused)
{
	wSelectFont(_("XTrackCAD Font"));
}

#define MAX_STICKY_GROUPS 32

EXPORT long stickySet = 0;
static wControl_p stickyW;
static const char * stickyLabels[MAX_STICKY_GROUPS + 1];
static paramData_t stickyPLs[] = { {
		PD_TOGGLE, &stickySet, "set", PDO_NOPSHUPD,
		stickyLabels, "", 0
	}
};
static paramGroup_t stickyPG = { "sticky", PGO_RECORD, stickyPLs,COUNT( stickyPLs )};

static void StickyOk(void * unused)
{
	long changes = GetChanges( &stickyPG );
	wHide(stickyW);
	DoChangeNotification(changes);
}


static void StickyChange( long changes )
{
}

EXPORT void DoSticky(void * unused)
{
	if (!stickyW) {
		FormRegister(&stickyPG);

		stickyW = FormCreateDialog(&stickyPG, MakeWindowTitle(_("Sticky Commands")),
		                           _("Ok"), StickyOk,
		                           _("Cancel"), FormCancel_Restore,
		                           TRUE, F_RESIZE, NULL);
	}
	FormLoadControls(&stickyPG);
	wShow(stickyW);
}

/*****************************************************************************
 *
 * DEBUG DIALOG
 *
 */

static wControl_p debugW;

static int debugCnt = 0;
static paramIntegerRange_t r0_100 = { 0, 100, 80 };
static void DebugOk(void * unused);
static paramData_t debugPLs[30];
static paramData_t p0[] = {
	{ PD_BUTTON, TestMallocs, "test", PDO_DLGHORZ, NULL, N_("Test Mallocs") }
};

#define MAX_ACTIVE_LOGS	30
static long debug_values[MAX_ACTIVE_LOGS];
static int debug_index[MAX_ACTIVE_LOGS];

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
	debugW = FormCreateDialog(&debugPG, MakeWindowTitle(_("Debug")),
	                          _("Ok"),DebugOk,
	                          _("Cancel"), FormCancel_Current, FALSE, 0, NULL);
	wHide(debugW);
}

static void InitDebug(const char * label, long * valueP)
{
	CHECK( debugCnt+1 < COUNT( debugPLs ) );
	memset(&debugPLs[debugCnt+1], 0, sizeof debugPLs[debugCnt]);
	debugPLs[debugCnt+1].type = PD_LONG;
	debugPLs[debugCnt+1].valueP = valueP;
	debugPLs[debugCnt+1].nameStr = CAST_AWAY_CONST label;
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
		for ( int inx=0; inx<logTable_da.cnt && inx < MAX_ACTIVE_LOGS; inx++ ) {
			if (logTable(inx).name[0] ) {
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
		wMenuPushEnable(menuPG.paramPtr[inx].control, enable);
	}
}
/*****************************************************************************
 *
 * RECENT MESSAGES LIST
 *
 */

static wControl_p messageList_ml;
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
		snprintf(msgKey, STR_SIZE, _("No help for %s"), msgSrc);
		wNoticeWithIcon( NT_INFORMATION, msgKey, _("Ok"), NULL);
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
extern wTooltip_t tooltipTexts[];

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
	wTooltip_t * bh;
#ifdef CHECK_UNUSED_BALLOONHELP
	if ( balloonHelpCnts == NULL ) {
		for ( bh=tooltipTexts; bh->name; bh++ );
		balloonHelpCnts = (int*)malloc( (sizeof *(int*)0) * (bh-tooltipTexts) );
		memset( balloonHelpCnts, 0, (sizeof *(int*)0) * (bh-tooltipTexts) );
	}
#endif
	for (bh = tooltipTexts; bh->name; bh++) {
		if (strcmp(bh->name, helpKey) == 0) {
#ifdef CHECK_UNUSED_BALLOONHELP
			balloonHelpCnts[(bh-tooltipTexts)]++;
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
	for ( cnt=0; tooltipTexts[cnt].name; cnt++ )
		if ( balloonHelpCnts[cnt] == 0 ) {
			fprintf( stderr, "unused BH %s\n", tooltipTexts[cnt].name );
		}
}
#endif

/*****************************************************************************
 *
 * CREATE MENUS
 *
 */

wControl_p AddToolbarButton(const char* helpStr, wIcon_p icon, long options,
                            wButtonCallBack_p action, void* context)
{
	wControl_p bb;
	wIndex_t inx;

	if (context == NULL) {
		for (inx = 0; inx < menuPG.paramCnt; inx++) {
			if (action != DoCommandB && menuPG.paramPtr[inx].valueP == I2VP(action)) {
				// button is used to trigger a menu entry
				context = &menuPG.paramPtr[inx];
				action = FormMenuPush;
				menuPG.paramPtr[inx].context = I2VP(buttonCnt);
				menuPG.paramPtr[inx].option |= IC_PLAYBACK_PUSH;
				break;
			}
		}
	}
	long opt = 0L;
	if (options & IC_ABUT) {
		opt = BO_ABUT;
	}

	HandleCmdGroupChange(cmdGroup);

	if (options & IC_TOGGLE) {
		bb = wToggleCreateForToolbar(mainW, 0, 0, helpStr, icon,
		                             opt | BO_ICON, 0, action, context);
	} else if (options & IC_STICKY) {
		bb = wStickyCreateForToolbar(mainW, 0, 0, helpStr, icon,
		                             opt | BO_ICON, 0, action, context);
	} else {
		bb = wButtonCreateForToolbar(mainW, 0, 0, helpStr, icon,
		                             opt | BO_ICON, 0, action, context);
	}

	ToolbarControlAdd(bb, options, cmdGroup);

	return bb;
}

/*****************************************************************************
 *
 * CONSTANTS AND ENUMS
 *
 */



// Maximum number of sticky button groups
#ifndef MAX_STICKY_GROUPS
#define MAX_STICKY_GROUPS 32
#endif

/*****************************************************************************
 *
 * STATIC VARIABLES
 *
 */

// Button group state - managed by ButtonGroupBegin/End
static const char *buttonGroupMenuTitle = NULL;
static const char *buttonGroupHelpKey = NULL;
static const char *buttonGroupStickyLabel = NULL;


// Sticky button tracking
static int stickyCnt = 0;

// Static menu state for button groups
static wMenu_p commandsSubmenu = NULL;
static wMenu_p popup1Submenu = NULL;
static wMenu_p popup2Submenu = NULL;

// Track previous cmdGroup to detect changes
static int prevCmdGroup = -1;
static wControl_p currentSplitButton = NULL;  // Track active split button

/*****************************************************************************
 *
 * HELPER FUNCTIONS
 *
 */

/**
 * Get the appropriate popup menus based on option flags.
 *
 * \param options Option flags (IC_POPUP2, IC_POPUP3, etc.)
 * \param p1m OUT First popup menu
 * \param p2m OUT Second popup menu
 */
static void GetPopupMenus(long options, wMenu_p *p1m, wMenu_p *p2m)
{
	if (!p1m || !p2m) {
		return;
	}

	if (options & IC_POPUP2) {
		*p1m = popup1aM;
		*p2m = popup2aM;
	} else if (options & IC_POPUP3) {
		*p1m = popup1mM;
		*p2m = popup2mM;
	} else {
		*p1m = popup1M;
		*p2m = popup2M;
	}
}

/**
 * Create submenus for a button group.
 *
 * \param menu Main menu to add submenu to
 * \param options Option flags
 * \return TRUE on success, FALSE on failure
 */
static wBool_t CreateButtonGroupSubmenus(wMenu_p menu, long options)
{
	if (!menu || !buttonGroupMenuTitle) {
		return FALSE;
	}

	// Create main commands submenu
	commandsSubmenu = wMenuMenuCreate(menu, "", buttonGroupMenuTitle);
	if (!commandsSubmenu) {
		fprintf(stderr, "ERROR: Failed to create commands submenu\n");
		return FALSE;
	}

	// Determine which popup menus to use
	wMenu_p popup1Parent, popup2Parent;

	if (options & IC_POPUP2) {
		popup1Parent = popup1aM;
		popup2Parent = popup2aM;
	} else if (options & IC_POPUP3) {
		popup1Parent = popup1mM;
		popup2Parent = popup2mM;
	} else {
		popup1Parent = popup1M;
		popup2Parent = popup2M;
	}

	// Create popup submenus
	popup1Submenu = wMenuMenuCreate(popup1Parent, "", buttonGroupMenuTitle);
	popup2Submenu = wMenuMenuCreate(popup2Parent, "", buttonGroupMenuTitle);

	if (!popup1Submenu || !popup2Submenu) {
		fprintf(stderr, "ERROR: Failed to create popup submenus\n");
		return FALSE;
	}

	return TRUE;
}

/**
 * Setup sticky button behavior and update sticky labels.
 *
 * \param options Option flags
 * \param newButtonGroup TRUE if this is a new button group
 * \param nameStr Button name
 * \param stickyIndexOut OUT Index of sticky group (if created)
 * \return Sticky mask for this button, or 0 if not sticky
 */
static long SetupStickyBehavior(long options, wBool_t newButtonGroup,
                                const char *nameStr, int *stickyIndexOut)
{
	// Not a sticky button
	if (!(options & IC_STICKY)) {
		return 0;
	}

	int stickyIndex;

	// Check if we need to start a new sticky group
	if (buttonGroupPopupM == NULL || newButtonGroup) {
		// Validate we haven't exceeded maximum groups
		if (stickyCnt >= MAX_STICKY_GROUPS) {
			fprintf(stderr, "ERROR: Exceeded maximum sticky groups (%d)\n",
			        MAX_STICKY_GROUPS);
			return 0;
		}

		// Allocate new sticky group
		stickyIndex = stickyCnt;
		stickyCnt++;
	} else {
		// Use existing sticky group
		stickyIndex = stickyCnt - 1;
	}

	// Set the label for this sticky group
	// Use group label if in a button group, otherwise use button name
	if (buttonGroupPopupM != NULL && buttonGroupStickyLabel != NULL) {
		stickyLabels[stickyIndex] = buttonGroupStickyLabel;
	} else if (nameStr) {
		stickyLabels[stickyIndex] = nameStr;
	} else {
		stickyLabels[stickyIndex] = "Unknown";
	}

	// Calculate sticky mask
	long stickyMask = 1L << stickyIndex;

	// Set initial sticky state (default is sticky unless IC_INITNOTSTICKY)
	if ((options & IC_INITNOTSTICKY) == 0) {
		stickySet |= stickyMask;
	}

	if (stickyIndexOut) {
		*stickyIndexOut = stickyIndex;
	}

	return stickyMask;
}


/**
 * Check if a dropdown menu will be needed for this button.
 * This is used to determine if a split button should be created.
 *
 * \return TRUE if this button will trigger creation of a dropdown menu
 */

static wBool_t WillNeedDropdown(void)
{
	// Dropdown is needed if we're about to start a new button group
	return (buttonGroupMenuTitle != NULL && buttonGroupPopupM == NULL);
}

/**
 * Add a gap after the previous command group using a separator.
 * The gap size depends on whether BG_BIGGAP was set in the previous cmdGroup.
 *
 * \param prevGroup Previous command group flags
 */
static void AddGapAfterGroup(int prevGroup)
{
	if (prevGroup < 0) {
		return;  // No previous group
	}

	// Determine gap width based on previous group's flags
	int width = (prevGroup & BG_BIGGAP) ? 16 : 8;

	// Create invisible separator
	wControl_p separator = wSeparatorCreateForToolbar(mainW, width);

	if (separator) {
		long option = (prevGroup & BG_BIGGAP) ? BO_BIGGAP : BO_GAP;
		ToolbarControlAdd(separator, option, prevGroup);
	}
}

/**
 * Check if cmdGroup has changed and add gap if needed.
 * Should be called at the start of AddMenuButton.
 *
 * \param currentGroup Current cmdGroup value
 */
static void HandleCmdGroupChange(int currentGroup)
{
	// Check if this is a new command group
	if (prevCmdGroup >= 0 && prevCmdGroup != currentGroup) {
		// cmdGroup changed - add gap after previous group
		AddGapAfterGroup(prevCmdGroup);
	}

	// Update tracked cmdGroup
	prevCmdGroup = currentGroup;
}
/*****************************************************************************
 *
 * MAIN FUNCTION
 *
 */

/**
 * Add a menu button with toolbar button and menu entries.
 */
EXPORT wIndex_t AddMenuButton(wMenu_p menu, procCommand_t command,
                              const char *helpKey, const char *nameStr,
                              wIcon_p icon, int reqLevel, long options,
                              long acclKey, void *context)
{
	if (!menu || !helpKey || !nameStr) {
		return -1;
	}

	// Handle command group changes
	HandleCmdGroupChange(cmdGroup);

	wIndex_t buttInx = -1;
	wBool_t newButtonGroup = FALSE;
	wBool_t useSplitButton;
	long stickyMask = 0;
	wControl_p cmdMenus[NUM_CMDMENUS] = {NULL, NULL, NULL, NULL};

	// ========================================================================
	// Toolbar Button Creation
	// ========================================================================

	if (icon != NULL) {
		if (buttonGroupPopupM != NULL) {
			// Already in button group
			if (currentSplitButton != NULL) {
				// Split button already exists - keep its current icon (the first one)
				// Don't update icon or context during group building
				buttInx = buttonCnt - 1;
			} else {
				// Regular button group
				if (buttonCnt < 2) {
					return -1;
				}
				buttInx = buttonCnt - 2;
			}
		} else {
			// Not in button group yet
			useSplitButton = WillNeedDropdown();

			if (useSplitButton) {
				// Create popup menu first
				buttonGroupPopupM = wMenuPopupCreate(mainW, buttonGroupMenuTitle);
				if (!buttonGroupPopupM) {
					return -1;
				}

				// Create button using existing function
				buttInx = buttonCnt;
				wControl_p button = AddToolbarButton(helpKey, icon, options,
				                                     DoCommandB, I2VP(commandCnt));

				if (button) {
					// Wrap it as a split button (modifies button in place)
					wButtonMakeSplit(button, buttonGroupPopupM);
					currentSplitButton = button;
				}

				newButtonGroup = TRUE;
				CreateButtonGroupSubmenus(menu, options);
			} else {
				// Create regular button
				buttInx = buttonCnt;
				AddToolbarButton(helpKey, icon, options, DoCommandB, I2VP(commandCnt));
			}
		}
	}

	// ========================================================================
	// Sticky Button Setup
	// ========================================================================

	if (nameStr[0] != '\0') {
		int stickyIndex;
		stickyMask = SetupStickyBehavior(options, newButtonGroup, nameStr,
		                                 &stickyIndex);
	}

	// ========================================================================
	// Menu Entry Creation
	// ========================================================================

	if (nameStr[0] != '\0') {
		wMenu_p targetMenu = (buttonGroupPopupM && commandsSubmenu) ?
		                     commandsSubmenu : menu;

		cmdMenus[MENU_MAIN] = wMenuPushCreate(targetMenu, helpKey, nameStr,
		                                      acclKey, DoCommandB, I2VP(commandCnt));

		if (buttonGroupPopupM) {
			cmdMenus[MENU_BUTTONGROUP] = wMenuPushCreate(
			                                     buttonGroupPopupM, helpKey, GetBalloonHelpStr(helpKey),
			                                     0, DoCommandB, I2VP(commandCnt));
		}

		if (options & (IC_POPUP | IC_POPUP2 | IC_POPUP3)) {
			wMenu_p popup1Menu, popup2Menu;

			if (buttonGroupPopupM) {
				popup1Menu = popup1Submenu;
				popup2Menu = popup2Submenu;
			} else {
				GetPopupMenus(options, &popup1Menu, &popup2Menu);
			}

			if (!(options & IC_SELECTED) && popup1Menu) {
				cmdMenus[MENU_POPUP1] = wMenuPushCreate(popup1Menu, helpKey,
				                                        nameStr, 0, DoCommandB,
				                                        I2VP(commandCnt));
			}

			if (popup2Menu) {
				cmdMenus[MENU_POPUP2] = wMenuPushCreate(popup2Menu, helpKey,
				                                        nameStr, 0, DoCommandB,
				                                        I2VP(commandCnt));
			}
		}
	}

	// ========================================================================
	// Command Registration
	// ========================================================================

	wIndex_t cmdInx = AddCommand(command, helpKey, nameStr, icon, reqLevel,
	                             options, acclKey, buttInx, stickyMask,
	                             cmdMenus, context);

	return cmdInx;
}

/*****************************************************************************
 *
 * BUTTON GROUP MANAGEMENT
 *
 */

/**
 * Begin a new button group.
 * Subsequent AddMenuButton calls will be grouped under a dropdown menu.
 *
 * @param menuTitle Title for the button group menu
 * @param helpKey Help key for the group
 * @param stickyLabel Label for sticky button behavior
 */
EXPORT void ButtonGroupBegin(const char *menuTitle, const char *helpKey,
                             const char *stickyLabel)
{
	buttonGroupMenuTitle = menuTitle;
	buttonGroupHelpKey = helpKey;
	buttonGroupStickyLabel = stickyLabel;
	buttonGroupPopupM = NULL;
	currentSplitButton = NULL;

	// Reset submenu pointers for new group
	commandsSubmenu = NULL;
	popup1Submenu = NULL;
	popup2Submenu = NULL;
}

/**
 * End the current button group.
 * Subsequent AddMenuButton calls will create standalone buttons.
 */
EXPORT void ButtonGroupEnd(void)
{
	buttonGroupMenuTitle = NULL;
	buttonGroupHelpKey = NULL;
	buttonGroupStickyLabel = NULL;
	buttonGroupPopupM = NULL;
}

/**  these seem to be menu entries that will be  used by demo playback */

static void MiscMenuItemCreate(wMenu_p m1, wMenu_p m2, const char * name,
                               const char * label, long acclKey, void * func, long option, void * context)
{
	wControl_p mp;
	mp = wMenuPushCreate(m1, name, label, acclKey, FormMenuPush,
	                     &menuPLs[menuPG.paramCnt]);
	if (m2)
		wMenuPushCreate(m2, name, label, acclKey, FormMenuPush,
		                &menuPLs[menuPG.paramCnt]);
	menuPLs[menuPG.paramCnt].control = (wControl_p) mp;
	menuPLs[menuPG.paramCnt].type = PD_MENUITEM;
	menuPLs[menuPG.paramCnt].valueP = func;
	menuPLs[menuPG.paramCnt].nameStr = CAST_AWAY_CONST name;
	menuPLs[menuPG.paramCnt].option = option;
	menuPLs[menuPG.paramCnt].context = context;

	menuPG.paramCnt++;
	CHECK( menuPG.paramCnt < COUNT(menuPLs) );
}

//static wMenu_p toolbarM;
static addButtonCallBack_t paramFilesCallback;

EXPORT void CreateMenus(void)
{
	wMenu_p fileM, editM, viewM, optionM, windowM, macroM, helpM,
	        manageM, addM, changeM, drawM;
	wMenu_p zoomM, zoomSubM;

	wControl_p zoomInM, zoomOutM, zoomExtentsM;

	wInitTooltip( tooltipTexts, TooltipsGetCount() );
	fileM = wMenuBarAdd(mainW, "menuFile", _("_File"));
	editM = wMenuBarAdd(mainW, "menuEdit", _("_Edit"));
	viewM = wMenuBarAdd(mainW, "menuView", _("_View"));
	addM = wMenuBarAdd(mainW, "menuAdd", _("_Add"));
	changeM = wMenuBarAdd(mainW, "menuChange", _("_Change"));
	drawM = wMenuBarAdd(mainW, "menuDraw", _("_Draw"));
	manageM = wMenuBarAdd(mainW, "menuManage", _("_Manage"));
	optionM = wMenuBarAdd(mainW, "menuOption", _("_Options"));
	macroM = wMenuBarAdd(mainW, "menuMacro", _("Mac_ro"));
	windowM = wMenuBarAdd(mainW, "menuWindow", _("_Window"));
	helpM = wMenuBarAdd(mainW, "menuHelp", _("_Help"));

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
	MiscMenuItemCreate(fileM, NULL, "clear", _("_New ..."), ACCL_NEW,
	                   DoClear, 0, NULL);
	wMenuPushCreate(fileM, "load", _("_Open ..."), ACCL_OPEN,
	                ChkLoad, NULL);

	wMenu_p recentuseMenu = wMenuMenuCreate(fileM, "menuRecentlyUsedFiles",
	                                        _("Recent files"));
	fileList_ml = wMenuListCreate(recentuseMenu, "fileListMenu", NEWEST_TOP,
	                              NUM_FILELIST,
	                              ChkFileList);

	wMenuSeparatorCreate(fileM);

	wMenuPushCreate(fileM, "save", _("_Save"), ACCL_SAVE,
	                DoSave, NULL);
	wMenuPushCreate(fileM, "saveAs", _("Save _As ..."), ACCL_SAVEAS,
	                DoSaveAs, NULL);
	wMenuPushCreate(fileM, "revert", _("Revert"), ACCL_REVERT,
	                ChkRevert, NULL);

	cmdGroup = BG_FILE | BG_BIGGAP;
	AddToolbarButton("clear", CreateToolbarIconFromResource("doc-new.png"),
	                 IC_MODETRAIN_TOO, DoClear, NULL);
	AddToolbarButton("load", CreateToolbarIconFromResource("doc-open.png"),
	                 IC_MODETRAIN_TOO, ChkLoad, NULL);
	AddToolbarButton("save", CreateToolbarIconFromResource("doc-save.png"),
	                 IC_MODETRAIN_TOO, DoSave, NULL);
	wMenuSeparatorCreate(fileM);

	cmdGroup = BG_PRINT;
	MiscMenuItemCreate(fileM, NULL, "printSetup", _("P&rint Setup ..."),
	                   ACCL_PRINTSETUP, (wMenuCallBack_p) wPrintSetup, 0,
	                   I2VP(0));
	InitCmdPrint(fileM);
	AddToolbarButton("menuFile-setup",
	                 CreateToolbarIconFromResource("doc-setup.png"),
	                 IC_MODETRAIN_TOO, (wMenuCallBack_p) wPrintSetup, I2VP(0));

	wMenuSeparatorCreate(fileM);
	MiscMenuItemCreate(fileM, NULL, "cmdImport", _("&Import"), ACCL_IMPORT,
	                   DoImportObjects, 0, I2VP(0));
	MiscMenuItemCreate(fileM, NULL, "cmdImportModule", _("Import &Module"),
	                   ACCL_IMPORT_MOD,
	                   DoImportModule, 0, I2VP(1));
	MiscMenuItemCreate(fileM, NULL, "cmdImportDxf", _("Import &Dxf"),
	                   ACCL_IMPORT_DXF,
	                   DoImportDxf, 0, I2VP(1));

	wMenuSeparatorCreate(fileM);

	MiscMenuItemCreate(fileM, NULL, "cmdExport", _("E&xport"), ACCL_EXPORT,
	                   DoExport, IC_SELECTED, NULL);
	MiscMenuItemCreate(fileM, NULL, "cmdOutputbitmap", _("Export to &Bitmap"),
	                   ACCL_PRINTBM, OutputBitMapInit(), 0,
	                   NULL);
	MiscMenuItemCreate(fileM, NULL, "cmdExportDXF", _("Export DXF"),
	                   ACCL_EXPORTDXF, DoExportDxf, IC_SELECTED,
	                   NULL);
#if XTRKCAD_CREATE_SVG
	MiscMenuItemCreate( fileM, NULL, "cmdExportSVG", _("Export SVG"),
	                    ACCL_EXPORTSVG, DoExportSVG, IC_SELECTED, NULL);
#endif
	wMenuSeparatorCreate(fileM);

	paramFilesCallback = ParamFilesInit();
	MiscMenuItemCreate(fileM, NULL, "cmdPrmfile", _("Parameter &Files ..."),
	                   ACCL_PARAMFILES, paramFilesCallback, 0, NULL);
	MiscMenuItemCreate(fileM, NULL, "cmdFileNote", _("No&tes ..."), ACCL_NOTES,
	                   DoNote, 0, NULL);


	//fileList_ml = wMenuListCreate(fileM, "menuFileList", NEWEST_TOP, NUM_FILELIST,
	//                              ChkFileList);
	wMenuSeparatorCreate(fileM);
	wMenuPushCreate(fileM, "quit", _("E&xit"), 0,
	                DoQuit, NULL);

	InitCmdExport();

	AddToolbarButton("menuFile-parameter",
	                 CreateToolbarIconFromResource("parameter.png"),
	                 IC_MODETRAIN_TOO, paramFilesCallback, NULL);

	cmdGroup = BG_ZOOM;
	zoomUpB = AddToolbarButton("cmdZoomIn",
	                           CreateToolbarIconFromResource("zoom-in.png"),
	                           IC_MODETRAIN_TOO, DoZoomUp, NULL);
	zoomM = wMenuPopupCreate(mainW, "");
	AddToolbarButton("cmdZoom", CreateToolbarIconFromResource("zoom-choose.png"),
	                 IC_MODETRAIN_TOO,
	                 (wButtonCallBack_p) wMenuPopupShow, zoomM);
	zoomDownB = AddToolbarButton("cmdZoomOut",
	                             CreateToolbarIconFromResource("zoom-out.png"),
	                             IC_MODETRAIN_TOO, DoZoomDown, NULL);
	zoomExtentsB = AddToolbarButton("cmdZoomExtent",
	                                CreateToolbarIconFromResource("zoom-extent.png"),
	                                IC_MODETRAIN_TOO, DoZoomExtents, NULL);

	cmdGroup = BG_UNDO;
	undoB = AddToolbarButton("cmdUndo", CreateToolbarIconFromResource("undo.png"),
	                         0,
	                         UndoUndo, NULL);
	redoB = AddToolbarButton("cmdRedo", CreateToolbarIconFromResource("redo.png"),
	                         0,
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


	mapShowMI = wMenuToggleCreate(viewM, "cmdMapShow", _("Show/Hide Map"),
	                              ACCL_MAPSHOW, MapGetVisiblePref(),
	                              MapWindowToggleShow, NULL);

	wMenuSeparatorCreate(viewM);

	InitToolbar();
	MiscMenuItemCreate(viewM, NULL, "cmdToolbarOpt", _("&Toolbar Options..."),
	                   0L, DoToolbar, IC_MODETRAIN_TOO, NULL);

	cmdGroup = BG_EASE;
	InitCmdEasement();

	cmdGroup = BG_SNAP;
	InitSnapGridButtons();
	magnetsB = AddToolbarButton("cmdMagneticSnap",
	                            CreateToolbarIconFromResource("magnet.png"),
	                            IC_MODETRAIN_TOO | IC_TOGGLE, MagneticSnapToggle, NULL);
	wControlLinkedSet(magnetsMI,  magnetsB);
	wButtonSetBusy(magnetsB, (wBool_t) magneticSnap);

	mapShowB = AddToolbarButton("cmdMapShow",
	                            CreateToolbarIconFromResource("map.png"),
	                            IC_MODETRAIN_TOO | IC_TOGGLE, MapWindowToggleShow, NULL);
	wControlLinkedSet( mapShowMI, mapShowB);
	wButtonSetBusy(mapShowB, (wBool_t) mapVisible);

	/*
	 * ADD MENU
	 */

	cmdGroup = BG_TRKCRT;
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
	FormRegister(&addElevPG);

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

#define MAX_ENTRIES_IN_WINDOWLIST 25

	winList_mi = wMenuListCreate(windowM, "menuWindow", NEWEST_TOP,
	                             MAX_ENTRIES_IN_WINDOWLIST,
	                             DoShowWindow);
	wMenuListAdd(winList_mi, 0, _("Main window"), mainW);

	/*
	 * HELP MENU
	 */

	/* main help window */
	wMenuAddHelp(helpM);

	/* help on recent messages */
	wMenuSeparatorCreate(helpM);
	wMenu_p messageListM = wMenuMenuCreate(helpM, "menuHelpRecentMessages",
	                                       _("Recent Messages"));
	messageList_ml = wMenuListCreate(messageListM, "messageListM", NEWEST_TOP, 10,
	                                 ShowMessageHelp);
	wMenuListAdd(messageList_ml, 0, _(MESSAGE_LIST_EMPTY), NULL);
	wMenuPushCreate(helpM, "menuHelpProblemrep", _("Collect Problem Info"), 0,
	                DoProblemCollect, NULL);

	/* tip of the day */
	wMenuSeparatorCreate( helpM );
	wMenuPushCreate( helpM, "cmdTip", _("Tip of the Day..."), 0, ShowTip,
	                 I2VP(SHOWTIP_FORCESHOW | SHOWTIP_NEXTTIP));
	demoM = wMenuMenuCreate( helpM, "cmdDemo", _("_Demos") );
	wMenuPushCreate( helpM, "cmdExamples", _("Examples..."), 0, ChkExamples, NULL);

	/* about window */
	wMenuSeparatorCreate(helpM);
	wMenuPushCreate(helpM, "about", _("About"), 0,
	                CreateAboutW, NULL);

	/*
	 * MANAGE MENU
	 */

	cmdGroup = BG_TRAIN;
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

	cmdGroup = BG_LAYER;

	InitCmdSelect2(changeM);
	InitCmdDescribe2(changeM);
	InitCmdPan2(changeM);

	/**  \todo Layers for the toolbar */
	InitLayers(BG_LAYER);

	cmdGroup = BG_HOTBAR;
	InitHotBar();

	InitBenchDialog();

	ToolbarLayout(NULL);

	wPrefGetInteger( "sticky", "set", &stickySet, stickySet );
	FormRegister(&stickyPG);
	RegisterChangeNotification( StickyChange );
}


static void InitCmdExport(void)
{
	ButtonGroupBegin( _("Import/Export"), "cmdExportImportSetCmd",
	                  _("Import/Export") );
	cmdGroup = BG_EXPORTIMPORT;
	AddToolbarButton("cmdExport", CreateToolbarIconFromResource("doc-export.png"),
	                 IC_SELECTED | IC_ACCLKEY, DoExport, NULL);
	AddToolbarButton("cmdExportDXF",
	                 CreateToolbarIconFromResource("doc-export-dxf.png"),
	                 IC_SELECTED | IC_ACCLKEY, DoExportDxf, I2VP(1));
	AddToolbarButton("cmdExportBmap",
	                 CreateToolbarIconFromResource("doc-export-bmap.png"), IC_ACCLKEY,
	                 OutputBitMapInit(), NULL);
#if XTRKCAD_CREATE_SVG
	AddToolbarButton("cmdExportSVG",
	                 CreateToolbarIconFromResource("doc-export-svg.png"),
	                 IC_ACCLKEY, DoExportSVG, NULL); // IC_SELECTED |
#endif
	AddToolbarButton("cmdImport", CreateToolbarIconFromResource("doc-import.png"),
	                 IC_ACCLKEY,
	                 DoImportObjects, I2VP(0));
	AddToolbarButton("cmdImportModule",
	                 CreateToolbarIconFromResource("doc-import-mod.png"), IC_ACCLKEY,
	                 DoImportModule, I2VP(1));
	AddToolbarButton("cmdImportDxf",
	                 CreateToolbarIconFromResource("doc-import-dxf.png"), IC_ACCLKEY,
	                 DoImportDxf, I2VP(1));
	ButtonGroupEnd();

	FormRegister( &menuPG );
	AddPlaybackProc( "MENU", MenuPlayback, NULL );

	ButtonGroupEnd();

}
