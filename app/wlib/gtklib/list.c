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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "gtkint.h"
#include "i18n.h"


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
		wList_p b )		
{
	assert( b!= NULL);

	b->recursion++;

	if ( b->type == B_DROPLIST ) {
		wDropListClear( b );
	}	
	else {
		wTreeViewClear( b );
	}	

	b->recursion--;
	b->last = -1;
	b->count = 0;
}

/**
 * Makes the <val>th entry (0-origin) the current selection.
 * If <val> if '-1' then no entry is selected.
 * \param b IN List
 * \param element IN Index
 */

void wListSetIndex(
	wList_p b,		
	int element )		
{
	int cur;
	wListItem_p id_p;
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	
	if (b->widget == 0) abort();
	b->recursion++;

	if ( b->type == B_DROPLIST ) {
		wDropListSetIndex( b, element );
	} else {
		wlibTreeViewToggleSelected( b, element );
	}
	b->last = element;
	b->recursion--;
}

int
CompareListData( GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data ) 
{
	wListItem_p id_p;
	GValue value = { 0 };
	
	g_value_init( &value, G_TYPE_STRING );
//	listItem = gtk_tree_model_get_value( model, iter, 
	
//	id_p = (wListItem_p)g_object_get_data(listItem, ListItemDataKey );
	if ( id_p && id_p->label && strcmp( id_p->label, data ) == 0 ) {
		return TRUE;
	} else {
		return FALSE;
	}	
}

/**
 * Find the row which contains the specified text
 * 
 * \param b IN 
 * \param val IN 
 * \returns found row or -1 if not found
 */

wIndex_t wListFindValue(
		wList_p b,
		const char * val )
{
	GList * child;
    GtkTreeIter iter;

	GtkObject * listItem;

	wIndex_t inx;

	assert(b!=NULL);
	assert( b->listStore!=NULL);
	
	gtk_tree_model_foreach(GTK_TREE_MODEL(b->listStore), CompareListData, (void *)val );

	return -1;
}



/**
 * Return the number of rows in the list
 * 
 * \param b IN widget
 * \returns number of rows
 */

wIndex_t wListGetCount(
	wList_p b )
{
	if ( b->type == B_DROPLIST )
		return wDropListGetCount( b );
	else	
		return wTreeViewGetCount( b );
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
		wIndex_t inx )
{
	wListItem_p id_p;
	if ( inx < 0 )
		return NULL;
		
	if( b->type == B_DROPLIST ) {
		return wDropListGetItemContext( b, inx );
	} else {
		return wTreeViewGetItemContext( b, inx );		
	}		
}

/**
 * Check whether row is selected
 * \param b IN widget
 * \param inx IN row
 * \returns TRUE if selected, FALSE if not existant or unselected
 */

wBool_t wListGetItemSelected(
	wList_p b,
	wIndex_t inx )
{
	wListItem_p id_p;
	if ( inx < 0 )
		return FALSE;
	id_p = wlibListStoreGetContext( b->listStore, inx );
	if ( id_p )
		return id_p->selected;
	else
		return FALSE;
}

/**
 * Count the number of selected rows in list
 * 
 * \param b IN widget
 * \returns count of selected rows
 */

wIndex_t wListGetSelectedCount(
	wList_p b )
{
	wIndex_t selcnt, inx;
	for ( selcnt=inx=0; inx<b->count; inx++ )
		if ( wListGetItemSelected( b, inx ) )
			selcnt++;
	return selcnt;
}

/**
 * Select all items in list.
 *
 * \param bl IN list handle
 * \return
 */

void wListSelectAll( wList_p bl )
{
	wIndex_t inx;
	wListItem_p ldp;
	GtkTreeSelection *selection;

	assert( bl != NULL );
	// mark all items selected
	selection = gtk_tree_view_get_selection ( GTK_TREE_VIEW(bl->treeView));
	gtk_tree_selection_select_all (selection);	
	
	// and synchronize the internal data structures
	wListGetCount(bl);
	for ( inx=0; inx<bl->count; inx++ ) {
		ldp = wlibListStoreGetContext( bl->listStore, inx );
		if( ldp )
			ldp->selected = TRUE;
	}
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
		void *itemData )

{
	wListItem_p id_p;

	assert(b->listStore != NULL );
	
	b->recursion++;
	
	if( b->type == B_DROPLIST ) {
		wDropListSetValues( b, row, labelStr, bm, itemData );
	} else {
		wlibListStoreUpdateValues( b->listStore, row, b->colCnt, (char *)labelStr, bm );
	}
	b->recursion--;
	return TRUE;
}

/**
 * Remove a line from the list 
 * \param b IN widget
 * \param inx IN row
 */

void wListDelete(
	wList_p b,
	wIndex_t inx )

{
	wListItem_p id_p;
	GList * child;
	GtkTreeIter iter;

	assert (b->listStore != 0);
	b->recursion++;
	
	if ( b->type == B_DROPLIST ) {
//		id_p = getListItem( b, inx, &child );
		if (id_p != NULL) {
			gtk_container_remove( GTK_CONTAINER(b->listStore), child->data );
			b->count--;
			g_free(id_p);
		}
	} else {
		gtk_tree_model_iter_nth_child (GTK_TREE_MODEL(b->listStore),
							   &iter,
							   NULL,
							   inx);
		gtk_list_store_remove( b->listStore, &iter );
		b->count--;
	}
	b->recursion--;
	return;
}

/**
 * Get the widths of the columns 
 * 
 * @param bl IN widget
 * @param colCnt IN number of columns
 * @param colWidths OUT array for widths 
 * @returns 
 */

int wListGetColumnWidths(
		wList_p bl,
		int colCnt,
		wPos_t * colWidths )
{
	int inx;

	if ( bl->type != B_LIST )
		return 0;
	if ( bl->colWidths == NULL )
		return 0;
	for ( inx=0; inx<colCnt; inx++ ) {
		if ( inx < bl->colCnt ) {
			colWidths[inx] = bl->colWidths[inx];
		} else {
			colWidths[inx] = 0;
		}
	}
	return bl->colCnt;
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
	void * itemData )
{
	wListItem_p id_p;
	

	int i;

	assert( b != NULL );
	
	b->recursion++;

	id_p = (wListItem_p)g_malloc( sizeof *id_p );
	memset( id_p, 0, sizeof *id_p );
	id_p->itemData = itemData;
	id_p->active = TRUE;
	if ( labelStr == NULL )
		labelStr = "";
	id_p->label = strdup( labelStr );
	id_p->listP = b;
		
	if ( b->type == B_DROPLIST ) {
		wDropListAddValue( b, (char *)labelStr, id_p );
	} else {
		wlibTreeViewAddRow( b, (char *)labelStr, bm, id_p );
	}

	b->count++;
	b->recursion--;

	if ( b->count == 1 ) {
		b->last = 0;
	}

	return b->count-1;
}


/**
 * Set the size of the list 
 * 
 * \param bl IN widget
 * \param w IN width
 * \param h IN height (ignored for droplist)
 */

void wListSetSize( wList_p bl, wPos_t w, wPos_t h )
{
	if (bl->type == B_DROPLIST) {
		gtk_widget_set_size_request( bl->widget, w, -1 );
	} else {
		gtk_widget_set_size_request( bl->widget, w, h );
	}
	bl->w = w;
	bl->h = h;
}


//static int unselectCList(
		//GtkWidget * clist,
		//int row,
		//int col,
		//GdkEventButton* event,
		//gpointer data )
//{
	//wList_p bl = (wList_p)data;
	//wListItem_p id_p;

	//if (gdk_pointer_is_grabbed()) {
		//gdk_pointer_ungrab(0);
	//}
	//wFlush();
	//if (bl->recursion)
		//return 0;
	//id_p = gtk_clist_get_row_data( GTK_CLIST(clist), row );
	//if ( id_p == NULL ) return 1;
	//id_p->selected = FALSE;
	//if (bl->action)
		//bl->action( row, id_p->label, 2, bl->data, id_p->itemData );
	//return 1;
//}


/**
 * Create a single column list box (not what the names suggests!)
 * \todo Improve or discard totally, in this case, remove from param.c \
 * as well.
 *
 * \param varname1 IN this is a variable
 * \param varname2 OUT and another one that is modified
 * \return    describe the return value
 */
 
EXPORT wList_p wComboListCreate(
		wWin_p	parent,		/* Parent window */
		wPos_t	x,		/* X-position */
		wPos_t	y,		/* Y-position */
		const char 	* helpStr,	/* Help string */
		const char	* labelStr,	/* Label */
		long	option,		/* Options */
		long	number,		/* Number of displayed list entries */
		wPos_t	width,		/* Width */
		long	*valueP,	/* Selected index */
		wListCallBack_p action,	/* Callback */
		void 	*data )		/* Context */
/*
*/
{
	//return wListCreate( parent, x, y, helpStr, labelStr, option, number, width, 0, NULL, NULL, NULL, valueP, action, data );
	wNotice( "ComboLists are not implemented!", "Abort", NULL );
	abort();
}


