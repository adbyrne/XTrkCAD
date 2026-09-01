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
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <wlib.h>

#ifdef WINDOWS
#define strcasecmp _stricmp
#endif

#define BORDERSIZE (4)
#define LABEL_OFFSET (3)
#define MENUH (24)

#define PPI (72.0)
#define P2I(P) ((P) / PPI)

#define CENTERMARK_LENGTH (60) /**< size of cross marking center of circles */
#define DASH_LENGTH (8.0)

#define CSS_FILENAME "xtrackcad.css"

extern int wlibRecursionTrace;

#ifdef CURSOR_SURFACE
typedef struct {
	cairo_surface_t *surface;
	wWinPix_t width;
	wWinPix_t height;
	wBool_t show;
} wCursorSurface_t, *wSurface_p;
#endif

typedef struct {
	GQueue *elements; // GLib Queue for MRU list
	int max_capacity; // mamimum number of entries, -1 for unlimited
} MRUList;

MRUList *MRUCreate(int max_capacity);
void *MRUTouchEntry(MRUList *list, const char *label, void *entry);
void *MRUAppendEntry(MRUList *list, const char *label, void *entry);
void *MRUGetRecent(MRUList *list);
void *MRUGetNth(MRUList *list, int index);
int MRUGetCount(MRUList *list);
void *MRURemoveEntry(MRUList *list, const char *label);
void MRUClear(MRUList *list);
void MRUDestroy(MRUList *list);

typedef enum {
	W_MAIN,
	W_POPUP,
	W_FILESELECT,
	W_DIALOG,
	B_BUTTON,
	B_RADIO,
	B_TOGGLE,
	B_STICKY,
	B_CANCEL,
	B_POPUP,
	B_TEXT,
	B_INTEGER,
	B_FLOAT,
	B_LIST,
	B_COMBOBOX,
	B_DRAW,
	B_MENU,
	B_MULTITEXT,
	B_MESSAGE,
	B_LINES,
	B_MENUITEM,
	B_BOX,
	B_BITMAP,
	B_STATUS,
	B_COLORBUTTON,
	B_STACK,
	B_NOTEBOOK,
	B_EXPANDER,
	M_MENU,
	M_SUBMENU,
	M_PUSH,
	M_TOGGLE,
	M_RADIO,
	M_SEPARATOR,
	M_RECENTUSE,
	B_SCALE,
	B_TAG,
	B_SEPARATOR
} wType_e;

typedef void (*repaintProcCallback_p)(wControl_p);
typedef void (*doneProcCallback_p)(wControl_p b);
typedef void (*setTriggerCallback_p)(wControl_p b);

/*
 * Definition of attributes structs for the controls. Combining them into one
 * union as part of a struct avoids problems with compilers that optimize
 * using aliasing (eg. gcc or clang)
 *
 * see
 * https://stackoverflow.com/questions/52749409/multiple-structs-same-fields-that-need-to-be-accessed-in-a-method
 *
 */

struct bitmap {
	char unused;
};

enum TIMER_STATE { INITIAL_DELAY, REPEATED };

struct button {
	wButtonCallBack_p action;
	wIcon_p icon;
	wBool_t is_pressed;
	guint timeout_id;
	enum TIMER_STATE timer_state;
	int recursion;
};

struct choice {
	long *valueP;
	wChoiceCallBack_p action;
	char *labelStr;
	int recursion;
};

struct colorbutton {
	wDrawColor *valueP;
	wColorSelectButtonCallBack_p action;
};

struct rect {
	wDrawPix_t x;
	wDrawPix_t y;
	wDrawPix_t w;
	wDrawPix_t h;
};

struct draw {
	wDrawActionCallBack_p action;
	wDrawRedrawCallBack_p redraw; //< handle redraw requests
	bool bTempMode;
	gdouble dpi; //< resolution of screen
	gint width;  //< size of drawing area and surface
	gint height;
	cairo_surface_t *surface; //< main drawing surface
	cairo_surface_t *temp_surface;
	wAction_t lastAction;
	long lastX; //< position of last mouse click
	long lastY;
	long option; //< creation options
	unsigned drawDestination;
	unsigned delayUpdate;
	GdkPixbuf *pixbuf; /** \todo Check necessity */
	GdkPixbuf *background;
	GdkPixbuf *pixbufBackup; //< snapshot saved by wDrawSaveImage/wDrawRestoreImage
	cairo_t *cr;
	double scale_adjust; /** \todo Check necessity */
	double scale_text;   /** \todo Check necessity */
	struct rect *clipregion;

	// state of Cairo context
	wDrawColor activeColor;
	wDrawWidth activeWidth;
	wDrawLineType_e activeStyle;
	int pathOpen;
	int fill;
	int batchCount;
};

struct entry {
	char *valueP;            //< location of entered value
	unsigned valueL;         //< maximum length of entered value
	wEntryCallBack_p action; //< callback
};

struct expander {
	wExpanderToggleCallback_p action;        /* user-toggle callback        */
	unsigned long             notifyHandler; /* notify::expanded handler id  */
	int                       inProgrammaticSet; /* guard: suppress action   */
};

struct list {
	GtkListStore *listStore;
	GtkTreeView *treeView;
	long *valueP;
	int editted;
	int last;
	wListCallBack_p action;
	int recursion;
	int count;
};

struct menu {
	GSList *radioGroup; /* radio button group */
	wMenuTraceCallBack_p traceFunc;
	void *traceData;
	long option;
	wControl_p first;
	wControl_p last;
};

struct menuitem {
	wControl_p parentMenu;
	wMenuCallBack_p action;
	int type;
	gulong toggleHandler;
	char * label;
	int set;
	wControl_p next;
};

struct message {
	char unused; //< msvc doesn't allow empty structs
};
struct radio {
	long *valueP;
	wChoiceCallBack_p action;
	int recursion;
};
struct toggle {
	long *valueP;
	wChoiceCallBack_p action;
	unsigned long toggleHandler;
	int recursion;
};
struct recentuse {
	MRUList *mrulist;
	wMenuListCallBack_p action;
	wControl_p parentMenu;
	GtkWidget *emptyList;
	SORTORDER sortorder;
};

struct scale {
	wScaleCallBack_p action;
	double *valuePointer;
};

struct notebook {
	wChoiceCallBack_p action;
};

struct stack {
	void *callback;
};

struct tag {
	wButtonCallBack_p callback;
	GtkButton *button;
	GtkLabel *label;
};
struct text {
	int changed;
	long option;
	GtkWidget *text;
};

struct window {
	wWinCallBack_p winProc; /**< window procedure */
	GtkWidget *menubar;     /**< menubar handle (if exists) */
	GtkWidget *toolbar;     /**< toolbar handle (if exists) */
	long option;            /**< creation options for window */
	GtkBuilder *builder;
	GtkAccelGroup *accelGroup; /**< accelerator group if main window */
	GtkContainer *statusbar;
	bool maximize_initially;
	int resizeTimer; /** resizing **/
	int w;
	int h;
	wBool_t size_changed;
};

struct control {
	wType_e type;
	GtkWidget *widget;
	void *context;
	const char *name;
	wControl_p synonym;
	wControl_p parent;
	GtkWidget *label;
	const char *customTooltip;
	union {
		struct bitmap bitmap;
		struct button button;
		struct choice choice;
		struct colorbutton colorbutton;
		struct draw draw;
		struct entry entry;
		struct expander expander;
		struct message message;
		struct notebook notebook;
		struct text text;
		struct list list;
		struct menu menu;
		struct menuitem menuitem;
		struct radio radio;
		struct recentuse recentuse;
		struct scale scale;
		struct stack stack;
		struct tag tag;
		struct toggle toggle;
		struct window window;
	} attributes;
};

#define CONTROL_GET_ATTRIBUTES_PTR(base, field) (&(base->attributes.field))
#define ISDEFINEDINBUILDER(parent)                                             \
  ((parent)->attributes.window.option & F_DEFINEDINBUILDER)

struct wObjCommon {
	wType_e type;      /**< type of control */
	wWin_p parent;     /**< parent control */
	long option;       /**< creation options*/
	void *attributes;  /**< application additional attributes */
	GtkWidget *widget; /**< GTK widget */
	gchar *helpTopic;
	gchar *name;
	gchar *labelStr;
};

struct wWindow_t {
	wType_e type;
	wWinCallBack_p winProc;      /**< window procedure */
	GtkWidget *menubar;          /**< GTK menubar handle if present */
	GtkWidget *toolbar;          /**< GTK toolbar handle if present */
	GtkContainer *statusbar;     /**< GTK statusbar handle if present */
	GtkAccelGroup *accelGroup;   /**< accelerator group connected to Window */
	GtkDrawingArea *drawingArea; /**< the drawing area */
	GtkWidget *gtkWindow;        /**< the GTK window */
	GtkBuilder *builder;
	const char *name; /**< unique name for window */
	const char *helpTopic;
};

/** /todo Check necessity of above fields */

// typedef struct wWindow_t * wWindow_p;

#define WOBJ_COMMON                                                            \
  wType_e type;                                                                \
  wControl_p next;                                                             \
  wControl_p synonym;                                                          \
  wWin_p parent;                                                               \
  wWinPix_t origX, origY;                                                      \
  wWinPix_t realX, realY;                                                      \
  wWinPix_t default_size_x, default_size_y;                                    \
  wWinPix_t labelW;                                                            \
  wWinPix_t w, h;                                                              \
  int maximize_initially;                                                      \
  long option;                                                                 \
  const char *labelStr;                                                        \
  repaintProcCallback_p repaintProc;                                           \
  GtkWidget *widget;                                                           \
  GtkWidget *label;                                                            \
  doneProcCallback_p doneProc;                                                 \
  cairo_t *cr;                                                                 \
  /* CURSOR_SURFACE wCursorSurface_t cursor_surface;*/                         \
  wBool_t outline;                                                             \
  void *attributes;                                                            \
  int fromTemplate; /**< widget was build from ui template */                  \
  char *template_id;

struct wWin_t {
	WOBJ_COMMON
	GtkWidget *gtkwin; /**< GTK window */
	wWinPix_t lastX, lastY;
	wControl_p first, last;
	wWinCallBack_p winProc; /**< window procedure */
	wBool_t shown;          /**< visibility state */
	const char *nameStr;    /**< window name (not title) */
	GtkWidget *menubar;     /**< menubar handle (if exists) */
	GtkWidget *toolbar;
	int menu_height;
	int gc_linewidth; /**< ??? */
	wBool_t busy;
	int resizeTimer; /** resizing **/
	int resizeW, resizeH;
	int timer_idle_count;
	int timer_busy_count;
	int modalLevel;
	GtkBuilder *builder;
};

// struct wControl_t {
//		WOBJ_COMMON
//		};

typedef struct wListItem_t *wListItem_p;

struct wListItem_t {
	wBool_t active;
	void *itemData;
	char *label;
	wBool_t selected;
	wControl_p listP;
};

extern char wConfigName[];
extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;

/* appwindow.c*/
GtkAccelGroup *wlibAppWinGetAccelGroup(void);
GtkWidget *wlibAppWinGetMain(void);

/* basicdraw.c — public API now in wlib.h as wBasicXxx */

/* bitmap.c */
#define ICON_PIXBUF 2
#define ICON_PIXBUF_FROM_TEXT 3
#define ICON_PIXBUF_FROM_RESOURCE 4

struct wIcon_t {
	int gtkIconType;
	GdkPixbuf *bits;
	wWinPix_t w;
	wWinPix_t h;
	wDrawColor color;
	const char *text;
	const char *filename;
};

/* boxes.c */

/* builder.c */
GString *wlibFileNameFromDialog(const char *dialog);
GtkWidget *wlibWidgetFromId(wControl_p win, const char *id);
GtkWidget *wlibWidgetFromIdWarn(wControl_p win, const char *id);
GtkWidget *wlibCreateWindowFromBuilder(wControl_p window, const char *nameStr,
                                       long option);
bool wlibExistsTemplate(const char *name);

/* button.c */
void wlibButtonDoAction(wControl_p bb);
void wlibAddButtonToToolbar(wControl_p buttonControl, const char *helpStr);

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
wControl_p wlibControlNew(wType_e type, wControl_p parent, const char *name,
                          void *context);

/* dialog.c */
void wlibBasicGridAttach(wControl_p parent, GtkWidget *widget, unsigned xPos,
                         unsigned yPos, unsigned colSpan, unsigned rowSpan);

/* lists */
enum columns {
	LISTCOL_ERROR_COLOR, /**< use error color for display if set */
	LISTCOL_DATA,        /**< user attributes not for display */
	LISTCOL_BITMAP,      /**< bitmap column */
	LISTCOL_TEXT,        /**< starting point for text columns */
};

struct wList_t {
	wType_e type;
	GtkWidget *widget;
	GtkListStore *listStore;
	GtkTreeView *treeView;
	long *valueP;
	void *attributes;
	int editted;
	int last;
	wListCallBack_p action;
	int recursion;
};

/* entry.c */

/* filesel.c */

/* font.c */
PangoLayout *
wlibFontCreatePangoLayout(GtkWidget *widget, void *cairo, wFont_p fp,
                          wFontSize_t fs, const char *s, wDrawPix_t *width_p,
                          wDrawPix_t *height_p, wDrawPix_t *ascent_p,
                          wDrawPix_t *descent_p, wDrawPix_t *baseline_p);
void wlibFontDestroyPangoLayout(PangoLayout *layout);
const char *wlibFontTranslate(wFont_p fp);

/* freetypelabel.c */
void wlibFTLabelChangeColor(wIcon_p button, wDrawColor newColor);

/* help.c */

/* lines.c */
void wlibLineShow(wLine_p bl, wBool_t visible);

/* list.c */
int CompareListData(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                    gpointer attributes);

/* liststore.c */
wListItem_p wlibListItemGet(GtkListStore *ls, wIndex_t inx, GList **childR);
void *wlibListStoreGetContext(GtkListStore *ls, int inx);
void wlibListStoreClear(GtkListStore *listStore);
GtkListStore *wlibNewListStore(int colCnt);
int wlibListStoreUpdateValues(GtkListStore *ls, int row, char *labels,
                              wIcon_p bm);

void wlibListStoreAppendRow(GtkListStore *store, GtkTreeIter *iter,
                            wListItem_p data);
void wlibListStoreSetIcon(GtkListStore *store, GtkTreeIter *iter, wIcon_p icon);
void wlibListStoreSetData(GtkListStore *store, GtkTreeIter *iter, int column,
                          const char *label);
/* main.c */
char *wlibGetAppName(void);
GtkApplication *wlibGetApp(void);

/* menu.c */

/* misc.c */
typedef struct accelData_t {
	wAccelKey_e key;
	int modifier;
	wAccelKeyCallBack_p action;
	void *data;
} accelData_t;

void wlibAddLabel(wControl_p b, wWinPix_t x, wWinPix_t y, const char *labelStr);
void wlibControlGetSize(wControl_p b);
char *wlibConvertInput(const char *inString);
char *wlibConvertOutput(const char *inString);
struct accelData_t *wlibFindAccelKey(GdkEventKey *event);
wBool_t wlibHandleAccelKey(GdkEventKey *event);

/* notice.c */

/* png.c */

/* wPref.c */
GtkPrintSettings *wlibPrefGetPrintSettings(void);
void wlibPrefSetPrintSettings(GtkPrintSettings *settings);
GtkPageSetup *wlibPrefGetPageSetup(void);
void wlibPrefSetPageSetup(GtkPageSetup *page_setup);

/* print.c */
void WlibApplySettings(GtkPrintOperation *op);
void WlibSaveSettings(GtkPrintOperation *op);

// static void wlibGetPaperSize(void);

/* single.c */

/* splash.c */

/* text.c */

/* timer.c */
void wlibSetTrigger(wControl_p b, setTriggerCallback_p trigger);

/* toggle.c */

/* togglegroup.c */
void wlibToggleGroupsInit(void);
void wlibToggleGroupConnect(const gchar *group_name, GCallback callback,
                            wControl_p  user_data);
struct wChoice_t {
	wType_e type;
	GtkWidget *widget;
	long *valueP;
	wChoiceCallBack_p action;
	char *labelStr;
	void *attributes;
};

/* toolbarctrl.c */
GtkWidget *wlibToolbarCreate(GtkWidget *container);
void wlibSetAbutStyle(GtkWidget *button);
void wlibAddButtonToToolbar(wControl_p buttonControl, const char *helpStr);

/* tooltip.c */
void wlibAddTooltip(GtkWidget *widget, const char *dialog,
                    const char *dialogitem);

/* treeview.c */
wListItem_p wlibAllocateListItem(wControl_p b, const char *labelStr,
                                 void *itemData);
void wlibTreeViewSetSelected(wControl_p b, int index);
GtkTreeView *wlibNewTreeView(GtkListStore *ls, int showTitles,
                             int multiSelection);
int wlibTreeViewAddColumns(GtkTreeView *tv, int count);
int wlibAddColumnTitles(GtkTreeView *tv, const char **titles);
gboolean changeSelection(GtkTreeSelection *selection, GtkTreeModel *model,
                         GtkTreePath *path, gboolean path_currently_selected,
                         gpointer attributes);
void wlibTreeViewShowIcon(GtkTreeView *tv);

int wTreeViewGetCount(wControl_p b);
void *wTreeViewGetItemContext(wControl_p b, int row);

void wlibTreeSelectionChanged(GtkTreeSelection *selection, void *context);

/* window.c */
wBool_t UpdateModifierKeyState(GdkEventKey *event);
wBool_t UpdateModifierKeyStateFromButton(GdkEventButton *event);
void wlibBasicGridAttach(wControl_p parent, GtkWidget *widget, unsigned xPos,
                         unsigned yPos, unsigned colSpan, unsigned rowSpan);
/* wpref.c */

/* wlibpaths.c */

#endif
