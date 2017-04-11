/** \file liststore.c
 * Handling of the list store used for tree views and drop boxes
 */

/*
 *
 * Copyright 2016 Martin Fischer <m_fischer@users.sf.net>
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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "gtkint.h"

/**
 * Get data of one row in a list
 * \param b IN list widget
 * \param inx IN row to retrieve
 * \param childR IN list for data (?)
 * \returns
 */

wListItem_p wlibListItemGet(
    GtkListStore *ls,
    wIndex_t inx,
    GList ** childR)
{
    wListItem_p id_p;

    assert(ls != NULL);

    if (childR) {
        *childR = NULL;
    }

    if (inx < 0) {
        return NULL;
    }

    id_p = wlibListStoreGetContext(ls, inx);

    return id_p;
}

/**
 * Get the context (user data) for a row in the list store
 *
 * \param ls IN list store
 * \param inx IN row
 * \returns pointer to data
 */

void *
wlibListStoreGetContext(GtkListStore *ls, int inx)
{
    GtkTreeIter iter;
    gchar *string = NULL;
    gboolean result;
    gint childs;

    childs = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(ls),
                                            NULL);

    if (inx < childs) {
        result = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ls),
                                               &iter,
                                               NULL,
                                               inx);

        if (result) {
            gtk_tree_model_get(GTK_TREE_MODEL(ls),
                               &iter,
                               LISTCOL_DATA,
                               &string,
                               -1);
        } else {
            printf("Invalid index %d for list!\n", inx);

        }
    }

    return (string);
}


/**
 * Clear the list store
 *
 * \param listStore IN
 */

void
wlibListStoreClear(GtkListStore *listStore)
{
    assert(listStore != NULL);

    /** \todo this looks like a memory leak. should probably free the id's */
    gtk_list_store_clear(listStore);
}

/**
 * Create a list store. The list store will have one column for user
 * data that will not be displayed, a column for a bitmap and <cnt>
 * colors for data.
 *
 * \param colCnt IN number of additional columns
 * \returns the list store
 */

GtkListStore *
wlibNewListStore(int colCnt)
{
    GtkListStore *ls;
    GType *colTypes;
    int i;

    /* create the list store, using strings for all columns */
    colTypes = g_malloc(sizeof(GType) * (colCnt + LISTCOL_TEXT));
    colTypes[ LISTCOL_BITMAP ] = GDK_TYPE_PIXBUF;
    colTypes[ LISTCOL_DATA ] = G_TYPE_POINTER;

    for (i = 0; i < colCnt; i++) {
        colTypes[ LISTCOL_TEXT + i ] = G_TYPE_STRING;
    }

    ls = gtk_list_store_newv(colCnt + LISTCOL_TEXT, colTypes);
    g_free(colTypes);

    return (ls);
}

/**
 * Update the list store at the iter's position
 *
 * \param ls IN list store
 * \param iter IN iterator into list store
 * \param labels IN tab separated label string
 * \returns number of updated columns
 */

static int
wlibListStoreUpdateIter(GtkListStore *ls, GtkTreeIter *iter, char *labels)
{
    char *convertedLabels;
    char *text;
    char *start;
    int current = 0;

    convertedLabels = strdup(wlibConvertInput(labels));
    start = convertedLabels;

    while ((text = strchr(start, '\t')) != NULL) {
        *text = '\0';
        gtk_list_store_set(ls, iter, LISTCOL_TEXT + current, start, -1);
        start = text + 1;
        current++;
    }

    /* add the last piece of the string */
    gtk_list_store_set(ls, iter, LISTCOL_TEXT + current, start, -1);

    free(convertedLabels);
    return (current+1);
}

/**
 * Add a pixbuf to the list store
 * \param ls IN list store
 * \param iter IN position
 * \param pixbuf IN pixbuf to add
 */

void
wlibListStoreSetPixbuf(GtkListStore *ls, GtkTreeIter *iter, GdkPixbuf *pixbuf)
{
    gtk_list_store_set(ls, iter, LISTCOL_BITMAP, pixbuf, -1);
}
/**
 * Add a row to the list store
 *
 * \param ls IN the list store
 * \param cols IN columns in list store
 * \param id IN id
 * \returns number columns added
 */

int
wlibListStoreAddData(GtkListStore *ls, GdkPixbuf *pixbuf, int cols,
                     wListItem_p id)
{
    GtkTreeIter iter;
    int count;

    gtk_list_store_append(ls, &iter);
    gtk_list_store_set(ls, &iter, LISTCOL_DATA, id, -1);

    if (pixbuf) {
        wlibListStoreSetPixbuf(ls, &iter, pixbuf);
    }

    count = wlibListStoreUpdateIter(ls, &iter, (char *)id->label);

    return (count);
}

/**
 * Change a row in the list store. The passed strings are placed
 * in the first cols text columns of the list store.
 *
 * \param ls IN list store
 * \param row IN row in list store
 * \param cols IN number of columns to set
 * \param labels IN tab separated list of texts
 * \param bm IN bitmap
 * \return count of updated text fields
 */

int
wlibListStoreUpdateValues(GtkListStore *ls, int row, int cols, char *labels,
                          wIcon_p bm)
{
    GtkTreeIter iter;
    int count;

    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(ls),
                                  &iter,
                                  NULL,
                                  row);

    count = wlibListStoreUpdateIter(ls, &iter, labels);

    if (bm) {
        GdkPixbuf *pixbuf;

        pixbuf = wlibMakePixbuf(bm);
        wlibListStoreSetPixbuf(ls, &iter, pixbuf);
    }

    return (count);
}

