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
#include <wlib.h>
#include <datastore.h>
#include "gtkint.h"
#include "i18n.h"
#include "dynstring.h"

int iDebugList = 0;

struct listSearch {
	const char *search;
	char *result;
	int row;
};

static void
ScrollToLastLine(wControl_p control)
{
	GtkAdjustment* adj;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(control, list);

	adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(control->widget));
	lcontrol->last = gtk_tree_model_iter_n_children(gtk_tree_view_get_model(
	                         GTK_TREE_VIEW(
	                                 lcontrol->treeView)), NULL);

	if (gtk_adjustment_get_upper(adj) < gtk_adjustment_get_step_increment(adj) *
	    lcontrol->last + 1) {
		gtk_adjustment_set_upper(adj,
		                         gtk_adjustment_get_upper(adj) +
		                         gtk_adjustment_get_step_increment(adj));
	}
}

static int
GetRowCount(struct list* lcontrol)
{
	int count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(lcontrol->listStore),
	                NULL);
	if (count == 1) {
		lcontrol->last = 0;
	}
	return(count);
}



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
        wControl_p b)
{
	g_assert(b!= NULL);

	struct list *lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	wlibListStoreClear(lcontrol->listStore);
	lcontrol->last = -1;
}

/**
 * Makes the <val>th entry (0-origin) the current selection.
 * If <val> if '-1' then no entry is selected.
 * \param b IN List
 * \param element IN Index
 */

void wListSetIndex(
        wControl_p b,
        int element)
{
	struct list *lcontrol;
	if (b->widget == 0) {
		abort();
	}

	lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	if (b->type == B_COMBOBOX) {
		wComboBoxSetIndex(b, element);
	} else {
		wlibTreeViewSetSelected(b, element);
	}

	lcontrol->last = element;
}

/**
 * CompareListData is called when a list is searched for a specific
 * context entry. It is called in sequence and does a string compare
 * between the label of the current row and the search argument. If
 * identical the label is placed in the search argument.
 * It is a GTK foreach() function.
 *
 * \param model IN searched model
 * \param path IN unused
 * \param iter IN current iterator
 * \param context IN/OUT pointer to context structure with search criteria
 * \return TRUE if identical, FALSE otherwise
 */

int
CompareListData(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                gpointer attributes)
{
	struct listSearch *search = (struct listSearch *)attributes;
	DynString strKey;
	DynStringMalloc( &strKey, 0 );
	gint iColCnt = gtk_tree_model_get_n_columns (model);

	for ( int inx = LISTCOL_TEXT; inx < iColCnt; inx++ ) {
		char * strP;
		gtk_tree_model_get(model,
		                   iter,
		                   inx,
		                   &strP,
		                   -1 );
		if ( inx != LISTCOL_TEXT ) {
			DynStringCatCStr( &strKey, "\t" );
		}
		DynStringCatCStr( &strKey, strP );
	}
	if ( iDebugList >= 4 ) {
		printf( "  CompareListData %s <> %s\n", DynStringToCStr(&strKey),
		        search->search );
	}
	if ( !strcmp( DynStringToCStr(&strKey), search->search ) ) {
		search->result = g_strdup(&strKey);
	} else {
		search->result = NULL;
		search->row++;
	}
	DynStringFree( &strKey );
	return search->result != NULL;
}

/**
 * Find the row which contains the specified text.
 *
 * \param b IN
 * \param val IN
 * \returns found row or -1 if not found
 */

wIndex_t wListFindValue(
        wControl_p b,
        const char * val)
{
	struct listSearch thisSearch;

	g_assert(b!=NULL);
	g_assert(b->attributes.list.listStore!=NULL);


	if ( iDebugList >= 3 ) {
		printf( "wListFindValue \"%s\"?\"%s\"\n",
		        b->name,  val );
	}
	thisSearch.search = val;
	thisSearch.row = 0;

	gtk_tree_model_foreach(GTK_TREE_MODEL(b->attributes.list.listStore),
	                       CompareListData,
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
        wControl_p b)
{
	if (b->type == B_COMBOBOX) {
		return wComboBoxGetCount(b);
	} else {
		return wTreeViewGetCount(b);
	}
}

/**
 * Get the user context for a list element
 *
 * \param b IN widget
 * \param inx IN row
 * \returns the user context for the specified row
 */

void * wListGetItemContext(
        wControl_p b,
        wIndex_t inx)
{
	if (inx < 0) {
		return NULL;
	}

	if (b->type == B_COMBOBOX) {
		return wComboBoxGetItemContext(b, inx);
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
        wControl_p bl,
        char * labelStr,
        int labelSize,
        void * * listDataRet,
        void * * itemDataRet)
{
	wListItem_p id_p;
	wIndex_t inx;
	const char * entry_value = "";
	void * item_data = NULL;
	struct list* lcontrol;

	g_assert(bl != NULL);
	g_assert(bl->attributes.list.listStore != NULL);

	lcontrol = CONTROL_GET_ATTRIBUTES_PTR(bl, list);
	inx = lcontrol->last;

	if (bl->type == B_COMBOBOX && lcontrol->editted) {

		if (gtk_combo_box_get_has_entry(GTK_COMBO_BOX(bl->widget))) {
			/* Nothing selected, user is entering text directly */
//			inx = -1;
			GtkEntry* entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(bl->widget)));
			if (entry == NULL) {
				return 0;
			}
			const char* string1 = gtk_entry_get_text(entry);
			if (string1 == NULL) {
				return 0;
			}
		}


		GtkBin *bin = GTK_BIN( bl->widget );
		GtkCellView *cellView = gtk_bin_get_child( bin );
		GtkEntry *entry = GTK_ENTRY( cellView );
		entry_value = gtk_entry_get_text( entry );
		//entry_value = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN( bl->widget))));
		item_data = NULL;
		inx = lcontrol->last = -1;
	} else {
		int count = gtk_tree_model_iter_n_children(
		                    GTK_TREE_MODEL(lcontrol->listStore), NULL);
		//Make sure in range
		if (lcontrol->last > count-1) {
			lcontrol->last = count-1;
		}
		inx = lcontrol->last;


		if (inx >= 0) {
			id_p = wlibListStoreGetContext(lcontrol->listStore, inx);

			if (id_p==NULL) {
				if ( iDebugList >= 1 ) {
					fprintf(stderr, "wListGetValues - id_p == NULL\n");
				}
				lcontrol->last = -1;
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
		*listDataRet = bl->context;
	}

	if (itemDataRet) {
		*itemDataRet = item_data;
	}

	return lcontrol->last;
}

/**
 * Check whether row is selected
 * \param b IN widget
 * \param inx IN row
 * \returns TRUE if selected, FALSE if not existant or unselected
 */

wBool_t wListGetItemSelected(
        wControl_p listControl,
        wIndex_t inx)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	struct list* list = CONTROL_GET_ATTRIBUTES_PTR(listControl, list);

	if (inx < 0) {
		return FALSE;
	}

	gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list->listStore), &iter, NULL,
	                              inx);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list->treeView));
	return(gtk_tree_selection_iter_is_selected(selection, &iter));


	//id_p = wlibListStoreGetContext(listControl->listStore, inx);

	//if (id_p) {
	//	return id_p->selected;
	//} else {
	//	return FALSE;
	//}
}

/**
 * Count the number of selected rows in list
 *
 * \param b IN widget
 * \returns count of selected rows
 */

wIndex_t wListGetSelectedCount(
        wControl_p b)
{
	wIndex_t selcnt = 0;
	GtkTreeSelection* selection;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	g_assert(b != NULL);
	g_assert(b->type == B_LIST);

	selection = gtk_tree_view_get_selection(lcontrol->treeView);
	selcnt = gtk_tree_selection_count_selected_rows(selection);

	return selcnt;
}

/**
 * Select all items in list.
 *
 * \param bl IN list handle
 * \return
 */

void wListSelectAll(wControl_p bl)
{
	GtkTreeSelection *selection;

	g_assert(bl != NULL);
	// mark all items selected
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
	                bl->attributes.list.treeView));
	gtk_tree_selection_select_all(selection);
}

/**
 * Set the value for a row in the listbox
 *
 * \param b IN widget
 * \param row IN row to change
 * \param labelStr IN string with new tab separated values
 * \param bm IN icon
 * \param itemData IN context for row
 * \returns TRUE
 */

wBool_t wListSetValues(
        wControl_p b,
        wIndex_t row,
        const char * labelStr,
        wIcon_p bm,
        void *itemData)
{
	g_assert(b->attributes.list.listStore != NULL);

	if (b->type == B_COMBOBOX) {
		wComboBoxSetValues(b, row, labelStr, bm, itemData);
	} else {
		wlibListStoreUpdateValues(b->attributes.list.listStore, row, (char *)labelStr,
		                          bm);
	}

	return TRUE;
}

static void
remove_row(GtkTreeRowReference* ref, GtkTreeModel* model)
{
	GtkTreeIter iter;
	GtkTreePath* path;

	path = gtk_tree_row_reference_get_path(ref);
	gtk_tree_model_get_iter(model, &iter, path);

	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

}

static void
free_row_reference_cb(gpointer data, gpointer user_data)
{
	gtk_tree_row_reference_free((GtkTreeRowReference*)data);
}

static void
free_tree_path_cb(gpointer data, gpointer user_data)
{
	gtk_tree_path_free((GtkTreePath*)data);
}

/* delete all selected rows from a list store, thanks to Andrew Krause */

void
wListDeleteSelected(wControl_p list)
{
	GtkTreeSelection* selection;
	GtkTreeModel* model;
	GtkTreeRowReference* ref;
	GList* rows, *ptr, *references = NULL;

	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(list, list);

	selection = gtk_tree_view_get_selection(lcontrol->treeView);
	model = gtk_tree_view_get_model(lcontrol->treeView);
	rows = gtk_tree_selection_get_selected_rows(selection, &model);

	ptr = rows;
	while (ptr != NULL) {
		ref = gtk_tree_row_reference_new(model, (GtkTreePath*)ptr->data);
		references = g_list_prepend(references, gtk_tree_row_reference_copy(ref));
		gtk_tree_row_reference_free(ref);
		ptr = ptr->next;
	}

	g_list_foreach(references, (GFunc)remove_row, model);

	g_list_foreach(references, free_row_reference_cb, NULL);
	g_list_foreach(rows, free_tree_path_cb, NULL);
	g_list_free(references);
	g_list_free(rows);

	gtk_tree_selection_unselect_all(selection);
}

/**
 * Remove a line from the list
 * \param b IN widget
 * \param inx IN row
 */

void wListDelete(
        wControl_p b,
        wIndex_t inx)
{
	GtkTreeIter iter;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	g_assert(b->attributes.list.listStore != 0);

	if (b->type == B_COMBOBOX) {
		wNotice("Deleting from dropboxes is not implemented!", "Continue", NULL);
	} else {
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(lcontrol->listStore),
		                              &iter,
		                              NULL,
		                              inx);

		gtk_list_store_remove(lcontrol->listStore, &iter);
	}

	if (lcontrol->last == inx-1) {
		lcontrol->last = -1;
	} else if (lcontrol->last >= inx) {
		lcontrol->last = -1;
	}

	return;
}

#define FIXEDCOLUMNS 2

unsigned int
wListGetColumnCount(wControl_p listControl)
{
	GList* columns;
	int count;

	columns = gtk_tree_view_get_columns(listControl->attributes.list.treeView);
	count = g_list_length(columns) - FIXEDCOLUMNS;
	g_list_free(columns);

	return(count+1);
}

static wWinPix_t
TreeViewGetColumnWidth(GtkTreeView *treeView, unsigned index)
{
	wWinPix_t width = 0;

	GtkTreeViewColumn* column = gtk_tree_view_get_column(treeView, index);
	if (column) {
		width = gtk_tree_view_column_get_width(column);
	}
	return(width);
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
        wControl_p bl,
        unsigned int colCnt,
        wWinPix_t * colWidths)
{
	g_assert(bl->type == B_LIST);

	for (unsigned int i = 0; i < colCnt; i++) {
		colWidths[i] = TreeViewGetColumnWidth(bl->attributes.list.treeView,
		                                      i + FIXEDCOLUMNS-1);
	}

	return(0);
}

GtkTreeIter
NewListRow(wControl_p control, const char* labelStr, void* addInfo)
{
	GtkTreeIter iter;
	wListItem_p itemData;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(control, list);

	itemData = wlibAllocateListItem(control, labelStr,
	                                addInfo); /** \todo Check and rework usage of wListItem */
	wlibListStoreAppendRow(lcontrol->listStore, &iter, itemData);

	return(iter);
}

static void
AddIconToRow(struct list *lcontrol, GtkTreeIter *iterPointer, wIcon_p bm)
{

	if (bm) {
		wlibListStoreSetIcon(lcontrol->listStore, iterPointer, bm);
		wlibTreeViewShowIcon(lcontrol->treeView);
	}
}

#ifdef TODO_UNUSED
static void
AddDataToRow(struct list* lcontrol, GtkTreeIter* iterPointer,
             const char* labelStr, va_list arguments)
{
	int column = 0;
	while (labelStr) {
		wlibListStoreSetData(lcontrol->listStore, iterPointer, column, labelStr);
		column++;
		labelStr = va_arg(arguments, char*);
	}
}
#endif

static void
AddDataArrayToRow(struct list* lcontrol, GtkTreeIter* iterPointer, char **data)
{
	char* value = *data;
	int column = 0;

	while (value) {
		wlibListStoreSetData(lcontrol->listStore, iterPointer, column, value);
		column++;
		value = data[column];
	}
}

wIndex_t
wListAddValuesArr(wControl_p b,
                  wIcon_p bm,
                  void* itemInfo,
                  char** values)
{
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);
	GtkTreeIter iter;

	g_assert(b != NULL);
	g_assert(b->type == B_LIST);

	iter = NewListRow(b, values[0], itemInfo);

	AddIconToRow(lcontrol, &iter, bm);
	AddDataArrayToRow(lcontrol, &iter, values);

	ScrollToLastLine(b);

	return(GetRowCount(lcontrol) - 1);
}




/**
 * Add a row to a list having several columns. Each string is added to a additionalValues. Columns are
 * expected to be consecutive.
 *
 * \param b		IN widget
 * \param bm	IN Entry bitmap
 * \param itemData IN User context
 * \param labelStr IN label
 * \param		IN variable number of strings, terminated with NULL
 *
 * \return		row count
 */
wIndex_t wListAddValueVar(
        wControl_p b,
        wIcon_p bm,
        void* itemInfo,
        const char* labelStr,
        ...)
{
	va_list arguments;
	unsigned additionalValues = 0;
	char** data = NULL;
	wIndex_t rowCount = 0;

	g_assert(b != NULL);
	g_assert(b->type == B_LIST);

	va_start(arguments, labelStr);
	while (va_arg(arguments, char *)) {
		additionalValues++;
	}
	va_end(arguments);

	//add 2 as array will hold labelStrplus terminating NULL
	data = g_malloc0((additionalValues+2) * sizeof(char*));
	if ( iDebugList >= 2 ) {
		printf( "wListAddValueVar\n" );
	}
	if (data) {
		data[0] = (char *)labelStr;
		va_start(arguments, labelStr);
		for (unsigned i = 1; i <= additionalValues; i++) {
			char* arg = va_arg(arguments, char*);
			data[i] = arg;
		}
		va_end(arguments);

		rowCount = wListAddValuesArr(b, bm, itemInfo, data);

		g_free(data);
	}

	return(rowCount);
}

/**
 * Adds a entry to the list with label. In case the labelStr holds tab-separated values these
 * are split into an NULL terminated array
 *
 * \param b		IN widget
 * \param bm	IN Entry bitmap
 * \param itemData IN User context
 * \param labelStr IN label
 *
 * \returns number of rows
 */

wIndex_t wListAddValue(
        wControl_p b,
        const char* labelStr,
        wIcon_p bm,
        void* itemInfo)
{
	wIndex_t rowCount;
	gchar** data;

	g_assert(b != NULL);
	g_assert(b->type == B_LIST);
	if ( iDebugList >= 2 ) {
		printf( "wListAddValue \"%s\" =  \"%s\"\n", b->name, labelStr );
	}
	data = g_strsplit(labelStr, "\t", -1);

	rowCount = wListAddValuesArr(b, bm, itemInfo, data);

	g_strfreev(data);

	return(rowCount);
}


/**
 * Set the size of the list
 *
 * \param bl IN widget
 * \param w IN width
 * \param h IN height (ignored for droplist)
 */

void wListSetSize(wControl_p bl, wWinPix_t w, wWinPix_t h)
{
	//if (bl->type == B_DROPLIST) {
	//    gtk_widget_set_size_request(bl->widget, w, -1);
	//} else {
	//    gtk_widget_set_size_request(bl->widget, w, h);
	//}

	//bl->w = w;
	//bl->h = h;
}

void
wlibTreeViewEdited(GtkWidget* renderer, const char* path, const char* new_text,
                   void (*cb)( int row, const char *text))
{
	// for a simple listbox the path is the current row
	(*cb)(atoi(path), new_text);
}

/**
 * Connect the list to a existing liststore.
 */

void
wListSetStore(wControl_p list, DataStore *store)
{
	g_assert(list);
	g_assert(store);
	g_assert(list->type==B_LIST);

	GtkTreeView* treeview = list->attributes.list.treeView;

	list->attributes.list.listStore = store->listStore;

	gtk_tree_view_set_model(treeview,
	                        GTK_TREE_MODEL(store->listStore));

	if (store->selectionChanged) {
		GtkTreeSelection* selection;

		selection = gtk_tree_view_get_selection(treeview);
		g_signal_connect(selection, "changed", (GCallback)wlibTreeSelectionChanged,
		                 NULL);
	}

	if (store->edited) {

		GtkWidget *renderer = wlibWidgetFromIdWarn(list->parent, store->editable);

		g_signal_connect( renderer, "edited", (GCallback)wlibTreeViewEdited,
		                  store->edited);
	}

}

/**
 * Create a multi column list box.
 * if colCnt is set to zero, a single text column is created as default
 *
 * ### Usage in dialogs
 *
 * - Builder: yes
 * - Runtime: yes
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
 * \param colCnt IN     number of text columns for user data, ignored for builder defined
 * \param colWidths IN  width of columns, ignored for builder defined
 * \param colRightJust IN justification of columns, ignored for builder defined
 * \param colTitles IN  array of titles for columns
 * \param valueP IN     selected index
 * \param action IN     callback
 * \param context IN       context
 * \returns created list box
 */

wControl_p wListCreate(
        wControl_p	parent,
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
        void* context)
{
	wControl_p bl;
	struct list* lcontrol;
	static wWinPix_t zeroPos = 0;


	bl = wlibControlNew(B_LIST, parent, helpStr, context);
	lcontrol = CONTROL_GET_ATTRIBUTES_PTR(bl, list);
	lcontrol->valueP = valueP;
	lcontrol->action = action;
	lcontrol->last = 0;

	if (ISDEFINEDINBUILDER(parent)) {

		bl->widget = wlibWidgetFromIdWarn(parent, helpStr);
		lcontrol->treeView = GTK_TREE_VIEW(wlibWidgetFromIdWarn(parent, "treeview"));
		g_assert(lcontrol->treeView != NULL);

		if (option & BL_NODATASTORE) {
			lcontrol->listStore = NULL;
		} else {
			GtkTreeSelection* selection;

			lcontrol->listStore = GTK_LIST_STORE(gtk_tree_view_get_model(
			                lcontrol->treeView));

			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(lcontrol->treeView));
			g_signal_connect(selection, "changed", (GCallback)wlibTreeSelectionChanged, bl);
		}

	} else {
		GtkTreeSelection* sel;

		g_assert(width != 0);

		if (colCnt <= 0) {
			colCnt = 1;
			colWidths = &zeroPos;
		}

		//bl->colCnt = colCnt;
		//bl->colWidths = (wWinPix_t*)malloc(colCnt * sizeof * (wWinPix_t*)0);
		//memcpy(bl->colWidths, colWidths, colCnt * sizeof * (wWinPix_t*)0);

		/* create the attributes structure for context */
		lcontrol->listStore = wlibNewListStore(colCnt);
		/* create the widget for the list store */
		lcontrol->treeView = wlibNewTreeView(lcontrol->listStore,
		                                     colTitles != NULL,
		                                     option & BL_MANY);

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(lcontrol->treeView));

		gtk_tree_selection_set_select_function(sel,
		                                       changeSelection,
		                                       bl,
		                                       NULL);

		wlibTreeViewAddColumns(lcontrol->treeView, colCnt);
		wlibAddColumnTitles(lcontrol->treeView, colTitles);


		bl->widget = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bl->widget),
		                               GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

		gtk_container_add(GTK_CONTAINER(bl->widget), GTK_WIDGET(lcontrol->treeView));

		if (labelStr) {
			// bl->labelW = wlibAddLabel((wControl_p)bl, labelStr);
		}
	}

	gtk_widget_show_all(bl->widget);

	wlibAddTooltip(bl->widget, parent->name, helpStr);
	//wlibAddTooltip(bl->widget, helpStr);

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

wControl_p wComboListCreate(
        wControl_p	parent,		/* Parent window */
        wWinPix_t	x,		/* X-position */
        wWinPix_t	y,		/* Y-position */
        const char 	* helpStr,	/* Help string */
        const char	* labelStr,	/* Label */
        long	option,		/* Options */
        long	number,		/* Number of displayed list entries */
        wWinPix_t	width,		/* Width */
        long	*valueP,	/* Selected index */
        wListCallBack_p action,	/* Callback */
        void 	*attributes)		/* Context */
{
	wNotice("ComboLists are not implemented!", "Abort", NULL);
	abort();
}


