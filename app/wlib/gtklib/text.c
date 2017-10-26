/** \file text.c
 * multi line text entry widget
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "i18n.h"
#include "gtkint.h"

struct PrintData {
    wText_p	tb;
    gint lines_per_page;
    gdouble font_size;
    gchar **lines;
    gint total_lines;
    gint total_pages;
};

#define HEADER_HEIGHT 20.0
#define HEADER_GAP 8.5


/*
 *****************************************************************************
 *
 * Multi-line Text Boxes
 *
 *****************************************************************************
 */

struct wText_t {
    WOBJ_COMMON
    wPos_t width, height;
    int changed;
    GtkWidget *text;
};

/**
 * Reset a text entry by clearing text and resetting the readonly and the
 * change flag.
 *
 * \param bt IN text entry
 * \return
 */

void wTextClear(wText_p bt)
{
    GtkTextBuffer *tb;
    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bt->text));
    gtk_text_buffer_set_text(tb, "", -1);

    if (bt->option & BO_READONLY) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(bt->text), FALSE);
    }

    bt->changed = FALSE;
}

/**
 * Append text to the end of the entry field's text buffer.
 *
 * \param bt IN text buffer
 * \param text IN text to append
 * \return
 */

void wTextAppend(wText_p bt,
                 const char *text)
{
    GtkTextBuffer *tb;
    GtkTextIter ti1;

    if (bt->text == 0) {
        abort();
    }

    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bt->text));
    // convert to utf-8
    text = wlibConvertInput(text);
    // append to end of buffer
    gtk_text_buffer_get_end_iter(tb, &ti1);
    gtk_text_buffer_insert(tb, &ti1, text, -1);
    bt->changed = FALSE;
}

/**
 * Get the text from a text buffer in system codepage
 * The caller is responsible for free'ing the allocated storage.
 *
 * \todo handling of return from gtkConvertOutput can be improved
 *
 * \param bt IN the text widget
 * \return    pointer to the converted text
 */

static char *wlibGetText(wText_p bt)
{
    GtkTextBuffer *tb;
    GtkTextIter ti1, ti2;
    char *cp, *cp1, *res;

    if (bt->text == 0) {
        abort();
    }

    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bt->text));
    gtk_text_buffer_get_bounds(tb, &ti1, &ti2);
    cp = gtk_text_buffer_get_text(tb, &ti1, &ti2, FALSE);
    cp1 = wlibConvertOutput(cp);
    res = strdup(cp1);
    g_free(cp);
    return res;
}

/**
 * Save the text from the widget to a file
 *
 * \param bt IN the text widget
 * \param fileName IN name of save file
 * \return    TRUE is success, FALSE if not
 */

wBool_t wTextSave(wText_p bt, const char *fileName)
{
    FILE *f;
    char *cp;
    f = fopen(fileName, "w");

    if (f==NULL) {
        wNoticeEx(NT_ERROR, fileName, "Ok", NULL);
        return FALSE;
    }

    cp = wlibGetText(bt);
    fwrite(cp, 1, strlen(cp), f);
    free(cp);
    fclose(f);
    return TRUE;
}

/**
 * Begin the printing by retrieving the contents of the text box and
 * count the lines of text.
 *
 * \param operation IN the GTK print operation
 * \param context IN print context
 * \param pd IN data structure for user data
 *
 */

static void
begin_print(GtkPrintOperation *operation,
            GtkPrintContext *context,
            struct PrintData *pd)
{
    gchar *contents;
    gdouble height;
    contents =  wlibGetText(pd->tb);
    pd->lines = g_strsplit(contents, "\n", 0);
    /* Count the total number of lines in the file. */
    /* ignore the header lines */
    pd->total_lines = 6;

    while (pd->lines[pd->total_lines] != NULL) {
        pd->total_lines++;
    }

    /* Based on the height of the page and font size, calculate how many lines can be
    * rendered on a single page. A padding of 3 is placed between lines as well.
    * Space for page header, table header and footer lines is subtracted from the total size
    */
    height = gtk_print_context_get_height(context) - (pd->font_size + 3) - 2 *
             (HEADER_HEIGHT + HEADER_GAP);
    pd->lines_per_page = floor(height / (pd->font_size + 3));
    pd->total_pages = (pd->total_lines - 1) / pd->lines_per_page + 1;
    gtk_print_operation_set_n_pages(operation, pd->total_pages);
    free(contents);
}

/**
 * Draw the page, which includes a header with the file name and page number along
 * with one page of text with a font of "Monospace 10".
 *
 * \param operation IN the GTK print operation
 * \param context IN print context
 * \param page_nr IN page to print
 * \param pd IN data structure for user data
 *
 *
 */

static void
draw_page(GtkPrintOperation *operation,
          GtkPrintContext *context,
          gint page_nr,
          struct PrintData *pd)
{
    cairo_t *cr;
    PangoLayout *layout;
    gdouble width, text_height, height;
    gint line, i, text_width, layout_height;
    PangoFontDescription *desc;
    gchar *page_str;
    cr = gtk_print_context_get_cairo_context(context);
    width = gtk_print_context_get_width(context);
    layout = gtk_print_context_create_pango_layout(context);
    desc = pango_font_description_from_string("Monospace");
    pango_font_description_set_size(desc, pd->font_size * PANGO_SCALE);
    /*
     * render the header line with document type parts list on left and
     * first line of layout title on right
     */
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, pd->lines[ 0 ], -1); 	// document type
    pango_layout_set_width(layout, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_get_size(layout, NULL, &layout_height);
    text_height = (gdouble) layout_height / PANGO_SCALE;
    cairo_move_to(cr, 0, (HEADER_HEIGHT - text_height) / 2);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_text(layout, pd->lines[ 2 ], -1);		// layout title
    pango_layout_get_size(layout, &text_width, NULL);
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    cairo_move_to(cr, width - (text_width / PANGO_SCALE),
                  (HEADER_HEIGHT - text_height) / 2);
    pango_cairo_show_layout(cr, layout);
    /* Render the column header */
    cairo_move_to(cr, 0, HEADER_HEIGHT + HEADER_GAP + pd->font_size + 3);
    pango_layout_set_text(layout, pd->lines[ 6 ], -1);
    pango_cairo_show_layout(cr, layout);
    cairo_rel_move_to(cr, 0, pd->font_size + 3);
    pango_layout_set_text(layout, pd->lines[ 7 ], -1);
    pango_cairo_show_layout(cr, layout);
    /* Render the page text with the specified font and size. */
    cairo_rel_move_to(cr, 0, pd->font_size + 3);
    line = page_nr * pd->lines_per_page + 8;

    for (i = 0; i < pd->lines_per_page && line < pd->total_lines; i++) {
        pango_layout_set_text(layout, pd->lines[line], -1);
        pango_cairo_show_layout(cr, layout);
        cairo_rel_move_to(cr, 0, pd->font_size + 3);
        line++;
    }

    /*
     * Render the footer line with date on the left and page number
     * on the right
     */
    pango_layout_set_text(layout, pd->lines[ 5 ], -1); 	// date
    pango_layout_set_width(layout, -1);
    pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
    pango_layout_get_size(layout, NULL, &layout_height);
    text_height = (gdouble) layout_height / PANGO_SCALE;
    height = gtk_print_context_get_height(context);
    cairo_move_to(cr, 0, height - ((HEADER_HEIGHT - text_height) / 2));
    pango_cairo_show_layout(cr, layout);
    page_str = g_strdup_printf(_("%d of %d"), page_nr + 1,
                               pd->total_pages);  // page number
    pango_layout_set_text(layout, page_str, -1);
    pango_layout_get_size(layout, &text_width, NULL);
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    cairo_move_to(cr, width - (text_width / PANGO_SCALE),
                  height - ((HEADER_HEIGHT - text_height) / 2));
    pango_cairo_show_layout(cr, layout);
    g_free(page_str);
    g_object_unref(layout);
    pango_font_description_free(desc);
}

/**
 * Clean up after the printing operation since it is done.
 *
 * \param operation IN the GTK print operation
 * \param context IN print context
 * \param pd IN data structure for user data
 *
 *
 */
static void
end_print(GtkPrintOperation *operation,
          GtkPrintContext *context,
          struct PrintData *pd)
{
    g_strfreev(pd->lines);
    free(pd);
}

/**
 * Print the content of a multi line text box. This function is only used
 * for printing the parts list. So it makes some assumptions on the structure
 * and the content. Change if the multi line entry is changed.
 * The deprecated gtk_text is not supported by this function.
 *
 * Thanks to Andrew Krause's book for a good starting point.
 *
 * \param bt IN the text field
 * \return    TRUE on success, FALSE on error
 */

wBool_t wTextPrint(
    wText_p bt)
{
    GtkPrintOperation *operation;
    GtkWidget *dialog;
    GError *error = NULL;
    gint res;
    struct PrintData *data;
    /* Create a new print operation, applying saved print settings if they exist. */
    operation = gtk_print_operation_new();
    WlibApplySettings(operation);
    data = malloc(sizeof(struct PrintData));
    data->font_size = 10.0;
    data->tb = bt;
    g_signal_connect(G_OBJECT(operation), "begin_print",
                     G_CALLBACK(begin_print), (gpointer) data);
    g_signal_connect(G_OBJECT(operation), "draw_page",
                     G_CALLBACK(draw_page), (gpointer) data);
    g_signal_connect(G_OBJECT(operation), "end_print",
                     G_CALLBACK(end_print), (gpointer) data);
    /* Run the default print operation that will print the selected file. */
    res = gtk_print_operation_run(operation,
                                  GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                                  GTK_WINDOW(gtkMainW->gtkwin), &error);

    /* If the print operation was accepted, save the new print settings. */
    if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
        WlibSaveSettings(operation);
    }
    /* Otherwise, report that the print operation has failed. */
    else if (error) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(gtkMainW->gtkwin),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                        "%s",error->message);
        g_error_free(error);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    g_object_unref(operation);
    return TRUE;
}


/**
 * Get the length of text
 *
 * \param bt IN the text widget
 * \return    length of string
 */

int wTextGetSize(wText_p bt)
{
    char *cp = wlibGetText(bt);
    int len = strlen(cp);
    free(cp);
    return len;
}

/**
 * Get the  text
 *
 * \param bt IN the text widget
 * \param text IN the buffer
 * \param len IN maximum number of bytes to return
 * \return
 */

void wTextGetText(wText_p bt, char *text, int len)
{
    char *cp;
    cp = wlibGetText(bt);
    strncpy(text, cp, len);

    if (len > 0) {
        text[len - 1] = '\0';
    }

    free(cp);
}

/**
 * Get the  read-only state of the text entry
 *
 * \param bt IN the text widget
 * \param ro IN read only flag
 * \return
 */

void wTextSetReadonly(wText_p bt, wBool_t ro)
{
    gtk_text_view_set_editable(GTK_TEXT_VIEW(bt->text), !ro);

    if (ro) {
        bt->option |= BO_READONLY;
    } else {
        bt->option &= ~BO_READONLY;
    }
}

/**
 * check whether text has been modified
 *
 * \param bt IN the text widget
 * \return    TRUE of changed, FALSE otherwise
 */

wBool_t wTextGetModified(wText_p bt)
{
    return bt->changed;
}

/**
 * set the size of the text widget
 *
 * \param bt IN the text widget
 * \param w IN width
 * \param h IN height
 * \return
 */

void wTextSetSize(wText_p bt, wPos_t w, wPos_t h)
{
    gtk_widget_set_size_request(bt->widget, w, h);
    bt->w = w;
    bt->h = h;
}

/**
 * calculate the required size of the widget based on number of lines and columns
 * \todo this calculation is based on a fixed size font and is not useful in a generic setup
 *
 * \param bt IN the text widget
 * \param rows IN text rows
 * \param cols IN text columns
 * \param width OUT width in pixel
 * \param height OUT height in pixel
 * \return
 */

void wTextComputeSize(wText_p bt, int rows, int cols, wPos_t *width,
                      wPos_t *height)
{
    *width = rows * 7;
    *height = cols * 14;
}

/**
 * set the position of the text widget ????
 *
 * \param bt IN the text widget
 * \param pos IN position
 * \return
 */

void wTextSetPosition(wText_p bt, int pos)
{
    /* TODO */
}

/**
 * signal handler for changed signal
 *
 * \param widget IN
 * \param bt IN text entry field
 * \return
 */

static void textChanged(GtkWidget *widget, wText_p bt)
{
    if (bt == 0) {
        return;
    }

    bt->changed = TRUE;
}

/**
 * Create a multiline text entry field
 *
 * \param parent IN parent window
 * \param x IN x position
 * \param Y IN y position
 * \param helpStr IN balloon help string
 * \param labelStr IN Button label ???
 * \param option IN
 * \param width IN
 * \param valueP IN Current color ???
 * \param action IN Button callback procedure
 * \param data IN ???
 * \return 	bb handle for created text widget
 */

wText_p
wTextCreate(wWin_p	parent,
            wPos_t	x,
            wPos_t	y,
            const char 	 *helpStr,
            const char	 *labelStr,
            long	option,
            wPos_t	width,
            wPos_t	height)
{
    wText_p bt;
    GtkTextBuffer *tb;
    // create the widget
    bt = wlibAlloc(parent, B_MULTITEXT, x, y, labelStr, sizeof *bt, NULL);
    bt->width = width;
    bt->height = height;
    bt->option = option;
    wlibComputePos((wControl_p)bt);
    // create a scroll window with scroll bars that are automatically created
    bt->widget = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bt->widget),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    // create a text view and place it inside the scroll widget
    bt->text = gtk_text_view_new();

    if (bt->text == 0) {
        abort();
    }

    gtk_container_add(GTK_CONTAINER(bt->widget), bt->text);
    // get the text buffer and add a bold tag to it
    tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(bt->text));
    gtk_text_buffer_create_tag(tb, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);

    // this seems to assume some fixed size fonts, not really helpful
    if (option&BT_CHARUNITS) {
        width *= 7;
        height *= 14;
    }

    // show the widgets
    gtk_widget_show(bt->text);
    gtk_widget_show(bt->widget);
    // set the size???
    gtk_widget_set_size_request(GTK_WIDGET(bt->widget),
                                width+15/*requisition.width*/, height);

    // configure read-only mode
    if (bt->option&BO_READONLY) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(bt->text), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(bt->text), FALSE);
    }

    if (labelStr) {
        bt->labelW = wlibAddLabel((wControl_p)bt, labelStr);
    }

    wlibAddHelpString(bt->widget, helpStr);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(bt->text), GTK_WRAP_WORD);
    g_signal_connect(G_OBJECT(tb), "changed", G_CALLBACK(textChanged), bt);
    // place the widget in a fixed position of the parent
    gtk_fixed_put(GTK_FIXED(parent->widget), bt->widget, bt->realX, bt->realY);
    wlibControlGetSize((wControl_p)bt);
    wlibAddButton((wControl_p)bt);
    // done, return the finished widget
    return bt;
}
