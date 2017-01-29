/** \file droplist.c
 * Dropdown list functions
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "gtkint.h"
#include "i18n.h"

/* define the column count for the tree model */
#define DROPLIST_TEXTCOLUMNS 1 

/**
 * Add the data columns to the droplist. 
 * 
 * \param dropList IN 
 * \param columns IN 
 * \returns number of columns created
 */

int
wlibDropListAddColumns( GtkWidget *dropList, int columns )
{
	int i;
	GtkCellRenderer *cell;
	
	/* Create cell renderer. */
	cell = gtk_cell_renderer_text_new();

	for ( i = 0; i < columns; i++ ) {
		/* Pack it into the droplist */
		gtk_cell_layout_pack_start( GTK_CELL_LAYOUT( dropList ), cell, TRUE );
	
		/* Connect renderer to data source */
		gtk_cell_layout_set_attributes( GTK_CELL_LAYOUT( dropList ), 
										cell, 
										"text", 
										LISTCOL_TEXT + i, 
										NULL );
	}
	return( i );
}

/**
 * Get the number of rows in drop list
 * 
 * \param b IN widget
 * \return number of rows
 */

wIndex_t wDropListGetCount( wList_p b )
{
	return( gtk_tree_model_iter_n_children( GTK_TREE_MODEL( b->listStore ), NULL ));
}

/**
 * Clear the whole droplist
 *
 * \param b IN the droplist
 * \return 
 */

void 
wDropListClear( wList_p b )
{
	wlibListStoreClear( b->listStore );	
}	

/**
 * Get the user data / context information for a row in the droplist
 * \param b IN widget
 * \param inx IN row 
 * \returns pointer to context information
 */

void *wDropListGetItemContext( wList_p b, wIndex_t inx )
{
	GtkTreeIter iter;
	gchar *text;
	void *data = NULL;
	
	if( gtk_tree_model_iter_nth_child( GTK_TREE_MODEL( b->listStore ), &iter, NULL, inx )) {	
		gtk_tree_model_get( GTK_TREE_MODEL( b->listStore ), LISTCOL_DATA, &data, -1 );
	}
	return( data );	
}

/**
 * Add an entry to a dropdown list. Only single text entries are allowed
 *
 * \param b IN the widget
 * \param text IN string to add
 * \return    describe the return value
 */
 
void wDropListAddValue(
		wList_p b,
		char *text,
		void *data )
{
	GtkTreeIter iter;
	unsigned cnt;

	assert( b != NULL );
	assert( text != NULL );
	
	gtk_list_store_append( b->listStore, &iter );	// append new row to tree store
	
	gtk_list_store_set ( b->listStore, &iter,
						 LISTCOL_TEXT, text,
						 LISTCOL_DATA, data,
						 -1);
}	

/**
 * Set the value to the entry field of a droplist
 * \param bl IN 
 * \param val IN 
 */

void wListSetValue(
		wList_p bl,
		const char * val )
{
	assert(bl->listStore!=NULL);
	
	bl->recursion++;
	if (bl->type == B_DROPLIST) {
		bl->editted = TRUE;
		gtk_entry_set_text( GTK_ENTRY( gtk_bin_get_child(GTK_BIN(bl->widget))), val );
		if (bl->action) {
			bl->action( -1, val, 0, bl->data, NULL );
		}
	} else {
		assert( FALSE );
	}	
	bl->recursion--;
}

/**
 * Makes the <val>th entry (0-origin) the current selection.
 * If <val> if '-1' then no entry is selected.
 *
 * \param b IN the widget
 * \param val IN the index
 * 
 * \todo it seems BL_NONE is not used in current code, check and clean up
 */

void wDropListSetIndex( wList_p b, int val )
{
	if ((b->option&BL_NONE)!=0 && val < 0) {
		gtk_combo_box_set_active( GTK_COMBO_BOX( b->widget ), -1 );
	} else {
		gtk_combo_box_set_active( GTK_COMBO_BOX( b->widget ), val );
	}
}

/**
 * Set the values for a row in the droplist
 *
 * \param b IN drop list widget
 * \param row IN index 
 * \param labelStr IN new text
 * \param bm IN ignored
 * \param itemData IN ignored
 * \return
 */

wBool_t wDropListSetValues(
		wList_p b,
		wIndex_t row,
		const char * labelStr,
		wIcon_p bm,
		void *itemData )
{
	GtkTreeIter iter;
	
	if( gtk_tree_model_iter_nth_child( GTK_TREE_MODEL( b->listStore ), &iter, NULL, row )) {
		gtk_tree_store_set( GTK_TREE_STORE( b->listStore ), &iter, LISTCOL_TEXT, labelStr, -1 );	
		return( TRUE );
	} else {
		return( FALSE );
	}	
}		

/**
 * Signal handler for the "changed"-signal in drop list. Gets the selected
 * text and determines the selected row in the tree model. 
 *
 * \param comboBox IN the combo_box
 * \param data IN the drop list handle
 * \return    
 */

static int DropListSelectChild(
		GtkComboBox * comboBox,
		gpointer data )
{
	wList_p bl = (wList_p)data;
	GtkTreeModel *model;
	GtkTreeIter iter;

	wIndex_t inx = 0;
	gchar *string;
	void *addData;

	if (bl->recursion)
		return 0;

	if ( bl->type == B_DROPLIST && bl->editable ) {
		if ( bl->editted == FALSE )
			return 0;
		gtkSetTrigger( NULL, NULL );
	}
	bl->editted = FALSE;
	
	/* Obtain currently selected item from combo box.
	 * If nothing is selected, do nothing. */
	if( gtk_combo_box_get_active_iter( GTK_COMBO_BOX( comboBox ), &iter ))
	{
		/* Obtain data model from combo box. */
		model = gtk_combo_box_get_model( comboBox );
		
		/* get the selected row */
		string = gtk_tree_model_get_string_from_iter ( model,
													   &iter);
		inx = atoi( string );
		g_free( string );											   					
    
		/* Obtain string from model. */
		gtk_tree_model_get( model, &iter, 
							LISTCOL_TEXT, &string, 
							LISTCOL_DATA, &addData,
							-1 );
		
	}
	
	/* selection changed, store new selections and call back */
	if ( bl->last != inx ) {
		
		bl->last = inx;

		if (bl->valueP)
			*bl->valueP = inx;

		/* selection changed -> callback */
		if (string && bl->action)
			bl->action( inx, string, 1, bl->data, addData );
	}
	return 1;
}

//static void triggerDListEntry(
		//wControl_p b )
//{
	//wList_p bl = (wList_p)b;
	//const char * entry_value;

	//if (bl == 0)
		//return;
	//if (bl->widget == 0) abort();
	//entry_value = gtk_entry_get_text( GTK_ENTRY(GTK_COMBO(bl->widget)->entry) );
	//if (entry_value == NULL) return;
	//if (debugWindow >= 2) printf("triggerListEntry: %s text = %s\n", bl->labelStr?bl->labelStr:"No label", entry_value );
	//if (bl->action) {
		//bl->recursion++;
		//bl->action( -1, entry_value, 0, bl->data, NULL );
		//bl->recursion--;
	//}
	//gtkSetTrigger( NULL, NULL );
	//return;
//}

//static void updateDListEntry(
		//GtkEntry * widget,
		//wList_p bl )
//{
	//const char *entry_value;
	//if (bl == 0)
		//return;
	//if (bl->recursion)
		//return;
	//if (!bl->editable)
		//return;
	//entry_value = gtk_entry_get_text( GTK_ENTRY(GTK_COMBO(bl->widget)->entry) );
	//bl->editted = TRUE;
	//if (bl->valueP != NULL)
		// *bl->valueP = -1;
	//bl->last = -1;
	//if (bl->action)
		//gtkSetTrigger( (wControl_p)bl, triggerDListEntry );
	//return;
//}

/**
 * Create a droplist for a given liststore
 * 
 * \param ls IN list store for dropbox
 * \param editable IN droplist with entry field
 * \returns the newly created widget
 */

GtkWidget *
wlibNewDropList( GtkListStore *ls, int editable )
{
	GtkWidget *widget;
	
	if( editable ) {
		widget = gtk_combo_box_new_with_model_and_entry( GTK_TREE_MODEL( ls ) );
	}	
	else { 	
		widget = gtk_combo_box_new_with_model( GTK_TREE_MODEL( ls ) );
	}	
	return( widget );
}	

/** 
 * Create a drop down list. The drop down is created and intialized with the supplied values.
 *
 *	\param IN parent Parent window
 *	\param IN x, X-position
 *	\param IN y	 Y-position
 *	\param IN helpStr Help string
 *	\param IN labelStr Label
 *	\param IN option Options
 *	\param IN number Number of displayed entries
 *	\param IN width Width
 *	\param IN valueP Selected index
 *	\param IN action Callback
 *	\param IN data Context
 */

EXPORT wList_p wDropListCreate(
		wWin_p	parent,		
		wPos_t	x,		
		wPos_t	y,		
		const char 	* helpStr,
		const char	* labelStr,
		long	option,		
		long	number,		
		wPos_t	width,		
		long	*valueP,	
		wListCallBack_p action,
		void 	*data )		
{
	wList_p b;
	
	b = (wList_p)wlibAlloc( parent, B_DROPLIST, x, y, labelStr, sizeof *b, data );
	b->option = option;
	b->number = number;
	b->count = 0;
	b->last = -1;
	b->valueP = valueP;
	b->action = action;
	b->listX = b->realX;
	b->colCnt = 0;
	b->colWidths = NULL;
	b->colRightJust = NULL;
	b->editable = (option & BL_EDITABLE != 0 );
	
	assert( width != 0 );
	
	wlibComputePos( (wControl_p)b );


	// create tree store for storing the contents 
	b->listStore = wlibNewListStore( DROPLIST_TEXTCOLUMNS );
	if( !b->listStore ) abort();
	
	// create the droplist
	b->widget = wlibNewDropList( b->listStore,
								 option & BL_EDITABLE );
									
	if (b->widget == 0) abort();
	
	g_object_unref( G_OBJECT( b->listStore ) );
	
	wlibDropListAddColumns( b->widget, DROPLIST_TEXTCOLUMNS );
	
	g_signal_connect( GTK_OBJECT(b->widget), "changed", G_CALLBACK(DropListSelectChild), b );

	gtk_widget_set_size_request( b->widget, width, -1 );

	gtk_fixed_put( GTK_FIXED(parent->widget), b->widget, b->realX, b->realY );
	wlibControlGetSize( (wControl_p)b );

	if (labelStr)
		b->labelW = wlibAddLabel( (wControl_p)b, labelStr );

	gtk_widget_show( b->widget );
	wlibAddButton( (wControl_p)b );
	wlibAddHelpString( b->widget, helpStr );
			
	return b;
}

