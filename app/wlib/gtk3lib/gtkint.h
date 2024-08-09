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

#ifndef MISC_H
#include "dynarr.h"
#endif

#define BORDERSIZE	(4)
#define LABEL_OFFSET	(3)
#define MENUH	(24)

#define PPI (72.0)
#define P2I( P ) ((P)/PPI)

#define CENTERMARK_LENGTH (60)				/**< size of cross marking center of circles */
#define DASH_LENGTH (8.0)

#define MINLINEWIDTHBITMAP (1.0)
#define MINLINEWIDTHPRINT (0.09)

extern wWin_p gtkMainW;


#ifdef CURSOR_SURFACE
typedef struct {
		cairo_surface_t* surface;
		wWinPix_t width;
		wWinPix_t height;
		wBool_t show;
} wCursorSurface_t, * wSurface_p;
#endif

//typedef enum { M_MENU, M_SEPARATOR, M_PUSH, M_LIST, M_LISTITEM, M_TOGGLE, M_RADIO } mtype_e;

typedef enum {
		W_MAIN, W_POPUP, W_FILESELECT, W_DIALOG,
		B_BUTTON, B_CANCEL, B_POPUP, B_TEXT, B_INTEGER, B_FLOAT,
		B_LIST, B_DROPLIST, B_COMBOBOX,
		B_RADIO, B_TOGGLE,
		B_DRAW, B_MENU, B_MULTITEXT, B_MESSAGE, B_LINES,
		B_MENUITEM, B_BOX,
		B_BITMAP, B_STATUS,
		B_COLORBUTTON,
		M_MENU, M_SUBMENU, M_PUSH, M_TOGGLE, M_RADIO, M_SEPARATOR, M_RECENTUSE
} wType_e;

typedef void (*repaintProcCallback_p)( wControl_p );
typedef void (*doneProcCallback_p)( wControl_p b );
typedef void (*setTriggerCallback_p)( wControl_p b );


/*
 * Definition of attributes structs for the controls. Combining them into one
 * union as part of a struct avoids problems with compilers that optimize
 * using aliasing (eg. gcc or clang)
 * 
 * see  https://stackoverflow.com/questions/52749409/multiple-structs-same-fields-that-need-to-be-accessed-in-a-method
 *
 */

struct bitmap {
	char unused;
};

struct button {
	wButtonCallBack_p action;
	void* attributes;
};

struct choice {
	long* valueP;
	wChoiceCallBack_p action;
	char* labelStr;
};

struct colorbutton {
	wDrawColor* valueP;
	wColorSelectButtonCallBack_p action;
};

struct draw {
	wDrawActionCallBack_p action;
	wDrawRedrawCallBack_p redraw;	//< handle redraw requests
	bool bTempMode;
	gdouble dpi;					//< resolution of screen
	gint width;						//< size of drawing area and surface
	gint height;
	cairo_surface_t *surface;		//< main drawing surface
	cairo_surface_t *temp_surface;
	wAction_t lastAction;
	long lastX;					//< position of last mouse click
	long lastY;
	long option;					//< creation options

};

struct entry {
	char* valueP;				//< location of entered value
	unsigned valueL;			//< maximum length of entered value
	wEntryCallBack_p action;	//< callback
};

struct list {
	GtkListStore* listStore;
	GtkTreeView* treeView;
	long* valueP;
	int editted;
	int last;
	wListCallBack_p action;
};

struct menu {
	GSList* radioGroup;			/* radio button group */
	wMenuTraceCallBack_p traceFunc;
	void* traceData;
};

struct menuitem {
	wControl_p parentMenu;
	wMenuCallBack_p action;
};

struct message {
	char unused;				//< msvc doesn't allow empty structs
};
struct radio {
	long* valueP;
	wChoiceCallBack_p action;
};
struct toggle {
	long* valueP;
	wChoiceCallBack_p action;
};
struct recentuse {
	unsigned max;				// maximum members
	unsigned current;			// current number of members
	GSList* elements;			// list of elements
	GtkWidget** widgets;		// display widgets
	wMenuListCallBack_p action;
	wControl_p parentMenu;
};

struct text {
	gchar* placeholder;
	GtkTextTag* placeholderTag;
	int changed;
	long option;
	GtkWidget* text;
};

struct window {
	wWinCallBack_p winProc;			/**< window procedure */
	GtkWidget* menubar;				/**< menubar handle (if exists) */
	GtkWidget* toolbar;				/**< toolbar handle (if exists) */
	long option;					/**< creation options for window */
	GtkBuilder* builder;
	GtkAccelGroup* accelGroup;		/**< accelerator group if main window */
	GtkContainer* statusbar;
	bool maximize_initially;		
};

struct control {
	wType_e type;
	GtkWidget* widget;
	void* context;
	char* name;
	wControl_p synonym;
	wControl_p parent;
	GtkWidget* label;
	union {
		struct bitmap bitmap;
		struct button button;
		struct choice choice;
		struct colorbutton colorbutton;
		struct draw draw;
		struct entry entry;
		struct message message;
		struct text text;
		struct list list;
		struct menu menu;
		struct menuitem menuitem;
		struct radio radio;
		struct recentuse recentuse;
		struct toggle toggle;
		struct window window;
	} attributes;
};

#define CONTROL_GET_ATTRIBUTES_PTR(base, field) (&(base->attributes.field))
#define HASDIALOGBUILDER(parent) ((parent)->attributes.window.option & BO_USEBUILDER)

struct wObjCommon {
	wType_e type;			/**< type of control */
	wWin_p parent;			/**< parent control */
	long option;			/**< creation options*/
	void * attributes;			/**< application additional attributes */
	GtkWidget * widget;		/**< GTK widget */
	gchar *helpTopic;
	gchar *name;
	gchar *labelStr;
};

struct  wWindow_t {	
	wType_e type;
	wWinCallBack_p winProc;     /**< window procedure */
	GtkWidget *menubar;			/**< GTK menubar handle if present */
	GtkWidget *toolbar;			/**< GTK toolbar handle if present */
	GtkContainer* statusbar;	/**< GTK statusbar handle if present */
	GtkAccelGroup *accelGroup;	/**< accelerator group connected to Window */
	GtkDrawingArea* drawingArea;/**< the drawing area */
	GtkWidget* gtkWindow;		/**< the GTK window */
	GtkBuilder* builder;
	const char* name;			/**< unique name for window */
	const char* helpTopic;
};

/** /todo Check necessity of above fields */

//typedef struct wWindow_t * wWindow_p;

#define WOBJ_COMMON \
		wType_e type; \
		wControl_p next; \
		wControl_p synonym; \
		wWin_p parent; \
		wWinPix_t origX, origY; \
		wWinPix_t realX, realY; \
		wWinPix_t default_size_x, default_size_y; \
		wWinPix_t labelW; \
		wWinPix_t w, h; \
		int maximize_initially; \
		long option; \
		const char * labelStr; \
		repaintProcCallback_p repaintProc; \
		GtkWidget * widget; \
		GtkWidget * label; \
		doneProcCallback_p doneProc; \
		cairo_t * cr; \
		/* CURSOR_SURFACE wCursorSurface_t cursor_surface;*/ \
		wBool_t outline; \
		void * attributes; \
		int fromTemplate;               /**< widget was build from ui template */  \
        char * template_id; \


struct wWin_t {
		WOBJ_COMMON
		GtkWidget *gtkwin;             /**< GTK window */
		wWinPix_t lastX, lastY;
		wControl_p first, last;
		wWinCallBack_p winProc;        /**< window procedure */
		wBool_t shown;                 /**< visibility state */
		const char * nameStr;          /**< window name (not title) */
		GtkWidget * menubar;           /**< menubar handle (if exists) */
		GtkWidget * toolbar;
		int menu_height;
		int gc_linewidth;              /**< ??? */
		wBool_t busy;
		int resizeTimer;		       /** resizing **/
		int resizeW,resizeH;
		int timer_idle_count;
		int timer_busy_count;
		int modalLevel;
        GtkBuilder *builder;
		};

//struct wControl_t {
//		WOBJ_COMMON
//		};
		
typedef struct wListItem_t * wListItem_p;



struct wListItem_t {
		wBool_t active;
		void * itemData;
		char * label;
		GtkLabel * labelG;
		wBool_t selected;
		wControl_p listP;
		};		


extern char wConfigName[];
extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;

/* appwindow.c*/
GtkAccelGroup* wlibAppWinGetAccelGroup(void);
GtkWidget* wlibAppWinGetMain(void);

/* basicdraw.c */

void wlibBasicClear( wDraw_p bd );
void wlibBasicDrawLine(
	wDraw_p bd,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawPix_t x1, wDrawPix_t y1,
    double width,
	double minWidth,
    wDrawLineType_e lineType,
    wDrawColor color,
    wDrawOpts opts);

void wlibBasicDrawArc(
	wDraw_p bd,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawPix_t r,
    double angle0,
    double angle1,
    wBool_t drawCenter,
    double width,
	double minWidth,
    wDrawLineType_e lineType,
    wDrawColor color,
    wDrawOpts opts);

void wlibBasicDrawString(
	wDraw_p bd,
    wDrawPix_t x, wDrawPix_t y,
    double a,
    char * s,
    wFont_p fp,
    double fs,
    double width,
	double minWidth,
    wDrawColor color,
    wDrawOpts opts);

void wlibBasicDrawFillPolygon(
	wDraw_p bd,
    wDrawPix_t p[][2],
	wPolyLine_e type[],
    int cnt,
    wDrawColor color,
    wDrawOpts opts,
	int fill,
	int open );

void wlibBasicDrawFillRectangle(
	wDraw_p bd,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawPix_t x1, wDrawPix_t y1,
    wDrawColor color,
    wDrawOpts opts);

void wlibBasicDrawFillCircle(
	wDraw_p bd,
    wDrawPix_t x0, wDrawPix_t y0,
    wDrawPix_t r,
    wDrawColor color,
    wDrawOpts opts);

/* bitmap.c */
#define ICON_BITMAP (1)
#define ICON_PIXMAP (2)

struct wIcon_t {
	int gtkIconType;
	wWinPix_t w;
	wWinPix_t h;
	wDrawColor color;
	const void* bits;
};

/* boxes.c */
void wlibDrawBox(wWin_p win, wBoxType_e style, wWinPix_t x, wWinPix_t y, wWinPix_t w, wWinPix_t h);

/* builder.c */
wControl_p wlibDialogFromTemplate( int winType, const char *labelStr, 
	const char *nameStr, long option, void *attributes );
GString *wlibFileNameFromDialog( const char *dialog );
GtkWidget *wlibGetWidgetFromName( wControl_p parent, const char *dialogname, const char *suffix, wBool_t ignore_failures );
GtkWidget *wlibWidgetFromId( wControl_p win, const char *id );
GtkWidget *wlibWidgetFromIdWarn( wControl_p win, const char *id );
void wlibAddContentFromTemplate( wWin_p win, const char *nameStr);
bool wlibExistsTemplate(const char *name);

/* button.c */
void wlibSetLabel(GtkWidget *widget, long option, const char *labelStr, GtkLabel **labelG, GtkWidget **imageG);
void wlibButtonDoAction(wControl_p bb);


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

GdkRGBA wlibGetColor(wDrawColor color, wBool_t normal);

/* control.c */
wBool_t wControlExpose (GtkWidget * widget, cairo_t *cr, wControl_p b);
wControl_p wlibControlNew(wType_e type, wControl_p parent, const char* name, 
	void* context);

/* dialog.c */
void wlibBasicGridAttach(wControl_p parent, GtkWidget *widget, unsigned xPos,
	unsigned yPos, unsigned colSpan, unsigned rowSpan);

/* droplist.c */
enum columns {
	LISTCOL_DATA,			/**< user attributes not for display */
	LISTCOL_BITMAP,         /**< bitmap column */
	LISTCOL_TEXT, 			/**< starting point for text columns */
};

/**
 * \todo partly convert to union for combobox vs.listbox. treeView is valid for 
 * listBox, editted and last for combobox
 * union {
 *		struct {
 * 			int editted;
 *			int last;
 *		} cd;
 *		struct {
 *			GkTreeView *treeView
 *		} tv;
 * } listData;
 */

struct wList_t {
	wType_e type;
	GtkWidget* widget;
	GtkListStore* listStore;
	GtkTreeView* treeView;
	long* valueP;
	void* attributes;
	int editted;
	int last;
	wListCallBack_p action;
};

GtkWidget *wlibNewDropList(GtkListStore *ls, int editable);

wIndex_t wDropListGetCount(wControl_p b);
void wDropListClear(wList_p b);
void *wDropListGetItemContext(wControl_p b, wIndex_t inx);
void wDropListAddValue(wControl_p b, char *text, wListItem_p attributes);
void wDropListSetIndex(wList_p b, int val);
wBool_t wDropListSetValues(wControl_p b, wIndex_t row, const char *labelStr, wIcon_p bm, void *itemData);
wList_p wDropListCreate(wWin_p parent, wWinPix_t x, wWinPix_t y, const char *helpStr, const char *labelStr, long option, long number, wWinPix_t width, long *valueP, wListCallBack_p action, void *attributes);

/* filesel.c */

/* font.c */
PangoLayout *wlibFontCreatePangoLayout(GtkWidget *widget, void *cairo, wFont_p fp, wFontSize_t fs, const char *s, wDrawPix_t *width_p, wDrawPix_t *height_p, wDrawPix_t *ascent_p, wDrawPix_t *descent_p, wDrawPix_t *baseline_p);
void wlibFontDestroyPangoLayout(PangoLayout *layout);
const char *wlibFontTranslate(wFont_p fp);

/* help.c */

/* lines.c */
void wlibLineShow(wLine_p bl, wBool_t visible);

/* list.c */
int CompareListData(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer attributes);

/* liststore.c */
wListItem_p wlibListItemGet(GtkListStore *ls, wIndex_t inx, GList **childR);
void *wlibListStoreGetContext(GtkListStore *ls, int inx);
void wlibListStoreClear(GtkListStore *listStore);
GtkListStore *wlibNewListStore(int colCnt);
void wlibListStoreSetPixbuf(GtkListStore *ls, GtkTreeIter *iter, GdkPixbuf *pixbuf);
int wlibListStoreAddData(GtkListStore *ls, GdkPixbuf *pixbuf, wListItem_p id);
int wlibListStoreUpdateValues(GtkListStore *ls, int row, char *labels, wIcon_p bm);

/* main.c */
char *wlibGetAppName(void);
GtkApplication *wlibGetApp(void);

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
void wlibAddLabel(wControl_p b, wWinPix_t x, wWinPix_t y,
	const char* labelStr);
void *wlibAlloc(wWin_p parent, wType_e type, wWinPix_t origX, wWinPix_t origY, const char *labelStr, int size, void *attributes);
void wlibComputePos(wControl_p b);
void wlibControlGetSize(wControl_p b);
void wlibAddButton(wControl_p b);
wControl_p wlibGetControlFromPos(wWin_p win, wWinPix_t x, wWinPix_t y);
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

		cairo_surface_t * surface;
		cairo_surface_t * temp_surface;
		
		cairo_t *printContext;
		cairo_surface_t * curPrintSurface;

		wBool_t clip_set;
		GdkRectangle rect;

		GdkPixbuf * pixbuf;
		GdkPixbuf * pixbufBackup;

		double dpi;
		double scale_adjust;
		double scale_text;

		wDrawWidth lineWidth;
		wDrawOpts opts;
		wWinPix_t maxW;
		wWinPix_t maxH;
		unsigned long lastColor;
		wBool_t lastColorInverted;
		const char * helpStr;

		wWinPix_t lastX;
		wWinPix_t lastY;

		wBool_t delayUpdate;
		GdkPixbuf * background;

		wBool_t bTempMode;
		unsigned drawDestination;
		};

void WlibApplySettings(GtkPrintOperation *op);
void WlibSaveSettings(GtkPrintOperation *op);
void psPrintLine(wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t x1, wDrawPix_t y1, wDrawWidth width, wDrawLineType_e lineType, wDrawColor color, wDrawOpts opts);
void psPrintArc(wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t r, double angle0, double angle1, wBool_t drawCenter, wDrawWidth width, wDrawLineType_e lineType, wDrawColor color, wDrawOpts opts);
void psPrintFillRectangle(wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t x1, wDrawPix_t y1, wDrawColor color, wDrawOpts opts);
void psPrintFillPolygon(wDrawPix_t p[][2], wPolyLine_e type[], int cnt, wDrawColor color, wDrawOpts opts, int fill, int open);
void psPrintFillCircle(wDrawPix_t x0, wDrawPix_t y0, wDrawPix_t r, wDrawColor color, wDrawOpts opts);
void psPrintString(wDrawPix_t x, wDrawPix_t y, double a, char *s, wFont_p fp, double fs, wDrawColor color, wDrawOpts opts);
static void WlibGetPaperSize(void);

/* single.c */
void wlibStringUpdate();

/* splash.c */

/* text.c */

/* timer.c */
void wlibSetTrigger(wControl_p b, setTriggerCallback_p trigger);

/* toggle.c */


/**
 * \todo Check usage of labelStr
 */
struct wChoice_t {
	wType_e type;
	GtkWidget* widget;
	long* valueP;
	wChoiceCallBack_p action;
	char* labelStr;
	void* attributes;
};

/* tooltip.c */
#define HELPDATAKEY "HelpDataKey"
void wlibAddHelpString(GtkWidget *widget, const char *helpStr);
void wlibAddTooltip(GtkWidget* widget, char* dialog, char* field);
void wlibHelpHideBalloon();

/* treeview.c */
void wlibTreeViewSetSelected(wControl_p b, int index);
GtkTreeView *wlibNewTreeView(GtkListStore *ls, int showTitles, int multiSelection);
int wlibTreeViewAddColumns(GtkTreeView *tv, int count);
int wlibAddColumnTitles(GtkTreeView *tv, const char **titles);
int wlibTreeViewAddData(GtkTreeView *tv, char *label, GdkPixbuf *pixbuf, 
	wListItem_p userData);
void wlibTreeViewAddRow(wControl_p b, char *label, wIcon_p bm, 
	wListItem_p id_p);
gboolean changeSelection(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean path_currently_selected, gpointer attributes);

int wTreeViewGetCount(wControl_p b);
void wTreeViewClear(wList_p b);
void *wTreeViewGetItemContext(wControl_p b, int row);

/* window.c */
void wlibDoModal(wWin_p win0, wBool_t modal);
wBool_t UpdateModifierKeyState(GdkEventKey *event);
wControl_p wlibCreateFromTemplate( wControl_p parent, int winType, wWinPix_t x, wWinPix_t y,
    const char * labelStr, const char * nameStr, long option,
    wWinCallBack_p winProc, void * attributes);
void wlibAddButtonToolbar(wButton_p button);
void wlibBasicGridAttach(wControl_p parent, GtkWidget* widget, unsigned xPos, unsigned yPos, unsigned colSpan, unsigned rowSpan);
/* wpref.c */

/* wlibpaths.c */

#endif
