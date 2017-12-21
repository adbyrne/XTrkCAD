/** \file gtkint.h 
 * Internal definitions for the gtk-library
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
		B_BITMAP, B_STATUS } wType_e;

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
		int maximize_initially; \
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
		int menu_height;
		GdkGC * gc;                    /**< graphics context */
		int gc_linewidth;              /**< ??? */
		wBool_t busy;
		int resizeTimer;		       /** resizing **/
		int resizeW,resizeH;
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

extern char wConfigName[];
extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;



/* boxes.c */
void wlibDrawBox(wWin_p win, wBoxType_e style, wPos_t x, wPos_t y, wPos_t w, wPos_t h);

/* button.c */
void wlibSetLabel(GtkWidget *widget, long option, const char *labelStr, GtkLabel **labelG, GtkWidget **imageG);
void wlibButtonDoAction(wButton_p bb);

struct wButton_t {
    WOBJ_COMMON
    GtkLabel * labelG;
    GtkWidget * imageG;
    wButtonCallBack_p action;
    int busy;
    int recursion;
};

/* color.c */
typedef struct {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    GdkColor normalColor;
    GdkColor invertColor;
    long rgb;
    int colorChar;
} colorMap_t;

GdkColor *wlibGetColor(wDrawColor color, wBool_t normal);

/* control.c */

/* droplist.c */
enum columns {
	LISTCOL_DATA,			/**< user data not for display */
	LISTCOL_BITMAP,         /**< bitmap column */
	LISTCOL_TEXT, 			/**< starting point for text columns */
};
GtkWidget *wlibNewDropList(GtkListStore *ls, int editable);

wIndex_t wDropListGetCount(wList_p b);
void wDropListClear(wList_p b);
void *wDropListGetItemContext(wList_p b, wIndex_t inx);
void wDropListAddValue(wList_p b, char *text, wListItem_p data);
void wDropListSetIndex(wList_p b, int val);
wBool_t wDropListSetValues(wList_p b, wIndex_t row, const char *labelStr, wIcon_p bm, void *itemData);
wList_p wDropListCreate(wWin_p parent, wPos_t x, wPos_t y, const char *helpStr, const char *labelStr, long option, long number, wPos_t width, long *valueP, wListCallBack_p action, void *data);

/* filesel.c */

/* font.c */
PangoLayout *wlibFontCreatePangoLayout(GtkWidget *widget, void *cairo, wFont_p fp, wFontSize_t fs, const char *s, int *width_p, int *height_p, int *ascent_p, int *descent_p);
void wlibFontDestroyPangoLayout(PangoLayout *layout);
const char *wlibFontTranslate(wFont_p fp);

/* help.c */

/* lines.c */
void wlibLineShow(wLine_p bl, wBool_t visible);

/* list.c */
int CompareListData(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);

/* liststore.c */
wListItem_p wlibListItemGet(GtkListStore *ls, wIndex_t inx, GList **childR);
void *wlibListStoreGetContext(GtkListStore *ls, int inx);
void wlibListStoreClear(GtkListStore *listStore);
GtkListStore *wlibNewListStore(int colCnt);
void wlibListStoreSetPixbuf(GtkListStore *ls, GtkTreeIter *iter, GdkPixbuf *pixbuf);
int wlibListStoreAddData(GtkListStore *ls, GdkPixbuf *pixbuf, int cols, wListItem_p id);
int wlibListStoreUpdateValues(GtkListStore *ls, int row, int cols, char *labels, wIcon_p bm);

/* main.c */
char *wlibGetAppName(void);

/* menu.c */
int getMlistOrigin(wMenuList_p ml, GList **pChildren);

/* misc.c */
typedef struct accelData_t {
    wAccelKey_e key;
    int modifier;
    wAccelKeyCallBack_p action;
    void * data;
} accelData_t;


GdkPixbuf* wlibPixbufFromXBM(wIcon_p ip);
int wlibAddLabel(wControl_p b, const char *labelStr);
void *wlibAlloc(wWin_p parent, wType_e type, wPos_t origX, wPos_t origY, const char *labelStr, int size, void *data);
void wlibComputePos(wControl_p b);
void wlibControlGetSize(wControl_p b);
void wlibAddButton(wControl_p b);
wControl_p wlibGetControlFromPos(wWin_p win, wPos_t x, wPos_t y);
char *wlibConvertInput(const char *inString);
char *wlibConvertOutput(const char *inString);
struct accelData_t *wlibFindAccelKey(GdkEventKey *event);
wBool_t wlibHandleAccelKey(GdkEventKey *event);

/* notice.c */

/* pixbuf.c */
GdkPixbuf *wlibMakePixbuf(wIcon_p ip);

/* png.c */

/* print.c */
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

void WlibApplySettings(GtkPrintOperation *op);
void WlibSaveSettings(GtkPrintOperation *op);
void psPrintLine(wPos_t x0, wPos_t y0, wPos_t x1, wPos_t y1, wDrawWidth width, wDrawLineType_e lineType, wDrawColor color, wDrawOpts opts);
void psPrintArc(wPos_t x0, wPos_t y0, wPos_t r, double angle0, double angle1, wBool_t drawCenter, wDrawWidth width, wDrawLineType_e lineType, wDrawColor color, wDrawOpts opts);
void psPrintFillRectangle(wPos_t x0, wPos_t y0, wPos_t x1, wPos_t y1, wDrawColor color, wDrawOpts opts);
void psPrintFillPolygon(wPos_t p[][2], int cnt, wDrawColor color, wDrawOpts opts);
void psPrintFillCircle(wPos_t x0, wPos_t y0, wPos_t r, wDrawColor color, wDrawOpts opts);
void psPrintString(wPos_t x, wPos_t y, double a, char *s, wFont_p fp, double fs, wDrawColor color, wDrawOpts opts);
static void WlibGetPaperSize(void);

/* single.c */

/* splash.c */

/* text.c */

/* timer.c */
void wlibSetTrigger(wControl_p b, setTriggerCallback_p trigger);

/* tooltip.c */
#define HELPDATAKEY "HelpDataKey"
void wlibAddHelpString(GtkWidget *widget, const char *helpStr);
void wlibHelpHideBalloon();

/* treeview.c */
void wlibTreeViewSetSelected(wList_p b, int index);
GtkWidget *wlibNewTreeView(GtkListStore *ls, int showTitles, int multiSelection);
int wlibTreeViewAddColumns(GtkWidget *tv, int count);
int wlibAddColumnTitles(GtkWidget *tv, const char **titles);
int wlibTreeViewAddData(GtkWidget *tv, int cols, char *label, GdkPixbuf *pixbuf, wListItem_p userData);
void wlibTreeViewAddRow(wList_p b, char *label, wIcon_p bm, wListItem_p id_p);
gboolean changeSelection(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer data);

int wTreeViewGetCount(wList_p b);
void wTreeViewClear(wList_p b);
void *wTreeViewGetItemContext(wList_p b, int row);

/* window.c */
void wlibDoModal(wWin_p win0, wBool_t modal);
wBool_t catch_shift_ctrl_alt_keys(GtkWidget *widget, GdkEventKey *event, void *data);

/* wpref.c */

#endif
