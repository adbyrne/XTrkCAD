/** \file list.c
 * Listboxes, dropdown boxes, combo boxes
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

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "wlib.h"
#include "gtkint.h"
#include "i18n.h"

struct listSearch {
	const char *search;
	char *result;
	int row;
};

/*
 *****************************************************************************
 *
 * List Boxes
 *
 *****************************************************************************
 */

/**
 * Remove all entries from the list
 *
 * \param b IN list
 * \return
 */

void wListClear(
        wList_p b)
{
	g_assert(b!= NULL);

	wlibListStoreClear(b->listStore);
	b->last = -1;
}

/**
 * Makes the <val>th entry (0-origin) the current selection.
 * If <val> if '-1' then no entry is selected.
 * \param b IN List
 * \param element IN Index
 */

void wListSetIndex(
        wList_p b,
        int element)
{
	if (b->widget == 0) {
		abort();
	}

	if (b->type == B_DROPLIST) {
		wComboBoxSetIndex(b, element);
	} else {
		wlibTreeViewSetSelected(b, element);
	}

	b->last = element;
}

/**
 * CompareListData is called when a list is searched for a specific
 * data entry. It is called in sequence and does a string compare
 * between the label of the current row and the search argument. If
 * identical the label is placed in the search argument.
 * It is a GTK foreach() function.
 *
 * \param model IN searched model
 * \param path IN unused
 * \param iter IN current iterator
 * \param data IN/OUT pointer to data structure with search criteria
 * \return TRUE if identical, FALSE otherwise
 */

int
CompareListData(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                gpointer data)
{
	wListItem_p id_p;
	struct listSearch *search = (struct listSearch *)data;

	gtk_tree_model_get(model,
	                   iter,
	                   LISTCOL_DATA,
	                   &id_p,
	                   -1);

	if (id_p && id_p->label && !strcmp(id_p->label, search->search)) {
		search->result = (char *)id_p->label;
		return TRUE;
	} else {
		search->result = NULL;
		search->row++;
		return FALSE;
	}
}

/**
 * Find the row which contains the specified text.
 *
 * \param b IN
 * \param val IN
 * \returns found row or -1 if not found
 */

wIndex_t wListFindValue(
        wList_p b,
        const char * val)
{
	struct listSearch thisSearch;

	g_assert(b!=NULL);
	g_assert(b->listStore!=NULL);

	thisSearch.search = val;
	thisSearch.row = 0;

	gtk_tree_model_foreach(GTK_TREE_MODEL(b->listStore), CompareListData,
	                       (void *)&thisSearch);

	if (!thisSearch.result) {
		return -1;
	} else {
		return thisSearch.row;
	}
}

/**
 * Return the number of rows in the list
 *
 * \param b IN widget
 * \returns number of rows
 */

wIndex_t wListGetCount(
        wList_p b)
{
	if (b->type == B_DROPLIST) {
		return wDropListGetCount(b);
	} else {
		return wTreeViewGetCount(b);
	}
}

/**
 * Get the user data for a list element
 *
 * \param b IN widget
 * \param inx IN row
 * \returns the user data for the specified row
 */

void * wListGetItemContext(
        wList_p b,
        wIndex_t inx)
{
	if (inx < 0) {
		return NULL;
	}

	if (b->type == B_DROPLIST) {
		return wDropListGetItemContext(b, inx);
	} else {
		return wTreeViewGetItemContext(b, inx);
	}
}

/**
 *
 * \param bl IN widget
 * \param labelStr IN ?
 * \param labelSize IN ?
 * \param listDataRet IN
 * \param itemDataRet IN
 * \returns
 */

wIndex_t wListGetValues(
        wList_p bl,
        char * labelStr,
        int labelSize,
        void * * listDataRet,
        void * * itemDataRet)
{
	wListItem_p id_p;
	wIndex_t inx = bl->last;
	const char * entry_value = "";
	void * item_data = NULL;


	g_assert(bl != NULL);
	g_assert(bl->listStore != NULL);

	if (bl->type == B_DROPLIST && bl->editted) {
		entry_value = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(
		                bl->widget))));
		item_data = NULL;
		inx = bl->last = -1;
	} else {
		int count = gtk_tree_model_iter_n_children(
		                    GTK_TREE_MODEL(bl->listStore), NULL);
		//Make sure in range
		if (bl->last > count-1) {
			bl->last = count-1;
		}
		inx = bl->last;


		if (inx >= 0) {
			id_p = wlibListStoreGetContext(bl->listStore, inx);

			if (id_p==NULL) {
				fprintf(stderr, "wListGetValues - id_p == NULL\n");
				bl->last = -1;
			} else {
				entry_value = id_p->label;
				item_data = id_p->itemData;
			}
		}
	}

	if (labelStr) {
		strncpy(labelStr, entry_value, labelSize);
	}

	if (listDataRet) {
		*listDataRet = bl->data;
	}

	if (itemDataRet) {
		*itemDataRet = item_data;
	}

	return bl->last;
}

/**
 * Check whether row is selected
 * \param b IN widget
 * \param inx IN row
 * \returns TRUE if selected, FALSE if not existant or unselected
 */

wBool_t wListGetItemSelected(
        wList_p b,
        wIndex_t inx)
{
	wListItem_p id_p;

	if (inx < 0) {
		return FALSE;
	}

	id_p = wlibListStoreGetContext(b->listStore, inx);

	if (id_p) {
		return id_p->selected;
	} else {
		return FALSE;
	}
}

/**
 * Count the number of selected rows in list
 *
 * \param b IN widget
 * \returns count of selected rows
 */

wIndex_t wListGetSelectedCount(
        wList_p b)
{
	wIndex_t selcnt, inx;
	int count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(b->listStore), NULL);

	for (selcnt=inx=0; inx<count; inx++)
		if (wListGetItemSelected(b, inx)) {
			selcnt++;
		}

	return selcnt;
}

/**
 * Select all items in list.
 *
 * \param bl IN list handle
 * \return
 */

void wListSelectAll(wList_p bl)
{
	GtkTreeSelection *selection;

	g_assert(bl != NULL);
	// mark all items selected
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(bl->treeView));
	gtk_tree_selection_select_all(selection);
}

/**
 * Set the value for a row in the listbox
 *
 * \param b IN widget
 * \param row IN row to change
 * \param labelStr IN string with new tab separated values
 * \param bm IN icon
 * \param itemData IN data for row
 * \returns TRUE
 */

wBool_t wListSetValues(
        wList_p b,
        wIndex_t row,
        const char * labelStr,
        wIcon_p bm,
        void *itemData)

{
	g_assert(b->listStore != NULL);

	if (b->type == B_DROPLIST) {
		wDropListSetValues(b, row, labelStr, bm, itemData);
	} else {
		wlibListStoreUpdateValues(b->listStore, row, (char *)labelStr, bm);
	}

	return TRUE;
}

/**
 * Remove a line from the list
 * \param b IN widget
 * \param inx IN row
 */

void wListDelete(
        wList_p b,
        wIndex_t inx)

{
	GtkTreeIter iter;

	g_assert(b->listStore != 0);
	g_assert(b->type != B_DROPLIST);

	if (b->type == B_DROPLIST) {
		wNotice("Deleting from dropboxes is not implemented!", "Continue", NULL);
	} else {
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(b->listStore),
		                              &iter,
		                              NULL,
		                              inx);
		gtk_list_store_remove(b->listStore, &iter);
	}

	if (b->last == inx-1) {
		b->last = -1;
	} else if (b->last >= inx) {
		b->last = -1;
	}

	return;
}

/**
 * Get the widths of the columns
 *
 * \param bl IN widget
 * \param colCnt IN number of columns
 * \param colWidths OUT array for widths
 * \returns
 */

int wListGetColumnWidths(
        wList_p bl,
        int colCnt,
        wWinPix_t * colWidths)
{
	//int inx;

	//if (bl->type != B_LIST) {
	//    return 0;
	//}

	//if (bl->colWidths == NULL) {
	//    return 0;
	//}

	//for (inx=0; inx<colCnt; inx++) {
	//    if (inx < bl->colCnt) {
	//        colWidths[inx] = bl->colWidths[inx];
	//    } else {
	//        colWidths[inx] = 0;
	//    }
	//}

	//return bl->colCnt;
	return(0);
}

/**
 * Adds a entry to the list <b> with name <name>.
 *
 * \param b IN widget
 * \param labelStr IN Entry name
 * \param bm IN Entry bitmap
 * \param itemData IN User context
 * \returns
 */

wIndex_t wListAddValue(
        wList_p b,
        const char * labelStr,
        wIcon_p bm,
        void * itemData)
{
	wListItem_p id_p;
	wIndex_t count;

	g_assert(b != NULL);

	id_p = (wListItem_p)g_malloc(sizeof *id_p);
	memset(id_p, 0, sizeof *id_p);
	id_p->itemData = itemData;
	id_p->active = TRUE;

	if (labelStr == NULL) {
		labelStr = "";
	}

	id_p->label = g_strdup(labelStr);
	id_p->listP = b;

	if (b->type == B_DROPLIST) {
		wDropListAddValue(b, (char *)labelStr, id_p);
	} else {
		wlibTreeViewAddRow(b, (char *)labelStr, bm, id_p);
	}

	count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(b->listStore), NULL);
	if (count == 1) {
		b->last = 0;
	}

	return count-1;
}


/**
 * Set the size of the list
 *
 * \param bl IN widget
 * \param w IN width
 * \param h IN height (ignored for droplist)
 */

void wListSetSize(wList_p bl, wWinPix_t w, wWinPix_t h)
{
	//if (bl->type == B_DROPLIST) {
	//    gtk_widget_set_size_request(bl->widget, w, -1);
	//} else {
	//    gtk_widget_set_size_request(bl->widget, w, h);
	//}

	//bl->w = w;
	//bl->h = h;
}

/**
 * Create a multi column list box.
 * if colCnt is set to zero, a single text column is created as default
 *
 * ### Usage in dialogs
 *
 * - Generated: yes
 *
 * ### Options
 * BL_MANY
 * : allow selection of multiple entries
 *
 * \param parent IN     parent window
 * \param x,y  IN       position
 * \param helpStr IN    help string
 * \param labelStr IN   label
 * \param option IN     options
 * \param number IN     number of displayed entries
 * \param width IN      width of list
 * \param colCnt IN     number of columns
 * \param colWidths IN  width of columns
 * \param colRightJust IN justification of columns
 * \param colTitles IN  array of titles for columns
 * \param valueP IN     selected index
 * \param action IN     callback
 * \param data IN       context
 * \returns created list box
 */

wList_p wListCreate(
        wWindow_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char* helpStr,
        const char* labelStr,
        long	option,
        long	number,
        wWinPix_t	width,
        int	colCnt,
        wWinPix_t* colWidths,
        wBool_t* colRightJust,
        const char** colTitles,
        long* valueP,
        wListCallBack_p action,
        void* data)
{
	wList_p bl;
	static wWinPix_t zeroPos = 0;

	g_assert(width != 0);

	bl = g_malloc0(sizeof(struct wList_t));
	bl->valueP = valueP;
	bl->action = action;
	bl->type = B_LIST;


	if (option & BO_USEBUILDER) {
		/** \todo Implement builder support */
	} else {
		GtkTreeSelection* sel;

		if (colCnt <= 0) {
			colCnt = 1;
			colWidths = &zeroPos;
		}

		//bl->colCnt = colCnt;
		//bl->colWidths = (wWinPix_t*)malloc(colCnt * sizeof * (wWinPix_t*)0);
		//memcpy(bl->colWidths, colWidths, colCnt * sizeof * (wWinPix_t*)0);

		/* create the data structure for data */
		bl->listStore = wlibNewListStore(colCnt);
		/* create the widget for the list store */
		bl->treeView = wlibNewTreeView(bl->listStore,
		                               colTitles != NULL,
		                               option & BL_MANY);

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(bl->treeView));

		gtk_tree_selection_set_select_function(sel,
		                                       changeSelection,
		                                       bl,
		                                       NULL);

		wlibTreeViewAddColumns(bl->treeView, colCnt);

		wlibAddColumnTitles(bl->treeView, colTitles);

		//   wlibComputePos((wControl_p)bl);

		bl->widget = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bl->widget),
		                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

		gtk_container_add(GTK_CONTAINER(bl->widget), GTK_WIDGET(bl->treeView));
		//   gtk_widget_set_size_request(bl->widget, width, (number + 1) * ROW_HEIGHT);

		if (labelStr) {
			// bl->labelW = wlibAddLabel((wControl_p)bl, labelStr);
		}
	}
	gtk_widget_show_all(bl->widget);

	wlibAddButton((wControl_p)bl);
	wlibAddHelpString(bl->widget, helpStr);
	wlibAddTooltip(bl->widget, parent->name, helpStr);

	return bl;
}

/**
 * Create a single column list box (not what the names suggests!)
 * \todo Improve or discard totally, in this case, remove from param.c \
 * as well.
 *
 * \param varname1 IN this is a variable
 * \param varname2 OUT and another one that is modified
 * \return    describe the return value
 */

wList_p wComboListCreate(
        wWin_p	parent,		/* Parent window */
        wWinPix_t	x,		/* X-position */
        wWinPix_t	y,		/* Y-position */
        const char 	* helpStr,	/* Help string */
        const char	* labelStr,	/* Label */
        long	option,		/* Options */
        long	number,		/* Number of displayed list entries */
        wWinPix_t	width,		/* Width */
        long	*valueP,	/* Selected index */
        wListCallBack_p action,	/* Callback */
        void 	*data)		/* Context */
{
	wNotice("ComboLists are not implemented!", "Abort", NULL);
	abort();
}


