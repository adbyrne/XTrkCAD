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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "gtkint.h"

/**
 * Get attributes of one row in a list
 * \param ls IN list widget
 * \param inx IN row to retrieve
 * \param childR IN list for attributes (?)
 * \returns
 */

wListItem_p wlibListItemGet(
        GtkListStore *ls,
        wIndex_t inx,
        GList ** childR)
{
	wListItem_p id_p;

	g_assert(ls != NULL);

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
 * Get the context (user attributes) for a row in the list store
 *
 * \param ls IN list store
 * \param inx IN row
 * \returns pointer to context
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
 * Clear the list store. All attributes in the list store will be automatically
 * free'd when the list store is cleared.
 *
 * \param listStore IN
 */

void
wlibListStoreClear(GtkListStore *listStore)
{
	wListItem_p id_p;
	int i = 0;
	GPtrArray *items;

	g_assert(listStore != NULL);

	/* Collect every row's context first and defer freeing until after
	 * gtk_list_store_clear() returns -- clearing can reentrantly fire
	 * selection-changed signals (via GtkTreeView's cursor reset) whose
	 * handlers read this same context data, so freeing it up front left
	 * those handlers touching already-freed memory. */
	items = g_ptr_array_new();
	id_p = wlibListStoreGetContext(listStore, i++);
	while (id_p) {
		g_ptr_array_add(items, id_p);
		id_p = wlibListStoreGetContext(listStore, i++);
	}

	gtk_list_store_clear(listStore);

	for (unsigned int j = 0; j < items->len; j++) {
		id_p = g_ptr_array_index(items, j);
		if (id_p->label) {
			g_free(id_p->label);
		}
		g_free(id_p);
	}
	g_ptr_array_free(items, TRUE);
}

/**
 * Create a list store. The list store will have one column for user
 * attributes that will not be displayed, a column for a bitmap and \a colCnt
 * columns for text strings.
 *
 * \param colCnt IN number of additional columns
 * \returns the list store
 */

GtkListStore *
wlibNewListStore(int colCnt)
{
	GtkListStore *ls;
	GType *colTypes;

	/* create the list store, using strings for all additional columns */
	colTypes = g_malloc(sizeof(GType) * (colCnt + LISTCOL_TEXT));
	colTypes[LISTCOL_ERROR_COLOR] = G_TYPE_INT;
	colTypes[ LISTCOL_BITMAP ] = GDK_TYPE_PIXBUF;
	colTypes[ LISTCOL_DATA ] = G_TYPE_POINTER;

	for (int i = 0; i < colCnt; i++) {
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

	convertedLabels = g_strdup(wlibConvertInput(labels));
	start = convertedLabels;
	/** \todo replace strchr with g_strsplit */
	while ((text = strchr(start, '\t')) != NULL) {
		*text = '\0';
		gtk_list_store_set(ls, iter, LISTCOL_TEXT + current, start, -1);
		start = text + 1;
		current++;
	}

	/* add the last piece of the string */
	gtk_list_store_set(ls, iter, LISTCOL_TEXT + current, start, -1);

	g_free(convertedLabels);
	return (current+1);
}



/**
 * Add a row to the list store
 *
 * \param store IN the list store
 * \param iter IN position
 * \param data IN row data
 */

void
wlibListStoreAppendRow(GtkListStore* store, GtkTreeIter *iter, wListItem_p data)
{
	gtk_list_store_append(store, iter);
	gtk_list_store_set(store, iter, LISTCOL_DATA, data, -1);
}

/**
 * Add a pixbuf to the list store. The pixbuf is unref'ed so it will be freed
 * with the list store.
 *
 * \param store IN list store
 * \param iter IN position
 * \param icon IN icon to add
 */

void
wlibListStoreSetIcon(GtkListStore* store, GtkTreeIter* iter, wIcon_p icon)
{
	gtk_list_store_set(store, iter, LISTCOL_BITMAP, icon->bits, -1);
}

void
wlibListStoreSetData(GtkListStore* store, GtkTreeIter* iter, int column,
                     const char* label )
{
	gtk_list_store_set(store, iter, LISTCOL_TEXT+column, label, -1);
}

//void
//wlibListStoreAddRow(GtkListStore* listStore, wIcon_p icon, wListItem_p userData, const char* label)
//{
//	GtkTreeIter iter;
//
//	ListStoreAppendRow(listStore, &iter, userData);
//
//	if (icon) {
//		ListStoreSetIcon(listStore, &iter, icon);
//	}
//
//	ListStoreSetData(listStore, &iter, label);
//}

/**
 * Change a row in the list store. The passed strings are placed
 * in the first cols text columns of the list store.
 *
 * \param ls IN list store
 * \param row IN row in list store
 * \param labels IN tab separated list of texts
 * \param bm IN bitmap
 * \return count of updated text fields
 */

int
wlibListStoreUpdateValues(GtkListStore *ls, int row, char *labels,
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
		//GdkPixbuf *pixbuf;

		//pixbuf = wlibMakePixbuf(bm);
		wlibListStoreSetIcon(ls, &iter, bm);
	}

	return (count);
}

