/** \file treeview.c
 * Basic treeview functionality for dropbox and listbox
 */

/* XTrkCad - Model Railroad CAD
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

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"

wListItem_p 
wlibAllocateListItem(wControl_p b, const char *labelStr, void *itemData)
{
	wListItem_p id_p;

	id_p = (wListItem_p)g_malloc0(sizeof * id_p);
	
	id_p->itemData = itemData;
	id_p->active = TRUE;

	if (labelStr == NULL) {
		labelStr = "";
	}

	id_p->label = g_strdup(labelStr);
	id_p->listP = b;

	return(id_p);
}


/**
 * Get the count of columns in list
 *
 * \param b IN widget
 * \returns count row
 */

int
wTreeViewGetCount(wControl_p b)
{
	return (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(b->attributes.list.listStore), NULL));
}


/**
 * Get the user attributes for a list element
 *
 * \param b IN widget
 * \param inx IN row
 * \returns the user attributes for the specified row
 */

void *
wTreeViewGetItemContext(wControl_p b, int row)
{
	wListItem_p id_p;

	id_p = wlibListItemGet(b->attributes.list.listStore, row, NULL);

	if (id_p) {
		return id_p->itemData;
	} else {
		return NULL;
	}
}


/**
 * Returns the current selected list entry.
 * If <val> if '-1' then no entry is selected.
 *
 * \param b IN widget
 * \returns row of selected entry or -1 if none is selected
 */

wIndex_t wListGetIndex(wControl_p b)
{
	g_assert(b!=NULL);

	return b->attributes.list.last;
}

/**
 * Set an entry in the list to selected.
 *
 * \param b IN widget
 * \param index IN entry if -1 the current selection is cleared
 *
 */

void
wlibTreeViewSetSelected(wControl_p b, int index)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(lcontrol->treeView));

	// unselect current selection if one exists
	if (gtk_tree_selection_count_selected_rows(sel)) {
		gtk_tree_selection_unselect_all(sel);

	}

	if (index != -1) {
		gint childs;

		childs = gtk_tree_model_iter_n_children (GTK_TREE_MODEL(lcontrol->listStore),
		         NULL );

		if(index < childs) {
			gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(lcontrol->listStore),
			                              &iter,
			                              NULL,
			                              index);
			gtk_tree_selection_select_iter(sel,
			                               &iter);
		}
	}
}

/**
 * Create a new tree view for a list store. Titles are enabled optionally.
 *
 * \param ls IN         list store
 * \param showTitles IN add column header
 * \param multiSelection IN enable selecting multiple rows
 * \returns the treeview
 */

GtkTreeView *
wlibNewTreeView(GtkListStore *ls, int showTitles, int multiSelection)
{
	GtkTreeView *treeView;
	GtkTreeSelection *sel;
	g_assert(ls != NULL);

	/* create and configure the tree view */
	treeView = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(ls)));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), showTitles);

	/* set up selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
	gtk_tree_selection_set_mode(sel,
	                            (multiSelection)?GTK_SELECTION_MULTIPLE:GTK_SELECTION_BROWSE);

	return (treeView);
}

/**
 * Create and initialize a column in treeview. Initially all columns are
 * invisible. Visibility is set when values are added to the specific
 * column
 *
 * \param tv IN treeview
 * \param renderer IN renderer to use
 * \param attribute IN attribute for column
 * \param value IN value to set
 */

static void
wlibAddColumn(GtkTreeView *tv, int visibility, GtkCellRenderer *renderer,
              char *attribute, int value)
{
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column,
	                                renderer,
	                                TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, attribute, value);
	gtk_tree_view_column_set_visible(column, visibility);
	gtk_tree_view_column_set_resizable(column, TRUE);

	gtk_tree_view_append_column(GTK_TREE_VIEW(tv), column);

}

/**
 * Add a number of columns to the text view. This includes the bitmap
 * columns and a given number of text columns.
 *
 * \param tv IN tree view
 * \param count IN number of text columns
 * \returns number of columns
 */

int
wlibTreeViewAddColumns(GtkTreeView *tv, int count)
{
	GtkCellRenderer *renderer;
	int i;

	g_assert(tv != NULL);
	renderer = gtk_cell_renderer_pixbuf_new();
	/* first visible column is used for bitmaps */
	wlibAddColumn(tv, FALSE, renderer, "pixbuf", LISTCOL_BITMAP);

	renderer = gtk_cell_renderer_text_new();

	/* add renderers to all columns */
	for (i = 0; i < count; i++) {
		wlibAddColumn(tv, TRUE, renderer, "text", i + LISTCOL_TEXT);
	}

	return i;
}

/**
 * Add the titles to all columns in a tree view.
 *
 * \param tv IN treeview
 * \param titles IN titles
 * \returns number of titles set
 */

int
wlibAddColumnTitles(GtkTreeView *tv, const char **titles)
{
	int i = 0;

	g_assert(tv != NULL);

	if (titles) {
		while (*titles) {
			GtkTreeViewColumn *column;

			column = gtk_tree_view_get_column(GTK_TREE_VIEW(tv), i + 1);

			if (column) {
				gtk_tree_view_column_set_title(column, titles[ i ]);
				i++;
			} else {
				break;
			}
		}
	}

	return i;
}

#define TREEVIEWCOL_BITMAP 0

void
wlibTreeViewShowIcon(GtkTreeView *tv)
{
	GtkTreeViewColumn* column;

	// first column in list store has pixbuf
	column = gtk_tree_view_get_column(tv, TREEVIEWCOL_BITMAP);
	gtk_tree_view_column_set_visible(column, TRUE);
}

/**
 * Add text to the text columns of the tree view and update the context
 * information
 *
 * \param tv IN treeview
 * \param cols IN number of cols to change
 * \param label IN tab separated string of values
 * \param userData IN additional context information
 * \returns
 */

//int
//wlibTreeViewAddData(GtkTreeView *tv, wIcon_p bm, wListItem_p userData, const char* label)
//{
//	GtkListStore* listStore = GTK_LIST_STORE(gtk_tree_view_get_model(tv));
//
//	wlibListStoreAddRow(listStore, bm, userData, label);
//
//	if (bm) {
//		GtkTreeViewColumn *column;
//
//		// first column in list store has pixbuf
//		column = gtk_tree_view_get_column(GTK_TREE_VIEW(tv), TREEVIEWCOL_BITMAP );
//		gtk_tree_view_column_set_visible(column, TRUE);
//	}
//
//	return 0;
//}



/**
 * Add a row to the tree view. As necessary the adjustment is update in
 * order to make sure, that the list box is fully visible or has a
 * scrollbar.
 *
 * \param b IN the list box
 * \param label IN the text labels
 * \param bm IN bitmap to show at start
 * \param id_p IN user attributes
 */


void
wlibTreeViewAddRow(wControl_p control, wIcon_p bm, wListItem_p id_p, const char* label)
{
	//struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(control, list);

	//wlibTreeViewAddData(lcontrol->treeView, bm, id_p, (char*)label);

	//ScrollToLastLine(control);
}

/**
 * Function for handling a selection change. The internal attributes structure
 * for the changed row is updated. If a handler function for the list
 * is given, the attributes for the row are retrieved and passed to that
 * function. This is used to update other fields in a dialog (see Price
 * List for an example).
 *
 * \param selection IN current selection
 * \param model IN
 * \param path IN
 * \param path_currently_selected IN
 * \param attributes IN the list widget
 */

gboolean
changeSelection(GtkTreeSelection *selection,
                GtkTreeModel *model,
                GtkTreePath *path,
                gboolean path_currently_selected,
                gpointer attributes)
{
	GtkTreeIter iter;
	GValue value = { 0 };
	wListItem_p id_p = NULL;
	wControl_p bl = (wControl_p)attributes;
	long row;
	char *text;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(((wControl_p)attributes), list);

	text = gtk_tree_path_to_string(path);
	row = (long)g_ascii_strtoll(text, NULL, 10);
	g_free(text);

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get_value(model, &iter, LISTCOL_DATA, &value);

	id_p = g_value_get_pointer(&value);
	id_p->selected = !path_currently_selected;

	if (id_p->selected) {
		lcontrol->last = row;

		if (lcontrol->valueP) {
			*lcontrol->valueP = row;
		}

		if (lcontrol->action) {
			lcontrol->action(row, id_p->label, 1, bl->context, id_p->itemData);
		}
	}

	return TRUE;
}

void
wlibTreeSelectionChanged(GtkTreeSelection* selection, void* context)
{
	GList* selectedRows;
	GtkTreeModel* model;
	wControl_p control = context;
	struct list* list = CONTROL_GET_ATTRIBUTES_PTR(control, list);

	model = gtk_tree_view_get_model(list->treeView);

	selectedRows = gtk_tree_selection_get_selected_rows(selection, NULL);

	for (unsigned int i = 0; i < g_list_length(selectedRows); i++) {
		GtkTreeIter iter;
		GValue value = { 0 };
		wListItem_p id_p = NULL;
		char* text;
		unsigned row;

		gtk_tree_model_get_iter(model, &iter, g_list_nth_data(selectedRows, i));
		gtk_tree_model_get_value(model, &iter, LISTCOL_DATA, &value);
		id_p = g_value_get_pointer(&value);
		if (id_p) {
			text = gtk_tree_path_to_string(g_list_nth_data(selectedRows, i));
			row = (long)g_ascii_strtoll(text, NULL, 10);
			g_free(text);

			id_p->selected = TRUE;

			if (id_p->selected) {
				list->last = row;

				if (list->valueP) {
					*list->valueP = row;
				}

				if (list->action) {
					list->action(row, id_p->label, 1, control->context, id_p->itemData);
				}
			}
		}
	}

	g_list_free_full(selectedRows, (GDestroyNotify)gtk_tree_path_free);
}
