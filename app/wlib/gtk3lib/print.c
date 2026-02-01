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

#include <stdio.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

static GtkPrintSettings* settings = NULL;			/**< current printer settings */
static GtkPageSetup* page_setup;			/**< current paper settings */

#define PAGESETTINGS "xtrkcad.page"			/**< filename for page settings */
#define PRINTSETTINGS "xtrkcad.printer"		/**< filename for printer settings */

// defined in /usr/lib/x86_64-linux-gnu/glib-2.0/include/glibconfig.h
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static double paperWidth;		/**< physical paper width */
static double paperHeight;		/**< physical paper height */
static double tBorder;			/**< top margin */
static double rBorder;			/**< right margin */
static double lBorder;			/**< left margin */
static double bBorder;			/**< bottom margin */

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
 * Initialize printer und paper selection using the saved settings
 *
 * \param op IN print operation to initialize. If NULL only the global
 * 				settings are loaded.
 */

void
WlibApplySettings(GtkPrintOperation* op)
{
	gchar* filename;
	GError* err = NULL;
	GtkWidget* dialog;

	filename = g_build_filename(wGetAppWorkDir(), PRINTSETTINGS, NULL);

	if (!(settings = gtk_print_settings_new_from_file(filename, &err))) {
		if (err->code != G_FILE_ERROR_NOENT) {
			// ignore file not found error as defaults will be used
			dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
				"%s", err->message);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		}
		else {
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
				"%s", err->message);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		}
		else {
			page_setup = gtk_page_setup_new();
		}

		g_error_free(err);
	}
	else {
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
WlibSaveSettings(GtkPrintOperation* op)
{
	GError* err = NULL;
	gchar* filename;
	GtkWidget* dialog;

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
			"%s", err->message);

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
			"%s", err->message);

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
	GtkPageSetup* new_page_setup;
	//    gchar *filename;
	//    GError *err;
	//    GtkWidget *dialog;

	if (!settings) {
		WlibApplySettings(NULL);
	}

	new_page_setup = gtk_print_run_page_setup_dialog(GTK_WINDOW(wlibAppWinGetMain()),
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


const char * wPrintGetName()
{
    printf("Not yet implemented wPrintGetName() %s:%d\n", __FILE__, __LINE__);
    return(NULL);
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
    printf("Not yet implemented wPrintClip() %s:%d\n", __FILE__, __LINE__);
}

/*****************************************************************************
 *
 * PAGE FUNCTIONS
 *
 */

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
    printf("Not yet implemented wPrintGetMargins() %s:%d\n", __FILE__, __LINE__);
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
    printf("Not yet implemented wPrintGetPageSize() %s:%d\n", __FILE__, __LINE__);
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
    printf("Not yet implemented wPrintPageStart() %s:%d\n", __FILE__, __LINE__);
    return(NULL);
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
        printf("Not yet implemented wPrintPageEnd() %s:%d\n", __FILE__, __LINE__);
        return(FALSE);
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
    printf("Not yet implemented wPrintDocStart() %s:%d\n", __FILE__, __LINE__);
    return(FALSE);
}

/**
 * Finish the print operation
 * \return
 */

void wPrintDocEnd(void)
{
        printf("Not yet implemented wPrintDocEnd() %s:%d\n", __FILE__, __LINE__);
}


wBool_t wPrintQuit(void)
{
    return FALSE;
}


wBool_t wPrintInit(void)
{
    return TRUE;
}
