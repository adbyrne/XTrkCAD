/**
 * \file   combobox.c
 * \brief  Functions for comboboxes
 *
 * \author mf
 * \date   May 2024
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) Dave Bullis 2005, Martin Fischer 2016
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gtkint.h"
#include "i18n.h"

/* define the column count for the tree model */
#define COMBOBOX_TEXTCOLUMNS 1


/**
 * Show the context columns in the combobox. If combobox has an entry field
 * the first text column is not added here as this is done by GTK
 * automatically.
 *
 * \param comboBox	IN	the comboxbox
 * \param columns	IN	number of text columns to create
 * \returns number of additional columns added
 */

int
wlibComboBoxAddColumns(GtkWidget *comboBox, int columns)
{
	int i;
	int start = 0;
	GtkCellRenderer *cell;

	if (gtk_combo_box_get_has_entry(GTK_COMBO_BOX(comboBox))) {
		start = 1;
	}

	/* Create cell renderer. */
	cell = gtk_cell_renderer_text_new();

	for (i = start; i < columns; i++) {
		/* Pack it into the droplist */
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(comboBox), cell, TRUE);

		/* Connect renderer to attributes source */
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(comboBox),
		                               cell,
		                               "text",
		                               LISTCOL_TEXT + i,
		                               NULL);
	}

	return (i);
}

/**
 * Get the number of rows in combobox
 *
 * \param b IN widget
 * \return number of rows
 */

wIndex_t wComboBoxGetCount(wList_p b)
{
	return (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(b->listStore), NULL));
}

/**
 * Clear the whole combobox
 *
 * \param b IN combobox
 * \return
 */

void
wComboBoxClear(wList_p b)
{
	wlibListStoreClear(b->listStore);
}

/**
 * Get the user attributes / context information for a row in the combobox. The
 * context information is stored with the row in a hidden field.
 *
 * \param b		IN widget
 * \param inx	IN row
 * \returns pointer to context information
 */

void *wComboBoxGetItemContext(wList_p b, wIndex_t inx)
{
	GtkTreeIter iter;
	wListItem_p attributes = NULL;

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(b->listStore), &iter, NULL,
	                                  inx)) {
		gtk_tree_model_get(GTK_TREE_MODEL(b->listStore),
		                   &iter,
		                   LISTCOL_DATA, (void *)&attributes,
		                   -1);
	}

	if (attributes) {
		return (attributes->itemData);
	} else {
		return (NULL);
	}
}

/**
 * Add an entry to a combobox list. Only single text entries are allowed
 *
 * \param b		IN the combobox
 * \param text	IN string to add
 * \return    describe the return value
 */

void wComboBoxAddValue(
        wControl_p b,
        char *text,
        wControl_p attributes)
{
	GtkTreeIter iter;
	struct list *list = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	gtk_list_store_append(list->listStore, &iter);	// append new row to tree store

	gtk_list_store_set(list->listStore, &iter,
	                   LISTCOL_TEXT, text,
	                   LISTCOL_DATA, (void *)attributes,
	                   -1);
}

/**
 * Set the value to the entry field of a combobox
 * \param bl	IN combobox
 * \param val	IN string to show
 */

void wListSetValue(
        wControl_p bl,
        const char * val)
{
	struct list* list = CONTROL_GET_ATTRIBUTES_PTR(bl, list);

	list->editted = TRUE;
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(bl->widget))), val);

	if (list->action) {
		list->action(-1, val, 0, bl->context, NULL);
	}
}

/**
 * Makes the <val>th entry (0-origin) the current selection.
 * If <val> if '-1' then no entry is selected.
 *
 * \param b		IN combobox
 * \param val	IN the index
 *
 */

void wComboBoxSetIndex(wControl_p b, int val)
{
	gtk_combo_box_set_active(GTK_COMBO_BOX(b->widget), val);
}

/**
 * Set the values for a row in the combobox
 *
 * \param b			IN combobox
 * \param row		IN index
 * \param labelStr	IN new text
 * \param bm		IN ignored
 * \param itemData	IN ignored
 * \return
 */

wBool_t wComboBoxSetValues(
        wControl_p b,
        wIndex_t row,
        const char * labelStr,
        wIcon_p bm,
        void *itemData)
{
	GtkTreeIter iter;
	struct list *lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);

	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(lcontrol->listStore), &iter,
	                                  NULL,
	                                  row)) {
		gtk_list_store_set(lcontrol->listStore,
		                   &iter,
		                   LISTCOL_TEXT, labelStr,
		                   -1);
		return (TRUE);
	} else {
		return (FALSE);
	}
}

/**
 * Signal handler for the "changed"-signal in combobox.
 * Gets the selected text and determines the selected row in the tree model.
 * Or handles user entered text.
 *
 * \param comboBox  IN the combobox
 * \param context      IN user context / pointer to the control
 * \return
 */

static int ComboBoxChanged(
        GtkComboBox * comboBox,
        gpointer attributes)
{
	wControl_p bl = (wControl_p)attributes;
	GtkTreeIter iter;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(bl, list);

	wIndex_t inx = 0;
	gchar *string = NULL;
	wListItem_p listItemP = NULL;

	/* Obtain currently selected item from combobox. */
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(comboBox), &iter)) {
		GtkTreeModel *model;

		/* Obtain attributes model from combobox. */
		model = gtk_combo_box_get_model(comboBox);

		/* get the selected row */
		string = gtk_tree_model_get_string_from_iter(model,
		         &iter);
		inx = atoi(string);
		g_free(string);
		string = NULL;

		/* Obtain string from model. */
		gtk_tree_model_get(model, &iter,
		                   LISTCOL_TEXT, &string,
		                   LISTCOL_DATA, (void *)&listItemP,
		                   -1);
		lcontrol->editted = FALSE;

	} else {
		/* Nothing selected, user is entering text directly */
		inx = -1;
		GtkEntry * entry = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(bl->widget)));
		if ( entry == NULL ) {
			return 0;
		}
		const char * string1 = gtk_entry_get_text(entry);
		if ( string1 == NULL ) {
			return 0;
		}
		string = g_strdup(string1);
		lcontrol->editted = TRUE;
	}

	/* selection changed, store new selections and call back */
	if (lcontrol->last != inx || lcontrol->editted == TRUE) {
		lcontrol->last = inx;

		if (lcontrol->valueP) {
			*lcontrol->valueP = inx;
		}

		/* selection changed -> callback */
		if (string && lcontrol->action) {
			lcontrol->action(inx, string, 1, bl->context,
			                 listItemP?listItemP->itemData:NULL);
		}
	}

	if ( string ) {
		g_free(string);
	}
	return 1;
}

/**
 * Create a combobox for a given liststore
 *
 * \param ls        IN list store for combobox
 * \param editable  IN combobox with entry field
 * \returns the newly created widget
 */

GtkWidget *
wlibNewComboBox(GtkListStore *ls, int editable)
{
	GtkWidget *widget;

	if (editable) {
		widget = gtk_combo_box_new_with_model_and_entry(GTK_TREE_MODEL(ls));
	} else {
		widget = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ls));
	}

	return (widget);
}

/**
 * Signal handler for the "changed"-signal in combobox's entry field.
 * Get the entered text and calls the 'action' for handling of entered
 * value.
 * *
 * \param entry	IN entry field of the combobox
 * \param context	IN pointer to control
 * \return
 */

static void ComboBoxEntryEntered(
        GtkEntry *entry,
        gpointer userData)
{
	const gchar *text;
	wControl_p c = userData;
	struct list* lcontrol = CONTROL_GET_ATTRIBUTES_PTR(c, list);

	text = gtk_entry_get_text(entry);

	if (text && *text != '\0') {
		gchar *copyOfText = g_strdup(text);
		lcontrol->editted = TRUE;
		lcontrol->action(-1, copyOfText, 1, ((wControl_p)userData)->context, NULL);
		g_free((gpointer)copyOfText);
	} else {
		wBeep();
	}
}

/**
 * Create a combobox. The combobox is created having one text column.
 *
 * ### Usage in dialogs
 *
 * - Generated: yes
 * - Builder: yes
 *
 * ### Options
 * BL_EDITABLE
 * : add and edit field to the combobox
 *
 *	\param IN parent    Parent window
 *	\param IN x, y      position
 *	\param IN helpStr   Help string
 *	\param IN labelStr  Label
 *	\param IN option    Options
 *	\param IN number    unused
 *	\param IN width     Width
 *	\param IN valueP    selected index
 *	\param IN action    Callback
 *	\param IN context      Context
 */

wControl_p wComboBoxCreate(
        wControl_p	parent,
        wWinPix_t	x,
        wWinPix_t	y,
        const char 	* helpStr,
        const char	* labelStr,
        long	option,
        long	number,
        wWinPix_t	width,
        long	*valueP,
        wListCallBack_p action,
        void 	*context)
{
	wControl_p b;
	struct list* lcontrol;

	b = wlibControlNew(B_COMBOBOX, parent, helpStr, context);
	lcontrol = CONTROL_GET_ATTRIBUTES_PTR(b, list);
	lcontrol->valueP = valueP;
	lcontrol->action = action;
	lcontrol->last = -1;

	if(HASDIALOGBUILDER(parent)) {
		b->widget = wlibWidgetFromIdWarn(parent, helpStr);
		lcontrol->listStore = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(b->widget)));
		if (!lcontrol->listStore) {
			abort();
		}

	} else {

		// create tree store for storing the contents
		lcontrol->listStore = wlibNewListStore(COMBOBOX_TEXTCOLUMNS);

		if (!lcontrol->listStore) {
			abort();
		}

		// create the droplist
		b->widget = wlibNewComboBox(lcontrol->listStore,
		                            option & BL_EDITABLE);

		if (b->widget == 0) {
			abort();
		}


		// combo's style
		//gtk_rc_parse_string("style \"my-style\" { GtkComboBox::appears-as-list = 1 } widget \"*.mycombo\" style \"my-style\"  ");
		// gtk_widget_set_name(b->widget, "mycombo");

		wlibBasicGridAttach(parent, b->widget, x, y, width, 1);

		if (labelStr) {
			wlibAddLabel((wControl_p)b, x-1, y, labelStr);
		}

		gtk_widget_show(b->widget);
	}
	wlibAddHelpString(b->widget, helpStr);

	g_object_ref_sink(lcontrol->listStore);
	g_object_unref(G_OBJECT(lcontrol->listStore));

	wlibComboBoxAddColumns(b->widget, COMBOBOX_TEXTCOLUMNS);

	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(b->widget),
	                                    LISTCOL_TEXT);

	g_signal_connect(G_OBJECT(b->widget), "changed",
	                 G_CALLBACK(ComboBoxChanged), b);

//	wlibAddTooltip(b->widget, parent->name, helpStr);
	return b;
}

