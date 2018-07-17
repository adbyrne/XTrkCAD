/** \file notice.c
 * Misc. message type windows
 *
 * Copyright 2016 Martin Fischer <m_fischer@sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

static char * wlibChgMnemonic(char *label);

typedef struct {
    GtkWidget * win;
    GtkWidget * label;
    GtkWidget * butt[3];
} notice_win;
static notice_win noticeW;
static long noticeValue;

/**
 *
 * @param widget IN
 * @param value IN
 */

static void doNotice(
    GtkWidget * widget,
    long value)
{
    noticeValue = value;
    gtk_widget_destroy(noticeW.win);
    wlibDoModal(NULL, FALSE);
}

/**
 * Show a notification window with a yes/no reply and an icon.
 *
 * \param type IN type of message: Information, Warning, Error
 * \param msg  IN message to display
 * \param yes  IN text for accept button
 * \param no   IN text for cancel button
 * \return    True when accept was selected, false otherwise
 */

int wNoticeEx(int type,
              const char * msg,
              const char * yes,
              const char * no)
{

    int res;
    unsigned flag;
    char *headline;
    GtkWidget *dialog;
    GtkWindow *parent = NULL;

    switch (type) {
    case NT_INFORMATION:
        flag = GTK_MESSAGE_INFO;
        headline = _("Information");
        break;

    case NT_WARNING:
        flag = GTK_MESSAGE_WARNING;
        headline = _("Warning");
        break;

    case NT_ERROR:
        flag = GTK_MESSAGE_ERROR;
        headline = _("Error");
        break;
    }

    if (gtkMainW) {
        parent = GTK_WINDOW(gtkMainW->gtkwin);
    }

    dialog = gtk_message_dialog_new(parent,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    flag,
                                    ((no==NULL)?GTK_BUTTONS_OK:GTK_BUTTONS_YES_NO),
                                    "%s", msg);
    gtk_window_set_title(GTK_WINDOW(dialog), headline);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return res == GTK_RESPONSE_OK  || res == GTK_RESPONSE_YES;
}


/**
 * Popup up a notice box with one or two buttons.
 * When this notice box is displayed the application is paused and
 * will not response to other actions.
 *
 * @param msg IN message
 * @param yes IN first button label
 * @param no IN second button label (or NULL)
 * @returns TRUE for first FALSE for second button
 */

int wNotice(
    const char * msg,		/* Message */
    const char * yes,		/* First button label */
    const char * no)		/* Second label (or 'NULL') */
{
    return wNotice3(msg, yes, no, NULL);
}

/** \brief Popup a notice box with three buttons.
 *
 * Popup up a notice box with three buttons.
 * When this notice box is displayed the application is paused and
 * will not response to other actions.
 *
 * Pushing the first button returns 1
 * Pushing the second button returns 0
 * Pushing the third button returns -1
 *
 * \param msg Text to display in message box
 * \param yes First button label
 * \param no  Second label (or 'NULL')
 * \param cancel Third button label (or 'NULL')
 *
 * \returns 1, 0 or -1
 */

int wNotice3(
    const char * msg,		/* Message */
    const char * affirmative,		/* First button label */
    const char * cancel,		/* Second label (or 'NULL') */
    const char * alternate)
{
    notice_win *nw;
    GtkWidget * top_grid;
    GtkWidget * hbox;
    GtkWidget * hbox1;
    GtkWidget * image;
    nw = &noticeW;

    char *aff = NULL;
    char *can = NULL;
    char *alt = NULL;


    nw->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_position(GTK_WINDOW(nw->win), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(nw->win), 0);
    gtk_window_set_resizable(GTK_WINDOW(nw->win), FALSE);
    gtk_window_set_modal(GTK_WINDOW(nw->win), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(nw->win), GDK_WINDOW_TYPE_HINT_DIALOG);

    top_grid = gtk_grid_new();
    gtk_widget_show(top_grid);
    gtk_container_add(GTK_CONTAINER(nw->win), top_grid);
    gtk_container_set_border_width(GTK_CONTAINER(top_grid), 12);


    //hbox = gtk_hbox_new(FALSE, 12);
    //gtk_box_pack_start(GTK_BOX(top_grid), hbox, TRUE, TRUE, 0);
    //gtk_widget_show(hbox);
    hbox = gtk_grid_new();
    gtk_widget_show(hbox);
    gtk_grid_attach (GTK_GRID (top_grid), hbox, 0, 0, 1, 1);

    image = gtk_image_new_from_icon_name(_("dialog-warning"),
                                     GTK_ICON_SIZE_DIALOG);
    gtk_widget_show(image);
    gtk_grid_attach (GTK_GRID (hbox), image, 0, 0, 1, 1);
    //gtk_box_pack_start(GTK_BOX(hbox), image, TRUE, TRUE, 0);
    //gtk_misc_set_alignment(GTK_MISC(image), 0, 0);

    /* create the text label, allow GTK to wrap and allow for markup (for future enhancements) */
    nw->label = gtk_label_new(msg);
    gtk_widget_show(nw->label);
    gtk_grid_attach_next_to (GTK_GRID (hbox), nw->label, image, GTK_POS_RIGHT, 1, 1);
    //gtk_box_pack_end(GTK_BOX(hbox), nw->label, TRUE, TRUE, 0);
    gtk_label_set_use_markup(GTK_LABEL(nw->label), FALSE);
    gtk_label_set_line_wrap(GTK_LABEL(nw->label), TRUE);
    //gtk_misc_set_alignment(GTK_MISC(nw->label), 0, 0);

    /* this hbox will include the button bar */
    hbox1 = gtk_grid_new();
    gtk_widget_show(hbox1);
    gtk_grid_attach_next_to (GTK_GRID (top_grid), hbox1, hbox, GTK_POS_BOTTOM, 1, 1);
    //gtk_box_pack_start(GTK_BOX(top_grid), hbox1, FALSE, TRUE, 0);
    //gtk_grid_attach (GTK_GRID (top_grid), image, 0, 0, 1, 1);

    /* add the respective buttons */
    aff = wlibChgMnemonic((char *) affirmative);
    nw->butt[ 0 ] = gtk_button_new_with_mnemonic(aff);
    gtk_widget_show(nw->butt[ 0 ]);
    gtk_grid_attach (GTK_GRID (hbox1), nw->butt[0], 0, 0, 1, 1);
    //gtk_box_pack_end(GTK_BOX(hbox1), nw->butt[ 0 ], TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(nw->butt[ 0 ]), 3);
    g_signal_connect(nw->butt[0], "clicked", G_CALLBACK(doNotice),
                     (void*)1);
    gtk_widget_set_can_default(nw->butt[ 0 ], TRUE);

    if (cancel) {
        can = wlibChgMnemonic((char *) cancel);
        nw->butt[ 1 ] = gtk_button_new_with_mnemonic(can);
        gtk_widget_show(nw->butt[ 1 ]);
        gtk_grid_attach_next_to (GTK_GRID (hbox1), nw->butt[0], nw->butt[1], GTK_POS_RIGHT, 1, 1);
        //gtk_box_pack_end(GTK_BOX(hbox1), nw->butt[ 1 ], TRUE, TRUE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(nw->butt[ 1 ]), 3);
        g_signal_connect(nw->butt[1], "clicked", G_CALLBACK(doNotice),
                         (void*)0);
        gtk_widget_set_can_default(nw->butt[ 1 ], TRUE);

        if (alternate) {
            alt = wlibChgMnemonic((char *) alternate);
            nw->butt[ 2 ] = gtk_button_new_with_mnemonic(alt);
            gtk_widget_show(nw->butt[ 2 ]);
            gtk_grid_attach_next_to (GTK_GRID (hbox1), nw->butt[1], nw->butt[2], GTK_POS_RIGHT, 1, 1);
            //gtk_box_pack_start(GTK_BOX(hbox1), nw->butt[ 2 ], TRUE, TRUE, 0);
            gtk_container_set_border_width(GTK_CONTAINER(nw->butt[ 2 ]), 3);
            g_signal_connect(nw->butt[2], "clicked", G_CALLBACK(doNotice),
                             (void*)-1);
            gtk_widget_set_can_default(nw->butt[ 2 ], TRUE);
        }
    }

    gtk_widget_grab_default(nw->butt[ 0 ]);
    gtk_widget_grab_focus(nw->butt[ 0 ]);

    gtk_widget_show(nw->win);

    if (gtkMainW) {
        gtk_window_set_transient_for(GTK_WINDOW(nw->win), GTK_WINDOW(gtkMainW->gtkwin));
        /*		gdk_window_set_group( nw->win->window, gtkMainW->gtkwin->window ); */
    }

    wlibDoModal(NULL, TRUE);

    if (aff) {
        free(aff);
    }

    if (can) {
        free(can);
    }

    if (alt) {
        free(alt);
    }

    return noticeValue;
}

/* \brief Convert label string from Windows mnemonic to GTK
 *
 * The first occurence of '&' in the passed string is changed to '_'
 *
 * \param label the string to convert
 * \return pointer to modified string, has to be free'd after usage
 *
 */
static
char * wlibChgMnemonic(char *label)
{
    char *ptr;
    char *cp;

    cp = strdup(label);

    ptr = strchr(cp, '&');

    if (ptr) {
        *ptr = '_';
    }

    return (cp);
}


