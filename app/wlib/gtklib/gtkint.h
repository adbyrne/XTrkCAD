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

/* gtkmisc.c */
void * gtkAlloc( wWin_p, wType_e, wPos_t, wPos_t, const char *, int, void * );
void gtkComputePos( wControl_p );
void gtkAddButton( wControl_p );
int gtkAddLabel( wControl_p, const char * );
void gtkControlGetSize( wControl_p );
struct accelData_t;
struct accelData_t * gtkFindAccelKey( GdkEventKey * event );
wBool_t gtkHandleAccelKey( GdkEventKey * );
wBool_t catch_shift_ctrl_alt_keys( GtkWidget *, GdkEventKey *, void * );
void gtkSetReadonly( wControl_p, wBool_t );
wControl_p gtkGetControlFromPos( wWin_p, wPos_t, wPos_t );
void gtkSetTrigger( wControl_p, setTriggerCallback_p );
GdkPixmap * gtkMakeIcon( GtkWidget *, wIcon_p, GdkBitmap ** );
char * gtkConvertInput( const char * );
char * gtkConvertOutput( const char * );

/* gtkwindow.c */
void gtkDoModal( wWin_p, wBool_t );

/* gtkhelp.c */
void load_into_view( char *, int );
void gtkAddHelpString( GtkWidget *, const char * );
void gtkHelpHideBalloon( void );

/* gtksimple.c */
void gtkDrawBox( wWin_p, wBoxType_e, wPos_t, wPos_t, wPos_t, wPos_t );
void gtkLineShow( wLine_p, wBool_t );

/* gktlist.c */
void gtkListShow( wList_p, wBool_t );
void gtkListSetPos( wList_p );
void gtkListActive( wList_p, wBool_t );
void gtkDropListPos( wList_p );

/* gtktext.c */
void gtkTextFreeze( wText_p );
void gtkTextThaw( wText_p );

/* gtkfont.c */
const char * gtkFontTranslate( wFont_p );
PangoLayout *gtkFontCreatePangoLayout( GtkWidget *, void *cairo,
									  wFont_p, wFontSize_t, const char *,
									  int *, int *, int *, int * );

/* gtkbutton.c */
void gtkButtonDoAction( wButton_p );
void gtkSetLabel( GtkWidget*, long, const char *, GtkLabel**, GtkWidget** );

/* gtkcolor.c */
void gtkGetColorMap( void );
GdkColor * gtkGetColor( wDrawColor, wBool_t );
int gtkGetColorChar( wDrawColor );
void gtkPrintColorMap( FILE *, int, int );
int gtkMapPixel( long );

/* psprint.c */
typedef struct {
		wIndex_t cmdOrFile;
		FILE * f;
		} wPrinterStream_t;
typedef wPrinterStream_t * wPrinterStream_p;

wPrinterStream_p wPrinterOpen( void );
void wPrinterWrite( wPrinterStream_p p, char * buff, int siz );
void wPrinterClose( wPrinterStream_p );
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


void ApplySettings( GtkPrintOperation *op );
void SaveSettings( GtkPrintOperation *op );
#endif
