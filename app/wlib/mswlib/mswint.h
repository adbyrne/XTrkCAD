#include "wlib.h"
#include "mswlib.h"
#include "dynarr.h"

#include <FreeImage.h>
#include <stdio.h>

#ifdef WIN32
#ifdef FAR
#undef FAR
#endif
#define FAR
#define _export
#define MoveTo(HDC,X,Y) MoveToEx(HDC,X,Y,NULL)
#define READ	(0)
#define WRITE	(1)
#define XEXPORT
#define XWNDPROC		WNDPROC
#define WCMD_PARAM_HWND	lParam
#define WCMD_PARAM_NOTF	HIWORD(wParam)
#define WCMD_PARAM_ID	LOWORD(wParam)
#define WSCROLL_PARAM_CODE	LOWORD(wParam)
#define WSCROLL_PARAM_NPOS	HIWORD(wParam)
#define WSCROLL_PARAM_HWND	lParam
#else
#define XEXPORT			_export
#define XWNDPROC		FARPROC
#define WCMD_PARAM_HWND	LOWORD(lParam)
#define WCMD_PARAM_NOTF	HIWORD(lParam)
#define WCMD_PARAM_ID	wParam
#define WSCROLL_PARAM_CODE	wParam
#define WSCROLL_PARAM_NPOS	LOWORD(lParam)
#define WSCROLL_PARAM_HWND	HIWORD(lParam)
#endif

#ifndef CAST_AWAY_CONST
	#define CAST_AWAY_CONST (char *)
#endif

#define BOOL_T wBool_t
#define INDEX_T wIndex_t
#define INTEGER_T wInteger_t

typedef enum {
		W_MAIN, W_POPUP,
		B_BUTTON, B_STRING, B_INTEGER, B_FLOAT,
		B_LIST, B_DROPLIST, B_COMBOLIST,
		B_RADIO, B_TOGGLE,
		B_DRAW, B_TEXT, B_MESSAGE, B_LINES,
		B_MENUITEM, B_CHOICEITEM, B_BOX, 
		B_BITMAP } wType_e;

typedef void (*repaintProcCallback_p)( HWND, wControl_p );
typedef void (*doneProcCallback_p)( wControl_p b );
typedef LRESULT (*messageCallback_p)( wControl_p, HWND, UINT, WPARAM, LPARAM );
typedef void (*setTriggerCallback_p)( wControl_p b );
typedef void (*setBusyCallback_p)( wControl_p, BOOL_T );
typedef void (*showCallback_p)( wControl_p, BOOL_T );
typedef void (*setPosCallback_p)( wControl_p, wWinPix_t, wWinPix_t );

typedef struct {
		repaintProcCallback_p	repaintProc;
		doneProcCallback_p		doneProc;
		messageCallback_p		messageProc;
		setBusyCallback_p		setBusyProc;
		showCallback_p			showProc;
		setPosCallback_p		setPosProc;
		} callBacks_t;

#define CALLBACK_CNT (B_BOX+1)
extern callBacks_t *mswCallBacks[CALLBACK_CNT];


#define WOBJ_COMMON \
		wType_e type; \
		wControl_p next; \
		wControl_p synonym; \
		wWin_p parent; \
		wWinPix_t x, y; \
		wWinPix_t w, h; \
		long option; \
		wWinPix_t labelX, labelY; \
		const char * labelStr; \
		const char * helpStr; \
		const char * tipStr; \
		char * errStr; \
		HWND hWnd; \
		void * data;\
		wControl_p focusChainNext; \
		wBool_t shown; \
		wBool_t hilite;

struct wControl_t { 
		WOBJ_COMMON
		};

typedef struct {
		unsigned key;
		wDrawColor color;
		} wIconColorMap_t;
#define mswIcon_bitmap (1)
#define mswIcon_pixmap (2)

struct wIcon_t {
		int type;
		wWinPix_t w;				/**< width */
		wWinPix_t h;				/**< height */
		wDrawColor color;
		int colorcnt;			/**< number of colors */
		RGBQUAD *colormap;
		char *pixels;			/**< pointer to pixel information */
		int transparent;		/**< index of transparent color */
		};

struct wDraw_t {
		WOBJ_COMMON
		HDC hDc;
		double wFactor;
		double hFactor;
		double DPI;
		wDrawRedrawCallBack_p drawRepaint;
		wDrawActionCallBack_p action;
		HBITMAP hBmMain;
		HBITMAP hBmTemp;
		HBITMAP hBmOld;
		HPEN hPen;
		HBRUSH hBrush;
		wDraw_p drawNext;
		wBool_t hasPalette;
		int paletteClock;
		HBITMAP hBmBackup;
		HDC hDcBackup;
		HBITMAP hBmBackupOld;
		FIBITMAP *background;
		wBool_t bTempMode;
		wBool_t bCopiedMain;

		wDrawPix_t lastX;
		wDrawPix_t lastY;

		};

extern HINSTANCE mswHInst;
extern char mswTmpBuff[1024];
extern HWND mswHWnd;
extern const char *mswDrawWindowClassName;
char *mswProfileFile;
extern int mswEditHeight;
extern int mswAllowBalloonHelp;
extern HFONT mswOldTextFont;
extern HFONT mswLabelFont;
extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;
extern long mswThickFont;
extern double mswScale;
extern double scaleIcon;

DWORD mswGetBaseStyle( wWin_p );
char * mswStrdup( const char * );
HBITMAP mswCreateBitMap( COLORREF, COLORREF, COLORREF, int, int, const char * );
void mswFail( const char * );
void mswResize( wWin_p );
wControl_p mswMapIndex( INDEX_T );
void mswButtPush( wControl_p );
void * mswAlloc( wWin_p, wType_e, const char *, int, void *, int * );
void mswComputePos( wControl_p, wWinPix_t, wWinPix_t );
void mswAddButton( wControl_p, BOOL_T, const char * );
void mswRepaintLabel( HWND, wControl_p );
int mswRegister( wControl_p );
void mswUnregister( int );
void mswChainFocus( wControl_p );
void mswSetFocus( wControl_p );
void mswSetTrigger( wControl_p, setTriggerCallback_p );
void mswMenuPush( wControl_p );
void mswCreateCheckBitmaps( void );
LRESULT FAR PASCAL XEXPORT mswDrawPush( HWND, UINT, WPARAM, LPARAM );
#ifdef WIN32
DWORD GetTextExtent( HDC, CHAR *, UINT );
#endif
void mswRedrawAll( void );
void mswRepaintAll( void );
HDC mswGetPrinterDC( void );
int mswMenuAccelerator( wWin_p, long );
void mswMenuMove( wMenu_p, wWinPix_t, wWinPix_t );
void mswRegisterBitMap( HBITMAP );
void mswFontInit( void );
void mswInitColorPalette( void );
void mswPutCustomColors( void );
COLORREF mswGetColor( wBool_t, wDrawColor );
int mswGetColorList( RGBQUAD * );
int mswGetPaletteClock( void );
HPALETTE mswPalette;
HPALETTE mswCreatePalette( void );

/* mswbitmaps.c */
void deleteBitmaps( void );
void mswDrawIcon( HDC, int, int, wIcon_p, int, COLORREF, COLORREF );

/* gwin32.c*/
char *g_win32_getlocale (void);

