/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/gtklib/gtkint.h,v 1.8 2009-12-12 17:16:08 m_fischer Exp $
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GTKINT_H
#define GTKINT_H
#include "wlib.h"

#include "gdk/gdk.h"
#include "gtk/gtk.h"

#define EXPORT

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif

#include "dynarr.h"

#define BORDERSIZE	(4)
#define LABEL_OFFSET	(3)
#define MENUH	(24)

extern wWin_p gtkMainW;

typedef enum {
		W_MAIN, W_POPUP,
		B_BUTTON, B_CANCEL, B_POPUP, B_TEXT, B_INTEGER, B_FLOAT,
		B_LIST, B_DROPLIST, B_COMBOLIST,
		B_RADIO, B_TOGGLE,
		B_DRAW, B_MENU, B_MULTITEXT, B_MESSAGE, B_LINES,
		B_MENUITEM, B_BOX,
		B_BITMAP } wType_e;

typedef void (*repaintProcCallback_p)( wControl_p );
typedef void (*doneProcCallback_p)( wControl_p b );
typedef void (*setTriggerCallback_p)( wControl_p b );
#define WOBJ_COMMON \
		wType_e type; \
		wControl_p next; \
		wControl_p synonym; \
		wWin_p parent; \
		wPos_t origX, origY; \
		wPos_t realX, realY; \
		wPos_t labelW; \
		wPos_t w, h; \
		long option; \
		const char * labelStr; \
		repaintProcCallback_p repaintProc; \
		GtkWidget * widget; \
		GtkWidget * label; \
		doneProcCallback_p doneProc; \
		void * data;

struct wWin_t {
		WOBJ_COMMON
		GtkWidget *gtkwin;             /**< GTK window */
		wPos_t lastX, lastY;
		wControl_p first, last;
		wWinCallBack_p winProc;        /**< window procedure */
		wBool_t shown;                 /**< visibility state */
		const char * nameStr;          /**< window name (not title) */
		GtkWidget * menubar;           /**< menubar handle (if exists) */
		GdkGC * gc;                    /**< graphics context */
		int gc_linewidth;              /**< ??? */
		wBool_t busy;
		int modalLevel;
		};

struct wControl_t {
		WOBJ_COMMON
		};
		
typedef struct wListItem_t * wListItem_p;

struct wList_t {
		WOBJ_COMMON
//		GtkWidget *list;
		int count;
		int number;
		int colCnt;
		wPos_t *colWidths;
		wBool_t *colRightJust;
		GtkListStore *listStore;
		GtkWidget  *treeView;
		int last;
		wPos_t listX;
		long * valueP;
		wListCallBack_p action;
		int recursion;
		int editted;
		int editable;
		};


struct wListItem_t {
		wBool_t active;
		void * itemData;
		const char * label;
		GtkLabel * labelG;
		wBool_t selected;
		wList_p listP;
		};		

#define gtkIcon_bitmap (1)
#define gtkIcon_pixmap (2)
struct wIcon_t {
		int gtkIconType;
		wPos_t w;
		wPos_t h;
		wDrawColor color;
		const void * bits;
		};

extern char wAppName[];
extern char wConfigName[];
extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;

/* boxes.c */
static void boxRepaint(wControl_p b);

/* button.c */
static void pushButt(GtkWidget *widget, gpointer value);
static long choiceGetValue(wChoice_p bc);
static int pushChoice(GtkWidget *widget, gpointer b);
static void choiceRepaint(wControl_p b);

/* misc.c */

int wlibAddLabel( wControl_p, const char * );
void * wlibAlloc( wWin_p, wType_e, wPos_t, wPos_t, const char *, int, void * );
void wlibComputePos( wControl_p );
void wlibControlGetSize( wControl_p );
void wlibAddButton( wControl_p );
wControl_p wlibGetControlFromPos( wWin_p, wPos_t, wPos_t );
char * wlibConvertInput( const char * );
char * wlibConvertOutput( const char * );
struct accelData_t;
struct accelData_t * wlibFindAccelKey( GdkEventKey * event );
wBool_t wlibHandleAccelKey( GdkEventKey * );

wBool_t catch_shift_ctrl_alt_keys( GtkWidget *, GdkEventKey *, void * );
void gtkSetReadonly( wControl_p, wBool_t );
void gtkSetTrigger( wControl_p, setTriggerCallback_p );
GdkPixmap * wlibMakeIcon( GtkWidget *, wIcon_p, GdkBitmap ** );


/* notice.c */
static char * wlibChgMnemonic( char *label );

/* window.c */
void wlibDoModal( wWin_p, wBool_t );

/* gtkhelp.c */
void load_into_view( char *, int );
void gtkAddHelpString( GtkWidget *, const char * );
void gtkHelpHideBalloon( void );

/* boxes.c */
void wlibDrawBox( wWin_p, wBoxType_e, wPos_t, wPos_t, wPos_t, wPos_t );

/* lines.c */
void wlibLineShow( wLine_p b, wBool_t show );

/* gktlist.c */
void gtkListShow( wList_p, wBool_t );
void gtkListSetPos( wList_p );
void gtkListActive( wList_p, wBool_t );
void gtkDropListPos( wList_p );

/* gtktext.c */
void gtkTextFreeze( wText_p );
void gtkTextThaw( wText_p );

/* font.c */
const char * wlibFontTranslate( wFont_p );
PangoLayout *wlibFontCreatePangoLayout( GtkWidget *, void *cairo,
									  wFont_p, wFontSize_t, const char *,
									  int *, int *, int *, int * );

/* gtkbutton.c */
void wlibButtonDoAction( wButton_p );
void wlibSetLabel( GtkWidget*, long, const char *, GtkLabel**, GtkWidget** );

/* gtkcolor.c */
void wlibGetColorMap( void );
GdkColor * wlibGetColor( wDrawColor, wBool_t );

/* psprint.c */

void WlibApplySettings( GtkPrintOperation *op );
void WlibSaveSettings( GtkPrintOperation *op );

void psPrintLine( wPos_t, wPos_t, wPos_t, wPos_t,
				wDrawWidth, wDrawLineType_e, wDrawColor, wDrawOpts );
void psPrintArc( wPos_t, wPos_t, wPos_t, double, double, int,
				wDrawWidth, wDrawLineType_e, wDrawColor, wDrawOpts );
void psPrintString( wPos_t x, wPos_t y, double a, char * s,
				wFont_p fp,	double fs,	wDrawColor color,	wDrawOpts opts );

void psPrintFillRectangle( wPos_t, wPos_t, wPos_t, wPos_t, wDrawColor, wDrawOpts );
void psPrintFillPolygon( wPos_t [][2], int, wDrawColor, wDrawOpts );
void psPrintFillCircle( wPos_t, wPos_t, wPos_t, wDrawColor, wDrawOpts );

struct wDraw_t {
		WOBJ_COMMON
		void * context;
		wDrawActionCallBack_p action;
		wDrawRedrawCallBack_p redraw;

		GdkPixmap * pixmap;
		GdkPixmap * pixmapBackup;

		double dpi;

		GdkGC * gc;
		wDrawWidth lineWidth;
		wDrawOpts opts;
		wPos_t maxW;
		wPos_t maxH;
		unsigned long lastColor;
		wBool_t lastColorInverted;
		const char * helpStr;

		wPos_t lastX;
		wPos_t lastY;

		wBool_t delayUpdate;
		cairo_t *printContext;
		cairo_surface_t *curPrintSurface;
		};
		
/* main.c */

char *wlibGetAppName( void );	

/* tooltip.c */	

#define HELPDATAKEY "HelpDataKey"
void wlibAddHelpString( GtkWidget * widget,	const char * helpStr );

enum columns {
	LISTCOL_DATA,			/**< user data not for display */
	LISTCOL_BITMAP,         /**< bitmap column */
	LISTCOL_TEXT, 			/**< starting point for text columns */
};

/* droplist.c */
int wlibDropListAddColumns( GtkWidget *dropList, int columns );
wIndex_t wDropListGetCount( wList_p b );
void *wDropListGetItemContext( wList_p b, wIndex_t inx );
void wDropListSetIndex( wList_p b, int val );
void wDropListClear( wList_p b );
void wDropListAddValue(	wList_p b, char *text, void *data );
wBool_t wDropListSetValues(	wList_p b, wIndex_t row, const char * labelStr,	wIcon_p bm, void *itemData );

/* treeview.c */
void wTreeViewClear( wList_p b );
GtkWidget *wlibNewTreeView(GtkListStore *ls, int showTitles, int multiSelection);
int wlibTreeViewAddColumns( GtkWidget *tv, int count );
int wlibAddColumnTitles( GtkWidget *tv, const char **titles );
int wlibTreeViewAddData( GtkWidget *tv, int cols, char *label, GdkPixbuf *pixbuf, wListItem_p userData );
void wlibTreeViewAddRow( wList_p b, char *label, wIcon_p bm, wListItem_p id_p );
void wlibListStoreClear( GtkListStore *listStore );
void *wTreeViewGetItemContext( wList_p b, int row );
int wTreeViewGetCount( wList_p b );
void wlibTreeViewSetSelected( wList_p b, int index );

/* liststore.c */
wListItem_p wlibListItemGet(GtkListStore *ls,wIndex_t inx, GList ** childR );
GtkListStore *wlibNewListStore( int colCnt );
void *wlibListStoreGetContext( GtkListStore *ls, int inx );
void wlibListStoreClear( GtkListStore *listStore );
int wlibListStoreUpdateValues( GtkListStore *ls, int row, int cols, char *labels, wIcon_p bm );
int wlibListStoreAddData( GtkListStore *ls, GdkPixbuf *pixbuf, int cols, wListItem_p id );

/* pixmap.c */
GdkPixbuf* wlibMakePixbuf( wIcon_p ip );

void wlibSetLabel(GtkWidget *widget, long option, const char *labelStr, GtkLabel **labelG, GtkWidget **imageG);
#endif
