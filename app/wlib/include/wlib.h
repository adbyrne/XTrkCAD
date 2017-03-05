/** \file wlib.h
 * Common definitions and declarations for the wlib library
 */

#ifndef WIN_H
#define WIN_H
#ifdef WINDOWS
#include <stdio.h>
#define FILE_SEP_CHAR "\\"
#else
#define FILE_SEP_CHAR "/"
#endif

#ifdef USE_SIMPLE_GETTEXT
char *bindtextdomain( char *domainname, char *dirname );
char *bind_textdomain_codeset(char *domainname, char *codeset );
char *textdomain( char *domainname );
char *gettext( const char *msgid );

char *g_win32_getlocale (void);
#endif

/*
 * Interface types
 */

typedef long wInteger_t;
typedef int wPos_t;
typedef int wBool_t;
typedef int wIndex_t;

/*
 * Opaque Pointers
 */
typedef struct wWin_t       * wWin_p;
typedef struct wControl_t   * wControl_p;
typedef struct wButton_t    * wButton_p;
typedef struct wString_t    * wString_p;
typedef struct wInteger_t   * wInteger_p;
typedef struct wFloat_t     * wFloat_p;
typedef struct wList_t      * wList_p;
typedef struct wChoice_t    * wChoice_p;
typedef struct wDraw_t      * wDraw_p;
typedef struct wMenu_t      * wMenu_p;
typedef struct wText_t      * wText_p;
typedef struct wMessage_t   * wMessage_p;
typedef struct wLine_t      * wLine_p;
typedef struct wMenuList_t  * wMenuList_p;
typedef struct wMenuPush_t  * wMenuPush_p;
typedef struct wMenuRadio_t * wMenuRadio_p;
typedef struct wMenuToggle_t* wMenuToggle_p;
typedef struct wBox_t       * wBox_p;
typedef struct wIcon_t      * wIcon_p;
typedef struct wDrawBitMap_t * wDrawBitMap_p;
typedef struct wFont_t      * wFont_p;
typedef struct wBitmap_t	* wBitmap_p;
typedef int wDrawWidth;
typedef int wDrawColor;

typedef struct {
	const char * name;
	const char * value;
	} wBalloonHelp_t;

extern long debugWindow;
extern long wDebugFont;

/*------------------------------------------------------------------------------
 *
 * Bitmap Controls bitmap.c
 */

wControl_p wBitmapCreate(wWin_p parent, wPos_t x, wPos_t y, long options, wIcon_p iconP);
wIcon_p wIconCreateBitMap(wPos_t w, wPos_t h, const char *bits, wDrawColor color);
wIcon_p wIconCreatePixMap(char *pm[]);
void wIconSetColor(wIcon_p ip, wDrawColor color);

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
	wBoxTrough }
		wBoxType_e;

void wBoxSetSize(wBox_p b, wPos_t w, wPos_t h);
void wlibDrawBox(wWin_p win, wBoxType_e style, wPos_t x, wPos_t y, wPos_t w, wPos_t h);
wBox_p wBoxCreate(wWin_p parent, wPos_t bx, wPos_t by, const char *labelStr, wBoxType_e boxTyp, wPos_t bw, wPos_t bh);

/*------------------------------------------------------------------------------
 *
 * Buttons, toggles and radiobuttons button.c
 *
 */

/* Creation CallBacks */
typedef void (*wButtonCallBack_p)( void * );

/* Creation Options */
#define BB_DEFAULT	(1L<<5)
#define BB_CANCEL	(1L<<6)
#define BB_HELP (1L<<7)

/* Creation CallBacks */
typedef void (*wChoiceCallBack_p)( long, void * );

/* Creation Options */
#define BC_ICON 	(1L<<0)
#define BC_HORZ 	(1L<<22)
#define BC_NONE 	(1L<<19)
#define BC_NOBORDER 	(1L<<15)

void wButtonSetLabel(wButton_p bb, const char *labelStr);
void wButtonSetBusy(wButton_p bb, int value);
wButton_p wButtonCreate(wWin_p parent, wPos_t x, wPos_t y, const char *helpStr, const char *labelStr, long option, wPos_t width, wButtonCallBack_p action, void *data);
void wRadioSetValue(wChoice_p bc, long value);
long wRadioGetValue(wChoice_p bc);
void wToggleSetValue(wChoice_p bc, long value);
long wToggleGetValue(wChoice_p b);
wChoice_p wRadioCreate(wWin_p parent, wPos_t x, wPos_t y, const char *helpStr, const char *labelStr, long option, const char **labels, long *valueP, wChoiceCallBack_p action, void *data);
wChoice_p wToggleCreate(wWin_p parent, wPos_t x, wPos_t y, const char *helpStr, const char *labelStr, long option, const char **labels, long *valueP, wChoiceCallBack_p action, void *data);


/*------------------------------------------------------------------------------
 *
 * System Interface
 */

void wInitAppName(char *appName);

const char * wGetAppLibDir(			void );
const char * wGetAppWorkDir(			void );
const char * wGetUserHomeDir( void );
wBool_t wCheckExecutable(		void );

void wBeep(			void );
wBool_t wNotice(		const char *, const char *, const char * );
int wNotice3(			const char *, const char *, const char *, const char * );
void wHelp(			const char * );

#define NT_INFORMATION 1
#define NT_WARNING	   2
#define NT_ERROR	   4

wBool_t wNoticeEx( int type, const char * msg, const char * yes, const char * no );



void wSetBalloonHelp ( wBalloonHelp_t * );
void wEnableBalloonHelp ( int );
void wBalloonHelpUpdate ( void );

void wFlush(			void );

typedef void (*wAlarmCallBack_p)( void );
void wAlarm(			long, wAlarmCallBack_p );
void wPause(			long );
unsigned long wGetTimer(	void );

void wExit(			int );

typedef enum {	wCursorNormal,
		wCursorWait,
		wCursorIBeam,
		wCursorCross,
		wCursorQuestion } wCursor_t;
void wSetCursor( wCursor_t );

const char * wMemStats( void );

#define WKEY_SHIFT	(1<<1)
#define WKEY_CTRL	(1<<2)
#define WKEY_ALT	(1<<3)
int wGetKeyState(		void );

void wGetDisplaySize(		wPos_t*, wPos_t* );

wIcon_p wIconCreateBitMap(	wPos_t, wPos_t, const char * bits, wDrawColor );
wIcon_p wIconCreatePixMap(	char *[] );
void wIconSetColor(		wIcon_p, wDrawColor );
void wIconDraw( wDraw_p d, wIcon_p bm, wPos_t x, wPos_t y );

void wConvertToCharSet(		char *, int );
void wConvertFromCharSet(	char *, int );
#ifdef WINDOWS
FILE * wFileOpen(		const char *, const char * );
#endif

/*------------------------------------------------------------------------------
 *
 * Main and Popup Windows
 */

/* Creation CallBacks */
typedef enum {
	wClose_e,
	wResize_e,
	wQuit_e,
	wRedraw_e }
		winProcEvent;
typedef void (*wWinCallBack_p)( wWin_p, winProcEvent, void * );

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
#define F_HIDE		(1L<<12)

wWin_p wWinMainCreate(	        const char *, wPos_t, wPos_t, const char *, const char *, const char *,
				long, wWinCallBack_p, void * );
wWin_p wWinPopupCreate(		wWin_p, wPos_t, wPos_t, const char *, const char *, const char *,
				long, wWinCallBack_p, void * );

wWin_p wMain(			int, char *[] );
void wWinSetBigIcon(		wWin_p, wIcon_p );
void wWinSetSmallIcon(		wWin_p, wIcon_p );
void wWinShow(			wWin_p, wBool_t );
wBool_t wWinIsVisible(		wWin_p );
void wWinGetSize (		wWin_p, wPos_t *, wPos_t * );
void wWinSetSize(		wWin_p, wPos_t, wPos_t );
void wWinSetTitle(		wWin_p, const char * );
void wWinSetBusy(		wWin_p, wBool_t );
const char * wWinGetTitle(		wWin_p );
void wWinClear(			wWin_p, wPos_t, wPos_t, wPos_t, wPos_t );
void wMessage(			wWin_p, const char *, wBool_t );
void wWinTop(			wWin_p );
void wWinDoCancel(		wWin_p );
void wWinBlockEnable(		wBool_t );

int wCreateSplash( char *appName, char *appVer );
int wSetSplashInfo( char *msg );
void wDestroySplash( void );

/*------------------------------------------------------------------------------
 *
 * Controls in general
 */

/* Creation Options */
#define BO_ICON		(1L<<0)
#define BO_DISABLED	(1L<<1)
#define BO_READONLY	(1L<<2)
#define BO_NOTAB	(1L<<8)
#define BO_BORDER	(1L<<9)

wPos_t wLabelWidth(		const char * );
const char * wControlGetHelp(		wControl_p );
void wControlSetHelp(		wControl_p, const char * );
void wControlShow(		wControl_p, wBool_t );
wPos_t wControlGetWidth(	wControl_p );
wPos_t wControlGetHeight(	wControl_p );
wPos_t wControlGetPosX(		wControl_p );
wPos_t wControlGetPosY(		wControl_p );
void wControlSetPos(		wControl_p, wPos_t, wPos_t );
void wControlSetFocus(		wControl_p );
void wControlActive(		wControl_p, wBool_t );
void wControlSetBalloon(	wControl_p, wPos_t, wPos_t, const char * );
void wControlSetLabel(		wControl_p, const char * );
void wControlSetBalloonText(	wControl_p, const char * );
void wControlSetContext(	wControl_p, void * );
void wControlHilite(		wControl_p, wBool_t );

void wControlLinkedSet( wControl_p b1, wControl_p b2 );
void wControlLinkedActive( wControl_p b, int active );

/*------------------------------------------------------------------------------
 *
 * String entry
 */

#define BS_TRIM			(1<<12)
/* Creation CallBacks */
typedef void (*wStringCallBack_p)( const char *, void * );
wString_p wStringCreate(	wWin_p, wPos_t, wPos_t, const char *, const char *, long,
				wPos_t, char *, wIndex_t, wStringCallBack_p,
				void * );
void wStringSetValue(		wString_p, const char * );
void wStringSetWidth(		wString_p, wPos_t );
const char * wStringGetValue(		wString_p );


/*------------------------------------------------------------------------------
 *
 * Numeric Entry
 */

/* Creation CallBacks */
typedef void (*wIntegerCallBack_p)( long, void * );
typedef void (*wFloatCallBack_p)( double, void * );
wInteger_p wIntegerCreate(	wWin_p, wPos_t, wPos_t, const char *, const char *, long,
				wPos_t, wInteger_t, wInteger_t, wInteger_t *,
				wIntegerCallBack_p, void * );
wFloat_p wFloatCreate(		wWin_p, wPos_t, wPos_t, const char *, const char *, long,
				wPos_t, double, double, double *,
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
typedef void (*wListCallBack_p)( wIndex_t, const char *, wIndex_t, void *, void * );

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

wList_p wListCreate(		wWin_p, wPos_t, wPos_t, const char *, const char *, long,
				long, wPos_t, int, wPos_t *, wBool_t *, const char **, long *, wListCallBack_p, void * );
wList_p wDropListCreate(	wWin_p, wPos_t, wPos_t, const char *, const char *, long,
				long, wPos_t, long *, wListCallBack_p, void * );
				
wList_p wComboListCreate(wWin_p parent, wPos_t x, wPos_t y, const char *helpStr, const char *labelStr, long option, long number, wPos_t width, long *valueP, wListCallBack_p action, void *data);	
void wListClear(wList_p b);
void wListSetIndex(wList_p b, int element);
wIndex_t wListFindValue(wList_p b, const char *val);
wIndex_t wListGetCount(wList_p b);
wIndex_t wListGetIndex(		wList_p );
void *wListGetItemContext(wList_p b, wIndex_t inx);
wBool_t wListGetItemSelected(wList_p b, wIndex_t inx);
wIndex_t wListGetSelectedCount(wList_p b);
void wListSelectAll(wList_p bl);
wBool_t wListSetValues(wList_p b, wIndex_t row, const char *labelStr, wIcon_p bm, void *itemData);
void wListDelete(wList_p b, wIndex_t inx);
int wListGetColumnWidths(wList_p bl, int colCnt, wPos_t *colWidths);
wIndex_t wListAddValue(wList_p b, const char *labelStr, wIcon_p bm, void *itemData);
void wListSetSize(wList_p bl, wPos_t w, wPos_t h);
wIndex_t wListGetValues(	wList_p, char *, int, void * *, void * * );

/** \todo Check for the existance of following functions */
void  wListSetValue(		wList_p, const char * );
void wListSetActive(		wList_p, wIndex_t, wBool_t );
void wListSetEditable(		wList_p, wBool_t );


/*------------------------------------------------------------------------------
 *
 * Messages
 */

#define BM_LARGE (1L<<24)
#define BM_SMALL (1L<<25)

#define wMessageSetFont( x ) ( x & (BM_LARGE | BM_SMALL ))

#define wMessageCreate( w, p1, p2, l, p3, m ) wMessageCreateEx( w, p1, p2, l, p3, m, 0 )
wMessage_p wMessageCreateEx(	wWin_p, wPos_t, wPos_t, const char *,
				wPos_t, const char *, long );

void wMessageSetValue(		wMessage_p, const char * );
void wMessageSetWidth(		wMessage_p, wPos_t );
wPos_t wMessageGetHeight( long );


/*------------------------------------------------------------------------------
 *
 * Lines
 */

typedef struct {
	int width;
	int x0, y0;
	int x1, y1;
	} wLines_t, * wLines_p;

wLine_p wLineCreate(		wWin_p, const char *, int, wLines_t *);


/*------------------------------------------------------------------------------
 *
 * Text
 */

/* Creation Options */
#define BT_HSCROLL 	(1L<<24)
#define BT_CHARUNITS	(1L<<23)
#define BT_FIXEDFONT	(1L<<22)
#define BT_DOBOLD	(1L<<21)

wText_p wTextCreate(		wWin_p, wPos_t, wPos_t, const char *, const char *, long,
				wPos_t, wPos_t );
void wTextClear(		wText_p );
void wTextAppend(		wText_p, const char * );
void wTextSetReadonly(		wText_p, wBool_t );
int wTextGetSize(		wText_p );
void wTextGetText(		wText_p, char *, int );
wBool_t wTextGetModified(	wText_p );
void wTextReadFile(		wText_p, const char * );
wBool_t wTextSave(		wText_p, const char * );
wBool_t wTextPrint(		wText_p );
void wTextSetSize(		wText_p, wPos_t, wPos_t );
void wTextComputeSize(		wText_p, int, int, wPos_t *, wPos_t * );
void wTextSetPosition(		wText_p bt, int pos );


/*------------------------------------------------------------------------------
 *
 * Draw
 */


typedef int wDrawOpts;
#define wDrawOptTemp	(1<<0)
#define wDrawOptNoClip	(1<<1)

typedef enum {
	wDrawLineSolid,
	wDrawLineDash }
		wDrawLineType_e;

typedef int wAction_t;
#define wActionMove		(1)
#define wActionLDown		(2)
#define wActionLDrag		(3)
#define wActionLUp		(4)
#define wActionRDown		(5)
#define wActionRDrag		(6)
#define wActionRUp		(7)
#define wActionText		(8)
#define wActionExtKey		(9)
#define wActionWheelUp (10)
#define wActionWheelDown (11)
#define wActionLast		wActionWheelDown


#define wRGB(R,G,B)\
	(long)(((((long)(R)<<16))&0xFF0000L)|((((long)(G))<<8)&0x00FF00L)|(((long)(B))&0x0000FFL))


/* Creation CallBacks */
typedef void (*wDrawRedrawCallBack_p)( wDraw_p, void *, wPos_t, wPos_t );
typedef void (*wDrawActionCallBack_p)(	wDraw_p, void*, wAction_t, wPos_t, wPos_t );

/* Creation Options */
#define BD_TICKS	(1L<<25)
#define BD_DIRECT	(1L<<26)
#define BD_NOCAPTURE	(1L<<27)

/* Create: */
wDraw_p wDrawCreate(		wWin_p, wPos_t, wPos_t, const char *, long,
				wPos_t, wPos_t, void *,
				wDrawRedrawCallBack_p, wDrawActionCallBack_p );

/* Draw: */
void wDrawLine(			wDraw_p, wPos_t, wPos_t, wPos_t, wPos_t,
				wDrawWidth, wDrawLineType_e, wDrawColor,
				wDrawOpts );
#define double2wAngle_t( A )	(A)
typedef double wAngle_t;
void wDrawArc(			wDraw_p, wPos_t, wPos_t, wPos_t, wAngle_t, wAngle_t,
				int, wDrawWidth, wDrawLineType_e, wDrawColor,
				wDrawOpts );
void wDrawPoint(		wDraw_p, wPos_t, wPos_t, wDrawColor, wDrawOpts );
#define double2wFontSize_t( FS )	(FS)
typedef double wFontSize_t;
void wDrawString(		wDraw_p, wPos_t, wPos_t, wAngle_t, const char *, wFont_p,
		  		wFontSize_t, wDrawColor, wDrawOpts );
void wDrawFilledRectangle(	wDraw_p, wPos_t, wPos_t, wPos_t, wPos_t,
				wDrawColor, wDrawOpts );
void wDrawFilledPolygon(	wDraw_p, wPos_t [][2], wIndex_t, wDrawColor,
				wDrawOpts );
void wDrawFilledCircle(		wDraw_p, wPos_t, wPos_t, wPos_t, wDrawColor, wDrawOpts );

void wDrawGetTextSize(		wPos_t *, wPos_t *, wPos_t *, wDraw_p, const char *, wFont_p,
				wFontSize_t );
void wDrawClear(		wDraw_p );

void wDrawDelayUpdate(		wDraw_p, wBool_t );
void wDrawClip(			wDraw_p, wPos_t, wPos_t, wPos_t, wPos_t );
wDrawColor wDrawColorGray(	int );
wDrawColor wDrawFindColor(	long );
long wDrawGetRGB(		wDrawColor );

/* Geometry */
double wDrawGetDPI(		wDraw_p );
double wDrawGetMaxRadius(	wDraw_p );
void wDrawSetSize(		wDraw_p, wPos_t, wPos_t );
void wDrawGetSize(		wDraw_p, wPos_t *, wPos_t * );

/* Bitmaps */
wDrawBitMap_p wDrawBitMapCreate( wDraw_p, int, int, int, int, const char * );
void wDrawBitMap(		wDraw_p, wDrawBitMap_p, wPos_t, wPos_t,
				wDrawColor, wDrawOpts );

wDraw_p wBitMapCreate(		wPos_t, wPos_t, int );
wBool_t wBitMapDelete(		wDraw_p );
wBool_t wBitMapWriteFile(	wDraw_p, const char * );

/* Misc */
void * wDrawGetContext(		wDraw_p );
void wDrawSaveImage(		wDraw_p );
void wDrawRestoreImage(		wDraw_p );

/*------------------------------------------------------------------------------
 *
 * Fonts
 */
void wInitializeFonts();
void wSelectFont(		const char * );
wFontSize_t wSelectedFontSize(	void );
void wSetSelectionFontSize(int);
#define F_TIMES	(1)
#define F_HELV	(2)
wFont_p wStandardFont(		int, wBool_t, wBool_t );


/*------------------------------------------------------------------------------
 *
 * Printing
 */

typedef void (*wAddPrinterCallBack_p)( const char *, const char * );
typedef void (*wAddMarginCallBack_p)( const char *, double, double, double, double );
typedef void (*wAddFontAliasCallBack_p)( const char *, const char * );
typedef void (*wPrintSetupCallBack_p)( wBool_t );

wBool_t wPrintInit(		void );
void wPrintSetup(		wPrintSetupCallBack_p );
void wPrintSetCallBacks(	wAddPrinterCallBack_p, wAddMarginCallBack_p, wAddFontAliasCallBack_p );
void wPrintGetPageSize(		double *, double * );
void wPrintGetPhysSize(		double *, double * );
wBool_t wPrintDocStart(		const char *, int, int * );
wDraw_p wPrintPageStart(	void );
wBool_t wPrintPageEnd(		wDraw_p );
void wPrintDocEnd(		void );
wBool_t wPrintQuit(		void );
void wPrintClip(		wPos_t, wPos_t, wPos_t, wPos_t );


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
	wAccelKey_F12 }
	wAccelKey_e;

/* Creation CallBacks */
typedef void (*wMenuCallBack_p)( void * );
typedef void (*wMenuListCallBack_p)( int, const char *, void * );
typedef void (*wMenuToggleCallBack_p)( wBool_t , void * );
typedef void (*wAccelKeyCallBack_p)( wAccelKey_e, void * );
typedef void (*wMenuTraceCallBack_p)( wMenu_p, const char *, void * );

/* Creation Options */
#define BM_ICON		(1L<<0)

wMenu_p wMenuCreate(		wWin_p, wPos_t, wPos_t, const char *, const char *, long );
wMenu_p wMenuBarAdd(		wWin_p, const char *, const char * );

wMenuPush_p wMenuPushCreate(	wMenu_p, const char *, const char *, long,
				wMenuCallBack_p, void * );
wMenuRadio_p wMenuRadioCreate(	wMenu_p, const char *, const char *, long,
				wMenuCallBack_p, void * );

wMenu_p wMenuMenuCreate(	wMenu_p, const char *, const char * );
wMenu_p wMenuPopupCreate(	wWin_p, const char * );
void wMenuSeparatorCreate(	wMenu_p );
wMenuList_p wMenuListCreate(	wMenu_p, const char *, int, wMenuListCallBack_p );
void wMenuRadioSetActive( wMenuRadio_p );
void wMenuPushEnable(		wMenuPush_p, wBool_t );
void wMenuListAdd(		wMenuList_p, int, const char *, const void * );
void wMenuListDelete(		wMenuList_p, const char * );
const char * wMenuListGet(	wMenuList_p, int, void ** );
void wMenuListClear(		wMenuList_p );

wMenuToggle_p wMenuToggleCreate(	wMenu_p, const char *, const char *, long, wBool_t, wMenuToggleCallBack_p, void * );
wBool_t wMenuToggleSet(		wMenuToggle_p, wBool_t );
wBool_t wMenuToggleGet(		wMenuToggle_p );
void wMenuToggleEnable(		wMenuToggle_p, wBool_t );

void wMenuPopupShow(		wMenu_p );

void wMenuAddHelp(		wMenu_p );

void wMenuSetTraceCallBack(	wMenu_p, wMenuTraceCallBack_p, void * );
wBool_t wMenuAction(		wMenu_p, const char * );

void wAttachAccelKey( wAccelKey_e, int, wAccelKeyCallBack_p, void * );

/*------------------------------------------------------------------------------
 *
 * File Selection
 */

#define FS_MULTIPLEFILES	1

struct wFilSel_t;
typedef enum {
	FS_SAVE,
	FS_LOAD,
	FS_UPDATE }
		wFilSelMode_e;
typedef int (*wFilSelCallBack_p)( int files, char ** fileName, void * );
struct wFilSel_t * wFilSelCreate(wWin_p, wFilSelMode_e, int, const char *, const char *,
				wFilSelCallBack_p, void * );
int wFilSelect(			struct wFilSel_t *, const char * );


/*------------------------------------------------------------------------------
 *
 * Color Selection
 */
/* Creation CallBacks */
typedef void (*wColorSelectButtonCallBack_p)( void *, wDrawColor );

wBool_t wColorSelect( const char *, wDrawColor * );
wButton_p wColorSelectButtonCreate( wWin_p, wPos_t, wPos_t, const char *, const char *,
        long, wPos_t, wDrawColor *, wColorSelectButtonCallBack_p, void * );
void wColorSelectButtonSetColor( wButton_p, wDrawColor );
wDrawColor wColorSelectButtonGetColor( wButton_p );

/*------------------------------------------------------------------------------
 *
 * Preferences
 */

void wPrefSetString(		const char *, const char *, const char * );
const char * wPrefGetString(		const char *, const char * );
void wPrefSetInteger(		const char *, const char *, long );
wBool_t wPrefGetInteger(	const char *, const char *, long *, long );
void wPrefSetFloat(		const char *, const char *, double );
wBool_t wPrefGetFloat(		const char *, const char *, double *, double );
const char * wPrefGetSectionItem( const char * sectionName, wIndex_t * index, const char ** name );
void wPrefFlush(		void );
void wPrefReset(		void );

void CleanupCustom( void );

#endif
