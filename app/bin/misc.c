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

#include <inttypes.h>

#include <stdint.h>

#define DEFAULT_SCALE ("N")


extern wBalloonHelp_t balloonHelp[];

static wMenuToggle_p mapShowMI;
static wMenuToggle_p magnetsMI;

#ifdef DEBUG
#define CHECK_BALLOONHELP
/*#define CHECK_UNUSED_BALLOONHELP*/
#endif
#ifdef CHECK_UNUSED_BALLOONHELP
static void ShowUnusedBalloonHelp(void);
#endif

/****************************************************************************
 *
 EXPORTED VARIABLES
 *
 */

EXPORT int iconSize = 0;

EXPORT int foobar = 0;

EXPORT int log_error;
static int log_command;

EXPORT wWin_p mainW;

EXPORT char message[STR_HUGE_SIZE];
static char message2[STR_LONG_SIZE];

EXPORT REGION_T curRegion = 0;

EXPORT long paramVersion = -1;

EXPORT coOrd zero = { 0.0, 0.0 };

EXPORT wBool_t extraButtons = FALSE;

EXPORT long onStartup; /**< controls behaviour after startup: load last layout if zero, else start with blank canvas */

EXPORT wButton_p undoB;
EXPORT wButton_p redoB;

EXPORT wButton_p zoomUpB;
EXPORT wButton_p zoomDownB;
EXPORT wButton_p zoomExtentsB;
wButton_p mapShowB;
wButton_p magnetsB;
wButton_p backgroundB;

EXPORT wIndex_t checkPtMark = 0;

EXPORT wMenu_p demoM;
EXPORT wMenu_p popup1M, popup2M;
EXPORT wMenu_p popup1aM, popup2aM;
EXPORT wMenu_p popup1mM, popup2mM;

static wIndex_t curCommand = 0;
EXPORT void * commandContext;
EXPORT wIndex_t cmdGroup;
EXPORT wIndex_t joinCmdInx;
EXPORT wIndex_t modifyCmdInx;
EXPORT long rightClickMode = 0;
EXPORT long selectMode = 0;
EXPORT long selectZero = 1;
EXPORT DIST_T easementVal = 0.0;
EXPORT DIST_T easeR = 0.0;
EXPORT DIST_T easeL = 0.0;
EXPORT coOrd cmdMenuPos;

EXPORT wWinPix_t DlgSepLeft = 12;
EXPORT wWinPix_t DlgSepMid = 18;
EXPORT wWinPix_t DlgSepRight = 12;
EXPORT wWinPix_t DlgSepTop = 12;
EXPORT wWinPix_t DlgSepBottom = 12;
EXPORT wWinPix_t DlgSepNarrow = 6;
EXPORT wWinPix_t DlgSepWide = 12;
EXPORT wWinPix_t DlgSepFrmLeft = 4;
EXPORT wWinPix_t DlgSepFrmRight = 4;
EXPORT wWinPix_t DlgSepFrmTop = 4;
EXPORT wWinPix_t DlgSepFrmBottom = 4;

static int verbose = 0;

static wMenuList_p winList_mi;
static BOOL_T inMainW = TRUE;

static long stickySet = 0;
static long stickyCnt = 0;
static const char * stickyLabels[33];
#define TOOLBARSET_INIT				(0xFFFF)
EXPORT long toolbarSet = TOOLBARSET_INIT;
EXPORT wWinPix_t toolbarHeight = 0;
static wWinPix_t toolbarWidth = 0;

static wMenuList_p messageList_ml;
static BOOL_T messageListEmpty = TRUE;
#define MESSAGE_LIST_EMPTY			N_("No Messages")

#define NUM_FILELIST (5)

extern long curTurnoutEp;
static wIndex_t printCmdInx;
static wIndex_t gridCmdInx;
static paramData_t menuPLs[101] = { { PD_LONG, &toolbarSet, "toolbarset" }, {
		PD_LONG, &curTurnoutEp, "cur-turnout-ep" } };
static paramGroup_t menuPG = { "misc", PGO_RECORD, menuPLs, 2 };

extern wBool_t wDrawDoTempDraw;

/****************************************************************************
 *
 * LOCAL UTILITIES
 *
 */

EXPORT size_t totalMallocs = 0;
EXPORT size_t totalMalloced = 0;
EXPORT size_t totalRealloced = 0;
EXPORT size_t totalReallocs = 0;
EXPORT size_t totalFreeed = 0;
EXPORT size_t totalFrees = 0;

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

static void RecordMalloc(void * p, size_t size) {


	if (!StorageLog) StorageLog = malloc(sizeof(slog_t)*LOG_SIZE);
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

static void RecordMyFree(void *p) {
	slog_p log_p = StorageLog;
	if (log_p) {
		for (int i=0;i<StorageLogCurrent;i++) {
			if (!log_p[i].freed && log_p[i].storage_p == p) {
				log_p[i].freed = TRUE;
			}
		}
	}
}

#define SLOG_FMT "0x%.12" PRIxPTR

EXPORT BOOL_T TestMallocs() {
	size_t oldSize;
	size_t testedMallocs = 0;
	void * old;
	slog_p log_p = StorageLog;
	BOOL_T rc = TRUE;
	if (log_p) {
		for (int i=0;i<StorageLogCurrent;i++) {
			if (log_p[i].freed) continue;
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

EXPORT void * MyMalloc(size_t size) {
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
	if (extraButtons)
		RecordMalloc(p,size);
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

EXPORT void * MyRealloc(void * old, size_t size) {
	size_t oldSize;
	void * new;
	if (old == NULL)
		return MyMalloc(size);
	totalReallocs++;
	totalRealloced += size;
	CHECKMSG( (*(unsigned long*) ((char*) old - sizeof(unsigned long)) == guard0),
		("Guard0 is hosed") );
	oldSize = *(size_t*) ((char*) old - sizeof(unsigned long) - sizeof(size_t));
	CHECKMSG( (*(unsigned long*) ((char*) old + oldSize) == guard1),
		( "Guard1 is hosed" ) );

	LOG1(log_malloc, ("  Realloc (" SLOG_FMT ",%ld) was %d\n", (size_t)old, size, oldSize ))

	if ((long) oldSize == size) {
		return old;
	}
	new = MyMalloc(size);
	memcpy(new, old, min((size_t )size, oldSize));
	MyFree(old);
	return new;
}

EXPORT void MyFree(void * ptr) {
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

EXPORT void * memdup(void * src, size_t size) {
	void * p;
	p = MyMalloc(size);
	memcpy(p, src, size);
	return p;
}

EXPORT char * MyStrdup(const char * str) {
	char * ret;
	ret = (char*) MyMalloc(strlen(str) + 1);
	strcpy(ret, str);
	return ret;
}

/*
 * Convert Text into the equivalent form that can be written to a file or put in a text box by adding escape characters
 *
 * The following special characters are produced -
 *  \n for LineFeed 	0x0A
 *  \t for Tab 			0x09
 *  "" for "  			This is so that a CSV conformant type program can interpret the file output
 *
 */
EXPORT char * ConvertToEscapedText(const char * text) {
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
EXPORT char * ConvertFromEscapedText(const char * text) {
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
	if ( sFormat == NULL )
		return "";
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
	snprintf( message, sizeof message, "%s: %s:%d %s", sCond, sFileName, iLineNumber, sMsg?sMsg:"" );
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

EXPORT char * Strcpytrimed(char * dst, const char * src, BOOL_T double_quotes) {
	const char * cp;
	while (*src && isspace((unsigned char) *src))
		src++;
	if (!*src)
		return dst;
	cp = src + strlen(src) - 1;
	while (cp > src && isspace((unsigned char) *cp))
		cp--;
	while (src <= cp) {
		if (*src == '"' && double_quotes)
			*dst++ = '"';
		*dst++ = *src++;
	}
	*dst = '\0';
	return dst;
}

static char * directory;

EXPORT wBool_t CheckHelpTopicExists(const char * topic) {

	char * htmlFile;

 	// Check the file exits in the distro

	if (!directory)
		directory = malloc(BUFSIZ);

    if (directory == NULL) return 0;

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

EXPORT char * BuildTrimedTitle(char * cp, const char * sep, const char * mfg, const char * desc,
		const char * partno) {
	cp = Strcpytrimed(cp, mfg, FALSE);
	strcpy(cp, sep);
	cp += strlen(cp);
	cp = Strcpytrimed(cp, desc, FALSE);
	strcpy(cp, sep);
	cp += strlen(cp);
	cp = Strcpytrimed(cp, partno, FALSE);
	return cp;
}

static void ShowMessageHelp(int index, const char * label, void * data) {
	char msgKey[STR_SIZE], *cp, *msgSrc;
	msgSrc = (char*) data;
	if (!msgSrc)
		return;
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

static const char * ParseMessage(const char *msgSrc) {
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
		if (messageListEmpty) {
			wMenuListDelete(messageList_ml, _(MESSAGE_LIST_EMPTY));
			messageListEmpty = FALSE;
		}
		wMenuListAdd(messageList_ml, 0, cp1, _(msgSrc));
		return cp2;
	} else {
		return _(msgSrc);
	}
}

EXPORT void InfoMessage(const char * format, ...) {
	va_list ap;
	va_start(ap, format);
	format = ParseMessage(format);
	vsnprintf(message2, 1020, format, ap);
	va_end(ap);
	/*InfoSubstituteControl( NULL, NULL );*/
	if (inError)
		return;
	SetMessage(message2);
}

EXPORT void ErrorMessage(const char * format, ...) {
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

EXPORT int NoticeMessage(const char * format, const char * yes, const char * no, ...) {
	va_list ap;
	va_start(ap, no);
	format = ParseMessage(format);
	vsnprintf(message2, 1020, format, ap);
	va_end(ap);
	return wNotice(message2, yes, no);
}

EXPORT int NoticeMessage2(int playbackRC, const char * format, const char * yes, const char * no,
		...) {
	va_list ap;
	if (inPlayback)
		return playbackRC;
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

bool
Confirm(char * label2, doSaveCallBack_p after)
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

static void ChkLoad(void * unused) {
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
static void AfterFileList(void) {
	DoFileList(0, NULL, fileListPathName);
}

static void ChkFileList(int index, const char * label, void * data) {
	fileListPathName = (char*) data;
	Confirm(_("Load"), AfterFileList);
}

/**
 * Save information about current files and some settings to preferences file.
 */

EXPORT void SaveState(void) {
	wWinPix_t width, height;
	const char * fileName;
	void * pathName;
	char file[6];
	int inx;

	wWinGetSize(mainW, &width, &height);
	wPrefSetInteger("draw", "mainwidth", (int)width);
	wPrefSetInteger("draw", "mainheight", (int)height);
	SaveParamFileList();
	ParamUpdatePrefs();

	wPrefSetString( "misc", "lastlayout", GetLayoutFullPath());
	wPrefSetInteger( "misc", "lastlayoutexample", bExample );

	if (fileList_ml) {
		strcpy(file, "file");
		file[5] = 0;
		for (inx = 0; inx < NUM_FILELIST; inx++) {
			fileName = wMenuListGet(fileList_ml, inx, &pathName);
			if (fileName) {
				file[4] = '0' + inx;
				sprintf(message, "%s", (char* )pathName);
				wPrefSetString("filelist", file, message);
			}
		}
	}
	wPrefFlush("");
}

/*
 * Clean up before quitting
 */
static void DoQuitAfter(void) {
	changed = 0;
	CleanupFiles();  //Get rid of checkpoint if we quit.
	SaveState();
}
/**
 * Process shutdown request. This function is called when the user requests
 * to close the application. Before shutting down confirmation is gotten to
 * prevent data loss.
 */
void DoQuit(void * unused) {
	if (Confirm(_("Quit"), DoQuitAfter)) {

#ifdef CHECK_UNUSED_BALLOONHELP
		ShowUnusedBalloonHelp();
#endif
		LogClose();
		wExit(0);
	}
}

static void DoClearAfter(void) {

	Reset();
	ClearTracks();

	/* set all layers to their default properties and set current layer to 0 */
	DoLayout(NULL);
	checkPtMark = changed = 0;
	DoChangeNotification( CHANGE_MAIN|CHANGE_MAP );
	bReadOnly = TRUE;
	EnableCommands();
	SetLayoutFullPath("");
	SetWindowTitle();
	LayoutBackGroundInit(TRUE);
}

static void DoClear(void * unused) {
	Confirm(_("Clear"), DoClearAfter);
}

/**
 * Toggle visibility state of map window.
 */

void MapWindowToggleShow(void * unused) {
	MapWindowShow(!mapVisible);
}

/**
 * Set visibility state of map window.
 *
 * \param state IN TRUE if visible, FALSE if hidden
 */

void MapWindowShow(int state) {
	mapVisible = state;
	wPrefSetInteger("misc", "mapVisible", mapVisible);
	wMenuToggleSet(mapShowMI, mapVisible);

	if (mapVisible) {
		DoChangeNotification(CHANGE_MAP);
	}

	wWinShow(mapW, mapVisible);
	wButtonSetBusy(mapShowB, (wBool_t) mapVisible);
}

/**
 * Set magnets state
 */
int MagneticSnap(int state)
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
void MagneticSnapToggle(void * unused) {
	MagneticSnap(!magneticSnap);
}


static void DoShowWindow(int index, const char * name, void * data) {
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

EXPORT void wShow(wWin_p win) {
	int inx;
	if (inPlayback && win != demoW) {
		wWinSetBusy(win, TRUE);
		for (inx = 0; inx < demoWindows_da.cnt; inx++)
			if ( demoWindows(inx) == win)
				break;
		if (inx >= demoWindows_da.cnt) {
			for (inx = 0; inx < demoWindows_da.cnt; inx++)
				if ( demoWindows(inx) == NULL)
					break;
			if (inx >= demoWindows_da.cnt) {
				DYNARR_APPEND(wWin_p, demoWindows_da, 10);
				inx = demoWindows_da.cnt - 1;
			}
			demoWindows(inx) = win;
		}
	}
	if (win != mainW)
		wMenuListAdd(winList_mi, -1, wWinGetTitle(win), win);
	wWinShow(win, TRUE);
}

EXPORT void wHide(wWin_p win) {
	int inx;
	wWinShow(win, FALSE);
	wWinSetBusy(win, FALSE);
	if (inMainW && win == aboutW)
		return;
	wMenuListDelete(winList_mi, wWinGetTitle(win));
	ParamResetInvalid( win );
	if (inPlayback)
		for (inx = 0; inx < demoWindows_da.cnt; inx++)
			if ( demoWindows(inx) == win)
				demoWindows(inx) = NULL;
}

EXPORT void CloseDemoWindows(void) {
	int inx;
	for (inx = 0; inx < demoWindows_da.cnt; inx++)
		if ( demoWindows(inx) != NULL)
			wHide(demoWindows(inx));
	demoWindows_da.cnt = 0;
}

EXPORT void DefaultProc(wWin_p win, winProcEvent e, void * data) {
	switch (e) {
	case wClose_e:
		wMenuListDelete(winList_mi, wWinGetTitle(win));
		if (data != NULL)
			ConfirmReset( FALSE);
		wWinDoCancel(win);
		break;
	default:
		break;
	}
}

static void NextWindow(void) {
}

EXPORT void SelectFont(void * unused) {
	wSelectFont(_("XTrackCAD Font"));
}

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
		if (menuPLs[inx].control == NULL)
			continue;
		if ((menuPLs[inx].option & IC_SELECTED) && selectedTrackCount <= 0)
			enable = FALSE;
		else if ((programMode == MODE_TRAIN
				&& (menuPLs[inx].option & (IC_MODETRAIN_TOO | IC_MODETRAIN_ONLY))
						== 0)
				|| (programMode != MODE_TRAIN
						&& (menuPLs[inx].option & IC_MODETRAIN_ONLY) != 0))
			enable = FALSE;
		else
			enable = TRUE;
		wMenuPushEnable((wMenuPush_p) menuPLs[inx].control, enable);
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

static wIndex_t autosave_count = 0;

EXPORT void TryCheckPoint() {
	if (checkPtInterval > 0
				&& changed >= checkPtMark + (wIndex_t) checkPtInterval
				&& !inPlayback) {
			DoCheckPoint();
			checkPtMark = changed;

			autosave_count++;

			if ((autosaveChkPoints>0) && (autosave_count>=autosaveChkPoints)) {
				if ( bReadOnly || *(GetLayoutFilename()) == '\0') {
					SetAutoSave();
				} else
					DoSave(NULL);
				InfoMessage(_("File AutoSaved"));
				autosave_count = 0;
			}
		}
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

static void DoCommandBIndirect(void * cmdInxP) {
	wIndex_t cmdInx;
	cmdInx = *(wIndex_t*) cmdInxP;
	DoCommandB(I2VP(cmdInx));
}

EXPORT void LayoutSetPos(wIndex_t inx) {
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
			if (action != DoCommandB && menuPLs[inx].valueP == I2VP(action)) {
				context = &menuPLs[inx];
				action = ParamMenuPush;
				menuPLs[inx].context = I2VP(buttonCnt);
				menuPLs[inx].option |= IC_PLAYBACK_PUSH;
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
typedef struct {
	char * label;
	wMenu_p menu;
} menuTrace_t, *menuTrace_p;
static dynArr_t menuTrace_da;
#define menuTrace(N) DYNARR_N( menuTrace_t, menuTrace_da, N )

static void DoMenuTrace(wMenu_p menu, const char * label, void * data) {
	/*printf( "MENUTRACE: %s/%s\n", (char*)data, label );*/
	if (recordF) {
		fprintf(recordF, "MOUSE 1 %0.3f %0.3f\n", oldMarker.x, oldMarker.y);
		fprintf(recordF, "MENU %0.3f %0.3f \"%s\" \"%s\"\n", oldMarker.x,
				oldMarker.y, (char*) data, label);
	}
}

EXPORT wMenu_p MenuRegister(const char * label) {
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

void MenuPlayback(char * line) {
	char * menuName, *itemName;
	coOrd pos;
	menuTrace_p mt;

	if (!GetArgs(line, "pqq", &pos, &menuName, &itemName))
		return;
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
/*--------------------------------------------------------------------*/

static wWin_p stickyW;

static void StickyOk(void * unused);
static paramData_t stickyPLs[] = { { PD_TOGGLE, &stickySet, "set", 0,
		stickyLabels } };
static paramGroup_t stickyPG = { "sticky", PGO_RECORD, stickyPLs,
		COUNT( stickyPLs ) };

static void StickyOk(void * unused) {
	wHide(stickyW);
}

static void DoSticky(void * unused) {
	if (!stickyW)
		stickyW = ParamCreateDialog(&stickyPG,
				MakeWindowTitle(_("Sticky Commands")), _("Ok"), StickyOk, wHide,
				TRUE, NULL, 0, NULL);
	ParamLoadControls(&stickyPG);
	wShow(stickyW);
}
/*--------------------------------------------------------------------*/

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
NULL };
static long AllToolbarMasks[] = { 1 << BG_FILE, 1<< BG_PRINT, 1<< BG_EXPORTIMPORT, 
        1<< BG_ZOOM, 1<< BG_UNDO, 1<< BG_EASE, 1 << BG_SNAP, 1 << BG_TRKCRT, 
	    1<< BG_CONTROL, 1<< BG_TRKMOD, 1 << BG_SELECT, 1 << BG_TRKGRP, 1 << BG_TRAIN, 
	    1<< BG_MISCCRT, 1<< BG_RULER, 1 << BG_LAYER, 1 << BG_HOTBAR };

static wMenuToggle_p AllToolbarMI[ COUNT( AllToolbarMasks ) ];

static void ToolbarAction(void * data) {
	int inx = (int)VP2L(data);
	CHECK( inx >=0 && inx < COUNT( AllToolbarMasks ) );
	wBool_t set = wMenuToggleGet( AllToolbarMI[inx] );
	long mask = AllToolbarMasks[inx];
	if (set)
		toolbarSet |= mask;
	else
		toolbarSet &= ~mask;
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

static void CreateToolbarM(wMenu_p toolbarM) {
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

/*--------------------------------------------------------------------*/

static wWin_p addElevW;
#define addElevF (wFloat_p)addElevPD.control
EXPORT DIST_T addElevValueV;
static void DoAddElev(void * unused);

static paramFloatRange_t rn1000_1000 = { -1000.0, 1000.0 };
static paramData_t addElevPLs[] = { { PD_FLOAT, &addElevValueV, "value",
		PDO_NOPREF|PDO_DIM, &rn1000_1000, NULL, 0 } };
static paramGroup_t addElevPG = { "addElev", 0, addElevPLs, COUNT( addElevPLs ) };

static void DoAddElev(void * unused) {
	ParamLoadData(&addElevPG);
	AddElevations(addElevValueV);
	wHide(addElevW);
}

static void ShowAddElevations(void * unused) {
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

/*--------------------------------------------------------------------*/

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
		{ PD_STRING, &trackIndex, "select",	PDO_NOPREF|PDO_NORECORD|PDO_STRINGLIMITLENGTH, I2VP(STR_SIZE-1), N_("Indexes:"), 0, 0, sizeof(trackIndex) } };
static paramGroup_t indexPG = { "index", 0, indexPLs, COUNT( indexPLs ) };

static paramFloatRange_t r_1000_1000 = { -1000.0, 1000.0, 80 };
static void MoveEnterOk(void * unused);
static paramData_t movePLs[] = {
		{ PD_FLOAT, &moveValue.x, "moveX", PDO_NOPREF|PDO_DIM|PDO_NORECORD, &r_1000_1000, N_("Move X:") },
		{ PD_FLOAT, &moveValue.y, "moveY", PDO_NOPREF|PDO_DIM|PDO_NORECORD, &r_1000_1000, N_("Move Y:") } };
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

static void MoveEnterOk(void * unused) {
	ParamLoadData(&movePG);
	moveDialogCallBack(&moveValue);
	wHide(moveW);
}

static void IndexEnterOk(void * unused) {
	ParamLoadData(&indexPG);
	indexDialogCallBack(trackIndex);
	wHide(indexW);
}

static void RotateEnterOk(void * unused) {
	ParamLoadData(&rotatePG);
	if (angleSystem == ANGLE_POLAR)
		rotateDialogCallBack(I2VP(rotateValue * 1000));
	else
		rotateDialogCallBack(I2VP(rotateValue * 1000));
	wHide(rotateW);
}

static void RotateDialogInit(void) {
	ParamRegister(&rotatePG);
}

static void MoveDialogInit(void) {
	ParamRegister(&movePG);
}

static void IndexDialogInit(void) {
	ParamRegister(&indexPG);
}

EXPORT void AddMoveMenu(wMenu_p m, moveDialogCallBack_t func) {
	wMenuPushCreate(m, "", _("Enter Move ..."), 0,
			StartMoveDialog, func);
}

EXPORT void AddIndexMenu(wMenu_p m, indexDialogCallBack_t func) {
	wMenuPushCreate(m, "cmdSelectIndex", _("Select Track Index ..."), 0,
			StartIndexDialog, func);
}

//All values multipled by 100 to support decimal points from PD_FLOAT
EXPORT void AddRotateMenu(wMenu_p m, rotateDialogCallBack_t func) {
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
 * INITIALIZATON
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

static void DebugOk(void * unused) {
	for (int i = 0; i<debugCnt;i++) {
			logTable(debug_index[i]).level = debug_values[i];
	}
	wHide(debugW);
}

static void CreateDebugW(void) {
	debugPG.paramCnt = debugCnt+1;
	ParamRegister(&debugPG);
	debugW = ParamCreateDialog(&debugPG, MakeWindowTitle(_("Debug")), _("Ok"),
			DebugOk, wHide, FALSE, NULL, 0, NULL);
	wHide(debugW);
}

EXPORT void DebugInit(void * unused) {

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


EXPORT void InitDebug(const char * label, long * valueP) {
	CHECK( debugCnt+1 < COUNT( debugPLs ) );
	memset(&debugPLs[debugCnt+1], 0, sizeof debugPLs[debugCnt]);
	debugPLs[debugCnt+1].type = PD_LONG;
	debugPLs[debugCnt+1].valueP = valueP;
	debugPLs[debugCnt+1].nameStr = label;
	debugPLs[debugCnt+1].winData = &r0_100;
	debugPLs[debugCnt+1].winLabel = label;
	debugCnt++;
}

void RecomputeElevations(void * unused );

static void MiscMenuItemCreate(wMenu_p m1, wMenu_p m2, const char * name,
		const char * label, long acclKey, void * func, long option, void * context) {
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

	if (name)
		GetBalloonHelpStr(name);
	menuPG.paramCnt++;
}


/*****************************************************************************
 *
 * ACCEL KEY
 *
 */

enum eAccelAction_t { EA_ZOOMUP, EA_ZOOMDOWN, EA_REDRAW, EA_DELETE, EA_UNDO, EA_COPY, EA_PASTE, EA_CUT, EA_NEXT, EA_HELP };
struct accelKey_s {
	const char * sPrefName;
	wAccelKey_e eKey;
	int iMode;
	enum eAccelAction_t iAction;
	int iContext; } aAccelKeys[] = {
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
		{ "help-context", wAccelKey_F1, 0, EA_HELP, 3 } };

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
		"F6", "F7", "F8", "F9", "F10", "F11", "F12", "NumpadAdd", "NumpadSub" };

static void SetAccelKeys()
{
	for ( int iAccelKey = 0; iAccelKey < COUNT( aAccelKeys ); iAccelKey++ )
	{
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


/*****************************************************************************
 *
 * MENUS
 *
 */


#include "bitmaps/zoom-in.xpm"
#include "bitmaps/zoom-choose.xpm"
#include "bitmaps/zoom-out.xpm"
#include "bitmaps/zoom-extent.xpm"
#include "bitmaps/undo.xpm"
#include "bitmaps/redo.xpm"
// #include "bitmaps/partlist.xpm" // unused
#include "bitmaps/doc-export.xpm"
#include "bitmaps/doc-export-bmap.xpm"
#include "bitmaps/doc-export-dxf.xpm"
 #include "bitmaps/doc-export-svg.xpm"
#include "bitmaps/doc-import.xpm"
#include "bitmaps/doc-import-mod.xpm" 
#include "bitmaps/doc-new.xpm"
#include "bitmaps/doc-save.xpm"
#include "bitmaps/doc-open.xpm"
// #include "bitmaps/doc-print.xpm"
#include "bitmaps/doc-setup.xpm"
#include "bitmaps/parameter.xpm"
#include "bitmaps/map.xpm"
#include "bitmaps/magnet.xpm"

//static wMenu_p toolbarM;
static addButtonCallBack_t paramFilesCallback;

static void CreateMenus(void) {
	wMenu_p fileM, editM, viewM, optionM, windowM, macroM, helpM, toolbarM,
			messageListM, manageM, addM, changeM, drawM;
	wMenu_p zoomM, zoomSubM;

	wMenuPush_p zoomInM, zoomOutM, zoomExtentsM;

	wPrefGetInteger("DialogItem", "pref-iconsize", (long *) &iconSize, 0);

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
	MiscMenuItemCreate(popup1M, popup2M, "cmdMagneticSnap", _(" Enable Magnetic Snap"), 0,
			MagneticSnapToggle, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdMapShow", _("Show/Hide Map"), 0,
				MapWindowToggleShow, 0, NULL);
	MiscMenuItemCreate(popup1M, popup2M, "cmdBackgroundShow", _("Show/Hide Background"), 0,
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
	AddToolbarButton("menuFile-clear", wIconCreatePixMap(doc_new_xpm[iconSize]),
		IC_MODETRAIN_TOO, DoClear, NULL);
	AddToolbarButton("menuFile-load", wIconCreatePixMap(doc_open_xpm[iconSize]),
		IC_MODETRAIN_TOO, ChkLoad, NULL);
	AddToolbarButton("menuFile-save", wIconCreatePixMap(doc_save_xpm[iconSize]),
		IC_MODETRAIN_TOO, DoSave, NULL);

	cmdGroup = BG_PRINT;
	MiscMenuItemCreate(fileM, NULL, "printSetup", _("P&rint Setup ..."),
			ACCL_PRINTSETUP, (wMenuCallBack_p) wPrintSetup, 0,
			I2VP(0));
	printCmdInx = InitCmdPrint(fileM);
	AddToolbarButton("menuFile-setup", wIconCreatePixMap(doc_setup_xpm[iconSize]),
		IC_MODETRAIN_TOO, (wMenuCallBack_p) wPrintSetup, I2VP(0));

	wMenuSeparatorCreate(fileM);
	MiscMenuItemCreate(fileM, NULL, "cmdImport", _("&Import"), ACCL_IMPORT,
			DoImport, 0, I2VP(0));
	MiscMenuItemCreate(fileM, NULL, "cmdImportModule", _("Import &Module"), ACCL_IMPORT_MOD,
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

	AddToolbarButton("menuFile-parameter", wIconCreatePixMap(parameter_xpm[iconSize]),
		IC_MODETRAIN_TOO, paramFilesCallback, NULL); 

	cmdGroup = BG_ZOOM;
	zoomUpB = AddToolbarButton("cmdZoomIn", wIconCreatePixMap(zoom_in_xpm[iconSize]),
		IC_MODETRAIN_TOO, DoZoomUp, NULL);
	zoomM = wMenuPopupCreate(mainW, "");
	AddToolbarButton("cmdZoom", wIconCreatePixMap(zoom_choose_xpm[iconSize]), IC_MODETRAIN_TOO,
		(wButtonCallBack_p) wMenuPopupShow, zoomM);
	zoomDownB = AddToolbarButton("cmdZoomOut", wIconCreatePixMap(zoom_out_xpm[iconSize]),
		IC_MODETRAIN_TOO, DoZoomDown, NULL);
    zoomExtentsB = AddToolbarButton("cmdZoomExtent", wIconCreatePixMap(zoom_extent_xpm[iconSize]), 
        IC_MODETRAIN_TOO, DoZoomExtents, NULL);

	cmdGroup = BG_UNDO;
	undoB = AddToolbarButton("cmdUndo", wIconCreatePixMap(undo_xpm[iconSize]), 0,
		UndoUndo, NULL);
	redoB = AddToolbarButton("cmdRedo", wIconCreatePixMap(redo_xpm[iconSize]), 0,
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
	MiscMenuItemCreate( editM, NULL, "cmdSelectAll", _("Select &All"), ACCL_SELECTALL, (wMenuCallBack_p)SetAllTrackSelect, 0, I2VP(TRUE) );
	MiscMenuItemCreate( editM, NULL, "cmdSelectCurrentLayer", _("Select Current Layer"), ACCL_SETCURLAYER, SelectCurrentLayer, 0, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdSelectByIndex", _("Select By Index"), 0L, StartIndexDialog, 0, &SelectByIndex );
	MiscMenuItemCreate( editM, NULL, "cmdDeselectAll", _("&Deselect All"), ACCL_DESELECTALL, (wMenuCallBack_p)SetAllTrackSelect, 0, I2VP(FALSE) );
	MiscMenuItemCreate( editM, NULL,  "cmdSelectInvert", _("&Invert Selection"), 0L, InvertTrackSelect, 0, NULL);
	MiscMenuItemCreate( editM, NULL,  "cmdSelectOrphaned", _("Select Stranded Track"), 0L, OrphanedTrackSelect, 0, NULL);
	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdTunnel", _("Tu&nnel"), ACCL_TUNNEL, SelectTunnel, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdBridge", _("B&ridge"), ACCL_BRIDGE, SelectBridge, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdRoadbed", _("&Roadbed"), ACCL_ROADBED, SelectRoadbed, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdTies", _("Ties/NoTies"), ACCL_TIES, SelectTies, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdAbove", _("Move to &Front"), ACCL_ABOVE, SelectAbove, IC_SELECTED, NULL);
	MiscMenuItemCreate( editM, NULL, "cmdBelow", _("Move to &Back"), ACCL_BELOW, SelectBelow, IC_SELECTED, NULL);

	wMenuSeparatorCreate( editM );
	MiscMenuItemCreate( editM, NULL, "cmdWidth0", _("Thin Tracks"), ACCL_THIN, SelectTrackWidth, IC_SELECTED, I2VP(0) );
	MiscMenuItemCreate( editM, NULL, "cmdWidth2", _("Medium Tracks"), ACCL_MEDIUM, SelectTrackWidth, IC_SELECTED, I2VP(2) );
	MiscMenuItemCreate( editM, NULL, "cmdWidth3", _("Thick Tracks"), ACCL_THICK, SelectTrackWidth, IC_SELECTED, I2VP(3) );

	/*
	 * VIEW MENU
	 */

	zoomInM = wMenuPushCreate(viewM, "menuEdit-zoomIn", _("Zoom &In"),
			ACCL_ZOOMIN, DoZoomUp, I2VP(1));
	zoomSubM = wMenuMenuCreate(viewM, "menuEdit-zoomTo", _("&Zoom"));
	zoomOutM = wMenuPushCreate(viewM, "menuEdit-zoomOut", _("Zoom &Out"),
			ACCL_ZOOMOUT, DoZoomDown, I2VP(1));
    zoomExtentsM = wMenuPushCreate(viewM, "menuEdit-zoomExtents", _("Zoom &Extents"),
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
	gridCmdInx = InitGrid(viewM);

	// visibility toggle for anchors
	// get the start value
	long anchors_long;
	wPrefGetInteger("misc", "anchors", &anchors_long, 1);
	magneticSnap = anchors_long ? TRUE : FALSE;
	magnetsMI = wMenuToggleCreate(viewM, "cmdMagneticSnap", _("Enable Magnetic Snap"),
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
	magnetsB = AddToolbarButton("cmdMagneticSnap", wIconCreatePixMap(magnet_xpm[iconSize]),
				IC_MODETRAIN_TOO, MagneticSnapToggle, NULL);
		wControlLinkedSet((wControl_p) magnetsMI, (wControl_p) magnetsB);
		wButtonSetBusy(magnetsB, (wBool_t) magneticSnap);

	mapShowB = AddToolbarButton("cmdMapShow", wIconCreatePixMap(map_xpm[iconSize]),
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
	ButtonGroupBegin( _("Control Element"), "cmdControlElements", _("Control Element") );
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
	messageListM = wMenuMenuCreate(helpM, "menuHelpRecentMessages",
			_("Recent Messages"));
	messageList_ml = wMenuListCreate(messageListM, "messageListM", 10,
			ShowMessageHelp);
	wMenuListAdd(messageList_ml, 0, _(MESSAGE_LIST_EMPTY), NULL);

	/* tip of the day */
	wMenuSeparatorCreate( helpM );
	wMenuPushCreate( helpM, "cmdTip", _("Tip of the Day..."), 0, ShowTip, I2VP(SHOWTIP_FORCESHOW | SHOWTIP_NEXTTIP));
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

	SetAccelKeys();

	InitBenchDialog();
	wPrefGetInteger( "DialogItem", "sticky-set", &stickySet, stickySet );
}

static void LoadFileList(void) {
	char file[6];
	int inx;
	const char * cp;
	const char *fileName, *pathName;
	strcpy(file, "fileX");
	for (inx = NUM_FILELIST - 1; inx >= 0; inx--) {
		file[4] = '0' + inx;
		cp = wPrefGetString("filelist", file);
		if (!cp)
			continue;
		pathName = MyStrdup(cp);
		fileName = FindFilename((char *) pathName);
		if (fileName)
			wMenuListAdd(fileList_ml, 0, fileName, pathName);
	}
}

//EXPORT void InitCmdEnumerate(void) {
//	AddToolbarButton("cmdEnumerate", wIconCreatePixMap(partlist_xpm),
//			IC_SELECTED | IC_ACCLKEY, EnumerateTracks,
//			NULL);
//}

EXPORT void InitCmdExport(void) {
	ButtonGroupBegin( _("Import/Export"), "cmdExportImportSetCmd", _("Import/Export") );
	cmdGroup = BG_EXPORTIMPORT;
	AddToolbarButton("cmdExport", wIconCreatePixMap(doc_export_xpm[iconSize]),
		IC_SELECTED | IC_ACCLKEY, DoExport, NULL); 
	AddToolbarButton("cmdExportDXF", wIconCreatePixMap(doc_export_dxf_xpm[iconSize]), 
		IC_SELECTED | IC_ACCLKEY, DoExportDXF, I2VP(1)); 
	AddToolbarButton("cmdExportBmap", wIconCreatePixMap(doc_export_bmap_xpm[iconSize]), IC_ACCLKEY,
		OutputBitMapInit(), NULL);
#if XTRKCAD_CREATE_SVG
	AddToolbarButton("cmdExportSVG", wIconCreatePixMap(doc_export_svg_xpm[iconSize]), 
		IC_ACCLKEY, DoExportSVG, NULL); // IC_SELECTED | 
#endif
	AddToolbarButton("cmdImport", wIconCreatePixMap(doc_import_xpm[iconSize]), IC_ACCLKEY,
			DoImport, I2VP(0));
	AddToolbarButton("cmdImportModule", wIconCreatePixMap(doc_import_mod_xpm[iconSize]), IC_ACCLKEY,
			DoImport, I2VP(1));
	ButtonGroupEnd();
}

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
	if (ret == 1)
		printf(_("Reload Checkpoint Selected\n"));
	else if (ret == -1)
		printf(_("Reload Checkpoint With New Name Selected\n"));
	else
		printf(_("Ignore Checkpoint Selected\n"));
	if (ret>=0) {
		/* load the checkpoint file */
		LoadCheckpoint(ret==1);
		ret = TRUE;

	}
	return (ret>=0);
}

EXPORT wWin_p wMain(int argc, char * argv[]) {
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
	if (optind < argc)
		initialFile = strdup(argv[optind]);

	extraButtons = (getenv(sEnvExtra) != NULL);
	LogOpen(logFileName);
	log_init = LogFindIndex("init");
	log_malloc = LogFindIndex("malloc");
	log_error = LogFindIndex("error");
	log_command = LogFindIndex("command");

	LOG1(log_init, ( "initCustom\n" ))
	InitCustom();

	/*
	 * MAIN WINDOW
	 */
	LOG1(log_init, ( "create main window\n" ))
	SetLayoutTitle(sProdName);
	sprintf(message, _("Unnamed Trackplan - %s(%s)"), sProdName, sVersion);
	wSetBalloonHelp(balloonHelp);

	wGetDisplaySize(&displayWidth, &displayHeight);
	mainW = wWinMainCreate(buffer, (displayWidth * 2) / 3,
			(displayHeight * 2) / 3, "xtrkcadW", message, "main",
			F_RESIZE | F_MENUBAR | F_NOTAB | F_RECALLPOS | F_RECALLSIZE | F_HIDE, MainProc,
			NULL);
	if (mainW == NULL)
		return NULL;

	InitAppDefaults();
	drawColorBlack  = wDrawFindColor( wRGB(  0,  0,  0) );
	drawColorWhite  = wDrawFindColor( wRGB(255,255,255) );
	drawColorRed    = wDrawFindColor( wRGB(255,  0,  0) );
	drawColorBlue   = wDrawFindColor( wRGB(  0,  0,255) );
	drawColorGreen  = wDrawFindColor( wRGB(  0,255,  0) );
	drawColorAqua   = wDrawFindColor( wRGB(  0,255,255) );

	// Last component of spectial color must be > 3
	drawColorPreviewSelected = wDrawFindColor( wRGB ( 6, 6, 255) );   //Special Blue
	drawColorPreviewUnselected = wDrawFindColor( wRGB( 255, 215, 6)); //Special Yellow

	drawColorPowderedBlue = wDrawFindColor( wRGB(129, 212, 250) );
	drawColorPurple = wDrawFindColor( wRGB(255,  0,255) );
	drawColorGold   = wDrawFindColor( wRGB(255,215,  0) );
	drawColorGrey10  = wDrawFindColor( wRGB(26,26,26) );
	drawColorGrey20  = wDrawFindColor( wRGB(51,51,51) );
	drawColorGrey30  = wDrawFindColor( wRGB(72,72,72) );
	drawColorGrey40  = wDrawFindColor( wRGB(102,102,102) );
	drawColorGrey50  = wDrawFindColor( wRGB(128,128,128) );
	drawColorGrey60  = wDrawFindColor( wRGB(153,153,153) );
	drawColorGrey70  = wDrawFindColor( wRGB(179,179,179) );
	drawColorGrey80  = wDrawFindColor( wRGB(204,204,204) );
	drawColorGrey90  = wDrawFindColor( wRGB(230,230,230) );
	snapGridColor = drawColorGreen;
	markerColor = drawColorRed;
	borderColor = drawColorBlack;
	crossMajorColor = drawColorRed;
	crossMinorColor = drawColorBlue;
	selectedColor = drawColorRed;
	normalColor = drawColorBlack;
	elevColorIgnore = drawColorBlue;
	elevColorDefined = drawColorGold;
	profilePathColor = drawColorPurple;
	exceptionColor = wDrawFindColor(wRGB(255, 89, 0 ));
	tieColor = wDrawFindColor(wRGB(153, 89, 68));

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
	LOG1(log_init, ( "initInfoBar\n" ))
	InitInfoBar();
	wSetSplashInfo("Misc2 Init...");
	LOG1(log_init, ( "misc2Init\n" ))
	Misc2Init();

	RotateDialogInit();
	MoveDialogInit();
	IndexDialogInit();

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

	LOG1(log_init, ( "initialize\n" ))
	if (!Initialize())
		return NULL;
	ParamRegister(&menuPG);
	ParamRegister(&stickyPG);

	/* initialize the layers */
	DefaultLayerProperties();
	LOG1(log_init, ( "loadFileList\n" ))
	LoadFileList();
	AddPlaybackProc("MENU", MenuPlayback, NULL);
	//CreateDebugW();

	/*
	 * TIDY UP
	 */
	curCommand = 0;
	commandContext = commandList[curCommand].context;

	/*
	 * READ PARAMETERS
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
	wSetSplashInfo(_("Reading parameter files"));
	LOG1(log_init, ( "paramFileInit\n" ))

	SetParamFileDir(GetCurrentPath(LAYOUTPATHKEY));  //Set default for new parms to be the same as the layout

	if (!ParamFileListInit())
		return NULL;
		// LOG1(log_init, ("!ParamFileListInit()\n"))

	curCommand = describeCmdInx;
	LOG1(log_init, ( "Reset\n" ))
	Reset();

	/*
	 * SCALE
	 */

	/* Set up the data for scale and gauge description */
	DoSetScaleDesc();

	// get the preferred scale from the configuration file
	pref = wPrefGetString("misc", "scale");
	if (!pref)
		// if preferred scale was not set (eg. during initial run), initialize to a default value
		pref = DEFAULT_SCALE;
	strcpy(buffer, pref);
	DoSetScale(buffer);

	/* see whether last layout should be reopened on startup */
	wPrefGetInteger("DialogItem", "pref-onstartup", &onStartup, 0);

	/*
	 * THE END
	 */

	LOG1(log_init, ( "the end\n" ))
	EnableCommands();
	LOG1(log_init, ( "Initialization complete\n" ))
	wSetSplashInfo(_("Initialization complete"));
	RegisterChangeNotification(ToolbarChange);
	DoChangeNotification( CHANGE_MAIN | CHANGE_MAP);

	wWinShow(mainW, TRUE);
	wWinShow(mapW, mapVisible);
	wDestroySplash();

	/* this has to be called before ShowTip() */
	InitSmallDlg();

    /* Compare the program version and display Beta warning if appropriate */
    pref = wPrefGetString("misc", "version");
    if((!pref) || (strcmp(pref,XTRKCAD_VERSION) != 0))
    {
        if(strstr(XTRKCAD_VERSION,"Beta") != NULL)
        {
            NoticeMessage(MSG_BETA_NOTICE, _("Ok"),NULL, XTRKCAD_VERSION);
        }
        //else {
        //    NoticeMessage(_("New version welcome..."),_("Ok"),NULL);
        //}
        wPrefSetString("misc", "version", XTRKCAD_VERSION);
    }
    else {
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
			DoFileList(0, "1", initialFile);   //Will load Background values, if archive, leave
			if (onStartup == 1)
				LayoutBackGroundInit(TRUE);     //Wipe Out Prior Background
			else
				LayoutBackGroundInit(FALSE);    //Get Prior BackGround
		} else
			LayoutBackGroundInit(TRUE);     // If onStartup==1 and no initial file - Wipe Out Prior Background

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
