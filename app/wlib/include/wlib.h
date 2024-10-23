/** \file wlib.h
 * Common definitions and declarations for the wlib library
 */

#ifndef HAVE_WLIB_H
#define HAVE_WLIB_H
#ifdef WINDOWS
#include <stdio.h>
#define FILE_SEP_CHAR "\\"
#include "getline.h"
#else
#define FILE_SEP_CHAR "/"
#endif

#include <stdbool.h>

#ifdef USE_SIMPLE_GETTEXT
char *bindtextdomain( char *domainname, char *dirname );
char *bind_textdomain_codeset(char *domainname, char *codeset );
char *textdomain( char *domainname );
char *gettext( const char *msgid );

//char *g_win32_getlocale (void);
#endif

// conversion routines to and from UTF-8
bool wSystemToUTF8(const char *inString, char *outString,
                   unsigned outStringLength);
bool wUTF8ToSystem(const char *inString, char *outString,
                   unsigned outStringLength);
bool wIsUTF8(const char * string);

/*
 * Interface types
 */

// a big integer
typedef long wInteger_t;
// Position/Size of objects drawn on a WDraw canvas (fractional pixels)
typedef double wDrawPix_t;
// Position/Size of controls/windows (integral pixels)
typedef long wWinPix_t;
// Boolean
typedef int wBool_t;
// index for lists etc
typedef int wIndex_t;

/*
 * Opaque Pointers
 */
typedef struct wWin_t       * wWin_p;
typedef struct wWindow_t    * wWindow_p;
typedef struct control      * wControl_p;
typedef struct wButton_t    * wButton_p;
typedef struct wEntry_t     * wEntry_p;
typedef struct wInteger_t   * wInteger_p;
typedef struct wFloat_t     * wFloat_p;
typedef struct wList_t      * wList_p;
typedef struct wChoice_t    * wChoice_p;
typedef struct wDraw_t      * wDraw_p;
//typedef struct wMenu_t      * wMenu_p;
#define wMenu_p wControl_p
typedef struct wText_t      * wText_p;
typedef struct wMessage_t   * wMessage_p;
typedef struct wLine_t      * wLine_p;
typedef struct wMenuList_t  * wMenuList_p;
typedef struct wMenuPush_t  * wMenuPush_p;
typedef struct wMenuRadio_t * wMenuRadio_p;
//typedef struct wMenuToggle_t* wMenuToggle_p;
#define wMenuToggle_p wControl_p
typedef struct wBox_t       * wBox_p;
typedef struct wIcon_t      * wIcon_p;
typedef struct wDrawBitMap_t * wDrawBitMap_p;
typedef struct wFont_t      * wFont_p;
typedef struct wBitmap_t	* wBitmap_p;
typedef struct wStatus_t    * wStatus_p;
typedef struct wColorButton_t* wColorButton_p;

typedef int wDrawWidth;
typedef unsigned long wDrawColor;

typedef struct {
	const char * name;
	const char * value;
} wBalloonHelp_t;

extern long debugWindow;
extern long wDebugFont;

/*----------------------------------------------------------------------------
 * Application main window 
 */

 /* Creation CallBacks */
typedef enum {
    wClose_e,
    wResize_e,
    wState_e,
    wQuit_e,
    wRedraw_e,
    wCancel_e,
    wAccept_e
} winProcEvent;

typedef bool (*wWinCallBack_p)(wControl_p control, 
                               winProcEvent event, 
                               void* data1, 
                               void* data2);

/* Creation Options */
#define F_AUTOSIZE	(1L<<1)
#define F_HEADER 	(1L<<2)
#define F_RESIZE 	(1L<<3)
#define F_BLOCK  	(1L<<4)
#define F_MENUBAR 	(1L<<5)
#define F_NOTAB		(1L<<8)
#define F_RECALLPOS	(1L<<9)
#define F_RECALLSIZE	(1L<<10)
#define F_TOP		(1L<<11)
#define F_CENTER	(1L<<12)
#define F_HIDE		(1L<<13)
#define F_MAXIMIZE  (1L<<14)
#define F_RESTRICT  (1L<<15)
#define F_NOTTRANSIENT (1L<<16)

wControl_p wWinMainCreate(
    const char* name,	        /* Application name */
    wWinPix_t x,		        /* Initial window width */
    wWinPix_t y,		        /* Initial window height */
    const char* helpStr,	    /* Help topic string */
    const char* labelStr,	    /* Window title */
    const char* nameStr,	    /* Window name */
    long option,		        /* Options */
    wWinCallBack_p winProc,	    /* Call back function */
    void* context );

/*----------------------------------------------------------------------------
 *
 * Bitmap Controls bitmap.c
 */

wControl_p wBitmapViewCreate(wControl_p parent, 
                         wWinPix_t x, 
                         wWinPix_t y, 
                         long options, 
                         const wIcon_p iconP);
wIcon_p wIconCreateBitMap(wWinPix_t w, 
                          wWinPix_t h, 
                          const char* bits,
                          wDrawColor color);
wIcon_p wIconCreatePixMap(char* pm[]);
void wIconSetColor(wIcon_p ip, wDrawColor color);

/*------------------------------------------------------------------------------
 *
 * Buttons, toggles and radiobuttons button.c
 *
 */

 /* Creation Options */
#define BB_DEFAULT	(1L<<5)
#define BB_CANCEL   (1L<<6)
#define BB_HELP (1L<<7)
#define BC_ICON 	(1L<<0)
#define BC_NOBORDER 	(1L<<15)
#define BC_HORIZONTAL 	(1L<<22)

/* Creation CallBacks */
typedef void (*wChoiceCallBack_p)(long, void*);

/**  Buttons */

/* Creation CallBacks */
typedef void (*wButtonCallBack_p)(void* choice);

void wButtonSetIcon(wControl_p bb, wIcon_p icon);
void wButtonSetLabel(wControl_p bb, const char* labelStr);
void wButtonSetBusy(wControl_p bb, int value);
wControl_p wButtonCreate(wControl_p parent, 
                        wWinPix_t x, 
                        wWinPix_t y,
                        const char* helpStr, 
                        const char* labelStr, 
                        long option,
                        wWinPix_t width, 
                        wButtonCallBack_p action, 
                        void* context);


wControl_p wButtonCreateForToolbar(wControl_p  w, 
                                   wWinPix_t x, 
                                   wWinPix_t y, 
                                   const char* helpStr, 
                                   wIcon_p icon, 
                                   long option, 
                                   wWinPix_t width, 
                                   wButtonCallBack_p action, 
                                   void* context);

/** Radio buttons */

void wRadioSetValue(wControl_p bc, long value);
long wRadioGetValue(wControl_p bc);
wControl_p wRadioCreate(wControl_p parent, 
                        wWinPix_t x, 
                        wWinPix_t y,
                        const char* helpStr, 
                        const char* labelStr, 
                        long option,
                        const char* const* labels, 
                        long* valueP, 
                        wChoiceCallBack_p action, 
                        void* context);

/** Toggle buttons */

void wToggleSetValue(wControl_p bc, long value);
long wToggleGetValue(wControl_p b);
wControl_p wToggleCreate(wControl_p parent,
                         wWinPix_t x, 
                         wWinPix_t y,
                         const char* helpStr, 
                         const char* labelStr, 
                         long option,
                         const char* const* labels, 
                         long* valueP,
                         wChoiceCallBack_p action, 
                         void* context);

/*------------------------------------------------------------------------------
 *
 * Color Selection
 */

 /* Creation CallBacks */
typedef void (*wColorSelectButtonCallBack_p)(void*, wDrawColor);

wControl_p wColorSelectButtonCreate( wControl_p	parent,
                                     wWinPix_t x,
                                     wWinPix_t y,
                                     const char* helpStr,
                                     const char* labelStr,
                                     long option,
                                     wWinPix_t width,
                                     wDrawColor* valueP,
                                     wColorSelectButtonCallBack_p action,
                                     void* context);
void wColorSelectButtonSetColor(wControl_p bb, wDrawColor color);
wDrawColor wColorSelectButtonGetColor(wControl_p bb);
wDrawColor wDrawColorGray(int percent);

// the following are placeholders for functions that are no longer needed
// after color palettes were removed. 

#define wDrawFindColor(rgb) \
_Pragma("message (\"Obsolete function wDrawFindColor used!\")") \
(rgb)

#define wDrawGetRGB( color ) \
_Pragma("message (\"Obsolete function wDrawGetRGB used!\")") \
(color) 

/*------------------------------------------------------------------------------
 *
 * Dialog Windows
 */

#define DO_FILESYSTEM 1

wControl_p wWinDialogCreate(wControl_p parent, 
                            const char* helpStr, 
                            const char* titleStr,
                            const char* nameStr, 
                            long option, 
                            wWinCallBack_p winProc, 
                            void* context);

void wDialogButtonsConfigure(wControl_p dialog, 
                             const char* okLabel,
                             const char* cancelLabel, 
                             const char* helpLabel);

/*------------------------------------------------------------------------------
 *
 * Drawing area
 */

typedef int wAction_t;
#define wActionMove		    (1)
#define wActionLDown		(2)
#define wActionLDrag		(3)
#define wActionLUp		    (4)
#define wActionRDown		(5)
#define wActionRDrag		(6)
#define wActionRUp		    (7)
#define wActionText		    (8)
#define wActionExtKey		(9)
#define wActionWheelUp      (10)
#define wActionWheelDown    (11)
#define wActionLDownDouble  (12)
#define wActionModKey       (13)
#define wActionScrollUp     (14)
#define wActionScrollDown   (15)
#define wActionScrollLeft   (16)
#define wActionScrollRight  (17)
#define wActionMDown        (18)
#define wActionMDrag        (19)
#define wActionMUp          (20)
#define wActionLast		    wActionMUp

/* Creation CallBacks */
typedef void (*wDrawRedrawCallBack_p)(wControl_p control,
    void* context,
    wWinPix_t posX,
    wWinPix_t posY);

typedef void (*wDrawActionCallBack_p)(wControl_p control,
    void* context,
    wAction_t action,
    wDrawPix_t posX,
    wDrawPix_t posY);

/* Creation Options */
#define BD_TICKS	 (1L<<25)
#define BD_DIRECT	 (1L<<26)
#define BD_NOCAPTURE (1L<<27)
#define BD_NOFOCUS   (1L<<28)
#define BD_MODKEYS   (1L<<29)

/* Create: */
wControl_p wDrawCreate(wControl_p parent, 
                       wWinPix_t x, 
                       wWinPix_t y,
                       const char* helpStr, 
                       long option, 
                       wWinPix_t width, 
                       wWinPix_t height,
                       void* context, 
                       wDrawRedrawCallBack_p redraw, 
                       wDrawActionCallBack_p action);

/*------------------------------------------------------------------------------
 *
 * File Selection
 */

#define FSO_MULTIPLEFILES	1
#define FSO_PICTURES        2

struct wFilSel_t;
typedef enum {
    FS_SAVE,
    FS_LOAD,
    FS_UPDATE
} wFilSelMode_e;

typedef int (*wFilSelCallBack_p)(int files,
    char** fileNames,
    void* context);

struct wFilSel_t* wFilSelCreate(wControl_p parent,
    wFilSelMode_e mode,
    int options,
    const char* title,
    const char* patternList,
    wFilSelCallBack_p action,
    void* context);

int wFilSelect(struct wFilSel_t* fs,
    const char* directoryName);

/*------------------------------------------------------------------------------
 *
 * Messages
 */

#define BM_LARGE (1L<<24)
#define BM_SMALL (1L<<25)
#define BM_ALIGNCENTER (0)
#define BM_ALIGNRIGHT (1L<<26)
#define BM_ALIGNLEFT (1L<<27)
#define COMBOBOX (1L)

#define wMessageSetFont( x ) ( x & (BM_LARGE | BM_SMALL ))

#define wMessageCreate( w, p1, p2, l, p3, m ) wMessageCreateEx( w, p1, p2, l, p3, m, 0 )

wControl_p wMessageCreateEx(wControl_p parent, 
                            wWinPix_t x, 
                            wWinPix_t y,
                            const char* labelStr, 
                            wWinPix_t width, 
                            const char* message, 
                            long flags);

void wMessageSetValue(wControl_p b, const char* arg);
void wMessageSetWidth(wControl_p b, wWinPix_t width);
void wMessageSetLength(wControl_p control, size_t length);

wWinPix_t wMessageGetWidth(const char* testString);
wWinPix_t wMessageGetHeight(long flags);

/*------------------------------------------------------------------------------
 *
 * Notice dialogs
 */

#define NT_INFORMATION 1
#define NT_WARNING	   2
#define NT_ERROR	   4

int wNotice(
    const char* msg,
    const char* yes,
    const char* no);

int wNotice3(
    const char* msg,
    const char* affirmative,
    const char* cancel,
    const char* alternate);

int wNoticeWithIcon(int type,
    const char* msg,
    const char* yes,
    const char* no);

/*----------------------------------------------------------------------------
 *
 * Splash window
 */

int wCreateSplash(char* appName, char* appVer);
int wSetSplashInfo(char* msg);
void wDestroySplash(void);

/*---------------------------------------------------------------------------- 
 *
 * String entry
 */

 /* Creation CallBacks */
typedef bool (*wEntryCallBack_p)(const char* enteredString, void* userData);

wControl_p wEntryCreate(wControl_p parent, 
                        wWinPix_t x, 
                        wWinPix_t y,
                        const char* helpStr, 
                        const char* labelStr, 
                        long option, 
                        wWinPix_t width,
                        char* valueP, 
                        wIndex_t valueL, 
                        wEntryCallBack_p action, 
                        void* context);
void wEntrySetValue(wControl_p control, 
                    const char* value);
void wEntrySetWidth(wControl_p control, 
                    wWinPix_t width);
const char* wEntryGetValue(wControl_p control);

/*------------------------------------------------------------------------------
 *
 * Text
 */

 /* Creation Options */
#define BT_HSCROLL 	    (1L<<24)
#define BT_CHARUNITS	(1L<<23)
#define BT_FIXEDFONT	(1L<<22)
#define BT_TOP		    (1L<<20)	/* Show the top of the text */

wControl_p
wTextCreate(wControl_p parent,
    wWinPix_t	x,
    wWinPix_t	y,
    const char* helpStr,
    const char* labelStr,
    long	option,
    wWinPix_t	width,
    wWinPix_t	height);

void wTextClear(wControl_p bt);
void wTextAppend(wControl_p bt, const char* text);
wBool_t wTextSave(wControl_p bt, const char* fileName);
wBool_t wTextPrint(wControl_p bt);
int wTextGetSize(wControl_p bt);
void wTextGetText(wControl_p bt, char* text, int len);
void wTextSetReadonly(wControl_p bt, wBool_t ro);
wBool_t wTextGetModified(wControl_p bt);
void wTextSetSize(wControl_p bt, wWinPix_t w, wWinPix_t h);
void wTextComputeSize(wControl_p bt, 
                      wWinPix_t rows, 
                      wWinPix_t cols,
                      wWinPix_t* width, 
                      wWinPix_t* height);
void wTextSetPosition(wControl_p bt, int pos);

/*------------------------------------------------------------------------------
 *
 * Lines
 */

typedef struct {
    int width;
    int x0, y0;
    int x1, y1;
} wLines_t, * wLines_p;

wLine_p wLineCreate(wWindow_p	parent, const char* labelStr, int	count,
    wLines_t* lines);
  



/* = *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= *= */

/*------------------------------------------------------------------------------
 *
 * Frames around widgets boxes.c
 *
 */

typedef enum {
	wBoxThinB,
	wBoxThinW,
	wBoxAbove,
	wBoxBelow,
	wBoxThickB,
	wBoxThickW,
	wBoxRidge,
	wBoxTrough
}
wBoxType_e;

void wBoxSetSize(wBox_p b, wWinPix_t w, wWinPix_t h);
void wlibDrawBox(wWin_p win, wBoxType_e style, wWinPix_t x, wWinPix_t y,
                 wWinPix_t w, wWinPix_t h);
wBox_p wBoxCreate(wWin_p parent, wWinPix_t bx, wWinPix_t by,
                  const char *labelStr, wBoxType_e boxTyp, wWinPix_t bw, wWinPix_t bh);




/*------------------------------------------------------------------------------
 *
 * System Interface
 */

void wInitAppName(char *appName);

const char * wGetAppLibDir(			void );
const char * wGetAppWorkDir(			void );
const char * wGetUserHomeDir( void );

void wSetAudio(bool setting);
void wBeep( void );

void wHelp(const char*);

unsigned wOpenFileExternal(char *filename);


void wSetBalloonHelp ( wBalloonHelp_t * );
void wEnableBalloonHelp ( int );
void wBalloonHelpUpdate ( void );

void wFlush(			void );

typedef void (*wAlarmCallBack_p)( void );
void wAlarm(			long, wAlarmCallBack_p );
void wPause( long duration );
unsigned long wGetTimer(	void );

void wExit(			int );

typedef enum {	wCursorNormal,
                wCursorNone,
                wCursorAppStart,
                wCursorHand,
                wCursorNo,
                wCursorSizeAll,
                wCursorSizeNESW,
                wCursorSizeNS,
                wCursorSizeNWSE,
                wCursorSizeWE,
                wCursorWait,
                wCursorIBeam,
                wCursorCross,
                wCursorQuestion
             } wCursor_t;
void wSetCursor( wControl_p window, wCursor_t cursor);
#define defaultCursor wCursorCross

const char * wMemStats( void );

#define WKEY_SHIFT	(1<<1)
#define WKEY_CTRL	(1<<2)
#define WKEY_ALT	(1<<3)
int wGetKeyState(		void );

void wGetDisplaySize(		wWinPix_t*, wWinPix_t* );

wIcon_p wIconCreateBitMap(	wWinPix_t, wWinPix_t, const char * bits,
                                wDrawColor );
wIcon_p wIconCreatePixMap(	char *[] );
void wIconSetColor(		wIcon_p, wDrawColor );
void wIconDraw( wDraw_p d, wIcon_p bm, wWinPix_t x, wWinPix_t y );

void wConvertToCharSet(		char *, int );
void wConvertFromCharSet(	char *, int );
#ifdef WINDOWS
FILE * wFileOpen(		const char *, const char * );
#endif

/*------------------------------------------------------------------------------
 *
 * Main and Dialog Windows
 */


wControl_p wMain(			int, char *[] );
void wWinSetBigIcon(		wWin_p, wIcon_p );
void wWinSetSmallIcon(		wWin_p, wIcon_p );
void wWinShow( wControl_p control, wBool_t visibility);
wBool_t wWinIsVisible(		wWin_p );
wBool_t wWinIsMaximized( wWin_p win);
void wWinGetSize (		wControl_p window, wWinPix_t * width, wWinPix_t *height );
void wWinSetSize(		wControl_p, wWinPix_t, wWinPix_t );
void wWinSetTitle(		wWin_p, const char * );
void wWinSetBusy(wControl_p win, wBool_t busy);
const char * wWinGetTitle(		wWin_p );
void wWinClear(			wWin_p, wWinPix_t, wWinPix_t, wWinPix_t, wWinPix_t );
void wMessage(			wWin_p, const char *, wBool_t );
void wWinTop(			wWin_p );
void wWinDoCancel(		wWin_p );
void wWinBlockEnable(		wBool_t );
void wSetGeometry(wControl_p win, wWinPix_t min_width, wWinPix_t max_width, wWinPix_t min_height, wWinPix_t max_height, wWinPix_t base_width, wWinPix_t base_height, double aspect_ratio);

void wlibRedraw(wWin_p win);

wControl_p wWindowCreate(
    wControl_p parent,
    int winType,
    wWinPix_t x,
    wWinPix_t y,
    const char* labelStr,
    const char* nameStr,
    long option,
    wWinCallBack_p winProc,
    void* context);

wBool_t wWinIsTemplated(wWin_p win);

/*------------------------------------------------------------------------------
 *
 * Controls in general
 */

/* Creation Options */
#define BO_ICON		(1L<<0)
#define BO_DISABLED	(1L<<1)
#define BO_READONLY	(1L<<2)
#define BO_ABUT     (1L<<6)
#define BO_GAP      (1L<<7)
#define BO_NOTAB	(1L<<8)
#define BO_BORDER	(1L<<9)
//#define BO_ENTER    (1L<<10)
#define BO_ENTER    0
#define BO_REPEAT   (1L<<11)
#define BO_IGNFOCUS	(1L<<12)
#define BO_DIALOGFROMBUILDER (1L<<13)

wWinPix_t wLabelWidth(		const char * );
const char * wControlGetHelp(		wControl_p );
void wControlSetHelp(		wControl_p, const char * );
void wControlShow(		wControl_p, wBool_t );
wWinPix_t wControlGetWidth(	wControl_p );
wWinPix_t wControlGetHeight(	wControl_p );
wWinPix_t wControlGetPosX(		wControl_p );
wWinPix_t wControlGetPosY(		wControl_p );
void wControlSetPos(		wControl_p, wWinPix_t, wWinPix_t );
void wControlSetFocus(		wControl_p );
void wControlActive(		wControl_p, wBool_t );
void wControlSetBalloon(	wControl_p, wWinPix_t, wWinPix_t, const char * );
void wControlSetLabel(		wControl_p, const char * );
void wControlSetBalloonText(	wControl_p, const char * );
void wControlSetContext(	wControl_p, void * );
void wControlHilite(		wControl_p, wBool_t );

void wControlLinkedSet( wControl_p b1, wControl_p b2 );
void wControlLinkedActive( wControl_p b, int active );


/*------------------------------------------------------------------------------
 *
 * Numeric Entry
 */

/* Creation CallBacks */
typedef void (*wIntegerCallBack_p)( long, void *, int);
typedef void (*wFloatCallBack_p)( double, void *, int);
wInteger_p wIntegerCreate(	wWin_p, wWinPix_t, wWinPix_t, const char *,
                                const char *, long,
                                wWinPix_t, wInteger_t, wInteger_t, wInteger_t *,
                                wIntegerCallBack_p, void * );
wFloat_p wFloatCreate(		wWin_p, wWinPix_t, wWinPix_t, const char *,
                                const char *, long,
                                wWinPix_t, double, double, double *,
                                wFloatCallBack_p, void * );
void wIntegerSetValue(		wInteger_p, wInteger_t );
void wFloatSetValue(		wFloat_p, double );
wInteger_t wIntegerGetValue(	wInteger_p );
double wFloatGetValue(		wFloat_p );


/*------------------------------------------------------------------------------
 *
 * Lists
 */

/* Creation CallBacks */
typedef void (*wListCallBack_p)( unsigned int , const char *, unsigned int, void *,
                                 void * );

/* Creation Options */
#define BL_DUP		(1L<<16)
#define BL_SORT		(1L<<17)
#define BL_MANY 	(1L<<18)
#define BL_NONE 	(1L<<19)
#define BL_SETSTAY 	(1L<<20)
#define BL_DBLCLICK 	(1L<<21)
#define BL_FIXFONT 	(1L<<22)
#define BL_EDITABLE	(1L<<23)
#define BL_ICON		(1L<<0)


/* lists, droplists and combo boxes */
wControl_p wListCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
    const char* helpStr, const char* labelStr, long option, long number,
    wWinPix_t width, int colCnt, wWinPix_t* colWidths, wBool_t* colRightJust,
    const char** colTitles, long* valueP, wListCallBack_p action, void* attributes);

wControl_p wComboBoxCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
    const char* helpStr, const char* labelStr, long option, long	number,
    wWinPix_t width, long* valueP, wListCallBack_p action, void* attributes);

wControl_p wComboBoxCreateForToolbar(wControl_p	parent, const char* helpStr, const char* labelStr,
    long	option, wWinPix_t	width, long* valueP, wListCallBack_p action,
    void* context);

void wComboBoxAddValue(wControl_p b, char* text, void * attributes);
void wComboBoxSetIndex(wControl_p b, int row);
wIndex_t wComboBoxGetCount(wControl_p b);
void* wComboBoxGetItemContext(wControl_p b, wIndex_t inx);
wBool_t wComboBoxSetValues( wControl_p b, wIndex_t row, const char* labelStr, wIcon_p bm,
    void* itemData); 

wControl_p wComboListCreate(wControl_p parent, wWinPix_t x, wWinPix_t y,
                         const char *helpStr, const char *labelStr, long option, long number,
                         wWinPix_t width, long *valueP, wListCallBack_p action, void *attributes);
void wListClear(wControl_p b);
void wListSetIndex(wControl_p b, int element);
wIndex_t wListFindValue(wControl_p b, const char* val);
wIndex_t wListGetCount(wControl_p b);

wIndex_t wListGetIndex(	wControl_p b );
void* wListGetItemContext(wControl_p b, wIndex_t inx);
wBool_t wListGetItemSelected( wControl_p b, wIndex_t inx);
wIndex_t wListGetSelectedCount( wControl_p b);
void wListSelectAll(wControl_p bl);
wBool_t wListSetValues( wControl_p b, wIndex_t row, const char* labelStr,
    wIcon_p bm, void* itemData);
void wListDelete(wControl_p b, wIndex_t inx);
int wListGetColumnWidths(wControl_p bl, int colCnt, wWinPix_t* colWidths);
wIndex_t wListAddValue(wControl_p b, const char* labelStr, wIcon_p bm,
    void* itemData);
void wListSetSize(wControl_p bl, wWinPix_t w, wWinPix_t h);
wIndex_t wListGetValues(wControl_p bl, char* labelStr, int labelSize,
    void** listDataRet, void** itemDataRet);


void wListSetValue(wControl_p bl, const char* val);
void wListSetActive(		wList_p, wIndex_t, wBool_t );
void wListSetEditable(		wList_p, wBool_t );

/*------------------------------------------------------------------------------
 *
 * Draw
 */


typedef int wDrawOpts;
#define wDrawOptTemp	(1<<0)
#define wDrawOptNoClip	(1<<1)
#define wDrawOptTransparent  (1<<2)
#define wDrawOutlineFont (1<<3)
#ifdef CURSOR_SURFACE
#define wDrawOptCursor  (1<<4)
#define wDrawOptCursorClr (1<<5)
#define wDrawOptCursorClr (1<<6)
#define wDrawOptCursorRmv (1<<7)
#define wDrawOptCursorQuit (1<<8)
#define wDrawOptOpaque   (1<<9)
#endif

#define EXPORTBITMAP (1)

typedef enum {
	wDrawLineSolid,
	wDrawLineDash,
	wDrawLineDot,
	wDrawLineDashDot,
	wDrawLineDashDotDot,
	wDrawLineCenter,
	wDrawLinePhantom
}
wDrawLineType_e;

typedef enum {
	wPolyLineStraight,
	wPolyLineSmooth,
	wPolyLineRound
}
wPolyLine_e;


#define wRGB(R,G,B)\
	(long)(((((long)(R)<<16))&0xFF0000L)|((((long)(G))<<8)&0x00FF00L)|(((long)(B))&0x0000FFL))


/* Draw: */
void wDrawLine(
    wControl_p drawingArea,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawPix_t x1, wDrawPix_t y1,
    wDrawWidth width,
    wDrawLineType_e lineType,
    wDrawColor color,
    wDrawOpts opts);

#define double2wAngle_t( A )	(A)
typedef double wAngle_t;
void wDrawArc(
    wControl_p drawingArea,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawPix_t r,
    wAngle_t angle0,
    wAngle_t angle1,
    int drawCenter,
    wDrawWidth width,
    wDrawLineType_e lineType,
    wDrawColor color,
    wDrawOpts opts);

void wDrawPoint(
    wControl_p drawingArea,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawColor color,
    wDrawOpts opts);

#define double2wFontSize_t( FS )	(FS)
typedef double wFontSize_t;
void wDrawString(
    wControl_p drawingArea,
    wDrawPix_t x, wDrawPix_t y,
    wAngle_t a,
    const char* s,
    wFont_p fp,
    wFontSize_t fs,
    wDrawColor color,
    wDrawOpts opts);

void wDrawFilledRectangle(
    wControl_p drawingArea,
    wDrawPix_t x,
    wDrawPix_t y,
    wDrawPix_t w,
    wDrawPix_t h,
    wDrawColor color,
    wDrawOpts opt);

void wDrawPolygon(
    wControl_p bd,
    wDrawPix_t p[][2],
    wPolyLine_e type[],
    int cnt,
    wDrawColor color,
    wDrawWidth dw,
    wDrawLineType_e lt,
    wDrawOpts opt,
    int fill,
    int open);

void wDrawFilledCircle(
    wControl_p bd,
    wDrawPix_t x0,
    wDrawPix_t y0,
    wDrawPix_t r,
    wDrawColor color,
    wDrawOpts opt);

void wDrawGetTextSize(
    wDrawPix_t* w,
    wDrawPix_t* h,
    wDrawPix_t* d,
    wDrawPix_t* a,
    wControl_p drawingArea,
    const char* s,
    wFont_p fp,
    wFontSize_t fs);

void wDrawClear(
    wControl_p bd);

void wDrawClearTemp(wControl_p drawingArea);
wBool_t wDrawSetTempMode( wControl_p bd, wBool_t bTemp);

void wDrawDelayUpdate(		wControl_p, wBool_t );
void wDrawClip(wControl_p drawingArea,
    wDrawPix_t x,
    wDrawPix_t y,
    wDrawPix_t w,
    wDrawPix_t h);

/* Geometry */
double wDrawGetDPI(	wControl_p drawingArea );
double wDrawGetMaxRadius( wControl_p drawingArea);
void wDrawSetSize(
    wControl_p drawingArea,
    wWinPix_t w,
    wWinPix_t h, void* redraw);
void wDrawGetSize(
    wControl_p drawingArea,
    wWinPix_t* w,
    wWinPix_t* h);

/* Bitmaps */
wDrawBitMap_p wDrawBitMapCreate(
    wControl_p drawingArea,
    int w,
    int h,
    int x,
    int y,
    const unsigned char* fbits);
void wDrawBitMap(
    wControl_p bd,
    wDrawBitMap_p bm,
    wDrawPix_t x, wDrawPix_t y,
    wDrawColor color,
    wDrawOpts opts);

wDraw_p wBitmapCreate(		wWinPix_t, wWinPix_t, int );
wBool_t wBitmapDelete(		wDraw_p );
wBool_t wBitmapWriteFile(	wDraw_p, const char * );

/* Misc */
void* wDrawGetContext(
    wControl_p drawingArea);
void wDrawSaveImage(		wDraw_p );
void wDrawRestoreImage(		wDraw_p );
int wDrawSetBackground(wControl_p bd, char* path, char** error);
void wDrawCloneBackground(wDraw_p from, wDraw_p to);
void wDrawShowBackground(wControl_p drawingArea, wWinPix_t pos_x, wWinPix_t pos_y,
    wWinPix_t size, wAngle_t angle, int screen);

/*------------------------------------------------------------------------------
 *
 * Fonts
 */
void wInitializeFonts();
void wSelectFont(		const char * );
wFontSize_t wSelectedFontSize(	void );
void wSetSelectedFontSize(wFontSize_t size);
#define F_TIMES	(1)
#define F_HELV	(2)
wFont_p wStandardFont(		int, wBool_t, wBool_t );


/*------------------------------------------------------------------------------
 *
 * Printing
 */

typedef void (*wPrintSetupCallBack_p)( wBool_t );

wBool_t wPrintInit(		void );
void wPrintSetup(		wPrintSetupCallBack_p );
void wPrintGetMargins(		double *, double *, double *, double * );
void wPrintGetPageSize(		double *, double * );
wBool_t wPrintDocStart(		const char *, int, int * );
wDraw_p wPrintPageStart(	void );
wBool_t wPrintPageEnd(		wDraw_p );
void wPrintDocEnd(		void );
wBool_t wPrintQuit(		void );
void wPrintClip(		wDrawPix_t, wDrawPix_t, wDrawPix_t, wDrawPix_t );
const char * wPrintGetName(	void );


/*------------------------------------------------------------------------------
 *
 * Menus
 */
#define WACCL_BASE	(1000)
#define WALT		(1<<10)
#define WCTL		(1<<11)
#define WMETA		(1<<12)
#define WSHIFT		(1<<13)

typedef enum {
	wAccelKey_None,
	wAccelKey_Del,
	wAccelKey_Ins,
	wAccelKey_Home,
	wAccelKey_End,
	wAccelKey_Pgup,
	wAccelKey_Pgdn,
	wAccelKey_Up,
	wAccelKey_Down,
	wAccelKey_Right,
	wAccelKey_Left,
	wAccelKey_Back,
	wAccelKey_F1,
	wAccelKey_F2,
	wAccelKey_F3,
	wAccelKey_F4,
	wAccelKey_F5,
	wAccelKey_F6,
	wAccelKey_F7,
	wAccelKey_F8,
	wAccelKey_F9,
	wAccelKey_F10,
	wAccelKey_F11,
	wAccelKey_F12,
	wAccelKey_Numpad_Add,
	wAccelKey_Numpad_Subtract,
	wAccelKey_LineFeed
}
wAccelKey_e;

typedef enum {
	wModKey_None,
	wModKey_Alt,
	wModKey_Shift,
	wModKey_Ctrl
}
wModKey_e;

void wDoAccelHelp( wAccelKey_e key, void * );

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * Menu creation
 * 
 */

/* Creation CallBacks */
typedef void (*wMenuCallBack_p)( void * );
typedef void (*wMenuListCallBack_p)( int index, const char *label, const void * attributes);
typedef void (*wMenuCallBack_p)( void * );
typedef void (*wAccelKeyCallBack_p)( wAccelKey_e, void * );
typedef void (*wMenuTraceCallBack_p)( wMenu_p, const char *, void * );

/* Creation Options */
#define BM_ICON		(1L<<0)

wControl_p wMenuBarAdd(
    wControl_p w,
    const char* helpStr,
    const char* labelStr);

wControl_p wMenuPushCreate(
    wControl_p m,
    const char* helpStr,
    const char* labelStr,
    long acclKey,
    wMenuCallBack_p action,
    void* attributes);

wControl_p wMenuRadioCreate(
    wControl_p m,
    const char* helpStr,
    const char* labelStr,
    long acclKey,
    wMenuCallBack_p action,
    void* attributes);

wControl_p wMenuMenuCreate(
    wControl_p m,
    const char* helpStr,
    const char* labelStr);

wControl_p wMenuPopupCreate(
    wControl_p parent,
    const char* labelStr);

void wMenuSeparatorCreate(
    wControl_p m);

void wMenuRadioSetActive( wControl_p mi);

void wMenuPushEnable(
    wControl_p mi,
    wBool_t enable);

wControl_p wMenuListCreate(wControl_p m, const char* helpStr, int max,
    wMenuListCallBack_p action);

void wMenuListAdd(wControl_p ml, int index, const char* labelStr, const void* attributes);
const char* wMenuListGet(wControl_p ml, unsigned int index, void** attributes);
void wMenuListClear(wControl_p ml);
void wMenuListDelete(wControl_p ml, const char* labelStr);
const char* wMenuListGet(wControl_p ml, unsigned int index, void** attributes);


wControl_p wMenuToggleCreate(
    wControl_p m,
    const char* helpStr,
    const char* labelStr,
    long acclKey,
    wBool_t set,
    wMenuCallBack_p action,
    void* attributes);

wBool_t wMenuToggleSet(
    wControl_p mt,
    wBool_t set);

wBool_t wMenuToggleGet(
    wControl_p mt);

void wMenuToggleEnable(
    wControl_p mt,
    wBool_t enable);

void wMenuPopupShow(wControl_p mp);

void wMenuAddHelp(		wMenu_p );


wMenu_p wMenuCreate(wWin_p, wWinPix_t, wWinPix_t, const char*, const char*,
    long);

void wMenuSetTraceCallBack(
    wControl_p m,
    wMenuTraceCallBack_p func,
    void* attributes); 

wBool_t wMenuAction( wControl_p control, const char * label);

void wAttachAccelKey( wAccelKey_e, int, wAccelKeyCallBack_p, void * );

/*------------------------------------------------------------------------------
 *
 * Preferences
 */

void wPrefSetString(const char *, const char *, const char * );
char * wPrefGetString(const char *section, const char *name);
char * wPrefGetStringBasic( const char *section, const char *name );
char * wPrefGetStringExt(const char *section, const char *name);

void wPrefsLoad(char * name);

void wPrefSetInteger(const char *, const char *, long );
wBool_t wPrefGetInteger(const char *section, const char *name, long *result,
                        long defaultValue);
wBool_t wPrefGetIntegerBasic(const char *section, const char *name,
                             long *result, long defaultValue);
wBool_t wPrefGetIntegerExt(const char *section, const char *name, long *result,
                           long defaultValue);

void wPrefSetFloat(		const char *, const char *, double );
wBool_t wPrefGetFloat(const char *section, const char *name, double *result,
                      double defaultValue);
wBool_t wPrefGetFloatBasic(const char *section, const char *name,
                           double *result, double defaultValue);
wBool_t wPrefGetFloatExt(const char *section, const char *name, double *result,
                         double defaultValue);

//const char * wPrefGetSectionItem( const char * sectionName, wIndex_t * index,
//                                  const char ** name );
void wPrefFlush( char * name);
void wPrefReset( void );
void wPrefTokenize(char* line, char** section, char** name, char** value);
void wPrefFormatLine(const char* section, const char* name,
                    const char* value, char* result);

//void CleanupCustom( void );

/*------------------------------------------------------------------------------
 *
 * Statusbar
 */

wControl_p wStatusCreate(wControl_p	parent, const char* labelStr,
    const char* message);

wWinPix_t wStatusGetWidth(const char *testString);
wWinPix_t wStatusSetRequiredHeight(wControl_p label, long flags);

void wStatusSetValue(wControl_p b, const char* arg);
void wStatusSetWidth(wControl_p b, wWinPix_t width);

void wStatusClearControls(wWin_p win);
void wStatusAttachControl(wWin_p win, wControl_p b);
void wStatusRevealControlSet(wWin_p win, char *id);

/*------------------------------------------------------------------------------
 *
 * Stack Container
 */

void wStackPageShow(wControl_p stack, const char* pageName);
wControl_p wStackCreate(wControl_p	parent, wWinPix_t	x, wWinPix_t	y, const char* helpStr,
    const char* labelStr, long 	option, wWinPix_t 	width, wButtonCallBack_p action, void* context);

/*------------------------------------------------------------------------------
 *
 * System-Information
 */

char* wGetTempPath(void);
char* wGetOSVersion(void);
char* wGetProfileFilename(void);
char* wGetUserID(void);
const char* wGetUserHomeRootDir(void);
const char *wGetPlatformVersion(void);

/*-------------------------------------------------------------------------------
 * User Preferences
 */

#define PREFSECTION "Preference"
#define LARGEICON   "LargeIcons"
#define DPISET      "ScreenDPI"
#define PRINTSCALE    "PrintScale"
#define PRINTTEXTSCALE  "PrintTextScale"
#endif
