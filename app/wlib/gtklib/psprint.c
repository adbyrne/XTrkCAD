/** \file print.c
 * Printing functions using GTK's print API
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2015 Martin Fischer
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <math.h>
#include <locale.h>

#include <stdint.h>

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>
#include <gtk/gtkprintjob.h>

#include "gtkint.h"
#include "wlib.h"
/* #include "dynarr.h" */
#include "i18n.h"

#ifndef TRUE
#define TRUE	(1)
#define FALSE	(0)
#endif

#define MM(m) ((m)/25.4)

extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;

/*****************************************************************************
 *
 * MACROS
 *
 */

#define PRINT_PORTRAIT  (0)
#define PRINT_LANDSCAPE (1)

#define min(a,b) ((a)<(b) ? (a) : (b))
#define PPI (72.0)
#define P2I( P ) ((P)/PPI)

#define DPI (1440.0)
#define D2I( D ) (((double)(D))/DPI)

#define CENTERMARK_LENGTH 60

#define WFONT		"WFONT"
#define WPRINTER	"WPRINTER"
#define WMARGIN		"WMARGIN"
#define WMARGINMAP	"WMARGINMAP"
#define WPRINTFONT	"WPRINTFONT"

/*****************************************************************************
 *
 * VARIABLES
 *
 */

#define PAGESETTINGS "xtrkcad.page"			/**< filename for page settings */
#define PRINTSETTINGS "xtrkcad.printer"		/**< filename for printer settings */
static GtkPrintSettings *settings;			/**< current printer settings */
static GtkPageSetup *page_setup;			/**< current paper settings */
static GtkPrinter *selPrinter;				/**< printer selected by user */
static GtkPrintJob *curPrintJob;			/**< currently active print job */

extern struct wDraw_t psPrint_d;

static wBool_t printContinue;
static wWin_p printAbortW;
static wMessage_p printAbortT;
static wMessage_p printAbortM;

static wWin_p printFileW;
static wWin_p newFontAliasW;
static wWin_p printSetupW;
static wList_p optPrinterB;
static wList_p optPaperSizeB;
static wMessage_p newFontAliasXFntB;
static wList_p optMarginB;
static wButton_p optMarginDelB;
static wFloat_p optTopMargin;
static wFloat_p optBottomMargin;
static wFloat_p optRightMargin;
static wFloat_p optLeftMargin;
static wChoice_p optFormat;
static wList_p optXFontL;
static wString_p optPSFontS;
static wFloat_p optFontSizeFactor;
static long optXFontX;
static const char * optXFont;
static char optPSFont[200];

static FILE * psFile;
static wPrinterStream_p psFileStream;
static wIndex_t pageCount;
static wIndex_t totalPageCount;

static long newPPrinter;
static long newPPaper;
static wPrintSetupCallBack_p printSetupCallBack;

static double paperWidth;		/**< physical paper width */
static double paperHeight;		/**< physical paper height */
static double tBorder;			/**< top margin */
static double rBorder;			/**< right margin */
static double lBorder;			/**< left margin */
static double bBorder;			/**< bottom margin */

static long printFormat = PRINT_LANDSCAPE;

static long curPrinter = 0;
static char *sPrintFileName;
static long curMargin = 0;

static const char * prefName;
static const char * prefPaper;
static const char * prefMargin;
static const char * prefFormat;

static char newMarginName[256];

static double fontSizeFactor = 1.0;

static struct {
		const char * name;
		double w, h;
		} papers[] = {
		{ "Letter", 8.5, 11.0 },
		{ "Legal", 8.5, 14.0 },
		{ "Tabloid", 11.0, 17.0 },
		{ "Ledger", 17.0, 11.0 },
		{ "Fan Fold", 13.2, 11.0 },
		{ "Statement", 5.5, 8.5 },
		{ "Executive", 7.5, 10.0 },
		{ "Folio", 8.27, 13 },
		{ "A0", MM(841), MM(1189) },
		{ "A1", MM(594), MM(841) },
		{ "A2", MM(420), MM(594) },
		{ "A3", MM(297), MM(420) },
		{ "A4", MM(210), MM(297) },
		{ "A5", MM(148), MM(210) },
		{ "A6", MM(105), MM(148) },
		{ "A7", MM(74), MM(105) },
		{ "A8", MM(52), MM(74) },
		{ "A9", MM(37), MM(52) },
		{ "A10", MM(26), MM(37) },
		{ "B0", MM(1000), MM(1414) },
		{ "B1", MM(707), MM(1000) },
		{ "B2", MM(500), MM(707) },
		{ "B3", MM(353), MM(500) },
		{ "B4", MM(250), MM(353) },
		{ "B5", MM(176), MM(250) },
		{ "B6", MM(125), MM(176) },
		{ "B7", MM(88), MM(125) },
		{ "B8", MM(62), MM(88) },
		{ "B9", MM(44), MM(62) },
		{ "B10", MM(31), MM(44) },
		{ "C0", MM(917), MM(1297) },
		{ "C1", MM(648), MM(917) },
		{ "C2", MM(458), MM(648) },
		{ "C3", MM(324), MM(458) },
		{ "C4", MM(229), MM(324) },
		{ "C5", MM(162), MM(229) },
		{ "C6", MM(114), MM(162) },
		{ "C7", MM(81), MM(114) },
		{ "DL", MM(110), MM(220) },
		{ NULL } };
wIndex_t curPaper = 0;

typedef struct {
		const char * name;
		const char * cmd;
		wIndex_t class;
		} printers_t;
dynArr_t printers_da;
#define printers(N) DYNARR_N(printers_t,printers_da,N)

typedef struct {
		const char * name;
		double t, b, r, l;
		} margins_t;
dynArr_t margins_da;
#define margins(N) DYNARR_N(margins_t,margins_da,N)

//static void printFileNameSel( void * junk );
static void printInit( void );

/*
 * Stuff related to determining the list of fonts used in the
 * Postscript file. A simple linked-list is used to implement a
 * stack. Everything is specialized to this application.
 */

/**
 * Nodes of the \a fontsUsed list.
 */
struct list_node {
  struct list_node *next;
  char *data;
} ;

/**
 * Pointer to the \a fontsUsed list.
 */
static struct list_node *fontsUsed = NULL;


/**
 * Pushes its argument on to the \a fontsUsed list.
 * \param item - IN pointer to a string to put on the list
 * \return nothing
 */
void fontsUsedPush( const char *item) {
  struct list_node *newitem;
  newitem = malloc(sizeof(struct list_node));
  if (newitem ==  NULL) exit (2);
  newitem->next=fontsUsed;
  newitem->data = strdup(item);
  if (newitem->data == NULL) exit(3);
  fontsUsed=newitem;
}

/**
 * Pops the top node from the \a fontsUsed list.
 * Note that  a pointer to the complete node is returned. The
 * caller is responsible for freeing both the data and the list
 * node when it is finished using them.
 * \return pointer to the list node.
 */
//struct list_node * fontsUsedPop() {
  //struct list_node *item;
  //if (fontsUsed == NULL) return NULL;
  //item = fontsUsed;
  //fontsUsed = item->next;
  //return item ;
//}

///**
 //* \a fontsUsed list (re-)initializer.
 //*/
//void fontsUsedInit() {
  //struct list_node *p;
  //while ((p=fontsUsedPop()) != NULL) {
    //free(p->data);
    //free(p);
  //}
  //fontsUsed=NULL;
//}

/**
 * Checks if \a s is already in \a fontsUsed list.
 * \param s - IN  string to be checked.
 * \return TRUE if found, FALSE if not.
 */
int fontsUsedContains( const char *s ) {
  struct list_node *ptr;
  ptr = fontsUsed;
  while ( ptr != NULL ) {
    if ( strcmp(s, ptr->data) == 0 ) return TRUE;
    ptr= ptr->next;
  }
  return FALSE ;
}

/**
 * Adds the \a fontName to the list of fonts being used.
 * Only if it is not already in the list.
 *
 * This function should be called anywhere the string "findfont"
 * is being emitted to the Postscript file.
 * \param \a fontName IN - string contaning the name to add.
 */
void addFontName( const char * fontName){
  if (fontsUsedContains(fontName)) return;
  fontsUsedPush(fontName);
}

/* ***************************************** */

/**
 * This function does a normal printf but uses the default C
 * locale as decimal separator.
 *
 * \param template IN printf-like format string
 * ... IN parameters according to format string
 * \return    describe the return value
 */

static void
psPrintf (FILE *ps, const char *template, ...)
{
	va_list ap;

  	setlocale( LC_NUMERIC, "C" );

   //va_start( ap, template );
   //vfprintf( ps, template, ap );
   //va_end( ap );

  	setlocale( LC_NUMERIC, "" );
}

/**
 * Page setup function
 *
 * \param callback IN unused?
 */

void wPrintSetup( wPrintSetupCallBack_p callback )
{
	GtkPageSetup *new_page_setup;
	gchar *filename;
	GError *err;
	GtkWidget *dialog;

	printInit( );

	ApplySettings( NULL );

	new_page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (gtkMainW->gtkwin),
														page_setup, settings);
	if (page_setup)
		g_object_unref (page_setup);

	page_setup = new_page_setup;
	SaveSettings( NULL );
}

static void pSetupOk( void )
{
	curPrinter = newPPrinter;
	curPaper = newPPaper;
	wWinShow( printSetupW, FALSE );
	wPrefSetString( "printer", "name", printers(curPrinter).name );
	wPrefSetString( "printer", "paper", papers[curPaper].name );
	if ( curMargin < margins_da.cnt )
		wPrefSetString( "printer", "margin", margins(curMargin).name );
	wPrefSetString( "printer", "format", (printFormat==PRINT_LANDSCAPE?"landscape":"portrait") );
	if (printSetupCallBack)
		printSetupCallBack( TRUE );
	wPrefSetFloat( WPRINTFONT, "factor", fontSizeFactor );
}

static void pSetupCancel( void )
{
	wWinShow( printSetupW, FALSE );
	if (printSetupCallBack)
		printSetupCallBack( FALSE );
}


/*****************************************************************************
 *
 * PRINTER LIST MANAGEMENT
 *
 */


static wBool_t wPrintNewPrinter(
		const char * name )
{
	char * cp;
    const char *cpEqual;

	printInit();
	DYNARR_APPEND( printers_t, printers_da, 10 );
	cpEqual = strchr( name, '=' );
	if (cpEqual == NULL) {
		printers(printers_da.cnt-1).cmd = strdup( "lpr -P%s" );
		printers(printers_da.cnt-1).name = name;
	} else {
		cp = strdup( name );
		cp[cpEqual-name] = 0;
		printers(printers_da.cnt-1).name = cp;
		printers(printers_da.cnt-1).cmd = cp+(cpEqual-name+1);
		name = cp;
	}
	if (optPrinterB) {
		wListAddValue( optPrinterB, printers(printers_da.cnt-1).name, NULL, (void*)(intptr_t)(printers_da.cnt-1) );
		if ( prefName && strcasecmp( prefName, name ) == 0 ) {
			curPrinter = printers_da.cnt-1;
			wListSetIndex( optPrinterB, curPrinter );
		}
	}
	return TRUE;
}


static void doMarginSel(
		wIndex_t inx,
		const char * name,
		wIndex_t op,
		void * listData,
		void * itemData )
{
	margins_t * p;
	static margins_t dummy = { "", 0, 0, 0, 0 };
	if ( inx < 0 ) {
		for ( inx=0,p=&margins(0); inx<margins_da.cnt; inx++,p++ ) {
			if ( strcasecmp( name, margins(inx).name ) == 0 )
				break;
		}
		if ( inx >= margins_da.cnt ) {
			strncpy( newMarginName, name, sizeof newMarginName );
			p = &dummy;
		}
	} else {
		p = &margins(inx);
	}
	curMargin = inx;
	tBorder = p->t;
	bBorder = p->b;
	rBorder = p->r;
	lBorder = p->l;
	wFloatSetValue( optTopMargin, tBorder );
	wFloatSetValue( optBottomMargin, bBorder );
	wFloatSetValue( optRightMargin, rBorder );
	wFloatSetValue( optLeftMargin, lBorder );
}

static wIndex_t wPrintNewMargin(
		const char * name,
		const char * value )
{
	margins_t * m;
	int rc;
	DYNARR_APPEND( margins_t, margins_da, 10 );
	m = &margins(margins_da.cnt-1);

	setlocale( LC_NUMERIC, "C" );
	if ((rc=sscanf( value, "%lf %lf %lf %lf", &m->t, &m->b, &m->r, &m->l ))!=4) {
		margins_da.cnt--;
		setlocale( LC_NUMERIC, "" );
		return FALSE;
	}
	setlocale( LC_NUMERIC, "" );

	m->name = strdup( name );
	if (optMarginB)
		wListAddValue( optMarginB, name, NULL, NULL );
	if ( prefMargin && strcasecmp( prefMargin, name ) == 0 ) {
		curMargin = margins_da.cnt-1;
		wListSetIndex( optMarginB, curMargin );
		tBorder = m->t;
		bBorder = m->b;
		rBorder = m->r;
		lBorder = m->l;
		wFloatSetValue( optTopMargin, tBorder );
		wFloatSetValue( optBottomMargin, bBorder );
		wFloatSetValue( optRightMargin, rBorder );
		wFloatSetValue( optLeftMargin, lBorder );
	}
	return TRUE;
}


static void doChangeMargin( void )
{
	static char marginValue[256];
	margins_t * m;
	sprintf( marginValue, "%0.3f %0.3f %0.3f %0.3f", tBorder, bBorder, rBorder, lBorder );
	if ( curMargin >= margins_da.cnt ) {
		DYNARR_APPEND( margins_t, margins_da, 10 );
		curMargin = margins_da.cnt-1;
		margins(curMargin).name = strdup( newMarginName );
		wListAddValue( optMarginB, margins(curMargin).name, NULL, NULL );
		wListSetIndex( optMarginB, curMargin );
	}
	m = &margins(curMargin);
	m->t = tBorder;
	m->b = bBorder;
	m->r = rBorder;
	m->l = lBorder;
	wPrefSetString( WMARGIN, m->name, marginValue );
}


static void doMarginDelete( void )
{
	int inx;
	if ( curMargin >= margins_da.cnt || margins_da.cnt <= 1 || curMargin == 0 )
		return;
	wPrefSetString( WMARGIN, margins(curMargin).name, "" );
	free( (char*)margins(curMargin).name );
	for ( inx=curMargin+1; inx<margins_da.cnt; inx++ )
		margins(inx-1) = margins(inx);
	margins_da.cnt--;
	wListDelete( optMarginB, curMargin );
	if ( curMargin >= margins_da.cnt )
		curMargin--;
	doMarginSel( curMargin, margins(curMargin).name, 0, NULL, NULL );
}


static const char * curPsFont = NULL;
static const char * curXFont = NULL;


static void newFontAliasSel( const char * alias, void * data )
{
	wPrefSetString( WFONT, curXFont, alias );
	curPsFont = wPrefGetString( WFONT, curXFont );
	wWinShow( newFontAliasW, FALSE );
	wListAddValue( optXFontL, curXFont, NULL, NULL );
}



/*****************************************************************************
 *
 * BASIC PRINTING
 *
 */


/**
 * set the current line type for printing operations
 *
 * \param lineWidth IN new line width
 * \param lineType IN flag for line type (dashed or full)
 * \param opts IN unused
 * \return
 */


static void setLineType(
		double lineWidth,
		wDrawLineType_e lineType,
		wDrawOpts opts )
{
	cairo_t *cr = psPrint_d.printContext;
	double dashLength = 2.0;

	if (lineWidth < 0.0) {
		lineWidth = P2I(-lineWidth)*2.0;
	}

	// make sure that there is a minimum line width used
	if ( lineWidth == 0.0 )
		lineWidth = 1.0;

	/** \todo need to find out the correct units for line width */
	cairo_set_line_width( cr, lineWidth );

	if (lineType == wDrawLineDash)
		cairo_set_dash( cr, NULL, 0, 0.0 );
}

/**
 * set the color for the following print operations
 *
 * \param color IN the new color
 * \return
 */

static void psSetColor(
		wDrawColor color )
{
	cairo_t *cr = psPrint_d.printContext;
	GdkColor* const gcolor = gtkGetColor(color, TRUE);

	cairo_set_source_rgb(cr, gcolor->red / 65535.0,
							 gcolor->green / 65535.0,
							 gcolor->blue / 65535.0);
}


void psPrintLine(
		wPos_t x0, wPos_t y0,
		wPos_t x1, wPos_t y1,
		wDrawWidth width,
		wDrawLineType_e lineType,
		wDrawColor color,
		wDrawOpts opts )
{
	if (color == wDrawColorWhite)
		return;
	if (opts&wDrawOptTemp)
		return;
	psSetColor(color);
	setLineType( width, lineType, opts );
	cairo_move_to( psPrint_d.printContext,
					x0, paperHeight * psPrint_d.dpi - y0 );
	cairo_line_to( psPrint_d.printContext,
					x1, paperHeight * psPrint_d.dpi - y1 );
	cairo_stroke( psPrint_d.printContext );
}

/**
 * Print an arc around a specified center
 *
 * \param x0, y0 IN  center of arc
 * \param r IN radius
 * \param angle0, angle1 IN start and end angle
 * \param drawCenter draw marking for center
 * \param width line width
 * \param lineType
 * \param color color
 * \param opts ?
 */

void psPrintArc(
		wPos_t x0, wPos_t y0,
		wPos_t r,
		double angle0,
		double angle1,
		wBool_t drawCenter,
		wDrawWidth width,
		wDrawLineType_e lineType,
		wDrawColor color,
		wDrawOpts opts )
{
	cairo_t *cr = psPrint_d.printContext;

	if (color == wDrawColorWhite)
		return;
	if (opts&wDrawOptTemp)
		return;

	psSetColor(color);
	setLineType(width, lineType, opts);

	if (angle1 >= 360.0)
		angle1 = 359.999;
	angle1 = 90.0-(angle0+angle1);
	while (angle1 < 0.0) angle1 += 360.0;
	while (angle1 >= 360.0) angle1 -= 360.0;
	angle0 = 90.0-angle0;
	while (angle0 < 0.0) angle0 += 360.0;
	while (angle0 >= 360.0) angle0 -= 360.0;

	// draw the curve
	cairo_arc( cr, x0, y0, r, angle1 * M_PI / 180.0, angle0 * M_PI / 180.0 );

	if( drawCenter ) {
		// draw crosshair for center of curve
		cairo_move_to( cr, x0 - CENTERMARK_LENGTH / 2, y0 );
		cairo_line_to( cr, x0 + CENTERMARK_LENGTH / 2, y0 );
		cairo_move_to( cr, x0, y0 - CENTERMARK_LENGTH / 2 );
		cairo_line_to( cr, x0, y0 + CENTERMARK_LENGTH / 2 );
	}
	cairo_stroke( psPrint_d.printContext );
}


void psPrintFillRectangle(
		wPos_t x0, wPos_t y0,
		wPos_t x1, wPos_t y1,
		wDrawColor color,
		wDrawOpts opts )
{
	if (color == wDrawColorWhite)
		return;
	if (opts&wDrawOptTemp)
		return;
	psSetColor(color);
	psPrintf(psFile,
				"%0.3f %0.3f moveto %0.3f %0.3f lineto closepath fill\n",
				D2I(x0), D2I(y0), D2I(x1), D2I(y1) );
}

/**
 * Print a filled polygon
 *
 * \param p IN a list of x and y coordinates
 * \param cnt IN the number of ponts
 * \param color IN fill color
 * \param opts IN options
 * \return
 */

void psPrintFillPolygon(
		wPos_t p[][2],
		int cnt,
		wDrawColor color,
		wDrawOpts opts )
{
	int inx;
	cairo_t *cr = psPrint_d.printContext;

	if (color == wDrawColorWhite)
		return;
	if (opts&wDrawOptTemp)
		return;

	psSetColor(color);

	cairo_move_to( cr, p[ 0 ][ 0 ], p[ 0 ][ 1 ] );
	for (inx=0; inx<cnt; inx++)
		cairo_line_to( cr, p[ inx ][ 0 ], p[ inx ][ 1 ] );
	cairo_fill( cr );
}


void psPrintFillCircle(
		wPos_t x0, wPos_t y0,
		wPos_t r,
		wDrawColor color,
		wDrawOpts opts )
{
	if (color == wDrawColorWhite)
		return;
	if (opts&wDrawOptTemp)
		return;
	psSetColor(color);
	psPrintf(psFile,
		"newpath %0.3f %0.3f %0.3f 0.0 360.0 arc fill\n",
		D2I(x0), D2I(y0), D2I(r) );
}


/**
 * Print a string at the given position using specified font and text size.
 *
 * \param x IN x position
 * \param y IN y position
 * \param a IN angle of baseline in degrees. Positive is clockwise, 0 is direction of positive x axis
 * \param s IN string to print
 * \param fp IN font
 * \param fs IN font size
 * \param color IN text color
 * \param opts IN ???
 * \return
 */

void psPrintString(
		wPos_t x, wPos_t y,
		double a,
		char * s,
		wFont_p fp,
		double fs,
		wDrawColor color,
		wDrawOpts opts )
{
	char * cp;
	cairo_t *cr;
	PangoLayout *layout;
	PangoFontDescription *desc;
	double text_height, text_width, w, h;
	int height, width;
	GdkColor* const gcolor = gtkGetColor(color, TRUE);

	if (color == wDrawColorWhite)
		return;

	cr = psPrint_d.printContext;

	cairo_save( cr );

	cairo_translate( cr, x, paperHeight * psPrint_d.dpi - y  );
	cairo_rotate( cr, -a * M_PI / 180.0  );

	layout = pango_cairo_create_layout( cr );
	/** \todo use a getter function instead of double conversion */
	desc = pango_font_description_from_string (gtkFontTranslate( fp ));
	pango_font_description_set_size(desc, fs * PANGO_SCALE);

	/*
	 *
	 */

	pango_layout_set_font_description (layout, desc);
	pango_layout_set_text (layout, s, -1);
	pango_layout_set_width (layout, -1);
	pango_layout_set_alignment (layout, PANGO_ALIGN_LEFT);
	pango_layout_get_size (layout, &width, &height);
	text_height = (gdouble) height / PANGO_SCALE;
	text_width = (gdouble) width / PANGO_SCALE;

	cairo_set_source_rgb(cr, gcolor->red / 65535.0, gcolor->green / 65535.0, gcolor->blue / 65535.0);

	pango_cairo_show_layout (cr, layout);
	cairo_restore( cr );
}

void wPrintClip( wPos_t x, wPos_t y, wPos_t w, wPos_t h )
{
	psPrintf( psFile, "\
%0.3f %0.3f moveto \n\
%0.3f %0.3f lineto \n\
%0.3f %0.3f lineto \n\
%0.3f %0.3f lineto \n\
closepath clip newpath\n",
		D2I(x), D2I(y),
		D2I(x+w), D2I(y),
		D2I(x+w), D2I(y+h),
		D2I(x), D2I(y+h) );
}

/*****************************************************************************
 *
 * PAGE FUNCTIONS
 *
 */

/**
 * Get the paper dimensions and margins and setup the internal variables
 * \return
 */

static void
WlibGetPaperSize( void )
{
	bBorder = gtk_page_setup_get_bottom_margin( page_setup, GTK_UNIT_INCH );
	tBorder = gtk_page_setup_get_top_margin( page_setup, GTK_UNIT_INCH );
	lBorder = gtk_page_setup_get_left_margin( page_setup, GTK_UNIT_INCH );
	rBorder = gtk_page_setup_get_right_margin( page_setup, GTK_UNIT_INCH );
	paperHeight = gtk_page_setup_get_paper_height( page_setup, GTK_UNIT_INCH );
	paperWidth = gtk_page_setup_get_paper_width( page_setup, GTK_UNIT_INCH );
}

/**
 * Initialize printer und paper selection using the saved settings
 *
 * \param op IN print operation to initialize. If NULL only the global
 * 				settings are loaded.
 */

void
ApplySettings( GtkPrintOperation *op )
{
	gchar *filename;
	GError *err = NULL;
	GtkWidget *dialog;

	filename = g_build_filename( wGetAppWorkDir(), PRINTSETTINGS, NULL );

	if( !(settings = gtk_print_settings_new_from_file( filename, &err ))) {
		if( err->code != G_FILE_ERROR_NOENT ) {
			// ignore file not found error as defaults will be used
			dialog = gtk_message_dialog_new (GTK_WINDOW (gtkMainW->gtkwin),
											 GTK_DIALOG_DESTROY_WITH_PARENT,
											 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                             err->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
		g_error_free (err);
	}
	g_free( filename );

	if (settings && op )
			gtk_print_operation_set_print_settings (op, settings);

	err = NULL;
	filename = g_build_filename( wGetAppWorkDir(), PAGESETTINGS, NULL );
	if( !(page_setup = gtk_page_setup_new_from_file( filename, &err ))) {
		// ignore file not found error as defaults will be used
		if( err->code != G_FILE_ERROR_NOENT ) {
			dialog = gtk_message_dialog_new (GTK_WINDOW (gtkMainW->gtkwin),
											 GTK_DIALOG_DESTROY_WITH_PARENT,
											 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                             err->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		}
		g_error_free (err);
	} else {
		// on success get the paper dimensions
		bBorder = gtk_page_setup_get_bottom_margin( page_setup, GTK_UNIT_INCH );
		tBorder = gtk_page_setup_get_top_margin( page_setup, GTK_UNIT_INCH );
		lBorder = gtk_page_setup_get_left_margin( page_setup, GTK_UNIT_INCH );
		rBorder = gtk_page_setup_get_right_margin( page_setup, GTK_UNIT_INCH );
		paperHeight = gtk_page_setup_get_paper_height( page_setup, GTK_UNIT_INCH );
		paperWidth = gtk_page_setup_get_paper_width( page_setup, GTK_UNIT_INCH );
	}
	g_free( filename );

	if( page_setup && op )
		gtk_print_operation_set_default_page_setup (op, page_setup);

}

/**
 * Save the printer settings. If op is not NULL the settings are retrieved
 * from the print operation. Otherwise the state of the globals is saved.
 *
 * \param op IN printer operation. If NULL the glabal variables are used
 */

void
SaveSettings( GtkPrintOperation *op )
{
	GError *err = NULL;
	gchar *filename;
	GtkWidget *dialog;

	if( op ) {
		if (settings != NULL)
			g_object_unref (settings);
		settings = g_object_ref (gtk_print_operation_get_print_settings (op));
	}
    filename = g_build_filename( wGetAppWorkDir(), PRINTSETTINGS, NULL );
    if( !gtk_print_settings_to_file( settings, filename, &err )) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (gtkMainW->gtkwin),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                             err->message);

		g_error_free (err);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	g_free( filename );

	if( op ) {
		if (page_setup != NULL)
			g_object_unref (page_setup);
		page_setup = g_object_ref (gtk_print_operation_get_default_page_setup (op));
	}
    filename = g_build_filename( wGetAppWorkDir(), PAGESETTINGS, NULL );
    if( !gtk_page_setup_to_file( page_setup, filename, &err )) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (gtkMainW->gtkwin),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                             err->message);

		g_error_free (err);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	g_free( filename );

}

/**
 * Get the paper size. The size returned is the printable area of the
 * currently selected paper, ie. the physical size minus the margins.
 * \param w OUT printable width of the paper in inches
 * \param h OUT printable height of the paper in inches
 * \return
 */

void wPrintGetPageSize(
		double * w,
		double * h )
{
	// if necessary load the settings
	if( !settings )
		ApplySettings( NULL );

	WlibGetPaperSize();

	*w = paperWidth -lBorder - rBorder;
	*h = paperHeight - tBorder - bBorder;
}

/**
 * Get the paper size. The size returned is the physical size of the
 * currently selected paper.
 * \param w OUT physical width of the paper in inches
 * \param h OUT physical height of the paper in inches
 * \return
 */

void wPrintGetPhysSize(
		double * w,
		double * h )
{
	// if necessary load the settings
	if( !settings )
		ApplySettings( NULL );

	WlibGetPaperSize();

	*w = paperWidth;
	*h = paperHeight;
}


static void printAbort( void * context )
{
	printContinue = FALSE;
	wWinShow( printAbortW, FALSE );
}

/**
 * Initialize new page.
 *
 * \return   print context for the print operation
 */
wDraw_p wPrintPageStart( void )
{
	pageCount++;

	return &psPrint_d;
}

/**
 * End of page
 *
 * \param p IN ignored
 * \return    always printContinue
 */


wBool_t wPrintPageEnd( wDraw_p p )
{
	cairo_show_page( psPrint_d.printContext );

	return printContinue;
}

/*****************************************************************************
 *
 * PRINT START/END
 *
 */

/**
 * Allow the user to enter a new file name and location for the file.
 * Thanks to Andrew Krause's great book Foundations of GTK+ Development
 * for this code snippet.
 *
 * \param junk IN ignored
 */

//static void printFileNameSel( void * junk )
//{
	//GtkWidget *dialog;
	//gchar *filename;
	//gint result;

	//dialog = gtk_file_chooser_dialog_new (_("Print to file ..."), (GtkWindow *)printSetupW->gtkwin,
								//GTK_FILE_CHOOSER_ACTION_SAVE,
								//GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
								//GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
								//NULL);

	//result = gtk_dialog_run (GTK_DIALOG (dialog));
	//if (result == GTK_RESPONSE_ACCEPT)
	//{
		//filename = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( dialog ));
		//if( filename ) {
			//sPrintFileName = malloc( strlen( filename ) + 1 );
			//if( sPrintFileName ) {
				//strcpy( sPrintFileName, filename );
			//}
			//else {
				//fputs( "Insufficient memory for printing to file\n", stderr );
				//abort();
			//}
			//g_free( filename );
		//}
	//}

	//gtk_widget_destroy (dialog);
//}

/**
 * Start a new document
 *
 * \param title IN title of document ( name of layout )
 * \param fTotalPageCount IN number of pages to print (unused)
 * \param copiesP OUT ???
 * \return TRUE if successful, FALSE if cancelled by user
 */

wBool_t wPrintDocStart( const char * title, int fTotalPageCount, int * copiesP )
{
	GtkWidget *printDialog;
	gint res;
	cairo_surface_type_t surface_type;

	printDialog = gtk_print_unix_dialog_new( title, GTK_WINDOW(gtkMainW->gtkwin));

	// load the settings
	ApplySettings( NULL );

	// and apply them to the printer dialog
	gtk_print_unix_dialog_set_settings( (GtkPrintUnixDialog *)printDialog, settings );
	gtk_print_unix_dialog_set_page_setup( (GtkPrintUnixDialog *)printDialog, page_setup );

	res = gtk_dialog_run( (GtkDialog *)printDialog );
	if( res == GTK_RESPONSE_OK ) {
		selPrinter = gtk_print_unix_dialog_get_selected_printer( (GtkPrintUnixDialog *)printDialog );

		if( settings )
			g_object_unref (settings);
		settings = gtk_print_unix_dialog_get_settings( (GtkPrintUnixDialog *)printDialog );

		if( page_setup )
			g_object_unref( page_setup );
		page_setup = gtk_print_unix_dialog_get_page_setup( (GtkPrintUnixDialog *)printDialog );

		curPrintJob = gtk_print_job_new( title,
							 selPrinter,
							 settings,
							 page_setup );

		psPrint_d.curPrintSurface = gtk_print_job_get_surface( curPrintJob,
								   NULL );
		psPrint_d.printContext = cairo_create( psPrint_d.curPrintSurface );
//		g_object_unref( curPrintSurface );

		//update the paper dimensions
		WlibGetPaperSize();

		/* for the file based surfaces the resolution is 72 dpi (see documentation) */
		surface_type = cairo_surface_get_type( psPrint_d.curPrintSurface );
		if( surface_type == CAIRO_SURFACE_TYPE_PDF ||
			surface_type == CAIRO_SURFACE_TYPE_PS  ||
			surface_type == CAIRO_SURFACE_TYPE_SVG )
			psPrint_d.dpi = 72;
		else
			psPrint_d.dpi = (double)gtk_print_settings_get_resolution( settings );

		SaveSettings( NULL );
	}
	gtk_widget_destroy (printDialog);

	if (copiesP)
		*copiesP = 1;

	if( res != GTK_RESPONSE_OK )
		return FALSE;
	else
		return TRUE;
}

/**
 * Callback for job finished event. Destroys the cairo context.
 *
 * \param job IN unused
 * \param data IN unused
 * \param err IN if != NULL, an error dialog ist displayed
 * \return
 */

void
doPrintJobFinished( GtkPrintJob *job, void *data, GError *err )
{
	GtkWidget *dialog;

	cairo_destroy( psPrint_d.printContext );
	if( err ) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (gtkMainW->gtkwin),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				                             err->message);

		g_error_free (err);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}
/**
 * Finish the print operation
 * \return
 */

void wPrintDocEnd( void )
{
	cairo_surface_finish( psPrint_d.curPrintSurface );

	gtk_print_job_send( curPrintJob,
						doPrintJobFinished,
						NULL,
						NULL );

//	wWinShow( printAbortW, FALSE );
}


wBool_t wPrintQuit( void )
{
	return FALSE;
}


static void pLine( double x0, double y0, double x1, double y1 )
{
	psPrintf( psFile, "%0.3f %0.3f moveto %0.3f %0.3f lineto stroke\n",
		x0, y0, x1, y1 );
}

/**
 * Generate a test page that helps setting up printer margins.
 */

static void pTestPage( void )
{
	double w, h;
	long oldPrinter;
	int i, j, k, run;
	double x0, x1, y0, y1;
	const char * psFont, * xFont;
	long curMargin0;

	oldPrinter = curPrinter;
	curPrinter = newPPrinter;
	curMargin0 = curMargin;
	curMargin = 0;
	wPrintDocStart( _("Printer Margin Test Page"), 1, NULL );
	wPrintPageStart();
	curMargin = curMargin0;
	w = papers[curPaper].w;
	h = papers[curPaper].h;
	if ( psFile == NULL )
		return;

#define MAXIMUM (100)

	psPrintf( psFile, "/Times-Roman findfont 0.06 scalefont setfont\n" );
	addFontName("Times-Roman");
	for ( i=5; i<=MAXIMUM; i+=5 ) {
		x0 = ((double)i)/100;
		pLine( 0.5, x0, w-0.5, x0 );
		pLine( 0.5, h-x0, w-0.5, h-x0 );
		pLine( x0, 0.5, x0, h-0.5 );
		pLine( w-x0, 0.5, w-x0, h-0.5 );

		psPrintf( psFile, "%0.3f %0.3f moveto (%0.2f) show\n",
				1.625 + x0*5 - 0.05, 0.2+MAXIMUM/100.0, x0 );
		pLine( 1.625 + x0*5, (0.2+MAXIMUM/100.0), 1.625 + x0*5, x0 );
		psPrintf( psFile, "%0.3f %0.3f moveto (%0.2f) show\n",
				1.625 + x0*5 - 0.05, h-(0.2+MAXIMUM/100.0)-0.05, x0 );
		pLine( 1.625 + x0*5, h-(0.2+MAXIMUM/100.0), 1.625 + x0*5, h-x0 );

		psPrintf( psFile, "%0.3f %0.3f moveto (%0.2f) show\n",
				(0.2+MAXIMUM/100.0), 1.625 + x0*5-0.020, x0 );
		pLine( (0.2+MAXIMUM/100.0), 1.625 + x0*5, x0, 1.625 + x0*5 );
		psPrintf( psFile, "%0.3f %0.3f moveto (%0.2f) show\n",
				w-(0.2+MAXIMUM/100.0)-0.10, 1.625 + x0*5-0.020, x0 );
		pLine( w-(0.2+MAXIMUM/100.0), 1.625 + x0*5, w-x0, 1.625 + x0*5 );
	}

	psPrintf( psFile, "/Times-Bold findfont 0.20 scalefont setfont\n" );
	addFontName("Times-Bold");
	psPrintf( psFile, "%0.3f %0.3f moveto (%s) show\n", 2.0, h-2.0, "Printer Margin Setup" );
	psPrintf( psFile, "/Times-Roman findfont 0.12 scalefont setfont\n" );
	addFontName("Times-Roman");
	psPrintf( psFile, "%0.3f %0.3f moveto (%s) show\n", 2.0, h-2.15,
		"Enter the position of the first visible line for each margin on the Printer Setup dialog");
	if ( curMargin < margins_da.cnt )
		psPrintf( psFile, "%0.3f %0.3f moveto ("
			"Current margins for the %s printer are: Top: %0.3f, Left: %0.3f, Right: %0.3f, Bottom: %0.3f"
			") show\n", 2.0, h-2.30,
			margins(curMargin).name, margins(curMargin).t, margins(curMargin).l, margins(curMargin).r, margins(curMargin).b );


	psPrintf( psFile, "/Times-Bold findfont 0.20 scalefont setfont\n" );
	addFontName("Times-Bold");
	psPrintf( psFile, "%0.3f %0.3f moveto (%s) show\n", 2.0, h-3.0, "Font Map" );
	for (i=j=0; 0.2*j < h-5.0 && (psFont = wPrefGetSectionItem( WFONT, &i, &xFont )) != NULL; j++ ) {
		if ( psFont[0] == '\0' ) continue;
		psPrintf( psFile, "/Times-Roman findfont 0.12 scalefont setfont\n" );
		addFontName("Times-Roman");
		psPrintf( psFile, "%0.3f %0.3f moveto (%s -> %s) show\n", 2.0, h-3.15-0.15*j, xFont, psFont );
		psPrintf( psFile, "/%s findfont 0.12 scalefont setfont\n", psFont );
		addFontName(psFont);
		psPrintf( psFile, "%0.3f %0.3f moveto (%s) show\n", 5.5, h-3.15-0.15*j, "ABCD wxyz 0123 -+$!" );
	}
	x0 = 0.5;
	run = TRUE;
	i = 0;
	while (run) {
		x1 = x0 + 0.25;
		if (x1 >= w-0.5) {
			x1 = w-0.5;
			run = FALSE;
		}
		for ( j = 1; j<5; j++ ) {
			y0 = ((double)(i+j))/100;
			for (k=0; k<MAXIMUM/25; k++) {
				pLine( x0, y0+k*0.25, x1, y0+k*0.25 );
				pLine( x0, h-y0-k*0.25, x1, h-y0-k*0.25 );
			}
		}
		x0 += 0.25;
		i += 5;
		if (i >= 25)
			i = 0;
	}

	y0 = 0.5;
	run = TRUE;
	i = 0;
	while (run) {
		y1 = y0 + 0.25;
		if (y1 >= h-0.5) {
			y1 = h-0.5;
			run = FALSE;
		}
		for ( j = 1; j<5; j++ ) {
			x0 = ((double)(i+j))/100;
			for (k=0; k<MAXIMUM/25; k++) {
			   pLine( x0+k*0.25, y0, x0+k*0.25, y1 );
			   pLine( w-x0-k*0.25, y0, w-x0-k*0.25, y1 );
			}
		}
		y0 += 0.25;
		i += 5;
		if (i >= 25)
		   i = 0;
	}

	/*	psPrintf( psFile, "showpage\n"); */
	wPrintPageEnd(NULL);
	wPrintDocEnd();
	curPrinter = oldPrinter;
}



static wLines_t lines[] = {
		{ 1,  25,  11,  95,  11 },
		{ 1,  95,  11,  95, 111 },
		{ 1,  95, 111,  25, 111 },
		{ 1,  25, 111,  25,  11 }};
static const char * printFmtLabels[]  = { N_("Portrait"), N_("Landscape"), NULL };

static struct {
		const char * xfontname, * psfontname;
		} fontmap[] = {
		{ "times-medium-r", "Times-Roman" },
		{ "times-medium-i", "Times-Italic" },
		{ "times-bold-r", "Times-Bold" },
		{ "times-bold-i", "Times-BoldItalic" },
		{ "helvetica-medium-r", "Helvetica" },
		{ "helvetica-medium-o", "Helvetica-Oblique" },
		{ "helvetica-bold-r", "Helvetica-Bold" },
		{ "helvetica-bold-o", "Helvetica-BoldOblique" },
		{ "courier-medium-r", "Courier" },
		{ "courier-medium-o", "Courier-Oblique" },
		{ "courier-medium-i", "Courier-Oblique" },
		{ "courier-bold-r", "Courier-Bold" },
		{ "courier-bold-o", "Courier-BoldOblique" },
		{ "courier-bold-i", "Courier-BoldOblique" },
		{ "avantgarde-book-r", "AvantGarde-Book" },
		{ "avantgarde-book-o", "AvantGarde-BookOblique" },
		{ "avantgarde-demi-r", "AvantGarde-Demi" },
		{ "avantgarde-demi-o", "AvantGarde-DemiOblique" },
		{ "palatino-medium-r", "Palatino-Roman" },
		{ "palatino-medium-i", "Palatino-Italic" },
		{ "palatino-bold-r", "Palatino-Bold" },
		{ "palatino-bold-i", "Palatino-BoldItalic" },
		{ "new century schoolbook-medium-r", "NewCenturySchlbk-Roman" },
		{ "new century schoolbook-medium-i", "NewCenturySchlbk-Italic" },
		{ "new century schoolbook-bold-r", "NewCenturySchlbk-Bold" },
		{ "new century schoolbook-bold-i", "NewCenturySchlbk-BoldItalic" },
		{ "zapfchancery-medium-i", "ZapfChancery-MediumItalic" } };

static struct {
		const char * name, * value;
		} pagemargins [] = {
		 { "None", "0.00 0.00 0.00 0.00" },
		 { "BJC-600", "0.10 0.44 0.38 0.13" },
		 { "DeskJet", "0.167 0.50 0.25 0.25" },
		 { "PaintJet", "0.167 0.167 0.167 0.167" },
		 { "DJ505", "0.25 0.668 0.125 0.125" },
		 { "DJ560C", "0.37 0.46 0.25 0.25" },
		 { "LaserJet", "0.43 0.21 0.43 0.28" } };


static void doSetOptXFont(
	wIndex_t inx,
	const char * xFont,
	wIndex_t inx2,
	void * itemData,
	void * listData )
{
	const char * cp;
	optXFont = xFont;
	cp = wPrefGetString( WFONT, xFont );
	if ( !cp )
		cp = "";
	wStringSetValue( optPSFontS, cp );
}


static void doSetOptPSFont(
	const char * psFont,
	void * data )
{
	if ( optXFont &&
		 psFont[0] )
		wPrefSetString( WFONT, optXFont, psFont );
}


static void printInit( void )
{
	wIndex_t i;
	wPos_t x, y;
	static wBool_t printInitted = FALSE;
	const char * cp, * cq;
    char num[10];

	if (printInitted)
		return;

	printInitted = TRUE;
	prefName = wPrefGetString( "printer", "name" );
	prefPaper = wPrefGetString( "printer", "paper" );
	prefMargin = wPrefGetString( "printer", "margin" );
	prefFormat = wPrefGetString( "printer", "format" );
	if (prefFormat && strcasecmp(prefFormat, "landscape") == 0)
		printFormat = PRINT_LANDSCAPE;
	else
		printFormat = PRINT_PORTRAIT;
	wPrefGetFloat( WPRINTFONT, "factor", &fontSizeFactor, 1.0 );
	if ( fontSizeFactor < 0.5 || fontSizeFactor > 2.0 ) {
		fontSizeFactor = 1.0;
		wPrefSetFloat( WPRINTFONT, "factor", fontSizeFactor );
	}

	x = wLabelWidth( _("Paper Size") )+4;
	printSetupW = wWinPopupCreate( NULL, 4, 4, "printSetupW",  _("Print Setup"), "xvprintsetup", F_AUTOSIZE|F_RECALLPOS, NULL, NULL );
	optPrinterB = wDropListCreate( printSetupW, x, -4, "printSetupPrinter", _("Printer"), 0, 4, 100, &newPPrinter, NULL, NULL );

	optPaperSizeB = wDropListCreate( printSetupW, x, -4, "printSetupPaper", _("Paper Size"), 0, 4, 100, &newPPaper, NULL, NULL );
	y = wControlGetPosY( (wControl_p)optPaperSizeB ) + wControlGetHeight( (wControl_p)optPaperSizeB ) + 10;
	for ( i=0; i<sizeof lines / sizeof lines[0]; i++ ) {
		lines[i].x0 += x;
		lines[i].x1 += x;
		lines[i].y0 += y;
		lines[i].y1 += y;
	}
	wLineCreate( printSetupW, NULL, sizeof lines / sizeof lines[0], lines );
	optTopMargin = wFloatCreate( printSetupW, x+35,  y, "printSetupMargin", NULL, 0, 50, 0.0, 1.0, &tBorder, (wFloatCallBack_p)doChangeMargin, NULL );
	optLeftMargin = wFloatCreate( printSetupW,  x, y+50, "printSetupMargin", _("Margin"), 0, 50, 0.0, 1.0, &lBorder, (wFloatCallBack_p)doChangeMargin, NULL );
	optRightMargin = wFloatCreate( printSetupW, x+70, y+50, "printSetupMargin", NULL, 0, 50, 0.0, 1.0, &rBorder, (wFloatCallBack_p)doChangeMargin, NULL );
	optBottomMargin = wFloatCreate( printSetupW, x+35, y+100, "printSetupMargin", NULL, 0, 50, 0.0, 1.0, &bBorder, (wFloatCallBack_p)doChangeMargin, NULL );
	optMarginB = wDropListCreate( printSetupW, x, -5, "printSetupMargin", NULL, BL_EDITABLE, 4, 100, NULL, doMarginSel, NULL );
	optMarginDelB = wButtonCreate( printSetupW, wControlGetPosX((wControl_p)optMarginB)+wControlGetWidth((wControl_p)optMarginB)+5, wControlGetPosY((wControl_p)optMarginB), "printSetupMarginDelete", "Delete", 0, 0, (wButtonCallBack_p)doMarginDelete, NULL );

	optFormat = wRadioCreate( printSetupW, x, -5, "printSetupFormat", _("Format"), BC_HORZ,
				printFmtLabels, &printFormat, NULL, NULL );
	optXFontL = wDropListCreate( printSetupW, x, -6, "printSetupXFont", _("X Font"), 0, 4, 200, &optXFontX, doSetOptXFont, NULL );
	optPSFontS = wStringCreate( printSetupW, x, -4, "printSetupPSFont", _("PS Font"), 0, 200, optPSFont, 0, doSetOptPSFont, NULL );
	optFontSizeFactor = wFloatCreate( printSetupW, x, -4, "printSetupFontSizeFactor", _("Factor"), 0, 50, 0.5, 2.0, &fontSizeFactor, (wFloatCallBack_p)NULL, NULL );
	y = wControlGetPosY( (wControl_p)optFontSizeFactor ) + wControlGetHeight( (wControl_p)optFontSizeFactor ) + 10;
	x = wControlGetPosX( (wControl_p)optPrinterB ) + wControlGetWidth( (wControl_p)optPrinterB ) + 10;
	wButtonCreate( printSetupW,	  x, 4, "printSetupOk", _("Ok"), 0, 0, (wButtonCallBack_p)pSetupOk, NULL );
	wButtonCreate( printSetupW, x, -4, "printSetupCancel", _("Cancel"), 0, 0, (wButtonCallBack_p)pSetupCancel, NULL );
	wButtonCreate( printSetupW, x, -14, "printSetupTest", _("Print Test Page"), 0, 0, (wButtonCallBack_p)pTestPage, NULL );



	//printFileW = wWinPopupCreate( printSetupW, 2, 2, "printFileNameW", _("Print To File"), "xvprinttofile", F_BLOCK|F_AUTOSIZE|F_RECALLPOS, NULL, NULL );
	//wStringCreate( printFileW, 100, 3, "printFileName",
				//_("File Name? "), 0, 150, sPrintFileName, sizeof sPrintFileName,
				//NULL, NULL  );
	//wButtonCreate( printFileW, -4, 3, "printFileNameOk", _("Ok"), BB_DEFAULT, 0, printFileNameSel, NULL );

	//newFontAliasW = wWinPopupCreate( printSetupW, 2, 2, "printFontAliasW", _("Font Alias"), "xvfontalias", F_BLOCK|F_AUTOSIZE|F_RECALLPOS, NULL, NULL );
	//wMessageCreate( newFontAliasW, 0, 0, NULL, 200, _("Enter a post-script font name for:") );
	//newFontAliasXFntB = wMessageCreate( newFontAliasW, 0, -3, NULL, 200, "" );
	//wStringCreate( newFontAliasW, 0, -3, "printFontAlias", NULL, 0, 200, NULL, 0, newFontAliasSel, NULL );

	//for (i=0; papers[i].name; i++ ) {
		//wListAddValue( optPaperSizeB, papers[i].name, NULL, (void*)(intptr_t)i );
		//if ( prefPaper && strcasecmp( prefPaper, papers[i].name ) == 0 ) {
			//curPaper = i;
			//wListSetIndex( optPaperSizeB, i );
		//}
	//}

	printAbortW = wWinPopupCreate( printSetupW, 2, 2, "printAbortW", _("Printing"), "xvprintabort", F_AUTOSIZE|F_RECALLPOS, NULL, NULL );
	printAbortT = wMessageCreate( printAbortW, 0, 0, "printAbortW", 200, _("Now printing") );
	printAbortM = wMessageCreate( printAbortW, 0, -4, "printAbortW", 200, NULL );
	wButtonCreate( printAbortW, 0, 80, "printAbortW", _("Abort Print"), 0,  0, printAbort, NULL );

	for (i=0;i<sizeof fontmap/sizeof fontmap[0]; i++) {
		cp = wPrefGetString( WFONT, fontmap[i].xfontname );
		if (!cp)
			wPrefSetString( WFONT, fontmap[i].xfontname, fontmap[i].psfontname );
	}

	//cp = wPrefGetString( WPRINTER, "1" );
	//if (!cp)
		//wPrefSetString( WPRINTER, "1", "lp=lpr -P%s" );
	//wPrintNewPrinter( "FILE" );
	//for (i=1; ;i++) {
		//sprintf( num, "%d", i );
		//cp = wPrefGetString( WPRINTER, num );
		//if (!cp)
			//break;
		//wPrintNewPrinter(cp);
	//}

	for (i=0;i<sizeof pagemargins/sizeof pagemargins[0]; i++) {
		cp = wPrefGetString( WMARGIN, pagemargins[i].name );
		if (!cp)
			wPrefSetString( WMARGIN, pagemargins[i].name, pagemargins[i].value );
		sprintf( num, "%d", i );
		wPrefSetString( WMARGINMAP, num, pagemargins[i].name );
	}
	for (i=0; (cq = wPrefGetSectionItem( WMARGIN, &i, &cp )); ) {
		wPrintNewMargin(cp, cq);
	}

	for ( i=0, optXFont=NULL; wPrefGetSectionItem( WFONT, &i, &cp ); ) {
		if ( optXFont == NULL )
			optXFont = cp;
		wListAddValue( optXFontL, cp, NULL, NULL );
	}
	wListSetIndex( optXFontL, 0 );
	if ( optXFont ) {
		cp = wPrefGetString( WFONT, optXFont );
		wStringSetValue( optPSFontS, cp );
	}

}


wBool_t wPrintInit( void )
{
	return TRUE;
}

/*****************************************************************************
 *
 * TEST
 *
 */

#ifdef TEST

void main ( INT_T argc, char * argv[] )
{
	if (argc != 7) {
		fprintf( stderr, "%s <L|P> <origX> <origY> <roomSizeX> <roomSizeY>\n", argv[0] );
		exit(1);
	}
	argv++;
	printFormat = (*(*argv++)=='L')?PRINT_LANDSCAPE:PRINT_PORTRAIT;
	printDraw_d.orig.x = atof(*argv++);
	printDraw_d.orig.y = atof(*argv++);
	printRoomSize.x = atof(*argv++);
	printRoomSize.y = atof(*argv++);
	fprintf( stderr, "Fmt=%c, orig=(%0.3f %0.3f) RS=(%0.3f %0.3f)\n",
		(printFormat==PRINT_LANDSCAPE)?'L':'P',
		printDraw_d.orig.x, printDraw_d.orig.y,
		printRoomSize.x, printRoomSize.y );
	wPrintGetPageSize(PRINT_GAUDY, printFormat);
	fprintf( stderr, "PageSize= (%0.3f %0.3f)\n", printDraw_d.size.x, printDraw_d.size.y );

	wPrintDocStart( PRINT_GAUDY );
	wPrintPage( PRINT_GAUDY, 0, 0 );
	wPrintDocEnd( );
}

#endif
