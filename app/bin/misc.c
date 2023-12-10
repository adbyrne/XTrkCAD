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



#include "misc.h"
#include "cselect.h"
#include "cundo.h"
#include "custom.h"
#include "draw.h"
#include "fileio.h"
#include "layout.h"
#include "param.h"
#include "include/paramfilelist.h"
#include "paths.h"
#include "smalldlg.h"
#include "track.h"
#include "common-ui.h"

#include <inttypes.h>

#define DEFAULT_SCALE ("N")


/****************************************************************************
 *
 EXPORTED VARIABLES
 *
 */

EXPORT int iconSize = 0;

EXPORT wWin_p mainW;

EXPORT char message[STR_HUGE_SIZE];
static char message2[STR_LONG_SIZE];

EXPORT long paramVersion = -1;

EXPORT coOrd zero = { 0.0, 0.0 };

EXPORT wBool_t extraButtons = FALSE;

EXPORT long
onStartup; /**< controls behaviour after startup: load last layout if zero, else start with blank canvas */

static int verbose = 0;

static BOOL_T inMainW = TRUE;

EXPORT long units = 0;				/**< measurement units: 0 = English, 1 = metric */

EXPORT long labelScale = 8;
EXPORT long labelEnable = (LABELENABLE_ENDPT_ELEV|LABELENABLE_CARS);
/** @prefs [draw] label-when=2 Unknown */
EXPORT long labelWhen = 2;

EXPORT long dontHideCursor = 0;
#ifdef HIDESELECTIONWINDOW
EXPORT long hideSelectionWindow = 0;
#endif


/****************************************************************************
 *
 * MEMORY ALLOCATION
 *
 */

static size_t totalMallocs = 0;
static size_t totalMalloced = 0;
static size_t totalRealloced = 0;
static size_t totalReallocs = 0;
static size_t totalFreeed = 0;
static size_t totalFrees = 0;

static void * StorageLog;

typedef struct slog_t {
	void * storage_p;
	size_t storage_size;
	BOOL_T freed;
} slog_t, * slog_p;

static int StorageLogCurrent = 0;


#define LOG_SIZE 1000000


static unsigned long guard0 = 0xDEADBEEF;
static unsigned long guard1 = 0xAF00BA8A;
static int log_malloc;

static void RecordMalloc(void * p, size_t size)
{


	if (!StorageLog) { StorageLog = malloc(sizeof(slog_t)*LOG_SIZE); }
	slog_p log_p = StorageLog;
	if (StorageLogCurrent<LOG_SIZE) {
		log_p[StorageLogCurrent].storage_p = p;
		log_p[StorageLogCurrent].storage_size = size;
		StorageLogCurrent++;
	} else {
		printf("Storage Log size exceeded, wrapped\n");
		log_p[0].storage_p = p;
		log_p[0].storage_size = size;
		StorageLogCurrent = 1;
	}
}

static void RecordMyFree(void *p)
{
	slog_p log_p = StorageLog;
	if (log_p) {
		for (int i=0; i<StorageLogCurrent; i++) {
			if (!log_p[i].freed && log_p[i].storage_p == p) {
				log_p[i].freed = TRUE;
			}
		}
	}
}

#define SLOG_FMT "0x%.12" PRIxPTR

EXPORT BOOL_T TestMallocs()
{
	size_t oldSize;
	size_t testedMallocs = 0;
	void * old;
	slog_p log_p = StorageLog;
	BOOL_T rc = TRUE;
	if (log_p) {
		for (int i=0; i<StorageLogCurrent; i++) {
			if (log_p[i].freed) { continue; }
			old = log_p[i].storage_p;
			oldSize = log_p[i].storage_size;
			if (*(unsigned long*) ((char*) old - sizeof(unsigned long)) != guard0) {
				LogPrintf("Guard 0 hosed, " SLOG_FMT " size: %llu \n", (uintptr_t)old, oldSize);
				rc = FALSE;
			}
			if (*(unsigned long*) ((char*) old + oldSize) != guard1) {
				LogPrintf("Guard 1 hosed, " SLOG_FMT " size: %llu \n", (uintptr_t)old, oldSize);
				rc = FALSE;
			}
			testedMallocs++;
		}
	}
	LogPrintf("Tested: %llu Mallocs: %llu Total Malloced: %llu Freed: %llu Total Freed: %llu \n",
	          testedMallocs, totalMallocs, totalMalloced, totalFrees, totalFreeed);
	return rc;
}

/**
 * Allocate memory
 *
 * Allocated memory has 'guard' values, before and after to detect overruns.
 * Aborts on allocation failure.
 *
 * \param size IN amount of memory to allocate
 *
 * \return Pointer to allocated memory - never NULL
 */

EXPORT void * MyMalloc(size_t size)
{
	void * p;
	totalMallocs++;
	totalMalloced += size;
	p = malloc((size_t) size + sizeof(size_t) + 2 * sizeof(unsigned long));
	if ( p == NULL ) {
		// We're hosed, get out of town
		lprintf( "malloc(%ld) failed\n", size );
		abort();
	}

	LOG1(log_malloc,
	     ( "  Malloc(%ld) = " SLOG_FMT " (" SLOG_FMT "-" SLOG_FMT ")\n",
	       size, (size_t)((char*)p+sizeof (size_t) + sizeof (unsigned long)),
	       (size_t)p,
	       (size_t)((char*)p+size+sizeof (size_t) + 2 * sizeof(unsigned long))));

	*(size_t*) p = (size_t) size;
	p = (char*) p + sizeof(size_t);
	*(unsigned long*) p = guard0;
	p = (char*) p + sizeof(unsigned long);
	*(unsigned long*) ((char*) p + size) = guard1;
	memset(p, 0, (size_t )size);
	if (extraButtons) {
		RecordMalloc(p,size);
	}
	return p;
}

/**
 * Reallocate memory
 *
 * Allocated memory has 'guard' values, before and after to detect overruns.
 * Aborts on allocation failure.
 *
 * \param old IN existing pointer to allocated memory
 * \param size IN amount of memory to allocate
 *
 * \return Pointer to reallocated memory - never NULL
 */

EXPORT void * MyRealloc(void * old, size_t size)
{
	size_t oldSize;
	void * new;
	if (old == NULL) {
		return MyMalloc(size);
	}
	totalReallocs++;
	totalRealloced += size;
	CHECKMSG( (*(unsigned long*) ((char*) old - sizeof(unsigned long)) == guard0),
	          ("Guard0 is hosed") );
	oldSize = *(size_t*) ((char*) old - sizeof(unsigned long) - sizeof(size_t));
	CHECKMSG( (*(unsigned long*) ((char*) old + oldSize) == guard1),
	          ( "Guard1 is hosed" ) );

	LOG1(log_malloc, ("  Realloc (" SLOG_FMT ",%ld) was %d\n", (size_t)old, size,
	                  oldSize ))

	if ((long) oldSize == size) {
		return old;
	}
	new = MyMalloc(size);
	memcpy(new, old, min((size_t )size, oldSize));
	MyFree(old);
	return new;
}

EXPORT void MyFree(void * ptr)
{
	size_t oldSize;
	totalFrees++;
	if (ptr==NULL) {
		return;
	}
	CHECKMSG( (*(unsigned long*) ((char*) ptr - sizeof(unsigned long)) == guard0),
	          ( "Guard0 is hosed") );
	oldSize = *(size_t*) ((char*) ptr - sizeof(unsigned long)
	                      - sizeof(size_t));
	CHECKMSG( (*(unsigned long*) ((char*) ptr + oldSize) == guard1),
	          ( "Guard1 is hosed" ) );

	LOG1(log_malloc,
	     ("  Free %ld at " SLOG_FMT " (" SLOG_FMT "-" SLOG_FMT ")\n",
	      oldSize,
	      (size_t)ptr,
	      (size_t)((char*)ptr-sizeof *(size_t*)0-sizeof *(long*)0),
	      (size_t)((char*)ptr+oldSize+sizeof *(long*)0)));

	totalFreeed += oldSize;
	free((char*) ptr - sizeof *(long*) 0 - sizeof *(size_t*) 0);
	if (extraButtons) {
		RecordMyFree(ptr);
	}
}

EXPORT void * memdup(void * src, size_t size)
{
	void * p;
	p = MyMalloc(size);
	memcpy(p, src, size);
	return p;
}

EXPORT char * MyStrdup(const char * str)
{
	char * ret;
	ret = (char*) MyMalloc(strlen(str) + 1);
	strcpy(ret, str);
	return ret;
}


/****************************************************************************
 *
 * CHARACTER CONVERSION
 *
 */
/*
 * Convert Text into the equivalent form that can be written to a file or put in a text box by adding escape characters
 *
 * The following special characters are produced -
 *  \n for LineFeed 	0x0A
 *  \t for Tab 			0x09
 *  "" for "  			This is so that a CSV conformant type program can interpret the file output
 *
 */
EXPORT char * ConvertToEscapedText(const char * text)
{
	int text_i = 0;
	int add = 0;   //extra chars for escape
	while (text[text_i]) {
		switch (text[text_i]) {
		case '\n':
			add++;
			break;
		case '\t':
			add++;
			break;
		case '\\':
			add++;
			break;
		case '\"':
			add++;
			break;
		}
		text_i++;
	}
	size_t cnt = strlen(text) + 1 + add;
#ifdef WINDOWS
	cnt *= 2;
#endif
	char * cout = MyMalloc(cnt);
	int cout_i = 0;
	text_i = 0;
	while (text[text_i]) {
		char c = text[text_i];
		switch (c) {
		case '\n':
			cout[cout_i] = '\\';
			cout_i++;
			cout[cout_i] = 'n';
			cout_i++;
			break;	// Line Feed
		case '\t':
			cout[cout_i] = '\\';
			cout_i++;
			cout[cout_i] = 't';
			cout_i++;
			break;	// Tab
		case '\\':
			cout[cout_i] = '\\';
			cout_i++;
			cout[cout_i] = '\\';
			cout_i++;
			break;	// BackSlash
		case '\"':
			cout[cout_i] = '\"';
			cout_i++;
			cout[cout_i] = '\"';
			cout_i++;
			break; // Double Quotes
		default:
			cout[cout_i] = c;
			cout_i++;
		}
		text_i++;
	}
	cout[cout_i] = '\0';
#ifdef UTFCONVERT
	wSystemToUTF8(cout, cout, (unsigned int)cnt);
#endif // UTFCONVERT

	return cout;
}

/*
 * Convert Text that has embedded escape characters into the equivalent form that can be shown on the screen
 *
 * The following special characters are supported -
 *  \n = LineFeed 	0x0A
 *  \t = Tab 		0x09
 *  \\ = \ 			The way to still produce backslash
 *
 */
EXPORT char * ConvertFromEscapedText(const char * text)
{
	enum {
		CHARACTER, ESCAPE
	} state = CHARACTER;
	char * cout = MyMalloc(strlen(text) + 1);  //always equal to or shorter than
	int text_i = 0;
	int cout_i = 0;
	int c;
	while (text[text_i]) {
		c = text[text_i];
		switch (state) {
		case CHARACTER:
			if (c == '\\') {
				state = ESCAPE;
			} else {
				cout[cout_i] = c;
				cout_i++;
			}
			break;

		case ESCAPE:
			switch (c) {
			case '\\':
				cout[cout_i] = '\\';
				cout_i++;
				break;  // "\\" = "\"
			case 'n':
				cout[cout_i] = '\n';
				cout_i++;
				break;	// LF
			case 't':
				cout[cout_i] = '\t';
				cout_i++;
				break;	// TAB
			}
			state = CHARACTER;
			break;
		}
		text_i++;
	}
	cout[cout_i] = '\0';
	return cout;
}


/****************************************************************************
 *
 * MESSAGES
 *
 */
/**
 * Print the message into internal buffer
 *
 * Called from CHECKMSG define
 * \param sFormat IN printf-type format string
 * \param ... IN vargs printf-type operands
 *
 * \return address of internal buffer containing formatted message
 */
EXPORT const char * AbortMessage(
        const char * sFormat,
        ... )
{
	static char sMessage[STR_SIZE];
	if ( sFormat == NULL ) {
		return "";
	}
	va_list ap;
	va_start(ap, sFormat);
	vsnprintf(sMessage, sizeof sMessage, sFormat, ap);
	va_end(ap);
	return sMessage;
}

/**
 * Display error notice box
 * Offer chance to save layout
 * Abort the program
 *
 * Called from CHECK/CHECKMSG defines
 *
 * \param sCond IN string-ized error condition
 * \param sFileName IN file name of fault
 * \param iLineNumber IN line number of fault
 * \param sMsg IN extra message with additional info
 *
 * \return No return
 */
EXPORT void AbortProg(
        const char * sCond,
        const char * sFileName,
        int iLineNumber,
        const char * sMsg )
{
	static BOOL_T abort2 = FALSE;
	snprintf( message, sizeof message, "%s: %s:%d %s", sCond, sFileName,
	          iLineNumber, sMsg?sMsg:"" );
	if (abort2) {
		wNoticeEx( NT_ERROR, message, _("ABORT"), NULL);
	} else {
		abort2 = TRUE;  // no 2nd chance
		strcat(message, _("\nDo you want to save your layout?"));
		int rc = wNoticeEx( NT_ERROR, message, _("Ok"), _("ABORT"));
		if (rc) {
			DoSaveAs(abort);
		} else {
			abort();
		}
	}
}

EXPORT char * Strcpytrimed(char * dst, const char * src, BOOL_T double_quotes)
{
	const char * cp;
	while (*src && isspace((unsigned char) *src)) {
		src++;
	}
	if (!*src) {
		return dst;
	}
	cp = src + strlen(src) - 1;
	while (cp > src && isspace((unsigned char) *cp)) {
		cp--;
	}
	while (src <= cp) {
		if (*src == '"' && double_quotes) {
			*dst++ = '"';
		}
		*dst++ = *src++;
	}
	*dst = '\0';
	return dst;
}

// ??? Referenced from gtklib.browserhelp.c ???
static char * directory;

EXPORT wBool_t CheckHelpTopicExists(const char * topic)
{

	char * htmlFile;

	// Check the file exits in the distro

	if (!directory) {
		directory = malloc(BUFSIZ);
	}

	if (directory == NULL) { return 0; }

	sprintf(directory, "%s/html/", wGetAppLibDir());

	htmlFile = malloc(strlen(directory)+strlen(topic) + 6);

	sprintf(htmlFile, "%s%s.html", directory, topic);

	if( access( htmlFile, F_OK ) == -1 ) {

		printf("Missing help topic %s\n",topic);

		free(htmlFile);

		return 0;

	}

	free(htmlFile);

	return 1;

}

static const char * ParseMessage(const char *msgSrc)
{
	char *cp1 = NULL, *cp2 = NULL;
	static char shortMsg[STR_SIZE];
	cp1 = strchr(_(msgSrc), '\t');
	if (cp1) {
		cp2 = strchr(cp1 + 1, '\t');
		if (cp2) {
			cp1++;
			memcpy(shortMsg, cp1, cp2 - cp1);
			shortMsg[cp2 - cp1] = 0;
			cp1 = shortMsg;
			cp2++;
		} else {
			cp1++;
			cp2 = cp1;
		}
		MessageListAppend( cp1, _(msgSrc) );
		return cp2;
	} else {
		return _(msgSrc);
	}
}

EXPORT void InfoMessage(const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	format = ParseMessage(format);
	vsnprintf(message2, 1020, format, ap);
	va_end(ap);
	/*InfoSubstituteControl( NULL, NULL );*/
	if (inError) {
		return;
	}
	SetMessage(message2);
}

EXPORT void ErrorMessage(const char * format, ...)
{
	va_list ap;
	va_start(ap, format);
	format = ParseMessage(format);
	vsnprintf(message2, 1020, format, ap);
	va_end(ap);
	InfoSubstituteControls( NULL, NULL);
	SetMessage(message2);
	wBeep();
	inError = TRUE;
}

EXPORT int NoticeMessage(const char * format, const char * yes, const char * no,
                         ...)
{
	va_list ap;
	va_start(ap, no);
	format = ParseMessage(format);
	vsnprintf(message2, 1020, format, ap);
	va_end(ap);
	return wNotice(message2, yes, no);
}

EXPORT int NoticeMessage2(int playbackRC, const char * format, const char * yes,
                          const char * no,
                          ...)
{
	va_list ap;
	if (inPlayback) {
		return playbackRC;
	}
	va_start(ap, no);
	format = ParseMessage(format);
	vsnprintf(message2, 1020, format, ap);
	va_end(ap);
	return wNoticeEx( NT_INFORMATION, message2, yes, no);
}


/*****************************************************************************
 *
 * MAIN BUTTON HANDLERS
 *
 */
/**
 * Confirm a requested operation in case of possible loss of changes.
 *
 * \param label2 IN operation to be cancelled, unused at the moment
 * \param after IN function to be executed on positive confirmation
 * \return true if proceed, false if cancel operation
 */

/** TODO: make sensible messages when requesting confirmation */

EXPORT bool Confirm(char * label2, doSaveCallBack_p after)
{
	int rc = -1;
	if (changed) {
		rc = wNotice3(_("Save changes to the layout design before closing?\n\n"
		                "If you don't save now, your unsaved changes will be discarded."),
		              _("&Save"), _("&Cancel"), _("&Don't Save"));
	}

	switch (rc) {
	case -1:	/* do not save */
		after();
		break;
	case 0:		/* cancel operation */
		break;
	case 1:		/* save */
		//LayoutBackGroundInit(FALSE);
		//LayoutBackGroundSave();
		DoSave(after);
		break;
	}
	return(rc != 0);
}


/*
 * Clean up before quitting
 */
static void DoQuitAfter(void)
{
	changed = 0;
	CleanupCheckpointFiles();  //Get rid of checkpoint if we quit.
	CleanupTempArchive();		// removefiels used for archive handling
	SaveState();
}
/**
 * Process shutdown request. This function is called when the user requests
 * to close the application. Before shutting down confirmation is gotten to
 * prevent data loss.
 */
EXPORT void DoQuit(void * unused)
{
	if (Confirm(_("Quit"), DoQuitAfter)) {

#ifdef CHECK_UNUSED_BALLOONHELP
		ShowUnusedBalloonHelp();
#endif
		LogClose();
		wExit(0);
	}
}

static void DoClearAfter(void)
{

	Reset();
	ClearTracks();
	ResetLayers(); // set all layers to their default properties and set current layer to 0

	DoLayout(NULL);
	checkPtMark = changed = 0;
	DoChangeNotification( CHANGE_MAIN|CHANGE_MAP );
	bReadOnly = TRUE;
	EnableCommands();
	SetLayoutFullPath("");
	SetWindowTitle();
	LayoutBackGroundInit(TRUE);
}

EXPORT void DoClear(void * unused)
{
	Confirm(_("Clear"), DoClearAfter);
}

/**
 * Toggle visibility state of map window.
 */

EXPORT void MapWindowToggleShow(void * unused)
{
	MapWindowShow(!mapVisible);
}

/**
 * Set visibility state of map window.
 *
 * \param state IN TRUE if visible, FALSE if hidden
 */

EXPORT void MapWindowShow(int state)
{
	mapVisible = state;
	wPrefSetInteger("misc", "mapVisible", mapVisible);
	wMenuToggleSet(mapShowMI, mapVisible);

	if (mapVisible) {
		DoChangeNotification(CHANGE_MAP);
	}

	wWinShow(mapW, mapVisible);
	wButtonSetBusy(mapShowB, (wBool_t) mapVisible);
}


EXPORT void DoShowWindow(int index, const char * name, void * data)
{
	if (data == mapW) {
		if (mapVisible == FALSE) {
			MapWindowShow( TRUE);
			return;
		}
	}
	wWinShow((wWin_p) data, TRUE);
}

static dynArr_t demoWindows_da;
#define demoWindows(N) DYNARR_N( wWin_p, demoWindows_da, N )

EXPORT void wShow(wWin_p win)
{
	int inx;
	if (inPlayback && win != demoW) {
		wWinSetBusy(win, TRUE);
		for (inx = 0; inx < demoWindows_da.cnt; inx++)
			if ( demoWindows(inx) == win) {
				break;
			}
		if (inx >= demoWindows_da.cnt) {
			for (inx = 0; inx < demoWindows_da.cnt; inx++)
				if ( demoWindows(inx) == NULL) {
					break;
				}
			if (inx >= demoWindows_da.cnt) {
				DYNARR_APPEND(wWin_p, demoWindows_da, 10);
				inx = demoWindows_da.cnt - 1;
			}
			demoWindows(inx) = win;
		}
	}
	if (win != mainW) {
		wMenuListAdd(winList_mi, -1, wWinGetTitle(win), win);
	}
	wWinShow(win, TRUE);
}

EXPORT void wHide(wWin_p win)
{
	int inx;
	wWinShow(win, FALSE);
	wWinSetBusy(win, FALSE);
	if (inMainW && win == aboutW) {
		return;
	}
	wMenuListDelete(winList_mi, wWinGetTitle(win));
	ParamResetInvalid( win );
	if (inPlayback)
		for (inx = 0; inx < demoWindows_da.cnt; inx++)
			if ( demoWindows(inx) == win) {
				demoWindows(inx) = NULL;
			}
}

EXPORT void CloseDemoWindows(void)
{
	int inx;
	for (inx = 0; inx < demoWindows_da.cnt; inx++)
		if ( demoWindows(inx) != NULL) {
			wHide(demoWindows(inx));
		}
	DYNARR_RESET( wWin_p, demoWindows_da );
}

EXPORT void DefaultProc(wWin_p win, winProcEvent e, void * data)
{
	switch (e) {
	case wClose_e:
		wMenuListDelete(winList_mi, wWinGetTitle(win));
		if (data != NULL) {
			ConfirmReset( FALSE);
		}
		wWinDoCancel(win);
		break;
	default:
		break;
	}
}

static void NextWindow(void)
{
}



/*****************************************************************************
 *
 * ACCEL KEYS
 *
 */

enum eAccelAction_t { EA_ZOOMUP, EA_ZOOMDOWN, EA_REDRAW, EA_DELETE, EA_UNDO, EA_COPY, EA_PASTE, EA_CUT, EA_NEXT, EA_HELP };
struct accelKey_s {
	const char * sPrefName;
	wAccelKey_e eKey;
	int iMode;
	enum eAccelAction_t iAction;
	int iContext;
} aAccelKeys[] = {
	{ "zoomUp", wAccelKey_Pgdn, 0, EA_ZOOMUP, 1 },
	{ "zoomDown", wAccelKey_Pgup, 0, EA_ZOOMDOWN, 1 },
	{ "redraw", wAccelKey_F5, 0, EA_REDRAW, 0 },
#ifdef WINDOWS
	{ "delete", wAccelKey_Del, 0, EA_DELETE, 0 },
#endif
	{ "undo", wAccelKey_Back, WKEY_SHIFT, EA_UNDO, 0 },
	{ "copy", wAccelKey_Ins, WKEY_CTRL, EA_COPY, 0 },
	{ "paste", wAccelKey_Ins, WKEY_SHIFT, EA_PASTE, 0 },
	{ "cut", wAccelKey_Del, WKEY_SHIFT, EA_CUT, 0 },
	{ "nextWindow", wAccelKey_F6, 0, EA_NEXT, 0 },
	{ "zoomUp", wAccelKey_Numpad_Add, WKEY_CTRL, EA_ZOOMUP, 1 },
	{ "zoomDown", wAccelKey_Numpad_Subtract, WKEY_CTRL, EA_ZOOMDOWN, 1 },
	{ "help", wAccelKey_F1, WKEY_SHIFT, EA_HELP, 1 },
	{ "help-context", wAccelKey_F1, 0, EA_HELP, 3 }
};

static void AccelKeyDispatch( wAccelKey_e key, void * accelKeyIndexVP )
{
	int iAccelKeyIndex = (int)VP2L(accelKeyIndexVP);
	switch( aAccelKeys[iAccelKeyIndex].iAction ) {
	case EA_ZOOMUP:
		DoZoomUp( I2VP(aAccelKeys[iAccelKeyIndex].iContext) );
		break;
	case EA_ZOOMDOWN:
		DoZoomDown( I2VP(aAccelKeys[iAccelKeyIndex].iContext) );
		break;
	case EA_REDRAW:
		MainRedraw();
		break;
	case EA_DELETE:
		TrySelectDelete();
		break;
	case EA_UNDO:
		UndoUndo(NULL);
		break;
	case EA_COPY:
		EditCopy(NULL);
		break;
	case EA_PASTE:
		EditPaste(NULL);
		break;
	case EA_CUT:
		EditCut(NULL);
		break;
	case EA_NEXT:
		NextWindow();
		break;
	case EA_HELP:
		wDoAccelHelp(key, I2VP(aAccelKeys[iAccelKeyIndex].iContext));
		break;
	default:
		CHECK(FALSE);
	}
}

static char * accelKeyNames[] = { "Del", "Ins", "Home", "End", "Pgup", "Pgdn",
                                  "Up", "Down", "Right", "Left", "Back", "F1", "F2", "F3", "F4", "F5",
                                  "F6", "F7", "F8", "F9", "F10", "F11", "F12", "NumpadAdd", "NumpadSub"
                                };

static void SetAccelKeys()
{
	for ( int iAccelKey = 0; iAccelKey < COUNT( aAccelKeys ); iAccelKey++ ) {
		struct accelKey_s * akP = &aAccelKeys[iAccelKey];
		int eKey = akP->eKey;
		int iMode = akP->iMode;
		const char * sPrefValue = wPrefGetString("accelKey", akP->sPrefName);
		if (sPrefValue != NULL) {
			int iMode1 = 0;
			while (sPrefValue[1] == '-') {
				switch (sPrefValue[0]) {
				case 'S':
					iMode1 |= WKEY_SHIFT;
					break;
				case 'C':
					iMode1 |= WKEY_CTRL;
					break;
				case 'A':
					iMode1 |= WKEY_ALT;
					break;
				default:
					;
				}
				sPrefValue += 2;
			}
			for (int inx = 0; inx < COUNT( accelKeyNames ); inx++) {
				if (strcmp(sPrefValue, accelKeyNames[inx]) == 0) {
					eKey = inx + 1;
					iMode = iMode1;
					break;
				}
			}
		}
		wAttachAccelKey(eKey, iMode, AccelKeyDispatch, I2VP(iAccelKey));
	}
}


//EXPORT void InitCmdEnumerate(void) {
//	AddToolbarButton("cmdEnumerate", wIconCreatePixMap(partlist_xpm),
//			IC_SELECTED | IC_ACCLKEY, EnumerateTracks,
//			NULL);
//}



/****************************************************************************
 *
 * WMAIN
 *
 */

/* Give user the option to continue work after crash. This function gives the user
 * the option to load the checkpoint file to continue working after a crash.
 *
 * \param none
 * \return TRUE for resume, FALSE otherwise
 *
 */


static int OfferCheckpoint( void )
{
	int ret = FALSE;

	/* sProdName */
	ret =
	        wNotice3(
	                _(
	                        "Program was not terminated properly. Do you want to resume working on the previous trackplan?"),
	                _("Resume"), _("Resume with New Name"), _("Ignore Checkpoint"));
	//ret==1 Same, ret==-1 New, ret==0 Ignore
	if (ret == 1) {
		printf(_("Reload Checkpoint Selected\n"));
	} else if (ret == -1) {
		printf(_("Reload Checkpoint With New Name Selected\n"));
	} else {
		printf(_("Ignore Checkpoint Selected\n"));
	}
	if (ret>=0) {
		/* load the checkpoint file */
		LoadCheckpoint(ret==1);
		ret = TRUE;

	}
	return (ret>=0);
}

EXPORT wWin_p wMain(int argc, char * argv[])
{
	int c;
	int resumeWork;
	char * logFileName = NULL;
	int log_init = 0;
	int initialZoom = 0;
	char * initialFile = NULL;
	const char * pref;
	coOrd roomSize;
	long oldToolbarMax;
	long newToolbarMax;
	char *cp;
	char buffer[STR_SIZE];
	unsigned int i;
	wWinPix_t displayWidth;
	wWinPix_t displayHeight;
	BOOL_T bRunTests = FALSE;

	strcpy(buffer, sProdNameLower);

	/* Initialize application name */
	wInitAppName(buffer);

	/* Initialize gettext */
	InitGettext();

	/* Save user locale */
	SetCLocale();
	SetUserLocale();

	/*
	 * ARGUMENTS
	 */

	opterr = 0;
	LogSet("dummy",0);

	while ((c = getopt(argc, argv, "vl:d:c:mVT")) != -1)
		switch (c) {
		case 'c': /* configuration name */
			/* test for valid filename */
			for (i = 0; i < strlen(optarg); i++) {
				if (!isalnum((unsigned char) optarg[i]) && optarg[i] != '.') {
					NoticeMessage(MSG_BAD_OPTION, _("Ok"), NULL, optarg);
					exit(1);
				}
			}
			/* append delimiter and argument to configuration name */
			if (strlen(optarg) < STR_SIZE - strlen(";") - strlen(buffer) - 1) {
				strcat(buffer, ";");
				strcat(buffer, optarg);
			} else {
				NoticeMessage(MSG_BAD_OPTION, _("Ok"), NULL, optarg);
				exit(1);
			}
			break;
		case 'v': /* verbose flag  */
			verbose++;
			break;
		case 'd': /* define loglevel for a group */
			cp = strchr(optarg, '=');
			if (cp != NULL) {
				*cp++ = '\0';
				LogSet(optarg, atoi(cp));
			} else {
				LogSet(optarg, 1);
			}
			break;
		case 'l': /* define log filename */
			logFileName = strdup(optarg);
			break;
		case '?':
			NoticeMessage(MSG_BAD_OPTION, _("Ok"), NULL, argv[optind - 1]);
			exit(1);
		case 'm': // temporary: use MainRedraw instead of TempRedraw
			wDrawDoTempDraw = FALSE;
			break;
		case ':':
			NoticeMessage("Missing parameter for %s", _("Ok"), NULL,
			              argv[optind - 1]);
			exit(1);
			break;
		case 'V': // display version
			printf("Version: %s\n",XTRKCAD_VERSION);
			exit(0);
			break;
		case 'T': // run tests
			LogSet( "regression", 2 );
			bRunTests = TRUE;
			break;
		default:
			CHECK(FALSE);
		}
	if (optind < argc) {
		initialFile = strdup(argv[optind]);
	}

	extraButtons = (getenv(sEnvExtra) != NULL);
	LogOpen(logFileName);
	log_init = LogFindIndex("init");
	log_malloc = LogFindIndex("malloc");

	LOG1(log_init, ( "initCustom\n" ))
	InitCustom();

	/*
	 * MAIN WINDOW
	 */
	LOG1(log_init, ( "create main window\n" ))
	SetLayoutTitle(sProdName);
	sprintf(message, _("Unnamed Trackplan - %s(%s)"), sProdName, sVersion);

	wGetDisplaySize(&displayWidth, &displayHeight);
	mainW = wWinMainCreate(buffer, (displayWidth * 2) / 3,
	                       (displayHeight * 2) / 3, "xtrkcadW", message, "main",
	                       F_RESIZE | F_MENUBAR | F_NOTAB | F_RECALLPOS | F_RECALLSIZE | F_HIDE, MainProc,
	                       NULL);
	if (mainW == NULL) {
		return NULL;
	}

	wSetGeometry(mainW, displayWidth/2, displayWidth, displayHeight/2,
	             displayHeight, -1, -1, -1);
	InitAppDefaults();

	newToolbarMax = (1 << BG_COUNT) - 1;
	wPrefGetInteger("misc", "toolbarset", &toolbarSet, newToolbarMax);
	wPrefGetInteger("misc", "max-toolbarset", &oldToolbarMax, 0);
	toolbarSet |= newToolbarMax & ~oldToolbarMax;
	wPrefSetInteger("misc", "max-toolbarset", newToolbarMax);
	wPrefSetInteger("misc", "toolbarset", toolbarSet);

	LOG1(log_init, ( "fontInit\n"))

	wInitializeFonts();

	LOG1(log_init, ( "fileInit\n" ))
	FileInit();

	wCreateSplash(sProdName, sVersion);

	if (!initialFile) {
		WDOUBLE_T tmp;
		LOG1(log_init, ( "set roomsize\n" ))
		wPrefGetFloat("draw", "roomsizeX", &tmp, 96.0);
		roomSize.x = tmp;
		wPrefGetFloat("draw", "roomsizeY", &tmp, 48.0);
		roomSize.y = tmp;
		SetRoomSize(roomSize);
	}

	/*
	 * INITIALIZE
	 */
	LOG1(log_init, ( "initColor\n" ))
	InitColor();
	LOG1(log_init, ( "initInfoBar\n" ))
	InitInfoBar();
	wSetSplashInfo("Scale Init...");
	LOG1(log_init, ( "ScaleInit\n" ))
	ScaleInit();
	wPrefGetInteger( "draw", "label-when", &labelWhen, labelWhen );


	wSetSplashInfo(_("Initializing commands"));
	LOG1(log_init, ( "paramInit\n" ))
	ParamInit();
	LOG1(log_init, ( "initTrkTrack\n" ))
	InitTrkTrack();

	/*
	 * MENUS
	 */
	wSetSplashInfo(_("Initializing menus"));
	LOG1(log_init, ( "createMenus\n" ))
	CreateMenus();

	SetAccelKeys();

	LOG1(log_init, ( "initialize\n" ))
	if (!Initialize()) {
		return NULL;
	}

	LOG1(log_init, ( "loadFileList\n" ))
	LoadFileList();
	//CreateDebugW();

	/*
	 * TIDY UP
	 */
	if (toolbarSet&(1<<BG_HOTBAR)) {
		LayoutHotBar( NULL );
	} else {
		LayoutHotBar( NULL );   /* Must run once to set it up */
		HideHotBar();           /* Then hide */
	}
	LOG1(log_init, ( "drawInit\n" ))
	DrawInit(initialZoom);

	MacroInit();

	/*
	 * READ PARAMETERS
	 */
	wSetSplashInfo(_("Reading parameter files"));
	LOG1(log_init, ( "paramFileInit\n" ))

	SetParamFileDir(GetCurrentPath(
	                        LAYOUTPATHKEY));  //Set default for new parms to be the same as the layout

	if (!ParamFileListInit()) {
		return NULL;
	}

	/* initialize the layers */
	LOG1(log_init, ("DefaultLayerProperties"));
	DefaultLayerProperties();

	// LOG1(log_init, ("!ParamFileListInit()\n"))

	CommandInit();
	LOG1(log_init, ( "Reset\n" ))
	Reset();

	/*
	 * SCALE
	 */

	/* Set up the data for scale and gauge description */
	DoAllSetScaleDesc();

	// get the preferred scale from the configuration file
	pref = wPrefGetString("misc", "scale");
	if ( pref == NULL || DoSetScale( pref ) == FALSE ) {
		// if preferred scale was not set (eg. during initial run), initialize to a default value
		DoSetScale( DEFAULT_SCALE );
	}

	/* see whether last layout should be reopened on startup */
	wPrefGetInteger("DialogItem", "pref-onstartup", &onStartup, 0);

	/*
	 * THE END
	 */

	LOG1(log_init, ( "the end\n" ))
	EnableCommands();
	LOG1(log_init, ( "Initialization complete\n" ))
	wSetSplashInfo(_("Initialization complete"));
	DoChangeNotification( CHANGE_MAIN | CHANGE_MAP);

	wWinShow(mainW, TRUE);
	wWinShow(mapW, mapVisible);
	wDestroySplash();

	/* this has to be called before ShowTip() */
	InitSmallDlg();

	/* Compare the program version and display Beta warning if appropriate */
	pref = wPrefGetString("misc", "version");
	if((!pref) || (strcmp(pref,XTRKCAD_VERSION) != 0)) {
		if(strstr(XTRKCAD_VERSION,"Beta") != NULL) {
			NoticeMessage(MSG_BETA_NOTICE, _("Ok"),NULL, XTRKCAD_VERSION);
		}
		//else {
		//    NoticeMessage(_("New version welcome..."),_("Ok"),NULL);
		//}
		wPrefSetString("misc", "version", XTRKCAD_VERSION);
	} else {
		ShowTip(SHOWTIP_NEXTTIP);
	}

	/* check for existing checkpoint file */
	resumeWork = FALSE;
	if (ExistsCheckpoint()) {
		resumeWork = OfferCheckpoint();
	}

	if (!resumeWork) {
		/* if work is not to be resumed and no filename was given on startup, load last layout */
		if ((onStartup == 0) && (!initialFile || !strlen(initialFile))) {
			long iExample;
			initialFile = (char*)wPrefGetString("misc", "lastlayout");
			wPrefGetInteger("misc", "lastlayoutexample", &iExample, 0);
			bExample = (iExample == 1);
		}
		if (initialFile && strlen(initialFile)) {
			DoFileList(0, "1",
			           initialFile);   //Will load Background values, if archive, leave
			if (onStartup == 1) {
				LayoutBackGroundInit(TRUE);        //Wipe Out Prior Background
			} else {
				LayoutBackGroundInit(FALSE);        //Get Prior BackGround
			}
		} else {
			LayoutBackGroundInit(
			        TRUE);        // If onStartup==1 and no initial file - Wipe Out Prior Background
		}

	}
	MainRedraw();
	inMainW = FALSE;
	if ( bRunTests ) {
		int nFail = RegressionTestAll();
		if ( nFail == 0 ) {
			lprintf( "Regression Tests Pass\n" );
			exit( 0 );
		} else {
			lprintf( "%d Regression Tests Fail\n", nFail );
			exit( 1 );
		}
	}
	return mainW;
}

/****************************************************************************
 *
 * CHANGE NOTIFICATION
 *
 */
#define CNCB_COUNT (40)

static changeNotificationCallBack_t changeNotificationCallBacks[CNCB_COUNT];
static int changeNotificationCallBackCnt = 0;

EXPORT void RegisterChangeNotification(
        changeNotificationCallBack_t action )
{
	CHECK( (changeNotificationCallBackCnt + 1) < CNCB_COUNT );
	changeNotificationCallBacks[changeNotificationCallBackCnt] = action;
	changeNotificationCallBackCnt++;
}


EXPORT void DoChangeNotification( long changes )
{
	int inx;
	for (inx=0; inx<changeNotificationCallBackCnt; inx++) {
		changeNotificationCallBacks[inx](changes);
	}
}

