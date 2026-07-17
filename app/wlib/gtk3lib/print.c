/** \file print.c
 * Printing functions using GTK's portable print API (GtkPrintOperation).
 *
 * This is the GTK3 rewrite of the former GtkUnixPrint-based implementation.
 * It uses GtkPrintOperation so the same code path works on Linux (CUPS) and
 * Windows (GDI). The imperative page loop that XTrackCAD's cprint.c used to
 * drive directly has been inverted: GtkPrintOperation owns the loop and calls
 * back into a per-page render procedure that cprint.c registers.
 *
 * Ownership split (agreed design):
 *   - GTK owns the physical *sheet* (paper size, margins, orientation) via
 *     GtkPageSetup, edited through the native page-setup dialog.
 *   - XTrackCAD owns the *tiling* of the layout onto pages (the print grid,
 *     its origin/angle/size, page order, decorations).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <math.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#define PRODUCT "XTRKCAD"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
/* NOTE: <gtk/gtkunixprint.h> is deliberately NOT included any more.
 * All Unix-only objects (GtkPrintUnixDialog, GtkPrintJob, GtkPrinter,
 * gtk_enumerate_printers) have been removed for Windows portability. */

#include "wlib.h"
#include "i18n.h"

extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;

/*****************************************************************************
 *
 * MACROS
 *
 */

#define PRINT_PORTRAIT  (0)
#define PRINT_LANDSCAPE (1)

#define PPI (72.0)

#define CENTERMARK_LENGTH (60)				/**< size of cross marking center of circles */
#define DASH_LENGTH (8.0)					/**< length of single dash */

/*****************************************************************************
 *
 * VARIABLES
 *
 */

static GtkPrintSettings *settings = NULL;	/**< current printer settings */
static GtkPageSetup *page_setup = NULL;		/**< current paper settings */

static GtkPrintOperation *printOp =
        NULL;	/**< the active portable print operation */

/*
 * The print target is a real B_DRAW control (struct control) whose struct draw
 * has drawDestination == DIRECTCAIRO. This routes every wDraw*() primitive
 * through the shared wlibBasicDraw* path (drawcairo.c), which draws directly
 * onto the drawable's cairo context (bd->cr) with no widget, surface, batching
 * or invalidation - exactly what printing needs, and the same path used for
 * bitmap export.
 *
 * The control is allocated once (lazily) and its per-page cairo, scale and
 * transform are refreshed on each wPrintPageStart().
 */
static wControl_p printControl =
        NULL;	/**< B_DRAW control used as the print target */

static wBool_t printContinue;	/**< control print job, FALSE for cancelling */

static wIndex_t pageCount;		/**< unused, could be used for progress indicator */

static double paperWidth;		/**< physical paper width */
static double paperHeight;		/**< physical paper height */
static double tBorder;			/**< top margin */
static double rBorder;			/**< right margin */
static double lBorder;			/**< left margin */
static double bBorder;			/**< bottom margin */

static double scale_adjust = 1.0;
static double scale_text = 1.0;

/*
 * Bridge to cprint.c.
 *
 * cprint.c builds the ordered list of pages to print, then registers a
 * per-page render procedure. GtkPrintOperation drives the loop and the
 * draw-page handler below invokes this procedure once per page, after the
 * cairo context and coordinate transform have been established.
 */
static wPrintPageRenderProc gRenderProc =
        NULL;	/**< per-page callback into cprint.c */
static void *gRenderData = NULL;				/**< opaque data passed back to cprint.c */
static int gNumPages = 0;						/**< number of pages to print */

/*****************************************************************************
 *
 * FUNCTIONS
 *
 */

static void WlibGetPaperSize(void);

/**
 * Initialize printer and paper selection using the saved settings.
 *
 * Print settings and page setup are persisted in the shared preferences
 * (.ini) store via wlibPrefGetPrintSettings() / wlibPrefGetPageSetup() in
 * wpref.c, rather than in separate files. Defaults are returned when no saved
 * settings exist.
 *
 * \param op IN print operation to initialize. If NULL only the global
 * 				settings are loaded.
 */

void
WlibApplySettings(GtkPrintOperation *op)
{
	if (settings) {
		g_object_unref(settings);
	}
	settings = wlibPrefGetPrintSettings();

	if (settings && op) {
		gtk_print_operation_set_print_settings(op, settings);
	}

	if (page_setup) {
		g_object_unref(page_setup);
	}
	page_setup = wlibPrefGetPageSetup();

	if (page_setup) {
		WlibGetPaperSize();

		if (op) {
			gtk_print_operation_set_default_page_setup(op, page_setup);
		}
	}
}

/**
 * Save the printer settings and page setup into the shared preferences store.
 * If op is not NULL the current settings and page setup are retrieved from the
 * print operation first; otherwise the state of the globals is saved.
 *
 * The values are written into the in-memory preferences (GKeyFile) via
 * wlibPrefSetPrintSettings() / wlibPrefSetPageSetup(); they are persisted to
 * disk with the rest of the preferences on the normal prefs flush.
 *
 * \param op IN printer operation. If NULL the global variables are used
 */

void
WlibSaveSettings(GtkPrintOperation *op)
{
	if (op) {
		if (settings != NULL) {
			g_object_unref(settings);
		}
		settings = g_object_ref(gtk_print_operation_get_print_settings(op));
	}

	if (!settings) {
		settings = gtk_print_settings_new();
	}

	wlibPrefSetPrintSettings(settings);

	if (op) {
		GtkPageSetup *op_page_setup =
		        gtk_print_operation_get_default_page_setup(op);
		if (op_page_setup) {
			if (page_setup != NULL) {
				g_object_unref(page_setup);
			}
			page_setup = g_object_ref(op_page_setup);
		}
	}

	if (!page_setup) {
		page_setup = gtk_page_setup_new();
	}

	wlibPrefSetPageSetup(page_setup);
}

/**
 * Page setup function. Previous settings are loaded and the setup
 * dialog is shown. The settings are saved after the dialog ends.
 *
 * This uses gtk_print_run_page_setup_dialog(), which is portable across
 * Linux and Windows, so it is unchanged from the Unix implementation.
 *
 * \param callback IN unused
 */

void wPrintSetup(wPrintSetupCallBack_p callback)
{
	GtkPageSetup *new_page_setup;

	if ( !settings ) {
		WlibApplySettings(NULL);
	}

	new_page_setup = gtk_print_run_page_setup_dialog(wlibAppWinGetMain(),
	                 page_setup, settings);

	if (page_setup
	    && (page_setup != new_page_setup)) {      //Can be the same if no mods...
		g_object_unref(page_setup);
	}

	page_setup = new_page_setup;

	WlibGetPaperSize();
	WlibSaveSettings(NULL);
}

/*****************************************************************************
 *
 * PRINTER NAME
 *
 * On the portable API the selected printer is not exposed as a GtkPrinter
 * object. Its identity lives in the print settings under the "printer" key,
 * which is populated once the user has run the print dialog at least once.
 * There is therefore no portable way to look up the *default* printer before
 * any dialog has been shown; the name may be empty on a fresh install until
 * the user has printed or run printer setup once.
 *
 */

const char * wPrintGetName()
{
	static char sPrinterName[100];
	const char *pPrinterName = NULL;

	WlibApplySettings( NULL );

	if ( settings ) {
		/* the printer chosen in a previous print dialog */
		pPrinterName = gtk_print_settings_get_printer( settings );

		/* fall back to the printer a page setup was formatted for */
		if ( pPrinterName == NULL ) {
			pPrinterName = gtk_print_settings_get( settings, "format-for-printer" );
		}
	}

	if ( pPrinterName == NULL ) {
		pPrinterName = "";
	}

	strncpy( sPrinterName, pPrinterName, sizeof sPrinterName - 1 );
	sPrinterName[ sizeof sPrinterName - 1 ] = '\0';

	for ( char * cp = sPrinterName; *cp; cp++ )
		if ( *cp == ':' ) {
			*cp = '-';
		}

	return sPrinterName;
}

/*****************************************************************************
 *
 * BASIC PRINTING
 *
 * Printing draws through the shared wlibBasicDraw* path (drawcairo.c /
 * basicdraw.c) by handing cprint.c a B_DRAW control whose drawDestination is
 * DIRECTCAIRO. That path renders directly onto the control's cairo context
 * (struct draw::cr) using struct draw::scale_adjust / ::scale_text, with the
 * y-flip carried by the cairo matrix we install per page. There are therefore
 * no print-specific drawing primitives here any more - the former psPrint*()
 * family and the psPrint_d drawable have been removed.
 *
 */

/**
 * Lazily allocate the B_DRAW control used as the print target.
 *
 * This is a widget-less control: it is never shown, never receives events, and
 * owns no surface. Only the fields consulted by the wlibBasicDraw* path and by
 * wDrawGetDPI() are populated. The per-page cairo, scale and transform are set
 * in wPrintPageStart().
 *
 * \return the print control, or NULL on allocation failure
 */

static wControl_p
GetPrintControl(void)
{
	struct draw *bd;

	if (printControl) {
		return printControl;
	}

	printControl = (wControl_p) calloc(1, sizeof(struct control));
	if (!printControl) {
		return NULL;
	}

	printControl->type = B_DRAW;
	printControl->widget = NULL;			/* no backing widget when printing */
	printControl->name = "print";

	bd = CONTROL_GET_ATTRIBUTES_PTR(printControl, draw);
	bd->drawDestination = DIRECTCAIRO;		/* use bd->cr via wlibBasicDraw* */
	bd->delayUpdate = TRUE;					/* never invalidate a widget */
	bd->cr = NULL;
	bd->surface = NULL;
	bd->temp_surface = NULL;
	bd->clipregion = NULL;
	bd->scale_adjust = scale_adjust;
	bd->scale_text = scale_text;

	return printControl;
}

/**
 * Release the print control (if any).
 */

static void
FreePrintControl(void)
{
	if (printControl) {
		struct draw *bd = CONTROL_GET_ATTRIBUTES_PTR(printControl, draw);
		if (bd->clipregion) {
			g_free(bd->clipregion);
			bd->clipregion = NULL;
		}
		free(printControl);
		printControl = NULL;
	}
}

/**
 * Create a rectangular clipping region on the current print page.
 *
 * basicdraw.c has no clip primitive (bitmap export never clips), so this is
 * implemented directly on the page cairo context. The coordinates are in the
 * same y-flipped page space the drawing primitives use, so no INMAP-style
 * conversion is applied here.
 *
 * \param x, y IN starting position
 * \param w, h IN width and height of rectangle
 */

void wPrintClip(wDrawPix_t x, wDrawPix_t y, wDrawPix_t w, wDrawPix_t h)
{
	struct draw *bd;

	if (!printControl) {
		return;
	}

	bd = CONTROL_GET_ATTRIBUTES_PTR(printControl, draw);
	if (!bd->cr) {
		return;
	}

	cairo_move_to(bd->cr, x, y);
	cairo_rel_line_to(bd->cr, w, 0);
	cairo_rel_line_to(bd->cr, 0, h);
	cairo_rel_line_to(bd->cr, -w, 0);
	cairo_close_path(bd->cr);
	cairo_clip(bd->cr);
}


/*****************************************************************************
 *
 * PAGE FUNCTIONS
 *
 */

/**
 * Get the paper dimensions and margins and setup the internal variables
 */

static void
WlibGetPaperSize(void)
{
	double temp;

	if (!page_setup) {
		return;
	}

	bBorder = gtk_page_setup_get_bottom_margin(page_setup, GTK_UNIT_INCH);
	tBorder = gtk_page_setup_get_top_margin(page_setup, GTK_UNIT_INCH);
	lBorder = gtk_page_setup_get_left_margin(page_setup, GTK_UNIT_INCH);
	rBorder = gtk_page_setup_get_right_margin(page_setup, GTK_UNIT_INCH);
	paperHeight = gtk_page_setup_get_paper_height(page_setup, GTK_UNIT_INCH);
	paperWidth = gtk_page_setup_get_paper_width(page_setup, GTK_UNIT_INCH);

	// XTrackCAD does page orientation itself. Basic assumption is that the
	// paper is always oriented in portrait mode. Ignore settings by user
	if (paperHeight < paperWidth) {
		temp = paperHeight;
		paperHeight = paperWidth;
		paperWidth = temp;
	}
}

/**
 * Get the printable margins of the currently selected paper.
 * \param tMargin OUT top margin in inches
 * \param rMargin OUT right margin in inches
 * \param bMargin OUT bottom margin in inches
 * \param lMargin OUT left margin in inches
 */

void wPrintGetMargins(
        double * tMargin,
        double * rMargin,
        double * bMargin,
        double * lMargin )
{
	if ( tMargin ) { *tMargin = tBorder; }
	if ( rMargin ) { *rMargin = rBorder; }
	if ( bMargin ) { *bMargin = bBorder; }
	if ( lMargin ) { *lMargin = lBorder; }
}

/**
 * Get the paper size. The size returned is the physical size of the
 * currently selected paper.
 * \param w OUT physical width of the paper in inches
 * \param h OUT physical height of the paper in inches
 */

void wPrintGetPageSize(
        double * w,
        double * h)
{
	// if necessary load the settings
	if (!settings || !page_setup) {
		WlibApplySettings(NULL);
	}

	WlibGetPaperSize();

	*w = paperWidth;
	*h = paperHeight;
}

/*****************************************************************************
 *
 * PER-PAGE RENDERING
 *
 * The page cairo context, scale factors and coordinate transform are
 * established here, once per page, from the GtkPrintContext, before cprint.c's
 * per-page render procedure is called back. The transform reproduces the
 * former print setup: the origin is moved to the bottom-left of the printable
 * area and the y-axis is flipped (XTrackCAD is y-down; cairo/PDF is y-up).
 * The shared wlibBasicDraw* string primitive is written to expect exactly this
 * flipped matrix and compensates so glyphs stay upright.
 *
 */

static double printDpi =
        600.0;	/**< internal rendering resolution for the current job */

/**
 * Determine the internal rendering resolution and the resulting scale
 * factors. The GtkPrintContext cairo context always operates in points
 * (1 unit = 1/72 inch) regardless of device DPI - GTK handles the device
 * scaling - so scale_adjust maps XTrackCAD's dpi-space coordinates into
 * points.
 *
 * Historically the Unix code used 72 dpi for real printers (to sidestep a
 * CUPS up-scaling bug) and 600 dpi for file export. That printer-name-based
 * distinction is no longer available on the portable API, so we default to a
 * fine internal resolution and expose two override knobs, matching the old
 * behaviour:
 *   - XTRKCADPRINTSCALE     / preference PRINTSCALE
 *   - XTRKCADPRINTTEXTSCALE / preference PRINTTEXTSCALE
 *
 * \param context IN the GtkPrintContext for the current operation
 */

static void
WlibSetupResolution(GtkPrintContext *context)
{
	char * sEnvScale = PRODUCT "PRINTSCALE";
	char * sEnvTextScale = PRODUCT "PRINTTEXTSCALE";
	double printScale, printTextScale;

	scale_text = 1.0;
	scale_adjust = 1.0;

	wPrefGetFloat(PREFSECTION, PRINTSCALE, &printScale, -1.0);
	wPrefGetFloat(PREFSECTION, PRINTTEXTSCALE, &printTextScale, -1.0);

	// If the preferences are not set, look at environment variables
	if (printScale < 0.0 ) {
		if (getenv(sEnvScale) && (atof(getenv(sEnvScale)) > 0.0)) {
			printScale = atof(getenv(sEnvScale));
		}
	}
	if (printTextScale < 0.0 ) {
		if (getenv(sEnvTextScale) && (atof(getenv(sEnvTextScale)) > 0.0)) {
			printTextScale = atof(getenv(sEnvTextScale));
		}
	}

	if (printScale > 0.0) {
		// explicit override: drive coordinates at 72 dpi and scale by the
		// requested factor
		scale_adjust = printScale;
		printDpi = 72.0;
	} else {
		// default: render at a fine internal resolution for accurate
		// coordinate placement. The context is in points, so a higher
		// internal dpi only buys finer granularity, never lower quality.
		printDpi = 600.0;
		scale_adjust = 72.0 / printDpi;
	}

	if (printTextScale > 0.0) {
		scale_text = printTextScale;
	}

	(void)context;
}

/**
 * Establish the page cairo context, scale and coordinate transform on the
 * print control. MUST run before any wDraw*() primitive for that page.
 *
 * \param context IN the GtkPrintContext supplied by draw-page
 */

static void
WlibBeginPageTransform(GtkPrintContext *context)
{
	cairo_t *cr = gtk_print_context_get_cairo_context(context);
	wControl_p ctl = GetPrintControl();
	struct draw *bd;

	if (!ctl) {
		return;
	}
	bd = CONTROL_GET_ATTRIBUTES_PTR(ctl, draw);

	// The cairo context is owned by GTK; we must not create or destroy it.
	bd->cr = cr;
	bd->dpi = printDpi;
	bd->scale_adjust = scale_adjust;
	bd->scale_text = scale_text;

	// refresh paper/margin globals from the page setup GTK actually used
	GtkPageSetup *ctxPageSetup = gtk_print_context_get_page_setup(context);
	if (ctxPageSetup) {
		if (page_setup) {
			g_object_unref(page_setup);
		}
		page_setup = g_object_ref(ctxPageSetup);
	}
	WlibGetPaperSize();

	// in XTrackCAD 0,0 is top left, in cairo bottom left. This is corrected
	// via the following transformations. The translate also makes sure the
	// drawing is rendered within the paper margin. The shared basicdraw
	// string primitive expects this flipped matrix.
	cairo_translate(cr, lBorder * 72.0, (paperHeight - bBorder) * 72.0);
	cairo_scale(cr, 1.0 * scale_adjust, -1.0 * scale_adjust);
}

/**
 * Initialize a new page. Called by cprint.c from within its per-page render
 * procedure. The page cairo, scale and transform have already been established
 * by the draw-page handler, so this only balances the save/restore that
 * wPrintPageEnd performs and returns the print control (as wDraw_p; it is a
 * real B_DRAW control, which wDrawGetDPI() and the wDraw*() primitives require).
 *
 * \return   the print drawable, or NULL on failure
 */
wDraw_p wPrintPageStart(void)
{
	wControl_p ctl = GetPrintControl();
	struct draw *bd;

	pageCount++;

	if (!ctl) {
		return NULL;
	}
	bd = CONTROL_GET_ATTRIBUTES_PTR(ctl, draw);
	if (bd->cr) {
		cairo_save(bd->cr);
	}

	return (wDraw_p) ctl;
}

/**
 * End of page. GtkPrintOperation emits the page itself between draw-page
 * calls, so - unlike the Unix implementation - this must NOT call
 * cairo_show_page(). It only restores the state saved by wPrintPageStart and
 * reports whether printing should continue.
 *
 * \param p IN ignored
 * \return    always printContinue
 */

wBool_t wPrintPageEnd(wDraw_p p)
{
	if (printControl) {
		struct draw *bd = CONTROL_GET_ATTRIBUTES_PTR(printControl, draw);
		if (bd->cr) {
			cairo_restore(bd->cr);
		}
	}

	return printContinue;
}


/*****************************************************************************
 *
 * GtkPrintOperation SIGNAL HANDLERS
 *
 */

/**
 * "begin-print" handler. Tell GTK how many pages will be produced and
 * determine the internal rendering resolution for this operation.
 */

static void
doBeginPrint(GtkPrintOperation *op, GtkPrintContext *context, gpointer data)
{
	WlibSetupResolution(context);

	pageCount = 0;
	printContinue = TRUE;

	gtk_print_operation_set_n_pages(op, gNumPages > 0 ? gNumPages : 1);
}

/**
 * "draw-page" handler. Establish the context and transform, then call back
 * into cprint.c to render the requested page. A FALSE return from the render
 * procedure requests that the job be cancelled at the end of the page.
 */

static void
doDrawPage(GtkPrintOperation *op, GtkPrintContext *context, gint page_nr,
           gpointer data)
{
	cairo_t *cr = gtk_print_context_get_cairo_context(context);

	// isolate everything this page does behind a single save/restore so no
	// transform or state leaks between pages
	cairo_save(cr);

	WlibBeginPageTransform(context);

	if (gRenderProc) {
		if (!gRenderProc(page_nr, gRenderData)) {
			printContinue = FALSE;
			gtk_print_operation_cancel(op);
		}
	}

	cairo_restore(cr);

	// the cairo context is owned by GTK and about to become invalid; drop our
	// borrowed reference on the print control
	if (printControl) {
		struct draw *bd = CONTROL_GET_ATTRIBUTES_PTR(printControl, draw);
		bd->cr = NULL;
	}
}

/**
 * "end-print" handler. Persist the settings and page setup that the user
 * confirmed in the dialog so they are restored next time.
 */

static void
doEndPrint(GtkPrintOperation *op, GtkPrintContext *context, gpointer data)
{
	WlibSaveSettings(op);
}

/*****************************************************************************
 *
 * PRINT START/END - BRIDGE TO cprint.c
 *
 */

/**
 * Register the per-page render procedure and the total page count.
 *
 * cprint.c builds the ordered list of pages to print, then calls this so the
 * draw-page handler can request each page in turn. Copies and collation are
 * left to GTK, so the render procedure is only ever asked to draw each page
 * once.
 *
 * \param nPages IN number of pages to be printed
 * \param proc IN callback invoked once per page from draw-page
 * \param data IN opaque pointer passed back to proc
 */

void wPrintDocSetPages(int nPages, wPrintPageRenderProc proc, void *data)
{
	gNumPages = nPages;
	gRenderProc = proc;
	gRenderData = data;
}

/**
 * Start a new document. Creates the portable print operation, applies the
 * saved settings and page setup, and connects the signal handlers. The actual
 * dialog and page loop are driven later by wPrintDocRun().
 *
 * \param title IN title of document ( name of layout )
 * \param fTotalPageCount IN number of pages to print
 * \param copiesP OUT always set to 1: GTK handles copies and collation
 * \return TRUE if the operation was created successfully
 */

wBool_t wPrintDocStart(const char * title, int fTotalPageCount, int * copiesP)
{
	if (printOp) {
		g_object_unref(printOp);
		printOp = NULL;
	}

	printOp = gtk_print_operation_new();

	// load and apply saved settings + page setup
	WlibApplySettings(NULL);

	if (settings) {
		gtk_print_operation_set_print_settings(printOp, settings);
	}
	if (page_setup) {
		gtk_print_operation_set_default_page_setup(printOp, page_setup);
	}

	if (title) {
		gtk_print_operation_set_job_name(printOp, title);
	}

	// XTrackCAD lays out its own pages and computes its own margin offsets
	// in WlibBeginPageTransform() below, which assumes the draw-page cairo
	// context spans the full physical sheet (origin at the physical page
	// corner). With use_full_page FALSE, GTK itself insets the origin by
	// the margins, which would double-apply the offset and push content
	// past the printable area.
	gtk_print_operation_set_use_full_page(printOp, TRUE);
	gtk_print_operation_set_unit(printOp, GTK_UNIT_POINTS);
	gtk_print_operation_set_show_progress(printOp, TRUE);

	gNumPages = fTotalPageCount;

	g_signal_connect(printOp, "begin-print", G_CALLBACK(doBeginPrint), NULL);
	g_signal_connect(printOp, "draw-page", G_CALLBACK(doDrawPage), NULL);
	g_signal_connect(printOp, "end-print", G_CALLBACK(doEndPrint), NULL);

	if (copiesP) {
		// GTK performs copies/collation itself; cprint.c draws each page once
		*copiesP = 1;
	}

	printContinue = TRUE;

	return (printOp != NULL);
}

/**
 * Run the print operation. Shows the native print dialog and then drives the
 * begin-print / draw-page / end-print cycle. This call blocks until the job
 * is finished or cancelled.
 *
 * \return TRUE if the user confirmed and the job ran, FALSE on cancel/error
 */

wBool_t wPrintDocRun(void)
{
	GError *err = NULL;
	GtkPrintOperationResult res;

	if (!printOp) {
		return FALSE;
	}

	res = gtk_print_operation_run(printOp,
	                              GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
	                              wlibAppWinGetMain(),
	                              &err);

	if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
		GtkWidget *dialog =
		        gtk_message_dialog_new(wlibAppWinGetMain(),
		                               GTK_DIALOG_DESTROY_WITH_PARENT,
		                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		                               "%s", err ? err->message : "print error");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		if (err) {
			g_error_free(err);
		}
		return FALSE;
	}

	// keep the confirmed settings for the next run
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		if (settings) {
			g_object_unref(settings);
		}
		settings = g_object_ref(gtk_print_operation_get_print_settings(printOp));
	}

	return (res == GTK_PRINT_OPERATION_RESULT_APPLY);
}

/**
 * Finish the print operation and release the render bridge.
 */

void wPrintDocEnd(void)
{
	if (printOp) {
		g_object_unref(printOp);
		printOp = NULL;
	}

	gRenderProc = NULL;
	gRenderData = NULL;
	gNumPages = 0;

	FreePrintControl();
}

wBool_t wPrintQuit(void)
{
	return FALSE;
}

wBool_t wPrintInit(void)
{
	return TRUE;
}
