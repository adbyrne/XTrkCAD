/** \file font.c
 * Font selection and loading.
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

#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "wlib.h"
#include "gtkint.h"
#include "i18n.h"

/*
 * Macro for debug purposes. Possible debug macro values:
 *
 *   0 - no messages to console (use this value when building in release mode)
 *   1 - send errors
 *   2 - send details
 *   3 - send more details
 */
#define WLIB_FONT_DEBUG 0

static gchar sampleText[] = "AbCdE0129!@$&()[]{}";

static GtkWidget *fontSelectionDialog;


/*****************************************************************************
 * FONT HANDLERS
 */

#define FW_MEDIUM	(0)
#define FW_BOLD		(1)
#define FS_REGULAR	(0)
#define FS_ITALIC	(1)

/* absoluteFontSize was introduced to keep the font size information synchron
 * between the Dt.size of ctext.c and it's drop list on the status bar and
 * the font size coming from the gtk font dialog which is located in this file */
int absoluteFontSize = 18;

struct wFont_t {
    PangoFontDescription *fontDescription;
};

static wFont_p standardFonts[F_HELV-F_TIMES+1][2][2];
static wFont_p curFont = NULL;

/**
 * Callback for font selection dialog
 *
 * \param fontSelectionDialog IN dialog
 * \param response IN response code from dialog
 * \param data IN unused
 * \return
 */

static void fontSelectionDialogCallback(GtkFontSelectionDialog
                                        *fontSelectionDialog, gint response, gpointer data)
{
    gchar *fontName;

    if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_OK) {
        fontName = gtk_font_selection_dialog_get_font_name(fontSelectionDialog);
        wPrefSetString("font", "name", fontName);
        pango_font_description_free(curFont->fontDescription);
        curFont->fontDescription = pango_font_description_from_string(fontName);
        absoluteFontSize = (pango_font_description_get_size(
                                curFont->fontDescription))/PANGO_SCALE;
#if WLIB_FONT_DEBUG >= 2
        fprintf(stderr, "new font selection:\n");
        fprintf(stderr, "  font name \"%s\"\n", fontName);
        fprintf(stderr, "  font size is %d\n",
                pango_font_description_get_size(curFont->fontDescription)/PANGO_SCALE);
        fprintf(stderr, "  font size is absolute %d\n",
                pango_font_description_get_size_is_absolute(curFont->fontDescription));
#endif
        g_free(fontName);
    }

    if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_CANCEL) {
        gtk_widget_hide(GTK_WIDGET(fontSelectionDialog));
    }
}

static wBool_t fontInitted = FALSE;

static wBool_t fontInit()
{
    const char *fontNames[] = {
        "times 18",
        "times italic 18",
        "times bold 18",
        "times bold italic 18",
        "helvetica 18",
        "helvetica oblique 18",
        "helvetica bold 18",
        "helvetica bold oblique 18",
    };
    int s = 0;
    int i, j, k;

    for (i = F_TIMES; i <= F_HELV; ++i) {
        for (j = FW_MEDIUM; j <= FW_BOLD; ++j) {
            for (k = FS_REGULAR; k <= FS_ITALIC; ++k) {
                PangoFontDescription *fontDescription = pango_font_description_from_string(
                        fontNames[s++]);
                wFont_p standardFont = (wFont_p) malloc(sizeof(struct wFont_t));
                standardFont->fontDescription = fontDescription;
                standardFonts[i-F_TIMES][j][k] = standardFont;
            }
        }
    }

    if (curFont == NULL) {
        curFont = (wFont_p) malloc(sizeof(struct wFont_t));

        if (curFont == NULL) {
            return FALSE;
        }

        const char *fontName = wPrefGetString("font", "name");
        curFont->fontDescription = pango_font_description_from_string(
                                       fontName ? fontName : "helvetica 18");
        absoluteFontSize = (int) PANGO_PIXELS(pango_font_description_get_size(
                curFont->fontDescription));
    }

    fontInitted = TRUE;
    return TRUE;
}


static double fontFactor = 1.0;

#define FONTSIZE_TO_PANGOSIZE(fs) ((gint) ((fs) * (fontFactor) + .5))

/**
 * Create a Pango layout with a specified font and font size
 *
 * \param widget IN
 * \param cairo IN cairo context
 * \param fp IN font
 * \param fs IN size
 * \param s IN ???
 * \param width_p OUT width of layout
 * \param height_p OUT height of layout
 * \param ascent_p OUT ascent of layout
 * \param descent_p OUT descent of layout
 * \return    the created Pango layout
 */

PangoLayout *wlibFontCreatePangoLayout(GtkWidget *widget,
                                       void *cairo,
                                       wFont_p fp,
                                       wFontSize_t fs,
                                       const char *s,
                                       int *width_p,
                                       int *height_p,
                                       int *ascent_p,
                                       int *descent_p)
{
    if (!fontInitted) {
        fontInit();
    }

    PangoLayout *layout = NULL;
    gchar *utf8 = wlibConvertInput(s);
    /* RPH -- pango_cairo_create_layout() is missing in CentOS 4.8.
              CentOS 4.8 only has GTK 2.4.13 and Pango 1.6.0 and does not have
              libpangocairo at all.
              pango_cairo_create_layout() was introduced with Pango 1.10. */
#if PANGO_VERSION_MAJOR >= 1 && PANGO_VERSION_MINOR >= 10

    if (cairo != NULL) {
        layout = pango_cairo_create_layout((cairo_t *) cairo);
        pango_layout_set_text(layout, utf8, -1);
    } else
#endif
        layout = gtk_widget_create_pango_layout(widget, utf8);

    PangoFontDescription *fontDescription = (fp ? fp : curFont)->fontDescription;
    PangoContext *context;
    PangoFontMetrics *metrics;
    /* set attributes */
    pango_font_description_set_size(fontDescription,
                                    FONTSIZE_TO_PANGOSIZE(fs) * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDescription);
    /* get layout measures */
    pango_layout_get_pixel_size(layout, width_p, height_p);
    context = gtk_widget_get_pango_context(widget);
    metrics = pango_context_get_metrics(context, fontDescription,
                                        pango_context_get_language(context));
    *ascent_p  = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics));
    *descent_p = PANGO_PIXELS(pango_font_metrics_get_descent(metrics));
    pango_font_metrics_unref(metrics);
#if WLIB_FONT_DEBUG >= 3
    fprintf(stderr, "font layout created:\n");
    fprintf(stderr, "  widget:         %p\n", widget);
    //fprintf(stderr, "  font description:%p\n", fp);
    fprintf(stderr, "  font size:      %f\n", fs);
    fprintf(stderr, "  layout text:    \"%s\" (utf8)\n", utf8);
    fprintf(stderr, "  layout width:   %d\n", *width_p);
    fprintf(stderr, "  layout height:  %d\n", *height_p);
    fprintf(stderr, "  layout ascent:  %d (pixels)\n", *ascent_p);
    fprintf(stderr, "  layout descent: %d (pixels)\n", *descent_p);
#endif
    return layout;
}

/**
 * Destroy a previously allocated  Pango layout
 * \return
 */

void wlibFontDestroyPangoLayout(PangoLayout *layout)
{
    g_object_unref(G_OBJECT(layout));
}

/**
 * Initialize font data
 * \return
 */

void wInitializeFonts()
{
    if (!fontInitted) {
        fontInit();
    }
}

/**
 * Initialize and run the font dialog
 *
 * \param title IN dialog box title
 * \return
 */

void wSelectFont(
    const char * title)
{
    gchar *fontName;

    if (!fontInitted) {
        fontInit();
    }

    if (fontSelectionDialog == NULL) {
        fontSelectionDialog = gtk_font_selection_dialog_new(_("Font Select"));
        gtk_window_set_position(GTK_WINDOW(fontSelectionDialog), GTK_WIN_POS_MOUSE);
        gtk_window_set_modal(GTK_WINDOW(fontSelectionDialog), TRUE);
        gtk_font_selection_dialog_set_preview_text(GTK_FONT_SELECTION_DIALOG(
                    fontSelectionDialog), sampleText);
        g_signal_connect(G_OBJECT(fontSelectionDialog), "response",
                         G_CALLBACK(fontSelectionDialogCallback), NULL);
        g_signal_connect(G_OBJECT(fontSelectionDialog), "destroy",
                         G_CALLBACK(gtk_widget_destroyed), &fontSelectionDialog);
    }

    gtk_window_set_title(GTK_WINDOW(fontSelectionDialog), title);

    if (curFont != NULL) {
        /* the curFont description contains the latest font info
         * which is depended on the current scale
         * overwrite it with the absoluteFontSize */
        pango_font_description_set_size(curFont->fontDescription,
                                        FONTSIZE_TO_PANGOSIZE(absoluteFontSize) * PANGO_SCALE);
        fontName = pango_font_description_to_string(curFont->fontDescription);
        gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(
                fontSelectionDialog), fontName);
        g_free(fontName);
    }

    gtk_widget_show(fontSelectionDialog);
}

/**
 * Get the current font
 *
 * \return    describe the return value
 */

static wFont_p wlibSelectedFont(void)
{
    if (!fontInitted) {
        fontInit();
    }

    return curFont;
}

/**
 * Get the default font size
 *
 * \return    describe the return value
 */

wFontSize_t wSelectedFontSize(void)
{
    if (!fontInitted) {
        fontInit();
    }

#if WLIB_FONT_DEBUG >= 3
    fprintf(stderr, "the font size of current font description is: %d\n",
            pango_font_description_get_size(curFont->fontDescription)/PANGO_SCALE);
    fprintf(stderr, "the font size of absoluteFontSize is: %d\n",absoluteFontSize);
#endif
    return absoluteFontSize;
}

/**
 * Set the default font size
 *
 * \param size IN font size
 * \return    describe the return value
 */

void wSetSelectedFontSize(int size)
{
    absoluteFontSize = (wFontSize_t)size;
}

/**
 * get the Pango font description as a string from a font definition.
 * If the font definition is NULL, a default font is return. This is
 * the current font if one is set. If not the first font from the font
 * list is returned.
 *
 * \param fp IN the font definition
 * \return    the font description
 */

const char *wlibFontTranslate(wFont_p fp)
{
    static gchar *fontName = NULL;

    if (fontName != NULL) {
        g_free(fontName);
    }

    if (!fontInitted) {
        fontInit();
    }

    if (fp == NULL) {
        fp = wlibSelectedFont();
    }

    if (fp == NULL) {
        fp = standardFonts[0][FW_MEDIUM][FS_REGULAR];
    }

    fontName = pango_font_description_to_string(fp->fontDescription);
#if WLIB_FONT_DEBUG >= 2
    fprintf(stderr, "font translation: ");
    fprintf(stderr, "  \"%s\"\n", fontName);
#endif
    return (const char *) fontName;
}

/**
 * Return description for one of the standard fonts
 *
 * \return
 */

wFont_p wStandardFont(int face, wBool_t bold, wBool_t italic)
{
    if (!fontInitted) {
        fontInit();
    }

    return standardFonts[face-F_TIMES][bold][italic];
}
