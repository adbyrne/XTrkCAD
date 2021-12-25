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
#include <gtk/gtkunixprint.h>

#include "wlib.h"
#include "i18n.h"

extern wDrawColor wDrawColorWhite;
extern wDrawColor wDrawColorBlack;

/*****************************************************************************
 *
 * MACROS
 *
 */


#define PAGESETTINGS "xtrkcad.page"			/**< filename for page settings */
#define PRINTSETTINGS "xtrkcad.printer"		/**< filename for printer settings */

/*****************************************************************************
 *
 * VARIABLES
 *
 */

static GtkPrintSettings *settings = NULL;			/**< current printer settings */
static GtkPageSetup *page_setup;			/**< current paper settings */
static GtkPrinter *selPrinter = NULL;				/**< printer selected by user */
static GtkPrintJob *curPrintJob;			/**< currently active print job */
extern struct wDraw_t psPrint_d;

static wBool_t printContinue;	/**< control print job, FALSE for cancelling */

static wIndex_t pageCount;		/**< unused, could be used for progress indicator */
static wIndex_t
totalPageCount; /**< unused, could be used for progress indicator */

static double paperWidth;		/**< physical paper width */
static double paperHeight;		/**< physical paper height */
static double tBorder;			/**< top margin */
static double rBorder;			/**< right margin */
static double lBorder;			/**< left margin */
static double bBorder;			/**< bottom margin */

//static double scale_adjust = 1.0;
//static double scale_text = 1.0;

static long printFormat = PRINT_LANDSCAPE;

/*****************************************************************************
 *
 * FUNCTIONS
 *
 */

static void WlibGetPaperSize(void);

/**
 * Initialize printer und paper selection using the saved settings
 *
 * \param op IN print operation to initialize. If NULL only the global
 * 				settings are loaded.
 */

void
WlibApplySettings(GtkPrintOperation *op)
{
	gchar *filename;
	GError *err = NULL;
	GtkWidget *dialog;

	filename = g_build_filename(wGetAppWorkDir(), PRINTSETTINGS, NULL);

	if (!(settings = gtk_print_settings_new_from_file(filename, &err))) {
		if (err->code != G_FILE_ERROR_NOENT) {
			// ignore file not found error as defaults will be used
			dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
			                                GTK_DIALOG_DESTROY_WITH_PARENT,
			                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			                                "%s",err->message);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		} else {
			// create  default print settings
			settings = gtk_print_settings_new();
		}

		g_error_free(err);
	}

	g_free(filename);

	if (settings && op) {
		gtk_print_operation_set_print_settings(op, settings);
	}

	err = NULL;
	filename = g_build_filename(wGetAppWorkDir(), PAGESETTINGS, NULL);

	if (!(page_setup = gtk_page_setup_new_from_file(filename, &err))) {
		// ignore file not found error as defaults will be used
		if (err->code != G_FILE_ERROR_NOENT) {
			dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
			                                GTK_DIALOG_DESTROY_WITH_PARENT,
			                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
			                                "%s",err->message);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		} else {
			page_setup = gtk_page_setup_new();
		}

		g_error_free(err);
	} else {
		// on success get the paper dimensions
		WlibGetPaperSize();
	}

	g_free(filename);

	if (page_setup && op) {
		gtk_print_operation_set_default_page_setup(op, page_setup);
	}

}

/**
 * Save the printer settings. If op is not NULL the settings are retrieved
 * from the print operation. Otherwise the state of the globals is saved.
 *
 * \param op IN printer operation. If NULL the glabal variables are used
 */

void
WlibSaveSettings(GtkPrintOperation *op)
{
	GError *err = NULL;
	gchar *filename;
	GtkWidget *dialog;

	if (op) {
		if (settings != NULL) {
			g_object_unref(settings);
		}

		settings = g_object_ref(gtk_print_operation_get_print_settings(op));
	}

	filename = g_build_filename(wGetAppWorkDir(), PRINTSETTINGS, NULL);

	if (!gtk_print_settings_to_file(settings, filename, &err)) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
		                                GTK_DIALOG_DESTROY_WITH_PARENT,
		                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		                                "%s",err->message);

		g_error_free(err);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	g_free(filename);

	if (op) {
		if (page_setup != NULL) {
			g_object_unref(page_setup);
		}

		page_setup = g_object_ref(gtk_print_operation_get_default_page_setup(op));
	}

	filename = g_build_filename(wGetAppWorkDir(), PAGESETTINGS, NULL);

	if (!gtk_page_setup_to_file(page_setup, filename, &err)) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
		                                GTK_DIALOG_DESTROY_WITH_PARENT,
		                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		                                "%s",err->message);

		g_error_free(err);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	g_free(filename);

}

/**
 * Page setup function. Previous settings are loaded and the setup
 * dialog is shown. The settings are saved after the dialog ends.
 *
 * \param callback IN unused
 */

void wPrintSetup(wPrintSetupCallBack_p callback)
{
	GtkPageSetup *new_page_setup;
	gchar *filename;
	GError *err;
	GtkWidget *dialog;

	if ( !settings ) {
		WlibApplySettings(NULL);
	}

	new_page_setup = gtk_print_run_page_setup_dialog(GTK_WINDOW(gtkMainW->gtkwin),
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
 *
 *
 */


static GtkPrinter * pDefaultPrinter = NULL;
gboolean isDefaultPrinter( GtkPrinter * printer, gpointer data )
{
	const char * pPrinterName = gtk_printer_get_name( printer );
	if ( gtk_printer_is_default( printer ) ) {
		pDefaultPrinter = printer;
		return TRUE;
	}
	return FALSE;
}

static void getDefaultPrinter()
{
	pDefaultPrinter = NULL;
	gtk_enumerate_printers( isDefaultPrinter, NULL, NULL, TRUE );
}

const char * wPrintGetName()
{
	static char sPrinterName[100];
	WlibApplySettings( NULL );
	const char * pPrinterName =
	        gtk_print_settings_get( settings, "format-for-printer" );
	if ( pPrinterName == NULL ) {
		getDefaultPrinter();
		if ( pDefaultPrinter ) {
			pPrinterName = gtk_printer_get_name( pDefaultPrinter );
		}
	}
	if ( pPrinterName == NULL ) {
		pPrinterName = "";
	}
	strncpy (sPrinterName, pPrinterName, sizeof sPrinterName - 1 );
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
 */

/**
 * Create clipping rectangle.
 *
 * \param x, y IN starting position
 * \param w, h IN width and height of rectangle
 * \return
 */

void wPrintClip(wDrawPix_t x, wDrawPix_t y, wDrawPix_t w, wDrawPix_t h)
{
	cairo_move_to(psPrint_d.cr, x, y);
	cairo_rel_line_to(psPrint_d.cr, w, 0);
	cairo_rel_line_to(psPrint_d.cr, 0, h);
	cairo_rel_line_to(psPrint_d.cr, -w, 0);
	cairo_close_path(psPrint_d.cr);
	cairo_clip(psPrint_d.cr);
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
WlibGetPaperSize(void)
{
	double temp;

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
 * Get the paper size. The size returned is the printable area of the
 * currently selected paper, ie. the physical size minus the margins.
 * \param w OUT printable width of the paper in inches
 * \param h OUT printable height of the paper in inches
 * \return
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
 * \return
 */

void wPrintGetPageSize(
        double * w,
        double * h)
{
	// if necessary load the settings
	if (!settings) {
		WlibApplySettings(NULL);
	}

	WlibGetPaperSize();

	*w = paperWidth -lBorder - rBorder;
	*h = paperHeight - tBorder - bBorder;
}


/**
 * Cancel the current print job. This function is preserved here for
 * reference in case the function should be implemented again.
 * \param context IN unused
 * \return
 */
static void printAbort(void * context)
{
	printContinue = FALSE;
//	wWinShow( printAbortW, FALSE );
}

/**
 * Initialize new page.
 * The cairo_save() / cairo_restore() cycle was added to solve problems
 * with a multi page print operation. This might actually be a bug in
 * cairo but I didn't examine that any further.
 *
 * \return   print context for the print operation
 */
wDraw_p wPrintPageStart(void)
{
	pageCount++;

	cairo_save(psPrint_d.cr);

	return &psPrint_d;
}

/**
 * End of page. This function returns the contents of printContinue. The
 * caller continues printing as long as TRUE is returned. Setting
 * printContinue to FALSE in an asynchronous handler therefore cleanly
 * terminates a print job at the end of the page.
 *
 * \param p IN ignored
 * \return    always printContinue
 */


wBool_t wPrintPageEnd(wDraw_p p)
{
	cairo_show_page(psPrint_d.cr);

	cairo_restore(psPrint_d.cr);

	return printContinue;
}

/*****************************************************************************
 *
 * PRINT START/END
 *
 */


/**
 * Start a new document
 *
 * \param title IN title of document ( name of layout )
 * \param fTotalPageCount IN number of pages to print (unused)
 * \param copiesP OUT ???
 * \return TRUE if successful, FALSE if cancelled by user
 */

wBool_t wPrintDocStart(const char * title, int fTotalPageCount, int * copiesP)
{
	GtkWidget *printDialog;
	gint res;
	cairo_surface_type_t surface_type;
	cairo_matrix_t matrix;

	printDialog = gtk_print_unix_dialog_new(title, GTK_WINDOW(gtkMainW->gtkwin));

	// load the settings
	WlibApplySettings(NULL);

	// and apply them to the printer dialog
	gtk_print_unix_dialog_set_settings((GtkPrintUnixDialog *)printDialog, settings);
	gtk_print_unix_dialog_set_page_setup((GtkPrintUnixDialog *)printDialog,
	                                     page_setup);

	res = gtk_dialog_run((GtkDialog *)printDialog);

	if (res == GTK_RESPONSE_OK) {
		selPrinter = gtk_print_unix_dialog_get_selected_printer((
		                        GtkPrintUnixDialog *)printDialog);

		if (settings) {
			g_object_unref(settings);
		}

		settings = gtk_print_unix_dialog_get_settings((GtkPrintUnixDialog *)
		                printDialog);

		if (page_setup) {
			g_object_unref(page_setup);
		}

		page_setup = gtk_print_unix_dialog_get_page_setup((GtkPrintUnixDialog *)
		                printDialog);

		curPrintJob = gtk_print_job_new(title,
		                                selPrinter,
		                                settings,
		                                page_setup);

		psPrint_d.surface = gtk_print_job_get_surface(curPrintJob,
		                    NULL);
		psPrint_d.cr = cairo_create(psPrint_d.surface);

		WlibApplySettings( NULL );
		//update the paper dimensions
		WlibGetPaperSize();

		/* for all surfaces including files the resolution is always 72 ppi (as all GTK uses PDF) */
		surface_type = cairo_surface_get_type(psPrint_d.surface);

		/*
		 * Override up-scaling for some printer drivers/Linux systems that don't support the latest CUPS
		 * - the user either sets preferences or the environment variable XTRKCADPRINTSCALE to a value
		 * and we just let the dpi default to 72ppi and set scaling to that value.
		 * And for PangoText we allow an override via preferences or variable XTRKCADPRINTTEXTSCALE
		 * Note - doing this will introduce differing artifacts.
		 *
		 */
		char * sEnvScale = PRODUCT "PRINTSCALE";
		char * sEnvTextScale = PRODUCT "PRINTTEXTSCALE";

		psPrint_d.scale_text = 1.0;
		psPrint_d.scale_adjust = 1.0;

		double printScale,printTextScale;

		wPrefGetFloat(PREFSECTION, PRINTSCALE, &printScale, -1.0);
		wPrefGetFloat(PREFSECTION, PRINTTEXTSCALE, &printTextScale, -1.0);


		//If the preferences are not set, look at environmental variables

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

		const char * sPrinterName = gtk_printer_get_name( selPrinter );
		if ((strcmp(sPrinterName,"Print to File") == 0) || printScale < 0.0) {
			double p_def = 600;
			cairo_surface_set_fallback_resolution(psPrint_d.surface, p_def, p_def);
			psPrint_d.dpi = p_def;
			psPrint_d.scale_adjust = 72/p_def;
		} else {
			if (printTextScale > 0.0) {
				psPrint_d.scale_text = printTextScale;
			}
			if (printScale > 0.0) {
				psPrint_d.scale_adjust = printScale;
			}
			psPrint_d.dpi = 72;
		}

		// in XTrackCAD 0,0 is top left, in cairo bottom left. This is
		// corrected via the following transformations.
		// also the translate makes sure that the drawing is rendered
		// within the paper margins

		cairo_translate(psPrint_d.cr, lBorder*72,  (paperHeight-bBorder)*72 );

		cairo_scale (psPrint_d.cr,
		             1.0 * psPrint_d.scale_adjust,
		             -1.0 * psPrint_d.scale_adjust);

		WlibSaveSettings(NULL);
	}

	gtk_widget_destroy(printDialog);

	if (copiesP) {
		*copiesP = 1;
	}

	printContinue = TRUE;

	if (res != GTK_RESPONSE_OK) {
		return FALSE;
	} else {
		return TRUE;
	}
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
doPrintJobFinished(GtkPrintJob *job, void *data, GError *err)
{
	GtkWidget *dialog;

	cairo_destroy(psPrint_d.cr);

	if (err) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
		                                GTK_DIALOG_DESTROY_WITH_PARENT,
		                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
		                                "%s",err->message);
	}
}

/**
 * Finish the print operation
 * \return
 */

void wPrintDocEnd(void)
{
	cairo_surface_finish(psPrint_d.surface);

	gtk_print_job_send(curPrintJob,
	                   (GtkPrintJobCompleteFunc)doPrintJobFinished,
	                   NULL,
	                   NULL);

//	wWinShow( printAbortW, FALSE );
}


wBool_t wPrintQuit(void)
{
	return FALSE;
}


wBool_t wPrintInit(void)
{
	return TRUE;
}
