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

#define PRINT_PORTRAIT  (0)
#define PRINT_LANDSCAPE (1)

#define PPI (72.0)
#define P2I( P ) ((P)/PPI)

#define CENTERMARK_LENGTH (60)				/**< size of cross marking center of circles */
#define DASH_LENGTH (8.0)					/**< length of single dash */

#define PAGESETTINGS "xtrkcad.page"			/**< filename for page settings */
#define PRINTSETTINGS "xtrkcad.printer"		/**< filename for printer settings */

/*****************************************************************************
 *
 * VARIABLES
 *
 */

static GtkPrintSettings *settings;			/**< current printer settings */
static GtkPageSetup *page_setup;			/**< current paper settings */
static GtkPrinter *selPrinter;				/**< printer selected by user */
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

static double scale_adjust = 1.0;

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

    WlibApplySettings(NULL);

    new_page_setup = gtk_print_run_page_setup_dialog(GTK_WINDOW(gtkMainW->gtkwin),
                     page_setup, settings);

    if (page_setup) {
        g_object_unref(page_setup);
    }

    page_setup = new_page_setup;

    WlibGetPaperSize();
    WlibSaveSettings(NULL);
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
    wDrawOpts opts)
{
    cairo_t *cr = psPrint_d.printContext;

    double dashes[] = { DASH_LENGTH, 3 };							//Reduce gap in between dashes
    static int len_dashes  = sizeof(dashes) / sizeof(dashes[0]);

    if (lineWidth < 0.0) {
        lineWidth = P2I(-lineWidth)*2.0/scale_adjust;
    }

    // make sure that there is a minimum line width used
    if (lineWidth <= 0.09) {
        lineWidth = 0.1/scale_adjust;
    }

    cairo_set_line_width(cr, lineWidth);

    if (lineType == wDrawLineDash) {
        cairo_set_dash(cr, dashes, len_dashes, 0.0);
    } else {
        cairo_set_dash(cr, NULL, 0, 0.0);
    }
}

/**
 * set the color for the following print operations
 *
 * \param color IN the new color
 * \return
 */

static void psSetColor(
    wDrawColor color)
{
    cairo_t *cr = psPrint_d.printContext;
    GdkColor* const gcolor = wlibGetColor(color, TRUE);

    cairo_set_source_rgb(cr, gcolor->red / 65535.0,
                         gcolor->green / 65535.0,
                         gcolor->blue / 65535.0);
}

/**
 * Print a straight line
 *
 * \param x0, y0 IN  starting point in pixels
 * \param x1, y1 IN  ending point in pixels
 * \param width line width
 * \param lineType
 * \param color color
 * \param opts ?
 */

void psPrintLine(
    wPos_t x0, wPos_t y0,
    wPos_t x1, wPos_t y1,
    wDrawWidth width,
    wDrawLineType_e lineType,
    wDrawColor color,
    wDrawOpts opts)
{
    if (color == wDrawColorWhite) {
        return;
    }

    if (opts&wDrawOptTemp) {
        return;
    }

    psSetColor(color);
    setLineType(width, lineType, opts);

    cairo_move_to(psPrint_d.printContext,
                  x0, y0);
    cairo_line_to(psPrint_d.printContext,
                  x1, y1);
    cairo_stroke(psPrint_d.printContext);
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
    wDrawOpts opts)
{
    cairo_t *cr = psPrint_d.printContext;

    if (color == wDrawColorWhite) {
        return;
    }

    if (opts&wDrawOptTemp) {
        return;
    }

    psSetColor(color);
    setLineType(width, lineType, opts);

    if (angle1 >= 360.0) {
        angle1 = 359.999;
    }

    angle1 = 90.0-(angle0+angle1);

    while (angle1 < 0.0) {
        angle1 += 360.0;
    }

    while (angle1 >= 360.0) {
        angle1 -= 360.0;
    }

    angle0 = 90.0-angle0;

    while (angle0 < 0.0) {
        angle0 += 360.0;
    }

    while (angle0 >= 360.0) {
        angle0 -= 360.0;
    }

    // draw the curve
    cairo_arc(cr, x0, y0, r, angle1 * M_PI / 180.0, angle0 * M_PI / 180.0);

    if (drawCenter) {
        // draw crosshair for center of curve
        cairo_move_to(cr, x0 - CENTERMARK_LENGTH / 2, y0);
        cairo_line_to(cr, x0 + CENTERMARK_LENGTH / 2, y0);
        cairo_move_to(cr, x0, y0 - CENTERMARK_LENGTH / 2);
        cairo_line_to(cr, x0, y0 + CENTERMARK_LENGTH / 2);
    }

    cairo_stroke(psPrint_d.printContext);
}

/**
 * Print a filled rectangle
 *
 * \param x0, y0 IN top left corner
 * \param x1, y1 IN bottom right corner
 * \param color IN fill color
 * \param opts IN options
 * \return
 */

void psPrintFillRectangle(
    wPos_t x0, wPos_t y0,
    wPos_t x1, wPos_t y1,
    wDrawColor color,
    wDrawOpts opts)
{
    cairo_t *cr = psPrint_d.printContext;
    double width = x0 - x1;
    double height = y0 - y1;

    if (color == wDrawColorWhite) {
        return;
    }

    if (opts&wDrawOptTemp) {
        return;
    }

    psSetColor(color);

    cairo_rectangle(cr, x0, y0, width, height);

    cairo_fill(cr);
}

/**
 * Print a filled polygon
 *
 * \param p IN a list of x and y coordinates
 * \param cnt IN the number of points
 * \param color IN fill color
 * \param opts IN options
 * \return
 */

void psPrintFillPolygon(
    wPos_t p[][2],
    int cnt,
    wDrawColor color,
    wDrawOpts opts)
{
    int inx;
    cairo_t *cr = psPrint_d.printContext;

    if (color == wDrawColorWhite) {
        return;
    }

    if (opts&wDrawOptTemp) {
        return;
    }

    psSetColor(color);

    cairo_move_to(cr, p[ 0 ][ 0 ], p[ 0 ][ 1 ]);

    for (inx=0; inx<cnt; inx++) {
        cairo_line_to(cr, p[ inx ][ 0 ], p[ inx ][ 1 ]);
    }

    cairo_fill(cr);
}

/**
 * Print a filled circle
 *
 * \param x0, y0  IN coordinates of center (in pixels )
 * \param r IN radius
 * \param color IN fill color
 * \param opts IN options
 * \return
 */

void psPrintFillCircle(
    wPos_t x0, wPos_t y0,
    wPos_t r,
    wDrawColor color,
    wDrawOpts opts)
{
    if (color == wDrawColorWhite) {
        return;
    }

    if (opts&wDrawOptTemp) {
        return;
    }

    psSetColor(color);

    cairo_arc(psPrint_d.printContext,
              x0, y0, r, 0.0, 2 * M_PI);

    cairo_fill(psPrint_d.printContext);
}


/**
 * Print a string at the given position using specified font and text size.
 * The orientation of the y-axis in XTrackCAD is wrong for cairo. So for
 * all other print primitives a flip operation is done. As this would
 * also affect the string orientation, printing a string has to be
 * treated differently. The starting point is transformed, then the
 * string is rotated and scaled as needed. Finally the string position
 * translated to the starting point calculated previously. The same
 * solution would have to be applied to a bitmap should printing
 * bitmaps ever be implemented.
 *
 * \param x IN x position in pixels
 * \param y IN y position in pixels
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
    wDrawOpts opts)
{
    char * cp;
    double x0 = (double)x, y0 = (double)y;
    int text_height, text_width;
    double ascent;

    cairo_t *cr;
    cairo_matrix_t matrix;

    PangoLayout *layout;
    PangoFontDescription *desc;
    PangoFontMetrics *metrics;
    PangoContext *pcontext;

    if (color == wDrawColorWhite) {
        return;
    }

    cr = psPrint_d.printContext;

    // get the current transformation matrix and transform the starting
    // point of the string

    cairo_save(cr);

    cairo_get_matrix(cr, &matrix);

    cairo_matrix_transform_point(&matrix, &x0, &y0);


    layout = pango_cairo_create_layout(cr);

    // set the correct font and size
    /** \todo use a getter function instead of double conversion */
    desc = pango_font_description_from_string(wlibFontTranslate(fp));

    pango_font_description_set_size(desc, fs * PANGO_SCALE );

    // render the string to a Pango layout
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, s, -1);
    pango_layout_set_width(layout, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    // get the height of the string
    pcontext = pango_cairo_create_context(cr);
    metrics = pango_context_get_metrics(pcontext, desc,
                                        pango_context_get_language(pcontext));

    ascent = pango_font_metrics_get_ascent(metrics) / PANGO_SCALE *scale_adjust;

    cairo_identity_matrix(cr);

    cairo_translate(cr, x0 + ((ascent + (bBorder*scale_adjust)) * sin(-a * M_PI / 180.0))+((lBorder*scale_adjust)* cos(a * M_PI / 180.0)),
    					y0 - ((ascent + (bBorder*scale_adjust)) * cos( a * M_PI / 180.0))+((lBorder*scale_adjust)* sin(a * M_PI / 180.0)));

    cairo_rotate(cr, -a * M_PI / 180.0);

    // set the color
    psSetColor(color);

    // and show the string
    pango_cairo_show_layout(cr, layout);

    // free unused objects
    g_object_unref(layout);
    g_object_unref(pcontext);

    cairo_restore(cr);
}

/**
 * Create clipping rectangle.
 *
 * \param x, y IN starting position
 * \param w, h IN width and height of rectangle
 * \return
 */

void wPrintClip(wPos_t x, wPos_t y, wPos_t w, wPos_t h)
{
    cairo_move_to(psPrint_d.printContext, x, y);
    cairo_rel_line_to(psPrint_d.printContext, w, 0);
    cairo_rel_line_to(psPrint_d.printContext, 0, h);
    cairo_rel_line_to(psPrint_d.printContext, -w, 0);
    cairo_close_path(psPrint_d.printContext);
    cairo_clip(psPrint_d.printContext);
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
 * Get the paper size. The size returned is the physical size of the
 * currently selected paper.
 * \param w OUT physical width of the paper in inches
 * \param h OUT physical height of the paper in inches
 * \return
 */

void wPrintGetPhysSize(
    double * w,
    double * h)
{
    // if necessary load the settings
    if (!settings) {
        WlibApplySettings(NULL);
    }

    WlibGetPaperSize();

    *w = paperWidth;
    *h = paperHeight;
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

    cairo_save(psPrint_d.printContext);

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
    cairo_show_page(psPrint_d.printContext);

    cairo_restore(psPrint_d.printContext);

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

        psPrint_d.curPrintSurface = gtk_print_job_get_surface(curPrintJob,
                                    NULL);
        psPrint_d.printContext = cairo_create(psPrint_d.curPrintSurface);

        //update the paper dimensions
        WlibGetPaperSize();

        /* for all surfaces including files the resolution is always 72 ppi (as all GTK uses PDF) */
        surface_type = cairo_surface_get_type(psPrint_d.curPrintSurface);

        const char * printer_name = gtk_print_settings_get_printer(settings);
        /*
         * Override up-scaling for some printer drivers/Linux systems that don't support the latest CUPS
         * - the user sets the environment variable XTRKCADPRINTSCALE1 to a value
         * and we just let the dpi default to 72ppi and set scaling to 1.0.
         *
         */
        char * sEnvScale = PRODUCT "PRINTSCALE1";

        if ((strcmp(printer_name,"Print to File") != 0) || getenv(sEnvScale) == 0) {
			double p_def = 600;
			cairo_surface_set_fallback_resolution(psPrint_d.curPrintSurface, p_def, p_def);
			psPrint_d.dpi = p_def;
			scale_adjust = 72/p_def;
		} else {
			scale_adjust = 1.0;
			psPrint_d.dpi = (double)gtk_print_settings_get_resolution(settings);
		}

        // in XTrackCAD 0,0 is top left, in cairo bottom left. This is
        // corrected via the following transformations.
        // also the translate makes sure that the drawing is rendered
        // within the paper margin

        cairo_translate(psPrint_d.printContext, lBorder*72,  (paperHeight-bBorder)*72 );

        cairo_scale(psPrint_d.printContext, 1.0 * scale_adjust,  -1.0 * scale_adjust);

        //cairo_translate(psPrint_d.printContext, 0, -paperHeight* psPrint_d.dpi);

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

    cairo_destroy(psPrint_d.printContext);

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
    cairo_surface_finish(psPrint_d.curPrintSurface);

    gtk_print_job_send(curPrintJob,
                       doPrintJobFinished,
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
